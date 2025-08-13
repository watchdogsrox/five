//
// Filename:	texLod.cpp
// Description:	handles streaming & swapping of top mip out of texture dicts
// Written by:	JohnW
//
//	5/10/2010	-	JohnW:	- initial;
//
//
//

#include "scene/texLod.h"

#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG
#elif __PS3
#include "grcore/gputimer.h"
#include "grcore/im.h"
#include "grcore/texturegcm.h"
#include "grcore/wrapper_gcm.h"
#include "vector/colors.h"
#elif __WIN32PC
#include "grcore/texturepc.h"
#endif

#if RSG_DURANGO || RSG_ORBIS
#include "grcore/orbisdurangoresourcebase.h"
#endif // RSG_DURANGO || RSG_ORBIS

// Rage:
#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/slider.h"
#include "bank/text.h"
#include "grcore/debugdraw.h"
#include "grcore/setup.h"
#endif //__BANK
#include "grcore/stateblock.h"
#include "grcore/texture.h"
#include "system/virtualallocator.h"


//fw
#include "fwscene/search/Search.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/drawablestore.h"

// Game headers:
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/Replay.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/Ped.h"
#include "peds/PlayerPed.h"
#include "peds/rendering/pedvariationdebug.h"
#include "renderer/Renderer.h"
#include "scene/Entity.h"
#include "scene/lod/LodScale.h"
#include "scene/world/GameWorld.h"
#include "scene/scene_channel.h"
#include "streamer/SceneStreamerMgr.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "world/GameWorld.h"

// debug
#include "debug/UiGadget/UiGadgetWindow.h"
#include "debug/UiGadget/UiGadgetList.h"
#include "debug/UiGadget/UiGadgetText.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerSearch.h" // for CTxdRef, GetAssociatedTxds_ModelInfo
#include "debug/GtaPicker.h"
#include "vehicles/vehicle.h"
#include "vehicles/trailer.h"
#include "Network/NetworkInterface.h"
#include "scene/FocusEntity.h"

#if __BANK
#include "modelinfo/VehicleModelInfo.h"
#include "String.h"

fwTxd*			CTexLod::m_pHighTxd = NULL;
fwTxd*			CTexLod::m_pHigh2Txd = NULL;
fwTxd*			CTexLod::m_pLowTxd = NULL;
grcTexture*		CTexLod::m_TexTab[4]={NULL};
u32             CTexLod::m_totalMemoryUsed = 0;
bool			CTexLod::m_showHDTextureMemUsage = false;
bool            CTexLod::m_showHDMemUsage = false;
bool            CTexLod::m_onlyExternalRequsts = false;

bool bListHDTxds = false;
bool bColourize = false;

bool g_renderHdArea;

CUiGadgetWindow*	CTexLod::ms_pDebugWindow = NULL;
CUiGadgetList*		CTexLod::ms_pHDTxdList = NULL;
CUiGadgetList*		CTexLod::ms_pTexList = NULL;
fwAssetLocation		CTexLod::ms_currentAsset(STORE_ASSET_INVALID, 0);
strLocalIndex		CTexLod::ms_debugTxdIdx = strLocalIndex(-1);
strLocalIndex		CTexLod::ms_debugTriggerMI = strLocalIndex(fwModelId::MI_INVALID);

u32 assetHashSlotArray[64];
s32 targetTxdListIndex = -1;

// HD tex picker stuff
CEntity*			CTexLod::ms_pSelectedEntity = NULL;
float				CTexLod::ms_selectedEntityDist = 0.0f;
bool				CTexLod::ms_bIsActivated = false;
bool				CTexLod::ms_bDisableHDTex = false;
float				CTexLod::ms_HDActivationDistance = 1.0f;

#endif //__BANK

atMap<s32, CTxdPairBinding*>										CTexLod::ms_boundTxds;
atMap<fwAssetLocation, s32>											CTexLod::ms_assetToHDMappings;
atFixedArray<CExternalRequest, MAX_EXTERNAL_HD_TXD_REQUESTS>		CTexLod::ms_externalRequests;
#if NG_HD_SPLIT
atArray<CTexLod::sVirtMemData>										CTexLod::ms_virtMemToFree;
#endif // NG_HD_SPLIT

bool			CTexLod::ms_bEnabled = true;
bool			CTexLod::ms_bEnableSwapping = true;
bool			CTexLod::ms_bAmbientRequestsDisabled = false;
bool			CTexLod::ms_bHasHdArea = false;
spdSphere		CTexLod::ms_hdArea;
u32				CTexLod::ms_nextHdAreaUpdate = 0;
bool			CTexLod::ms_allowAmbientRequests = true;

// HDAssetManager stuff ---

CHDAssetManager		g_HDAssetMgr;

void CHDAssetManager::Init(void)
{
	m_cutsceneRequests.Reset();

	m_enableDefaultHDRequests = true;

#if __BANK
	m_displayHDRequests = false;
#endif //__BANK
}

void CHDAssetManager::Shutdown(void)
{
	m_cutsceneRequests.Reset();
}

#if __BANK
void CHDAssetManager::DebugUpdate(void)
{
	if (!m_displayHDRequests){
		return;
	}

	grcDebugDraw::AddDebugOutput("");
	grcDebugDraw::AddDebugOutput("Current cutscene HD requests");
	grcDebugDraw::AddDebugOutput("--------------------------------");

	u8 modelType = 0;

	for(u32 i=0; i<3; i++)
	{
		switch(i){
		case 0:
			modelType = MI_TYPE_PED;
			grcDebugDraw::AddDebugOutput(" Peds:");
			break;
		case 1:
			modelType = MI_TYPE_VEHICLE;
			grcDebugDraw::AddDebugOutput(" Vehicles:");
			break;
		case 2:
			modelType = MI_TYPE_WEAPON;
			grcDebugDraw::AddDebugOutput(" Weapons:");
			break;
		}

		for(u32 j=0; j<m_cutsceneRequests.GetCount(); j++)
		{
			CBaseModelInfo* pBMI = m_cutsceneRequests[j];

			if (pBMI->GetModelType() == modelType)
			{
				switch(modelType)
				{
				case MI_TYPE_VEHICLE:
					{
						CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pBMI);
						grcDebugDraw::AddDebugOutput("  (%c) : %s",pVMI->GetAreHDFilesLoaded() ? '*' : ' ', pBMI->GetModelName());
						break;
					}
				case MI_TYPE_PED:
					{
						CPedModelInfo* pPMI = static_cast<CPedModelInfo*>(pBMI);
						grcDebugDraw::AddDebugOutput("  (%c) : %s",pPMI->GetAreHDFilesLoaded() ? '*' : ' ', pBMI->GetModelName());
						break;
					}
				case MI_TYPE_WEAPON:
					{
						CWeaponModelInfo* pWMI = static_cast<CWeaponModelInfo*>(pBMI);
						grcDebugDraw::AddDebugOutput("  (%c) : %s",pWMI->GetAreHDFilesLoaded() ? '*' : ' ', pBMI->GetModelName());
						break;
					}
				}
			}
		}
	}
	grcDebugDraw::AddDebugOutput("---");
}
#endif //__BANK

void CHDAssetManager::AddCutsceneRequestForHD(atHashString nameHash)
{
	if (!m_enableDefaultHDRequests)
	{
		return;
	}

	fwArchetype* pArch = fwArchetypeManager::GetArchetypeFromHashKey(nameHash, NULL);
	if (pArch)
	{
		CBaseModelInfo* pBMI = static_cast<CBaseModelInfo*>(pArch);
		if (pBMI->GetModelType() == MI_TYPE_WEAPON || pBMI->GetModelType() == MI_TYPE_VEHICLE || pBMI->GetModelType() == MI_TYPE_PED)
		{	
			if (m_cutsceneRequests.Find(pBMI) == -1)
			{
				m_cutsceneRequests.PushAndGrow(pBMI);

				switch(pBMI->GetModelType())
				{
				case MI_TYPE_VEHICLE:
					{
						CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pBMI);
						pVMI->AddToHDInstanceList((size_t)&g_HDAssetMgr);
						break;
					}
				case MI_TYPE_PED:
					{
						break;
					}
				case MI_TYPE_WEAPON:
					{
						CWeaponModelInfo* pWMI = static_cast<CWeaponModelInfo*>(pBMI);
						pWMI->ConfirmHDFiles();
						pWMI->AddToHDInstanceList((size_t)&g_HDAssetMgr);
						break;
					}
				}
			}
		}
	}
}

void CHDAssetManager::RemoveCutsceneRequestForHD(atHashString UNUSED_PARAM(nameHash))
{
	if (!m_enableDefaultHDRequests)
	{
		return;
	}

// 	fwArchetype* pArch = fwArchetypeManager::GetArchetypeFromHashKey(nameHash, NULL);
// 	if (pArch)
// 	{
// 		CBaseModelInfo* pBMI = static_cast<CBaseModelInfo*>(pArch);
// 		u8 modelType = pBMI->GetModelType();
// 
// 		switch(modelType)
// 		{
// 		case MI_TYPE_VEHICLE:
// 			{
// 				break;
// 			}
// 		case MI_TYPE_PED:
// 			{
// 				break;
// 			}
// 		case MI_TYPE_WEAPON:
// 			{
// 				break;
// 			}
// 		}

// 		if (pBMI->GetModelType() == MI_TYPE_WEAPON || pBMI->GetModelType() == MI_TYPE_VEHICLE || pBMI->GetModelType() == MI_TYPE_PED)
// 		{	
// 			m_cutsceneRequests.DeleteMatches(pBMI);
// 		}
//	}
}

bool CHDAssetManager::AreCutsceneRequestsLoaded(void)
{

	Assert(false);

// 	u32 numReq = m_cutsceneRequests.GetCount();
// 
// 	for(u32 i=0; i<numReq; i++)
// 	{
// 		CEntity* pEntity = m_cutsceneRequests[i];
// 		if (pEntity && !pEntity->GetIsCurrentlyHD())
// 		{
// 			return(false);
// 		}
// 	}
	
	return(true);
}

void CHDAssetManager::FlushCutsceneRequestsHD(void)
{
	for(u32 i = 0; i<m_cutsceneRequests.GetCount(); i++)
	{
		CBaseModelInfo* pBMI = m_cutsceneRequests[i];
		u8 modelType = pBMI->GetModelType();

		switch(modelType)
		{
		case MI_TYPE_VEHICLE:
			{
				CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pBMI);
				pVMI->RemoveFromHDInstanceList((size_t)&g_HDAssetMgr);
				break;
			}
		case MI_TYPE_WEAPON:
			{
				CWeaponModelInfo* pVMI = static_cast<CWeaponModelInfo*>(pBMI);
				pVMI->RemoveFromHDInstanceList((size_t)&g_HDAssetMgr);
				break;
			}
		}
	}

	m_cutsceneRequests.Reset();
}


// --- Class CMipSwitcher --- store data when switching one texture to another so that switching can be undone later.
void CMipSwitcher::SetSwapState(bool bSwap NG_HD_SPLIT_ONLY(, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset)){

	// skip if already in the desired state
	if (bSwap == m_bIsSwapped){
		return;
	}

#if __PS3
	CellGcmTexture* pTexLow = m_pLow->GetTexturePtr();
	CellGcmTexture* pTexHigh = m_pHigh->GetTexturePtr();

	if (pTexLow && pTexHigh){

		if (bSwap){
			m_mipMap = pTexLow->mipmap;
			m_offset = pTexLow->offset;
			m_width = pTexLow->width;
			m_height = pTexLow->height;

			pTexLow->mipmap = pTexHigh->mipmap;
			pTexLow->offset = pTexHigh->offset;
			pTexLow->width = pTexHigh->width;
			pTexLow->height = pTexHigh->height;
		} else {
			pTexLow->mipmap = m_mipMap;
			pTexLow->offset = m_offset;
			pTexLow->width = m_width;
			pTexLow->height = m_height;
		}

		m_bIsSwapped = !m_bIsSwapped;

#if USE_PACKED_GCMTEX
		m_pLow->UpdatePackedTexture();
		m_pHigh->UpdatePackedTexture();
#endif
	}
#elif __XENON
	grcTextureObject* pTexLow = m_pLow->GetTexturePtr();
	grcTextureObject* pTexHigh = m_pHigh->GetTexturePtr();

	if (pTexLow && pTexHigh){

		if (bSwap){
			// back up texture data	
			m_mipMap    = pTexLow->Format.MaxMipLevel << 4 | pTexLow->Format.MinMipLevel;
			m_offset    = pTexLow->Format.BaseAddress << 9 | pTexLow->Format.Pitch;
			m_mipOffset = pTexLow->Format.MipAddress;

			m_width  = pTexLow->Format.Size.TwoD.Width;
			m_height = pTexLow->Format.Size.TwoD.Height;

			if (pTexHigh->Format.MaxMipLevel == 0) {
				// Set Tex1 to use high res info
				pTexLow->Format.MaxMipLevel += 1;

				pTexLow->Format.MipAddress  = pTexLow->Format.BaseAddress;
				pTexLow->Format.BaseAddress = pTexHigh->Format.BaseAddress;
			} else {
				// OLD CODEPATH FOR OLD DATA
				pTexLow->Format.MaxMipLevel = pTexHigh->Format.MaxMipLevel;
				pTexLow->Format.MinMipLevel = pTexHigh->Format.MinMipLevel;

				pTexLow->Format.BaseAddress = pTexHigh->Format.BaseAddress;
				pTexLow->Format.MipAddress  = pTexHigh->Format.MipAddress;
			}

			pTexLow->Format.Size.TwoD.Width = pTexHigh->Format.Size.TwoD.Width;
			pTexLow->Format.Size.TwoD.Height = pTexHigh->Format.Size.TwoD.Height;

			pTexLow->Format.Pitch = pTexHigh->Format.Pitch;

			m_pLow->GetGcmTexture().width = static_cast<uint16_t>(pTexHigh->Format.Size.TwoD.Width + 1);
			m_pLow->GetGcmTexture().height = static_cast<uint16_t>(pTexHigh->Format.Size.TwoD.Height + 1);
		} else {
			// Restore tex1
			pTexLow->Format.MaxMipLevel = m_mipMap >> 4;
			pTexLow->Format.MinMipLevel = m_mipMap & 0xF;

			pTexLow->Format.BaseAddress = m_offset >> 9;
			pTexLow->Format.MipAddress  = m_mipOffset;

			pTexLow->Format.Size.TwoD.Width = m_width;
			pTexLow->Format.Size.TwoD.Height = m_height;

			pTexLow->Format.Pitch = m_offset & 0x1FF;

 			m_pLow->GetGcmTexture().width = static_cast<uint16_t>(m_width + 1);
 			m_pLow->GetGcmTexture().height = static_cast<uint16_t>(m_height + 1);
		}

		Assert(pTexLow->Format.BaseAddress != pTexLow->Format.MipAddress);

		m_bIsSwapped = !m_bIsSwapped;
	}
#elif __WIN32PC
	grcTexturePC* pTexLow = (grcTexturePC*) m_pLow.ptr;
	grcTexturePC* pTexHigh = (grcTexturePC*) m_pHigh.ptr;

	if (pTexLow->GetTexturePtr() && pTexHigh->GetTexturePtr()){
		pTexLow->HDOverrideSwap(pTexHigh);

		m_bIsSwapped = !m_bIsSwapped;
	}
#elif (RSG_DURANGO || RSG_ORBIS) && NG_HD_SPLIT
	grcOrbisDurangoTextureBase *pTexLow = (grcOrbisDurangoTextureBase *)m_pLow.ptr;
	grcOrbisDurangoTextureBase *pTexHigh = (grcOrbisDurangoTextureBase *)m_pHigh.ptr;

	if (pTexLow && pTexHigh) {
		void* pDeferredFreePtr = NULL;
		size_t deferredFreeSize = 0;
		m_pOldHdAddr = grcOrbisDurangoTextureBase::PeformHDOverrideSwap(pTexLow, pTexHigh, m_pOldHdAddr, pDeferredFreePtr, deferredFreeSize);
		m_bIsSwapped = !m_bIsSwapped;

		CTexLod::AddDeferredFreePtr(pDeferredFreePtr, deferredFreeSize, hdTxdIdx, targetAsset);
	}
#endif //#elif __XENON #elif __WIN32PC #elif (RSG_DURANGO || RSG_ORBIS)
}

void CMipSwitcher::Set(grcTexture* pLow, grcTexture* pHigh NG_HD_SPLIT_ONLY(, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset)){

	Assert(pLow && pHigh);

	if (pLow && pHigh)
	{
		m_pLow = pLow;
		m_pHigh = pHigh;

#if NG_HD_SPLIT
		m_pOldHdAddr = NULL;
#endif

		SetSwapState(true NG_HD_SPLIT_ONLY(, hdTxdIdx, targetAsset));
	}
}

void CMipSwitcher::Clear(NG_HD_SPLIT_ONLY(strLocalIndex hdTxdIdx, fwAssetLocation targetAsset)){

	Assert( !m_bIsSwapped || ( m_pLow && m_pHigh ) );

	if (m_pLow && m_pHigh)
	{
		SetSwapState(false NG_HD_SPLIT_ONLY(, hdTxdIdx, targetAsset));

		m_pLow = NULL;
		m_pHigh = NULL;
	}
}

// --- class CTxdPairBinding --- contains all info for switching textures in one txd with textures specified in another so switch can be undone.

// enforce the swap state of all the textures for this binding
void CTxdPairBinding::SetSwapStateAll(bool bSwap)
{
	u32 numTex =m_boundTexSwitchers.GetCount();
	while(numTex-- > 0){
		if (m_boundTexSwitchers[numTex].IsValid()){
			m_boundTexSwitchers[numTex].SetSwapState(bSwap NG_HD_SPLIT_ONLY(, m_highTxdIdx, m_targetAsset));
		}
	}	
}

#if __DEV
static int g_MaxTargetTxds = 0;
#endif // __DEV

void CTxdPairBinding::SetHighTxdIdx(s32 highTxdIdx)
{
	if (g_TxdStore.GetIsBoundHD(strLocalIndex(highTxdIdx)))
	{
		Assertf(false,"txd slot=%d (%s) already bound - cannot be bound again", highTxdIdx, g_TxdStore.GetName(strLocalIndex(highTxdIdx)));
		return;
	}

	m_highTxdIdx = highTxdIdx;
}

// switch all the textures in the target asset with the textures with the same name in high Txd
void CTxdPairBinding::ActivateSwap(fwAssetLocation target){

	if (m_highTxdIdx == -1)
	{
		sceneAssert(false);
		return;
	}

#if __BANK
	char buffer[255];
	const char* name = strStreamingEngine::GetInfo().GetObjectPath(target.GetStreamingIndex(), buffer, sizeof(buffer));
	DIAG_CONTEXT_MESSAGE(name);
#endif


	sceneAssert(g_TxdStore.HasObjectLoaded(m_highTxdIdx));

	fwTxd* pHighTxd = g_TxdStore.Get(m_highTxdIdx);
	sceneAssert(pHighTxd);
	s32 numHighTex = pHighTxd->GetCount();

	if (!Verifyf(numHighTex > 0, "%d entries in HD txd '%s'!", numHighTex, g_TxdStore.GetName(m_highTxdIdx)))
		return;

	if (!Verifyf(target.IsValid(), "target is invalid!"))
		return;

	if (!target.HasLoaded())
	{
		Assertf(false, "target asset type=%s slot=%d (%s) not loaded - cannot be bound", target.GetStoreTypeStr(), target.GetStoreSlot().Get(), target.GetName()); 
		return;
	}

	if (target.GetIsBoundHD())
	{
		Assertf(false,"target asset type=%s slot=%d (%s) already bound - cannot be bound again", target.GetStoreTypeStr(), target.GetStoreSlot().Get(), target.GetName());
		return;
	}

	g_TxdStore.AddRef(m_highTxdIdx, REF_RENDER);
	g_TxdStore.SetIsBoundHD(strLocalIndex(m_highTxdIdx), true);
#if !__FINAL
	g_TxdStore.Validate(m_highTxdIdx);
#endif // #if !__FINAL

	// get the target Txd(s) from the target asset
	fwPtrListSingleLink	targetTxdList;
	target.GetTxdList(targetTxdList);

#if __DEV
	const int numUsed = targetTxdList.CountElements();
	if (g_MaxTargetTxds < numUsed) {
		g_MaxTargetTxds = numUsed;
		Displayf("g_MaxTargetTxds = %d", g_MaxTargetTxds);
	}
#endif // __DEV

	strStreamingModule *pModule = target.GetStreamingModule();

	if (pModule)
	{
		pModule->AddRef(target.GetStoreSlot(), REF_RENDER);
		target.SetIsBoundHD(true);
#if !__FINAL
		target.Validate();
#endif // !__FINAL		
	}

	// replace textures of the same name in the target txds with the textures in the high txd
	Assertf(!m_targetAsset.IsValid(), "target asset type=%s slot=%d (%s) is being overridden with asset type=%s slot=%d (%s)", m_targetAsset.GetStoreTypeStr(), m_targetAsset.GetStoreSlot().Get(), m_targetAsset.GetName(), target.GetStoreTypeStr(), target.GetStoreSlot().Get(), target.GetName());
	m_targetAsset = target;

    s32 numTargetTxds = targetTxdList.CountElements(); 

	m_boundTexSwitchers.Reserve(numHighTex * numTargetTxds);
	m_boundTexSwitchers.Resize(numHighTex * numTargetTxds);

    u32 currentTarget = 0;
	fwPtrNodeSingleLink* pLowTxdEntry = targetTxdList.GetHeadPtr();
	while (pLowTxdEntry != NULL)
    {
		fwTxd* pLowTxd = (fwTxd*)pLowTxdEntry->GetPtr();
		Assert(pLowTxd);

        for (s32 i = 0; i < numHighTex; ++i)
        {
			u32 highTexHash = pHighTxd->GetCode(i);
			grcTexture* pLowTex = pLowTxd->LookupLocal(highTexHash);
			if (pLowTex){
				grcTexture* pHighTex = pHighTxd->LookupLocal(highTexHash);
				Assert(pHighTex);
				m_boundTexSwitchers[i + currentTarget * numHighTex].Set(pLowTex, pHighTex NG_HD_SPLIT_ONLY(, m_highTxdIdx, m_targetAsset));
			}
        }
		pLowTxdEntry = (fwPtrNodeSingleLink*)pLowTxdEntry->GetNextPtr();
        currentTarget++;
	}

	targetTxdList.Flush();

	ResetCooldownTimer();
}

// clear the binding between asset and high Txd & dec ref counts accordingly
CTxdPairBinding::~CTxdPairBinding(){

	u32 numTex =m_boundTexSwitchers.GetCount();
	while(numTex-- > 0){
		m_boundTexSwitchers[numTex].Clear(NG_HD_SPLIT_ONLY(m_highTxdIdx, m_targetAsset));
	}

	if (m_highTxdIdx != -1)
	{
		g_TxdStore.ClearRequiredFlag(m_highTxdIdx.Get(), STRFLAG_DONTDELETE);

		if (g_TxdStore.HasObjectLoaded(m_highTxdIdx))
		{
	#if !__FINAL
			g_TxdStore.Validate(m_highTxdIdx);
	#endif // !__FINAL	
			if (g_TxdStore.GetIsBoundHD(strLocalIndex(m_highTxdIdx)))
			{
				gDrawListMgr->AddRefCountedModuleIndex(m_highTxdIdx.Get(), &g_TxdStore);
			}
			g_TxdStore.SetIsBoundHD(strLocalIndex(m_highTxdIdx), false);
		}
	}

	if (m_targetAsset.IsValid()){
		strStreamingModule *pModule = m_targetAsset.GetStreamingModule();
		strLocalIndex objIdx = m_targetAsset.GetLocalIndex();
		gDrawListMgr->AddRefCountedModuleIndex(objIdx.Get(), pModule);
		m_targetAsset.SetIsBoundHD(false);
		m_targetAsset.Validate();
	}
}

void CTxdPairBinding::Update(void) { 
	u32 curStep = fwTimer::GetTimeStepInMilliseconds();
	//if we are doing less than 30 fps then 
	//cool down as though we were doing 30 fps
	if (curStep > 33) 
	{
		m_cooldownTimer -= 33;
	}
	else
	{
		m_cooldownTimer -= curStep;
	}
}

u32 CTxdPairBinding::GetTextureMemUsage()
{
	u32 totalMemory = 0;
	for (int i = 0; i < m_boundTexSwitchers.size(); ++i)
	{
		CMipSwitcher *pMipSwitchter =  &m_boundTexSwitchers[i];
		if (pMipSwitchter->GetHighTex())
		{
			totalMemory += pMipSwitchter->GetHighTex()->GetPhysicalSize();
		}
	}

	return totalMemory;
}


// CTexLodInterface
void CTexLodInterface::SetSwapStateSingle(bool bSwap, fwAssetLocation targetAsset)
{
	CTexLod::SetSwapStateSingle(bSwap, targetAsset);
}

// --- class CTexLod : manages mip texture swapping for the game

//
//
//
//
grcTexture* CTexLod::LoadTexture(fwTxd* pTxd, char *texname)
{
	grcTexture* pTexture = NULL;

	if (pTxd){
		pTexture = pTxd->Lookup(texname);
		Assertf(pTexture, "Cannot find CTexLod debug texture!");
		pTexture->AddRef();
	}

	return(pTexture);
}

//
//
//
//

inline float GetClampedScaleValue() 
{
	return(MAX(1.0f, g_LodScale.GetGlobalScale()));
}

void CTexLod::Init()
{
	ms_boundTxds.Reset();
	ms_assetToHDMappings.Reset();
	ms_externalRequests.Reset();

	g_HDAssetMgr.Init();

	fwTexLod::InitClass(rage_new CTexLodInterface());
}

void CTexLod::InitSession(unsigned UNUSED_PARAM(initMode))
{
	ms_bHasHdArea = false;
	ms_nextHdAreaUpdate = 0;
}

//
void CTexLod::ShutdownSession(u32 /*shutdownMode*/)
{
	FlushAllUpgrades();

#if NG_HD_SPLIT
	for (s32 i = 0; i < ms_virtMemToFree.GetCount(); ++i)
		ms_virtMemToFree[i].Release();

	ms_virtMemToFree.Reset();
#endif // NG_HD_SPLIT

	if (CStreaming::GetReloadPackfilesOnRestart())
	{
		ms_assetToHDMappings.Kill();
	}
}
//
//
//
void CTexLod::Shutdown()
{
	ms_assetToHDMappings.Kill();
	g_HDAssetMgr.Shutdown();

	ms_boundTxds.Reset();
	ms_externalRequests.Reset();

	fwTexLod::ShutdownClass();
}

bool CTexLod::AreFromSameArchive(fwAssetLocation targetAsset, s32* pTxdSlotHigh)
{
	if (pTxdSlotHigh)
	{
		int targetArchive = targetAsset.GetStreamingInfo()->GetHandle();
		int highTxdArchive = g_TxdStore.GetStreamingInfo(strLocalIndex(*pTxdSlotHigh))->GetHandle();

		if (fiCollection::GetCollectionIndex(targetArchive) == fiCollection::GetCollectionIndex(highTxdArchive))
		{
			return(true);
		}

#if !__FINAL

		Warningf("%s and %s may not texLod bind across archives!", targetAsset.GetName(), g_TxdStore.GetName(*pTxdSlotHigh));

		const strStreamingFile* targetStreamingFile = strPackfileManager::GetImageFileFromHandle(targetArchive);
		const strStreamingFile* highTxdStreamingFile = strPackfileManager::GetImageFileFromHandle(highTxdArchive);

		const char* pTargetArchiveName = targetStreamingFile ? (targetStreamingFile->m_name.GetCStr()) : "";
		const char* pHighTxdArchiveName =  highTxdStreamingFile ? (highTxdStreamingFile->m_name.GetCStr()) : "";

		Warningf("Attempt to bind between assets in [%s] and [%s]", pTargetArchiveName, pHighTxdArchiveName);
		Assertf(false,"Binding across archive detected");		// check warnings!

#endif //!__FINAL

	}

	return(false);
}

/// handle upgrading to HD txds through a modelinfo (start issuing streaming requests & storing refs to streamed txds)
//void CTexLod::TriggerHDTxdUpgrade(u32 triggeringMI){
void CTexLod::TriggerHDTxdUpgrade(fwAssetLocation targetAsset, u32 triggeringMI, bool onlyIfLoaded)
{
	Assert(triggeringMI != fwModelId::MI_INVALID);
#if __DEV
	fwAssetLocation debugAsset = targetAsset;
#endif
	// iterate over all txds in chain for this modelinfo
	while(targetAsset.IsValid()){
#if __DEV
		if(!targetAsset.PreValidate())
		{
			Assertf(false, "PreValidate failed, we may crash later on. url:bugstar:2730194");
			CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(triggeringMI)));
			Displayf("Prevalidate Failed (%s:%s), printing asset tree:", pMI->GetModelName() ? pMI->GetModelName() : "", targetAsset.GetName());
			//Something has gone wrong...printing the asset tree
			while(debugAsset.IsValid()){
				Displayf("%s",debugAsset.GetName());
				debugAsset = debugAsset.GetParentAsset();
			}
		}
#endif
		//targeted fix for url:bugstar:2730194, don't know the reason for the asset 
		//being streamed out but this should stop the crash
		if (!targetAsset.PreValidateTexLod() && (CReplayMgr::IsExporting() || CReplayMgr::IsScrubbing()))
		{
			targetAsset = targetAsset.GetParentAsset();
			continue;
		}
		//Displayf("target:%s",targetAsset.GetName());

		s32 *pTxdSlotHigh = ms_assetToHDMappings.Access(targetAsset);

		// the target and the high txd must be from the same archive!
		bool bFromSameArchive = AreFromSameArchive(targetAsset, pTxdSlotHigh);

		if ((pTxdSlotHigh!=NULL) && bFromSameArchive)
		{
			strLocalIndex txdSlotHigh = strLocalIndex(*pTxdSlotHigh);
			//don't load new hd textures if told not to, 
			//only let already resident ones tick over
			if(onlyIfLoaded && !g_TxdStore.HasObjectLoaded(txdSlotHigh))
			{
				targetAsset = targetAsset.GetParentAsset();
				continue;
			}
			// get the current binding for this txd, or create a new one (and add to our map)
			CTxdPairBinding** ppBoundTxd = ms_boundTxds.Access(targetAsset);
			CTxdPairBinding* pBoundTxd = NULL;
			if (!ppBoundTxd){
				pBoundTxd = rage_new(CTxdPairBinding);
				Assert(pBoundTxd);
				ms_boundTxds.Insert(targetAsset, pBoundTxd);
			} else {
				pBoundTxd = *ppBoundTxd;
			}
			Assert(pBoundTxd);

			pBoundTxd->ResetCooldownTimer();					// make sure this stays alive now
			pBoundTxd->SetTriggeringModelIndex(triggeringMI);	// useful info - keep track of which type is keeping this entry active

			if (!pBoundTxd->IsAlreadyBound(targetAsset)){
				CStreaming::RequestObject(txdSlotHigh, g_TxdStore.GetStreamingModuleId(),  STRFLAG_DONTDELETE);
				pBoundTxd->SetHighTxdIdx(txdSlotHigh.Get());
				if (g_TxdStore.HasObjectLoaded(txdSlotHigh)){
					CStreaming::ClearRequiredFlag(txdSlotHigh, g_TxdStore.GetStreamingModuleId(), STRFLAG_DONTDELETE);
					pBoundTxd->ActivateSwap(targetAsset);
				}
			}
		}
		targetAsset = targetAsset.GetParentAsset();
	}
}

// 20 kph
#define HD_UPGRADE_SPEED_LIMIT ((25.0f *( 1000.0f)) / (60.0f * 60.0f)) 

// search the world looking for models which can trigger an HD txd upgrade, keep all nearby HD txds alive, any which have timed out then clean up.
void CTexLod::Update(){
	UpdateInternal();
	
#if GTA_REPLAY && REPLAY_FORCE_LOAD_SCENE_DURING_EXPORT
	if (CReplayMgr::IsExporting())
	{
		CStreaming::LoadAllRequestedObjects();
		UpdateInternal();
	}
#endif	//GTA_REPLAY
}
void CTexLod::UpdateInternal(){
	
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::TEXLOD);

#if __BANK
	DebugUpdate();
#endif // __BANK

	if (!ms_bEnableSwapping)
	{
		return;
	}

	fwPtrListSingleLink	entityList;	
	Vector3 velocity = CFocusEntityMgr::GetMgr().GetVel();
	ms_allowAmbientRequests = true;
	if (ms_bEnabled)
	{
		const float capSpeedSquard = (HD_UPGRADE_SPEED_LIMIT * HD_UPGRADE_SPEED_LIMIT);
		// Block all but the external the HD tex requests if the current scene 
		// isn't in a good state to be taking on additional requests
		CStreamingBucketSet &neededBucketSet = g_SceneStreamerMgr.GetStreamingLists().GetNeededEntityList().GetBucketSet();
		BucketEntry::BucketEntryArray &neededList = neededBucketSet.GetBucketEntries();
		neededBucketSet.WaitForSorting();
		int neededListCount = neededList.GetCount();
		bool streamerNotReady = (neededListCount > 0 && neededList[neededListCount-1].m_Score >= STREAMBUCKET_VISIBLE_FAR);		

		if (   velocity.Mag2() > capSpeedSquard 
			|| streamerNotReady
			|| ms_bAmbientRequestsDisabled
			)
		{
			ms_allowAmbientRequests = false;
		}

		//script must enable this every frame		
		ms_bAmbientRequestsDisabled = false;		

		BANK_ONLY(if (!m_onlyExternalRequsts))
		{
            // use renderlists for HD upgrade - not a world search
            int renderListIdx = g_SceneToGBufferPhaseNew->GetEntityListIndex();
            fwRenderListDesc& GBufRenderListDesc = gRenderListGroup.GetRenderListForPhase(renderListIdx);

            u32 size = GBufRenderListDesc.GetNumberOfEntities(RPASS_VISIBLE);
            for(u32 i=0; i<size; i++)
            {
    //			CEntity* pDebugEntity = static_cast<CEntity*>(GBufRenderListDesc.GetEntity(i, RPASS_VISIBLE));
    //			if (strcmpi(pDebugEntity->GetBaseModelInfo()->GetModelName(), "v_46_mainshell")==0)
    //			{
    //				Displayf("shell");
    //			}

                u16 customFlags = GBufRenderListDesc.GetEntityCustomFlags(i, RPASS_VISIBLE);
                if (customFlags & fwEntityList::CUSTOM_FLAGS_HD_TEX_CAPABLE)
                {
                    float minDistance = 0.0f;
                    float distance = GBufRenderListDesc.GetEntitySortVal(i, RPASS_VISIBLE);
                    u32 baseFlags = GBufRenderListDesc.GetEntityBaseFlags(i, RPASS_VISIBLE);

                    CEntity* pEntity = static_cast<CEntity*>(GBufRenderListDesc.GetEntity(i, RPASS_VISIBLE));
                    Debug_RegisterEntityDistance(pEntity, distance);

                    if (baseFlags & fwEntity::HAS_HD_TEX_DIST) 
                    {
                        Assertf(pEntity && pEntity->GetIsTypePed(), "Only Peds should have fwEntity::HAS_HD_TEX_DIST flag set");
                        minDistance = static_cast<CPed*>(pEntity)->GetPedModelInfo()->GetHDDist();
                    }
                    else
                    {
                        minDistance = pEntity->GetBaseModelInfo()->GetHDTextureDistance();
                    }

                    bool requireHighModel = distance < minDistance * GetClampedScaleValue();
    #if __BANK
                    if (pEntity && pEntity->GetIsTypePed())
                    {
                        if (CPedVariationDebug::GetForceLdAssets())
                            requireHighModel = false;
                        if (CPedVariationDebug::GetForceHdAssets())
                            requireHighModel = true;
                    }
    #endif // __BANK
                    if (requireHighModel)
                    {
                        CBaseModelInfo* pBMI = pEntity->GetBaseModelInfo();
                        Assert(pBMI);
                        if (pBMI->GetHasLoaded())
                        {
							CTexLod::TriggerHDTxdUpgrade(pBMI->GetAssetLocation(), pEntity->GetModelIndex(), !ms_allowAmbientRequests);
							Debug_RegisterEntityActivations(pEntity);
                        }
                    }
                }
            }

            size = GBufRenderListDesc.GetNumberOfEntities(RPASS_TREE);
            for(u32 i=0; i<size; i++)
            {
                u16 customFlags = GBufRenderListDesc.GetEntityCustomFlags(i, RPASS_TREE);
                if (customFlags & fwEntityList::CUSTOM_FLAGS_HD_TEX_CAPABLE)
                {
                    float distance = GBufRenderListDesc.GetEntitySortVal(i, RPASS_TREE);
                    CEntity* pEntity = static_cast<CEntity*>(GBufRenderListDesc.GetEntity(i, RPASS_TREE));
                    Debug_RegisterEntityDistance(pEntity, distance);

                    if (distance < pEntity->GetBaseModelInfo()->GetHDTextureDistance() * GetClampedScaleValue())
                    {
                        CBaseModelInfo* pBMI = pEntity->GetBaseModelInfo();
                        Assert(pBMI);
                        if (pBMI->GetHasLoaded())
                        {
							CTexLod::TriggerHDTxdUpgrade(pBMI->GetAssetLocation(), pEntity->GetModelIndex(), !ms_allowAmbientRequests);
							Debug_RegisterEntityActivations(pEntity);
                        }
                    }
                }
            }

            size = GBufRenderListDesc.GetNumberOfEntities(RPASS_DECAL);
            for(u32 i=0; i<size; i++)
            {
                u16 customFlags = GBufRenderListDesc.GetEntityCustomFlags(i, RPASS_DECAL);
                if (customFlags & fwEntityList::CUSTOM_FLAGS_HD_TEX_CAPABLE)
                {
                    float distance = GBufRenderListDesc.GetEntitySortVal(i, RPASS_DECAL);
                    CEntity* pEntity = static_cast<CEntity*>(GBufRenderListDesc.GetEntity(i, RPASS_DECAL));
                    Debug_RegisterEntityDistance(pEntity, distance);

                    if (distance < pEntity->GetBaseModelInfo()->GetHDTextureDistance() * GetClampedScaleValue())
                    {
                        CBaseModelInfo* pBMI = pEntity->GetBaseModelInfo();
                        Assert(pBMI);
                        if (pBMI->GetHasLoaded())
                        {
							CTexLod::TriggerHDTxdUpgrade(pBMI->GetAssetLocation(), pEntity->GetModelIndex(), !ms_allowAmbientRequests);
							Debug_RegisterEntityActivations(pEntity);
                        }
                    }
                }
            }

            // process cutscene peds HD .txd upgrades
            for(u32 i=0; i<g_HDAssetMgr.GetCutsceneRequests().GetCount(); i++)
            {
                CBaseModelInfo* pBMI = g_HDAssetMgr.GetCutsceneRequests()[i];

                if (pBMI && (pBMI->GetModelType()==MI_TYPE_PED) && pBMI->GetHasLoaded())
                {
                    CTexLod::TriggerHDTxdUpgrade(pBMI->GetAssetLocation(), pBMI->GetStreamSlot().Get(), !ms_allowAmbientRequests);
                }
            }

			// process hd area
			u32 curTime = fwTimer::GetTimeInMilliseconds();
			if (ms_bHasHdArea && curTime >= ms_nextHdAreaUpdate)
			{
				fwIsSphereIntersecting sphere(ms_hdArea);
				u32 searchType = ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_OBJECT;
				CGameWorld::ForAllEntitiesIntersecting(&sphere, TriggerHdForEntity, NULL, searchType, SEARCH_LOCATION_EXTERIORS);

				// the trigger will stay for 60 frames, so we do the world search every 1.5 seconds, since all these entities
				// get triggered at once.
				// we could even do it on a toggle on set/clear hd area if this is too expensive
				ms_nextHdAreaUpdate = curTime + 1500;
			}
		}

		// process external requests for HD .txd upgrading
		CPed* playerPed = CGameWorld::FindLocalPlayer();
		u32 playerTrailerIdx = (u32)strLocalIndex::INVALID_INDEX;
		u32 playerVehicleIdx = (u32)strLocalIndex::INVALID_INDEX;
		if(playerPed && playerPed->GetMyVehicle())
		{
			playerVehicleIdx = playerPed->GetMyVehicle()->GetModelIndex();
			if(playerPed->GetMyVehicle()->HasTrailer())
			{
				const CTrailer* pTrailer = playerPed->GetMyVehicle()->GetAttachedTrailerOrDummyTrailer();
				if(pTrailer)
				{
					playerTrailerIdx = pTrailer->GetModelIndex();
				}
			}
		}

		for(u32 i=0; i<ms_externalRequests.GetCount(); i++)
		{
			CExternalRequest& request = ms_externalRequests[i];			
			CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(request.m_modelIndex)));
			Assert(pMI);
			if (pMI && pMI->GetHasLoaded())
			{
				if (velocity.Mag2() >= capSpeedSquard)
				{
					if (playerTrailerIdx != request.m_modelIndex && playerVehicleIdx != request.m_modelIndex)
						continue;
				}
				CTexLod::TriggerHDTxdUpgrade(request.m_assetLoc, request.m_modelIndex);
			}
		}

		ms_externalRequests.Reset();
	}

#if __BANK 
	m_totalMemoryUsed = 0;
#endif

	//look for HD entries which have timed out
	atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
	it.Start();
	while(!it.AtEnd()){
		s32 keyToDelete = it.GetKey();
		CTxdPairBinding* pPairBinding = it.GetData();

		it.Next();

		pPairBinding->Update();
		if (pPairBinding->IsTimedOut()){
			if (pPairBinding->GetHighTxdIdx() != -1)
			{
				CStreaming::ClearRequiredFlag(pPairBinding->GetHighTxdIdx(), g_TxdStore.GetStreamingModuleId(), STRFLAG_DONTDELETE);
			}
			delete(pPairBinding);
			ms_boundTxds.Delete(keyToDelete);
		}
		else
		{
#if __BANK 
			m_totalMemoryUsed += pPairBinding->GetTextureMemUsage();
#endif
		}
	}

#if NG_HD_SPLIT
	for (s32 i = 0; i < ms_virtMemToFree.GetCount(); ++i)
	{
		if (ms_virtMemToFree[i].ptr != NULL)
		{
			if (ms_virtMemToFree[i].freeCountdown > 0)
			{
				ms_virtMemToFree[i].freeCountdown--;
			}
			else
			{
				ms_virtMemToFree[i].Release();
			}
		}
	}
#endif // NG_HD_SPLIT
}

bool CTexLod::TriggerHdForEntity(CEntity* pEntity, void* UNUSED_PARAM(pData))
{
	CBaseModelInfo* pBMI = pEntity->GetBaseModelInfo();
	Assert(pBMI);
	if (pBMI->GetHasLoaded())
	{
		CTexLod::TriggerHDTxdUpgrade(pBMI->GetAssetLocation(), pEntity->GetModelIndex());
		Debug_RegisterEntityActivations(pEntity);
	}
	return true;
}

void CTexLod::FlushAllUpgrades()
{
#if __BANK 
	m_totalMemoryUsed = 0;
#endif

	atMap<s32, CTxdPairBinding*>::Iterator it = ms_boundTxds.CreateIterator();
	it.Start();
	while (!it.AtEnd())
	{
		s32 keyToDelete = it.GetKey();
		CTxdPairBinding* pPairBinding = it.GetData();
		it.Next();

		delete pPairBinding;
		ms_boundTxds.Delete(keyToDelete);
	}
}

#if NG_HD_SPLIT
void CTexLod::AddDeferredFreePtr(void* ptr, size_t size, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset)
{
	if (!ptr)
		return;

	sVirtMemData* pNewData = NULL;
	for (s32 i = 0; i < ms_virtMemToFree.GetCount(); ++i)
	{
		if (ms_virtMemToFree[i].ptr == NULL)
		{
			pNewData = &ms_virtMemToFree[i];
			break;
		}
	}

	if (!pNewData)
		pNewData = &ms_virtMemToFree.Grow();

	pNewData->ptr = ptr;
	pNewData->size = size;
	pNewData->hdTxdIdx = hdTxdIdx;
	pNewData->targetAsset = targetAsset;
	pNewData->freeCountdown = 4;

	if (targetAsset.IsValid())
		targetAsset.GetStreamingModule()->AddRef(targetAsset.GetLocalIndex(), REF_RENDER);
	if (hdTxdIdx.IsValid())
		g_TxdStore.AddRef(hdTxdIdx, REF_RENDER);
}
#endif // NG_HD_SPLIT

#if NG_HD_SPLIT
void CTexLod::sVirtMemData::Release()
{
	if (ptr != NULL)
	{
		sysMemVirtualAllocator::sm_Instance->UnmapVirtual(ptr, size);
		sysMemVirtualAllocator::sm_Instance->FreeVirtual(ptr);
		ptr = NULL;

		if (hdTxdIdx.IsValid())
		{
			g_TxdStore.RemoveRef(hdTxdIdx, REF_RENDER);
			hdTxdIdx.Invalidate();
		}

		if (targetAsset.IsValid())
		{

			strStreamingModule *module = targetAsset.GetStreamingModule();
			strLocalIndex objIdx = targetAsset.GetLocalIndex();
			module->RemoveRef(objIdx, REF_RENDER);
			targetAsset.Invalidate();
		}
	}
}
#endif

void CTexLod::FlushMemRemaps()
{
#if NG_HD_SPLIT
	for (s32 i = 0; i < ms_virtMemToFree.GetCount(); ++i)
		ms_virtMemToFree[i].Release();
#endif
}

void CTexLod::SetSwapStateSingle(bool bSwap, fwAssetLocation targetAsset)
{
	if (!ms_bEnableSwapping){
		return;
	}

	atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
	for ( it.Start(); !it.AtEnd(); it.Next() )
	{
		CTxdPairBinding* pPairBinding = it.GetData();

		fwAssetLocation pairHdTxdAsset(STORE_ASSET_TXD,pPairBinding->GetHighTxdIdx().Get());
		fwAssetLocation pairTargetAsset = pPairBinding->GetTargetAsset();
		fwAssetLocation otherAsset;
		if (pairTargetAsset == targetAsset)
		{
			otherAsset = pairHdTxdAsset;
		} 
		else if (pairHdTxdAsset == targetAsset)
		{
			otherAsset = pairTargetAsset;
		}
		else
		{
			continue;
		}

		// Ignore request to re-enable a swap if the other asset is still being
		// defragged.  This can occur in rare circumstances when both assets are
		// being defragged at the same time, then the
		// strStreamingModule::DefragmentComplete call is made for the first
		// asset.  In this situation, the swap will be re-enabled when either
		// the second asset gets a DefragComplete call, or when the defrag frame
		// update completes, which ever comes first.
		if (!bSwap || !otherAsset.IsBeingDefragged())
		{
			Assert(!bSwap || !targetAsset.IsBeingDefragged());
			pPairBinding->SetSwapStateAll(bSwap);
		}
	}
}

void CTexLod::SetSwapStateAll(bool bSwap, bool bDefragOnly){

	if (!ms_bEnableSwapping){
		return;
	}

	if (bDefragOnly)
	{
		atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
		for ( it.Start(); !it.AtEnd(); it.Next() )
		{
			CTxdPairBinding* pPairBinding = it.GetData();

			strLocalIndex highTxdIdx = strLocalIndex(pPairBinding->GetHighTxdIdx());
			fwAssetLocation targetAsset = pPairBinding->GetTargetAsset();

			if (highTxdIdx != -1 && targetAsset.IsValid())
			{
				if (g_TxdStore.IsBeingDefragged(highTxdIdx) || targetAsset.IsBeingDefragged())
				{
					pPairBinding->SetSwapStateAll(bSwap);
				}
			}
		}
	} 
	else
	{
		Assertf(false,"Shouldn't be using this code path");

#if __BANK
		atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
		for ( it.Start(); !it.AtEnd(); it.Next() )
		{
			CTxdPairBinding* pPairBinding = it.GetData();

			if (bSwap == false)
			{
				g_TxdStore.AddRef(strLocalIndex(pPairBinding->GetHighTxdIdx()), REF_OTHER);

				fwAssetLocation target = pPairBinding->GetTargetAsset();
				strStreamingModule *pModule = target.GetStreamingModule();
				Assert(pModule);
				if (pModule)
				{
					pModule->AddRef(target.GetStoreSlot(), REF_OTHER);	
				}
			} else {
				g_TxdStore.RemoveRef(strLocalIndex(pPairBinding->GetHighTxdIdx()), REF_OTHER);

				fwAssetLocation target = pPairBinding->GetTargetAsset();
				strStreamingModule *pModule = target.GetStreamingModule();
				Assert(pModule);
				if (pModule)
				{
					pModule->RemoveRef(target.GetStoreSlot(), REF_OTHER);	
				}
			}

			pPairBinding->SetSwapStateAll(bSwap);
		}
#endif //__BANK

	}
}

// cleanup all currently bound txds immediately
void CTexLod::UnbindAllBoundTxds(void){

	atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
	for ( it.Start(); !it.AtEnd();  )
	{
		s32 keyToDelete = it.GetKey();
		CTxdPairBinding* pPairBinding = it.GetData();

		it.Next();

		delete (pPairBinding);
		ms_boundTxds.Delete(keyToDelete);
	}
	ms_boundTxds.Reset();
}

void CTexLod::StoreHDMapping(eStoreType type, u32 assetSlot, s32 txdHDSlot)
{
	fwAssetLocation location(type, assetSlot);

// 	if (stricmp(location.GetName(),"vehshare")==0)
// 	{
// 		Displayf("found");
// 	}
// 
// 	if (stricmp(location.GetName(),"tailgater")==0)
// 	{
// 		Displayf("found");
// 	}


	if (Verifyf(location.IsValid() && location.GetStoreSlot().IsValid(), "Asset Location Invalid") && (Verifyf(g_TxdStore.IsValidSlot(strLocalIndex(txdHDSlot)), "Invalid .txd slot for HD mapping")))
	{
		if (ms_assetToHDMappings.Access(location) == NULL)
		{
			ms_assetToHDMappings.Insert(location, txdHDSlot);
			location.SetIsHDCapable(true);
			Assertf(ms_assetToHDMappings.GetNumUsed() < ((RSG_PC || RSG_DURANGO || RSG_ORBIS) ? 32767 : 19000), "Too many HD mappings in TexLod");
		}
	}
}

// ---- BANK stuff ----

#if __BANK

// the update cell callback can be as simple as...
void CTexLod::UpdateCellForTxd(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (assetHashSlotArray[targetTxdListIndex] == 0){
		ms_debugTriggerMI = fwModelId::MI_INVALID;
		return;
	}

	fwAssetLocation loc = static_cast<fwAssetLocation>(assetHashSlotArray[targetTxdListIndex]);
	CTxdPairBinding** ppBoundTxd = ms_boundTxds.Access(loc);

	if (col == 0){
		if (ppBoundTxd && (*ppBoundTxd) && (*ppBoundTxd)->IsTexIndexInTxdBound(row)){
			pResult->SetString("*");
		} else {
			pResult->SetString(" ");
		}
	} 
	else if  (col == 1)
	{
		if (ppBoundTxd && (*ppBoundTxd) && (*ppBoundTxd)->GetHighTxdIdx().Get() > -1)
		{
			fwTxd* pTargetTxd = g_TxdStore.Get(strLocalIndex((*ppBoundTxd)->GetHighTxdIdx()));
			if (pTargetTxd)
			{
				const grcTexture* pTex = pTargetTxd->GetEntry(row);
				if (pTex)
				{
					pResult->SetString(pTex->GetName());
				}
			}
		}
	} 
	else if  (col == 2)
	{	
		if (ppBoundTxd && (*ppBoundTxd) && (*ppBoundTxd)->GetHighTxdIdx().Get() > -1)
		{
			fwTxd* pTargetTxd = g_TxdStore.Get(strLocalIndex((*ppBoundTxd)->GetHighTxdIdx()));
			if (pTargetTxd)
			{
				const grcTexture* pTex = pTargetTxd->GetEntry(row);
				if (pTex)
				{
					char sizeInK[16];
					snprintf(sizeInK, 16, "%dK", int((double)pTex->GetPhysicalSize() / 1024.0));
					sizeInK[15] = '\0';

					pResult->SetString(sizeInK);
				}
			}
		}
	} 
	else 
	{
		pResult->SetString("");
		ms_debugTriggerMI = fwModelId::MI_INVALID;
	} 
}

// the update cell callback can be as simple as...
void CTexLod::UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (assetHashSlotArray[row] == 0){
		return;
	}

	fwAssetLocation loc = static_cast<fwAssetLocation>(assetHashSlotArray[row]);
	CTxdPairBinding** ppBoundTxd = ms_boundTxds.Access(loc);
	
	if (col == 0){
		if (ppBoundTxd && (*ppBoundTxd)){
			CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex((*ppBoundTxd)->GetTriggeringMI())));
			Assert(pMI);
			pResult->SetString(pMI->GetModelName());
		}
	} else if (col == 1){
		pResult->SetString(loc.GetStoreTypeStr());
	} else if (col == 2){
		pResult->SetString(loc.GetName());
	} else if (col == 3){
		if (ppBoundTxd && (*ppBoundTxd) && (*ppBoundTxd)->GetHighTxdIdx().Get() > -1){
			pResult->SetString(g_TxdStore.GetName(strLocalIndex((*ppBoundTxd)->GetHighTxdIdx())));
		} else {
			pResult->SetString("<NULL>");
		}
	} else if (col == 4){
		char	coolDownStars[7] = "******";
		if (ppBoundTxd && (*ppBoundTxd)){
			u32 nullPos = (*ppBoundTxd)->GetTimeoutVal();
			coolDownStars[nullPos] = '\0';
		}

		pResult->SetString(coolDownStars);
	} else {
		if (ppBoundTxd && (*ppBoundTxd))
		{
			char sizeInK[16];
 			snprintf(sizeInK, 16, "%dK", int((double)(*ppBoundTxd)->GetTextureMemUsage() / 1024.0));
 			sizeInK[15] = '\0';
 			pResult->SetString(sizeInK);
		}
	}
}

#define MAX_NAME_LEN (64)
char selectedEntityName[MAX_NAME_LEN];
char selectedEntityDistance[MAX_NAME_LEN];

// update the debug window
void CTexLod::DebugUpdate(void){

	g_HDAssetMgr.DebugUpdate();

	if (g_PickerManager.IsEnabled() && g_PickerManager.IsCurrentOwner("HD tex distance"))
	{
		ms_pSelectedEntity = reinterpret_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
		if (ms_pSelectedEntity)
		{
			if (!ms_pSelectedEntity->GetIsHDTxdCapable())
			{
				ms_pSelectedEntity = NULL;
				g_PickerManager.ResetList(true);
				g_PickerManager.OnDisplayResultsWindow();

				selectedEntityName[0] = '\0';
				selectedEntityDistance[0] = '\0';

				ms_selectedEntityDist = -1.0f;
				ms_bIsActivated = false;
			} else
			{
				ms_HDActivationDistance = ms_pSelectedEntity->GetBaseModelInfo()->GetHDTextureDistance();
				float HDActivationDistanceScaled = ms_pSelectedEntity->GetBaseModelInfo()->GetHDTextureDistance() * GetClampedScaleValue();

				sprintf(selectedEntityName,"%s",ms_pSelectedEntity->GetBaseModelInfo()->GetModelName());
				sprintf(selectedEntityDistance,"%.1f  (%s)", ms_selectedEntityDist, ms_bIsActivated ? "In range" : "_");

				grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

				char tempString[128];
				sprintf(tempString, "Model: %s", selectedEntityName);
				grcDebugDraw::Text(Vector2(0.25f, 0.8f), Color32(255,80,80), tempString, true, 3.0f, 3.0f);

				sprintf(tempString, "curr dist: %s", selectedEntityDistance);
				grcDebugDraw::Text(Vector2(0.25f, 0.84f), Color32(255,80,80), tempString,  true, 3.0f, 3.0f);

				sprintf(tempString, "HD activate dist: %.1f (%.1f)", HDActivationDistanceScaled, ms_HDActivationDistance);
				grcDebugDraw::Text(Vector2(0.25f, 0.88f), Color32(255,80,80), tempString,  true, 3.0f, 3.0f);

				grcDebugDraw::TextFontPop();
			}
		}
	}

	if (m_showHDTextureMemUsage)
	{
		grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

		const atVarString temp("HD Texture Memory Usage - %.2fMB", float((double)m_totalMemoryUsed / (1024.0 * 1024.0)) );

		grcDebugDraw::Text(Vector2(270.0f, 580.0f), DD_ePCS_Pixels, CRGBA_White(), temp, true, 1.0f, 1.0f);

		grcDebugDraw::TextFontPop();
	}

	if (m_showHDMemUsage)
	{
		u32 vechiclePhsyical;
		u32 vechicleVirtual;

		CVehicleModelInfo::GetHDMemoryUsage(vechiclePhsyical, vechicleVirtual);
		u32 totalPhysical = m_totalMemoryUsed + vechiclePhsyical;

		grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

		const atVarString temp("Total HD Memory Usage - Physical %.2fMB, Virtual %.2fMB", 
								float((double)totalPhysical / (1024.0 * 1024.0)),
								float((double)vechicleVirtual / (1024.0 * 1024.0)));

		grcDebugDraw::Text(Vector2(270.0f, 560.0f), DD_ePCS_Pixels, CRGBA_White(), temp, true, 1.0f, 1.0f);

		grcDebugDraw::TextFontPop();
	}
	if (bListHDTxds){
		// create the debug window if not already existing
		if (ms_pDebugWindow==NULL){
			// this creates a window
			CUiColorScheme colorScheme;
			ms_pDebugWindow = rage_new CUiGadgetWindow(40, 40, 620, 320, colorScheme);
			ms_pDebugWindow->SetTitle("CTexLod HD Active bindings");

			// this attaches a scrolly list to the window
			float afColumnOffsets[] = { 0.0f, 130.0f, 170.0f, 320.0f, 470.0f, 550.0f };
			const char* columnTitles1[] = { "Model", "Type", "Asset Name", "HD Txd", "Cooldown", "HD Size" };
			u32 numListEntries = 12;
			ms_pHDTxdList = rage_new CUiGadgetList(42, 40+38.0f, 620-20.0f, numListEntries, 6, afColumnOffsets, colorScheme, columnTitles1);
			ms_pDebugWindow->AttachChild(ms_pHDTxdList);
			ms_pHDTxdList->SetUpdateCellCB(CTexLod::UpdateCellForAsset);

			// this attaches a scrolly list to the window
 			float afColumnOffsets2[] = { 0.0f, 40.0f, 260.0f };
			const char* columnTitles2[] = { "Bound", "Texture name", "HD Size" };
			numListEntries = 8;
			ms_pTexList = rage_new CUiGadgetList(42, 220+38.0f, 620-20.0f, numListEntries, 3, afColumnOffsets2, colorScheme, columnTitles2);
			ms_pDebugWindow->AttachChild(ms_pTexList);
			ms_pTexList->SetUpdateCellCB(CTexLod::UpdateCellForTxd);

			// this attaches all of that to the root gadget
			g_UiGadgetRoot.AttachChild(ms_pDebugWindow);
		}

		s32 currentSelectionSlot = ms_pHDTxdList->GetCurrentIndex();
		s32 userSelectSlot = -1;
		if (ms_pHDTxdList->UserProcessClick()){
			userSelectSlot = ms_pHDTxdList->GetCurrentIndex();		// user clicked, so pick up new slot
		}

		// if no target slot then make sure target asset is also invalid
		if (currentSelectionSlot == -1){
			ms_currentAsset.Set(STORE_ASSET_INVALID,0);
		}

		u32 numEntries = ms_boundTxds.GetNumUsed();
		ms_pHDTxdList->SetNumEntries(ms_boundTxds.GetNumUsed());	// this invalidates current index in window!		

		if (numEntries == 0){
			ms_pTexList->SetNumEntries(0);
			targetTxdListIndex = -1;
			return;
		}
		
 		u32 i=0;
		s32 currentAssetSlot = -1;
 		atMap<s32, CTxdPairBinding*>::Iterator it	= ms_boundTxds.CreateIterator();
 		for ( it.Start(); !it.AtEnd(); it.Next() )
 		{
			s32 assetLocHash = it.GetKey();
			if (assetLocHash == ms_currentAsset){
				currentAssetSlot = i;				// establish slot that currently selected asset is in
			}
 			assetHashSlotArray[i++] = assetLocHash;
 		}

		// if user made a selection - use it in preference to tracking the current asset
		if (userSelectSlot != -1){
			currentAssetSlot = userSelectSlot;
		}

		ms_pHDTxdList->SetCurrentIndex(currentAssetSlot);
		targetTxdListIndex = currentAssetSlot;

		// user select, so update the target asset accordingly
		if ((userSelectSlot != -1)){
			// use the asset out of the current slot as our target asset
			if (currentAssetSlot >= 0){	
				fwAssetLocation loc = static_cast<fwAssetLocation>(assetHashSlotArray[currentAssetSlot]);
				CTxdPairBinding** ppBoundTxd = ms_boundTxds.Access(loc);
				if (ppBoundTxd && (*ppBoundTxd) && (*ppBoundTxd)->GetHighTxdIdx().Get() > -1){
					fwTxd* pTargetTxd = g_TxdStore.Get(strLocalIndex((*ppBoundTxd)->GetHighTxdIdx()));
					ms_debugTxdIdx = (*ppBoundTxd)->GetHighTxdIdx();
					ms_debugTriggerMI = (*ppBoundTxd)->GetTriggeringMI();
					if (pTargetTxd && (ms_currentAsset!=loc)){
						ms_pTexList->SetNumEntries(pTargetTxd->GetCount());
					}
				}
				ms_currentAsset = loc;
			} 
		}
	} else {
		// destroy debug window when no longer required
		if (ms_pDebugWindow){
			targetTxdListIndex = -1;
			ms_pTexList->DetachFromParent();
			ms_pHDTxdList->DetachFromParent();
			ms_pDebugWindow->DetachFromParent();

			delete(ms_pDebugWindow);
			ms_pDebugWindow = NULL;

			delete(ms_pHDTxdList);
			ms_pHDTxdList = NULL;
			
			delete(ms_pTexList);
			ms_pTexList = NULL;
		}
	}

	if (g_renderHdArea)
	{
		Vec3V pos = ms_hdArea.GetCenter();
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pos), ms_hdArea.GetRadiusf(), ms_bHasHdArea ? Color32(0, 0, 255) : Color32(200, 200, 200), false);
	}
}

void CTexLod::UnbindAllCB(void){
	CTexLod::UnbindAllBoundTxds();
}

bool CTexLod::IsTxdUpgradedToHD(const fwTxd *pfwTxd)
{
	atMap<s32, CTxdPairBinding*>::Iterator it = ms_boundTxds.CreateIterator();
	while (!it.AtEnd())
	{
		CTxdPairBinding* pPairBinding = it.GetData();

		fwPtrListSingleLink txdList;
		pPairBinding->GetTargetAsset().GetTxdList(txdList);
#if __DEV
		const int numUsed = txdList.CountElements();
		if (g_MaxTargetTxds < numUsed) {
			g_MaxTargetTxds = numUsed;
			Displayf("g_MaxTargetTxds = %d", g_MaxTargetTxds);
		}
#endif // __DEV

		fwPtrNodeSingleLink* pEntry = txdList.GetHeadPtr();

		while (pEntry != NULL)
		{
			fwTxd *pEntryTxd = (fwTxd*)pEntry->GetPtr();
			if (pEntryTxd == pfwTxd)
			{
				return true;
			}

			pEntry = (fwPtrNodeSingleLink*)pEntry->GetNextPtr();
		}

		it.Next();
	}

	return false;
}

bool CTexLod::IsTextureUpgradedToHD(const grcTexture *pTexture)
{
	atMap<s32, CTxdPairBinding*>::Iterator it = ms_boundTxds.CreateIterator();
	while (!it.AtEnd())
	{
		CTxdPairBinding* pPairBinding = it.GetData();

		atArray<CMipSwitcher> &boundTexSwitchers = pPairBinding->GetBoundTextureSwitchers();
		for (int i = 0; i < boundTexSwitchers.GetCount(); ++i)
		{
			if (boundTexSwitchers[i].GetLowTex() == pTexture)
			{
				return true;
			}
		}
		it.Next();
	}
	return false;
}

// this should be safe to call from either update or render threads, since it doesn't rely on ms_boundTxds
bool CTexLod::IsModelUpgradedToHD(const u32 modelIndex, bool bBaseAssetOnly)
{
	const CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

	if (pMI)
	{
		fwAssetLocation targetAsset = pMI->GetAssetLocation();

		// iterate over all txds in chain for this modelinfo
		while(targetAsset.IsValid()){
			if (targetAsset.GetIsBoundHD())
				return true;
			else if (bBaseAssetOnly)
				return false;
			targetAsset = targetAsset.GetParentAsset();
		}
	}
	return false;
}

// this should be safe to call from either update or render threads, since it doesn't rely on ms_boundTxds
bool CTexLod::IsModelHDTxdCapable(const u32 modelIndex, bool bBaseAssetOnly)
{
	const CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId((strLocalIndex(modelIndex))));

	if (pMI)
	{
		fwAssetLocation targetAsset = pMI->GetAssetLocation();

		// iterate over all txds in chain for this modelinfo
		while(targetAsset.IsValid()){
			if (targetAsset.GetIsHDCapable())
				return true;
			else if (bBaseAssetOnly)
				return false;
			targetAsset = targetAsset.GetParentAsset();
		}
	}
	return false;
}

bool CTexLod::IsEntityCloseEnoughForHDSwitch(const CEntity* pEntity)
{
	if (pEntity->GetIsHDTxdCapable())
	{
		const float camDist = Mag(pEntity->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(camInterface::GetPos())).Getf();
		float minDistance = 0.0f;

		if (pEntity->IsBaseFlagSet(fwEntity::HAS_HD_TEX_DIST))
		{
			Assertf(pEntity && pEntity->GetIsTypePed(), "Only Peds should have fwEntity::HAS_HD_TEX_DIST flag set");
			minDistance = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetHDDist();
		}
		else
		{
			minDistance = pEntity->GetBaseModelInfo()->GetHDTextureDistance();
		}

		return camDist < minDistance * GetClampedScaleValue();
	}

	return false;
}

// update txd view callbacks
void CTexLod::ShowTargetAssetInTexViewCB(void)
{
	if (ms_debugTriggerMI != fwModelId::MI_INVALID)
	{
		CDebugTextureViewerInterface::SelectModelInfo(ms_debugTriggerMI.Get(), NULL, false, true);
	}
}
	
void CTexLod::ShowTargetHDTxdInTexViewCB(void)
{
	if (ms_debugTriggerMI != fwModelId::MI_INVALID)
	{
		CTxdRef	ref(AST_TxdStore, ms_debugTxdIdx.Get(), INDEX_NONE, "");
		CDebugTextureViewerInterface::SelectTxd(ref, false, true);
	}
}

void EnableHDTexturePickerCB(void)
{
	// Enable picker when it changes
	if (g_PickerManager.IsEnabled() == false)
	{
		//if (m_usePicker)
		{
			g_PickerManager.TakeControlOfPickerWidget("HD tex distance");

			fwPickerManagerSettings texLodSettings(ENTITY_RENDER_PICKER, true, true, 
				ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT, false);
			g_PickerManager.SetPickerManagerSettings(&texLodSettings);

			g_PickerManager.ResetList(false);
		}

		g_PickerManager.SetEnabled(true);
	} else {
		g_PickerManager.SetEnabled(false);
	}
}

void CTexLod::HDActivationDistanceCB(void)
{
	if (ms_pSelectedEntity)
	{
		CBaseModelInfo* pMI = ms_pSelectedEntity->GetBaseModelInfo();
		Assert(pMI);

		if (pMI->GetIsHDTxdCapable())
		{
			pMI->SetHDTextureDistance(ms_HDActivationDistance);
		}
	}
}

void CTexLod::DisableHDTexCB(void)
{
	if (ms_bDisableHDTex){
		CTexLod::SetSwapStateAll(false, false);
		CTexLod::EnableStateSwapper(false);
	} else {
		CTexLod::EnableStateSwapper(true);
		CTexLod::SetSwapStateAll(true, false);
	}
}

bool CTexLod::AssetHasRequest(fwAssetLocation targetAsset)
{
	return ms_boundTxds.Access(targetAsset) != NULL;
}

void CTexLod::AddWidgets(bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup("HD Assets", false);

	pBank->AddToggle("Display HD area", &g_renderHdArea);
	pBank->AddToggle("Only External Requests", &m_onlyExternalRequsts);

	pBank->PushGroup("HD assets for cutscenes",false);
		pBank->AddToggle("Enable default HD requests", &g_HDAssetMgr.m_enableDefaultHDRequests);
		pBank->AddToggle("Display HD requests", &g_HDAssetMgr.m_displayHDRequests);
	pBank->PopGroup();

	pBank->AddToggle("Show HD Memory Usage", &m_showHDMemUsage);

	pBank->PushGroup("Textures", false);
		pBank->AddToggle("Enabled", &CTexLod::ms_bEnabled);
		pBank->AddButton("Unbind all", UnbindAllCB);
		pBank->AddToggle("Show HD Texture Memory Usage", &m_showHDTextureMemUsage);
		pBank->AddToggle("List HD txds", &bListHDTxds);
		pBank->AddButton("Show target asset in texture viewer", &ShowTargetAssetInTexViewCB);
		pBank->AddButton("Show bound HD Txd in texture viewer", &ShowTargetHDTxdInTexViewCB);
	pBank->PopGroup();

	CVehicleModelInfo::AddHDWidgets(*pBank);

	pBank->PushGroup("HD texture distance edit", false);
		pBank->AddButton("Toggle HD texture distance picker", EnableHDTexturePickerCB);
		pBank->AddSlider("HD activation distance", &ms_HDActivationDistance, 1.0f, 200.0f, 1.0f, HDActivationDistanceCB);
		pBank->AddToggle("Disable HD tex", &ms_bDisableHDTex, DisableHDTexCB);
		pBank->AddText("Selected entity name", selectedEntityName, MAX_NAME_LEN, true);
		pBank->AddText("Selected entity distance", selectedEntityDistance, MAX_NAME_LEN, true);
	pBank->PopGroup();

	pBank->PopGroup();
}


#endif //__BANK
