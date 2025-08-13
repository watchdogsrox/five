/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/ipl/IplCullBoxEditor.h
// PURPOSE : debug tools to allow management and editing of map cull boxes
// AUTHOR :  Ian Kiigan
// CREATED : 05/04/2011
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_IPL_IPLCULLBOXEDITOR_H_
#define _SCENE_IPL_IPLCULLBOXEDITOR_H_

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "debug/UiGadget/UiGadgetList.h"

class CIplCullBoxContainer;

class CIplCullBoxEditor
{
public:
	static void InitWidgets(bkBank& bank);
	static void UpdateDebug();

private:
	static void DisplayBoxes();
	static void DisplayCulledIpls();
	static void DisplayIntersectingBoxes();
	static void UiGadgetsCB();
	static void UpdateBoxCellCB(CUiGadgetText* pResult, u32 row, u32 col, void *extraCallbackData );
	static void UpdateContainerCellCB(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void CreateBoxAtCameraPosCB();
	static void DeleteCurrentBoxCB();
	static void ToggleBoxEnabledCB();
	static void DuplicateCB();
	static void FindByNameCB();
	static void FindNextCB();
	static void SetNameCB();
	static void RefreshTweakVals();
	static void Load();
	static void CreateFromCurrentBox();
	static void GenerateCullList();
	static int	CompareContainerNameCB(const CIplCullBoxContainer* pContainer1, const CIplCullBoxContainer* pContainer2);
	static void EnableCullingCB();
	static void CullAllCB();
	static void ClearAllCB();
	
	static char ms_achCurrentBoxName[];
	static char ms_achSearchBoxName[];
	static bool ms_bDisplayAllBoxes;
	static bool ms_bDisplayCulled;
	static bool ms_bDisplayIntersectingBoxes;
	static float ms_fPosX, ms_fPosY, ms_fPosZ;
	static float ms_fSizeX, ms_fSizeY, ms_fSizeZ;
	static u32 ms_lastIndex;
	static CUiGadgetSimpleListAndWindow* ms_pBoxListWindow;
	static CUiGadgetSimpleListAndWindow* ms_pContainerListWindow;

	enum
	{
		DEFAULT_BOX_SIZE	= 20,
		BOX_NAME_MAX		= 256
	};
};

#endif	//__BANK

#endif	//_SCENE_IPL_IPLCULLBOXEDITOR_H_