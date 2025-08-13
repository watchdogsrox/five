/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    control/GarageEditor.h
// PURPOSE : debug tools to allow management and editing of garages
// AUTHOR :  Flavius Alecu
// CREATED : 17/11/2011
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _GARAGE_EDITOR_H_
#define _GARAGE_EDITOR_H_

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "control/garages.h"
#include "debug/UiGadget/UiGadgetList.h"

class CIplCullBoxContainer;

class CGarageEditor
{
public:
	static void InitWidgets(bkBank& bank);
	static void UpdateDebug();
private:
	static void SelectedBoxCallback();
	static void CreateBoxAtCameraPosCB();
	static void DeleteCurrentBoxCB();
	static void ResetCurrentGarageBBCallback();
	static void ClearSelectedGarageCB();
	static void ClearSelectedGarageOfPedsCB();
	static void ClearSelectedGarageOfVehiclesCB();
	static void ClearSelectedGarageOfObjectsCB();


	static void SnapDelta2PerpendicularToDelta1CB();
	static void SnapDelta1PerpendicularToDelta2CB();


	static void DisplayGarages();
	static void UiGadgetsCB();
	static void UpdateBoxCellCB(CUiGadgetText* pResult, u32 row, u32 col, void* extraCallbackData );
	static void CreateGarageAtCameraPosCB();
	static void DeleteCurrentGarageCB();
	static void SetNameCB();
	static void SetOwnerCB();
	static u32  GetOwnerIDXFromHash(u32 hash);
	static u32	GetOwnerHashFromIDX(u32 idx);

	static void EnclosedGarageFlagSetCB();
	static void MPGarageFlagSetCB();
	static void ExteriorFlagSetCB();
	static void InteriorFlagSetCB();

	static void RefreshTweakVals();
	static void Save();
	static void Load();
	
	static char ms_achCurrentGarageName[];
	static bool ms_bDisplayAllGarages;
	static bool ms_bDisplayBoundingBoxes;
	static bool ms_drawClearObjectsSphere;
	static bool ms_drawCentreSphere;
	static float ms_baseX, ms_baseY, ms_baseZ;
	static float ms_delta1X, ms_delta1Y;
	static float ms_delta2X, ms_delta2Y;
	static float ms_ceilingZ;
	static s32 ms_type;
	static int ms_permittedVehicleType;
	static bool ms_startWithVehicleSavingEnable;
	static bool ms_vehicleSavingEnable;
	static bool ms_useLineIntersection;
	static CUiGadgetSimpleListAndWindow* ms_pBoxListWindow;
	static bool ms_bDisplayGarageDebugInfo;
	static bool ms_bDisplayBoundingVolumePoints;

	static int ms_selectedBox;

	static bool ms_debugTestPeds;
	static bool ms_debugTestVehicles;
	static bool ms_debugTestObjects;

	static int	ms_OwnerSelection;			// ID after selection via combo box

	static u8	ms_SelectedBoxPulse;		// To pulse the line drawing for the garage, 'cos I just can't see 'em otherwise.

	static bool ms_bEnclosedGarageFlag;
	static bool ms_bMPGarageFlag;
	static bool	ms_bExteriorFlag;
	static bool	ms_bInteriorFlag;

	static bool ms_drawSmallSpheres;
	static bool ms_drawLargeSpheres;
	static bool ms_drawRectLines;
	static bool ms_drawBox2DTest;


	enum
	{
		DEFAULT_GARAGE_WIDTH	= 3,
		DEFAULT_GARAGE_HEIGHT	= 3,
		DEFAULT_GARAGE_DEPTH	= 4,
		GARAGE_NAME_MAX			= 256
	};
};

#endif	//__BANK

#endif	//_GARAGE_EDITOR_H_
