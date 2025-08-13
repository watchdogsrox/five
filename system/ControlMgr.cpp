//
// name:		controlmgr.cpp
// description:	class that encapsulates all game controllers (pad, mouse, keyboard)

// Rage headers
#include "bank/io.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "grcore/device.h"
#include "input/headtracking.h"
#include "profile/telemetry.h"
#include "security/yubikey/yubikeymanager.h"
#include "system/exception.h"
#include "system/ipc.h"
#include "system/param.h"
#include "system/userlist.h"

// game headers
#include "core/game.h"
#include "SaveLoad/savegame_autoload.h"
#include "script/script_debug.h"	//	for CScriptDebug::GetDisableDebugCamAndPlayerWarping()
#include "system/controlmgr.h"
#include "debug/debug.h"
#include "Frontend/NewHud.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/WarningScreen.h" 
#include "Peds/Ped.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/gameWorld.h"
#include "script/script_debug.h"
#include "text/messages.h"
#include "Network/live/livemanager.h"
#include "Network/NetworkInterface.h"
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "Network/NetworkInterface.h"
#include "system/system.h"
#include "control/replay/replay.h"
#include "control/videorecording/videorecording.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "text/TextFormat.h"

#if RSG_PC
#include "frontend/TextInputBox.h"
#include "rline/rlpc.h"
#endif // RSG_PC

#if RSG_DURANGO
#include "rline/durango/rlxbl_interface.h"
#endif

#if __BANK
#include "pathserver/PathServer.h"
#include "Cutscene/CutSceneManagerNew.h"
#endif

// static member variables
sysIpcThreadId CControlMgr::m_updateThreadId;
rage::sysIpcCurrentThreadId CControlMgr::sm_ThreadId = sysIpcCurrentThreadIdInvalid;
sysIpcSema CControlMgr::m_runUpdateSema;
sysIpcSema CControlMgr::m_updateFinishedSema;
sysCriticalSectionToken CControlMgr::m_Cs;
CPad CControlMgr::m_pads[NUM_PADS];
CPad CControlMgr::m_debugPad;
CKeyboard CControlMgr::m_keyboard;
CControl CControlMgr::m_controls[MAX_CONTROL];
//u32 CControlMgr::ms_disablePlayerControls;	moved to CPlayerInfo
//eCONTROL_METHOD CControlMgr::m_ControlMethod;
s32 CControlMgr::sm_PlayerPadIndex = -1;
s32 CControlMgr::sm_DebugPadIndex = -1;
bool CControlMgr::sm_bNoPadMessageActive = false;
bool volatile CControlMgr::sm_bShutdownThreads = false;


bank_u32 CControlMgr::sm_InputDisableDuration = 500U;

#if DEBUG_PAD_INPUT
PARAM(showDebugPadValues, "Shows the debug pad values.");
bool CControlMgr::sm_bDebugPadValues = false;
#endif

#if __BANK

s32  CControlMgr::sm_nSaveAsDefaultNum = CONTROLS_CONFIG_DEFAULT;
u32 CControlMgr::sm_iShakeDuration0 = 0;
u32 CControlMgr::sm_iShakeDuration1 = 0;
u32 CControlMgr::sm_iShakeFrequency0 = 0;
u32 CControlMgr::sm_iShakeFrequency1 = 0;
s32  CControlMgr::sm_iShakeDelayAfterThisOne = 0;
float  CControlMgr::sm_fMultiplier = 0.0f;
bool   CControlMgr::sm_bOverrideShakeFreq0 = false;
bool   CControlMgr::sm_bOverrideShakeFreq1 = false;
bool   CControlMgr::sm_bOverrideShakeDur0 = false;
bool   CControlMgr::sm_bOverrideShakeDur1 = false;
bool   CControlMgr::sm_bOverrideShakeDelay = false;
bool   CControlMgr::sm_bUseThisMultiplier = false;
u32 CControlMgr::sm_iDebugShakeDuration = 500;
u32 CControlMgr::sm_iDebugMotorToShake = 0;
bool CControlMgr::sm_bDebugShakeMotor = false;
float  CControlMgr::sm_fDebugShakeIntensity = 0.5f;
s32  CControlMgr::sm_iHardcodedMappingIndex = 0;
char CControlMgr::sm_CustomInputMetaFilePath[RAGE_MAX_PATH];
char CControlMgr::sm_DefaultVirtualKeyboardText[MAX_KEYBOARD_TEXT] = "Default Text";
int  CControlMgr::sm_VirtualKeyboardTextLimit = ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD;
s32  CControlMgr::sm_KeyboardMode = 0;
bool CControlMgr::sm_bCheckKeyboard = false;
#endif  //__BANK

// static variables
bool CControlMgr::sm_bDebugPad = false;
Functor0 CControlMgr::sm_initProfileFn = MakeFunctor(Functor0::NullFn);
Functor0 CControlMgr::sm_shutdownProfileFn = MakeFunctor(Functor0::NullFn);

bool CControlMgr::sm_bShakeWhenControllerDisabled = false;

#if RSG_DURANGO
sysCriticalSectionToken CControlMgr::sm_ConstrainedCs;
ServiceDelegate CControlMgr::sm_Delegate;
bool CControlMgr::sm_IsGameConstrained = false;
#endif // RSG_DURANGO

#if DEBUG_DRAW
PARAM(debugInputs, "Shows input values and disable state.");
CControlMgr::InputDisplayType CControlMgr::sm_eInputDisplayType = CControlMgr::IDT_NONE;

#if __BANK
bool CControlMgr::sm_bPrintToGameplayLog = false;
const char* CControlMgr::sm_InputDislayTypeNames[IDT_AMOUNT] = { "Do not show", "Show all inputs", "Show non-zero inputs", "Show disabled inputs" };
#endif // __BANK

float CControlMgr::sm_InputDisplayScale = 1.0f;
float CControlMgr::sm_InputDisplayX = 0.05f;
s32  CControlMgr::sm_InputDislayMaxLength = 0;
#endif // DEBUG_DRAW

#if !__FINAL
bool CControlMgr::sm_bAllowTildeCharacterFromVirtualKeyboard = false;
#endif	//	!__FINAL

s32 CControlMgr::sm_fontBitField = 0;

s32 gPadNumberToDebug = 0;
bool gbShowDebugKeys = false;
bool gbShowLevelDebugKeys = false;
bool gSceneRenderingPaused = false;

ioVirtualKeyboard CControlMgr::sm_VirtualKeyboard;
char CControlMgr::sm_VirtualKeyboardResult[ioVirtualKeyboard::VIRTUAL_KEYBOARD_RESULT_BUFFER_SIZE];
bool CControlMgr::sm_bVirtualKeyboardIsEnteringAPassword = false;

#if OVERLAY_KEYBOARD
bool CControlMgr::sm_bUsingOverlayKeyboard = false;
#endif // OVERLAY_KEYBOARD

NOSTRIP_PC_PARAM(mouseexclusive, "Game uses mouse exclusively.");
NOSTRIP_PC_XPARAM(streamingbuild);

#if !__FINAL
PARAM(usecontroller, "Forces a specific controler to be used.");
PARAM(disallowdebugpad, "Don't assume the next pad that has input is the debug pad.");

#endif  //__FINAL

#define TIMESTEP_BETWEEN_DEVICE_CHECK 1000

CPlayerInfo* GetLocalPlayerInfo()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayerSafe();
	return (pPlayer ? pPlayer->GetPlayerInfo() : NULL);
}
// some functions to save and load defaults for debug:
#if __BANK



//
// name:		SaveAsDefaultsButton
// description:	saves the defaults from the bank button using the current default number
void CControlMgr::SaveAsDefaultsButton()
{
	char cDefaultsFilename[255];

	sprintf(cDefaultsFilename, "%s%d", STANDARD_DEFAULT_CONTROLS_FILENAME, CControlMgr::sm_nSaveAsDefaultNum);

	GetControl(CONTROL_MAIN_PLAYER).SaveControls(cDefaultsFilename, CONTROLS_VERSION_NUMBER);
}

//
// name:		LoadDefaultsButton
// description:	loads the defaults from the bank button using the current default number
void CControlMgr::LoadDefaultsButton()
{
	char cDefaultsFilename[255];

	sprintf(cDefaultsFilename, "%s%d", STANDARD_DEFAULT_CONTROLS_FILENAME, CControlMgr::sm_nSaveAsDefaultNum);

	GetControl(CONTROL_MAIN_PLAYER).LoadControls(cDefaultsFilename, CONTROLS_VERSION_NUMBER);  // load the controls
	GetControl(CONTROL_DEBUG).LoadControls(cDefaultsFilename, CONTROLS_VERSION_NUMBER);  // load the controls
}

#endif //__BANK


//
// name:		Init
// description:	Initialise class
void CControlMgr::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
	{
	    // Init Rage classes
		bool inWindow = CSystem::InWindow();
		::rage::INPUT.Begin(inWindow, PARAM_mouseexclusive.Get());

        sm_PlayerPadIndex = -1;
		sm_bShutdownThreads = false;

		OVERLAY_KEYBOARD_ONLY(sm_bUsingOverlayKeyboard = false);

	    for(s32 i=0; i<NUM_PADS; i++)
	    {
		    m_pads[i].SetPlayerNum(i);
		    m_pads[i].Clear();
	    }
	    m_debugPad.Clear();

	    m_controls[CONTROL_MAIN_PLAYER].Init(0);
		WIN32PC_ONLY(m_controls[CONTROL_MAIN_PLAYER].SetEnableDirectInputDevices(true));
	    m_controls[CONTROL_EMPTY].Init(0);
#if ENABLE_DEBUG_CONTROLS
	    m_controls[CONTROL_DEBUG].Init(0);
#endif // ENABLE_DEBUG_CONTROLS 

	    m_controls[CONTROL_EMPTY].InitEmptyControls();		// Set controls as if no input has happened. (Used for when controls disabled)  Only needs to do this once

    //	m_ControlMethod = CONTROL_METHOD_JOYPAD;  // joypad as default

	    m_keyboard.Init();

    //	ms_disablePlayerControls = false; now in CPlayerInfo
	    bkIo::SetPadIndex(1);

	    sm_bShakeWhenControllerDisabled = false;

	    // Set player controller to pad 0 on PS3
#if __PS3
	    SetMainPlayerIndex(0);
#endif

#if DEBUG_PAD_INPUT
		if(PARAM_showDebugPadValues.Get())
		{
			sm_bDebugPadValues = true;
		}
		else
		{
			sm_bDebugPadValues = false;
		}
#endif

#if DEBUG_DRAW
		// determine the longest input name for displaying on the screen.
		for(s32 i = 0; i < MAX_INPUTS; ++i)
		{
			const char* name = m_controls[CONTROL_MAIN_PLAYER].GetInputName(static_cast<InputType>(i));

			s32 len = static_cast<s32>(strlen(name));

			// We are ignoring the INPUT_. We don't assume it starts with this.
			if(strncmp(name, "INPUT_", 6) == 0)
			{
				len -= 6;
			}

			// add space for the value (-nn.nn) pus a space at the end.
			len += 8;

			if(len > sm_InputDislayMaxLength)
			{
				sm_InputDislayMaxLength = len;
			}
		}

		s32 displayType = CControlMgr::IDT_NONE;
		if(PARAM_debugInputs.Get(displayType))
		{
			if(displayType >= 0 && displayType < CControlMgr::IDT_AMOUNT)
			{
				sm_eInputDisplayType = static_cast<InputDisplayType>(displayType);
			}
		}

#endif // DEBUG_DRAW
		m_runUpdateSema = sysIpcCreateSema(0);
		m_updateFinishedSema = sysIpcCreateSema(0);
		m_updateThreadId = sysIpcCreateThread(&*CControlMgr::UpdateThread, NULL, 32*1024, PRIO_HIGHEST, "[GTA] ControlMgr Update Thread", 2, "ControlMgrUpdateThread");

#if RSG_DURANGO
		sm_Delegate.Bind(&CControlMgr::HandleConstrainedChanged);
		g_SysService.AddDelegate(&sm_Delegate);
#endif // RSG_DURANGO

#if RAGE_SEC_ENABLE_YUBIKEY
		CYubiKeyManager::Create();
#endif
    }
    else if(initMode == INIT_SESSION)
    {
        // ensure controller 0 is used if it hasn't been set
	    if(GetMainPlayerIndex() == -1)
		    SetMainPlayerIndex(0);

	    // Make sure to clear any old input data history.
	    GetMainPlayerControl().InitEmptyControls();
    }

	for(u32 i = 0; i < MAX_CONTROL; ++i)
	{
		m_controls[i].Init(initMode);
	}
}

void CControlMgr::ReInitControls()
{
	m_controls[CONTROL_MAIN_PLAYER].SetInitialDefaultMappings(false);
#if KEYBOARD_MOUSE_SUPPORT
#if __BANK
	// in bank mode we can disable pc input to better work with rag.
	if(!PARAM_noKeyboardMouseControls.Get())
#endif // __BANK
	{
		m_controls[CONTROL_MAIN_PLAYER].ResetKeyboardMouseSettings();
	}
#endif
	m_controls[CONTROL_EMPTY].ClearInputMappings();
#if ENABLE_DEBUG_CONTROLS
	m_controls[CONTROL_DEBUG].ClearInputMappings();
#endif //ENABLE_DEBUG_CONTROLS
}

void CControlMgr::SetDefaultLookInversions()
{
	m_controls[CONTROL_MAIN_PLAYER].SetDefaultLookInversions();
	m_controls[CONTROL_EMPTY].SetDefaultLookInversions();
#if ENABLE_DEBUG_CONTROLS
	m_controls[CONTROL_DEBUG].SetDefaultLookInversions();
#endif //ENABLE_DEBUG_CONTROLS
}

#if __BANK
void CControlMgr::LoadCustomControls()
{
	m_controls[CONTROL_MAIN_PLAYER].SetInitialDefaultMappings(false, false, sm_CustomInputMetaFilePath);
	m_controls[CONTROL_EMPTY].ClearInputMappings();
#if ENABLE_DEBUG_CONTROLS
	m_controls[CONTROL_DEBUG].ClearInputMappings();
#endif //ENABLE_DEBUG_CONTROLS
}
#endif // __BANK

//
// name:		Shutdown
// description:	Shutdown class
void CControlMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if RSG_DURANGO
		g_SysService.RemoveDelegate(&sm_Delegate);
		sm_Delegate.Reset();
#endif // RSG_DURANGO

	    // Shutdown Rage classes
		::rage::INPUT.End();

		m_keyboard.Shutdown();

		sm_bShutdownThreads = true;

		// Wake up the control update thread to exit.
		sysIpcSignalSema(m_runUpdateSema);
    }
}

void CControlMgr::StartUpdateThread()
{
	PF_AUTO_PUSH_TIMEBAR("CControlMgr::StartUpdateThread");
	sysIpcSignalSema(m_runUpdateSema);
}

void CControlMgr::WaitForUpdateThread()
{
	PF_AUTO_PUSH_TIMEBAR("CControlMgr::WaitForUpdateThread");
	sysIpcWaitSema(m_updateFinishedSema);

	// Check for pause when the console menu is up (not in multiplayer)
	// NOTE:	The PS4 can tell us our pad is intercepted if the user manually switches off the pad.
	//			We don't want to pause in this situation when exporting a video (url:bugstar:2140784).
	if( (CPad::IsIntercepted() VIDEO_RECORDING_ENABLED_ONLY(&& !VideoRecording::IsRecording()))
		|| CLiveManager::IsSystemUiShowing()
#if !RSG_PC
		// Do not pause when we are recording a video otherwise we could corrupt it (url:bugstar:2140784).
			|| (GetPlayerPad() && GetPlayerPad()->IsConnected() == false VIDEO_RECORDING_ENABLED_ONLY(&& !VideoRecording::IsRecording()))
#endif // !RSG_PC
		DURANGO_ONLY(|| IsConstrained()) )
	{
		for(s32 i = 0; i < MAX_CONTROL; ++i)
		{
			m_controls[i].DisableAllInputs(sm_InputDisableDuration);
		}

		if(NetworkInterface::CanPause()
#if RSG_PC
			REPLAY_ONLY(&& !CReplayCoordinator::IsExportingToVideoFile())
#endif
			)
		{
			if( !fwTimer::IsSystemPaused() )
			{
#if !__NO_OUTPUT
				Displayf("CControlMgr::WaitForUpdateThread - about to call fwTimer::StartSystemPause()");
				Displayf("CPad::IsIntercepted() = %s", CPad::IsIntercepted()?"true":"false");
				Displayf("CLiveManager::IsSystemUiShowing() = %s", CLiveManager::IsSystemUiShowing()?"true":"false");
#if !RSG_PC
				if (GetPlayerPad())
				{
					Displayf("GetPlayerPad()->IsConnected() = %s", GetPlayerPad()->IsConnected()?"true":"false");
				}
				else
				{
					Displayf("GetPlayerPad() returned NULL");
				}
#endif // !RSG_PC
				DURANGO_ONLY(Displayf("IsConstrained() = %s", IsConstrained()?"true":"false");)
				VIDEO_RECORDING_ENABLED_ONLY(Displayf("VideoRecording::IsRecording() = %s", VideoRecording::IsRecording()?"true":"false");)
#endif	//	!__NO_OUTPUT

				fwTimer::StartSystemPause();
			}

			if(!CPauseMenu::GetPauseRenderPhasesStatus() ) 
			{
				CPauseMenu::TogglePauseRenderPhases(false, OWNER_CONTROLMGR, __FUNCTION__ );
				gSceneRenderingPaused = true;
			}
		}
	}
	else
	{
		if( fwTimer::IsSystemPaused() )
		{
			Displayf("CControlMgr::WaitForUpdateThread - about to call fwTimer::EndSystemPause()");
			fwTimer::EndSystemPause();
		}

		if(gSceneRenderingPaused == true && CPauseMenu::GetPauseRenderPhasesStatus() ) 
		{
			CPauseMenu::TogglePauseRenderPhases(true, OWNER_CONTROLMGR, __FUNCTION__ );
			gSceneRenderingPaused = false;
		}
	}
}

//
// name:		SetProfileChangeFunctions
// description:	Setup callback functions for profile changes
void CControlMgr::SetProfileChangeFunctions(Functor0 init, Functor0 shutdown)
{
	sm_initProfileFn = init;
	sm_shutdownProfileFn = shutdown;

	// if the main controller index has already been setup, call the Init function now
	if(sm_PlayerPadIndex >= 0)
	{
#if __XENON
		// on PS3, we expect this to occur since the controller index is set to 0 in the Init call.
		// On 360, this is not expected so log out if we end up here
		Warningf("CControlMgr :: Controller index setup prior to calling SetProfileChangeFunctions!");
#endif
		sm_initProfileFn();
	}
}

//
// name:		CControlMgr::GetNumberOfConnectedPads
// description:	returns the number of pads that are connected to the console
int CControlMgr::GetNumberOfConnectedPads()
{
	int total_number_of_connected_pads = 0;
	for (int pad_loop = 0; pad_loop < NUM_PADS; pad_loop++)
	{
		if (m_pads[pad_loop].IsConnected())
		{
			total_number_of_connected_pads++;
		}
	}

	return total_number_of_connected_pads;
}

//
// name:		CControlMgr::SetMainPlayerControl
// description:	
void CControlMgr::SetMainPlayerIndex(s32 index)
{
	bool newIndex = false;
	
	// has index changed?
	if(sm_PlayerPadIndex != index)
	{
		// did we have a valid profile previously. If so shutdown previous profile
		if(sm_PlayerPadIndex != -1)
			sm_shutdownProfileFn();
		newIndex = true;
		sm_DebugPadIndex = -1;
	}
	sm_PlayerPadIndex = index;
	m_controls[CONTROL_MAIN_PLAYER].SetPad(sm_PlayerPadIndex);
	Assert(sm_PlayerPadIndex < NUM_PADS);
	Displayf("Using controller:%d", sm_PlayerPadIndex);

	// if index change init profile
	if(newIndex)
		sm_initProfileFn();
}

#if RSG_PC

void CControlMgr::UpdateScuiInput()
{
	// SCUI input cannot be updated until the rlPC GamePad Manager is initialized
	if (g_rlPc.GetGamepadManager() == NULL)
		return;

	// Enumerate through our pads
	for (unsigned i = 0; i < NUM_PADS && i < g_rlPc.GetNumScuiPads(); i++)
	{
		rgsc::RgscGamepad* scuiPad = g_rlPc.GetScuiPad(i);
		if (scuiPad == nullptr)
			continue;

		CPad& pad = m_pads[i];

		// Update the SCUI pad's connection state with our own
		scuiPad->SetIsConnected(pad.IsConnected());

		// Only update the pad if it is connected
		if (pad.IsConnected())
		{
			// Pull directly from the underlying pad, since the CPad doesn't update when the SCUI is on screen.
			scuiPad->SetButtons(pad.m_pPad->GetButtons());
			scuiPad->SetAxis(0, (rgsc::u8)pad.m_pPad->GetLeftX());
			scuiPad->SetAxis(1, (rgsc::u8)pad.m_pPad->GetLeftY());
			scuiPad->SetAxis(2, (rgsc::u8)pad.m_pPad->GetRightX());
			scuiPad->SetAxis(3, (rgsc::u8)pad.m_pPad->GetRightY());
			scuiPad->SetHasInput();
		}
	}
}
#endif // RSG_PC
//
// name:		Update
// description:	Update class
void CControlMgr::Update()
{
	sysCriticalSection lock(m_Cs);

#if RSG_PC
	// If the controls are not being updated on the control update thread then we will need to re-capture devices.
	if(sm_ThreadId == sysIpcCurrentThreadIdInvalid || sm_ThreadId != sysIpcGetCurrentThreadId())
	{
		WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::Update() - RecaptureLostDevices()"));
		WIN32PC_ONLY(ioInput::RecaptureLostDevices());
		WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));
	}

	if(PARAM_streamingbuild.Get())
	{
		SetMainPlayerIndex(0);
	}
#endif // RSG_PC

	PF_PUSH_TIMEBAR("INPUT.Update");
	WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateInputDevices() - ::rage::INPUT.Update"));
	
	bool bIgnoreInput = RSG_PC ? (CLiveManager::IsInitialized() && CLiveManager::IsSystemUiShowing()) : false;

#if RSG_PC
	// On PC, we need to update padInput because the SCUI uses it even when on screen.
	// Otherwise, ignore mouse and keyboard when the SCUI is up.
	::rage::INPUT.Update(false, bIgnoreInput, bIgnoreInput, !__WIN32PC);

	// Hack to get debug cameras working with PS4 pads for working from home.
	NOTFINAL_ONLY(CControl::EmulateXInputPads());
#else
	// On consoles, we don't ignore input
	::rage::INPUT.Update();
#endif

#if RAGE_SEC_ENABLE_YUBIKEY
	bool listenForYubiKeyInput = UpdateYubikey();
#endif

	WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));
	PF_POP_TIMEBAR();

	// TR: TODO: Remove this as it is better to call this from the user/network code.
	sysUserList::GetInstance().Update();

	if(GetMainPlayerIndex() >= 0)
	{
		sysUserList::GetInstance().UpdateUserInfo( static_cast<u32>(GetMainPlayerIndex()) );
	}

	PF_PUSH_TIMEBAR("Pad.Update");
	WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateControls() - Update Pads"));
	if(g_PlayerSwitch.IsActive())
	{
		StopPadsShaking();
	}

	for(s32 i=0; i<NUM_PADS; i++)
	{
		// On PC, we ignore input (i.e. controller state) while the system UI is showing. 
		// However, we want to make sure pad vibrations continue to process, so they don't run 
		//	indefinitely while the SCUI is on screen.
		if (!bIgnoreInput)
		{
			m_pads[i].Update();
		}
		else
		{
			m_pads[i].UpdateActuatorsAndShakes();
		}		
	}

	WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));
	PF_POP_TIMEBAR();

#if RSG_PC
	PF_PUSH_TIMEBAR("ScuiInput.Update");
	UpdateScuiInput();
	PF_POP_TIMEBAR();
#endif

	PF_PUSH_TIMEBAR("Keyboard.Update");
	WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateControls() - m_keyboard.Update"));
	m_keyboard.Update();
	WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Player/Update");
#if !__FINAL
	if(sm_PlayerPadIndex < 0)
	{
        int ctrlIdx;
        if(PARAM_usecontroller.Get(ctrlIdx))
        {
			SetMainPlayerIndex(ctrlIdx);
        }
    }
#endif  //__FINAL

	// If we don't have a player pad then wait until we get input
	if(sm_PlayerPadIndex < 0)
	{
		for(s32 i=0; i<NUM_PADS; i++)
		{
#if RSG_DURANGO
			bool bIsSignedIn = rlPresence::IsSignedIn(i);
			bool bIsSignedInAsGuest = rlPresence::IsSignedIn(i) && g_rlXbl.IsInitialized() && rlPresence::IsGuest(i);

			// Guest accounts are not valid controllers for the main player index
			if(m_pads[i].HasInput() && g_rlXbl.IsInitialized() && (!bIsSignedInAsGuest || !bIsSignedIn))
#else
			if(m_pads[i].HasInput())
#endif
			{
				SetMainPlayerIndex(i);
				break;
			}
		}
	}

    if(sm_PlayerPadIndex >= 0 NOTFINAL_ONLY(&& !PARAM_disallowdebugpad.Get()))
    {
#if ENABLE_DEBUG_CONTROLS
	    //if we have a player pad but not a debug pad then the next pad that
        //has input is the debug pad
	    if(sm_DebugPadIndex < 0)
	    {
		    for(s32 i=0; i<NUM_PADS; i++)
		    {
			    if(i != sm_PlayerPadIndex && m_pads[i].HasInput())
			    {
				    sm_DebugPadIndex = i;
				    m_debugPad.SetPlayerNum(sm_DebugPadIndex);
				    m_controls[CONTROL_DEBUG].SetPad(sm_DebugPadIndex);
                    bkIo::SetPadIndex(sm_DebugPadIndex);
				    break;
			    }
		    }
	    }
#endif
    }
	PF_POP_TIMEBAR();

	// switch off all vibration if player controls are disabled/lost
	PF_PUSH_TIMEBAR("StopPadsShaking");
	if (CControlMgr::ShouldStopPadsShaking())
		StopPadsShaking();
	PF_POP_TIMEBAR();

	// TODO: This is a check that should not exist really because who ever sets this flag should unset it.!!
	CPlayerInfo* pPlayerInfo = GetLocalPlayerInfo();
	if (pPlayerInfo && !pPlayerInfo->AreControlsDisabled())
		sm_bShakeWhenControllerDisabled = false;

	PF_PUSH_TIMEBAR("Debug.Update");
	// This is able debug input will we are listening for Yubikey input, as that is random keyboard input
	// it can trigger debug input, that makes it go in to strange states. (this just disabling it at startup, on Yubikey builds)
#if RAGE_SEC_ENABLE_YUBIKEY
	if (!listenForYubiKeyInput)
	{
#endif
#if ENABLE_DEBUG_CONTROLS
#if !__FINAL
	XPARAM(nocheats);
	if (!CScriptDebug::GetDisableDebugCamAndPlayerWarping() && !PARAM_nocheats.Get())	//	For build for Image Metrics
#endif
	{
        if(sm_DebugPadIndex >= 0)
        {
            m_debugPad.Update();
        }
        else if(!sm_bDebugPad)
        {
            m_debugPad.Clear();
        }

		//Allow toggling of the debug pad on Start+Square/X.
		CPad* playerPad = GetPlayerPad();
        const bool toggleDebug	= (m_keyboard.GetKeyJustDown(KEY_D, KEYBOARD_MODE_DEBUG, "Toggle debug pad") ||
									REPLAY_ONLY(m_keyboard.GetKeyJustDown(KEY_D, KEYBOARD_MODE_REPLAY, "Toggle debug pad")  ||)
									(playerPad && (playerPad->GetPressedDebugButtons() & ioPad::RLEFT)));

		if(toggleDebug)
		{
			sm_bDebugPad = !sm_bDebugPad;
#if __BANK
			if(sm_ThreadId != sysIpcCurrentThreadIdInvalid && sysIpcGetCurrentThreadId() == sm_ThreadId && !CSystem::IsThisThreadId(SYS_THREAD_RENDER))
			{
				const char* stringLabel	= sm_bDebugPad ? "HELP_DBGPADON" : "HELP_DBGPADOFF";
				char *string			= TheText.Get(stringLabel);
				CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
			}
#endif // __BANK
		}

	    if(sm_bDebugPad)
	    {
            if(sm_PlayerPadIndex >= 0)
            {
    		    m_debugPad.UpdateFromPad(m_pads[sm_PlayerPadIndex]);
                m_controls[CONTROL_DEBUG].SetPad(sm_PlayerPadIndex);
		        m_controls[CONTROL_DEBUG].Update();  // only update the debug control map
		        bkIo::SetPadIndex(sm_PlayerPadIndex);
	    	    m_pads[sm_PlayerPadIndex].Clear();
            }
	    }
	    else if(toggleDebug && sm_DebugPadIndex >= 0)
        {
            m_controls[CONTROL_DEBUG].SetPad(sm_DebugPadIndex);
	        bkIo::SetPadIndex(sm_DebugPadIndex);
	    }
	}	//	end of !CTheScripts::GetDisableDebugCamAndPlayerWarping()

#if !__FINAL
	if (CScriptDebug::GetDisableDebugCamAndPlayerWarping())	//	For build for Image Metrics
	{	//	just to be sure
		m_debugPad.Clear();
	}
#endif
#endif //ENABLE_DEBUG_CONTROLS
#if RAGE_SEC_ENABLE_YUBIKEY
	}
#endif //RAGE_SEC_ENABLE_YUBIKEY
	PF_POP_TIMEBAR();

#if ENABLE_DEBUG_CONTROLS
	if(!sm_bDebugPad)
#endif
	{
		PF_PUSH_TIMEBAR("controls.Update");
		WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateControls() - controls.Update"));
		// update all main control maps for each player (note we do not update the debug pad here)
		for(s32 i=0; i<NUM_PLAYERS; i++)
		{
			m_controls[i].Update();
		}
		WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));

		PF_POP_TIMEBAR();
	}
	// debug stuff
#if __BANK
	if(sm_bDebugShakeMotor)
	{
		// Directly shake pad 0 as we do not have functions yet to do this at game level.
		ioPad::GetPad(0).SetActuatorValue(sm_iDebugMotorToShake, sm_fDebugShakeIntensity);
	}

	if(m_keyboard.GetKeyJustDown(KEY_K, KEYBOARD_MODE_DEBUG_CTRL, "Toggle display of keys"))
		gbShowDebugKeys = !gbShowDebugKeys;

	if(IsDebugPadValuesOn())
		m_pads[gPadNumberToDebug].PrintDebug();
	if(gbShowDebugKeys)
		m_keyboard.PrintDebugKeys();
	if(gbShowLevelDebugKeys)
		m_keyboard.PrintMarketingKeys();

	if(sm_bCheckKeyboard)
	{
		ioVirtualKeyboard::eKBState state = UpdateVirtualKeyboard();
		if(state == ioVirtualKeyboard::kKBState_SUCCESS)
		{
			safecpy(sm_DefaultVirtualKeyboardText, sm_VirtualKeyboardResult);
		}
		sm_bCheckKeyboard = (state == ioVirtualKeyboard::kKBState_PENDING);
	}
#endif

#if DEBUG_PAD_INPUT
	if( (CPathServer::m_iVisualiseNavMeshes == CPathServer::NavMeshVis_Off) && m_keyboard.GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_CTRL, "Toggle Display Values"))
	{
		sm_bDebugPadValues = !sm_bDebugPadValues;
	}

#if DEBUG_DRAW && KEYBOARD_MOUSE_SUPPORT
	m_controls[CONTROL_MAIN_PLAYER].DebugDrawMouseInput();

	static bool showMouseDebugPos = false;
	if(GetKeyboard().GetKeyJustDown(KEY_M, KEYBOARD_MODE_DEBUG_CNTRL_ALT))
	{
		showMouseDebugPos = !showMouseDebugPos;
	}

	if(showMouseDebugPos)
	{
		const float dx = ioMouse::GetDX() * g_InvWindowWidth;
		const float dy = ioMouse::GetDY() * g_InvWindowHeight;

		static float x = 0.5f;
		static float y = 0.5f;

		x += dx;
		y += dy;

		x = Clamp(x, 0.0f, 1.0f);
		y = Clamp(y, 0.0f, 1.0f);
		grcDebugDraw::Line(Vector2(0.5f, 0.0f), Vector2(0.5f, 1.0f), Color32(255, 0, 0), Color32(255, 0, 0));
		grcDebugDraw::Line(Vector2(0.0f, 0.5f), Vector2(1.0f, 0.5f), Color32(255, 0, 0), Color32(255, 0, 0));
		grcDebugDraw::Circle(Vector2(0.5f + dx, 0.5f + dy), 0.02f, Color32(0, 255, 0));
		grcDebugDraw::Circle(Vector2(x, y), 0.02f, Color32(0, 0, 255));
	}
#endif // DEBUG_DRAW && KEYBOARD_MOUSE_SUPPORT
#endif // DEBUG_PAD_INPUT

	DEBUG_DRAW_ONLY(DisplayDisabledInputs());

#if KEYBOARD_MOUSE_SUPPORT && BACKTRACE_ENABLED
	static bool s_setInitially = false, s_previouslyKbm = false;
	bool currentlyKbm = m_controls[CONTROL_MAIN_PLAYER].WasKeyboardMouseLastKnownSource();
	if (currentlyKbm != s_previouslyKbm || !s_setInitially)
	{
		s_setInitially = true;
		s_previouslyKbm = currentlyKbm;
		sysException::SetAnnotation("control_type", currentlyKbm ? "Keyboard & Mouse" : "Pad");
	}
#endif
}

void CControlMgr::UpdateThread(void* UNUSED_PARAM(param))
{
	sm_ThreadId = sysIpcGetCurrentThreadId();

	CControl::SetControlUpdateThread(sm_ThreadId);

	while(!sm_bShutdownThreads)
	{
		sysIpcWaitSema(m_runUpdateSema);

		WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateThread()"));

#if RSG_PC
		TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateThread() - Wait for Windows Message Pump!");
		ioInput::WaitForMessagePumpUpdateSignal();
		TELEMETRY_END_ZONE(__FILE__,__LINE__);
#endif // RSG_PC


		WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateThread() - Update()"));
		DURANGO_ONLY(PF_PUSH_MARKERC(0xff000000,"CControlMgr::UpdateThread"));
		Update();
		DURANGO_ONLY(PF_POP_MARKER());
		WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));


		sysIpcSignalSema(m_updateFinishedSema);

		WIN32PC_ONLY(TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"CControlMgr::UpdateThread() - RecaptureLostDevices()"));
		WIN32PC_ONLY(ioInput::RecaptureLostDevices());
		WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));

		STEAM_CONTROLLER_ONLY(m_controls[CONTROL_MAIN_PLAYER].CheckForSteamController());

		WIN32PC_ONLY(TELEMETRY_END_ZONE(__FILE__,__LINE__));
	}

	CControl::SetControlUpdateThread(sysIpcCurrentThreadIdInvalid);
	sm_ThreadId = sysIpcCurrentThreadIdInvalid;
}

#if __BANK

void DebugInputPrint(const Vector2 &DEBUG_DRAW_ONLY(pos), 
					 Color32 DEBUG_DRAW_ONLY(disabledColor), 
					 const char *pString, 
					 bool DEBUG_DRAW_ONLY(bDrawQuad), 
					 float DEBUG_DRAW_ONLY(fScaleX), 
					 float DEBUG_DRAW_ONLY(fScaleY))
{
	if(CControlMgr::IsPrintingToGameplayLog())
	{
		atString string(pString); string += '\n';
		CAILogManager::GetLog().Log(string.c_str());
	}
	else
	{
#if DEBUG_DRAW
		grcDebugDraw::Text( pos,
			disabledColor,
			pString,
			bDrawQuad,
			fScaleX,
			fScaleY );
#endif
	}
}

void CControlMgr::PrintDisabledInputsToGameplayLog()
{
	CControlMgr::sm_bPrintToGameplayLog = true;
	CControlMgr::InputDisplayType OldDisplayType = CControlMgr::sm_eInputDisplayType;
	CControlMgr::sm_eInputDisplayType = CControlMgr::IDT_DISABLED_ONLY;
	CControlMgr::DisplayDisabledInputs();
	CControlMgr::sm_eInputDisplayType = OldDisplayType;
	CControlMgr::sm_bPrintToGameplayLog = false;

}

void CControlMgr::DisplayDisabledInputs()
{
	if(GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_CNTRL_ALT))
	{
		s32 displayType = sm_eInputDisplayType + 1;
		if(displayType >= IDT_AMOUNT)
		{
			displayType = 0;
		}

		sm_eInputDisplayType = static_cast<InputDisplayType>(displayType);
	}

	if(sm_eInputDisplayType != IDT_NONE)
	{
		// output colors.
		const static Color32 enabledColor  = Color32(255, 255, 255);
		const static Color32 disabledColor = Color32(255, 0, 0);

		// the Y starting position
		const float ystart = 0.05f;

#if DEBUG_DRAW
		// Bottom bar has two lines of text.
		const float BottomBarHeight = (3.5f * static_cast<float>(grcDebugDraw::GetScreenSpaceTextHeight()))
			/ static_cast<float>(grcDevice::GetHeight());

		// output increments.
		const float dy = (grcDebugDraw::GetScreenSpaceTextHeight() / static_cast<float>(grcDevice::GetHeight()) * sm_InputDisplayScale);
#else
		const float BottomBarHeight = 0.0f;
		const float dy = 0.0f;
#endif

		// This number was deduced by zooming in and counting the pixels.
		const s32 TEXT_WIDTH = 9;

		// we use 8 as the width. This was deduced from as we cannot get the width so we are assuming the width and hight are the same.
		const float dx = (TEXT_WIDTH / static_cast<float>(grcDevice::GetWidth())) * sm_InputDislayMaxLength * sm_InputDisplayScale;

		float ypos = ystart;
		float xpos = sm_InputDisplayX;

		// print out the radio/weapon wheels
#if __DEV
		if( const char* pszWeaponWheel = CNewHud::GetWeaponWheelDisabledReason() )
		{
			DebugInputPrint( Vector2(xpos, ypos),
				disabledColor,
				pszWeaponWheel,
				true,
				sm_InputDisplayScale,
				sm_InputDisplayScale );
			ypos += dy;
		}

		if( const char* pszRadioWheel = CNewHud::GetRadioWheelDisabledReason() )
		{
			DebugInputPrint( Vector2(xpos, ypos),
				disabledColor,
				pszRadioWheel,
				true,
				sm_InputDisplayScale,
				sm_InputDisplayScale );
			ypos += dy;
		}

#endif

		const CPlayerInfo* pPlayerInfo = GetLocalPlayerInfo();

		bool anyControlDisabled = false;

		// Show information about player control enable state. NOTE: We do not need to check what GetMainPlayerControl() returns as we are going to
		// check all the checks currently in GetMainPlayerControl(). However doing it this way, if someone edits GetMainPlayerControl() and adds a new
		// disable condition, we will detect it and inform the reason is unknown.
		if(&m_controls[CONTROL_EMPTY] == &GetMainPlayerControl())
		{
			bool hasReason = false;
			anyControlDisabled = true;

			if(CWarningScreen::IsActive())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Player Control (Warning Screen Visible)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(CLiveManager::IsSystemUiShowing())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Player Control (System UI Showing)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(pPlayerInfo == NULL)
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Player Control (No Local Player Info)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			else
			{
				if(pPlayerInfo->AreControlsDisabled())
				{
					DebugInputPrint( Vector2(xpos, ypos),
						disabledColor,
						"Player Control (Manually Disabled)",
						true,
						sm_InputDisplayScale,
						sm_InputDisplayScale );
					hasReason = true;
					ypos += dy;
				}
				if(pPlayerInfo->GetPlayerPed() == NULL)
				{
					DebugInputPrint( Vector2(xpos, ypos),
						disabledColor,
						"Player Control (No Player Ped)",
						true,
						sm_InputDisplayScale,
						sm_InputDisplayScale );
					hasReason = true;
					ypos += dy;
				}
				else if(pPlayerInfo->GetPlayerPed()->GetIsArrested())
				{
					DebugInputPrint( Vector2(xpos, ypos),
						disabledColor,
						"Player Control (Player Arrested)",
						true,
						sm_InputDisplayScale,
						sm_InputDisplayScale );
					hasReason = true;
					ypos += dy;
				}
			}

			if(!hasReason)
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Player Control (Unknown)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				ypos += dy;
			}
		}

		// Show information about camera control enable state. NOTE: We do not need to check what GetMainCameraControl() returns as we are going to
		// check all the checks currently in GetMainCameraControl(). However doing it this way, if someone edits GetMainCameraControl() and adds a new
		// disable condition, we will detect it and inform the reason is unknown.
		if(&m_controls[CONTROL_EMPTY] == &GetMainCameraControl())
		{
			bool hasReason = false;
			anyControlDisabled = true;

			if(CWarningScreen::IsActive())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (Warning Screen Visible)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(CLiveManager::IsSystemUiShowing())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (System UI Showing)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(pPlayerInfo == NULL)
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (No Local Player Info)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			else
			{
				if(pPlayerInfo->IsControlsCameraDisabled())
				{
					DebugInputPrint( Vector2(xpos, ypos),
						disabledColor,
						"Camera Control (Manually Disabled)",
						true,
						sm_InputDisplayScale,
						sm_InputDisplayScale );
					hasReason = true;
					ypos += dy;
				}
				if(NetworkInterface::IsGameInProgress() && pPlayerInfo->IsControlsFrontendDisabled())
				{
					DebugInputPrint( Vector2(xpos, ypos),
						disabledColor,
						"Camera Control (Controls Frontend Disabled)",
						true,
						sm_InputDisplayScale,
						sm_InputDisplayScale );
					hasReason = true;
					ypos += dy;
				}
			}

			if(CNewHud::ShouldWheelBlockCamera())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (Weapon Wheel)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}

			if(!hasReason)
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (Unknown)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				ypos += dy;
			}
		}

		// Show information about frontend control enable state. NOTE: We do not need to check what GetMainFrontendControl() returns as we are going to
		// check all the checks currently in GetMainFrontendControl(). However doing it this way, if someone edits GetMainFrontendControl() and adds a new
		// disable condition, we will detect it and inform the reason is unknown.
		if(&m_controls[CONTROL_EMPTY] == &GetMainFrontendControl())
		{
			bool hasReason = false;
			anyControlDisabled = true;

			if(!CLiveManager::IsInitialized())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Frontend Control (Live Manager Not Init)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(CLiveManager::IsSystemUiShowing())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Frontend Control (System UI Showing)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}
			if(pPlayerInfo != NULL && !CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Frontend Control (No Local Player Info)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				hasReason = true;
				ypos += dy;
			}

			if(!hasReason)
			{
				DebugInputPrint( Vector2(xpos, ypos),
					disabledColor,
					"Camera Control (Unknown)",
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );
				ypos += dy;
			}
		}

		// Leave a gap between disabled controllers and inputs.
		if(anyControlDisabled)
		{
			ypos += (2 * dy);
		}

		const char* pszType = NULL;
		switch(sm_eInputDisplayType)
		{
			case IDT_ALL:			pszType = "All Inputs";				break;
			case IDT_NONZERO:		pszType = "Non-Zero Inputs";		break;
			case IDT_DISABLED_ONLY: pszType = "Disabled Inputs Only";	break;
			default: 
				controlAssertf(false, "Unknown input display type!");
				break;
		}

		if( pszType )
		{
			DebugInputPrint( Vector2(xpos, ypos),
				Color32(0, 255, 0),
				pszType,
				true,
				sm_InputDisplayScale,
				sm_InputDisplayScale );

			ypos += dy;
		}


		char buffer[100];

		ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
		options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);

		// Show inputs enable state and value.
		for(s32 i = 0; i < MAX_INPUTS; ++i)
		{
			if(ypos > (1.0f - dy - BottomBarHeight))
			{
				ypos = ystart;
				xpos += dx;
			}

			buffer[0] = '\0';

			const ioValue& value = m_controls[CONTROL_MAIN_PLAYER].GetValue(i);

			if(sm_eInputDisplayType == IDT_ALL 
				|| (sm_eInputDisplayType == IDT_DISABLED_ONLY && !value.IsEnabled()) 
				|| (sm_eInputDisplayType == IDT_NONZERO && value.GetUnboundNorm(options) != 0.0f ))
			{
				const char* inputName = m_controls[CONTROL_MAIN_PLAYER].GetInputName(static_cast<InputType>(i));

				if(strncmp(inputName, "INPUT_", 6) == 0)
				{
					inputName += 6;
				}

				formatf(buffer, 100, "%s (%.2f)",
					inputName,
					value.GetUnboundNorm(options) );

				DebugInputPrint( Vector2(xpos, ypos),
					(value.IsEnabled() ? enabledColor : disabledColor),
					buffer,
					true,
					sm_InputDisplayScale,
					sm_InputDisplayScale );

				ypos += dy;
			}
		}
	}
	else
	{
		controlAssertf(sm_eInputDisplayType == IDT_NONE, "Unknown display type for outputting input status!");
	}
}
#endif // __BANK

#if __BANK

static int gFakeKeypress;
void FakeKeypress()
{
	CControlMgr::GetKeyboard().FakeKeyPress(gFakeKeypress);
}

void CControlMgr::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Input");
	bank.AddSlider("Overlay Disable Duration:", &sm_InputDisableDuration, 0U, 5000U, 1U);

#if DEBUG_DRAW
	bank.PushGroup("On Screen Inputs");
	bank.AddCombo("Inputs:", reinterpret_cast<int*>(&sm_eInputDisplayType), IDT_AMOUNT, sm_InputDislayTypeNames);
	bank.AddSlider("Text Scale", &sm_InputDisplayScale, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Text X Position", &sm_InputDisplayX, -10.0f, 1.0f, 0.01f);
	bank.PopGroup();
#endif // DEBUG_DRAW

	// Load a custom meta data file
	bank.PushGroup("Custom Meta Input File");
	bank.AddText("Custom Meta File Path:",sm_CustomInputMetaFilePath, RAGE_MAX_PATH);
	bank.AddButton("Load Custom Controls", LoadCustomControls);
	bank.AddButton("Load Default Controls", ReInitControls);
	bank.PopGroup();

	bank.PushGroup("Pad");
	bank.AddToggle("Debug pad", &sm_bDebugPad);
	bank.AddToggle("Debug Pad Values", &sm_bDebugPadValues);
	bank.AddSlider("Debug Pad Number", &gPadNumberToDebug, 0, NUM_PADS-1, 1);
	bank.AddSlider("Defaults Setting Num", &sm_nSaveAsDefaultNum, 0, 9, 1);
	bank.AddButton("Save Defaults", &SaveAsDefaultsButton);
	bank.AddButton("Load Defaults", &LoadDefaultsButton);

	const s32 iMaxHardcodedMappings = 3;
	const char* mappingNames[iMaxHardcodedMappings] =
	{
		"Default",
		"Test1",
		"Test2",
	};

	bank.AddCombo("HardcodedMapping", &sm_iHardcodedMappingIndex, iMaxHardcodedMappings, mappingNames, HardcodedMappingCallback);

	bank.AddToggle("Apply frequency for Motor 0", &sm_bOverrideShakeFreq0);
	bank.AddSlider("Shake Frequency - Motor 0",   &sm_iShakeFrequency0, 0, 256, 1);
	bank.AddToggle("Apply duration for Motor 0",  &sm_bOverrideShakeDur0);
	bank.AddSlider("Duration", &sm_iShakeDuration0, 0, 255, 1);

	bank.AddToggle("Apply frequency for Motor 1", &sm_bOverrideShakeFreq1);
	bank.AddSlider("Shake Frequency - Motor 1", &sm_iShakeFrequency1, 0, 256, 1);
	bank.AddToggle("Apply duration for Motor 1",  &sm_bOverrideShakeDur1);
	bank.AddSlider("Shake Duration", &sm_iShakeDuration1, 0, 255, 1);

	bank.AddButton("Shake Pad", DebugShakeUsingOverridesCallback);

	bank.AddToggle("Apply Delay after shake.",    &sm_bOverrideShakeDelay);
	bank.AddSlider("Shake delay after this one", &sm_iShakeDelayAfterThisOne, 0, 2555555, 1);

	bank.AddToggle("Use widget multiplier", &sm_bUseThisMultiplier);
	bank.AddSlider("Multiplier", &sm_fMultiplier, 0.0f, 10000.0f, 1.0f);

	bank.PushGroup("Debug Shake Intensity");
	bank.AddSlider("Intensity",&sm_fDebugShakeIntensity,0.0f,1.0f,0.01f);
	bank.AddSlider("Duration",&sm_iDebugShakeDuration,0,5000,1);
	bank.AddButton("Do shake",DebugShakeCallback);
	bank.AddSlider("Motor to shake", &sm_iDebugMotorToShake, 0, ioPad::MAX_ACTUATORS -1, 1);
	bank.AddToggle("Shake Motor", &sm_bDebugShakeMotor);
	bank.PopGroup();
	bank.PopGroup();
	bank.PushGroup("Keyboard");
	bank.AddToggle("Show Debug Keys", &gbShowDebugKeys);
	bank.AddToggle("Show Marketing Debug Keys", &gbShowLevelDebugKeys);
	bank.AddSlider("Key value", &gFakeKeypress, 0, 255, 1);
	bank.AddButton("Fake keypress", datCallback(FakeKeypress));

	static const char* virtualKeyboardModes [ioVirtualKeyboard::kTextType_COUNT] = { "Default", "Email", "Password", "Numeric", "Alphabetic", "Gamertag" };
	bank.AddCombo("Virtual Keyboard Mode", &sm_KeyboardMode, ioVirtualKeyboard::kTextType_COUNT, virtualKeyboardModes);
	bank.AddSlider("Virtual Keyboard Text Limit", &sm_VirtualKeyboardTextLimit, 0, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD, 1);
	bank.AddText("Virtual Keyboard Text", sm_DefaultVirtualKeyboardText, MAX_KEYBOARD_TEXT);
	bank.AddButton("Show Virtual Keyboard", DebugShowVirtualKeyboard);
	bank.AddButton("Hide Virtual Keyboard", CancelVirtualKeyboard);
	bank.PopGroup();

	// now show virtual inputs.
	bank.PushGroup("Channel/Virtual Inputs");
	m_controls[CONTROL_MAIN_PLAYER].InitWidgets(bank, "Main Player");
	m_controls[CONTROL_EMPTY].InitWidgets(bank, "Empty");
#if ENABLE_DEBUG_CONTROLS
	m_controls[CONTROL_DEBUG].InitWidgets(bank, "Debug");
#endif // ENABLE_DEBUG_CONTROLS
	bank.PopGroup();

// for now we are only adding mapping widgets to pc version.
#if __WIN32PC
	// now add the mappings controls.
	bank.PushGroup("Input Mapping");
	m_controls[CONTROL_MAIN_PLAYER].InitMappingWidgets(bank);
	m_controls[CONTROL_MAIN_PLAYER].InitJoystickCalibrationWidgets(bank);
	bank.AddButton("Reload Controls", datCallback(CFA(CControlMgr::ReInitControls)));
	bank.PopGroup();

	bank.AddSlider("Mouse Smoothing Frames", &rage::ioMouse::sm_MouseSmoothingFrames, 1, 4, 1);
	bank.AddSlider("Mouse Smoothing Threshold", &rage::ioMouse::sm_MouseSmoothingThreshold, -1.0f, 255.0f, 1.0f);
	bank.AddSlider("Mouse Blend out Threshold", &rage::ioMouse::sm_MouseSmoothingBlendLimit, 0.0f, 255.0f, 1.0f);
	bank.AddSlider("Mouse Smoothing Weight", &rage::ioMouse::sm_MouseWeightScale, 0.01f, 1.0f, 0.005f);
#endif // __WIN32PC

	BANK_ONLY(ioHeadTracking::InitWidgets();)

#if __PPU
	CPad* pad = GetPlayerPad();
	if(pad)
	{
		CPadGesture* gesture = pad->GetPadGesture();
		if(gesture)
		{
			gesture->InitWidgets(bank);
		}
	}
#endif // __PPU

#if RSG_ORBIS
	bank.PushGroup("Orbis Touch Pad");
	CTouchPadGesture::InitWidgets(bank);
	bank.PopGroup();
#endif // RSG_ORBIS


}
#endif // __BANK

// returns local main player's control
CControl& CControlMgr::GetMainPlayerControl(bool bAllowDisabling)
{
	CPlayerInfo* pPlayerInfo = GetLocalPlayerInfo();
	// If the controls for this player are disabled we return the 'empty' controls.
	if (CWarningScreen::IsActive() || CLiveManager::IsSystemUiShowing() || !pPlayerInfo || (bAllowDisabling && pPlayerInfo->AreControlsDisabled()) || !pPlayerInfo->GetPlayerPed() || pPlayerInfo->GetPlayerPed()->GetIsArrested() )
	{
		return (m_controls[CONTROL_EMPTY]);
	}
	return (m_controls[CONTROL_MAIN_PLAYER]);
}

// returns local main player's camera control
// it is optional whether the controls can be disabled or not...
CControl& CControlMgr::GetMainCameraControl(bool bAllowDisabling)
{
	CPlayerInfo* pPlayerInfo = GetLocalPlayerInfo();
	// If the camera controls for this player are disabled we return the 'empty' controls.
	// NOTE: We effectively disable the camera controls when the frontend menu has disabled controls in a network game, as this prevents the
	// last valid control input from being locked-in while the menu is rendering and the game remains active in the background (not paused.)
	if(CWarningScreen::IsActive() || CLiveManager::IsSystemUiShowing() || !pPlayerInfo 
		|| ( bAllowDisabling &&
			( pPlayerInfo->IsControlsCameraDisabled() ||
			  CNewHud::ShouldWheelBlockCamera() ||
			  ( NetworkInterface::IsGameInProgress() && pPlayerInfo->IsControlsFrontendDisabled() ) 
			)))
	{
		return (m_controls[CONTROL_EMPTY]);
	}
	return (m_controls[CONTROL_MAIN_PLAYER]);
}

// returns local main player's control for use with frontend features that don't care whether game control is disabled
CControl& CControlMgr::GetMainFrontendControl(bool bAllowDisabling)
{
	if ((!CLiveManager::IsInitialized() || CLiveManager::IsSystemUiShowing()) || 
		(bAllowDisabling && (!GetLocalPlayerInfo()) && (CGame::IsSessionInitialized()) && (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame()) ) )
	{
		return (m_controls[CONTROL_EMPTY]);
	}
	return (m_controls[CONTROL_MAIN_PLAYER]);
}


#if ENABLE_DEBUG_CONTROLS
// returns local main player's control
CControl& CControlMgr::GetDebugControl()
{
	return (m_controls[CONTROL_DEBUG]);
}
#endif

//
// name:		StopPadsShaking
// description:	Stop all the pads shaking
void CControlMgr::StopPadsShaking()
{
	for(s32 i=0; i<MAX_CONTROL; i++)
	{
		StopPlayerPadShaking(true, i);
	}
}

void CControlMgr::AllowPadsShaking()
{
	for(s32 i=0; i<MAX_CONTROL; i++)
	{
		m_controls[i].AllowPlayerPadShaking();
	}
}

//
// name:		ShouldStopPadsShaking
// description:	Returns true if the manager has to stop all pads shaking.
bool CControlMgr::ShouldStopPadsShaking()
{
	CPlayerInfo* pPlayerInfo = GetLocalPlayerInfo();
	if (!pPlayerInfo)
		return true;

	CViewport* pViewport = NULL;
	if (gVpMan.PrimaryOrthoViewportExists())
		pViewport = gVpMan.GetPrimaryOrthoViewport();
	else
		return true;

	bool bFaderStopsVibration = false;
	if (pViewport)
	{
		// Don't vibrate if we're fading in/out, or if we're faded out.
		bFaderStopsVibration = !camInterface::IsFadedIn();
	}
	// Wasted Cam let vibration continue
	if (pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED && !bFaderStopsVibration)
	{
		for(s32 i=0; i<NUM_PADS; i++)
		{
			if(!m_pads[i].IsPreventedFromShaking())
				m_pads[i].NoMoreShake();
		}

		return false;
	}
	// Player is not playing
	else if (pPlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		return true;
	}
	// Camera is in slow motion
	else if (!sm_bShakeWhenControllerDisabled && pPlayerInfo && pPlayerInfo->AreControlsDisabled() && 
			 !pPlayerInfo->IsControlsFrontendDisabled() && 
			 !pPlayerInfo->IsControlsScriptDisabled() && 
			 !pPlayerInfo->IsControlsScriptAmbientDisabled() && 
			 !pPlayerInfo->IsControlsCutscenesDisabled())
	{
		return true;
	}
	//else if (!sm_bShakeWhenControllerDisabled && pPlayerInfo && 
	//	fwTimer::GetTimeWarp() != 1.0f && 
	//	!pPlayerInfo->IsControlsScriptDisabled() && 
	//	!pPlayerInfo->IsControlsScriptAmbientDisabled() && 
	//	!pPlayerInfo->IsControlsCutscenesDisabled())
	//{
	//	return true;
	//}
	// Screen is Fading or faded out
	if (pViewport && bFaderStopsVibration)
	{
		return true;
	}
	// System UI active
	else if(CLiveManager::IsSystemUiShowing() && !NetworkInterface::IsGameInProgress())
	{
		return true;
	}
	// Game is Paused
	else if (fwTimer::IsGamePaused() && !CPauseMenu::IsActive())
	{
		return true;
	}
#if GTA_REPLAY
	// We are in the replay editor or replay-preview
	else if ( CReplayCoordinator::IsActive() )
	{
		return true;
	}
#endif

	return false;
}

void CControlMgr::StartPlayerPadShakeByIntensity( u32 iDuration, float fIntensity, s32 iDelayAfterThisOne, s32 iController )
{
	fIntensity = rage::Clamp(fIntensity,0.0f,1.0f);

	// At low intensity only motor 1 plays (it is weaker)
	// then motor 0 replaces it
	// ..finally both come in

	// Tuned for each platform, due to slight hardware differences

#if USE_ACTUATOR_EFFECTS

#if RSG_ORBIS
	static dev_float fMinPadShakeFreq0 = 100.0f;
	static dev_float fMaxPadShakeFreq0 = 255.0f;
	static dev_float fMinPadShakeFreq1 = 1.0f;
	static dev_float fMaxPadShakeFreq1 = 255.0f;
	static dev_float fMotor1Stop = -1.0f;
	static dev_float fBothMotorStart = 0.9f;

#elif RSG_DURANGO
	static dev_float fMinPadShakeFreq0 = 150.0f;
	static dev_float fMaxPadShakeFreq0 = 255.0f;
	static dev_float fMinPadShakeFreq1 = 50.0f;
	static dev_float fMaxPadShakeFreq1 = 120.0f;
	static dev_float fMotor1Stop = 0.1f;
	static dev_float fBothMotorStart = 0.9f;

#else // PC / XINPUT
	static dev_float fMinPadShakeFreq0 = 75.0f;
	static dev_float fMaxPadShakeFreq0 = 255.0f;
	static dev_float fMinPadShakeFreq1 = 50.0f;
	static dev_float fMaxPadShakeFreq1 = 120.0f;
	static dev_float fMotor1Stop = 0.1f;
	static dev_float fBothMotorStart = 0.9f;
#endif

#else

#if __PS3
	static dev_float fMinPadShakeFreq0 = 100.0f;
	static dev_float fMaxPadShakeFreq0 = 255.0f;
	static dev_float fMinPadShakeFreq1 = 1.0f;
	static dev_float fMaxPadShakeFreq1 = 255.0f;
	static dev_float fMotor1Stop = -1.0f;			// Don't use motor 1 at low end for ps3
	static dev_float fBothMotorStart = 0.9f;

#else // XENON
	static dev_float fMinPadShakeFreq0 = 15.0f;
	static dev_float fMaxPadShakeFreq0 = 200.0f;
	static dev_float fMinPadShakeFreq1 = 50.0f;
	static dev_float fMaxPadShakeFreq1 = 120.0f;
	static dev_float fMotor1Stop = 0.1f;
	static dev_float fBothMotorStart = 0.9f;
#endif

#endif // USE_ACTUATOR_EFFECTS

	u32 iFreq0, iFreq1;

	if(fIntensity == 0.0f)
	{
		iFreq0 = iFreq1 = 0;
	}
	else if (fIntensity < fMotor1Stop)
	{
		iFreq1 = (u32)(fMinPadShakeFreq1 + ((fMaxPadShakeFreq1 - fMinPadShakeFreq1)*(fIntensity/fMotor1Stop)));
		iFreq0 = 0;
	}
	else if (fIntensity < fBothMotorStart)
	{
		iFreq0 = (u32)(fMinPadShakeFreq0 + ((fMaxPadShakeFreq0 - fMinPadShakeFreq0)*fIntensity));
		iFreq1 = 0;
	}
	else
	{
		iFreq0 = (u32)(fMinPadShakeFreq0 + ((fMaxPadShakeFreq0 - fMinPadShakeFreq0)*fIntensity));
		iFreq1 = (u32)fMaxPadShakeFreq1;
	}

	StartPlayerPadShake(iDuration,iFreq0,iDuration,iFreq1,iDelayAfterThisOne, iController);

}

#if __BANK
void CControlMgr::DebugShakeCallback()
{
	StartPlayerPadShakeByIntensity(sm_iDebugShakeDuration,sm_fDebugShakeIntensity,0);
}

void CControlMgr::DebugShakeUsingOverridesCallback()
{
	StartPlayerPadShake(sm_iShakeDuration0 * 10, sm_iShakeFrequency0, sm_iShakeDuration1 * 10, sm_iShakeFrequency1);
}

void CControlMgr::HardcodedMappingCallback()
{
	m_controls[CONTROL_MAIN_PLAYER].SetMappingsTest(sm_iHardcodedMappingIndex);
	m_controls[CONTROL_EMPTY].SetMappingsTest(sm_iHardcodedMappingIndex);
#if ENABLE_DEBUG_CONTROLS
	m_controls[CONTROL_DEBUG].SetMappingsTest(sm_iHardcodedMappingIndex);
#endif
}
void CControlMgr::DebugShowVirtualKeyboard()
{
	ioVirtualKeyboard::Params params;
	char16 defaultText[MAX_KEYBOARD_TEXT];
	Utf8ToWide(defaultText, sm_DefaultVirtualKeyboardText, MAX_KEYBOARD_TEXT);

	params.m_InitialValue = defaultText;
	params.m_KeyboardType = static_cast<ioVirtualKeyboard::eTextType>(sm_KeyboardMode);
	params.m_Description = _C16("Rag Debug Test");
	params.m_Title = _C16("Debug Title");
	params.m_MaxLength = MAX_KEYBOARD_TEXT;
	params.m_PlayerIndex = 0;
	params.m_MaxLength = sm_VirtualKeyboardTextLimit;

	ShowVirtualKeyboard(params);
	sm_bCheckKeyboard = true;
}

#endif

#if DEBUG_PAD_INPUT
bool CControlMgr::IsDebugPadValuesOn() 
{ 
	bool bPlayingCutscene = CutSceneManager::GetInstancePtr() && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning();

	return sm_bDebugPadValues && !bPlayingCutscene; 
}
#endif

void CControlMgr::ShowVirtualKeyboard(const ioVirtualKeyboard::Params& params)
{
	sm_VirtualKeyboardResult[0] = '\0';

	// The title in the text box has been removed for the time being.
	// The title passed into this function will be passed to the input box as its description.
	// Future Todo: support max lenth and input type masking
	Displayf("Showing ioVirtualKeyboard!");
	ioVirtualKeyboard::Params params2 = params;
	params2.m_PlayerIndex = sm_PlayerPadIndex != -1 ? sm_PlayerPadIndex : 0;
	sm_bVirtualKeyboardIsEnteringAPassword = (params.m_KeyboardType == ioVirtualKeyboard::kTextType_PASSWORD);

#if RSG_ORBIS
	params2.m_UserId = g_rlNp.GetUserServiceId(params2.m_PlayerIndex);
#endif

	// work out language for virtual keyboard
	if(params2.m_AlphabetType == ioVirtualKeyboard::kAlphabetType_INVALID)
	{
		switch(CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
		case LANGUAGE_RUSSIAN:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CYRILLIC;
			break;
		case LANGUAGE_CHINESE_TRADITIONAL:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CHINESE;
			break;
		case LANGUAGE_CHINESE_SIMPLIFIED:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CHINESE_SIMPLIFIED;
			break;
		case LANGUAGE_KOREAN:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_KOREAN;
			break;
/*	Need 2mb for the memory container on PS3*/
#if __XENON
		case LANGUAGE_JAPANESE:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_JAPANESE;
			break;
#endif
		case LANGUAGE_POLISH :
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_POLISH;
			break;
		default:
			params2.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_ENGLISH;
			break;
		}
	}

#if RSG_PC
#if OVERLAY_KEYBOARD
	// Try and show the big picture keyboard overlay. This can fail when the user is not in big picture mode. In this case we fallback to the
	// PC text input box.
	sm_bUsingOverlayKeyboard = sm_VirtualKeyboard.Show(params2);
	if(sm_bUsingOverlayKeyboard)
	{
		return;
	}
#endif // OVERLAY_KEYBOARD

	STextInputBox::GetInstance().Open(params2);
#else
	Verifyf(sm_VirtualKeyboard.Show(params2), "Failed to show ioVirtualKeyboard!");
#endif // RSG_PC
}


void CControlMgr::ReplaceCharacterIfUnsupportedInFont(char16 &input, s32 fontStyle, eFontBitField fontBit, const char* OUTPUT_ONLY(pFontName), int OUTPUT_ONLY(characterIndex))
{
	if ((sm_fontBitField & fontBit) != 0)
	{
		if (!CTextFormat::DoesCharacterExistInFont(fontStyle, input))
		{
			Displayf("***GRAEME*** CControlMgr::ReplaceCharacterIfUnsupportedInFont - character %d (%c) doesn't exist in the %s font so I'll replace it with a space", characterIndex, input, pFontName);
			input = ' ';
		}
	}
}

//	Copied from rage::WideToUtf8
//	Replace a few characters that would cause us problems such as ~ and characters that could be used for HTML
//	Should we avoid any foreign language characters?
char *CControlMgr::WideToSafeUtf8(char *out,const char16 *in,int length)
{
	u8 * utf8out = (u8*)out;
	for (int i=0; in[i] && i<length;i++)
	{
		char16 input = in[i];

//		Displayf("GraemeW - %c has ASCII code %d", input, input);

		if (!sm_bVirtualKeyboardIsEnteringAPassword)
		{	//	B*1718510 - We don't want to replace any characters that the player types as part of a password
			if (!CTextFormat::DoesCharacterExistInFont(FONT_STYLE_STANDARD, input))
			{
				Displayf("***GRAEME*** CControlMgr::WideToSafeUtf8 - character %d (%c) doesn't exist in the Standard font so I'll replace it with a space", i, input);
				input = ' ';
			}

			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_CURSIVE, FONT_BIT_CURSIVE, "Cursive", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_ROCKSTAR_TAG, FONT_BIT_ROCKSTAR_TAG, "Rockstar Tag", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_LEADERBOARD, FONT_BIT_LEADERBOARD, "Leaderboard", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_CONDENSED, FONT_BIT_CONDENSED, "Condensed", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_FIXED_WIDTH_NUMBERS, FONT_BIT_FIXED_WIDTH_NUMBERS, "Fixed Width Numbers", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_CONDENSED_NOT_GAMERNAME, FONT_BIT_CONDENSED_NOT_GAMERNAME, "Condensed Not Gamername", i);
			ReplaceCharacterIfUnsupportedInFont(input, FONT_STYLE_PRICEDOWN, FONT_BIT_PRICEDOWN, "Pricedown", i);

			switch (input)
			{
			case '~' :
			case '&' :
				{
#if  !__FINAL
					if (sm_bAllowTildeCharacterFromVirtualKeyboard)
					{
						//	Keep the tilde character as it is
						break;
					}
#endif	//	!__FINAL

					//	Otherwise fall through and replace the tilde with a hyphen
				}
			case '<' :
			case '>' :
			case '\\' :
			case '\"' :
			case 0xa4 : // microsoft MS points symbol
				input = '-';
				break;

			case 0xA6 : // '¦'  // swap this out for '|' which is supported
				input = '|';
			}
		}	//	if (!sm_bVirtualKeyboardIsEnteringAPassword)

		if (input<=0x7F)
			*utf8out++ = (u8)input;
		else if (input <= 0x7FF)
		{
			*utf8out++ = u8(0xC0 + (input >> 6));
			*utf8out++ = u8(0x80 + (input & 0x3f));
		}
		else
		{
			*utf8out++ = u8(0xE0 +  (input >> 12));
			*utf8out++ = u8(0x80 + ((input >> 6)&0x3f));
			*utf8out++ = u8(0x80 +  (input & 0x3f));
		}
	}
	*utf8out = 0;
	return out;
}

ioVirtualKeyboard::eKBState CControlMgr::UpdateVirtualKeyboard()
{
#if !RSG_PC || OVERLAY_KEYBOARD
#if OVERLAY_KEYBOARD
	if(sm_bUsingOverlayKeyboard)
#endif // OVERLAY_KEYBOARD
	{
		if (sm_VirtualKeyboard.IsPending())
		{
			sm_VirtualKeyboard.Update();

			if (sm_VirtualKeyboard.Succeeded())
			{
				Displayf("ioVirtualKeyboard succeeded, getting result.");

				char16* kbdData = sm_VirtualKeyboard.GetResult();
				WideToSafeUtf8(sm_VirtualKeyboardResult, kbdData, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);

				Displayf("ioVirtualKeyboard result was, %s.", sm_VirtualKeyboardResult);
			}

			if (!sm_VirtualKeyboard.IsPending())
			{
				sm_fontBitField = 0;	//	Reset this for the next time the onscreen keyboard is displayed
			}
		}


		Assertf( sm_VirtualKeyboard.GetState() != ioVirtualKeyboard::kKBState_INVALID || sm_VirtualKeyboard.GetState() != ioVirtualKeyboard::kKBState_FAILED,
				 "Failed in updating ioVirtualKeyboard, state is %d!",
				 sm_VirtualKeyboard.GetState() );

		return sm_VirtualKeyboard.GetState();
	}
#endif // !RSG_PC || OVERLAY_KEYBOARD

#if RSG_PC
	// PC Input box has a registered update call

	ioVirtualKeyboard::eKBState eState = STextInputBox::GetInstance().GetState();
	if(eState == ioVirtualKeyboard::kKBState_SUCCESS)
    {
        CTextInputBox& textBoxInstance = STextInputBox::GetInstance();
        char16 const * const c_textResult = textBoxInstance.GetTextChar16();

        int const c_wideLength = c_textResult ? (int)wcslen( c_textResult ) : 0;
        int const c_maxLength = Min( c_wideLength, (int)textBoxInstance.GetMaxLength() );

        int convertedCount(0);
        WideToUtf8( sm_VirtualKeyboardResult, c_textResult, c_maxLength, ioVirtualKeyboard::VIRTUAL_KEYBOARD_RESULT_BUFFER_SIZE, &convertedCount );

		Utf8RemoveInvalidSurrogates( sm_VirtualKeyboardResult );
	}

	return eState;
#endif // RSG_PC
}


void CControlMgr::CancelVirtualKeyboard()
{
#if !RSG_PC
	sm_VirtualKeyboard.Cancel();
#else
#if OVERLAY_KEYBOARD
	if(sm_bUsingOverlayKeyboard)
	{
		sm_VirtualKeyboard.Cancel();
	}
	else
#endif // OVERLAY_KEYBOARD
	{
		STextInputBox::GetInstance().Close();
	}
#endif // !RSG_PC
}

char* CControlMgr::GetVirtualKeyboardResult()
{
	return sm_VirtualKeyboardResult;
}

ioVirtualKeyboard::eKBState CControlMgr::GetVirtualKeyboardState()
{
#if RSG_PC
#if OVERLAY_KEYBOARD
	if(sm_bUsingOverlayKeyboard)
	{
		return sm_VirtualKeyboard.GetState();
	}
#endif // OVERLAY_KEYBOARD

	return STextInputBox::GetInstance().GetState();
#else
	return sm_VirtualKeyboard.GetState();
#endif
}

#if RSG_DURANGO
void CControlMgr::HandleConstrainedChanged( sysServiceEvent* evt )
{
	if(evt != NULL)
	{
		if(evt->GetType() == sysServiceEvent::CONSTRAINED)
		{
			sysCriticalSection lock(sm_ConstrainedCs);
			sm_IsGameConstrained = true;
		}
		else if(evt->GetType() == sysServiceEvent::UNCONSTRAINED)
		{
			sysCriticalSection lock(sm_ConstrainedCs);
			sm_IsGameConstrained = false;
		}
	}
}
#endif // RSG_DURANGO

bool CControlMgr::IsShowingKeyboardTextbox()
{
#if RSG_PC
	return STextInputBox::GetInstance().IsActive();
#else
	return false;
#endif // RSG_PC
}

#if RAGE_SEC_ENABLE_YUBIKEY
bool CControlMgr::UpdateYubikey()
{
	// Wait for the legal screen to finish displaying.
	if (!CLoadingScreens::IsDisplayingLegalScreen())
	{
		return CYubiKeyManager::GetInstance()->Update();
	}
	else
	{
		return true;
	}
}
#endif //RAGE_SEC_ENABLE_YUBIKEY

bool CControlMgr::CheckForControllerDisconnect()
{
#if !RSG_PC
	// Pause on controller disconnect unless we are recording a video otherwise we could corrupt it (url:bugstar:2140784).
	if (CControlMgr::GetPlayerPad() && !CControlMgr::GetPlayerPad()->IsConnected(PPU_ONLY(true)) VIDEO_RECORDING_ENABLED_ONLY(&& !VideoRecording::IsRecording()))
	{
#if RSG_PS3
		++iNumFramesControllerDisconnected;
		iNumFramesControllerDisconnected = rage::Min(iNumFramesControllerDisconnected, CONTROLLER_RECONNECT_WARNING_THRESHOLD);

		if (iNumFramesControllerDisconnected >= CONTROLLER_RECONNECT_WARNING_THRESHOLD)
#endif //RSG_PS3
		{
#if RSG_ORBIS
			if (CControlMgr::GetPlayerPad()->HasBeenConnected())
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "PAD_REASSIGNED", FE_WARNING_NONE);
			}
			else
#elif RSG_DURANGO
			// NOTE: We do not show this message when we are constrained. This is to stop the controller disconnected message showing when instant on
			// is enabled. This for url:bugstar:2024506 but instead of clearing the message, we just stop the problematic message.
			if(g_SysService.IsConstrained() == false)
#endif	//	RSG_ORBIS
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "NO_PAD", FE_WARNING_NONE);
			}
			sm_bNoPadMessageActive = true;
		}
	}
	else
	{
#if RSG_PS3
		iNumFramesControllerDisconnected = 0;
#endif //RSG_PS3
		sm_bNoPadMessageActive = false;
	}
#endif // !RSG_PC

	return sm_bNoPadMessageActive;
}
