//
// name:        NetworkDebug.cpp
// description: Network debug widget functionality
// written by:  Daniel Yelland
//

#include "network/Debug/NetworkDebug.h"

#include "net/netdiag.h"
#include "net/nethardware.h"
#include "rline/ros/rlroscommon.h"
#include "rline/savemigration/rlsavemigration.h"
#include "grcore/debugdraw.h"
#include "diag/output.h"
#include "fwnet/neteventmgr.h"
#include "fwnet/netlog.h"
#include "fwnet/netutils.h"
#include "fwnet/netremotelog.h"
#include "fwmaths/random.h"
#include "fwnet/netscgameconfigparser.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/Viewport.h"
#include "debug/debugscene.h"
#include "debug/VectorMap.h"
#include "debug/DebugRecorder.h"
#include "control/gamelogic.h"
#include "core/game.h"
#include "frontend/PauseMenu.h"
#include "frontend/MiniMap.h"
#include "game/clock.h"
#include "game/weather.h"
#include "glassPaneSyncing/GlassPaneInfo.h"
#include "Network/Network.h"
#include "network/NetworkInterface.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "network/events/NetworkEventTypes.h"
#include "network/Debug/NetworkBot.h"
#include "network/Debug/NetworkDebugMsgs.h"
#include "network/Debug/NetworkDebugPlayback.h"
#include "network/Debug/NetworkSoakTests.h"
#include "network/general/NetworkDamageTracker.h"
#include "network/general/NetworkFakePlayerNames.h"
#include "network/general/NetworkSynchronisedScene.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"
#include "network/general/ScriptWorldState/WorldStates/NetworkEntityAreaWorldState.h"
#include "network/Live/livemanager.h"
#include "network/Live/NetworkTelemetry.h"
#include "network/Live/NetworkDebugTelemetry.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/objects/entities/netobjbike.h"
#include "network/objects/entities/netobjboat.h"
#include "network/objects/entities/netobjDoor.h"
#include "network/objects/entities/netobjheli.h"
#include "network/objects/entities/netobjplane.h"
#include "network/objects/entities/netobjsubmarine.h"
#include "network/objects/entities/netobjtrailer.h"
#include "network/objects/entities/netobjtrain.h"
#include "network/objects/entities/netobjpickup.h"
#include "network/objects/entities/netobjpickupplacement.h"
#include "network/objects/entities/netobjglasspane.h"
#include "network/objects/entities/netobjplayer.h"
#include "network/objects/entities/netobjvehicle.h"
#include "network/objects/prediction/NetBlenderBoat.h"
#include "network/objects/prediction/NetBlenderHeli.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/Party/NetworkParty.h"
#include "network/players/NetworkPlayerMgr.h"
#include "network/roaming/RoamingBubbleMgr.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/Shop/NetworkShopping.h"
#include "peds/Ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "pickups/data/pickupids.h"
#include "pickups/PickUpManager.h"
#include "scene/DownloadableTextureManager.h"
#include "scene/world/GameWorld.h"
#include "scene/RegdRefTypes.h"
#include "script/commands_object.h"
#include "Script/commands_task.h"
#include "script/script.h"
#include "script/streamedscripts.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/General/TaskBasic.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streaminginfo.h"
#include "vehicles/vehiclepopulation.h"
#include "system/controlMgr.h"
#include "system/simpleallocator.h"
#include "Peds/rendering/PedVariationDebug.h"
#include "debug/BlockView.h"
#include "vehicles/Trailer.h"
#include "vehicles/train.h"
#include "vehicles/vehiclefactory.h"
#include "stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "task/system/TaskTypes.h"
#include "text/TextConversion.h"

//#include "vehicles/trailer.h"
#include "vehicleai/vehicleintelligence.h"

// rage headers
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "net/net.h"
#include "System/bootmgr.h"

#include "rline/rltitleid.h"
namespace rage
{
	extern const rlTitleId* g_rlTitleId;
}

#if !__FINAL
static const unsigned MAX_DEBUG_TEXT = 128;
static char s_debugText[MAX_DEBUG_TEXT];
static bool s_bDisplayPauseText = false;

static char s_httpReqTrackerText[256];
static bool s_bDisplayHttpReqTrackerText = false;
static int s_httpReqTrackerTextDisplayTimer = 0;

static bool s_DisplayConfigurationDetails = false;
#endif // !__FINAL

#if __BANK
#include "text/messages.h"
#include "text/TextConversion.h"
static bool s_bDisplayDebugText = false;
static int s_debugDisplayTimer = 0;

static bool s_debugWeaponDamageEvent = false;

static bool s_failCloneRespawnNetworkEvent = false;
static bool s_failRespawnNetworkEvent = false;

static bool		s_UseNearestScenarioWarp		= false;
static float	s_UseNearestScenarioRange		= 5.0f;

#endif // __BANK

BANK_ONLY(static bool s_DisplayAverageRTT = false);

RAGE_DEFINE_SUBCHANNEL(net, debug, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_debug

NETWORK_OPTIMISATIONS()

PARAM(netdebugdisplay, "[network] Display network object debug information");
PARAM(netscriptdisplay, "[network] Display network script object debug information");
PARAM(netBindPoseClones, "[network] Perform a bind pose analysis of the clone peds in a network game");
PARAM(netBindPoseOwners, "[network] Perform a bind pose analysis of the owner peds in a network game");
PARAM(netBindPoseCops,   "[network] Perform a bind pose analysis of the cop peds in a network game");
PARAM(netBindPoseNonCops,"[network] Perform a bind pose analysis of the non cop peds in a network game");

PARAM(netTaskMotionPedDebug, "[network] Print out all task motion ped syncing to the TTY");

PARAM(netDebugTaskLogging, "[network] Activate the task logging on specific tasks for network debugging");

PARAM(netdebugdisplaytargetobjects,"[network] Allow display of target object data");
PARAM(netdebugdisplaytargetpeds,"[network] Allow display of target ped data");
PARAM(netdebugdisplaytargetplayers,"[network] Allow display of target player data");
PARAM(netdebugdisplaytargetvehicles,"[network] Allow display of target vehicle data");

PARAM(netdebugdisplaypredictioninfo,"[network] Allow display of prediction info for targetted types");
PARAM(netdebugdisplayvisibilityinfo,"[network] Allow display of visibility info for targetted types");
PARAM(netdebugdisplayghostinfo,"[network] Allow display of ghost info for targetted types");
PARAM(netdebugdisplayconfigurationdetails, "[network] Display network configuration details");

PARAM(suppressPauseTextMP, "Suppresses the pause text that appears onscreen when someone pauses the game in MP");
XPARAM(notimeouts); //Disable timeouts for network sessions
XPARAM(netCompliance); // Force XR/TRC compliance on non-final builds

PARAM(debugTargetting, "debug targetting");
PARAM(debugTargettingLOS, "debug targetting LOS");

PARAM(nobandwidthlogging, "disable bandwidth logging");
PARAM(minimalpredictionlogging, "minimal prediction logging");

PARAM(netdebugdesiredvelocity, "[network] Enable debugging of desired velocities");

PARAM(netdebugnetworkentityareaworldstate, "[network] Enable visual debugging of network entity area world state");

PARAM(netdebugupdatelevels, "[network] Include update level information with the prediction logs");

PARAM(netMpAccessCode, "[network] Override multiplayer access code", "Default multiplayer access behaviour", "All", "");
PARAM(netMpAccessCodeReady, "[network] Force the access code check to be flagged as ready", "Default multiplayer access behaviour", "All", "");
PARAM(netMpAccessCodeNotReady, "[network] Force the access code check to be flagged as not ready", "Default multiplayer access behaviour", "All", "");

namespace rage
{
extern sysMemAllocator* g_rlAllocator;
}

// Network bandwidth overview
PF_PAGE(Network, "Network Overview");
PF_GROUP(Total);
PF_LINK(Network,Total);
PF_VALUE_FLOAT(TotalIn, Total);
PF_VALUE_FLOAT(TotalOut, Total);

PF_GROUP_OFF(ArrayManager);
PF_LINK(Network,ArrayManager);
PF_VALUE_FLOAT(ArrayManagerIn, ArrayManager);
PF_VALUE_FLOAT(ArrayManagerOut, ArrayManager);

PF_GROUP_OFF(EventManager);
PF_LINK(Network,EventManager);
PF_VALUE_FLOAT(EventManagerIn, EventManager);
PF_VALUE_FLOAT(EventManagerOut, EventManager);

PF_GROUP_OFF(ObjectManager);
PF_LINK(Network,ObjectManager);
PF_VALUE_FLOAT(ObjectManagerIn, ObjectManager);
PF_VALUE_FLOAT(ObjectManagerOut, ObjectManager);

PF_GROUP_OFF(PlayerManager);
PF_LINK(Network,PlayerManager);
PF_VALUE_FLOAT(PlayerManagerIn, PlayerManager);
PF_VALUE_FLOAT(PlayerManagerOut, PlayerManager);

PF_GROUP_OFF(HeaderOverhead);
PF_LINK(Network,HeaderOverhead);
PF_VALUE_FLOAT(HeaderOverheadIn, HeaderOverhead);
PF_VALUE_FLOAT(HeaderOverheadOut, HeaderOverhead);

PF_GROUP_OFF(MiscSystems);
PF_LINK(Network,MiscSystems);
PF_VALUE_FLOAT(MiscSystemsIn, MiscSystems);
PF_VALUE_FLOAT(MiscSystemsOut, MiscSystems);

PARAM(disablemessagelogs, "disable message logging");
PARAM(predictionLogging, "enable prediction logging");
PARAM(overrideSpawnPoints, "[network] Do not use spawn points in network sessions");
PARAM(recordbandwidth, "[network] Record bandwidth");

namespace rage
{
XPARAM(ragenet_tty);
}   //rage

#if !__NO_OUTPUT
extern int s_BankNetworkAccessCode;
extern bool s_BankForceReadyForAccessCheck;
extern bool s_BankForceNotReadyForAccessCheck;
#endif

namespace NetworkDebug
{
#if !__FINAL
	static NetworkDebugMessageHandler s_MessageHandler;
#endif

    //This bool indicates whether we should record bandwidth or not
    #if __DEV
    // static bool recordBandwidth = true;
    #else
    // static bool recordBandwidth = false;
    #endif

static unsigned gNumPedsToCreate    = 0;
static unsigned gNumCarsToCreate    = 0;
static unsigned gNumObjectsToCreate = 0;
static unsigned gNumPickupsToCreate = 0;

//This is the sample interval for the bandwidth recorders used in
//CNetwork.
#if __BANK

static bkBank                *ms_pBank                     = 0;
static bkGroup				 *ms_pDebugGroup			   = 0;
static bkGroup				 *ms_pSyncTreeGroup			   = 0;
static int					  ms_FocusEntityId;
static bool					  ms_bCatchCloneBindPose	   = false;
static bool					  ms_bCatchOwnerBindPose	   = false;
static bool					  ms_bCatchCopBindPose		   = false;
static bool					  ms_bCatchNonCopBindPose	   = false;
static bool					  ms_bTaskMotionPedDebug	   = false;
static bool					  ms_bDebugTaskLogging		   = false;
static bool					  ms_bViewFocusEntity;
static bool                   ms_bOverrideSpawnPoint       = false;
static bool                   ms_bInMPCutscene			   = false;
static bool                   ms_bInMocapCutscene		   = false;
static bool                   ms_bRenderNetworkStatusBars  = true;
static bool                   m_bAllowOwnershipChange;    // allow ownership of non-player entities to be passed between different players
static bool                   m_bAllowProximityOwnership; // allow ownership of non-player entities to move to the closest player
static bool                   m_bUsePlayerCameras;        // uses the player cameras to dictate sync update rates
static bool                   m_bSkipAllSyncs;            // skip all sync message
static bool                   m_bIgnoreGiveControlEvents; // ignores any give control events from other machines
static bool                   m_bIgnoreVisibilityChecks;  // ignores the checks for visibility and proximity to other players when spawning population
static bool					  m_bEnablePhysicsOptimisations;// toggle enabling recent physics optimisations
static bool                   s_bMinimalPredictionLogging  = false;

static bool                   s_LogExtraDebugNetworkPopulation = false; // toggle loads of extra population is logged to Object Manager

static NetworkDisplaySettings ms_netDebugDisplaySettings;
static const char            *s_Players[MAX_NUM_ACTIVE_PLAYERS];
static unsigned int           s_NumPlayers = 0;
static const char            *s_KickPlayers[MAX_NUM_ACTIVE_PLAYERS];
static unsigned int           s_NumKickPlayers = 0;
static netLog                *s_PopulationLog = 0;
// warp to player
static bkCombo*     s_PlayerCombo = 0;
static int          s_PlayerComboIndex = 0;

// Kick Player Combo
static bkCombo*     s_KickPlayerCombo = 0;
static int          s_KickPlayerComboIndex = 0;

static int   s_bRemotePlayerFadeOutimer = 0;

// debug cameras
static bkCombo*     s_FollowPedCombo = 0;
static const char*  s_FollowPedTargets[MAX_NUM_ACTIVE_PLAYERS];
static int          s_FollowPedTargetIndices[MAX_NUM_ACTIVE_PLAYERS];
static unsigned int s_NumFollowPedTargets = 0;
static int          s_FollowPedIndex      = 0;
// break points
static bool			m_breakTypes[NUM_BREAK_TYPES];
static const char*	m_breakTypeNames[] = 
{
	"Focus entity create",
	"Focus entity update",
	"Focus entity write sync",
	"Focus entity write sync node",
	"Focus entity read sync",
	"Focus entity read sync node",
	"Focus entity clone create",
	"Focus entity clone remove",
	"Focus entity try to migrate",
	"Focus entity migrate",
	"Focus entity destroy",
	"Focus entity no longer needed",
	"Focus entity collision off",
	"Focus entity sync node ready"
};
CompileTimeAssert((sizeof(m_breakTypeNames) / sizeof(char *)) == NUM_BREAK_TYPES);
static bool			sm_FlagNoTimeouts = false;

static const char* s_RosPrivileges[] = 
{
	"NONE",
	"CREATE_TICKET",
	"PROFILE_STATS_WRITE",
	"MULTIPLAYER",
	"LEADERBOARD_WRITE",
	"CLOUD_STORAGE_READ",
	"CLOUD_STORAGE_WRITE",
	"BANNED",
	"CLAN",
	"PRIVATE_MESSAGING",
	"SOCIAL_CLUB",
	"PRESENCE_WRITE",
	"DEVELOPER",
	"HTTP_REQUEST_LOGGING",
	"UGCWRITE",
	"PURCHASEVC",
	"TRANSFERVC",
	"CANBET",
	"CREATORS",
	"CLOUD_TUNABLES",
	"CHEATER_POOL",
	"COMMENTWRITE",
	"CANUSELOTTERY",
	"ALLOW_MEMBER_REDIRECT",
	"PLAYED_LAST_GEN",
	"UNLOCK_SPECIAL_EDITION",
	"CONTENT_CREATOR",
	"CONDUCTOR"
};
const int NUMBER_OF_ROS_PRIVILEGES = sizeof(s_RosPrivileges) / sizeof(s_RosPrivileges[0]);
CompileTimeAssert(NUMBER_OF_ROS_PRIVILEGES == RLROS_NUM_PRIVILEGEID);
static bool s_RosPrivilegesTypes[NUMBER_OF_ROS_PRIVILEGES];

// Toggle the enabling of network XR/TRC compliance even in non-final builds (same effect as -netCompliance)
static bool s_ForceNetworkCompliance = false;

#if __DEV
static RegdPed      s_FollowPedTarget(NULL);
#endif

// debug logging
static bkCombo*     s_UpdateRateCombo = 0;

// convert gamertag to XUID

bool ms_bWidgetsCreated = false;

static size_t ms_SCxnMgrHeapTideMark = CNetwork::SCXNMGR_HEAP_SIZE;
//static size_t ms_RlineHeapTideMark   = CLiveManager::RLINE_HEAP_SIZE;

//static CPed* ms_lastCreatedPed = NULL;

//static unsigned	heapUsage = 0;
//static unsigned	maxHeapUsage = 0;

static bool ms_DisplayPredictionFocusOnVectorMap    = false;
static bool ms_DisplayOrientationOnlyOnVectorMap    = false;
static bool ms_EnablePredictionLogging              = false;
static bool ms_RestrictPredictionLoggingToSelection = false;
static int  ms_PredictionLoggingSelection           = 0;

enum
{
    E_RESTRICT_TO_FOCUS_ENTITY,
    E_RESTRICT_TO_VEHICLES,
    E_RESTRICT_TO_PEDS,
    E_NUM_RESTRICTIONS
};

static const char *ms_PredictionLoggingSelections[E_NUM_RESTRICTIONS] =
{
    "Focus Entity Only",
    "Vehicles Only",
    "Peds Only"
};

static unsigned ms_NumPredictionLoggingSelections = sizeof(ms_PredictionLoggingSelections)  / sizeof(const char *);

static netLog *s_PredictionLog = 0;
static u32     m_cachedPredictionLogSize = 0;

//Clan id used to read/update leaderboards
static unsigned s_LeaderboardClanIndex = 0;
static rlClanId s_LeaderboardCurrentClanId = 0;

// debug entity damage
static bool  s_DebugEntityDamage = false;

// debug ped navmesh bandwidth optimisation
static unsigned gCurrentPedNavmeshPosStartTime        = 0;
static unsigned gNumPedNavmeshPosSyncRescans          = 0;
static unsigned gNumPedNavmeshPosSyncGroundProbes     = 0;
static unsigned gLastNumPedNavmeshPosSyncRescans      = 0;
static unsigned gLastNumPedNavmeshPosSyncGroundProbes = 0;

//Debug missing damage events due to ped health being set to 0 without calling UpdateDamageTracker()
BANK_ONLY( static bool s_DebugPedMissingDamageEvents = false; )

#if __BANK
// Network access states
static int s_NetworkAccessComboIndex = 0;

static const char *s_NetworkAccessCombo[] =
{
    "Access_BankDefault",
    "Access_Invalid",
    "Access_Granted",
    "Access_Deprecated_TunableNotFound",
    "Access_Deprecated_TunableFalse",
    "Access_Denied_MultiplayerLocked",
    "Access_Denied_InvalidProfileSettings",
    "Access_Denied_PrologueIncomplete",
    "Access_Denied_NotSignedIn",
    "Access_Denied_NotSignedOnline",
    "Access_Denied_NoOnlinePrivilege",
    "Access_Denied_NoRosPrivilege",
    "Access_Denied_MultiplayerDisabled",
    "Access_Denied_NoTunables",
    "Access_Denied_NoBackgroundScript",
    "Access_Denied_NoNetworkAccess"
};
#endif // __BANK

// debug prediction
class CFocusEntityPredictionData
{
public:
    
    CFocusEntityPredictionData() :
    m_PredictionFocusEntityID(NETWORK_INVALID_OBJECT_ID)
    , m_TimeAveragesReset(0)
    , m_AverageTimeBetweenUpdates(0)
    , m_NumUpdateTimes(0)
    {
    }

    ObjectId GetFocusEntityID() { return m_PredictionFocusEntityID; }

    u32 GetAverageTimeBetweenUpdate()
    {
        return m_AverageTimeBetweenUpdates;
    }

    void UpdateAverageTimeBetweenUpdates()
    {
        m_AverageTimeBetweenUpdates = 0;
        u32 total   = 0;

        for(unsigned index = 1; index < m_NumUpdateTimes; index++)
        {
            u32 timeBetweenUpdates = m_UpdateTimes[index] - m_UpdateTimes[index - 1];
            total += timeBetweenUpdates;
        }

        if(m_NumUpdateTimes > 1)
        {
            m_AverageTimeBetweenUpdates = total / m_NumUpdateTimes;
        }
    }

    void Update()
    {
        if(NetworkInterface::IsGameInProgress())
        {
            u32 timeSinceAveragesReset = NetworkInterface::GetNetworkTime() - m_TimeAveragesReset;
            if(m_PredictionFocusEntityID != ms_FocusEntityId || (timeSinceAveragesReset > MAX_TIME_BEFORE_AVERAGES_RESET))
            {
                if(timeSinceAveragesReset > MAX_TIME_BEFORE_AVERAGES_RESET)
                {
                    UpdateAverageTimeBetweenUpdates();
                }
                m_PredictionFocusEntityID = static_cast<ObjectId>(ms_FocusEntityId);
                Reset();
            }

            netObject *networkObject = NetworkInterface::GetNetworkObject(static_cast<ObjectId>(ms_FocusEntityId));
            netBlenderLinInterp *netBlender = networkObject ? SafeCast(netBlenderLinInterp, networkObject->GetNetBlender()) : 0;

            if(netBlender)
            {
                u32 lastUpdateTime = 0;
                netBlender->GetLastPositionReceived(&lastUpdateTime);

                if(m_NumUpdateTimes == 0 || (m_UpdateTimes[m_NumUpdateTimes-1] != lastUpdateTime))
                {
                    if(m_NumUpdateTimes == MAX_CACHED_UPDATE_TIMES)
                    {
                        UpdateAverageTimeBetweenUpdates();
                        Reset();
                    }

                    m_UpdateTimes[m_NumUpdateTimes] = lastUpdateTime;
                    m_NumUpdateTimes++;
                }
            }
        }
    }

    void Reset()
    {
        m_TimeAveragesReset = NetworkInterface::GetNetworkTime();

        for(unsigned index = 0; index < MAX_CACHED_UPDATE_TIMES; index++)
        {
            m_UpdateTimes[index] = 0;
        }

        m_NumUpdateTimes = 0;
    }

private:
    static const unsigned MAX_TIME_BEFORE_AVERAGES_RESET = 1000;
    static const unsigned MAX_CACHED_UPDATE_TIMES        = 10;

    ObjectId m_PredictionFocusEntityID;
    u32      m_TimeAveragesReset;
    u32      m_AverageTimeBetweenUpdates;
    u32      m_NumUpdateTimes;
    u32      m_UpdateTimes[MAX_CACHED_UPDATE_TIMES];
};

u32 gCurrentPopStartTime                = 0;
u32 gLastNumPredictionPopsPerSecond     = 0;
u32 gNumPredictionPopsPerSecond         = 0;
u32 gTotalPredictionPops                = 0;
u32 gCurrentBBCheckStartTime            = 0;
u32 gCurrentNoCollisionStartTime        = 0;
u32 gMaxBBChecksBeforeWarpStartTime     = 0;
u32 gLastNumPredictionBBChecksPerSecond = 0;
u32 gNumPredictionBBChecksPerSecond     = 0;
u32 gMaxTimeWithNoCollision             = 0;
u32 gMaxBBChecksBeforeWarp              = 0;

bool gFollowNextPredictionPop = false;

ObjectId gMaxNoCollisionObjectID        = NETWORK_INVALID_OBJECT_ID;
ObjectId gMaxBBChecksObjectID           = NETWORK_INVALID_OBJECT_ID;

CFocusEntityPredictionData gFocusEntityPredictionData;

struct OrientationBlendFailureData
{
    phBound *m_Bound1;
    phBound *m_Bound2;
    Mat34V   m_Matrix1;
    Mat34V   m_Matrix2;
};

static OrientationBlendFailureData gOrientationBlendFailures[MAX_NUM_NETOBJVEHICLES];
static unsigned gNumOrientationBlendFailures  = 0;
static bool     gbShowFailedOrientationBlends = false;

enum
{
    E_MESSAGES_LOG,
    E_BANDWIDTH_LOG,
    E_ARRAY_MANAGER_LOG,
    E_EVENT_MANAGER_LOG,
    E_OBJECT_MANAGER_LOG,
    E_PLAYER_MANAGER_LOG,
    E_PREDICTION_LOG,
    E_NUM_LOG_OPTIONS
};

static const char *s_LogFiles[E_NUM_LOG_OPTIONS] =
{
    "Messages",
    "Bandwidth",
    "Array Manager",
    "Event Manager",
    "Object Manager",
    "Player Manager",
    "Prediction"
};

static bool s_EnableLogs[E_NUM_LOG_OPTIONS] =
{
    true,
    true,
    true,
    true,
    true,
	true,
	true
};

static unsigned int s_NumLogFiles = sizeof(s_LogFiles) / sizeof(const char *);

static bool s_DisplayLoggingStatistics = false;

static bool  sanityCheckHeapOnDoAllocate;
//static float s_TalkerProximity = 0.0f;

static u16 gSectorX = 0;
static u16 gSectorY = 0;
static u16 gSectorZ = 0;
static Vector3 gSectorPos;
static unsigned gMaxGangTutorialPlayers = MAX_NUM_PHYSICAL_PLAYERS;
static bool s_PassControlInTutorial = false;

static bool s_ForceCloneTasksOutOfScope = false;

// Debug CPU usage
static bool s_DisplayBasicCPUInfo               = false;
static bool s_DisplaySyncTreeBatchingInfo       = false;
static bool s_DisplaySyncTreeReads              = false;
static bool s_DisplaySyncTreeWrites             = false;
static bool s_DisplayPlayerLocationInfo         = false;
static bool s_DisplayPedNavmeshSyncStats        = false;
static bool s_DisplayExpensiveFunctionCallStats = false;
static bool s_DisplayLatencyStats               = false;

// keep track of average number of calls to netObject::ManageUpdateLevel()
static const unsigned MAX_UPDATE_SAMPLES = 30;
static const unsigned SAMPLE_INTERVAL    = 1000;
static DataAverageCalculator<MAX_UPDATE_SAMPLES, SAMPLE_INTERVAL> m_AverageManageUpdateLevelCallsPerUpdate;
static unsigned gNumManageUpdateLevelCalls = 0;

// keep track of average number of sync updates sent per frame
static DataAverageCalculator<MAX_UPDATE_SAMPLES, SAMPLE_INTERVAL> m_AverageSyncCloneCallsPerUpdate;

// keep track of average number of sync updates sent per frame
static DataAverageCalculator<MAX_UPDATE_SAMPLES, 0> m_AverageInboundLatency;
static DataAverageCalculator<MAX_UPDATE_SAMPLES, 0> m_AverageOutboundCloneSyncLatency;
static DataAverageCalculator<MAX_UPDATE_SAMPLES, 0> m_AverageOutboundConnectionSyncLatency;

// Debug model usage
static u32 s_ModelToDump = 0;

// Keep track of last number of outgoing reliable messages
unsigned gLastNumOutgoingReliables = 0;
#endif // __BANK

#if !__FINAL

// debug time
static float		m_lastTimeScale = 1.0f;
static bool			m_lastTimePause = false;
static bool			m_timeScaleLocallyAltered = false;
static bool			m_timePauseLocallyAltered = false;
static bool			m_timeStepRemotelyAltered = false;

void NetworkDebugMessageHandler::OnMessageReceived(const ReceivedMessageData &messageData)
{
	unsigned msgId = 0;

	if (gnetVerify(messageData.IsValid()))
	{
		if(netMessage::GetId(&msgId, messageData.m_MessageData, messageData.m_MessageDataSize))
		{
			if (msgId == msgDebugTimeScale::MSG_ID())
			{
				msgDebugTimeScale msg;

				if(AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
				{
					fwTimer::SetDebugTimeScale(msg.m_Scale);
					m_lastTimeScale = msg.m_Scale;
					m_timeScaleLocallyAltered = false;
				}
			}
			else if (msgId == msgDebugTimePause::MSG_ID())
			{
				msgDebugTimePause msg;

				if(AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
				{
					if (msg.m_Paused)
					{
						fwTimer::StartUserPause(msg.m_ScriptsPaused);

						m_lastTimePause = true;

						if (msg.m_SingleStep)
						{
							fwTimer::SetWantsToSingleStepNextFrame();
							m_timeStepRemotelyAltered = true;
						}
						else if (!PARAM_suppressPauseTextMP.Get())
						{
							char message[MAX_DEBUG_TEXT];

							const float spentRealMoney = StatsInterface::GetFloatStat(STAT_MPPLY_STORE_MONEY_SPENT);

							formatf(message, MAX_DEBUG_TEXT, "%s has paused the game~n~~n~Character WALLET(%" I64FMT "d)+BANK(%" I64FMT "d) = EVC(%" I64FMT "d)+PVC(%" I64FMT "d), Store Spent(%f)"
								, messageData.m_FromPlayer ? messageData.m_FromPlayer->GetLogName() : "??"
								, MoneyInterface::GetVCWalletBalance()
								, MoneyInterface::GetVCBankBalance()
								, MoneyInterface::GetEVCBalance()
								, MoneyInterface::GetPVCBalance()
								, spentRealMoney);

							DisplayPauseMessage(message);
						}
					}
					else
					{
						fwTimer::EndUserPause();
					}

					m_timePauseLocallyAltered = false;
				}
			}
#if __BANK
			else if (msgId == msgDebugNoTimeouts::MSG_ID())
			{
				if (NetworkInterface::IsGameInProgress() && !PARAM_notimeouts.Get())
				{
					msgDebugNoTimeouts msg;

					if(AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
					{
						CNetwork::ResetConnectionPolicies(msg.m_NoTimeouts ? 0 : CNetwork::GetTimeoutTime());
						sm_FlagNoTimeouts = msg.m_NoTimeouts;

						gnetDebug1("Player \"%s\" flag notimeouts set=\"%s\" timeout value=\"%d\""
												,messageData.m_FromPlayer ? messageData.m_FromPlayer->GetLogName() : "??"
												,msg.m_NoTimeouts ? "TRUE" : "FALSE"
												,CNetwork::GetTimeoutTime());
					}
				}
			}
			else if (msgId == msgDebugAddFailMark::MSG_ID())
			{
				if(NetworkInterface::IsGameInProgress())
				{
					msgDebugAddFailMark msg;

					if(AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
					{
						if (gnetVerify(msg.m_FailMarkMsg))
						{
							NetworkDebug::RegisterFailMark(msg.m_FailMarkMsg,msg.m_bVerboseFailmark);
#if !__NO_OUTPUT
							char message[MAX_DEBUG_TEXT];
							formatf(message, MAX_DEBUG_TEXT, "Player \"%s\" Added \"%s\".", messageData.m_FromPlayer ? messageData.m_FromPlayer->GetLogName() : "??", msg.m_FailMarkMsg);
							gnetDebug1("%s", message);
#endif // !__NO_OUTPUT

							// stall detect logging
							gnetDebug1("NetStallRemote :: %s added failmark. Message: %s", messageData.m_FromPlayer ? messageData.m_FromPlayer->GetLogName() : "??", msg.m_FailMarkMsg);
						}
					}
				}
			}
			else if (msgId == msgSyncDRDisplay::MSG_ID())
			{
				msgSyncDRDisplay msg;
				if(AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
				{
					DR_ONLY(debugPlayback::SyncToNet(msg));
				}
			}
#endif // __BANK
		}
	}
}

void RenderDebugMessage()
{
	static Vector2 texPos(0.1f, 0.5f);

	CTextLayout TextLayout;
	TextLayout.Render(texPos, s_debugText);
}

bool ShouldRenderConfigurationDetails()
{
	int commandLine = 0;
	if(PARAM_netdebugdisplayconfigurationdetails.Get(commandLine))
	{
		if(commandLine == 0)
			return false;
		
		return true;
	}

	return s_DisplayConfigurationDetails;
}

void RenderConfigurationDetails()
{
	static char s_NativeEnvironmentText[64];
	static char s_RosEnvironmentText[64];
	static char s_SkuText[64];
	static char s_CountryText[64];

	// cache data so that we don't need to build the strings every frame
	static rlRosEnvironment s_RosEnvironment = rlRosGetEnvironment();
	static rlEnvironment s_NativeEnvironment = rlGetNativeEnvironment();

	// current values
	rlRosEnvironment rosEnvironment = rlRosGetEnvironment();
	rlEnvironment nativeEnvironment = rlGetNativeEnvironment();

	// always do this once
	static bool s_First = true; 

	if(s_First || (rosEnvironment != s_RosEnvironment))
		formatf(s_RosEnvironmentText, "RosEnv: %s (Stage: %s)", rlRosGetEnvironmentAsString(rosEnvironment), rlIsUsingStagingEnvironment() ? "True" : "False");

	if(s_First || (nativeEnvironment != s_NativeEnvironment))
		formatf(s_NativeEnvironmentText, "PlatformEnv: %s", rlGetEnvironmentString(nativeEnvironment));

	if(s_First)
	{
#if RSG_PROSPERO
		// ps5 has a different SKU for disc / digital
		formatf(s_SkuText, "Sku: %s (%s)", sysAppContent::GetBuildLocaleCode(), sysAppContent::IsDiskSKU() ? "Disc" : "Digital");
#else
		formatf(s_SkuText, "Sku: %s", sysAppContent::GetBuildLocaleCode());
#endif

		// use platform services to get the country code
		char countryCode[3];
		countryCode[0] = '\0';

		// only apply a code if we have a valid one
		bool hasValidCountryCode = false;

#if RSG_SCE
		if(RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
			hasValidCountryCode = g_rlNp.GetCountryCode(NetworkInterface::GetLocalGamerIndex(), countryCode);
#elif RSG_XBOX
		hasValidCountryCode = g_SysService.GetSystemCountry(countryCode);
#endif

		formatf(s_CountryText, "AccountCountry: %s", hasValidCountryCode ? countryCode : "Unknown");
	}
		
	s_RosEnvironment = rosEnvironment;
	s_NativeEnvironment = nativeEnvironment;
	s_First = false;

	static const float s_xStart = 0.99f;
	static const float s_yStart = 0.05f;
	static const float s_ySep = 0.04f;
	static const float s_DefaultScale = 0.4f;

	CTextLayout textLayout;
	textLayout.SetWrap(Vector2(0.03f, 0.97f));
	textLayout.SetOrientation(FONT_RIGHT);
	textLayout.SetScale(Vector2(s_DefaultScale, s_DefaultScale));
	textLayout.SetColor(Color32(50, 200, 50));

	// ROS environment
	textLayout.Render(Vector2(s_xStart, s_yStart), s_RosEnvironmentText);

	// Platform environment
	textLayout.Render(Vector2(s_xStart, s_yStart + s_ySep), s_NativeEnvironmentText);

	// SKU text
	textLayout.Render(Vector2(s_xStart, s_yStart + s_ySep + s_ySep), s_SkuText);

	// Country text
	textLayout.Render(Vector2(s_xStart, s_yStart + s_ySep + s_ySep + s_ySep), s_CountryText);
}

void RenderHttpReqTrackerMessage()
{
	static Vector2 texPos(0.0f, 0.8f);
	static Vector2 texScale(0.5f, 0.5f);

	CTextLayout TextLayout;
	TextLayout.SetScale(texScale);
	TextLayout.SetColor(Color32(255, 0, 0));
	TextLayout.Render(texPos, s_httpReqTrackerText);
}

#endif // !__FINAL

#if __BANK
static void UpdateBandwidthProfiler();
static void UpdateBandwidthLog();
static void UpdateCombos();
static void RenderNetworkStatusBars(unsigned int totalIn,
                             unsigned int totalOut,
                             unsigned int total,
                             unsigned int heapUsage,
                             unsigned int heapSize);
void SetupBlendingWidgets();

struct PeakTracker
{
    static const unsigned TIME_TO_RESET_PEAKS = 1000;

    PeakTracker() :
    m_CurrentCount(0)
    , m_PeakCount(0)
    , m_LastPeakReset(0)
    {
    }

    void IncrementCount()
    {
        m_CurrentCount++;
    }

    u32 GetPeakCount()
    {
        return m_PeakCount;
    }

    void Update()
    {
        u32 currentTime    = sysTimer::GetSystemMsTime();
        u32 timeSinceReset = currentTime - m_LastPeakReset;

        if(timeSinceReset >= TIME_TO_RESET_PEAKS)
        {
            m_PeakCount     = 0;
            m_LastPeakReset = currentTime;
        }

        if(m_CurrentCount > m_PeakCount)
        {
            m_PeakCount = m_CurrentCount;
        }

        m_CurrentCount = 0;
    }

    u32 m_CurrentCount;
    u32 m_PeakCount;
    u32 m_LastPeakReset;
};

struct PeakDurationTracker
{
    static const unsigned TIME_TO_RESET_PEAKS = 1000;

    PeakDurationTracker() :
    m_CurrentDuration(0)
    , m_PeakDuration(0)
    , m_LastPeakReset(0)
    {
    }

    void AddDuration(float time)
    {
        m_CurrentDuration += time;
    }

    float GetPeak()
    {
        return m_PeakDuration;
    }

    void Update()
    {
        u32 currentTime    = sysTimer::GetSystemMsTime();
        u32 timeSinceReset = currentTime - m_LastPeakReset;

        if(timeSinceReset >= TIME_TO_RESET_PEAKS)
        {
            m_PeakDuration  = 0.0f;
            m_LastPeakReset = currentTime;
        }

        if(m_CurrentDuration > m_PeakDuration)
        {
            m_PeakDuration = m_CurrentDuration;
        }

        m_CurrentDuration = 0;
    }

    float m_CurrentDuration;
    float m_PeakDuration;
    u32   m_LastPeakReset;
};

static PeakTracker g_PeakPedCreates;
static PeakTracker g_PeakVehicleCreates;
static PeakTracker g_PeakOtherCreates;
static PeakTracker g_PeakPedRemoves;
static PeakTracker g_PeakVehicleRemoves;
static PeakTracker g_PeakOtherRemoves;
static PeakDurationTracker g_PeakCloneCreateDuration;
static PeakDurationTracker g_PeakCloneRemoveDuration;

const netPlayer *GetPlayerFromUpdateRateIndex(int playerIndex)
{
    const netPlayer* pTargetPlayer = 0;

    int playerCount = 0;

	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
    const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        const netPlayer *pPlayer = activePlayers[index];

		if(playerCount == playerIndex)
		{
			pTargetPlayer = pPlayer;
            break;
		}

        playerCount++;
	}

    return pTargetPlayer;
}

bool OverrideSpawnPoint()
{
    return ms_bOverrideSpawnPoint;
}

void ClearOverrideSpawnPoint()
{
    ms_bOverrideSpawnPoint = false;
}

void AddPedNavmeshPosSyncRescan()
{
    gNumPedNavmeshPosSyncRescans++;
}

void AddPedNavmeshPosGroundProbe()
{
    gNumPedNavmeshPosSyncGroundProbes++;
}

void AddManageUpdateLevelCall()
{
    gNumManageUpdateLevelCalls++;
}

void AddPedCloneCreate()
{
    g_PeakPedCreates.IncrementCount();
}

void AddVehicleCloneCreate()
{
    g_PeakVehicleCreates.IncrementCount();
}

void AddOtherCloneCreate()
{
    g_PeakOtherCreates.IncrementCount();
}

void AddPedCloneRemove()
{
    g_PeakPedRemoves.IncrementCount();
}

void AddVehicleCloneRemove()
{
    g_PeakVehicleRemoves.IncrementCount();
}

void AddOtherCloneRemove()
{
    g_PeakOtherRemoves.IncrementCount();
}

void AddCloneCreateDuration(float time)
{
    g_PeakCloneCreateDuration.AddDuration(time);
}

void AddCloneRemoveDuration(float time)
{
    g_PeakCloneRemoveDuration.AddDuration(time);
}

void AddInboundLatencySample(unsigned latencyMS)
{
    m_AverageInboundLatency.AddSample(static_cast<float>(latencyMS));
}

void AddOutboundLatencyCloneSyncSample(unsigned latencyMS)
{
    m_AverageOutboundCloneSyncLatency.AddSample(static_cast<float>(latencyMS));
}

void AddOutboundLatencyConnectionSyncSample(unsigned latencyMS)
{
    m_AverageOutboundConnectionSyncLatency.AddSample(static_cast<float>(latencyMS));
}


void GetPedNavmeshPosSyncStats(unsigned &numRescans, unsigned &numGroundProbes)
{
    const unsigned COLLECTION_INTERVAL = 1000;
    u32 currentTime = sysTimer::GetSystemMsTime();

    if((currentTime - gCurrentPedNavmeshPosStartTime) > COLLECTION_INTERVAL)
    {
        gCurrentPedNavmeshPosStartTime        = currentTime;
        gLastNumPedNavmeshPosSyncRescans      = gNumPedNavmeshPosSyncRescans;
        gLastNumPedNavmeshPosSyncGroundProbes = gNumPedNavmeshPosSyncGroundProbes;
        gNumPedNavmeshPosSyncRescans          = 0;
        gNumPedNavmeshPosSyncGroundProbes     = 0;
    }

    // reset the counting if this wasn't displayed in the last frame
    static u32 lastFrameCalled = 0;

    if((fwTimer::GetFrameCount() - lastFrameCalled) > 1)
    {
        gLastNumPedNavmeshPosSyncRescans      = 0;
        gLastNumPedNavmeshPosSyncGroundProbes = 0;
    }

    numRescans      = gLastNumPedNavmeshPosSyncRescans;
    numGroundProbes = gLastNumPedNavmeshPosSyncGroundProbes;
}

bool IsPredictionFocusEntityID(ObjectId objectID)
{
    return (gFocusEntityPredictionData.GetFocusEntityID() == objectID);
}

u32 GetPredictionFocusEntityUpdateAverageTime()
{
    return gFocusEntityPredictionData.GetAverageTimeBetweenUpdate();
}

static void ViewFocusEntity();

static const char *GetPredictionPopObjectName(NetworkObjectType objectType)
{
    if(IsVehicleObjectType(objectType))
    {
        return "Vehicle";
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_DOOR:
            return "Door";
        case NET_OBJ_TYPE_OBJECT:
            return "Object";
        case NET_OBJ_TYPE_PED:      // intentional fall through
        case NET_OBJ_TYPE_PLAYER:
            return "Ped";
        case NET_OBJ_TYPE_PICKUP:
            return "Pickup";
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
            return "Pickup Placement";
        case NET_OBJ_TYPE_GLASS_PANE:
            return "Glass pane";
        default:
            gnetAssertf(0, "Unknown game object type!");
            return "Game Object";
        }
    }
}

void AddPredictionPop(ObjectId objectID, NetworkObjectType objectType, bool fixedByNetwork)
{
    bool outputPopToTTY = NetworkDebug::IsPredictionLoggingEnabled();

    if(gFollowNextPredictionPop)
    {
        if(!fwTimer::IsGamePaused())
        {
            ms_bViewFocusEntity = true;
            ms_FocusEntityId    = objectID;
            UpdateFocusEntity(false);
            ViewFocusEntity();

            fwTimer::StartUserPause(true);

            outputPopToTTY = true;
        }

        gFollowNextPredictionPop = false;
    }

    if(outputPopToTTY)
    {
		// This is here to see whether GetLogName is somehow causing the hang or something in the block above instead.
        char logName[256];
        CNetwork::GetObjectManager().GetLogName(logName, sizeof(logName), objectID);
        gnetDebug2("%s has popped! %s%s", GetPredictionPopObjectName(objectType), logName, fixedByNetwork ? " (fixed by network)" : "");
    }

    gNumPredictionPopsPerSecond++;
    gTotalPredictionPops++;
}

void AddPredictionBBCheck()
{
    gNumPredictionBBChecksPerSecond++;
}

void AddNumBBChecksBeforeWarp(u32 numChecks, ObjectId objectID)
{
    if(numChecks > gMaxBBChecksBeforeWarp)
    {
        gMaxBBChecksBeforeWarp = numChecks;
        gMaxBBChecksObjectID   = objectID;
    }
}

unsigned GetNumPredictionPopsPerSecond()
{
    const unsigned PREDICTION_POP_INTERVAL = 1000;
    u32 currentTime = sysTimer::GetSystemMsTime();

    if((currentTime - gCurrentPopStartTime) > PREDICTION_POP_INTERVAL)
    {
        gCurrentPopStartTime            = currentTime;
        gLastNumPredictionPopsPerSecond = gNumPredictionPopsPerSecond;
        gNumPredictionPopsPerSecond     = 0;
    }

    return gLastNumPredictionPopsPerSecond;
}

unsigned GetNumPredictionBBChecksPerSecond()
{
    const unsigned PREDICTION_BB_CHECK_INTERVAL = 1000;
    u32 currentTime = sysTimer::GetSystemMsTime();

    if((currentTime - gCurrentBBCheckStartTime) > PREDICTION_BB_CHECK_INTERVAL)
    {
        gCurrentBBCheckStartTime            = currentTime;
        gLastNumPredictionBBChecksPerSecond = gNumPredictionBBChecksPerSecond;
        gNumPredictionBBChecksPerSecond     = 0;
    }

    return gLastNumPredictionBBChecksPerSecond;
}

unsigned GetMaxTimeWithNoCollision()
{
    const unsigned NO_COLLISION_CHECK_INTERVAL = 1000;
    u32 currentTime = sysTimer::GetSystemMsTime();

    if((currentTime - gCurrentNoCollisionStartTime) > NO_COLLISION_CHECK_INTERVAL)
    {
        gCurrentNoCollisionStartTime = currentTime;
        gMaxTimeWithNoCollision      = 0;
        gMaxNoCollisionObjectID      = NETWORK_INVALID_OBJECT_ID;
    }

    return gMaxTimeWithNoCollision;
}

unsigned GetMaxBBChecksBeforeWarp()
{
    const unsigned MAX_BB_BEFORE_WARP_CHECK_INTERVAL = 1000;
    u32 currentTime = sysTimer::GetSystemMsTime();

    if((currentTime - gMaxBBChecksBeforeWarpStartTime) > MAX_BB_BEFORE_WARP_CHECK_INTERVAL)
    {
        gMaxBBChecksBeforeWarpStartTime = currentTime;
        gMaxBBChecksBeforeWarp          = 0;
        gMaxBBChecksObjectID            = NETWORK_INVALID_OBJECT_ID;
    }

    return gMaxBBChecksBeforeWarp;
}

void AddOrientationBlendFailure(phBound *bound1, phBound *bound2, Mat34V_In matrix1, Mat34V_In matrix2)
{
    if(gbShowFailedOrientationBlends)
    {
        if(gnetVerifyf(gNumOrientationBlendFailures < MAX_NUM_NETOBJVEHICLES, "Adding too many orientation blend failures!"))
        {
            gOrientationBlendFailures[gNumOrientationBlendFailures].m_Bound1  = bound1;
            gOrientationBlendFailures[gNumOrientationBlendFailures].m_Bound2  = bound2;
            gOrientationBlendFailures[gNumOrientationBlendFailures].m_Matrix1 = matrix1;
            gOrientationBlendFailures[gNumOrientationBlendFailures].m_Matrix2 = matrix2;
            gNumOrientationBlendFailures++;
        }
        else
        {
            gnetDebug2("Dropping orientation blend failure");
        }
    }
}

unsigned GetNumPredictionCollisionDisabled()
{
    unsigned numDisabledCollisions = 0;

    s32 index = CNetObjVehicle::GetPool()->GetSize();

    while(index--)
    {
        CNetObjVehicle *netObjVehicle = CNetObjVehicle::GetPool()->GetSlot(index);

        if(netObjVehicle && netObjVehicle->IsClone())
        {
            CNetBlenderPhysical *blender = SafeCast(CNetBlenderPhysical, netObjVehicle->GetNetBlender());
            CVehicle            *vehicle = NetworkUtils::GetVehicleFromNetworkObject(netObjVehicle);

            if(vehicle && !vehicle->GetIsAnyFixedFlagSet())
            {
                if(blender->GetDisableCollisionTimer() > 0 && ((vehicle->GetNoCollisionFlags() & NO_COLLISION_NETWORK_OBJECTS) != 0))
                {
                    u32 timeWithNoCollision = blender->GetTimeWithNoCollision();

                    if(timeWithNoCollision > gMaxTimeWithNoCollision)
                    {
                        gMaxTimeWithNoCollision = timeWithNoCollision;
                        gMaxNoCollisionObjectID = netObjVehicle->GetObjectID();
                    }

                    numDisabledCollisions++;
                }
            }
        }
    }

    return numDisabledCollisions;
}

unsigned GetNumMovingVehiclesFixedByNetwork()
{
    unsigned numMovingVehiclesFixedByNetwork = 0;

    s32 index = CNetObjVehicle::GetPool()->GetSize();

    while(index--)
    {
        CNetObjVehicle *netObjVehicle = CNetObjVehicle::GetPool()->GetSlot(index);

        if(netObjVehicle && netObjVehicle->IsClone())
        {
            CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(netObjVehicle);

            if(vehicle && vehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
            {
                CNetBlenderPhysical *blender = SafeCast(CNetBlenderPhysical, netObjVehicle->GetNetBlender());

                if(blender && blender->GetLastVelocityReceived().Mag2() > 0.01f)
                {
                    numMovingVehiclesFixedByNetwork++;
                }
            }
        }
    }

    return numMovingVehiclesFixedByNetwork;
}

void UpdatePredictionLogForPed(CPed *ped, const atArray<ObjectId> &lastFrameUpdateObjects, atArray<ObjectId> &thisFrameUpdateObjects, bool justPaused)
{
    if(ped)
    {
        CNetObjPed *networkObject = SafeCast(CNetObjPed, ped->GetNetworkObject());

        bool newThisFrame = lastFrameUpdateObjects.Find(networkObject->GetObjectID()) == -1;

        const unsigned PED_LOG_DATA_SIZE = 512;
        char pedLogData[PED_LOG_DATA_SIZE];

		if(newThisFrame)
		{
			formatf(pedLogData, PED_LOG_DATA_SIZE, "%s,,Error, Vel Change, Last Pop,CV Mag,,Curr X,Curr Y,Curr Z, Vel X, Vel Y, Vel Z,Ragdoll,Physics mode,MBR X, MBR Y,,Current Heading, Desired Heading, Target Heading,,Target X,Target Y, Target Z, Predict Vel X, Predict Vel Y, Predict Vel Z,,Predict/Anim Speed Diff,Heading To Move Dir Angle Diff,, Predict Error,, Predict X,Predict Y, Predict Z,,Timestamp,,Flags", networkObject->GetLogName());
			s_PredictionLog->Log(", %s\r\n", pedLogData);
		}

        if(justPaused)
        {
            formatf(pedLogData, PED_LOG_DATA_SIZE, "%s, GAME PAUSED", networkObject->GetLogName());
        }
        else 
        {
            const Vector3 &currPos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
            const Vector3 &currVel = ped->GetVelocity();

            float desiredMoveBlendRatioX = 0.0f;
            float desiredMoveBlendRatioY = 0.0f;
            ped->GetMotionData()->GetDesiredMoveBlendRatio(desiredMoveBlendRatioX, desiredMoveBlendRatioY);

            if(networkObject->IsClone())
            {
                u32 timestamp = 0;

                CNetBlenderPed *netBlender          = static_cast<CNetBlenderPed *>(networkObject->GetNetBlender());
                const Vector3 &blendTarget          = static_cast<CNetBlenderPed *>(networkObject->GetNetBlender())->GetLastPositionReceived(&timestamp);
                Vector3         predictPos          = netBlender ? netBlender->GetLastPredictedPos() : currPos;
                float           error               = netBlender->GetLastDisableZBlending() ? (predictPos - currPos).XYMag() : (predictPos - currPos).Mag();
                float           lastHeadingReceived = static_cast<CNetBlenderPed *>(networkObject->GetNetBlender())->GetLastHeadingReceived();

                unsigned networkBlenderOverrideReason = NBO_NOT_OVERRIDDEN;
                unsigned canBlendFailReason           = CB_SUCCESS;

                if(networkObject->NetworkBlenderIsOverridden(&networkBlenderOverrideReason))
                {
                    if(networkBlenderOverrideReason == NBO_PED_TASKS)
                    {
                        const char *overridingTask = 0;
                        CTask *task = ped->GetPedIntelligence()->GetTaskActive();

                        while(task && (overridingTask == 0))
                        {
                            if(task->IsClonedFSMTask() && static_cast<CTaskFSMClone *>(task)->OverridesNetworkBlender(ped))
                            {
                                overridingTask = TASKCLASSINFOMGR.GetTaskName(task->GetTaskType());
                            }

                            task = task->GetSubTask();
                        }

                        if(overridingTask == 0)
                        {
                            overridingTask = "No Task";
                        }

                        formatf(pedLogData, PED_LOG_DATA_SIZE, "%s, NETWORK BLENDER OVERRIDDEN - %s (%s),%.2f,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s,,,,%.2f,%.2f,%.2f,,,,,,,,,,,,,,%.2f,%.2f,%.2f,,%u",
                            networkObject->GetLogName(),
                            NetworkUtils::GetNetworkBlenderOverriddenReason(networkBlenderOverrideReason),
                            overridingTask ? overridingTask : "No Task",
                            error,
                            currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
							ped->GetUsingRagdoll() ? "Ragdoll" : "Animated",
							ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) ? "Low LOD" : "High LOD",
                            ped->GetCurrentHeading(), ped->GetDesiredHeading(), lastHeadingReceived,
                            predictPos.x, predictPos.y, predictPos.z, timestamp);
                    }
                    else
                    {
                        formatf(pedLogData, PED_LOG_DATA_SIZE, "%s, NETWORK BLENDER OVERRIDDEN - %s,%.2f,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,,,%.2f,%.2f,%.2f,,,,,,,,,,,,,,%.2f,%.2f,%.2f,,%u",
                            networkObject->GetLogName(),
                            NetworkUtils::GetNetworkBlenderOverriddenReason(networkBlenderOverrideReason),
                            error,
                            currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                            ped->GetCurrentHeading(), ped->GetDesiredHeading(),
                            static_cast<CNetBlenderPed *>(networkObject->GetNetBlender())->GetLastHeadingReceived(),
                            predictPos.x, predictPos.y, predictPos.z, timestamp);
                    }
                }
                else if(!networkObject->CanBlend(&canBlendFailReason))
                {
                    formatf(pedLogData, PED_LOG_DATA_SIZE, "%s, CAN'T BLEND - %s,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,,,,,%.2f,%.2f,,,,,,,,,,,,,,,%.2f,%.2f,%.2f,,%u",
                        networkObject->GetLogName(),
                        NetworkUtils::GetCanBlendErrorString(canBlendFailReason),
                        currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
						ped->GetUsingRagdoll() ? "Ragdoll" : "Animated",
                        ped->GetCurrentHeading(), ped->GetDesiredHeading(),
                        predictPos.x, predictPos.y, predictPos.z, timestamp);
                }
                else
                {
                    Vector3 targetVel        = netBlender->GetLastSnapVel();
                    float   cvMag            = netBlender->GetCorrectionVector().Mag();
                    float   velChange        = netBlender->GetCurrentVelChange();
                    float   predError        = netBlender->GetPredictionError();
                    u32     timeSinceLastPop = (fwTimer::GetTimeInMilliseconds() > netBlender->GetLastPopTime()) ? (fwTimer::GetTimeInMilliseconds() - netBlender->GetLastPopTime()) : 0;

                    float   predictedToAnimatedSpeedDiff = targetVel.Mag() - ped->GetDesiredVelocity().Mag();
                    float   headingToMoveSpeedDiff       = 0.0f;

                    if(targetVel.Mag2() > 0.1f)
                    {
                        Vector3 moveDir       = targetVel;
                        Vector3 forwardVector = VEC3V_TO_VECTOR3(ped->GetTransform().GetB());
                        moveDir.Normalize();

                        headingToMoveSpeedDiff = Acosf(Clamp(moveDir.Dot(forwardVector), -1.0f, 1.0f));
                    }

                    char cloneFlags[128];
                    sprintf(cloneFlags, "%s%s%s%s%s%s%s", netBlender->GetLastDisableZBlending() ? "" : "blendz;", netBlender->IsBlendingInAllDirections() ? "alldir;" : "", !netBlender->IsBlendingHeading() ? "nheadb;" : "", networkObject->NetworkHeadingBlenderIsOverridden() ? "headbo;" : "", networkObject->HasPedStopped() ? "stop;" : "", netBlender->UsedAnimatedRagdollBlendingLastFrame() ? "arb" : "", netBlender->WasResetLastFrame()  ? "rlf" : "");
                    
                    formatf(pedLogData, PED_LOG_DATA_SIZE, "%s,,%.2f,%.2f,%d,%.2f,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.2f,,%.2f,%.2f,%.2f,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,%.2f,%.2f,,%.2f,,%.2f,%.2f,%.2f,,%u,,%s",
                        networkObject->GetLogName(),
                        error, velChange, timeSinceLastPop, cvMag,
                        currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                        ped->GetUsingRagdoll() ? "Ragdoll" : "Animated",
                        ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) ? "Low LOD" : "High LOD",
                        desiredMoveBlendRatioX, desiredMoveBlendRatioY, ped->GetCurrentHeading(), ped->GetDesiredHeading(), static_cast<CNetBlenderPed *>(networkObject->GetNetBlender())->GetLastHeadingReceived(),
                        blendTarget.x, blendTarget.y, blendTarget.z, targetVel.x, targetVel.y, targetVel.z,
                        predictedToAnimatedSpeedDiff,
                        headingToMoveSpeedDiff,
                        predError, predictPos.x, predictPos.y, predictPos.z, timestamp, cloneFlags);
                }
            }           
            else
            {
                char updateLevelDebug[256];
                updateLevelDebug[0] = '\0';

                if(PARAM_netdebugupdatelevels.Get())
                {
                    unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                    netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

                    for (unsigned i = 0; i < numRemotePhysicalPlayers; i++)
                    {
                        netPlayer *remotePlayer = remotePhysicalPlayers[i];

                        if(remotePlayer && remotePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
                        {
                            char playerUpdateLevelInfo[128];
                            formatf(playerUpdateLevelInfo, ", %s:%s", remotePlayer->GetLogName(), GetUpdateLevelString(networkObject->GetUpdateLevel(remotePlayer->GetPhysicalPlayerIndex())));
                            safecat(updateLevelDebug, playerUpdateLevelInfo);
                        }
                    }
                }

                formatf(pedLogData, PED_LOG_DATA_SIZE, "%s,,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.2f,%.2f,%.2f,,%s,,,,,,,,,,,,,,,,",
                    networkObject->GetLogName(),
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                    ped->GetUsingRagdoll() ? "Ragdoll" : "Animated",
                    ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) ? "Low LOD" : "High LOD",
                    desiredMoveBlendRatioX, desiredMoveBlendRatioY, ped->GetCurrentHeading(), ped->GetDesiredHeading(),
                    updateLevelDebug);
            }
        }

        s_PredictionLog->Log(", %s\r\n", pedLogData);

        thisFrameUpdateObjects.PushAndGrow(networkObject->GetObjectID());
    }
}

const char *GetDummyModeString(CVehicle &vehicle)
{
    switch(vehicle.GetVehicleAiLod().GetDummyMode())
    {
    case VDM_REAL:
        return "Real";
    case VDM_DUMMY:
        return "Dummy";
    case VDM_SUPERDUMMY:
        {
            if(vehicle.GetCollider())
            {
                return "SuperDummy (C)";
            }
            else
            {
                return "SuperDummy";
            }
        }
    default:
        return "Unknown";
    }
}

void UpdatePredictionLogForVehicle(CVehicle *vehicle, const atArray<ObjectId> &lastFrameUpdateObjects, atArray<ObjectId> &thisFrameUpdateObjects, bool justPaused)
{
    if(vehicle)
    {
        netObject *networkObject = vehicle->GetNetworkObject();

        bool newThisFrame = lastFrameUpdateObjects.Find(networkObject->GetObjectID()) == -1;

        static const unsigned MAX_PRED_LINE_LENGTH = 700;

        char vehicleLogData[MAX_PRED_LINE_LENGTH];
        if(newThisFrame)
        {
            if(!s_bMinimalPredictionLogging)
            {
                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s, Last Blend Target Time,,Error, Too Far, Vel Change, Vel Error, Last Pop, Speed Diff,,Curr X,Curr Y,Curr Z,Vel X,Vel Y,Vel Z, Speed, Dummy Mode, Throttle, Brake,,Target X,Target Y,Target Z,Target Vel X,Target Vel Y,Target Vel Z,Target Speed, Predict Error,,,Predict X, Predict Y, Predict Z,,Timestamp,, Node List,, Rot X, Rot Y, Rot Z, Ang Vel X, Ang Vel Y, Ang Vel Z, Target Rot X, Target Rot Y, Target Rot Z, Target Ang Vel X, Target Ang Vel Y, Target Ang Vel Z, Blending Orientation", networkObject->GetLogName());
            }
            else
            {
                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s,,Error, Too Far, Vel Change, Last Pop,,Curr X,Curr Y,Curr Z,Vel X,Vel Y,Vel Z, Dummy Mode,,Target X,Target Y,Target Z,Target Vel X,Target Vel Y,Target Vel Z, Predict Error,,,Predict X, Predict Y, Predict Z,,Timestamp,, Node List", networkObject->GetLogName());
            }

            s_PredictionLog->Log(", %s\r\n", vehicleLogData);
        }

        const Vector3 &currPos    = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
        const Vector3 &currVel    = vehicle->GetVelocity();
        const Vector3 &currAngVel = vehicle->GetAngVelocity();
        const char *dummyMode     = GetDummyModeString(*vehicle);
        float throttle            = vehicle->m_vehControls.m_throttle;
        float brake               = vehicle->m_vehControls.m_brake;

        Matrix34 currMatrix   = MAT34V_TO_MATRIX34(vehicle->GetTransform().GetMatrix());

        Vector3 currEulers = currMatrix.GetEulers();

        char nodeList[128];
        CVehicleNodeList *pNodeList = vehicle->GetIntelligence()->GetNodeList();

        if(pNodeList == 0)
        {
            strcpy(nodeList, "None");
        }
        else
        {
            int oldNode = 0;
            int newNode = 1;

            if(pNodeList->GetTargetNodeIndex() > 0)
            {
                oldNode = pNodeList->GetTargetNodeIndex() - 1;
                newNode = pNodeList->GetTargetNodeIndex();
            }
            
            const CNodeAddress &oldNodeAddr = pNodeList->GetPathNodeAddr(oldNode);
            const CNodeAddress &newNodeAddr = pNodeList->GetPathNodeAddr(newNode);
            formatf(nodeList, 128, "(%d:%d)->(%d:%d)", oldNodeAddr.GetRegion(), oldNodeAddr.GetIndex(), newNodeAddr.GetRegion(), newNodeAddr.GetIndex());
        }

        unsigned networkBlenderOverrideReason = NBO_NOT_OVERRIDDEN;
        unsigned canBlendFailReason           = CB_SUCCESS;

        if(justPaused)
        {
            formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s, GAME PAUSED", networkObject->GetLogName());
        }
        else if(networkObject->IsClone() && networkObject->NetworkBlenderIsOverridden(&networkBlenderOverrideReason))
        {
            CNetBlenderVehicle *netBlender = static_cast<CNetBlenderVehicle *>(networkObject->GetNetBlender());
            Vector3             predictPos = netBlender ? netBlender->GetCurrentPredictedPosition() : currPos;
            float               error      = (predictPos - currPos).Mag();

            formatf(vehicleLogData, 256, "%s, NETWORK BLENDER OVERRIDDEN - %s,,%.2f,,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,,,,,,,,,,,,,%.2f,%.2f,%.2f",
                networkObject->GetLogName(),
                NetworkUtils::GetNetworkBlenderOverriddenReason(networkBlenderOverrideReason),
                error,
                currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                predictPos.x, predictPos.y, predictPos.z);
        }
        else if(networkObject->IsClone() && !networkObject->CanBlend(&canBlendFailReason))
        {
            CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(networkObject->GetNetBlender());

            u32 timestamp = netBlender->GetLastSnapTime();

            const Vector3 &blendTarget    = netBlender->GetLastSnapPos();
            const Vector3 &blendVel       = netBlender->GetLastSnapVel();
            const Vector3 &predictPos     = netBlender->GetLastPredictedPos();

            if(!s_bMinimalPredictionLogging)
            {
                formatf(vehicleLogData, 256, "%s, CAN'T BLEND - %s,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,",
                    networkObject->GetLogName(),
                    NetworkUtils::GetCanBlendErrorString(canBlendFailReason),
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z);
            }
            else
            {
                formatf(vehicleLogData, 256, "%s, CAN'T BLEND - %s,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,%.2f,%.2f,%.2f,,%u,,%s",
                    networkObject->GetLogName(),
                    NetworkUtils::GetCanBlendErrorString(canBlendFailReason),
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                    blendTarget.x, blendTarget.y, blendTarget.z, blendVel.x, blendVel.y, blendVel.z,
                    predictPos.x, predictPos.y, predictPos.z, timestamp, nodeList);
            }
        }
        else if(networkObject->IsClone())
        {
            CNetBlenderVehicle *netBlender = static_cast<CNetBlenderVehicle *>(networkObject->GetNetBlender());

            u32 timestamp = netBlender->GetLastSnapTime();
            
            const Vector3 &blendTarget    = netBlender->GetLastSnapPos();
            const Vector3 &blendVel       = netBlender->GetLastSnapVel();
            const Vector3 &blendAngVel    = netBlender->GetLastSnapAngVel();
            const Vector3 &predictPos     = netBlender->GetLastPredictedPos();
            unsigned       lastTargetTime = netBlender->GetLastTargetTime();
            float error                   = (predictPos - currPos).Mag();
            bool  tooFar                  = false;
            float velChange               = netBlender->GetCurrentVelChange();
            float velError                = netBlender->GetLastVelocityError();
            float predError               = netBlender->GetPredictionError();
            u32   timeSinceLastPop        = (fwTimer::GetTimeInMilliseconds() > netBlender->GetLastPopTime()) ? (fwTimer::GetTimeInMilliseconds() - netBlender->GetLastPopTime()) : 0;
            float speedDiff               = currVel.Mag() - blendVel.Mag();
            
            Matrix34 targetMatrix = netBlender->GetLastSnapMatrix();

            Vector3 targetEulers = targetMatrix.GetEulers();

            netBlender->GetPositionDelta(tooFar);

            if(!s_bMinimalPredictionLogging)
            {
                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s,%d,,%.2f,%s,%.2f,%.2f,%d,%.2f,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %.2f, %s, %.2f, %.2f,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %.2f, %.2f,,,%.2f, %.2f, %.2f,,%d,,%s,, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %s",
                                              networkObject->GetLogName(), lastTargetTime,
                                              error, tooFar ? "TOO FAR" : "", velChange, velError, timeSinceLastPop, speedDiff,
                                              currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,currVel.Mag(),
                                              dummyMode, throttle, brake,
                                              blendTarget.x, blendTarget.y, blendTarget.z, blendVel.x, blendVel.y, blendVel.z,blendVel.Mag(),
                                              predError, predictPos.x, predictPos.y, predictPos.z, timestamp, nodeList,
                                              currEulers.x, currEulers.y, currEulers.z, currAngVel.x, currAngVel.y, currAngVel.z,
                                              targetEulers.x, targetEulers.y, targetEulers.z, blendAngVel.x, blendAngVel.y, blendAngVel.z, netBlender->IsBlendingOrientation() ? "b" : "");
            }
            else
            {
                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s,,%.2f,%s,%.2f,%d,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %s,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %.2f,,,%.2f, %.2f, %.2f,,%u,,%s",
                                              networkObject->GetLogName(),
                                              error, tooFar ? "TOO FAR" : "", velChange, timeSinceLastPop,
                                              currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                                              dummyMode,
                                              blendTarget.x, blendTarget.y, blendTarget.z, blendVel.x, blendVel.y, blendVel.z,
                                              predError, predictPos.x, predictPos.y, predictPos.z, timestamp, nodeList);
            }
        }
        else
        {
            if(!s_bMinimalPredictionLogging)
            {
                char updateLevelDebug[256];
                updateLevelDebug[0] = '\0';

                if(PARAM_netdebugupdatelevels.Get())
                {
                    unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                    netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

                    for (unsigned i = 0; i < numRemotePhysicalPlayers; i++)
                    {
                        netPlayer *remotePlayer = remotePhysicalPlayers[i];

                        if(remotePlayer && remotePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
                        {
                            char playerUpdateLevelInfo[128];
                            formatf(playerUpdateLevelInfo, ", %s:%s", remotePlayer->GetLogName(), GetUpdateLevelString(networkObject->GetUpdateLevel(remotePlayer->GetPhysicalPlayerIndex())));
                            safecat(updateLevelDebug, playerUpdateLevelInfo);
                        }
                    }
                }

                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s,,,,,,,,,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %.2f, %s, %.2f, %.2f,,,,,,,,,,,,,,,,,,,%s,, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f,,%s",
                                              networkObject->GetLogName(),
                                              currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,currVel.Mag(),
                                              dummyMode, throttle, brake, nodeList,
                                              currEulers.x, currEulers.y, currEulers.z, currAngVel.x, currAngVel.y, currAngVel.z,
                                              updateLevelDebug);
            }
            else
            {
                formatf(vehicleLogData, MAX_PRED_LINE_LENGTH, "%s,,,,,,,%.2f, %.2f, %.2f,%.2f, %.2f, %.2f, %s,,,,,,,,,,,,,,,,,%s",
                                              networkObject->GetLogName(),
                                              currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,
                                              dummyMode, nodeList);
            }
        }

        s_PredictionLog->Log(", %s\r\n", vehicleLogData);

        thisFrameUpdateObjects.PushAndGrow(networkObject->GetObjectID());
    }
}

#define LOG_OBJECT_PREDICTION(OBJECT_NAME,ERROR,TOO_FAR,VEL_CHANGE,VEL_ERROR,LAST_POP,SPEED_DIFF,CURR_X,CURR_Y,CURR_Z,VEL_X,VEL_Y,VEL_Z,SPEED,TARGET_X,TARGET_Y,TARGET_Z,TARGET_VEL_X,TARGET_VEL_Y,TARGET_VEL_Z,TARGET_SPEED, PREDICT_ERROR,PREDICT_X, PREDICT_Y, PREDICT_Z,TIMESTAMP) \
    ""#OBJECT_NAME",,"#ERROR","#TOO_FAR","#VEL_CHANGE","#VEL_ERROR","#LAST_POP","#SPEED_DIFF",,"#CURR_X","#CURR_Y","#CURR_Z","#VEL_X","#VEL_Y","#VEL_Z","#SPEED",,"#TARGET_X","#TARGET_Y","#TARGET_Z","#TARGET_VEL_X","#TARGET_VEL_Y","#TARGET_VEL_Z","#TARGET_SPEED",,,"#PREDICT_ERROR","#PREDICT_X","#PREDICT_Y","#PREDICT_Z",,"#TIMESTAMP""

void UpdatePredictionLogForObject(CObject *object, const atArray<ObjectId> &lastFrameUpdateObjects, atArray<ObjectId> &thisFrameUpdateObjects, bool justPaused)
{
    if(object && !object->IsADoor())
    {
        netObject *networkObject = object->GetNetworkObject();

        bool newThisFrame = lastFrameUpdateObjects.Find(networkObject->GetObjectID()) == -1;

        char objectLogData[320];
        if(newThisFrame)
        {
            formatf(objectLogData, 320, LOG_OBJECT_PREDICTION(%s,Error, Too Far, Vel Change, Vel Error, Last Pop, Speed Diff,Curr X,Curr Y,Curr Z,Vel X,Vel Y,Vel Z,Speed,Target X,Target Y,Target Z,Target Vel X,Target Vel Y,Target Vel Z,Target Speed,Predict Error,Predict X,Predict Y,Predict Z,Timestamp), networkObject->GetLogName());
            s_PredictionLog->Log(", %s\r\n", objectLogData);
        }

        const Vector3 &currPos = VEC3V_TO_VECTOR3(object->GetTransform().GetPosition());
        const Vector3 &currVel = object->GetVelocity();

        unsigned networkBlenderOverrideReason = NBO_NOT_OVERRIDDEN;
        unsigned canBlendFailReason           = CB_SUCCESS;

        if(justPaused)
        {
            formatf(objectLogData, 320, "%s, GAME PAUSED", networkObject->GetLogName());
        }
        else if(networkObject->IsClone() && networkObject->NetworkBlenderIsOverridden(&networkBlenderOverrideReason))
        {
            CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(networkObject->GetNetBlender());
            Vector3              predictPos = netBlender ? netBlender->GetCurrentPredictedPosition() : currPos;
            float                error      = (predictPos - currPos).Mag();
            u32                  timestamp  = netBlender ? netBlender->GetLastSnapTime() : 0;

            formatf(objectLogData, 256, LOG_OBJECT_PREDICTION(%s,%s%s,%.2f,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,,,,,,%.2f,%.2f,%.2f,%u),
                networkObject->GetLogName(),
                "NETWORK BLENDER OVERRIDDEN - ", NetworkUtils::GetNetworkBlenderOverriddenReason(networkBlenderOverrideReason),
                error,
                currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z, currVel.Mag(),
                predictPos.x, predictPos.y, predictPos.z,
                timestamp);
        }
        else if(networkObject->IsClone() && !networkObject->CanBlend(&canBlendFailReason))
        {
            CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(networkObject->GetNetBlender());
            if (netBlender)
            {
                const Vector3& blendPos = netBlender->GetLastSnapPos();
                u32 timestamp = netBlender->GetLastSnapTime();

                formatf(objectLogData, 256, LOG_OBJECT_PREDICTION(%s,%s%s,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,,,,,,%u),
                    networkObject->GetLogName(),
                    "CAN'T BLEND - ", NetworkUtils::GetCanBlendErrorString(canBlendFailReason),
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z, currVel.Mag(),
                    blendPos.x, blendPos.y, blendPos.z,
                    timestamp);
            }
            else
            {
                formatf(objectLogData, 256, LOG_OBJECT_PREDICTION(%s,%s%s,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,,,,,,,,,,,,),
                    networkObject->GetLogName(),
                    "CAN'T BLEND(No blender) - ", NetworkUtils::GetCanBlendErrorString(canBlendFailReason),
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z, currVel.Mag());
            }
        }
        else if(networkObject->IsClone())
        {
            CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(networkObject->GetNetBlender());

            if (gnetVerifyf(netBlender, "Object %s doesn't have a networkBlender", networkObject->GetLogName()))
            {
                u32 timestamp = netBlender->GetLastSnapTime();
                const Vector3 &blendTarget = netBlender->GetLastSnapPos();
                const Vector3 &blendVel = netBlender->GetLastSnapVel();
                const Vector3 &predictPos = netBlender->GetLastPredictedPos();
                float error = (predictPos - currPos).Mag();
                bool  tooFar = false;
                float velChange = netBlender->GetCurrentVelChange();
                float velError = netBlender->GetLastVelocityError();
                float predError = netBlender->GetPredictionError();
                u32   timeSinceLastPop = (fwTimer::GetTimeInMilliseconds() > netBlender->GetLastPopTime()) ? (fwTimer::GetTimeInMilliseconds() - netBlender->GetLastPopTime()) : 0;
                float speedDiff = currVel.Mag() - blendVel.Mag();
                netBlender->GetPositionDelta(tooFar);

                formatf(objectLogData, 320, LOG_OBJECT_PREDICTION(%s, %.2f, %s, %.2f, %.2f, %u, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %u),
                    networkObject->GetLogName(),
                    error, tooFar ? "TOO FAR" : "", velChange, velError, timeSinceLastPop, speedDiff,
                    currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z, currVel.Mag(),
                    blendTarget.x, blendTarget.y, blendTarget.z, blendVel.x, blendVel.y, blendVel.z, blendVel.Mag(),
                    predError, predictPos.x, predictPos.y, predictPos.z, timestamp);
            }
        }
        else
        {
            char updateLevelDebug[256];
            updateLevelDebug[0] = '\0';

            if(PARAM_netdebugupdatelevels.Get())
            {
                unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

                for (unsigned i = 0; i < numRemotePhysicalPlayers; i++)
                {
                    netPlayer *remotePlayer = remotePhysicalPlayers[i];

                    if(remotePlayer && remotePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
                    {
                        char playerUpdateLevelInfo[128];
                        formatf(playerUpdateLevelInfo, ", %s:%s", remotePlayer->GetLogName(), GetUpdateLevelString(networkObject->GetUpdateLevel(remotePlayer->GetPhysicalPlayerIndex())));
                        safecat(updateLevelDebug, playerUpdateLevelInfo);
                    }
                }
            }

            formatf(objectLogData, 320, LOG_OBJECT_PREDICTION(%s,,,,,,,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,,,,,,,,,,,),
                                          networkObject->GetLogName(),
                                          currPos.x, currPos.y, currPos.z, currVel.x, currVel.y, currVel.z,currVel.Mag(),
                                          updateLevelDebug);
        }

        s_PredictionLog->Log(", %s\r\n", objectLogData);

        thisFrameUpdateObjects.PushAndGrow(networkObject->GetObjectID());
    }
}

void UpdatePredictionLog()
{
    static atArray<ObjectId>  updatedObjects1;
    static atArray<ObjectId>  updatedObjects2;
    static atArray<ObjectId> *lastFrameUpdateObjects = &updatedObjects1;
    static atArray<ObjectId> *thisFrameUpdateObjects = &updatedObjects2;

    static bool wasPaused  = fwTimer::IsGamePaused();
    bool        isPaused   = fwTimer::IsGamePaused();
    bool        justPaused = isPaused && !wasPaused;

    wasPaused = isPaused;

    if(isPaused && !justPaused)
    {
        return;
    }

    atArray<ObjectId> *temp = lastFrameUpdateObjects;
    lastFrameUpdateObjects  = thisFrameUpdateObjects;
    thisFrameUpdateObjects  = temp;

    thisFrameUpdateObjects->Reset();

	if (s_PredictionLog->GetLogSizeInBytes() < m_cachedPredictionLogSize)
	{
		lastFrameUpdateObjects->Reset();
		m_cachedPredictionLogSize = s_PredictionLog->GetLogSizeInBytes();
	}
	else
	{
		m_cachedPredictionLogSize = s_PredictionLog->GetLogSizeInBytes();
	}

    if(NetworkDebug::IsPredictionLoggingEnabled() && s_PredictionLog)
    {
        if(ms_RestrictPredictionLoggingToSelection && ms_PredictionLoggingSelection == E_RESTRICT_TO_FOCUS_ENTITY)
        {
            netObject *networkObject = NetworkInterface::GetNetworkObject(gFocusEntityPredictionData.GetFocusEntityID());
            CEntity   *entity        = networkObject ? networkObject->GetEntity() : 0;

            if(entity)
            {
                if(entity->GetIsTypePed())
                {
                    CPed *ped = SafeCast(CPed, entity);
                    UpdatePredictionLogForPed(ped, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                }
                else if(entity->GetIsTypeVehicle())
                {
                    CVehicle *vehicle = SafeCast(CVehicle, entity);
                    UpdatePredictionLogForVehicle(vehicle, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                }
                else if(entity->GetIsTypeObject())
                {
                    CObject *object = SafeCast(CObject, entity);
                    UpdatePredictionLogForObject(object, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                }
            }
        }
        else
        {
            bool logPeds     = !ms_RestrictPredictionLoggingToSelection || ms_PredictionLoggingSelection == E_RESTRICT_TO_PEDS;
            bool logVehicles = !ms_RestrictPredictionLoggingToSelection || ms_PredictionLoggingSelection == E_RESTRICT_TO_VEHICLES;
            bool logObjects  = !ms_RestrictPredictionLoggingToSelection;
            
            netObject* objects[MAX_NUM_NETOBJECTS];
            unsigned numObjects = CNetwork::GetObjectManager().GetAllObjects(objects, MAX_NUM_NETOBJECTS, true, false);

            for(unsigned index = 0; index < numObjects; index++)
            {
                netObject *networkObject = objects[index];

                if(networkObject)
                {
                    CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

                    if(ped && (ped->IsPlayer() || !s_bMinimalPredictionLogging))
                    {
                        if(logPeds)
                        {
                            UpdatePredictionLogForPed(ped, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                        }
                    }
                    else
                    {
                        CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

                        if(vehicle && logVehicles)
                        {
                            UpdatePredictionLogForVehicle(vehicle, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                        }
                        else if(!s_bMinimalPredictionLogging)
                        {
                            CObject *object = NetworkUtils::GetCObjectFromNetworkObject(networkObject);

                            if(object && logObjects)
                            {
                                UpdatePredictionLogForObject(object, *lastFrameUpdateObjects, *thisFrameUpdateObjects, justPaused);
                            }
                        }
                    }
                }
            }
        }
    }
}

void DisplayCarOnVectorMap(CVehicle *vehicle, const Vector3 &coors, Vector3 &forward, Vector3 &side, Color32 col)
{
    side    *= (vehicle->GetBoundingBoxMax().x * 3.0f);
    forward *= (vehicle->GetBoundingBoxMax().y * 3.0f);

    CVectorMap::DrawLine(Vector3(coors.x + side.x + forward.x, coors.y + side.y + forward.y, 0.0f), Vector3(coors.x - side.x + forward.x, coors.y - side.y + forward.y, 0.0f), col, col, true);		// forward bumper
    CVectorMap::DrawLine(Vector3(coors.x + side.x - forward.x, coors.y + side.y - forward.y, 0.0f), Vector3(coors.x - side.x - forward.x, coors.y - side.y - forward.y, 0.0f), col, col, true);		// rear bumper
    CVectorMap::DrawLine(Vector3(coors.x - side.x - forward.x, coors.y - side.y - forward.y, 0.0f), Vector3(coors.x - side.x + forward.x, coors.y - side.y + forward.y, 0.0f), col, col, true);		// left side
    CVectorMap::DrawLine(Vector3(coors.x + side.x - forward.x, coors.y + side.y - forward.y, 0.0f), Vector3(coors.x + side.x + forward.x, coors.y + side.y + forward.y, 0.0f), col, col, true);		// right side

    CVectorMap::DrawLine(Vector3(coors.x + side.x + 0.4f * forward.x, coors.y + side.y + 0.4f * forward.y, 0.0f), Vector3(coors.x - side.x + 0.4f * forward.x, coors.y - side.y + 0.4f * forward.y, 0.0f), col, col, true);		// line a bit before back from forward bumper (so that we can the orientation of the car)
}

void DisplayPredictionFocusOnVectorMap()
{
    if(ms_DisplayPredictionFocusOnVectorMap)
    {
        netObject *networkObject = NetworkInterface::GetNetworkObject(gFocusEntityPredictionData.GetFocusEntityID());
        CEntity   *entity        = networkObject ? networkObject->GetEntity() : 0;

        if(entity && entity->GetIsTypeVehicle())
        {
            CVehicle *vehicle = SafeCast(CVehicle, entity);
            //CVectorMap::DrawVehicle(vehicle, Color32(1.0f, 1.0f, 1.0f), true);

            Vector3	coors = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
	        Vector3	forward = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetB());
	        Vector3	side = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetA());
            Color32 col(1.0f, 1.0f, 1.0f);

            DisplayCarOnVectorMap(vehicle, coors, forward, side, col);

            if(vehicle->IsNetworkClone())
            {
                CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, vehicle->GetNetworkObject()->GetNetBlender());

                if(netBlenderPhysical)
                {
                    if(!ms_DisplayOrientationOnlyOnVectorMap)
                    {
                        coors   = netBlenderPhysical->GetLastPredictedPos();
                    }
                    forward = netBlenderPhysical->GetLastMatrixReceived().b;
                    side    = netBlenderPhysical->GetLastMatrixReceived().a;

                    Color32 col(0.0f, 1.0f, 0.0f);
                    DisplayCarOnVectorMap(vehicle, coors, forward, side, col);
                }
            }
        }

        CVectorMap::SetZoom(30.0f);

        static unsigned numTimesToZoom = 5;

        for(unsigned index = 0; index < numTimesToZoom; index++)
        {
            CVectorMap::ZoomIn();
        }
    }
}

bool IsOwnershipChangeAllowed()
{
    return m_bAllowOwnershipChange;
}

bool IsTaskDebugStateLoggingEnabled()
{
	return ms_bDebugTaskLogging;
}

bool IsBindPoseCatchingEnabled()
{
	return ms_bCatchCloneBindPose || ms_bCatchOwnerBindPose || ms_bCatchCopBindPose || ms_bCatchNonCopBindPose;
}

bool IsTaskMotionPedDebuggingEnabled()
{
	return ms_bTaskMotionPedDebug;
}

void PerformPedBindPoseCheckDuringUpdate(bool owners, bool clones, bool cops, bool noncops, CTaskTypes::eTaskType const taskType /* = CTaskTypes::TASK_INVALID_ID */)
{
	if(NetworkInterface::IsGameInProgress() && NetworkDebug::IsBindPoseCatchingEnabled())
	{
		gnetAssertf(owners || clones || cops || noncops, "NetworkDebug[namespace]::PerformPedBindPoseCheckDuringUpdate - No peds selected to scan?!");

		fwPool<CPed> *PedPool = CPed::GetPool();
		if(PedPool)
		{
			s32 i=PedPool->GetSize();
			while(i--)
			{
				CPed* pPed = PedPool->GetSlot(i);

				if(pPed && pPed->IsNetworkClone() && !clones)
				{
					continue;
				}

				if(pPed && !pPed->IsNetworkClone() && !owners)
				{
					continue;
				}

				if(pPed && pPed->GetPedModelInfo() && pPed->GetPedModelInfo()->GetIsCop() && !cops)
				{
					continue;
				}

				if(pPed && pPed->GetPedModelInfo() && !pPed->GetPedModelInfo()->GetIsCop() && !noncops)
				{
					continue;
				}

				if((taskType != CTaskTypes::TASK_INVALID_ID) && (pPed && pPed->GetPedIntelligence() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(taskType)))
				{
					continue;
				}

				if(pPed && !pPed->IsLocalPlayer())
				{
					Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					if(pPed->GetPedIntelligence() 
						&& !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DO_NOTHING)	// just created...
						&& !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)	// just created...
						&& !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP)		// streaming anims - currently known about...
						&& !pPed->GetMyVehicle()																						// police response peds
					)	
					{
						Matrix34 rootMatrix;
						if(pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
						{
							Vector3 rootFwd = rootMatrix.b;
							Vector3 pedFwd  = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

							// compare the directions...
							float dot = rootFwd.Dot(pedFwd);
							if(dot < -0.8f)
							{
								grcDebugDraw::Line(pedPos, pedPos + pedFwd, Color_red, Color_red);
								grcDebugDraw::Line(pedPos, pedPos + rootFwd, Color_green, Color_green);

								NetworkDebug::LookAtAndPause(NetworkUtils::GetObjectIDFromGameObject(pPed));
							}
						}
					}
				}
			}
		}
	}
}

bool IsProximityOwnershipAllowed()
{
    return m_bAllowProximityOwnership;
}

bool UsingPlayerCameras()
{
    return m_bUsePlayerCameras;
}

bool ShouldSkipAllSyncs()
{
    return m_bSkipAllSyncs;
}

bool ShouldIgnoreGiveControlEvents()
{
    return m_bIgnoreGiveControlEvents;
}

bool ShouldIgnoreVisibilityChecks()
{
    return m_bIgnoreVisibilityChecks;
}

bool IsPredictionLoggingEnabled()
{
    return ms_EnablePredictionLogging || s_bMinimalPredictionLogging;
}

bool ArePhysicsOptimisationsEnabled()
{
	return m_bEnablePhysicsOptimisations;
}

void ToggleDisplayObjectInfo()
{
    ms_netDebugDisplaySettings.m_displayObjectInfo = !ms_netDebugDisplaySettings.m_displayObjectInfo;
}

void ToggleDisplayObjectVisibilityInfo()
{
	ms_netDebugDisplaySettings.m_displayVisibilityInfo = !ms_netDebugDisplaySettings.m_displayVisibilityInfo;
}

bool*  GetLogExtraDebugNetworkPopulation()
{
	return &s_LogExtraDebugNetworkPopulation;
}

bool  LogExtraDebugNetworkPopulation()
{
	return s_LogExtraDebugNetworkPopulation;
}

void  ToggleLogExtraDebugNetworkPopulation( )
{
	s_LogExtraDebugNetworkPopulation = true;
}

void  UnToggleLogExtraDebugNetworkPopulation( )
{
	s_LogExtraDebugNetworkPopulation = false;
}

NetworkDisplaySettings &GetDebugDisplaySettings()
{
    return ms_netDebugDisplaySettings;
}

// Name         :   GetPlayerStateString
// Purpose      :   Returns the player state as a string
static const char *GetPlayerStateString(CPlayerInfo::State eState)
{
    switch(eState)
    {
    case CPlayerInfo::PLAYERSTATE_PLAYING:
        return "Playing";
    case CPlayerInfo::PLAYERSTATE_HASDIED:
        return "Has Died";
    case CPlayerInfo::PLAYERSTATE_HASBEENARRESTED:
        return "Has Been Arrested";
    case CPlayerInfo::PLAYERSTATE_FAILEDMISSION:
        return "Failed Mission";
    case CPlayerInfo::PLAYERSTATE_LEFTGAME:
        return "Left game";
	case CPlayerInfo::PLAYERSTATE_RESPAWN:
		return "Respawn";
	case CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE:
		return "In MP cutscene";
    default:
        return "Unknown";
    }
}

// debug network object predicates
static bool UnregisteringPedsPredicate(const netObject *networkObject)
{
    if(networkObject && networkObject->GetObjectType() == NET_OBJ_TYPE_PED)
    {
        if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
        {
            return true;
        }
    }

    return false;
}

static bool UnregisteringVehiclesPredicate(const netObject *networkObject)
{
    if(networkObject && IsVehicleObjectType(networkObject->GetObjectType()))
    {
        if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
        {
            return true;
        }
    }

    return false;
}

static bool UnregisteringObjectsPredicate(const netObject *networkObject)
{
    if(networkObject && networkObject->GetObjectType() == NET_OBJ_TYPE_OBJECT)
    {
        if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
        {
            return true;
        }
    }

    return false;
}

static void DisplayNetMemoryUsage()
{
    // sort the categories by memory used
    unsigned sortedCategories[NET_MEM_NUM_MANAGERS];

    for(unsigned index = 0; index < NET_MEM_NUM_MANAGERS; index++)
    {
        sortedCategories[index] = index;
    }

    bool swap = true;

    while(swap)
    {
        swap = false;

        for(unsigned index = 0; index < NET_MEM_NUM_MANAGERS - 1; index++)
        {
            if(g_NetworkHeapUsage[sortedCategories[index]] < g_NetworkHeapUsage[sortedCategories[index + 1]])
            {
                unsigned temp = sortedCategories[index];
                sortedCategories[index] = sortedCategories[index + 1];
                sortedCategories[index + 1] = temp;
                swap = true;
            }
        }
    }

    // display the memory usage
    u32 totalAllocated = 16; // 16 is allocated for alignment reasons when the heap is created

    for(unsigned index = 0; index < NET_MEM_NUM_MANAGERS; index++)
    {
        unsigned sortedIndex = sortedCategories[index];

        if(g_NetworkHeapUsage[sortedIndex] > 0)
        {
            grcDebugDraw::AddDebugOutput("%s : %d", GetMemCategoryName(static_cast<NetworkMemoryCategory>(sortedIndex)), g_NetworkHeapUsage[sortedIndex]);
            totalAllocated += g_NetworkHeapUsage[sortedIndex];
        }
    }

    grcDebugDraw::AddDebugOutput("Total : %d", totalAllocated);
}

static unsigned           g_NumVeryHighUpdates = 0;
static unsigned           g_NumHighUpdates     = 0;
static unsigned           g_NumMediumUpdates   = 0;
static unsigned           g_NumLowUpdates      = 0;
static unsigned           g_NumVeryLowUpdates  = 0;

static NetworkObjectTypes gObjectType          = NUM_NET_OBJ_TYPES;
static bool               g_CountVehicles      = false;

static bool CPU_UsagePredictate(const netObject *object)
{
    if(object && !object->IsClone())
    {
        bool includeObject = false;

        if(g_CountVehicles)
        {
            if(IsVehicleObjectType(object->GetObjectType()))
            {
                includeObject = true;
            }
        }
        else if(object->GetObjectType() == gObjectType)
        {
            includeObject = true;
        }

        if(includeObject)
        {
            switch(object->GetMinimumUpdateLevel())
            {
            case CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH:
                g_NumVeryHighUpdates++;
                break;
            case CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH:
                g_NumHighUpdates++;
                break;
            case CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM:
                g_NumMediumUpdates++;
                break;
            case CNetworkSyncDataULBase::UPDATE_LEVEL_LOW:
                g_NumLowUpdates++;
                break;
            case CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW:
                g_NumVeryLowUpdates++;
                break;
            }
        }

        return includeObject;
    }

    return false;
}

static void DisplaySyncCloneInfo(netSyncTree *syncTree, const char *name, bool displayReads, bool displayWrites)
{
    if(syncTree)
    {
        if(displayReads)
        {
            unsigned averageCloneReadsPerUpdate = static_cast<unsigned>(syncTree->GetAverageReadsPerUpdate());

            if(averageCloneReadsPerUpdate > 0)
            {
                grcDebugDraw::AddDebugOutput("%s\tNum Reads Per Update: %d", name, averageCloneReadsPerUpdate);
            }
        }

        if(displayWrites)
        {
            unsigned averageCloneWritesPerUpdate = static_cast<unsigned>(syncTree->GetAverageWritesPerUpdate());

            if(averageCloneWritesPerUpdate > 0)
            {
                grcDebugDraw::AddDebugOutput("%s\tNum Writes Per Update: %d", name, averageCloneWritesPerUpdate);
            }
        }
    }
}

static void DisplayPlayerLocationInfo()
{
    unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

        CPed *playerPed = remotePlayer->GetPlayerPed();

        if(playerPed)
        {
            if(remotePlayer->IsInDifferentTutorialSession())
            {
                grcDebugDraw::AddDebugOutput("%s - Different Tutorial Session", remotePlayer->GetLogName());
            }
            else
            {
		        float distance = Dist(playerPed->GetTransform().GetPosition(), FindFollowPed()->GetTransform().GetPosition()).Getf();

                const char *taskName = "No Task";

                CTask *task = playerPed->GetPedIntelligence()->GetTaskActive();

                if(task)
                {
                    taskName = task->GetTaskName();
                }

                grcDebugDraw::AddDebugOutput("%s - Distance %.2f %s%s", remotePlayer->GetLogName(), distance, playerPed->GetIsInVehicle() ? "In Vehicle " : "", taskName);
            }
        }
    }
}

static void DisplayNetCPUUsage()
{
    if(s_DisplayBasicCPUInfo)
    {
        // display how many objects we own and their update level
        g_NumVeryHighUpdates = 0;
        g_NumHighUpdates     = 0;
        g_NumMediumUpdates   = 0;
        g_NumLowUpdates      = 0;
        g_NumVeryLowUpdates  = 0;
        g_CountVehicles      = true;
        unsigned numVehicles = CNetwork::GetObjectManager().GetNumLocalObjects(CPU_UsagePredictate);

        grcDebugDraw::AddDebugOutput("Local Vehicles : %d (VH:%d, H:%d, M:%d, L:%d, VL:%d)", numVehicles, g_NumVeryHighUpdates, g_NumHighUpdates, g_NumMediumUpdates, g_NumLowUpdates, g_NumVeryLowUpdates);

        g_NumVeryHighUpdates = 0;
        g_NumHighUpdates     = 0;
        g_NumMediumUpdates   = 0;
        g_NumLowUpdates      = 0;
        g_NumVeryLowUpdates  = 0;
        g_CountVehicles      = false;
        gObjectType          = NET_OBJ_TYPE_PED;
        unsigned numPeds     = CNetwork::GetObjectManager().GetNumLocalObjects(CPU_UsagePredictate);

        grcDebugDraw::AddDebugOutput("Local Peds : %d (VH:%d, H:%d, M:%d, L:%d, VL:%d)", numPeds, g_NumVeryHighUpdates, g_NumHighUpdates, g_NumMediumUpdates, g_NumLowUpdates, g_NumVeryLowUpdates);

        // display the peak number of objects removed/deleted in a single frame in the last second
        grcDebugDraw::AddDebugOutput("Peak Clone Creates Peds:%d Vehicles:%d Other:%d - Peak time: %.2fms", g_PeakPedCreates.GetPeakCount(), g_PeakVehicleCreates.GetPeakCount(), g_PeakOtherCreates.GetPeakCount(), g_PeakCloneCreateDuration.GetPeak() * 1000.0f);
        grcDebugDraw::AddDebugOutput("Peak Clone Removes Peds:%d Vehicles:%d Other:%d - Peak time: %.2fms", g_PeakPedRemoves.GetPeakCount(), g_PeakVehicleRemoves.GetPeakCount(), g_PeakOtherRemoves.GetPeakCount(), g_PeakCloneRemoveDuration.GetPeak() * 1000.0f);

        // display the average number of clone syncs sent per update
        grcDebugDraw::AddDebugOutput("Average number of clone syncs sent: %.2f%s", m_AverageSyncCloneCallsPerUpdate.GetAverage(), NetworkInterface::GetObjectManager().IsUsingBatchedUpdates() ? " B" : "");
    }

    // Display the sync tree balancing information (just cars and peds initially)
    netSyncTree *autoSyncTree = CNetObjAutomobile::GetStaticSyncTree();
    netSyncTree *pedSyncTree  = CNetObjPed::GetStaticSyncTree();

    if(autoSyncTree && pedSyncTree)
    {
        // Display duration and num update calls
        unsigned numVehiclesUpdated = autoSyncTree->GetNumTimesUpdateCalled();
        unsigned numPedsUpdated     = pedSyncTree->GetNumTimesUpdateCalled();
        grcDebugDraw::AddDebugOutput("Sync Tree Update Time:%d Num Updates: Vehicles:%d Peds:%d", NetworkInterface::GetObjectManager().GetLastSyncTreeUpdateDuration(), numVehiclesUpdated, numPedsUpdated);

        if(s_DisplaySyncTreeBatchingInfo)
        {
            grcDebugDraw::AddDebugOutput("");
            grcDebugDraw::AddDebugOutput("Automobiles\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tPeds");

            for(unsigned updateLevel = 0; updateLevel < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS; updateLevel++)
            {
                u8 numAllowedAutoBatches = autoSyncTree->GetNumAllowedBatches(updateLevel);
                u8 numAllowedPedBatches  = pedSyncTree->GetNumAllowedBatches(updateLevel);

                grcDebugDraw::AddDebugOutput("\tUpdate Level: %-11s Num Batches: %d\t\t\t\t\tUpdate Level: %-11s Num Batches: %d", GetUpdateLevelString(updateLevel), numAllowedAutoBatches, GetUpdateLevelString(updateLevel), numAllowedPedBatches);

                u8 maxBatches = MAX(numAllowedAutoBatches, numAllowedPedBatches);

                for(unsigned batch = 0; batch < maxBatches; batch+=2)
                {
                    char autoBatchInfo[128];

                    if(batch+1 < numAllowedAutoBatches)
                    {
                        formatf(autoBatchInfo, 128, "Batch:%d Num Objects: %d Batch:%d Num Objects: %d", batch, autoSyncTree->GetObjectCount(updateLevel, batch), batch+1, autoSyncTree->GetObjectCount(updateLevel, batch+1));
                    }
                    else if(batch < numAllowedAutoBatches)
                    {
                        formatf(autoBatchInfo, 128, "Batch:%d Num Objects: %d\t\t\t\t\t\t\t\t\t\t ", batch, autoSyncTree->GetObjectCount(updateLevel, batch));
                    }

                    char pedBatchInfo[128];

                    if(batch+1 < numAllowedPedBatches)
                    {
                        formatf(pedBatchInfo, 128, "Batch:%d Num Objects: %d Batch:%d Num Objects: %d", batch, pedSyncTree->GetObjectCount(updateLevel, batch), batch+1, pedSyncTree->GetObjectCount(updateLevel, batch+1));
                    }
                    else if(batch < numAllowedPedBatches)
                    {
                        formatf(pedBatchInfo, 128, "Batch:%d Num Objects: %d", batch, pedSyncTree->GetObjectCount(updateLevel, batch));
                    }

                    if(batch < numAllowedAutoBatches && batch < numAllowedPedBatches)
                    {
                        grcDebugDraw::AddDebugOutput("\t\t%s\t\t\t%s", autoBatchInfo, pedBatchInfo);
                    }
                    else if(batch < numAllowedAutoBatches)
                    {
                        grcDebugDraw::AddDebugOutput("\t\t%s", autoBatchInfo);
                    }
                    else if(batch < numAllowedPedBatches)
                    {
                        grcDebugDraw::AddDebugOutput("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t%s", pedBatchInfo);
                    }
                }
            }
        }
    }

    // Display information about the number of objects being synced per frame
    if(s_DisplaySyncTreeReads || s_DisplaySyncTreeWrites)
    {
        DisplaySyncCloneInfo(CNetObjAutomobile::GetStaticSyncTree(),      "Automobiles",        s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjBike::GetStaticSyncTree(),            "Bikes",              s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
	    DisplaySyncCloneInfo(CNetObjBoat::GetStaticSyncTree(),            "Boats",              s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
	    DisplaySyncCloneInfo(CNetObjSubmarine::GetStaticSyncTree(),       "Submarines",         s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjHeli::GetStaticSyncTree(),            "Helis",              s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjObject::GetStaticSyncTree(),          "Objects",            s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjPed::GetStaticSyncTree(),             "Peds",               s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
#if GLASS_PANE_SYNCING_ACTIVE
	    DisplaySyncCloneInfo(CNetObjGlassPane::GetStaticSyncTree(),       "Glass panes",        s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
#endif /* GLASS_PANE_SYNCING_ACTIVE */
        DisplaySyncCloneInfo(CNetObjPickup::GetStaticSyncTree(),          "Pickups",            s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjPickupPlacement::GetStaticSyncTree(), "Pickup Placements",  s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
        DisplaySyncCloneInfo(CNetObjPlayer::GetStaticSyncTree(),          "Players",            s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
	    DisplaySyncCloneInfo(CNetObjTrain::GetStaticSyncTree(),           "Trains",             s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
	    DisplaySyncCloneInfo(CNetObjPlane::GetStaticSyncTree(),           "Planes",             s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
	    DisplaySyncCloneInfo(CNetObjDoor::GetStaticSyncTree(),            "Doors",              s_DisplaySyncTreeReads, s_DisplaySyncTreeWrites);
    }

    // Display information about other players in the session for performance reasons
    if(s_DisplayPlayerLocationInfo)
    {
        DisplayPlayerLocationInfo();
    }

    // Display performance stats relating to the bandwidth optimisation for peds walking on the navmesh
    if(s_DisplayPedNavmeshSyncStats)
    {
        unsigned numRescans      = 0;
        unsigned numGroundProbes = 0;
        GetPedNavmeshPosSyncStats(numRescans, numGroundProbes);

        grcDebugDraw::AddDebugOutput("Num Ped Navmesh Sync Rescans:       %d", numRescans);
        grcDebugDraw::AddDebugOutput("Num Ped Navmesh Sync Ground Probes: %d", numGroundProbes);
    }

    // Displays performance stats relating to the number of calls to expensive functions
    if(s_DisplayExpensiveFunctionCallStats)
    {
        grcDebugDraw::AddDebugOutput("Average number of GetNetworkObject() calls: %.2f", NetworkInterface::GetObjectManager().GetAverageNumGetNetworkObjectCalls());
        grcDebugDraw::AddDebugOutput("Average number of ManageUpdateLevel() calls: %.2f", m_AverageManageUpdateLevelCallsPerUpdate.GetAverage());
    }
}

void PrintPhysicsInfo()
{
	s32 instCount=0;
	s32 instActiveCount=0;

	s32 vehCount=0;
	s32 vehActiveCount=0;
	s32 vehLocalRealCount=0;
	s32 vehLocalDummyCount=0;
	s32 vehRemoteRealCount=0;
	s32 vehRemoteDummyCount=0;

	s32 pedCount=0;
	s32 pedActiveCount=0;
	s32 pedLocalRealCount=0;
	s32 pedLocalDummyCount=0;
	s32 pedRemoteRealCount=0;
	s32 pedRemoteDummyCount=0;

	s32 objCount=0;
	s32 objActiveCount=0;
	s32 objActiveLocalCount=0;

	for(int i = 0; i < CPhysics::GetLevel()->GetMaxObjects(); ++i)
	{
		if(!CPhysics::GetLevel()->IsNonexistent(i))
		{
			phInst* pInst = CPhysics::GetLevel()->GetInstance(i);
			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if(pEntity && pInst->GetClassType() != PH_INST_MAPCOL)
			{
				bool bActive = false;

				if(CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()) && pEntity->GetCollider() && !pEntity->GetCollider()->GetSleep()->IsAsleep())
				{
					instActiveCount++;
					bActive = true;
				}

				if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

					vehCount++;

					if (pVehicle->IsDummy())
					{
					    if (!pVehicle->IsNetworkClone())
						{
						    vehLocalDummyCount++;
					    }
					    else
					    {
    						vehRemoteDummyCount++;
					    }
					}

					if (bActive)
					{
						vehActiveCount++;

						if (!pVehicle->IsNetworkClone())
						{
							vehLocalRealCount++;
						}
						else
						{
						    vehRemoteRealCount++;
					    }
					}
				}

				if (pEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pEntity);

					pedCount++;

					if (pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
					{
						if (!pPed->IsNetworkClone())
						{
							pedLocalDummyCount++;
						}
						else
						{
							pedRemoteDummyCount++;
						}
					}

					if (bActive)
					{
						pedActiveCount++;

						if (!pPed->IsNetworkClone())
						{
							pedLocalRealCount++;
						}
						else
						{
							pedRemoteRealCount++;
						}
					}
				}

				if (pEntity->GetIsTypeObject())
				{
					objCount++;

					if (bActive)
					{
						objActiveCount++;

						if (!static_cast<CPhysical*>(pEntity)->IsNetworkClone())
						{
							objActiveLocalCount++;
						}
					}
				}

				instCount++;
			}
		}
	}

	grcDebugDraw::AddDebugOutput("%d physics instances active (%d entities)", instActiveCount, vehActiveCount+pedActiveCount+objActiveCount);
	grcDebugDraw::AddDebugOutput("%d real vehicles active (local %d, clone %d)", vehLocalRealCount+vehRemoteRealCount, vehLocalRealCount, vehRemoteRealCount);
	grcDebugDraw::AddDebugOutput("%d dummy vehicles active (local %d, clone %d)", vehLocalDummyCount+vehRemoteDummyCount, vehLocalDummyCount, vehRemoteDummyCount);
	grcDebugDraw::AddDebugOutput("%d full physics peds active (local %d, clone %d)", pedLocalRealCount+pedRemoteRealCount, pedLocalRealCount, pedRemoteRealCount);
	grcDebugDraw::AddDebugOutput("%d low lod physics peds active (local %d, clone %d)", pedLocalDummyCount+pedRemoteDummyCount, pedLocalDummyCount, pedRemoteDummyCount);
	grcDebugDraw::AddDebugOutput("%d/%d objects active (local %d, clone %d)", objActiveCount, objCount, objActiveLocalCount, objActiveCount-objActiveLocalCount);
}

void DisplayLoggingStatistics()
{
#if __BANK
    netLoggingInterface *logFiles[] = 
    {
        &CNetwork::GetArrayManager().GetLog(),
        &CNetwork::GetEventManager().GetLog(),
        &CNetwork::GetObjectManager().GetLog(),
        &NetworkInterface::GetPlayerMgr().GetLog(),
        &CNetwork::GetMessageLog(),
        s_PredictionLog
    };

    unsigned numLogFiles = sizeof(logFiles) / sizeof(netLoggingInterface *);

    for(unsigned index = 0; index < numLogFiles; index++)
    {
        const unsigned DEBUG_TEXT_LENGTH = 256;
        char debugText[DEBUG_TEXT_LENGTH];

        logFiles[index]->GetDebugStatisticsText(debugText, DEBUG_TEXT_LENGTH);

        grcDebugDraw::AddDebugOutput(debugText);
    }

    float completionPercent = 0.0f;
    const char *logFileSpooling = netLog::GetLogFileBeingSpooled(completionPercent);

    if(logFileSpooling)
    {
        grcDebugDraw::AddDebugOutput("Currently spooling %s (%.2f%%)", logFileSpooling, completionPercent);
    }
#endif // __BANK
}

int ShowTemporaryRealVehicles()
{
	int counter = 0;
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32)VehiclePool->GetSize();
	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);
		if(pVehicle)
		{
			if (NetworkInterface::IsSuperDummyStillLaunchedIntoAir(pVehicle))
			{
				counter++;
			}
		}
	}

	return counter;
}

// Name         :   PrintLocalUserId
// Purpose      :   Prints the local gamer id
void PrintLocalUserId()
{
	gnetAssertf(NetworkInterface::IsLocalPlayerOnline(), "Local player is not online");

	CNetGamePlayer* player = NetworkInterface::GetLocalPlayer();
	if (player)
	{
		char name[RL_MAX_GAMER_HANDLE_CHARS];
		player->GetGamerInfo().GetGamerHandle().ToString(name, RL_MAX_GAMER_HANDLE_CHARS);
		gnetError("Local Gamer: \"%s\"", name);
	}
}

// Name         :   NetworkBank_ShowAverageRTT
// Purpose      :   Prints average RTT to screen
void NetworkBank_ShowAverageRTT()
{
	s_DisplayAverageRTT = !s_DisplayAverageRTT;
}

// Name         :   NetworkBank_ExportImportGamerHandle
// Purpose      :   Exports and imports a gamer handle
void  NetworkBank_ExportImportGamerHandle()
{
    rlGamerHandle hLocal = NetworkInterface::GetLocalGamerHandle();

    char aBuf[GAMER_HANDLE_SIZE];
    hLocal.Export(aBuf, GAMER_HANDLE_SIZE);
    hLocal.Import(aBuf, GAMER_HANDLE_SIZE);
}

// Name         :   PrintNetworkInfo
// Purpose      :   Prints some network info on the screen
void PrintNetworkInfo()
{
	if (!NetworkInterface::IsGameInProgress())
	{
		CNetwork::GetBandwidthManager().DisplayDebugInfo();
		return;
	}

    char str[100];
    int lineNo = 0;
    str[0]='\0';

    if (ms_netDebugDisplaySettings.m_displayNetInfo)
    {
        // we display network information further down on the screen than the other debug output
        int debugTextX = 5;

        lineNo = 9;

        if (NetworkInterface::IsHost())
            sprintf(str, "I am host");
        else
            sprintf(str, "I am client");

        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);

        sprintf(str, "Build: %s (%s)", __DATE__, __TIME__);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);

        int totalNumObjects = NetworkInterface::GetObjectManager().GetTotalNumObjects();

        int numLocalPeds         = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED);
        int numRemotePeds        = CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_PED);
        int numUnregisteringPeds = NetworkInterface::GetObjectManager().GetNumLocalObjects(UnregisteringPedsPredicate);

        int numLocalVehicles         = CNetworkObjectPopulationMgr::GetNumLocalVehicles();
        int numRemoteVehicles        = CNetworkObjectPopulationMgr::GetNumRemoteVehicles();
        int numUnregisteringVehicles = NetworkInterface::GetObjectManager().GetNumLocalObjects(UnregisteringVehiclesPredicate);

        int numLocalObjects         = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_OBJECT);
        int numRemoteObjects        = CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_OBJECT);
        int numUnregisteringObjects = NetworkInterface::GetObjectManager().GetNumLocalObjects(UnregisteringObjectsPredicate);

		netIpAddress ip;
		netHardware::GetPublicIpAddress(&ip);
		char ipPubStr[netIpAddress::MAX_STRING_BUF_SIZE] = {};
		ip.Format(ipPubStr);

        sprintf(str, "My IP = %s", ipPubStr);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Num connections = %d", CNetwork::GetConnectionManager().GetActiveConnectionCount());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Network time = %d", NetworkInterface::GetNetworkTime());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Network time pausable = %d", CNetwork::GetNetworkTimePausable());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Hours = %d, minutes = %d%s", CClock::GetHour(), CClock::GetMinute(), CNetwork::IsClockTimeOverridden() ? " (Overridden)" : "");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Old weather = %u, new weather = %u", g_weather.GetPrevTypeIndex(), g_weather.GetNextTypeIndex());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "OBJECTS");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "=======");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Total num objects = %d", totalNumObjects);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Remote clones = %d", numRemotePeds + numRemoteVehicles + numRemoteObjects);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Local objects");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - peds : %d (unregistering : %d)", numLocalPeds, numUnregisteringPeds);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - vehicles : %d (unregistering : %d)", numLocalVehicles, numUnregisteringVehicles);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - objects : %d (unregistering : %d)", numLocalObjects, numUnregisteringObjects);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Remote objects");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - peds : %d", numRemotePeds);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - vehicles : %d", numRemoteVehicles);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "   - objects : %d", numRemoteObjects);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
#if __DEV
        sprintf(str, "EVENTS");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "======");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Num queued events = %d", NetworkInterface::GetEventManager().GetSizeOfEventList());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "Num used event ids = %d", NetworkInterface::GetEventManager().GetNumUsedEventIds());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
#endif
        sprintf(str, "NET STATS");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
        sprintf(str, "=========");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);

		const float inKbps = (CNetwork::GetBandwidthManager().GetSocketRecorder().GetInboundBandwidth() << 3) / 1024.0f;
        const float outKbps = (CNetwork::GetBandwidthManager().GetSocketRecorder().GetOutboundBandwidth() << 3) / 1024.0f;

        grcDebugDraw::PrintToScreenCoors("Bandwidth:", debugTextX, lineNo++);

        sprintf(str, "Kbits per sec : %f (in: %f, out:%f)", inKbps+outKbps, inKbps, outKbps);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);

        const unsigned inPps = CNetwork::GetBandwidthManager().GetSocketRecorder().GetInboundPacketFreq();
        const unsigned outPps = CNetwork::GetBandwidthManager().GetSocketRecorder().GetOutboundPacketFreq();

        sprintf(str, "Packets per sec : %u (in : %u, out : %u)", inPps+outPps, inPps, outPps);
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
#if __DEV
        sprintf(str, "Average num bandwidth send failures per update : %d (current %d)", CNetwork::GetBandwidthManager().GetAverageFailuresPerUpdate(), CNetwork::GetBandwidthManager().GetNumFailuresPerUpdate());
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
#endif
    }
    else if (ms_netDebugDisplaySettings.m_displayBasicInfo)
    {
        // if we are not displaying full network output, just write some basic information after the standard debug text

        grcDebugDraw::AddDebugOutput("");
        if (NetworkInterface::IsHost())
        {
            grcDebugDraw::AddDebugOutput("I (%s) am host",
                                    NetworkInterface::GetLocalPlayer()->GetLogName());
        }
        else
        {
            const CNetGamePlayer *hostPlayer = 0;

            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                if(player && player->IsHost())
                {
                    hostPlayer = player;
                }
            }

			grcDebugDraw::AddDebugOutput("I (%s) am client, (%s) is host",
            NetworkInterface::GetLocalPlayer()->GetLogName(), hostPlayer ? hostPlayer->GetLogName() : "No-one");
        }

        const float inKbps = (CNetwork::GetBandwidthManager().GetSocketRecorder().GetInboundBandwidth() << 3) / 1024.0f;
        const float outKbps = (CNetwork::GetBandwidthManager().GetSocketRecorder().GetOutboundBandwidth() << 3) / 1024.0f;

		if (s_DisplayAverageRTT)
		{
			grcDebugDraw::AddDebugOutput("Average Client RTT: %d", CNetwork::GetClientAverageRTT());
		}

        grcDebugDraw::AddDebugOutput("Bandwidth : In %.4f : Out %.4f", inKbps, outKbps);

        grcDebugDraw::AddDebugOutput("Build: %s (%s)", __DATE__, __TIME__);

        //SCxnMgr heap
        size_t heapUsage = CNetwork::GetSCxnMgrAllocator().GetHeapSize() - CNetwork::GetSCxnMgrAllocator().GetMemoryAvailable();
        size_t maxHeapUsage = CNetwork::GetSCxnMgrAllocator().GetHeapSize() - ms_SCxnMgrHeapTideMark;
        grcDebugDraw::AddDebugOutput("SCxnMsg heap: %d / %d (%.2f%%). Max used: %d (%.2f%%)",
                                heapUsage,
                                CNetwork::GetSCxnMgrAllocator().GetHeapSize(),
                                (100.0f * heapUsage / CNetwork::GetSCxnMgrAllocator().GetHeapSize()),
                                maxHeapUsage,
                                (100.0f * maxHeapUsage / CNetwork::GetSCxnMgrAllocator().GetHeapSize()));

        // network heap
        heapUsage = CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable();
        grcDebugDraw::AddDebugOutput("Network heap: %d / %d (%.2f%%)",
                                heapUsage,
                                CNetwork::GetNetworkHeap()->GetHeapSize(),
                                ((100.0f * heapUsage) / CNetwork::GetNetworkHeap()->GetHeapSize()));

		grcDebugDraw::AddDebugOutput("Network time: %d", NetworkInterface::GetSyncedTimeInMilliseconds());
		grcDebugDraw::AddDebugOutput("Game time: %02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());

		if (CVehiclePopulation::ms_NetworkShowTemporaryRealVehicles)
		{			
			int temporaryRealVehicles = ShowTemporaryRealVehicles();
			grcDebugDraw::AddDebugOutput("Temporary Real Vehicles Count: %d", temporaryRealVehicles);
		}
    }
	
	if(ms_netDebugDisplaySettings.m_displayPlayerInfoDebug)
    {
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

            if(player->GetPlayerPed())
            {
				u32 timeSinceLastChange = fwTimer::GetTimeInMilliseconds_NonScaledClipped() - player->GetPlayerPed()->GetPlayerInfo()->GetTimeOfPlayerStateChange();
				const rlClanDesc& clanDesc = player->GetClanDesc();
				grcDebugDraw::AddDebugOutput("%s : %s (%d) %s", player->GetGamerInfo().GetName(), 
																GetPlayerStateString(player->GetPlayerPed()->GetPlayerInfo()->GetPlayerState()), 
																timeSinceLastChange, 
																clanDesc.IsValid() ? clanDesc.m_ClanTag : "");
            }
		}
    }

    if(ms_netDebugDisplaySettings.m_displayNetMemoryUsage)
    {
        DisplayNetMemoryUsage();
    }

    if(s_DisplayBasicCPUInfo || s_DisplaySyncTreeBatchingInfo || s_DisplaySyncTreeReads || s_DisplaySyncTreeWrites || s_DisplayPlayerLocationInfo || s_DisplayPedNavmeshSyncStats || s_DisplayExpensiveFunctionCallStats)
    {
        DisplayNetCPUUsage();
    }

    if(s_DisplayLatencyStats)
    {
        grcDebugDraw::AddDebugOutput("");
        grcDebugDraw::AddDebugOutput("Average in latency: %.2f", m_AverageInboundLatency.GetAverage());
        grcDebugDraw::AddDebugOutput("Average out latency (clone_sync):   %.2f", m_AverageOutboundCloneSyncLatency.GetAverage());
        grcDebugDraw::AddDebugOutput("Average out latency (connect sync): %.2f", m_AverageOutboundConnectionSyncLatency.GetAverage());
    }

    if(ms_netDebugDisplaySettings.m_displayPredictionInfo)
    {
        grcDebugDraw::AddDebugOutput("Num prediction pops / s : %d", NetworkDebug::GetNumPredictionPopsPerSecond());
        grcDebugDraw::AddDebugOutput("Num prediction BB checks / s : %d", NetworkDebug::GetNumPredictionBBChecksPerSecond());
        grcDebugDraw::AddDebugOutput("Num prediction collision disabled : %d", NetworkDebug::GetNumPredictionCollisionDisabled());
        grcDebugDraw::AddDebugOutput("Max time with collision disabled : %d", NetworkDebug::GetMaxTimeWithNoCollision());
        grcDebugDraw::AddDebugOutput("Max BB checks before warp : %d", NetworkDebug::GetMaxBBChecksBeforeWarp());
        grcDebugDraw::AddDebugOutput("Num moving vehicles fixed by network : %d", NetworkDebug::GetNumMovingVehiclesFixedByNetwork());
        grcDebugDraw::AddDebugOutput("Total prediction pops : %d", gTotalPredictionPops);
    }
    else
    {
        gTotalPredictionPops = 0;
    }

	if(ms_netDebugDisplaySettings.m_displayHostBroadcastDataAllocations)
	{
		CNetwork::GetArrayManager().DisplayHostBroadcastDataAllocations();
	}
	if(ms_netDebugDisplaySettings.m_displayPlayerBroadcastDataAllocations)
	{
		CNetwork::GetArrayManager().DisplayPlayerBroadcastDataAllocations();
	}

	if(ms_netDebugDisplaySettings.m_displayPortablePickupInfo)
	{
		CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();

		if (pPlayerInfo)
		{
			grcDebugDraw::AddDebugOutput("Portable pickup pending : %s", pPlayerInfo->PortablePickupPending ? "true" : "false");

			if (pPlayerInfo->m_maxPortablePickupsCarried != -1)
			{
				grcDebugDraw::AddDebugOutput("Max pickups carried : %d/%d", pPlayerInfo->m_numPortablePickupsCarried, pPlayerInfo->m_maxPortablePickupsCarried);
			}

			for (u32 i=0; i<CPlayerInfo::MAX_PORTABLE_PICKUP_INFOS; i++)
			{
				CPlayerInfo::sPortablePickupInfo* info = &pPlayerInfo->PortablePickupsInfo[i];

				if (info->modelIndex != 0)
				{
					grcDebugDraw::AddDebugOutput("Portable pickups of type %d carried : %d/%d", info->modelIndex, info->numCarried, info->maxPermitted);
				}
			}
		}
	}

	if(ms_netDebugDisplaySettings.m_displayDispatchInfo)
	{
		NetworkInterface::GetArrayManager().GetIncidentsArrayHandler()->DisplayDebugInfo();
	}

	if(ms_netDebugDisplaySettings.m_displayObjectPopulationReservationInfo)
	{
		u32 reservedPeds;
		u32 reservedVehicles;
		u32 reservedObjects;
		u32 createdPeds;
		u32 createdVehicles;
		u32 createdObjects;
		u32 unusedPeds;
		u32 unusedVehicles;
		u32 unusedObjects;

		CTheScripts::GetScriptHandlerMgr().GetNumReservedScriptEntities(reservedPeds, reservedVehicles, reservedObjects);
		CTheScripts::GetScriptHandlerMgr().GetNumCreatedScriptEntities(createdPeds, createdVehicles, createdObjects);

		unusedPeds = (reservedPeds > createdPeds) ? reservedPeds - createdPeds : 0;
		unusedVehicles = (reservedVehicles > createdVehicles) ? reservedVehicles - createdVehicles : 0;
		unusedObjects = (reservedObjects > createdObjects) ? reservedObjects - createdObjects : 0;

		grcDebugDraw::AddDebugOutput("Local Script Reservations: %d (%d/%d) peds, %d (%d/%d) vehicles, %d (%d/%d) objects", unusedPeds, createdPeds, reservedPeds, unusedVehicles, createdVehicles, reservedVehicles, unusedObjects, createdObjects, reservedObjects);
			
		CTheScripts::GetScriptHandlerMgr().GetRemoteScriptEntityReservations(reservedPeds, reservedVehicles, reservedObjects, createdPeds, createdVehicles, createdObjects);
			
		unusedPeds = (reservedPeds > createdPeds) ? reservedPeds - createdPeds : 0;
		unusedVehicles = (reservedVehicles > createdVehicles) ? reservedVehicles - createdVehicles : 0;
		unusedObjects = (reservedObjects > createdObjects) ? reservedObjects - createdObjects : 0;

		grcDebugDraw::AddDebugOutput("Remote Script Reservations: %d (%d/%d) peds, %d (%d/%d) vehicles, %d (%d/%d) objects", unusedPeds, createdPeds, reservedPeds, unusedVehicles, createdVehicles, reservedVehicles, unusedObjects, createdObjects, reservedObjects);

		CDispatchManager::GetInstance().GetNetworkReservations(reservedPeds, reservedVehicles, reservedObjects, createdPeds, createdVehicles, createdObjects);

		unusedPeds = (reservedPeds > createdPeds) ? reservedPeds - createdPeds : 0;
		unusedVehicles = (reservedVehicles > createdVehicles) ? reservedVehicles - createdVehicles : 0;
		unusedObjects = (reservedObjects > createdObjects) ? reservedObjects - createdObjects : 0;

		grcDebugDraw::AddDebugOutput("Dispatch Reservations: %d (%d/%d) peds, %d (%d/%d) vehicles, %d (%d/%d) objects", unusedPeds, createdPeds, reservedPeds, unusedVehicles, createdVehicles, reservedVehicles, unusedObjects, createdObjects, reservedObjects);

		CTheScripts::GetScriptHandlerMgr().DisplayReservationInfo();
	}

    if(ms_netDebugDisplaySettings.m_displayConnectionMetrics)
    {
        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(player)
            {
                PhysicalPlayerIndex playerIndex = player->GetPhysicalPlayerIndex();
                grcDebugDraw::AddDebugOutput("%s: Using Relay: %s, Base Resend Freq: %d, Latency: %.2f, Ping: %.2f, Packet Loss: %.2f", player->GetGamerInfo().GetName(), NetworkInterface::IsConnectedViaRelay(playerIndex) ? "Yes" : "No", CProjectBaseSyncDataNode::GetBaseResendFrequency(playerIndex), NetworkInterface::GetAverageSyncLatency(playerIndex), NetworkInterface::GetAverageSyncRTT(playerIndex), NetworkInterface::GetAveragePacketLoss(playerIndex));
            }
        }
    }

	CNetwork::GetBandwidthManager().DisplayDebugInfo();

#if __DEV
	CNetworkKillTracker::DisplayDebugInfo();
	CNetworkHeadShotTracker::DisplayDebugInfo();
#endif // __DEV

    CNetwork::GetObjectManager().DisplayDebugInfo();

	if (ms_netDebugDisplaySettings.m_displayPlayerScripts)
	{
		DisplayNetworkScripts();
	}

#if ENABLE_NETWORK_BOTS
    CNetworkBot::DisplayDebugText();
#endif // ENABLE_NETWORK_BOTS

	CNetworkObjectPopulationMgr::DisplayDebugText();

	CNetwork::GetRoamingBubbleMgr().DisplayDebugText();

	NetworkScriptWorldStateTypes::DisplayDebugText();

	CNetworkSynchronisedScenes::DisplayDebugText();

	if(s_DisplayLoggingStatistics)
	{
		DisplayLoggingStatistics();
	}	

	// display some status bars to indicate bandwidth/memory usage
	if (ms_bRenderNetworkStatusBars)
	{
		const unsigned int inKbps    = CNetwork::GetBandwidthManager().GetSocketRecorder().GetInboundBandwidth()  << 3;
		const unsigned int outKbps   = CNetwork::GetBandwidthManager().GetSocketRecorder().GetOutboundBandwidth() << 3;
		const unsigned int totalKbps = inKbps + outKbps;
		const unsigned int heapUsage = static_cast<unsigned int>(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

		DLC_Add( RenderNetworkStatusBars, inKbps, outKbps, totalKbps, heapUsage, (u32)CNetwork::GetNetworkHeap()->GetHeapSize());
	}

	if (s_bDisplayPauseText && fwTimer::IsGamePaused())
	{
		DLC_Add(RenderDebugMessage);
	}
	else if (s_bDisplayDebugText && s_debugDisplayTimer > 0)
	{
		DLC_Add(RenderDebugMessage);
		s_debugDisplayTimer -= fwTimer::GetTimeStepInMilliseconds(); 
	}
	else
	{
		s_bDisplayPauseText = false;
		s_bDisplayDebugText = false;
		s_debugDisplayTimer = 0;
	}

	if(ms_netDebugDisplaySettings.m_displayChallengeFlyableAreas)
	{
		CStatsMgr::drawNonFlyableAreas();
	}

	if (s_bDisplayHttpReqTrackerText && s_httpReqTrackerTextDisplayTimer > 0)
	{
		DLC_Add(RenderHttpReqTrackerMessage);
		s_httpReqTrackerTextDisplayTimer -= fwTimer::GetTimeStepInMilliseconds(); 
	}
	else 
	{
		s_bDisplayHttpReqTrackerText = false;
		s_httpReqTrackerTextDisplayTimer = 0;
	}

	if(ShouldRenderConfigurationDetails())
	{
		DLC_Add(RenderConfigurationDetails);
	}
}

void DisplayNetworkScripts()
{
#if __DEV
	CTheScripts::GetScriptHandlerMgr().DisplayScriptHandlerInfo();
#endif
}

void RenderDebug()
{
    for(unsigned index = 0; index < gNumOrientationBlendFailures; index++)
    {
        OrientationBlendFailureData &blendFailData = gOrientationBlendFailures[index];

        if(blendFailData.m_Bound1)
        {
            grcColor(Color_red);
            blendFailData.m_Bound1->Draw(blendFailData.m_Matrix1);
        }

        if(blendFailData.m_Bound2)
        {
            grcColor(Color_purple);
            blendFailData.m_Bound2->Draw(blendFailData.m_Matrix2);
        }
    }
    
    gNumOrientationBlendFailures = 0;
}

void AddFailMark(const char* failStr)
{
	size_t len = strlen(failStr);
	if (gnetVerify(len < 90))
	{
		bool bVerboseFailmark = (len > 2) && (failStr[0] == 'F') && (failStr[1] == 'A') ? true : false;

		Displayf("AddFailmark failStr[%s] len[%d] failStr[0][%c] failStr[1][%c] bVerboseFailmark[%d]",failStr,(int)len, len > 2 ? failStr[0] : ' ',len > 2 ? failStr[1] : ' ',bVerboseFailmark);

		// stall detect logging
		gnetDebug1("NetStallDetect :: Local added failmark. Message: %s", failStr);

		//Register Marker in local Logs
		const char* debugString = NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetLogName() : "" : failStr;
		RegisterFailMark(debugString,bVerboseFailmark);

		//Send Mark to be registered on all players.
		if (NetworkInterface::IsGameInProgress())
		{
			msgDebugAddFailMark msg(debugString, bVerboseFailmark);
			s_MessageHandler.SendMessageToAllPlayers(msg);
		}
	}
}

void SyncDRFrame(msgSyncDRDisplay &msg)
{
	//Send Frame to be shown to all players
	if (NetworkInterface::IsGameInProgress())
	{
		s_MessageHandler.SendMessageToAllPlayers(msg);
	}
}

void RegisterFailMark(const char* failStr, const bool bVerboseFailmark)
{
	Displayf(TBlue "========================================================================" TNorm);
	Displayf(TBlue "= FailMark" TNorm);

	if (failStr)
	{
		if (NetworkInterface::IsNetworkOpen())
			Displayf(TBlue"= Requested from %s",failStr);
		else
			Displayf(TBlue"= %s" TNorm,failStr);
	}
	
	Displayf(TBlue "========================================================================" TNorm);

	// Player
	Vector3 vLocalPos = CGameWorld::FindLocalPlayerCoors();
	Displayf(TBlue"Ped: %0.2f,%0.2f,%0.2f" TNorm, vLocalPos.x, vLocalPos.y, vLocalPos.z);

	// Camera
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vCamPos = camInterface::GetPos();
	Vector3 vCamDir = Vector3(0,0,0);

	if (debugDirector.IsFreeCamActive())
	{
		const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
		vLocalPos = vCamPos;

		freeCamFrame.GetWorldMatrix().ToEulersXYZ(vCamDir);
		vCamDir *= RtoD; //Convert as level guys work in Degrees, not Radians.

		float fFOV = freeCamFrame.GetFov();

		Displayf(TBlue"Free Cam: %0.2f,%0.2f,%0.2f  Dir:%0.2f,%0.2f,%0.2f  Fov:%0.2f" TNorm,vCamPos.x, vCamPos.y, vCamPos.z, vCamDir.x, vCamDir.y, vCamDir.z, fFOV);
	}
	else
	{
		if (CPauseMenu::IsActive())
		{
			Displayf(TBlue"Map Cursor: %0.2f,%0.2f" TNorm,CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y);
		}
		else
		{
			Displayf(TBlue"Cam: %0.2f,%0.2f,%0.2f" TNorm,vCamPos.x, vCamPos.y, vCamPos.z);
		}
	}

	// Block
	if (CBlockView::GetNumberOfBlocks())
	{
		int block = CBlockView::GetCurrentBlockInside();
		Displayf(TBlue"Block: %d %s %s" TNorm, block, CBlockView::GetBlockName(block), CBlockView::GetBlockUser(block));
	}
	else
	{
		Displayf(TBlue"Block: ? <unknown> <unknown>");
	}

	Displayf(TBlue"Version: (v%s) Time: %02d:%02d Fps:%3.1f" TNorm, CDebug::GetVersionNumber(), CClock::GetHour(),CClock::GetMinute(),(1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep())));
	
	//Network only failmark logging
	if (NetworkInterface::IsNetworkOpen())
	{
		Displayf(TBlue"Player: %s" TNorm,NetworkInterface::GetLocalPlayerName());

		Displayf(TBlue"Host: %s" TNorm,NetworkInterface::GetHostName());

		Displayf(TBlue"Freemode Script Host: %s" TNorm,NetworkInterface::GetFreemodeHostName());

		Displayf(TBlue"Players: %d" TNorm,NetworkInterface::GetNumActivePlayers());

		if (bVerboseFailmark)
		{
			unsigned numActivePlayers = netInterface::GetNumActivePlayers();
			Displayf("========================================================================");
			Displayf("Players: active players[%d]",numActivePlayers);
			Displayf("IsInMPTutorial: %s", NetworkInterface::IsInMPTutorial() ? "true" : "false");
			Displayf("IsInMPCutscene: %s", NetworkInterface::IsInMPCutscene() ? "true" : "false");
			Displayf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ");

			const netPlayer* const *activePlayers = netInterface::GetAllActivePlayers();

			for(unsigned index = 0; index < numActivePlayers; index++)
			{
				const CNetGamePlayer *netGamePlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);
				CNetObjPed* pNetObjPed = netGamePlayer && netGamePlayer->GetPlayerPed() && netGamePlayer->GetPlayerPed()->GetNetworkObject() ? static_cast<CNetObjPed*>(netGamePlayer->GetPlayerPed()->GetNetworkObject()) : NULL;
				if (pNetObjPed)
				{
					pNetObjPed->ProcessFailmark();
				}
			}

			//------
			//Isolate closest vehicles

			CPed* pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			CVehicle* pNextClosestVehicle = NULL;
			CVehicle* pClosestVehicle = NULL;
			CVehicle* pClosestPlaneHeli = NULL;
			if (pLocalPlayer && NetworkInterface::GetLocalPlayer())
			{
				Vector3 vLocalPlayerPos = NetworkInterface::GetPlayerFocusPosition(*NetworkInterface::GetLocalPlayer());

				float fClosestVehicleDistance = 99999999.f;
				float fNextClosestVehicleDistance = 99999999.f;
				float fClosestPlaneHeliDistance = 99999999.f;
				CVehicle::Pool *VehiclePool = CVehicle::GetPool();
				CVehicle* pVehicle;
				s32 i= (s32) VehiclePool->GetSize();
				while(i--)
				{
					pVehicle = VehiclePool->GetSlot(i);
					if(pVehicle)
					{
						float fDiff = (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - vLocalPlayerPos).Mag();
						if (fDiff < fClosestVehicleDistance)
						{
							if (pClosestVehicle)
							{
								pNextClosestVehicle = pClosestVehicle;
								fNextClosestVehicleDistance = fClosestVehicleDistance;
							}

							pClosestVehicle = pVehicle;
							fClosestVehicleDistance = fDiff;
						}
						else
						{
							if (fDiff < fNextClosestVehicleDistance)
							{
								pNextClosestVehicle = pVehicle;
								fNextClosestVehicleDistance = fDiff;
							}
						}

						if ((pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE) || (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI))
						{
							if (fDiff < fClosestPlaneHeliDistance)
							{
								pClosestPlaneHeli = pVehicle;
							}
						}
					}
				}
			}

			if (pClosestVehicle && pClosestVehicle->GetNetworkObject())
			{
				Displayf("========================================================================");
				Displayf("Closest vehicle:");

				CNetObjVehicle* pNetworkClosestVehicle = static_cast<CNetObjVehicle*>(pClosestVehicle->GetNetworkObject());
				if (pNetworkClosestVehicle)
					pNetworkClosestVehicle->ProcessFailmark();
			}

			if (pNextClosestVehicle)
			{
				Displayf("========================================================================");
				Displayf("Next Closest vehicle:");

				CNetObjVehicle* pNetworkNextClosestVehicle = static_cast<CNetObjVehicle*>(pNextClosestVehicle->GetNetworkObject());
				if (pNetworkNextClosestVehicle)
					pNetworkNextClosestVehicle->ProcessFailmark();
			}

			if (pClosestPlaneHeli)
			{
				Displayf("========================================================================");
				Displayf("Closest plane/heli:");

				CNetObjVehicle* pNetworkClosestVehicle = static_cast<CNetObjVehicle*>(pClosestPlaneHeli->GetNetworkObject());
				if (pNetworkClosestVehicle)
					pNetworkClosestVehicle->ProcessFailmark();
			}

			Displayf("========================================================================");
			Displayf("Closest ambient ped:");

			CPed* pClosestPed = NULL;
			if (pLocalPlayer && NetworkInterface::GetLocalPlayer())
			{
				Vector3 vLocalPlayerPos = NetworkInterface::GetPlayerFocusPosition(*NetworkInterface::GetLocalPlayer());

				float fClosestPedDistance = 99999999.f;
				CPed::Pool *PedPool = CPed::GetPool();
				CPed* pPed;
				s32 i= (s32) PedPool->GetSize();
				while(i--)
				{
					pPed = PedPool->GetSlot(i);
					if(pPed && !pPed->IsPlayer())
					{
						float fDiff = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vLocalPlayerPos).Mag();
						if (fDiff < fClosestPedDistance)
						{
							pClosestPed = pPed;
							fClosestPedDistance = fDiff;
						}
					}
				}
			}

			if (pClosestPed && pClosestPed->GetNetworkObject())
			{
				CNetObjPed* pNetworkClosestPed = static_cast<CNetObjPed*>(pClosestPed->GetNetworkObject());
				if (pNetworkClosestPed)
					pNetworkClosestPed->ProcessFailmark();
			}

			Displayf("========================================================================");

			CTrain::DumpTrainInformation();
		}
	}

	Displayf(TBlue "========================================================================" TNorm);
	Displayf(TBlue "= End fail mark" TNorm);
	Displayf(TBlue "========================================================================" TNorm);

	if (NetworkInterface::IsGameInProgress() &&  gnetVerify(strlen(failStr) < 180))
	{
		char logStr[200];
		sprintf(logStr, "@@ %s @@", failStr);

		NetworkLogUtils::WriteLogEvent(CNetwork::GetMessageLog(), logStr, "");
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), logStr, "");
		NetworkLogUtils::WriteLogEvent(CNetwork::GetPlayerMgr().GetLog(), logStr, "");
		NetworkLogUtils::WriteLogEvent(CNetwork::GetEventManager().GetLog(), logStr, "");
		NetworkLogUtils::WriteLogEvent(CNetwork::GetArrayManager().GetLog(), logStr, "");
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetReassignMgr().GetLog(),logStr, "");

        if(CTheScripts::GetScriptHandlerMgr().GetLog())
        {
            NetworkLogUtils::WriteLogEvent(*CTheScripts::GetScriptHandlerMgr().GetLog(), logStr, "");
        }

        gnetDebug2("%s", failStr);
	}
}

void DisplayDebugMessage(const char* message)
{
	if (!s_bDisplayPauseText)
	{
		if (gnetVerifyf(strlen(message) < MAX_DEBUG_TEXT, "NetworkDebug::DisplayDebugMessage - message is >= 100: %s", message))
		{
			safecpy( s_debugText, message );
			s_bDisplayDebugText = true;
			s_debugDisplayTimer = 1000;
		}
	}
}

void DisplayHttpReqTrackerMessage(const char* message)
{
	netDebug("NetworkDebug::DisplayHttpReqTrackerMessage");

	if (gnetVerifyf(strlen(message) < 256, "NetworkDebug::DisplayHttpReqTrackerMessage - message is >= 256: %s", message))
	{
		safecpy( s_httpReqTrackerText, message );
		s_bDisplayHttpReqTrackerText = true;
		s_httpReqTrackerTextDisplayTimer = 10000;
	}
}

bool FailCloneRespawnNetworkEvent()
{
	return s_failCloneRespawnNetworkEvent;
}

bool FailRespawnNetworkEvent()
{
	return s_failRespawnNetworkEvent;
}

bool  DebugWeaponDamageEvent()
{
	return s_debugWeaponDamageEvent;
}

bool CanDebugBreak(eDebugBreakType breakType)
{
	bool bCanBreak = m_breakTypes[breakType] && sysBootManager::IsDebuggerPresent();

	return bCanBreak;
}

bool DisplayDebugEntityDamageInfo()
{
	return s_DebugEntityDamage;
}

void FlushDebugLogs(bool waitForFlush)
{
	if (s_PredictionLog)
    {
		s_PredictionLog->Flush(waitForFlush);
    }
}

bool CanDebugBreakForFocus(eDebugBreakType breakType, netObject& netObj)
{
	bool bCanBreak = false;

	if (CanDebugBreak(breakType))
	{
		CEntity* pFocus = CDebugScene::FocusEntities_Get(0);

		if (pFocus && pFocus->GetIsPhysical())
		{
			bCanBreak = static_cast<CPhysical*>(pFocus)->GetNetworkObject() == &netObj;
		}
	}

	return bCanBreak;
}


bool CanDebugBreakForFocusId(eDebugBreakType breakType, ObjectId objectId)
{
	bool bCanBreak = false;

	if (CanDebugBreak(breakType))
	{
		bCanBreak = ((int)objectId == ms_FocusEntityId);
	}

	return bCanBreak;
}

void UpdateFocusEntity(bool bNewEntityChosen)
{
	CEntity* pFocus = CDebugScene::FocusEntities_Get(0);
	netObject* pFocusNetObj = (pFocus && pFocus->GetIsPhysical()) ? static_cast<CPhysical*>(pFocus)->GetNetworkObject() : NULL;
	int focusId = pFocusNetObj ? pFocusNetObj->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

	if (ms_FocusEntityId == NETWORK_INVALID_OBJECT_ID)
	{
		ms_FocusEntityId = focusId;
	}
	else if (focusId != ms_FocusEntityId)
	{
		if (bNewEntityChosen)
		{
			ms_FocusEntityId = focusId;
		}
		else
		{
			netObject* pNetObj = NetworkInterface::GetNetworkObject(static_cast<ObjectId>(ms_FocusEntityId));
			CEntity* pFocusEntity = pNetObj ? pNetObj->GetEntity() : NULL;

			if (!pFocusEntity)
            {
				ms_FocusEntityId = NETWORK_INVALID_OBJECT_ID;

                CDebugScene::FocusEntities_Clear();
            }
            else
            {
			    CDebugScene::FocusEntities_Set(pFocusEntity, 0);
            }
		}
	}
}

const char *GetMemCategoryName(NetworkMemoryCategory category)
{
    switch(category)
    {
	case NET_MEM_GAME_ARRAY_HANDLERS:
		return "Network Game Array handlers";
	case NET_MEM_SCRIPT_ARRAY_HANDLERS:
		return "Network Script Array handlers";
    case NET_MEM_BOTS:
        return "Network Bots";
    case NET_MEM_CONNECTION_MGR_HEAP:
        return "Network Connection Manager Heap";
    case NET_MEM_RLINE_HEAP:
        return "Rline Heap";
    case NET_MEM_LIVE_MANAGER_AND_SESSIONS:
        return "Network Live Manager / Sessions";
    case NET_MEM_MISC:
        return "Network misc";
    case NET_MEM_OBJECT_MANAGER:
        return "Network Object Manager";
    case NET_MEM_EVENT_MANAGER:
        return "Network Event Manager";
    case NET_MEM_ARRAY_MANAGER:
        return "Network Array Manager";
    case NET_MEM_BANDWIDTH_MANAGER:
        return "Network Bandwidth Manager";
    case NET_MEM_ROAMING_BUBBLE_MANAGER:
        return "Network Roaming Bubble Manager";
    case NET_MEM_NETWORK_OBJECTS:
        return "Network Objects";
    case NET_MEM_NETWORK_BLENDERS:
        return "Network Blenders";
	case NET_MEM_VOICE:
		return "Network Voice";
	case NET_MEM_PLAYER_SYNC_DATA:
        return "Network Player Sync Data";
    case NET_MEM_PED_SYNC_DATA:
        return "Network Ped Sync Data";
    case NET_MEM_VEHICLE_SYNC_DATA:
        return "Network Vehicle Sync Data";
    case NET_MEM_OBJECT_SYNC_DATA:
        return "Network Object Sync Data";
    case NET_MEM_PICKUP_SYNC_DATA:
        return "Network Pickup Sync Data";
    case NET_MEM_PICKUP_PLACEMENT_SYNC_DATA:
        return "Network Pickup Placement Sync Data";
    case NET_MEM_PLAYER_MGR:
        return "Network Player Manager";
	case NET_MEM_DOOR_SYNC_DATA:
		return "Network Door Sync Data";
	case NET_MEM_GLASS_PANE_SYNC_DATA:
		return "Network Glass Pane Sync Data";
	case NET_MEM_LEADERBOARD_DATA:
		return "Network Leaderboard Data";
    default:
        gnetAssertf(0, "Unexpected memory category specified!");
        return "Unknown";
    }
}

#if __ASSERT

void SanityCheckHeapTracking()
{
    size_t totalAllocated = 0;

    // take into account 16 bytes automatically reserved by the allocator
    totalAllocated += 16;

    for(unsigned index = 0; index < NET_MEM_NUM_MANAGERS; index++)
    {
        totalAllocated += g_NetworkHeapUsage[index];
    }

    size_t heapUsage = CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable();
    gnetAssertf(totalAllocated == heapUsage, "Memory tracking fail!");
}

#endif // __ASSERT

void RegisterMemoryAllocated(NetworkMemoryCategory category, u32 memoryAllocated)
{
    if(gnetVerifyf(category >= 0 && category < NET_MEM_NUM_MANAGERS, "Invalid memory category specified!"))
    {
        g_NetworkHeapUsage[category] += memoryAllocated;
    }

    //ASSERT_ONLY(SanityCheckHeapTracking());
}

void RegisterMemoryDeallocated(NetworkMemoryCategory category, u32 memoryDeallocated)
{
    if(gnetVerifyf(category >= 0 && category < NET_MEM_NUM_MANAGERS, "Invalid memory category specified!"))
    {
        if(g_NetworkHeapUsage[category] >= memoryDeallocated)
        {
            g_NetworkHeapUsage[category] -= memoryDeallocated;
        }
    }

    //ASSERT_ONLY(SanityCheckHeapTracking());
}

bool ShouldForceCloneTasksOutOfScope(const CPed *ped)
{
    bool forceOutOfScope = false;

    if(ped && s_ForceCloneTasksOutOfScope)
    {
        CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

	    if(focusEntity == ped)
	    {
            return true;
        }
    }

    return forceOutOfScope;
}

void LookAtAndPause(ObjectId objectID)
{
    if(!fwTimer::IsGamePaused())
    {
        ms_bViewFocusEntity = true;
        ms_FocusEntityId    = objectID;
        UpdateFocusEntity(false);
        ViewFocusEntity();

        fwTimer::StartUserPause(true);

        char logName[256];
        CNetwork::GetObjectManager().GetLogName(logName, sizeof(logName), objectID);
        gnetDebug2("Looking at %s", logName);
    }
}

void LogVehiclesUsingInappropriateModels()
{
    NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "DUMPING_INAPPROPRIATE_VEHICLE_USAGE","");

    s32 index = CNetObjVehicle::GetPool()->GetSize();

    while(index--)
    {
        CNetObjVehicle *vehicle = CNetObjVehicle::GetPool()->GetSlot(index);

        if(vehicle && vehicle->GetVehicle())
        {
            int numModels = gPopStreaming.GetInAppropriateLoadedCars().CountMembers();

            for(int model = 0; model < numModels; model++)
            {
                if((u32)vehicle->GetVehicle()->GetModelIndex() == gPopStreaming.GetInAppropriateLoadedCars().GetMember(model))
                {
                    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), vehicle->GetPhysicalPlayerIndex(), "USING_INAPPROPRIATE_VEHICLE", vehicle->GetLogName());
                    NetworkInterface::GetObjectManagerLog().WriteDataValue("Model", vehicle->GetVehicle()->GetBaseModelInfo()->GetModelName());
                    NetworkInterface::GetObjectManagerLog().WriteDataValue("Last Remove Fail Reason", vehicle->GetVehicle()->GetLastVehPopRemovalFailReason());
                    NetworkInterface::GetObjectManagerLog().WriteDataValue("Last Remove Fail Time", "%d", vehicle->GetVehicle()->GetLastVehPopRemovalFailFrame());
                }
            }
        }
    }
}

void DumpVehiclesUsingInappropriateModelsToTTY()
{
    s32 index = CNetObjVehicle::GetPool()->GetSize();

    while(index--)
    {
        CNetObjVehicle *vehicle = CNetObjVehicle::GetPool()->GetSlot(index);

        if(vehicle && vehicle->GetVehicle())
        {
            int numModels = gPopStreaming.GetInAppropriateLoadedCars().CountMembers();

            for(int model = 0; model < numModels; model++)
            {
                if((u32)vehicle->GetVehicle()->GetModelIndex() == gPopStreaming.GetInAppropriateLoadedCars().GetMember(model))
                {
                    Displayf("Inappropriate %s vehicle: %s - model (%s) - remove fail reason (%s), checked at frame (%d)", vehicle->IsClone() ? "remote" : "local", vehicle->GetLogName(), vehicle->GetVehicle()->GetBaseModelInfo()->GetModelName(), vehicle->GetVehicle()->GetLastVehPopRemovalFailReason(), vehicle->GetVehicle()->GetLastVehPopRemovalFailFrame());
                }
            }
        }
    }
}

void LogPedsUsingInappropriateModels()
{
    NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "DUMPING_INAPPROPRIATE_PED_USAGE","");

    s32 index = CNetObjPed::GetPool()->GetSize();

    while(index--)
    {
        CNetObjPed *ped = CNetObjPed::GetPool()->GetSlot(index);

        if(ped && ped->GetPed())
        {
            int numModels = gPopStreaming.GetInAppropriateLoadedPeds().CountMembers();

            for(int model = 0; model < numModels; model++)
            {
                if((u32)ped->GetPed()->GetModelIndex() == gPopStreaming.GetInAppropriateLoadedPeds().GetMember(model))
                {
                    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), ped->GetPhysicalPlayerIndex(), "USING_INAPPROPRIATE_PED", ped->GetLogName());
                    NetworkInterface::GetObjectManagerLog().WriteDataValue("Model", ped->GetPed()->GetBaseModelInfo()->GetModelName());
                }
            }
        }
    }
}

bool  DebugPedMissingDamageEvents()
{
	return s_DebugPedMissingDamageEvents;
}

u32   RemotePlayerFadeOutGetTimer()
{
	return (u32)s_bRemotePlayerFadeOutimer;
}

bool ShouldForceNetworkCompliance()
{
	return s_ForceNetworkCompliance;
}

#endif  //__BANK


#if __DEV

CPed *GetCameraTargetPed()
{
    if(!s_FollowPedTarget)
    {
        s_FollowPedTarget = FindFollowPed();
    }

    return s_FollowPedTarget;
}

//
// name:        UpdatePopulationLog
// description: updates the population log
//
void UpdatePopulationLog()
{
    static bool firstTimeRun = true;

    if(s_PopulationLog)
    {
        if(firstTimeRun)
        {
            s_PopulationLog->Log("Frame, No creation pos, No car model, Network Population, Going against traffic, dead end chosen, No path nodes");
            s_PopulationLog->Log(", ground placement, ground placement special, Network Visibility, Point creation distance, Point creation distance special");
            s_PopulationLog->Log(", Relative movement, Bounding box, Bounding box special, Vehicle create, Driver Add");
            s_PopulationLog->LineBreak();
            firstTimeRun = false;
        }
        else
        {
            s_PopulationLog->Log("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
                                 fwTimer::GetFrameCount(),
                                 CVehiclePopulation::m_populationFailureData.m_numNoCreationPosFound,
                                 CVehiclePopulation::m_populationFailureData.m_numNoVehModelFound,
                                 CVehiclePopulation::m_populationFailureData.m_numNetworkPopulationFail,
                                 CVehiclePopulation::m_populationFailureData.m_numVehsGoingAgainstTrafficFlow,
                                 CVehiclePopulation::m_populationFailureData.m_numDeadEndsChosen,
                                 CVehiclePopulation::m_populationFailureData.m_numNoPathNodesAvailable,
								 CVehiclePopulation::m_populationFailureData.m_numGroundPlacementFail,
                                 CVehiclePopulation::m_populationFailureData.m_numGroundPlacementSpecialFail,
                                 CVehiclePopulation::m_populationFailureData.m_numNetworkVisibilityFail,
								 CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistFail,
                                 CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistSpecialFail,
                                 CVehiclePopulation::m_populationFailureData.m_numRelativeMovementFail,
								 CVehiclePopulation::m_populationFailureData.m_numBoundingBoxFail,
                                 CVehiclePopulation::m_populationFailureData.m_numBoundingBoxSpecialFail,
                                 CVehiclePopulation::m_populationFailureData.m_numVehicleCreateFail,
                                 CVehiclePopulation::m_populationFailureData.m_numDriverAddFail);

            s_PopulationLog->LineBreak();
        }
    }
}
#endif  //__DEV

#if __BANK
//
// name:        UpdateBandwidthProfiler
// description: updates the bandwidth profiler
//
void UpdateBandwidthProfiler()
{
    // update the bandwidth rankings
    static u32 lastTimeUpdated = fwTimer::GetSystemTimeInMilliseconds();
    u32 currentTime            = fwTimer::GetSystemTimeInMilliseconds();
    u32 timeSinceLastUpdate    = currentTime - lastTimeUpdated;

    if(timeSinceLastUpdate > 1000)
    {
        lastTimeUpdated = currentTime;

		netBandwidthStatistics::UpdateBandwidthRankings();
    }
}

//
// name:        UpdateBandwidthLog
// description: updates the bandwidth log with the bandwidth used this frame
//
void UpdateBandwidthLog()
{
    static bool firstTimeRun = true;
	static const u32  numBandwidthRecordersToDisplay = 5;
	static const unsigned sizeOfLogStr = netLogImpl::MAX_LOG_STRING;

	char logStr[sizeOfLogStr];

	u32 stats;

    if(firstTimeRun)
    {
        formatf(logStr, sizeOfLogStr, ", Frame, Packets In, Packets Out, Total In, Total Out, Game In, Game Out, SNet In, SNet Out, SAct In, SAct Out, Voice Session In, Voice Session Out, Voice In, Voice Out");
 
		for (stats=0; stats<CNetwork::GetBandwidthManager().GetNumStatistics(); stats++)
		{
			const netBandwidthStatistics* pStats = CNetwork::GetBandwidthManager().GetStatistics(stats);

			if (AssertVerify(pStats))
			{
				snprintf(logStr, sizeOfLogStr, "%s, %s In, %s Out", logStr, pStats->GetName(), pStats->GetName());
			}
		}
        snprintf(logStr, sizeOfLogStr, "%s, Total Managers In, Total Managers Out", logStr);
        snprintf(logStr, sizeOfLogStr, "%s, Estimated Packet Overhead In, Estimated Packet Overhead Out, Estimated Rage Headers In, Estimated Rage Headers Out", logStr);
        snprintf(logStr, sizeOfLogStr, "%s, Networkobject ownership count, Networkobject total cloned count", logStr);
        snprintf(logStr, sizeOfLogStr, "%s, Misc In, Misc Out", logStr);

        // output the message counts
        snprintf(logStr, sizeOfLogStr, "%s, ArrayManagerMsg, ArrayManagerAckMsg, EventManagerMsg, EventManagerReliablesMsg", logStr);
        snprintf(logStr, sizeOfLogStr, "%s, CloneSyncMsg, CloneSyncAckMsg, PackedReliablesMsg", logStr);
        snprintf(logStr, sizeOfLogStr, "%s, ReassignNegotiateMsg, ReassignConfirmMsg, ReassignResponseMsg, NonPhysicalUpdateMsgs, MiscReliableMsg, MiscUnreliableMsg, OutgoingReliables, OutgoingUnreliables", logStr);

        // log the number of failures to send syncs
        snprintf(logStr, sizeOfLogStr, "%s, Num Failures Per Update %d", logStr, CNetwork::GetBandwidthManager().GetNumFailuresPerUpdate());

        // output the highest bandwidth users (in/out)
        for(unsigned int bandwidthIndex = 0; bandwidthIndex < numBandwidthRecordersToDisplay; bandwidthIndex++)
        {
            snprintf(logStr, sizeOfLogStr, "%s, HighestInName, HighestInValue", logStr);
        }

         for(unsigned int bandwidthIndex = 0; bandwidthIndex < numBandwidthRecordersToDisplay; bandwidthIndex++)
        {
            snprintf(logStr, sizeOfLogStr, "%s, HighestOutName, HighestOutValue", logStr);
        }

		snprintf(logStr, sizeOfLogStr, "%s\n", logStr);

		CNetwork::GetBandwidthManager().GetLog().Log(logStr);
		
		firstTimeRun = false;
    }

	if(netBandwidthStatistics::GetLastFrameSorted() == fwTimer::GetFrameCount())
    {
		const netBandwidthRecorder& socketRecorder = CNetwork::GetBandwidthManager().GetSocketRecorder();
		const netBandwidthRecorder& gameRecorder = CNetwork::GetBandwidthManager().GetGameChannelRecorder();
		const netBandwidthRecorder& gameSessionRecorder = CNetwork::GetBandwidthManager().GetVoiceSessionChannelRecorder();
        const netBandwidthRecorder& activitySessionRecorder = CNetwork::GetBandwidthManager().GetActivitySessionChannelRecorder();
        const netBandwidthRecorder& voiceSessionRecorder = CNetwork::GetBandwidthManager().GetGameSessionChannelRecorder();
        const netBandwidthRecorder& voiceRecorder = CNetwork::GetBandwidthManager().GetVoiceChannelRecorder();

        unsigned numUnreliableMessagesSent = netInterface::GetNumUnreliableMessagesSent();
        unsigned numReliableMessagesSent   = netInterface::GetNumReliableMessagesSent();

        gLastNumOutgoingReliables = numReliableMessagesSent;

        // NOTE: All of these values are in bits
        float totalIn  = static_cast<float>(socketRecorder.GetInboundBandwidth() << 3);
        float totalOut = static_cast<float>(socketRecorder.GetOutboundBandwidth() << 3);
        float gameIn   = static_cast<float>(gameRecorder.GetInboundBandwidth() << 3);//GetArrayManager().GetBandwidthInRate()  + NetworkInterface::GetEventManager().GetBandwidthInRate()  + NetworkInterface::GetObjectManager().GetBandwidthInRate()  + NetworkInterface::GetBandwidthInRate();
        float gameOut  = static_cast<float>(gameRecorder.GetOutboundBandwidth() << 3);//GetArrayManager().GetBandwidthOutRate() + NetworkInterface::GetEventManager().GetBandwidthOutRate() + NetworkInterface::GetObjectManager().GetBandwidthOutRate() + NetworkInterface::GetBandwidthOutRate();
        float sGameIn  = static_cast<float>(gameSessionRecorder.GetInboundBandwidth() << 3);
        float sGameOut = static_cast<float>(gameSessionRecorder.GetOutboundBandwidth() << 3);
        float sActIn   = static_cast<float>(activitySessionRecorder.GetInboundBandwidth() << 3);
        float sActOut  = static_cast<float>(activitySessionRecorder.GetOutboundBandwidth() << 3);
        float sVoiceIn = static_cast<float>(voiceSessionRecorder.GetInboundBandwidth() << 3);
        float sVoiceOut  = static_cast<float>(voiceSessionRecorder.GetOutboundBandwidth() << 3);
        float voiceIn  = static_cast<float>(voiceRecorder.GetInboundBandwidth() << 3);
        float voiceOut = static_cast<float>(voiceRecorder.GetOutboundBandwidth() << 3);
        float estimatedPacketOverheadIn     = socketRecorder.GetInboundPacketFreq()  * 48.0f * 8.0f;
        float estimatedPacketOverheadOut    = socketRecorder.GetOutboundPacketFreq() * 48.0f * 8.0f;
        float estimatedRageHeadersIn        = static_cast<float>((socketRecorder.GetInboundPacketFreq() * netPacket::BYTE_SIZEOF_HEADER +
                                              socketRecorder.GetInboundPacketFreq() * netBundle::BYTE_SIZEOF_HEADER) << 3);
        float estimatedRageHeadersOut       = static_cast<float>((socketRecorder.GetOutboundPacketFreq() * netPacket::BYTE_SIZEOF_HEADER +
                                              socketRecorder.GetOutboundPacketFreq() * netBundle::BYTE_SIZEOF_HEADER +
                                              numReliableMessagesSent * netFrame::BYTE_SIZEOF_RHEADER +
                                              numUnreliableMessagesSent * netFrame::BYTE_SIZEOF_UHEADER) << 3);
        float miscIn   = totalIn  - (gameIn  + sGameIn  + sActIn  + sVoiceIn  + voiceIn  + estimatedRageHeadersIn  + estimatedPacketOverheadIn);
        float miscOut  = totalOut - (gameOut + sGameOut + sActOut + sVoiceOut + voiceOut + estimatedRageHeadersOut + estimatedPacketOverheadOut);
        miscIn  = (miscIn > 0.0f)  ? miscIn  : 0.0f;
        miscOut = (miscOut > 0.0f) ? miscOut : 0.0f;

        unsigned int numLocalNetworkObjects = 0;
        unsigned int numTotalClonedObjects  = 0;
        NetworkInterface::GetObjectManager().GetLocallyOwnedObjectsInfo(numLocalNetworkObjects, numTotalClonedObjects);

        formatf(logStr, sizeOfLogStr, ", %d, %d, %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f", fwTimer::GetFrameCount(), socketRecorder.GetInboundPacketFreq(), socketRecorder.GetOutboundPacketFreq(), totalIn, totalOut, gameIn, gameOut, sGameIn, sGameOut, sActIn, sActOut, sVoiceIn, sVoiceOut, voiceIn, voiceOut);

		for (stats=0; stats<CNetwork::GetBandwidthManager().GetNumStatistics(); stats++)
		{
			const netBandwidthStatistics* pStats = CNetwork::GetBandwidthManager().GetStatistics(stats);

			if (AssertVerify(pStats))
			{
				snprintf(logStr, sizeOfLogStr, "%s, %.2f, %.2f", logStr, pStats->GetBandwidthInRate(), pStats->GetBandwidthOutRate());
			}
		}
		snprintf(logStr, sizeOfLogStr, "%s, %.2f, %.2f", logStr, netBandwidthStatistics::GetTotalBandwidthInRate(), netBandwidthStatistics::GetTotalBandwidthOutRate());
        snprintf(logStr, sizeOfLogStr, "%s, %.2f, %.2f, %.2f, %.2f", logStr, estimatedPacketOverheadIn, estimatedPacketOverheadOut, estimatedRageHeadersIn, estimatedRageHeadersOut);
        snprintf(logStr, sizeOfLogStr, "%s, %d, %d", logStr, numLocalNetworkObjects, numTotalClonedObjects);
        snprintf(logStr, sizeOfLogStr, "%s, %.2f, %.2f", logStr, miscIn, miscOut);

        // output the message counts
        snprintf(logStr, sizeOfLogStr, "%s, %d, %d, %d, %d", logStr, NetworkInterface::GetArrayManager().GetNumArrayManagerUpdateMessagesSent(),
                                                                     NetworkInterface::GetArrayManager().GetNumArrayManagerAckMessagesSent(),
                                                                     NetworkInterface::GetEventManager().GetNumUnreliableEventMessagesSent(),
                                                                     NetworkInterface::GetEventManager().GetNumReliableEventMessagesSent());
        snprintf(logStr, sizeOfLogStr, "%s, %d, %d, %d", logStr, NetworkInterface::GetObjectManager().GetNumCloneSyncMessagesSent(),
                                                                 NetworkInterface::GetObjectManager().GetNumCloneSyncAckMessagesSent(),
                                                                 NetworkInterface::GetObjectManager().GetNumPackedReliableMessagesSent());
        snprintf(logStr, sizeOfLogStr, "%s, %d, %d, %d, %d, %d, %d, %d, %d", logStr, NetworkInterface::GetObjectManager().GetReassignMgr().GetNumReassignNegotiateMessagesSent(),
                                                                             NetworkInterface::GetObjectManager().GetReassignMgr().GetNumReassignConfirmMessagesSent(),
                                                                             NetworkInterface::GetObjectManager().GetReassignMgr().GetNumReassignResponseMessagesSent(),
                                                                             NetworkInterface::GetPlayerMgr().GetNumNonPhysicalUpdateMessagesSent(),
                                                                             NetworkInterface::GetPlayerMgr().GetNumMiscReliableMessagesSent(),
                                                                             NetworkInterface::GetPlayerMgr().GetNumMiscUnreliableMessagesSent(),
                                                                             numReliableMessagesSent,
                                                                             numUnreliableMessagesSent);
        netInterface::ResetMessageCounts();

        // log the number of failures to send syncs
        snprintf(logStr, sizeOfLogStr, "%s, %d", logStr, CNetwork::GetBandwidthManager().GetNumFailuresPerUpdate());

        // output the highest bandwidth users (in/out)
        for(unsigned int bandwidthIndex = 0; bandwidthIndex < numBandwidthRecordersToDisplay; bandwidthIndex++)
        {
            const char *highestInName   = "";
            float       highestInValue  = 0.0f;
            netBandwidthStatistics::GetHighestBandwidthIn (bandwidthIndex, highestInName,  highestInValue);
            snprintf(logStr, sizeOfLogStr, "%s, %s, %.2f", logStr, highestInName, highestInValue);
        }

        for(unsigned int bandwidthIndex = 0; bandwidthIndex < numBandwidthRecordersToDisplay; bandwidthIndex++)
        {
            const char *highestOutName  = "";
            float       highestOutValue = 0.0f;
            netBandwidthStatistics::GetHighestBandwidthOut(bandwidthIndex, highestOutName, highestOutValue);
            snprintf(logStr, sizeOfLogStr, "%s, %s, %.2f", logStr, highestOutName, highestOutValue);
        }

		snprintf(logStr, sizeOfLogStr, "%s\n", logStr);

		CNetwork::GetBandwidthManager().GetLog().Log(logStr);
     }
}

static float GetWidthAndColour(float value, float target, float warningThreshold, CRGBA &colour)
{
    float ratio = value / target;
    if(ratio > 1.0f)
    {
        colour = CRGBA(255,0,0,255);
        ratio  = 1.0f;
    }
    else if(ratio > warningThreshold)
    {
        colour = CRGBA(255,255,0,255);
    }
    else
    {
        colour = CRGBA(0,255,0,255);
    }

    return ratio;
}

static void RenderNetworkStatusBar(float startX, float startY, float widthScale, const CRGBA &barColour)
{
    static bank_float STATUS_BAR_WIDTH   = 0.2f;
    static bank_float STATUS_BAR_HEIGHT  = 0.01f;
    static bank_u8    BORDER_ALPHA       = 220;

    // Border
    CSprite2d::DrawRect(startX,
                        startY,
                        startX + STATUS_BAR_WIDTH,
                        startY + STATUS_BAR_HEIGHT,
                        0.0f,
                        CRGBA_Black(BORDER_ALPHA));

    // don't both rendering status bars with very small sizes
    if(fabs(widthScale) > 0.01f)
    {
        // Bar
        static bank_float BORDER_SIZE_X = 0.0025f;
        static bank_float BORDER_SIZE_Y = 0.0025f;

        float barStartX     = startX + BORDER_SIZE_X;
        float barStartY     = startY + BORDER_SIZE_Y;
        float adjustedWidth = STATUS_BAR_WIDTH * widthScale;
        CSprite2d::DrawRect(barStartX,
                            barStartY,
                            barStartX + adjustedWidth     - (BORDER_SIZE_X * 2.0f),
                            barStartY + STATUS_BAR_HEIGHT - (BORDER_SIZE_Y * 2.0f),
                            0.0f,
                            barColour);
    }
}

//
// name:        RenderNetworkStatusBars
// description: renders the bandwidth and memory status bars
//
void RenderNetworkStatusBars(unsigned int totalIn,
                             unsigned int totalOut,
                             unsigned int UNUSED_PARAM(total),
                             unsigned int heapUsage,
                             unsigned int heapSize)
{
    if(!grcDebugDraw::GetDisplayDebugText())
        return;

    unsigned targetInBandwidth  = CNetwork::GetBandwidthManager().GetDebugTargetInBandwidth();
    unsigned targetOutBandwidth = CNetwork::GetBandwidthManager().GetDebugTargetOutBandwidth();

    Color32 memoryColour;
    float   memoryWidth = static_cast<float>(heapUsage) / heapSize;
    if(memoryWidth > 0.95f)
    {
        memoryColour = CRGBA(255,0,0,255);
    }
    else if(memoryWidth > 0.9f)
    {
        memoryColour = CRGBA(255,255,0,255);
    }
    else
    {
        memoryColour = CRGBA(0,255,0,255);
    }

    // display the status bars in the calculated colour and width
    static bank_float STATUS_BAR_START_X = 0.75f;
    static bank_float STATUS_BAR_START_Y = 0.14f;
    static bank_float STATUS_BAR_GAP     = 0.0125f;

    float barStartY = STATUS_BAR_START_Y;

    RenderNetworkStatusBar(STATUS_BAR_START_X, barStartY, memoryWidth, memoryColour);

    static bank_float STATUS_BAR_HEIGHT = 0.01f;

    barStartY += STATUS_BAR_HEIGHT + STATUS_BAR_GAP;

    CRGBA inBwColour;
    float inBwWidth = GetWidthAndColour(static_cast<float>(totalIn), static_cast<float>(targetInBandwidth), 0.9f, inBwColour);
    RenderNetworkStatusBar(STATUS_BAR_START_X, barStartY, inBwWidth, inBwColour);

    barStartY += STATUS_BAR_HEIGHT + STATUS_BAR_GAP;

    CRGBA outBwColour;
    float outBwWidth = GetWidthAndColour(static_cast<float>(totalOut), static_cast<float>(targetOutBandwidth), 0.9f, outBwColour);
    RenderNetworkStatusBar(STATUS_BAR_START_X, barStartY, outBwWidth, outBwColour);

    static bank_s32 STATUS_BAR_TEXT_X = 93;
    static bank_s32 STATUS_BAR_TEXT_Y = 6;
    int ty = STATUS_BAR_TEXT_Y;
    char buf[64];
    grcDebugDraw::PrintToScreenCoors("MEM", STATUS_BAR_TEXT_X, ty);
    ++ty;
    formatf(buf, sizeof(buf), "%dK IN:%.2f", (int)(targetInBandwidth / 1024.0f), totalIn / 1024.f);
    grcDebugDraw::PrintToScreenCoors(buf, STATUS_BAR_TEXT_X, ty);
    ++ty;
    formatf(buf, sizeof(buf), "%dK OUT:%.2f", (int)(targetOutBandwidth / 1024.0f), totalOut / 1024.f);
    grcDebugDraw::PrintToScreenCoors(buf, STATUS_BAR_TEXT_X, ty);

    if(CNetwork::GetBandwidthManager().ShouldDisplayManagerStatusBars())
    {
        CRGBA barColour;
        for (unsigned stats=0; stats<CNetwork::GetBandwidthManager().GetNumStatistics(); stats++)
		{
			const netBandwidthStatistics* pStats = CNetwork::GetBandwidthManager().GetStatistics(stats);

			if (AssertVerify(pStats))
			{
                static bank_s32 BANDWIDTH_BAR_TEXT_X = 87;

                ++ty;
                formatf(buf, sizeof(buf), "%s In", pStats->GetName());
                grcDebugDraw::PrintToScreenCoors(buf, BANDWIDTH_BAR_TEXT_X, ty);

                float bandwidthInRate  = pStats->GetBandwidthInRate();
                float bandwidthInWidth = GetWidthAndColour(bandwidthInRate, static_cast<float>(targetInBandwidth), 0.9f, barColour);
                barStartY += STATUS_BAR_HEIGHT + STATUS_BAR_GAP;
                RenderNetworkStatusBar(STATUS_BAR_START_X, barStartY, bandwidthInWidth, barColour);

                ++ty;
                formatf(buf, sizeof(buf), "%s Out", pStats->GetName());
                grcDebugDraw::PrintToScreenCoors(buf, BANDWIDTH_BAR_TEXT_X, ty);

                float bandwidthOutRate  = pStats->GetBandwidthOutRate();
                float bandwidthOutWidth = GetWidthAndColour(bandwidthOutRate, static_cast<float>(targetOutBandwidth), 0.9f, barColour);
                barStartY += STATUS_BAR_HEIGHT + STATUS_BAR_GAP;
                RenderNetworkStatusBar(STATUS_BAR_START_X, barStartY, bandwidthOutWidth, barColour);
			}
		}
    }
}

static
void NetworkBank_SpewPosixTime()
{
	int year, month, day, hour, min, sec;
	StatsInterface::GetPosixDate(year , month , day , hour , min , sec);
	gnetDebug1("Retrieve posix time: %d:%d:%d, %d-%d-%d ", hour, min, sec, day, month, year);
}

static
void NetworkBank_ResetNoTimeouts()
{
	if(!PARAM_notimeouts.Get() && NetworkInterface::IsGameInProgress())
	{
		sm_FlagNoTimeouts = !sm_FlagNoTimeouts;

		CNetwork::ResetConnectionPolicies(sm_FlagNoTimeouts ? 0 : CNetwork::GetTimeoutTime());

		msgDebugNoTimeouts msg(sm_FlagNoTimeouts);
		s_MessageHandler.SendMessageToAllPlayers(msg);

		gnetDebug1("Local player set flag notimeouts=\"%s\" timeout value=\"%u\""
									,sm_FlagNoTimeouts ? "TRUE" : "FALSE"
									,CNetwork::GetTimeoutTime());
	}
}

static bool s_UplinkDisconnection = false;

static
void NetworkBank_UplinkDisconnection()
{
    netSocket::SetFakeUplinkDisconnection(s_UplinkDisconnection);
}


static
void NetworkBank_StartTestCoopScript()
{/*
	static const char* testCoopScriptName = "tempcooptest";

	s32 id = g_StreamedScripts.FindSlot(testCoopScriptName);

	if(AssertVerify(id != -1))
	{
		if (!g_StreamedScripts.HasObjectLoaded(id))
		{
			g_StreamedScripts.StreamingRequest(id, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		}
		else
		{
			scrValue NewThreadID;
			scrValue InputParams[2];

			InputParams[0].String = testCoopScriptName;
			InputParams[1].Int = 8192;

			scrThread::Info info(&NewThreadID, 2, &InputParams[0]);

			scrThread::StartNewScriptWithArgs(info);
		}
	}*/
}

static eBailReason s_BailReason = eBailReason::BAIL_FROM_RAG;
static int s_BailErrorCode = eBailContext::BAIL_CTX_NONE;

static 
void NetworkBank_SetBailReason(s32 bailReason)
{
	gnetAssert(bailReason >= eBailReason::BAIL_INVALID && bailReason < eBailReason::BAIL_NUM);
	gnetDebug1("Setting bail reason to %s", NetworkUtils::GetBailReasonAsString(s_BailReason));
	s_BailReason = (eBailReason)bailReason;
}

static void NetworkBank_SetBailErrorCode(s32 bailErrorCode)
{
	gnetAssert(bailErrorCode >= eBailContext::BAIL_CTX_NONE && bailErrorCode < eBailContext::BAIL_CTX_COUNT);
	gnetDebug1("Setting bail context to %s", NetworkUtils::GetBailErrorCodeAsString(bailErrorCode));
	s_BailErrorCode = bailErrorCode;
}

static
void NetworkBank_Bail()
{
	gnetAssert(s_BailReason >= eBailReason::BAIL_INVALID && s_BailReason < eBailReason::BAIL_NUM);
	gnetDebug1("Bailing due to Network Bank. Reason: %s", NetworkUtils::GetBailReasonAsString(s_BailReason));
	NetworkInterface::Bail(s_BailReason, s_BailErrorCode);
}

static
void NetworkBank_AccessAlert()
{
	CNetwork::CheckNetworkAccessAndAlertIfFail(eNetworkAccessArea::AccessArea_Multiplayer);
}

static
void NetworkBank_ChangeFollowPedTarget()
{
    const bool bIndexValid  = static_cast<unsigned int>(s_FollowPedIndex) < s_NumFollowPedTargets;

    if(bIndexValid)
    {
		CNetGamePlayer* targetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(s_FollowPedTargetIndices[s_FollowPedIndex]));
		netObject* netobj = targetPlayer && targetPlayer->GetPlayerPed() ? targetPlayer->GetPlayerPed()->GetNetworkObject() : 0;
		if (netobj)
        {
			ObjectId objid = targetPlayer->IsLocal() ? NETWORK_INVALID_OBJECT_ID : netobj->GetObjectID();
			NetworkInterface::SetSpectatorObjectId(objid);
        }
    }
}

static
void NetworkBank_FollowLocalPlayer()
{
	NetworkInterface::SetSpectatorObjectId(NETWORK_INVALID_OBJECT_ID);
}

static
void NetworkBank_FollowFocusPed()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed() && static_cast<CPed*>(pFocusEntity)->GetNetworkObject())
	{
		NetworkInterface::SetSpectatorObjectId(static_cast<CPed*>(pFocusEntity)->GetNetworkObject()->GetObjectID());
	}
}

static
void NetworkBank_ResetAlphaOnFocusEntity()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	if(pFocusEntity && pFocusEntity->GetIsPhysical())
	{
		CNetObjPhysical* pNetObj = static_cast<CNetObjPhysical*>(static_cast<CPhysical*>(pFocusEntity)->GetNetworkObject());

		if (pNetObj)
		{
			pNetObj->SetAlphaFading(false);
			pNetObj->StopAlphaRamping();
			pNetObj->SetIsVisible(true, "NetworkBank_ResetAlphaOnFocusEntity");
		}
	}

}

void NetworkBank_FadeOutFocusEntity()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	if(pFocusEntity && pFocusEntity->GetIsPhysical())
	{
		CNetObjPhysical* pNetObj = static_cast<CNetObjPhysical*>(static_cast<CPhysical*>(pFocusEntity)->GetNetworkObject());

		if (pNetObj)
		{
			NetworkBank_ResetAlphaOnFocusEntity();
			pNetObj->SetAlphaFading(true);
		}
	}
}

static
void NetworkBank_AlphaRampInFocusEntity()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	if(pFocusEntity && pFocusEntity->GetIsPhysical())
	{
		CNetObjPhysical* pNetObj = static_cast<CNetObjPhysical*>(static_cast<CPhysical*>(pFocusEntity)->GetNetworkObject());

		if (pNetObj)
		{
			NetworkBank_ResetAlphaOnFocusEntity();
			pNetObj->SetAlphaRampFadeInAndQuit(true);
		}
	}
}

static
void NetworkBank_AlphaRampOutFocusEntity()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	if(pFocusEntity && pFocusEntity->GetIsPhysical())
	{
		CNetObjPhysical* pNetObj = static_cast<CNetObjPhysical*>(static_cast<CPhysical*>(pFocusEntity)->GetNetworkObject());

		if (pNetObj)
		{
			NetworkBank_ResetAlphaOnFocusEntity();
			pNetObj->SetAlphaRampFadeOut(true);
		}
	}
}

static netLoggingInterface *NetworkBank_GetLogFromComboIndex(int comboIndex)
{
    netLoggingInterface *logFile = 0;

    switch(comboIndex)
    {
    case E_MESSAGES_LOG:
        logFile = &CNetwork::GetMessageLog();
        break;
    case E_BANDWIDTH_LOG:
        logFile = &CNetwork::GetBandwidthManager().GetLog();
        break;
    case E_ARRAY_MANAGER_LOG:
        logFile = &CNetwork::GetArrayManager().GetLog();
        break;
    case E_EVENT_MANAGER_LOG:
        logFile = &CNetwork::GetEventManager().GetLog();
        break;
    case E_OBJECT_MANAGER_LOG:
        logFile = &CNetwork::GetObjectManager().GetLog();
        break;
    case E_PLAYER_MANAGER_LOG:
        logFile = &NetworkInterface::GetPlayerMgr().GetLog();
        break;
    case E_PREDICTION_LOG:
        logFile = s_PredictionLog;
        break;
    default:
        gnetAssertf(false, "Invalid Log File Index!");
        break;
    }

    return logFile;
}

static void UpdateLogEnabledStatus()
{
    for(unsigned index = 0; index < s_NumLogFiles; index++)
    {
        netLoggingInterface *logFile = NetworkBank_GetLogFromComboIndex(index);

        if(logFile)
        {
            s_EnableLogs[index] = logFile->IsEnabled();
        }
        else
        {
            s_EnableLogs[index] = false;
        }
    }
}

static void NetworkBank_ChangeLogFile()
{
    for(unsigned index = 0; index < s_NumLogFiles; index++)
    {
        netLoggingInterface *logFile = NetworkBank_GetLogFromComboIndex(index);

        if(logFile)
        {
            if(s_EnableLogs[index])
            {
                logFile->Enable();
            }
            else
            {
                logFile->Disable();
            }
        }
    }
}

static void ViewFocusEntity()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();

	if (ms_bViewFocusEntity)
	{
		CEntity* pFocus = CDebugScene::FocusEntities_Get(0);

		if (pFocus)
		{
			// if focus if a ped, use the debug follow cam to follow him.
			// if it is a vehicle, follow the driver if he exists
			// otherwise just point the debug cam at the entity
			if (pFocus->GetIsTypePed() || (pFocus->GetIsTypeVehicle() && static_cast<CVehicle*>(pFocus)->GetDriver()))
			{
				camInterface::DebugFollowFocusPed(true);
			}
			else
			{
				debugDirector.ActivateFreeCam();	//Turn on debug free cam.

				//Move free camera to desired place.
				camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

				static float y = 10.0f;
				static float z = 0.5f;

				Vector3 focusPos = VEC3V_TO_VECTOR3(pFocus->GetTransform().GetPosition());
				Vector3 camPos = focusPos + Vector3(0.0f, y, y*z);
				Vector3 camDir = focusPos - camPos;

				freeCamFrame.SetPosition(camPos);
				freeCamFrame.SetWorldMatrixFromFront(camDir);

				CControlMgr::SetDebugPadOn(true);	
			}
		}
	}
	else if (debugDirector.IsFreeCamActive())
	{
		camInterface::GetDebugDirector().DeactivateFreeCam();
		CControlMgr::SetDebugPadOn(false);	
	}
	else
	{
		camInterface::DebugFollowFocusPed(false);
	}
}

static void NetworkBank_FollowMaxBBChecksEntityAndPause()
{
    if(gMaxBBChecksObjectID != NETWORK_INVALID_OBJECT_ID && !fwTimer::IsGamePaused())
    {
        ms_bViewFocusEntity = true;
        ms_FocusEntityId    = gMaxBBChecksObjectID;
        UpdateFocusEntity(false);
        ViewFocusEntity();

        fwTimer::StartUserPause(true);
    }
}

static void NetworkBank_FollowMaxNoCollisionEntityAndPause()
{
    if(gMaxNoCollisionObjectID != NETWORK_INVALID_OBJECT_ID && !fwTimer::IsGamePaused())
    {
        ms_bViewFocusEntity = true;
        ms_FocusEntityId    = gMaxNoCollisionObjectID;
        UpdateFocusEntity(false);
        ViewFocusEntity();

        fwTimer::StartUserPause(true);
    }
}

static void NetworkBank_FollowNextPredictionPopAndPause()
{
    gFollowNextPredictionPop = true;
}

static void NetworkBank_MakeFocusPedSeekCover()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
    if(pFocusEntity && pFocusEntity->GetIsTypePed())
    {
        CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);

        // only pass ownership if we own the focus ped and a network game is in progress
        if(pFocusPed && !pFocusPed->IsNetworkClone() && NetworkInterface::IsGameInProgress())
        {
            Vector3 vFrom(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
			CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(vFrom));
			pCoverTask->SetSearchFlags(CTaskCover::CF_SeekCover | CTaskCover::CF_CoverSearchAny);
            pFocusPed->GetPedIntelligence()->AddTaskAtPriority(pCoverTask, PED_TASK_PRIORITY_PRIMARY, true);
        }
    }
}

static void NetworkBank_GiveFocusPedScriptedUseNearestScenarioToPos()
{
    CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
    if(pFocusEntity)
    {
        // only pass ownership to the focus ped and a network game is in progress
        if(pFocusEntity && NetworkInterface::IsGameInProgress())
        {
            if(pFocusEntity->GetIsTypePed())
            {
                CPed *ped = SafeCast(CPed, pFocusEntity);

                if(ped)
                {
					CTask* pTaskToAdd = NULL;

					Vector3 vPos1 = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
					CPhysics::GetMeasuringToolPos(0, vPos1);
					scrVector vec;
					vec.x = vPos1.x;
					vec.y = vPos1.y;
					vec.z = vPos1.z;

					pTaskToAdd = task_commands::CommonUseNearestScenarioToPos( ped, vec, s_UseNearestScenarioRange,s_UseNearestScenarioWarp, true, 0 );

#if !__FINAL
					netObject *focusNetObj = pFocusEntity->GetNetworkObject();
					if(pTaskToAdd==NULL)
					{
						Displayf("%s No nearest scenario task found.", focusNetObj?focusNetObj->GetLogName():"No net obj");
					}
					else
					{
						
						Displayf("%s Found nearest scenario task %s.", focusNetObj?focusNetObj->GetLogName():"No net obj", pTaskToAdd->GetTaskName());
					}
#endif // !__FINAL

                    NetworkInterface::GivePedScriptedTask(ped, pTaskToAdd, "TASK_USE_NEAREST_SCENARIO_TO_COORD");

                }
            }
        }
    }
}


static void NetworkBank_UseMyClan()
{
	if (s_LeaderboardClanIndex == 0 && CLiveManager::IsOnline() && CLiveManager::GetNetworkClan().HasPrimaryClan())
	{
		const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
		const rlClanDesc* clan = CLiveManager::GetNetworkClan().GetPrimaryClan();
		if (gnetVerify(clan))
		{
			s_LeaderboardCurrentClanId = clan->m_Id;
			s_LeaderboardClanIndex = (s_LeaderboardClanIndex + 1) % CLiveManager::GetNetworkClan().GetMembershipCount(gi->GetGamerHandle());

			gnetDebug1("Write/Read Leaderboards - Using clan id %" I64FMT "d", s_LeaderboardCurrentClanId);
		}
	}
	else
	{
		s_LeaderboardClanIndex = 0;
	}
}

static void NetworkBank_SendRemoteLog()
{
	gnetDebug1("%s was called...", __FUNCTION__);
	netRemoteLog::Append(ATSTRINGHASH("NetworkBank_SendRemoteLog was called...", 0x32404cad));

	netRemoteLog::BeginSend();
}

static void NetworkBank_ApplyNetworkAccessCode()
{
#if !__NO_OUTPUT
    gnetDebug1("Apply debug access code");
    s_BankNetworkAccessCode = (s_NetworkAccessComboIndex - 2);
#endif
}

rlClanId  NetworkBank_GetLeaderboardClanId()
{
	return s_LeaderboardCurrentClanId;
}

static void NetworkBank_PassControlOfFocusEntity()
{
    CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
    if(pFocusEntity)
    {
		// only pass ownership if we own the focus ped and a network game is in progress
        if(pFocusEntity && !pFocusEntity->IsNetworkClone() && NetworkInterface::IsGameInProgress())
        {
            float                 fClosestDistance = 1000000.0f;// Default big value.
            const CNetGamePlayer *pNearestPlayer   = 0;

			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

                if(remotePlayer->GetPlayerPed())
                {
				    float fDistance = Dist(remotePlayer->GetPlayerPed()->GetTransform().GetPosition(), pFocusEntity->GetTransform().GetPosition()).Getf();
				    if(fDistance < fClosestDistance)
				    {
					    pNearestPlayer = remotePlayer;
					    fClosestDistance = fDistance;
				    }
                }
            }

            if(pNearestPlayer)
            {
                netObject *focusNetObj = pFocusEntity->GetNetworkObject();

                if (focusNetObj)
                {
                    unsigned resultCode = 0;
                    if(focusNetObj->CanPassControl(*pNearestPlayer, MIGRATE_FORCED, &resultCode))
                    {
                        CGiveControlEvent::Trigger(*pNearestPlayer, focusNetObj, MIGRATE_FORCED);
                    }
                    else
                    {
                        gnetDebug2("Cannot pass control of %s: Reason: %s", focusNetObj->GetLogName(), NetworkUtils::GetCanPassControlErrorString(resultCode));
                    }
                }
            }
		}
    }
}


static void NetworkBank_RequestControlOfFocusEntity()
{
    CDynamicEntity *focusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;

	if(focusEntity)
	{
		netObject* netObj = focusEntity->GetNetworkObject();
		if (netObj
			&& netObj->IsClone()
			&& AssertVerify(NetworkInterface::GetLocalPlayer()))
		{
            unsigned resultCode = 0;
            if(netObj->CanPassControl(*NetworkInterface::GetLocalPlayer(), MIGRATE_SCRIPT, &resultCode))
            {
			    CRequestControlEvent::Trigger(netObj NOTFINAL_ONLY(, "Rag debug"));
            }
            else
            {
                gnetDebug2("Cannot request control of %s: Reason: %s", netObj->GetLogName(), NetworkUtils::GetCanPassControlErrorString(resultCode));
            }
		}
		else if (netObj && !netObj->IsClone())
		{
			netDebug1("NetworkBank_RequestControlOfFocusEntity: Already in control of Focus Ped");
		}
	}
}

static
void NetworkBank_CreateBarrel()
{
	// for container - replace with ("Prop_Contr_03b_LD", 0x342160A2)
    CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("prop_barrel_01a",0x9866A5DB), NULL);

    if(modelInfo)
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            Vector3 newPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(playerPed->GetTransform().GetB()) * 2.0f);

            int newObjectIndex = 0;
			object_commands::ObjectCreationFunction(modelInfo->GetHashKey(), newPos.x, newPos.y, newPos.z, false, true, -1, newObjectIndex, true, false);

			CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(newObjectIndex);
            if(pObject)
            {
                pObject->m_nObjectFlags.bIsStealable = true;
            }
        }
    }
}

static void NetworkBank_CreateFire()
{
	CPed *playerPed = FindFollowPed();

	if(playerPed)
	{
		Vector3 firePos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(playerPed->GetTransform().GetB()) * 2.0f);
		Vector3 fireNormal(0.0f, 0.0f, 1.0f);
		g_fireMan.StartMapFire(RCC_VEC3V(firePos), RCC_VEC3V(fireNormal), 0, NULL, 20.0f, 1.0f, 2.0f, 0.0f, 0.0f, 1, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
	}
}

static void NetworkBank_InMPCutscene()
{
	NetworkInterface::SetInMPCutsceneDebug(ms_bInMPCutscene);

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

	if (ms_bInMocapCutscene && pLocalPlayer && pLocalPlayer->GetNetworkObject())
	{
		if (ms_bInMPCutscene)
		{
			NetworkInterface::CutsceneStartedOnEntity(*pLocalPlayer);
		}
		else
		{
			NetworkInterface::CutsceneFinishedOnEntity(*pLocalPlayer);
		}

		SafeCast(CNetObjPhysical, pLocalPlayer->GetNetworkObject())->SetRemotelyVisibleInCutscene(true);
	}
}

static bool DumpObjectsUsingModelPredicate(const netObject *networkObject)
{
    CEntity *entity = networkObject ? networkObject->GetEntity() : 0;

    if(entity && ((u32)entity->GetModelIndex() == s_ModelToDump))
    {
        Displayf("%s", networkObject->GetLogName());
        return true;
    }

    return false;
}

static void NetworkBank_DumpNetworkObjectsUsingModel()
{
    NetworkInterface::GetObjectManager().GetTotalNumObjects(DumpObjectsUsingModelPredicate);
}

static void NetworkBank_LogLookAts()
{
	unsigned           numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		CNetGamePlayer* pPlayer = SafeCast(CNetGamePlayer, allPhysicalPlayers[index]);
		CPed* pPed = pPlayer->GetPlayerPed();
		if(pPed->GetIkManager().IsLooking())
			gnetDebug1("LogLookAts :: %s is looking. LookAt hash: %d", pPlayer->GetLogName(), pPed->GetIkManager().GetLookAtHashKey());
		else
			gnetDebug1("LogLookAts :: %s is not looking.", pPlayer->GetLogName());
	}
}

static void NetworkBank_ToggleConcealFocusPlayer()
{
    CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
    if(pFocusEntity)
    {
        // only pass ownership if we own the focus ped and a network game is in progress
        if(pFocusEntity && pFocusEntity->IsNetworkClone() && NetworkInterface::IsGameInProgress())
        {
            if(pFocusEntity->GetIsTypePed())
            {
                CPed *ped = SafeCast(CPed, pFocusEntity);

                if(ped && ped->IsPlayer())
                {
                    CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, ped->GetNetworkObject());

                    bool conceal = !netObjPlayer->IsConcealed();
                    netObjPlayer->ConcealEntity(conceal);
                }
            }
        }
    }
}

static void NetworkBank_ToggleConcealFocusEntity()
{	
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	if(pFocusEntity && NetworkInterface::IsGameInProgress() && pFocusEntity->GetIsPhysical())
	{
		CPhysical *physicalEntity = SafeCast(CPhysical, pFocusEntity);

		if(physicalEntity)
		{
			CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physicalEntity->GetNetworkObject());

			if (netObjPhysical)
			{
				bool conceal = !netObjPhysical->IsConcealed();
				netObjPhysical->ConcealEntity(conceal);
			}			
		}			
	}	
}

static void NetworkBank_RemoveAllStickyBombsFromPlayer()
{
	CProjectileManager::RemoveAllStickyBombsAttachedToEntity(CGameWorld::FindLocalPlayer(), true);
}

static const CNetGamePlayer *NetworkBank_GetChosenNetworkPlayer()
{
	if(unsigned(s_PlayerComboIndex) < s_NumPlayers)
	{
		const char* targetName = s_Players[s_PlayerComboIndex];
		gnetAssert(targetName);

		unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
        const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
        
        for(unsigned index = 0; index < numActivePlayers; index++)
        {
            const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);

			if (!strcmp(pPlayer->GetGamerInfo().GetName(), targetName))
			{
				return pPlayer;
			}
		}
	}

	return NULL;
}

static const CNetGamePlayer *NetworkBank_GetChosenKickNetworkPlayer()
{
	if(unsigned(s_KickPlayerComboIndex) < s_NumKickPlayers)
	{
		const char* targetName = s_KickPlayers[s_KickPlayerComboIndex];
		gnetAssert(targetName);

		unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
        const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
        
        for(unsigned index = 0; index < numActivePlayers; index++)
        {
            const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);

			if (!strcmp(pPlayer->GetGamerInfo().GetName(), targetName))
			{
				return pPlayer;
			}
		}
	}

	return NULL;
}

static
void NetworkBank_WarpToPlayer()
{
    ClearOverrideSpawnPoint();

     const CNetGamePlayer* target = NetworkBank_GetChosenNetworkPlayer();
	 CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

	 if (pLocalPlayer && !pLocalPlayer->IsDead() && target && target->GetPlayerPed())
     {
        pLocalPlayer->Teleport(VEC3V_TO_VECTOR3(target->GetPlayerPed()->GetTransform().GetPosition()), target->GetPlayerPed()->GetCurrentHeading());
     }
}

static
void NetworkBank_KickPlayer()
{
	if (NetworkInterface::IsHost())
	{
		const CNetGamePlayer* target = NetworkBank_GetChosenKickNetworkPlayer();
		if (target && target->GetPlayerPed())
		{
			gnetVerify(CNetwork::GetNetworkSession().KickPlayer(target, KickReason::KR_VotedOut, 1, NetworkInterface::GetLocalGamerHandle()));
		}
	}
}

static
void NetworkBank_ClearBlacklist()
{
    CNetwork::GetNetworkSession().GetBlacklistedGamers().ClearBlacklist();
}

static
void NetworkBank_FlushLogFiles()
{
	CNetwork::FlushAllLogFiles(true);
}

static void NetworkBank_LaunchTrainDebugScript()
{
   static scrThreadId debugScriptID = THREAD_INVALID;

   if(gnetVerifyf(debugScriptID == 0, "There is already a train debug script running!"))
   {
       const strLocalIndex scriptID = g_StreamedScripts.FindSlot("train_create_widget");

       if(gnetVerifyf(scriptID != -1, "Train debug script doesn't exist!"))
       {
           g_StreamedScripts.StreamingRequest(scriptID, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);

           CStreaming::LoadAllRequestedObjects();

           if(gnetVerifyf(g_StreamedScripts.HasObjectLoaded(scriptID), "Failed to stream in train debug script"))
           {
			   debugScriptID = CTheScripts::GtaStartNewThreadOverride("train_create_widget", NULL, 0, GtaThread::GetSoakTestStackSize());

               if(gnetVerifyf(debugScriptID != 0, "Failed to start train debug script"))
               {
                   gnetDebug1("Started running train debug script");
               }
           }
       }
   }
}

static const unsigned MAX_WEATHER_NAME = 32; 
static char g_BankWeatherTypeName[MAX_WEATHER_NAME];
static int g_BankWeatherTransitionTime = 0; 

static void NetworkBank_LogGlobalClockAndWeatherTime()
{
	// grab the global clock time
	int nHour, nMinute, nSecond;
	CNetwork::GetGlobalClock(nHour, nMinute, nSecond);

	// clock logging
	gnetDebug1("LogClockAndWeather :: Current global time is %02d:%02d:%02d. Ratio: %.2f", nHour, nMinute, nSecond, CNetwork::GetGlobalClockRatio());

	// grab the global weather time
	s32 nCycleIndex;
	u32 nCycleTimer;
	CNetwork::GetGlobalWeather(nCycleIndex, nCycleTimer);

	// weather logging
	gnetDebug1("LogClockAndWeather :: Current global weather is Index: %d, Timer: %d. Ratio: %.2f", nCycleIndex, nCycleTimer, CNetwork::GetGlobalWeatherRatio());
}

static void NetworkBank_ApplyGlobalClockAndWeatherTime()
{
	CNetwork::ApplyClockAndWeather(g_BankWeatherTransitionTime != 0, static_cast<u32>(g_BankWeatherTransitionTime)); 
}

static void NetworkBank_StopApplyingGlobalClockAndWeatherTime()
{
	CNetwork::StopApplyingGlobalClockAndWeather(); 
}

static void NetworkBank_ForceWeather()
{
	s32 nWeatherTypeIndex = g_weather.GetTypeIndex(g_BankWeatherTypeName);
	if(gnetVerifyf((nWeatherTypeIndex >= 0 && nWeatherTypeIndex < g_weather.GetNumTypes()), "NetworkBank_ForceWeatherType :: Invalid weather type: %s!", g_BankWeatherTypeName))
	{
		if(g_BankWeatherTransitionTime > 0)
		{
			gnetDebug1("NetworkBank_ForceWeatherType :: Calling ForceWeatherTypeOverTime for %s over %ums", g_BankWeatherTypeName, static_cast<u32>(g_BankWeatherTransitionTime));
			g_weather.ForceWeatherTypeOverTime(static_cast<u32>(nWeatherTypeIndex), static_cast<float>(g_BankWeatherTransitionTime));
		}
		else
		{
			gnetDebug1("NetworkBank_ForceWeatherType :: Calling ForceTypeNow for %s", g_BankWeatherTypeName);
			g_weather.ForceTypeNow(nWeatherTypeIndex, CWeather::FT_SendNetworkUpdate);
		}
	}
}

static void NetworkBank_ClearForceWeather()
{
	// just re-initialise the transition (multiply time by 1000, the main weather transition works in seconds)
	if(CNetwork::IsApplyingMultiplayerGlobalClockAndWeather() && g_BankWeatherTransitionTime > 0)
		CNetwork::StartWeatherTransitionToGlobal(static_cast<u32>(g_BankWeatherTransitionTime) * 1000); 
	else // no network update needed
		g_weather.ForceTypeNow(-1, CWeather::FT_SendNetworkUpdate);
}

static void NetworkBank_OverrideWeather()
{
	s32 nWeatherTypeIndex = g_weather.GetTypeIndex(g_BankWeatherTypeName);
	if(gnetVerifyf((nWeatherTypeIndex >= 0 && nWeatherTypeIndex < g_weather.GetNumTypes()), "NetworkBank_OverrideWeather :: Invalid weather type: %s!", g_BankWeatherTypeName))
	{
		gnetDebug1("NetworkBank_OverrideWeather :: Calling OverrideType for %s", g_BankWeatherTypeName);
		g_weather.OverrideType(nWeatherTypeIndex);
	}
}

static void NetworkBank_ClearOverrideWeather()
{
	gnetDebug1("NetworkBank_ClearOverrideWeather :: Calling StopOverriding");
	g_weather.StopOverriding();
}

static void NetworkBank_StopNetworkTimePausable()
{
    CNetwork::StopNetworkTimePausable();
}

static void NetworkBank_RestartNetworkTimePausable()
{
    CNetwork::RestartNetworkTimePausable();
}

static void NetworkBank_TogglePlayerControlAndVisibility()
{
    static bool toggle = true;

    CPed *playerPed = FindPlayerPed();

    if(playerPed)
    {
        if(toggle)
        {
            playerPed->GetPlayerInfo()->DisableControlsScript();
            playerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false);
        }
        else
        {
            playerPed->GetPlayerInfo()->EnableControlsScript();
            playerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, true);
        }

        toggle = !toggle;
    }
}

static void NetworkBank_CacheLocalPlayerHeadBlendData()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	if (pPlayerPed)
	{
		CPedHeadBlendData *pedHeadBlendData = pPlayerPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

		if (pedHeadBlendData)
		{
			NetworkInterface::CachePlayerHeadBlendData(NetworkInterface::GetLocalPhysicalPlayerIndex(), *pedHeadBlendData);
		}
	}
}

static void NetworkBank_ApplyCachedPlayerHeadBlendData()
{
	const CNetGamePlayer* target = NetworkBank_GetChosenNetworkPlayer();

	if (target && target->GetPlayerPed())
	{
		CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;

		if (pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			NetworkInterface::ApplyCachedPlayerHeadBlendData(*((CPed*)pFocusEntity), target->GetPhysicalPlayerIndex());
		}
		else
		{
			NetworkInterface::ApplyCachedPlayerHeadBlendData(*target->GetPlayerPed(), target->GetPhysicalPlayerIndex());
		}
	}
}

static void NetworkBank_ToggleFocusEntityAsGhost()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

    if (pFocusEntity && pFocusEntity->GetIsPhysical())
	{
		CPhysical* pPhysical = (CPhysical*)pFocusEntity;

		if (pPhysical->GetNetworkObject())
		{
			CNetObjPhysical* pNetObj = static_cast<CNetObjPhysical*>(pPhysical->GetNetworkObject());

			pNetObj->SetAsGhost(!pNetObj->IsGhost() BANK_ONLY(, SPG_RAG));
		}
	}
}

static void NetworkBank_ToggleInvertGhosting()
{
	CNetObjPhysical::SetInvertGhosting(!CNetObjPhysical::GetInvertGhosting());
}

//static void NetworkBank_SetPedDefensiveArea()
//{
//    CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
//    if(pFocusEntity)
//    {
//        // only pass ownership if we own the focus ped and a network game is in progress
//        if(pFocusEntity && NetworkInterface::IsGameInProgress())
//        {
//            if(pFocusEntity->GetIsTypePed())
//            {
//                CPed *ped = SafeCast(CPed, pFocusEntity);
//
//                if(ped && ped->IsNetworkClone())
//                {
//                    static bool setDefensiveArea = true;
//
//                    CTaskSetPedDefensiveArea *task = 0;
//
//                    if(setDefensiveArea)
//                    {
//                        task = rage_new CTaskSetPedDefensiveArea(VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()), 100.0f);
//                    }
//                    else
//                    {
//                        task = rage_new CTaskSetPedDefensiveArea();
//                    }
//
//                    NetworkInterface::GivePedScriptedTask(ped, task, "SET_PED_DEFENSIVE_AREA");
//
//                    setDefensiveArea = !setDefensiveArea;
//                }
//            }
//        }
//    }
//}

static void NetworkBank_LeavePedBehindAndWarp()
{
    CPed *playerPed = FindPlayerPed();

    if(playerPed)
    {
        CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject());

        if(netObjPlayer && netObjPlayer->CanRespawnLocalPlayerPed())
        {
            Vector3 pos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            const bool triggerNetworkRespawnEvent = netObjPlayer->RespawnPlayerPed(pos, false, NETWORK_INVALID_OBJECT_ID);

            pos += (VEC3V_TO_VECTOR3(playerPed->GetTransform().GetB()) * 30.0f);

			if (triggerNetworkRespawnEvent)
			{
				CRespawnPlayerPedEvent::Trigger(pos, netObjPlayer->GetRespawnNetObjId(), NetworkInterface::GetNetworkTime(), false, false);
			}

            netObjPlayer->GetPlayerPed()->Teleport(pos, playerPed->GetCurrentHeading());
        }
    }
}

static void NetworkBank_LeavePedBehindAndEnterCutscene()
{
    CPed *playerPed = FindPlayerPed();

    if(playerPed)
    {
        CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject());

        if(netObjPlayer && netObjPlayer->CanRespawnLocalPlayerPed())
        {
            Vector3 pos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            const bool triggerNetworkRespawnEvent = netObjPlayer->RespawnPlayerPed(pos, false, NETWORK_INVALID_OBJECT_ID);

			if (triggerNetworkRespawnEvent)
			{
				CRespawnPlayerPedEvent::Trigger(pos, netObjPlayer->GetRespawnNetObjId(), NetworkInterface::GetNetworkTime(), false, true);
			}

            ms_bInMPCutscene = true;
            NetworkInterface::SetInMPCutscene(ms_bInMPCutscene, true);
        }
    }
}

static void NetworkBank_KillFocusPed()
{
    CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
    if(pFocusEntity)
    {
        // only pass ownership if we own the focus ped and a network game is in progress
        if(pFocusEntity && !pFocusEntity->IsNetworkClone() && NetworkInterface::IsGameInProgress())
        {
            if(pFocusEntity->GetIsTypePed())
            {
                CPed *ped = SafeCast(CPed, pFocusEntity);

                if(ped)
                {
                    ped->SetHealth(0.0f);
                }
            }
        }
    }
}

static void NetworkBank_GiveFocusPedLocalCloneTask()
{
	CDynamicEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0) ? SafeCast(CDynamicEntity, CDebugScene::FocusEntities_Get(0)) : 0;
	if(pFocusEntity)
	{
		if (pFocusEntity && pFocusEntity->IsNetworkClone())
		{
			CPed *ped = SafeCast(CPed, pFocusEntity);

			if (ped)
			{
				ped->GetPedIntelligence()->AddLocalCloneTask(rage_new CTaskHandsUp(1000000),  PED_TASK_PRIORITY_PRIMARY);
			}
		}
	}
}

//static void NetworkBank_ForcePlayerAppearanceSync()
//{
//    CPed *playerPed = FindPlayerPed();
//
//    if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetSyncTree())
//    {
//        CPlayerSyncTree *playerSyncTree = (CPlayerSyncTree *)playerPed->GetNetworkObject()->GetSyncTree();
//        playerSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, playerPed->GetNetworkObject()->GetActivationFlags(), playerPed->GetNetworkObject(), *playerSyncTree->GetPlayerAppearanceNode());
//    }
//}
//
//static void NetworkBank_ForcePlayerSyncIfDirty()
//{
//    CPed *playerPed = FindPlayerPed();
//
//    if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetSyncTree())
//    {
//        CPlayerSyncTree *playerSyncTree = (CPlayerSyncTree *)playerPed->GetNetworkObject()->GetSyncTree();
//        playerSyncTree->DirtyNode(playerPed->GetNetworkObject(), *playerSyncTree->GetPlayerAppearanceNode());
//
//        ((CNetObjPed *)(playerPed->GetNetworkObject()))->ForceResendAllData();
//    }
//}
//
//static void NetworkBank_DirtyAllNodes()
//{
//    CPed *playerPed = FindPlayerPed();
//
//    if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetSyncTree())
//    {
//        CPlayerSyncTree *playerSyncTree = (CPlayerSyncTree *)playerPed->GetNetworkObject()->GetSyncTree();
//        DataNodeFlags nodeFlags = ~0u;
//        playerSyncTree->DirtyNodes(playerPed->GetNetworkObject(), nodeFlags);
//    }
//}
//
//static void NetworkBank_SetPlayerRelationshipGroupToPlayer()
//{
//    CPed *localPlayerPed = FindPlayerPed();
//
//    if(localPlayerPed)
//    {
//        localPlayerPed->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pPlayerGroup);
//    }
//}

//static void NetworkBank_TestStarvationCode()
//{
//    CPed *playerPed = FindFollowPed();
//
//    if(playerPed && playerPed->GetNetworkObject())
//    {
//        const PhysicalPlayerIndex player1 = 30;
//        const PhysicalPlayerIndex player2 = 31;
//
//        netObject *object = playerPed->GetNetworkObject();
//
//        netObject::SetStarvationThreshold(1);
//
//        // test basic starvation checks before increasing any counts
//        bool isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        // test increasing the starvation count
//        object->IncreaseStarvationCount(player1);
//        object->IncreaseStarvationCount(player2);
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        // test changing the threshold
//        netObject::SetStarvationThreshold(5);
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        object->IncreaseStarvationCount(player1);
//        object->IncreaseStarvationCount(player2);
//        object->IncreaseStarvationCount(player1);
//        object->IncreaseStarvationCount(player2);
//        object->IncreaseStarvationCount(player1);
//        object->IncreaseStarvationCount(player2);
//        object->IncreaseStarvationCount(player1);
//        object->IncreaseStarvationCount(player2);
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        object->ResetStarvationCount(player1);
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        object->ResetStarvationCount(player2);
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//
//        // test overflowing the starvation count
//        for(unsigned index = 0; index <= 16; index++)
//        {
//            object->IncreaseStarvationCount(player1);
//            object->IncreaseStarvationCount(player2);
//        }
//
//        isStarving = object->IsStarving();
//        Displayf("Object is %sstarving\n", isStarving ? "" : "not ");
//
//        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
//        {
//            isStarving = object->IsStarving(index);
//            Displayf("Object is %sstarving on player %d\n", isStarving ? "" : "not ", index);
//        }
//    }
//}

//static void NetworkBank_VEH_TO_TRAILER()
//{
//    CEntity *firstEntity  = CDebugScene::FocusEntities_Get(0);
//    CEntity *secondEntity = CDebugScene::FocusEntities_Get(1);
//
//    if((firstEntity  && firstEntity->GetIsTypeVehicle()) &&
//       (secondEntity && secondEntity->GetIsTypeVehicle() && ((CVehicle *)secondEntity)->InheritsFromTrailer()))
//    {
//        CVehicle *parentVehicle = static_cast<CVehicle *>(CDebugScene::FocusEntities_Get(0));
//        CTrailer *trailer       = static_cast<CTrailer *>(CDebugScene::FocusEntities_Get(1));
//
//        trailer->AttachToParentVehicle(parentVehicle, true);
//    }
//}
//
//static void NetworkBank_VEH_TO_VEH()
//{
//    CEntity *firstEntity  = CDebugScene::FocusEntities_Get(0);
//    CEntity *secondEntity = CDebugScene::FocusEntities_Get(1);
//
//    if((firstEntity  && firstEntity->GetIsTypeVehicle()) &&
//       (secondEntity && secondEntity->GetIsTypeVehicle() && ((CVehicle *)secondEntity)->InheritsFromTrailer()))
//    {
//        CVehicle *parentVehicle = static_cast<CVehicle *>(CDebugScene::FocusEntities_Get(0));
//        CTrailer *trailer       = static_cast<CTrailer *>(CDebugScene::FocusEntities_Get(1));
//
//        Quaternion quatRotate;
//		quatRotate.Identity();
//
//        Vector3 offset;
//        offset.Zero();
//
//		u16 nBasicAttachFlags = ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP;
//        int iOtherBoneIndex = trailer->GetBoneIndex(TRAILER_ATTACH);
//		trailer->AttachToPhysicalBasic(parentVehicle, iOtherBoneIndex, nBasicAttachFlags, &offset, &quatRotate);
//    }
//}
//
//static void NetworkBank_VEH_TO_VEH_PHYS()
//{
//    CEntity *firstEntity  = CDebugScene::FocusEntities_Get(0);
//    CEntity *secondEntity = CDebugScene::FocusEntities_Get(1);
//
//    if((firstEntity  && firstEntity->GetIsTypeVehicle()) &&
//       (secondEntity && secondEntity->GetIsTypeVehicle() && ((CVehicle *)secondEntity)->InheritsFromTrailer()))
//    {
//        CVehicle *parentVehicle = static_cast<CVehicle *>(CDebugScene::FocusEntities_Get(0));
//        CTrailer *trailer       = static_cast<CTrailer *>(CDebugScene::FocusEntities_Get(1));
//
//        Quaternion quatRotate;
//		quatRotate.Identity();
//
//        Vector3 offset, offset2;
//        offset.Zero();
//        offset2.Zero();
//
//		u32 nPhysicalAttachFlags = ATTACH_STATE_PHYSICAL|ATTACH_FLAG_POS_CONSTRAINT;
//
//		//if(bDoInitialWarp)
//			nPhysicalAttachFlags |= ATTACH_FLAG_INITIAL_WARP;
//
//        int iThisBoneIndex  = parentVehicle->GetBoneIndex(VEH_ATTACH);
//	    int iOtherBoneIndex = trailer->GetBoneIndex(TRAILER_ATTACH);
//
//		trailer->AttachToPhysicalUsingPhysics(parentVehicle, iThisBoneIndex, iOtherBoneIndex, nPhysicalAttachFlags, &offset, &quatRotate, &offset2, -1);
//    }
//}
//
//static void NetworkBank_DetachTrailer()
//{
//    //CEntity *firstEntity  = FocusEntities_Get(0);
//    CEntity *secondEntity = CDebugScene::FocusEntities_Get(1);
//
//    if(//(firstEntity  && firstEntity->GetIsTypeVehicle()) &&
//       (secondEntity && secondEntity->GetIsTypeVehicle() && ((CVehicle *)secondEntity)->InheritsFromTrailer()))
//    {
//        //CVehicle *parentVehicle = static_cast<CVehicle *>(FocusEntities_Get(0));
//        CTrailer *trailer       = static_cast<CTrailer *>(CDebugScene::FocusEntities_Get(1));
//
//        trailer->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
//    }
//}

//static void NetworkBank_AttachVehicleToTrailer()
//{
//    CTrailer *trailer = static_cast<CTrailer *>(CDebugScene::FocusEntities_Get(0));
//    CVehicle *vehicle = static_cast<CVehicle *>(CDebugScene::FocusEntities_Get(1));
//
//    if(trailer && vehicle)
//    {
//        Vector3 vecRotation(0.0f, 0.0f, 180.0f);
//        Vector3 vecEntity1Offset(VEC3_ZERO);
//        Vector3 vecEntity2Offset(0.0f,1.4f,-0.45f);
//
//        Quaternion quatRotate;
//        quatRotate.Identity();
//        if(vecRotation.IsNonZero())
//        {
//            CScriptEulers::QuaternionFromEulers(quatRotate, DtoR * vecRotation);
//        }
//
//        u32 nPhysicalAttachFlags = ATTACH_FLAG_POS_CONSTRAINT|ATTACH_FLAG_ACTIVATE_ON_DETACH;
//
//        nPhysicalAttachFlags |= ATTACH_STATE_PHYSICAL;
//        nPhysicalAttachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;
//        nPhysicalAttachFlags |= ATTACH_FLAG_INITIAL_WARP;
//
//        dev_float invMassScaleA = 1.0f;
//        dev_float invMassScaleB = 0.1f;
//
//        float fPhysicalStrength = -1;
//
//        vehicle->AttachToPhysicalUsingPhysics(trailer, 0, 0, nPhysicalAttachFlags, &vecEntity2Offset, &quatRotate, &vecEntity1Offset, fPhysicalStrength, false, invMassScaleA, invMassScaleB);
//    }
//}

static
void NetworkBank_TeamSwapPedWarp()
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if (AssertVerify(player) && AssertVerify(player->GetNetworkObject()) && AssertVerify(NetworkInterface::GetLocalPlayer()))
	{
		CNetObjPlayer* netObj = static_cast<CNetObjPlayer*>(player->GetNetworkObject());

		netObject* pedObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(netObj->GetRespawnNetObjId(), *NetworkInterface::GetLocalPlayer());
		if(AssertVerify(pedObject) 
			&& AssertVerify(pedObject->GetEntity()) 
			&& AssertVerify(pedObject->GetEntity()->GetIsDynamic())
			&& AssertVerify(pedObject->GetEntity()->GetIsTypePed()))
		{
			pedObject->GetEntity()->Teleport(VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()), player->GetCurrentHeading(), false, true, false, true, false, true);
		}
	}
}

static
void NetworkBank_TeamSwapPedVisible()
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if (AssertVerify(player) && AssertVerify(player->GetNetworkObject()) && AssertVerify(NetworkInterface::GetLocalPlayer()))
	{
		CNetObjPlayer* netObj = static_cast<CNetObjPlayer*>(player->GetNetworkObject());

		netObject* pedObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(netObj->GetRespawnNetObjId(), *NetworkInterface::GetLocalPlayer());
		if(AssertVerify(pedObject) 
			&& AssertVerify(pedObject->GetEntity()) 
			&& AssertVerify(pedObject->GetEntity()->GetIsDynamic())
			&& AssertVerify(pedObject->GetEntity()->GetIsTypePed()))
		{
			pedObject->GetEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG, !pedObject->GetEntity()->GetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG));
		}
	}
}

static
void NetworkBank_TeamSwapPedCollision()
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if (AssertVerify(player) && AssertVerify(player->GetNetworkObject()) && AssertVerify(NetworkInterface::GetLocalPlayer()))
	{
		CNetObjPlayer* netObj = static_cast<CNetObjPlayer*>(player->GetNetworkObject());

		netObject* pedObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(netObj->GetRespawnNetObjId(), *NetworkInterface::GetLocalPlayer());
		if(AssertVerify(pedObject) 
			&& AssertVerify(pedObject->GetEntity()) 
			&& AssertVerify(pedObject->GetEntity()->GetIsDynamic())
			&& AssertVerify(pedObject->GetEntity()->GetIsTypePed()))
		{
			if (pedObject->GetEntity()->IsCollisionEnabled())
			{
				pedObject->GetEntity()->DisableCollision();
			}
			else
			{
				pedObject->GetEntity()->EnableCollision();
			}
		}
	}
}

static
void NetworkBank_ApplyPlayerScarData()
{
	if (NetworkInterface::IsGameInProgress())
	{
		CPed* player = FindPlayerPed();

		if (player)
		{
			StatsInterface::ApplyPedScarData(player, StatsInterface::GetCurrentMultiplayerCharaterSlot());
		}
	}
}


static void NetworkBank_DumpWorldPosToTTY()
{
    Vector3 realPos;
    CNetObjProximityMigrateable::GetWorldCoords(gSectorX, gSectorY, gSectorZ, gSectorPos, realPos);
    gnetDebug2("World Position is %.2f, %.2f, %.2f", realPos.x, realPos.y, realPos.z);
}

static void NetworkBank_StartSoloTutorialSession()
{
    CPed *pPlayerPed = FindPlayerPed();

    if(pPlayerPed)
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(netObjPlayer)
        {
            netObjPlayer->StartSoloTutorial();
        }
    }
}

static void NetworkBank_StartGangTutorialSession()
{
    CPed *pPlayerPed = FindPlayerPed();

    if(pPlayerPed)
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(netObjPlayer)
        {
            netObjPlayer->StartGangTutorial(0, gMaxGangTutorialPlayers);
        }
    }
}

static void NetworkBank_EndTutorialSession()
{
    CPed *pPlayerPed = FindPlayerPed();

    if(pPlayerPed)
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(netObjPlayer)
        {
            netObjPlayer->EndTutorial();
        }
    }
}

static void NetworkBank_ControlPassInTutorial()
{
	CEntity* focusEnt = CDebugScene::FocusEntities_Get(0);
	if(focusEnt && focusEnt->GetIsPhysical())
	{
		netObject* netFocus = NetworkUtils::GetNetworkObjectFromEntity(focusEnt);
		if(netFocus)
		{
			SafeCast(CNetObjPhysical, netFocus)->SetPassControlInTutorial(s_PassControlInTutorial);
		}
	}
}

static void AddCameraDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug Cameras", false);
            ms_pBank->AddToggle("Render player cameras",&ms_netDebugDisplaySettings.m_renderPlayerCameras);
            ms_pBank->PushGroup("Follow Player", false);
                s_FollowPedCombo = ms_pBank->AddCombo("Target Ped", &s_FollowPedIndex, s_NumFollowPedTargets, (const char **)s_FollowPedTargets, NetworkBank_ChangeFollowPedTarget);
            ms_pBank->PopGroup();
			ms_pBank->AddButton("Follow Local Player", datCallback(NetworkBank_FollowLocalPlayer));
			ms_pBank->AddButton("Follow Focus Ped", datCallback(NetworkBank_FollowFocusPed));
			ms_pBank->PopGroup();
    }
}

static void AddCPUPerformanceWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug CPU Usage", false);
            ms_pBank->AddToggle("Display Basic Usage Info",&s_DisplayBasicCPUInfo);
            ms_pBank->AddToggle("Display Sync Tree Batching Info",&s_DisplaySyncTreeBatchingInfo);
            ms_pBank->AddToggle("Display Sync Tree Reads",&s_DisplaySyncTreeReads);
            ms_pBank->AddToggle("Display Sync Tree Writes",&s_DisplaySyncTreeWrites);
            ms_pBank->AddToggle("Display Player Location Info",&s_DisplayPlayerLocationInfo);
            ms_pBank->AddToggle("Display Ped Navmesh Sync Stats",&s_DisplayPedNavmeshSyncStats);
            ms_pBank->AddToggle("Display Expensive Function Call Stats",&s_DisplayExpensiveFunctionCallStats);
        ms_pBank->PopGroup();
    }
}

static void AddLoggingDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug Logging", false);
            ms_pBank->AddToggle("Display Logging Statistics", &s_DisplayLoggingStatistics);

            ms_pBank->PushGroup("Enable Log Files", false);
                for(unsigned index = 0; index < s_NumLogFiles; index++)
                {
                    ms_pBank->AddToggle(s_LogFiles[index], &s_EnableLogs[index], NetworkBank_ChangeLogFile);
                }
            ms_pBank->PopGroup();
        ms_pBank->PopGroup();
    }
}

#if __DEV
static void ClearDebugDraw()
{
	CPhysics::ms_debugDrawStore.ClearAll();
}
#endif // __DEV

static void AddPredictionDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug Prediction", false);
            ms_pBank->AddToggle("Display prediction info",&ms_netDebugDisplaySettings.m_displayPredictionInfo);
            ms_pBank->AddToggle("Display prediction focus on vector map",   &ms_DisplayPredictionFocusOnVectorMap);
            ms_pBank->AddToggle("Display orientation only on vector map",   &ms_DisplayOrientationOnlyOnVectorMap);
            ms_pBank->AddToggle("Enable prediction logging",                &ms_EnablePredictionLogging);
            ms_pBank->AddToggle("Restrict prediction logging to selection", &ms_RestrictPredictionLoggingToSelection);
            ms_pBank->AddCombo("Restriction Selection",                     &ms_PredictionLoggingSelection,
                                                                             ms_NumPredictionLoggingSelections,
                                                                             (const char **)ms_PredictionLoggingSelections);

#if __DEV
			ms_pBank->AddToggle("Display network blend update position",	&netBlenderLinInterp::ms_bDebugNetworkBlend_UpdatePosition);
			ms_pBank->AddToggle("Display desired velocities",				&CNetObjPhysical::ms_bDebugDesiredVelocity);
			ms_pBank->AddButton("Clear debug draw",datCallback(ClearDebugDraw));
#endif // __DEV
#if __BANK
            ms_pBank->AddToggle("Override blender for focus entity",		&CNetObjPhysical::ms_bOverrideBlenderForFocusEntity);
#endif // __BANK

            ms_pBank->AddButton("Follow Max BB Checks Entity And Pause",    NetworkBank_FollowMaxBBChecksEntityAndPause);
            ms_pBank->AddButton("Follow Max No Collision Entity And Pause", NetworkBank_FollowMaxNoCollisionEntityAndPause);
            ms_pBank->AddButton("Follow Next Prediction Pop And Pause",     NetworkBank_FollowNextPredictionPopAndPause);
        ms_pBank->PopGroup();
    }
}

static void NetworkBank_DumpConnectionManagerStats()
{
	CNetwork::GetConnectionManager().DumpStats();
}

static void NetworkBank_ForceTimeoutAllConnections()
{
	CNetwork::GetConnectionManager().ForceTimeOutAllConnections();
}

static void NetworkBank_ForceConnectionOutOfMemory()
{
	CNetwork::GetConnectionManager().ForceOutOfMemory();
}

static void NetworkBank_ForceCloseConnectionlessEndpoints()
{
	CNetwork::GetConnectionManager().ForceCloseConnectionlessEndpoints();
}

static void NetworkBank_ForceStallLeadingToNetworkTimeouts()
{
	gnetDebug1("NetworkBank_ForceStallLeadingToNetworkTimeouts");

	const unsigned nNetworkTimeout = CNetwork::GetTimeoutTime();
	
	// this won't work unless we have a valid timeout
	if(nNetworkTimeout == 0)
		return;

	// stall for the timeout plus a second to be sure
	const unsigned STALL_TIME = nNetworkTimeout + 1000;

	// grab the current time
	const unsigned nStartTime = sysTimer::GetSystemMsTime();
	while((sysTimer::GetSystemMsTime() - STALL_TIME) < nStartTime)
	{
		// spin around here long enough to cause our network connections to sever
	}
}

static void AddConnectionWidgets()
{
	if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Connection", false);
		ms_pBank->AddButton("Dump Connection Mgr Stats", NetworkBank_DumpConnectionManagerStats);
		ms_pBank->AddButton("Force Timeout All Connections", NetworkBank_ForceTimeoutAllConnections);
		ms_pBank->AddButton("Force Out of Memory", NetworkBank_ForceConnectionOutOfMemory);
		ms_pBank->AddButton("Force Close All Connectionless Endpoints", NetworkBank_ForceCloseConnectionlessEndpoints);
		ms_pBank->AddButton("Force Stall Leading to Network Timeouts", NetworkBank_ForceStallLeadingToNetworkTimeouts);
		ms_pBank->PopGroup();
	}
}

static void AddDebugDisplayWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug Displays", false);
			ms_pBank->AddToggle("Display net info",&ms_netDebugDisplaySettings.m_displayNetInfo);
			ms_pBank->AddToggle("Display basic net info",&ms_netDebugDisplaySettings.m_displayBasicInfo);
            ms_pBank->AddToggle("Display net memory usage",&ms_netDebugDisplaySettings.m_displayNetMemoryUsage);
			ms_pBank->AddToggle("Display object info",&ms_netDebugDisplaySettings.m_displayObjectInfo);
            ms_pBank->AddToggle("Do object info room checks",&ms_netDebugDisplaySettings.m_DoObjectInfoRoomChecks);
			ms_pBank->AddToggle("Display object script info",&ms_netDebugDisplaySettings.m_displayObjectScriptInfo);
            ms_pBank->AddToggle("Display player info debug info",&ms_netDebugDisplaySettings.m_displayPlayerInfoDebug);
			ms_pBank->AddToggle("Display host broadcast data",&ms_netDebugDisplaySettings.m_displayHostBroadcastDataAllocations);
			ms_pBank->AddToggle("Display player broadcast data",&ms_netDebugDisplaySettings.m_displayPlayerBroadcastDataAllocations);
			ms_pBank->AddToggle("Display portable pickup info",&ms_netDebugDisplaySettings.m_displayPortablePickupInfo);
			ms_pBank->AddToggle("Display dispatch info",&ms_netDebugDisplaySettings.m_displayDispatchInfo);
			ms_pBank->AddToggle("Display object population reservation info",&ms_netDebugDisplaySettings.m_displayObjectPopulationReservationInfo);
			ms_pBank->AddToggle("Display Entity damage debug info",&s_DebugEntityDamage);
			ms_pBank->AddToggle("Display visibility info",&ms_netDebugDisplaySettings.m_displayVisibilityInfo);
			ms_pBank->AddToggle("Display connection metrics",&ms_netDebugDisplaySettings.m_displayConnectionMetrics);
			ms_pBank->AddToggle("Display ghost info",&ms_netDebugDisplaySettings.m_displayGhostInfo);
			ms_pBank->AddToggle("Display Challenge FlyableAreas",&ms_netDebugDisplaySettings.m_displayChallengeFlyableAreas);
			ms_pBank->PushGroup("Display Targets", false);
			    ms_pBank->AddToggle("Objects",&ms_netDebugDisplaySettings.m_displayTargets.m_displayObjects);
			    ms_pBank->AddToggle("Peds",&ms_netDebugDisplaySettings.m_displayTargets.m_displayPeds);
			    ms_pBank->AddToggle("Players",&ms_netDebugDisplaySettings.m_displayTargets.m_displayPlayers);
			    ms_pBank->AddToggle("Vehicles",&ms_netDebugDisplaySettings.m_displayTargets.m_displayVehicles);
                ms_pBank->AddToggle("Trailers",&ms_netDebugDisplaySettings.m_displayTargets.m_displayTrailers);
            ms_pBank->PopGroup();
            ms_pBank->PushGroup("Display Information", false);
				ms_pBank->AddToggle("Position",&ms_netDebugDisplaySettings.m_displayInformation.m_displayPosition);
                ms_pBank->AddToggle("Velocity",&ms_netDebugDisplaySettings.m_displayInformation.m_displayVelocity);
                ms_pBank->AddToggle("Angular Velocity",&ms_netDebugDisplaySettings.m_displayInformation.m_displayAngVelocity);
                ms_pBank->AddToggle("Orientation",&ms_netDebugDisplaySettings.m_displayInformation.m_displayOrientation);
                ms_pBank->AddToggle("Prediction Info",&ms_netDebugDisplaySettings.m_displayInformation.m_displayPredictionInfo);
                ms_pBank->AddToggle("Damage Trackers",&ms_netDebugDisplaySettings.m_displayInformation.m_displayDamageTrackers);
                ms_pBank->AddToggle("Update Rate",&ms_netDebugDisplaySettings.m_displayInformation.m_displayUpdateRate);
				ms_pBank->AddToggle("Vehicle",&ms_netDebugDisplaySettings.m_displayInformation.m_displayVehicle);
				ms_pBank->AddToggle("Vehicle Damage",&ms_netDebugDisplaySettings.m_displayInformation.m_displayVehicleDamage);
				ms_pBank->AddToggle("Ped ragdoll",&ms_netDebugDisplaySettings.m_displayInformation.m_displayPedRagdoll);
                ms_pBank->AddToggle("Estimated update level",&ms_netDebugDisplaySettings.m_displayInformation.m_displayEstimatedUpdateLevel);
				ms_pBank->AddToggle("Pickup Regeneration",&ms_netDebugDisplaySettings.m_displayInformation.m_displayPickupRegeneration);
                ms_pBank->AddToggle("Collision timer",&ms_netDebugDisplaySettings.m_displayInformation.m_displayCollisionTimer);
                
				ms_pBank->AddToggle("Interior Info",&ms_netDebugDisplaySettings.m_displayInformation.m_displayInteriorInfo);
                ms_pBank->AddToggle("Fixed by network",&ms_netDebugDisplaySettings.m_displayInformation.m_displayFixedByNetwork);
                ms_pBank->AddToggle("Can pass control",&ms_netDebugDisplaySettings.m_displayInformation.m_displayCanPassControl);
				ms_pBank->AddToggle("Can accept control",&ms_netDebugDisplaySettings.m_displayInformation.m_displayCanAcceptControl);
				ms_pBank->AddToggle("Stealth noise",&ms_netDebugDisplaySettings.m_displayInformation.m_displayStealthNoise);
                s_UpdateRateCombo = ms_pBank->AddCombo("Relative Player", &ms_netDebugDisplaySettings.m_displayInformation.m_updateRatePlayer, s_NumFollowPedTargets, (const char **)s_FollowPedTargets, NullCB);
           ms_pBank->PopGroup();
        ms_pBank->PopGroup();
    }
}

static void AddUpdateRateDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        ms_pBank->PushGroup("Debug update rate", false);
		    ms_pBank->AddSlider("Player update rate",   &CNetObjPlayer::ms_updateRateOverride, -1, 4, 1);
            ms_pBank->AddSlider("Ped update rate",      &CNetObjPed::ms_updateRateOverride, -1, 4, 1);
			ms_pBank->AddSlider("Vehicle update rate",  &CNetObjVehicle::ms_updateRateOverride, -1, 4, 1);
			ms_pBank->AddSlider("Object update rate",   &CNetObjObject::ms_updateRateOverride, -1, 4, 1);
		ms_pBank->PopGroup();
    }
}

static void AddMiscDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
		ms_pBank->AddButton("Pass Control Of Focus Entity", datCallback(NetworkBank_PassControlOfFocusEntity));
		ms_pBank->AddButton("Request Control Of Focus Entity", datCallback(NetworkBank_RequestControlOfFocusEntity));
		ms_pBank->AddButton("Kill Focus Ped", datCallback(NetworkBank_KillFocusPed));
		ms_pBank->AddButton("Give Focus Ped Local Clone Task", datCallback(NetworkBank_GiveFocusPedLocalCloneTask));
        ms_pBank->AddToggle("Override Spawn Point",&ms_bOverrideSpawnPoint);
		ms_pBank->AddButton("Create Barrel", datCallback(NetworkBank_CreateBarrel));
		ms_pBank->AddButton("Create Fire", datCallback(NetworkBank_CreateFire));
		ms_pBank->AddToggle("Set in MP cutscene", &ms_bInMPCutscene, datCallback(NetworkBank_InMPCutscene));
		ms_pBank->AddToggle("Fake mocap cutscene", &ms_bInMocapCutscene);
		ms_pBank->AddSlider("Model To Dump", &s_ModelToDump, 0, 100000, 1);
		ms_pBank->AddButton("Dump Network Objects Using Model", datCallback(NetworkBank_DumpNetworkObjectsUsingModel));
        ms_pBank->AddButton("Dump Inappropriate vehicles to TTY", datCallback(DumpVehiclesUsingInappropriateModelsToTTY));
		ms_pBank->AddToggle("Render Network status Bars", &ms_bRenderNetworkStatusBars);
		ms_pBank->AddToggle("Debug Ped Missing damage Events",&s_DebugPedMissingDamageEvents);
		ms_pBank->AddToggle("Log local visibility override",&CNetObjEntity::ms_logLocalVisibilityOverride);
		ms_pBank->AddButton("Log Network Player Look Ats", datCallback(NetworkBank_LogLookAts));
		ms_pBank->AddButton("Toggle Conceal Focus Player", datCallback(NetworkBank_ToggleConcealFocusPlayer));
		ms_pBank->AddButton("Toggle Conceal Focus Entity", datCallback(NetworkBank_ToggleConcealFocusEntity));
		ms_pBank->AddButton("Remove All Sticky Bombs From Player", datCallback(NetworkBank_RemoveAllStickyBombsFromPlayer));
		ms_pBank->AddButton("Toggle Player Control and Visibility", datCallback(NetworkBank_TogglePlayerControlAndVisibility));
		ms_pBank->AddButton("Cache local player head blend data", datCallback(NetworkBank_CacheLocalPlayerHeadBlendData));
		ms_pBank->AddButton("Apply cached player head blend data", datCallback(NetworkBank_ApplyCachedPlayerHeadBlendData));
		ms_pBank->AddButton("Toggle Focus Entity As Ghost", datCallback(NetworkBank_ToggleFocusEntityAsGhost));
		ms_pBank->AddButton("Toggle Invert Ghosting", datCallback(NetworkBank_ToggleInvertGhosting));
	}
}

void AddAlphaFadeWidgets()
{
	if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug alpha fade", false);
			ms_pBank->AddButton("Fade Out Focus Entity", datCallback(NetworkBank_FadeOutFocusEntity));
			ms_pBank->AddButton("Alpha Ramp In Focus Entity", datCallback(NetworkBank_AlphaRampInFocusEntity));
			ms_pBank->AddButton("Alpha Ramp Out Focus Entity", datCallback(NetworkBank_AlphaRampOutFocusEntity));
			ms_pBank->AddButton("Reset Alpha On Focus Entity", datCallback(NetworkBank_ResetAlphaOnFocusEntity));
		ms_pBank->PopGroup();
	}
}

static void NetworkBank_SetupRosPrivilege()
{
	for (int i=0; i<RLROS_NUM_PRIVILEGEID; i++)
		rlRos::SetPrivilege(NetworkInterface::GetLocalGamerIndex(), (rlRosPrivilegeId)i, s_RosPrivilegesTypes[i]);
}

static void AddSessionWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
		s_NumKickPlayers = 0;

        ms_pBank->PushGroup("Session", false);

			ms_pBank->PushGroup("Setup Ros Privileges", false);
			for (u32 i=0; i<RLROS_NUM_PRIVILEGEID; i++)
			{
				ms_pBank->AddToggle(s_RosPrivileges[i], &s_RosPrivilegesTypes[i], NetworkBank_SetupRosPrivilege);
			}
			ms_pBank->PopGroup();

			ms_pBank->AddButton("Spew Posix Time",              datCallback(NetworkBank_SpewPosixTime));
			ms_pBank->AddButton("Set/Reset Notimeouts Flag",    datCallback(NetworkBank_ResetNoTimeouts));
			ms_pBank->AddToggle("Uplink Disconnection (not as good as the original)", &s_UplinkDisconnection, datCallback(NetworkBank_UplinkDisconnection));
		    ms_pBank->AddButton("Start test coop script",       datCallback(NetworkBank_StartTestCoopScript));
			ms_pBank->AddButton("Bail",							datCallback(NetworkBank_Bail));
			ms_pBank->AddButton("Access Alert",					datCallback(NetworkBank_AccessAlert));
	
			ms_pBank->PushGroup("Select Bail Reason", false);
			{
				const auto list = ms_pBank->AddList("Bail Reasons List");

				bkList::ClickItemFuncType bailReasonClickCB;
				bailReasonClickCB.Reset<&NetworkBank_SetBailReason>();

				list->SetSingleClickItemFunc(bailReasonClickCB);

				list->AddColumnHeader(0, "Value", bkList::ItemType::INT);
				list->AddColumnHeader(1, "Name", bkList::ItemType::STRING);

				for (int bailReason = 0; bailReason < eBailReason::BAIL_NUM; ++bailReason)
				{
					list->AddItem(bailReason, 0, bailReason);
					list->AddItem(bailReason, 1, NetworkUtils::GetBailReasonAsString((eBailReason)bailReason));
				}
			}
			ms_pBank->PopGroup();
			ms_pBank->PushGroup("Select Bail Context", false);
			{
				const auto list = ms_pBank->AddList("Bail Context List");

				bkList::ClickItemFuncType bailContextClickCB;
				bailContextClickCB.Reset<&NetworkBank_SetBailErrorCode>();

				list->SetSingleClickItemFunc(bailContextClickCB);

				list->AddColumnHeader(0, "Value", bkList::ItemType::INT);
				list->AddColumnHeader(1, "Name", bkList::ItemType::STRING);

				for (int bailErrorCode = 0; bailErrorCode < eBailContext::BAIL_CTX_COUNT; ++bailErrorCode)
				{
					const char* bailContextString = NetworkUtils::GetBailErrorCodeAsString(bailErrorCode);

					if (bailContextString[0] != '\0')
					{
						list->AddItem(bailErrorCode, 0, bailErrorCode);
						list->AddItem(bailErrorCode, 1, NetworkUtils::GetBailErrorCodeAsString(bailErrorCode));
					}
				}
			}
			ms_pBank->PopGroup();

			ms_pBank->AddButton("Clear Blacklist", datCallback(NetworkBank_ClearBlacklist));

			s_KickPlayerCombo = ms_pBank->AddCombo ("Player", &s_KickPlayerComboIndex, s_NumKickPlayers, (const char **)s_KickPlayers);
			ms_pBank->AddButton("Kick Player", datCallback(NetworkBank_KickPlayer));
			ms_pBank->AddButton("Print Local Player Id", datCallback(NetworkDebug::PrintLocalUserId));

			ms_pBank->AddButton("Show Average Client RTT", datCallback(NetworkBank_ShowAverageRTT));

            ms_pBank->AddButton("Export / Import GamerHandle", datCallback(NetworkBank_ExportImportGamerHandle));
       ms_pBank->PopGroup();   //Session
    }
}

static void AddTeamSwapDebugWidgets()
{
	if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug Team Swap", false);
		{
			ms_pBank->AddToggle("Fail Clone Respawn Network Event", &s_failCloneRespawnNetworkEvent);
			ms_pBank->AddToggle("Fail Local Respawn Network Event", &s_failRespawnNetworkEvent);
			ms_pBank->AddButton("Warp to Player",  datCallback(NetworkBank_TeamSwapPedWarp));
			ms_pBank->AddButton("Make Visible",  datCallback(NetworkBank_TeamSwapPedVisible));
			ms_pBank->AddButton("Set Collisions",  datCallback(NetworkBank_TeamSwapPedCollision));
			ms_pBank->AddButton("Apply player Scars",  datCallback(NetworkBank_ApplyPlayerScarData));
			ms_pBank->AddButton("Leave Ped Behind And Warp", datCallback(NetworkBank_LeavePedBehindAndWarp));
            ms_pBank->AddButton("Leave Ped Behind And Enter MP cutscene", datCallback(NetworkBank_LeavePedBehindAndEnterCutscene));
			ms_pBank->AddSlider("Player Fade out - Use timer only value", &s_bRemotePlayerFadeOutimer, 0, 500000000, 1);
		}
		ms_pBank->PopGroup();
	}
}

static void NetworkBank_CollectMetricTimebars()
{
#if RAGE_TIMEBARS
	NOTFINAL_ONLY( CNetworkDebugTelemetry::CollectMetricTimebars(); )
#endif
}

static void NetworkBank_AppendMetricFileMark()
{
	CNetworkTelemetry::AddMetricFileMark();
	NOTFINAL_ONLY( CNetworkDebugTelemetry::ResetMetricsDebugStartTime(); )
}

static void AddTutorialSessionDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug Tutorial Sessions", false);
		{
			ms_pBank->AddButton("Start Solo Tutorial Session",  datCallback(NetworkBank_StartSoloTutorialSession));
			ms_pBank->AddButton("Start Gang Tutorial Session",  datCallback(NetworkBank_StartGangTutorialSession));
			ms_pBank->AddButton("End Tutorial Session",  datCallback(NetworkBank_EndTutorialSession));
            ms_pBank->AddSlider("Max Players For Gang Tutorial", &gMaxGangTutorialPlayers, 1, MAX_NUM_PHYSICAL_PLAYERS, 1);
		
			ms_pBank->AddToggle("Pass Control In Tutorial", &s_PassControlInTutorial);
			ms_pBank->AddButton("Set Focus Entity Control Pass In Tutorial", datCallback(NetworkBank_ControlPassInTutorial));
		}
		ms_pBank->PopGroup();
	}
}

static void AddScriptWorldStateDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug Script World State", false);
		{
            NetworkScriptWorldStateTypes::AddDebugWidgets();
		}
		ms_pBank->PopGroup();
	}
}

static void AddSectorDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug Sector Positions", false);
		{
            ms_pBank->AddSlider("Sector X", &gSectorX, 0, 512, 1);
            ms_pBank->AddSlider("Sector Y", &gSectorY, 0, 512, 1);
            ms_pBank->AddSlider("Sector Z", &gSectorZ, 0, 512, 1);
            ms_pBank->AddSlider("Sector Pos X", &gSectorPos.x, 0.0f, WORLD_WIDTHOFSECTOR_NETWORK, 0.1f);
            ms_pBank->AddSlider("Sector Pos Y", &gSectorPos.y, 0.0f, WORLD_DEPTHOFSECTOR_NETWORK, 0.1f);
            ms_pBank->AddSlider("Sector Pos Z", &gSectorPos.z, 0.0f, WORLD_HEIGHTOFSECTOR_NETWORK, 0.1f);
            ms_pBank->AddButton("Dump World Pos To TTY", datCallback(NetworkBank_DumpWorldPosToTTY));
		}
		ms_pBank->PopGroup();
	}
}

static void AddDebugVehicleWidgets()
{
	if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pBank->PushGroup("Debug Vehicle Damage", false);
			CNetObjVehicle::AddDebugVehicleDamage(ms_pBank);
		ms_pBank->PopGroup();

		ms_pBank->PushGroup("Debug Vehicle Population", false);
			CNetObjVehicle::AddDebugVehiclePopulation(ms_pBank);
			ms_pBank->AddToggle("Add extra debug population", NetworkDebug::GetLogExtraDebugNetworkPopulation());
		ms_pBank->PopGroup();
	}
}

static void AddSyncTreeWidgets()
{
	if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
	{
		ms_pSyncTreeGroup = ms_pBank->PushGroup("Sync Trees", false);

		for (u32 i=0; i<NUM_NET_OBJ_TYPES; i++)
		{
			CProjectSyncTree* pSyncTree = NULL;

			switch (i)
			{
			case NET_OBJ_TYPE_AUTOMOBILE:
				pSyncTree = CNetObjAutomobile::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_BIKE:
				pSyncTree = CNetObjBike::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_BOAT:
				pSyncTree = CNetObjBoat::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_DOOR:
				pSyncTree = CNetObjDoor::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_HELI:
				pSyncTree = CNetObjHeli::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_OBJECT:
				pSyncTree = CNetObjObject::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_PED:
				pSyncTree = CNetObjPed::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_PICKUP:
				pSyncTree = CNetObjPickup::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_PICKUP_PLACEMENT:
				pSyncTree = CNetObjPickupPlacement::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_GLASS_PANE:
#if GLASS_PANE_SYNCING_ACTIVE
				pSyncTree = CNetObjGlassPane::GetStaticSyncTree();
				break;
#else
				continue;
#endif /* GLASS_PANE_SYNCING_ACTIVE */
			case NET_OBJ_TYPE_PLANE:
				pSyncTree = CNetObjPlane::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_SUBMARINE:
				pSyncTree = CNetObjSubmarine::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_PLAYER:
				pSyncTree = CNetObjPlayer::GetStaticSyncTree();
				break;
			case NET_OBJ_TYPE_TRAILER:
				//pSyncTree = CNetObjTrailer::GetStaticSyncTree();
				//break;
				continue;
			case NET_OBJ_TYPE_TRAIN:
				pSyncTree = CNetObjTrain::GetStaticSyncTree();
				break;
			case NUM_NET_OBJ_TYPES:
				break;
			}

			if (gnetVerify(pSyncTree))
			{
				pSyncTree->AddWidgets(ms_pBank);
			}
		}		

		ms_pBank->PopGroup();
	}
}

#if __DEV && 0		// Temporarily disabled, was getting errors from the compiler in non-unity PS3 Beta, because the symbols are unreferenced (button widgets disabled). /FF

static void* s_CxnMgrMemPtr = 0;
static unsigned s_CxnMgrMemUsed = 0;
static void* s_SnetMemPtr = 0;
static unsigned s_SnetMemUsed = 0;
static void* s_RlineMemPtr = 0;
static unsigned s_RlineMemUsed = 0;

static void  NetworkBank_UseSCxnMgrAllocator()
{
	if (!s_CxnMgrMemPtr)
	{
		sysMemAllocator* previousHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(CNetwork::GetSCxnMgrAllocator());

		s_CxnMgrMemUsed = CNetwork::GetSCxnMgrAllocator().GetMemoryAvailable();
		while (!(s_CxnMgrMemPtr = CNetwork::GetSCxnMgrAllocator().Allocate(s_CxnMgrMemUsed, 0)) && s_CxnMgrMemUsed > 100)
		{
			s_CxnMgrMemUsed -= 10;
		}

		if (s_CxnMgrMemPtr)
		{
			s_CxnMgrMemPtr = new(s_CxnMgrMemPtr) char[s_CxnMgrMemUsed];

			if (!s_CxnMgrMemPtr)
			{
				s_CxnMgrMemUsed = 0;
			}
			else
			{
				sysMemSet(s_CxnMgrMemPtr, 0, s_CxnMgrMemUsed);

				gnetWarning("SCxnMgr Memory:");
				gnetWarning("................... Allocate: \"%d\"", s_CxnMgrMemUsed);
				gnetWarning(".................. Available: \"%d\"", CNetwork::GetSCxnMgrAllocator().GetMemoryAvailable());
			}
		}
		else
		{
			s_CxnMgrMemUsed = 0;
		}

		sysMemAllocator::SetCurrent(*previousHeap);
	}
}

static void  NetworkBank_UseRlineAllocator()
{
	if (!s_RlineMemPtr)
	{
		sysMemAllocator* previousHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(*g_rlAllocator);

		s_RlineMemUsed = g_rlAllocator->GetMemoryAvailable();
		while (!(s_RlineMemPtr = g_rlAllocator->Allocate(s_RlineMemUsed, 0)) && s_RlineMemUsed > 100)
		{
			s_RlineMemUsed -= 10;
		}

		if (s_RlineMemPtr)
		{
			s_RlineMemPtr = new(s_RlineMemPtr) char[s_RlineMemUsed];

			if (!s_RlineMemPtr)
			{
				s_RlineMemUsed = 0;
			}
			else
			{
				sysMemSet(s_RlineMemPtr, 0, s_RlineMemUsed);

				gnetWarning("Rline Memory:");
				gnetWarning(".............. Allocate: \"%d\"", s_RlineMemUsed);
				gnetWarning("............. Available: \"%d\"", g_rlAllocator->GetMemoryAvailable());
			}
		}
		else
		{
			s_RlineMemUsed = 0;
		}

		sysMemAllocator::SetCurrent(*previousHeap);
	}
}

static void NetworkBank_FreeRageMemory()
{
	if(s_CxnMgrMemPtr)
	{
		gnetWarning("Connection Memory:");
		gnetWarning("................ Allocated: \"%d\"", s_CxnMgrMemUsed);

		sysMemAllocator* previousHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(CNetwork::GetCxnMgrAllocator());

		gnetAssert(CNetwork::GetCxnMgrAllocator().IsValidPointer(s_CxnMgrMemPtr));

		CNetwork::GetCxnMgrAllocator().Free(s_CxnMgrMemPtr);
		s_CxnMgrMemPtr = 0;
		s_CxnMgrMemUsed = 0;

		sysMemAllocator::SetCurrent(*previousHeap);

		gnetWarning("................ Available: \"%d\"", CNetwork::GetCxnMgrAllocator().GetMemoryAvailable());

	}

	if(s_SnetMemPtr)
	{
		gnetWarning("Snet Memory:");
		gnetWarning(".......... Allocated: \"%d\"", s_SnetMemUsed);

		sysMemAllocator* previousHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(CNetwork::GetSnetAllocator());

		gnetAssert(CNetwork::GetSnetAllocator().IsValidPointer(s_SnetMemPtr));

		CNetwork::GetSnetAllocator().Free(s_SnetMemPtr);
		s_SnetMemPtr = 0;
		s_SnetMemUsed = 0;

		sysMemAllocator::SetCurrent(*previousHeap);

		gnetWarning(".......... Available: \"%d\"", CNetwork::GetSnetAllocator().GetMemoryAvailable());
	}

	if(s_RlineMemPtr)
	{
		gnetWarning("Rline Memory:");
		gnetWarning("........... Allocated: \"%d\"", s_RlineMemUsed);

		sysMemAllocator* previousHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(*g_rlAllocator);

		gnetAssert(g_rlAllocator->IsValidPointer(s_RlineMemPtr));

		g_rlAllocator->Free(s_RlineMemPtr);
		s_RlineMemPtr = 0;
		s_RlineMemUsed = 0;

		sysMemAllocator::SetCurrent(*previousHeap);

		gnetWarning("........... Available: \"%d\"", g_rlAllocator->GetMemoryAvailable());
	}
}

#endif // __DEV

static void AddDebugWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
		ms_pDebugGroup = ms_pBank->PushGroup("Debug", false);

		    ms_pBank->AddToggle("Log Extra tty for Damage Events", &s_debugWeaponDamageEvent);

			ms_pBank->PushGroup("Debug Break Points", false);
				for (u32 i=0; i<NUM_BREAK_TYPES; i++)
				{
					ms_pBank->AddToggle(m_breakTypeNames[i], &m_breakTypes[i]);
				}
			ms_pBank->PopGroup();

			AddDebugDisplayWidgets();

			ms_pBank->PushGroup("Debug Game", false);
				AddCameraDebugWidgets();
				ms_pBank->PushGroup("Debug 1st person", false);
					ms_pBank->AddToggle("Use FP idle movement on clones", &CNetObjPlayer::ms_bUseFPIdleMovementOnClones);
				ms_pBank->PopGroup();
				ms_pBank->PushGroup("Debug bind poses", false);
					ms_pBank->AddToggle("Catch Bind Posing Clones", &ms_bCatchCloneBindPose);
					ms_pBank->AddToggle("Catch Bind Posing Owners", &ms_bCatchOwnerBindPose);
					ms_pBank->AddToggle("Catch Bind Posing Cops",	&ms_bCatchCopBindPose);
					ms_pBank->AddToggle("Catch Bind Posing Non Cops", &ms_bCatchNonCopBindPose);
				ms_pBank->PopGroup();
				ms_pBank->PushGroup("Debug clone tasks", false);
					ms_pBank->AddToggle("TTY log all state transitions / network requests", &ms_bDebugTaskLogging);
					ms_pBank->AddToggle("Force Focus Entity Clone Tasks Out Of Scope", &s_ForceCloneTasksOutOfScope);
                    ms_pBank->AddButton("Make Focus Ped Seek Cover", datCallback(NetworkBank_MakeFocusPedSeekCover));
					ms_pBank->AddToggle("Enable TTY logging of clone TaskMotionPed updates", &ms_bTaskMotionPedDebug);
				ms_pBank->PopGroup();
				ms_pBank->PushGroup("Debug Scripted Ped Tasks", false);
					ms_pBank->AddSeparator();
					ms_pBank->AddButton("Give Focus Scripted Ped Use Nearest Scenario To Pos", datCallback(NetworkBank_GiveFocusPedScriptedUseNearestScenarioToPos));
					ms_pBank->AddToggle("Use Nearest Scenario Warp", &s_UseNearestScenarioWarp);
					ms_pBank->AddSlider("Nearest Scenario Range", &s_UseNearestScenarioRange, 0.1f, 100.0f, 0.1f);
				ms_pBank->PopGroup();
				CNetworkDamageTracker::AddDebugWidgets();
				CFakePlayerNames::AddDebugWidgets();
				CNetworkObjectMgr::AddDebugWidgets();
				AddScriptWorldStateDebugWidgets();
                AddSectorDebugWidgets();
                CNetworkSynchronisedScenes::AddDebugWidgets();
				AddDebugVehicleWidgets();
                CNetworkWorldGridManager::AddDebugWidgets();
				AddUpdateRateDebugWidgets();
				AddTeamSwapDebugWidgets();
                AddTutorialSessionDebugWidgets();
				AddAlphaFadeWidgets();
				AddMiscDebugWidgets();
			ms_pBank->PopGroup();

			ms_pBank->PushGroup("Debug Live", false);
				CLiveManager::InitWidgets();
				CNetwork::GetVoice().InitWidgets();
				CLiveManager::GetNetworkClan().InitWidgets();
                
				if(CNetwork::IsNetworkSessionValid())
					CNetwork::GetNetworkSession().InitWidgets();

				if (CNetwork::IsNetworkPartyValid())
					CNetwork::GetNetworkParty().InitWidgets();

				CNetwork::GetNetworkVoiceSession().InitWidgets();

				CLiveManager::GetSocialClubMgr().InitWidgets(ms_pBank);

				ms_pBank->PushGroup("Debug Leaderboards", false);
					GAME_CONFIG_PARSER.Bank_InitWidgets( ms_pBank );
					ms_pBank->PushGroup("Debug Session", false);
						ms_pBank->AddButton("Print/Use Clan Group Id", datCallback(NetworkBank_UseMyClan));
						ms_pBank->AddSeparator("");
						CNetwork::GetLeaderboardMgr().InitWidgets(ms_pBank);
					ms_pBank->PopGroup();
				ms_pBank->PopGroup();
				
				ms_pBank->PushGroup("Debug Metrics", false);
					ms_pBank->AddButton("Collect Timebars Info", datCallback(NetworkBank_CollectMetricTimebars));
					ms_pBank->AddButton("Add Metric File debug Mark", datCallback(NetworkBank_AppendMetricFileMark));
				ms_pBank->PopGroup();

				ms_pBank->AddButton("Request Send Remote Log", datCallback(NetworkBank_SendRemoteLog));
			
			ms_pBank->PopGroup();

			ms_pBank->PushGroup("Debug System", false);

				////Uncomment to debug memory
				//ms_pBank->PushGroup("Rage Memory tests", false);
				//{
				//	ms_pBank->AddButton("Use all SCxnMgr Allocator Memory ", datCallback(NetworkBank_UseSCxnMgrAllocator));
				//	ms_pBank->AddButton("Use all Rline Allocator Memory ", datCallback(NetworkBank_UseRlineAllocator));
				//	ms_pBank->AddButton("Free up Memory ", datCallback(NetworkBank_FreeRageMemory));
				//}
				//ms_pBank->PopGroup();

				CNetworkBandwidthManager::AddDebugWidgets();
                if(ms_pBank)
                {
                    ms_pBank->AddToggle("Display Latency Stats", &s_DisplayLatencyStats);
                }                
                AddCPUPerformanceWidgets();
				AddLoggingDebugWidgets();
#if ENABLE_NETWORK_BOTS
				CNetworkBot::AddDebugWidgets();
#endif // ENABLE_NETWORK_BOTS
                BANK_ONLY(CNetworkDebugPlayback::AddDebugWidgets());
                AddPredictionDebugWidgets();
				CRoamingBubbleMgr::AddDebugWidgets();
				AddConnectionWidgets();

                ms_pBank->AddCombo("Network Access Code", &s_NetworkAccessComboIndex, COUNTOF(s_NetworkAccessCombo), s_NetworkAccessCombo);
				ms_pBank->AddButton("Apply Access Code", datCallback(NetworkBank_ApplyNetworkAccessCode));
				ms_pBank->AddToggle("Force ready for network access check", &s_BankForceReadyForAccessCheck);
                ms_pBank->AddToggle("Force not ready for network access check", &s_BankForceNotReadyForAccessCheck);

				s_ForceNetworkCompliance = PARAM_netCompliance.Get();
				ms_pBank->AddToggle("Enable compliance", &s_ForceNetworkCompliance);

			ms_pBank->PopGroup();

			ms_pBank->PushGroup("Debug Shop", false);
				NETWORK_SHOPPING_MGR.Bank_InitWidgets( ms_pBank );
			ms_pBank->PopGroup();

      ms_pBank->PopGroup();
    }
}

static void AddSettingsWidgets()
{
    if(gnetVerifyf(ms_pBank, "Unable to find network bank!"))
    {
        netObject::netScopeData& netScopeDataPlayer			 = CNetObjPlayer::GetStaticScopeData();
        netObject::netScopeData& netScopeDataPed			 = CNetObjPed::GetStaticScopeData();
        netObject::netScopeData& netScopeDataAuto			 = CNetObjVehicle::GetStaticScopeData();
        netObject::netScopeData& netScopeDataBoat			 = CNetObjBoat::GetStaticScopeData();
		netObject::netScopeData& netScopeDataHeli			 = CNetObjHeli::GetStaticScopeData();
		netObject::netScopeData& netScopeDataPlane			 = CNetObjPlane::GetStaticScopeData();
		netObject::netScopeData& netScopeDataTrain			 = CNetObjTrain::GetStaticScopeData();
	    netObject::netScopeData& netScopeDataObj			 = CNetObjObject::GetStaticScopeData();
	    netObject::netScopeData& netScopeDataPickup			 = CNetObjPickup::GetStaticScopeData();
	    netObject::netScopeData& netScopeDataPickupPlacement = CNetObjPickupPlacement::GetStaticScopeData();
		netObject::netScopeData& netScopeDataDoor			 = CNetObjDoor::GetStaticScopeData();

        ms_pBank->PushGroup("Settings", false);
            ms_pBank->AddToggle("Allow ownership change",&m_bAllowOwnershipChange);
            ms_pBank->AddToggle("Allow proximity ownership",&m_bAllowProximityOwnership);
            ms_pBank->AddToggle("Use player cameras",&m_bUsePlayerCameras);
            ms_pBank->AddToggle("Skip all sync messages",&m_bSkipAllSyncs);
		    ms_pBank->AddToggle("Ignore give control events", &m_bIgnoreGiveControlEvents);
		    ms_pBank->AddToggle("Ignore visibility checks", &m_bIgnoreVisibilityChecks);
			ms_pBank->AddToggle("Enable physics optimisations", &m_bEnablePhysicsOptimisations);
            ms_pBank->PushGroup("Sync ranges", false);
                ms_pBank->PushGroup("Player", false);
                    ms_pBank->AddSlider("Scope distance",&netScopeDataPlayer.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
                    ms_pBank->AddSlider("Distance near",&netScopeDataPlayer.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
                    ms_pBank->AddSlider("Distance far",&netScopeDataPlayer.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
                ms_pBank->PopGroup();
                ms_pBank->PushGroup("Ped", false);
                    ms_pBank->AddSlider("Scope distance",&netScopeDataPed.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
                    ms_pBank->AddSlider("Distance near",&netScopeDataPed.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
                    ms_pBank->AddSlider("Distance far",&netScopeDataPed.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
                ms_pBank->PopGroup();
                ms_pBank->PushGroup("Vehicle", false);
                    ms_pBank->AddSlider("Scope distance",&netScopeDataAuto.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
                    ms_pBank->AddSlider("Distance near",&netScopeDataAuto.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
                    ms_pBank->AddSlider("Distance far",&netScopeDataAuto.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
                ms_pBank->PopGroup();
                ms_pBank->PushGroup("Boat", false);
                    ms_pBank->AddSlider("Scope distance",&netScopeDataBoat.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
                    ms_pBank->AddSlider("Distance near",&netScopeDataBoat.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
                    ms_pBank->AddSlider("Distance far",&netScopeDataBoat.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
                ms_pBank->PopGroup();
				ms_pBank->PushGroup("Plane", false);
					ms_pBank->AddSlider("Scope distance",&netScopeDataPlane.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
					ms_pBank->AddSlider("Distance near",&netScopeDataPlane.m_syncDistanceNear, 0.0f, 1000.0f, 1.0f);
					ms_pBank->AddSlider("Distance far",&netScopeDataPlane.m_syncDistanceFar, 0.0f, 1000.0f, 1.0f);
				ms_pBank->PopGroup();
				ms_pBank->PushGroup("Heli", false);
					ms_pBank->AddSlider("Scope distance",&netScopeDataHeli.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
					ms_pBank->AddSlider("Distance near",&netScopeDataHeli.m_syncDistanceNear, 0.0f, 1000.0f, 1.0f);
					ms_pBank->AddSlider("Distance far",&netScopeDataHeli.m_syncDistanceFar, 0.0f, 1000.0f, 1.0f);
				ms_pBank->PopGroup();
				ms_pBank->PushGroup("Train", false);
				ms_pBank->AddSlider("Scope distance",&netScopeDataTrain.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
				ms_pBank->AddSlider("Distance near",&netScopeDataTrain.m_syncDistanceNear, 0.0f, 1000.0f, 1.0f);
				ms_pBank->AddSlider("Distance far",&netScopeDataTrain.m_syncDistanceFar, 0.0f, 1000.0f, 1.0f);
				ms_pBank->PopGroup();
                ms_pBank->PushGroup("Object", false);
                    ms_pBank->AddSlider("Scope distance",&netScopeDataObj.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
                    ms_pBank->AddSlider("Distance near",&netScopeDataObj.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
                    ms_pBank->AddSlider("Distance far",&netScopeDataObj.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
                 ms_pBank->PopGroup();
			     ms_pBank->PushGroup("Pickup", false);
				    ms_pBank->AddSlider("Scope distance",&netScopeDataPickup.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
				    ms_pBank->AddSlider("Distance near",&netScopeDataPickup.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
				    ms_pBank->AddSlider("Distance far",&netScopeDataPickup.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
			     ms_pBank->PopGroup();
			     ms_pBank->PushGroup("Pickup Placement", false);
				    ms_pBank->AddSlider("Scope distance",&netScopeDataPickupPlacement.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
				    ms_pBank->AddSlider("Distance near",&netScopeDataPickupPlacement.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
				     ms_pBank->AddSlider("Distance far",&netScopeDataPickupPlacement.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
			     ms_pBank->PopGroup();
				 ms_pBank->PushGroup("Door", false);
					 ms_pBank->AddSlider("Scope distance",&netScopeDataDoor.m_scopeDistance, 0.0f, 1000.0f, 1.0f);
					 ms_pBank->AddSlider("Distance near",&netScopeDataDoor.m_syncDistanceNear, 0.0f, 100.0f, 1.0f);
					 ms_pBank->AddSlider("Distance far",&netScopeDataDoor.m_syncDistanceFar, 0.0f, 100.0f, 1.0f);
				 ms_pBank->PopGroup();
            ms_pBank->PopGroup();
            ms_pBank->PushGroup("Update Rate Prediction Error Threshold Sqr", false);
                ms_pBank->PushGroup("Vehicle", false);
                    ms_pBank->PushGroup("Position", false);
                        ms_pBank->AddSlider("Very Low Update", &CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW],  0.0f, 100.0f, 0.1f);
                        ms_pBank->AddSlider("Low Update",      &CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW],       0.0f, 100.0f, 0.1f);
                        ms_pBank->AddSlider("Medium Update",   &CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM],    0.0f, 100.0f, 0.1f);
                        ms_pBank->AddSlider("High Update",     &CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH],      0.0f, 100.0f, 0.1f);
                        ms_pBank->AddSlider("Very High Update",&CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH], 0.0f, 100.0f, 0.1f);
                    ms_pBank->PopGroup();
                ms_pBank->PopGroup();
            ms_pBank->PopGroup();
            SetupBlendingWidgets();
            ms_pBank->AddToggle("Sanity Check Heap",&sanityCheckHeapOnDoAllocate);
        ms_pBank->PopGroup();   //Settings
    }
}

void InitWidgets()
{
	if (PARAM_netdebugdisplay.Get())
		ms_netDebugDisplaySettings.m_displayObjectInfo = true;

	if (PARAM_netscriptdisplay.Get())
		ms_netDebugDisplaySettings.m_displayObjectScriptInfo = true;

	if (PARAM_netdebugdisplaytargetobjects.Get())
		ms_netDebugDisplaySettings.m_displayTargets.m_displayObjects = true;

	if (PARAM_netdebugdisplaytargetpeds.Get())
		ms_netDebugDisplaySettings.m_displayTargets.m_displayPeds = true;

	if (PARAM_netdebugdisplaytargetplayers.Get())
		ms_netDebugDisplaySettings.m_displayTargets.m_displayPlayers = true;

	if (PARAM_netdebugdisplaytargetvehicles.Get())
		ms_netDebugDisplaySettings.m_displayTargets.m_displayVehicles = true;

	if (PARAM_netdebugdisplaypredictioninfo.Get())
		ms_netDebugDisplaySettings.m_displayInformation.m_displayPredictionInfo = true;

	if (PARAM_netdebugdisplayvisibilityinfo.Get())
		ms_netDebugDisplaySettings.m_displayVisibilityInfo = true;

	if (PARAM_netdebugdisplayghostinfo.Get())
		ms_netDebugDisplaySettings.m_displayGhostInfo = true;

	if (PARAM_netdebugdesiredvelocity.Get())
		CNetObjPhysical::ms_bDebugDesiredVelocity = true;

	if (PARAM_debugTargetting.Get())
		CPedTargetting::DebugTargetting = true;

	if (PARAM_debugTargettingLOS.Get())
		CPedTargetting::DebugTargettingLos = true;

	if (PARAM_netdebugnetworkentityareaworldstate.Get())
		CNetworkEntityAreaWorldStateData::gs_bDrawEntityAreas = true;

	// create the network widget
	ms_pBank = BANKMGR.FindBank("Network");

	if (!ms_pBank)
	{
		ms_pBank = &BANKMGR.CreateBank("Network");
	}

	if(gnetVerifyf(ms_pBank, "Failed to create network bank"))
	{
		DOWNLOADABLETEXTUREMGRDEBUG.AddWidgets(*ms_pBank);

		AddSessionWidgets();
		AddSettingsWidgets();
		AddDebugWidgets();
/*
		ms_pBank->PushGroup("Misc", false);
		{
			ms_pBank->AddButton("Create Ped",CreatePedCB);
			ms_pBank->AddButton("Delete Ped",DeletePedCB);
		}
		ms_pBank->PopGroup();
*/
		s_PlayerCombo = ms_pBank->AddCombo ("Player", &s_PlayerComboIndex, s_NumPlayers, (const char **)s_Players);

		ms_pBank->AddButton("Warp To Player",  datCallback(NetworkBank_WarpToPlayer));

		ms_pBank->AddText("Focus entity id", &ms_FocusEntityId);
		ms_pBank->AddToggle("View focus entity", &ms_bViewFocusEntity, ViewFocusEntity);

		ms_pBank->AddButton("Flush log files", datCallback(NetworkBank_FlushLogFiles));

		ms_pBank->AddToggle("Enable Train in MP", &CTrain::ms_bEnableTrainsInMP);
		ms_pBank->AddButton("Launch Train debug script", datCallback(NetworkBank_LaunchTrainDebugScript));

		ms_pBank->AddText("Weather Type Name", g_BankWeatherTypeName, MAX_WEATHER_NAME, NullCB);
		ms_pBank->AddText("Weather Transition Time", &g_BankWeatherTransitionTime);
		ms_pBank->AddButton("Log Global Clock and Weather", datCallback(NetworkBank_LogGlobalClockAndWeatherTime));
		ms_pBank->AddButton("Apply Global Clock and Weather", datCallback(NetworkBank_ApplyGlobalClockAndWeatherTime));
		ms_pBank->AddButton("Stop Global Clock and Weather", datCallback(NetworkBank_StopApplyingGlobalClockAndWeatherTime));
		ms_pBank->AddButton("Force Weather", datCallback(NetworkBank_ForceWeather));
		ms_pBank->AddButton("Clear Force Weather", datCallback(NetworkBank_ClearForceWeather));
		ms_pBank->AddButton("Override Weather", datCallback(NetworkBank_OverrideWeather));
		ms_pBank->AddButton("Clear Override Weather", datCallback(NetworkBank_ClearOverrideWeather));

        ms_pBank->AddButton("Stop Network time pausable", datCallback(NetworkBank_StopNetworkTimePausable));
        ms_pBank->AddButton("Restart Network time pausable", datCallback(NetworkBank_RestartNetworkTimePausable));
		
		//ms_pBank->AddButton("Force player appearance data sync", datCallback(NetworkBank_ForcePlayerAppearanceSync));
        //ms_pBank->AddButton("Force player resend dirty", datCallback(NetworkBank_ForcePlayerSyncIfDirty));
        //ms_pBank->AddButton("Dirty all nodes", datCallback(NetworkBank_DirtyAllNodes));
        //ms_pBank->AddButton("Set Player Rel Group To Player", datCallback(NetworkBank_SetPlayerRelationshipGroupToPlayer));
        //ms_pBank->AddButton("Set Ped Defensive Area", datCallback(NetworkBank_SetPedDefensiveArea));
		//ms_pBank->AddButton("Test Starvation Code", datCallback(NetworkBank_TestStarvationCode));
		//ms_pBank->AddButton("VEH_TO_TRAILER test",  datCallback(NetworkBank_VEH_TO_TRAILER));
		//ms_pBank->AddButton("VEH_TO_VEH test",      datCallback(NetworkBank_VEH_TO_VEH));
		//ms_pBank->AddButton("VEH_TO_VEH_PHYS test", datCallback(NetworkBank_VEH_TO_VEH_PHYS));
		//ms_pBank->AddButton("Detach trailer",       datCallback(NetworkBank_DetachTrailer));
        //ms_pBank->AddButton("Attach Vehicle To Trailer", datCallback(NetworkBank_AttachVehicleToTrailer));
	}
}

template <class T> void AddPedBlendingWidgets(T *blenderData, const char *blenderName)
{
    ms_pBank->PushGroup(blenderName, false);
    ms_pBank->AddToggle("Active",                                      &blenderData->m_BlendingOn);
    ms_pBank->AddToggle("Use Blend Velocity Smoothing",                &blenderData->m_UseBlendVelSmoothing);
    ms_pBank->AddSlider("Blend Velocity Smooth Time",                  &blenderData->m_BlendVelSmoothTime,          35, 1000, 5);
    ms_pBank->AddSlider("Blend Ramp Time",                             &blenderData->m_BlendRampTime,               0, 10000, 5);
    ms_pBank->AddSlider("Blend Stop Time",                             &blenderData->m_BlendStopTime,               0, 10000, 5);
    ms_pBank->AddSlider("Min position delta",                          &blenderData->m_PositionDeltaMin,            0.0f, 1.0f, 0.01f);
    ms_pBank->AddToggle("Apply velocity",                              &blenderData->m_ApplyVelocity);
    ms_pBank->AddToggle("Predict Acceleration",                        &blenderData->m_PredictAcceleration);
    ms_pBank->AddSlider("Max Predict Time",                            &blenderData->m_MaxPredictTime,              0.0f, 2.0f, 0.01f);
    ms_pBank->AddSlider("Max Speed To Predict",                        &blenderData->m_MaxSpeedToPredict,           0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max heading delta",                           &blenderData->m_MaxHeightDelta,              0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Heading",                                     &blenderData->m_HeadingBlend,                0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Min heading delta",                           &blenderData->m_HeadingBlendMin,             0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max heading delta",                           &blenderData->m_HeadingBlendMax,             0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max pos delta different interiors",           &blenderData->m_MaxPosDeltaDiffInteriors,    0.0f, 100.0f, 0.1f);
    ms_pBank->AddToggle("Extrapolate heading",                         &blenderData->m_ExtrapolateHeading);
    ms_pBank->AddSlider("Velocity Error Threshold",                    &blenderData->m_VelocityErrorThreshold,      0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Normal Mode Pos Threshold",                   &blenderData->m_NormalModePositionThreshold, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Low Speed Threshold",                         &blenderData->m_LowSpeedThresholdSqr,        0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("High Speed Threshold",                        &blenderData->m_HighSpeedThresholdSqr,       0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("Max position delta after blend (On Screen )", &blenderData->m_MaxOnScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta after blend (Off Screen)", &blenderData->m_MaxOffScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Logarithmic Blend Ratio",                     &blenderData->m_LogarithmicBlendRatio,        0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Logarithmic Max Vel Change",                  &blenderData->m_LogarithmicBlendMaxVelChange, 0.0f, 150.0f, 1.0f);
    ms_pBank->AddSlider("Lerp on object blend ratio",                  &blenderData->m_LerpOnObjectBlendRatio,       0.1f, 1.0f, 0.05f);
    ms_pBank->AddSlider("Lerp on object speed threshold",              &blenderData->m_LerpOnObjectThreshold,        0.0f, 500.0f, 0.1f);
    ms_pBank->AddSlider("Start all direction blend threshold",         &blenderData->m_StartAllDirThreshold,         0.0f, 500.0f, 0.1f);
    ms_pBank->AddSlider("Stop all direction blend threshold",          &blenderData->m_StopAllDirThreshold,          0.0f, 500.0f, 0.1f);
    ms_pBank->AddSlider("Minimum speed for all direction blending",    &blenderData->m_MinAllDirThreshold,           0.0f, 500.0f, 0.1f);
    ms_pBank->AddSlider("Maximum correction vector magnitude",         &blenderData->m_MaxCorrectionVectorMag,       0.0f, 500.0f, 0.1f);
#if DEBUG_DRAW
    ms_pBank->AddToggle("Display Probe Hits",                          &blenderData->m_DisplayProbeHits);
#endif // DEBUG_DRAW

    ms_pBank->PushGroup("Low Speed Mode");
    ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_LowSpeedMode.m_MinVelChange,           0.0f, 10.0f,  0.05f);
    ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_LowSpeedMode.m_MaxVelChange,           0.0f, 10.0f,  0.05f);
    ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_LowSpeedMode.m_VelChangeRate,          0.0f, 1.0f,   0.025f);
    ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_LowSpeedMode.m_ErrorIncreaseVel,       0.0f, 10.0f,  0.025f);
    ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_LowSpeedMode.m_ErrorDecreaseVel,       0.0f, 10.0f,  0.025f);
    ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_LowSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_LowSpeedMode.m_MaxVelDiffFromTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_LowSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_LowSpeedMode.m_PositionDeltaMaxLow,    0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_LowSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_LowSpeedMode.m_PositionDeltaMaxHigh,   0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("Normal Mode");
        ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_NormalMode.m_MinVelChange,           0.0f, 10.0f,  0.05f);
        ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_NormalMode.m_MaxVelChange,           0.0f, 10.0f,  0.05f);
        ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_NormalMode.m_VelChangeRate,          0.0f, 1.0f,   0.025f);
        ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_NormalMode.m_ErrorIncreaseVel,       0.0f, 10.0f,  0.025f);
        ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_NormalMode.m_ErrorDecreaseVel,       0.0f, 10.0f,  0.025f);
        ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_NormalMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_NormalMode.m_MaxVelDiffFromTarget,   0.0f, 50.0f,  0.025f);
        ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_NormalMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
       	ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_NormalMode.m_PositionDeltaMaxLow,    0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_NormalMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_NormalMode.m_PositionDeltaMaxHigh,   0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("High Speed Mode");
        ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_HighSpeedMode.m_MinVelChange,           0.0f, 10.0f,  0.05f);
        ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_HighSpeedMode.m_MaxVelChange,           0.0f, 10.0f,  0.05f);
        ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_HighSpeedMode.m_VelChangeRate,          0.0f, 1.0f,   0.025f);
        ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_HighSpeedMode.m_ErrorIncreaseVel,       0.0f, 10.0f,  0.025f);
        ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_HighSpeedMode.m_ErrorDecreaseVel,       0.0f, 10.0f,  0.025f);
        ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_HighSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_HighSpeedMode.m_MaxVelDiffFromTarget,   0.0f, 50.0f,  0.025f);
        ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_HighSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
        ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_HighSpeedMode.m_PositionDeltaMaxLow,    0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_HighSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_HighSpeedMode.m_PositionDeltaMaxHigh,   0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("Ragdoll Blending");
        ms_pBank->AddSlider("Position (Low)",              &blenderData->m_RagdollPosBlendLow,      0.0f, 1.0f,   0.01f);
        ms_pBank->AddSlider("Position (Medium)",           &blenderData->m_RagdollPosBlendMedium,   0.0f, 1.0f,   0.01f);
        ms_pBank->AddSlider("Position (High)",             &blenderData->m_RagdollPosBlendHigh,     0.0f, 1.0f,   0.01f);
        ms_pBank->AddSlider("Ragdoll Pos Delta",           &blenderData->m_RagdollPositionMax,      0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Ragdoll On Object Pos Delta", &blenderData->m_RagdollOnObjPosDeltaMax, 0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Ragdoll Attach Distance",     &blenderData->m_RagdollAttachDistance,   0.0f, 10.0f, 0.1f);
        ms_pBank->AddSlider("Ragdoll Attach Z Offset",     &blenderData->m_RagdollAttachZOffset,    0.0f, 10.0f, 0.1f);
        ms_pBank->AddSlider("Ragdoll Time Before Attach",  &blenderData->m_RagdollTimeBeforeAttach, 0,    10000, 1);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("Collision Mode");
        ms_pBank->AddSlider("Collision Mode Duration",          &blenderData->m_CollisionModeDuration,  0, 60000, 100);
        ms_pBank->AddSlider("Collision Mode Impulse Threshold", &blenderData->m_CollisionModeThreshold, 0.0f, 5000.0f, 100.0f);
    ms_pBank->PopGroup();

    ms_pBank->PopGroup();
}

template <class T> void AddPhysicalBlendingWidgets(T *blenderData, const char *blenderName)
{
	ms_pBank->PushGroup(blenderName, false);
	ms_pBank->AddToggle("Active",                                         &blenderData->m_BlendingOn);
    ms_pBank->AddToggle("Use Blend Velocity Smoothing",                   &blenderData->m_UseBlendVelSmoothing);
    ms_pBank->AddSlider("Blend Velocity Smooth Time",                     &blenderData->m_BlendVelSmoothTime, 35, 1000, 5);
	ms_pBank->AddSlider("Blend Ramp Time",                                &blenderData->m_BlendRampTime, 0, 10000, 5);
	ms_pBank->AddSlider("Blend Stop Time",                                &blenderData->m_BlendStopTime, 0, 10000, 5);
	ms_pBank->AddSlider("Blend Orientation Stop Time",                    &blenderData->m_BlendOrientationStopTime, 0, 10000, 5);
    ms_pBank->AddSlider("Time To Apply Angular Velocity",                 &blenderData->m_TimeToApplyAngVel, 0, 10000, 5);
	ms_pBank->AddSlider("Orientation",                                    &blenderData->m_OrientationBlend, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Min Orientation Diff To Start Blend",            &blenderData->m_MinOrientationDiffToStartBlend, 0.0f, PI / 4.0f, 0.01f);
	ms_pBank->AddSlider("Min position delta",                             &blenderData->m_PositionDeltaMin, 0.0f, 1.0f, 0.01f);
	ms_pBank->AddSlider("Min orientation delta",                          &blenderData->m_OrientationDeltaMin, 0.0f, TWO_PI, 0.01f);
	ms_pBank->AddSlider("Max orientation delta",                          &blenderData->m_OrientationDeltaMax, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max orientation delta after blend (On Screen)",  &blenderData->m_MaxOnScreenOrientationDeltaAfterBlend, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max orientation delta after blend (Off Screen)", &blenderData->m_MaxOffScreenOrientationDeltaAfterBlend, 0.0f, TWO_PI, 0.01f);
	ms_pBank->AddToggle("Apply velocity",                                 &blenderData->m_ApplyVelocity);
    ms_pBank->AddToggle("Predict Acceleration",                           &blenderData->m_PredictAcceleration);
	ms_pBank->AddSlider("Max Predict Time",                               &blenderData->m_MaxPredictTime, 0.0f, 2.0f, 0.01f);
    ms_pBank->AddSlider("Velocity Error Threshold",                       &blenderData->m_VelocityErrorThreshold, 0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Normal Mode Pos Threshold",                      &blenderData->m_NormalModePositionThreshold, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Low Speed Threshold",                            &blenderData->m_LowSpeedThresholdSqr, 0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("High Speed Threshold",                           &blenderData->m_HighSpeedThresholdSqr, 0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("Max position delta after blend (On Screen)",     &blenderData->m_MaxOnScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta after blend (Off Screen)",    &blenderData->m_MaxOffScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Logarithmic Blend Ratio",                        &blenderData->m_LogarithmicBlendRatio, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Logarithmic Max Vel Change",                     &blenderData->m_LogarithmicBlendMaxVelChange, 0.0f, 150.0f, 1.0f);
#if DEBUG_DRAW
    ms_pBank->AddToggle("Display Orientation Debug",                      &blenderData->m_DisplayOrientationDebug);
    ms_pBank->AddToggle("Restrict Debug To Focus Entity",                 &blenderData->m_RestrictDebugDisplayToFocusEntity);
    ms_pBank->AddSlider("Orientation Debug Scale",                        &blenderData->m_DisplayOrientationDebugScale, 1.0f, 100.0f, 0.5f);
#endif // DEBUG_DRAW


    ms_pBank->PushGroup("Low Speed Mode");
    ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_LowSpeedMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_LowSpeedMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_LowSpeedMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_LowSpeedMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_LowSpeedMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_LowSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_LowSpeedMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
    ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_LowSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_LowSpeedMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_LowSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_LowSpeedMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("Normal Mode");
        ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_NormalMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
        ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_NormalMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
        ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_NormalMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
        ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_NormalMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
        ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_NormalMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
        ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_NormalMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_NormalMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
        ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_NormalMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
       	ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_NormalMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_NormalMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_NormalMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("High Speed Mode");
        ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_HighSpeedMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
        ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_HighSpeedMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
        ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_HighSpeedMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
        ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_HighSpeedMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
        ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_HighSpeedMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
        ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_HighSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
        ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_HighSpeedMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
        ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_HighSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
        ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_HighSpeedMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_HighSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
	    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_HighSpeedMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

	ms_pBank->PopGroup();
}

template <class T> void AddVehicleBlendingWidgets(T *blenderData, const char *blenderName)
{
    ms_pBank->PushGroup(blenderName, false);
    ms_pBank->AddToggle("Active",                                         &blenderData->m_BlendingOn);
    ms_pBank->AddToggle("Use Blend Velocity Smoothing",                   &blenderData->m_UseBlendVelSmoothing);
    ms_pBank->AddSlider("Blend Velocity Smooth Time",                     &blenderData->m_BlendVelSmoothTime, 35, 1000, 5);
    ms_pBank->AddSlider("Blend Ramp Time",                                &blenderData->m_BlendRampTime, 0, 10000, 5);
    ms_pBank->AddSlider("Blend Stop Time",                                &blenderData->m_BlendStopTime, 0, 10000, 5);
    ms_pBank->AddSlider("Blend Orientation Stop Time",                    &blenderData->m_BlendOrientationStopTime, 0, 10000, 5);
    ms_pBank->AddSlider("Time To Apply Angular Velocity",                 &blenderData->m_TimeToApplyAngVel, 0, 10000, 5);
    ms_pBank->AddSlider("Orientation",                                    &blenderData->m_OrientationBlend, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Min Orientation Diff To Start Blend",            &blenderData->m_MinOrientationDiffToStartBlend, 0.0f, PI / 4.0f, 0.01f);
    ms_pBank->AddSlider("Min position delta",                             &blenderData->m_PositionDeltaMin, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Min orientation delta",                          &blenderData->m_OrientationDeltaMin, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max orientation delta",                          &blenderData->m_OrientationDeltaMax, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max orientation delta after blend (On Screen)",  &blenderData->m_MaxOnScreenOrientationDeltaAfterBlend, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max orientation delta after blend (Off Screen)", &blenderData->m_MaxOffScreenOrientationDeltaAfterBlend, 0.0f, TWO_PI, 0.01f);
    ms_pBank->AddSlider("Max steer angle delta",                          &blenderData->m_SteerAngleMax, 0.0f, 2.0f, 0.1f);
    ms_pBank->AddSlider("Min steer angle delta",                          &blenderData->m_SteerAngleMin, 0.0f, 2.0f, 0.1f);
    ms_pBank->AddSlider("Steer angle blend ratio",                        &blenderData->m_SteerAngleRatio, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddToggle("Apply velocity",                                 &blenderData->m_ApplyVelocity);
    ms_pBank->AddToggle("Predict Acceleration",                           &blenderData->m_PredictAcceleration);
    ms_pBank->AddSlider("Max Predict Time",                               &blenderData->m_MaxPredictTime, 0.0f, 2.0f, 0.01f);
    ms_pBank->AddSlider("Velocity Error Threshold",                       &blenderData->m_VelocityErrorThreshold, 0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Normal Mode Pos Threshold",                      &blenderData->m_NormalModePositionThreshold, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Low Speed Threshold",                            &blenderData->m_LowSpeedThresholdSqr, 0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("High Speed Threshold",                           &blenderData->m_HighSpeedThresholdSqr, 0.0f, 10000.0f, 10.0f);
    ms_pBank->AddSlider("Max position delta after blend (On Screen)",     &blenderData->m_MaxOnScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta after blend (Off Screen)",    &blenderData->m_MaxOffScreenPositionDeltaAfterBlend,  0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Logarithmic Blend Ratio",                        &blenderData->m_LogarithmicBlendRatio, 0.0f, 1.0f, 0.01f);
    ms_pBank->AddSlider("Logarithmic Max Vel Change",                     &blenderData->m_LogarithmicBlendMaxVelChange, 0.0f, 150.0f, 1.0f);
    ms_pBank->AddSlider("Orientation Blend Nearby Vehicle Threshold",     &blenderData->m_NearbyVehicleThreshold, 0.0f, 10000.0f, 0.5f);
#if DEBUG_DRAW
    ms_pBank->AddToggle("Display Orientation Debug",                      &blenderData->m_DisplayOrientationDebug);
    ms_pBank->AddToggle("Restrict Debug To Focus Entity",                 &blenderData->m_RestrictDebugDisplayToFocusEntity);
    ms_pBank->AddSlider("Orientation Debug Scale",                        &blenderData->m_DisplayOrientationDebugScale, 1.0f, 100.0f, 0.5f);
#endif // DEBUG_DRAW

    ms_pBank->PushGroup("Low Speed Mode");
    ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_LowSpeedMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_LowSpeedMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_LowSpeedMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_LowSpeedMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_LowSpeedMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_LowSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_LowSpeedMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
    ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_LowSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_LowSpeedMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_LowSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_LowSpeedMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("Normal Mode");
    ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_NormalMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_NormalMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_NormalMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_NormalMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_NormalMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_NormalMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_NormalMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
    ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_NormalMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_NormalMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_NormalMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_NormalMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PushGroup("High Speed Mode");
    ms_pBank->AddSlider("Min Velocity Change",        &blenderData->m_HighSpeedMode.m_MinVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Max Velocity Change",        &blenderData->m_HighSpeedMode.m_MaxVelChange,     0.0f, 10.0f, 0.05f);
    ms_pBank->AddSlider("Velocity Change Rate ",      &blenderData->m_HighSpeedMode.m_VelChangeRate,    0.0f, 1.0f, 0.025f);
    ms_pBank->AddSlider("Error Increase Threshold",   &blenderData->m_HighSpeedMode.m_ErrorIncreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Error Decrease Threshold",   &blenderData->m_HighSpeedMode.m_ErrorDecreaseVel, 0.0f, 10.0f, 0.025f);
    ms_pBank->AddSlider("Small Velocity Squared",     &blenderData->m_HighSpeedMode.m_SmallVelocitySquared,   0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max Vel Diff From Target",   &blenderData->m_HighSpeedMode.m_MaxVelDiffFromTarget, 0.0f, 50.0f, 0.025f);
    ms_pBank->AddSlider("Max Vel Change To Target",   &blenderData->m_HighSpeedMode.m_MaxVelChangeToTarget,   0.0f, 50.0f,  0.025f);
    ms_pBank->AddSlider("Max position delta (Low)",   &blenderData->m_HighSpeedMode.m_PositionDeltaMaxLow, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (Medium)",&blenderData->m_HighSpeedMode.m_PositionDeltaMaxMedium, 0.0f, 100.0f, 0.1f);
    ms_pBank->AddSlider("Max position delta (High)",  &blenderData->m_HighSpeedMode.m_PositionDeltaMaxHigh, 0.0f, 100.0f, 0.1f);
    ms_pBank->PopGroup();

    ms_pBank->PopGroup();
}

void SetupBlendingWidgets()
{
    ms_pBank->PushGroup("Blending", false);
        ms_pBank->PushGroup("Ped", false);
            ms_pBank->AddToggle("Display push round cars debug",        &CNetBlenderPed::ms_DisplayPushRoundCarsDebug);
            ms_pBank->AddToggle("Use local player for car push target", &CNetBlenderPed::ms_UseLocalPlayerForCarPushTarget);
            ms_pBank->AddSlider("Push round cars box expand size",      &CNetBlenderPed::ms_PushRoundCarsBoxExpandSize, 0.0f, 1.0f, 0.05f);
            ms_pBank->AddSlider("Push round cars force",                &CNetBlenderPed::ms_PushRoundCarsForce, 0.0f, 5000.0f, 0.5f);
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataOnFoot,                   "On Foot");
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataInWater,                   "In Water");
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataInAir,                    "In Air");
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataTennis,                   "Tennis");
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataFirstPerson,              "First Person");
            AddPedBlendingWidgets(CNetObjPed::ms_pedBlenderDataHiddenUnderMapForVehicle, "Hidden Under Map For Vehicle");
            AddPedBlendingWidgets(CNetBlenderPed::ms_pedBlenderDataAnimatedRagdoll,      "Animated Ragdoll");
		ms_pBank->PopGroup();
        AddPhysicalBlendingWidgets(CNetObjObject::ms_ObjectBlenderDataStandard,      "Object Standard");
        AddPhysicalBlendingWidgets(CNetObjObject::ms_ObjectBlenderDataHighPrecision, "Object High Precision");
        AddPhysicalBlendingWidgets(CNetObjObject::ms_ObjectBlenderDataArenaBall,     "Arena Ball");
		AddPhysicalBlendingWidgets(CNetObjPhysical::ms_physicalBlenderData,          "Physical");
        AddVehicleBlendingWidgets(CNetObjVehicle::ms_vehicleBlenderData,		     "Vehicle");
		AddVehicleBlendingWidgets(CNetObjHeli::ms_heliBlenderData,				     "Helicopter");
		AddVehicleBlendingWidgets(CNetObjPlane::ms_planeBlenderData,			     "Plane");
		AddVehicleBlendingWidgets(CNetObjTrain::ms_trainBlenderData,			     "Train");
        AddVehicleBlendingWidgets(CNetObjBoat::ms_boatBlenderData,			         "Boat");
        ms_pBank->AddToggle("Debug draw bounds for failed vehicle orientation blends", &gbShowFailedOrientationBlends);
        ms_pBank->AddSlider("Collision time (vehicle)",&CNetObjPhysical::ms_collisionSyncIgnoreTimeVehicle, 0, 2000, 10);
        ms_pBank->AddSlider("Collision time (map)",&CNetObjPhysical::ms_collisionSyncIgnoreTimeMap, 0, 2000, 10);
        ms_pBank->AddSlider("Collision impulse",&CNetObjPhysical::ms_collisionMinImpulse, 0.0f, 2000.0f, 10.0f);
        ms_pBank->AddSlider("Collision time (Arena ball)", &CNetObjObject::ms_collisionSyncIgnoreTimeArenaBall, 0, 2000, 10);
        ms_pBank->AddSlider("Collision impulse (Arena ball)", &CNetObjObject::ms_collisionMinImpulseArenaBall, 0.0f, 2000.0f, 10.0f);
    ms_pBank->PopGroup();
}

void UpdateCombos()
{
    // don't update the combos if the network bank is closed
    if(ms_pBank && !ms_pBank->AreAnyWidgetsShown())
        return;

    // check whether it is time to refresh all of the combo boxes
    // refresh the list every second
    bool refreshCombos = false;

    static unsigned lastTime = fwTimer::GetSystemTimeInMilliseconds();
           unsigned thisTime = fwTimer::GetSystemTimeInMilliseconds();

    if((thisTime > lastTime) && ((thisTime - lastTime) > 1000))
    {
        lastTime      = thisTime;
        refreshCombos = true;
    }

    // update the player list
    s_NumPlayers = 0;
	unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
    const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
    
    for(unsigned index = 0; index < numRemoteActivePlayers; index++)
    {
        const netPlayer *pPlayer = remoteActivePlayers[index];
		s_Players[s_NumPlayers++] = pPlayer->GetGamerInfo().GetName();
	}
	// account for selected player having left
	s_PlayerComboIndex = Min(s_PlayerComboIndex, static_cast<int>(s_NumPlayers) - 1);
	s_PlayerComboIndex = Max(s_PlayerComboIndex, 0);

	s_NumKickPlayers = 0;
	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
    const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        const netPlayer *pPlayer = activePlayers[index];
		s_KickPlayers[s_NumKickPlayers++] = pPlayer->GetGamerInfo().GetName();
	}
	// account for selected player having left
	s_KickPlayerComboIndex = Min(s_KickPlayerComboIndex, static_cast<int>(s_NumKickPlayers) - 1);
	s_KickPlayerComboIndex = Max(s_KickPlayerComboIndex, 0);

	// avoid empty combo boxes (RAG can't handle them)
	int nNumPlayers = s_NumPlayers;

	if(nNumPlayers == 0)
	{
		s_Players[0] = "--No Players--";
		nNumPlayers  = 1;
	}

	// avoid empty combo boxes (RAG can't handle them)
	int nNumKickPlayers = s_NumKickPlayers;

	if(nNumKickPlayers == 0)
	{
		s_KickPlayers[0] = "--No Players--";
		nNumKickPlayers  = 1;
	}

	if(refreshCombos)
    {
        // update player combo
        if(s_PlayerCombo)
        {
			s_PlayerCombo->UpdateCombo("Player", &s_PlayerComboIndex, nNumPlayers, (const char **)s_Players);
        }

		// update kick player combo
		if(s_KickPlayerCombo)
		{
			s_KickPlayerCombo->UpdateCombo("Kick Player", &s_KickPlayerComboIndex, nNumKickPlayers, (const char **)s_KickPlayers);
		}
	}

    if(refreshCombos)
    {
        // update the follow ped targets
        s_NumFollowPedTargets = 0;

		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

            if(player->GetPlayerPed())
            {
			    s_FollowPedTargets[s_NumFollowPedTargets] = player->GetGamerInfo().GetName();
                s_FollowPedTargetIndices[s_NumFollowPedTargets] = player->GetPhysicalPlayerIndex();

			    //Update the currently tracked ped in the combo box
				netObject* netobj = player->GetPlayerPed()->GetNetworkObject();
			    if(netobj && NetworkInterface::GetSpectatorObjectId() == netobj->GetObjectID())
			    {
				    s_FollowPedIndex = s_NumFollowPedTargets;
			    }

			    s_NumFollowPedTargets++;
            }
		}

        int nNumFollowPedTargets = s_NumFollowPedTargets;

        if(nNumFollowPedTargets == 0)
        {
            s_FollowPedTargets[0] = "--No Players--";
            s_FollowPedTargetIndices[0] = 0;
            nNumFollowPedTargets = 1;
        }

        if(s_FollowPedCombo)
        {
			if (s_FollowPedIndex >= nNumFollowPedTargets)
			{
				s_FollowPedIndex = 0;
			}

            s_FollowPedCombo->UpdateCombo("Follow Player", &s_FollowPedIndex, nNumFollowPedTargets, (const char **)s_FollowPedTargets, NetworkBank_ChangeFollowPedTarget);
        }

        if(s_UpdateRateCombo)
        {
			if (ms_netDebugDisplaySettings.m_displayInformation.m_updateRatePlayer >= nNumFollowPedTargets)
			{
				ms_netDebugDisplaySettings.m_displayInformation.m_updateRatePlayer = 0;
			}

            // use follow ped targets data as it is the same
            s_UpdateRateCombo->UpdateCombo("Relative player", &ms_netDebugDisplaySettings.m_displayInformation.m_updateRatePlayer, nNumFollowPedTargets, (const char **)s_FollowPedTargets, NullCB);
        }
    }
}

unsigned GetNumOutgoingReliables()
{
    return gLastNumOutgoingReliables;
}

#else

void PrintNetworkInfo()
{
#if !__FINAL
	if (!NetworkInterface::IsGameInProgress())
		return;

	if (s_bDisplayPauseText && fwTimer::IsGamePaused())
	{
		DLC_Add(RenderDebugMessage);
	}
	else
	{
		s_bDisplayPauseText = false;
	}
#endif
}

void DisplayNetworkScripts()
{
}

#endif // __BANK

void Init()
{
#if !__DEV
    // recordBandwidth = PARAM_recordbandwidth.Get();
#endif // !__DEV

#if __BANK

	ms_FocusEntityId			= NETWORK_INVALID_OBJECT_ID;
	ms_bViewFocusEntity			= false;
    m_bAllowOwnershipChange     = true;
    m_bAllowProximityOwnership  = true;
    m_bUsePlayerCameras         = true;
    m_bSkipAllSyncs             = false;
    m_bIgnoreGiveControlEvents  = false;
    m_bIgnoreVisibilityChecks   = false;
    ms_EnablePredictionLogging  = PARAM_predictionLogging.Get();
	m_bEnablePhysicsOptimisations = true;
    s_bMinimalPredictionLogging = PARAM_minimalpredictionlogging.Get();

	ms_bCatchCloneBindPose		= PARAM_netBindPoseClones.Get();
	ms_bCatchOwnerBindPose		= PARAM_netBindPoseOwners.Get();
	ms_bCatchCopBindPose		= PARAM_netBindPoseCops.Get();
	ms_bCatchNonCopBindPose		= PARAM_netBindPoseNonCops.Get();

	ms_bTaskMotionPedDebug		= PARAM_netTaskMotionPedDebug.Get();

	ms_bDebugTaskLogging		= PARAM_netDebugTaskLogging.Get();

    for(unsigned index = 0; index < NET_MEM_NUM_MANAGERS; index++)
    {
        g_NetworkHeapUsage[index] = 0;
    }

    gNumOrientationBlendFailures = 0;

#endif // __BANK

#if __DEV
	for (u32 i=0; i<NUM_BREAK_TYPES; i++)
	{
		m_breakTypes[i] = false;
	}
#endif // __DEV

#if __BANK
	for (u32 i=0; i<RLROS_NUM_PRIVILEGEID; i++)
		s_RosPrivilegesTypes[i] = false;
#endif

#if !__NO_OUTPUT
	int nDebugAccessCode = 0;
	if (PARAM_netMpAccessCode.Get(nDebugAccessCode))
	{
		s_BankNetworkAccessCode = nDebugAccessCode;
	}

	if (PARAM_netMpAccessCodeNotReady.Get())
	{
		s_BankForceNotReadyForAccessCheck = true;
	}

	if (PARAM_netMpAccessCodeReady.Get())
	{
		s_BankForceReadyForAccessCheck = true;
	}
#endif
}

#if __BANK
void CredentialsUpdated(const int localGamerIndex)
{
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		for (u32 i=0; i<RLROS_NUM_PRIVILEGEID; i++)
			s_RosPrivilegesTypes[i] = rlRos::HasPrivilege(localGamerIndex, (rlRosPrivilegeId)i);
	}
}
#endif

void Shutdown()
{

}

void InitSession()
{
#if __BANK
	CredentialsUpdated(NetworkInterface::GetLocalGamerIndex());

    const unsigned PREDICTION_LOG_BUFFER_SIZE = 40 * 1024;
    s_PopulationLog = rage_new netLog("PopulationFail.csv", LOGOPEN_CREATE, netInterface::GetDefaultLogFileTargetType(), netInterface::GetDefaultLogFileBlockingMode());
    s_PredictionLog = rage_new netLog("Prediction.log",     LOGOPEN_CREATE, netInterface::GetDefaultLogFileTargetType(), netInterface::GetDefaultLogFileBlockingMode(), PREDICTION_LOG_BUFFER_SIZE);
#endif // __BANK
#if !__FINAL
#if ENABLE_NETWORK_BOTS
    CNetworkBot::Init();
#endif // ENABLE_NETWORK_BOTS
    BANK_ONLY(CNetworkDebugPlayback::Init());
    CNetworkSoakTests::Init();

    if(PARAM_disablemessagelogs.Get())
    {
        CNetwork::GetMessageLog().Disable();
    }
#endif // !__FINAL
}

void ShutdownSession()
{
#if __BANK
    delete s_PopulationLog;
    s_PopulationLog = 0;
    delete s_PredictionLog;
    s_PredictionLog = 0;
#endif // __BANK
#if !__FINAL
#if ENABLE_NETWORK_BOTS
    CNetworkBot::Shutdown();
#endif // ENABLE_NETWORK_BOTS
    BANK_ONLY(CNetworkDebugPlayback::Shutdown());
#endif // !__FINAL
}

void OpenNetwork()
{
#if !__FINAL
	s_MessageHandler.Init();
#endif

#if __BANK
	if (ms_pBank && ms_pDebugGroup)
	{
		ms_pBank->SetCurrentGroup(*ms_pDebugGroup);
		AddSyncTreeWidgets();
		ms_pBank->UnSetCurrentGroup(*ms_pDebugGroup);					
	}
#endif // __BANK
}

void CloseNetwork()
{
#if !__FINAL
	s_MessageHandler.Shutdown();
#endif

#if __BANK
	if (ms_pSyncTreeGroup)
	{
		ms_pSyncTreeGroup->Destroy();
		ms_pSyncTreeGroup = 0;
	}
#endif // __BANK
}

void CreatePed(fwModelId pedModelId, const Vector3& pos)
{
	Matrix34 TempMat;

	TempMat.Identity();
	TempMat.d = pos;

	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		return;
	}

#if __ASSERT
	CPedModelInfo * pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	gnetAssert(pPedModelInfo);
#endif

	// create the debug ped which will have it's textures played with
	const CControlledByInfo localAiControl(false, false);
	CPed *pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &TempMat, true, true, true);

    if(pPed)
    {
		pPed->PopTypeSet(POPTYPE_TOOL);
		pPed->SetDefaultDecisionMaker();
		pPed->SetCharParamsBasedOnManagerType();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

        if(pPed->GetNetworkObject())
        {
            pPed->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true); 
        }

        pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskDoNothing(-1));

	    CGameWorld::Add(pPed, CGameWorld::OUTSIDE );
		pPed->GetPortalTracker()->ScanUntilProbeTrue();
    }
}

void CreateCar(fwModelId vehModelId, const Vector3& pos)
{
    Matrix34 TempMat;

	TempMat.Identity();
	TempMat.d = pos;

	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(vehModelId))
	{
		CModelInfo::RequestAssets(vehModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(vehModelId))
	{
		return;
	}

    CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(vehModelId, ENTITY_OWNEDBY_SCRIPT, POPTYPE_MISSION, &TempMat);

    if(pNewVehicle)
	{
		pNewVehicle->SetIsAbandoned();

		CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);

		CPortalTracker* pPT = pNewVehicle->GetPortalTracker();
		if (pPT)
		{
			pPT->RequestRescanNextUpdate();
			pPT->Update(VEC3V_TO_VECTOR3(pNewVehicle->GetTransform().GetPosition()));
		}

		if (pNewVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_INITIALLY)		// The game creates police cars with doors locked. For debug cars we don't want this.
		{
			pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
		}
	}
}

void CreateObject(fwModelId objModelId, const Vector3& pos)
{
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(objModelId))
	{
		CModelInfo::RequestAssets(objModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(objModelId))
	{
		return;
	}

    CObject *pNewObject = CObjectPopulation::CreateObject(objModelId, ENTITY_OWNEDBY_SCRIPT, true);

    if(pNewObject)
	{
		pNewObject->SetupMissionState();
        pNewObject->SetPosition(pos);
		CGameWorld::Add(pNewObject, CGameWorld::OUTSIDE);
	}
}

void CreatePickup(fwModelId objModelId, Vector3& pos)
{
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(objModelId))
	{
		CModelInfo::RequestAssets(objModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(objModelId))
	{
		return;
	}

    Vector3 pickupOrientation = Vector3(1.0f, 1.0f, 1.0f);
    PlacementFlags flags = CPickupPlacement::PLACEMENT_CREATION_FLAG_FIXED;

    CPickupManager::RegisterPickupPlacement(PICKUP_WEAPON_PISTOL, pos, pickupOrientation, flags);
}

void UpdatePerformanceTest()
{
	if(NetworkInterface::IsInSession() && NetworkInterface::IsHost())
    {
        static int timeToWait = 10000;

        if(timeToWait > 0)
        {
            timeToWait -= fwTimer::GetTimeStepInMilliseconds();
        }
        else
        {
            static bool objectsCreated = false;
            if(!objectsCreated)
            {/*
                gNumPedsToCreate = 63;
                gNumCarsToCreate = 70;
                gNumObjectsToCreate = 50;
                gNumPickupsToCreate = 7;
				m_bAllowProximityOwnership = false;*/
                objectsCreated = true;
            }

            if(gNumPickupsToCreate > 0)
            {
                unsigned pickupIndex = 32 - gNumPickupsToCreate;

                const Vector3 startPos(-62.0f, -28.0f, 1.0f);
                Vector3 pickupPos = startPos;
                pickupPos.x += (1.0f * pickupIndex);
                fwModelId modelId = CModelInfo::GetModelIdFromName("W_PI_Pistol");
                CreatePickup(modelId, pickupPos);

                gNumPickupsToCreate--;
            }

            if(gNumObjectsToCreate > 0)
            {
                unsigned objIndex = 50 - gNumObjectsToCreate;

                const Vector3 startPos(-62.0f, -29.0f, 1.0f);
                Vector3 objPos = startPos;
                objPos.x += (1.0f * objIndex);
                fwModelId modelId = CModelInfo::GetModelIdFromName("Prop_Barrel_01A");
                CreateObject(modelId, objPos);

                gNumObjectsToCreate--;
            }

            if(gNumCarsToCreate > 0)
            {
                unsigned carIndex = 70 - gNumCarsToCreate;
                unsigned YOffset = carIndex / 25;
                unsigned XOffset = carIndex % 25;

                const Vector3 startPos(-62.0f, -48.0f, 1.5f);
                Vector3 carPos = startPos;
                carPos.x += (4.0f * XOffset);
                carPos.y += (6.0f * YOffset);
                
                fwModelId modelId = CModelInfo::GetModelIdFromName("tailgater");
                CreateCar(modelId, carPos);

                gNumCarsToCreate--;
            }

            if(gNumPedsToCreate > 0)
            {
                // generate a location to create the ped from the camera position & orientation
                Vector3 srcPos = camInterface::GetPos();
                Vector3 destPos = srcPos;
                Vector3 viewVector = camInterface::GetFront();

                static dev_float DIST_IN_FRONT = 7.0f;
                destPos += viewVector*DIST_IN_FRONT;		// create at position DIST_IN_FRONT metres in front of current camera position

                WorldProbe::CShapeTestProbeDesc probeDesc;
                WorldProbe::CShapeTestFixedResults<> probeResults;
                probeDesc.SetResultsStructure(&probeResults);
                probeDesc.SetStartAndEnd(srcPos, destPos);
                probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
                probeDesc.SetExcludeEntity(CGameWorld::FindLocalPlayer());
                probeDesc.SetContext(WorldProbe::LOS_Unspecified);
                if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
                {
	                destPos = probeResults[0].GetHitPosition();
	                static dev_float MOVE_BACK_DIST = 1.f;
	                destPos -= viewVector * MOVE_BACK_DIST;
                }

                // try to avoid creating the ped on top of the player
                const Vector3 vLocalPlayerPosition = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
                if((vLocalPlayerPosition - destPos).XYMag2() < 0.8f*0.8f)
                {
	                if((vLocalPlayerPosition - camInterface::GetPos()).Mag2() < square(3.0f))
		                destPos -= viewVector;
                }

                if (destPos.z <= -100.0f)
                {
	                destPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, destPos.x, destPos.y);
                }
                destPos.z += 1.0f;

                fwModelId modelId = CModelInfo::GetModelIdFromName("A_F_M_BEACH_01");

                CreatePed(modelId, destPos);

                gNumPedsToCreate--;
            }
        }
    }
}

#if __BANK

void  UpdateSanityCheckHeap( )
{
	const u32 currFrame = fwTimer::GetFrameCount();
	const u32 currTime = sysTimer::GetSystemMsTime();

	static u32  s_LastUpdateFrame = 0;
	static u32  s_LastUpdateTime = 0;
	static bool s_AssertOnLowHeap = true;
	static u32  s_FrameCounterUntilAssertReset = 0;

	//Don't update more than once per frame.
	if (s_LastUpdateFrame == currFrame)
	{
		return;
	}

	if((currTime - s_LastUpdateTime) > 2000)
	{
		s_AssertOnLowHeap = false;
		s_FrameCounterUntilAssertReset = 0;
	}

	if (!s_AssertOnLowHeap)
	{
		if (s_FrameCounterUntilAssertReset > 10)
		{
			s_FrameCounterUntilAssertReset = 0;
			s_AssertOnLowHeap = true;
		}

		++s_FrameCounterUntilAssertReset;
	}

	size_t memAvailable = CNetwork::GetSCxnMgrAllocator().GetMemoryAvailable();
	if (memAvailable < ms_SCxnMgrHeapTideMark)
		ms_SCxnMgrHeapTideMark = memAvailable;

	// assert when heap gets too full
	if (memAvailable < unsigned(CNetwork::GetSCxnMgrAllocator().GetHeapSize() * 0.1f))
	{
		if (!s_AssertOnLowHeap)
		{
			gnetWarning("Connection Manager Network memory heap nearly full (assert disabled). Available: %lu, Size: %lu, Low: %lu", memAvailable, CNetwork::GetSCxnMgrAllocator().GetHeapSize(), ms_SCxnMgrHeapTideMark);
			gnetWarning("Update s_LastUpdateTime=%u, currTime=%u", s_LastUpdateTime, currTime);
			gnetWarning("Update s_LastUpdateFrame=%u, currFrame=%u", s_LastUpdateFrame, currFrame);
		}

		//gnetAssertf(s_AssertOnLowHeap, "Connection Manager Network memory heap nearly full");
	}

	//Update last update time.
	s_LastUpdateTime = currTime;
	s_LastUpdateFrame = currFrame;
}

#endif // __BANK

void Update()
{
	netTimeBarWrapper timeBarWrapper(netInterface::ShouldUpdateTimebars());

	timeBarWrapper.StartTimer("UpdateCombos");
	BANK_ONLY(UpdateCombos();)

	if (CNetwork::IsNetworkOpen())
	{
#if __BANK
		if(NetworkDebug::IsBindPoseCatchingEnabled())
		{
			PerformPedBindPoseCheckDuringUpdate(ms_bCatchOwnerBindPose, ms_bCatchCloneBindPose, ms_bCatchCopBindPose, ms_bCatchNonCopBindPose);
		}
#endif

#if __DEV
		timeBarWrapper.StartTimer("Heap Sanity Check");
		sysMemSimpleAllocator *networkHeap = dynamic_cast<sysMemSimpleAllocator *>(CNetwork::GetNetworkHeap());
        if(networkHeap)
        {
			networkHeap->SanityCheckHeapOnDoAllocate(sanityCheckHeapOnDoAllocate);

            // Disabled for now to improve network performance in Beta builds
            /*if(!sanityCheckHeapOnDoAllocate && IsGameInProgress())
            {
                if((fwTimer::GetFrameCount()%5) == 0)
                {
                    networkHeap->SanityCheck();
                }
            }*/
        }

		sysMemSimpleAllocator *gameHeap = dynamic_cast<sysMemSimpleAllocator *>(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
        if(gameHeap)
        {
            gameHeap->SanityCheckHeapOnDoAllocate(sanityCheckHeapOnDoAllocate);

            // Disabled for now to improve network performance in Beta builds
            /*if(!sanityCheckHeapOnDoAllocate && IsGameInProgress())
            {
                if((fwTimer::GetFrameCount()%5) == 0)
                {
                    gameHeap->SanityCheck();
                }
            }*/
        }
#endif // __DEV

#if __BANK
		timeBarWrapper.StartTimer("UpdateBandwidthProfiler");
		UpdateBandwidthProfiler();
       
		timeBarWrapper.StartTimer("UpdateBandwidthLog");
        if(!PARAM_nobandwidthlogging.Get())
        {
		    UpdateBandwidthLog();
			CTheScripts::GetScriptHandlerMgr().UpdateBandwidthLog();
        }
        
		timeBarWrapper.StartTimer("Sample Bandwidth");

        m_AverageManageUpdateLevelCallsPerUpdate.AddSample(static_cast<float>(gNumManageUpdateLevelCalls));
        gNumManageUpdateLevelCalls = 0;

        m_AverageSyncCloneCallsPerUpdate.AddSample(static_cast<float>(NetworkInterface::GetObjectManager().GetNumUpdatesSent()));

#endif // __BANK

#if !__FINAL
#if ENABLE_NETWORK_BOTS
		timeBarWrapper.StartTimer("Update Bots");
		if(NetworkInterface::IsInSession())
        {
            CNetworkBot::UpdateAllBots();
        }
#endif // ENABLE_NETWORK_BOTS 
        BANK_ONLY(CNetworkDebugPlayback::Update());
        UpdatePerformanceTest();

		timeBarWrapper.StartTimer("BroadcastDebugTimeSettings");
		BroadcastDebugTimeSettings();
#endif // !__FINAL

#if __BANK
		timeBarWrapper.StartTimer("UpdatePredictionLog");
		UpdatePredictionLog();
		timeBarWrapper.StartTimer("UpdateLogEnabledStatus");
		UpdateLogEnabledStatus();
		timeBarWrapper.StartTimer("UpdateFocusEntitys");
		UpdateFocusEntity(false);

        s_PredictionLog->PostNetworkUpdate();

		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_SHIFT, "Warp to player"))
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

			if (pLocalPlayer && !pLocalPlayer->IsDead())
			{
				unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
                const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
                
	            if(numRemoteActivePlayers > 0)
                {
                    const CNetGamePlayer *target = SafeCast(const CNetGamePlayer, remoteActivePlayers[0]);

					if (target->GetPlayerPed())
					{
						pLocalPlayer->Teleport(VEC3V_TO_VECTOR3(target->GetPlayerPed()->GetTransform().GetPosition()) + Vector3(1.0f, 0.0f, 0.0f), target->GetPlayerPed()->GetCurrentHeading());
					}
				}
			}
		}

		timeBarWrapper.StartTimer("EntityPredictionData");
		gFocusEntityPredictionData.Update();

		timeBarWrapper.StartTimer("UpdateSanityCheckHeap");
		UpdateSanityCheckHeap( );

        // reset sync tree update times
        netSyncTree *autoSyncTree = CNetObjAutomobile::GetStaticSyncTree();
        netSyncTree *pedSyncTree  = CNetObjPed::GetStaticSyncTree();

        if(autoSyncTree && pedSyncTree)
        {
            autoSyncTree->ResetNumTimesUpdateCalled();
            pedSyncTree->ResetNumTimesUpdateCalled();
        }

        // update peak trackers
        g_PeakPedCreates.Update();
        g_PeakVehicleCreates.Update();
        g_PeakOtherCreates.Update();
        g_PeakPedRemoves.Update();
        g_PeakVehicleRemoves.Update();
        g_PeakOtherRemoves.Update();

        g_PeakCloneCreateDuration.Update();
        g_PeakCloneRemoveDuration.Update();
#endif // __BANK
	}
}

void PlayerHasJoined(const netPlayer& BANK_ONLY(player))
{
#if __BANK
	if (!player.IsLocal())
	{
		if (fwTimer::GetDebugTimeScale() != 1.0f && m_timeScaleLocallyAltered)
		{
			msgDebugTimeScale msg(fwTimer::GetDebugTimeScale());
			s_MessageHandler.SendMessageToPlayer(player, msg);
		}

		if (fwTimer::IsUserPaused() && m_timePauseLocallyAltered)
		{
			msgDebugTimePause msg(true, fwTimer::ShouldScriptsBePaused(), false);
			s_MessageHandler.SendMessageToPlayer(player, msg);
		}
	}
#endif // __BANK
}


#if !__FINAL
void DisplayPauseMessage(const char* message)
{
    if(gnetVerifyf(strlen(message) < MAX_DEBUG_TEXT, "Trying to display pause message text with too many characters"))
    {
	    safecpy( s_debugText, message );
	    s_bDisplayPauseText = true;
    }
}

void BroadcastDebugTimeSettings()
{
	if (m_lastTimeScale != fwTimer::GetDebugTimeScale())
	{
		m_lastTimeScale = fwTimer::GetDebugTimeScale();
		m_timeScaleLocallyAltered = true;

		msgDebugTimeScale msg(m_lastTimeScale);
		s_MessageHandler.SendMessageToAllPlayers(msg);
	}

	if (fwTimer::IsUserPaused() != m_lastTimePause)
	{
		m_lastTimePause = fwTimer::IsUserPaused();
		m_timePauseLocallyAltered = true;

		msgDebugTimePause msg(m_lastTimePause, fwTimer::ShouldScriptsBePaused(), fwTimer::GetWantsToSingleStepNextFrame());
		s_MessageHandler.SendMessageToAllPlayers(msg);

		if (m_lastTimePause && !PARAM_suppressPauseTextMP.Get())
		{
			char message[MAX_DEBUG_TEXT];

			const float spentRealMoney = StatsInterface::GetFloatStat(STAT_MPPLY_STORE_MONEY_SPENT);

			formatf(message, MAX_DEBUG_TEXT, "%s has paused the game~n~~n~Character WALLET(%" I64FMT "d)+BANK(%" I64FMT "d) = EVC(%" I64FMT "d)+PVC(%" I64FMT "d), Store Spent(%f)"
				, (FindPlayerPed() && FindPlayerPed()->GetNetworkObject()) ? FindPlayerPed()->GetNetworkObject()->GetLogName() : "??"
				, MoneyInterface::GetVCWalletBalance()
				, MoneyInterface::GetVCBankBalance()
				, MoneyInterface::GetEVCBalance()
				, MoneyInterface::GetPVCBalance()
				, spentRealMoney);

			DisplayPauseMessage(message);
		}
	}

	if ((fwTimer::GetSingleStepThisFrame()||fwTimer::GetWantsToSingleStepNextFrame()) && !m_timeStepRemotelyAltered)
	{
		msgDebugTimePause msg(true, fwTimer::ShouldScriptsBePaused(), true);
		s_MessageHandler.SendMessageToAllPlayers(msg);
	}
}
#endif// !__FINAL

} // namespace rage
