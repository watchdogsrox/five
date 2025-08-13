//
// name:		control.cpp
// description:	Class converting controller inputs into player actions
#include "system/control.h"

#include <stdio.h>

#if __PPU
// PS3 headers
#include <cell/error.h>
#include <sysutil/sysutil_sysparam.h>
#endif //__PPU

// Rage headers
#include "input/input.h"
#include "fragment/type.h"
#include "system/ipc.h"
#include "system/param.h"
#include "profile/timebars.h"
#include "fwsys/timer.h"
#include "rline/rlpc.h"

#if __WIN32PC
#include "atl/map.h"
#include "string/unicode.h"
#endif // __WIN32PC

// game headers
#include "Frontend/PauseMenu.h"
#include "Frontend/ProfileSettings.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "system/controlmgr.h"
#include "system/keyboard.h"
#include "Peds/PlayerSpecialAbility.h"
#if __BANK
#	include "text/TextFormat.h"
#endif
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"

#if KEYBOARD_MOUSE_SUPPORT
#include "stats/StatsInterface.h"
#endif // KEYBOARD_MOUSE_SUPPORT

//OPTIMISATIONS_OFF()

RAGE_DEFINE_CHANNEL(Control)

#define DEFAULT_SETTINGS_EXT		"meta"
#define DEFAULT_SETTINGS_DIR		"common:/data/control/"

#define INPUT_SETTINGS_FILE                      DEFAULT_SETTINGS_DIR "settings."                       DEFAULT_SETTINGS_EXT

#define COMMON_LAYOUT_FILE						 DEFAULT_SETTINGS_DIR "common."							DEFAULT_SETTINGS_EXT

#define STANDARD_LAYOUT_FILE                     DEFAULT_SETTINGS_DIR "standard."                       DEFAULT_SETTINGS_EXT
#define TRIGGER_SWAP_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "trigger_swap."                   DEFAULT_SETTINGS_EXT
#define SOUTHPAW_LAYOUT_FILE                     DEFAULT_SETTINGS_DIR "southpaw."                       DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TRIGGER_SWAP_LAYOUT_FILE        DEFAULT_SETTINGS_DIR "southpaw_trigger_swap."          DEFAULT_SETTINGS_EXT

#define STANDARD_SCRIPT_LAYOUT_FILE              DEFAULT_SETTINGS_DIR "standard_scripted."              DEFAULT_SETTINGS_EXT
#define TRIGGER_SWAP_SCRIPT_LAYOUT_FILE          DEFAULT_SETTINGS_DIR "trigger_swap_scripted."          DEFAULT_SETTINGS_EXT
#define SOUTHPAW_SCRIPT_LAYOUT_FILE              DEFAULT_SETTINGS_DIR "southpaw_scripted."              DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TRIGGER_SWAP_SCRIPT_LAYOUT_FILE DEFAULT_SETTINGS_DIR "southpaw_trigger_swap_scripted." DEFAULT_SETTINGS_EXT

#define STANDARD_TPS_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "standard_tps."                   DEFAULT_SETTINGS_EXT
#define TRIGGER_SWAP_TPS_LAYOUT_FILE             DEFAULT_SETTINGS_DIR "trigger_swap_tps."               DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TPS_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "southpaw_tps."                   DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT_FILE    DEFAULT_SETTINGS_DIR "southpaw_trigger_swap_tps."      DEFAULT_SETTINGS_EXT

#define STANDARD_FPS_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "standard_fps."                   DEFAULT_SETTINGS_EXT
#define TRIGGER_SWAP_FPS_LAYOUT_FILE             DEFAULT_SETTINGS_DIR "trigger_swap_fps."               DEFAULT_SETTINGS_EXT
#define SOUTHPAW_FPS_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "southpaw_fps."                   DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT_FILE    DEFAULT_SETTINGS_DIR "southpaw_trigger_swap_fps."      DEFAULT_SETTINGS_EXT

#define STANDARD_FPS_ALTERNATE_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "standard_fps_alternate."                   DEFAULT_SETTINGS_EXT
#define TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT_FILE             DEFAULT_SETTINGS_DIR "trigger_swap_fps_alternate."               DEFAULT_SETTINGS_EXT
#define SOUTHPAW_FPS_ALTERNATE_LAYOUT_FILE                 DEFAULT_SETTINGS_DIR "southpaw_fps_alternate."                   DEFAULT_SETTINGS_EXT
#define SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT_FILE    DEFAULT_SETTINGS_DIR "southpaw_trigger_swap_fps_alternate."      DEFAULT_SETTINGS_EXT

#define DEFAULT_ACCEPT_CANCEL_SETTINGS_FILE      DEFAULT_SETTINGS_DIR "standard_accept_cancel."         DEFAULT_SETTINGS_EXT
#define ALTERNATE_ACCEPT_CANCEL_SETTINGS_FILE    DEFAULT_SETTINGS_DIR "alternate_accept_cancel."        DEFAULT_SETTINGS_EXT

#define DEFAULT_DUCK_HANDBRAKE_SETTINGS_FILE      DEFAULT_SETTINGS_DIR "standard_duck_handbrake."       DEFAULT_SETTINGS_EXT
#define ALTERNATE_DUCK_HANDBRAKE_SETTINGS_FILE    DEFAULT_SETTINGS_DIR "alternate_duck_handbrake."      DEFAULT_SETTINGS_EXT

#if RSG_ORBIS
#define GESTURES_LAYOUT_FILE					 DEFAULT_SETTINGS_DIR "gestures."						DEFAULT_SETTINGS_EXT
#endif

#if KEYBOARD_MOUSE_SUPPORT
#define PC_SETTINGS_DIR			"platform:/data/control/"
#define PC_DEVICE_SETTINGS_DIR	PC_SETTINGS_DIR "Devices/"

#define PC_INPUT_SETTINGS_FILE              PC_SETTINGS_DIR "settings."                     DEFAULT_SETTINGS_EXT
#define PC_DEFAULT_SETTINGS_FILE			PC_SETTINGS_DIR	"default."						DEFAULT_SETTINGS_EXT
#define PC_FIXED_SETTINGS_FILE				PC_SETTINGS_DIR	"fixed."						DEFAULT_SETTINGS_EXT
#define PC_STANDARD_MOUSE_SETTINGS_FILE		PC_SETTINGS_DIR "standard_mouse."				DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_SETTINGS_FILE		PC_SETTINGS_DIR "inverted_mouse."				DEFAULT_SETTINGS_EXT
#define PC_GAMEPAD_DEFINITION_FILE			PC_SETTINGS_DIR	"gamepad."						DEFAULT_SETTINGS_EXT
#define PC_DYNAMIC_SETTINGS_FILE			PC_SETTINGS_DIR "dynamic."						DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_CAR_CENTERED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_car_centered."	DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_CAR_SCALED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_car_scaled."		DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_FLY_CENTERED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_fly_centered."	DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_FLY_SCALED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_fly_scaled."		DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_SUB_CENTERED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_sub_centered."	DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_SUB_SCALED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_sub_scaled."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_FLY_CENTERED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_fly_centered."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_FLY_SCALED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_fly_scaled."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_SUB_CENTERED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_sub_centered."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_SUB_SCALED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_sub_scaled."		DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_FLY_CENTERED_SWAPPED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_fly_centered_xy_swapped."	DEFAULT_SETTINGS_EXT
#define PC_MOUSE_VEHICLE_FLY_SCALED_SWAPPED_FILE	PC_SETTINGS_DIR	"mouse_vehicle_fly_scaled_xy_swapped."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_FLY_CENTERED_SWAPPED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_fly_centered_xy_swapped."		DEFAULT_SETTINGS_EXT
#define PC_INVERTED_MOUSE_VEHICLE_FLY_SCALED_SWAPPED_FILE	PC_SETTINGS_DIR	"inverted_mouse_vehicle_fly_scaled_xy_swapped."		DEFAULT_SETTINGS_EXT

#define KEYBOARD_LAYOUT_DIR					PC_SETTINGS_DIR	"Keyboard Layout/"

#define KEYBOARD_LAYOUT_EXTENSION			".meta"
#define DEFAULT_KEYBOARD_LAYOUT				"en"


#define USER_SETTINGS_EXT			"xml"
#define AUTOSAVE_EXT				"autosave"

// TODO: update once the user path is known.
#define PC_USER_SETTINGS_DIR			"control/"

#define PC_USER_SETTINGS_FILE			"user."		USER_SETTINGS_EXT
#define PC_AUTOSAVE_USER_SETTINGS_FILE	"user."		AUTOSAVE_EXT
#define PC_USER_GAMEPAD_DEFINITION_FILE	"gamepad."	USER_SETTINGS_EXT

#define BUFFER_SIZE	50

#	if __BANK
	PARAM(noKeyboardMouseControls,"[control] Stops PC controls from being loaded.");
#	endif // __BANK

NOSTRIP_PC_PARAM(keyboardLocal, "[control] Sets the keyboard layout to the specified region.");

#if !RSG_FINAL
XPARAM(enableXInputEmulation);
#endif // !RSG_FINAL

#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_STEAM_CONTROLLER
#define STEAM_CONTROLLER_SETTINGS_FILE    PC_SETTINGS_DIR "steam_controller."      DEFAULT_SETTINGS_EXT
PARAM(steamControllerSettingsFile, "[control] The settings file for the steam controller.");
#endif // USE_STEAM_CONTROLLER

#define ALLOW_USER_CONTROLS (1 && KEYBOARD_MOUSE_SUPPORT)

AI_OPTIMISATIONS()

#if FPS_MODE_SUPPORTED
// TODO: Remove this once this is an option in the pause menu.
XPARAM(firstperson);
#endif // FPS_MODE_SUPPORTED



CControl::Settings CControl::ms_settings = CControl::Settings();
ControlInput::ControlSettings CControl::ms_StandardScriptedMappings;
s32 CControl::ms_LastTimeTouched = 0;


rage::sysIpcCurrentThreadId CControl::ms_threadId = sysIpcCurrentThreadIdInvalid;
rage::dev_u32 CControl::SYSTEM_EVENT_TIMOUT_DURATION = 1000u;

// TODO: For now, use the enum name until we create proper strings.
extern const char* parser_rage__Mapper_Strings[];


extern ::rage::parEnumData parser_rage__InputType_Data;
extern ::rage::parEnumData parser_rage__InputGroup_Data;
extern ::rage::parEnumData parser_rage__ioMapperParameter_Data;
extern ::rage::parEnumData parser_rage__ioMapperSource_Data;

#if !__FINAL
atString CControl::ms_ThreadlStack;
sysCriticalSectionToken CControl::ms_Lock;
#endif // !__FINAL

/*
// enum of standard default types for each supported pad style (360 or PS2)
enum {
		DEFAULT_360_PAD_CONTROLS_CONFIG = 0,
		DEFAULT_PS2_PAD_CONTROLS_CONFIG,
		MAX_DEFAULT_PAD_CONTROLS_CONFIGS
	 };
*/
//
// name:		CControl::CControl
// description:	constructor
CControl::CControl()
{
}

CControl::~CControl()
{
	g_SysService.RemoveDelegate(&m_Delegate);
	m_Delegate.Reset();
}

void CControl::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		ms_LastTimeTouched = fwTimer::GetTimeInMilliseconds();
		WIN32PC_ONLY(ShutdownPCScriptControlMappings());
	}
}
//
// name:		CControl::Init
// description:	initialise structures
void CControl::Init(s32 device)
{
	sysCriticalSection lock(m_Cs);

	if(m_Delegate.IsBound() == false)
	{
		m_Delegate.Bind(this, &CControl::HandleSystemEvent);
		g_SysService.AddDelegate(&m_Delegate);
	}

	for(s32 i = 0; i < MAX_INPUTS; ++i)
	{
		DEV_ONLY(m_inputs[i].SetId(i));
		m_isInputMapped[i] = false;
	}

	SetPad(device);
	ms_settings.Init();

	ms_LastTimeTouched=fwTimer::GetTimeInMilliseconds();

	m_InputDisableEndTime = 0;
	m_DisableOptions = ioValue::DisableOptions();
	m_bInputsStillDisabled = false;

	m_useAlternateScriptedControlsNextFrame = false;

	FPS_MODE_SUPPORTED_ONLY(m_FpsControlLayout = INVALID_LAYOUT);
	m_TpsControlLayout    = INVALID_LAYOUT;
	m_HandbrakeControl	  = -1; // Invalid.
	m_scriptedInputLayout = STANDARD_TPS_LAYOUT;

	m_ToggleRunOn         = false;
	m_WasControlsRefreshedThisFrame = false;
	m_HasScriptCheckControlsRefresh = false;
	m_WasControlsRefreshedLastFrame = false;
	m_WasUsingFpsMode = false;

#if KEYBOARD_MOUSE_SUPPORT
	m_currentMapping.m_ScanMapper = NULL;

	m_LastKnownSource = ioSource::UNKNOWN_SOURCE;

	m_MouseSettings				= 0u;
	m_HasLoadedDefaultControls	= false;

	m_IsUserSignedIn			= false;
	m_ToggleAim					= false;

	m_ToggleAimOn				= false;
	m_ResetToggleAim			= false;
	m_ToggleBikeSprintOn		= false;
	m_BackButtonEnableTime		= 0;
	m_LastFrameDeviceIndex		= ioSource::IOMD_UNKNOWN;

	m_RecachedConflictCount		= true;
	m_CachedConflictCount		= 0u;

	m_DriveCameraToggleOn		= false;
	m_UseSecondaryDriveCameraToggle = false;

	m_CurrentLoadedDynamicScheme	   = atHashString("", 0);
	m_CurrentScriptDynamicScheme	   = atHashString("", 0);
	m_CurrentCodeOverrideDynamicScheme = atHashString("", 0);

#if DEBUG_DRAW
	m_OnFootMouseSensitivity	= 0.0f;
	m_InVehicleMouseSensitivity	= 0.0f;
#endif // DEBUG_DRAW
#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_STEAM_CONTROLLER
	m_SteamController = 0;
	m_SteamActionSet  = 0;
	m_SteamHandleMap.Reset();
#endif // USE_STEAM_CONTROLLER

#if __WIN32PC
	// TR TODO: Tie this to the pause menu.
#if __BANK
	safecpy(m_RagPCControlScheme, "Default", RagData::TEXT_SIZE);
	m_RagLoadedPCControlScheme[0] = '\0';
#endif // __BANK

	// TR TODO: Read these from the frontend.
	m_MouseVehicleCarAutoCenter = true;
	m_MouseVehicleFlyAutoCenter = true;
	m_MouseVehicleSubAutoCenter = false;

	m_UseExtraDevices = false;
#endif // __WIN32PC

#if LIGHT_EFFECTS_SUPPORT
#if RSG_BANK
	m_RagLightEffectEnabled = false;
	m_RagLightEffectColor = Color32(255, 255, 255);
	m_RagLightEffect = ioConstantLightEffect(m_RagLightEffectColor);
	m_LightEffects[RAG_LIGHT_EFFECT].m_Effect = NULL;
	m_LightEffects[RAG_LIGHT_EFFECT].m_StartTime = 0u;
#endif // RSG_BANK
	m_ActiveLightDevices.Reset();

#if RSG_ORBIS
	m_PadLightDevice = ioPadLightDevice(device);
	m_ActiveLightDevices.Push(&m_PadLightDevice);
#elif RSG_PC
	if(m_LogitechLightDevice.IsValid())
	{
		m_ActiveLightDevices.Push(&m_LogitechLightDevice);
	}
#endif // RSG_ORBIS
	m_LightEffectsEnabled = true;
	ResetLightDeviceColor();
#endif // LIGHT_EFFECTS_SUPPORT

#if RSG_ORBIS
	m_IsRemotePlayer = false;
	m_IsAcceptCrossSetting = 0;
#endif // RSG_ORBIS

#if USE_ACTUATOR_EFFECTS
	m_Recoil = fwRecoilEffect();
	m_Rumble = ioRumbleEffect();
#endif // USE_ACTUATOR_EFFECTS

	m_ShakeSupressId = NO_SUPPRESS;

	m_SystemFrontendBackOccurredTime  = 0u;
	m_SystemPhoneBackOccurredTime = 0u;
	m_SystemPlayPauseOccurredTime = 0u;
	m_SystemFrontendViewOccuredTime = 0u;
	m_SystemGameViewOccuredTime = 0u;
}

//
// name:		CControl::SetPad
// description:	Set the pad number for this control
void CControl::SetPad(s32 device)
{
	m_padNum = device;
#if LIGHT_EFFECTS_SUPPORT
	ORBIS_ONLY(m_PadLightDevice = ioPadLightDevice(device));
#endif // LIGHT_EFFECTS_SUPPORT

	for(u32 i = 0; i < MAPPERS_MAX; ++i)
		m_Mappers[i].SetPlayer(m_padNum);
}

bool CControl::GetPedSprintIsDown() const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun())
	{
		if(m_ToggleRunOn)
		{
			return m_inputs[INPUT_SPRINT].IsUp() || !m_inputs[INPUT_SPRINT].HistoryHeldDown(KEYBOARD_START_SPRINT_HELD_TIME);
		}
		else
		{
			return m_inputs[INPUT_SPRINT].HistoryHeldDown(KEYBOARD_START_SPRINT_HELD_TIME);
		}
	}

	return m_inputs[INPUT_SPRINT].IsDown();
}

bool CControl::GetPedSprintIsPressed() const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun() && m_ToggleRunOn)
	{
		return m_inputs[INPUT_SPRINT].IsReleased();
	}

	return m_inputs[INPUT_SPRINT].IsPressed();
}

bool CControl::GetPedSprintIsReleased() const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun() && m_ToggleRunOn)
	{
		return m_inputs[INPUT_SPRINT].IsPressed();
	}

	return m_inputs[INPUT_SPRINT].IsReleased();
}

bool CControl::GetPedSprintHistoryIsPressed(u32 durationMS) const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun() && m_ToggleRunOn)
	{
		return m_inputs[INPUT_SPRINT].HistoryReleased(durationMS);
	}

	return m_inputs[INPUT_SPRINT].HistoryPressed(durationMS);
}

bool CControl::GetPedSprintHistoryHeldDown(u32 durationMS) const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun() && m_ToggleRunOn)
	{
		return m_inputs[INPUT_SPRINT].HistoryReleased(durationMS);
	}

	return m_inputs[INPUT_SPRINT].HistoryHeldDown(durationMS);
}

bool CControl::GetPedSprintHistoryReleased(u32 durationMS) const
{
	if(m_inputs[INPUT_SPRINT].IsEnabled() && CanUseToggleRun() && m_ToggleRunOn)
	{
		return m_inputs[INPUT_SPRINT].HistoryPressed(durationMS);
	}

	return m_inputs[INPUT_SPRINT].HistoryReleased(durationMS);
}

bool CControl::GetVehiclePushbikeSprintIsDown() const
{
#if KEYBOARD_MOUSE_SUPPORT
	if(m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].IsEnabled() && IsUsingKeyboardAndMouseForMovement() && m_ToggleBikeSprintOn)
	{
		return m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].IsUp();
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].IsDown();
}

bool CControl::IsVehicleAttackInputDown() const
{
#if KEYBOARD_MOUSE_SUPPORT
	// PREF_ALTERNATE_DRIVEBY shouldn't change mouse.
	if(WasKeyboardMouseLastKnownSource())
	{
		return GetVehicleAttack().IsDown();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY))
	{
		return GetVehicleAttack().IsDown();
	}
	else
	{
		return GetVehicleAim().IsDown();
	}
}

bool CControl::IsVehicleAttackInputReleased() const
{
#if KEYBOARD_MOUSE_SUPPORT
	// PREF_ALTERNATE_DRIVEBY shouldn't change mouse.
	if(WasKeyboardMouseLastKnownSource())
	{
		return GetVehicleAttack().IsReleased();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY))
	{
		return GetVehicleAttack().IsReleased();
	}
	else
	{
		return GetVehicleAim().IsReleased();
	}
}

float CControl::GetVehicleAttackInputNorm() const
{
#if KEYBOARD_MOUSE_SUPPORT
	// PREF_ALTERNATE_DRIVEBY shouldn't change mouse.
	if(WasKeyboardMouseLastKnownSource())
	{
		return GetVehicleAttack().GetNorm();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY))
	{
		return GetVehicleAttack().GetNorm();
	}
	else
	{
		return GetVehicleAim().GetNorm();
	}
}

void CControl::HandleSystemEvent(sysServiceEvent* evt)
{
	if(evt)
	{
		if(evt->GetType() == sysServiceEvent::BACK)
		{
			m_SystemFrontendBackOccurredTime = fwTimer::GetSystemTimeInMilliseconds();
			m_SystemPhoneBackOccurredTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		else if(evt->GetType() == sysServiceEvent::PAUSE || evt->GetType() == sysServiceEvent::PLAY || evt->GetType() == sysServiceEvent::MENU)
		{
			m_SystemPlayPauseOccurredTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		else if(evt->GetType() == sysServiceEvent::VIEW)
		{
			m_SystemFrontendViewOccuredTime = fwTimer::GetSystemTimeInMilliseconds();
			m_SystemGameViewOccuredTime     = fwTimer::GetSystemTimeInMilliseconds();
		}
	}
}

//
// name:		CControl::LoadControls
// description:	save the controls to a file
bool CControl::LoadControls(char *pFilename, s32 iVersionNum)
{
	bool result = false;
	fiStream *inStream = ASSET.Open(pFilename,"cfg");
	if (inStream)
	{
		// we've got a file, so we need to clear controls before we load it
		for(u32 i = 0; i < MAPPERS_MAX; ++i)
			m_Mappers[i].ClearValues();

		result = m_Mappers[ON_FOOT].Load(inStream,iVersionNum);

		if (result)
		{
			result = m_Mappers[MELEE].Load(inStream,iVersionNum);
		}

		if (result)
		{
			result = m_Mappers[IN_VEHICLE].Load(inStream,iVersionNum);
		}
		
		if (result)
		{
			result = m_Mappers[PARACHUTE].Load(inStream,iVersionNum);
		}

		if (result)
		{
			result = m_Mappers[FRONTEND].Load(inStream,iVersionNum);
		}

		if (result)
		{
			result = m_Mappers[REPLAY].Load(inStream,iVersionNum);
		}
		
		inStream->Close();
	}

	return result;
}


//
// name:		CControl::SaveControls
// description:	save the controls to a file
bool CControl::SaveControls(char *pFilename, s32 iVersionNum)
{
	bool result = false;
	fiStream *outStream = ASSET.Create(pFilename,"cfg");
	if (outStream)
	{
		result = m_Mappers[ON_FOOT].Save(outStream,iVersionNum);

		if (result)
			result = m_Mappers[MELEE].Save(outStream,iVersionNum);

		if (result)
			result = m_Mappers[IN_VEHICLE].Save(outStream,iVersionNum);

		if (result)
			result = m_Mappers[FRONTEND].Save(outStream,iVersionNum);

		if (result)
			result = m_Mappers[REPLAY].Save(outStream,iVersionNum);

		outStream->Close();
	}

	return result;
}


//
// name:		CControl::AddActionToString
// description:	adds the action to pString that can be displayed
bool CControl::AddActionToString(s32 action, char *pString, ioMapper *map_type)
{
	char this_string[50];
	bool bDirectString = true;


	switch (map_type->GetMapperSource(map_type->FindValue(m_inputs[action])))
	{
		case IOMS_DIGITALBUTTON_AXIS:
		{
			switch(action)
			{
				case INPUT_MOVE_LR:
				case INPUT_MOVE_UD:
				case INPUT_LOOK_LR:
				case INPUT_LOOK_UD:
				case INPUT_VEH_MOVE_UD:
				case INPUT_VEH_MOVE_LR:
				case INPUT_VEH_GUN_LR:
				case INPUT_VEH_GUN_UD:
				{
					unsigned int other_key = (map_type->GetParameter(map_type->FindValue(m_inputs[action])));  // get key on opposite axis

					other_key &= 0xFF;
	
					unsigned int c = 1;
					
					sprintf(this_string, "NOPAD");

					for (int cc = 1; cc < 32; cc++)
					{
						if (other_key & c)
						{
							sprintf(this_string, "PAD %d", cc);  // sets the correct pad value
							bDirectString = false;  // use this actual string rather than from text file
							break;
						}

						c *= 2;
					}

					break;
				}

				default:
				{
					Assert(0);
					// cannot be used
					break;
				}
			}
			 
			break;
		}

		case IOMS_KEYBOARD:
		{
			sprintf(this_string, "ACTN_%d", map_type->GetParameter(map_type->FindValue(m_inputs[action])));
			break;
		}
		case IOMS_JOYSTICK_BUTTON:
		case IOMS_PAD_DIGITALBUTTON:
		case IOMS_PAD_DIGITALBUTTONANY:
		case IOMS_PAD_ANALOGBUTTON:
		{
			unsigned int pad_num = map_type->GetParameter(map_type->FindValue(m_inputs[action]));

			unsigned int c = 1;
			
			sprintf(this_string, "NOPAD");

			for (int cc = 1; cc < 32; cc++)
			{
				if (pad_num & c)
				{
					sprintf(this_string, "PAD %d", cc);  // sets the correct pad value
					bDirectString = false;  // use this actual string rather than from text file
					break;
				}

				c *= 2;
			}

			break;
		}

		case IOMS_MOUSE_BUTTON:
		{
			unsigned int but_num = map_type->GetParameter(map_type->FindValue(m_inputs[action]));

			switch (but_num)
			{
				case ioMouse::MOUSE_LEFT:
				{
					sprintf(this_string, "MS_LEFT");
					break;
				}
				case ioMouse::MOUSE_RIGHT:
				{
					sprintf(this_string, "MS_RIGHT");
					break;
				}
				case ioMouse::MOUSE_MIDDLE:
				{
					sprintf(this_string, "MS_MID");
					break;
				}
				case ioMouse::MOUSE_EXTRABTN1:
				{
					sprintf(this_string, "MS_BUT1");
					break;
				}
				case ioMouse::MOUSE_EXTRABTN2:
				{
					sprintf(this_string, "MS_BUT2");
					break;
				}
				case ioMouse::MOUSE_EXTRABTN3:
				{
					sprintf(this_string, "MS_BUT3");
					break;
				}
				case ioMouse::MOUSE_EXTRABTN4:
				{
					sprintf(this_string, "MS_BUT4");
					break;
				}
				case ioMouse::MOUSE_EXTRABTN5:
				{
					sprintf(this_string, "MS_BUT5");
					break;
				}
				default:
				{
					Assertf(0, "Unknown mouse button pressed");
					break;
				}
			}
			break;
		}

		case IOMS_PAD_AXIS:
		{
			switch(action)
			{
				case INPUT_MOVE_LR:
				case INPUT_MOVE_UD:
				case INPUT_LOOK_LR:
				case INPUT_LOOK_UD:
				case INPUT_VEH_MOVE_UD:
				case INPUT_VEH_MOVE_LR:
				case INPUT_VEH_GUN_LR:
				case INPUT_VEH_GUN_UD:
				{
					sprintf(this_string, "AXISLR_%d", map_type->GetParameter(map_type->FindValue(m_inputs[action])));
					break;
				}
				default:
				{
					Assert(0);
					// cannot be used
					break;
				}
			}
			
			break;
		}

		case IOMS_JOYSTICK_AXIS:
		{
			switch(action)
			{
				case INPUT_MOVE_LR:
				case INPUT_MOVE_UD:
				case INPUT_LOOK_LR:
				case INPUT_LOOK_UD:
				case INPUT_VEH_MOVE_UD:
				case INPUT_VEH_MOVE_LR:
				case INPUT_VEH_GUN_LR:
				case INPUT_VEH_GUN_UD:
				{
					sprintf(this_string, "AXISJ_%d", map_type->GetParameter(map_type->FindValue(m_inputs[action])));
					break;
				}

				default:
				{
					Assert(0);
					// cannot be used
					break;
				}
			}
			
			break;
		}

		case IOMS_MOUSE_ABSOLUTEAXIS:
		{
			switch(action)
			{
				case INPUT_MOVE_LR:
				case INPUT_MOVE_UD:
				case INPUT_LOOK_LR:
				case INPUT_LOOK_UD:
				case INPUT_VEH_MOVE_UD:
				case INPUT_VEH_MOVE_LR:
				case INPUT_VEH_GUN_LR:
				case INPUT_VEH_GUN_UD:
				{
					switch (map_type->GetParameter(map_type->FindValue(m_inputs[action])))
					{
						case IOM_AXIS_X:
						{
							sprintf(this_string, "AXISM_X");
							break;
						}

						case IOM_AXIS_Y:
						{
							sprintf(this_string, "AXISM_Y");
							break;
						}

						default:
						{
							Assert(0);
							// cannot be used
							break;
						}
					}
					
					break;
				}

				default:
				{
					Assert(0);
					// cannot be used
					break;
				}
			}
			
			break;
		}

		case IOMS_MOUSE_WHEEL:
		{
			switch(action)
			{
				case INPUT_MOVE_LR:
				case INPUT_MOVE_UD:
				case INPUT_LOOK_LR:
				case INPUT_LOOK_UD:
				case INPUT_VEH_MOVE_UD:
				case INPUT_VEH_MOVE_LR:
				case INPUT_VEH_GUN_LR:
				case INPUT_VEH_GUN_UD:
				{
					// cannot be assigned
					break;
				}

				default:
				{
					switch (map_type->GetParameter(map_type->FindValue(m_inputs[action])))
					{
						case IOM_WHEEL_UP:
						{
							sprintf(this_string, "AXISW_U");
							break;
						}

						case IOM_WHEEL_DOWN:
						{
							sprintf(this_string, "AXISW_D");
							break;
						}

						default:
						{
							Assert(0);
							// cannot be used
							break;
						}
					}
					
					break;
				}
			}
			
			break;
		}


		default:
		{
			// unknown input value:
			sprintf(this_string, "UNKNOWN");
			bDirectString = false;
			break;
		}
	}

	strcat(pString, this_string);

	return (bDirectString);  // whether to use this string or one from the text file (used for pad button text)
}

void CControl::EnableAlternateScriptedControlsLayout()
{
	sysCriticalSection lock(m_Cs);

	m_useAlternateScriptedControlsNextFrame = true;
	LoadScriptedSettings(false, false);
}

void CControl::UseDefaultScriptedControlsLayout()
{
	sysCriticalSection lock(m_Cs);
	m_useAlternateScriptedControlsNextFrame = false;
	LoadScriptedSettings(true, false);
}

void CControl::LoadScriptedSettings(bool loadStandardControls, bool forceLoad)
{
	eControlLayout desiredLayout = INVALID_LAYOUT;
	const ControlInput::ControlSettings* scriptSettings = NULL;

	if(loadStandardControls)
	{
		desiredLayout  = STANDARD_TPS_LAYOUT;
		scriptSettings = &ms_StandardScriptedMappings;
	}
#if FPS_MODE_SUPPORTED
	else if(IsUsingFpsMode())
	{
		desiredLayout  = ConvertToScriptedLayout(GetFpsLayout());
		scriptSettings = &m_AlternateFpsScriptedSettings;

	}
#endif // FPS_MODE_SUPPORTED
	else
	{
		desiredLayout = ConvertToScriptedLayout(GetTpsLayout());
		scriptSettings = &m_AlternateTpsScriptedSettings;
	}

	if((desiredLayout != m_scriptedInputLayout && scriptSettings != NULL) || forceLoad)
	{
		// remove current mappings
		for(s32 input = SCRIPTED_INPUT_FIRST; input <= SCRIPTED_INPUT_LAST; ++input)
		{
			ioMapper& mapper = m_Mappers[ms_settings.m_InputMappers[input]];
			mapper.RemoveDeviceMappings(m_inputs[input], ioSource::IOMD_DEFAULT);

#if RSG_PC
			if(m_UseExtraDevices)
			{
				for(s32 joystickIndex = 0; joystickIndex < ioJoystick::GetStickCount(); ++joystickIndex)
				{
					CPad& pad = CControlMgr::GetPad(joystickIndex);

					// if the pad is not connected then it is a DirectInput Device. If it is connected check that is is not using pad
					// rather than direct input mappings.
					if( !pad.IsConnected() || (!pad.IsXBox360CompatiblePad() && !pad.IsPs3Pad()) )
					{
						mapper.RemoveDeviceMappings(m_inputs[input], joystickIndex);
					}
				}
			}
#endif // RSG_PC
		}

		LoadSettings(*scriptSettings, ioSource::IOMD_DEFAULT);

#if RSG_PC
		if(m_UseExtraDevices)
		{
			for(s32 joystickIndex = 0; joystickIndex < ioJoystick::GetStickCount(); ++joystickIndex)
			{
				CPad& pad = CControlMgr::GetPad(joystickIndex);

				// if the pad is not connected then it is a DirectInput Device. If it is connected check that is is not using pad
				// rather than direct input mappings.
				if( !pad.IsConnected() || (!pad.IsXBox360CompatiblePad() && !pad.IsPs3Pad()) )
				{
					const ControlInput::Gamepad::Definition* definition = ms_settings.GetGamepadDefinition(ioJoystick::GetStick(joystickIndex));
					if(definition != NULL)
					{
						LoadGamepadControls(*definition, *scriptSettings, joystickIndex);
					}
				}
			}
		}
#endif // RSG_PC

		m_scriptedInputLayout = desiredLayout;
	}

}



#define PAD_OVERRIDE -1


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetDefaultLookInversions
// PURPOSE:	Set the look inversons based upon the users settings.
/////////////////////////////////////////////////////////////////////////////////////
void CControl::SetDefaultLookInversions()
{
	if(CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK))
	{
		m_inputs[INPUT_LOOK_UD].MakeInverted();
		m_inputs[INPUT_VEH_GUN_UD].MakeInverted();
		m_inputs[INPUT_SCALED_LOOK_UD].MakeInverted();
	}
	else
	{
		m_inputs[INPUT_LOOK_UD].MakeNormal();
		m_inputs[INPUT_VEH_GUN_UD].MakeNormal();
		m_inputs[INPUT_SCALED_LOOK_UD].MakeNormal();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetInitialDefaultMappings
// PURPOSE:	hard codes the initial values of the control mappings.
/////////////////////////////////////////////////////////////////////////////////////
void CControl::SetInitialDefaultMappings(bool forceReload, bool WIN32PC_ONLY(loadAutoSavedSettings) BANK_ONLY(, const char* customMappingsFile /*= NULL*/))
{
	sysCriticalSection lock(m_Cs);

	ClearInputMappings();

#if (RSG_PC && ALLOW_USER_CONTROLS) || (__BANK)
	bool result = false;
#endif

	// We are using the pause menu preferences instead of the CProfileSettings as the profile
	// settings do not appear to be updated when we are informed of a control change.
	const bool rebuild = ( forceReload || m_TpsControlLayout != GetTpsLayout() 
		FPS_MODE_SUPPORTED_ONLY(|| m_FpsControlLayout != GetFpsLayout())
		ORBIS_ONLY(|| m_IsRemotePlayer != GetPad().IsRemotePlayPad() || m_IsAcceptCrossSetting != CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS)) );

#if __BANK
	// Load custom settings path
	if(customMappingsFile != NULL)
	{
		ControlInput::ControlSettings tmpSettings;
		result = LoadSettingsFile(customMappingsFile, false, tmpSettings);
		if(result)
		{
			LoadSettings(tmpSettings, ioSource::IOMD_DEFAULT);
		}
	}

	if(!result)
#endif
	{
		if(rebuild)
		{
			//! reset handbrake control when we rebuild as otherwise they'll never be remapped unless we change profile option.
			m_HandbrakeControl = -1;
				 
			ClearToggleRun();

			// Clear previous settings.
			m_GamepadCommonSettings.m_Mappings.Resize(0);
			m_GamepadTpsBaseLayoutSettings.m_Mappings.Resize(0);
			m_GamepadTpsSpecificLayoutSettings.m_Mappings.Resize(0);

			m_GamepadAcceptCancelSettings.m_Mappings.Resize(0);
			m_GamepadDuckBrakeSettings.m_Mappings.Resize(0);
			ORBIS_ONLY(m_GamepadGestureSettings.m_Mappings.Resize(0));

#if FPS_MODE_SUPPORTED
			m_GamepadFpsBaseLayoutSettings.m_Mappings.Resize(0);
			m_GamepadFpsSpecificLayoutSettings.m_Mappings.Resize(0);

			// Load FPS mode settings files.
			eControlLayout fpsLayout = GetFpsLayout();
			LoadSettingsFile(GetGamepadBaseLayoutFile(fpsLayout), false, m_GamepadFpsBaseLayoutSettings);
			LoadSettingsFile(GetGamepadSpecificLayoutFile(fpsLayout), false, m_GamepadFpsSpecificLayoutSettings);
#endif // FPS_MODE_SUPPORTED

			// Load TPS mode settings files.
			eControlLayout tpsLayout = GetTpsLayout();
			LoadSettingsFile(COMMON_LAYOUT_FILE, false, m_GamepadCommonSettings);
			LoadSettingsFile(GetGamepadBaseLayoutFile(tpsLayout), false, m_GamepadTpsBaseLayoutSettings);
			LoadSettingsFile(GetGamepadSpecificLayoutFile(tpsLayout), false, m_GamepadTpsSpecificLayoutSettings);
			
			LoadSettingsFile(GetAcceptCancelLayoutFile(), false, m_GamepadAcceptCancelSettings);
			ORBIS_ONLY(LoadSettingsFile(GESTURES_LAYOUT_FILE, false, m_GamepadGestureSettings));
		}

		const s32 handbrakeControl = CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE);
		if(m_HandbrakeControl != handbrakeControl)
		{
			m_HandbrakeControl = handbrakeControl;
			LoadSettingsFile(GetDuckHandbrakeLayoutFile(), false, m_GamepadDuckBrakeSettings);
		}

		// Load mappings from settings.
		LoadSettings(m_GamepadCommonSettings, ioSource::IOMD_DEFAULT);

#if FPS_MODE_SUPPORTED
		// TODO: Use pause menu setting once it is hooked up!
		if(IsUsingFpsMode())
		{
			LoadSettings(m_GamepadFpsBaseLayoutSettings, ioSource::IOMD_DEFAULT);
			LoadSettings(m_GamepadFpsSpecificLayoutSettings, ioSource::IOMD_DEFAULT);
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			LoadSettings(m_GamepadTpsBaseLayoutSettings, ioSource::IOMD_DEFAULT);
			LoadSettings(m_GamepadTpsSpecificLayoutSettings, ioSource::IOMD_DEFAULT);
		}

		LoadSettings(m_GamepadAcceptCancelSettings, ioSource::IOMD_DEFAULT);
		LoadSettings(m_GamepadDuckBrakeSettings, ioSource::IOMD_DEFAULT);

		ORBIS_ONLY(LoadSettings(m_GamepadGestureSettings, ioSource::IOMD_DEFAULT));

		ORBIS_ONLY(m_IsRemotePlayer = GetPad().IsRemotePlayPad());
	}

	ORBIS_ONLY(m_IsAcceptCrossSetting = CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS));

	m_TpsControlLayout = GetTpsLayout();
	FPS_MODE_SUPPORTED_ONLY(m_FpsControlLayout = GetFpsLayout());
	
	if(rebuild)
	{
		LoadScriptedMappings();
	}

	// This actually loads the scripted controls.
	LoadScriptedSettings(true, true);

#if RSG_PC
	if(m_UseExtraDevices)
	{
		if(forceReload)
		{
			ms_settings.RebuildGamepadDefinitionList();
		}

		LoadDeviceControls(loadAutoSavedSettings);
	}
#endif // RSG_PC

#if KEYBOARD_MOUSE_SUPPORT
#if __BANK
	// in bank mode we can disable pc input to better work with rag.
	if(!PARAM_noKeyboardMouseControls.Get())
#endif // __BANK
	{
		// We are using the pause menu preferences instead of the CProfileSettings as the profile
		// settings do not appear to be updated when we are informed of a control change.
		const bool axisInversion = (CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK) != 0);
		const bool mouseInversion = (CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE) != 0);
		const bool mouseFlyingInversion = (CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE_FLYING) != 0);
		const bool mouseSubInversion = (CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE_SUB) != 0);
		const bool mouseFlyingXYSwap = (CPauseMenu::GetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING) != 0);

		// If the user signed in status has changed then we are using different settings files. Force a reload!.
		bool isUserSignedIn = IsUserSignedIn();
		if(isUserSignedIn != m_IsUserSignedIn)
		{
			m_IsUserSignedIn = isUserSignedIn;
			forceReload = true;
		}

		u32 mouseSettings = 0u;

		// Axis (gamepad) inversion is done on an input so if the input is assigned a value of 1.0f
		// it gets inverted to -1.0f. To separate out mouse inversion from gamepad inversion, we map
		// invert the value as it is assigned to the input. The result of this is that the two can
		// conflict.
		// i.e. inversing the input/gamepad will invert the mouse, if we want to invert the gamepad
		// without the mouse we need to invert both so that the mouse's value get inverted then
		// inverted back.
		if((axisInversion && mouseInversion) || (axisInversion == false && mouseInversion == false))
		{
			mouseSettings |= MOUSE_INVERTED;
		}

		// Reload the mouse look settings if needed.
		if( forceReload ||
			(mouseSettings & MOUSE_INVERTED) != (m_MouseSettings & MOUSE_INVERTED) )
		{
			m_MouseLookSettings.m_Mappings.Resize(0);

			const bool bLoadInvertedMouseSettings = (mouseSettings & MOUSE_INVERTED) == 0;
			if(!bLoadInvertedMouseSettings)
			{
				LoadSettingsFile(PC_STANDARD_MOUSE_SETTINGS_FILE, false, m_MouseLookSettings);
			}
			else
			{
				LoadSettingsFile(PC_INVERTED_MOUSE_SETTINGS_FILE, false, m_MouseLookSettings);
			}
		}

		// TODO: Hook these up when we have pause menu options.
		if(m_MouseVehicleCarAutoCenter)
		{
			mouseSettings |= MOUSE_CAR_AUTO_CENTER;
		}
		// TODO: Hook these up when we have pause menu options.
		if(m_MouseVehicleFlyAutoCenter)
		{
			mouseSettings |= MOUSE_FLY_AUTO_CENTER;
		}
		// TODO: Hook these up when we have pause menu options.
		if(m_MouseVehicleSubAutoCenter)
		{
			mouseSettings |= MOUSE_SUB_AUTO_CENTER;
		}

		bool bMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == eMSM_Vehicle;
		bool bCameraMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == eMSM_Camera;

		if(( bMouseSteering && !m_DriveCameraToggleOn) || (bCameraMouseSteering && m_DriveCameraToggleOn))
		{
			mouseSettings |= MOUSE_VEHICLE_CAR;
		}

		// Reload the mouse driving settings if needed.
		if( forceReload ||
			(mouseSettings & MOUSE_VEHICLE_CAR) != (m_MouseSettings & MOUSE_VEHICLE_CAR) ||
			(mouseSettings & MOUSE_CAR_AUTO_CENTER) != (m_MouseSettings & MOUSE_CAR_AUTO_CENTER) )
		{
			m_MouseCarSettings.m_Mappings.Resize(0);

			// Load mouse car controls if required.
			if((mouseSettings & MOUSE_VEHICLE_CAR) != 0)
			{
				if((mouseSettings & MOUSE_CAR_AUTO_CENTER) != 0)
				{
					LoadSettingsFile(PC_MOUSE_VEHICLE_CAR_SCALED_FILE, false, m_MouseCarSettings);
				}
				else
				{
					LoadSettingsFile(PC_MOUSE_VEHICLE_CAR_CENTERED_FILE, false, m_MouseCarSettings);	
				}
			}
		}

		bool bMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle;
		bool bCameraMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera;

		if((bMouseFlySteering && !m_DriveCameraToggleOn) || (bCameraMouseFlySteering && m_DriveCameraToggleOn))
		{
			if(((axisInversion && mouseFlyingInversion) || (axisInversion == false && mouseFlyingInversion == false)) == 0)// Yes this is confusing but look at comment about mouse inversion above
			{
				mouseSettings |= MOUSE_VEHICLE_FLY_INVERTED;
			}
			else
			{
				mouseSettings |= MOUSE_VEHICLE_FLY;
			}

			if(mouseFlyingXYSwap)
			{
				mouseSettings |= MOUSE_FLY_SWAP_XY;
			}
		}

		// Reload the mouse flying settings if needed.
		if( forceReload ||
			(mouseSettings & MOUSE_VEHICLE_FLY) != (m_MouseSettings & MOUSE_VEHICLE_FLY) ||
			(mouseSettings & MOUSE_VEHICLE_FLY_INVERTED) != (m_MouseSettings & MOUSE_VEHICLE_FLY_INVERTED) ||
			(mouseSettings & MOUSE_FLY_AUTO_CENTER) != (m_MouseSettings & MOUSE_FLY_AUTO_CENTER) ||
			(mouseSettings & MOUSE_FLY_SWAP_XY) != (m_MouseSettings & MOUSE_FLY_SWAP_XY) )
		{
			m_MouseFlySettings.m_Mappings.Resize(0);

			// Load mouse fly controls if required.
			if((mouseSettings & MOUSE_VEHICLE_FLY) != 0 || (mouseSettings & MOUSE_VEHICLE_FLY_INVERTED) != 0)
			{

				//m_MouseFlySettings
				if(mouseFlyingInversion)
				{
					if((mouseSettings & MOUSE_FLY_AUTO_CENTER) != 0)
					{
						if(mouseSettings & MOUSE_FLY_SWAP_XY)
						{
							LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_FLY_SCALED_SWAPPED_FILE, false, m_MouseFlySettings);
						}
						else
						{
							LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_FLY_SCALED_FILE, false, m_MouseFlySettings);
						}
					}
					else
					{
						if(mouseSettings & MOUSE_FLY_SWAP_XY)
						{
							LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_FLY_CENTERED_SWAPPED_FILE, false, m_MouseFlySettings);	
						}
						else
						{
							LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_FLY_CENTERED_FILE, false, m_MouseFlySettings);	
						}
					}
				}
				else
				{
					if((mouseSettings & MOUSE_FLY_AUTO_CENTER) != 0)
					{
						if(mouseSettings & MOUSE_FLY_SWAP_XY)
						{
							LoadSettingsFile(PC_MOUSE_VEHICLE_FLY_SCALED_SWAPPED_FILE, false, m_MouseFlySettings);
						}
						else
						{
							LoadSettingsFile(PC_MOUSE_VEHICLE_FLY_SCALED_FILE, false, m_MouseFlySettings);
						}
					}
					else
					{
						if(mouseSettings & MOUSE_FLY_SWAP_XY)
						{
							LoadSettingsFile(PC_MOUSE_VEHICLE_FLY_CENTERED_SWAPPED_FILE, false, m_MouseFlySettings);	
						}
						else
						{
							LoadSettingsFile(PC_MOUSE_VEHICLE_FLY_CENTERED_FILE, false, m_MouseFlySettings);	
						}
					}
				}
			}
		}

		bool bMouseSubSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_SUB) == CControl::eMSM_Vehicle;
		bool bCameraMouseSubSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_SUB) == CControl::eMSM_Camera;

		if((bMouseSubSteering && !m_DriveCameraToggleOn) || (bCameraMouseSubSteering && m_DriveCameraToggleOn))
		{
			if(((axisInversion && mouseSubInversion) || (axisInversion == false && mouseSubInversion == false)) == 0)// Yes this is confusing but look at comment about mouse inversion above
			{
				mouseSettings |= MOUSE_VEHICLE_SUB_INVERTED;
			}
			else
			{
				mouseSettings |= MOUSE_VEHICLE_SUB;
			}
		}

		// Reload the mouse sub settings if needed.
		if( forceReload ||
			(mouseSettings & MOUSE_VEHICLE_SUB) != (m_MouseSettings & MOUSE_VEHICLE_SUB) ||
			(mouseSettings & MOUSE_VEHICLE_SUB_INVERTED) != (m_MouseSettings & MOUSE_VEHICLE_SUB_INVERTED) )
		{
			m_MouseSubSettings.m_Mappings.Resize(0);

			// Load mouse sub controls if required.
			if((mouseSettings & MOUSE_VEHICLE_SUB) != 0 || (mouseSettings & MOUSE_VEHICLE_SUB_INVERTED) != 0)
			{
				if(mouseSubInversion)
				{
					if((mouseSettings & MOUSE_SUB_AUTO_CENTER) != 0)
					{
						LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_SUB_SCALED_FILE, false, m_MouseSubSettings);
					}
					else
					{
						LoadSettingsFile(PC_INVERTED_MOUSE_VEHICLE_SUB_CENTERED_FILE, false, m_MouseSubSettings);	
					}
				}
				else
				{
					if((mouseSettings & MOUSE_SUB_AUTO_CENTER) != 0)
					{
						LoadSettingsFile(PC_MOUSE_VEHICLE_SUB_SCALED_FILE, false, m_MouseSubSettings);
					}
					else
					{
						LoadSettingsFile(PC_MOUSE_VEHICLE_SUB_CENTERED_FILE, false, m_MouseSubSettings);	
					}
				}
			}
		}

		// Reload the PC controls if required.
		if(forceReload || !m_HasLoadedDefaultControls)
		{
			m_RecachedConflictCount = true;

			// Clear previous settings.
			m_UserKeyboardMouseSettings.m_Mappings.Resize(0);

		#if RSG_PC && ALLOW_USER_CONTROLS
			result = false;

			char path[PATH_BUFFER_SIZE] = {0};
			GetUserSettingsPath(path, PC_AUTOSAVE_USER_SETTINGS_FILE);
			if(loadAutoSavedSettings && ASSET.Exists(path, ""))
			{
				// User file that they can edit so we read safe but slow.
				result = LoadSettingsFile(path, true, m_UserKeyboardMouseSettings);
			}

			GetUserSettingsPath(path, PC_USER_SETTINGS_FILE);
			if(!result && ASSET.Exists(path, ""))
			{
				// User file that they can edit so we read safe but slow.
				LoadSettingsFile(path, true, m_UserKeyboardMouseSettings);
			}
		#endif // RSG_PC && ALLOW_USER_CONTROLS
		}

		// Load the default and fixed controls on first load only, assume these never change!
		if(!m_HasLoadedDefaultControls)
		{
			m_HasLoadedDefaultControls = true;

			m_DefaultKeyboardMouseSettings.m_Mappings.Resize(0);
			m_KeyboardMouseFixedSettings.m_Mappings.Resize(0);

			// load default controls instead of user controls.
			LoadSettingsFile(PC_DEFAULT_SETTINGS_FILE, false, m_DefaultKeyboardMouseSettings);

			// Load default FIXED settings.
			LoadSettingsFile(PC_FIXED_SETTINGS_FILE, false, m_KeyboardMouseFixedSettings);
		}

		// Load mappings from settings.
		LoadSettings(m_DefaultKeyboardMouseSettings, ioSource::IOMD_KEYBOARD_MOUSE);
		ReplaceSettings(m_UserKeyboardMouseSettings, ioSource::IOMD_KEYBOARD_MOUSE);

		AddKeyboardMouseExtraSettings(m_KeyboardMouseFixedSettings);
		AddKeyboardMouseExtraSettings(m_MouseLookSettings);
		AddKeyboardMouseExtraSettings(m_MouseCarSettings);
		AddKeyboardMouseExtraSettings(m_MouseFlySettings);
		AddKeyboardMouseExtraSettings(m_MouseSubSettings);

		m_MouseSettings = mouseSettings;

		// We need to reset the current scheme here as the controls have been trashed. If we don't, they will fail to
		// load as it believes they are already loaded.
		m_CurrentLoadedDynamicScheme = atHashString("", 0);
		LoadDynamicControlMappings();

		// TODO: Enable this once there all conflicts have been fixed otherwise we will get an assert on startup.
	//	BANK_ONLY(GetNumberOfMappingConflicts());
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	// Map all unmapped inputs to a blank mapping so that the inputs get updated (this is to work around a limitation in ioMapper). We use
	// A keyboard mapping of KEY_NULL for this as it means nothing.
	for (u32 i = 0; i < MAX_INPUTS; ++i)
	{
		if(m_isInputMapped[i] == false)
		{
			m_Mappers[ms_settings.m_InputMappers[i]].Map(IOMS_KEYBOARD, KEY_NULL, m_inputs[i]);
			m_isInputMapped[i] = true;
		}
	}

	SetDefaultLookInversions();
}

void CControl::ClearInputMappings()
{
	sysCriticalSection lock(m_Cs);
	m_WasControlsRefreshedThisFrame = true;

	// 1st set the hard coded defaults: (this must be done so we define what maps we use in Rage)...
	for(u32 i = 0; i < MAPPERS_MAX; ++i)
	{
		m_Mappers[i].Reset();
	}

	for(u32 i = 0; i < MAX_INPUTS; ++i)
	{
		m_isInputMapped[i] = false;
	}

	for(u32 i = 0; i < ms_settings.m_HistorySupport.size(); ++i)
	{
		m_inputs[ms_settings.m_HistorySupport[i]].SupportHistory();
	}
}

rage::ioSource CControl::GetInputSource( InputType input, s32 KEYBOARD_MOUSE_ONLY(device), u8 KEYBOARD_MOUSE_ONLY(mappingIndex), bool KEYBOARD_MOUSE_ONLY(allowFallback) ) const
{
	sysCriticalSection lock(m_Cs);

	ioMapper::ioSourceList sources;
	GetInputSources(input, sources);
	// TODO: Get the source that has the same device as the last input source.
	if(sources.size() > 0)
	{
#if KEYBOARD_MOUSE_SUPPORT
		// We could check that the mapper for this input is valid but GetInputSources() does this and if there is a source the mapper must have been
		// valid to extract them.
		const ioMapper& mapper = m_Mappers[ms_settings.m_InputMappers[input]];

		// We will default to PC mappings if we cannot find the source required.
		ioSource alternativeSource = ioSource::UNKNOWN_SOURCE;

		s32 lastDeviceIndex = m_LastKnownSource.m_DeviceIndex;
		if(device != ioSource::IOMD_DEFAULT && device != ioSource::IOMD_ANY)
		{
			lastDeviceIndex = device;
		}

		// default to keyboard/mouse.
		if(!ioSource::IsValidDevice(lastDeviceIndex))
		{
			lastDeviceIndex = ioSource::IOMD_KEYBOARD_MOUSE;
		}
		else if(lastDeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE STEAM_CONTROLLER_ONLY(&& lastDeviceIndex != ioSource::IOMD_GAME))
		{
			lastDeviceIndex = ioSource::IOMD_DEFAULT;
		}

		u8 currentIndex = 0;

		// Find the appropriate source.
		for(s32 i = 0; i < sources.size(); ++i) 
		{
			if(lastDeviceIndex == sources[i].m_DeviceIndex || (sources[i].m_DeviceIndex == ioSource::IOMD_DEFAULT && mapper.GetPlayer() == lastDeviceIndex))
			{
				// currentIndex can be greater than mappingIndex if the mapping was invalid and allowFallback was true and there was no previous valid mapping.
				if(currentIndex >= mappingIndex)
				{
					// Never return KEY_NULL mapping.
					if(sources[i].m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE || sources[i].m_Parameter != KEY_NULL)
					{
						return sources[i];
					}
					// If we should stop searching.
					else if(allowFallback == false || ioSource::IsValidDevice(alternativeSource.m_DeviceIndex))
					{
						break;
					}
				}

				++currentIndex;
			}
			
			// NOTE: This has been broken down as it was getting hard to read and needed updated for steam controller support.
			// If the source's device is the device we want or it's the keyboard/mouse (and not KEY_NULL which means invalid), then we might be able to use it.
			if((sources[i].m_DeviceIndex == lastDeviceIndex || sources[i].m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE) && sources[i].m_Parameter != KEY_NULL)
			{
				// If the alternate device is not valid then fall back to the new source.
				if(!ioSource::IsValidDevice(alternativeSource.m_DeviceIndex))
				{
					alternativeSource = sources[i];
				}
				// If the alternate device is valid but not the device we would prefer and the source is the device we would prefer then fall back to that instead.
				else if(alternativeSource.m_DeviceIndex != lastDeviceIndex && sources[i].m_DeviceIndex == lastDeviceIndex)
				{
					alternativeSource = sources[i];
				}
			}
		}

		ioSource result = ioSource::UNKNOWN_SOURCE;
		if(allowFallback && ioSource::IsValidDevice(alternativeSource.m_DeviceIndex))
		{
			result = alternativeSource;
		}
		else if(lastDeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
		{
			result = ioSource(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
		}

		return result;
#else
		// Never return KEY_NULL mapping.
		if(sources[0].m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE || sources[0].m_Parameter != KEY_NULL)
		{
			return sources[0];
		}
#endif // KEYBOARD_MOUSE_SUPPORT
	}

	return ioSource::UNKNOWN_SOURCE;
}

void CControl::GetInputSources( InputType input, ioMapper::ioSourceList& sources ) const
{
	if(controlVerifyf(input < MAX_INPUTS && input != UNDEFINED_INPUT, "Invalid input with name '%s' and value %d used to retrieve input sources!", GetInputName(input), input))
	{
		STEAM_CONTROLLER_ONLY(GetSteamControllerInputSources(input, sources));
		Mapper mapperId = ms_settings.m_InputMappers[input];

		controlAssertf(mapperId != DEPRECATED_MAPPER, "%s is Deprecated so there can be no sources mapped to it!", GetInputName(input));
		if(controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "%s has an invalid mapper assigned to it!", GetInputName(input)))
		{
			const ioMapper& mapper = m_Mappers[mapperId];

			mapper.GetMappedSources(m_inputs[input], sources);
		}
	}
}

void CControl::GetSpecificInputSources( InputType input, ioMapper::ioSourceList& sources, s32 deviceId ) const
{
	GetInputSources(input, sources);
	
	s32 current = 0;

	// remove all other sources from a different device.
	for(s32 i = 0; i < sources.GetCount(); ++i)
	{
		// If we are not removing the element then copy it to its new location.
		if(sources[i].m_DeviceIndex == deviceId)
		{
			if(current != i)
			{
				sources[current] = sources[i];
			}
			++current;
		}
	}

	sources.SetCount(current);
}

//
// name:		Update
// description:	Get pad class for this control
void CControl::Update()
{
#if RSG_ORBIS
	// If the user has changed from remote play to local play (or vise vera) then reload the controls for this CControl only.
	if(GetPad().IsRemotePlayPad() != m_IsRemotePlayer)
	{
		SetInitialDefaultMappings(false);
	}
#endif // RSG_ORBIS

	sysCriticalSection lock(m_Cs);

#if KEYBOARD_MOUSE_SUPPORT
	// Reset the controls once the user first logs in.
	if(BANK_ONLY(!PARAM_noKeyboardMouseControls.Get() &&) !m_IsUserSignedIn && IsUserSignedIn())
	{
		SetInitialDefaultMappings(true);
	}
#endif // KEYBOARD_MOUSE_SUPPORT

#if FPS_MODE_SUPPORTED
	if(IsControlUpdateThread())
	{
		bool isUsingFpsMode = IsUsingFpsMode();
		if(m_WasUsingFpsMode != isUsingFpsMode)
		{
			CControlMgr::ReInitControls();
		}
		m_WasUsingFpsMode = isUsingFpsMode;
	}
#endif // FPS_MODE_SUPPORTED

	if(m_WasControlsRefreshedLastFrame)
	{
		m_HasScriptCheckControlsRefresh = false;
		m_WasControlsRefreshedLastFrame = false;
	}

	if(m_WasControlsRefreshedThisFrame && m_HasScriptCheckControlsRefresh)
	{
		m_WasControlsRefreshedThisFrame = false;
		m_WasControlsRefreshedLastFrame = true;
	}

	KEYBOARD_MOUSE_ONLY(UpdateKeyBindings());
	WIN32PC_ONLY(UpdateJoystickBindings());

	LIGHT_EFFECTS_ONLY(UpdateLightEffects());
	PF_PUSH_TIMEBAR("controls.Update.replaceScriptedControls");
	
	// If script changes the controls then we reset them here after two frames (as normal). If this happens in the loading screen the controls are updated by
	// the render thread. As the two are out of sync, this can cause disk thrashing. For this reason we only reset on the controls update thread! 
	if(IsControlUpdateThread())
	{
		if(m_useAlternateScriptedControlsNextFrame)
		{
			m_useAlternateScriptedControlsNextFrame = false;
		}
		else if(m_scriptedInputLayout != STANDARD_TPS_LAYOUT)
		{
			UseDefaultScriptedControlsLayout();
		}
	}
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("controls.Update.inputMappers");
	UpdateInputMappers();
	PF_POP_TIMEBAR();

	STEAM_CONTROLLER_ONLY(UpdateSteamController());

	PF_PUSH_TIMEBAR("controls.Update.timedDisableInputs");
	if(m_InputDisableEndTime > fwTimer::GetSystemTimeInMilliseconds())
	{
		DisableAllInputs(m_DisableOptions);
		m_bInputsStillDisabled = true;
	}
	else
	{
		// Inputs only get disabled one frame after we flag them to be disabled, so the inputs will still be disabled here. 
		if (m_bInputsStillDisabled && m_InputDisableEndTime == 0)
		{
			m_bInputsStillDisabled = false;
		}
		m_InputDisableEndTime = 0;
		m_DisableOptions = ioValue::DEFAULT_DISABLE_OPTIONS;
	}

	if(m_BackButtonEnableTime <= fwTimer::GetSystemTimeInMilliseconds())
	{
		m_BackButtonEnableTime = 0;
	}
	else
	{
		DisableBackButtonInputs();
	}
	PF_POP_TIMEBAR();

#if RSG_BANK || RSG_PC
	ioValue::ReadOptions options = ioValue::DEFAULT_UNBOUND_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
#endif // RSG_BANK || RSG_PC

#if __BANK
	PF_PUSH_TIMEBAR("controls.Update.bankDisableInputs");
	// Disable inputs whilst they are disabled in rag.
	for(u32 i = 0; i < MAX_INPUTS; ++i)
	{
		if(m_ragData[i].m_ForceDisabled)
		{
			m_inputs[i].Disable();
		}
		m_ragData[i].m_Enabled = m_inputs[i].IsEnabled();
		m_ragData[i].m_InputValue = m_inputs[i].GetLastUnboundNorm(options);
	}
	PF_POP_TIMEBAR();
#endif // __BANK

	m_pedCollectedPickupConsumed = false;		// In the next frame the button hasn't been used yet.

	// If script changes the controls then we reset them here after two frames (as normal). If this happens in the loading screen the controls are updated by
	// the render thread. As the two are out of sync, this can cause disk thrashing. For this reason we only reset on the controls update thread! 
	if(IsControlUpdateThread())
	{
		PF_PUSH_TIMEBAR("controls.Update.RunAimToggle");

		if(!CPlayerSpecialAbilityManager::HasSpecialAbilityDisabledSprint() && (m_inputs[INPUT_SPRINT].GetLastSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || CanUseToggleRun()))
		{
			if(CPlayerSpecialAbilityManager::CanActivateSprintToggle())
			{
				m_ToggleRunOn = !m_ToggleRunOn;
				CPlayerSpecialAbilityManager::ResetActivateSprintToggle();
			}
			else
			{
				if(m_inputs[INPUT_SPRINT].IsReleased() && !m_inputs[INPUT_SPRINT].IsReleasedAfterHistoryHeldDown(KEYBOARD_START_SPRINT_HELD_TIME) )
				{
					m_ToggleRunOn = !m_ToggleRunOn;
				}
			}
		}

		PF_POP_TIMEBAR();
	}

	//Now work out if control has been touched
	//and log the current time.
#if KEYBOARD_MOUSE_SUPPORT

	PF_PUSH_TIMEBAR("controls.Update.pcToggles");
	if( m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].GetLastSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE &&
		m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].IsReleased() && 
		!m_inputs[INPUT_VEH_PUSHBIKE_SPRINT].IsReleasedAfterHistoryHeldDown(KEYBOARD_START_SPRINT_HELD_TIME) )
	{
		m_ToggleBikeSprintOn = !m_ToggleBikeSprintOn;
	}

	if(m_ToggleAim && CanUseToggleAim())
	{
		if(!m_ResetToggleAim && !m_ToggleAimOn && m_inputs[INPUT_AIM].IsReleased() && m_inputs[INPUT_AIM].GetLastSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
		{
			m_ToggleAimOn = true;
		}
		// Use pressed to turn toggle aim off as AI states can turn this off if we don't. We would then turn it back on.
		else if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsPressed())
		{
			// Do not re-enable on the Release of this press.
			m_ResetToggleAim = true;

			// reset if using a controller.
			m_ToggleAimOn = false;
		}
	}
	else if(m_ToggleAimOn)
	{
		m_ToggleAimOn = false;
	}

	if(m_ResetToggleAim && m_inputs[INPUT_AIM].IsUp())
	{
		m_ResetToggleAim = false;
	}

	UpdateLastFrameDeviceIndex();

	PF_POP_TIMEBAR();

	enum PcInputType
	{
		OTHER,
		KEYBOARD_MOUSE,
		STEAM_CONTROLLER,
	};
#if USE_STEAM_CONTROLLER
	const static PcInputType priorityDevice = STEAM_CONTROLLER;
#else
	const static PcInputType priorityDevice = KEYBOARD_MOUSE;
#endif // USE_STEAM_CONTROLLER
	PcInputType foundType = OTHER;
#endif // KEYBOARD_MOUSE_SUPPORT

	PF_PUSH_TIMEBAR("controls.Update.legacyInputOccurredEvent");

	// Update the deadzone value to 75% when detecting a switch from keyboard/mouse to gamepad (url:bugstar:2214956).
	KEYBOARD_MOUSE_ONLY(options.m_DeadZone = 0.75f);

	for(u32 i=0;i<MAX_INPUTS;i++)
	{
#if KEYBOARD_MOUSE_SUPPORT
		// If the input is fired and the source is from a valid device. We check that the input is fired first as some inputs will
		// the source as a gamepad if one is attached due to stick noise. GetNorm() checks this but applies deadzoning when appropriate.
		if( foundType < priorityDevice && ioSource::IsValidDevice(m_inputs[i].GetSource().m_DeviceIndex) &&
			m_inputs[i].GetUnboundNorm(options) &&
			m_inputs[i].GetSource().m_Device != IOMS_MOUSE_CENTEREDAXIS &&
			m_inputs[i].GetSource().m_Device != IOMS_MOUSE_ABSOLUTEAXIS &&
			m_inputs[i].GetSource().m_Device != IOMS_MOUSE_NORMALIZED &&
			m_inputs[i].GetSource().m_Parameter != KEY_GRAVE )
		{
			const ioSource& source = m_inputs[i].GetSource();
#if USE_STEAM_CONTROLLER
			if(foundType < STEAM_CONTROLLER && source.m_DeviceIndex == ioSource::IOMD_GAME && source.m_Device == IOMS_GAME_CONTROLLED)
			{
				m_LastKnownSource = source;
				foundType = STEAM_CONTROLLER;
			}
			else
#endif // USE_STEAM_CONTROLLER
			// If this source is a keyboard/mouse then we stop looking as the keyboard/mouse overrides any other input source.
			if(foundType < KEYBOARD_MOUSE && source.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
			{
				m_LastKnownSource = source;
				foundType = KEYBOARD_MOUSE;
			}
			else if(foundType == OTHER)
			{
				m_LastKnownSource = m_inputs[i].GetSource();
			}
		}
#endif // KEYBOARD_MOUSE_SUPPORT
		switch(i)
		{
			case INPUT_LOOK_LR:
			case INPUT_LOOK_UD:
			case INPUT_VEH_MOVE_UD:
			case INPUT_VEH_MOVE_LR:
			case INPUT_VEH_GUN_LR: 
			case INPUT_VEH_GUN_UD:
			case INPUT_SNIPER_ZOOM:
			{
				if(m_inputs[i].GetNorm() != 0.0f)
				{
					SetInputOccurred();
				}
				break;
			}
			default:
			{
				if( m_inputs[i].IsDown()
					KEYBOARD_MOUSE_ONLY(&& m_inputs[i].GetSource().m_Device != IOMS_MOUSE_CENTEREDAXIS)
					KEYBOARD_MOUSE_ONLY(&& m_inputs[i].GetSource().m_Device != IOMS_MOUSE_ABSOLUTEAXIS)
					KEYBOARD_MOUSE_ONLY(&& m_inputs[i].GetSource().m_Device != IOMS_MOUSE_NORMALIZED) )
				{
					SetInputOccurred();
				}
				break;
			}
		}
	}
	PF_POP_TIMEBAR();

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastFrameDeviceIndex != m_LastKnownSource.m_DeviceIndex)
	{
		StopPlayerPadShaking();
	}
#endif // KEYBOARD_MOUSE_SUPPORT
}

#if LIGHT_EFFECTS_SUPPORT
void CControl::SetLightEffect( const ioLightEffect* effect, LightEffectType type )
{
	sysCriticalSection lock(m_Cs);
	if(effect != NULL)
	{
		m_LightEffects[type].m_Effect	 = effect;
		m_LightEffects[type].m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
	}
}

void CControl::ClearLightEffect( const ioLightEffect* effect, LightEffectType type )
{
	sysCriticalSection lock(m_Cs);
	if(effect == m_LightEffects[type].m_Effect)
	{
		m_LightEffects[type].m_Effect	 = NULL;
		m_LightEffects[type].m_StartTime = 0u;
	}
}

void CControl::UpdateLightEffects()
{
	if( m_LightEffectsEnabled )
	{
		const u32 timeNow = fwTimer::GetSystemTimeInMilliseconds();

		for(s32 effectIndex = m_LightEffects.size() -1; effectIndex >= 0; --effectIndex)
		{
			if(m_LightEffects[effectIndex].m_Effect != NULL)
			{
				float progress = static_cast<float>(timeNow - m_LightEffects[effectIndex].m_StartTime) /
					static_cast<float>(m_LightEffects[effectIndex].m_Effect->GetDuration());

				if(progress > 1.0f)
				{
					// Cycle effect.
					m_LightEffects[effectIndex].m_StartTime = timeNow;
				}

				progress = Clamp(progress, 0.0f, 1.0f);

				for(s32 deviceIndex = 0; deviceIndex < m_ActiveLightDevices.size(); ++deviceIndex)
				{
					m_LightEffects[effectIndex].m_Effect->Update(progress, m_ActiveLightDevices[deviceIndex]);
				}
				return;
			}
		}
	}
}

void CControl::ResetLightDeviceColor()
{
	sysCriticalSection lock(m_Cs);
	for(u32 i = 0; i < MAX_LIGHT_EFFECT_TYPES; ++i)
	{
		m_LightEffects[i].m_Effect	  = NULL;
		m_LightEffects[i].m_StartTime = 0u;
	}

	SetLightEffectsToDefault();
}

void CControl::SetLightEffectEnabled(bool enabled)
{
	sysCriticalSection lock(m_Cs);
	m_LightEffectsEnabled = enabled;

	if(enabled == false)
	{
		SetLightEffectsToDefault();
	}
}

void CControl::SetLightEffectsToDefault()
{
	for(u32 i = 0; i < m_ActiveLightDevices.size(); ++i)
	{
		m_ActiveLightDevices[i]->SetAllLightsToDefaultColor();
	}
}

#if RSG_BANK
void CControl::UpdateRagLightEffect()
{
	if(m_RagLightEffectEnabled)
	{
		m_RagLightEffect = ioConstantLightEffect(m_RagLightEffectColor);
		m_LightEffects[RAG_LIGHT_EFFECT].m_Effect    = &m_RagLightEffect;
		m_LightEffects[RAG_LIGHT_EFFECT].m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
	}
	else
	{
		m_LightEffects[RAG_LIGHT_EFFECT].m_Effect    = NULL;
		m_LightEffects[RAG_LIGHT_EFFECT].m_StartTime = 0u;
	}
}

CControl::DebugLightDevice::DebugLightDevice()
	: m_Color(0, 0, 0)
{
}

bool CControl::DebugLightDevice::IsValid() const
{
	return true;
}

bool CControl::DebugLightDevice::HasLightSource(Source source) const
{
	return source == ioLightDevice::MIDDLE;
}

void CControl::DebugLightDevice::SetLightColor(Source source, u8 red, u8 green, u8 blue)
{
	if(source == ioLightDevice::MIDDLE)
	{
		m_Color = Color32(red, green, blue);
	}
}

void CControl::DebugLightDevice::SetAllLightsToDefaultColor()
{
	m_Color = Color32(0, 0, 0);
}

void CControl::DebugLightDevice::Update()
{
}

#endif // RSG_BANK
#endif // LIGHT_EFFECTS_SUPPORT

bool CControl::GetOrientation(Quaternion& ORBIS_ONLY(result))
{
#if RSG_ORBIS
	CPad &pad = GetPad();
	return pad.GetOrientation(result);
#else
	return false;
#endif // RSG_ORBIS
}

void CControl::ResetOrientation()
{
#if RSG_ORBIS
	CPad &pad = GetPad();
	pad.ResetOrientation();
#endif // RSG_ORBIS
}

void CControl::SetInputOccurred()
{
	ms_LastTimeTouched=fwTimer::GetTimeInMilliseconds();
}

void CControl::DisableAllInputs(u32 duration, const ioValue::DisableOptions& options)
{
	controlDebugf2("All inputs disabled for %dms.", duration);

	u32 time = duration + fwTimer::GetSystemTimeInMilliseconds();

	if(time > m_InputDisableEndTime)
	{
		if(m_InputDisableEndTime == 0)
		{
			m_DisableOptions = options;
		}
		else if(m_DisableOptions.m_DisableFlags != options.m_DisableFlags)
		{
			controlWarningf("Two timed input disables have conflicting flag. Flags will be set if they are set in both!");
			m_DisableOptions.Merge(options);
		}
		m_InputDisableEndTime = time;
	}

	DisableAllInputs(m_DisableOptions);
}

void CControl::DisableAllInputs(const ioValue::DisableOptions& options)
{
	controlDebugf2("All inputs disabled.");

	for(int i = 0; i < MAX_INPUTS; ++i)
	{
		m_inputs[i].Disable(options);
	}
}

void CControl::SetPedWalkInverted(bool invert)
{
	m_inputs[INPUT_MOVE_LR].SetInverted(invert);
	m_inputs[INPUT_MOVE_UD].SetInverted(invert);
}

void CControl::SetPedLookInverted(bool invert)
{
	m_inputs[INPUT_LOOK_LR].SetInverted(invert);
	m_inputs[INPUT_LOOK_UD].SetInverted(invert);
}

void CControl::SetInputExclusive(int action, const ioValue::DisableOptions& options)
{
	controlDebugf2("Input %s (%d) set exlusive.", GetInputName(static_cast<InputType>(action)), action);
	DebugOutputScriptCallStack();

	ioMapper::ioSourceList sources;
	GetInputSources(static_cast<InputType>(action), sources);
	
	DisableInputsByActiveSources(sources, options);

	InputType firstDependent;
	InputType secondDependent;

	// Also disable related inputs.
	for(int i = 0; i < ms_settings.m_AxisDefinitions.size(); ++i)
	{
		firstDependent = UNDEFINED_INPUT;
		secondDependent = UNDEFINED_INPUT;

		const ControlInput::AxisDefinition& definition = ms_settings.m_AxisDefinitions[i];
		if(definition.m_Input == action)
		{
			firstDependent  = definition.m_Positive;
			secondDependent = definition.m_Negative;
		}
		else if(definition.m_Positive == action)
		{
			firstDependent  = definition.m_Input;
			secondDependent = definition.m_Negative;
		}
		else if(definition.m_Negative == action)
		{
			firstDependent  = definition.m_Input;
			secondDependent = definition.m_Positive;
		}

		if(firstDependent != UNDEFINED_INPUT)
		{
			ioMapper::ioSourceList dependents;
			GetInputSources(firstDependent, dependents);
			DisableInputsByActiveSources(dependents, options);
		}
		if(secondDependent != UNDEFINED_INPUT)
		{
			ioMapper::ioSourceList dependents;
			GetInputSources(secondDependent, dependents);
			DisableInputsByActiveSources(dependents, options);
		}
	}

	// we use this function rather than setting the input to enabled directly as it has some rag override checks.
	EnableInput(action);
}

void CControl::DisableInputsByActiveSources(const ioMapper::ioSourceList& sources, const ioValue::DisableOptions& options)
{
	ioMapper::ioSourceList linkedSources;

	for(u32 sourceIndex = 0; sourceIndex < sources.GetCount(); ++sourceIndex)
	{
#if KEYBOARD_MOUSE_SUPPORT
		// If we are using the keyboard/mouse and this source is a keyboard/mouse or we are not using the keyboard and
		// mouse and the source is not a keyboard/mouse.
		if( (WasKeyboardMouseLastKnownSource() && sources[sourceIndex].m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE) ||
			(WasKeyboardMouseLastKnownSource() == false && sources[sourceIndex].m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE) )
#endif // KEYBOARD_MOUSE_SUPPORT
		{
			DisableInputsBySource(sources[sourceIndex], options);

			ioMapper::GetRelatedSources(sources[sourceIndex], linkedSources);
			for(u32 linkedIndex = 0; linkedIndex < linkedSources.size(); ++linkedIndex)
			{
				DisableInputsBySource(linkedSources[linkedIndex], options);
			}
			linkedSources.clear();
		}
	}
}

void CControl::DisableInputsBySource(const ioSource& source, const ioValue::DisableOptions& options)
{
	ioMapper::ioValueList values;

	for(u32 mapperIndex = 0; mapperIndex < MAPPERS_MAX; ++mapperIndex)
	{
		// Ignore invalid key.
		if(source.m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE || source.m_Parameter != KEY_NULL)
		{
			m_Mappers[mapperIndex].GetMappedValues(source, values);

			for(u32 valueIndex = 0; valueIndex < values.GetCount(); ++valueIndex)
			{
#if __DEV
				// The id is only stored in the value in dev builds
				controlDebugf2("%s (%d) disabled.", GetInputName(static_cast<InputType>(values[valueIndex]->GetId())), values[valueIndex]->GetId());
#endif // __DEV
				DoDisableInput(static_cast<unsigned>(values[valueIndex] - m_inputs), options, false);
			}
			values.clear();
		}
	}
}

bool CControl::IsInputDisabled(const ioValue &value) const
{
	return !value.IsEnabled();
}

int CControl::InputHowLongAgo() const 
{
	return (fwTimer::GetTimeInMilliseconds() - ms_LastTimeTouched);
}

void CControl::UpdateInputMappers()
{
	// ALWAYS update the mappers as bad stuff happens, stead, disable inputs rather than stopping them updated otherwise they get stuck with their last value which
	// Can break things (particularly the detection of which device the player is using!). This fixes url:bugstar:2199263 and is the cause of url:bugstar:2193623 which
	// we fixed separately.
	for(u32 i = 0; i < MAPPERS_MAX; ++i)
	{
		m_Mappers[i].Update(fwTimer::GetSystemTimeInMilliseconds(), false, fwTimer::GetTimeStep());
	}
	
	const bool disableInputs = CPauseMenu::IsActive( PM_WaitUntilFullyFadedInOnly ) REPLAY_ONLY(|| CReplayMgr::IsEditModeActive() || CVideoEditorInterface::IsActive());
	if( disableInputs REPLAY_ONLY(&& !CReplayMgr::IsEditingCamera()) )
	{
		ioValue::DisableOptions options(ioValue::DisableOptions::F_ALLOW_SUSTAINED);

		// disable all non-frontend inputs.
		for(u32 i = 0; i < MAX_INPUTS; ++i)
		{
			if (ms_settings.m_InputMappers[i] != FRONTEND REPLAY_ONLY(&& ms_settings.m_InputMappers[i] != REPLAY))
			{
				m_inputs[i].Disable(options);
			}
		}
	}

	if(CLiveManager::IsInitialized() == false || CLiveManager::IsSystemUiShowing() == false)
	{
		UpdateInputFromSystemEvent(INPUT_FRONTEND_CANCEL, m_SystemFrontendBackOccurredTime);
		UpdateInputFromSystemEvent(INPUT_CELLPHONE_CANCEL, m_SystemPhoneBackOccurredTime);
		UpdateInputFromSystemEvent(INPUT_FRONTEND_PAUSE, m_SystemPlayPauseOccurredTime);

		// NOTE: We treat the game and frontend inputs seperately when the 'view' event occurs. This is because the
		// inputs can be disabled indipendently.
		UpdateInputFromSystemEvent(INPUT_FRONTEND_SELECT, m_SystemFrontendViewOccuredTime);
		UpdateInputFromSystemEvent(INPUT_NEXT_CAMERA, m_SystemGameViewOccuredTime);
	}
}

void CControl::UpdateInputFromSystemEvent(InputType input, u32& timer)
{
	if(timer > 0u)
	{
		u32 now = fwTimer::GetSystemTimeInMilliseconds();

		// Give up if the input is disabled and its been too long. If we don't do this the input could be disabled for ages (e.g. a cutscene)
		// and then suddenly we would simulate a button press a good while after the system told us to. We fire the input when we give up
		// as some menu's script disable the inputs and read the disabled input value (fixes url:bugstar:1905198).
		if(m_inputs[input].IsEnabled() || (timer + SYSTEM_EVENT_TIMOUT_DURATION) <= now)
		{
			m_inputs[input].SetCurrentValue(
				ioValue::MAX_AXIS,
				ioSource(IOMS_UNDEFINED, ioSource::UNDEFINED_PARAMETER, ioSource::IOMD_SYSTEM) );
			timer = 0u;
		}
	}
}


//
// name:		GetPad
// description:	Get pad class for this control
CPad& CControl::GetPad()
{
	return CControlMgr::GetPad(m_padNum);
}



//
// name:		InitEmptyControls
// description:	Init this CControl as if no input had occured.
void CControl::InitEmptyControls()
{
}

void CControl::EnableAllInputs()
{
	controlDebugf2("All inputs enabled.");
	DebugOutputScriptCallStack();

	for(int i = 0; i < MAX_INPUTS; ++i)
	{
#if __BANK
		if(!m_ragData[i].m_ForceDisabled)
		{
#endif // __BANK
			m_inputs[i].Enable();
#if __BANK
		}
#endif // __BANK
	}
}

void CControl::DisableInputsOnBackButton(u32 duration /*= 2000*/)
{
	m_BackButtonEnableTime = fwTimer::GetSystemTimeInMilliseconds() + duration;
	DisableBackButtonInputs();
}

void CControl::DisableInput(ioMapperSource source, unsigned param, const ioValue::DisableOptions& UNUSED_PARAM(options))
{
	controlDebugf2("Disabling all inputs with source %d and param %d.", source, param);
	DebugOutputScriptCallStack();
	
	ioMapper* pCurrMapper = NULL;

	// ioMapper::Map adjusts the parameter depending on the pad override
	param |= (static_cast<unsigned>(PAD_OVERRIDE) << 24);

	for (u32 mapper=0; mapper< MAPPERS_MAX; mapper++)
	{
		pCurrMapper = &m_Mappers[mapper];

		if (pCurrMapper)
		{
			for (u32 i=0; i<pCurrMapper->GetCount(); i++)
			{
				if (pCurrMapper->GetMapperSource(i) == source && pCurrMapper->GetParameter(i) == param)
				{
					ioValue* value = pCurrMapper->GetValue(i);

					if (AssertVerify(value))
					{
						for (u32 j=0; j<MAX_INPUTS; j++)
						{
							if (&m_inputs[j] == value)
							{
								//m_inputs[j].Disable(options);
							}
						}
					}
				}
			}
		}
	}
}

void CControl::DoDisableInput(unsigned input, const ioValue::DisableOptions& options, bool disableRelatedInputs)
{
	m_inputs[input].Disable(options);

	if(disableRelatedInputs)
	{
		controlDebugf2("Disabling related inputs to %s.", GetInputName(static_cast<InputType>(input)));

		// Check if the input has related inputs.
		for(u32 groupIndex = 0; groupIndex < ms_settings.m_RelatedInputs.size(); ++groupIndex)
		{
			const ControlInput::RelatedInputs& relatedInputs = ms_settings.m_RelatedInputs[groupIndex];
			if(relatedInputs.m_Inputs.Find(static_cast<InputType>(input)) != -1)
			{
				for (u32 inputIndex = 0; inputIndex < relatedInputs.m_Inputs.size(); ++inputIndex)
				{
					InputType relatedInput = relatedInputs.m_Inputs[inputIndex];

					controlDebugf2( "%s has been disabled as it is related to %s.",
						GetInputName(relatedInput),
						GetInputName(static_cast<InputType>(input)) );
					m_inputs[relatedInput].Disable(options);
				}
			}
		}
	}
}

void CControl::DoEnableInput(unsigned input, bool enableRelatedInputs)
{
#if __BANK
	if(!m_ragData[input].m_ForceDisabled)
#endif // __BANK
	{
		m_inputs[input].Enable();
	}

	if(enableRelatedInputs)
	{
		controlDebugf2("Enabling related inputs to %s.", GetInputName(static_cast<InputType>(input)));

		// Check if the input has related inputs.
		for(u32 groupIndex = 0; groupIndex < ms_settings.m_RelatedInputs.size(); ++groupIndex)
		{
			const ControlInput::RelatedInputs& relatedInputs = ms_settings.m_RelatedInputs[groupIndex];
			if(relatedInputs.m_Inputs.Find(static_cast<InputType>(input)) != -1)
			{
				for (u32 inputIndex = 0; inputIndex < relatedInputs.m_Inputs.size(); ++inputIndex)
				{
					InputType relatedInput = relatedInputs.m_Inputs[inputIndex];
#if __BANK
					if(!m_ragData[relatedInput].m_ForceDisabled)
#endif // __BANK
					{
						controlDebugf2("%s has been enabled as it is related to %s.", GetInputName(relatedInput), GetInputName(static_cast<InputType>(input)));
						m_inputs[relatedInput].Enable();
					}
				}
			}
		}
	}
}

bool CControl::IsInputDisabled(unsigned inputIndex) const
{
	if(controlVerifyf(inputIndex < MAX_INPUTS, "Invalid input index!"))
		return !m_inputs[inputIndex].IsEnabled();
	else
		return true; // input does not exist so act as if it is disabled.
}

const ioValue& CControl::GetValue( s32 index ) const
{
	switch(index)
	{
	case INPUT_MOVE_LEFT:
	case INPUT_MOVE_RIGHT:
		return m_inputs[INPUT_MOVE_LR];

	case INPUT_MOVE_UP:
	case INPUT_MOVE_DOWN:
		return m_inputs[INPUT_MOVE_UD];

	case INPUT_LOOK_LEFT:
	case INPUT_LOOK_RIGHT:
		return m_inputs[INPUT_LOOK_LR];

	case INPUT_LOOK_UP:
	case INPUT_LOOK_DOWN:
		return m_inputs[INPUT_LOOK_UD];

	case INPUT_SNIPER_ZOOM_IN:
	case INPUT_SNIPER_ZOOM_OUT:
	case INPUT_SNIPER_ZOOM_IN_ALTERNATE:
	case INPUT_SNIPER_ZOOM_OUT_ALTERNATE:
		return m_inputs[INPUT_SNIPER_ZOOM];

	case INPUT_VEH_MOVE_LEFT:
	case INPUT_VEH_MOVE_RIGHT:
		return m_inputs[INPUT_VEH_MOVE_LR];

	case INPUT_VEH_MOVE_UP:
	case INPUT_VEH_MOVE_DOWN:
		return m_inputs[INPUT_VEH_MOVE_UD];

	case INPUT_VEH_GUN_LEFT:
	case INPUT_VEH_GUN_RIGHT:
		return m_inputs[INPUT_VEH_GUN_LR];

	case INPUT_VEH_GUN_UP:
	case INPUT_VEH_GUN_DOWN:
		return m_inputs[INPUT_VEH_GUN_LR];

	// I'm certain these are not used but they are here just in case.
	case INPUT_VEH_LOOK_LEFT:
	case INPUT_VEH_LOOK_RIGHT:
		return m_inputs[INPUT_LOOK_LR];

	case INPUT_MELEE_ATTACK1:
		return m_inputs[INPUT_MELEE_ATTACK_LIGHT];

	case INPUT_MELEE_ATTACK2:
		return m_inputs[INPUT_MELEE_ATTACK_HEAVY];

	case INPUT_NEXT_WEAPON:
		return m_inputs[INPUT_WEAPON_WHEEL_NEXT];
	case INPUT_PREV_WEAPON:
		return m_inputs[INPUT_WEAPON_WHEEL_PREV];

	default:
		return m_inputs[index];
	}
}

void CControl::SetWeaponSelectExclusive()
{
	SetInputExclusive(INPUT_WEAPON_WHEEL_NEXT);
	SetInputExclusive(INPUT_WEAPON_WHEEL_PREV);
	SetInputExclusive(INPUT_WEAPON_WHEEL_UD);
	SetInputExclusive(INPUT_WEAPON_WHEEL_LR);

	// On PC NEXT and PREV use the mouse wheel which means that they will disable each other (making INPUT_WEAPON_WHEEL_NEXT) disabled.
	EnableInput(INPUT_WEAPON_WHEEL_NEXT);
}

void CControl::SetInputValueNextFrame(InputType input, float value)
{
	if(controlVerifyf(input >= 0 && input < MAX_INPUTS, "Invalid Input %d", input))
	{
		m_inputs[input].SetNextValue(value,	ioSource(IOMS_UNDEFINED, ioSource::UNDEFINED_PARAMETER, ioSource::IOMD_SYSTEM));
	}
}

void CControl::SetInputValueNextFrame(InputType input, float value, ioSource source)
{
	if(controlVerifyf(input >= 0 && input < MAX_INPUTS, "Invalid Input %d", input))
	{
		m_inputs[input].SetNextValue(value,	source);
	}
}

void CControl::ShakeController( int duration, int frequency, bool IsScriptCommand, ioRumbleEffect *pRumbleEffect)
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;

#if __WIN32PC
	// if a joystick vibrate it.
	if(GetMappingType(m_LastKnownSource.m_Device) == JOYSTICK)
	{
		ioJoystick::ShakeDevice(deviceId, frequency, 0, duration);
		return;
	}
#endif // __WIN32PC

	CPad &pad = CControlMgr::GetPad(deviceId);
#else
	CPad &pad = GetPad();
#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_ACTUATOR_EFFECTS
	if (pRumbleEffect)
	{
		pad.ApplyActuatorEffect(pRumbleEffect, false);
	}
	else
	{
		m_Rumble = ioRumbleEffect(duration, static_cast<float>(frequency) / MAX_VIBRATION_FREQUENCY);
		pad.ApplyActuatorEffect(&m_Rumble, IsScriptCommand);
	}

#else
	pad.AllowShake();

	pad.SetScriptShake(IsScriptCommand);
	pad.StartShake(duration, frequency);
	pad.SetScriptShake(false);
#endif // USE_ACTUATOR_EFFECTS
}

void CControl::StartPlayerPadShake_Distance( u32 Duration, s32 Frequency, float X, float Y, float Z )
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;

#if __WIN32PC
	// if a joystick vibrate it.
	if(GetMappingType(m_LastKnownSource.m_Device) == JOYSTICK)
	{
		ioJoystick::ShakeDevice(deviceId, static_cast<s32>(X), static_cast<s32>(Y), Duration);
		return;
	}
#endif // __WIN32PC

	CPad &pad = CControlMgr::GetPad(deviceId);
#else
	CPad &pad = GetPad();
#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_ACTUATOR_EFFECTS
	// NOTE: This code was taken from the CPad::StartShake_Distance to work create a compatible rumble effect.
	Vector3 pos(X,Y,Z);
	float	Distance2 = (camInterface::GetPos() - pos).Mag2();

	if (Distance2 < (70.0f * 70.0f) )	// For now simple on/off. Might get more sophisticated
	{
		m_Rumble = ioRumbleEffect(Duration, 0.0f, static_cast<float>(Frequency) / MAX_VIBRATION_FREQUENCY);
		pad.ApplyActuatorEffect(&m_Rumble);
	}
#else
	pad.StartShake_Distance(Duration, Frequency, X, Y, Z);
#endif // USE_ACTUATOR_EFFECTS
}


void CControl::StartShaking( s32 MotorFrequency0, s32 MotorFrequency1 )
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;

#if __WIN32PC
	// if a joystick vibrate it.
	if(GetMappingType(m_LastKnownSource.m_Device) == JOYSTICK)
	{
		ioJoystick::ShakeDevice(deviceId, MotorFrequency0, MotorFrequency1, ioJoystick::IOJ_INFINITE);
		return;
	}
#endif // __WIN32PC

	CPad &pad = CControlMgr::GetPad(deviceId);
#else
	CPad &pad = GetPad();
#endif // KEYBOARD_MOUSE_SUPPORT


#if USE_ACTUATOR_EFFECTS

	// MOTORS: MotorFrequency0 is Heavy, MotorFrequency1 is Light
	// Pad system expects (Heavy, Light) due to legacy DualShock setup
	// Actuator system expects (Light, Heavy) so make sure values are switched (or fix actuator system at some point!)

	m_Rumble = ioRumbleEffect(1, static_cast<float>(MotorFrequency1) / MAX_VIBRATION_FREQUENCY, static_cast<float>(MotorFrequency0) / MAX_VIBRATION_FREQUENCY);
	pad.ApplyActuatorEffect(&m_Rumble);
#else
	pad.StartShake(MotorFrequency0, MotorFrequency1);
#endif // USE_ACTUATOR_EFFECTS
}

void CControl::StartPlayerPadShake(u32 Duration0, s32 MotorFrequency0, u32 Duration1, s32 MotorFrequency1, s32 DelayAfterThisOne)
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;

#if __WIN32PC
	// if a joystick vibrate it.
	if(GetMappingType(m_LastKnownSource.m_Device) == JOYSTICK)
	{
		ioJoystick::ShakeDevice(deviceId, MotorFrequency0, MotorFrequency1, Duration0);
		return;
	}
#endif // __WIN32PC

	CPad &pad = CControlMgr::GetPad(deviceId);
#else
	CPad &pad = GetPad();
#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_ACTUATOR_EFFECTS
	u32 duration = Max(Duration0, Duration1);

	// MOTORS: MotorFrequency0 is Heavy, MotorFrequency1 is Light
	// Pad system expects (Heavy, Light) due to legacy DualShock setup
	// Actuator system expects (Light, Heavy) so make sure values are switched (or fix actuator system at some point!)

	m_Rumble = ioRumbleEffect(duration, static_cast<float>(MotorFrequency1) /MAX_VIBRATION_FREQUENCY, static_cast<float>(MotorFrequency0) / MAX_VIBRATION_FREQUENCY);
	pad.ApplyActuatorEffect(&m_Rumble, false, CPad::NO_SUPPRESS, DelayAfterThisOne);
#else
	pad.StartShake(Duration0, MotorFrequency0, Duration1, MotorFrequency1, DelayAfterThisOne);
#endif // USE_ACTUATOR_EFFECTS
}

#if HAS_TRIGGER_RUMBLE
void CControl::StartTriggerShake( u32 durationLeft, u32 freqLeft, u32 durationRight, u32 freqRight, s32 DelayAfterThisOne, bool isScriptedCommand )
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

	CPad &pad = GetPad();

	u32 duration = Max( durationLeft, durationRight );

	m_TriggerRumble = ioTriggerRumbleEffect( duration, static_cast<float>( freqLeft ) / MAX_VIBRATION_FREQUENCY, static_cast<float>( freqRight ) / MAX_VIBRATION_FREQUENCY );
	pad.ApplyActuatorEffect( &m_TriggerRumble, isScriptedCommand, CPad::NO_SUPPRESS, DelayAfterThisOne );
}
#endif // #if HAS_TRIGGER_RUMBLE


void CControl::ApplyRecoilEffect( u32 duration, float intensity, float triggerIntensity )
{
	if(m_ShakeSupressId != NO_SUPPRESS)
	{
		return;
	}

#if USE_ACTUATOR_EFFECTS

#if KEYBOARD_MOUSE_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;
	CPad &pad = CControlMgr::GetPad(deviceId);
#else
	CPad &pad = GetPad();
#endif // KEYBOARD_MOUSE_SUPPORT

	//Pick trigger to rumble based on input mappings
	fwRecoilEffect::Trigger triggerToRumble = fwRecoilEffect::NONE;
	if (triggerIntensity > 0)
	{
		const ioSource &controlSource = GetPedAttack().GetSource();
		if (controlSource.m_Device == IOMS_PAD_ANALOGBUTTON || controlSource.m_Device == IOMS_PAD_DIGITALBUTTON)
		{
			ioMapperParameter controlParam = ioMapper::ConvertParameterToMapperValue(controlSource.m_Device, controlSource.m_Parameter);

			if (controlParam == R2_INDEX || controlParam == R2)
				triggerToRumble = fwRecoilEffect::RIGHT;

			else if (controlParam == L2_INDEX || controlParam == L2)
				triggerToRumble = fwRecoilEffect::LEFT;
		}
	}

	m_Recoil = fwRecoilEffect(duration, triggerToRumble, intensity, triggerIntensity);
	pad.ApplyActuatorEffect(&m_Recoil);
#else
	ShakeController(duration, static_cast<u32>(intensity * MAX_VIBRATION_FREQUENCY), false);
#endif // USE_ACTUATOR_EFFECTS
}

void CControl::StopPlayerPadShaking(bool bForce)
{
#if __WIN32PC
	ioJoystick::StopAllForces();
#endif  // __WIN32PC

	GetPad().StopShaking(bForce);

#if USE_ACTUATOR_EFFECTS
	m_Recoil = fwRecoilEffect();
	m_Rumble = ioRumbleEffect();
#endif // USE_ACTUATOR_EFFECTS
}

void CControl::AllowPlayerPadShaking()
{
	GetPad().AllowShake();
}

bool CControl::IsPlayerPadShaking()
{
	return GetPad().IsShaking();
}

eControlLayout CControl::ConvertToScriptedLayout(eControlLayout layout) const
{
	switch(layout)
	{
	case STANDARD_TPS_LAYOUT:
	case STANDARD_FPS_LAYOUT:
	case STANDARD_FPS_ALTERNATE_LAYOUT:
		return STANDARD_TPS_LAYOUT;

	case TRIGGER_SWAP_TPS_LAYOUT:
	case TRIGGER_SWAP_FPS_LAYOUT:
	case TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
		return TRIGGER_SWAP_TPS_LAYOUT;

	case SOUTHPAW_TPS_LAYOUT:
	case SOUTHPAW_FPS_LAYOUT:
	case SOUTHPAW_FPS_ALTERNATE_LAYOUT:
		return SOUTHPAW_TPS_LAYOUT;

	case SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
		return SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT;

	default:
		controlAssertf(false, "Unknown third person shooter gamepad configuration %d, using default!", layout);
		return STANDARD_TPS_LAYOUT;

	}
}

const char* CControl::GetScriptedInputLayoutFile(eControlLayout layout)
{
	// Control Layouts
	switch(ConvertToScriptedLayout(layout))
	{
	case STANDARD_TPS_LAYOUT:
		return STANDARD_SCRIPT_LAYOUT_FILE;

	case TRIGGER_SWAP_TPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_SCRIPT_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return TRIGGER_SWAP_SCRIPT_LAYOUT_FILE;
		}

	case SOUTHPAW_TPS_LAYOUT:
		return SOUTHPAW_SCRIPT_LAYOUT_FILE;

	case SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_SCRIPT_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_TRIGGER_SWAP_SCRIPT_LAYOUT_FILE;
		}

	default:
		controlAssertf(false, "Unknown scripted third person gamepad configuration %d, using default!", layout);
		return STANDARD_SCRIPT_LAYOUT_FILE;
	}
}

const char* CControl::GetGamepadBaseLayoutFile(eControlLayout layout)
{
	// Control Layouts
	switch(layout)
	{
	case STANDARD_TPS_LAYOUT:
	case STANDARD_FPS_LAYOUT:
	case STANDARD_FPS_ALTERNATE_LAYOUT:
		return STANDARD_LAYOUT_FILE;

	case TRIGGER_SWAP_TPS_LAYOUT:
	case TRIGGER_SWAP_FPS_LAYOUT:
	case TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return TRIGGER_SWAP_LAYOUT_FILE;
		}

	case SOUTHPAW_TPS_LAYOUT:
	case SOUTHPAW_FPS_LAYOUT:
	case SOUTHPAW_FPS_ALTERNATE_LAYOUT:
		return SOUTHPAW_LAYOUT_FILE;

	case SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_TRIGGER_SWAP_LAYOUT_FILE;
		}

	default:
		controlAssertf(false, "Unknown gamepad third person configuration %d, using default!", layout);
		return STANDARD_LAYOUT_FILE;
	}
}

const char* CControl::GetGamepadSpecificLayoutFile(eControlLayout layout)
{
	// Control Layouts
	switch(layout)
	{
	case STANDARD_TPS_LAYOUT:
		return STANDARD_TPS_LAYOUT_FILE;

	case TRIGGER_SWAP_TPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return TRIGGER_SWAP_TPS_LAYOUT_FILE;
		}

	case SOUTHPAW_TPS_LAYOUT:
		return SOUTHPAW_TPS_LAYOUT_FILE;

	case SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT_FILE;
		}

	case STANDARD_FPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return STANDARD_FPS_LAYOUT_FILE;
		}

	case TRIGGER_SWAP_FPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return TRIGGER_SWAP_FPS_LAYOUT_FILE;
		}

	case SOUTHPAW_FPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_FPS_LAYOUT_FILE;
		}

	case SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT_FILE;
		}

	case STANDARD_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return STANDARD_FPS_ALTERNATE_LAYOUT_FILE;
		}

	case TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return STANDARD_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT_FILE;
		}

	case SOUTHPAW_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_FPS_ALTERNATE_LAYOUT_FILE;
		}

	case SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
#if RSG_ORBIS
		if(IsUsingRemotePlay())
		{
			return SOUTHPAW_TPS_LAYOUT_FILE;
		}
		else
#endif // RSG_ORIBS
		{
			return SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT_FILE;
		}

	default:
		controlAssertf(false, "Unknown third person shooter gamepad configuration %d, using default!", layout);
		return STANDARD_TPS_LAYOUT_FILE;
	}
}

const char* CControl::GetAcceptCancelLayoutFile()
{
#if __PPU || RSG_ORBIS
	if(!CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS))
	{
		return ALTERNATE_ACCEPT_CANCEL_SETTINGS_FILE;
	}
#endif // __PPU
	
	return DEFAULT_ACCEPT_CANCEL_SETTINGS_FILE;
}

const char* CControl::GetDuckHandbrakeLayoutFile()
{
	const s32 iDuckBrakeMenuPref = CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE);
	if(iDuckBrakeMenuPref > 0)
	{
		return ALTERNATE_DUCK_HANDBRAKE_SETTINGS_FILE;
	}

	return DEFAULT_DUCK_HANDBRAKE_SETTINGS_FILE;
}

#if FPS_MODE_SUPPORTED

bool CControl::IsUsingFpsMode() const
{
	if(camInterface::IsInitialized())
	{
		return camInterface::GetGameplayDirector().ShouldUse1stPersonControls();
	}

	return false;
}

eControlLayout CControl::GetFpsLayout() const
{
	// Add STANDARD_FPS_LAYOUT since this is the start of FPS layouts and UI options for PREF_CONTROL_CONFIG_FPS for FPS modes starts at 0.
	return static_cast<eControlLayout>(CPauseMenu::GetMenuPreference(PREF_CONTROL_CONFIG_FPS));
}

#endif // FPS_MODE_SUPPORTED

eControlLayout CControl::GetTpsLayout() const
{
	return static_cast<eControlLayout>(CPauseMenu::GetMenuPreference(PREF_CONTROL_CONFIG));
}

eControlLayout CControl::GetActiveLayout() const
{
#if FPS_MODE_SUPPORTED
	if(IsUsingFpsMode())
	{
		return GetFpsLayout();
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		return GetTpsLayout();
	}
}


bool CControl::LoadSettingsFile(const char* settingsFile, bool readSafeButSlow, ControlInput::ControlSettings& controlSettings)
{
	parSettings settings;

	// we want to detect errors when parsing the xml.
	settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, readSafeButSlow);

	INIT_PARSER;

	bool result = PARSER.LoadObject(settingsFile, "", controlSettings, &settings);
	controlAssertf(result, "Error loading control configuration from file: %s", settingsFile);

	SHUTDOWN_PARSER;

	return result;
}

bool CControl::LoadOverrideSettingsFile(const char* settingsFile, s32 deviceId, bool readSafeButSlow)
{

	parSettings settings;

	// we want to detect errors when parsing the xml.
	settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, readSafeButSlow);

	INIT_PARSER;

	ControlInput::ControlSettings controlSettings;
	bool result = PARSER.LoadObject(settingsFile, "", controlSettings, &settings);

	if(controlVerifyf(result, "Error loading control configuration from file: %s", settingsFile))
	{
		for(u32 i = 0; i < controlSettings.m_Mappings.size(); ++i)
		{
			const InputType input = controlSettings.m_Mappings[i].m_Input;
			ioMapper& mapper = m_Mappers[ms_settings.m_InputMappers[input]];
			mapper.RemoveDeviceMappings(m_inputs[input], ioSource::IOMD_DEFAULT);
		}

		LoadSettings(controlSettings, deviceId);
	}

	SHUTDOWN_PARSER;

	return result;
}

void CControl::LoadSettings(const ControlInput::ControlSettings &settings, s32 deviceId )
{	
	controlAssertf(ioSource::IsValidDevice(deviceId), "Invalid device id, using default!");
	for (u32 i = 0; i < settings.m_Mappings.size(); ++i)
	{
		const ControlInput::Mapping &mapping = settings.m_Mappings[i];

		ioSource source;
		source.m_Device			= mapping.m_Source;
		source.m_DeviceIndex	= deviceId;

		switch(mapping.m_Source)
		{
		case IOMS_MKB_AXIS:
			// Keyboard axis expects the first parameter to be the high bits and the second parameter to be the low bits.
			controlAssertf(mapping.m_Parameters.size() == 2, "Invalid number of parameters for a keyboard axis, only the first two will be used (if possible)!");
			if(mapping.m_Parameters.size() >= 2)
			{
				source.m_Parameter = ioMapper::MakeMkbAxis(mapping.m_Parameters[0],mapping.m_Parameters[1]);
			}
			break;
		case IOMS_DIGITALBUTTON_AXIS:
			// Keyboard axis expects the first parameter to be the high bits and the second parameter to be the low bits.
			controlAssertf(mapping.m_Parameters.size() == 2, "Invalid number of parameters for a keyboard axis, only the first two will be used (if possible)!");
			if(mapping.m_Parameters.size() >= 2)
			{
				u32 high = ioMapper::ConvertParameterToDeviceValue(mapping.m_Source, mapping.m_Parameters[0]);
				u32 low  = ioMapper::ConvertParameterToDeviceValue(mapping.m_Source, mapping.m_Parameters[1]);

				source.m_Parameter = high << 8 | low;
			}
			break;

		case IOMS_MOUSE_BUTTON:
		case IOMS_PAD_DIGITALBUTTON:
		case IOMS_PAD_DEBUGBUTTON:
			{
				u32 buttons = 0;
				for(u32 parameterIndex = 0; parameterIndex < mapping.m_Parameters.size(); ++parameterIndex)
				{
					buttons |= ioMapper::ConvertParameterToDeviceValue(mapping.m_Source, mapping.m_Parameters[parameterIndex]);
				}

				source.m_Parameter = buttons;
			}
			break;

		default:
			controlAssertf(mapping.m_Parameters.size() == 1, "Invalid number of parameters, only the first will be used (if possible)!");
			if(mapping.m_Parameters.size() >= 1)
			{
				source.m_Parameter = ioMapper::ConvertParameterToDeviceValue(mapping.m_Source, mapping.m_Parameters[0]);
			}
		}

		AddMapping(mapping.m_Input, source);
	}
}

void CControl::ReplaceSettings(const ControlInput::ControlSettings &settings, s32 deviceId)
{
	for(u32 i = 0; i < settings.m_Mappings.size(); ++i)
	{
		const ControlInput::Mapping& mapping = settings.m_Mappings[i];
		const u32 mapperIndex = ms_settings.m_InputMappers[mapping.m_Input];

		if(mapperIndex != DEPRECATED_MAPPER && controlVerifyf(mapperIndex >= 0 && mapperIndex <= MAPPERS_MAX, "Invalid mapper for '%s'!", GetInputName(mapping.m_Input)))
		{
			m_Mappers[mapperIndex].RemoveDeviceMappings(m_inputs[mapping.m_Input], deviceId);
		}
	}

	LoadSettings(settings, deviceId);
}

void CControl::LoadScriptedMappings()
{
	ms_StandardScriptedMappings.m_Mappings.clear();
	m_AlternateTpsScriptedSettings.m_Mappings.clear();

	INIT_PARSER;

	// we want to detect errors when parsing the xml.
	parSettings settings;
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);

	bool result = PARSER.LoadObject(STANDARD_SCRIPT_LAYOUT_FILE, "", ms_StandardScriptedMappings, &settings);

	controlAssertf(result, "Error loading standard scripted configuration from file: %s", STANDARD_SCRIPT_LAYOUT_FILE);

	eControlLayout tpsLayout = GetTpsLayout();

	// Only load alternate layout settings if the user is not using standard controls layout.
	if(tpsLayout != STANDARD_TPS_LAYOUT && tpsLayout != STANDARD_FPS_LAYOUT)
	{
		result = PARSER.LoadObject(GetScriptedInputLayoutFile(tpsLayout), "", m_AlternateTpsScriptedSettings, &settings);
		controlAssertf(result, "Error loading alternate scripted configuration from file: %s", GetScriptedInputLayoutFile(tpsLayout));
	}

#if FPS_MODE_SUPPORTED
	m_AlternateFpsScriptedSettings.m_Mappings.Resize(0);

	eControlLayout fpsLayout = GetFpsLayout();

	// Only load alternate layout settings if the user is not using standard controls layout.
	if(fpsLayout != STANDARD_TPS_LAYOUT && fpsLayout != STANDARD_FPS_LAYOUT)
	{
		result = PARSER.LoadObject(GetScriptedInputLayoutFile(fpsLayout), "", m_AlternateFpsScriptedSettings, &settings);
		controlAssertf(result, "Error loading alternate scripted configuration from file: %s", GetScriptedInputLayoutFile(fpsLayout));
	}
#endif // FPS_MODE_SUPPORTED

	SHUTDOWN_PARSER;
	m_useAlternateScriptedControlsNextFrame = false;
	m_scriptedInputLayout = STANDARD_TPS_LAYOUT;
}

CControl::MappingType CControl::GetMappingType( ioMapperSource source )
{
	// we ignore keyboard axis as keyboard axis (and IOMS_DIGITALBUTTON_AXIS for that matter) is mapped differently.
	switch(source)
	{
	case IOMS_KEYBOARD:
	case IOMS_MKB_AXIS:
	case IOMS_MOUSE_ABSOLUTEAXIS:
	case IOMS_MOUSE_CENTEREDAXIS:
	case IOMS_MOUSE_RELATIVEAXIS:
	case IOMS_MOUSE_SCALEDAXIS:
	case IOMS_MOUSE_WHEEL:
	case IOMS_MOUSE_BUTTON:
	case IOMS_MOUSE_BUTTONANY:
		return PC_KEYBOARD_MOUSE;

	case IOMS_PAD_AXIS:
		return PAD_AXIS;

	case IOMS_JOYSTICK_AXIS:
	case IOMS_JOYSTICK_IAXIS:
	case IOMS_JOYSTICK_AXIS_NEGATIVE:
	case IOMS_JOYSTICK_AXIS_POSITIVE:
	case IOMS_JOYSTICK_BUTTON:
	case IOMS_JOYSTICK_POV:
		return JOYSTICK;

	default:
		// unknown type.
		return UNKNOWN_MAPPING_TYPE;
	}
}

// Extremely poor implementation, this will need replacing once a translation for the inputs has been creating.
const char* CControl::GetParameterText( ioMapperParameter parameter )
{
	for(u32 i = 0; i < parser_rage__ioMapperParameter_Data.m_NumEnums; ++i)
	{
		if(parser_rage__ioMapperParameter_Data.m_Enums[i].m_Value == parameter)
			return parser_rage__ioMapperParameter_Data.m_Names[i];
	}

	controlErrorf("Unknown parameter (%d)!", parameter);
	return "";
}

const char* CControl::GetSourceText( ioMapperSource source )
{
	for(u32 i = 0; i < parser_rage__ioMapperSource_Data.m_NumEnums; ++i)
	{
		if(parser_rage__ioMapperSource_Data.m_Enums[i].m_Value == source)
			return parser_rage__ioMapperSource_Data.m_Names[i];
	}

	controlErrorf("Unknown source (%d)!", source);
	return "";
}

rage::InputType CControl::GetInputByName( const char *name )
{
	for(u32 i = 0; i < parser_rage__InputType_Data.m_NumEnums; ++i)
	{
		if(stricmp(parser_rage__InputType_Data.m_Names[i], name) == 0)
			return static_cast<InputType>(parser_rage__InputType_Data.m_Enums[i].m_Value);
	}

	controlErrorf("%s is not a valid input!", name);
	return UNDEFINED_INPUT;
}

rage::InputGroup CControl::GetInputGroupByName( const char *name )
{
	for(u32 i = 0; i < parser_rage__InputGroup_Data.m_NumEnums; ++i)
	{
		if(stricmp(parser_rage__InputGroup_Data.m_Names[i], name) == 0)
			return static_cast<InputGroup>(parser_rage__InputGroup_Data.m_Enums[i].m_Value);
	}

	controlErrorf("%s is not a valid input group!", name);
	return INPUTGROUP_INVALID;
}

const char* CControl::GetInputName( InputType input )
{
	if(controlVerifyf(input < MAX_INPUTS && input != UNDEFINED_INPUT, "Invalid input with value %d!", input ) )
	{
		return parser_rage__InputType_Data.m_Names[input];
	}

	return "";
}

const char* CControl::GetInputGroupName( InputGroup inputGroup )
{
	if(controlVerifyf(inputGroup < MAX_INPUTGROUPS && INPUTGROUP_INVALID, "Invalid input group!"))
	{
		return parser_rage__InputGroup_Data.m_Names[inputGroup];
	}

	return "";
}

void CControl::AddMapping( const InputType input, const ioSource& source )
{
	MappingType type = GetMappingType(source.m_Device);
	// use the same code as UpdateMapping but pass in the mapping number of max int.
	UpdateMapping(input, type, source, 0xFFFFFFFFu, true);
}

void CControl::UpdateMapping(const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, bool updateDependantMappings)
{
	if(input != UNDEFINED_INPUT)
	{
		s32 mapperId = ms_settings.m_InputMappers[input];

		controlAssertf(mapperId != DEPRECATED_MAPPER, "%s is deprecated so it is not possible to map a device to it!", GetInputName(input));
		if(controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "%s has an invalid mapper so it is not possible to map a device to it!", GetInputName(input)))
		{
			ioMapper& mapper = m_Mappers[mapperId];

			// find previous sources mapped to input.
			ioMapper::ioSourceList sources;
			mapper.GetMappedSources(m_inputs[input], sources);

			// keep track of when the primary mapping is found (so we can skip it when searching for the secondary mapping).
			u32 mappingCount = 0;
			bool updated = false;
			for(u32 i = 0; i < sources.size() && !updated; ++i)
			{
				// check if the device is mappable (e.g. the keyboard).
				if(GetMappingType(sources[i].m_Device) == type)
				{
					if(mappingCount == mappingNumber)
					{
						if(updateDependantMappings)
						{
							// remove previous dependent mappings.
							UpdateDependantMappings(input, type, sources[i], mappingNumber, REMOVE);
						}

						// replace previous mappings.
						mapper.Change(i, source, m_inputs[input]);
						KEYBOARD_MOUSE_ONLY(m_RecachedConflictCount = true);
						m_isInputMapped[input] = true;

#if __BANK && __WIN32PC
						// update rag.
						// joystick text.
						if(type == JOYSTICK)
						{
							GetMappingParameterName(source, m_ragData[input].m_Joystick, RagData::TEXT_SIZE);
						}
						// Primary text.
						else if(mappingCount == 0)
						{
							GetMappingParameterName(source, m_ragData[input].m_KeyboardMousePrimary, RagData::TEXT_SIZE);
						}
						// secondary text.
						else if(mappingCount == 1)
						{
							GetMappingParameterName(source, m_ragData[input].m_KeyboardMouseSecondary, RagData::TEXT_SIZE);
						}
#endif // __BANK && __WIN32PC
						updated = true;
					}

					// only increment the counter if we have not found the mapping we are looking for.
					if(!updated)
					{
						++mappingCount;
					}
				}
			}

			// if nothing was updated then add the mapping.
			if(!updated)
			{
				mapper.Map(source.m_Device, source.m_Parameter, m_inputs[input], source.m_DeviceIndex);
				m_isInputMapped[input] = true;

#if __BANK && __WIN32PC
				// update rag 
				// joystick text.
				if(type == JOYSTICK)
				{
					GetMappingParameterName(source, m_ragData[input].m_Joystick, RagData::TEXT_SIZE);
				}
				// Primary text.
				else if(type == PC_KEYBOARD_MOUSE)
				{
					if(mappingCount == 0)
					{
						GetMappingParameterName(source, m_ragData[input].m_KeyboardMousePrimary, RagData::TEXT_SIZE);
					}
					// secondary text.
					else if(mappingCount == 1)
					{
						GetMappingParameterName(source, m_ragData[input].m_KeyboardMouseSecondary, RagData::TEXT_SIZE);
					}
				}
#endif // __BANK && __WIN32PC
			}

			if(type != UNKNOWN_MAPPING_TYPE)
			{
				if(mappingNumber > mappingCount)
				{
					// the input was added so update the index accordingly.
					mappingNumber = mappingCount;
				}

				if(updateDependantMappings)
				{
					UpdateDependantMappings(input, type, source, mappingNumber, ADD_UPDATE);
				}
			}
		}
	}
}

void CControl::UpdateDependantMappings( const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, UpdateType updateType )
{
	UpdateAxisDefinitionMappings(input, type, source, mappingNumber, updateType);
	KEYBOARD_MOUSE_ONLY(UpdateIdenticalMappings(input, type, source, mappingNumber));
}

void CControl::UpdateAxisDefinitionMappings( const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, UpdateType updateType )
{
	// we need a copy to find the device id.
	ioSource thisSource = source;

	// find all inputs that are dependent on this input.
	for(u32 axisIndex = 0; axisIndex < ms_settings.m_AxisDefinitions.size(); ++axisIndex)
	{
		const ControlInput::AxisDefinition& axisDefinition = ms_settings.m_AxisDefinitions[axisIndex];

		// see if the mapping is dependent.
		if(axisDefinition.m_Negative == input || axisDefinition.m_Positive == input)
		{
			InputType altInputId = UNDEFINED_INPUT;

			// Get the other input this axis is based on.
			if(axisDefinition.m_Negative == input)
			{
				altInputId = axisDefinition.m_Positive;
			}
			else
			{
				altInputId = axisDefinition.m_Negative;
			}
			Mapper mapperId = ms_settings.m_InputMappers[altInputId];

			controlAssertf(mapperId != DEPRECATED_MAPPER, "%s is deprecated so it is not possible to map a device to it!", GetInputName(altInputId));
			if(controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "%s has an invalid mapper so it is not possible to map a device to it!", GetInputName(altInputId)))
			{
				ioMapper::ioSourceList sources;
				GetSpecificInputSources(altInputId, sources, source.m_DeviceIndex);

				const static ioSource NULL_MAPPING = ioSource(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
				const ioSource *negative = &NULL_MAPPING;
				const ioSource *positive = &NULL_MAPPING;

				const bool doesMappingExist = mappingNumber < sources.size();
				if(axisDefinition.m_Negative == input)
				{
					negative = &thisSource;
					if(doesMappingExist)
					{
						positive = &sources[mappingNumber];
					}
				}
				else
				{
					if(doesMappingExist)
					{
						negative = &sources[mappingNumber];
					}
					positive = &thisSource;
				}

				// Retrieves the axis source based on two other inputs. 
				ioSource axisSource = GetAxisCompatableSource(*negative, *positive);

				if(axisSource.m_Device != IOMS_UNDEFINED)
				{
					if(updateType == REMOVE)
					{
						RemoveMapping(axisSource, axisDefinition.m_Input, type, mappingNumber);
					}
					else
					{
						UpdateMapping(axisDefinition.m_Input, type, axisSource, mappingNumber, true);
					}
				}
				else if(updateType == ADD_UPDATE && doesMappingExist)
				{
					// remove previous incompatible input.
					RemoveMapping(sources[mappingNumber], altInputId, type, mappingNumber);
				}
			}
		}
	}
}

bool CControl::RemoveMapping(const ioSource& source, InputType inputId, MappingType WIN32PC_ONLY(BANK_ONLY(type)), u32 WIN32PC_ONLY(BANK_ONLY(mappingNumber)))
{
	bool result = false;
	Mapper mapperId = ms_settings.m_InputMappers[inputId];

	controlAssertf(mapperId != DEPRECATED_MAPPER, "%s is deprecated so it is not possible to map a device to it!", GetInputName(inputId));
	if(controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "%s has an invalid mapper so it is not possible to map a device to it!", GetInputName(inputId)))
	{
		result = m_Mappers[mapperId].Remove(source, m_inputs[inputId]);

#if __BANK && __WIN32PC
		if(result)
		{
			switch(type)
			{
			case JOYSTICK:
				safecpy(m_ragData[inputId].m_Joystick, "<NOT ASSIGNED>", RagData::TEXT_SIZE);
				break;
			case PC_KEYBOARD_MOUSE:
				if(mappingNumber == 0)
				{
					safecpy(m_ragData[inputId].m_KeyboardMousePrimary, "<NOT ASSIGNED>", RagData::TEXT_SIZE);
				}
				else if(mappingNumber == 1)
				{
					safecpy(m_ragData[inputId].m_KeyboardMouseSecondary, "<NOT ASSIGNED>", RagData::TEXT_SIZE);
				}
				break;
			default:
				break;
			}
		}
#endif // __BANK && __WIN32PC
	}

	return result;
}

ioSource CControl::GetAxisCompatableSource( const ioSource& negative, const ioSource& positive )
{
	ioSource source;
	source.m_Device = IOMS_UNDEFINED;

	switch(negative.m_Device)
	{
	case IOMS_KEYBOARD:
	case IOMS_MOUSE_BUTTON:
	case IOMS_MOUSE_WHEEL:
		{
			if(positive.m_Parameter == IOM_WHEEL_UP && negative.m_Parameter == IOM_WHEEL_DOWN) 
			{
				source.m_Device			= IOMS_MOUSE_WHEEL;
				source.m_Parameter		= IOM_AXIS_WHEEL_RELATIVE;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if(positive.m_Parameter == IOM_WHEEL_DOWN && negative.m_Parameter == IOM_WHEEL_UP)
			{
				source.m_Device			= IOMS_MOUSE_WHEEL;
				source.m_Parameter		= IOM_IAXIS_WHEEL_RELATIVE;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else
			{
				source.m_Parameter = ioMapper::MakeMkbAxis(
					ioMapper::ConvertParameterToMapperValue(positive.m_Device, positive.m_Parameter),
					ioMapper::ConvertParameterToMapperValue(negative.m_Device, negative.m_Parameter) );

				source.m_Device			= IOMS_MKB_AXIS;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}

			break;
		}
	case IOMS_PAD_DIGITALBUTTON:
		{
			// errornous params.
			if(negative.m_Device != positive.m_Device)
				return source;

			source.m_Device		= IOMS_DIGITALBUTTON_AXIS;
			source.m_Parameter = negative.m_Parameter | positive.m_Parameter << 8;
			source.m_DeviceIndex	= negative.m_DeviceIndex;
			break;
		}
	case IOMS_PAD_AXIS:
		{
			// errornous params.
			if(negative.m_Device != positive.m_Device || negative.m_DeviceIndex != positive.m_DeviceIndex)
				return source;

			ioMapperParameter negParam = ioMapper::ConvertParameterToMapperValue(negative.m_Device, negative.m_Parameter);
			ioMapperParameter posParam = ioMapper::ConvertParameterToMapperValue(positive.m_Device, positive.m_Parameter);

			if(    (negParam == IOM_AXIS_LY_UP && posParam == IOM_AXIS_LY_DOWN)
				|| (negParam == IOM_AXIS_LUP && posParam == IOM_AXIS_LDOWN) )
			{
				source.m_Device		= IOMS_PAD_AXIS;
				source.m_Parameter	= IOM_AXIS_LY;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if( (negParam == IOM_AXIS_LX_LEFT && posParam == IOM_AXIS_LX_RIGHT)
				||   (negParam == IOM_AXIS_LLEFT && posParam == IOM_AXIS_LRIGHT) )
			{
				source.m_Device		= IOMS_PAD_AXIS;
				source.m_Parameter	= IOM_AXIS_LX;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if( (negParam == IOM_AXIS_RY_UP && posParam == IOM_AXIS_RY_DOWN)
				||   (negParam == IOM_AXIS_RUP && posParam == IOM_AXIS_RDOWN) )
			{
				source.m_Device		= IOMS_PAD_AXIS;
				source.m_Parameter	= IOM_AXIS_RY;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if( (negParam == IOM_AXIS_RX_LEFT && posParam == IOM_AXIS_RX_RIGHT)
				||   (negParam == IOM_AXIS_RLEFT && posParam == IOM_AXIS_RRIGHT) )
			{
				source.m_Device		= IOMS_PAD_AXIS;
				source.m_Parameter	= IOM_AXIS_RX;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else
			{
				// During load, we temporarily map to key null until the other direction is know. Ignore this.
				controlAssertf( (positive.m_Device == IOMS_KEYBOARD && positive.m_Parameter == KEY_NULL) || (negative.m_Device == IOMS_KEYBOARD && negative.m_Parameter == KEY_NULL),
					"unsupported gamepad axis assigned to axis!");
			}

			break;
		}
	case IOMS_JOYSTICK_AXIS_NEGATIVE:
		{
			if(positive.m_Device != IOMS_JOYSTICK_AXIS_POSITIVE || positive.m_Parameter != negative.m_Parameter || positive.m_DeviceIndex != negative.m_DeviceIndex)
			{
				return source;
			}


			ioMapperParameter negParam = ioMapper::ConvertParameterToMapperValue(negative.m_Device, negative.m_Parameter);
			ioMapperParameter posParam = ioMapper::ConvertParameterToMapperValue(positive.m_Device, positive.m_Parameter);


			if( negParam >= IOM_JOYSTICK_AXIS1 && negParam <= IOM_JOYSTICK_AXIS8 &&
				posParam >= IOM_JOYSTICK_AXIS1 && posParam <= IOM_JOYSTICK_AXIS8 )
			{
				source.m_Device			= IOMS_JOYSTICK_AXIS;
				source.m_Parameter		= negative.m_Parameter;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			break;
		}
	case IOMS_JOYSTICK_AXIS_POSITIVE:
		{
			if(positive.m_Device != IOMS_JOYSTICK_AXIS_NEGATIVE || positive.m_Parameter != negative.m_Parameter || positive.m_DeviceIndex != negative.m_DeviceIndex)
			{
				return source;
			}


			ioMapperParameter negParam = ioMapper::ConvertParameterToMapperValue(negative.m_Device, negative.m_Parameter);
			ioMapperParameter posParam = ioMapper::ConvertParameterToMapperValue(positive.m_Device, positive.m_Parameter);


			if( negParam >= IOM_JOYSTICK_AXIS1 && negParam <= IOM_JOYSTICK_AXIS8 &&
				posParam >= IOM_JOYSTICK_AXIS1 && posParam <= IOM_JOYSTICK_AXIS8 )
			{
				source.m_Device			= IOMS_JOYSTICK_IAXIS;
				source.m_Parameter		= negative.m_Parameter;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			break;
		}
	case IOMS_JOYSTICK_POV:
		{
			if(positive.m_Device != IOMS_JOYSTICK_POV || positive.m_DeviceIndex != negative.m_DeviceIndex || positive.m_Parameter == negative.m_Parameter)
			{
				return source;
			}

			source.m_Device			= IOMS_JOYSTICK_POV_AXIS;
			source.m_Parameter		= negative.m_Parameter | positive.m_Parameter << 8;
			source.m_DeviceIndex	= negative.m_DeviceIndex;

			break;
		}
	case IOMS_MOUSE_ABSOLUTEAXIS:
	case IOMS_MOUSE_CENTEREDAXIS:
	case IOMS_MOUSE_RELATIVEAXIS:
	case IOMS_MOUSE_SCALEDAXIS:
		{
			if(    (positive.m_Parameter == IOM_AXIS_X_RIGHT && negative.m_Parameter == IOM_AXIS_X_LEFT)
				|| (positive.m_Parameter == IOM_AXIS_X && negative.m_Parameter == IOM_AXIS_X) )
			{
				source.m_Device			= negative.m_Device;
				source.m_Parameter		= IOM_AXIS_X;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if(    (positive.m_Parameter == IOM_AXIS_X_LEFT && negative.m_Parameter == IOM_AXIS_X_RIGHT)
				|| (positive.m_Parameter == IOM_IAXIS_X && negative.m_Parameter == IOM_IAXIS_X) )
			{
				source.m_Device			= negative.m_Device;
				source.m_Parameter		= IOM_IAXIS_X;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if(   (positive.m_Parameter == IOM_AXIS_Y_DOWN && negative.m_Parameter == IOM_AXIS_Y_UP)
				|| (positive.m_Parameter == IOM_AXIS_Y && negative.m_Parameter == IOM_AXIS_Y) )
			{
				source.m_Device			= negative.m_Device;
				source.m_Parameter		= IOM_AXIS_Y;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else if(   (positive.m_Parameter == IOM_AXIS_Y_UP && negative.m_Parameter == IOM_AXIS_Y_DOWN)
				|| (positive.m_Parameter == IOM_IAXIS_Y && negative.m_Parameter == IOM_IAXIS_Y) )
			{
				source.m_Device			= negative.m_Device;
				source.m_Parameter		= IOM_IAXIS_Y;
				source.m_DeviceIndex	= negative.m_DeviceIndex;
			}
			else
			{
				// During load, we temporarily map to key null until the other direction is know. Ignore this.
				controlAssertf( (positive.m_Device == IOMS_KEYBOARD && positive.m_Parameter == KEY_NULL) || (negative.m_Device == IOMS_KEYBOARD && negative.m_Parameter == KEY_NULL),
					"Incompatible mouse axis type assigned to input!");
			}
			break;
		}
	default:
		break;
	}

	return source;
}

// TEMPORARY UNTIL STRING READ FROM LANGUAGE FILE
#if KEYBOARD_MOUSE_SUPPORT
void CControl::GetMappingParameterName( const ioSource& source, char *buffer, size_t bufferSize )
{
	if(source.m_Device == IOMS_MKB_AXIS)
	{
		ioMapperParameter negative = ioMapper::ConvertParameterToMapperValue(source.m_Device, ioMapper::GetMkbAxisNegative(source.m_Parameter, true));
		ioMapperParameter positive = ioMapper::ConvertParameterToMapperValue(source.m_Device, ioMapper::GetMkbAxisPositive(source.m_Parameter, true));

		formatf(buffer, bufferSize, "%s, %s", GetParameterText(negative), GetParameterText(positive));
	}
	else if(source.m_Device == IOMS_DIGITALBUTTON_AXIS || source.m_Device == IOMS_JOYSTICK_POV_AXIS)
	{
		ioMapperParameter parameter1 = ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter & 0xFF);
		ioMapperParameter parameter2 = ioMapper::ConvertParameterToMapperValue(source.m_Device, (source.m_Parameter & 0xFF00) >> 8);
		formatf(buffer, bufferSize, "%s, %s", GetParameterText(parameter1), GetParameterText(parameter2));
	}
	else
	{
		ioMapperParameter parameter = ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter);
		safecpy(buffer, GetParameterText(parameter), bufferSize);
	}
}
#endif // KEYBOARD_MOUSE_SUPPORT

void CControl::EnableFrontendInputs()
{
	for(u32 i = INPUT_FRONTEND_CONTROL_BEGIN; i <= INPUT_FRONTEND_CONTROL_END; ++i)
	{
#if __BANK
		if(!m_ragData[i].m_ForceDisabled)
#endif // __BANK
		{
			m_inputs[i].Enable();
		}
	}
}

void CControl::DisableBackButtonInputs()
{
	ioMapper::ioSourceList sources;
	GetInputSources(INPUT_FRONTEND_CANCEL, sources);
	DisableInputsByActiveSources(sources, ioValue::DEFAULT_DISABLE_OPTIONS);
}

bool CControl::IsUsingAnalogueAiming() const
{
#if RSG_ORBIS
	// Remote play (vita) does not have analogue aiming.
	if(IsUsingRemotePlay())
	{
		return false;
	}
	else
#endif // RSG_ORBIS
	{
		eControlLayout layout = GetActiveLayout();
		return (layout != TRIGGER_SWAP_TPS_LAYOUT && layout != SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT);
	}
}

CControl::eMouseSteeringMode CControl::GetMouseSteeringMode(s32 iPref)
{
	return (CControl::eMouseSteeringMode)CPauseMenu::GetMenuPreference(iPref);
}

bool CControl::IsMouseSteeringOn(s32 iPref)
{
	switch(GetMouseSteeringMode(iPref))
	{
	case(eMSM_Vehicle):
		return true;
	default:
		break;
	}

	return false;
}


void CControl::SetVehicleSteeringExclusive()
{
#if KEYBOARD_MOUSE_SUPPORT
	if(m_inputs[INPUT_VEH_MOVE_LR].IsEnabled())
	{
		bool bMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == eMSM_Vehicle;
		bool bCameraMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == eMSM_Camera;

		// Do not set exclusivity if the steering has been disabled by some other code and the user is not shooting or aiming.
		if( ((bMouseSteering && m_inputs[INPUT_VEH_ATTACK].IsUp() && m_inputs[INPUT_VEH_AIM].IsUp() && !m_DriveCameraToggleOn) || (bCameraMouseSteering && m_DriveCameraToggleOn)) && WasKeyboardMouseLastKnownSource() && (m_inputs[INPUT_VEH_MOUSE_CONTROL_OVERRIDE].IsUp() || m_UseSecondaryDriveCameraToggle) )
		{
			// Disable mouse looking up and down (the left and right gets disabled when we set INPUT_VEH_MOVE_LR exclusive).
			DisableInput(INPUT_LOOK_UD);
			DisableInput(INPUT_LOOK_UP_ONLY);
			DisableInput(INPUT_LOOK_DOWN_ONLY);

			SetInputExclusive(INPUT_VEH_MOVE_LR);

			// enable only versions, we use EnableInput() function for extra rag checks.
			EnableInput(INPUT_VEH_MOVE_LEFT_ONLY);
			EnableInput(INPUT_VEH_MOVE_RIGHT_ONLY);
		}
		else
		{
			// Auto center is scaled, centered axis does not auto center.
			const ioMapperSource source = (m_MouseVehicleCarAutoCenter) ? IOMS_MOUSE_SCALEDAXIS : IOMS_MOUSE_CENTEREDAXIS;
			m_inputs[INPUT_VEH_MOVE_LR].Ignore(source);
			m_inputs[INPUT_VEH_MOVE_LEFT_ONLY].Ignore(source);
			m_inputs[INPUT_VEH_MOVE_RIGHT_ONLY].Ignore(source);
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT
}

void CControl::SetVehicleFlySteeringExclusive()
{
#if KEYBOARD_MOUSE_SUPPORT
	if(m_inputs[INPUT_VEH_FLY_ROLL_LR].IsEnabled() && m_inputs[INPUT_VEH_FLY_PITCH_UD].IsEnabled())
	{
		bool bMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle;
		bool bCameraMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera;
		bool mouseFlyingXYSwap = (CPauseMenu::GetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING) != 0);

		// Do not set exclusivity if the steering has been disabled by some other code and the user is not shooting or aiming.
		// TR TODO: Should we be using INPUT_AIM or check the right mouse button directly as we are in PC specific code?
		if( ((bMouseFlySteering && !m_DriveCameraToggleOn) ||(bCameraMouseFlySteering && m_DriveCameraToggleOn)) && WasKeyboardMouseLastKnownSource())
		{
			if(mouseFlyingXYSwap)
			{

				SetInputExclusive(INPUT_VEH_FLY_PITCH_UD);
				SetInputExclusive(INPUT_VEH_FLY_YAW_LEFT);
				SetInputExclusive(INPUT_VEH_FLY_YAW_RIGHT);

				// enable only versions, we use EnableInput() function for extra rag checks.
				EnableInput(INPUT_VEH_FLY_PITCH_UP_ONLY);
				EnableInput(INPUT_VEH_FLY_PITCH_DOWN_ONLY);
				EnableInput(INPUT_VEH_FLY_YAW_LEFT);
				EnableInput(INPUT_VEH_FLY_YAW_RIGHT);

			}
			else
			{

				SetInputExclusive(INPUT_VEH_FLY_ROLL_LR);
				SetInputExclusive(INPUT_VEH_FLY_PITCH_UD);

				// enable only versions, we use EnableInput() function for extra rag checks.
				EnableInput(INPUT_VEH_FLY_ROLL_LEFT_ONLY);
				EnableInput(INPUT_VEH_FLY_ROLL_RIGHT_ONLY);
				EnableInput(INPUT_VEH_FLY_PITCH_UP_ONLY);
				EnableInput(INPUT_VEH_FLY_PITCH_DOWN_ONLY);
			}
		}
		else
		{
			if(mouseFlyingXYSwap)
			{
				// Auto center is scaled, centered axis does not auto center.
				const ioMapperSource source = (m_MouseVehicleFlyAutoCenter) ? IOMS_MOUSE_SCALEDAXIS : IOMS_MOUSE_CENTEREDAXIS;
				m_inputs[INPUT_VEH_FLY_PITCH_UD].Ignore(source);
				m_inputs[INPUT_VEH_FLY_PITCH_UP_ONLY].Ignore(source);
				m_inputs[INPUT_VEH_FLY_PITCH_DOWN_ONLY].Ignore(source);
				m_inputs[INPUT_VEH_FLY_YAW_LEFT].Ignore(source);
				m_inputs[INPUT_VEH_FLY_YAW_RIGHT].Ignore(source);
			}
			else
			{
				// Auto center is scaled, centered axis does not auto center.
				const ioMapperSource source = (m_MouseVehicleFlyAutoCenter) ? IOMS_MOUSE_SCALEDAXIS : IOMS_MOUSE_CENTEREDAXIS;
				m_inputs[INPUT_VEH_FLY_ROLL_LR].Ignore(source);
				m_inputs[INPUT_VEH_FLY_PITCH_UD].Ignore(source);
				m_inputs[INPUT_VEH_FLY_ROLL_LEFT_ONLY].Ignore(source);
				m_inputs[INPUT_VEH_FLY_ROLL_RIGHT_ONLY].Ignore(source);
				m_inputs[INPUT_VEH_FLY_PITCH_UP_ONLY].Ignore(source);
				m_inputs[INPUT_VEH_FLY_PITCH_DOWN_ONLY].Ignore(source);
			}
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT
}

void CControl::SetVehicleSubSteeringExclusive()
{
#if KEYBOARD_MOUSE_SUPPORT
	if(m_inputs[INPUT_VEH_SUB_TURN_LR].IsEnabled() && m_inputs[INPUT_VEH_SUB_PITCH_UD].IsEnabled())
	{
		bool bMouseSubSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_SUB) == CControl::eMSM_Vehicle;
		bool bCameraMouseSubSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_SUB) == CControl::eMSM_Camera;

		// Do not set exclusivity if the steering has been disabled by some other code and the user is not shooting or aiming.
		if( ((bMouseSubSteering && m_inputs[INPUT_VEH_FLY_ATTACK].IsUp() && !m_DriveCameraToggleOn) || (bCameraMouseSubSteering && m_DriveCameraToggleOn)) && WasKeyboardMouseLastKnownSource() )
		{
			SetInputExclusive(INPUT_VEH_SUB_TURN_LR);
			SetInputExclusive(INPUT_VEH_SUB_PITCH_UD);

			// enable only versions, we use EnableInput() function for extra rag checks.
			EnableInput(INPUT_VEH_SUB_TURN_LEFT_ONLY);
			EnableInput(INPUT_VEH_SUB_TURN_RIGHT_ONLY);
			EnableInput(INPUT_VEH_SUB_PITCH_UP_ONLY);
			EnableInput(INPUT_VEH_SUB_PITCH_DOWN_ONLY);
			EnableInput(INPUT_VEH_SUB_TURN_HARD_LEFT);
			EnableInput(INPUT_VEH_SUB_TURN_HARD_RIGHT);
		}
		else
		{
			// Auto center is scaled, centered axis does not auto center.
			const ioMapperSource source = (m_MouseVehicleSubAutoCenter) ? IOMS_MOUSE_SCALEDAXIS : IOMS_MOUSE_CENTEREDAXIS;
			m_inputs[INPUT_VEH_SUB_TURN_LR].Ignore(source);
			m_inputs[INPUT_VEH_SUB_PITCH_UD].Ignore(source);
			m_inputs[INPUT_VEH_SUB_TURN_LEFT_ONLY].Ignore(source);
			m_inputs[INPUT_VEH_SUB_TURN_RIGHT_ONLY].Ignore(source);
			m_inputs[INPUT_VEH_SUB_PITCH_UP_ONLY].Ignore(source);
			m_inputs[INPUT_VEH_SUB_PITCH_DOWN_ONLY].Ignore(source);
			m_inputs[INPUT_VEH_SUB_TURN_HARD_LEFT].Ignore(source);
			m_inputs[INPUT_VEH_SUB_TURN_HARD_RIGHT].Ignore(source);
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT
}


bool CControl::IsUsingCursor() const
{
#if KEYBOARD_MOUSE_SUPPORT
	return WasKeyboardMouseLastKnownSource();
#elif RSG_TOUCHPAD
	return (CControlMgr::GetPad(m_padNum).GetNumTouches() != 0);
#else
	return false;
#endif // KEYBOARD_MOUSE_SUPPORT
}

bool CControl::IsUsingRemotePlay() const
{
#if RSG_ORBIS
	return CControlMgr::GetPad(m_padNum).IsRemotePlayPad();
#else
	return false;
#endif // RSG_ORBIS
}

bool CControl::ScriptCheckForControlsChange()
{
	if(m_WasControlsRefreshedThisFrame)
	{
		m_HasScriptCheckControlsRefresh = true;
	}

	return m_WasControlsRefreshedThisFrame ||
		   m_WasControlsRefreshedLastFrame
		   KEYBOARD_MOUSE_ONLY(|| m_LastFrameDeviceIndex != m_LastKnownSource.m_DeviceIndex);
}

bool CControl::CanUseToggleRun() const
{
	const eControlLayout layout = GetActiveLayout();

	return layout == STANDARD_FPS_LAYOUT ||
		   layout == TRIGGER_SWAP_FPS_LAYOUT ||
		   layout == SOUTHPAW_FPS_LAYOUT ||
		   layout == SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT
		   KEYBOARD_MOUSE_ONLY(|| WasKeyboardMouseLastKnownSource());
}

void CControl::ClearToggleRun()	
{ 
	bool bToggleRunValue = false;

#if KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED 
	// B*2312353: Default toggle run value to on when in FPS mode using mouse/keyboard.
	const CPed* pPed = CPedFactory::GetFactory() ? CGameWorld::FindLocalPlayer() : NULL;
	if (pPed && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && WasKeyboardMouseLastKnownSource() && !pPed->GetIsSwimming())
	{
		bToggleRunValue = true;
	}
#endif	// KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED

	m_ToggleRunOn = bToggleRunValue; 
}

bool CControl::CanUseToggleAim() const
{
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	if(pPed)
	{
		const CPedWeaponManager *pWeapMgr = pPed->GetWeaponManager(); 
		if (pWeapMgr) 
		{ 
			if(pWeapMgr->GetEquippedVehicleWeapon()) 
			{ 
				if( pWeapMgr->GetEquippedVehicleWeapon()->GetWeaponInfo())
				{
					return pWeapMgr->GetEquippedVehicleWeapon()->GetWeaponInfo()->GetIsGun();
				}
			} 
			else if(pWeapMgr->GetEquippedWeaponInfo() && pWeapMgr->GetEquippedWeaponInfo() == pWeapMgr->GetSelectedWeaponInfo()) 
			{ 
				return pWeapMgr->GetEquippedWeaponInfo()->GetIsGun();
			} 
		}
	}

	return false;
}

CControl::Settings::Settings()
	: m_AxisDefinitions()
	, m_HistorySupport()
#if KEYBOARD_MOUSE_SUPPORT
	, m_MappingSettings()
	, m_DynamicMappingList()
	, m_InputCategories(0)
#endif // KEYBOARD_MOUSE_SUPPORT
	, m_NeedsInitializing(true)
{}

void CControl::Settings::Init()
{
	if(m_NeedsInitializing)
	{
		m_NeedsInitializing = false;

		INIT_PARSER;

		ControlInput::InputSettings settings;
		controlVerifyf(PARSER.LoadObject(INPUT_SETTINGS_FILE, "", settings), "Error loading input settings file!");

		m_HistorySupport  = settings.m_HistorySupport;
		m_RelatedInputs   = settings.m_RelatedInputs;

		KEYBOARD_MOUSE_ONLY(LoadKeyboardLayout());
		m_AxisDefinitions = settings.m_AxisDefinitions;

		WIN32PC_ONLY(m_GamepadDefinitionList.m_Devices.Reset());

		// set all mappers to an undefined mapper.
		for(u32 i = 0; i < MAX_INPUTS; ++i)
		{
			m_InputMappers[i] = UNDEFINED_MAPPER;
		}

		// put inputs mappers into an array indexed by input id.
		for(u32 i = 0; i < settings.m_MapperAssignements.size(); ++i)
		{
			ControlInput::InputMapperAssignment assignment = settings.m_MapperAssignements[i];
			for(u32 inputIndex = 0; inputIndex < assignment.m_Inputs.size(); ++inputIndex)
			{
				u32 inputId = assignment.m_Inputs[inputIndex];

				controlAssertf( (assignment.m_Mapper > UNDEFINED_MAPPER && assignment.m_Mapper < MAPPERS_MAX) || assignment.m_Mapper == DEPRECATED_MAPPER,
					"Invalid mapper assigned to input %s!",
					parser_rage__InputType_Data.m_Names[inputId] );

				controlAssertf( m_InputMappers[inputId] == UNDEFINED_MAPPER, "'%s' is assigned to the '%s' mapper but is also assigned to '%s' in '%s'!",
					parser_rage__InputType_Data.m_Names[inputId],
					parser_rage__Mapper_Strings[m_InputMappers[inputId]],
					parser_rage__Mapper_Strings[assignment.m_Mapper],
					INPUT_SETTINGS_FILE );

				m_InputMappers[inputId] = assignment.m_Mapper;
			}
		}

		// Check for any inputs that do not have a mapper assigned.
		for(s32 i = 0; i < MAX_INPUTS; ++i)
		{
			if( !controlVerifyf(m_InputMappers[i] != UNDEFINED_MAPPER, "Input %s is not assigned to a mapper in %s!", GetInputName(static_cast<InputType>(i)), INPUT_SETTINGS_FILE) )
			{
				m_InputMappers[i] = (Mapper)0;
			}
		}

		controlAssertf( settings.m_InputGroupDefinitions.size() == MAX_INPUTGROUPS,
			"The wrong number of INPUTGROUP definitions exist in '%s'. Expected %d but got %d!",
			INPUT_SETTINGS_FILE,
			MAX_INPUTGROUPS,
			settings.m_InputGroupDefinitions.size() );

		for(s32 i = 0; i < settings.m_InputGroupDefinitions.size(); ++i)
		{
			InputGroup group = settings.m_InputGroupDefinitions[i].m_InputGroup;

			if( controlVerifyf(m_InputGroupDefinitions[group].size() == 0, "Duplicate definition for '%s' in '%s'!", GetInputGroupName(group), INPUT_SETTINGS_FILE) )
			{
				m_InputGroupDefinitions[group] = settings.m_InputGroupDefinitions[i].m_Inputs;
			}
		}

#if KEYBOARD_MOUSE_SUPPORT
		// As it is only possible to re-map controls in on pc we do not need to store information on mapping inputs on other platforms.
		controlVerifyf(PARSER.LoadObject(PC_INPUT_SETTINGS_FILE, "", m_MappingSettings), "Error loading keyboard/mouse settings file!");

		// Create a catagory lookup by input.
		atMap<atHashString, ControlInput::InputList>::Iterator it = m_MappingSettings.m_Categories.CreateIterator();
		for (it.Start(); !it.AtEnd(); it.Next())
		{
			for (u32 i = 0; i < it.GetData().m_Inputs.size(); ++i)
			{
				const InputType input = it.GetData().m_Inputs[i];

				controlAssertf( m_InputCategories[input] == 0,
					     "Input '%s' (%d) has been placed in more than one catagory inside '" PC_INPUT_SETTINGS_FILE "'!",
						 GetInputName(input),
						 input );

				m_InputCategories[input] = it.GetKey().GetHash();
			}
		}

		m_MouseSettings = m_MappingSettings.m_MouseSettings;

		// By default the input is mappable unless marked as unmappable.
		for(s32 input = 0; input < m_InputMappable.size(); ++input)
		{
			m_InputMappable[input] = true;
		}

		// Mark all unmappable inputs as unmappable. This store these hash lookups in a better way for looking up.
		for(s32 catagoryIndex = 0; catagoryIndex < ms_settings.m_MappingSettings.m_UnmappableList.size(); ++catagoryIndex)
		{
			const ControlInput::InputList* inputs = ms_settings.m_MappingSettings.m_Categories.Access(m_MappingSettings.m_UnmappableList[catagoryIndex]);
			if(inputs != NULL)
			{
				for(s32 inputIndex = 0; inputIndex < inputs->m_Inputs.size(); ++inputIndex)
				{
					m_InputMappable[inputs->m_Inputs[inputIndex]] = false;
				}
			}
		}

		// Free memory.
		ms_settings.m_MappingSettings.m_UnmappableList.Reset();

		controlVerifyf(PARSER.LoadObject(PC_DYNAMIC_SETTINGS_FILE, "", m_DynamicMappingList), "Error loading dynamic mappings file!");
#if RSG_ASSERT
		// ensure all inputs are in a catagory.
		for(u32 i = 0; i < m_InputCategories.size(); ++i)
		{
			controlAssertf( m_InputCategories[i] != 0,
							"'%s' (%d) has not been assigned a catagory inside '" PC_INPUT_SETTINGS_FILE "'!",
							GetInputName(static_cast<InputType>(i)),
							i );
		}

		// ensure all mappings are mapped correctly.
		::rage::atMap< ::rage::atHashWithStringNotFinal, rage::ControlInput::DynamicMappings >::Iterator iterator = m_DynamicMappingList.m_DynamicMappings.CreateIterator();
		for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
		{
			controlAssertf( iterator.GetData().m_Mappings.size() == iterator.GetData().m_Mappings.max_size(),
							"Incorrect number of mappings for dynamic mappings!" );
		}
#endif // RSG_ASSERT
#endif // KEYBOARD_MOUSE_SUPPORT

		WIN32PC_ONLY(RebuildGamepadDefinitionList());

		SHUTDOWN_PARSER;
	}

}


int CControl::Settings::InputCompare(const InputType* a, const InputType* b)
{
	return *a - *b;
}

#if KEYBOARD_MOUSE_SUPPORT

void CControl::Settings::LoadKeyboardLayout()
{
	const char* localName = NULL;
	char16 wideLocalName[LOCALE_NAME_MAX_LENGTH];
	GetSystemDefaultLocaleName(reinterpret_cast<wchar_t*>(wideLocalName), LOCALE_NAME_MAX_LENGTH);

	USES_CONVERSION;
	const char* defaultLocalName = WIDE_TO_UTF8(wideLocalName);

	if(PARAM_keyboardLocal.Get())
	{
		PARAM_keyboardLocal.Get(localName);
		controlDisplayf("Looking for keyboard layout %s as requested by command line.", localName);
	}
	else if(StringLength(defaultLocalName) > 0)
	{
		localName = defaultLocalName;
		controlDisplayf("Looking for keyboard layout %s as returned by system.", localName);
	}
	else
	{
		localName = DEFAULT_KEYBOARD_LAYOUT;
		controlAssertf(false, "Looking for keyboard layout %s as we cannot retrieve correct layout from the system!", localName);
	}

	char filePath[RAGE_MAX_PATH];
	controlDisplayf("Attempting to load keyboard layout at '%s%s'.", localName, KEYBOARD_LAYOUT_EXTENSION);
	formatf(filePath, "%s%s%s", KEYBOARD_LAYOUT_DIR, localName, KEYBOARD_LAYOUT_EXTENSION);

	// If the keyboard layout file does not exist then try and load the region neutral version.
	if(!ASSET.Exists(filePath, ""))
	{
		// Get the neutral region name (e.g. en-GB and en-US will just be en).
		char localNeutralFileName[LOCALE_NAME_MAX_LENGTH] = {0};
		safecpy(localNeutralFileName, localName);
		char* newEndPos = strstr(localNeutralFileName, "-");
		if(newEndPos)
		{
			*newEndPos = 0;
		}

		controlDisplayf("Attempting to Fall back to keyboard layout at '%s%s'.", localNeutralFileName, KEYBOARD_LAYOUT_EXTENSION);
		formatf(filePath, "%s%s%s", KEYBOARD_LAYOUT_DIR, localNeutralFileName, KEYBOARD_LAYOUT_EXTENSION);

		// If the region neutral keyboard layout file does not exist then use the default en-US version.
		if(!ASSET.Exists(filePath, ""))
		{
			controlDisplayf("Could not fall back to '%s%s', falling back to default keyboard layout at '%s%s'.",
							localNeutralFileName,
							KEYBOARD_LAYOUT_EXTENSION,
							DEFAULT_KEYBOARD_LAYOUT,
							KEYBOARD_LAYOUT_EXTENSION);
			formatf(filePath, "%s%s%s", KEYBOARD_LAYOUT_DIR, DEFAULT_KEYBOARD_LAYOUT, KEYBOARD_LAYOUT_EXTENSION);
		}
	}

	LoadKeyboardLayoutFile(filePath);
}

void CControl::Settings::LoadKeyboardLayoutFile(const char* layoutFile)
{
	ControlInput::Keyboard::Layout layout;
	controlVerifyf(PARSER.LoadObject(layoutFile, "", layout), "Error loading keyboard layout file '%s'!", layoutFile);

#if RSG_ASSERT
	atRangeArray<bool, ControlInput::Keyboard::MAX_NUM_KEYS> keyLoaded;
#endif // RSG_ASSERT

	m_KeyInfo.Resize(m_KeyInfo.max_size());
	controlAssert(m_KeyInfo.max_size() == keyLoaded.max_size());

	for(u32 i = 0; i < m_KeyInfo.size(); ++i)
	{
		m_KeyInfo[i].m_Icon = KeyInfo::INVALID_ICON;
		safecpy(m_KeyInfo[i].m_Text, "???");
		ASSERT_ONLY(keyLoaded[i] = false);
	}

	for(u32 i = 0; i < layout.m_Keys.size(); ++i)
	{
		ControlInput::Keyboard::KeyInfo& key = layout.m_Keys[i];
		controlAssertf(key.m_Key != 0, "Error parsing keyboard layout file '%s'; invalid key name", layoutFile);
		if(key.m_Key > 0 && key.m_Key < m_KeyInfo.size())
		{
			KeyInfo& info = m_KeyInfo[key.m_Key];

			controlAssertf( keyLoaded[key.m_Key] == false,
				"'%s' has multiple entries in keyboard layout file '%s'!",
				GetParameterText(static_cast<ioMapperParameter>(key.m_Key)),
				layoutFile );

			ASSERT_ONLY(keyLoaded[key.m_Key] = true);

			if(key.m_Icon == KeyInfo::TEXT_ICON || key.m_Icon == KeyInfo::LARGE_TEXT_ICON)
			{
				if( controlVerifyf(StringLength(key.m_Text) != 0,
					"Invalid key text for %s as there is not icon in '%s'!", 
					GetParameterText(static_cast<ioMapperParameter>(key.m_Key)),
					layoutFile) )
				{
					info.m_Icon = key.m_Icon;
					safecpy(info.m_Text, key.m_Text);
				}
				else
				{
					info.m_Icon = KeyInfo::INVALID_ICON;
				}
			}
			else
			{
				info.m_Icon = key.m_Icon;
			}
		}
	}

#if RSG_ASSERT
	// NOTE: This is incredibly slow as GetParameterText() does a linier search through an array of all parameter names
	// (not just key codes).
	//
	// We are only doing this so we can catch a keycode that does not have icon information in debug builds!
	for(u32 i = 0; i < keyLoaded.size(); ++i)
	{
		const char* keyName = GetParameterText(static_cast<ioMapperParameter>(i));
		if( strnicmp(keyName, "KEY_", 4) == 0 &&
			stricmp(keyName, "KEY_NULL") != 0 &&
			strnicmp(keyName, "KEY_RAGE_", 9) != 0 &&
			stricmp(keyName, "KEY_CHATPAD_GREEN_SHIFT") != 0 &&
			stricmp(keyName, "KEY_CHATPAD_ORANGE_SHIFT") != 0 )
		{
			controlAssertf(keyLoaded[i] != false, "'%s' has no keyboard layout information in '%s'!", keyName, layoutFile);
		}
	}
#endif /// RSG_ASSERT
}

void CControl::Settings::EnumKeyboardLayoutFilesCallback(const fiFindData& data, void* userArg)
{
	const char* ext = ASSET.FindExtensionInPath(data.m_Name);
	if(ext != NULL && strcasecmp(ext, "." DEFAULT_SETTINGS_EXT) == 0)
	{
		static_cast<EnumFileList*>(userArg)->PushAndGrow(data.m_Name);
	}
}

#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
void CControl::Settings::RebuildGamepadDefinitionList()
{
	controlDisplayf("Rebuilding gamepad definition scan!");
	m_GamepadDefinitionList.m_Devices.Reset();

	// load default gamepad definitions.
	controlDisplayf("Loading default gamepad definition file '" PC_GAMEPAD_DEFINITION_FILE "'!");
	if( ASSET.Exists(PC_GAMEPAD_DEFINITION_FILE, "")
		&& !controlVerifyf(PARSER.LoadObject(PC_GAMEPAD_DEFINITION_FILE, "", m_GamepadDefinitionList), "Error loading default gamepad definitions file!") )
	{
		// error loading so tidy up structure.
		m_GamepadDefinitionList.m_Devices.Reset();
		controlErrorf("Error loading gamepad definition file '" PC_GAMEPAD_DEFINITION_FILE "'!");
	}

	char path[PATH_BUFFER_SIZE] = {0};
	GetUserSettingsPath(path, PC_USER_GAMEPAD_DEFINITION_FILE);
	ControlInput::Gamepad::DefinitionList userDefinitions;
	if( ASSET.Exists(path, "") )
	{
		controlDisplayf("Loading user gamepad definition file '%s'!", path);
		if(controlVerifyf(PARSER.LoadObject(path, "", userDefinitions), "Error loading user gamepad definitions file!") )
		{
			// If the user settings successfully loaded then copy the user settings over the defaults.
			atMap< atFinalHashString, ControlInput::Gamepad::Definition >::Iterator iterator = userDefinitions.m_Devices.CreateIterator();
			for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
			{
				controlDisplayf("Custom user definition for device '%s' loaded!", iterator.GetKey().GetCStr());
				m_GamepadDefinitionList.m_Devices[iterator.GetKey()] = iterator.GetData();
			}
		}
		else
		{
			controlErrorf("Error loading gamepad definition file '%s'!", path);
		}
	}
	else
	{
		controlDisplayf("User gamepad definition file '%s' does not exist!", path);
	}
}

const ControlInput::Gamepad::Definition* CControl::Settings::GetGamepadDefinition(const ioJoystick &stick) const
{
	const ControlInput::Gamepad::Definition* definition = m_GamepadDefinitionList.m_Devices.Access(atFinalHashString(stick.GetProductGuidStr()));
	if(definition == NULL)
	{
		definition = m_GamepadDefinitionList.m_Devices.Access(atFinalHashString("Default"));
	}

	return definition;
}

const ControlInput::Gamepad::Definition*  CControl::Settings::GetValidGamepadDefinition(const ioJoystick &stick) const
{
	const ControlInput::Gamepad::Definition* definition = GetGamepadDefinition(stick);
	if(definition != NULL && definition->m_Definitions.size() > 0)
	{
		return definition;
	}

	return NULL;
}

#if !RSG_FINAL
void CControl::EmulateXInputPads()
{
	// This is a hack to get the debug cammera working on the PS4 controller on PC. This is needed for people working from home.
	if(PARAM_enableXInputEmulation.Get())
	{
		for(int stickIndex = 0, padIndex = 0; stickIndex < ioJoystick::GetStickCount() && padIndex < ioPad::MAX_PADS; ++stickIndex)
		{
			const ioJoystick& stick = ioJoystick::GetStick(stickIndex);
			const ControlInput::Gamepad::Definition* pDefinition = ms_settings.GetGamepadDefinition(stick);
			if(pDefinition)
			{
				for(int definitionIndex = 0; definitionIndex < pDefinition->m_Definitions.size(); ++definitionIndex)
				{
					const rage::ControlInput::Gamepad::Source& source = pDefinition->m_Definitions[definitionIndex];
					const float value = GetJoystickValue(stick, source.m_JoystickParameter);
					SetPadValue(padIndex, source.m_PadParameter, value);
				}
				++padIndex;
			}
		}

		// Moved to end because sm_Updaters may modify pad values (e.g. ioFFWheel)
		PF_PUSH_TIMEBAR("ioPad::UpdateDebugAll");
		ioPad::UpdateDebugAll();
		PF_POP_TIMEBAR();
	}
}

float CControl::GetJoystickValue(const ioJoystick& stick, ioMapperParameter param)
{
	if(param >= IOM_JOYSTICK_BUTTON1 && param <= IOM_JOYSTICK_BUTTON32)
	{
		const unsigned buttonBit = 0x1 << (param - IOM_JOYSTICK_BUTTON1);
		if((stick.GetButtons() & buttonBit) == buttonBit)
		{
			return 1.0f;
		}
		else
		{
			return 0.0f;
		}
	}
	else if(param >= IOM_JOYSTICK_AXIS1 && param <= IOM_JOYSTICK_AXIS8)
	{
		const int axis = param - IOM_JOYSTICK_AXIS1;
		return stick.GetNormAxis(axis);
	}
	else if(param >= IOM_POV1_UP && param <= IOM_POV4_LEFT)
	{
		enum {
			POV_UP = 0,
			POV_RIGHT,
			POV_DOWN,
			POV_LEFT
		};
		const int povOffset = param - IOM_POV1_UP;
		const int povIndex = povOffset / 4; // 4 for the 4 directions, up down, left and right.
		const int povDirection = povOffset % 4;
		const int povValue = stick.GetPOV(povIndex);

		// -1 is center
		if(povValue != -1)
		{
			if( (povDirection == POV_UP && (povValue > 27000 || povValue < 9000)) || // up
				(povDirection == POV_RIGHT && (povValue > 0 && povValue < 18000)) || // right
				(povDirection == POV_DOWN && (povValue > 9000 && povValue < 27000)) || // down
				(povDirection == POV_LEFT && (povValue > 18000)) ) // left
			{
				return 1.0f;
			}
		}
		
		return 0.0f;
	}

	return 0.0f;
}

void CControl::SetPadValue(int padIndex, ioMapperParameter param, float value)
{
	ioPad& pad = ioPad::GetPad(padIndex);
	if(param >= L2 && param <= LLEFT)
	{
		const int button = param - L2;
		pad.MergeButton(button, value);
	}
	else if(param >= IOM_AXIS_LX && param <= IOM_AXIS_RY)
	{
		const int axis = param - IOM_AXIS_LX;
		pad.MergeAxis(axis, value);
	}
}
#endif // !RSG_FINAL

#endif // RSG_PC

#if __BANK
CControl::RagData::RagData()
	: m_ForceDisabled(false)
	, m_InputValue(0.0f)
	, m_Enabled(true)
{
#if __WIN32PC
		m_KeyboardMousePrimary[0] = '\0';
		m_KeyboardMouseSecondary[0] = '\0';
#endif // __WIN32PC
}

void CControl::InitWidgets( bkBank& bank, const char *title )
{
	bank.PushGroup(title);

	char buffer[255];
	buffer[0] = '\0';

	// create a list of available inputs.
	// go through each input and add key mapping options.
	for(s32 mapperIndex = 0; mapperIndex < MAPPERS_MAX; ++mapperIndex)
	{
		//TODO: replace parser text with descriptive alternative.
		bank.PushGroup(parser_rage__Mapper_Strings[mapperIndex]);

		for(s32 inputIndex = 0; inputIndex < MAX_INPUTS; ++inputIndex)
		{
			if(ms_settings.m_InputMappers[inputIndex] == mapperIndex)
			{
				//TODO: replace parser text with descriptive alternative.
				const char *inputName = parser_rage__InputType_Data.m_Names[inputIndex];
				RagData& data = m_ragData[inputIndex];

				bank.AddTitle(inputName);
				bank.AddToggle("Force Disable:", &data.m_ForceDisabled);
				bank.AddText("Enabled:", &data.m_Enabled, true);
				bank.AddText("Value:", &data.m_InputValue, true);

				bank.AddSeparator();
			}
		}
		bank.PopGroup();
	}

#if LIGHT_EFFECTS_SUPPORT
	bank.AddToggle("Override Light Effect", &m_RagLightEffectEnabled, datCallback(MFA(CControl::UpdateRagLightEffect), this));
	bank.AddColor("Light Effect Color", &m_RagLightEffectColor, datCallback(MFA(CControl::UpdateRagLightEffect), this));
	bank.AddColor("Output Effect Color", &m_RagDebugLightDevice.m_Color);
	m_ActiveLightDevices.Push(&m_RagDebugLightDevice);
#endif // LIGHT_EFFECTS_SUPPORT

	bank.AddToggle("Toggle Run", &m_ToggleRunOn);
	bank.PopGroup();
}

void CControl::SetMappingsTest(s32 iIndex)
{
	SetInitialDefaultMappings(false);

	switch(iIndex)
	{
	case 0:
		break;

	case 1:
		break;

	case 2:
		break;

	default:
		Assertf(false, "Invalid index [%d] passed to CControl::SetMappingsTest", iIndex);
		break;
	}
}

#if __WIN32PC
CControl::JoystickDefinitionRagData::JoystickDefinitionRagData()
	: m_LastDeviceIndex(0)
	, m_AutoSave(true)
{
	m_Guid[0] = '\0';
	m_Status[0] = '\0';

	for(int i = 0; i < ioJoystick::MAX_STICKS; ++i)
	{
		m_Name[i][0] = '\0';
		m_NameComboHook[i] = m_Name[i];
	}

	for(int i = 0; i < NUM_PAD_SOURCES; ++i)
	{
		m_Parameters[i][0] = '\0';
		m_Sources[i][0] = '\0';
	}

	safecpy(m_SettingsPath, PC_GAMEPAD_DEFINITION_FILE, RAGE_MAX_PATH);
}

void CControl::AddKeyboardMouseUpdateButtonToRag(bkBank& bank, InputType input, const char* sourceName, bool isPrimary)
{
	const char*	typeStr		= NULL;
	Member1		func		= NULL;
	char*		inputString	= NULL;

	if(isPrimary)
	{
		typeStr		= "Primary";
		inputString	= m_ragData[input].m_KeyboardMousePrimary;
		func		= MFA1(CControl::UpdatePrimaryKeyMapping);
	}
	else
	{
		typeStr		= "Secondary";
		inputString	= m_ragData[input].m_KeyboardMouseSecondary;
		func		= MFA1(CControl::UpdateSecondaryKeyMapping);
	}

	char buffer[255];

	formatf(buffer, "%s Assigned:", typeStr);
	safecpy(inputString, sourceName, RagData::TEXT_SIZE);
	bank.AddText(buffer, inputString, RagData::TEXT_SIZE, true);

	formatf(buffer, "Update %s", typeStr);
	bank.AddButton( buffer, datCallback(func, this, (CallbackData)input) );
}

void CControl::AddJoystickUpdateButtonToRag(bkBank& bank, InputType input, const char* sourceName)
{
	safecpy(m_ragData[input].m_Joystick, sourceName, RagData::TEXT_SIZE);
	bank.AddText("Joystick Assigned:", m_ragData[input].m_Joystick, RagData::TEXT_SIZE, true);

	bank.AddButton( "Update Joystick", datCallback(MFA1(CControl::UpdateJoystickMapping), this, (CallbackData)input) );
}

void CControl::UpdatePrimaryKeyMapping( CallbackData data )
{
	// Unfortunately we need to cast to uptr before we can cast to InputType.
	m_currentMapping.m_Input = static_cast<InputType>((uptr)data);
	m_currentMapping.m_Type = PC_KEYBOARD_MOUSE;
	m_currentMapping.m_MappingIndex = 0;
	m_currentMapping.m_MappingCameFromRag = true; // Rag will automatically overwrite conflicts.
}

void CControl::UpdateSecondaryKeyMapping( CallbackData data )
{
	// Unfortunately we need to cast to uptr before we can cast to InputType.
	m_currentMapping.m_Input = static_cast<InputType>((uptr)data);
	m_currentMapping.m_Type = PC_KEYBOARD_MOUSE;
	m_currentMapping.m_MappingIndex = 1;
	m_currentMapping.m_MappingCameFromRag = true; // Rag will automatically overwrite conflicts.
}

void CControl::UpdateJoystickMapping( CallbackData data )
{
	// Unfortunately we need to cast to uptr before we can cast to InputType.
	m_currentMapping.m_Input = static_cast<InputType>((uptr)data);
	m_currentMapping.m_Type = JOYSTICK;
	m_currentMapping.m_MappingIndex = 0;
	m_currentMapping.m_MappingCameFromRag = true; // Rag will automatically overwrite conflicts.
}

void CControl::SwitchJoystickDefinitionDevice()
{
	if(controlVerifyf(m_CurrentJoystickDefinition.m_DeviceIndex < ioJoystick::GetStickCount(), "Invalid stick id"))
	{
		// ensure the stick is not an XInput device.
		if(CControlMgr::GetPad(m_CurrentJoystickDefinition.m_DeviceIndex).IsXBox360CompatiblePad())
		{
			m_CurrentJoystickDefinition.m_DeviceIndex = m_JoystickDefinitionRagData.m_LastDeviceIndex;
			safecpy(m_JoystickDefinitionRagData.m_Status, "Cannot update XInput device!", JoystickDefinitionRagData::TEXT_SIZE);
		}
		else if(!m_JoystickDefinitionRagData.m_AutoSave || SaveCurrentJoystickDefinition(m_JoystickDefinitionRagData.m_SettingsPath))
		{
			if(!m_JoystickDefinitionRagData.m_AutoSave)
			{
				safecpy(m_JoystickDefinitionRagData.m_Status, "Auto-saved succeeded!", JoystickDefinitionRagData::TEXT_SIZE);
			}

			LoadCurrentJoystickDefinition(m_JoystickDefinitionRagData.m_SettingsPath);
			m_JoystickDefinitionRagData.m_LastDeviceIndex = m_CurrentJoystickDefinition.m_DeviceIndex;

		}
		else
		{
			m_CurrentJoystickDefinition.m_DeviceIndex = m_JoystickDefinitionRagData.m_LastDeviceIndex;
			safecpy(m_JoystickDefinitionRagData.m_Status, "Save failed! Check read-only status.", JoystickDefinitionRagData::TEXT_SIZE);
		}
	}
	else
	{
		m_CurrentJoystickDefinition.m_DeviceIndex = m_JoystickDefinitionRagData.m_LastDeviceIndex;
		safecpy(m_JoystickDefinitionRagData.m_Status, "Invalid Stick id!", JoystickDefinitionRagData::TEXT_SIZE);
	}
}

void CControl::CalibrateJoystickDevice()
{
	m_CurrentJoystickDefinition.m_Calibrate = true;
}

void CControl::SaveJoystickDefinitionDevice()
{
	if(SaveCurrentJoystickDefinition(m_JoystickDefinitionRagData.m_SettingsPath) && ioJoystick::SaveUserCalibrations(true))
	{
		safecpy(m_JoystickDefinitionRagData.m_Status, "Successfully Saved", JoystickDefinitionRagData::TEXT_SIZE);
	}
}

void CControl::ScanJoystickDefinitionDevice( CallbackData data )
{
	GamepadDefinitionSource param = static_cast<GamepadDefinitionSource>((uptr)data);
	if(controlVerifyf(param >= 0 && param < NUM_PAD_SOURCES, "Invalid gamepad parameter!"))
	{
		m_CurrentJoystickDefinition.m_PadParameter = ConvertGamepadInputToParameter(param);
	}
}

void CControl::UpdateJoystickDefinitionRagData()
{
	safecpy(m_JoystickDefinitionRagData.m_Guid, m_CurrentJoystickDefinition.m_Guid.GetCStr(), ControlInput::Gamepad::GUID_SIZE);

	for(u32 i = 0; i < NUM_PAD_SOURCES; ++i)
	{
		m_JoystickDefinitionRagData.m_Parameters[i][0] = '\0';
		m_JoystickDefinitionRagData.m_Sources[i][0] = '\0';
	}

	m_JoystickDefinitionRagData.m_Status[0] = '\0';

	for(u32 i = 0; i < m_CurrentJoystickDefinition.m_Data.m_Definitions.size(); ++i)
	{
		ControlInput::Gamepad::Source& definition = m_CurrentJoystickDefinition.m_Data.m_Definitions[i];
		GamepadDefinitionSource param			  = ConvertParameterToGampadInput(definition.m_PadParameter);
		
		if(param != INVALID_PAD_SOURCE)
		{
			safecpy( m_JoystickDefinitionRagData.m_Parameters[param],
					 GetParameterText(definition.m_JoystickParameter),
					 JoystickDefinitionRagData::TEXT_SIZE );

			safecpy( m_JoystickDefinitionRagData.m_Sources[param],
					 GetSourceText(definition.m_JoystickSource),
					 JoystickDefinitionRagData::TEXT_SIZE );
		}
	}
}

// Create mapping widgets in rag.
void CControl::InitMappingWidgets(bkBank& bank)
{
	char buffer[255];
	buffer[0] = '\0';

	bank.PushGroup("PC Mappings");

	bank.AddToggle("Toggle Aim",                  &m_ToggleAim);

	bank.AddText("Current Dynamic Control Scheme:", m_RagLoadedPCControlScheme, RagData::TEXT_SIZE, true);
	bank.AddText("Dynamic Control Scheme:",         m_RagPCControlScheme, RagData::TEXT_SIZE);
	bank.AddButton("Load Dynamic Control Scheme",   datCallback(MFA(CControl::RagLoadPCMappings), this));

	bank.PushGroup("Mouse");
	bank.AddToggle("Auto-centered Steering for Cars",	&m_MouseVehicleCarAutoCenter,	datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddToggle("Auto-centered Steering for Flying",	&m_MouseVehicleFlyAutoCenter,	datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddToggle("Auto-centered Steering for Subs",	&m_MouseVehicleSubAutoCenter,	datCallback(MFA(CControl::ReloadMappings), this));

	bank.AddSlider("On Foot Min Sensitivity", &ms_settings.m_MouseSettings.m_OnFootMinMouseSensitivity, 0.0001f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddSlider("On Foot Max Sensitivity", &ms_settings.m_MouseSettings.m_OnFootMaxMouseSensitivity, 1.0f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddSlider("On Foot Power Curve", &ms_settings.m_MouseSettings.m_OnFootMouseSensitivityPower, 0.0001f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddSlider("In Vehicle Min Sensitivity", &ms_settings.m_MouseSettings.m_InVehicleMinMouseSensitivity, 0.0001f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddSlider("In Vehicle Max Sensitivity", &ms_settings.m_MouseSettings.m_InVehicleMaxMouseSensitivity, 0.0001f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.AddSlider("In Vehicle Power Curve", &ms_settings.m_MouseSettings.m_InVehicleMouseSensitivityPower, 0.0001f, 100.0f, 0.1f, datCallback(MFA(CControl::ReloadMappings), this));
	bank.PopGroup();

	for(u32 mappingListIndex = 0; mappingListIndex < ms_settings.m_MappingSettings.m_MappingList.size(); ++mappingListIndex)
	{
		// The current mapping list (e.g. page/section of controls such as On Foot or Melee).
		const ControlInput::MappingList& mappingList = ms_settings.m_MappingSettings.m_MappingList[mappingListIndex];
		bank.PushGroup(mappingList.m_Name.GetCStr());

		// Loop through all categories in the mapping list.
		for (u32 catagoryIndex = 0; catagoryIndex < mappingList.m_Categories.size(); ++catagoryIndex)
		{
			const ControlInput::InputList* list = ms_settings.m_MappingSettings.m_Categories.Access(mappingList.m_Categories[catagoryIndex]);
			if( controlVerifyf(list != NULL, 
				"Invalid category '%s' (%d) whilst showing bank mappings widgets",
				mappingList.m_Categories[catagoryIndex].TryGetCStr(),
				mappingList.m_Categories[catagoryIndex].GetHash()) )
			{
				const atArray<InputType>& inputs = list->m_Inputs;

				// Loop through all the inputs in the category.
				for(u32 inputIndex = 0; inputIndex < inputs.size(); ++inputIndex)
				{
					InputType input = inputs[inputIndex];
					Mapper mapperId = ms_settings.m_InputMappers[input];
					
					bank.AddTitle(GetInputName(input));

					if( mapperId != DEPRECATED_MAPPER &&
						controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "Invalid mapper assigned to %s!", GetInputName(input)) )
					{
						ioMapper::ioSourceList sources;
						const ioMapper& mapper = m_Mappers[ms_settings.m_InputMappers[input]];
						mapper.GetMappedSources(m_inputs[input], sources);// Get all mapping to the input.

						// Each input can have a primary and a secondary mapping.
						u32 mappingCount = 0;
						// indicates if the joystick mapping has been added.
						bool joystickAdded = false;

						char buffer[RagData::TEXT_SIZE];

						// only show two mappings for an input
						for(s32 sourceIndex = 0; sourceIndex < sources.size() && mappingCount < 2; ++sourceIndex)
						{
							const ioSource& source = sources[sourceIndex];

							MappingType type = GetMappingType(source.m_Device);

							// only add the first two pc keyboard/mouse mappings.
							if(mappingCount < 2 && type == PC_KEYBOARD_MOUSE)
							{
								GetMappingParameterName(source, buffer, RagData::TEXT_SIZE);
								AddKeyboardMouseUpdateButtonToRag(bank, input, buffer, (mappingCount == 0));
								++mappingCount;
							}
							// add joystick text
							else if(!joystickAdded && type == JOYSTICK)
							{
								GetMappingParameterName(source, buffer, RagData::TEXT_SIZE);
								AddJoystickUpdateButtonToRag(bank, input, buffer);
								joystickAdded = true;
							}
						}

						// if there are no primary mappings then add a button to map it
						if(mappingCount == 0)
						{
							AddKeyboardMouseUpdateButtonToRag(bank, input, "<NOT ASSIGNED>", true);
						}

						// if there are no secondary mappings then add a button to map it
						if(mappingCount < 2)
						{
							AddKeyboardMouseUpdateButtonToRag(bank, input, "<NOT ASSIGNED>", false);
						}

						// if there are no mapping for joystick
						if(joystickAdded == false)
						{
							AddJoystickUpdateButtonToRag(bank, input, "<NOT ASSIGNED>");

						}
					}

					bank.AddSeparator();
				}
			}
		}
		bank.PopGroup();
	}

	// add save load buttons.
	bank.AddButton("Save All Control Changes", datCallback(MFA(CControl::SaveAllUserControls), this));
	bank.AddButton("Validate Keyboard & Mouse Controls", datCallback(MFA(CControl::GetNumberOfMappingConflicts), this));

	bank.PopGroup();
}

void CControl::InitJoystickCalibrationWidgets( bkBank &bank )
{
	USES_CONVERSION;

	if(ioJoystick::GetStickCount() > 0)
	{
		char16 buffer[100];

		bool indexSet = false;

		for(u8 i = 0; i < ioJoystick::GetStickCount(); ++i)
		{
			const CPad& pad = CControlMgr::GetPad(i);
			
			// Do not add XInput devices.
			if(!pad.IsXBox360CompatiblePad())
			{
				buffer[0] = '\0';
				const ioJoystick& stick = ioJoystick::GetStick(i);
				stick.GetProductName(buffer, 100);
				safecpy(m_JoystickDefinitionRagData.m_Name[i], W2A(buffer), JoystickDefinitionRagData::NAME_LENGTH);

				if(!indexSet)
				{
					indexSet = true;
					m_CurrentJoystickDefinition.m_DeviceIndex = i;
					m_JoystickDefinitionRagData.m_LastDeviceIndex = i;
				}
			}
			else
			{
				buffer[0] = '\0';
				safecpy(m_JoystickDefinitionRagData.m_Name[i], "--XInput Device--", JoystickDefinitionRagData::NAME_LENGTH);
			}
		}

		// only add if there is a valid DirectInput device.
		if(indexSet)
		{
			// Add Pad Definitions.
			bank.PushGroup("Pad Definitions");
			bank.AddText("Status:", m_JoystickDefinitionRagData.m_Status, JoystickDefinitionRagData::TEXT_SIZE, true);
			bank.AddText("Settings Path:", m_JoystickDefinitionRagData.m_SettingsPath, RAGE_MAX_PATH);
			bank.AddToggle("Autosave:", &m_JoystickDefinitionRagData.m_AutoSave);
			bank.AddButton("Save Settings", datCallback(MFA(CControl::SaveJoystickDefinitionDevice), this));
			bank.AddButton("Reload Controls", datCallback(MFA(CControl::ReloadMappings), this));
			bank.AddButton("Calibrate Device", datCallback(MFA(CControl::CalibrateJoystickDevice), this));
			bank.AddSeparator();

			bank.AddCombo( "Device:",
				&m_CurrentJoystickDefinition.m_DeviceIndex,
				ioJoystick::GetStickCount(),
				&m_JoystickDefinitionRagData.m_NameComboHook[0],
				datCallback(MFA(CControl::SwitchJoystickDefinitionDevice), this) );
			
			bank.AddText("GUID:", m_JoystickDefinitionRagData.m_Guid, ControlInput::Gamepad::GUID_SIZE, true);
			bank.AddSeparator();

			for(int i = 0; i < NUM_PAD_SOURCES; ++i)
			{
				ioMapperParameter param = ConvertGamepadInputToParameter(static_cast<GamepadDefinitionSource>(i));
				bank.AddTitle(GetParameterText(param));
				bank.AddText("Parameter:", m_JoystickDefinitionRagData.m_Parameters[i], JoystickDefinitionRagData::TEXT_SIZE, true);
				bank.AddText("Source:", m_JoystickDefinitionRagData.m_Sources[i], JoystickDefinitionRagData::TEXT_SIZE, true);
				
				if(!JoystickDefinitionData::IsAutoMapped(param))
				{
					bank.AddButton("Scan Device", datCallback(MFA1(CControl::ScanJoystickDefinitionDevice), this, (CallbackData)i));
				}
				bank.AddSeparator();
			}

			bank.PopGroup();
			LoadCurrentJoystickDefinition(m_JoystickDefinitionRagData.m_SettingsPath);
		}
	}
}

void CControl::ReloadMappings()
{
	if(!m_JoystickDefinitionRagData.m_AutoSave || SaveCurrentJoystickDefinition(m_JoystickDefinitionRagData.m_SettingsPath))
	{
		SetInitialDefaultMappings(true);
		LoadDeviceControls(false);
		ResetKeyboardMouseSettings();
		safecpy(m_JoystickDefinitionRagData.m_Status, "Controls successfully loaded!", JoystickDefinitionRagData::TEXT_SIZE);

	}
	else
	{
		safecpy(m_JoystickDefinitionRagData.m_Status, "Auto-save Failed, controls not loaded!", JoystickDefinitionRagData::TEXT_SIZE);
	}
}

void CControl::SaveAllUserControls()
{
	SaveUserControls();
	for(s32 i = 0; i < ioJoystick::GetStickCount(); ++i)
	{
		if(!CControlMgr::GetPad(i).IsXBox360CompatiblePad())
		{
			SaveUserControls(false, i);
		}
	}
}

#endif // __WIN32PC

#endif // __BANK


#if KEYBOARD_MOUSE_SUPPORT
void CControl::AddKeyboardMouseExtraSettings(const ControlInput::ControlSettings &settings)
{
	ioMapper::ioSourceList sources;
	for(u32 inputIndex = 0; inputIndex < settings.m_Mappings.size(); ++inputIndex)
	{
		const InputType input = settings.m_Mappings[inputIndex].m_Input;
		GetSpecificInputSources(input, sources, ioSource::IOMD_KEYBOARD_MOUSE);

		s32 needed = NUM_MAPPING_SCREEN_SLOTS - sources.size();
		for(s32 slotIndex = 0; slotIndex < needed; ++slotIndex)
		{
			const static ioSource NULL_MAPPING = ioSource(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
			AddMapping(input, NULL_MAPPING);
		}

		sources.Reset();
	}

	LoadSettings(settings, ioSource::IOMD_KEYBOARD_MOUSE);
}

void CControl::GetUserSettingsPath(char (&path)[PATH_BUFFER_SIZE], const char* const file)
{
	CompileTimeAssert(PATH_BUFFER_SIZE == rgsc::RGSC_MAX_PATH);

	if(IsUserSignedIn())
	{
		if(controlVerifyf(SUCCEEDED(g_rlPc.GetFileSystem()->GetTitleProfileDirectory(path, true)), "Failed to to get user directory!"))
		{
			safecat(path, PC_USER_SETTINGS_DIR);
			safecat(path, file);
			return;
		}
		else
		{
			controlErrorf("Failed to to get user directory!");
		}
	}

	// Fallback to default path!
	formatf(path, "user:/%s%s", PC_USER_SETTINGS_DIR, file);
}

bool CControl::IsUserSignedIn()
{
	return g_rlPc.IsInitialized() && g_rlPc.GetProfileManager() && g_rlPc.GetProfileManager()->IsSignedIn();
}

bool CControl::UpdateMappingFromInputScan()
{
	bool updated = false;
	if(m_currentMapping.m_Input != UNDEFINED_INPUT)
	{
		// scan input for a new source.
		ioSource source = m_currentMapping.m_Source;

		if( source.m_DeviceIndex != ioSource::IOMD_UNKNOWN ||				// if we've already mapped it, skip the scan (since we'll wipe out our saved conflict)
			GetMappingType(source.m_Device) == m_currentMapping.m_Type ||
			(m_currentMapping.m_ScanMapper != NULL &&
			m_currentMapping.m_ScanMapper->Scan(source, ioMapper::DEFAULT_DEVICE_SCAN_OPTIONS) &&
			GetMappingType(source.m_Device) == m_currentMapping.m_Type) )
		{
			controlDebugf1( "Update key mapping for %s to %s.",
							GetInputName(m_currentMapping.m_Input),
							GetParameterText(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter)) );

			// NOTE: GetConflictingInputData() is slow.
			if(!GetConflictingInputData(m_currentMapping.m_Input, source, true, m_currentMapping.m_Conflict, false))
			{
				UpdateMapping(m_currentMapping.m_Input, m_currentMapping.m_Type, source, m_currentMapping.m_MappingIndex, true);
				updated = true;
				controlDebugf1("Key mapping update successful.");
			}
			else if (m_currentMapping.m_Conflict.m_Conflicts.size() == 1 && m_currentMapping.m_Conflict.m_Conflicts[0].m_Input == m_currentMapping.m_Input)
			{
				if(m_currentMapping.m_Conflict.m_Conflicts[0].m_MappingIndex >= NUM_MAPPING_SCREEN_SLOTS)
				{
					CancelCurrentMapping();
				}
				else if(m_currentMapping.m_MappingIndex != m_currentMapping.m_Conflict.m_Conflicts[0].m_MappingIndex)
				{
					controlDebugf1( "Moving %s for %s from slot %d to %d.",
						GetParameterText(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter)),
						GetInputName(m_currentMapping.m_Input),
						m_currentMapping.m_Conflict.m_Conflicts[0].m_MappingIndex,
						m_currentMapping.m_MappingIndex );

					// Add mapping to new slot.
					UpdateMapping(m_currentMapping.m_Input, m_currentMapping.m_Type, source, m_currentMapping.m_MappingIndex, true);

					// Remove mapping from previous slot.
					const ioSource unmapped(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
					UpdateMapping( m_currentMapping.m_Conflict.m_Conflicts[0].m_Input,
						m_currentMapping.m_Type,
						unmapped,
						m_currentMapping.m_Conflict.m_Conflicts[0].m_MappingIndex,
						true );
				}
				else
				{
					controlDebugf1( "%s is already mapped to %s in slot %d, ignoring.",
						GetParameterText(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter)),
						GetInputName(m_currentMapping.m_Input),
						m_currentMapping.m_MappingIndex );
				}

				m_currentMapping.m_Conflict = MappingConflictData();
				updated = true;
			}
			else
			{
				m_currentMapping.m_Source = source;

				// We cache everything needed to update the mapping so the user has the option to replace the previous mapping.
				updated = false;
				controlDebugf1( "Key mapping failed! %s is already mapped to %s and %d more, waiting for user response.",
								GetParameterText(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter)),
								GetInputName(m_currentMapping.m_Conflict.m_Conflicts[0].m_Input),
								m_currentMapping.m_Conflict.m_NumConflictingInputs - 1 );
			}
		}
	}
	return updated;
}

void CControl::UpdateIdenticalMappings( const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber )
{
	// Identical mappings are for keyboard/mouse only!
	if(type == PC_KEYBOARD_MOUSE)
	{
		// Check all input lists.
		for (u32 listIdx = 0; listIdx < ms_settings.m_MappingSettings.m_IdenticalMappingLists.size(); ++listIdx)
		{
			const ControlInput::InputList& inputList = ms_settings.m_MappingSettings.m_IdenticalMappingLists[listIdx];

			// See if the input is in this list, if it is, then all other inputs in this list need to be updated.
			for(u32 inputIdx = 0; inputIdx < inputList.m_Inputs.size(); ++inputIdx)
			{
				if(inputList.m_Inputs[inputIdx] == input)
				{
					UpdateIdenticalMappingsInList(input, inputList, type, source, mappingNumber);
				}
			}
		}
	}
}

void CControl::UpdateIdenticalMappingsInList( const InputType input,
											 const ControlInput::InputList& inputList,
											 const MappingType type,
											 const ioSource& source,
											 u32 mappingNumber )
{
	for(u32 i = 0; i < inputList.m_Inputs.size(); ++i)
	{
		if(inputList.m_Inputs[i] != input)
		{
			UpdateMapping(inputList.m_Inputs[i], type, source, mappingNumber, false);
		}
	}
}

bool CControl::GetConflictingInputData(const InputType input, const ioSource& source, bool allowConfictWithSelf, MappingConflictData& conflictData, bool assertOnConflict) const
{
	// For sanity, clear previous conflicting data.
	conflictData.m_Conflicts.Reset();
	conflictData.m_NumConflictingInputs = 0u;

	const u32 catagory = ms_settings.m_InputCategories[input];

	// Check each conflict list and see if the inputs catagory is in it, if it is then we need to check all inputs on the
	// list to see if any of them are already mapped to this source.
	for(u32 listIdx = 0; listIdx < ms_settings.m_MappingSettings.m_ConflictList.size(); ++listIdx)
	{
		const ControlInput::ConflictList& conflictList = ms_settings.m_MappingSettings.m_ConflictList[listIdx];

		// Loop through all categories in the mapping list.
		for (u32 catIdx = 0;  catIdx < conflictList.m_Categories.size(); ++catIdx)
		{
			// If the input's catagory is in the conflict list then we need to check for conflicts on all inputs in all
			// the categories.  If there is a conflict then the conflict mapping data has been saved inside m_currentMapping.
			if(conflictList.m_Categories[catIdx].GetHash() == catagory)
			{
				GetConflictingInputData(input, conflictList, source, allowConfictWithSelf, conflictData, assertOnConflict);
			}
		}
	}

	return conflictData.m_Conflicts.size() > 0;
}

bool CControl::GetConflictingInputData( const InputType input,
									    const ControlInput::ConflictList& conflictList,
										const ioSource& source,
										bool allowConfictWithSelf,
										MappingConflictData& conflictData,
										bool ASSERT_ONLY(assertOnConflict)) const
{
	const MappingType type = GetMappingType(source.m_Device);

	controlDebugf2("Testing for mapping conflict for '%s'.", GetInputName(input));

	// The keyboard key 'KEY_NULL' represents unbound and never conflicts!
	if(type == PC_KEYBOARD_MOUSE && source.m_Parameter != KEY_NULL)
	{
		for (u32 catIdx = 0; catIdx < conflictList.m_Categories.size(); ++catIdx)
		{
			controlDebugf2("Testing for mapping conflict for '%s' in '%s' category.", GetInputName(input), conflictList.m_Categories[catIdx].GetCStr());
			const ControlInput::InputList* list = ms_settings.m_MappingSettings.m_Categories.Access(conflictList.m_Categories[catIdx]);

			if( controlVerifyf(list != NULL, 
				"Invalid category '%s' (%d) whilst checking mapping conflict!",
				conflictList.m_Categories[catIdx].TryGetCStr(),
				conflictList.m_Categories[catIdx].GetHash()) )
			{
				const ControlInput::InputList& inputList = *list;

				for(u32 inputIdx = 0; inputIdx < inputList.m_Inputs.size(); ++inputIdx)
				{
					const InputType testInput  = inputList.m_Inputs[inputIdx];
					const ioMapper& mapper     = m_Mappers[ms_settings.m_InputMappers[testInput]];

					controlDebugf2("Testing for mapping conflict for '%s' with '%s'.", GetInputName(input), GetInputName(testInput));

					ioMapper::ioSourceList sources;
					mapper.GetMappedSources(m_inputs[testInput], sources);

					u8 mappingIndex = 0;
					for(u32 sourceIdx = 0; sourceIdx < sources.size(); ++sourceIdx)
					{
						const ioSource& testSource = sources[sourceIdx];

						// If there is a conflict and no exceptions.
						if( (testInput != input || allowConfictWithSelf) && testSource.m_Device == source.m_Device && testSource.m_Parameter == source.m_Parameter &&
							testSource.m_DeviceIndex == source.m_DeviceIndex && HasConflictException(input, testInput) == false )
						{
							controlWarningf("'%s' conflicts with '%s'.", GetInputName(input), GetInputName(testInput));

							s32 inputIndex = GetConflictIndex(conflictData.m_Conflicts, testInput);
							if(inputIndex == -1 || conflictData.m_Conflicts[inputIndex].m_MappingIndex != mappingIndex)
							{
								controlAssertf(!assertOnConflict, "'%s' conflicts with '%s' and its a new conflict.", GetInputName(input), GetInputName(testInput));
								controlWarningf("Mapping Conflict! %s is mapped to %s and %s!",
									GetParameterText(ioMapper::ConvertParameterToMapperValue(testSource.m_Device, testSource.m_Parameter)),
									GetInputName(input),
									GetInputName(testInput) );

								conflictData.m_IsNewConflict = true;
							}

							// If there are no conflicts yet add the input to the list.
							if(conflictData.m_Conflicts.size() == 0)
							{
								conflictData.m_Conflicts.Push(MappingConflict(testInput, mappingIndex));
								++conflictData.m_NumConflictingInputs;
							}
							// If the input is not already in the list of conflicts.
							else if(inputIndex == -1)
							{
								// If the input is not mappable add the input to the front. NOTE that a mapping in a slot >= NUM_MAPPING_SCREEN_SLOTS
								// is considered not mappable!
								if(!IsInputMappable(testInput) || mappingIndex >= NUM_MAPPING_SCREEN_SLOTS)
								{
									// Make space if needed.
									if(conflictData.m_Conflicts.size() == conflictData.m_Conflicts.max_size())
									{
										conflictData.m_Conflicts.Pop();
									}
									conflictData.m_Conflicts.Insert(0) = MappingConflict(testInput, mappingIndex);
									++conflictData.m_NumConflictingInputs;
								}
								// If there is space in the array and the conflict.
								else if(conflictData.m_Conflicts.size() < conflictData.m_Conflicts.max_size())
								{
									conflictData.m_Conflicts.Push(MappingConflict(testInput, mappingIndex));
									++conflictData.m_NumConflictingInputs;
								}
							}
						}
						else if(GetMappingType(testSource.m_Device) == type)
						{
							++mappingIndex;
						}
					}
				}
			}
		}
	}

	return conflictData.m_Conflicts.size() > 0;
}

s32 CControl::GetConflictIndex(const MappingConflictList& conflicts, InputType input)
{
	for(s32 i = 0; i < conflicts.size(); ++i)
	{
		if(conflicts[i].m_Input == input)
		{
			return i;
		}
	}

	return -1;
}

bool CControl::HasConflictException(const InputType input1, const InputType input2) const
{
	controlDebugf2("Testing for conflict exception for '%s' and '%s'.", GetInputName(input1), GetInputName(input2));

	// Testing if an input conflicts with itself can have undefined results based on the XML. Always consider as a conflict!
	// Higher up code handles this case.
	if(input1 == input2)
	{
		controlDebugf2("Two inputs are identical, an input always conflicts with itself!");
		return false;
	}

	for(u32 i = 0; i < ms_settings.m_MappingSettings.m_ConflictExceptions.size(); ++i)
	{
		const ControlInput::InputList& exceptions = ms_settings.m_MappingSettings.m_ConflictExceptions[i];

		bool input1InList = false;
		for(u32 inputIndex = 0; inputIndex < exceptions.m_Inputs.size(); ++inputIndex)
		{
			if(exceptions.m_Inputs[inputIndex] == input1)
			{
				input1InList = true;
				break;
			}
		}

		if(input1InList)
		{
			for(u32 inputIndex = 0; inputIndex < exceptions.m_Inputs.size(); ++inputIndex)
			{
				if(exceptions.m_Inputs[inputIndex] == input2)
				{
					controlDebugf1("'%s' has a conflict exception for '%s'.", GetInputName(input1), GetInputName(input2));
					return true;
				}
			}
		}
	}

	// NOTE: If two inputs are identical mappings they implicitly have a conflict exception.
	for(u32 i = 0; i <  ms_settings.m_MappingSettings.m_IdenticalMappingLists.size(); ++i)
	{
		const ControlInput::InputList& exceptions = ms_settings.m_MappingSettings.m_IdenticalMappingLists[i];

		bool input1InList = false;
		for(u32 inputIndex = 0; inputIndex < exceptions.m_Inputs.size(); ++inputIndex)
		{
			if(exceptions.m_Inputs[inputIndex] == input1)
			{
				input1InList = true;
				break;
			}
		}

		if(input1InList)
		{
			for(u32 inputIndex = 0; inputIndex < exceptions.m_Inputs.size(); ++inputIndex)
			{
				if(exceptions.m_Inputs[inputIndex] == input2)
				{
					controlDebugf2("'%s' is and identical mapping for '%s' so cannot conflict.", GetInputName(input1), GetInputName(input2));
					return true;
				}
			}
		}
	}

	controlDebugf2("No conflict exception found for '%s' and '%s'.", GetInputName(input1), GetInputName(input2));
	return false;
}

void CControl::UpdateLastFrameDeviceIndex()
{
	//Update using keyboard
	if (NetworkInterface::IsGameInProgress())
	{
		StatsInterface::UpdateUsingKeyboard(WasKeyboardMouseLastKnownSource(), m_LastFrameDeviceIndex, m_LastKnownSource.m_DeviceIndex);
	}

	m_LastFrameDeviceIndex = m_LastKnownSource.m_DeviceIndex;
}

void CControl::ResetKeyboardMouseSettings()
{
	float sensitivity = CalculateMouseSensitivity( (static_cast<float>(CPauseMenu::GetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE)) / MAX_MOUSE_SCALE),
		ms_settings.m_MouseSettings.m_OnFootMinMouseSensitivity,
		ms_settings.m_MouseSettings.m_OnFootMaxMouseSensitivity,
		ms_settings.m_MouseSettings.m_OnFootMouseSensitivityPower );
	m_Mappers[ON_FOOT].SetMouseScale(sensitivity);
	DEBUG_DRAW_ONLY(m_OnFootMouseSensitivity = sensitivity);

	eMenuPref eVehicleMouseScale = PREF_MOUSE_DRIVING_SCALE;
	const CPed* pPed = CGameWorld::FindLocalPlayerSafe();
	if(pPed && pPed->GetVehiclePedInside())
	{
		switch(pPed->GetVehiclePedInside()->GetVehicleType())
		{
			case VEHICLE_TYPE_PLANE:
			{
				eVehicleMouseScale = PREF_MOUSE_PLANE_SCALE;
				break;
			}
			case VEHICLE_TYPE_HELI: // fall through
			case VEHICLE_TYPE_BLIMP: // fall though
			case VEHICLE_TYPE_AUTOGYRO:
			{
				eVehicleMouseScale = PREF_MOUSE_HELI_SCALE;
				break;
			}
			case VEHICLE_TYPE_SUBMARINE:
			{
				eVehicleMouseScale = PREF_MOUSE_SUB_SCALE;
				break;
			}
			default:
			{
				eVehicleMouseScale = PREF_MOUSE_DRIVING_SCALE;
				break;
			}
		}
	}
	
	sensitivity = CalculateMouseSensitivity( (static_cast<float>(CPauseMenu::GetMenuPreference(eVehicleMouseScale)) / MAX_MOUSE_SCALE),
		ms_settings.m_MouseSettings.m_InVehicleMinMouseSensitivity,
		ms_settings.m_MouseSettings.m_InVehicleMaxMouseSensitivity,
		ms_settings.m_MouseSettings.m_InVehicleMouseSensitivityPower );
	m_Mappers[IN_VEHICLE].SetMouseScale(sensitivity);
	DEBUG_DRAW_ONLY(m_InVehicleMouseSensitivity = sensitivity);
}

bool CControl::AreConflictingInputsMappable() const
{
	if(HasMappingConflict())
	{
		const MappingConflictList& conflicts = GetMappingConflictList();
		for(u32 i = 0; i < conflicts.size(); ++i)
		{
			//  NOTE that a mapping in a slot >= NUM_MAPPING_SCREEN_SLOTSis considered not mappable!
			if(!IsInputMappable(conflicts[i].m_Input) || conflicts[i].m_MappingIndex >= NUM_MAPPING_SCREEN_SLOTS)
			{
				return false;
			}
		}
	}

	return true;
}

#if DEBUG_DRAW
void CControl::DebugDrawMouseInput()
{
	if(CControlMgr::IsDebugPadValuesOn() && grcDebugDraw::GetIsInitialized() && CLoadingScreens::AreActive() == false)
	{
		Vector2 pos(0.3f, 0.8f);
		const float dy = grcDebugDraw::GetScreenSpaceTextHeight() / static_cast<float>(grcDevice::GetHeight());
		char buffer[128] = {0};

		// Relative Movement
		formatf( buffer,
				 "Mouse Relative: (%d, %d)",
				 ioMouse::GetDX(),
				 ioMouse::GetDY() );
		grcDebugDraw::Text(pos, Color32(255,0,0), buffer);
		pos.y += dy;

		// Scaled Relative (On Foot) Movement
		formatf( buffer,
				 "Mouse Relative (On Foot): (%.3f, %.3f)",
				 static_cast<float>(ioMouse::GetDX()) * m_OnFootMouseSensitivity,
				 static_cast<float>(ioMouse::GetDY()) * m_OnFootMouseSensitivity);
		grcDebugDraw::Text(pos, Color32(255,0,0), buffer);
		pos.y += dy;


		// Scaled Relative (On Foot) Movement
		formatf( buffer,
				 "Mouse Relative (On Foot): (%.3f, %.3f)",
				 static_cast<float>(ioMouse::GetDX()) * m_InVehicleMouseSensitivity,
				 static_cast<float>(ioMouse::GetDY()) * m_InVehicleMouseSensitivity);
		grcDebugDraw::Text(pos, Color32(255,0,0), buffer);
		pos.y += dy;


		// Absolute Position
		formatf( buffer,
			"Mouse Absolute: (%.3f, %.3f)",
			ioMouse::GetNormX(),
			ioMouse::GetNormY() );
		grcDebugDraw::Text(pos, Color32(255,0,0), buffer);
		pos.y += dy;

		

		struct sDebugMouseEvent
		{
			Vector2 vNormPos;
			u32 uTime;
			unsigned mouseButton;
			bool bIsPress;

			void Draw(float fPercentComplete, bool bSolid)
			{
				static BankFloat MIN_SCREEN_SIZE = 0.010f;
				static BankFloat MAX_SCREEN_SIZE = 0.030f;

				Color32 drawColor(0x66666666);
				drawColor.SetAlpha( Min(255,int(196.0*rage::dSlowOut(fPercentComplete) + 40.0)));
				if( mouseButton & ioMouse::MOUSE_LEFT )   drawColor.SetGreen(255);
				if( mouseButton & ioMouse::MOUSE_MIDDLE ) drawColor.SetBlue(255);
				if( mouseButton & ioMouse::MOUSE_RIGHT )  drawColor.SetRed(255);

				grcDebugDraw::Circle(vNormPos, Lerp(fPercentComplete, MIN_SCREEN_SIZE, MAX_SCREEN_SIZE), drawColor, bSolid);
			}
		};

		Vector2 vNormPos(ioMouse::GetX() / static_cast<float>(grcDevice::GetWidth()), ioMouse::GetY() / static_cast<float>(grcDevice::GetHeight()));
		
		// draw shadow of cursor first, so that mouse states sit on top of it
		grcDebugDraw::Cross(vNormPos+Vector2(1/ static_cast<float>(grcDevice::GetWidth()), 1 / static_cast<float>(grcDevice::GetHeight())), 0.01f, Color_black);

		static atArray<sDebugMouseEvent> s_MouseEvents;
		u32 now = fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
		

		if( unsigned uDownBtns = ioMouse::GetButtons() )
		{	
			sDebugMouseEvent newEvent;
			newEvent.vNormPos = vNormPos;
			newEvent.mouseButton = uDownBtns & (ioMouse::MOUSE_LEFT | ioMouse::MOUSE_RIGHT | ioMouse::MOUSE_MIDDLE); // only the three buttons will do for now
			// just show current live state now
			newEvent.Draw(0.0f, true);	
		}

		if( unsigned uPressedBtns = ioMouse::GetPressedButtons() )
		{
			sDebugMouseEvent newEvent;
			newEvent.uTime = now;
			newEvent.vNormPos = vNormPos;
			newEvent.mouseButton = uPressedBtns & (ioMouse::MOUSE_LEFT | ioMouse::MOUSE_RIGHT | ioMouse::MOUSE_MIDDLE); // only the three buttons will do for now
			newEvent.bIsPress = true;
			s_MouseEvents.PushAndGrow(newEvent);
		}

		if( unsigned uReleasedBtns = ioMouse::GetReleasedButtons() )
		{
			sDebugMouseEvent newEvent;
			newEvent.uTime = now;
			newEvent.vNormPos = vNormPos;
			newEvent.mouseButton = uReleasedBtns & (ioMouse::MOUSE_LEFT | ioMouse::MOUSE_RIGHT | ioMouse::MOUSE_MIDDLE); // only the three buttons will do for now
			newEvent.bIsPress = false;
			s_MouseEvents.PushAndGrow(newEvent);
		}
		
		static BankFloat PRESS_LIFETIME = 125.0f;
		static BankFloat RELEASE_LIFETIME = 250.0f;

		// work from the back forward so if we delete we don't adjust the rest of the stack	
		for( int i=s_MouseEvents.GetCount()-1; i >= 0; i-- )
		{
			sDebugMouseEvent& curEvent = s_MouseEvents[i];
			float fPercentComplete = float(now-curEvent.uTime)/(curEvent.bIsPress ? PRESS_LIFETIME : RELEASE_LIFETIME);
			if( fPercentComplete > 1.0 )
			{
				s_MouseEvents.Delete(i);
				continue;
			}

			// press events shrink towards the min size at twice the normal rate
			curEvent.Draw( curEvent.bIsPress ? (1.0f-fPercentComplete) : fPercentComplete, false);
		}


		// draw the current mouse position last
		grcDebugDraw::Cross(vNormPos, 0.01f, Color_HotPink);



	}
}
#endif // DEBUG_DRAW

float CControl::CalculateMouseSensitivity(float percentage, float min, float max, float powArc)
{
	if(controlVerifyf(powArc > 0.0f, "Invalid power to scale (%f), setting to 1.0f (linear)!", powArc) == false)
	{
		powArc = 1.0f;
	}
	if(controlVerifyf(min < max, "min (%f, is greater than max %f, swapping!", min, max) ==  false)
	{
		float tmp = min;
		min = max;
		max = tmp;
	}

	controlAssertf(percentage >= 0.0f && percentage <= 1.0f, "Invalid percentage %f, clamping!", percentage);
	Clamp(percentage, 0.0f, 1.0f);

	const float range = max - min;

	return (powf(percentage, powArc) * range) + min;
}

void CControl::GetMappingCategories(MappingCategoriesList& categories)
{
	categories.Reserve(ms_settings.m_MappingSettings.m_MappingList.size());
	for(u32 i = 0; i < ms_settings.m_MappingSettings.m_MappingList.size(); ++i)
	{
		categories.PushAndGrow(ms_settings.m_MappingSettings.m_MappingList[i].m_Name);
	}
}

void CControl::GetCategoryInputs(const atHashString& category, InputList& categoryInputs)
{
	// Find the catagory.
	for(u32 categoryIndex = 0; categoryIndex < ms_settings.m_MappingSettings.m_MappingList.size(); ++categoryIndex)
	{
		if(category == ms_settings.m_MappingSettings.m_MappingList[categoryIndex].m_Name)
		{
			const ControlInput::MappingList& mappingList = ms_settings.m_MappingSettings.m_MappingList[categoryIndex];
			
			// First find out how many inputs there are so we can reserve the memory.
			u32 numInputs = 0u;

			// NOTE: in the mapping files the 'sections' are called categories but from the point of view of the mapping screen,
			// categories are things like 'On Foot'.
			for(u32 sectionIndex = 0; sectionIndex < mappingList.m_Categories.size(); ++sectionIndex)
			{
				const ControlInput::InputList* inputList = ms_settings.m_MappingSettings.m_Categories.Access(mappingList.m_Categories[sectionIndex]);
				if(controlVerifyf(inputList != NULL, "Invalid category '%s'!", mappingList.m_Categories[sectionIndex].GetCStr()))
				{
					numInputs += inputList->m_Inputs.size();
				}
			}

			categoryInputs.Reserve(numInputs);

			// Get the inputs. NOTE: in the mapping files the 'sections' are called categories but from the point of view of the mapping screen,
			// categories are things like 'On Foot'.
			for(u32 sectionIndex = 0; sectionIndex < mappingList.m_Categories.size(); ++sectionIndex)
			{
				const ControlInput::InputList* inputList = ms_settings.m_MappingSettings.m_Categories.Access(mappingList.m_Categories[sectionIndex]);
				if(inputList != NULL)
				{
					for(u32 inputIndex = 0; inputIndex < inputList->m_Inputs.size(); ++inputIndex)
					{
						categoryInputs.PushAndGrow(inputList->m_Inputs[inputIndex]);
					}
				}
			}

			return;
		}
	}
}

void CControl::StartInputMappingScan(InputType input, MappingType type, u8 mappingSlot)
{
	m_currentMapping.Clear();

	m_currentMapping.m_Input		= input;
	m_currentMapping.m_Type			= type;
	m_currentMapping.m_MappingIndex	= mappingSlot;
	BANK_ONLY(m_currentMapping.m_MappingCameFromRag = false);
}

void CControl::CancelCurrentMapping()
{
	m_currentMapping.Clear();
}

void CControl::DeleteInputMapping(InputType input, MappingType type, u8 mappingSlot)
{
	if(controlVerifyf(mappingSlot < NUM_MAPPING_SCREEN_SLOTS, "Cannot delete a mapping in slot %d as its not changable!", mappingSlot))
	{
		// Remove mapping from previous slot.
		const ioSource unmapped(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
		UpdateMapping( input,
			type,
			unmapped,
			mappingSlot,
			true );
	}
}

#endif // KEYBOARD_MOUSE_SUPPORT

#if USE_STEAM_CONTROLLER
void CControl::CheckForSteamController()
{
	if(m_SteamController == 0 && SteamController())
	{
		static bool hasInit = false;
		if(!hasInit)
		{
			SteamController()->Init();
			hasInit = true;
		}
		ControllerHandle_t controllers[STEAM_CONTROLLER_MAX_COUNT] = {0};
		const int result = SteamController()->GetConnectedControllers(controllers);
		if(result > 0)
		{
			m_SteamController = controllers[0];
			INIT_PARSER;
			const char* path = STEAM_CONTROLLER_SETTINGS_FILE;
#if RSG_BANK
			if(PARAM_steamControllerSettingsFile.Get())
			{
				PARAM_steamControllerSettingsFile.Get(path);
			}
#endif // RSG_BANK
			controlVerifyf(PARSER.LoadObject(path, "", m_SteamControllerSettings), "Failed to load '%s'!", path);
			SHUTDOWN_PARSER;


			m_SteamActionSet = SteamController()->GetActionSetHandle(m_SteamControllerSettings.m_LoadSet);
			SteamController()->ActivateActionSet(m_SteamController, m_SteamActionSet);

			for(u32 i = 0; i < m_SteamControllerSettings.m_Buttons.size(); ++i)
			{
				const ControlInput::SteamController::SingleMappings& mapping = m_SteamControllerSettings.m_Buttons[i];
				ControllerDigitalActionHandle_t handle = SteamController()->GetDigitalActionHandle(mapping.m_Action);
				controlDisplayf("Steam Controller Digital Action '%s' has handle %p", mapping.m_Action, handle);
				m_SteamHandleMap[mapping.m_Action] = handle;
			}

			for(u32 i = 0; i < m_SteamControllerSettings.m_Triggers.size(); ++i)
			{
				const ControlInput::SteamController::SingleMappings& mapping = m_SteamControllerSettings.m_Triggers[i];
				ControllerAnalogActionHandle_t handle = SteamController()->GetAnalogActionHandle(mapping.m_Action);
				controlDisplayf("Steam Controller Trigger Action '%s' has handle %p", mapping.m_Action, handle);
				m_SteamHandleMap[mapping.m_Action] = handle;
			}

			for(u32 i = 0; i < m_SteamControllerSettings.m_Axis.size(); ++i)
			{
				const ControlInput::SteamController::AxisMappings& mapping = m_SteamControllerSettings.m_Axis[i];
				ControllerAnalogActionHandle_t handle = SteamController()->GetAnalogActionHandle(mapping.m_Action);
				controlDisplayf("Steam Controller Digital Action '%s' has handle %p", mapping.m_Action, handle);
				m_SteamHandleMap[mapping.m_Action] = handle;
			}
		}
	}
}

void CControl::UpdateSteamController()
{
	if(m_SteamController != 0 && SteamController())
	{
		EControllerActionOrigin mappedOrigins[STEAM_CONTROLLER_MAX_ORIGINS] = {k_EControllerActionOrigin_None};

		SteamController()->ActivateActionSet(m_SteamController, m_SteamActionSet);
		for(u32 mappingIndex = 0; mappingIndex < m_SteamControllerSettings.m_Buttons.size(); ++mappingIndex)
		{
			const ControlInput::SteamController::SingleMappings mapping = m_SteamControllerSettings.m_Buttons[mappingIndex];
			const ControllerDigitalActionHandle_t handle = m_SteamHandleMap[mapping.m_Action];
			ControllerDigitalActionData_t data = SteamController()->GetDigitalActionData(m_SteamController, handle);
			if(data.bActive)
			{
				const float value = (data.bState) ? 1.0f : 0.0f;
				EControllerActionOrigin origin = k_EControllerActionOrigin_None;
				if(SteamController()->GetDigitalActionOrigins(m_SteamController, m_SteamActionSet, handle, mappedOrigins) > 0)
				{
					origin = mappedOrigins[0];
				}
				m_SteamSources[origin].m_Type = SteamSource::BUTTON;
				m_SteamSources[origin].m_Index = mappingIndex;

				for(u32 inputIndex = 0; inputIndex < mapping.m_Inputs.size(); ++inputIndex)
				{
					UpdateInputFromSteamSource(mapping.m_Inputs[inputIndex], value, origin);
				}
			}
		}


		for(u32 mappingIndex = 0; mappingIndex < m_SteamControllerSettings.m_Triggers.size(); ++mappingIndex)
		{
			const ControlInput::SteamController::SingleMappings mapping = m_SteamControllerSettings.m_Triggers[mappingIndex];
			const ControllerAnalogActionHandle_t handle = m_SteamHandleMap[mapping.m_Action];
			ControllerAnalogActionData_t data = SteamController()->GetAnalogActionData(m_SteamController, handle);
			if(data.bActive)
			{
				EControllerActionOrigin origin = k_EControllerActionOrigin_None;
				if(SteamController()->GetAnalogActionOrigins(m_SteamController, m_SteamActionSet, handle, mappedOrigins) > 0)
				{
					origin = mappedOrigins[0];
				}
				m_SteamSources[origin].m_Type = SteamSource::TRIGGER;
				m_SteamSources[origin].m_Index = mappingIndex;
				for(u32 inputIndex = 0; inputIndex < mapping.m_Inputs.size(); ++inputIndex)
				{
					UpdateInputFromSteamSource(mapping.m_Inputs[inputIndex], data.x, origin);
				}
			}
		}


		for(u32 mappingIndex = 0; mappingIndex < m_SteamControllerSettings.m_Axis.size(); ++mappingIndex)
		{
			const ControlInput::SteamController::AxisMappings mapping = m_SteamControllerSettings.m_Axis[mappingIndex];
			const ControllerAnalogActionHandle_t handle = m_SteamHandleMap[mapping.m_Action];
			ControllerAnalogActionData_t data = SteamController()->GetAnalogActionData(m_SteamController, handle);
			if(data.bActive)
			{
				EControllerActionOrigin origin = k_EControllerActionOrigin_None;
				if(SteamController()->GetAnalogActionOrigins(m_SteamController, m_SteamActionSet, handle, mappedOrigins) > 0)
				{
					origin = mappedOrigins[0];
					
					// We only support 1 icon for the gyro so use only the move.
					if(origin > k_EControllerActionOrigin_Gyro_Move && origin <= k_EControllerActionOrigin_Gyro_Roll)
					{
						origin = k_EControllerActionOrigin_Gyro_Move;
					}
				}
				m_SteamSources[origin].m_Type = SteamSource::AXIS;
				m_SteamSources[origin].m_Index = mappingIndex;

				// The Y axis is in a different direction to what we use internally.
				if(data.eMode == k_EControllerSourceMode_JoystickMove)
				{
					data.y = -data.y;
				}

				data.x *= mapping.m_Scale;
				data.y *= mapping.m_Scale;

				for(u32 inputIndex = 0; inputIndex < mapping.m_XInputs.size(); ++inputIndex)
				{
					UpdateInputFromSteamSource(mapping.m_XInputs[inputIndex], data.x, origin);
				}

				if(data.x < 0.0f)
				{
					for(u32 inputIndex = 0; inputIndex < mapping.m_XLeftInputs.size(); ++inputIndex)
					{
						UpdateInputFromSteamSource(mapping.m_XLeftInputs[inputIndex], -data.x, origin);
					}
				}
				else
				{
					for(u32 inputIndex = 0; inputIndex < mapping.m_XRightInputs.size(); ++inputIndex)
					{
						UpdateInputFromSteamSource(mapping.m_XRightInputs[inputIndex], data.x, origin);
					}
				}

				for(u32 inputIndex = 0; inputIndex < mapping.m_YInputs.size(); ++inputIndex)
				{
					UpdateInputFromSteamSource(mapping.m_YInputs[inputIndex], data.y, origin);
				}

				if(data.y < 0.0f)
				{
					for(u32 inputIndex = 0; inputIndex < mapping.m_YUpInputs.size(); ++inputIndex)
					{
						UpdateInputFromSteamSource(mapping.m_YUpInputs[inputIndex], -data.y, origin);
					}
				}
				else
				{
					for(u32 inputIndex = 0; inputIndex < mapping.m_YDownInputs.size(); ++inputIndex)
					{
						UpdateInputFromSteamSource(mapping.m_YDownInputs[inputIndex], data.y, origin);
					}
				}
			}
		}
	}
}

void CControl::UpdateInputFromSteamSource(InputType input, float value, EControllerActionOrigin source)
{
	if(controlVerifyf(input >= 0 && input < MAX_INPUTS, "Invalid input %d!", input))
	{
		ioValue& ioVal = m_inputs[input];
		if( Abs(ioVal.GetValue()) < Abs(value))
		{
			// Invert the value back if the input is inverted as steam handles inversions itself.
			if(ioVal.IsInverted())
			{
				value = -value;
			}
			ioVal.SetCurrentValue(value, ioSource(IOMS_GAME_CONTROLLED, source, ioSource::IOMD_GAME));
		}
	}
}

void CControl::GetSteamControllerInputSources(InputType input, ioMapper::ioSourceList& sources) const
{
	if(m_SteamController && SteamController())
	{
		EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS] = {k_EControllerActionOrigin_None};
		for(u32 buttonIndex = 0; buttonIndex < m_SteamControllerSettings.m_Buttons.size(); ++buttonIndex)
		{
			const ControlInput::SteamController::SingleMappings& mapping = m_SteamControllerSettings.m_Buttons[buttonIndex];
			if(mapping.m_Inputs.Find(input) != -1)
			{
				const u64* handle = m_SteamHandleMap.Access(mapping.m_Action);
				if(handle && SteamController()->GetDigitalActionOrigins(m_SteamController, m_SteamActionSet, *handle, origins) > 0)
				{
					sources.Push(ioSource(IOMS_GAME_CONTROLLED, origins[0], ioSource::IOMD_GAME));
				}
			}
		}

		for(u32 triggerIndex = 0; triggerIndex < m_SteamControllerSettings.m_Triggers.size(); ++triggerIndex)
		{
			const ControlInput::SteamController::SingleMappings& mapping = m_SteamControllerSettings.m_Triggers[triggerIndex];
			if(mapping.m_Inputs.Find(input) != -1)
			{
				const u64* handle = m_SteamHandleMap.Access(mapping.m_Action);
				if(handle && SteamController()->GetAnalogActionOrigins(m_SteamController, m_SteamActionSet, *handle, origins) > 0)
				{
					sources.Push(ioSource(IOMS_GAME_CONTROLLED, origins[0], ioSource::IOMD_GAME));
				}
			}
		}

		for(u32 axisIndex = 0; axisIndex < m_SteamControllerSettings.m_Axis.size(); ++axisIndex)
		{
			const ControlInput::SteamController::AxisMappings& mapping = m_SteamControllerSettings.m_Axis[axisIndex];

			// Axis we need to check 6 array.
			if( mapping.m_XInputs.Find(input) != -1 || mapping.m_YInputs.Find(input) != -1 ||
				mapping.m_XLeftInputs.Find(input) != -1 || mapping.m_XRightInputs.Find(input) != -1 ||
				mapping.m_YUpInputs.Find(input) != -1 || mapping.m_YDownInputs.Find(input) != -1 )
			{
				const u64* handle = m_SteamHandleMap.Access(mapping.m_Action);
				if(handle && SteamController()->GetAnalogActionOrigins(m_SteamController, m_SteamActionSet, *handle, origins) > 0)
				{
					// We only support 1 icon for the gyro so use only the move.
					if(origins[0] > k_EControllerActionOrigin_Gyro_Move && origins[0] <= k_EControllerActionOrigin_Gyro_Roll)
					{
						origins[0] = k_EControllerActionOrigin_Gyro_Move;
					}
					sources.Push(ioSource(IOMS_GAME_CONTROLLED, origins[0], ioSource::IOMD_GAME));
				}
			}
		}
	}
}

void CControl::DisableInputOnSteamControllerSource(const ioSource& source, const ioValue::DisableOptions& options)
{
	if(source.m_Device == IOMS_GAME_CONTROLLED && source.m_DeviceIndex == ioSource::IOMD_GAME)
	{
		EControllerActionOrigin origin = static_cast<EControllerActionOrigin>(source.m_Parameter);
		if(origin > k_EControllerActionOrigin_Gyro_Move && origin <= k_EControllerActionOrigin_Gyro_Roll)
		{
			origin = k_EControllerActionOrigin_Gyro_Move;
		}

		const SteamSource& steamSouce = m_SteamSources[origin];
		switch(steamSouce.m_Type)
		{
		case SteamSource::BUTTON:
			DisableInputsInArray(m_SteamControllerSettings.m_Buttons[steamSouce.m_Index].m_Inputs, options);
			break;

		case SteamSource::TRIGGER:
			DisableInputsInArray(m_SteamControllerSettings.m_Triggers[steamSouce.m_Index].m_Inputs, options);

			break;
		case SteamSource::AXIS:
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_XInputs, options);
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_XLeftInputs, options);
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_XRightInputs, options);
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_YInputs, options);
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_YUpInputs, options);
			DisableInputsInArray(m_SteamControllerSettings.m_Axis[steamSouce.m_Index].m_YDownInputs, options);
			break;
		default:
			break;

		}
	}
}

void CControl::DisableInputsInArray(const atArray<InputType>& inputs, const ioValue::DisableOptions& options)
{
	for (u32 i = 0; i < inputs.size(); ++i)
	{
		DoDisableInput(inputs[i], options, false);
	}
}

#endif // USE_STEAM_CONTROLLER


#if __WIN32PC
CControl::JoystickDefinitionData::JoystickDefinitionData()
	: m_Data()
	, m_Guid()
	, m_PadParameter(IOM_UNDEFINED)
	, m_ConflictingSource(INVALID_PAD_SOURCE)
	, m_DeviceIndex(0)
	, m_ScanStarted(false)
	, m_ScanReady(false)
	, m_Calibrate(false)
{
	m_ScanOptions.m_JoystickOptions = ioMapper::ScanOptions::JOYSTICK_BUTTONS
									| ioMapper::ScanOptions::JOYSTICK_POV
									| ioMapper::ScanOptions::JOYSTICK_AXIS
									| ioMapper::ScanOptions::JOYSTICK_AXIS_DIRECTION;
}

bool CControl::JoystickDefinitionData::GetAutoMappings( const ioSource& negativeSource, ioSource& positiveMapping, ioSource& fullAxisMapping, bool isPositive )
{
	bool mappingFound = false;

	if(negativeSource.m_Parameter >= IOM_JOYSTICK_AXIS1 && negativeSource.m_Parameter <= IOM_JOYSTICK_AXIS8)
	{
		if(negativeSource.m_Device == IOMS_JOYSTICK_AXIS_NEGATIVE)
		{
			positiveMapping = negativeSource;
			fullAxisMapping = negativeSource;

			positiveMapping.m_Device = IOMS_JOYSTICK_AXIS_POSITIVE;

			if(isPositive)
			{
				fullAxisMapping.m_Device = IOMS_JOYSTICK_IAXIS;
			}
			else
			{
				fullAxisMapping.m_Device = IOMS_JOYSTICK_AXIS;
			}

			mappingFound = true;
		}
		else if(negativeSource.m_Device == IOMS_JOYSTICK_AXIS_POSITIVE)
		{
			positiveMapping = negativeSource;
			fullAxisMapping = negativeSource;

			positiveMapping.m_Device = IOMS_JOYSTICK_AXIS_NEGATIVE;
			fullAxisMapping.m_Device = IOMS_JOYSTICK_IAXIS;

			if(isPositive)
			{
				fullAxisMapping.m_Device = IOMS_JOYSTICK_AXIS;
			}
			else
			{
				fullAxisMapping.m_Device = IOMS_JOYSTICK_IAXIS;
			}

			mappingFound = true;
		}
	}

	return mappingFound;
}

bool CControl::JoystickDefinitionData::GetAutoMappingParameters( const ioMapperParameter& parameter, ioMapperParameter& opposite, ioMapperParameter& fullAxis )
{
	switch(parameter)
	{
	case IOM_AXIS_LX_RIGHT:
		opposite = IOM_AXIS_LX_LEFT;
		fullAxis = IOM_AXIS_LX;
		return true;

	case IOM_AXIS_LX_LEFT:
		opposite = IOM_AXIS_LX_RIGHT;
		fullAxis = IOM_AXIS_LX;
		return true;

	case IOM_AXIS_LY_DOWN:
		opposite = IOM_AXIS_LY_UP;
		fullAxis = IOM_AXIS_LY;
		return true;

	case IOM_AXIS_LY_UP:
		opposite = IOM_AXIS_LY_DOWN;
		fullAxis = IOM_AXIS_LY;
		return true;

	case IOM_AXIS_RX_RIGHT:
		opposite = IOM_AXIS_RX_LEFT;
		fullAxis = IOM_AXIS_RX;
		return true;

	case IOM_AXIS_RX_LEFT:
		opposite = IOM_AXIS_RX_RIGHT;
		fullAxis = IOM_AXIS_RX;
		return true;

	case IOM_AXIS_RY_DOWN:
		opposite = IOM_AXIS_RY_UP;
		fullAxis = IOM_AXIS_RY;
		return true;

	case IOM_AXIS_RY_UP:
		opposite = IOM_AXIS_RY_DOWN;
		fullAxis = IOM_AXIS_RY;
		return true;

	default:
		return false;
	}
}

ioMapperParameter CControl::JoystickDefinitionData::GetAlternateButtonMappingParameters( ioMapperParameter button )
{
	switch(button)
	{
	case L2:
		return L2_INDEX;
	case R2:
		return R2_INDEX;
	case L1:
		return L1_INDEX;
	case R1:
		return R1_INDEX;
	case RUP:
		return RUP_INDEX;
	case RRIGHT:
		return RRIGHT_INDEX;
	case RDOWN:
		return RDOWN_INDEX;
	case RLEFT:
		return RLEFT_INDEX;
	case SELECT:
		return SELECT_INDEX;
	case L3:
		return L3_INDEX;
	case R3:
		return R3_INDEX;
	case START:
		return START_INDEX;
	case LUP:
		return LUP_INDEX;
	case LRIGHT:
		return LRIGHT_INDEX;
	case LDOWN:
		return LDOWN_INDEX;
	case LLEFT:
		return LLEFT_INDEX;
	case TOUCH:
		return TOUCH_INDEX;

	default:
		return IOM_UNDEFINED;
	}
}

void CControl::JoystickDefinitionData::ConvertPadMappingsToJoystickMappings(const ControlInput::Gamepad::Definition& configuration,
													const ControlInput::ControlSettings& padMappings,
													ControlInput::ControlSettings& joystickMappings )
{
	joystickMappings.m_Mappings.Reserve(padMappings.m_Mappings.size());

	// Convert mappings to direct input equivalents.
	for(s32 mappingIndex = 0; mappingIndex < padMappings.m_Mappings.size(); ++mappingIndex)
	{
		const ControlInput::Mapping& mapping = padMappings.m_Mappings[mappingIndex];

		if(mapping.m_Parameters.size() > 0)
		{
			// There is no guarantee of the order of inputs.
			for(s32 configIndex = 0; configIndex < configuration.m_Definitions.size(); ++configIndex)
			{
				const ControlInput::Gamepad::Source& inputConfig = configuration.m_Definitions[configIndex];

				// if this is the pad parameter we are looking for then use its joystick equivalent.
				if( inputConfig.m_PadParameter == mapping.m_Parameters[0] || 
					GetAlternateButtonMappingParameters(inputConfig.m_PadParameter) == mapping.m_Parameters[0])
				{
					ControlInput::Mapping newMapping;
					newMapping.m_Input = mapping.m_Input;
					newMapping.m_Source = inputConfig.m_JoystickSource;
					newMapping.m_Parameters.Reserve(1);
					newMapping.m_Parameters.Push(inputConfig.m_JoystickParameter);

					joystickMappings.m_Mappings.PushAndGrow(newMapping);

					// We don't stop the loop as some Joystick definitions (notably the default) may have several alternative for
					// each input (such as axis or button for triggers).
				}
			}
		}
	}
}


void CControl::UpdateJoystickBindings()
{
	if(m_CurrentJoystickDefinition.m_Calibrate)
	{
		if(ioJoystick::GetStick(m_CurrentJoystickDefinition.m_DeviceIndex).HasInputFocus())
		{
			ioJoystick::AutoCalibrate(m_CurrentJoystickDefinition.m_DeviceIndex, true);
			m_CurrentJoystickDefinition.m_Calibrate = false;
		}

		// Do not scan the device if we are/need calibrating.
		return;
	}

	PF_PUSH_TIMEBAR("controls.Update.joystickBindScan");
	if(m_CurrentJoystickDefinition.m_PadParameter != IOM_UNDEFINED)
	{
		if(!m_CurrentJoystickDefinition.m_ScanStarted)
		{
			controlDebugf1( "Gamepad Definition - Start scan for '%s'. Waiting for no input.",
							GetParameterText(m_CurrentJoystickDefinition.m_PadParameter) );
			m_CurrentJoystickDefinition.m_ScanStarted = true;
			m_CurrentJoystickDefinition.m_ScanReady = false;
			ioMapper::BeginScan();
		}

		ioSource source;
		m_CurrentJoystickDefinition.m_ScanOptions.m_DeviceID = m_CurrentJoystickDefinition.m_DeviceIndex;

		// Wait for the button to be released before we allow scanning of the next button.
		if(m_CurrentJoystickDefinition.m_ScanReady == false && m_Mappers[0].Scan(source, m_CurrentJoystickDefinition.m_ScanOptions) == false)
		{
			controlDebugf1( "Gamepad Definition - No Input detected, now scanning for '%s'.",
							GetParameterText(m_CurrentJoystickDefinition.m_PadParameter) );
			m_CurrentJoystickDefinition.m_ScanReady = true;
		}
		else if(m_CurrentJoystickDefinition.m_ScanReady && m_Mappers[0].Scan(source, m_CurrentJoystickDefinition.m_ScanOptions))
		{
			ioMapperParameter param = ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter);

			controlDebugf1( "Gamepad Definition - Mapped '%s' to '%s'.",
				GetParameterText(m_CurrentJoystickDefinition.m_PadParameter),
				GetParameterText(param) );

			// update parameter to mapper parameter.
			source.m_Parameter = param;

			// find the corresponding data.
			ControlInput::Gamepad::Source definition;
			definition.m_PadParameter = m_CurrentJoystickDefinition.m_PadParameter;
			definition.m_JoystickParameter = param;
			definition.m_JoystickSource = source.m_Device;

			// Do not map a button/axis to more than one button/axis.
			if( source.m_DeviceIndex < 0 || CControlMgr::GetPad(source.m_DeviceIndex).IsXBox360CompatiblePad() || IsGamepadDefinitionValid(definition) == false ||
				(m_CurrentJoystickDefinition.m_DeviceIndex != ioSource::IOMD_ANY && source.m_DeviceIndex != m_CurrentJoystickDefinition.m_DeviceIndex) )
			{
				controlDebugf1( "Gamepad Definition - '%s' invalid for '%s', ignoring.",
								GetParameterText(definition.m_JoystickParameter),
								GetParameterText(m_CurrentJoystickDefinition.m_PadParameter) );

				// Wait for button to be released.
				m_CurrentJoystickDefinition.m_ScanReady = false;
			}
			else
			{
				m_CurrentJoystickDefinition.m_ConflictingSource = GetCurrentGamepadDefinition(definition);
				if(m_CurrentJoystickDefinition.m_ConflictingSource != INVALID_PAD_SOURCE)
				{
					controlDebugf1( "Gamepad Definition - '%s' already mapped to '%s', ignoring.",
									GetParameterText(definition.m_JoystickParameter),
									GetParameterText(ConvertGamepadInputToParameter(m_CurrentJoystickDefinition.m_ConflictingSource)) );

					// Wait for button to be released.
					m_CurrentJoystickDefinition.m_ScanReady = false;
				}
				else
				{
					UpdateCurrentGamepadDefinition(definition);

					ioMapperParameter positive;
					ioMapperParameter fullAxis;
					ioSource positiveSource;
					ioSource fullAxisSource;

					const bool isPositive = ( definition.m_PadParameter == IOM_AXIS_LX_RIGHT || definition.m_PadParameter == IOM_AXIS_LY_DOWN ||
											  definition.m_PadParameter == IOM_AXIS_RX_RIGHT || definition.m_PadParameter == IOM_AXIS_RY_DOWN );

					// if the input should auto-map related axis and auto mappings can be generated.
					if(    JoystickDefinitionData::GetAutoMappingParameters(definition.m_PadParameter, positive, fullAxis)
						&& JoystickDefinitionData::GetAutoMappings(source, positiveSource, fullAxisSource, isPositive) )
					{
						definition.m_PadParameter = positive;
						definition.m_JoystickParameter = param;
						definition.m_JoystickSource = positiveSource.m_Device;
						UpdateCurrentGamepadDefinition(definition);

						definition.m_PadParameter = fullAxis;
						definition.m_JoystickParameter = param;
						definition.m_JoystickSource = fullAxisSource.m_Device;
						UpdateCurrentGamepadDefinition(definition);
					}

					BANK_ONLY(UpdateJoystickDefinitionRagData());

					ioMapper::EndScan();
					m_CurrentJoystickDefinition.m_ScanStarted  = false;
					m_CurrentJoystickDefinition.m_ScanReady	   = false;
					m_CurrentJoystickDefinition.m_PadParameter = IOM_UNDEFINED;

					m_CurrentJoystickDefinition.m_DeviceIndex  = source.m_DeviceIndex;
					m_CurrentJoystickDefinition.m_Guid		   = ioJoystick::GetStick(m_CurrentJoystickDefinition.m_DeviceIndex).GetProductGuidStr();
				}
			}
		}
	}
	PF_POP_TIMEBAR();
}

void CControl::LoadGamepadControls(const ControlInput::Gamepad::Definition& definition, const ControlInput::ControlSettings& settings, s32 padIndex)
{
#if !RSG_FINAL
	if(PARAM_enableXInputEmulation.Get())
	{
		return;
	}
#endif // !RSG_FINAL
	ControlInput::ControlSettings joystickSettings;
	JoystickDefinitionData::ConvertPadMappingsToJoystickMappings(definition, settings, joystickSettings);
	LoadSettings(joystickSettings, padIndex);
}

void CControl::ApplyDirectionalForce(float FORCE_FEEDBACK_ONLY(X), float FORCE_FEEDBACK_ONLY(Y), s32 FORCE_FEEDBACK_ONLY(timeMS))
{
#if FORCE_FEEDBACK_SUPPORT
	if(m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || !ioSource::IsValidDevice(m_LastKnownSource.m_DeviceIndex))
	{
		return;
	}

	s32 deviceId = (m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_DEFAULT) ? m_padNum : m_LastKnownSource.m_DeviceIndex;

	// if a joystick vibrate it.
	if(GetMappingType(m_LastKnownSource.m_Device) == JOYSTICK)
	{
		ioJoystick::ApplyDirectionalForce(deviceId, X, Y, timeMS);
	}
#endif // FORCE_FEEDBACK_SUPPORT
}

bool CControl::LoadPCScriptControlMappings(const atHashString& mappingSceme)
{
	sysCriticalSection lock(m_Cs);

	Assertf(strcmp(m_ControlSchemeScriptOwner, "") == 0, "'%s' has setup scripted controls but not shut them down!", m_ControlSchemeScriptOwner);
	ASSERT_ONLY(safecpy(m_ControlSchemeScriptOwner, CTheScripts::GetCurrentScriptName()));

	m_CurrentScriptDynamicScheme = mappingSceme;
	return LoadDynamicControlMappings();
}

bool CControl::SwitchPCScriptControlMappings(const atHashString& mappingSceme)
{
	sysCriticalSection lock(m_Cs);

	Assertf( strcmp(m_ControlSchemeScriptOwner, CTheScripts::GetCurrentScriptName()) == 0,
			 "'%s' has called 'LOAD_PC_SCRIPTED_CONTROL_SCHEME()' without initialising PC scripted controls!",
			 CTheScripts::GetCurrentScriptName() );
	ASSERT_ONLY(safecpy(m_ControlSchemeScriptOwner, CTheScripts::GetCurrentScriptName()));

	m_CurrentScriptDynamicScheme = mappingSceme;
	return LoadDynamicControlMappings();
}

void CControl::ShutdownPCScriptControlMappings()
{
	sysCriticalSection lock(m_Cs);

	ASSERT_ONLY(m_ControlSchemeScriptOwner[0] = '\0');
	m_CurrentScriptDynamicScheme = atHashString("Default",0xE4DF46D5);
	LoadDynamicControlMappings();
}

bool CControl::LoadVideoEditorScriptControlMappings()
{
	sysCriticalSection lock(m_Cs);
	m_CurrentCodeOverrideDynamicScheme = atHashString("REPLAY CAMERA",0xfcdb2688);
	return LoadDynamicControlMappings();
}

bool CControl::UnloadVideoEditorScriptControlMappings()
{
	sysCriticalSection lock(m_Cs);
	m_CurrentCodeOverrideDynamicScheme = atHashString("Default",0xE4DF46D5);
	return LoadDynamicControlMappings();
}

bool CControl::LoadDynamicControlMappings()
{
	const static atHashString defaultMappings("Default",0xE4DF46D5);

	atHashString mappingScheme = m_CurrentScriptDynamicScheme;
	if(m_CurrentCodeOverrideDynamicScheme.GetHash() != 0 && m_CurrentCodeOverrideDynamicScheme != defaultMappings)
	{
		mappingScheme = m_CurrentCodeOverrideDynamicScheme;
	}

	bool result = true;
	if(m_CurrentLoadedDynamicScheme != mappingScheme)
	{
		// ensure the requested mapping scheme is valid.
		ControlInput::DynamicMappings* mappings = ms_settings.m_DynamicMappingList.m_DynamicMappings.Access(mappingScheme);
		controlAssertf(mappings != NULL, "Dynamic mapping '%s' does not exist! Reverting to default!", mappingScheme.GetCStr());

		if(mappings == NULL)
		{
			// we have failed to find the mapping but we can use the default.
			result = false;

			if(m_CurrentLoadedDynamicScheme != defaultMappings)
			{
				// fall back to default!
				mappings = ms_settings.m_DynamicMappingList.m_DynamicMappings.Access(defaultMappings);
				controlAssertf(mappings != NULL, "Default dynamic mapping does not exist!");
#if __BANK
				if(mappings != NULL)
				{
					safecpy(m_RagLoadedPCControlScheme, "Default", RagData::TEXT_SIZE);
				}
				else
				{
					safecpy(m_RagLoadedPCControlScheme, "<Error: NO CONTROLS LOADED>", RagData::TEXT_SIZE);
				}
#endif // __BANK
			}
		}
#if __BANK
		else
		{
			safecpy(m_RagLoadedPCControlScheme, mappingScheme.GetCStr(), RagData::TEXT_SIZE);
		}
#endif // __BANK

		if(mappings != NULL)
		{
			ioMapper::ioSourceList primarySources;
			ioMapper::ioSourceList secondarySources;

			for(s32 mappingIndex = 0; mappingIndex < mappings->m_Mappings.size(); ++mappingIndex)
			{
				const ControlInput::DynamicMapping& mapping = mappings->m_Mappings[mappingIndex];
				ioMapper& mapper = m_Mappers[ms_settings.m_InputMappers[mapping.m_Input]];

				// firstly we need to unmap all the previous mappings. We could have removed all these at the start
				// but then if we forget to map something we could have an input that is not mapped!
				mapper.RemoveDeviceMappings(m_inputs[mapping.m_Input], ioSource::IOMD_KEYBOARD_MOUSE);

				// Ensure we are mapping for a scripted input only and that there is a dynamic mapping for the scripted input
				if(    controlVerifyf(mapping.m_Input >= SCRIPTED_INPUT_FIRST && mapping.m_Input <= SCRIPTED_INPUT_LAST, "Invalid dynamic input!")
					&& controlVerifyf(mapping.m_Mappings.GetCount() > 0, "No mappings for dynamic input %s!", GetInputName(mapping.m_Input)))
				{

					// UNDEFINED_INPUT means no mapping for this scripted input. 
					if(mapping.m_Mappings[0] != UNDEFINED_INPUT && ValidPrimaryDynamicMapping(mapping))
					{
						// DYNAMIC_MAPPING_MOUSE_X uses the mouse x cursor position from the center of the window.
						if(mapping.m_Mappings[0] == DYNAMIC_MAPPING_MOUSE_X)
						{
							controlAssertf(mapping.m_Mappings.GetCount() == 1, "%s is mapped to DYNAMIC_MAPPING_MOUSE_X and another input!", GetInputName(mapping.m_Input));
							mapper.Map(IOMS_MOUSE_CENTEREDAXIS, IOM_AXIS_X, m_inputs[mapping.m_Input], ioSource::IOMD_KEYBOARD_MOUSE);
							m_isInputMapped[mapping.m_Input] = true;
						}

						// DYNAMIC_MAPPING_MOUSE_X uses the mouse y cursor position from the center of the window.
						else if(mapping.m_Mappings[0] == DYNAMIC_MAPPING_MOUSE_Y)
						{
							controlAssertf(mapping.m_Mappings.GetCount() == 1, "%s is mapped to DYNAMIC_MAPPING_MOUSE_Y and another input!", GetInputName(mapping.m_Input));
							mapper.Map(IOMS_MOUSE_CENTEREDAXIS, IOM_AXIS_Y, m_inputs[mapping.m_Input], ioSource::IOMD_KEYBOARD_MOUSE);
							m_isInputMapped[mapping.m_Input] = true;
						}

						// If there is one mapping or the second is valid then proceed with the mapping!
						else if(ValidSecondaryDynamicMapping(mapping))
						{
							primarySources.clear();
							GetSpecificInputSources(mapping.m_Mappings[0], primarySources, ioSource::IOMD_KEYBOARD_MOUSE);

							s32 numSources = primarySources.GetCount();

							// If there is more than one mapping then we are trying to create an axis out of other inputs.
							if(mapping.m_Mappings.GetCount() > 1)
							{
								secondarySources.clear();
								GetSpecificInputSources(mapping.m_Mappings[1], secondarySources, ioSource::IOMD_KEYBOARD_MOUSE);

								// The number of mappings should be the same, if not, then we use the smaller size and ignore the rest of the mapping when creating an axis.
								// This will happen if the user has a primary and secondary mapping for one input but only a primary for the other.
								if(primarySources.GetCount() == secondarySources.GetCount())
								{
									controlWarningf("Dynamic mapping using two inputs have mismatching mapping amounts, excess will be ignored!");
								}
								numSources = Min(primarySources.GetCount(), secondarySources.GetCount());
							}

							ioSource source;
							// Go through all sources and create mappings.
							for(s32 sourceIndex = 0; sourceIndex < numSources; ++sourceIndex)
							{
								// If we are trying to create an axis then get a compatible source.
								if(mapping.m_Mappings.GetCount() > 1)
								{
									source = GetAxisCompatableSource(primarySources[sourceIndex], secondarySources[sourceIndex]); 
								}
								else
								{
									source = primarySources[sourceIndex];
								}

								// Ensure the source is valid (this is primarily for axis but there is no harm in checking).
								if(controlVerifyf(source.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE, "Dynamic mapping for '%s' is not incompatible!", GetInputName(mapping.m_Input)))
								{
									mapper.Map(source.m_Device, source.m_Parameter, m_inputs[mapping.m_Input], ioSource::IOMD_KEYBOARD_MOUSE);
									m_isInputMapped[mapping.m_Input] = true;
								}
							}
						}
					}
					else
					{
						controlAssertf(mapping.m_Mappings.GetCount() == 1, "%s is mapped to UNDEFINED_INPUT and another input!", GetInputName(mapping.m_Input));
					}
				}
			}

			m_CurrentLoadedDynamicScheme = mappingScheme;
		}
	}

	return result;
}

void CControl::RestoreDefaultMappings()
{
	char path[PATH_BUFFER_SIZE] = {0};
	GetUserSettingsPath(path, PC_USER_SETTINGS_FILE);
	if(ASSET.Exists(path, ""))
	{
		const fiDevice* device = fiDevice::GetDevice(path, false);
		if(controlVerifyf(device != NULL, "Failed to get user settings device!"))
		{
			controlVerifyf(device->Delete(path), "Failed to delete user settings file at '%s'!", path);
		}
	}
	SetInitialDefaultMappings(true);
}

bool CControl::DoesUsersMappingFileNeedUpdating() const
{
	sysCriticalSection lock(m_Cs);

	if(m_RecachedConflictCount)
	{
		m_CachedConflictCount = GetNumberOfMappingConflicts();
		m_RecachedConflictCount = false;
	}

	return m_CachedConflictCount > 0;
}

void CControl::GetUnmappedInputs(InputList& unmappedInputs ASSERT_ONLY(, bool assertOnUnmappedInput)) const
{
	for(s32 input = 0; input < MAX_INPUTS; ++input)
	{
		if(ms_settings.m_InputMappers[input] != DEPRECATED_MAPPER)
		{
			ioMapper::ioSourceList sources;
			GetSpecificInputSources(static_cast<InputType>(input), sources, ioSource::IOMD_KEYBOARD_MOUSE);

			// The input does not have a keyboard/mouse mapping so check it is marked as unmappable.
			bool hasMapping = false;
			
			// Any mapping in index NUM_MAPPING_SCREEN_SLOTS or greater cannot be changed. We do not count it for mappings!
			const s32 numChangableMappings = Min(sources.size(), NUM_MAPPING_SCREEN_SLOTS);
			for(s32 sourceIndex = 0; hasMapping == false && sourceIndex < numChangableMappings; ++sourceIndex)
			{
				hasMapping = (sources[sourceIndex].m_Parameter != KEY_NULL);
			}

			if(hasMapping == false)
			{
				if(ms_settings.m_InputMappable[input])
				{
					// We add error tty so we can get a list of unmapped inputs!
					controlErrorf("'%s' is currently unmapped!", GetInputName(static_cast<InputType>(input)));
					controlAssertf(assertOnUnmappedInput == false, "'%s' is currently unmapped!", GetInputName(static_cast<InputType>(input)));
					unmappedInputs.PushAndGrow(static_cast<InputType>(input));
				}
			}
		}
	}
}

bool CControl::SaveUserControls(bool isAutoSave, s32 deviceId)
{
#if RSG_ASSERT
	if(deviceId == ioSource::IOMD_KEYBOARD_MOUSE && !isAutoSave)
	{
		GetNumberOfMappingConflicts(true);
	}
#endif // RSG_ASSERT

#if ALLOW_USER_CONTROLS
	// If the device is not the keyboard/mouse and (is a valid non XInput gamepad without a gamepad definition).
	if( deviceId != ioSource::IOMD_KEYBOARD_MOUSE &&
		(!IsValidJoystickDevice(deviceId) ||
		controlVerifyf(!ms_settings.GetValidGamepadDefinition(ioJoystick::GetStick(deviceId)), "Cannot save custom device settings as a gamepad definition exists!")) )
	{
		return false;
	}

	INIT_PARSER;

	// Due to the way that inputs are shown on the mapping screen, an input may be in multiple categories.
	// Keep track of each input that has been saved.
	bool inputAlreadySaved[MAX_INPUTS];
	sysMemSet(inputAlreadySaved, 0, sizeof(bool) * MAX_INPUTS);
	ControlInput::DeviceSettings deviceSettings;

	// Go through the controls in the order they are shown on the screen and check if they have changed from the defaults, if they have we then
	// save them out. This will automatically ignore un-mappable controls and group the controls relative to their on-screen placement making
	// them easier for users to alter; it also means we do not need to keep a separate list of un-mappable inputs. The downside is nested loops
	// and less efficient code but this only runs when the user saves their controls.
	// 
	// The structure of the controls screen order is as follows:
	// MappingList - Defines a page/section of controls e.g. On Foot.
	// +--- m_Categories - An array of hash string name of a category. E.g. In Car will have a general vehicle category and car specific inputs.
	//     +--- Input - Categories are an array of inputs.
	for(u32 mappingListIndex = 0; mappingListIndex < ms_settings.m_MappingSettings.m_MappingList.size(); ++mappingListIndex)
	{
		// The current mapping list (e.g. page/section of controls such as On Foot or Melee).
		const ControlInput::MappingList& mappingList = ms_settings.m_MappingSettings.m_MappingList[mappingListIndex];

		// Loop through all categories in the mapping list.
		for (u32 catagoryIndex = 0; catagoryIndex < mappingList.m_Categories.size(); ++catagoryIndex)
		{
			const ControlInput::InputList* list = ms_settings.m_MappingSettings.m_Categories.Access(mappingList.m_Categories[catagoryIndex]);
			if( controlVerifyf(list != NULL, 
				"Invalid category '%s' (%d) whilst saving control mappings",
				mappingList.m_Categories[catagoryIndex].TryGetCStr(),
				mappingList.m_Categories[catagoryIndex].GetHash()) )
			{
				const atArray<InputType>& inputs = list->m_Inputs;

				// Loop through all the inputs in the category.
				for(u32 inputIndex = 0; inputIndex < inputs.size(); ++inputIndex)
				{
					InputType input = inputs[inputIndex];
					Mapper mapperId = ms_settings.m_InputMappers[input];

					if( inputAlreadySaved[input] == false &&
						mapperId != DEPRECATED_MAPPER &&
						controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "Invalid mapper assigned to %s!", GetInputName(input)) )
					{
						inputAlreadySaved[input] = true;
						SaveChangedInput(input, deviceId, m_DefaultKeyboardMouseSettings, deviceSettings.m_ControlSettings);
					}
				}
			}
		}
	}

	if(deviceId == ioSource::IOMD_KEYBOARD_MOUSE && !isAutoSave)
	{
		InputList unmappedInputs;
		GetUnmappedInputs(unmappedInputs ASSERT_ONLY(, true));
		if(unmappedInputs.size() > 0)
		{
			return false;
		}
	}

	char path[PATH_BUFFER_SIZE] = {0};
	if(deviceId == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		if(isAutoSave)
		{
			GetUserSettingsPath(path, PC_AUTOSAVE_USER_SETTINGS_FILE);
		}
		else
		{
			GetUserSettingsPath(path, PC_USER_SETTINGS_FILE);
		}

		ASSET.CreateLeadingPath(path);
		PARSER.SaveObjectAnyBuild(path, "", &deviceSettings.m_ControlSettings);
	}
	else
	{	
		const ioJoystick& stick = ioJoystick::GetStick(deviceId);
		stick.GetProductName(deviceSettings.m_Description, COUNTOF(deviceSettings.m_Description));

		GetUserSettingsPath(path);
		safecat(path, stick.GetProductGuidStr());
		safecat(path, ".");
			
		if(isAutoSave)
		{
			safecat(path, AUTOSAVE_EXT);
		}
		else
		{
			safecat(path, USER_SETTINGS_EXT);
		}

		ASSET.CreateLeadingPath(path);
		PARSER.SaveObjectAnyBuild(path, "", &deviceSettings);
	}


	SHUTDOWN_PARSER;
#endif // ALLOW_USER_CONTROLS

	return true;
}

bool CControl::HasAutoSavedUserControls(s32 deviceId) const
{
	char path[PATH_BUFFER_SIZE] = {0};
	if(deviceId == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		GetUserSettingsPath(path, PC_AUTOSAVE_USER_SETTINGS_FILE);
	}
	else if(controlVerifyf(IsValidJoystickDevice(deviceId), "Invalid Device ID %d!", deviceId))
	{
		char fileName[50] = {0};
		formatf(fileName, "%s.%s", ioJoystick::GetStick(deviceId).GetProductGuidStr(), AUTOSAVE_EXT);
		GetUserSettingsPath(path, fileName);
	}
	else
	{
		return false;
	}
	return ASSET.Exists(path, "");
}

bool CControl::DeleteAutoSavedUserControls(s32 deviceId) const
{
	char path[PATH_BUFFER_SIZE] = {0};
	if(deviceId == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		GetUserSettingsPath(path, PC_AUTOSAVE_USER_SETTINGS_FILE);
	}
	else if(controlVerifyf(IsValidJoystickDevice(deviceId), "Invalid Device ID %d!", deviceId))
	{
		char fileName[50] = {0};
		formatf(fileName, "%s.%s", ioJoystick::GetStick(deviceId).GetProductGuidStr(), AUTOSAVE_EXT);
		GetUserSettingsPath(path, fileName);
	}
	else
	{
		return false;
	}

	const fiDevice* device = fiDevice::GetDevice(path, false);
	if(device != NULL)
	{
		return device->Delete(path);
	}

	return false;
}

void CControl::SaveChangedInput(const InputType input, const s32 deviceId, const ControlInput::ControlSettings& defaultSettings, ControlInput::ControlSettings& userSettings) const
{
	ioMapper::ioSourceList sources;
	GetSpecificInputSources(input, sources, deviceId);

	// The converted ioSourceList to ControlInput::Mapping.

	u32 nullMappingsToAdd = 0u;
	atFixedArray<ControlInput::Mapping, NUM_MAPPING_SCREEN_SLOTS> newSettings;
	for(s32 sourceIdx = 0; sourceIdx < Min(sources.size(), NUM_MAPPING_SCREEN_SLOTS); ++sourceIdx)
	{
		const ioSource& source = sources[sourceIdx];

		ControlInput::Mapping mapping;
		mapping.m_Input = input;
		mapping.m_Source = source.m_Device;

		// for now just assume there is one parameter.
		// TODO: we might need to check this later.

		if(source.m_Device == IOMS_MKB_AXIS)
		{
			mapping.m_Parameters.Reserve(2);
			mapping.m_Parameters.Push(ioMapper::ConvertParameterToMapperValue(IOMS_MKB_AXIS, ioMapper::GetMkbAxisPositive(source.m_Parameter, true)));
			mapping.m_Parameters.Push(ioMapper::ConvertParameterToMapperValue(IOMS_MKB_AXIS, ioMapper::GetMkbAxisNegative(source.m_Parameter, true)));
		}
		else
		{
			mapping.m_Parameters.Reserve(1);
			mapping.m_Parameters.Push(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter));
		}

		// If null mapping then only add it when we find a non-null mapping unless its the first mapping.
		if(newSettings.size() > 0 && mapping.m_Source == IOMS_KEYBOARD && mapping.m_Parameters.size() == 1 && mapping.m_Parameters[0] == KEY_NULL)
		{
			++nullMappingsToAdd;
		}
		else
		{
			// If there is a null mapping before a non-null mapping add a null mapping for each null mapping before. 
			if(nullMappingsToAdd > 0)
			{
				ControlInput::Mapping nullMapping;
				nullMapping.m_Input	 = input;
				nullMapping.m_Source = IOMS_KEYBOARD;
				nullMapping.m_Parameters.Reserve(1);
				nullMapping.m_Parameters.Push(KEY_NULL);

				for(u32 nullIndex = 0; nullIndex < nullMappingsToAdd; ++nullIndex)
				{
					newSettings.Push(nullMapping);
				}
				nullMappingsToAdd = 0;
			}
			newSettings.Push(mapping);
		}
	}

	bool settingChanged = false;
	u32 mappingIndex = 0;

	// Check for change
	for(u32 defaultIdx = 0; defaultIdx < defaultSettings.m_Mappings.size(); ++defaultIdx)
	{
		const ControlInput::Mapping& defaultMapping = defaultSettings.m_Mappings[defaultIdx];
		if(defaultMapping.m_Input == input)
		{		
			if( mappingIndex >= newSettings.size() ||
				!AreTwoMappingsEqual(defaultMapping, newSettings[mappingIndex]))
			{
				settingChanged = true;
				break;
			}

			++mappingIndex;

			// Any mapping over NUM_MAPPING_SCREEN_SLOTS is considered fixed and unchangeable!
			if(mappingIndex >= NUM_MAPPING_SCREEN_SLOTS)
			{
				break;
			}
		}
	}

	if(settingChanged || mappingIndex < newSettings.size())
	{
		for(u32 settingIdx = 0; settingIdx < newSettings.size(); ++settingIdx)
		{
			userSettings.m_Mappings.PushAndGrow(newSettings[settingIdx]);
		}
	}
}

bool CControl::AreTwoMappingsEqual(const ControlInput::Mapping& lhs, const ControlInput::Mapping& rhs)
{
	if(lhs.m_Source != rhs.m_Source ||	lhs.m_Parameters.size() != rhs.m_Parameters.size())
	{
		return false;
	}

	// compare each element of params.
	for(u32 paramIndex = 0; paramIndex < rhs.m_Parameters.size(); ++paramIndex)
	{
		if(lhs.m_Parameters[paramIndex] != rhs.m_Parameters[paramIndex])
		{
			return false;
		}
	}

	return true;
}

void CControl::UpdateKeyBindings()
{
	PF_PUSH_TIMEBAR("controls.Update.keyBindScan");
	if(m_currentMapping.m_Input != UNDEFINED_INPUT)
	{
		if(m_currentMapping.m_ScanMapper == NULL)
		{
			s32 mapperId = ms_settings.m_InputMappers[m_currentMapping.m_Input];

			controlAssertf( mapperId != DEPRECATED_MAPPER,
							"%s is deprecated so cannot map a device to it!",
							GetInputName(m_currentMapping.m_Input) );

			if(controlVerifyf( mapperId >= 0 && mapperId < MAPPERS_MAX,
							   "%s has an invalid mapper so cannot map a device it it!",
							   GetInputName(m_currentMapping.m_Input) ))
			{
				m_currentMapping.m_ScanMapper = &m_Mappers[mapperId];
				ioMapper::BeginScan();
			}
			else
			{
				// Reset on error.
				m_currentMapping = CurrentMappingData();
			}
		}
		else if(m_currentMapping.m_Conflict.m_Conflicts.size() > 0)
		{
			if( m_currentMapping.m_CancelMapping )
			{
				m_currentMapping = CurrentMappingData();
				ioMapper::EndScan();
			}
			else if ( m_currentMapping.m_ReplaceConflict BANK_ONLY(|| m_currentMapping.m_MappingCameFromRag) )
			{
				if(controlVerifyf(AreConflictingInputsMappable(), "Mapping cannot be replaced as a conflict is not re-mappable! canceling mapping!"))
				{
					for(u32 i = 0; i < m_currentMapping.m_Conflict.m_Conflicts.size(); ++i)
					{
							// Clear conflict resolution options just in case.
							m_currentMapping.m_CancelMapping = false;
							m_currentMapping.m_ReplaceConflict = false;

							const ioSource unmapped(IOMS_KEYBOARD, KEY_NULL, ioSource::IOMD_KEYBOARD_MOUSE);
							UpdateMapping( m_currentMapping.m_Conflict.m_Conflicts[i].m_Input,
								m_currentMapping.m_Type,
								unmapped,
								m_currentMapping.m_Conflict.m_Conflicts[i].m_MappingIndex,
								true);
					}

					m_currentMapping.m_Conflict = MappingConflictData();
				}
				else
				{
					CancelCurrentMapping();
				}
			}
		}
		else if(UpdateMappingFromInputScan())
		{
			m_currentMapping = CurrentMappingData();
			ioMapper::EndScan();
		}
	}
	PF_POP_TIMEBAR();
}

u32 CControl::GetNumberOfMappingConflicts(bool assertOnConflict /* = false */) const
{
	InputList unmappedInputs;
	GetConflictingInputs(unmappedInputs, assertOnConflict);

	// We divide by two because if A conflicts with B then both will be marked as conflicting with each other.
	return unmappedInputs.size() / 2;
}

void CControl::GetConflictingInputs(InputList& conflictingInputs, bool assertOnConflict) const
{
	// Check for any conflict mappings. NOTE: this will be very slow!
	ioMapper::ioSourceList sources;
	MappingConflictData tmpData;
	for(u32 inpIdx = 0; inpIdx < MAX_INPUTS; ++inpIdx)
	{
		const u32 mapperId = ms_settings.m_InputMappers[inpIdx];
		if(mapperId != DEPRECATED_MAPPER && controlVerifyf(mapperId >= 0 && mapperId < MAPPERS_MAX, "Invalid mapper %d!", mapperId))
		{
			const InputType input = static_cast<InputType>(inpIdx);
			GetSpecificInputSources(input, sources, ioSource::IOMD_KEYBOARD_MOUSE);
			for(u32 srcIdx = 0; srcIdx < sources.size(); ++srcIdx)
			{
				bool hasConflict = GetConflictingInputData(input, sources[srcIdx], false, tmpData, assertOnConflict);
				s32 conflictIndex = GetConflictIndex(tmpData.m_Conflicts, input);
				if( hasConflict && (conflictIndex == -1 || srcIdx != tmpData.m_Conflicts[conflictIndex].m_MappingIndex))
				{
					conflictingInputs.PushAndGrow(input);

					for(u32 i = 0; i < tmpData.m_Conflicts.size(); ++i)
					{
						conflictingInputs.PushAndGrow(tmpData.m_Conflicts[i].m_Input);
					}
				}

				// reset
				tmpData.Clear();
			}
			sources.Reset();
		}
	}

	// remove duplicates
	for(int i=0; i < conflictingInputs.GetCount(); ++i)
	{
		int j=conflictingInputs.Find(conflictingInputs[i], i+1);
		while(j!=-1)
		{
			conflictingInputs.DeleteFast(j);
			j=conflictingInputs.Find(conflictingInputs[i], j);
		}
	}
}
void CControl::LoadDeviceControls(bool loadAutosave)
{
#if RSG_PC
	sysCriticalSection lock(m_Cs);

	INIT_PARSER;

	parSettings settings;
	// we want to detect errors when parsing the xml.
	settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true);

	// Cache the user settings diretory so we do not keep calling the dll.
	char userSettingsDir[PATH_BUFFER_SIZE] = {0};
	GetUserSettingsPath(userSettingsDir);

	char filePath[PATH_BUFFER_SIZE] = {0};
	for(s32 i = 0; i < ioJoystick::GetStickCount(); ++i)
	{
		CPad& pad = CControlMgr::GetPad(i);

		// if the pad is not connected then it is a DirectInput Device. If it is connected check that is is not using pad
		// rather than direct input mappings.
		if( !pad.IsConnected() || (!pad.IsXBox360CompatiblePad() && !pad.IsPs3Pad()) )
		{
			const ioJoystick& stick = ioJoystick::GetStick(i);
			bool mappingsLoaded = false;

			if(!loadAutosave || !ASSET.Exists(formatf(filePath, "%s%s.%s", userSettingsDir, stick.GetProductGuidStr(), AUTOSAVE_EXT), ""))
			{
				formatf(filePath, "%s%s.%s", userSettingsDir, stick.GetProductGuidStr(), USER_SETTINGS_EXT);
			}

			if(ASSET.Exists(filePath, ""))
			{
				ControlInput::DeviceSettings deviceSettings;
				controlDisplayf("Loading user device file '%s' for device!", filePath);
				if(controlVerifyf(PARSER.LoadObject(filePath, "", deviceSettings, &settings), "Failed to load device settings file '%s'!", filePath))
				{
					LoadSettings(deviceSettings.m_ControlSettings, i);
					mappingsLoaded = true;
				}
			}

			if(!mappingsLoaded)
			{
				const ControlInput::Gamepad::Definition* definition = NULL;

				// If there is a calibrated gamepad (joystick definition) in memory then use that.
				if(m_CurrentJoystickDefinition.m_Guid == atFinalHashString(stick.GetProductGuidStr()) && m_CurrentJoystickDefinition.m_Data.m_Definitions.size() > 0)
				{
					controlDisplayf("Using currently loaded gamepad definition data for device!");
					definition = &m_CurrentJoystickDefinition.m_Data;
				}
				else
				{
					controlDisplayf("Using gamepad definition data for device!");
					definition = ms_settings.GetValidGamepadDefinition(stick);
				}

				if(definition != NULL)
				{
					LoadGamepadControls(*definition, m_GamepadCommonSettings, i);

#if FPS_MODE_SUPPORTED
					if(IsUsingFpsMode())
					{
						LoadGamepadControls(*definition, m_GamepadFpsBaseLayoutSettings, i);
						LoadGamepadControls(*definition, m_GamepadFpsSpecificLayoutSettings, i);
					}
					else
#endif // FPS_MODE_SUPPORTED
					{
						LoadGamepadControls(*definition, m_GamepadTpsBaseLayoutSettings, i);
						LoadGamepadControls(*definition, m_GamepadTpsSpecificLayoutSettings, i);
					}

					LoadGamepadControls(*definition, m_GamepadAcceptCancelSettings, i);
					LoadGamepadControls(*definition, m_GamepadDuckBrakeSettings, i);

					mappingsLoaded = true;
				}
			}


			if(!mappingsLoaded && ASSET.Exists(formatf(filePath, "%s%s.%s", PC_DEVICE_SETTINGS_DIR, stick.GetProductGuidStr(), DEFAULT_SETTINGS_EXT), ""))
			{
				ControlInput::DeviceSettings deviceSettings;
				controlDisplayf("Loading default device file '%s' for device!", filePath);
				if(controlVerifyf(PARSER.LoadObject(filePath, "", deviceSettings, &settings), "Failed to load device settings file '%s'!", filePath))
				{
					LoadSettings(deviceSettings.m_ControlSettings, i);
					mappingsLoaded = true;
				}
			}
		}
	}

	SHUTDOWN_PARSER;

#endif // RSG_PC
}


void CControl::ScanGamepadDefinitionSource(GamepadDefinitionSource source)
{
	if(Verifyf(source >= 0 && source < NUM_PAD_SOURCES, "Invalid GampadDefinitionSource (%d)!", source))
	{
		m_CurrentJoystickDefinition.m_PadParameter = ConvertGamepadInputToParameter(source);
	}
	else
	{
		// Cancel any existing mappings just in case.
		CancelGampadDefinitionSourceScan();
	}
}

bool CControl::ClearGamepadDefinitionSource(GamepadDefinitionSource source)
{
	ioMapperParameter param = ConvertGamepadInputToParameter(source);

	if(controlVerifyf(param != IOM_UNDEFINED, "Invalid GamepadDefinitionSource %d", source))
	{
		for(u32 i = 0; i < m_CurrentJoystickDefinition.m_Data.m_Definitions.size(); ++i)
		{
			if(m_CurrentJoystickDefinition.m_Data.m_Definitions[i].m_PadParameter == param)
			{
				m_CurrentJoystickDefinition.m_Data.m_Definitions.DeleteFast(i);
				return true;
			}
		}
	}

	return false;
}

void CControl::CancelGampadDefinitionScan()
{
	CancelGampadDefinitionSourceScan();
	m_CurrentJoystickDefinition.m_Data.m_Definitions.Reset();
}

void CControl::CancelGampadDefinitionSourceScan()
{
	m_CurrentJoystickDefinition.m_PadParameter = IOM_UNDEFINED;
	m_CurrentJoystickDefinition.m_ScanStarted  = false;
	m_CurrentJoystickDefinition.m_ScanReady    = false;
	ClearConflictingDefinitionSource();
}

bool CControl::IsGampadDefinitionScanInProgress() const
{
	return m_CurrentJoystickDefinition.m_PadParameter != IOM_UNDEFINED;
}

bool CControl::IsValidJoystickDevice(s32 deviceId) const
 {
	return deviceId >= 0 && deviceId < ioJoystick::GetStickCount() && CControlMgr::GetPad(deviceId).IsXBox360CompatiblePad();
}

const ioMapperParameter CControl::ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM[] =
{
	RDOWN, RRIGHT, RLEFT, RUP,
	L1, L2, R1, R2,
	START, SELECT,
	LUP, LDOWN, LLEFT, LRIGHT,
	L3, IOM_AXIS_LY_UP, IOM_AXIS_LX_RIGHT,
	R3, IOM_AXIS_RY_UP, IOM_AXIS_RX_RIGHT,
};

CControl::GamepadDefinitionSource CControl::ConvertParameterToGampadInput( ioMapperParameter param )
{
	// This is here as ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM is private.
	CompileTimeAssert(NUM_PAD_SOURCES == COUNTOF(ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM));

	// Convert pad index to pad button type first.
	if((param & IOMT_PAD_INDEX) == IOMT_PAD_INDEX)
	{
		param = static_cast<ioMapperParameter>((param & ~IOMT_PAD_INDEX) | IOMT_PAD_BUTTON);
	}

	for(u32 i = 0; i < NUM_PAD_SOURCES; ++i)
	{
		if(ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM[i] == param)
		{
			return static_cast<GamepadDefinitionSource>(i);
		}
	}
	return INVALID_PAD_SOURCE;
}

rage::ioMapperParameter CControl::ConvertGamepadInputToParameter( GamepadDefinitionSource input )
{
	if(Verifyf(input >= 0 && input < NUM_PAD_SOURCES, "Invalid GamepadDefinitionSource (%d)!", input))
	{
		return ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM[input];
	}
	else
	{
		return IOM_UNDEFINED;
	}
}

void CControl::StartGamepadDefinitionScan()
{
	m_CurrentJoystickDefinition.m_Data.m_Definitions.Reset();
	ClearConflictingDefinitionSource();
	m_CurrentJoystickDefinition.m_PadParameter = IOM_UNDEFINED;
}

void CControl::EndGamepadDefinitionScan(bool saveGamepadDefinition)
{
	if(saveGamepadDefinition)
	{
		char path[PATH_BUFFER_SIZE] = {0};
		GetUserSettingsPath(path, PC_USER_GAMEPAD_DEFINITION_FILE);
		controlDisplayf("Saving gamepad definition to '%s'!", path);
		controlVerifyf( SaveCurrentJoystickDefinition(path),
						"Failed to save gamepad definition file ('%s')!",
						path );

		ms_settings.RebuildGamepadDefinitionList();
	}
	else
	{
		controlDisplayf("Cancelling gamepad definition scan!");
	}

	CancelGampadDefinitionScan();
	m_CurrentJoystickDefinition.m_Guid = 0;
	SetInitialDefaultMappings(true);
}

bool CControl::SaveCurrentJoystickDefinition( const char* userFilePath ) const
{
	bool saved = false;

	// don't save empty definitions
	if(m_CurrentJoystickDefinition.m_Data.m_Definitions.size() > 0 && controlVerifyf(m_CurrentJoystickDefinition.m_Guid != 0, "Invalid GUID for gamepad!"))
	{
		// first we need to load the user file so we do not overwrite other device settings.
		INIT_PARSER;
		ControlInput::Gamepad::DefinitionList definitions;

		parSettings settings;
		// we want to detect errors when parsing the xml.
		settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true);

		// If the file does not exist then it is still ok to continue.
		if(ASSET.Exists(userFilePath, ""))
		{
			controlVerifyf(PARSER.LoadObject(userFilePath, "", definitions, &settings), "Error loading joystick definition if file '%s'", userFilePath);
		}

		definitions.m_Devices[m_CurrentJoystickDefinition.m_Guid] = m_CurrentJoystickDefinition.m_Data;

		ASSET.CreateLeadingPath(userFilePath);
		saved = PARSER.SaveObjectAnyBuild(userFilePath, "", &definitions);
		controlAssertf(saved, "Error saving joystick definition file '%s'", userFilePath);

		SHUTDOWN_PARSER;
	}
	else
	{
		// treat empty definitions as a success.
		saved = true;
	}

	return saved;
}

bool CControl::LoadCurrentJoystickDefinition( const char* userFilePath )
{
	bool found = false;

	const ControlInput::Gamepad::Definition* definition = NULL;

	m_CurrentJoystickDefinition.m_Guid = ioJoystick::GetStick(m_CurrentJoystickDefinition.m_DeviceIndex).GetProductGuidStr();
	m_CurrentJoystickDefinition.m_Data.m_Definitions.clear();

	// do not assert if the file does not exist.
	char defaultUserGampadPath[PATH_BUFFER_SIZE] = {0};
	GetUserSettingsPath(defaultUserGampadPath, PC_USER_GAMEPAD_DEFINITION_FILE);
	if(ASSET.Exists(userFilePath, "") && strcasecmp(userFilePath, PC_GAMEPAD_DEFINITION_FILE) != 0 && strcasecmp(userFilePath, defaultUserGampadPath) != 0)
	{
		// first we need to load the user file so we do not overwrite other device settings.
		INIT_PARSER;

		parSettings settings;
		// we want to detect errors when parsing the xml.
		settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true);

		// If the default settings file exists and we FAIL to load it.
		ControlInput::Gamepad::DefinitionList definitions;
		if( controlVerifyf(PARSER.LoadObject(userFilePath, "", definitions, &settings), "Error loading joystick definition file '%s'!", userFilePath) )
		{
			definition = definitions.m_Devices.Access(m_CurrentJoystickDefinition.m_Guid);
		}

		SHUTDOWN_PARSER;
	}

	if(definition == NULL)
	{
		definition = ms_settings.GetGamepadDefinition(ioJoystick::GetStick(m_CurrentJoystickDefinition.m_DeviceIndex));
	}

	if(definition != NULL)
	{
		found = true;
		m_CurrentJoystickDefinition.m_Data = *definition;
	}

	BANK_ONLY(UpdateJoystickDefinitionRagData());

	return found;
}

void CControl::UpdateCurrentGamepadDefinition( const ControlInput::Gamepad::Source& definition )
{
	bool updated = false;

	for(u32 i = 0; !updated && i < m_CurrentJoystickDefinition.m_Data.m_Definitions.size(); ++i)
	{
		if(m_CurrentJoystickDefinition.m_Data.m_Definitions[i].m_PadParameter == definition.m_PadParameter)
		{
			updated = true;
			m_CurrentJoystickDefinition.m_Data.m_Definitions[i] = definition;
		}
	}

	if(!updated)
	{
		// add definition.
		m_CurrentJoystickDefinition.m_Data.m_Definitions.PushAndGrow(definition);
	}
}

CControl::GamepadDefinitionSource CControl::GetCurrentGamepadDefinition(const ControlInput::Gamepad::Source& definition ) const
{
	for(u32 i = 0; i < m_CurrentJoystickDefinition.m_Data.m_Definitions.size(); ++i)
	{
		const ControlInput::Gamepad::Source& current = m_CurrentJoystickDefinition.m_Data.m_Definitions[i];

		// If the current is mapped to the same button/stick as definition && (current is not a trigger || definition is not a trigger)
		if( current.m_JoystickParameter == definition.m_JoystickParameter &&
			((current.m_PadParameter != L2 && current.m_PadParameter != L2_INDEX && current.m_PadParameter != R2 && current.m_PadParameter != R2_INDEX) ||
			(definition.m_PadParameter != L2 && definition.m_PadParameter != L2_INDEX && definition.m_PadParameter != R2 && definition.m_PadParameter != R2_INDEX)) )
		{
			return ConvertParameterToGampadInput(current.m_PadParameter);
		}
	}

	return INVALID_PAD_SOURCE;
}

bool CControl::IsGamepadDefinitionValid( const ControlInput::Gamepad::Source& definition ) const
{
	// If definition.m_padParameter and definition.m_JoystickParameter are the same thing (axis or buttons) or definition.m_PadParameter is a trigger.
	return ((definition.m_PadParameter & IOMT_PAD_AXIS) == IOMT_PAD_AXIS && (definition.m_JoystickParameter & IOMT_JOYSTICK_AXIS) == IOMT_JOYSTICK_AXIS) ||
		   ((definition.m_PadParameter & IOMT_PAD_AXIS) == 0 && (definition.m_JoystickParameter & IOMT_JOYSTICK_AXIS) == 0 ) ||
		   definition.m_PadParameter == L2 || definition.m_PadParameter == L2_INDEX || definition.m_PadParameter == R2 || definition.m_PadParameter == R2_INDEX;
}

#endif // __WIN32PC

#if !__FINAL
void CControl::ChanneledOutputForStackTrace(const char* fmt, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vformatf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	ms_ThreadlStack += buffer;
	ms_ThreadlStack += "\n";
}

#endif // !__FINAL
