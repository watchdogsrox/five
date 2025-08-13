#ifndef __PC_GAMEPAD_CALIBRATION_H__
#define __PC_GAMEPAD_CALIBRATION_H__

#if RSG_PC

#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"

class CScaleformMovieWrapper;

struct CalibComplexObject
{
	enum Enum
	{
		Root = 0,

		Content,
		StageText,
		CompleteText,
		PromptText,
		PromptText2,

		Noof
	};
};

struct CalibStage
{
	enum Enum
	{
		Loading,
		SelectDevice,
		Calibrate,
		TestAndConfirm,

		Noof
	};
};

//////////////////////////////////////////////////////////////////////////
class CPCGamepadCalibration
{
public:
	static void SetActive(bool bValue) { ms_bActive = bValue; }
	static bool IsActive() { return ms_bActive; }
	static bool IsLoading() { return ms_bLoading; }
	static void LoadCalibrationMovie();
	static void RemoveCalibrationMovie();
	static bool HasCompleted();
	static void Render();
	static void Update();
	static void Init();
private:

	static bool HasSelectedBack();
	static bool HasSelectedExit();

	static void UpdateSelectDevice();
	static void UpdateCalibration();
	static void UpdateTestAndConfirm();

	static bool ConstructCalibButtonString();
	static bool ConstructConfirmStrings();
	static bool PromptStringConstruct(const char* gameTextString, s8 buttonIndex, bool secondPrompt = false, bool clearOld = true, bool hasFill = false);

	static void DrawInstructionalButtons();
	static void ClearInstructionalButtons();

	static void BackOutOfCalibrationMovie();

	static CScaleformMovieWrapper ms_MovieWrapper;
	static bool ms_bActive;
	static bool ms_bLoading;
	static bool ms_bHasCompleted;
	static bool ms_bHasHoldCompleted;
	static bool ms_bPromptForExist;

	static s8 ms_currentButtonIndex;

	static CComplexObject ms_ComplexObject[CalibComplexObject::Noof];

	static CalibStage::Enum ms_CalibStage;

	static u32 ms_acceptHoldTimer;
	static u32 ms_declineHoldTimer;

	static s32 ms_ButtonMovieId;
};

#endif // RSG_PC

#endif // __PC_GAMEPAD_CALIBRATION_H__
