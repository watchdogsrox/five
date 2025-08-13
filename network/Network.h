//
// name:        network.h
// description: High level interface to network stuff. All external access to the network code
//              must go through this class.
// written by:  John Gurney
//

#ifndef NETWORK_H
#define NETWORK_H

// rage includes
#include "net/connectionmanager.h"
#include "net/timesync.h"
#include "net/httprequesttracker.h"

// framework includes
#include "fwsys/timer.h"

// game includes
#include "Network/NetworkTypes.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Debug/NetworkDebug.h"

namespace rage
{
	class netBandwidthManager;
    class netConnectionManager;
    class netEvent;
    class netEventMgr;
    class netLoggingInterface;
    class netPlayer;
    class netSocket;
	class netSocketManager;
	class netTimeSync;
    class sysMemAllocator;
    class sysMemSimpleAllocator;
}

class CGameArrayMgr;
class CNetworkAssetVerifier;
class CNetworkBandwidthManager;
class CNetworkLeaderboardMgr;
class CNetworkObjectMgr;
class CNetworkParty;
class CNetworkPlayerMgr;
class CNetworkScratchpads;
class CNetworkSession;
class CNetworkTextChat;
class CNetworkVoice;
class CNetworkVoiceSession;
class CRoamingBubbleMgr;
class NetworkGameConfig;

class CNetworkTunablesListener : public TunablesListener
{
public:
#if !__NO_OUTPUT
	CNetworkTunablesListener() : TunablesListener("CNetworkTunablesListener") {}
#endif
	virtual void OnTunablesRead();
};

#if RSG_PC
// Used to control the Exit flow on pc
class NetworkExitFlow
{
public:
	
	enum ExitToDesktopState
	{
		ETD_NONE,
		ETD_WAITING_FOR_SAVE_READY,
		ETD_WAITING_FOR_SAVE_FINISHED,
		ETD_SAVE_FAILED,
		ETD_SAVE_SUCCESS,
		// PLEASE DO NOT INSERT STATES WILLY NILLY
		ETD_SHUTDOWN_START,
		ETD_FLUSH_TELEMETRY,
		ETD_UNADVERTISE_MM,
		ETD_UNADVERTISING,
		ETD_SESSION_SHUTDOWNS,
		ETD_SC_SIGNOUT,
		ETD_WAITING_FOR_SC_SIGNOUT,
		ETD_WAITING_FOR_UPNP_DELETE,
		ETD_WAITING_FOR_PCP_DELETE,
		ETD_WAITING_FOR_SCUI_SHUTDOWN,
		ETD_READY_TO_SHUTDOWN
	};

	NetworkExitFlow();

	//! starts the exit to desktop flow on pc
	ExitToDesktopState GetExitFlowState();
	void ResetExitFlowState();
	bool IsExitFlowRunning();

	void StartExitSaveFlow();
	bool IsSaveFlowRunning();
	bool IsSaveFlowFinished();

	void StartShutdownTasks();
	bool IsShutdownTasksRunning();
	bool IsReadyToShutdown();

	void SetShutdownState(ExitToDesktopState s);
	void Update();
	bool FromMP();
	const char* GetExitString();
private:
	ExitToDesktopState	m_ExitToDesktopState;
	netStatus m_MyStatus;
	netStatus m_UpNpStatus;
	netStatus m_PcpStatus;
	bool m_fromMP;
};
#endif

struct sBailParameters
{
	sBailParameters()
    {
        m_BailReason = eBailReason::BAIL_INVALID;
        m_ErrorCode = static_cast<int>(eBailContext::BAIL_CTX_NONE);
        m_ContextParam1 = -1;
        m_ContextParam2 = -1;
        m_ContextParam3 = -1;
        m_ErrorString = nullptr;
        m_PassThroughString = nullptr; 
        m_nNumbers = 0;
		m_Flags = BailFlag_None;
		m_EndPosixTime = 0;
    }

	sBailParameters(const eBailReason bailReason, const int errorCode, const int contextParam1 = -1, const int contextParam2 = -1, const int contextParam3 = -1)
		: m_BailReason(bailReason)
		, m_ErrorCode(errorCode)
		, m_ContextParam1(contextParam1)
		, m_ContextParam2(contextParam2)
		, m_ContextParam3(contextParam3)
	{
		m_ErrorString = nullptr;
		m_PassThroughString = nullptr;
		m_nNumbers = 0;
		m_Flags = BailFlag_None;
		m_EndPosixTime = 0;
	}

	eBailReason m_BailReason;
    int m_ErrorCode;
    int m_ContextParam1;
    int m_ContextParam2;
    int m_ContextParam3;
    u64 m_EndPosixTime;
    const char* m_ErrorString;
    const char* m_PassThroughString;
    static const unsigned MAX_NUMBERS = 3;
    int m_Numbers[MAX_NUMBERS];
    unsigned m_nNumbers;
	unsigned m_Flags;

    void AddNumber(const int number)
    {
		if(m_nNumbers < MAX_NUMBERS)
		{
			m_Numbers[m_nNumbers] = number;
			m_nNumbers++;
		}
    }
};

struct sNetworkErrorDetails
{
	const char*             m_ErrorMsg;
	int                     m_ErrorCode;
	u64						m_EndPosixTime;
	AlertScreenSource       m_AlertSource;
	u32                     m_AlertContext;
	bool                    m_HasErrorCode;
	
	static const unsigned	MAX_SUBSTRING_LENGTH = 64;
	char					m_SubString[MAX_SUBSTRING_LENGTH];
	bool					m_HasSubString;

#if RSG_OUTPUT
	bool                    m_DeveloperMode;
#endif

	sNetworkErrorDetails()
	{
		Reset();
	}

	void Reset()
	{
		m_ErrorMsg = nullptr;
		m_ErrorCode = 0;
		m_EndPosixTime = 0;
		m_HasErrorCode = false;
		m_HasSubString = false;
		m_SubString[0] = '\0';
#if RSG_OUTPUT
		m_DeveloperMode = false;
#endif
	}

	unsigned GetErrorSupportHash() const { return (m_HasErrorCode && (m_ErrorCode > 0)) ? m_ErrorCode : atStringHash(m_ErrorMsg); }
	bool HasError() const { return m_ErrorMsg != nullptr || (m_HasErrorCode && m_ErrorCode != 0); }
	bool HasEndTime() const { return m_EndPosixTime > 0; }
};

class CNetwork
{
public:

    static int DEFAULT_CONNECTION_TIMEOUT;
	static int DEFAULT_CONNECTION_KEEP_ALIVE_INTERVAL;

//  When the player wants to start a new network session, the game does a full ShutdownSession and
//  InitSession. This clears all the scripts including the global variables. A few of these variables
//  (mostly concerning the appearance of the cellphone) need to be carried across to the new session.
//  This structure stores these values so that they can be retrieved by the scripts when the new session begins.
//  For this to work, the structure must not be cleared during the ShutdownSession and InitSession.
    struct ScriptValues
    {
        #define MAX_NUMBER_OF_SCRIPT_VALUES_TO_STORE_FOR_NETWORK_GAME (10)

        void Clear();

        int values[MAX_NUMBER_OF_SCRIPT_VALUES_TO_STORE_FOR_NETWORK_GAME];
    };

	static netSocketManager* GetSocketMgr();

    static unsigned short GetSocketPort();

    static unsigned GetVersion();

    //PURPOSE
    //  Returns true if we're permitting only LAN sessions.
    static bool LanSessionsOnly();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void InitTunables();
	static void ShutdownTunables();
	static void ShutdownConnectionManager();

	static void OnTunablesRead();

    static void Update() { Update(false); } // required to interface with game skeleton code
	static void Update(bool fromLoadingCode);

    // bails out of the network game.
	static bool Bail(const sBailParameters bailParams, bool bSendScriptEvent = true);
	static void BailImmediate(const sBailParameters& bailParams);

	// Generates a date string which can be used in a text such as "You have been banned until August 22 2018"
	// Would fit better in fwLanguagePack but that's in rage so it hasn't got access to CVarString
	static void FormatPosixTimeAsUntilDate(const s64 posixTime, char* dateString, const unsigned maxLength);

	// network alert screen maintenance
	static void ActionBail();
	static void SetupNetworkAlertScreen();
    static void UpdateNetworkAlertScreen();
    static void ClearNetworkAlertScreen();
    static void CleanupNetworkAlertScreen();

	// clear alert screens when a platform invite is progressed
	static void OnPlatformInvitePassedChecks();

	static bool IsNetworkOpening()	{ return sm_bNetworkOpening; }
	// returns true if we have started the network, but may not be necessarily connected to any other machines
	static bool IsNetworkOpen()	{ return sm_bNetworkOpen; }

	// returns true if we have started closing the network and are currently processing the network close.
	static bool IsNetworkClosing() { return sm_bNetworkClosing; }

	static bool IsNetworkClosePending() { return sm_bNetworkClosePending; }

    // returns true if the match has been started
    static bool HasMatchStarted() { return sm_bMatchStarted; }

    // returns true if a network game is running
	static bool IsGameInProgress() { return sm_bGameInProgress; }

    // returns whether there is collision loaded around the local player
    static bool IsCollisionLoadedAroundLocalPlayer() { return sm_bCollisionAroundLocalPlayer; }

    static bool AreCarGeneratorsDisabled() { return sm_bDisableCarGeneratorTime > 0; }

    static netConnectionManager& GetConnectionManager();

	static CNetworkLeaderboardMgr&   GetLeaderboardMgr()	 { return sm_LeaderboardMgr; }
    static CNetworkPlayerMgr&        GetPlayerMgr()          { return sm_PlayerMgr; }
    static netTimeSync&              GetNetworkClock()       { return sm_networkClock; }
    static CNetworkObjectMgr&        GetObjectManager()      { return *sm_ObjectMgr; }
    static netEventMgr&              GetEventManager()       { return *sm_EventMgr; }
    static CGameArrayMgr&			 GetArrayManager()       { return *sm_ArrayMgr; }
    static CNetworkBandwidthManager& GetBandwidthManager()   { return sm_BandwidthMgr; }
    static CRoamingBubbleMgr&        GetRoamingBubbleMgr()   { return sm_RoamingBubbleMgr; }
    static CNetworkAssetVerifier&    GetAssetVerifier()		 { return sm_AssetVerifier; }
	static CNetworkScratchpads&		 GetScriptScratchpads()	 { return sm_ScriptScratchpads; }
	static CNetworkSession&			 GetNetworkSession()	 { return *sm_NetworkSession; }
	static bool                      IsNetworkSessionValid() { return (sm_NetworkSession != nullptr); }
	static CNetworkParty&			 GetNetworkParty()		 { return *sm_NetworkParty; }
	static bool                      IsNetworkPartyValid()   { return (sm_NetworkParty != nullptr); }
	static CNetworkVoiceSession&	 GetNetworkVoiceSession(){ return sm_NetworkVoiceSession; }
	static CNetworkVoice&			 GetVoice()				 { return sm_NetworkVoice; }

	#if __WIN32PC
	static CNetworkTextChat&		 GetTextChat()			 { return sm_NetworkTextChat; }
	static NetworkExitFlow&			 GetNetworkExitFlow()	 { return sm_ExitToDesktopFlow;	}
	#endif // __WIN32PC

	//#if __BANK

	static sysMemAllocator& GetSCxnMgrAllocator();

//#endif // __BANK

    static void SetInMPTutorial(bool inTutorial)			{ sm_bInMPTutorial = inTutorial; }
    static bool IsInMPTutorial()							{ return sm_bInMPTutorial; }
	static void SetInMPCutscene(bool inCutscene)			{ sm_bInMPCutscene = inCutscene; if (!inCutscene) sm_bNetworkCutsceneEntities = false; }
	static bool IsInMPCutscene()							{ return sm_bInMPCutscene; }
	static void SetNetworkCutsceneEntities(bool bNetwork)	{ sm_bNetworkCutsceneEntities = bNetwork; }
	static bool AreCutsceneEntitiesNetworked()				{ return sm_bNetworkCutsceneEntities; }

    static void SetFriendlyFireAllowed(bool allowed)		{ sm_bFriendlyFireAllowed = allowed; }
    static bool IsFriendlyFireAllowed()						{ return sm_bFriendlyFireAllowed; }

	static void SetScriptReadyForEvents(bool bReady);
	static bool IsScriptReadyForEvents()					{ return sm_bScriptReadyForEvents; }

    static void SetScriptAutoMuted(bool bIsAutoMuted);
    static bool IsScriptAutoMuted()							{ return sm_bIsScriptAutoMuted; }

    // returns true if we have privileges to play online.
    static bool HaveOnlinePrivileges();
	static bool HaveUserContentPrivileges(const int nPrivilegeType);
	static bool HaveCommunicationPrivileges(const int nPrivilegeType);
	
	// returns true if the local player is flagged as a cheater
	static bool IsCheater(const int nLocalPlayerIndex = -1);

    // informs the rest of the network managers that a player has joined/left the game
    static void PlayerHasJoinedSession(const netPlayer& pPlayer);
    static void PlayerHasLeftSession(const netPlayer& pPlayer);

	// informs the rest of the network managers that a player has joined/left the local players roaming bubble
	static void PlayerHasJoinedBubble(const netPlayer& pPlayer);
	static void PlayerHasLeftBubble(const netPlayer& pPlayer);

	static bool IsConnectionValid(const int connectionId);

    static netLoggingInterface &GetMessageLog();
    static void                 FlushAllLogFiles(bool waitForFlush);

#if ENABLE_NETWORK_LOGGING
    static void OnAssertionFired();
#endif // ENABLE_NETWORK_LOGGING

    // network heap code.
    static bool CreateNetworkHeap();
    static void DeleteNetworkHeap();
    static void UseNetworkHeap(NetworkMemoryCategory category);
    static void StopUsingNetworkHeap();
    static void AcquireNetworkHeapMemory();
    static void RelinquishNetworkHeapMemory();

    static sysMemAllocator* GetNetworkHeap();

    static unsigned GetNetworkTime();
	static unsigned GetNetworkTimeLocked();
    static unsigned GetNetworkTimePausable();
	static unsigned GetTimeoutTime();

#if !__NO_OUTPUT
	static void DumpNetworkTimeInfo();
#endif

    static void StopNetworkTimePausable();
    static void RestartNetworkTimePausable();

#if !__FINAL
	static bool IsTimeoutModifiedByCommandLine();
#endif

    // This is a millisecond timer that is synced between machines in a network game, but is still valid
    // to use in a single player game. Internally this is the game timer in single player games, and the network
    // timer in network games. If you use this function you must handle this function returning timer value less
    // than returned in a previous frame - this can happen if the clock is corrected
	static void SetSyncedTimeInMilliseconds(unsigned time);
	static unsigned GetSyncedTimeInMilliseconds()	{ return sm_syncedTimeInMilliseconds; }

    static void RenderNetworkInfo();

	static void SetGoStraightToMultiplayer( bool GoStraightToMultiplayer );
	static bool GetGoStraightToMultiplayer();

    static void SetGoStraightToMPEvent( bool goStraightToMpEvent );
    static bool GetGoStraightToMPEvent( );

	static void SetGoStraightToMPRandomJob( bool goStraightToMpRandomJob );
	static bool GetGoStraightToMPRandomJob( );

	static void SetShutdownSessionClearsTheGoStraightToMultiplayerFlag(bool bShutdownSessionClearsTheFlag);

	static void ResetConnectionPolicies(const unsigned timeout, const unsigned keepAliveInterval = 0);

#if __BANK
	static void InitWidgets();
	static void InitLevelWidgets();
	static void ShutdownLevelWidgets();
#endif

    static bool OpenNetwork();
	static void EnterLobby();
	static void StartMatch();
    static void CloseNetwork(bool bFullClose);

    static bool HasCalledFirstEntryOpen() { return sm_bFirstEntryNetworkOpen; }
    static bool HasCalledFirstEntryStart() { return sm_bFirstEntryMatchStarted; }

	static void InitVoice(); 
    static void InitManagers();
    static bool ManagersAreInitialised() { return sm_bManagersAreInitialised; }
	static void ShutdownVoice(bool bForce = false);
	static void ShutdownManagers();

    static void ClearPopulation(bool bIncludeMissionEntities);

    static void OverrideClockTime(int hour, int minute, int second, bool bFromScript);
	static void OverrideClockRate(u32 msPerGameMinute);
    static void ClearClockTimeOverride();
    static bool IsClockTimeOverridden();
	static bool IsClockOverrideFromScript();
	static bool IsClockOverrideLocked();
	static void SetClockOverrideLocked(bool bLocked);
	static void HandleClockUpdateDuringOverride(int hour, int minute, int second, int lastMinAdded);
    static void GetClockTimeStoredForOverride(int &hour, int &minute, int &second, int &lastMinAdded);

    static bool IsInTutorialSession() { return sm_IsInTutorialSession; }
    static bool IsNetworkTutorialSessionTurn();
	static bool IsConcealedPlayerTurn();

	static void SetNetworkGameMode(u16 nGameMode);
	static u16 GetNetworkGameMode() { return sm_nGameMode; }
	static void SetNetworkGameModeState(int nGameModeState);
	static int GetNetworkGameModeState() { return sm_nGameModeState; }

	static void SkipRadioResetNextClose() { sm_bSkipRadioResetNextClose = true; }
	static void SkipRadioResetNextOpen() { sm_bSkipRadioResetNextOpen = true; }

	//Calculate average RTT over time for the multiplayer game
	static u32 GetClientAverageRTT();

	// clock / weather management
	static void ApplyClockAndWeather(bool bDoTransition, unsigned nTransitionTime); 
	static void UpdateClockAndWeather(); 
    static void StartClockTransitionToGlobal(unsigned nTransitionTime = 0);
	static void StartWeatherTransitionToGlobal(unsigned nTransitionTime = 0);
	static bool IsApplyingMultiplayerGlobalClockAndWeather() { return sm_bAppliedGlobalSettings; }
	static void StopApplyingGlobalClockAndWeather();

	// global multiplayer settings for time and weather
    static float GetGlobalDateRatio();
    static void GetGlobalDate(int& nGameDay);
    static float GetGlobalClockRatio();
	static void GetGlobalClock(int& nHour, int& nMinute, int& nSecond);
	static float GetGlobalWeatherRatio();
	static void GetGlobalWeather(s32& nCycleIndex, u32& nCycleTimer);

#if __BANK
	static bool IsSaveMigrationEnabled() { return sm_bSaveMigrationEnabled; }
#endif // __BANK
	
#if RSG_ORBIS
	static void InitialiseLiveStreaming();
	static void ShutdownLiveStreaming();
	static void EnableLiveStreaming(bool bEnable);
	static bool IsLiveStreamingEnabled();
	static bool IsPlayingInBackCompatOnPS5();
#endif

	// transition time / load tracking
	static void StartTransitionTracker(int nTransitionType, int nContextParam1, int nContextParam2, int nContextParam3);
	static void AddTransitionTrackerStage(int nStageSlot, int nStageHash, int nContextParam1, int nContextParam2, int nContextParam3);
	static void FinishTransitionTracker(int nContextParam1, int nContextParam2, int nContextParam3); 
	static void ClearTransitionTracker();

    // network access checks, if IsReadyToCheckNetworkAccess returns false the result might be incorrect
    static bool IsReadyToCheckNetworkAccess(const eNetworkAccessArea accessArea, IsReadyToCheckAccessResult* pResult = nullptr);
	static bool CanAccessNetworkFeatures(const eNetworkAccessArea nAccessArea, eNetworkAccessCode* nAccessCode = nullptr);
	static eNetworkAccessCode CheckNetworkAccess(const eNetworkAccessArea nAccessArea, u64* endPosixTime = nullptr);
	static eNetworkAccessCode CheckNetworkAccessAndPopulateErrorDetails(const eNetworkAccessArea nAccessArea, sNetworkErrorDetails& errorDetails);
	static bool ShowSystemUIForAccessCode(eNetworkAccessCode const nAccessCode, bool& bNeedInGameUi_Out);
	static bool CheckNetworkAccessAndAlertIfFail(const eNetworkAccessArea nAccessArea);
	static bool IsAccessCodeApplicableToArea(const eNetworkAccessCode nAccessCode, const eNetworkAccessArea nAccessArea);

	static const char* GetNetworkAccessErrorString(const eNetworkAccessArea nAccessArea, const eNetworkAccessCode nAccessCode);
	static bool GetNetworkAccessErrorCode(const eNetworkAccessCode nAccessCode, int& nErrorCode);
	static bool GetBailExpectsErrorCode(const eBailReason nReason);

	static void ShowBailWarningScreen(); 

#if !__FINAL
	static bool BypassMultiplayerAccessChecks();
#endif

	static bool IsMultiplayerLocked();
	static void SetMultiplayerLocked(const bool multiplayerLocked);
	static bool IsMultiplayerDisabled(); 

	static void SetSpPrologueRequired(const bool bIsRequired);
	static bool IsSpPrologueRequired();
	static IntroSettingResult HasCompletedSpPrologue();
	static bool SetCompletedSpPrologue(const bool bHasCompleted);
	static bool CheckSpPrologueRequirement();
		
	static bool IsLastDamageEventPendingOverrideEnabled();

private:

    static void UpdateSyncedTimeInMilliseconds();

	static void UpdateConcealedPlayerCachedVariables();

    static void UpdateTutorialSessionCachedVariables();

	static void UpdateEventTriggerCounters();

	static void UpdateScuiHotkeys();

    static void OnNetEvent(netConnectionManager* cxnMgr,
                            const netEvent* evt);

#if !__NO_OUTPUT
	static void OnRequestTrackerEvent(const char* sRuleName, const char* sRuleMatch, const unsigned nTokenLimit, const float nTokenRate);
#endif

#if __WIN32PC
	static NetworkExitFlow sm_ExitToDesktopFlow;
#endif // __WIN32PC

public:
	// size of heap for network
	static const int SCXNMGR_HEAP_SIZE;

	static bool sm_playerIsVisible;

#if !__NO_OUTPUT
	static void LogSummary();
#endif

private:

	static bool                             sm_bMultiplayerLocked;

	static bool								sm_bSpPrologueRequired;

	static bool                             sm_bNetworkOpen;
	static bool                             sm_bNetworkOpening;
	static bool								sm_bNetworkClosing;
	static bool								sm_bNetworkClosePending;
	static bool								sm_bNetworkClosePendingIsFull;
	static bool								sm_bFirstEntryNetworkOpen; 
	static bool								sm_bAllowPendingCloseNetwork;
	static bool								sm_bFatalErrorForDoubleOpen;
	static bool								sm_bFatalErrorForCloseDuringOpen;
	static bool								sm_bFatalErrorForStartWhenClosed;
	static bool								sm_bFatalErrorForStartWhenFullyClosed;
	static bool								sm_bFatalErrorForStartWhenManagersShutdown;
	static bool								sm_bFatalErrorForInvalidPlayerManager;

	static unsigned							sm_nMinStallMsgTotalThreshold;
	static unsigned							sm_nMinStallMsgNetworkThreshold;

	static bool								sm_IsBailPending; 
	static bool								sm_AlertScriptForBail;
	static bool								sm_PendingBailWarningScreen;
	static sBailParameters					sm_BailParams;

	static bool								sm_bScriptReadyForEvents;
    static bool                             sm_bIsScriptAutoMuted;
	static bool                             sm_bInMPTutorial;
	static bool                             sm_bInMPCutscene;
	static bool                             sm_bNetworkCutsceneEntities;
	static bool                             sm_bFriendlyFireAllowed;
	static bool                             sm_bManagersAreInitialised;
	static bool                             sm_bMatchStarted;
    static bool                             sm_bFirstEntryMatchStarted;
	static bool                             sm_bGameInProgress;
	static bool                             sm_bGoStraightToMultiplayer;
    static bool                             sm_bGoStraightToMpEvent;
	static bool                             sm_bGoStraightToMPRandomJob;
	static bool								sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag;
    static bool                             sm_bCollisionAroundLocalPlayer;
	static bool								sm_bSkipRadioResetNextClose;
	static bool								sm_bSkipRadioResetNextOpen;
	static bool								sm_bLiveStreamingInitialised;
    static unsigned                         sm_bDisableCarGeneratorTime;
	static u16							    sm_nGameMode;
	static int                              sm_nGameModeState;

#if __BANK
	static bool                             sm_bSaveMigrationEnabled;
#endif // __BANK

#if !__NO_OUTPUT
	static netHttpRequestTracker::Delegate  sm_RequestTrackerDlgt;
#endif

    static netTimeSync&                     sm_networkClock;
    static CNetworkObjectMgr*               sm_ObjectMgr;
    static netEventMgr*                     sm_EventMgr;
    static CGameArrayMgr*					sm_ArrayMgr;
    static CNetworkBandwidthManager&        sm_BandwidthMgr;
    static netConnectionManager             sm_CxnMgr;
    static netConnectionManager::Delegate   sm_CxnMgrGameDlgts[NET_MAX_CHANNELS];
    static CNetworkPlayerMgr&               sm_PlayerMgr;
	static CNetworkLeaderboardMgr&			sm_LeaderboardMgr;
    static CNetworkSession*                 sm_NetworkSession;
	static CNetworkParty*					sm_NetworkParty;
	static CNetworkVoiceSession&            sm_NetworkVoiceSession;
	static CNetworkScratchpads&				sm_ScriptScratchpads;
    static CRoamingBubbleMgr&               sm_RoamingBubbleMgr;
	static CNetworkVoice&					sm_NetworkVoice;
	static CNetworkTextChat&				sm_NetworkTextChat;
    static char                             sm_logFileBuffer[256];
    static unsigned                         sm_syncedTimeInMilliseconds;
	static unsigned                         sm_lockedNetworkTime;
	static unsigned							sm_NetworkTimeWhenDebugPaused;
	static unsigned							sm_EventTriggerLastUpdate;
	static bool                             sm_IsInTutorialSession;
	static bool								sm_ThereAreConcealedPlayers;

    // network heap
    static class sysMemAllocator*           sm_pNetworkHeap;
    static void*                            sm_pHeapBuffer;
    static s32                              sm_HeapBufferRefCount;
    static CNetworkAssetVerifier&           sm_AssetVerifier;

	//Calculate average RTT over time for the multiplayer game.
	// Issues:
	//  - Only works for clients since it uses network clock RTT - its only calculated for the clients.
	//  - If the host is slow will flag all clients as being slow.
	static u32                              sm_averageRTT;
	static u32                              sm_averageRTTWorst;
	static u32                              sm_averageRTTSamples;
	static u32                              sm_averageRTTInterval;

	// stashed clock times for clients
	static int m_nClockHour;
	static int m_nClockMinute;
	static int m_nClockSecond;
	static unsigned m_nClockTransitionTimeLeft;
	static unsigned m_nClockAndWeatherTransitionTime;
	static int m_LastClockUpdateTime;
	static int m_NextClockSyncTime;
    static bool sm_bAppliedGlobalSettings;

	static bool sm_lastDamageEventPendingOverrideEnabled;

	static CNetworkTunablesListener* m_pTunablesListener;
};

inline unsigned
CNetwork::GetNetworkTime()
{
	if (fwTimer::IsUserPaused())
	{
		return sm_NetworkTimeWhenDebugPaused;
	}

	if (sm_networkClock.HasStarted())
	{
		return sm_networkClock.GetTime();
	}

	return 0;
}

// Network Allocator
extern void sysMemStartNetwork();
extern void sysMemEndNetwork();
struct sysMemAutoUseNetworkMemory
{
	sysMemAutoUseNetworkMemory() {sysMemStartNetwork();}
	~sysMemAutoUseNetworkMemory() {sysMemEndNetwork();}
};

#endif  //NETWORK_H
