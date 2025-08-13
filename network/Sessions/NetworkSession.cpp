//
// NetworkSession.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//
#include "NetworkSession.h"

//rage includes
#include "diag/seh.h"
#include "system/nelem.h"	//for COUNTOF
#include "system/simpleallocator.h"
#include "net/message.h"
#include "net/task.h"
#include "net/timesync.h"
#include "net/nethardware.h"
#include "rline/rltask.h"
#include "rline/rlsessionmanager.h"

#if RSG_XENON
#include "rline/rlxbl.h"
#elif RSG_NP
#include "rline/rlnp.h"
#endif

#if RSG_DURANGO
#include "rline/durango/rlxbl_interface.h"
#include "rline/durango/rlXblPrivacySettings_interface.h"
#endif

// framework includes
#include "fwnet/NetLogUtils.h"
#include "fwnet/netchannel.h"
#include "fwnet/netutils.h"
#include "fwnet/netplayer.h"
#include "fwnet/netremotelog.h"

// network includes
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Commerce/CommerceManager.h"
#include "Network/Debug/NetworkBot.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkSCPresenceUtil.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Network/roaming/RoamingBubbleMgr.h"
#include "Network/roaming/RoamingMessages.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSessionMessages.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/xlast/Fuzzy.schema.h"
#include "network/Cloud/Tunables.h"

// game includes
#include "Event/EventGroup.h"
#include "Event/EventNetwork.h"
#include "frontend/CFriendsMenu.h"
#include "frontend/ContextMenu.h"
#include "Frontend/loadingscreens.h"
#include "frontend/ProfileSettings.h"
#include "frontend/WarningScreen.h"
#include "Frontend/ReportMenu.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PlayerInfo.h"
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "Control/gamelogic.h"
#include "SaveLoad/savegame_queue.h"
#include "script/commands_network.h"
#include "script/streamedscripts.h"
#include "scene/ExtraContent.h"
#include "scene/world/GameWorld.h"
#include "security/ragesecgameinterface.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "stats/StatsInterface.h"
#include "stats/StatsTypes.h"

#if __DEV
#include "text/messages.h"
#include "text/TextConversion.h"
#endif // __DEV

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

RAGE_DEFINE_SUBCHANNEL(net, mminfo, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_mminfo
#define netMmInfoDebug(fmt, ...)		RAGE_DEBUGF1(net_mminfo, fmt, ##__VA_ARGS__)

RAGE_DEFINE_SUBCHANNEL(net, game, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_game

// these prevent spam in the net_game channel
RAGE_DEFINE_CHANNEL(net_transition_msg,			DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netTransitionMsgDebug(fmt, ...)			RAGE_DEBUGF1(net_transition_msg, fmt, ##__VA_ARGS__)
#define netTransitionMsgError(fmt, ...)			RAGE_ERRORF(net_transition_msg, fmt, ##__VA_ARGS__)
#define netTransitionMsgVerify(cond)			RAGE_VERIFY(net_transition_msg,cond)
#define netTransitionMsgAssertf(cond, fmt, ...)	RAGE_ASSERTF(net_transition_msg,cond, fmt, ##__VA_ARGS__)

RAGE_DEFINE_CHANNEL(net_playercard,				DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netPlayerCardDebug(fmt, ...)			RAGE_DEBUGF1(net_playercard, fmt, ##__VA_ARGS__)
#define netPlayerCardWarning(fmt, ...)			RAGE_WARNINGF(net_playercard, fmt, ##__VA_ARGS__)
#define netPlayerCardAssertf(cond, fmt, ...)	RAGE_ASSERTF(net_playercard,cond, fmt, ##__VA_ARGS__)

#if RSG_PC
RAGE_DEFINE_CHANNEL(net_dinput,					DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netDInputDebug(fmt, ...)				RAGE_DEBUGF3(net_dinput, fmt, ##__VA_ARGS__)
#endif

PARAM(netSessionForceHost,				    "[session] If present, will always host when calling FindSessions");
PARAM(netSessionRejectJoin,				    "[session] If present, will always reject join attempts");

PARAM(netSessionEnableLanguageBias,			"[session] If present, will enable languagw biased matchmaking for non-final");
PARAM(netSessionEnableRegionBias,			"[session] If present, will enable region biased matchmaking for non-final");

PARAM(netSessionIgnoreAll,				    "[session] If present, will ignore data hash, cheater and aim preferences matchmaking");
PARAM(netSessionIgnoreDataHash,			    "[session] If present, will ignore data hash matchmaking value");
PARAM(netSessionIgnoreCheater,			    "[session] If present, will ignore cheater matchmaking value");
PARAM(netSessionIgnoreAim,				    "[session] If present, will ignore aim preference matchmaking value");
PARAM(netSessionIgnoreTimeout,			    "[session] If present, will ignore time out matchmaking value. Not included in -All");
PARAM(netSessionIgnoreVersion,			    "[session] If present, will ignore time out matchmaking version");

// these are command line parameters to simulate bail events with very specific time requirements
PARAM(netSessionBailFinding,			    "[session] If present, we will simulate a bail when failing during find game process");
PARAM(netSessionBailAtHost,				    "[session] If present, we will simulate a bail when failing when call to host is made");
PARAM(netSessionBailHosting,			    "[session] If present, we will simulate a bail when failing during host game process");
PARAM(netSessionBailAtJoin,				    "[session] If present, we will simulate a bail when failing when call to join is made");
PARAM(netSessionBailJoining,			    "[session] If present, we will simulate a bail when failing during join game process");
PARAM(netSessionBailAttr,				    "[session] If present, we will simulate a bail when failing during change attribute process");
PARAM(netSessionBailJoinTimeout,		    "[session] If present, we will simulate a bail when failing during join game process via timeout");
PARAM(netSessionBailMigrate,		        "[session] If present, we will simulate a bail when migrating");

PARAM(netSessionStateTimeout,			    "[session] If present, sets the join state timeout to X");

PARAM(netSessionHostProbabilityEnable,      "[session] If present, disables probability MM");
PARAM(netSessionHostProbabilityMax,		    "[session] If present, sets the maximum probability matchmaking attempts to X");
PARAM(netSessionHostProbabilityStart,	    "[session] If present, sets the starting value of the host probability");
PARAM(netSessionHostProbabilityScale,	    "[session] If present, sets the scaling value (per MM) of the host probability");

PARAM(netSessionMaxPlayers,				    "[session] If present, sets the maximum number of players in a single session");
PARAM(netSessionMaxPlayersFreemode,		    "[session] If present, sets the maximum number of freemode players in a single session");
PARAM(netSessionMaxPlayersSCTV,			    "[session] If present, sets the maximum number of SCTV players in a single session");
PARAM(netSessionMaxPlayersActivity,		    "[session] If present, sets the maximum number of players in an activity");

PARAM(netSessionUniqueCrewLimit,		    "[session] If present, sets the maximum number of unique crews in a session");
PARAM(netSessionUniqueCrewLimitTransition,  "[session] If present, sets the maximum number of unique crews in a transition session");

PARAM(netSessionNoSessionBlacklisting,	    "[session] If present, prevents sessions from being blacklisted");
PARAM(netSessionNoGamerBlacklisting,	    "[session] If present, prevents gamers from being blacklisted");

PARAM(netSessionDisablePeerComplainer,	    "[session] If present, disables peer complainer");
PARAM(netSessionDisablePlatformMM,		    "[session] If present, disables platform matchmaking in quickmatch");
PARAM(netSessionDisableSocialMM,		    "[session] If present, disables social matchmaking in quickmatch");

PARAM(netAutoJoin,						    "[session] If present, joins a game straight away.");
PARAM(netAutoForceJoin,						"[session] If present, keeps matchmaking until a game is available.");
PARAM(netAutoForceHost,						"[session] If present, does not look for a game and goes straight to hosting one.");

PARAM(netSessionNoTransitionQuickmatch,     "[session] If present, transition quickmatch does not host");
PARAM(netSessionShowStateChanges,			"[session] If present, show session state changes on screen");

PARAM(netSessionEnableLaunchComplaints,		"[session] If present, enable client complaints during activity launch");
PARAM(netSessionCheckPartyForActivity,		"[session] If present, prompts players to select how to proceed with respect to parties on starting activities");
PARAM(netSessionDisableImmediateLaunch,		"[session] If present, disables immediate launch");
PARAM(netMatchmakingDisableDumpTelemetry,	"[session] If present, disable dumping telemetry to logging");
PARAM(netMatchmakingMaxTelemetry,			"[session] If present, max out the telemetry to test");
PARAM(netMatchmakingAlwaysRunSorting,		"[session] If present, will run sorting even with one session");
PARAM(netMatchmakingCrewID,					"[session] If present, sets the crew ID used for crew matchmaking");
PARAM(netMatchmakingTotalCrewCount,			"[session] If present, sets the total number of players in our crew");
PARAM(netSessionEnableActivityMatchmakingAnyJobQueries,		"[session] If present, enables augmenting job searches with any job of specified type");
PARAM(netSessionEnableActivityMatchmakingInProgressQueries,	"[session] If present, enables augmenting job searches with in progress jobs");
PARAM(netSessionEnableActivityMatchmakingContentFilter,		"[session] If present, enables content filtering on job queries");
PARAM(netSessionEnableActivityMatchmakingActivityIsland,	"[session] If present, enables activity island filtering on job queries");
PARAM(netSessionOpenNetworkOnQuickmatch,	"[session] If present, opens the network when we quickmatch");
PARAM(netBubbleAlwaysCheckIfRequired,		"[session] If present, enforces that hosts always check if bubbles are required");

#if RSG_PC
#include <windows.h>
#include <psapi.h>
#endif

#if !__FINAL
PARAM(netSessionAllAdminInvitesTreatedAsDev,"[session] If present, set all admin invite dev setting");
PARAM(netSessionNoAdminInvitesTreatedAsDev, "[session] If present, set no admin invite dev setting");
PARAM(netSessionAllowAdminInvitesToSolo,	"[session] If present, allow admin invites to solo");
#endif

XPARAM(mpavailable);
PARAM(netSessionMpAccessCode,               "[session] If present, transition quickmatch does not host");

static const char* gs_szNetTestBedScript = "MpTestbed";

static const int kMaxSearchAttempts = 5;
static const float kHostProbabilityStart = 0.1f;
static const float kHostProbabilityScale = 1.5f;
static const int g_MentalStateScale = (int)(250.0f / (float) MAX_MENTAL_STATE);
static const float DEFAULT_MIGRATION_TIMEOUT_MULTIPLIER = 1.2f;

// interval at which we increase the beacon value
static const int DEFAULT_BEACON_INTERVAL = 15000;

// aggressive - we want to fail fast
static const unsigned DEFAULT_PEER_COMPLAINER_AGGRESSIVE_BOOT_TIME = CNetwork::DEFAULT_CONNECTION_TIMEOUT / 2;

// normal - this will be setup based off the timeout in the tunables initialization
static const unsigned DEFAULT_PEER_COMPLAINER_GRACE_TIME = 5000;

// boss thresholds
static const unsigned DEFAULT_BOSS_DEPRIORITISE_THRESHOLD = 4;
static const unsigned DEFAULT_BOSS_REJECT_THRESHOLD = 6;
static const unsigned DEFAULT_BOSS_REJECT_THRESHOLD_INTRO_FLOW = 10;

// pending joiner
static const unsigned DEFAULT_PENDING_JOINER_TIMEOUT = 30000;

NETWORK_OPTIMISATIONS()

enum TimeoutSetting
{
	Timeout_Matchmaking,
	Timeout_MatchmakingOneSession,
	Timeout_MatchmakingMultipleSessions,
	Timeout_DirectJoin,
	Timeout_TransitionJoin,
	Timeout_TransitionLaunch,
	Timeout_TransitionKick,
	Timeout_TransitionComplainInterval,
	Timeout_Hosting,
	Timeout_Ending,
	Timeout_MatchmakingAttemptLong,
	Timeout_QuickmatchLong,
	Timeout_SessionBlacklist,
	Timeout_TransitionToGame,
	Timeout_TransitionPendingLeave,
	Timeout_TransitionWaitForUpdate,
	Timeout_Max,
};

// network timeouts
int s_TimeoutValues[] = 
{
#if RSG_DURANGO
	60000,										// TimeoutSetting::Timeout_Matchmaking (enough to survive a tunnel failure and presence request failure)
#else
	45000,										// TimeoutSetting::Timeout_Matchmaking (enough to survive a tunnel failure and presence request failure)
#endif
	80000,										// TimeoutSetting::Timeout_MatchmakingOneSession
	50000,										// TimeoutSetting::Timeout_MatchmakingMultipleSessions
	80000,										// TimeoutSetting::Timeout_DirectJoin
	50000,										// TimeoutSetting::Timeout_TransitionJoin
	15000,										// TimeoutSetting::Timeout_TransitionLaunch
	10000,										// TimeoutSetting::Timeout_TransitionKick
	CNetwork::DEFAULT_CONNECTION_TIMEOUT / 2,	// TimeoutSetting::Timeout_TransitionComplainInterval,
	45000,										// TimeoutSetting::Timeout_Hosting
	15000,										// TimeoutSetting::Timeout_Ending
	180000,										// TimeoutSetting::Timeout_MatchmakingAttemptLong
	180000,										// TimeoutSetting::Timeout_QuickmatchLong
	120000,										// TimeoutSetting::Timeout_SessionBlacklist
	50000, 										// TimeoutSetting::Timeout_TransitionToGame
	10000,										// TimeoutSetting::Timeout_TransitionPendingLeave
	30000,										// TimeoutSetting::Timeout_TransitionWaitForUpdate
};
CompileTimeAssert(COUNTOF(s_TimeoutValues) == TimeoutSetting::Timeout_Max);

const char* ATTR_TRANSITION_TOKEN = "trtok";
const char* ATTR_TRANSITION_ID = "trid";
const char* ATTR_TRANSITION_INFO = "trinfo";
const char* ATTR_TRANSITION_IS_HOST = "trhost";
const char* ATTR_TRANSITION_IS_JOINABLE = "trjoin";

#if !__NO_OUTPUT
static const char* gs_szMatchmakingGroupNames[] =
{
	"MM_GROUP_FREEMODER",	// MM_GROUP_FREEMODER
	"MM_GROUP_COP",			// MM_GROUP_COP
	"MM_GROUP_VAGOS",		// MM_GROUP_VAGOS
	"MM_GROUP_LOST",		// MM_GROUP_LOST
	"MM_GROUP_SCTV",		// MM_GROUP_SCTV
};
CompileTimeAssert(COUNTOF(gs_szMatchmakingGroupNames) == MM_GROUP_MAX);

static const char* gs_szSchemaNames[] = 
{
	"SCHEMA_GENERAL",		// SCHEMA_GENERAL
	"SCHEMA_GROUPS",		// SCHEMA_GROUPS
	"SCHEMA_ACTIVITY",   	// SCHEMA_ACTIVITY
};
CompileTimeAssert(COUNTOF(gs_szSchemaNames) == SCHEMA_NUM);

//static const char* gs_szPolicyNames[] = 
//{
//	"POLICY_NORMAL",		// POLICY_NORMAL
//	"POLICY_AGGRESSIVE",	// POLICY_AGGRESSIVE
//};
//CompileTimeAssert(COUNTOF(gs_szPolicyNames) == CNetworkSession::POLICY_MAX);

static const char* gs_szLeaveReason[] = 
{
    "Leave_Normal",				// Leave_Normal
    "Leave_TransitionLaunch",	// Leave_TransitionLaunch
    "Leave_Kicked",				// Leave_Kicked
    "Leave_ReservingSlot",		// Leave_ReservingSlot
    "Leave_Error",				// Leave_Error
    "Leave_KickedAdmin",		// Leave_KickedAdmin
};
CompileTimeAssert(COUNTOF(gs_szLeaveReason) == LeaveSessionReason::Leave_Num);

const char* CNetworkSession::GetSessionStateAsString(eSessionState nState)
{
	static const char* s_StateNames[] =
	{
		"SS_IDLE",
		"SS_FINDING",
		"SS_HOSTING",
		"SS_JOINING",
		"SS_ESTABLISHING",
		"SS_ESTABLISHED",
		"SS_BAIL",
		"SS_HANDLE_KICK",
		"SS_LEAVING",
		"SS_DESTROYING",
	};
	CompileTimeAssert(COUNTOF(s_StateNames) == SS_MAX);
	return s_StateNames[nState == SS_INVALID ? m_SessionState : nState];
}

const char* CNetworkSession::GetTransitionStateAsString(eTransitionState nState)
{
	static const char* s_StateNames[] =
	{
		"TS_IDLE",
		"TS_HOSTING",
		"TS_JOINING",
		"TS_ESTABLISHING",
		"TS_ESTABLISHED",
		"TS_LEAVING",
		"TS_DESTROYING",
	};
	CompileTimeAssert(COUNTOF(s_StateNames) == TS_MAX);
	return s_StateNames[nState == TS_INVALID ? m_TransitionState : nState];
}

const char* GetSoloReasonAsString(eSoloSessionReason nReason)
{
	static const char* s_ReasonNames[] =
	{
		"SOLO_SESSION_HOSTED_FROM_SCRIPT",
		"SOLO_SESSION_HOSTED_FROM_MM",
		"SOLO_SESSION_HOSTED_FROM_JOIN_FAILURE",
		"SOLO_SESSION_HOSTED_FROM_QM",
		"SOLO_SESSION_HOSTED_FROM_UNKNOWN",
		"SOLO_SESSION_MIGRATE_WHEN_JOINING_VIA_MM",
		"SOLO_SESSION_MIGRATE_WHEN_JOINING_DIRECT",
		"SOLO_SESSION_MIGRATE",
		"SOLO_SESSION_TO_GAME_FROM_SCRIPT",
		"SOLO_SESSION_HOSTED_TRANSITION_WITH_GAMERS",
		"SOLO_SESSION_HOSTED_TRANSITION_WITH_FOLLOWERS",
		"SOLO_SESSION_PLAYERS_LEFT",
	};
	CompileTimeAssert(COUNTOF(s_ReasonNames) == SOLO_SESSION_MAX);
	return s_ReasonNames[nReason];
}

const char* GetVisibilityAsString(eSessionVisibility nVisibility)
{
	static const char* s_VisibilityNames[] =
	{
		"VISIBILITY_OPEN",
		"VISIBILITY_PRIVATE",
		"VISIBILITY_CLOSED_FRIEND",
		"VISIBILITY_CLOSED_CREW"
	};
	CompileTimeAssert(COUNTOF(s_VisibilityNames) == VISIBILITY_MAX);
	return s_VisibilityNames[nVisibility];
}

const char* GetJoinFailureActionAsString(JoinFailureAction joinFailureAction)
{
	static const char* s_Strings[] =
	{
		"JFA_None",
		"JFA_Quickmatch",
		"JFA_HostPublic",
		"JFA_HostPrivate",
		"JFA_HostSolo",
	};
	CompileTimeAssert(COUNTOF(s_Strings) == JoinFailureAction::JFA_Max);
	return ((joinFailureAction >= JoinFailureAction::JFA_None) && (joinFailureAction < JoinFailureAction::JFA_Max)) ? s_Strings[joinFailureAction] : "JFA_Invalid";
}

const char* GetMatchmakingQueryTypeAsString(MatchmakingQueryType query)
{
	static const char* s_Strings[] =
	{
		"MQT_Standard",
		"MQT_Social",
		"MQT_Friends",
	};
	CompileTimeAssert(COUNTOF(s_Strings) == MatchmakingQueryType::MQT_Num);
	return ((query > MatchmakingQueryType::MQT_Invalid) && (query < MatchmakingQueryType::MQT_Num)) ? s_Strings[query] : "MQT_Invalid";
}

const char* GetActivityMatchmakingQueryAsString(ActivityMatchmakingQuery query)
{
	static const char* s_Strings[] =
	{
		"AMQ_SpecificJob",
		"AMQ_AnyJob",
		"AMQ_InProgressSpecificJob",
		"AMQ_InProgressAnyJob",
	};
	CompileTimeAssert(COUNTOF(s_Strings) == ActivityMatchmakingQuery::AMQ_Num);
	return ((query > ActivityMatchmakingQuery::AMQ_Invalid) && (query < ActivityMatchmakingQuery::AMQ_Num)) ? s_Strings[query] : "AMQ_Invalid";
}

#define CHECK_AND_LOG_FLAG(x) if((nFlags & x) != 0) { gnetDebug1("\t%s", #x); }

void LogHostFlags(const unsigned nFlags, const char* szFunction)
{
    gnetDebug1("%s :: LogHostFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_FLAG(HostFlags::HF_IsPrivate);
	CHECK_AND_LOG_FLAG(HostFlags::HF_NoMatchmaking);
	CHECK_AND_LOG_FLAG(HostFlags::HF_SingleplayerOnly);
	CHECK_AND_LOG_FLAG(HostFlags::HF_Closed);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ClosedFriends);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ClosedCrew);
	CHECK_AND_LOG_FLAG(HostFlags::HF_AllowPrivate);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaMatchmaking);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaScript);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaJoinFailure);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaQuickmatchNoQueries);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaRetry);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaScriptToGame);
	CHECK_AND_LOG_FLAG(HostFlags::HF_JobToJob);
	CHECK_AND_LOG_FLAG(HostFlags::HF_Asynchronous);
	CHECK_AND_LOG_FLAG(HostFlags::HF_Premium);
	CHECK_AND_LOG_FLAG(HostFlags::HF_ViaTransitionLobby);
	CHECK_AND_LOG_FLAG(HostFlags::HF_JoinViaPresenceDisabled);
	CHECK_AND_LOG_FLAG(HostFlags::HF_JoinInProgressDisabled);
}

void LogJoinFlags(const unsigned nFlags, const char* szFunction)
{
    gnetDebug1("%s :: LogJoinFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_ViaInvite);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_ConsumePrivate);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_IsAdminDev);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_IsAdmin);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_Asynchronous);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_BadReputation);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_IsBoss);
	CHECK_AND_LOG_FLAG(JoinFlags::JF_ExpandedIntroFlow);
}

void LogLeaveSessionFlags(const unsigned nFlags, const char* szFunction)
{
	gnetDebug1("%s :: LogLeaveSessionFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_FLAG(LeaveFlags::LF_ReturnToLobby);
	CHECK_AND_LOG_FLAG(LeaveFlags::LF_BlacklistSession);
	CHECK_AND_LOG_FLAG(LeaveFlags::LF_NoTransitionBail);
}

void LogMatchmakingFlags(const unsigned nFlags, const char* szFunction)
{
    gnetDebug1("%s :: LogMatchmakingFlags (0x%x)", szFunction, nFlags);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_Asynchronous);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_Quickmatch);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_AllowBlacklisted);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_AnySessionType);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_SameRegionOnly);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_SortInQueryOrder);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_DisableAnyJobQueries);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_DisableInProgressQueries);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_QueryOnly);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_RockstarCreatedOnly);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_UserCreatedOnly);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_IsBoss);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_JobToJob);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_NoScriptEventOnBail);
    CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_BailFromLaunchedJobs);
	CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_InProgressOnly);
	CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_ToGameViaTransiton);
	CHECK_AND_LOG_FLAG(MatchmakingFlags::MMF_ExpandedIntroFlow);
}
#endif  // !__NO_OUTPUT

bool CheckFlag(const unsigned nFlags, const unsigned nFlagsToCheck)
{
	return (nFlags & nFlagsToCheck) == nFlagsToCheck;
}

void SetFlags(unsigned& nFlags, const unsigned nFlagsToAdd)
{
	nFlags |= nFlagsToAdd;
}

void RemoveFlags(unsigned& nFlags, const unsigned nFlagsToRemove)
{
	nFlags &= ~nFlagsToRemove;
}

bool IsKickDueToConnectivity(KickReason kickReason)
{
	switch(kickReason)
	{
	case KickReason::KR_PeerComplaints:
	case KickReason::KR_ConnectionError:
	case KickReason::KR_NatType:
		return true;

	default:
		return false;
	}
}

#if RSG_DURANGO

// allows a permission check prior to a 
class SendPresenceInviteTask : public rlTaskBase
{
public:

	RL_TASK_DECL(SendPresenceInviteTask);
	SendPresenceInviteTask() { m_nGamers = 0; } 

	bool Configure(
		const unsigned nGamers, 
		const rlGamerHandle* pGamers, 
		const rlSessionInfo& hSession, 
		const char* szContentId, 
		const rlGamerHandle& hContentCreator,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType, 
		const unsigned nPlaylistLength, 
		const unsigned nPlaylistCurrent,
		const unsigned nFlags);

	virtual void Start();
	virtual void Finish(const FinishType finishType, const int resultCode = 0);
	virtual void Update(const unsigned timeStep);

private:

	unsigned m_nGamers;
	netStatus m_PermissionStatus[RL_MAX_GAMERS_PER_SESSION];
	PrivacySettingsResult m_PermissionResult[RL_MAX_GAMERS_PER_SESSION];
	rlGamerHandle m_Gamers[RL_MAX_GAMERS_PER_SESSION];
	rlSessionInfo m_hSession;
	char m_szContentId[RLUGC_MAX_CONTENTID_CHARS];
	rlGamerHandle m_hContentCreator;
	char m_UniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
	int m_InviteFrom;
	int m_InviteType;
	unsigned m_nPlaylistLength;
	unsigned m_nPlaylistCurrent;
	unsigned m_Flags;
};

bool SendPresenceInviteTask::Configure(
	const unsigned nGamers, 
	const rlGamerHandle* pGamers, 
	const rlSessionInfo& hSession, 
	const char* szContentId, 
	const rlGamerHandle& hContentCreator, 
	const char* pUniqueMatchId, 
	const int nInviteFrom, 
	const int nInviteType, 
	const unsigned nPlaylistLength, 
	const unsigned nPlaylistCurrent, 
	const unsigned nFlags)
{
	m_nGamers = nGamers;
	if(m_nGamers > RL_MAX_GAMERS_PER_SESSION)
	{
		rlTaskDebug("Only %d gamers supported. Not sending to %d gamers.", RL_MAX_GAMERS_PER_SESSION, RL_MAX_GAMERS_PER_SESSION - nGamers)
		m_nGamers = RL_MAX_GAMERS_PER_SESSION;
	}

	// copy in gamers
	for(unsigned i = 0; i < m_nGamers; i++)
		m_Gamers[i] = pGamers[i];

	m_hSession = hSession;
	safecpy(m_szContentId, szContentId);
	m_hContentCreator = hContentCreator;
	safecpy(m_UniqueMatchId, pUniqueMatchId);
	m_InviteFrom = nInviteFrom;
	m_InviteType = nInviteType;
	m_nPlaylistLength = nPlaylistLength;
	m_nPlaylistCurrent = nPlaylistCurrent;
	m_Flags = nFlags;

	return true;
}

void SendPresenceInviteTask::Start()
{
	this->rlTaskBase::Start();
	for(unsigned i = 0; i < m_nGamers; i++)
		IPrivacySettings::CheckPermission(NetworkInterface::GetLocalGamerIndex(), IPrivacySettings::PERM_PlayMultiplayer, m_Gamers[i], &m_PermissionResult[i], &m_PermissionStatus[i]);
}

void SendPresenceInviteTask::Update(const unsigned timeStep)
{
	this->rlTaskBase::Update(timeStep);

	bool bFinished = true;
	for(unsigned i = 0; i < m_nGamers; i++) if(m_PermissionStatus[i].Pending())
		bFinished = false;

	if(bFinished)
		Finish(rlTaskBase::FINISH_SUCCEEDED);
}

void SendPresenceInviteTask::Finish(const FinishType finishType, const int resultCode)
{
	// call up to ensure task is cleaned up
	this->rlTaskBase::Finish(finishType, resultCode);

	unsigned nValidHandles = 0;
	rlGamerHandle aValidHandles[RL_MAX_GAMERS_PER_SESSION];
	for(unsigned i = 0; i < m_nGamers; i++)
	{
		if(m_PermissionResult[i].m_isAllowed)
			aValidHandles[nValidHandles++] = m_Gamers[i];
	}

	if(nValidHandles > 0)
		CGameInvite::Trigger(aValidHandles, nValidHandles, m_hSession, m_szContentId, m_hContentCreator, m_UniqueMatchId, m_InviteFrom, m_InviteType, m_nPlaylistLength, m_nPlaylistCurrent, m_Flags);
}
#endif

void SessionUserData_Freeroam::Clear()
{
	gnetDebug1("SessionUserData_Freeroam::Clear");
	m_ux = m_uy = m_uz = 0;
    m_nPlayerAim = CPedTargetEvaluator::MAX_TARGETING_OPTIONS;
    m_nAverageCharacterRank = 0;
    m_nTransitionPlayers = 0;
    for(int i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
        m_nPropertyID[i] = NO_PROPERTY_ID;
    }
	m_nAverageMentalState = 0;
	m_nBeacon = 0;
	m_nFlags = 0;
	for(int i = 0; i < MAX_UNIQUE_CREWS; i++)
	{
		m_nCrewID[i] = RL_INVALID_CLAN_ID;
	}
	m_nNumBosses = 0;
	WIN32PC_ONLY(m_nAveragePedDensity = 0;)
}

void SessionUserData_Activity::Clear()
{
	gnetDebug1("SessionUserData_Activity::Clear");
    m_hContentCreator.Clear();
    m_nPlayerAim = CPedTargetEvaluator::MAX_TARGETING_OPTIONS;
    m_nAverageCharacterRank = 0;
	m_nMinimumRank = 0; 
	m_nBeacon = 0;
	m_nAverageELO = 0;
	m_bIsWaitingAsync = false;
	m_bIsPreferred = false;
	m_InProgressFinishTime = 0;
}

CompileTimeAssert(sizeof(sMatchmakingInfoMine) <= RL_MAX_SESSION_DATA_MINE_SIZE);

ConnectionManagerDelegate::ConnectionManagerDelegate()
{
	m_CxnMgrDlgt.Bind(this, &ConnectionManagerDelegate::OnConnectionEvent);
}

void ConnectionManagerDelegate::Init(netConnectionManager* pCxnMgr, unsigned nChannelID)
{
	m_nChannelID = nChannelID;
	pCxnMgr->AddChannelDelegate(&m_CxnMgrDlgt, m_nChannelID);
}

void ConnectionManagerDelegate::Shutdown()
{
	m_CxnMgrDlgt.Unregister();
}

void ConnectionManagerDelegate::OnConnectionEvent(netConnectionManager* pCxnMgr, const netEvent* pEvent)
{
	// forward to network session
	CNetwork::GetNetworkSession().OnConnectionEvent(pCxnMgr, pEvent);
}

static mthRandom gs_Random;

enum ReservationGroup
{
    // non zero values
    RESERVE_PLAYER = 100,
    RESERVE_SPECTATOR = 200,
};

CNonPhysicalPlayerData* TryAllocateNonPhysicalPlayerData()
{
	if(!CNetwork::GetPlayerMgr().IsInitialized())
		return nullptr;

	if(!CNonPhysicalPlayerData::GetPool())
		return nullptr; 

	return rage_checked_pool_new(CNonPhysicalPlayerData);
}

CNetworkSession::CNetworkSession() 
: m_SessionState(SS_IDLE)
, m_LastUpdateTime(0)
, m_SessionStateTime(0)
, m_IsInitialised(false)
, m_LeaveReason(LeaveSessionReason::Leave_Normal)
, m_LeaveSessionFlags(LeaveFlags::LF_Default)
, m_bIsJoiningViaMatchmaking(false)
, m_nQuickMatchStarted(0)
, m_TimeSessionStarted(0)
, m_nSessionTimeout(0)
, m_fMigrationTimeoutMultiplier(DEFAULT_MIGRATION_TIMEOUT_MULTIPLIER)
, m_WaitFramesSinglePlayerScriptCleanUpBeforeStart(0)
, m_nBailReason(BAIL_INVALID)
, m_bBlockJoinRequests(false)
, m_bJoinedViaInvite(false)
, m_bJoinIncomplete(false)
, m_nMatchmakingFlags(0)
, m_bScriptValidatingJoin(false)
, m_bScriptHandlingClockAndWeather(false)
, m_bEnableHostProbability(true)
, m_fMatchmakingHostProbability(1.0f)
, m_nMatchmakingAttempts(0)
, m_MaxMatchmakingAttempts(kMaxSearchAttempts)
, m_fMatchmakingHostProbabilityStart(kHostProbabilityStart)
, m_fMatchmakingHostProbabilityScale(kHostProbabilityScale)
, m_MatchmakingGroup(MM_GROUP_INVALID)
, m_bIsMigrating(false)
, m_TransitionState(TS_IDLE)
, m_TransitionStateTime(0)
, m_bLaunchingTransition(false)
, m_TimeTokenForLaunchedTransition(0)
, m_bTransitionStartPending(false)
, m_bDoLaunchPending(false)
, m_nDoLaunchCountdown(0)
, m_nImmediateLaunchCountdown(0)
, m_nImmediateLaunchCountdownTime(DEFAULT_IMMEDIATE_LAUNCH_COUNTDOWN)
, m_bLaunchOnStartedFromJoin(false)
, m_bAllowImmediateLaunchDuringJoin(true)
, m_bAllowBailForLaunchedJoins(true)
, m_bHasPendingTransitionBail(false)
, m_bDoNotLaunchFromJoinAsMigratedHost(false)
, m_bIsTransitionMatchmaking(false)
, m_nTransitionMatchmakingFlags(0)
, m_bIsTransitionToGame(false)
, m_bIsTransitionToGameInSession(false)
, m_bIsTransitionToGameNotifiedSwap(false)
, m_bIsTransitionToGamePendingLeave(false)
, m_bIsTransitionToGameFromLobby(false)
, m_bTransitionToGameRemovePhysicalPlayers(true)
, m_nToGameCompletedTimestamp(0)
, m_nGamersToGame(0) 
, m_bJoinedTransitionViaInvite(false)
, m_bPreventClearTransitionWhenJoiningInvite(true)
, m_bAllAdminInvitesTreatedAsDev(false)
, m_bNoAdminInvitesTreatedAsDev(false)
, m_bAllowAdminInvitesToSolo(true)
, m_nPostMigrationBubbleCheckTime(0)
, m_nTransitionInviteFlags(0)
, m_bIsTransitionAsyncWait(false)
, m_bBlockTransitionJoinRequests(false)
, m_bAutoJoining(false)
, m_bIsInviteProcessPendingForCommerce(false)
, m_bIsInvitePendingJoin(false)
, m_bTransitionViaMainMatchmaking(false)
, m_JoinFailureAction(JoinFailureAction::JFA_None)
, m_bRunningDetailQuery(false)
, m_nJoinSessionDetail(0)
, m_HasDirtySessionUserDataFreeroam(false)
, m_nLastSessionUserDataRefresh(0)
, m_HasDirtySessionUserDataActivity(false)
, m_nMatchmakingPropertyID(NO_PROPERTY_ID)
, m_nMatchmakingMentalState(NO_MENTAL_STATE)
, m_nMatchmakingMinimumRank(0)
, m_nMatchmakingELO(0)
#if RSG_PRESENCE_SESSION
, m_bPresenceSessionInvitesBlocked(false)
, m_bRequirePresenceSession(false)
, m_bPresenceSessionFailed(false)
#endif //RSG_PRESENCE_SESSION
, m_bHasSetActivitySpectatorsMax(false)
, m_nActivitySpectatorsMax(0)
, m_nActivityPlayersMax(0)
, m_bIsActivitySession(false)
, m_bIsLeavingLaunchedActivity(false)
, m_LastKickReason(KickReason::KR_Invalid)
, m_bPreviousWasPrivate(false)
, m_nPreviousMaxSlots(MAX_NUM_PHYSICAL_PLAYERS)
, m_bMainSyncedRadio(false)
, m_bTransitionSyncedRadio(false)
, m_nLastActivitySlotsCheck(0)
, m_nLastMatchmakingGroupCheck(0)
, m_bForceScriptPresenceUpdate(false)
, m_bCheckForAnotherSession(false)
, m_nBeaconInterval(DEFAULT_BEACON_INTERVAL)
, m_bUseMatchmakingBeacon(true)
, m_nGamersToActivity(0)
, m_bIsActivityGroupQuickmatch(false)
, m_bIsActivityGroupQuickmatchAsync(false)
, m_nFollowers(false)
, m_bRetainFollowers(false)
, m_nLanguageBiasMask(0)
, m_nRegionBiasMask(0)
, m_nLanguageBiasMaskForRegion(0)
, m_nRegionBiasMaskForLanguage(0)
, m_bEnableLanguageBias(true)
, m_bEnableRegionBias(true)
, m_bCanRetryJoinFailure(false)
, m_nJoinFlags(JoinFlags::JF_Default)
, m_bCanRetryTransitionJoinFailure(false)
, m_nTransitionJoinFlags(JoinFlags::JF_Default)
#if RSG_DURANGO
, m_bHandlePartyActivityDialog(false)
, m_bShowingPartyActivityDialog(false)
, m_bValidateGameSession(false)
, m_bMigrationSentMultiplayerEndEvent(false)
, m_bMigrationSentMultiplayerEndEventTransition(false)
#endif
, m_nJoinQueueToken(0)
, m_bCanQueue(false)
, m_LastCheckForJoinRequests(0)
#if RL_NP_SUPPORT_PLAY_TOGETHER
, m_nPlayTogetherPlayers(0)
, m_bUsePlayTogetherOnFilter(true)
, m_bUsePlayTogetherOnJoin(true)
#endif
, m_nNumQuickmatchOverallResultsBeforeEarlyOut(6)
, m_nNumQuickmatchOverallResultsBeforeEarlyOutNonSolo(0)
, m_nNumQuickmatchResultsBeforeEarlyOut(0)
, m_nNumQuickmatchSocialResultsBeforeEarlyOut(0)
, m_nNumSocialResultsBeforeEarlyOut(0)
, m_bUseBlacklist(true)
, m_AdminInviteNotificationRequired(false)
, m_AdminInviteKickToMakeRoom(false)
, m_AdminInviteKickIncludesLocal(false)
, m_bImmediateFriendSessionRefresh(true)
, m_nPeerComplainerBootDelay((CNetwork::DEFAULT_CONNECTION_TIMEOUT / 2) + DEFAULT_PEER_COMPLAINER_GRACE_TIME)
, m_nPeerComplainerAggressiveBootDelay(DEFAULT_PEER_COMPLAINER_AGGRESSIVE_BOOT_TIME)
, m_bAllowComplaintsWhenEstablishing(true)
, m_bPreferToBootEstablishingCandidates(true)
, m_bPreferToBootLowestNAT(true)
, m_InvitedPlayers(SessionType::ST_Physical)
, m_TransitionInvitedPlayers(SessionType::ST_Transition)
, m_nNumActivityResultsBeforeEarlyOut(0)
, m_nNumActivityOverallResultsBeforeEarlyOutNonSolo(0)
, m_bActivityMatchmakingDisableAnyJobQueries(false)
, m_bActivityMatchmakingDisableInProgressQueries(false)
, m_bActivityMatchmakingContentFilterDisabled(false)
, m_bActivityMatchmakingActivityIslandDisabled(false)
, m_nBossRejectThreshold(DEFAULT_BOSS_REJECT_THRESHOLD)
, m_nBossDeprioritiseThreshold(DEFAULT_BOSS_DEPRIORITISE_THRESHOLD)
, m_bRejectSessionsWithLargeNumberOfBosses(false)
, m_IntroBossRejectThreshold(DEFAULT_BOSS_REJECT_THRESHOLD_INTRO_FLOW)
, m_IntroBossDeprioritiseThreshold(DEFAULT_BOSS_DEPRIORITISE_THRESHOLD)
, m_IntroRejectSessionsWithLargeNumberOfBosses(true)
, m_bEnterMatchmakingAsBoss(false)
, m_bMigrationSessionNoEstablishedPools(false)
, m_bMigrationTransitionNoEstablishedPools(false)
, m_bMigrationTransitionNoEstablishedPoolsForToGame(false)
, m_bMigrationTransitionNoSpectatorPools(false)
, m_bMigrationTransitionNoSpectatorPoolsForToGame(false)
, m_bMigrationTransitionNoAsyncPools(false)
, m_bDisableDelayedMatchmakingAdvertise(false)
, m_bCancelAllTasksOnLeave(true)
, m_bCancelAllConcurrentTasksOnLeave(false)
, m_bCheckForAnotherSessionOnMigrationFail(true)
, m_bPreventStartWithTemporaryPlayers(true)
, m_bPreventStartWhileAddingPlayers(true)
, m_nEnterMultiplayerTimestamp(0)
, m_MigrationStartedTimestamp(0)
, m_MigrationStartPlayerCount(0)
, m_nHostAimPreference(CPedTargetEvaluator::MAX_TARGETING_OPTIONS)
, m_bFreeroamIsJobToJob(false)
, m_bSendRemovedFromSessionKickTelemetry(true)
, m_bKickWhenRemovedFromSession(false)
, m_bKickCheckForAnotherSession(false)
, m_SendMigrationTelemetry(true)
, m_bSendHostDisconnectMsg(true)
, m_PendingHostPhysical(false)
, m_PendingJoinerTimeout(DEFAULT_PENDING_JOINER_TIMEOUT)
, m_PendingJoinerEnabled(true)
, m_SessionTypeForTelemetry(SessionTypeForTelemetry::STT_Invalid)
{
    // initialise session pointers
	m_pSession = &m_Sessions[0];
	m_pTransition = &m_Sessions[1];
	m_pSessionStatus = &m_Status[0];
	m_pTransitionStatus = &m_Status[1];
	
    // bind session delegates
	m_SessionDlgt[SessionType::ST_Physical].Bind(this, &CNetworkSession::OnSessionEvent);
	m_SessionDlgt[SessionType::ST_Transition].Bind(this, &CNetworkSession::OnSessionEventTransition);

	// bind session manager delegate
	rlSessionManager::ms_rlSesMgrQueryDelegate.Bind(this, &CNetworkSession::OnSessionQueryReceived);
	rlSessionManager::ms_rlSesMgrGetInfoMineDelegate.Bind(this, &CNetworkSession::OnGetInfoMine);

	// initialise session owner data
	m_SessionOwner[SessionType::ST_Physical].HandleJoinRequest.Bind(this, &CNetworkSession::OnHandleGameJoinRequest);
	m_SessionOwner[SessionType::ST_Physical].GetGamerData.Bind(this, &CNetworkSession::OnGetGameGamerData);
	m_SessionOwner[SessionType::ST_Physical].GetHostMigrationCandidates.Bind(this, &CNetworkSession::GetMigrationCandidates);
	m_SessionOwner[SessionType::ST_Physical].ShouldMigrate.Bind(this, &CNetworkSession::ShouldMigrate);

	// initialise transition owner data
	//@@: location CNETWORKSESSION_INIT_DEFINE_SESSION_TRANSITIONS
	m_SessionOwner[SessionType::ST_Transition].HandleJoinRequest.Bind(this, &CNetworkSession::OnHandleTransitionJoinRequest);
	m_SessionOwner[SessionType::ST_Transition].GetGamerData.Bind(this, &CNetworkSession::OnGetGamerDataTransition);
	m_SessionOwner[SessionType::ST_Transition].GetHostMigrationCandidates.Bind(this, &CNetworkSession::GetMigrationCandidatesTransition);
	m_SessionOwner[SessionType::ST_Transition].ShouldMigrate.Bind(this, &CNetworkSession::ShouldMigrateTransition);

	m_TimeCreated[SessionType::ST_Physical] = 0;
	m_TimeCreated[SessionType::ST_Transition] = 0;

#if RSG_PRESENCE_SESSION
	// bind presence session delegate
	m_PresenceSessionDlgt.Bind(this, &CNetworkSession::OnSessionEventPresence);
#endif

	// bind peer complainer delegate
	m_PeerComplainerDelegate.Bind(this, &CNetworkSession::OnPeerComplainerBootRequest);

    for(int i = 0; i < MM_GROUP_MAX; i++)
        m_MatchmakingGroupMax[i] = 0;

	for(int i = 0; i < SessionType::ST_Max; i++)
	{
		m_nHostFlags[i] = HostFlags::HF_Default;
		m_bIgnoreMigrationFailure[i] = false;
		m_nSessionPool[i] = POOL_NORMAL;
        m_bLockSetting[i] = true;
        m_bLockVisibility[i] = false; 
        m_bChangeAttributesPending[i] = false;
		m_LastMigratedPosix[i] = 0;
		DURANGO_ONLY(m_bHasWrittenStartEvent[i] = false;)
	}

    for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
        m_MatchmakingQueryEnabled[i] = true;

	// default retry policy
    SetSessionPolicies(POLICY_NORMAL);

	// clear out previous session
	m_PreviousSession.Clear(); 
	m_JoinDetailStatus.Reset();

#if RSG_NP || __STEAM_BUILD
	// do we need to clear our presence blob?
	if(m_SessionInfoBlob.IsValid())
	{
		rlPresence::SetBlob(NULL, 0);
		m_SessionInfoBlob.Clear();
	}
#endif

#if RSG_OUTPUT
	gnetDebug1("Session :: FreeroamDataSize: %u, ActivityDataSize: %u",
		static_cast<unsigned>(sizeof(SessionUserData_Freeroam)),
		static_cast<unsigned>(sizeof(SessionUserData_Activity)));
#endif
}

CNetworkSession::~CNetworkSession()
{
	// reset session manager delegates
	rlSessionManager::ms_rlSesMgrQueryDelegate.Reset();
	rlSessionManager::ms_rlSesMgrGetInfoMineDelegate.Reset();
}

bool CNetworkSession::Init()
{
	gnetDebug1("Init");

	if(!gnetVerify(!m_IsInitialised))
	{
		gnetError("Init :: Already initialised!");
		return false; 
	}

	// initialise random number generator
	gs_Random.Reset(static_cast<int>(sysTimer::GetSystemMsTime()));

    m_DelayedPresenceInfo.Reset();

	gnetAssertf(SS_IDLE == m_SessionState, "Init :: Initialising NetworkSession when it is in an invalid state!");
	gnetAssertf(!m_Config[SessionType::ST_Physical].IsValid(), "Init :: Initialising NetworkSession when it has a valid match config!");

	// initialise session
	bool bSuccess = m_pSession->Init(CLiveManager::GetRlineAllocator(),
								     m_SessionOwner[SessionType::ST_Physical],
								     &CNetwork::GetConnectionManager(),
								     NETWORK_SESSION_GAME_CHANNEL_ID,
								     "gta5");

	// apply attribute names
	m_pSession->SetPresenceAttributeNames(rlScAttributeId::GameSessionToken.Name,
										  rlScAttributeId::GameSessionId.Name,
										  rlScAttributeId::GameSessionInfo.Name,
										  rlScAttributeId::IsGameHost.Name,
										  rlScAttributeId::IsGameJoinable.Name,
										  false);

	if(!gnetVerify(bSuccess))
	{
		gnetError("Init :: Error initialising session");
		return false; 
	}

	m_pSession->AddDelegate(&m_SessionDlgt[SessionType::ST_Physical]);
	m_ConnectionDlgtGame.Init(&CNetwork::GetConnectionManager(), NETWORK_GAME_CHANNEL_ID);
    m_ConnectionDlgtGameSession.Init(&CNetwork::GetConnectionManager(), NETWORK_SESSION_GAME_CHANNEL_ID);
    m_ConnectionDlgtActivitySession.Init(&CNetwork::GetConnectionManager(), NETWORK_SESSION_ACTIVITY_CHANNEL_ID);

	InitTransition();

#if RSG_PRESENCE_SESSION
	// initialise presence session
	InitPresenceSession();
#endif

#if RSG_NP || __STEAM_BUILD
    // do we need to clear our presence blob?
    if(m_SessionInfoBlob.IsValid())
    {
        rlPresence::SetBlob(NULL, 0);
        m_SessionInfoBlob.Clear();
    }
#endif

#if !__FINAL
	int nParam; 
	if(PARAM_netSessionAllAdminInvitesTreatedAsDev.Get(nParam))
	{
		gnetDebug1("Init :: %s AllAdminInvitesAsDev", nParam ? "Enabling" : "Disabling");
		m_bAllAdminInvitesTreatedAsDev = (nParam > 0);
	}
	if(PARAM_netSessionNoAdminInvitesTreatedAsDev.Get(nParam))
	{
		gnetDebug1("Init :: %s NoAdminInvitesAsDev", nParam ? "Enabling" : "Disabling");
		m_bNoAdminInvitesTreatedAsDev = (nParam > 0);
	}
	if(PARAM_netSessionAllowAdminInvitesToSolo.Get(nParam))
	{
		gnetDebug1("Init :: %s AllowAdminInvitesToSolo", nParam ? "Enabling" : "Disabling");
		m_bAllowAdminInvitesToSolo = (nParam > 0);
	}
#endif

	m_BlacklistedSessions.Reset();

	m_SessionStateTime = 0;
	m_LeaveReason = LeaveSessionReason::Leave_Normal;
	m_LeaveSessionFlags = LeaveFlags::LF_Default;
	m_bIsJoiningViaMatchmaking = false;
	m_GameMMResults.m_nCandidateToJoinIndex = 0;
	m_TimeSessionStarted = 0;
	m_nSessionTimeout = 0;
	m_bScriptValidatingJoin = false;
	m_bScriptHandlingClockAndWeather = false;
	m_Inviter.Clear();

	m_bRunningDetailQuery = false;
	m_JoinFailureAction = JoinFailureAction::JFA_None;
	m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Invalid;
	m_nJoinSessionDetail = 0;
	m_JoinDetailStatus.Reset();

    m_bIsInviteProcessPendingForCommerce = false;
    m_bIsInvitePendingJoin = false;

	for(int i = 0; i < MM_GROUP_MAX; i++)
		m_MatchmakingGroupMax[i] = 0;

	ClearActiveMatchmakingGroups();

	// force these for now
	m_MatchmakingGroup = MM_GROUP_FREEMODER;
	//@@: location CNETWORKSESSION_INIT_SET_MATCHMAKING_GROUP_MAX
	m_MatchmakingGroupMax[MM_GROUP_FREEMODER] = 24;
	m_MatchmakingGroupMax[MM_GROUP_COP] = 16;
	m_MatchmakingGroupMax[MM_GROUP_VAGOS] = 16;
	m_MatchmakingGroupMax[MM_GROUP_LOST] = 16;
	m_MatchmakingGroupMax[MM_GROUP_SCTV] = 8;

	// reset matchmaking parameters 
	m_nMatchmakingPropertyID = NO_PROPERTY_ID;
	m_nMatchmakingMentalState = NO_MENTAL_STATE;
	m_nMatchmakingMinimumRank = 0;
	m_nMatchmakingELO = 0;
	m_bEnterMatchmakingAsBoss = false;

	// reset states
	SetSessionState(SS_IDLE);
	SetTransitionState(TS_IDLE);

	// setup matchmaking queries
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
		m_MatchMakingQueries[i].Init(&m_SessionFilter);

	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++)
		m_ActivityMMQ[i].Init(&m_TransitionFilter[i]);

#if !__FINAL
	// set debug parameters
	if(PARAM_netSessionIgnoreAll.Get())
	{
		// not setting timeout here - we should explicitly toggle that on if required
		PARAM_netSessionIgnoreDataHash.Set("1");
		PARAM_netSessionIgnoreCheater.Set("1");
		PARAM_netSessionIgnoreAim.Set("1");
	}
#endif

	// successfully initialised
    m_IsInitialised = true;

	// check for auto-join
	static bool s_bHasProcessedAutoJoin = false;
	if(!s_bHasProcessedAutoJoin && PARAM_netAutoJoin.Get())
	{
		m_bAutoJoining = true; 
		s_bHasProcessedAutoJoin = true;
	}

	m_bForceScriptPresenceUpdate = false;
    m_bCheckForAnotherSession = false;
    m_PendingHostPhysical = false;
    m_TimeReturnedToSinglePlayer = 0;

#if RSG_PC
	// Set the true DInputCount
	CheckDinputCount();

	// Set the next time to check
	m_dinputCheckTime.Set(sysTimer::GetSystemMsTime());
#endif
	return true;
}

void CNetworkSession::Shutdown(bool isSudden, eBailReason bailReasonIfSudden)
{
    gnetDebug1("Shutdown :: Initialised: %s, Sudden: %s, Bail Reason (If Sudden): %s", m_IsInitialised ? "true" : "false", isSudden ? "true" : "false", NetworkUtils::GetBailReasonAsString(bailReasonIfSudden));

	if(!m_IsInitialised)
		return;
	
#if RSG_DURANGO
    // we also need to correctly inform Xbox One services so that the events match up in the event of an error
    WriteMultiplayerEndEventSession(false OUTPUT_ONLY(, "Shutdown"));
	WriteMultiplayerEndEventTransition(false OUTPUT_ONLY(, "Shutdown"));

    // clean up the party if this is not a suspend
    if(!(isSudden && (bailReasonIfSudden == eBailReason::BAIL_SYSTEM_SUSPENDED)))
        RemoveFromParty(false);
#endif
	
#if !__NO_OUTPUT
	if(isSudden && (m_SessionState == SS_ESTABLISHING))
		DumpEstablishingStateInformation();
#endif

	//@@: location CNETWORKSESSION_SHUTDOWN_SESSION_REMOVE_PARTY_DELEGATE
	m_pSession->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Physical]);

	m_ConnectionDlgtGame.Shutdown();
	m_ConnectionDlgtGameSession.Shutdown();
    m_ConnectionDlgtActivitySession.Shutdown();

	for(int i = 0; i < SessionType::ST_Max; i++)
	{
		m_PeerComplainer[i].Shutdown();
		m_Config[i].Clear();
		m_nHostFlags[i] = HostFlags::HF_Default;
		m_PendingReservations[i].Reset();
		m_PendingJoiners[i].Reset();
	}

    m_pSession->Shutdown(isSudden, bailReasonIfSudden);

    m_RecentPlayers.ClearRecentPlayers();
    m_BlacklistedGamers.ClearBlacklist();
    m_InvitedPlayers.ClearAllInvites();
	m_BlacklistedSessions.Reset();

	if(m_pSessionStatus->Pending())
    {
        gnetDebug1("Shutdown :: Session status is pending. Cancelling");
		rlGetTaskManager()->CancelTask(m_pSessionStatus);
    }
    m_pSessionStatus->Reset();

    for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].IsBusy())
    {
        gnetDebug1("Shutdown :: Matchmaking status %d is pending. Cancelling", i);
        netTask::Cancel(&m_MatchMakingQueries[i].m_TaskStatus);
        m_MatchMakingQueries[i].Reset(static_cast<MatchmakingQueryType>(i));
    }

	m_bChangeAttributesPending[SessionType::ST_Physical] = false;

    SetSessionState(SS_IDLE);

	m_SessionStateTime = 0;
	m_bIsJoiningViaMatchmaking = false;
    m_GameMMResults.m_nCandidateToJoinIndex = 0;
    m_LastUpdateTime = 0;
    m_IsInitialised = false;
	m_TimeSessionStarted = 0;
	m_nSessionTimeout = 0;
	m_bScriptValidatingJoin = false;
	m_bScriptHandlingClockAndWeather = false;
	m_bIsMigrating = false;
	m_nBailReason = BAIL_INVALID;
    m_bJoinedViaInvite = false;
    m_nGamersToGame = 0;
	m_Inviter.Clear();
	m_bFreeroamIsJobToJob = false;

	m_nEnterMultiplayerTimestamp = 0;
	m_MigrationStartedTimestamp = 0;
	m_MatchmakingInfoMine.ClearSession();

    m_bHasSetActivitySpectatorsMax = false;
    m_bIsActivitySession = false;
	m_bIsLeavingLaunchedActivity = false;

    m_bMainSyncedRadio = false;
    m_bTransitionSyncedRadio = false;

	m_bRunningDetailQuery = false;
    m_JoinFailureAction = JoinFailureAction::JFA_None;
	m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Invalid;
    m_nJoinSessionDetail = 0;

    if(m_JoinDetailStatus.Pending())
    {
        gnetDebug1("Shutdown :: Join detail status is pending. Cancelling");
        rlSessionManager::Cancel(&m_JoinDetailStatus);
    }
    m_JoinDetailStatus.Reset();

	// shutdown transition
	m_pTransition->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Transition]);
	m_pTransition->Shutdown(isSudden, bailReasonIfSudden);

	if(m_pTransitionStatus->Pending())
    {
        gnetDebug1("Shutdown :: Transition detail status is pending. Cancelling");
		rlGetTaskManager()->CancelTask(m_pTransitionStatus);
    }
	m_pTransitionStatus->Reset();

	SetTransitionState(TS_IDLE);
	m_bChangeAttributesPending[SessionType::ST_Transition] = false;
	m_nLastTransitionMessageTime = 0;
	//@@: location CNETWORKSESSION_SHUTDOWN_NEGATE_BOOLEANS
	m_bIsTransitionToGame = false;
	m_bIsTransitionToGameInSession = false;
	m_bIsTransitionToGameNotifiedSwap = false;
    m_bIsTransitionToGamePendingLeave = false;
	m_bIsTransitionToGameFromLobby = false;
	m_bIsTransitionMatchmaking = false;
	m_hTransitionHostGamer.Clear();
	m_bLaunchingTransition = false;
	m_bTransitionStartPending = false;
	m_bDoLaunchPending = false;
	m_bLaunchOnStartedFromJoin = false;
	m_bDoNotLaunchFromJoinAsMigratedHost = false;
    m_TimeTokenForLaunchedTransition = 0;
	
    m_nGamersToActivity = 0;
    m_bIsActivityGroupQuickmatch = false;
	m_bIsActivityGroupQuickmatchAsync = false;
	m_ActivityGroupSession.Clear();
	
	m_hToGameManager.Clear();
	m_hActivityQuickmatchManager.Clear();

	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].IsBusy())
	{
		gnetDebug1("Shutdown :: Activity matchmaking status %d is pending. Cancelling", i);
		netTask::Cancel(&m_ActivityMMQ[i].m_TaskStatus);
		m_ActivityMMQ[i].Reset(MatchmakingQueryType::MQT_Standard);
	}

    m_bJoinedTransitionViaInvite = false;
	m_nTransitionInviteFlags = 0;
    m_TransitionInviter.Clear();
	m_bIsTransitionAsyncWait = false;

    // wipe user data structures
    m_SessionUserDataFreeroam.Clear();
    m_SessionUserDataActivity.Clear();

	// clear out transition player tracker
	m_TransitionPlayers.Reset();

	// shutdown presence
#if RSG_PRESENCE_SESSION
	m_PresenceSession.Shutdown(isSudden, bailReasonIfSudden);
	if(m_PresenceStatus.Pending())
		rlGetTaskManager()->CancelTask(&m_PresenceStatus);
    m_PresenceStatus.Reset();
    m_nPresenceState = PS_IDLE;
    m_bPresenceSessionInvitesBlocked = false;
	m_bRequirePresenceSession = false;
	m_bPresenceSessionFailed = false;
#else
	m_pSessionModifyPresenceStatus.Reset();
#endif

#if RSG_NP || __STEAM_BUILD
    // do we need to clear our presence blob?
    if(m_SessionInfoBlob.IsValid())
    {
        rlPresence::SetBlob(NULL, 0);
        m_SessionInfoBlob.Clear();
    }
#elif RSG_DURANGO
	m_bHandlePartyActivityDialog = false;
	m_bShowingPartyActivityDialog = false;
	m_bMigrationSentMultiplayerEndEvent = false;
	m_bMigrationSentMultiplayerEndEventTransition = false;
#endif

    m_DelayedPresenceInfo.Reset();

	m_bForceScriptPresenceUpdate = false;
    m_bCheckForAnotherSession = false;

	m_LastCheckForJoinRequests = 0;
}

void CNetworkSession::OpenNetwork()
{
    gnetDebug1("OpenNetwork");
	//@@: location CNETWORK_SESSION_OPEN_NETWORK
    CNetwork::OpenNetwork();
}

void CNetworkSession::CloseNetwork(bool bFullClose)
{
    gnetDebug1("CloseNetwork :: FullClose: %s", bFullClose ? "True" : "False");
	//@@: location CNETWORK_SESSION_CLOSE_NETWORK
    CNetwork::CloseNetwork(bFullClose);
}

void CNetworkSession::OnBail(const sBailParameters bailParams)
{
	gnetDebug1("OnBail :: %s (%s), Context: %d, %d, %d", 
		NetworkUtils::GetBailReasonAsString(bailParams.m_BailReason), 
		NetworkUtils::GetBailErrorCodeAsString(bailParams.m_ErrorCode),
		bailParams.m_ContextParam1, 
		bailParams.m_ContextParam2,
		bailParams.m_ContextParam3);

	// use a time relative to our context
	unsigned nTime = 0;
	if(m_SessionState >= SS_ESTABLISHED)
		nTime = GetTimeInSession();
	else if(m_SessionState >= SS_FINDING)
	{
		if(m_bIsJoiningViaMatchmaking)
			nTime = m_nMatchmakingStarted;
		else
			nTime = m_SessionStateTime;
	}

	// bail telemetry
	CNetworkTelemetry::NetworkBail(bailParams,
								   m_SessionState, 
								   m_pSession->Exists() ? m_pSession->GetSessionId() : 0, 
								   m_pSession->Exists() ? m_pSession->IsHost() : false, 
								   m_bIsJoiningViaMatchmaking, 
								   m_bJoinedViaInvite, 
								   nTime);
}

void CNetworkSession::OnStallDetected(unsigned nStallLength)
{
	gnetDebug1("OnStallDetected :: %u", nStallLength);

	// record for data mine
	m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nStallCount);
	if(nStallLength > 1000)
		m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nStallCountOver1s);
}

void CNetworkSession::OnReturnToSinglePlayer()
{
    gnetDebug1("OnReturnToSinglePlayer");
    m_TimeReturnedToSinglePlayer = sysTimer::GetSystemMsTime() | 0x01;
}

void CNetworkSession::OnServiceEventSuspend()
{
	// these need to be send on suspend in the event that we don't resume
	DURANGO_ONLY(WriteMultiplayerEndEventSession(false OUTPUT_ONLY(, "OnSuspend")));
	DURANGO_ONLY(WriteMultiplayerEndEventTransition(false OUTPUT_ONLY(, "OnSuspend")));
}

bool CNetworkSession::DropSession()
{
	unsigned nChannelID = m_pSession->GetChannelId();
	gnetDebug1("DropSession :: Using %s", NetworkUtils::GetChannelAsString(nChannelID));

	// we also need to correctly inform Xbox One services so that the events match up in the event of an error
	DURANGO_ONLY(WriteMultiplayerEndEventSession(false OUTPUT_ONLY(, "DropSession")));

	// drop and shutdown current session
	m_pSession->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Physical]);

	m_PeerComplainer[SessionType::ST_Physical].Shutdown();
	m_pSession->Shutdown(true, eBailReason::BAIL_INVALID);

	// reset transition session tracking
	m_bIsTransitionToGameInSession = false;

	if(m_pSessionStatus->Pending())
	{
		gnetDebug1("DropSession :: Session status is pending. Cancelling");
		rlGetTaskManager()->CancelTask(m_pSessionStatus);
	}
        
	m_pSessionStatus->Reset();

	// cancel matchmaking queries
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].IsBusy())
	{
		gnetDebug1("DropSession :: Matchmaking status %d is pending. Cancelling", i);
		netTask::Cancel(&m_MatchMakingQueries[i].m_TaskStatus);
		m_MatchMakingQueries[i].Reset(static_cast<MatchmakingQueryType>(i));
	}

	// initialise anew
	bool bSuccess = m_pSession->Init(CLiveManager::GetRlineAllocator(),
								     m_SessionOwner[SessionType::ST_Physical],
								     &CNetwork::GetConnectionManager(),
								     nChannelID,
								     "gta5");

	// apply attribute names
	//@@: location CNETWORKSESSION_DROPSESSION_SETATTRIBUTE_NAMES
	m_pSession->SetPresenceAttributeNames(rlScAttributeId::GameSessionToken.Name,
										  rlScAttributeId::GameSessionId.Name,
										  rlScAttributeId::GameSessionInfo.Name,
										  rlScAttributeId::IsGameHost.Name,
										  rlScAttributeId::IsGameJoinable.Name,
										  false);

	if(!gnetVerify(bSuccess))
	{
		gnetError("DropSession :: Error initialising session");
		return false; 
	}

	// clear any pending reservations
	m_PendingReservations[SessionType::ST_Physical].Reset();

	m_pSession->AddDelegate(&m_SessionDlgt[SessionType::ST_Physical]);

	return true;
}

void CNetworkSession::Update()
{
	const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
	bool bIsActive = IsSessionActive();

	if(m_bAutoJoining)
		ProcessAutoJoin();

    // need to keep this updated to maintain shutdown / cancel
    m_pSession->Update(nCurrentTime);

    // if active, maintain the migrating state
    if(bIsActive)
	    m_bIsMigrating = m_pSession->IsMigrating();

	// tracking transitions into / out of multiplayer
	static bool s_bWasActive = false; 
	bool isAnyActive = NetworkInterface::IsAnySessionActive();
	static bool s_bWasSolo = false;
	bool bIsSolo = IsSoloMultiplayer();

#if !__NO_OUTPUT
	// track these states
	if(isAnyActive != s_bWasActive)
		gnetDebug1("%s Multiplayer", isAnyActive ? "Entering" : "Exiting");
	if(bIsSolo != s_bWasSolo)
		gnetDebug1("%s Solo Multiplayer", bIsSolo ? "Entering" : "Exiting");
#endif

	if(isAnyActive && !s_bWasActive)
	{
		m_TimeReturnedToSinglePlayer = 0;
	}
	else if(s_bWasActive && !isAnyActive)
	{	

	}
	s_bWasSolo = bIsSolo;
	s_bWasActive = isAnyActive;
	
    // update invited player tracking
	m_InvitedPlayers.Update();
	m_TransitionInvitedPlayers.Update();
    
    // update blacklisted gamers
    m_BlacklistedGamers.Update();

    // update gamers in store
    int nGamersInStore = m_GamerInStore.GetCount();
    for(int i = 0; i < nGamersInStore; i++)
    {
        const GamerInStore& tGamer = m_GamerInStore[i];
        if((nCurrentTime - tGamer.m_nReservationTime) > tGamer.m_nTimeEnteredStore)
        {
            gnetDebug2("Update :: Gamer in store %s has not returned within reservation time. Removing.", NetworkUtils::LogGamerHandle(tGamer.m_hGamer));
            
            // generate script event and remove from tracking
            GetEventScriptNetworkGroup()->Add(CEventNetworkStorePlayerLeft(tGamer.m_hGamer, tGamer.m_szGamerName));
            
            // remove gamer entry and decrement counter / number
            m_GamerInStore.Delete(i);
            --i;
            --nGamersInStore;
        }
    }

	// update peer complainers
	for(int i = 0; i < SessionType::ST_Max; i++)
	{
		// only update if we've flagged that we're ready (when the session state is started)
		if(IsReadyForComplaints(static_cast<SessionType>(i)))
			m_PeerComplainer[i].Update();
	}

    // check pending host physical flag
    if(m_PendingHostPhysical)
    {
        netPlayer* pHostPlayer = GetHostPlayer();
        if(pHostPlayer && pHostPlayer->HasValidPhysicalPlayerIndex())
        {
            gnetDebug2("Update :: Clearing pending host physical flag");
            NetworkInterface::NewHost();
            m_PendingHostPhysical = false; 
        }
    }

    // manage check for another session
    if(m_bCheckForAnotherSession)
    {
        // be very specific about testing this again for the exact conditions
        if(!IsSessionEstablished())
        {
            // if matchmaking, and we have more results
            if(m_bIsJoiningViaMatchmaking && ((m_GameMMResults.m_nCandidateToJoinIndex + 1) < m_GameMMResults.m_nNumCandidates))
            {
                gnetDebug1("Update :: Processing check for another session. Matchmaking.");
                CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_CHECK_MATCHMAKING);
            }
            else if(m_JoinFailureAction == JoinFailureAction::JFA_Quickmatch)
            {
                gnetDebug1("Update :: Processing check for another session. Join Failure Action.");
                CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_CHECK_JOIN_ACTION);
            }
            else
            {
                gnetDebug1("Update :: Clearing check for another session. Not actionable");
                m_bCheckForAnotherSession = false;
            }
        }
    }

	// manage session state
    switch(m_SessionState)
    {
    case SS_IDLE:
        break;

    case SS_FINDING:
        ProcessFindingState();
        break;

	case SS_HOSTING:
		ProcessHostingState();
		break;

    case SS_JOINING:
        ProcessJoiningState();
        break;

	case SS_ESTABLISHING:
		ProcessEstablishingState();
		break;

	case SS_ESTABLISHED:
		break;

	case SS_HANDLE_KICK:
		ProcessHandleKickState();
		break;

    case SS_LEAVING:
        if(m_pSessionStatus->Succeeded())
        {
            Destroy();
        }
        else if(!m_pSessionStatus->Pending())
        {
            // failed to leave
            gnetError("Update :: LeaveSession failed. Rline: %" I64FMT "u.", CLiveManager::GetRlineAllocator()->GetMemoryAvailable());
            DropSession();
			ClearSession(m_LeaveSessionFlags, m_LeaveReason);
		}	
        else if(!m_pSession->IsMigrating())
        {
#if !__FINAL
            if(CNetwork::GetTimeoutTime() > 0)
#endif
            {
                // still pending - bail if this is taking too long
                const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
                if(nCurrentTime - m_SessionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_Ending])
				{
					gnetError("Update :: Leaving Timed out [%d/%dms]", nCurrentTime - m_SessionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_Ending]);
					DropSession();
					ClearSession(m_LeaveSessionFlags, m_LeaveReason);
				}
            }
        }
        break;

    case SS_DESTROYING:
        if(!m_pSessionStatus->Pending())
        {
			if(!m_pSessionStatus->Succeeded())
			{
				// failed to leave
				gnetError("Update :: Destroy failed. Dropping.");
				DropSession();
			}
			
			// clear out session parameters
			ClearSession(m_LeaveSessionFlags, m_LeaveReason);
		}
		else if(!m_pSession->IsMigrating())
		{
#if !__FINAL
			if(CNetwork::GetTimeoutTime() > 0)
#endif
			{
				// still pending - bail if this is taking too long
				const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
				if(nCurrentTime - m_SessionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_Ending])
				{
					gnetError("Update :: Destroying Timed out [%d/%dms]", nCurrentTime - m_SessionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_Ending]);
					DropSession();
					ClearSession(m_LeaveSessionFlags, m_LeaveReason);
				}
			}
		}
		break;
        break;

	case SS_BAIL:
		// make sure a reason was given
		gnetAssertf(m_nBailReason != BAIL_INVALID, "Bailing in Session :: Invalid Reason!");

		// if matchmaking, and we have more results
		if(m_bCheckForAnotherSessionOnMigrationFail)
			CheckForAnotherSessionOrBail(m_nBailReason, BAIL_CTX_NONE);
		else
			CNetwork::Bail(sBailParameters(m_nBailReason, BAIL_CTX_NONE));

		// reset bail state and set session to idle
		m_nBailReason = BAIL_INVALID;
		break;

    default:
		gnetAssertf(0, "Update :: Invalid network session state!");
        break;
    }

	// manage attribute changes
	if(m_bChangeAttributesPending[SessionType::ST_Physical])
	{
        if(m_pSession->IsHost() && !IsSessionLeaving())
        {
		    if(!m_ChangeAttributesStatus[SessionType::ST_Physical].Pending())
		    {
			    gnetDebug2("Update :: Processing Attribute Change");
			    m_pSession->ChangeAttributes(m_Config[SessionType::ST_Physical].GetMatchingAttributes(), &m_ChangeAttributesStatus[SessionType::ST_Physical]);
			    m_bChangeAttributesPending[SessionType::ST_Physical] = false;
		    }
        }
        else // the host will send any attribute changes through
            m_bChangeAttributesPending[SessionType::ST_Physical] = false;
	}

	// invite to action
	if(m_bIsInviteProcessPendingForCommerce)
	{
        // check we have full use of the network heap
        if(IsNetworkMemoryFree())
        {
			gnetDebug1("Update :: Network Heap Free. Processing Invite.");
			ActionInvite();
			m_bIsInviteProcessPendingForCommerce = false;
		}
	}

    // pending invite completion
    if(m_bIsInvitePendingJoin)
    {
        if(CLiveManager::GetInviteMgr().HasConfirmedAcceptedInvite())
        {
            gnetError("Update :: Joining invite via wait.");
            JoinInvite();
        }
        else if(!gnetVerify(CLiveManager::GetInviteMgr().HasPendingAcceptedInvite()))
        {
            gnetError("Update :: Invite cleared while pending!");
            m_bIsInvitePendingJoin = false;

            // an attempt at a fallback
            if(!DoJoinFailureAction())
            {
                gnetDebug1("Update :: Failed to action join failure.");
                CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_PENDING_INVITE));
            }
        }
    }

	// process detail query
	if(m_bRunningDetailQuery)
	{
		// if the task is no longer pending
		if(!m_JoinDetailStatus.Pending())
		{
			// check we have a valid result
            bool bActioned = false;
			if(m_nJoinSessionDetail > 0 && m_JoinDetailStatus.Succeeded())
			{
				const rlSessionConfig& config = m_JoinSessionDetail.m_SessionConfig;

				// logging
				gnetDebug1("DetailQuery :: Retrieved session detail!"); 
				gnetDebug2("  Token            = Token: 0x%016" I64FMT "x", m_JoinSessionDetail.m_SessionInfo.GetToken().m_Value);
				gnetDebug2("  Host             = %s", m_JoinSessionDetail.m_HostName);
				gnetDebug2("  Game Mode        = %d", config.m_Attrs.GetGameMode());
                gnetDebug2("  Session Type     = %s", NetworkUtils::GetSessionPurposeAsString(static_cast<SessionPurpose>(config.m_Attrs.GetSessionPurpose())));

				const u32* pDisc = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_DISCRIMINATOR);
                const u32* pAimValue = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_AIM_TYPE);
                
                // verify that the query was successful
                if(gnetVerifyf(pDisc, "DetailQuery :: No discriminator!") && gnetVerifyf(pAimValue, "DetailQuery :: No aim setting!"))
                {
				    m_JoinDetailStatus.Reset();
                    
                    // shorter retry timeout - we just heard from this session
                    SetSessionPolicies(POLICY_AGGRESSIVE);
					SetSessionTimeout(TimeoutSetting::Timeout_DirectJoin);

                    // detail retrieved, valid, take action
                    bActioned = true;

				    // join the correct session type
				    if(config.m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Freeroam)
					    JoinSession(m_JoinSessionInfo, m_JoinSlotType, m_nJoinFlags);
				    else
					    JoinTransition(m_JoinSessionInfo, m_JoinSlotType, m_nJoinFlags);
                }
			}

            // do join failure action if we haven't made it
			if(!bActioned)
			    DoJoinFailureAction();

			// reset flag
			m_bRunningDetailQuery = false;
		}
	}

	// update transitioning to a concrete session
	UpdateTransitionToGame(); 

	// update user data
    UpdateUserData();

    // matchmaking groups 
    if(IsSessionActive())
    {
        // this will force an update when we become active
        // the timestamp is reset in the function to account for forced calls at certain times
        if(nCurrentTime > (m_nLastMatchmakingGroupCheck + MATCHMAKING_GROUP_CHECK_INTERVAL))
            UpdateMatchmakingGroups();
    }

    // activity slots
    if(IsSessionActive() || IsTransitionActive())
    {
        // this will force an update when we become active
        // the timestamp is reset in the function to account for forced calls at certain times
        if(nCurrentTime > (m_nLastActivitySlotsCheck + ACTIVITY_SLOT_CHECK_INTERVAL))
            UpdateActivitySlots();
    }

	// pending reservations / joins
	for(unsigned i = 0; i < SessionType::ST_Max; i++)
	{
		snSession* pSession = GetSessionPtr(static_cast<SessionType>(i));

		// for pending reservations, wait until we have an established session and then try to reserve slots
		if(m_PendingReservations[i].m_nGamers > 0 && (pSession && pSession->IsEstablished()))
		{
			// try private first and then public if that fails
			bool bReservedSlot = pSession->ReserveSlots(m_PendingReservations[i].m_hGamers, m_PendingReservations[i].m_nGamers, RL_SLOT_PRIVATE, m_PendingReservations[i].m_nReservationTime, false, RESERVE_PLAYER);
			if(!bReservedSlot)
				bReservedSlot = pSession->ReserveSlots(m_PendingReservations[i].m_hGamers, m_PendingReservations[i].m_nGamers, RL_SLOT_PUBLIC, m_PendingReservations[i].m_nReservationTime, false, RESERVE_PLAYER);

			gnetDebug1("Cleared pending reservations - Success: %s", bReservedSlot ? "True" : "False");
			m_PendingReservations[i].Reset();
		}

		// for pending joins, we just want to make sure that we time anyone out who hasn't managed to join
		int numPendingJoiners = m_PendingJoiners[i].m_Joiners.GetCount();
		for(unsigned j = 0; j < numPendingJoiners; j++)
		{
			if((sysTimer::GetSystemMsTime() - m_PendingJoiners[i].m_Joiners[j].m_JoinRequestTime) > m_PendingJoinerTimeout)
			{
				gnetDebug1("Cleared timed out pending joiner - Gamer: %s", NetworkUtils::LogGamerHandle(m_PendingJoiners[i].m_Joiners[j].m_hGamer));
				m_PendingJoiners[i].m_Joiners.Delete(j);
				numPendingJoiners--;
				j--;
			}
		}
	}

	UpdateTransition();

#if RSG_DURANGO
	HandlePartyActivityDialog();
#endif

	UpdateQueuedJoinRequests();

#if RSG_PRESENCE_SESSION
	UpdatePresenceSession();
#endif

#if RSG_NP || __STEAM_BUILD
	UpdatePresenceBlob();
#endif

    m_LastUpdateTime = nCurrentTime;
}

void CNetworkSession::InitTunables()
{
    // setup any tunables
    s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_MATCHMAKING", 0x742d7afb), s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking]);
    s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingOneSession] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_MM_ONE_SESSION", 0x357de54c), s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingOneSession]);
    s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingMultipleSessions] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_MM_MULTIPLE_SESSIONS", 0x524be8cf), s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingMultipleSessions]);
	s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_DIRECT_JOIN", 0xea25ec70), s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);
	s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_JOIN", 0xcfcbcc24), s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin]);
	s_TimeoutValues[TimeoutSetting::Timeout_TransitionComplainInterval] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_COMPLAIN_INTERVAL", 0x983f3879), s_TimeoutValues[TimeoutSetting::Timeout_TransitionComplainInterval]);
	s_TimeoutValues[TimeoutSetting::Timeout_Hosting] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_HOSTING", 0xa268295d), s_TimeoutValues[TimeoutSetting::Timeout_Hosting]);
    s_TimeoutValues[TimeoutSetting::Timeout_Ending] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_ENDING", 0xffd74fe6), s_TimeoutValues[TimeoutSetting::Timeout_Ending]);
    s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingAttemptLong] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_MATCHMAKING_ATTEMPT_LONG", 0x5bf68a80), s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingAttemptLong]);
    s_TimeoutValues[TimeoutSetting::Timeout_QuickmatchLong] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_QUICKMATCH_LONG", 0xe65c3894), s_TimeoutValues[TimeoutSetting::Timeout_QuickmatchLong]);
    s_TimeoutValues[TimeoutSetting::Timeout_SessionBlacklist] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_SESSION_BLACKLIST", 0x8826056c), s_TimeoutValues[TimeoutSetting::Timeout_SessionBlacklist]);
    s_TimeoutValues[TimeoutSetting::Timeout_TransitionToGame] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_TO_GAME", 0x5eab310e), s_TimeoutValues[TimeoutSetting::Timeout_TransitionToGame]);
	s_TimeoutValues[TimeoutSetting::Timeout_TransitionPendingLeave] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_PENDING_LEAVE", 0x91df0b1a), s_TimeoutValues[TimeoutSetting::Timeout_TransitionPendingLeave]);
	s_TimeoutValues[TimeoutSetting::Timeout_TransitionWaitForUpdate] = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_WAIT_FOR_UPDATE", 0x502887e3), s_TimeoutValues[TimeoutSetting::Timeout_TransitionWaitForUpdate]);

	int nNetworkTimeout = static_cast<int>(CNetwork::GetTimeoutTime());

	// these are based on network timeout, or tunable if specified
	bool bTunableFound = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_LAUNCH", 0xbbc65c9c), s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch]);
	if(!bTunableFound)
		s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch] = nNetworkTimeout > 0 ? Max(nNetworkTimeout, s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch]) : 0;
	bTunableFound = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_TIME_TRANSITION_KICK", 0x5a43d7e5), s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick]);
	if(!bTunableFound)
		s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick] = nNetworkTimeout > 0 ? Max(nNetworkTimeout, s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick]) : 0;

	// needs to be < TimeoutSetting::Timeout_TransitionLaunch
	static const int MIN_TRANSITION_LAUNCH_KICK_DIFF = 5000;
	s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick] = Min(s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick], s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch] - MIN_TRANSITION_LAUNCH_KICK_DIFF);

	// starting
	m_bPreventStartWithTemporaryPlayers = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_PREVENT_START_WITH_TEMP_PLAYERS", 0x7d8c9b0d), m_bPreventStartWithTemporaryPlayers);
	m_bPreventStartWhileAddingPlayers = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_PREVENT_START_WHILE_ADDING_PLAYERS", 0x9647bd8b), m_bPreventStartWhileAddingPlayers);
	
	// migration timeout
	m_fMigrationTimeoutMultiplier = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TIMEOUT_MULTIPLIER", 0x419ecdd8), DEFAULT_MIGRATION_TIMEOUT_MULTIPLIER);

	// allow launch from join
	m_bAllowImmediateLaunchDuringJoin = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_IMMEDIATE_TRANSITION_LAUNCH", 0xfe1321f8), m_bAllowImmediateLaunchDuringJoin);
	m_nImmediateLaunchCountdownTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("IMMEDIATE_LAUNCH_COUNTDOWN_TIME", 0x2fed8dcd), static_cast<int>(DEFAULT_IMMEDIATE_LAUNCH_COUNTDOWN));
	m_bAllowBailForLaunchedJoins = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_BAIL_FOR_LAUNCHED_JOINS", 0x926d2561), m_bAllowBailForLaunchedJoins);

	// transition to game
	m_bTransitionToGameRemovePhysicalPlayers = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TRANSITION_TO_GAME_REMOVE_PHYSICAL_PLAYERS", 0x5cc1baae), m_bTransitionToGameRemovePhysicalPlayers);

	// peer complainer
	bool bUseTimeoutRatio = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_BOOT_DELAY_USE_TIMEOUT", 0x903aaee9), true);
	float fTimeoutRatio = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_BOOT_DELAY_TIMEOUT_RATIO", 0x7ef5be9f), 0.5f);
	unsigned nPeerComplainerBootGraceTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_BOOT_DELAY_GRACE_TIME", 0xae944d5d), static_cast<int>(DEFAULT_PEER_COMPLAINER_GRACE_TIME));
	m_nPeerComplainerBootDelay = (bUseTimeoutRatio ? static_cast<unsigned>((fTimeoutRatio * static_cast<float>(CNetwork::GetTimeoutTime()))) : 0) + nPeerComplainerBootGraceTime;
	m_nPeerComplainerAggressiveBootDelay = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_AGGRESSIVE_BOOT_TIME", 0x466dbf7b), static_cast<int>(DEFAULT_PEER_COMPLAINER_AGGRESSIVE_BOOT_TIME));
	m_bAllowComplaintsWhenEstablishing = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_ALLOW_ESTABLISHING_COMPLAINTS", 0x6e5fa4dd), m_bAllowComplaintsWhenEstablishing);
	m_bPreferToBootEstablishingCandidates = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_PREFER_TO_BOOT_ESTABLISHING_CANDIDATES", 0x467ee0fe), m_bPreferToBootEstablishingCandidates);
	m_bPreferToBootLowestNAT = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PEER_COMPLAINER_PREFER_TO_BOOT_LOWEST_NAT", 0x06bb8567), m_bPreferToBootLowestNAT);

#if !__FINAL
	if(PARAM_netSessionDisableImmediateLaunch.Get())
		m_bAllowImmediateLaunchDuringJoin = false;
#endif

    // setup host probability
    m_bEnableHostProbability = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROBABILITY_ENABLED", 0xd2f7914a), true);
    m_MaxMatchmakingAttempts = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROBABILITY_MAX_ATTEMPTS", 0x0ff8672e), kMaxSearchAttempts);
    m_fMatchmakingHostProbabilityStart = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROBABILITY_START", 0xe7b7ec5e), kHostProbabilityStart);
    m_fMatchmakingHostProbabilityScale = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROBABILITY_SCALE", 0x8499e9b3), kHostProbabilityScale);

    // matchmaking
	m_nLargeCrewValue = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MATCHMAKING_LARGE_CREW_VALUE", 0xf36534ef), static_cast<int>(DEFAULT_LARGE_CREW_VALUE));
    m_nBeaconInterval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MATCHMAKING_BEACON_INTERVAL", 0xe4b48ae4), DEFAULT_BEACON_INTERVAL);
    m_bUseMatchmakingBeacon = !Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MATCHMAKING_BEACON_DISABLE", 0xc357c07b), false);

	const unsigned DEFAULT_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT = 6;
	const unsigned DEFAULT_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT_NON_SOLO = 4;
	const unsigned DEFAULT_NUM_QUICKMATCH_SOCIAL_QUERY_RESULTS_BEFORE_EARLY_OUT = 4;
	const unsigned DEFAULT_NUM_ACTIVITY_OVERALL_RESULTS_BEFORE_EARLY_OUT = 6;
	
	// sets the number of successful tunnel/host query results we want before we early out of the rlSessionQueryDetailTask
	m_nNumQuickmatchOverallResultsBeforeEarlyOut = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT", 0xcc14d9d9), DEFAULT_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT);
	m_nNumQuickmatchOverallResultsBeforeEarlyOutNonSolo = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT_NON_SOLO", 0x52cc5272), DEFAULT_NUM_QUICKMATCH_OVERALL_RESULTS_BEFORE_EARLY_OUT_NON_SOLO);
	m_nNumQuickmatchResultsBeforeEarlyOut = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_QUICKMATCH_RESULTS_BEFORE_EARLY_OUT", 0xf7c93b25), 0);
	m_nNumQuickmatchSocialResultsBeforeEarlyOut = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_QUICKMATCH_SOCIAL_QUERY_RESULTS_BEFORE_EARLY_OUT", 0xad5cb126), DEFAULT_NUM_QUICKMATCH_SOCIAL_QUERY_RESULTS_BEFORE_EARLY_OUT);
	m_nNumSocialResultsBeforeEarlyOut = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_SOCIAL_RESULTS_BEFORE_EARLY_OUT", 0xe59f02e5), 0);

	m_nNumActivityResultsBeforeEarlyOut = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_ACTIVITY_OVERALL_RESULTS_BEFORE_EARLY_OUT", 0x2008eed3), DEFAULT_NUM_ACTIVITY_OVERALL_RESULTS_BEFORE_EARLY_OUT);
	m_nNumActivityOverallResultsBeforeEarlyOutNonSolo = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_NUM_ACTIVITY_OVERALL_RESULTS_BEFORE_EARLY_OUT_NON_SOLO", 0x3dcc210d), 0);
	m_bActivityMatchmakingDisableAnyJobQueries = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_ACTIVITY_DISABLE_ANY_JOB_QUERIES", 0xbf7a93aa), m_bActivityMatchmakingDisableAnyJobQueries);
	m_bActivityMatchmakingDisableInProgressQueries = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_ACTIVITY_DISABLE_IN_PROGRESS_QUERIES", 0x2ee42087), m_bActivityMatchmakingDisableInProgressQueries);
	m_bActivityMatchmakingContentFilterDisabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_ACTIVITY_DISABLE_CONTENT_FILTERING", 0x5aac752d), m_bActivityMatchmakingContentFilterDisabled);
	m_bActivityMatchmakingActivityIslandDisabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_ACTIVITY_DISABLE_ACTIVITY_ISLAND_FILTERING", 0x3a0247ff), m_bActivityMatchmakingActivityIslandDisabled);

	// matchmaking 
	m_bDisableDelayedMatchmakingAdvertise = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_DISABLE_DELAYED_ADVERTISE", 0x90e95ddd), m_bDisableDelayedMatchmakingAdvertise);

	// session
	m_bCancelAllTasksOnLeave = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_CANCEL_ALL_TASKS_ON_LEAVE", 0xb65d35ca), m_bCancelAllTasksOnLeave);
	m_bCancelAllConcurrentTasksOnLeave = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_CANCEL_ALL_CONCURRENT_TASKS_ON_LEAVE", 0x18d9ea0e), m_bCancelAllConcurrentTasksOnLeave);

	// migration
	m_bCheckForAnotherSessionOnMigrationFail = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_CHECK_FOR_ANOTHER_SESSION_ON_MIGRATION_FAIL", 0x83b08f53), m_bCheckForAnotherSessionOnMigrationFail);

	// boss matchmaking thresholds
	m_nBossRejectThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_REJECT_THRESHOLD", 0x7e317eff), static_cast<int>(DEFAULT_BOSS_REJECT_THRESHOLD));
	m_nBossDeprioritiseThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_DEPRIORITISE_THRESHOLD", 0x65df9828), static_cast<int>(DEFAULT_BOSS_DEPRIORITISE_THRESHOLD));
	m_bRejectSessionsWithLargeNumberOfBosses = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_REJECT_SESSIONS", 0xc170df99), false);

	// boss matchmaking thresholds for the expanded intro flow
	m_IntroBossRejectThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_REJECT_THRESHOLD_INTRO", 0xf415b03e), static_cast<int>(DEFAULT_BOSS_REJECT_THRESHOLD_INTRO_FLOW));
	m_IntroBossDeprioritiseThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_DEPRIORITISE_THRESHOLD_INTRO", 0x54cb0cea), static_cast<int>(DEFAULT_BOSS_DEPRIORITISE_THRESHOLD));
	m_IntroRejectSessionsWithLargeNumberOfBosses = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_BOSS_REJECT_SESSIONS_INTRO", 0xd248f1ee), true);

	// pending joiner
	m_PendingJoinerTimeout = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_PENDING_JOINER_TIMEOUT", 0x1b17bc83), DEFAULT_PENDING_JOINER_TIMEOUT);
	m_PendingJoinerEnabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_PENDING_JOINER_ENABLED", 0x0423baaf), true);

#if !__FINAL
	if(PARAM_netSessionEnableActivityMatchmakingAnyJobQueries.Get())
		m_bActivityMatchmakingDisableAnyJobQueries = false;
	if(PARAM_netSessionEnableActivityMatchmakingInProgressQueries.Get())
		m_bActivityMatchmakingDisableInProgressQueries = false;
	if(PARAM_netSessionEnableActivityMatchmakingContentFilter.Get())
		m_bActivityMatchmakingContentFilterDisabled = false;
	if(PARAM_netSessionEnableActivityMatchmakingActivityIsland.Get())
		m_bActivityMatchmakingActivityIslandDisabled = false;
#endif

	// migration
	m_bMigrationSessionNoEstablishedPools = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_SESSION_NO_ESTABLISHED_POOLS", 0xd5b1d0eb), false);
	m_bMigrationTransitionNoEstablishedPools = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TRANSITION_NO_ESTABLISHED_POOLS", 0x8285077e), false);
	m_bMigrationTransitionNoEstablishedPoolsForToGame = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TRANSITION_NO_ESTABLISHED_POOLS_TO_GAME", 0x0a1aef83), false);
	m_bMigrationTransitionNoSpectatorPools = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TRANSITION_NO_SPECTATOR_POOLS", 0x82a0065f), false);
	m_bMigrationTransitionNoSpectatorPoolsForToGame = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TRANSITION_NO_SPECTATOR_POOLS_TO_GAME", 0xef42ab04), false);
	m_bMigrationTransitionNoAsyncPools = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MIGRATION_TRANSITION_NO_ASYNC_POOLS", 0x40077aa4), false);

    m_nLanguageBiasMask = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LANGUAGE_BIAS_MASK", 0xf99c2499), 0);
    m_nRegionBiasMask = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_BIAS_MASK", 0xf8894a4c), 0);
    m_nLanguageBiasMaskForRegion = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LANGUAGE_BIAS_MASK_FOR_REGION", 0xc8413f65), 0);
    m_nRegionBiasMaskForLanguage = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_BIAS_MASK_FOR_LANGUAGE", 0x502b6244), 0);
    m_bEnableLanguageBias = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LANGUAGE_BIAS", 0xa0b7e8b7), false);
    m_bEnableRegionBias = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_BIAS", 0xe6b586f5), true);

	m_bUseRegionMatchmakingForToGame = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_REGION_FOR_TO_GAME", 0xef2d86ca), true);
	m_bUseLanguageMatchmakingForToGame = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_LANGUAGE_FOR_TO_GAME", 0xbbb44193), false);
	m_bUseRegionMatchmakingForActivity = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_REGION_FOR_ACTIVITY", 0x68d912b1), true);
	m_bUseLanguageMatchmakingForActivity = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_LANGUAGE_FOR_ACTIVITY", 0x72c1398f), false);

	// join queue
	m_QueuedInviteTimeout = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("JOIN_QUEUE_INVITE_TIMEOUT", 0xbcac6eea), static_cast<int>(JOIN_QUEUE_INVITE_TIMEOUT));
	m_MaxWaitForQueuedInvite = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("JOIN_QUEUE_MAX_WAIT_QUEUED_INVITE", 0x8e901923), static_cast<int>(JOIN_QUEUE_MAX_WAIT_QUEUED_INVITE));
	m_JoinQueueInviteCooldown = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("JOIN_QUEUE_INVITE_COOLDOWN", 0x5388d39a), static_cast<int>(JOIN_QUEUE_INVITE_COOLDOWN));

	// admin invites
	m_AdminInviteNotificationRequired = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ADMIN_INVITE_NOTIFICATION_REQUIRED", 0x17bfcd48), false);
	m_AdminInviteKickToMakeRoom = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ADMIN_INVITE_KICK_TO_MAKE_ROOM", 0x927dd51f), false);
	m_AdminInviteKickIncludesLocal = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ADMIN_INVITE_KICK_INCLUDES_LOCAL", 0xf939e556), false);

	// blacklist
	m_bUseBlacklist = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_USE_BLACKLIST", 0x258d029a), m_bUseBlacklist);

	// friends
	m_bImmediateFriendSessionRefresh = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_IMMEDIATE_FRIEND_REFRESH", 0x57681C9E), m_bImmediateFriendSessionRefresh);

	// kick
	m_bSendRemovedFromSessionKickTelemetry = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_REMOVED_FROM_SESSION_KICK_TELEMETRY", 0x53c857af), m_bSendRemovedFromSessionKickTelemetry);
	
	// this is a per player sample (not a per occurrence sample) so that we can detect patterns on a player basis
	if(m_bSendRemovedFromSessionKickTelemetry)
	{
		const float kickTelemetrySampleRate = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_KICK_TELEMETRY_SAMPLE_RATE", 0x4c94e315), 1.0f);
		m_bSendRemovedFromSessionKickTelemetry = gs_Random.GetFloat() <= kickTelemetrySampleRate;
		gnetDebug1("InitTunables :: Send Kick Telemetry: %s", m_bSendRemovedFromSessionKickTelemetry ? "True" : "False");
	}

	m_bKickWhenRemovedFromSession = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("KICK_WHEN_REMOVED_FROM_SESSION", 0xabc3e138), m_bKickWhenRemovedFromSession);
	m_bSendHostDisconnectMsg = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_HOST_DISCONNECT_MSG", 0xa04a33d3), m_bSendHostDisconnectMsg);

	// migration
	m_SendMigrationTelemetry = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_MIGRATION_TELEMETRY", 0x34e793a0), m_SendMigrationTelemetry);

	// invites
	m_bPreventClearTransitionWhenJoiningInvite = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PREVENT_CLEAR_TRANSITION_WHEN_JOINING_INVITE", 0x4c39b660), m_bPreventClearTransitionWhenJoiningInvite);
	m_bAllAdminInvitesTreatedAsDev = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("INVITE_ALL_ADMIN_INVITES_TREATED_AS_DEV", 0xa606a9e0), m_bAllAdminInvitesTreatedAsDev);
	m_bNoAdminInvitesTreatedAsDev = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("INVITE_NO_ADMIN_INVITES_TREATED_AS_DEV", 0xb33656fb), m_bNoAdminInvitesTreatedAsDev);
	m_bAllowAdminInvitesToSolo = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("INVITE_ALLOW_ADMIN_INVITES_TO_SOLO", 0x65769863), m_bAllowAdminInvitesToSolo);

	// bubble 
	m_nPostMigrationBubbleCheckTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BUBBLE_POST_MIGRATION_CHECK_TIME", 0xaf864fdb), m_nPostMigrationBubbleCheckTime);
	bool bNeverCheckBubbleRequirement = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BUBBLE_NEVER_CHECK_IF_REQUIRED", 0x072afa80), false);
	bool bAlwaysCheckBubbleRequirement = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BUBBLE_ALWAYS_CHECK_IF_REQUIRED", 0x361575ea), false);

#if !__FINAL
	bAlwaysCheckBubbleRequirement &= PARAM_netBubbleAlwaysCheckIfRequired.Get();
#endif

	CRoamingBubbleMgr& bubbleMgr = CNetwork::GetRoamingBubbleMgr();
	bubbleMgr.SetNeverCheckBubbleRequirement(bNeverCheckBubbleRequirement);
	bubbleMgr.SetAlwaysCheckBubbleRequirement(bAlwaysCheckBubbleRequirement);

#if !__FINAL
    m_bEnableHostProbability &= PARAM_netSessionHostProbabilityEnable.Get();
    PARAM_netSessionHostProbabilityMax.Get(m_MaxMatchmakingAttempts);
    PARAM_netSessionHostProbabilityStart.Get(m_fMatchmakingHostProbabilityStart);
    PARAM_netSessionHostProbabilityScale.Get(m_fMatchmakingHostProbabilityScale);
#endif

#if RL_NP_SUPPORT_PLAY_TOGETHER
	// play together
	m_bUsePlayTogetherOnFilter = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_PLAY_TOGETHER_ON_FILTER", 0x4b6863c6), m_bUsePlayTogetherOnFilter);
	m_bUsePlayTogetherOnJoin = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_PLAY_TOGETHER_ON_JOIN", 0xceb9122e), m_bUsePlayTogetherOnJoin);
#endif

#if RSG_DURANGO
	m_bValidateGameSession = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_VALIDATE_PARTY_SESSION", 0xf94ec34e), true);
#endif
	
	// set stall timeout
	bool bUseSessionStallRemoval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_SESSION_STALL_REMOVAL", 0x8cc9c6ab), true);
	if(bUseSessionStallRemoval)
		snSession::SetSelfRemoveStallTime(CNetwork::GetTimeoutTime());

	bool bConnectToPeerChecks = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SN_SESSION_CONNECT_TO_PEER_CHECKS", 0x9DB3E21C), true);
	snSession::SetConnectToPeerChecks(bConnectToPeerChecks);

#if RSG_DURANGO
	// tasks
	int nNotifyDisplayNameTimeout = snMigrateSessionTask::DEFAULT_REQUEST_INTERVAL; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_NOTIFY_DISPLAY_NAME_TIMEOUT", 0x366ef8c1), nNotifyDisplayNameTimeout))
		snNotifyAddGamerTask::SetTimeoutPolicy(static_cast<unsigned>(nNotifyDisplayNameTimeout));
#endif
}

void CNetworkSession::OnCredentialsResult(bool UNUSED_PARAM(bSuccess))
{

}

void CNetworkSession::UpdateTransitionToGame()
{
	// current system time
	unsigned nCurrentTime = sysTimer::GetSystemMsTime();
	
	// catch edge cases in transition to game where the host fails to move us back to freeroam promptly
	// or a migrated host encounters an error
	// do not do this if we are the session host as script pick this up in that case
	bool bValidFlowState = IsActivitySession() && !IsSessionLeaving() && !IsTransitionLeaving() && IsSessionActive();
	if(m_hTransitionHostGamer.IsValid() && !IsHost() && bValidFlowState && ((nCurrentTime - m_nToGameTrackingTimestamp) > s_TimeoutValues[TimeoutSetting::Timeout_TransitionToGame]))
	{
		gnetDebug1("UpdateTransitionToGame :: Transition to game timed out [%d/%dms]. Taking Control.", (nCurrentTime - m_nToGameTrackingTimestamp), s_TimeoutValues[TimeoutSetting::Timeout_TransitionToGame]);

		m_bIsTransitionToGame = false;
		m_bIsTransitionToGamePendingLeave = false;
		m_hTransitionHostGamer.Clear();

		// going to freemode - we always want these
		SetMatchmakingGroup(MM_GROUP_FREEMODER);
		AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);
		AddActiveMatchmakingGroup(MM_GROUP_SCTV);

		// call up a transition to game
		DoTransitionToGame(m_TransitionToGameGameMode, nullptr, 0, true, MAX_NUM_PHYSICAL_PLAYERS, 0);
	}

	// manage leaving a transition session
	if(m_bIsTransitionToGame && m_bIsTransitionToGamePendingLeave && IsTransitionEstablished())
	{
		bool bTimeUp = GetTimeInSession() > s_TimeoutValues[TimeoutSetting::Timeout_TransitionPendingLeave];
		bool bAllTransitioned = true;
		if(m_nGamersToGame > 0)
		{
			for(unsigned i = 0; i < m_nGamersToGame; i++)
			{
				if(!m_pSession->IsMember(m_hGamersToGame[i]))
				{
					bool bIsMember = false;
#if !__NO_OUTPUT
					rlGamerInfo hInfo;
					bIsMember = m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
#else
					bIsMember = m_pTransition->IsMember(m_hGamersToGame[i]);
#endif
					// is this player still a member of the transition session
					if(bIsMember)
					{
#if !__NO_OUTPUT
						if (bTimeUp) // log when time is up so that we know who the stragglers are
							gnetDebug1("UpdateTransitionToGame :: %s has not transitioned yet.", hInfo.GetName());
#endif
						bAllTransitioned = false;
					}
				}
			}
		}

#if !__NO_OUTPUT
		if(!bAllTransitioned && bTimeUp)
			gnetDebug1("UpdateTransitionToGame :: Forcing leave, not all players transitioned");
#endif

		if((bAllTransitioned || bTimeUp) && !m_bIsMigrating)
		{
			m_nGamersToGame = 0;
			m_bIsTransitionToGamePendingLeave = false;

			gnetDebug1("UpdateTransitionToGame :: Fully completed transition to game");
			LeaveTransition();

			m_bIsTransitionToGame = false;
			m_nToGameCompletedTimestamp = sysTimer::GetSystemMsTime();
		}
	}
}

void CNetworkSession::UpdateUserData()
{
    // current system time
    unsigned nCurrentTime = sysTimer::GetSystemMsTime();

    // update beacon values
    if(m_bUseMatchmakingBeacon)
    {
        // we don't need this for launched activities
        if(m_pSession->Exists() && !IsActivitySession())
        {
            if((nCurrentTime - m_nBeaconInterval) > m_nMatchmakingBeaconTimer[SessionType::ST_Physical])
            {
                m_nMatchmakingBeacon[SessionType::ST_Physical]++;
                m_nMatchmakingBeaconTimer[SessionType::ST_Physical] = nCurrentTime;
            }
        }
        else
        {
            m_nMatchmakingBeacon[SessionType::ST_Physical] = 0;
            m_nMatchmakingBeaconTimer[SessionType::ST_Physical] = nCurrentTime;
        }

        if(m_pTransition->Exists())
        {
            if((nCurrentTime - m_nBeaconInterval) > m_nMatchmakingBeaconTimer[SessionType::ST_Transition])
            {
                m_nMatchmakingBeacon[SessionType::ST_Transition]++;
                m_nMatchmakingBeaconTimer[SessionType::ST_Transition] = 0;
            }
        }
        else
        {
            m_nMatchmakingBeacon[SessionType::ST_Transition] = 0;
            m_nMatchmakingBeaconTimer[SessionType::ST_Transition] = nCurrentTime;
        }
    }

    // should we refresh...
    bool bRefreshUserData = (nCurrentTime > (m_nLastSessionUserDataRefresh + SESSION_USERDATA_CHECK_INTERVAL));

    // if time to refresh or data is dirty
    if(bRefreshUserData || m_HasDirtySessionUserDataFreeroam)
    {
        // session user data - only run this as host
        if(!IsActivitySession() && m_pSession->Exists())
        {
            float fScale = 0.0f;
            Vector3 vAveragePosition(VEC3_ZERO);

            // reset transition player tracking
            m_SessionUserDataFreeroam.m_nTransitionPlayers = 0;

            // grab from all players
            unsigned nCharacterRankTotal = 0;
            unsigned nPlayers = 0;

			u32 averageMentalState = 0;
			WIN32PC_ONLY(u32 averagePedDensity = 0;)

			m_SessionUserDataFreeroam.m_nAverageMentalState = 0;
			WIN32PC_ONLY(m_SessionUserDataFreeroam.m_nAveragePedDensity = 0;)

            for(unsigned i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; i++)
            {
                const CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(i));
                if(pPlayer)
                {
                    // grab position
                    Vector3 vPlayerPos = NetworkUtils::GetNetworkPlayerPosition(*pPlayer);

                    // players running around have more weight than those in coronas
                    float fWeight = 1.0f;
                    if(!pPlayer->HasStartedTransition())
                    {
                        if(!Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_POSITION_CORONA_WEIGHT", 0x9fb96395), fWeight))
                            fWeight = 1.5f; 
                    }
                    else
                        m_SessionUserDataFreeroam.m_nTransitionPlayers++;

                    // add to running tallies
                    fScale += fWeight;
                    vAveragePosition += (vPlayerPos * fWeight);

                    // add rank to running tally
                    nCharacterRankTotal += pPlayer->GetCharacterRank();
                    nPlayers++;

                    // grab the current property ID
                    m_SessionUserDataFreeroam.m_nPropertyID[i] = pPlayer->GetPropertyID();
					
					// grab the current mental state
					averageMentalState += (u32)pPlayer->GetMentalState()*g_MentalStateScale;

					WIN32PC_ONLY(averagePedDensity += (u32) pPlayer->GetPedDensity();)
                }
                else 
				{
                    m_SessionUserDataFreeroam.m_nPropertyID[i] = NO_PROPERTY_ID;
				}
            }

			// work out average mental state
			if (averageMentalState > 0)
			{
				float fAverageMentalState = static_cast<float>(averageMentalState) / static_cast<float>(nPlayers);
				gnetAssert(fAverageMentalState < (float)(sizeof(m_SessionUserDataFreeroam.m_nAverageMentalState)<<8));
				m_SessionUserDataFreeroam.m_nAverageMentalState = static_cast<u8>(fAverageMentalState);
			}
#if RSG_PC
			// work out average ped density
			if (averagePedDensity > 0)
			{
				float fAveragePedDensity = static_cast<float>(averagePedDensity) / static_cast<float>(nPlayers);
				gnetAssert(fAveragePedDensity < (float)(sizeof(m_SessionUserDataFreeroam.m_nAveragePedDensity)<<8));
				m_SessionUserDataFreeroam.m_nAveragePedDensity = static_cast<u8>(fAveragePedDensity);
			}
#endif

            // flags
            m_SessionUserDataFreeroam.m_nFlags = 0;
            if(IsHost())
            {
                if(IsClosedFriendsSession(SessionType::ST_Physical))
                    m_SessionUserDataFreeroam.m_nFlags |= SessionUserData_Freeroam::USERDATA_FLAG_CLOSED_FRIENDS;
                if(IsClosedCrewSession(SessionType::ST_Physical))
                    m_SessionUserDataFreeroam.m_nFlags |= SessionUserData_Freeroam::USERDATA_FLAG_CLOSED_CREW;

                for(int i = 0; i < MAX_UNIQUE_CREWS; i++)
                    m_SessionUserDataFreeroam.m_nCrewID[i] = static_cast<s32>(m_UniqueCrewData[SessionType::ST_Physical].m_nActiveCrewIDs[i]);
            }

            // work out average character rank
            m_SessionUserDataFreeroam.m_nAverageCharacterRank = static_cast<u16>(static_cast<float>(nCharacterRankTotal) / static_cast<float>(nPlayers));

            // store real player aim (necessary due to having players with GTA and assisted in the same pool)
            m_SessionUserDataFreeroam.m_nPlayerAim = static_cast<u8>(CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG));

            // apply beacon value
            m_SessionUserDataFreeroam.m_nBeacon = static_cast<u8>(m_nMatchmakingBeacon[SessionType::ST_Physical]);

            // calculate average position
            vAveragePosition /= fScale;
            m_SessionUserDataFreeroam.m_ux = static_cast<u16>(vAveragePosition.x);
            m_SessionUserDataFreeroam.m_uy = static_cast<u16>(vAveragePosition.y);
            m_SessionUserDataFreeroam.m_uz = static_cast<u16>(vAveragePosition.z);

            // flag for update
            m_HasDirtySessionUserDataFreeroam = true;
        }

        // maintain the local player copy of rank
        if(NetworkInterface::IsNetworkOpen())
        {
            CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
            if(pLocalPlayer)
                pLocalPlayer->SetCharacterRank(static_cast<u16>(StatsInterface::GetIntStat(STAT_RANK_FM.GetStatId())));
        }
    }

    if(bRefreshUserData || m_HasDirtySessionUserDataActivity)
    {
        // session user data - only run this as host
        if((m_pTransition->Exists() && !m_bIsTransitionToGame) || (IsActivitySession() && m_pSession->Exists()))
        {
            // total character attributes for matchmaking
            unsigned nCharacterRankTotal = 0;
            unsigned nCharacterELOTotal = 0;
            unsigned nPlayers = m_TransitionPlayers.GetCount();
            for(unsigned i = 0; i < nPlayers; i++)
            {
                nCharacterRankTotal += m_TransitionPlayers[i].m_nCharacterRank;
                nCharacterELOTotal += m_TransitionPlayers[i].m_nELO;
            }

            // grab averages
            m_SessionUserDataActivity.m_nAverageCharacterRank = static_cast<u16>(static_cast<float>(nCharacterRankTotal) / static_cast<float>(nPlayers));
            m_SessionUserDataActivity.m_nAverageELO = static_cast<u16>(static_cast<float>(nCharacterELOTotal) / static_cast<float>(nPlayers));

            // store real player aim (necessary due to having players with GTA and assisted in the same pool)
            m_SessionUserDataActivity.m_nPlayerAim = static_cast<u8>(CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG));

			// minimum rank
			m_SessionUserDataActivity.m_nMinimumRank = m_nMatchmakingMinimumRank;

            // apply beacon value
            m_SessionUserDataActivity.m_nBeacon = static_cast<u8>(m_nMatchmakingBeacon[SessionType::ST_Transition]);
        
            // flag for refresh
            m_HasDirtySessionUserDataActivity = true;
        }

        // convenient place for the transition gamer data update
        if(NetworkInterface::IsLocalPlayerSignedIn() && NetworkInterface::IsNetworkOpen())
            UpdateTransitionGamerData();
    }

    // reset time
    if(bRefreshUserData)
        m_nLastSessionUserDataRefresh = nCurrentTime;

    if(m_HasDirtySessionUserDataFreeroam)
    {
        if(!IsActivitySession() && m_pSession->Exists())
            m_pSession->SetSessionUserData(reinterpret_cast<u8*>(&m_SessionUserDataFreeroam), sizeof(SessionUserData_Freeroam));

        m_HasDirtySessionUserDataFreeroam = false;
    }

    // manage user data
    if(m_HasDirtySessionUserDataActivity)
    {
#if RSG_OUTPUT
		// track changes to the activity data content creator
		static rlGamerHandle s_ContentCreator; 
		if(!s_ContentCreator.Equals(m_SessionUserDataActivity.m_hContentCreator, true))
		{
			gnetDebug1("UpdateUserData :: Activity - Applying ContentCreator changes (%s -> %s)",
				s_ContentCreator.ToString(),
				m_SessionUserDataActivity.m_hContentCreator.ToString());

			s_ContentCreator = m_SessionUserDataActivity.m_hContentCreator;
		}
#endif
        if(m_pTransition->Exists() && !m_bIsTransitionToGame)
            m_pTransition->SetSessionUserData(reinterpret_cast<u8*>(&m_SessionUserDataActivity), sizeof(SessionUserData_Activity));
        if(IsActivitySession() && m_pSession->Exists())
            m_pSession->SetSessionUserData(reinterpret_cast<u8*>(&m_SessionUserDataActivity), sizeof(SessionUserData_Activity));

        m_HasDirtySessionUserDataActivity = false;
    }
}

void CNetworkSession::ProcessAutoJoin()
{
	if(!IsSessionActive())
	{
		// wait until session state has settled
		if(CGame::IsChangingSessionState())
		{
			gnetDebug2("ProcessAutoJoin :: Changing Session State");
			return;
		}
		// wait until we have a local player
		if(!CGameWorld::FindLocalPlayer())
		{
			gnetDebug2("ProcessAutoJoin :: No Local Player");
			return;
		}
		// wait until we are flagged as online
		if(!NetworkInterface::IsLocalPlayerOnline())
		{
			gnetDebug2("ProcessAutoJoin :: Not Online");
			return;
		}
		// wait until we have cloud tunables
		if(!PARAM_netSessionIgnoreDataHash.Get())
		{
			if(NetworkInterface::IsCloudAvailable() && (!Tunables::GetInstance().HasCloudRequestFinished()))
			{
				gnetDebug2("ProcessAutoJoin :: Waiting for Tunables");
				return;
			}
		}
		// wait for background scripts
		if(NetworkInterface::IsCloudAvailable() && (!SBackgroundScripts::GetInstance().HasCloudScripts()))
		{
			gnetDebug2("ProcessAutoJoin :: Waiting for BackgroundScripts");
			return;
		}

		// if the pause menu is open, close it
		if(CPauseMenu::IsActive())
			CPauseMenu::Close();

		AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);
		SetMatchmakingGroup(MM_GROUP_FREEMODER);

		// Do different things based on the command line parameters set.
		if (PARAM_netAutoForceHost.Get()) 
		{
			// Don't look for a match and instead go straight to hosting.
			gnetVerify(HostSession(MultiplayerGameMode::GameMode_TestBed, MAX_NUM_PHYSICAL_PLAYERS, HostFlags::HF_Default));
		}
		else if (PARAM_netAutoForceJoin.Get()) 
		{
			// Looking for a match endlessly (don't send a script event if it doesn't find anything):
			gnetVerify(DoFreeroamMatchmaking(MultiplayerGameMode::GameMode_TestBed, SCHEMA_GROUPS, 
				                             MAX_NUM_PHYSICAL_PLAYERS, 
				                             MatchmakingFlags::MMF_AllowBlacklisted | MatchmakingFlags::MMF_NoScriptEventOnBail));
		}
		else
		{
			// Just do as normal and look for a quick match once:
			gnetVerify(DoQuickMatch(MultiplayerGameMode::GameMode_TestBed, SCHEMA_GROUPS, MAX_NUM_PHYSICAL_PLAYERS, true));
		}		
	}
	else if(IsSessionEstablished())
	{
		const strLocalIndex nScriptID = g_StreamedScripts.FindSlot(gs_szNetTestBedScript);
		if(gnetVerifyf(nScriptID != -1, "ProcessAutoJoin :: Script doesn't exist \"%s\"", gs_szNetTestBedScript))
		{
			g_StreamedScripts.StreamingRequest(nScriptID, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
			CStreaming::LoadAllRequestedObjects();

			if(gnetVerifyf(g_StreamedScripts.HasObjectLoaded(nScriptID), "ProcessAutoJoin :: Failed to stream in script \"%s\"", gs_szNetTestBedScript))
			{
				NET_ASSERTS_ONLY(scrThreadId nThreadId = )
					CTheScripts::GtaStartNewThreadOverride(gs_szNetTestBedScript, NULL, 0, GtaThread::GetMissionStackSize());
				gnetAssert(nThreadId != 0);
			}
		}

		// we're finished
		m_bAutoJoining = false;
	}
}

void CNetworkSession::SetSessionState(const eSessionState nState, bool bSkipChangeLogic)
{
	if(nState != m_SessionState)
	{
#if !__NO_OUTPUT
		gnetDebug1("SetSessionState :: Next: %s, Prev: %s, TimeInState: %u, SkipChangeLogic: %s", GetSessionStateAsString(nState), GetSessionStateAsString(), (m_SessionStateTime > 0) ? (sysTimer::GetSystemMsTime() | 0x01) - m_SessionStateTime : 0, bSkipChangeLogic ? "True" : "False");
#if __DEV
		if(PARAM_netSessionShowStateChanges.Get())
			CMessages::AddMessage(GetSessionStateAsString(), TheText.GetBlockContainingLastReturnedString(), 4000, false, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
#endif // __DEV
#endif  //__NO_OUTPUT

		// mark when we entered this state
		m_SessionStateTime = sysTimer::GetSystemMsTime() | 0x01;

		if(!bSkipChangeLogic)
		{
			switch(nState)
			{
			case SS_IDLE:
				if(!IsTransitioning())
					m_Config[SessionType::ST_Physical].Clear();

				m_SessionFilter.Clear();
				m_bIsMigrating = false;
				break;

			case SS_FINDING:
				{
					// shorter retry policy
					SetSessionPolicies(POLICY_AGGRESSIVE);
					m_nMatchmakingStarted = sysTimer::GetSystemMsTime();

					// send telemetry for matchmaking start
					m_nUniqueMatchmakingID[SessionType::ST_Physical] = SendMatchmakingStartTelemetry(m_MatchMakingQueries, MatchmakingQueryType::MQT_Num);
				}
				break; 

			case SS_ESTABLISHING:
				break;

			case SS_ESTABLISHED:
				{
					//Reset and refresh number of friends in match we are starting.
					CLiveManager::RefreshFriendCountInMatch();
					//Reset the time match started
					m_TimeSessionStarted = sysTimer::GetSystemMsTime();
					//Register Network match Match Started
					StatsInterface::IncrementStat(STAT_MP_SESSIONS_STARTED, 1.0f);

					// reset joining flags
					m_bIsJoiningViaMatchmaking = false;

					// check if any players were disconnected during the transition
					unsigned numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
					netPlayer* const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

					for(unsigned index = 0; index < numPhysicalPlayers; index++)
					{
						CNetGamePlayer *pPhysicalPlayer = SafeCast(CNetGamePlayer, allPhysicalPlayers[index]);
						if(pPhysicalPlayer && pPhysicalPlayer->WasDisconnectedDuringTransition())
						{
							if(m_pSession->GetPeerCxn(pPhysicalPlayer->GetRlPeerId()) == NET_INVALID_CXN_ID)
							{
								gnetDebug1("SetSessionState :: SS_ESTABLISHED - %s disconnected during transition and not in session - Sending complaint", pPhysicalPlayer->GetLogName());
								TryPeerComplaint(pPhysicalPlayer->GetRlPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical);
							}
							pPhysicalPlayer->SetDisconnectedDuringTransition(false);
						}
					}

					// leave the transition session
					if(m_bIsTransitionToGame)
					{
						gnetDebug1("SetSessionState :: SS_ESTABLISHED :: Transitioning from activity.");

						if(IsTransitionEstablished())
						{
							// are we transferring with other players - if so, work out if they've all made it back yet
							bool bAllTransitioned = true;
							if(m_nGamersToGame > 0)
							{
								for(unsigned i = 0; i < m_nGamersToGame; i++)
								{	
									if(!m_pSession->IsMember(m_hGamersToGame[i]))
									{
										bool bIsMember = false;
#if !__NO_OUTPUT
										rlGamerInfo hInfo;
										bIsMember = m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
#else
										bIsMember = m_pTransition->IsMember(m_hGamersToGame[i]);
#endif
										// is this player still a member of the transition session
										if(bIsMember)
										{
											gnetDebug1("SetSessionState :: SS_ESTABLISHED :: %s has not transitioned yet.", hInfo.GetName());
											bAllTransitioned = false;
										}
									}
								}
							}

							// reset gamer tracking
							if(bAllTransitioned)
							{
								m_nGamersToGame = 0;

								gnetDebug1("SetSessionState :: SS_ESTABLISHED :: Fully completed transition to game");
								LeaveTransition();

								m_bIsTransitionToGame = false;
								m_nToGameCompletedTimestamp = sysTimer::GetSystemMsTime();
							}
							else
							{
								gnetDebug1("SetSessionState :: SS_ESTABLISHED :: Waiting on players. Postponing leave.");
								m_bIsTransitionToGamePendingLeave = true;
							}
						}
						else
						{
							m_bIsTransitionToGame = false;
							m_nToGameCompletedTimestamp = sysTimer::GetSystemMsTime();
						}
					}     

					m_hTransitionHostGamer.Clear();

					// reset join failure flag
					m_JoinFailureAction = JoinFailureAction::JFA_None;
					m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Invalid;
				}
				break;

			default:
				break;
			}
		}
		
		m_SessionState = nState;
	}
}

void CNetworkSession::SetMatchmakingGroup(eMatchmakingGroup nGroup)
{
    if(m_MatchmakingGroup == nGroup)
        return;

    gnetDebug3("SetMatchmakingGroup :: Matchmaking group set to %s!", nGroup == MM_GROUP_INVALID ? "MM_GROUP_INVALID" : gs_szMatchmakingGroupNames[nGroup]);
	m_MatchmakingGroup = nGroup;
    
    // if currently active - update the matchmaking groups
    if(IsSessionActive())
		//@@: location CNETWORKSESSION_SETMATCHMAKINGGROUP_UPDATE_GROUPS
        UpdateMatchmakingGroups();
}

eMatchmakingGroup CNetworkSession::GetMatchmakingGroup()
{
	return m_MatchmakingGroup;
}

void CNetworkSession::AddActiveMatchmakingGroup(eMatchmakingGroup nGroup)
{
	// this will be a verify / assert but CnC will break this limit at the moment
	if(m_nActiveMatchmakingGroups >= MAX_ACTIVE_MM_GROUPS)
		return;

	// if this matchmaking group already exists, don't add it
	if(GetActiveMatchmakingGroupIndex(nGroup) >= 0)
		return;

	// assign and increment active matchmaking groups
	gnetDebug3("AddActiveMatchmakingGroup :: Added matchmaking group %s!", gs_szMatchmakingGroupNames[nGroup]);
	m_ActiveMatchmakingGroups[m_nActiveMatchmakingGroups++] = nGroup;
}

void CNetworkSession::ClearActiveMatchmakingGroups()
{
	m_nActiveMatchmakingGroups = 0;
	for(int i = 0; i < MAX_ACTIVE_MM_GROUPS; i++)
		m_ActiveMatchmakingGroups[i] = MM_GROUP_INVALID;
}

void CNetworkSession::SetMatchmakingGroupMax(const eMatchmakingGroup nGroup, unsigned nMaximum)
{
	// verify that we have a valid group
	if(!gnetVerify(nGroup != MM_GROUP_INVALID))
	{
		gnetError("SetMatchmakingGroupMax :: Invalid group!");
		return; 
	}

#if !__FINAL
    // override with command line parameter
    unsigned nDebugOverride = 0; 
    if(PARAM_netSessionMaxPlayersFreemode.Get(nDebugOverride))
    {
        if(m_MatchmakingGroupMax[nGroup] != nDebugOverride)
        {
            gnetDebug3("SetMatchmakingGroupMax :: Setting maximum in matchmaking group %s to override %d!", gs_szMatchmakingGroupNames[nGroup], nDebugOverride);
            m_MatchmakingGroupMax[nGroup] = nDebugOverride;
        }
    }
    else
#endif
    {
        if(m_MatchmakingGroupMax[nGroup] != nMaximum)
        {
            gnetDebug3("SetMatchmakingGroupMax :: Setting maximum in matchmaking group %s to %d!", gs_szMatchmakingGroupNames[nGroup], nMaximum);
            m_MatchmakingGroupMax[nGroup] = nMaximum;
        }
    }
}

unsigned CNetworkSession::GetMatchmakingGroupMax(const eMatchmakingGroup nGroup) const
{
	return m_MatchmakingGroupMax[nGroup];
}

void CNetworkSession::SetUniqueCrewLimit(const SessionType sessionType, unsigned nUniqueCrewLimit)
{
    // grab the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

	// verify that we have a sensible value
	if(!gnetVerify(nUniqueCrewLimit <= MAX_UNIQUE_CREWS))
	{
        gnetError("SetUniqueCrewLimit :: %s :: Crew limit of %d exceeds maximum of %d!", NetworkUtils::GetSessionTypeAsString(sessionType), nUniqueCrewLimit, MAX_UNIQUE_CREWS);
		return; 
	}

    unsigned nLimit = 0;

#if !__FINAL
    // override with command line parameter
    if(PARAM_netSessionUniqueCrewLimit.Get(nLimit) && nLimit != pData->m_nLimit)
        gnetDebug3("SetUniqueCrewLimit :: %s :: Setting maximum unique crews to override %d!", NetworkUtils::GetSessionTypeAsString(sessionType), nLimit);
    else
#endif
    {
        nLimit = nUniqueCrewLimit;
        if(nLimit != pData->m_nLimit)
            gnetDebug3("SetUniqueCrewLimit :: %s :: Setting maximum unique crews to %d!", NetworkUtils::GetSessionTypeAsString(sessionType), nUniqueCrewLimit);
    }

    bool bChanged = (nLimit != pData->m_nLimit);

    // assign limit
    pData->m_nLimit = static_cast<u8>(nLimit);
    
    if(!pData->m_bOnlyCrews)
    {
        gnetDebug3("SetUniqueCrewLimit :: %s :: Enabling crew members only!", NetworkUtils::GetSessionTypeAsString(sessionType));
        pData->m_bOnlyCrews = true;
    }
  
    // need to update the crew numbers
    if(bChanged)
        UpdateCurrentUniqueCrews(sessionType);
}

void CNetworkSession::SetUniqueCrewOnlyCrews(const SessionType sessionType, bool bOnlyCrews)
{
    // grab the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

    if(pData->m_bOnlyCrews != bOnlyCrews)
    {
        gnetDebug3("SetUniqueCrewOnlyCrews :: %s :: %s!", NetworkUtils::GetSessionTypeAsString(sessionType), bOnlyCrews ? "Only crews" : "Anyone");
        pData->m_bOnlyCrews = bOnlyCrews;
    }
}

unsigned CNetworkSession::GetUniqueCrewLimit(const SessionType sessionType) const
{
	return m_UniqueCrewData[sessionType].m_nLimit;
}

void CNetworkSession::SetCrewLimitMaxMembers(const SessionType sessionType, unsigned nCrewLimitMaxMembers)
{
    // grab the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

    if(pData->m_nMaxCrewMembers != nCrewLimitMaxMembers)
    {
        gnetDebug3("SetCrewLimitMaxMembers :: %s :: Crews limited to %d members!", NetworkUtils::GetSessionTypeAsString(sessionType), nCrewLimitMaxMembers);
        pData->m_nMaxCrewMembers = static_cast<u8>(nCrewLimitMaxMembers);
    }

    // need to update the crew numbers
    UpdateCurrentUniqueCrews(sessionType);
}

void CNetworkSession::SetMatchmakingPropertyID(u8 nPropertyID)
{
    if(m_nMatchmakingPropertyID != nPropertyID)
    {
	    gnetDebug3("SetMatchmakingPropertyID :: Setting matchmaking property ID to %d!", nPropertyID);
		m_nMatchmakingPropertyID = nPropertyID;
	}
}

void CNetworkSession::SetMatchmakingMentalState(u8 nMentalState)
{
    if(m_nMatchmakingMentalState != nMentalState)
    {
        gnetDebug3("SetMatchmakingMentalState :: Setting matchmaking mental state to %d!", nMentalState);
		m_nMatchmakingMentalState = nMentalState;
	}
}

void CNetworkSession::SetMatchmakingMinimumRank(u32 nMinimumRank)
{
    if(m_nMatchmakingMinimumRank != nMinimumRank)
    {
	    gnetDebug3("SetMatchmakingMinimumRank :: Setting matchmaking minimum rank to %d!", nMinimumRank);
		m_nMatchmakingMinimumRank = nMinimumRank;
	}
}

void CNetworkSession::SetMatchmakingELO(u16 nELO)
{
    if(m_nMatchmakingELO != nELO)
    {
        gnetDebug3("SetMatchmakingELO :: Setting matchmaking ELO to %d!", nELO);
		m_nMatchmakingELO = nELO;
	}
}

void CNetworkSession::SetEnterMatchmakingAsBoss(const bool bEnterMatchmakingAsBoss)
{
	if(m_bEnterMatchmakingAsBoss != bEnterMatchmakingAsBoss)
	{
		gnetDebug1("SetEnterMatchmakingAsBoss :: %s Boss Matchmaking", bEnterMatchmakingAsBoss ? "Enabling" : "Disabling");
		m_bEnterMatchmakingAsBoss = bEnterMatchmakingAsBoss; 
	}
}

void CNetworkSession::SetMatchmakingQueryState(MatchmakingQueryType nQuery, bool bEnabled)
{
    m_MatchmakingQueryEnabled[nQuery] = bEnabled;
}

bool CNetworkSession::GetMatchmakingQueryState(MatchmakingQueryType nQuery)
{
    return m_MatchmakingQueryEnabled[nQuery];
}

#if RL_NP_SUPPORT_PLAY_TOGETHER
void CNetworkSession::RegisterPlayTogetherGroup(const rlGamerHandle* phGamers, unsigned nGamers)
{
	gnetDebug1("RegisterPlayTogetherGroup :: Adding %u players", nGamers);

	m_nPlayTogetherPlayers = nGamers;
	if(!gnetVerify(m_nPlayTogetherPlayers <= RL_MAX_PLAY_TOGETHER_GROUP))
		m_nPlayTogetherPlayers = RL_MAX_PLAY_TOGETHER_GROUP;

	for(unsigned i = 0; i < m_nPlayTogetherPlayers; i++)
		m_PlayTogetherPlayers[i] = phGamers[i];
}

void CNetworkSession::InvitePlayTogetherGroup()
{
	// early exit if we have no PlayTogether players
	if(m_nPlayTogetherPlayers == 0)
		return;

	gnetDebug1("InvitePlayTogetherGroup");

	// if we can send invites, do so now
	snSession* pPresenceSession = GetCurrentPresenceSession();
    if(pPresenceSession)
    {
        gnetDebug1("InvitePlayTogetherGroup :: SessionId: 0x%016" I64FMT "x", pPresenceSession->GetSessionToken().m_Value);
        if(pPresenceSession->IsInvitable())
        {
            // send invites to all play together players
            gnetDebug1("InvitePlayTogetherGroup :: Inviting %u players", m_nPlayTogetherPlayers);
            SendInvites(m_PlayTogetherPlayers, m_nPlayTogetherPlayers);
            m_nPlayTogetherPlayers = 0;
        }
        else
        {
            gnetDebug1("InvitePlayTogetherGroup :: Not invitable"); 
            gnetDebug1("InvitePlayTogetherGroup :: RlSession Invitable: %s", pPresenceSession->GetRlSession()->IsInvitable() ? "True" : "False");
            gnetDebug1("InvitePlayTogetherGroup :: Presence Enabled: %s", pPresenceSession->IsPresenceEnabled() ? "True" : "False");
            gnetDebug1("InvitePlayTogetherGroup :: Hosting: %s", pPresenceSession->IsHost() ? "True" : "False");
            gnetDebug1("InvitePlayTogetherGroup :: Accepting Join Requests: %s", pPresenceSession->IsAcceptingJoinRequests() ? "True" : "False");
            gnetDebug1("InvitePlayTogetherGroup :: Established: %s", pPresenceSession->IsEstablished() ? "True" : "False");
            gnetDebug1("InvitePlayTogetherGroup :: Slots: %u / %u", pPresenceSession->GetEmptySlots(RL_SLOT_PUBLIC), pPresenceSession->GetEmptySlots(RL_SLOT_PRIVATE));
            gnetDebug1("InvitePlayTogetherGroup :: Create Flags: 0x%x", pPresenceSession->GetRlSession()->GetCreateFlags());
        }
    }
	else
	{
		gnetDebug1("InvitePlayTogetherGroup :: Invalid presence session. Waiting...");
	}
}
#endif

void CNetworkSession::AddFollowers(rlGamerHandle* phGamers, unsigned nGamers)
{
    gnetDebug2("AddFollowers :: Adding %u followers", nGamers);

    // add followers
    unsigned nAdded = 0;
    for(unsigned i = 0; i < nGamers; i++)
    {
        bool bIsFollower = false;
        for(unsigned j = 0; j < m_nFollowers; j++)
        {
            if(m_hFollowers[j] == phGamers[i])
            {
                bIsFollower = true;
                break;
            }
        }

        if(!bIsFollower)
        {
			if(m_nFollowers < RL_MAX_GAMERS_PER_SESSION)
			{
				gnetDebug3("\tAddFollowers :: Adding %s", NetworkUtils::LogGamerHandle(phGamers[i]));
				m_hFollowers[m_nFollowers++] = phGamers[i];
				nAdded++;
			}
			OUTPUT_ONLY(else { gnetDebug3("\tAddFollowers :: Couldn't add %s", NetworkUtils::LogGamerHandle(phGamers[i])); })
        }
		else // count up followers who have already been added
			nAdded++;
    }

    // acknowledge any followers that we could not add
    if(!gnetVerify(nAdded == nGamers))
    {
        gnetError("AddFollowers :: Failed to add %d followers", nGamers - nAdded);
    }
}

void CNetworkSession::RemoveFollowers(rlGamerHandle* phGamers, unsigned nGamers)
{
    gnetDebug2("RemoveFollowers :: Removing %d followers", nGamers);

    for(unsigned i = 0; i < nGamers; i++)
    {
        for(unsigned j = 0; j < m_nFollowers; j++)
        {
            if(phGamers[i] == m_hFollowers[j])
            {
                gnetDebug3("\tRemoveFollowers :: Removing %s", NetworkUtils::LogGamerHandle(phGamers[i]));
                
                // remove (via swap with last follower) and fix counters
                m_hFollowers[j] = m_hFollowers[m_nFollowers - 1];
                --m_nFollowers;
                --j;
            }
        }
    }
}

bool CNetworkSession::HasFollower(const rlGamerHandle& hGamer)
{
    for(unsigned j = 0; j < m_nFollowers; j++)
    {
        if(hGamer == m_hFollowers[j])
            return true;
    }
    return false;
}

void CNetworkSession::ClearFollowers()
{
    if(m_nFollowers > 0)
    {
        gnetDebug2("ClearFollowers :: Clearing %d followers", m_nFollowers);
        m_nFollowers = 0;
        m_bRetainFollowers = false;
    }
}

void CNetworkSession::RetainFollowers(bool bRetain)
{
    if(m_bRetainFollowers != bRetain)
    {
        gnetDebug2("RetainFollowers :: Setting to %s", bRetain ? "True" : "False");
        m_bRetainFollowers = bRetain;
    }
}

bool CNetworkSession::CanEnterMultiplayer()
{
	// check thin data population is not in progress
    if(!IsNetworkMemoryFree())
    {
        gnetDebug2("CanEnterMultiplayer :: Cannot enter. Network Memory in use.");
        return false;
    }

    // do not enter while the tunables request is pending
    if(NetworkInterface::HasValidRosCredentials() && !Tunables::GetInstance().HasCloudRequestFinished())
    {
        gnetDebug2("CanEnterMultiplayer :: Cannot enter. Cloud tunables pending!.");
        return false; 
    }

	// check session thins now - are we in a session already
	if(IsSessionActive())
	{
		gnetDebug2("CanEnterMultiplayer :: Cannot enter. In an existing session! State: %s", GetSessionStateAsString());
		return false;
	}

	// done
	return true;
}

bool CNetworkSession::IsNetworkMemoryFree()
{
    // thin data is used to setup PS3 commerce system - this happens once when we detect an online connection
    // this allocates ~1MB from the network heap and initialising the network while this is happening 
    // will not end well. This will complete in enough time 99.9% of the time before entering multiplayer but 
    // the fatality resulting in these overlapping means we should guard against it

    // check thin data population is not in progress
    if(CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress())
        return false;

    // check thin data population has completed
    if(!CLiveManager::GetCommerceMgr().HasThinDataPopulationBeenCompleted())
        return false;

    // check that the single player save queue is empty
    if(CSavegameQueue::ContainsASinglePlayerSave())
        return false;

    // we have memory
    return true; 
}

bool CNetworkSession::VerifyMultiplayerAccess(const char* OUTPUT_ONLY(szFunction), const bool bCountLAN)
{
    if(!gnetVerify(NetworkInterface::IsLocalPlayerOnline(bCountLAN)))
    {
        gnetError("%s :: Local player not online!", szFunction);
        return false; 
    }

    if(!gnetVerify(NetworkInterface::CheckOnlinePrivileges()))
    {
		gnetError("%s :: Local player does not have multiplayer privileges!", szFunction);
		return false;
    }

    // check ROS privilege
    if(!gnetVerify(rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_MULTIPLAYER)))
    {
        gnetError("%s :: No ROS multiplayer privilege!", szFunction);
        return false; 
    }

    // checks passed
    return true; 
}

bool CNetworkSession::DoMatchmakingCommon(const int nGameMode, const int nSchemaID, unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	if(!gnetVerify(SS_FINDING != m_SessionState))
	{
		gnetError("DoMatchmakingCommon :: Invalid state!");
		return false; 
	}

    // check we have access via profile and privilege
	if(!VerifyMultiplayerAccess("DoMatchmakingCommon"))
        return false;

    // verify that we have multiplayer access
    if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
    {
        gnetError("DoMatchmakingCommon :: Cannot access multiplayer!");
        return false; 
    }

	if(!gnetVerify(m_MatchmakingGroup != MM_GROUP_INVALID))
	{
		gnetError("DoMatchmakingCommon :: Invalid matchmaking group!");
		return false; 
	}

	// verify max players
	if(!gnetVerify(nMaxPlayers > 0))
	{
		gnetError("DoMatchmakingCommon :: Invalid Max Players: %d. Must be at least 1!", nMaxPlayers);
		return false; 
	}

	// verify max players
	if(!gnetVerify(nMaxPlayers <= RL_MAX_GAMERS_PER_SESSION))
	{
		gnetError("DoMatchmakingCommon :: Invalid Max Players: %d. Cannot be more than %d!", nMaxPlayers, RL_MAX_GAMERS_PER_SESSION);
		nMaxPlayers = RL_MAX_GAMERS_PER_SESSION;
	}

	// verify commerce has finished with the network heap
	if(!IsNetworkMemoryFree())
	{
		gnetError("DoMatchmakingCommon :: Network Memory Unavailable! Call CanEnterMultiplayer!");
		return false; 
	}

	// check for command line override
	m_nQuickmatchFallbackMaxPlayers = nMaxPlayers;
	NOTFINAL_ONLY(PARAM_netSessionMaxPlayersFreemode.Get(m_nQuickmatchFallbackMaxPlayers));

	// find and join (hosting if this fails)
	// choose correct filter schema based on query ID
	NetworkGameFilter ngFilter;
	switch(nSchemaID)
	{
	case SCHEMA_GENERAL:
		{
			player_schema::MatchingFilter_General mmSchema;
			ngFilter.Reset(mmSchema);
		}
		break;
	case SCHEMA_GROUPS:
		{
			player_schema::MatchingFilter_Group mmSchema;
			ngFilter.Reset(mmSchema);

			// verify that we have some active matchmaking groups
			if(!gnetVerify(m_nActiveMatchmakingGroups > 0))
			{
				gnetError("DoMatchmakingCommon :: Using Group Schema with no active matchmaking groups!");
				return false; 
			}

			// grab the players matchmaking group - check that it's valid
			int nGroupIndex = GetActiveMatchmakingGroupIndex(m_MatchmakingGroup);
			if(!gnetVerify(nGroupIndex != -1))
			{
				gnetError("DoMatchmakingCommon :: Player matchmaking group not active!");
				return false; 
			}

			// 
			if(nGroupIndex != -1)
				ngFilter.SetGroupLimit(nGroupIndex, m_MatchmakingGroupMax[m_MatchmakingGroup]);
		}
		break;
	case SCHEMA_ACTIVITY:
		{
			player_schema::MatchingFilter_Activity mmSchema;
			ngFilter.Reset(mmSchema);
		}
		break;
	default:
		gnetAssertf(0, "DoMatchmakingCommon :: Invalid Schema ID!");
		return false;
	}

	// if we're entering as a boss, enable the flag
	// clear the flag immediately, this will need to be specified each time we enter matchmaking
	if(m_bEnterMatchmakingAsBoss)
	{
		matchmakingFlags |= MatchmakingFlags::MMF_IsBoss;
		m_bEnterMatchmakingAsBoss = false;
	}

	// log out flags and scheme type
	gnetDebug1("DoMatchmakingCommon :: Flags: 0x%x, GameMode: %s [%d], Using %s", matchmakingFlags, NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, gs_szSchemaNames[nSchemaID]);
	m_nMatchmakingFlags = matchmakingFlags;
    OUTPUT_ONLY(LogMatchmakingFlags(m_nMatchmakingFlags, __FUNCTION__));

	// apply region / language settings
	ApplyMatchmakingRegionAndLanguage(&ngFilter, true, true);

	// setup matchmaking filter
    ngFilter.SetGameMode(static_cast<u16>(nGameMode));
	ngFilter.SetSessionPurpose(SessionPurpose::SP_Freeroam);
	
	// setup probability matchmaking
	m_nMatchmakingAttempts = 1;
	//@@: location CNETWORKSESSION_DOMATCHMAKINGCOMMON_SETUP_PROBABILITY_MATCHMAKING
	m_fMatchmakingHostProbability = m_fMatchmakingHostProbabilityStart;

	// reset parameters
	m_bJoinedViaInvite = false;
	m_nGamersToGame = 0;
	m_nEnterMultiplayerTimestamp = sysTimer::GetSystemMsTime();

	// clear out joining problem sessions
	m_JoinFailureSessions.Reset(); 

	// clear out matchmaking queries
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
		m_MatchMakingQueries[i].Reset(static_cast<MatchmakingQueryType>(i));

	// apply 
	m_SessionFilter = ngFilter; 

	return true; 
}

void CNetworkSession::ApplyMatchmakingRegionAndLanguage(NetworkGameFilter* pFilter, const bool bUseRegion, const bool bUseLanguage)
{
	// apply matchmaking biases
	bool bEnableLanguageBias = m_bEnableLanguageBias && ((m_nLanguageBiasMask & (1 << NetworkBaseConfig::GetMatchmakingLanguage())) == 0) && ((m_nRegionBiasMaskForLanguage & (1 << NetworkBaseConfig::GetMatchmakingRegion())) == 0);
	bool bEnableRegionBias = m_bEnableRegionBias && ((m_nRegionBiasMask & (1 << NetworkBaseConfig::GetMatchmakingRegion())) == 0) && ((m_nLanguageBiasMaskForRegion & (1 << NetworkBaseConfig::GetMatchmakingLanguage())) == 0);
#if !__FINAL
	bEnableLanguageBias &= PARAM_netSessionEnableLanguageBias.Get();
	bEnableRegionBias &= PARAM_netSessionEnableRegionBias.Get();
#endif

	if(bUseLanguage && bEnableLanguageBias)
	{
		gnetDebug3("ApplyMatchmakingRegionAndLanguage :: Applying language bias");
		pFilter->ApplyLanguage(true);
	}

	//@@: range CNETWORKSESSION_DOMATCHMAKINGCOMMON_APPLY_REGION {
	if(bUseRegion && bEnableRegionBias)
	{
		gnetDebug3("ApplyMatchmakingRegionAndLanguage :: Applying region bias");
		pFilter->ApplyRegion(true);
	} 
	//@@: } CNETWORKSESSION_DOMATCHMAKINGCOMMON_APPLY_REGION
}

bool CNetworkSession::DoFreeroamMatchmaking(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, const unsigned matchmakingFlags)
{
	// log out matchmaking type
	gnetDebug1("DoFreeroamMatchmaking");

	// check we're not in a session already
	if(!gnetVerify(!IsSessionActive()))
	{
		gnetError("DoFreeroamMatchmaking :: Session already active!");
		return false; 
	}

	// common setup for matchmaking
	if(!DoMatchmakingCommon(nGameMode, nSchemaID, nMaxPlayers, matchmakingFlags))
		return false;

#if RL_NP_SUPPORT_PLAY_TOGETHER
	// play together players
	if(m_bUsePlayTogetherOnFilter && m_nPlayTogetherPlayers > 0)
	{
		// if we're matchmaking as a group
		int nGroupIndex = GetActiveMatchmakingGroupIndex(MM_GROUP_FREEMODER);
		if(nGroupIndex != -1)
		{
			// limit includes one player, if we're pulling everyone, make sure there's space for the group
			unsigned nLimit = m_MatchmakingGroupMax[MM_GROUP_FREEMODER] - m_nPlayTogetherPlayers;
			m_SessionFilter.SetGroupLimit(nGroupIndex, nLimit);
		}
	}
#endif

	// log out filter contents
	m_SessionFilter.DebugPrint();

	// we want to end up in a session
	m_nQuickMatchStarted = sysTimer::GetSystemMsTime();

	// 'regular' quick match calls friends, social and platform matchmaking
	bool bSuccess = true; 
	bool bMadeQuery = false;

#if !__FINAL
	if(!PARAM_netSessionDisableSocialMM.Get())
#endif
	{
		// check we're connected to ROS
		//@@: location CNETWORKSESSION_DOQUICKMATCH_CHECK_ROS_CREDENTIALS
		if(NetworkInterface::HasValidRosCredentials())
		{
			// friends query
			if(m_MatchmakingQueryEnabled[MatchmakingQueryType::MQT_Friends])
			{
				bSuccess &= FindSessionsFriends(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends], false);
				bMadeQuery = true; 
			}

			// social query - if we have a current crew
			if(m_MatchmakingQueryEnabled[MatchmakingQueryType::MQT_Social])
			{
				rlClanId nCrewID = RL_INVALID_CLAN_ID;

				NetworkClan& tClan = CLiveManager::GetNetworkClan();
				if(tClan.HasPrimaryClan())
					nCrewID = tClan.GetPrimaryClan()->m_Id;
#if !__FINAL
				s32 nCommandCrewId;
				if(PARAM_netMatchmakingCrewID.Get(nCommandCrewId))
					nCrewID = static_cast<rlClanId>(nCommandCrewId);
#endif
				if(nCrewID != RL_INVALID_CLAN_ID)
				{
					char szParams[256];
					snprintf(szParams, 256, "@crewid,%" I64FMT "d", nCrewID);
					bSuccess &= FindSessionsSocial(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Social], "CrewmateSessions", szParams);

					unsigned nMemberCount = tClan.GetMemberCount();
					NOTFINAL_ONLY(PARAM_netMatchmakingTotalCrewCount.Get(nMemberCount));

					// if we're in a large crew, require that we're in the same region
					if(nMemberCount > m_nLargeCrewValue)
						m_MatchMakingQueries[MatchmakingQueryType::MQT_Social].m_bSameRegionOnly = true;

					// in here, in case we have no clan
					bMadeQuery = true;
				}
			}
		}
	}

#if !__FINAL
	if(!PARAM_netSessionDisablePlatformMM.Get())
#endif
	{
		// platform query
		if(m_MatchmakingQueryEnabled[MatchmakingQueryType::MQT_Standard])
		{
			//@@: location CNETWORKSESSION_DOQUICKMATCH_FIND_SESSIONS
			bSuccess &= FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]);
			bMadeQuery = true;
		}
	}

	// set searching state if successful
	if(bMadeQuery)
	{
		if(bSuccess)
		{
			// open the network if it's not currently open
			if(PARAM_netSessionOpenNetworkOnQuickmatch.Get() && !CNetwork::IsNetworkOpen())
			{
				// have to reset the state prior to opening the network
				SetSessionState(SS_IDLE, true);
				OpenNetwork();
			}

			SetSessionState(SS_FINDING);

			//@@: location CNETWORKSESSION_DOQUICKMATCH_ADD_FIND_SESSION_EVENT
			GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
		}
	}
	else
	{
		m_SessionFilter.Clear();
		//@@: location CNETWORKSESSION_DOQUICKMATCH_HOST_SESSION
		HostSession(nGameMode, nMaxPlayers, HostFlags::HF_ViaQuickmatchNoQueries);
	}

	// all good?
	return bSuccess; 
}

bool CNetworkSession::DoQuickMatch(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	// log out matchmaking type
	gnetDebug1("DoQuickMatch");

	// ensure the quickmatch flag is applied
	matchmakingFlags |= MatchmakingFlags::MMF_Quickmatch;

	// call into freeroam matchmaking
	return DoFreeroamMatchmaking(nGameMode, nSchemaID, nMaxPlayers, matchmakingFlags);
}

bool CNetworkSession::DoFriendMatchmaking(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	// log out matchmaking type
	gnetDebug1("DoFriendMatchmaking");

	// check ROS credentials
	if(!gnetVerify(NetworkInterface::HasValidRosCredentials()))
	{
		gnetError("DoFriendMatchmaking :: No ROS credentials!");
		return false; 
	}

    // check we're not in a session already
	if(!gnetVerify(!IsSessionActive()))
    {
        gnetError("DoFriendMatchmaking :: Session already active!");
        return false; 
    }

	// always consider blacklisted for friends matchmaking
	matchmakingFlags |= MatchmakingFlags::MMF_AllowBlacklisted;

	// common setup for matchmaking
	if(!DoMatchmakingCommon(nGameMode, nSchemaID, nMaxPlayers, matchmakingFlags))
		return false;

	// log out filter contents
	m_SessionFilter.DebugPrint();

	// setup friends matchmaking
	bool bSuccess = FindSessionsFriends(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends], true);

	// set searching state if successful
	if(bSuccess)
	{
		SetSessionState(SS_FINDING);
		GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
	}

	// indicate result
	return bSuccess; 
}

bool CNetworkSession::DoSocialMatchmaking(const char* szQueryName, const char* szParams, const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	// log out matchmaking type
	gnetDebug1("DoSocialMatchmaking :: QueryName: %s, Params: %s", szQueryName, szParams);

	// check ROS credentials
	if(!gnetVerify(NetworkInterface::HasValidRosCredentials()))
	{
		gnetError("DoSocialMatchmaking :: No ROS credentials!");
		return false; 
	}

    // check we're not in a session already
	if(!gnetVerify(!IsSessionActive()))
    {
        gnetError("DoSocialMatchmaking :: Session already active!");
        return false; 
    }

	// always consider blacklisted for friends matchmaking
	matchmakingFlags |= MatchmakingFlags::MMF_AllowBlacklisted;

	// common setup for matchmaking
	if(!DoMatchmakingCommon(nGameMode, nSchemaID, nMaxPlayers, matchmakingFlags))
		return false;

	// log out filter contents
	m_SessionFilter.DebugPrint();

	// setup friends matchmaking
	bool bSuccess = FindSessionsSocial(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Social], szQueryName, szParams);

	// call social matchmaking query
	if(bSuccess)
	{
		SetSessionState(SS_FINDING);
		GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
	}

	// indicate result
	return bSuccess;
}

bool CNetworkSession::DoActivityQuickmatch(const int nGameMode, const unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, unsigned matchmakingFlags)
{
	// log out matchmaking type
	gnetDebug1("DoActivityQuickmatch :: ActivityType: %d, ActivityID: 0x%08x", nActivityType, nActivityID);

    // check we're not in a session already
    if(!gnetVerify(!IsSessionActive()))
    {
        gnetError("DoActivityQuickmatch :: Session already active!");
        return false; 
    }

	// apply common flags for activity quickmatch
	matchmakingFlags |= (MatchmakingFlags::MMF_Quickmatch | MatchmakingFlags::MMF_AllowBlacklisted);
	
	// common setup for matchmaking
	if(!DoMatchmakingCommon(nGameMode, SCHEMA_ACTIVITY, nMaxPlayers, matchmakingFlags))
		return false;

	// check if we've been given an activity type
	if(nActivityType != INVALID_ACTIVITY_TYPE)
	{
		MatchmakingAttributes::sAttribute* pAttribute = m_MatchmakingAttributes.GetAttribute(MatchmakingAttributes::MM_ATTR_ACTIVITY_TYPE);
		pAttribute->Apply(static_cast<unsigned>(nActivityType));
		m_SessionFilter.SetActivityType(pAttribute->GetValue());
	}

	// check if we've been given an activity ID
	if(nActivityID != 0)
	{
		MatchmakingAttributes::sAttribute* pAttribute = m_MatchmakingAttributes.GetAttribute(MatchmakingAttributes::MM_ATTR_ACTIVITY_ID);
		pAttribute->Apply(nActivityID);
		m_SessionFilter.SetActivityID(pAttribute->GetValue());
	}

	// log out filter contents
	m_SessionFilter.DebugPrint();

	// we want to end up in a session
	m_nQuickMatchStarted = sysTimer::GetSystemMsTime();

	// call platform matchmaking query
	if(FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]))
	{ 
		SetSessionState(SS_FINDING);
		GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
		return true;
	}

	// matchmaking failed
	return false;
}

bool CNetworkSession::HostSession(const int nGameMode, int nMaxPlayers, unsigned nHostFlags)
{
	// logging
    gnetDebug1("HostSession :: nGameMode: %s [%d], nMaxPlayers: %d, Flags: %d", NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nMaxPlayers, nHostFlags);

	if(!gnetVerify(!IsSessionCreated()))
	{
		gnetError("HostSession :: Already in session!");
		return false; 
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("HostSession"))
        return false;

	// verify that we have multiplayer access
	if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
	{
		gnetError("HostSession :: Cannot access multiplayer!");
		return false; 
	}

	// verify max players
	if(!gnetVerify(nMaxPlayers > 0))
	{
		gnetError("HostSession :: Invalid Max Players: %d. Must be at least 1!", nMaxPlayers);
		return false; 
	}

	// verify max players
	if(!gnetVerify(nMaxPlayers <= RL_MAX_GAMERS_PER_SESSION))
	{
		gnetError("HostSession :: Invalid Max Players: %d. Cannot be more than %d!", nMaxPlayers, RL_MAX_GAMERS_PER_SESSION);
		nMaxPlayers = RL_MAX_GAMERS_PER_SESSION;
	}

    // verify commerce has finished with the network heap
    if(!IsNetworkMemoryFree())
    {
        gnetError("HostSession :: Network Memory Unavailable! Call CanEnterMultiplayer!");
        return false; 
    }

    // add single player host flag if max players is 1
    if(nMaxPlayers == 1)
        SetFlags(nHostFlags, HostFlags::HF_SingleplayerOnly);

	// add an additional slot if we allow admin players to join solo sessions
	if(m_bAllowAdminInvitesToSolo && CheckFlag(nHostFlags, HostFlags::HF_SingleplayerOnly))
	{
		gnetDebug1("HostSession :: Incrementing players for ScAdmin slot");
		nMaxPlayers += 1;
	}

	// setup host config
	NetworkGameConfig ngConfig;
	player_schema::MatchingAttributes mmAttrs;
	ngConfig.Reset(mmAttrs, static_cast<u16>(nGameMode), SessionPurpose::SP_Freeroam, nMaxPlayers, CheckFlag(nHostFlags, HostFlags::HF_IsPrivate) ? nMaxPlayers : 0);
	
	// script treat a closed crew as maximum of 1 (limits of 2, 3 or 4 are crew sessions)
	if(m_UniqueCrewData[SessionType::ST_Physical].m_nLimit == 1)
		SetFlags(nHostFlags, HostFlags::HF_ClosedCrew);

	// host match
	m_nHostRetries = 0;
	return HostSession(ngConfig, nHostFlags);
}

bool CNetworkSession::HostClosed(const int nGameMode, const int nMaxPlayers, unsigned nHostFlags)
{
    // logging
    gnetDebug1("HostClosed :: nGameMode: %s [%d], nMaxPlayers: %d", NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nMaxPlayers);
	return HostSession(nGameMode, nMaxPlayers, nHostFlags | HostFlags::HF_NoMatchmaking | HostFlags::HF_Closed);
}

bool CNetworkSession::HostClosedFriends(const int nGameMode, const int nMaxPlayers, unsigned nHostFlags)
{
    // logging
    gnetDebug1("HostClosedFriends :: nGameMode: %s [%d], nMaxPlayers: %d", NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nMaxPlayers);
	return HostSession(nGameMode, nMaxPlayers, nHostFlags | HostFlags::HF_NoMatchmaking | HostFlags::HF_ClosedFriends);
}

bool CNetworkSession::HostClosedCrew(const int nGameMode, const int nMaxPlayers, const unsigned nUniqueCrewLimit, const unsigned nCrewLimitMaxMembers, unsigned nHostFlags)
{
    // logging
    gnetDebug1("HostClosedCrew :: nGameMode: %s [%d], nMaxPlayers: %d, nUniqueCrewLimit: %d, nCrewLimitMaxMembers: %d", NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nMaxPlayers, nUniqueCrewLimit, nCrewLimitMaxMembers);

    // apply if non-zero
    if(nUniqueCrewLimit > 0)
        SetUniqueCrewLimit(SessionType::ST_Physical, nUniqueCrewLimit);
    if(nCrewLimitMaxMembers > 0)
        SetCrewLimitMaxMembers(SessionType::ST_Physical, nCrewLimitMaxMembers);

    return HostSession(nGameMode, nMaxPlayers, nHostFlags | HostFlags::HF_NoMatchmaking | HostFlags::HF_ClosedCrew);
}

void CNetworkSession::JoinOrHostSession(const unsigned sessionIndex)
{
#if !__FINAL
	if(PARAM_netSessionBailFinding.Get())
	{
		gnetWarning("JoinOrHostSession :: Simulating eBAIL_SESSION_FIND_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_FIND_FAILED, BAIL_CTX_FIND_FAILED_CMD_LINE));
		return;
	}
#endif 

	// grab these
	const rlNetworkMode netMode = NetworkInterface::GetNetworkMode();
    unsigned nCurrentTime = sysTimer::GetSystemMsTime();
	
	// check whether this session should be considered
	//@@: location CNETWORKSESSION_JOINSESSIONORHOST_CONSIDER_SESSION_CREATE_BOOL
	bool bConsiderSession = sessionIndex < m_GameMMResults.m_nNumCandidates;
	
    // should we just fall into hosting
    bool bForceHost = false;

    // if matchmaking has run too long without success
    if(CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch)
#if !__FINAL
        && CNetwork::GetTimeoutTime() > 0
#endif
        )
    {
        if(s_TimeoutValues[TimeoutSetting::Timeout_QuickmatchLong] > 0 && m_nQuickMatchStarted > 0 && ((nCurrentTime - m_nQuickMatchStarted) > s_TimeoutValues[TimeoutSetting::Timeout_QuickmatchLong]))
        {
            gnetDebug1("JoinOrHostSession :: Quickmatch process has taken longer than %dms. Forcing host!", s_TimeoutValues[TimeoutSetting::Timeout_QuickmatchLong]);
            bForceHost = true;
            bConsiderSession = false;
        }
    }

	// logging
	gnetDebug1("JoinOrHostSession :: Session Index: %u / %u, Considered: %s, CanJoin: %s", sessionIndex, m_GameMMResults.m_nNumCandidates, bConsiderSession ? "True" : "False", snSession::CanJoin(netMode) ? "True" : "False");
	
	// attempt join
	//@@: location CNETWORKSESSION_JOINSESSIONORHOST_CONSIDER_SESSION_CAN_JOIN
	if(bConsiderSession && snSession::CanJoin(netMode) && !PARAM_netSessionForceHost.Get())
	{
        // we need to turn on matchmaking here so that we continue to process if we hit a failure
        m_bIsJoiningViaMatchmaking = true;

		// if we have been searching for a long time (perhaps we took time when failing to join a session or 
		// attempted a number of failed joins) our session results might be stale - fire off a refresh
		if(CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch)
#if !__FINAL
            && CNetwork::GetTimeoutTime() > 0
#endif
            )
		{
			if((m_GameMMResults.m_MatchmakingRetrievedTime > 0) && (nCurrentTime - m_GameMMResults.m_MatchmakingRetrievedTime) > s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingAttemptLong])
			{
                gnetDebug1("JoinOrHostSession :: Matchmaking results stale. Running search again!");
                
				// reissue active matchmaking queries
				bool bSuccess = true;
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends].m_bIsActive)
					bSuccess &= FindSessionsFriends(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends], !m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard].m_bIsActive);
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Social].m_bIsActive)
					bSuccess &= FindSessionsSocial(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Social], m_szSocialQuery, m_szSocialParams);
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard].m_bIsActive)
					bSuccess &= FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]);

                if(bSuccess)
	            {
		            SetSessionState(SS_FINDING);
		            GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
	            }
				
				// we are re-entering the joining process
				return;
			}
		}

        // take transition members with us if directed
        unsigned nGroupSize = 0;
        rlGamerHandle aGroup[MAX_NUM_PHYSICAL_PLAYERS];
        
        if(m_nGamersToGame > 0)
        {
            gnetDebug2("JoinOrHostSession :: Have %d group members", m_nGamersToGame);

            const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
            if(pInfo)
            {
                // copy in gamers
                rlGamerInfo hGamerInfo;
                for(unsigned i = 0; i < m_nGamersToGame; i++)
                {
                    // account for the targeted gamer having left the session
                    bool bGamerFound = m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hGamerInfo);
                    if(bGamerFound && nGroupSize < MAX_NUM_PHYSICAL_PLAYERS)
                    {
                        gnetDebug2("JoinOrHostSession :: Adding %s to group (from match transition)", hGamerInfo.GetName());
                        aGroup[nGroupSize++] = hGamerInfo.GetGamerHandle();
                    }
                }
            }
        }

#if RL_NP_SUPPORT_PLAY_TOGETHER
		// take play together gamers with us
		if(m_bUsePlayTogetherOnJoin) for(unsigned i = 0; i < m_nPlayTogetherPlayers; i++)
		{
			if(nGroupSize < MAX_NUM_PHYSICAL_PLAYERS)
			{
				gnetDebug2("JoinOrHostSession :: Adding %s to group (from play together)", NetworkUtils::LogGamerHandle(m_PlayTogetherPlayers[i]));
				aGroup[nGroupSize++] = m_PlayTogetherPlayers[i];
			}
		}
#endif

		// if we specify a group, make sure the local player is present
		if(nGroupSize > 0)
		{
			bool bLocalPresent = false;
			rlGamerHandle hLocal = NetworkInterface::GetLocalGamerHandle();

			// look for local player
			for(unsigned i = 0; i < nGroupSize; i++)
			{
				if(aGroup[i] == hLocal)
				{
					bLocalPresent = true;
					break;
				}
			}

			// need to add the local player here
			if(!bLocalPresent)
			{
				gnetDebug2("JoinOrHostSession :: Adding local to group");
				aGroup[nGroupSize++] = NetworkInterface::GetLocalGamerHandle();
			}
		}

		// apply timeouts and join
		SetSessionTimeout((sessionIndex + 1) < m_GameMMResults.m_nNumCandidates ? s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingMultipleSessions] : s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingOneSession]);

        // join the correct session type
		if(m_GameMMResults.m_CandidatesToJoin[sessionIndex].m_Session.m_SessionConfig.m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Freeroam)
        {
			if(!JoinMatchmakingSession(sessionIndex, aGroup, nGroupSize))
            {
                gnetWarning("JoinOrHostSession :: Bailing, JoinMatchmakingSession failed!");
                CNetwork::Bail(sBailParameters(BAIL_SESSION_FIND_FAILED, BAIL_CTX_FIND_FAILED_JOIN_FAILED));
            }
        }
		else
		{
			if(gnetVerify(CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_AnySessionType)))
			{
				m_bTransitionViaMainMatchmaking = true;
				m_bJoiningTransitionViaMatchmaking = true;
				JoinTransition(m_GameMMResults.m_CandidatesToJoin[sessionIndex].m_Session.m_SessionInfo, RL_SLOT_PUBLIC, JoinFlags::JF_Default);
			}
			else
			{
				// this is bad - inception! recursion should work here
				gnetError("JoinOrHostSession :: Session hosted by %s, Token: 0x%016" I64FMT "x, is a transition session!", m_GameMMResults.m_CandidatesToJoin[sessionIndex].m_Session.m_HostName, m_GameMMResults.m_CandidatesToJoin[sessionIndex].m_Session.m_SessionInfo.GetToken().m_Value);
				JoinOrHostSession(sessionIndex + 1);
			}
		}
	}
	else 
	{
        // if there are no platform matchmaking queries available, and all SCS queries failed (actually failed, 
        // and not just produced no results)
        bool bServiceFailure = true;
        for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
        {
            if(m_MatchMakingQueries[i].m_bIsActive)
            {
				bool bScMatchmaking = m_MatchMakingQueries[i].IsSocial() || m_MatchMakingQueries[i].IsPlatform();
                if(bScMatchmaking && (m_MatchMakingQueries[i].m_TaskStatus.GetStatus() != netStatus::NET_STATUS_FAILED))
                    bServiceFailure = false;
            }
        }

        // add logging
#if !__NO_OUTPUT
        if(bServiceFailure)
            gnetWarning("JoinOrHostSession :: Matchmaking failed due to service error!");
#endif

		// if host probability - not applied in social matchmaking
		if(!CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch) && !bServiceFailure && !bForceHost && m_bEnableHostProbability && m_nMatchmakingAttempts < m_MaxMatchmakingAttempts)
		{
			// generate a random number, compare to current probability of hosting
			if(m_fMatchmakingHostProbability < gs_Random.GetFloat())
			{
				gnetDebug1("JoinOrHostSession :: Returning to matchmaking via probability!");

				// update probability variables
				m_nMatchmakingAttempts++;
				m_fMatchmakingHostProbability *= m_fMatchmakingHostProbabilityScale;

				// reissue active matchmaking queries
				bool bSuccess = true;
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends].m_bIsActive)
					bSuccess &= FindSessionsFriends(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends], !m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard].m_bIsActive);
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Social].m_bIsActive)
					bSuccess &= FindSessionsSocial(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Social], m_szSocialQuery, m_szSocialParams);
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard].m_bIsActive)
					bSuccess &= FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]);

                if(bSuccess)
	            {
		            SetSessionState(SS_FINDING);
		            GetEventScriptNetworkGroup()->Add(CEventNetworkFindSession());
	            }

				// we are re-entering the joining process
				return;
			}
		}

		// reset this
		m_bIsJoiningViaMatchmaking = false;

		// if this isn't a quick match, fall out
		if(!CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch))
		{
            // specific failure case for a service failure
			gnetWarning("JoinOrHostSession :: Matchmaking failed to find and join a viable session.");
            CNetwork::Bail(sBailParameters(bServiceFailure ? BAIL_MATCHMAKING_FAILED : BAIL_SESSION_FIND_FAILED,
						   bServiceFailure ? BAIL_CTX_NONE : BAIL_CTX_FIND_FAILED_NOT_QUICKMATCH, 
						   -1,
						   -1,
						   !CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_NoScriptEventOnBail)));
		}
		// host check for normal sessions
		else if(snSession::CanHost(netMode))
		{
			// setup host config - base on the matchmaking filter
			NetworkGameConfig ngConfig;
			player_schema::MatchingAttributes mmAttrs;
			ngConfig.Reset(mmAttrs, m_SessionFilter.GetGameMode(), SessionPurpose::SP_Freeroam, m_nQuickmatchFallbackMaxPlayers, 0);
			ngConfig.Apply(&m_SessionFilter);

			// for an activity session, check what was applied for the search
			if(m_SessionFilter.GetMatchingFilterID() == SCHEMA_ACTIVITY)
			{
				int nActivityType = -1;
				unsigned uActivityType = 0;
				unsigned nActivityID = 0;
				if(m_SessionFilter.GetActivityType(uActivityType))
					nActivityType = static_cast<int>(uActivityType);

				m_SessionFilter.GetActivityID(nActivityID);

				ngConfig.SetActivityType(nActivityType);
				ngConfig.SetActivityID(nActivityID);
			}

			// acknowledge in the matchmaking results
			m_GameMMResults.m_bHosted = true;

			// host match
			m_nHostRetries = 0;
			if(!HostSession(ngConfig, HostFlags::HF_ViaMatchmaking))
            {
                gnetWarning("JoinOrHostSession :: Bailing, HostSession failed!");
                CNetwork::Bail(sBailParameters(BAIL_SESSION_FIND_FAILED, BAIL_CTX_FIND_FAILED_HOST_FAILED));
            }
		}
		else
		{
			gnetWarning("JoinOrHostSession :: Bailing, cannot join or host!");
			CNetwork::Bail(sBailParameters(BAIL_SESSION_FIND_FAILED, BAIL_CTX_FIND_FAILED_CANNOT_HOST));
		}
	}
}

void CNetworkSession::CheckForAnotherSessionOrBail(eBailReason nBailReason, int nBailContext)
{
	gnetDebug1("CheckForAnotherSessionOrBail :: Host: %s, CheckForAnotherSession: %s, ViaMM: %s, FailureAction: %s", 
		NetworkInterface::IsHost() ? "True" : "False", 
		m_bIsJoiningViaMatchmaking ? "True" : "False", 
		m_bCheckForAnotherSession ? "True" : "False", 
		GetJoinFailureActionAsString(m_JoinFailureAction));

	// check if we have another session to join
    if((!NetworkInterface::IsHost() || m_bCheckForAnotherSession) && (m_bIsJoiningViaMatchmaking || m_JoinFailureAction != JoinFailureAction::JFA_None))
	{
		// check if game mode needs cached (convert constant to u16 to account for wrap)
		if(m_Config[SessionType::ST_Physical].GetGameMode() != static_cast<u16>(MultiplayerGameMode::GameMode_Invalid))
			m_JoinFailureGameMode = m_Config[SessionType::ST_Physical].GetGameMode();
		
		if(CNetwork::IsNetworkOpen())
			CloseNetwork(false);

		// drop the session only
		DropSession();

		// reset the state ahead of another join / host attempt
		SetSessionState(SS_IDLE, true);

		// reset session config
		m_Config[SessionType::ST_Physical].Clear();

        if(m_bIsJoiningViaMatchmaking)
        {
			// the session might have migrated, so grab the current token
			rlSessionToken nToken = m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_Session.m_SessionInfo.GetToken();
			if(m_pSession->Exists())
				nToken = m_pSession->GetSessionInfo().GetToken();

		    // add to join failure sessions
			if(nToken.IsValid())
				AddJoinFailureSession(nToken);
			
			// record bail reason (offset to distinguish from response codes)
			m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_Result = (static_cast<unsigned>(nBailReason) | sMatchmakingResults::BAIL_OFFSET);
			m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_nTimeFinished = sysTimer::GetSystemMsTime();

			// and try the next session
			JoinOrHostSession(++m_GameMMResults.m_nCandidateToJoinIndex);
	    }
        else if(!DoJoinFailureAction())
        {
            gnetDebug1("CheckForAnotherSessionOrBail :: Failed to action join failure.");
            CNetwork::Bail(sBailParameters(nBailReason, nBailContext));
        }
	}
	else
		CNetwork::Bail(sBailParameters(nBailReason, nBailContext));

    // reset check flag
    m_bCheckForAnotherSession = false;
}

static const unsigned MAXIMUM_TIMEOUT = 20000;

void CNetworkSession::InitRegistrationPolicies()
{
    // setup the policies
	unsigned nTimeout = CNetwork::GetTimeoutTime();
#if !__FINAL
	if(CNetwork::IsTimeoutModifiedByCommandLine())
		nTimeout = Min(MAXIMUM_TIMEOUT, nTimeout);
#endif

	unsigned nMigrateTaskTimeout = nTimeout > 0 ? ((nTimeout * 3) / 2) : MAXIMUM_TIMEOUT;
	snMigrateSessionTask::SetTimeoutPolicy(nMigrateTaskTimeout);
}

void CNetworkSession::SetSessionTimeout(unsigned nTimeout)
{
		unsigned t = 0;

		// if we've specified a specific timeout
		if(nTimeout != 0)
		{
			unsigned nNetworkTimeout = CNetwork::GetTimeoutTime();
			if(nNetworkTimeout == 0)
				nTimeout = 0;
			else
			{
				if(PARAM_netSessionStateTimeout.Get(t))
					nTimeout = t;
	#if !__FINAL
				// match our timeout to the network timeout so that we can persist through stalled machines on the other side
				if(CNetwork::IsTimeoutModifiedByCommandLine())
					nTimeout = Max(nTimeout, CNetwork::GetTimeoutTime() * 2);
	#endif
			}
		}

	if(m_nSessionTimeout != nTimeout)
	{
		gnetDebug3("SetSessionTimeout :: %dms", nTimeout);
		m_nSessionTimeout = nTimeout;
	}
}

unsigned CNetworkSession::GetSessionTimeout()
{
	// add a multiplier if we're migrating
	if(m_nSessionTimeout > 0 && m_pSession->IsMigrating())
		return static_cast<unsigned>(static_cast<float>(m_nSessionTimeout) * m_fMigrationTimeoutMultiplier);

	// otherwise, just use the standard timeout
	return m_nSessionTimeout;
}

void CNetworkSession::SetSessionPolicies(eTimeoutPolicy nPolicy)
{
	static eTimeoutPolicy s_Policy = POLICY_INVALID;
	if(s_Policy == nPolicy)
		return;

	// update tracked policy
	s_Policy = nPolicy;

    // different amount of retries when we are matchmaking and have a number to get through
    static const unsigned NUM_RETRIES_NORMAL = 6;
    static const unsigned NUM_RETRIES_AGGRESSIVE = 3;

    // work out retry
    unsigned nRetries = (nPolicy == POLICY_NORMAL) ? NUM_RETRIES_NORMAL : NUM_RETRIES_AGGRESSIVE;

    // set retry policy
    snQueryHostConfigTask::SetRetryPolicy(snQueryHostConfigTask::DEFAULT_TIMEOUT_MS, nRetries);
    rlSessionManager::SetRetryPolicy(rlSessionManager::HOST_QUERY_DEFAULT_TIMEOUT_MS, nRetries);

	// setup the policies
	unsigned nTimeout = CNetwork::GetTimeoutTime();
#if !__FINAL
	if(CNetwork::IsTimeoutModifiedByCommandLine())
		nTimeout = Min(MAXIMUM_TIMEOUT, nTimeout);
	else
#endif
	{
		// half timeout with aggressive policy is switched on
		if(nPolicy == POLICY_AGGRESSIVE)
			nTimeout /= 2;
	}
}

bool CNetworkSession::IsSessionCompatible(NetworkGameFilter* pFilter, rlSessionDetail* pDetails, const unsigned nMmFlags OUTPUT_ONLY(, bool bLog))
{
	// convenience
	rlMatchingAttributes* pAttrs = &pDetails->m_SessionConfig.m_Attrs;

	// grab the user data size
	unsigned nUserDataSize = pDetails->m_SessionUserDataSize;

	// check session ID is valid
	if(pDetails->m_SessionConfig.m_SessionId == 0)
	{
		OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session hosted by %s. Invalid Session ID.", pDetails->m_HostName));
		return false;
	}

	// check session info is valid
	if(!pDetails->m_SessionInfo.IsValid())
	{
		OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session hosted by %s. Invalid Session Info.", pDetails->m_HostName));
		return false;
	}

	// check we have public slots
	if(pDetails->m_SessionConfig.m_MaxPublicSlots <= pDetails->m_NumFilledPublicSlots)
	{
		OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No public slots [%d/%d]", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pDetails->m_NumFilledPublicSlots, pDetails->m_SessionConfig.m_MaxPublicSlots));
		return false;
	}

	// check session type
	if(!CheckFlag(nMmFlags, MatchmakingFlags::MMF_AnySessionType) && (pAttrs->GetSessionPurpose() != pFilter->GetSessionPurpose()))
	{
		OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Different Session Type. Filter: %d, Session: %d.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pFilter->GetSessionPurpose(), pAttrs->GetSessionPurpose()));
		return false;
	}

    // check game mode
    if(pAttrs->GetGameMode() != pFilter->GetGameMode())
    {
        OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Different Game Mode. Filter: %d, Session: %d.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pFilter->GetGameMode(), pAttrs->GetGameMode()));
        return false;
    }

	//@@: range CNETWORKSESSION_APPLYSESSION_FILTER_CHECK_DISCRIMINATOR {
	// check discriminator
	const u32* pDiscriminator = pAttrs->GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_DISCRIMINATOR);
	if(!pDiscriminator)
	{
		OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No discriminator!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName));
		return false;
	}
	else if((pAttrs->GetSessionPurpose() == SessionPurpose::SP_Freeroam) && (pDetails->m_SessionUserData[0]) && (nUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Freeroam))))
	{
		SessionUserData_Freeroam* pData = reinterpret_cast<SessionUserData_Freeroam*>(pDetails->m_SessionUserData);

		// intro flow reject sessions with too many bosses
		const unsigned numBosses = GetNumBossesInFreeroam();
		if(m_IntroRejectSessionsWithLargeNumberOfBosses && CheckFlag(nMmFlags, MatchmakingFlags::MMF_ExpandedIntroFlow) && (m_IntroBossRejectThreshold > 0) && (numBosses >= m_IntroBossRejectThreshold))
		{
			OUTPUT_ONLY(if (bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s (IntroFlow). Too many bosses [%u]!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, numBosses));
			return false;
		}
		
		// reject sessions with too many bosses
		if(m_bRejectSessionsWithLargeNumberOfBosses && CheckFlag(nMmFlags, MatchmakingFlags::MMF_IsBoss) && (m_nBossRejectThreshold > 0) && (numBosses >= m_nBossRejectThreshold))
		{
			OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Too many bosses [%u]!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, numBosses));
			return false;
		}

		// check discriminators match
		if(*pDiscriminator != pFilter->GetDiscriminator())
		{
			// override for non-matching discriminator
			bool bException = false;

			// we can only get a non-matching discriminator through SCS matchmaking. Add additional checks for
			// social matchmaking to allow players to join 'closed' sessions for friends / crew
			// to check this, remove the visibility flag
			unsigned nDiscriminator = pFilter->GetDiscriminator() & ~(1 << NetworkBaseConfig::VISIBLE_SHIFT);

			// if these are now equal - we can check closed settings (if not a quickmatch)
			if(!CheckFlag(nMmFlags, MatchmakingFlags::MMF_Quickmatch) && *pDiscriminator == nDiscriminator)
			{
				if(m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends].m_bIsActive
					&& ((pData->m_nFlags & SessionUserData_Freeroam::USERDATA_FLAG_CLOSED_FRIENDS) != 0)
					&& rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), pDetails->m_HostHandle))
				{
					OUTPUT_ONLY(if(bLog) gnetDebug2("IsSessionCompatible :: Accepting closed friend session 0x%016" I64FMT "x hosted by %s.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName));
					bException = true;
				}

				if(!bException && m_MatchMakingQueries[MatchmakingQueryType::MQT_Social].m_bIsActive && ((pData->m_nFlags & SessionUserData_Freeroam::USERDATA_FLAG_CLOSED_CREW) != 0))
				{
					NetworkClan& tClan = CLiveManager::GetNetworkClan();
					if(tClan.HasPrimaryClan()) for(int c = 0; c < MAX_UNIQUE_CREWS; c++)
					{
						if(pData->m_nCrewID[c] != RL_INVALID_CLAN_ID && pData->m_nCrewID[c] == static_cast<s32>(tClan.GetPrimaryClan()->m_Id))
						{
							OUTPUT_ONLY(if(bLog) gnetDebug2("IsSessionCompatible :: Accepting closed crew session 0x%016" I64FMT "x hosted by %s.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName));
							bException = true;
							break;
						}
					}
				}
			}

			if(!bException)
			{
				OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Different Discriminator. Filter: %08x, Session: %08x.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pFilter->GetDiscriminator(), *pDiscriminator));
				return false;
			}
		}
	}
	//@@: } CNETWORKSESSION_APPLYSESSION_FILTER_CHECK_DISCRIMINATOR

	// check if we're looking for same region only sessions
	if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_SameRegionOnly))
	{
		// get our local region
		unsigned nLocalRegion = NetworkBaseConfig::GetMatchmakingRegion();

		// check region field
		const u32* pRegionField = pAttrs->GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_REGION);
		if(!pRegionField)
		{
			OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No region!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName));
			return false;
		}
		else
		{
			if(*pRegionField != nLocalRegion)
			{
				OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Different Region! Local: %d, Session: %d.", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, nLocalRegion, *pRegionField));
				return false;
			}
		}
	}

	// check groups
	if(pAttrs->GetSessionPurpose() == SessionPurpose::SP_Freeroam && pFilter->GetMatchingFilterID() == SCHEMA_GROUPS)
	{
		for(int j = 0; j < MAX_ACTIVE_MM_GROUPS; j++)
		{
			// optional value, so check this first
			const u32* pValue = pFilter->GetMatchingFilter().GetValue(pFilter->GetGroupFieldIndex(j));
			if(pValue)
			{
				unsigned nAttributeID = pFilter->GetMatchingFilter().GetSessionAttrFieldIdForCondition(pFilter->GetGroupFieldIndex(j));

				// need to have this field set
				const u32* pNumber = pAttrs->GetValueById(nAttributeID);
				if(!pNumber)
				{
					OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No %s value!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, gs_szMatchmakingGroupNames[m_ActiveMatchmakingGroups[j]]));
					return false;
				}
				// if we are requesting more space in the group than available (and our player count is in line with the group count)
				else if((*pNumber >= *pValue) && (pDetails->m_NumFilledPublicSlots >= *pNumber))
				{
					OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No space in requested group. Group: %s, Limit: %d, Number: %d, Players: %d", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, gs_szMatchmakingGroupNames[m_ActiveMatchmakingGroups[j]], *pValue, *pNumber, pDetails->m_NumFilledPublicSlots));
					return false;
				}
			}
		}
	}
	else if(pAttrs->GetSessionPurpose() == SessionPurpose::SP_Activity && pFilter->GetMatchingFilterID() == SCHEMA_ACTIVITY)
	{
		// optional value, so check this first
		const u32* pValue = pFilter->GetMatchingFilter().GetValue(pFilter->GetActivityPlayersFieldIndex());
		if(pValue)
		{
			// need to have this field set
			const u32* pNumber = pAttrs->GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_ACTIVITY_PLAYERS);
			if(!pNumber)
			{
				OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No activity slots value!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName));
				return false;
			}
			// if we are requesting more space in the group than available (and our player count is in line with the group count)
			else if(*pValue > *pNumber)
			{
				OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. No activity slots available, Available: %d, Requested: %d, Players: %d", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, *pNumber, *pValue, pDetails->m_NumFilledPublicSlots));
				return false;
			}
		}
	}

	// check user data
	switch(pAttrs->GetSessionPurpose())
	{
	case SessionPurpose::SP_Activity:
		if(nUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Activity)))
		{
			SessionUserData_Activity* pData = reinterpret_cast<SessionUserData_Activity*>(pDetails->m_SessionUserData);
			if(pData->m_hContentCreator.IsValid())
			{
#if RSG_SCE
				if(CLiveManager::IsInBlockList(pData->m_hContentCreator))
				{
					OUTPUT_ONLY(if (bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Blocked content creator (%s)!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pData->m_hContentCreator.ToString()));
					return false;
				}
				else
#endif
				if(!CLiveManager::CheckUserContentPrivileges())
				{
					OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Incompatible content creator (%s)!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pData->m_hContentCreator.ToString()));
					return false;
				}	
				else if(CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_ANYONE, false))
				{
					OUTPUT_ONLY(if(bLog) gnetDebug1("IsSessionCompatible :: Rejecting session 0x%016" I64FMT "x hosted by %s. Incompatible content creator (%s) for inactive profile!", pDetails->m_SessionInfo.GetToken().GetValue(), pDetails->m_HostName, pData->m_hContentCreator.ToString()));
					return false;
				}
			}
		}
		break;

	default:
		break;
	}

	return true;
}

unsigned CNetworkSession::ApplySessionFilter(NetworkGameFilter* pFilter, rlSessionDetail* pDetails, int nResults, const unsigned nMmFlags)
{
	bool bIsValid[MAX_MATCHMAKING_RESULTS];

	// make a note of all valid results in the detail array
	for(int i = 0; i < nResults; i++)
		bIsValid[i] = IsSessionCompatible(pFilter, &pDetails[i], nMmFlags OUTPUT_ONLY(, true));
		
	// move all valid results to the front
	int nValidResults = 0;
	for(int i = 0; i < nResults; i++)
	{
		if(!bIsValid[i])
			continue;

		if(i > nValidResults)
		{
			pDetails[nValidResults].Clear();
			pDetails[nValidResults].Reset(pDetails[i].m_HostPeerInfo,
										  pDetails[i].m_SessionInfo,
										  pDetails[i].m_SessionConfig, 
										  pDetails[i].m_HostHandle,
										  pDetails[i].m_HostName,
										  pDetails[i].m_NumFilledPublicSlots,
										  pDetails[i].m_NumFilledPrivateSlots,
										  pDetails[i].m_SessionUserData,
										  pDetails[i].m_SessionUserDataSize,
										  pDetails[i].m_SessionInfoMineData,
										  pDetails[i].m_SessionInfoMineDataSize);
		}
		
		// increment valid results
		nValidResults++;
	}

	return nValidResults;
}

bool CNetworkSession::IsMatchmakingPending(sMatchmakingQuery* pQuery)
{
	if(!pQuery->m_bIsPending)
		return false;

	// check status progress
	if(!pQuery->m_TaskStatus.Pending())
	{
		// matchmaking query completed
		pQuery->m_bIsPending = false;
		return false;
	}

	return true; 
}

// candidate score per criteria
struct sMatchmakingSort
{
    static bool Sort(const sMatchmakingSort& a, const sMatchmakingSort& b) { return a.nScore > b.nScore; }

    int nScore;
    u8 nCandidateID;
};

void CNetworkSession::DoSortMatchmakingPreference(NetworkGameFilter* pFilter, sMatchmakingCandidate* pCandidates, unsigned nCandidates, const unsigned nMmFlags)
{
    // no sorting required
    if(nCandidates <= 1 NOTFINAL_ONLY(&& !PARAM_netMatchmakingAlwaysRunSorting.Get()))
        return;

	// grab tunables
	CTunables& rTunables = Tunables::GetInstance();

	// check if sorting is disabled:
	const bool bSortingDisabled = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_SORTING_DISABLED", 0x308c1f01), false);
	if(bSortingDisabled)
	{
		gnetDebug1("DoSortMatchmakingPreference :: Sorting is disabled");
		return;
	}

    // matchmaking constants
    const int nRankRange = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_RANK_RANGE", 0x7676e5c1), 2);
    
    int aQueryHitScore[MatchmakingQueryType::MQT_Num];
    aQueryHitScore[MatchmakingQueryType::MQT_Friends] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_FRIEND_SCORE", 0x72ab0a56), 800);
    aQueryHitScore[MatchmakingQueryType::MQT_Social] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_SOCIAL_SCORE", 0x5d3ea92d), 400);
    aQueryHitScore[MatchmakingQueryType::MQT_Standard] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PLATFORM_SCORE", 0x456a738d), 100);

    // these are distinct blocks so that we can use the ATSTRINGHASH macro (rather than building a string)
	float fPreferFilledSessionChance[SessionType::ST_Max];
	float fUseSessionBeaconChance[SessionType::ST_Max];
    float aWeight[SessionType::ST_Max][CRITERIA_NUM];
    
    // freeroam
	fPreferFilledSessionChance[SessionType::ST_Physical] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PREFER_FILLED_SESSION_CHANCE", 0x90206c1a), 0.9f);
	fUseSessionBeaconChance[SessionType::ST_Physical] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_SESSION_BEACON_CHANCE", 0x6bc451fb), 1.0f);
    aWeight[SessionType::ST_Physical][CRITERIA_COMPOSITION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_COMPOSITION_WEIGHT", 0x1c653189), 1.5f);
    aWeight[SessionType::ST_Physical][CRITERIA_PLAYER_COUNT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PLAYER_COUNT_WEIGHT", 0xea830e8e), 1.5f);
    aWeight[SessionType::ST_Physical][CRITERIA_FREEROAM_PLAYERS] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_FREEROAM_PLAYERS_WEIGHT", 0x644ba952), 1.0f);
    aWeight[SessionType::ST_Physical][CRITERIA_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_RANK_WEIGHT", 0xac337772), 0.5f);
	aWeight[SessionType::ST_Physical][CRITERIA_PROPERTY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROPERTY_WEIGHT", 0xb2036813), 0.2f);
	aWeight[SessionType::ST_Physical][CRITERIA_MENTAL_STATE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MENTAL_STATE_WEIGHT", 0x23e7469b), 0.5f);
	aWeight[SessionType::ST_Physical][CRITERIA_MINIMUM_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MINIMUM_RANK_WEIGHT", 0xab21d357), 10.0f);
    aWeight[SessionType::ST_Physical][CRITERIA_LOCATION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LOCATION_WEIGHT", 0x00797143), 1.0f);
    aWeight[SessionType::ST_Physical][CRITERIA_BEACON] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BEACON_WEIGHT", 0x4a79cfd8), 0.0f);
    aWeight[SessionType::ST_Physical][CRITERIA_ELO] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ELO_WEIGHT", 0x58eba8d8), 2.0f);
	aWeight[SessionType::ST_Physical][CRITERIA_ASYNC] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ASYNC_WEIGHT", 0x7e7339ab), 8.0f);
	aWeight[SessionType::ST_Physical][CRITERIA_REGION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_WEIGHT", 0xafc76632), 1.5f);
	aWeight[SessionType::ST_Physical][CRITERIA_BOSS_DEPRIORITISE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_DEPRIORITISE_WEIGHT", 0xd6d7a812), 2.5f);
	aWeight[SessionType::ST_Physical][CRITERIA_BOSS_REJECT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_REJECT_WEIGHT", 0xc96c84b9), 20.0f);
	aWeight[SessionType::ST_Physical][CRITERIA_ISLAND] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ISLAND_WEIGHT", 0x5fa3535b), 0.0f);
	aWeight[SessionType::ST_Physical][CRITERIA_IN_PROGRESS_TIME_REMAINING] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_IN_PROGRESS_TIME_REMAINING_WEIGHT", 0xe8a622cf), 0.0f);
	WIN32PC_ONLY(aWeight[SessionType::ST_Physical][CRITERIA_PED_DENSITY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PED_DENSITY_WEIGHT", 0x88566cde), 1.0f);)

	// activity - use specific tunables for Heist
	static const unsigned HEIST_PREP_ACTIVITY_ID = 10;
	static const unsigned HEIST_FINALE_ACTIVITY_ID = 11;

	unsigned nActivityID = 0;
	if(pFilter)
		pFilter->GetActivityID(nActivityID);

    if((nActivityID == HEIST_PREP_ACTIVITY_ID) || (nActivityID == HEIST_FINALE_ACTIVITY_ID))
	{
		fPreferFilledSessionChance[SessionType::ST_Transition] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PREFER_FILLED_SESSION_CHANCE_HEIST", 0x5cbee856), 0.35f);
		fUseSessionBeaconChance[SessionType::ST_Transition] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_SESSION_BEACON_CHANCE_HEIST", 0xf426f8fc), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_COMPOSITION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_COMPOSITION_WEIGHT_HEIST", 0x5d7ea2a5), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_PLAYER_COUNT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PLAYER_COUNT_WEIGHT_HEIST", 0xa022b233), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_FREEROAM_PLAYERS] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_FREEROAM_PLAYERS_WEIGHT_HEIST", 0x7066fe47), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_RANK_WEIGHT_HEIST", 0xef6cea30), 0.75f);
		aWeight[SessionType::ST_Transition][CRITERIA_PROPERTY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROPERTY_WEIGHT_HEIST", 0x58e80554), 0.2f);
		aWeight[SessionType::ST_Transition][CRITERIA_MENTAL_STATE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MENTAL_STATE_WEIGHT_HEIST", 0xce3a05de), 0.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_MINIMUM_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MINIMUM_RANK_WEIGHT_HEIST", 0xb63f632b), 10.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_LOCATION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LOCATION_WEIGHT_HEIST", 0xd8882d24), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_BEACON] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BEACON_WEIGHT_HEIST", 0x0db17a56), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_ELO] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ELO_WEIGHT_HEIST", 0x217a81f7), 2.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_ASYNC] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ASYNC_WEIGHT_ASYNC_HEIST", 0x9873a765), 0.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_REGION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_WEIGHT_HEIST", 0x01a8b0cc), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_BOSS_DEPRIORITISE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_DEPRIORITISE_WEIGHT_HEIST", 0x65de3d07), 2.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_BOSS_REJECT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_REJECT_WEIGHT_HEIST", 0x422bea6d), 20.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_ISLAND] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ISLAND_WEIGHT_HEIST", 0x9e07a55e), 20.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_IN_PROGRESS_TIME_REMAINING] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_IN_PROGRESS_TIME_REMAINING_WEIGHT_HEIST", 0x02485c24), 20.0f);
		WIN32PC_ONLY(aWeight[SessionType::ST_Transition][CRITERIA_PED_DENSITY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PED_DENSITY_WEIGHT_HEIST", 0x52363dcc), 1.0f);)
	}
	else
	{
		fPreferFilledSessionChance[SessionType::ST_Transition] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PREFER_FILLED_SESSION_CHANCE_ACTIVITY", 0xfad04cfe), 0.65f);
		fUseSessionBeaconChance[SessionType::ST_Transition] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_USE_SESSION_BEACON_CHANCE_ACTIVITY", 0xa437c3c6), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_COMPOSITION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_COMPOSITION_WEIGHT_ACTIVITY", 0x6f0a09b7), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_PLAYER_COUNT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PLAYER_COUNT_WEIGHT_ACTIVITY", 0x173a53f6), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_FREEROAM_PLAYERS] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_FREEROAM_PLAYERS_WEIGHT_ACTIVITY", 0x07db27a3), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_RANK_WEIGHT_ACTIVITY", 0xe3de4d36), 0.75f);
		aWeight[SessionType::ST_Transition][CRITERIA_PROPERTY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PROPERTY_WEIGHT_ACTIVITY", 0x917bf7ca), 0.2f);
		aWeight[SessionType::ST_Transition][CRITERIA_MENTAL_STATE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MENTAL_STATE_WEIGHT_ACTIVITY", 0x783c97ab), 0.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_MINIMUM_RANK] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_MINIMUM_RANK_WEIGHT_ACTIVITY", 0xb6998f79), 10.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_LOCATION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_LOCATION_WEIGHT_ACTIVITY", 0x452ba423), 1.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_BEACON] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BEACON_WEIGHT_ACTIVITY", 0x991d74cb), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_ELO] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ELO_WEIGHT_ACTIVITY", 0x9140e84c), 2.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_ASYNC] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ASYNC_WEIGHT_ACTIVITY", 0x7a5f95ea), 8.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_REGION] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_REGION_WEIGHT_ACTIVITY", 0x2422b9f0), 1.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_BOSS_DEPRIORITISE] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_DEPRIORITISE_WEIGHT_ACTIVITY", 0x2939cbfe), 2.5f);
		aWeight[SessionType::ST_Transition][CRITERIA_BOSS_REJECT] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_BOSS_REJECT_WEIGHT_ACTIVITY", 0xbcd203ec), 20.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_ISLAND] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_ISLAND_WEIGHT_ACTIVITY", 0x9a7cf984), 20.0f);
		aWeight[SessionType::ST_Transition][CRITERIA_IN_PROGRESS_TIME_REMAINING] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_IN_PROGRESS_TIME_REMAINING_WEIGHT_ACTIVITY", 0xdee703c3), 5.0f);
		WIN32PC_ONLY(aWeight[SessionType::ST_Transition][CRITERIA_PED_DENSITY] = rTunables.TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MATCHMAKING_PED_DENSITY_WEIGHT_ACTIVITY", 0x540066ff), 1.0f);)
	}

    // random chance that we prefer filled sessions
    float fFilledChance = gs_Random.GetFloat();
    bool bPreferFilledSessions[SessionType::ST_Max];
    for(int i = 0; i < SessionType::ST_Max; i++)
        bPreferFilledSessions[i] = (fFilledChance >= (1.0f - fPreferFilledSessionChance[i]));

	// random chance that we use the session beacon
	float fBeaconChance = gs_Random.GetFloat();
	bool bUseSessionBeacon[SessionType::ST_Max];
	for(int i = 0; i < SessionType::ST_Max; i++)
		bUseSessionBeacon[i] = (fBeaconChance >= (1.0f - fUseSessionBeaconChance[i]));

    // flatten scores
	// scores are sorted where bigger numbers are better
    static const unsigned MAX_RESULTS = MatchmakingQueryType::MQT_Num * MAX_MATCHMAKING_RESULTS;
    sMatchmakingSort aSort[CRITERIA_NUM][MAX_RESULTS];
    for(unsigned i = 0; i < CRITERIA_NUM; i++)
    {
        for(unsigned j = 0; j < MAX_RESULTS; j++)
        {
            aSort[i][j].nScore = 0;
            aSort[i][j].nCandidateID = static_cast<u8>(j);
        }
    }   

    // grab local character rank
	u16 nMyCharacterRank = 0;
	StatId tRankStat = StatId(STAT_RANK_FM.GetHash(CHAR_MP0+StatsInterface::GetCurrentMultiplayerCharaterSlot()));
	if(gnetVerifyf(StatsInterface::IsKeyValid(tRankStat), "Failing to retrieve the current character rank"))
		nMyCharacterRank = (u16) StatsInterface::GetIntStat(tRankStat);
	// Log character rank.
	gnetDebug1("DoSortMatchmakingPreference :: MyCharacterRank: %u", nMyCharacterRank);
	  
	// grab local matchmaking region
	//@@: location CNETWORKSESSION_DOSORTMATCHMAKINGPREFERENCE_GET_REGION
	unsigned nLocalRegion = NetworkBaseConfig::GetMatchmakingRegion();
    
    // assign each candidate a score
    for(unsigned i = 0; i < nCandidates; i++)
    {
        // reset score
        pCandidates[i].m_nCandidateID = static_cast<u8>(i);
        pCandidates[i].m_fScore = 0.0f;

        // grab the user data size
        unsigned nUserDataSize = pCandidates[i].m_pDetail->m_SessionUserDataSize;
        unsigned nSessionPlayers = (pCandidates[i].m_pDetail->m_NumFilledPrivateSlots + pCandidates[i].m_pDetail->m_NumFilledPublicSlots);

        // track whether we've found the character rank
        bool bHaveCharacterRank = false;
        u16 nCharacterRankAverage = 0;

        // track whether we have beacon data
        bool bHaveBeaconData = true; 
        u8 nMatchmakingBeacon = 0;

        // CRITERIA_COMPOSITION
        for(unsigned j = 0; j < MatchmakingQueryType::MQT_Num; j++)
            aSort[CRITERIA_COMPOSITION][i].nScore += (pCandidates[i].m_nHits[j] * aQueryHitScore[j]);

		// CRITERIA_REGION
		const u32* pRegionField = pCandidates[i].m_pDetail->m_SessionConfig.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_REGION);
		aSort[CRITERIA_REGION][i].nScore = pRegionField ? ((nLocalRegion == *pRegionField) ? 1 : 0) : 1;

        // game mode specific
        switch(pCandidates[i].m_pDetail->m_SessionConfig.m_Attrs.GetSessionPurpose())
        {
        case SessionPurpose::SP_Freeroam:

            // CRITERIA_PLAYER_COUNT
            if(bPreferFilledSessions[SessionType::ST_Physical])
                aSort[CRITERIA_PLAYER_COUNT][i].nScore += nSessionPlayers;

            if(nUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Freeroam)))
            {
                SessionUserData_Freeroam* pData = reinterpret_cast<SessionUserData_Freeroam*>(pCandidates[i].m_pDetail->m_SessionUserData);

                // CRITERIA_FREEROAM_PLAYERS
                float fFreeroamRatio = 1.0f - (static_cast<float>(pData->m_nTransitionPlayers) / static_cast<float>(nSessionPlayers));
                aSort[CRITERIA_FREEROAM_PLAYERS][i].nScore = static_cast<int>((fFreeroamRatio * 100.0f)) * (nSessionPlayers - pData->m_nTransitionPlayers);

                // CRITERIA_PROPERTY
                // don't do this when coming back with gamers
                if(!(m_bIsTransitionToGame && m_nGamersToGame > 0))
                {
                    int nSameProperties = 0;
                    if(m_nMatchmakingPropertyID != NO_PROPERTY_ID)
                    {
                        for(int j = 0; j < MAX_NUM_PHYSICAL_PLAYERS; j++)
                        {
                            if(m_nMatchmakingPropertyID == pData->m_nPropertyID[j])
                                nSameProperties++;
                        }
                    }
                    aSort[CRITERIA_PROPERTY][i].nScore += (nSameProperties * -1);
                }

				// CRITERIA_MENTAL_STATE
				// don't do this when coming back with gamers
				if(!(m_bIsTransitionToGame && m_nGamersToGame > 0))
					aSort[CRITERIA_MENTAL_STATE][i].nScore = Abs((m_nMatchmakingMentalState * g_MentalStateScale) - pData->m_nAverageMentalState) * -1;
				
#if RSG_PC
				// CRITERIA PED DENSITY
				u8 nLocalPedDensity = static_cast<u8>(CSettingsManager::GetInstance().GetSettings().m_graphics.m_CityDensity*10.0f);
				if(!(m_bIsTransitionToGame && m_nGamersToGame > 0))
					aSort[CRITERIA_PED_DENSITY][i].nScore = Abs(nLocalPedDensity - pData->m_nAveragePedDensity)*-1;
#endif

                // CRITERIA_LOCATION
                // from single player, we are specifically placed somewhere, so this is not relevant
                if(m_bIsTransitionToGame)
                {
                    Vector3 vAveragePosition(static_cast<float>(pData->m_ux), static_cast<float>(pData->m_uy), static_cast<float>(pData->m_uz));
                    float fMag = (m_vToGamePosition - vAveragePosition).Mag2();

                    // -1, closer is better
                    aSort[CRITERIA_LOCATION][i].nScore += (static_cast<int>(fMag) * -1);
                }

				// boss checks for normal matchmaking
				const unsigned numBosses = GetNumBossesInFreeroam();
				if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_IsBoss))
				{
					// CRITERIA_BOSS_DEPRIORITISE
					if(numBosses < m_nBossDeprioritiseThreshold)
						aSort[CRITERIA_BOSS_DEPRIORITISE][i].nScore = 1;

					// CRITERIA_BOSS_REJECT
					if(!m_bRejectSessionsWithLargeNumberOfBosses && (numBosses < m_nBossRejectThreshold))
						aSort[CRITERIA_BOSS_REJECT][i].nScore = 1;
				}

				// boss checks for expanded intro matchmaking
				if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_ExpandedIntroFlow))
				{
					// CRITERIA_BOSS_DEPRIORITISE
					if(numBosses < m_IntroBossDeprioritiseThreshold)
						aSort[CRITERIA_BOSS_DEPRIORITISE][i].nScore = 1;

					// CRITERIA_BOSS_REJECT
					if(!m_IntroRejectSessionsWithLargeNumberOfBosses && (numBosses < m_IntroBossRejectThreshold))
						aSort[CRITERIA_BOSS_REJECT][i].nScore = 1;
				}

                // have valid character rank data
                bHaveCharacterRank = true;
                nCharacterRankAverage = pData->m_nAverageCharacterRank;

                // have valid beacon data
                bHaveBeaconData = bUseSessionBeacon[SessionType::ST_Physical]; 
                nMatchmakingBeacon = pData->m_nBeacon;
            }
            break;

        case SessionPurpose::SP_Activity:

            // CRITERIA_PLAYER_COUNT
            if(bPreferFilledSessions[SessionType::ST_Transition])
                aSort[CRITERIA_PLAYER_COUNT][i].nScore += nSessionPlayers;

            if(nUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Activity)))
            {
                SessionUserData_Activity* pData = reinterpret_cast<SessionUserData_Activity*>(pCandidates[i].m_pDetail->m_SessionUserData);

				// CRITERIA_MINIMUM_RANK
				// binary result with large weight
				aSort[CRITERIA_MINIMUM_RANK][i].nScore = nMyCharacterRank >= pData->m_nMinimumRank ? 1 : 0;

                // CRITERIA_ELO
                aSort[CRITERIA_ELO][i].nScore = Abs(m_nMatchmakingELO - pData->m_nAverageELO) * -1;

				// CRITERIA_ASYNC
				// binary result with large weight
				if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_Asynchronous))
					aSort[CRITERIA_ASYNC][i].nScore = (pData->m_bIsPreferred == TRUE) ? 1 : 0;
				else
					aSort[CRITERIA_ASYNC][i].nScore = (pData->m_bIsWaitingAsync == TRUE) ? 0 : 1;

				// if this filter allows for in progress jobs
				if(pFilter->IsInProgress())
				{
					// CRITERIA_IN_PROGRESS_TIME_REMAINING
					u32 nTimeLeft = 0;
					if(pData->m_InProgressFinishTime > 0)
					{
						u32 nPosixTime = static_cast<u32>(rlGetPosixTime());
						nTimeLeft = (nPosixTime >= pData->m_InProgressFinishTime) ? 0 : (pData->m_InProgressFinishTime - nPosixTime);
					}

					// smaller value 'better' but score is ordered by biggest so flip values 
					// values of 0, best, are unaffected
					aSort[CRITERIA_IN_PROGRESS_TIME_REMAINING][i].nScore = static_cast<int>(nTimeLeft) * -1; 
				}

				// CRITERIA_ISLAND
				unsigned nActivityID = 0;
				if(pFilter->GetActivityID(nActivityID))
				{
					// if we specifically looked for a particular ID and this job is in the specific activity island, boost it
					// It's player_schema::SchemaBase::FIELD_ID_MMATTR_MM_GROUP_2 because that's the one that happens to map to the one we want for activity island.
					const u32* pActivityIslandField = pCandidates[i].m_pDetail->m_SessionConfig.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_MM_GROUP_2);
					aSort[CRITERIA_ISLAND][i].nScore = pActivityIslandField ? ((ACTIVITY_ISLAND_SPECIFIC == *pActivityIslandField) ? 1 : 0) : 0;
				}
				
                // have valid character rank data
                bHaveCharacterRank = true;
                nCharacterRankAverage = pData->m_nAverageCharacterRank;

                // have valid beacon data
				bHaveBeaconData = bUseSessionBeacon[SessionType::ST_Transition]; 
                nMatchmakingBeacon = pData->m_nBeacon;
            }
            break;

        default:
            break;
        }

        // CRITERIA_RANK
        if(bHaveCharacterRank)
        {
			// Get difference between nMyCharacterRank and the nCharacterRankAverage.
            u16 nRankDiff = Max(nMyCharacterRank, nCharacterRankAverage) - Min(nMyCharacterRank, nCharacterRankAverage);
			if((nRankDiff - nRankRange) > nRankRange)
                aSort[CRITERIA_RANK][i].nScore += (((nRankDiff - nRankRange) / nRankRange) * -1);
        }

        // CRITERIA_BEACON
        if(bHaveBeaconData)
            aSort[CRITERIA_BEACON][i].nScore += static_cast<int>(nMatchmakingBeacon);

        // CRITERIA_ELO invalidates other criteria
        // easier to have this catch all than add the check around every criteria
        if(m_nMatchmakingELO > 0)
        {
            for(unsigned j = 0; j < CRITERIA_NUM; j++) if(j != CRITERIA_ELO)
                aSort[j][i].nScore = 0;
		}
    }

    // sort each criteria
    for(unsigned i = 0; i < CRITERIA_NUM; i++)
    {
        // use predicate above
        std::sort(&aSort[i][0], &aSort[i][nCandidates], sMatchmakingSort::Sort);

        // 
        unsigned nUniquePosition = 1;
        for(unsigned j = 0; j < nCandidates; j++)
        {
            // consider equal scores
            if(j > 0)
            {
                if(aSort[i][j - 1].nScore != aSort[i][j].nScore)
                    nUniquePosition++;
            }

            // determine correct weightings (this allows us to support mixed results (using, for 
            // instance, friends / social matchmaking)
            unsigned sessionType;
            switch(pCandidates[aSort[i][j].nCandidateID].m_pDetail->m_SessionConfig.m_Attrs.GetSessionPurpose())
            {
            case SessionPurpose::SP_Freeroam: sessionType = SessionType::ST_Physical;                break;
            case SessionPurpose::SP_Activity: sessionType = SessionType::ST_Transition;    break;
            default: 
                gnetAssertf(0, "Unsupported Matchmaking Session Type!"); 
                sessionType = SessionType::ST_Physical;
                break;
            }

            // add to overall score
            pCandidates[aSort[i][j].nCandidateID].m_fScore += (static_cast<float>(nUniquePosition) * aWeight[sessionType][i]);
        }
    }

    // and, finally, sort the candidates
    std::sort(&pCandidates[0], &pCandidates[nCandidates], sMatchmakingCandidate::Sort); 

    // log out candidate order and scores
#if !__NO_OUTPUT

	// shorthand for logging
	const char* szCritNames[] = 
	{ 
		"CM",					// CRITERIA_COMPOSITION,
		"PL", 					// CRITERIA_PLAYER_COUNT,
		"FR", 					// CRITERIA_FREEROAM_PLAYERS,
		"LO", 					// CRITERIA_LOCATION,
		"RA", 					// CRITERIA_RANK,
		"PR", 					// CRITERIA_PROPERTY,
		"MS", 					// CRITERIA_MENTAL_STATE,
		"MR", 					// CRITERIA_MINIMUM_RANK,
		"BE", 					// CRITERIA_BEACON,
		"EL", 					// CRITERIA_ELO,
		"AS", 					// CRITERIA_ASYNC,
		"RE", 					// CRITERIA_REGION,
		"BD", 					// CRITERIA_BOSS_DEPRIORITISE,
		"BR", 					// CRITERIA_BOSS_REJECT,
		"IS", 					// CRITERIA_ISLAND,
		"IP" 					// CRITERIA_IN_PROGRESS_TIME_REMAINING,
		WIN32PC_ONLY(, "PD") 	// WIN32PC_ONLY( CRITERIA_PED_DENSITY, )
	};
	CompileTimeAssert(COUNTOF(szCritNames) == CRITERIA_NUM);
    
    // general logging
    gnetDebug1("DoSortMatchmakingPreference :: Prefer filled sessions: %s / %s!",
                bPreferFilledSessions[SessionType::ST_Physical] ? "True" : "False",
                bPreferFilledSessions[SessionType::ST_Transition] ? "True" : "False");
    
    // for all candidates
    for(unsigned i = 0; i < nCandidates; i++)
    {
        char szTemp[256];
        atString tString("DoSortMatchmakingPreference :: Host: "); 
        tString += pCandidates[i].m_pDetail->m_HostName;
        tString += ", Candidate: ";
        formatf(szTemp, "%d", i);
        tString += szTemp;

        formatf(szTemp, ", Score: %.2f, Criteria: ", pCandidates[i].m_fScore);
        tString += szTemp;

        for(unsigned j = 0; j < CRITERIA_NUM; j++)
        {
            if(j > 0)
				tString += ", ";

            for(unsigned k = 0; k < nCandidates; k++)
            {
                // criteria lists have been sorted so need to find the candidate
                if(pCandidates[i].m_nCandidateID == aSort[j][k].nCandidateID)
                {
                    formatf(szTemp, "%s: %d", szCritNames[j], aSort[j][k].nScore);
                    tString += szTemp;
                }
			}
        }

        // log out score 
        gnetDebug1("%s", tString.c_str());
    }
#endif
}

void CNetworkSession::ProcessMatchmakingResults(sMatchmakingQuery* pQueries, unsigned nQueries, sMatchmakingResults* pResults, const unsigned nMmFlags, const u64 nUniqueID)
{
    gnetDebug1("ProcessMatchmakingResults :: Flags: 0x%x, Unique ID: 0x%016" I64FMT "x", nMmFlags, nUniqueID);
    
    // candidate list
    sMatchmakingCandidate aCandidates[MAX_MATCHMAKING_RESULTS * MatchmakingQueryType::MQT_Num];
    unsigned nCandidates = 0;

	// check results
    for(unsigned i = 0; i < nQueries; i++)
    {
        sMatchmakingQuery* pQuery = &pQueries[i];
        if(!pQuery->m_bIsActive)
            continue; 

        gnetDebug1("\t%s[%u]. Found %d sessions.", GetMatchmakingQueryTypeAsString(pQuery->m_QueryType), i, pQuery->m_nResults);
        
		// filter to same region only if specified
		unsigned nFilterMmFlags = nMmFlags;
		if(pQuery->m_bSameRegionOnly)
			nFilterMmFlags |= MatchmakingFlags::MMF_SameRegionOnly;

        // filter out incompatible sessions
		pQuery->m_nResults = ApplySessionFilter(pQuery->m_pFilter, pQuery->m_Results, pQuery->m_nResults, nFilterMmFlags); 

		unsigned nQueryCandidatesIdx = nCandidates;
		unsigned nQueryCandidatesNum = 0;

        // check what's left
        for(unsigned j = 0; j < pQuery->m_nResults; j++)
        {
            rlSessionToken nSessionToken = pQuery->m_Results[j].m_SessionInfo.GetToken();
            
            // search existing candidates
            bool bDuplicate = false;
            for(unsigned k = 0; k < nCandidates; k++)
            {
                if(nSessionToken == aCandidates[k].m_pDetail->m_SessionInfo.GetToken())
                {
                    // increase hits for the current query type
                    aCandidates[k].m_nHits[pQuery->m_QueryType]++;
                    
                    // flag as duplicate so that we don't enter it again
                    gnetDebug1("\tDuplicate session hosted by %s removed", pQuery->m_Results[j].m_HostName);
                    bDuplicate = true;
                    break;
                }
            }

            // skip adding duplicate
            if(bDuplicate)
                continue;

            // copy in query
            aCandidates[nCandidates].m_pDetail = &pQuery->m_Results[j];

            // flatten hit tracking
            for(unsigned k = 0; k < MatchmakingQueryType::MQT_Num; k++)
                aCandidates[nCandidates].m_nHits[k] = 0;

            // we know how many friends are contained from the presence query
            if(pQuery->m_QueryType == MatchmakingQueryType::MQT_Friends)
                aCandidates[nCandidates].m_nHits[pQuery->m_QueryType] = static_cast<u8>(pQuery->m_nNumHits[j]);
            else
                aCandidates[nCandidates].m_nHits[pQuery->m_QueryType] = 1;

#if !__NO_OUTPUT
            player_schema::SchemaBase::FieldId nFieldID = player_schema::SchemaBase::FIELD_ID_MMATTR_MM_GROUP_2;
            if(pQuery->m_Results[j].m_SessionConfig.m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Activity)
                nFieldID = player_schema::SchemaBase::FIELD_ID_MMATTR_ACTIVITY_PLAYERS;

            unsigned nPlayers = 0;
            const u32* pNumber = pQuery->m_Results[j].m_SessionConfig.m_Attrs.GetValueById(nFieldID);
            if(pNumber)
                nPlayers = *pNumber;

            gnetDebug1("\tCandidate %d hosted by %s. Token: 0x%016" I64FMT "x, ID: 0x%016" I64FMT "x, Mode: %d, SessionPurpose: %s, Players: %d (%d) / %d. Session is %s", 
                       j+1, 
                       pQuery->m_Results[j].m_HostName, 
                       nSessionToken.m_Value, 
                       pQuery->m_Results[j].m_SessionConfig.m_SessionId,
					   pQuery->m_Results[j].m_SessionConfig.m_Attrs.GetGameMode(),
                       NetworkUtils::GetSessionPurposeAsString(static_cast<SessionPurpose>(pQuery->m_Results[j].m_SessionConfig.m_Attrs.GetSessionPurpose())),
                       pQuery->m_Results[j].m_NumFilledPublicSlots + pQuery->m_Results[j].m_NumFilledPrivateSlots, 
                       nPlayers,
                       pQuery->m_Results[j].m_SessionConfig.m_MaxPublicSlots + pQuery->m_Results[j].m_SessionConfig.m_MaxPrivateSlots, 
                       IsSessionBlacklisted(nSessionToken) ? "Blacklisted" : (HasSessionJoinFailed(nSessionToken) ? "Failed to Join" : "Available"));

			// if we have information
			if(pQuery->m_Results[j].m_SessionInfoMineDataSize > 0)
			{
				sMatchmakingInfoMine* pInfo = reinterpret_cast<sMatchmakingInfoMine*>(pQuery->m_Results[j].m_SessionInfoMineData);
				unsigned nSessionAge = static_cast<u32>(rlGetPosixTime()) - pInfo->m_nSessionStartedPosix;
				netMmInfoDebug("Matchmaking InfoMine: Host: %s, Index: %d, Token: 0x%016" I64FMT "x, ", pQuery->m_Results[j].m_HostName, j+1, nSessionToken.m_Value);
				netMmInfoDebug("\tSession Entry Count: %u", pInfo->m_nSessionEntryCount);
				netMmInfoDebug("\tSession Entry Time Taken: %us", pInfo->m_SessionTimeTaken_s);
				netMmInfoDebug("\tSession Entry Time Taken (Rolling Average): %us", pInfo->m_SessionTimeTaken_s_Rolling);
				netMmInfoDebug("\tSession Players On Entry: %u", pInfo->m_nPlayersOnEntry);
				netMmInfoDebug("\tSession Players On Entry (Rolling Average): %u", pInfo->m_nPlayersOnEntry_Rolling);
				netMmInfoDebug("\tSession Age: %us (Started: %u)", nSessionAge, pInfo->m_nSessionStartedPosix);
				netMmInfoDebug("\tMatchmaking Time Taken: %us", pInfo->m_MatchmakingTimeTaken_s);
				netMmInfoDebug("\tMatchmaking Attempts: %u", pInfo->m_nMatchmakingAttempts);
				netMmInfoDebug("\tMatchmaking Sc Matchmaking Count: %u", pInfo->m_nMatchmakingScMmCount);
				netMmInfoDebug("\tMatchmaking Sc Matchmaking Player Count Avg: %u", pInfo->m_nMatchmakingScMmPlayerAvg);
				netMmInfoDebug("\tMatchmaking Matchmaking Count: %u", pInfo->m_nMatchmakingTotalCount);
				netMmInfoDebug("\tMatchmaking Matchmaking Player Count Avg: %u", pInfo->m_nMatchmakingTotalPlayerAvg);
				netMmInfoDebug("\tMatchmaking Total Count: %us", pInfo->m_nMatchmakingCount);
				netMmInfoDebug("\tMatchmaking Time Taken (Rolling Average): %us", pInfo->m_MatchmakingTimeTaken_s_Rolling);
				netMmInfoDebug("\tMatchmaking Attempts (Rolling Average): %u", pInfo->m_nMatchmakingAttempts_Rolling);
				netMmInfoDebug("\tMatchmaking Sc Matchmaking Count (Rolling Average): %u", pInfo->m_nMatchmakingScMmCount_Rolling);
				netMmInfoDebug("\tMatchmaking Sc Matchmaking Player Count Avg (Rolling Average): %u", pInfo->m_nMatchmakingScMmPlayerAvg_Rolling);
				netMmInfoDebug("\tMatchmaking Matchmaking Count (Rolling Average): %u", pInfo->m_nMatchmakingTotalCount_Rolling);
				netMmInfoDebug("\tMatchmaking Matchmaking Player Count Avg (Rolling Average): %u", pInfo->m_nMatchmakingTotalPlayerAvg_Rolling);
				netMmInfoDebug("\tSession Players Added: %u", pInfo->m_nSessionPlayersAdded);
				netMmInfoDebug("\tSession Players Removed: %u", pInfo->m_nSessionPlayersRemoved);
				netMmInfoDebug("\tSession Players Added p/s: %.2f", (pInfo->m_nSessionPlayersAdded > 0) ? (static_cast<float>(nSessionAge) / static_cast<float>(pInfo->m_nSessionPlayersAdded)) : 0);
				netMmInfoDebug("\tSession Players Removed p/s: %.2f", (pInfo->m_nSessionPlayersRemoved > 0) ? (static_cast<float>(nSessionAge) / static_cast<float>(pInfo->m_nSessionPlayersRemoved)) : 0);
				netMmInfoDebug("\tSession Last Player Added: %us ago (Posix: %u)", static_cast<u32>(rlGetPosixTime()) - pInfo->m_LastPlayerAddedPosix, pInfo->m_LastPlayerAddedPosix);
				netMmInfoDebug("\tSession Times Queried: %u", pInfo->m_TimesQueried);
				netMmInfoDebug("\tSession Last Time Queried: %us ago (Posix: %u)", static_cast<u32>(rlGetPosixTime()) - pInfo->m_LastQueryPosix, pInfo->m_LastQueryPosix);
				netMmInfoDebug("\tQueries p/s: %.2f", (pInfo->m_TimesQueried > 0) ? (static_cast<float>(nSessionAge) / static_cast<float>(pInfo->m_TimesQueried)) : 0);
				netMmInfoDebug("\tMatchmaking Migration Churn (Last): %u", pInfo->m_nMigrationChurn);
				netMmInfoDebug("\tMatchmaking Migration Time Taken (Last): %us", pInfo->m_nMigrationTimeTaken_s);
				netMmInfoDebug("\tMatchmaking Migration Candidate (Last): %u", pInfo->m_nMigrationCandidateIdx);
				netMmInfoDebug("\tMatchmaking Migration Count: %u", pInfo->m_nMigrationCount);
				netMmInfoDebug("\tMatchmaking Migration Churn (Rolling Average): %u", pInfo->m_nMigrationChurn_Rolling);
				netMmInfoDebug("\tMatchmaking Migration Time Taken (Rolling Average): %us", pInfo->m_nMigrationTimeTaken_s_Rolling);
				netMmInfoDebug("\tMatchmaking Migration Candidate (Rolling Average): %u", pInfo->m_nMigrationCandidateIdx_Rolling);
				netMmInfoDebug("\tMatchmaking Solo Count: %u", pInfo->m_nNumTimesSolo);
				netMmInfoDebug("\tMatchmaking Solo Reason: %u", pInfo->m_nSoloReason);
				netMmInfoDebug("\tMatchmaking Kick Count: %u", pInfo->m_nNumTimesKicked);
				netMmInfoDebug("\tMatchmaking Stall Count: %u", pInfo->m_nStallCount);
				netMmInfoDebug("\tMatchmaking Stall Count (>1s): %u", pInfo->m_nStallCountOver1s);
			}
#endif
            // increment number of candidates
            nCandidates++;
			nQueryCandidatesNum++;
        }

		// ... and sort
		if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_SortInQueryOrder))
			DoSortMatchmakingPreference(pQuery->m_pFilter, &aCandidates[nQueryCandidatesIdx], nQueryCandidatesNum, nMmFlags);
    }

    // log candidates...
    gnetDebug1("ProcessMatchmakingResults :: Have %d Candidates", nCandidates);

	// just use the first filter for now (consider assert if we don't have one)
	NetworkGameFilter* pFilter = (nQueries > 0) ? pQueries[0].m_pFilter : nullptr; 

    // ... and sort
	if(!CheckFlag(nMmFlags, MatchmakingFlags::MMF_SortInQueryOrder))
		DoSortMatchmakingPreference(pFilter, aCandidates, nCandidates, nMmFlags);

    // reset session count
	pResults->m_bHosted = false;
	pResults->m_UniqueID = nUniqueID;
	pResults->m_GameMode = pFilter ? pFilter->GetGameMode() : MultiplayerGameMode::GameMode_Freeroam;
    pResults->m_SessionType = pFilter ? pFilter->GetSessionPurpose() : SessionPurpose::SP_Freeroam;
	pResults->m_nNumCandidates = 0;
	pResults->m_nNumCandidatesAttempted = 0;

	//@@: range CNETWORKSESSION_PROCESSMATCHMAKINGRESULTS_HANDLE_BLACKLISTED {
	// add non blacklisted sessions
	//@@: location CNETWORKSESSION_PROCESSMATCHMAKINGRESULTS_ADD_MATCHMAKING_RESULTS
	AddMatchmakingResults(aCandidates, nCandidates, pResults, false);

	// if we're adding blacklisted sessions, add to the back of the list
	// we want to consider these on occasion if we have no other option (rather than host ourselves)
	if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_AllowBlacklisted))
		AddMatchmakingResults(aCandidates, nCandidates, pResults, true);
	//@@: } CNETWORKSESSION_PROCESSMATCHMAKINGRESULTS_HANDLE_BLACKLISTED
} 

void CNetworkSession::AddMatchmakingResults(sMatchmakingCandidate* pCandidates, unsigned nCandidates, sMatchmakingResults* pResults, bool bBlacklisted)
{
	// check if we already have our quota
	//@@: location CNETWORKSESSION_ADDMATCHMAKINGRESULTS_CHECK_MAX_SESSIONS
	if(pResults->m_nNumCandidates >= NETWORK_MAX_MATCHMAKING_CANDIDATES)
		return; 

	// add sessions to our join list
	for(unsigned i = 0; i < nCandidates; i++)
	{
		//@@: range CNETWORKSESSION_ADDMATCHMAKINGRESULTS_CHECK_BLACKLISTED {
		rlSessionToken nSessionToken = pCandidates[i].m_pDetail->m_SessionInfo.GetToken();
		bool bBlacklistCheck = bBlacklisted ? IsSessionBlacklisted(nSessionToken) : !IsSessionBlacklisted(nSessionToken);
		if(bBlacklistCheck && !HasSessionJoinFailed(nSessionToken))
		{
			// copy session into our list of sessions to join
			pResults->m_CandidatesToJoin[pResults->m_nNumCandidates].Clear();
			pResults->m_CandidatesToJoin[pResults->m_nNumCandidates].m_fScore = pCandidates[i].m_fScore;
			pResults->m_CandidatesToJoin[pResults->m_nNumCandidates].m_Session.Reset(pCandidates[i].m_pDetail->m_HostPeerInfo,
																					 pCandidates[i].m_pDetail->m_SessionInfo,
																					 pCandidates[i].m_pDetail->m_SessionConfig, 
																					 pCandidates[i].m_pDetail->m_HostHandle,
																					 pCandidates[i].m_pDetail->m_HostName,
																					 pCandidates[i].m_pDetail->m_NumFilledPublicSlots,
																					 pCandidates[i].m_pDetail->m_NumFilledPrivateSlots,
																					 pCandidates[i].m_pDetail->m_SessionUserData,
																					 pCandidates[i].m_pDetail->m_SessionUserDataSize,
																					 pCandidates[i].m_pDetail->m_SessionInfoMineData,
																					 pCandidates[i].m_pDetail->m_SessionInfoMineDataSize);

			// work out which queries gave this result
			for(unsigned j = 0; j < MatchmakingQueryType::MQT_Num; j++) if(pCandidates[i].m_nHits[j] > 0)
				pResults->m_CandidatesToJoin[pResults->m_nNumCandidates].m_nFromQueries |= (1 << j);

			// increment session count
			if(++pResults->m_nNumCandidates == NETWORK_MAX_MATCHMAKING_CANDIDATES)
				break;
		} 
		//@@: } CNETWORKSESSION_ADDMATCHMAKINGRESULTS_CHECK_BLACKLISTED
	}
}

void CNetworkSession::ProcessFindingState()
{
	// check queries
	bool bFinished = true; 

	// add non blacklisted sessions
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
	{
		if(m_MatchMakingQueries[i].m_bIsActive)
		{
			bFinished &= !IsMatchmakingPending(&m_MatchMakingQueries[i]);

			// platform matchmaking can run with a regional bias first
			if(!IsMatchmakingPending(&m_MatchMakingQueries[i]) && m_MatchMakingQueries[i].IsPlatform() && m_MatchMakingQueries[i].m_nResults == 0)
			{
                bool bReissue = false;

				if(m_MatchMakingQueries[i].m_pFilter->IsLanguageApplied())
                {
					gnetDebug1("ProcessFindingState - No results. Removing language bias.");
                    bReissue = true;
					m_MatchMakingQueries[i].m_pFilter->ApplyLanguage(false);
                }
				else if(m_MatchMakingQueries[i].m_pFilter->IsRegionApplied())
                {
                    gnetDebug1("ProcessFindingState - No results. Removing region bias.");
                    bReissue = true;
                    m_MatchMakingQueries[i].m_pFilter->ApplyRegion(false);  
                }

                if(bReissue)
                {
                    // run search again and reset timeout
					FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]);
                    m_nMatchmakingStarted = sysTimer::GetSystemMsTime();

                    // early exit
                    return; 
                }
			}
		}
	}

	// wait until all queries have completed before kicking through
	if(!bFinished)
    {
		const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

        // still pending - if we've disconnected from the host and haven't advanced far enough into the process, allow bail
#if !__FINAL
        if(CNetwork::GetTimeoutTime() > 0)
#endif
        {
            if(nCurrentTime - m_nMatchmakingStarted > s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking])
            {
				// log time out
				gnetWarning("ProcessFindingState :: Matchmaking timed out [%d/%dms]!", nCurrentTime - m_nMatchmakingStarted, s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking]);

                // cancel any pending queries
				for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].IsBusy())
				{
					gnetDebug1("UpdateTransitionMatchmaking :: Cancelling Query %s[%d], Results: %u", GetMatchmakingQueryTypeAsString(m_MatchMakingQueries[i].m_QueryType), i, m_MatchMakingQueries[i].m_nResults);
					netTask::Cancel(&m_MatchMakingQueries[i].m_TaskStatus);
				}

				// flag that we're finished
				bFinished = true; 
			}
		}

		// check if we should early out
		if(m_nNumQuickmatchOverallResultsBeforeEarlyOut > 0)
		{
			// tally current result count
			unsigned nResults = 0;
			unsigned nResultsSolo = 0;
			for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].m_bIsActive)
			{
				for(unsigned j = 0; j < m_MatchMakingQueries[i].m_nResults; j++)
				{
					// for social matchmaking, skip different regions in our early out checks
					unsigned nMmFlags = m_nMatchmakingFlags;
					if(m_MatchMakingQueries[i].IsSocial())
						nMmFlags |= MatchmakingFlags::MMF_SameRegionOnly;

					// only include compatible sessions in our list
					if(!IsSessionCompatible(m_MatchMakingQueries[i].m_pFilter, &m_MatchMakingQueries[i].m_Results[j], nMmFlags OUTPUT_ONLY(, false)))
						continue;

					// do not include blacklisted sessions in our list
					if(!CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_AllowBlacklisted) && IsSessionBlacklisted(m_MatchMakingQueries[i].m_Results[j].m_SessionInfo.GetToken()))
						continue;

					// do not include join failure sessions in our list
					if(HasSessionJoinFailed(m_MatchMakingQueries[i].m_Results[j].m_SessionInfo.GetToken()))
						continue;

					// valid session
					nResults++;

					// tally solo sessions
					if((m_MatchMakingQueries[i].m_Results[j].m_SessionConfig.m_MaxPublicSlots + m_MatchMakingQueries[i].m_Results[j].m_SessionConfig.m_MaxPrivateSlots) <= 1)
						nResultsSolo++;
				}
			}

			// if we breach our threshold, consider matchmaking complete
			// check our results aren't polluted with a high volume of solo sessions
			if((nResults >= m_nNumQuickmatchOverallResultsBeforeEarlyOut) && ((nResults - nResultsSolo) >= m_nNumQuickmatchOverallResultsBeforeEarlyOutNonSolo))
			{
				// log that we're done
				gnetDebug1("ProcessFindingState :: Matchmaking has %u results (%u required). Earlying out - Time: %dms!", nResults, m_nNumQuickmatchOverallResultsBeforeEarlyOut, nCurrentTime - m_nMatchmakingStarted);

				// cancel any pending queries
				for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].IsBusy())
					netTask::Cancel(&m_MatchMakingQueries[i].m_TaskStatus);

				// flag that we're finished
				bFinished = true;
			}
		}

		// if we're still not finished, bail
		if(!bFinished)
			return;
    }

	// make a note of when we got these results
	m_GameMMResults.m_MatchmakingRetrievedTime = sysTimer::GetSystemMsTime();

	// clear out blacklisted sessions
	ClearBlacklistedSessions();

	// process matchmaking queries
	ProcessMatchmakingResults(m_MatchMakingQueries, MatchmakingQueryType::MQT_Num, &m_GameMMResults, m_nMatchmakingFlags, m_nUniqueMatchmakingID[SessionType::ST_Physical]);

	// fill in data mine data
	m_MatchmakingInfoMine.ClearMatchmaking();
	m_MatchmakingInfoMine.m_MatchmakingTimeTaken_s = static_cast<u8>(((m_nEnterMultiplayerTimestamp > 0) ? (sysTimer::GetSystemMsTime() - m_nEnterMultiplayerTimestamp) : 0) / 1000);

	// grab session count data
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
	{
		if(m_MatchMakingQueries[i].m_bIsActive)
		{
			m_MatchmakingInfoMine.m_nMatchmakingTotalCount += static_cast<u8>(m_MatchMakingQueries[i].m_nResults);
			if(i == MatchmakingQueryType::MQT_Standard)
				m_MatchmakingInfoMine.m_nMatchmakingScMmCount += static_cast<u8>(m_MatchMakingQueries[i].m_nResults);

			for(unsigned j = 0; j < m_MatchMakingQueries[i].m_nResults; j++)
			{
				u8 nPlayerCount = static_cast<u8>(m_MatchMakingQueries[i].m_Results[j].m_NumFilledPublicSlots + m_MatchMakingQueries[i].m_Results[j].m_NumFilledPrivateSlots);
				m_MatchmakingInfoMine.m_nMatchmakingTotalPlayerAvg += nPlayerCount;
				if(i == MatchmakingQueryType::MQT_Standard)
					m_MatchmakingInfoMine.m_nMatchmakingScMmPlayerAvg += nPlayerCount;
			}
		}
	}

	if(m_MatchmakingInfoMine.m_nMatchmakingScMmCount > 0)
		m_MatchmakingInfoMine.m_nMatchmakingScMmPlayerAvg /= m_MatchmakingInfoMine.m_nMatchmakingScMmCount;
	if(m_MatchmakingInfoMine.m_nMatchmakingTotalCount > 0)
		m_MatchmakingInfoMine.m_nMatchmakingTotalPlayerAvg /= m_MatchmakingInfoMine.m_nMatchmakingTotalCount;
	
	// cumulative matchmaking data
	if(m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nMatchmakingCount))
	{
		m_MatchmakingInfoMine.m_MatchmakingTimeTaken_s_Rolling = static_cast<u16>((((m_MatchmakingInfoMine.m_nMatchmakingCount - 1) * m_MatchmakingInfoMine.m_MatchmakingTimeTaken_s_Rolling) + m_MatchmakingInfoMine.m_MatchmakingTimeTaken_s) / m_MatchmakingInfoMine.m_nMatchmakingCount);
		m_MatchmakingInfoMine.m_nMatchmakingScMmCount_Rolling = static_cast<s16>((((m_MatchmakingInfoMine.m_nMatchmakingCount - 1) * m_MatchmakingInfoMine.m_nMatchmakingScMmCount_Rolling) + m_MatchmakingInfoMine.m_nMatchmakingScMmCount) / m_MatchmakingInfoMine.m_nMatchmakingCount);
		m_MatchmakingInfoMine.m_nMatchmakingScMmPlayerAvg_Rolling = static_cast<u8>((((m_MatchmakingInfoMine.m_nMatchmakingCount - 1) * m_MatchmakingInfoMine.m_nMatchmakingScMmPlayerAvg_Rolling) + m_MatchmakingInfoMine.m_nMatchmakingScMmPlayerAvg) / m_MatchmakingInfoMine.m_nMatchmakingCount);
		m_MatchmakingInfoMine.m_nMatchmakingTotalCount_Rolling = static_cast<s16>((((m_MatchmakingInfoMine.m_nMatchmakingCount - 1) * m_MatchmakingInfoMine.m_nMatchmakingTotalCount_Rolling) + m_MatchmakingInfoMine.m_nMatchmakingTotalCount) / m_MatchmakingInfoMine.m_nMatchmakingCount);
		m_MatchmakingInfoMine.m_nMatchmakingTotalPlayerAvg_Rolling = static_cast<u8>((((m_MatchmakingInfoMine.m_nMatchmakingCount - 1) * m_MatchmakingInfoMine.m_nMatchmakingTotalPlayerAvg_Rolling) + m_MatchmakingInfoMine.m_nMatchmakingTotalPlayerAvg) / m_MatchmakingInfoMine.m_nMatchmakingCount);
	}
	
	// start from the top
	m_GameMMResults.m_nCandidateToJoinIndex = 0;
	
	//@@: location CNETWORKSESSION_PROCESSFINDINGSTATE_JOIN_SESSION_OR_HOST
	JoinOrHostSession(m_GameMMResults.m_nCandidateToJoinIndex);
}

void CNetworkSession::CompleteSessionCreate()
{
	// clear this to wait for scripts to be punted
	m_WaitFramesSinglePlayerScriptCleanUpBeforeStart = 0;

	// allocate an initial roaming bubble for our local player if we are in the session on our own
	if(NetworkInterface::IsHost() && !NetworkInterface::GetLocalPlayer()->IsInRoamingBubble())
		CNetwork::GetRoamingBubbleMgr().AllocatePlayerInitialBubble(NetworkInterface::GetLocalPlayer());

	CNetwork::EnterLobby();
	SetSessionState(SS_ESTABLISHING);

	UpdateSCPresenceInfo();
}

void CNetworkSession::ProcessHostingState()
{
#if !__FINAL
	if(PARAM_netSessionBailHosting.Get())
	{
		gnetDebug1("ProcessHostingState :: Simulating eBAIL_SESSION_HOST_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_HOST_FAILED, BAIL_CTX_HOST_FAILED_CMD_LINE));
		return;
	}
#endif 

	if(m_pSessionStatus->Succeeded())
	{
		// update the session attributes
		const rlMatchingAttributes& attrs = m_Config[SessionType::ST_Physical].GetMatchingAttributes();
		m_Config[SessionType::ST_Physical].ResetAttrs(attrs);
		m_bChangeAttributesPending[SessionType::ST_Physical] = true;

		// set the host preference to the local setting
		m_nHostAimPreference = NetworkGameConfig::GetAimPreference();

		// this manages which state we go to next
		CompleteSessionCreate();
	}
	else if(!m_pSessionStatus->Pending())
	{
		if(m_nHostRetries < NUM_HOST_RETRIES)
		{
			// increment the retries
			m_nHostRetries++;

			gnetDebug1("ProcessHostingState :: Host failed. Retry %d", m_nHostRetries);
			
			// we need to take a snapshot of the config and invalidate it
			NetworkGameConfig tConfig = m_Config[SessionType::ST_Physical];
			m_Config[SessionType::ST_Physical].Clear();

            // stop the network clock, it will be started again when we retry hosting
            if(!IsTransitioning())
                CNetwork::GetNetworkClock().Stop();

			// we need to flatten the session state
			SetSessionState(SS_IDLE, true);

			// try to re-host
			HostSession(tConfig, m_nHostFlags[SessionType::ST_Physical] | HostFlags::HF_ViaRetry);
		}
		else
		{
			gnetDebug1("ProcessHostingState :: Bailing due to failure in STATE_HOSTING_SESSION");
			CNetwork::Bail(sBailParameters(BAIL_SESSION_HOST_FAILED, BAIL_CTX_HOST_FAILED_ERROR, m_pSessionStatus->GetResultCode()));
		}
	}
    else
    {
		unsigned nSessionTimeout = GetSessionTimeout();
        if(nSessionTimeout > 0)
        {
            // check if we've been sitting in this state for too long
            const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
            if((nCurrentTime - m_SessionStateTime) > nSessionTimeout)
            {
                gnetDebug1("ProcessHostingState :: Hosting timed out [%d/%dms]", nCurrentTime - m_SessionStateTime, nSessionTimeout);
                CNetwork::Bail(sBailParameters(BAIL_SESSION_HOST_FAILED, BAIL_CTX_HOST_FAILED_TIMED_OUT));
            }
        }	
    }
}

void CNetworkSession::ProcessJoiningState()
{
#if !__FINAL
	if(PARAM_netSessionBailJoining.Get() && !m_bIsJoiningViaMatchmaking)
	{
		gnetDebug1("ProcessJoiningState :: Simulating eBAIL_SESSION_JOIN_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_CMD_LINE));
		return;
	}
#endif 

	if(m_pSessionStatus->Succeeded())
	{
		// this manages which state we go to next
		CompleteSessionCreate();
	}
	else if(!m_pSessionStatus->Pending())
	{
		m_bCanRetryJoinFailure = !m_bIsJoiningViaMatchmaking;
		if(!m_bIsJoiningViaMatchmaking)
		{
			gnetWarning("ProcessJoiningState :: Failed, checking fallback");
			CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_DIRECT_NO_FALLBACK);
		}
		else 
		{
			// add to join failure sessions and try the next session
			AddJoinFailureSession(m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_Session.m_SessionInfo.GetToken());

			//@@: location CNETWORK_SESSION_PROCESSJOININGSTATE_JOIN_OR_HOST
			JoinOrHostSession(++m_GameMMResults.m_nCandidateToJoinIndex);
		}
	}
	else
	{
		// check for timeout
		unsigned nSessionTimeout = GetSessionTimeout();
		if(nSessionTimeout > 0)
		{
			const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
			if(nCurrentTime - m_SessionStateTime > nSessionTimeout)
			{
				if(m_bJoinIncomplete)
			    {
				    // try and find another session and toggle flag off
				    CheckForAnotherSessionOrBail(BAIL_JOINING_STATE_TIMED_OUT, BAIL_CTX_JOINING_STATE_TIMED_OUT_JOINING); 
				    m_bJoinIncomplete = false;
			    }
				// not established... likely service connectivity issues. 
				else if(!m_pSession->Exists())
				{
					gnetWarning("ProcessJoiningState :: Joining timed out [%d/%dms]. Session not established!", nCurrentTime - m_SessionStateTime, nSessionTimeout);
                    CheckForAnotherSessionOrBail(m_bIsJoiningViaMatchmaking ? BAIL_SESSION_FIND_FAILED : BAIL_JOINING_STATE_TIMED_OUT, m_bIsJoiningViaMatchmaking ? BAIL_CTX_FIND_FAILED_JOINING : BAIL_CTX_JOINING_STATE_TIMED_OUT_JOINING); 
				}
			}
		}
	}
}

#if !__NO_OUTPUT
void CNetworkSession::DumpEstablishingStateInformation()
{
	gnetDebug1("DumpEstablishingStateInformation :: NetworkOpen: %s", CNetwork::IsNetworkOpen() ? "True" : "False");
	if(!CNetwork::IsNetworkOpen())
		return;

	// log pending players
	unsigned                 numPendingPlayers = netInterface::GetPlayerMgr().GetNumPendingPlayers();
	const netPlayer * const *pendingPlayers    = netInterface::GetPlayerMgr().GetPendingPlayers();

	for(unsigned index = 0; index < numPendingPlayers; index++)
		gnetDebug1("DumpEstablishingStateInformation :: %s is still pending.", pendingPlayers[index]->GetLogName());

	// log pending bubble players
	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
	const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

	for(unsigned index = 0; index < numActivePlayers; index++)
	{
		const CNetGamePlayer *pActivePlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);
		if(pActivePlayer && !pActivePlayer->IsInRoamingBubble())
			gnetDebug1("DumpEstablishingStateInformation :: %s is not in a bubble.", pActivePlayer->GetLogName());
	}

	// log pending object ID players
	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *pPhysicalPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
		if(pPhysicalPlayer && NetworkInterface::GetObjectManager().GetObjectIDManager().IsWaitingForObjectIDs(pPhysicalPlayer->GetPhysicalPlayerIndex()))
			gnetDebug1("DumpEstablishingStateInformation :: Waiting on object IDs from %s.", pPhysicalPlayer->GetLogName());
	}
}
#endif

void CNetworkSession::ProcessEstablishingState()
{
	const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	if(!CanStart())
	{
		// timeout time of 0 indicates no timeouts
        // do not process while migrating
		unsigned nSessionTimeout = GetSessionTimeout();
		if(nSessionTimeout > 0 && !m_pSession->IsMigrating())
		{
			// in activity sessions - we want to kick stalling players here
			bool bHostCheck = IsHost() && (IsActivitySession() || m_bIsTransitionToGame) && s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick] > 0 && ((nCurrentTime - m_SessionStateTime) > s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick]);
			bool bClientCheck = PARAM_netSessionEnableLaunchComplaints.Get() && !IsHost() && IsActivitySession() && s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick] > 0 && ((nCurrentTime - m_nTransitionClientComplaintTimeCheck) > s_TimeoutValues[TimeoutSetting::Timeout_TransitionComplainInterval]);

#if !__NO_OUTPUT
			if(bHostCheck)
				gnetDebug1("ProcessEstablishingState :: Time (%d) exceeds transition kick (%d). Checking for players to kick.", nCurrentTime - m_SessionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_TransitionKick]);
#endif
			// reset check time for client complaints
			if(bClientCheck)
				m_nTransitionClientComplaintTimeCheck = nCurrentTime;

			if(bHostCheck || bClientCheck)
			{
				// kick pending players
				unsigned                 numPendingPlayers = netInterface::GetPlayerMgr().GetNumPendingPlayers();
				const netPlayer * const *pendingPlayers    = netInterface::GetPlayerMgr().GetPendingPlayers();

				for(unsigned index = 0; index < numPendingPlayers; index++)
				{
					if(!pendingPlayers[index]->IsLocal())
					{
						if(bHostCheck)
						{
							gnetDebug1("ProcessEstablishingState :: Kick check. %s is still pending. Kicking.", pendingPlayers[index]->GetLogName());
							KickPlayer(pendingPlayers[index], KickReason::KR_ConnectionError, 1, NetworkInterface::GetLocalGamerHandle());
							if(numPendingPlayers > netInterface::GetPlayerMgr().GetNumPendingPlayers())
							{
								--index;
								--numPendingPlayers;
							}
						}
						else if(bClientCheck)
						{
							gnetDebug1("ProcessEstablishingState :: Complaint check. %s is still pending. Sending complaint.", pendingPlayers[index]->GetLogName());
							TryPeerComplaint(pendingPlayers[index]->GetRlPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical);
						}
					}
				}

				// kick pending bubble players
				unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
				const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

				for(unsigned index = 0; index < numActivePlayers; index++)
				{
					const CNetGamePlayer* pActivePlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);

					if(pActivePlayer && !pActivePlayer->IsInRoamingBubble() && !pActivePlayer->IsLocal())
					{
						if(bHostCheck)
						{
							gnetDebug1("ProcessEstablishingState :: Kick check. %s is not in a bubble. Kicking.", pActivePlayer->GetLogName());
							KickPlayer(pActivePlayer, KickReason::KR_ConnectionError, 1, NetworkInterface::GetLocalGamerHandle());
							if(numActivePlayers > netInterface::GetNumActivePlayers())
							{
								--index;
								--numActivePlayers;
							}
						}
						else if(bClientCheck)
						{
							gnetDebug1("ProcessStartPendingState :: Complaint check. %s is not in a bubble. Sending complaint.", pActivePlayer->GetLogName());
							TryPeerComplaint(pActivePlayer->GetRlPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical);
						}
					}
				}

				// kick pending object ID players
				unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
				const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

				for(unsigned index = 0; index < numPhysicalPlayers; index++)
				{
					const CNetGamePlayer* pPhysicalPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
					if(pPhysicalPlayer && !pPhysicalPlayer->IsLocal())
					{
						if(NetworkInterface::GetObjectManager().GetObjectIDManager().IsWaitingForObjectIDs(pPhysicalPlayer->GetPhysicalPlayerIndex()))
						{
							if(bHostCheck)
							{
								gnetDebug1("ProcessEstablishingState :: Kick check. Waiting on object IDs from %s. Kicking.", pPhysicalPlayer->GetLogName());
								KickPlayer(pPhysicalPlayer, KickReason::KR_ConnectionError, 1, NetworkInterface::GetLocalGamerHandle());
								if(numPhysicalPlayers > netInterface::GetNumPhysicalPlayers())
								{
									--index;
									--numPhysicalPlayers;
								}
							}
							else if(bClientCheck)
							{
								gnetDebug1("ProcessEstablishingState :: Complaint check. Waiting on object IDs from %s. Sending complaint.", pPhysicalPlayer->GetLogName());
								TryPeerComplaint(pPhysicalPlayer->GetRlPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical);
							}
						}
						// remove any complaints for this player when fully joined
						else if(!IsHost())
						{
							if(m_PeerComplainer[SessionType::ST_Physical].IsComplaintRegistered(pPhysicalPlayer->GetRlPeerId()))
								m_PeerComplainer[SessionType::ST_Physical].UnregisterComplaint(pPhysicalPlayer->GetRlPeerId());
						}
					}
				}
			}

			// beyond the state timeout
			if((nCurrentTime - m_SessionStateTime) > nSessionTimeout)
			{
				// dump information to logs to see where the issue is
				gnetDebug1("ProcessEstablishingState :: Starting timed out [%d/%dms]. Host: %s, Activity: %s, To Game: %s", (nCurrentTime - m_SessionStateTime), nSessionTimeout, IsHost() ? "True" : "False", IsActivitySession() ? "True" : "False", m_bIsTransitionToGame ? "True" : "False");
				OUTPUT_ONLY(DumpEstablishingStateInformation();)
				CheckForAnotherSessionOrBail(BAIL_JOINING_STATE_TIMED_OUT, BAIL_CTX_JOINING_STATE_TIMED_OUT_START_PENDING);
			}
		}

		return;
	}

	// if we're in a transition to a game, check if our transition manager is in this session
	// if not, allow some time for an update to come through (their session may have migrated since the initial message)
	if(m_bIsTransitionToGame)
	{
		if(m_hTransitionHostGamer.IsValid() && !m_pSession->IsMember(m_hTransitionHostGamer))
		{
			if((nCurrentTime - m_SessionStateTime) < s_TimeoutValues[TimeoutSetting::Timeout_TransitionWaitForUpdate])
				return;
		}
	}

#if !__FINAL
	if(PARAM_netSessionBailJoinTimeout.Get())
	{
		gnetDebug1("ProcessEstablishingState :: Simulating eBAIL_JOINING_STATE_TIMED_OUT Bail!");
		CNetwork::Bail(sBailParameters(BAIL_JOINING_STATE_TIMED_OUT, BAIL_CTX_JOINING_STATE_TIMED_OUT_CMD_LINE));
		return;
	}
#endif 

	// wait for single player scripts
	if(!SendEventStartAndWaitSPScriptsCleanUp())
		return;

	// restore normal policies
	SetPeerComplainerPolicies(&m_PeerComplainer[SessionType::ST_Physical], m_pSession, POLICY_NORMAL);

    // is this the first entry
	bool bIsFirstEntry = !CNetwork::HasCalledFirstEntryStart();

#if !__NO_OUTPUT
	if(m_bTransitionStartPending)
		gnetDebug1("ProcessEstablishingState :: Launch Complete. Time taken: %d", sysTimer::GetSystemMsTime() - m_nLaunchTrackingTimestamp);
	else
		gnetDebug1("ProcessEstablishingState :: Start Complete, Time: %u", (m_nEnterMultiplayerTimestamp > 0) ? (sysTimer::GetSystemMsTime() - m_nEnterMultiplayerTimestamp) : 0);
#endif

	// all good, start the match
	CNetwork::StartMatch();

	// mark the local gamer as established and let everyone know
	m_pSession->MarkAsEstablished(NetworkInterface::GetLocalGamerHandle());
	MsgSessionEstablished msgEstablished(m_pSession->GetSessionId(), NetworkInterface::GetLocalGamerHandle());
	m_pSession->BroadcastMsg(msgEstablished, NET_SEND_RELIABLE);

	// enable matchmaking
	m_pSession->EnableMatchmakingAdvertising(NULL);

    // first entry - we want to apply clock and weather
    if(bIsFirstEntry)
    {
        // apply clock and weather settings
        if(!m_bScriptHandlingClockAndWeather)
            CNetwork::ApplyClockAndWeather(true, 0);
    }

	// fill in info mine data (except if this is a launch)
	if(!m_bTransitionStartPending && m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nSessionEntryCount))
	{
		m_MatchmakingInfoMine.m_SessionTimeTaken_s = static_cast<u8>(((m_nEnterMultiplayerTimestamp > 0) ? (sysTimer::GetSystemMsTime() - m_nEnterMultiplayerTimestamp) : 0) / 1000);
		m_MatchmakingInfoMine.m_SessionTimeTaken_s_Rolling = static_cast<u16>((((m_MatchmakingInfoMine.m_nSessionEntryCount - 1) * m_MatchmakingInfoMine.m_SessionTimeTaken_s_Rolling) + m_MatchmakingInfoMine.m_SessionTimeTaken_s) / m_MatchmakingInfoMine.m_nSessionEntryCount);
		m_MatchmakingInfoMine.m_nPlayersOnEntry = static_cast<u8>(m_pSession->GetGamerCount());
		m_MatchmakingInfoMine.m_nPlayersOnEntry_Rolling = static_cast<u16>((((m_MatchmakingInfoMine.m_nSessionEntryCount - 1) * m_MatchmakingInfoMine.m_nPlayersOnEntry_Rolling) + m_MatchmakingInfoMine.m_nPlayersOnEntry) / m_MatchmakingInfoMine.m_nSessionEntryCount);
		m_MatchmakingInfoMine.m_nSessionStartedPosix = static_cast<u32>(rlGetPosixTime());
		if(((IsHost() && CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaMatchmaking)) || m_bIsJoiningViaMatchmaking))
			m_MatchmakingInfoMine.m_nMatchmakingAttempts = static_cast<u8>(m_GameMMResults.m_nCandidateToJoinIndex);
	}

	// telemetry start
	if(IsHost())
	{
		if(!IsActivitySession())
		{
			m_TimeCreated[SessionType::ST_Physical] = rlGetPosixTime();
		}
		
		CNetworkTelemetry::SessionHosted(m_pSession->GetSessionId(),
										 m_pSession->GetSessionToken().GetValue(),
										 m_TimeCreated[SessionType::ST_Physical],
                                         static_cast<SessionPurpose>(m_pSession->GetConfig().m_Attrs.GetSessionPurpose()),
                                         m_pSession->GetMaxSlots(RL_SLOT_PUBLIC),
                                         m_pSession->GetMaxSlots(RL_SLOT_PRIVATE),
										 NetworkBaseConfig::GetMatchmakingUser(false),
										 CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaMatchmaking) ? m_nUniqueMatchmakingID[SessionType::ST_Physical] : 0);
	}
	else
	{
		CNetworkTelemetry::SessionJoined(m_pSession->GetSessionId(),
			                             m_pSession->GetSessionToken().GetValue(),
			                             m_TimeCreated[SessionType::ST_Physical],
                                         static_cast<SessionPurpose>(m_pSession->GetConfig().m_Attrs.GetSessionPurpose()),
                                         m_pSession->GetMaxSlots(RL_SLOT_PUBLIC),
                                         m_pSession->GetMaxSlots(RL_SLOT_PRIVATE),
										 NetworkBaseConfig::GetMatchmakingUser(false),
										 m_bIsJoiningViaMatchmaking ? m_nUniqueMatchmakingID[SessionType::ST_Physical] : 0);
	}

	bool bSendMatchmakingTelemetry = (m_pSession->GetConfig().m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Freeroam) && ((IsHost() && CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaMatchmaking)) || m_bIsJoiningViaMatchmaking);
	if(bSendMatchmakingTelemetry)
		SendMatchmakingResultsTelemetry(&m_GameMMResults);

	// let script know that we've made it
	if(m_bTransitionStartPending)
	{
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_END_LAUNCH));
		m_bTransitionStartPending = false;
	}

	// update our session types for presence / telemetry
	UpdateSessionTypes();

	// advance state
	SetSessionState(SS_ESTABLISHED);
}

void CNetworkSession::ProcessHandleKickState()
{
	// wait until any operations are completed
	if(m_pSessionStatus->Pending())
		return;

    // blacklist the session
#if !__FINAL
    if(!PARAM_netSessionNoSessionBlacklisting.Get())
#endif
    {
        BlacklistedSession sSession;
        sSession.m_nSessionToken = m_pSession->GetSessionInfo().GetToken();
        sSession.m_nTimestamp = sysTimer::GetSystemMsTime();
        m_BlacklistedSessions.PushAndGrow(sSession);

        gnetDebug1("ProcessHandleKickState :: Blacklisting Session - ID: 0x%016" I64FMT "x. Time: %d", sSession.m_nSessionToken.m_Value, sSession.m_nTimestamp);
    }

    // if we should attempt to re-enter matchmaking...
    if(m_bKickCheckForAnotherSession)
        CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_KICKED);
    else
    {
        // determine the end reason
        LeaveSessionReason nLeaveReason = LeaveSessionReason::Leave_Normal;
        switch(m_LastKickReason)
        {
		case KickReason::KR_PeerComplaints:
		case KickReason::KR_ConnectionError:
		case KickReason::KR_NatType:
            nLeaveReason = LeaveSessionReason::Leave_Error;
            break;
		case KickReason::KR_VotedOut:
            nLeaveReason = LeaveSessionReason::Leave_Kicked;
            break;
		case KickReason::KR_ScAdmin:
            nLeaveReason = LeaveSessionReason::Leave_KickedAdmin;
            break;
        default:
            break;
        }

		// exit the session
		bool bSuccess = LeaveSession(LeaveFlags::LF_Default, nLeaveReason);

        // reset kick reason
		m_LastKickReason = KickReason::KR_Invalid;

        // we need to get out so bail if we were unable to leave
        if(!gnetVerify(bSuccess))
        {
            gnetError("ProcessHandleKickState :: Failed to leave match!");
            CNetwork::Bail(sBailParameters(BAIL_NETWORK_ERROR, BAIL_CTX_NETWORK_ERROR_KICKED));
        }
    }
}

bool CNetworkSession::FindSessions(sMatchmakingQuery* pQuery)
{
	gnetDebug1("FindSessions");
	
	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("FindSessions :: The network session is not initialised!");
		return false; 
	}

	// validate the query is setup / initialised correctly
	if(!ValidateMatchmakingQuery(pQuery))
		return false;

	// reset the find results
	pQuery->Reset(MatchmakingQueryType::MQT_Standard);

	// search for sessions to join
	bool bSuccess = rlSessionFinder::Find(NetworkInterface::GetLocalGamerIndex(),
                                          NetworkInterface::GetNetworkMode(),
                                          NETWORK_GAME_CHANNEL_ID,
                                          1,
                                          pQuery->m_pFilter->GetMatchingFilter(),
										  pQuery->m_Results,
										  MAX_MATCHMAKING_RESULTS,
										  m_nNumQuickmatchResultsBeforeEarlyOut,
										  &pQuery->m_nResults,
										  &pQuery->m_TaskStatus);

	if(!gnetVerify(bSuccess))
	{
		gnetError("FindSessions :: Failed to start session finder!");
		return false; 
	}

	// set tracking states
	pQuery->m_bIsActive = true;
	pQuery->m_bIsPending = true;

	// success
    return true;
}

bool CNetworkSession::FindSessionsSocial(sMatchmakingQuery* pQuery, const char* szQueryName, const char* szQueryParams)
{
	gnetDebug1("FindSessionsSocial");

	if(!gnetVerify(m_IsInitialised))
	{
		gnetError( "FindSessionsSocial :: The network session is not initialised!");
		return false; 
	}

	// validate the query is setup / initialised correctly
	if(!ValidateMatchmakingQuery(pQuery))
		return false;

	// reset the find results
	pQuery->Reset(MatchmakingQueryType::MQT_Social);

	// search for sessions to join
	bool bSuccess = rlSessionFinder::FindSocial(NetworkInterface::GetLocalGamerIndex(),
												NETWORK_GAME_CHANNEL_ID,
												szQueryName,
												szQueryParams,
												pQuery->m_Results,
												MAX_MATCHMAKING_RESULTS,
												CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch) ? m_nNumQuickmatchSocialResultsBeforeEarlyOut : m_nNumSocialResultsBeforeEarlyOut,
												&pQuery->m_nResults,
												&pQuery->m_TaskStatus);

	if(!gnetVerify(bSuccess))
	{
		gnetError("FindSessionsSocial :: Failed to start session finder!");
		return false; 
	}

	// set tracking states
	pQuery->m_bIsActive = true;
	pQuery->m_bIsPending = true;

	// copy out query and params in case we need to run this again
	snprintf(m_szSocialQuery, kMAX_SOCIAL_QUERY, "%s", szQueryName);
	snprintf(m_szSocialParams, kMAX_SOCIAL_PARAMS, "%s", szQueryParams);

	// success
	return true;
}

bool CNetworkSession::FindSessionsFriends(sMatchmakingQuery* pQuery, const bool bFriendMatchmaking)
{
	gnetDebug1("FindSessionsFriends");

	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("FindSessionsFriends :: The network session is not initialised!");
		return false; 
	}

	// validate the query is setup / initialised correctly
	if(!ValidateMatchmakingQuery(pQuery))
		return false;

	// reset the find results
	pQuery->Reset(MatchmakingQueryType::MQT_Friends);

	// get friend gamer handles
	rlGamerHandle aHandles[rlPresenceQuery::MAX_SESSIONS_TO_FIND];
	unsigned nFriends = Min(CLiveManager::GetFriendsPage()->m_NumFriends, static_cast<unsigned>(rlFriendsPage::MAX_FRIEND_PAGE_SIZE));
	unsigned nHandles = 0;
	for(unsigned i = 0; (i < nFriends) && (nHandles < rlPresenceQuery::MAX_SESSIONS_TO_FIND); i++)
	{
		CLiveManager::GetFriendsPage()->m_Friends[i].GetGamerHandle(&aHandles[nHandles]);
		if(aHandles[nHandles].IsValid())
        {
			gnetDebug1("FindSessionsFriends - Found friend %s. %s Title. %s in Session", 
				CLiveManager::GetFriendsPage()->m_Friends[i].GetName(), 
				CLiveManager::GetFriendsPage()->m_Friends[i].IsInSameTitle() ? "Same" : "Different",
				CLiveManager::GetFriendsPage()->m_Friends[i].IsInSession() ? "Is" : "Not");

			if(CLiveManager::GetFriendsPage()->m_Friends[i].IsInSameTitle() || CLiveManager::GetFriendsPage()->m_Friends[i].IsInSession())
                nHandles++;
        }
		OUTPUT_ONLY(else {gnetError("FindSessionsFriends - Invalid friend handle at index %d", i);})
	}

    // no handles... don't make the query
	if(nHandles == 0)
    {
        gnetDebug3("FindSessionsFriends :: No valid friends!");
        
        // still set this to active for the matchmaking flow 
        pQuery->m_bIsActive = true;
        return true;
    }

    static const unsigned FRIEND_MATCHMAKING_MAX_ATTEMPTS = 6;
    unsigned nMaxAttempts = bFriendMatchmaking ? FRIEND_MATCHMAKING_MAX_ATTEMPTS : 0;

	// search for sessions to join
	bool bSuccess = rlPresenceQuery::FindGamerSessions(NetworkInterface::GetLocalGamerIndex(),
													   NETWORK_GAME_CHANNEL_ID, 
													   0,
													   nMaxAttempts, 
													   bFriendMatchmaking,
													   aHandles, 
													   nHandles, 
													   pQuery->m_Results,
													   pQuery->m_nNumHits,
													   MAX_MATCHMAKING_RESULTS, 
													   CheckFlag(m_nMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch) ? m_nNumQuickmatchSocialResultsBeforeEarlyOut : m_nNumSocialResultsBeforeEarlyOut,
													   &pQuery->m_nResults, 
													   &pQuery->m_TaskStatus);

	if(!gnetVerify(bSuccess))
	{
		gnetError("FindSessionsFriends :: Failed to run presence query!");
		return false; 
	}

	// set tracking states
    pQuery->m_bCountHits = true;
    pQuery->m_bIsActive = true;
    pQuery->m_bIsPending = true;

	// success
	return true;
}

const rlSession* CNetworkSession::OnSessionQueryReceived(const rlSession* pSession, const rlGamerHandle& hGamer, rlSessionInfo* pInfo)
{
	// data mining
	if(!hGamer.IsValid() && m_pSession->Exists() && (pSession->GetSessionInfo().GetToken() == m_pSession->GetSessionInfo().GetToken()))
	{
		if(!IsActivitySession())
		{
			m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_TimesQueried);
			m_MatchmakingInfoMine.m_LastQueryPosix = static_cast<u32>(rlGetPosixTime());
		}
	}

#if RSG_PRESENCE_SESSION
	gnetDebug2("OnSessionQueryReceived :: Token: 0x%016" I64FMT "x, Handle: %s", pSession->GetSessionInfo().GetToken().m_Value, NetworkUtils::LogGamerHandle(hGamer));

	// if no gamer is specified, this session was found via matchmaking - return NULL to indicate no session override
	if(!hGamer.IsValid())
	{
		// make sure that this isn't for the presence session (we should always have a valid gamer handle)
		// if it is the presence session, assert but allow the logic below this check to run so that we are giving back a valid session
		const bool isPresenceSession = m_PresenceSession.Exists() && pSession->GetSessionInfo().GetToken() == m_PresenceSession.GetSessionInfo().GetToken();
		if(gnetVerifyf(!isPresenceSession, "OnSessionQueryReceived :: For presence session, but invalid gamer handle!"))
		{
			// returning nullptr here means that the session that was queried will be returned
			return nullptr;
		}
	}

	// for the main session
	if(m_pSession->Exists() && (pSession->GetSessionInfo().GetToken() == m_pSession->GetSessionInfo().GetToken()))
	{
		// if we are in an activity, and this is for the local gamer or someone in the same activity
		if(m_pTransition->IsEstablished() && (hGamer == NetworkInterface::GetLocalGamerHandle() || IsTransitionMember(hGamer)))
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting main. Giving back transition (0x%016" I64FMT "x)", m_pTransition->GetSessionInfo().GetToken().m_Value);
			return m_pTransition->GetRlSession();
		}

		// otherwise, if they are in another activity, we need to pass back the session info for this
		// this will be treated as a 'forward' - where the query will be rerouted to this session instead
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
		if(pPlayer && pPlayer->GetTransitionSessionInfo().IsValid())
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting main. Forwarding to (0x%016" I64FMT "x)", pPlayer->GetTransitionSessionInfo().GetToken().m_Value);
			*pInfo = pPlayer->GetTransitionSessionInfo();
		}
		else
			gnetDebug2("\tOnSessionQueryReceived :: Requesting main. No forwarding needed.");

		// no override needed
		return nullptr;
	}
	else if(m_pTransition->Exists() && (pSession->GetSessionInfo().GetToken() == m_pTransition->GetSessionInfo().GetToken()))
	{
		// will only happen when we have launched an activity and are leaving the presence enabled
		// freeroam session, now parked under the transition pointer
		// if this is for the local player or another member of the activity, give that back
		if(IsTransitionLeaving() && m_pSession->IsEstablished() && (hGamer == NetworkInterface::GetLocalGamerHandle() || IsSessionMember(hGamer)))
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting transition. Giving back main (0x%016" I64FMT "x)", m_pSession->GetSessionInfo().GetToken().m_Value);
			return m_pSession->GetRlSession();
		}

		// otherwise, no override
		gnetDebug2("\tOnSessionQueryReceived :: Requesting transition. No forwarding needed.");
	}
	else if(m_PresenceSession.Exists() && (pSession->GetSessionInfo().GetToken() == m_PresenceSession.GetSessionInfo().GetToken()))
	{
		// this replaces the old forward session system - effect is the same
		snSession* pForwardSession = GetCurrentMainSession();
		if(pForwardSession)
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting presence. Giving back (0x%016" I64FMT "x)", pForwardSession->GetSessionInfo().GetToken().m_Value);
			return pForwardSession->GetRlSession();
		}
		gnetDebug2("\tOnSessionQueryReceived :: Requesting presence. No forward session.");
	}

	// shouldn't get here
	return nullptr;
#else
	// if no gamer is specified, this session was found via matchmaking - return NULL to indicate no session override
	if(!hGamer.IsValid())
	{
		return nullptr;
	}

	// for the main session
	if(m_pSession->Exists() && (pSession->GetSessionInfo().GetToken() == m_pSession->GetSessionInfo().GetToken()))
	{
		// if we are in an activity, and this is for the local gamer or someone in the same activity
		if(m_pTransition->IsEstablished() && (hGamer == NetworkInterface::GetLocalGamerHandle() || IsTransitionMember(hGamer)))
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting main. Giving back transition (0x%016" I64FMT "x)", m_pTransition->GetSessionInfo().GetToken().m_Value);
			return m_pTransition->GetRlSession();
		}

		// otherwise, if they are in another activity, we need to pass back the session info for this
		// this will be treated as a 'forward' - where the query will be rerouted to this session instead
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
		if(pPlayer && pPlayer->GetTransitionSessionInfo().IsValid())
		{
			gnetDebug2("\tOnSessionQueryReceived :: Requesting main. Forwarding to (0x%016" I64FMT "x)", pPlayer->GetTransitionSessionInfo().GetToken().m_Value);
			*pInfo = pPlayer->GetTransitionSessionInfo();
		}
	}

	// no action needed
	return nullptr;
#endif
}

void CNetworkSession::OnGetInfoMine(const rlSession* session, rlSessionInfoMine* pInfoMine)
{
	// only for the freeroam session
	if(m_pSession->GetRlSession() == session)
	{
		sysMemCpy(pInfoMine->m_Data, &m_MatchmakingInfoMine, sizeof(sMatchmakingInfoMine));
		pInfoMine->m_SizeofData = static_cast<u16>(sizeof(sMatchmakingInfoMine));
	}
}

#if RSG_PRESENCE_SESSION

bool CNetworkSession::InitPresenceSession()
{
    // setup session owner
    snSessionOwner sSessionOwner;
	sSessionOwner.HandleJoinRequest.Bind(this, &CNetworkSession::OnHandleJoinRequestPresence);
	sSessionOwner.GetGamerData.Bind(this, &CNetworkSession::OnGetGamerDataPresence);

	// initialise session
	bool bSuccess = m_PresenceSession.Init(CLiveManager::GetRlineAllocator(),
										   sSessionOwner,
										   &CNetwork::GetConnectionManager(),
										   NETWORK_SESSION_GAME_CHANNEL_ID,
										   "gta5");

	if(!gnetVerify(bSuccess))
	{
		gnetError("InitPresenceSession :: Error initialising presence session");
		return false; 
	}

	// add delegate
	m_PresenceSession.AddDelegate(&m_PresenceSessionDlgt);

	// initialise state
	m_nPresenceState = PS_IDLE; 
	m_PresenceStatus.Reset();
	m_bHasPendingForwardSession = false;
	m_pPendingForwardSession = NULL;
    m_pForwardSession = NULL;
    m_bPresenceSessionInvitesBlocked = false;

	// indicate result
	return bSuccess;
}

void CNetworkSession::DropPresenceSession()
{
    gnetDebug1("DropPresenceSession");

    // drop and shutdown session
	m_PresenceSession.RemoveDelegate(&m_PresenceSessionDlgt);
	m_PresenceSession.Shutdown(true, -1);

    // kill any current task
    if(m_PresenceStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_PresenceStatus);
    m_PresenceStatus.Reset();

    // setup session owner
    snSessionOwner sSessionOwner;
    sSessionOwner.HandleJoinRequest.Bind(this, &CNetworkSession::OnHandleJoinRequestPresence);
    sSessionOwner.GetGamerData.Bind(this, &CNetworkSession::OnGetGamerDataPresence);

    // initialise session
    bool bSuccess = m_PresenceSession.Init(CLiveManager::GetRlineAllocator(),
                                           sSessionOwner,
                                           &CNetwork::GetConnectionManager(),
                                           NETWORK_SESSION_GAME_CHANNEL_ID,
                                           "gta5");

    if(!gnetVerify(bSuccess))
    {
        gnetError("DropPresenceSession :: Error initialising session");
        return;
    }

	// add delegate
	m_PresenceSession.AddDelegate(&m_PresenceSessionDlgt);

    m_nPresenceState = PS_IDLE; 
    m_bHasPendingForwardSession = false;
    m_pPendingForwardSession = NULL;
    m_pForwardSession = NULL;
    m_bPresenceSessionInvitesBlocked = false;
}

void CNetworkSession::OnSessionEventPresence(snSession* OUTPUT_ONLY(pSession), const snEvent* 
#if RL_NP_SUPPORT_PLAY_TOGETHER || !__NO_OUTPUT
	pEvent
#endif
)
{
	// log event
	gnetDebug1("OnSessionEvent :: Presence: 0x%016" I64FMT "x - [%08u:%u] %s", pSession->Exists() ? pSession->GetSessionInfo().GetToken().m_Value : 0, CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), pEvent->GetAutoIdNameFromId(pEvent->GetId()));

#if RL_NP_SUPPORT_PLAY_TOGETHER
	switch(pEvent->GetId())
	{
        case SNET_EVENT_SESSION_ESTABLISHED:
        case SNET_EVENT_ADDED_GAMER:
            InvitePlayTogetherGroup();
        break;

	default:
		break;
	}
#endif
}

void CNetworkSession::OnGetGamerDataPresence(snSession*, const rlGamerInfo&, snGetGamerData*) const
{
	gnetAssertf(0, "OnGetGamerDataPresence :: This should not be invoked!");
}

bool CNetworkSession::OnHandleJoinRequestPresence(snSession*, const rlGamerInfo&, snJoinRequest*)
{
	gnetAssertf(0, "OnHandleJoinRequestPresence :: Someone is trying to join the presence session!");
	return false;
}

void CNetworkSession::UpdatePresenceSession()
{
	// work out if we require a presence session
	bool bNeedPresenceSession = false; 

	// check if we should be hosting or maintaining a presence session
	if(m_pTransition->Exists() || m_pSession->Exists() || m_bLaunchingTransition || m_bIsTransitionToGame)
		bNeedPresenceSession = true;

	// host a new presence session
	if(bNeedPresenceSession)
	{
		// we prevent this if we previously failed to host / start a presence session
		// this flag is reset between sessions or when we return to single player
		if(!m_bPresenceSessionFailed && m_nPresenceState == PS_IDLE && !m_PresenceStatus.Pending())
			HostPresenceSession();
	}
	// leave if not required
	else if(m_nPresenceState == PS_ESTABLISHED)
		LeavePresenceSession();

	// manage presence session
	snSession* pPresenceSession = GetCurrentPresenceSession();
	if(pPresenceSession && pPresenceSession->IsHost())
	{
        // blocked allows us to effectively turn off Guide invite / JvP
		unsigned nPresenceFlags = 0;
        if(!m_bPresenceSessionInvitesBlocked)
		{
			if((pPresenceSession->GetEmptySlots(RL_SLOT_PUBLIC) > 0 || pPresenceSession->GetEmptySlots(RL_SLOT_PRIVATE) > 0))
				nPresenceFlags |= RL_SESSION_PRESENCE_FLAG_INVITABLE;
			if(pPresenceSession->GetEmptySlots(RL_SLOT_PUBLIC) > 0)
				nPresenceFlags |= RL_SESSION_PRESENCE_FLAG_JOIN_VIA_PRESENCE;
		}

		if(pPresenceSession->GetPresenceFlags() != nPresenceFlags)
		{
			if(!m_ModifyPresenceStatus.Pending())
			    pPresenceSession->ModifyPresenceFlags(nPresenceFlags, &m_ModifyPresenceStatus);
		}
	}

	const u32 nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	// only update this if we need to
	if(m_nPresenceState > PS_IDLE)
		m_PresenceSession.Update(nCurrentTime);

	// manage presence session flow
	switch(m_nPresenceState)
	{
	case PS_IDLE:
		break;
	case PS_HOSTING:
		if(m_PresenceStatus.Succeeded())
		{
			gnetDebug1("UpdatePresenceSession :: Hosted presence session.");
			m_nPresenceState = PS_ESTABLISHED;

            // check if we need to invite play together players
			RL_NP_PLAYTOGETHER_ONLY(InvitePlayTogetherGroup());
			
			// Move the match status from default state (WAITING) to the playing state.
			NP_CPPWEBAPI_ONLY(m_PresenceSession.SetWebApiMatchStatus(rlNpMatchStatus::Playing));
		}
		else if(!m_PresenceStatus.Pending())
		{
			// failed
			gnetError("UpdatePresenceSession :: Failed to host presence session.");
			DropPresenceSession();
			m_bPresenceSessionFailed = true; 
		}
		break;
	case PS_ESTABLISHED:
		break;
	case PS_LEAVING:
		if(m_PresenceStatus.Succeeded())
		{
			gnetDebug1("UpdatePresenceSession :: Left session.");
			m_nPresenceState = PS_DESTROYING;
			m_PresenceStatus.Reset();
			m_PresenceSession.Destroy(&m_PresenceStatus);
		}
		else if(!m_PresenceStatus.Pending())
		{
			// failed
			gnetError("UpdatePresenceSession :: Failed to leave session.");
            DropPresenceSession();
        }
		break;
	case PS_DESTROYING:
		if(m_PresenceStatus.Succeeded())
		{
			gnetDebug1("UpdatePresenceSession :: Destroyed session.");
			m_nPresenceState = PS_IDLE;
			m_PresenceStatus.Reset();
		}
		else if(!m_PresenceStatus.Pending())
		{
			// failed
			gnetError("UpdatePresenceSession :: Failed to destroy session.");
            DropPresenceSession();
        }
		break;
	default:
		break;
	}
}

bool CNetworkSession::HostPresenceSession()
{
	gnetDebug1("HostPresenceSession");

	// verify that we've initialised the session
	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("HostPresenceSession :: Not initialised!");
		return false; 
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("HostPresenceSession"))
        return false;

	// verify state
	if(!gnetVerify(m_nPresenceState == PS_IDLE))
	{
		gnetError("HostPresenceSession :: Not idle");
		return false; 
	}

	// verify that we can host
	if(!gnetVerify(snSession::CanHost(NetworkInterface::GetNetworkMode())))
	{
		gnetError("HostPresenceSession :: Cannot Host");
		return false; 
	}

	// verify that we've initialised the session
	if(!gnetVerify(!m_PresenceStatus.Pending()))
	{
		gnetError("HostPresenceSession :: Busy!");
		return false; 
	}

	// dummy data
	u8 aData[1];
	u32 nSizeOfData = 1;

#if RSG_PROSPERO
	SetDefaultPlayerSessionData();

	// copy platform attributes
	rlPlatformAttributes tPlatformAttributes = m_platformAttrs;
#endif

	// dummy attributes
	rlMatchingAttributes tMatchingAttributes;
	tMatchingAttributes.SetGameMode(MultiplayerGameMode::GameMode_Presence);
	tMatchingAttributes.SetSessionPurpose(SessionPurpose::SP_Presence);

	// not searchable
	unsigned nCreateFlags = RL_SESSION_CREATE_FLAG_MATCHMAKING_DISABLED;

#if RSG_DURANGO
	nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif

	// host the network session
	bool bSuccess = m_PresenceSession.Host(NetworkInterface::GetLocalGamerIndex(),
										   NetworkInterface::GetNetworkMode(),
										   2,
										   2,	// the local player and one other slot so that the session is 'invitable'
										   tMatchingAttributes,
										   nCreateFlags,
										   aData,
										   nSizeOfData,
										   &m_PresenceStatus);

	// verify that call was successful
	if(!gnetVerify(bSuccess))
	{
		gnetError("HostPresenceSession :: Host failed!");
		return false; 
	}

	// update state
	m_nPresenceState = PS_HOSTING;

	// indicate success
	return true;
}

bool CNetworkSession::LeavePresenceSession()
{
	// verify that we've initialised the session
	if(!gnetVerify(!m_PresenceStatus.Pending()))
	{
		gnetError("LeavePresenceSession :: Busy!");
		return false; 
	}

	gnetDebug1("LeavePresenceSession");

	// need to end the session first so that we report presence sessions to Microsoft's
	// stat collection reports correctly
	bool bSuccess = m_PresenceSession.Leave(NetworkInterface::GetLocalGamerIndex(), &m_PresenceStatus);
	if(!bSuccess)
	{
		m_PresenceStatus.SetPending();
		m_PresenceStatus.SetFailed();
	}

	// update state
	m_nPresenceState = PS_LEAVING;

	// indicate success
	return bSuccess;
}

snSession* CNetworkSession::GetCurrentPresenceSession()
{
	if(m_pSession->Exists() && m_pSession->IsPresenceEnabled() && !IsSessionLeaving())
		return m_pSession;
	else if(m_pTransition->Exists() && m_pTransition->IsPresenceEnabled() && !IsTransitionLeaving())
		return m_pTransition;
	else if(m_PresenceSession.Exists() && gnetVerify(m_PresenceSession.IsPresenceEnabled()) && (m_nPresenceState != ePresenceState::PS_LEAVING))
		return &m_PresenceSession;

	return NULL; 
}

snSession* CNetworkSession::GetCurrentMainSession()
{
	// work out which session is our currently 'active' session
    if(m_bIsTransitionToGame)
    {
        if(m_bIsTransitionToGameInSession && m_pSession->Exists())
            return m_pSession;
        
        // we are in the process of leaving this session to return to freemode
        // indicate no forward session and we'll set this when we hit the above condition
        return NULL;
    }
    else if(IsActivitySession() && m_pSession->Exists() && !IsSessionLeaving())
        return m_pSession;
    else if(m_pTransition->Exists() && !IsTransitionLeaving())
        return m_pTransition;
    else if(m_pSession->Exists() && !IsSessionLeaving())
        return m_pSession;

	return NULL;
}
#endif // RSG_PRESENCE_SESSION

#if RSG_PRESENCE_SESSION
void CNetworkSession::SetPresenceInvitesBlocked(const SessionType UNUSED_PARAM(sessionType), bool bBlocked)
{
	if(m_bPresenceSessionInvitesBlocked != bBlocked)
	{
		gnetDebug1("SetPresenceInvitesBlocked :: Invites are %s", bBlocked ? "Blocked" : "Not blocked");
		m_bPresenceSessionInvitesBlocked = bBlocked;
	}
}

bool CNetworkSession::IsJoinViaPresenceBlocked(const SessionType UNUSED_PARAM(sessionType))
{
	snSession* pPresenceSession = GetCurrentPresenceSession();
	if(pPresenceSession)
		return !pPresenceSession->IsJoinableViaPresence();
	return true;
}
#else
void CNetworkSession::SetPresenceInvitesBlocked(const SessionType sessionType, bool bBlocked)
{
	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

	if(pSession->IsHost())
	{
		if (CheckFlag(pSession->GetConfig().m_CreateFlags, RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED) || 
			CheckFlag(pSession->GetConfig().m_CreateFlags, RL_SESSION_CREATE_FLAG_JOIN_IN_PROGRESS_DISABLED))
		{
			gnetWarning("SetPresenceInvitesBlocked :: Session: %s, FAILED as this feature is disabled in the create flags", NetworkUtils::GetSessionTypeAsString(sessionType));
		}
		else if (bBlocked != IsJoinViaPresenceBlocked(sessionType))
		{
			gnetDebug1("SetPresenceInvitesBlocked :: Session: %s Invites are %s", NetworkUtils::GetSessionTypeAsString(sessionType), bBlocked ? "Blocked" : "Not blocked");

			if (!m_pSessionModifyPresenceStatus.Pending())
			{
				unsigned int presenceFlags = m_pSession->GetPresenceFlags();

				if (bBlocked)
				{
					SetFlags(m_nHostFlags[sessionType], HostFlags::HF_JoinViaPresenceDisabled);
					RemoveFlags(presenceFlags, RL_SESSION_PRESENCE_FLAG_JOIN_VIA_PRESENCE);
				}
				else
				{
					RemoveFlags(m_nHostFlags[sessionType], HostFlags::HF_JoinViaPresenceDisabled);
					SetFlags(presenceFlags, RL_SESSION_PRESENCE_FLAG_JOIN_VIA_PRESENCE);
				}

				m_pSession->ModifyPresenceFlags(presenceFlags, &m_pSessionModifyPresenceStatus);
			}
			else
			{
				gnetAssertf((bBlocked == CheckFlag(m_nHostFlags[sessionType], HostFlags::HF_JoinViaPresenceDisabled)), "SetPresenceInvitesBlocked :: Session: %s, FAILED as there is a pending transaction", NetworkUtils::GetSessionTypeAsString(sessionType));
			}
		}
	}
}

bool CNetworkSession::IsJoinViaPresenceBlocked(const SessionType sessionType)
{
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;
	return !pSession->IsJoinableViaPresence();
}
#endif // RSG_PRESENCE_SESSION

#if RSG_ORBIS || __STEAM_BUILD
void CNetworkSession::UpdatePresenceBlob()
{
	// work out what session we should be advertising to friends
	// sessions must have public slots to be advertised in this way
	// we do not advertise tournament sessions
	snSession* pSessionBlob = NULL;
	if(!CContextMenuHelper::IsPlayerContextInTournament())
	{
		if(m_pTransition->Exists() && !IsTransitionLeaving() && !m_bIsTransitionToGame && m_pTransition->GetConfig().m_MaxPublicSlots > 0)
			pSessionBlob = m_pTransition;
		else if(m_pSession->Exists() && !IsSessionLeaving() && m_pSession->GetConfig().m_MaxPublicSlots > 0)
			pSessionBlob = m_pSession;
	}

	// if we have an established session
	if(pSessionBlob ORBIS_ONLY(&& pSessionBlob->GetSessionInfo().IsValid()))
	{
		// if this is different, check 
		if(pSessionBlob->GetSessionInfo() != m_SessionInfoBlob)
		{
			gnetDebug1("UpdatePresenceBlob :: Setting Blob - Token: 0x%016" I64FMT "x", pSessionBlob->GetSessionInfo().GetToken().m_Value);

			m_SessionInfoBlob = pSessionBlob->GetSessionInfo();
			m_SessionInfoBlob.ClearNonAdvertisableData();

			u32 sessionSize;
#if RSG_ORBIS
			// export to binary, the NP Web API task encodes this to Base64 before being sent to PSN.
			// Requires a special export function to keep it under the 128 byte limit.
			char sessionBuf[rlSessionInfo::NP_PRESENCE_DATA_MAX_EXPORTED_SIZE_IN_BYTES] = {0};
			m_SessionInfoBlob.ExportForNpPresence(sessionBuf, sizeof(sessionBuf), &sessionSize);
#else
			// export to Base64 directly
			char sessionBuf[rlSessionInfo::TO_STRING_BUFFER_SIZE] = {0};
			m_SessionInfoBlob.ToString(sessionBuf, sizeof(sessionBuf), &sessionSize);
#endif
			rlPresence::SetBlob(&sessionBuf, sessionSize);
		}
	}
	else
	{
		// do we need to clear our presence blob?
		if(m_SessionInfoBlob.IsValid())
		{
			gnetDebug1("UpdatePresenceBlob :: Clearing Blob - Was: 0x%016" I64FMT "x", m_SessionInfoBlob.GetToken().m_Value);

			rlPresence::SetBlob(NULL, 0);
			m_SessionInfoBlob.Clear();
		}
	}
}
#endif

#if RSG_DURANGO
netStatus CNetworkSession::m_SessionRegisterStatus;
netStatus CNetworkSession::m_SessionDisassociateStatus;

bool CNetworkSession::RegisterPlatformPartySession(const snSession* pSession)
{
	if(!gnetVerify(pSession))
	{
		gnetError("RegisterPlatformPartySession :: Session Info is NULL!");
		return false;
	}

	if(!gnetVerify(pSession->Exists()))
	{
		gnetError("RegisterPlatformPartySession :: Session Info is not valid!");
		return false;
	}

	if(!gnetVerify(NetworkInterface::IsLocalPlayerOnline()))
	{
		gnetError("RegisterPlatformPartySession :: Local player not online!");
		return false;
	}

	if(m_SessionRegisterStatus.Pending())
	{
		gnetError("RegisterPlatformPartySession :: Status is pending!");
		return false;
	}

	if(g_rlXbl.GetPartyManager()->IsRegisteringGameSession())
	{
		gnetDebug1("RegisterPlatformPartySession :: Already registering a session.");
		return false;
	}

	if(g_rlXbl.GetPartyManager()->IsRegisteredGameSession(reinterpret_cast<const rlXblSessionHandle*>(pSession->GetSessionHandle())))
	{
		gnetDebug1("RegisterPlatformPartySession :: This session is already registered");
		return false;
	}

	gnetDebug1("RegisterPlatformPartySession :: Registering Session - ID: 0x%016" I64FMT "x. Time: %d", pSession->GetSessionInfo().GetToken().m_Value, static_cast<unsigned>(rlGetPosixTime()));
	return g_rlXbl.GetPartyManager()->RegisterGameSessionToParty(NetworkInterface::GetLocalGamerIndex(), reinterpret_cast<const rlXblSessionHandle*>(pSession->GetSessionHandle()), &m_SessionRegisterStatus);
}

bool CNetworkSession::DisassociatePlatformPartySession(const snSession* pSession)
{
	if(!gnetVerify(NetworkInterface::IsLocalPlayerOnline()))
	{
		gnetError("DisassociatePlatformPartySession :: Local player not online!");
		return false;
	}

	if(m_SessionDisassociateStatus.Pending())
	{
		gnetError("DisassociatePlatformPartySession :: Status is pending!");
		return false;
	}

	// call platform function
	bool bSucceeded = g_rlXbl.GetPartyManager()->DisassociateGameSessionFromParty(NetworkInterface::GetLocalGamerIndex(), pSession ? reinterpret_cast<const rlXblSessionHandle*>(pSession->GetSessionHandle()) : NULL, &m_SessionDisassociateStatus);
	
	// log and return result
	gnetDebug1("DisassociatePlatformPartySession :: Disassociating Session - ID: 0x%016" I64FMT "x. Succeeded: %s, Time: %d", pSession ? pSession->GetSessionInfo().GetToken().m_Value : 0, bSucceeded ? "True" : "False", static_cast<unsigned>(rlGetPosixTime()));
	return bSucceeded;
}

bool CNetworkSession::IsInPlatformPartyGameSession()
{
	// no party game session
	if(!g_rlXbl.GetPartyManager()->HasRegisteredGameSession())
		return false;

	rlXblSessionHandle hGameSession; 
	g_rlXbl.GetPartyManager()->GetRegisteredGameSession(&hGameSession);

	if(hGameSession.IsValid())
	{
		if(m_pSession->Exists() && m_pSession->GetSessionInfo().GetSessionHandle() == hGameSession)
			return true; 
		else if(m_pSession->Exists() && m_pSession->GetSessionInfo().GetSessionHandle() == hGameSession)
			return true;
	}

	// not in this session
	return false;
}

bool CNetworkSession::RemoveFromParty(bool bNotInSoloParty)
{
	if(!NetworkInterface::IsLocalPlayerSignedIn())
	{
		gnetDebug1("RemoveFromParty :: Not signed in");
		return false;
	}

	if(!g_rlXbl.GetPartyManager()->IsInParty())
	{
		gnetDebug1("RemoveFromParty :: Not in a party");
		return false;
	}

	if(bNotInSoloParty && g_rlXbl.GetPartyManager()->IsInSoloParty())
	{
		gnetDebug1("RemoveFromParty :: Not leaving solo party");
		DisassociatePlatformPartySession(NULL);
		return false;
	}

	// Pass NULL for fire and forget
	gnetDebug1("RemoveFromParty :: Removing");
	return g_rlXbl.GetPartyManager()->RemoveLocalUsersFromParty(NetworkInterface::GetLocalGamerIndex(), NULL);
}
#endif

void CNetworkSession::OnPartyChanged()
{
#if RSG_DURANGO
	if(!g_rlXbl.GetPartyManager()->IsInParty())
	{
		if((m_pTransition->Exists() && !IsTransitionLeaving()) || (m_pSession->Exists() && !IsSessionLeaving()))
		{
			gnetDebug1("OnPartyChanged :: Starting new party");
			g_rlXbl.GetPartyManager()->AddLocalUsersToParty(NetworkInterface::GetLocalGamerIndex(), NULL);
		}
	}
	else
	{
		// if we're already doing this, no need to register again
		if(m_SessionRegisterStatus.Pending())
			return;

		bool bRegisterSession = false;

		// check if we need to register a session
		if(!g_rlXbl.GetPartyManager()->HasRegisteredGameSession() && !g_rlXbl.GetPartyManager()->IsRegisteringGameSession())
			bRegisterSession = true; 
		else if(g_rlXbl.GetPartyManager()->HasRegisteredGameSession())
		{
			if(!(m_pSession->Exists() && g_rlXbl.GetPartyManager()->IsRegisteredGameSession(reinterpret_cast<const rlXblSessionHandle*>(m_pSession->GetSessionHandle()))) ||
				(m_pTransition->Exists() && g_rlXbl.GetPartyManager()->IsRegisteredGameSession(reinterpret_cast<const rlXblSessionHandle*>(m_pTransition->GetSessionHandle()))))
			{
				gnetDebug1("OnPartyChanged :: Unknown registered session!");
				if(g_rlXbl.GetPartyManager()->IsInSoloParty())
					bRegisterSession = true; 
				else if(m_bValidateGameSession)
				{
					// validate the game session
					gnetDebug1("OnPartyChanged :: Validate session");
					g_rlXbl.GetPartyManager()->ValidateGameSession(NetworkInterface::GetLocalGamerIndex());
				}
			}
		}

		if(bRegisterSession)
		{
			if(m_pTransition->Exists() && !IsTransitionLeaving() && !IsInTransitionToGame())
			{
				gnetDebug1("OnPartyChanged :: Registering Transition");
				RegisterPlatformPartySession(m_pTransition);
			}
			else if(m_pSession->Exists() && !IsSessionLeaving())
			{
				gnetDebug1("OnPartyChanged :: Registering Session");
				RegisterPlatformPartySession(m_pSession);
			}
		}
	}
#endif	//	RSG_DURANGO
}

void CNetworkSession::OnPartySessionInvalid()
{
#if RSG_DURANGO
	if(!g_rlXbl.GetPartyManager()->IsInParty())
		return;

	// if we're already doing this, no need to register again
	if(m_SessionRegisterStatus.Pending())
		return;
	
	// register a session
	if(m_pTransition->Exists() && !IsTransitionLeaving() && !IsInTransitionToGame())
	{
		gnetDebug1("OnPartyChanged :: Registering Transition");
		RegisterPlatformPartySession(m_pTransition);
	}
	else if(m_pSession->Exists() && !IsSessionLeaving())
	{
		gnetDebug1("OnPartyChanged :: Registering Session");
		RegisterPlatformPartySession(m_pSession);
	}
#endif
}

#if RSG_DURANGO
void CNetworkSession::HandlePartyActivityDialog()
{
	if(!m_bHandlePartyActivityDialog)
		return;

	// wait until clear
	if(CWarningScreen::IsActive() && !m_bShowingPartyActivityDialog)
		return;

	// show message (and flag that we are showing this)
	m_bShowingPartyActivityDialog = true;
	CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, "NT_PARTY_ACTIVITY_CHECK", FE_WARNING_YES_NO);
			
	// check result - bail if not an expected result
	eWarningButtonFlags kResult = CWarningScreen::CheckAllInput();
	if((kResult & FE_WARNING_YES_NO) == 0)
		return;

	// register activity session to party
	if(kResult == FE_WARNING_YES)
	{
		// make sure that this is still relevant
		gnetDebug1("HandlePartyActivityDialog :: Accepted - Registering Transition. Still Relevant: %s", (m_pTransition->Exists() && !IsTransitionLeaving()) ? "True" : "False");
		if(m_pTransition->Exists() && !IsTransitionLeaving())
			RegisterPlatformPartySession(m_pTransition);
	}
	else if(kResult == FE_WARNING_NO)
	{
		// this sorts itself out in OnPartyChanged handler
		gnetDebug1("HandlePartyActivityDialog :: Declined - Leaving Party");
		RemoveFromParty(false);
	}

	// clean up 
	m_bHandlePartyActivityDialog = false;
	m_bShowingPartyActivityDialog = false;
	CWarningScreen::Remove();
}
#endif

bool CNetworkSession::CanQueueForPreviousSessionJoin()
{
	return m_bCanQueue && m_hJoinQueueInviter.IsValid();
}

unsigned CNetworkSession::GetJoinQueueToken()
{
	return m_nJoinQueueToken;
}

const rlGamerHandle& CNetworkSession::GetJoinQueueGamer()
{
	return m_hJoinQueueGamer;
}

void CNetworkSession::ClearQueuedJoinRequest()
{
	if(m_nJoinQueueToken > 0)
	{
		gnetDebug2("ClearQueuedJoinRequest");
		m_nJoinQueueToken = 0;
		m_hJoinQueueGamer.Clear();
	}
}

bool CNetworkSession::SendQueuedJoinRequest()
{
	if(!CanQueueForPreviousSessionJoin())
		return false;

	// send our crew
	u64 nCrewID = CLiveManager::GetNetworkClan().HasPrimaryClan() ? CLiveManager::GetNetworkClan().GetPrimaryClan()->m_Id : RL_INVALID_CLAN_ID;

	m_nJoinQueueToken = static_cast<unsigned>(gs_Random.GetInt());
    m_hJoinQueueGamer = m_hJoinQueueInviter;
	gnetDebug2("SendQueuedJoinRequest :: Queuing for %s. Spectator: %s, CrewID: %" I64FMT "d, ConsumePrivate: %s, Token: 0x%08x", NetworkUtils::LogGamerHandle(m_hJoinQueueGamer), m_bJoinQueueAsSpectator ? "T" : "F", nCrewID, m_bJoinQueueConsumePrivate ? "T" : "F", m_nJoinQueueToken);
	CJoinQueueRequest::Trigger(m_hJoinQueueGamer, NetworkInterface::GetLocalGamerHandle(), m_bJoinQueueAsSpectator, nCrewID, m_bJoinQueueConsumePrivate, m_nJoinQueueToken);

	return true; 
}

void CNetworkSession::HandleJoinQueueInviteReply(const rlGamerHandle& hFrom, const rlSessionToken& nSessionToken, const bool bDecisionMade, const bool bDecision, const unsigned nStatusFlags)
{
	// if we're no longer in this session, do nothing
	snSession* pSession = GetSessionFromToken(nSessionToken.m_Value);
	if(!pSession)
	{
		gnetDebug2("HandleJoinQueueInviteReply :: Ignoring from %s for 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(hFrom), nSessionToken.m_Value);
		return;
	}

	// check for requests for this player
	int nQueuedIndex = -1;
	int nCount = m_QueuedJoinRequests.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_QueuedJoinRequests[i].m_hGamer == hFrom)
		{
			nQueuedIndex = i;
			break;
		}
	}

	// bail if not found
	if(nQueuedIndex == -1)
	{
		gnetDebug2("HandleJoinQueueInviteReply :: No request for %s for 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(hFrom), nSessionToken.m_Value);
		return;
	}

	// if this player is already in the session we invited them to, consider this request fulfilled
	if((nStatusFlags & InviteResponseFlags::IRF_StatusInSession) != 0)
	{
		gnetDebug2("HandleJoinQueueInviteReply :: Remove request for %s. Already in this session", NetworkUtils::LogGamerHandle(hFrom));
		m_QueuedJoinRequests.Delete(nQueuedIndex);
		return;
	}

	if(pSession->IsHost())
	{
		// extend slot reservation (will automatically happen with the ReserveSlots call)
		if(bDecisionMade && !bDecision)
		{
			gnetDebug2("HandleJoinQueueInviteReply :: Free reservation for %s for 0x%016" I64FMT "x", NetworkUtils::LogGamerHandle(hFrom), pSession->GetSessionInfo().GetToken().m_Value);
			pSession->FreeSlots(&hFrom, 1);
		}
		else
		{
			if(!(pSession->ReserveSlots(&hFrom, 1, RL_SLOT_PUBLIC, m_MaxWaitForQueuedInvite, false) || pSession->ReserveSlots(&hFrom, 1, RL_SLOT_PRIVATE, m_MaxWaitForQueuedInvite, false)))
			{
				gnetDebug2("HandleJoinQueueInviteReply :: Failed to extend reservation for %s for 0x%016" I64FMT "x", NetworkUtils::LogGamerHandle(hFrom), pSession->GetSessionInfo().GetToken().m_Value);
			}
			else
			{
				gnetDebug2("HandleJoinQueueInviteReply :: Extending reservation for %s for 0x%016" I64FMT "x", NetworkUtils::LogGamerHandle(hFrom), pSession->GetSessionInfo().GetToken().m_Value);
			}
		}
	}
	else
	{
		gnetDebug2("HandleJoinQueueInviteReply :: From %s for 0x%016" I64FMT "x. Declined: %s", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[nQueuedIndex].m_hGamer), pSession->GetSessionInfo().GetToken().m_Value, (bDecisionMade && !bDecision) ? "True" : "False");

		// not host, so ask the host to invite for us
		MsgCheckQueuedJoinRequestInviteReply msg;
		msg.Reset(m_QueuedJoinRequests[nQueuedIndex].m_hGamer, pSession->GetSessionInfo().GetToken().m_Value, bDecisionMade && !bDecision);

		u64 nHostPeerId = 0;
		pSession->GetHostPeerId(&nHostPeerId);

		// send message and increment tracking
		if(nHostPeerId != RL_INVALID_PEER_ID)
		{
			pSession->SendMsg(nHostPeerId, msg, NET_SEND_RELIABLE);
			BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
		}
	}

	// if not the auto-busy setting, remove this player
	if(bDecisionMade && !bDecision && (nStatusFlags & InviteResponseFlags::IRF_StatusInActivity) == 0)
	{
		gnetDebug2("HandleJoinQueueInviteReply :: Remove request for %s. Invite Rejected", NetworkUtils::LogGamerHandle(hFrom));
		m_QueuedJoinRequests.Delete(nQueuedIndex);
	}
	else
	{
		// reset this so that the request does not time out
		m_QueuedJoinRequests[nQueuedIndex].m_nInviteSentTimestamp = 0;
	}
}

void CNetworkSession::HandleJoinQueueUpdate(const bool bCanQueue, const unsigned nUniqueToken)
{
	if(nUniqueToken != m_nJoinQueueToken)
	{
		gnetDebug1("HandleJoinQueueUpdate :: Ignoring. MyToken: 0x%08x, Token: 0x%08x", m_nJoinQueueToken, nUniqueToken);
		return;
	}

	gnetDebug1("HandleJoinQueueUpdate :: Token: 0x%08x, CanQueue: %s", nUniqueToken, bCanQueue ? "True" : "False");
	
	if(!bCanQueue)
		ClearQueuedJoinRequest();
}

bool CNetworkSession::AddQueuedJoinRequest(const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, bool bConsumePrivate, unsigned nUniqueToken)
{
	// check if we currently have a full queue
	if(m_QueuedJoinRequests.IsFull())
	{
		gnetDebug2("AddQueuedJoinRequest :: Received from %s", NetworkUtils::LogGamerHandle(hGamer));
		CJoinQueueUpdate::Trigger(hGamer, false, nUniqueToken);
		return false;
	}

	// check we don't have this player in our queue already
	int nCount = m_QueuedJoinRequests.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_QueuedJoinRequests[i].m_hGamer == hGamer)
		{
			m_QueuedJoinRequests[i].m_nUniqueToken = nUniqueToken;
			gnetDebug2("AddQueuedJoinRequest :: Received from %s. Already Queuing - Updating Token: 0x%08x", NetworkUtils::LogGamerHandle(hGamer), nUniqueToken);
			return false;
		}
	}

	// add request
	sQueuedJoinRequest tRequest;
	tRequest.m_hGamer = hGamer;
	tRequest.m_bIsSpectator = bIsSpectator;
	tRequest.m_nCrewID = nCrewID; 
	tRequest.m_bConsumePrivate = bConsumePrivate;
	tRequest.m_nUniqueToken = nUniqueToken;
	tRequest.m_nQueuedTimestamp = sysTimer::GetSystemMsTime();
	tRequest.m_nInviteSentTimestamp = 0;
	m_QueuedJoinRequests.Push(tRequest);

	gnetDebug2("AddQueuedJoinRequest :: Received from %s - Spectator: %s, CrewID: %" I64FMT "d, ConsumePrivate: %s, Token: 0x%08x, Adding (%d Queuing)", NetworkUtils::LogGamerHandle(hGamer), bIsSpectator ? "True" : "False", nCrewID, bConsumePrivate ? "True" : "False", nUniqueToken, m_QueuedJoinRequests.GetCount());

	return false;
}

void CNetworkSession::CheckForQueuedJoinRequests()
{
	unsigned nPlayersInvited = 0;
	unsigned nSpectatorsInvited = 0;
	unsigned nCurrentTime = sysTimer::GetSystemMsTime();

	int nCount = m_QueuedJoinRequests.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		// ignore players that we have already invited
		if(m_QueuedJoinRequests[i].m_nLastInviteSentTimestamp != 0 && (nCurrentTime - m_QueuedJoinRequests[i].m_nLastInviteSentTimestamp) < m_JoinQueueInviteCooldown)
			continue;

		SessionType sessionType = SessionType::ST_None;
		unsigned* pTypeInvited = m_QueuedJoinRequests[i].m_bIsSpectator ? &nSpectatorsInvited : &nPlayersInvited;
        unsigned nReservationParameter = m_QueuedJoinRequests[i].m_bIsSpectator ? RESERVE_SPECTATOR : RESERVE_PLAYER;

        snSession* pSession = nullptr;
		if(IsTransitionEstablished() && !IsActivitySession() && !IsTransitionLeaving() && !IsInTransitionToGame())
		{
			if((GetActivityPlayerNum(m_QueuedJoinRequests[i].m_bIsSpectator) + *pTypeInvited + m_pTransition->GetNumSlotsWithParam(nReservationParameter)) < GetActivityPlayerMax(m_QueuedJoinRequests[i].m_bIsSpectator))
			{
				// check we aren't blocking queued join requests or we're not the host
				if(!m_bBlockTransitionJoinRequests && m_pTransition->IsHost())
				{
                    sessionType = SessionType::ST_Transition;
					pSession = m_pTransition;
				}
			}
		}
		else if(IsSessionEstablished() && !IsSessionLeaving())
		{
			if(( IsActivitySession() && ((GetActivityPlayerNum(m_QueuedJoinRequests[i].m_bIsSpectator) + *pTypeInvited + m_pSession->GetNumSlotsWithParam(nReservationParameter)) < GetActivityPlayerMax(m_QueuedJoinRequests[i].m_bIsSpectator))) ||
			   (!IsActivitySession() && ((GetMatchmakingGroupNum(m_QueuedJoinRequests[i].m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER) + *pTypeInvited + m_pSession->GetNumSlotsWithParam(nReservationParameter)) < GetMatchmakingGroupMax(m_QueuedJoinRequests[i].m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER)))) 
			{
				// check we aren't blocking queued join requests or we're not the host
				if(!m_bBlockJoinRequests && m_pSession->IsHost() && !m_bFreeroamIsJobToJob)
				{
                    sessionType = SessionType::ST_Physical;
					pSession = m_pSession;
				}
			}
		}

		if(pSession)
		{
			// if we move into a private session, don't send to players who were not originally invited
			if(pSession->GetMaxSlots(RL_SLOT_PRIVATE) > 0 && !m_QueuedJoinRequests[i].m_bConsumePrivate)
				continue;

			(*pTypeInvited)++;
			if(pSession->IsHost())
			{
				if(IsClosedFriendsSession(sessionType) && !rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), m_QueuedJoinRequests[i].m_hGamer))
					continue;
				if(HasCrewRestrictions(sessionType) && !CanJoinWithCrewID(sessionType, m_QueuedJoinRequests[i].m_nCrewID))
					continue;

				if(pSession->ReserveSlots(&m_QueuedJoinRequests[i].m_hGamer, 1, RL_SLOT_PUBLIC, m_QueuedInviteTimeout, false, nReservationParameter) || pSession->ReserveSlots(&m_QueuedJoinRequests[i].m_hGamer, 1, RL_SLOT_PRIVATE, m_QueuedInviteTimeout, false, nReservationParameter))
				{
					gnetDebug2("CheckForQueuedJoinRequests :: Sending invite to %s for 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[i].m_hGamer), pSession->GetSessionInfo().GetToken().m_Value);
					CGameInvite::TriggerFromJoinQueue(m_QueuedJoinRequests[i].m_hGamer, pSession->GetSessionInfo(), m_QueuedJoinRequests[i].m_nUniqueToken);
					m_QueuedJoinRequests[i].m_nInviteSentTimestamp = sysTimer::GetSystemMsTime();
					m_QueuedJoinRequests[i].m_nLastInviteSentTimestamp = m_QueuedJoinRequests[i].m_nInviteSentTimestamp;
				}
				else
				{
					gnetDebug2("CheckForQueuedJoinRequests :: Failed to reserve slot for %s in 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[i].m_hGamer), pSession->GetSessionInfo().GetToken().m_Value);
				}
			}
			else
			{
				gnetDebug3("CheckForQueuedJoinRequests :: Sending message to host for %s for 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[i].m_hGamer), pSession->GetSessionInfo().GetToken().m_Value);

				// not host, so ask the host to invite for us
				MsgCheckQueuedJoinRequest msg;
				msg.Reset(m_QueuedJoinRequests[i].m_hGamer, m_QueuedJoinRequests[i].m_bIsSpectator, m_QueuedJoinRequests[i].m_nCrewID, m_QueuedJoinRequests[i].m_nUniqueToken, pSession->GetSessionInfo().GetToken().m_Value);

				u64 nHostPeerId = 0;
				pSession->GetHostPeerId(&nHostPeerId);

				// send message and increment tracking
				pSession->SendMsg(nHostPeerId, msg, NET_SEND_RELIABLE);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
			}
		}
	}

	// update last check time
	m_LastCheckForJoinRequests = nCurrentTime;
}
					
bool CNetworkSession::RemoveQueuedJoinRequest(const rlGamerHandle& hGamer)
{
	// check we don't have one 
	int nCount = m_QueuedJoinRequests.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_QueuedJoinRequests[i].m_hGamer == hGamer)
		{
			// we prevent duplicate entries so return straight out
			gnetDebug3("RemoveQueuedJoinRequests :: Removing %s", NetworkUtils::LogGamerHandle(hGamer));
			CJoinQueueUpdate::Trigger(m_QueuedJoinRequests[i].m_hGamer, false, m_QueuedJoinRequests[i].m_nUniqueToken);
			m_QueuedJoinRequests.Delete(i);
			return true;
		}
	}
	return false;
}

void CNetworkSession::RemoveAllQueuedJoinRequests()
{
	if(!m_QueuedJoinRequests.IsEmpty())
	{
		gnetDebug3("RemoveAllQueuedJoinRequests");
		int nCount = m_QueuedJoinRequests.GetCount();
		for(int i = 0; i < nCount; i++)
			CJoinQueueUpdate::Trigger(m_QueuedJoinRequests[i].m_hGamer, false, m_QueuedJoinRequests[i].m_nUniqueToken);

		m_QueuedJoinRequests.Reset();
	}
}

void CNetworkSession::UpdateQueuedJoinRequests()
{
	unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	// issue a check every so often
	static const int CHECK_JOIN_REQUESTS_INTERVAL = 30000;
	unsigned nCheckJoinRequestsInterval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CHECK_JOIN_REQUESTS_INTERVAL", 0x76206fca), static_cast<int>(CHECK_JOIN_REQUESTS_INTERVAL));
	if((nCurrentTime - nCheckJoinRequestsInterval) > m_LastCheckForJoinRequests)
		CheckForQueuedJoinRequests();

	// reinitialise CheckForQueuedJoinRequests, which may issue invites with time
	nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	// check if we've been out of multiplayer for long enough to drop our queue
	unsigned nTimeSpentInSpToRemoveQueue = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("JOIN_QUEUE_TIME_SPENT_IN_SP_TO_REMOVE", 0xd7652e76), static_cast<int>(JOIN_QUEUE_TIME_SPENT_IN_SP_TO_REMOVE));
	if(!m_QueuedJoinRequests.IsEmpty() && m_TimeReturnedToSinglePlayer > 0 && ((nCurrentTime - m_TimeReturnedToSinglePlayer) > nTimeSpentInSpToRemoveQueue))
	{
		gnetDebug1("UpdateQueuedJoinRequests :: Spent %dms out of MP. Dropping queued join requests", (nCurrentTime - m_TimeReturnedToSinglePlayer));
		RemoveAllQueuedJoinRequests();
	}

	// check for timed out requests
	int nCount = m_QueuedJoinRequests.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_QueuedJoinRequests[i].m_nInviteSentTimestamp > 0 && (nCurrentTime - m_QueuedJoinRequests[i].m_nInviteSentTimestamp) > m_QueuedInviteTimeout)
		{
			gnetDebug2("UpdateQueuedJoinRequests :: Response from %s timed out [%d/%dms]", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[i].m_hGamer), (nCurrentTime - m_QueuedJoinRequests[i].m_nInviteSentTimestamp), m_QueuedInviteTimeout);

			// remove this request
			m_QueuedJoinRequests.Delete(i);
			--nCount;
			--i;
		}
	}
}

bool CNetworkSession::IsSessionHost(const rlSessionInfo& hInfo)
{
#if RSG_PRESENCE_SESSION
    if(m_PresenceSession.GetSessionInfo() == hInfo && m_PresenceSession.IsHost())
        return true;
#endif

    if(m_pSession->Exists() && m_pSession->GetSessionInfo() == hInfo && m_pSession->IsHost())
        return true;
    else if(m_pTransition->Exists() && m_pTransition->GetSessionInfo() == hInfo && m_pTransition->IsHost())
        return true;

    return false;
}

bool CNetworkSession::HostSession(NetworkGameConfig& tConfig, const unsigned nHostFlags)
{
	// logging
    gnetDebug1("HostSession :: Channel: %s", NetworkUtils::GetChannelAsString(m_pSession->GetChannelId()));
	
#if !__FINAL
	// simulate a failure in the host call - this is to replicate time sensitive failures
	// to verify that we're handling these correctly in script
	if(PARAM_netSessionBailAtHost.Get())
	{
		gnetDebug1("HostSession :: Simulating eBAIL_SESSION_HOST_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_HOST_FAILED, BAIL_CTX_HOST_FAILED_CMD_LINE));
		return false;
	}
#endif

	// verify that we've initialised the session
	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("HostSession :: Not initialised!");
		return false; 
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("HostSession"))
        return false;

    // verify that we have multiplayer access
    if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
    {
        gnetError("HostSession :: Cannot access multiplayer!");
        return false; 
    }

	// verify state
	if(!gnetVerify((m_SessionState == SS_IDLE) || (m_SessionState == SS_FINDING) || (m_SessionState == SS_JOINING)))
	{
		gnetError("HostSession :: Invalid State (%d)", m_SessionState);
		return false; 
	}

	// check that we have a valid config
	if(!gnetVerify(tConfig.IsValid()))
	{
		gnetError("HostSession :: Invalid config!");
		tConfig.DebugPrint(true);
		return false; 
	}

	// check that our current config has been cleared
    // this is not a blocker on the host function succeeding
	if(!gnetVerify(!m_Config[SessionType::ST_Physical].IsValid()))
	{
		gnetError("HostSession :: Config not cleared!");
	}

	// verify that we can host
	if(!gnetVerify(snSession::CanHost(NetworkInterface::GetNetworkMode())))
	{
		gnetError("HostSession :: Cannot Host");
		return false; 
	}

	// verify that we've initialised the session
	if(!gnetVerify(!IsBusy()))
	{
		gnetError("HostSession :: Busy!");
		return false; 
	}

	// verify that we're not launching
	if(!gnetVerify(!IsLaunchingTransition() && !m_bTransitionStartPending))
	{
		gnetError("HostSession :: Launching Transition!");
		return false; 
	}

	// reset blocking flag and, as host, did not join via invite / matchmaking
	m_bBlockJoinRequests = false;
	m_bJoinedViaInvite = false;
	m_bIsJoiningViaMatchmaking = false;
	m_bJoinIncomplete = false;
	m_LeaveSessionFlags = LeaveFlags::LF_Default;
	m_LeaveReason = LeaveSessionReason::Leave_Normal;
    m_nHostFlags[SessionType::ST_Physical] = HostFlags::HF_Default; //< set to nHostFlags on success below
	m_nEnterMultiplayerTimestamp = sysTimer::GetSystemMsTime();

    // reset locked visibility
    m_bLockVisibility[SessionType::ST_Physical] = false;
    m_bLockSetting[SessionType::ST_Physical] = true;

    // apply timeout
    SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_Hosting]);

    // reset retry policy
    SetSessionPolicies(POLICY_NORMAL);
    
	// open the network if it's not currently open
	if(!CNetwork::IsNetworkOpen())
	{
		// have to reset the state prior to opening the network
		SetSessionState(SS_IDLE, true);
		OpenNetwork();
	}

	// if we're coming from single player and in a transition, set local player flag
	if(IsTransitionActive() && m_pTransition->Exists())
	{
		NetworkInterface::GetLocalPlayer()->SetStartedTransition(true);
		NetworkInterface::GetLocalPlayer()->SetTransitionSessionInfo(m_pTransition->GetSessionInfo());
	}

	// reset player data
	u8 aData[256];
	u32 nSizeOfData = NetworkInterface::GetPlayerMgr().GetPlayerData(NetworkInterface::GetLocalPlayer()->GetRlGamerId(), aData, sizeof(aData));

	// log out the impending session config
	tConfig.DebugPrint(true);

	// log out to player manager
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "Hosting session", "\r\n");

	// setup presence flag
	unsigned nCreateFlags = 0;
#if RSG_PRESENCE_SESSION
	nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#endif

#if RSG_DURANGO
	if(IsTransitionActive() && !m_bIsTransitionToGame)
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif

	// for freeroam, wait until the session reaches the start state before we advertise
	if(!m_bDisableDelayedMatchmakingAdvertise)
		nCreateFlags |= RL_SESSION_CREATE_FLAG_MATCHMAKING_ADVERTISE_DISABLED;

    // without matchmaking - sessions are not searchable
    if(CheckFlag(nHostFlags, HostFlags::HF_NoMatchmaking))
	    tConfig.SetVisible(false);

	// make sure that we disable these if we're creating a solo session with extra slots
	if(m_bAllowAdminInvitesToSolo && CheckFlag(nHostFlags, HostFlags::HF_SingleplayerOnly))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#if RSG_DURANGO
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif
	}

	// disable the join via presence if requested
	if(CheckFlag(nHostFlags, HostFlags::HF_JoinViaPresenceDisabled))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_JOIN_VIA_PRESENCE_DISABLED;
	}

	// disable the join in progress if requested
	if(CheckFlag(nHostFlags, HostFlags::HF_JoinInProgressDisabled))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_JOIN_IN_PROGRESS_DISABLED;
	}

	// if this is a closed friends session, indicate that only friends can join via the platform shell
	if(CheckFlag(nHostFlags, HostFlags::HF_ClosedFriends))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_JOIN_VIA_PRESENCE_FRIENDS_ONLY;
	}
	
	// host the network session
	bool bSuccess = m_pSession->Host(NetworkInterface::GetLocalGamerIndex(),
									 NetworkInterface::GetNetworkMode(),
									 tConfig.GetMaxPublicSlots(),
									 tConfig.GetMaxPrivateSlots(),
									 tConfig.GetMatchingAttributes(),
									 nCreateFlags,
									 aData,
									 nSizeOfData,
									 m_pSessionStatus);

	// verify that call was successful
	if(!gnetVerify(bSuccess))
	{
		gnetError("HostSession :: Host failed!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_HOST_FAILED, BAIL_CTX_HOST_FAILED_ON_STARTING));
		return false;
	}

	bool bStartClock = false;
    if(IsTransitioning())
    {
		if(!gnetVerify(CNetwork::GetNetworkClock().HasStarted()))
		{
			gnetError("HostSession :: Network clock not active when hosting via transition!");
			bStartClock = true; 
		}
		else
		{
			// need to set ourselves up as clock server (if we've come from a session where
			// the transition host was not the session host)
			CNetwork::GetNetworkClock().Restart(netTimeSync::FLAG_SERVER, 0, 0);
		}
    }
    else
	{
        if(!gnetVerify(!CNetwork::GetNetworkClock().HasStarted()))
        {
            gnetError("HostSession :: Network clock is still active at an unexpected time!");
            CNetwork::GetNetworkClock().Stop();
        }

		bStartClock = true;
	}

	if(bStartClock)
	{
		// start the network timer
		CNetwork::GetNetworkClock().Start(&CNetwork::GetConnectionManager(),
			netTimeSync::FLAG_SERVER,
			NET_INVALID_ENDPOINT_ID,
			netTimeSync::GenerateToken(),
			NULL,
			NETWORK_GAME_CHANNEL_ID,
			netTimeSync::DEFAULT_INITIAL_PING_INTERVAL,
			netTimeSync::DEFAULT_FINAL_PING_INTERVAL);
	}

	// update network synced time and reset game clock to prevent it jumping at the next update
	CNetwork::SetSyncedTimeInMilliseconds(NetworkInterface::GetNetworkTime());

	// copy out the config and host flags
	m_Config[SessionType::ST_Physical] = tConfig;
    m_nHostFlags[SessionType::ST_Physical] = nHostFlags;
    OUTPUT_ONLY(LogHostFlags(m_nHostFlags[SessionType::ST_Physical], __FUNCTION__));

    // lock visibility when we specify no matchmaking
	if(CheckFlag(nHostFlags, HostFlags::HF_NoMatchmaking))
		SetSessionVisibilityLock(SessionType::ST_Physical, true, false);

	// lock matchmaking pool
	m_nSessionPool[SessionType::ST_Physical] = NetworkBaseConfig::GetMatchmakingPool(); 

	// let script know
	GetEventScriptNetworkGroup()->Add(CEventNetworkHostSession());

	// update state
	SetSessionState(SS_HOSTING);

	// indicate success
    return true;
}

bool CNetworkSession::JoinInvite()
{
	// check that we have a confirmed invite
    InviteMgr& inviteMgr = CLiveManager::GetInviteMgr();
    if(!gnetVerify(inviteMgr.HasPendingAcceptedInvite()))
    {
        gnetError("JoinInvite :: No Invite!");
        return false;
    }
	
	// check that we have a confirmed invite
	if(!gnetVerify(inviteMgr.HasConfirmedAcceptedInvite()))
	{
		gnetError("JoinInvite :: Invite Not Confirmed!");
		return false;
	}

    // clear out the wait flag
    ClearWaitFlag();

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("JoinInvite"))
        return false;

	// grab invite info
	sAcceptedInvite* pInvite = inviteMgr.GetAcceptedInvite();
	if(!gnetVerify(pInvite))
	{
		gnetError("JoinInvite :: No Invite!");
		return false;
	}

	// verify the session is valid
	if(!gnetVerify(pInvite->GetSessionInfo().IsValid()))
	{
		gnetError("JoinInvite :: SessionInfo Invalid!");
		return false;
	}

	// verify that we're not in the middle of launching
	if(!gnetVerify(!IsLaunchingTransition()))
	{
		gnetError("JoinInvite :: Launching transition!");
		return false;
	}

	// check main player vs. invited player
	const rlGamerInfo* pMainInfo = NetworkInterface::GetActiveGamerInfo();
	if(!gnetVerify(pMainInfo->GetGamerId() == pInvite->GetInvitee().GetGamerId()))
	{
		gnetError("JoinInvite :: Not the main gamer! Main: %s. Invited: %s", pMainInfo->GetName(), pInvite->GetInvitee().GetName());
		return false;
	}

    gnetDebug1("JoinInvite :: Host: %s, Token: 0x%016" I64FMT "x", pInvite->GetSessionDetail().m_HostName, pInvite->GetSessionInfo().GetToken().m_Value);

    // longer retry policy
    SetSessionPolicies(POLICY_NORMAL);

	// check the session type
	switch(pInvite->GetSessionPurpose())
	{
	case SessionPurpose::SP_Freeroam:
		{
			// check we're not already in a session
			if(!gnetVerify(!m_pSession->Exists()))
			{
				gnetError("JoinInvite :: Already in a session! Not joining!");
				return false;
			}
		}
		break;

	case SessionPurpose::SP_Activity:
		{
			// check we're not already in a transition
			if(!gnetVerify(!m_pTransition->Exists()))
			{
				gnetError("JoinInvite :: Already in a transition! Not joining!");
				return false;
			}

			// check we're not in an activity session
			if(!gnetVerify(!(m_pSession->Exists() && IsActivitySession())))
			{
				gnetError("JoinInvite :: In an activity session! Not joining!");
				return false;
			}
		}
		break;

	default:
		gnetAssertf(0, "JoinInvite :: Invalid session type!");
		break;
	}

	// check we have full use of the network heap
	if(!IsNetworkMemoryFree())
	{
		gnetDebug1("JoinInvite :: Network Memory Unavailable. Waiting.");
		m_bIsInviteProcessPendingForCommerce = true; 
	}

	// return true if we're waiting on commerce
	if(m_bIsInviteProcessPendingForCommerce)
		return true; 

	// action the invite
	return ActionInvite();
}

bool CNetworkSession::ActionInvite()
{
	// grab invite
	sAcceptedInvite* pInvite = CLiveManager::GetInviteMgr().GetAcceptedInvite();
	
	// log the inviter
	gnetDebug1("ActionInvite :: Target: %s", NetworkUtils::LogGamerHandle(pInvite->GetInviter()));

	// slot type depends on whether this was invoked by the local user
	rlSlotType kSlotType = pInvite->IsJVP() ? RL_SLOT_PUBLIC : RL_SLOT_PRIVATE;

	// apply timeout
	SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);

	// build join flags
	unsigned nJoinFlags = JoinFlags::JF_ViaInvite;
	if(CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsPrivate())
		nJoinFlags |= JoinFlags::JF_ConsumePrivate;
	if(CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsViaAdmin())
		nJoinFlags |= JoinFlags::JF_IsAdmin;
	if(!m_bNoAdminInvitesTreatedAsDev && (CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsAdminDev() || (m_bAllAdminInvitesTreatedAsDev && CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsViaAdmin())))
		nJoinFlags |= JoinFlags::JF_IsAdminDev;

	// check the session type
	bool bSuccess = false;
	switch(pInvite->GetSessionPurpose())
	{
	case SessionPurpose::SP_Freeroam:

		// flag that this is not a matchmaking join
		m_bIsJoiningViaMatchmaking = false;
        m_nGamersToGame = 0;
		bSuccess = JoinSession(pInvite->GetSessionInfo(), kSlotType, nJoinFlags);
		m_Inviter = pInvite->GetInviter();
		m_bJoinedViaInvite = bSuccess;
		break;

	case SessionPurpose::SP_Activity:

        m_nGamersToActivity = 0;
		bSuccess = JoinTransition(pInvite->GetSessionInfo(), kSlotType, nJoinFlags);
		m_TransitionInviter = pInvite->GetInviter();
		m_bJoinedTransitionViaInvite = bSuccess;
		m_nTransitionInviteFlags = static_cast<unsigned>(pInvite->GetFlags());
		break;

	default:
		gnetAssertf(0, "ActionInvite :: Invalid session type!");
		break;
	}

	if(bSuccess)
	{
		// clear out invite and fire event for script
		GetEventScriptNetworkGroup()->Add(CEventNetworkSummon(pInvite->GetGameMode(), pInvite->GetSessionPurpose()));
		CLiveManager::GetInviteMgr().ActionInvite();
		m_nEnterMultiplayerTimestamp = sysTimer::GetSystemMsTime();
	}

	return bSuccess;
}

void CNetworkSession::WaitForInvite(JoinFailureAction nJoinFailureAction, const int nJoinFailureGameMode)
{
    gnetDebug1("WaitForInvite :: JoinFailureAction: %s, GameMode: %s [%d]", GetJoinFailureActionAsString(nJoinFailureAction), NetworkUtils::GetMultiplayerGameModeIntAsString(nJoinFailureGameMode), nJoinFailureGameMode);
    m_bIsInvitePendingJoin = true;
	m_JoinFailureAction = nJoinFailureAction;
	m_JoinFailureGameMode = nJoinFailureGameMode;
}

void CNetworkSession::ClearWaitFlag()
{
    gnetDebug1("ClearWaitFlag");
    m_bIsInvitePendingJoin = false;
    m_JoinFailureAction = JoinFailureAction::JFA_None;
	m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Invalid;
}

bool CNetworkSession::JoinPreviousSession(JoinFailureAction nJoinFailureAction, bool bRunDetailQuery)
{
	gnetDebug1("JoinPreviousSession");

    // setup a failure action based on the previous session information
    if(nJoinFailureAction == JoinFailureAction::JFA_None)
    {
        nJoinFailureAction = JoinFailureAction::JFA_Quickmatch;
        if(m_bPreviousWasPrivate)
            nJoinFailureAction = (m_nPreviousMaxSlots == 1) ? JoinFailureAction::JFA_HostSolo : JoinFailureAction::JFA_HostPrivate;
    }
    
	// apply timeout
	SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);

	// call into JoinSession - we want to run a query here
	return JoinSession(m_PreviousSession, bRunDetailQuery, nJoinFailureAction, m_PreviousGameMode, RL_SLOT_PRIVATE);
}

bool CNetworkSession::JoinSession(const rlSessionInfo& hInfo, bool bRunDetailQuery, JoinFailureAction nJoinFailureAction, const int nJoinFailureGameMode, rlSlotType nSlotType)
{
    gnetDebug1("JoinSession :: Token: 0x%016" I64FMT "x, Get Detail: %s, FailureAction: %s, FailureGameMode: %s [%d]", 
		hInfo.GetToken().m_Value, 
		bRunDetailQuery ? "T" : "F", 
		GetJoinFailureActionAsString(nJoinFailureAction), 
		NetworkUtils::GetMultiplayerGameModeIntAsString(nJoinFailureGameMode),
		nJoinFailureGameMode);

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("JoinSession"))
        return false;

    // setup join process
    m_JoinFailureAction = nJoinFailureAction;
	m_JoinFailureGameMode = nJoinFailureGameMode;

    m_nJoinSessionDetail = 0;
    m_JoinSessionInfo = hInfo;
    m_JoinSlotType = nSlotType;
	m_nJoinFlags = JoinFlags::JF_Default;

	m_LeaveSessionFlags = LeaveFlags::LF_Default;
	m_LeaveReason = LeaveSessionReason::Leave_Normal;

    // assume false, run checks
    bool bDoJoinFailureAction = false;

	// check that our session info is valid
	if(!gnetVerify(hInfo.IsValid()))
	{
		gnetError("JoinSession :: Session not valid!");
		bDoJoinFailureAction = true; 
	}

	// run a detail query to see if this session still exists
	if(bRunDetailQuery && !bDoJoinFailureAction)
	{
		// check that our previous session info is valid
		if(!gnetVerify(!m_JoinDetailStatus.Pending()))
		{
			gnetError("JoinSession :: Already joining!");
			bDoJoinFailureAction = true; 
		}

		// check that this isn't a local address
		if(m_JoinSessionInfo.GetHostPeerAddress().IsLocal())
		{
			gnetWarning("JoinSession :: Attempt to query detail for a local session!");
			bDoJoinFailureAction = true; 
		}

		// if we're good to query
		if(!bDoJoinFailureAction)
		{
			// make sure status is reset
			m_JoinDetailStatus.Reset();

			// kick off detail query
			bool bSuccess = rlSessionManager::QueryDetail(RL_NETMODE_ONLINE,
														  NETWORK_SESSION_GAME_CHANNEL_ID,
														  0,
														  0,
														  0,
														  true,
														  &m_JoinSessionInfo,
														  1,
														  &m_JoinSessionDetail,
														  &m_nJoinSessionDetail,
														  &m_JoinDetailStatus);

			// check that our previous session info is valid
			if(!gnetVerify(bSuccess))
			{
				// join the session directly
				gnetError("JoinSession :: Failed to run detail query!");
				bDoJoinFailureAction = true; 
			}
			else // flag that we have a query pending
				m_bRunningDetailQuery = true; 
		}
	}
	else
	{
        // longer retry timeout
        SetSessionPolicies(POLICY_NORMAL);
		SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);

		// join the session directly
		if(!JoinSession(m_JoinSessionInfo, nSlotType, JoinFlags::JF_Default))
            bDoJoinFailureAction = true; 
	}

	// if we failed to join, kick in our action
	if(bDoJoinFailureAction)
	    DoJoinFailureAction();
	
	// success
	return true;
}

bool CNetworkSession::JoinPreviouslyFailed()
{
	gnetDebug1("JoinPreviouslyFailed :: Available: %s, Token: 0x%016" I64FMT "x", m_bCanRetryJoinFailure ? "T" : "F", m_JoinSessionInfo.GetToken().m_Value);

	// check that our session info is valid
	if(!gnetVerify(m_bCanRetryJoinFailure))
	{
		gnetError("JoinPreviouslyFailed :: Not a valid action. Only available for direct joins.");
		return false;
	}

	// check that our session info is valid
	if(!gnetVerify(m_JoinSessionInfo.IsValid()))
	{
		gnetError("JoinPreviouslyFailed :: Session not valid!");
		return false;
	}

	// use stashed information
	return JoinSession(m_JoinSessionInfo, m_JoinSlotType, m_nJoinFlags);
}

bool CNetworkSession::DoJoinFailureAction()
{
    // we want to enter the matchmaking process now
    if(m_JoinFailureAction == JoinFailureAction::JFA_None)
        return false; 

	// use free roam if we have a path that doesn't set this
	if(m_JoinFailureGameMode < 0 || m_JoinFailureGameMode == static_cast<u16>(MultiplayerGameMode::GameMode_Invalid))
		m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Freeroam;

    // log our action
    gnetDebug1("DoJoinFailureAction :: Action: %s, GameMode: %s [%d]", GetJoinFailureActionAsString(m_JoinFailureAction), NetworkUtils::GetMultiplayerGameModeIntAsString(m_JoinFailureGameMode), m_JoinFailureGameMode);

    // going to freemode - we always want these
    SetMatchmakingGroup(MM_GROUP_FREEMODER);
    AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);
    AddActiveMatchmakingGroup(MM_GROUP_SCTV);

    bool bSuccess = false;

    // do action
    switch(m_JoinFailureAction)
    {
    case JoinFailureAction::JFA_Quickmatch:
        bSuccess = DoQuickMatch(m_JoinFailureGameMode, SCHEMA_GROUPS, MAX_NUM_PHYSICAL_PLAYERS, true);
        break;
    case JoinFailureAction::JFA_HostPrivate:
        bSuccess = HostSession(m_JoinFailureGameMode, MAX_NUM_PHYSICAL_PLAYERS, HostFlags::HF_IsPrivate | HostFlags::HF_ViaJoinFailure);
        break;
    case JoinFailureAction::JFA_HostPublic:
        bSuccess = HostSession(m_JoinFailureGameMode, MAX_NUM_PHYSICAL_PLAYERS, HostFlags::HF_ViaJoinFailure);
        break;
    case JoinFailureAction::JFA_HostSolo:
        bSuccess = HostSession(m_JoinFailureGameMode, 1, HostFlags::HF_IsPrivate | HostFlags::HF_SingleplayerOnly | HostFlags::HF_ViaJoinFailure);
        break;
    default:
        gnetAssertf(0, "DoJoinFailureAction :: Invalid join action!");
        break;
    }

    // reset action / mode
    m_JoinFailureAction = JoinFailureAction::JFA_None;
	m_JoinFailureGameMode = MultiplayerGameMode::GameMode_Invalid;

    // indicate that we did something 
    return bSuccess;
}

void CNetworkSession::SetScriptValidateJoin()
{
    if(!m_bScriptValidatingJoin)
    {
	    gnetDebug3("SetScriptValidateJoin :: Expecting script to validate join");
		m_bScriptValidatingJoin = true; 
	}
}

void CNetworkSession::ValidateJoin(bool bJoinSuccessful)
{
	// check if this is flagged
	if(!gnetVerify(m_bScriptValidatingJoin))
	{
		gnetError("ValidateJoin :: Script not flagged to validate join!");
		return;
	}

	gnetDebug3("ValidateJoin :: Script Validating Join. Successful: %s", bJoinSuccessful ? "True" : "False");

	// on failure - if we are matchmaking, we want to re-enter the process, otherwise bail
	//@@: location CNETWORKSESSION_VALIDATEJOIN_ON_FAILURE_CHECK
	if(!bJoinSuccessful)
		CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_VALIDATION);
}

void CNetworkSession::SetScriptHandlingClockAndWeather(bool bScriptHandling)
{
	if(m_bScriptHandlingClockAndWeather != bScriptHandling)
	{
		gnetDebug3("SetScriptHandlingClockAndWeather :: Script %s clock and weather!", bScriptHandling ? "handling" : "not handling");
		m_bScriptHandlingClockAndWeather = bScriptHandling; 
	}
}

unsigned CNetworkSession::GetTimeInSession() const
{
	if(m_TimeSessionStarted > 0)
		return sysTimer::GetSystemMsTime() - m_TimeSessionStarted;

	return 0;
}

bool CNetworkSession::LeaveSession(const unsigned leaveFlags, const LeaveSessionReason leaveReason)
{
	gnetDebug1("LeaveSession :: %s", gs_szLeaveReason[leaveReason]);
	OUTPUT_ONLY(LogLeaveSessionFlags(leaveFlags, __FUNCTION__));

	// check that we're in a session
	if(!gnetVerify(IsSessionCreated()))
	{
		gnetError("LeaveSession :: Not in a session!");
		return false; 
	}

	// check that we're not performing another session operation
	if(!gnetVerify(!IsBusy()))
	{
		gnetError("LeaveSession :: Operation in process!");
		return false; 
	}

    // clear this out so that it doesn't persist to new sessions
    m_PendingHostPhysical = false;

	// check that we're not already leaving
	if(IsSessionLeaving())
		return false;

    // if we're bailing in the middle of a transition to game, we need to reset those parameters
    // otherwise, we'll skip a bunch of shutdown flow and leave our next session with invalid state 
    if((leaveReason != Leave_TransitionLaunch) && m_bIsTransitionToGame)
    {
        gnetDebug1("LeaveSession :: Abandoning Transition to Freeroam. Resetting transition state");
        m_bIsTransitionToGame = false;
        m_hTransitionHostGamer.Clear();
    }
  
	// retain
	m_LeaveReason = leaveReason;
	m_LeaveSessionFlags = leaveFlags;

	// add session to blacklist if required
	if((leaveFlags & LeaveFlags::LF_BlacklistSession) != 0
#if !__FINAL
		&& !PARAM_netSessionNoSessionBlacklisting.Get()
#endif
		)
	{
		BlacklistedSession sSession;
		sSession.m_nSessionToken = m_pSession->GetSessionInfo().GetToken();
		sSession.m_nTimestamp = sysTimer::GetSystemMsTime();
		m_BlacklistedSessions.PushAndGrow(sSession);

		gnetDebug1("LeaveSession :: Blacklisting Session - ID: 0x%016" I64FMT "x. Time: %d", sSession.m_nSessionToken.m_Value, sSession.m_nTimestamp);
	}
  
	// log out for reference
	if(NetworkInterface::IsNetworkOpen())
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "LEAVE_GAME", "\r\n");

	// check if we should cancel all tasks (wrap it all in tags to ignore migration failures)
	m_bIgnoreMigrationFailure[SessionType::ST_Physical] = true;
	if(m_bCancelAllTasksOnLeave)
	{
		m_pSession->CancelAllSerialTasks();
		if(m_bCancelAllConcurrentTasksOnLeave)
			m_pSession->CancelAllConcurrentTasks();
	}
	else
	{
		// cancel any migration tasks
		m_pSession->CancelMigrationTasks();
	}
	m_bIgnoreMigrationFailure[SessionType::ST_Physical] = false;

	// we also need to correctly inform Xbox One services so that the events match up
	DURANGO_ONLY(WriteMultiplayerEndEventSession(true OUTPUT_ONLY(, "LeaveSession")));

	// the active gamer is assumed signed in if not NULL
	if(!m_pSession->Leave(NetworkInterface::GetLocalGamerIndex(), m_pSessionStatus))
	{
		m_pSessionStatus->SetPending();
		m_pSessionStatus->SetFailed();
	}

	GetEventScriptNetworkGroup()->Add(CEventNetworkEndSession(leaveReason));

	// Inform our friends page that we're leaving a session, so our friend session state its not stale.
	ClearFriendSessionStatus();

	// Re-Issue a clearing of the reported DWORDs
	// This handles the scenario to which we're transitioning from a multiplayer
	// session back into a single player session
	rageSecReportCache::Clear();

	// mark as leaving and capture current session data
	SetSessionState(SS_LEAVING);
	CaptureSessionData();

	// success
    return true;
}

void CNetworkSession::ClearFriendSessionStatus()
{
	if (m_bImmediateFriendSessionRefresh)
	{
		// After leaving a session, we want to ensure the next refresh call is allowed, we get as fresh data as possible.
		rlFriendsManager::ForceAllowFriendsRefresh();

		if (NetworkInterface::HasValidRosCredentials() && rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
		{
			rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(), CLiveManager::GetFriendsPage(), 0, CPlayerListMenuDataPaginator::FRIEND_UI_PAGE_SIZE, true);
		}
	}
}

void CNetworkSession::CaptureSessionData()
{
	// grab previous session info
	m_PreviousSession = m_pSession->GetSessionInfo();
    m_bPreviousWasPrivate = (m_Config[SessionType::ST_Physical].HasPrivateSlots());
    m_nPreviousMaxSlots = m_Config[SessionType::ST_Physical].GetMaxSlots();
	m_nPreviousOccupiedSlots = m_pSession->GetGamerCount();
	m_PreviousGameMode = static_cast<int>(m_Config[SessionType::ST_Physical].GetGameMode());

	// reset started variable
	m_TimeSessionStarted = 0;

	// update presence
	UpdateSCPresenceInfo();
}

bool CNetworkSession::ReserveSlots(const SessionType sessionType, const rlGamerHandle* pHandles, const unsigned nHandles, const unsigned nReservationTime)
{
	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

	if(!(pSession->IsHostPending() || pSession->IsHost()))
	{
		gnetError("ReserveSlots :: %s - Not hosting", NetworkUtils::GetSessionTypeAsString(sessionType));
		return false;
	}

#if !__NO_OUTPUT
	for(unsigned i = 0; i < nHandles; i++)
		gnetDebug2("ReserveSlots :: %s - 0x%016" I64FMT "x, Reserving slot for %s for %ums", NetworkUtils::GetSessionTypeAsString(sessionType), pSession->GetSessionToken().m_Value, NetworkUtils::LogGamerHandle(pHandles[i]), nReservationTime);
#endif

	bool bReservedSlot = false;
	if(pSession->IsEstablished())
	{
		// try private first and then public if that fails
		bReservedSlot = pSession->ReserveSlots(pHandles, nHandles, RL_SLOT_PRIVATE, nReservationTime, false, RESERVE_PLAYER) ||
					    pSession->ReserveSlots(pHandles, nHandles, RL_SLOT_PUBLIC, nReservationTime, false, RESERVE_PLAYER);
	}
	else
	{
		gnetDebug2("ReserveSlots :: Session not established - caching to reserve later");

		// add to pending reservations
		for(unsigned i = 0; i < nHandles; i++)
		{
			if(m_PendingReservations[sessionType].m_nGamers < RL_MAX_GAMERS_PER_SESSION)
			{
				m_PendingReservations[sessionType].m_hGamers[m_PendingReservations[sessionType].m_nGamers] = pHandles[i];
				m_PendingReservations[sessionType].m_nGamers++;
			}
			else
			{
				gnetWarning("ReserveSlots :: Cannot add pending reservation for %s", NetworkUtils::LogGamerHandle(pHandles[i]));
			}
		}

		// we don't need to support different reservation times
		m_PendingReservations[sessionType].m_nReservationTime = nReservationTime;

		// we don't know the result yet, just return true
		bReservedSlot = true;
	}

	return bReservedSlot;
}

bool CNetworkSession::SendPlatformInvite(const rlSessionToken& sessionToken, const rlGamerHandle* pGamers, const unsigned nGamers)
{
	if(IsMySessionToken(sessionToken))
		return SendInvites(pGamers, nGamers);
	else if(IsMyTransitionToken(sessionToken))
		return SendTransitionInvites(pGamers, nGamers);

	return false;
}

bool CNetworkSession::AddDelayedPresenceInvite(
	const rlGamerHandle& hGamer, 
	const bool bIsImportant,
	const bool isTransition,
	const char* szContentId, 
	const char* pUniqueMatchId, 
	const int nInviteFrom, 
	const int nInviteType, 
	const unsigned nPlaylistLength,
	const unsigned nPlaylistCurrent)
{
    // check that the session is established
	if(!gnetVerify(m_pSession->Exists()))
	{
		gnetError("AddDelayedPresenceInvite :: Session not established!");
		return false;
	}

    // check that the gamer handle is valid
	if(!gnetVerify(hGamer.IsValid()))
	{
		gnetError("AddDelayedPresenceInvite :: Invalid gamer handle!");
		return false;
	} 

    if(szContentId != nullptr)
    {
        int maxDelayedInvites = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MAX_DELAYED_INVITES", 0xc856afe5), MAX_DELAYED_INVITES);

        if(maxDelayedInvites <= 0 || maxDelayedInvites > MAX_DELAYED_INVITES)
        {
            maxDelayedInvites = MAX_DELAYED_INVITES;
        }

        // if the invite details have changes send the batched invites
        if((strcmp(szContentId, m_DelayedPresenceInfo.m_ContentId) != 0) ||
           m_DelayedPresenceInfo.m_IsTransition    != isTransition     ||
           m_DelayedPresenceInfo.m_PlayListLength  != nPlaylistLength  ||
           m_DelayedPresenceInfo.m_PlayListCurrent != nPlaylistCurrent ||
           m_DelayedPresenceInfo.m_NumDelayedInvites >= maxDelayedInvites)
        {
            SendDelayedPresenceInvites();

			safecpy(m_DelayedPresenceInfo.m_ContentId, szContentId, RLUGC_MAX_CONTENTID_CHARS);
			safecpy(m_DelayedPresenceInfo.m_UniqueMatchId, pUniqueMatchId, MAX_UNIQUE_MATCH_ID_CHARS);
			m_DelayedPresenceInfo.m_InviteFrom      = nInviteFrom;
			m_DelayedPresenceInfo.m_InviteType      = nInviteType;
            m_DelayedPresenceInfo.m_IsTransition    = isTransition;
            m_DelayedPresenceInfo.m_PlayListLength  = nPlaylistLength;
            m_DelayedPresenceInfo.m_PlayListCurrent = nPlaylistCurrent;
        }

        if(gnetVerifyf(m_DelayedPresenceInfo.m_NumDelayedInvites < MAX_DELAYED_INVITES, "AddDelayedPresenceInvite :: Failed to add delayed invite!"))
        {
            m_DelayedPresenceInfo.m_hGamersToInvite[m_DelayedPresenceInfo.m_NumDelayedInvites] = hGamer;
			m_DelayedPresenceInfo.m_bIsImportant[m_DelayedPresenceInfo.m_NumDelayedInvites] = bIsImportant;
			m_DelayedPresenceInfo.m_NumDelayedInvites++;
        }
    }

    return true;
}

void CNetworkSession::SendDelayedPresenceInvites()
{
    if(m_DelayedPresenceInfo.m_NumDelayedInvites > 0)
    {
        if(m_DelayedPresenceInfo.m_IsTransition && !IsActivitySession())
        {
            SendPresenceInvitesToTransition(
				m_DelayedPresenceInfo.m_hGamersToInvite, 
				m_DelayedPresenceInfo.m_bIsImportant, 
				m_DelayedPresenceInfo.m_NumDelayedInvites, 
				m_DelayedPresenceInfo.m_ContentId,
				m_DelayedPresenceInfo.m_UniqueMatchId,
				m_DelayedPresenceInfo.m_InviteFrom,
				m_DelayedPresenceInfo.m_InviteType,
				m_DelayedPresenceInfo.m_PlayListLength, 
				m_DelayedPresenceInfo.m_PlayListCurrent);
        }
        else
        {
            SendPresenceInvites(
				m_DelayedPresenceInfo.m_hGamersToInvite, 
				m_DelayedPresenceInfo.m_bIsImportant, 
				m_DelayedPresenceInfo.m_NumDelayedInvites, 
				m_DelayedPresenceInfo.m_ContentId,
				m_DelayedPresenceInfo.m_UniqueMatchId,
				m_DelayedPresenceInfo.m_InviteFrom,
				m_DelayedPresenceInfo.m_InviteType,
				m_DelayedPresenceInfo.m_PlayListLength, 
				m_DelayedPresenceInfo.m_PlayListCurrent);
        }

        m_DelayedPresenceInfo.Reset();
    }
}

bool CNetworkSession::SendInvites(const rlGamerHandle* pGamers, const unsigned nGamers, const char* pSubject, const char* pMessage, netStatus* pStatus)
{
	// validate incoming text
	gnetAssertf(!pSubject || strlen(pSubject) < RL_MAX_INVITE_SUBJECT_CHARS, "SendInvites :: Invalid subject!");
	gnetAssertf(!pMessage || strlen(pMessage) < RL_MAX_INVITE_SALUTATION_CHARS, "SendInvites :: Invalid salutation!");

	// we use a standard header and message if these haven't been supplied
	const char* szSubject = pSubject ? pSubject : TheText.Get("INVITE_SUBJECT");
	const char* szMessage = pMessage ? pMessage : TheText.Get("INVITE_MSG");

	// send the invite
	bool bSuccess = false;
#if RSG_PRESENCE_SESSION
	snSession* pPresenceSession = GetCurrentPresenceSession();
	if(pPresenceSession)
		bSuccess = pPresenceSession->SendInvites(pGamers, nGamers, szSubject, szMessage, pStatus);
#else
	//@@: location CNETWORKSESSION_SENDINVITES
	bSuccess = m_pSession->SendInvites(pGamers, nGamers, szSubject, szMessage, pStatus);
#endif
    if(bSuccess)
        m_InvitedPlayers.AddInvites(pGamers, nGamers, CNetworkInvitedPlayers::FLAG_ALLOW_TYPE_CHANGE);

#if !__NO_OUTPUT
    gnetDebug2("SendInvites :: %s", bSuccess ? "Success" : "Failed");
	for(int i = 0; i < nGamers; i++)
        gnetDebug3("\tInvited %s", NetworkUtils::LogGamerHandle(pGamers[i]));
#endif

    return bSuccess;
}

bool CNetworkSession::SendPresenceInvites(
	const rlGamerHandle* pGamers, 
	const bool* pbImportant,
	const unsigned nGamers, 
	const char* szContentId, 
	const char* pUniqueMatchId, 
	const int nInviteFrom, 
	const int nInviteType, 
	const unsigned nPlaylistLength, 
	const unsigned nPlaylistCurrent)
{
	// check that the session is established
	if(!gnetVerify(m_pSession->Exists()))
	{
		gnetError("SendPresenceInvites :: Session not established!");
		return false;
	}

	// indicate that we need an ack if this invite is important
	unsigned nFlags = pbImportant[0] ? 0 :  InviteFlags::IF_NotTargeted;

#if RSG_DURANGO
	rlFireAndForgetTask<SendPresenceInviteTask>* pTask = nullptr;
	rlGetTaskManager()->CreateTask(&pTask);
	if(pTask)
	{
		rlTaskBase::Configure(pTask, nGamers, pGamers, m_pSession->GetSessionInfo(), szContentId, m_SessionUserDataActivity.m_hContentCreator, pUniqueMatchId, nInviteFrom, nInviteType, nPlaylistLength, nPlaylistCurrent, nFlags, &pTask->m_Status);
		rlGetTaskManager()->AddParallelTask(pTask);
	}
	else // send the invite anyway
#endif
	{
		// invite can be sent outside multiplayer so use presence
		CGameInvite::Trigger(
			pGamers,
			nGamers,
			m_pSession->GetSessionInfo(),
			szContentId,
			m_SessionUserDataActivity.m_hContentCreator,
			pUniqueMatchId,
			nInviteFrom,
			nInviteType,
			nPlaylistLength,
			nPlaylistCurrent,
			nFlags);
	}
	
	for(unsigned i = 0; i < nGamers; i++)
	{
		// check that the gamer handle is valid
		if(!gnetVerify(pGamers[i].IsValid()))
		{
			gnetError("SendPresenceInvites :: Invalid gamer handle!");
			continue;
		} 

		// invite flags 
		unsigned nFlags = CNetworkInvitedPlayers::FLAG_ALLOW_TYPE_CHANGE | CNetworkInvitedPlayers::FLAG_VIA_PRESENCE;
		if(pbImportant[i])
			nFlags |= CNetworkInvitedPlayers::FLAG_IMPORTANT;

		// log out request
		m_InvitedPlayers.AddInvite(pGamers[i], nFlags);
		gnetDebug2("SendPresenceInvites :: Inviting %s", NetworkUtils::LogGamerHandle(pGamers[i]));
	}

	return true;
}

void CNetworkSession::RemoveInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
	for(unsigned i = 0; i < nGamers; i++)
	{
		// log out request
		gnetDebug2("RemoveInvites :: Removing invite for %s", NetworkUtils::LogGamerHandle(pGamers[i]));
		m_InvitedPlayers.RemoveInvite(pGamers[i]);
	}
}

void CNetworkSession::RemoveAllInvites()
{
    gnetDebug2("RemoveAllInvites");
    m_InvitedPlayers.ClearAllInvites();
}

bool CNetworkSession::CancelInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
	// check that the session is established
	if(!gnetVerify(m_pSession->Exists()))
	{
		gnetError("CancelInvites :: Session not established!");
		return false;
	}

	unsigned int invitesToCancelCount=0;
	rlGamerHandle inviteesToCancel[MAX_INVITES_TO_CANCEL];
	for(unsigned i = 0; i < nGamers; i++)
	{
		// check that the gamer handle is valid
		if(!gnetVerify(pGamers[i].IsValid()))
		{
			gnetError("CancelInvites :: Invalid gamer handle!");
			continue;
		}

		// check we have a pending invite for this gamer
		if(!m_InvitedPlayers.DidInvite(pGamers[i]))
			continue;

		// if we sent this via presence, fire a cancel event
		if(m_InvitedPlayers.DidInviteViaPresence(pGamers[i]))
		{
			if(gnetVerifyf(invitesToCancelCount<MAX_INVITES_TO_CANCEL, "Cancelling too many invites at the same time"))
			{
				inviteesToCancel[invitesToCancelCount]=pGamers[i];
				invitesToCancelCount++;
			}
		}
		// log out request
		gnetDebug2("CancelInvites :: Removing invite for %s", NetworkUtils::LogGamerHandle(pGamers[i]));
		m_InvitedPlayers.RemoveInvite(pGamers[i]);
	}
	if(invitesToCancelCount>0)
	{
		CGameInviteCancel::Trigger(inviteesToCancel, invitesToCancelCount, m_pSession->GetSessionInfo());
	}
	return true;
}

void CNetworkSession::RemoveAndCancelAllInvites()
{
	// always clear the invites
	gnetDebug3("RemoveAndCancelAllInvites :: Exists: %s", m_pSession->Exists() ? "True" : "False");
	m_InvitedPlayers.ClearAllInvites();

	// we need a valid transition session to get the rlSessionInfo
	if(m_pSession->Exists())
	{
		unsigned int invitesToCancelCount=0;
		rlGamerHandle inviteesToCancel[MAX_INVITES_TO_CANCEL];
		int nCount = m_InvitedPlayers.GetNumInvitedPlayers();
		for(unsigned i = 0; i < nCount; i++)
		{
			// grab player
			const CNetworkInvitedPlayers::sInvitedGamer& tInvite = m_InvitedPlayers.GetInvitedPlayerFromIndex(i);

			// if we sent this via presence, fire a cancel event
			if(tInvite.IsViaPresence())
			{
				if(gnetVerifyf(invitesToCancelCount<MAX_INVITES_TO_CANCEL, "Cancelling too many invites at the same time"))
				{
					gnetDebug2("RemoveAndCancelAllInvites :: Cancelling invite for %s", NetworkUtils::LogGamerHandle(tInvite.m_hGamer));
					inviteesToCancel[invitesToCancelCount]=tInvite.m_hGamer;
					invitesToCancelCount++;
				}
			}
		}
		if(invitesToCancelCount>0)
		{
			CGameInviteCancel::Trigger(inviteesToCancel, invitesToCancelCount, m_pSession->GetSessionInfo());
		}
	}
}

const NetworkGameConfig &CNetworkSession::GetMatchConfig() const
{
	gnetAssertf(SS_IDLE != m_SessionState, "GetMatchConfig :: Accessing the match config at an unexpected time!");
    return m_Config[SessionType::ST_Physical];
}

bool CNetworkSession::IsSoloMultiplayer() const
{
	bool bIsInMultiplayer = false;
	bool bIsInNonSoloSession = false;

	if(m_SessionState > SS_IDLE)
	{
		bIsInMultiplayer = true;
		if(!IsSoloSession(SessionType::ST_Physical) || m_Config[SessionType::ST_Physical].GetMaxSlots() > 2)
			bIsInNonSoloSession = true; 
	}

	if(m_TransitionState > TS_IDLE || m_bIsTransitionMatchmaking)
	{
		bIsInMultiplayer = true;
		if(!IsSoloSession(SessionType::ST_Transition) || m_Config[SessionType::ST_Transition].GetMaxSlots() > 2)
			bIsInNonSoloSession = true;
	}

	if(!bIsInMultiplayer)
		return false;
	if(bIsInNonSoloSession)
		return false;
	if(CNetwork::GetNetworkVoiceSession().IsVoiceSessionActive())
		return false;

	// we're in multiplayer and all sessions are solo
	return true; 
}

bool CNetworkSession::AreAllSessionsNonVisible() const
{
	bool isInVisibleSession = false;
	if(m_SessionState > SS_IDLE && IsSessionVisible(SessionType::ST_Physical))
		isInVisibleSession = true;
	if(m_TransitionState > TS_IDLE && IsSessionVisible(SessionType::ST_Transition))
		isInVisibleSession = true;
	return !isInVisibleSession;
}

bool CNetworkSession::IsActivitySession() const
{
	return m_bIsActivitySession;
}

bool CNetworkSession::IsClosedSession(const SessionType sessionType) const
{
	return 
		(m_nHostFlags[sessionType] & HostFlags::HF_Closed) != 0 ||
		(m_nHostFlags[sessionType] & HostFlags::HF_ClosedFriends) != 0 ||
		(m_nHostFlags[sessionType] & HostFlags::HF_ClosedCrew) != 0;
}

bool CNetworkSession::IsClosedFriendsSession(const SessionType sessionType) const
{
    return (m_nHostFlags[sessionType] & HostFlags::HF_ClosedFriends) == HostFlags::HF_ClosedFriends;
}

bool CNetworkSession::IsClosedCrewSession(const SessionType sessionType) const
{
    return (m_nHostFlags[sessionType] & HostFlags::HF_ClosedCrew) == HostFlags::HF_ClosedCrew;
}

bool CNetworkSession::HasCrewRestrictions(const SessionType sessionType) const
{
	// true if this is a closed crew session or there are crew limits
	return
		(m_nHostFlags[sessionType] & HostFlags::HF_ClosedCrew) == HostFlags::HF_ClosedCrew ||
		(GetUniqueCrewLimit(sessionType) > 0);
}

bool CNetworkSession::IsSoloSession(const SessionType sessionType) const
{
	return CheckFlag(m_nHostFlags[sessionType], HostFlags::HF_SingleplayerOnly);
}

bool CNetworkSession::IsPrivateSession(const SessionType sessionType) const
{
	return CheckFlag(m_nHostFlags[sessionType], HostFlags::HF_IsPrivate) && (m_Config[sessionType].HasPrivateSlots());
}

eSessionVisibility CNetworkSession::GetSessionVisibility(const SessionType sessionType) const
{
	if(IsPrivateSession(sessionType))
		return VISIBILITY_PRIVATE;
	else if(IsClosedFriendsSession(sessionType))
		return VISIBILITY_CLOSED_FRIEND;
	else if(IsClosedCrewSession(sessionType))
		return VISIBILITY_CLOSED_CREW;
	return VISIBILITY_OPEN;
}

bool CNetworkSession::MarkAsGameSession()
{
	// don't bother if visibility already matches request
	if(!gnetVerify(IsActivitySession()))
	{
		gnetError("MarkAsGameSession :: Not an activity session!");
		return false;
	}

	gnetDebug1("MarkAsGameSession :: Setting back to game session");

	// flag this as not an activity session 
    m_bIsActivitySession = false;

#if RSG_PRESENCE_SESSION
	// we need to keep the presence session active, since we are not transitioning to 
	// a new session when returning to GTAO
	m_bRequirePresenceSession = true;
#endif

	// setup general session variables
	m_MatchmakingGroup = MM_GROUP_FREEMODER;
	AddActiveMatchmakingGroup(MM_GROUP_SCTV);
	AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);

    // need to update the advertised attributes - only the host can do this
    if(m_pSession->IsHost())
    {
		// the config needs set back to a game session
	    m_Config[SessionType::ST_Physical].SetSessionPurpose(SessionPurpose::SP_Freeroam);
        m_bChangeAttributesPending[SessionType::ST_Physical] = true;

		// set this to false - we cannot up the slots to match for freemode
		// this functionality will only be used to come back to freemode as staging
		// session for another transition
	    SetSessionVisibility(SessionType::ST_Physical, false);
        SetSessionVisibilityLock(SessionType::ST_Physical, true, false);

		// update the matchmaking groups 
		UpdateMatchmakingGroups();
    }

    // we need to swap channels on the session 
    if(gnetVerify(m_pSession->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID))
    {
        m_pSession->SwapChannelId(NETWORK_SESSION_GAME_CHANNEL_ID);
        m_pTransition->SwapChannelId(NETWORK_SESSION_ACTIVITY_CHANNEL_ID);
    }
    else
        gnetError("MarkAsGameSession :: Session channel is %d", m_pSession->GetChannelId());

	// success
	return true;
}

void CNetworkSession::SetSessionInProgress(const SessionType sessionType, const bool bIsInProgress)
{
	// don't bother if setting already matches request
	if(m_Config[sessionType].IsInProgress() == bIsInProgress)
		return;

	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

	// need to update the advertised attributes - only the host can do this
	if(pSession->IsHost())
	{
		gnetDebug1("SetSessionInProgress :: AttributeChange - Session: %s, IsInProgress: %s", NetworkUtils::GetSessionTypeAsString(sessionType), bIsInProgress ? "True" : "False");

		m_Config[sessionType].SetInProgress(bIsInProgress);
		m_bChangeAttributesPending[sessionType] = true;
	}
}

bool CNetworkSession::GetSessionInProgress(const SessionType sessionType)
{
	return m_Config[sessionType].IsInProgress();
}

void CNetworkSession::SetContentCreator(const SessionType sessionType, const unsigned nContentCreator)
{
	// must be a job session
	if(!gnetVerify(m_Config[sessionType].GetSessionPurpose() == SessionPurpose::SP_Activity))
	{
		gnetDebug1("SetContentCreator :: Can only be called on an activity / job session!");
		return;
	}
	
	// don't bother if setting already matches request
	if(m_Config[sessionType].GetContentCreator() == nContentCreator)
		return;

	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

	// need to update the advertised attributes - only the host can do this
	if(pSession->IsHost())
	{
		gnetDebug1("SetContentCreator :: AttributeChange - Session: %s, ContentCreator: %u", NetworkUtils::GetSessionTypeAsString(sessionType), nContentCreator);

		m_Config[sessionType].SetContentCreator(nContentCreator);
		m_bChangeAttributesPending[sessionType] = true;
	}
}

unsigned CNetworkSession::GetContentCreator(const SessionType sessionType)
{
	return m_Config[sessionType].GetContentCreator();
}

void CNetworkSession::SetActivityIsland(const SessionType sessionType, const int nActivityIsland)
{
	// must be a job session
	if(!gnetVerify(m_Config[sessionType].GetSessionPurpose() == SessionPurpose::SP_Activity))
	{
		gnetDebug1("SetActivityIsland :: Can only be called on an activity / job session!");
		return;
	}

	// don't bother if setting already matches request
	if(m_Config[sessionType].GetActivityIsland() == static_cast<unsigned>(nActivityIsland))
		return;

	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

	// need to update the advertised attributes - only the host can do this
	if(pSession->IsHost())
	{
		gnetDebug1("SetActivityIsland :: AttributeChange - Session: %s, ActivityIsland: %d", NetworkUtils::GetSessionTypeAsString(sessionType), nActivityIsland);

		m_Config[sessionType].SetActivityIsland(static_cast<unsigned>(nActivityIsland));
		m_bChangeAttributesPending[sessionType] = true;
	}
}

int CNetworkSession::GetActivityIsland(const SessionType sessionType)
{
	return static_cast<int>(m_Config[sessionType].GetActivityIsland());
}

void CNetworkSession::SetSessionVisibility(const SessionType sessionType, const bool bIsVisible)
{
    // check lock setting
    if(m_bLockVisibility[sessionType])
    {
        if(!gnetVerify(bIsVisible == m_bLockSetting[sessionType]))
		{
            gnetError("SetSessionVisibility :: %s - Visibility locked. Cannot change to %s", NetworkUtils::GetSessionTypeAsString(sessionType), bIsVisible ? "Visible" : "Not Visible");
            return;
        }
    }

	// don't bother if visibility already matches request
	if(m_Config[sessionType].IsVisible() == bIsVisible)
		return;

	// we swap session pointers, so can't index in
	snSession* pSession = (sessionType == SessionType::ST_Physical) ? m_pSession : m_pTransition;

    // need to update the advertised attributes - only the host can do this
    if(pSession->IsHost())
    {
	    gnetDebug1("SetSessionVisibility :: AttributeChange - Session: %s, Is Visible: %s", NetworkUtils::GetSessionTypeAsString(sessionType), bIsVisible ? "True" : "False");

	    m_Config[sessionType].SetVisible(bIsVisible);
		m_bChangeAttributesPending[sessionType] = true;
    }
}

bool CNetworkSession::IsSessionVisible(const SessionType sessionType) const
{
    return m_Config[sessionType].IsVisible();
}

void CNetworkSession::SetSessionVisibilityLock(const SessionType sessionType, bool bLockVisibility, bool bLockSetting)
{
    // check that something has changed
    if(m_bLockSetting[sessionType] != bLockSetting || m_bLockVisibility[sessionType] != bLockVisibility)
    {
        gnetDebug1("SetSessionVisibilityLock :: %s - Is Locked: %s, Was Locked: %s, Is Visible: %s, Was Visible: %s", NetworkUtils::GetSessionTypeAsString(sessionType), bLockVisibility ? "True" : "False", m_bLockVisibility[sessionType] ? "True" : "False", bLockSetting ? "True" : "False", m_bLockSetting[sessionType] ? "True" : "False");
        m_bLockVisibility[sessionType] = bLockVisibility;
        m_bLockSetting[sessionType] = bLockSetting;
        if(m_bLockSetting[sessionType] != IsSessionVisible(sessionType))
            SetSessionVisibility(sessionType, m_bLockSetting[sessionType]);
	}
}

bool CNetworkSession::IsSessionVisibilityLocked(const SessionType sessionType) const
{
    return m_bLockVisibility[sessionType];
}

void CNetworkSession::ChangeSessionSlots(const unsigned nPublic, const unsigned nPrivate)
{
	if((nPublic == m_pSession->GetMaxSlots(RL_SLOT_PUBLIC)) && (nPrivate == m_pSession->GetMaxSlots(RL_SLOT_PRIVATE)))
	{
		gnetDebug2("ChangeSessionSlots :: Already have these slots.");
		return;
	}
	
    // need to update the advertised attributes - only the host can do this
    if(m_pSession->IsHost())
    {
        gnetDebug2("ChangeSessionSlots :: AttributeChange - Public: %d, Private: %d", nPublic, nPrivate);

		// apply the current config - chiefly, we do this to allocate the schema index values 
		player_schema::MatchingAttributes mmAttrs;
	    m_Config[SessionType::ST_Physical].Reset(mmAttrs, m_pSession->GetConfig().m_Attrs.GetGameMode(), m_pSession->GetConfig().m_Attrs.GetSessionPurpose(), nPublic + nPrivate, nPrivate);
		m_Config[SessionType::ST_Physical].Apply(m_pSession->GetConfig());

		m_bChangeAttributesPending[SessionType::ST_Physical] = true;
	}
}

void CNetworkSession::UpdateMatchmakingGroups()
{
    bool bGroupsChanged = false;

    // not for non-hosts (or when migrating)
    if(!m_pSession->IsHost() || m_pSession->IsMigrating())
        return;

    // not for activity sessions
    if(IsActivitySession())
        return;
 
    // check all groups
    for(int i = 0; i < m_nActiveMatchmakingGroups; i++)
    {
        unsigned nGroupMembersPrevious = m_Config[SessionType::ST_Physical].GetNumberInGroup(i);
        unsigned nGroupMembersNow = GetMatchmakingGroupNum(m_ActiveMatchmakingGroups[i]);

        if(nGroupMembersPrevious != nGroupMembersNow)
        {
            gnetDebug2("UpdateMatchmakingGroups :: AttributeChange - Matchmaking Group %s now has %d members, was %d", gs_szMatchmakingGroupNames[m_ActiveMatchmakingGroups[i]], nGroupMembersNow, nGroupMembersPrevious);
            m_Config[SessionType::ST_Physical].SetNumberInGroup(i, nGroupMembersNow);
            bGroupsChanged = true;
        }
    }
    
    // reset check interval
    m_nLastMatchmakingGroupCheck = sysTimer::GetSystemMsTime();

    // flag session attributes for update
    if(bGroupsChanged)
        m_bChangeAttributesPending[SessionType::ST_Physical] = true;
}

unsigned CNetworkSession::GetMatchmakingGroupNum(const eMatchmakingGroup nGroup) const
{
	unsigned nMembers = 0;

	rlGamerInfo gamerInfos[RL_MAX_GAMERS_PER_SESSION];
	const unsigned nGamers = m_pSession->GetGamers(gamerInfos, COUNTOF(gamerInfos));
	for(unsigned i = 0; i < nGamers; ++i)
	{
		// check member status
		if(!m_pSession->IsMember(gamerInfos[i].GetGamerId()))
			continue;
		
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(gamerInfos[i].GetGamerId());
		if(!gnetVerify(pPlayer))
		{
			gnetError("GetMatchmakingGroupNum :: Player in session does not exist!");
			continue;
		}

		// check if the group is the same as the passed in group
		if(pPlayer->GetMatchmakingGroup() == nGroup)
			nMembers++;
	}

	return nMembers;
}

unsigned CNetworkSession::GetMatchmakingGroupFree(const eMatchmakingGroup nGroup) const
{
    int nActiveIndex = GetActiveMatchmakingGroupIndex(nGroup);
    if(nActiveIndex < 0)
        return 0;

    return GetMatchmakingGroupMax(nGroup) - GetMatchmakingGroupNum(nGroup);
}

int CNetworkSession::GetActiveMatchmakingGroupIndex(eMatchmakingGroup nGroup) const
{
	for(int i = 0; i < m_nActiveMatchmakingGroups; i++)
	{
		if(m_ActiveMatchmakingGroups[i] == nGroup)
			return i;
	}

	// not active
	return -1;
}

void CNetworkSession::SetBlockJoinRequests(bool bBlockJoinRequests)
{
	// can only call this as session host
	if(!gnetVerify(IsHost()))
	{
		gnetError("SetBlockJoinRequests :: Calling as non-host!");
		return;
	}

    if(m_bBlockJoinRequests != bBlockJoinRequests)
    {
        gnetDebug2("SetBlockJoinRequests :: %s join requests.", bBlockJoinRequests ? "Blocking" : "Not Blocking");
	    m_bBlockJoinRequests = bBlockJoinRequests;
    }
}

bool CNetworkSession::CanStart() const
{
#if !__NO_OUTPUT
    enum CannotStartReason
    {
        CannotStart_Invalid = -1,
        CannotStart_SessionNotEstablished,
        CannotStart_NoLocalPlayer,
        CannotStart_HasPendingPlayers,
        CannotStart_HasTemporaryPlayers,
        CannotStart_HasDisplayNameTasks,
        CannotStart_WaitingForObjectIds,
        CannotStart_PendingBubbleAssignments,
    };
    static CannotStartReason s_LastReason = CannotStartReason::CannotStart_Invalid;
    static unsigned s_ReasonStartStamp = 0;
    static unsigned s_ReasonLogStamp = 0;
    static const unsigned LOG_INTERVAL = 1000;

    // promoted this logging to level 1 but we don't want it to spam
    // log when we encounter a new waiting reason so that we can see the progression and also at LOG_INTERVAL
    // so that we can see when we're in a state for a particularly long time
#define CANNOT_START(reason)                                                                                                                \
    if(s_LastReason != reason)                                                                                                              \
    {                                                                                                                                       \
        gnetDebug1("CanStart :: Waiting: %s", #reason);                                                                                     \
        s_LastReason = reason;                                                                                                              \
        s_ReasonStartStamp = sysTimer::GetSystemMsTime();                                                                                   \
        s_ReasonLogStamp = s_ReasonStartStamp;                                                                                              \
    }                                                                                                                                       \
    else if((sysTimer::GetSystemMsTime() - s_ReasonLogStamp) > LOG_INTERVAL)                                                                \
    {                                                                                                                                       \
        gnetDebug1("CanStart :: Waiting: %s, Total: %ums", #reason, (sysTimer::GetSystemMsTime() - s_ReasonStartStamp));                    \
        s_ReasonLogStamp = sysTimer::GetSystemMsTime();                                                                                     \
    }                                                                                                                                       \
    return false;
#else
    // nothing to do in non-logging builds except bail out
#define CANNOT_START(reason)                                                                                                                \
    return false;
#endif

	// check that the session is established - this means that we've connected to all pre-existed players 
	// in the session - this is similar to how we did this with the previous match system
    if(!m_pSession->IsEstablished())
    {
        CANNOT_START(CannotStartReason::CannotStart_SessionNotEstablished);
    }

	// we can't enter the lobby if the local player hasn't been created yet
    if(!CGameWorld::FindLocalPlayer())
    {
        CANNOT_START(CannotStartReason::CannotStart_NoLocalPlayer);
    }

	// we can't enter the lobby while we have pending players
    if(NetworkInterface::GetPlayerMgr().GetNumPendingPlayers() > 0)
    {
        CANNOT_START(CannotStartReason::CannotStart_HasPendingPlayers);
    }

	// we can't enter while we have temporary players
	if(m_bPreventStartWithTemporaryPlayers && NetworkInterface::GetPlayerMgr().GetNumTempPlayers() > 0)
    {
        CANNOT_START(CannotStartReason::CannotStart_HasTemporaryPlayers);
    }

	// we can't enter while we're in the middle of adding gamers
	if(m_bPreventStartWhileAddingPlayers && m_pSession->HasInFlightDisplayNameTasks())
    {
        CANNOT_START(CannotStartReason::CannotStart_HasDisplayNameTasks);
    }

    // we need to wait for all connected players to be assigned physical player indices before entering the lobby
    unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
    const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        const CNetGamePlayer* pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);
        if(pPlayer && !pPlayer->IsInRoamingBubble())
        {
            CANNOT_START(CannotStartReason::CannotStart_PendingBubbleAssignments);
        }
    }

    // we need to negotiate for object IDs with all players in the session before entering the lobby
    if(NetworkInterface::GetObjectManager().GetObjectIDManager().IsWaitingForObjectIDs(!m_bTransitionStartPending))
    {
        CANNOT_START(CannotStartReason::CannotStart_WaitingForObjectIds);
    }

	return true;
}

bool CNetworkSession::CanJoinMatchmakingGroup(const eMatchmakingGroup nGroup) const
{
	// check that the group is valid
	if(!gnetVerify(nGroup != MM_GROUP_INVALID))
	{
		gnetError("CanJoinMatchmakingGroup :: Invalid group!");
		return false;
	}
	
	// if we have a maximum limit, check that we have space
	if(GetMatchmakingGroupMax(nGroup) > 0)
		return GetMatchmakingGroupNum(nGroup) < GetMatchmakingGroupMax(nGroup);

	// otherwise true
	return true; 
}

bool CNetworkSession::CanAcceptJoiners()
{
    if(IsHost() && CNetwork::GetPlayerMgr().IsInitialized())
    {
        if(m_SessionState >= SS_ESTABLISHING && m_SessionState < SS_ESTABLISHED)
            return true;
    }

    return false;
}

bool CNetworkSession::BuildJoinRequest(u8* pData, unsigned nMaxSize, unsigned& nSizeOfData, unsigned joinFlags)
{
	// grab the local players primary clan (if present)
	rlClanId nClanID = RL_INVALID_CLAN_ID;
	NetworkClan& tClan = CLiveManager::GetNetworkClan();
	if(tClan.HasPrimaryClan())
		nClanID = tClan.GetPrimaryClan()->m_Id;

	// grab the local players team (if setup)
	int nTeam = -1;
	if(CNetwork::IsNetworkOpen())
		nTeam = NetworkInterface::GetLocalPlayer()->GetTeam();
	else
		nTeam = CGameWorld::GetMainPlayerInfo()->Team;

    // work out matchmaking group (invite overrides)
	// validate that we have an invite - we can arrive here from a failed previous session join attempt
	// to an invite, and will no longer have the invite. In that case, the matchmaking group should be used.
    eMatchmakingGroup nMatchmakingGroup = m_MatchmakingGroup;
    if(CheckFlag(joinFlags, JoinFlags::JF_ViaInvite) && CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
        nMatchmakingGroup = CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsSCTV() ? MM_GROUP_SCTV : MM_GROUP_FREEMODER;

	unsigned nPlayerFlags = 0;

	// these are only relevant for the transition
	if(m_TransitionData.m_PlayerData.m_bIsSpectator)      
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR;
	// check if we're async / on call
	if(m_TransitionData.m_PlayerData.m_bIsAsync)
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_IS_ASYNC;

	if(NetworkInterface::IsRockstarDev())
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_ROCKSTAR_DEV;

	// check if the local player is a boss
	const int characterSlot = NetworkInterface::GetActiveCharacterIndex();
	const u64 bossId = StatsInterface::IsValidCharacterSlot(characterSlot) ? StatsInterface::GetBossGoonUUID(characterSlot) : 0;
	if(bossId != 0)
	{
		// these are somewhat distinct concepts but potential for some merging here
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS;
		joinFlags |= JoinFlags::JF_IsBoss;
	}

	// build player data
	CNetGamePlayerDataMsg playerData;
	playerData.Reset(
		CNetwork::GetVersion(),
		netHardware::GetNatType(),
		CNetGamePlayer::NETWORK_PLAYER,
		nMatchmakingGroup,
		nPlayerFlags,
		nTeam,
		NetworkGameConfig::GetAimPreference(),
		nClanID,
		m_TransitionData.m_PlayerData.m_nCharacterRank,      //< only relevant for transition
		m_TransitionData.m_PlayerData.m_nELO,                //< only relevant for transition
		NetworkGameConfig::GetMatchmakingRegion());

	// make sure this flag is applied
	if(CLiveManager::IsOverallReputationBad())
		joinFlags |= JoinFlags::JF_BadReputation;

	// build join request
	CMsgJoinRequest tJoinRequest;
	tJoinRequest.Reset(joinFlags, playerData);

	// dump asset verifier values
	OUTPUT_ONLY(CNetwork::GetAssetVerifier().DumpSyncData());

	gnetDebug2("BuildJoinRequest :: Matchmaking Group: %s, Spectator: %s, CrewId: %" I64FMT "d, Pool: %s, Aim: %d, JoinFlags: 0x%x",
				gs_szMatchmakingGroupNames[nMatchmakingGroup],
				m_TransitionData.m_PlayerData.m_bIsSpectator ? "True" : "False",
                nClanID,
				NetworkUtils::GetPoolAsString(NetworkGameConfig::GetMatchmakingPool()),
				NetworkGameConfig::GetAimPreference(),
				joinFlags);

	// export join request
	return tJoinRequest.Export(pData, nMaxSize, &nSizeOfData);
}

void CNetworkSession::BuildTransitionPlayerData(CNetGamePlayerDataMsg* pMsg, const sTransitionPlayer* const pGamer) const
{
	// build player flags
	unsigned nPlayerFlags = 0;
	if(pGamer->m_bIsSpectator)  
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR;
	if(NetworkInterface::IsRockstarDev())
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_ROCKSTAR_DEV;

	// check if the local player is a boss
	const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
	const u64 bossId = StatsInterface::GetUInt64Stat(statId);
	if(bossId != 0)
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS;

	// sync async / on call status
	if(pGamer->m_bIsAsync)
		nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_IS_ASYNC;

	// use a player data message so that this works across transitions
	pMsg->Reset(CNetwork::GetVersion(),						//< this will always match local
				pGamer->m_NatType,							//< individual
				CNetGamePlayer::NETWORK_PLAYER,				//< no bots supported in job lobbies
				MM_GROUP_FREEMODER,							//< no matchmaking groups in jobs
				nPlayerFlags,								//< individual
				-1,											//< not used
				NetworkGameConfig::GetAimPreference(),		//< this will always match local
				pGamer->m_nClanID,							//< individual
				pGamer->m_nCharacterRank,					//< individual
				pGamer->m_nELO,								//< individual
				NetworkGameConfig::GetMatchmakingRegion());
}

bool CNetworkSession::JoinMatchmakingSession(const unsigned mmIndex, const rlGamerHandle* pHandles, const unsigned nGamers)
{
	// check no queries are pending
	for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++)
	{
		if(!gnetVerify(!m_MatchMakingQueries[i].IsBusy()))
		{
			gnetError("JoinMatchmakingSession :: %s Matchmaking in progress!", GetMatchmakingQueryTypeAsString(m_MatchMakingQueries[i].m_QueryType));
			return false;
		}
	}

	if(!gnetVerify(mmIndex < m_GameMMResults.m_nNumCandidates))
	{
		gnetError("JoinMatchmakingSession :: Invalid Matchmaking Index!");
		return false;
	}

	// increase attempted count
	m_GameMMResults.m_nNumCandidatesAttempted++;
	
	// grab matchmaking session result, log session information
	const rlSessionDetail& mmDetail = m_GameMMResults.m_CandidatesToJoin[mmIndex].m_Session;
	gnetDebug1("JoinMatchmakingSession :: Session %d, Hosted by %s, Players %d", mmIndex + 1, mmDetail.m_HostName, mmDetail.m_NumFilledPublicSlots + mmDetail.m_NumFilledPrivateSlots);

	// record when we started to join this session
	m_GameMMResults.m_CandidatesToJoin[mmIndex].m_nTimeStarted = sysTimer::GetSystemMsTime();

	// build join flags
	unsigned joinFlags = JoinFlags::JF_Default;
	if((m_nMatchmakingFlags & MatchmakingFlags::MMF_IsBoss) != 0)
		joinFlags |= JoinFlags::JF_IsBoss;
	if((m_nMatchmakingFlags & MatchmakingFlags::MMF_ExpandedIntroFlow) != 0)
		joinFlags |= JoinFlags::JF_ExpandedIntroFlow;

	// call into JoinSession function
    return JoinSession(mmDetail.m_SessionInfo, RL_SLOT_PUBLIC, joinFlags, pHandles, nGamers);
}

bool CNetworkSession::JoinSession(const rlSessionInfo& tSessionInfo, const rlSlotType kSlotType, unsigned nJoinFlags, const rlGamerHandle* reservedGamers, const unsigned numReservedGamers)
{
	gnetDebug1("JoinSession :: Token: 0x%016" I64FMT "x, Channel: %s, Flags: 0x%x", tSessionInfo.GetToken().m_Value, NetworkUtils::GetChannelAsString(m_pSession->GetChannelId()), nJoinFlags);

#if !__FINAL
	// simulate a failure in the join call - this is to replicate time sensitive failures
	// to verify that we're handling these correctly in script
	if(PARAM_netSessionBailAtJoin.Get())
	{
		gnetDebug1("JoinSession :: Simulating eBAIL_SESSION_JOIN_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_CMD_LINE));
		return false;
	}
#endif

	// verify that we've initialised the session
	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("JoinSession :: Not initialised!");
		return false;
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("JoinSession"))
        return false;

	// verify that we have multiplayer access
	if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
	{
		gnetError("JoinSession :: Cannot access multiplayer!");
		return false; 
	}

    // verify state
	if(!gnetVerify((m_SessionState == SS_IDLE) || (m_SessionState == SS_FINDING) || (m_SessionState == SS_JOINING)))
	{
		gnetError("JoinSession :: Invalid State (%d)", m_SessionState);
		return false;
	}

	// verify session info
	if(!gnetVerify(tSessionInfo.IsValid()))
	{
		gnetError("JoinSession :: Invalid Session Info");
		return false;
	}

	// verify slot type
	if(!gnetVerify(kSlotType != RL_SLOT_INVALID))
	{
		gnetError("JoinSession :: Invalid Slot Type");
		return false;
	}

	// verify that we can join
	if(!gnetVerify(snSession::CanJoin(NetworkInterface::GetNetworkMode())))
	{
		gnetError("JoinSession :: Cannot Join");
		return false;
	}

	// verify that we've initialised the session
	if(!gnetVerify(!IsBusy()))
	{
		gnetError("JoinSession :: Busy!");
		return false;
	}

	// verify that we're not launching
	if(!gnetVerify(!IsLaunchingTransition() && !m_bTransitionStartPending))
	{
		gnetError("JoinSession :: Launching Transition!");
		return false; 
	}

	// reset flags
	m_bBlockJoinRequests = false;
	m_bJoinIncomplete = false;

	// reset locked visibility
	m_bLockVisibility[SessionType::ST_Physical] = false;
	m_bLockSetting[SessionType::ST_Physical] = true;

	// open the network if it's not currently open
	if(!CNetwork::IsNetworkOpen())
	{
		// have to reset the state prior to opening the network
		SetSessionState(SS_IDLE, true);
		OpenNetwork();
	}

	// if we're coming from single player and in a transition, set local player flag
	if(IsTransitionActive() && m_pTransition->Exists())
	{
		NetworkInterface::GetLocalPlayer()->SetStartedTransition(true);
		NetworkInterface::GetLocalPlayer()->SetTransitionSessionInfo(m_pTransition->GetSessionInfo());
	}

	// grab the local players primary clan (if present)
	rlClanId nClanID = RL_INVALID_CLAN_ID;
	NetworkClan& tClan = CLiveManager::GetNetworkClan();
	if(tClan.HasPrimaryClan())
		nClanID = tClan.GetPrimaryClan()->m_Id;

	// we can also consume private if this is a non-invite join with private slots
	const bool bViaInvite = (nJoinFlags & JoinFlags::JF_ViaInvite) != 0;
	if(!bViaInvite && kSlotType == RL_SLOT_PRIVATE)
		nJoinFlags |= JoinFlags::JF_ConsumePrivate;

	// build join request
	u8 aData[256]; unsigned nSizeOfData;
	bool bSuccess = BuildJoinRequest(aData, 256, nSizeOfData, nJoinFlags);

	// verify that we've initialised the session
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinSession :: Failed to build Join Request!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST));
		return false;
	}

	// log out to player manager
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "JOIN_GAME", "\r\n");

    // setup presence flag
    unsigned nCreateFlags = 0;
#if RSG_PRESENCE_SESSION
    nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#endif

#if RSG_DURANGO
	if(IsTransitionActive() && !m_bIsTransitionToGame)
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif

	// for freeroam, wait until the session reaches the start state before we advertise
	if(!m_bDisableDelayedMatchmakingAdvertise)
		nCreateFlags |= RL_SESSION_CREATE_FLAG_MATCHMAKING_ADVERTISE_DISABLED;

	// call into join session
	bSuccess = m_pSession->Join(NetworkInterface::GetLocalGamerIndex(),
							    tSessionInfo,
							    NetworkInterface::GetNetworkMode(),
							    kSlotType,
							    nCreateFlags,
							    aData,
							    nSizeOfData,
								reservedGamers,
								numReservedGamers,
							    m_pSessionStatus);

	// verify that call was successful
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinSession :: Join failed!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_ON_STARTING));
		return false;
	}

	// let script know
	GetEventScriptNetworkGroup()->Add(CEventNetworkJoinSession());

	// lock matchmaking pool
	m_nSessionPool[SessionType::ST_Physical] = NetworkBaseConfig::GetMatchmakingPool();

	// stash join parameters
	m_JoinSessionInfo = tSessionInfo;
	m_JoinSlotType = kSlotType;
	m_nJoinFlags = nJoinFlags;

    OUTPUT_ONLY(LogJoinFlags(m_nJoinFlags, __FUNCTION__));

	// update state
	SetSessionState(SS_JOINING); 

	// indicate success
	return true;
}

#if RSG_NP
bool CNetworkSession::JoinFriendSession(const rlFriend* pFriend)
{
	if(!pFriend)
		return false;

	char aBlob[RL_PRESENCE_MAX_BUF_SIZE] = {};
	unsigned nBlobSize = 0;
	pFriend->GetPresenceBlob(NetworkInterface::GetLocalGamerIndex(), aBlob, &nBlobSize, RL_PRESENCE_MAX_BUF_SIZE);

	rlSessionInfo sessionInfo;
	sessionInfo.ImportFromNpPresence(aBlob, nBlobSize);

	gnetDebug1("JoinFriendSession :: Friend: %s, Token: 0x%016" I64FMT "x", pFriend->GetName(), sessionInfo.GetToken().m_Value);

    if(sessionInfo.IsValid())
    {
		const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
			
		rlGamerHandle hGamer;
		pFriend->GetGamerHandle(&hGamer);

		// fake an invite accepted event
		CLiveManager::GetInviteMgr().HandleJoinRequest(sessionInfo, *pInfo, hGamer, InviteFlags::IF_None);

		// we are joining
		return true;
    }
    else
	{
		gnetDebug1("\tJoinFriendSession :: Invalid session info!");
	}
		
	return false;
}
#else
bool CNetworkSession::JoinFriendSession(const rlFriend* )
{
	// not available
	return false;
}
#endif

void CNetworkSession::Destroy()
{
	gnetAssertf(!IsBusy(), "Destroy :: Cannot destroy a network session while a session operation is in progress!");
	gnetDebug1("Destroy");

    if(!m_pSession->Destroy(m_pSessionStatus))
    {
        m_pSessionStatus->SetPending();
        m_pSessionStatus->SetFailed();
    }

    SetSessionState(SS_DESTROYING);
}

void CNetworkSession::ClearSession(const unsigned leaveFlags, const LeaveSessionReason leaveReason)
{
	// is this a full network close
	bool bFullClose = true; 
	bFullClose &= (!IsTransitioning());
	bFullClose &= (leaveReason != Leave_ReservingSlot);

	// if we're leaving and keeping our transition active explicitly, do not fully close
	if(CheckFlag(leaveFlags, LeaveFlags::LF_NoTransitionBail) && IsTransitionActive())
		bFullClose = false;

	gnetDebug1("ClearSession :: IsTransitioning: %s, LeaveFlags: 0x%x, LeaveReason: %s, FullClose: %s", 
		IsTransitioning() ? "True" : "False", 
		leaveFlags,
		gs_szLeaveReason[leaveReason],
		bFullClose ? "True" : "False");

    // telemetry leave
	CNetworkTelemetry::SessionLeft(m_pSession->GetSessionId(), IsTransitioning());
	
	// only do this when leaving back to SP
    if(!IsTransitioning() && 
	   !CheckFlag(leaveFlags, LeaveFlags::LF_NoTransitionBail) &&
	   !(m_bPreventClearTransitionWhenJoiningInvite && m_bJoinedTransitionViaInvite && (m_TransitionState < TS_ESTABLISHED)))
	{
		// if in a transition session, leave immediately
		// this can happen if we launch a transition, swapping the freeroam session to the main session
		// and then leave the launch transition immediately, before the old freeroam session has completed
		// leaving. 
		if(IsTransitionActive() && !IsTransitionLeaving())
		{
			gnetDebug1("ClearSession :: Still in a transition. Leaving GTAO - Bailing Transition");
			BailTransition(BAIL_TRANSITION_CLEAR_SESSION);
		}

		// wipe session type
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Invalid;

        // wipe this if we're not transitioning
        m_bJoinedViaInvite = false;
        m_Inviter.Clear();
        m_nGamersToGame = 0;

        // clear out transition inviter
        m_bJoinedTransitionViaInvite = false;
		m_nTransitionInviteFlags = 0;
        m_TransitionInviter.Clear();
		m_bIsTransitionAsyncWait = false;

        // clear out unique crew limit
        m_UniqueCrewData[SessionType::ST_Physical].Clear();

        // clear radio synced state
        m_bMainSyncedRadio = false;

        // swap the sessions back round if this is an activity session
        if(m_pSession->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID)
			SwapSessions();

		// clear activity parameters
		m_bIsActivitySession = false;
		m_bHasSetActivitySpectatorsMax = false;
		m_bTransitionStartPending = false;

        // clear out blacklisted gamers
        m_BlacklistedGamers.ClearBlacklist();

		// reset info mine
		m_nEnterMultiplayerTimestamp = 0;
		m_MatchmakingInfoMine.ClearSession();
	}

    // reset host flags
    m_nHostFlags[SessionType::ST_Physical] = HostFlags::HF_Default;
	m_LastMigratedPosix[SessionType::ST_Physical] = 0;

#if RSG_PRESENCE_SESSION
	// reset presence flag
	m_bRequirePresenceSession = false;
	// reset this flag so that we try again next time
	m_bPresenceSessionFailed = false;
#endif

	// reset the session state
	SetSessionState(SS_IDLE);

	// reset matchmaking groups 
	m_MatchmakingGroup = MM_GROUP_FREEMODER;
	ClearActiveMatchmakingGroups();
	m_nMatchmakingPropertyID = NO_PROPERTY_ID;
	m_nMatchmakingMentalState = NO_MENTAL_STATE;
	m_nMatchmakingMinimumRank = 0;
    m_nMatchmakingELO = 0;
	m_bEnterMatchmakingAsBoss = false;

	// wipe user data
	m_SessionUserDataFreeroam.Clear();

	// update presence info
	UpdateSCPresenceInfo();

	// close out the session
	CloseNetwork(bFullClose);

#if RSG_DURANGO
	if(bFullClose)
	{
		// if we aren't about to move into a platform party session, leave
		if(!(CLiveManager::GetInviteMgr().HasConfirmedAcceptedInvite() && CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsViaPlatformPartyAny()))
			RemoveFromParty(false);
	}
#endif

	// reset the peer complainer
	m_PeerComplainer[SessionType::ST_Physical].Shutdown();

	// wipe out invites
	m_InvitedPlayers.ClearAllInvites();

	// clear matchmaking attributes
	m_MatchmakingAttributes.Clear();

    // reset gamers in store
    m_GamerInStore.Reset();

    // reset any group activity joins
    m_bIsActivityGroupQuickmatch = false;
	m_bIsActivityGroupQuickmatchAsync = false;
	m_ActivityGroupSession.Clear();
	m_bFreeroamIsJobToJob = false;
	
	// clear managing gamer tracking
	m_hToGameManager.Clear();
	m_hActivityQuickmatchManager.Clear();
}

unsigned CNetworkSession::GetSessionMemberCount() const
{
	return m_pSession->GetGamerCount();
}

unsigned CNetworkSession::GetSessionMembers(rlGamerInfo aInfo[], const unsigned nMaxGamers) const
{
    return m_pSession->GetGamers(aInfo, nMaxGamers);
}

netPlayer* CNetworkSession::GetHostPlayer() const
{
	netPlayer* pHostPlayer = NULL;

	u64 hostPeerId = 0;

	if (m_pSession->GetHostPeerId(&hostPeerId))
	{
		pHostPlayer = NetworkInterface::GetPlayerFromPeerId(hostPeerId);
	}

	return pHostPlayer;
}

bool CNetworkSession::IsHostPlayerAndRockstarDev(const rlGamerHandle& handle ) const
{
	bool bIsHost = false;
	bool bIsRockstarDev = false;
	rlGamerInfo gamerInfo; 
	
	m_pTransition->GetGamerInfo(handle, &gamerInfo);
	
	bIsHost = gamerInfo.IsValid() && m_pTransition->IsHostPeerId(gamerInfo.GetPeerInfo().GetPeerId());
	

	for (int i = 0; i < m_TransitionPlayers.GetCount(); i++)
	{
		if (m_TransitionPlayers[i].m_hGamer == handle)
		{
			bIsRockstarDev = m_TransitionPlayers[i].m_bIsRockstarDev;
		}
	}

	return bIsHost && bIsRockstarDev;
}

bool CNetworkSession::IsInvitable() const
{
	return m_pSession->IsInvitable();
}

bool CNetworkSession::IsOnline() const
{
	return m_pSession->IsOnline();
}

bool CNetworkSession::IsLan() const
{
	return m_pSession->IsLan();
}

bool CNetworkSession::IsHost() const
{
	return m_pSession->IsHost();
}

bool CNetworkSession::IsSessionMember(const rlGamerHandle& hGamer) const
{
	return m_pSession->IsMember(hGamer);
}

bool CNetworkSession::GetSessionMemberInfo(const rlGamerHandle& hGamer, rlGamerInfo* pInfo) const
{
    return m_pSession->GetGamerInfo(hGamer, pInfo);
}

bool CNetworkSession::GetSessionNetEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId)
{
	*endpointId = NET_INVALID_ENDPOINT_ID;

    // check we have a valid member first
    if(!m_pSession->IsMember(sInfo.GetGamerId()))
        return false;

	int nConnectionID = m_pSession->GetPeerCxn(sInfo.GetPeerInfo().GetPeerId());
    const EndpointId epId = m_pSession->GetPeerEndpointId(sInfo.GetPeerInfo().GetPeerId());
	if((nConnectionID >= 0) && NET_IS_VALID_ENDPOINT_ID(epId))
    {
        *endpointId = epId;
        return true;
    }
    return false;
}

bool CNetworkSession::IsInSession(const rlSessionInfo& hInfo)
{
	if(m_pSession->Exists() && m_pSession->GetSessionInfo() == hInfo)
		return true;
	else if(m_pTransition->Exists() && m_pTransition->GetSessionInfo() == hInfo)
		return true;
	
	// not in this session
	return false;
}

bool CNetworkSession::IsJoiningSession(const rlSessionInfo& hInfo)
{
	if(m_SessionState == SS_JOINING && m_JoinSessionInfo == hInfo)
		return true;
	else if(m_TransitionState == TS_JOINING && m_TransitionJoinSessionInfo == hInfo)
		return true;

	// not joining this session
	return false;
}

bool CNetworkSession::IsMySessionToken(const rlSessionToken& hToken)
{
    return (m_pSession->Exists() && m_pSession->GetSessionInfo().GetToken() == hToken);
}

bool CNetworkSession::IsMyTransitionToken(const rlSessionToken& hToken)
{
    return (m_pTransition->Exists() && m_pTransition->GetSessionInfo().GetToken() == hToken);
}

NetworkGameConfig* CNetworkSession::GetInviteDetails(rlSessionInfo* pInfo)
{
#if RSG_PRESENCE_SESSION
	if(m_PresenceSession.GetSessionInfo() == *pInfo && m_PresenceSession.IsHost())
	{
		// for invites to a presence session, we need to change the rlSessionInfo to the correct target session
		if(m_pTransition->Exists())
		{
			*pInfo = m_pTransition->GetSessionInfo();
			return &m_Config[SessionType::ST_Transition];
		}
		else if(m_pSession->Exists())
		{
			*pInfo = m_pSession->GetSessionInfo();
			return &m_Config[SessionType::ST_Physical];
		}
	}
	else
#endif
	{
		if(m_pTransition->Exists() && m_pTransition->GetSessionInfo() == *pInfo)
			return &m_Config[SessionType::ST_Transition];
		else if(m_pSession->Exists() && m_pSession->GetSessionInfo() == *pInfo)
			return &m_Config[SessionType::ST_Physical];
	}

	// not found
	return NULL;
}

bool CNetworkSession::AckInvite(const rlGamerHandle& hGamer)
{
	// check transition first
	if(m_TransitionInvitedPlayers.AckInvite(hGamer))
		return true; 
	//@@: location CNETWORKSESSION_ACKINVITE_AFTER_TRANSITION
	if(m_InvitedPlayers.AckInvite(hGamer))
		return true;

	return false;
}

bool CNetworkSession::SetInviteDecisionMade(const rlGamerHandle& hGamer)
{
	// check transition first
	if(m_TransitionInvitedPlayers.SetDecisionMade(hGamer))
		return true; 
	if(m_InvitedPlayers.SetDecisionMade(hGamer))
		return true; 

	return false;
}

bool CNetworkSession::IsInviteDecisionMade(const rlGamerHandle& hGamer)
{
	// check transition first
	if(m_TransitionInvitedPlayers.IsDecisionMade(hGamer))
		return true;
	if(m_InvitedPlayers.IsDecisionMade(hGamer))
		return true;

	return false;
}

bool CNetworkSession::SetInviteStatus(const rlGamerHandle& hGamer, u32 nCurrentStatus)
{
	// check transition first
	if(m_TransitionInvitedPlayers.SetInviteStatus(hGamer, nCurrentStatus))
		return true; 
	if(m_InvitedPlayers.SetInviteStatus(hGamer, nCurrentStatus))
		return true; 

	return false;
}

unsigned CNetworkSession::GetInviteStatus(const rlGamerHandle& hGamer)
{
	// check transition first
	if(m_TransitionInvitedPlayers.GetInviteStatus(hGamer))
		return m_TransitionInvitedPlayers.GetInviteStatus(hGamer); 
	if(m_InvitedPlayers.GetInviteStatus(hGamer))
		return m_InvitedPlayers.GetInviteStatus(hGamer); 

	return InviteResponseFlags::IRF_None;
}

bool CNetworkSession::InitTransition()
{
	// initialise session
	bool bSuccess = m_pTransition->Init(CLiveManager::GetRlineAllocator(),
										m_SessionOwner[SessionType::ST_Transition],
										&CNetwork::GetConnectionManager(),
										NETWORK_SESSION_ACTIVITY_CHANNEL_ID,
										"gta5");

	// apply attribute names
	m_pTransition->SetPresenceAttributeNames(ATTR_TRANSITION_TOKEN,
											 ATTR_TRANSITION_ID,
											 ATTR_TRANSITION_INFO,
											 ATTR_TRANSITION_IS_HOST,
											 ATTR_TRANSITION_IS_JOINABLE,
											 false);

	if(!gnetVerify(bSuccess))
	{
		gnetError("Init :: Error initialising transition session");
		return false;
	}

	m_pTransition->AddDelegate(&m_SessionDlgt[SessionType::ST_Transition]);

	// initialise state
	SetTransitionState(TS_IDLE); 

	// clear out parameters
	ClearTransitionParameters();

	// indicate result
	return bSuccess;
}

void CNetworkSession::DropTransition()
{
    // keep the channel ID as we need to set this back up 
    unsigned nChannelID = m_pTransition->GetChannelId();
    gnetDebug1("DropTransition :: Using %s", NetworkUtils::GetChannelAsString(nChannelID));

	// we also need to correctly inform Xbox One services so that the events match up
	DURANGO_ONLY(WriteMultiplayerEndEventTransition(false OUTPUT_ONLY(, "DropTransition")));

    // drop and shutdown current session
	m_pTransition->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Transition]);
	m_pTransition->Shutdown(true, eBailReason::BAIL_INVALID);

    // kill any current task
	if(!gnetVerify(!m_pTransitionStatus->Pending()))
    {
		gnetDebug1("DropTransition :: Status is pending! Cancelling.");
        rlGetTaskManager()->CancelTask(m_pTransitionStatus);
    }
	m_pTransitionStatus->Reset();

    bool bSuccess = m_pTransition->Init(CLiveManager::GetRlineAllocator(),
                                        m_SessionOwner[SessionType::ST_Transition],
                                        &CNetwork::GetConnectionManager(),
                                        nChannelID,
                                        "gta5");

    if(!gnetVerify(bSuccess))
    {
        gnetError("DropTransition :: Error initialising session");
        return;
    }

    // apply attribute names
    m_pTransition->SetPresenceAttributeNames(ATTR_TRANSITION_TOKEN,
											 ATTR_TRANSITION_ID,
                                             ATTR_TRANSITION_INFO,
                                             ATTR_TRANSITION_IS_HOST,
                                             ATTR_TRANSITION_IS_JOINABLE,
                                             false);

	m_pTransition->AddDelegate(&m_SessionDlgt[SessionType::ST_Transition]);

	// clear any pending reservations
	m_PendingReservations[SessionType::ST_Transition].Reset();
	m_bHasPendingTransitionBail = false;

	// clear out transition parameters for freeroam players
	if(CNetwork::IsNetworkOpen())
	{
		NetworkInterface::GetLocalPlayer()->SetStartedTransition(false);
		NetworkInterface::GetLocalPlayer()->InvalidateTransitionSessionInfo();
	}

	// if we have no active session, the network is open and we left our last session and explicitly
	// kept this transition session active, do a full network close (in case we return to single player)
	if(!IsSessionActive() && CNetwork::IsNetworkOpen() && CheckFlag(m_LeaveSessionFlags, LeaveFlags::LF_NoTransitionBail))
	{
		gnetDebug1("DropTransition :: No active session, full network close");
		CloseNetwork(true);
	}
}

void CNetworkSession::ClearTransition()
{
	gnetDebug3("ClearTransition");

	SetTransitionState(TS_IDLE);
	m_pTransitionStatus->Reset();

	// reset transition session data
	m_bJoinedTransitionViaInvite = false;
	m_nTransitionInviteFlags = 0;
	m_TransitionInviter.Clear();
	m_bChangeAttributesPending[SessionType::ST_Transition] = false;
	m_LastMigratedPosix[SessionType::ST_Transition] = 0;
	m_bIsTransitionAsyncWait = false;

	// clear out launch data
	m_bLaunchOnStartedFromJoin = false;
	m_bDoNotLaunchFromJoinAsMigratedHost = false;
	m_bIsLeavingLaunchedActivity = false;

	// clear out transition invites
	m_TransitionInvitedPlayers.ClearAllInvites();

	// reset unique crews
	m_UniqueCrewData[SessionType::ST_Transition].Clear();

	// clear radio synced state
	m_bTransitionSyncedRadio = false;

	// reset minimum rank
	SetMatchmakingMinimumRank(0);
	SetMatchmakingELO(0);

	// clear out transition player tracker
	m_TransitionPlayers.Reset();

	// clear config
	m_Config[SessionType::ST_Transition].Clear();
	m_nHostFlags[SessionType::ST_Transition] = HostFlags::HF_Default;

	// clear out the peer complainer
	m_PeerComplainer[SessionType::ST_Transition].Shutdown();

	// add call to this here, it may shutdown after the network session 
	// and the call to CloseNetwork will not be able to clear up the voice class
	CNetwork::ShutdownVoice();
}

void CNetworkSession::CancelTransitionMatchmaking()
{
	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].IsBusy())
	{
		gnetDebug1("CancelTransitionMatchmaking :: Cancelling: %d", i);
		netTask::Cancel(&m_ActivityMMQ[i].m_TaskStatus);
		m_ActivityMMQ[i].Reset(MatchmakingQueryType::MQT_Standard);
	}
	m_bIsTransitionMatchmaking = false;
}

void CNetworkSession::BailTransition(eTransitionBailReason OUTPUT_ONLY(nBailReason), const int OUTPUT_ONLY(nContextParam1), int const OUTPUT_ONLY(nContextParam2), const int OUTPUT_ONLY(nContextParam3))
{
    gnetDebug1("BailTransition :: %s, Params: %d, %d, %d", NetworkUtils::GetTransitionBailReasonAsString(nBailReason), nContextParam1, nContextParam2, nContextParam3);

	// if we are wiping our activity session, clear out the related data
	if(m_pTransition->GetConfig().m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Activity)
		m_SessionUserDataActivity.Clear();

     // drop transition - this takes out the session
    DropTransition();

    // we also perform full transition hazing from our Shutdown function
    SetTransitionState(TS_IDLE);
    m_bChangeAttributesPending[SessionType::ST_Transition] = false;
    m_nLastTransitionMessageTime = 0;

    m_bIsTransitionToGame = false;
    m_bIsTransitionToGameInSession = false;
    m_bIsTransitionToGamePendingLeave = false;
    m_bIsTransitionMatchmaking = false;
    m_hTransitionHostGamer.Clear();
    m_bLaunchingTransition = false;
    m_bDoLaunchPending = false;
    m_bLaunchOnStartedFromJoin = false;
	m_bDoNotLaunchFromJoinAsMigratedHost = false;
    m_nGamersToActivity = 0;

	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].IsBusy())
	{
		gnetDebug1("BailTransition :: Activity matchmaking status %d is pending. Cancelling", i);
		netTask::Cancel(&m_ActivityMMQ[i].m_TaskStatus);
		m_ActivityMMQ[i].Reset(MatchmakingQueryType::MQT_Invalid);
	}

    m_bJoinedTransitionViaInvite = false;
    m_TransitionInviter.Clear();
	m_bIsTransitionAsyncWait = false;
}

void CNetworkSession::FailTransitionHost()
{
    gnetDebug1("FailTransitionHost");
    SetTransitionState(TS_IDLE);

    // flatten the transition
    DropTransition();

    // generate events
    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_HOST_FAILED));
    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionStarted(false));
}

void CNetworkSession::FailTransitionJoin()
{
    gnetDebug1("FailTransitionJoin");
    SetTransitionState(TS_IDLE);

	// flatten the transition
	DropTransition();

    // generate event
    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionStarted(false));

	// update retry flag
	m_bCanRetryTransitionJoinFailure = !m_bJoiningTransitionViaMatchmaking;

    // if this is matchmaking, then re-enter the process
    if(m_bJoiningTransitionViaMatchmaking)
    {
		sMatchmakingResults* pResults = NULL;
        if(m_bTransitionViaMainMatchmaking)
		{
			pResults = &m_GameMMResults;
			JoinOrHostSession(++m_GameMMResults.m_nCandidateToJoinIndex);
		}
        else
		{
			pResults = &m_TransitionMMResults;
			JoinOrHostTransition(++m_TransitionMMResults.m_nCandidateToJoinIndex);
		}

		if(pResults)
		{
			// record bail reason (offset to distinguish from response codes)
			pResults->m_CandidatesToJoin[pResults->m_nCandidateToJoinIndex - 1].m_Result = (static_cast<unsigned>(BAIL_JOINING_STATE_TIMED_OUT) | sMatchmakingResults::BAIL_OFFSET);
			pResults->m_CandidatesToJoin[pResults->m_nCandidateToJoinIndex - 1].m_nTimeFinished = sysTimer::GetSystemMsTime();
		}
    }
	else
	{
#if RSG_DURANGO
		if(m_bJoinedTransitionViaInvite && sAcceptedInvite::AreFlagsViaPlatformPartyAny(m_nTransitionInviteFlags))
		{
			gnetDebug1("FailTransitionJoin :: Via Platform Party. Eject!");
			RemoveFromParty(true);
		}
#endif
	}
}

void CNetworkSession::UpdateTransition()
{
	const u32 nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	// only update this if we need to
	if(m_TransitionState > TS_IDLE)
	{
		m_pTransition->Update(nCurrentTime);

		// check if we should send a transition update message
		if(IsTransitionEstablished() && !m_bLaunchingTransition && !m_bIsTransitionToGame && m_pTransition->GetGamerCount() > 1)
		{
			if(nCurrentTime - TRANSITION_MESSAGE_INTERVAL > m_nLastTransitionMessageTime)
			{
				unsigned nNumMessageParameters = 0;
				sTransitionParameter aMessageParameters[TRANSITION_MAX_PARAMETERS];

				for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
				{
					if(m_bIsTransitionParameterDirty[i])
					{
						aMessageParameters[nNumMessageParameters].m_nID = m_TransitionData.m_aParameters[i].m_nID;
						aMessageParameters[nNumMessageParameters].m_nValue = m_TransitionData.m_aParameters[i].m_nValue;
						nNumMessageParameters++;
					}
				}

				// if we have some parameters to send
				if(nNumMessageParameters > 0)
				{
					// logging when this is sent
					netTransitionMsgDebug("UpdateTransition :: Sending MsgTransitionParameters - Num: %d", nNumMessageParameters);
					
					// setup message
					MsgTransitionParameters msg;
					msg.Reset(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), aMessageParameters, nNumMessageParameters);

					// send message
					m_pTransition->BroadcastMsg(msg, NET_SEND_RELIABLE);

					// clear dirty data
					for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
						m_bIsTransitionParameterDirty[i] = false;
				}

				// string parameter
                for(unsigned i = 0; i < TRANSITION_MAX_STRINGS; i++)
                {
                    if(m_bIsTransitionStringDirty[i])
				    {
						// logging when this is sent
						netTransitionMsgDebug("UpdateTransition :: Sending MsgTransitionParameterString - Index: %d", i);

					    // setup message
					    MsgTransitionParameterString msg;
						msg.Reset(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), m_TransitionData.m_szString[i]);

					    // send message
					    m_pTransition->BroadcastMsg(msg, NET_SEND_RELIABLE);

					    // clear dirty flag
					    m_bIsTransitionStringDirty[i] = false;
				    }
                }

				// mark last check
				m_nLastTransitionMessageTime = nCurrentTime; 
			}
		}
	}

	// push through launch if it's pending
	if(m_bDoLaunchPending)
	{
		if(--m_nDoLaunchCountdown <= 0 && !m_pTransition->IsMigrating())
		{
			// catch the state that we need to place the transition session into
			eTransitionState nState;
			switch(m_SessionState)
			{
			case SS_LEAVING: nState = TS_LEAVING; break;
			case SS_DESTROYING: nState = TS_DESTROYING; break;
			default:
				nState = TS_IDLE;
				break;
			}

			// close the network if it's open
			if(CNetwork::IsNetworkOpen())
				ClearSession(LeaveFlags::LF_Default, LeaveSessionReason::Leave_Normal);

            // if we've become session host and we didn't launch as session host, 
            // we need to set ourselves up as host
            // instead of resending this for other migrated clients, we just assume the 
            // information the host gave us
            // anyone that migrated will setup using that and this allows the session to
            // proceed without further delay
            if(m_pTransition->IsHost() && !m_bLaunchedAsHost)
            {
                gPopStreaming.SetNetworkModelVariationID(m_sMsgLaunch.m_ModelVariationID);

                // fudge the clock
                if(CNetwork::GetNetworkClock().HasStarted())
                    CNetwork::GetNetworkClock().Stop();

                // this uses the launch time the host supplied us to seed the clock
                CNetwork::GetNetworkClock().Start(&CNetwork::GetConnectionManager(),
                                                  netTimeSync::FLAG_SERVER,
                                                  NET_INVALID_ENDPOINT_ID,
                                                  m_sMsgLaunch.m_HostTimeToken,
                                                  &m_sMsgLaunch.m_HostTime,
                                                  NETWORK_GAME_CHANNEL_ID,
                                                  netTimeSync::DEFAULT_INITIAL_PING_INTERVAL,
                                                  netTimeSync::DEFAULT_FINAL_PING_INTERVAL);
            }

			// kick the launch through
			DoLaunchTransition();

			// apply fudged state
			SetTransitionState(nState);

			// reset flag
			m_bDoLaunchPending = false;
        }
	}

	// manage session flow
	switch(m_TransitionState)
	{
	case TS_IDLE:
		break;
	case TS_HOSTING:
        if(m_pTransitionStatus->Succeeded())
        {
            gnetDebug1("UpdateTransition :: Hosted transition session.");		

            SetTransitionState(TS_ESTABLISHING);
			CNetwork::InitVoice();

            // let other players know we've hosted a session (with info)
            if(CNetwork::IsNetworkOpen())
            {
                if(gnetVerify(m_pTransition->Exists()))
                    NetworkInterface::GetLocalPlayer()->SetTransitionSessionInfo(m_pTransition->GetSessionInfo());
                else
                    gnetError("UpdateTransition :: Hosted session does not exist!");
            }

			// write solo session telemetry
			gnetDebug1("UpdateTransition :: Sending solo telemetry - Reason: %s, Visibility: %s", GetSoloReasonAsString(SOLO_SESSION_HOSTED_FROM_SCRIPT), GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Transition)));
			CNetworkTelemetry::EnteredSoloSession(m_pTransition->GetSessionId(), 
												  m_pTransition->GetSessionToken().GetValue(), 
												  m_Config[SessionType::ST_Transition].GetSessionPurpose(), 
												  SOLO_SESSION_HOSTED_FROM_SCRIPT, 
												  GetSessionVisibility(SessionType::ST_Transition),
												  SOLO_SOURCE_MIGRATE,
												  0,
												  0,
												  0);

            // generate event
            GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_STARTING));
        }
        else if(!m_pTransitionStatus->Pending())
        {
            if(m_nHostRetriesTransition < NUM_HOST_RETRIES)
            {
                // increment the retries
                m_nHostRetriesTransition++;

                gnetDebug1("UpdateTransition :: Host failed. Retry %d", m_nHostRetriesTransition);

                // we need to take a snapshot of the config and invalidate it
                NetworkGameConfig tConfig = m_Config[SessionType::ST_Transition];
                m_Config[SessionType::ST_Transition].Clear();

                // we need to flatten the session state
                SetTransitionState(TS_IDLE);

                // try to re-host
                HostTransition(tConfig, m_nHostFlags[SessionType::ST_Transition] | HostFlags::HF_ViaRetry);
            }
            else
            {
                // failed
                gnetError("UpdateTransition :: Failed to host transition session.");
                FailTransitionHost();
            }
        }
        else
        {
			unsigned nSessionTimeout = GetSessionTimeout();
            if(nSessionTimeout > 0 && s_TimeoutValues[TimeoutSetting::Timeout_Hosting] > 0)
            {
                // check if we've been sitting in this state for too long
                const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
                if(nCurrentTime - m_TransitionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_Hosting])
                {
                    gnetDebug1("UpdateTransition :: Hosting timed out [%d/%dms] hosting", nCurrentTime - m_TransitionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_Hosting]);
                    FailTransitionHost();
                }
            }	
        }
		break;
	case TS_JOINING:
		if(m_pTransitionStatus->Succeeded())
		{
			gnetDebug1("UpdateTransition :: Joined session.");

			if(m_bHasPendingTransitionBail)
			{
				gnetDebug1("UpdateTransition :: Processing deferred bail!");
				FailTransitionJoin();
				m_bHasPendingTransitionBail = false;
			}
			else
			{
				CNetwork::InitVoice();

				// remove any presence invites for this session
				CLiveManager::GetInviteMgr().FlushRsInvites(m_pTransition->GetSessionInfo());

				SetTransitionState(TS_ESTABLISHING);

				// let other players know we've joined a session (with info)
				if(CNetwork::IsNetworkOpen())
				{
					if(gnetVerify(m_pTransition->Exists()))
						NetworkInterface::GetLocalPlayer()->SetTransitionSessionInfo(m_pTransition->GetSessionInfo());
					else
						gnetError("UpdateTransition :: Joined session does not exist!");
				}
			}
		}
		else if(!m_pTransitionStatus->Pending())
		{
			// failed
			gnetError("UpdateTransition :: Failed to join session.");
            FailTransitionJoin();
        }
        else
        {
			// check for timeout
			unsigned nSessionTimeout = GetSessionTimeout();
			if(nSessionTimeout > 0)
            {
                const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
                if(nCurrentTime - m_TransitionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin])
                {
                    gnetDebug1("UpdateTransition :: Joining timed out [%d/%dms] joining", nCurrentTime - m_TransitionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin]);
                    FailTransitionJoin();
                }
            }
        }
		break;

    case TS_ESTABLISHING:
        
        // if we're migrating, wait until we're done
        if(m_pTransition->IsMigrating())
            break;
        else
        {
            // if we have no host peer and have made it this far, the host has left and we are marooned
            u64 nHostPeerID = 0;
            if(!m_pTransition->GetHostPeerId(&nHostPeerID))
            {
                gnetDebug1("UpdateTransition :: In lobby with no host. Kick off migrate!");
				m_pTransition->Migrate();
                break;
            }
        }

        // wait for any join operation on the main session to complete
        if(IsSessionActive() && !IsSessionEstablished())
            break;

        // if the match is already in progress, move to start pending
        // for hosts, this might have happened if we started migrating before receiving the guest start message
        if(m_pTransition->IsEstablished())
        {
			gnetDebug1("UpdateTransition :: Establishing session. Time taken: %u", sysTimer::GetSystemMsTime() - m_nCreateTrackingTimestamp);
            SetTransitionState(TS_ESTABLISHED);

            if(m_bLaunchOnStartedFromJoin)
            {
				if(!(m_pTransition->IsHost() && !m_bDoNotLaunchFromJoinAsMigratedHost))
				{
					gnetDebug1("UpdateTransition :: Launching from join");
					LaunchTransition();
				}
				else
				{
					gnetDebug1("UpdateTransition :: Skipping launch from join");
				}

				// reset flag
                m_bLaunchOnStartedFromJoin = false;
            }

			// reset this
			m_bDoNotLaunchFromJoinAsMigratedHost = false;

            // generate event
            GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionStarted(true));
        }
        else
        {
			// check for timeout
			unsigned nSessionTimeout = GetSessionTimeout();
			if(nSessionTimeout > 0)
            {
                const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
                if(nCurrentTime - m_TransitionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin])
				{
					gnetDebug1("UpdateTransition :: Starting timed out [%d/%dms]", nCurrentTime - m_TransitionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_TransitionJoin]);
					FailTransitionJoin();
				}
			}
        }
        break;
	case TS_ESTABLISHED:
		break;
	case TS_LEAVING:
		if(m_pTransitionStatus->Succeeded())
		{
			gnetDebug1("UpdateTransition :: Left session. Destroying");

			bool bSuccess = m_pTransition->Destroy(m_pTransitionStatus);
			if(!bSuccess)
			{
				m_pTransitionStatus->SetPending();
				m_pTransitionStatus->SetFailed();
			}
			SetTransitionState(TS_DESTROYING);
		}
		else if(!m_pTransitionStatus->Pending())
		{
			// failed - flatten session
			gnetError("UpdateTransition :: Failed to leave session.");
            DropTransition();
			ClearTransition();
		}
		else
		{
#if !__FINAL
			if(CNetwork::GetTimeoutTime() > 0)
#endif
			{
				// timeout detection, flatten session if we run long
				const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
				if(nCurrentTime - m_TransitionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_Ending])
				{
					gnetError("UpdateTransition :: Leaving timed out [%d/%dms]", nCurrentTime - m_TransitionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_Ending]);
					DropTransition();
					ClearTransition();
				}
			}
		}
		break;
	case TS_DESTROYING:
		if(m_pTransitionStatus->Succeeded())
		{
			gnetDebug1("UpdateTransition :: Destroyed Session");
			ClearTransition();
		}
		else if(!m_pTransitionStatus->Pending())
		{
			// failed - flatten session
			gnetError("UpdateTransition :: Failed to destroy session");
			DropTransition();
			ClearTransition();
		}
		else
		{
#if !__FINAL
			if(CNetwork::GetTimeoutTime() > 0)
#endif
			{
				// timeout detection, flatten session if we run long
				const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
				if(nCurrentTime - m_TransitionStateTime > s_TimeoutValues[TimeoutSetting::Timeout_Ending])
				{
					gnetError("UpdateTransition :: Destroying timed out [%d/%dms]", nCurrentTime - m_TransitionStateTime, s_TimeoutValues[TimeoutSetting::Timeout_Ending]);
					DropTransition();
					ClearTransition();
				}
			}
		}
		break;
	default:
		break;
	}

	// manage immediate launch - wait until we've completed the join
	if(m_bLaunchOnStartedFromJoin && m_bAllowImmediateLaunchDuringJoin && (m_TransitionState >= TS_ESTABLISHING) && (--m_nImmediateLaunchCountdown <= 0))
	{
		if(!(m_pTransition->IsHost() && !m_bDoNotLaunchFromJoinAsMigratedHost))
		{
			gnetDebug1("UpdateTransition :: Immediately launching from join");
			LaunchTransition();
		}
		else
		{
			gnetDebug1("UpdateTransition :: Skipping immediate launch from join");
		}

		// reset flag
		m_bLaunchOnStartedFromJoin = false;
		m_bDoNotLaunchFromJoinAsMigratedHost = false;
	}

	// manage attribute changes
	if(m_bChangeAttributesPending[SessionType::ST_Transition] && m_pTransition->IsHost() && !IsTransitionLeaving())
	{
		if(!m_ChangeAttributesStatus[SessionType::ST_Transition].Pending())
		{
			gnetDebug1("UpdateTransition :: Processing Attribute Change");
			m_pTransition->ChangeAttributes(m_Config[SessionType::ST_Transition].GetMatchingAttributes(), &m_ChangeAttributesStatus[SessionType::ST_Transition]);
			m_bChangeAttributesPending[SessionType::ST_Transition] = false;
		}
	}

	// process any active matchmaking queries
	UpdateTransitionMatchmaking();
}

void CNetworkSession::UpdateTransitionMatchmaking()
{
	// this will be active if any queries are processing
	if(!m_bIsTransitionMatchmaking)
		return;

	// check all queries
	bool bFinished = true; 
	bool bAnyReissued = false;
	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++)
	{
		if(m_ActivityMMQ[i].m_bIsActive)
		{
			bFinished &= !IsMatchmakingPending(&m_ActivityMMQ[i]);

			// platform matchmaking can run with a regional bias first
			if(!IsMatchmakingPending(&m_ActivityMMQ[i]) && m_ActivityMMQ[i].m_nResults == 0)
			{
				bool bReissue = false;
				if(m_ActivityMMQ[i].m_pFilter->IsLanguageApplied())
				{
					gnetDebug1("UpdateTransitionMatchmaking [%d] - No results. Removing language bias.", i);
					bReissue = true;
					m_ActivityMMQ[i].m_pFilter->ApplyLanguage(false);
				}
				else if(m_ActivityMMQ[i].m_pFilter->IsRegionApplied())
				{
					gnetDebug1("UpdateTransitionMatchmaking [%d] - No results. Removing region bias.", i);
					bReissue = true;
					m_ActivityMMQ[i].m_pFilter->ApplyRegion(false);  
				}

				if(bReissue)
				{
					// run search again and reset timeout
					FindActivitySessions(&m_ActivityMMQ[i]); 
					m_nTransitionMatchmakingStarted = sysTimer::GetSystemMsTime();
					bAnyReissued = true;
				}
			}
		}
	}

	// not finished so just bail
	if(bAnyReissued)
		return;

	// wait until all queries have completed before kicking through
	if(!bFinished)
	{
		const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

		// still pending - if we've disconnected from the host and haven't advanced far enough into the process, allow bail
#if !__FINAL
		if(CNetwork::GetTimeoutTime() > 0)
#endif
		{
			if(nCurrentTime - m_nTransitionMatchmakingStarted > s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking])
			{
				// log time out
				gnetDebug1("UpdateTransitionMatchmaking :: Matchmaking timed out [%d/%dms]!", nCurrentTime - m_nTransitionMatchmakingStarted, s_TimeoutValues[TimeoutSetting::Timeout_Matchmaking]);

				// cancel any pending queries
				for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].IsBusy())
				{
					gnetDebug1("UpdateTransitionMatchmaking :: Cancelling Query %s[%d], Results: %u", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i, m_ActivityMMQ[i].m_nResults);
					netTask::Cancel(&m_ActivityMMQ[i].m_TaskStatus);
				}

				// flag that we're finished
				bFinished = true; 
			}
		}

		// if we're still not finished, bail
		if(m_nNumActivityResultsBeforeEarlyOut > 0)
		{
			// tally current result count
			unsigned nResults = 0;
			unsigned nResultsSolo = 0;
			for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].m_bIsActive)
			{
				for(unsigned j = 0; j < m_ActivityMMQ[i].m_nResults; j++)
				{
					// only include compatible sessions in our list
					if(!IsSessionCompatible(m_ActivityMMQ[i].m_pFilter, &m_ActivityMMQ[i].m_Results[j], m_nTransitionMatchmakingFlags OUTPUT_ONLY(, false)))
						continue;

					// do not include blacklisted sessions in our list
					if(!CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_AllowBlacklisted) && IsSessionBlacklisted(m_ActivityMMQ[i].m_Results[j].m_SessionInfo.GetToken()))
						continue;

					// do not include join failure sessions in our list
					if(HasSessionJoinFailed(m_ActivityMMQ[i].m_Results[j].m_SessionInfo.GetToken()))
						continue;

					// valid session
					nResults++;

					// tally solo sessions
					if((m_ActivityMMQ[i].m_Results[j].m_SessionConfig.m_MaxPublicSlots + m_ActivityMMQ[i].m_Results[j].m_SessionConfig.m_MaxPrivateSlots) <= 1)
						nResultsSolo++;
				}
			}

			// if we breach our threshold, consider matchmaking complete
			// check our results aren't polluted with a high volume of solo sessions
			if((nResults >= m_nNumActivityResultsBeforeEarlyOut) && ((nResults - nResultsSolo) >= m_nNumActivityOverallResultsBeforeEarlyOutNonSolo))
			{
				// log that we're done
				gnetDebug1("UpdateTransitionMatchmaking :: Matchmaking has %u results (%u required). Earlying out - Time: %dms!", nResults, m_nNumActivityResultsBeforeEarlyOut, nCurrentTime - m_nTransitionMatchmakingStarted);

				// cancel any pending queries
				for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].IsBusy())
				{
					gnetDebug1("UpdateTransitionMatchmaking :: Cancelling Query %s[%d], Results: %u", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i, m_ActivityMMQ[i].m_nResults);
					netTask::Cancel(&m_ActivityMMQ[i].m_TaskStatus);
				}

				// flag that we're finished
				bFinished = true;
			}
		}

		// if we're still not finished, bail
		if(!bFinished)
			return;
	}

	// kill tracking flag
	m_bIsTransitionMatchmaking = false; 

	// make a note of when we got these results
	m_TransitionMMResults.m_MatchmakingRetrievedTime = sysTimer::GetSystemMsTime();

	// clear out blacklisted sessions
	ClearBlacklistedSessions();

	// process platform matchmaking
	ProcessMatchmakingResults(m_ActivityMMQ, ActivityMatchmakingQuery::AMQ_Num, &m_TransitionMMResults, m_nTransitionMatchmakingFlags, m_nUniqueMatchmakingID[SessionType::ST_Transition]);

	// start from the top
	if(!CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_QueryOnly))
	{
		m_TransitionMMResults.m_nCandidateToJoinIndex = 0;
		JoinOrHostTransition(m_TransitionMMResults.m_nCandidateToJoinIndex);
	}
}

void CNetworkSession::UpdateTransitionGamerData()
{
    // update the local player data
    m_TransitionData.m_PlayerData.m_hGamer = NetworkInterface::GetLocalGamerHandle();
    m_TransitionData.m_PlayerData.m_nCharacterRank = static_cast<u16>(StatsInterface::GetIntStat(STAT_RANK_FM.GetStatId()));
    m_TransitionData.m_PlayerData.m_nELO = static_cast<u16>(m_nMatchmakingELO);
    m_TransitionData.m_PlayerData.m_nClanID = static_cast<rlClanId>(RL_INVALID_CLAN_ID);
	m_TransitionData.m_PlayerData.m_NatType = netHardware::GetNatType();
	m_TransitionData.m_PlayerData.m_bIsRockstarDev = NetworkInterface::IsRockstarDev();
	m_TransitionData.m_PlayerData.m_bIsAsync = m_bIsTransitionAsyncWait;
	NetworkClan& tClan = CLiveManager::GetNetworkClan();
    if(tClan.HasPrimaryClan())
        m_TransitionData.m_PlayerData.m_nClanID = tClan.GetPrimaryClan()->m_Id;
}

void CNetworkSession::SetActivityPlayersMax(unsigned nPlayersMax)
{
	// validate that we're within session limits
	if(!gnetVerify(nPlayersMax <= RL_MAX_GAMERS_PER_SESSION))
	{
		gnetError("SetActivityPlayersMax :: Higher than Maximum Session Limit, Limit: %u, Supplied: %u. Capping value.", RL_MAX_GAMERS_PER_SESSION, nPlayersMax);
		nPlayersMax = RL_MAX_GAMERS_PER_SESSION;
	}

	// if we're currently active, prevent this being set to an invalid value
	if((IsTransitionActive() && (IsTransitionHostPending() || IsTransitionCreated())) || IsActivitySession())
	{
		unsigned nMaxSlots = IsTransitionActive() ? m_Config[SessionType::ST_Transition].GetMaxSlots() : m_Config[SessionType::ST_Physical].GetMaxSlots();
		unsigned nPlayersMaxLimit = nMaxSlots - m_nActivitySpectatorsMax;
		if(!gnetVerify(nPlayersMax <= nPlayersMaxLimit))
		{
			gnetError("SetActivityPlayersMax :: Higher than Active Session Limit, Limit: %u (%u / %u), Supplied: %u. Capping value.", nPlayersMaxLimit, nMaxSlots, m_nActivitySpectatorsMax, nPlayersMax);
			nPlayersMax = nPlayersMaxLimit;
		}
	}

	if(m_nActivityPlayersMax != nPlayersMax)
    {
		gnetDebug3("SetActivityPlayerMax :: Maximum set to %u", nPlayersMax);
		m_nActivityPlayersMax = nPlayersMax;
	}

	// update our activity slots
	UpdateActivitySlots();
}

void CNetworkSession::SetActivitySpectatorsMax(unsigned nSpectatorsMax)
{
	// validate that we're within session limits
	if(!gnetVerify(nSpectatorsMax <= RL_MAX_GAMERS_PER_SESSION))
	{
		gnetError("SetActivitySpectatorsMax :: Higher than Maximum Session Limit, Limit: %u, Supplied: %u. Capping value.", RL_MAX_GAMERS_PER_SESSION, nSpectatorsMax);
		nSpectatorsMax = RL_MAX_GAMERS_PER_SESSION;
	}

	// if we're currently active, prevent this being set to an invalid value
	if((IsTransitionActive() && (IsTransitionHostPending() || IsTransitionCreated())) || IsActivitySession())
	{
		unsigned nMaxSlots = IsActivitySession() ? m_Config[SessionType::ST_Physical].GetMaxSlots() : m_Config[SessionType::ST_Transition].GetMaxSlots();
		unsigned nSpectatorsMaxLimit = nMaxSlots - m_nActivityPlayersMax;
		if(!gnetVerify(nSpectatorsMax <= nSpectatorsMaxLimit))
		{
			gnetError("SetActivitySpectatorsMax :: Higher than Active Session Limit, Limit: %u (%u / %u), Supplied: %u. Capping value.", nSpectatorsMaxLimit, nMaxSlots, m_nActivityPlayersMax, nSpectatorsMax);
			nSpectatorsMax = nSpectatorsMaxLimit;
		}
	}

    if(m_nActivitySpectatorsMax != nSpectatorsMax)
    {
        gnetDebug3("SetActivitySpectatorsMax :: Maximum set to %u", nSpectatorsMax);
        m_nActivitySpectatorsMax = nSpectatorsMax;
    }
    m_bHasSetActivitySpectatorsMax = true;
}

void CNetworkSession::SetActivitySpectator(bool bIsSpectator)
{
    if(m_TransitionData.m_PlayerData.m_bIsSpectator != bIsSpectator)
    {
        gnetDebug3("SetActivitySpectator :: Setting as %s", bIsSpectator ? "Spectator" : "Player");
        m_TransitionData.m_PlayerData.m_bIsSpectator = bIsSpectator;
    }
}

unsigned CNetworkSession::GetActivityPlayerNum(bool bSpectator)
{
    // verify that we're in a valid activity
    if(!gnetVerify(IsActivitySession() || IsTransitionActive()))
    {
        gnetError("IsActivitySpectator :: Not in activity!");
        return 0;
    }

    unsigned nPlayers = 0;

    // in a launched transition...
    if(IsActivitySession())
    {
        rlGamerInfo aInfo[RL_MAX_GAMERS_PER_SESSION];
        const unsigned nGamers = m_pSession->GetGamers(aInfo, COUNTOF(aInfo));
        for(unsigned i = 0; i < nGamers; ++i)
        {
            CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(aInfo[i].GetGamerId());
            if(pPlayer && (bSpectator == pPlayer->IsSpectator()))
                nPlayers++;
        }
    }
    else if(IsTransitionActive())
    {
        int nTransitionPlayers = m_TransitionPlayers.GetCount();
        for(int i = 0; i < nTransitionPlayers; i++)
        {
            if(bSpectator == m_TransitionPlayers[i].m_bIsSpectator)
                nPlayers++;
        }
    }

    // 
    return nPlayers;
}

bool CNetworkSession::IsActivitySpectator(const rlGamerHandle& hGamer)
{
    // verify that we're in a valid activity
    if(!gnetVerify(IsActivitySession() || IsTransitionActive()))
    {
        gnetError("IsActivitySpectator :: Not in activity!");
        return false;
    }

    // in a launched transition...
    if(IsActivitySession())
    {
        CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
        if(pPlayer)
            return pPlayer->IsSpectator();
    }
    else if(IsTransitionActive())
    {
        int nTransitionPlayers = m_TransitionPlayers.GetCount();
        for(int i = 0; i < nTransitionPlayers; i++)
        {
            if(m_TransitionPlayers[i].m_hGamer == hGamer)
                return m_TransitionPlayers[i].m_bIsSpectator;
        }
    }

    // no record for this player
    return false;
}

unsigned CNetworkSession::GetActivityPlayerMax(bool bSpectator)
{
    // verify that we're in a valid activity
    if(!gnetVerify(IsActivitySession() || IsTransitionActive()))
    {
        gnetError("GetActivityPlayerMax :: Not in activity!");
        return 0;
    }

    if(bSpectator)
        return m_nActivitySpectatorsMax;
    else
		return m_nActivityPlayersMax;
}

void CNetworkSession::UpdateActivitySlots()
{
    NetworkGameConfig* pConfig = NULL; 
    bool* bChangeParam = NULL;
   
    if(IsActivitySession() && !m_pSession->IsMigrating())
    {
        // only the session host can change this
        if(!m_pSession->IsHost()) 
            return;

        pConfig = &m_Config[SessionType::ST_Physical];
        bChangeParam = &m_bChangeAttributesPending[SessionType::ST_Physical];
    }
    else if(IsTransitionActive() && !m_pTransition->IsMigrating())
    {
        // only the session host can change this
        if(!m_pTransition->IsHost()) 
            return;

        pConfig = &m_Config[SessionType::ST_Transition];
        bChangeParam = &m_bChangeAttributesPending[SessionType::ST_Transition];
    }

    // no config, just bail
    if(!pConfig)
        return;

	// prevent wrap around - activity player maximum can be changed mid-session so make 
	// sure that we cope with the current number being greater than the maximum
	unsigned nActivityPlayerNum = GetActivityPlayerNum(false);
	unsigned nActivityPlayerMax = GetActivityPlayerMax(false);
    unsigned nSlotsNow = (nActivityPlayerMax > nActivityPlayerNum) ? (nActivityPlayerMax - nActivityPlayerNum) : 0;

	// check if we need to update our slots
    unsigned nSlotsPrevious = pConfig->GetActivityPlayersSlotsAvailable();
    if(nSlotsPrevious != nSlotsNow)
    {
        gnetDebug2("UpdateActivitySlots :: Activity Player Slots Available: %d, Was: %d", nSlotsNow, nSlotsPrevious);
        pConfig->SetActivityPlayersSlotsAvailable(nSlotsNow);
        
        // flag attributes for update
        *bChangeParam = true;
    }

    // reset check interval
    m_nLastActivitySlotsCheck = sysTimer::GetSystemMsTime();
}

bool CNetworkSession::DoTransitionQuickmatch(const int nGameMode, unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, const int nActivityIsland, const unsigned nMmFlags, rlGamerHandle* pGamers, unsigned nGamers)
{
	if(!gnetVerify(!IsTransitionActive()))
	{
		gnetError("DoTransitionQuickmatch :: Already in transition!");
		return false;
	}

	if(!gnetVerify(!m_bIsTransitionMatchmaking))
	{
		gnetError("DoTransitionQuickmatch :: Already matchmaking for transition sessions!");
		return false;
	}

    // check we have access via profile and privilege
	if (!VerifyMultiplayerAccess("DoTransitionQuickmatch"))
		return false;

	// we don't want to quickmatch - we don't know how many players will be required for the session
	// should we fail to join a viable candidate. Instead, tell script and they can handle the host. 
	m_nTransitionMatchmakingFlags = nMmFlags | MatchmakingFlags::MMF_AllowBlacklisted | MatchmakingFlags::MMF_SortInQueryOrder;
    OUTPUT_ONLY(LogMatchmakingFlags(m_nTransitionMatchmakingFlags, __FUNCTION__));

	// if we are running with in progress jobs, make sure that the bail from launched job flag is not on
	if(!CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_DisableInProgressQueries))
	{
		gnetAssertf(!CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_BailFromLaunchedJobs), "DoTransitionQuickmatch :: Cannot use MatchmakingFlags::MMF_BailFromLaunchedJobs for in progress queries!");
		m_nTransitionMatchmakingFlags &= ~MatchmakingFlags::MMF_BailFromLaunchedJobs;
	}

	// logging
	gnetDebug1("DoTransitionQuickmatch :: With %d gamers, GameMode: %s [%d], ActivityType: %d, ActivityId: 0x%08x, ActivityIsland: %d. Flags: 0x%x", nGamers, NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nActivityType, nActivityID, nActivityIsland, m_nTransitionMatchmakingFlags);

    // only run this if we are in a quickmatch and will end up hosting, otherwise not required
    // as above, this code will not run as it stands, but leaving this to highlight what would be needed
    if(CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch))
    {
	    // verify max players
	    if(!gnetVerify(nMaxPlayers > 0))
	    {
		    gnetError("DoTransitionQuickmatch :: Invalid Max Players: %d. Must be at least 1!", nMaxPlayers);
		    return false;
	    }

		// verify max players
		if(!gnetVerify(nMaxPlayers <= RL_MAX_GAMERS_PER_SESSION))
		{
			gnetError("DoTransitionQuickmatch :: Invalid Max Players: %d. Cannot be more than %d!", nMaxPlayers, RL_MAX_GAMERS_PER_SESSION);
			nMaxPlayers = RL_MAX_GAMERS_PER_SESSION;
		}
    }

	// check for command line override
	m_nTransitionQuickmatchFallbackMaxPlayers = nMaxPlayers;
	NOTFINAL_ONLY(PARAM_netSessionMaxPlayersActivity.Get(m_nTransitionQuickmatchFallbackMaxPlayers));

    // reset gamer tracking
    m_nGamersToActivity = 0;
    m_bRetainActivityGroupOnQuickmatchFail = false;

    // copy in gamers
    if(nGamers > 0)
    {
        bool bLocalFound = false;
        rlGamerInfo hGamerInfo;
        for(unsigned i = 0; i < nGamers; i++)
        {
            // validate gamer is in session
            bool bGamerFound = m_pSession->GetGamerInfo(pGamers[i], &hGamerInfo);
            if(bGamerFound)
            {
                gnetDebug1("\tDoTransitionQuickmatch :: With %s", hGamerInfo.GetName());
                m_hGamersToActivity[m_nGamersToActivity++] = pGamers[i];

                // track local
                if(hGamerInfo.IsLocal())
                    bLocalFound = true; 
            }
        }
        gnetDebug1("\tDoTransitionQuickmatch :: Found %d Gamers", m_nGamersToActivity);

        // verify that the local gamer was passed
        if(!gnetVerify(bLocalFound))
        {
            gnetError("DoTransitionQuickmatch :: Local gamer not provided!");
            m_hGamersToActivity[m_nGamersToActivity++] = NetworkInterface::GetLocalGamerHandle();
        }

        // only the local player - just cancel the grouping
        if(bLocalFound && m_nGamersToActivity == 1)
        {
            gnetDebug1("\tDoTransitionQuickmatch :: Only the local player");
            m_nGamersToActivity = 0;
        }
    }

	// check which queries need to run
	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++)
	{
		// reset all queries
		m_ActivityMMQ[i].Reset(MatchmakingQueryType::MQT_Standard);

		// skip specific queries when we haven't searched for a specific activity ID
		if(((i == ActivityMatchmakingQuery::AMQ_SpecificJob) || (i == ActivityMatchmakingQuery::AMQ_InProgressSpecificJob)) && nActivityID == 0)
		{
			gnetDebug1("\tDoTransitionQuickmatch :: Skipping Specific Query %s[%d], No Activity ID", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i);
			continue; 
		}
		// skip any job queries when we've searched for specific job with tunable / flag disabled
		if(((i == ActivityMatchmakingQuery::AMQ_AnyJob) || (i == ActivityMatchmakingQuery::AMQ_InProgressAnyJob)) && (m_bActivityMatchmakingDisableAnyJobQueries || CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_DisableAnyJobQueries)) && nActivityID != 0)
		{
			gnetDebug1("\tDoTransitionQuickmatch :: Skipping Any Query %s[%d], Flag: %s, Disabled: %s", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i, CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_DisableAnyJobQueries) ? "True" : "False", m_bActivityMatchmakingDisableAnyJobQueries ? "True" : "False");
			continue;
		}
		// skip in progress job queries with tunable disabled
		if(((i == ActivityMatchmakingQuery::AMQ_InProgressSpecificJob) || (i == ActivityMatchmakingQuery::AMQ_InProgressAnyJob)) && (m_bActivityMatchmakingDisableInProgressQueries || CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_DisableInProgressQueries)))
		{
			gnetDebug1("\tDoTransitionQuickmatch :: Skipping In Progress Query %s[%d], Flag: %s, Disabled: %s", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i, CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_DisableInProgressQueries) ? "True" : "False", m_bActivityMatchmakingDisableInProgressQueries ? "True" : "False");
			continue;
		}
        // skip jobs not in progress with flag enabled
        if(((i == ActivityMatchmakingQuery::AMQ_SpecificJob) || (i == ActivityMatchmakingQuery::AMQ_AnyJob)) && CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_InProgressOnly))
        {
            gnetDebug1("\tDoTransitionQuickmatch :: Skipping Not In Progress Query %s[%d], Flag: %s", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i, CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_InProgressOnly) ? "True" : "False");
            continue;
        }

		gnetDebug1("\tDoTransitionQuickmatch :: Executing Query %s[%d]", GetActivityMatchmakingQueryAsString(static_cast<ActivityMatchmakingQuery>(i)), i);

		// setup matchmaking filter
		player_schema::MatchingFilter_Activity mmSchema;
		m_TransitionFilter[i].Reset(mmSchema);
        m_TransitionFilter[i].SetGameMode(static_cast<u16>(nGameMode));
        m_TransitionFilter[i].SetSessionPurpose(SessionPurpose::SP_Activity);

		// for in progress searches, apply the in progress parameter
		if((i == ActivityMatchmakingQuery::AMQ_InProgressSpecificJob) || (i == ActivityMatchmakingQuery::AMQ_InProgressAnyJob))
			m_TransitionFilter[i].SetInProgress(true);

		// if we're not spectating, we need to request a player slot
		// we do not have enough matchmaking attributes to do the same for spectators
		if(!m_TransitionData.m_PlayerData.m_bIsSpectator)
			m_TransitionFilter[i].SetActivitySlotsRequired((m_nGamersToActivity > 0) ? m_nGamersToActivity : 1);

		// check if we've been given an activity type
		if(nActivityType != INVALID_ACTIVITY_TYPE)
		{
			MatchmakingAttributes::sAttribute* pAttribute = m_MatchmakingAttributes.GetAttribute(MatchmakingAttributes::MM_ATTR_ACTIVITY_TYPE);
			pAttribute->Apply(static_cast<unsigned>(nActivityType));
			m_TransitionFilter[i].SetActivityType(pAttribute->GetValue());
		}

		// check if we've been given an activity ID, ignore if we're looking for any job
		if(nActivityID != 0 && (i != ActivityMatchmakingQuery::AMQ_AnyJob) && (i != ActivityMatchmakingQuery::AMQ_InProgressAnyJob))
		{
			MatchmakingAttributes::sAttribute* pAttribute = m_MatchmakingAttributes.GetAttribute(MatchmakingAttributes::MM_ATTR_ACTIVITY_ID);
			pAttribute->Apply(nActivityID);
			m_TransitionFilter[i].SetActivityID(pAttribute->GetValue());
		}

		// if we've been given an activity island
		if(nActivityIsland != NO_ACTIVITY_ISLAND && !m_bActivityMatchmakingActivityIslandDisabled)
			m_TransitionFilter[i].SetActivityIsland(static_cast<unsigned>(nActivityIsland));

		// check if we should filter content
		if(!m_bActivityMatchmakingContentFilterDisabled)
		{
			// check if we should be looking for activities using user created content only
			if(CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_RockstarCreatedOnly))
				m_TransitionFilter[i].SetContentFilter(ACTIVITY_CONTENT_SEARCH_ROCKSTAR_ONLY);
			// check if we should be looking for activities using user created content only
			else if(CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_UserCreatedOnly))
				m_TransitionFilter[i].SetContentFilter(ACTIVITY_CONTENT_SEARCH_USER_ONLY);
		}
		
		// apply region / language settings
		ApplyMatchmakingRegionAndLanguage(&m_TransitionFilter[i], m_bUseRegionMatchmakingForActivity, m_bUseLanguageMatchmakingForActivity);

		// start search
		if(!FindActivitySessions(&m_ActivityMMQ[i]))
		{
			gnetError("DoTransitionQuickmatch :: Failed to start search for sessions!");
			return false;
		}
	}

	// log out filter contents - just use the first active one
	for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++) if(m_ActivityMMQ[i].m_bIsActive)
	{
		m_TransitionFilter[i].DebugPrint();
		break;
	}

    // send message to clients
    if(m_nGamersToActivity > 0)
    {
        MsgTransitionToActivityStart msg;
		msg.Reset(NetworkInterface::GetLocalGamerHandle(), CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_Asynchronous));

        for(unsigned i = 0; i < m_nGamersToActivity; i++)
        {	
            // grab gamer info for peer ID to message
            rlGamerInfo hInfo;
            m_pSession->GetGamerInfo(m_hGamersToActivity[i], &hInfo);
            if(!hInfo.IsLocal())
                m_pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
        }
    }

	// matchmaking is active
	m_bIsTransitionMatchmaking = true; 
    m_nTransitionMatchmakingStarted = sysTimer::GetSystemMsTime();

    // aggressive for matchmaking
    SetSessionPolicies(POLICY_AGGRESSIVE);

	// send start telemetry
	m_nUniqueMatchmakingID[SessionType::ST_Transition] = SendMatchmakingStartTelemetry(m_ActivityMMQ, ActivityMatchmakingQuery::AMQ_Num);

	// generate script event
	GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_QUICKMATCH));
	return true; 
}

bool CNetworkSession::ValidateMatchmakingQuery(sMatchmakingQuery* pQuery)
{
	// check filter is valid
	if(!gnetVerify(pQuery->m_pFilter != NULL))
	{
		gnetError("ValidateMatchmakingQuery :: Matchmaking filter not set!");
		return false;
	}

	// check filter is valid
	if(!gnetVerify(pQuery->m_pFilter->IsValid()))
	{
		gnetError("ValidateMatchmakingQuery :: Matchmaking filter not valid!");
		return false;
	}

	return true;
}

bool CNetworkSession::FindActivitySessions(sMatchmakingQuery* pQuery)
{
	// validate the query is setup / initialised correctly
	if(!ValidateMatchmakingQuery(pQuery))
		return false;

	// reset the find results
	pQuery->Reset(MatchmakingQueryType::MQT_Standard);

	// search for sessions to join
	bool bSuccess = rlSessionFinder::Find(NetworkInterface::GetLocalGamerIndex(),
										  NetworkInterface::GetNetworkMode(),
										  m_pTransition->GetChannelId(),
										  (m_nGamersToActivity > 0) ? m_nGamersToActivity : 1,
										  pQuery->m_pFilter->GetMatchingFilter(),
										  pQuery->m_Results,
										  MAX_MATCHMAKING_RESULTS,
										  m_nNumQuickmatchResultsBeforeEarlyOut,
										  &pQuery->m_nResults,
										  &pQuery->m_TaskStatus);

	if(!gnetVerify(bSuccess))
	{
		gnetError("FindActivitySessions :: Failed to start session finder!");
		return false;
	}

	// set tracking states
	pQuery->m_bIsActive = true;
	pQuery->m_bIsPending = true;

	// success
	return true; 
}

void CNetworkSession::CompleteTransitionQuickmatch()
{
    gnetDebug1("CompleteTransitionQuickmatch :: Success: %s", m_pTransition->Exists() ? "True" : "False");

    // send message to clients
    if(m_nGamersToActivity > 0)
    {
        MsgTransitionToActivityFinish msg;
        if(m_pTransition->Exists())
			msg.Reset(NetworkInterface::GetLocalGamerHandle(), m_pTransition->GetSessionInfo(), m_pTransition->GetMaxSlots(RL_SLOT_PUBLIC) == 0);
        else
			msg.Reset(NetworkInterface::GetLocalGamerHandle());

        rlGamerInfo hInfo;
        for(unsigned i = 0; i < m_nGamersToActivity; i++)
        {	
            // grab gamer info for peer ID to message - they might have left since, so check
            bool bGamerFound = m_pSession->GetGamerInfo(m_hGamersToActivity[i], &hInfo);
            if(bGamerFound && !hInfo.IsLocal())
                m_pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
        }
    }

    // send message to followers
    if(m_nFollowers > 0)
    {
        gnetDebug1("CompleteTransitionQuickmatch :: Inviting %d followers", m_nFollowers);
        CFollowInvite::Trigger(m_hFollowers, m_nFollowers, m_pTransition->GetSessionInfo());
        if(!m_bRetainFollowers)
            ClearFollowers();
    }
}

void CNetworkSession::MarkAsPreferredActivity(bool bIsPreferred)
{
	u8 nIsPreferred = (bIsPreferred ? TRUE : FALSE);
	if(m_SessionUserDataActivity.m_bIsPreferred != nIsPreferred)
	{
		gnetDebug2("MarkAsPreferredActivity :: Setting to %s", bIsPreferred ? "True" : "False");
		m_SessionUserDataActivity.m_bIsPreferred = nIsPreferred;
		m_HasDirtySessionUserDataActivity = true;
	}
}

void CNetworkSession::MarkAsWaitingAsync(bool bIsWaitingAsync)
{
	u8 nIsWaitingAsync = (bIsWaitingAsync ? TRUE : FALSE);
	if(m_SessionUserDataActivity.m_bIsWaitingAsync != nIsWaitingAsync)
	{
		gnetDebug2("MarkAsWaitingAsync :: Setting to %s", bIsWaitingAsync ? "True" : "False");
		m_SessionUserDataActivity.m_bIsWaitingAsync = nIsWaitingAsync;
		m_HasDirtySessionUserDataActivity = true;
	}
}

void CNetworkSession::SetInProgressFinishTime(unsigned nInProgressFinishTime)
{
	if(m_SessionUserDataActivity.m_InProgressFinishTime != nInProgressFinishTime)
	{
		gnetDebug2("SetInProgressFinishTime :: Setting to %u", nInProgressFinishTime);
		m_SessionUserDataActivity.m_InProgressFinishTime = nInProgressFinishTime;
		m_HasDirtySessionUserDataActivity = true;
	}
}

void CNetworkSession::SetActivityCreatorHandle(const rlGamerHandle& hGamer)
{
	if(!m_SessionUserDataActivity.m_hContentCreator.Equals(hGamer, true))
	{
		gnetDebug3("SetActivityCreatorHandle :: %s -> %s", m_SessionUserDataActivity.m_hContentCreator.ToString(), hGamer.ToString());
		m_SessionUserDataActivity.m_hContentCreator = hGamer;
		m_HasDirtySessionUserDataActivity = true;
	}
}

void CNetworkSession::SetNumBosses(const unsigned nNumBosses)
{
	if(m_SessionUserDataFreeroam.m_nNumBosses != static_cast<u8>(nNumBosses))
	{
		gnetDebug3("SetNumBosses :: %u Bosses", nNumBosses);
		m_SessionUserDataFreeroam.m_nNumBosses = static_cast<u8>(nNumBosses);
		m_HasDirtySessionUserDataFreeroam = true; 
	}
}

unsigned CNetworkSession::GetNumBossesInFreeroam() const
{
	// get the current number of bosses that have completed a join
	unsigned numBosses = m_SessionUserDataFreeroam.m_nNumBosses;

	// validate that our physical session is freeroam and then count up any pending joiners that are also bosses
	if(m_pSession->IsEstablished() && m_pSession->GetConfig().m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Freeroam)
	{
		int numPendingJoiners = m_PendingJoiners[SessionType::ST_Physical].m_Joiners.GetCount();
		for(unsigned j = 0; j < numPendingJoiners; j++)
		{
			if((m_PendingJoiners[SessionType::ST_Physical].m_Joiners[j].m_JoinFlags & JoinFlags::JF_IsBoss) != 0)
				numBosses++;
		}
	}

	return numBosses;
}

bool CNetworkSession::JoinGroupActivity()
{
	gnetDebug1("JoinGroupActivity :: Token: 0x%016" I64FMT "x, Async: %s", m_ActivityGroupSession.GetToken().m_Value, m_bIsActivityGroupQuickmatchAsync ? "True" : "False");
	
    bool bSuccess = false;
    if(m_ActivityGroupSession.IsValid())
	{
		// build join flags
		unsigned nJoinFlags = JoinFlags::JF_ViaInvite;
		if(m_bIsActivityGroupQuickmatchAsync)
			nJoinFlags |= JoinFlags::JF_Asynchronous;

        bSuccess = JoinTransition(m_ActivityGroupSession, RL_SLOT_PUBLIC, nJoinFlags);
	}

    return bSuccess;
}

void CNetworkSession::ClearGroupActivity()
{
	if(m_ActivityGroupSession.IsValid())
	{
		gnetDebug1("ClearGroupActivity :: Token: 0x%016" I64FMT "x", m_ActivityGroupSession.GetToken().m_Value);
		m_ActivityGroupSession.Clear();
		m_bIsActivityGroupQuickmatchAsync = false;
	}
}

void CNetworkSession::RetainActivityGroupOnQuickmatchFail()
{
	if(!m_bRetainActivityGroupOnQuickmatchFail)
	{
		gnetDebug1("RetainActivityGroupOnQuickmatchFail");
		m_bRetainActivityGroupOnQuickmatchFail = true;
	}
}

bool CNetworkSession::IsNextJobTransition(const rlGamerHandle& hGamer, const rlSessionInfo& hInfo)
{
	if((hGamer == m_hToGameManager) && (m_hActivityQuickmatchManager == hGamer))
	{
		// grab this gamer to check if they have this session set as their current transition
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
		if(pPlayer && pPlayer->GetTransitionSessionInfo() == hInfo)
			return true;
	}

	return false;
}

bool CNetworkSession::CanTransitionToGame()
{
    if(IsTransitionLeaving())
    {
        gnetDebug3("CanTransitionToGame :: Leave pending on transition!");
        return false;
    }

    return true; 
}

bool CNetworkSession::DoTransitionToGame(const int nGameMode, rlGamerHandle* pGamers, unsigned nGamers, bool bSocialMatchmaking, unsigned nMaxPlayers, unsigned nMmFlags)
{
    gnetDebug1("DoTransitionToGame :: With %d Gamers, GameMode: %s [%d], Social Matchmaking: %s, MatchmakingFlags: 0x%x", nGamers, NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, bSocialMatchmaking ? "T" : "F", nMmFlags);

    // soft checks
    if(!CanTransitionToGame())
        return false;

	// not allowed to run multiple transitions
	if(!gnetVerify(!m_bIsTransitionToGame))
	{
		gnetError("DoTransitionToGame :: Already transitioning back to game!");
		return false;
	}

	// if another player has started a transition, we must wait for it to complete / fail
	if(!gnetVerify(!m_hTransitionHostGamer.IsValid()))
	{
		gnetError("DoTransitionToGame :: %s has already started a transition!", NetworkUtils::LogGamerHandle(m_hTransitionHostGamer));
		return false;
	}

	// verify session states
	snSession* pSession = nullptr;
	if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_ToGameViaTransiton))
	{
		if(!gnetVerify(IsTransitionActive()))
		{
			gnetError("DoTransitionToGame :: Not in a transition session!");
			return false;
		}

		if (!gnetVerify(!IsSessionActive()))
		{
			gnetError("DoTransitionToGame :: Transition via lobby but main session is still active!");
			return false;
		}

		pSession = m_pTransition;
	}
	else
	{
		if(!gnetVerify(!IsTransitionActive()))
		{
			gnetError("DoTransitionToGame :: In a transition session!");
			return false;
		}

		// we can only switch from an activity or from a transition 'lobby'
		if(!gnetVerify(IsActivitySession()))
		{
			gnetError("DoTransitionToGame :: Not in activity session!");
			return false;
		}

		pSession = m_pSession;
	}

	// common setup for matchmaking
	nMmFlags |= MatchmakingFlags::MMF_Quickmatch | MatchmakingFlags::MMF_AllowBlacklisted;
	if(!DoMatchmakingCommon(nGameMode, SCHEMA_GROUPS, nMaxPlayers, nMmFlags))
		return false;

	// check if this is job to job
	if(CheckFlag(nMmFlags, MatchmakingFlags::MMF_JobToJob))
	{
		gnetDebug1("\tDoTransitionToGame :: Job to Job transition");
		m_bFreeroamIsJobToJob = true;
	}

	// reset gamer tracking
	m_nGamersToGame = 0;

	// copy in gamers
	if(nGamers > 0)
	{
		bool bLocalFound = false;
		rlGamerInfo hGamerInfo;
		for(unsigned i = 0; i < nGamers; i++)
		{
			// validate gamer is in session
			bool bGamerFound = pSession->GetGamerInfo(pGamers[i], &hGamerInfo);
			if(bGamerFound)
			{
				gnetDebug1("\tDoTransitionToGame :: With %s", hGamerInfo.GetName());
				m_hGamersToGame[m_nGamersToGame++] = pGamers[i];

				// track local
				if(hGamerInfo.IsLocal())
					bLocalFound = true; 
			}
		}
		gnetDebug1("\tDoTransitionToGame :: Found %d Gamers", m_nGamersToGame);

		// verify that the local gamer was passed
		if(!gnetVerify(bLocalFound))
		{
			gnetError("DoTransitionToGame :: Local gamer not provided!");
			m_hGamersToGame[m_nGamersToGame++] = NetworkInterface::GetLocalGamerHandle();
		}

		// only the local player - just cancel the grouping
		if(bLocalFound && m_nGamersToGame == 1)
		{
			gnetDebug1("\tDoTransitionToGame :: Only the local player");
			m_nGamersToGame = 0;
		}
	}

    // work out how many players of each type are transferring
    unsigned nPlayers = 0;
    unsigned nSCTV = 0;

    // if we are using a number of gamers, check each
    if(m_nGamersToGame > 0)
    {
        for(unsigned i = 0; i < m_nGamersToGame; i++)
            IsActivitySpectator(m_hGamersToGame[i]) ? nSCTV++ : nPlayers++;
    }
    else // just the local player
        IsActivitySpectator() ? nSCTV++ : nPlayers++;

    // freeroam players
    if(nPlayers > 0)
    {
		// if we're matchmaking as a group
		int nGroupIndex = GetActiveMatchmakingGroupIndex(MM_GROUP_FREEMODER);
		if(nGroupIndex != -1)
		{
			// limit includes one player, if we're pulling everyone, make sure there's space for the group
            unsigned nLimit;
#if !__FINAL
        // grab any overridden group maximums
            if(!PARAM_netSessionMaxPlayersFreemode.Get(nLimit))
#endif
            {
                nLimit = m_MatchmakingGroupMax[MM_GROUP_FREEMODER];
            }

            // The group limit check works by comparing the current member count (set by the host)
            // versus the limit set in the filter (specifically, MEMBERS < LIMIT)
            // This check accounts for one player so we minus one from the player count and remove this
            // from the limit for this group provided by script
            // A check for 4 gamers would be (with a limit of 16), MEMBERS < 13, meaning that if we
            // currently have less than 13 gamers, we can add 4. About as clever as it gets with 
            // the comparison functionality available
            nLimit -= (nPlayers - 1);

            // apply limit
            m_SessionFilter.SetGroupLimit(nGroupIndex, nLimit);
        }
    }

    // SCTV players
    if(nSCTV > 0)
    {
        // if we're matchmaking as a group
        int nGroupIndex = GetActiveMatchmakingGroupIndex(MM_GROUP_SCTV);
        if(nGroupIndex != -1)
        {
            // limit includes one player, if we're pulling everyone, make sure there's space for the group
            unsigned nLimit;
#if !__FINAL
            // grab any overridden group maximums
            if(!PARAM_netSessionMaxPlayersSCTV.Get(nLimit))
#endif
            {
                nLimit = m_MatchmakingGroupMax[MM_GROUP_SCTV];
            }

            nLimit -= (nSCTV - 1);
            m_SessionFilter.SetGroupLimit(nGroupIndex, nLimit);
        }   
    }

	// apply region / language settings
	ApplyMatchmakingRegionAndLanguage(&m_SessionFilter, m_bUseRegionMatchmakingForToGame, m_bUseLanguageMatchmakingForToGame);

    // log out filter contents
    m_SessionFilter.DebugPrint();

    // we need to take the local player position now in the event that it's chucked 
    CPed* pLocalPed = CGameWorld::FindLocalPlayer();
    if(pLocalPed)
        m_vToGamePosition = VEC3V_TO_VECTOR3(pLocalPed->GetTransform().GetPosition());
    else
        m_vToGamePosition.Zero(); 

    // we want to end up in a session
    m_nQuickMatchStarted = sysTimer::GetSystemMsTime();

    // 'regular' quick match calls friends, social and platform matchmaking
    bool bSuccess = true; 

    // if we're coming back on our own - we can run a social check
    if((m_nGamersToGame == 0 || bSocialMatchmaking)
#if !__FINAL
        && !PARAM_netSessionDisableSocialMM.Get()
#endif
        )
    {
        // check we're connected to ROS
        if(NetworkInterface::HasValidRosCredentials())
        {
            // friends query
			bSuccess &= FindSessionsFriends(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Friends], false);

            // social query - if we have a current crew
            NetworkClan& tClan = CLiveManager::GetNetworkClan();
            if(tClan.HasPrimaryClan())
            {
                char szParams[256];
                snprintf(szParams, 256, "@crewid,%" I64FMT "d", tClan.GetPrimaryClan()->m_Id);
                bSuccess &= FindSessionsSocial(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Social], "CrewmateSessions", szParams);
            }
        }
    }

    // platform query
	//@@: range CNETWORKSESSION_DOTRANSITIONTOGAME_FINDSESSIONS {
	bSuccess &= FindSessions(&m_MatchMakingQueries[MatchmakingQueryType::MQT_Standard]);
	//@@: } CNETWORKSESSION_DOTRANSITIONTOGAME_FINDSESSIONS

    // set searching state if successful
    if(bSuccess)
        SetSessionState(SS_FINDING);

#if !__NO_OUTPUT
    m_nToGameTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

    // swap sessions (if this isn't via a transition lobby)
	if(!CheckFlag(nMmFlags, MatchmakingFlags::MMF_ToGameViaTransiton))
	{
		SwapToTransitionSession();
	}

	// send message to clients
	if(m_nGamersToGame > 0)
	{
        MsgTransitionToGameStart msg;
		msg.Reset(NetworkInterface::GetLocalGamerHandle(), nGameMode, CheckFlag(nMmFlags, MatchmakingFlags::MMF_ToGameViaTransiton));

        for(unsigned i = 0; i < m_nGamersToGame; i++)
        {	
            // grab gamer info for peer ID to message
            rlGamerInfo hInfo;
            m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
            if(!hInfo.IsLocal())
                m_pTransition->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
        }
	}
	else
	{
		DURANGO_ONLY(RemoveFromParty(true));
		gnetDebug1("\tDoTransitionToGame :: Not with players, leaving activity session!");
		LeaveTransition();
	}

	// transition is active
	m_bIsTransitionToGame = true; 
	m_bIsTransitionToGameInSession = false;
	m_bIsTransitionToGameNotifiedSwap = false;
    m_bIsTransitionToGamePendingLeave = false;
	m_bIsTransitionToGameFromLobby = false;
    m_hTransitionHostGamer.Clear();
    m_nToGameCompletedTimestamp = 0;
	m_bIsLeavingLaunchedActivity = true;

	// no longer an activity session
    m_bIsActivitySession = false;

	// generate script event
	GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_TO_GAME_START));

	return true; 
}

bool CNetworkSession::DoTransitionToNewGame(const int nGameMode, rlGamerHandle* pGamers, unsigned nGamers, unsigned nMaxPlayers, unsigned nHostFlags, const bool bAllowPreviousJoin)
{
    gnetDebug1("DoTransitionToNewGame :: With %d Gamers, GameMode: %s [%d], MaxPlayers: %d, Flags: 0x%x", nGamers, NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nMaxPlayers, nHostFlags);

    // soft checks
    if(!CanTransitionToGame())
        return false;

	// not allowed multiple transitions
    if(!gnetVerify(!m_bIsTransitionToGame))
    {
        gnetError("DoTransitionToNewGame :: Already matchmaking back to game!");
        return false;
    }

	// if another player has started a transition, we must wait for it to complete / fail
	if(!gnetVerify(!m_hTransitionHostGamer.IsValid()))
	{
		gnetError("DoTransitionToGame :: %s has already started a transition!", NetworkUtils::LogGamerHandle(m_hTransitionHostGamer));
		return false;
	}

	// verify session states
	snSession* pSession = nullptr;
	if(CheckFlag(nHostFlags, HostFlags::HF_ViaTransitionLobby))
	{
		if(!gnetVerify(IsTransitionActive()))
		{
			gnetError("DoTransitionToNewGame :: Not in a transition session!");
			return false;
		}

		pSession = m_pTransition;
	}
	else
	{
		if(!gnetVerify(!IsTransitionActive()))
		{
			gnetError("DoTransitionToNewGame :: In a transition session!");
			return false;
		}

		// we can only switch from an activity or from a transition 'lobby'
		if(!gnetVerify(IsActivitySession()))
		{
			gnetError("DoTransitionToNewGame :: Not in activity session!");
			return false;
		}

		pSession = m_pSession;
	}

    // copy in gamers
    if(nGamers > 0)
    {
        // reset gamer tracking
        m_nGamersToGame = 0;

        bool bLocalFound = false;
        rlGamerInfo hGamerInfo;
        for(unsigned i = 0; i < nGamers; i++)
        {
            // validate gamer is in session
            bool bGamerFound = pSession->GetGamerInfo(pGamers[i], &hGamerInfo);
            if(bGamerFound)
            {
                gnetDebug1("\tDoTransitionToNewGame :: With %s", hGamerInfo.GetName());
                m_hGamersToGame[m_nGamersToGame++] = pGamers[i];

                // track local
                if(hGamerInfo.IsLocal())
                    bLocalFound = true; 
            }
        }
        gnetDebug1("\tDoTransitionToNewGame :: Found %d Gamers", m_nGamersToGame);

        // verify that the local gamer was passed
        if(!gnetVerify(bLocalFound))
        {
            gnetError("DoTransitionToNewGame :: Local gamer not provided!");
            m_hGamersToGame[m_nGamersToGame++] = NetworkInterface::GetLocalGamerHandle();
        }

        // only the local player - just cancel the grouping
        if(bLocalFound && m_nGamersToGame == 1)
        {
            gnetDebug1("\tDoTransitionToNewGame :: Only the local player");
            m_nGamersToGame = 0;
        }
    }

    // swap sessions (only if not via lobby) - this needs to happen sooner on hosting
	if(!CheckFlag(nHostFlags, HostFlags::HF_ViaTransitionLobby))
	{
		SwapToTransitionSession();
	}

#if !__NO_OUTPUT
    m_nToGameTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

    // reset state
    SetSessionState(SS_IDLE, true);

    // transition is active
    m_bIsTransitionToGame = true; 
    m_bIsTransitionToGameInSession = false;
	m_bIsTransitionToGameNotifiedSwap = false;
    m_bIsTransitionToGamePendingLeave = false;
	m_bIsTransitionToGameFromLobby = false;
    m_hTransitionHostGamer.Clear();
    m_nToGameCompletedTimestamp = 0;
	m_bIsLeavingLaunchedActivity = true;

    // no longer an activity session
    m_bIsActivitySession = false;

    // generate script event
    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_TO_GAME_START));

    // if this is a private join, check if our previous session was private and if we can re-join it
	// skip if we were in a previously single player private session or if we're setting up a single player private session
	bool bIsPrivate = CheckFlag(nHostFlags, HostFlags::HF_IsPrivate);
	if(bIsPrivate && bAllowPreviousJoin && m_bPreviousWasPrivate && (m_nPreviousMaxSlots > 1) && (m_nPreviousOccupiedSlots > 1) && nMaxPlayers > 1)
        JoinPreviousSession(JoinFailureAction::JFA_HostPrivate, false);
    else
	{
		if(bIsPrivate) nHostFlags |= HostFlags::HF_AllowPrivate;
		HostSession(nGameMode, nMaxPlayers, nHostFlags);
	}

	// check if this is job to job
	if(CheckFlag(nHostFlags, HostFlags::HF_JobToJob))
	{
		gnetDebug1("\tDoTransitionToNewGame :: Job to Job transition");
		m_bFreeroamIsJobToJob = true;
	}

    // send message to clients
    if(m_nGamersToGame > 0)
    {
        MsgTransitionToGameStart msg;
		msg.Reset(NetworkInterface::GetLocalGamerHandle(), nGameMode, CheckFlag(nHostFlags, HostFlags::HF_ViaTransitionLobby));

        for(unsigned i = 0; i < m_nGamersToGame; i++)
        {	
            // grab gamer info for peer ID to message
            rlGamerInfo hInfo;
            m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
            if(!hInfo.IsLocal())
                m_pTransition->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
        }
    }
    else
    {
        gnetDebug1("\tDoTransitionToNewGame :: Not with players, leaving activity session!");

		DURANGO_ONLY(RemoveFromParty(true));

		// need to reset this flag here to leave
        m_bIsTransitionToGame = false; 
        LeaveTransition();
        m_bIsTransitionToGame = true; 
    }

    return true; 
}

void CNetworkSession::OnSessionEntered(const bool bAsHost)
{
	// track solo sessions - if we are host in a single player session
	if((m_pSession->IsHost() || bAsHost) && (m_pSession->GetGamerCount() <= 1) && !m_pSession->HasJoinersPending())
	{
		eSoloSessionReason nReason = SOLO_SESSION_HOSTED_FROM_UNKNOWN;
		int nUserParam1 = 0, nUserParam2 = 0, nUserParam3 = 0;

		// determine why we are in a session on our own
		if(m_bIsTransitionToGame && (m_nGamersToGame > 0))
		{
			nReason = SOLO_SESSION_HOSTED_TRANSITION_WITH_GAMERS;
			nUserParam1 = m_nGamersToGame;
		}
		else if(m_nFollowers > 0)
		{
			nReason = SOLO_SESSION_HOSTED_TRANSITION_WITH_FOLLOWERS;
			nUserParam1 = m_nFollowers;
		}
		else if(CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaScript))
			nReason = SOLO_SESSION_HOSTED_FROM_SCRIPT;
		else if(CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaScriptToGame))
			nReason = SOLO_SESSION_TO_GAME_FROM_SCRIPT;
		else if(CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaMatchmaking))
		{
			nReason = SOLO_SESSION_HOSTED_FROM_MM;

			// grab some matchmaking stats
			nUserParam1 = static_cast<int>(sysTimer::GetSystemMsTime() - m_nQuickMatchStarted);
			nUserParam2 = m_GameMMResults.m_nNumCandidatesAttempted;
			nUserParam3 = m_GameMMResults.m_nNumCandidates;
		}
		else if(CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaJoinFailure))
			nReason = SOLO_SESSION_HOSTED_FROM_JOIN_FAILURE;
		else if(CheckFlag(m_nHostFlags[SessionType::ST_Physical], HostFlags::HF_ViaQuickmatchNoQueries))
			nReason = SOLO_SESSION_HOSTED_FROM_QM;

		// track any unknowns 
		gnetAssertf(nReason != SOLO_SESSION_HOSTED_FROM_UNKNOWN, "OnSessionEntered :: Host reason not tracked!");

		// track this in logging
		gnetDebug1("OnSessionEntered :: Sending solo telemetry - Host: %s, AsHost: %s, Reason: %s, Visibility: %s, User1: %d, User2: %d, User3: %d", 
				   m_pSession->IsHost() ? "True" : "False",
				   bAsHost ? "True" : "False",
				   GetSoloReasonAsString(nReason), 
				   GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Physical)), 
				   nUserParam1,
				   nUserParam2,
				   nUserParam3);

		// fire telemetry
		CNetworkTelemetry::EnteredSoloSession(m_pSession->GetSessionId(), 
											  m_pSession->GetSessionToken().GetValue(), 
											  m_Config[SessionType::ST_Physical].GetSessionPurpose(), 
											  nReason, 
											  GetSessionVisibility(SessionType::ST_Physical),
											  bAsHost ? SOLO_SOURCE_HOST : SOLO_SOURCE_JOIN,
											  nUserParam1,
											  nUserParam2,
											  nUserParam3);

		// increase solo count and set solo reason
		m_MatchmakingInfoMine.m_nSoloReason = static_cast<u8>(nReason); 
		m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesSolo);
	}

	if(m_bIsTransitionToGame)
	{
		gnetAssertf(m_nToGameTrackingTimestamp > 0, "OnSessionEntered :: Invalid tracking time!");
		gnetDebug1("OnSessionEntered :: Via Transition - Time taken: %u", sysTimer::GetSystemMsTime() - m_nToGameTrackingTimestamp);

		// instantly drop and bring back the network
	    if(CNetwork::IsNetworkOpen())
	    {
		    CloseNetwork(false);
		    OpenNetwork();
	    }
    	
	    // send message to clients.
        if(m_nGamersToGame > 0)
        {
			MsgTransitionToGameNotify msg;
			msg.Reset(NetworkInterface::GetLocalGamerHandle(),
					   m_pSession->GetSessionInfo(), 
					   m_bIsTransitionToGameNotifiedSwap ? MsgTransitionToGameNotify::NOTIFY_UPDATE : MsgTransitionToGameNotify::NOTIFY_SWAP,
					   m_pSession->GetMaxSlots(RL_SLOT_PUBLIC) == 0);
            
            rlGamerInfo hInfo;
            for(unsigned i = 0; i < m_nGamersToGame; i++)
            {	
                // grab gamer info for peer ID to message - they might have left since, so check
                bool bGamerFound = m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
                if(bGamerFound && !hInfo.IsLocal())
                    m_pTransition->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
            }
        }

	    // flag that we've made it to a session
		m_bIsTransitionToGameNotifiedSwap = true;
	    m_bIsTransitionToGameInSession = true;

	    // generate script event
	    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_TO_GAME_FINISH));
    }

    // send message to followers
    if(m_nFollowers > 0)
    {
        gnetDebug1("OnSessionEntered :: Inviting %d followers", m_nFollowers);
        CFollowInvite::Trigger(m_hFollowers, m_nFollowers, m_pSession->GetSessionInfo());
        if(!m_bRetainFollowers)
            ClearFollowers();
    }
}

void CNetworkSession::UpdateSessionTypes()
{
#if !__NO_OUTPUT
    static const char* s_SessionTypeForPresenceNames[SessionTypeForPresence::STP_Num] =
    {
		"STP_InviteOnly",		// STP_InviteOnly
		"STP_FriendsOnly",		// STP_FriendsOnly
		"STP_CrewOnly",			// STP_CrewOnly
		"STP_CrewSession",		// STP_CrewSession
		"STP_Solo",				// STP_Solo
		"STP_Public",			// STP_Public
    };

	static const char* s_SessionTypeForTelemetryNames[SessionTypeForTelemetry::STT_Num] =
	{
		"STT_Solo",				// STT_Solo
		"STT_Activity",			// STT_Activity
		"STT_Freeroam",			// STT_Freeroam
		"STT_ClosedCrew",		// STT_ClosedCrew
		"STT_Private",			// STT_Private
		"STT_ClosedFriends",	// STT_ClosedFriends
		"STT_Crew",				// STT_Crew
	};
#endif

    // assume public for presence
    SessionTypeForPresence sessionTypeForPresence = SessionTypeForPresence::STP_Public;

	// assume freeroam for telemetry
	m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Freeroam;

	// work these out
	if(IsSoloSession(SessionType::ST_Physical))
	{
		sessionTypeForPresence = SessionTypeForPresence::STP_Solo;
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Solo;
	}
	// IsSoloSession is checked above so the latter check is just for clarity since these
	// are reported as seperate concepts (and solo sessions are also private sessions)
	else if(IsPrivateSession(SessionType::ST_Physical) && !IsSoloSession(SessionType::ST_Physical))
	{
		sessionTypeForPresence = SessionTypeForPresence::STP_InviteOnly;
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Private;
	}
	else if(IsActivitySession())
	{
		// for presence, this doesn't matter but for telemetry - maintain the historical 
		// way that we did this for data integrity
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Activity;
	}
	else if(IsClosedFriendsSession(SessionType::ST_Physical))
	{
		sessionTypeForPresence = SessionTypeForPresence::STP_FriendsOnly;
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_ClosedFriends;
	}
	else if(m_UniqueCrewData[SessionType::ST_Physical].m_nLimit > 0 && !IsClosedCrewSession(SessionType::ST_Physical))
	{
		sessionTypeForPresence = SessionTypeForPresence::STP_CrewSession;
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_Crew;
	}
	else if(IsClosedCrewSession(SessionType::ST_Physical))
	{
		sessionTypeForPresence = SessionTypeForPresence::STP_CrewOnly;
		m_SessionTypeForTelemetry = SessionTypeForTelemetry::STT_ClosedCrew;
	}

#if RSG_OUTPUT
	static SessionTypeForPresence s_LastPresenceType = SessionTypeForPresence::STP_Unknown;
	static SessionTypeForTelemetry s_LastTelemetryType = SessionTypeForTelemetry::STT_Invalid;

	if(s_LastPresenceType != sessionTypeForPresence)
	{
		gnetDebug1("UpdateSessionTypes :: SessionTypeForPresence: %s [%d]", s_SessionTypeForPresenceNames[sessionTypeForPresence], sessionTypeForPresence);
		s_LastPresenceType = sessionTypeForPresence;
	}

	if(s_LastTelemetryType != m_SessionTypeForTelemetry)
	{
		gnetDebug1("UpdateSessionTypes :: SessionTypeForTelemetry: %s [%d]", s_SessionTypeForTelemetryNames[m_SessionTypeForTelemetry], m_SessionTypeForTelemetry);
		s_LastTelemetryType = m_SessionTypeForTelemetry;
	}
#endif

	rlPresenceAttributeHelper::SetSessionType(NetworkInterface::GetLocalGamerIndex(), static_cast<int>(sessionTypeForPresence));
}

bool CNetworkSession::IsTransitioning()
{
	return m_bLaunchingTransition || m_bIsTransitionToGame;
}

void CNetworkSession::JoinOrHostTransition(unsigned sessionType)
{
#if !__FINAL
	if(PARAM_netSessionBailFinding.Get())
	{
		gnetWarning("JoinOrHostTransition :: Simulating eBAIL_SESSION_FIND_FAILED Bail!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_FIND_FAILED, BAIL_CTX_FIND_FAILED_CMD_LINE));
		return;
	}
#endif 

	// Clear out the list of reported tamper event DWORDS 
	// This handles the safe scenario of moving from SP=>MP
	rageSecReportCache::Clear();

	// grab these
	const rlNetworkMode netMode = NetworkInterface::GetNetworkMode();

	// check whether this session should be considered
	bool bConsiderSession = sessionType < m_TransitionMMResults.m_nNumCandidates;

	// attempt join
	if(bConsiderSession && snSession::CanJoin(netMode) && !PARAM_netSessionForceHost.Get())
	{
		m_bJoiningTransitionViaMatchmaking = true;
		m_bTransitionViaMainMatchmaking = false;

		// generate script event
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_QUICKMATCH_JOINING));

        // is this a group join
        unsigned nGroupSize = 0;
        rlGamerHandle aGroup[MAX_NUM_PHYSICAL_PLAYERS];
        if(m_nGamersToActivity > 0)
        {
            gnetDebug2("JoinOrHostTransition :: Have %d group members", m_nGamersToActivity);

            const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
            if(pInfo)
            {
                // copy in gamers
                rlGamerInfo hGamerInfo;
                for(unsigned i = 0; i < m_nGamersToActivity; i++)
                {
                    // account for the targeted gamer having left the session
                    bool bGamerFound = m_pSession->GetGamerInfo(m_hGamersToActivity[i], &hGamerInfo);
                    if(bGamerFound && nGroupSize < MAX_NUM_PHYSICAL_PLAYERS)
                    {
                        gnetDebug2("JoinOrHostTransition :: Adding %s to group", hGamerInfo.GetName());
                        aGroup[nGroupSize++] = hGamerInfo.GetGamerHandle();
                    }
                }
            }
        }

		// grab matchmaking session result, log session information
		const rlSessionDetail& mmDetail = m_TransitionMMResults.m_CandidatesToJoin[sessionType].m_Session;
		gnetDebug1("JoinOrHostTransition :: Session %d, Hosted by %s, Players %d", sessionType + 1, mmDetail.m_HostName, mmDetail.m_NumFilledPublicSlots + mmDetail.m_NumFilledPrivateSlots);

		// increase attempted count
		m_TransitionMMResults.m_nNumCandidatesAttempted++;

		// record time that we started this process
		m_TransitionMMResults.m_CandidatesToJoin[sessionType].m_nTimeStarted = sysTimer::GetSystemMsTime();

		// build join flags
		unsigned nJoinFlags = JoinFlags::JF_Default;
		if(CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_Asynchronous))
			nJoinFlags |= JoinFlags::JF_Asynchronous;

		// call into JoinTransition
		if(!JoinTransition(mmDetail.m_SessionInfo, RL_SLOT_PUBLIC, nJoinFlags, aGroup, nGroupSize))
        {
            gnetWarning("JoinOrHostTransition :: Bailing, JoinTransition failed!");
            BailTransition(BAIL_TRANSITION_JOIN_FAILED);
		}
	}
	else 
	{
		// reset these
		m_bJoiningTransitionViaMatchmaking = false;

		// if this isn't a quickmatch, fall out
		if(!CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_Quickmatch))
		{
			gnetDebug1("JoinOrHostTransition :: Matchmaking failed to find a viable session!");
			GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_QUICKMATCH_FAILED));

            // inform any remote gamers (this will let remote players know we failed)
            CompleteTransitionQuickmatch();

            // script can optionally keep the group for any host request coming from this
            if(!m_bRetainActivityGroupOnQuickmatchFail)
                m_nGamersToActivity = 0;
            else if(m_nGamersToActivity > 0)
            {
                gnetDebug1("JoinOrHostTransition :: Retaining group of %d gamers!", m_nGamersToActivity);
                m_bRetainActivityGroupOnQuickmatchFail = false;
            }
		}
		// host check for normal sessions
		else if(snSession::CanHost(netMode))
		{
			// check what was applied for the search - work upwards through the queries to get
			// the narrowest definition of what the player intended
			NetworkGameFilter* pFilter = NULL; 
			for(int i = 0; i < ActivityMatchmakingQuery::AMQ_Num; i++)
			{
				if(m_ActivityMMQ[i].m_bIsActive)
				{
					pFilter = m_ActivityMMQ[i].m_pFilter;
					break;
				}
			}

			// set some defaults
			int nActivityType = -1; 
			unsigned uActivityType = 0;
			unsigned nActivityID = 0;
			unsigned nActivityIsland = ACTIVITY_ISLAND_GENERAL;
			unsigned nContentFilter = ACTIVITY_CONTENT_ROCKSTAR_CREATED;
			u16 nGameMode = MultiplayerGameMode::GameMode_Freeroam;

			// this should possibly be an assert / verify
			if(pFilter)
			{
				if(pFilter->GetActivityType(uActivityType))
					nActivityType = static_cast<int>(uActivityType);
				pFilter->GetActivityID(nActivityID);
				pFilter->GetActivityIsland(nActivityIsland);
				pFilter->GetContentFilter(nContentFilter);
                nGameMode = pFilter->GetGameMode();
            }

			// generate script event
			GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_QUICKMATCH_HOSTING));

			// acknowledge in the matchmaking results
			m_TransitionMMResults.m_bHosted = true;

			// host match
			m_nHostRetriesTransition = 0;
			if(!HostTransition(static_cast<int>(nGameMode), m_nTransitionQuickmatchFallbackMaxPlayers, nActivityType, nActivityID, nActivityIsland, nContentFilter, HostFlags::HF_Default))
            {
                gnetWarning("JoinOrHostTransition :: Bailing, HostTransition failed!");
                BailTransition(BAIL_TRANSITION_HOST_FAILED);
			}
		}
		else
		{
			gnetAssertf(0, "JoinOrHostTransition :: Bailing, Cannot join or host!");
            BailTransition(BAIL_TRANSITION_CANNOT_HOST_OR_JOIN);
		}
	}
}

void CNetworkSession::SetTransitionState(eTransitionState nState, bool bSkipChangeLogic)
{
    if(nState != m_TransitionState)
    {
        gnetDebug1("SetTransitionState :: Next: %s, Prev: %s, TimeInState: %u, SkipChangeLogic: %s", GetTransitionStateAsString(nState), GetTransitionStateAsString(), (m_TransitionStateTime > 0) ? (sysTimer::GetSystemMsTime() | 0x01) - m_TransitionStateTime : 0, bSkipChangeLogic ? "True" : "False");

		if(!bSkipChangeLogic)
		{
			if(nState == TS_ESTABLISHED)
			{
				if (IsTransitionHost())
				{
					m_TimeCreated[SessionType::ST_Transition] = rlGetPosixTime();
				}

				// reset flags so that they don't carry through sessions.
				m_nTransitionMatchmakingFlags = 0;

				// send results telemetry
				bool bSendMatchmakingTelemetry = ((IsTransitionHost() && CheckFlag(m_nHostFlags[SessionType::ST_Transition], HostFlags::HF_ViaMatchmaking)) || m_bJoiningTransitionViaMatchmaking);
				if(bSendMatchmakingTelemetry)
					SendMatchmakingResultsTelemetry(&m_TransitionMMResults);

#if RSG_DURANGO
				if(g_rlXbl.GetPartyManager()->IsInParty())
				{
					// ask user if specified
					if(!g_rlXbl.GetPartyManager()->IsInSoloParty() && PARAM_netSessionCheckPartyForActivity.Get())
					{
						gnetDebug1("SetTransitionState :: kTransitionState_Started - Checking party with user");
						m_bHandlePartyActivityDialog = true; 
						m_bShowingPartyActivityDialog = false;
						HandlePartyActivityDialog();
					}
					// otherwise, register the session
					else if(m_pTransition->Exists() && !IsTransitionLeaving() && !IsInTransitionToGame())
					{
						gnetDebug1("SetTransitionState :: kTransitionState_Started - Registering Transition");
						RegisterPlatformPartySession(m_pTransition);
					}
				}
				else
				{
					// add to party if we aren't in one
					gnetDebug1("SetTransitionState :: kTransitionState_Started - Starting party");
					g_rlXbl.GetPartyManager()->AddLocalUsersToParty(NetworkInterface::GetLocalGamerIndex(), NULL);
				}
#endif

				// mark the local gamer as established and let everyone know
				m_pTransition->MarkAsEstablished(NetworkInterface::GetLocalGamerHandle());
				MsgSessionEstablished msgEstablished(m_pTransition->GetSessionId(), NetworkInterface::GetLocalGamerHandle());
				m_pTransition->BroadcastMsg(msgEstablished, NET_SEND_RELIABLE);
			}
		}

        // mark when we entered this state
        m_TransitionState = nState;
        m_TransitionStateTime = sysTimer::GetSystemMsTime() | 0x01;
    }
}

bool CNetworkSession::HostTransition(const int nGameMode, unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, const int nActivityIsland, const unsigned nContentCreator, unsigned nHostFlags)
{
	// log out
	gnetDebug1("HostTransition :: GameMode: %s [%d], ActivityType: %d, ActivityID: 0x%08x, ActivityIsland: %d, ContentCreator: %u, Flags: 0x%x, MaxPlayers: %u", NetworkUtils::GetMultiplayerGameModeIntAsString(nGameMode), nGameMode, nActivityType, nActivityID, nActivityIsland, nContentCreator, nHostFlags, nMaxPlayers);

	// verify max players
	if(!gnetVerify(nMaxPlayers > 0))
	{
		gnetError("HostTransition :: Invalid Max Players: %d. Must be at least 1!", nMaxPlayers);
		return false;
	}

	// verify max players
	if(!gnetVerify(nMaxPlayers <= RL_MAX_GAMERS_PER_SESSION))
	{
		gnetError("HostTransition :: Invalid Max Players: %d. Cannot be more than %d!", nMaxPlayers, RL_MAX_GAMERS_PER_SESSION);
		nMaxPlayers = RL_MAX_GAMERS_PER_SESSION;
	}

	// verify spectator maximum has been set
	gnetAssertf(m_bHasSetActivitySpectatorsMax, "HostTransition :: Activity Spectator maximum not set!");

	// check for command line override
	unsigned nMaxSessionPlayers = nMaxPlayers;
	NOTFINAL_ONLY(PARAM_netSessionMaxPlayersActivity.Get(nMaxSessionPlayers));

	// this will be a distinct parameter eventually
	m_nActivityPlayersMax = nMaxSessionPlayers - m_nActivitySpectatorsMax;

	// add single player host flag if max players is 1
	if(nMaxPlayers == 1)
		SetFlags(nHostFlags, HostFlags::HF_SingleplayerOnly);

	// add an additional slot if we allow admin players to join solo sessions
	if(m_bAllowAdminInvitesToSolo && CheckFlag(nHostFlags, HostFlags::HF_SingleplayerOnly))
	{
		gnetDebug1("HostTransition :: Incrementing players for ScAdmin slot");
		nMaxPlayers += 1;
	}

	// setup host config
	NetworkGameConfig ngConfig;
	player_schema::MatchingAttributes mmAttrs;
	ngConfig.Reset(mmAttrs, static_cast<u16>(nGameMode), SessionPurpose::SP_Activity, nMaxSessionPlayers, CheckFlag(nHostFlags, HostFlags::HF_IsPrivate) ? nMaxSessionPlayers : 0);
	ngConfig.SetActivityType(nActivityType);
	ngConfig.SetActivityID(nActivityID);
	ngConfig.SetActivityIsland(nActivityIsland);
	ngConfig.SetContentCreator(nContentCreator);
	ngConfig.SetVisible(!CheckFlag(nHostFlags, HostFlags::HF_NoMatchmaking));
	
    // initialise available slots - max players minus spectator slots minus local slot
    ngConfig.SetActivityPlayersSlotsAvailable(m_nActivityPlayersMax - 1);

	// until we start calling the function directly
	if(m_UniqueCrewData[SessionType::ST_Transition].m_nLimit > 0)
		SetFlags(nHostFlags, HostFlags::HF_ClosedCrew);

	// call up to session
	m_nHostRetriesTransition = 0;
	return HostTransition(ngConfig, nHostFlags);
}

bool CNetworkSession::HostTransition(const NetworkGameConfig& tConfig, unsigned nHostFlags)
{
	// logging
    gnetDebug1("HostTransition :: Channel: %s, Flags: 0x%08x", NetworkUtils::GetChannelAsString(m_pTransition->GetChannelId()), nHostFlags);

	// verify that we've initialised the session
	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("HostTransition :: Not initialised!");
		return false;
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("HostTransition"))
        return false;

    // verify that we have multiplayer access
    if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
    {
        gnetError("HostTransition :: Cannot access multiplayer!");
        return false; 
    }

	// verify state
	if(!gnetVerify(m_TransitionState == TS_IDLE))
	{
		gnetError("HostTransition :: Invalid State");
		return false;
	}

    // verify that we are not in a launched transition session already
    if(!gnetVerify(!IsActivitySession()))
    {
        gnetError("HostTransition :: In a launched activity session!");
        return false;
    }

	// verify that we can host
	if(!gnetVerify(snSession::CanHost(NetworkInterface::GetNetworkMode())))
	{
		gnetError("HostTransition :: Cannot Host");
		return false;
	}

	// verify that we've initialised the session
	if(!gnetVerify(!IsTransitionStatusPending()))
	{
		gnetError("HostTransition :: Busy! Idle - Dropping!");
		DropTransition();
		return false;
	}

#if !__NO_OUTPUT
	tConfig.DebugPrint(true);
#endif

	// verify that we've initialised the session
	if(!gnetVerify(tConfig.IsValid()))
	{
		gnetError("HostTransition :: Invalid Config!");
		return false;
	}

	// reset tracking flags
	m_bJoinedTransitionViaInvite = false;
	m_bJoiningTransitionViaMatchmaking = false;
	m_bBlockTransitionJoinRequests = false;
	m_nTransitionInviteFlags = 0;
	m_nHostFlags[SessionType::ST_Transition] = HostFlags::HF_Default;

	// clear out transition player tracker
	m_TransitionPlayers.Reset();

    // reset locked visibility
    m_bLockVisibility[SessionType::ST_Transition] = false;
    m_bLockSetting[SessionType::ST_Transition] = false;

	// reset player data
	u8 aData[1];
	u32 nSizeOfData = 1;

	unsigned nCreateFlags = 0;
#if RSG_PRESENCE_SESSION
	nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#endif

#if RSG_DURANGO
	nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif

	// make sure that we disable these if we're creating a solo session with extra slots
	if(m_bAllowAdminInvitesToSolo && CheckFlag(nHostFlags, HostFlags::HF_SingleplayerOnly))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#if RSG_DURANGO
		nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif
	}

	// disable the join via presence if requested
	if (CheckFlag(nHostFlags, HostFlags::HF_JoinViaPresenceDisabled))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_JOIN_VIA_PRESENCE_DISABLED;
	}

	// disable the join in progress if requested
	if (CheckFlag(nHostFlags, HostFlags::HF_JoinInProgressDisabled))
	{
		nCreateFlags |= RL_SESSION_CREATE_FLAG_JOIN_IN_PROGRESS_DISABLED;
	}

    // reset retry policy
    SetSessionPolicies(POLICY_NORMAL);

	// set flag indicating that we're waiting asynchronously
	m_bIsTransitionAsyncWait = CheckFlag(nHostFlags, HostFlags::HF_Asynchronous);

    // update gamer data
    UpdateTransitionGamerData();

	// host session
	m_pTransitionStatus->Reset();
	bool bSuccess = m_pTransition->Host(NetworkInterface::GetLocalGamerIndex(), 
										RL_NETMODE_ONLINE, 
										tConfig.GetMaxPublicSlots(), 
										tConfig.GetMaxPrivateSlots(), 
										tConfig.GetMatchingAttributes(), 
										nCreateFlags, 
										aData, 
										nSizeOfData, 
										m_pTransitionStatus);

	if(!gnetVerify(bSuccess))
	{
		gnetError("HostTransition :: Failed to host!");
		return false;
	}

	// get current channel policies, and set that we always maintain a keep alive.
	// we don't want to rely on the game channel here as the 
	netChannelPolicies tPolicies; 
	CNetwork::GetConnectionManager().GetChannelPolicies(m_pTransition->GetChannelId(), &tPolicies);
	tPolicies.m_AlwaysMaintainKeepAlive = true; 
	CNetwork::GetConnectionManager().SetChannelPolicies(m_pTransition->GetChannelId(), tPolicies);

	// stash config and lock matchmaking pool
	m_Config[SessionType::ST_Transition] = tConfig;
	m_nSessionPool[SessionType::ST_Transition] = NetworkBaseConfig::GetMatchmakingPool();
	m_nHostFlags[SessionType::ST_Transition] = nHostFlags;
    OUTPUT_ONLY(LogHostFlags(m_nHostFlags[SessionType::ST_Transition], __FUNCTION__));

	// lock visibility when we specify no matchmaking
	if(CheckFlag(nHostFlags, HostFlags::HF_NoMatchmaking))
		SetSessionVisibilityLock(SessionType::ST_Transition, true, false);

	// mark transition hosted setting correctly
	if(CheckFlag(nHostFlags, HostFlags::HF_ViaMatchmaking))
		m_TransitionMMResults.m_bHosted = true;

	SetTransitionState(TS_HOSTING);

#if !__NO_OUTPUT
	m_nCreateTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

	// clear out parameters
	ClearTransitionParameters();

	// let other players know we're starting a transition
	if(CNetwork::IsNetworkOpen())
	{
		NetworkInterface::GetLocalPlayer()->SetStartedTransition(true);
		NetworkInterface::GetLocalPlayer()->InvalidateTransitionSessionInfo();
	}

	CContextMenuHelper::InitVotes();

	// generate event
	GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_HOSTING)); 

	return bSuccess;
}

bool CNetworkSession::JoinTransition(const rlSessionInfo& tSessionInfo, const rlSlotType kSlotType, unsigned nJoinFlags, const rlGamerHandle* pHandles, const unsigned nGamers)
{
    gnetDebug1("JoinTransition :: Token: 0x%016" I64FMT "x, Channel: %s, Flags: 0x%x", tSessionInfo.GetToken().m_Value, NetworkUtils::GetChannelAsString(m_pTransition->GetChannelId()), nJoinFlags);

	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("JoinTransition - Session not initialised!");
		return false;
	}

	if(!gnetVerify(tSessionInfo.IsValid()))
	{
		gnetError("JoinTransition - Invalid Session Info!");
		return false;
	}

	if(!gnetVerify(!IsTransitionActive()))
	{
		gnetError("JoinTransition - Already in a transition!");
		return false;
	}

    // verify that we are not in a launched transition session already
    if(!gnetVerify(!IsActivitySession()))
    {
        gnetError("JoinTransition :: In a launched activity session!");
        return false;
    }

	if(!gnetVerify(!IsTransitionStatusPending()))
	{
		if(m_TransitionState == TS_IDLE)
		{
			gnetError("JoinTransition - Busy! Idle - Dropping!");
			DropTransition();
		}
		else
		{
			gnetError("JoinTransition - Busy!");
			return false;
		}
	}

    // check we have access via profile and privilege
    if(!VerifyMultiplayerAccess("JoinTransition"))
        return false;

    // verify that we have multiplayer access
    if(!gnetVerify(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer)))
    {
        gnetError("JoinTransition :: Cannot access multiplayer!");
        return false; 
    }

	// reset transition state
	SetTransitionState(TS_IDLE);

    // reset joined from launch flag
    m_bLaunchOnStartedFromJoin = false;

	// clear out transition player tracker
	m_TransitionPlayers.Reset();

	// reset maximum limits - these will be provided when we join
	m_nActivityPlayersMax = 0;
	m_nActivitySpectatorsMax = 0;

    // reset locked visibility
    m_bLockVisibility[SessionType::ST_Transition] = false;
    m_bLockSetting[SessionType::ST_Transition] = true;

    // via invite
    const bool bViaInvite = (nJoinFlags & JoinFlags::JF_ViaInvite) != 0;

	// if not via invite, this is through matchmaking
	m_bJoiningTransitionViaMatchmaking = !bViaInvite; 
	if(bViaInvite)
		SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);

	// initialise voice (internally handles already being initialised)
	CNetwork::InitVoice();

	CContextMenuHelper::InitVotes();

	// set flag indicating that we're waiting asynchronously
	m_bIsTransitionAsyncWait = CheckFlag(nJoinFlags, JoinFlags::JF_Asynchronous);

    // update gamer data
    UpdateTransitionGamerData();

	// we can also consume private if this is a non-invite join with private slots
	if(!bViaInvite && kSlotType == RL_SLOT_PRIVATE)
		nJoinFlags |= JoinFlags::JF_ConsumePrivate;

	// build join request
	u8 aData[256]; unsigned nSizeOfData;
	bool bSuccess = BuildJoinRequest(aData, 256, nSizeOfData, nJoinFlags);

	// verify that we've initialised the session
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinTransition :: Failed to build Join Request!");
		CNetwork::Bail(sBailParameters(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST_ACTIVITY));
		return false;
	}

    // setup presence flag
    unsigned nCreateFlags = 0;
#if RSG_PRESENCE_SESSION
    nCreateFlags |= RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;
#endif

#if RSG_DURANGO
	nCreateFlags |= RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED;
#endif

	// call into session
	bSuccess = m_pTransition->Join(NetworkInterface::GetLocalGamerIndex(),
								   tSessionInfo, 
								   NetworkInterface::GetNetworkMode(),
								   kSlotType,
								   nCreateFlags,
								   aData, 
								   nSizeOfData, 
								   pHandles,
								   nGamers,
								   m_pTransitionStatus);

    // always clear this
    m_ActivityGroupSession.Clear();
	m_bIsActivityGroupQuickmatchAsync = false;

    // check success
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinTransition - Error joining transition session!");
		return false;
	}

	// get current channel policies, and set that we always maintain a keep alive.
	// we don't want to rely on the game channel here as the 
	netChannelPolicies tPolicies; 
	CNetwork::GetConnectionManager().GetChannelPolicies(m_pTransition->GetChannelId(), &tPolicies);
	tPolicies.m_AlwaysMaintainKeepAlive = true; 
	CNetwork::GetConnectionManager().SetChannelPolicies(m_pTransition->GetChannelId(), tPolicies);

	// clear out parameters
	ClearTransitionParameters();

	// let other players know we're starting a transition
	if(CNetwork::IsNetworkOpen())
	{
		NetworkInterface::GetLocalPlayer()->SetStartedTransition(true);
		NetworkInterface::GetLocalPlayer()->InvalidateTransitionSessionInfo();
	}

	// generate script event
	GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_JOINING));

#if !__NO_OUTPUT
	m_nCreateTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

	// lock matchmaking pool
	m_nSessionPool[SessionType::ST_Transition] = NetworkBaseConfig::GetMatchmakingPool();

	// stash session parameters
	m_TransitionJoinSessionInfo = tSessionInfo;
	m_TransitionJoinSlotType = kSlotType;
	m_nTransitionJoinFlags = nJoinFlags;

    OUTPUT_ONLY(LogJoinFlags(m_nTransitionJoinFlags, __FUNCTION__));

	// all checks passed - update state
	SetTransitionState(TS_JOINING);
	return true;
}

bool CNetworkSession::JoinPreviouslyFailedTransition()
{
	gnetDebug1("JoinPreviouslyFailedTransition :: Available: %s, Token: 0x%016" I64FMT "x", m_bCanRetryTransitionJoinFailure ? "T" : "F", m_TransitionJoinSessionInfo.GetToken().m_Value);

	// check that our session info is valid
	if(!gnetVerify(m_bCanRetryTransitionJoinFailure))
	{
		gnetError("JoinPreviouslyFailedTransition :: Not a valid action. Only available for direct joins.");
		return false;
	}

	// check that our session info is valid
	if(!gnetVerify(m_TransitionJoinSessionInfo.IsValid()))
	{
		gnetError("JoinPreviouslyFailedTransition :: Session not valid!");
		return false;
	}

	// use stashed information
	return JoinTransition(m_TransitionJoinSessionInfo, m_TransitionJoinSlotType, m_nTransitionJoinFlags);
}

bool CNetworkSession::JoinPlayerTransition(CNetGamePlayer* pPlayer)
{
	gnetDebug1("JoinPlayerTransition :: Joining %s", pPlayer->GetLogName());

	if(!gnetVerify(pPlayer->HasStartedTransition()))
	{
		gnetError("JoinPlayerTransition :: Not started transition!");
		return false;
	}

	const rlSessionInfo& hInfo = pPlayer->GetTransitionSessionInfo();
	if(!gnetVerify(hInfo.IsValid()))
	{
		gnetError("JoinPlayerTransition :: Session details are invalid!");
		return false;
	}

    // longer retry policy
    SetSessionPolicies(POLICY_NORMAL);

	// join up
	return JoinTransition(hInfo, RL_SLOT_PRIVATE, JoinFlags::JF_ViaInvite | JoinFlags::JF_ConsumePrivate);
}

void CNetworkSession::SetTransitionActivityType(int nActivityType)
{
	// don't bother if visibility already matches request
	if(static_cast<int>(m_Config[SessionType::ST_Transition].GetActivityType()) == nActivityType)
		return;

    // need to update the advertised attributes - only the host can do this
    if(m_pTransition->IsHost())
    {
	    gnetDebug3("SetTransitionActivityType :: AttributeChange - ActivityType: %d", nActivityType);

	    m_Config[SessionType::ST_Transition].SetActivityType(nActivityType);
		m_bChangeAttributesPending[SessionType::ST_Transition] = true;
    }
}

void CNetworkSession::SetTransitionActivityID(unsigned nActivityID)
{
	// don't bother if visibility already matches request
	if(m_Config[SessionType::ST_Transition].GetActivityID() == nActivityID)
		return;

    // need to update the advertised attributes - only the host can do this
    if(m_pTransition->IsHost())
    {
	    gnetDebug3("SetTransitionActivityID :: AttributeChange - ActivityID: %08x", nActivityID);

	    m_Config[SessionType::ST_Transition].SetActivityID(nActivityID);
		m_bChangeAttributesPending[SessionType::ST_Transition] = true;
    }
}

void CNetworkSession::ChangeTransitionSlots(const unsigned nPublic, const unsigned nPrivate)
{
	if((nPublic == m_pTransition->GetMaxSlots(RL_SLOT_PUBLIC)) && (nPrivate == m_pTransition->GetMaxSlots(RL_SLOT_PRIVATE)))
	{
		gnetDebug1("ChangeTransitionSlots :: Already have these slots.");
		return;
	}

    // need to update the advertised attributes - only the host can do this
    if(m_pTransition->IsHost())
    {
        gnetDebug3("ChangeTransitionSlots :: AttributeChange - Public: %d, Private: %d", nPublic, nPrivate);

	    // apply the current config - chiefly, we do this to allocate the schema index values 
	    player_schema::MatchingAttributes mmAttrs;
	    m_Config[SessionType::ST_Transition].Reset(mmAttrs, m_pTransition->GetConfig().m_Attrs.GetGameMode(), m_pTransition->GetConfig().m_Attrs.GetSessionPurpose(), nPublic + nPrivate, nPrivate);
	    m_Config[SessionType::ST_Transition].Apply(m_pTransition->GetConfig());

		m_bChangeAttributesPending[SessionType::ST_Transition] = true;
    }
}

void CNetworkSession::SetBlockTransitionJoinRequests(bool bBlockJoinRequests)
{
	// can only call this as session host
	if(!gnetVerify(IsTransitionHost()))
	{
		gnetError("SetBlockTransitionJoinRequests :: Calling as non-host!");
		return;
	}

    if(m_bBlockTransitionJoinRequests != bBlockJoinRequests)
    {
        gnetDebug2("SetBlockTransitionJoinRequests :: %s join requests.", bBlockJoinRequests ? "Blocking" : "Not Blocking");
        m_bBlockTransitionJoinRequests = bBlockJoinRequests;
    }
}

bool CNetworkSession::LeaveTransition()
{
	// check we're not already processing an operation using the status
	if(!gnetVerify(!IsTransitionStatusPending()))
	{
		gnetError("LeaveTransition :: Busy!");
		return false;
	}

	// check we're in a valid state to leave
	if(!gnetVerify(IsTransitionEstablished()))
	{
		gnetError("LeaveTransition :: Not started!");
		return false;
	}

	// check we're not launching a transition
	if(!gnetVerify(!IsLaunchingTransition()))
	{
		gnetError("LeaveTransition :: In the middle of launching!");
		return false;
	}

	// check we're not transitioning to game and not yet in session
	if(!gnetVerify(!(m_bIsTransitionToGame && !m_bIsTransitionToGameInSession)))
	{
		gnetError("LeaveTransition :: In the middle of transitioning to game!");
		return false;
	}

	// check we're not transitioning to game and have pending players
	if(!gnetVerify(!(m_bIsTransitionToGame && m_nGamersToGame > 0)))
	{
		gnetError("LeaveTransition :: In the middle of transitioning with gamers!");
		return false;
	}

    if(!gnetVerify(!m_bIsTransitionToGamePendingLeave))
    {
        gnetError("LeaveTransition :: Postponed leave to wait for transitioning players!");
        return false;
    }

	gnetDebug1("LeaveTransition :: Leave transition session");

#if RSG_DURANGO
	// leave party if transitioning solo and in a party of more than 1
	if(g_rlXbl.GetPartyManager()->IsInParty())
	{
		if(IsSessionEstablished())
		{
			if(g_rlXbl.GetPartyManager()->IsInSoloParty())
			{
				// need to swap back to our game session
				gnetDebug1("LeaveTransition :: Making freeroam party game session!");
				RegisterPlatformPartySession(m_pSession);
			}
			else if(!IsInTransitionToGame())
			{
				bool bRemoveFromParty = g_rlXbl.GetPartyManager()->IsInParty() && !g_rlXbl.GetPartyManager()->IsInSoloParty();
				if(bRemoveFromParty && IsTransitionHost() && (m_pTransition->GetGamerCount() == 1) && IsSessionEstablished())
				{
					// find party members in freeroam
					int nPartySize = g_rlXbl.GetPartyManager()->GetPartySize();
					for(int i = 0; i < nPartySize; i++)
					{
						if(m_pSession->IsMember(g_rlXbl.GetPartyManager()->GetPartyMember(i)->m_GamerHandle))
						{
							// don't remove in this case as we've left a solo transition back to the session containing
							// previous party members (who haven't yet joined this transition)
							bRemoveFromParty = false; 
							break;
						}
					}
				}
				if(bRemoveFromParty)
					RemoveFromParty(true);
			}
		}
	}
#endif

    // if we're leaving a transition and going back to an active freemode - resync the radio
    if(!m_bIsTransitionToGame && IsSessionEstablished() && !IsHost())
    {
        u64 nHostPeerID = 0;
        bool bHaveHostPeer = m_pSession->GetHostPeerId(&nHostPeerID);
        if(bHaveHostPeer && NetworkInterface::GetActiveGamerInfo())
        {
            MsgRadioStationSyncRequest msg;
			msg.Reset(m_pSession->GetSessionId(), NetworkInterface::GetActiveGamerInfo()->GetGamerId());
            m_pSession->SendMsg(nHostPeerID, msg, NET_SEND_RELIABLE);
            BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
        }
    }

#if !__FINAL
    if(!PARAM_netSessionNoSessionBlacklisting.Get())
#endif
    {
        BlacklistedSession sSession;
        sSession.m_nSessionToken = m_pTransition->GetSessionInfo().GetToken();
        sSession.m_nTimestamp = sysTimer::GetSystemMsTime();
        m_BlacklistedSessions.PushAndGrow(sSession);

        gnetDebug1("LeaveTransition :: Blacklisting Session - ID: 0x%016" I64FMT "x. Time: %d", sSession.m_nSessionToken.m_Value, sSession.m_nTimestamp);
    }

	// check if we should cancel all tasks (wrap it all in tags to ignore migration failures)
	m_bIgnoreMigrationFailure[SessionType::ST_Transition] = true;
	if(m_bCancelAllTasksOnLeave)
	{
		m_pTransition->CancelAllSerialTasks();
		if(m_bCancelAllConcurrentTasksOnLeave)
			m_pTransition->CancelAllConcurrentTasks();
	}
	else
	{
		// cancel any migration tasks
		m_pTransition->CancelMigrationTasks();
	}
	m_bIgnoreMigrationFailure[SessionType::ST_Transition] = false;

	// we also need to correctly inform Xbox One services so that the events match up
	DURANGO_ONLY(WriteMultiplayerEndEventTransition(true OUTPUT_ONLY(, "LeaveTransition")));

    // call into session
	bool bSuccess = m_pTransition->Leave(NetworkInterface::GetLocalGamerIndex(), m_pTransitionStatus);
	if(!bSuccess)
	{
		m_pTransitionStatus->SetPending();
		m_pTransitionStatus->SetFailed();
	}
	// let others know we're no longer in a transition
	else
	{
		// generate script event
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_ENDING));

		if(CNetwork::IsNetworkOpen())
		{
			NetworkInterface::GetLocalPlayer()->SetStartedTransition(false);
			NetworkInterface::GetLocalPlayer()->InvalidateTransitionSessionInfo();
		}
	}

	SetTransitionState(TS_LEAVING);

	return bSuccess;
}

bool CNetworkSession::LaunchTransition()
{
	gnetDebug1("LaunchTransition :: Launching transition session");

	m_bLaunchingTransition = true; 
    m_bLaunchedAsHost = m_pTransition->IsHost();

	if(!SReportMenu::IsInstantiated())
		SReportMenu::Instantiate();

	SReportMenu::GetInstance().ResetAbuseList();

    m_TimeTokenForLaunchedTransition = netTimeSync::GenerateToken();

    gnetDebug1("LaunchTransition :: Generated time token for launched transition: %u", m_TimeTokenForLaunchedTransition);

    if(m_bLaunchedAsHost)
	{
		// broadcast launch message
		SendLaunchMessage();

		// we want to cancel this here - we are no longer on call 
		// and this would continue to advertise the session as such
		MarkAsWaitingAsync(false);
	}

	// we need to end any current sessions
	bool bSuccess = true; 
	if(IsSessionCreated())
	{
		// let everyone in freeroam know so that they can rule the local player out for a migration
		// should the session host be leaving in a corona with us
		MsgTransitionLaunchNotify msgLaunchNotify(NetworkInterface::GetLocalGamerHandle());
		m_pSession->BroadcastMsg(msgLaunchNotify, NET_SEND_RELIABLE);

		// and exit
		LeaveSession(LeaveFlags::LF_Default, Leave_TransitionLaunch);
	}
	else
	{
		gnetDebug1("LaunchTransition :: Launching from direct join without passing through game session.");
	}

	// call this when countdown has expired, when script have had a chance to execute the end call
	m_bDoLaunchPending = true; 
	m_nDoLaunchCountdown = DEFAULT_LAUNCH_COUNTDOWN;

	// grab timestamp
#if !__NO_OUTPUT
	m_nLaunchTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

	// generate script event
	GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_START_LAUNCH));

	return bSuccess;
}

void CNetworkSession::SetDoNotLaunchFromJoinAsMigratedHost(bool bLaunch)
{
	if(m_bDoNotLaunchFromJoinAsMigratedHost != bLaunch)
	{
		gnetDebug1("SetDoNotLaunchFromJoinAsMigratedHost :: %s", bLaunch ? "True" : "False");
		m_bDoNotLaunchFromJoinAsMigratedHost = bLaunch;
	}
}

void CNetworkSession::SwapToGameSession()
{
	gnetDebug1("SwapToGameSession");

    // setup the transition state
	switch(m_SessionState)
	{
	case SS_LEAVING: SetTransitionState(TS_LEAVING); break;
	case SS_DESTROYING: SetTransitionState(TS_DESTROYING); break;
	default:
		SetTransitionState(TS_IDLE);
		break;
	}

	SwapSessions();

    // swap configs
    NetworkGameConfig tConfig = m_Config[SessionType::ST_Transition];
    m_Config[SessionType::ST_Transition] = m_Config[SessionType::ST_Physical];
    m_Config[SessionType::ST_Physical] = tConfig;

    // swap visibility lock
    bool bLockVisibility = m_bLockVisibility[SessionType::ST_Transition];
    m_bLockVisibility[SessionType::ST_Transition] = m_bLockVisibility[SessionType::ST_Physical];
    m_bLockVisibility[SessionType::ST_Physical] = bLockVisibility;
    bool bLockSetting = m_bLockSetting[SessionType::ST_Transition];
    m_bLockSetting[SessionType::ST_Transition] = m_bLockSetting[SessionType::ST_Physical];
    m_bLockSetting[SessionType::ST_Physical] = bLockSetting;

    // swap attribute change state
    bool bChangeAttributesPendingTransition = m_bChangeAttributesPending[SessionType::ST_Transition];
    m_bChangeAttributesPending[SessionType::ST_Transition] = m_bChangeAttributesPending[SessionType::ST_Physical];
    m_bChangeAttributesPending[SessionType::ST_Physical] = bChangeAttributesPendingTransition;

    // swap crew tracking
    sUniqueCrewData tData = m_UniqueCrewData[SessionType::ST_Transition];
    m_UniqueCrewData[SessionType::ST_Transition] = m_UniqueCrewData[SessionType::ST_Physical];
    m_UniqueCrewData[SessionType::ST_Physical] = tData;

	// migrate invites
	m_InvitedPlayers = m_TransitionInvitedPlayers;
	m_TransitionInvitedPlayers.ClearAllInvites();

	// migrate tracked invite
	m_bJoinedViaInvite = m_bJoinedTransitionViaInvite;
	m_bJoinedTransitionViaInvite = false;
	m_Inviter = m_TransitionInviter;
	m_TransitionInviter.Clear();
}

void CNetworkSession::SwapToTransitionSession()
{
	gnetDebug1("SwapToTransitionSession");

	SwapSessions();

    // fudge to established state
	SetTransitionState(TS_ESTABLISHED, true);
	
    // swap configs
    m_Config[SessionType::ST_Transition] = m_Config[SessionType::ST_Physical];
    m_Config[SessionType::ST_Physical].Clear();

    // swap visibility lock
    m_bLockVisibility[SessionType::ST_Transition] = m_bLockVisibility[SessionType::ST_Physical];
    m_bLockVisibility[SessionType::ST_Physical] = false;
    m_bLockSetting[SessionType::ST_Transition] = m_bLockSetting[SessionType::ST_Physical];
    m_bLockSetting[SessionType::ST_Physical] = false;

    // swap attribute change state
    m_bChangeAttributesPending[SessionType::ST_Transition] = m_bChangeAttributesPending[SessionType::ST_Physical];
    m_bChangeAttributesPending[SessionType::ST_Physical] = false;

	// migrate invites
	m_TransitionInvitedPlayers = m_InvitedPlayers;
	m_InvitedPlayers.ClearAllInvites();

	// migrate tracked invite
	m_bJoinedTransitionViaInvite = m_bJoinedViaInvite;
    m_bJoinedViaInvite = false;
    m_TransitionInviter = m_Inviter;
    m_Inviter.Clear();

    // swap crew tracking (clear for main)
    m_UniqueCrewData[SessionType::ST_Transition] = m_UniqueCrewData[SessionType::ST_Physical];
    m_UniqueCrewData[SessionType::ST_Physical].Clear();

    // additional freemode flattening
    m_bBlockJoinRequests = false; 
    m_bMainSyncedRadio = false;
}

void CNetworkSession::SwapSessions()
{
    gnetDebug1("SwapSessions");

    // swap session pointers
    snSession* pTempSession = m_pSession;
    m_pSession = m_pTransition;
    m_pTransition = pTempSession;

    // swap status pointers
    netStatus* pTempStatus = m_pSessionStatus;
    m_pSessionStatus = m_pTransitionStatus;
    m_pTransitionStatus = pTempStatus;

    // swap radio sync state
    bool bTransitionSyncedRadio = m_bTransitionSyncedRadio;
    m_bTransitionSyncedRadio = m_bMainSyncedRadio;
    m_bMainSyncedRadio = bTransitionSyncedRadio;

	// swap host flags
	unsigned nHostFlags = m_nHostFlags[SessionType::ST_Physical];
	m_nHostFlags[SessionType::ST_Physical] = m_nHostFlags[SessionType::ST_Transition];
	m_nHostFlags[SessionType::ST_Transition] = nHostFlags;

	// swap matchmaking IDs
	u64 nUniqueMatchmakingID = m_nUniqueMatchmakingID[SessionType::ST_Physical];
	m_nUniqueMatchmakingID[SessionType::ST_Physical] = m_nUniqueMatchmakingID[SessionType::ST_Transition];
	m_nUniqueMatchmakingID[SessionType::ST_Transition] = nUniqueMatchmakingID;

	u64 timeCreated = m_TimeCreated[SessionType::ST_Physical];
	m_TimeCreated[SessionType::ST_Physical] = m_TimeCreated[SessionType::ST_Transition];
	m_TimeCreated[SessionType::ST_Transition] = timeCreated;

#if RSG_DURANGO
	// swap event flags 
	bool bHasWrittenStartEvent = m_bHasWrittenStartEvent[SessionType::ST_Physical];
	m_bHasWrittenStartEvent[SessionType::ST_Physical] = m_bHasWrittenStartEvent[SessionType::ST_Transition];
	m_bHasWrittenStartEvent[SessionType::ST_Transition] = bHasWrittenStartEvent;
#endif

    // swap block states
    bool bBlockTransitionJoinRequests = m_bBlockTransitionJoinRequests;
    m_bBlockTransitionJoinRequests = m_bBlockJoinRequests;
    m_bBlockJoinRequests = bBlockTransitionJoinRequests;

	// swap matchmaking pools
	eMultiplayerPool nTempPool = m_nSessionPool[SessionType::ST_Physical];
	m_nSessionPool[SessionType::ST_Physical] = m_nSessionPool[SessionType::ST_Transition];
	m_nSessionPool[SessionType::ST_Transition] = nTempPool;

    // remove existing delegates
    m_pTransition->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Physical]);
    m_pSession->RemoveDelegate(&m_SessionDlgt[SessionType::ST_Transition]);

    // setup game session
    m_pSession->SetOwner(m_SessionOwner[SessionType::ST_Physical]);
    m_pSession->AddDelegate(&m_SessionDlgt[SessionType::ST_Physical]);

    // apply attribute names
    m_pSession->SetPresenceAttributeNames(rlScAttributeId::GameSessionToken.Name,
										  rlScAttributeId::GameSessionId.Name,
                                          rlScAttributeId::GameSessionInfo.Name,
                                          rlScAttributeId::IsGameHost.Name,
                                          rlScAttributeId::IsGameJoinable.Name,
                                          true);

    // setup transition session
    m_pTransition->SetOwner(m_SessionOwner[SessionType::ST_Transition]);
    m_pTransition->AddDelegate(&m_SessionDlgt[SessionType::ST_Transition]);

    // apply attribute names
    m_pTransition->SetPresenceAttributeNames(ATTR_TRANSITION_TOKEN,
											 ATTR_TRANSITION_ID,		
                                             ATTR_TRANSITION_INFO,
                                             ATTR_TRANSITION_IS_HOST,
                                             ATTR_TRANSITION_IS_JOINABLE,
                                             true);

    // reset this, no longer applies
    m_PendingHostPhysical = false;

#if RSG_PRESENCE_SESSION
	// reset this flag so that we try again next time
	m_bPresenceSessionFailed = false;
#endif
}

snSession* CNetworkSession::GetCurrentSession(bool bAllowLeaving)
{
	// if in a corona, that is the current session
	if(m_pTransition->Exists() && !IsActivitySession() && (bAllowLeaving || !IsTransitionLeaving()))
		return m_pTransition;
	else if(m_pSession->Exists() && (bAllowLeaving || !IsSessionLeaving()))
		return m_pSession;

	// no current session
	return NULL;
}

snSession* CNetworkSession::GetSessionFromChannelID(unsigned nChannelID, bool bAllowLeaving)
{
	if(m_pTransition->Exists() && (m_pTransition->GetChannelId() == nChannelID) && (bAllowLeaving || !IsTransitionLeaving()))
		return m_pTransition;
	else if(m_pSession->Exists() && (m_pSession->GetChannelId() == nChannelID) && (bAllowLeaving || !IsSessionLeaving()))
		return m_pSession;

	// no relevant session
	return NULL; 
}

snSession* CNetworkSession::GetSessionFromToken(u64 nSessionToken, bool bAllowLeaving)
{
	if(m_pTransition->Exists() && (m_pTransition->GetSessionInfo().GetToken().m_Value == nSessionToken) && (bAllowLeaving || !IsTransitionLeaving()))
		return m_pTransition;
	else if(m_pSession->Exists() && (m_pSession->GetSessionInfo().GetToken().m_Value == nSessionToken) && (bAllowLeaving || !IsSessionLeaving()))
		return m_pSession;

	// no relevant session
	return NULL; 
}

SessionType CNetworkSession::GetSessionType(snSession* pSession)
{
	if(pSession == m_pSession)
		return IsActivitySession() ? SessionType::ST_Transition : SessionType::ST_Physical;
	else if(pSession == m_pTransition)
		return IsActivitySession() ? SessionType::ST_Physical : SessionType::ST_Transition;

	return SessionType::ST_None;
}

snSession* CNetworkSession::GetSessionPtr(const SessionType sessionType)
{
	switch(sessionType)
	{
	case SessionType::ST_Physical: return m_pSession;
	case SessionType::ST_Transition: return m_pTransition;
	default: 
		gnetAssertf(0, "GetSessionPtr :: Invalid Session Index"); 
		return NULL;
	}
}

void CNetworkSession::SendLaunchMessage(u64 nPeerID)
{
	// send message and increment tracking
	MsgTransitionLaunch msgLaunch;

	// include network time
	msgLaunch.m_HostTimeToken    = m_TimeTokenForLaunchedTransition;
	msgLaunch.m_HostTime = CNetwork::GetNetworkTime();
	msgLaunch.m_ModelVariationID = gPopStreaming.GetNetworkModelVariationID();
	msgLaunch.m_TimeCreated = m_TimeCreated[SessionType::ST_Transition];

	if(nPeerID == 0)
	{
		// log out message clock contents
		gnetDebug2("SendLaunchMessage :: Broadcasting launch message");
		gnetDebug2("\tLocal Override: %s", CNetwork::IsClockTimeOverridden() ? "True" : "False");
        gnetDebug2("\tHost Time Token: %d", msgLaunch.m_HostTimeToken);
		gnetDebug2("\tHost Time: %u", msgLaunch.m_HostTime);
		
		// send to all transition participants
		m_pTransition->BroadcastMsg(msgLaunch, NET_SEND_RELIABLE);
	}
	else
	{
#if !__NO_OUTPUT
		rlGamerInfo hGamer[1];
		m_pSession->GetGamers(nPeerID, hGamer, 1);
		gnetDebug2("LaunchTransition :: Sending launch message to %s", hGamer->GetName());
#endif	
		m_pTransition->SendMsg(nPeerID, msgLaunch, NET_SEND_RELIABLE);
	}
}

bool CNetworkSession::DoLaunchTransition()
{
	gnetDebug1("DoLaunchTransition :: All ready, starting. Time taken: %d", sysTimer::GetSystemMsTime() - m_nLaunchTrackingTimestamp);
#if !__NO_OUTPUT
	m_nLaunchTrackingTimestamp = sysTimer::GetSystemMsTime();
#endif

	// there isn't a lot we can do in these situations apart from bail
	if(!gnetVerify(!IsTransitionLeaving()))
	{
		gnetDebug1("DoLaunchTransition :: We're in the process of leaving!");
		CNetwork::Bail(sBailParameters(BAIL_TRANSITION_LAUNCH_FAILED, BAIL_CTX_TRANSITION_LAUNCH_LEAVING));
	}
	
	// there isn't a lot we can do in these situations apart from bail
	if(!gnetVerify(IsTransitionActive()))
	{
		gnetDebug1("DoLaunchTransition :: We're not in a transition!");
		CNetwork::Bail(sBailParameters(BAIL_TRANSITION_LAUNCH_FAILED, BAIL_CTX_TRANSITION_LAUNCH_NO_TRANSITION));
	}

	// swap sessions - grab the transition state prior to this so that we can map it correctly
	unsigned nPreviousTransitionState = m_TransitionState;
	SwapToGameSession();

	// get current channel policies, and set that we don't maintain a keep alive.
	// we can now use the game channel for activities
	netChannelPolicies tPolicies; 
	CNetwork::GetConnectionManager().GetChannelPolicies(m_pTransition->GetChannelId(), &tPolicies);
	tPolicies.m_AlwaysMaintainKeepAlive = false; 
	CNetwork::GetConnectionManager().SetChannelPolicies(m_pTransition->GetChannelId(), tPolicies);

	// open the network (if not already)
	if(!CNetwork::IsNetworkOpen())
		OpenNetwork();

    // allocate an initial roaming bubble for our local player if we are in the session on our own
    if(NetworkInterface::IsHost() && !NetworkInterface::GetLocalPlayer()->IsInRoamingBubble())
        CNetwork::GetRoamingBubbleMgr().AllocatePlayerInitialBubble(NetworkInterface::GetLocalPlayer());

	// reset the peer complainer
	InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession);
	m_PeerComplainer[SessionType::ST_Transition].Shutdown();
	
	// any straddling display name tasks need to be told not to add the player - we do it ourselves here
	m_pSession->NotifyDisplayNameTasksAddedPlayer();

	if(m_pSession->IsHost())
	{
		// need to set ourselves up as clock server (if we've come from a session where
		// the transition host was not the session host)
        CNetwork::GetNetworkClock().Restart(netTimeSync::FLAG_SERVER, &m_TimeTokenForLaunchedTransition, 0);

		// update network synced time and reset game clock to prevent it jumping at the next update
		CNetwork::SetSyncedTimeInMilliseconds(NetworkInterface::GetNetworkTime());
	}
	else
	{
		// log out message clock contents
		gnetDebug2("LaunchTransition :: Applying launch message");
		gnetDebug2("\tLocal Override: %s", CNetwork::IsClockTimeOverridden() ? "True" : "False");
        gnetDebug2("\tHost Time Token: %d", m_sMsgLaunch.m_HostTimeToken);
        gnetDebug2("\tHost Time: %d", m_sMsgLaunch.m_HostTime);

		EndpointId aHost;
		m_pSession->GetHostEndpointId(&aHost);

        if(CNetwork::GetNetworkClock().HasStarted())
            CNetwork::GetNetworkClock().Stop();

        CNetwork::GetNetworkClock().Start(&CNetwork::GetConnectionManager(),
										  netTimeSync::FLAG_CLIENT,
										  aHost,
                                          m_sMsgLaunch.m_HostTimeToken,
										  &m_sMsgLaunch.m_HostTime,
										  NETWORK_GAME_CHANNEL_ID,
										  netTimeSync::DEFAULT_INITIAL_PING_INTERVAL,
										  netTimeSync::DEFAULT_FINAL_PING_INTERVAL);

		gnetAssert(NetworkInterface::NetworkClockHasSynced());
		CNetwork::SetSyncedTimeInMilliseconds(m_sMsgLaunch.m_HostTime);
		gPopStreaming.SetNetworkModelVariationID(m_sMsgLaunch.m_ModelVariationID);
		m_TimeCreated[SessionType::ST_Physical] = m_sMsgLaunch.m_TimeCreated;
	}

	// call this again here (we would already have done this for hosts in LaunchTransition)
	// make sure that sessions are no longer advertised as on call when we launch
	MarkAsWaitingAsync(false);

	// use aggressive policies during launch
	SetPeerComplainerPolicies(&m_PeerComplainer[SessionType::ST_Physical], m_pSession, POLICY_AGGRESSIVE);

	// start complaint register for clients
	m_nTransitionClientComplaintTimeCheck = sysTimer::GetSystemMsTime() | 0x01;

	// we need to set a state for the main session flow
	switch(nPreviousTransitionState)
    {
		case TS_ESTABLISHING:
#if !__NO_OUTPUT
			if(!gnetVerify(m_bAllowImmediateLaunchDuringJoin))
				gnetError("DoLaunchTransition :: Invalid InLobby state for non-immediate launch!");
#endif
			CNetwork::EnterLobby();
			SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);
			SetSessionState(SS_ESTABLISHING);
			break;

		case TS_ESTABLISHED:
			CNetwork::EnterLobby();
			SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch]);
			SetSessionState(SS_ESTABLISHING);
			break;

		default:
			// run the standard launch
			gnetError("DoLaunchTransition :: Invalid state (%d)!", nPreviousTransitionState);
			CNetwork::EnterLobby();
			SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_TransitionLaunch]);
			SetSessionState(SS_ESTABLISHING);
			break;
	}

    // reset transition chat override
    CNetwork::GetVoice().SetOverrideTransitionChatScript(false);
    CNetwork::GetVoice().SetRemainInGameChat(false);

	// grab all gamers
	rlGamerInfo aInfo[RL_MAX_GAMERS_PER_SESSION];
	unsigned nGamers = m_pSession->GetGamers(aInfo, RL_MAX_GAMERS_PER_SESSION);

    // need to build a player data message for all players
	for(int i = 0; i < nGamers; i++)
	{
		CNetGamePlayerDataMsg tPlayerData;

        // and now find this player in our own array
        int nPlayers = m_TransitionPlayers.GetCount();
        for(int j = 0; j < nPlayers; j++)
        {
            if(m_TransitionPlayers[j].m_hGamer == aInfo[i].GetGamerHandle())
            {
				// individual player data settings
				BuildTransitionPlayerData(&tPlayerData, &m_TransitionPlayers[j]);
                break;
            }
        }

		int nConnectionID = m_pSession->GetPeerCxn(aInfo[i].GetPeerInfo().GetPeerId());
        const EndpointId endpointId = m_pSession->GetPeerEndpointId(aInfo[i].GetPeerInfo().GetPeerId());
		const bool isConnected = (nConnectionID >= 0) && NET_IS_VALID_ENDPOINT_ID(endpointId) && m_pSession->GetCxnMgr()->GetAddress(endpointId).IsValid();

		// check that we can allocate 
		CNonPhysicalPlayerData* pNonPhysicalData = TryAllocateNonPhysicalPlayerData();

        // it's possible that this player left / errored during the launch process but the host hasn't 
        // informed us yet. Validate the address and, if not, just don't add this player. 
        // It would generate a long assert chain if we did. The peer complainer system will sort out any 
        // genuinely broken p2p connections. It's safe to call RemovePlayer if we did not add.
        if((aInfo[i].IsLocal() || isConnected) && gnetVerify(pNonPhysicalData))
        {
            gnetDebug1("DoLaunchTransition :: Adding Player: %s", aInfo[i].GetName());

            // add this player
            CNetwork::GetPlayerMgr().AddPlayer(aInfo[i], endpointId, tPlayerData, pNonPhysicalData);

            // add to live manager for tracking
            CLiveManager::AddPlayer(aInfo[i]);

            // add met gamer
            if(aInfo[i].IsRemote())
                m_RecentPlayers.AddRecentPlayer(aInfo[i]);
        }
        else
		{
            gnetDebug1("DoLaunchTransition :: Not Adding Player: %s. Invalid Address. Complaining: %s", aInfo[i].GetName(), m_PeerComplainer[SessionType::ST_Physical].HavePeer(aInfo[i].GetPeerInfo().GetPeerId()) ? "True" : "False");
			TryPeerComplaint(aInfo[i].GetPeerInfo().GetPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical OUTPUT_ONLY(, true));
		}
	}

    // update matchmaking groups
    UpdateMatchmakingGroups();

    // dump transition players - we only need this to store players
    m_TransitionPlayers.Reset();

	// reset launch flag and flag that we're waiting for the session to start
	m_bLaunchingTransition = false;
	m_bTransitionStartPending = true;
    
	// this is now an activity session
    m_bIsActivitySession = true;

    // apply the activity game mode when we launch
    CNetwork::SetNetworkGameMode(m_Config[SessionType::ST_Physical].GetGameMode());
    
#if RSG_PRESENCE_SESSION
	// presence session no longer required, picked up by activity session
	m_bRequirePresenceSession = false;
#endif

	// success
	return true; 
}

bool CNetworkSession::SendTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers, const char* pSubject /* = NULL */, const char* pMessage /* = NULL */)
{
	// validate incoming text
	gnetAssertf(!pSubject || strlen(pSubject) < RL_MAX_INVITE_SUBJECT_CHARS, "SendTransitionInvites :: Invalid subject!");
	gnetAssertf(!pMessage || strlen(pMessage) < RL_MAX_INVITE_SALUTATION_CHARS, "SendTransitionInvites :: Invalid salutation!");

	// we use a standard header and message if these haven't been supplied
	const char* szSubject = pSubject ? pSubject : TheText.Get("INVITE_SUBJECT");
	const char* szMessage = pMessage ? pMessage : TheText.Get("INVITE_MSG");

	// send the invite
	bool bSuccess = false;
#if RSG_PRESENCE_SESSION
	snSession* pPresenceSession = GetCurrentPresenceSession();
	if(pPresenceSession)
		bSuccess = pPresenceSession->SendInvites(pGamers, nGamers, szSubject, szMessage, NULL);
#else
	bSuccess = m_pTransition->SendInvites(pGamers, nGamers, szSubject, szMessage, NULL);
#endif
	if(bSuccess)
		m_TransitionInvitedPlayers.AddInvites(pGamers, nGamers, CNetworkInvitedPlayers::FLAG_ALLOW_TYPE_CHANGE);

#if !__NO_OUTPUT
    gnetDebug3("SendTransitionInvites :: %s", bSuccess ? "Success" : "Failed");
	for(int i = 0; i < nGamers; i++)
        gnetDebug3("\tInvited %s", NetworkUtils::LogGamerHandle(pGamers[i]));
#endif

	return bSuccess;
}

bool CNetworkSession::SendPresenceInvitesToTransition(
	const rlGamerHandle* pGamers,
	const bool* pbImportant,
	const unsigned nGamers,
	const char* szContentId,
	const char* pUniqueMatchId,
	const int nInviteFrom,
	const int nInviteType,
	const unsigned nPlaylistLength,
	const unsigned nPlaylistCurrent)
{
	// check that the session is established
	if(!gnetVerify(m_pTransition->Exists()))
	{
		gnetError("SendPresenceInvitesToTransition :: Session not established!");
		return false;
	}

	// indicate that we need an ack if this invite is important
	unsigned nFlags = pbImportant[0] ? 0 : InviteFlags::IF_NotTargeted;

#if RSG_DURANGO
	rlFireAndForgetTask<SendPresenceInviteTask>* pTask = NULL;
	rlGetTaskManager()->CreateTask(&pTask);
	if(pTask)
	{
		rlTaskBase::Configure(pTask, nGamers, pGamers, m_pTransition->GetSessionInfo(), szContentId, m_SessionUserDataActivity.m_hContentCreator, pUniqueMatchId, nInviteFrom, nInviteType, nPlaylistLength, nPlaylistCurrent, nFlags, &pTask->m_Status);
		rlGetTaskManager()->AddParallelTask(pTask);
	}
	else // send the invite anyway
#endif
	{
		// invite can be sent outside multiplayer so use presence
		CGameInvite::Trigger(
			pGamers, 
			nGamers, 
			m_pTransition->GetSessionInfo(), 
			szContentId, 
			m_SessionUserDataActivity.m_hContentCreator,
			pUniqueMatchId,
			nInviteFrom,
			nInviteType,
			nPlaylistLength, 
			nPlaylistCurrent, 
			nFlags);
	}

	for(unsigned i = 0; i < nGamers; i++)
	{
		// check that the gamer handle is valid
		if(!gnetVerify(pGamers[i].IsValid()))
		{
			gnetError("SendPresenceInvitesToTransition :: Invalid gamer handle!");
			continue;
		}

		// invite flags 
		unsigned nFlags = CNetworkInvitedPlayers::FLAG_ALLOW_TYPE_CHANGE | CNetworkInvitedPlayers::FLAG_VIA_PRESENCE;
		if(pbImportant[i])
			nFlags |= CNetworkInvitedPlayers::FLAG_IMPORTANT;

		// log out request
		gnetDebug3("SendPresenceInvitesToTransition :: Inviting %s", NetworkUtils::LogGamerHandle(pGamers[i]));
		m_TransitionInvitedPlayers.AddInvite(pGamers[i], nFlags);
	}

	return true;
}

void CNetworkSession::RemoveTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
	for(unsigned i = 0; i < nGamers; i++)
	{
		// log out request
		gnetDebug3("RemoveTransitionInvites :: Removing invite for %s", NetworkUtils::LogGamerHandle(pGamers[i]));
		m_TransitionInvitedPlayers.RemoveInvite(pGamers[i]);
	}
}

void CNetworkSession::RemoveAllTransitionInvites()
{
	gnetDebug3("RemoveAllTransitionInvites");
	m_TransitionInvitedPlayers.ClearAllInvites();
}

bool CNetworkSession::CancelTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
	// check that the session is established
	if(!gnetVerify(m_pTransition->Exists()))
	{
		gnetError("CancelTransitionInvites :: Session not established!");
		return false; 
	}

	unsigned int invitesToCancelCount=0;
	rlGamerHandle inviteesToCancel[MAX_INVITES_TO_CANCEL];
	for(unsigned i = 0; i < nGamers; i++)
	{
		// check that the gamer handle is valid
		if(!gnetVerify(pGamers[i].IsValid()))
		{
			gnetError("CancelTransitionInvites :: Invalid gamer handle!");
			continue; 
		}

		// check we have a pending invite for this gamer
		if(!m_TransitionInvitedPlayers.DidInvite(pGamers[i]))
			continue;

		// if we sent this via presence, fire a cancel event
		if(m_TransitionInvitedPlayers.DidInviteViaPresence(pGamers[i]))
		{
			if(gnetVerifyf(invitesToCancelCount<MAX_INVITES_TO_CANCEL, "Cancelling too many invites at the same time"))
			{
				inviteesToCancel[invitesToCancelCount]=pGamers[i];
				invitesToCancelCount++;
			}
		}
		// log out request
		gnetDebug3("CancelTransitionInvites :: Removing invite for %s", NetworkUtils::LogGamerHandle(pGamers[i]));
		m_TransitionInvitedPlayers.RemoveInvite(pGamers[i]);
	}
	if(invitesToCancelCount>0)
	{
		CGameInviteCancel::Trigger(inviteesToCancel, invitesToCancelCount, m_pTransition->GetSessionInfo());
	}
	return true;
}

void CNetworkSession::RemoveAndCancelAllTransitionInvites()
{
	// always clear the invites
	gnetDebug3("RemoveAndCancelAllTransitionInvites :: Exists: %s", m_pTransition->Exists() ? "True" : "False");
	m_TransitionInvitedPlayers.ClearAllInvites();

	// we need a valid transition session to get the rlSessionInfo
	if(m_pTransition->Exists())
	{
		unsigned int invitesToCancelCount=0;
		rlGamerHandle inviteesToCancel[MAX_INVITES_TO_CANCEL];
		int nCount = m_TransitionInvitedPlayers.GetNumInvitedPlayers();
		for(unsigned i = 0; i < nCount; i++)
		{
			// grab player
			const CNetworkInvitedPlayers::sInvitedGamer& tInvite = m_TransitionInvitedPlayers.GetInvitedPlayerFromIndex(i);
		
			// if we sent this via presence, fire a cancel event
			if(tInvite.IsViaPresence())
			{
				if(gnetVerifyf(invitesToCancelCount<MAX_INVITES_TO_CANCEL, "Cancelling too many invites at the same time"))
				{
					gnetDebug3("RemoveAndCancelAllTransitionInvites :: Cancelling invite for %s", NetworkUtils::LogGamerHandle(tInvite.m_hGamer));
					inviteesToCancel[invitesToCancelCount]=tInvite.m_hGamer;
					invitesToCancelCount++;
				}	
			}
		}

		if(invitesToCancelCount>0)
		{
			CGameInviteCancel::Trigger(inviteesToCancel, invitesToCancelCount, m_pTransition->GetSessionInfo());
		}
	}
}

void CNetworkSession::AddTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
#if !__NO_OUTPUT
	for(int i = 0; i < nGamers; i++)
		gnetDebug3("AddTransitionInvites :: Inviting %s", NetworkUtils::LogGamerHandle(pGamers[i]));
#endif

	m_TransitionInvitedPlayers.AddInvites(pGamers, nGamers, CNetworkInvitedPlayers::FLAG_SCRIPT_DUMMY);
}

unsigned CNetworkSession::GetNumTransitionNonAsyncGamers()
{
	unsigned nGamers = 0;

	int nPlayers = m_TransitionPlayers.GetCount();
	for(int i = 0; i < nPlayers; i++)
	{
		if(!m_TransitionPlayers[i].m_bIsAsync)
			nGamers++;
	}

	return nGamers;
}

unsigned CNetworkSession::GetTransitionMemberCount()
{
    return m_pTransition->GetGamerCount();
}

unsigned CNetworkSession::GetTransitionMembers(rlGamerInfo aInfo[], const unsigned nMaxGamers) const
{
	return m_pTransition->GetGamers(aInfo, nMaxGamers);
}

bool CNetworkSession::IsTransitionMember(const rlGamerHandle& hGamer) const
{
	return m_pTransition->IsMember(hGamer);
}

bool CNetworkSession::IsTransitionMemberSpectating(const rlGamerHandle& hGamer) const
{
	int nPlayers = m_TransitionPlayers.GetCount();
	for(int i = 0; i < nPlayers; i++)
	{
		if(m_TransitionPlayers[i].m_hGamer == hGamer)
			return m_TransitionPlayers[i].m_bIsSpectator;
	}

	return false;
}

bool CNetworkSession::IsTransitionMemberAsync(const rlGamerHandle& hGamer) const
{
	int nPlayers = m_TransitionPlayers.GetCount();
	for(int i = 0; i < nPlayers; i++)
	{
		if(m_TransitionPlayers[i].m_hGamer == hGamer)
			return m_TransitionPlayers[i].m_bIsAsync;
	}

	return false;
}

bool CNetworkSession::GetTransitionNetEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId)
{
	*endpointId = NET_INVALID_ENDPOINT_ID;

	// check we have a valid member first
	if(!m_pTransition->IsMember(sInfo.GetGamerId()))
		return false;

	int nConnectionID = m_pTransition->GetPeerCxn(sInfo.GetPeerInfo().GetPeerId());
    const EndpointId epId = m_pTransition->GetPeerEndpointId(sInfo.GetPeerInfo().GetPeerId());
	if((nConnectionID >= 0) && NET_IS_VALID_ENDPOINT_ID(epId))
    {
        *endpointId = epId;
        return true;
    }
    return false;
}

bool CNetworkSession::GetTransitionMemberInfo(const rlGamerHandle& hGamer, rlGamerInfo* pInfo) const
{
    return m_pTransition->GetGamerInfo(hGamer, pInfo);
}

bool CNetworkSession::GetTransitionHost(rlGamerHandle* pHandle)
{
    pHandle->Clear();

    u64 nHostPeerID;
    if(!m_pTransition->GetHostPeerId(&nHostPeerID))
        return false;

    rlGamerHandle hHost[1]; 
    if(m_pTransition->GetGamers(nHostPeerID, hHost, 1) == 0)
        return false;

    *pHandle = hHost[0];
    return true;
}

void CNetworkSession::ApplyTransitionParameter(u16 nID, u32 nValue, bool bForceDirty)
{
	for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
	{
		if(m_TransitionData.m_aParameters[i].m_nID == nID)
		{
			if(m_TransitionData.m_aParameters[i].m_nValue != nValue)
			{
				netTransitionMsgDebug("ApplyTransitionParameter :: Update - ID: %d, Value: %d", nID, nValue);

				// update value and indicate dirty state
				m_TransitionData.m_aParameters[i].m_nValue = nValue;
				m_bIsTransitionParameterDirty[i] = true; 	
			}
			else if(bForceDirty)
			{
				netTransitionMsgDebug("ApplyTransitionParameter :: ForceDirty - ID: %d, Value: %d", nID, nValue);
				m_bIsTransitionParameterDirty[i] = true; 
			}

			// our work here is done
			return; 
		}
	}

	// still here? - find an empty slot
	for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
	{
		if(m_TransitionData.m_aParameters[i].m_nID == TRANSITION_INVALID_ID)
		{
			netTransitionMsgDebug("ApplyTransitionParameter :: Adding - ID: %d, Value: %d", nID, nValue);

			// update value and indicate dirty state
			Assign(m_TransitionData.m_aParameters[i].m_nID, nID);
			m_TransitionData.m_aParameters[i].m_nValue = nValue;
			m_bIsTransitionParameterDirty[i] = true; 

			// our work here is done
			return; 
		}
	}

	// still here?! - we've run out of parameter slots
	gnetAssertf(0, "ApplyTransitionParameter :: No free slots - Maximum of %u hit!", TRANSITION_MAX_PARAMETERS);
}

void CNetworkSession::ApplyTransitionParameter(int nIndex, const char* szValue, bool bForceDirty)
{
    // safety checks
    if(nIndex < 0 || nIndex >= TRANSITION_MAX_STRINGS)
        return;

	if(strncmp(szValue, m_TransitionData.m_szString[nIndex], TRANSITION_PARAMETER_STRING_MAX) != 0)
	{
		netTransitionMsgDebug("ApplyTransitionParameter :: Update - Index: %d, Value: %s", nIndex, szValue);
		safecpy(m_TransitionData.m_szString[nIndex], szValue);
		m_bIsTransitionStringDirty[nIndex] = true;
	}
	else if(bForceDirty)
	{
		netTransitionMsgDebug("ApplyTransitionParameter :: ForceDirty - Index: %d, Value: %s", nIndex, szValue);
		m_bIsTransitionStringDirty[nIndex] = true; 
	}
}

void CNetworkSession::ClearTransitionParameters()
{
	netTransitionMsgDebug("ClearTransitionParameters");
	
	for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
	{
		m_TransitionData.m_aParameters[i].m_nID = TRANSITION_INVALID_ID;
		m_bIsTransitionParameterDirty[i] = false;
	}

    for(unsigned i = 0; i < TRANSITION_MAX_STRINGS; i++)
    {
        m_TransitionData.m_szString[i][0] = '\0';
        m_bIsTransitionStringDirty[i] = false;
    }
	
	m_nLastTransitionMessageTime = 0;
}

void CNetworkSession::ForceSendTransitionParameters(const rlGamerInfo& hInfo)
{
    unsigned nNumMessageParameters = 0;
    sTransitionParameter aMessageParameters[TRANSITION_MAX_PARAMETERS];

    for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
    {
        if(m_TransitionData.m_aParameters[i].m_nID != TRANSITION_INVALID_ID)
        {
            aMessageParameters[nNumMessageParameters].m_nID = m_TransitionData.m_aParameters[i].m_nID;
            aMessageParameters[nNumMessageParameters].m_nValue = m_TransitionData.m_aParameters[i].m_nValue;
            nNumMessageParameters++;
        }
    }

	// we need to work out which session to send these via
	snSession* pSession = IsActivitySession() ? m_pSession : m_pTransition;

    // if we have some parameters to send
    if(nNumMessageParameters > 0)
    {
        // setup message
        MsgTransitionParameters msg;
		msg.Reset(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), aMessageParameters, nNumMessageParameters);

		// send message
		pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
    }

    // string parameter
	OUTPUT_ONLY(int nStringParameters = 0);
    for(unsigned i = 0; i < TRANSITION_MAX_STRINGS; i++)
    {
        if(m_TransitionData.m_szString[i][0] != '\0')
        {
			OUTPUT_ONLY(nStringParameters++);

            // setup message
            MsgTransitionParameterString msg;
			msg.Reset(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), m_TransitionData.m_szString[i]);

            // send message
            pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
        }
    }

	// logging
	netTransitionMsgDebug("ForceSendTransitionParameters :: To: %s, Session: %s, Num: %u, Strings: %d", hInfo.GetName(), IsActivitySession() ? "Activity" : "Transition", nNumMessageParameters, nStringParameters);
}

bool CNetworkSession::SendTransitionGamerInstruction(const rlGamerHandle& hGamer, const char* szGamerName, const int nInstruction, const int nInstructionParam, const bool bBroadcast)
{
	// check session is valid
	if(!netTransitionMsgVerify(IsTransitionActive()))
	{
		netTransitionMsgError("SendTransitionGamerInstruction :: Not in a transition!");
		return false;
	}

	// check handle is valid
	if(!netTransitionMsgVerify(hGamer.IsValid()))
	{
		netTransitionMsgError("SendTransitionGamerInstruction :: Invalid gamer handle!");
		return false;
	}

	// get local player info
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!netTransitionMsgVerify(pInfo))
	{
		netTransitionMsgError("SendTransitionGamerInstruction :: Local gamer not signed in!");
		return false;
	}

	// make sure we have the biggest of these values
	CompileTimeAssert(RL_MAX_DISPLAY_NAME_BUF_SIZE >= RL_MAX_NAME_BUF_SIZE);

	// build message
	MsgTransitionGamerInstruction msg;
	msg.m_hToGamer = hGamer;
	msg.m_hFromGamer = pInfo->GetGamerHandle();
    safecpy(msg.m_szGamerName, szGamerName);
	msg.m_nInstruction = nInstruction;
	msg.m_nInstructionParam = nInstructionParam;
	msg.m_nUniqueToken = static_cast<unsigned>(gs_Random.GetInt());

	// we need to work out which session to send these via
	snSession* pSession = IsActivitySession() ? m_pSession : m_pTransition;

	// logging
	netTransitionMsgDebug("SendTransitionGamerInstruction :: Sending MsgTransitionGamerInstruction - Gamer: %s, From: %s, Name: %s, Instruction: %d, Param: %d, Token: 0x%08x, Broadcast: %s, Session: %s", NetworkUtils::LogGamerHandle(msg.m_hToGamer), NetworkUtils::LogGamerHandle(msg.m_hFromGamer), szGamerName, nInstruction, nInstructionParam, msg.m_nUniqueToken, bBroadcast ? "Yes" : "No", IsActivitySession() ? "Activity" : "Transition");
	
	// send message
	if(bBroadcast)
		pSession->BroadcastMsg(msg, NET_SEND_RELIABLE);
	else
	{
        // check this handle is a member of the transition session
        if(!netTransitionMsgVerify(pSession->IsMember(hGamer)))
        {
            netTransitionMsgError("SendTransitionGamerInstruction :: %s is not a member of the transition session!", NetworkUtils::LogGamerHandle(hGamer));
            return false;
        }

		// grab gamer info
		rlGamerInfo hInfo;
		pSession->GetGamerInfo(hGamer, &hInfo);

		// logging
		netTransitionMsgDebug("SendTransitionGamerInstruction :: Sending MsgTransitionGamerInstruction to %s [%s], ID: %" I64FMT "d", hInfo.GetName(), NetworkUtils::LogGamerHandle(hInfo.GetGamerHandle()), hInfo.GetPeerInfo().GetPeerId());

		// check if we're trying to send this to the local gamer
		if(hInfo.GetGamerHandle() == NetworkInterface::GetLocalGamerHandle())
		{
			netTransitionMsgError("SendTransitionGamerInstruction :: Sending to local!");
			return false;
		}

		// send only to the player
		pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
	}

	BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());

	// success
	return true;
}

bool CNetworkSession::MarkTransitionPlayerAsFullyJoined(const rlGamerHandle& hGamer)
{
    int nPlayers = m_TransitionPlayers.GetCount();
    for(int i = 0; i < nPlayers; i++)
    {
        if(m_TransitionPlayers[i].m_hGamer == hGamer)
        {
            if(!m_TransitionPlayers[i].m_bHasFullyJoinedForScript)
            {
                gnetDebug3("MarkTransitionPlayerAsFullyJoined :: %s has fully joined for script!", NetworkUtils::LogGamerHandle(hGamer));
                m_TransitionPlayers[i].m_bHasFullyJoinedForScript = true;
            }
            return true; 
        }
    }

    // didn't find this guy
    return false;
}

bool CNetworkSession::HasTransitionPlayerFullyJoined(const rlGamerHandle& hGamer)
{
    int nPlayers = m_TransitionPlayers.GetCount();
    for(int i = 0; i < nPlayers; i++)
    {
        if(m_TransitionPlayers[i].m_hGamer == hGamer)
            return m_TransitionPlayers[i].m_bHasFullyJoinedForScript;
    }

    // didn't find this guy
    return false;
}

void CNetworkSession::OnSessionEventTransition(snSession* pSession, const snEvent* pEvent)
{
	// log event
	gnetDebug1("OnSessionEvent :: Transition: 0x%016" I64FMT "x - [%08u:%u] %s", pSession->Exists() ? pSession->GetSessionInfo().GetToken().m_Value : 0, CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), pEvent->GetAutoIdNameFromId(pEvent->GetId()));
	
	// not for transition...
	if(pSession != m_pTransition)
	{
		gnetDebug1("OnSessionEvent :: Transition - For different session! Game: %s", (pSession == m_pSession) ? "True" : "False");
		return;
	}

	switch(pEvent->GetId())
	{		
	case SNET_EVENT_SESSION_HOSTED:
        
        // pull gamers in our group along
        CompleteTransitionQuickmatch();
        m_nGamersToActivity = 0;
        m_bLaunchOnStartedFromJoin = false;
        
		// initialise peer complainer - peer complainer only runs for transitions into a job
		if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity)
			InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Transition], m_pTransition);

		// generate script event
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_HOSTED));
		break; 

	case SNET_EVENT_SESSION_JOINED:
		{
            // pull gamers in our group along
            CompleteTransitionQuickmatch();
            m_nGamersToActivity = 0;

			// initialise peer complainer - peer complainer only runs for transitions into a job
			if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity)
				InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Transition], m_pTransition);

			// apply the current config - chiefly, we do this to allocate the schema index values 
            if(!m_pTransition->IsHost())
            {
				OUTPUT_ONLY(m_Config[SessionType::ST_Transition].LogDifferences(m_pTransition->GetConfig(), "\tSessionJoined"));

			    player_schema::MatchingAttributes mmAttrs;
			    m_Config[SessionType::ST_Transition].Reset(mmAttrs, m_pTransition->GetConfig().m_Attrs.GetGameMode(), m_pTransition->GetConfig().m_Attrs.GetSessionPurpose(), m_pTransition->GetConfig().m_MaxPublicSlots + m_pTransition->GetConfig().m_MaxPrivateSlots, m_pTransition->GetConfig().m_MaxPrivateSlots);
			    m_Config[SessionType::ST_Transition].Apply(m_pTransition->GetConfig());
            }

			// grab the joined event
			snEventSessionJoined* pJE = pEvent->m_SessionJoined;

			// grab the incoming data
			unsigned nMessageID;
			if(!gnetVerify(netMessage::GetId(&nMessageID, pJE->m_Data, pJE->m_SizeofData)))
			{
				gnetError("SessionJoined :: Failed to get message ID!");
				return; 
			}

			// grab join response
			CMsgJoinResponse msg;
			if(CMsgJoinResponse::MSG_ID() == nMessageID)
			{
				// validate import
				if(!gnetVerify(msg.Import(pJE->m_Data, pJE->m_SizeofData)))
				{
					gnetError("SessionJoined :: Failed to import CMsgJoinResponse message!");
					return; 
				}

				// validate session type is what we expect
				if(!gnetVerify(msg.m_SessionPurpose == SessionPurpose::SP_Activity))
				{
					gnetError("SessionJoined :: Invalid SessionPurpose (%d)!", msg.m_SessionPurpose);
					return; 
				}

				// copy in data
				m_nHostFlags[SessionType::ST_Transition] = msg.m_HostFlags;
				SetActivitySpectatorsMax(msg.m_ActivitySpectatorsMax);
				SetActivityPlayersMax(msg.m_ActivityPlayersMax);
                OUTPUT_ONLY(LogHostFlags(m_nHostFlags[SessionType::ST_Transition], __FUNCTION__));

                // copy in crew data
                m_UniqueCrewData[SessionType::ST_Transition].m_nLimit = msg.m_UniqueCrewLimit;
                m_UniqueCrewData[SessionType::ST_Transition].m_bOnlyCrews = msg.m_CrewsOnly;
                m_UniqueCrewData[SessionType::ST_Transition].m_nMaxCrewMembers = msg.m_MaxMembersPerCrew;

				if(msg.m_bHasLaunched)
				{
					gnetDebug1("\tSessionJoined :: Joined launched activity");

					if(m_bJoiningTransitionViaMatchmaking && m_bAllowBailForLaunchedJoins && CheckFlag(m_nTransitionMatchmakingFlags, MatchmakingFlags::MMF_BailFromLaunchedJobs))
					{
						gnetDebug1("\tSessionJoined :: Blocked from joining launched activities - Queuing bail");
						m_bHasPendingTransitionBail = true;
					}
					else
					{
						// copy message
						m_sMsgLaunch.m_HostTimeToken = msg.m_HostTimeToken;
						m_sMsgLaunch.m_HostTime = msg.m_HostTime;
						m_sMsgLaunch.m_ModelVariationID = msg.m_ModelVariationID;
						m_sMsgLaunch.m_TimeCreated = msg.m_TimeCreated;

						// launch the transition session
						m_bLaunchOnStartedFromJoin = true;
						m_nImmediateLaunchCountdown = m_nImmediateLaunchCountdownTime;

						// generate event
						GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_JOINED_LAUNCHED));
					}
				}
				else 
				{
					// we've joined a non-launched / corona based transition (if we have not previously received a message
					// instructing us that the session launched mid-join)
					if(!m_bLaunchOnStartedFromJoin)
						GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_JOINED));
					else // otherwise, fire the launched event
						GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_JOINED_LAUNCHED));
				}
			}
			else
				gnetError("SessionJoined :: Unhandled response message!");

			// record results for telemetry
			if(m_bJoiningTransitionViaMatchmaking)
			{
				m_TransitionMMResults.m_CandidatesToJoin[m_TransitionMMResults.m_nCandidateToJoinIndex].m_nTimeFinished = sysTimer::GetSystemMsTime();
				m_TransitionMMResults.m_CandidatesToJoin[m_TransitionMMResults.m_nCandidateToJoinIndex].m_Result = RESPONSE_ACCEPT;
			}
		}
		break;

	case SNET_EVENT_SESSION_BOT_JOIN_FAILED:
	case SNET_EVENT_JOIN_FAILED:
		{
			// grab the joined event
			snEventJoinFailed* pJF = pEvent->m_JoinFailed;

			gnetDebug1("\tJoinFailed :: Session Response: %s", NetworkUtils::GetSessionResponseAsString(pJF->m_ResponseCode));

			// for the game response
			eJoinResponseCode nResponse = RESPONSE_DENY_UNKNOWN;
			switch(pJF->m_ResponseCode)
			{
				case SNET_JOIN_DENIED_NOT_JOINABLE:			nResponse = RESPONSE_DENY_NOT_JOINABLE; break;
				case SNET_JOIN_DENIED_NO_EMPTY_SLOTS:		nResponse = RESPONSE_DENY_SESSION_FULL; break;
				case SNET_JOIN_DENIED_FAILED_TO_ESTABLISH:	nResponse = RESPONSE_DENY_FAILED_TO_ESTABLISH; break;
				case SNET_JOIN_DENIED_PRIVATE:				nResponse = RESPONSE_DENY_PRIVATE_ONLY; break;
				default: break;
			}

			// reset can queue variable
			m_bCanQueue = false;
			bool bCanQueue = false;

			// no game response, bail out
			if(pJF->m_Response)
			{
				// grab the incoming data
				unsigned nMessageID;
				if(!gnetVerify(netMessage::GetId(&nMessageID, pJF->m_Response, pJF->m_SizeofResponse)))
				{
					gnetError("JoinFailed :: Failed to get message ID!");
					return; 
				}

				if(CMsgJoinResponse::MSG_ID() == nMessageID)
				{
					CMsgJoinResponse msg;
					if(!gnetVerify(pJF->m_SizeofResponse && msg.Import(pJF->m_Response, pJF->m_SizeofResponse)))
					{
						gnetError("JoinFailed :: Invalid response data!");
						return; 
					}

					nResponse = msg.m_ResponseCode;
					bCanQueue = msg.m_bCanQueue;
				}
				else
					gnetDebug1("\tJoinFailed :: Unknown response message!");
				
				gnetDebug1("\tJoinFailed :: Game Response: %s", NetworkUtils::GetResponseCodeAsString(nResponse));

				// record results for telemetry
				if(m_bJoiningTransitionViaMatchmaking)
				{
					m_TransitionMMResults.m_CandidatesToJoin[m_TransitionMMResults.m_nCandidateToJoinIndex].m_nTimeFinished = sysTimer::GetSystemMsTime();
					m_TransitionMMResults.m_CandidatesToJoin[m_TransitionMMResults.m_nCandidateToJoinIndex].m_Result = nResponse;
				}

				// logging for join queue
				if(nResponse == RESPONSE_DENY_GROUP_FULL)
				{
					gnetDebug1("\tJoinFailed :: RESPONSE_DENY_GROUP_FULL - CanQueue: %s, Invited: %s, Inviter: %s", bCanQueue ? "True" : "False", m_bJoinedTransitionViaInvite ? "True" : "False", NetworkUtils::LogGamerHandle(m_TransitionInviter));
					m_bCanQueue = bCanQueue && m_bJoinedTransitionViaInvite && m_TransitionInviter.IsValid();
					if(m_bCanQueue)
			        {
						m_hJoinQueueInviter = m_TransitionInviter;
						m_bJoinQueueAsSpectator = m_TransitionData.m_PlayerData.m_bIsSpectator;
						m_bJoinQueueConsumePrivate = CheckFlag(m_nTransitionJoinFlags, JoinFlags::JF_ConsumePrivate);
					}
				}
			}

			// generate script event
			GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_JOIN_FAILED, static_cast<int>(nResponse)));
		}
		break; 

	case SNET_EVENT_ADDING_GAMER:
		{
			// if we're leaving or if this is a swapped freemode session, ignore
			if(IsTransitionLeaving() || m_pTransition->GetConfig().m_Attrs.GetSessionPurpose() != SessionPurpose::SP_Activity)
				break;

			// add player (will be removed and re-initialised when we fully add)
			AddTransitionPlayer(pEvent->m_AddingGamer->m_GamerInfo, pEvent->m_AddingGamer->m_EndpointId, pEvent->m_AddingGamer->m_Data, pEvent->m_AddingGamer->m_SizeofData);

			// send migrate messages
			SendMigrateMessages(pEvent->m_AddingGamer->m_GamerInfo.GetPeerInfo().GetPeerId(), m_pTransition, m_TransitionState >= TS_ESTABLISHED);

			// update the active crews and reset beacon
			m_nMatchmakingBeacon[SessionType::ST_Transition] = 0;
			UpdateActivitySlots();
			UpdateCurrentUniqueCrews(SessionType::ST_Transition);
        }
		break;

	case SNET_EVENT_ADDED_GAMER:
		{
			// add player to voice
			NetworkInterface::GetVoice().PlayerHasJoined(pEvent->m_AddedGamer->m_GamerInfo, pEvent->m_AddedGamer->m_EndpointId);

			// if we're leaving or if this is a swapped freemode session, ignore
			if(IsTransitionLeaving() || m_pTransition->GetConfig().m_Attrs.GetSessionPurpose() != SessionPurpose::SP_Activity)
				break;
				
			// add player
			sTransitionPlayer* pPlayer = AddTransitionPlayer(pEvent->m_AddedGamer->m_GamerInfo, pEvent->m_AddedGamer->m_EndpointId, pEvent->m_AddedGamer->m_Data, pEvent->m_AddedGamer->m_SizeofData);
			if(!pPlayer)
				break;

			// if remote
			if(pEvent->m_AddedGamer->m_GamerInfo.IsRemote())
			{
				// player joined
				GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionMemberJoined(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle(), pEvent->m_AddedGamer->m_GamerInfo.GetDisplayName(), pPlayer->m_nCharacterRank));

				// send relevant telemetry
				SendAddedGamerTelemetry(true, m_pTransition->GetSessionId(), pEvent->m_AddedGamer, pPlayer->m_NatType);

				// remove from pending joiner list
				if(m_PendingJoiners[SessionType::ST_Transition].Remove(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle()))
				{
					gnetDebug2("AddedGamer: Removed Pending Joiner - %s, Num: %d", pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle().ToString(), m_PendingJoiners[SessionType::ST_Transition].m_Joiners.GetCount());
				}

				// add peer entry - peer complainer only runs for transitions into a job
				if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity)
					m_PeerComplainer[SessionType::ST_Transition].AddPeer(pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId() OUTPUT_ONLY(, pEvent->m_AddedGamer->m_GamerInfo.GetName()));

				// request parameters
				MsgRequestTransitionParameters msg;
				msg.Reset(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle());

				// send request and force send our own parameters
				ForceSendTransitionParameters(pEvent->m_AddedGamer->m_GamerInfo);
				m_pTransition->SendMsg(pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);

				if(!SPlayerCardManager::IsInstantiated())
					SPlayerCardManager::Instantiate();

				// add player card remote corona players
				{
					// this can happen for players joining.
					CPlayerCardManagerCurrentTypeHelper helper(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);

					BasePlayerCardDataManager* coronamgr = SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);
					if(gnetVerify(coronamgr))
					{
						if (!CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle()))
						{
							netPlayerCardDebug("[corona_sync] AddGamerHandle '%s'.", NetworkUtils::LogGamerHandle(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle()));
							static_cast< CScriptedCoronaPlayerCardDataManager* >(coronamgr)->AddGamerHandle(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle(), pEvent->m_AddedGamer->m_GamerInfo.GetName());
						}
					}
				}
            }
            
            // if we're not the host and this player is the host, request a radio sync
            if(!m_pTransition->IsHost())
            {
                u64 nHostPeerID = 0;
                bool bHaveHostPeer = m_pTransition->GetHostPeerId(&nHostPeerID);
                if(bHaveHostPeer && (nHostPeerID == pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId()) && NetworkInterface::GetActiveGamerInfo())
                {
                    MsgRadioStationSyncRequest msg;
					msg.Reset(m_pTransition->GetSessionId(), NetworkInterface::GetActiveGamerInfo()->GetGamerId());
                    m_pTransition->SendMsg(nHostPeerID, msg, NET_SEND_RELIABLE);
                    BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
                }
            }

			// if we've launched during this player joining, let them know
			if(m_pTransition->IsHost() && m_bLaunchingTransition)
				SendLaunchMessage(pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId());

#if !RSG_DURANGO
			// send migrate messages - this will have already happened in ADDING_GAMER on Xbox
			SendMigrateMessages(pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId(), m_pTransition, m_TransitionState >= TS_ESTABLISHED);
#endif

			// update the active crews and reset beacon
			m_nMatchmakingBeacon[SessionType::ST_Transition] = 0;
			UpdateCurrentUniqueCrews(SessionType::ST_Transition);
			UpdateActivitySlots();

			if(m_pTransition == GetCurrentSession())
				RemoveQueuedJoinRequest(pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle());
		}
		break;

	case SNET_EVENT_REMOVED_GAMER:
		{
			gnetDebug1("\tRemovedGamer :: Name: %s", pEvent->m_RemovedGamer->m_GamerInfo.GetName());
			
			// if we're leaving or if this is a swapped freemode session, ignore
			if(!IsTransitionLeaving() && m_pTransition->GetConfig().m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Activity)
				GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionMemberLeft(pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle(), pEvent->m_RemovedGamer->m_GamerInfo.GetName()));

			bool bIsSpectator = false;

            // check if we have an existing entry
            int nPlayers = m_TransitionPlayers.GetCount();
            for(int i = 0; i < nPlayers; i++)
            {
                if(m_TransitionPlayers[i].m_hGamer == pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle())
                {
                    gnetDebug2("\tRemovedGamer :: Removing Player Entry for %s", pEvent->m_RemovedGamer->m_GamerInfo.GetName());
					bIsSpectator = m_TransitionPlayers[i].m_bIsSpectator;
                    m_TransitionPlayers.Delete(i);
                    break;
                }
            }

			// remove the physical player entry if this is a transition to game (and we're not yet in the new session)
			if(m_bIsTransitionToGame && !m_bIsTransitionToGameInSession && CNetwork::IsNetworkOpen() && m_bTransitionToGameRemovePhysicalPlayers)
			{
				gnetDebug2("\tRemovedGamer :: Removing Physical Player Entry for %s", pEvent->m_RemovedGamer->m_GamerInfo.GetName());
				CNetwork::GetPlayerMgr().RemovePlayer(pEvent->m_RemovedGamer->m_GamerInfo);
			}

			// clear our the transition host tracking if this player was handling it
			if(!m_bIsTransitionToGame && m_bIsTransitionToGameFromLobby && m_hTransitionHostGamer == pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle())
			{
				gnetDebug1("\tRemovedGamer :: Was transition host. Resetting");
				m_hTransitionHostGamer.Clear();
			}

			// remove peer entry - peer complainer only runs for transitions into a job
			if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity && pEvent->m_RemovedGamer->m_GamerInfo.IsRemote() && m_PeerComplainer[SessionType::ST_Transition].HavePeer(pEvent->m_AddedGamer->m_GamerInfo.GetPeerInfo().GetPeerId()))
				m_PeerComplainer[SessionType::ST_Transition].RemovePeer(pEvent->m_RemovedGamer->m_GamerInfo.GetPeerInfo().GetPeerId());

            // remove player from voice
            NetworkInterface::GetVoice().PlayerHasLeft(pEvent->m_RemovedGamer->m_GamerInfo);

#if RSG_PC
			NetworkInterface::GetTextChat().PlayerHasLeft(pEvent->m_RemovedGamer->m_GamerInfo);
#endif

			// if someone leaves, clear out invite tracking
			if(m_TransitionInvitedPlayers.DidInvite(pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle()))
			{
				gnetDebug3("\tRemovedInvite :: Name: %s", pEvent->m_RemovedGamer->m_GamerInfo.GetName());
				m_TransitionInvitedPlayers.RemoveInvite(pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle());
			}

			// check if we've been left on our own (as the host, we'll pick this up in migrate as a client)
			if(!IsTransitionLeaving() && (m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && IsHost() && m_pTransition->GetGamerCount() == 1)
			{
				// write solo session telemetry
				gnetDebug1("\tRemovedGamer :: Sending solo telemetry - Reason: %s, Visibility: %s", GetSoloReasonAsString(SOLO_SESSION_PLAYERS_LEFT), GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Transition)));
				CNetworkTelemetry::EnteredSoloSession(m_pTransition->GetSessionId(), 
													  m_pTransition->GetSessionToken().GetValue(), 
													  m_Config[SessionType::ST_Transition].GetGameMode(), 
													  SOLO_SESSION_PLAYERS_LEFT, 
													  GetSessionVisibility(SessionType::ST_Transition),
													  SOLO_SOURCE_PLAYERS_LEFT,
													  0,
													  0,
													  0);
			}

            // update the active crews
            UpdateCurrentUniqueCrews(SessionType::ST_Transition);
			UpdateActivitySlots();

			// check queued requests
			CheckForQueuedJoinRequests();

			// remove from player card remote corona players
			if (SPlayerCardManager::IsInstantiated())
			{
				//This can happen for players joining.
				CPlayerCardManagerCurrentTypeHelper helper(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);

				BasePlayerCardDataManager* coronamgr = SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);
				if (gnetVerify(coronamgr))
				{
					netPlayerCardDebug("[corona_sync] RemoveGamerHandle '%s'.", NetworkUtils::LogGamerHandle(pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle()));
					static_cast< CScriptedCoronaPlayerCardDataManager* >(coronamgr)->RemoveGamerHandle(pEvent->m_RemovedGamer->m_GamerInfo.GetGamerHandle());
				}
			}
		}
		break;

	case SNET_EVENT_SESSION_MIGRATE_START:
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_MIGRATE_START));

#if RSG_DURANGO
		// if we've established, flag that we're leaving this MPSD session
		if(m_pTransition->IsEstablished())
		{
			WriteMultiplayerEndEventTransition(false OUTPUT_ONLY(, "MigrateStart"));
			m_bMigrationSentMultiplayerEndEventTransition = true; 
		}
		else
		{
			m_bMigrationSentMultiplayerEndEventTransition = false;
		}
#endif
		break;

	case SNET_EVENT_SESSION_MIGRATE_END:
		
		// record when this happened
		m_LastMigratedPosix[SessionType::ST_Transition] = rlGetPosixTime();

		if(!pEvent->m_SessionMigrateEnd->m_Succeeded)
		{
			gnetDebug1("\tMigrateEnd :: Failed");
            if(IsTransitionActive() && !IsTransitionLeaving() && !m_bIgnoreMigrationFailure[SessionType::ST_Transition])
            {
				// if we're launching, we can't leave - and our main session is in the process of being left
				// we'll need to bail so that we can recover from script
				if(IsLaunchingTransition())
				{
					gnetDebug1("\tMigrateEnd :: Failed but launching. Bailing.");
					DropTransition();
					CNetwork::Bail(sBailParameters(BAIL_TRANSITION_LAUNCH_FAILED, BAIL_CTX_TRANSITION_LAUNCH_MIGRATE_FAILED));
				}
				else
				{
					gnetDebug1("\tMigrateEnd :: Leaving transition session");
					LeaveTransition();
				}
            }
		}
		else
		{
#if RSG_DURANGO
			// if we've established, flag that we're starting a new MPSD session (will happen in the establish event otherwise)
			if(m_pTransition->IsEstablished() && m_bMigrationSentMultiplayerEndEventTransition)
			{
				WriteMultiplayerStartEventTransition(OUTPUT_ONLY("MigrateEnd"));
				m_bMigrationSentMultiplayerEndEventTransition = false;
			}
			if(m_bValidateGameSession)
				g_rlXbl.GetPartyManager()->ValidateGameSession(NetworkInterface::GetLocalGamerIndex());
#endif

#if !__NO_OUTPUT
			rlGamerInfo hGamer[1];
			m_pTransition->GetGamers(pEvent->m_SessionMigrateEnd->m_NewHost.GetPeerId(), hGamer, 1);
			gnetDebug1("\tMigrateEnd :: %s gamer %s is new host", pEvent->m_SessionMigrateEnd->m_NewHost.IsLocal() ? "Local" : "Remote", hGamer->GetName());
#endif

			// update the match config if we are the new host
			if(m_pTransition->IsHost())
			{
                // reset block requests - this needs to be called again if we want to turn this on
				m_bBlockTransitionJoinRequests = false;

				// we are host - peer complainer only runs for transitions into a job
				if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity)
					MigrateOrInitPeerComplainer(&m_PeerComplainer[SessionType::ST_Transition], m_pTransition, NET_INVALID_ENDPOINT_ID);
			}
            else
            {
                // if we haven't managed to sync radio yet
                if(!m_bTransitionSyncedRadio)
                {
                    if(NetworkInterface::GetActiveGamerInfo())
                    {
                        MsgRadioStationSyncRequest msg;
						msg.Reset(m_pTransition->GetSessionId(), NetworkInterface::GetActiveGamerInfo()->GetGamerId());
                        m_pTransition->SendMsg(pEvent->m_SessionMigrateEnd->m_NewHost.GetPeerId(), msg, NET_SEND_RELIABLE);
                        BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
                    }
                }

				// peer complainer only runs for transitions into a job
				if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !m_bIsLeavingLaunchedActivity)
				{
					// migrate to new host
					EndpointId hostEndpointId;
					m_pTransition->GetHostEndpointId(&hostEndpointId);
					MigrateOrInitPeerComplainer(&m_PeerComplainer[SessionType::ST_Transition], m_pTransition, hostEndpointId);
				}
            }

			// we need to update the transition session info since the token will change when
			// the session has a new owner
			if(CNetwork::IsNetworkOpen())
				NetworkInterface::GetLocalPlayer()->SetTransitionSessionInfo(m_pTransition->GetSessionInfo());
		}

		// track solo sessions
		if(m_pTransition->GetGamerCount() <= 1)
		{
			eSoloSessionReason nReason = SOLO_SESSION_MIGRATE;
			if((m_pTransition->GetChannelId() == NETWORK_SESSION_ACTIVITY_CHANNEL_ID) && !IsTransitionEstablished())
				nReason = m_bIsTransitionMatchmaking ? SOLO_SESSION_MIGRATE_WHEN_JOINING_VIA_MM : SOLO_SESSION_MIGRATE_WHEN_JOINING_DIRECT;

			// write solo session telemetry
			gnetDebug1("\tMigrateEnd :: Sending solo telemetry - Reason: %s, Visibility: %s", GetSoloReasonAsString(nReason), GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Transition)));
			CNetworkTelemetry::EnteredSoloSession(m_pTransition->GetSessionId(), 
												  m_pTransition->GetSessionToken().GetValue(), 
												  m_Config[SessionType::ST_Transition].GetGameMode(), 
												  nReason, 
												  GetSessionVisibility(SessionType::ST_Transition),
												  SOLO_SOURCE_MIGRATE,
												  0,
												  0,
												  0);
		}

		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_MIGRATE_END));
		break;

	case SNET_EVENT_SESSION_ATTRS_CHANGED:
		{
            if(!m_pTransition->IsHost())
            {
				OUTPUT_ONLY(m_Config[SessionType::ST_Transition].LogDifferences(m_pTransition->GetConfig(), "\tAttributesChanged"));

			    // apply the current config
			    player_schema::MatchingAttributes mmAttrs;
			    m_Config[SessionType::ST_Transition].Reset(mmAttrs, m_pTransition->GetConfig().m_Attrs.GetGameMode(), m_pTransition->GetConfig().m_Attrs.GetSessionPurpose(), m_pTransition->GetConfig().m_MaxPublicSlots + m_pTransition->GetConfig().m_MaxPrivateSlots, m_pTransition->GetConfig().m_MaxPrivateSlots);
			    m_Config[SessionType::ST_Transition].Apply(m_pTransition->GetConfig());
            }
		}
		break;

	case SNET_EVENT_SESSION_ESTABLISHED:
		GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_STARTED));
		DURANGO_ONLY(WriteMultiplayerStartEventTransition(OUTPUT_ONLY("OnSessionEventTransition - SNET_EVENT_SESSION_ESTABLISHED")));
		break; 

	case SNET_EVENT_SESSION_DESTROYED:
		break;

	case SNET_EVENT_SESSION_BOT_JOINED:
		break;

	case SNET_EVENT_REMOVED_FROM_SESSION:
	{
		SendKickTelemetry(
			m_pTransition,
			KickSource::Source_RemovedFromSession,
			pEvent->m_RemovedFromSession->m_NumGamers,
			SessionType::ST_Transition,
			m_TransitionState == TS_ESTABLISHED ? m_TransitionStateTime : 0,
			static_cast<u32>(rlGetPosixTime() - m_LastMigratedPosix[SessionType::ST_Transition]),
			0,
			rlGamerHandle());
	}
	break;

	default:
		break;
	}
}

void CNetworkSession::OnGetGamerDataTransition(snSession*, const rlGamerInfo& hInfo, snGetGamerData* pData) const
{
    // find this player
    int nPlayers = m_TransitionPlayers.GetCount();
    for(int i = 0; i < nPlayers; i++)
    {
        if(m_TransitionPlayers[i].m_hGamer == hInfo.GetGamerHandle())
        {
			// use a player data message so that this works across transitions
			CNetGamePlayerDataMsg tPlayerData;
			BuildTransitionPlayerData(&tPlayerData, &m_TransitionPlayers[i]);

			// export message
			if(!tPlayerData.Export(pData, sizeof(pData->m_Data), &pData->m_SizeofData))
				pData->m_SizeofData = 0;

            // found our player, exit
			break;
        }
    }
}

CNetworkSession::sTransitionPlayer* CNetworkSession::AddTransitionPlayer(const rlGamerInfo& hInfo, const EndpointId OUTPUT_ONLY(endpointId), const void* pData, const unsigned nSizeofData)
{
	// check if we have an existing entry for this player
	int nPlayers = m_TransitionPlayers.GetCount();
	for(int i = 0; i < nPlayers; i++) if(m_TransitionPlayers[i].m_hGamer == hInfo.GetGamerHandle())
	{
		// just wipe the player out, we'll be fully initialising again
		m_TransitionPlayers.Delete(i);
		break;
	}

	// if this gamer is remote and this is not a join request
	if(hInfo.IsRemote())
	{
		// setup player data
		sTransitionPlayer tPlayer; 
		tPlayer.m_hGamer = hInfo.GetGamerHandle();

		// check that we got some data
		bool bValidData = false;

		// grab the incoming data
		unsigned nMessageID;
		if(netMessage::GetId(&nMessageID, pData, nSizeofData))
		{
			// client to host
			CNetGamePlayerDataMsg tPlayerData;
			if(CNetGamePlayerDataMsg::MSG_ID() == nMessageID)
			{
				// we receive this when we join a launched transition
				if(tPlayerData.Import(pData, nSizeofData))
					bValidData = true;
			}
			else if(CMsgJoinRequest::MSG_ID() == nMessageID)
			{
				// we receive this as host when a client joins
				CMsgJoinRequest tJoinRequest;
				if(tJoinRequest.Import(pData, nSizeofData))
				{
					tPlayerData = tJoinRequest.m_PlayerData;
					bValidData = true;
				}
			}

			// we receive this when getting messages from existing session players
			if(bValidData)
			{
				tPlayer.m_bIsSpectator = (tPlayerData.m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR) != 0;
				tPlayer.m_nClanID = tPlayerData.m_ClanId;
				tPlayer.m_nCharacterRank = tPlayerData.m_CharacterRank;
				tPlayer.m_nELO = tPlayerData.m_ELO;
				tPlayer.m_NatType = tPlayerData.m_NatType;
				tPlayer.m_bIsRockstarDev = (tPlayerData.m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_ROCKSTAR_DEV) != 0;
				tPlayer.m_bIsAsync = (tPlayerData.m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_IS_ASYNC) != 0;
			}
		}

		// acknowledge invalid data
#if !__NO_OUTPUT
		if(!bValidData)
			gnetDebug1("\tAddTransitionPlayer :: Invalid Transition Gamer Data");
		else
			gnetDebug1("\tAddTransitionPlayer :: Remote - Name: %s, Handle: %s, Addr: " NET_ADDR_FMT ", Crew: %" I64FMT "d, Spectator: %s, Async: %s", hInfo.GetName(), NetworkUtils::LogGamerHandle(hInfo.GetGamerHandle()), NET_ADDR_FOR_PRINTF(CNetwork::GetConnectionManager().GetAddress(endpointId)), tPlayer.m_nClanID, tPlayer.m_bIsSpectator ? "True" : "False", tPlayer.m_bIsAsync ? "True" : "False");
#endif

		// add the player
		sTransitionPlayer& tNewPlayer = m_TransitionPlayers.Append();
		tNewPlayer = tPlayer;
		return &tNewPlayer;
	}
	else // add local data
	{
		gnetDebug1("\tAddTransitionPlayer :: Local - Name: %s, Handle: %s, Crew: %" I64FMT "d, Spectator: %s, Async: %s", hInfo.GetName(), NetworkUtils::LogGamerHandle(hInfo.GetGamerHandle()), m_TransitionData.m_PlayerData.m_nClanID, m_TransitionData.m_PlayerData.m_bIsSpectator ? "True" : "False", m_TransitionData.m_PlayerData.m_bIsAsync ? "True" : "False");
		sTransitionPlayer& tNewPlayer = m_TransitionPlayers.Append();
		tNewPlayer = m_TransitionData.m_PlayerData;
		return &tNewPlayer;
	}
}

void CNetworkSession::InitPeerComplainer(netPeerComplainer* pPeerComplainer, snSession* pSession)
{
	gnetAssertf(m_IsInitialised, "InitPeerComplainer :: Network session is not initialised!");
	gnetDebug1("InitPeerComplainer :: 0x%016" I64FMT "x", pSession->GetSessionInfo().GetToken().m_Value);

    EndpointId hostEndpointId = NET_INVALID_ENDPOINT_ID;
    if(!pSession->IsHost())
    {
		if(!pSession->GetHostEndpointId(&hostEndpointId))
		{
			// can happen if the host ejects during the join process
			gnetError("InitPeerComplainer :: Failed to get the host endpointId!");
			return;
		}
    }

    u64 nMyPeerID = 0;
    if(!gnetVerify(rlPeerInfo::GetLocalPeerId(&nMyPeerID)))
	{
		gnetError("InitPeerComplainer :: Failed to retrieve local peer ID!");
		return; 
	}

	// default to normal policies
	netPeerComplainer::Policies tPolcies;
	GetPeerComplainerPolicies(&tPolcies, pSession, POLICY_NORMAL);

	// initialise (shutdown first to kill any prior state)
    pPeerComplainer->Shutdown();
    pPeerComplainer->Init(nMyPeerID,
						  pSession->GetCxnMgr(),
						  pSession->IsHost() ? NET_INVALID_ENDPOINT_ID : hostEndpointId,
						  m_PeerComplainerDelegate,
						  tPolcies,
						  pSession->GetSessionInfo().GetToken().m_Value);

	// add anyone already in this session
    rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
    const unsigned nGamers = pSession->GetGamers(aGamers, COUNTOF(aGamers));

    for(unsigned i = 0; i < nGamers; i++)
    {
        const u64 nPeerID = aGamers[i].GetPeerInfo().GetPeerId();
        if(!pPeerComplainer->HavePeer(nPeerID) && aGamers[i].IsRemote())
            pPeerComplainer->AddPeer(nPeerID OUTPUT_ONLY(, aGamers[i].GetName()));
    }
}

void CNetworkSession::MigrateOrInitPeerComplainer(netPeerComplainer* pPeerComplainer, snSession* pSession, const EndpointId serverEndpointId)
{
	if(!pPeerComplainer)
		return;

	if(!pSession)
		return;

	gnetDebug1("MigrateOrInitPeerComplainer :: 0x%016" I64FMT "x", pSession->GetSessionInfo().GetToken().m_Value);

	if(pPeerComplainer->IsInitialized())
	{
        gnetDebug1("MigrateOrInitPeerComplainer :: Migrating peer complainer. ");
		pPeerComplainer->MigrateServer(serverEndpointId, pSession->GetSessionInfo().GetToken().m_Value);
	}
	else
	{
        gnetDebug1("MigrateOrInitPeerComplainer :: Initialising peer complainer");
		InitPeerComplainer(pPeerComplainer, pSession);
	}
}

void CNetworkSession::OnSessionEvent(snSession* pSession, const snEvent* pEvent)
{
	// log event
	gnetDebug1("OnSessionEvent :: Game:0x%016" I64FMT "x - [%08u:%u] %s", pSession->Exists() ? pSession->GetSessionInfo().GetToken().m_Value : 0, CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), pEvent->GetAutoIdNameFromId(pEvent->GetId()));

	// not for game session...
	if(pSession != m_pSession)
	{
		gnetDebug1("OnSessionEvent :: Game - For different session! Game: %s", (pSession == m_pTransition) ? "True" : "False");
		return;
	}

	switch(pEvent->GetId())
	{
	case SNET_EVENT_SESSION_HOSTED:
		{
			// complete transition to game
		    OnSessionEntered(true);

			if(!IsSessionLeaving())
				InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession);

			// apply the game-mode
			CNetwork::SetNetworkGameMode(m_Config[SessionType::ST_Physical].GetGameMode());
		}
		break;

	case SNET_EVENT_SESSION_JOINED:
		ProcessSessionJoinedEvent(pEvent->m_SessionJoined);
		break;

	case SNET_EVENT_JOIN_FAILED:
		ProcessSessionJoinFailedEvent(pEvent->m_JoinFailed);
		break;

	case SNET_EVENT_ADDING_GAMER:
		ProcessAddingGamerEvent(pEvent->m_AddingGamer);
		break;

	case SNET_EVENT_ADDED_GAMER:
		ProcessAddedGamerEvent(pEvent->m_AddedGamer);
		break;
#if RSG_DURANGO
	case SNET_EVENT_DISPLAY_NAME_RETRIEVED:
		ProcessDisplayNameEvent(pEvent->m_SessionDisplayNameRetrieved);
		break;
#endif
	case SNET_EVENT_REMOVED_GAMER:
		gnetDebug1("\tRemovedGamer :: Name: %s, Reason: %d", pEvent->m_RemovedGamer->m_GamerInfo.GetName(), pEvent->m_RemovedGamer->m_RemoveReason);
		ProcessRemovedGamerEvent(pEvent->m_RemovedGamer);
		break;

	case SNET_EVENT_SESSION_ESTABLISHED:
		DURANGO_ONLY(WriteMultiplayerStartEventSession(OUTPUT_ONLY("OnSessionEvent - SNET_EVENT_SESSION_ESTABLISHED")));
		RL_NP_PLAYTOGETHER_ONLY(InvitePlayTogetherGroup());
		break;

	case SNET_EVENT_SESSION_MIGRATE_START:
		ProcessSessionMigrateStartEvent(pEvent->m_SessionMigrateStart);
		break;

	case SNET_EVENT_SESSION_MIGRATE_END:
		ProcessSessionMigrateEndEvent(pEvent->m_SessionMigrateEnd);
		break;

	case SNET_EVENT_SESSION_ATTRS_CHANGED:
		{
            if(!m_pSession->IsHost() && IsSessionActive() && !IsSessionLeaving())
            {
				OUTPUT_ONLY(m_Config[SessionType::ST_Physical].LogDifferences(m_pSession->GetConfig(), "\tAttributesChanged"));

                // apply the current config
				player_schema::MatchingAttributes mmAttrs;
				m_Config[SessionType::ST_Physical].Reset(mmAttrs, m_pSession->GetConfig().m_Attrs.GetGameMode(), m_pSession->GetConfig().m_Attrs.GetSessionPurpose(), m_pSession->GetConfig().m_MaxPublicSlots + m_pSession->GetConfig().m_MaxPrivateSlots, m_pSession->GetConfig().m_MaxPrivateSlots);
			    m_Config[SessionType::ST_Physical].Apply(m_pSession->GetConfig());

                // apply the game-mode
                CNetwork::SetNetworkGameMode(m_Config[SessionType::ST_Physical].GetGameMode());
            }
		}
		break;

	case SNET_EVENT_SESSION_DESTROYED:
		break;

	case SNET_EVENT_REMOVED_FROM_SESSION:
		{
			SendKickTelemetry(
				m_pSession, 
				KickSource::Source_RemovedFromSession, 
				pEvent->m_RemovedFromSession->m_NumGamers, 
				SessionType::ST_Physical,
				m_TimeSessionStarted,
				static_cast<u32>(rlGetPosixTime() - m_LastMigratedPosix[SessionType::ST_Physical]),
				0,
				rlGamerHandle());

			if(m_bKickWhenRemovedFromSession)
			{
				gnetDebug1("\tKicking from session");
				HandleLocalPlayerBeingKicked(KickReason::KR_PeerComplaints);
			}
			else
			{
				// increase data mine kick count (also ticked in HandleLocalPlayerBeingKicked, so handled separately otherwise)
				m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesKicked);
			}

			// push an event to script
			GetEventScriptNetworkGroup()->Add(CEventNetworkRemovedFromSessionDueToComplaints());
		}
		break;

	case SNET_EVENT_REMOVED_DUE_TO_STALL:
		{
			// push an event to script
			GetEventScriptNetworkGroup()->Add(CEventNetworkRemovedFromSessionDueToStall());
		}
		break;
	}
}

CNetGamePlayer* CNetworkSession::GetPlayerFromConnectionID(int nCxnID, unsigned nChannelID)
{
	switch(nChannelID)
	{
	case NETWORK_GAME_CHANNEL_ID:
        return NetworkInterface::GetPlayerFromConnectionId(nCxnID);
	case NETWORK_SESSION_GAME_CHANNEL_ID:
    case NETWORK_SESSION_ACTIVITY_CHANNEL_ID:
        {
			u64 nPeerID = m_pSession->GetPeerIdByCxn(nCxnID);
			return NetworkInterface::GetPlayerFromPeerId(nPeerID);
		}
	default:
		gnetAssertf(0, "GetPlayerFromConnectionID :: Cannot retrieve player details on this channel!");
		return NULL;
	}	
}

CNetGamePlayer* CNetworkSession::GetPlayerFromEndpointId(const EndpointId endpointId, unsigned nChannelID)
{
	switch(nChannelID)
	{
	case NETWORK_GAME_CHANNEL_ID:
        return NetworkInterface::GetPlayerFromEndpointId(endpointId);
	case NETWORK_SESSION_GAME_CHANNEL_ID:
    case NETWORK_SESSION_ACTIVITY_CHANNEL_ID:
        {
			u64 nPeerID = m_pSession->GetPeerIdByEndpointId(endpointId);
			return NetworkInterface::GetPlayerFromPeerId(nPeerID);
		}
	default:
		gnetAssertf(0, "GetPlayerFromEndpointId :: Cannot retrieve player details on this channel!");
		return NULL;
	}	
}

void CNetworkSession::OnConnectionEvent(netConnectionManager* OUTPUT_ONLY(pCxnMgr), const netEvent* pEvent)
{
	switch(pEvent->GetId())
	{
	case NET_EVENT_CONNECTION_REQUESTED:
#if !__NO_OUTPUT
			// we only care about this when hosting
		if((m_pSession->Exists() && (m_pSession->GetChannelId() == pEvent->m_ChannelId) && (m_pSession->IsHost())) || (m_pTransition->Exists() && (m_pTransition->GetChannelId() == pEvent->m_ChannelId) && (m_pTransition->IsHost())))
			gnetDebug2("CxnEvent :: Requested [%08u:%u] :: Channel: %s, Addr: %s", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId), pEvent->m_CxnRequested->m_Sender.ToString());
#endif
	break;

	case NET_EVENT_CONNECTION_ESTABLISHED:
#if !__NO_OUTPUT
		gnetDebug1("CxnEvent :: Established [%08u:%u] :: Channel: %s, Addr: %s, ID: 0x%08x", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId), pCxnMgr->GetAddress(pEvent->GetEndpointId()).ToString(), pEvent->m_CxnId);
#endif
		break;

		// there was an error on the connection.
	case NET_EVENT_CONNECTION_ERROR:
    case NET_EVENT_CONNECTION_CLOSED:
		{

#if !__NO_OUTPUT
			static const char* szCxnErr[] =
			{
				"TimedOut",		// CXNERR_TIMED_OUT
				"SendError",	// CXNERR_SEND_ERROR
				"Terminated",	// CXNERR_TERMINATED
			};

			if(pEvent->GetId() == NET_EVENT_CONNECTION_ERROR)
				gnetDebug1("CxnEvent :: Error [%08u:%u] :: Channel: %s, ID: 0x%08x, Reason: %s", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId), pEvent->m_CxnId, szCxnErr[pEvent->m_CxnError->m_Code]);
			else if(pEvent->GetId() == NET_EVENT_CONNECTION_CLOSED)
				gnetDebug1("CxnEvent :: Closed [%08d:%d] :: Channel: %s, Addr: %s, ID: 0x%08x", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId), pCxnMgr->GetAddress(pEvent->GetEndpointId()).ToString(), pEvent->m_CxnId);
#endif

			// disconnecting peer
			u64 nDisconnectingPeerID = RL_INVALID_PEER_ID;
			
			// process errors
			bool bProcessErrors = true; 
			OUTPUT_ONLY(bool bLogComplainerRejections = false); 

			// if we're currently transitioning, ignore connection errors - pick these up when we're done
			const bool bInTransition = IsTransitioning();
			if(bInTransition)
			{
				gnetDebug3("\tCxnEvent :: Transitioning. Ignoring error");
				bProcessErrors = false; 
			}

			snSession* pSession = NULL;
			if(pEvent->m_ChannelId == NETWORK_SESSION_GAME_CHANNEL_ID || pEvent->m_ChannelId == NETWORK_SESSION_ACTIVITY_CHANNEL_ID)
			{
				// get the correct session
				if(m_pSession->GetPeerIdByCxn(pEvent->m_CxnId) != RL_INVALID_PEER_ID)
					pSession = m_pSession;
				else if(m_pTransition->GetPeerIdByCxn(pEvent->m_CxnId) != RL_INVALID_PEER_ID)
					pSession = m_pTransition;

				if(pSession)
				{
					nDisconnectingPeerID = pSession->GetPeerIdByCxn(pEvent->m_CxnId);

					// grab the gamer information for this player 
					if(nDisconnectingPeerID != RL_INVALID_PEER_ID)
					{
						rlGamerInfo hGamer[1];
						unsigned nFound = pSession->GetGamers(nDisconnectingPeerID, hGamer, 1);
						if(nFound == 1)
						{
							gnetDebug1("\tCxnEvent :: %s Session [%u] :: Gamer: %s", pSession == m_pSession ? "Main" : "Transition", pEvent->m_ChannelId, hGamer[0].GetName());
							
							if(pSession == m_pSession)
							{
								// if we're on the main session, check if we have a player associated with this
								CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer->GetGamerHandle());
								if(pPlayer && !pPlayer->HasSentTimeoutScriptEvent() && ((pEvent->GetId() == NET_EVENT_CONNECTION_ERROR) && (pEvent->m_CxnError->m_Code == netEventConnectionError::CXNERR_TIMED_OUT)))
								{
									if(pPlayer->HasValidPhysicalPlayerIndex())
									{
										// push an event to script
										gnetDebug2("\tCxnEvent :: Adding TimeOut script event for Player %u", pPlayer->GetPhysicalPlayerIndex());
										pPlayer->SetSentTimeoutScriptEvent(true);
										GetEventScriptNetworkGroup()->Add(CEventNetworkConnectionTimeout(pPlayer->GetPhysicalPlayerIndex()));
									}
									else
									{
										gnetDebug2("\tCxnEvent :: Skipping TimeOut script event for player with invalid player index");
									}
								}
							}
						}
					}
					
					// always log session rejections if not host
					OUTPUT_ONLY(bLogComplainerRejections = !pSession->IsHost());

					pSession->MarkAsNotMigrating(nDisconnectingPeerID);
					if(!pSession->Exists())
					{
						gnetDebug3("\tCxnEvent :: Session does not exist. Ignoring error");
						bProcessErrors = false; 
					}
				}
			}
			else if(pEvent->m_ChannelId == NETWORK_GAME_CHANNEL_ID)
			{
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromConnectionId(pEvent->m_CxnId);
				if(pPlayer && pPlayer->IsValid())
				{
					gnetDebug1("\tCxnEvent :: Game Channel :: Gamer: %s, WasDisconnected: %s", pPlayer->GetLogName(), pPlayer->IsLocallyDisconnected() ? "True" : "False");

					// grab the session peer ID
					nDisconnectingPeerID = pPlayer->GetRlPeerId();

					// physical players always exist on the main session
					pSession = m_pSession;

					// do not consider this player for migration - it's expected that this connection will fall
					// over shortly. If the session connection remains active, this will delay and possibly split
					// the local player. 
					if(bProcessErrors)
					{
#if !__NO_OUTPUT
						NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "LOCALLY_DISCONNECTED", pPlayer->GetLogName());
						if(CTheScripts::GetScriptHandlerMgr().GetLog())
							NetworkLogUtils::WriteLogEvent(*CTheScripts::GetScriptHandlerMgr().GetLog(), "LOCALLY_DISCONNECTED", pPlayer->GetLogName());

						// only log rejections when we first encounter a time out
						if(!pPlayer->IsLocallyDisconnected())
							bLogComplainerRejections = true; 
#endif

						if(!pPlayer->HasSentTimeoutScriptEvent() && ((pEvent->GetId() == NET_EVENT_CONNECTION_ERROR) && (pEvent->m_CxnError->m_Code == netEventConnectionError::CXNERR_TIMED_OUT)))
						{
							if(pPlayer->HasValidPhysicalPlayerIndex())
							{
								// push an event to script
								gnetDebug2("\tCxnEvent :: Adding TimeOut script event for Player %u", pPlayer->GetPhysicalPlayerIndex());
								pPlayer->SetSentTimeoutScriptEvent(true);
								GetEventScriptNetworkGroup()->Add(CEventNetworkConnectionTimeout(pPlayer->GetPhysicalPlayerIndex()));
							}
							else
							{
								gnetDebug2("\tCxnEvent :: Skipping TimeOut script event for player with invalid player index");
							}
						}

						pPlayer->SetLocallyDisconnected(true);

						// remove this peer from migration consideration
						m_pSession->MarkAsNotMigrating(nDisconnectingPeerID);
					}
					else if(bInTransition && m_pSession->GetPeerCxn(pPlayer->GetRlPeerId()) == NET_INVALID_CXN_ID)
						pPlayer->SetDisconnectedDuringTransition(true);
				}
				else
					bProcessErrors = false;
			}

			// check if a complaint should be registered
			if(bProcessErrors && pSession)
			{
				const SessionType sessionType = (pSession == m_pSession) ? SessionType::ST_Physical : SessionType::ST_Transition;
				TryPeerComplaint(nDisconnectingPeerID, &m_PeerComplainer[sessionType], pSession, sessionType OUTPUT_ONLY(, bLogComplainerRejections));
			}
			
			// channel specific
			if(pEvent->m_ChannelId == NETWORK_GAME_CHANNEL_ID)
            {
				if(bProcessErrors)
				{
					CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromConnectionId(pEvent->m_CxnId);
					if(pPlayer && pPlayer->IsValid())
					{
						// if we're the host, kick this player
						if(m_pSession->IsHost())
						{
							// if not migrating - we'll resolve this at the end in a cleaner fashion in that case
							if(!m_bIsMigrating)
							{
								// ...only if they are in session, might have been handled by session.cpp on a connection error on SNET channel
								if(m_pSession->IsMember(pPlayer->GetGamerInfo().GetGamerHandle()))
									KickPlayer(pPlayer, KickReason::KR_ConnectionError, 1, NetworkInterface::GetLocalGamerHandle());
							}						
						}
						else
						{
							// grab the session host ID
							u64 nHostPeerID; 
							m_pSession->GetHostPeerId(&nHostPeerID);

							// if we're the client, migrate if the disconnecting peer is the host
							// check that we're not already migrating - we might have disconnected on the session channel
							if(nHostPeerID == nDisconnectingPeerID)
							{
								gnetDebug1("\tCxnEvent :: Lost connection to host - DisconnectMsg: %s", m_bSendHostDisconnectMsg ? "True" : "False");
								
								// send a message on the session channel to the host to invoke a kick on the remote side
								if(m_bSendHostDisconnectMsg && pSession->CanSendToPeer(nDisconnectingPeerID))
								{
									MsgLostConnectionToHost msg;
									msg.Reset(m_pSession->GetSessionId(), NetworkInterface::GetLocalGamerHandle());
									pSession->SendMsg(nHostPeerID, msg, NET_SEND_RELIABLE);
								}
								
								// kick off migration
								if(!m_pSession->IsMigrating() && ShouldMigrate())
								{
									gnetDebug1("\tCxnEvent :: Starting migration");
									m_pSession->Migrate();
								}
							}
						}
					}
				}
			}
			else if(pEvent->m_ChannelId == m_pSession->GetChannelId())
			{
				// grab the session host ID
				u64 nHostPeerID; 
				bool bHasHost = m_pSession->GetHostPeerId(&nHostPeerID);

				// if we are in the process of getting into a session...
				if(m_pSession->Exists() && (m_SessionState > SS_IDLE) && (m_SessionState < SS_ESTABLISHING))
				{
					// if this is the host, or we have no host, and we aren't a full member of the session yet...
					const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
					if(((nHostPeerID == nDisconnectingPeerID) || !bHasHost) && !m_pSession->IsMember(pInfo->GetGamerId()))
					{
						gnetDebug1("\tCxnEvent :: Host disconnected and we haven't fully joined yet!");
						m_bJoinIncomplete = true; 
					}
				}
			}
		}
		break;

		// a frame of data was received.
	case NET_EVENT_FRAME_RECEIVED:
		{
			const netEventFrameReceived* pFrame = pEvent->m_FrameReceived;

			unsigned messageId;
			if(!netMessage::GetId(&messageId, pFrame->m_Payload, pFrame->m_SizeofPayload))
				break;

			// cache endpoint 
			const EndpointId endpointId = pFrame->GetEndpointId();

			// work out session from the channelId
			const snSession* pSession = (pFrame->m_ChannelId == m_pSession->GetChannelId()) ? m_pSession : (pFrame->m_ChannelId == m_pTransition->GetChannelId() ? m_pTransition : nullptr);

			// some helper macros for readability

#define MSG_LOG_RECEIVED(loggingCmd) \
			/* log name / details */ \
			loggingCmd("OnCxnEvent :: Received %s - From: %s, ChannelId: %u, Endpoint: %u, Session: 0x%016" I64FMT "x:0x%016" I64FMT "x", \
				netMessage::GetName(messageId), \
				Util_GetGamerLogString(pSession, endpointId), \
				pFrame->m_ChannelId, \
				endpointId, \
				pSession ? pSession->GetSessionId() : 0, \
				pSession ? pSession->GetSessionToken().GetValue() : 0); \
						
#define MSG_CHECK_IMPORT(msg) \
			/* verify message imports correctly	*/ \
			if (!msg.Import(pFrame->m_Payload, pFrame->m_SizeofPayload)) \
			{ \
				gnetError("OnCxnEvent :: Failed to import %s", msg.GetMsgName()); \
				break; \
			}

#define MSG_CHECK_LOCAL_IS_HOST() \
			/* check that the local gamer is the host */ \
			if(!pSession->IsHost()) \
			{ \
				gnetWarning("\tLocal is not host"); \
				break; \
			}

#define MSG_CHECK_SENT_FROM_HOST() \
			/* check that this came from the host */ \
			if((pSession->IsHost() || !pSession->IsHostEndpointId(endpointId)) || pSession->IsMigrating()) \
			{ \
				gnetWarning("\tNot sent from host (LocalHost: %s, HostEndpoint: %s, Migrating: %s)", pSession->IsHost() ? "True" : "False", pSession->IsHostEndpointId(endpointId) ? "True" : "False", pSession->IsMigrating() ? "True" : "False"); \
				break; \
			}

#define MSG_CHECK_IS_PHYSICAL_SESSION() \
			/* only valid for the main / gameplay session */ \
			if(pSession != m_pSession) \
			{ \
				gnetWarning("\tNot received on physical session!"); \
				break; \
			}

#define MSG_CHECK_IS_FREEROAM_SESSION() \
			/* only valid for freeroam session */ \
			if(pSession->GetConfig().m_Attrs.GetSessionPurpose() != SessionPurpose::SP_Freeroam) \
			{ \
				gnetWarning("\tNot received on freeroam session!"); \
				break; \
			}

#define MSG_CHECK_IS_PHYSICAL_SESSION_WITH_MSG_ID(msg) \
			/* only valid for the main / gameplay session */ \
			if((pSession != m_pSession) || (m_pSession->GetSessionId() != msg.m_SessionId)) \
			{ \
				gnetWarning("\tNot received on physical session! SessionId: 0x%016" I64FMT "x", msg.m_SessionId); \
				break; \
			}

#define MSG_CHECK_IS_TRANSITION_SESSION() \
			/* only valid for the transition session */ \
			if(pSession != m_pTransition) \
			{ \
				gnetWarning("\tNot received on transition session!"); \
				break; \
			}

#define MSG_CHECK_IS_ACTIVITY_SESSION() \
			/* only valid for the main / gameplay session */ \
			if(pSession->GetConfig().m_Attrs.GetSessionPurpose() != SessionPurpose::SP_Activity) \
			{ \
				gnetWarning("\tNot received on activity session!"); \
				break; \
			}

#define MSG_CHECK_IS_TRANSITION_SESSION_WITH_MSG_ID(msg) \
			/* only valid for the transition session */ \
			if((pSession != m_pTransition) || (m_pTransition->GetSessionId() != msg.m_SessionId)) \
			{ \
				gnetWarning("\tNot received on transition session! SessionId: 0x%016" I64FMT "x", msg.m_SessionId); \
				break; \
			}

			// specific message processing
			if(messageId == MsgBlacklist::MSG_ID())
			{
				// message checks
				MsgBlacklist msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_SENT_FROM_HOST();
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_PHYSICAL_SESSION_WITH_MSG_ID(msg);

                // logging
                gnetDebug2("\tBlacklistedGamer: %s", Util_GetGamerLogString(pSession, msg.m_hGamer));

				// blacklist gamer
				if(msg.m_hGamer.IsValid())
					m_BlacklistedGamers.BlacklistGamer(msg.m_hGamer, static_cast<eBlacklistReason>(msg.m_nReason));
			}
			else if(messageId == MsgLostConnectionToHost::MSG_ID())
			{
				// message checks
				MsgLostConnectionToHost msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_PHYSICAL_SESSION_WITH_MSG_ID(msg);
				MSG_CHECK_LOCAL_IS_HOST();

				// get the player associated with this handle
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(msg.m_hGamer);

				// logging (for consistency, use the logging function)
				gnetDebug2("\tFrom: %s", Util_GetGamerLogString(pSession, pPlayer));

				if(pPlayer)
				{
					// the remote player has indicated that they lost connection to the local host
					// kick the player to cleanly removed them from the game
					KickPlayer(pPlayer, KickReason::KR_ConnectionError, 1, NetworkInterface::GetLocalGamerHandle());
				}
			}
			else if(messageId == MsgKickPlayer::MSG_ID())
			{
				// message checks
				MsgKickPlayer msg;
				MSG_LOG_RECEIVED(gnetDebug1);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_PHYSICAL_SESSION_WITH_MSG_ID(msg);

				// check if it's a kick for the local player
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(msg.m_GamerId);

                // logging
				gnetDebug1("\tFor: %s, NumComplaints: %u, Complainer: %s", 
					Util_GetGamerLogString(pSession, pPlayer),
					msg.m_NumberOfComplaints,
					Util_GetGamerLogString(pSession, msg.m_hComplainer));

				// validate that the player is local
				if(pPlayer && pPlayer->IsLocal())
				{
					if(m_SessionState >= SS_ESTABLISHING && m_SessionState < SS_LEAVING)
					{
						if(IsKickDueToConnectivity(static_cast<KickReason>(msg.m_Reason)))
						{
							SendKickTelemetry(
								m_pSession,
								KickSource::Source_KickMessage,
								m_pSession->GetGamerCount(),
								m_SessionState,
								m_TimeSessionStarted,
								static_cast<u32>(rlGetPosixTime() - m_LastMigratedPosix[SessionType::ST_Physical]),
								msg.m_NumberOfComplaints,
								msg.m_hComplainer);
						}

						// we only need to action this if it came from the host
						// we now (generally) process the snSession level remove gamer commands before this message 
						// so we'll have already actioned this in that case
						if(!IsHost() && m_pSession->IsHostEndpointId(endpointId))
							HandleLocalPlayerBeingKicked(static_cast<KickReason>(msg.m_Reason));
					}
#if !__NO_OUTPUT
					else
						gnetDebug2("\tInvalid State: %s", GetSessionStateAsString(m_SessionState));
#endif
				}
			}
            else if(messageId == MsgRequestTransitionParameters::MSG_ID())
            {
				// message checks
				MsgRequestTransitionParameters msg;
				MSG_LOG_RECEIVED(netTransitionMsgDebug);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_TRANSITION_SESSION();

                // don't give script parameters for players we don't know about yet
                if(!pSession->IsMember(msg.m_hGamer))
				{
					netTransitionMsgDebug("OnCxnEvent :: Received MsgRequestTransitionParameters from unknown player: %s", msg.m_hGamer.ToString());
					break;
				}

                // grab gamer info
                rlGamerInfo hInfo;
                pSession->GetGamerInfo(msg.m_hGamer, &hInfo);

                // force send transition parameters back to this player
                ForceSendTransitionParameters(hInfo);
            }
			else if(messageId == MsgTransitionParameters::MSG_ID())
			{
				// message checks
				MsgTransitionParameters msg;
				MSG_LOG_RECEIVED(netTransitionMsgDebug);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_TRANSITION_SESSION();

                // don't give script parameters for players we don't know about yet
                if(!pSession->IsMember(msg.m_hGamer))
				{
					netTransitionMsgDebug("OnCxnEvent :: Received MsgTransitionParameters from unknown player: %s. Num: %d", msg.m_hGamer.ToString(), msg.m_nNumParameters);
					break;
				}

                netTransitionMsgDebug("\tNumParams: %u", msg.m_nNumParameters);

				// fire an event for each parameter change
				int nParams = 0;
				sParameterData aParamData[sTransitionParameterData::MAX_NUM_PARAMS];

				// fire an event for each parameter that is valid
				for(int i = 0; i < msg.m_nNumParameters; i++)
				{
					if(nParams == sTransitionParameterData::MAX_NUM_PARAMS)
					{
						GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionParameterChanged(msg.m_hGamer, nParams, aParamData));
						nParams = 0;
					}

					aParamData[nParams].m_id.Int    = static_cast<int>(msg.m_Parameters[i].m_nID);
					aParamData[nParams].m_value.Int = static_cast<int>(msg.m_Parameters[i].m_nValue);
					++nParams;
				}

				if(nParams > 0)
					GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionParameterChanged(msg.m_hGamer, nParams, aParamData));
			}
			else if(messageId == MsgTransitionParameterString::MSG_ID())
			{
				// message checks
				MsgTransitionParameterString msg;
				MSG_LOG_RECEIVED(netTransitionMsgDebug);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_TRANSITION_SESSION();

                // don't give script parameters for players we don't know about yet
                if(!m_pTransition->IsMember(msg.m_hGamer))
				{
					netTransitionMsgDebug("OnCxnEvent :: Received MsgTransitionParameterString from unknown player: %s", msg.m_hGamer.ToString());
					break;
				}

				// fire an event
				GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionStringChanged(msg.m_hGamer, 0, msg.m_szParameter));
			}
			else if(messageId == MsgTransitionGamerInstruction::MSG_ID())
			{
				// message checks
				MsgTransitionGamerInstruction msg;
				MSG_LOG_RECEIVED(netTransitionMsgDebug);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_TRANSITION_SESSION();

				// log parameters
				netTransitionMsgDebug("\tTo: %s", Util_GetGamerLogString(pSession, msg.m_hToGamer));
				netTransitionMsgDebug("\tInstruction: %d", msg.m_nInstruction);
				netTransitionMsgDebug("\tInstructionParam: %d", msg.m_nInstructionParam);
				netTransitionMsgDebug("\tToken: %d", msg.m_nUniqueToken);

				GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionGamerInstruction(msg.m_hFromGamer, msg.m_hToGamer, msg.m_szGamerName, msg.m_nInstruction, msg.m_nInstructionParam));

				// catch errors where the from gamer is the local player
				// expedite discovery of issue by validating the from gamer assert
				netTransitionMsgAssertf(msg.m_hFromGamer != NetworkInterface::GetLocalGamerHandle(), "OnCxnEvent :: Received MsgTransitionGamerInstruction - From Local Gamer!");
			}
			else if(messageId == MsgSessionEstablished::MSG_ID())
			{
				// message checks
				MsgSessionEstablished msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

				if(pSession == m_pSession && pSession->GetSessionId() == msg.m_SessionId)
				{
					if(m_pSession->IsMember(msg.m_hFromGamer))
						m_pSession->MarkAsEstablished(msg.m_hFromGamer);
				}
				else if(pSession == m_pTransition && pSession->GetSessionId() == msg.m_SessionId)
				{
					if(m_pTransition->IsMember(msg.m_hFromGamer))
						m_pTransition->MarkAsEstablished(msg.m_hFromGamer);
				}
			}
			else if(messageId == MsgSessionEstablishedRequest::MSG_ID())
			{
				// message checks
				MsgSessionEstablishedRequest msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

				bool bStarted = false;
				if(pSession == m_pSession && pSession->GetSessionId() == msg.m_SessionId)
					bStarted = m_SessionState >= SS_ESTABLISHED;
				else if(pSession == m_pTransition && pSession->GetSessionId() == msg.m_SessionId)
					bStarted = m_TransitionState >= TS_ESTABLISHED;

				gnetDebug2("\tSessionStarted: %s", bStarted ? "True" : "False");

				const u64 peerId = pSession ? pSession->GetPeerIdByCxn(pFrame->m_CxnId) : RL_INVALID_PEER_ID;
				if(peerId != RL_INVALID_PEER_ID)
				{
					if(bStarted)
					{
						MsgSessionEstablished msgEstablished(pSession->GetSessionId(), NetworkInterface::GetLocalGamerHandle());
						pSession->SendMsg(peerId, msgEstablished, NET_SEND_RELIABLE);
					}
				}
			}
			else if(messageId == MsgTransitionLaunch::MSG_ID())
			{
				// message checks
				MsgTransitionLaunch msg;
				MSG_LOG_RECEIVED(gnetDebug1);
				MSG_CHECK_SENT_FROM_HOST();
				MSG_CHECK_IMPORT(msg);

				// do not process if we're leaving the session
                if(IsTransitionLeaving())
                {
                    gnetDebug1("\tIgnoring, we're leaving");
                }
                else
                {
                    // keep this as we need the contents for later
                    m_sMsgLaunch = msg;

                    // if we're still joining... delay the launch
                    if(m_TransitionState < TS_ESTABLISHED)
					{
						gnetDebug1("\tReceived before established. Delaying...");
                        m_bLaunchOnStartedFromJoin = true;
						m_nImmediateLaunchCountdown = m_nImmediateLaunchCountdownTime;
					}
                    else
                    {
                        // launch transition
                        if(!m_bLaunchingTransition)
                            LaunchTransition();
                    }
                }
			}
			else if(messageId == MsgTransitionLaunchNotify::MSG_ID())
			{
				// message checks
				MsgTransitionLaunchNotify msg;
				MSG_LOG_RECEIVED(gnetDebug1);
				MSG_CHECK_IMPORT(msg);
				MSG_CHECK_IS_FREEROAM_SESSION();

				// if this isn't our physical session, just ignore (it means we launched)
				if(pSession != m_pSession)
					return;

				// we don't care if this is someone from our own transition
				if(!(m_pTransition->Exists() && m_pTransition->IsMember(msg.m_hGamer)))
				{
					// grab gamer info
					rlGamerInfo hInfo;
					if(m_pSession->GetGamerInfo(msg.m_hGamer, &hInfo))
					{
						// remove this peer from migration consideration
						m_pSession->MarkAsNotMigrating(hInfo.GetPeerInfo().GetPeerId());
					}

					CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(msg.m_hGamer);
					if(pPlayer)
						pPlayer->SetLaunchingTransition(true);
				}
			}
			else if(messageId == MsgTransitionToGameStart::MSG_ID())
			{
				// message checks
				MsgTransitionToGameStart msg;
				MSG_LOG_RECEIVED(gnetDebug1);
				MSG_CHECK_IMPORT(msg);

                // check flow states (if transitioning from a lobby, the expected state is different)
                bool bValidFlowState = (msg.m_IsTransitionFromLobby && IsTransitionActive() && !IsActivitySession() && !IsSessionActive() && !IsTransitionLeaving()) ||
									  (!msg.m_IsTransitionFromLobby && IsActivitySession() && !IsSessionLeaving() && !IsTransitionLeaving() && IsSessionActive());
					
				// check we don't already have a valid host to transition with or that this is from a different host and we are migrating
                // and we have a valid flow state
				if((!m_hTransitionHostGamer.IsValid() || (m_hTransitionHostGamer != msg.m_hGamer && m_pSession->IsMigrating())) && bValidFlowState)
				{
                    gnetDebug1("\tAccepted - From: %s, Mode: %d, FromLobby: %s", msg.m_hGamer.ToString(), msg.m_GameMode, msg.m_IsTransitionFromLobby ? "True" : "False");

					m_TransitionToGameGameMode = msg.m_GameMode;
					m_bIsTransitionToGameInSession = false;
                    m_bIsTransitionToGamePendingLeave = false;
					m_bIsTransitionToGameFromLobby = msg.m_IsTransitionFromLobby;
					m_hTransitionHostGamer = msg.m_hGamer;
                    m_nToGameCompletedTimestamp = 0;
					m_nToGameTrackingTimestamp = sysTimer::GetSystemMsTime();
				}
#if !__NO_OUTPUT
				else
				{
					gnetDebug1("\tRejected - From: %s (TransitionHostGamer: %s), ToGame: %s, IsTransitionFromLobby: %s, ValidFlowState: %s, IsActivitySession: %s, SessionState: %d, TransitionState: %d", 
						msg.m_hGamer.ToString(),
						m_hTransitionHostGamer.ToString(),
						m_bIsTransitionToGame ? "True" : "False", 
						msg.m_IsTransitionFromLobby ? "True" : "False",
						bValidFlowState ? "True" : "False",
						IsActivitySession() ? "True" : "False", 
						m_SessionState, 
						m_TransitionState);
				}
#endif		
			}
			else if(messageId == MsgTransitionToGameNotify::MSG_ID())
			{
				// message checks
				MsgTransitionToGameNotify msg;
				MSG_LOG_RECEIVED(gnetDebug1);
				MSG_CHECK_IMPORT(msg);

				gnetDebug1("\tFrom: %s (TransitionHostGamer: %s), Type: %d (%s), Token: 0x%016" I64FMT "x, IsTransitionToGame: %s", 
					msg.m_hGamer.ToString(),
					m_hTransitionHostGamer.ToString(),
					msg.m_NotificationType,
					msg.m_NotificationType == MsgTransitionToGameNotify::NOTIFY_SWAP ? "Swap" : "Update",
					msg.m_SessionInfo.GetToken().m_Value,
					m_bIsTransitionToGame ? "True" : "False");

				// validate that this is the same gamer that started the transition
				if(m_hTransitionHostGamer.IsValid() && (m_hTransitionHostGamer == msg.m_hGamer))
				{
					if(msg.m_NotificationType == MsgTransitionToGameNotify::NOTIFY_SWAP)
					{
						// swap to the transition session and reset the session state (if not a lobby transition)
						if(!m_bIsTransitionToGameFromLobby)
							SwapToTransitionSession();

						SetSessionState(SS_IDLE);

						// transitioning to session
						m_bIsTransitionToGame = true;

						// reset flag
						m_JoinFailureAction = msg.m_bIsPrivate ? JoinFailureAction::JFA_HostPrivate : JoinFailureAction::JFA_Quickmatch;
						m_nGamersToGame = 0;

						// no longer an activity session
						m_bIsActivitySession = false;
						m_bIsLeavingLaunchedActivity = true;

						// join indicated session
						m_bIsJoiningViaMatchmaking = false; 
						SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_DirectJoin]);

						m_hTransitionToGameSession = msg.m_SessionInfo;
						m_nTransitionToGameSwitchCount = 0;

						// generate script event
						GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_TO_GAME_START));

						// track who managed this
						m_hToGameManager = m_hTransitionHostGamer;
						JoinSession(msg.m_SessionInfo, msg.m_bIsPrivate ? RL_SLOT_PRIVATE : RL_SLOT_PUBLIC, JoinFlags::JF_Default);
					}
					else if(msg.m_NotificationType == MsgTransitionToGameNotify::NOTIFY_UPDATE)
					{
						// we won't swap sessions here, requires a NOTIFY_SWAP
						if(!IsActivitySession() && 
							m_bIsTransitionToGame &&
						   !m_pSession->IsMember(m_hTransitionHostGamer) && 
						   !(m_pSession->Exists() && m_pSession->GetSessionToken() == msg.m_SessionInfo.GetToken()) &&
						   !(m_hTransitionToGameSession == msg.m_SessionInfo))
						{
							gnetDebug1("\tAccepted - SwitchCount: %d", m_nTransitionToGameSwitchCount);

							// close the network
							if(CNetwork::IsNetworkOpen())
								CloseNetwork(false);

							// drop the session only
							DropSession();

							// reset the state ahead of another join / host attempt
							SetSessionState(SS_IDLE, true);

							// reset session config
							m_Config[SessionType::ST_Physical].Clear();

							// join this new session
							if(!JoinSession(msg.m_SessionInfo, msg.m_bIsPrivate ? RL_SLOT_PRIVATE : RL_SLOT_PUBLIC, JoinFlags::JF_Default))
								CheckForAnotherSessionOrBail(BAIL_SESSION_JOIN_FAILED, BAIL_CTX_JOIN_FAILED_NOTIFY);

							// update the session and switch count
							m_hTransitionToGameSession = msg.m_SessionInfo;
							m_nTransitionToGameSwitchCount++;
						}
					}
				}
			}
            else if(messageId == MsgTransitionToActivityStart::MSG_ID())
            {
                // make sure we're not already transitioning as a group
                if(!m_bIsActivityGroupQuickmatch)
                {
					// message checks
					MsgTransitionToActivityStart msg;
					MSG_LOG_RECEIVED(gnetDebug2);
					MSG_CHECK_IMPORT(msg);

                    gnetDebug2("\tFrom: %s, Async: %s", msg.m_hGamer.ToString(), msg.m_bIsAsync ? "True" : "False");

                    // track and generate script event
                    m_bIsActivityGroupQuickmatch = true;
					m_bIsActivityGroupQuickmatchAsync = msg.m_bIsAsync;
                    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_GROUP_QUICKMATCH_STARTED, msg.m_hGamer));
                }
            }
            else if(messageId == MsgTransitionToActivityFinish::MSG_ID())
            {
				// make sure we have an in-flight group switch
                if(m_bIsActivityGroupQuickmatch)
                {
					// message checks
					MsgTransitionToActivityFinish msg;
					MSG_LOG_RECEIVED(gnetDebug2);
					MSG_CHECK_IMPORT(msg);

					gnetDebug2("\tFrom %s. Token: 0x%016" I64FMT "x. Success: %s", msg.m_hGamer.ToString(), msg.m_SessionInfo.GetToken().m_Value, msg.m_bDidSucceed ? "True" : "False");

					// update tracking only if this new information is valid (don't stomp on valid information with invalid)
					if(msg.m_bDidSucceed && msg.m_SessionInfo.IsValid())
					{
						m_ActivityGroupSession = msg.m_SessionInfo;
						m_nGamersToActivity = 0;
					}

					// reset this (only on success - the host might setup their own session after finding no results which will not create a new start message)
                    if(msg.m_bDidSucceed)
                        m_bIsActivityGroupQuickmatch = false;

					// track who managed this
					m_hActivityQuickmatchManager = msg.m_hGamer;

                    // generate script event
                    GetEventScriptNetworkGroup()->Add(CEventNetworkTransitionEvent(CEventNetworkTransitionEvent::EVENT_GROUP_QUICKMATCH_FINISHED, msg.m_hGamer, (msg.m_bDidSucceed && msg.m_SessionInfo.IsValid()) ? 1 : 0));
                }
            }
            else if(messageId == MsgRadioStationSync::MSG_ID())
            {
				// message checks
				MsgRadioStationSync msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

                // whether we want to process this message or not
                bool bProcess = false;

                // reply on the correct session
                rlGamerInfo aGamerInfo;
                if(m_pSession->GetSessionId() == msg.m_SessionId)
                {
                    if(!IsSessionLeaving())
                        bProcess = true;

                    gnetDebug3("OnCxnEvent :: Received MsgRadioStationSync for main session. %s, Stations: %d", bProcess ? "Processing" : "Discarding", msg.m_nRadioStations);
                    m_bMainSyncedRadio = true;
                }
                else if(m_pTransition->GetSessionId() == msg.m_SessionId)
                {
                    if(!IsTransitionLeaving())
                        bProcess = true;

                    gnetDebug3("OnCxnEvent :: Received MsgRadioStationSync for transition session. %s, Stations: %d", bProcess ? "Processing" : "Discarding", msg.m_nRadioStations);
                    m_bTransitionSyncedRadio = true;
                }

                for(u8 i = 0; i < msg.m_nRadioStations; i++)
                {
					audRadioStation* pStation = audRadioStation::FindStation(msg.m_SyncData[i].nameHash);
					if(gnetVerifyf(pStation, "Failed to find radio station for syncing data!"))
						pStation->SyncStation(msg.m_SyncData[i]);
                }
            }
            else if(messageId == MsgRadioStationSyncRequest::MSG_ID())
            {
				// message checks
				MsgRadioStationSyncRequest msg;
				MSG_LOG_RECEIVED(gnetDebug3);
				MSG_CHECK_IMPORT(msg);

				SyncRadioStations(msg.m_SessionId, msg.m_GamerID);
            }
			else if(messageId == MsgCheckQueuedJoinRequest::MSG_ID())
			{
				// message checks
				MsgCheckQueuedJoinRequest msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

				// look up session, and check if we have space
				snSession* pRequestedSession = GetSessionFromToken(msg.m_nSessionToken);
				if(!pRequestedSession)
				{	
					gnetDebug2("\tIgnoring for 0x%016" I64FMT "x", msg.m_nSessionToken);
					break; 
				}

				if(!pRequestedSession->IsHost())
				{	
					gnetDebug2("\tNot host of 0x%016" I64FMT "x", msg.m_nSessionToken);
					break; 
				}

                // slot type
                unsigned nReservationParameter = msg.m_bIsSpectator ? RESERVE_SPECTATOR : RESERVE_PLAYER;

				// check if we have space to reserve
				const SessionType sessionType = GetSessionType(pRequestedSession);
                if(sessionType == SessionType::ST_Physical)
				{
					if(!m_bBlockJoinRequests)
					{
						if(!((GetMatchmakingGroupNum(msg.m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER) + pRequestedSession->GetNumSlotsWithParam(nReservationParameter)) < GetMatchmakingGroupMax(msg.m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER)))
						{
							gnetDebug2("\tNo Space for %s in 0x%016" I64FMT "x, Num: %d, Reservations: %d, Max: %d", msg.m_bIsSpectator ? "SPECTATOR" : "PLAYER", pRequestedSession->GetSessionInfo().GetToken().m_Value, GetMatchmakingGroupNum(msg.m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER), pRequestedSession->GetNumSlotsWithParam(nReservationParameter), GetMatchmakingGroupMax(msg.m_bIsSpectator ? MM_GROUP_SCTV : MM_GROUP_FREEMODER));
							break; 
						}
					}
				}
				else if(sessionType == SessionType::ST_Transition)
				{
					if(!m_bBlockTransitionJoinRequests)
					{
						if(!((GetActivityPlayerNum(msg.m_bIsSpectator) + pRequestedSession->GetNumSlotsWithParam(nReservationParameter)) < GetActivityPlayerMax(msg.m_bIsSpectator)))
						{
							gnetDebug2("\tNo Space for %s in 0x%016" I64FMT "x, Num: %d, Reservations: %d, Max: %d", msg.m_bIsSpectator ? "SPECTATOR" : "PLAYER", pRequestedSession->GetSessionInfo().GetToken().m_Value, GetActivityPlayerNum(msg.m_bIsSpectator), pRequestedSession->GetNumSlotsWithParam(nReservationParameter), GetActivityPlayerMax(msg.m_bIsSpectator));
							break; 
						}
					}
				}
				else
				{
					gnetDebug2("\tInvalid type for 0x%016" I64FMT "x", msg.m_nSessionToken);
					break; 
				}

				// validate that this player is a friend
				if(IsClosedFriendsSession(sessionType) && !rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), msg.m_hGamer))
					break;

				// validate that this player can join with the given crew ID
				if(HasCrewRestrictions(sessionType) && !CanJoinWithCrewID(sessionType, msg.m_nCrewID))
					break;

				// extend slot reservation (will automatically happen with the ReserveSlots call)
				if(!(pRequestedSession->ReserveSlots(&msg.m_hGamer, 1, RL_SLOT_PUBLIC, m_QueuedInviteTimeout, false, nReservationParameter) || pRequestedSession->ReserveSlots(&msg.m_hGamer, 1, RL_SLOT_PRIVATE, m_QueuedInviteTimeout, false, nReservationParameter)))
				{
					gnetDebug2("\tFailed to add reservation for %s for 0x%016" I64FMT "x", msg.m_hGamer.ToString(), pRequestedSession->GetSessionInfo().GetToken().m_Value);
					break;
				}

				// find player by looking up connection ID
				const u64 peerId = pRequestedSession->GetPeerIdByCxn(pFrame->m_CxnId);
				if(peerId == RL_INVALID_PEER_ID)
				{
					gnetDebug2("\tReserving slot for %s for 0x%016" I64FMT "x. Cannot find peerId", msg.m_hGamer.ToString(), pRequestedSession->GetSessionInfo().GetToken().m_Value);
					break;
				}

				gnetDebug2("\tReserving slot for %s (via %s) for 0x%016" I64FMT "x.", msg.m_hGamer.ToString(), Util_GetGamerLogString(pRequestedSession, peerId), pRequestedSession->GetSessionInfo().GetToken().m_Value);

				MsgCheckQueuedJoinRequestReply msgReply;
				msgReply.Reset(msg.m_nUniqueToken, true, msg.m_nSessionToken);
				pRequestedSession->SendMsg(peerId, msgReply, NET_SEND_RELIABLE);
			}
			else if(messageId == MsgCheckQueuedJoinRequestInviteReply::MSG_ID())
			{
				// message checks
				MsgCheckQueuedJoinRequestInviteReply msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

				snSession* pRequestedSession = GetSessionFromToken(msg.m_nSessionToken);
				if(!pRequestedSession)
				{	
					gnetDebug2("\tIgnoring for 0x%016" I64FMT "x", msg.m_nSessionToken);
					break; 
				}

				if(!pRequestedSession->IsHost())
				{	
					gnetDebug2("\tNot host of 0x%016" I64FMT "x", msg.m_nSessionToken);
					break; 
				}

				// extend slot reservation (will automatically happen with the ReserveSlots call)
				if(msg.m_bFreeSlots)
				{
					gnetDebug2("\tFree reservation for %s for 0x%016" I64FMT "x", msg.m_hGamer.ToString(), pRequestedSession->GetSessionInfo().GetToken().m_Value);
					pRequestedSession->FreeSlots(&msg.m_hGamer, 1);
				}
				else
				{
					if(!(pRequestedSession->ReserveSlots(&msg.m_hGamer, 1, RL_SLOT_PUBLIC, m_MaxWaitForQueuedInvite, false) || pRequestedSession->ReserveSlots(&msg.m_hGamer, 1, RL_SLOT_PRIVATE, m_MaxWaitForQueuedInvite, false)))
					{
						gnetDebug2("\tFailed to extend reservation for %s for 0x%016" I64FMT "x", msg.m_hGamer.ToString(), pRequestedSession->GetSessionInfo().GetToken().m_Value);
						break;
					}
					
					// nothing else to do here, just log the result
					gnetDebug2("\tExtending reservation for %s for 0x%016" I64FMT "x", msg.m_hGamer.ToString(), pRequestedSession->GetSessionInfo().GetToken().m_Value);
				}
			}
			else if(messageId == MsgCheckQueuedJoinRequestReply::MSG_ID())
			{
				// message checks
				MsgCheckQueuedJoinRequestReply msg;
				MSG_LOG_RECEIVED(gnetDebug2);
				MSG_CHECK_IMPORT(msg);

				if(msg.m_bReservedSlot)
				{
					// look up session, and check if we have space
					snSession* pRequestedSession = GetSessionFromToken(msg.m_nSessionToken);
					if(!pRequestedSession)
					{	
						gnetDebug2("\tIgnoring for 0x%016" I64FMT "x", msg.m_nSessionToken);
						break; 
					}

					int nCount = m_QueuedJoinRequests.GetCount();
					for(int i = 0; i < nCount; i++)
					{
						// look up via unique token
						if(m_QueuedJoinRequests[i].m_nUniqueToken == msg.m_nUniqueToken)
						{	
							gnetDebug2("\tSending invite to %s for 0x%016" I64FMT "x.", NetworkUtils::LogGamerHandle(m_QueuedJoinRequests[i].m_hGamer), pRequestedSession->GetSessionInfo().GetToken().m_Value);
							CGameInvite::TriggerFromJoinQueue(m_QueuedJoinRequests[i].m_hGamer, pRequestedSession->GetSessionInfo(), m_QueuedJoinRequests[i].m_nUniqueToken);
							m_QueuedJoinRequests[i].m_nInviteSentTimestamp = sysTimer::GetSystemMsTime();
							m_QueuedJoinRequests[i].m_nLastInviteSentTimestamp = m_QueuedJoinRequests[i].m_nInviteSentTimestamp;
							break;
						}
					}
				}
			}
			else if(messageId == MsgPlayerCardSync::MSG_ID())
			{
				// message checks
				MsgPlayerCardSync msg;
				MSG_CHECK_IMPORT(msg);

				netPlayerCardDebug("[corona_sync] OnCxnEvent :: Received MsgPlayerCardSync");
				ReceivePlayerCardData(msg.m_gamerID, msg.m_sizeOfData, msg.m_data);
			}
			else if(messageId == MsgPlayerCardRequest::MSG_ID())
			{
				// message checks
				MsgPlayerCardRequest msg;
				MSG_CHECK_IMPORT(msg);

				netPlayerCardDebug("[corona_sync] OnCxnEvent :: Received MsgPlayerCardRequest");
				ReceivePlayerCardDataRequest(msg.m_gamerID);
			}
#if !__NO_OUTPUT
			else if(messageId == MsgDebugStall::MSG_ID())
			{
				if(NetworkInterface::IsGameInProgress())
				{
					MsgDebugStall msg;
					MSG_CHECK_IMPORT(msg);

					rlGamerInfo hInfo;
					if(pSession)
					{
						u64 peerId = pSession->GetPeerIdByCxn(pFrame->m_CxnId);
						pSession->GetGamers(peerId, &hInfo, 1);
					}
					
					if(gnetVerify(msg.Import(pFrame->m_FrameReceived->m_Payload, pFrame->m_FrameReceived->m_SizeofPayload)))
					{
						gnetWarning("NetStallDetect :: Network :: Remote player %s - Stall of %ums (Network Update: %ums), Channel: %s", 
									hInfo.IsValid() ? hInfo.GetName() : "[Unknown]",
									msg.m_StallLength, 
									msg.m_NetworkUpdateTimeMs,
									NetworkUtils::GetChannelAsString(pFrame->m_ChannelId));
					}
				}
			}
#endif
		}
		break;

	case NET_EVENT_ACK_RECEIVED:
		break;

	case NET_EVENT_BANDWIDTH_EXCEEDED:
		gnetDebug1("CxnEvent :: BandwidthExceeded [%08u:%u] :: Channel: %s", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId));
		break;

	case NET_EVENT_OUT_OF_MEMORY:
		gnetDebug1("CxnEvent :: OutOfMemory [%08u:%u] :: Channel: %s, ID: 0x%08x", CNetwork::GetNetworkTime(), static_cast<unsigned>(rlGetPosixTime()), NetworkUtils::GetChannelAsString(pEvent->m_ChannelId), pEvent->m_CxnId);
		break;
	}
}

bool CNetworkSession::SendPlayerCardData(const rlGamerHandle& handle, const u32 sizeOfData, u8* data)
{
	if (gnetVerify(sizeOfData) && gnetVerify(data))
	{
		const rlGamerInfo* gamerinfo = NetworkInterface::GetActiveGamerInfo();

		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(int i=0; i<nGamers; i++)
		{
			if (aGamers[i].GetGamerHandle() == handle)
			{
				MsgPlayerCardSync msg;
				msg.Reset(gamerinfo->GetGamerId(), sizeOfData, data);
				m_pTransition->SendMsg(aGamers[i].GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
				
				netPlayerCardDebug("[corona_sync] SendPlayerCardData reliable message MsgPlayerCardSync - Success - '%s'.", NetworkUtils::LogGamerHandle(handle));

				return true;
			}
		}
	}

	netPlayerCardDebug("[corona_sync] SendPlayerCardData reliable message MsgPlayerCardSync - Fail - '%s'.", NetworkUtils::LogGamerHandle(handle));

	return false;
}

bool CNetworkSession::SendPlayerCardDataRequest(const rlGamerHandle& handle)
{
	if (gnetVerify(handle.IsValid()) 
		&& gnetVerify(!handle.IsLocal()) 
		&& gnetVerify(IsTransitionMember(handle)))
	{
		rlGamerInfo* rgi = 0;

		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		const unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(int i=0; i<nGamers; i++)
		{
			if (aGamers[i].GetGamerHandle() == handle)
			{
				rgi = &aGamers[i];
			}
		}

		if (gnetVerify(rgi))
		{
			const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
			if (gnetVerify(gi))
			{
				MsgPlayerCardRequest msg;
				msg.Reset(gi->GetGamerId());
				m_pTransition->SendMsg(rgi->GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());

				netPlayerCardDebug("[corona_sync] SendPlayerCardDataRequest reliable message MsgPlayerCardRequest - Success - '%s'.", NetworkUtils::LogGamerHandle(handle));

				return true;
			}
		}
	}

	netPlayerCardDebug("[corona_sync] SendPlayerCardDataRequest reliable message MsgPlayerCardRequest - Fail - '%s'.", NetworkUtils::LogGamerHandle(handle));

	return false;
}
void CNetworkSession::ReceivePlayerCardData(const rlGamerId& gamerID, const u32 sizeOfData, u8* data)
{
	snSession* pSession = NULL;
	rlGamerInfo gamerInfo;

	netPlayerCardDebug("[corona_sync] ReceivePlayerCardData reliable message MsgPlayerCardSync");

	if(m_pTransition->GetGamerInfo(gamerID, &gamerInfo))
	{
		pSession = m_pTransition;
	}

	// it's from the corona session
	if (pSession && gnetVerify(sizeOfData) && gnetVerify(data))
	{
		if(SPlayerCardManager::IsInstantiated())
		{
			BasePlayerCardDataManager* coronamgr = SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);
			if (gnetVerify(coronamgr))
			{
				netPlayerCardDebug("[corona_sync] ReceivePlayerCardData '%s'.", NetworkUtils::LogGamerHandle(gamerInfo.GetGamerHandle()));

				static_cast<CScriptedCoronaPlayerCardDataManager*>(coronamgr)->ReceivePlayerCardData(gamerInfo, data, sizeOfData);
			}
		}
	}
	else if (!pSession)
	{
		netPlayerCardDebug("[corona_sync] ReceivePlayerCardData reliable message MsgPlayerCardSync - FAILED - transition session is not active.");
	}
}

void CNetworkSession::ReceivePlayerCardDataRequest(const rlGamerId& gamerID)
{
	snSession* pSession = NULL;
	rlGamerInfo gi;

	netPlayerCardDebug("[corona_sync] ReceivePlayerCardDataRequest reliable message MsgPlayerCardRequest");

	if(m_pTransition->GetGamerInfo(gamerID, &gi))
	{
		pSession = m_pTransition;
	}

	// it's from the corona session
	if (pSession && gnetVerify(IsTransitionMember(gi.GetGamerHandle())))
	{
		if(SPlayerCardManager::IsInstantiated())
		{
			if(!SCPlayerCardCoronaManager::GetInstance().RequestFromGamer(gi.GetGamerHandle()))
			{
				netPlayerCardWarning("[corona_sync] ReceivePlayerCardDataRequest - FAILED - mgr.RequestFromGamer() - '%s'.", NetworkUtils::LogGamerHandle(gi.GetGamerHandle()));
			}
		}
	}
	else if (!pSession)
	{
		netPlayerCardWarning("[corona_sync] ReceivePlayerCardDataRequest reliable message MsgPlayerCardRequest - FAILED - transition session is not active.");
	}
}

void CNetworkSession::OnReceivedTransitionSessionInfo(const CNetGamePlayer* OUTPUT_ONLY(pPlayer), bool OUTPUT_ONLY(bInvalidate))
{
#if !__NO_OUTPUT
	// we already log enough information about the local player
	if(pPlayer->IsLocal())
		return;

	if(bInvalidate)
		gnetDebug1("OnReceivedTransitionSessionInfo :: %s has left transition", pPlayer->GetLogName());
	else
	{
		if(pPlayer->GetTransitionSessionInfo().IsValid())
			gnetDebug1("OnReceivedTransitionSessionInfo :: %s - Started transition. Token: 0x%016" I64FMT "x.", pPlayer->GetLogName(), pPlayer->GetTransitionSessionInfo().GetToken().m_Value);
		else
			gnetError("OnReceivedTransitionSessionInfo :: %s - Invalid info. Token: 0x%016" I64FMT "x, AddressValid: %s", pPlayer->GetLogName(), pPlayer->GetTransitionSessionInfo().GetToken().m_Value, pPlayer->GetTransitionSessionInfo().GetHostPeerAddress().IsValid() ? "True" : "False");
	}
#endif
}

void CNetworkSession::TryPeerComplaint(u64 nPeerID, netPeerComplainer* pPeerComplainer, snSession* pSession, const SessionType sessionType OUTPUT_ONLY(, bool bLogRejections))
{
	// validate peer complainer
	if(!gnetVerify(pPeerComplainer))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Invalid peer complainer"); })
		return;
	}

	// validate session
	if(!gnetVerify(pSession))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Invalid session"); })
		return;
	}

	// validate session index
	if(!(sessionType == SessionType::ST_Physical || sessionType == SessionType::ST_Transition))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Invalid session index: %d", sessionType); })
		return;
	}

	// check we're ready for complaints
	if(!IsReadyForComplaints(sessionType))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Peer complainer is not ready"); })
		return;
	}

	// check peer ID is valid
	if(nPeerID == RL_INVALID_PEER_ID)
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Invalid PeerID"); })
		return;
	}

#if !__FINAL 
	if(PARAM_netSessionDisablePeerComplainer.Get())
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Peer complainer disabled with -netSessionDisablePeerComplainer"); })
		return;
	}
#endif

	// check we're not the host
	if(pPeerComplainer->IsServer())
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Local player is peer complainer authority"); })
		return;
	}

	// grab the session host ID to verify this isn't the host we're trying to disconnect
	u64 nHostPeerID; 
	m_pSession->GetHostPeerId(&nHostPeerID);
	if(nHostPeerID == nPeerID)
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Complaining about host PeerID: 0x%" I64FMT "x", nPeerID); })
		return;
	}

	// check peer is known
	if(!pPeerComplainer->HavePeer(nPeerID))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Unknown PeerID: 0x%" I64FMT "x", nPeerID); })
		return;
	}

	// check we haven't already registered a complaint
	if(pPeerComplainer->IsComplaintRegistered(nPeerID))
	{
		OUTPUT_ONLY(if(bLogRejections) { gnetDebug1("TryPeerComplaint :: Complaint already registered for PeerID: 0x%" I64FMT "x", nPeerID); })
		return;
	}

	// register complaint
	gnetDebug2("TryPeerComplaint :: [0x%016" I64FMT "x] %s - Registering complaint for PeerID: 0x%" I64FMT "x", pPeerComplainer->GetToken(), NetworkUtils::GetSessionTypeAsString(sessionType), nPeerID);
	pPeerComplainer->RegisterComplaint(nPeerID);
}

bool CNetworkSession::IsReadyForComplaints(const SessionType sessionType)
{
	if(sessionType == SessionType::ST_Physical)
	{
		bool bAllowComplaintsWhenEstablishing = m_bAllowComplaintsWhenEstablishing || IsActivitySession();
		return (m_SessionState == SS_ESTABLISHED) || (bAllowComplaintsWhenEstablishing && (m_SessionState == SS_ESTABLISHING));
	}
	else if(sessionType == SessionType::ST_Transition)
		return (m_TransitionState == TS_ESTABLISHED);

	return false;
}

void CNetworkSession::GetPeerComplainerPolicies(netPeerComplainer::Policies* pPolicies, snSession* pSession, eTimeoutPolicy nPolicy)
{
	// setup the policies for peer complainer 
	pPolicies->m_BootDelay = (nPolicy == POLICY_AGGRESSIVE) ? m_nPeerComplainerAggressiveBootDelay : m_nPeerComplainerBootDelay;
	pPolicies->m_ComplaintResendInterval = pPolicies->m_BootDelay / 2;
	pPolicies->m_ChannelId = pSession->GetChannelId();
}

void CNetworkSession::SetPeerComplainerPolicies(netPeerComplainer* pPeerComplainer, snSession* pSession, eTimeoutPolicy nPolicy)
{
	netPeerComplainer::Policies tPolicies; 
	GetPeerComplainerPolicies(&tPolicies, pSession, nPolicy);
	pPeerComplainer->SetPolicies(tPolicies);
}

netNatType CNetworkSession::GetNatTypeFromPeerID(const SessionType sessionType, const u64 nPeerID)
{
	if(sessionType == SessionType::ST_Physical)
	{
		const netPlayer* pPlayer = NetworkInterface::GetPlayerFromPeerId(nPeerID);
		if(pPlayer)
			return pPlayer->GetNatType();
		else
		{
			gnetError("GetNatTypeFromPeerID :: Peer [0x%" I64FMT "x] is not a valid network player", nPeerID);
			return NET_NAT_UNKNOWN;
		}
	}
	else
	{
		rlGamerInfo tGamerInfo;
		unsigned nGamers = m_pTransition->GetGamers(nPeerID, &tGamerInfo, 1);

		if(nGamers == 1)
		{
			unsigned nGamers = m_TransitionPlayers.GetCount();
			for(unsigned i = 0; i < nGamers; i++) if(m_TransitionPlayers[i].m_hGamer == tGamerInfo.GetGamerHandle())
				return m_TransitionPlayers[i].m_NatType;
		}
	}

	return NET_NAT_UNKNOWN;
}

void CNetworkSession::OnPeerComplainerBootRequest(netPeerComplainer* pPeerComplainer, netPeerComplainer::BootCandidate* pCandidates, const unsigned nCandidates)
{
	// work out which session this complaint is for
	SessionType sessionType = SessionType::ST_None;
	for(int i = 0; i < SessionType::ST_Max; i++)
	{
		if(pPeerComplainer == &m_PeerComplainer[i])
		{
			sessionType = static_cast<SessionType>(i);
			break;
		}
	}

	// should consider associating the session with the peer complainer so we don't need this conversion
	snSession* pSession = nullptr;
	switch(sessionType)
	{
	case SessionType::ST_Physical: pSession = m_pSession; break;
	case SessionType::ST_Transition: pSession = m_pTransition; break;
	default: gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Invalid session!", pPeerComplainer->GetToken()); return;
	}

    gnetDebug1("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Checking %d candidates", pPeerComplainer->GetToken(), nCandidates);
#if !__NO_OUTPUT
    for(unsigned i = 0; i < nCandidates; i++)
        gnetDebug1("\tOnPeerComplainerBootRequest ::[0x%016" I64FMT "x] Candidate %d [0x%" I64FMT "x] has %d complaints. Has Previous: %s", pPeerComplainer->GetToken(), i + 1, pCandidates[i].m_PeerId, pCandidates[i].m_NumComplaints, pCandidates[i].m_HasPrevious ? "T" : "F");
#endif

	// check we're host
	if(!gnetVerify(pSession->IsHost()))
	{
		gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Not session host!", pPeerComplainer->GetToken());
		return; 
	}

	// check we've been given some candidates
	if(!gnetVerify(nCandidates > 0))
	{
		gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] No candidates to boot!", pPeerComplainer->GetToken());
		return; 
	}

	int nCandidateToBoot = -1;
	int nOtherCandidate = -1;
	if(nCandidates > 1)
	{
		// if one player has more complaints than anyone else (assumes sorted list)
		if(pCandidates[0].m_NumComplaints > pCandidates[1].m_NumComplaints)
			nCandidateToBoot = 0;
		else
		{
			// we know that two players have the same number of complaints, check if any further players do
			unsigned nWithSameComplaints = 2;
            if(nCandidates > 2) for(unsigned i = 2; i < nCandidates; ++i)
			{
				if(pCandidates[i].m_NumComplaints == pCandidates[0].m_NumComplaints)
					++nWithSameComplaints;
			}

			// candidate list
			unsigned aCandidateIdx[RL_MAX_GAMERS_PER_SESSION];
			memset(aCandidateIdx, 0, sizeof(unsigned) * RL_MAX_GAMERS_PER_SESSION);
			unsigned nNumCandidates = 0;

			// check if we have a bias for establishing candidates and eject them first
			bool bHasEstablishingCandidates = false;
			if(m_bPreferToBootEstablishingCandidates && sessionType == SessionType::ST_Physical)
			{
				for(unsigned i = 0; i < nWithSameComplaints && nNumCandidates < RL_MAX_GAMERS_PER_SESSION; ++i)
				{
					rlGamerInfo aGamer[1];
					unsigned nGamers = pSession->GetGamers(pCandidates[i].m_PeerId, aGamer, 1);
					if(nGamers > 0 && !pSession->IsGamerEstablished(aGamer[0].GetGamerHandle()))
					{
						gnetDebug1("OnPeerComplainerBootRequest :: Adding establishing candidate %s", aGamer[0].GetName());
						aCandidateIdx[nNumCandidates++] = i;
					}
				}

				// we have establishing candidates if anyone 
				bHasEstablishingCandidates = (nNumCandidates > 0);
			}

			// if none, check other criteria
			if(!bHasEstablishingCandidates)
			{
				// chuck someone randomly with the lowest NAT
				netNatType kLowestNAT = NET_NAT_UNKNOWN;
				for(unsigned i = 0; i < nWithSameComplaints; ++i)
				{
					netNatType kNAT = GetNatTypeFromPeerID(sessionType, pCandidates[i].m_PeerId);

					// if this is a new worst NAT, acknowledge and reset count
					if(kNAT > kLowestNAT)
						kLowestNAT = kNAT;
				}

				// generate candidate list
				for(unsigned i = 0; i < nWithSameComplaints && nNumCandidates < RL_MAX_GAMERS_PER_SESSION; ++i)
				{
					netNatType kNAT = GetNatTypeFromPeerID(sessionType, pCandidates[i].m_PeerId);

					if(!m_bPreferToBootLowestNAT)
						aCandidateIdx[nNumCandidates++] = i;
					else if(kNAT == kLowestNAT)
						aCandidateIdx[nNumCandidates++] = i;
					else // skip candidate boosting
						continue;

					// folks with previous have more chance of being selected
					if(pCandidates[i].m_HasPrevious && nNumCandidates < RL_MAX_GAMERS_PER_SESSION)
						aCandidateIdx[nNumCandidates++] = i;

					if(sessionType == SessionType::ST_Physical)
					{
						const netPlayer* pPlayer = NetworkInterface::GetPlayerFromPeerId(pCandidates[i].m_PeerId);

						// folks with last receive time of greater than half the network timeout have more chance of being selected
						if(pPlayer && CNetwork::GetConnectionManager().GetDeltaLastReceiveTime(pPlayer->GetEndpointId()) > static_cast<int>((static_cast<float>(CNetwork::GetTimeoutTime()) * 0.5f)) && nNumCandidates < RL_MAX_GAMERS_PER_SESSION)
							aCandidateIdx[nNumCandidates++] = i;
					}
				}
			}

			// pick a random index
			if(gnetVerify(nNumCandidates > 0))
			{
				// generate a random number within range
				int nIndex = Max(gs_Random.GetRanged(0, static_cast<int>(nNumCandidates - 1)), static_cast<int>(nNumCandidates - 1));
#if !__NO_OUTPUT
				gnetDebug1("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Generated %d Candidate Indexes. Index %d selected at random.", pPeerComplainer->GetToken(), nNumCandidates, nIndex);
				for(unsigned i = 0; i < nNumCandidates; ++i)
				{
					const netPlayer* pPlayer = NetworkInterface::GetPlayerFromPeerId(pCandidates[aCandidateIdx[i]].m_PeerId);
					gnetDebug1("\tOnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Candidate Index %d is %d [%s]", pPeerComplainer->GetToken(), i + 1, aCandidateIdx[i], pPlayer ? pPlayer->GetLogName() : "INVALID");
				}
#endif
				nCandidateToBoot = aCandidateIdx[nIndex];

				// pick any other candidate index
				for(unsigned i = 0; i < nWithSameComplaints; ++i)
				{
					if(i != aCandidateIdx[nIndex])
					{
						nOtherCandidate = i;
						break;
					}
				}
			}
			else
			{
				// just pick the first guy
				gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] No candidates!", pPeerComplainer->GetToken());
				nCandidateToBoot = 0;
			}
		}
	}
	else
		nCandidateToBoot = 0;

	// check we generated an ID
    gnetDebug1("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Candidate Boot Index is %d", pPeerComplainer->GetToken(), nCandidateToBoot);
	if(!gnetVerify(nCandidateToBoot >= 0))
	{
		gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] No suitable candidate found!", pPeerComplainer->GetToken());
		return; 
	}

    // check ID is not out of range
    if(!gnetVerify(nCandidateToBoot < nCandidates))
    {
        gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Candidate index is out of range!", pPeerComplainer->GetToken());
        return; 
    }

	// different action based on the session
	bool bSuccess = false;
	if(sessionType == SessionType::ST_Physical)
	{
		// find the player to kick
		netPlayer* pPlayer = NetworkInterface::GetPlayerFromPeerId(pCandidates[nCandidateToBoot].m_PeerId);
		if(!gnetVerify(pPlayer))
		{
			gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Invalid candidate!", pPeerComplainer->GetToken());
			return; 
		}

		// include the gamer handle of another candidate with the same number of complaints
		netPlayer* pOtherCandidate = NULL;
		if(nOtherCandidate >= 0)
		{
			pOtherCandidate = NetworkInterface::GetPlayerFromPeerId(pCandidates[nOtherCandidate].m_PeerId);
			if(!gnetVerify(pOtherCandidate))
			{
				gnetError("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Invalid other candidate!", pPeerComplainer->GetToken());
			}
		}

		// kick player
		gnetDebug1("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Kicking %s", pPeerComplainer->GetToken(), pPlayer->GetLogName());
		bSuccess = KickPlayer(pPlayer, KickReason::KR_PeerComplaints, pCandidates[nCandidateToBoot].m_NumComplaints, pOtherCandidate ? pOtherCandidate->GetGamerInfo().GetGamerHandle() : RL_INVALID_GAMER_HANDLE);
	}
	else if(sessionType == SessionType::ST_Transition)
	{
		rlGamerInfo tGamerInfo;
		unsigned nGamers = pSession->GetGamers(pCandidates[nCandidateToBoot].m_PeerId, &tGamerInfo, 1);

		// for the transition session, we just need to kick the player (this matches the connection error path for the main session)
		if(nGamers == 1)
		{
			gnetDebug1("OnPeerComplainerBootRequest :: [0x%016" I64FMT "x] Kicking %s", pPeerComplainer->GetToken(), tGamerInfo.GetName());
			bSuccess = pSession->Kick(&tGamerInfo.GetGamerHandle(), 1, snSession::KICK_INFORM_PEERS, NULL);
		}
	}

	// let peer complainer know that we kicked this player
	if(bSuccess)
		pCandidates[nCandidateToBoot].m_Booted = true;
}

bool CNetworkSession::KickPlayer(const netPlayer* pPlayerToKick, const KickReason kickReason, const unsigned nComplaints, const rlGamerHandle& hGamer)
{
	// not hosting... bail
	if(!gnetVerify(IsHost()))
	{
		gnetError("KickPlayer :: Not session host!");
		return false; 
	}

	// no player... bail
	if(!gnetVerify(pPlayerToKick))
	{
		gnetError("KickPlayer :: Invalid player specified!");
		return false; 
	}

	// log out name and reason
	gnetDebug1("KickPlayer :: Kicking %s, Reason: %s", pPlayerToKick->GetLogName(), NetworkSessionUtils::GetKickReasonAsString(kickReason));

	// is this a connection error
	bool bIsConnectionError = (kickReason == KickReason::KR_ConnectionError) || (kickReason == KickReason::KR_PeerComplaints);

	// if the player to be kicked is the local player, handle the kick
	if(pPlayerToKick->IsLocal())
		HandleLocalPlayerBeingKicked(kickReason);

	// send kick message to remote player - we do not do this for connection errors. The desired outcome
	// is that the remote player drops into their own session - not that they exit multiplayer
	else if(kickReason != KickReason::KR_ConnectionError)
	{
		// send a message to the player to let them kick themselves
		MsgKickPlayer msgKick;
		msgKick.m_SessionId = m_pSession->GetConfig().m_SessionId;
		msgKick.m_GamerId = pPlayerToKick->GetRlGamerId();
		msgKick.m_Reason = kickReason;
		msgKick.m_NumberOfComplaints = nComplaints;
		msgKick.m_hComplainer = hGamer;

		// send message and increment tracking
		m_pSession->SendMsg(pPlayerToKick->GetRlPeerId(), msgKick, NET_SEND_RELIABLE);
		BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
	}

	// work out whether we should blacklist or not
	bool bBlackList = m_bUseBlacklist; 
	bBlackList &= (kickReason != KickReason::KR_ScAdmin); //< silent kick, we use KICKREASON_SCADMIN_BLACKLIST to blacklist

	if(bBlackList)
	{
		// add a blacklist reason 
		eBlacklistReason nBlReason = (kickReason == KickReason::KR_VotedOut || kickReason == KickReason::KR_ScAdminBlacklist) ? BLACKLIST_VOTED_OUT : BLACKLIST_CONNECTION;

		// black list the gamer (we don't do this for connection errors)
		if(!bIsConnectionError NOTFINAL_ONLY(&& !PARAM_netSessionNoGamerBlacklisting.Get()))
			m_BlacklistedGamers.BlacklistGamer(pPlayerToKick->GetGamerInfo().GetGamerHandle(), nBlReason);
	}

	// this informs players of who is being kicked
	return m_pSession->Kick(&pPlayerToKick->GetGamerInfo().GetGamerHandle(), 1, snSession::KICK_INFORM_PEERS, NULL);
}

bool CNetworkSession::KickRandomPlayer(const bool isSpectator, const rlGamerHandle& handleToExclude, const KickReason kickReason)
{
	// candidate list
	int kickCandidateIndexes[MAX_NUM_PHYSICAL_PLAYERS];
	unsigned numKickCandidates = 0;

	// check if any players were disconnected during the transition
	unsigned numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	netPlayer* const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

	for(unsigned i = 0; i < numPhysicalPlayers; i++)
	{
		CNetGamePlayer *pPhysicalPlayer = SafeCast(CNetGamePlayer, allPhysicalPlayers[i]);
		if(!pPhysicalPlayer)
			continue;

		if(pPhysicalPlayer->IsSpectator() != isSpectator)
			continue;

		if(pPhysicalPlayer->GetGamerInfo().GetGamerHandle() == handleToExclude)
			continue;

		if(pPhysicalPlayer->IsLocal() && !m_AdminInviteKickIncludesLocal)
			continue; 

		kickCandidateIndexes[numKickCandidates] = i;
		numKickCandidates++;
	}

	// kick a random qualifying player
	if(numKickCandidates > 0)
	{
		int kickIndex = gs_Random.GetRanged(0, numKickCandidates - 1);
		return KickPlayer(allPhysicalPlayers[kickIndex], kickReason, 0, RL_INVALID_GAMER_HANDLE);
	}

	return false;
}

bool CNetworkSession::OnHandleJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest, const SessionType sessionType)
{
	// assume we'll accept...
	eJoinResponseCode nResponseCode = RESPONSE_ACCEPT;

	// request
	CMsgJoinRequest tJoinRequest; 
	bool bHasRequestData = tJoinRequest.Import(pRequest->m_RequestData, pRequest->m_SizeofRequestData);

	// work these out up front
    bool bIsSCTV = false;
	bool bCanQueue = false;

	switch(sessionType)
	{
	case SessionType::ST_Physical: 
        {
            bIsSCTV = (tJoinRequest.m_PlayerData.m_MatchmakingGroup == MM_GROUP_SCTV);
        }
        break;
    case SessionType::ST_Transition:
        {
            bIsSCTV = (tJoinRequest.m_PlayerData.m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR) != 0;
        }
        break;
	default: 
        break;
	}
	unsigned nReservationParameter = bIsSCTV ? RESERVE_SPECTATOR : RESERVE_PLAYER;

	// cache flags
	const bool isViaInvite = CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_ViaInvite);
	const bool canConsumePrivate = CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_ConsumePrivate);
	const bool isAdmin = CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_IsAdmin);
	const bool isAdminDev = CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_IsAdminDev);
	const bool isReputationBad = CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_BadReputation);
	const bool isAdminDevInvite = isViaInvite && isAdminDev;

    // log request
    gnetDebug1("OnHandleJoinRequest: From %s, Session: %s", gamerInfo.GetName(), NetworkUtils::GetSessionTypeAsString(sessionType));
	OUTPUT_ONLY(LogJoinFlags(tJoinRequest.m_JoinRequestFlags, "OnHandleJoinRequest"));

	// invalid join request data
	if(!bHasRequestData)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Invalid request data", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_INVALID_REQUEST_DATA;
	}
	// different sessions
    else if(pSession != m_pSession && pSession != m_pTransition)
    {
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Session", gamerInfo.GetName());
        nResponseCode = RESPONSE_DENY_WRONG_SESSION;
    }
    // build type
    else if(DEFAULT_BUILD_TYPE_VALUE != tJoinRequest.m_BuildType)
    {
        gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Build Type", gamerInfo.GetName());
        nResponseCode = RESPONSE_DENY_DIFFERENT_BUILD;
    }
	// socket port
	else if(CNetwork::GetSocketPort() != tJoinRequest.m_SocketPort)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Socket Port (Local: %d, Remote: %d)", gamerInfo.GetName(), CNetwork::GetSocketPort(), tJoinRequest.m_SocketPort);
		nResponseCode = RESPONSE_DENY_DIFFERENT_PORT;
	}
	// invalid assets
	else if(!CNetwork::GetAssetVerifier().Equals(tJoinRequest.m_AssetVerifier OUTPUT_ONLY(, gamerInfo.GetName())))
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Incompatible assets", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_INCOMPATIBLE_ASSETS;
	}
	// version failsafe - should be blocked out by MM
	else if(
#if !__FINAL
		!PARAM_netSessionIgnoreVersion.Get() && 
#endif
		CNetwork::GetVersion() != tJoinRequest.m_PlayerData.m_GameVersion)
    {
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Version. Local: %d, Joiner: %d", gamerInfo.GetName(), CNetwork::GetVersion(), tJoinRequest.m_PlayerData.m_GameVersion);
		nResponseCode = RESPONSE_DENY_WRONG_VERSION;
    }
	// aiming failsafe - should be blocked out by MM
	// allow spectator players through this check, they won't be aiming
	else if(!bIsSCTV &&
#if !__FINAL
		!PARAM_netSessionIgnoreAim.Get() && 
#endif
        // use setting from the config - on PC, the setting returned by NetworkGameConfig::GetMatchmakingAimBucket can change if we switch between mouse and game pad
		// convert remote to matchmaking aim setting for compatibility check
		m_Config[sessionType].GetAimBucket() != NetworkGameConfig::GetMatchmakingAimBucket(tJoinRequest.m_PlayerData.m_AimPreference))
	{
        gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Aiming Preference. Local: %d, Joiner: %d", gamerInfo.GetName(), NetworkGameConfig::GetMatchmakingAimBucket(), NetworkGameConfig::GetMatchmakingAimBucket(tJoinRequest.m_PlayerData.m_AimPreference));
		nResponseCode = RESPONSE_DENY_AIM_PREFERENCE;
	}
	// cheater failsafe - should be blocked out by MM
	else if(
#if !__FINAL
		!PARAM_netSessionIgnoreCheater.Get() && 
#endif
		m_nSessionPool[sessionType] != tJoinRequest.m_Pool &&
		!isAdminDevInvite)
	{
        gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different Pool. Local: %s, Joiner: %s", 
			NetworkUtils::GetPoolAsString(tJoinRequest.m_Pool),
            NetworkUtils::GetPoolAsString(m_nSessionPool[sessionType]),
			gamerInfo.GetName());
		
		switch(m_nSessionPool[sessionType])
		{
		case POOL_NORMAL:		nResponseCode = RESPONSE_DENY_POOL_NORMAL; break;
		case POOL_BAD_SPORT:	nResponseCode = RESPONSE_DENY_POOL_BAD_SPORT; break;
		case POOL_CHEATER:		nResponseCode = RESPONSE_DENY_POOL_CHEATER; break;
		default:				nResponseCode = RESPONSE_DENY_POOL_NORMAL; break;
		}	 
	}
	// timeout failsafe - should be blocked out by MM
	else if(
#if !__FINAL
		!PARAM_netSessionIgnoreTimeout.Get() && 
#endif
		CNetwork::GetTimeoutTime() != (tJoinRequest.m_NetworkTimeout * 1000))
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different timeouts. Local: %d, Joiner: %d", gamerInfo.GetName(), CNetwork::GetTimeoutTime(), tJoinRequest.m_NetworkTimeout * 1000);
		nResponseCode = RESPONSE_DENY_TIMEOUT;
	}
	// data hash failsafe - should be blocked out by MM
	else if(
#if !__FINAL
		!PARAM_netSessionIgnoreDataHash.Get() && 
#endif
		NetworkGameConfig::GetMatchmakingDataHash() != tJoinRequest.m_CloudDataHash)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Different cloud data hash. Local: %d, Joiner: %d", gamerInfo.GetName(), NetworkGameConfig::GetMatchmakingDataHash(), tJoinRequest.m_CloudDataHash);
		nResponseCode = RESPONSE_DENY_DATA_HASH;
	}
#if !__FINAL
	else if(PARAM_netSessionRejectJoin.Get())
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Blocking with netSessionRejectJoin", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_BLOCKING;
	}
#endif
	// private only (exclude crew sessions from this check)
	else if(m_UniqueCrewData[sessionType].m_nLimit == 0 && pSession->GetConfig().m_MaxPublicSlots == 0 && !(canConsumePrivate || (isViaInvite && CheckFlag(m_nHostFlags[sessionType], HostFlags::HF_AllowPrivate))))
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Private session and not invited!", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_PRIVATE_ONLY;
	}
	// premium session (matchmaking access only)
	else if(CheckFlag(m_nHostFlags[sessionType], HostFlags::HF_Premium) && isViaInvite && !isAdminDevInvite)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Premium session!", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_PREMIUM;
	}
    // unique crew quota full - check the joiner is in an existing crew
	// admin invites to developers / security can bypass this requirement
    else if(!bIsSCTV && !CanJoinWithCrewID(sessionType, tJoinRequest.m_PlayerData.m_ClanId) && !isAdminDevInvite)
    {
        gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Unique crew limit filled or blocked for non-crews. Crew: %d", gamerInfo.GetName(), static_cast<int>(tJoinRequest.m_PlayerData.m_ClanId));
        nResponseCode = RESPONSE_DENY_CREW_LIMIT;
	}
	// closed friends session - only friends can join
	// admin invites to developers / security can bypass this requirement
	else if(IsClosedFriendsSession(sessionType) && !rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), gamerInfo.GetGamerHandle()) && !isAdminDevInvite)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Non friend trying to join friends only session", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_NOT_FRIEND;
	}
	// solo session
	// admin invites to developers / security can bypass this requirement
	else if(IsSoloSession(sessionType) && !isAdminDevInvite)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Trying to join a solo session", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_SOLO;
	}
	// admin invite check - if we require a notification / reservation to be sent, check we have one
	else if(isAdmin && m_AdminInviteNotificationRequired && !CLiveManager::GetInviteMgr().HasAdminInviteNotification(gamerInfo.GetGamerHandle()))
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - No Admin notification", gamerInfo.GetName());
		nResponseCode = RESPONSE_DENY_ADMIN_BLOCKED;
	}
	// check reputation, we allow invites into private / closed friend sessions
	else if(!CLiveManager::IsOverallReputationBad() && isReputationBad && !((canConsumePrivate && m_Config[sessionType].HasPrivateSlots()) || IsClosedFriendsSession(sessionType)) && !isAdminDevInvite)
	{
		gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Reputation is different. Reputation: %s", gamerInfo.GetName(), CLiveManager::IsOverallReputationBad() ? "Bad" : "Good");
		nResponseCode = RESPONSE_DENY_REPUTATION;
	}
	else
	{
		// session differences
		switch(sessionType)
		{
		case SessionType::ST_Physical:
			{
				// joiner is blacklisted
				if(m_BlacklistedGamers.IsBlacklisted(gamerInfo.GetGamerHandle()) && (m_BlacklistedGamers.GetBlacklistReason(gamerInfo.GetGamerHandle()) == BLACKLIST_VOTED_OUT) && !isAdminDevInvite)
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Joiner is blacklisted", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_BLACKLISTED;
				}
                // session full - possible that we added a player between the matchmaking results coming back
                else if(m_pSession->GetGamerCount() >= (m_Config[SessionType::ST_Physical].GetMaxPublicSlots() + m_Config[SessionType::ST_Physical].GetMaxPrivateSlots()))
				{
					if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
					{
						gnetDebug1("OnHandleJoinRequest: Session full - kicking random player to make room");
						KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
					}
					else
					{
						gnetDebug1("OnHandleJoinRequest: Denying join request from %s - No space in session. Count: %d, Slots: %d", gamerInfo.GetName(), m_pSession->GetGamerCount(), (m_Config[SessionType::ST_Physical].GetMaxPublicSlots() + m_Config[SessionType::ST_Physical].GetMaxPrivateSlots()));
						nResponseCode = RESPONSE_DENY_SESSION_FULL;
					}
				}
				// not hosting session
				else if(!IsHost())
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Not Hosting", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_NOT_HOSTING;
				}
				// not ready
				else if(!CNetwork::GetPlayerMgr().IsInitialized() || !(m_SessionState >= SS_ESTABLISHING && m_SessionState < SS_LEAVING))
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Not Ready (1). CNetwork::GetPlayerMgr().IsInitialized() = %s, m_SessionState = %d", gamerInfo.GetName(), CNetwork::GetPlayerMgr().IsInitialized() ? "true" : "false", (int)m_SessionState);
					nResponseCode = RESPONSE_DENY_NOT_READY;
				}
				// session join requests blocked
				else if(m_bBlockJoinRequests)
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Blocking with m_bBlockJoinRequests", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_BLOCKING;
				}
				else
				{
					// check if we have activity slot
					if(IsActivitySession())
					{
						// make sure we've always been told how many spectators this mode supports
						if(!gnetVerifyf(m_bHasSetActivitySpectatorsMax, "OnHandleJoinRequest: Activity session but no call to SetActivitySpectatorsMax from script!"))
						{
							gnetError("OnHandleJoinRequest: Activity session but no call to SetActivitySpectatorsMax!");
						}

						if(bIsSCTV && (GetActivityPlayerNum(true) >= GetActivityPlayerMax(true)))
						{
							if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
							{
								gnetDebug1("OnHandleJoinRequest: Slots full - kicking random player to make room");
								KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
							}
							else
							{
								bCanQueue = (GetActivityPlayerMax(true) > 1);
								gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Maximum activity spectators. Num: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(true), GetActivityPlayerMax(true), bCanQueue ? "True" : "False");
								nResponseCode = RESPONSE_DENY_GROUP_FULL;
							}
						}
						else if(!bIsSCTV && (GetActivityPlayerNum(false) >= GetActivityPlayerMax(false)))
						{
							if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
							{
								gnetDebug1("OnHandleJoinRequest: Activity slots full - kicking random player to make room");
								KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
							}
							else
							{
								bCanQueue = (GetActivityPlayerMax(false) > 1);
								gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Maximum activity players. Num: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(false), GetActivityPlayerMax(false), bCanQueue ? "True" : "False");
								nResponseCode = RESPONSE_DENY_GROUP_FULL;
							}
						}
						else if(!((GetActivityPlayerNum(bIsSCTV) + pSession->GetNumSlotsWithParam(nReservationParameter)) < GetActivityPlayerMax(bIsSCTV)) && !pSession->HasSlotReservation(gamerInfo.GetGamerHandle()))
						{
							if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
							{
								gnetDebug1("OnHandleJoinRequest: Slots full - kicking random player to make room");
								KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
							}
							else
							{
								bCanQueue = (GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup) > 1);
								gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Free activity slots reserved. Num: %d, Res: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(false), pSession->GetNumSlotsWithParam(nReservationParameter), GetActivityPlayerMax(false), bCanQueue ? "True" : "False");
								nResponseCode = RESPONSE_DENY_GROUP_FULL;
							}
						}
					}
					else
					{
						// matchmaking group full - check if the joiner can fit into the matchmaking group
						if(!CanJoinMatchmakingGroup(tJoinRequest.m_PlayerData.m_MatchmakingGroup))
						{
							if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
							{
								gnetDebug1("OnHandleJoinRequest: Group full - kicking random player to make room");
								KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
							}
							else
							{
								bCanQueue = (GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup) > 1);
								gnetDebug1("OnHandleJoinRequest: Denying join request from %s - No space in group \"%s\". Num: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), gs_szMatchmakingGroupNames[tJoinRequest.m_PlayerData.m_MatchmakingGroup], GetMatchmakingGroupNum(tJoinRequest.m_PlayerData.m_MatchmakingGroup), GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup), bCanQueue ? "True" : "False");
								nResponseCode = RESPONSE_DENY_GROUP_FULL;
							}
						}
						else if(!((GetMatchmakingGroupNum(tJoinRequest.m_PlayerData.m_MatchmakingGroup) + pSession->GetNumSlotsWithParam(nReservationParameter)) < GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup)) && !pSession->HasSlotReservation(gamerInfo.GetGamerHandle()))
						{
							if(isAdminDevInvite && m_AdminInviteKickToMakeRoom)
							{
								gnetDebug1("OnHandleJoinRequest: Slots full - kicking random player to make room");
								KickRandomPlayer(bIsSCTV, gamerInfo.GetGamerHandle(), KickReason::KR_ScAdmin);
							}
							else
							{
								bCanQueue = (GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup) > 1);
								gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Free \"%s\" group slots reserved. Num: %d, Res: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), gs_szMatchmakingGroupNames[tJoinRequest.m_PlayerData.m_MatchmakingGroup], GetMatchmakingGroupNum(tJoinRequest.m_PlayerData.m_MatchmakingGroup), pSession->GetNumSlotsWithParam(nReservationParameter), GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup), bCanQueue ? "True" : "False");
								nResponseCode = RESPONSE_DENY_GROUP_FULL;
							}
						}

						// check boss thresholds
						const unsigned numBosses = GetNumBossesInFreeroam();

						// intro flow reject sessions with too many bosses
						if(m_IntroRejectSessionsWithLargeNumberOfBosses && CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_ExpandedIntroFlow) && (m_IntroBossRejectThreshold > 0) && (numBosses >= m_IntroBossRejectThreshold))
						{
							gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Too many bosses (%u / %u) for expanded intro flow player!", gamerInfo.GetName(), numBosses, m_IntroBossRejectThreshold);
							nResponseCode = RESPONSE_DENY_TOO_MANY_BOSSES; 
						}

						// reject sessions with too many bosses
						if(m_bRejectSessionsWithLargeNumberOfBosses && CheckFlag(tJoinRequest.m_JoinRequestFlags, JoinFlags::JF_IsBoss) && (m_nBossRejectThreshold > 0) && (numBosses >= m_nBossRejectThreshold))
						{
							gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Too many bosses (%u / %u) for boss player!", gamerInfo.GetName(), numBosses, m_nBossRejectThreshold);
							nResponseCode = RESPONSE_DENY_TOO_MANY_BOSSES;
						}
					}
				}
			}
			break;

		case SessionType::ST_Transition:
			{
                // make sure we've always been told how many spectators this mode supports
                if(!gnetVerifyf(m_bHasSetActivitySpectatorsMax, "OnHandleJoinRequest: Activity session but no call to SetActivitySpectatorsMax from script!"))
                {
                    gnetError("OnHandleJoinRequest: Transition session but no call to SetActivitySpectatorsMax!");
                }

				// joiner is blacklisted
				if(m_BlacklistedGamers.IsBlacklisted(gamerInfo.GetGamerHandle()) && m_BlacklistedGamers.GetBlacklistReason(gamerInfo.GetGamerHandle()) == BLACKLIST_VOTED_OUT)
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Joiner is blacklisted", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_BLACKLISTED;
				}
				// session full - possible that we added a player between the matchmaking results coming back
				else if(m_pTransition->GetGamerCount() >= (m_Config[SessionType::ST_Transition].GetMaxPublicSlots() + m_Config[SessionType::ST_Transition].GetMaxPrivateSlots()))
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - No space in session. Count: %d, Slots: %d", gamerInfo.GetName(), m_pTransition->GetGamerCount(), (m_Config[SessionType::ST_Transition].GetMaxPublicSlots() + m_Config[SessionType::ST_Transition].GetMaxPrivateSlots()));
					nResponseCode = RESPONSE_DENY_SESSION_FULL;
				}
				// not hosting session
				else if(!m_pTransition->IsHost())
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Not Hosting", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_NOT_HOSTING;
				}
				// not ready
				else if(!(m_TransitionState >= TS_ESTABLISHING && m_TransitionState < TS_LEAVING))
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Not Ready. State: %d", gamerInfo.GetName(), static_cast<int>(m_TransitionState));
					nResponseCode = RESPONSE_DENY_NOT_READY;
				}
				// in transition to game - this won't be handled
				else if(m_bIsTransitionToGame)
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - In transition to game", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_NOT_READY;
				}
				// session join requests blocked
				else if(m_bBlockTransitionJoinRequests)
				{
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Blocking with m_bBlockTransitionJoinRequests", gamerInfo.GetName());
					nResponseCode = RESPONSE_DENY_BLOCKING;
				}
                // check we have activity slots
                if(bIsSCTV && (GetActivityPlayerNum(true) >= GetActivityPlayerMax(true)))
                {
					bCanQueue = (GetActivityPlayerMax(true) > 1);
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Maximum activity spectators. Num: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(true), GetActivityPlayerMax(true), bCanQueue ? "True" : "False");
					nResponseCode = RESPONSE_DENY_GROUP_FULL;
				}
                else if(!bIsSCTV && (GetActivityPlayerNum(false) >= GetActivityPlayerMax(false)))
                {
					bCanQueue = (GetActivityPlayerMax(false) > 1);
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Maximum activity players. Num: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(false), GetActivityPlayerMax(false), bCanQueue ? "True" : "False");
					nResponseCode = RESPONSE_DENY_GROUP_FULL;
				}
				else if(!((GetActivityPlayerNum(bIsSCTV) + pSession->GetNumSlotsWithParam(nReservationParameter)) < GetActivityPlayerMax(bIsSCTV)) && !pSession->HasSlotReservation(gamerInfo.GetGamerHandle()))
				{
					bCanQueue = (GetMatchmakingGroupMax(tJoinRequest.m_PlayerData.m_MatchmakingGroup) > 1);
					gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Free activity slots reserved. Num: %d, Res: %d, Max: %d, CanQueue: %s", gamerInfo.GetName(), GetActivityPlayerNum(false), pSession->GetNumSlotsWithParam(nReservationParameter), GetActivityPlayerMax(false), bCanQueue ? "True" : "False");
					nResponseCode = RESPONSE_DENY_GROUP_FULL;
				}
			}
			break;

		default:
			gnetAssertf(0, "OnHandleJoinRequest :: Invalid session type!");
			break;
		}

		// visibility failsafe - should be blocked out by MM but potential edge case if the attributes are not advertised promptly
		// this only blocks players who have joined via matchmaking
		if(!m_Config[sessionType].IsVisible() && !isViaInvite)
		{
			bool bException = false;
			if(IsClosedFriendsSession(sessionType) && rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), gamerInfo.GetGamerHandle()))
				bException = true;

			if(!bException && HasCrewRestrictions(sessionType))
			{
				for(int i = 0; i < MAX_UNIQUE_CREWS; i++)
				{     
					if(m_UniqueCrewData[sessionType].m_nActiveCrewIDs[i] != RL_INVALID_CLAN_ID && m_UniqueCrewData[sessionType].m_nActiveCrewIDs[i] == tJoinRequest.m_PlayerData.m_ClanId)
					{
						bException = true;
						break;
					}
				}
			}

			if(!bException)
			{
				gnetDebug1("OnHandleJoinRequest: Denying join request from %s - Not visible", gamerInfo.GetName());
				nResponseCode = RESPONSE_DENY_NOT_VISIBLE;
			}
		}
	}

	// if we passed all checks
	bool bAccept = (nResponseCode == RESPONSE_ACCEPT);

	// log acceptance
#if !__NO_OUTPUT
	if(bAccept)
		gnetDebug1("OnHandleJoinRequest: Accepting join request from %s", gamerInfo.GetName());
#endif

	// build response
	CMsgJoinResponse tResponse;
	tResponse.m_ResponseCode = nResponseCode;
	tResponse.m_bCanQueue = bCanQueue;
	tResponse.m_HostFlags = m_nHostFlags[sessionType];
	tResponse.m_SessionPurpose = (sessionType == SessionType::ST_Physical) ? (IsActivitySession() ? SessionPurpose::SP_Activity : SessionPurpose::SP_Freeroam) : SessionPurpose::SP_Activity;

	// we only bother with the detailed response if the player is being accepted into the session
	if(bAccept)
	{
		// if accepting, add a pending joiner
		if(m_PendingJoinerEnabled)
		{
			m_PendingJoiners[sessionType].Add(gamerInfo.GetGamerHandle(), tJoinRequest.m_JoinRequestFlags);
			gnetDebug2("OnHandleJoinRequest: Added Pending Joiner - %s, Flags: 0x%x, Num: %d", gamerInfo.GetGamerHandle().ToString(), tJoinRequest.m_JoinRequestFlags, m_PendingJoiners[sessionType].m_Joiners.GetCount());
		}

        // crews 
        tResponse.m_UniqueCrewLimit = m_UniqueCrewData[sessionType].m_nLimit;
        tResponse.m_CrewsOnly = m_UniqueCrewData[sessionType].m_bOnlyCrews;
        tResponse.m_MaxMembersPerCrew = m_UniqueCrewData[sessionType].m_nMaxCrewMembers;

		// different response messages depending on request
		if(tResponse.m_SessionPurpose == SessionPurpose::SP_Freeroam)
		{
			// store time/weather info so the joiner can sync up if they are accepted into the session
            tResponse.m_HostTimeToken = CNetwork::GetNetworkClock().GetToken();
			tResponse.m_HostTime = CNetwork::GetNetworkTime();
			tResponse.m_ModelVariationID = gPopStreaming.GetNetworkModelVariationID();
			tResponse.m_TimeCreated = m_TimeCreated[SessionType::ST_Physical];

			// matchmaking group information
			tResponse.m_NumActiveMatchmakingGroups = Min(static_cast<unsigned>(MAX_ACTIVE_MM_GROUPS), m_nActiveMatchmakingGroups);
			for(int i = 0; i < m_nActiveMatchmakingGroups; i++)
			{
				tResponse.m_ActiveMatchmakingGroup[i] = static_cast<unsigned>(m_ActiveMatchmakingGroups[i]);
				tResponse.m_MatchmakingGroupMax[i] = m_MatchmakingGroupMax[m_ActiveMatchmakingGroups[i]];
			}
		}
		else if(tResponse.m_SessionPurpose == SessionPurpose::SP_Activity)
		{
			tResponse.m_bHasLaunched = IsActivitySession();
			tResponse.m_ActivitySpectatorsMax = GetActivityPlayerMax(true);
			tResponse.m_ActivityPlayersMax = GetActivityPlayerMax(false);
			tResponse.m_TimeCreated = m_TimeCreated[SessionType::ST_Transition];

			if(tResponse.m_bHasLaunched)
			{
                tResponse.m_HostTimeToken = CNetwork::GetNetworkClock().GetToken();
				tResponse.m_HostTime = CNetwork::GetNetworkTime();
				tResponse.m_ModelVariationID = gPopStreaming.GetNetworkModelVariationID();
			}
		}
	}

	// export response data
	if(!tResponse.Export(pRequest->m_ResponseData, sizeof(pRequest->m_ResponseData), &pRequest->m_SizeofResponseData))
	{
		gnetAssertf(0, "OnHandleJoinRequest: Failed exporting join response data!");
		return false;
	}

    return bAccept;
}

bool CNetworkSession::OnHandleGameJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest)
{
	return OnHandleJoinRequest(pSession, gamerInfo, pRequest, SessionType::ST_Physical);
}

bool CNetworkSession::OnHandleTransitionJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest)
{
	return OnHandleJoinRequest(pSession, gamerInfo, pRequest, SessionType::ST_Transition);
}

void CNetworkSession::OnGetGameGamerData(snSession*, const rlGamerInfo& gamerInfo, snGetGamerData* pData) const
{
	pData->m_SizeofData = NetworkInterface::GetPlayerMgr().GetPlayerData(gamerInfo.GetGamerId(),
                                                                         pData->m_Data,
                                                                         sizeof(pData->m_Data));
}

// used for sorting rlPeerInfos.
struct PeerPred
{
	bool operator()(const rlPeerInfo& a, const rlPeerInfo& b) const
	{
		return a.GetPeerId() < b.GetPeerId();
	}
};

bool CNetworkSession::ShouldMigrate()
{
#if !__FINAL
    if(PARAM_netSessionBailMigrate.Get())
    {
        gnetDebug1("ShouldMigrate :: Bailing next frame due to a failed host migration");
        m_nBailReason = BAIL_SESSION_MIGRATE_FAILED;
        SetSessionState(SS_BAIL);
        return false;
    }
#endif 

	// being kicked - we're bailing anyway
	if(m_SessionState == SS_HANDLE_KICK)
		return false;

	// already leaving the session
	if(IsSessionLeaving())
		return false;

	// not if we're launching a transition
	if(m_bLaunchingTransition)
		return false;

	return true;
}

bool CNetworkSession::ShouldMigrateTransition()
{
	// already leaving the session
	if(IsTransitionLeaving())
		return false;

    // not if we're transitioning to game as a lone player
    if(m_bIsTransitionToGame && !m_hTransitionHostGamer.IsValid())
        return false;

	return true;
}

unsigned CNetworkSession::GetMigrationCandidates(snSession* pSession, rlPeerInfo* pCandidates, const unsigned ASSERT_ONLY(nMaxCandidates))
{
	gnetDebug1("GetMigrationCandidates");

    unsigned nCandidates = 0;
	bool bHasNonEstablishedCandidates = false;

	gnetDebug1("\tGetMigrationCandidates :: EstablishedPool: %s", m_bMigrationSessionNoEstablishedPools ? "False" : "True");

	// get peer info
	rlGamerInfo tGamerInfo[RL_MAX_GAMERS_PER_SESSION];
	const unsigned nGamers = pSession->GetGamers(tGamerInfo, COUNTOF(tGamerInfo));

	// check all candidates
	for(unsigned i = 0; i < nGamers; i++)
	{
		// skip if flagged as not migrating
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())))
		{
			gnetDebug1("\tGetMigrationCandidates :: Rejecting %s. Migration not Allowed!", tGamerInfo[i].GetName());
			continue;
		}
		
		CNetGamePlayer*	pPlayer = NetworkInterface::GetPlayerFromPeerId(tGamerInfo[i].GetPeerInfo().GetPeerId());
		
		// we don't want players we're locally disconnected from or who indicated that they are launching a transition
		if(pPlayer && pPlayer->IsLocallyDisconnected())
		{
			gnetDebug1("\tGetMigrationCandidates :: Rejecting %s. Locally Disconnected!", tGamerInfo[i].GetName());
			continue; 
		}

		// we don't want players we're locally disconnected from or who indicated that they are launching a transition
		if(pPlayer && pPlayer->IsLaunchingTransition())
		{
			gnetDebug1("\tGetMigrationCandidates :: Rejecting %s. Launching Transition!", tGamerInfo[i].GetName());
			continue; 
		}

		// only established gamers on this loop
		if(!m_bMigrationSessionNoEstablishedPools && !pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()))
		{
			bHasNonEstablishedCandidates = true; 
			gnetDebug1("\tGetMigrationCandidates :: Rejecting %s. Not Established!", tGamerInfo[i].GetName());
			continue;
		}

		gnetDebug1("\tGetMigrationCandidates :: Accepting %s, PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	unsigned nCandidateMark = nCandidates;

	// sort established candidates
	if(nCandidates > 1)
		std::sort(&pCandidates[0], &pCandidates[nCandidates], PeerPred());

	// add established, ordered candidates
	if(m_SendMigrationTelemetry) for(unsigned i = 0; i < nCandidates; i++)
	{
		for(unsigned j = 0; j < nGamers; j++)
		{
			if(tGamerInfo[j].GetPeerInfo() == pCandidates[i])
				m_MigratedTelemetry.AddCandidate(tGamerInfo[j].GetName(), MetricSessionMigrated::CANDIDATE_CODE_ESTABLISHED);
		}
	}

	if(bHasNonEstablishedCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// silently break - we logged all this information already
		CNetGamePlayer*	pPlayer = NetworkInterface::GetPlayerFromPeerId(tGamerInfo[i].GetPeerInfo().GetPeerId());
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || (pPlayer && (pPlayer->IsLocallyDisconnected() || pPlayer->IsLaunchingTransition())))
			continue;

		// skip established candidates that were added above
		if(pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidates :: Accepting Non-Established candidate %s, PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort non-established candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// add not established, ordered candidates
	if(m_SendMigrationTelemetry) for(unsigned i = nCandidateMark; i < nCandidates; i++)
	{
		for(unsigned j = 0; j < nGamers; j++)
		{
			if(tGamerInfo[j].GetPeerInfo() == pCandidates[i])
				m_MigratedTelemetry.AddCandidate(tGamerInfo[j].GetName(), MetricSessionMigrated::CANDIDATE_CODE_NOT_ESTABLISHED);
		}
	}

	// add candidates that we aren't considering
	if(m_SendMigrationTelemetry) for(unsigned i = 0; i < nGamers; i++)
	{
		// skip if flagged as not migrating
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())))
		{
			m_MigratedTelemetry.AddCandidate(tGamerInfo[i].GetName(), MetricSessionMigrated::CANDIDATE_CODE_NOT_MIGRATING);
			continue;
		}

		CNetGamePlayer*	pPlayer = NetworkInterface::GetPlayerFromPeerId(tGamerInfo[i].GetPeerInfo().GetPeerId());

		// we don't want players we're locally disconnected from or who indicated that they are launching a transition
		if(pPlayer && pPlayer->IsLocallyDisconnected())
		{
			m_MigratedTelemetry.AddCandidate(tGamerInfo[i].GetName(), MetricSessionMigrated::CANDIDATE_CODE_LOCALLY_DISCONNECTED);
			continue; 
		}

		// we don't want players we're locally disconnected from or who indicated that they are launching a transition
		if(pPlayer && pPlayer->IsLaunchingTransition())
		{
			m_MigratedTelemetry.AddCandidate(tGamerInfo[i].GetName(), MetricSessionMigrated::CANDIDATE_CODE_LAUNCHING_TRANSITION);
			continue; 
		}
	}

	// log out migration candidates in order
#if !__NO_OUTPUT
	for(unsigned i = 0; i < nCandidates; i++)
	{
		rlGamerInfo hGamer[1];
		pSession->GetGamers(pCandidates[i].GetPeerId(), hGamer, 1);
		gnetDebug1("\tGetMigrationCandidates :: Candidate %d is %s", i + 1, hGamer->GetName());
	}
#endif

	// verify we haven't blown limit
	ASSERT_ONLY(gnetAssertf(nCandidates < nMaxCandidates, "\tGetMigrationCandidates :: Added too many candidates!");)

	return nCandidates;
}

unsigned CNetworkSession::GetMigrationCandidatesTransition(snSession* pSession, rlPeerInfo* pCandidates, const unsigned ASSERT_ONLY(nMaxCandidates))
{
	gnetDebug1("GetMigrationCandidatesTransition");

	bool bNoEstablishedPools = m_bMigrationTransitionNoEstablishedPools;
	bool bNoSpectatorPools = m_bMigrationTransitionNoSpectatorPools;
	if(m_bMigrationTransitionNoEstablishedPoolsForToGame || m_bMigrationTransitionNoSpectatorPoolsForToGame)
	{
		static const unsigned TRANSITION_TO_GAME_COOLDOWN = 10 * 1000;
		if(m_nToGameCompletedTimestamp > 0 && (sysTimer::GetSystemMsTime() - m_nToGameCompletedTimestamp < TRANSITION_TO_GAME_COOLDOWN))
		{
			gnetDebug1("\tGetMigrationCandidatesTransition :: In transition cooldown!");
			bNoEstablishedPools |= m_bMigrationTransitionNoEstablishedPoolsForToGame;
			bNoSpectatorPools |= m_bMigrationTransitionNoSpectatorPoolsForToGame;
		}
	}

	gnetDebug1("\tGetMigrationCandidatesTransition :: EstablishedPool: %s, SpectatorPool: %s, AsyncPool: %s", bNoEstablishedPools ? "False" : "True", bNoSpectatorPools ? "False" : "True", m_bMigrationTransitionNoAsyncPools ? "False" : "True");

	unsigned nCandidates = 0;
	unsigned nCandidateMark = 0;
	bool bHasSpectatorCandidates = false;
	bool bHasAsyncCandidates = false; 
	bool bHasNonEstablishedCandidates = false;

	// get peer info
	rlGamerInfo tGamerInfo[RL_MAX_GAMERS_PER_SESSION];
	const unsigned nGamers = pSession->GetGamers(tGamerInfo, COUNTOF(tGamerInfo));

	// check all candidates
	for(unsigned i = 0; i < nGamers; i++)
	{
		// skip players who have been flagged to not allow migration
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())))
		{
			gnetDebug1("\tGetMigrationCandidatesTransition :: Rejecting %s. Migration not Allowed!", tGamerInfo[i].GetName());
			continue;
		}

		// only non spectator gamers on this loop
		if(!bNoSpectatorPools && IsActivitySpectator(tGamerInfo[i].GetGamerHandle()))
		{
			bHasSpectatorCandidates = true;
			continue;
		}

		// only non async gamers on this loop
		if(!m_bMigrationTransitionNoAsyncPools && IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
		{
			bHasAsyncCandidates = true;
			continue;
		}

		// only established gamers on this loop
		if(!bNoEstablishedPools && !pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()))
		{
			bHasNonEstablishedCandidates = true; 
			gnetDebug1("\tGetMigrationCandidatesTransition :: Rejecting %s. Not Established!", tGamerInfo[i].GetName());
			continue;
		}

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Established, Non-Async, Non-Spectator) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort established non-spectator candidates
	if(nCandidates > 1)
		std::sort(&pCandidates[0], &pCandidates[nCandidates], PeerPred());
	
	// take a mark
	nCandidateMark = nCandidates;

	// add established async players
	if(bHasAsyncCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// no logging this time
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || !pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()) || IsActivitySpectator(tGamerInfo[i].GetGamerHandle()) || !IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Established, Async) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort established async candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// take a mark
	nCandidateMark = nCandidates;

	// add established spectators
	if(bHasSpectatorCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// no logging this time
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || !pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()) || !IsActivitySpectator(tGamerInfo[i].GetGamerHandle()) || IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Established, Spectator) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort established spectator candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// take a mark
	nCandidateMark = nCandidates;

	// add non-established non-spectators
	if(bHasNonEstablishedCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// no logging this time
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()) || IsActivitySpectator(tGamerInfo[i].GetGamerHandle()) || IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Non-Established, Non-Async, Non-Spectator) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort non-established non-spectator candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// take a mark
	nCandidateMark = nCandidates;

	// add non-established async
	if(bHasNonEstablishedCandidates && bHasAsyncCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// no logging this time
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()) || IsActivitySpectator(tGamerInfo[i].GetGamerHandle()) || !IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Non-Established, Async) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort non-established async candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// take a mark
	nCandidateMark = nCandidates;

	// finally, add non-established spectators
	if(bHasNonEstablishedCandidates && bHasSpectatorCandidates) for(unsigned i = 0; i < nGamers; i++)
	{
		// no logging this time
		if((tGamerInfo[i].IsRemote() && !pSession->DoesAllowMigration(tGamerInfo[i].GetPeerInfo().GetPeerId())) || pSession->IsGamerEstablished(tGamerInfo[i].GetGamerHandle()) || !IsActivitySpectator(tGamerInfo[i].GetGamerHandle()) || IsTransitionMemberAsync(tGamerInfo[i].GetGamerHandle()))
			continue;

		gnetDebug1("\tGetMigrationCandidatesTransition :: Accepting %s (Non-Established, Spectator) PeerID: 0x%" I64FMT "x", tGamerInfo[i].GetName(), tGamerInfo[i].GetPeerInfo().GetPeerId());
		pCandidates[nCandidates++] = tGamerInfo[i].GetPeerInfo();
	}

	// sort non-established spectator candidates
	if((nCandidates - nCandidateMark) > 1)
		std::sort(&pCandidates[nCandidateMark], &pCandidates[nCandidates], PeerPred());

	// take a mark
	nCandidateMark = nCandidates;

	// log out migration candidates in order
#if !__NO_OUTPUT
	for(unsigned i = 0; i < nCandidates; i++)
	{
		rlGamerInfo hGamer[1];
		pSession->GetGamers(pCandidates[i].GetPeerId(), hGamer, 1);
		gnetDebug1("\tGetMigrationCandidatesTransition :: Candidate %d is %s", i + 1, hGamer->GetName());
	}
#endif

	// verify we haven't blown limit
	ASSERT_ONLY(gnetAssertf(nCandidates < nMaxCandidates, "\tGetMigrationCandidatesTransition :: Added too many candidates!");)

	return nCandidates;
}

void CNetworkSession::SendMigrateMessages(const u64 nPeerID, snSession* pSession, const bool bStarted)
{
	// let remote gamer know if we've already started
	if(bStarted)
	{
		MsgSessionEstablished msgEstablished(pSession->GetSessionId(), NetworkInterface::GetLocalGamerHandle());
		pSession->SendMsg(nPeerID, msgEstablished, NET_SEND_RELIABLE);
	}

	// also send a request to find out if they have started
	// this eliminates the possibility of staggered starts meaning that we miss this information
	MsgSessionEstablishedRequest msgEstablishedRequest(pSession->GetSessionId());
	pSession->SendMsg(nPeerID, msgEstablishedRequest, NET_SEND_RELIABLE);
}

void CNetworkSession::ProcessAddingGamerEvent(snEventAddingGamer* pEvent)
{
	// grab this once
	const rlGamerInfo& tGamerInfo = pEvent->m_GamerInfo;

	// grab the incoming data
	unsigned nMessageID;
	if(!gnetVerify(netMessage::GetId(&nMessageID, pEvent->m_Data, pEvent->m_SizeofData)))
	{
		gnetError("ProcessAddingGamerEvent :: Failed to get message ID!");
		return;
	}

	CNetGamePlayerDataMsg tPlayerData;
	if(CNetGamePlayerDataMsg::MSG_ID() == nMessageID)
	{
		// we receive this when getting messages from existing session players
		if(!gnetVerify(tPlayerData.Import(pEvent->m_Data, pEvent->m_SizeofData)))
		{
			gnetError("ProcessAddingGamerEvent :: Failed to import player message data!");
			return; 
		}
	}
	else if(CMsgJoinRequest::MSG_ID() == nMessageID)
	{
		// we receive this as host when a client joins
		CMsgJoinRequest tJoinRequest;
		if(!gnetVerify(tJoinRequest.Import(pEvent->m_Data, pEvent->m_SizeofData)))
		{
			gnetError("ProcessAddingGamerEvent :: Failed to import join request data!");
			return; 
		}

		// join request contains player data
		tPlayerData = tJoinRequest.m_PlayerData;

		// allocate non-physical player data
		CNonPhysicalPlayerData* pNonPhysicalData = TryAllocateNonPhysicalPlayerData();
		if(!gnetVerify(pNonPhysicalData))
		{	
			gnetError("ProcessAddingGamerEvent :: Failed to allocate non physical player data! %u members, %u active players!", m_pSession->GetGamerCount(), CNetwork::GetPlayerMgr().GetNumActivePlayers());
			return;
		}

		// add temp player
		CNetwork::GetPlayerMgr().AddTemporaryPlayer(tGamerInfo, pEvent->m_EndpointId, tPlayerData, pNonPhysicalData);
	}
	else
	{
		gnetAssertf(false, "ProcessAddingGamerEvent with unexpected message ID");
	}

	// logging
	gnetDebug1("\tAddingGamer :: Name: %s, Group: %s", pEvent->m_GamerInfo.GetName(), gs_szMatchmakingGroupNames[tPlayerData.m_MatchmakingGroup]);

#if RSG_DURANGO
	// send migrate messages
	SendMigrateMessages(tGamerInfo.GetPeerInfo().GetPeerId(), m_pSession, m_SessionState >= SS_ESTABLISHED);

	// add peer entry
	if(tGamerInfo.IsRemote())
		m_PeerComplainer[SessionType::ST_Physical].AddPeer(tGamerInfo.GetPeerInfo().GetPeerId() OUTPUT_ONLY(, tGamerInfo.GetName()));
#endif

	// update matchmaking groups, crew and beacon
	UpdateMatchmakingGroups();
	UpdateCurrentUniqueCrews(SessionType::ST_Physical);
	m_nMatchmakingBeacon[SessionType::ST_Physical] = 0;

	if(m_pSession == GetCurrentSession())
		RemoveQueuedJoinRequest(pEvent->m_GamerInfo.GetGamerHandle());
}

void CNetworkSession::ProcessAddedGamerEvent(snEventAddedGamer* pEvent)
{
    // add player to voice
    NetworkInterface::GetVoice().PlayerHasJoined(pEvent->m_GamerInfo, pEvent->m_EndpointId);
	
    // if we're leaving, don't bother
	if(IsSessionLeaving())
		return;

	// grab the incoming data
	unsigned nMessageID;
	if(!gnetVerify(netMessage::GetId(&nMessageID, pEvent->m_Data, pEvent->m_SizeofData)))
	{
		gnetError("\tAddedGamer :: Failed to get message ID!");
		return;
	}

	CNetGamePlayerDataMsg tPlayerData;
	if(CNetGamePlayerDataMsg::MSG_ID() == nMessageID)
	{
		// we receive this when getting messages from existing session players
		if(!gnetVerify(tPlayerData.Import(pEvent->m_Data, pEvent->m_SizeofData)))
		{
			gnetError("\tAddedGamer :: Failed to import player message data!");
			return; 
		}
	}
	else if(CMsgJoinRequest::MSG_ID() == nMessageID)
	{
		// we receive this as host when a client joins
		CMsgJoinRequest tJoinRequest;
		if(!gnetVerify(tJoinRequest.Import(pEvent->m_Data, pEvent->m_SizeofData)))
		{
			gnetError("\tAddedGamer :: Failed to import join request data!");
			return; 
		}

		// join request contains player data
		tPlayerData = tJoinRequest.m_PlayerData;
	}

	// send relevant telemetry
	SendAddedGamerTelemetry(IsActivitySession(), m_pSession->GetSessionId(), pEvent, tPlayerData.m_NatType);

    // logging
    gnetDebug1("\tAddedGamer :: Name: %s, Handle: %s, Addr: " NET_ADDR_FMT ", Crew: %" I64FMT "d, Group: %s, Region: %u, NAT: %s", 
			   pEvent->m_GamerInfo.GetName(), 
			   NetworkUtils::LogGamerHandle(pEvent->m_GamerInfo.GetGamerHandle()),
			   NET_IS_VALID_ENDPOINT_ID(pEvent->m_EndpointId) ?
			   NET_ADDR_FOR_PRINTF(CNetwork::GetConnectionManager().GetAddress(pEvent->m_EndpointId)) : "", 
			   tPlayerData.m_ClanId, 
			   gs_szMatchmakingGroupNames[tPlayerData.m_MatchmakingGroup], 
			   tPlayerData.m_Region,
			   netHardware::GetNatTypeString(tPlayerData.m_NatType));

	// grab this once
	const rlGamerInfo& tGamerInfo = pEvent->m_GamerInfo;

	// allocate non-physical player data
	CNonPhysicalPlayerData* pNonPhysicalData = TryAllocateNonPhysicalPlayerData();
	if(!gnetVerify(pNonPhysicalData))
	{	
		gnetError("\tAddedGamer :: Failed to allocate non physical player data! %u members, %u active players!", m_pSession->GetGamerCount(), CNetwork::GetPlayerMgr().GetNumActivePlayers());
		return;
	}

#if ENABLE_NETWORK_BOTS
    if(tPlayerData.m_PlayerType == CNetGamePlayer::NETWORK_BOT)
    {
        if(tGamerInfo.IsRemote())
            CNetworkBot::AddRemoteNetworkBot(tGamerInfo);

        // add network bot
        CNetwork::GetPlayerMgr().AddNetworkBot(tGamerInfo, pNonPhysicalData);
    }
	else
#endif // ENABLE_NETWORK_BOTS
	{
		// work out if this player is the host
		u64 nHostPeerID = 0;
		bool bJoinerIsHost = false;
		if(!m_pSession->IsHost())
		{
			bool bHaveHostPeer = m_pSession->GetHostPeerId(&nHostPeerID);
			if(bHaveHostPeer && (nHostPeerID == tGamerInfo.GetPeerInfo().GetPeerId()))
				bJoinerIsHost = true;
		}

		// remove from pending joiner list
		if(m_PendingJoiners[SessionType::ST_Physical].Remove(tGamerInfo.GetGamerHandle()))
		{
			gnetDebug2("\tAddedGamer: Removed Pending Joiner - %s, Num: %d", tGamerInfo.GetGamerHandle().ToString(), m_PendingJoiners[SessionType::ST_Physical].m_Joiners.GetCount());
		}

		// if the host has added themselves to the session before we've fully joined - reset the peer complainer now
		if(bJoinerIsHost && !m_PeerComplainer[SessionType::ST_Physical].IsInitialized() && (m_SessionState == SS_JOINING))
			InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession);

		// add this player
		CNetwork::GetPlayerMgr().AddPlayer(tGamerInfo, pEvent->m_EndpointId, tPlayerData, pNonPhysicalData);
        
		// add to live manager for tracking
		CLiveManager::AddPlayer(tGamerInfo);

		// add from player card remote corona players
		if (SPlayerCardManager::IsInstantiated() && IsTransitionMember( pEvent->m_AddedGamer->m_GamerInfo.GetGamerHandle()))
		{
			//This can happen for players joining.
			CPlayerCardManagerCurrentTypeHelper helper(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);

			BasePlayerCardDataManager* coronamgr = SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);
			if (gnetVerify(coronamgr))
			{
				netPlayerCardDebug("[corona_sync] AddGamerHandle '%s'.", NetworkUtils::LogGamerHandle(pEvent->m_GamerInfo.GetGamerHandle()));
				static_cast< CScriptedCoronaPlayerCardDataManager* >(coronamgr)->AddGamerHandle(pEvent->m_GamerInfo.GetGamerHandle(), pEvent->m_GamerInfo.GetName());			
			}
		}

		// add met gamer and peer entry
        if(tGamerInfo.IsRemote())
        {
            m_RecentPlayers.AddRecentPlayer(tGamerInfo);

#if !RSG_DURANGO
			// player will have been added in the adding gamer above
			m_PeerComplainer[SessionType::ST_Physical].AddPeer(tGamerInfo.GetPeerInfo().GetPeerId() OUTPUT_ONLY(, tGamerInfo.GetName()));
#endif
		}

        // if we're not the host and this player is the host...
        if(bJoinerIsHost && NetworkInterface::GetActiveGamerInfo())
        {
            MsgRadioStationSyncRequest msg;
			msg.Reset(m_pSession->GetSessionId(), NetworkInterface::GetActiveGamerInfo()->GetGamerId());
            m_pSession->SendMsg(nHostPeerID, msg, NET_SEND_RELIABLE);
            BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());

            // grab the aim type for the host
            m_nHostAimPreference = tPlayerData.m_AimPreference;

            // if we're joining multiple sessions and the host connection is on relay, give this some additional time to complete
			if(m_bIsJoiningViaMatchmaking && m_GameMMResults.m_nNumCandidates > 1)
            {
                if(CNetwork::GetConnectionManager().GetAddress(pEvent->m_EndpointId).IsRelayServerAddr())
                {
					gnetDebug1("\tAddedGamer :: Host on relay connection. Bumping timeout!");
                    SetSessionTimeout(s_TimeoutValues[TimeoutSetting::Timeout_MatchmakingOneSession]);
                }
            }
        }
    }

#if !RSG_DURANGO
	// send migrate messages - this will have already happened in ADDING_GAMER on Xbox
	SendMigrateMessages(tGamerInfo.GetPeerInfo().GetPeerId(), m_pSession, m_SessionState >= SS_ESTABLISHED);
#endif

    // is this a gamer that left for the store
    int nGamerStoreIndex = FindGamerInStore(tGamerInfo.GetGamerHandle());
    if(nGamerStoreIndex >= 0)
    {
        gnetDebug1("\tAddedGamer :: Gamer has returned from the store. Removing tracked entry!");
        m_GamerInStore.Delete(nGamerStoreIndex);
    }

	// update matchmaking data mine data
	if(!tGamerInfo.IsLocal())
	{
		m_MatchmakingInfoMine.m_nSessionPlayersAdded++;
		m_MatchmakingInfoMine.m_LastPlayerAddedPosix = static_cast<u32>(rlGetPosixTime());
	}

    // update matchmaking groups, crew and beacon
    UpdateMatchmakingGroups();
	UpdateCurrentUniqueCrews(SessionType::ST_Physical);
    m_nMatchmakingBeacon[SessionType::ST_Physical] = 0;
}

#if RSG_DURANGO
void CNetworkSession::ProcessDisplayNameEvent(snEventSessionDisplayNameRetrieved* pEvent)
{
	const rlGamerInfo& tGamerInfo = pEvent->m_GamerInfo;
	const rlGamerId& tGamerId = tGamerInfo.GetGamerId();
	CNetGamePlayer* pPlayer = static_cast<CNetGamePlayer*>(CNetwork::GetPlayerMgr().GetPlayerFromGamerId(tGamerId));
	if (pPlayer)
	{
		pPlayer->SetDisplayName((rlDisplayName*)tGamerInfo.GetDisplayName());
	}
}
#endif

void CNetworkSession::ProcessRemovedGamerEvent(snEventRemovedGamer* pEvent)
{
    // remove player from voice
    const rlGamerInfo& tGamerInfo = pEvent->m_GamerInfo;
    NetworkInterface::GetVoice().PlayerHasLeft(tGamerInfo);

#if RSG_PC
	//remove from text chat
	NetworkInterface::GetTextChat().PlayerHasLeft(tGamerInfo);
#endif

    // if we're leaving, don't bother
	if(IsSessionLeaving())
		return;

	// grab these once
	const rlGamerId& tGamerId = tGamerInfo.GetGamerId();

	// what kind of player
	if(tGamerInfo.IsRemote())
	{
		CNetGamePlayer* pPlayer = static_cast<CNetGamePlayer*>(CNetwork::GetPlayerMgr().GetPlayerFromGamerId(tGamerId));
		if(pPlayer)
		{
			pPlayer->SetSessionBailReason(static_cast<eBailReason>(pEvent->m_RemoveReason));

#if ENABLE_NETWORK_BOTS
			if(pPlayer->GetPlayerType() == CNetGamePlayer::NETWORK_BOT)
			{
				CNetworkBot* pNetworkBot = CNetworkBot::GetNetworkBot(tGamerInfo);
				if(gnetVerifyf(pNetworkBot, "ProcessRemovedGamerEvent :: Network bot does not exist!"))
					CNetworkBot::RemoveNetworkBot(pNetworkBot);
			}
#endif // ENABLE_NETWORK_BOTS
		}

		// remove the peer if we have it (possible that we don't on XBO if we're still processing a display name request)
		const u64 nPeerID = tGamerInfo.GetPeerInfo().GetPeerId();
		if(m_PeerComplainer[SessionType::ST_Physical].HavePeer(nPeerID))
			m_PeerComplainer[SessionType::ST_Physical].RemovePeer(nPeerID);
	}

	// get this players matchmaking group
	eMatchmakingGroup nGroup = MM_GROUP_INVALID;
	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(tGamerId);
	if(pPlayer)
	{
		nGroup = pPlayer->GetMatchmakingGroup();
		gnetAssertf(nGroup != MM_GROUP_INVALID, "RemovePlayer :: Player does not have valid matchmaking group!");
	}

	// check if we've been left on our own (as the host, we'll pick this up in migrate as a client)
	if(!IsSessionLeaving() && (m_pSession->GetChannelId() == NETWORK_SESSION_GAME_CHANNEL_ID) && IsHost() && m_pSession->GetGamerCount() == 1)
	{
		// write solo session telemetry
		gnetDebug1("ProcessRemovedGamerEvent :: Sending solo telemetry - Reason: %s, Visibility: %s", GetSoloReasonAsString(SOLO_SESSION_PLAYERS_LEFT), GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Physical)));
		CNetworkTelemetry::EnteredSoloSession(m_pSession->GetSessionId(), 
											  m_pSession->GetSessionToken().GetValue(), 
											  m_Config[SessionType::ST_Physical].GetGameMode(), 
											  SOLO_SESSION_PLAYERS_LEFT, 
											  GetSessionVisibility(SessionType::ST_Physical),
											  SOLO_SOURCE_PLAYERS_LEFT,
											  pPlayer ? (pPlayer->IsLaunchingTransition() ? 1 : 0) : 0,
											  pPlayer ? (pPlayer->IsLocallyDisconnected() ? 1 : 0) : 0,
											  0);

		// increase solo count and set solo reason
		m_MatchmakingInfoMine.m_nSoloReason = static_cast<u8>(SOLO_SESSION_PLAYERS_LEFT); 
		m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesSolo);
	}

	// remove from live manager tracking
	CLiveManager::RemovePlayer(tGamerInfo);

	// remove this player
	CNetwork::GetPlayerMgr().RemovePlayer(tGamerInfo);

	// remove from player card remote corona players
	if (SPlayerCardManager::IsInstantiated())
	{
		//This can happen for players joining.
		CPlayerCardManagerCurrentTypeHelper helper(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);

		BasePlayerCardDataManager* coronamgr = SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);
		if (gnetVerify(coronamgr))
		{
			netPlayerCardDebug("[corona_sync] RemoveGamerHandle '%s'.", NetworkUtils::LogGamerHandle(pEvent->m_GamerInfo.GetGamerHandle()));
			static_cast< CScriptedCoronaPlayerCardDataManager* >(coronamgr)->RemoveGamerHandle(pEvent->m_GamerInfo.GetGamerHandle());
		}
	}

	// update friends session status
	const rlGamerHandle& gh = pEvent->m_GamerInfo.GetGamerHandle();
	if (rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), gh))
	{
		// if the friend is on the current page.
		if (CLiveManager::GetFriendsPage())
		{
			rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(gh);
			if (pFriend && pFriend->IsInSession())
			{
				gnetDebug3("ProcessRemovedGamerEvent :: Friend %s leaving our session.", NetworkUtils::LogGamerHandle(gh));

				// We know they are leaving our session, so we've just immediately nuked their 'is in session' status. 
				// Queue up a new read for the friend so we can see if they left our session for another MP session, or for SP.
				// For PSN, we can get that friend directly. For XBox/PC, if they are on our current page, refresh that page. If they
				// aren't on our current page, this code won't be hit and the next UI paging action will update the data.
				if (m_bImmediateFriendSessionRefresh)
				{
#if RSG_NP
					rlGamerHandle hFriend;
					pFriend->GetGamerHandle(&hFriend);

					gnetDebug3("ProcessRemovedGamerEvent :: Refreshing presence status for %s", hFriend.GetNpOnlineId().data);
					g_rlNp.GetWebAPI().GetPresence(NetworkInterface::GetLocalGamerIndex(), hFriend);
#elif RSG_DURANGO || RSG_PC
					gnetDebug3("ProcessRemovedGamerEvent :: Forcing refresh of friend presence status for the main page");
					rlFriendsManager::ForceAllowFriendsRefresh();
					if (NetworkInterface::HasValidRosCredentials() && 
						rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
					{
						rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(), CLiveManager::GetFriendsPage(), 0, CPlayerListMenuDataPaginator::FRIEND_UI_PAGE_SIZE, true);
					}
#endif
				}
			}
			else
			{
				gnetDebug3("ProcessRemovedGamerEvent :: Friend %s left session, was not part of current friends page, updating reference only.", NetworkUtils::LogGamerHandle(gh));
			}
		}
	}

	// clear out invite tracking
	if(m_InvitedPlayers.DidInvite(tGamerInfo.GetGamerHandle()))
	{
		gnetDebug1("\tRemovedGamer :: Removed Invite. Name: %s", tGamerInfo.GetName());
		m_InvitedPlayers.RemoveInvite(tGamerInfo.GetGamerHandle());
	}

	// clear our the transition host tracking if this player was handling it
	if(!m_bIsTransitionToGame && m_hTransitionHostGamer == tGamerInfo.GetGamerHandle())
	{
        gnetDebug1("\tRemovedGamer :: Was transition host. Resetting"); 
		m_hTransitionHostGamer.Clear();
	}

	// updating matchmaking data mine data
	m_MatchmakingInfoMine.m_nSessionPlayersRemoved++;

    // update matchmaking groups
    UpdateMatchmakingGroups();

	// update crew counts
	UpdateCurrentUniqueCrews(SessionType::ST_Physical);

	// check queued requests
	CheckForQueuedJoinRequests();
}

void CNetworkSession::ProcessSessionMigrateStartEvent(snEventSessionMigrateStart* UNUSED_PARAM(pEvent))
{
	// pause the network timer for the duration of the migration 
	CNetwork::GetNetworkClock().Pause();

	// grab matchmaking started time
	m_MigrationStartedTimestamp = sysTimer::GetSystemMsTime();
	m_MigrationStartPlayerCount = m_pSession->GetGamerCount();

	// start pulling the migration telemetry
	if(m_SendMigrationTelemetry)
		m_MigratedTelemetry.Start(m_pSession->GetSessionToken().GetValue(), static_cast<u32>(rlGetPosixTime()), m_pSession->GetConfig().m_Attrs.GetGameMode());

#if RSG_DURANGO
	// if we've established, flag that we're leaving this MPSD session
	if(m_pSession->IsEstablished())
	{
		WriteMultiplayerEndEventSession(false OUTPUT_ONLY(, "MigrateStart"));
		m_bMigrationSentMultiplayerEndEvent = true; 
	}
	else
	{
		m_bMigrationSentMultiplayerEndEvent = false;
	}
#endif
}

void CNetworkSession::ProcessSessionMigrateEndEvent(snEventSessionMigrateEnd* pEvent)
{
	// unpause the network timer, we'll restart with the new host
	CNetwork::GetNetworkClock().Unpause();

    // reset this whether we process further or not
    m_PendingHostPhysical = false;

	// if we're leaving, don't bother
	if(IsSessionLeaving())
		return;

	// record when this happened
	m_LastMigratedPosix[SessionType::ST_Physical] = rlGetPosixTime();

	// update matchmaking info mine
	unsigned nMigrationTime = sysTimer::GetSystemMsTime() - m_MigrationStartedTimestamp;
	unsigned nMigrationTime_s = nMigrationTime / 1000;
	int nMigrationChurn = static_cast<int>(m_MigrationStartPlayerCount) - static_cast<int>(m_pSession->GetGamerCount());

	// cumulative migration tracking
	if(m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nMigrationCount))
	{
		m_MatchmakingInfoMine.m_nMigrationTimeTaken_s_Rolling = static_cast<u16>((((m_MatchmakingInfoMine.m_nMigrationCount - 1) * m_MatchmakingInfoMine.m_nMigrationTimeTaken_s) + nMigrationTime_s) / m_MatchmakingInfoMine.m_nMigrationCount);
		m_MatchmakingInfoMine.m_nMigrationChurn_Rolling = static_cast<s16>((((m_MatchmakingInfoMine.m_nMigrationCount - 1) * m_MatchmakingInfoMine.m_nMigrationChurn) + nMigrationChurn) / m_MatchmakingInfoMine.m_nMigrationCount);
		m_MatchmakingInfoMine.m_nMigrationCandidateIdx_Rolling = static_cast<u8>((((m_MatchmakingInfoMine.m_nMigrationCount - 1) * m_MatchmakingInfoMine.m_nMigrationCandidateIdx) + pEvent->m_CandidateIdx) / m_MatchmakingInfoMine.m_nMigrationCount);
	}
	
	gnetDebug1("ProcessSessionMigrateEndEvent :: Migration %s, Time: %u, Churn: %u (%u/%u), Candidates: %d/%d", pEvent->m_Succeeded ? "Suceeded" : "Failed", nMigrationTime, nMigrationChurn, m_pSession->GetGamerCount(), m_MigrationStartPlayerCount, pEvent->m_CandidateIdx + 1, pEvent->m_NumCandidates);

    // bail if we fail to migrate
    if(!pEvent->m_Succeeded)
    {
        if(!m_bIgnoreMigrationFailure[SessionType::ST_Physical])
        {
		    gnetDebug1("\tMigrateEnd :: Bailing next frame due to a failed host migration");
		    m_nBailReason = BAIL_SESSION_MIGRATE_FAILED;
			SetSessionState(SS_BAIL);
        }

		if(m_SendMigrationTelemetry)
		{
			// interpret a time of 0 as a failed migration
			m_MigratedTelemetry.Finish(0);
			APPEND_METRIC_FLUSH(m_MigratedTelemetry, false);
		}
    }
    else
    {
#if RSG_DURANGO
		// if we've established, flag that we're starting a new MPSD session (will happen in the establish event otherwise)
		if(m_pSession->IsEstablished() && m_bMigrationSentMultiplayerEndEvent)
		{
			WriteMultiplayerStartEventSession(OUTPUT_ONLY("MigrateEnd"));
			m_bMigrationSentMultiplayerEndEvent = false;
		}
		if(m_bValidateGameSession)
			g_rlXbl.GetPartyManager()->ValidateGameSession(NetworkInterface::GetLocalGamerIndex());
#endif

		rlGamerInfo hNewHost[1];
		m_pSession->GetGamers(pEvent->m_NewHost.GetPeerId(), hNewHost, 1);
		gnetDebug1("\tMigrateEnd :: %s gamer %s is new host", pEvent->m_NewHost.IsLocal() ? "Local" : "Remote", hNewHost->GetName());

		if(m_SendMigrationTelemetry)
		{
			// work out who's left and update the telemetry
			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			unsigned nGamers = m_pSession->GetGamers(aGamers, RL_MAX_GAMERS_PER_SESSION);
		
			// set everyone as connected 
			for(unsigned i = 0; i < nGamers; i++)
				m_MigratedTelemetry.SetCandidateResult(aGamers[i].GetName(), MetricSessionMigrated::CANDIDATE_RESULT_CLIENT);

			// and then set the host and finish
			m_MigratedTelemetry.SetCandidateResult(hNewHost[0].GetName(), MetricSessionMigrated::CANDIDATE_RESULT_HOST);
			m_MigratedTelemetry.Finish(nMigrationTime);

			APPEND_METRIC_FLUSH(m_MigratedTelemetry, false);

	#if !__FINAL || __FINAL_LOGGING
			if(!PARAM_netMatchmakingDisableDumpTelemetry.Get())
			{
				char metricBuf[2048] = {0};
				RsonWriter rw(metricBuf, RSON_FORMAT_JSON);
				m_MigratedTelemetry.Write(&rw);
				gnetDebug1("\tMigrateEnd :: Writing telemetry (Length: %u):", rw.Length());
				diagLoggedPrintLn(rw.ToString(), rw.Length());
			}
	#endif
		}

        // update the peer complainer
        if(IsHost())
			MigrateOrInitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession, NET_INVALID_ENDPOINT_ID);
		else
        {
            EndpointId hostEndpointId;
            m_pSession->GetHostEndpointId(&hostEndpointId);
			MigrateOrInitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession, hostEndpointId);
		}

        if(CNetwork::IsNetworkOpen())
        {
            gnetAssertf(!IsHost() || pEvent->m_NewHost.IsLocal(), "\tMigrateEnd :: Host does not match current session host!");

            netPlayer* pPlayer = CNetwork::GetPlayerMgr().GetPlayerFromPeerId(pEvent->m_NewHost.GetPeerId());
            gnetAssertf(pPlayer != nullptr, "\tMigrateEnd :: Unknown host!");

            if(pPlayer->HasValidPhysicalPlayerIndex())
            {
                gnetDebug1("\tMigrateEnd :: Calling NewHost, PPI: %d", pPlayer->GetPhysicalPlayerIndex());
                NetworkInterface::NewHost();
            }
            else
            {
                gnetDebug1("\tMigrateEnd :: Pending NewHost - Invalid Physical Player Index");
                m_PendingHostPhysical = true; 
            }
        }

        // special handling for launching states - if we migrate here, we need to allow 
        // the game some time to resolve 
        if(m_SessionState == SS_ESTABLISHING)
        {
            gnetDebug1("\tMigrateEnd :: Resetting state timeout.");
            m_SessionStateTime = sysTimer::GetSystemMsTime();
        }

		// validate local player
		bool bValidLocalPlayer = CNetwork::IsNetworkOpen() && CNetwork::GetPlayerMgr().IsInitialized() && (CNetwork::GetPlayerMgr().GetMyPlayer() != NULL);

		// if we are managing a transition to game, we need to update players who are still transferring over with this new information
		if(m_bIsTransitionToGame && m_bIsTransitionToGameInSession && m_nGamersToGame > 0)
		{
			MsgTransitionToGameNotify msg;
			msg.Reset(NetworkInterface::GetLocalGamerHandle(), m_pSession->GetSessionInfo(), MsgTransitionToGameNotify::NOTIFY_UPDATE, m_pSession->GetMaxSlots(RL_SLOT_PUBLIC) == 0);

			rlGamerInfo hInfo;
			for(unsigned i = 0; i < m_nGamersToGame; i++)
			{	
				// grab gamer info for peer ID to message - they might have left since, so check
				bool bGamerFound = m_pTransition->GetGamerInfo(m_hGamersToGame[i], &hInfo);
				if(bGamerFound && !hInfo.IsLocal())
					m_pTransition->SendMsg(hInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
			}
		}

        // update settings
        if(IsHost())
        {
			if(m_nPostMigrationBubbleCheckTime != 0)
			{
				gnetDebug1("\tMigrateEnd :: Setting post migration bubble check time of %ums", m_nPostMigrationBubbleCheckTime);
				CNetwork::GetRoamingBubbleMgr().SetCheckBubbleRequirementTime(m_nPostMigrationBubbleCheckTime);
			}

			// if we don't have valid bubble info, we probably joined on the cusp of the 
			// previous host leaving. As host, we can allocate ourselves a bubble
			if(bValidLocalPlayer && !CNetwork::GetPlayerMgr().GetMyPlayer()->GetRoamingBubbleMemberInfo().IsValid())
			{
				gnetDebug1("\tMigrateEnd :: No local bubble info. Allocating.");
				CNetwork::GetRoamingBubbleMgr().AllocatePlayerInitialBubble(NetworkInterface::GetLocalPlayer());
			}

			CNetworkTelemetry::SessionBecameHost(m_pSession->GetSessionId());

			// check that the clock was started, this could happen if we failed to process the
			// joined message from the previous host correctly
			if(!CNetwork::GetNetworkClock().HasStarted())
			{
				// start the network timer
				CNetwork::GetNetworkClock().Start(&CNetwork::GetConnectionManager(),
												  netTimeSync::FLAG_SERVER,
												  NET_INVALID_ENDPOINT_ID,
                                                  netTimeSync::GenerateToken(),
												  NULL,
												  NETWORK_GAME_CHANNEL_ID,
												  netTimeSync::DEFAULT_INITIAL_PING_INTERVAL,
												  netTimeSync::DEFAULT_FINAL_PING_INTERVAL);
			}
			else // otherwise, just restart
				CNetwork::GetNetworkClock().Restart(netTimeSync::FLAG_SERVER, 0, 0);

			// reset block requests - this needs to be called again if we want to turn this on
			m_bBlockJoinRequests = false;

            // only do this if we're still waiting in the activity session
            // detect if we're currently waiting for the host to kick off a transition
            if(m_bIsActivitySession)
            {
				if(m_hTransitionHostGamer.IsValid())
				{
					gnetDebug1("\tMigrateEnd :: Become Host during transition to game (Was: %s)", NetworkUtils::LogGamerHandle(m_hTransitionHostGamer));

					// reset these so that script can pick up the pieces
					m_bIsTransitionToGame = false;
					m_bIsTransitionToGamePendingLeave = false;
					m_hTransitionHostGamer.Clear();

					// find out who's left in the session
					rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
					rlGamerInfo aGamerInfo[MAX_NUM_PHYSICAL_PLAYERS];
					unsigned nGamers = CNetwork::GetNetworkSession().GetSessionMembers(aGamerInfo, MAX_NUM_PHYSICAL_PLAYERS);
					for(unsigned i = 0; i < nGamers; i++)
						hGamers[i] = aGamerInfo[i].GetGamerHandle();

					// going to freemode - we always want these
					SetMatchmakingGroup(MM_GROUP_FREEMODER);
					AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);
					AddActiveMatchmakingGroup(MM_GROUP_SCTV);

					// call up a transition to game
					DoTransitionToGame(m_TransitionToGameGameMode, hGamers, nGamers, true, MAX_NUM_PHYSICAL_PLAYERS, 0);
				}
            }
            else
            {
                // if we've ended up in a session by ourselves whilst joining and we have a fallback
                if(m_pSession->GetGamerCount() <= 1)
                {
					// increase solo count
					m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesSolo);

					if(!IsSessionEstablished())
                    {
                        // if matchmaking, and we have more results
                        if(m_bIsJoiningViaMatchmaking && ((m_GameMMResults.m_nCandidateToJoinIndex + 1) < m_GameMMResults.m_nNumCandidates))
                        {
                            gnetDebug1("\tMigrateEnd :: Migrated to solo session. Returning to matchmaking.");
                            m_bCheckForAnotherSession = true;
                        }

                        // if we have a join failure action to matchmake
                        if(m_JoinFailureAction == JoinFailureAction::JFA_Quickmatch)
                        {
							gnetDebug1("\tMigrateEnd :: Migrated to solo session. Actioning join failure.");
                            m_bCheckForAnotherSession = true;
		                }
                    }
                }
            }

			// track solo sessions
			if(m_pSession->GetGamerCount() <= 1)
			{
				eSoloSessionReason nReason = SOLO_SESSION_MIGRATE;
				if(!IsActivitySession() && !IsSessionEstablished())
					nReason = m_bIsJoiningViaMatchmaking ? SOLO_SESSION_MIGRATE_WHEN_JOINING_VIA_MM : SOLO_SESSION_MIGRATE_WHEN_JOINING_DIRECT;

				// write solo session telemetry
				gnetDebug1("\tMigrateEnd :: Sending solo telemetry - Reason: %s, Visibility: %s", GetSoloReasonAsString(nReason), GetVisibilityAsString(GetSessionVisibility(SessionType::ST_Physical)));
				CNetworkTelemetry::EnteredSoloSession(m_pSession->GetSessionId(), 
													  m_pSession->GetSessionToken().GetValue(), 
													  m_Config[SessionType::ST_Physical].GetGameMode(), 
													  nReason, 
													  GetSessionVisibility(SessionType::ST_Physical),
													  SOLO_SOURCE_MIGRATE,
													  static_cast<int>(m_MigrationStartPlayerCount),
													  static_cast<int>(nMigrationTime),
													  IsSessionEstablished() ? GetTimeInSession() : 0);
			
				// set data mine solo reason
				m_MatchmakingInfoMine.m_nSoloReason = static_cast<u8>(nReason);
				m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesSolo);

				// detect if we lost connection to the session host (i.e. we were split due to connection error)
				// if there were more than two players in the original session (more than the local player and the host)
				// then we've been split (not true in 100% of cases but we have a specific kick code to filter)
				if(m_MigrationStartPlayerCount > 2)
				{
					SendKickTelemetry(
						m_pSession,
						KickSource::Source_LostConnectionToHost,
						m_MigrationStartPlayerCount,
						SessionType::ST_Physical,
						m_TimeSessionStarted,
						static_cast<u32>(rlGetPosixTime() - m_LastMigratedPosix[SessionType::ST_Physical]),
						0,
						rlGamerHandle());
				}
			}

			// special tracking for a migration to the local player
			m_MatchmakingInfoMine.m_nMigrationTimeTaken_s = static_cast<u16>(nMigrationTime_s);
			m_MatchmakingInfoMine.m_nMigrationChurn = static_cast<s16>(nMigrationChurn);
			m_MatchmakingInfoMine.m_nMigrationCandidateIdx = static_cast<u8>(pEvent->m_CandidateIdx);

			// add script event
			GetEventScriptNetworkGroup()->Add(CEventNetworkSessionEvent(CEventNetworkSessionEvent::EVENT_MIGRATE_END, m_pSession->GetGamerCount()));

            // update matchmaking groups
            UpdateMatchmakingGroups();
		}
        else
        {
			// if we don't have valid bubble info, we probably joined on the cusp of the 
			// previous host leaving. Request bubble from the new host
			if(bValidLocalPlayer && !CNetwork::GetPlayerMgr().GetMyPlayer()->GetRoamingBubbleMemberInfo().IsValid())
			{
				gnetDebug1("\tMigrateEnd :: No local bubble info. Requesting.");
				CNetwork::GetRoamingBubbleMgr().RequestBubble();
			}

            // restart clock with new host
			EndpointId hostEndpointId;
			m_pSession->GetHostEndpointId(&hostEndpointId);
			CNetwork::GetNetworkClock().Restart(netTimeSync::FLAG_CLIENT, 0, hostEndpointId);

            // if we haven't managed to sync radio yet
            if(!m_bMainSyncedRadio)
            {
                if(NetworkInterface::GetActiveGamerInfo())
                {
                    MsgRadioStationSyncRequest msg;
					msg.Reset(m_pSession->GetSessionId(), NetworkInterface::GetActiveGamerInfo()->GetGamerId());
                    m_pSession->SendMsg(pEvent->m_NewHost.GetPeerId(), msg, NET_SEND_RELIABLE);
                    BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
                }
            }
		}
    }
}

void CNetworkSession::ProcessSessionJoinedEvent(snEventSessionJoined* pEvent)
{
    if(IsSessionLeaving())
		return;

	GetEventScriptNetworkGroup()->Add(CEventNetworkJoinSessionResponse(true, RESPONSE_ACCEPT));

	if(!IsHost())
    {
		OUTPUT_ONLY(m_Config[SessionType::ST_Physical].LogDifferences(m_pSession->GetConfig(), "\tProcessSessionJoinedEvent"));

		// apply the current config - chiefly, we do this to allocate the schema index values 
		player_schema::MatchingAttributes mmAttrs;
		m_Config[SessionType::ST_Physical].Reset(mmAttrs, m_pSession->GetConfig().m_Attrs.GetGameMode(), m_pSession->GetConfig().m_Attrs.GetSessionPurpose(), m_pSession->GetConfig().m_MaxPublicSlots + m_pSession->GetConfig().m_MaxPrivateSlots, m_pSession->GetConfig().m_MaxPrivateSlots);
		m_Config[SessionType::ST_Physical].Apply(m_pSession->GetConfig());

        // apply the game-mode
        CNetwork::SetNetworkGameMode(m_Config[SessionType::ST_Physical].GetGameMode());
    }

    // complete transition to game
    OnSessionEntered(false);

	// have we already added the host and initialised the peer complainer
	bool bHaveHostPlayer = false;
	if(!m_pSession->IsHost())
	{
		u64 nHostPeerID = 0;
		if(m_pSession->GetHostPeerId(&nHostPeerID) && NetworkInterface::GetPlayerFromPeerId(nHostPeerID))
		{
			gnetDebug1("ProcessSessionJoinedEvent :: Already added host player!");
			bHaveHostPlayer = true;
		}
	}

	// reset peer complainer
	if(!(bHaveHostPlayer && m_PeerComplainer[SessionType::ST_Physical].IsInitialized()) || m_PeerComplainer[SessionType::ST_Physical].IsServer())
		InitPeerComplainer(&m_PeerComplainer[SessionType::ST_Physical], m_pSession);

	// import the game response 
	CMsgJoinResponse msg;
	if(!gnetVerify(msg.Import(pEvent->m_Data, pEvent->m_SizeofData)))
	{
		gnetError("ProcessSessionJoinedEvent :: Failed importing join event data!");
		return; 
	}

	// validate session type is what we expect
	if(!gnetVerify(msg.m_SessionPurpose == SessionPurpose::SP_Freeroam))
	{
		gnetError("ProcessSessionJoinedEvent :: Invalid Session Purpose (%d)!", msg.m_SessionPurpose);
		return; 
	}

    if(CNetwork::GetNetworkClock().HasStarted())
        CNetwork::GetNetworkClock().Stop();

	CNetwork::GetNetworkClock().Start(&CNetwork::GetConnectionManager(),
                                      netTimeSync::FLAG_CLIENT,
                                      pEvent->m_HostEndpointId,
									  msg.m_HostTimeToken,
                                      &msg.m_HostTime,
                                      NETWORK_GAME_CHANNEL_ID,
                                      netTimeSync::DEFAULT_INITIAL_PING_INTERVAL,
                                      netTimeSync::DEFAULT_FINAL_PING_INTERVAL);

	gnetAssertf(NetworkInterface::NetworkClockHasSynced(), "SessionJoined :: Network clock has not synced!");
	CNetwork::SetSyncedTimeInMilliseconds(msg.m_HostTime);
	gPopStreaming.SetNetworkModelVariationID(msg.m_ModelVariationID);
	m_nHostFlags[SessionType::ST_Physical] = msg.m_HostFlags;
    OUTPUT_ONLY(LogHostFlags(m_nHostFlags[SessionType::ST_Physical], __FUNCTION__));

    // copy in crew data
    m_UniqueCrewData[SessionType::ST_Physical].m_nLimit = msg.m_UniqueCrewLimit;
    m_UniqueCrewData[SessionType::ST_Physical].m_bOnlyCrews = msg.m_CrewsOnly;
    m_UniqueCrewData[SessionType::ST_Physical].m_nMaxCrewMembers = msg.m_MaxMembersPerCrew;

	// main session join
	m_TimeCreated[SessionType::ST_Physical] = msg.m_TimeCreated;

	// setup matchmaking groups
	if(!gnetVerify(msg.m_NumActiveMatchmakingGroups <= MAX_ACTIVE_MM_GROUPS))
	{
		gnetError("SessionJoined :: Invalid matchmaking group number: %u, Max: %u", msg.m_NumActiveMatchmakingGroups, MAX_ACTIVE_MM_GROUPS);
		msg.m_NumActiveMatchmakingGroups = MAX_ACTIVE_MM_GROUPS;
	}

	ClearActiveMatchmakingGroups();
	for(unsigned i = 0; i < msg.m_NumActiveMatchmakingGroups; i++)
	{
		AddActiveMatchmakingGroup(static_cast<eMatchmakingGroup>(msg.m_ActiveMatchmakingGroup[i]));
		SetMatchmakingGroupMax(static_cast<eMatchmakingGroup>(msg.m_ActiveMatchmakingGroup[i]), msg.m_MatchmakingGroupMax[i]);
	}

	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "SESSION_JOINED", "");
	NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("Network time", "%d", msg.m_HostTime);
	
	// record results for telemetry
	if(m_bIsJoiningViaMatchmaking)
	{
		m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_nTimeFinished = sysTimer::GetSystemMsTime();
		m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_Result = RESPONSE_ACCEPT;
	}
	
	UpdateSCPresenceInfo();
}

void CNetworkSession::ProcessSessionJoinFailedEvent(snEventJoinFailed* pEvent)
{
	gnetDebug1("\tJoinFailed :: Session Response: %s", NetworkUtils::GetSessionResponseAsString(pEvent->m_ResponseCode));

	// for the game response
	eJoinResponseCode nResponse = RESPONSE_DENY_UNKNOWN;
	switch(pEvent->m_ResponseCode)
	{
	case SNET_JOIN_DENIED_NOT_JOINABLE:			nResponse = RESPONSE_DENY_NOT_JOINABLE; break;
	case SNET_JOIN_DENIED_NO_EMPTY_SLOTS:		nResponse = RESPONSE_DENY_SESSION_FULL; break;
	case SNET_JOIN_DENIED_FAILED_TO_ESTABLISH:	nResponse = RESPONSE_DENY_FAILED_TO_ESTABLISH; break;
	case SNET_JOIN_DENIED_PRIVATE:				nResponse = RESPONSE_DENY_PRIVATE_ONLY; break;
	default: break;
	}

	// set up join queue
	m_bCanQueue = false;

	// check game response, bail out
    if(pEvent->m_Response)
	{
		CMsgJoinResponse msg;
		if(!gnetVerify(pEvent->m_SizeofResponse && msg.Import(pEvent->m_Response, pEvent->m_SizeofResponse)))
		{
			gnetError("JoinFailed :: Invalid response data!");
			return; 
		}

		// apply response code
		nResponse = msg.m_ResponseCode;

		// generic logging and asserts where the user should probably be informed
		gnetError("JoinFailed :: Game Response: %s", NetworkUtils::GetResponseCodeAsString(nResponse));
		
		// logging for join queue
		if(nResponse == RESPONSE_DENY_GROUP_FULL)
		{
			gnetDebug1("\tJoinFailed :: RESPONSE_DENY_GROUP_FULL - CanQueue: %s, Invited: %s, Inviter: %s", msg.m_bCanQueue ? "True" : "False", m_bJoinedViaInvite ? "True" : "False", NetworkUtils::LogGamerHandle(m_Inviter));
			m_bCanQueue = msg.m_bCanQueue && m_bJoinedViaInvite && m_Inviter.IsValid();
			if(m_bCanQueue)
			{
				m_hJoinQueueInviter = m_Inviter;
				m_bJoinQueueAsSpectator = (m_MatchmakingGroup == MM_GROUP_SCTV);
				m_bJoinQueueConsumePrivate = CheckFlag(m_nJoinFlags, JoinFlags::JF_ConsumePrivate);
			}
		}

		if(m_bIsJoiningViaMatchmaking)
		{
			// record the time at which we gave our last attempt and stuff the bail reason into our result code
			m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_nTimeFinished = sysTimer::GetSystemMsTime();
			m_GameMMResults.m_CandidatesToJoin[m_GameMMResults.m_nCandidateToJoinIndex].m_Result = msg.m_ResponseCode;
		}

	#if __ASSERT
		// if we tried to join via invite, assert in some instances
		if(m_bJoinedViaInvite)
		{
			switch(nResponse)
			{
				// worthy enough to assert about
			case RESPONSE_DENY_INCOMPATIBLE_ASSETS:
			#if !__FINAL
				Displayf("Dumping interior proxy infos due to RESPONSE_DENY_INCOMPATIBLE_ASSETS:");
				CInteriorProxy::DumpInteriorProxyInfo();
			#endif
				gnetAssertf(0, "\tJoinFailed :: Incompatible Assets!");
				break;
			case RESPONSE_DENY_TIMEOUT:
				gnetAssertf(0, "\tJoinFailed :: Timeout Value Different From Host!");
				break;
			default:
				break; 
			}
		}
	#endif
	}

	// generate script event
	GetEventScriptNetworkGroup()->Add(CEventNetworkJoinSessionResponse(false, nResponse));
}

void CNetworkSession::ProcessCrewID(rlClanId nClanID, unsigned& nNoCrewPlayers, const SessionType sessionType)
{
    // select the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

    // if they have no valid ID, make a note and skip
    if(nClanID == RL_INVALID_CLAN_ID)
    {
        if(pData->m_bOnlyCrews)
            gnetError("ProcessCrewID :: %s :: Non-crew player found! This is blocked!", NetworkUtils::GetSessionTypeAsString(sessionType));

        // increment tally
        nNoCrewPlayers++;
    }
    else
    {
        // check if we already tagged this clan ID
        bool bFound = false;
        for(int i = 0; i < MAX_UNIQUE_CREWS; i++)
        {
            if(nClanID == pData->m_nActiveCrewIDs[i])
            {
                pData->m_nCurrentNumMembers[i]++;
                bFound = true;
                break;
            }
        }

        // this clan ID not previously tagged, add to list and increment count
        if(!bFound)
        {
            pData->m_nActiveCrewIDs[pData->m_nCurrentUniqueCrews] = nClanID;
            pData->m_nCurrentNumMembers[pData->m_nCurrentUniqueCrews] = 1;
            pData->m_nCurrentUniqueCrews++;
        }
    }
}

void CNetworkSession::UpdateCurrentUniqueCrews(const SessionType sessionType)
{
    // select the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

	// check that we have a unique crew limit set - otherwise, bail
	if(pData->m_nLimit == 0)
		return;

	// active ID tracking
	pData->m_nCurrentUniqueCrews = 0;
    for(int i = 0; i < MAX_UNIQUE_CREWS; i++)
    {
        pData->m_nActiveCrewIDs[i] = RL_INVALID_CLAN_ID;
        pData->m_nCurrentNumMembers[i] = 0;
    }

	// tracking variables
	unsigned nNoCrewPlayers = 0;

    // different depending on the session type
    if(sessionType == SessionType::ST_Physical)
    {
        rlGamerInfo aInfo[RL_MAX_GAMERS_PER_SESSION];
        const unsigned nGamers = m_pSession->GetGamers(aInfo, COUNTOF(aInfo));
        for(unsigned i = 0; i < nGamers; ++i)
        {
            CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(aInfo[i].GetGamerId());
            if(pPlayer && pPlayer->GetMatchmakingGroup() != MM_GROUP_SCTV) 
                ProcessCrewID(pPlayer->GetCrewId(), nNoCrewPlayers, sessionType);
        }
    }
    else
    {
        // assign crew slots
        unsigned nGamers = m_TransitionPlayers.GetCount();
        for(unsigned i = 0; i < nGamers; i++) if(!m_TransitionPlayers[i].m_bIsSpectator)
            ProcessCrewID(m_TransitionPlayers[i].m_nClanID, nNoCrewPlayers, sessionType);
    }

	// if we have players without a crew, count them as one crew 
	if(nNoCrewPlayers > 0)
    {
        // park non-crew players numbers at the end
        pData->m_nCurrentNumMembers[pData->m_nCurrentUniqueCrews] = static_cast<u8>(nNoCrewPlayers);
        pData->m_nCurrentUniqueCrews++;
    }

	// stash unique crew count
	gnetDebug1("UpdateCurrentUniqueCrews :: %s :: %d unique crews. Non-crew players: %d", NetworkUtils::GetSessionTypeAsString(sessionType), pData->m_nCurrentUniqueCrews, nNoCrewPlayers);
#if !__NO_OUTPUT
    for(u8 i = 0; i < pData->m_nCurrentUniqueCrews; i++)
    {
        gnetDebug1("\tCrew %d: Id: %" I64FMT "d, NumMembers: %u", i + 1, pData->m_nActiveCrewIDs[i], pData->m_nCurrentNumMembers[i]);
    }
#endif
}

bool CNetworkSession::CanJoinWithCrewID(const SessionType sessionType, rlClanId nClanID)
{
    // select the correct data
    sUniqueCrewData* pData = &m_UniqueCrewData[sessionType];

    // no crew limit
    if(pData->m_nLimit == 0)
        return true;

    // check if we're blocking non-crew players
    if(pData->m_bOnlyCrews && nClanID == RL_INVALID_CLAN_ID)
        return false;

    // not at the limit, we can join (providing we don't have a crew member limit)
    if(pData->m_nCurrentUniqueCrews < pData->m_nLimit && (pData->m_nMaxCrewMembers == 0))
        return true;

    // all crews account for or member limit 
    for(unsigned i = 0; i < pData->m_nLimit; i++)
    {
        // if we haven't assigned a crew to this ID yet
        if(pData->m_nActiveCrewIDs[i] == RL_INVALID_CLAN_ID)
            return true;

        if(nClanID == pData->m_nActiveCrewIDs[i])
        {
            // we fit into an existing crew
            if(pData->m_nMaxCrewMembers == 0)
                return true; 
            // check limit
            else if(pData->m_nCurrentNumMembers[i] < pData->m_nMaxCrewMembers)
                return true;
                
            // exit process
            break;
        }
    }

    // cannot join
    return false;
}

void CNetworkSession::HandleLocalPlayerBeingKicked(const KickReason kickReason)
{
	// check that we're not already leaving
	if(!gnetVerify(SS_LEAVING > m_SessionState))
	{
		gnetError("HandleLocalPlayerBeingKicked :: Player is leaving and cannot be kicked!");
		return; 
	}
	
	// log out reason
	gnetDebug1("HandleLocalPlayerBeingKicked :: Reason: %s", NetworkSessionUtils::GetKickReasonAsString(kickReason));
	m_LastKickReason = kickReason;

    // reset
    m_bKickCheckForAnotherSession = false;

	// handle specifics for connectivity errors
	if(IsKickDueToConnectivity(kickReason))
	{
		// detect if we should attempt to re-enter matchmaking
		if(!IsSessionEstablished() && !NetworkInterface::IsHost() && (m_bIsJoiningViaMatchmaking || m_JoinFailureAction != JoinFailureAction::JFA_None))
		{
			gnetDebug1("HandleLocalPlayerBeingKicked :: Not started. Will check for another session");
			m_bKickCheckForAnotherSession = true;
		}
	}

	// increase data mine kick count
	m_MatchmakingInfoMine.IncrementStat(m_MatchmakingInfoMine.m_nNumTimesKicked);

	// handle the kick in the next update
	SetSessionState(SS_HANDLE_KICK);
}

void CNetworkSession::SendKickTelemetry(
	const snSession* session, 
	const KickSource kickSource, 
	const unsigned numSessionMembers,
	const unsigned sessionIndex,
	const unsigned timeSinceEstablished,
	const unsigned timeSinceLastMigration,
	const unsigned numComplainers,
	const rlGamerHandle& hComplainer)
{
	if(!m_bSendRemovedFromSessionKickTelemetry)
		return;

	CNetworkTelemetry::NetworkKicked(session->GetSessionToken().GetValue(),
		session->GetSessionId(),
		session->GetConfig().m_Attrs.GetGameMode(),
		kickSource,
		numSessionMembers,
		sessionIndex,
		timeSinceEstablished,
		timeSinceLastMigration,
		numComplainers,
		hComplainer);
}

bool CNetworkSession::SendEventStartAndWaitSPScriptsCleanUp()
{
	if(m_WaitFramesSinglePlayerScriptCleanUpBeforeStart == 0) 
	{   
		//Single player scripts only start the clean up process once they've seen
		//the EVENT_NETWORK_SESSION_START event sent.
		GetEventScriptNetworkGroup()->Add(CEventNetworkStartSession());
		GetEventScriptNetworkGroup()->Add(CEventNetworkStartMatch());
	}

	if(m_WaitFramesSinglePlayerScriptCleanUpBeforeStart >= MAX_WAIT_FRAMES_SINGLE_PLAYER_SCRIPT_END_BEFORE_START)
	{
		return true;
	}

	if(GtaThread::CountSinglePlayerOnlyScripts()==0)
	{
		return true;
	}

	m_WaitFramesSinglePlayerScriptCleanUpBeforeStart++;
	return false;
}

void CNetworkSession::ClearBlacklistedSessions()
{
	bool bFinished = false;
	while(!bFinished)
	{
		bFinished = true; 
		int nBlacklistedSessions = m_BlacklistedSessions.GetCount();
		for(int i = 0; i < nBlacklistedSessions; i++)
		{
			if((sysTimer::GetSystemMsTime() - m_BlacklistedSessions[i].m_nTimestamp) > s_TimeoutValues[TimeoutSetting::Timeout_SessionBlacklist])
			{
				gnetDebug2("Removing blacklisted session - ID: 0x%016" I64FMT "x.", m_BlacklistedSessions[i].m_nSessionToken.m_Value);

				m_BlacklistedSessions.Delete(i);
				bFinished = false;
				break;
			}
		}
	}
}

bool CNetworkSession::IsSessionBlacklisted(const rlSessionToken& nSessionToken)
{
	// check blacklisted sessions
	int nBlacklistedSessions = m_BlacklistedSessions.GetCount(); 
	for(int i = 0; i < nBlacklistedSessions; i++)
	{
		if(m_BlacklistedSessions[i].m_nSessionToken == nSessionToken)
			return true;
	}
	return false;
}

bool CNetworkSession::AddJoinFailureSession(const rlSessionToken& nSessionToken)
{
	// check token is valid
	if(!nSessionToken.IsValid())
		return false;

	// if we have space (we absolutely should), add this session to our list of failed attempts
	// this is preferred instead of popping our first value
	if(m_JoinFailureSessions.GetCount() < MAX_MATCHMAKING_RESULTS)
	{
        gnetDebug2("AddJoinFailureSession :: ID: 0x%016" I64FMT "x.", nSessionToken.m_Value);
		m_JoinFailureSessions.Push(nSessionToken);
		return true;
	}

	return false;
}

bool CNetworkSession::HasSessionJoinFailed(const rlSessionToken& nSessionToken)
{
	// check join failure sessions
	int nJoinFailureSessions = m_JoinFailureSessions.GetCount(); 
	for(int i = 0; i < nJoinFailureSessions; i++)
	{
		if(m_JoinFailureSessions[i] == nSessionToken)
			return true;
	}
	return false;
}

void CNetworkSession::UpdateSCPresenceInfo()
{
	if (m_pSession != NULL)
	{
		NetworkSCPresenceUtil::UpdateMPSCPresenceInfo(*m_pSession);
	}
}

bool CNetworkSession::DidGamerLeaveForStore(const rlGamerHandle& hGamer)
{
    return FindGamerInStore(hGamer) >= 0;
}

int CNetworkSession::FindGamerInStore(const rlGamerHandle& hGamer)
{
    int nGamersInStore = m_GamerInStore.GetCount();
    for(int i = 0; i < nGamersInStore; i++)
    {
        if(m_GamerInStore[i].m_hGamer == hGamer)
            return i;
    }

    return -1; 
}

void CNetworkSession::SyncRadioStations(const u64 nSessionID, const rlGamerId& nGamerID)
{
	// work out requirements for syncing radio stations
	static const unsigned nOneRadioStatioPayload = MsgRadioStationSync::GetRadioStationBitSize();
	static const unsigned nMaxRadioStationPayloadPerMsg = (netMessage::MAX_BYTE_SIZEOF_PAYLOAD * 8) - MsgRadioStationSync::GetHeaderBitSize();
	static const unsigned nMaxRadioStationsToSyncPerMsg = (nMaxRadioStationPayloadPerMsg / nOneRadioStatioPayload);

	// work out the session to reply on
	snSession* pSession = NULL;
	rlGamerInfo tGamerInfo;
	if(m_pSession->GetSessionId() == nSessionID)
	{
		if(m_pSession->GetGamerInfo(nGamerID, &tGamerInfo))
			pSession = m_pSession;
	}
	else if(m_pTransition->GetSessionId() == nSessionID)
	{
		if(m_pTransition->GetGamerInfo(nGamerID, &tGamerInfo))
			pSession = m_pTransition;
	}

	// validate that we got a session
	if(!pSession)
	{
		gnetWarning("SyncRadioStations :: Failed to find session or gamer");
		return;
	}

	unsigned nStationsSynced = 0;
	unsigned nMessagesRequired = 0;
	unsigned nStationTotal = audRadioStation::GetNumTotalStations();

	MsgRadioStationSync msg;
	msg.Reset(nSessionID);

	for(unsigned i = 0; i < nStationTotal; i++)
	{
		audRadioStation* pStation = audRadioStation::GetStation(i);

		// check station exists
		if(!pStation)
			continue;

#if __WIN32PC // user music
		// can't sync stations playing user radio
		if(pStation->IsUserStation())
			continue;
#endif

		// populate sync data
		pStation->PopulateSyncData(msg.m_SyncData[msg.m_nRadioStations]);
		msg.m_nRadioStations++;
		nStationsSynced++;

		if(msg.m_nRadioStations == nMaxRadioStationsToSyncPerMsg)
		{
			// we've filled our message, send it off and reset the message 
			pSession->SendMsg(tGamerInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
			msg.Reset(nSessionID);

			// track messages sent
			nMessagesRequired++;
		}
	}

	// if we have some unsent data, send it off
	if(msg.m_nRadioStations > 0)
	{
		// we've filled our message, send it off and reset the message 
		pSession->SendMsg(tGamerInfo.GetPeerInfo().GetPeerId(), msg, NET_SEND_RELIABLE);
		nMessagesRequired++;
	}

	// logging
	gnetDebug2("SyncRadioStations :: Stations: %d, Synced: %d, Messages: %d, Data: %db, %db, %d, Token: 0x%" I64FMT "x, Gamer: %" I64FMT "u", nStationTotal, nStationsSynced, nMessagesRequired, nOneRadioStatioPayload, nMaxRadioStationPayloadPerMsg, nMaxRadioStationsToSyncPerMsg, pSession->GetSessionInfo().GetToken().m_Value, nGamerID.GetId());
}

void CNetworkSession::SendStallMessage(const unsigned stallIncludingNetworkUpdate, const unsigned networkUpdateTime)
{
	MsgDebugStall msg(stallIncludingNetworkUpdate, networkUpdateTime);
	  
	if (m_pSession->Exists())
	{
		m_pSession->BroadcastMsg(msg, 0);
	}
	if (m_pTransition->Exists()) 
	{
		m_pTransition->BroadcastMsg(msg, 0);
	}
}

void CNetworkSession::SendAddedGamerTelemetry(bool isTransitioning, const u64 sessionId, const snEventAddedGamer* pEvent, netNatType natType)
{
	CNetworkTelemetry::OnPeerPlayerConnected(isTransitioning, sessionId, pEvent, natType, NET_IS_VALID_ENDPOINT_ID(pEvent->m_EndpointId) ? CNetwork::GetConnectionManager().GetAddress(pEvent->m_EndpointId) : netAddress::INVALID_ADDRESS);
}

u64 CNetworkSession::SendMatchmakingStartTelemetry(sMatchmakingQuery* pQueries, const unsigned nQueries)
{
	// telemetry for matchmaking - generate new matchmaking unique ID
	u64 nUniqueMatchmakingID = 0;
	rlCreateUUID(&nUniqueMatchmakingID);

	// bit flags for active queries
	unsigned nActiveQueryFlags = 0;
	NetworkGameFilter* pFilter = NULL;
	for(int i = 0; i < nQueries; i++) if(pQueries[i].m_bIsActive)
	{
		nActiveQueryFlags |= (1 << pQueries[i].m_QueryType);
		if(!pFilter && pQueries[i].m_pFilter)
			pFilter = pQueries[i].m_pFilter;
	}

	// this should never happen
	if(gnetVerify(pFilter))
	{
		// telemetry
		MetricMatchmakingQueryStart m(pFilter->GetGameMode(), nUniqueMatchmakingID, nActiveQueryFlags);
		for(u32 i = 0; i < pFilter->GetMatchingFilter().GetConditionCount(); ++i)
			m.AddFilterValue(pFilter->GetMatchingFilter().GetParamFieldIdForCondition(i), pFilter->GetMatchingFilter().GetValue(i));
		APPEND_METRIC(m);

#if !__FINAL || __FINAL_LOGGING
		if(!PARAM_netMatchmakingDisableDumpTelemetry.Get())
		{
			char metricBuf[2048] = {0};
			RsonWriter rw(metricBuf, RSON_FORMAT_JSON);
			m.Write(&rw);
			gnetDebug1("SendMatchmakingStartTelemetry :: Writing telemetry (Length: %u):", rw.Length());
			diagLoggedPrintLn(rw.ToString(), rw.Length());
		}
#endif
	}

	// return ID
	return nUniqueMatchmakingID;
}

void CNetworkSession::SendMatchmakingResultsTelemetry(sMatchmakingResults* pResults)
{
	// add metric data for matchmaking query results
	unsigned nTimeTaken = (pResults->m_MatchmakingRetrievedTime > 0) ? (sysTimer::GetSystemMsTime() - pResults->m_MatchmakingRetrievedTime + 500) / 1000 : 0;
	MetricMatchmakingQueryResults m(pResults->m_UniqueID, pResults->m_GameMode, pResults->m_nNumCandidatesAttempted, nTimeTaken, pResults->m_bHosted);

#if !__FINAL
	if(PARAM_netMatchmakingMaxTelemetry.Get())
	{
		// write maximum number of candidates with maximum values
		for(unsigned i = 0; i < NETWORK_MAX_MATCHMAKING_CANDIDATES; i++)
		{
			MetricMatchmakingQueryResults::sCandidateData tCandidateData;
			tCandidateData.m_sessionid		= static_cast<u64>(-7673711341053468107); 
			tCandidateData.m_fromQueries	= 8; 
			tCandidateData.m_maxSlots		= 32;
			tCandidateData.m_usedSlots		= 32; 
			tCandidateData.m_ResultCode		= 64; 
			tCandidateData.m_TimeAttempted	= 40;
			m.AddCandidate(tCandidateData);
		}
	}
	else
#endif
	{
		for(unsigned i = 0; i < pResults->m_nNumCandidates; i++)
		{
			MetricMatchmakingQueryResults::sCandidateData tCandidateData;
			tCandidateData.m_sessionid		= pResults->m_CandidatesToJoin[i].m_Session.m_SessionConfig.m_SessionId;
			tCandidateData.m_fromQueries	= pResults->m_CandidatesToJoin[i].m_nFromQueries;
			tCandidateData.m_maxSlots		= pResults->m_CandidatesToJoin[i].m_Session.m_SessionConfig.m_MaxPrivateSlots + pResults->m_CandidatesToJoin[i].m_Session.m_SessionConfig.m_MaxPublicSlots;
			tCandidateData.m_usedSlots		= pResults->m_CandidatesToJoin[i].m_Session.m_NumFilledPrivateSlots + pResults->m_CandidatesToJoin[i].m_Session.m_NumFilledPublicSlots;
			tCandidateData.m_ResultCode		= pResults->m_CandidatesToJoin[i].m_Result;
			tCandidateData.m_TimeAttempted	= pResults->m_CandidatesToJoin[i].m_nTimeStarted > 0 ? ((pResults->m_CandidatesToJoin[i].m_nTimeFinished - pResults->m_CandidatesToJoin[i].m_nTimeStarted + 500) / 1000) : 0;
			m.AddCandidate(tCandidateData);
		}
	}
	
	APPEND_METRIC(m);

#if !__FINAL || __FINAL_LOGGING
	if(!PARAM_netMatchmakingDisableDumpTelemetry.Get())
	{
		char metricBuf[2048] = {0};
		RsonWriter rw(metricBuf, RSON_FORMAT_JSON);
		m.Write(&rw);
		gnetDebug1("SendMatchmakingResultsTelemetry :: Writing telemetry (Length: %u):", rw.Length());
		diagLoggedPrintLn(rw.ToString(), rw.Length());
	}
#endif
}

#if RSG_DURANGO
void CNetworkSession::WriteMultiplayerStartEventSession(OUTPUT_ONLY(const char* szTag))
{
	if(gnetVerifyf(!m_bHasWrittenStartEvent[SessionType::ST_Physical], "WriteMultiplayerStartEventSession :: Already started!"))
	{
		gnetDebug1("WriteMultiplayerStartEventSession :: WriteEvent_MultiplayerRoundStart - %s [%u]", szTag, static_cast<unsigned>(rlGetPosixTime()));
		events_durango::WriteEvent_MultiplayerRoundStart(m_pSession, 0, events_durango::MatchType_Competitive);
		m_bHasWrittenStartEvent[SessionType::ST_Physical] = true;
	}
}

void CNetworkSession::WriteMultiplayerEndEventSession(bool bClean OUTPUT_ONLY(, const char* szTag))
{
	gnetDebug1("WriteMultiplayerEndEventSession :: WriteEvent_MultiplayerRoundEnd - %s [%u]. Started: %s, Status: %s, Exists: %s", szTag, static_cast<unsigned>(rlGetPosixTime()), m_bHasWrittenStartEvent[SessionType::ST_Physical] ? "True" : "False", bClean ? "Clean" : "Error", m_pSession->Exists() ? "True" : "False");
	if(m_bHasWrittenStartEvent[SessionType::ST_Physical])
	{
		if(m_pSession->Exists())
			events_durango::WriteEvent_MultiplayerRoundEnd(m_pSession, 0, events_durango::MatchType_Competitive, 0, bClean ? events_durango::ExitStatus_Clean : events_durango::ExitStatus_Error);
	}
	m_bHasWrittenStartEvent[SessionType::ST_Physical] = false;
}

void CNetworkSession::WriteMultiplayerStartEventTransition(OUTPUT_ONLY(const char* szTag))
{
	if(gnetVerifyf(!m_bHasWrittenStartEvent[SessionType::ST_Transition], "WriteMultiplayerStartEventTransition :: Already started!"))
	{
		gnetDebug1("WriteMultiplayerStartEventTransition :: WriteEvent_MultiplayerRoundStart - %s [%u]", szTag, static_cast<unsigned>(rlGetPosixTime()));
		events_durango::WriteEvent_MultiplayerRoundStart(m_pTransition, static_cast<int>(m_Config[SessionType::ST_Transition].GetActivityType()), events_durango::MatchType_Competitive);
		m_bHasWrittenStartEvent[SessionType::ST_Transition] = true;
	}
}

void CNetworkSession::WriteMultiplayerEndEventTransition(bool bClean OUTPUT_ONLY(, const char* szTag))
{
	gnetDebug1("WriteMultiplayerEndEventTransition :: WriteEvent_MultiplayerRoundEnd - %s [%u]. Started: %s, Status: %s, Exists: %s", szTag, static_cast<unsigned>(rlGetPosixTime()), m_bHasWrittenStartEvent[SessionType::ST_Transition] ? "True" : "False", bClean ? "Clean" : "Error", m_pTransition->Exists() ? "True" : "False");
	if(m_bHasWrittenStartEvent[SessionType::ST_Transition])
	{
		if(m_pTransition->Exists())
			events_durango::WriteEvent_MultiplayerRoundEnd(m_pTransition, static_cast<int>(m_Config[SessionType::ST_Transition].GetActivityID()), events_durango::MatchType_Competitive, 0, bClean ? events_durango::ExitStatus_Clean : events_durango::ExitStatus_Error);
	}
	m_bHasWrittenStartEvent[SessionType::ST_Transition] = false;
}
#elif RSG_SCARLETT
void CNetworkSession::WriteMultiplayerStartEventSession(OUTPUT_ONLY(const char* szTag))
{
	if (gnetVerifyf(!m_bHasWrittenStartEvent[SessionType::ST_Physical], "WriteMultiplayerStartEventSession :: Already started!"))
	{
		gnetDebug1("WriteMultiplayerStartEventSession :: WriteEvent_MultiplayerRoundStart - %s [%u]", szTag, static_cast<unsigned>(rlGetPosixTime()));
		events_scarlett::WriteEvent_MultiplayerRoundStart(m_pSession, 0, events_scarlett::MatchType_Competitive);
		m_bHasWrittenStartEvent[SessionType::ST_Physical] = true;
	}
}

void CNetworkSession::WriteMultiplayerEndEventSession(bool bClean OUTPUT_ONLY(, const char* szTag))
{
	gnetDebug1("WriteMultiplayerEndEventSession :: WriteEvent_MultiplayerRoundEnd - %s [%u]. Started: %s, Status: %s, Exists: %s", szTag, static_cast<unsigned>(rlGetPosixTime()), m_bHasWrittenStartEvent[SessionType::ST_Physical] ? "True" : "False", bClean ? "Clean" : "Error", m_pSession->Exists() ? "True" : "False");
	if (m_bHasWrittenStartEvent[SessionType::ST_Physical])
	{
		if (m_pSession->Exists())
			events_scarlett::WriteEvent_MultiplayerRoundEnd(m_pSession, 0, events_scarlett::MatchType_Competitive, 0, bClean ? events_scarlett::ExitStatus_Clean : events_scarlett::ExitStatus_Error);
	}
	m_bHasWrittenStartEvent[SessionType::ST_Physical] = false;
}

void CNetworkSession::WriteMultiplayerStartEventTransition(OUTPUT_ONLY(const char* szTag))
{
	if (gnetVerifyf(!m_bHasWrittenStartEvent[SessionType::ST_Transition], "WriteMultiplayerStartEventTransition :: Already started!"))
	{
		gnetDebug1("WriteMultiplayerStartEventTransition :: WriteEvent_MultiplayerRoundStart - %s [%u]", szTag, static_cast<unsigned>(rlGetPosixTime()));
		events_scarlett::WriteEvent_MultiplayerRoundStart(m_pTransition, static_cast<int>(m_Config[SessionType::ST_Transition].GetActivityType()), events_scarlett::MatchType_Competitive);
		m_bHasWrittenStartEvent[SessionType::ST_Transition] = true;
	}
}

void CNetworkSession::WriteMultiplayerEndEventTransition(bool bClean OUTPUT_ONLY(, const char* szTag))
{
	gnetDebug1("WriteMultiplayerEndEventTransition :: WriteEvent_MultiplayerRoundEnd - %s [%u]. Started: %s, Status: %s, Exists: %s", szTag, static_cast<unsigned>(rlGetPosixTime()), m_bHasWrittenStartEvent[SessionType::ST_Transition] ? "True" : "False", bClean ? "Clean" : "Error", m_pTransition->Exists() ? "True" : "False");
	if (m_bHasWrittenStartEvent[SessionType::ST_Transition])
	{
		if (m_pTransition->Exists())
			events_scarlett::WriteEvent_MultiplayerRoundEnd(m_pTransition, static_cast<int>(m_Config[SessionType::ST_Transition].GetActivityID()), events_scarlett::MatchType_Competitive, 0, bClean ? events_scarlett::ExitStatus_Clean : events_scarlett::ExitStatus_Error);
	}
	m_bHasWrittenStartEvent[SessionType::ST_Transition] = false;
}
#endif

void CNetworkSession::SetDefaultPlayerSessionData()
{
#if RSG_PROSPERO
	m_platformAttrs.SetPublic(true);
	
	// Currently always 2 so there's a slot for the host and a slot for the 
	// 'joining' player who doesn't actually join the session.
	m_platformAttrs.SetMaxSlots(2);

	char buffer[64] = {0};

	// We have to pass through the text for each supported language
	for (int i = LANGUAGE_FIRST; i < MAX_LANGUAGES; ++i)
	{
		const sysLanguage lang = (sysLanguage)i;
		formatf(buffer, "UI_PLAYER_SESSION_FM_%s", fwLanguagePack::GetIsoLanguageCodeWithSimpChinese(lang));
		m_platformAttrs.SetSessionName(lang, TheText.Get(buffer));
	}
#endif
}

#if __BANK

static bkBank* g_pNetworkBank = NULL;
static bkGroup* g_pSessionGroup = NULL;
static bkCombo* g_pFriendsCombo = NULL;
static bkCombo* g_pTransitionCombo = NULL;
static bool g_bBankInitialised = false;
static bkCombo* g_pPlayersCombo = NULL;
static char gs_szTextMessage[MAX_TEXT_MESSAGE_LENGTH];
static bkCombo* g_pDataBurstCombo = NULL;
static int g_DataBurstComboIndex = 0;
static int g_BankGameMode = MultiplayerGameMode::GameMode_Freeroam;

static const char* g_DataBurstComboOptions[] = 
{
	"UNRELIABLE",
	"RELIABLE",
	"MIXTURE"
};

void  
CNetworkSession::InitWidgets()
{
	g_pNetworkBank = BANKMGR.FindBank("Network");
	if(gnetVerify(g_pNetworkBank))
	{
		bkGroup* pLiveGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live"));
		if(gnetVerify(pLiveGroup))
		{
			m_BankPlayers[0] = "--No Players--";
			m_BankPlayersComboIndex = 0;

			m_BankFriends[0] = "--No Friends--";
			m_BankFriendsComboIndex = 0;

			m_BankTransitionPlayers[0] = "--No Members--";
			m_BankTransitionPlayersComboIndex = 0;

			m_szSessionString[0] = '\0';
            gs_szTextMessage[0] = '\0';
		
			g_pSessionGroup = g_pNetworkBank->PushGroup("Debug Session");
				g_pNetworkBank->AddButton("Print Session Config", datCallback(MFA(CNetworkSession::Bank_PrintSessionConfig), (datBase*)this));
				g_pNetworkBank->AddButton("Quickmatch", datCallback(MFA(CNetworkSession::Bank_DoQuickmatch), (datBase*)this));
				g_pNetworkBank->AddButton("Host Single Player Session", datCallback(MFA(CNetworkSession::Bank_HostSinglePlayerSession), (datBase*)this));
				g_pNetworkBank->AddButton("Leave Session", datCallback(MFA(CNetworkSession::Bank_LeaveSession), (datBase*)this));
				g_pNetworkBank->AddSeparator();
				g_pPlayersCombo = g_pNetworkBank->AddCombo("Players", &m_BankPlayersComboIndex, 1, (const char**)m_BankPlayers);
				g_pNetworkBank->AddButton("Refresh Player List", datCallback(MFA(CNetworkSession::Bank_RefreshPlayerList), (datBase*)this));
				g_pNetworkBank->AddButton("Kick Player", datCallback(MFA(CNetworkSession::Bank_KickPlayer), (datBase*)this));
				g_pNetworkBank->AddButton("Add Peer Complaint", datCallback(MFA(CNetworkSession::Bank_AddPeerComplaint), (datBase*)this));
				g_pNetworkBank->AddButton("Close Connection", datCallback(MFA(CNetworkSession::Bank_CloseConnection), (datBase*)this));
				g_pNetworkBank->AddButton("Open Tunnel", datCallback(MFA(CNetworkSession::Bank_OpenTunnel), (datBase*)this));
				g_pNetworkBank->AddText("Text Message", gs_szTextMessage, MAX_TEXT_MESSAGE_LENGTH, NullCB);
                g_pNetworkBank->AddButton("Send Text Message", datCallback(MFA(CNetworkSession::Bank_SendTextMessage), (datBase*)this));
				g_pDataBurstCombo = g_pSessionGroup->AddCombo("Data Burst", &g_DataBurstComboIndex, NELEM(g_DataBurstComboOptions), (const char**)g_DataBurstComboOptions);
				g_pNetworkBank->AddButton("Send Data Burst", datCallback(MFA(CNetworkSession::Bank_SendDataBurst), (datBase*)this));
				g_pNetworkBank->AddSeparator();
				g_pFriendsCombo = g_pSessionGroup->AddCombo("Friends", &m_BankFriendsComboIndex, 1, (const char**)m_BankFriends);
				g_pNetworkBank->AddButton("Refresh Friends", datCallback(MFA(CNetworkSession::Bank_RefreshFriends), (datBase*)this));
				g_pNetworkBank->AddButton("Join Friend Session", datCallback(MFA(CNetworkSession::Bank_JoinFriendSession), (datBase*)this));
				g_pNetworkBank->AddButton("Send Presence Invite", datCallback(MFA(CNetworkSession::Bank_SendPresenceInviteToFriend), (datBase*)this));
				g_pNetworkBank->AddButton("Send Presence Invite To Transition", datCallback(MFA(CNetworkSession::Bank_SendPresenceTransitionInviteToFriend), (datBase*)this));
				g_pNetworkBank->AddButton("Accept Presence Invite", datCallback(MFA(CNetworkSession::Bank_AcceptPresenceInvite), (datBase*)this));
				g_pNetworkBank->AddSeparator();
				g_pTransitionCombo = g_pSessionGroup->AddCombo("Transition Members", &m_BankTransitionPlayersComboIndex, 1, (const char**)m_BankTransitionPlayers);
				g_pNetworkBank->AddButton("Refresh Transition Members", datCallback(MFA(CNetworkSession::Bank_RefreshTransitionMembers), (datBase*)this));
				g_pNetworkBank->AddText("Game Mode", &g_BankGameMode, false);
				g_pNetworkBank->AddButton("Host Transition", datCallback(MFA(CNetworkSession::Bank_HostTransition), (datBase*)this));
				g_pNetworkBank->AddButton("Invite To Transition", datCallback(MFA(CNetworkSession::Bank_InviteToTransition), (datBase*)this));
				g_pNetworkBank->AddButton("Leave Transition", datCallback(MFA(CNetworkSession::Bank_LeaveTransition), (datBase*)this));
				g_pNetworkBank->AddButton("Launch Transition", datCallback(MFA(CNetworkSession::Bank_LaunchTransition), (datBase*)this));
				g_pNetworkBank->AddButton("Join Player Transition", datCallback(MFA(CNetworkSession::Bank_JoinPlayerTransition), (datBase*)this));
				g_pNetworkBank->AddButton("Transition Matchmaking", datCallback(MFA(CNetworkSession::Bank_DoTransitionMatchmaking), (datBase*)this));
				g_pNetworkBank->AddButton("Transition Matchmaking Specific", datCallback(MFA(CNetworkSession::Bank_DoTransitionMatchmakingSpecific), (datBase*)this));
				g_pNetworkBank->AddButton("Transition Matchmaking User Content", datCallback(MFA(CNetworkSession::Bank_DoTransitionMatchmakingUserContent), (datBase*)this));
				g_pNetworkBank->AddButton("Mark Transition In Progress", datCallback(MFA(CNetworkSession::Bank_MarkTransitionInProgress), (datBase*)this));
				g_pNetworkBank->AddButton("Mark Transition Using User Content", datCallback(MFA(CNetworkSession::Bank_MarkTransitionUsingUserCreatedContent), (datBase*)this));
				g_pNetworkBank->AddSeparator();
				g_pNetworkBank->AddButton("Join Previous Session", datCallback(MFA(CNetworkSession::Bank_JoinPreviousSession), (datBase*)this));
				g_pNetworkBank->AddSeparator();
				g_pNetworkBank->AddText("Session String", m_szSessionString, rlSessionInfo::TO_STRING_BUFFER_SIZE, NullCB);
				g_pNetworkBank->AddButton("Session From String", datCallback(MFA(CNetworkSession::Bank_LogSessionString), (datBase*)this));
                g_pNetworkBank->AddSeparator();
                g_pNetworkBank->AddButton("Toggle Multiplayer Available", datCallback(MFA(CNetworkSession::Bank_ToggleMultiplayerAvailable), (datBase*)this));
#if RSG_DURANGO
				g_pNetworkBank->AddButton("Log MPSD Session Properties", datCallback(MFA(CNetworkSession::Bank_LogXblSessionDetails), (datBase*)this));
				g_pNetworkBank->AddButton("Write Custom MPSD Properties", datCallback(MFA(CNetworkSession::Bank_WriteXblSessionCustomProperties), (datBase*)this));
#endif

#if RSG_PC
				g_pNetworkBank->AddButton("Test Simple Mod Detection", datCallback(MFA(CNetworkSession::Bank_TestSimpleModDetection), (datBase*)this));
#endif
			g_pNetworkBank->PopGroup();
		}
	}

	g_bBankInitialised = true; 
}

void CNetworkSession::Bank_DoQuickmatch()
{
	AddActiveMatchmakingGroup(MM_GROUP_FREEMODER);
	SetMatchmakingGroup(MM_GROUP_FREEMODER);
	DoQuickMatch(g_BankGameMode, SCHEMA_GROUPS, 32, true);
}

void CNetworkSession::Bank_HostSinglePlayerSession()
{
	m_nHostRetries = 0;
	HostSession(g_BankGameMode, 1, HostFlags::HF_IsPrivate | HostFlags::HF_SingleplayerOnly);
}

void CNetworkSession::Bank_LeaveSession()
{
	LeaveSession(LeaveFlags::LF_Default, LeaveSessionReason::Leave_Normal);
}

void CNetworkSession::Bank_PrintSessionConfig()
{
	gnetDebug1("====== Session Config ======");
	gnetDebug1("Session Config :: %s mode", CNetwork::LanSessionsOnly() ? "LAN" : "Online");
	gnetDebug1("Session Config :: Timeout - %d", CNetwork::GetTimeoutTime());

#if RSG_NP
	if(m_pSession->Exists())
	{
		gnetDebug1("Session Config :: Current Token: 0x%" I64FMT "x", m_pSession->GetSessionId());
	}

	if(m_pTransition->Exists())
	{
		gnetDebug1("Session Config :: Current Token: 0x%" I64FMT "x", m_pTransition->GetSessionId());
	}
#endif

	switch(m_SessionState)
	{
	case SS_IDLE:
		gnetDebug1("Session Config :: Not Active!");
		break;
	case SS_FINDING:
		gnetDebug1("Session Config :: Matchmaking - Displaying Filter:");
		m_SessionFilter.DebugPrint();
		break;
	default:
		gnetDebug1("Session Config :: Active - Displaying Config:");
		m_Config[SessionType::ST_Physical].DebugPrint(true);
		break;
	}
}

void CNetworkSession::Bank_RefreshPlayerList()
{
	unsigned nBankNumPlayers = 0;

	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
	const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

	for(unsigned index = 0; index < numActivePlayers; index++)
	{
		const netPlayer* pPlayer = activePlayers[index];
		m_BankPlayers[nBankNumPlayers] = pPlayer->GetGamerInfo().GetName();
		nBankNumPlayers++;
	}

	if(nBankNumPlayers == 0)
	{
		m_BankPlayers[0] = "--No Players--";
		nBankNumPlayers = 1;
	}

	m_BankPlayersComboIndex = Min(m_BankPlayersComboIndex, static_cast<int>((nBankNumPlayers - 1)));
	m_BankPlayersComboIndex = Max(m_BankPlayersComboIndex, 0);

	if(g_pPlayersCombo)
		g_pPlayersCombo->UpdateCombo("Players", &m_BankPlayersComboIndex, nBankNumPlayers, (const char**)m_BankPlayers);
}

const CNetGamePlayer* CNetworkSession::Bank_GetChosenPlayer()
{
	const char* szTargetName = m_BankPlayers[m_BankPlayersComboIndex];
	
	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
	const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

	for(unsigned index = 0; index < numActivePlayers; index++)
	{
		const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);
		if(!strcmp(pPlayer->GetGamerInfo().GetName(), szTargetName))
			return pPlayer;
	}

	return NULL;
}

void CNetworkSession::Bank_KickPlayer()
{
	if(!IsHost())
		return;

	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(!pPlayer)
		return;

	KickPlayer(pPlayer, KickReason::KR_VotedOut, 1, NetworkInterface::GetLocalGamerHandle());
}

void CNetworkSession::Bank_AddPeerComplaint()
{
	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(!pPlayer)
		return;

	if(pPlayer->IsLocal())
		return;

	TryPeerComplaint(pPlayer->GetRlPeerId(), &m_PeerComplainer[SessionType::ST_Physical], m_pSession, SessionType::ST_Physical);
}

void CNetworkSession::Bank_OpenTunnel()
{
	static netTunnelRequest s_TunnelRqst; 
	static netStatus s_Status; 
	if(gnetVerifyf(s_TunnelRqst.Pending(), "Bank_OpenTunnel :: Tunnel Request Pending!"))
		return;

	CNetGamePlayer* pPlayer = const_cast<CNetGamePlayer*>(Bank_GetChosenPlayer());
	if(pPlayer->IsLocal())
		return;

	rlSessionInfo hInfo = m_pSession->GetSessionInfo();

	const bool isBilateral = false;
	const netTunnelDesc tunnelDesc(NET_TUNNELTYPE_ONLINE, NET_TUNNEL_REASON_P2P, hInfo.GetToken().GetValue(), isBilateral);
	CNetwork::GetConnectionManager().OpenTunnel(pPlayer->GetPeerInfo().GetPeerAddress(),
												tunnelDesc,
											    &s_TunnelRqst,
											    &s_Status);
}

void CNetworkSession::Bank_CloseConnection()
{
	CNetGamePlayer* pPlayer = const_cast<CNetGamePlayer*>(Bank_GetChosenPlayer());
	if(pPlayer->IsLocal())
		return;

	NetworkInterface::GetPlayerMgr().CloseConnection(*pPlayer);
}

void CNetworkSession::Bank_SendTextMessage()
{
    CNetGamePlayer* pPlayer = const_cast<CNetGamePlayer*>(Bank_GetChosenPlayer());
    if(pPlayer->IsLocal())
        return;

    CNetwork::GetVoice().SendTextMessage(gs_szTextMessage, pPlayer->GetGamerInfo().GetGamerHandle());
}

void CNetworkSession::Bank_SendDataBurst()
{
	CNetGamePlayer* pPlayer = const_cast<CNetGamePlayer*>(Bank_GetChosenPlayer());
	if(pPlayer->IsLocal())
		return;

	static const unsigned SMALL_PACKET_SIZE = 20;
	static const unsigned MEDIUM_PACKET_SIZE = 200;
	static const unsigned LARGE_PACKET_SIZE = 700;
	char packet[netPacket::MAX_BYTE_SIZEOF_PACKET];

	gnetDebug1("SendDataBurst :: %u", netPacket::MAX_BYTE_SIZEOF_PACKET);

	static const unsigned BURST_SIZE = 5;

	netConnectionManager& cxnMgr = CNetwork::GetConnectionManager();
	const int cxnId = pPlayer->GetConnectionId(); 
	const bool isOpen = cxnMgr.IsOpen(cxnId) || cxnMgr.IsPendingOpen(cxnId);

	if(isOpen)
	{
		netSequence netSeq;

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, LARGE_PACKET_SIZE,  sendFlag, &netSeq);
			gnetDebug1("Sending large packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, SMALL_PACKET_SIZE, sendFlag, &netSeq);
			gnetDebug1("Sending small packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, MEDIUM_PACKET_SIZE, sendFlag, &netSeq);
			gnetDebug1("Sending medium packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, SMALL_PACKET_SIZE, sendFlag, &netSeq);
			gnetDebug1("Sending small packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, LARGE_PACKET_SIZE, sendFlag, &netSeq);
			gnetDebug1("Sending large packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}

		for(unsigned i = 0; i < BURST_SIZE; i++)
		{
			unsigned sendFlag = (g_DataBurstComboIndex == 0) ? 0 : ((g_DataBurstComboIndex == 1 ) ? NET_SEND_RELIABLE : (gs_Random.GetBool() ? NET_SEND_RELIABLE : 0));
			cxnMgr.Send(cxnId, packet, MEDIUM_PACKET_SIZE, sendFlag, &netSeq);
			gnetDebug1("Sending medium packet: Sequence: %u, Reliable: %s", netSeq, sendFlag != 0 ? "True" : "False");
		}
	}
}

void CNetworkSession::Bank_RefreshFriends()
{
	if(!g_pFriendsCombo)
		return;

	// update the friend list
	int nFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
	for(u32 i = 0; i < nFriends; i++)
		m_BankFriends[i] = CLiveManager::GetFriendsPage()->m_Friends[i].GetName();

	// avoid empty combo box
	if(nFriends == 0)
	{
		m_BankFriends[0] = "--No Friends--";
		nFriends = 1;
	}

	m_BankFriendsComboIndex = Clamp(m_BankFriendsComboIndex, 0, static_cast<int>(nFriends) - 1);
	g_pFriendsCombo->UpdateCombo("Friends", &m_BankFriendsComboIndex, nFriends, (const char**)m_BankFriends);
}

void CNetworkSession::Bank_JoinFriendSession()
{
#if RSG_NP
	int nFriendIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(m_BankFriends[m_BankFriendsComboIndex]);
	if(nFriendIndex < 0)
		return;

	rlFriend* pFriend = &CLiveManager::GetFriendsPage()->m_Friends[nFriendIndex];
	JoinFriendSession(pFriend);

	// force any blockers off
	CLiveManager::GetInviteMgr().AllowProcessInPlayerSwitch(true);
	CLiveManager::GetInviteMgr().SuppressProcess(false);
#endif
}

void CNetworkSession::Bank_HostTransition()
{
	m_nHostRetriesTransition = 0;
	HostTransition(g_BankGameMode, RL_MAX_GAMERS_PER_SESSION, 1, 0x10050001, ACTIVITY_ISLAND_GENERAL, ACTIVITY_CONTENT_ROCKSTAR_CREATED, HostFlags::HF_Default);
}

void CNetworkSession::Bank_InviteToTransition()
{
	int nFriendIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(m_BankFriends[m_BankFriendsComboIndex]);
	if(nFriendIndex < 0)
		return;

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nFriendIndex];
	rlGamerHandle hFriend;
	pFriend.GetGamerHandle(&hFriend);
	if(!m_pTransition->IsMember(hFriend))
		SendTransitionInvites(&hFriend, 1);
}

void CNetworkSession::Bank_LaunchTransition()
{
	LaunchTransition();
}

void CNetworkSession::Bank_LeaveTransition()
{
	LeaveTransition();
}

void CNetworkSession::Bank_RefreshTransitionMembers()
{
	if(!g_pTransitionCombo)
		return; 

	u64 nHostPeerID;
	m_pTransition->GetHostPeerId(&nHostPeerID);

	rlGamerInfo tHostInfo[1];
	m_pTransition->GetGamers(nHostPeerID, tHostInfo, 1);

	// fill in members
	rlGamerInfo memberInfo[RL_MAX_GAMERS_PER_SESSION];
	u32 nMembers = m_pTransition->GetGamers(memberInfo, RL_MAX_GAMERS_PER_SESSION);
	for(u32 i = 0; i < nMembers; i++)
	{
		if(memberInfo[i] == tHostInfo[0])
		{
			snprintf(m_BankLeaderName, RL_MAX_NAME_BUF_SIZE + 1, "%s*", memberInfo[i].GetName());
			m_BankTransitionPlayers[i] = m_BankLeaderName;
		}
		else 
			m_BankTransitionPlayers[i] = memberInfo[i].GetName();
	}

	// avoid empty combo box
	if(nMembers == 0)
	{
		m_BankTransitionPlayers[0] = "--No Members--";
		nMembers = 1;
	}

	m_BankTransitionPlayersComboIndex = Clamp(m_BankTransitionPlayersComboIndex, 0, static_cast<int>(nMembers) - 1);
	g_pTransitionCombo->UpdateCombo("Transition Members", &m_BankTransitionPlayersComboIndex, nMembers, (const char**)m_BankTransitionPlayers);
}

void CNetworkSession::Bank_JoinPlayerTransition()
{
	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(pPlayer->IsLocal())
		return;

	if(!pPlayer->HasStartedTransition())
		return;

	const rlSessionInfo& hInfo = pPlayer->GetTransitionSessionInfo();
	if(!hInfo.IsValid())
		return;

	JoinTransition(hInfo, RL_SLOT_PUBLIC, JoinFlags::JF_ViaInvite);
}

void CNetworkSession::Bank_DoTransitionMatchmaking()
{
	DoTransitionQuickmatch(g_BankGameMode, RL_MAX_GAMERS_PER_SESSION, -1, 0, NO_ACTIVITY_ISLAND, MatchmakingFlags::MMF_QueryOnly);
}

void CNetworkSession::Bank_DoTransitionMatchmakingSpecific()
{
	DoTransitionQuickmatch(g_BankGameMode, RL_MAX_GAMERS_PER_SESSION, 1, 0x10050001, NO_ACTIVITY_ISLAND, MatchmakingFlags::MMF_QueryOnly);
}

void CNetworkSession::Bank_DoTransitionMatchmakingUserContent()
{
	DoTransitionQuickmatch(g_BankGameMode, RL_MAX_GAMERS_PER_SESSION, -1, 0, NO_ACTIVITY_ISLAND, MatchmakingFlags::MMF_QueryOnly | MatchmakingFlags::MMF_UserCreatedOnly);
}

void CNetworkSession::Bank_MarkTransitionInProgress()
{
	SetSessionInProgress(SessionType::ST_Transition, true);
}

void CNetworkSession::Bank_MarkTransitionUsingUserCreatedContent()
{
	SetContentCreator(SessionType::ST_Transition, ACTIVITY_CONTENT_USER_CREATED);
}

#if RSG_PC 
void CNetworkSession::Bank_TestSimpleModDetection()
{
	network_commands::CommandShouldWarnOfSimpleModCheck();
}
#endif

void CNetworkSession::Bank_SendPresenceInviteToFriend()
{
	int nFriendIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(m_BankFriends[m_BankFriendsComboIndex]);
	if(nFriendIndex < 0)
		return;

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nFriendIndex];
	rlGamerHandle hFriend;
	pFriend.GetGamerHandle(&hFriend);
	if(!m_pSession->IsMember(hFriend))
	{
		bool bIsImportant = true;
		SendPresenceInvites(&hFriend, &bIsImportant, 1, "ContentId", "MatchId", 0, 0, 0, 0);
	}
}

void CNetworkSession::Bank_SendPresenceTransitionInviteToFriend()
{
	int nFriendIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(m_BankFriends[m_BankFriendsComboIndex]);
	if(nFriendIndex < 0)
		return;

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nFriendIndex];
	rlGamerHandle hFriend;
	pFriend.GetGamerHandle(&hFriend);
	if(!m_pTransition->IsMember(hFriend))
	{
		bool bIsImportant = true;
		SendPresenceInvitesToTransition(&hFriend, &bIsImportant, 1, "ContentId", "MatchId", 0, 0, 0, 0);
	}
}

void CNetworkSession::Bank_AcceptPresenceInvite()
{
	static const unsigned nIndex = 0;
	CLiveManager::GetInviteMgr().AcceptRsInvite(nIndex);
}

void CNetworkSession::Bank_JoinPreviousSession()
{
	// matchmake on failure
	JoinPreviousSession(JoinFailureAction::JFA_Quickmatch);
}

void CNetworkSession::Bank_LogSessionString()
{
	rlSessionInfo hInfo;
	hInfo.FromString(m_szSessionString);
	gnetDebug1("Session Token: 0x%016" I64FMT "x", hInfo.GetToken().m_Value);

	char szPeerAddr[rlPeerAddress::TO_STRING_BUFFER_SIZE];
	hInfo.GetHostPeerAddress().ToString(szPeerAddr);
	gnetDebug1("Session Host Address: %s", szPeerAddr);
}

void CNetworkSession::Bank_ToggleMultiplayerAvailable()
{
    bool bIsMultiplayerDisabled = false;
    Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MULTIPLAYER_DISABLED", 0x3c0af527), bIsMultiplayerDisabled);
    Tunables::GetInstance().Insert(TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("MULTIPLAYER_DISABLED", 0x3c0af527)), !bIsMultiplayerDisabled, 0, 0, true);
}

#if RSG_DURANGO

static netStatus g_BankXblTaskStatus;
void CNetworkSession::Bank_LogXblSessionDetails()
{
	if(!gnetVerify(!g_BankXblTaskStatus.Pending()))
		return;

	g_rlXbl.GetSessionManager()->LogSessionDetails(reinterpret_cast<const rlXblSessionHandle*>(m_pSession->GetSessionHandle()), &g_BankXblTaskStatus);
}

void CNetworkSession::Bank_WriteXblSessionCustomProperties()
{
	if(!gnetVerify(!g_BankXblTaskStatus.Pending()))
		return;

	g_rlXbl.GetSessionManager()->WriteCustomProperties(reinterpret_cast<const rlXblSessionHandle*>(m_pSession->GetSessionHandle()), m_pSession->GetSessionInfo().GetToken(), &g_BankXblTaskStatus);
}

#endif

#endif // if __BANK

#if !__NO_OUTPUT
const char* GetGamerLogString(const rlGamerInfo& info)
{
	static const unsigned MAX_LOGGING_STRINGS = 5;
	static const unsigned MAX_STRING = 64;
	static char szString[MAX_LOGGING_STRINGS][MAX_STRING];
	static unsigned logIndex = 0;

	// get handle
	static char szLoggingString[MAX_STRING];
	if(info.IsValid())
		formatf(szLoggingString, "%s [%s]", info.GetName(), NetworkUtils::LogGamerHandle(info.GetGamerHandle()));
	else
		safecpy(szLoggingString, "Invalid");

	// copy into our current logging index
	formatf(szString[logIndex], "[%s]", szLoggingString);

	// having a rolling buffer allows for multiple calls to this function in the same logging line
	int loggingIndexUsed = logIndex;
	logIndex++;
	if(logIndex >= MAX_LOGGING_STRINGS)
		logIndex = 0;

	return szString[loggingIndexUsed];
}

const char* CNetworkSession::Util_GetGamerLogString(const snSession* pSession, const CNetGamePlayer* player)
{
	if(player)
		return Util_GetGamerLogString(pSession, player->GetGamerInfo().GetGamerHandle());
	
	return "Invalid";
}

const char* CNetworkSession::Util_GetGamerLogString(const snSession* pSession, const rlGamerHandle& hGamer)
{
	static rlGamerInfo hInfo;
	if(pSession)
	{
		if(pSession->GetGamerInfo(hGamer, &hInfo))
			return GetGamerLogString(hInfo);
	}
	else
	{
		if(m_pSession->GetGamerInfo(hGamer, &hInfo))
			return GetGamerLogString(hInfo);
		else if(m_pTransition->GetGamerInfo(hGamer, &hInfo))
			return GetGamerLogString(hInfo);
	}

	return NetworkUtils::LogGamerHandle(hGamer);
}

const char* CNetworkSession::Util_GetGamerLogString(const snSession* pSession, const EndpointId epId)
{
	static rlGamerInfo hInfo[1];
	if(pSession)
	{
		if(pSession->GetGamers(epId, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
	}
	else
	{
		if(m_pSession->GetGamers(epId, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
		else if(m_pTransition->GetGamers(epId, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
	}
	return "Invalid";
}

const char* CNetworkSession::Util_GetGamerLogString(const snSession* pSession, const u64 nPeerID)
{
	static rlGamerInfo hInfo[1];
	if(pSession)
	{
		if(pSession->GetGamers(nPeerID, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
	}
	else
	{
		if(m_pSession->GetGamers(nPeerID, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
		else if(m_pTransition->GetGamers(nPeerID, hInfo, 1))
			return GetGamerLogString(hInfo[0]);
	}
	return "Invalid";
}

#endif

#if RSG_PC
#define NUMBER_HANDLES 256
void CNetworkSession::CheckDinputCount()
{
	// Mostly copy/pasta from
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682621(v=vs.85).aspx
	HMODULE hMods[NUMBER_HANDLES] = {0};
	DWORD cbNeeded;
	//@@: location CNETWORKSESSION_CHECKDINPUTCOUNT_ENTRY
	HANDLE hProcess = GetCurrentProcess();
	netDInputDebug("Checking the number of dinput8.dll binaries");
	u32 dinputCount = 0;
	//@@: range CNETWORKSESSION_CHECKDINPUTCOUNT_BODY {
	const unsigned int dinputHash = 0xDA9D2ABF;
	if(EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++ )
		{
			TCHAR szModName[MAX_PATH];
			if ( GetModuleBaseName(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
			{
				// Store our module in an atString for easy access.
				atString dib(szModName);
				
				// Make it lowercase so we always compare against a good hash.
				dib.Lowercase();

				// Compute the hash string
				atHashString dibHash(dib.c_str());
				// Compare it to our known, for dinputHash
				if(dibHash.GetHash() == dinputHash)
				{
					dinputCount++;
				}
				// Sanitize
				dib = "";
			}
		}
	}
	netDInputDebug("Found %d number of dinput8.dll's", dinputCount);
	SetDinputCount(dinputCount);
	m_dinputCheckTime.Set(sysTimer::GetSystemMsTime());
	//@@: } CNETWORKSESSION_CHECKDINPUTCOUNT_BODY
	// Sanitize the handles of modules
	//@: location CNETWORKSESSION_CHECKDINPUTCOUNT_EXIT
	memset(hMods, 0, sizeof(HANDLE)*NUMBER_HANDLES);
}

#endif