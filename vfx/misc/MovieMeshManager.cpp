
#include "MovieMeshManager.h"

VFX_MISC_OPTIMISATIONS()

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/timer.h"
#include "streaming/streamingengine.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// rage
#include "grcore/image.h"
#include "grcore/texture.h"
#include "system/memops.h"
#include "system/nelem.h"

// game
#include "Core/Game.h"
#include "System/FileMgr.h"
#include "renderer/RenderTargetMgr.h"
#include "vfx/misc/MovieManager.h"
#include "vfx/misc/Fire.h"
#include "script/commands_graphics.h"
#include "modelinfo/ModelInfo.h"
#include "Objects/object.h"
#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "objects/objectpopulation.h"
#include "scene/world/GameWorld.h"
#include "animation/MoveObject.h"

#include "atl/atfixedstring.h"

#define MM_SCRIPTED_RT_PREFIX	"script_rt_"
#define MM_DATA_PATH			"common:/data/effects/bink/"
#define MOVIEMESH_MNGR_STRING	"MovieMeshManager"

CMovieMeshManager	g_movieMeshMgr;

u32 CMovieMeshManager::ms_globalRtHandle = atStringHash(MOVIEMESH_MNGR_STRING);


//////////////////////////////////////////////////////////////////////////
// MovieMeshHandle
//////////////////////////////////////////////////////////////////////////

void MovieMeshHandle::Release()
{
	if (refCount > 0)
	{
		refCount--; 

		REPLAY_ONLY( if( !CReplayMgr::IsEditModeActive() ) )
		{
			g_movieMgr.Release(movieHandle);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
// MovieMeshGlobalPool
//////////////////////////////////////////////////////////////////////////
MovieMeshGlobalPool::MovieMeshGlobalPool() {
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++) 
	{
		m_uniqueHandles[i].movieHandle = INVALID_MOVIE_MESH_HANDLE;
		m_uniqueHandles[i].rtHandle = INVALID_MOVIE_MESH_HANDLE;
	}
}

bool MovieMeshGlobalPool::GetFreeHandle(MovieMeshHandle*& pHandle) 
{
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++)
	{
		if (m_uniqueHandles[i].bInUse == false) {
			Assertf(m_uniqueHandles[i].rtHandle == INVALID_MOVIE_MESH_HANDLE && m_uniqueHandles[i].refCount == 0 && m_uniqueHandles[i].bInUse == false, "MovieMeshGlobalPool::GetFreeHandle: entry is corrupted");
			pHandle = &m_uniqueHandles[i];
			return true;
		}
	}

	// no free handles
	pHandle = NULL;
	return false;
}

bool MovieMeshGlobalPool::FindHandle(MovieMeshHandle*& pHandle, u32 movieHandle)
{
	Assertf(movieHandle != INVALID_MOVIE_MESH_HANDLE, "MovieMeshGlobalPool::FindHandle: Trying to find an entry with invalid handles");
	
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++)
	{
		if (m_uniqueHandles[i].movieHandle == movieHandle) {
			pHandle = &m_uniqueHandles[i];
			return true;
		}
	}

	pHandle = NULL;
	return false;
}

bool MovieMeshGlobalPool::HandleExists(const MovieMeshHandle* const pHandle) const 
{
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++)
	{
		const MovieMeshHandle* const pCmpHandle = &m_uniqueHandles[i];
		if (pCmpHandle == pHandle) 
		{
			return true;
		}
	}

	return false;
}

bool MovieMeshGlobalPool::AddHandle(MovieMeshHandle*& pHandle, u32 movieHandle) 
{
	// if it's already in the pool, increment ref count
	if (FindHandle(pHandle, movieHandle)) 
	{
		pHandle->IncRef();
		return true;
	} 
	// not in the pool, try to add it
	else if (GetFreeHandle(pHandle)) 
	{
		Assertf(movieHandle != INVALID_MOVIE_MESH_HANDLE, "MovieMeshGlobalPool::AddHandle: Trying to add an entry with invalid handles");

		// initialise handle
		pHandle->movieHandle = movieHandle;
		pHandle->bInUse = true;
		pHandle->IncRef();
		return true;
	}

	// something's gone wrong...
	pHandle = NULL;
	return false;
}

void MovieMeshGlobalPool::ModifyHandle(MovieMeshHandle* const pHandle, u32 movieHandle, u32 rtHandle, u32 rtNameHash) 
{
	if (HandleExists(pHandle))
	{	
		Assertf(pHandle->bInUse != false, "MovieMeshGlobalPool::ModifyHandle: corrupted handle (not in use)");
		Assertf(pHandle->movieHandle == movieHandle, "MovieMeshGlobalPool::ModifyHandle: corrupted handle (movie handle doesn't match)");
		pHandle->rtHandle = rtHandle;
		pHandle->movieHandle = movieHandle;
		pHandle->rtNameHash = rtNameHash;
		return;
	}

	Assertf(0, "MovieMeshGlobalPool::ModifyHandle: tried to modify a handle that doesn't exist");
}

void MovieMeshGlobalPool::Update() 
{
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++)
	{
		MovieMeshHandle* pHandle = &m_uniqueHandles[i];

		if (pHandle->bInUse && pHandle->refCount == 0) 
		{
			// release bink movie
			if (pHandle->movieHandle != INVALID_MOVIE_MESH_HANDLE) 
			{
				g_movieMgr.Release(pHandle->movieHandle);
			}

			// release render target
			if (pHandle->rtHandle != INVALID_MOVIE_MESH_HANDLE) 
			{
				g_movieMeshMgr.GetRTManager().Release(pHandle->rtNameHash);
			}

			pHandle->Reset();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CMovieMeshRenderTargetManager class
//////////////////////////////////////////////////////////////////////////

void CMovieMeshRenderTargetManager::Init()
{
	sysMemSet(m_numDelayedReleaseRts, 0, sizeof(m_numDelayedReleaseRts));
	m_delayedReleaseRtsFrame = 0;
}

void CMovieMeshRenderTargetManager::Shutdown()
{
	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		MovieMeshRenderTarget* pTarget = &m_renderTargets[i];
		pTarget->m_numRefs = 0;
		pTarget->m_state = MovieMeshRenderTarget::kWaitingForRelease;
		ProcessWaitingOnReleaseState(pTarget);
	}
}

u32 CMovieMeshRenderTargetManager::Register(atFinalHashString name)
{

	// Check if the render target exists
	MovieMeshRenderTarget* pRt = Get(name);
	if (pRt)
	{
		// Render target is good to reuse
		if (pRt->m_state == MovieMeshRenderTarget::kLoading || pRt->m_state == MovieMeshRenderTarget::kReady)
		{
			pRt->m_numRefs++;
			return pRt->m_name.GetHash();
		}

		// Render target exists, but in a state it cannot be used, try later
		return 0U;
	}

	// Check whether we can create a new one
	if (m_renderTargets.GetCount() == MAX_MOVIE_MESH_RT)
	{
		return 0U;
	}

	// It doesn't exist, create one
	pRt = &(m_renderTargets.Append());
	pRt->Reset();
	pRt->m_name = name;
	pRt->m_state = MovieMeshRenderTarget::kLoading;
	pRt->m_numRefs++;

	return pRt->m_name.GetHash();
}

bool CMovieMeshRenderTargetManager::Exists(atFinalHashString name) const
{
	return (Get(name) != NULL);
}

void CMovieMeshRenderTargetManager::Release(atFinalHashString name)
{
	// Remove a ref
	MovieMeshRenderTarget* pRt = Get(name);
	if (Verifyf(pRt != NULL, "target does not exists"))
	{
		if (pRt->m_numRefs <= 1)
		{
			// Don't release now, we'll do it during the update tick
			pRt->m_numRefs = 0;
			pRt->m_state = MovieMeshRenderTarget::kWaitingForRelease;
			return;
		}
		pRt->m_numRefs--;
	}
}

void CMovieMeshRenderTargetManager::Release(u32 rtHash)
{
	// Remove a ref
	MovieMeshRenderTarget* pRt = Get(rtHash);
	if (Verifyf(pRt != NULL, "target does not exists"))
	{
		if (pRt->m_numRefs <= 1)
		{
			// Don't release now, we'll do it during the update tick
			pRt->m_numRefs = 0;
			pRt->m_state = MovieMeshRenderTarget::kWaitingForRelease;
			return;
		}
		pRt->m_numRefs--;
	}
}

void CMovieMeshRenderTargetManager::Update()
{
	const unsigned delayedReleaseRtsFrame = (m_delayedReleaseRtsFrame + 1) % NELEM(m_delayedReleaseRts);
	m_delayedReleaseRtsFrame = delayedReleaseRtsFrame;
	const unsigned numDelayedReleaseRts = m_numDelayedReleaseRts[delayedReleaseRtsFrame];
	for (int i = 0; i < numDelayedReleaseRts; i++)
	{
		grcRenderTarget *const rt = m_delayedReleaseRts[delayedReleaseRtsFrame][i];
		m_delayedReleaseRts[delayedReleaseRtsFrame][i] = NULL;
		rt->Release();
	}
	m_numDelayedReleaseRts[delayedReleaseRtsFrame] = 0;

	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		MovieMeshRenderTarget* pRt = &m_renderTargets[i];
		MovieMeshRenderTarget::eState rtState = pRt->m_state;

		// Release target
		if (rtState == MovieMeshRenderTarget::kWaitingForRelease)
		{
			Assert(pRt->m_numRefs == 0);
			if (ProcessWaitingOnReleaseState(pRt))
			{
				m_renderTargets.DeleteFast(i);
				i--;
			}
		}
		// Check whether it's ready
		else if (rtState == MovieMeshRenderTarget::kLoading)
		{
			ProcessLoadingState(pRt);
		}
		// Do stuff
		else if (rtState == MovieMeshRenderTarget::kReady)
		{
			ProcessReadyState(pRt);
		}
		// Shouldn't happen...
		else
		{
			Assertf(0, "Invalid state");
		}
	}
}

bool CMovieMeshRenderTargetManager::ProcessWaitingOnReleaseState(MovieMeshRenderTarget* pTarget)
{
	// Release render target
	grcRenderTarget *const rt = pTarget->m_pRenderTarget;
	if (rt)
	{
		const unsigned delayedReleaseRtsFrame = m_delayedReleaseRtsFrame;
		const unsigned numDelayedReleaseRts = m_numDelayedReleaseRts[delayedReleaseRtsFrame];
		Assert(numDelayedReleaseRts < MAX_MOVIE_MESH_RT);
		m_delayedReleaseRts[delayedReleaseRtsFrame][numDelayedReleaseRts] = rt;
		m_numDelayedReleaseRts[delayedReleaseRtsFrame] = numDelayedReleaseRts+1;
		pTarget->m_pRenderTarget = NULL;
	}

	// Remove ref from txd
	bool bRefAdded = pTarget->m_bTxdRefAdded;
	s32 txdIdx = pTarget->m_txdIdx;
	if (bRefAdded && txdIdx != -1 && g_TxdStore.HasObjectLoaded(strLocalIndex(txdIdx)))
	{
		gDrawListMgr->AddRefCountedModuleIndex(txdIdx, &g_TxdStore);
	}

	pTarget->Reset();

	return true;
}

void CMovieMeshRenderTargetManager::ReleaseOnFailure(MovieMeshRenderTarget* pTarget)
{
	pTarget->m_numRefs = 0;
	pTarget->m_state = MovieMeshRenderTarget::kWaitingForRelease;
	ProcessWaitingOnReleaseState(pTarget);
}

static inline void EnsureTextureGpuWritable(grcTexture *texture)
{
#if RSG_DURANGO || RSG_ORBIS
	static_cast<grcOrbisDurangoTextureBase*>(texture)->EnsureGpuWritable();
#else
	(void)texture;
#endif
}

void CMovieMeshRenderTargetManager::ProcessLoadingState(MovieMeshRenderTarget* pTarget)
{
	strLocalIndex txdIdx = strLocalIndex(pTarget->m_txdIdx);

	// We still don't have a txd slot
	if (txdIdx == -1)
	{
		atHashString txdName("vfx_reference");
		txdIdx = g_TxdStore.FindSlotFromHashKey(txdName);
	}
	
	// If we do and it's loaded, prepare the render target
	if (txdIdx != -1 && g_TxdStore.HasObjectLoaded(txdIdx))
	{
		bool bOk = true;

		// Make sure the texture we're after exists
		fwTxd* pTxd = g_TxdStore.Get(txdIdx);
		grcTexture* pTex = pTxd->Lookup(pTarget->m_name);
		EnsureTextureGpuWritable(pTex);
		CStreaming::SetDoNotDefrag(txdIdx, g_TxdStore.GetStreamingModuleId());

		grcRenderTarget* pRt = NULL;
		if (pTex)
		{
			pRt = CreateRenderTarget(pTarget, pTex);
			bOk = (pRt != NULL);
		}

		// All good to go
		if (bOk)
		{
			// Add a ref to the TXD
			g_TxdStore.AddRef(txdIdx, REF_OTHER);
			pTarget->m_bTxdRefAdded = true;
			pTarget->m_pTexture = pTex;
			pTarget->m_pRenderTarget = pRt;
			pTarget->m_state = MovieMeshRenderTarget::kReady;
		}
		// Something went wrong, release immediately
		else
		{
			ReleaseOnFailure(pTarget);
		}
	}
}

void CMovieMeshRenderTargetManager::ProcessReadyState(MovieMeshRenderTarget* )
{

}

grcRenderTarget* CMovieMeshRenderTargetManager::CreateRenderTarget(MovieMeshRenderTarget* pTarget, grcTexture* pTexture)
{
	grcRenderTarget* pRenderTarget = NULL;

	EnsureTextureGpuWritable(pTexture);

	u32 imgFormat = pTexture->GetImageFormat();
	bool isSupported =	imgFormat == grcImage::A8R8G8B8 ||
		imgFormat == grcImage::A8B8G8R8 ||
		imgFormat == grcImage::L8 ||
		imgFormat == grcImage::A1R5G5B5 ||
		imgFormat == grcImage::R5G6B5;

	Assertf(isSupported == true, "Warning: RT is in an unsupported format");
	if(isSupported)
	{
		pRenderTarget = grcTextureFactory::GetInstance().CreateRenderTarget(pTarget->m_name.GetCStr(), pTexture->GetTexturePtr());
	}

	return pRenderTarget;	
}

MovieMeshRenderTarget* CMovieMeshRenderTargetManager::Get(atFinalHashString name)
{
	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		if (m_renderTargets[i].m_name == name)
			return &m_renderTargets[i];
	}
	return NULL;
}

const MovieMeshRenderTarget* CMovieMeshRenderTargetManager::Get(atFinalHashString name) const
{
	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		if (m_renderTargets[i].m_name == name)
			return &m_renderTargets[i];
	}
	return NULL;
}

MovieMeshRenderTarget* CMovieMeshRenderTargetManager::Get(u32 rtHash)
{
	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		if (m_renderTargets[i].m_name.GetHash() == rtHash)
			return &m_renderTargets[i];
	}
	return NULL;
}

void CMovieMeshRenderTargetManager::BufferDataForDrawList(MovieMeshRenderTargetBufferedData* pOut)
{
	int count = m_renderTargets.GetCount();

	for (int i = 0; i < count; i++)
	{
		MovieMeshRenderTarget& current = m_renderTargets[i];

		grcRenderTarget* pRt = current.m_pRenderTarget;
		grcTexture* pTexture = NULL;

		if (pRt && current.m_state == MovieMeshRenderTarget::kReady)
		{
			pTexture = current.m_pTexture;

			// Add a temporary ref to the resource the render target belongs to.
			// Should be inside of a txd, a frag, etc.
			if (pTexture)
			{
				const strIndex strIdx = strStreamingEngine::GetStrIndex(&*pTexture);
				Assert(strIdx.IsValid());
				strStreamingModule* const module = strStreamingEngine::GetInfo().GetModule(strIdx);
				Assert(module);
				const strLocalIndex objIdx = module->GetObjectIndex(strIdx);
				module->AddRef(objIdx, REF_RENDER);
				gDrawListMgr->AddRefCountedModuleIndex(objIdx.Get(), module);
			}
			else
			{
				// Clear out the render target pointer that is going to be
				// stored.
				pRt = NULL;
			}
		}

		pOut->pRt	= pRt;
		pOut->tex	= pTexture;
		pOut->id	= current.m_name.GetHash();
		++pOut;
	}
}

bool CMovieMeshRenderTargetManager::IsReady(u32 rtHash) const
{
	for (int i = 0; i < m_renderTargets.GetCount(); i++)
	{
		if (m_renderTargets[i].m_name.GetHash() == rtHash && m_renderTargets[i].m_state == MovieMeshRenderTarget::kReady)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// CMovieMeshManager class
//////////////////////////////////////////////////////////////////////////

CMovieMeshManager::CMovieMeshManager()
{
	Reset();
}


CMovieMeshManager::~CMovieMeshManager()
{
	Reset();
}

void CMovieMeshManager::Init(unsigned initMode)
{
	g_movieMeshMgr.InitInternal(initMode);
}

void CMovieMeshManager::Shutdown(unsigned shutdownMode)
{
	g_movieMeshMgr.ShutdownInternal(shutdownMode);
}

void CMovieMeshManager::InitInternal(unsigned UNUSED_PARAM(initMode))
{
	Reset();
	m_renderTargetMgr.Init();
}

void CMovieMeshManager::ShutdownInternal(unsigned UNUSED_PARAM(shutdownMode))
{
	Reset(true);
}

void CMovieMeshManager::Reset(bool bReleaseResources)
{
#if __BANK
	m_bDebugIgnoreAnimData = false;
#endif

	if (bReleaseResources)
	{
		for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++) 
		{
			MovieMeshSet* pCurSet =	&m_setPool[i];
			ReleaseSet(pCurSet);
		}

		m_renderTargetMgr.Shutdown();
	}
	m_loadRequestPending = false;
	m_releaseRequestPending = false;
}

void CMovieMeshManager::Update()
{	
	// process deferred deletion requests
	ProcessDeleteRequests();

	// pool housekeeping
	m_globalPool.Update();

	// process deferred loading requests
	ProcessLoadRequests();

	m_renderTargetMgr.Update();

	// update active sets
	UpdateSets();

	// draw any rt/bink pair
	Render();

#if __BANK
	// update debug widgets
	CMovieMeshDebugTool::Update();
#endif

}


void CMovieMeshManager::UpdateSets()
{
	// update individual sets
	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		MovieMeshSet* pCurSet = &m_setPool[i];
		UpdateSet(pCurSet);
	}
}

void CMovieMeshManager::Render()
{
	// draw binks
	for (int i = 0; i < MOVIE_MESH_MAX_HANDLES; i++)
	{
		MovieMeshHandle& curHandle = m_globalPool.m_uniqueHandles[i];
		if (curHandle.rtHandle == INVALID_MOVIE_MESH_HANDLE) 
		{
			continue;
		}

		if (m_renderTargetMgr.IsReady(curHandle.rtHandle))
		{
			CScriptHud::scriptTextRenderID = curHandle.rtHandle;
			gRenderTargetMgr.UseRenderTarget(CRenderTargetMgr::RTI_MovieMesh);
			CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;

			graphics_commands::DrawSpriteTex(NULL, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 255, 255, 255, 255, curHandle.movieHandle, SCRIPT_GFX_SPRITE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
		}
	}
}

#if __BANK
void CMovieMeshManager::RenderDebug3D()
{
	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		const MovieMeshSet* pCurSet = &m_setPool[i];
		if (pCurSet->m_state == MMS_LOADED)
		{
			CMovieMeshDebugTool::Render(pCurSet);
		}
	}
}
#endif //__BANK

void CMovieMeshManager::UpdateSet(MovieMeshSet* pCurSet)
{
	// return if there's no active set
	if (pCurSet == NULL)
	{
		return;
	}

	// update anim data that hasn't been already processed
	if (pCurSet->m_state != MMS_FAILED)
		UpdateAnimDataForSet(pCurSet);


	// return if the whole set is not ready
	if (pCurSet->m_state != MMS_LOADED)
	{
		return;
	}

	// if set is loaded, register fires
	MovieMeshInfoList& meshInfo = pCurSet->m_info;
	for (int i = 0; i < meshInfo.m_BoundingSpheres.GetCount(); i++) 
	{
		Vec4V vSpherePosAndRadius = RC_VEC4V(meshInfo.m_BoundingSpheres[i]);
		g_fireMan.RegisterPtFxFire(NULL, vSpherePosAndRadius.GetXYZ(), pCurSet, i, vSpherePosAndRadius.GetWf(), false, false);
	}

}

void CMovieMeshManager::UpdateAnimDataForSet(MovieMeshSet* pCurSet)
{

	for (int i = 0; i < pCurSet->m_pMeshes.GetCount(); i++)
	{
		// skip if object has already been processed and is ready
		if (pCurSet->m_pMeshes[i].m_bIsReady)
		{
			continue;
		}

		CObject* pObject = static_cast<CObject*>(pCurSet->m_pMeshes[i].m_pObj.Get());

		if (pObject == NULL) 
		{
			continue;
		}

		u32 modelIndex = CModelInfo::GetModelIdFromName(pCurSet->m_info.m_Data[i].m_ModelName.c_str()).GetModelIndex();
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		// check clip dictionary index to determine whether the model has animation or not
		s32 clipDictionaryIndex = pModelInfo->GetClipDictionaryIndex().Get();
		
		// object has no animation 
		if (clipDictionaryIndex == -1 BANK_ONLY(|| m_bDebugIgnoreAnimData == true))
		{
			pCurSet->m_pMeshes[i].m_bIsReady = true;
			continue;
		}

		// object has animation and needs to be initialised once a drawable is available
		// drawable might not be available, don't keep the system from updating because of it
		if (pObject->GetDrawable() != NULL) 
		{
			pObject->InitAnimLazy();
			if (pObject->GetAnimDirector())
			{

				const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex, pModelInfo->GetHashKey());

				// there's no clip matching the model's name, no further processing needed
				// but model should has probably been exported with hasAnim == false
				if (pClip == NULL)
				{
					vfxDisplayf("CMovieMeshManager::UpdateAnimDataForSet: expected an animation clip for model \"%s\", please fix me", pCurSet->m_info.m_Data[i].m_ModelName.c_str());
					pCurSet->m_pMeshes[i].m_bIsReady = true;
					continue;
				}

				CMoveObject& moveObj = pObject->GetMoveObject();
				moveObj.GetClipHelper().BlendInClipByClip( pObject, pClip, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, true, false);
				pCurSet->m_pMeshes[i].m_bIsReady = true;

			}	
		}
	}

	// set state
	pCurSet->m_state = MMS_LOADED;
}

MovieMeshSetHandle CMovieMeshManager::GetTemporaryHandle(const char* pFilename) const
{
	if (m_loadRequestPending == false)
	{
		vfxDisplayf("CMovieMeshManager::GetTemporaryHandle: a handle has been requested but there are no pending load requests");
		return INVALID_MOVIE_MESH_SET_HANDLE;
	}

	return atStringHash(pFilename);
}

MovieMeshSetHandle CMovieMeshManager::LoadSet(const char* pFilename)
{
	// check whether the set is already loaded
	MovieMeshSetHandle tmpHandle = atStringHash(pFilename);
	MovieMeshSet* pCurSet = FindSet(tmpHandle);
	if (pCurSet != NULL)
	{
		vfxAssertf(0, "CMovieMeshManager::LoadSet: the set \"%s\" is already loaded", pFilename);
		return pCurSet->m_handle;
	}

	int scriptHandle;
	pCurSet = GetFreeSet(scriptHandle);

	if (pCurSet == NULL)
	{
		Assert(scriptHandle == INVALID_MOVIE_MESH_SCRIPT_ID);
		return scriptHandle;
	}

	pCurSet->m_loadRequestName = atString(pFilename);
	pCurSet->m_state = MMS_PENDING_LOAD;
	pCurSet->m_scriptId = scriptHandle;
	pCurSet->m_handle = atStringHash(pFilename);
	m_loadRequestPending = true;

	return pCurSet->m_handle;
}

void CMovieMeshManager::ProcessLoadRequests()
{
	// release requests should have been processed first
	vfxAssertf(m_releaseRequestPending == false, "CMovieMeshManager::ProcessLoadRequests: trying to process load requests before processing deletions");

	// no pending load requests
	if (m_loadRequestPending == false)
	{
		return;
	}

	// reset pending load requests flag
	m_loadRequestPending = false;

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		MovieMeshSet* pCurSet = &m_setPool[i];
		if (pCurSet->m_state == MMS_PENDING_LOAD)
		{
			ProcessLoadRequest(pCurSet);
		}
	}


}

void CMovieMeshManager::ProcessLoadRequest(MovieMeshSet* pCurSet)
{
	Assertf(pCurSet->m_state == MMS_PENDING_LOAD, "CMovieMeshManager::ProcessLoadRequest: trying to load a set with state %d", pCurSet->m_state);
	if (pCurSet == NULL)
	{
		vfxDisplayf("CMovieMeshManager::ProcessLoadRequests: Failed to find a free set");
		return;
	}

	// try loading movie the mesh info data
	bool bOk = LoadMovieMeshInfoData(pCurSet);

	// try loading the models
	bOk = bOk && LoadMovieMeshObjects(pCurSet);

	// try loading bink movies
	bOk = bOk && LoadMovies(pCurSet);

	// try loading and linking named render targets
	bOk = bOk && LoadNamedRenderTargets(pCurSet);

	// if something went wrong, return set to pool
	if (bOk)
	{
		SetupBoundingSpheres(pCurSet);
	}
	else
	{
		ReleaseSet(pCurSet);
	}

	// set state as loaded
	pCurSet->m_state = MMS_LOADED;
}

void CMovieMeshManager::SetupBoundingSpheres(MovieMeshSet* pCurSet)
{

	MovieMeshInfoList& setInfo = pCurSet->m_info;

	// initialise default transform 
	Mat34V vTransform;
	vTransform.SetIdentity3x3();


	// check if interior is loaded and ready
	CInteriorInst* pInteriorInst = GetInteriorAtCoordsOfType(setInfo.m_InteriorPosition, setInfo.m_InteriorName.c_str());
	bool bIsInteriorLoaded = IsInteriorReady(pInteriorInst);

	if (pInteriorInst == NULL || bIsInteriorLoaded == false)
	{
		vfxDisplayf("CMovieMeshManager::SetupBoundingSpheres: Interior \"%s\" is not available", setInfo.m_InteriorName.c_str());
	}
	else 
	{
		pInteriorInst->GetTransform().GetMatrixCopy(vTransform);
	}


	for (int i=0; i < setInfo.m_BoundingSpheres.GetCount(); i++)
	{
		// the position stored in the xml data is relative to m_InteriorMaxPosition,
		// so we need to take the offset into account to make the position local to the interior origin.
		Vector3 origPos = setInfo.m_BoundingSpheres[i].GetVector3();
		const float radius = setInfo.m_BoundingSpheres[i].GetW();
		const Vector3 interiorPos = setInfo.m_InteriorMaxPosition;

		// the position stored in the xml data is relative to m_InteriorMaxPosition,
		// so we need to take the offset into account to make the position local to the interior origin.
		Vector3 localTranslation = origPos-setInfo.m_InteriorMaxPosition;
		Mat34V vLocalTransform;
		vLocalTransform.SetIdentity3x3();
		vLocalTransform.Setd(VECTOR3_TO_VEC3V(localTranslation));

		Mat34V vFinalTransform;
		Transform(vFinalTransform, vTransform, vLocalTransform);

		setInfo.m_BoundingSpheres[i].SetVector3(VEC3V_TO_VECTOR3(vFinalTransform.d()));
		setInfo.m_BoundingSpheres[i].w = radius;
	}

}

void CMovieMeshManager::ProcessDeleteRequests()
{
	if (m_releaseRequestPending == false)
	{
		return;
	}
	
	m_releaseRequestPending = false;

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		MovieMeshSet* pCurSet = &m_setPool[i];

		if (pCurSet->m_state == MMS_PENDING_DELETE)
		{
			ReleaseSet(pCurSet);
		}
	}
}

CObject* CMovieMeshManager::CreateMovieMeshObject(u32 modelIndex, Mat34V_In vTransform, fwInteriorLocation intLoc)
{
	fwModelId modelId((strLocalIndex(modelIndex)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);

	if(pModelInfo==NULL)
	{
		Displayf ("CMovieMeshManager::CreateMovieMeshObject: invalid model: %d", modelIndex);
		return NULL;
	}

	RegdEnt pEnt(NULL); 
	if(pModelInfo->GetIsTypeObject())
	{
		pEnt = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_VFX, true);
	}
	else
	{
		Displayf("CMovieMeshManager::CreateMovieMeshObject: model type not supported");
		return NULL;
	}

	if (pEnt != NULL)
	{
//		fwMatrixTransform trans = fwMatrixTransform(vTransform);
		pEnt->SetMatrix(MAT34V_TO_MATRIX34(vTransform));

		fragInst* pFragInst = pEnt->GetFragInst();
		if (pFragInst != NULL)
		{
			pFragInst->SetResetMatrix(MAT34V_TO_MATRIX34(vTransform));
		}
		CGameWorld::Add(pEnt, intLoc);


		if(pEnt->GetIsTypeObject())
		{
			CObject* pDynamicEnt = static_cast<CObject*>(pEnt.Get()); 
			pDynamicEnt->GetPortalTracker()->RequestRescanNextUpdate();
			pDynamicEnt->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition()));

			pDynamicEnt->SetupMissionState();
		}	
	}

	if (pEnt && pEnt->GetIsTypeObject())
	{
		return static_cast<CObject*>(pEnt.Get());
	}


	return NULL;
}

MovieMeshSet* CMovieMeshManager::GetFreeSet(int& scriptIdx)
{
	scriptIdx = INVALID_MOVIE_MESH_SCRIPT_ID;

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++) 
	{
		if (m_setPool[i].m_handle == INVALID_MOVIE_MESH_SET_HANDLE)
		{
			scriptIdx = i+1;
			return &m_setPool[i];
		}

	}

	Assertf(0, "CMovieMeshManager::GetFreeSet: No free sets"); 
	return NULL;
}

MovieMeshSet* CMovieMeshManager::FindSet(MovieMeshSetHandle handle)
{
	if (handle == INVALID_MOVIE_MESH_SET_HANDLE)
	{
		return NULL;
	}

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++) 
	{
		if (m_setPool[i].m_handle == handle)
		{
			return &m_setPool[i];
		}
	}

	return NULL;
}

const MovieMeshSet* CMovieMeshManager::FindSet(MovieMeshSetHandle handle) const
{
	if (handle == INVALID_MOVIE_MESH_SET_HANDLE)
	{
		return NULL;
	}

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++) 
	{
		if (m_setPool[i].m_handle == handle)
		{
			return &m_setPool[i];
		}
	}

	return NULL;
}

bool CMovieMeshManager::LoadMovieMeshObjects(MovieMeshSet* pCurSet)
{
	const MovieMeshInfoList& setInfo = pCurSet->m_info;
	bool bOk = true;

	// initialise default transform 
	Mat34V vTransform;
	vTransform.SetIdentity3x3();

	// default interior location (outdoors) for sets that are not bound to an interior
	fwInteriorLocation intLoc = CGameWorld::OUTSIDE;

	bool bIsInterior = setInfo.m_UseInterior;

	// check if the set belongs in an interior 
	if (bIsInterior)
	{
		CInteriorInst* pInteriorInst = GetInteriorAtCoordsOfType(setInfo.m_InteriorPosition, setInfo.m_InteriorName.c_str());

		// check if interior is loaded and ready
		bool bIsInteriorLoaded = IsInteriorReady(pInteriorInst);

		if (pInteriorInst == NULL || bIsInteriorLoaded == false)
		{
			bOk = false;
			vfxDisplayf("CMovieMeshManager::LoadMovieMeshObjects: Interior \"%s\" is not available", setInfo.m_InteriorName.c_str());
			
		}
		else 
		{
			pInteriorInst->GetTransform().GetMatrixCopy(vTransform);
			intLoc = CInteriorInst::CreateLocation(pInteriorInst, 0);
		}
	}
	

	// iterate through meshes in the set
	if (bOk)
	{
		for (int i = 0; i < setInfo.m_Data.GetCount(); i++)
		{
			// get a model index for the current mesh
			u32 nModelIndex = CModelInfo::GetModelIdFromName(setInfo.m_Data[i].m_ModelName.c_str()).GetModelIndex();

			// check model info is valid
			if (CModelInfo::IsValidModelInfo(nModelIndex) == false)
			{
				vfxDisplayf("CMovieMeshManager::LoadMovieMeshObjects: Invalid model index for \"%s\"", setInfo.m_Data[i].m_ModelName.c_str());
				bOk = false;
				break;
			}
		

			Mat34V vLocalTransform;
			vLocalTransform.Seta(VECTOR3_TO_VEC3V(setInfo.m_Data[i].m_TransformA));
			vLocalTransform.Setb(VECTOR3_TO_VEC3V(setInfo.m_Data[i].m_TransformB));
			vLocalTransform.Setc(VECTOR3_TO_VEC3V(setInfo.m_Data[i].m_TransformC));

			// the position stored in the xml data is relative to m_InteriorMaxPosition,
			// so we need to take the offset into account to make the position local to the interior origin.
			Vector3 localTranslation = setInfo.m_Data[i].m_TransformD-setInfo.m_InteriorMaxPosition;
			vLocalTransform.Setd(VECTOR3_TO_VEC3V(localTranslation));

			Mat34V vFinalTransform;
			Transform(vFinalTransform, vTransform, vLocalTransform);

			ReOrthonormalize3x3(vFinalTransform, vFinalTransform);

			//load the entered object
			CObject* pObj = CreateMovieMeshObject(nModelIndex, vFinalTransform, intLoc);

			// check object was created ok
			if (pObj == NULL)
			{
				vfxDisplayf("CMovieMeshManager::LoadMovieMeshObjects: Failed to load model \"%s\"", setInfo.m_Data[i].m_ModelName.c_str());
				bOk = false;
				break;
			}

			MovieMeshSet::MeshObject mesh;
			mesh.m_bIsReady = false;
			mesh.m_pObj = RegdEnt(pObj);

			// add object to active set
			pCurSet->m_pMeshes.PushAndGrow(mesh);
		}
	}

	// potentially need to release some objects from the current set
	if (bOk == false)
	{
		ReleaseObjectsFromSet(pCurSet);
		vfxDisplayf("CMovieMeshManager::LoadMovieMeshObjects: Failed to load set \"%s\"", pCurSet->m_loadRequestName.c_str());
		return false;
	}

	return bOk;
}

bool CMovieMeshManager::LoadMovieMeshInfoData(MovieMeshSet* pCurSet)
{
	MovieMeshInfoList& setInfo = pCurSet->m_info;

	// compose full file name
	atFixedString<1024> str = MM_DATA_PATH;
	str.Append(pCurSet->m_loadRequestName);

	bool bOk = fwPsoStoreLoader::LoadDataIntoObject(str, "", setInfo);

	// check mesh info data was parsed successfully
	if (bOk == false)
	{
		vfxDisplayf("CMovieMeshManager::LoadMovieMeshInfoData: Failed to load mesh info data for set \"%s\"", pCurSet->m_loadRequestName.c_str());
		return bOk;
	}

	// assign a valid handle to active set
	pCurSet->m_handle = atStringHash(pCurSet->m_loadRequestName);

	return bOk;
}

bool CMovieMeshManager::LoadMovies(MovieMeshSet* pCurSet)
{	
	const MovieMeshInfoList& setInfo = pCurSet->m_info;
	bool bOk = true;

	for (int i = 0; i < setInfo.m_Data.GetCount(); i++)
	{
		const char* pMovieName = setInfo.m_Data[i].m_TextureName.c_str();

		u32	movieHandle = 0;

#if GTA_REPLAY
		if( CReplayMgr::IsEditModeActive() )
		{
			movieHandle = atStringHash(pMovieName);
		}
		else
#endif	//GTA_REPLAY
		{
			movieHandle = g_movieMgr.PreloadMovie(pMovieName);

			if (g_movieMgr.IsHandleValid(movieHandle) == false)
			{
				vfxDisplayf("CMovieMeshManager::LoadMovies: Failed to load bink movie \"%s\"", pMovieName);
				bOk = false;
				break;
			}
			// start playing movie
			g_movieMgr.Play(movieHandle);
		}

		// add movie handle to set
		if (AddMovieHandle(pCurSet, movieHandle) == false)
		{
			Displayf("CMovieMeshManager::LoadMovies: failed to allocate handle for movie \"%s\"", pMovieName);
			bOk = false;
			break;
		}
	}

	// potentially need to release some movies and objects from the current set
	if (bOk == false)
	{
		ReleaseMoviesFromSet(pCurSet);
		ReleaseObjectsFromSet(pCurSet);
		return false;
	}

	return bOk;
}

bool CMovieMeshManager::LoadNamedRenderTargets(MovieMeshSet* pCurSet)
{
	const MovieMeshInfoList& setInfo = pCurSet->m_info;
	bool bOk = true;

	for (int i = 0; i < setInfo.m_Data.GetCount(); i++)
	{

		atFinalHashString rtName(setInfo.m_Data[i].m_TextureName.c_str());

		if (rtName.IsNull())
		{
			bOk = false;
			vfxDisplayf("CMovieMeshManager::LoadNamedRenderTargets: Failed to setup named render target for \"%s\"", setInfo.m_Data[i].m_TextureName.c_str());
			break;
		}

		// try registering only if it hasn't been registered previously
		u32 rtId = 0U;
		if (m_renderTargetMgr.Exists(rtName))
		{
			rtId = rtName.GetHash();
		}	
		else
		{
			rtId = m_renderTargetMgr.Register(rtName);
		}

		// invalid id
		if (rtId == 0)
		{
			bOk = false;
			break;
		}

		// add render target id to set
		AddRenderTargetHandle(pCurSet->m_pHandles[i], rtId, rtName.GetHash());
	}

	// potentially need to release some movies and objects from the current set
	if (bOk == false)
	{
		ReleaseMoviesFromSet(pCurSet);
		ReleaseObjectsFromSet(pCurSet);
		return false;
	}


	return bOk;
}

void CMovieMeshManager::AddRenderTargetHandle(MovieMeshHandle* pHandle, u32 rtId, u32 rtNameHash)
{
	Assertf(pHandle->movieHandle != INVALID_MOVIE_MESH_HANDLE, "CMovieMeshManager::AddRenderTargetHandle: trying to set up a bink rt, but associated bink movie handle is invalid");
	Assertf(rtId != INVALID_MOVIE_MESH_HANDLE && rtNameHash != INVALID_MOVIE_MESH_HANDLE, "CMovieMeshManager::AddRenderTargetHandle: trying to set up a bink rt with an invalid handle");
	m_globalPool.ModifyHandle(pHandle, pHandle->movieHandle, rtId, rtNameHash);
}

bool CMovieMeshManager::AddMovieHandle(MovieMeshSet* pCurSet, u32 movieHandle) 
{
	Assertf(movieHandle != INVALID_MOVIE_MESH_HANDLE, "CMovieMeshManager::AddMovieHandle: trying to set up a bink movie, but handle is invalid");

	MovieMeshHandle* pHandle = NULL;
	bool bOk = m_globalPool.AddHandle(pHandle, movieHandle);

	if (bOk)
	{
		Assertf(pHandle != NULL, "CMovieMeshManager::AddMovieHandle: null handle");
		pCurSet->m_pHandles.PushAndGrow(pHandle);
	}

	return bOk;
}

void CMovieMeshManager::ReleaseSet(MovieMeshSet* pCurSet)
{
	if (pCurSet == NULL)
	{
		return;
	}

	ReleaseMoviesFromSet(pCurSet);
	ReleaseObjectsFromSet(pCurSet);

	pCurSet->m_handle = INVALID_MOVIE_MESH_SET_HANDLE;
	pCurSet->m_state = MMS_FAILED;
}

void CMovieMeshManager::ReleaseObjectsFromSet(MovieMeshSet* pCurSet)
{
	vfxAssert(pCurSet != NULL);

	// release all objects
	for (int i = 0; i < pCurSet->m_pMeshes.GetCount(); i++)
	{
		CObject* pObject = static_cast<CObject*>(pCurSet->m_pMeshes[i].m_pObj.Get());
		pCurSet->m_pMeshes[i].m_bIsReady = false;

		if (pObject != NULL)
		{
			CObjectPopulation::DestroyObject(pObject);
		}
	}

	// reset array
	pCurSet->m_pMeshes.Reset();

}

void CMovieMeshManager::ReleaseMoviesFromSet(MovieMeshSet* pCurSet)
{
	vfxAssert(pCurSet != NULL);

	// release all objects
	for (int i = 0; i < pCurSet->m_pHandles.GetCount(); i++)
	{
		pCurSet->m_pHandles[i]->Release();
	}

	pCurSet->m_pHandles.Reset();
}

bool CMovieMeshManager::HandleExists(MovieMeshSetHandle handle) const
{
	return (IsHandleValid(handle) && FindSet(handle));
}

CInteriorInst* CMovieMeshManager::GetInteriorAtCoordsOfType(const Vector3& scrVecInCoors, const char* typeName)
{
	u32 typeKey = atStringHash(typeName);

	Vector3 vecPositionToCheck(scrVecInCoors);

	// get a list of MLOs at desired position
	fwIsSphereIntersecting intersection(spdSphere(RCC_VEC3V(vecPositionToCheck), ScalarV(V_FLT_SMALL_1)));
	fwPtrListSingleLink	scanList;
	CInteriorInst::FindIntersectingMLOs(intersection, scanList);

	// get the closest interior of the right type
	fwPtrNode* pNode = scanList.GetHeadPtr();
	CInteriorInst* pClosest = NULL;
	float closestDist2 = 0.0f;

	Vec3V vPositionToCheck = RCC_VEC3V(vecPositionToCheck);
	while(pNode){
		CInteriorInst* pIntInst = reinterpret_cast<CInteriorInst*>(pNode->GetPtr());
		if (pIntInst){
			CBaseModelInfo* pModelInfo = pIntInst->GetBaseModelInfo();
			if (pModelInfo->GetHashKey() == typeKey){
				float dist2 = DistSquared(pIntInst->GetTransform().GetPosition(), vPositionToCheck).Getf();
				if (pClosest){
					if (dist2 < closestDist2){
						pClosest = pIntInst;
						closestDist2 = dist2;
					}
				} else {
					pClosest = pIntInst;
					closestDist2 = dist2;
				}
			}
		}

		pNode = pNode->GetNextPtr();
	}

	return pClosest;
}

bool CMovieMeshManager::IsInteriorReady(CInteriorInst* pInteriorInst)
{
	if (pInteriorInst == NULL)
	{
		return false;
	}

	// tbr: for some reason GetCurrentState is not const, so tough luck for our unprotected pInteriorInst...
	CInteriorProxy* pIntProxy = pInteriorInst->GetProxy();
	Assert(pIntProxy);

	if (pIntProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
	{
		return true;
	}

	return false;
}

void CMovieMeshManager::ExtractRenderTargetName(const char* pInputName, const char*& pOutputName)
{
	const char* scriptRT = strstr(pInputName,MM_SCRIPTED_RT_PREFIX);
	pOutputName = NULL;

	if(scriptRT)
	{	
		pOutputName = scriptRT + strlen(MM_SCRIPTED_RT_PREFIX);
	}
}

void CMovieMeshManager::ReleaseSet(MovieMeshSetHandle handle)
{
	MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet == NULL)
	{
		return;
	}

	pCurSet->m_state = MMS_PENDING_DELETE;
	m_releaseRequestPending = true;

}

eMovieMeshSetState CMovieMeshManager::QuerySetStatus(MovieMeshSetHandle handle) const
{
	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet == NULL)
	{
		// set still hasn't been loaded
		if (m_loadRequestPending == true)
		{
			return MMS_PENDING_LOAD;
		}

		// set doesn't exist
		return MMS_FAILED;
	}

	return pCurSet->m_state;
}

int CMovieMeshManager::GetScriptIdFromHandle(MovieMeshSetHandle handle) const
{
	if (handle == INVALID_MOVIE_MESH_SET_HANDLE)
	{
		return INVALID_MOVIE_MESH_SCRIPT_ID;
	}

	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet == NULL)
	{
		vfxDisplayf("CMovieMeshManager::GetScriptIdFromHandle: could not find set for valid handle");
		return INVALID_MOVIE_MESH_SCRIPT_ID;
	}

	return pCurSet->m_scriptId;
}

MovieMeshSetHandle CMovieMeshManager::GetHandleFromScriptId(int idx) const
{
	// script id is invalid
	if (idx == INVALID_MOVIE_MESH_SCRIPT_ID || idx < 0 || idx >= MOVIE_MESH_MAX_SETS)	{
		Assertf(idx >= 0 && idx < MOVIE_MESH_MAX_SETS, "CMovieMeshManager::GetHandleFromScriptId: script id out of range");
		return INVALID_MOVIE_MESH_SET_HANDLE;
	}

	const MovieMeshSet* pCurSet = &m_setPool[idx-1];	
	return pCurSet->m_handle;

}

#if __BANK
const char* CMovieMeshManager::GetSetName(MovieMeshSetHandle handle) const
{
	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet != NULL)
	{
		return pCurSet->m_loadRequestName.c_str();
	}

	return NULL;
}

void CMovieMeshManager::SetMeshIsVisible(MovieMeshSetHandle handle, int i, bool isVisible)
{
	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet != NULL && i < pCurSet->m_pMeshes.GetCount())
	{
		pCurSet->m_pMeshes[i].m_pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_VFX, isVisible);
	}
}

bool CMovieMeshManager::GetMeshIsVisible(MovieMeshSetHandle handle, int i) const
{
	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet != NULL && i < pCurSet->m_pMeshes.GetCount())
	{
		return pCurSet->m_pMeshes[i].m_pObj->GetIsVisible();
	}

	return false;
}

int	CMovieMeshManager::GetNumMeshes(MovieMeshSetHandle handle) const
{
	const MovieMeshSet* pCurSet = FindSet(handle);

	if (pCurSet != NULL)
	{
		return pCurSet->m_info.m_Data.GetCount();
	}

	return 0;
}

void CMovieMeshManager::InitWidgets()
{
	CMovieMeshDebugTool::InitWidgets();
}
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
void CMovieMeshManager::CountMemoryUsageForMovies(MovieMeshSetHandle handle)
{
	const MovieMeshSet* pCurSet = FindSet(handle);
	if (pCurSet != NULL)
	{
		for (int i = 0; i < pCurSet->m_pHandles.GetCount(); i++)
		{
			u32 movieHandle = pCurSet->m_pHandles[i]->movieHandle;
			CTheScripts::GetScriptHandlerMgr().AddToMoviesUsedByMovieMeshes(movieHandle);
		}
	}
}

void CMovieMeshManager::CountMemoryUsageForRenderTargets(MovieMeshSetHandle)
{

}

void CMovieMeshManager::CountMemoryUsageForObjects(MovieMeshSetHandle handle)
{
	const MovieMeshSet* pCurSet = FindSet(handle);
	if (pCurSet != NULL)
	{
		for (int i = 0; i < pCurSet->m_pMeshes.GetCount(); i++)
		{
			CObject* pObject = static_cast<CObject*>(pCurSet->m_pMeshes[i].m_pObj.Get());
			if (pObject != NULL)
			{
				CTheScripts::GetScriptHandlerMgr().AddToModelsUsedByMovieMeshes(pObject->GetModelIndex());
			}
		}
	}
}
#endif // __SCRIPT_MEM_CALC


//////////////////////////////////////////////////////////////////////////
// CMovieMeshDebugTool
//////////////////////////////////////////////////////////////////////////
#if __BANK

// widgets
bkGroup*			CMovieMeshDebugTool::ms_pBankGroup				= NULL;
bkCombo*			CMovieMeshDebugTool::ms_pLoadedSetsCombo		= NULL;
bkText*				CMovieMeshDebugTool::ms_pLoadRequestBox			= NULL;
bkButton*			CMovieMeshDebugTool::ms_pCreateButton			= NULL;
bkButton*			CMovieMeshDebugTool::ms_pLoadSetButton			= NULL;
bkButton*			CMovieMeshDebugTool::ms_pReleaseSetButton		= NULL;

const char*			CMovieMeshDebugTool::ms_comboSetNames[MOVIE_MESH_MAX_SETS]; 
MovieMeshSetHandle	CMovieMeshDebugTool::ms_setHandles[MOVIE_MESH_MAX_SETS];
char				CMovieMeshDebugTool::ms_strSetName[128]			= "\0";
bool				CMovieMeshDebugTool::ms_bActiveSetInitialised	= false;
int					CMovieMeshDebugTool::ms_comboSelIndex			= 0;
const char*			CMovieMeshDebugTool::ms_comboEmptyMessage		= "[empty slot]";
bool				CMovieMeshDebugTool::ms_bIgnoreAnimData			= false;
bool				CMovieMeshDebugTool::ms_bShowFireBoundingSpheres= false;

void CMovieMeshDebugTool::InitWidgets()
{

	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		ms_comboSetNames[i] = &ms_comboEmptyMessage[0];
		ms_setHandles[i] = INVALID_MOVIE_MESH_HANDLE;
	}

	ms_pBankGroup = vfxWidget::GetBank()->PushGroup("Movie Mesh Manager");
	ms_pCreateButton = vfxWidget::GetBank()->AddButton("Create Movie Mesh Debug Widgets", CFA1(CMovieMeshDebugTool::CreateWidgetsOnDemand));
	vfxWidget::GetBank()->PopGroup();

}

void CMovieMeshDebugTool::CreateWidgetsOnDemand()
{
	bkBank* pBank = vfxWidget::GetBank();

	if(ms_pCreateButton)
	{
		pBank->Remove(*((bkWidget*)ms_pCreateButton));
		ms_pCreateButton = NULL;
	}

	pBank->SetCurrentGroup(*ms_pBankGroup);
		pBank->AddToggle("Show Fire Bounding Spheres", &ms_bShowFireBoundingSpheres);
		ms_pLoadRequestBox = pBank->AddText("Enter set name:", &ms_strSetName[0], 128, false, NullCB);
		ms_pLoadSetButton = pBank->AddButton("Load Movie Mesh Set", CFA1(CMovieMeshDebugTool::LoadSet));
		pBank->AddToggle("Ignore Animation Data When Loading Set", &ms_bIgnoreAnimData, CFA1(CMovieMeshDebugTool::OnIgnoreAnimDataToggleChange));
		pBank->AddSeparator();
		ms_pReleaseSetButton = pBank->AddButton("Release Movie Mesh Set", CFA1(CMovieMeshDebugTool::ReleaseSet));
		ms_pLoadedSetsCombo = pBank->AddCombo("Available Set Slots", &ms_comboSelIndex, MOVIE_MESH_MAX_SETS, &ms_comboSetNames[0], NullCB);
	pBank->UnSetCurrentGroup(*ms_pBankGroup);
}

void CMovieMeshDebugTool::LoadSet()
{
	if (IsInputStringValid())
	{
		
		for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++) 
		{
			if (ms_setHandles[i] == INVALID_MOVIE_MESH_HANDLE) 
			{
				ms_setHandles[i] = g_movieMeshMgr.LoadSet(ms_strSetName);
				ms_comboSetNames[i] = g_movieMeshMgr.GetSetName(ms_setHandles[i]);
				RefreshSetCombo();
				return;
			}
		}

	}
}

void CMovieMeshDebugTool::ReleaseSet()
{
	if (ms_comboSelIndex <0 || ms_comboSelIndex >= MOVIE_MESH_MAX_SETS)
	{
		Displayf("CMovieMeshDebugTool::ReleaseSet: trying to release an invalid set (index %d)", ms_comboSelIndex);
		return;
	}

	MovieMeshSetHandle curHandle = ms_setHandles[ms_comboSelIndex];
	if (g_movieMeshMgr.HandleExists(curHandle))
	{
		g_movieMeshMgr.ReleaseSet(curHandle);
		ms_setHandles[ms_comboSelIndex] = INVALID_MOVIE_MESH_HANDLE;
		ms_comboSetNames[ms_comboSelIndex] = &ms_comboEmptyMessage[0];
		ms_comboSelIndex = 0;
		RefreshSetCombo();
	}
}

void CMovieMeshDebugTool::RefreshSetCombo()
{
	// refresh combo
	bkBank* pBank = vfxWidget::GetBank();
	pBank->SetCurrentGroup(*ms_pBankGroup);
	pBank->Remove(*((bkWidget*)ms_pLoadedSetsCombo));
	ms_pLoadedSetsCombo = pBank->AddCombo("Available Set Slots", &ms_comboSelIndex, MOVIE_MESH_MAX_SETS, &ms_comboSetNames[0], NullCB);
	pBank->UnSetCurrentGroup(*ms_pBankGroup);
}

void CMovieMeshDebugTool::Render(const MovieMeshSet* pCurSet)
{
	// render bounding spheres
	if (ms_bShowFireBoundingSpheres)
	{
		if (pCurSet)
		{
			const MovieMeshInfoList& setInfo = pCurSet->m_info;
			for (int k=0; k < setInfo.m_BoundingSpheres.GetCount(); k++)
			{
				const Vector3 spherePos = Vector3(setInfo.m_BoundingSpheres[k].x, setInfo.m_BoundingSpheres[k].y, setInfo.m_BoundingSpheres[k].z);
				const float radius = setInfo.m_BoundingSpheres[k].w;
				grcDebugDraw::Sphere(spherePos, radius, Color32(255, 0, 0, 128));
			}
		}
	}
}

void CMovieMeshDebugTool::Update()
{
	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{	
		if (ms_setHandles[i] == INVALID_MOVIE_MESH_HANDLE)
		{
			continue;
		}
		// if there's a set that failed to load or that is about to be deleted, remove it from the combo box
		if (g_movieMeshMgr.QuerySetStatus(ms_setHandles[i]) == MMS_FAILED ||
			g_movieMeshMgr.QuerySetStatus(ms_setHandles[i]) == MMS_PENDING_DELETE)
		{
			ms_setHandles[i] = INVALID_MOVIE_MESH_HANDLE;
			ms_comboSetNames[ms_comboSelIndex] = &ms_comboEmptyMessage[0];
			RefreshSetCombo();
		}
	}
}



bool CMovieMeshDebugTool::IsInputStringValid()
{
	int strLength = istrlen(ms_strSetName);
	return (strLength > 0);
}

void CMovieMeshDebugTool::OnComboSelectionChange()
{
}

void CMovieMeshDebugTool::OnIgnoreAnimDataToggleChange()
{
	g_movieMeshMgr.SetIgnoreAnimDataOnLoad(ms_bIgnoreAnimData);
}

#endif // __BANK
