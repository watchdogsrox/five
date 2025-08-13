//
// name:		game.cpp
// description:	Class controlling all the game code
//

#include "core/gamesessionstatemachine.h"

// rage headers
#include "bank/msgbox.h"
#include "grprofile/timebars.h"
#include "system/param.h"

// framework headers
#include "fwutil/Gen9Settings.h"

// game headers
#include "audio/radioaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "control/ScriptRouterLink.h"
#include "core/game.h"
#include "core/core_channel.h"
#include "frontend/CSettingsMenu.h"
#include "frontend/DisplayCalibration.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/languageselect.h"
#include "frontend/loadingscreens.h"
#include "frontend/WarningScreen.h"
#include "frontend/ProfileSettings.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "game/modelindices.h"
#include "network/Live/livemanager.h"
#include "network/network.h"
#include "network/NetworkInterface.h"
#include "rline/rlsystemui.h"
#include "saveload/genericgamestorage.h"
#include "saveload/savegame_autoload.h"
#include "SaveLoad/savegame_frontend.h"
#include "saveload/savegame_list.h"
#include "saveload/savegame_load.h"
#include "security/yubikey/yubikeymanager.h"
#include "streaming/streamingvisualize.h"
#include "system/controlmgr.h"
#include "system/EndUserBenchmark.h"
#include "system/EntitlementManager.h"
#include "system/param.h"
#include "system/SteamVerification.h"
#include "text/text.h"

#if __BANK
PARAM(askBeforeRestart, "Brings up a ready to restart message box before the game restarts.");
#endif

#if defined(MASTER_NO_SCUI)
namespace rage
{
	NOSTRIP_XPARAM(noSocialClub);
}
#elif !__FINAL

PARAM(noautoload, "[app] Don't attempt to autoload the most recent saved game on the loading screens");
PARAM(noprofilesignin, "[code] Don't check for a profile signin at game startup");
PARAM(stopLoadingScreensEarly, "[gamesessionstatemachine] Don't wait for script to stop the loading screens");

namespace rage 
{
	XPARAM(noSocialClub);
	XPARAM(unattended);
}

#if UI_LANDING_PAGE_ENABLED
PARAM(skipLandingPage, "Skip over the Landing Page");
#endif // UI_LANDING_PAGE_ENABLED

#endif

#if RSG_PC
#include "core/app.h"
#endif // RSG_PC

#if __WIN32PC
#include "rline/rlpc.h"
#include "SaveLoad/savegame_users_and_devices.h"
#endif

CGameSessionStateMachine::GameSessionState CGameSessionStateMachine::m_State = IDLE;
int CGameSessionStateMachine::m_Level = DEFAULT_START_LEVEL;
int CGameSessionStateMachine::m_ReInitLevel = DEFAULT_START_LEVEL;
bool CGameSessionStateMachine::m_ForceInitLevel = false;
bool CGameSessionStateMachine::m_StartedAutoload = false;

#if __WIN32PC
bool CGameSessionStateMachine::m_RGSCFailedInit = false;
bool CGameSessionStateMachine::m_bSupportURLDisplayed = false;
bool CGameSessionStateMachine::m_bEntitlementUpdating = false;
#endif

CGameSessionStateMachine::CGameSessionStateMachine()
{
}

CGameSessionStateMachine::~CGameSessionStateMachine()
{
}

#if RSG_PC
static bool is_SCui_showing()
{
	return g_rlPc.IsUiShowing();
}
#endif

void CGameSessionStateMachine::Init()
{
    m_Level = DEFAULT_START_LEVEL;
	//@@: location CGAMESESSIONSTATEMACHINE_INIT
    m_StartedAutoload = false;

#if RAGE_SEC_ENABLE_YUBIKEY
	SetState(TWO_FACTOR_AUTH_CHECK);
#else
#if __WIN32PC
	SetState(CHECK_FOR_RGSC);
	GRCDEVICE.CursorVisibilityOverride = is_SCui_showing;
#else
	SetState(INIT);
#endif //__WIN32PC
#endif //RAGE_SEC_ENABLE_YUBIKEY
}

void CGameSessionStateMachine::ReInit(int level)
{
#if RSG_ENTITLEMENT_ENABLED
	CEntitlementManager::Reset();
	if (UI_LANDING_PAGE_ONLY(m_State == ENTER_LANDING_PAGE ||) m_State == GAMMA_CALIBRATION)
	{
		SetState(REQUIRE_PC_ENTITLEMENT);
	}
	else if (m_State != REINIT && m_State != REQUIRE_PC_ENTITLEMENT)
	{
		SetState(REINIT);
		m_ReInitLevel = level;
	}
	else if (m_State == REQUIRE_PC_ENTITLEMENT || m_State == REINIT)
	{
		CEntitlementManager::StartCheck();
	}

	// url:bugstar:2307904 - Launching the benchmark from the landing page and then signing out of Social Club 
	//                       will causes the game to constantly load when signing back in.
	if (EndUserBenchmarks::IsBenchmarkScriptRunning() &&
		EndUserBenchmarks::GetWasStartedFromPauseMenu(FE_MENU_VERSION_LANDING_MENU))
	{
		CPauseMenu::SetGameWantsToRestart(true);
		NetworkInterface::GetNetworkExitFlow().StartShutdownTasks(); 
	}
#else
	StartNewGame(level);
#endif
}

#if RSG_PC
void CGameSessionStateMachine::ProcessReInitState()
{
	// Grab the level from the reinit call
	m_Level = m_ReInitLevel;
	CGameLogic::SetRequestedLevelIndex(m_ReInitLevel);

	// Start displaying loading screens and shutdown the current session
	StartDisplayingLoadingScreens();

	// Don't shutdown unless required
	if (CGame::IsSessionInitialized())
	{
		CGame::ShutdownSession();
	}

	// Change level if necessary
	if(m_Level != CGameLogic::GetCurrentLevelIndex())
	{
		CGame::ShutdownLevel();
		CGame::InitLevel(m_Level);
	}
	else if(!CGame::IsLevelInitialized() && m_Level == DEFAULT_START_LEVEL)
	{
		CGame::InitLevel(m_Level);
	}

	// Init the session
	CGame::InitSession(m_Level, false);

#if RSG_ENTITLEMENT_ENABLED
	extern bool g_GameRunning;
	if (g_GameRunning)
	{
		PF_POP_TIMEBAR(); // Update
	}

	m_bEntitlementUpdating = true;
	CEntitlementManager::StartCheck();
	while (CEntitlementManager::IsPending())
	{
		PF_FRAMEINIT_TIMEBARS(0);
		CApp::CheckExit();

		// Pump necessary function to prevent hang
		sysIpcSleep(33);
		fwTimer::Update();
		CControlMgr::StartUpdateThread();
		CControlMgr::WaitForUpdateThread();
		CNetwork::Update(true);
		CEntitlementManager::Update();		
		CWarningScreen::Update();
		CMousePointer::Update();

		if (g_GameRunning)
		{
			CScaleformMgr::UpdateAtEndOfFrame(false);
		}

		gRenderThreadInterface.FlushAndDeviceCheck();
		PF_FRAMEEND_TIMEBARS();
	}
	m_bEntitlementUpdating = false;

	if (g_GameRunning)
	{
		PF_PUSH_TIMEBAR_BUDGETED("Update", 20); // reenable Update
	}
#endif

	StopDisplayingLoadingScreens();
	
	PF_FINISH_STARTUPBAR();
	SetState(IDLE, true);
}

void CGameSessionStateMachine::ProcessCheckForRGSC()
{
	// Check for noSocialClub
#if !__FINAL || defined(MASTER_NO_SCUI)
	if (PARAM_noSocialClub.Get())
	{
		m_Level = CGameLogic::GetRequestedLevelIndex();
		SetState(LOAD_LEVEL);
		return;
	}
#endif

	// Check rgsc's initialization
	//@@: location CGAMESESSIONSTATEMACHINE_PROCESSCHECKFORRGSC
	if (!g_rlPc.IsInitialized())
	{
		m_RGSCFailedInit = true;
		SetState(HANDLE_RGSC_FAILED);
	}
	else
	{
		// RGSC Successful, continue with autoload
#if __STEAM_BUILD
	SetState(STEAM_VERIFICATION);
#else
	SetState(REQUIRE_PC_ENTITLEMENT);
#endif
	}
}

void CGameSessionStateMachine::ProcessRGSCFailed()
{
	// Stop here
	while(true)
	{
		sysIpcSleep(33);
		CApp::CheckExit();

		const char *pHeader = "MO_SC_ERR_HEAD";
		char pTextLabel[RAGE_MAX_PATH];
		char stringParam[128] = {0};

#if !RSG_FINAL
		// Print out the error number and enum
		#define SC_DEBUG "DEBUG_"
		#define ERROR_EVENT_CASE(x) case x: formatf(stringParam, "%d (%s)", x, #x); break;
		switch(g_rlPc.GetInitErrorCode())
		{
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_NONE);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_NO_DLL);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_NO_SC_COMMAND_LINE);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_PROGRAM_FILES_NOT_FOUND);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_LOAD_LIBRARY_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_GET_PROC_ADDRESS_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_CALL_EXPORTED_FUNC_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_QUERY_INTERFACE_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_UNKNOWN_ENV);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_INIT_CALL_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_GET_SUBSYSTEM_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_RGSC_NULL_AFTER_LOAD);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_CREATE_DEVICE_DX11);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_CREATE_DEVICE_DX9);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_NULL_PIPE);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_PIPE_CONNECT_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_SIGNATURE_VERIFICATION_FAILED);
			ERROR_EVENT_CASE(rlPc::SC_INIT_ERR_WEBSITE_FAILED_LOAD);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_RAGE);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_FILE_SYSTEM);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_JSON);
		case rgsc::FATAL_ERROR_INIT_METADATA:
			formatf(stringParam, "%d (Missing meta data file (./x64/metadata.dat in working directory: try force sync?)", g_rlPc.GetInitErrorCode()); break;
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_ACHIEVEMENT_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_PLAYER_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_GAMERPIC_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_PROFILE_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_PRESENCE_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_COMMERCE_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_TASK_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_UI);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_SUBPROCESS_NOT_FOUND);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_INIT_SUBPROCESS_WRONG_VERSION);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_TELEMETRY_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_STEAM_MANAGER);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_NETWORK_INTERFACE);
			ERROR_EVENT_CASE(rgsc::FATAL_ERROR_CLOUD_INTERFACE);
		default:
			formatf(stringParam, "%d", g_rlPc.GetInitErrorCode()); 
			break;
		}
#else
		#define SC_DEBUG ""
		formatf(stringParam, "%d", g_rlPc.GetInitErrorCode());
#endif

		switch(g_rlPc.GetInitErrorCode())
		{
		case rlPc::SC_INIT_ERR_NO_DLL:
			safecpy(pTextLabel, "MO_SC_ERR_" SC_DEBUG "NO_DLL");
			break;
		case rlPc::SC_INIT_ERR_QUERY_INTERFACE_FAILED:
			safecpy(pTextLabel, "MO_SC_ERR_" SC_DEBUG "OLD_VERSION");
			formatf(stringParam, g_rlPc.GetSocialClubVersion());
			break;
		case rlPc::SC_INIT_ERR_NO_SC_COMMAND_LINE:
			safecpy(pTextLabel, "MO_SC_ERR_NO_SC");
			break;
		default:
			safecpy(pTextLabel, "MO_SC_ERR_GENERIC");
			break;
		}

		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, pHeader, pTextLabel, FE_WARNING_OK_CANCEL, false, 0, stringParam);

		eWarningButtonFlags result = CWarningScreen::CheckAllInput();

		// TODO: Maybe see about moving this to a nicer place
		if (!m_bSupportURLDisplayed &&
			result == FE_WARNING_OK)
		{
			const char* url = "https://support.rockstargames.com/hc/articles/204075496";
			ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWMAXIMIZED);
			m_bSupportURLDisplayed = true;
			CPauseMenu::SetGameWantsToExitToWindows(true);
		}
		else if (result == FE_WARNING_CANCEL)
		{
#if __FINAL
			CPauseMenu::SetGameWantsToExitToWindows(true);
#else
			// Allow dev users to continue into the game
			CWarningScreen::Remove();
			m_RGSCFailedInit = false;
			ShutdownInitialDialogueScreensOnly();
			m_Level = CGameLogic::GetRequestedLevelIndex();
			SetState(LOAD_PROFILE, true);
			return;
#endif
		}

		UpdateInitialDialogueScreensOnly();
		gRenderThreadInterface.FlushAndDeviceCheck();
	}
}

#if __STEAM_BUILD
void CGameSessionStateMachine::ProcessSteamVerification()
{
	CSteamVerification::Init();
	while(true)
	{
		sysIpcSleep(33);
		CApp::CheckExit();

		if(CSteamVerification::IsPending() || !CSteamVerification::IsInitialized())
		{
			CSteamVerification::Update();
		}
		else if(CSteamVerification::IsFailed())
		{
#if STEAM_VERIFY_WITH_CALLBACKS
			if(CSteamVerification::GetFailureType() == rlSteam::STEAM_FAILURE_GET_AUTH_SESSION)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, 
					"STEAM_ERROR", 
					"STEAM_AUTH_FAILED", 
					FE_WARNING_OK);
			}
			else if(CSteamVerification::GetFailureType() == rlSteam::STEAM_FAILURE_BEGIN_AUTH_SESSION_RESPONSE || 
					CSteamVerification::GetFailureType() == rlSteam::STEAM_FAILURE_BEGIN_AUTH_SESSION_RESULT )
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, 
				"STEAM_ERROR", 
				"STEAM_BEGIN_AUTH_SESSION_FAILED", 
				FE_WARNING_OK);
			}
#else
			if(CSteamVerification::GetFailureType() == rlSteam::STEAM_FAILURE_NOT_SUBSCRIBED)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, 
					"STEAM_ERROR", 
					"STEAM_AUTH_FAILED", 
					FE_WARNING_OK);
			}
#endif
			eWarningButtonFlags result = CWarningScreen::CheckAllInput();
			if (result == FE_WARNING_OK)
			{
				gnetDebug1("Update :: Offline, Shutdown requested");
				CPauseMenu::SetGameWantsToExitToWindows(true);
			}
		}
		else
			break;

		UpdateInitialDialogueScreensOnly();
		gRenderThreadInterface.FlushAndDeviceCheck();
	}
#if RSG_ENTITLEMENT_ENABLED
	SetState(REQUIRE_PC_ENTITLEMENT);
#else
	SetState(LOAD_PROFILE);
#endif
}
#endif

void CGameSessionStateMachine::ProcessRequirePCEntitlement(void)
{
#if RSG_ENTITLEMENT_ENABLED
	CEntitlementManager::StartCheck();
	
	m_bEntitlementUpdating = true;
	while (CEntitlementManager::IsPending())
	{
		// Pump necessary function to prevent hang
		sysIpcSleep(33);

		WIN32PC_ONLY(CApp::CheckExit());
		// Update entitlement manager before UpdateInitialDialogueScreensOnly, because UpdateInitialDialogueScreensOnly will update the warning screen, clearing flags for any warning screens created in CApp::CheckExit
		CEntitlementManager::Update(); 

		UpdateInitialDialogueScreensOnly();

#if __D3D11
		gRenderThreadInterface.FlushAndDeviceCheck();
#endif // __D3D11

		// SCUI has initialized but failed to load the website. Set the init error code and transition
		// to the RGSC_FAILED_STATE to present the user with a failure message.
		if (g_rlPc.GetUiInterface()->IsInFailState())
		{
			g_rlPc.SetInitErrorCode(rlPc::SC_INIT_ERR_WEBSITE_FAILED_LOAD);
			SetState(HANDLE_RGSC_FAILED, true);
			return;
		}
	}
#endif // RSG_ENTITLEMENT_ENABLED
	m_bEntitlementUpdating = false;

	SetState(LOAD_PROFILE, true);
}

#endif // RSG_PC

#if UI_LANDING_PAGE_ENABLED
void CGameSessionStateMachine::ReturnToLandingPage(const ReturnToLandingPageReason UNUSED_PARAM(reason))
{
#if GEN9_LANDING_PAGE_ENABLED
	ScriptRouterLink::ClearScriptRouterLink();
	CGame::ShutdownSession();

	CGenericGameStorage::ResetForEpisodeChange();

	SetState(ENTER_LANDING_PAGE);
#endif // GEN9_LANDING_PAGE_ENABLED
}
#endif // UI_LANDING_PAGE_ENABLED

/*void CGameSessionStateMachine::StartAutoload()
{
    Assert(m_State == IDLE); // can't start a new game while the session state machine is active yet
    m_State = START_AUTOLOAD;
    m_Level = 0;
    m_StartedAutoload = true;

    // do an update know as this is a synchronous operation that is not handled
    Update();
}*/

void CGameSessionStateMachine::StartNewGame(int level)
{
    Assert(m_State == IDLE); // can't start a new game while the session state machine is active yet

    if (!CGame::IsSessionInitialized())
        return;

	SetState(START_NEW_GAME);
    m_Level = level;

    CGameLogic::SetRequestedLevelIndex(level);
}

void CGameSessionStateMachine::LoadSaveGame()
{
    Assert(m_State == IDLE); // can't start a new game while the session state machine is active yet
    SetState(START_LOADING_SAVE_GAME);
}

void CGameSessionStateMachine::HandleLoadedSaveGame()
{
//    Assert(m_State == IDLE); // can't start to deserialize a savegame while the session state machine is active

	Assertf(m_State == WAIT_FOR_SAVE_GAME, "Expected state to be WAIT_FOR_SAVE_GAME when loading from the menu. (It might have a different value if using the RAG widgets to load from your PC)");
    SetState(HANDLE_LOADED_SAVE_GAME);
}

void CGameSessionStateMachine::SetStateToIdle()
{
	SetState(IDLE);
}

void CGameSessionStateMachine::OnSignedOffline()
{
	Displayf("CGameSessionStateMachine::OnSignedOffline");
}

void CGameSessionStateMachine::OnSignOut()
{
	Displayf("CGameSessionStateMachine::OnSignOut");
}

void CGameSessionStateMachine::ChangeLevel(int level)
{
    Assert(m_State == IDLE); // can't start a new game while the session state machine is active yet
    SetState(CHANGE_LEVEL);
    m_Level = level;

    CGameLogic::SetRequestedLevelIndex(level);
}

void CGameSessionStateMachine::Update()
{
    int oldState = m_State;

#if __ASSERT
    unsigned numStateChanges = 0;
#endif // __ASSERT

    do
    {
        oldState = m_State;

#if RSG_PC
		CApp::CheckExit();
#endif

		//@@: range CGAMESESSIONSTATEMACHINE_SWITCH_SECTION {
		switch(m_State)
		{
		case IDLE:
			ProcessIdleState();
			break;
		case INIT:
			ProcessInitState();
			break;
#if __WIN32PC
		case REINIT:
			ProcessReInitState();
			break;
		case CHECK_FOR_RGSC:
			ProcessCheckForRGSC();
			break;
		case HANDLE_RGSC_FAILED:
			ProcessRGSCFailed();
			break;
#if __STEAM_BUILD
		case STEAM_VERIFICATION:
			ProcessSteamVerification();
			break;
#endif
		case REQUIRE_PC_ENTITLEMENT:
			ProcessRequirePCEntitlement();
			break;
		case TWO_FACTOR_AUTH_CHECK:
			ProcessTwoFactorAuthCheck();
			break;
#endif	// __WIN32PC
		case CHECK_SIGN_IN_STATE:
			ProcessCheckSignInState();
			break;
		case LOAD_PROFILE:
			LoadProfileState();
			break;
#if !__PS3
		case CHOOSE_LANGUAGE:
			ChooseLanguageState();
			break;
#endif
		case GAMMA_CALIBRATION:
			ProcessGammaCalibration();
			break;
		case CHECK_FOR_INVITE:
			ProcessCheckForInviteState();
			break;
		case START_AUTOLOAD:
			ProcessStartAutoloadState();
			break;
		case ENUMERATE_SAVED_GAMES:
			ProcessEnumerateSavedGamesState();
			break;
		case AUTOLOAD_SAVE_GAME:
			ProcessAutoLoadSaveGameState();
			break;
		case START_NEW_GAME:
			ProcessStartNewGameState();
			break;
		case START_LOADING_SAVE_GAME:
			ProcessStartLoadingSaveGameState();
			break;
		case WAIT_FOR_SAVE_GAME:
			ProcessWaitForSaveGameState();
			break;
		case HANDLE_LOADED_SAVE_GAME:
			ProcessHandleLoadedSaveGameState();
			break;
		case LOAD_LEVEL:
			ProcessLoadLevelState();
			break;
		case CHANGE_LEVEL:
			ProcessChangeLevelState();
            break;
#if UI_LANDING_PAGE_ENABLED
		case SHUTDOWN_FOR_LANDING_PAGE:
			ProcessShutdownForLandingPage();
			break;
		case ENTER_LANDING_PAGE:
			ProcessEnterLandingPage();
			break;
		case DISMISS_LANDING_PAGE:
			ProcessDismissLandingPage();
			break;
#endif // UI_LANDING_PAGE_ENABLED
		default:
			{
				Assertf(0, "Unexpected state that is not handled");
				SetState(IDLE);
			}
			break;
		}
		//@@: } CGAMESESSIONSTATEMACHINE_SWITCH_SECTION


        Assertf(numStateChanges++ < MAX_STATE_CHANGES, "Possible infinite loop in state machine detected: %u changes", numStateChanges);
    }
    while(m_State != oldState);
}

void CGameSessionStateMachine::ProcessIdleState()
{
    if(m_StartedAutoload)
    {
        CSavegameAutoload::ResetAfterAutoloadAtStartOfGame();
        CGameLogic::SetRequestedLevelIndex(CGameLogic::GetRequestedLevelIndex());
        m_StartedAutoload = false;
	}
}

void CGameSessionStateMachine::ProcessInitState()
{
	SetState( CHECK_SIGN_IN_STATE );
}

void CGameSessionStateMachine::ProcessStartAutoloadState()
{
    if (!CFileMgr::IsGameInstalled()
#if !__FINAL
        || (PARAM_noautoload.Get() && !CNetwork::GetGoStraightToMultiplayer()) // If the user selects GoStraightIntoMultiplayer via command line or landing page
#endif																			// we want this functionality to work despite noautoload.
		)
	{
		m_Level = CGameLogic::GetRequestedLevelIndex();
		SetState(LOAD_LEVEL);
	}
	else
	{
		m_Level = 0;
		m_StartedAutoload = true;
		CSavegameAutoload::InitialiseAutoLoadAtStartOfGame();
#if __WIN32PC
		if (m_RGSCFailedInit)
		{
			SetState(HANDLE_RGSC_FAILED);
		}
		else
#endif
		{
			SetState(ENUMERATE_SAVED_GAMES);
		}
	}
}

void CGameSessionStateMachine::ProcessCheckSignInState()
{
#if defined(MASTER_NO_SCUI) || !__FINAL
#if defined(MASTER_NO_SCUI)
	if (PARAM_noSocialClub.Get())
#elif !__FINAL && __WIN32PC
	if(PARAM_noprofilesignin.Get() || PARAM_noSocialClub.Get() || PARAM_unattended.Get())
#elif !__FINAL && !__WIN32PC
	if(PARAM_noprofilesignin.Get() || PARAM_unattended.Get())
#endif
	{
		// Load the required level
		m_Level = CGameLogic::GetRequestedLevelIndex();
		SetState(LOAD_LEVEL);
		return;
	}
#endif //defined(MASTER_NO_SCUI) || !__FINAL

	// check whether the player is signed in
	CSavegameAutoload::ResetAutoLoadSignInState();
	eAutoLoadSignInReturnValue checkSignInStatus = CSavegameAutoload::AutoLoadCheckForSignedInPlayer();

	while(checkSignInStatus == AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING)
	{
		sysIpcSleep(33);

		checkSignInStatus = CSavegameAutoload::AutoLoadCheckForSignedInPlayer();

		// If the state is not still AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING, then any dialogue is closed, so tell the game logic about it.
		if(checkSignInStatus != AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING )
		{
			CSavegameDialogScreens::SetMessageAsProcessing(false);
		}
		// Call after AutoLoadCheckForSignedInPlayer() so it can resume the loading screens when dialogues are closed.
		UpdateInitialDialogueScreensOnly();
	}

	if(checkSignInStatus == AUTOLOAD_SIGN_IN_RETURN_PLAYER_IS_SIGNED_IN)
	{
		CLiveManager::Update(sysTimer::GetSystemMsTime());
#if __STEAM_BUILD
		SetState(STEAM_VERIFICATION);
#else
#if RSG_ENTITLEMENT_ENABLED
		SetState(REQUIRE_PC_ENTITLEMENT);
#else
		SetState(LOAD_PROFILE);
#endif
#endif

		
	}
	else
	{
		Assert(checkSignInStatus == AUTOLOAD_SIGN_IN_RETURN_PLAYER_NOT_SIGNED_IN);

		ShutdownInitialDialogueScreensOnly();

		// Load the required level
		m_Level = CGameLogic::GetRequestedLevelIndex();
		SetState(LOAD_LEVEL);
	}
}

void CGameSessionStateMachine::LoadProfileState()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	settings.UpdateProfileSettings(true);

#if !__PS3
	SetState(CHOOSE_LANGUAGE);
#else 
	SetState(GAMMA_CALIBRATION);
#endif
}

void CGameSessionStateMachine::ChooseLanguageState()
{
#if !__PS3
	// 360 profile is loaded by this point so we can work out which language to select
	CLanguageSelect::ShowLanguageSelectScreen();
#endif

	u32 sysLanguage = CPauseMenu::GetLanguageFromSystemLanguage();
	if(sysLanguage == LANGUAGE_UNDEFINED)
		sysLanguage = LANGUAGE_ENGLISH;

	u32 profileLanguage = sysLanguage;
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if( settings.AreSettingsValid() && settings.Exists(CProfileSettings::DISPLAY_LANGUAGE) )
	{
		profileLanguage = settings.GetInt(CProfileSettings::DISPLAY_LANGUAGE);
	}

	if( strcmp(CText::GetLanguageFilename(profileLanguage), CText::GetLanguageFilename(sysLanguage) ))
	{
		CGameStreamMgr::Shutdown(SHUTDOWN_CORE);
		CLoadingScreens::Shutdown(SHUTDOWN_CORE);
		CLoadingScreens::SetShutdown(true);

		gRenderThreadInterface.SetDefaultRenderFunction();
		gRenderThreadInterface.Flush(true);

		// Need to update loading screens once and then scaleform 3 times to actually set the movies to be
		// available for deletion and then tell the scaleform store to delete all unreferenced entries
		CLoadingScreens::Update();
		CText::Shutdown(SHUTDOWN_CORE);
		CScaleformMgr::KillAllMovies();
		CScaleformMgr::UpdateMoviesUntilReadyForDeletion();
		g_ScaleformStore.RemoveAll();

		CText::Init(INIT_CORE);
		TheText.Load(false);

		CLoadingScreens::SetShutdown(false);
		CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_LOADLEVEL, 0);

		CGameStreamMgr::Init(INIT_CORE);
	}
	else if(profileLanguage != sysLanguage)
	{
		TheText.ReloadAfterLanguageChange();
	}
	SetState(GAMMA_CALIBRATION);
}

void CGameSessionStateMachine::ProcessGammaCalibration()
{
	if(CDisplayCalibration::ShouldActivateAtStartup())
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();

		CLoadingScreens::SleepUntilPastLegalScreens();

		CLoadingScreens::Suspend();

		CDisplayCalibration::LoadCalibrationMovie();
		CDisplayCalibration::Update();

		CPauseMenu::UpdateProfileFromMenuOptions();  // update the profile with the new gamma settings
		settings.Write();

		CDisplayCalibration::RemoveCalibrationMovie();
		CLoadingScreens::Resume();
	}

	SetState(CHECK_FOR_INVITE);
}

void CGameSessionStateMachine::ProcessCheckForInviteState()
{
	CSavegameAutoload::ResetAutoLoadInviteState();
	eAutoLoadInviteReturnValue checkInviteStatus = CSavegameAutoload::AutoLoadCheckForAcceptingInvite();

    while(checkInviteStatus == AUTOLOAD_INVITE_RETURN_STILL_CHECKING_INVITE)
    {
        sysIpcSleep(33);
        UpdateInitialDialogueScreensOnly();

        checkInviteStatus = CSavegameAutoload::AutoLoadCheckForAcceptingInvite();
    }

	if(checkInviteStatus == AUTOLOAD_INVITE_RETURN_NO_INVITE)
	{
#if UI_LANDING_PAGE_ENABLED
        if( CLandingPageArbiter::ShouldShowLandingPageOnBoot() )
		{
			SetState(ENTER_LANDING_PAGE);
		}
        else
#endif // UI_LANDING_PAGE_ENABLED
		{
#if !__NO_OUTPUT
			if ( CLandingPageArbiter::IsSkipLandingPageSettingEnabled() )
			{
				gnetDebug1("Skipping landing page due to user preferences.");
			}
#endif
			SetState(START_AUTOLOAD); // Begin loading
		}
	}
	else
	{
		Assert(checkInviteStatus == AUTOLOAD_INVITE_RETURN_INVITE_ACCEPTED);

		ShutdownInitialDialogueScreensOnly();

		m_Level = CSavegameAutoload::GetEpisodeNumberForTheSaveGameToBeLoaded();
		SetState(LOAD_LEVEL);
	}
}

void CGameSessionStateMachine::ProcessEnumerateSavedGamesState()
{
#if !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	CSavegameAutoload::ResetAutoLoadFileEnumerationState();
    eAutoLoadEnumerationReturnValue checkEnumerateStatus = CSavegameAutoload::AutoLoadEnumerateContent();

    while(checkEnumerateStatus == AUTOLOAD_ENUMERATION_RETURN_STILL_CHECKING)
    {
        sysIpcSleep(33);
        UpdateInitialDialogueScreensOnly();

        checkEnumerateStatus = CSavegameAutoload::AutoLoadEnumerateContent();
    }

    Assert(checkEnumerateStatus == AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED ||
           checkEnumerateStatus == AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_COMPLETE);
#endif // !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

	CSavegameAutoload::ResetAutoLoadDecisionState();
	eAutoLoadDecisionReturnValue checkForLoad = CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame();

	while(checkForLoad == AUTOLOAD_DECISION_RETURN_STILL_CHECKING)
	{
		sysIpcSleep(33);
		UpdateInitialDialogueScreensOnly();

		checkForLoad = CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame();
	}

#if __PPU
	CSavegameAutoload::SetAutoloadEnumerationHasCompleted();
#endif	//	__PPU

	if(checkForLoad == AUTOLOAD_DECISION_RETURN_START_NEW_GAME)
	{
		ShutdownInitialDialogueScreensOnly();

		m_Level = CSavegameAutoload::GetEpisodeNumberForTheSaveGameToBeLoaded();
		SetState(LOAD_LEVEL);
	}
	else
	{
		Assert(checkForLoad == AUTOLOAD_DECISION_RETURN_DO_AUTOLOAD);
		SetState(AUTOLOAD_SAVE_GAME);
	}
}

void CGameSessionStateMachine::ProcessAutoLoadSaveGameState()
{
	CSavegameAutoload::BeginGameAutoload();

    Assertf(CGenericGameStorage::IsSavegameLoadInProgress(), "Expected a game load to be in progress here");

	GameSessionState newState = IDLE;
    MemoryCardError loadStatus = CSavegameLoad::GenericLoad();

    while(loadStatus == MEM_CARD_BUSY)
    {
        sysIpcSleep(33);
        UpdateInitialDialogueScreensOnly();

        loadStatus = CSavegameLoad::GenericLoad();
    }

    ShutdownInitialDialogueScreensOnly();

	m_Level = CSavegameAutoload::GetEpisodeNumberForTheSaveGameToBeLoaded();

    if(loadStatus == MEM_CARD_COMPLETE)
    {
		ProcessLoadLevelState(true);

		CNetwork::Update(true);

		if (CSavegameLoad::DeSerialize() == MEM_CARD_ERROR)
		{	
			//	Deserialize failed

//	Copied from CGameSessionStateMachine::ProcessHandleLoadedSaveGameState
			Displayf("CGameSessionStateMachine::ProcessAutoLoadSaveGameState - CSavegameLoad::DeSerialize failed");

#if RSG_PC
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_MOST_RECENT_PC_SAVE_IS_DAMAGED);
			while (!CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				sysIpcSleep(33);
				UpdateInitialDialogueScreensOnly();
			}

			ShutdownInitialDialogueScreensOnly();
#endif	//	RSG_PC

#if GEN9_LANDING_PAGE_ENABLED

			CGame::ShutdownSession();

			CGenericGameStorage::ResetForEpisodeChange();

			newState = ENTER_LANDING_PAGE;
#else
//			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{


				// Neither of these fades seems to be enough to get the Loading screen
				//	to reappear for the RestartSessionOrMap
				//	Will need to look at this again later
				//			CFrontEnd::sm_Fader[FRONTEND_FADER_SCREENFADE].StartFullFade(0, 255, 0);  // fade back in
				camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

				StartDisplayingLoadingScreens();

				CGame::ShutdownSession();

//	Graeme - if we ever have more than one level then I'll uncomment this and test that it's correct. I'm not sure if GetCurrentLevelIndex() will return the correct value at this stage.
// 				if(m_Level != CGameLogic::GetCurrentLevelIndex())
// 				{
// 					CGame::ShutdownLevel();
// 					CGame::InitLevel(m_Level);
// 				}

				CGame::InitSession(m_Level, false);
				StopDisplayingLoadingScreens();

				CGenericGameStorage::ResetForEpisodeChange();

			}
#endif // UI_LANDING_PAGE_ENABLED
// End of Copied from CGameSessionStateMachine::ProcessHandleLoadedSaveGameState
		}
		else
		{	//	Deserialize succeeded
			CGame::InitLevelAfterLoadingFromMemoryCard(m_Level);	//	This used to pass in CGameLogic::GetRequestedLevelIndex() but I'm going to try m_Level
			CGenericGameStorage::DoGameSpecificStuffAfterSuccessLoad(true);

//			camInterface::FadeIn(VIEWPORT_LEVEL_INIT_FADE_TIME);
		}
    }
    else
    {	//	The load failed
        Assert(loadStatus == MEM_CARD_ERROR);

		ProcessLoadLevelState(false);
    }

	SetState(newState);
}

void CGameSessionStateMachine::ProcessTwoFactorAuthCheck()
{
#if RAGE_SEC_ENABLE_YUBIKEY
	while (!CYubiKeyManager::GetInstance()->IsVerified() || CYubiKeyManager::GetInstance()->IsYubiKeyConnected())
	{
		// Pump necessary function to prevent hang
		sysIpcSleep(33);

		WIN32PC_ONLY(CApp::CheckExit());

		rtry
		{
			rcheck(!CLoadingScreens::IsDisplayingLegalScreen(), forwardLoop, );

			bool const isYubiKeyConnected = CYubiKeyManager::GetInstance()->IsYubiKeyConnected();
			bool const isVerified = CYubiKeyManager::GetInstance()->IsVerified();
			u32 const attempts = CYubiKeyManager::GetInstance()->GetVerificationAttempts();

			if (!isYubiKeyConnected && !isVerified)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "TWO_FACTOR_AUTH_CHECK", "YUBIKEY_INSERT", FE_WARNING_NONE);
			}
			else if (isYubiKeyConnected && isVerified)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "TWO_FACTOR_AUTH_CHECK", "YUBIKEY_REMOVE", FE_WARNING_NONE);
			}
			else if (isYubiKeyConnected && !isVerified)
			{
				if (attempts == 0)
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "TWO_FACTOR_AUTH_CHECK", "YUBIKEY_WAITING_FOR_INPUT", FE_WARNING_NONE);
				}
				else
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "TWO_FACTOR_AUTH_CHECK", "YUBIKEY_INVALID_INPUT", FE_WARNING_EXIT);
				}
			}
			
			// Pump necessary function to prevent hang
			UpdateInitialDialogueScreensOnly();

#if RSG_PC && __D3D11
			gRenderThreadInterface.FlushAndDeviceCheck();
#endif // RSG_PC && __D3D11
		}
		rcatch(forwardLoop)
		{
			continue;
		}
	}
#endif // ENABLE_SECURITY_YUBIKEY

#if RSG_PC
	SetState(CHECK_FOR_RGSC);
#else
	SetState(INIT);
#endif //RSG_PC
}

#if __BANK
namespace rage
{
	XPARAM(nopopups);
}

void CGameSessionStateMachine::AskBeforeRestart()
{
    if (PARAM_askBeforeRestart.Get())
    {
		const s32 LengthOfBackUpString = 32;
		//	sysParam::Set() has a note saying "The pointer must point into statically allocated storage"
		static char noPopUpsBackUpString[LengthOfBackUpString];

		bool bNoPopUpsWasSet = false;

		const char *pNoPopUpsSettingString;
		if (PARAM_nopopups.Get(pNoPopUpsSettingString))
		{
			bNoPopUpsWasSet = true;
			Assertf(strlen(pNoPopUpsSettingString)<LengthOfBackUpString, "CGameSessionStateMachine::AskBeforeRestart - nopopups setting string is too long");
			safecpy(noPopUpsBackUpString, pNoPopUpsSettingString, LengthOfBackUpString);

			PARAM_nopopups.Set(NULL);	//	temporarily enable pop-ups
		}

		// Make sure text is unloaded since it's in an RPF now.  Otherwise it doesn't get reloaded until later.
		TheText.Unload();

		bkMessageBox("Ask Before Restart", "The game is paused now and image files can be changed. Press Okay when you are ready to continue?", bkMsgOk, bkMsgQuestion);

		if (bNoPopUpsWasSet)
		{
			PARAM_nopopups.Set(noPopUpsBackUpString);
		}

		TheText.Load();
	}
}
#endif

void CGameSessionStateMachine::ProcessStartNewGameState()
{
#if GEN9_LANDING_PAGE_ENABLED
	// Extend normal flow for loading into singleplayer to check for story mode entitlement
	const bool allowedToEnterStoryMode = UserAllowedToEnterStoryMode();
	if (!allowedToEnterStoryMode)
	{
		// Story mode is unavailable, show upsell instead
		ShowStoryModeUpsell();
	}
	else
#endif // GEN9_LANDING_PAGE_ENABLED
	{
		bool fadeFinished = ProcessRestartFade();
		ConductorMessageData messageData;
		messageData.conductorName = SuperConductor;
		messageData.message = MuteGameWorld;
		SUPERCONDUCTOR.SendConductorMessage(messageData);
		if (fadeFinished)
		{
			PF_FINISH_STARTUPBAR();

#if __BANK
			if (!PARAM_askBeforeRestart.Get())
#endif
			{
				// always start the loading screens on "new game"
				StartDisplayingLoadingScreens();
			}

			CGame::ShutdownSession();

			if (m_ForceInitLevel || m_Level != CGameLogic::GetCurrentLevelIndex())
			{
				CGame::ShutdownLevel();
			}

#if __BANK
			AskBeforeRestart();
#endif

			if (m_ForceInitLevel || m_Level != CGameLogic::GetCurrentLevelIndex())
			{
				CGame::InitLevel(m_Level);
			}

			m_ForceInitLevel = false;

			CGame::InitSession(m_Level, false);

			// stop the loading screens once we have loaded
			StopDisplayingLoadingScreens();

			PF_FINISH_STARTUPBAR();

			SetState(IDLE);
		}
	}
}

void CGameSessionStateMachine::ProcessStartLoadingSaveGameState()
{
// The old contents of this function have been moved to the MANUAL_LOAD_PROGRESS_BEGIN_LOAD case inside CSavegameQueuedOperation_ManualLoad::Update()

    SetState(WAIT_FOR_SAVE_GAME);
}

void CGameSessionStateMachine::ProcessWaitForSaveGameState()
{
	Assertf(CGenericGameStorage::IsSavegameLoadInProgress(), "Expected a game load to be in progress here");

// The old contents of this function have been moved to the MANUAL_LOAD_PROGRESS_CHECK_LOAD case inside CSavegameQueuedOperation_ManualLoad::Update()
}

void CGameSessionStateMachine::ProcessHandleLoadedSaveGameState()
{
	Assertf(CGenericGameStorage::IsSavegameLoadInProgress(), "CGame::HandleGameLoading - about to DeSerialize but no load is in progress");

#if GEN9_LANDING_PAGE_ENABLED
	// Extend normal flow for loading into singleplayer to check for story mode entitlement
	if (!CNetwork::GetGoStraightToMultiplayer() && !UserAllowedToEnterStoryMode())
	{
		// Story mode is unavailable, redirect to landing page
		coreErrorf("Attempting to load into singleplayer without story mode entitlement");
		SetState(ENTER_LANDING_PAGE);
	}
	else
#endif // GEN9_LANDING_PAGE_ENABLED
	{
		PF_FINISH_STARTUPBAR();
		STRVIS_SET_MARKER_TEXT("Process Handle Loaded Save Game");

		GameSessionState newState = IDLE;

		StartDisplayingLoadingScreens();

		m_Level = CSavegameList::GetEpisodeNumberForTheSaveGameToBeLoaded();

		CGame::ShutdownSession();

		if (m_Level != CGameLogic::GetCurrentLevelIndex())
		{
			CGame::ShutdownLevel();
		}

#if __BANK
		AskBeforeRestart();
#endif

		if (m_Level != CGameLogic::GetCurrentLevelIndex())
		{
			CGame::InitLevel(m_Level);
		}

		CGame::InitSession(m_Level, true);
		StopDisplayingLoadingScreens();

		if (CSavegameLoad::DeSerialize() == MEM_CARD_ERROR)
		{
			Displayf("CGameSessionStateMachine::ProcessHandleLoadedSaveGameState - CSavegameLoad::DeSerialize failed");

#if RSG_PC
			PF_POP_TIMEBAR(); // Update

			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_PC_SAVE);
			while (!CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				UpdateLoadFailedDialogueScreens();
			}

			ShutdownInitialDialogueScreensOnly();

			PF_PUSH_TIMEBAR_BUDGETED("Update", 20); // reenable Update
#else	//	RSG_PC
			Assertf(0, "CGameSessionStateMachine::ProcessHandleLoadedSaveGameState - CSavegameLoad::DeSerialize failed");
#endif	//	RSG_PC

			// GTA B*1393626 - Assert if CSavegameLoad::DeSerialize() fails
			//	Seems a bit dodgy to remove this call, but then it's only been getting one frame to run for the last 3 years and no-one has noticed
			//		if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				// Neither of these fades seems to be enough to get the Loading screen
				//	to reappear for the RestartSessionOrMap
				//	Will need to look at this again later
	//			CFrontEnd::sm_Fader[FRONTEND_FADER_SCREENFADE].StartFullFade(0, 255, 0);  // fade back in
				camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

				StartDisplayingLoadingScreens();

				CGame::ShutdownSession();

#if GEN9_LANDING_PAGE_ENABLED
				CGenericGameStorage::ResetForEpisodeChange();

				newState = ENTER_LANDING_PAGE;
#else
				if (m_Level != CGameLogic::GetCurrentLevelIndex())
				{
					CGame::ShutdownLevel();
					CGame::InitLevel(m_Level);
				}

				CGame::InitSession(m_Level, false);
				StopDisplayingLoadingScreens();

				CGenericGameStorage::ResetForEpisodeChange();
#endif
			}
		}
		else
		{
			CGame::InitLevelAfterLoadingFromMemoryCard(m_Level);
			CGenericGameStorage::DoGameSpecificStuffAfterSuccessLoad(true);
			CPauseMenu::SetValuesBasedOnCurrentSettings(CPauseMenu::UPDATE_PREFS_PROFILE_CHANGE);
		}

		PF_FINISH_STARTUPBAR();
		SetState(newState);
	}
}

void CGameSessionStateMachine::ProcessChangeLevelState()
{
    bool fadeFinished = ProcessRestartFade();

    if(fadeFinished)
    {
        StartDisplayingLoadingScreens();

        CGame::ShutdownSession();
        CGame::ShutdownLevel();
		//@@: range CGAMESTATEMACHINE_PROCESSCHANGELEVELSTATE_INIT_LEVEL {
        CGame::InitLevel(m_Level);
		//@@: } CGAMESTATEMACHINE_PROCESSCHANGELEVELSTATE_INIT_LEVEL
        CGame::InitSession(m_Level, false);
        StopDisplayingLoadingScreens();

        SetState(IDLE);
    }
}

#if UI_LANDING_PAGE_ENABLED

void CGameSessionStateMachine::ProcessShutdownForLandingPage()
{
	camInterface::FadeOut(0);
	CNetwork::ClearTransitionTracker();

	if (!CLoadingScreens::AreShutdown())
	{
		CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_LANDING, 0);
	}

	ReturnToLandingPage();
}

void CGameSessionStateMachine::ProcessEnterLandingPage()
{
	// Ensure legal screens are finished showing
	CLoadingScreens::SleepUntilPastLegalScreens();

#if GEN9_LANDING_PAGE_ENABLED
    if( Gen9LandingPageEnabled() )
    {
        CLandingPage::Launch();
        // Modern landing page sets idle and runs a specific update in game.cpp
        SetState(IDLE);
    }
#if RSG_PC
    else
#endif // RSG_PC
#endif // GEN9_LANDING_PAGE_ENABLED
#if RSG_PC
    {
        CLegacyLandingScreen::Init();

        // whereas legacy landing spins on landing update, which in turn contains
        // all the relevant updates required
        // TODO_UI - Homogenize these as they will likely be 95% equal?
        while(!SLegacyLandingScreen::GetInstance().Update());

        CLegacyLandingScreen::Shutdown();
        SetState(START_AUTOLOAD);
    }
#endif // RSG_PC
}

bool IsValidMultiplayerGameMode(MultiplayerGameMode eGameMode)
{
	switch (eGameMode)
	{
	case GameMode_Invalid:
		return false;
	case GameMode_Editor:
		return false;
	case GameMode_Creator:
		return false;

	default:
		return true;
	}
}

void CGameSessionStateMachine::ProcessDismissLandingPage()
{
#if GEN9_LANDING_PAGE_ENABLED 
	CLandingPage::Dismiss();

	// If there is no game session, it should be created now
	if (!CGame::IsSessionInitialized())
	{		

		// Determine which type of game session we're creating
		MultiplayerGameMode eGameMode = GameMode_Freeroam;
		if (ScriptRouterLink::HasPendingScriptRouterLink())
		{
			ScriptRouterContextData contextData;
			if (ScriptRouterLink::ParseScriptRouterLink(contextData))
			{
				eGameMode = ScriptRouterContext::GetGameModeFromSRCData(contextData);
			}
			else
			{
				coreErrorf("Failed to parse ScriptRouterLink, defaulting game mode to %d", eGameMode);
			}
		}
		else
		{
			coreDebugf1("No ScriptRouterLink set, defaulting game mode to %d", eGameMode);
		}

		const bool bIsMultiplayer = IsValidMultiplayerGameMode(eGameMode);
		const bool bIsSingleplayer = !bIsMultiplayer;

		CNetwork::SetGoStraightToMultiplayer(bIsMultiplayer);
		CNetwork::SetGoStraightToMPEvent(false);
		CNetwork::SetGoStraightToMPRandomJob(false);
		CLoadingScreens::SetScreenOrder(bIsSingleplayer);
		CLoadingScreens::SetNewsScreenOrder(bIsSingleplayer);
		CLoadingScreens::CommitToMpSp();

		SetState(START_AUTOLOAD);
	}
	else
#endif // GEN9_LANDING_PAGE_ENABLED
	{
		SetState(IDLE);
	}
}

#endif // UI_LANDING_PAGE_ENABLED

void CGameSessionStateMachine::ProcessLoadLevelState(bool bLoadingSavegame)
{
#if __PPU
	CSavegameAutoload::SetAutoloadEnumerationHasCompleted();
#endif	//	__PPU

#if GEN9_LANDING_PAGE_ENABLED
	// Extend normal flow for loading into singleplayer to check for story mode entitlement
	if (!CNetwork::GetGoStraightToMultiplayer()
		&& !UserAllowedToEnterStoryMode()
		&& Gen9LandingPageEnabled()) // Ensure the Gen9 boot flow is enabled before attempting to redirect to the Landing Page
	{
		// Story mode is unavailable, redirect to landing page
		coreErrorf("Attempting to load into singleplayer without story mode entitlement");

		// If we came here from a ScriptRouterLink, it should be cleared to prevent a redirect loop
		if (ScriptRouterLink::HasPendingScriptRouterLink())
		{
			ScriptRouterLink::ClearScriptRouterLink();
		}

		SetState(ENTER_LANDING_PAGE);
	}
	else
#endif // GEN9_LANDING_PAGE_ENABLED
	{
		StartDisplayingLoadingScreens();

#if GEN9_LANDING_PAGE_ENABLED
		// RM(DND) gen9 will regularly shutdown the game session without shutting down the level for launching the landing page
		// ensure we are only loading level if not already loaded
		if (!CGame::IsLevelInitialized())
#endif // GEN9_LANDING_PAGE_ENABLED
		{
			// Load the required level
			//@@: range CGAMESTATEMACHINE_PROCESSLOADLEVELSTATE_INIT_LEVEL {
		
			CGame::InitLevel(m_Level);
		
			//@@: } CGAMESTATEMACHINE_PROCESSLOADLEVELSTATE_INIT_LEVEL
		}

		CGame::InitSession(m_Level, bLoadingSavegame);

		//@@: location CGAMESTATEMACHINE_PROCESSLOADLEVELSTATE_MATCHALLMODELSTRINGS
		CModelIndex::MatchAllModelStrings(); // Calling again as some new models could be loaded with DLCs from CGame::InitSession

		//CLoadingScreens::CommitToMpSp(); // No option to switch between mp/sp after this point. - Fix for 1765491 - BenR suggests we leave this up to script as its the 1st thing they call and always called 100%

		if (!CNetwork::GetGoStraightToMultiplayer())
		{
			// Single player game, stop the loading screens now.
#if GEN9_LANDING_PAGE_ENABLED
			// ensure script router navigation has completed
			if (!ScriptRouterLink::HasPendingScriptRouterLink())
#endif // GEN9_LANDING_PAGE_ENABLED
			{
				StopDisplayingLoadingScreens();
			}

		}

		SetState(IDLE);
	}
}

bool CGameSessionStateMachine::ProcessRestartFade()
{
    bool fadeFinished = false;

    if (CWarningScreen::IsActive())
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE, true))
			CWarningScreen::Remove();
	}
	else if(camInterface::GetFadeLevel() == 0.0f && camInterface::IsFading() == false)
	{
		camInterface::FadeOut(300);
	}
	else if(camInterface::GetFadeLevel() != 1.0)
	{

	}
// 	else if (CPauseMenu::IsActive() && !CWarningScreen::IsActive())
// 	{  
// 		// ?
// 	}
	else if (CPauseMenu::IsActive())
	{	//	GSW - To fix bug 176273, I had to leave one frame between closing the frontend and the LoadScene
		//	inside StartNewSinglePlayerSession. This gives the LoadingRenderFunction a chance to start
		CPauseMenu::Close();
	}
	else
	{
		CControlMgr::StopPlayerPadShaking(true);
		fadeFinished = true;
	}

    return fadeFinished;
}

void CGameSessionStateMachine::StartDisplayingLoadingScreens()
{
    CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_LOADLEVEL, 0);
	CLoadingScreens::SetAllowScaleformUpdateAtEOF(true);
}

void CGameSessionStateMachine::StopDisplayingLoadingScreens()
{
//	In Final builds, do nothing. Leave it to script to stop the loading screens
//	In non-Final builds, only stop the loading screens early if we're running with -stopLoadingScreensEarly
#if !__FINAL
	if (PARAM_stopLoadingScreensEarly.Get())
	{
		CLoadingScreens::Shutdown(SHUTDOWN_CORE);
	}
#endif	//	!__FINAL
	CLoadingScreens::SetAllowScaleformUpdateAtEOF(false);
}

void CGameSessionStateMachine::UpdateInitialDialogueScreensOnly()
{
	CSystem::BeginUpdate();

	// Update controls using async thread.
	CControlMgr::StartUpdateThread();
	CControlMgr::WaitForUpdateThread();

#if RSG_PC
	CMousePointer::Update();
	strStreamingEngine::GetLoader().LoadRequestedObjects();		// Pump the streaming system to issue new requests
	strStreamingEngine::GetLoader().ProcessAsyncFiles();
#endif

 	CWarningScreen::Update();
 	CProfileSettings::GetInstance().UpdateProfileSettings(false);

	if ( (CSavegameDialogScreens::IsMessageProcessing()) && (!CLoadingScreens::AreSuspended()) )
	{
		CLoadingScreens::Suspend();
	}

	if ( (!CSavegameDialogScreens::IsMessageProcessing()) && (CLoadingScreens::AreSuspended()) )
	{
		CLoadingScreens::Resume();
	}

	CNetwork::Update(true);

	CSystem::EndUpdate();

 	PF_FRAMEINIT_TIMEBARS(gDrawListMgr->GetUpdateThreadFrameNumber());
}

void CGameSessionStateMachine::ShutdownInitialDialogueScreensOnly()
{
	gVpMan.ShutdownSession();

	CWarningScreen::Update();  // one last update as we shutdown to switch off warningscreens that have been set

	camInterface::FadeOut(0);  // fully faded out
	CViewportPrimaryOrtho *pOrthoVP	= (CViewportPrimaryOrtho*)gVpMan.GetPrimaryOrthoViewport();
	if (pOrthoVP)
	{
		pOrthoVP->SetFadeFirstTime();  // set to fade 1st time again each time we init a session
	}

	CLoadingScreens::Resume();
}

#if RSG_PC
//	Copied from the while loop inside CGameSessionStateMachine::ProcessReInitState()
void CGameSessionStateMachine::UpdateLoadFailedDialogueScreens()
{
	PF_FRAMEINIT_TIMEBARS(0);
	CApp::CheckExit();

	// Pump necessary function to prevent hang
	sysIpcSleep(33);
	fwTimer::Update();
	CControlMgr::StartUpdateThread();
	CControlMgr::WaitForUpdateThread();
	CNetwork::Update(true);
//	CEntitlementManager::Update();		
	CWarningScreen::Update();
	CMousePointer::Update();

//	Copied from CGameSessionStateMachine::UpdateInitialDialogueScreensOnly()
	if ( (CSavegameDialogScreens::IsMessageProcessing()) && (!CLoadingScreens::AreSuspended()) )
	{
		CLoadingScreens::Suspend();
	}

	if ( (!CSavegameDialogScreens::IsMessageProcessing()) && (CLoadingScreens::AreSuspended()) )
	{
		CLoadingScreens::Resume();
	}
//	End of Copied from CGameSessionStateMachine::UpdateInitialDialogueScreensOnly()

//	if (g_GameRunning)
	{
		CScaleformMgr::UpdateAtEndOfFrame(false);
	}

	gRenderThreadInterface.FlushAndDeviceCheck();
	PF_FRAMEEND_TIMEBARS();
}
#endif	//	RSG_PC

void CGameSessionStateMachine::SetState(GameSessionState newState, bool WIN32PC_ONLY(bCanLeaveEntitlementState))
{
#if !__NO_OUTPUT
	static const char *s_StateNames[] =
	{
		"IDLE",
		"INIT",
#if RSG_PC
		"CHECK_FOR_RGSC",
		"HANDLE_RGSC_FAILED",
		"REQUIRE_PC_ENTITLEMENT",
		"REINIT",
		"TWO_FACTOR_AUTH_CHECK",
#if __STEAM_BUILD
		"STEAM_VERIFICATION",
#endif
#endif
		"START_AUTOLOAD",
		"CHECK_SIGN_IN_STATE",
		"LOAD_PROFILE",
		"CHOOSE_LANGUAGE",
		"CHECK_FOR_INVITE",
		"ENUMERATE_SAVED_GAMES",
		"AUTOLOAD_SAVE_GAME",
		"START_NEW_GAME",
		"START_LOADING_SAVE_GAME",
		"WAIT_FOR_SAVE_GAME",
		"HANDLE_LOADED_SAVE_GAME",
		"LOAD_LEVEL",
		"CHANGE_LEVEL",
#if UI_LANDING_PAGE_ENABLED
		"SHUTDOWN_FOR_LANDING_PAGE",
		"ENTER_LANDING_PAGE",
		"DISMISS_LANDING_PAGE",
#endif
		"GAMMA_CALIBRATION",
	};

	CompileTimeAssert(COUNTOF(s_StateNames) == MAX_STATE_CHANGES);

	coreDebugf1("SetState (%s) from (%s)", s_StateNames[(int)newState], s_StateNames[(int)m_State]);
#endif

#if RSG_PC
	// Trying to transition out of the entitlement states without permission
	if ((m_State == REQUIRE_PC_ENTITLEMENT || m_State == REINIT) && !bCanLeaveEntitlementState)
	{
		coreDebugf1("Trying to transition out of the entitlement states, ignoring state transition");
		return;
	}
#endif
	
	m_State = newState;
}

#if GEN9_LANDING_PAGE_ENABLED

bool CGameSessionStateMachine::UserAllowedToEnterStoryMode()
{
	return UserHasStoryModeEntitlement() || IgnoreStoryModeEntitlement();
}

bool CGameSessionStateMachine::IgnoreStoryModeEntitlement()
{
	// if we are trying to get into a non-story mode via a script router link
	// we sometimes need to pass through story mode (e.g. editor)
	if (ScriptRouterLink::HasPendingScriptRouterLink())
	{
		ScriptRouterContextData scrData;
		if (ScriptRouterLink::ParseScriptRouterLink(scrData))
		{
			if (scrData.m_ScriptRouterMode.Int != (int)SRCM_STORY)
			{
				Displayf("RM DEBUG: IgnoreStoryModeEntitlement returning true.");
				return true;
			}
		}
	}

	Displayf("RM DEBUG: IgnoreStoryModeEntitlement returning false.");
	return false;
}

bool CGameSessionStateMachine::UserHasStoryModeEntitlement()
{
	bool const c_hasStoryModeEntitlement = CLandingPageArbiter::IsStoryModeAvailable();
    return c_hasStoryModeEntitlement;
}

void CGameSessionStateMachine::ShowStoryModeUpsell()
{
	// TODO GEN9_STANDALONE: Introduce upsell state before proceeding to singleplayer or redirecting to landing page
	const GameSessionState targetState = ENTER_LANDING_PAGE;
	SetState(targetState);
}

#endif // GEN9_LANDING_PAGE_ENABLED
