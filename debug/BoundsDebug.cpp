/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/BoundsDebug.h
// PURPOSE : useful debug functions static bounds etc
// AUTHOR :  Ian Kiigan
// CREATED : 27/09/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/BoundsDebug.h"

#if __BANK

#include "atl/string.h"
#include "debug/DebugScene.h"
#include "fwscene/stores/staticboundsstore.h"
#include "grcore/debugdraw.h"
#include "physics/gtaMaterialDebug.h"
#include "physics/gtaMaterialManager.h"
#include "scene/Entity.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"

s32 CBoundsDebug::ms_selectedBoundsComboIndex = 0;
s32 CBoundsDebug::ms_mode = CBoundsDebug::MODE_NONE;
atArray<s32> CBoundsDebug::ms_slotIndices;
atArray<const char*> CBoundsDebug::ms_apszDebugSlotNames;
bool CBoundsDebug::ms_bDisplayMoverWeaponBreakdown = false;

static const char* apszModes[] = { "NONE", "UNDER MOUSE", "FROM LIST" };

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		adds widgets for bounds debugging
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::InitWidgets(bkBank& bank)
{
	ms_slotIndices.Reset();
	ms_apszDebugSlotNames.Reset();
	for(s32 i=0; i<g_StaticBoundsStore.GetCount(); i++)
	{	
		fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(strLocalIndex(i));
		if (pDef && pDef->m_name.GetLength()>0)
		{
			ms_slotIndices.PushAndGrow(i);
		}
	}
	ms_slotIndices.QSort(0, -1, CompareNameCB);
	for(s32 i=0; i<ms_slotIndices.size(); i++)
	{	
		strLocalIndex slotIndex = strLocalIndex(ms_slotIndices[i]);
		fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(slotIndex);
		Assert(pDef);
		ms_apszDebugSlotNames.PushAndGrow(pDef->m_name.GetCStr());
	}

	bank.PushGroup("Bounds Debug");
	bank.AddCombo("Show collision", &ms_mode, MODE_TOTAL, &apszModes[0]);
	bank.AddCombo("Static Bounds", &ms_selectedBoundsComboIndex, ms_apszDebugSlotNames.GetCount(), &ms_apszDebugSlotNames[0]);
	bank.AddToggle("Display mover / weapon breakdown summary", &ms_bDisplayMoverWeaponBreakdown);
	g_StaticBoundsStore.InitWidgets(bank);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update any debug draw for active widgets
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::Update()
{
	switch (ms_mode)
	{
	case MODE_COLLISION_UNDER_MOUSE:
		DisplayCollisionUnderMouse();
		break;
	case MODE_COLLISION_FROM_LIST:
		DisplayCollisionInList();
		break;
	default:
		break;
	}

	if (ms_bDisplayMoverWeaponBreakdown)
	{
		DisplayWeaponMoverBreakdownSummary();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayCollisionUnderMouse
// PURPOSE:		displays the name of the entity or bounds under the mouse
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::DisplayCollisionUnderMouse()
{
	CEntity* pEntity = CDebugScene::GetEntityUnderMouse();
	if (pEntity)
	{
		if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
		{
			strLocalIndex slotIndex = strLocalIndex(pEntity->GetIplIndex());
			if (slotIndex.Get()>=0)
			{
				DrawStaticBounds(slotIndex, pEntity);
			}
		}
		else
		{
			grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white),
				"COLLISION %s: ENTITY (model:%s entaddr:%p", apszModes[ms_mode], pEntity->GetModelName(), pEntity);

			phInst* pInst = pEntity->GetCurrentPhysicsInst();
			phBound* pBound = pInst->GetArchetype()->GetBound();
			Mat34V vMtx = pInst->GetMatrix();
			PGTAMATERIALMGR->GetDebugInterface().UserRenderBound(pInst, pBound, vMtx, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayCollisionInList
// PURPOSE:		display info / debug draw for selected bounds file from list, if loaded
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::DisplayCollisionInList()
{
	strLocalIndex slotIndex = strLocalIndex(ms_slotIndices[ms_selectedBoundsComboIndex]);
	if (slotIndex.Get() >= 0)
	{
		DrawStaticBounds(slotIndex, NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawStaticBounds
// PURPOSE:		draws some debug draw and stats for a specified static bounds slot index
//				(and an optional corresponding entity)
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::DrawStaticBounds(strLocalIndex slotIndex, CEntity* pEntity)
{
	fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(strLocalIndex(slotIndex));
	u8 assetType = g_StaticBoundsStore.GetBoxStreamer().GetAssetType(slotIndex.Get());

	if (pDef && pDef->m_pObject)
	{
		fwBoundData* pDefData = g_StaticBoundsStore.GetSlotData(slotIndex.Get());
		Assertf(pDefData, "This is really bad! Unable to find fwBoundData for fwBoundDef %d", slotIndex.Get());

		for (s32 i=0; i<pDefData->m_physInstCount; i++)
		{
			phInst* pInst = pDefData->m_physicsInstArray[i];

			if (!pEntity || pInst==pEntity->GetCurrentPhysicsInst())
			{
				// debug draw the bounds polys, within a certain radius
				phBound* pBound = pInst->GetArchetype()->GetBound();
				Mat34V vMtx = pInst->GetMatrix();
				PGTAMATERIALMGR->GetDebugInterface().UserRenderBound(pInst, pBound, vMtx, false);

				// draw bounding box
				grcDebugDraw::BoxAxisAligned(pBound->GetBoundingBoxMin(), pBound->GetBoundingBoxMax(), Color_red, false);

				// draw some other debug info
				grcDebugDraw::AddDebugOutputEx(
					false, Color32(Color_white),
					"COLLISION %s: STATIC BOUNDS (name:%s inst:%d/%d slot:%d type:%s entaddr:%p)",
					apszModes[ms_mode],
					pDef->m_name.GetCStr(),
					i+1, pDefData->m_physInstCount,
					slotIndex,
					fwBoxStreamer::apszBoxStreamerAssetTypes[assetType],
					pEntity
				);				
			}
		}
	
		strIndex streamingIndex = g_StaticBoundsStore.GetStreamingIndex(slotIndex);
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
		grcDebugDraw::AddDebugOutputEx(
			false, Color32(Color_white),
			"... total size of %s: %dK VRAM and %dK MAIN)",
			pDef->m_name.GetCStr(),
			info.ComputePhysicalSize(streamingIndex, true) >> 10,
			info.ComputeVirtualSize(streamingIndex, true) >> 10
		);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CompareNameCB
// PURPOSE:		compare two slots by name, for qsort purposes
//////////////////////////////////////////////////////////////////////////
s32	CBoundsDebug::CompareNameCB(const s32* pIndex1, const s32* pIndex2)
{
	fwBoundDef* pDef1 = g_StaticBoundsStore.GetSlot(strLocalIndex(*pIndex1));
	fwBoundDef* pDef2 = g_StaticBoundsStore.GetSlot(strLocalIndex(*pIndex2));
	Assert(pDef1 && pDef2);
	const char* pszName1 = pDef1->m_name.GetCStr();
	const char* pszName2 = pDef2->m_name.GetCStr();
	char achName1[256];
	char achName2[256];
	ASSET.BaseName(achName1, 32, ASSET.FileName(pszName1));
	ASSET.BaseName(achName2, 32, ASSET.FileName(pszName2));
	return stricmp(achName1, achName2);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayWeaponMoverBreakdownSummary
// PURPOSE:		prints some stats about all static bounds in the store
//////////////////////////////////////////////////////////////////////////
void CBoundsDebug::DisplayWeaponMoverBreakdownSummary()
{
	enum
	{
		STAT_NUM_SLOTS_TOTAL = 0,
		STAT_NUM_SLOTS_LOADED,
		STAT_NUM_INSTS_LOADED,
		STAT_MAINRAM_LOADED,
		STAT_MAINRAM_TOTAL,

		STAT_TOTAL
	};
	s32 aStatWeapon[STAT_TOTAL];
	s32 aStatMover[STAT_TOTAL];
	for (s32 i=0; i<STAT_TOTAL; i++)
	{
		aStatMover[i] = 0;
		aStatWeapon[i] = 0;
	}

	s32 sizeOfStore = g_StaticBoundsStore.GetCount();
	s32 totalUsedSlots = 0;
	
	// collect stats
	for (s32 i=0; i<sizeOfStore; i++)
	{
		fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(strLocalIndex(i));
		if (pDef && pDef->m_name.GetLength()>0)
		{
			totalUsedSlots++;

			strIndex streamingIndex = g_StaticBoundsStore.GetStreamingIndex(strLocalIndex(i));
			strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);

			s32* statArray = NULL;
	
			switch (g_StaticBoundsStore.GetBoxStreamer().GetAssetType(i))
			{
			case fwBoxStreamerAsset::ASSET_STATICBOUNDS_MOVER:
				statArray = aStatMover;
				break;
			case fwBoxStreamerAsset::ASSET_STATICBOUNDS_WEAPONS:
				statArray = aStatWeapon;
				break;
			default:
				Assert(false);
				break;
			}

			if (statArray)
			{
				statArray[STAT_NUM_SLOTS_TOTAL] += 1;
				statArray[STAT_MAINRAM_TOTAL] += (info.ComputeVirtualSize(streamingIndex, true) >> 10);

				if (pDef->m_pObject != NULL)
				{
					fwBoundData* pDefData = g_StaticBoundsStore.GetSlotData(i);
					Assertf(pDefData, "This is really bad! Unable to find fwBoundData for fwBoundDef %d", i);

					statArray[STAT_NUM_SLOTS_LOADED] += 1;
					statArray[STAT_NUM_INSTS_LOADED] += pDefData->m_physInstCount;
					statArray[STAT_MAINRAM_LOADED] += (info.ComputeVirtualSize(streamingIndex, true) >> 10);
				}
			}
		}
	}

	// print out stats
	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "STATICBOUNDSSTORE: poolsize=%d, totalslots=%d", sizeOfStore, totalUsedSlots);
	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "MOVER: totalslots=%d, loadedslots=%d, numloadedinsts=%d",
		aStatMover[STAT_NUM_SLOTS_TOTAL], aStatMover[STAT_NUM_SLOTS_LOADED], aStatMover[STAT_NUM_INSTS_LOADED]);
	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "MOVER: memtotal=%dK, memloaded=%dK",
		aStatMover[STAT_MAINRAM_TOTAL], aStatMover[STAT_MAINRAM_LOADED]);

	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "WEAPON: totalslots=%d, loadedslots=%d, numloadedinsts=%d",
		aStatWeapon[STAT_NUM_SLOTS_TOTAL], aStatWeapon[STAT_NUM_SLOTS_LOADED], aStatWeapon[STAT_NUM_INSTS_LOADED]);
	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "WEAPON: memtotal=%dK, memloaded=%dK",
		aStatWeapon[STAT_MAINRAM_TOTAL], aStatWeapon[STAT_MAINRAM_LOADED]);
}

#endif	//__BANK
