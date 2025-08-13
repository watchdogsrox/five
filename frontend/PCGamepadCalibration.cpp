#include "Frontend/PCGamepadCalibration.h"

#if RSG_PC

#include "grprofile/timebars.h"
#include "Frontend/PauseMenu.h"
#include "System/controlMgr.h"
#include "Frontend/BusySpinner.h"
#include "Text/TextFormat.h"
#include "Text/TextConversion.h"
#include "rline/rlpc.h"

#define GAMEPAD_FILENAME "GAMEPAD_CALIBRATION"
#define BUTTONMENU_FILENAME "store_instructional_buttons"

FRONTEND_OPTIMISATIONS()

static u32 ms_InputLookup[]  = { rage::InputType::INPUT_FRONTEND_ACCEPT,
						  rage::InputType::INPUT_FRONTEND_CANCEL,
						  rage::InputType::INPUT_FRONTEND_X,
						  rage::InputType::INPUT_FRONTEND_Y,
						  rage::InputType::INPUT_FRONTEND_LB,
						  rage::InputType::INPUT_FRONTEND_LT,
						  rage::InputType::INPUT_FRONTEND_RB,
						  rage::InputType::INPUT_FRONTEND_RT,
						  rage::InputType::INPUT_FRONTEND_PAUSE,
						  rage::InputType::INPUT_FRONTEND_SELECT,
						  rage::InputType::INPUT_FRONTEND_UP,
						  rage::InputType::INPUT_FRONTEND_DOWN,
						  rage::InputType::INPUT_FRONTEND_LEFT,
						  rage::InputType::INPUT_FRONTEND_RIGHT,
						  rage::InputType::INPUT_FRONTEND_LS,
						  rage::InputType::INPUT_FRONTEND_AXIS_Y,
						  rage::InputType::INPUT_FRONTEND_AXIS_X,
						  rage::InputType::INPUT_FRONTEND_RS,
						  rage::InputType::INPUT_FRONTEND_RIGHT_AXIS_Y,
						  rage::InputType::INPUT_FRONTEND_RIGHT_AXIS_X };

CScaleformMovieWrapper CPCGamepadCalibration::ms_MovieWrapper;
bool CPCGamepadCalibration::ms_bActive = false;
bool CPCGamepadCalibration::ms_bLoading = false;
bool CPCGamepadCalibration::ms_bHasHoldCompleted = false;
bool CPCGamepadCalibration::ms_bHasCompleted = false;
bool CPCGamepadCalibration::ms_bPromptForExist = false;
s8 CPCGamepadCalibration::ms_currentButtonIndex = 0;
CComplexObject CPCGamepadCalibration::ms_ComplexObject[CalibComplexObject::Noof];
CalibStage::Enum CPCGamepadCalibration::ms_CalibStage = CalibStage::Noof;
u32 CPCGamepadCalibration::ms_acceptHoldTimer = 0;
u32 CPCGamepadCalibration::ms_declineHoldTimer = 0;
s32 CPCGamepadCalibration::ms_ButtonMovieId = -1;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::Render()
// PURPOSE: Renders the calibration screen
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::Render()
{
	if (ms_bActive && ms_MovieWrapper.IsActive())
	{
		GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0.0f, 0); 
		ms_MovieWrapper.Render(true);

		// hide instructional buttons if SCUI is showing ...hints are confusing
		if (!g_rlPc.IsUiShowing())
		{
			if ( ms_ButtonMovieId != -1)
			{
				float fTimer = fwTimer::GetSystemTimeStep();
				CScaleformMgr::RenderMovie(ms_ButtonMovieId, fTimer, true, true);
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::HasCompleted
// PURPOSE:	Indicates that the gamepad has now been calibrated
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::HasCompleted()
{
	return ms_bHasCompleted;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::HasSelectedBack
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::HasSelectedBack()
{
	const CControl& control = CControlMgr::GetMainFrontendControl();

	return ms_bActive && control.GetFrontendCancel().IsReleased() && control.GetFrontendCancel().GetLastSource().m_DeviceIndex != control.GetScanningDeviceIndex();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::HasSelectedExit
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::HasSelectedExit()
{
	const CControl& control = CControlMgr::GetMainFrontendControl();

	return ms_bActive && control.GetFrontendEnterExit().IsReleased() && control.GetFrontendEnterExit().GetLastSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::Update
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::Update()
{
	if (ms_bLoading)
	{
		if (ms_MovieWrapper.IsActive())
		{
			if (CScaleformMgr::IsMovieActive(ms_ButtonMovieId))
			{
				ms_bActive = true;
				ms_bLoading = false;

				Init();
			}
		}
	}

	if (ms_bActive == false || HasCompleted())
		return;

	switch (ms_CalibStage)
	{
	case CalibStage::Loading:
	case CalibStage::SelectDevice:
		{
			UpdateSelectDevice();
		}
		break;
	case CalibStage::Calibrate:
		{
			UpdateCalibration();
		}
		break;
	case CalibStage::TestAndConfirm:
		{
			UpdateTestAndConfirm();
		}
		break;
	default : return;
	};

	if (ms_CalibStage == CalibStage::SelectDevice || ms_CalibStage == CalibStage::Calibrate)
	{
		if (HasSelectedBack())
		{
			BackOutOfCalibrationMovie();
		}
		else if (HasSelectedExit())
		{
			ms_bHasCompleted = true;
		}
	}

	// if this is to be used at the start of the game,
	// check out DisplayCalibration::Update() ...you can just use that
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::UpdateSelectDevice
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::UpdateSelectDevice()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();

	if(control.IsGampadDefinitionScanInProgress() == false && ms_CalibStage == CalibStage::Loading)
	{
		control.SetScanningDeviceIndex(ioSource::IOMD_ANY);
		control.ScanGamepadDefinitionSource(static_cast<CControl::GamepadDefinitionSource>(ms_currentButtonIndex));
		ms_CalibStage = CalibStage::SelectDevice;
	}
	else if (control.IsGampadDefinitionScanInProgress() == false)
	{
		control.CancelGampadDefinitionScan();
		control.CalibrateGamepad(control.GetScanningDeviceIndex());

		ms_CalibStage = CalibStage::Calibrate;

		ms_ComplexObject[CalibComplexObject::StageText].SetVisible(true);
		ms_ComplexObject[CalibComplexObject::PromptText2].SetVisible(false);

		// update progress bar
		if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "BAR_POSITION"))
		{
			CScaleformMgr::AddParamFloat(0.0f);
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SHOW_BAR"))
		{
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::EndMethod();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::UpdateCalibration
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::UpdateCalibration()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();

	if(control.IsGampadDefinitionScanInProgress() == false && ms_currentButtonIndex < CControl::NUM_PAD_SOURCES)
	{
		control.ScanGamepadDefinitionSource(static_cast<CControl::GamepadDefinitionSource>(ms_currentButtonIndex));
		++ms_currentButtonIndex;
	}

	if (control.IsGamepadDefinitionSourceConflicting())
	{
		ms_ComplexObject[CalibComplexObject::PromptText2].SetTextField(TheText.Get("MO_INPUT_IN_USE_PMT"));
		ms_ComplexObject[CalibComplexObject::PromptText2].SetVisible(true);
	}
	else
	{
		ms_ComplexObject[CalibComplexObject::PromptText2].SetVisible(false); 
	}

	if (ms_currentButtonIndex >= CControl::NUM_PAD_SOURCES && control.IsGampadDefinitionScanInProgress() == false)
	{
		if(ms_bPromptForExist == false)
		{
			ms_bPromptForExist = true;

			ms_ComplexObject[CalibComplexObject::StageText].SetTextField(TheText.Get("MO_CALIB_COMPLETE_PMT"));
			ms_ComplexObject[CalibComplexObject::CompleteText].SetTextField(TheText.Get("MO_TEST_CALIB_PMT"));

			ms_ComplexObject[CalibComplexObject::PromptText2].SetVisible(true);
			ConstructCalibButtonString();

			if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "END_CALIBRATION"))
			{
				CScaleformMgr::EndMethod();
			}

			ClearInstructionalButtons();

			control.SetInitialDefaultMappings(true);
			control.ResetKeyboardMouseSettings();

			ms_CalibStage = CalibStage::TestAndConfirm;
			ms_acceptHoldTimer = 0;
			ms_declineHoldTimer = 0;
		}
	}
	else
	{
		// show new button
		if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SHOW_BUTTON"))
		{
			CScaleformMgr::AddParamInt(ms_currentButtonIndex-1);
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::EndMethod();
		}

		// construct string with button glyph
		if (!ConstructConfirmStrings())
		{
			ms_ComplexObject[CalibComplexObject::PromptText].SetTextField(NULL);
		}

		const u8 stringLen = 8;
		char stageString[stringLen];
		formatf(stageString, stringLen, "%d/%d", ms_currentButtonIndex, CControl::NUM_PAD_SOURCES);
		ms_ComplexObject[CalibComplexObject::StageText].SetTextField(stageString);
	}

	// update progress bar
	if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "BAR_POSITION"))
	{
		const float progress = Min(1.0f, static_cast<float>(ms_currentButtonIndex -1) / static_cast<float>(CControl::NUM_PAD_SOURCES));
		CScaleformMgr::AddParamFloat(progress);
		CScaleformMgr::EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::UpdateTestAndConfirm
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::UpdateTestAndConfirm()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();

	if (ms_bHasHoldCompleted)
	{
		if (ms_acceptHoldTimer > 0 && control.GetFrontendAccept().IsReleased())
		{
			ms_bHasCompleted = true;
		}
		else if (ms_declineHoldTimer > 0 && control.GetFrontendCancel().IsReleased())
		{
			ms_bHasCompleted = true;
		}

		if (ms_bHasCompleted && ms_ButtonMovieId != INVALID_MOVIE_ID )
		{
			CBusySpinner::UnregisterInstructionalButtonMovie(ms_ButtonMovieId);  // register this "instructional button" movie with the spinner system
			CScaleformMgr::RequestRemoveMovie(ms_ButtonMovieId);
			ms_ButtonMovieId = INVALID_MOVIE_ID;
		}
		return;
	}

	bool leftStickOn = false;
	bool rightStickOn = false;
	for (u32 button = 0; button < CControl::NUM_PAD_SOURCES; ++button)
	{
		bool isPressed = false;
		bool isReleased = false;

		bool isLeftStick = (button == CControl::PAD_AXIS_LY_UP || button == CControl::PAD_AXIS_LX_RIGHT);
		bool isRightStick = (button == CControl::PAD_AXIS_RY_UP || button == CControl::PAD_AXIS_RX_RIGHT );
		if ( isLeftStick || isRightStick )
		{
			if ((isLeftStick && leftStickOn) ||
				(isRightStick && rightStickOn))
				continue;

			float axisValue = control.GetValue(ms_InputLookup[button]).GetNorm();

			// could test against previous, but might be dodgy if something is already being held down entering this page
			// not as bad with buttons, so just deal with this
			isReleased = (axisValue > -0.1f && axisValue < 0.1f);
			if (!isReleased)
			{
				isPressed = true;
				if (isLeftStick)
					leftStickOn = true;
				if (isRightStick)
					rightStickOn = true;
			}
		}
		else
		{
			isPressed = control.GetValue(ms_InputLookup[button]).IsPressed();
			if (!isPressed)
				isReleased = control.GetValue(ms_InputLookup[button]).IsReleased();
		}

		if (isPressed)
		{
			if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SHOW_BUTTON"))
			{
				CScaleformMgr::AddParamInt(button);
				CScaleformMgr::AddParamBool(true);
				CScaleformMgr::AddParamBool(false);
				CScaleformMgr::EndMethod();
			}
		}
		else if (isReleased)
		{
			if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SHOW_BUTTON"))
			{
				CScaleformMgr::AddParamInt(button);
				CScaleformMgr::AddParamBool(false);
				CScaleformMgr::AddParamBool(false);
				CScaleformMgr::EndMethod();
			}
		}
	}


	// hold to exit
	const u32 holdTime = 4000; // 4 seconds

	if (control.GetFrontendAccept().IsDown() && ms_declineHoldTimer == 0)
	{
		// disable cancel anim if accept is pressed
		ms_declineHoldTimer = 0;
		if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_PRESS_STATE"))
		{
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::AddParamInt(1);
			CScaleformMgr::EndMethod();
		}

		if (ms_acceptHoldTimer == 0)
		{
			if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_PRESS_STATE"))
			{
				CScaleformMgr::AddParamBool(true);
				CScaleformMgr::AddParamInt(0);
				CScaleformMgr::EndMethod();
			}
		}

		ms_acceptHoldTimer += fwTimer::GetTimeStepInMilliseconds();
		if (ms_acceptHoldTimer >= holdTime)
		{
			ms_bHasHoldCompleted = true;
			control.EndGamepadDefinitionScan(true);
			control.ResetKeyboardMouseSettings();
		}
	}
	else
	{
		if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_PRESS_STATE"))
		{
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::AddParamInt(0);
			CScaleformMgr::EndMethod();
		}

		ms_acceptHoldTimer = 0;
		
		if (control.GetFrontendCancel().IsDown())
		{
			if (ms_declineHoldTimer == 0)
			{
				if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_PRESS_STATE"))
				{
					CScaleformMgr::AddParamBool(true);
					CScaleformMgr::AddParamInt(1);
					CScaleformMgr::EndMethod();
				}
			}


			ms_declineHoldTimer += fwTimer::GetTimeStepInMilliseconds();
			if (ms_declineHoldTimer >= holdTime)
			{
				ms_bHasHoldCompleted = true;
				control.EndGamepadDefinitionScan(false);
				control.ResetKeyboardMouseSettings();
			}
		}
		else
		{
			ms_declineHoldTimer = 0;				
			
			if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_PRESS_STATE"))
			{
				CScaleformMgr::AddParamBool(false);
				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::EndMethod();
			}
		}
	}

}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::Init
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::Init()
{
	ms_ComplexObject[CalibComplexObject::Root] = CComplexObject::GetStageRoot(ms_MovieWrapper.GetMovieID());

	if (ms_ComplexObject[CalibComplexObject::Root].IsActive())
	{
		// Content
		ms_ComplexObject[CalibComplexObject::Content] = ms_ComplexObject[CalibComplexObject::Root].GetObject("CONTENT");
		if (ms_ComplexObject[CalibComplexObject::Content].IsActive())
		{
			ms_ComplexObject[CalibComplexObject::StageText] = ms_ComplexObject[CalibComplexObject::Content].GetObject("stageTXT");
			if (ms_ComplexObject[CalibComplexObject::StageText].IsActive())
			{
				ms_ComplexObject[CalibComplexObject::StageText].SetTextField(NULL);
			}

			ms_ComplexObject[CalibComplexObject::CompleteText] = ms_ComplexObject[CalibComplexObject::Content].GetObject("completeTXT");
			if (ms_ComplexObject[CalibComplexObject::CompleteText].IsActive())
			{
				ms_ComplexObject[CalibComplexObject::CompleteText].SetTextField(NULL);
			}

			ms_ComplexObject[CalibComplexObject::PromptText] = ms_ComplexObject[CalibComplexObject::Content].GetObject("promptTXT");
			if (ms_ComplexObject[CalibComplexObject::PromptText].IsActive())
			{
				ms_ComplexObject[CalibComplexObject::PromptText].SetTextField(NULL);
			}

			ms_ComplexObject[CalibComplexObject::PromptText2] = ms_ComplexObject[CalibComplexObject::Content].GetObject("promptTXT2");
			if (ms_ComplexObject[CalibComplexObject::PromptText2].IsActive())
			{
				ms_ComplexObject[CalibComplexObject::PromptText2].SetTextField(NULL);
			}
		}
	}

	ms_currentButtonIndex = 0;
	ms_bHasCompleted	  = false;
	ms_bPromptForExist	  = false;

	CControlMgr::GetPlayerMappingControl().StartGamepadDefinitionScan();

	// set up select device page
	ms_CalibStage = CalibStage::Loading;


	char cHtmlFormattedTextString[MAX_CHARS_FOR_TEXT_STRING];
	CTextConversion::TextToHtml(TheText.Get("MO_SELECT_PAD"), cHtmlFormattedTextString, NELEM(cHtmlFormattedTextString));
	ms_ComplexObject[CalibComplexObject::PromptText].SetTextFieldHTML(cHtmlFormattedTextString);

	ms_ComplexObject[CalibComplexObject::PromptText2].SetTextField(TheText.Get("MO_USE_OTHER_WHEN_NAV"));

	ms_ComplexObject[CalibComplexObject::StageText].SetVisible(false);

	ms_bHasHoldCompleted = false;

	DrawInstructionalButtons();

}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::ConstructCalibButtonString
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::ConstructCalibButtonString()
{
	if (PromptStringConstruct(TheText.Get("MO_CALIBRATION_SAVE"), 1, false, false, true))
	{
		if (PromptStringConstruct(TheText.Get("MO_CALIB_DISCARD"), 2, true, false, true))
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::ConstructConfirmStrings
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::ConstructConfirmStrings()
{
	const char* gameTextString = NULL;
	s8 button = ms_currentButtonIndex-1;
	if ( button == CControl::PAD_AXIS_LY_UP || button == CControl::PAD_AXIS_LX_RIGHT || 
		button == CControl::PAD_AXIS_RY_UP || button == CControl::PAD_AXIS_RX_RIGHT )
	{
		gameTextString = TheText.Get("MO_MOVE_HOLD_PMT");
	}
	else
	{
		gameTextString = TheText.Get("MO_PRESS_HOLD_PMT");
	}

	return PromptStringConstruct(gameTextString, ms_currentButtonIndex);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::PromptStringConstruct
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CPCGamepadCalibration::PromptStringConstruct(const char* gameTextString, s8 buttonIndex, bool secondPrompt, bool clearOld, bool hasFill)
{
	if (!gameTextString)
		return false;

	s32 stringLength = static_cast<s32>(strlen(gameTextString));

	s32 endOfFirstString = -1;
	bool lookingForStartOfSecond = false;
	const char* startOfSecondString = NULL;
	for (s32 i = 0; i < stringLength; ++i)
	{
		startOfSecondString = &gameTextString[i];

		if (lookingForStartOfSecond)
		{
			// found start of second string by it having content
			if (*startOfSecondString != '~' || *startOfSecondString != ' ')
				break;
		}

		if (*startOfSecondString == '~')
		{
			if (endOfFirstString == -1)
			{
				endOfFirstString = i;
			}
			else
			{
				lookingForStartOfSecond = true;
			}
		}
	}

	if (endOfFirstString == -1 || !startOfSecondString)
		return false;

	const s32 maxStartStringLength = 256;
	if (endOfFirstString >= maxStartStringLength)
	{
		uiAssertf(true, "CPCGamepadCalibration::PromptStringConstruct - start of string is logner than %d characters. Can't cope.", maxStartStringLength); 
		return false;
	}

	char startString[256];
	// can't have start string for fill buttons (in this case)
	if (hasFill)
		formatf(startString, " ");
	else
		formatf(startString, endOfFirstString+1, "%s", gameTextString);

	if (CScaleformMgr::BeginMethod(ms_MovieWrapper.GetMovieID(), SF_BASE_CLASS_GENERIC, "CONSTRUCT_STRING"))
	{
		CScaleformMgr::AddParamInt(buttonIndex-1);
		CScaleformMgr::AddParamString(startString, false);
		CScaleformMgr::AddParamString(startOfSecondString, false);
		CScaleformMgr::AddParamInt(secondPrompt ? 1 : 0);
		CScaleformMgr::AddParamBool(clearOld);
		CScaleformMgr::AddParamBool(hasFill);

		CScaleformMgr::EndMethod();

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::DrawInstructionalButtons
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::DrawInstructionalButtons()
{
	bool mouseSupport = false;

#if KEYBOARD_MOUSE_SUPPORT
	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "TOGGLE_MOUSE_BUTTONS"))
	{
		CScaleformMgr::AddParamBool(true);
		CScaleformMgr::EndMethod();
		mouseSupport = true;
	}
#endif

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT_EMPTY"))
	{
		CScaleformMgr::EndMethod();
	}

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_CLEAR_SPACE"))
	{
		CScaleformMgr::AddParamInt(200);
		CScaleformMgr::EndMethod();
	}

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
	{
		char buttonText[64] = { 0 };

		IconParams params	 = CTextFormat::DEFAULT_ICON_PARAMS;
		params.m_AllowXOSwap = false;

		CTextFormat::GetInputButtons(INPUT_FRONTEND_CANCEL, buttonText, 64, NULL, 0, params);
		CScaleformMgr::AddParamInt(0);
		CScaleformMgr::AddParamString(buttonText);
		CScaleformMgr::AddParamString(TheText.Get("CMRC_BACK"));

		CScaleformMgr::AddParamBool(mouseSupport);
		CScaleformMgr::AddParamInt(mouseSupport ? INPUT_FRONTEND_CANCEL : -1);

		CScaleformMgr::EndMethod();
	}

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "DRAW_INSTRUCTIONAL_BUTTONS"))
	{            
		CScaleformMgr::EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::ClearInstructionalButtons
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::ClearInstructionalButtons()
{
	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT_EMPTY"))
	{
		CScaleformMgr::EndMethod();
	}

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_CLEAR_SPACE"))
	{
		CScaleformMgr::AddParamInt(200);
		CScaleformMgr::EndMethod();
	}

	if ( CScaleformMgr::BeginMethod(ms_ButtonMovieId, SF_BASE_CLASS_GENERIC, "DRAW_INSTRUCTIONAL_BUTTONS"))
	{            
		CScaleformMgr::EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::LoadCalibrationMovie
// PURPOSE:	streams in the movie
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::LoadCalibrationMovie()
{
	// still loading? pause menu should take care of this, but let's be safe
	if (ms_bLoading)
		return;
	
	// already loaded? something funny going on. let's ignore it as it's probably deleting ...they'll press again
	if (CScaleformMgr::FindMovieByFilename(GAMEPAD_FILENAME) != INVALID_MOVIE_ID)
		return;

	ms_MovieWrapper.CreateMovie(SF_BASE_CLASS_GENERIC, GAMEPAD_FILENAME, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));

	if ( CScaleformMgr::FindMovieByFilename(BUTTONMENU_FILENAME) == INVALID_MOVIE_ID )
	{
		ms_ButtonMovieId = CScaleformMgr::CreateMovie(BUTTONMENU_FILENAME, Vector2(0,0), Vector2(1.0f, 1.0f));
		CBusySpinner::RegisterInstructionalButtonMovie(INVALID_MOVIE_ID);  // register this "instructional button" movie with the spinner system
	}

	ms_bLoading = true;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::BackOutOfCalibrationMovie
// PURPOSE:	Removes the movie
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::BackOutOfCalibrationMovie()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();

	if(--ms_currentButtonIndex >= 1)
	{
		// We use -1 as ms_currentButtonIndex is 1 higher than the current scan.
		control.ClearGamepadDefinitionSource(static_cast<CControl::GamepadDefinitionSource>(ms_currentButtonIndex-1));
		control.ScanGamepadDefinitionSource(static_cast<CControl::GamepadDefinitionSource>(ms_currentButtonIndex-1));
		control.ClearConflictingDefinitionSource();
	}
	
	if(ms_currentButtonIndex <= 0)
	{
		control.CancelCurrentMapping();
		ms_bHasCompleted = true;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPCGamepadCalibration::RemoveCalibrationMovie
// PURPOSE:	Removes the movie
/////////////////////////////////////////////////////////////////////////////////////
void CPCGamepadCalibration::RemoveCalibrationMovie()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();
	control.CancelGampadDefinitionScan();

	if (ms_ComplexObject[CalibComplexObject::Root].IsActive())
		ms_ComplexObject[CalibComplexObject::Root].Release();

	if (ms_ComplexObject[CalibComplexObject::Content].IsActive())
		ms_ComplexObject[CalibComplexObject::Content].Release();

	if (ms_ComplexObject[CalibComplexObject::StageText].IsActive())
		ms_ComplexObject[CalibComplexObject::StageText].Release();

	if (ms_ComplexObject[CalibComplexObject::CompleteText].IsActive())
		ms_ComplexObject[CalibComplexObject::CompleteText].Release();

	if (ms_ComplexObject[CalibComplexObject::PromptText].IsActive())
		ms_ComplexObject[CalibComplexObject::PromptText].Release();

	if (ms_ComplexObject[CalibComplexObject::PromptText2].IsActive())
		ms_ComplexObject[CalibComplexObject::PromptText2].Release();

	if (ms_MovieWrapper.IsActive())
	{
		ms_MovieWrapper.RemoveMovie();
	}
	ms_bActive = false;
	ms_bLoading = false;
	ms_bHasCompleted = false;
}

#endif // RSG_PC