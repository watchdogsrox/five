/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneCostTracker.cpp
// PURPOSE : debug widgets for tracking cost of a scene
// AUTHOR :  Ian Kiigan
// CREATED : 08/06/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/SceneCostTracker.h"

SCENE_OPTIMISATIONS();

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "camera/CamInterface.h"
#include "debug/debugscene.h"
#include "grcore/debugdraw.h"
#include "rmcore/drawable.h"
#include "fragment/type.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/world/entitydesc.h"
#include "input/mouse.h"
#include "renderer/sprite2d.h"
#include "scene/entity.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodDrawable.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/texLod.h"
#include "scene/world/GameWorld.h"
#include "text/text.h"
#include "text/textconversion.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "grcore/viewport.h"
#include "fwmaths/Rect.h"
#include "streaming/streaming.h"
#include "streaming/streamingcleanup.h"
#include "streaming/zonedassetmanager.h"
#include "system/controlmgr.h"
#include "system/keyboard.h"
#include "vector/color32.h"
#include "vfx/ptfx/ptfxmanager.h"

XPARAM(maponly);
XPARAM(maponlyextra);

CSceneCostTracker g_SceneCostTracker;
CSceneMemoryCheck g_SceneMemoryCheck;

bool CSceneCostTracker::ms_bShowBudgets = false;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Txds = true;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Dwds = true;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Drawables = true;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Total = true;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Frags = true;
bool CSceneCostTracker::ms_bShowSceneCostFromList_Ptfx = true;
bool CSceneCostTracker::ms_bShowSceneNumShaders = false;

// drawing info
bool CSceneCostTracker::ms_bPhysical = true;
bool CSceneCostTracker::ms_bVirtual = true;
u32 CSceneCostTracker::ms_nWidth = 400;
u32 CSceneCostTracker::ms_nHeight = 20;
bool CSceneCostTracker::ms_bMove = false;
Vector2 CSceneCostTracker::ms_vPos;
Vector2 CSceneCostTracker::ms_vClickDragDelta;
u32 CSceneCostTracker::ms_nNumEntries = 0;
u32 CSceneCostTracker::ms_nMaxMemPhysMB = 100;
u32 CSceneCostTracker::ms_nMaxMemVirtMB = 100;
u32 CSceneCostTracker::ms_nMaxNumShaders = 1000;
bool CSceneCostTracker::ms_bOnlyTrackGbuf = false;
bool CSceneCostTracker::ms_bOnlyTrackNonzeroAlpha = false;
bool CSceneCostTracker::ms_bFilterResults = false;
bool CSceneCostTracker::ms_bTrackParentTxds = true;
bool CSceneCostTracker::ms_bOnlyTrackMapData = false;
bool CSceneCostTracker::ms_bShowLodBreakdown = false;
bool CSceneCostTracker::ms_bIncludeHdTxdPromotion = true;
bool CSceneCostTracker::ms_bWriteLog = false;
bool CSceneCostTracker::ms_bIncludeZoneAssets = false;
s32 CSceneCostTracker::ms_nSelectedFilter = 0;
s32 CSceneCostTracker::ms_nSelectedList = SCENECOSTTRACKER_LIST_PVS;
char CSceneCostTracker::ms_achLogFileName[RAGE_MAX_PATH] = "X:\\gta5\\build\\dev\\SceneCosts.txt";

Vec3V CSceneCostTracker::ms_vSearchPos;


// world search
#define COSTTRACKER_MAX_RADIUS (4500)
s32 CSceneCostTracker::ms_nSearchRadius = COSTTRACKER_MAX_RADIUS;

enum
{
	FILTER_TYPE_HD,
	FILTER_TYPE_LOD,
	FILTER_TYPE_SLOD1,
	FILTER_TYPE_SLOD2,
	FILTER_TYPE_SLOD3,
	FILTER_TYPE_SLOD4,
	FILTER_TYPE_ORPHANHD,
	FILTER_TYPE_LODDEDHD,
	FILTER_TYPE_PROPS,
	FILTER_TYPE_ANYLOD,

	FILTER_TYPE_TOTAL
};

const char* apszFilterNames[FILTER_TYPE_TOTAL] =
{
	"HD",
	"LOD",
	"SLOD1",
	"SLOD2",
	"SLOD3",
	"SLOD4",
	"ORPHAN HD",
	"LODDED HD",
	"PROPS",
	"ANY LOD"
};

Color32 aLodBreakdownColors[LODTYPES_MAX_NUM_LEVELS+1] =
{
	Color32(255,255,0),		//yellow - HD
	Color32(0,255,255),		//cyan - LOD
	Color32(0,0,255),		//blue - SLOD1
	Color32(255,0,255),		//pink - SLOD2
	Color32(255,0,0),		//red - SLOD3
	Color32(255,255,255),	//white - orphan HD
	Color32(100,100,100)	//grey - SLOD4
};

#define TITLE_H (20)
#define SCREEN_COORDS(x,y)			Vector2( ( static_cast<float>(x) / grcViewport::GetDefaultScreen()->GetWidth() ), ( static_cast<float>(y) / grcViewport::GetDefaultScreen()->GetHeight() ) )
#define DRAW_BAR_PHYS(x,y, stat, b)	DrawMemBar(x, y, psStats->GetPhysCost(stat), psStats->GetPhysCostPeak(stat), psStats->GetPhysCostFiltered(stat), ms_nMaxMemPhysMB*1024, b)
#define DRAW_BAR_VIRT(x,y, stat, b)	DrawMemBar(x, y, psStats->GetVirtCost(stat), psStats->GetVirtCostPeak(stat), psStats->GetVirtCostFiltered(stat), ms_nMaxMemVirtMB*1024, b)

const char* apszListTypes[SCENECOSTTRACKER_LIST_TOTAL] =
{
	"Visibility",					//SCENECOSTTRACKER_LIST_PVS
	"Sphere",						//SCENECOSTTRACKER_LIST_SPHERE
	"Scene streamer",				//SCENECOSTTRACKER_LIST_PSS_ALL
	"Scene streamer (unloaded)"		//SCENECOSTTRACKER_LIST_PSS_NOTLOADED
};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		add bank widgets
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddWidgets(bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup("Costs", false);

	pBank->AddToggle("SHOW scene costs", &ms_bShowBudgets);
	pBank->AddCombo("Source list", &ms_nSelectedList, SCENECOSTTRACKER_LIST_TOTAL, &apszListTypes[0]);
	pBank->AddSlider("Search radius", &ms_nSearchRadius, 1, COSTTRACKER_MAX_RADIUS, 1);
	pBank->AddSeparator();

	pBank->AddTitle("Costs to track");
	pBank->AddToggle("Total costs", &ms_bShowSceneCostFromList_Total);
	pBank->AddToggle("Txd costs", &ms_bShowSceneCostFromList_Txds);
	pBank->AddToggle("Dwd costs", &ms_bShowSceneCostFromList_Dwds);
	pBank->AddToggle("Drawable costs", &ms_bShowSceneCostFromList_Drawables);
	pBank->AddToggle("Frag costs", &ms_bShowSceneCostFromList_Frags);
	pBank->AddToggle("Ptfx costs", &ms_bShowSceneCostFromList_Ptfx);
	pBank->AddSeparator();

	pBank->AddTitle("Counts to track");
	pBank->AddToggle("Count num shaders", &ms_bShowSceneNumShaders);

	pBank->AddSeparator();
	pBank->AddTitle("Memory");
	pBank->AddToggle("Show VRAM", &ms_bPhysical);
	pBank->AddToggle("Show MAIN RAM", &ms_bVirtual);
	pBank->AddSlider("Max VRAM (mb)", &ms_nMaxMemPhysMB, 1, 512, 1);
	pBank->AddSlider("Max MAIN RAM (mb)", &ms_nMaxMemVirtMB, 1, 512, 1);
	pBank->AddSlider("Max num shaders", &ms_nMaxNumShaders, 1, 5000, 100);
	pBank->AddSeparator();

	pBank->AddTitle("Settings");
	pBank->AddToggle("Only track map data", &ms_bOnlyTrackMapData);
	pBank->AddToggle("Only track entries in GBUF phase", &ms_bOnlyTrackGbuf);
	pBank->AddToggle("Only track entries with alpha>0", &ms_bOnlyTrackNonzeroAlpha);
	pBank->AddToggle("Include parent TXDs", &ms_bTrackParentTxds);
	pBank->AddToggle("Show LOD breakdown", &ms_bShowLodBreakdown);
	pBank->AddSeparator();
	pBank->AddToggle("Enable filter", &ms_bFilterResults);
	pBank->AddToggle("Include zoned assets", &ms_bIncludeZoneAssets);
	pBank->AddToggle("Include HD promotion", &ms_bIncludeHdTxdPromotion);
	pBank->AddCombo("Filter by", &ms_nSelectedFilter, FILTER_TYPE_TOTAL, &apszFilterNames[0]);
	pBank->AddSeparator();
	pBank->AddText("Log file", ms_achLogFileName, 256);
	pBank->AddButton("Pick file", SelectLogFileCB);
	pBank->AddButton("Save to log", SaveToLogCB);

		pBank->PushGroup("Memory check");
		pBank->AddButton("Start mem check", StartMemCheckCB);
		pBank->AddButton("Stop mem check", StopMemCheckCB);
		pBank->PopGroup();

	pBank->PopGroup();

	ms_vPos.Set(700, 100);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ResetThisFrame
// PURPOSE:		clear stats for this frame
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::ResetThisFrame()
{
	CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_UPDATE];
	psStats->Reset();

	m_txdMap.Reset();
	m_drawableMap.Reset();
	m_fragMap.Reset();
	m_dwdMap.Reset();
	m_ptfxMap.Reset();

	m_streamingIndices.Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsFilteredEntity
// PURPOSE:		returns true if specified entity is one we are interested in tracking
//////////////////////////////////////////////////////////////////////////
bool CSceneCostTracker::IsFilteredEntity(CEntity* pEntity)
{
	if (pEntity && ms_bFilterResults)
	{
		switch (ms_nSelectedFilter)
		{
		case FILTER_TYPE_HD:
			return pEntity->GetLodData().IsHighDetail();
		case FILTER_TYPE_LOD:
			return pEntity->GetLodData().IsLod();
		case FILTER_TYPE_SLOD1:
			return pEntity->GetLodData().IsSlod1();
		case FILTER_TYPE_SLOD2:
			return pEntity->GetLodData().IsSlod2();
		case FILTER_TYPE_SLOD3:
			return pEntity->GetLodData().IsSlod3();
		case FILTER_TYPE_SLOD4:
			return pEntity->GetLodData().IsSlod4();
		case FILTER_TYPE_ORPHANHD:
			return pEntity->GetLodData().IsOrphanHd();
		case FILTER_TYPE_LODDEDHD:
			return pEntity->GetLodData().IsLoddedHd();
		case FILTER_TYPE_PROPS:
			{
				CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
				return pModelInfo->GetIsProp();
			}
		case FILTER_TYPE_ANYLOD:
			return !pEntity->GetLodData().IsHighDetail();
		default:
			break;
		}
		
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddCostsForPvsList
// PURPOSE:		process a PVS list and feeds memory costs into stats
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddCostsForPvsList(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	ms_vSearchPos = VECTOR3_TO_VEC3V( camInterface::GetPos() );

	Assert(pStart && pStop);
	Assert(ms_nSelectedList==SCENECOSTTRACKER_LIST_PVS);

	ResetThisFrame();

	// update scene budget tracker - filtered entities first
	// to ensure they are counted towards filtered total
	CGtaPvsEntry* pPvsEntry = pStart;
	if (FilteringResults())
	{
		while (pPvsEntry != pStop)
		{
			if (pPvsEntry)
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				if (pEntity && IsFilteredEntity(pEntity))
				{
					AddCostsForPvsEntry(pPvsEntry);
				}
			}
			pPvsEntry++;
		}
	}

	// now process everything
	pPvsEntry	= pStart;
	while (pPvsEntry != pStop)
	{
		if (pPvsEntry)
		{
			CEntity* pEntity = pPvsEntry->GetEntity();
			if (pEntity)
			{
				AddCostsForPvsEntry(pPvsEntry);
			}
		}
		pPvsEntry++;
	}

	if (ms_bWriteLog)
	{
		WriteToLog(ms_achLogFileName);
		ms_bWriteLog = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddCostsForSphere
// PURPOSE:		search a sphere around player pos and feeds memory costs into stats
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddCostsForSphere()
{
	Assert(ms_nSelectedList==SCENECOSTTRACKER_LIST_SPHERE);

	ResetThisFrame();

	atArray<CEntity*> entityArray;

	// perform world search
	CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

	ms_vSearchPos = VECTOR3_TO_VEC3V( vPlayerPos );

	s32 entityTypeFlags = ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT;

	spdSphere sphere(RCC_VEC3V(vPlayerPos), ScalarV((float)ms_nSearchRadius));
	fwIsSphereIntersectingVisible searchSphere(sphere);
	CGameWorld::ForAllEntitiesIntersecting
	(
		&searchSphere,
		AppendToListCB,
		(void*) &entityArray,
		entityTypeFlags,
		SEARCH_LOCATION_INTERIORS | SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_ALL,
		SEARCH_OPTION_FORCE_PPU_CODEPATH | SEARCH_OPTION_LARGE,
		WORLDREP_SEARCHMODULE_DEFAULT
	);

	// update scene budget tracker - filtered entities first
	// to ensure they are counted towards filtered total
	if (FilteringResults())
	{
		for (u32 i=0; i<entityArray.GetCount(); i++)
		{
			CEntity* pEntity = entityArray[i];
			if (pEntity && IsFilteredEntity(pEntity))
			{
				AddCostsForEntity(pEntity);
			}
		}
	}

	// now process everything
	for (u32 i=0; i<entityArray.GetCount(); i++)
	{
		CEntity* pEntity = entityArray[i];
		if (pEntity)
		{
			AddCostsForEntity(pEntity);
		}
	}

	if (ms_bIncludeZoneAssets)
	{
		CZonedAssetManager::Update( VECTOR3_TO_VEC3V(vPlayerPos) );
		atArray<strIndex> assets;
		CZonedAssetManager::GetZonedAssets(assets);

		for (s32 i=0; i<assets.GetCount(); i++)
		{
			AddCostsForZonedAsset(assets[i]);
		}
	}

	if (ms_bWriteLog)
	{
		WriteToLog(ms_achLogFileName);
		ms_bWriteLog = false;
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AppendToListCB
// PURPOSE:		callback for world search code to append a registered entity to list
//////////////////////////////////////////////////////////////////////////
bool CSceneCostTracker::AppendToListCB(CEntity* pEntity, void* pData)
{
	if (pEntity && pData && g_LodMgr.WithinVisibleRange(pEntity))
	{
		atArray<CEntity*>* pEntities = (atArray<CEntity*>*) pData;
		pEntities->PushAndGrow(pEntity);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SaveToLogCB
// PURPOSE:		set flag to write out report
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::SaveToLogCB()
{
	ms_bWriteLog = true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SelectLogFileCB
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::SelectLogFileCB()
{
	if (!BANKMGR.OpenFile(ms_achLogFileName, 256, "*.txt", true, "Cost tracker log (*.txt)"))
	{
		ms_achLogFileName[0] = '\0';
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddCostsForPvsEntry
// PURPOSE:		adds the cost of the specified pvs entry
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddCostsForPvsEntry(CGtaPvsEntry* pPvsEntry)
{
	if (!pPvsEntry || !pPvsEntry->GetEntity()) { return; }
	u32	phaseFlag;
	phaseFlag = 1 << g_SceneToGBufferPhaseNew->GetEntityListIndex();
	
	if (ms_bOnlyTrackGbuf && (pPvsEntry->GetVisFlags() & phaseFlag)) { return; }
	AddCostsForEntity(pPvsEntry->GetEntity());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddCostsForZonedAsset
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddCostsForZonedAsset(strIndex modelIndex)
{
	{
		u32 objIndex = fwArchetypeManager::GetStreamingModule()->GetObjectIndex(modelIndex).Get();
		fwModelId modelId((strLocalIndex(objIndex)));
		CBaseModelInfo* pModelInfo = (CBaseModelInfo*) fwArchetypeManager::GetArchetype( modelId );

		if (pModelInfo)
		{
			CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_UPDATE];

			const bool bFiltered = false;				// for now exclude zoned assets from filtering
			const u32 lodType = LODTYPES_DEPTH_HD;		// for now assume zoned assets are lodded HD

			s32 txdModuleId = g_TxdStore.GetStreamingModuleId();
			s32 drawableModuleId = g_DrawableStore.GetStreamingModuleId();
			s32 fragModuleId = g_FragmentStore.GetStreamingModuleId();
			s32 dwdModuleId = g_DwdStore.GetStreamingModuleId();
			s32 ptfxModuleId =  ptfxManager::GetAssetStore().GetStreamingModuleId();

			strStreamingModule* pTxdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(txdModuleId);
			strStreamingModule* pDrawableStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(drawableModuleId);
			strStreamingModule* pFragStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fragModuleId);
			strStreamingModule* pDwdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(dwdModuleId);
			strStreamingModule* pPtfxStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(ptfxModuleId);

			//////////////////////////////////////////////////////////////////////////
			// update txd costs
			strLocalIndex txdDictIndex = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());
			if (txdDictIndex.Get()!=-1)
			{
				u32 * const pResult = m_txdMap.Access(txdDictIndex.Get());
				if (!pResult)
				{
					// stat hasn't been counted yet
					strIndex txdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(txdDictIndex);
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(txdStreamingIndex);
					psStats->IncrementCost(LODTYPES_DEPTH_HD, SCENECOSTTRACKER_COST_LIST_TXD, info.ComputePhysicalSize(txdStreamingIndex, true), info.ComputeVirtualSize(txdStreamingIndex, true), false);
					m_txdMap.Insert(txdDictIndex.Get(), 1);
					m_streamingIndices.PushAndGrow(txdStreamingIndex);

					// check parent txd
					strLocalIndex parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(txdDictIndex));
					while (parentTxdIndex!=-1)
					{
						u32 * const pParentResult = m_txdMap.Access(parentTxdIndex.Get());
						if (!pParentResult && ms_bTrackParentTxds)
						{
							// stat hasn't been counted yet
							strIndex parentTxdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(parentTxdIndex);
							strStreamingInfo& parentInfo = *strStreamingEngine::GetInfo().GetStreamingInfo(parentTxdStreamingIndex);
							psStats->IncrementCost(lodType, SCENECOSTTRACKER_COST_LIST_TXD, parentInfo.ComputePhysicalSize(parentTxdStreamingIndex, true), parentInfo.ComputeVirtualSize(parentTxdStreamingIndex, true), bFiltered);
							m_txdMap.Insert(parentTxdIndex.Get(), 1);
							m_streamingIndices.PushAndGrow(parentTxdStreamingIndex);
						}
						parentTxdIndex = g_TxdStore.GetParentTxdSlot(parentTxdIndex);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

			//////////////////////////////////////////////////////////////////////////
			// update dwd costs
			strLocalIndex dwdDictIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLEDICTIONARY) ? strLocalIndex(pModelInfo->GetDrawDictIndex()) : strLocalIndex(INVALID_DRAWDICT_IDX);
			if (dwdDictIndex.Get()!=INVALID_DRAWDICT_IDX)
			{
				u32 * const pResult = m_dwdMap.Access(dwdDictIndex.Get());
				if (!pResult)
				{
					// stat hasn't been counted yet
					strIndex dwdStreamingIndex = pDwdStreamingModule->GetStreamingIndex(dwdDictIndex);
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(dwdStreamingIndex);
					psStats->IncrementCost(lodType, SCENECOSTTRACKER_COST_LIST_DWD, info.ComputePhysicalSize(dwdStreamingIndex, true), info.ComputeVirtualSize(dwdStreamingIndex, true), bFiltered);
					m_dwdMap.Insert(dwdDictIndex.Get(), 1);
					m_streamingIndices.PushAndGrow(dwdStreamingIndex);
				}
			}
			//////////////////////////////////////////////////////////////////////////

			//////////////////////////////////////////////////////////////////////////
			// update drawable costs
			strLocalIndex drawableIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE) ? strLocalIndex(pModelInfo->GetDrawableIndex()) : strLocalIndex(INVALID_DRAWABLE_IDX);
			if (drawableIndex.Get()!=INVALID_DRAWABLE_IDX)
			{
				u32 * const pResult = m_drawableMap.Access(drawableIndex.Get());
				if (!pResult)
				{
					// stat hasn't been counted yet
					strIndex drawableStreamingIndex = pDrawableStreamingModule->GetStreamingIndex(drawableIndex);
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(drawableStreamingIndex);
					psStats->IncrementCost(lodType, SCENECOSTTRACKER_COST_LIST_DRAWABLE, info.ComputePhysicalSize(drawableStreamingIndex, true), info.ComputeVirtualSize(drawableStreamingIndex, true), bFiltered);
					m_drawableMap.Insert(drawableIndex.Get(), 1);
					m_streamingIndices.PushAndGrow(drawableStreamingIndex);
				}
			}
			//////////////////////////////////////////////////////////////////////////

			//////////////////////////////////////////////////////////////////////////
			// update frag costs
			strLocalIndex fragIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_FRAGMENT) ? strLocalIndex(pModelInfo->GetFragmentIndex()) : strLocalIndex(INVALID_FRAG_IDX);
			if (fragIndex.Get()!=INVALID_FRAG_IDX)
			{
				u32 * const pResult = m_fragMap.Access(fragIndex.Get());
				if (!pResult)
				{
					// stat hasn't been counted yet
					strIndex fragStreamingIndex = pFragStreamingModule->GetStreamingIndex(fragIndex);
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(fragStreamingIndex);
					psStats->IncrementCost(lodType, SCENECOSTTRACKER_COST_LIST_FRAG, info.ComputePhysicalSize(fragStreamingIndex, true), info.ComputeVirtualSize(fragStreamingIndex, true), bFiltered);
					m_fragMap.Insert(fragIndex.Get(), 1);
					m_streamingIndices.PushAndGrow(fragStreamingIndex);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// particle fx asset costs
			strLocalIndex ptfxAssetIndex = strLocalIndex(pModelInfo->GetPtFxAssetSlot());
			if (ptfxAssetIndex.Get() != -1)
			{
				u32 * const pResult = m_ptfxMap.Access(ptfxAssetIndex.Get());
				if (!pResult)
				{
					// stat hasn't been counted yet
					strIndex ptfxAssetStreamingIndex = pPtfxStreamingModule->GetStreamingIndex(ptfxAssetIndex);
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(ptfxAssetStreamingIndex);
					psStats->IncrementCost(lodType, SCENECOSTTRACKER_COST_LIST_PTFX, info.ComputePhysicalSize(ptfxAssetStreamingIndex, true), info.ComputeVirtualSize(ptfxAssetStreamingIndex, true), bFiltered);
					m_ptfxMap.Insert(ptfxAssetIndex.Get(), 1);
					m_streamingIndices.PushAndGrow(ptfxAssetStreamingIndex);
				}
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddCostsForEntity
// PURPOSE:		adds the cost of the specified entity
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::AddCostsForEntity(CEntity* pEntity)
{
	CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_UPDATE];

	if (pEntity)
	{
		if (ms_bOnlyTrackMapData && (pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle()))
		{
			return;
		}

		if (ms_bOnlyTrackNonzeroAlpha && !pEntity->GetAlpha())
		{
			return;
		}
		bool bFiltered = IsFilteredEntity(pEntity);

		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if (ms_nSelectedList==SCENECOSTTRACKER_LIST_PSS_NOTLOADED && pModelInfo->GetHasLoaded())
		{
			return;
		}

		s32 txdModuleId = g_TxdStore.GetStreamingModuleId();
		s32 drawableModuleId = g_DrawableStore.GetStreamingModuleId();
		s32 fragModuleId = g_FragmentStore.GetStreamingModuleId();
		s32 dwdModuleId = g_DwdStore.GetStreamingModuleId();
		s32 ptfxModuleId =  ptfxManager::GetAssetStore().GetStreamingModuleId();

		strStreamingModule* pTxdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(txdModuleId);
		strStreamingModule* pDrawableStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(drawableModuleId);
		strStreamingModule* pFragStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fragModuleId);
		strStreamingModule* pDwdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(dwdModuleId);
		strStreamingModule* pPtfxStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(ptfxModuleId);

		//////////////////////////////////////////////////////////////////////////
		// update txd costs
		strLocalIndex txdDictIndex = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());
		if (txdDictIndex.Get()!=-1)
		{
			u32 * const pResult = m_txdMap.Access(txdDictIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex txdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(txdDictIndex);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(txdStreamingIndex);
				psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_TXD, info.ComputePhysicalSize(txdStreamingIndex, true), info.ComputeVirtualSize(txdStreamingIndex, true), bFiltered);
				m_txdMap.Insert(txdDictIndex.Get(), 1);
				m_streamingIndices.PushAndGrow(txdStreamingIndex);

				// check parent txd
				strLocalIndex parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(txdDictIndex));
				while (parentTxdIndex!=-1)
				{
					u32 * const pParentResult = m_txdMap.Access(parentTxdIndex.Get());
					if (!pParentResult && ms_bTrackParentTxds)
					{
						// stat hasn't been counted yet
						strIndex parentTxdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(parentTxdIndex);
						strStreamingInfo& parentInfo = *strStreamingEngine::GetInfo().GetStreamingInfo(parentTxdStreamingIndex);
						psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_TXD, parentInfo.ComputePhysicalSize(parentTxdStreamingIndex, true), parentInfo.ComputeVirtualSize(parentTxdStreamingIndex, true), bFiltered);
						m_txdMap.Insert(parentTxdIndex.Get(), 1);
						m_streamingIndices.PushAndGrow(parentTxdStreamingIndex);
					}
					parentTxdIndex = g_TxdStore.GetParentTxdSlot(parentTxdIndex);
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update dwd costs
		strLocalIndex dwdDictIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLEDICTIONARY) ? strLocalIndex(pModelInfo->GetDrawDictIndex()) : strLocalIndex(INVALID_DRAWDICT_IDX);
		if (dwdDictIndex.Get()!=INVALID_DRAWDICT_IDX)
		{
			u32 * const pResult = m_dwdMap.Access(dwdDictIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex dwdStreamingIndex = pDwdStreamingModule->GetStreamingIndex(dwdDictIndex);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(dwdStreamingIndex);
				psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_DWD, info.ComputePhysicalSize(dwdStreamingIndex, true), info.ComputeVirtualSize(dwdStreamingIndex, true), bFiltered);
				m_dwdMap.Insert(dwdDictIndex.Get(), 1);
				m_streamingIndices.PushAndGrow(dwdStreamingIndex);
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update drawable costs
		strLocalIndex drawableIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE) ? strLocalIndex(pModelInfo->GetDrawableIndex()) : strLocalIndex(INVALID_DRAWABLE_IDX);
		if (drawableIndex.Get()!=INVALID_DRAWABLE_IDX)
		{
			u32 * const pResult = m_drawableMap.Access(drawableIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex drawableStreamingIndex = pDrawableStreamingModule->GetStreamingIndex(drawableIndex);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(drawableStreamingIndex);
				psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_DRAWABLE, info.ComputePhysicalSize(drawableStreamingIndex, true), info.ComputeVirtualSize(drawableStreamingIndex, true), bFiltered);
				m_drawableMap.Insert(drawableIndex.Get(), 1);
				m_streamingIndices.PushAndGrow(drawableStreamingIndex);
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update frag costs
		strLocalIndex fragIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_FRAGMENT) ? strLocalIndex(pModelInfo->GetFragmentIndex()) : strLocalIndex(INVALID_FRAG_IDX);
		if (fragIndex.Get()!=INVALID_FRAG_IDX)
		{
			u32 * const pResult = m_fragMap.Access(fragIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex fragStreamingIndex = pFragStreamingModule->GetStreamingIndex(fragIndex);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(fragStreamingIndex);
				psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_FRAG, info.ComputePhysicalSize(fragStreamingIndex, true), info.ComputeVirtualSize(fragStreamingIndex, true), bFiltered);
				m_fragMap.Insert(fragIndex.Get(), 1);
				m_streamingIndices.PushAndGrow(fragStreamingIndex);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// particle fx asset costs
		strLocalIndex ptfxAssetIndex = strLocalIndex(pModelInfo->GetPtFxAssetSlot());
		if (ptfxAssetIndex.Get() != -1)
		{
			u32 * const pResult = m_ptfxMap.Access(ptfxAssetIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex ptfxAssetStreamingIndex = pPtfxStreamingModule->GetStreamingIndex(ptfxAssetIndex);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(ptfxAssetStreamingIndex);
				psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_PTFX, info.ComputePhysicalSize(ptfxAssetStreamingIndex, true), info.ComputeVirtualSize(ptfxAssetStreamingIndex, true), bFiltered);
				m_ptfxMap.Insert(ptfxAssetIndex.Get(), 1);
				m_streamingIndices.PushAndGrow(ptfxAssetStreamingIndex);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// HD texlod assets
		if (ms_bIncludeHdTxdPromotion && pModelInfo->GetIsHDTxdCapable())
		{
			const ScalarV highDetailDist( pModelInfo->GetHDTextureDistance() );
			const Vec3V vEntityPos = pEntity->GetTransform().GetPosition();
			const ScalarV distanceToEntity = Dist(ms_vSearchPos, vEntityPos);
			
			if (IsLessThanOrEqualAll(distanceToEntity, highDetailDist))
			{
				strLocalIndex hdTxdSlot = CTexLod::GetHDTxdSlotFromMapping(pModelInfo);
				if (hdTxdSlot.Get() != -1)
				{
					// get the size of the TXD if it hasn't already been counted, and add it to the total costs for TXDSTORE
					u32 * const pResult = m_txdMap.Access(hdTxdSlot.Get());
					if (!pResult)
					{
						strIndex hdTxdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(hdTxdSlot);
						strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(hdTxdStreamingIndex);
						psStats->IncrementCost(pEntity->GetLodData().GetLodType(), SCENECOSTTRACKER_COST_LIST_TXD, info.ComputePhysicalSize(hdTxdStreamingIndex, true), info.ComputeVirtualSize(hdTxdStreamingIndex, true), bFiltered);
						m_txdMap.Insert(hdTxdSlot.Get(), 1);
						m_streamingIndices.PushAndGrow(hdTxdStreamingIndex);
					}
				} 
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// update count of shaders
		rmcDrawable* pDrawable = pModelInfo->GetDrawable();
		if (pDrawable && pEntity->GetDrawHandlerPtr()!=NULL)
		{
			u32 shadercount = GetShaderCount(pEntity,pDrawable);
			psStats->IncrementCount(SCENECOSTTRACKER_COUNTS_SHADERS, shadercount, bFiltered);
		}
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetShaderCount
// PURPOSE:		Try to get a better shader count number for a given drawable.
//////////////////////////////////////////////////////////////////////////
u32 CSceneCostTracker::GetShaderCount(CEntity* pEntity, rmcDrawable *pDrawable)
{
	u32 shadercount = 0;

	if( pDrawable->GetLodGroup().ContainsLod(LOD_HIGH) )
	{
		u32 LODLevel = 0;
		s32 crossFade = -1;
		float dist = LODDrawable::CalcLODDistance(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
		LODDrawable::crossFadeDistanceIdx fadeDist = pEntity->GetUseAltFadeDistance() ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;
		LODDrawable::CalcLODLevelAndCrossfade(pDrawable, dist, LODLevel, crossFade, fadeDist, pEntity->GetLastLODIdx() ASSERT_ONLY(,pEntity->GetBaseModelInfo()));

		if (crossFade > -1)
		{
			Assert(LODLevel+1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel+1));

			u32 fadeAlphaN1 = crossFade;
			u32 fadeAlphaN2 = 255 - crossFade;

			if (fadeAlphaN1 > 0)
			{
				const rmcLod &lod = pDrawable->GetLodGroup().GetLod(LODLevel);
				u32 modelCount = lod.GetCount();

				for(int i=0;i<modelCount;i++)
				{
					shadercount += lod.GetModel(i)->GetGeometryCount();
				}
			}

			if (fadeAlphaN2 > 0)
			{
				const rmcLod &lod = pDrawable->GetLodGroup().GetLod(LODLevel+1);	
				u32 modelCount = lod.GetCount();

				for(int i=0;i<modelCount;i++)
				{
					shadercount += lod.GetModel(i)->GetGeometryCount();
				}
			}
		}
		else if (pDrawable->GetLodGroup().ContainsLod(LODLevel))
		{
			const rmcLod &lod = pDrawable->GetLodGroup().GetLod(LODLevel);
			u32 modelCount = lod.GetCount();

			for(int i=0;i<modelCount;i++)
			{
				shadercount += lod.GetModel(i)->GetGeometryCount();
			}
		}
	}

	return shadercount;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		handle mouse dragging etc
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F5, KEYBOARD_MODE_DEBUG_ALT, "New junction memory test"))
	{
		ms_vPos.Set(700, 100);	// reset pos of tracker in case it hasn't been initialised
		g_SceneMemoryCheck.Start();
	}

	g_SceneMemoryCheck.Update();

	if (!ms_bShowBudgets)  {return;}

	// copy dbl buffer stat vals
	sysMemCpy
	(
		&m_sStatBuffers[SCENECOSTTRACKER_BUFFER_RENDER],
		&m_sStatBuffers[SCENECOSTTRACKER_BUFFER_UPDATE],
		sizeof(CSceneCostTrackerStatsBuffer)
	);

	// handle mouse click
	const Vector2 mousePosition = Vector2( static_cast<float>(ioMouse::GetX()), static_cast<float>(ioMouse::GetY()) );

	if (ms_bMove)
	{
		// already dragging so adjust position of results box
		ms_vPos = mousePosition - ms_vClickDragDelta;
		if (ms_vPos.x<0.0f)
		{
			ms_vPos.Set(0.0f, ms_vPos.y);
			ms_vClickDragDelta = mousePosition - ms_vPos;
		}
		if (ms_vPos.y<0.0f)
		{
			ms_vPos.Set(0.0f, ms_vPos.y);
			ms_vClickDragDelta = mousePosition - ms_vPos;
		}

		// check for button release
		if ((ioMouse::GetButtons()&ioMouse::MOUSE_LEFT) == false)
		{
			ms_bMove = false;
		}
	}
	else
	{
		// check for player clicking on title bar
		if (ioMouse::GetButtons()&ioMouse::MOUSE_LEFT)
		{
			fwRect rect
				(
				ms_vPos.x,
				ms_vPos.y,
				ms_vPos.x + ms_nWidth,
				ms_vPos.y + ms_nNumEntries*(TITLE_H+ms_nHeight)
				);

			if (rect.IsInside(mousePosition))
			{
				ms_bMove = true;
				ms_vClickDragDelta = mousePosition - ms_vPos;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetTitleSt
// PURPOSE:		fills a string for the title of stat box
//////////////////////////////////////////////////////////////////////////
s32 CSceneCostTracker::GetCostTitleStr(char* pszResult, const char* pszTitle, u32 eStat, u32 eMemType)
{
	CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_RENDER];

	switch (eMemType)
	{
	case SCENECOSTTRACKER_MEMTYPE_PHYS:
		sprintf(
			pszResult,
			"%s curr %dK peak %dK filt %dK",
			pszTitle,
			psStats->GetPhysCost(eStat) >>10,
			psStats->GetPhysCostPeak(eStat) >>10,
			psStats->GetPhysCostFiltered(eStat) >>10 
		);
		return psStats->GetPhysCost(eStat) >>10;
	case SCENECOSTTRACKER_MEMTYPE_VIRT:
		sprintf(
			pszResult,
			"%s curr %dK peak %dK filt %dK",
			pszTitle,
			psStats->GetVirtCost(eStat) >>10,
			psStats->GetVirtCostPeak(eStat) >>10,
			psStats->GetVirtCostFiltered(eStat) >>10 
		);
		return psStats->GetVirtCost(eStat) >>10;
	default:
		return 0;
	}
}

u32 CSceneCostTracker::GetTotalCost(bool bVram)
{
	CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_RENDER];

	if (bVram)
	{
		return psStats->GetPhysCost(SCENECOSTTRACKER_COST_LIST_TOTAL);
	}
	return psStats->GetVirtCost(SCENECOSTTRACKER_COST_LIST_TOTAL);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawDebug
// PURPOSE:		draw some debug charts on screen to illustrate scene cost
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawDebug()
{
	if (ms_bShowBudgets)
	{
		char achTitle[200];

		u32 x = (u32)ms_vPos.x;
		u32 y = (u32)ms_vPos.y;

		CSceneCostTrackerStatsBuffer* psStats = &m_sStatBuffers[SCENECOSTTRACKER_BUFFER_RENDER];

		// draw filtered-by title
		DrawFilteredByMsg(x, y-10);

		ms_nNumEntries = 0;
		if (ms_bPhysical)
		{
			if (ms_bShowSceneCostFromList_Total)
			{
				GetCostTitleStr(achTitle, "VRAM (Total):", SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_BARTYPE_IMPORTANT);
				ms_nNumEntries++;

				if (ms_bShowLodBreakdown)
				{
					DrawLodBreakdown(x, y-ms_nHeight, psStats->m_anPerLodTotalPhys, (float)ms_nMaxMemPhysMB*1024);
				}
			}
			if (ms_bShowSceneCostFromList_Txds)
			{
				GetCostTitleStr(achTitle, "VRAM (Txd):", SCENECOSTTRACKER_COST_LIST_TXD, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_TXD, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Dwds)
			{
				GetCostTitleStr(achTitle, "VRAM (Dwd):", SCENECOSTTRACKER_COST_LIST_DWD, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_DWD, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Drawables)
			{
				GetCostTitleStr(achTitle, "VRAM (Drawable):", SCENECOSTTRACKER_COST_LIST_DRAWABLE, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_DRAWABLE, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Frags)
			{
				GetCostTitleStr(achTitle, "VRAM (Frag):", SCENECOSTTRACKER_COST_LIST_FRAG, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_FRAG, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Ptfx)
			{
				GetCostTitleStr(achTitle, "VRAM (Ptfx):", SCENECOSTTRACKER_COST_LIST_PTFX, SCENECOSTTRACKER_MEMTYPE_PHYS);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_PHYS(x, y, SCENECOSTTRACKER_COST_LIST_PTFX, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
		}

		if (ms_bVirtual)
		{
			if (ms_bShowSceneCostFromList_Total)
			{
				GetCostTitleStr(achTitle, "MAIN (Total):", SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_BARTYPE_IMPORTANT);
				ms_nNumEntries++;

				if (ms_bShowLodBreakdown)
				{
					DrawLodBreakdown(x, y-ms_nHeight, psStats->m_anPerLodTotalVirt, (float)ms_nMaxMemVirtMB*1024);
				}
			}
			if (ms_bShowSceneCostFromList_Txds)
			{
				GetCostTitleStr(achTitle, "MAIN (Txd):", SCENECOSTTRACKER_COST_LIST_TXD, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_TXD, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Dwds)
			{
				GetCostTitleStr(achTitle, "MAIN (Dwd):", SCENECOSTTRACKER_COST_LIST_DWD, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_DWD, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Drawables)
			{
				GetCostTitleStr(achTitle, "MAIN (Drawable):", SCENECOSTTRACKER_COST_LIST_DRAWABLE, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_DRAWABLE, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Frags)
			{
				GetCostTitleStr(achTitle, "MAIN (Frag):", SCENECOSTTRACKER_COST_LIST_FRAG, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_FRAG, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
			if (ms_bShowSceneCostFromList_Ptfx)
			{
				GetCostTitleStr(achTitle, "MAIN (Ptfx):", SCENECOSTTRACKER_COST_LIST_PTFX, SCENECOSTTRACKER_MEMTYPE_VIRT);
				DrawTitle(&x, &y, achTitle, ms_nWidth);
				DRAW_BAR_VIRT(x, y, SCENECOSTTRACKER_COST_LIST_PTFX, SCENECOSTTRACKER_BARTYPE_REGULAR);
				ms_nNumEntries++;
			}
		}

		if (ms_bShowSceneNumShaders)
		{
			sprintf(achTitle, "Total num shaders: curr %d peak %d filt %d",
				psStats->GetCount(SCENECOSTTRACKER_COUNTS_SHADERS),
				psStats->GetCountPeak(SCENECOSTTRACKER_COUNTS_SHADERS),
				psStats->GetCountFiltered(SCENECOSTTRACKER_COUNTS_SHADERS)
			);
			DrawTitle(&x, &y, achTitle, ms_nWidth);
			DrawCountBar(x, y, psStats->GetCount(SCENECOSTTRACKER_COUNTS_SHADERS), psStats->GetCountFiltered(SCENECOSTTRACKER_COUNTS_SHADERS), ms_nMaxNumShaders, SCENECOSTTRACKER_BARTYPE_REGULAR);
			ms_nNumEntries++;
		}

		if (ms_bShowLodBreakdown)
		{
			u32 legendX = (u32) ms_vPos.x + ms_nWidth + 5;
			u32 legendY = (u32) ms_vPos.y;
			DrawLegend(legendX, legendY);
		}

		CText::Flush();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawFilteredByMsg
// PURPOSE:		draws a msg onscreen informing user what they are filtering stats by
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawFilteredByMsg(u32 x, u32 y)
{
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.4f, 0.4f));

	char achMsg[200];
	switch (ms_nSelectedList)
	{
	case SCENECOSTTRACKER_LIST_PVS:
		sprintf(achMsg, "Src: Visibility");
		break;
	case SCENECOSTTRACKER_LIST_PSS_ALL:
		sprintf(achMsg, "Src: Scene streamer");
		break;
	case SCENECOSTTRACKER_LIST_PSS_NOTLOADED:
		sprintf(achMsg, "Src: Scene streamer (unloaded)");
		break;
	case SCENECOSTTRACKER_LIST_SPHERE:
		sprintf(achMsg, "Src: Sphere");
		break;
	default:
		break;
	}

	if (ms_bFilterResults)
	{
		strcat(achMsg, " / Filt  by: ");
		strcat(achMsg, apszFilterNames[ms_nSelectedFilter]);
	}
	
	DebugTextLayout.SetColor(Color32(0, 255, 255, 255));
	DebugTextLayout.Render(SCREEN_COORDS(x, y-20), achMsg);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawTitle
// PURPOSE:		draws a bit of text in a box, and increases *y
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawTitle(u32* x, u32* y, const char* pszText, u32 width)
{
	fwRect titleBoxRect
	(
		(float) *x,				// left
		(float) *y + TITLE_H,	// bottom
		(float) *x + width,		// right
		(float) *y				// top
	);
	Color32 titleBoxColour(0, 0, 0, 128);
	CSprite2d::DrawRectSlow(titleBoxRect, titleBoxColour);

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));

	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
	DebugTextLayout.Render(SCREEN_COORDS(*x, *y), pszText);

	*y += 20;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawMemBar
// PURPOSE:		draws a memory bar on screen, changing smoothly between values
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawMemBar(u32& x, u32& y, const u32 nStat, const u32 UNUSED_PARAM(nPeak), const u32 nFilteredStat, const u32 nMax, const u32 eBarType)
{
	bool bLessImportant = (eBarType == SCENECOSTTRACKER_BARTYPE_REGULAR);

	// draw bg box
	fwRect bgBoxRect
	(
		(float) x,					// left
		(float) y + ms_nHeight,		// bottom
		(float) x + ms_nWidth,		// right
		(float) y					// top
	);
	Color32 bgBoxColour(0, 0, 0, 128);
	CSprite2d::DrawRectSlow(bgBoxRect, bgBoxColour);

	// max dimensions of stat bar
	float fBorder = 2.0f;
	float fBarH = (float)ms_nHeight - 2 * fBorder;
	float fMaxBarW = (float) ms_nWidth - 2 * fBorder;

	// figure out target length of bar
	float fBarW = ((fMaxBarW * (float)(nStat>>10)) / (float)nMax);
	fBarW = Min(fBarW, fMaxBarW);

	fwRect statBoxRect
	(
		x + fBorder,			// left
		y + fBorder + fBarH,	// bottom
		x + fBorder + fBarW,	// right
		y + fBorder				// top
	);
	Color32 statBoxColour(0, 255, 0, 255);
	if (fBarW == fMaxBarW) { statBoxColour.Set(255, 0, 0, 255); }
	statBoxColour.SetAlpha(bLessImportant ? 75 : 255);

	if (nStat)
	{
		CSprite2d::DrawRectSlow(statBoxRect, statBoxColour);
	}

	if (nFilteredStat && ms_bFilterResults)
	{
		float fFilteredBarW = ((fMaxBarW * (float)(nFilteredStat>>10)) / (float)nMax);
		fFilteredBarW = Min(fFilteredBarW, fMaxBarW);
		fwRect filteredBoxRect
		(
			x + fBorder,					// left
			y + fBorder + fBarH,			// bottom
			x + fBorder + fFilteredBarW,	// right
			y + fBorder						// top
		);
		Color32 filteredBoxColour(255, 255, 255, 150);
		CSprite2d::DrawRectSlow(filteredBoxRect, filteredBoxColour);
	}

	// draw text with stat info
// 	char achTmp[100];
// 	u32 numX = x + ms_nWidth;
// 	u32 numY = y;
// 	sprintf(achTmp, "%dK (peak %dK, filt %dK)", nStat>>10, nPeak>>10, nFilteredStat>>10);
// 	DrawTitle(&numX, &numY, achTmp, 210);

	// draw measure markers
	u32 nMaxMB = nMax/1024;
	if (nMaxMB>1)
	{
		if (nMaxMB<=20)
		{
			// draw marker for each mb
			float fDiv = fMaxBarW / nMaxMB;
			for (u32 i=0; i<nMaxMB+1; i++)
			{
				Vector2 p1(SCREEN_COORDS(x+fBorder+i*fDiv, y+fBorder+fBarH));
				Vector2 p2(SCREEN_COORDS(x+fBorder+i*fDiv, y+fBorder+fBarH/2));
				grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
			}
		}
		else
		{
			// draw marker for every 10mb
			float fDiv = fMaxBarW / nMaxMB;
			for (u32 i=0; i<(nMaxMB/10)+1; i++)
			{
				Vector2 p1(SCREEN_COORDS(x+fBorder+i*fDiv*10, y+fBorder+fBarH));
				Vector2 p2(SCREEN_COORDS(x+fBorder+i*fDiv*10, y+fBorder+fBarH/2));
				grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
			}
		}
	}
	Vector2 p1(SCREEN_COORDS(x+fBorder,				y+fBorder+fBarH));
	Vector2 p2(SCREEN_COORDS(x+fBorder+fMaxBarW,	y+fBorder+fBarH));
	grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));

	// increment y for next stat
	y += ms_nHeight;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawCountBar
// PURPOSE:		draws a counter bar on screen, changing smoothly between values
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawCountBar(u32& x, u32& y, const u32 nStat, const u32 nFilteredStat, const u32 nMax, const u32 eBarType)
{
	bool bLessImportant = (eBarType == SCENECOSTTRACKER_BARTYPE_REGULAR);

	// draw bg box
	fwRect bgBoxRect
	(
		(float) x,					// left
		(float) y + ms_nHeight,		// bottom
		(float) x + ms_nWidth,		// right
		(float) y					// top
	);
	Color32 bgBoxColour(0, 0, 0, 128);
	CSprite2d::DrawRectSlow(bgBoxRect, bgBoxColour);

	// max dimensions of stat bar
	float fBorder = 2.0f;
	float fBarH = (float)ms_nHeight - 2 * fBorder;
	float fMaxBarW = (float) ms_nWidth - 2 * fBorder;

	// figure out target length of bar
	float fBarW = ((fMaxBarW * (float)(nStat)) / (float)nMax);
	fBarW = Min(fBarW, fMaxBarW);

	fwRect statBoxRect
		(
		x + fBorder,			// left
		y + fBorder + fBarH,	// bottom
		x + fBorder + fBarW,	// right
		y + fBorder				// top
		);
	Color32 statBoxColour(0, 255, 0, 255);
	if (fBarW == fMaxBarW) { statBoxColour.Set(255, 0, 0, 255); }
	statBoxColour.SetAlpha(bLessImportant ? 75 : 255);
	if (nStat)
	{
		CSprite2d::DrawRectSlow(statBoxRect, statBoxColour);
	}

	if (nFilteredStat && ms_bFilterResults)
	{
		float fFilteredBarW = ((fMaxBarW * (float)(nFilteredStat)) / (float)nMax);
		fFilteredBarW = Min(fFilteredBarW, fMaxBarW);

		fwRect filteredBoxRect
			(
			x + fBorder,					// left
			y + fBorder + fBarH,			// bottom
			x + fBorder + fFilteredBarW,	// right
			y + fBorder						// top
			);
		Color32 filteredBoxColour(255, 255, 255, 150);
		CSprite2d::DrawRectSlow(filteredBoxRect, filteredBoxColour);
	}

	// draw measure markers
	u32 nMaxMB = nMax/1024;
	if (nMaxMB>1)
	{
		if (nMaxMB<=20)
		{
			// draw marker for each mb
			float fDiv = fMaxBarW / nMaxMB;
			for (u32 i=0; i<nMaxMB+1; i++)
			{
				Vector2 p1(SCREEN_COORDS(x+fBorder+i*fDiv, y+fBorder+fBarH));
				Vector2 p2(SCREEN_COORDS(x+fBorder+i*fDiv, y+fBorder+fBarH/2));
				grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
			}
		}
		else
		{
			// draw marker for every 10mb
			float fDiv = fMaxBarW / nMaxMB;
			for (u32 i=0; i<(nMaxMB/10)+1; i++)
			{
				Vector2 p1(SCREEN_COORDS(x+fBorder+i*fDiv*10, y+fBorder+fBarH));
				Vector2 p2(SCREEN_COORDS(x+fBorder+i*fDiv*10, y+fBorder+fBarH/2));
				grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
			}
		}
	}
	Vector2 p1(SCREEN_COORDS(x+fBorder,				y+fBorder+fBarH));
	Vector2 p2(SCREEN_COORDS(x+fBorder+fMaxBarW,	y+fBorder+fBarH));
	grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));

	// increment y for next stat
	y += ms_nHeight;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawLodBreakdown
// PURPOSE:		print a multicolor breakdown for each lod level
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawLodBreakdown(u32 x, u32 y, u32* pData, float fMaxMem)
{
	const u32 lodLevels[] = {
		LODTYPES_DEPTH_ORPHANHD,
		LODTYPES_DEPTH_HD,
		LODTYPES_DEPTH_LOD,
		LODTYPES_DEPTH_SLOD1,
		LODTYPES_DEPTH_SLOD2,
		LODTYPES_DEPTH_SLOD3,
		LODTYPES_DEPTH_SLOD4,
	};
	CompileTimeAssert(NELEM(lodLevels) >= (LODTYPES_MAX_NUM_LEVELS + 1)); // otherwise loop below will access invalid memory

	const float fBorder = 2.0f;
	const float fBarH = 5.0f;
	const float fMaxBarW = (float) ms_nWidth - 2.0f * fBorder;

	float fScreenX = x + fBorder;
	float fScreenY = y + fBorder;

	for (u32 i=0; i<=LODTYPES_MAX_NUM_LEVELS; i++)
	{
		u32 level = lodLevels[i];
		Color32 boxColor = aLodBreakdownColors[level];
		u32 nStat = pData[level];

		// figure out target length of bar
		float fBarW = ((fMaxBarW * (float)(nStat>>10)) / (float)fMaxMem);

		fwRect rect
		(
			fScreenX,				// left
			fScreenY + fBarH,		// bottom
			fScreenX + fBarW,		// right
			fScreenY				// top
		);
		CSprite2d::DrawRectSlow(rect, boxColor);
		fScreenX += fBarW;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawLegend
// PURPOSE:		for lod breakdown overlay, draw guide to colors
//////////////////////////////////////////////////////////////////////////
void CSceneCostTracker::DrawLegend(u32 x, u32 y)
{
	const float fBoxW = 10.0f;
	const u32 lodLevels[] = {
		LODTYPES_DEPTH_ORPHANHD,
		LODTYPES_DEPTH_HD,
		LODTYPES_DEPTH_LOD,
		LODTYPES_DEPTH_SLOD1,
		LODTYPES_DEPTH_SLOD2,
		LODTYPES_DEPTH_SLOD3,
		LODTYPES_DEPTH_SLOD4,
	};
	CompileTimeAssert(NELEM(lodLevels) >= (LODTYPES_MAX_NUM_LEVELS + 1)); // otherwise loop below will access invalid memory

	for (u32 i=0; i<=LODTYPES_MAX_NUM_LEVELS; i++)
	{
		u32 level = lodLevels[i];
		Color32 boxColor = aLodBreakdownColors[level];
		const char* label = fwLodData::ms_apszLevelNames[level];

		float fScreenX = (float)x;
		float fScreenY = y + i * 20.0f;

		fwRect boxRect
		(
			fScreenX,				// left
			fScreenY + fBoxW,		// bottom
			fScreenX + fBoxW,		// right
			fScreenY				// top
		);
		CSprite2d::DrawRectSlow(boxRect, boxColor);

		CTextLayout DebugTextLayout;
		DebugTextLayout.SetScale(Vector2(0.2f, 0.2f));
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(SCREEN_COORDS(fScreenX+10.0f, fScreenY-5.0f), label);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		clear stats for buffer
//////////////////////////////////////////////////////////////////////////
void CSceneCostTrackerStatsBuffer::Reset()
{
	for (u32 i=0; i<SCENECOSTTRACKER_COST_TOTAL; i++)
	{
		m_anCostVirt[i] = 0;
		m_anCostPhys[i] = 0;
		m_anCostVirtFiltered[i] = 0;
		m_anCostPhysFiltered[i] = 0;
	}

	for (u32 i=0; i<SCENECOSTTRACKER_COUNTS_TOTAL; i++)
	{
		m_anCount[i] = 0;
		m_anCountFiltered[i] = 0;
	}

	for (u32 i=0; i<=LODTYPES_MAX_NUM_LEVELS; i++)
	{
		m_anPerLodTotalVirt[i] = 0;
		m_anPerLodTotalPhys[i] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IncrementCost
// PURPOSE:		increases a stat
//////////////////////////////////////////////////////////////////////////
void CSceneCostTrackerStatsBuffer::IncrementCost(u32 lodType, u32 eCost, u32 nPhysical, u32 nVirtual, bool bFiltered)
{
	// update specified stat
	m_anCostVirt[eCost] += nVirtual;
	m_anCostPhys[eCost] += nPhysical;

	if (bFiltered)
	{
		m_anCostVirtFiltered[eCost] += nVirtual;
		m_anCostPhysFiltered[eCost] += nPhysical;
	}

	switch (eCost)
	{
	case SCENECOSTTRACKER_COST_LIST_TXD:
	case SCENECOSTTRACKER_COST_LIST_DRAWABLE:
	case SCENECOSTTRACKER_COST_LIST_FRAG:
	case SCENECOSTTRACKER_COST_LIST_DWD:
	case SCENECOSTTRACKER_COST_LIST_PTFX:
		// update list total
		m_anCostVirt[SCENECOSTTRACKER_COST_LIST_TOTAL] += nVirtual;
		m_anCostPhys[SCENECOSTTRACKER_COST_LIST_TOTAL] += nPhysical;

		m_anPerLodTotalVirt[lodType] += nVirtual;
		m_anPerLodTotalPhys[lodType] += nPhysical;

		if (bFiltered)
		{
			m_anCostVirtFiltered[SCENECOSTTRACKER_COST_LIST_TOTAL] += nVirtual;
			m_anCostPhysFiltered[SCENECOSTTRACKER_COST_LIST_TOTAL] += nPhysical;
		}
		break;
	default:
		break;
	}

	// update peak stats
	if (GetVirtCost(eCost) > GetVirtCostPeak(eCost))
	{
		m_anCostVirtPeak[eCost] = GetVirtCost(eCost);
	}
	if (GetPhysCost(eCost) > GetPhysCostPeak(eCost))
	{
		m_anCostPhysPeak[eCost] = GetPhysCost(eCost);
	}
	if (GetVirtCost(SCENECOSTTRACKER_COST_LIST_TOTAL) > GetVirtCostPeak(SCENECOSTTRACKER_COST_LIST_TOTAL))
	{
		m_anCostVirtPeak[SCENECOSTTRACKER_COST_LIST_TOTAL] = GetVirtCost(SCENECOSTTRACKER_COST_LIST_TOTAL);
	}
	if (GetPhysCost(SCENECOSTTRACKER_COST_LIST_TOTAL) > GetPhysCostPeak(SCENECOSTTRACKER_COST_LIST_TOTAL))
	{
		m_anCostPhysPeak[SCENECOSTTRACKER_COST_LIST_TOTAL] = GetPhysCost(SCENECOSTTRACKER_COST_LIST_TOTAL);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IncrementCount
// PURPOSE:		increase counter
//////////////////////////////////////////////////////////////////////////
void CSceneCostTrackerStatsBuffer::IncrementCount(u32 eCount, u32 nNum, bool bFiltered)
{
	m_anCount[eCount] += nNum;
	
	if (bFiltered)
	{
		m_anCountFiltered[eCount] += nNum;
	}

	if (GetCount(eCount) > GetCountPeak(eCount))
	{
		m_anCountPeak[eCount] = GetCount(eCount);
	}
}

void CSceneCostTracker::SetJunctionCaptureMode(bool bEnabled)
{
	ms_bShowBudgets = bEnabled;

	if (bEnabled)
	{
		ms_bShowSceneCostFromList_Total = true;
		ms_bShowSceneCostFromList_Txds = false;
		ms_bShowSceneCostFromList_Dwds = false;
		ms_bShowSceneCostFromList_Drawables = false;
		ms_bShowSceneCostFromList_Frags = false;
		ms_bShowSceneCostFromList_Ptfx = false;
		ms_nSelectedList = SCENECOSTTRACKER_LIST_SPHERE;
		ms_nSearchRadius = COSTTRACKER_MAX_RADIUS;
		ms_bIncludeZoneAssets = true;
		ms_bIncludeHdTxdPromotion = true;
	}
}

s32 CSceneCostTracker::SortByNameCB(const strIndex* pIndex1, const strIndex* pIndex2)
{
	char achName1[RAGE_MAX_PATH];
	char achName2[RAGE_MAX_PATH];
	const char *name1 = strStreamingEngine::GetInfo().GetObjectName(*pIndex1, achName1, RAGE_MAX_PATH);
	const char *name2 = strStreamingEngine::GetInfo().GetObjectName(*pIndex2, achName2, RAGE_MAX_PATH);
	return stricmp(name1, name2);
}

void CSceneCostTracker::WriteToLog(const char* pszFileName)
{
	if (!pszFileName)
		return;

	fiStream* s_LogStream = NULL;

	// start log

	s_LogStream = pszFileName[0] ? ASSET.Create(pszFileName, "") : NULL;

	if (!s_LogStream)
	{
		Errorf("Could not create '%s'", ms_achLogFileName);
		return;
	}

	// write list
	//CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	//const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	fprintf(s_LogStream, "** START LOG: COST TRACKER ASSETS (name, main, vram) **\n\n" );
	fflush(s_LogStream);

	m_streamingIndices.QSort(0, -1, SortByNameCB);
	for (s32 i=0; i<m_streamingIndices.GetCount(); i++)
	{
		const char* name = strStreamingEngine::GetInfo().GetObjectName(m_streamingIndices[i]);
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(m_streamingIndices[i]);
		fprintf(s_LogStream, "%s, %dkb, %dkb\n", name,
			( info.ComputeVirtualSize(m_streamingIndices[i], true) >> 10 ), ( info.ComputePhysicalSize(m_streamingIndices[i], true) >> 10) );
		fflush(s_LogStream);
	}
	
	// end log
	fprintf(s_LogStream, "\n** END LOG: COST TRACKER **\n\n");
	fflush(s_LogStream);
	s_LogStream->Close();
	s_LogStream = NULL;
}

void CSceneCostTracker::StartMemCheckCB()
{
	g_SceneMemoryCheck.Start();
}

void CSceneCostTracker::StopMemCheckCB()
{
	g_SceneMemoryCheck.Stop();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::Update()
{
	switch (m_eState)
	{
	case STATE_IDLE:
		// do nothing
		break;
	case STATE_FLUSHING:
		if (CStreaming::GetCleanup().HasFlushBeenProcessed())
		{
			// if flush has completed, we can take readings, unpause streaming and let the map data stream in etc
			SetState(STATE_FLUSHING_AGAIN);
		}
		break;
	case STATE_FLUSHING_AGAIN:
		if (CStreaming::GetCleanup().HasFlushBeenProcessed())
		{
			// if flush has completed, we can take readings, unpause streaming and let the map data stream in etc
			SetState(STATE_LOADING_MAP);
		}
		break;
	case STATE_LOADING_MAP:
		{
			// if map is loaded, we can safely take a cost tracker reading
			Vec3V vPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );
			if ( fwMapDataStore::GetStore().GetBoxStreamer().HasLoadedAboutPos(vPos, fwBoxStreamerAsset::MASK_MAPDATA) 
				&& !strStreamingEngine::GetInfo().GetNumberRealObjectsRequested() )
			{
				SetState(STATE_SAMPLING_COSTTRACKER);
			}
		}
		break;
	case STATE_SAMPLING_COSTTRACKER:
		SampleCostTracker();
		if (m_numSamples >= NUM_TRACKER_SAMPLES)
		{
			SetState(STATE_FINISHED);
		}
		break;
	case STATE_FINISHED:
		if ( m_bActive && IsFinished() )
		{
			// display results onscreen if they are known
			DisplayResults();
		}
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetState
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::SetState(eState newState)
{
	switch (newState)
	{
	case STATE_IDLE:
		m_bActive = false;
		break;
	case STATE_FLUSHING:
		Assertf(PARAM_maponly.Get() || PARAM_maponlyextra.Get(), "This test requires -mapdata param enabled");
		m_vPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );

		Displayf("*** Starting junction test at (%4.2f, %4.2f, %4.2f) ***", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf());
		
		m_bActive = true;
		m_results.Reset();														// invalidate results
		g_SceneCostTracker.SetJunctionCaptureMode(true);						// activate cost tracker
		CStreaming::SetStreamingPaused(true);									// pause streaming so we get a clean measure of mem without map
		fwTimer::StartUserPause(true);											// pause game
		g_SceneStreamerMgr.GetStreamingLists().SetSceneStreamingEnabled(false);	// disable scene streaming
		FreezePlayer(true);
		CStreaming::GetCleanup().RequestFlush(false);							// flush scene
		break;
	case STATE_FLUSHING_AGAIN:
		CStreaming::GetCleanup().RequestFlush(false);							// flush scene
		break;
	case STATE_LOADING_MAP:
		CalcAndStoreMemAvailable();												// check amount of free memory the scene has to play with
		CStreaming::SetStreamingPaused(false);									// unpause streaming so we can starting bringing in required map data
		break;
	case STATE_SAMPLING_COSTTRACKER:
		m_numSamples = 0;
		m_maxTrackerMain = 0;
		m_maxTrackerVram = 0;
		break;
	case STATE_FINISHED:
	
		CalcAndStoreSceneCosts();
		m_results.SetIsValid(true);												// validate results

		//g_SceneCostTracker.WriteToLog("X:\\output.txt");

		g_SceneCostTracker.SetJunctionCaptureMode(false);						// deactivate cost tracker
		fwTimer::EndUserPause();												// unpause game
		g_SceneStreamerMgr.GetStreamingLists().SetSceneStreamingEnabled(true);	// enable scene streaming
		FreezePlayer(false);

		PrintResults();
		Displayf("*** Finished junction test at (%4.2f, %4.2f, %4.2f) ***", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf());
		break;
	}

	m_eState = newState;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SampleCostTracker
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::SampleCostTracker()
{
	Vec3V vPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );

	if ( fwMapDataStore::GetStore().GetBoxStreamer().HasLoadedAboutPos(vPos, fwBoxStreamerAsset::MASK_MAPDATA) 
		&& !strStreamingEngine::GetInfo().GetNumberRealObjectsRequested() )
	{
		m_numSamples++;

		m_maxTrackerMain = Max( m_maxTrackerMain, g_SceneCostTracker.GetTotalCost(false) );
		m_maxTrackerVram = Max( m_maxTrackerVram, g_SceneCostTracker.GetTotalCost(true) );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CalcAndStoreCosts
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::CalcAndStoreSceneCosts()
{
	Displayf("Free after flush: M:%d kb V:%d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_MAIN_FREE_AFTER_FLUSH)>>10, m_results.GetStat(CSceneMemoryCheckResult::STAT_VRAM_FREE_AFTER_FLUSH)>>10);

	// scene cost
	u32 sceneMain = m_maxTrackerMain;
	u32 sceneVram = m_maxTrackerVram;
	Displayf("Scene cost: M:%d kb V:%d kb", sceneMain>>10, sceneVram>>10);

	// static bounds cost
	u32 staticCollisionMain = 0;
	u32 staticCollisionVram = 0;
	g_StaticBoundsStore.GetBoxStreamer().GetCosts(staticCollisionMain, staticCollisionVram);
	Displayf("Static collision cost: M:%d kb V:%d kb", staticCollisionMain>>10, staticCollisionVram>>10);

	// imap and ityp costs ???

	// store results
#if __XENON || RSG_DURANGO || RSG_ORBIS
	m_results.SetStat( CSceneMemoryCheckResult::STAT_MAIN_SCENE_REQUIRED, (sceneMain + sceneVram + staticCollisionMain + staticCollisionVram ) );
	m_results.SetStat( CSceneMemoryCheckResult::STAT_VRAM_SCENE_REQUIRED, (0) );
#else
	m_results.SetStat( CSceneMemoryCheckResult::STAT_MAIN_SCENE_REQUIRED, (sceneMain + staticCollisionMain ) );
	m_results.SetStat( CSceneMemoryCheckResult::STAT_VRAM_SCENE_REQUIRED, (sceneVram + staticCollisionVram ) );
#endif
}

void CSceneMemoryCheck::FreezePlayer(bool bFrozen)
{
	CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();

	if (bFrozen)
	{
		pPlayerPed->DisableCollision(NULL, true);
		pPlayerPed->SetFixedPhysics(true);
	}
	else
	{
		pPlayerPed->EnableCollision(NULL);
		pPlayerPed->SetFixedPhysics(false);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CalcAndStoreMemAvailable
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::CalcAndStoreMemAvailable()
{

#if __PS3
	u32 usedStreamingMain			= (u32) strStreamingEngine::GetAllocator().GetVirtualMemoryUsed();
	u32 usedStreamingVram			= (u32) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();

	u32 availableStreamingMain		= (u32) strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable();
	u32 availableStreamingVram		= (u32) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
#else
	u32 usedStreamingMain			= (u32) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	u32 usedStreamingVram			= 0;
	u32 availableStreamingMain		= (u32) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
	u32 availableStreamingVram		= 0;
#endif

	m_results.SetStat( CSceneMemoryCheckResult::STAT_MAIN_USED_AFTER_FLUSH, (usedStreamingMain) );
	m_results.SetStat( CSceneMemoryCheckResult::STAT_VRAM_USED_AFTER_FLUSH, (usedStreamingVram) );

	m_results.SetStat( CSceneMemoryCheckResult::STAT_MAIN_FREE_AFTER_FLUSH, (availableStreamingMain - usedStreamingMain) );
	m_results.SetStat( CSceneMemoryCheckResult::STAT_VRAM_FREE_AFTER_FLUSH, (availableStreamingVram - usedStreamingVram) );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayResults
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::DisplayResults() const
{
	// display results
	if (m_results.SceneFits())
	{
		grcDebugDraw::AddDebugOutput( Color_green, "Scene at (%4.2f, %4.2f, %4.2f) fits in memory", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf() );
	}
	else
	{
		grcDebugDraw::AddDebugOutput( Color_red, "Scene at (%4.2f, %4.2f, %4.2f) does NOT fit in memory", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf() );
	}
	grcDebugDraw::AddDebugOutput( "MAIN after flush: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_MAIN_FREE_AFTER_FLUSH) >> 10 );
	grcDebugDraw::AddDebugOutput( "MAIN required by scene: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_MAIN_SCENE_REQUIRED) >> 10 );
	grcDebugDraw::AddDebugOutput( "MAIN shortfall: %d kb", m_results.GetMainShortfall() >> 10 );
	grcDebugDraw::AddDebugOutput( "VRAM after flush: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_VRAM_FREE_AFTER_FLUSH) >> 10 );
	grcDebugDraw::AddDebugOutput( "VRAM required by scene: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_VRAM_SCENE_REQUIRED) >> 10 );
	grcDebugDraw::AddDebugOutput( "VRAM shortfall: %d kb", m_results.GetVramShortfall() >> 10 );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PrintResults
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CSceneMemoryCheck::PrintResults() const
{
	// display results
	if (m_results.SceneFits())
	{
		Displayf( "Scene at (%4.2f, %4.2f, %4.2f) fits in memory", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf() );
	}
	else
	{
		Displayf( "Scene at (%4.2f, %4.2f, %4.2f) does NOT fit in memory", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf() );
	}
	Displayf( "MAIN after flush: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_MAIN_FREE_AFTER_FLUSH) >> 10 );
	Displayf( "MAIN required by scene: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_MAIN_SCENE_REQUIRED) >> 10 );
	Displayf( "MAIN shortfall: %d kb", m_results.GetMainShortfall() >> 10 );
	Displayf( "VRAM after flush: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_VRAM_FREE_AFTER_FLUSH) >> 10 );
	Displayf( "VRAM required by scene: %d kb", m_results.GetStat(CSceneMemoryCheckResult::STAT_VRAM_SCENE_REQUIRED) >> 10 );
	Displayf( "VRAM shortfall: %d kb", m_results.GetVramShortfall() >> 10 );
}

#endif //__BANK


