//
// name:        NetworkTypes.h
// description: Shared network definitions at game level. 
//

#ifndef NETWORK_TYPES_GAME_H
#define NETWORK_TYPES_GAME_H

// rage includes
#include "fwnet/nettypes.h"
#include "fwutil/Gen9Settings.h"
#include "rline/scmembership/rlscmembershipcommon.h"
#include "script/wrapper.h"

#define NETWORK_LOCKED_DEFAULT (false)
#define NETWORK_MAX_MATCHMAKING_CANDIDATES (10)

#define USE_NEW_NETWORK_VOICE (0)

// the network session flow supports two different sessions that can be switched around
// the physical session is our active session with gameplay
// the transition session is a non-physical session that can be used when switching between two different sessions
enum SessionType
{
	ST_None = -1,
	ST_Physical,
	ST_Transition,
	ST_Max,
};

// what the session is being used for
enum SessionPurpose
{
	SP_None = -1,
	SP_Freeroam = 0,
	SP_Activity,
	SP_Party,
	SP_Presence,
	SP_Max,
};

enum GamerIndexWithType
{
	GAMER_INDEX_MIN = -3,
	GAMER_INDEX_EVERYONE = -3,	//< NOTE: This is a TU added entry, used only in CheckUserContentPrivileges. Don't expect it to work somewhere else
	GAMER_INDEX_ANYONE = -2,
	GAMER_INDEX_LOCAL = -1,
	GAMER_INDEX_MAX = RL_MAX_LOCAL_GAMERS
};

#define VALID_GAMER_INDEX_OR_TYPE(x) gnetVerifyf(x >= GAMER_INDEX_MIN && x < GAMER_INDEX_MAX, "VALID_GAMER_INDEX_OR_TYPE :: Invalid Index: %d", x)

// Multiplayer Game Modes
// From scripts:
//		shared\include\globals\shared_global_definitions.sch
//		multiplayer\globals\mp_globals_arcade.sch
// Any changes should also be reflected in GetMultiplayerGameModeAsString
enum MultiplayerGameMode
{
	// Normal Modes
	GameMode_Invalid = -1,
	GameMode_Freeroam = 0,
	GameMode_TestBed = 1,
	GameMode_Creator = 2,
	GameMode_Editor = 3,

	// This seems to be a menu mode for arcade
	GameMode_FakeMultiplayer = 4,

	// Arcade Modes
	GameMode_Arcade_Start = 5,
	GameMode_Arcade_CnC = GameMode_Arcade_Start,
	GameMode_Arcade_EndlessWinter,
	GameMode_Arcade_Last = GameMode_Arcade_EndlessWinter,

	// Tutorial
	GameMode_Tutorial = -1090,

	// Code Modes
    GameMode_Presence = 1000,
	GameMode_Voice = 1001,
	GameMode_Party = 1002,
};

enum MultiplayerGameModeState
{
	GameModeState_Invalid = -1,
	GameModeState_None = 0,

	GameModeState_CnC_Cash,
	GameModeState_CnC_Escape
};

// multiplayer access codes
enum eNetworkAccessCode
{
	Access_BankDefault = -2,
	Access_Invalid = -1,
	Access_Granted, 
	Access_Deprecated_TunableNotFound,
	Access_Deprecated_TunableFalse,
	Access_Denied_MultiplayerLocked,
	Access_Denied_InvalidProfileSettings,
	Access_Denied_PrologueIncomplete,
	Access_Denied_NotSignedIn,			/* Not signed into the Xbox profile */
	Access_Denied_NotSignedOnline,		/* Not signed into PSN or Xbox Live */
	Access_Denied_NoOnlinePrivilege,
	Access_Denied_NoRosCredentials,
	Access_Denied_NoRosPrivilege,
	Access_Denied_MultiplayerDisabled,
	Access_Denied_NoTunables,
	Access_Denied_NoBackgroundScript,

	// add new reasons for code after this
	// script enumerate anything above and would need an update 

	Access_Denied_NoNetworkAccess,		/* Ethernet or wifi is disconnected; only use by the landing page */
	Access_Denied_RosBanned,
	Access_Denied_RosSuspended,
};

enum eNetworkAccessArea
{
	AccessArea_Invalid,
	AccessArea_First = 0,
	AccessArea_Landing = AccessArea_First,
	AccessArea_Multiplayer,
	AccessArea_MultiplayerEnter,
	AccessArea_Num,
};

// multiplayer access check ready state
enum IsReadyToCheckAccessResult
{
	Result_Invalid = -1,

	// ready results
	Result_Ready_First,
	Result_Ready_CommandLine = Result_Ready_First,
	Result_Ready_InvalidLocalGamerIndex,
	Result_Ready_NpUnavailable,
	Result_Ready_PassedAllChecks,
	Result_Ready_Last = Result_Ready_PassedAllChecks,

	// not ready results
	Result_NotReady_CommandLine,
	Result_NotReady_RecentlyResumed,
	Result_NotReady_WaitingForAvailabilityResult,
	Result_NotReady_WaitingForRosCredentials,
	Result_NotReady_NotReadyToCheckUnlockState,
	Result_NotReady_WaitingForPrivilegeResult,
	Result_NotReady_WaitingForPermissions,
	Result_NotReady_WaitingForPlusEvent,
	Result_NotReady_WaitingForSignInCompletion
};

enum IntroSettingResult
{
	Result_Unknown = -1,
	Result_NotComplete = 0,
	Result_Complete,
};

enum AlertScreenSource
{
	Source_CodeTransition,
	Source_CodeBail,
	Source_CodeAccess,
	Source_Script,
};

// set our session type attribute for telemetry
// this is a little muddled as STT_Activity can technically be private or closed too
enum SessionTypeForTelemetry
{
	STT_Invalid = -1,
	STT_Solo,
	STT_Activity,
	STT_Freeroam,
	STT_ClosedCrew,
	STT_Private,
	STT_ClosedFriends,
	STT_Crew,
	STT_Num,
};

// session type attribute for presence
// this is mainly for joinability (so activity or not isn't considered)
enum SessionTypeForPresence
{
	STP_Unknown = -1,
	STP_InviteOnly,
	STP_FriendsOnly,
	STP_CrewOnly,
	STP_CrewSession,
	STP_Solo,
	STP_Public,
	STP_Num,
};

enum
{
	BailFlag_None = 0,
	BailFlag_ReportOnly = BIT0,		// indicates we shouldn't bail - this is just for reporting only
	BailFlag_Expected = BIT1,		// indicates that this is expected
};

// team defines
enum 
{
	eTEAM_NOT_SET = -1,

	// These used to reference 'faction' teams for CnC, then script started using them as generic mission teams.
	// However, they did so without realising that some parts of code still assumed these teams were associated with roles. These are removed and renamed now.
	eTEAM_ZERO = 0,		// eTEAM_COP
	eTEAM_ONE = 1,		// eTEAM_VAGOS
	eTEAM_TWO = 2,		// eTEAM_LOST
	eTEAM_THREE = 3,	// eTEAM_MAX_GANGS

	eTEAM_SOLO_TUTORIAL = 5,
	eTEAM_INVALID_TUTORIAL = eTEAM_SOLO_TUTORIAL + 1,

	eTEAM_SCTV = 8,
	eTEAM_FREEMODE = 9,
};

// matchmaking group - this is used to validate that the joining player can join the 
// indicated player group for matchmaking or joining via invite
enum eMatchmakingGroup
{
	MM_GROUP_INVALID = -1,
	MM_GROUP_FREEMODER = 0,
	MM_GROUP_COP,
	MM_GROUP_VAGOS,
	MM_GROUP_LOST,
	MM_GROUP_SCTV,
	MM_GROUP_MAX,
};

#define MAX_ACTIVE_MM_GROUPS (2)

// reasons we can leave a session
enum LeaveSessionReason
{
	Leave_Normal,
	Leave_TransitionLaunch,
	Leave_Kicked,
	Leave_ReservingSlot,
	Leave_Error,
	Leave_KickedAdmin,
    Leave_Num,
};

enum KickSource
{
	Source_KickMessage,
	Source_RemovedFromSession,
	Source_LostConnectionToHost,
};

enum eBlacklistReason
{
	BLACKLIST_UNKNOWN = -1,
	BLACKLIST_CONNECTION,
	BLACKLIST_VOTED_OUT,
	BLACKLIST_NUM,
};

// join response codes
enum eJoinResponseCode
{
	RESPONSE_ACCEPT,
	RESPONSE_DENY_UNKNOWN,
	RESPONSE_DENY_WRONG_SESSION,
	RESPONSE_DENY_NOT_HOSTING,
	RESPONSE_DENY_NOT_READY,
	RESPONSE_DENY_BLACKLISTED,
	RESPONSE_DENY_INVALID_REQUEST_DATA,
	RESPONSE_DENY_INCOMPATIBLE_ASSETS,
	RESPONSE_DENY_SESSION_FULL,
	RESPONSE_DENY_GROUP_FULL,
	RESPONSE_DENY_WRONG_VERSION,
	RESPONSE_DENY_NOT_VISIBLE,
	RESPONSE_DENY_BLOCKING,
	RESPONSE_DENY_AIM_PREFERENCE,
	RESPONSE_DENY_CHEATER,
	RESPONSE_DENY_TIMEOUT,
	RESPONSE_DENY_DATA_HASH,
	RESPONSE_DENY_CREW_LIMIT,
	RESPONSE_DENY_POOL_NORMAL,
	RESPONSE_DENY_POOL_BAD_SPORT,
	RESPONSE_DENY_POOL_CHEATER,
	RESPONSE_DENY_NOT_JOINABLE,
    RESPONSE_DENY_PRIVATE_ONLY,
	RESPONSE_DENY_DIFFERENT_BUILD,
	RESPONSE_DENY_DIFFERENT_PORT,
    RESPONSE_DENY_DIFFERENT_CONTENT_SETTING,
    RESPONSE_DENY_NOT_FRIEND,
	RESPONSE_DENY_REPUTATION,
	RESPONSE_DENY_FAILED_TO_ESTABLISH,
	RESPONSE_DENY_PREMIUM,
	RESPONSE_DENY_SOLO,
	RESPONSE_DENY_ADMIN_BLOCKED,
	RESPONSE_DENY_TOO_MANY_BOSSES,
    RESPONSE_NUM_CODES
};

// bail reasons
enum eBailReason
{
	BAIL_INVALID = -1,
	BAIL_FROM_SCRIPT,
	BAIL_FROM_RAG,
	BAIL_PROFILE_CHANGE,
	BAIL_NEW_CONTENT_INSTALLED,
	BAIL_SESSION_FIND_FAILED,
	BAIL_SESSION_HOST_FAILED,
	BAIL_SESSION_JOIN_FAILED,
	BAIL_SESSION_START_FAILED,
	BAIL_SESSION_ATTR_FAILED,
	BAIL_SESSION_MIGRATE_FAILED,
	BAIL_PARTY_HOST_FAILED,
	BAIL_PARTY_JOIN_FAILED,
	BAIL_JOINING_STATE_TIMED_OUT,
	BAIL_NETWORK_ERROR,
	BAIL_TRANSITION_LAUNCH_FAILED,
	BAIL_END_TIMED_OUT,
	BAIL_MATCHMAKING_TIMED_OUT,
	BAIL_CLOUD_FAILED,
	BAIL_COMPAT_PACK_CONFIG_INCORRECT,
	BAIL_CONSOLE_BAN,
	BAIL_MATCHMAKING_FAILED,
	BAIL_ONLINE_PRIVILEGE_REVOKED,
	BAIL_SYSTEM_SUSPENDED,
	BAIL_EXIT_GAME,
	BAIL_TOKEN_REFRESH_FAILED,
	BAIL_CATALOG_REFRESH_FAILED,
	BAIL_SESSION_REFRESH_FAILED,
	BAIL_SESSION_RESTART_FAILED,
	BAIL_GAME_SERVER_MAINTENANCE,
	BAIL_GAME_SERVER_FORCE_BAIL,
	BAIL_GAME_SERVER_HEART_BAIL,
	BAIL_GAME_SERVER_GAME_VERSION,
	BAIL_CATALOGVERSION_REFRESH_FAILED,
	BAIL_CATALOG_BUFFER_TOO_SMALL,
	BAIL_INVALIDATED_ROS_TICKET,
	BAIL_NUM,
};

// bail reasons
enum eTransitionBailReason
{
	BAIL_TRANSITION_INVALID = -1,
	BAIL_TRANSITION_SCRIPT,
	BAIL_TRANSITION_JOIN_FAILED,
	BAIL_TRANSITION_HOST_FAILED,
	BAIL_TRANSITION_CANNOT_HOST_OR_JOIN,
	BAIL_TRANSITION_CLEAR_SESSION,
	BAIL_TRANSITION_MAX,
};

// bail sub types
// added here as adding new bail types is problematic for script
enum eBailContext
{
	BAIL_CTX_NONE									= 0x1000,

	// find failed
	BAIL_CTX_FIND_FAILED_JOIN_FAILED				= 0x1100,
	BAIL_CTX_FIND_FAILED_NOT_QUICKMATCH				= 0x1101,
	BAIL_CTX_FIND_FAILED_HOST_FAILED				= 0x1102,
	BAIL_CTX_FIND_FAILED_CANNOT_HOST				= 0x1103,
	BAIL_CTX_FIND_FAILED_WAITING_FOR_PRESENCE		= 0x1104,
	BAIL_CTX_FIND_FAILED_JOINING					= 0x1105,
#if !__FINAL
	BAIL_CTX_FIND_FAILED_CMD_LINE					= 0x1106,
#endif

	// host failed
	BAIL_CTX_HOST_FAILED_ERROR						= 0x1200,
	BAIL_CTX_HOST_FAILED_TIMED_OUT					= 0x1201,
	BAIL_CTX_HOST_FAILED_ON_STARTING				= 0x1202,
#if !__FINAL
	BAIL_CTX_HOST_FAILED_CMD_LINE					= 0x1203,
#endif

	// join failed
	BAIL_CTX_JOIN_FAILED_DIRECT_NO_FALLBACK			= 0x1300,
	BAIL_CTX_JOIN_FAILED_PENDING_INVITE				= 0x1301,
	BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST			= 0x1302,
	BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST_ACTIVITY	= 0x1303,
	BAIL_CTX_JOIN_FAILED_ON_STARTING				= 0x1304,
	BAIL_CTX_JOIN_FAILED_CHECK_MATCHMAKING			= 0x1305,
	BAIL_CTX_JOIN_FAILED_CHECK_JOIN_ACTION			= 0x1306,
	BAIL_CTX_JOIN_FAILED_KICKED						= 0x1307,
	BAIL_CTX_JOIN_FAILED_VALIDATION					= 0x1308,
	BAIL_CTX_JOIN_FAILED_NOTIFY						= 0x1309,
	BAIL_CTX_JOIN_FAILED_RESTART_SIGNALING			= 0x1310,
#if !__FINAL
	BAIL_CTX_JOIN_FAILED_CMD_LINE					= 0x1311,
#endif

	// start failed
	BAIL_CTX_START_FAILED_ERROR						= 0x1400,
#if !__FINAL
	BAIL_CTX_START_FAILED_CMD_LINE					= 0x1401,
#endif

	// joining state timed out
	BAIL_CTX_JOINING_STATE_TIMED_OUT_JOINING		= 0x1500,
	BAIL_CTX_JOINING_STATE_TIMED_OUT_LOBBY			= 0x1501,
	BAIL_CTX_JOINING_STATE_TIMED_OUT_START_PENDING	= 0x1502,
#if !__FINAL
	BAIL_CTX_JOINING_STATE_TIMED_OUT_CMD_LINE		= 0x1503,
#endif

	// transition launch
	BAIL_CTX_TRANSITION_LAUNCH_LEAVING				= 0x1600,
	BAIL_CTX_TRANSITION_LAUNCH_NO_TRANSITION		= 0x1601,
	BAIL_CTX_TRANSITION_LAUNCH_MIGRATE_FAILED		= 0x1602,

	// network error
	BAIL_CTX_NETWORK_ERROR_KICKED					= 0x2000,
	BAIL_CTX_NETWORK_ERROR_OUT_OF_MEMORY			= 0x2001,
	BAIL_CTX_NETWORK_ERROR_EXHAUSTED_EVENTS_POOL	= 0x2002,
	BAIL_CTX_NETWORK_ERROR_CHEAT_DETECTED			= 0x2003,

	// profile change
	BAIL_CTX_PROFILE_CHANGE_SIGN_OUT				= 0x3000,
	BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE		= 0x3001,
	BAIL_CTX_PROFILE_CHANGE_INVITE_DIFFERENT_USER	= 0x3002,
	BAIL_CTX_PROFILE_CHANGE_FROM_STORE				= 0x3003,

	// suspending
	BAIL_CTX_SUSPENDED_SIGN_OUT						= 0x3100,
	BAIL_CTX_SUSPENDED_ON_RESUME					= 0x3101,

	// exit game
	BAIL_CTX_EXIT_GAME_FROM_APP						= 0x4000,
	BAIL_CTX_EXIT_GAME_FROM_FLOW					= 0x4001,

	BAIL_CTX_CATALOG_FAIL							= 0x5000,
	BAIL_CTX_CATALOG_FAIL_TRIGGER_VERSION_CHECK		= 0x5001,
	BAIL_CTX_CATALOG_FAIL_TRIGGER_CATALOG_REFRESH	= 0x5002,
	BAIL_CTX_CATALOG_TASKCONFIGURE_FAILED			= 0x5003,
	BAIL_CTX_CATALOG_VERSION_TASKCONFIGURE_FAILED	= 0x5004,
	BAIL_CTX_CATALOG_DESERIALIZE_FAILED				= 0x5005,

    // privilege
    BAIL_CTX_PRIVILEGE_DEFAULT                        = 0x6001,
    BAIL_CTX_PRIVILEGE_PROMOTION_ENDED                = 0x6002,
    BAIL_CTX_PRIVILEGE_SESSION_ACTION_AFTER_PROMOTION = 0x6003,

	BAIL_CTX_COUNT //count, add all other contexts above this
};

enum 
{
	SCHEMA_INVALID = -1,
	SCHEMA_GENERAL = 0,
	SCHEMA_GROUPS,
	SCHEMA_ACTIVITY,
	SCHEMA_NUM,
};

enum MatchmakingAimBucket
{
	AimBucket_Invalid = -1,
	AimBucket_Assisted = 0,
	AimBucket_Free,
	AimBucket_Max,
};

enum eMultiplayerPool
{
	POOL_NORMAL = 0,
	POOL_CHEATER,
	POOL_BAD_SPORT,
	POOL_NUM,
};

enum 
{
    FINAL_BUILD_TYPE = 0,
    BANK_BUILD_TYPE,
    DEV_BUILD_TYPE,
    NUM_BUILD_TYPES,
};

enum eSoloSessionReason
{
	SOLO_SESSION_HOSTED_FROM_SCRIPT = 0,
	SOLO_SESSION_HOSTED_FROM_MM,
	SOLO_SESSION_HOSTED_FROM_JOIN_FAILURE,
	SOLO_SESSION_HOSTED_FROM_QM,
	SOLO_SESSION_HOSTED_FROM_UNKNOWN,
	SOLO_SESSION_MIGRATE_WHEN_JOINING_VIA_MM,
	SOLO_SESSION_MIGRATE_WHEN_JOINING_DIRECT,
	SOLO_SESSION_MIGRATE,
	SOLO_SESSION_TO_GAME_FROM_SCRIPT,
	SOLO_SESSION_HOSTED_TRANSITION_WITH_GAMERS,
	SOLO_SESSION_HOSTED_TRANSITION_WITH_FOLLOWERS,
	SOLO_SESSION_PLAYERS_LEFT,

	// 
	SOLO_SESSION_MAX,
};

enum eSoloSessionSource
{
	SOLO_SOURCE_HOST,
	SOLO_SOURCE_JOIN,
	SOLO_SOURCE_MIGRATE,
	SOLO_SOURCE_PLAYERS_LEFT,
};

enum eSessionVisibility
{
	VISIBILITY_OPEN,
	VISIBILITY_PRIVATE,
	VISIBILITY_CLOSED_FRIEND,
	VISIBILITY_CLOSED_CREW,
	VISIBILITY_MAX,
};

enum RealTimeMultiplayerScriptFlags
{
	RTS_None = 0,
	RTS_Disable = 0x1,
	RTS_PlayerOverride = 0x2,
	RTS_SpectatorOverride = 0x4
};

enum SentFromGroup
{
	Group_None = 0,
	Group_LocalPlayer = BIT0,
	Group_Friends = BIT1,
	Group_SmallCrew = BIT2,
	Group_LargeCrew = BIT3,
	Group_RecentPlayer = BIT4,
	Group_SameSession = BIT5,
	Group_SameTeam = BIT6,

	// special groups
	Group_Invalid = BIT7,
	Group_NotSameSession = BIT8,

	Group_Num = 9,

	// some combinations
	Groups_CloseContacts = Group_Friends | Group_SmallCrew,

	// everyone mask
	Group_All =
		Group_LocalPlayer |
		Group_Friends |
		Group_SmallCrew |
		Group_LargeCrew |
		Group_RecentPlayer |
		Group_SameSession |
		Group_SameTeam,
};

//Beta build
#if __DEV
#define DEFAULT_BUILD_TYPE_VALUE  (DEV_BUILD_TYPE)

//BankRealease build
#elif __BANK
#define DEFAULT_BUILD_TYPE_VALUE  (BANK_BUILD_TYPE)
#else

//Default build Type
#define DEFAULT_BUILD_TYPE_VALUE  (FINAL_BUILD_TYPE)
#endif

static const unsigned MAX_UNIQUE_MATCH_ID_CHARS = 30;

static const unsigned UGC_CONTENT_ID_MAX = 23;
static const unsigned INVALID_GARAGE_INDEX = 0;
static const u8 NO_PROPERTY_ID = static_cast<u8>(-1);
static const u8 NO_MENTAL_STATE = 0;
static const u8 MAX_PROPERTY_ID = 254;
static const u8 MAX_MENTAL_STATE = 7;
static const unsigned MAX_UNIQUE_CREWS = 4;
static const unsigned MAX_CLAN_TAG_LENGTH = 16;

static const int INVALID_ACTIVITY_TYPE = -1; 
static const int NO_ACTIVITY_ISLAND = -1; 

// common script 
class scrScMembershipInfo
{
public:
	scrValue m_HasMembership;
	scrValue m_MembershipStartPosix;
	scrValue m_MembershipEndPosix;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrScMembershipInfo);

#endif

//PURPOSE: Network Shop defines
#define __NET_SHOP_ACTIVE (__BANK || RSG_PC)

#if __NET_SHOP_ACTIVE
typedef u64   NetShopRequestId;         //Unique Id that identifies a server request.
typedef u32   NetShopItemId;             //Unique Id that identifies a item.
typedef u32   NetShopCategory;          //Unique Id that identifies the category of the item.
typedef u32   NetShopTransactionId;     //Unique Id that identifies a transaction in our list of transactions.
typedef u32   NetShopTransactionType;   //Unique Id that identifies the type of the transaction. See Defines for transaction types.
typedef u32   NetShopTransactionAction; //Unique Id that identifies transaction actions.

#define NET_SHOP_INVALID_TRANS_ID 0xFFFFFFFF //define an invalid transaction id
#define NET_SHOP_INVALID_ITEM_ID  0xFFFFFFFF //define an invalid Item Id

#define NET_SHOP_ONLY(x) x
#else // !__NET_SHOP_ACTIVE
#define NET_SHOP_ONLY(x)
#endif// __NET_SHOP_ACTIVE

#if RSG_PC
#define NOTPC_ONLY(x)
#else //!RSG_PC
#define NOTPC_ONLY(x) x
#endif //RSG_PC

// Windfall
#define NET_ENABLE_WINDFALL_METRICS			(IS_GEN9_PLATFORM)

#if NET_ENABLE_WINDFALL_METRICS
#define WINDFALL_METRICS_ONLY(...) __VA_ARGS__
#else //!NET_ENABLE_WINDFALL_METRICS
#define WINDFALL_METRICS_ONLY(...)
#endif //NET_ENABLE_WINDFALL_METRICS

// Membership
#define NET_ENABLE_MEMBERSHIP_FUNCTIONALITY (RL_SC_MEMBERSHIP_ENABLED && IS_GEN9_PLATFORM)
#define NET_ENABLE_MEMBERSHIP_METRICS		(IS_GEN9_PLATFORM)

// should map to rlScMembershipState::State_NotMember
#define NET_DUMMY_NOT_MEMBER_STATE (2)

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
#define MEMBERSHIP_ONLY(...) __VA_ARGS__
#else //!NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
#define MEMBERSHIP_ONLY(...)
#endif //NET_ENABLE_MEMBERSHIP_FUNCTIONALITY

//eof 

