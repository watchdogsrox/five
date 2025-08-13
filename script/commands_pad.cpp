// Rage headers
#include "script/wrapper.h"
// Game headers
#include "frontend/PauseMenu.h"
#include "frontend/ButtonEnum.h"
#include "frontend/WarningScreen.h"
#include "network/Live/livemanager.h"
#include "script/commands_hud.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "scene/world/gameWorld.h"
#include "system/controlMgr.h"
#include "system/control.h"
#include "system/pad.h"
#include "text/textformat.h"
#include "peds/ped.h"
#include "peds/PedWeapons/PedTargetEvaluator.h"
#include "script/script_hud.h"
#include "script/thread.h"
#include "math/amath.h"
#include "system/PadGestures.h"
#if __PPU
#include "frontend/ProfileSettings.h"
#endif

#if RSG_PC
#include "frontend/MultiplayerChat.h"
#endif
#include "system/companion.h"

namespace pad_commands
{

//
// name:		GetPad
// description:	Return the pad pointer from an index
#if ENABLE_DEBUG_CONTROLS
CPad* GetPad(s32 padNumber)
#else
CPad* GetPad(s32 )
#endif
{
#if ENABLE_DEBUG_CONTROLS
	if (padNumber >= NUM_PADS)
	{
		return &CControlMgr::GetDebugPad();
	}
	else
#endif // ENABLE_DEBUG_CONTROLS
	{
		return  CControlMgr::GetPlayerPad();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetPadState
// PURPOSE :  Returns the state of one of the buttons on one of the pads.
//				The numbers which have been assigned to the buttons must match 
//				the order in the script compiler file namedefs.h
// PARAMETERS : Pad Number (0 or 1), Button Number
// OUTPUT : State of the button
/////////////////////////////////////////////////////////////////////////////////
int GetPadState(int PadNumber, int ButtonNumber)
{
	// we dont want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return 0;
	}

	CPad *pPad = GetPad(PadNumber);
	if(SCRIPT_VERIFY(pPad, "CRunningScript::GetPadState - Unknown pad"))
	{
		switch (ButtonNumber)
		{
			case PAD_LEFTSTICKX :
				return (pPad->GetLeftStickX());

			case PAD_LEFTSTICKY :
				return (pPad->GetLeftStickY());

			case PAD_RIGHTSTICKX :
				return (pPad->GetRightStickX());

			case PAD_RIGHTSTICKY :
				return (pPad->GetRightStickY());

			case PAD_LEFTSHOULDER1 :
				return (pPad->GetLeftShoulder1());

			case PAD_LEFTSHOULDER2 :
				return (pPad->GetLeftShoulder2());

			case PAD_RIGHTSHOULDER1 :
				return (pPad->GetRightShoulder1());

			case PAD_RIGHTSHOULDER2 :
				return (pPad->GetRightShoulder2());

			case PAD_DPADUP :
				return (pPad->GetDPadUp());

			case PAD_DPADDOWN :
				return (pPad->GetDPadDown());

			case PAD_DPADLEFT :
				return (pPad->GetDPadLeft());

			case PAD_DPADRIGHT :
				return (pPad->GetDPadRight());

			case PAD_START :
				return (pPad->GetStart());

			case PAD_SELECT :
				return (pPad->GetSelect());

			case PAD_SQUARE :
				return (pPad->GetButtonSquare());

			case PAD_TRIANGLE :
				return (pPad->GetButtonTriangle());

			case PAD_CROSS :
				return (pPad->GetButtonCross());

			case PAD_CIRCLE :
				return (pPad->GetButtonCircle());

			case PAD_LEFTSHOCK :
				return (pPad->GetShockButtonL());

			case PAD_RIGHTSHOCK :
				return (pPad->GetShockButtonR());


			default :
				scriptAssertf(0, "%s:CRunningScript::GetPadState - Unknown button on pad", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				break;
		}
	}
	return(0);	//	will never get here
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetPadStateJustDown
// PURPOSE :  Returns the state of one of the buttons on one of the pads.
//				The numbers which have been assigned to the buttons must match 
//				the order in the script compiler file namedefs.h
// PARAMETERS : Pad Number (0 or 1), Button Number
// OUTPUT : State of the button
/////////////////////////////////////////////////////////////////////////////////
bool GetPadStateJustDown(int PadNumber, int ButtonNumber)
{
	// we dont want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	CPad *pPad = GetPad(PadNumber);
	if(SCRIPT_VERIFY(pPad, "CRunningScript::GetPadStateJustDown - Unknown pad"))
	{
		CControl *pControl;

#if ENABLE_DEBUG_CONTROLS
		if (PadNumber >= NUM_PADS)
		{
			pControl = &CControlMgr::GetDebugControl();
		}
		else
#endif // ENABLE_DEBUG_CONTROLS
		{
			pControl = &CControlMgr::GetMainPlayerControl();
		}

		switch (ButtonNumber)
		{

			case PAD_LEFTSHOULDER1 :
					// If the code has already consumed this button we don't tell the script about it.
					// This is a complete fudge and should look nice ones the pad code for script has been re-organised
				if (pControl->GetPedCollectPickupConsumed())
				{
					return false;
				}

				return pPad->LeftShoulder1JustDown();

			case PAD_LEFTSHOULDER2 :
				return pPad->LeftShoulder2JustDown();

			case PAD_RIGHTSHOULDER1 :
				return pPad->RightShoulder1JustDown();

			case PAD_RIGHTSHOULDER2 :
				return pPad->RightShoulder2JustDown();

			case PAD_DPADUP :
				return pPad->DPadUpJustDown();

			case PAD_DPADDOWN :
				return pPad->DPadDownJustDown();

			case PAD_DPADLEFT :
				return pPad->DPadLeftJustDown();

			case PAD_DPADRIGHT :
				return pPad->DPadRightJustDown();

			case PAD_START :
				return pPad->StartJustDown();

			case PAD_SELECT :
				return pPad->SelectJustDown();

			case PAD_SQUARE :
				return pPad->ButtonSquareJustDown();

			case PAD_TRIANGLE :
				return pPad->ButtonTriangleJustDown();

			case PAD_CROSS :
				return pPad->ButtonCrossJustDown();

			case PAD_CIRCLE :
				return pPad->ButtonCircleJustDown();

			case PAD_LEFTSHOCK :
				return pPad->ShockButtonLJustDown();

			case PAD_RIGHTSHOCK :
				return pPad->ShockButtonRJustDown();

			default :
				scriptAssertf(0, "%s:CRunningScript::GetPadStateJustDown - Unknown button on pad", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				break;
		}
	}
	return(false);	//	will never get here
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetPadStateJustUp
// PURPOSE :  Returns the state of one of the buttons on one of the pads.
//				The numbers which have been assigned to the buttons must match 
//				the order in the script compiler file namedefs.h
// PARAMETERS : Pad Number (0 or 1), Button Number
// OUTPUT : State of the button
/////////////////////////////////////////////////////////////////////////////////
bool GetPadStateJustUp(int PadNumber, int ButtonNumber)
{
	// we dont want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	CPad *pPad = GetPad(PadNumber);
	if(SCRIPT_VERIFY(pPad, "CRunningScript::GetPadStateJustDown - Unknown pad"))
	{
		CControl *pControl;

#if ENABLE_DEBUG_CONTROLS
		if (PadNumber >= NUM_PADS)
		{
			pControl = &CControlMgr::GetDebugControl();
		}
		else
#endif // ENABLE_DEBUG_CONTROLS
		{
			pControl = &CControlMgr::GetMainPlayerControl();
		}

		switch (ButtonNumber)
		{

		case PAD_LEFTSHOULDER1 :
			// If the code has already consumed this button we don't tell the script about it.
			// This is a complete fudge and should look nice ones the pad code for script has been re-organised
			if (pControl->GetPedCollectPickupConsumed())
			{
				return false;
			}

			return pPad->LeftShoulder1JustUp();

		case PAD_LEFTSHOULDER2 :
			return pPad->LeftShoulder2JustUp();

		case PAD_RIGHTSHOULDER1 :
			return pPad->RightShoulder1JustUp();

		case PAD_RIGHTSHOULDER2 :
			return pPad->RightShoulder2JustUp();

		case PAD_DPADUP :
			return pPad->DPadUpJustUp();

		case PAD_DPADDOWN :
			return pPad->DPadDownJustUp();

		case PAD_DPADLEFT :
			return pPad->DPadLeftJustUp();

		case PAD_DPADRIGHT :
			return pPad->DPadRightJustUp();

		case PAD_START :
			return pPad->StartJustUp();

		case PAD_SELECT :
			return pPad->SelectJustUp();

		case PAD_SQUARE :
			return pPad->ButtonSquareJustUp();

		case PAD_TRIANGLE :
			return pPad->ButtonTriangleJustUp();

		case PAD_CROSS :
			return pPad->ButtonCrossJustUp();

		case PAD_CIRCLE :
			return pPad->ButtonCircleJustUp();

		case PAD_LEFTSHOCK :
			return pPad->ShockButtonLJustUp();

		case PAD_RIGHTSHOCK :
			return pPad->ShockButtonRJustUp();

		default :
			scriptAssertf(0, "%s:CRunningScript::GetPadStateJustUp - Unknown button on pad", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			break;
		}
	}
	return(false);	//	will never get here
}



bool CommandIsButtonPressed(int PadNumber, int ButtonNumber)
{
	if ( (hud_commands::CommandIsPauseMenuActive())
		&& (CLiveManager::GetInviteMgr().IsDisplayingConfirmation()) )
	{	//	In Multiplayer games, the "Do you want to accept invite?" message will be displayed even if the
		//	script has called DEACTIVATE_FRONTEND.
		//	The script should not try to react to any control pad buttons which are actually intended
		//	as a response to the accept invite message.
		return false;
	}

	// we don't want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	bool LatestCmpFlagResult = false;
	if (GetPadState(PadNumber, ButtonNumber) )	//	&& !CControlMgr::GetPad(0).JustOutOfFrontEnd rage conversion
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsButtonReleased(int PadNumber, int ButtonNumber)
{
	if ( (hud_commands::CommandIsPauseMenuActive())
		&& (CLiveManager::GetInviteMgr().IsDisplayingConfirmation()) )
	{	//	In Multiplayer games, the "Do you want to accept invite?" message will be displayed even if the
		//	script has called DEACTIVATE_FRONTEND.
		//	The script should not try to react to any control pad buttons which are actually intended
		//	as a response to the accept invite message.
		return false;
	}

	// we don't want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	bool LatestCmpFlagResult = false;
	if (!GetPadState(PadNumber, ButtonNumber) )	//	&& !CControlMgr::GetPad(0).JustOutOfFrontEnd rage conversion
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

u32 CommandGetButtonValue(int PadNumber, int ButtonNumber)
{
	if ( (hud_commands::CommandIsPauseMenuActive())
		&& (CLiveManager::GetInviteMgr().IsDisplayingConfirmation()) )
	{	//	In Multiplayer games, the "Do you want to accept invite?" message will be displayed even if the
		//	script has called DEACTIVATE_FRONTEND.
		//	The script should not try to react to any control pad buttons which are actually intended
		//	as a response to the accept invite message.
		return false;
	}

	// we don't want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	return GetPadState(PadNumber, ButtonNumber);
}

bool CommandIsButtonJustPressed(int PadNumber, int ButtonNumber)
{
	if ( (hud_commands::CommandIsPauseMenuActive())
		&& (CLiveManager::GetInviteMgr().IsDisplayingConfirmation()) )
	{	//	In Multiplayer games, the "Do you want to accept invite?" message will be displayed even if the
		//	script has called DEACTIVATE_FRONTEND.
		//	The script should not try to react to any control pad buttons which are actually intended
		//	as a response to the accept invite message.
		return false;
	}

	// we don't want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	bool LatestCmpFlagResult = false;
	if (GetPadStateJustDown(PadNumber, ButtonNumber) )
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsButtonJustReleased(int PadNumber, int ButtonNumber)
{
	if ( (hud_commands::CommandIsPauseMenuActive())
		&& (CLiveManager::GetInviteMgr().IsDisplayingConfirmation()) )
	{	//	In Multiplayer games, the "Do you want to accept invite?" message will be displayed even if the
		//	script has called DEACTIVATE_FRONTEND.
		//	The script should not try to react to any control pad buttons which are actually intended
		//	as a response to the accept invite message.
		return false;
	}

	// we don't want script getting buttons when frontend is active (but not paused) in MP games
	// otherwise phone will not be seen but will still be controlled as the scripts are still
	// processing but nothing is actually rendered.   fix for bug 204425
	if (CPauseMenu::IsActive() && NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	bool LatestCmpFlagResult = false;
	if (GetPadStateJustUp(PadNumber, ButtonNumber) )
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

enum {
	PLAYER_CONTROL=0,
	CAMERA_CONTROL,
	FRONTEND_CONTROL
};

static CControl& GetControl(int control)
{
	if(control == PLAYER_CONTROL)
		return CControlMgr::GetMainPlayerControl();
	else if(control == CAMERA_CONTROL)
		return CControlMgr::GetMainPlayerControl();
	else //if(control == FRONTEND_CONTROL)
		return CControlMgr::GetMainFrontendControl();
}

// PURPOSE:	Returns the controller specified. Unlike GetControl() the controller is garenteed to be
//			the requested one and not an empty controller when player control is disabled.
static CControl& GetActualControl(int control)
{
	if(control == PLAYER_CONTROL)
		return CControlMgr::GetMainPlayerControl(false);
	else if(control == CAMERA_CONTROL)
		return CControlMgr::GetMainPlayerControl(false);
	else //if(control == FRONTEND_CONTROL)
		return CControlMgr::GetMainFrontendControl(false);
}

static CControl& GetPlayerMappingControl(int control)
{
	if(control == PLAYER_CONTROL)
		return CControlMgr::GetPlayerMappingControl();
	else
		return GetActualControl(control);
}


bool CommandIsControlEnabled(int control, int action)
{
	return GetActualControl(control).GetValue(action).IsEnabled();
}

bool CommandIsControlPressed(int control, int action)
{
	return GetControl(control).GetValue(action).IsDown();
}

bool CommandIsControlReleased(int control, int action)
{
	return GetControl(control).GetValue(action).IsUp();
}

bool CommandIsControlJustPressed(int control, int action)
{
	return GetControl(control).GetValue(action).IsPressed();
}

bool CommandIsControlJustReleased(int control, int action)
{
	return GetControl(control).GetValue(action).IsReleased();
}

u32 CommandGetControlValue(int control, int action)
{
	return Clamp((s32)(GetControl(control).GetValue(action).GetNorm() * 127.5f) + 127, 0, 255);
}

float CommandAdjustMouseValue(float fValue)
{
	bool bMultimonitor = false;
#if SUPPORT_MULTI_MONITOR
	bMultimonitor = GRCDEVICE.GetMonitorConfig().isMultihead();
#endif // SUPPORT_MULTI_MONITOR
	/*
	if(CHudTools::IsSuperWideScreen())
	{
		float fAspect = CHudTools::GetAspectRatio();
		const float SIXTEEN_BY_NINE = 16.0f/9.0f;
		float fInverse = (fAspect / SIXTEEN_BY_NINE);
		fValue =  0.5f - ((0.5f - fValue) * fInverse);
	}
	*/

	if(bMultimonitor)
	{
		float fAspect = CHudTools::GetAspectRatio(true);
		float fBaseAspect = CHudTools::GetAspectRatio(false);
		float fInverse = (fAspect / fBaseAspect);
		fValue =  0.5f - ((0.5f - fValue) * fInverse);
	}
	return fValue;
}

float CommandGetControlNormal(int control, int action)
{
	float fValue = GetControl(control).GetValue(action).GetNorm();
#if RSG_PC
	if(action == INPUT_CURSOR_X && CScriptHud::GetUseAdjustedMouseCoords())
	{
		fValue = CommandAdjustMouseValue(fValue);
	}
#endif // RSG_PC
	return fValue;
}

void CommandSetUseAdjustedMouseCoords(bool bUseAdjustedMouseCoords)
{
	CScriptHud::SetUseAdjustedMouseCoords(bUseAdjustedMouseCoords);
}

bool CommandGetUseAdjustedMouseCoords()
{
	return CScriptHud::GetUseAdjustedMouseCoords();
}

float CommandGetControlUnboundNormal(int control, int action)
{
	return GetControl(control).GetValue(action).GetUnboundNorm();
}

bool CommandSetControlValueNextFrame(int control, int action, float value)
{
	CControl& controlObj = GetActualControl(control);

	// Never update the disabled control!
	if(CControlMgr::IsDisabledControl(&controlObj) == false)
	{
		controlObj.SetInputValueNextFrame(static_cast<InputType>(action), value);
		return true;
	}
	else
	{
		return false;
	}
}

bool CommandIsDisabledControlPressed(int control, int action)
{
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return false;
	}
	#endif // RSG_PC

	ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	return GetActualControl(control).GetValue(action).IsDown(ioValue::BUTTON_DOWN_THRESHOLD, options);
}

bool CommandIsDisabledControlReleased(int control, int action)
{
	
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return false;
	}
	#endif // RSG_PC

	ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	return GetActualControl(control).GetValue(action).IsUp(ioValue::BUTTON_DOWN_THRESHOLD, options);
}

bool CommandIsDisabledControlJustPressed(int control, int action)
{
	
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return false;
	}
	#endif // RSG_PC

	ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	return GetActualControl(control).GetValue(action).IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options);
}

bool CommandIsDisabledControlJustReleased(int control, int action)
{
	
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return false;
	}
	#endif // RSG_PC

	ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	return GetActualControl(control).GetValue(action).IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options);
}

float CommandGetDisabledControlNormal(int control, int action)
{
	


	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	float fValue = GetActualControl(control).GetValue(action).GetNorm(options);
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return fValue;
	}

	if(action == INPUT_CURSOR_X && CScriptHud::GetUseAdjustedMouseCoords())
	{
		fValue = CommandAdjustMouseValue(fValue);
	}
#endif // RSG_PC

	return fValue;
}

float CommandGetDisabledControlUnboundNormal(int control, int action)
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	return GetActualControl(control).GetValue(action).GetUnboundNorm(options);
}

#if __PPU
inline int ScalePS3Frequency(int frequency)
{
	if(frequency > 0)
	{
		// The ps3 first motor does not vibrate until a frequency of 65 is passed in. This means its scale is effectively 191 (65 - 256).
		// Multiplying by 0.74609375f put the value in this scale.
		frequency = static_cast<int>(65.0f + (frequency * 0.74609375f));
		if(frequency > MAX_VIBRATION_FREQUENCY)
		{
			frequency = static_cast<int>(MAX_VIBRATION_FREQUENCY);
		}
	}
	return frequency;
}
#endif // __PPU

void CommandShakeControl(int control, int duration, int frequency)
{
	if(SCRIPT_VERIFY(frequency <= MAX_VIBRATION_FREQUENCY, "SET_CONTROL_SHAKE - Frequency for SHAKE_PAD must be between 0 and 256"))
	{
#if __PPU
		frequency = ScalePS3Frequency(frequency);		
#endif // __PPU

		GetPlayerMappingControl(control).ShakeController(duration, frequency, true);
	}
}

void CommandShakeControlTrigger(int TRIGGER_RUMBLE_ONLY(control),
								int TRIGGER_RUMBLE_ONLY(leftDuration),
								int TRIGGER_RUMBLE_ONLY(leftFrequency),
								int TRIGGER_RUMBLE_ONLY(rightDuration),
								int TRIGGER_RUMBLE_ONLY(rightFrequency))
{
#if HAS_TRIGGER_RUMBLE && !RSG_PROSPERO
	if( SCRIPT_VERIFY(leftFrequency <= MAX_VIBRATION_FREQUENCY, "SET_CONTROL_TRIGGER_SHAKE - leftFrequency for SHAKE_PAD must be between 0 and 256") &&
		SCRIPT_VERIFY(rightFrequency <= MAX_VIBRATION_FREQUENCY, "SET_CONTROL_TRIGGER_SHAKE - rightFrequency for SHAKE_PAD must be between 0 and 256") )
	{
		GetPlayerMappingControl(control).StartTriggerShake(leftDuration, leftFrequency, rightDuration, rightFrequency, 0, true);
	}
#endif // HAS_TRIGGER_RUMBLE
}

void CommandStopControlShake(int control)
{
	GetPlayerMappingControl(control).StopPlayerPadShaking(true);
}

void CommandSetControlShakeSuppressedId(int control, int uniqueId)
{
	GetPlayerMappingControl(control).SetShakeSuppressId(uniqueId);
}

void CommandClearControlShakeSuppressedId(int control)
{
	GetPlayerMappingControl(control).SetShakeSuppressId(CControl::NO_SUPPRESS);
}


void CommandGetPositionOfAnalogueSticks(int PadNumber, int &ReturnLeftX, int &ReturnLeftY, int &ReturnRightX, int &ReturnRightY)
{
	CPad *pPad = GetPad(PadNumber);
	if(SCRIPT_VERIFY(pPad, "GET_POSITION_OF_ANALOGUE_STICKS - Pad does not exist."))
	{
		ReturnLeftX = pPad->GetLeftStickX();
		ReturnLeftY = pPad->GetLeftStickY();
		ReturnRightX = pPad->GetRightStickX();
		ReturnRightY = pPad->GetRightStickY();
	}
	else
	{
		ReturnLeftX = 0;
		ReturnLeftY = 0;
		ReturnRightX = 0;
		ReturnRightY = 0;
	}
}

#if LIGHT_EFFECTS_SUPPORT
ioConstantLightEffect s_ScriptConstColorEffect;
#endif // LIGHT_EFFECTS_SUPPORT

void CommandSetControlLightEffectColor(int LIGHT_EFFECTS_ONLY(control), int LIGHT_EFFECTS_ONLY(red), int LIGHT_EFFECTS_ONLY(green), int LIGHT_EFFECTS_ONLY(blue))
{
#if LIGHT_EFFECTS_SUPPORT
	SCRIPT_ASSERTF(red >= 0 && red <= 255,     "SET_CONTROL_LIGHT_EFFECT_COLOR - invalid red value %d",   red);
	SCRIPT_ASSERTF(green >= 0 && green <= 255, "SET_CONTROL_LIGHT_EFFECT_COLOR - invalid green value %d", green);
	SCRIPT_ASSERTF(blue >= 0 && blue <= 255,   "SET_CONTROL_LIGHT_EFFECT_COLOR - invalid blue value %d",  blue);

	red   = Clamp(red,   0, 255);
	green = Clamp(green, 0, 255);
	blue  = Clamp(blue,  0, 255);
	s_ScriptConstColorEffect = ioConstantLightEffect(static_cast<u8>(red), static_cast<u8>(green), static_cast<u8>(blue));
	GetPlayerMappingControl(control).SetLightEffect(&s_ScriptConstColorEffect, CControl::SCRIPT_LIGHT_EFFECT);
#endif // LIGHT_EFFECTS_SUPPORT
}

void CComandClearControlEffect(int LIGHT_EFFECTS_ONLY(control))
{
#if LIGHT_EFFECTS_SUPPORT
	GetPlayerMappingControl(control).ClearLightEffect(&s_ScriptConstColorEffect, CControl::SCRIPT_LIGHT_EFFECT);
#endif // LIGHT_EFFECTS_SUPPORT
}


bool IsDebugKeyPressed(bool bJustPressed, int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
#if __ASSERT
	switch (KeyboardModifier)
	{
		case KEYBOARD_MODE_DEBUG :
		case KEYBOARD_MODE_DEBUG_SHIFT :
		case KEYBOARD_MODE_DEBUG_CTRL :
		case KEYBOARD_MODE_DEBUG_ALT :
		case KEYBOARD_MODE_DEBUG_CNTRL_SHIFT :
			break;
		default:
			if (bJustPressed)
			{
				scriptAssertf(0, "%s:IS_DEBUG_KEY_JUST_PRESSED - Unrecognised keyboard modifier %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), KeyboardModifier);
			}
			else
			{
				scriptAssertf(0, "%s:IS_DEBUG_KEY_PRESSED - Unrecognised keyboard modifier %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), KeyboardModifier);
			}
			break;
	}
#endif	//	__ASSERT

	if (bJustPressed)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KeyNumber, KeyboardModifier, (char *) pDescriptionOfUse))
		{
			return true;
		}
	}
	else
	{
		if (CControlMgr::GetKeyboard().GetKeyDown(KeyNumber, KeyboardModifier, (char *) pDescriptionOfUse))
		{
			return true;
		}
	}

	return false;
}

bool IsDebugKeyReleased(bool bJustReleased, int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
#if __ASSERT
	switch (KeyboardModifier)
	{
	case KEYBOARD_MODE_DEBUG :
	case KEYBOARD_MODE_DEBUG_SHIFT :
	case KEYBOARD_MODE_DEBUG_CTRL :
	case KEYBOARD_MODE_DEBUG_ALT :
	case KEYBOARD_MODE_DEBUG_CNTRL_SHIFT :
		break;
	default:
		if (bJustReleased)
		{
			scriptAssertf(0, "%s:IS_DEBUG_KEY_JUST_RELEASED - Unrecognised keyboard modifier %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), KeyboardModifier);
		}
		else
		{
			scriptAssertf(0, "%s:IS_DEBUG_KEY_RELEASED - Unrecognised keyboard modifier %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), KeyboardModifier);
		}
		break;
	}
#endif	//	__ASSERT

	if (bJustReleased)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustUp(KeyNumber, KeyboardModifier, (char *) pDescriptionOfUse))
		{
			return true;
		}
	}
	else
	{
		if (CControlMgr::GetKeyboard().GetKeyUp(KeyNumber, KeyboardModifier, (char *) pDescriptionOfUse))
		{
			return true;
		}
	}

	return false;
}

bool CommandIsDebugKeyPressed(int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
	return IsDebugKeyPressed(false, KeyNumber, KeyboardModifier, pDescriptionOfUse);
}

bool CommandIsDebugKeyJustPressed(int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
	return IsDebugKeyPressed(true, KeyNumber, KeyboardModifier, pDescriptionOfUse);
}

bool CommandIsDebugKeyReleased(int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
	return IsDebugKeyReleased(false, KeyNumber, KeyboardModifier, pDescriptionOfUse);
}

bool CommandIsDebugKeyJustReleased(int KeyNumber, int KeyboardModifier, const char *pDescriptionOfUse)
{
	return IsDebugKeyReleased(true, KeyNumber, KeyboardModifier, pDescriptionOfUse);
}

bool CommandIsKeyboardKeyPressed(int KeyNumber)
{
	bool LatestCmpFlagResult;

	LatestCmpFlagResult = false;

	if (CControlMgr::GetKeyboard().GetKeyDown(KeyNumber, KEYBOARD_MODE_DEBUG, "Used in script"))
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsKeyboardKeyJustPressed(int KeyNumber)
{
	bool LatestCmpFlagResult;

	LatestCmpFlagResult = false;

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KeyNumber, KEYBOARD_MODE_DEBUG, "Used in script"))
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}

bool CommandIsKeyboardKeyReleased(int KeyNumber)
{
	bool LatestCmpFlagResult;

	LatestCmpFlagResult = false;

	if (CControlMgr::GetKeyboard().GetKeyUp(KeyNumber, KEYBOARD_MODE_DEBUG, "Used in script"))
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsKeyboardKeyJustReleased(int KeyNumber)
{
	bool LatestCmpFlagResult;

	LatestCmpFlagResult = false;

	if (CControlMgr::GetKeyboard().GetKeyJustUp(KeyNumber, KEYBOARD_MODE_DEBUG, "Used in script"))
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}

bool CommandIsGameKeyboardKeyPressed(int KeyNumber)
{
	bool LatestCmpFlagResult = false;
	if (CControlMgr::GetKeyboard().GetKeyDown(KeyNumber, KEYBOARD_MODE_GAME, "Used in script - game (not levels) mode"))
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsGameKeyboardKeyJustPressed(int KeyNumber)
{
	bool LatestCmpFlagResult = false;
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KeyNumber, KEYBOARD_MODE_GAME, "Used in script - game (not levels) mode"))
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}

bool CommandIsGameKeyboardKeyReleased(int KeyNumber)
{
	bool LatestCmpFlagResult = false;
	if (CControlMgr::GetKeyboard().GetKeyUp(KeyNumber, KEYBOARD_MODE_GAME, "Used in script - game (not levels) mode"))
	{
		LatestCmpFlagResult = true;
	}

	return LatestCmpFlagResult;
}

bool CommandIsGameKeyboardKeyJustReleased(int KeyNumber)
{
	bool LatestCmpFlagResult = false;
	if (CControlMgr::GetKeyboard().GetKeyJustUp(KeyNumber, KEYBOARD_MODE_GAME, "Used in script - game (not levels) mode"))
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}

#if !__FINAL
void CommandFakeKeyPress(int KeyNumber)
{
	CControlMgr::GetKeyboard().FakeKeyPress(KeyNumber);
}
#endif

int CommandGetKeyboardMode()
{
	return CControlMgr::GetKeyboard().GetKeyboardMode();
}

void CommandSetKeyboardMode(s32 NOTFINAL_ONLY(KeyboardMode))
{
#if !__FINAL
	CControlMgr::GetKeyboard().SetCurrentMode(KeyboardMode);
#endif
}

bool CommandIsLookInverted()
{
	// For legacy reasons, this function only works with the y axis.
	// We could do with replacing this with one that supports both axis.
	if (CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK))
		return true;
	else
		return false;
}

bool CommandIsMouseLookInverted()
{
#if KEYBOARD_MOUSE_SUPPORT
	if (CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE))
	{
		return true;
	}
	else
#endif // KEYBOARD_MOUSE_SUPPORT
	{
		return false;
	}
}

int CommandGetLocalPlayerAimState()
{
#if KEYBOARD_MOUSE_SUPPORT
	if(GetPlayerMappingControl(PLAYER_CONTROL).WasKeyboardMouseLastKnownSource())
	{
		return CPedTargetEvaluator::TARGETING_OPTION_FREEAIM;
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG);
}

int CommandGetLocalPlayerGamepadAimState()
{
	return CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG);
}

bool CommandIsUsingAlternateHandbrake()
{
	if (CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE))
		return true;
	else
		return false;
}

bool CommandIsUsingAlternateDriveby()
{
	if (CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY))
		return true;
	else
		return false;
}

bool CommandIsMovementWhileZoomedEnabled()
{
	if (CPauseMenu::GetMenuPreference(PREF_SNIPER_ZOOM))
		return true;
	else
		return false;
}

void CommandSetPlayerpadShakesWhenControllerDisabled(bool bShake)
{
	CControlMgr::SetShakeWhenControllerDisabled(bShake);
}

bool CommandUsingStandardControls()
{
	return (GetPlayerMappingControl(PLAYER_CONTROL).GetActiveLayout() == STANDARD_TPS_LAYOUT);
}


// Motion control commands, implemented in PS3 branch

bool CommandGetPadPitchRoll(int PS3_ONLY(PadNumber), float& PS3_ONLY(fPitch), float& PS3_ONLY(fRoll))
{
#if __PPU

	if(!CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_SCRIPT))
	{
		return false;
	}

	CPad* pPad = GetPad(PadNumber);
	if(pPad == NULL || !pPad->HasSensors() || pPad->GetPadGesture() == NULL )
	{
		return false;
	}

	fPitch = pPad->GetPadGesture()->GetPadPitch();
	fRoll = pPad->GetPadGesture()->GetPadRoll();
	return true;

#else
	return false;
#endif
}

bool CommandHasReloadedWithMotionControl(int PS3_ONLY(PadNumber), int & PS3_ONLY(bHasReloaded))
{	
#if __PPU
	bHasReloaded = false;
	if(CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_RELOAD))
	{
		CPad* pPad = GetPad(PadNumber);
		if(pPad && pPad->GetPadGesture())
		{
			bHasReloaded = pPad->GetPadGesture()->GetHasReloaded();
			return true;
		}
	}
#endif
	return false;
}

// TODO: We should rename this function after GTA IV to "SetForceAllMotionControlOn"
#if USE_SIXAXIS_GESTURES
void CommandSetAllMotionControlPreferencesOnOff(bool bPreferences)
#else
void CommandSetAllMotionControlPreferencesOnOff(bool UNUSED_PARAM(bPreferences))
#endif
{
#if USE_SIXAXIS_GESTURES
	/*s32 value = (s32)bPreferences;
	Assert(value == 1 || value == 0);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_HELI, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_BIKE, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_BOAT, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_RELOAD, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_CALIBRATION, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_ACTIVITY, value);
	CFrontEnd::SetMenuOptionValue(PREF_SIXAXIS_AFTERTOUCH, value);
	// Save these to the profile settings
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_HELI, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_BIKE, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_BOAT, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_MISSION, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_RELOAD, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_ACTIVITY, value);
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_AFTERTOUCH, value);
	}*/

	if(bPreferences)
		CPadGestureMgr::SetMotionControlStatus(CPadGestureMgr::MC_STATUS_FORCE_ON);
	else
		CPadGestureMgr::SetMotionControlStatus(CPadGestureMgr::MC_STATUS_USE_PREFERENCE);
#endif
}

bool CommandGetMotionControlPreference(int PS3_ONLY(nPreferenceType))
{
#if __PPU
	CPadGestureMgr::eMotionControlTypes eMCType = (CPadGestureMgr::eMotionControlTypes)nPreferenceType;
	return CPadGestureMgr::GetMotionControlEnabled(eMCType);
#else
	return false;
#endif
}

void CommandGetMousePosition(float &x, float &y)
{
	x = ioMouse::GetNormX();
	y = ioMouse::GetNormY();
}

bool CommandIsMouseButtonJustPressed (int MouseButton)
{
	bool bCheckLeftMouse = (MouseButton & ioMouse::MOUSE_LEFT )? true : false;
	bool bCheckRightMouse = (MouseButton & ioMouse::MOUSE_RIGHT )? true : false;

	if (bCheckLeftMouse &&  bCheckRightMouse) 
	{
		return (((ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)) && (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT));
	}
	else if (bCheckLeftMouse)
	{
		return ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)) != 0;
	}
	else if (bCheckRightMouse)
	{
		return ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)) != 0;
	}
	else
	{
		return false;
	}
}

bool CommandIsMouseButtonPressed (int MouseButton)
{
	bool bCheckLeftMouse = (MouseButton & ioMouse::MOUSE_LEFT )? true : false;
	bool bCheckRightMouse = (MouseButton & ioMouse::MOUSE_RIGHT )? true : false;
	
	if (bCheckLeftMouse &&  bCheckRightMouse) 
	{
		return ((!(ioMouse::GetPressedButtons()& ioMouse::MOUSE_LEFT) && !(ioMouse::GetPressedButtons()& ioMouse::MOUSE_RIGHT)) && ((ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) && (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)));
	}
	else if (bCheckLeftMouse)
	{
		return (!(ioMouse::GetPressedButtons()& ioMouse::MOUSE_LEFT) && (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT));
	}
	else if (bCheckRightMouse)
	{
		return(!(ioMouse::GetPressedButtons()& ioMouse::MOUSE_RIGHT) &&  (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT));
	}
	else
	{
		return false;
	}
}

bool CommandIsMouseButtonJustReleased (int MouseButton)
{
	bool bCheckLeftMouse = (MouseButton & ioMouse::MOUSE_LEFT )? true : false;
	bool bCheckRightMouse = (MouseButton & ioMouse::MOUSE_RIGHT )? true : false;

	if (bCheckLeftMouse &&  bCheckRightMouse) 
	{
		return (((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)) && (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT));
	}
	else if (bCheckLeftMouse)
	{
		return ((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)) != 0;
	}
	else if (bCheckRightMouse)
	{
		return ((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT)) != 0;
	}
	else
	{
		return false;
	}
}

bool CommandIsMouseButtonReleased (int MouseButton)
{
	bool bCheckLeftMouse = (MouseButton & ioMouse::MOUSE_LEFT )? true : false;
	bool bCheckRightMouse = (MouseButton & ioMouse::MOUSE_RIGHT )? true : false;

	if (bCheckLeftMouse &&  bCheckRightMouse) 
	{
		return ((!(ioMouse::GetReleasedButtons()& ioMouse::MOUSE_LEFT) && !(ioMouse::GetReleasedButtons()& ioMouse::MOUSE_RIGHT)) && (!(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) && !(ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)));
	}
	else if (bCheckLeftMouse)
	{
		return (!(ioMouse::GetReleasedButtons()& ioMouse::MOUSE_LEFT) && !(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT));
	}
	else if (bCheckRightMouse)
	{
		return(!(ioMouse::GetReleasedButtons()& ioMouse::MOUSE_RIGHT) &&  !(ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT));
	}
	else
	{
		return false;
	}
}

void CommandSetInputExclusive(int control, int action)
{
	if (scriptVerifyf(action>=0 && action<MAX_INPUTS, "%s : SET_INPUT_EXCLUSIVE - action is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		GetActualControl(control).SetInputExclusive(action);
	}
}


void CommandDisableControlAction(int control, int action, bool disableRelatedActions)
{
	if (scriptVerifyf(action>=0 && action<MAX_INPUTS, "%s : DISABLE_CONTROL_ACTION - action is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CTheScripts::Frack();
		GetActualControl(control).DisableInput(action, ioValue::DEFAULT_DISABLE_OPTIONS, disableRelatedActions);
	}

	// if they disable the frontend control start menu then set the global flag to disable pause menu as this is what the code checks in various places
	if (control == FRONTEND_CONTROL && action == INPUT_FRONTEND_PAUSE)
	{
		CScriptHud::bDisablePauseMenuThisFrame = true;
	}
}

void CommandEnableControlAction(int control, int action, bool enableRelatedActions)
{
	if (scriptVerifyf(action>=0 && action<MAX_INPUTS, "%s : ENABLE_CONTROL_ACTION - action is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{		
		GetActualControl(control).EnableInput(action, enableRelatedActions);
	}

	// if they disable the frontend control start menu then set the global flag to disable pause menu as this is what the code checks in various places
	if (control == FRONTEND_CONTROL && action == INPUT_FRONTEND_PAUSE)
	{
		CScriptHud::bDisablePauseMenuThisFrame = !CTheScripts::Frack();
	}
}

void CommandDisableAllControlActions(int control)
{
	GetActualControl(control).DisableAllInputs();
	CTheScripts::Frack();
#if GTA_REPLAY
	CReplayControl::SetControlsDisabled();
#endif // GTA_REPLAY
}


void CommandEnableAllControlActions(int control)
{
	GetActualControl(control).EnableAllInputs();
}

void CommandDisableControlButton(int control, int ButtonNumber)
{
	switch (ButtonNumber)
	{
	case PAD_LEFTSTICKX :
		GetControl(control).DisableInput(IOMS_PAD_AXIS, IOM_AXIS_LX);
		break;

	case PAD_LEFTSTICKY :
		GetControl(control).DisableInput(IOMS_PAD_AXIS, IOM_AXIS_LY);
		break;

	case PAD_RIGHTSTICKX :
		GetControl(control).DisableInput(IOMS_PAD_AXIS, IOM_AXIS_RX);
		break;

	case PAD_RIGHTSTICKY :
		GetControl(control).DisableInput(IOMS_PAD_AXIS, IOM_AXIS_RY);
		break;

	case PAD_LEFTSHOULDER1 :
		GetControl(control).DisableInput(IOMS_PAD_ANALOGBUTTON, ioPad::L1_INDEX);
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::L1);
		break;

	case PAD_LEFTSHOULDER2 :
		GetControl(control).DisableInput(IOMS_PAD_ANALOGBUTTON, ioPad::L2_INDEX);
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::L2);
		break;

	case PAD_RIGHTSHOULDER1 :
		GetControl(control).DisableInput(IOMS_PAD_ANALOGBUTTON, ioPad::R1_INDEX);
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::R1);
		break;

	case PAD_RIGHTSHOULDER2 :
		GetControl(control).DisableInput(IOMS_PAD_ANALOGBUTTON, ioPad::R2_INDEX);
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::R2);
		break;

	case PAD_DPADUP :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::LUP);
		break;

	case PAD_DPADDOWN :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::LDOWN);
		break;

	case PAD_DPADLEFT :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::LLEFT);
		break;

	case PAD_DPADRIGHT :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::LRIGHT);
		break;

	case PAD_START :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::START);

		// if they disable the frontend control start menu then set the global flag to disable pause menu as this is what the code checks in various places
		if (control == FRONTEND_CONTROL)
		{
			CScriptHud::bDisablePauseMenuThisFrame = true;
		}

		break;

	case PAD_SELECT :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::SELECT);
		break;

	case PAD_SQUARE :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::RLEFT);
		break;

	case PAD_TRIANGLE :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::RUP);
		break;

	case PAD_CROSS :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::RDOWN);
		break;

	case PAD_CIRCLE :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::RRIGHT);
		break;

	case PAD_LEFTSHOCK :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::L3);
		break;

	case PAD_RIGHTSHOCK :
		GetControl(control).DisableInput(IOMS_PAD_DIGITALBUTTON, ioPad::R3);
		break;

	default :
		scriptAssertf(0, "%s:DISABLE_PAD_BUTTON - Unknown button", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		break;
	}
}

bool CommandInitPCScriptedControls(const char * WIN32PC_ONLY(schemeName))
{
#if __WIN32PC
	return CControlMgr::GetPlayerMappingControl().LoadPCScriptControlMappings(atHashString(schemeName));
#else
	return true;
#endif // __WIN32PC
}

bool CommandSwitchPCScriptedControls(const char * WIN32PC_ONLY(schemeName))
{
#if __WIN32PC
	return CControlMgr::GetPlayerMappingControl().SwitchPCScriptControlMappings(atHashString(schemeName));
#else
	return true;
#endif // __WIN32PC
}

void CommandShutdownPCScriptedControls()
{
	WIN32PC_ONLY(CControlMgr::GetPlayerMappingControl().ShutdownPCScriptControlMappings());
}

void CommandAllowAlternativeScriptControlsLayout(int control)
{
	GetActualControl(control).EnableAlternateScriptedControlsLayout();
}

int CommandControlHowLongAgo(int control)
{
	return GetControl(control).InputHowLongAgo();
}

int CommandDisabledControlHowLongAgo(int control)
{
	return GetPlayerMappingControl(control).InputHowLongAgo();
}

bool CommandIsUsingKeyboardAndMouse(int UNUSED_PARAM(control))
{
#if KEYBOARD_MOUSE_SUPPORT
	return CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
#else
	return false;
#endif // KEYBOARD_MOUSE_SUPPORT
}

bool CommandIsUsingCursor(int control)
{
	return GetControl(control).IsUsingCursor();
}

bool CommandSetCursorPosition(float WIN32PC_ONLY(x), float WIN32PC_ONLY(y))
{
#if RSG_PC
	scriptAssertf(x >= 0.0f && x <= 1.0f, "Invalid x value to SET_CURSOR_POSITION (%f)!", x);
	scriptAssertf(y >= 0.0f && y <= 1.0f, "Invalid y value to SET_CURSOR_POSITION (%f)!", y);
	Clamp(x, 0.0f, 1.0f);
	Clamp(y, 0.0f, 1.0f);

	return ioMouse::SetCursorPosition(x, y);
#else
	return false;
#endif // RSG_PC
}

bool CommandIsUsingRemotePlay(int control)
{
	return GetPlayerMappingControl(control).IsUsingRemotePlay();
}

bool CommandHaveControlsChanged(int UNUSED_PARAM(control))
{
	return CControlMgr::GetPlayerMappingControl().ScriptCheckForControlsChange();
}

int ConvertIconsToInstructionalButtons(const CTextFormat::Icon& icon, bool PS3_ONLY(allowXOSwap))
{
	s32 buttonId = icon.m_IconId;
	
#if __PS3
	if(allowXOSwap && !CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS))
	{
		if(buttonId == FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A)
		{
			buttonId = FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B;
		}	
		else if(buttonId == FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B)
		{
			buttonId = FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A;
		}
	}
#endif // __PS3

	buttonId -= FONT_CHAR_TOKEN_ID_CONTROLLER_UP;

	if(scriptVerifyf((buttonId >= 0 && buttonId < MAX_INSTRUCTIONAL_BUTTONS), "Invalid instructional button ID (%d) with Icon ID (%u)!", buttonId, icon.m_IconId))
	{
		return buttonId;
	}

	return ICON_INVALID;
}

int CommandGetInputInstructionalButton(int UNUSED_PARAM(control), int input, bool allowXOSwap)
{
	ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(static_cast<InputType>(input));
	if(scriptVerifyf( source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER, "No devices are mapped to specified input!"))
	{
		if(source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER)
		{
			CTextFormat::IconList icons;
			CTextFormat::GetTokenIDFromPadButton(source.m_Device, source.m_Parameter, icons);

#if !RSG_PC
			// This will go off all the time when keyboard and mouse are used!
			scriptAssertf(icons.size() == 1, "Expected exactly source for instructional button!");
#endif // !RSG_PC

			if(icons.size() > 0)
			{
#if RSG_PC
				if(icons[0].m_Type == CTextFormat::Icon::ICON_ID)
#endif // RSG_PC
				{
					return ConvertIconsToInstructionalButtons(icons[0], allowXOSwap);
				}
			}
		}
	}

	scriptAssertf(false, "No instructional button can be mapped to input source!");
	return ICON_INVALID;
}

const char* CommandGetInputInstructionalButtonsString(int UNUSED_PARAM(control), int input, bool allowXOSwap)
{
	if(scriptVerifyf(input >= 0 && input < MAX_INPUTS, "Invalid input %d!", input))
	{
		// 64 corresponds to TEXT_LABLE_63.
		const u32 BUFFER_SIZE = 64;
		static char buffer[MAX_INPUTS][BUFFER_SIZE];

		IconParams params = CTextFormat::DEFAULT_ICON_PARAMS;
		params.m_AllowXOSwap = allowXOSwap;
	
		CTextFormat::GetInputButtons( CControlMgr::GetPlayerMappingControl().GetInputName(static_cast<InputType>(input)),
			buffer[input],
			BUFFER_SIZE,
			NULL,
			0,
			params );

		return buffer[input];
	}

	return "";
}

int CommandGetInputGroupInstructionalButton(int UNUSED_PARAM(control), int inputGroup, bool allowXOSwap)
{
	CControl::InputGroupList inputs;
	CControl::GetInputsInGroup(static_cast<InputGroup>(inputGroup), inputs);

	CTextFormat::GroupSourceList sources;

	const CControl& currentControl = CControlMgr::GetPlayerMappingControl();
	for(u32 i = 0; i < inputs.size(); ++i)
	{
		ioSource source = currentControl.GetInputSource(inputs[i]);

		if(scriptVerifyf( source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER, "No devices are mapped to specified input!"))
		{
			sources.Push(source);
		}
	}

	if(sources.size() > 0)
	{
		CTextFormat::IconList icons;
		CTextFormat::GetDeviceIconsFromInputGroup(sources, icons);

#if !RSG_PC
		// This will go off all the time when keyboard and mouse are used!
		scriptAssertf(icons.size() == 1, "Expected exactly source for instructional button!");
#endif // !RSG_PC

		if(icons.size() > 0)
		{
#if RSG_PC
			if(icons[0].m_Type == CTextFormat::Icon::ICON_ID)
#endif // RSG_PC
			{
				return ConvertIconsToInstructionalButtons(icons[0], allowXOSwap);
			}
		}
	}

	scriptAssertf(false, "No instructional button can be mapped to input source!");
	return ICON_INVALID;
}

const char* CommandGetInputGroupInstructionalButtonsString(int UNUSED_PARAM(control), int inputGroup, bool allowXOSwap)
{
	if(scriptVerifyf(inputGroup >= 0 && inputGroup < MAX_INPUTGROUPS, "Invalid input group %d!", inputGroup))
	{
		// 64 corresponds to TEXT_LABLE_63.
		const u32 BUFFER_SIZE = 64;
		static char buffer[MAX_INPUTGROUPS][BUFFER_SIZE];

		IconParams params = CTextFormat::DEFAULT_ICON_PARAMS;
		params.m_AllowXOSwap = allowXOSwap;

		CTextFormat::GetInputGroupButtons( CControlMgr::GetPlayerMappingControl().GetInputGroupName(static_cast<InputGroup>(inputGroup)),
			buffer[inputGroup],
			BUFFER_SIZE,
			NULL,
			0,
			params );
	
		return buffer[inputGroup];
	}

	return "";
}

bool CommandGetIfCompanionDeviceIsConnected()
{
#if COMPANION_APP
	return CCompanionData::GetInstance()->IsConnected();
#else
	return false;
#endif
}

void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(IS_BUTTON_PRESSED,0x7bdf096145218859, CommandIsButtonPressed);
	SCR_REGISTER_UNUSED(IS_BUTTON_JUST_PRESSED,0x4f1e7dda6834e8d4, CommandIsButtonJustPressed);
	SCR_REGISTER_UNUSED(IS_BUTTON_RELEASED,0xb7d69f6a253b695f, CommandIsButtonReleased);
	SCR_REGISTER_UNUSED(IS_BUTTON_JUST_RELEASED,0xbd9daa2a34bb6051, CommandIsButtonJustReleased);
	SCR_REGISTER_UNUSED(GET_BUTTON_VALUE,0x75a69523a8ec1a39, CommandGetButtonValue);
	
	// Input Independent Commands.
	SCR_REGISTER_SECURE(IS_CONTROL_ENABLED,0xf98ff61cd2d3500d, CommandIsControlEnabled);
	SCR_REGISTER_SECURE(IS_CONTROL_PRESSED,0x4c1b8c5717647539, CommandIsControlPressed);
	SCR_REGISTER_SECURE(IS_CONTROL_RELEASED,0x98e8b3ffceda25a5, CommandIsControlReleased);
	SCR_REGISTER_SECURE(IS_CONTROL_JUST_PRESSED,0xf09a4f413b0d30eb, CommandIsControlJustPressed);
	SCR_REGISTER_SECURE(IS_CONTROL_JUST_RELEASED,0x26009f50a14ad073, CommandIsControlJustReleased);
	SCR_REGISTER_SECURE(GET_CONTROL_VALUE,0xf439ff1991626cb9, CommandGetControlValue);
	SCR_REGISTER_UNUSED(GET_ADJUSTED_MOUSE_VALUE,0xa3f7d2ce2a1a994a, CommandAdjustMouseValue);
	SCR_REGISTER_SECURE(GET_CONTROL_NORMAL,0x664c0a1bf5e133fa, CommandGetControlNormal);
	SCR_REGISTER_SECURE(SET_USE_ADJUSTED_MOUSE_COORDS,0x4522e5855673d159, CommandSetUseAdjustedMouseCoords);
	SCR_REGISTER_UNUSED(GET_USE_ADJUSTED_MOUSE_COORDS,0xa99cfcb5427dde55, CommandGetUseAdjustedMouseCoords);
	SCR_REGISTER_SECURE(GET_CONTROL_UNBOUND_NORMAL,0x3bf65cf27f6cfbbe, CommandGetControlUnboundNormal);
	SCR_REGISTER_SECURE(SET_CONTROL_VALUE_NEXT_FRAME,0x9b3ae3991f75ef59, CommandSetControlValueNextFrame);
	SCR_REGISTER_SECURE(IS_DISABLED_CONTROL_PRESSED,0x0dba73788f6e3c5f, CommandIsDisabledControlPressed);
	SCR_REGISTER_SECURE(IS_DISABLED_CONTROL_RELEASED,0x154529612c25bc17, CommandIsDisabledControlReleased);
	SCR_REGISTER_SECURE(IS_DISABLED_CONTROL_JUST_PRESSED,0xf01464d7af0b7347, CommandIsDisabledControlJustPressed);
	SCR_REGISTER_SECURE(IS_DISABLED_CONTROL_JUST_RELEASED,0x51844f589d928a86, CommandIsDisabledControlJustReleased);
	SCR_REGISTER_SECURE(GET_DISABLED_CONTROL_NORMAL,0x159f9b5a2920df4e, CommandGetDisabledControlNormal);
	SCR_REGISTER_SECURE(GET_DISABLED_CONTROL_UNBOUND_NORMAL,0x1ec2077a4d963881, CommandGetDisabledControlUnboundNormal);
	SCR_REGISTER_SECURE(GET_CONTROL_HOW_LONG_AGO,0xadb86701a67cb7e4, CommandControlHowLongAgo);
	SCR_REGISTER_UNUSED(GET_DISABLED_CONTROL_HOW_LONG_AGO,0x8587c8c397743fd1, CommandDisabledControlHowLongAgo);
	SCR_REGISTER_SECURE(IS_USING_KEYBOARD_AND_MOUSE,0x3a76a0944be2c291, CommandIsUsingKeyboardAndMouse);
	SCR_REGISTER_SECURE(IS_USING_CURSOR,0xe360d4c4ce76e4bb, CommandIsUsingCursor);
	SCR_REGISTER_SECURE(SET_CURSOR_POSITION,0x2969b478ff4db4df, CommandSetCursorPosition);
	SCR_REGISTER_SECURE(IS_USING_REMOTE_PLAY,0xff31e3ef4ed19ceb, CommandIsUsingRemotePlay);
	SCR_REGISTER_SECURE(HAVE_CONTROLS_CHANGED,0xb1c1e679bd17a4f0, CommandHaveControlsChanged);


	SCR_REGISTER_UNUSED(GET_CONTROL_INSTRUCTIONAL_BUTTON,0x43366bdb17bb18dc, CommandGetInputInstructionalButton);
	SCR_REGISTER_SECURE(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0x2018949b2c9fd96a, CommandGetInputInstructionalButtonsString);
	SCR_REGISTER_UNUSED(GET_CONTROL_GROUP_INSTRUCTIONAL_BUTTON,0x4463abaac9f67f70, CommandGetInputGroupInstructionalButton);
	SCR_REGISTER_SECURE(GET_CONTROL_GROUP_INSTRUCTIONAL_BUTTONS_STRING,0x19214818f925d149, CommandGetInputGroupInstructionalButtonsString);

	SCR_REGISTER_UNUSED(GET_POSITION_OF_ANALOGUE_STICKS,0x4188d4863c710507, CommandGetPositionOfAnalogueSticks);

	SCR_REGISTER_SECURE(SET_CONTROL_LIGHT_EFFECT_COLOR,0x2638db8310d6ee6b, CommandSetControlLightEffectColor);
	SCR_REGISTER_SECURE(CLEAR_CONTROL_LIGHT_EFFECT,0xe16df1340f6a4f39, CComandClearControlEffect);

	SCR_REGISTER_UNUSED(IS_DEBUG_KEY_PRESSED,0x70cea47cea5148b2, CommandIsDebugKeyPressed);
	SCR_REGISTER_UNUSED(IS_DEBUG_KEY_JUST_PRESSED,0x765fbb5f50ea1ba4, CommandIsDebugKeyJustPressed);
	SCR_REGISTER_UNUSED(IS_DEBUG_KEY_RELEASED,0x9283c20d88f9aa74, CommandIsDebugKeyReleased);
	SCR_REGISTER_UNUSED(IS_DEBUG_KEY_JUST_RELEASED,0xea3a04fc4ef2369a, CommandIsDebugKeyJustReleased);
	SCR_REGISTER_UNUSED(IS_KEYBOARD_KEY_PRESSED,0x5c5e9e56793fe873, CommandIsKeyboardKeyPressed);
	SCR_REGISTER_UNUSED(IS_KEYBOARD_KEY_JUST_PRESSED,0xd8a25bbf7491d032, CommandIsKeyboardKeyJustPressed);
	SCR_REGISTER_UNUSED(IS_KEYBOARD_KEY_RELEASED,0x54513f1f06e805b9, CommandIsKeyboardKeyReleased);
	SCR_REGISTER_UNUSED(IS_KEYBOARD_KEY_JUST_RELEASED,0x1c31a69e6cc1e85f, CommandIsKeyboardKeyJustReleased);
	SCR_REGISTER_UNUSED(IS_GAME_KEYBOARD_KEY_PRESSED,0x5985a1aa8005975a, CommandIsGameKeyboardKeyPressed);
	SCR_REGISTER_UNUSED(IS_GAME_KEYBOARD_KEY_JUST_PRESSED,0xc956318cc1099f68, CommandIsGameKeyboardKeyJustPressed);
	SCR_REGISTER_UNUSED(IS_GAME_KEYBOARD_KEY_RELEASED,0x9e122ff04ae5d18d, CommandIsGameKeyboardKeyReleased);
	SCR_REGISTER_UNUSED(IS_GAME_KEYBOARD_KEY_JUST_RELEASED,0x3665b5dfbe67b932, CommandIsGameKeyboardKeyJustReleased);
	SCR_REGISTER_UNUSED(GET_KEYBOARD_MODE,0x165be0f2a9685a34, CommandGetKeyboardMode);
	SCR_REGISTER_UNUSED(SET_KEYBOARD_MODE,0xbb6490cf594af527, CommandSetKeyboardMode);
#if !__FINAL
	SCR_REGISTER_UNUSED(FAKE_KEY_PRESS,0x8fc2e3197963de0a, CommandFakeKeyPress);
#endif
	SCR_REGISTER_SECURE(SET_CONTROL_SHAKE,0xf7a14a1a76b6dd17, CommandShakeControl);
	SCR_REGISTER_SECURE(SET_CONTROL_TRIGGER_SHAKE,0xf8bf168f565017ef, CommandShakeControlTrigger);
	SCR_REGISTER_SECURE(STOP_CONTROL_SHAKE,0xf4ab1adb142fd39d, CommandStopControlShake);
	SCR_REGISTER_SECURE(SET_CONTROL_SHAKE_SUPPRESSED_ID,0x6120122451823936, CommandSetControlShakeSuppressedId);
	SCR_REGISTER_SECURE(CLEAR_CONTROL_SHAKE_SUPPRESSED_ID,0xf3f6c6e90b9afc5d, CommandClearControlShakeSuppressedId);
	SCR_REGISTER_SECURE(IS_LOOK_INVERTED,0xc63a09aa9006f776, CommandIsLookInverted);
	SCR_REGISTER_SECURE(IS_MOUSE_LOOK_INVERTED,0x2ed671f67654fd28, CommandIsMouseLookInverted);
	SCR_REGISTER_SECURE(GET_LOCAL_PLAYER_AIM_STATE,0x6f8ba9ef200e4310, CommandGetLocalPlayerAimState);
	SCR_REGISTER_SECURE(GET_LOCAL_PLAYER_GAMEPAD_AIM_STATE,0x4d3f908dba3908e4, CommandGetLocalPlayerGamepadAimState);
	SCR_REGISTER_SECURE(GET_IS_USING_ALTERNATE_HANDBRAKE,0xb6b1a58467f8a268, CommandIsUsingAlternateHandbrake);
	SCR_REGISTER_SECURE(GET_IS_USING_ALTERNATE_DRIVEBY,0xc77fd7c62139abaa, CommandIsUsingAlternateDriveby);
	SCR_REGISTER_SECURE(GET_ALLOW_MOVEMENT_WHILE_ZOOMED,0x78711a80544b2439, CommandIsMovementWhileZoomedEnabled);
	SCR_REGISTER_SECURE(SET_PLAYERPAD_SHAKES_WHEN_CONTROLLER_DISABLED,0x802687ca71bc8666, CommandSetPlayerpadShakesWhenControllerDisabled);
	SCR_REGISTER_UNUSED(IS_USING_STANDARD_CONTROLS,0xb36d171806697a81, CommandUsingStandardControls);
	SCR_REGISTER_UNUSED(GET_PAD_PITCH_ROLL,0xeae8ff32a35b6797, CommandGetPadPitchRoll);
	SCR_REGISTER_UNUSED(HAS_RELOADED_WITH_MOTION_CONTROL,0xcabdedf70abe69db, CommandHasReloadedWithMotionControl);
	SCR_REGISTER_UNUSED(SET_ALL_MOTION_CONTROL_PREFERENCES,0xec16f157ca752992,	CommandSetAllMotionControlPreferencesOnOff);
	SCR_REGISTER_UNUSED(GET_MOTION_CONTROL_PREFERENCE,0x8e4183c5edc441b3, CommandGetMotionControlPreference);
	SCR_REGISTER_UNUSED(GET_MOUSE_POSITION,0x5a21367565868156, CommandGetMousePosition);
	SCR_REGISTER_UNUSED(IS_MOUSE_BUTTON_JUST_PRESSED,0x4b89e34427b3875c, CommandIsMouseButtonJustPressed);
	SCR_REGISTER_UNUSED(IS_MOUSE_BUTTON_PRESSED,0xa9eba80bd0e8cf96, CommandIsMouseButtonPressed);
	SCR_REGISTER_UNUSED(IS_MOUSE_BUTTON_JUST_RELEASED,0x4474cdf81869ab92, CommandIsMouseButtonJustReleased);
	SCR_REGISTER_UNUSED(IS_MOUSE_BUTTON_RELEASED,0x4ecabb4d295ea77b, CommandIsMouseButtonReleased);
	SCR_REGISTER_SECURE(SET_INPUT_EXCLUSIVE,0x07899aaa5d680386, CommandSetInputExclusive);
	SCR_REGISTER_SECURE(DISABLE_CONTROL_ACTION,0x7653d561c9574763,	CommandDisableControlAction);
	SCR_REGISTER_SECURE(ENABLE_CONTROL_ACTION,0x5089dd830fa61d71,	CommandEnableControlAction);
	SCR_REGISTER_SECURE(DISABLE_ALL_CONTROL_ACTIONS,0x58699da34f83b510,	CommandDisableAllControlActions);
	SCR_REGISTER_SECURE(ENABLE_ALL_CONTROL_ACTIONS,0xa308468b6494bc8b,	CommandEnableAllControlActions);
	SCR_REGISTER_UNUSED(DISABLE_CONTROL_BUTTON,0x2a51b0600c959ac9,	CommandDisableControlButton);
	SCR_REGISTER_SECURE(INIT_PC_SCRIPTED_CONTROLS,0x9e4a185a00c358e1, CommandInitPCScriptedControls);
	SCR_REGISTER_SECURE(SWITCH_PC_SCRIPTED_CONTROLS,0x870fdfbb89c863ca, CommandSwitchPCScriptedControls);
	SCR_REGISTER_SECURE(SHUTDOWN_PC_SCRIPTED_CONTROLS,0x027f803a942fb98f, CommandShutdownPCScriptedControls);
	SCR_REGISTER_SECURE(ALLOW_ALTERNATIVE_SCRIPT_CONTROLS_LAYOUT,0x7bc5e5a5ff7f278f, CommandAllowAlternativeScriptControlsLayout);

	SCR_REGISTER_UNUSED(COMPANION_IS_DEVICE_CONNECTED,0x012fe1d3a9ed3f8b, CommandGetIfCompanionDeviceIsConnected);
}


}	//	end of namespace pad_commands

