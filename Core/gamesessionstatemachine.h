//
// name:		gamesessionstatemachine.h
// description:	state machine for managing transitions between different session types
#ifndef INC_GAMESESSIONSTATEMACHINE_H_
#define INC_GAMESESSIONSTATEMACHINE_H_

#include "frontend/landing_page/LandingPageConfig.h"

#if UI_LANDING_PAGE_ENABLED
enum ReturnToLandingPageReason
{
	Reason_Default = 0,
	Reason_SignInStateChange,
};
#endif

class CGameSessionStateMachine
{
public:

    // Game session states
    enum GameSessionState
    {
        IDLE,
		INIT,
#if __WIN32PC
		CHECK_FOR_RGSC,
		HANDLE_RGSC_FAILED,
		REQUIRE_PC_ENTITLEMENT,
		REINIT,
		TWO_FACTOR_AUTH_CHECK,
#if __STEAM_BUILD
		STEAM_VERIFICATION,
#endif
#endif
        START_AUTOLOAD,
		CHECK_SIGN_IN_STATE,
		LOAD_PROFILE,
		CHOOSE_LANGUAGE,
        CHECK_FOR_INVITE,
		ENUMERATE_SAVED_GAMES,
        AUTOLOAD_SAVE_GAME,
        START_NEW_GAME,
        START_LOADING_SAVE_GAME,
        WAIT_FOR_SAVE_GAME,
        HANDLE_LOADED_SAVE_GAME,
		LOAD_LEVEL,
        CHANGE_LEVEL,
#if UI_LANDING_PAGE_ENABLED
		SHUTDOWN_FOR_LANDING_PAGE,
		ENTER_LANDING_PAGE,
		DISMISS_LANDING_PAGE,
#endif // UI_LANDING_PAGE_ENABLED
		GAMMA_CALIBRATION,
		MAX_STATE_CHANGES
    };

    // The default level to use for the game
    static const unsigned DEFAULT_START_LEVEL = 1;

public:

    CGameSessionStateMachine();
    ~CGameSessionStateMachine();

    //PURPOSE
    // Initialises the class member variables to their default values
    static void Init();
	static void ReInit(int level);
#if RSG_PC
	static bool IsCheckingEntitlement() { return m_State == REINIT || m_State == REQUIRE_PC_ENTITLEMENT || m_State == HANDLE_RGSC_FAILED STEAMBUILD_ONLY(|| m_State == STEAM_VERIFICATION); }
	static bool IsEntitlementUpdating() { return m_bEntitlementUpdating; }
#endif

#if UI_LANDING_PAGE_ENABLED
	static void ReturnToLandingPage(const ReturnToLandingPageReason reason = ReturnToLandingPageReason::Reason_Default);
#endif

    //PURPOSE
    // Returns whether the session state machine is currently idle (not managing transitions between sessions)
    static bool IsIdle() { return m_State == IDLE; }

    //PURPOSE
    // Starts a new game starting on the specified level
    //PARAMS
    // level - The level to start the new game on
    static void StartNewGame(int level);

    //PURPOSE
    // Loads a save game (currently via the CGenericGameStorage class, which determines which savegame to load)
    static void LoadSaveGame();

	//PURPOSE
	// Expects a savegame to have been loaded already. Shuts down the current session and starts a new session using the loaded save data
	static void HandleLoadedSaveGame();

	//PURPOSE
	// 
	static void SetStateToIdle();

	//PURPOSE
	// Handle any sign off line situation
	static void OnSignedOffline();

	//PURPOSE
	// Handle any sign out situation
	static void OnSignOut();

    //PURPOSE
    // Changes to the specified game level
    //PARAMS
    // level - The level to change to
    static void ChangeLevel(int level);

    //PURPOSE
    // Updates the state machine, should be called once per frame
    static void Update();

	//PURPOSE
	// Returns The current state machine state.
	static GameSessionState GetGameSessionState() {return m_State;}

	static void SetForceInitLevel() { m_ForceInitLevel = true; }

	static void SetState(GameSessionState newState, bool bCanLeaveEntitlementState = false);

private:

	//PURPOSE
	// Processes the state machines Idle state
	static void ProcessIdleState();

	//PURPOSE
	// Processes the state machines Init state
	static void ProcessInitState();

#if __WIN32PC
	//PURPOSE
	// Processes the state machines CheckForRGSC state
	static void ProcessCheckForRGSC();

	//PURPOSE
	// Process the state machines HandleRGSCFailed state
	static void ProcessRGSCFailed();

#if __STEAM_BUILD
	//PURPOSE
	// Process whether the user owns the Steam content or not
	static void ProcessSteamVerification();
#endif


	//PURPOSE
	// Processes the state machine's RequirePCEntitlement state
	static void ProcessRequirePCEntitlement();

	// PURPOSE
	// Processes the re-initialization logic REINIT State
	static void ProcessReInitState();
#endif

    //PURPOSE
    // Processes the state machines StartAutoLoad state
    static void ProcessStartAutoloadState();

	//PURPOSE
	// Processes the state machines GammaCalibration state
	static void ProcessGammaCalibration();

#if UI_LANDING_PAGE_ENABLED
	static void ProcessShutdownForLandingPage();
	
	//PURPOSE
	// Processes the landing page on platforms that have one
    static void ProcessEnterLandingPage();

	static void ProcessDismissLandingPage();
#endif // UI_LANDING_PAGE_ENABLED

	//PURPOSE
	// Processes the state machines CheckSignIn state
	static void ProcessCheckSignInState();

	//PURPOSE
	// Processes the state machines CheckSignIn state
	static void LoadProfileState();

	//PURPOSE
	// Processes the state machines CheckSignIn state
	static void ChooseLanguageState();

    //PURPOSE
    // Processes the state machines CheckForInvite state
    static void ProcessCheckForInviteState();

    //PURPOSE
    // Processes the state machines EnumerateSavedGames state
    static void ProcessEnumerateSavedGamesState();

    //PURPOSE
    // Processes the state machines AutoloadSaveGame state
    static void ProcessAutoLoadSaveGameState();

    //PURPOSE
    // Processes the state machines StartNewGame state
    static void ProcessStartNewGameState();

    //PURPOSE
    // Processes the state machines LoadingSaveGame state
    static void ProcessStartLoadingSaveGameState();

    //PURPOSE
    // Processes the state machines WaitForSaveGame state
    static void ProcessWaitForSaveGameState();

    //PURPOSE
    // Processes the state machines HandleLoadedSaveGame state
    static void ProcessHandleLoadedSaveGameState();

	//PURPOSE
	// Processes the state machines ChangeLevel state
	static void ProcessLoadLevelState(bool bLoadingSavegame = false);

	//PURPOSE
    // Processes the state machines ChangeLevel state
    static void ProcessChangeLevelState();

    //PURPOSE
    // Manages the fading in/out for the game prior to starting a new game/changing level
    static bool ProcessRestartFade();

	//PURPOSE
	// Processes the state machines YubiKeyCheck state
	static void ProcessTwoFactorAuthCheck();

    //PURPOSE
    // Displays the loading screens
    static void StartDisplayingLoadingScreens();

    //PURPOSE
    // Stops Displaying the loading screens
    static void StopDisplayingLoadingScreens();

    //PURPOSE
    // Updates the frontend systems that need updating while transitioning betweeen sessions synchronously
    static void UpdateInitialDialogueScreensOnly();

    //PURPOSE
    // Shuts down the frontend systems that are updated while transitioning betweeen sessions synchronously
	static void ShutdownInitialDialogueScreensOnly();

#if RSG_PC
	static void UpdateLoadFailedDialogueScreens();
#endif	//	RSG_PC

#if __BANK
	static void AskBeforeRestart();
#endif	//	__BANK

#if GEN9_LANDING_PAGE_ENABLED
	static bool UserAllowedToEnterStoryMode();
	static bool IgnoreStoryModeEntitlement();
	static bool UserHasStoryModeEntitlement();
	static void ShowStoryModeUpsell();
#endif // GEN9_LANDING_PAGE_ENABLED

    static GameSessionState m_State;                 // The current state machine state
	static int				m_ReInitLevel;			 // the level to use when reiniting
    static int              m_Level;                 // The level to use in the current session transition
	static bool             m_ForceInitLevel;    
	static bool             m_StartedAutoload;       // Indicates whether the state machine is performing an autoload
#if __WIN32PC
	static bool				m_RGSCFailedInit;		 // Indicates whether the RGSC failed to initialize (PC)
	static bool				m_bSupportURLDisplayed;  // Indicates whether the support URL for failed RSGC initialization has been shown
	static bool				m_bEntitlementUpdating;	 // Indicates whether the entitlement system is spin-updating.
#endif
};

#endif // INC_GAMESESSIONSTATEMACHINE_H_
