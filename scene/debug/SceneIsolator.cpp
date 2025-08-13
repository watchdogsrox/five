/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneIsolator.cpp
// PURPOSE : debug tool for isolating entries in PVS and displaying info etc
// AUTHOR :  Ian Kiigan
// CREATED : 05/07/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/SceneIsolator.h"

SCENE_OPTIMISATIONS()

#if __BANK
#include "bank/bank.h" 
#include "bank/bkmgr.h"
#include "debug/debugscene.h"
#include "debug/GtaPicker.h"
#include "grcore/debugdraw.h"
#include "fwmaths/rect.h"
#include "fwmaths/rect.h"
#include "rmcore/drawable.h"
#include "fragment/type.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/world/EntityContainer.h"
#include "fwscene/world/entitydesc.h"
#include "grcore/viewport.h"
#include "grcore/viewport.h"
#include "input/mouse.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "renderer/sprite2d.h"
#include "scene/entity.h"
#include "system/controlmgr.h"
#include "text/text.h"
#include "text/textconversion.h"
#include "vector/color32.h"
#include "vector/color32.h"


#define SCREEN_COORDS(x,y) Vector2( ( static_cast<float>(x) / grcViewport::GetDefaultScreen()->GetWidth() ), ( static_cast<float>(y) / grcViewport::GetDefaultScreen()->GetHeight() ) )
#define RESULTSBOX_ENTRY_W									(300)
#define RESULTSBOX_ENTRY_H									(20)
#define RESULTSBOX_W										(RESULTSBOX_ENTRY_W+10)
#define RESULTSBOX_BORDER									(2)
#define RESULTSBOX_SCROLLBAR_OFFSETX						(RESULTSBOX_ENTRY_W + 2)
#define RESULTSBOX_SCROLLBAR_OFFSETY						(RESULTSBOX_ENTRY_H)
#define RESULTSBOX_SCROLLBAR_W								(8)

bool CSceneIsolator::ms_bEnabled = false;
bool CSceneIsolator::ms_bLocked = false;
bool CSceneIsolator::ms_bOnlyShowSelected = false;
s32 CSceneIsolator::ms_nNumSlots = 10;
Vector2 CSceneIsolator::ms_vResultsBoxPos;
Vector2 CSceneIsolator::ms_vClickDragDelta;
bool CSceneIsolator::ms_bMoveResultsBox = false;
s32 CSceneIsolator::ms_nSelectorIndex = 0;
s32 CSceneIsolator::ms_nNumResults = 0;
s32 CSceneIsolator::ms_nListOffset = 0;
s32 CSceneIsolator::ms_nIndexInList = 0;
CEntity* CSceneIsolator::ms_pCurrent = NULL;
s32 CSceneIsolator::ms_index = -1;
atArray<CEntity*> CSceneIsolator::ms_entities;
bool CSceneIsolator::ms_bShowDescBoundingBox = false;
spdAABB CSceneIsolator::ms_descAabb;


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		adds rag widgets for pvs isolator
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::AddWidgets(bkBank* pSceneBank)
{
	// PVS selector tool
	Assert(pSceneBank);
	pSceneBank->PushGroup("PVS Entry Isolator",		false);
	pSceneBank->AddToggle("ENABLE",					&ms_bEnabled);
	pSceneBank->AddToggle("Lock PVS",				&ms_bLocked);
	pSceneBank->AddToggle("Only draw selected",		&ms_bOnlyShowSelected);
	pSceneBank->AddToggle("Show EntityDesc aabb",	&ms_bShowDescBoundingBox);
	pSceneBank->AddButton("Sort by name",			SortByName);
	pSceneBank->AddButton("Sort by size",			SortByCost);
	pSceneBank->AddButton("Set selected in picker",	SetSelectedInPicker);
	pSceneBank->PopGroup();

	CSceneIsolator::Init();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		called before adding PVS entries, resets the entity list
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::Start()
{
	ms_entities.Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Add
// PURPOSE:		adds PVS entry to stored list
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::Add(CEntity* pEntity)
{
	Assert(!ms_bLocked);

	if (pEntity)
	{
		ms_entities.PushAndGrow(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		called after adding PVS entries, resects the selected index etc
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::Stop()
{
	// current selection is invalid so find something new
	if (ms_entities.GetCount()>0)
	{
		ms_index = 0;
		ms_pCurrent = ms_entities[0];
	}
	else
	{
		ms_index = -1;
		ms_pCurrent = NULL;
	}

	ms_nNumResults = ms_entities.GetCount();
	ms_nSelectorIndex = 0;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:		draw debug stuff, aabb for selected entry etc
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::Draw()
{
	if (!ms_bLocked || !ms_bEnabled || ms_entities.GetCount()==0) { return; }

	if (ms_pCurrent)
	{
		// draw aabb
		Matrix34 m = MAT34V_TO_MATRIX34(ms_pCurrent->GetMatrix());
		grcDebugDraw::Axis(m, 1.0);
		CDebugScene::DrawEntityBoundingBox(ms_pCurrent, Color32(255, 0, 0, 255));

		if (ms_bShowDescBoundingBox && ms_pCurrent)
		{
			grcDebugDraw::BoxAxisAligned(ms_descAabb.GetMin(), ms_descAabb.GetMax(), Color32(0, 0, 255, 255), false);
		}

		// draw scrollbox with entities listed
		DrawResultsBox();

		CText::Flush();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawResults
// PURPOSE:		draws the selector / results box showing PVS list entries
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::DrawResultsBox()
{
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale( Vector2( 0.2f, 0.2f ) );

	u32 resultsBoxH = (ms_nNumSlots+1)*RESULTSBOX_ENTRY_H;

	fwRect borderRect(
		ms_vResultsBoxPos.x-RESULTSBOX_BORDER,
		ms_vResultsBoxPos.y+resultsBoxH+RESULTSBOX_BORDER,
		ms_vResultsBoxPos.x+RESULTSBOX_W+RESULTSBOX_BORDER,
		ms_vResultsBoxPos.y-RESULTSBOX_BORDER
		);

	fwRect panelRect(
		ms_vResultsBoxPos.x,
		ms_vResultsBoxPos.y+resultsBoxH,
		ms_vResultsBoxPos.x+RESULTSBOX_ENTRY_W,
		ms_vResultsBoxPos.y+RESULTSBOX_ENTRY_H
		);


	fwRect selectorRect(
		ms_vResultsBoxPos.x,
		ms_vResultsBoxPos.y + 2*RESULTSBOX_ENTRY_H+ms_nSelectorIndex*RESULTSBOX_ENTRY_H,
		ms_vResultsBoxPos.x+RESULTSBOX_ENTRY_W,
		ms_vResultsBoxPos.y+RESULTSBOX_ENTRY_H+ms_nSelectorIndex*RESULTSBOX_ENTRY_H
		);

	// draw background panel
	Color32 borderColour(255, 255, 255, 200);
	Color32 panelColour(180, 180, 180, 255);
	CSprite2d::DrawRectSlow(borderRect, borderColour);
	CSprite2d::DrawRectSlow(panelRect, panelColour);

	if (ms_nNumResults>0)
	{
		// draw selector
		Color32 selectorColour(0, 255, 255, 255);
		CSprite2d::DrawRectSlow(selectorRect, selectorColour);

		// draw scroll bar
		u32 nScrollBarH = ms_nNumSlots * RESULTSBOX_ENTRY_H;
		if (ms_nNumResults <= ms_nNumSlots)
		{
			fwRect scrollBarRect(
				ms_vResultsBoxPos.x + RESULTSBOX_SCROLLBAR_OFFSETX,												// left
				ms_vResultsBoxPos.y + RESULTSBOX_SCROLLBAR_OFFSETY + nScrollBarH,								// bottom
				ms_vResultsBoxPos.x + RESULTSBOX_SCROLLBAR_OFFSETX + RESULTSBOX_SCROLLBAR_W,					// right
				ms_vResultsBoxPos.y + RESULTSBOX_SCROLLBAR_OFFSETY												// top
				);
			Color32 scrollBarColour(0, 0, 0, 255);
			CSprite2d::DrawRectSlow(scrollBarRect, scrollBarColour);
		}
		else
		{
			float fScrollBarHeight = (float) ms_nNumSlots / (float) ms_nNumResults;
			fScrollBarHeight *= nScrollBarH;
			float fScrollBarY = (float) ms_nListOffset / (float) ms_nNumResults;
			fScrollBarY *= nScrollBarH;

			fwRect scrollBarRect(
				(float) ms_vResultsBoxPos.x + RESULTSBOX_SCROLLBAR_OFFSETX,										// left
				(float) (ms_vResultsBoxPos.y + RESULTSBOX_SCROLLBAR_OFFSETY + fScrollBarY + fScrollBarHeight),	// bottom
				(float) ms_vResultsBoxPos.x + RESULTSBOX_SCROLLBAR_OFFSETX + RESULTSBOX_SCROLLBAR_W,			// right
				(float) (ms_vResultsBoxPos.y + RESULTSBOX_SCROLLBAR_OFFSETY + fScrollBarY)						// top
				);

			Color32 scrollBarColour(0, 0, 0, 255);
			CSprite2d::DrawRectSlow(scrollBarRect, scrollBarColour);
		}

	}

	// draw title
	char debugText[50];
	sprintf(debugText, "PVS ENTRIES: %d", ms_nNumResults);

	DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
	DebugTextLayout.Render( SCREEN_COORDS( ms_vResultsBoxPos.x, ms_vResultsBoxPos.y + 4 ) , "Name");

	DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
	DebugTextLayout.Render( SCREEN_COORDS( ms_vResultsBoxPos.x+120, ms_vResultsBoxPos.y + 4 ) , "PHYS");

	DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
	DebugTextLayout.Render( SCREEN_COORDS( ms_vResultsBoxPos.x+180, ms_vResultsBoxPos.y + 4 ) , "VIRT");

	DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
	DebugTextLayout.Render( SCREEN_COORDS( ms_vResultsBoxPos.x+240, ms_vResultsBoxPos.y + 4 ) , "Txd Index");

	// draw contents
	if (ms_nNumResults>0)
	{
		for (u32 i=0; i<ms_nNumSlots; i++)
		{
			u32 tmp = ms_nListOffset + i;
			CEntity* pEntity = (tmp < ms_nNumResults) ? ms_entities[tmp] : NULL;
			if ((tmp < ms_nNumResults) && pEntity)
			{
				// draw model name
				DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
				DebugTextLayout.Render( SCREEN_COORDS(ms_vResultsBoxPos.x, ms_vResultsBoxPos.y+4+(i+1)*RESULTSBOX_ENTRY_H) , pEntity->GetBaseModelInfo()->GetModelName());

				// draw phys cost
				sprintf(debugText, "%dK", GetEntityCost(pEntity, SCENEISOLATOR_COST_PHYS)>>10);
				DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
				DebugTextLayout.Render( SCREEN_COORDS(ms_vResultsBoxPos.x + 120, ms_vResultsBoxPos.y+4+(i+1)*RESULTSBOX_ENTRY_H) , debugText);

				// draw virt cost cost
				sprintf(debugText, "%dK", GetEntityCost(pEntity, SCENEISOLATOR_COST_VIRT)>>10);

				DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
				DebugTextLayout.Render( SCREEN_COORDS(ms_vResultsBoxPos.x + 180, ms_vResultsBoxPos.y+4+(i+1)*RESULTSBOX_ENTRY_H) , debugText);

				// draw txd index
				CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
				sprintf(debugText, "%d", pModelInfo->GetAssetParentTxdIndex());

				DebugTextLayout.SetColor( Color32(0, 0, 0, 255));
				DebugTextLayout.Render( SCREEN_COORDS(ms_vResultsBoxPos.x + 240, ms_vResultsBoxPos.y+4+(i+1)*RESULTSBOX_ENTRY_H) , debugText);
			}
		}
	}	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates selector based on mouse wheel, drag etc
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::Update()
{
	if (!ms_bLocked || !ms_bEnabled || ms_entities.GetCount()==0) { return; }

	// check if current selection is not on-screen due to player adjusting the number
	// onscreen slots, and reset if necessary
	if (ms_nSelectorIndex >= ms_nNumSlots)
	{
		ms_nSelectorIndex = ms_nNumSlots-1;
	}
	if ((ms_nNumSlots < ms_nNumResults) && (ms_nListOffset+ms_nNumSlots)>ms_nNumResults)
	{
		ms_nListOffset = 0;
		ms_nIndexInList = 0;
		ms_nSelectorIndex = 0;
	}

	const Vector2 mousePosition = Vector2( static_cast<float>(ioMouse::GetX()), static_cast<float>(ioMouse::GetY()) );
	const s32 mouseWheel = ioMouse::GetDZ();

	// update moving of results box
	if (ms_bMoveResultsBox)
	{
		// already dragging so adjust position of results box
		ms_vResultsBoxPos = mousePosition - ms_vClickDragDelta;

		// check for button release
		if ((ioMouse::GetButtons()&ioMouse::MOUSE_LEFT) == false)
		{
			ms_bMoveResultsBox = false;
		}
	}
	else
	{
		// check for player clicking on title bar
		if (ioMouse::GetButtons()&ioMouse::MOUSE_LEFT)
		{
			fwRect titleRect
				(
				ms_vResultsBoxPos.x,
				ms_vResultsBoxPos.y,
				ms_vResultsBoxPos.x + RESULTSBOX_W,
				ms_vResultsBoxPos.y + RESULTSBOX_ENTRY_H
				);

			if (titleRect.IsInside(mousePosition))
			{
				ms_bMoveResultsBox = true;
				ms_vClickDragDelta = mousePosition - ms_vResultsBoxPos;
			}
		}
	}

	if (mouseWheel != 0)
	{
		// adjust position in list
		s32 newPos = ms_nIndexInList + mouseWheel*-1;
		if (newPos < 0) { newPos = 0; }
		else if (newPos>=ms_nNumResults) { newPos = ms_nNumResults-1; }
		ms_nIndexInList = newPos;
	}
	else if (ioMouse::GetButtons()&ioMouse::MOUSE_LEFT)
	{
		u32 resultsBoxH = (ms_nNumSlots+1)*RESULTSBOX_ENTRY_H;
		fwRect panelRect(
			ms_vResultsBoxPos.x,
			ms_vResultsBoxPos.y+resultsBoxH,
			ms_vResultsBoxPos.x+RESULTSBOX_ENTRY_W,
			ms_vResultsBoxPos.y+RESULTSBOX_ENTRY_H
			);

		// process mouse click
		if (!ms_bMoveResultsBox)
		{
			if (mousePosition.x >= panelRect.left && mousePosition.x <= panelRect.right && mousePosition.y >= panelRect.top && mousePosition.y <= panelRect.bottom)
			{
				u32 possibleEntry = (u32) ((mousePosition.y-panelRect.top)/(float)RESULTSBOX_ENTRY_H);
				if ((ms_nListOffset + possibleEntry) < ms_nNumResults)
				{
					ms_nIndexInList = ms_nListOffset+possibleEntry;
				}
			}
		}
	}

	// page up/down etc, keyboard input
	s32 newPos = ms_nIndexInList;
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SUBTRACT, KEYBOARD_MODE_DEBUG))
	{
		newPos -= ms_nNumSlots;
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_ADD, KEYBOARD_MODE_DEBUG))
	{
		newPos += ms_nNumSlots;
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG))
	{
		newPos--;
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG))
	{
		newPos++;
	}
	if (newPos < 0) { newPos = 0; }
	else if (newPos>=ms_nNumResults) { newPos = ms_nNumResults-1; }
	ms_nIndexInList = newPos;

	// adjust viewport
	if (ms_nIndexInList < ms_nListOffset)
	{
		// scroll up
		ms_nListOffset = ms_nIndexInList;
	}
	else if (ms_nIndexInList > (ms_nListOffset+ms_nNumSlots-1))
	{
		// scroll down
		ms_nListOffset += (ms_nIndexInList - (ms_nListOffset+ms_nNumSlots-1));
	}

	// adjust selector
	ms_nSelectorIndex = ms_nIndexInList - ms_nListOffset;

	ms_index = ms_nIndexInList;
	ms_pCurrent = ms_entities[ms_index];

	// update descriptor aabb
	if (ms_bShowDescBoundingBox && ms_pCurrent)
	{
		fwBaseEntityContainer* pContainer = ms_pCurrent->GetOwnerEntityContainer();
		if (pContainer)
		{
			u32 index = pContainer->GetEntityDescIndex(ms_pCurrent);
			if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)

			{
				pContainer->GetApproxBoundingBox(index, ms_descAabb);
			}
		}
	}
}

typedef CEntity * _ListType;
int CbCompareByModelName(const _ListType * ppEntity1, const _ListType * ppEntity2)
{
	const CEntity* pEntity1 = *ppEntity1;
	const CEntity* pEntity2 = *ppEntity2;
	const char* pszName1 = CModelInfo::GetBaseModelInfoName(pEntity1->GetModelId());
	const char* pszName2 = CModelInfo::GetBaseModelInfoName(pEntity2->GetModelId());
	return stricmp(pszName1, pszName2);
}

int CbCompareByCost(const _ListType * ppEntity1, const _ListType * ppEntity2)
{
	const CEntity* pEntity1 = *ppEntity1;
	const CEntity* pEntity2 = *ppEntity2;
	int cost1 = CSceneIsolator::GetEntityCost(pEntity1, SCENEISOLATOR_COST_TOTAL);
	int cost2 = CSceneIsolator::GetEntityCost(pEntity2, SCENEISOLATOR_COST_TOTAL);
	return cost2 - cost1;
}

void CSceneIsolator::SortByName()
{
	if (ms_entities.GetCount() == 0) { return; }

	ms_entities.QSort(0, -1, CbCompareByModelName);

	// reset current selected
	ms_nListOffset = 0;
	ms_nIndexInList = 0;
	ms_nSelectorIndex = 0;
	ms_index = 0;
	ms_pCurrent = ms_entities[0];
}

void CSceneIsolator::SortByCost()
{
	if (ms_entities.GetCount() == 0) { return; }

	ms_entities.QSort(0, -1, CbCompareByCost);

	// reset current selected
	ms_nListOffset = 0;
	ms_nIndexInList = 0;
	ms_nSelectorIndex = 0;
	ms_index = 0;
	ms_pCurrent = ms_entities[0];
}

int CSceneIsolator::GetEntityCost(const CEntity* pEntity, int eCost)
{
	int cost = 0;

	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		s32 txdModuleId = g_TxdStore.GetStreamingModuleId();
		s32 drawableModuleId = g_DrawableStore.GetStreamingModuleId();
		s32 fragModuleId = g_FragmentStore.GetStreamingModuleId();
		s32 dwdModuleId = g_TxdStore.GetStreamingModuleId();

		strStreamingModule* pTxdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(txdModuleId);
		strStreamingModule* pDrawableStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(drawableModuleId);
		strStreamingModule* pFragStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fragModuleId);
		strStreamingModule* pDwdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(dwdModuleId);

		//////////////////////////////////////////////////////////////////////////
		// update txd costs
		strLocalIndex txdDictIndex = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());
		if (txdDictIndex!=-1)
		{
			strIndex txdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(txdDictIndex);
			strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(txdStreamingIndex);

			switch (eCost)
			{
			case SCENEISOLATOR_COST_PHYS:
				cost += info.ComputePhysicalSize(txdStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_VIRT:
				cost += info.ComputeVirtualSize(txdStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_TOTAL:
				cost += info.ComputePhysicalSize(txdStreamingIndex, true);
				cost += info.ComputeVirtualSize(txdStreamingIndex, true);
				break;
			default:
				break;
			}

			// check parent txd
			strLocalIndex parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(txdDictIndex));
			while (parentTxdIndex!=-1)
			{
				// stat hasn't been counted yet
				strIndex parentTxdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(parentTxdIndex);
				strStreamingInfo& parentInfo = *strStreamingEngine::GetInfo().GetStreamingInfo(parentTxdStreamingIndex);
				switch (eCost)
				{
				case SCENEISOLATOR_COST_PHYS:
					cost += parentInfo.ComputePhysicalSize(parentTxdStreamingIndex, true);
					break;
				case SCENEISOLATOR_COST_VIRT:
					cost += parentInfo.ComputeVirtualSize(parentTxdStreamingIndex, true);
					break;
				case SCENEISOLATOR_COST_TOTAL:
					cost += parentInfo.ComputePhysicalSize(parentTxdStreamingIndex, true);
					cost += parentInfo.ComputeVirtualSize(parentTxdStreamingIndex, true);
					break;
				default:
					break;
				}
				parentTxdIndex = g_TxdStore.GetParentTxdSlot(parentTxdIndex);
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update dwd costs
		strLocalIndex dwdDictIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLEDICTIONARY) ? strLocalIndex(pModelInfo->GetDrawDictIndex()) : strLocalIndex(INVALID_DRAWDICT_IDX);
		if (dwdDictIndex !=INVALID_DRAWDICT_IDX)
		{
			// stat hasn't been counted yet
			strIndex dwdStreamingIndex = pDwdStreamingModule->GetStreamingIndex(dwdDictIndex);
			strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(dwdStreamingIndex);
			
			switch (eCost)
			{
			case SCENEISOLATOR_COST_PHYS:
				cost += info.ComputePhysicalSize(dwdStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_VIRT:
				cost += info.ComputeVirtualSize(dwdStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_TOTAL:
				cost += info.ComputePhysicalSize(dwdStreamingIndex, true);
				cost += info.ComputeVirtualSize(dwdStreamingIndex, true);
				break;
			default:
				break;
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update drawable costs
		strLocalIndex drawableIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE) ? strLocalIndex(pModelInfo->GetDrawableIndex()) : strLocalIndex(INVALID_DRAWABLE_IDX);
		if (drawableIndex!=INVALID_DRAWABLE_IDX)
		{
			// stat hasn't been counted yet
			strIndex drawableStreamingIndex = pDrawableStreamingModule->GetStreamingIndex(drawableIndex);
			strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(drawableStreamingIndex);
			
			switch (eCost)
			{
			case SCENEISOLATOR_COST_PHYS:
				cost += info.ComputePhysicalSize(drawableStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_VIRT:
				cost += info.ComputeVirtualSize(drawableStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_TOTAL:
				cost += info.ComputePhysicalSize(drawableStreamingIndex, true);
				cost += info.ComputeVirtualSize(drawableStreamingIndex, true);
				break;
			default:
				break;
			}
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// update frag costs
		strLocalIndex fragIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_FRAGMENT) ? strLocalIndex(pModelInfo->GetFragmentIndex()) : strLocalIndex(INVALID_FRAG_IDX);
		if (fragIndex!=INVALID_FRAG_IDX)
		{
			// stat hasn't been counted yet
			strIndex fragStreamingIndex = pFragStreamingModule->GetStreamingIndex(fragIndex);
			strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(fragStreamingIndex);
			
			switch (eCost)
			{
			case SCENEISOLATOR_COST_PHYS:
				cost += info.ComputePhysicalSize(fragStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_VIRT:
				cost += info.ComputeVirtualSize(fragStreamingIndex, true);
				break;
			case SCENEISOLATOR_COST_TOTAL:
				cost += info.ComputePhysicalSize(fragStreamingIndex, true);
				cost += info.ComputeVirtualSize(fragStreamingIndex, true);
				break;
			default:
				break;
			}
		}
	}
	return cost;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetSelectedInPicker
// PURPOSE:		attemps to set the selected pvs entry as the current entity in scene picker
//////////////////////////////////////////////////////////////////////////
void CSceneIsolator::SetSelectedInPicker()
{
	CEntity* pEntity = GetCurrentEntity();
	if (pEntity)
	{
		g_PickerManager.AddEntityToPickerResults(pEntity, true, true);
	}
}

#endif //__BANK
