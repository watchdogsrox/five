// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "grcore/debugdraw.h"
#include "spatialdata/transposedplaneset.h"

// framework headers
#include "fwmaths/Vector.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/stores/mapdatastore.h"

// game headers
#include "ai/navmesh/navmeshextents.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugLighting.h"
#include "entity/entity.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/Lights.h"
#include "renderer/Lights/LODLights.h"
#include "Renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "system/System.h"
#include "timecycle/TimeCycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "objects/DummyObject.h"
#include "Objects/Object.h"
#include "vector/color32.h"
#include "vfx/channel.h"
#include "vfx/misc/Coronas.h"
#include "vfx/misc/DistantLights.h"
#include "vfx/misc/DistantLightsVertexBuffer.h"
#include "vfx/misc/LODLightManager.h"
#include "vfx/vfxutil.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

VFX_MISC_OPTIMISATIONS();

bool CLODLightManager::m_LODLightsCanLoad = true;
bool CLODLightManager::m_requireRenderthreadFrameInfoClear = false;

fwLinkList<CLODLightBucket> CLODLightManager::m_LODLightBuckets;
fwLinkList<CDistantLODLightBucket> CLODLightManager::m_DistantLODLightBuckets;

atArray<u16> CLODLightManager::m_LODLightImapIDXs;

LODLightData CLODLightManager::m_LODLightData;
LODLightData* CLODLightManager::m_currentFrameInfo = NULL;

// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

class dlCmdSetupLODLightsFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupLODLightsFrameInfo,
	};

	dlCmdSetupLODLightsFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupLODLightsFrameInfo &) cmd).Execute(); }
	void Execute();

private:			

	LODLightData* newFrameInfo;
};

class dlCmdClearLODLightsFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearLODLightsFrameInfo,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearLODLightsFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupLODLightsFrameInfo::dlCmdSetupLODLightsFrameInfo()
{
	newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<LODLightData>(DPT_LIFETIME_RENDER_THREAD, sizeof(LODLightData));
	sysMemCpy(newFrameInfo, CLODLightManager::GetLODLightData(), sizeof(LODLightData));

	CLODLightManager::ResetUpdateLODLightsDisabledThisFrame();
}

void dlCmdSetupLODLightsFrameInfo::Execute()
{
	CLODLightManager::m_currentFrameInfo = newFrameInfo;
}


void dlCmdClearLODLightsFrameInfo::Execute()
{
	CLODLightManager::m_currentFrameInfo = NULL;
}

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

CLODLightManager::CLODLightManager()
{
}

// ----------------------------------------------------------------------------------------------- //

static rage::ServiceDelegate ms_serviceDelegate;

static void OnServiceEvent( rage::sysServiceEvent* evnt )
{
	if(evnt != NULL)
	{
		switch(evnt->GetType())
		{
		case sysServiceEvent::RESUME_IMMEDIATE:
			{
				CLODLightManager::Shutdown(SHUTDOWN_SESSION);
			}
			break;
		default:
			{
				break;
			}
		}
	}
}


void CLODLightManager::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		m_LODLightBuckets.Init(MAX_NUM_LOD_LIGHT_BUCKETS);
		m_DistantLODLightBuckets.Init(MAX_NUM_LOD_LIGHT_BUCKETS);

		ms_serviceDelegate.Bind(OnServiceEvent);
		g_SysService.AddDelegate(&ms_serviceDelegate);
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION || shutdownMode == SHUTDOWN_CORE)
	{
		m_requireRenderthreadFrameInfoClear = true;

		m_LODLightData.m_LODLightsToRender.Reset();
		m_LODLightData.m_DistLODLightsToRender[LODLIGHT_CATEGORY_SMALL].Reset();
		m_LODLightData.m_DistLODLightsToRender[LODLIGHT_CATEGORY_MEDIUM].Reset();
		m_LODLightData.m_DistLODLightsToRender[LODLIGHT_CATEGORY_LARGE].Reset();
	}

	if(shutdownMode == SHUTDOWN_CORE)
	{
		g_SysService.RemoveDelegate(&ms_serviceDelegate);
		ms_serviceDelegate.Reset();

		m_LODLightBuckets.Shutdown();
		m_DistantLODLightBuckets.Shutdown();
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::Update()
{
	SetIfCanLoad();
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::AddLODLight(CLODLight &LODLights, s32 mapDataSlotIndex, s32 parentMapDataSlotIndex)
{
	// Assert to catch if mapDataSlotIndex goes beyond the 16 bits of storage I've allowed for it.
	Assertf(mapDataSlotIndex < 65536, "mapDataSlotIndex above 16 bits");

	// Don't add more than we can store!
	if(Verifyf(m_LODLightBuckets.GetNumFree() > 0, "LODLightManager: ERROR - Ran out of buckets!"))
	{
		CLODLightBucketNode* pNode = m_LODLightBuckets.Insert();
		CLODLightBucket* pBucket = &(pNode->item);
		pBucket->mapDataSlotIndex = (u16)mapDataSlotIndex;
		pBucket->parentMapDataSlotIndex = (u16)parentMapDataSlotIndex;

		pBucket->lightCount = (u16)LODLights.m_direction.GetCount();
		pBucket->pLights = &LODLights;

		// Find the parent for this LODLight
		CDistantLODLightBucket *pParent = FindDistantLODLightBucket(parentMapDataSlotIndex);
		Assertf(pParent != NULL, "LODLightManager: Cannot find DistantLODLight for LODLight");
		pBucket->pDistLights = pParent->pLights;

		BANK_ONLY(DebugLighting::UpdateStatsLODLights(m_LODLightBuckets.GetNumUsed(), pBucket->lightCount, LODLIGHT_STANDARD));
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::RemoveLODLight(s32 mapDataSlotIndex)
{
	CLODLightBucketNode* pNode = m_LODLightBuckets.GetFirst()->GetNext();
	// Delete all LODLights with matching map indices.
	while(pNode != m_LODLightBuckets.GetLast())
	{
		CLODLightBucket* pCurEntry = &(pNode->item);
		CLODLightBucketNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		if (pCurEntry->mapDataSlotIndex == mapDataSlotIndex)
		{
			// Delete this entry.
			BANK_ONLY(DebugLighting::UpdateStatsLODLights(m_LODLightBuckets.GetNumUsed() - 1, -pCurEntry->lightCount, LODLIGHT_STANDARD));
			m_LODLightBuckets.Remove(pLastNode);

			// B*3371677: Crash - CLODLights::RenderLODLights():
			// ... and remove from m_LODLightsToRender list (if still there):
			atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS>& LODLightsToRender = GetLODLightsToRenderUpdateBuffer();
			const u32 count = LODLightsToRender.GetCount();
			for(int i=0; i<count; i++)
			{
				if(LODLightsToRender[i] == pCurEntry)
				{
					LODLightsToRender[i] = NULL;
					break;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

CDistantLODLightBucket *CLODLightManager::FindDistantLODLightBucket(s32 mapDataSlotIndex)
{

	CDistantLODLightBucketNode* pNode = m_DistantLODLightBuckets.GetFirst()->GetNext();
	while(pNode != m_DistantLODLightBuckets.GetLast())
	{
		CDistantLODLightBucket* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->mapDataSlotIndex == mapDataSlotIndex)
		{
			return pCurEntry;
		}
	}
	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::AddDistantLODLight(CDistantLODLight &distLODLights, s32 mapDataSlotIndex)
{
	// Assert to catch if mapDataSlotIndex goes beyond the 16 bits of storage I've allowed for it.
	Assertf(mapDataSlotIndex < 65536, "mapDataSlotIndex above 16 bits");

	// Don't add more than we can store!
	if(Verifyf(m_DistantLODLightBuckets.GetNumFree() > 0, "LODLightManager: ERROR - Ran out of Distantlight buckets!"))
	{
		CDistantLODLightBucketNode* pNode = m_DistantLODLightBuckets.Insert();
		CDistantLODLightBucket* pBucket = &(pNode->item);
		pBucket->mapDataSlotIndex = (u16)mapDataSlotIndex;
		pBucket->lightCount = (u16)distLODLights.m_position.GetCount();
		pBucket->pLights = &distLODLights;
		BANK_ONLY(DebugLighting::UpdateStatsLODLights(m_DistantLODLightBuckets.GetNumUsed(), pBucket->lightCount, LODLIGHT_DISTANT));
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::RemoveDistantLODLight(s32 mapDataSlotIndex)
{
	CDistantLODLightBucketNode* pNode = m_DistantLODLightBuckets.GetFirst()->GetNext();
	// Delete all LODLights with matching map indices.
	while(pNode != m_DistantLODLightBuckets.GetLast())
	{
		CDistantLODLightBucket* pCurEntry = &(pNode->item);
		CDistantLODLightBucketNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		if (pCurEntry->mapDataSlotIndex == mapDataSlotIndex)
		{
			BANK_ONLY(DebugLighting::UpdateStatsLODLights(m_DistantLODLightBuckets.GetNumUsed() - 1, -pCurEntry->lightCount, LODLIGHT_DISTANT));
			m_DistantLODLightBuckets.Remove(pLastNode);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::ResetLODLightsBucketVisibility()
{
	atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &LODLightsToRender = GetLODLightsToRenderUpdateBuffer();
	LODLightsToRender.Reset();
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::BuildLODLightsBucketVisibility(CViewport *pViewport)
{
	atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &LODLightsToRender = GetLODLightsToRenderUpdateBuffer();
	LODLightsToRender.Reset();

	if(!m_LODLightsCanLoad)
	{
		return;
	}

	// Setup cull planes (use exterior quad if in an interior or just the frustum)
	const grcViewport& grcVP = pViewport->GetGrcViewport();
	const Vec3V vCameraPos = grcVP.GetCameraPosition();

	spdTransposedPlaneSet8 cullPlanes;
	spdTransposedPlaneSet8 waterReflectioncullPlanes;
	if(g_SceneToGBufferPhaseNew && 
	   g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
	   g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid())
	{
		const bool bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

		if(bInteriorCullAll == false)
		{
			fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
			quad.GenerateFrustum(grcVP.GetCompositeMtx(), cullPlanes);
		}
		else
		{
			return;
		}		
	}
	else
	{
		cullPlanes.Set(grcVP, true, true);
	}
	Vec3V vWaterReflectionCameraPos;
	bool bCheckReflectionPhase = BANK_ONLY(DebugLighting::m_LODLightCoronaEnableWaterReflectionCheck && ) (Water::UseHQWaterRendering());
	const grcViewport* grcWaterReflectionVP = CRenderPhaseWaterReflectionInterface::GetViewport();
	if(grcWaterReflectionVP)
	{
		vWaterReflectionCameraPos = grcWaterReflectionVP->GetCameraPosition();
		waterReflectioncullPlanes.Set(*grcWaterReflectionVP, true, true);
	}
	else
	{
		bCheckReflectionPhase = false;
	}


	fwMapDataStore& mapStore = fwMapDataStore::GetStore();

	u32 startIndex = 0;
	u32 endIndex = m_LODLightBuckets.GetNumUsed();

	#if __BANK
		if (DebugLighting::m_LODLightBucketIndex > -1)
		{
			startIndex = DebugLighting::m_LODLightBucketIndex;
			endIndex = DebugLighting::m_LODLightBucketIndex + 1;
		}
	#endif


	spdSphere lodLightSphere[LODLIGHT_CATEGORY_COUNT];
	spdSphere waterReflectionLODLightSphere[LODLIGHT_CATEGORY_COUNT];
	for (u32 i = 0; i < LODLIGHT_CATEGORY_COUNT; i++)
	{
		ScalarV vlodLightRange =  ScalarV(CLODLights::GetUpdateLodLightRange((eLodLightCategory)i));
		lodLightSphere[i] = spdSphere(vCameraPos, vlodLightRange);
		waterReflectionLODLightSphere[i] = spdSphere(vWaterReflectionCameraPos, vlodLightRange);
	}

	CLODLightBucketNode* pNode = m_LODLightBuckets.GetFirst()->GetNext();
	//Skip to the start Index
	for(int i=0; i < startIndex; i++)
	{
		pNode = pNode->GetNext();
	}

	for(int i = startIndex; i < endIndex; i++)
	{
		CLODLightBucket *pBucket = &(pNode->item);
		pNode = pNode->GetNext();
		const spdAABB &physBounds = mapStore.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));

		CDistantLODLight *pBaseData = pBucket->pDistLights;
		const eLodLightCategory currentCategory = (eLodLightCategory)pBaseData->m_category;

		//Check if whole bucket is visible in any of the phases (main or water reflection)
		if(cullPlanes.IntersectsAABB(physBounds) || (bCheckReflectionPhase && waterReflectioncullPlanes.IntersectsAABB(physBounds)))
		{
			//Not sure if we should put this check before or after the stream index stuff
#if __BANK
			if( !DebugLighting::m_LODLightsVisibilityOnMainThread || 
				( DebugLighting::m_LODLightsVisibilityOnMainThread &&
				(physBounds.IntersectsSphere(lodLightSphere[currentCategory])|| (bCheckReflectionPhase && physBounds.IntersectsSphere(waterReflectionLODLightSphere[currentCategory])))))
#else
			if(physBounds.IntersectsSphere(lodLightSphere[currentCategory]) || (bCheckReflectionPhase && physBounds.IntersectsSphere(waterReflectionLODLightSphere[currentCategory])))
#endif
			{
				// Add a ref to ensure this data doesn't vanish
				fwMapDataStore::GetStore().AddRef(strLocalIndex(pBucket->mapDataSlotIndex), REF_RENDER);
				gDrawListMgr->AddRefCountedModuleIndex(pBucket->mapDataSlotIndex, &fwMapDataStore::GetStore());

				// Add bucket to render
				LODLightsToRender.Push(pBucket);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void	CLODLightManager::BuildDistLODLightsVisibility(CViewport *pViewport)
{
	atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS>* DistLODLightsToRender = GetDistLODLightsToRenderUpdateBuffers();
	
	for (u32 i = 0; i < LODLIGHT_CATEGORY_COUNT; i++)
	{
		DistLODLightsToRender[i].Reset();
	}

	if(!m_LODLightsCanLoad)
	{
		// Swap for next frame
		return;
	}

	// Setup cull planes (use exterior quad if in an interior or just the frustum)
	const grcViewport& grcVP = pViewport->GetGrcViewport();

	spdTransposedPlaneSet8 cullPlanes;
	if(g_SceneToGBufferPhaseNew && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid()
		BANK_ONLY(&& !g_distantLights.GetDisableStreetLightsVisibility()))
	{
		const bool bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

		if(bInteriorCullAll == false)
		{
			fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
			quad.GenerateFrustum(grcVP.GetCompositeMtx(), cullPlanes);
		}
		else
		{
			return;
		}
	}
	else
	{
		cullPlanes.Set(grcVP, true, true);
	}

	fwMapDataStore& mapStore = fwMapDataStore::GetStore();

	u32 startIndex = 0;
	u32 endIndex = m_DistantLODLightBuckets.GetNumUsed();

	#if __BANK
		if (DebugLighting::m_distantLODLightBucketIndex > -1)
		{
			startIndex = DebugLighting::m_distantLODLightBucketIndex;
			endIndex = DebugLighting::m_distantLODLightBucketIndex + 1;
		}
	#endif

	CDistantLODLightBucketNode* pNode = m_DistantLODLightBuckets.GetFirst()->GetNext();
	//Skip to the start Index
	for(int i=0; i < startIndex; i++)
	{
		pNode = pNode->GetNext();
	}

	for(int i = startIndex; i < endIndex; i++)
	{			
		CDistantLODLightBucket *pBucket = &(pNode->item);
		pNode = pNode->GetNext();
		const spdAABB &physBounds = mapStore.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));
		if(cullPlanes.IntersectsAABB(physBounds) BANK_ONLY(|| g_distantLights.GetDisableStreetLightsVisibility()))
		{
			// Add a ref to ensure this data doesn't vanish
			fwMapDataStore::GetStore().AddRef(strLocalIndex(pBucket->mapDataSlotIndex), REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(pBucket->mapDataSlotIndex, &fwMapDataStore::GetStore());

			DistLODLightsToRender[pBucket->pLights->m_category].Push(pBucket);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::RegisterBrokenLightsForEntity(CEntity *pEntity)
{
	// LOD Lights
	if(pEntity->GetIsTypeObject())
	{
		CObject *pObject = static_cast<CObject*>(pEntity);
		CDummyObject* pDummy = pObject->GetRelatedDummy();
		if(pDummy)
		{
			CBaseModelInfo* pModelInfo = pDummy->GetBaseModelInfo();
			GET_2D_EFFECTS(pModelInfo);
			for(int j=0;j<numEffects;j++)
			{
				C2dEffect *pEffect = a2dEffects[j];
				if(pEffect->GetType() == ET_LIGHT )
				{
					//Displayf("Found a light in a broken object %s", pModelInfo->GetModelName());
					Lights::RegisterBrokenLight(pDummy, j);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::UnregisterBrokenLightsForEntity(CEntity *pEntity)
{
	// LOD Lights
	if(pEntity->GetIsTypeObject())
	{
		CObject *pObject = static_cast<CObject*>(pEntity);
		CDummyObject* pDummy = pObject->GetRelatedDummy();
		if(pDummy)
		{
			CBaseModelInfo* pModelInfo = pDummy->GetBaseModelInfo();
			GET_2D_EFFECTS(pModelInfo);

			for(int j=0;j<numEffects;j++)
			{
				C2dEffect *pEffect = a2dEffects[j];
				if(pEffect->GetType() == ET_LIGHT )
				{
					u32 hash = GenerateLODLightHash(pEntity, j);
					Lights::RemoveRegisteredBrokenLight(hash);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //
// Whizzes through the vfxUtil/frags broken this frame stuff, and find objects that were broken this frame that have lights
// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::CheckFragsForBrokenLights()
{
	for(int i=0;i<vfxUtil::GetNumBrokenFrags();i++)
	{
		vfxBrokenFragInfo &brokenFrag = vfxUtil::GetBrokenFragInfo(i);
		// Check if this frag has a parent that has a dummy that has lights
		CEntity *pEntity = static_cast<CEntity*>(brokenFrag.pParentEntity.Get());
		if(pEntity && pEntity->GetIsTypeObject())
		{
			CObject *pObj = (CObject*)pEntity;
			CDummyObject* pDummy = pObj->GetRelatedDummy();
			if(pDummy)
			{
				CBaseModelInfo* pModelInfo = pDummy->GetBaseModelInfo();
				GET_2D_EFFECTS(pModelInfo);
				for(int j=0;j<numEffects;j++)
				{
					C2dEffect *pEffect = a2dEffects[j];
					if(pEffect->GetType() == ET_LIGHT )
					{
						//Displayf("Found a light in a broken object %s", pModelInfo->GetModelName());
						Lights::RegisterBrokenLight(pDummy, j);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::UnbreakLODLight(CLightEntity *pLightEntity)
{
	u32 hash = GenerateLODLightHash(pLightEntity);
	Lights::RemoveRegisteredBrokenLight(hash);
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::SetIfCanLoad()
{
	u32 hour = CClock::GetHour();

	if(	hour >= LODLIGHT_TURN_OFF_TIME && hour < LODLIGHT_TURN_ON_TIME )
	{
		if( m_LODLightsCanLoad )
		{	// Turn them off
			m_LODLightsCanLoad = false;
			TurnLODLightLoadingOnOff(m_LODLightsCanLoad);
		}
	}
	else
	{
		if( !m_LODLightsCanLoad )
		{	// Turn them on
			m_LODLightsCanLoad = true;
			TurnLODLightLoadingOnOff(m_LODLightsCanLoad);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::TurnLODLightLoadingOnOff(bool bOn)
{
	// run over LODLight imap cache 
	for(int i = 0; i < m_LODLightImapIDXs.GetCount(); i++)
	{
		u16 iMapIDX = m_LODLightImapIDXs[i];
		INSTANCE_STORE.GetBoxStreamer().SetIsCulled((s32)iMapIDX, !bOn);
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::CreateLODLightImapIDXCache()
{
	m_LODLightImapIDXs.Reset();
	fwMapDataStore& mapStore = fwMapDataStore::GetStore();

	for(int i = 0; i < mapStore.GetCount(); i++)
	{
		fwMapDataDef* pDef = mapStore.GetSlot(strLocalIndex(i));
		if(pDef && pDef->GetIsValid() && pDef->IsLodLightSpecific())
		{
			m_LODLightImapIDXs.Grow() = (u16)i;
		}
	}
}

void CLODLightManager::CacheLoadedLODLightImapIDX()
{
	m_LODLightImapIDXs.Reset();
	fwMapDataStore& mapStore = fwMapDataStore::GetStore();

	// Load first LOD lights and then distant LOD lights so the cache can be safely used for unloading map data.
	for(int i = 0; i < mapStore.GetCount(); i++)
	{
		strLocalIndex index(i);
		fwMapDataDef* pDef = mapStore.GetSlot(index);
		if(pDef && pDef->GetIsValid() && (pDef->GetContentFlags()&fwMapData::CONTENTFLAG_LOD_LIGHTS) && mapStore.HasObjectLoaded(index))
		{
			m_LODLightImapIDXs.Grow() = (u16)i;
		}
	}
	for(int i = 0; i < mapStore.GetCount(); i++)
	{
		strLocalIndex index(i);
		fwMapDataDef* pDef = mapStore.GetSlot(index);
		if(pDef && pDef->GetIsValid() && (pDef->GetContentFlags()&fwMapData::CONTENTFLAG_DISTANT_LOD_LIGHTS) && mapStore.HasObjectLoaded(index))
		{
			m_LODLightImapIDXs.Grow() = (u16)i;
		}
	}
}

void CLODLightManager::LoadLODLightImapIDXCache(u32 filterMask /* = LODLIGHT_IMAP_ANY */)
{
	for(int i = 0; i < m_LODLightImapIDXs.GetCount(); i++)
	{
		strLocalIndex index(m_LODLightImapIDXs[i]);
		ASSERT_ONLY(fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(index));
		FatalAssertf(pDef && pDef->GetIsValid() && pDef->GetContentFlags()&fwMapData::CONTENTFLAG_MASK_LODLIGHTS, 
			"Unexpected invalid LOD light map data index");
		if (HasLODLightImapFilterMask(index, filterMask) && !fwMapDataStore::GetStore().HasObjectLoaded(index))
		{
			fwMapDataStore::GetStore().StreamingRequest(index, STRFLAG_DONTDELETE);
		}
	}
}

void CLODLightManager::UnloadLODLightImapIDXCache(u32 filterMask /* = LODLIGHT_IMAP_ANY */)
{
	// Ensure gRenderThreadInterface.Flush() is called before using in case any IMAP is in use.
	for(int i = 0; i < m_LODLightImapIDXs.GetCount(); i++)
	{
		strLocalIndex index(m_LODLightImapIDXs[i]);
		ASSERT_ONLY(fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(index));
		FatalAssertf(pDef && pDef->GetIsValid() && pDef->GetContentFlags()&fwMapData::CONTENTFLAG_MASK_LODLIGHTS, 
			"Unexpected invalid LOD light map data index");
		if (HasLODLightImapFilterMask(index, filterMask))
		{
			if (fwMapDataStore::GetStore().HasObjectLoaded(index))
			{
				fwMapDataStore::GetStore().ClearRequiredFlag(m_LODLightImapIDXs[i], STRFLAG_DONTDELETE);
				fwMapDataStore::GetStore().SafeRemove(index);
			}
			fwMapDataStore::GetStore().SetStreamable(index, false);
		}
	}
}

bool CLODLightManager::HasLODLightImapFilterMask(strLocalIndex imapIndex, u32 filterMask)
{
	return ((filterMask & LODLIGHT_IMAP_OWNED_BY_SCRIPT) && fwMapDataStore::GetStore().IsScriptManaged(imapIndex)) ||
		((filterMask & LODLIGHT_IMAP_OWNED_BY_MAP_DATA) && !fwMapDataStore::GetStore().IsScriptManaged(imapIndex));
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::SetupRenderthreadFrameInfo()
{
	DLC(dlCmdSetupLODLightsFrameInfo, ());
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::ClearRenderthreadFrameInfo()
{
	DLC(dlCmdClearLODLightsFrameInfo, ());
}

// ----------------------------------------------------------------------------------------------- //

void CLODLightManager::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupLODLightsFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearLODLightsFrameInfo);
}

// ----------------------------------------------------------------------------------------------- //
// Generates a hash based on the bounding box extents of the entity and the light ID
// ----------------------------------------------------------------------------------------------- //

u32	CLODLightManager::GenerateLODLightHash(const CLightEntity *pLightEntity )
{
	const CEntity *pSourceEntity = pLightEntity->GetParent();
	Assert(pSourceEntity);
	s32 lightID = pLightEntity->Get2dEffectId();
	return GenerateLODLightHash(pSourceEntity, lightID );
}

u32	CLODLightManager::GenerateLODLightHash(const CEntity *pSourceEntity, s32 lightID )
{
	s32	hashArray[7];

	spdAABB boundBox;
	pSourceEntity->GetAABB(boundBox);

	Vector3 bbMin = VEC3V_TO_VECTOR3(boundBox.GetMin()); 
	Vector3 bbMax = VEC3V_TO_VECTOR3(boundBox.GetMax());

	bbMin *= 10.0f;
	bbMax *= 10.0f;

	hashArray[0] = static_cast<s32>(bbMin.x);
	hashArray[1] = static_cast<s32>(bbMin.y);
	hashArray[2] = static_cast<s32>(bbMin.z);
	hashArray[3] = static_cast<s32>(bbMax.x);
	hashArray[4] = static_cast<s32>(bbMax.y);
	hashArray[5] = static_cast<s32>(bbMax.z);
	hashArray[6] = static_cast<s32>(lightID);

	return hash((u32*)hashArray, 7, 0);
}

// ----------------------------------------------------------------------------------------------- //

// Macros internal to hash()
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define mix(a,b,c)						\
{										\
	a -= c;  a ^= rot(c, 4);  c += b;	\
	b -= a;  b ^= rot(a, 6);  a += c;	\
	c -= b;  c ^= rot(b, 8);  b += a;	\
	a -= c;  a ^= rot(c,16);  c += b;	\
	b -= a;  b ^= rot(a,19);  a += c;	\
	c -= b;  c ^= rot(b, 4);  b += a;	\
}

#define final(a,b,c)		\
{							\
	c ^= b; c -= rot(b,14);	\
	a ^= c; a -= rot(c,11);	\
	b ^= a; b -= rot(a,25);	\
	c ^= b; c -= rot(b,16);	\
	a ^= c; a -= rot(c,4);	\
	b ^= a; b -= rot(a,14);	\
	c ^= b; c -= rot(b,24);	\
}

// ----------------------------------------------------------------------------------------------- //
// k - the key, an array of uint32_t values
// length - the length of the key, as u32
// initval - the previous hash, or an arbitrary value
// ----------------------------------------------------------------------------------------------- //

u32	CLODLightManager::hash(	const u32 *k, u32 length, u32 initval)
{
	u32 a,b,c;

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + (length<<2) + initval;

	/* handle most of the key */
	while (length > 3)
	{
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a,b,c);
		length -= 3;
		k += 3;
	}

	/* handle the last 3 uint32_t's, all the case statements fall through */
	switch(length)
	{ 
	case 3 : c+=k[2];
	case 2 : b+=k[1];
	case 1 : a+=k[0];
		final(a,b,c);
	case 0:     /* case 0: nothing left to add */
		break;
	}
	/* report the result */
	return c;
}

// ----------------------------------------------------------------------------------------------- //
