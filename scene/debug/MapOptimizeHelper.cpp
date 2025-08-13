/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/MapOptimizeHelper.cpp
// PURPOSE : widget to guide map optimisation effort, tracking txd costs etc
// AUTHOR :  Ian Kiigan
// CREATED : 21/01/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/MapOptimizeHelper.h"

SCENE_OPTIMISATIONS();

#if __BANK

#include "atl/map.h"
#include "bank/bkmgr.h"
#include "debug/BlockView.h"
#include "debug/colorizer.h"
#include "debug/DebugScene.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerSearch.h" // for CTxdRef, GetAssociatedTxds_ModelInfo
#include "debug/TextureViewer/TextureViewerUtil.h"
#include "file/packfile.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodMgr.h"
#include "scene/world/gameworld.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingdebug.h"
#include "streaming/packfilemanager.h"

#define MAX_RADIUS	(4500)

bool CMapOptimizeHelper::ms_bEnabled = false;
bool CMapOptimizeHelper::ms_bShowResultsInTextureViewer = false;
bool CMapOptimizeHelper::ms_bNewScanRequired = false;
bool CMapOptimizeHelper::ms_bNewPvsScanRequired = false;
atArray<RegdEnt> CMapOptimizeHelper::ms_entities;
atArray<u32> CMapOptimizeHelper::ms_sortedEntityIndices;
atArray<u32> CMapOptimizeHelper::ms_sortedOrphanEntityIndices;
atArray<u32> CMapOptimizeHelper::ms_sortedBadLodDistEntityIndices;
CUiGadgetWindow* CMapOptimizeHelper::ms_pWindow = NULL;
CUiGadgetList* CMapOptimizeHelper::ms_pList1 = NULL;
CUiGadgetList* CMapOptimizeHelper::ms_pList2 = NULL;
CUiGadgetList* CMapOptimizeHelper::ms_pList3 = NULL;
CUiGadgetList* CMapOptimizeHelper::ms_pList4 = NULL;
CUiGadgetBox* CMapOptimizeHelper::ms_pInspectorBg = NULL;
CUiGadgetText* CMapOptimizeHelper::ms_apStatNames[CMapOptimizeHelper::INSPECTOR_STAT_TOTAL];
CUiGadgetText* CMapOptimizeHelper::ms_apStatValues[CMapOptimizeHelper::INSPECTOR_STAT_TOTAL];
atArray<CTxdRef> CMapOptimizeHelper::ms_sortedTxdRefList;
s32 CMapOptimizeHelper::ms_backupAssetIndex = -1;
s32 CMapOptimizeHelper::ms_backupEntityIndex = -1;
bool CMapOptimizeHelper::ms_bIsolateSelected = false;
bool CMapOptimizeHelper::ms_bWriteToLog = false;
bool CMapOptimizeHelper::ms_bWriteUnusedTexturesToLog = false;
bool CMapOptimizeHelper::ms_bWriteUnusedTexturesOnlyUsed = true;
bool CMapOptimizeHelper::ms_bWriteUniqueTextures = true;
bool CMapOptimizeHelper::ms_bWriteUniqueDuplicatesOnly = true;
bool CMapOptimizeHelper::ms_bWriteUniqueExcludePed = true;
s32 CMapOptimizeHelper::ms_mapDataType = 0;
s32 CMapOptimizeHelper::ms_radius = MAX_RADIUS;
char CMapOptimizeHelper::ms_achLogFileName[256] = "X:\\gta5\\build\\dev\\MapOptimizerLog.txt";
char CMapOptimizeHelper::ms_achFilterName[256] = "";
s32 CMapOptimizeHelper::ms_currentMode = CMapOptimizeHelper::MODE_TXD;
static fiStream* s_LogStream = NULL;
static const char*	apszModeDescs[] =
{
	"Txd analysis",				//MODE_TXD
	"Orphan entity analysis",	//MODE_ORPHANENTITY
	"Lod dist analysis"			//MODE_LODDIST
};
static const char*	apszMapTypeDescs[] =
{
	"Map only",		//MAPDATA_TYPE_MAPONLY
	"Props only",	//MAPDATA_TYPE_PROPSONLY
	"Map & Props",	//MAPDATA_TYPE_MAPANDPROPS
	"All",			//MAPDATA_TYPE_ALL
};
static const char* apszLodDistFaults[] =
{
	"none",
	"too small",
	"doesn't match sibling",
	"greater than parent"
};

static atString GetTxdRefShortDesc(const CTxdRef& ref) // a little shorter than the descriptions used in the texture viewer
{
	char achTemp[256] = "";
	sprintf(achTemp, "%s:%s", CAssetRef::GetAssetTypeName(ref.GetAssetType(), true), ref.GetAssetName());

	// to be consistent with WriteListToFile, capitalise the asset type
	for (char* s = &achTemp[0]; *s && *s != ':'; s++)
	{
		*s = (char) toupper(*s);
	}

	return atString(achTemp);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddWidgets
// PURPOSE:		debug widgets for map optimizer helper
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Map Optimization Helper");
	bank.AddToggle("Enable", &ms_bEnabled, datCallback(EnabledCB));
	bank.AddCombo("Mode", &ms_currentMode, MODE_TOTAL, &apszModeDescs[0], datCallback(SwitchModeCB));
	bank.AddToggle("Show results in Texture Viewer", &ms_bShowResultsInTextureViewer, datCallback(TextureViewerCB));
	bank.AddToggle("Isolate selected", &ms_bIsolateSelected);
	bank.AddSlider("Search radius", &ms_radius, 1, MAX_RADIUS, 1);
	bank.AddCombo("Data type", &ms_mapDataType, MAPDATA_TYPE_TOTAL, &apszMapTypeDescs[0]);
	bank.AddText("Search txd name", ms_achFilterName, 256, datCallback(FilterAssetsCB));
	bank.AddButton("Search", datCallback(FilterAssetsCB));
	bank.AddButton("Scan around player", datCallback(RequestNewScanCB));
	bank.AddButton("Scan visible", datCallback(RequestNewPvsScanCB));
	bank.AddSeparator();
	bank.AddText("Output file", ms_achLogFileName, 256);
	bank.AddButton("Pick file", datCallback(SelectLogFile));
	bank.AddButton("Write list to file", LogWriteRequired);
	bank.AddButton("Write unused texture list", LogWriteUnusedTextures);
	bank.AddToggle("Only write textures used by models", &ms_bWriteUnusedTexturesOnlyUsed);
	bank.AddToggle("Write unique textures", &ms_bWriteUniqueTextures);
	bank.AddToggle("Only out unique texture duplicates", &ms_bWriteUniqueDuplicatesOnly);
	bank.AddToggle("Exclude ped textures", &ms_bWriteUniqueExcludePed);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates lists for display
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::Update()
{
	if (ms_bWriteUnusedTexturesToLog)
	{
		WriteUnusedTexturesToFile();
		ms_bWriteUnusedTexturesToLog = false;
	}

	if (!ms_bEnabled) { return; }

	if (ms_bNewScanRequired)
	{
		CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
		PerformNewScan(vPlayerPos, true, strlen(ms_achFilterName)>0);
		ms_bNewScanRequired = false;
		ms_backupAssetIndex = -1;
	}

	if (ms_backupAssetIndex != ms_pList1->GetCurrentIndex())
	{
		ms_backupAssetIndex = ms_pList1->GetCurrentIndex();
		RegenSortedEntityList();
	}

	CEntity* pSelectedEntity = NULL;

	//////////////////////////////////////////////////////////////////////////
	// draw AABBs and get currently selected entity from whatever list has focus
	switch (ms_currentMode)
	{
	case MODE_TXD:
		{
			if (ms_pList1->GetCurrentIndex()!=-1 && ms_sortedEntityIndices.size()>0)
			{
				if (ms_pList1->HasFocus())
				{
					// draw aabb for all entities using this asset
					for (u32 i=0; i<ms_sortedEntityIndices.size(); i++)
					{
						u32 index = ms_sortedEntityIndices[i];
						if (ms_entities[index])
						{
							CDebugScene::DrawEntityBoundingBox(ms_entities[index], Color32(255, 0, 0, 255));
						}
					}
				}
				else if (ms_pList2->HasFocus() && ms_pList2->GetCurrentIndex()!=-1)
				{
					// draw only the aabb for selected entity
					u32 index = ms_sortedEntityIndices[ms_pList2->GetCurrentIndex()];
					pSelectedEntity = ms_entities[index];
					if (pSelectedEntity)
					{
						CDebugScene::DrawEntityBoundingBox(pSelectedEntity, Color32(255, 0, 0, 255));
					}
				}
			}
		}
		break;
	case MODE_ORPHANENTITY:
		{
			if (ms_pList3->GetCurrentIndex()!=-1 && ms_pList3->HasFocus() && ms_sortedOrphanEntityIndices.size()>0)
			{
				u32 index = ms_sortedOrphanEntityIndices[ms_pList3->GetCurrentIndex()];
				pSelectedEntity = ms_entities[index];
				if (pSelectedEntity)
				{
					CDebugScene::DrawEntityBoundingBox(pSelectedEntity, Color32(255, 0, 0, 255));
				}
			}
		}
		break;

	case MODE_LODDIST:
		{
			if (ms_pList4->GetCurrentIndex()!=-1 && ms_pList4->HasFocus() && ms_sortedBadLodDistEntityIndices.size()>0)
			{
				u32 index = ms_sortedBadLodDistEntityIndices[ms_pList4->GetCurrentIndex()];
				pSelectedEntity = ms_entities[index];
				if (pSelectedEntity)
				{
					CDebugScene::DrawEntityBoundingBox(pSelectedEntity, Color32(255, 0, 0, 255));
				}
			}
		}
		break;

	default:
		break;
	}

	UpdateInspector(pSelectedEntity);
	
	//////////////////////////////////////////////////////////////////////////
	// update texture viewer
	if (ms_bShowResultsInTextureViewer)
	{
		s32 list1Index = ms_pList1->GetCurrentIndex();

		if (ms_currentMode==MODE_TXD && ms_pList1->HasFocus() && list1Index!=-1 && list1Index<ms_pList1->GetNumEntries())
		{
			CDebugTextureViewerInterface::SelectTxd(ms_sortedTxdRefList[list1Index], false, true);
		}
		//else if (ms_pList2->HasFocus() && list2Index!=-1 && list2Index<ms_pList2->GetNumEntries())
		else if (pSelectedEntity)
		{
			CDebugTextureViewerInterface::SelectModelInfo(pSelectedEntity->GetModelIndex(), pSelectedEntity, false, true);
		}
	}
	
	// write out list to file
	if (ms_bWriteToLog)
	{
		WriteListToFile();
		ms_bWriteToLog = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ShouldDraw
// PURPOSE:		supports "isolate entities" toggle - returns true if entity
//				should not be culled
//////////////////////////////////////////////////////////////////////////
bool CMapOptimizeHelper::ShouldDraw(CEntity* pEntity)
{
	if (pEntity && ms_bEnabled && ms_bIsolateSelected)
	{
		switch (ms_currentMode)
		{
		case MODE_TXD:
			{
				if (ms_pList1->GetCurrentIndex()!=-1 && ms_sortedEntityIndices.size()>0)
				{
					if (ms_pList1->HasFocus())
					{
						for (u32 i=0; i<ms_sortedEntityIndices.size(); i++)
						{
							s32 index = ms_sortedEntityIndices[i];
							if (ms_entities[index] != NULL)
							{
								if (ms_entities[index] == pEntity)
								{
									pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
									return true;
								}
							}
						}
					}
					else if (ms_pList2->HasFocus() && ms_pList2->GetCurrentIndex()!=-1)
					{
						s32 index =  ms_sortedEntityIndices[ms_pList2->GetCurrentIndex()];
						if (ms_entities[index] && ms_entities[index]==pEntity)
						{
							pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
							return true;
						}
					}
					return false;
				}
			}
			break;
		case MODE_ORPHANENTITY:
			{
				if (ms_pList3->HasFocus() && ms_pList3->GetCurrentIndex()!=-1)
				{
					s32 index = ms_sortedOrphanEntityIndices[ms_pList3->GetCurrentIndex()];
					if (ms_entities[index] && ms_entities[index]==pEntity)
					{
						pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
						return true;
					}
					return false;
				}
			}
			break;
		case MODE_LODDIST:
			{
				if (ms_pList4->HasFocus() && ms_pList4->GetCurrentIndex()!=-1)
				{
					s32 index = ms_sortedBadLodDistEntityIndices[ms_pList4->GetCurrentIndex()];
					if (ms_entities[index] && ms_entities[index]==pEntity)
					{
						pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
						return true;
					}
					return false;
				}
			}
			break;
		default:
			break;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CompareTxdRefSizeCB
// PURPOSE:		callback for sorting by size
//////////////////////////////////////////////////////////////////////////
s32 CMapOptimizeHelper::CompareTxdRefSizeCB(const CTxdRef* pTxdRef1, const CTxdRef* pTxdRef2)
{
	int cost1 = pTxdRef1->GetStreamingAssetSize(false);
	int cost2 = pTxdRef2->GetStreamingAssetSize(false);
	if (cost1 == cost2)
	{
		return stricmp(pTxdRef2->GetAssetName(), pTxdRef1->GetAssetName());
	}
	return cost2 - cost1;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CompareNameCB
// PURPOSE:		callback for sorting by entity memory address
//////////////////////////////////////////////////////////////////////////
s32 CMapOptimizeHelper::CompareNameCB(const u32* pIndex1, const u32* pIndex2)
{
	s32 index1 = *pIndex1;
	s32 index2 = *pIndex2;

	const CEntity* pEntity1 = ms_entities[index1];
	const CEntity* pEntity2 = ms_entities[index2];
	const char* pszName1 = CModelInfo::GetBaseModelInfoName(pEntity1->GetModelId());
	const char* pszName2 = CModelInfo::GetBaseModelInfoName(pEntity2->GetModelId());
	return stricmp(pszName1, pszName2);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CompareLodDistCB
// PURPOSE:		callback for sorting by entity lod distance
//////////////////////////////////////////////////////////////////////////
s32 CMapOptimizeHelper::CompareLodDistCB(const u32* pIndex1, const u32* pIndex2)
{
	s32 index1 = *pIndex1;
	s32 index2 = *pIndex2;

	const CEntity* pEntity1 = ms_entities[index1];
	const CEntity* pEntity2 = ms_entities[index2];
	const s32 lodDist1 = (s32) pEntity1->GetLodDistance();
	const s32 lodDist2 = (s32) pEntity2->GetLodDistance();

	if (lodDist1 == lodDist2)
	{
		return ptrdiff_t_to_int((const u8*)pEntity1 - (const u8*)pEntity2);
	}
	return lodDist2 - lodDist1;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetTxdRefsUsedByEntity
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void GetTxdRefsUsedByEntity(atMap<CTxdRef, u32>& txdRefMap, const CEntity* pEntity)
{
	if (pEntity)
	{
		atArray<CTxdRef> temp;
		GetAssociatedTxds_ModelInfo(temp, pEntity->GetBaseModelInfo(), pEntity, NULL);

		for (int i = 0; i < temp.size(); i++)
		{
			txdRefMap[temp[i]] = 1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FilterAssetsCB
// PURPOSE:		regenerate filtered asset lists without doing a new world search
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::FilterAssetsCB()
{
	if (ms_entities.size()>0)
	{
		PerformNewScan(Vector3(), false, strlen(ms_achFilterName)>0);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PerformNewScan
// PURPOSE:		performs a spherical world search around player and populates
//				the asset and entity lists
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::PerformNewScan(const Vector3& vPos, bool bPerformScan, bool bFiltered)
{
	// search world
	ms_pList1->SetNumEntries(0);
	ms_pList2->SetNumEntries(0);
	ms_pList3->SetNumEntries(0);
	ms_pList4->SetNumEntries(0);

	if (bPerformScan)
	{
		ms_entities.Reset();
	}
	ms_sortedEntityIndices.Reset();
	ms_sortedOrphanEntityIndices.Reset();
	ms_sortedBadLodDistEntityIndices.Reset();

	if (bPerformScan)
	{
		s32 entityTypeFlags = ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT;

		if (ms_mapDataType == MAPDATA_TYPE_ALL)
		{
			entityTypeFlags |= ENTITY_TYPE_MASK_VEHICLE | ENTITY_TYPE_MASK_PED;
		}

		spdSphere hdSphere(RCC_VEC3V(vPos), ScalarV((float)ms_radius));
		fwIsSphereIntersectingVisible hdSearchSphere(hdSphere);
		CGameWorld::ForAllEntitiesIntersecting
		(
			&hdSearchSphere,
			AppendToListCB,
			NULL,
			entityTypeFlags,
			SEARCH_LOCATION_INTERIORS | SEARCH_LOCATION_EXTERIORS, // do we need a toggle to control this?
			SEARCH_LODTYPE_ALL,
			SEARCH_OPTION_FORCE_PPU_CODEPATH,
			WORLDREP_SEARCHMODULE_DEFAULT
		);
	}

	// generate maps
	atMap<CTxdRef, u32> txdRefMap;
	for (u32 i=0; i<ms_entities.size(); i++)
	{
		GetTxdRefsUsedByEntity(txdRefMap, ms_entities[i]);
	}

	// generate sorted lists
	ms_sortedTxdRefList.Reset();
	for (atMap<CTxdRef, u32>::Iterator iter = txdRefMap.CreateIterator(); !iter.AtEnd(); iter.Next())
	{
		const CTxdRef& ref = iter.GetKey();
		const fwTxd* txd = ref.GetTxd();

		if (txd && txd->GetCount()>0) // only show txds which are loaded and not empty
		{
			if (!bFiltered || strstr(ref.GetAssetName(), ms_achFilterName))
			{
				ms_sortedTxdRefList.PushAndGrow(ref);
			}
		}
	}
	ms_sortedTxdRefList.QSort(0, -1, CompareTxdRefSizeCB);

	for (u32 i=0; i<ms_entities.size(); i++) // if no txd is selected, then show all entities
	{
		if (ms_entities[i] != NULL)
		{
			ms_sortedEntityIndices.PushAndGrow(i);

			if (ms_entities[i]->GetLodData().IsOrphanHd())
			{
				ms_sortedOrphanEntityIndices.PushAndGrow(i);
			}

			if (FindLodDistFault(ms_entities[i]) != BADLODDIST_FAULT_NONE)
			{
				ms_sortedBadLodDistEntityIndices.PushAndGrow(i);
			}
		}
	}
	ms_sortedEntityIndices.QSort(0, -1, CompareNameCB);
	ms_sortedOrphanEntityIndices.QSort(0, -1, CompareLodDistCB);
	ms_sortedBadLodDistEntityIndices.QSort(0, -1, CompareNameCB);

	// update ui gadgets
	ms_pList1->SetNumEntries(ms_sortedTxdRefList.size());
	ms_pList2->SetNumEntries(ms_sortedEntityIndices.size());
	ms_pList3->SetNumEntries(ms_sortedOrphanEntityIndices.size());
	ms_pList4->SetNumEntries(ms_sortedBadLodDistEntityIndices.size());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	EnabledCB
// PURPOSE:		creates or destroys the UI gadgets for display, controlled by toggle
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::EnabledCB()
{
	if (!ms_bEnabled)
	{
		DestroyGadgets();
	}
	else
	{
		CreateGadgets();
	}

	SwitchModeCB();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TextureViewerCB
// PURPOSE:		hides the texture viewer when no longer required
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::TextureViewerCB()
{
	if (!ms_bShowResultsInTextureViewer)
	{
		CDebugTextureViewerInterface::Hide();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AppendToListCB
// PURPOSE:		callback for world search code to append a registered entity to list
//////////////////////////////////////////////////////////////////////////
bool CMapOptimizeHelper::AppendToListCB(CEntity* pEntity, void* UNUSED_PARAM(pData))
{
	if (pEntity && g_LodMgr.WithinVisibleRange(pEntity))
	{
		bool bAdd = true;
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		Assert (pModelInfo);
		bool bIsAProp = pModelInfo->GetIsProp();

		if (bIsAProp)
		{
			if (ms_mapDataType == MAPDATA_TYPE_MAPONLY)
			{
				bAdd = false;
			}
		}
		else // not a prop
		{
			if (ms_mapDataType == MAPDATA_TYPE_PROPSONLY)
			{
				bAdd = false;
			}
		}

		if (bAdd)
		{
			ms_entities.PushAndGrow(RegdEnt(pEntity));
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateCellForAsset
// PURPOSE:		callback for UI gadget to refresh an asset list cell
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (pResult && row<ms_sortedTxdRefList.size())
	{
		const CTxdRef& ref = ms_sortedTxdRefList[row];
		switch (col)
		{
		case TXDLIST_COLUMN_NAME:
			{
				pResult->SetString(GetTxdRefShortDesc(ref).c_str());
			}
			break;
		case TXDLIST_COLUMN_SIZE:
			{
				char achTmp[256];
				sprintf(achTmp, "%2.3f mb", (float) ref.GetStreamingAssetSize(false) / (1024.0f * 1024.0f));
				pResult->SetString(achTmp);
			}
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateCellForEntity
// PURPOSE:		callback for UI gadget to refresh entity list cell
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateCellForEntity(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (pResult && row<ms_sortedEntityIndices.size())
	{
		switch (col)
		{
		case ENTITYLIST_COLUMN_NAME:
			{
				s32 index = ms_sortedEntityIndices[row];
				if (ms_entities[index])
				{
					pResult->SetString(CModelInfo::GetBaseModelInfoName(ms_entities[index]->GetModelId()));
				}
			}
			break;
		case ENTITYLIST_COLUMN_DIST:
			{
				s32 index = ms_sortedEntityIndices[row];
				char achTmp[256];
				sprintf(achTmp, "%3.2f m", CLodDebug::GetDistFromCamera(ms_entities[index]));
				pResult->SetString(achTmp);
			}
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateCellForOrphanEntity
// PURPOSE:		callback for UI gadget to refresh orphan entity list cell
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateCellForOrphanEntity(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (pResult && row<ms_sortedOrphanEntityIndices.size())
	{
		switch (col)
		{
		case ORPHANLIST_COLUMN_NAME:
			{
				s32 index = ms_sortedOrphanEntityIndices[row];
				if (ms_entities[index])
				{
					pResult->SetString(CModelInfo::GetBaseModelInfoName(ms_entities[index]->GetModelId()));
				}
			}
			break;
		case ORPHANLIST_COLUMN_LODDIST:
			{
				s32 index = ms_sortedOrphanEntityIndices[row];
				char achTmp[256];
				sprintf(achTmp, "%d m", ms_entities[index]->GetLodDistance());
				pResult->SetString(achTmp);
			}
			break;
		case ORPHANLIST_COLUMN_COST:
			{
				s32 index = ms_sortedOrphanEntityIndices[row];
				if (ms_entities[index])
				{
					int cost = CSceneIsolator::GetEntityCost(ms_entities[index], SCENEISOLATOR_COST_TOTAL) >>10;
					char achTmp[256];
					sprintf(achTmp, "%d K", cost);
					pResult->SetString(achTmp);
				}
			}
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateCellForBadLodDistEntity
// PURPOSE:		callback for UI gadget to refresh bad lod dist entity list cell
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateCellForBadLodDistEntity(CUiGadgetText* pResult, u32 row, u32 col, void * UNUSED_PARAM(extraCallbackData) )
{
	if (pResult && row<ms_sortedBadLodDistEntityIndices.size())
	{
		switch (col)
		{
		case BADLODDISTLIST_COLUMN_NAME:
			{
				s32 index = ms_sortedBadLodDistEntityIndices[row];
				if (ms_entities[index])
				{
					pResult->SetString(CModelInfo::GetBaseModelInfoName(ms_entities[index]->GetModelId()));
				}
			}
			break;
		case BADLODDISTLIST_COLUMN_LODDIST:
			{
				s32 index = ms_sortedBadLodDistEntityIndices[row];
				if (ms_entities[index])
				{
					char achTmp[256];
					sprintf(achTmp, "%d m", ms_entities[index]->GetLodDistance());
					pResult->SetString(achTmp);
				}
			}
			break;
		case BADLODDISTLIST_COLUMN_FAULT:
			{
				s32 index = ms_sortedBadLodDistEntityIndices[row];
				if (ms_entities[index])
				{
					pResult->SetString(apszLodDistFaults[FindLodDistFault(ms_entities[index])]);
				}
			}
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateGadgets
// PURPOSE:		creates the UI components required for lists
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::CreateGadgets()
{
	if (!ms_pWindow && !ms_pList1 && !ms_pList2)
	{
		const float fDefaultX = 200.0f;
		const float fDefaultY = 50.0f;
		const float fDefaultW = 350.0f;

		const int numListEntries = 6;
		const int numListEntries3 = 13;
		const int numListEntries4 = 13;
		const float fPadding = 2.0f;
		const float fScrollBarW = 12.0f;
		const float fListX = fDefaultX + fPadding;
		const float fListW = fDefaultW - 2*fPadding;
		const float fWindowW = fDefaultW + fScrollBarW;
		const float fWindowH = 188.0f;
		const float fInspectorX = fDefaultX + fWindowW + fPadding;
		const float fInspectorY = fDefaultY + 10.0f;
		const float fInspectorW = 175.0f;

		CUiColorScheme colorScheme;
		float afColumnOffsets[] = { 0.0f, 250.0f };
		float afColumnOffsets3[] = { 0.0f, 150.0f, 250.0f };
		float afColumnOffsets4[] = { 0.0f, 150.0f, 200.0f };
		const char* columnTitles1[] = { "Asset", "Size" };
		const char* columnTitles2[] = { "Entity", "Distance" };
		const char* columnTitles3[] = { "Entity", "LodDist", "Cost" };
		const char* columnTitles4[] = { "Entity", "LodDist", "Fault" };
		ms_pWindow = rage_new CUiGadgetWindow(fDefaultX, fDefaultY, fWindowW, fWindowH, colorScheme);
		ms_pWindow->SetTitle("Map Optimization Helper");

		ms_pList1 = rage_new CUiGadgetList(fListX, fDefaultY+28.0f, fListW, numListEntries, 2, afColumnOffsets, colorScheme, columnTitles1);
		ms_pWindow->AttachChild(ms_pList1);
		ms_pList1->SetNumEntries(0);
		ms_pList1->SetUpdateCellCB(UpdateCellForAsset);
		ms_pList1->SetHasFocus(false);

		ms_pList2 = rage_new CUiGadgetList(fListX, fDefaultY+114.0f, fListW, numListEntries, 2, afColumnOffsets, colorScheme, columnTitles2);
		ms_pWindow->AttachChild(ms_pList2);
		ms_pList2->SetNumEntries(0);
		ms_pList2->SetUpdateCellCB(UpdateCellForEntity);
		ms_pList2->SetHasFocus(false);

		ms_pList3 = rage_new CUiGadgetList(fListX, fDefaultY+28.0f, fListW, numListEntries3, 3, afColumnOffsets3, colorScheme, columnTitles3);
		ms_pWindow->AttachChild(ms_pList3);
		ms_pList3->SetNumEntries(0);
		ms_pList3->SetUpdateCellCB(UpdateCellForOrphanEntity);
		ms_pList3->SetHasFocus(false);

		ms_pList4 = rage_new CUiGadgetList(fListX, fDefaultY+28.0f, fListW, numListEntries4, 3, afColumnOffsets4, colorScheme, columnTitles4);
		ms_pWindow->AttachChild(ms_pList4);
		ms_pList4->SetNumEntries(0);
		ms_pList4->SetUpdateCellCB(UpdateCellForBadLodDistEntity);
		ms_pList4->SetHasFocus(false);

		ms_pInspectorBg = rage_new CUiGadgetBox(fInspectorX, fDefaultY, fInspectorW, fWindowH, Color32(255,255,255,255));
		ms_pWindow->AttachChild(ms_pInspectorBg);
		const char* apszStatNames[] = {
			"Type", "LOD", "LOD dist", "Alpha", "Block", "Polys"
		};
		for (u32 i=0; i<INSPECTOR_STAT_TOTAL; i++)
		{
			ms_apStatNames[i] = rage_new CUiGadgetText(fInspectorX+fPadding, fInspectorY+i*30.0f, Color32(0,0,0), apszStatNames[i]);
			ms_apStatValues[i] = rage_new CUiGadgetText(fInspectorX+75.0f, fInspectorY+i*30.0f, Color32(0,0,200), "");
			ms_pInspectorBg->AttachChild(ms_apStatNames[i]);
			ms_pInspectorBg->AttachChild(ms_apStatValues[i]);
		}
		g_UiGadgetRoot.AttachChild(ms_pWindow);

		// update mode and gadget visibility
		SwitchModeCB();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DestroyGadgets
// PURPOSE:		deletes UI components for onscreen lists, window etc
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::DestroyGadgets()
{
	if (ms_pWindow && ms_pList1 && ms_pList2 && ms_pList3 && ms_pList4)
	{
		ms_pList1->DetachFromParent();
		ms_pList2->DetachFromParent();
		ms_pList3->DetachFromParent();
		ms_pList4->DetachFromParent();
		ms_pWindow->DetachFromParent();
		ms_pInspectorBg->DetachFromParent();

		for (u32 i=0; i<INSPECTOR_STAT_TOTAL; i++)
		{
			ms_apStatNames[i]->DetachFromParent();
			delete ms_apStatNames[i];
			ms_apStatNames[i] = NULL;

			ms_apStatValues[i]->DetachFromParent();
			delete ms_apStatValues[i];
			ms_apStatValues[i] = NULL;
		}

		delete ms_pInspectorBg;
		ms_pInspectorBg = NULL;

		delete ms_pList1;
		ms_pList1 = NULL;

		delete ms_pList2;
		ms_pList2 = NULL;

		delete ms_pList3;
		ms_pList3 = NULL;

		delete ms_pList4;
		ms_pList4 = NULL;

		delete ms_pWindow;
		ms_pWindow = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegenSortedEntityList
// PURPOSE:		when a user clicks a new asset, re-generate the list of entities using
//				that asset
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::RegenSortedEntityList()
{
	ms_pList2->SetNumEntries(0);
	ms_sortedEntityIndices.Reset();

	s32 currentRow = ms_pList1->GetCurrentIndex();

	if (currentRow!=-1 && currentRow<ms_sortedTxdRefList.size())
	{
		const CTxdRef& selectedTxdRef = ms_sortedTxdRefList[currentRow];
		{
			// run over all the entities in the list, check if they use the selected TXD
			for (u32 i=0; i<ms_entities.size(); i++)
			{
				atMap<CTxdRef, u32> txdRefMap;
				GetTxdRefsUsedByEntity(txdRefMap, ms_entities[i]);
				if (txdRefMap.Access(selectedTxdRef))
				{
					ms_sortedEntityIndices.PushAndGrow(i);
				}
			}
			ms_sortedEntityIndices.QSort(0, -1, CompareNameCB);
		}
	}
	else if (currentRow == -1)
	{
		for (u32 i=0; i<ms_entities.size(); i++) // if no txd is selected, then show all entities
		{
			if (ms_entities[i] != NULL)
			{
				ms_sortedEntityIndices.PushAndGrow(i);
			}
		}
		ms_sortedEntityIndices.QSort(0, -1, CompareNameCB);
	}

	ms_pList2->SetNumEntries(ms_sortedEntityIndices.size());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetLogFile
// PURPOSE:		lets user specify output log file
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::SelectLogFile()
{
	if (!BANKMGR.OpenFile(ms_achLogFileName, 256, "*.txt", true, "Map Optimizer log (*.txt)"))
	{
		ms_achLogFileName[0] = '\0';
	}
}

typedef struct 
{
	u32 count;
	u32 size;
	u32 uses;
	s32 imageIndex;
} Unique;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteUnusedTexturesToFile
// PURPOSE:		writes the unused textures out to a log file
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::WriteUnusedTexturesToFile()
{
	typedef atMap<atHashString, Unique> UniqueMap;

	UniqueMap uniques;

	u16 refcounts[1024];
	// start log
	if (!s_LogStream)
	{
		s_LogStream = ms_achLogFileName[0] ? fiStream::Create(ms_achLogFileName, fiDeviceLocal::GetInstance()) : NULL;
	}
	if (!s_LogStream)
	{
		Errorf("Could not create '%s'", ms_achLogFileName);
		return;
	}

	// write list
	CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	fprintf(s_LogStream, "** START LOG: MAP OPTIMIZER HELPER ( %f, %f, %f )**\n", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z);
	fflush(s_LogStream);

	size_t totalUnused = 0;
	size_t total = 0;


	strStreamingFile* file = strPackfileManager::GetImageFile(strPackfileManager::FindImage("platform:/models/cdimages/componentpeds.rpf"));
	s32 componentPedsIndex = file->m_packfile->GetPackfileIndex();
	file = strPackfileManager::GetImageFile(strPackfileManager::FindImage("platform:/models/cdimages/streamedpeds.rpf"));
	s32 streamedPedsIndex = file->m_packfile->GetPackfileIndex();

	const s32 count = g_TxdStore.GetCount();
	for (int index = 0; index < count; ++index)
	{
		fwTxdDef* def = g_TxdStore.GetSlot(strLocalIndex(index));
		if (def && def->m_pObject && g_TxdStore.GetNumRefs(strLocalIndex(index)))
		{
			memset(refcounts, 0, sizeof(refcounts));
			size_t txdUnused = 0;
			size_t txdTotal = 0;
			fwTxd* txd = def->m_pObject;

			bool unused = false;
			bool usedByModels = false;
			const int textureCount = txd->GetCount(); 

			strIndex txdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(index));
			strStreamingInfo& otherinfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(txdIndex);
			int imageIndex = otherinfo.GetHandle() >> 16;

			for (int textureIndex = 0; textureIndex < textureCount; ++textureIndex)
			{
				grcTexture* texture = txd->GetEntry(textureIndex);
				size_t size = GetTextureSizeInBytes(texture);
				
				txdTotal += size;
				total += size;

				strIndex dependents[64];

				atHashString name = texture->GetName();
				Unique& unique = uniques[name];
				unique.count++;
				unique.size = (u32) size;
				unique.imageIndex = imageIndex;

				int dependentCount = CStreamingDebug::GetDependents(txdIndex, dependents, 64);
				for (int i = 0; i < dependentCount; ++i) 
				{
					const strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(dependents[i]);

					if (info.IsFlagSet(STRFLAG_INTERNAL_DUMMY))
					{
						strLocalIndex modelIndex = strStreamingEngine::GetInfo().GetObjectIndex(dependents[i]);
						CBaseModelInfo* basemodelinfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

						if (IsTextureUsedByModel(texture, NULL, basemodelinfo, NULL))
						{
							refcounts[textureIndex]++;
							unique.uses++;
							usedByModels = true;
						}
					}
				}

				if ((usedByModels || !ms_bWriteUnusedTexturesOnlyUsed) && (texture->GetRefCount() + refcounts[textureIndex] <= 1))
				{
					if (!unused)
					{
						fprintf(s_LogStream, "-- Texture dictionary %s --\n", def->m_name.GetCStr());
						
						for (int i = 0; i < dependentCount; ++i) 
						{
							const strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(dependents[i]);
							if (!info.IsFlagSet(STRFLAG_INTERNAL_DUMMY))
							{
								fprintf(s_LogStream, " USED BY: %s\n", strStreamingEngine::GetObjectName(dependents[i]));
							}
						}

						unused = true; 
					}
					
					totalUnused += size;
					txdUnused += size;

					fprintf(s_LogStream, "%s %d (%dKb)\n", texture->GetName(), size, size >> 10);
				}
			}
			if (unused)
				fprintf(s_LogStream, "Totals for %s - All %d (%dKb) - Unused %d (%dKb) \n\n", def->m_name.GetCStr(), txdTotal, txdTotal >> 10, txdUnused, txdUnused >> 10);

			fflush(s_LogStream);
		}
	}

	fprintf(s_LogStream, "Overall total - All %d (%dKb) - Unused %d (%dKb) \n", total, total >> 10, totalUnused, totalUnused >> 10);

	if (ms_bWriteUniqueTextures)
	{
		fprintf(s_LogStream, "\n Unique textures \n");

		UniqueMap::Iterator it = uniques.CreateIterator();
		size_t totalDuplicatesSize = 0;
		u32 totalDuplicateCount = 0;
		for (it.Start(); !it.AtEnd(); it.Next())
		{
			Unique& u = it.GetData(); 

			if (ms_bWriteUniqueExcludePed && (u.imageIndex == componentPedsIndex || u.imageIndex == streamedPedsIndex))
			{
				continue;
			}

			if (u.count > 1 || !ms_bWriteUniqueDuplicatesOnly)
			{
				atHashString h(it.GetKey());
				fprintf(s_LogStream, "%s, %d, %d, %d\n", h.GetCStr(), u.count, u.size,  u.size >> 10);
				totalDuplicatesSize += u.size * (u.count - 1);
				totalDuplicateCount += (u.count - 1);
			}
		}

		fprintf(s_LogStream, "Total texture data %d (%dKb) - Duplicate textures %d (%dKb) = %3.1f%\n", total, total >> 10, totalDuplicatesSize,  totalDuplicatesSize >> 10, (totalDuplicatesSize * 100.0f)/(total) );
	}

	// end log
	fprintf(s_LogStream, "\n** END LOG: MAP OPTIMIZER HELPER **\n\n");
	fflush(s_LogStream);
	s_LogStream->Close();
	s_LogStream = NULL;
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteListToFile
// PURPOSE:		writes the current lists out to a log file
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::WriteListToFile()
{
	// start log
	if (!s_LogStream)
	{
		s_LogStream = ms_achLogFileName[0] ? fiStream::Create(ms_achLogFileName, fiDeviceLocal::GetInstance()) : NULL;
	}
	if (!s_LogStream)
	{
		Errorf("Could not create '%s'", ms_achLogFileName);
		return;
	}

	// write list
	CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	fprintf(s_LogStream, "** START LOG: MAP OPTIMIZER HELPER ( %f, %f, %f )**\n", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z);
	fflush(s_LogStream);

	switch (ms_currentMode)
	{
	case MODE_TXD:
		{
			fprintf(s_LogStream, "(TXD ANALYSIS MODE)\n\n");
			fflush(s_LogStream);

			for (u32 i=0; i<ms_sortedTxdRefList.size(); i++)
			{
				const CTxdRef& ref = ms_sortedTxdRefList[i];

				fprintf(s_LogStream, "%s SIZE:%2.3f mb\n", GetTxdRefShortDesc(ref).c_str(), (float) ref.GetStreamingAssetSize(false) / (1024.0f * 1024.0f));
				fflush(s_LogStream);

				for (u32 j=0; j<ms_sortedEntityIndices.size(); j++)
				{
					s32 index = ms_sortedEntityIndices[j];
					CEntity* pEntity = ms_entities[index];
					if (pEntity)
					{
						fprintf(s_LogStream, "\tENT:%s DIST:%d metres\n",
							pEntity->GetBaseModelInfo()->GetModelName(),
							CLodDebug::GetDistFromCamera(pEntity));
						fflush(s_LogStream);
					}
				}
			}
		}
		break;
	case MODE_ORPHANENTITY:
		{
			fprintf(s_LogStream, "(ORPHAN ENTITY ANALYSIS MODE)\n\n");
			fflush(s_LogStream);

			for (u32 i=0; i<ms_sortedOrphanEntityIndices.size(); i++)
			{
				s32 index = ms_sortedOrphanEntityIndices[i];
				CEntity* pEntity = ms_entities[index];
				if (pEntity)
				{
					s32 cost = CSceneIsolator::GetEntityCost(pEntity, SCENEISOLATOR_COST_TOTAL) >>10;
					fprintf(s_LogStream, "\tENT:%s LODDIST:%d metres COST: %d K\n",
						pEntity->GetBaseModelInfo()->GetModelName(),
						pEntity->GetLodDistance(),
						cost
					);
					fflush(s_LogStream);
				}
			}
		}
		break;
	case MODE_LODDIST:
		{
			fprintf(s_LogStream, "(BAD LOD DIST ANALYSIS MODE)\n\n");
			fflush(s_LogStream);

			for (u32 i=0; i<ms_sortedBadLodDistEntityIndices.size(); i++)
			{
				s32 index = ms_sortedBadLodDistEntityIndices[i];
				CEntity* pEntity = ms_entities[index];
				if (pEntity)
				{
					fprintf(s_LogStream, "\tENT:%s LODDIST:%d metres FAULT:%s\n",
						pEntity->GetBaseModelInfo()->GetModelName(),
						pEntity->GetLodDistance(),
						apszLodDistFaults[FindLodDistFault(pEntity)]
					);
					fflush(s_LogStream);
				}
			}
		}
		break;
	default:
		break;
	}
	
	// end log
	fprintf(s_LogStream, "\n** END LOG: MAP OPTIMIZER HELPER **\n\n");
	fflush(s_LogStream);
	s_LogStream->Close();
	s_LogStream = NULL;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateInspector
// PURPOSE:		updates each stat value for specified entity
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateInspector(CEntity* pEntity)
{
	if (pEntity)
	{
		// entity type
		switch (pEntity->GetType())
		{
		case ENTITY_TYPE_BUILDING:
			ms_apStatValues[INSPECTOR_STAT_ENTITYTYPE]->SetString("bldng");
			break;
		case ENTITY_TYPE_ANIMATED_BUILDING:
			ms_apStatValues[INSPECTOR_STAT_ENTITYTYPE]->SetString("anim bldng");
			break;
		case ENTITY_TYPE_OBJECT:
			ms_apStatValues[INSPECTOR_STAT_ENTITYTYPE]->SetString("obj");
			break;
		default:
			ms_apStatValues[INSPECTOR_STAT_ENTITYTYPE]->SetString("non-map");
			break;

		}
		
		// lod type
		static const char* apszLodTypes[] = { "lodded hd", "lod", "slod1", "slod2", "slod3", "orphan hd" };
		ms_apStatValues[INSPECTOR_STAT_LODTYPE]->SetString(apszLodTypes[pEntity->GetLodData().GetLodType()]);

		// lod dist
		char achTmp[256];
		sprintf(achTmp, "%d m", pEntity->GetLodDistance());
		ms_apStatValues[INSPECTOR_STAT_LODDIST]->SetString(achTmp);

		// alpha
		sprintf(achTmp, "%d", pEntity->GetAlpha());
		ms_apStatValues[INSPECTOR_STAT_ALPHA]->SetString(achTmp);

		// block
		s32 blockIndex = fwMapDataDebug::GetBlockIndex(pEntity->GetIplIndex());

		if (blockIndex>=0 && blockIndex<CBlockView::GetNumberOfBlocks())
		{
			ms_apStatValues[INSPECTOR_STAT_BLOCK]->SetString(CBlockView::GetBlockName(blockIndex));
		}
		else
		{
			ms_apStatValues[INSPECTOR_STAT_BLOCK]->SetString("");
		}

		// poly count
		sprintf(achTmp, "%d", ColorizerUtils::GetPolyCount(pEntity));
		ms_apStatValues[INSPECTOR_STAT_POLYCOUNT]->SetString(achTmp);
	}
	else
	{
		for (u32 i=0; i<INSPECTOR_STAT_TOTAL; i++)
		{
			ms_apStatValues[i]->SetString("");
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SwitchModeCB
// PURPOSE:		user has changed analysis mode
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::SwitchModeCB()
{
	if (ms_pWindow)
	{
		char achTmp[200];
		sprintf(achTmp, "Map Optimization Helper (%s)", apszModeDescs[ms_currentMode]);
		ms_pWindow->SetTitle(achTmp);

		ms_pList1->SetActive(ms_currentMode == MODE_TXD);
		ms_pList2->SetActive(ms_currentMode == MODE_TXD);
		ms_pList3->SetActive(ms_currentMode == MODE_ORPHANENTITY);
		ms_pList4->SetActive(ms_currentMode == MODE_LODDIST);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateFromPvs
// PURPOSE:		resets the entity list to the contents of the PVS rather than a world search
//////////////////////////////////////////////////////////////////////////
void CMapOptimizeHelper::UpdateFromPvs(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop)
{
	if (ms_bNewPvsScanRequired)
	{
		ms_bNewPvsScanRequired = false;
		ms_entities.Reset();

		CGtaPvsEntry* pPvsEntry = pPvsStart;
		while (pPvsEntry != pPvsStop)
		{
			CEntity* pEntity = pPvsEntry->GetEntity();
			if (pEntity)
			{
				AppendToListCB(pEntity, NULL);
			}
			pPvsEntry++;
		}
		PerformNewScan(Vector3(), false, strlen(ms_achFilterName)>0);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindLodDistFault
// PURPOSE:		returns the fault code for an entity w.r.t. bad lod distances
//////////////////////////////////////////////////////////////////////////
u32 CMapOptimizeHelper::FindLodDistFault(CEntity* pEntity)
{
	if (pEntity)
	{
		u32 lodDist = pEntity->GetLodDistance();
		fwEntity* pLod = pEntity->GetLod();

		if (pLod)
		{
			if (pLod->GetLodData().GetChildLodDistance() != lodDist)
			{
				return BADLODDIST_FAULT_DOESNTMATCHSIBLINGS;
			}
		}

		if (pLod && pLod->GetLodDistance()<=lodDist)
		{
			return BADLODDIST_FAULT_GREATERTHANPARENT;
		}

		if (lodDist < pEntity->GetBoundRadius())
		{
			return BADLODDIST_FAULT_TOOSMALL;
		}
	}
	return BADLODDIST_FAULT_NONE;
}

#endif	//__BANK
