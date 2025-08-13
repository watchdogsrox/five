//
// name:        network.cpp
// description: High level interface to network stuff. All external access to the network code
//              must go through this class.
// written by:  John Gurney
//
#include "network/Network.h"
#include "network/Cloud/Tunables.h"

// rage headers
#include "data/rson.h"
#include "diag/seh.h"
#include "file/device_installer.h"
#include "net/net.h"
#include "net/connectionrouter.h"
#include "net/keyexchange.h"
#include "net/natdetector.h"
#include "net/netallocator.h"
#include "net/netfuncprofiler.h"
#include "net/timesync.h"
#include "net/tunneler_ice.h"
#include "rline/rl.h"
#include "rline/scmatchmaking/rlscmatchmaking.h"
#include "rline/scmatchmaking/rlscmatchmanager.h"
#include "rline/scmembership/rlscmembership.h"
#include "rline/scpresence/rlscpresence.h"
#include "rline/scpresence/rlscpresencetasks.h"
#include "rline/rltitleid.h"
#include "string/stringutil.h"
#include "system/memmanager.h"
#include "system/param.h"
#include "system/simpleallocator.h"

// framework headers
#include "fwlocalisation/languagePack.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwnet/NetLogUtils.h"
#include "fwnet/NetPlayerMessages.h"
#include "fwnet/netchannel.h"
#include "fwnet/NetEventMgr.h"
#include "fwnet/netremotelog.h"
#include "fwnet/netscgamerdatamgr.h"
#include "fwnet/netutils.h"
#include "fwrenderer/renderthread.h"
#include "fwscene/stores/staticboundsstore.h"
#include "net/nethardware.h"
#include "streaming/packfilemanager.h"

// game headers
#include "audio/northaudioengine.h"
#include "audio/radiostation.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "control/gamelogic.h"
#include "core/app.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "event/EventDamage.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/MiniMap.h"
#include "Frontend/MultiplayerGamerTagHud.h"
#include "Frontend/MultiplayerChat.h"
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "game/weather.h"
#include "network/arrays/NetworkArrayMgr.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "network/Cloud/UserContentManager.h"
#include "network/Crews/NetworkCrewDataMgr.h"
#include "network/Debug/NetworkBot.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Debug/NetworkSoakTests.h"
#include "network/events/NetworkEventTypes.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "network/general/NetworkFakePlayerNames.h"
#include "Network/General/NetworkHasherUtil.h"
#include "Network/General/NetworkScratchpad.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "network/Live/livemanager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Party/NetworkParty.h"
#include "network/players/NetworkPlayerMgr.h"
#include "network/roaming/RoamingBubbleMgr.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/Shop/Catalog.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/Stats/networkleaderboardsessionmgr.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "network/Live/NetworkLegalRestrictionsManager.h"
#include "peds/PedPopulation.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerInfo.h"
#include "pickups/PickupManager.h"
#include "scene/DataFileMgr.h"
#include "scene/FileLoader.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/texlod.h"
#include "scene/world/GameWorld.h"
#include "script/commands_datafile.h"
#include "script/commands_network.h"
#include "script/script_hud.h"
#include "streaming/IslandHopper.h"
#include "streaming/PopulationStreaming.h"
#include "system/controlMgr.h"
#include "system/memvisualize.h"
#include "system/memmanager.h"
#include "system/InfoState.h"
#include "vehicles/vehiclepopulation.h"
#include "task/Physics/TaskNM.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Default/TaskIncapacitated.h"
#include "task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "task/Movement/TaskGetUp.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "task/Weapons/WeaponTarget.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "text/messages.h"
#include "Vehicles/Bike.h"
#include "Vehicles/cargen.h"
#include "Vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vfx/misc/MovieManager.h"
#include "vfx/vehicleglass/VehicleGlassComponent.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "weapons/Bullet.h"
#include "weapons/Weapon.h"
#include "stats/StatsInterface.h"
#include "stats/StatsDataMgr.h"
#include "stats/StatsMgr.h"
#include "stats/StatsSavesMgr.h"
#include "Objects/Door.h"
#include "stats/MoneyInterface.h"
#include "renderer/RenderTargetMgr.h"
#include "vehicles/vehicleDamage.h"

#include "frontend/UIWorldIcon.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/WarningScreen.h"

//ORBIS
#if RSG_ORBIS
#include <game_live_streaming.h>
#include <libsysmodule.h>
#pragma comment(lib,"SceGameLiveStreaming_stub_weak")

// Provided by Sony to specifically detect if we're running in back-compat mode
#include "system/kernel_cpumode_platform.h"
#pragma comment(lib,"kernel_cpumode_platform_stub_weak")
#endif

#if !__FINAL && RSG_ORBIS
#include <libdbg.h>
#endif

#if RSG_PC
#include "net/natdetector.h"
#include "net/natpcp.h"
#include "net/natupnp.h"
#include "net/tunneler_ice.h"
#endif

NETWORK_OPTIMISATIONS()

RAGE_DECLARE_CHANNEL(net);
#undef __net_channel
#define __net_channel net

// these prevent spam in the net channel
RAGE_DEFINE_SUBCHANNEL(net, tod, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)

#define netTodDebug(fmt, ...)	RAGE_DEBUGF1(net_tod, fmt, ##__VA_ARGS__)
#define netTodError(fmt, ...)	RAGE_ERRORF(net_tod, fmt, ##__VA_ARGS__)
#define netTodVerify(cond)		RAGE_VERIFY(net_tod, cond)

// these prevent spam in the net channel
RAGE_DEFINE_SUBCHANNEL(net, cxnevent, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netCxnEventDebug(fmt, ...)	RAGE_DEBUGF1(net_cxnevent, fmt, ##__VA_ARGS__)

RAGE_DEFINE_SUBCHANNEL(net, transition, DIAG_SEVERITY_DEBUG3)
#define netTransitionDebug(fmt, ...)			RAGE_DEBUGF1(net_transition, fmt, ##__VA_ARGS__)
#define netTransitionAssertf(cond, fmt, ...)	RAGE_ASSERTF(net_transition,cond, fmt, ##__VA_ARGS__)

// The Network update function can be called recursively during the loading
// We don't profile functions during loading to avoid pushing too many profilers
#define UPDATE_PROFILE(_functionName)		if(fromLoadingCode) { _functionName; } else { NPROFILE(_functionName); }
#define UPDATE_PROFILE_BLOCK(_blockName)	NPROFILE_BLOCK_COND(_blockName, fromLoadingCode);

// rage default is rage::netConnectionPolicies::DEFAULT_TIMEOUT_INTERVAL, which is different than this one. Timer can change via cloud
int CNetwork::DEFAULT_CONNECTION_TIMEOUT = netConnectionPolicies::DEFAULT_TIMEOUT_INTERVAL;
int CNetwork::DEFAULT_CONNECTION_KEEP_ALIVE_INTERVAL = netConnectionPolicies::DEFAULT_KEEP_ALIVE_INTERVAL;

static bool s_EnableCodeBailAlerting = false;

namespace rage
{
	extern sysMemAllocator* g_rlAllocator;
	extern const rlTitleId* g_rlTitleId;
	XPARAM(nopopups);
}

//static const unsigned MAX_VOICE_BANDWIDTH = 16*1024/8;  //kbps

NOSTRIP_FINAL_LOGGING_PARAM(socketport, "[network] 8-bit network port on which to communicate");
PARAM(notimeouts, "[network] Disable timeouts for network sessions");
PARAM(timeout, "[network] Sets the timeout period to a number of seconds");
PARAM(nonetwork, "[network] No network");
PARAM(netForceCheater, "[network] Force local player cheater setting (0 or 1)");
PARAM(nompvehicles, "[network] Don't switch to the lower-LOD vehicles in MP");
PARAM(disableLeavePed, "[network] Disable leave ped behind when a player leaves the session");
PARAM(netFunctionProfilerDisabled, "[network] Disables netFunctionProfiler");
PARAM(netFunctionProfilerAsserts, "[network] Asserts when function profiler exceeds supplied times");
PARAM(netFunctionProfilerThreshold, "[network] Only outputs function that took more than supplied ms");
PARAM(netSendStallTelemetry, "[network] Only outputs function that took more than supplied ms");
PARAM(netDisableLoggingStallTracking, "[network] Disables functionality to track logging stalls");
XPARAM(lan);

PARAM(netBypassMultiplayerAccessChecks, "[network] Bypass soft network access blockers");
PARAM(netRequireSpPrologue, "[network] Force a setting for requiring the SP prologue");
PARAM(netSpPrologueComplete, "[network] Force that the SP prologue is complete");
PARAM(netMpSkipCloudFileRequirements, "[session] If present, skips need for tunables or background scripts");
XPARAM(mpavailable);

#if RSG_XBOX
PARAM(netXblUseConnectivityForLinkConnected, "[network] Whether we use the network connectivity to drive whether we consider the cable connected");
#endif

PARAM(netObjectIdRequestIdThreshold, "[network] Object Id request Id release threshold");
PARAM(netObjectIdNumIdsToRequest, "[network] Object Id number of Ids that we request when ");
PARAM(netObjectIdMinTimeBetweenRequests, "[network] Object Id minimum time between Id requests");
PARAM(netObjectIdObjectIdReleaseThreshold, "[network] Object Id release threshold");

// starting position vector in the network.
PARAM(netStartPos, "[network] Set the player start position.");

// starting team in the network.
PARAM(netTeam, "[network] Set the player's team.");

PARAM(netHttpRequestTrackerDisplayOnScreenMessage, "[network] The HTTP request tracker will display a message on screen if there is a burst.");
XPARAM(mapMoverOnlyInMultiplayer);
PARAM(netNoScAuth, "Disables ScAuth signup and account linking");
PARAM(netScAuthNoExit, "Disable exiting the ScAuth flow through BACK button, requires browser javascript exit");
PARAM(netTestTunableConnectionMgrHeapSwap, "Tests swapping out the heap at the point of loading tunables");

#if RSG_ORBIS
PARAM(netForceBackCompatAsPS5, "[network] Force acting as playing on PS5 with back compat", "Disabled", "PS4", "");
PARAM(PS4BackwardsCompatibility, "Force acting as playing on PS5 with back compat", "Disabled", "PS4", "");
#endif

#if __BANK
PARAM(enableSaveMigration, "[network] Enable Single Player savegame migration even if the Tunable is not set");
#endif // __BANK

// Network Allocator
static __THREAD sysMemAllocator* s_NetworkAllocator;
static __THREAD s32 s_heapStackSize;

// Stall detection tunables
static const unsigned DEFAULT_LONG_FRAME_TIME = 100; 
static unsigned s_nLongFrameTime = DEFAULT_LONG_FRAME_TIME;
static const unsigned DEFAULT_STALL_DETECTION_SP = 1000;
static const unsigned DEFAULT_STALL_DETECTION_MP = 1000;
static unsigned s_nTimeForLongStallTelemetrySP = DEFAULT_STALL_DETECTION_SP;
static unsigned s_nTimeForLongStallTelemetryMP = DEFAULT_STALL_DETECTION_MP;

static const unsigned DEFAULT_ENDPOINT_POOL_HINT = MAX_NUM_ACTIVE_PLAYERS;
static const unsigned DEFAULT_CXN_POOL_HINT = DEFAULT_ENDPOINT_POOL_HINT * 2;

//bail
struct AlertParameters
{
	AlertParameters()
		: m_IsActive(false)
		, m_ResolveRequiresOnline(false)
		, m_InstanceHash(0)
	{

	}

	sNetworkErrorDetails                        m_ErrorDetails;
	bool										m_IsActive;
	bool                                        m_ResolveRequiresOnline;
	unsigned                                    m_InstanceHash;

	void SetActive()
	{
		m_IsActive = true;
	}

	bool HasActiveMessage() const
	{
		return m_IsActive;
	}

	void ClearMessage()
	{
		m_ResolveRequiresOnline = false;
		m_InstanceHash = 0;
		m_IsActive = false;
	}
};

static AlertParameters s_NetworkAlertParams;

void sysMemStartNetwork()
{
	gnetAssertf(CNetwork::GetNetworkHeap(), "Network heap doesn't exist");
	gnetAssertf(s_heapStackSize >= 0, "Invalid Network heap stack size: %d", s_heapStackSize);

	if (CNetwork::GetNetworkHeap() && s_heapStackSize == 0)
	{
		if (CNetwork::GetNetworkHeap() == &sysMemAllocator::GetCurrent())
		{
			Quitf(ERR_NET_MEM,"Network Heap is already in use! This is very bad and we will crash now...");
		}

		s_NetworkAllocator = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(*CNetwork::GetNetworkHeap());
	}

	s_heapStackSize++;
}

void sysMemEndNetwork()
{
	gnetAssertf(CNetwork::GetNetworkHeap(), "Network heap doesn't exist");
	gnetAssertf(s_heapStackSize > 0, "Invalid Network heap stack size: %d", s_heapStackSize);
	
	s_heapStackSize--;

	if (CNetwork::GetNetworkHeap() && s_heapStackSize == 0)
	{
	sysMemAllocator::SetCurrent(*s_NetworkAllocator);
	s_NetworkAllocator = NULL;
}
}

//PURPOSE
// Private class supporting game level queries from the framework level
class NetQueryFunctions : public netInterface::queryFunctions
{
public:

    NetQueryFunctions()  {}
    ~NetQueryFunctions() {}

	bool IsSnSessionEstablished() const
	{
		return CNetwork::GetNetworkSession().GetSnSession().IsEstablished();
	}

	bool IsSessionEstablished() const
    {
        return CNetwork::GetNetworkSession().IsSessionEstablished();
    }

    unsigned GetNetworkTime()
    {
        return CNetwork::GetNetworkTime();
    }

	void FlushAllLogFiles(bool waitForFlush)
	{
		return CNetwork::FlushAllLogFiles(waitForFlush);
	}
};

#if RSG_PC
NetworkExitFlow::NetworkExitFlow() 
	: m_ExitToDesktopState(ETD_NONE)
	, m_fromMP(false)
{

}

bool NetworkExitFlow::IsExitFlowRunning()
{
	return m_ExitToDesktopState != ETD_NONE;
}

void NetworkExitFlow::StartExitSaveFlow()
{
	gnetDebug1("Requested Exit MP to Desktop (Save Flow)");
	if(gnetVerifyf(!IsSaveFlowRunning() && !IsShutdownTasksRunning(), "Exit to desktop flow is already running"))
	{
		if(NetworkInterface::IsGameInProgress())
		{
			SetShutdownState(ETD_WAITING_FOR_SAVE_READY);
			StatsInterface::SetNoRetryOnFail(true);
			m_fromMP=true;
		}
	}
}

bool NetworkExitFlow::IsSaveFlowRunning()
{
	return m_ExitToDesktopState == ETD_WAITING_FOR_SAVE_FINISHED || m_ExitToDesktopState == ETD_WAITING_FOR_SAVE_READY;
}

bool NetworkExitFlow::IsSaveFlowFinished()
{
	return m_ExitToDesktopState == ETD_SAVE_SUCCESS || m_ExitToDesktopState == ETD_SAVE_FAILED;
}

void NetworkExitFlow::StartShutdownTasks()
{
	gnetDebug1("Requested Exit MP to Desktop (Shutdown Tasks)");
	if(gnetVerifyf(!IsShutdownTasksRunning(), "Shutdown tasks are already running"))
	{
		// Don't attempt to execute NetworkExit tasks with -nonetwork
		if (PARAM_nonetwork.Get())
		{
			SetShutdownState(ETD_READY_TO_SHUTDOWN);
		}
		else
		{
			SetShutdownState(ETD_SHUTDOWN_START);

			if(g_rlPc.GetUiInterface())
			{
				const int SHUTDOWN_TIMEOUT_MS = (3 * 1000);
				g_rlPc.GetUiInterface()->BeginShutdown(SHUTDOWN_TIMEOUT_MS);
			}
		}
		
		m_fromMP = NetworkInterface::IsGameInProgress();
	}
}

bool NetworkExitFlow::IsShutdownTasksRunning()
{
	return m_ExitToDesktopState >= ETD_SHUTDOWN_START && m_ExitToDesktopState < ETD_READY_TO_SHUTDOWN;
}

bool NetworkExitFlow::IsReadyToShutdown()
{
	return m_ExitToDesktopState == ETD_READY_TO_SHUTDOWN;
}

NetworkExitFlow::ExitToDesktopState NetworkExitFlow::GetExitFlowState()
{
	return m_ExitToDesktopState;
}

void NetworkExitFlow::ResetExitFlowState()
{
	gnetDebug1("Reset Exit to desktop");
	gnetAssertf(m_ExitToDesktopState == ETD_NONE || IsSaveFlowFinished() || IsReadyToShutdown(), "Exit to desktop flow is not finished yet");
	SetShutdownState(ETD_NONE);
}

void NetworkExitFlow::SetShutdownState(ExitToDesktopState s)
{
#if !__NO_OUTPUT
	static const char *s_StateNames[] =
	{
		"ETD_NONE",
		"ETD_WAITING_FOR_SAVE_READY",
		"ETD_WAITING_FOR_SAVE_FINISHED",
		"ETD_SAVE_FAILED",
		"ETD_SAVE_SUCCESS",
		"ETD_SHUTDOWN_START",
		"ETD_FLUSH_TELEMETRY",
		"ETD_UNADVERTISE_MM",
		"ETD_UNADVERTISING",
		"ETD_SESSION_SHUTDOWNS",
		"ETD_SC_SIGNOUT",
		"ETD_WAITING_FOR_SC_SIGNOUT",
		"ETD_WAITING_FOR_UPNP_DELETE",
		"ETD_WAITING_FOR_PCP_DELETE",
		"ETD_WAITING_FOR_SCUI_SHUTDOWN",
		"ETD_READY_TO_SHUTDOWN"
	};
	CompileTimeAssert(COUNTOF(s_StateNames) == ETD_READY_TO_SHUTDOWN + 1);
	gnetDebug1("SetShutdownState(%s)", s_StateNames[(int)s]);
#endif

	m_ExitToDesktopState = s;
}

void NetworkExitFlow::Update()
{
	if(m_ExitToDesktopState!=ETD_NONE)
	{
		if(!StatsInterface::SavePending() && 
			!StatsInterface::PendingSaveRequests() && 
			!CLiveManager::GetProfileStatsMgr().PendingFlush() && 
			!CLiveManager::GetProfileStatsMgr().PendingMultiplayerFlushRequest())
		{
			if(m_ExitToDesktopState == ETD_WAITING_FOR_SAVE_READY)
			{
				StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_IMMEDIATE_FLUSH, 0);
				SetShutdownState(ETD_WAITING_FOR_SAVE_FINISHED);
			}
			else if(m_ExitToDesktopState == ETD_WAITING_FOR_SAVE_FINISHED)
			{
				if(StatsInterface::CloudSaveFailed(STAT_MP_CATEGORY_DEFAULT) || CLiveManager::GetProfileStatsMgr().FlushFailed())
				{
					SetShutdownState(ETD_SAVE_FAILED);
					StatsInterface::ClearSaveFailed(STAT_MP_CATEGORY_DEFAULT);
					StatsInterface::SetNoRetryOnFail(false);
				}
				else
				{
					SetShutdownState(ETD_SAVE_SUCCESS);
					StatsInterface::SetNoRetryOnFail(false);
				}
			}
		}
		
		if (m_ExitToDesktopState == ETD_SHUTDOWN_START)
		{
			// deleting the uPnP mapping can take several seconds and can run in parallel with other tasks
			netNatUpNp::DeleteUpNpPortForwardRule(&m_UpNpStatus);
			netNatPcp::DeletePcpAddressReservation(&m_PcpStatus);

			if(CNetworkTelemetry::FlushTelemetry(true))
			{
				SetShutdownState(ETD_FLUSH_TELEMETRY);
			}
			else SetShutdownState(ETD_UNADVERTISE_MM);
		}
		else if (m_ExitToDesktopState == ETD_FLUSH_TELEMETRY)
		{
			if (!rlTelemetry::FlushInProgress())
			{
				SetShutdownState(ETD_UNADVERTISE_MM);
			}
		}
		else if (m_ExitToDesktopState == ETD_UNADVERTISE_MM)
		{
			if (rlScMatchManager::HasAnyMatches())
			{
				if(gnetVerifyf(rlScMatchmaking::UnadvertiseAll(0, &m_MyStatus), "Unadvertise matchmaking failed"))
				{
					SetShutdownState(ETD_UNADVERTISING);
				}
				else SetShutdownState(ETD_SESSION_SHUTDOWNS);
			}
			else SetShutdownState(ETD_SESSION_SHUTDOWNS);
		}
		else if (m_ExitToDesktopState == ETD_UNADVERTISING)
		{
			if (!m_MyStatus.Pending())
			{
				SetShutdownState(ETD_SESSION_SHUTDOWNS);
			}
		}
		else if (m_ExitToDesktopState == ETD_SESSION_SHUTDOWNS)
		{
			// Let others know we're leaving
			CNetwork::Bail(sBailParameters(BAIL_EXIT_GAME, BAIL_CTX_EXIT_GAME_FROM_FLOW));

			// Cancel all non essential tasks for shutdown
			rlGetTaskManager()->ShutdownCancel();
			netTask::CancelAll();
			SetShutdownState(ETD_SC_SIGNOUT);
		}
		else if (m_ExitToDesktopState == ETD_SC_SIGNOUT)
		{
			rlScSignOutTask* task = NULL;
			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			if (NetworkInterface::IsLocalPlayerSignedIn() &&
				rlGetTaskManager()->CreateTask(&task) && 
				rlTaskBase::Configure(task, localGamerIndex, &m_MyStatus) &&
			    rlGetTaskManager()->AddParallelTask(task))
			{
				SetShutdownState(ETD_WAITING_FOR_SC_SIGNOUT);
			}
			else
			{
				if (task != NULL)
				{
					rlGetTaskManager()->DestroyTask(task);
				}

				SetShutdownState(ETD_WAITING_FOR_UPNP_DELETE);
			}
		}
		else if (m_ExitToDesktopState == ETD_WAITING_FOR_SC_SIGNOUT)
		{
			if (!m_MyStatus.Pending())
			{
				SetShutdownState(ETD_WAITING_FOR_UPNP_DELETE);
			}
		}
		else if (m_ExitToDesktopState == ETD_WAITING_FOR_UPNP_DELETE)
		{
			if (!m_UpNpStatus.Pending())
			{
				SetShutdownState(ETD_WAITING_FOR_PCP_DELETE);
			}
		}
		else if (m_ExitToDesktopState == ETD_WAITING_FOR_PCP_DELETE)
		{
			if (!m_PcpStatus.Pending())
			{
				SetShutdownState(ETD_WAITING_FOR_SCUI_SHUTDOWN);
			}
		}
		else if (m_ExitToDesktopState == ETD_WAITING_FOR_SCUI_SHUTDOWN)
		{
			if (g_rlPc.GetRgscInterface() == NULL || g_rlPc.GetRgscInterface()->IsReadyToShutdown())
			{
				SetShutdownState(ETD_READY_TO_SHUTDOWN);
			}
		}
	}
}

bool NetworkExitFlow::FromMP()
{
	return m_fromMP;
}

const char* NetworkExitFlow::GetExitString()
{
	return (m_fromMP) ? "WARNING_EXIT_SESSION_SHUTDOWN" : "WARNING_EXIT_GAME";
}

#endif

//////////////////////////////////////////////////////////////////////////
// Member vars
//////////////////////////////////////////////////////////////////////////
// EJ: size of heap session for network (combined snet and contextmgr heaps)
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
const int CNetwork::SCXNMGR_HEAP_SIZE = (650 * 1024);
#else
const int CNetwork::SCXNMGR_HEAP_SIZE       = (128 * 1024);
#endif

static u8                    *s_SCxnMgrHeap      = 0;
static netMemAllocator		 *s_SCxnMgrAllocator = 0;

bool                                CNetwork::sm_bMultiplayerLocked = NETWORK_LOCKED_DEFAULT;
bool                                CNetwork::sm_bSpPrologueRequired = !RSG_GEN9;
bool								CNetwork::sm_bFirstEntryNetworkOpen = false;
bool                                CNetwork::sm_bNetworkOpen = false;
bool								CNetwork::sm_bNetworkOpening = false;
bool								CNetwork::sm_bNetworkClosing = false;
bool								CNetwork::sm_bNetworkClosePending = false;
bool								CNetwork::sm_bNetworkClosePendingIsFull = false;
bool								CNetwork::sm_bAllowPendingCloseNetwork = true;
bool								CNetwork::sm_bFatalErrorForDoubleOpen = true;
bool								CNetwork::sm_bFatalErrorForCloseDuringOpen = true;
bool								CNetwork::sm_bFatalErrorForStartWhenClosed = true;
bool								CNetwork::sm_bFatalErrorForStartWhenFullyClosed = true;
bool								CNetwork::sm_bFatalErrorForStartWhenManagersShutdown = true;
bool								CNetwork::sm_bFatalErrorForInvalidPlayerManager = true; 

unsigned							CNetwork::sm_nMinStallMsgTotalThreshold = 1000;
unsigned							CNetwork::sm_nMinStallMsgNetworkThreshold = 100;

bool								CNetwork::sm_IsBailPending = false;
bool								CNetwork::sm_AlertScriptForBail = false;
bool								CNetwork::sm_PendingBailWarningScreen = false;
sBailParameters						CNetwork::sm_BailParams;

bool								CNetwork::sm_bScriptReadyForEvents = false;
bool                                CNetwork::sm_bIsScriptAutoMuted = false;
bool                                CNetwork::sm_bInMPTutorial = false;
bool                                CNetwork::sm_bInMPCutscene = false;
bool                                CNetwork::sm_bNetworkCutsceneEntities = false;
bool                                CNetwork::sm_bFriendlyFireAllowed = false;
bool                                CNetwork::sm_bManagersAreInitialised = false;
bool								CNetwork::sm_bFirstEntryMatchStarted = false;
bool                                CNetwork::sm_bMatchStarted = false;
bool                                CNetwork::sm_bGameInProgress = false;
bool                                CNetwork::sm_bGoStraightToMultiplayer = false;
bool                                CNetwork::sm_bGoStraightToMpEvent = false;
bool                                CNetwork::sm_bGoStraightToMPRandomJob = false;
bool								CNetwork::sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag = true;
bool                                CNetwork::sm_bCollisionAroundLocalPlayer = false;
bool								CNetwork::sm_bSkipRadioResetNextClose = false;
bool								CNetwork::sm_bSkipRadioResetNextOpen = false;
bool								CNetwork::sm_bLiveStreamingInitialised = false;
unsigned                            CNetwork::sm_bDisableCarGeneratorTime = 0;
u16                                 CNetwork::sm_nGameMode = MultiplayerGameMode::GameMode_Freeroam;
int                                 CNetwork::sm_nGameModeState = MultiplayerGameModeState::GameModeState_None;

#if __BANK
bool								CNetwork::sm_bSaveMigrationEnabled = false;
#endif // __BANK

char                                CNetwork::sm_logFileBuffer[256];
static netLog                       *s_MessageLog = nullptr;

static CNetworkBandwidthManager     s_BandwidthMgr;
static netTimeSync                  s_networkClock;
static CNetworkPlayerMgr            s_PlayerMgr(s_BandwidthMgr);
static CNetworkVoiceSession         s_NetworkVoiceSession;
static CNetworkScratchpads			s_ScriptScratchpads;
static CRoamingBubbleMgr            s_RoamingBubbleMgr;
static NetQueryFunctions            s_QueryFunctions;
static CNetworkLeaderboardMgr		s_LeaderboardMgr;
static CNetworkVoice				s_NetworkVoice;

#if __WIN32PC
static CNetworkTextChat				s_NetworkTextChat;
#endif

static bool                         s_NetworkCoreInitialised  = false;

#if ENABLE_NETWORK_LOGGING
static bool                         s_AssertionFiredLastFrame = false;
#endif // ENABLE_NETWORK_LOGGING

netTimeSync&                        CNetwork::sm_networkClock(s_networkClock);
CNetworkObjectMgr*                  CNetwork::sm_ObjectMgr = 0;
netEventMgr*                        CNetwork::sm_EventMgr  = 0;
CGameArrayMgr*                      CNetwork::sm_ArrayMgr  = 0;
CNetworkBandwidthManager&           CNetwork::sm_BandwidthMgr(s_BandwidthMgr);
CNetworkPlayerMgr&                  CNetwork::sm_PlayerMgr(s_PlayerMgr);
CNetworkSession*                    CNetwork::sm_NetworkSession = 0;
CNetworkParty*						CNetwork::sm_NetworkParty = nullptr;
CNetworkVoiceSession&               CNetwork::sm_NetworkVoiceSession(s_NetworkVoiceSession);
CNetworkLeaderboardMgr&				CNetwork::sm_LeaderboardMgr(s_LeaderboardMgr);
CNetworkScratchpads&                CNetwork::sm_ScriptScratchpads(s_ScriptScratchpads);
CRoamingBubbleMgr&                  CNetwork::sm_RoamingBubbleMgr(s_RoamingBubbleMgr);
CNetworkVoice&						CNetwork::sm_NetworkVoice(s_NetworkVoice);

#if __WIN32PC
CNetworkTextChat&					CNetwork::sm_NetworkTextChat(s_NetworkTextChat);
NetworkExitFlow						CNetwork::sm_ExitToDesktopFlow;
#endif

netConnectionManager                CNetwork::sm_CxnMgr;
netConnectionManager::Delegate      CNetwork::sm_CxnMgrGameDlgts[NET_MAX_CHANNELS];

#if !__NO_OUTPUT
netHttpRequestTracker::Delegate		CNetwork::sm_RequestTrackerDlgt;
#endif

unsigned                            CNetwork::sm_syncedTimeInMilliseconds = 0;
unsigned                            CNetwork::sm_lockedNetworkTime = 0;
unsigned                            CNetwork::sm_NetworkTimeWhenDebugPaused = 0;
unsigned                            CNetwork::sm_EventTriggerLastUpdate = 0;
bool                                sm_WasDebugPausedLastFrame = false;

unsigned                            s_PausableNetworkTimeOffset      = 0;
unsigned                            s_TimePausableNetworkTimeStopped = 0;
bool                                s_PausableNetworkTimeStopped     = false;

bool                                CNetwork::sm_IsInTutorialSession = false;
bool                                s_WasTutorialSessionNetworkTurn  = false;
unsigned                            s_NumPlayersInTutorialSession    = 0;
unsigned                            s_TutorialSessionNetworkTurn     = 0;
unsigned                            s_TutorialSessionTurnDelay       = 0;

bool                                CNetwork::sm_ThereAreConcealedPlayers = false;
bool                                s_WasConcealedPlayerNetworkTurn  = false;
unsigned                            s_NumPlayersNotConcealedToMe    = 0;
unsigned                            s_ConcealedPlayerNetworkTurn     = 0;
unsigned                            s_ConcealedPlayerTurnDelay       = 0;

static CNetworkAssetVerifier        s_AssetVerifier;
CNetworkAssetVerifier&              CNetwork::sm_AssetVerifier(s_AssetVerifier);

CNetworkTunablesListener* 			CNetwork::m_pTunablesListener = NULL;

//Calculate average RTT over time for the multiplayer game
static const int DEFAULT_RTT_CHECK_INTERVAL = 1 * 60000; //interval between checking RTT values again
static const unsigned DEFAULT_INITIAL_RTT_CHECK_INTERVAL = 5000; //interval between checking RTT values again
static const unsigned DEFAULT_INITIAL_RTT_SAMPLES = 10; //initial number of samples gathered every DEFAULT_INITIAL_RTT_CHECK_INTERVAL
u32                                 CNetwork::sm_averageRTT = 0;
u32                                 CNetwork::sm_averageRTTWorst = 0;
u32                                 CNetwork::sm_averageRTTSamples = 0;
u32                                 CNetwork::sm_averageRTTInterval = 0;

int									CNetwork::m_nClockHour = 0;
int                                 CNetwork::m_nClockMinute = 0;
int                                 CNetwork::m_nClockSecond = 0;
unsigned                            CNetwork::m_nClockTransitionTimeLeft = 0;
unsigned                            CNetwork::m_nClockAndWeatherTransitionTime = 0;
int                                 CNetwork::m_LastClockUpdateTime = 0;
int                                 CNetwork::m_NextClockSyncTime = 0;
bool                                CNetwork::sm_bAppliedGlobalSettings = false;

bool								CNetwork::sm_lastDamageEventPendingOverrideEnabled = true;

// network heap
class sysMemAllocator*              CNetwork::sm_pNetworkHeap=NULL;
void*                               CNetwork::sm_pHeapBuffer=NULL;
int                                 CNetwork::sm_HeapBufferRefCount=0;
#if __BANK
const unsigned                      MAX_CATEGORY_STACK_LEVELS = 5;

NetworkMemoryCategory               s_MemCategoryStack[MAX_CATEGORY_STACK_LEVELS] =
{
    NET_MEM_NUM_MANAGERS,
    NET_MEM_NUM_MANAGERS,
    NET_MEM_NUM_MANAGERS,
    NET_MEM_NUM_MANAGERS,
    NET_MEM_NUM_MANAGERS
};

unsigned                            s_CurrentMemCategory = 0;
int                                 s_PreviousHeapUsage  = 0;
#endif // __BANK

static bool s_SendTransitionTrackerTelemetry = true; 

//Cpu affinity for connection manager.
static const int CXN_MGR_CPU_AFFINITY   = 0;

// Maximum size of network log files
static const unsigned MAXIMUM_LOG_FILE_SIZE = 300 * 1024;

///////////////////////////////////////////////////////////////////////////////
// Logging file access interface
///////////////////////////////////////////////////////////////////////////////
class CNetworkLogFileAccess : public netLogFileAccessInterface
{
public:

    virtual packedLogDataWriter *CreatePackedLogDataWriter(char     *LOGGING_ONLY(logBuffer),
                                                           unsigned  LOGGING_ONLY(logBufferSize))
    {
#if ENABLE_NETWORK_LOGGING
        return rage_new packedLogDataWriter(logBuffer, logBufferSize);
#else
		return 0;
#endif // ENABLE_NETWORK_LOGGING
    }

    virtual packedLogDataReader *CreatePackedLogDataReader(const char                           *LOGGING_ONLY(packedLogData),
                                                           unsigned                              LOGGING_ONLY(packedDataSize),
                                                           netLogFileAccessInterface::LogHandle &LOGGING_ONLY(logHandle))
    {
#if ENABLE_NETWORK_LOGGING
        return rage_new packedLogDataReader(packedLogData, packedDataSize, *this, logHandle);
#else
		return 0;
#endif // ENABLE_NETWORK_LOGGING
    }

    virtual LogHandle OpenFile(const char *filename, LogOpenType openType)
    {
		FileHandle hFile = INVALID_FILE_HANDLE;
		
		int retryCount = 3; 

		while (retryCount > 0 && hFile == INVALID_FILE_HANDLE)
		{
			switch(openType)
			{
			case LOGOPEN_CREATE:
				hFile = CFileMgr::OpenFileForWriting(filename);
				break;
			case LOGOPEN_APPEND:
				{
					hFile = CFileMgr::OpenFileForAppending(filename);

					if(hFile == INVALID_FILE_HANDLE)
					{
						hFile = CFileMgr::OpenFileForWriting(filename);
					}
				}
				break;
			default:
				gnetAssertf(0, "OpenFile :: Invalid Open Type!");
				break; 
			}

			// Retry for up to 3 times (3 seconds in total). This is for when the new log grabber locks the log files to copy them 
			// but they are already overflowing into the next one. This will freeze the game for that period, but we would crash anyway if the logs don't get created. 
			if (hFile == INVALID_FILE_HANDLE)
			{
				gnetWarning("CNetworkLogFileAccess::OpenFile Invalid file handle while trying to open file %s (Open Type: %d). Retrying! This will assert if 3 retries fail! Retry count pre-decrement: %d", 
					filename, openType, retryCount);
				sysIpcSleep(1000);
				retryCount--;
			}		
		}	

		gnetAssertf(hFile != INVALID_FILE_HANDLE, "OpenFile :: Invalid handle for %s. Open Type: %d", filename, openType);

        return static_cast<LogHandle>(hFile);
    }

    virtual void CloseFile(LogHandle hLog)
    {
		FileHandle hFile = static_cast<LogHandle>(hLog);
		if(!gnetVerifyf(hFile != INVALID_FILE_HANDLE, "CloseFile :: Invalid handle"))
			return;

        CFileMgr::CloseFile(hFile);
    }

    virtual void Flush(LogHandle hLog)
    {
		FileHandle hFile = static_cast<LogHandle>(hLog);
		if(!gnetVerifyf(hFile != INVALID_FILE_HANDLE, "Flush :: Invalid handle"))
			return;

		CFileMgr::Flush(hFile);
    }

    virtual void Write(LogHandle hLog, const char *logText, unsigned textSize)
    {
		FileHandle hFile = static_cast<LogHandle>(hLog);
		if(!gnetVerifyf(hFile != INVALID_FILE_HANDLE, "Write :: Invalid handle"))
			return;

		CFileMgr::Write(hFile, logText, textSize);
    }
};

static CNetworkLogFileAccess s_LogFileAccessInterface;

///////////////////////////////////////////////////////////////////////////////
// ScriptValues
///////////////////////////////////////////////////////////////////////////////

void CNetwork::ScriptValues::Clear()
{
    sysMemSet(values, 0, sizeof(values));
}

///////////////////////////////////////////////////////////////////////////////
// Clock Override Data
///////////////////////////////////////////////////////////////////////////////
struct ClockOverrideData
{
	enum eOverrideType {
		OVERRIDE_NONE,
		OVERRIDE_TIME,
		OVERRIDE_RATE,
	};

    ClockOverrideData() :
    m_Active(OVERRIDE_NONE)
    , m_Hour(0)     
    , m_Min(0)           
    , m_Second(0)
    , m_TimeMinLastAdded(0)
	, m_bFromScript(false)
	, m_bLocked(false)
    {
    }

    bool IsActive() const { return m_Active != OVERRIDE_NONE; }
	bool IsFromScript() const { return m_bFromScript; }
	bool IsLocked() const { return m_bLocked; }

    void Reset()
    {
#if !__NO_OUTPUT
		if (m_bLocked)
		{
			gnetError("Attempting to reset clock override with lock in place");
#if !__FINAL
			diagDefaultPrintStackTrace();
#endif
		}
#endif

        m_Active           = OVERRIDE_NONE;
        m_Hour             = 0;
        m_Min              = 0;
        m_Second           = 0;
        m_TimeMinLastAdded = 0;
		m_bFromScript      = false;
		m_bLocked          = false;
    }

    void Override(int hour, int min, int second, bool bFromScript)
    {
#if !__NO_OUTPUT
		if (m_bLocked)
		{
			gnetError("Attempting to override clock with lock in place");
#if !__FINAL
			diagDefaultPrintStackTrace();
#endif
		}
#endif

        if(!m_Active)
        {
            m_Active           = OVERRIDE_TIME;
            m_Hour             = CClock::GetHour();
            m_Min              = CClock::GetMinute();
            m_Second           = CClock::GetSecond();
            m_TimeMinLastAdded = CClock::GetTimeLastMinuteAdded();
        }

		m_bFromScript = bFromScript;

        gnetAssertf(CClock::GetMode()==CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");
	    CClock::SetTime(hour, min, second);
		CClock::SetMsPerGameMinute( CClock::GetDefaultMsPerGameMinute() );
    }

	void OverrideRate(u32 msPerGameMinute)
	{
#if !__NO_OUTPUT
		if (m_bLocked)
		{
			gnetError("Attempting to override clock with lock in place");
#if !__FINAL
			diagDefaultPrintStackTrace();
#endif
		}
#endif

		if (!m_Active)
		{
			m_Active			= OVERRIDE_RATE;
			m_Hour				= CClock::GetHour();
			m_Min				= CClock::GetMinute();
			m_Second			= CClock::GetSecond();
			m_TimeMinLastAdded	= CClock::GetTimeLastMinuteAdded();
		}

		m_bFromScript = true; // At the momemnt we dont have any other case, always from script

		gnetAssertf(CClock::GetMode() == CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");
		CClock::SetMsPerGameMinute(msPerGameMinute);
	}

    void StopOverriding()
    {
#if !__NO_OUTPUT
		if (m_bLocked)
		{
			gnetError("Attempting to remove clock override with lock in place");
#if !__FINAL
			diagDefaultPrintStackTrace();
#endif
		}
#endif

        switch(m_Active)
        {
		case OVERRIDE_RATE:
			CClock::SetMsPerGameMinute( CClock::GetDefaultMsPerGameMinute() );
			Reset();
			break;
		case OVERRIDE_TIME:
            CClock::SetTime(m_Hour, m_Min, m_Second);
            Reset();
			break;
		case OVERRIDE_NONE: break;
        }
    }

    void GetStoredSessionTime(int &hour, int &min, int &second, int &timeMinLastAdded)
    {
        if(gnetVerifyf(IsActive(), "retrieving stored session time when not overriding the game clock!"))
        {
             hour             = m_Hour;
             min              = m_Min;
             second           = m_Second;
             timeMinLastAdded = m_TimeMinLastAdded;
        }
    }

    void SetStoredSessionTime(int hour, int min, int second, int timeMinLastAdded)
    {
        if(gnetVerifyf(IsActive(), "updating stored session time when not overriding the game clock!"))
        {
            m_Hour             = hour;
            m_Min              = min;
            m_Second           = second;
            m_TimeMinLastAdded = timeMinLastAdded;
        }
	}

	bool IsOverrideLocked() const
	{
		return m_bLocked;
	}

	void SetOverrideLocked(bool bLocked)
	{
		m_bLocked = bLocked;
    }

	eOverrideType m_Active;	 // whether the override is active
    int  m_Hour;             // stored session game clock time in hours when override activated
    int  m_Min;              // stored session game clock time in minutes when override activated
    int  m_Second;           // stored session game clock time in seconds when override activated
    int  m_TimeMinLastAdded; // network time when the session game clock last updated the game clock time in minutes stored here
	bool m_bFromScript;      // whether script applied this override
	bool m_bLocked;          // whether attempts to update the clock override are locked
};

static ClockOverrideData gClockOverride;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNetworkTunablesListener::OnTunablesRead
// PURPOSE: Used to overload network variables from the cloud
/////////////////////////////////////////////////////////////////////////////////////
void CNetworkTunablesListener::OnTunablesRead()
{
	gnetDebug1("CNetworkTunablesListener::OnTunablesRead");
    
	// tunables for this file
	CNetwork::OnTunablesRead();

	// set whether we use time sync token system
	s_SendTransitionTrackerTelemetry = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TELEMETRY_SEND_TRANSITION_TRACKER", 0xedc65aad), s_SendTransitionTrackerTelemetry);

	// set whether we use time sync token system
	bool bUseTimeSyncToken = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("USE_TIME_SYNC_TOKEN", 0x9bef5adb), bUseTimeSyncToken))
		netTimeSync::SetUsingToken(bUseTimeSyncToken);

    // blacklisted gamer timeout
    int nBlacklistTimeout;
    if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("GAMER_BLACKLIST_TIME", 0xe8f181af), nBlacklistTimeout))
        CNetwork::GetNetworkSession().GetBlacklistedGamers().SetBlacklistTimeout(nBlacklistTimeout);

	// presence
	bool bRejectMismatchedAuthenticatedSender = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_PRESENCE_REJECT_MISMATCHED_AUTHENTICATED_SENDER", 0xd3ff022), bRejectMismatchedAuthenticatedSender))
		GamePresenceEvents::SetRejectMismatchedAuthenticatedSender(bRejectMismatchedAuthenticatedSender);

	// cloud tunables
	bool bCloudCacheEncryptedEnabled = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CLOUD_CACHE_ENCRYPTED_ENABLED", 0x237a6924), bCloudCacheEncryptedEnabled))
		CCloudManager::SetCacheEncryptedEnabled(bCloudCacheEncryptedEnabled);

	unsigned nullAllocatorPolicy = NullAllocatorPolicy::Policy_UseRline;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CLOUD_REQUEST_NULL_ALLOCATOR_POLICY", 0xd46f6359), nullAllocatorPolicy))
		netCloudRequestGetFile::SetNullAllocatorPolicy(static_cast<NullAllocatorPolicy>(nullAllocatorPolicy));

	// rson
	bool bAllowRsonSnscanf = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("RSON_ALLOW_SNSCANF", 0xFE3E45D0), bAllowRsonSnscanf))
		Rson::SetAllowSnscanf(bAllowRsonSnscanf);

	// stall detection
#if !__NO_OUTPUT
	s_nLongFrameTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_LONG_FRAME_TIME", 0xf710bb5c), static_cast<int>(DEFAULT_LONG_FRAME_TIME));
#endif
	s_nTimeForLongStallTelemetrySP = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TIME_FOR_STALL_TELEMETRY_SP", 0xec0004f9), static_cast<int>(DEFAULT_STALL_DETECTION_SP));
	s_nTimeForLongStallTelemetryMP = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TIME_FOR_STALL_TELEMETRY_MP", 0xd1b3eeb9), static_cast<int>(DEFAULT_STALL_DETECTION_MP));

#if RSG_GDK
	unsigned inviteResumeGracePeriodMs = 0; // use Access here, we don't know what the default is (behind rlXbl interfaces)
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_INVITE_RESUME_GRACE_PERIOD", 0x57fcdeeb), inviteResumeGracePeriodMs))
		rlXbl::SetInviteResumeGracePeriodMs(inviteResumeGracePeriodMs);
#endif

#if RSG_DURANGO
	// sessions
	unsigned asyncSessionTimeOut	= Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_ASYNC_TIMEOUT", 0x681aedec), ISessions::DEFAULT_ASYNC_SESSION_TIMEOUT_MS);
	unsigned memberReservedTimeout	= Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_MEMBER_RESERVED_TIMEOUT", 0x12196e09), ISessions::DEFAULT_MEMBER_RESERVED_TIMEOUT_MS);
	unsigned memberInactiveTimeout	= Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_MEMBER_INACTIVE_TIMEOUT", 0x8991c699), ISessions::DEFAULT_MEMBER_INACTIVE_TIMEOUT_MS);
	unsigned memberReadyTimeout		= Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_MEMBER_READY_TIMEOUT", 0xbd1dae07), ISessions::DEFAULT_MEMBER_READY_TIMEOUT_MS);
	unsigned sessionEmptyTimeout	= Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_EMPTY_TIMEOUT", 0xdc1f1113), ISessions::DEFAULT_SESSION_EMPTY_TIMEOUT_MS);
	g_rlXbl.GetSessionManager()->SetTimeoutPolicies(asyncSessionTimeOut, memberReservedTimeout, memberInactiveTimeout, memberReadyTimeout, sessionEmptyTimeout);

	bool bShouldCancelCommerceTasks = false;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SHOULD_CANCEL_COMMERCE_TASKS", 0x0d891586), bShouldCancelCommerceTasks))
		rlXbl::SetShouldCancelCommerceTasks(bShouldCancelCommerceTasks);

	bool bRunValidateHost = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_RUN_VALIDATE_HOST", 0x48fe5e12), bRunValidateHost))
		g_rlXbl.GetSessionManager()->SetRunValidateHost(bRunValidateHost);

	bool bConsiderInvalidHostAsInvalidSession = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_SESSION_CONSIDER_INVALID_HOST_AS_INVALID_SESSION", 0x47ba1d09), bConsiderInvalidHostAsInvalidSession))
		g_rlXbl.GetSessionManager()->SetConsiderInvalidHostAsInvalidSession(bConsiderInvalidHostAsInvalidSession);

	// parties
	bool bLeavePartyOnInvalidGameSession = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_PARTY_LEAVE_ON_INVALID_SESSION", 0x3e636e53), bLeavePartyOnInvalidGameSession))
		g_rlXbl.GetPartyManager()->SetLeavePartyOnInvalidGameSession(bLeavePartyOnInvalidGameSession);

	// hardware
	bool bUseConnectivityLevelForLinkConnected = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("XBL_USE_CONNECTIVITY_FOR_LINK_CONNECTED", 0x70e0a15b), bUseConnectivityLevelForLinkConnected))
		netXbl::SetUseConnectivityLevelForLinkConnected(bUseConnectivityLevelForLinkConnected);

	// command line overrides
#if !__FINAL
	if(PARAM_netXblUseConnectivityForLinkConnected.Get())
		netXbl::SetUseConnectivityLevelForLinkConnected(true);
#endif
#endif

#if RL_NP_DEEP_LINK_MANUAL_CLOSE
    unsigned browserDeepLinkCloseTime = 0;
    if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NP_BROWSER_DEEP_LINK_CLOSE_TIME", 0x738406c4), browserDeepLinkCloseTime))
        rlNpCommonDialog::SetBrowserDeepLinkCloseTime(browserDeepLinkCloseTime);
#endif

#if RSG_SCE
    bool dialogFailWhenInitialiseFails = false;
    if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NP_DIALOG_FAIL_WHEN_INITIALISE_FAILS", 0x89502fff), dialogFailWhenInitialiseFails))
        rlNpCommonDialog::SetFailWhenInitialiseFails(dialogFailWhenInitialiseFails);
	bool bReplayShouldRemapNpOnlineIdsToAccountIds = true;
    if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("REPLAY_SHOULD_REMAP_NP_ONLINE_IDS_TO_ACCOUNT_IDS", 0x75537c14), bReplayShouldRemapNpOnlineIdsToAccountIds))
        ReplayFileManager::SetShouldRemapNpOnlineIdsToAccountIds(bReplayShouldRemapNpOnlineIdsToAccountIds);
#endif
	
	// rlGamerHandle
	{
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("RL_GH_ALLOW_EXTRA_VALIDATION_ON_IMPORT", 0xF616982), rlGamerHandle::SetAllowExtraValidationOnImport);
	}

	// tunneler
	{
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_NET_TUNNELER_TELEMETRY", 0x8ABB10FB), netTunneler::SetAllowNetTunnelerTelemetry);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_NET_TUNNELER_ALLOW_KX_RETRY", 0x7B2840E1), netTunneler::SetAllowKeyExchangeRetry);
	}

	// connection manager
	{
		// connection manager main thread policy
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("CXN_ACQUIRE_CRIT_SEC_FOR_FULL_MAIN_UPDATE", 0xb4adfead), netConnectionManager::SetAcquireCritSecForFullMainUpdate);

		// connection endpoint timeouts 	
        AUTO_TUNABLE_CBS(unsigned, netEndpoint::DEFAULT_INACTIVITY_TIMEOUT_MS, ATSTRINGHASH("ENDPOINT_INACTIVITY_TIMEOUT_MS", 0xc58e20f8), netConnectionManager::SetEndpointInactivityTimeoutMs);
        AUTO_TUNABLE_CBS(unsigned, netEndpoint::DEFAULT_CXNLESS_TIMEOUT_MS, ATSTRINGHASH("ENDPOINT_CXNLESS_TIMEOUT_MS", 0x6e546beb), netConnectionManager::SetEndpointCxnlessTimeoutMs);

		// threshold for determining when an endpoint's connection quality is poor
		AUTO_TUNABLE_CBS(unsigned, netEndpoint::DEFAULT_BAD_QOS_THRESHOLD_MS, ATSTRINGHASH("ENDPOINT_BAD_QOS_THRESHOLD_MS", 0x23F2C485), netConnectionManager::SetBadQosThresholdMs);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("CXN_MGR_ENABLE_OVERALL_QOS", 0x83BC2E8E), netConnectionManager::SetEnableOverallQos);

		// connection pending close time
        AUTO_TUNABLE_CBS(unsigned, Cxn::DEFAULT_PENDING_CLOSE_MAX_TIME, ATSTRINGHASH("CXN_DEFAULT_PENDING_CLOSE_MAX_TIME", 0x69438f33), Cxn::SetPendingCloseMaxTime);

		// connection pending close time
		AUTO_TUNABLE_CBS(unsigned, netConnection::DEFAULT_RTT_WARNING_THRESHOLD_MS, ATSTRINGHASH("CXN_RTT_WARNING_THRESHOLD_MS", 0x49BA325F), netConnection::SetRttWarningThresholdMs);

		// enable/disable logic that cancels pending tunnel requests that match active endpoints - leaving code in place for now but disabled by default
		AUTO_TUNABLE_CBS(bool, false, ATSTRINGHASH("CXN_CANCEL_TUNNELS_MATCHING_ACTIVE_ENDPOINTS", 0xC7186B4A), netConnectionManager::SetCancelTunnelsMatchingActiveEndpoints);

		// connection endpoint / connection pools
		unsigned nEndpointPoolHint = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENDPOINT_POOL_HINT", 0xca2cde6d), static_cast<int>(DEFAULT_ENDPOINT_POOL_HINT));
		unsigned nCxnPoolHint = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CXN_POOL_HINT", 0xb2e7a8a5), static_cast<int>(DEFAULT_CXN_POOL_HINT));
		CNetwork::GetConnectionManager().SetPoolSizeHints(nEndpointPoolHint, nCxnPoolHint);
	}

	{
		// ICE Tunneler (NAT traversal tunables)
        AUTO_TUNABLE_CBS(unsigned, netIceTunneler::DEFAULT_NUM_FAILS_BEFORE_WARNING, ATSTRINGHASH("NUM_NAT_FAILS_BEFORE_WARNING", 0xB08126E), netIceTunneler::SetNumNatFailsBeforeWarning);
        AUTO_TUNABLE_CBS(unsigned, netIceTunneler::DEFAULT_NAT_FAIL_RATE_BEFORE_WARNING, ATSTRINGHASH("DEFAULT_NAT_FAIL_RATE_BEFORE_WARNING", 0x45EB1D81), netIceTunneler::SetNatFailRateBeforeWarning);
		AUTO_TUNABLE_CBS(unsigned, netIceTunneler::DEFAULT_SYMMETRIC_SOCKET_MODE, ATSTRINGHASH("SYMMETRIC_SOCKET_MODE", 0x4EB315AA), netIceTunneler::SetSymmetricSocketMode);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_SYMMETRIC_INFERENCE", 0x36EF8CCE), netIceTunneler::SetAllowSymmetricInference);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_CANONICAL_CANDIDATE", 0x2B53210E), netIceTunneler::SetAllowCanonicalCandidate);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_SAME_HOST_CANDIDATE", 0xEEB0B72A), netIceTunneler::SetAllowSameHostCandidate);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_CROSS_RELAY_CHECK", 0x8C065D10), netIceTunneler::SetAllowCrossRelayCheck);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_CROSS_RELAY_CHECK_DIRECT_OFFER_TIMED_OUT", 0x24194553), netIceTunneler::SetAllowCrossRelayCheckDirectOfferTimedOut);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_CROSS_RELAY_CHECK_REMOTE_STRICT", 0x8EECB9F0), netIceTunneler::SetAllowCrossRelayCheckRemoteStrict);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_CROSS_RELAY_CHECK_LOCAL_HOST", 0x45850026), netIceTunneler::SetAllowCrossRelayCheckLocalHost);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_RELAY_ROUTE_CHECK", 0xC2F22061), netIceTunneler::SetAllowRelayRouteCheck);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_PEER_REFLEXIVE_LIST", 0x6A821E8B), netIceTunneler::SetAllowPeerReflexiveList);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_PRESENCE_QUERY", 0x4B12A1DC), netIceTunneler::SetAllowPresenceQuery);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("PEER_REFLEXIVE_HIGHEST_PLUS_ONE", 0xFEFE87AB), netIceTunneler::SetAllowPeerReflexiveHighestPlusOne);
        AUTO_TUNABLE_CBS(unsigned, 15*60, ATSTRINGHASH("PEER_REFLEXIVE_TIMEOUT_SEC", 0x68481C4A), netIceTunneler::SetPeerReflexiveTimeoutSec);
        AUTO_TUNABLE_CBS(unsigned, 10, ATSTRINGHASH("MAX_FAILED_SYM_INF_TESTS", 0x650462B8), netIceTunneler::SetMaxFailedSymmetricInferenceTests);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_TUNNELER_TELEMETRY", 0x29E239A8), netIceTunneler::SetAllowIceTunnelerTelemetry);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_LOCAL_RELAY_REFRESH", 0x1E3F2A45), netIceTunneler::SetAllowLocalRelayRefresh);
        AUTO_TUNABLE_CBS(unsigned, 5000, ATSTRINGHASH("ICE_QUICK_CONNECT_TIMEOUT_MS", 0x6DA758CC), netIceTunneler::SetQuickConnectTimeoutMs);
		AUTO_TUNABLE_CBS(unsigned, netIceTunneler::DEFAULT_ROLE_TIE_BREAKER_STRATEGY, ATSTRINGHASH("ICE_ROLE_TIE_BREAKER_STRATEGY", 0x69BBE37B), netIceTunneler::SetRoleTieBreakerStrategy);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_DIRECT_OFFERS", 0x717C6FD3), netIceTunneler::SetAllowDirectOffers);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_ICE_DIRECT_ANSWERS", 0x1954C5A2), netIceTunneler::SetAllowDirectAnswers);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_RESERVED_CANDIDATES", 0x68D92EE0), netIceTunneler::SetAllowReservedCandidates);
        AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_RESERVED_RFC1918_CANDIDATES", 0xDFE28754), netIceTunneler::SetAllowReservedRfc1918Candidates);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("REQUIRE_RELAY_SUCCESS_FOR_QUICK_CONNECT", 0x2B1A4EBF), netIceTunneler::SetRequireRelaySuccessForQuickConnect);
	}

	// key exchange
	{
		AUTO_TUNABLE_CBS(unsigned, 500, ATSTRINGHASH("NET_KX_TRANSACTION_RETRY_INTERVAL_MS", 0x9728CA75), netKeyExchange::SetTransactionRetryIntervalMs);
		AUTO_TUNABLE_CBS(unsigned, 7, ATSTRINGHASH("NET_KX_MAX_RETRIES_PER_TRANSACTION", 0x122B2D01), netKeyExchange::SetMaxRetriesPerTransaction);
		AUTO_TUNABLE_CBS(unsigned, 5000, ATSTRINGHASH("NET_KX_MAX_SESSION_TIME_MS", 0xBC13DE72), netKeyExchange::SetMaxSessionTimeMs);
		AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("NET_KX_ALLOW_OFFER_RESEND_VIA_RELAY", 0xB21ACAEC), netKeyExchange::SetAllowOfferResendViaRelay);
	}

	// UPnP / PCP / NAT-PMP
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_PCP_ADDRESS_RESERVATION", 0x477E808F), netNatPcp::SetAllowPcpAddressReservation);
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("PCP_RESERVE_RELAY_MAPPED_ADDR", 0xD5E65E9F), netNatPcp::SetReserveRelayMappedAddress);
    AUTO_TUNABLE_CBS(unsigned, 20 * 60 * 1000, ATSTRINGHASH("PCP_REFRESH_TIME_MS", 0x96D3972D), netNatPcp::SetPcpRefreshTimeMs);
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_UPNP_PORT_FORWARDING", 0x5E735D5), netNatUpNp::SetAllowUpNpPortForwarding);
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("RESERVE_RELAY_MAPPED_PORT", 0x62E84D51), netNatUpNp::SetReserveRelayMappedPort);
    AUTO_TUNABLE_CBS(unsigned, 20 * 60 * 1000, ATSTRINGHASH("UPNP_REFRESH_TIME_MS", 0xA0AA8D4C), netNatUpNp::SetUpNpPortRefreshTimeMs);
	AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_UPNP_CLEANUP", 0x2F1E5075), netNatUpNp::SetAllowCleanUpOldEntries);

	// these systems are disabled until tunables have been received
	netNatUpNp::SetTunablesReceived();
	netNatPcp::SetTunablesReceived();

	// NAT Detector
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_NAT_UDP_TIMEOUT_MEASUREMENT", 0xF735874A), netNatDetector::SetAllowUdpTimeoutMeasurement);
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_NAT_ADJUSTABLE_PING_INTERVAL", 0x16CB3D8F), netNatDetector::SetAllowAdjustableUdpSendInterval);
    AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("ALLOW_NAT_INFO_TELEMETRY", 0xFFB91A49), netNatDetector::SetAllowNatInfoTelemetry);

	// Relay
	AUTO_TUNABLE_CBS(unsigned, 500, ATSTRINGHASH("NET_RELAY_PING_INTERVAL_ADJUSTMENT_MS", 0x5FAAFC4A), netRelay::SetPingIntervalAdjustmentMs);
	AUTO_TUNABLE_CBS(unsigned, 0, ATSTRINGHASH("NET_RELAY_PING_INTERVAL_MS", 0x24D9B7E6), netRelay::SetRelayPingIntervalMs);
	AUTO_TUNABLE_CBS(unsigned, 500, ATSTRINGHASH("NET_RELAY_PING_RETRY_MS", 0x78387754), netRelay::SetRelayPingRetryMs);
	AUTO_TUNABLE_CBS(bool, false, ATSTRINGHASH("NET_RELAY_ALLOW_MP_WITHOUT_RELAY_CXN", 0x74B331C6), netRelay::SetAllowMultiplayerWithoutRelayServer);

	AUTO_TUNABLE_CBS(unsigned, 1000 * 60 * 10, ATSTRINGHASH("NET_RELAY_TELEMETRY_SEND_INTERVAL_MS", 0xC84C275F), netRelay::SetRelayTelemetrySendIntervalMs);

	// P2P Relay and connection routing
	AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("NET_ALLOW_CONNECTION_ROUTING", 0x2BB47EFD), netConnectionRouter::SetAllowConnectionRouting);
	AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("NET_ALLOW_PEER_RELAY", 0x88C0358), netConnectionRouter::SetAllowPeerRelay);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::DEFAULT_MAX_HOP_COUNT, ATSTRINGHASH("NET_PEER_RELAY_MAX_HOPS", 0xC9B858CD), netConnectionRouter::SetMaxHopCount);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::DEFAULT_MAX_RELAYED_CXNS, ATSTRINGHASH("NET_PEER_RELAY_MAX_RELAYED_CXNS", 0x63693E3A), netConnectionRouter::SetMaxRelayedCxns);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::DEFAULT_EMERGENCY_RELAY_FALLBACK_TIME_MS, ATSTRINGHASH("NET_PEER_RELAY_EMERGENCY_RELAY_FALLBACK_TIME_MS", 0xFB9F395D), netConnectionRouter::SetEmergencyRelayFallbackTimeMs);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::DEFAULT_EMERGENCY_DIRECT_RECEIVE_TIMEOUT_MS, ATSTRINGHASH("NET_EMERGENCY_DIRECT_RECEIVE_TIMEOUT_MS", 0x4AA1A3A7), netConnectionRouter::SetEmergencyDirectReceiveTimeoutMs);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::BROKEN_LINK_DETECTION_TIME_MS, ATSTRINGHASH("NET_PEER_RELAY_BROKEN_LINK_DETECTION_TIME_MS", 0xB59F64F6), netConnectionRouter::SetBrokenLinkDetectionTimeMs);
	AUTO_TUNABLE_CBS(unsigned, netConnectionRouter::DEFAULT_ROUTE_CHANGE_WAIT_TIME_MS, ATSTRINGHASH("NET_ROUTE_CHANGE_WAIT_TIME_MS", 0x87DE1ECA), netConnectionRouter::SetRouteChangeWaitTimeMs);

	// netSocket
	AUTO_TUNABLE_CBS(bool, true, ATSTRINGHASH("SOCKET_SELECT_ON_SEND", 0x4E2FAD23), netSocket::SetRequireSelectOnSend);

	// rlProfileStats
	AUTO_TUNABLE_CBS(unsigned, rlProfileStats::MAX_NUM_GAMERS_FOR_STATS_READ, ATSTRINGHASH("MAX_NUM_GAMERS_FOR_STATS_READ", 0xd2212993), rlProfileStats::SetMaxNumGamersForStatsRead);

	// pc hardware/OS telemetry
	bool bAllowPCHardwareTelemetry = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_PC_HARDWARE_TELEMETRY", 0xDDDA97C5), bAllowPCHardwareTelemetry))
		CNetworkTelemetry::SetAllowPCHardwareTelemetry(bAllowPCHardwareTelemetry);

	// relay usage telemetry
	bool bAllowRelayUsageTelemetry = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_RELAY_USAGE_TELEMETRY", 0x1C4562D2), bAllowRelayUsageTelemetry))
		CNetworkTelemetry::SetAllowRelayUsageTelemetry(bAllowRelayUsageTelemetry);

	// whether to advertise the SESSION_ID matchmaking attribute (required server-side change to go live on prod first)
	bool bEnableSessionIdAttribute = false; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MM_SESSION_ID_ENABLE", 0xde13214b), bEnableSessionIdAttribute))
		rlScMatchmaking::EnableSessionIdAttribute(bEnableSessionIdAttribute);

	// matchmaking reaper
	bool bMatchmakingReaperEnabled = true; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MM_REAPER_ENABLE", 0xa8e4ccea), bMatchmakingReaperEnabled))
		rlScMatchmaking::EnableMatchReaper(bMatchmakingReaperEnabled);

	unsigned nReaperCheckIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_REAPER_CHECK_INTERVAL", 0xb2ccbaae), rlScMatchManager::DEFAULT_CHECK_INTERVAL_MS);
	unsigned nReaperRetryIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MM_REAPER_RETRY_INTERVAL", 0x141a6301), rlScMatchManager::DEFAULT_RETRY_INTERVAL_MS);
	rlScMatchmaking::SetMatchReaperIntervals(nReaperCheckIntervalMs, nReaperRetryIntervalMs);

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	// membership
	bool bAllowMembershipToExpire = true; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_ALLOW_MEMBERSHIP_TO_EXPIRE", 0x15764830), bAllowMembershipToExpire))
		rlScMembership::SetAllowMembershipsToExpire(bAllowMembershipToExpire);

	bool bUseMembershipServiceTasks = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_USE_MEMBERSHIP_SERVICE_TASKS", 0xcea379e1), bUseMembershipServiceTasks))
		rlScMembership::SetUseMembershipServiceTasks(bUseMembershipServiceTasks);

	bool bEnableEvaluateBenfits = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_ENABLE_EVALUATE_SUBSCRIPTION_BENEFITS", 0x59557150), bEnableEvaluateBenfits))
		rlScMembership::SetEnableEvaluateBenefits(bEnableEvaluateBenfits);

#if RL_ALLOW_DUMMY_MEMBERSHIP_DATA
	bool bHasMembership = true;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_DUMMY_HAS_MEMBERSHIP", 0x1de2617f), bHasMembership))
	{
		int membershipStart = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_DUMMY_START", 0x2edaa7a3), 0);
		int membershipEnd = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_DUMMY_END", 0x5fccdddd), 0);
		rlScMembership::SetDummyMembershipData(bHasMembership, static_cast<u64>(membershipStart), static_cast<u64>(membershipEnd));
	}
#endif
#endif

	// presence messages
	int nPresenceMessageDelayTimeMs = static_cast<int>(rlPresence::DEFAULT_RETRIEVE_MESSAGE_DELAY_MS);
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PRESENCE_MESSAGE_DELAY_MS", 0xe2902661), nPresenceMessageDelayTimeMs))
		rlPresence::SetPresenceMessageDelayTimeMs(static_cast<unsigned>(nPresenceMessageDelayTimeMs));

	int nPresenceMessageMaxToRetrieve = static_cast<int>(rlPresence::DEFAULT_MAX_PRESENCE_MESSAGES_TO_RETRIEVE);
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_PRESENCE_MESSAGES_TO_RETRIEVE", 0xfce61fd8), nPresenceMessageMaxToRetrieve))
		rlPresence::SetPresenceMessageMaxToRetrieve(static_cast<unsigned>(nPresenceMessageMaxToRetrieve));

#if SC_PRESENCE
	bool bPresenceReadSenderInfo = true; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PRESENCE_READ_SENDER_INFO", 0xAA58C713), bPresenceReadSenderInfo))
		rlScPresence::SetAllowReadingSenderInfo(bPresenceReadSenderInfo);
#endif
	
	// session
	int nMigrateRequestInterval = snMigrateSessionTask::DEFAULT_REQUEST_INTERVAL; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MIGRATE_REQUEST_INTERVAL", 0xa6304cd3), nMigrateRequestInterval))
		snMigrateSessionTask::SetMigrateRequestInterval(static_cast<unsigned>(nMigrateRequestInterval));

	bool bDataMineEnabled = true; 
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SESSION_MM_DATA_MINE_ENABLED", 0x4afb6f45), bDataMineEnabled))
		rlSessionManager::SetDataMineEnabled(bDataMineEnabled);

	AUTO_TUNABLE_CBS(unsigned, rlSessionManager::DEFAULT_NUM_QUERY_RESULTS_BEFORE_EARLY_OUT, ATSTRINGHASH("NET_NUM_QUERY_RESULTS_BEFORE_EARLY_OUT", 0x6c17bc27), rlSessionManager::SetDefaultNumEarlyOutResults);
	AUTO_TUNABLE_CBS(unsigned, rlSessionManager::DEFAULT_MAX_WAIT_SINCE_LAST_RESULT, ATSTRINGHASH("NET_MAX_WAIT_SINCE_LAST_RESULT", 0x246343b7), rlSessionManager::SetDefaultMaxWaitTimeAfterResult);

	// cloud cdn
	AUTO_TUNABLE_CBS(bool, CUserContentManager::DEFAULT_USE_CDN_TEMPLATE, ATSTRINGHASH("USE_CDN_TEMPLATE", 0x58F899C8), CUserContentManager::SetUseCdnTemplate);

	// scauth
	bool bScAuthEnabled = rlScAuth::IsEnabled();
	if (Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SCAUTH_ENABLED", 0x805B178E), bScAuthEnabled))
	{
		rlScAuth::SetEnabled(bScAuthEnabled);
	}
	else if (PARAM_netNoScAuth.Get())
	{
		rlScAuth::SetEnabled(false);
	}

	bool bDisableScAuthExit = rlScAuth::IsExitDisabled();
	if (Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SCAUTH_NOEXIT", 0x6D5268B), bDisableScAuthExit))
	{
		rlScAuth::DisableExit(bDisableScAuthExit);
	}
	else if (PARAM_netScAuthNoExit.Get())
	{
		rlScAuth::DisableExit(true);
	}

	bool bUseLinkTokens = rlScAuth::DEFAULT_LINK_TOKENS_ENABLED;
	if (Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SCAUTH_USE_LINK_TOKENS", 0xBF991867), bUseLinkTokens))
	{
		rlScAuth::SetUseLinkTokens(bUseLinkTokens);
	}

	s_EnableCodeBailAlerting = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_ENABLE_CODE_BAIL_ALERTING", 0xdf7dfb35), false);

    // load session tunables
    CNetwork::GetNetworkSession().InitTunables();

	// load livemanager and sub-system tunables
	CLiveManager::OnTunablesRead();

	// load text chat tunables
	WIN32PC_ONLY(s_NetworkTextChat.OnTunablesRead());

#if __NET_SHOP_ACTIVE
	// catalog tunables
	bool bNetCatalogCheckForDuplicates = false;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CATALOG_CHECK_FOR_DUPLICATES", 0x58ea1b2b), bNetCatalogCheckForDuplicates))
		netCatalog::SetCheckForDuplicatesOnAddItem(bNetCatalogCheckForDuplicates);
#endif

	unsigned nRequestIdThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OBJECT_ID_MGR_REQUEST_ID_THRESHOLD", 0x7f357842), netObjectIDMgr::DEFAULT_REQUEST_ID_THRESHOLD);
	unsigned nNumIdsToRequest = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OBJECT_ID_MGR_NUM_IDS_TO_REQUEST", 0x857efcc1), netObjectIDMgr::DEFAULT_NUM_IDS_TO_REQUEST);
	unsigned nMinTimeBetweenRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OBJECT_ID_MGR_MIN_TIME_BETWEEN_REQUESTS", 0xa4cdc466), netObjectIDMgr::DEFAULT_MIN_TIME_BETWEEN_REQUESTS);
	unsigned nObjectIdReleaseThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OBJECT_ID_MGR_ID_RELEASE_THRESHOLD", 0xfe229f30), netObjectIDMgr::DEFAULT_OBJECT_ID_RELEASE_THRESHOLD);
	
	// command line overrides
#if !__FINAL
	PARAM_netObjectIdRequestIdThreshold.Get(nRequestIdThreshold);
	PARAM_netObjectIdNumIdsToRequest.Get(nNumIdsToRequest);
	PARAM_netObjectIdMinTimeBetweenRequests.Get(nMinTimeBetweenRequests);
	PARAM_netObjectIdObjectIdReleaseThreshold.Get(nObjectIdReleaseThreshold);
#endif
	
	netObjectIDMgr::SetRequestIdThreshold(nRequestIdThreshold);
	netObjectIDMgr::SetNumIdsToRequest(nNumIdsToRequest);
	netObjectIDMgr::SetMinTimeBetweenRequests(nMinTimeBetweenRequests);
	netObjectIDMgr::SetObjectIdReleaseThreshold(nObjectIdReleaseThreshold);

	// Update vehicle pop tunables
	CVehicle::RetrieveAndCacheCloudTunables();
	CVehiclePopulation::InitTunables();
	CWeaponTarget::InitTunables();
	CTaskVehicleCombat::InitTunables();
	CTaskUseScenario::InitTunables();
	CTaskGetUp::InitTunables();
	CTaskExitVehicleSeat::InitTunables();
	CTaskMeleeActionResult::InitTunables();
#if CNC_MODE_ENABLED
	CTaskIncapacitated::InitTunables();
#endif
	CTaskPlayerOnFoot::InitTunables();
	CPlayerInfo::InitTunables();
	CProjectile::InitTunables();
	CVehicleDamage::InitTunables();
	CVisualEffects::InitTunables();
#if RL_FACEBOOK_ENABLED
	CLiveManager::GetFacebookMgr().InitTunables();
#endif // RL_FACEBOOK_ENABLED
	CWeapon::InitTunables();
	CStreamVolume::InitTunables();
	CIslandHopper::GetInstance().InitTunables();
	CPed::InitTunables();
	CPlayerSpecialAbilityManager::InitTunables();
	camFollowVehicleCamera::InitTunables();
	NetworkInterface::GetVoice().InitTunables();
	CPedDamageCalculator::InitTunables();
	CVehicleGlassComponent::InitTunables();
	CBullet::InitTunables();
	CExplosionManager::InitTunables();
	CVehicleWeaponBattery::InitTunables();

	BasePlayerCardDataManager::InitTunables();
	UserContentManager::GetInstance().InitTunables();
}

///////////////////////////////////////////////////////////////////////////////
// CNetwork
///////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
struct LongLoggingTracker
{
	struct LongLoggingEntry
	{
		LongLoggingEntry() { Reset(); }
		unsigned m_ExecutionTime;
		unsigned m_RelativeTime;
		unsigned m_GameFrame; 

		static const unsigned MAX_CHANNEL_NAME = 32;
		char m_ChannelName[MAX_CHANNEL_NAME];
		static const unsigned MAX_MESSAGE = 128;
		char m_Message[MAX_MESSAGE];

		void Reset()
		{
			m_ExecutionTime = 0;
			m_RelativeTime = 0;
			m_GameFrame = 0;
			m_ChannelName[0] = 0;
			m_Message[0] = 0;
		}

		void Assign(const unsigned executionTime, const unsigned relativeTime, const unsigned gameFrame, const char* channelTag, const char* message)
		{
			m_ExecutionTime = executionTime;
			m_RelativeTime = relativeTime; 
			m_GameFrame = gameFrame;
			safecpy(m_ChannelName, channelTag);
			safecpy(m_Message, message);
		}

		const LongLoggingEntry& operator=(const LongLoggingEntry& other)
		{
			m_ExecutionTime = other.m_ExecutionTime;
			m_RelativeTime = other.m_RelativeTime;
			m_GameFrame = other.m_GameFrame;
			safecpy(m_ChannelName, other.m_ChannelName);
			safecpy(m_Message, other.m_Message);
			return *this;
		}
	};

	bool HasFreeEntry() const
	{
		return m_NumEntries < NUM_LONG_LOGGING_ENTRIES;
	}

	void Reset()
	{
		m_NumEntries = 0;
		m_LongLoadingOverspillTime = 0;
	}

	static const unsigned NUM_LONG_LOGGING_ENTRIES = 10;
	LongLoggingEntry m_LongLoggingEntries[NUM_LONG_LOGGING_ENTRIES];
	unsigned m_NumEntries; 
	unsigned m_LongLoadingOverspillTime;
};

static LongLoggingTracker s_LongLoggingTrackerBuffer; 
static sysCriticalSectionToken s_LongLoggingToken; 

void LongLoggingExecutionCallback(const unsigned executionTime, const unsigned relativeTime, const unsigned gameFrame, const char* channelTag, const char* message)
{
	SYS_CS_SYNC(s_LongLoggingToken);

	if(s_LongLoggingTrackerBuffer.HasFreeEntry())
	{
		s_LongLoggingTrackerBuffer.m_LongLoggingEntries[s_LongLoggingTrackerBuffer.m_NumEntries].Assign(executionTime, relativeTime, gameFrame, channelTag, message);
		s_LongLoggingTrackerBuffer.m_NumEntries++;
		return; 
	}

	// if we're still here, we have no free logging entries, just add the execution time to the overspill bucket
	s_LongLoggingTrackerBuffer.m_LongLoadingOverspillTime += executionTime;
}

void TakeLongLoggingEntries(LongLoggingTracker& swap)
{
	// do this so that we can operate on a snapshot of the buffer without further stalled logging
	// including our own, operating on the list
	SYS_CS_SYNC(s_LongLoggingToken);
	swap = s_LongLoggingTrackerBuffer;
	s_LongLoggingTrackerBuffer.Reset(); 
}
#endif

netSocketManager*
CNetwork::GetSocketMgr()
{
    gnetAssert(CLiveManager::IsInitialized());

    return CLiveManager::GetSocketMgr();
}

unsigned short
CNetwork::GetSocketPort()
{
    static const int DEFAULT_SOCKET_PORT = __FINAL ? 0x4321 : 0x1234;

    int port;
#if !__FINAL
	if(!PARAM_socketport.Get(port) || !gnetVerifyf(port > 0, "GetSocketPort :: Invalid -socketport command line. Needs to be numeric and above 0"))
	{
		port = DEFAULT_SOCKET_PORT;
	}
#else
    port = DEFAULT_SOCKET_PORT;
#endif  //__FINAL

    gnetAssert(port < 0xFFFF);
    if(!gnetVerify(port > 0))
    {
        //Port will be zero if an invalid value was given on the command line.
        port = DEFAULT_SOCKET_PORT;
    }

    return (unsigned short) port;
}

unsigned
CNetwork::GetVersion()
{
	NET_ASSERTS_ONLY(unsigned NUM_VERSION_HIGH_BITS = 16);
	const unsigned NUM_VERSION_LOW_BITS = 16;

	unsigned versionHi = 0, versionLo = 0;
	sscanf(CDebug::GetVersionNumber(), "%d.%d", &versionHi, &versionLo);
	gnetAssert(versionHi < (1 << NUM_VERSION_HIGH_BITS));
	gnetAssert(versionLo < (1 << NUM_VERSION_LOW_BITS));

	const unsigned version = (versionHi << NUM_VERSION_LOW_BITS) | versionLo;

	return version;
}

bool
CNetwork::LanSessionsOnly()
{
    return PARAM_lan.Get();
}

OUTPUT_ONLY(static void NewLogFileCallback());

void
CNetwork::Init(unsigned initMode)
{
	// Register a function to call when diagChannel has opened a new log file.
	OUTPUT_ONLY(diagChannel::AddNewFileCallback(&NewLogFileCallback));

    gnetDebug1("CNetwork::Init :: InitMode: %d", initMode);

    if(initMode == INIT_CORE)
    {
		//Calculate executable size and cache it.
		NetworkInterface::GetExeSize( );

		netLog::SetFileAccessInterface(&s_LogFileAccessInterface);
        netLog::SetMaximumFileSize(MAXIMUM_LOG_FILE_SIZE);

		if (PARAM_nonetwork.Get())
		{
			InitTunables();
			return;
		}

#if __BANK
		sm_bSaveMigrationEnabled = false;
		if (PARAM_enableSaveMigration.Get())
		{
			sm_bSaveMigrationEnabled = true;
		}
#endif // __BANK

        sm_syncedTimeInMilliseconds = 0;
		sm_lockedNetworkTime = 0;
		sm_bGoStraightToMultiplayer=false;
		sm_bGoStraightToMpEvent=false;
		sm_bGoStraightToMPRandomJob=false;
		sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag = true;

        //The connection manager can handle allocation failures.
        UseNetworkHeap(NET_MEM_CONNECTION_MGR_HEAP);
        s_SCxnMgrHeap = rage_new u8[CNetwork::SCXNMGR_HEAP_SIZE];

		// set up our fixed sized buckets. The remaining memory is managed by the simple allocator.
		// The fixed allocator heap size is calculated by multiplying the number of chunks in each bucket with the size of chunks in each bucket.
		// The bucket sizes are chosen by collecting allocation statistics from QA running with -netMemAllocatorLogging (see netallocator.cpp).
		size_t fixedChunkSizes[] =						{16,  32,  48,  64,  80,  96,  128, 224, 336, 352, 512, 960};
		u8 fixedChunkCounts[COUNTOF(fixedChunkSizes)] = {255, 255, 255, 255, 255, 255, 255, 128, 255, 255, 64,  64}; // about 407k out of our 650k heap
		const unsigned numFixedBuckets = COUNTOF(fixedChunkSizes);
		CompileTimeAssert(numFixedBuckets <= netMemAllocator::MAX_FIXED_BUCKETS);
		s_SCxnMgrAllocator = rage_new netMemAllocator(s_SCxnMgrHeap, CNetwork::SCXNMGR_HEAP_SIZE, sysMemSimpleAllocator::HEAP_NET,
													  numFixedBuckets, fixedChunkSizes, fixedChunkCounts);
        s_SCxnMgrAllocator->SetQuitOnFail(false);
        StopUsingNetworkHeap();

		// Tell the memory tracker du jour about our network heap.
#if RAGE_TRACKING
		//diagTracker* t = diagTracker::GetCurrent();
		//if (t && sysMemVisualize::GetInstance().HasNetwork())
		//{
		//	t->InitHeap("Net - SCnxMgr", s_SCxnMgrHeap, CNetwork::SCXNMGR_HEAP_SIZE);
		//}
#endif // RAGE_TRACKING

		netSocketManager* sktMgr = CNetwork::GetSocketMgr();
				
		sm_CxnMgr.SetMemoryTrackingEnabled(true);
		gnetVerify(sm_CxnMgr.Init(s_SCxnMgrAllocator,
                                    DEFAULT_ENDPOINT_POOL_HINT,
									DEFAULT_CXN_POOL_HINT,
                                    sktMgr,
                                    CXN_MGR_CPU_AFFINITY,
									NULL,
									NULL,
									NULL));

		unsigned timeoutMs = GetTimeoutTime();
        ResetConnectionPolicies(timeoutMs);


        CNetworkSession::InitRegistrationPolicies();

#if !__FINAL
        sm_CxnMgr.SetDisableInactiveEndpointRemoval(timeoutMs == 0);
#endif // !__FINAL

        for(int i = 0; i < COUNTOF(sm_CxnMgrGameDlgts); ++i)
        {
            sm_CxnMgrGameDlgts[i].Bind(&CNetwork::OnNetEvent);
            sm_CxnMgr.AddChannelDelegate(&sm_CxnMgrGameDlgts[i], i);
        }

        rlSessionManager::SetLanMatchingPort(CNetwork::GetSocketPort());

        UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
        sm_NetworkSession = rage_new CNetworkSession;
		sm_NetworkParty = rage_new CNetworkParty;
        StopUsingNetworkHeap();

		gnetVerify(sm_NetworkSession->Init());
		gnetVerify(sm_NetworkParty->Init());

		sm_NetworkVoiceSession.Init(CLiveManager::GetRlineAllocator(), &sm_CxnMgr, NETWORK_SESSION_VOICE_CHANNEL_ID);

		sm_LeaderboardMgr.Init(CLiveManager::GetRlineAllocator());

		sm_NetworkVoice.InitCore(&sm_CxnMgr);

#if RSG_ORBIS		
		InitialiseLiveStreaming();
#endif
		s_NetworkCoreInitialised = true;

		datafile_commands::InitialiseThreadPool();

#if !__NO_OUTPUT
		// set up http request tracker
		netVerifyf(netHttpRequestTracker::Init(), "Error initializing netHttpRequestTracker");
		sm_RequestTrackerDlgt.Bind(&CNetwork::OnRequestTrackerEvent);
		netHttpRequestTracker::AddDelegate(&sm_RequestTrackerDlgt);
#endif

#if !__FINAL
		netFunctionProfiler::Init();
		netFunctionProfiler::SetProfilerEnabled(!PARAM_netFunctionProfilerDisabled.Get());
#endif

		CNetworkTelemetry::SetupTelemetryStream();

		// log out connection manager heap stats after initialization
		gnetDebug1("CNetwork::Init :: Connection Manager Heap Stats - Size: %d, Used: %d, Available: %d, Largest Block: %d", 
				   static_cast<int>(s_SCxnMgrAllocator->GetHeapSize()),		
				   static_cast<int>(s_SCxnMgrAllocator->GetMemoryUsed(-1)),
				   static_cast<int>(s_SCxnMgrAllocator->GetMemoryAvailable()),
				   static_cast<int>(s_SCxnMgrAllocator->GetLargestAvailableBlock()));
    }
    else if(initMode == INIT_SESSION)
    {
	    if(PARAM_nonetwork.Get())
		    return;

		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		netLog::StartLoggingThread();

		const unsigned MESSAGE_LOG_BUFFER_SIZE = 16 * 1024;
		static bool s_HasCreatedLog = false; 
		s_MessageLog = rage_new netLog("Message.log", s_HasCreatedLog ? LOGOPEN_APPEND : LOGOPEN_CREATE, netInterface::GetDefaultLogFileTargetType(),
                                                                      netInterface::GetDefaultLogFileBlockingMode(), MESSAGE_LOG_BUFFER_SIZE);
		s_HasCreatedLog = true;
		
		s_MessageLog->SetUsesStringTable();

#if !__NO_OUTPUT
		if(!PARAM_netDisableLoggingStallTracking.Get())
		{
			rage::diagSetLongExecutionCallback(&LongLoggingExecutionCallback);
		}
#endif

		NetworkDebug::InitSession();

		SetScriptReadyForEvents(false);

		sm_bInMPTutorial			= false;
		sm_bFriendlyFireAllowed		= false;

        sm_bFirstEntryNetworkOpen   = false;
        sm_bFirstEntryMatchStarted  = false;
        
		CFakePlayerNames::Init();

		gClockOverride.Reset();
		CNetwork::GetBandwidthManager().BandwidthTest().TestBandwidth(false, nullptr);

        ASSERT_ONLY(CQueriableInterface::ValidateTaskInfos());

        #if __WIN32PC
		sm_ExitToDesktopFlow.ResetExitFlowState();
		#endif
	}
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
        if (PARAM_nonetwork.Get())
		    return;

	    gnetAssert(CLiveManager::IsInitialized());

	    sm_bInMPTutorial			= false;
        sm_bFriendlyFireAllowed		= false;
	}

	CLiveManager::Init(initMode);
	
	if(initMode == INIT_CORE)
	{
		InitTunables();
        m_pTunablesListener = rage_new CNetworkTunablesListener();

#if !__FINAL
		// load local tunables after we've registered our network listener
		if(gnetVerify(Tunables::IsInstantiated()))
			Tunables::GetInstance().LoadLocalFile();
#endif
	}
}

void
CNetwork::Shutdown(unsigned shutdownMode)
{
	gnetDebug1("CNetwork::Shutdown :: ShutdownMode: %d", shutdownMode);
	
	if(shutdownMode == SHUTDOWN_SESSION)
    {
		// if we are running any session activities, we need to shutdown
		if(NetworkInterface::IsAnySessionActive())
		{
			// this is done outside of CloseNetwork because there are legitimate cases for that call
			// within sessioning (when launching an activity lobby)
			sm_NetworkSession->Shutdown(true, eBailReason::BAIL_INVALID);

			// immediately initialise again
			gnetVerify(sm_NetworkSession->Init());
		}

		// if the network is open then it needs to be cleaned up and 
		// pending tasks must be finished.
		if(NetworkInterface::IsNetworkOpen())
		{
			// update once to finish any pending tasks and then close (full close)
			CNetwork::Update(true);
			CNetwork::CloseNetwork(true);
		}

		if (sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag)
		{
			sm_bGoStraightToMultiplayer = false;
			sm_bGoStraightToMpEvent = false;
			sm_bGoStraightToMPRandomJob = false;
		}
		sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag = true;

		NetworkDebug::ShutdownSession();

        delete s_MessageLog;
        s_MessageLog = nullptr;

        netLog::StopLoggingThread();

		WIN32PC_ONLY(sm_ExitToDesktopFlow.ResetExitFlowState());
    }
	else if(shutdownMode == SHUTDOWN_CORE)
	{
		datafile_commands::ShutdownThreadPool();

		if (PARAM_nonetwork.Get())
		{
			ShutdownTunables();
			return;
		}

		delete m_pTunablesListener;
		m_pTunablesListener = NULL;

		for(int i = 0; i < COUNTOF(sm_CxnMgrGameDlgts); ++i)
		{
			sm_CxnMgr.RemoveChannelDelegate(&sm_CxnMgrGameDlgts[i]);
		}

#if !__NO_OUTPUT
		netHttpRequestTracker::RemoveDelegate(&sm_RequestTrackerDlgt);
#endif

		UseNetworkHeap(NET_MEM_LEADERBOARD_DATA);
		sm_LeaderboardMgr.Shutdown();
		StopUsingNetworkHeap();

		sm_NetworkSession->Shutdown(false, eBailReason::BAIL_INVALID);
		sm_NetworkParty->Shutdown();
        
		UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
        delete sm_NetworkSession;
		delete sm_NetworkParty;
        StopUsingNetworkHeap();
		
		sm_NetworkSession = nullptr;
		sm_NetworkParty = nullptr;
		
		sm_NetworkVoiceSession.Shutdown(false);

		WIN32PC_ONLY(sm_NetworkTextChat.Shutdown());

		// close the Network
		CNetwork::CloseNetwork(true);

		// shutdown voice
		ShutdownVoice(true);
		sm_NetworkVoice.ShutdownCore();

		sm_CxnMgr.Shutdown();

        UseNetworkHeap(NET_MEM_CONNECTION_MGR_HEAP);
        delete [] s_SCxnMgrHeap;
        delete s_SCxnMgrAllocator;
        s_SCxnMgrHeap      = 0;
        s_SCxnMgrAllocator = 0;
        StopUsingNetworkHeap();

 		// shutdown live streaming 
#if RSG_ORBIS
		ShutdownLiveStreaming();
#endif
		NOTFINAL_ONLY(netFunctionProfiler::Shutdown());
		s_NetworkCoreInitialised = false;
	}

	CLiveManager::Shutdown(shutdownMode);

	if(shutdownMode == SHUTDOWN_CORE)
	{
		ShutdownTunables();
	}
}

void
CNetwork::InitTunables()
{
	Tunables::Instantiate();
	Tunables::GetInstance().Init();
}

void
CNetwork::ShutdownTunables()
{
	if(Tunables::IsInstantiated())
	{
		Tunables::GetInstance().Shutdown();
	}
}

void 
CNetwork::ShutdownConnectionManager()
{
	sm_CxnMgr.Shutdown();
}

void
CNetwork::OnTunablesRead()
{
	// check for connection policy changes (timeout or keep alive interval)
	bool bTunableChanges = false;
	bTunableChanges |= Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_TIME_OUT", 0x25f8696e), CNetwork::DEFAULT_CONNECTION_TIMEOUT);
	bTunableChanges |= Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_KEEP_ALIVE_INTERVAL", 0xfaf51383), CNetwork::DEFAULT_CONNECTION_KEEP_ALIVE_INTERVAL);
	if(bTunableChanges)
		ResetConnectionPolicies(CNetwork::GetTimeoutTime(), static_cast<unsigned>(CNetwork::DEFAULT_CONNECTION_KEEP_ALIVE_INTERVAL));

	// tunables for open / close invalid states
	sm_bAllowPendingCloseNetwork = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_PENDING_CLOSE_NETWORK", 0x8a98d7d0), true);
	sm_bFatalErrorForDoubleOpen = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_DOUBLE_OPEN", 0xbd08c6f7), true);
	sm_bFatalErrorForCloseDuringOpen = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_CLOSE_DURING_OPEN", 0xb495efa5), true);
	sm_bFatalErrorForStartWhenClosed = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_START_WHEN_CLOSED", 0x7eb2601c), true);
	sm_bFatalErrorForStartWhenFullyClosed = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_START_WHEN_FULLY_CLOSED", 0x920124a3), true);
	sm_bFatalErrorForStartWhenManagersShutdown = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_START_WHEN_MANAGERS_SHUTDOWN", 0xb2bb6ec4), true);
	sm_bFatalErrorForInvalidPlayerManager = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FATAL_ERROR_FOR_INVALID_PLAYER_MGR", 0x294b3787), true);

	// tunables for sending stall messages to other players
	sm_nMinStallMsgTotalThreshold = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MIN_STALL_MSG_TOTAL_THRESHOLD", 0x33d23b75), 1000);
	sm_nMinStallMsgNetworkThreshold = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MIN_STALL_MSG_NETWORK_THRESHOLD", 0xb1398c19), 100);

	CLiveManager::GetCrewDataMgr().OnTunableRead();
}

OUTPUT_ONLY(static void WriteSummaryIfLogFileIsNew());

void
CNetwork::Update(bool fromLoadingCode)
{
	if(!s_NetworkCoreInitialised && !PARAM_nonetwork.Get())
	{
		return;
	}

#if !__FINAL
	scrThread::CheckIfMonitoredScriptArraySizeHasChanged("CNetwork::Update() - start", 0);
#endif

	OUTPUT_ONLY(WriteSummaryIfLogFileIsNew());

	// don't process again if we are already in the network update function
	// (for recursion)
	static int inNetworkUpdate = 0;

	// stall detection
	static unsigned s_LastTime = sysTimer::GetSystemMsTime();
	const unsigned nCurrentTime = sysTimer::GetSystemMsTime();

#if !__FINAL && !__PROFILE
	// disable any functions that we call within loading code from also pushing / popping
	netFunctionProfiler::SetEnablePushAndPop(!fromLoadingCode);

	if(0 == inNetworkUpdate)
	{
		netFunctionProfiler::GetProfiler().SetShouldProfile(!fromLoadingCode);
		if(!fromLoadingCode) // clean the profiler if we finished the network update function
		{
			netFunctionProfiler::GetProfiler().StartFrame();
		}
	}

	if(!fromLoadingCode) // clean the profiler if we finished the network update function
	{
		netFunctionProfiler::GetProfiler().PushProfiler("CNetwork::Update", PARAM_netFunctionProfilerAsserts.Get());
		unsigned profilerTreshold = 1;
		PARAM_netFunctionProfilerThreshold.Get(profilerTreshold);
		netFunctionProfiler::GetProfiler().SetTimeThreshold(profilerTreshold);
	}
#endif // !__FINAL && !__PROFILE

    UpdateSyncedTimeInMilliseconds();

	if (!PARAM_nonetwork.Get())
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		if (fwTimer::IsUserPaused() && sm_networkClock.HasSynched())
		{
			if(!sm_WasDebugPausedLastFrame)
			{
				sm_NetworkTimeWhenDebugPaused = sm_networkClock.GetTime();
				gnetDebug1("Network Timer Paused at %d", sm_NetworkTimeWhenDebugPaused);
			}

			sm_WasDebugPausedLastFrame = true;
		}
		else
		{
			if(sm_WasDebugPausedLastFrame)
				gnetDebug1("Network Timer Unpaused at %d", sm_NetworkTimeWhenDebugPaused);

			sm_WasDebugPausedLastFrame = false;
		}

#if !__FINAL
		UPDATE_PROFILE(CNetworkSoakTests::Update())
#endif // !__FINAL
	}

	// don't do these during a keep alive
	if(!fromLoadingCode)
	{
		// need to bail if we tried during an open
		if(sm_IsBailPending && !sm_bNetworkOpening)
		{
			gnetDebug1("CNetwork::Update :: Actioning pending bail");
			ActionBail();
			sm_IsBailPending = false;
		}
		
		// need to close if we tried during an open
		if(sm_bNetworkClosePending && !sm_bNetworkOpening)
		{
			gnetDebug1("CNetwork::Update :: Actioning pending close");
			CloseNetwork(sm_bNetworkClosePendingIsFull);
			sm_bNetworkClosePending = false;
		}
	}
	
	UpdateNetworkAlertScreen();

	// we don't update the timers when this function is called from loading code.
	// This is because the function can be called many times per frame which can
	// overflow the time bars.
    netInterface::SetShouldUpdateTimebars(!fromLoadingCode);
	netTimeBarWrapper timeBarWrapper(netInterface::ShouldUpdateTimebars());

    ++inNetworkUpdate;

	const unsigned curTime = sysTimer::GetSystemMsTime();

    if(1 == inNetworkUpdate)
    {
		bool bUseTimebars = sm_NetworkSession && sm_NetworkSession->IsSessionActive();
		if (bUseTimebars)
			timeBarWrapper.PushBudgetTimer("Network", 3.0f);
		else
			timeBarWrapper.PushTimer("Network",false);

		if (!PARAM_nonetwork.Get())
		{
			//Always update the connection manager and the session in order
			//to handle events that occur while we're restarting and
			//we have an active rendezvous session.
			
			// Call the connection manager's Update() every frame
			// to read/write messages to/from the network socket
			if(bUseTimebars) timeBarWrapper.StartTimer("netConnectionManager", false);
			//s_CxnMgrAllocator.SanityCheck();
			UPDATE_PROFILE(sm_CxnMgr.Update(curTime))
			
#if !__FINAL
			scrThread::CheckIfMonitoredScriptArraySizeHasChanged("sm_CxnMgr.Update()", 0);
#endif

			if(bUseTimebars) timeBarWrapper.StartTimer("NetworkSession", false);
			
            if(sm_NetworkSession)
            {
				UPDATE_PROFILE(sm_NetworkSession->Update())
            }

			if (sm_NetworkParty)
			{
				UPDATE_PROFILE(sm_NetworkParty->Update())
			}

			UPDATE_PROFILE(sm_NetworkVoiceSession.Update())

			if(bUseTimebars) timeBarWrapper.StartTimer("LeaderboardManager");
			UPDATE_PROFILE(sm_LeaderboardMgr.Update())

			// manage clock and weather update
			UPDATE_PROFILE(UpdateClockAndWeather())

			// manage additional SCUI hotkeys (pc-only)
			UPDATE_PROFILE(UpdateScuiHotkeys())

			if(!fromLoadingCode)
			{
				const unsigned nCurrentTimeIncNetworkUpdate = sysTimer::GetSystemMsTime();
				static unsigned s_LastTimeIncNetworkUpdate = sysTimer::GetSystemMsTime();
				static unsigned s_LastPosix = static_cast<unsigned>(rlGetPosixTime());
				static bool s_bWasInMultiplayer = NetworkInterface::IsAnySessionActive();
				const bool bIsInMultiplayer = NetworkInterface::IsAnySessionActive();
				static bool s_bWasInTransition = NetworkInterface::IsTransitioning();
				static bool s_bWasLaunching = NetworkInterface::IsLaunchingTransition();
				const unsigned nElapsedTimeIncNetworkUpdate = (nCurrentTimeIncNetworkUpdate - s_LastTimeIncNetworkUpdate);
				const unsigned nElapsedTime = (nCurrentTime - s_LastTime);

				// log when the time between updates is more than s_nLongFrameTime ms.
				// this will also fire for 'known' stalls such breakpoints, adding a bug 
				// or warping the player but it's only a log entry
				if(bIsInMultiplayer && s_bWasInMultiplayer && (nElapsedTimeIncNetworkUpdate > s_nLongFrameTime))
				{
					// if the elapsed time is greater than 1 second, pull an updated posix from the server to 'verify' the stall
#if !__NO_OUTPUT
					const unsigned nCurrentPosix = static_cast<unsigned>(rlGetPosixTime(nElapsedTimeIncNetworkUpdate > 1000));
					gnetWarning("NetStallDetect :: Network :: Stall of %ums (Network Update: %ums, Current: %dms, Last: %dms, Posix: %u, LastPosix: %u, ElapsedPosix: %u", nElapsedTimeIncNetworkUpdate, nElapsedTimeIncNetworkUpdate - nElapsedTime, nCurrentTime, s_LastTime, nCurrentPosix, s_LastPosix, nCurrentPosix - s_LastPosix);
#endif
					// if it exceeds one of two thresholds, send a warning about the stall to connected players
					if ((nElapsedTimeIncNetworkUpdate > sm_nMinStallMsgTotalThreshold) || 
						(nElapsedTimeIncNetworkUpdate - nElapsedTime) > sm_nMinStallMsgNetworkThreshold)
					{
						sm_NetworkSession->SendStallMessage(nElapsedTimeIncNetworkUpdate, nElapsedTimeIncNetworkUpdate - nElapsedTime);
					}
				}

				// this is when script have kicked off, guard using this to avoid any stalls during boot
				if(IsScriptReadyForEvents() NOTFINAL_ONLY(&& PARAM_netSendStallTelemetry.Get()))
				{
					if(bIsInMultiplayer && s_bWasInMultiplayer && (nElapsedTimeIncNetworkUpdate > s_nTimeForLongStallTelemetryMP))
					{
						u64 nSessionToken = 0, nSessionID = 0;
						if(sm_NetworkSession)
						{
							const snSession& rSession = sm_NetworkSession->GetSnSession();
							nSessionToken = rSession.Exists() ? rSession.GetSessionToken().GetValue() : 0;
							nSessionID = rSession.GetSessionId();
						}
						
						gnetDebug1("NetStallDetect :: StallTelemetry :: Stall: %ums, Network: %ums, Transition: %s, Launching: %s, Host: %s, TimeoutsPending: %u, Size: %u, Token: 0x%016" I64FMT "x, Id: 0x%016" I64FMT "x", 
								   nElapsedTimeIncNetworkUpdate,
								   nElapsedTimeIncNetworkUpdate - nElapsedTime,
								   s_bWasInTransition ? "True" : "False",
								   s_bWasLaunching ? "True" : "False",
								   NetworkInterface::IsHost() ? "True" : "False",
								   sm_CxnMgr.GetNumTimeoutPending(),
								   sm_PlayerMgr.IsInitialized() ? sm_PlayerMgr.GetNumActivePlayers() : 0,
								   nSessionToken,
								   nSessionID);

						CNetworkTelemetry::StallDetectedMultiPlayer(nElapsedTimeIncNetworkUpdate, 
																	nElapsedTimeIncNetworkUpdate - nElapsedTime,
																	s_bWasInTransition,
																	s_bWasLaunching,
																	NetworkInterface::IsHost(),
																	sm_CxnMgr.GetNumTimeoutPending(),
																	sm_PlayerMgr.IsInitialized() ? sm_PlayerMgr.GetNumActivePlayers() : 0,
																	nSessionToken,
																	nSessionID);

						// let session know
						sm_NetworkSession->OnStallDetected(nElapsedTimeIncNetworkUpdate);
					}
					else if(!bIsInMultiplayer && !s_bWasInMultiplayer && (nElapsedTimeIncNetworkUpdate > s_nTimeForLongStallTelemetrySP))
						CNetworkTelemetry::StallDetectedSinglePlayer(nElapsedTimeIncNetworkUpdate, nElapsedTimeIncNetworkUpdate - nElapsedTime);
				}

				s_LastPosix = static_cast<unsigned>(rlGetPosixTime());
				s_LastTimeIncNetworkUpdate = sysTimer::GetSystemMsTime();
				s_bWasInMultiplayer = bIsInMultiplayer;
				s_bWasInTransition = NetworkInterface::IsTransitioning();
				s_bWasLaunching = NetworkInterface::IsLaunchingTransition();

#if !__NO_OUTPUT
				// long logging tracker
				if(s_LongLoggingTrackerBuffer.m_NumEntries > 0)
				{
					LongLoggingTracker tracker; 
					TakeLongLoggingEntries(tracker);

					for(unsigned i = 0; i < tracker.m_NumEntries; i++)
					{
						gnetWarning("NetStallDetect :: Long Logging Execution: %ums, Msg: [%08u:%08u] %s: %s", 
							tracker.m_LongLoggingEntries[i].m_ExecutionTime,
							tracker.m_LongLoggingEntries[i].m_RelativeTime,
							tracker.m_LongLoggingEntries[i].m_GameFrame,
							tracker.m_LongLoggingEntries[i].m_ChannelName,
							tracker.m_LongLoggingEntries[i].m_Message);
					}

					if(tracker.m_LongLoadingOverspillTime)
					{
						gnetWarning("NetStallDetect :: Long Logging Overspill: %ums", tracker.m_LongLoadingOverspillTime);
					}
				}
#endif
			}

			if(sm_bNetworkOpen)
			{
				if (sm_networkClock.HasStarted())
				{
					timeBarWrapper.StartTimer("Time Management");
					UPDATE_PROFILE(sm_networkClock.Update())
				}
                else
                {
                    s_PausableNetworkTimeStopped     = false;
                    s_PausableNetworkTimeOffset      = 0;
                    s_TimePausableNetworkTimeStopped = 0;
                }

                // update locked time
				sm_lockedNetworkTime = GetNetworkTime();

                // update pausable network time
                if(s_PausableNetworkTimeStopped)
                {
                    s_PausableNetworkTimeOffset = sm_lockedNetworkTime - s_TimePausableNetworkTimeStopped;
                }

				if (sm_bManagersAreInitialised)
				{
#if !__FINAL
					scrThread::CheckIfMonitoredScriptArraySizeHasChanged("CNetwork::Update", 0);
#endif

					timeBarWrapper.StartTimer("Misc Networking Systems", false);

					timeBarWrapper.StartTimer("Player Manager");
					UPDATE_PROFILE(sm_PlayerMgr.Update())
					
					timeBarWrapper.PushTimer("NetworkDebug");
					UPDATE_PROFILE(NetworkDebug::Update())
					timeBarWrapper.PopTimer();

					// update the network components of the script handlers
					timeBarWrapper.StartTimer("Script handlers");
					UPDATE_PROFILE(CTheScripts::GetScriptHandlerMgr().NetworkUpdate())

#if !__FINAL
					scrThread::CheckIfMonitoredScriptArraySizeHasChanged("CTheScripts::GetScriptHandlerMgr().NetworkUpdate()", 0);
#endif

					// The rest of the managers are only updated once we have hosted or joined a session.
					// We need to wait until the network clock is synchronized properly.
					if (NetworkInterface::IsInSession())
					{
						timeBarWrapper.StartTimer("Bubble Manager");
						UPDATE_PROFILE(sm_RoamingBubbleMgr.Update())

						timeBarWrapper.StartTimer("Bandwidth Manager");
						UPDATE_PROFILE(sm_BandwidthMgr.Update())

						// don't update the rest of the managers until our local player has been assigned a bubble and a physical player index.
						if (NetworkInterface::GetLocalPlayer()->IsInRoamingBubble())
						{
							timeBarWrapper.PushTimer("Array Manager", false);
							UPDATE_PROFILE(sm_ArrayMgr->Update())
							timeBarWrapper.PopTimer(false);

#if !__FINAL
							scrThread::CheckIfMonitoredScriptArraySizeHasChanged("sm_ArrayMgr->Update()", 0);
#endif
							// the event mgr update must come before the object mgr update, because otherwise we can get the
							// situation where a clone is updated, then migrates due to an incoming GiveControl event, then it
							// is updated again as a local object in CWorldProcess. So in effect ProcessControl is called twice
							// resulting in some problems (eg. assert(0) in PedMoveBlendOnFoot)
							timeBarWrapper.StartTimer("Event Manager", false);
							UPDATE_PROFILE(sm_EventMgr->Update());
							UpdateEventTriggerCounters();

                            // update the cached flag for whether there is collision around the local player (using when updating network objects)
                            if(FindPlayerPed() && FindPlayerPed()->GetNetworkObject())
                            {
                                sm_bCollisionAroundLocalPlayer = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(FindPlayerPed()->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
                            }

							timeBarWrapper.PushTimer("Object Manager", false);
							UPDATE_PROFILE(sm_ObjectMgr->Update(!fromLoadingCode))
							timeBarWrapper.PopTimer(false);

							UPDATE_PROFILE(UpdateConcealedPlayerCachedVariables())

							UPDATE_PROFILE(UpdateTutorialSessionCachedVariables())

							UPDATE_PROFILE(NetworkHasherUtil::UpdateResultsQueue(fromLoadingCode))

#if !__FINAL							
							scrThread::CheckIfMonitoredScriptArraySizeHasChanged("sm_ArrayMgr->Update()", 0);
#endif
						}
					}

					UPDATE_PROFILE(CNetworkStreamingRequestManager::Update())

					// scope block for profile block
					{
						UPDATE_PROFILE_BLOCK(netLoggingInterface::PostNetworkUpdate);
						sm_ArrayMgr->GetLog().PostNetworkUpdate();
						sm_EventMgr->GetLog().PostNetworkUpdate();
						sm_ObjectMgr->GetLog().PostNetworkUpdate();
						sm_ObjectMgr->GetReassignMgr().GetLog().PostNetworkUpdate();
						sm_PlayerMgr.GetLog().PostNetworkUpdate();
						sm_BandwidthMgr.GetLog().PostNetworkUpdate();

						if(s_MessageLog != nullptr)
						{
							s_MessageLog->PostNetworkUpdate();
						}
					}

					UPDATE_PROFILE(CNetObjPlayer::UpdateHighestWantedLevelInArea())
				}

                if(CTheScripts::GetScriptHandlerMgr().GetLog())
                {
					CTheScripts::GetScriptHandlerMgr().GetLog()->PostNetworkUpdate();
                }

				//Calculate average RTT over time for the multiplayer game.
				//  - This only works when you are the client since it is using the network clock RTT.
				if (sm_averageRTTInterval < curTime 
					|| (sm_averageRTTSamples < DEFAULT_INITIAL_RTT_SAMPLES && sm_averageRTTInterval+DEFAULT_INITIAL_RTT_CHECK_INTERVAL < curTime))
				{
					int interval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("RTT_CHECK_INTERVAL", 0x0293014a), DEFAULT_RTT_CHECK_INTERVAL);
					sm_averageRTTInterval = curTime+interval;

					sm_averageRTT += sm_networkClock.GetAverageRTT();
					if (sm_networkClock.GetAverageRTT() > sm_averageRTTWorst)
					{
						sm_averageRTTWorst = sm_networkClock.GetAverageRTT();
					}
					++sm_averageRTTSamples;

					static StatId avgRTT = ATSTRINGHASH("MPPLY_AVERAGE_RTT", 0x5047E8CC);
					StatsInterface::SetStatData(avgRTT, (int)GetClientAverageRTT());
				}

                if(sm_bDisableCarGeneratorTime > 0)
                {
                    sm_bDisableCarGeneratorTime -= rage::Min(sm_bDisableCarGeneratorTime, fwTimer::GetSystemTimeStepInMilliseconds());
                }
			}

#if __WIN32PC
			sm_ExitToDesktopFlow.Update();
#endif // __WIN32PC

			if(bUseTimebars) timeBarWrapper.StartTimer("Voice", false);
			UPDATE_PROFILE(sm_NetworkVoice.Update())
			
#if __WIN32PC
			if(sm_NetworkTextChat.IsInitialized())
			{
				sm_NetworkTextChat.Update();
			}
#endif

			UPDATE_PROFILE(netRemoteLog::UpdateClass())

			// update tunables
			if(bUseTimebars) timeBarWrapper.StartTimer("Tunables", false);
			UPDATE_PROFILE(Tunables::GetInstance().Update())

		} // end !PARAM_nonetwork.Get

		if(bUseTimebars) timeBarWrapper.StartTimer("LiveManager", false);
		UPDATE_PROFILE(CLiveManager::Update(curTime | 0x01))

        timeBarWrapper.PopTimer(false);  //Network

		//CLiveManager calls rlUpdate(), which updates ongoing NAT
		//negotiations. Be sure to continue updating while loading.
	}

    --inNetworkUpdate;

    gnetAssert(inNetworkUpdate >= 0);

#if ENABLE_NETWORK_LOGGING
    if(s_AssertionFiredLastFrame)
    {
		UPDATE_PROFILE(CNetwork::FlushAllLogFiles(true))

        s_AssertionFiredLastFrame = false;
    }
#endif // ENABLE_NETWORK_LOGGING

#if __ASSERT
	fwTimer::SetIsNetworkGame(IsGameInProgress());
#endif

#if !__FINAL && !__PROFILE
	if(!fromLoadingCode) // clean the profiler if we finished the network update function
	{
		netFunctionProfiler::GetProfiler().PopProfiler("CNetwork::Update", s_nLongFrameTime);

		if(0 == inNetworkUpdate) // clean the profiler if we finished the network update function
		{
			netFunctionProfiler::GetProfiler().TerminateFrame();
		}
	}

	// always enable this at the end of the frame
	netFunctionProfiler::SetEnablePushAndPop(true);
#endif

#if !__NO_OUTPUT
	// http request tracking
	netHttpRequestTracker::Update();
#endif

	// do this at the bottom so that we don't include a long network frame in our detection
	if(0 == inNetworkUpdate && !fromLoadingCode)
	{
		s_LastTime = sysTimer::GetSystemMsTime();

#if !__NO_OUTPUT
		// log when the time for CNetwork::Update is more than s_nLongFrameTime ms.
		const unsigned nElapsedUpdateTime = (s_LastTime - nCurrentTime);
		if(nElapsedUpdateTime > s_nLongFrameTime)
		{
			gnetWarning("NetStallDetect :: Network :: Long Update of %dms (Top: %dms, Bottom: %dms) detected at %" I64FMT "u", nElapsedUpdateTime, nCurrentTime, s_LastTime, rlGetPosixTime());
		}
#endif  //!__NO_OUTPUT
	}

	CNetworkEventsDumpInfo::UpdateNetworkEventsCrashDumpData();
}

u32 
CNetwork::GetClientAverageRTT()
{
	u32 averagertt = sm_averageRTT;
	if (sm_averageRTTSamples == 2)
	{
		averagertt /= 2;
	}
	else if (sm_averageRTTSamples>2)
	{
		averagertt = (sm_averageRTT-sm_averageRTTWorst) / (sm_averageRTTSamples-1);
	}

	return averagertt;
}

bool IsUsingDebugClock()
{
	bool bUsingDebugClock = false;
#if !__FINAL
	bUsingDebugClock |= CClock::HasDebugModifiedInNetworkGame();
#endif
#if __BANK
	bUsingDebugClock |= CClock::GetTimeOverrideFlagRef();
#endif
	return bUsingDebugClock;
}

void CNetwork::ApplyClockAndWeather(bool bDoTransition, unsigned nTransitionTime)
{
	// no transition, flatten the timer
	if(!bDoTransition)
		m_nClockAndWeatherTransitionTime = 0;
	// do transition but not time given
	else if(bDoTransition)
	{
		if(nTransitionTime == 0)
		{
			int sTransitionTime = 0;
			bool bFound = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CLOCK_AND_WEATHER_TRANSITION_TIME", 0x8cb9ff24), sTransitionTime);	
			if(bFound)
				m_nClockAndWeatherTransitionTime = static_cast<unsigned>(sTransitionTime);
			else
			{
				static const unsigned DEFAULT_TRANSITION_TIME = 5000;
				m_nClockAndWeatherTransitionTime = DEFAULT_TRANSITION_TIME;
			}
		}
		else
			m_nClockAndWeatherTransitionTime = nTransitionTime;
	}

    // grab the global date
    int nGameDay;
    GetGlobalDate(nGameDay);

    static const s32 MULTIPLAYER_YEAR_START = 2013; 
    CClock::SetDate(1, Date::JANUARY, MULTIPLAYER_YEAR_START);
    CClock::AddDays(nGameDay);

    // date logging
    netTodDebug("ApplyClockAndWeather :: Date is %02d,%02d,%04d. DoW: %d. Ratio: %.2f", CClock::GetDay(), CClock::GetMonth(), CClock::GetYear(), CClock::GetDayOfWeek(), GetGlobalDateRatio());
    
	// grab the global clock time
	int nHour, nMinute, nSecond;
	GetGlobalClock(nHour, nMinute, nSecond);

	// if the clock time is overridden, we need to update the stored values
	if(CNetwork::IsClockTimeOverridden())
		CNetwork::HandleClockUpdateDuringOverride(nHour, nMinute, nSecond, sysTimer::GetSystemMsTime());
#if !__FINAL
	else
	{
		if(!IsUsingDebugClock())
		{
		    // reset transition timer
		    m_nClockTransitionTimeLeft = m_nClockAndWeatherTransitionTime;

		    // start overridding with current time if we're kicking off a transition
		    if(m_nClockTransitionTimeLeft > 0)
		    {
			    m_nClockHour = CClock::GetHour();
			    m_nClockMinute = CClock::GetMinute();
			    m_nClockSecond = CClock::GetSecond();

			    CNetwork::OverrideClockTime(m_nClockHour, m_nClockMinute, m_nClockSecond, false);
		    }

		    // apply and override the clock time
		    CClock::SetTime(nHour, nMinute, nSecond);
	    }
	}
#endif

	// clock logging
	netTodDebug("ApplyClockAndWeather :: Current global time is %02d:%02d:%02d. Ratio: %.2f", nHour, nMinute, nSecond, GetGlobalClockRatio());

	// grab the global weather time
	s32 nCycleIndex;
	u32 nCycleTimer;
	GetGlobalWeather(nCycleIndex, nCycleTimer);

	if(!g_weather.GetNetIsOverriding())
	{
		// apply the global weather time
		g_weather.NetworkInitFromGlobal(nCycleIndex, nCycleTimer, m_nClockAndWeatherTransitionTime);
	}
	
	// weather logging
	NOTFINAL_ONLY(netTodDebug("ApplyClockAndWeather :: Current global weather is Index: %d, Timer: %d. Interp: %.2f, Ratio: %.2f, SystemTime: %u, Prev: %s, Next: %s, Overidding: %s", nCycleIndex, nCycleTimer, g_weather.GetInterp(), GetGlobalWeatherRatio(), g_weather.GetSystemTime(), g_weather.GetPrevType().m_name, g_weather.GetNextType().m_name, g_weather.GetNetIsOverriding() ? "T" : "F"));

    // global settings applied
    sm_bAppliedGlobalSettings = true; 
    m_LastClockUpdateTime = sysTimer::GetSystemMsTime();

	sm_lastDamageEventPendingOverrideEnabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PED_LAST_DAMAGE_EVENT_PENDING_OVERRIDE", 0x5974776f), true);
}

void CNetwork::UpdateClockAndWeather()
{
	// sync to global clock every second
	static const unsigned GLOBAL_CLOCK_SYNC_MS = 1000;

    // wait until we apply before updating
    if(!sm_bAppliedGlobalSettings)
        return;

    // current time
    unsigned nSysTime = sysTimer::GetSystemMsTime();

    // should we update to the global time
    bool bUpdateToGlobal = (nSysTime > m_NextClockSyncTime);

	// update clock transition
	if(m_nClockTransitionTimeLeft != 0)
	{
        unsigned nClockTime = m_nClockTransitionTimeLeft - (nSysTime - m_LastClockUpdateTime);
		if((nClockTime == 0) || (nClockTime > m_nClockTransitionTimeLeft) || CNetwork::IsClockOverrideFromScript())
		{
			netTodDebug("UpdateClockAndWeather :: NetClock - Clock Transition completed - HasDebug: %s", IsUsingDebugClock() ? "False" : "True");

			m_nClockTransitionTimeLeft = 0;

			if(!IsUsingDebugClock() && !CNetwork::IsClockOverrideFromScript())
			{
				CNetwork::ClearClockTimeOverride();

				// grab the global clock time
				int nHour, nMinute, nSecond;
				GetGlobalClock(nHour, nMinute, nSecond);

				// apply
				CClock::SetTime(nHour, nMinute, nSecond);
			}

			// flag next update
			m_NextClockSyncTime = nSysTime + GLOBAL_CLOCK_SYNC_MS;
		}
		else
		{
			m_nClockTransitionTimeLeft = nClockTime;

			if(!IsUsingDebugClock())
			{
			    // get the target clock time
			    // grab the global clock time
			    int nHour, nMinute, nSecond;
			    GetGlobalClock(nHour, nMinute, nSecond);

			    Time tTimeThen(m_nClockHour, m_nClockMinute, m_nClockSecond);
			    Time tTimeNow(nHour, nMinute, nSecond);

			    float fFrom = CClock::GetDayRatio(tTimeThen);
			    float fTo = CClock::GetDayRatio(tTimeNow);

			    float fClockInterp = 1.0f - Clamp(static_cast<float>(m_nClockTransitionTimeLeft) / static_cast<float>(m_nClockAndWeatherTransitionTime), 0.0f, 1.0f);
			    float fRatio = Lerp(SlowInOut(fClockInterp), fFrom, fTo);

			    // work back to hours, minutes, seconds
			    CClock::FromDayRatio(fRatio, nHour, nMinute, nSecond);
			    CNetwork::OverrideClockTime(nHour, nMinute, nSecond, false);
		    }
	    }
	}
	else
	{
		// keep in step with the global times
		if(CNetwork::IsNetworkOpen() && bUpdateToGlobal)
		{
#if !__FINAL
			if(CClock::HasDebugModifiedInNetworkGame() && NetworkInterface::IsHost())
			{
				// lock players to the debug clock so that we don't drift
				netTodDebug("UpdateClockAndWeather :: CLOCK :: Debug Modified: %s, Current: %02d:%02d:%02d", NetworkInterface::IsHost() ? "Host" : "Not Host", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());
				CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_DEBUG_MAINTAIN);
			}
			else
#endif
			if(CutSceneManager::GetInstance()->IsCutscenePlayingBack() || CClock::GetIsPaused())
			{
				netTodDebug("UpdateClockAndWeather :: CLOCK :: Cutscene: %s, Paused: %s, Current: %02d:%02d:%02d", CutSceneManager::GetInstance()->IsCutscenePlayingBack() ? "True" : "False", CClock::GetIsPaused() ? "True" : "False", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());
			}
#if !__FINAL
			else if(fwTimer::IsUserPaused())
			{
				netTodDebug("UpdateClockAndWeather :: CLOCK :: User Paused, Current: %02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());
			}
#endif
			else
			{ 
				// grab the global clock time
				int nHour, nMinute, nSecond;
				GetGlobalClock(nHour, nMinute, nSecond);

                // clock logging
                netTodDebug("UpdateClockAndWeather :: CLOCK :: Global: %02d:%02d:%02d, Current: %02d:%02d:%02d, Ratio: %.2f. Override: %s", nHour, nMinute, nSecond, CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond(), GetGlobalClockRatio(), CNetwork::IsClockTimeOverridden() ? "True" : "False");
                
                // stash in override store
				if(CNetwork::IsClockTimeOverridden() BANK_ONLY(&& !CClock::GetTimeOverrideFlagRef()))
				{
					CNetwork::HandleClockUpdateDuringOverride(static_cast<int>(nHour),
															  static_cast<int>(nMinute),
															  static_cast<int>(nSecond),
															  fwTimer::GetTimeInMilliseconds());
				}
				else
                {
					if (!IsUsingDebugClock())
					{
						CClock::SetTime(nHour, nMinute, nSecond);

						// grab the global date
						int nGameDay;
						GetGlobalDate(nGameDay);

						static const s32 MULTIPLAYER_YEAR_START = 2013; 
						CClock::SetDate(1, Date::JANUARY, MULTIPLAYER_YEAR_START);
						CClock::AddDays(nGameDay);
					}
				}
            }
        }
    }

	// update weather transition
    if(!g_weather.IsInNetTransition())
    {
        if(CNetwork::IsNetworkOpen() && bUpdateToGlobal)
        {
            if(!g_weather.GetIsForced() && !g_weather.IsInOvertimeTransition() && !g_weather.GetNetIsOverriding())
			{
				// grab the global weather time
				s32 nCycleIndex;
				u32 nCycleTimer;
				GetGlobalWeather(nCycleIndex, nCycleTimer);

				// apply the global weather time
				g_weather.NetworkInitFromGlobal(nCycleIndex, nCycleTimer, 0, true);

                // weather logging
                NOTFINAL_ONLY(netTodDebug("UpdateClockAndWeather :: WEATHER:: Global: Index: %d, Timer: %d, Interp: %.2f, Ratio: %.2f. Prev: %s, Next: %s", nCycleIndex, nCycleTimer, g_weather.GetInterp(), GetGlobalWeatherRatio(), g_weather.GetPrevType().m_name, g_weather.GetNextType().m_name);)
            }
            else
				netTodDebug("UpdateClockAndWeather :: WEATHER :: Not applying weather. Forced: %s, OverTime: %s, Overridden: %s", g_weather.GetIsForced() ? "True" : "False", g_weather.IsInOvertimeTransition() ? "True" : "False", g_weather.GetNetIsOverriding() ? "True" : "False");
        }
	}

    if(bUpdateToGlobal)
        m_NextClockSyncTime = nSysTime + GLOBAL_CLOCK_SYNC_MS;

	m_LastClockUpdateTime = nSysTime;
}

void CNetwork::StartClockTransitionToGlobal(unsigned nTransitionTime)
{
    if(nTransitionTime == 0)
    {
        int sTransitionTime = 0;
        bool bFound = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CLOCK_AND_WEATHER_TRANSITION_TIME", 0x8cb9ff24), sTransitionTime);	
        if(bFound)
            nTransitionTime = static_cast<unsigned>(sTransitionTime);
        else
        {
            static const unsigned DEFAULT_TRANSITION_TIME = 5000;
            nTransitionTime = DEFAULT_TRANSITION_TIME;
        }
    }

    // reset transition timer
    m_nClockTransitionTimeLeft = nTransitionTime;

    // grab the global clock time
    int nHour, nMinute, nSecond;
    GetGlobalClock(nHour, nMinute, nSecond);

    // do not transition when we have debug time applied
	if(!IsUsingDebugClock())
	{
        // reset transition timer
        m_nClockTransitionTimeLeft = nTransitionTime;

        // start overriding with current time if we're kicking off a transition
        if(m_nClockTransitionTimeLeft > 0)
        {
            m_nClockHour = CClock::GetHour();
            m_nClockMinute = CClock::GetMinute();
            m_nClockSecond = CClock::GetSecond();

			// apply and override the clock time to the current time
			CNetwork::OverrideClockTime(m_nClockHour, m_nClockMinute, m_nClockSecond, false);

			// clock logging
			netTodDebug("StartClockTransitionToGlobal :: Global: %02d:%02d:%02d, Current: %02d:%02d:%02d, Ratio: %.2f, Time: %d", nHour, nMinute, nSecond, m_nClockHour, m_nClockMinute, m_nClockSecond, GetGlobalClockRatio(), m_nClockTransitionTimeLeft);
        }
		else
		{
			// clock logging
			netTodDebug("StartClockTransitionToGlobal :: Instant - Global: %02d:%02d:%02d", nHour, nMinute, nSecond);
			
			// automatically apply the final time if we have a time of 0
			CClock::SetTime(nHour, nMinute, nSecond);
		}
    }

    // refresh this
    m_LastClockUpdateTime = sysTimer::GetSystemMsTime();
}

void CNetwork::StartWeatherTransitionToGlobal(unsigned nTransitionTime)
{
	if(nTransitionTime == 0)
	{
		int sTransitionTime = 0;
		bool bFound = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CLOCK_AND_WEATHER_TRANSITION_TIME", 0x8cb9ff24), sTransitionTime);	
		if(bFound)
			nTransitionTime = static_cast<unsigned>(sTransitionTime);
		else
		{
			static const unsigned DEFAULT_TRANSITION_TIME = 5000;
			nTransitionTime = DEFAULT_TRANSITION_TIME;
		}
	}

	// grab the global weather time
	s32 nCycleIndex;
	u32 nCycleTimer;
	GetGlobalWeather(nCycleIndex, nCycleTimer);

	// apply the global weather time
	g_weather.NetworkInitFromGlobal(nCycleIndex, nCycleTimer, nTransitionTime);

	// weather logging
	netTodDebug("StartWeatherTransitionToGlobal :: Current global weather is Index: %d, Timer: %d. Ratio: %.2f", nCycleIndex, nCycleTimer, GetGlobalWeatherRatio());
}

void CNetwork::StopApplyingGlobalClockAndWeather()
{
	netTodDebug("StopApplyingGlobalClockAndWeather");
	sm_bAppliedGlobalSettings = false;
}

float CNetwork::GetGlobalDateRatio()
{
    // apply global date - work out time for one full year in seconds
    u32 nGameYear_ms = CClock::GetMsPerGameMinute() * 60 * 24 * 365; 
    u32 nGameYear_s = nGameYear_ms / 1000;

    // current server time in seconds
    u64 nTimePOSIX = rlGetPosixTime();

    // work out how far through the day we are
    u32 nIntoYear_s = static_cast<u32>(nTimePOSIX) % nGameYear_s;

    // return as a ratio of how far through the year we are
    return static_cast<float>(nIntoYear_s) / static_cast<float>(nGameYear_s);
}

void CNetwork::GetGlobalDate(int& nGameDay)
{
    float fRatio = GetGlobalDateRatio();
    nGameDay = static_cast<int>(365.0f * fRatio);
}

float CNetwork::GetGlobalClockRatio()
{
	// work out time for one full day in seconds
	u32 nGameDay_ms = CClock::GetMsPerGameMinute() * 60 * 24; 
	u32 nGameDay_s = nGameDay_ms / 1000;

	// current server time in seconds
	u64 nTimePOSIX = rlGetPosixTime();

	// work out how far through the day we are
	u32 nIntoDay_s = static_cast<u32>(nTimePOSIX) % nGameDay_s;

	// return as a ratio of how far through the day we are
	return static_cast<float>(nIntoDay_s) / static_cast<float>(nGameDay_s);
}

void CNetwork::GetGlobalClock(int& nHour, int& nMinute, int& nSecond)
{
	float fRatio = GetGlobalClockRatio();
	CClock::FromDayRatio(fRatio, nHour, nMinute, nSecond);
}

float CNetwork::GetGlobalWeatherRatio()
{
	// current server time in seconds
	u64 nTimePOSIX = rlGetPosixTime();

	// time for the weather system to fully cycle (all of its cycles)
	u32 nWeatherSystemTime_ms = g_weather.GetSystemTime();
	u32 nWeatherSystemTime_s = nWeatherSystemTime_ms / 1000;

	// work out how far through the cycle we are
	u32 nIntoSystem_s = static_cast<u32>(nTimePOSIX) % nWeatherSystemTime_s;

	// return as a ratio of how far through the day we are
	return static_cast<float>(nIntoSystem_s) / static_cast<float>(nWeatherSystemTime_s);
}

void CNetwork::GetGlobalWeather(s32& nCycleIndex, u32& nCycleTimer)
{
	float fRatio = GetGlobalWeatherRatio();
	g_weather.GetFromRatio(fRatio, nCycleIndex, nCycleTimer);
}

s32 QSortEventCountersCompareFunc(const MetricEventTriggerOveruse::EventData* const eventA, const MetricEventTriggerOveruse::EventData* const eventB)
{
	return ((*eventA).m_triggerCount < (*eventB).m_triggerCount) ? 1 : -1;
}

void CNetwork::UpdateEventTriggerCounters()
{
	const unsigned DEFAULT_EVENT_TRIGGER_UPDATE_FREQUENCY_MS	= 0;
	const int DEFAULT_MAX_NUM_TRIGGERED_EVENTS_FOR_PERIOD		= 10;

	unsigned EVENT_TRIGGER_UPDATE_FREQUENCY_MS = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EVENT_TRIGGER_COUNTER_FREQUENCY_MS", 0xA9331CB5), DEFAULT_EVENT_TRIGGER_UPDATE_FREQUENCY_MS);
	if(EVENT_TRIGGER_UPDATE_FREQUENCY_MS != 0 && (GetSyncedTimeInMilliseconds() - sm_EventTriggerLastUpdate) > EVENT_TRIGGER_UPDATE_FREQUENCY_MS)
	{
		sm_EventTriggerLastUpdate = GetSyncedTimeInMilliseconds();

		int MAX_NUM_TRIGGERED_EVENTS_FOR_PERIOD = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EVENT_TRIGGER_COUNTER_LIMIT", 0x8D519F29), DEFAULT_MAX_NUM_TRIGGERED_EVENTS_FOR_PERIOD);

		atFixedArray<MetricEventTriggerOveruse::EventData, netGameEvent::NETEVENTTYPE_MAXTYPES> allEventsInfo;
		atFixedArray<u32, netGameEvent::NETEVENTTYPE_MAXTYPES>& eventCounters = sm_EventMgr->GetEventTriggerCounters();
		bool shouldTriggerTelemetry = false;
		u32 totalEventCount = 0;
		for(int i = 0; i < eventCounters.GetCount(); i++)
		{
			MetricEventTriggerOveruse::EventData d(i, eventCounters[i]);
			allEventsInfo.Push(d);
			if(eventCounters[i] >= MAX_NUM_TRIGGERED_EVENTS_FOR_PERIOD)
			{
				shouldTriggerTelemetry = true;
			}
			totalEventCount += eventCounters[i];
			eventCounters[i] = 0;
		}

		if(shouldTriggerTelemetry)
		{
			allEventsInfo.QSort(0, -1, QSortEventCountersCompareFunc);

			atFixedArray<MetricEventTriggerOveruse::EventData, MetricEventTriggerOveruse::MAX_EVENTS_REPORTED> topEvents;
			for(int i = 0; i < MetricEventTriggerOveruse::MAX_EVENTS_REPORTED; i++)
			{
				atHashString eventNameHash = atHashString(NetworkInterface::GetEventManager().GetEventNameFromType((NetworkEventType)allEventsInfo[i].m_eventType));
				allEventsInfo[i].m_eventHash = eventNameHash.GetHash();
				gnetDebug3("[EventTelemetry] triggering telemetry, adding event type: %d with hash: %d", allEventsInfo[i].m_eventType, allEventsInfo[i].m_eventHash);
				topEvents.Push(allEventsInfo[i]);
			}

			
			Vector3 playerPos(0,0,0);
			u32 playerRank = 0;

			if(netPlayer* localPlayer = netInterface::GetLocalPlayer())
			{
				playerRank = SafeCast(CNetGamePlayer, localPlayer)->GetCharacterRank();
			}
			if(CPed* player = CPedFactory::GetFactory()->GetLocalPlayer())
			{
				playerPos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
			}

			CNetworkTelemetry::AppendEventTriggerOveruse(playerPos,
				playerRank,
				MAX_NUM_TRIGGERED_EVENTS_FOR_PERIOD,
				EVENT_TRIGGER_UPDATE_FREQUENCY_MS,
				totalEventCount,
				topEvents);
		}
	}
}

void CNetwork::UpdateScuiHotkeys()
{
#if RSG_PC
	CControl& control = CControlMgr::GetPlayerMappingControl();

	// If the SCUI is disalbed for shutdown, do not attempt to show the UI.
	// If the Frontend Social Club button is pressed on the controller, do not attempt to show the UI. Keyboard only.
	// If the KEY_HOME is used, ignore it because this is handled in the RGSC SDK.
	if ( !CApp::IsScuiDisabledForShutdown() &&
		g_rlPc.GetUiInterface() && 
		g_rlPc.GetUiInterface()->IsHotkeyEnabled() &&
		!control.IsMappingInProgress() && CControlMgr::GetMainFrontendControl().GetFrontendSocialClubSecondary().IsPressed() &&
		CControlMgr::GetMainFrontendControl().GetFrontendSocialClubSecondary().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE &&
		CControlMgr::GetMainFrontendControl().GetFrontendSocialClubSecondary().GetSource().m_Parameter != KEY_HOME)
	{
		g_rlPc.ShowUi();
	}
#endif
}

#if RSG_ORBIS
void CNetwork::InitialiseLiveStreaming()
{
	int sceRet = sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);

	if (gnetVerifyf(!sm_bLiveStreamingInitialised, "Live streaming already initialised") &&
		gnetVerifyf(sceRet == SCE_OK, "Failed to load live streaming module. Error code: 0x%x", sceRet))
	{
		sceRet = sceGameLiveStreamingInitialize(SCE_GAME_LIVE_STREAMING_HEAP_SIZE);

		gnetAssertf(sceRet != SCE_GAME_LIVE_STREAMING_ERROR_OUT_OF_MEMORY, "Couldn't initialise live streaming - out of memory");

		if (sceRet == SCE_OK) 
		{
			sm_bLiveStreamingInitialised = true;

			EnableLiveStreaming(true);
		}
	}
}

void CNetwork::ShutdownLiveStreaming()
{
	int sceRet = 0;

	if (sm_bLiveStreamingInitialised)
	{
		sceRet = sceGameLiveStreamingTerminate();

		if (gnetVerifyf(sceRet == SCE_OK, "Failed to terminate live streaming module. Error code: 0x%x", sceRet))
		{
			sceRet = sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);

			gnetAssertf(sceRet == SCE_OK, "Failed to unload live streaming module. Error code: 0x%x", sceRet);
		}

		sm_bLiveStreamingInitialised = false;
	}
}

void CNetwork::EnableLiveStreaming(bool bEnable)
{
	if (gnetVerifyf(sm_bLiveStreamingInitialised == true, "Called EnableLiveStreaming when live streaming isn't initialised"))
	{
		NET_ASSERTS_ONLY(int sceRet = )sceGameLiveStreamingEnableLiveStreaming(bEnable); 
		gnetAssertf(sceRet == SCE_OK, "Failed to enable/disable live streaming. Error code: 0x%x", sceRet);
	}
}

bool CNetwork::IsLiveStreamingEnabled()
{
	if (sm_bLiveStreamingInitialised == false)
	{
		return false;
	}

	SceGameLiveStreamingStatus status;

	int sceRet = sceGameLiveStreamingGetCurrentStatus(&status);

	if (gnetVerifyf(sceRet == SCE_OK, "Failed to get status of live streaming module. Error code: 0x%x", sceRet))
	{
		return status.isOnAir;
	}

	return false;
}

#define MIN_VERSION_FOR_PROSPERO_CHECK (0x08000000u)

bool CNetwork::IsPlayingInBackCompatOnPS5()
{
#if !RSG_FINAL
	if (PARAM_netForceBackCompatAsPS5.Get() || PARAM_PS4BackwardsCompatibility.Get())
		return true;
#endif

	// assume default of true if online (in retail, this means we have the latest system software)
	bool allowProsperoCheck = NetworkInterface::IsLocalPlayerOnline();

#if !RSG_FINAL
	// in non-final, possible that some kits are not on the recommended software version (which will cause a PRX failure on calling sceKernelIsProspero)
	// so check that the version is valid here
	static bool s_HasCheckedSystemSoftware = false;
	static bool s_SystemSoftwareResult = false;
	if (!s_HasCheckedSystemSoftware)
	{
		char sysversion[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];
		if (SCE_OK == sceDbgGetSystemSwVersion(sysversion, SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1))
		{
			u32 hexver = 0;
			for (int i = SCE_SYSTEM_SOFTWARE_VERSION_LEN, j = 0; i >= 0; i--)
				if (sysversion[i] >= '0' && sysversion[i] <= '9')
					hexver += (sysversion[i] - '0') * pow(16, j++);

			// this check is only valid on software above MIN_VERSION_FOR_PROSPERO_CHECK
			s_SystemSoftwareResult = (hexver > MIN_VERSION_FOR_PROSPERO_CHECK);

			gnetDebug1("IsPlayingInBackCompatOnPS5 :: SystemVersion: %s [0x%08x], AllowCheck: %s", sysversion, hexver, s_SystemSoftwareResult ? "True" : "False");
			s_HasCheckedSystemSoftware = true;
		}
	}
	allowProsperoCheck = s_SystemSoftwareResult;
#endif

	// if allowed, allow a tunable kill-switch for this
	if (allowProsperoCheck && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PS4_ALLOW_PROSPERO_CHECK", 0x3fd9b373), true))
	{
		static bool s_HasCheckedProspero = false;
		static bool s_ProsperoCheckResult = false;
		if (!s_HasCheckedProspero)
		{
			s_ProsperoCheckResult = sceKernelIsProspero();
			gnetDebug1("IsPlayingInBackCompatOnPS5 :: Prospero: %s", s_ProsperoCheckResult ? "True" : "False");
			s_HasCheckedProspero = true;
		}
		return s_ProsperoCheckResult;
	}

	return false;
}
#endif // RSG_ORBIS

static const unsigned MAX_TRACKER_PARALLEL_STAGES = 3;
unsigned s_TrackerLastStageTimestamp[MAX_TRACKER_PARALLEL_STAGES] = { 0, 0, 0 };
static MetricTransitionTrack s_TrackerTelemetryMetric; 

#if !__NO_OUTPUT
static unsigned s_TrackerStartTimestamp = 0;
static unsigned s_TrackerStageCount[MAX_TRACKER_PARALLEL_STAGES] = { 0, 0, 0 };
#endif

void CNetwork::StartTransitionTracker(int nTransitionType, int nContextParam1, int nContextParam2, int nContextParam3)
{
#if !__NO_OUTPUT
	if(s_TrackerStartTimestamp != 0)
	{
		netTransitionAssertf(0, "AddStage :: %s - Haven't called NETWORK_TRANSITION_FINISH", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	netTransitionDebug("Start :: %s - Type: %u, Params: %u, %u, %u", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nTransitionType, nContextParam1, nContextParam2, nContextParam3);
	s_TrackerStartTimestamp = sysTimer::GetSystemMsTime();
#endif
	s_TrackerTelemetryMetric.Start(nTransitionType, nContextParam1, nContextParam2, nContextParam3);
}

void CNetwork::AddTransitionTrackerStage(int nStageSlot, int nStageHash, int nContextParam1, int nContextParam2, int nContextParam3)
{
	unsigned nPrevStageTime = s_TrackerLastStageTimestamp[nStageSlot] > 0 ? sysTimer::GetSystemMsTime() - s_TrackerLastStageTimestamp[nStageSlot] : 0;
#if !__NO_OUTPUT
	if((nStageSlot < 0) || (nStageSlot >= MAX_TRACKER_PARALLEL_STAGES))
	{
		netTransitionAssertf(0, "AddStage :: %s - Invalid Stage Slot: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nStageSlot);
		return;
	}
	if(s_TrackerStartTimestamp == 0)
	{
		netTransitionAssertf(0, "AddStage :: %s - Haven't called NETWORK_TRANSITION_START", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	s_TrackerStageCount[nStageSlot]++;
	netTransitionDebug("\tAddStage :: %s: Slot: %u, Count: %u, Hash: 0x%08x, Params: %u, %u, %u, Last Stage: %ums, Total: %ums", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nStageSlot, s_TrackerStageCount[nStageSlot], nStageHash, nContextParam1, nContextParam2, nContextParam3, nPrevStageTime, sysTimer::GetSystemMsTime() - s_TrackerStartTimestamp);
#endif
	s_TrackerLastStageTimestamp[nStageSlot] = sysTimer::GetSystemMsTime();

	if(!s_TrackerTelemetryMetric.AddStage(nStageSlot, nStageHash, nContextParam1, nContextParam2, nContextParam3, nPrevStageTime))
	{
#if !__NO_OUTPUT
		netTransitionDebug("\tAddStage :: Writing telemetry (Length: %u):", s_TrackerTelemetryMetric.GetLength());
		s_TrackerTelemetryMetric.LogContents();
#endif
		if(s_SendTransitionTrackerTelemetry)
		{
			APPEND_METRIC_FLUSH(s_TrackerTelemetryMetric, false);
		}
		s_TrackerTelemetryMetric.ClearStages();
		gnetVerify(s_TrackerTelemetryMetric.AddStage(nStageSlot, nStageHash, nContextParam1, nContextParam2, nContextParam3, nPrevStageTime));
	}
}

void CNetwork::FinishTransitionTracker(int OUTPUT_ONLY(nContextParam1), int OUTPUT_ONLY(nContextParam2), int OUTPUT_ONLY(nContextParam3))
{
#if !__NO_OUTPUT
	if(s_TrackerStartTimestamp == 0)
	{
		netTransitionAssertf(0, "AddStage :: %s - Haven't called NETWORK_TRANSITION_START", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	netTransitionDebug("Finish :: %s: Total Time: %u, Params: %u, %u, %u", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sysTimer::GetSystemMsTime() - s_TrackerStartTimestamp, nContextParam1, nContextParam2, nContextParam3);
	s_TrackerStartTimestamp = 0;
#endif

	for(unsigned i = 0; i < MAX_TRACKER_PARALLEL_STAGES; i++)
	{
		s_TrackerLastStageTimestamp[i] = 0;
#if !__NO_OUTPUT
		s_TrackerStageCount[i] = 0;
#endif
	}

	// write what we have left
	s_TrackerTelemetryMetric.Finish();

#if !__NO_OUTPUT
	netTransitionDebug("Finish :: Writing telemetry (Length: %u):", s_TrackerTelemetryMetric.GetLength());
	s_TrackerTelemetryMetric.LogContents();
#endif

	if(s_SendTransitionTrackerTelemetry)
	{
		APPEND_METRIC_FLUSH(s_TrackerTelemetryMetric, false);
	}
}

void 
CNetwork::ClearTransitionTracker()
{
#if !__NO_OUTPUT
	netTransitionDebug("ClearTransitionTracker");
	s_TrackerStartTimestamp = 0;
#endif

	for(unsigned i = 0; i < MAX_TRACKER_PARALLEL_STAGES; i++)
	{
		s_TrackerLastStageTimestamp[i] = 0;
#if !__NO_OUTPUT
		s_TrackerStageCount[i] = 0;
#endif
	}
}

#if !__FINAL
bool CNetwork::BypassMultiplayerAccessChecks()
{
	return PARAM_netBypassMultiplayerAccessChecks.Get() || PARAM_mpavailable.Get();
}
#endif

bool CNetwork::IsMultiplayerDisabled()
{
	if(Tunables::GetInstance().HasCloudTunables())
		return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MULTIPLAYER_DISABLED", 0x3c0af527), false);

	return false;
}

bool CNetwork::IsMultiplayerLocked()
{
#if !__FINAL
	if(BypassMultiplayerAccessChecks())
		return false; 
#endif

	return sm_bMultiplayerLocked;
}

void CNetwork::SetMultiplayerLocked(const bool multiplayerLocked)
{
	if(sm_bMultiplayerLocked != multiplayerLocked)
	{
		gnetDebug1("SetMultiplayerLocked :: %s -> %s", sm_bMultiplayerLocked ? "True" : "False", multiplayerLocked ? "True" : "False");
		sm_bMultiplayerLocked = multiplayerLocked;
	}
}

void CNetwork::SetSpPrologueRequired(const bool bIsRequired)
{
	if(sm_bSpPrologueRequired != bIsRequired)
	{
		gnetDebug1("SetSpPrologueRequired :: Setting: %s", bIsRequired ? "True" : "False");
		sm_bSpPrologueRequired = bIsRequired;
	}
}

bool CNetwork::IsSpPrologueRequired()
{
#if !__FINAL
	int nCommandValue;
	if(PARAM_netRequireSpPrologue.Get(nCommandValue))
		return nCommandValue != 0;

	if(BypassMultiplayerAccessChecks())
		return false;
#endif

	return sm_bSpPrologueRequired;
}

IntroSettingResult CNetwork::HasCompletedSpPrologue()
{
#if !RSG_FINAL
	int nCommandValue;
	if(PARAM_netSpPrologueComplete.Get(nCommandValue))
		return static_cast<IntroSettingResult>(nCommandValue);
#endif

	// check profile settings are valid
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(!settings.AreSettingsValid())
	{
		return IntroSettingResult::Result_Unknown;
	}

	// profile settings - check prologue
	if(!settings.Exists(CProfileSettings::PROLOGUE_COMPLETE))
	{
		return IntroSettingResult::Result_Unknown;
	}

	// get the value
	return static_cast<IntroSettingResult>(settings.GetInt(CProfileSettings::PROLOGUE_COMPLETE));
}

bool CNetwork::SetCompletedSpPrologue(const bool bHasCompleted)
{
	// check profile settings are valid
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(!settings.AreSettingsValid())
	{
		return false;
	}

	gnetDebug1("SetCompletedSpPrologue :: Completed: %s", bHasCompleted ? "True" : "False");
	settings.Set(CProfileSettings::PROLOGUE_COMPLETE, bHasCompleted ? IntroSettingResult::Result_Complete : IntroSettingResult::Result_NotComplete);
	settings.Write();

	return true;
}

bool CNetwork::CheckSpPrologueRequirement()
{
	// combines checks for required and completed
	return !IsSpPrologueRequired() || (HasCompletedSpPrologue() == IntroSettingResult::Result_Complete);
}

bool CNetwork::IsLastDamageEventPendingOverrideEnabled()
{
	return sm_lastDamageEventPendingOverrideEnabled;
}

#if !__NO_OUTPUT
const char* s_AccessAreaString[] =
{
	"AccessArea_Landing",
	"AccessArea_Multiplayer",
	"AccessArea_MultiplayerEnter",
};
CompileTimeAssert(COUNTOF(s_AccessAreaString) == eNetworkAccessArea::AccessArea_Num);
#endif

bool IsMultiplayerArea(const eNetworkAccessArea accessArea)
{
	switch(accessArea)
	{
	case AccessArea_Multiplayer:
	case AccessArea_Landing:
	default:
		return true;
	}
}

bool CNetwork::IsAccessCodeApplicableToArea(const eNetworkAccessCode nAccessCode, const eNetworkAccessArea nAccessArea)
{
	// no default, we want a compile failure for unhandled codes
	switch (nAccessCode)
	{
		// the return value indicates whether an error code is expected in the alert string
	case eNetworkAccessCode::Access_BankDefault:						return false;
	case eNetworkAccessCode::Access_Invalid:							return false;
	case eNetworkAccessCode::Access_Deprecated_TunableFalse:			return false;
	case eNetworkAccessCode::Access_Deprecated_TunableNotFound:			return false;

	case eNetworkAccessCode::Access_Granted:							return true;
	case eNetworkAccessCode::Access_Denied_MultiplayerLocked:			return IsMultiplayerArea(nAccessArea) && (nAccessArea != eNetworkAccessArea::AccessArea_Landing);
	case eNetworkAccessCode::Access_Denied_MultiplayerDisabled:			return IsMultiplayerArea(nAccessArea);

#if RSG_PC
		// on PC, we can create a profile on the SCUI flow so we're not required to be signed in here
	case eNetworkAccessCode::Access_Denied_NotSignedIn:					return true; // (nAccessArea != eNetworkAccessArea::AccessArea_SocialClub);
	case eNetworkAccessCode::Access_Denied_NotSignedOnline:				return true; // (nAccessArea != eNetworkAccessArea::AccessArea_SocialClub);
#else
	case eNetworkAccessCode::Access_Denied_NotSignedIn:					return true;
	case eNetworkAccessCode::Access_Denied_NotSignedOnline:				return true;
#endif

	case eNetworkAccessCode::Access_Denied_NoNetworkAccess:				return (nAccessArea == eNetworkAccessArea::AccessArea_Landing);
	//case eNetworkAccessCode::Access_Denied_NoRosCredentials:			return true;

#if RSG_GGP
		// on GGP, we cannot access multiplayer menus when we do not have the multiplayer / online privilege
	case eNetworkAccessCode::Access_Denied_NoOnlinePrivilege:			return IsMultiplayerArea(nAccessArea);
#else
	case eNetworkAccessCode::Access_Denied_NoOnlinePrivilege:			return IsMultiplayerArea(nAccessArea) && (nAccessArea != eNetworkAccessArea::AccessArea_Landing);
#endif

	// Script can't handle Access_Denied_NoRosCredentials so we only track it for the landing page right now	
	case eNetworkAccessCode::Access_Denied_NoRosCredentials:			return nAccessArea == eNetworkAccessArea::AccessArea_Landing;
	case eNetworkAccessCode::Access_Denied_NoRosPrivilege:				return IsMultiplayerArea(nAccessArea);

		// these are specific to the multiplayer enter, we have a transition module which waits for these to complete
	case eNetworkAccessCode::Access_Denied_NoTunables:					return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);
	case eNetworkAccessCode::Access_Denied_NoBackgroundScript:			return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);

		// these should only fail on the enter - covers the invite case where we bypass the landing screen but fail the invite
		// we still need to verify that the policies succeeded in this flow
	//case eNetworkAccessCode::Access_Denied_PoliciesNotAccepted:			return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);
	//case eNetworkAccessCode::Access_Denied_PoliciesDownloadFailed:		return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);

	case eNetworkAccessCode::Access_Denied_InvalidProfileSettings:		return IsMultiplayerArea(nAccessArea);
	case eNetworkAccessCode::Access_Denied_PrologueIncomplete:			return IsMultiplayerArea(nAccessArea);

	case eNetworkAccessCode::Access_Denied_RosBanned:					return (nAccessArea != eNetworkAccessArea::AccessArea_MultiplayerEnter);
	case eNetworkAccessCode::Access_Denied_RosSuspended:				return (nAccessArea != eNetworkAccessArea::AccessArea_MultiplayerEnter);

	//case eNetworkAccessCode::Access_Denied_InvalidSandbox:				return true;
	//case eNetworkAccessCode::Access_Denied_NoXstsToken:					return true;
	//case eNetworkAccessCode::Access_Denied_NetworkManifestError:		return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);
	//case eNetworkAccessCode::Access_Denied_AgeRestricted:				return (nAccessArea != eNetworkAccessArea::AccessArea_Store);
	//case eNetworkAccessCode::Access_Denied_GameUpdateRequired:			return true;
	//case eNetworkAccessCode::Access_Denied_SystemUpdateRequired:		return true;
	//case eNetworkAccessCode::Access_Denied_ServiceUnreachable:			return true;
	//case eNetworkAccessCode::Access_Denied_NoRelayServerCxn:			return IsMultiplayerArea(nAccessArea) && (nAccessArea != eNetworkAccessArea::AccessArea_MultiplayerLanding);
	//case eNetworkAccessCode::Access_Denied_IncompatibleCommandLine:		return true;
	//case eNetworkAccessCode::Access_Denied_IncompatibleHostsFile:		return true;
	//case eNetworkAccessCode::Access_Denied_RateLimitCloudConfigFailed:	return (nAccessArea == eNetworkAccessArea::AccessArea_MultiplayerEnter);
	}
	return false;
}

#if !__NO_OUTPUT
int s_BankNetworkAccessCode = eNetworkAccessCode::Access_BankDefault;
bool s_BankForceReadyForAccessCheck = false;
bool s_BankForceNotReadyForAccessCheck = false;
#endif

bool CNetwork::IsReadyToCheckNetworkAccess(const eNetworkAccessArea OUTPUT_ONLY(accessArea), IsReadyToCheckAccessResult* pResult)
{
#define IS_READY_RESULT_COMMON(x)   \
    if(pResult)                     \
        *pResult = x;               \
    return (x >= IsReadyToCheckAccessResult::Result_Ready_First) && (x <= IsReadyToCheckAccessResult::Result_Ready_Last);

#if RSG_OUTPUT
	// keep track of the last result for each area 
	static IsReadyToCheckAccessResult s_LastResult[] =
	{
		IsReadyToCheckAccessResult::Result_Invalid,
		IsReadyToCheckAccessResult::Result_Invalid,
		IsReadyToCheckAccessResult::Result_Invalid,
	};
	CompileTimeAssert(COUNTOF(s_LastResult) == eNetworkAccessArea::AccessArea_Num);

#define IS_READY_RESULT(x)																			\
    if(s_LastResult[accessArea] != x)																\
    {																								\
        gnetDebug1("IsReadyToCheckNetworkAccess :: %s - %s", s_AccessAreaString[accessArea], #x);	\
        s_LastResult[accessArea] = x;																\
    }																								\
    IS_READY_RESULT_COMMON(x);
#else
	// nothing to do in non-logging builds except bail out
#define IS_READY_RESULT(x)   \
    IS_READY_RESULT_COMMON(x);
#endif

#if !RSG_FINAL
	if(s_BankForceReadyForAccessCheck)
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_Ready_CommandLine);
	}
	else if(s_BankForceNotReadyForAccessCheck)
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_CommandLine);
	}
#endif

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	// if we've not been assigned a valid user index, we can check (and fail)
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_Ready_InvalidLocalGamerIndex);
	}

	// if we've recently resumed the game, allow the system a window of time to sort itself out.
	if(CLiveManager::HasRecentResume())
	{
		// we can early out of this window if we've gained network access, signed online and received credentials
		bool skipResumeCheck =
			NetworkInterface::IsLocalPlayerOnline() &&
			NetworkInterface::HasNetworkAccess() &&
			NetworkInterface::HasValidRosCredentials();

		if(!skipResumeCheck)
		{
			IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_RecentlyResumed);
		}
	}

	// if we don't have an internet connection, we can check (and fail)
	if(!NetworkInterface::HasNetworkAccess())
	{
		return true;
	}

#if RSG_NP
	// check if we're still awaiting an NP result
	if (!g_rlNp.HasAvailabilityResult(localGamerIndex))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForAvailabilityResult);
	}

	// if not, and Np is unavailable, we can check (and fail)
	if (!g_rlNp.IsNpAvailable(localGamerIndex))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_Ready_NpUnavailable);
	}
#endif

#if RSG_PC
	// On PC, wait for sign-in to complete.
	if (g_rlPc.GetProfileManager()->IsSigningIn())
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForSignInCompletion);
	}
#endif

	// check if we're waiting for ROS credentials. On Social Club platforms, the lack of
	// platform credentials indicates the user is offline.
	const rlRosLoginStatus loginStatus = rlRos::GetLoginStatus(localGamerIndex);
	if(loginStatus == RLROS_LOGIN_STATUS_IN_PROGRESS
		NON_SC_PLATFORMS_ONLY(|| loginStatus == RLROS_LOGIN_STATUS_NEED_PLATFORM_CREDENTIALS))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForRosCredentials);
	}

#define RSG_SUPPORT_SOCIAL_CLUB_LANDING (0)
#if RSG_SUPPORT_SOCIAL_CLUB_LANDING
	// to this point, we've checked everything we need to for Social Club
	if((accessArea == AccessArea_SocialClub) || (accessArea == AccessArea_Cloud))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_Ready_PassedAllChecks);
	}
#endif

#define RSG_SUPPORT_LOCKING (0)
#if RSG_SUPPORT_LOCKING
	// if we're requesting access to an area that can fail due to Access_Denied_MultiplayerLocked
	// make sure that the unlock state is ready to be checked
	if(IsAccessCodeApplicableToArea(eNetworkAccessCode::Access_Denied_MultiplayerLocked, accessArea) && !IsReadyToCheckUnlockState())
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_NotReadyToCheckUnlockState);
	}
#endif

	// disabled this for now - need to add support that we ever had a result notification
#if RSG_XBL && 0
	// for multiplayer areas, we need to wait for the multiplayer permission result (check HasFiredPrivilegeResult which indicates
	// that the game layer will have been told about the result - our privilege checks use the cached copies in NetworkPrivileges)
	if(IsMultiplayerArea(accessArea))
	{
		IPrivileges* privilegesApi = g_rlXbl.GetPrivilegesManager();
		if (privilegesApi && !privilegesApi->GetPrivilegeCheckResult(localGamerIndex, rlPrivileges::PRIVILEGE_MULTIPLAYER_SESSIONS))
		{
			IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForPrivilegeResult);
		}
	}
#endif

#if RSG_NP
	// wait for NP permissions / PS+ check to complete
	if(!g_rlNp.GetNpAuth().HasRetrievedPermissions(localGamerIndex))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForPermissions);
	}

	// if we don't have PS+, we can still qualify for online play via a PS+ event 
	// wait for the tunables / event file to come down from the cloud before proceeding
	// these files are both required in order to determine if we are able to able to bypass PS+ gating
	if(!g_rlNp.GetNpAuth().IsPlusAuthorized(localGamerIndex) &&
	   (!SocialClubEventMgr::Get().GetDataReceived() || !Tunables::IsInstantiated() || !Tunables::GetInstance().HasCloudTunables()))
	{
		IS_READY_RESULT(IsReadyToCheckAccessResult::Result_NotReady_WaitingForPlusEvent);
	}
#endif

	// all checks passed
	IS_READY_RESULT(IsReadyToCheckAccessResult::Result_Ready_PassedAllChecks);
}

eNetworkAccessCode CNetwork::CheckNetworkAccess(const eNetworkAccessArea nAccessArea, u64* endPosixTime)
{
    if(endPosixTime != nullptr)
        *endPosixTime = 0;

    if(!gnetVerify(nAccessArea >= eNetworkAccessArea::AccessArea_First && nAccessArea < eNetworkAccessArea::AccessArea_Num))
    {
        gnetError("CheckNetworkAccess :: Invalid Access Area supplied!"); 
		return eNetworkAccessCode::Access_Invalid;
    }

#if RSG_OUTPUT
	// keep track of the last code 
	static eNetworkAccessCode s_Code[] = 
    { 
        eNetworkAccessCode::Access_Invalid,
		eNetworkAccessCode::Access_Invalid,
		eNetworkAccessCode::Access_Invalid,
    };
    CompileTimeAssert(COUNTOF(s_Code) == eNetworkAccessArea::AccessArea_Num);
#endif

#define NETWORK_ACCESS_RESULT_COMMON(x)					\
    return (x);

#if RSG_OUTPUT
#define NETWORK_ACCESS_RESULT(x, y)                                                                                                                                     \
    if(s_Code[nAccessArea] != x)                                                                                                                                        \
    {                                                                                                                                                                   \
        gnetDebug1("CheckNetworkAccess :: %s - %s, %s (%s)", (x == eNetworkAccessCode::Access_Granted) ? "Yes" : "No", s_AccessAreaString[nAccessArea], #x, y);   \
        s_Code[nAccessArea]  = x;                                                                                                                                       \
    }                                                                                                                                                                   \
    NETWORK_ACCESS_RESULT_COMMON(x)
#else
#define NETWORK_ACCESS_RESULT(x, y) NETWORK_ACCESS_RESULT_COMMON(x)
#endif

#define NETWORK_ACCESS_CHECK(x, y, z)							\
    if(IsAccessCodeApplicableToArea(x, nAccessArea) && (y))		\
    {															\
        NETWORK_ACCESS_RESULT(x, z)								\
    }  

#if !RSG_FINAL
	if(s_BankNetworkAccessCode != eNetworkAccessCode::Access_BankDefault)
	{
		NETWORK_ACCESS_RESULT(static_cast<eNetworkAccessCode>(s_BankNetworkAccessCode), "Using -netMpAccessCode");
	}
#endif

    // add this check first so that we display the appropriate alert
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_MultiplayerLocked, NetworkInterface::IsMultiplayerLocked(), "Multiplayer Locked");

    // for landing area specifically, we want to let the player through when multiplayer is locked (and skip any further checks)
    // so that we show our specific UI related to this
    if(nAccessArea == eNetworkAccessArea::AccessArea_Landing && IsMultiplayerLocked())
    {
        NETWORK_ACCESS_RESULT(eNetworkAccessCode::Access_Granted, "Allowing Access to Landing when Multiplayer Locked")
    }

	NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoNetworkAccess, !NetworkInterface::HasNetworkAccess(), "No Network Access");

#if RSG_XBL
    // if our sandbox is invalid, all remote access is broken
    //NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_InvalidSandbox, !rlGetTitleId()->GetXblTitleId()->IsSandboxIdValid(), (char*)rlGetTitleId()->GetXblTitleId()->GetSandboxId());
#endif

#if RSG_OUTPUT
	//NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_IncompatibleCommandLine, NetworkInterface::Live::HasIncompatibleCommandLines(), "Incompatible Network Command Line");
#endif

#if RSG_PC && RSG_OUTPUT
	//if((!NetworkInterface::IsLocalPlayerOnline() || !NetworkInterface::IsLocalPlayerSignedIn()))
	//{
	//	NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_IncompatibleHostsFile, !rlCheckHostsFile(rlGetTitleId()->GetRosTitleId()->GetEnvironmentName()), "Hosts file not set up correctly");
	//}
#endif

    //NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_GameUpdateRequired, NetworkInterface::Live::IsGameUpdateRequired(), "Game Update Required");
    //NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_SystemUpdateRequired, NetworkInterface::Live::IsSystemUpdateRequired(), "System Update Required");
    //NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_AgeRestricted, NetworkInterface::Live::LocalPlayerHasAgeRestrictions(), "Restricted by Age");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NotSignedIn, !NetworkInterface::IsLocalPlayerSignedIn(), "Not Signed In");
    //NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoNetworkAccess, !NetworkInterface::Live::HasNetworkAccess(), "No Network Access");
	//NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_ServiceUnreachable, NetworkInterface::Live::IsOnlineServiceUnreachable(), "Online Service Unreachable");
	NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NotSignedOnline, !NetworkInterface::IsLocalPlayerOnline(), "Not Signed Online");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoOnlinePrivilege, !NetworkInterface::CheckOnlinePrivileges(),"No Online Privileges");

#define NET_ENABLE_XBL_CONFIG_CHECKS (0)
#if NET_ENABLE_XBL_CONFIG_CHECKS && RSG_XDK && RSG_BANK
	// if we don't have an SC ticket, check if it's because we can't get an XSTS token
	int xstsTokenResult = 0;
	if(!NetworkInterface::HasValidRosCredentials() && !rlPresence::GetXblTokenResult(&xstsTokenResult))
	{
		char xstsTokenError[128] = {0};
		formatf(xstsTokenError, "No ROS credentials. Failed to retrieve an XSTS token. Error code: 0x%08x", xstsTokenResult);
		NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoXstsToken, !NetworkInterface::HasValidRosCredentials(), xstsTokenError);
	}
	else
#endif
	{
		NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoRosCredentials, !NetworkInterface::HasValidRosCredentials(), "No ROS credentials");
	}

#if NET_ENABLE_XBL_CONFIG_CHECKS && RSG_XDK
	const u32 networkManifestResultCode = NetworkInterface::Live::GetNetworkManifestResultCode();
	if((networkManifestResultCode != 0) && !NetworkInterface::Live::HasRelayServerConnection() && !NetworkInterface::Live::HasPresenceServerConnection())
	{
		// an exception occurred while loading the network manifest, and it appears that our main socket port is blocked
		char manifestError[128] = {0};
		formatf(manifestError, "Failed to load the network manifest. Error code: 0x%08x", networkManifestResultCode);
		NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NetworkManifestError, networkManifestResultCode != 0, manifestError);
	}
#endif

#define NET_MANAGE_BANNED_STATUS (1)
#if NET_MANAGE_BANNED_STATUS
    if(NetworkInterface::HasValidRosCredentials())
    {
        bool granted = false;

        const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
        const bool hasEndDate = credentials.HasPrivilegeEndDate(RLROS_PRIVILEGEID_BANNED, &granted, endPosixTime);

        NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_RosBanned, NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_BANNED) && !hasEndDate, "Banned account");
        NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_RosSuspended, NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_BANNED) && hasEndDate, "Suspended account");
    }
#endif

    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoRosPrivilege, NetworkInterface::HasValidRosCredentials() && !NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_MULTIPLAYER), "No ROS Multiplayer privilege");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoRosPrivilege, NetworkInterface::HasValidRosCredentials() && !NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_CLOUD_STORAGE_READ), "No ROS CloudRead privilege");
	//NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoRosPrivilege, !NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_CONDUCTOR), "No ROS Conductor privilege");

#if !__FINAL
    // this skips any of the softer requirements for multiplayer access
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Granted, BypassMultiplayerAccessChecks(), "BypassMultiplayerAccessChecks")
#endif

#define NET_MANAGE_POLICY (0)
#if NET_MANAGE_POLICY
#if !RSG_FINAL
    if(!CLandingPage::ShouldShowOnBoot())
#endif
    {
        NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_PoliciesNotAccepted, SCPolicyManager::GetInstance().NeedsPoliciesAcceptance(), "Policies not accepted");
        NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_PoliciesDownloadFailed, !SCPolicyManager::GetInstance().IsInitialized(nullptr), "Policies download failed");
    }
#endif

    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_PrologueIncomplete,      !CheckSpPrologueRequirement(), "Prologue not Complete");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_MultiplayerDisabled,      IsMultiplayerDisabled(), "Multiplayer Disabled");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoTunables,              !Tunables::GetInstance().HasCloudTunables() NOTFINAL_ONLY(&& !PARAM_netMpSkipCloudFileRequirements.Get()), "Invalid Tunables");
	//NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_RateLimitCloudConfigFailed, !netHttpRateLimiter::HasConfigFile(), "Rate limit config failed");
    NETWORK_ACCESS_CHECK(eNetworkAccessCode::Access_Denied_NoBackgroundScript,      !SBackgroundScripts::GetInstance().HasCloudScripts() NOTFINAL_ONLY(&& !PARAM_netMpSkipCloudFileRequirements.Get()), "Invalid Background Scripts");

    // all checks pass, access granted
    NETWORK_ACCESS_RESULT(eNetworkAccessCode::Access_Granted, "All checks passed");
}

bool CNetwork::CanAccessNetworkFeatures(const eNetworkAccessArea accessArea, eNetworkAccessCode* pAccessCode)
{
	const eNetworkAccessCode accessCode = CheckNetworkAccess(accessArea);
	if(pAccessCode)
		*pAccessCode = accessCode;

	return accessCode == eNetworkAccessCode::Access_Granted;
}

eNetworkAccessCode CNetwork::CheckNetworkAccessAndPopulateErrorDetails(const eNetworkAccessArea nAccessArea, sNetworkErrorDetails& errorDetails)
{   
    u64 nEndPosixTime;
	eNetworkAccessCode nAccessCode = CheckNetworkAccess(nAccessArea, &nEndPosixTime);

    if(nAccessCode == eNetworkAccessCode::Access_Granted)
    {
        errorDetails.Reset();
    }
    else
    {
        gnetDebug1("CheckNetworkAccess :: Cannot access!");

        // throw up an alert screen
        errorDetails.m_ErrorMsg = GetNetworkAccessErrorString(nAccessArea, nAccessCode);

        int nErrorCode;
        bool bExpectsErrorCode = GetNetworkAccessErrorCode(nAccessCode, nErrorCode);

        errorDetails.m_ErrorCode = nErrorCode;
        errorDetails.m_EndPosixTime = nEndPosixTime;
        errorDetails.m_HasErrorCode = bExpectsErrorCode;
        errorDetails.m_AlertSource = AlertScreenSource::Source_CodeAccess;
        errorDetails.m_AlertContext = static_cast<u32>(nAccessCode);

#if RSG_OUTPUT
        errorDetails.m_DeveloperMode = !NetworkInterface::IsUsingCompliance();
#endif
    }

    return nAccessCode;
}

bool CNetwork::ShowSystemUIForAccessCode(eNetworkAccessCode const nAccessCode, bool& bNeedInGameUi_Out)
{
    bool bShowPlatformUI = false;
    bNeedInGameUi_Out = true;

    // intercept when we need to show platform UI
	switch (nAccessCode)
	{
	case eNetworkAccessCode::Access_Denied_NoOnlinePrivilege:
		{
			// disable this for now, QA won't be using -netCompliance on V
#if !RSG_FINAL && 0
			// in non-prod environments, we default to an in-game alert with help prompt where we can 
			// with guides on how to create online enabled platform accounts (instead of showing platform UI)
			if(!NetworkInterface::IsUsingCompliance() && g_rlTitleId->m_RosTitleId.GetEnvironment() != RLROS_ENV_PROD)
			{
				gnetDebug1("ShowSystemUIForAccessCode :: Skipping Account Upsell UI in Development Environments");
				break;
			}
#endif

#if RSG_NP
			gnetDebug1("ShowSystemUIForAccessCode :: Showing Account Upsell");
			CLiveManager::ShowAccountUpgradeUI();
			bShowPlatformUI = true;
			bNeedInGameUi_Out = false;
#elif RSG_XBOX
			gnetDebug1("ShowSystemUIForAccessCode :: Invoking Privilege Resolution");
			if(CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_MULTIPLAYER_SESSIONS, true))
			{
				bShowPlatformUI = true;
				bNeedInGameUi_Out = false;
			}
			else
			{
				gnetDebug1("ShowSystemUIForAccessCode :: Invoking Privilege Resolution skipped");
			}
#endif
		}
		break; 

	default:
		break;
	}

    return bShowPlatformUI;
}

bool CNetwork::CheckNetworkAccessAndAlertIfFail(const eNetworkAccessArea nAccessArea)
{
    eNetworkAccessCode nAccessCode = CheckNetworkAccessAndPopulateErrorDetails(nAccessArea, s_NetworkAlertParams.m_ErrorDetails);

    bool hasError = s_NetworkAlertParams.m_ErrorDetails.HasError();
    if(hasError)
    {
        bool bNeedInGameUi = true;
        ShowSystemUIForAccessCode(nAccessCode, bNeedInGameUi);
        if(bNeedInGameUi)
        {
            // setup and update once to show the screen
            SetupNetworkAlertScreen();
            UpdateNetworkAlertScreen();
        }
    }

    return !hasError;
}

const char* CNetwork::GetNetworkAccessErrorString(const eNetworkAccessArea NET_ASSERTS_ONLY(nAccessArea), const eNetworkAccessCode nAccessCode)
{
	// check that the code / area match
	gnetAssert(IsAccessCodeApplicableToArea(nAccessCode, nAccessArea));

	// no default, we want a compile failure for unhandled codes
	switch (nAccessCode)
	{
		// we can consider returning different messages for different areas here
	case eNetworkAccessCode::Access_BankDefault:						return nullptr;
	case eNetworkAccessCode::Access_Invalid:							return nullptr;
	case eNetworkAccessCode::Access_Granted:							return nullptr;
	case eNetworkAccessCode::Access_Denied_MultiplayerLocked:			return "ACCESS_DENIED_NETWORK_LOCKED";
		// V specific
	case eNetworkAccessCode::Access_Denied_InvalidProfileSettings:		return "ACCESS_DENIED_NETWORK_INVALID_PROFILE_SETTINGS";
	case eNetworkAccessCode::Access_Denied_PrologueIncomplete:			return "ACCESS_DENIED_NETWORK_PROLOGUE_INCOMPLETE";
	case eNetworkAccessCode::Access_Denied_MultiplayerDisabled:			return "ACCESS_DENIED_MULTIPLAYER_DISABLED";
	case eNetworkAccessCode::Access_Denied_NotSignedIn:					return "ACCESS_DENIED_NOT_SIGNED_IN";
	case eNetworkAccessCode::Access_Denied_NoNetworkAccess:				return "ACCESS_DENIED_NO_NETWORK_ACCESS";
	case eNetworkAccessCode::Access_Denied_NotSignedOnline:				return "ACCESS_DENIED_NOT_SIGNED_ONLINE";
		
	case eNetworkAccessCode::Access_Denied_NoRosCredentials:			return "ACCESS_DENIED_NO_SCS_CREDENTIALS";
	case eNetworkAccessCode::Access_Denied_NoRosPrivilege:				return "ACCESS_DENIED_NO_SCS_CREDENTIALS"; // "ACCESS_DENIED_NO_SCS_PRIVILEGE";
	case eNetworkAccessCode::Access_Denied_NoOnlinePrivilege:			return "ACCESS_DENIED_NO_ONLINE_PRIVILEGE";
	case eNetworkAccessCode::Access_Denied_NoTunables:					return "ACCESS_DENIED_NO_TUNABLES";
	case eNetworkAccessCode::Access_Denied_NoBackgroundScript:			return "ACCESS_DENIED_NO_BACKGROUND_SCRIPT";
	//case eNetworkAccessCode::Access_Denied_PoliciesNotAccepted:			return "ACCESS_DENIED_POLICIES_NOT_ACCEPTED";
	//case eNetworkAccessCode::Access_Denied_PoliciesDownloadFailed:		return "ACCESS_DENIED_POLICIES_DOWNLOAD_FAILED";
	//case eNetworkAccessCode::Access_Denied_PrologueIncomplete:			return "ACCESS_DENIED_PROLOGUE_INCOMPLETE";`
	case eNetworkAccessCode::Access_Denied_RosBanned:					return "ACCESS_DENIED_BANNED";
	case eNetworkAccessCode::Access_Denied_RosSuspended:				return "ACCESS_DENIED_SUSPENDED";
	//case eNetworkAccessCode::Access_Denied_InvalidSandbox:				return "ACCESS_DENIED_INVALID_SANDBOX";
	//case eNetworkAccessCode::Access_Denied_NoXstsToken:					return "ACCESS_DENIED_NO_XSTS_TOKEN";
	//case eNetworkAccessCode::Access_Denied_NetworkManifestError:		return "ACCESS_DENIED_NETWORK_MANIFEST_ERROR";
	//case eNetworkAccessCode::Access_Denied_AgeRestricted:				return "ACCESS_DENIED_AGE_RESTRICTED";
	//case eNetworkAccessCode::Access_Denied_GameUpdateRequired:			return "ACCESS_DENIED_GAME_UPDATE_REQUIRED";
	//case eNetworkAccessCode::Access_Denied_SystemUpdateRequired:		return "ACCESS_DENIED_SYSTEM_UPDATE_REQUIRED";
	//case eNetworkAccessCode::Access_Denied_ServiceUnreachable:			return "ACCESS_DENIED_SERVICE_UNREACHABLE";
	//case eNetworkAccessCode::Access_Denied_NoRelayServerCxn:			return "ACCESS_DENIED_NO_RELAY_SERVER_CXN";
	//case eNetworkAccessCode::Access_Denied_IncompatibleCommandLine:		return "ACCESS_DENIED_INCOMPATIBLE_COMMAND_LINE";
	//case eNetworkAccessCode::Access_Denied_IncompatibleHostsFile:		return "ACCESS_DENIED_INCOMPATIBLE_HOSTS_FILE";
	//case eNetworkAccessCode::Access_Denied_RateLimitCloudConfigFailed:	return "ACCESS_DENIED_RATE_LIMIT_CLOUD_CONFIG_FAILED";

		// deprecated - fall out to default return value
	case eNetworkAccessCode::Access_Deprecated_TunableNotFound:
	case eNetworkAccessCode::Access_Deprecated_TunableFalse:
		break;
	}
	return nullptr;
}

bool CNetwork::GetNetworkAccessErrorCode(const eNetworkAccessCode UNUSED_PARAM(nAccessCode), int& UNUSED_PARAM(nErrorCode))
{
	// for now, just use a base string
	return false;
}

bool CNetwork::GetBailExpectsErrorCode(const eBailReason UNUSED_PARAM(nReason))
{
	// for now, just use a base string
	return false;
}

enum BailPriority
{
	Bail_HighestPriority,
	Bail_HighPriority,
	Bail_NormalPriority,
	Bail_LowPriority,
};

BailPriority GetBailPriority(const eBailReason UNUSED_PARAM(nReason))
{
#if RSG_PC
#define PROFILE_CHANGE_PRIORITY Bail_HighestPriority
#else
#define PROFILE_CHANGE_PRIORITY Bail_HighPriority
#endif

	// all the same for now
	return Bail_NormalPriority;
}

void CNetwork::ShowBailWarningScreen()
{
	// prevent multiple claims on the bail warning screen (or stale claims)
	if(!sm_PendingBailWarningScreen)
	{
		gnetDebug1("ShowBailWarningScreen :: No outstanding claim");
		return;
	}

	sm_PendingBailWarningScreen = false;

	// if the incoming bail reason is lesser or equal priority to an existing alert 
	if(s_NetworkAlertParams.HasActiveMessage() && s_NetworkAlertParams.m_ErrorDetails.m_AlertSource == AlertScreenSource::Source_CodeBail)
	{
		if(GetBailPriority(sm_BailParams.m_BailReason) <= GetBailPriority(static_cast<eBailReason>(s_NetworkAlertParams.m_ErrorDetails.m_AlertContext)))
		{
			gnetDebug1("ShowBailWarningScreen :: Existing alert of equal or higher priority. Skipping alert.");
			return;
		}
	}

	s_NetworkAlertParams.m_ErrorDetails.m_ErrorMsg = NetworkUtils::GetBailReasonAsString(sm_BailParams.m_BailReason);
	s_NetworkAlertParams.m_ErrorDetails.m_ErrorCode = sm_BailParams.m_ErrorCode;
	s_NetworkAlertParams.m_ErrorDetails.m_EndPosixTime = sm_BailParams.m_EndPosixTime;
	s_NetworkAlertParams.m_ErrorDetails.m_HasErrorCode = GetBailExpectsErrorCode(sm_BailParams.m_BailReason);
	s_NetworkAlertParams.m_ErrorDetails.m_AlertSource = AlertScreenSource::Source_CodeBail;
	s_NetworkAlertParams.m_ErrorDetails.m_AlertContext = static_cast<u32>(sm_BailParams.m_BailReason);
	s_NetworkAlertParams.m_ResolveRequiresOnline = false;

#if RSG_OUTPUT
	s_NetworkAlertParams.m_ErrorDetails.m_DeveloperMode = !NetworkInterface::IsUsingCompliance();
#endif

	// setup update once to show the screen
	SetupNetworkAlertScreen();
	UpdateNetworkAlertScreen();
}

bool CNetwork::Bail(const sBailParameters bailParams, bool bSendScriptEvent)
{
#if !__NO_OUTPUT
	// log callstack
	sysStack::PrintStackTrace();
#endif

	// logging
	gnetDebug1("Network::Bail :: %s, Code: 0x%08x, Error: %s, Context: %d / %d / %d, EndTime: %" I64FMT "d",
		NetworkUtils::GetBailReasonAsString(bailParams.m_BailReason),
		bailParams.m_ErrorCode,
		bailParams.m_ErrorString ? bailParams.m_ErrorString : "None",
		bailParams.m_ContextParam1,
		bailParams.m_ContextParam2,
		bailParams.m_ContextParam3,
		bailParams.m_EndPosixTime);

	// check if we have an equal or higher priority bail reason pending
	if(sm_IsBailPending && (GetBailPriority(sm_BailParams.m_BailReason) < GetBailPriority(bailParams.m_BailReason)))
	{
		gnetDebug1("Network::Bail :: Ignoring, higher priority pending bail: %s", NetworkUtils::GetBailReasonAsString(sm_BailParams.m_BailReason));
		return false;
	}

	// action bails in our update loop
	sm_IsBailPending = true;
	
	// capture parameters
	sm_AlertScriptForBail = bSendScriptEvent;
	sm_BailParams = bailParams;

	return true; 
}

void CNetwork::BailImmediate(const sBailParameters& p)
{
	gnetDebug1("Network::BailImmediate :: Bailing");

	if(Bail(p))
	{
		gnetDebug1("Network::BailImmediate :: Actioning");
		ActionBail();
	}
}

void CNetwork::ActionBail()
{
	gnetDebug1("Network::ActionBail :: %s, Code: 0x%08x, Error: %s, Context: %d / %d / %d, EndTime: %" I64FMT "d",
		NetworkUtils::GetBailReasonAsString(sm_BailParams.m_BailReason),
		sm_BailParams.m_ErrorCode,
		sm_BailParams.m_ErrorString ? sm_BailParams.m_ErrorString : "None",
		sm_BailParams.m_ContextParam1,
		sm_BailParams.m_ContextParam2,
		sm_BailParams.m_ContextParam3,
		sm_BailParams.m_EndPosixTime);

	// reset pending flag
	sm_IsBailPending = true;

	// the bail event (if requested) fires regardless of the network being open - we might trigger a bail during matchmaking 
	if(sm_AlertScriptForBail)
	{
		gnetDebug1("Network::ActionBail :: Alerting script");
		GetEventScriptNetworkGroup()->Add(CEventNetworkBail(sm_BailParams.m_BailReason, sm_BailParams.m_ErrorCode));
	}

	// check if we should handle the bail alerting in code
	if(s_EnableCodeBailAlerting)
	{
		gnetDebug1("Network::ActionBail :: Handling bail warning screen");
		CNetwork::ShowBailWarningScreen();
	}

	// shutdown network session ahead of closing the managers - flag this as a sudden shutdown
	if(sm_NetworkSession)
	{
		sm_NetworkSession->OnBail(sm_BailParams);
		sm_NetworkSession->Shutdown(true, sm_BailParams.m_BailReason);
	}

	// only close if we are currently open
	CNetwork::CloseNetwork(true);

	// at the least, reset these
	sm_bFirstEntryNetworkOpen = false;
	sm_bFirstEntryMatchStarted = false;

	if(sm_bSkipRadioResetNextClose)
	{
		audDisplayf("CNetwork::ActionBail :: Resetting sm_bSkipRadioResetNextClose");
		sm_bSkipRadioResetNextClose = false;
	}

	// Also reset the 'skip reset on next open' flag
	sm_bSkipRadioResetNextOpen = false;

	// reinitialize network session
	if(sm_NetworkSession)
	{
		gnetVerifyf(sm_NetworkSession->Init(), "CNetwork::ActionBail - Failed to initialise NetworkSession");
	}

	// if we are bailing BAIL_SYSTEM_SUSPENDED - we need to make sure we cancel any 
	// pending saves or saves in progress and that we block save requests. 
	// Reference - url:bugstar:6007512
	if(eBailReason::BAIL_SYSTEM_SUSPENDED == sm_BailParams.m_BailReason)
	{
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().SetBlockSaveRequests(true);
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().CancelAndClearSaveRequests();
	}
}

bool DoesDateHaveMonthDelimiter(const sysLanguage language)
{
	switch(language)
	{
		// the localization entries for these languages already include the month delimiter
		// instead of adding new entries or potentially breaking other uses of those labels,
		// override the delimiter setting here
	case LANGUAGE_KOREAN:
	case LANGUAGE_CHINESE_TRADITIONAL:
	case LANGUAGE_CHINESE_SIMPLIFIED:
		return false;

	default:
		return fwLanguagePack::HasMonthDelimiter(language);
	}
}

void CNetwork::FormatPosixTimeAsUntilDate(const s64 posixTime, char* dateString, const unsigned maxLength)
{
	if(dateString == nullptr)
		return;

	time_t theTime = posixTime;
	tm tmTime = *gmtime(&theTime);

	const sysLanguage language = g_SysService.GetSystemLanguage();

	LocalizationKey monthLngKey;
	fwLanguagePack::GetMonthString(monthLngKey.getBufferRef(), tmTime.tm_mon + 1, false, fwLanguagePack::NeedsLowerCaseMonth(language));
	
	LocalizationKey dayLngKey;
	fwLanguagePack::GetDayOfTheMonthString(dayLngKey.getBufferRef(), tmTime.tm_mday);

	const fwLanguagePack::DATE_FORMAT dateFormat = fwLanguagePack::GetDateFormatType(language);

	static const unsigned MAX_TOKEN_LENGTH = 64;
	char szDay[MAX_TOKEN_LENGTH], szMonth[MAX_TOKEN_LENGTH], szYear[MAX_TOKEN_LENGTH];

	formatf(szDay, "%s", TheText.Get(dayLngKey.getBufferRef()));
	formatf(szMonth, "%s", TheText.Get(monthLngKey.getBufferRef()));
	formatf(szYear, "%u", tmTime.tm_year + 1900);

	// delimiters - include a tunable to remove these
	const bool allowDelimiters = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_DATE_ALLOW_DELIMITERS", 0xf90a681d), true);
	const bool hasDayDelimiter = allowDelimiters && fwLanguagePack::HasDayDelimiter(language);
	const bool hasMonthDelimiter = allowDelimiters && DoesDateHaveMonthDelimiter(language);
	const bool hasYearDelimiter = allowDelimiters && fwLanguagePack::HasYearDelimiter(language);

	// spacing is embedded in the delimiter strings
	static const unsigned MAX_DELIM_LENGTH = 8;
	char szDayDelim[MAX_DELIM_LENGTH], szMonthDelim[MAX_DELIM_LENGTH], szYearDelim[MAX_DELIM_LENGTH], szPreDelim[MAX_DELIM_LENGTH], szPostDelim[MAX_DELIM_LENGTH];
	formatf(szDayDelim, "%s", hasDayDelimiter ? TheText.Get("DATE_DAY_DELIM") : " ");
	formatf(szMonthDelim, "%s", hasMonthDelimiter ? TheText.Get("DATE_MONTH_DELIM") : " ");
	formatf(szYearDelim, "%s", hasYearDelimiter ? TheText.Get("DATE_YEAR_DELIM") : " ");

	// work out whether we pre/post-fix delimiters with spaces or not
	formatf(szPreDelim, "%s", fwLanguagePack::DoesPrefixDelimiterWithSpace(language) ? " " : "");
	formatf(szPostDelim, "%s", fwLanguagePack::DoesPostfixDelimiterWithSpace(language) ? " " : "");

	// use an atString to make this easier
	atString date;

	// need three passes - add the correct delimiter for each case
	switch(dateFormat)
	{
	case fwLanguagePack::DATE_FORMAT_DMY: date += szDay; date += hasDayDelimiter ? szPreDelim : ""; date += szDayDelim; date += hasDayDelimiter ? szPostDelim : ""; break;
	case fwLanguagePack::DATE_FORMAT_MDY: date += szMonth; date += hasMonthDelimiter ? szPreDelim : ""; date += szMonthDelim; date += hasMonthDelimiter ? szPostDelim : ""; break;
	case fwLanguagePack::DATE_FORMAT_YMD: date += szYear; date += hasYearDelimiter ? szPreDelim : ""; date += szYearDelim; date += hasYearDelimiter ? szPostDelim : ""; break;
	default: break;
	}

	switch(dateFormat)
	{
	case fwLanguagePack::DATE_FORMAT_DMY: date += szMonth; date += hasMonthDelimiter ? szPreDelim : ""; date += szMonthDelim; date += hasMonthDelimiter ? szPostDelim : ""; break;
	case fwLanguagePack::DATE_FORMAT_MDY: date += szDay; date += hasDayDelimiter ? szPreDelim : ""; date += szDayDelim; date += hasDayDelimiter ? szPostDelim : ""; break;
	case fwLanguagePack::DATE_FORMAT_YMD: date += szMonth; date += hasMonthDelimiter ? szPreDelim : ""; date += szMonthDelim; date += hasMonthDelimiter ? szPostDelim : ""; break;
	default: break;
	}

	// don't add a space for the last token if we don't have a delimiter
	switch(dateFormat)
	{
	case fwLanguagePack::DATE_FORMAT_DMY: date += szYear; date += hasYearDelimiter ? szPreDelim : ""; date += hasYearDelimiter ? szYearDelim : "";	/* no space after the final token */ break;
	case fwLanguagePack::DATE_FORMAT_MDY: date += szYear; date += hasYearDelimiter ? szPreDelim : ""; date += hasYearDelimiter ? szYearDelim : "";	/* no space after the final token */ break;
	case fwLanguagePack::DATE_FORMAT_YMD: date += szDay; date += hasDayDelimiter ? szPreDelim : ""; date += hasDayDelimiter ? szDayDelim : "";		/* no space after the final token */ break;
	default: break;
	}

	safecpy(dateString, date.c_str(), maxLength);
}

void CNetwork::SetupNetworkAlertScreen()
{
	// build the network alert sub-string
	if(s_NetworkAlertParams.m_ErrorDetails.m_HasErrorCode)
	{
		formatf(s_NetworkAlertParams.m_ErrorDetails.m_SubString, "0x%08x", static_cast<unsigned>(s_NetworkAlertParams.m_ErrorDetails.m_ErrorCode));
		s_NetworkAlertParams.m_ErrorDetails.m_HasSubString = true;
	}
	else if(s_NetworkAlertParams.m_ErrorDetails.HasEndTime())
	{
		FormatPosixTimeAsUntilDate(
			s_NetworkAlertParams.m_ErrorDetails.m_EndPosixTime,
			s_NetworkAlertParams.m_ErrorDetails.m_SubString,
			sNetworkErrorDetails::MAX_SUBSTRING_LENGTH);

		s_NetworkAlertParams.m_ErrorDetails.m_HasSubString = true;
	}
	else
	{
		s_NetworkAlertParams.m_ErrorDetails.m_HasSubString = false;
	}

	// close the calibration screen if open (warning screen compatibility)
	CPauseMenu::CloseDisplayCalibrationScreen();

	// manage existing alerts
	if(s_NetworkAlertParams.HasActiveMessage())
	{
		// nothing to do, the warning is re-posted each frame on V
	}
	else
	{
		s_NetworkAlertParams.m_InstanceHash = static_cast<u32>(fwRandom::GetRandomNumber());
	}

	// set the message as active
	s_NetworkAlertParams.SetActive();

#define NET_SEND_ALERT_TELEMETRY (0)
#if NET_SEND_ALERT_TELEMETRY
	// send alert telemetry
	NetworkMetricsCommonInterface::MultiplayerErrorAlert(
		s_NetworkAlertParams.m_ErrorDetails.m_AlertSource,
		s_NetworkAlertParams.m_ErrorDetails.m_AlertContext,
		atStringHash(s_NetworkAlertParams.m_ErrorDetails.m_ErrorMsg),
		s_NetworkAlertParams.m_ErrorDetails.m_ErrorCode,
		s_NetworkAlertParams.m_InstanceHash,
		CNetwork::IsNetworkErrorCodeExpected(s_NetworkAlertParams.m_ErrorDetails.m_ErrorCode),
		false);
#endif
}

void
CNetwork::UpdateNetworkAlertScreen()
{
	if(s_NetworkAlertParams.HasActiveMessage())
	{
		if(CWarningScreen::CheckAllInput() == FE_WARNING_CONTINUE)
		{
			CWarningScreen::Remove();
			ClearNetworkAlertScreen();
		}
		else
		{
			static const char* s_Heading = "HUD_CONNPROB";

			CWarningScreen::SetMessageWithHeader(
				WARNING_MESSAGE_STANDARD,
				s_Heading,
				s_NetworkAlertParams.m_ErrorDetails.m_ErrorMsg,
				FE_WARNING_CONTINUE,
				false,
				-1,
				s_NetworkAlertParams.m_ErrorDetails.m_HasSubString ? s_NetworkAlertParams.m_ErrorDetails.m_SubString : nullptr);
		}
	}
}

void CNetwork::ClearNetworkAlertScreen()
{
	if(s_NetworkAlertParams.HasActiveMessage())
	{
		gnetDebug1("ClearNetworkAlertScreen");
		s_NetworkAlertParams.ClearMessage();
	}
}

void CNetwork::CleanupNetworkAlertScreen()
{
	gnetDebug1("CleanupNetworkAlertScreen");
	ClearNetworkAlertScreen();
}

void CNetwork::OnPlatformInvitePassedChecks()
{
	gnetDebug1("OnPlatformInvitePassedChecks");
	ClearNetworkAlertScreen();
}

netConnectionManager&
CNetwork::GetConnectionManager()
{
    return sm_CxnMgr;
}

// #if __BANK

sysMemAllocator&
CNetwork::GetSCxnMgrAllocator()
{
	return *s_SCxnMgrAllocator;
}

// #endif // __BANK

void
CNetwork::SetScriptReadyForEvents(bool bReady)	
{ 
	gnetDebug1("SetScriptReadyForEvents :: %s", bReady ? "Ready" : "Not Ready");
	sm_bScriptReadyForEvents = bReady; 
}

void 
CNetwork::SetScriptAutoMuted(bool bIsAutoMuted)
{
    if(sm_bIsScriptAutoMuted != bIsAutoMuted)
    {
        gnetDebug1("SetScriptAutoMuted :: %s", bIsAutoMuted ? "True" : "False");
        sm_bIsScriptAutoMuted = bIsAutoMuted;
    }
}

bool 
CNetwork::IsCheater(const int nLocalPlayerIndex)
{
#if !__FINAL
	int nCheater = 0;
	if(PARAM_netForceCheater.Get(nCheater))
		return nCheater > 0 ? true : false; 
#endif
	int playerIndex = nLocalPlayerIndex >= 0 ? nLocalPlayerIndex : NetworkInterface::GetLocalGamerIndex();
	if(playerIndex<0)
	{
		gnetWarning("CNetwork::IsCheater called with an invalid localPlayerIndex");
		return false;
	}

	// this is the master setting
	if(rlRos::HasPrivilege(playerIndex, RLROS_PRIVILEGEID_CHEATER_POOL))
		return true;

	// check override
	if(CLiveManager::GetSCGamerDataMgr().HasCheaterOverride())
		return false;

	return CLiveManager::GetSCGamerDataMgr().IsCheater(); 
}

void
CNetwork::PlayerHasJoinedSession(const netPlayer& player)
{
    if(sm_bNetworkOpen)
    {
		CContextMenuHelper::PlayerHasJoined(player.GetPhysicalPlayerIndex());

        if (sm_bManagersAreInitialised)
        {
			sm_RoamingBubbleMgr.PlayerHasJoined(player);

#if ENABLE_NETWORK_BOTS
            CNetworkBot::OnPlayerHasJoined(player);
#endif // ENABLE_NETWORK_BOTS
        }

		if (!player.IsLocal())
		{
			CStatsMgr::GetStatsDataMgr().PlayerHasJoinedSession(player.GetActivePlayerIndex());
		}

		// Update the kick votes on player that joined
		if (NetworkInterface::IsGameInProgress() && player.IsRemote() && !player.IsBot())
		{
			const PlayerFlags votes = netPlayer::GetLocalPlayerKickVotes();
			if (votes)
			{
				CSendKickVotesEvent::Trigger(votes, 1<<player.GetPhysicalPlayerIndex());
			}

			if (NetworkInterface::IsHost())
			{
#if !__FINAL
				// sync modification to clock
				if(CClock::HasDebugModifiedInNetworkGame())
                    CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_DEBUG_CHANGE, &player);
                else
                    CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_DEBUG_RESET, &player);

				// sync forced weather
				if(g_weather.GetIsForced() && g_weather.GetNetSendForceUpdate())
					CDebugGameWeatherEvent::Trigger(g_weather.GetIsForced(), g_weather.GetPrevTypeIndex(), g_weather.GetCycleIndex(), &player);
#endif
			}
		}
    }
}

void
CNetwork::PlayerHasLeftSession(const netPlayer& player)
{
    if(sm_bNetworkOpen)
    {
        gnetAssert(!player.IsMyPlayer());
		CStatsMgr::GetStatsDataMgr().PlayerHasLeftSession(player.GetActivePlayerIndex());

		CContextMenuHelper::PlayerHasLeft(player.GetPhysicalPlayerIndex());
		
		if(SPlayerCardManager::IsInstantiated())
		{
			CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();
			CActivePlayerCardDataManager* pCard = static_cast<CActivePlayerCardDataManager*>(cardMgr.GetDataManager(CPlayerCardManager::CARDTYPE_ACTIVE_PLAYER));
			if(pCard && pCard->IsValid())
			{
				pCard->FlagForRefresh();
			}
		}

		// inform pickup manager player has left
		if (player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
		{
			CPickupManager::RemovePlayerFromAllProhibitedCollections(player.GetPhysicalPlayerIndex());
		}

		if (player.IsRemote() && !player.IsBot())
		{
			netPlayer::RemoveLocalKickVote(player);

			// Remove this player's kick votes for other players
			unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
			const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
			for(unsigned index = 0; index < numActivePlayers; index++)
			{
				netPlayer *pPlayer = (netPlayer *)activePlayers[index];
				pPlayer->RemoveKickVote(player);
			}
		}

        if (sm_bManagersAreInitialised)
        {
             sm_RoamingBubbleMgr.PlayerHasLeft(player);
#if ENABLE_NETWORK_BOTS
             CNetworkBot::OnPlayerHasLeft(player);
#endif // ENABLE_NETWORK_BOTS
    }
    }
}

void
CNetwork::PlayerHasJoinedBubble(const netPlayer& player)
{
	// the player must have a valid physical player index at this point
	gnetAssert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	if(sm_bNetworkOpen && sm_bManagersAreInitialised)
	{
		sm_BandwidthMgr.PlayerHasJoined(player);

		NetworkDebug::PlayerHasJoined(player);

		if (player.IsMyPlayer())
		{
			// TODO: when the local player joins a new bubble, the managers may have to be informed
            PhysicalPlayerIndex playerIndex = player.GetPhysicalPlayerIndex();

            if(gnetVerifyf(playerIndex != INVALID_PLAYER_INDEX, "Adding local player to a bubble with an invalid physical player index"))
            {
                if(sm_ObjectMgr)
                {
                    sm_ObjectMgr->GetObjectIDManager().SetObjectIdRangeIndex(playerIndex);
                }
            }

			sm_bGameInProgress = sm_bMatchStarted;
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetMessageLog(), "PLAYER_HAS_JOINED", "%s", player.GetLogName());

            if(sm_EventMgr)
            {
                sm_EventMgr->PlayerHasJoined(player); // event manager has to be called first as other managers may trigger network events
            }

            if(sm_ObjectMgr)
            {
			    sm_ObjectMgr->PlayerHasJoined(player);
            }

            if(sm_ArrayMgr)
            {
			    sm_ArrayMgr->PlayerHasJoined(player);
            }

			// also inform the script handler manager (the script handlers have network components). This could be done better with a system event.
			CTheScripts::GetScriptHandlerMgr().PlayerHasJoined(player);

			//Inform telemtry player has joined
			CNetworkTelemetry::PlayerHasJoined(player);
		}

		// inform other systems that are interested
		CGarages::PlayerHasJoined(player.GetPhysicalPlayerIndex());

		// reset flags that use physical player index
		sm_NetworkVoice.PlayerHasJoinedBubble(player.GetGamerInfo());
	}
}

//Grabs the player ped and set a bogus ped to be cleared during player removal.
//Must be done before the player network object is removed since the playerped is cleared during that removal.
static CPed*  LeavePedBehindForPlayerLeavingSession(CPed* ped, ObjectId& objId, const netPlayer& leavingPlayer)
{
	NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), leavingPlayer.GetPhysicalPlayerIndex(), "LEAVE_PED_BEHIND_FOR_LEAVING_PLAYER", "");

	gnetDebug1("LeavePedBehindForPlayerLeavingSession() - %s", leavingPlayer.GetLogName());
	respawnDebugf3("LeavePedBehindForPlayerLeavingSession() - %s", leavingPlayer.GetLogName());

	objId = NETWORK_INVALID_OBJECT_ID;
	ped = 0;

	if (!NetworkInterface::IsGameInProgress())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !NetworkInterface::IsGameInProgress()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !NetworkInterface::IsGameInProgress()");
		return 0;
	}

	CPed* localPlayerPed = FindPlayerPed();
	if(!localPlayerPed)
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed");
		return 0;
	}

	if(!localPlayerPed->GetNetworkObject())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()");
		return 0;
	}

	if(!localPlayerPed->GetNetworkObject()->GetPlayerOwner())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()->GetPlayerOwner()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()->GetPlayerOwner()");
		return 0;
	}

	if(!localPlayerPed->GetNetworkObject()->GetPlayerOwner()->IsPhysical())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()->GetPlayerOwner()->IsPhysical()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !localPlayerPed->GetNetworkObject()->GetPlayerOwner()->IsPhysical()");
		return 0;
	}

	if (!leavingPlayer.IsPhysical())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !leavingPlayer.IsPhysical()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !leavingPlayer.IsPhysical()");
		return 0;
	}

	if (!leavingPlayer.IsRemote())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !leavingPlayer.IsRemote()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !leavingPlayer.IsRemote()");
		return 0;
	}

	if (leavingPlayer.IsBot())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - leavingPlayer.IsBot()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - leavingPlayer.IsBot()");
		return 0;
	}

	if (PARAM_disableLeavePed.Get())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - PARAM_disableLeavePed.Get()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - PARAM_disableLeavePed.Get()");
		return 0;
	}

	ped = ((CNetGamePlayer*)(&leavingPlayer))->GetPlayerPed();

    // stop any synced scene from running on the leaving player ped
    if(ped && ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE))
    {
        ped->GetPedIntelligence()->FlushImmediately(true);
    }

	//If they aren't visible then don't leave a ped behind even to make then disappear later.
	//Likely script or fading out left them invisible.  If it was script and in the apartment need to isolate there rather than here.
	respawnDebugf3("Note: (ped[%p] && ped->GetNetworkObject()[%p] && ped->IsVisible()[%d]) localPlayerPed->GetPlayerInfo()->GetPlayerLeavePedBehind()[%d]",ped,ped ? ped->GetNetworkObject() : nullptr,ped ? ped->IsVisible() : false,localPlayerPed->GetPlayerInfo() ? localPlayerPed->GetPlayerInfo()->GetPlayerLeavePedBehind() : false);
	if (ped && ped->GetNetworkObject() && ped->IsVisible())
	{
		//Leave ped behind has been disabled.
		if (static_cast< CNetObjPlayer* >( ped->GetNetworkObject() )->GetDisableLeavePedBehind())
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - GetDisableLeavePedBehind()");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - GetDisableLeavePedBehind()");
			return 0;
		}

		respawnDebugf3("(ped && ped->GetNetworkObject() && ped->IsVisible())");

		objId = ped->GetNetworkObject()->GetObjectID();

		const CControlledByInfo networkNpcControl(true, false);

		Matrix34 tempMat;
		tempMat.Identity();
		tempMat.d = VEC3V_TO_VECTOR3( ped->GetTransform().GetPosition() );

		CPed* deletePed = CPedFactory::GetFactory()->CreatePed(networkNpcControl, ped->GetModelId(), &tempMat, true, false, false);
		if (gnetVerify( deletePed ))
		{
			respawnDebugf3("(deletePed)");

			u32 defaultRelationshipGroupHash = ped->GetPedIntelligence()->GetRelationshipGroupDefault()->GetName().GetHash();
			u32 relationshipGroupHash = ped->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash();

			//This can distort the relationship groups with the call to SetDefaultRelationshipGroup - use the above cached values and set them for the ped after ChangePlayerPed is invoked.
			CGameWorld::ChangePlayerPed(*ped, *deletePed);

            //Add the delete ped to the scene update - this ped gets flagged for deletion in the same frame and
            //won't get cleaned up if we don't do this
            deletePed->AddToSceneUpdate();

			//Establish the proper relationship group for the replacement peds
			CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(defaultRelationshipGroupHash);
			if( pGroup )
			{
				ped->GetPedIntelligence()->SetRelationshipGroupDefault(pGroup);
				deletePed->GetPedIntelligence()->SetRelationshipGroupDefault(pGroup);
			}

			pGroup = CRelationshipManager::FindRelationshipGroup(relationshipGroupHash);
			if( pGroup )
			{
				ped->GetPedIntelligence()->SetRelationshipGroup(pGroup);
				deletePed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
			}

			//Remove all weapons except unarmed weapons
			ped->GetInventory()->RemoveAllWeaponsExcept( ped->GetDefaultUnarmedWeaponHash() );
			deletePed->GetInventory()->RemoveAllWeaponsExcept( deletePed->GetDefaultUnarmedWeaponHash() );

			//Ensure that the ped will not fight if unarmed
			ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanFightArmedPedsWhenNotArmed);
			deletePed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanFightArmedPedsWhenNotArmed);

			ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_Aggressive);
			deletePed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_Aggressive);

			ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_AlwaysFight);
			deletePed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_AlwaysFight);

			//Ensure the ped isn't freaked out by shocking events
			ped->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingEvents, true);
			deletePed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingEvents, true);

			//Adjust health so that the created ped will have less health and die more easily
			ped->SetMaxHealth(200.f);
			deletePed->SetMaxHealth(200.f);

			if (ped->GetHealth() > 150.f)
				ped->SetHealth(150.f, true);

			if (deletePed->GetHealth() > 150.f)
				deletePed->SetHealth(150.f, true);

			ped->m_nPhysicalFlags.bNotDamagedByAnything = false; //ensure this is reset

			if(!localPlayerPed->GetPlayerInfo()->GetPlayerLeavePedBehind())
			{
				//This will enable special alpha ramping for the replacement ped then after an interval the ped will be removed from the world
				ped->SetSpecialNetworkLeave();
			}
			else
			{
				ped->SetSpecialNetworkLeaveWhenDead();
			}

			if (ped->GetMyVehicle() && ped->GetVehiclePedInside())
			{
				respawnDebugf3("(ped->GetMyVehicle() && ped->GetVehiclePedInside())");

				CVehicle* pVehicle = ped->GetMyVehicle();

				if (pVehicle->GetIsAircraft())
				{
					//Give a parachute
					ped->GetInventory()->AddWeapon(GADGETTYPE_PARACHUTE);
					deletePed->GetInventory()->AddWeapon(GADGETTYPE_PARACHUTE);
				}

				//By default place the previous player out of the vehicle, the newplayerped is what should remain as the previous player ped is being removed.
				//This will have the ped get out of the vehicle and allow the co-driver to take control.  If the ped is in rear seats they will just get out.
				//If they don't get out they tend to aggress the driver.
				bool bFixupPedVehicleState = true;

				//override - if the vehicle is scripted then the replacement ped should always get out.
				bool bScriptedVehicleOverride = pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) : false;

				if (!bScriptedVehicleOverride && pVehicle->IsDriver(ped))
				{
					CPed* pSeat1 = pVehicle->GetSeatManager()->GetPedInSeat(1);
					if (!pSeat1 || !pSeat1->IsPlayer())
					{	
						//If the player ped is in a vehicle alone, or in a vehicle with AI peds in the co-driver seat. Then let them remain in the seat and continue driving as a new AI ped.
						bFixupPedVehicleState = false;

						//clear out the rear brake as well
						if (pVehicle->InheritsFromBicycle())
						{
							CBike* pVehAsBike = static_cast<CBike*>(pVehicle);
							pVehAsBike->SetRearBrake(0.0f);
						}
					}
				}

				if (bFixupPedVehicleState)
				{
					respawnDebugf3("(bFixupPedVehicleState)");

					//Ensure on change of ownership the ped will try to exit the vehicle
					ped->SetExitVehicleOnChangeOwner(true);
					deletePed->SetExitVehicleOnChangeOwner(true);

					//Ensure the ped will flee when he leaves the vehicle
					ped->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_AlwaysFlee);
					deletePed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_AlwaysFlee);

					//Ensure the old ped tasks are cleared out and started out of the vehicle
					ped->GetPedIntelligence()->ClearPrimaryTask();
					ped->GetPedIntelligence()->ClearTaskEventResponse();
					ped->GetPedIntelligence()->AddTaskDefault(ped->ComputeDefaultTask(*ped));

					//Ensure the replacement ped tasks are cleared out and started out of the vehicle
					deletePed->GetPedIntelligence()->ClearPrimaryTask();
					deletePed->GetPedIntelligence()->ClearTaskEventResponse();
					deletePed->GetPedIntelligence()->AddTaskDefault(deletePed->ComputeDefaultTask(*deletePed));
				}
			}
			else
			{
				respawnDebugf3("ped is not in a vehicle -- SetMyVehicle(NULL) -- so he doesn't get back into a vehicle he previously was in");
				ped->SetMyVehicle(NULL); //ensure this is cleared out if the ped isn't in a vehicle
				deletePed->SetMyVehicle(NULL);

				// make sure the ped has collision
				ped->EnableCollision();
			}

			if (gnetVerify(ped != ((CNetGamePlayer*)(&leavingPlayer))->GetPlayerPed()))
			{
				respawnDebugf3("ped != ((CNetGamePlayer*)(&leavingPlayer))->GetPlayerPed()");

				if (ped->IsBaseFlagSet(fwEntity::IS_FIXED))
				{
					respawnDebugf3("ped->IsBaseFlagSet(fwEntity::IS_FIXED)-->ped->SetFixedPhysics(false, false)");
					ped->SetFixedPhysics(false, false);
				}

				respawnDebugf3("return ped");
				return ped;
			}
			else if(gnetVerify(deletePed) && gnetVerify(ped != deletePed))
			{
				gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - CGameWorld::ChangePlayerPed()");
				respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - CGameWorld::ChangePlayerPed()");
				CPedFactory::GetFactory()->DestroyPed(deletePed);
			}
		}
		else
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - CPedFactory::GetFactory()->CreatePed()");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - CPedFactory::GetFactory()->CreatePed()");
		}
	}
	else if(!ped)
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped");
	}
	else if(!ped->GetNetworkObject())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->GetNetworkObject()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->GetNetworkObject()");
	}
	else if(!ped->IsVisible())
	{
		gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible()");
		respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible()");

#if !__NO_OUTPUT
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_DEBUG ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_DEBUG");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_DEBUG");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_CAMERA ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_CAMERA");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_CAMERA");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_SCRIPT");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_SCRIPT");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_CUTSCENE ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_CUTSCENE");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_CUTSCENE");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_GAMEPLAY");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_GAMEPLAY");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_FRONTEND ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_FRONTEND");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_FRONTEND");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_VFX ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_VFX");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_VFX");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_NETWORK ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_NETWORK");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_NETWORK");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_PHYSICS ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PHYSICS");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PHYSICS");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_WORLD ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_WORLD");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_WORLD");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_DUMMY_CONVERSION ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_DUMMY_CONVERSION");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_DUMMY_CONVERSION");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_PLAYER ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PLAYER");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PLAYER");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_PICKUP ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PICKUP");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_PICKUP");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_FIRST_PERSON ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_FIRST_PERSON");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_FIRST_PERSON");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_VEHICLE_RESPOT ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_VEHICLE_RESPOT");
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_VEHICLE_RESPOT");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_RESPAWN ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_RESPAWN");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_RESPAWN");
		}
		if (!ped->GetIsVisibleForModule( SETISVISIBLE_MODULE_REPLAY ))
		{
			gnetDebug1("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_REPLAY");
			respawnDebugf3("FAIL LeavePedBehindForPlayerLeavingSession() - !ped->IsVisible() - SETISVISIBLE_MODULE_REPLAY");
		}
#endif // 
	}

	objId = NETWORK_INVALID_OBJECT_ID;
	ped = 0;

	return 0;
}

//Creates a new network object using the player ID and sets that network object to use the ped that was formelly with the player leaving.
//Must be done after the player network object is UnRegistered.
static void  CreateClonePedForPlayerLeavingSession(CPed* gameObject, const ObjectId objId, const netPlayer& leavingPlayer)
{
	if (!gameObject)
		return;

	bool result = false;

	//create a ped network object and set to have this gameobject. add it to the reassign list.
	if (gameObject && objId != NETWORK_INVALID_OBJECT_ID)
	{
		if (!CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true))
		{
			NetworkInterface::GetObjectManager().TryToMakeSpaceForObject(NET_OBJ_TYPE_PED);
		}

		if (CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true))
		{
			netObject* netObj = CPed::StaticCreateNetworkObject(objId, leavingPlayer.GetPhysicalPlayerIndex(), 0, MAX_NUM_PHYSICAL_PLAYERS);
			if (gnetVerify(netObj))
			{
				netObj->SetGameObject( gameObject );
				gameObject->SetNetworkObject( netObj );

				//Important: ensure that when ChangeOwner is called for the CNetObjPed after the ownership is established the remote clone will process as if it was previously a local then transitioned to clone.
				//This must happen otherwise the remote will not properly be setup and will not properly be in or out of a vehicle. lavalley.
				CNetObjPed* pNetObjPed = static_cast<CNetObjPed*>(netObj);
				if (pNetObjPed)
					pNetObjPed->SetForceCloneTransitionChangeOwner();

				NetworkInterface::GetObjectManager().RegisterNetworkObject(netObj);
				CNetwork::GetObjectManager().GetReassignMgr().AddObjectToReassignList(*netObj);

				//Add network event informing the ped id.
				CEventNetworkPedLeftBehind scrEvent(gameObject, CEventNetworkPedLeftBehind::REASON_PLAYER_LEFT_SESSION);
				GetEventScriptNetworkGroup()->Add(scrEvent);

				result = true;
			}
		}
	}

	//If it fails destroy the GameObject.
	if (!result)
	{
		//Ensure the count is properly updated.
		CPedPopulation::RemovePedFromPopulationCount(gameObject);

		CPedFactory::GetFactory()->DestroyPlayerPed(gameObject);
	}
}

void
CNetwork::PlayerHasLeftBubble(const netPlayer& player)
{
	// the player must still have a valid physical player index at this point
	gnetAssert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	if(sm_bNetworkOpen && sm_bManagersAreInitialised)
	{
		sm_BandwidthMgr.PlayerHasLeft(player);

		if (player.IsMyPlayer())
		{
			// TODO: when the local player leaves his current bubble, the managers will have to be informed and probably have to do some cleanup

			sm_bGameInProgress = false;
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetMessageLog(), "PLAYER_HAS_LEFT", "%s", player.GetLogName());

			// inform the script handler manager (the script handlers have network components). This could be done better with a system event.
			CTheScripts::GetScriptHandlerMgr().PlayerHasLeft(player);

            if(sm_ObjectMgr)
            {
				//Leave a ped behind so that this player does not disappear.
				ObjectId objId = NETWORK_INVALID_OBJECT_ID;
				CPed* ped = 0;
				ped = LeavePedBehindForPlayerLeavingSession(ped, objId, player);

			    sm_ObjectMgr->PlayerHasLeft(player);

				//create a ped network object and set to have this ped. add it to the reassign list.
				CreateClonePedForPlayerLeavingSession(ped, objId, player);
            }
			
            if(sm_EventMgr)
            {
                sm_EventMgr->PlayerHasLeft(player);
            }

            if(sm_ArrayMgr)
            {
			    sm_ArrayMgr->PlayerHasLeft(player);
            }

			// inform pickup manager player has left
			if (player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
			{
				CPickupManager::RemovePlayerFromAllProhibitedCollections(player.GetPhysicalPlayerIndex());
			}
		}

		// inform other systems that are interested
		CGarages::PlayerHasLeft(player.GetPhysicalPlayerIndex());

		// reset flags that use physical player index
		sm_NetworkVoice.PlayerHasLeftBubble(player.GetGamerInfo());
	}
}

bool
CNetwork::IsConnectionValid(const int connectionId)
{
    return sm_CxnMgr.IsOpen(connectionId)
            && NetworkInterface::GetPlayerFromConnectionId(connectionId);
}

netLoggingInterface&
CNetwork::GetMessageLog()
{
    return *s_MessageLog;
}

#if ENABLE_NETWORK_LOGGING

void CNetwork::OnAssertionFired()
{
    s_AssertionFiredLastFrame = true;
}

#endif // ENABLE_NETWORK_LOGGING

void CNetwork::FlushAllLogFiles(bool waitForFlush)
{
    static bool flushingLogFiles = false;

    if(!flushingLogFiles)
    {
        flushingLogFiles = true;

        if(sm_bManagersAreInitialised)
        {
            if(sm_ArrayMgr)
            {
                sm_ArrayMgr->GetLog().Flush(waitForFlush);
            }

            if(sm_EventMgr)
            {
                sm_EventMgr->GetLog().Flush(waitForFlush);
            }

            if(sm_ObjectMgr)
            {
                sm_ObjectMgr->GetLog().Flush(waitForFlush);
                sm_ObjectMgr->GetReassignMgr().GetLog().Flush(waitForFlush);
            }

            sm_PlayerMgr.GetLog().Flush(waitForFlush);
            sm_BandwidthMgr.GetLog().Flush(waitForFlush);
        }

        BANK_ONLY(NetworkDebug::FlushDebugLogs(waitForFlush));

        if(s_MessageLog)
        {
            s_MessageLog->Flush(waitForFlush);
        }

        if(CTheScripts::GetScriptHandlerMgr().GetLog())
        {
            CTheScripts::GetScriptHandlerMgr().GetLog()->Flush(waitForFlush);
        }
    }

    flushingLogFiles = false;
}

bool
CNetwork::CreateNetworkHeap()
{
	gnetDebug1("Create Network Heap");

    bool success = false;

    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssertf(!sm_pNetworkHeap, "Network heap has already been created");

    AcquireNetworkHeapMemory();

    if(sm_pHeapBuffer)
    {
        sm_pNetworkHeap = rage_new sysMemSimpleAllocator(sm_pHeapBuffer, NETWORK_HEAP_SIZE, sysMemSimpleAllocator::HEAP_NET);

#if RAGE_TRACKING
		diagTracker* t = diagTracker::GetCurrent();
		if (t && sysMemVisualize::GetInstance().HasNetwork())
		{
			t->InitHeap("Network Heap", sm_pHeapBuffer, NETWORK_HEAP_SIZE);
		}
#endif // RAGE_TRACKING

#if __ASSERT
        sm_pNetworkHeap->BeginLayer();
#endif  //__ASSERT

        success = true;
    }

    return success;
}

void
CNetwork::DeleteNetworkHeap()
{
	gnetDebug1("Delete Network Heap");

    gnetAssertf(sm_pNetworkHeap, "Network heap doesn't exist");
    gnetAssertf(!s_heapStackSize, "Network heap is still being used!");

#if __ASSERT
    const int numLeaks = sm_pNetworkHeap->EndLayer("CNetwork Heap", NULL);
    gnetAssertf(0 == numLeaks, "CNetwork::DeleteNetworkHeap(): Network heap leaked:%d allocations", numLeaks);
#endif  //__ASSERT

    RelinquishNetworkHeapMemory();

    delete sm_pNetworkHeap;
    sm_pNetworkHeap = 0;
}

void CNetwork::UseNetworkHeap(NetworkMemoryCategory BANK_ONLY(category))
{
    gnetAssertf(sm_pNetworkHeap != 0, "Network heap doesn't exist");

    sysMemStartNetwork();

#if __BANK
	gnetAssertf(CNetwork::GetNetworkHeap() != NULL, "Network heap does not exist!");

    NetworkMemoryCategory currentCategory = s_MemCategoryStack[s_CurrentMemCategory];

    if(CNetwork::GetNetworkHeap() != NULL)
    {
        int currentHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

        if((category != currentCategory) && (currentCategory != NET_MEM_NUM_MANAGERS))
        {
            if(currentHeapUsage >= s_PreviousHeapUsage)
            {
                NetworkDebug::RegisterMemoryAllocated(currentCategory, (currentHeapUsage - s_PreviousHeapUsage));
            }
            else
            {
                NetworkDebug::RegisterMemoryDeallocated(currentCategory, (s_PreviousHeapUsage - currentHeapUsage));
            }
        }

        if(gnetVerifyf(s_CurrentMemCategory < (MAX_CATEGORY_STACK_LEVELS - 1), "Memory tracking stack depth is too high!"))
        {
            s_CurrentMemCategory++;
            s_MemCategoryStack[s_CurrentMemCategory] = category;
        }

        s_PreviousHeapUsage = currentHeapUsage;
    }

#endif // __BANK
}

void
CNetwork::StopUsingNetworkHeap()
{
	sysMemEndNetwork();

#if __BANK
    if(sm_pNetworkHeap)
    {
        NetworkMemoryCategory currentCategory = s_MemCategoryStack[s_CurrentMemCategory];

        if(currentCategory != NET_MEM_NUM_MANAGERS)
        {
            int currentHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

            if(currentHeapUsage >= s_PreviousHeapUsage)
            {
                NetworkDebug::RegisterMemoryAllocated(currentCategory, (currentHeapUsage - s_PreviousHeapUsage));
            }
            else
            {
                NetworkDebug::RegisterMemoryDeallocated(currentCategory, (s_PreviousHeapUsage - currentHeapUsage));
            }

            if(gnetVerifyf(s_CurrentMemCategory > 0, "Calling StopUsingNetworkHeap() too many times!"))
            {
                s_CurrentMemCategory--;
                s_PreviousHeapUsage = currentHeapUsage;
            }
        }
	}
#endif // __BANK
    }

void CNetwork::AcquireNetworkHeapMemory()
{
    if (sm_HeapBufferRefCount++ == 0)
    {
        gnetAssert(sm_pHeapBuffer==NULL);

#if RSG_ORBIS
		sm_pHeapBuffer = sysMemFlexAllocate(NETWORK_HEAP_SIZE);
#else
		gnetAssertf(sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetLargestAvailableBlock() > NETWORK_HEAP_SIZE, "No block large enough to hold the network memory required");
		sm_pHeapBuffer = sysMemAllocator::GetCurrent().RAGE_LOG_ALLOCATE_HEAP(NETWORK_HEAP_SIZE, 16, MEMTYPE_GAME_VIRTUAL);
#endif        
    }
}

void CNetwork::RelinquishNetworkHeapMemory()
{
    if (--sm_HeapBufferRefCount == 0)
    {
#if RSG_ORBIS
		sysMemFlexFree(sm_pHeapBuffer, NETWORK_HEAP_SIZE);
#else
		sysMemAllocator::GetCurrent().Free(sm_pHeapBuffer);
#endif        
        sm_pHeapBuffer = NULL;
    }
}

sysMemAllocator*
CNetwork::GetNetworkHeap()
{
    return  sm_pNetworkHeap;
}

unsigned
CNetwork::GetTimeoutTime()
{
	unsigned timeout = static_cast<unsigned>(DEFAULT_CONNECTION_TIMEOUT);

#if !__FINAL
	unsigned t = 0;
	if(PARAM_notimeouts.Get())
	{
		timeout = 0;
	}
	else if(PARAM_timeout.Get(t))
	{
		timeout = t * 1000;
	}
#endif

	return timeout;
}

#if !__FINAL
bool CNetwork::IsTimeoutModifiedByCommandLine()
{
	if(PARAM_notimeouts.Get() || PARAM_timeout.Get())
		return true;

	return false;
}
#endif

void CNetwork::SetSyncedTimeInMilliseconds(unsigned time)
{
	sm_syncedTimeInMilliseconds = time;
}

unsigned CNetwork::GetNetworkTimeLocked()
{
	return sm_lockedNetworkTime;
}

unsigned CNetwork::GetNetworkTimePausable()
{
    if(s_PausableNetworkTimeStopped)
    {
        return s_TimePausableNetworkTimeStopped;
    }
    else
    {
        return sm_lockedNetworkTime - s_PausableNetworkTimeOffset;
    }
}

void CNetwork::StopNetworkTimePausable()
{
    if(!s_PausableNetworkTimeStopped)
    {
        gnetDebug1("Stopping pausable network time at %d (offset: %d)", sm_lockedNetworkTime, s_PausableNetworkTimeOffset);
        s_TimePausableNetworkTimeStopped = CNetwork::GetNetworkTimePausable();
        s_PausableNetworkTimeStopped     = true;
    }
}

void CNetwork::RestartNetworkTimePausable()
{
    if(s_PausableNetworkTimeStopped)
    {
        gnetDebug1("Restarting pausable network time at %d (offset: %d)", sm_lockedNetworkTime - s_PausableNetworkTimeOffset, s_PausableNetworkTimeOffset);
        s_TimePausableNetworkTimeStopped = 0;
        s_PausableNetworkTimeStopped     = false;
    }
}

#if !__NO_OUTPUT
void CNetwork::DumpNetworkTimeInfo()
{
	gnetDebug1("DumpNetworkTimeInfo :: NetworkTime: %u", GetNetworkTime());
	gnetDebug1("DumpNetworkTimeInfo :: NetworkTimeLocked: %u", GetNetworkTimeLocked());
	gnetDebug1("DumpNetworkTimeInfo :: SyncedTimeMs: %u", sm_syncedTimeInMilliseconds);
	gnetDebug1("DumpNetworkTimeInfo :: FwTimeMs: %u", fwTimer::GetTimeInMilliseconds());
	gnetDebug1("DumpNetworkTimeInfo :: IsDebugPaused: %s", fwTimer::IsUserPaused() ? "True" : "False");
	gnetDebug1("DumpNetworkTimeInfo :: DebugPausedLastFrame: %s", sm_WasDebugPausedLastFrame ? "True" : "False");
	gnetDebug1("DumpNetworkTimeInfo :: TimeWhenPaused: %u", sm_NetworkTimeWhenDebugPaused);
	sm_networkClock.Dump();
}
#endif

// Name         :   RenderNetworkInfo
// Purpose      :   called during render to display network info on the screen
void CNetwork::RenderNetworkInfo()
{
    if(sm_ObjectMgr)
    {
        sm_ObjectMgr->DisplayObjectInfo();
    }

    CFakePlayerNames::RenderFakePlayerNames();
}

void CNetwork::SetGoStraightToMultiplayer(bool bGoStraightToMultiplayer)
{
	if(sm_bGoStraightToMultiplayer != bGoStraightToMultiplayer)
	{
		gnetDebug1("SetGoStraightToMultiplayer :: %s", bGoStraightToMultiplayer ? "True" : "False");
		sm_bGoStraightToMultiplayer = bGoStraightToMultiplayer;
	}
}

bool CNetwork::GetGoStraightToMultiplayer()
{
	return sm_bGoStraightToMultiplayer;
}

void CNetwork::SetGoStraightToMPEvent(bool bGoStraightToMpEvent)
{
	if(sm_bGoStraightToMpEvent != bGoStraightToMpEvent)
{
		gnetDebug1("SetGoStraightToMPEvent :: %s", bGoStraightToMpEvent ? "True" : "False");
		sm_bGoStraightToMpEvent = bGoStraightToMpEvent;
	}
}

bool CNetwork::GetGoStraightToMPEvent( )
{
    return sm_bGoStraightToMpEvent;
}

void CNetwork::SetGoStraightToMPRandomJob(bool bGoStraightToMPRandomJob)
{
	if(sm_bGoStraightToMPRandomJob != bGoStraightToMPRandomJob)
	{
		gnetDebug1("SetGoStraightToMPRandomJob :: %s", bGoStraightToMPRandomJob ? "True" : "False");
		sm_bGoStraightToMPRandomJob = bGoStraightToMPRandomJob;
	}
}

bool CNetwork::GetGoStraightToMPRandomJob( )
{
	return sm_bGoStraightToMPRandomJob;
}

void CNetwork::SetShutdownSessionClearsTheGoStraightToMultiplayerFlag(bool bShutdownSessionClearsTheFlag)
{
	if(sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag != bShutdownSessionClearsTheFlag)
	{
		gnetDebug1("SetShutdownSessionClearsTheGoStraightToMultiplayerFlag :: Shutdown - %s", bShutdownSessionClearsTheFlag ? "True" : "False");
		sm_bShutdownSessionClearsTheGoStraightToMultiplayerFlag = bShutdownSessionClearsTheFlag;
	}

	if(sm_bGoStraightToMpEvent != bShutdownSessionClearsTheFlag)
	{
		gnetDebug1("SetShutdownSessionClearsTheGoStraightToMultiplayerFlag :: Event %s", bShutdownSessionClearsTheFlag ? "True" : "False");
		sm_bGoStraightToMpEvent = bShutdownSessionClearsTheFlag;
	}
}

// Name         :   ResetConnectionPolicies
// Purpose      :   
void CNetwork::ResetConnectionPolicies(const unsigned timeout, const unsigned keepAliveInterval)
{
	gnetDebug1("ResetConnectionPolicies :: Timeout: %u, KeepAliveInterval: %u", timeout, keepAliveInterval);

	netConnectionPolicies policies;
	policies.SetTimeout(timeout);
	policies.SetKeepAliveInterval((timeout == 0) ? 0 : ((keepAliveInterval > 0) ? keepAliveInterval : CNetwork::DEFAULT_CONNECTION_KEEP_ALIVE_INTERVAL));

	//Set the default policies for all new connections on all channels.
	//Policies on existing connections are not changed.
	sm_CxnMgr.SetDefaultConnectionPolicies(policies);

    // sets up session policies based on timeout
    CNetworkSession::InitRegistrationPolicies();

	//Sets the policies for all existing connections.
	gnetVerify(sm_CxnMgr.SetConnectionPolicies(policies));
}

#if __BANK

// Name         :   InitWidgets
// Purpose      :   Initialises network widgets
void CNetwork::InitWidgets()
{
	NetworkDebug::InitWidgets();
    Tunables::GetInstance().InitWidgets();
}

void CNetwork::InitLevelWidgets()
{
}

void CNetwork::ShutdownLevelWidgets()
{
}

#endif  //__BANK

//private:

bool
CNetwork::OpenNetwork()
{
    if(!gnetVerify(!sm_bNetworkOpen))
	{
		gnetError("OpenNetwork :: Network already open!");
		if(sm_bFatalErrorForDoubleOpen)
			Quitf(0, "OpenNetwork :: Network already open!");

		// already open
		return true;
	}

	sm_bNetworkOpening = true; 

	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PushProfiler("CNetwork::OpenNetwork", PARAM_netFunctionProfilerAsserts.Get()));
	unsigned profilerTreshold=0;
	PARAM_netFunctionProfilerThreshold.Get(profilerTreshold);
	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().SetTimeThreshold(profilerTreshold));

	Displayf("OpenNetwork(): Network Heap used %" SIZETFMT "d, available %" SIZETFMT "d, size %" SIZETFMT "d", CNetwork::sm_pNetworkHeap->GetMemoryUsed(), CNetwork::sm_pNetworkHeap->GetMemoryAvailable(), CNetwork::GetNetworkHeap()->GetHeapSize());

    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(!(sm_NetworkSession && sm_NetworkSession->IsSessionCreated()));
#if __WIN32PC || RSG_DURANGO
	GRCDEVICE.SetAllowBlockUpdateThread(false);
#endif

	NOTFINAL_ONLY(bool bIsFirstEntryNetworkOpen = sm_bFirstEntryNetworkOpen;)
	gnetDebug1("OpenNetwork :: Is First Entry: %s", sm_bFirstEntryNetworkOpen ? "False" : "True");

	// Make sure that no Binks are allocated from the Network heap
	NPROFILE(g_movieMgr.DeleteAll())

	network_commands::ResetTrackingVariables();

	NPROFILE(CNetwork::InitManagers())
	
	NPROFILE(CMultiplayerGamerTagHud::Init(INIT_SESSION);)

#if RSG_PC
	NPROFILE(CMultiplayerChat::InitSession();)
#endif

	NPROFILE(NetworkDebug::OpenNetwork())

	CContextMenuHelper::InitVotes();

	if(!sm_bFirstEntryNetworkOpen)
	{
		NPROFILE(CMiniMap::ReInit())  // restart the minimap
		NPROFILE(CStatsMgr::OpenNetwork())
		NPROFILE(CLiveManager::GetProfileStatsMgr().OpenNetwork())
		NPROFILE(NETWORK_LEGAL_RESTRICTIONS.UpdateRestrictions())	

		NPROFILE(gnetVerify(CNetworkTelemetry::FlushTelemetry(true)))

		NPROFILE(CNetwork::InitVoice())

		CPed::SetRandomPedsDropMoney( false );

		// ensure that the look at talkers flags is defaulted to TRUE
		CNetObjPlayer::SetLookAtTalkers(true);

		// turn off the golf course
		if (CMiniMap::IsInGolfMap())  // ensure golf course is turned off before MP game is initialised
		{
			CMiniMap::SetCurrentGolfMap(GOLF_COURSE_OFF);
		}

		NPROFILE(CGarages::NetworkInit())

		// reset radio history, since MP history isn't compatible with SP
		if(!sm_bSkipRadioResetNextOpen)
		{
			NPROFILE(audRadioStation::ResetHistoryForNetworkGame())
		}
		sm_bSkipRadioResetNextOpen = false;

		NPROFILE(audNorthAudioEngine::NotifyOpenNetwork())

		SBackgroundScripts::GetInstance().SetGameMode(BGScriptInfo::FLAG_MODE_MP);

#if RSG_ORBIS
		NPROFILE(rlFriendsManager::Reactivate());
#elif RSG_DURANGO
		NPROFILE(rlFriendsManager::CheckListForChanges(NetworkInterface::GetLocalGamerIndex()));
#endif
	}

    sm_bNetworkOpen = true;
    sm_bMatchStarted = false;
	sm_bGameInProgress = false;
    
	if(!sm_bFirstEntryNetworkOpen)
	{
		NPROFILE(fwClipSetManager::StartNetworkSession())
		
		NPROFILE(fwAnimDirectorComponentSyncedScene::StartNetworkSession())
        // flag the next call to StartMatch as an entry point
        sm_bFirstEntryMatchStarted = false;
	}

    sm_bFirstEntryNetworkOpen = true;
	if(sm_bSkipRadioResetNextClose)
	{
		audDisplayf("CNetwork::OpenNetwork - resetting sm_bSkipRadioResetNextClose");
		sm_bSkipRadioResetNextClose = false;
	}

	if(!gnetVerify(netInterface::IsPlayerMgrValid()))
	{
		gnetError("OpenNetwork :: Player Manager Invalid!");
		if(sm_bFatalErrorForInvalidPlayerManager)
			Quitf(0, "OpenNetwork :: Player Manager Invalid!");
	}

	if(!gnetVerify(netInterface::IsPlayerMgrInitialised()))
	{
		gnetError("OpenNetwork :: Player Manager Not Initialized!");
		if(sm_bFatalErrorForInvalidPlayerManager)
			Quitf(0, "OpenNetwork :: Player Manager Not Initialized!");
	}

	NPROFILE(GamePresenceEvents::OpenNetworking())

	NPROFILE(CGameStreamMgr::GetGameStream()->FlushQueue())

    sm_bDisableCarGeneratorTime = 0;

	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetMessageLog(), "OPEN_NETWORK", "\r\n");
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetBandwidthManager().GetLog(), "OPEN_NETWORK", "\r\n");
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "OPEN_NETWORK", "\r\n");
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OPEN_NETWORK", "\r\n");
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "OPEN_NETWORK", "\r\n");
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetArrayManager().GetLog(), "OPEN_NETWORK", "\r\n");

	static const u32 STAT_NAT_TYPE = ATSTRINGHASH("_NatType", 0xA1C09C1A);
	NPROFILE(StatsInterface::SetStatData(STAT_NAT_TYPE, netHardware::GetNatType()))

	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PopProfiler("CNetwork::OpenNetwork", !bIsFirstEntryNetworkOpen ? 5000 : 1000));

	BasePlayerCardDataManager::ClearCachedStats();

	sm_bNetworkOpening = false; 

	return (sm_bNetworkOpen);
}

void
CNetwork::EnterLobby()
{
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "ENTERING_LOBBY", "\r\n");

	CMiniMap::UpdateWaypoint();  // clear any SP waypoint but don't remove it completely
	CMessages::ClearMessages();
	CMessages::ClearAllBriefMessages();
	fwTimer::SetTimeWarpAll(1.0f);
}

void
CNetwork::StartMatch()
{
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "STARTING_MATCH", "\r\n");

	if(!gnetVerify(sm_bNetworkOpen))
	{
		gnetError("StartMatch :: Network closed!");
		if(sm_bFatalErrorForStartWhenClosed)
			Quitf(0, "StartMatch :: Network closed!");

		return;
	}

	if(!gnetVerify(sm_bFirstEntryNetworkOpen))
	{
		gnetError("StartMatch :: Network fully closed!");
		if(sm_bFatalErrorForStartWhenFullyClosed)
			Quitf(0, "StartMatch :: Network fully closed!");

		return;
	}

	if(!gnetVerify(sm_bManagersAreInitialised))
	{
		gnetError("StartMatch :: Managers not initialised!");
		if(sm_bFatalErrorForStartWhenManagersShutdown)
			Quitf(0, "StartMatch :: Managers not initialised!");

		return;
	}

	if(!gnetVerify(sm_NetworkSession && sm_NetworkSession->CanStart()))
	{
		gnetError("StartMatch :: Sesson not ready to start!");
	}

	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PushProfiler("CNetwork::StartMatch", PARAM_netFunctionProfilerAsserts.Get()));
	unsigned profilerTreshold=0;
	PARAM_netFunctionProfilerThreshold.Get(profilerTreshold);
	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().SetTimeThreshold(profilerTreshold));

	NOTFINAL_ONLY(bool bFirstEntryMatchStarted = sm_bFirstEntryMatchStarted;)
	gnetDebug1("StartMatch :: Is First Entry: %s", sm_bFirstEntryMatchStarted ? "False" : "True");

	if (gnetVerify(!sm_bMatchStarted))
	{
		gnetAssertf(CGameWorld::GetMainPlayerInfo(), "Null player info.");
		CPed* pPlayerPed = FindPlayerPed();
		gnetAssertf(pPlayerPed, "Null player ped.");

		//This needs to be reset in the player - If this flag is set cops don't scan for crooks and their wanted levels
		if (pPlayerPed && pPlayerPed->IsLocalPlayer())
		{
			pPlayerPed->SetBlockingOfNonTemporaryEvents(false);
		}

		// Remove all population if we are joining as a client. This is a quick fix which may become more sophisticated in future (we may wish to keep our
		// population if we are not near any other players, etc)

		// TODO: this needs looked into. There may be more population that is allowed in a network game.
		//if (!NetworkInterface::IsHost())
		{
//			gnetAssertf(GtaThread::CountSinglePlayerOnlyScripts() == 0, "CNetwork::StartMatch - there are still single player scripts running");
			NPROFILE(CNetwork::ClearPopulation(false))
		}

		if(!sm_bFirstEntryMatchStarted)
		{
			// Make sure we clear out any stored door state (locked open or closed)
			NPROFILE(CDoorSystem::Shutdown())
			NPROFILE(CDoorSystem::Init())
			
			NPROFILE(EXTRACONTENT.ExecuteScriptPatch())
			NPROFILE(EXTRACONTENT.ExecuteWeaponPatchMP(true))

			NPROFILE(CTexLod::FlushAllUpgrades())

			// Clean out the static bounds store so that we don't have any issues where the weapon map collision
			// has streamed out but the corresponding mover collision hasn't had a chance to take on the extra collision flag.
			if(PARAM_mapMoverOnlyInMultiplayer.Get())
			{
				NPROFILE(g_StaticBoundsStore.RemoveAll())
			}
		}

		//Register our player with the object manager
		NPROFILE(NetworkInterface::RegisterObject(pPlayerPed, 0, netObject::GLOBALFLAG_PERSISTENTOWNER|CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|netObject::GLOBALFLAG_CLONEALWAYS))

		if (!pPlayerPed->GetNetworkObject()->CanSynchronise(true))
		{
			gnetWarning("StartMatch :: **WARNING**: PLAYER STILL HAS SINGLE PLAYER MODEL. This may cause player sync issues.");
		}

		// Reset ped groups and assign one for our player (this has to be done after the player ped is registered so we can get the physical player index)
		NPROFILE(CPedGroups::Init())

		CPlayerSyncTree* pPlayerSyncTree = SafeCast(CPlayerSyncTree, pPlayerPed->GetNetworkObject()->GetSyncTree());

		// have to initialise the current state buffer after we have set up the players default group
		NPROFILE(pPlayerSyncTree->InitialiseInitialStateBuffer(pPlayerPed->GetNetworkObject(), true))

		// if the player is invisible when the network starts, force an update of the visibility state
		// The player may have no sync data if he still has the single player model
		if (!pPlayerPed->GetIsVisible())
		{
			gnetWarning("StartMatch :: Player is invisible on registration");

			if (pPlayerPed->GetNetworkObject()->HasSyncData())
			{
				NPROFILE(pPlayerSyncTree->DirtyNode(pPlayerPed->GetNetworkObject(), *pPlayerSyncTree->GetPhysicalGameStateNode()))
			}
			else
			{
				gnetWarning("StartMatch :: **WARNING**: Could not dirty visibility node");
			}
		}

		// register any existing population
		if(!sm_bFirstEntryMatchStarted)
		{
			NPROFILE(CVehicleModelInfo::LoadResidentObjects())
		}

		NPROFILE(CVehiclePopulation::NetworkRegisterAllVehicles())
		NPROFILE(CPedPopulation::NetworkRegisterAllPedsAndDummies())
		NPROFILE(CObject::NetworkRegisterAllObjects())

		NPROFILE(CPedPopulation::StartSpawningPeds())
		NPROFILE(CVehiclePopulation::StartSpawningVehs())

        static const unsigned CARGENERATOR_DISABLE_TIME = 5000;
        sm_bDisableCarGeneratorTime = CARGENERATOR_DISABLE_TIME;

		NPROFILE(CTaskNMBehaviour::StartNetworkGame())
	
		NPROFILE(CPickupManager::SessionReset());

		// register static arrays
		if(gnetVerifyf(sm_ArrayMgr, "Array manager not initialised!"))
        {
			NPROFILE(sm_ArrayMgr->RegisterAllGameArrays())
        }

		// this needs reset when we are a cheater
		if(IsCheater())
		{
			NPROFILE(CStatsMgr::ResetCRCStats())
		}

		//Remote Profile stats manager
		NPROFILE(CStatsMgr::GetStatsDataMgr().StartNetworkMatch())
		
        if(NetworkInterface::IsHost())
        {
            // Choose the model variation set for vehicles
            int modelVariationID = fwRandom::GetRandomNumberInRange(0, NUM_NETWORK_MODEL_VARIATIONS);
            gPopStreaming.SetNetworkModelVariationID(static_cast<u32>(modelVariationID));
        }

		NPROFILE(CTheScripts::GetScriptHandlerMgr().RegisterExistingThreads())

		sm_bMatchStarted = true;
        sm_bFirstEntryMatchStarted = true;

		CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();

		if (pMyPlayer && pMyPlayer->IsInRoamingBubble())
		{
			sm_bGameInProgress = true;
		}

		NetworkInterface::BroadcastCachedLocalPlayerHeadBlendData();

#if __BANK
		CEntity::ResetNetworkVisFlagCallStackData();
#endif
	}

	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PopProfiler("CNetwork::StartMatch", bFirstEntryMatchStarted ? 5000 : 1000));
}

void
CNetwork::CloseNetwork(bool bFullClose)
{
	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PushProfiler("CNetwork::CloseNetwork", PARAM_netFunctionProfilerAsserts.Get())) ;

	gnetDebug1("CloseNetwork :: Open: %s, FirstEntryOpen: %s, FullClose: %s", sm_bNetworkOpen ? "True" : "False", sm_bFirstEntryNetworkOpen ? "True" : "False", bFullClose ? "True" : "False");

	if(GetObjectManager().GetForceCrashOnShutdownWhileProcessing())
	{
		const bool SHOULD_CRASH_DEFAULT_VAL = 
#if __FINAL
			false;
#else
			true;
#endif

		if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_CRASH_ON_SHUTDOWN_WHILE_PROCESSING", 0xDCFCB64C), SHOULD_CRASH_DEFAULT_VAL))
		{
			NETWORK_QUITF(0, "CloseNetwork is called while a sync message is being processed");
		}
	}

	if(!gnetVerify(!sm_bNetworkOpening))
	{
		gnetError("CloseNetwork :: Currently opening network!");
		if(sm_bFatalErrorForCloseDuringOpen)
			Quitf(0, "CloseNetwork :: Currently opening network!");

		// we still want to close since whatever called this will expect it
		sm_bNetworkClosePending = sm_bAllowPendingCloseNetwork; 
		sm_bNetworkClosePendingIsFull = bFullClose;

		return;
	}

	sm_bNetworkClosePending = false;

	if(sm_bNetworkOpen || (sm_bFirstEntryNetworkOpen && bFullClose))
	{
		bool bWasNetworkOpen = sm_bNetworkOpen;

		if(bWasNetworkOpen)
		{
			NPROFILE_BLOCK(WriteLogEvent);
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetMessageLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetBandwidthManager().GetLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetArrayManager().GetLog(), "CLOSE_NETWORK" "%s", bFullClose ? "Full close" : "Partial close");
		}

		sm_bMatchStarted = false;
		sm_bNetworkOpen = false;
		sm_bGameInProgress = false;

		sm_bNetworkClosing = true;

		if(bFullClose)
		{
            // reset first entry flags
			sm_bFirstEntryNetworkOpen = false;
            sm_bFirstEntryMatchStarted = false;

            // reset global weather flag
            sm_bAppliedGlobalSettings = false; 
		}

		NPROFILE(CMultiplayerGamerTagHud::Shutdown(SHUTDOWN_SESSION))
		CContextMenuHelper::InitVotes();

#if RSG_PC
		NPROFILE(CMultiplayerChat::ShutdownSession();)
#endif

		if(bFullClose)
		{
			NPROFILE(CMiniMap::ReInit())  // restart the minimap

			//Remote Profile stats manager
			NPROFILE(CStatsMgr::GetStatsDataMgr().EndNetworkMatch())

			BasePlayerCardDataManager::ClearCachedStats();
		}

		if(bWasNetworkOpen)
		{
			NPROFILE(CThePopMultiplierAreas::Reset())

			NPROFILE(GamePresenceEvents::CloseNetworking())
			NPROFILE(NetworkDebug::CloseNetwork())
			NPROFILE(CNetwork::ShutdownManagers())
		
			if(!sm_NetworkSession->IsTransitioning())
			{
				NPROFILE(sm_networkClock.Stop())
			}

			NPROFILE(CMessages::ClearMessages())
			NPROFILE(CMessages::ClearAllBriefMessages())

			NPROFILE(CPauseMenu::SetCurrentMissionDescription(false, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
			NPROFILE(CMiniMap::SwitchOffWaypoint())  // remove any network created waypoint

			NPROFILE(CScriptHud::OnNetworkClosed());

			if (CMiniMap::IsInBigMap())  // turn off bigmap if it is active
			{
				NPROFILE(CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP))
			}
		}

		CPed * pPlayer = FindPlayerPed();

		if(bFullClose)
		{
			NPROFILE(CLiveManager::GetProfileStatsMgr().CloseNetwork())

			NPROFILE(CStatsMgr::CloseNetwork())

			NPROFILE(CNetwork::ShutdownVoice())

			NPROFILE(CPed::SetRandomPedsDropMoney(true))

			// Stop any pad shaking
			NPROFILE(CControlMgr::StopPadsShaking())

			NPROFILE(CTexLod::ShutdownSession(0))

			NPROFILE(EXTRACONTENT.ExecuteWeaponPatchMP(false))

			//Fix any IK issues the player is having / going to have
			if (pPlayer)
			{
				NPROFILE(pPlayer->GetIkManager().ResetAllSolvers())
				pPlayer->ClearBaseFlag(fwEntity::IS_FIXED_BY_NETWORK);
			}

			// need to clean up our cutscene manager prior to flushing vehicle models, as the cutscene will still be refing it.
			if(CutSceneManager::GetInstancePtr())
			{
				NPROFILE(CutSceneManager::GetInstance()->ShutDownCutscene())
			}

			NPROFILE(CPickupManager::SessionReset());

			NPROFILE(CTaskNMBehaviour::EndNetworkGame())

			SBackgroundScripts::GetInstance().SetGameMode(BGScriptInfo::FLAG_MODE_SP);

			CNetObjPlayer::ClearAllCachedPlayerHeadBlendData();

			CScriptedGameEvent::ClearEventHistory();
		}
		else
		{
			CNetObjPlayer::ClearAllRemoteCachedPlayerHeadBlendData();
		}

		NPROFILE(CPedPopulation::StartSpawningPeds())
		NPROFILE(CVehicleModelInfo::LoadResidentObjects())
		NPROFILE(CVehiclePopulation::StartSpawningVehs())

		NPROFILE(CVehiclePopulation::ClearInterestingVehicles())
		NPROFILE(CPedGroups::RemoveAllRandomGroups())
		NPROFILE(CPickupManager::RemoveAllPickups(true))
		NPROFILE(COrderManager::GetInstance().ClearAllOrders())
		NPROFILE(CIncidentManager::GetInstance().ClearAllIncidents())
		NPROFILE(CProjectileManager::ClearAllNetSyncProjectile())

		NPROFILE(CGameStreamMgr::GetGameStream()->FlushQueue())

		//Reset Player Wanted Level
		if (pPlayer)
		{
			NPROFILE(pPlayer->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING))
		}

		CPlayerInfo* playerInfo = CGameWorld::GetMainPlayerInfo();
		if (playerInfo)
		{
			//Reset Player Team
			playerInfo->Team = -1;

			// clear cutscene state
			if (playerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
			{
				playerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
			}
		}

		SetInMPCutscene(false);

		NPROFILE(audNorthAudioEngine::NotifyCloseNetwork(bFullClose))

		if(bFullClose)
		{
			// Make sure we clear out any stored door state (locked open or closed)
			NPROFILE(CDoorSystem::Shutdown())
			NPROFILE(CDoorSystem::Init())

			NPROFILE(fwClipSetManager::EndNetworkSession())
			NPROFILE(fwAnimDirectorComponentSyncedScene::EndNetworkSession())
			
			// force update of the synced time here to restore the single player time 
			UpdateSyncedTimeInMilliseconds();
			
			// if the clock time is overridden, we also need to update the stored value for the sync time
			// leave the other values unchanged
			if(CNetwork::IsClockTimeOverridden())
			{
				int hour, min, sec, dummy = 0;
				CNetwork::GetClockTimeStoredForOverride(hour, min, sec, dummy);
				CNetwork::HandleClockUpdateDuringOverride(hour, min, sec, static_cast<int>(GetSyncedTimeInMilliseconds()));
			}

#if !__FINAL
			// ...and reset the clock so that the update timestamp is in step with the new synced time
			CClock::ResetDebugModifiedInNetworkGame();
#endif
			CClock::SetTime(CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());

			// reset weather
			NPROFILE(g_weather.NetworkShutdown())

			// reset radio history, since MP history isn't compatible with SP
			if(!sm_bSkipRadioResetNextClose)
			{
				NPROFILE(audRadioStation::ResetHistoryForSPGame())
			}
			// Also reset the 'skip reset on next open' flag
			sm_bSkipRadioResetNextOpen = false;

			// reset weapon damage modifiers|
			NPROFILE(CWeaponInfoManager::ResetDamageModifiers())
		}

		sm_bNetworkClosing = false;
    }

#if __WIN32PC|| RSG_DURANGO
	GRCDEVICE.SetAllowBlockUpdateThread(true);
#endif

	NOTFINAL_ONLY(netFunctionProfiler::GetProfiler().PopProfiler("CNetwork::CloseNetwork", bFullClose ? 5000 : 1000));
}

void
CNetwork::InitVoice()
{
	if(!sm_NetworkVoice.IsInitialized())
	{
		gnetDebug1("InitVoice");

		sm_NetworkVoice.InitWorkerThread();

		UseNetworkHeap(NET_MEM_VOICE);
		sm_NetworkVoice.Init();
		StopUsingNetworkHeap();
	}
}

void
CNetwork::InitManagers()
{
    if (gnetVerify(!sm_bManagersAreInitialised))
    {
		Displayf("InitManagers(): Network Heap used %" SIZETFMT "d, available %" SIZETFMT "d, size %" SIZETFMT "d", CNetwork::GetNetworkHeap()->GetMemoryUsed(), CNetwork::GetNetworkHeap()->GetMemoryAvailable(), CNetwork::GetNetworkHeap()->GetHeapSize());

        // usually 2721680 on first init, then often 2911696 on subsequent inits.
        // consistently 2721680 at the end of ShutdownManagers
        u32 heapUsage = (u32)(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());
        gnetDebug1("CNetwork::InitManagers BEGIN --> heap usage: %u", heapUsage);

#if __ASSERT
        const u32 HEAP_USAGE_THRESHOLD_ERROR = 3500000;
        gnetAssertf(heapUsage <= HEAP_USAGE_THRESHOLD_ERROR, "Detecting suspicious heapUsage (%u) on InitManagers", heapUsage);
#endif //__ASSERT

		netInterface::Init(sm_CxnMgr, sm_PlayerMgr, sm_networkClock, *s_MessageLog, s_QueryFunctions);

		{
			NPROFILE_BLOCK(CNetworkBandwidthManager::Init);
			UseNetworkHeap(NET_MEM_BANDWIDTH_MANAGER);
			sm_BandwidthMgr.Init(&sm_CxnMgr);
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(CNetworkPlayerMgr::Init);
			netPlayerMgrBase::PhysicalOnRemoteCallback cb;
			cb.Bind(&CNetworkObjectMgr::IsLocalPlayerPhysicalOnRemoteMachine);
			sm_PlayerMgr.Init(&sm_CxnMgr, NETWORK_GAME_CHANNEL_ID, cb);
		}

		{
			NPROFILE_BLOCK(CNetworkObjectMgr::Init);
			UseNetworkHeap(NET_MEM_OBJECT_MANAGER);
			sm_ObjectMgr = rage_new CNetworkObjectMgr(sm_BandwidthMgr);
			sm_ObjectMgr->Init();
			netInterface::SetObjectManager(*sm_ObjectMgr);
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(netEventMgr::Init);
			UseNetworkHeap(NET_MEM_EVENT_MANAGER);
			sm_EventMgr = rage_new netEventMgr(sm_BandwidthMgr);
			netInterface::SetEventManager(*sm_EventMgr);
			sm_EventMgr->Init();
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(CGameArrayMgr::Init);
			UseNetworkHeap(NET_MEM_ARRAY_MANAGER);
			sm_ArrayMgr = rage_new CGameArrayMgr(sm_BandwidthMgr);
			sm_ArrayMgr->Init();
			netInterface::SetArrayManager(*sm_ArrayMgr);
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(CRoamingBubbleMgr::Init);
			UseNetworkHeap(NET_MEM_ROAMING_BUBBLE_MANAGER);
			sm_RoamingBubbleMgr.Init();
			StopUsingNetworkHeap();
		}

#if __WIN32PC
		NPROFILE(sm_NetworkTextChat.Init(&sm_CxnMgr))
#endif

		NPROFILE(CTheScripts::GetScriptHandlerMgr().NetworkInit())

        NPROFILE(CNetworkStreamingRequestManager::Init())

        NPROFILE(NetworkEventTypes::RegisterNetworkEvents())
        
        NPROFILE(UIWorldIconManager::Open())

		sm_bManagersAreInitialised = true;

		if (NetworkInterface::GetVoice().IsInitialized())
		{
			NetworkInterface::GetVoice().SetTeamOnlyChat(false);
		}

		NPROFILE(NetworkHasherUtil::InitHasherThread())

        heapUsage = (u32)(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());
        gnetDebug1("CNetwork::InitManagers END --> heap usage: %u", heapUsage);
	}
}

void CNetwork::ShutdownVoice(bool bForce)
{
	if(bForce || !(sm_NetworkVoiceSession.IsInVoiceSession() || 
				   sm_NetworkSession->IsSessionActive() || 
				   sm_NetworkSession->IsTransitionActive() ||
				   sm_NetworkParty->IsInParty()))
	{
		gnetDebug1("ShutdownVoice :: Force: %s", bForce ? "True" : "False");

		sm_NetworkVoice.ShutdownWorkerThread();

		UseNetworkHeap(NET_MEM_VOICE);
		sm_NetworkVoice.Shutdown();
		StopUsingNetworkHeap();
	}
}

void CNetwork::ShutdownManagers()
{
    if (gnetVerify(sm_bManagersAreInitialised))
    {
        u32 heapUsage = (u32)(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());
        gnetDebug1("CNetwork::ShutdownManagers BEGIN --> heap usage: %u", heapUsage);

        netInterface::SetShuttingDown();

		NPROFILE(NetworkHasherUtil::ShutdownHasherThread())

    	NPROFILE(UIWorldIconManager::Close())
    
        NPROFILE(CNetworkStreamingRequestManager::Shutdown())

		// it would be better to pass an event to the scripts code here
		NPROFILE(CTheScripts::GetScriptHandlerMgr().NetworkShutdown())

		{
			NPROFILE_BLOCK(CRoamingBubbleMgr::Shutdown);
			UseNetworkHeap(NET_MEM_ROAMING_BUBBLE_MANAGER);
			sm_RoamingBubbleMgr.Shutdown();
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(netBandwidthMgr::Shutdown);
			UseNetworkHeap(NET_MEM_BANDWIDTH_MANAGER);
			sm_BandwidthMgr.Shutdown();
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(CGameArrayMgr::Shutdown);
			UseNetworkHeap(NET_MEM_ARRAY_MANAGER);
			sm_ArrayMgr->Shutdown();
			delete sm_ArrayMgr;
			sm_ArrayMgr = 0;
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(netEventMgr::Shutdown);
			UseNetworkHeap(NET_MEM_EVENT_MANAGER);
			sm_EventMgr->Shutdown();
			delete sm_EventMgr;
			sm_EventMgr = 0;
			StopUsingNetworkHeap();
		}

		{
			NPROFILE_BLOCK(CNetworkObjectMgr::Shutdown);
			UseNetworkHeap(NET_MEM_OBJECT_MANAGER);
			sm_ObjectMgr->Shutdown();
			delete sm_ObjectMgr;
			sm_ObjectMgr = 0;
			StopUsingNetworkHeap();
		}

#if __WIN32PC
		NPROFILE(sm_NetworkTextChat.Shutdown())
#endif

		if (sm_NetworkSession)
		{
			sm_NetworkSession->GetSnSession().CancelDisplayNameTasks();
		}

        NPROFILE(sm_PlayerMgr.Shutdown())

        heapUsage = (u32)(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());
        gnetDebug1("CNetwork::ShutdownManagers END --> heap usage: %u", heapUsage);

		NPROFILE(netInterface::Shutdown())

        sm_bManagersAreInitialised = false;
    }
}

void CNetwork::ClearPopulation(bool bIncludeMissionEntities)
{
    CVehiclePopulation::ClearInterestingVehicles();
	CObjectPopulation::DeleteAllTempObjects();

	if (bIncludeMissionEntities)
	{
		// remove all script & ambient entities
		CPedPopulation::RemoveAllPeds(CPedPopulation::PMR_ForceAllRandomAndMission);
		CObjectPopulation::DeleteAllMissionObjects();
		CVehiclePopulation::RemoveAllVehs(false);
	}
	else
	{
		// just remove all ambient entities
		CPedPopulation::RemoveAllRandomPeds();
		CVehiclePopulation::RemoveAllVehs(true);
	}

	CTrain::RemoveAllTrains();
	CPickupManager::RemoveAllPickups(bIncludeMissionEntities);
	COrderManager::GetInstance().ClearAllOrders();
	CIncidentManager::GetInstance().ClearAllIncidents();
	CProjectileManager::ClearAllNetSyncProjectile();

    CPed* pPlayerPed = FindPlayerPed();

    if (gnetVerify(pPlayerPed) && gnetVerify(pPlayerPed->GetPlayerWanted()))
	{
		pPlayerPed->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING);
	}
}

void CNetwork::OverrideClockTime(int hour, int minute, int second, bool bFromScript)
{
    gClockOverride.Override(hour, minute, second, bFromScript);
}

void CNetwork::OverrideClockRate(u32 msPerGameMinute)
{
	gClockOverride.OverrideRate(msPerGameMinute);
}

void CNetwork::ClearClockTimeOverride()
{
    gClockOverride.StopOverriding();
}

bool CNetwork::IsClockTimeOverridden()
{
    return gClockOverride.IsActive();
}

bool CNetwork::IsClockOverrideFromScript()
{
	return gClockOverride.IsFromScript();
}

bool CNetwork::IsClockOverrideLocked()
{
	return gClockOverride.IsOverrideLocked();
}

void CNetwork::SetClockOverrideLocked(bool bLocked)
{
	gClockOverride.SetOverrideLocked(bLocked);
}

void CNetwork::HandleClockUpdateDuringOverride(int hour, int minute, int second, int lastMinAdded)
{
    gClockOverride.SetStoredSessionTime(hour, minute, second, lastMinAdded);
}

void CNetwork::GetClockTimeStoredForOverride(int &hour, int &minute, int &second, int &lastMinAdded)
{
    gClockOverride.GetStoredSessionTime(hour, minute, second, lastMinAdded);
}

void CNetwork::SetNetworkGameMode(u16 nGameMode) 
{
    if(sm_nGameMode != nGameMode)
    {
		// Reset current game mode state when switching game mode
		SetNetworkGameModeState(GameModeState_None);

        gnetDebug1("SetNetworkGameMode: %s (%u) -> %s (%u)", NetworkUtils::GetMultiplayerGameModeAsString(sm_nGameMode), sm_nGameMode, NetworkUtils::GetMultiplayerGameModeAsString(nGameMode), nGameMode);
        //@@: location CNETWORK_SETNETWORKGAMEMODE
        sm_nGameMode = nGameMode;
    }
}

void CNetwork::SetNetworkGameModeState(int nGameModeState)
{
	if(sm_nGameModeState != nGameModeState)
	{
		gnetDebug1("SetNetworkGameModeState: %s (%u) -> %s (%u)", NetworkUtils::GetMultiplayerGameModeStateAsString(sm_nGameModeState), sm_nGameModeState, NetworkUtils::GetMultiplayerGameModeStateAsString(nGameModeState), nGameModeState);
		sm_nGameModeState = nGameModeState;
	}
}

bool CNetwork::IsNetworkTutorialSessionTurn()
{
    if(!gnetVerifyf(sm_IsInTutorialSession, "Calling IsNetworkTutorialSessionTurn() when not in a tutorial session!"))
    {
        return false;
    }

    if(s_TutorialSessionTurnDelay > 0)
    {
        return false;
    }

    // players within range are given turns to create vehicles, highest peer ID first.
    // the game waits for a short period between changing turns, to help prevent
    // cars being created on top of one another
    const unsigned numTurns        = s_NumPlayersInTutorialSession;
    const unsigned localPlayerTurn = s_TutorialSessionNetworkTurn;
    
    const unsigned DURATION_OF_TURN       = 400;
    const unsigned DURATION_BETWEEN_TURNS = 200;

    unsigned currentTime    = CNetwork::GetNetworkTime();
    unsigned timeWithinTurn = currentTime % (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);
    currentTime /= (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);

    unsigned currentTurn = currentTime % (numTurns);

    if((currentTurn == localPlayerTurn) && (timeWithinTurn <= DURATION_OF_TURN))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool CNetwork::IsConcealedPlayerTurn()
{
	if(!gnetVerifyf(sm_ThereAreConcealedPlayers, "Calling IsNetworkTutorialSessionTurn() when not in a tutorial session!"))
	{
		return false;
	}

	if(s_ConcealedPlayerTurnDelay > 0)
	{
		return false;
	}

	// players within range are given turns to create vehicles, highest peer ID first.
	// the game waits for a short period between changing turns, to help prevent
	// cars being created on top of one another
	const unsigned numTurns        = s_NumPlayersNotConcealedToMe;
	const unsigned localPlayerTurn = s_ConcealedPlayerNetworkTurn;

	const unsigned DURATION_OF_TURN       = 400;
	const unsigned DURATION_BETWEEN_TURNS = 200;

	unsigned currentTime    = CNetwork::GetNetworkTime();
	unsigned timeWithinTurn = currentTime % (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);
	currentTime /= (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);

	unsigned currentTurn = currentTime % (numTurns);

	if((currentTurn == localPlayerTurn) && (timeWithinTurn <= DURATION_OF_TURN))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CNetwork::UpdateConcealedPlayerCachedVariables()
{
	s_NumPlayersNotConcealedToMe = 0;
	s_ConcealedPlayerNetworkTurn  = 0;

	if (NetworkInterface::AreThereAnyConcealedPlayers())
	{
		// if we have entered a tutorial session this frame we need to wait a while
		// before allow us to think it is our turn - this gives time for remote players
		// to be informed about of change in session state and avoid turn conflicts
		if(!sm_ThereAreConcealedPlayers)
		{
			const unsigned TUTORIAL_SESSION_TURN_DELAY = 1000;
			s_ConcealedPlayerTurnDelay = TUTORIAL_SESSION_TURN_DELAY;
		}
		else if(s_ConcealedPlayerTurnDelay > 0)
		{
			s_ConcealedPlayerTurnDelay -= MIN(s_ConcealedPlayerTurnDelay, fwTimer::GetSystemTimeStepInMilliseconds());
		}

		sm_ThereAreConcealedPlayers = true;

		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
			const CPed* pPlayerPed = player ? player->GetPlayerPed() : NULL;
			const CNetObjPhysical* pNetObjPhysical = pPlayerPed ? static_cast<const CNetObjPhysical*>(pPlayerPed->GetNetworkObject()) : NULL;

			if(pNetObjPhysical && !pNetObjPhysical->IsConcealed())
			{
				if (NetworkInterface::GetLocalPlayer()->GetRlGamerId() < player->GetRlGamerId())
				{
					s_ConcealedPlayerNetworkTurn++;
				}

				s_NumPlayersNotConcealedToMe++;
			}
		}

		// include the local player
		s_NumPlayersNotConcealedToMe++;

		// check if it is our turn between all non concealed players
		if(IsConcealedPlayerTurn())
		{
			s_WasConcealedPlayerNetworkTurn = true;
		}
		else if(s_WasConcealedPlayerNetworkTurn)
		{
			CTheCarGenerators::ClearScheduledQueue();
			s_WasConcealedPlayerNetworkTurn = false;
		}
	}
	else
	{
		sm_ThereAreConcealedPlayers          = false;
		s_WasConcealedPlayerNetworkTurn = false;
	}
}

void CNetwork::UpdateTutorialSessionCachedVariables()
{
	CNetObjPlayer *netObjPlayer = FindPlayerPed() ? static_cast<CNetObjPlayer *>(FindPlayerPed()->GetNetworkObject()) : 0;

    s_NumPlayersInTutorialSession = 0;
    s_TutorialSessionNetworkTurn  = 0;

	if(netObjPlayer)
	{
		netObjPlayer->UpdatePendingTutorialSession();
	}

    if(netObjPlayer && netObjPlayer->IsInTutorialSession())
    {
        // if we have entered a tutorial session this frame we need to wait a while
        // before allow us to think it is our turn - this gives time for remote players
        // to be informed about of change in session state and avoid turn conflicts
        if(!sm_IsInTutorialSession)
        {
            const unsigned TUTORIAL_SESSION_TURN_DELAY = 1000;
            s_TutorialSessionTurnDelay = TUTORIAL_SESSION_TURN_DELAY;
        }
        else if(s_TutorialSessionTurnDelay > 0)
        {
            s_TutorialSessionTurnDelay -= MIN(s_TutorialSessionTurnDelay, fwTimer::GetSystemTimeStepInMilliseconds());
        }

        sm_IsInTutorialSession = true;

        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(!player->IsInDifferentTutorialSession())
            {
                if (NetworkInterface::GetLocalPlayer()->GetRlGamerId() < player->GetRlGamerId())
                {
                    s_TutorialSessionNetworkTurn++;
                }

                s_NumPlayersInTutorialSession++;
            }
        }

        // include the local player
        s_NumPlayersInTutorialSession++;

        // check if it is our tutorial session turn
        if(IsNetworkTutorialSessionTurn())
        {
            s_WasTutorialSessionNetworkTurn = true;
        }
        else if(s_WasTutorialSessionNetworkTurn)
        {
            CTheCarGenerators::ClearScheduledQueue();
            s_WasTutorialSessionNetworkTurn = false;
        }
    }
    else
    {
        sm_IsInTutorialSession          = false;
        s_WasTutorialSessionNetworkTurn = false;
    }
}

void CNetwork::UpdateSyncedTimeInMilliseconds()
{
    // update the synced timer - available for use in single player and network games
    if(NetworkInterface::IsNetworkOpen() && (NetworkInterface::IsHost() || NetworkInterface::NetworkClockHasSynced()))
    {
        sm_syncedTimeInMilliseconds = NetworkInterface::GetNetworkTime();
    }
    else
    {
        sm_syncedTimeInMilliseconds = fwTimer::GetTimeInMilliseconds();
    }
}

void
CNetwork::OnNetEvent(netConnectionManager* /*cxnMgr*/, const netEvent* evt)
{
    const unsigned eid = evt->GetId();

	// log out all connection events here
	netCxnEventDebug("OnNetEvent :: %s[%u] - %d::%08x - %u%s [%s]", evt->GetAutoIdNameFromId(eid), 
																	eid, 
																	evt->m_ChannelId, 
																	evt->m_CxnId, 
																	(NET_EVENT_FRAME_RECEIVED == eid) ? evt->m_FrameReceived->m_Sequence : 0, 
																	(NET_EVENT_FRAME_RECEIVED == eid) ? (((evt->m_FrameReceived->m_Flags & NET_SEND_RELIABLE) != 0) ? "R" : "U") : "",
																	(NET_EVENT_FRAME_RECEIVED == eid) ? (netMessage::IsMessage(evt->m_FrameReceived->m_Payload, evt->m_FrameReceived->m_SizeofPayload) ? netMessage::GetName(evt->m_FrameReceived->m_Payload, evt->m_FrameReceived->m_SizeofPayload) : "") : "");

    if(NET_EVENT_OUT_OF_MEMORY == eid)
    {
        const netEventOutOfMemory* pOutOfMemoryEvent = evt->m_OutOfMemory;

        gnetDebug1("CxnEvent :: OutOfMemory - [0x%08x][%s], Fatal: %s", evt->m_CxnId, NetworkUtils::GetChannelAsString(evt->m_ChannelId), pOutOfMemoryEvent->m_isFatal ? "True" : "False");

        // check if this is fatal
        if(pOutOfMemoryEvent->m_isFatal)
        {
			gnetError("CxnEvent :: Fatal Out of Memory Error - Size: %u, Used: %d, Available: %d, Largest Block: %d", 
					  static_cast<int>(s_SCxnMgrAllocator->GetHeapSize()),		
					  static_cast<int>(s_SCxnMgrAllocator->GetMemoryUsed(-1)),
					  static_cast<int>(s_SCxnMgrAllocator->GetMemoryAvailable()),
					  static_cast<int>(s_SCxnMgrAllocator->GetLargestAvailableBlock()));

			// dump all connection manager queues
			sm_CxnMgr.DumpStats();

#if !__FINAL
			// dump out allocator memory usage
			s_SCxnMgrAllocator->PrintMemoryUsage();
#endif
			
            // work out what to do
            switch(evt->m_ChannelId)
            {
            case NETWORK_VOICE_CHANNEL_ID:
                // don't care
                break;

            case NETWORK_SESSION_VOICE_CHANNEL_ID:
                // just kill the voice session
				if(sm_NetworkVoiceSession.IsInVoiceSession())
				{
					gnetWarning("CxnEvent :: Bailing from voice session. Out of Memory.");
					sm_NetworkVoiceSession.Bail(true);
				}
                break;

            case NETWORK_GAME_CHANNEL_ID:
            case NETWORK_SESSION_ACTIVITY_CHANNEL_ID:
            case NETWORK_SESSION_GAME_CHANNEL_ID:
                {
            // all multiplayer critical - abort, abort
			if(NetworkInterface::IsAnySessionActive() || IsNetworkOpen())
			{
				gnetWarning("CxnEvent :: Bailing from multiplayer. Out of memory");
						CNetwork::Bail(sBailParameters(BAIL_NETWORK_ERROR, BAIL_CTX_NETWORK_ERROR_OUT_OF_MEMORY, evt->m_ChannelId));
					}
                }
				break;

            default:
                gnetAssertf(0, "CxnEvent :: Unknown channel ID (%d)", evt->m_ChannelId);
				break;
			}
        }
	}
}

#if !__NO_OUTPUT

void CNetwork::OnRequestTrackerEvent(const char* sRuleName, 
									 const char* sRuleMatch, 
									 const unsigned nTokenLimit, 
									 const float nTokenRate)
{
	netDebug("HttpRequestTracker: Burst detected - Rule Name: %s, Match: %s, Token Limit: %u, Token Rate: %f",
		sRuleName, sRuleMatch, nTokenLimit, nTokenRate);

#if __BANK
	if (!PARAM_netHttpRequestTrackerDisplayOnScreenMessage.Get())
	{
		return;
	}

	// Display a message on the screen.
	char message[256];
	formatf(message, "HttpRequestTracker: Burst detected - Rule Name: %s, Rule Match: %s", 
		sRuleName, sRuleMatch);
	NetworkDebug::DisplayHttpReqTrackerMessage(message);
#endif
}

// Indicates that diagChannel has recently opened a new log file.
static bool s_LogIsYoung = false;
static sysCriticalSectionToken s_LogIsYoungToken;

// Called via a callback when a new log file is opened (not including the first one).
static void NewLogFileCallback()
{
	// Can't actually do any logging in here because we're inside the logging thread.
	SYS_CS_SYNC(s_LogIsYoungToken);
	s_LogIsYoung = true;
}

// Write out information that it's useful to have at the top of log files. When the testers
// have been running the game for a long time it's unhelpful to have to dig backwards through
// several log files to gain some context on what might have happened.
static void WriteSummaryIfLogFileIsNew()
{
	{
		// Acquire lock on the NeetToWrite boolean.
		SYS_CS_SYNC(s_LogIsYoungToken);

		// Do nothing if the callback hasn't fired.
		if (!s_LogIsYoung) 
		{
			return;
		}

		s_LogIsYoung = false;
	}

	gnetDebug1("%s :: Log file is fresh. Writing summary...", __FUNCTION__);

	CNetwork::LogSummary();
}

// Make subchannel for summary so it's easy to find.
RAGE_DEFINE_SUBCHANNEL(net, summary, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)

#define netSummaryDebug(fmt, ...)	RAGE_DEBUGF1(net_summary, fmt, ##__VA_ARGS__)

// Write a quick summary of the current situation to the log.
void CNetwork::LogSummary()
{
	netSummaryDebug("%s", __FUNCTION__);

	if (sm_NetworkSession)
	{
		netSummaryDebug("Session Details: Host: %s, Established: %s", 
			GetNetworkSession().IsHost() ? "Local Player" : GetNetworkSession().GetHostPlayer() ? GetNetworkSession().GetHostPlayer()->GetLogName() : "Unknown",
			GetNetworkSession().IsSessionEstablished() ? "True" : "False");
	}

	if (GetPlayerMgr().IsInitialized()) 
	{
		netSummaryDebug("Players: Physical: %u, Pending: %u, Active: %u", 
			GetPlayerMgr().GetNumPhysicalPlayers(), 
			GetPlayerMgr().GetNumPendingPlayers(), 
			GetPlayerMgr().GetNumActivePlayers());

		{
			const CNetGamePlayer* localPlayer = GetPlayerMgr().GetMyPlayer();

			if (localPlayer)
			{
				netSummaryDebug("Local Player Name: %s", localPlayer->GetLogName());
			}
			else
			{
				netSummaryDebug("Null local player.");
			}

		}
	}
	else
	{
		netSummaryDebug("Player Manager not initialized.");
	}
}

#endif // !__NO_OUTPUT
