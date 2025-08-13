#if RSG_PC
#include <tchar.h>
#endif

// Rage headers
#include "script/wrapper.h"
#include "script/array.h"
#include "rline/rl.h"
#include "rline/rlnp.h"
#include "rline/rlfriend.h"
#include "rline/rlprivileges.h"
#include "rline/rlsystemui.h"
#include "rline/scmembership/rlscmembership.h"
#include "string/stringutil.h"
#include "system/service.h"
#include "net/nethardware.h"
#include "net/tunneler_ice.h"

// Framework headers
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwnet/netarrayhandler.h"
#include "fwnet/neteventmgr.h"
#include "fwnet/netscgamerdatamgr.h"
#include "fwscript/scriptinterface.h"

// Game headers
#include "script/commands_network.h"
#include "control/gamelogic.h"
#include "control/restart.h"
#include "Core/Game.h"
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "Objects/Door.h"
#include "peds/Ped.h"
#include "pedgroup/pedgroup.h"
#include "peds/pedplacement.h"
#include "peds/rendering/PedOverlay.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupManager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "scene/DownloadableTextureManager.h"
#include "scene/ExtraContent.h"
#include "scene/portals/Portal.h"
#include "scene/world/gameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/Store/StoreTextureManager.h"
#include "frontend/WarningScreen.h"
#include "frontend/PauseMenu.h"
#include "frontend/CFriendsMenu.h"
#include "vehicles/vehicle.h"
#include "vehicles/heli.h"
#include "Script/script_channel.h"
#include "script/script_hud.h"
#include "task/Animation/TaskCutScene.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskParachute.h"
#include "peds/PedIntelligence.h"
#include "stats/StatsTypes.h"
#include "stats/StatsMgr.h"
#include "stats/StatsDataMgr.h"
#include "stats/StatsInterface.h"
#include "text/text.h"

// Network Headers
#include "Network/NetworkTypes.h"
#include "Network/Network.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Cloud/CloudManager.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Cloud/UserContentManager.h"
#include "Network/Crews/NetworkCrewMetadata.h"
#include "Network/Debug/NetworkSoakTests.h"
#include "Network/Events/NetworkEventTypes.h"
#include "network/General/NetworkDamageTracker.h"
#include "network/general/NetworkFakePlayerNames.h"
#include "Network/General/NetworkHasherUtil.h"
#include "Network/General/NetworkUtil.h"
#include "Network/General/NetworkScratchpad.h"
#include "Network/General/NetworkSynchronisedScene.h"
#include "network/general/scriptworldstate/worldstates/NetworkRopeWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkEntityAreaWorldState.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkRemoteCheaterDetector.h"
#include "Network/Live/NetworkRichPresence.h"
#include "Network/Live/NetworkSCPresenceUtil.h"
#include "Network/Live/NetworkMetrics.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/Entities/NetObjAutomobile.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Objects/Entities/NetObjSubmarine.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderVehicle.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/NetworkInterface.h"
#include "network/Debug/NetworkBot.h"
#include "Network/Live/NetworkClan.h"
#include "network/xlast/presenceutil.h"

#include <time.h>

#if RSG_PC
#include "frontend/MultiplayerChat.h"
#endif // RSG_PC

#if __DEV
#include "Script/commands_debug.h"
#include "debug/DebugScene.h"
#endif // __DEV

NETWORK_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#if RSG_ORBIS
PARAM(LiveAreaContentType, "[LiveArea] The type of the content to be loaded.");
#endif // RSG_ORBIS

PARAM(scriptCodeDev, "[network] Disable certain game features for code development");
PARAM(preventScriptMigration, "[network] Prevent the scripts from forcing script entities to only migrate to script participants");
PARAM(netAutoMultiplayerLaunch, "[network] If present the game will automatically launch into the multiplayer game-mode.");
PARAM(netClockAlwaysBlend, "[network] If present the game will always blend back to global from an override.");
PARAM(netDisableMissionCreators, "[network] Disable access to the mission creators");
PARAM(sc_IvePlayedAllMissions, "[network] Fakes the user having a record for UGC");
PARAM(netScriptHandleNewGameFlow, "[network] Script pick up the new game flow when delayed / postponed");
XPARAM(netStartPos);
XPARAM(netTeam);
#if RSG_ORBIS
NOSTRIP_XPARAM(netFromLiveArea);
#endif

//Read profile stats for script in multiplayer.
CProfileStatsReadScriptRequest   s_ScriptProfileStatsReader;

class ScriptClanData
{
public:
	scrValue        m_GamerHandle[13];
	scrValue        m_ClanId;
	scrTextLabel31  m_ClanName;
	scrTextLabel7   m_ClanTag;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(ScriptClanData);


//Leaderboard Clan Data
CGamersPrimaryClanData  s_leaderboardClanData;

//Script version of a rlClanDesc
class scrClanDesc
{
public:
	scrValue m_clanId;
	scrTextLabel63 m_ClanName;
	scrTextLabel7 m_ClanTag;
	scrValue m_memberCount;
	scrValue m_bSystemClan;
	scrValue m_bOpenClan;

	scrTextLabel31 m_RankName;
	scrValue m_rankOrder;

	scrValue m_CreatedTimePosix;

	scrValue m_ClanColor_Red;
	scrValue m_ClanColor_Green;
	scrValue m_ClanColor_Blue;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrClanDesc);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel7);

class scrCrewRecvdInviteData
{
public:
	scrClanDesc clanDesc;
	scrValue gamerTag;
	scrValue msgString;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrCrewRecvdInviteData);

//Shitting faux protection to handle unity vs. non-unity 
#ifndef __SCR_TEXT_LABEL_DEFINED
#define __SCR_TEXT_LABEL_DEFINED
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel63);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel31);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel15);
#endif

#if RSG_ORBIS
class scrSceNetStatisticsInfo
{
public:
	scrValue m_KernelMemFreeSize;
	scrValue m_KernelMemFreeMin;
	scrValue m_PacketCount;
	scrValue m_PacketQosCount;
	scrValue m_LibNetFreeSize;
	scrValue m_LibNetFreeMin;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSceNetStatisticsInfo);

class scrSceNetSignallingInfo
{
public:
	scrValue m_TotalMemSize;
	scrValue m_CurrentMemUsage;
	scrValue m_MaxMemUsage;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSceNetSignallingInfo);
#endif

// UDS Activities
const int MAX_SECONDARY_ACTORS = 8;
class scrActivitySecondaryActors
{
public:
	scrValue  m_SecondaryActors[MAX_SECONDARY_ACTORS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrActivitySecondaryActors);

const int MAX_ACTIVITY_TASKS = 32;
class scrActivityTaskStatus
{
public:
	scrValue m_InProgressTasks[MAX_ACTIVITY_TASKS];
	scrValue m_CompletedTasks[MAX_ACTIVITY_TASKS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrActivityTaskStatus);

// Unsure of limit for availability update, using 64 here as a reasonable limit for script purposes, can be changed in future if necessary
const int MAX_AVAILABILITY_ACTIVITIES = 64 + 1; // Add buffer entry to compensate for array length entry added by script
class scrActivityAvailabilityData
{
public:
	scrValue m_AvailableActivities[MAX_AVAILABILITY_ACTIVITIES];
	scrValue m_UnavailableActivities[MAX_AVAILABILITY_ACTIVITIES];
	scrValue m_UpdateMode;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrActivityAvailabilityData);

const int MAX_PRIORITISED_ACTIVITIES = 128 + 1; // Add buffer entry to compensate for array length entry added by script
class scrActivityPriorityChangeData
{
public:
	scrValue m_ActivityIdStrings[MAX_PRIORITISED_ACTIVITIES];
	scrValue m_ActivityPriorities[MAX_PRIORITISED_ACTIVITIES];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrActivityPriorityChangeData);

namespace network_commands
{

RAGE_DEFINE_SUBCHANNEL(script, network, DIAG_SEVERITY_DEBUG3)
#undef __script_channel
#define __script_channel script_network

enum eScriptLogDisplayCondition
{
    SCRIPTLOG_ALWAYS,
    SCRIPTLOG_ISNETWORKSESSION,
    SCRIPTLOG_NETWORKHAVESUMMONS,
    SCRIPTLOG_ISNETWORKGAMERUNNING,
    SCRIPTLOG_ISNETWORKPLAYERPHYSICAL,
    SCRIPTLOG_ISNETWORKPLAYERPARTICIPANT,
    SCRIPTLOG_PLAYERWANTSTOJOINNETWORKGAME
};

enum eUserGeneratedContentType
{
	UGC_TYPE_GTA5_MISSION = 0,
	UGC_TYPE_GTA5_MISSION_PLAYLIST,
	UGC_TYPE_GTA5_LIFE_INVADER_POST,
	UGC_TYPE_GTA5_PHOTO,
	UGC_TYPE_GTA5_CHALLENGE
};

enum eReservationType
{
	RT_ALL,
	RT_LOCAL,
	RT_GLOBAL
};

#if __DEV
static const unsigned int RESURRECT_LOCALPLAYER_DIST = 5;
static const unsigned int RESURRECT_LOCALPLAYER_TIME = 1*5*1000;
static const unsigned int LEAVE_PED_BEHIND_TIME      = 1*2*1000;
#endif // __DEV

#if __DEV && __BANK && !__PPU
void WriteToScriptLogFile(eScriptLogDisplayCondition eDisplayCondition, const char *logText, ...)
{
    if (!CScriptDebug::GetWriteNetworkCommandsToScriptLog() && !CScriptDebug::GetDisplayNetworkCommandsInConsole())
    {
        return;
    }

    switch (eDisplayCondition)
    {
        case SCRIPTLOG_ALWAYS :
            break;
        case SCRIPTLOG_ISNETWORKSESSION :
            if (!CScriptDebug::GetWriteIS_NETWORK_SESSIONCommand())
            {
                return;
            }
            break;
        case SCRIPTLOG_NETWORKHAVESUMMONS :
            if (!CScriptDebug::GetWriteNETWORK_HAVE_SUMMONSCommand())
            {
                return;
            }
            break;
        case SCRIPTLOG_ISNETWORKGAMERUNNING :
            if (!CScriptDebug::GetWriteIS_NETWORK_GAME_RUNNINGCommand())
            {
                return;
            }
            break;

        case SCRIPTLOG_ISNETWORKPLAYERPHYSICAL:
            if (!CScriptDebug::GetWriteIS_NETWORK_PLAYER_ACTIVECommand())
            {
                return;
            }
            break;

        case SCRIPTLOG_ISNETWORKPLAYERPARTICIPANT:
            if (!CScriptDebug::GetWriteIS_NETWORK_PLAYER_ACTIVECommand())
            {
                return;
            }
            break;

        case SCRIPTLOG_PLAYERWANTSTOJOINNETWORKGAME :
            if (!CScriptDebug::GetWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand())
            {
                return;
            }
            break;
    }

    static char BufferForNetworkLogOutput[netLog::MAX_LOG_STRING];
    va_list args;
    va_start(args,logText);
    vsprintf(BufferForNetworkLogOutput,logText,args);
    va_end(args);

    if (CScriptDebug::GetDisplayNetworkCommandsInConsole())
    {
        if (CTheScripts::GetCurrentGtaScriptThread())
        {
            Displayf("%s %s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter(), BufferForNetworkLogOutput);
        }
        else
        {
            Displayf("%s\n", BufferForNetworkLogOutput);
        }
    }
}
#else
#define WriteToScriptLogFile(...) do {} while (0)
#endif

static CGameScriptHandlerNetComponent* GetNetComponent_(
#if __ASSERT
	const char* commandName
#endif
	)
{
	CGameScriptHandlerNetComponent* pNetComponent = CTheScripts::GetCurrentGtaScriptThread()->m_NetComponent;

#if  __ASSERT
	if (!pNetComponent)
	{
		if (!CTheScripts::GetCurrentGtaScriptHandlerNetwork())
		{
			scriptAssertf(0, "%s: %s - This script has not been set as a network script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		else if (!NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(0,  "%s: %s - A network game is not in progress.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		else
		{
			scriptAssertf(0,  "%s: %s - This script is not in a NETSCRIPT_PLAYING state (NETWORK_GET_SCRIPT_STATUS is not returning a playing state)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
	}
#endif

	return pNetComponent;
}

#if __ASSERT
#define GetNetComponent(cn) GetNetComponent_(cn)
#else
#define GetNetComponent(cn) GetNetComponent_()
#endif

bool CommandNetworkAutoMultiplayerLaunch()
{
	return PARAM_netAutoMultiplayerLaunch.Get();
}

const char* CommandGetOnlineVersion()
{
	return CDebug::GetOnlineVersionNumber();
}

bool CommandNetworkIsSignedIn()
{
	return NetworkInterface::IsLocalPlayerSignedIn();
}

bool CommandNetworkIsOnline()
{
	return NetworkInterface::IsLocalPlayerOnline();
}

bool CommandNetworkIsGuest()
{
	return NetworkInterface::IsLocalPlayerGuest();
}

bool CommandNetworkIsScOfflineMode()
{
#if RSG_PC
	return g_rlPc.GetUiInterface()->IsReadyToAcceptCommands() && g_rlPc.GetUiInterface()->IsOfflineMode();
#else
	return false;
#endif
}

bool CommandNetworkIsNpOffline()
{
#if RSG_NP
    if(!NetworkInterface::IsLocalPlayerSignedIn())
        return true;

    return g_rlNp.IsOffline(NetworkInterface::GetLocalGamerIndex());
#else
    return true;
#endif
}

bool CommandNetworkIsNpAvailable()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && g_rlNp.GetNpAuth().IsNpAvailable(localGamerIndex);
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
	return true;
#endif
}

bool CommandNetworkIsNpPending()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && g_rlNp.IsPendingOnline(localGamerIndex);
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
	return true;
#endif
}

int CommandNetworkGetNpUnavailableReason()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return g_rlNp.GetNpAuth().GetNpUnavailabilityReason(localGamerIndex);
	}
	
	return rlNpAuth::NP_REASON_SIGNED_OUT;
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
	return 0;
#endif
}

bool CommandNetworkIsConnectedToNpPresence()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && g_rlNp.IsConnectedToPresence(NetworkInterface::GetLocalGamerIndex());
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
	return true;
#endif
}

bool CommandNetworkIsLoggedInToPsn()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && g_rlNp.IsLoggedIn(NetworkInterface::GetLocalGamerIndex());
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
	return false;
#endif
}

void CommandNetworkRecheckNpAvailability()
{
#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		// Np Must be in unavailable state
		Assert(!g_rlNp.IsNpAvailable(localGamerIndex));

		g_rlNp.RecheckNpAvailability(localGamerIndex);
	}
#else
	Assertf(false, "Np functionality not available on this platform (PS4 only)");
#endif
}
bool CommandNetworkHasValidRosCredentials()
{
	return NetworkInterface::HasValidRosCredentials();
}

bool CommandNetworkIsRefreshingRosCredentials()
{
	return NetworkInterface::IsRefreshingRosCredentials();
}

bool CommandNetworkIsCloudAvailable()
{
	return NetworkInterface::IsCloudAvailable();
}

bool CommandNetworkHasSocialClubAccount()
{
	return NetworkInterface::HasValidRockstarId();
}

bool CommandNetworkAreSocialClubPoliciesCurrent()
{
	return CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub() && CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate();
}

bool CommandNetworkLanSessionsOnly()
{
	return NetworkInterface::LanSessionsOnly();
}

bool CommandNetworkIsHost()
{
	return NetworkInterface::IsHost();
}

int CommandNetworkGetHostPlayerIndex()
{
	CNetGamePlayer* pHostPlayer = NetworkInterface::GetHostPlayer();
	return pHostPlayer ? pHostPlayer->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;
}

bool CommandNetworkWasGameSuspended()
{
	return CLiveManager::WasGameSuspended();
}

bool CommandIsReputationBad()
{
	return CLiveManager::IsOverallReputationBad();
}

bool CommandHaveOnlinePrivileges()
{
    return CLiveManager::CheckOnlinePrivileges();
}

bool CommandHasAgeRestrictions()
{
	return CLiveManager::CheckIsAgeRestricted();
}

bool CommandHasSocialNetworkingSharingPriv() 
{
	return CLiveManager::GetSocialNetworkingSharingPrivileges();
}

int CommandGetAgeGroup()
{
	return CLiveManager::GetAgeGroup();
}

bool CommandHaveUserContentPrivileges(int)
{
#if RSG_DURANGO || RSG_XENON || RSG_PC
	// Xbox TRC TCR 094 - use every gamer index
	return CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_EVERYONE);
#elif RSG_ORBIS || RSG_PS3
	// Playstation R4063 - use the local gamer index
	return CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL);
#endif
}

bool CommandHaveCommunicationPrivileges(int, int playerIndex)
{
	if (playerIndex != -1)
	{
		netPlayer* player = CTheScripts::FindNetworkPlayer( playerIndex );
		if(player)
		{
			if (player->IsLocal())
			{
				return CLiveManager::CheckVoiceCommunicationPrivileges();
			}
			else
			{
				return static_cast<CNetGamePlayer*>(player)->GetCommunicationPrivileges();
			}
		}
	}

	return CLiveManager::CheckVoiceCommunicationPrivileges();
}

bool CommandCheckOnlinePrivileges(const int nGamerIndex, bool bCheckHasPrivilege)
{
    return CLiveManager::CheckOnlinePrivileges(nGamerIndex, bCheckHasPrivilege);
}

bool CommandCheckUserContentPrivileges(int, const int nGamerIndex, bool bCheckHasPrivilege)
{
    return CLiveManager::CheckUserContentPrivileges(nGamerIndex, bCheckHasPrivilege);
}

bool CommandCheckCommunicationPrivileges(int, const int nGamerIndex, bool bCheckHasPrivilege)
{
    return CLiveManager::CheckVoiceCommunicationPrivileges(nGamerIndex, bCheckHasPrivilege);
}

bool CommandCheckTextCommunicationPrivileges(int, const int nGamerIndex, bool bCheckHasPrivilege)
{
	return CLiveManager::CheckTextCommunicationPrivileges(nGamerIndex, bCheckHasPrivilege);
}

bool CommandIsUsingOnlinePromotion()
{
#if RSG_ORBIS
    if(NetworkInterface::HasRawPlayStationPlusPrivileges())
        return false;

    return CLiveManager::CheckOnlinePrivileges(NetworkInterface::GetLocalGamerIndex());
#else
    return false;
#endif
}

bool CommandShouldShowPromotionAlertScreen()
{
#if RSG_ORBIS
    return NetworkInterface::IsPlayStationPlusPromotionEnabled() && !NetworkInterface::DoesTunableDisablePlusEntryPrompt();
#else
    return false;
#endif
}

bool CommandHavePlatformSubscription()
{
	// validate this for script calls (this will assert otherwise in the call - we want to keep those 
	// asserts to catch invalid code calls but returning false is fine for script)
	if(!NetworkInterface::IsLocalPlayerSignedIn())
		return false;

#if RSG_SCE
	// IsPlayStationPlusPromotionEnabled should not strictly enable this but script rely on this during PS+ promotions
	// if we need the distinction, we can add a specific check 
	return CLiveManager::HasPlatformSubscription() || CLiveManager::IsPlayStationPlusPromotionEnabled();
#else
	return CLiveManager::HasPlatformSubscription();
#endif
}

bool CommandIsPlatformSubscriptionCheckPending()
{
	// validate this for script calls (this will assert otherwise in the call - we want to keep those 
	// asserts to catch invalid code calls but returning false is fine for script)
	if (!NetworkInterface::IsLocalPlayerSignedIn())
		return false;

	return CLiveManager::IsPlatformSubscriptionCheckPending();
}

void CommandShowAccountUpgradeUI()
{
	CLiveManager::ShowAccountUpgradeUI();
}

bool CommandIsShowingSystemUiOrRecentlyRequestedUpsell()
{
	// accounts for both onscreen system UI or a recently requested upsell (deep link only)
	// a deep link upsell can sometimes take a little time to show on-screen
	return CLiveManager::IsSystemUiShowing() || CLiveManager::HasRecentUpsellRequest();
}

bool CommandNetworkNeedToStartNewGameButBlocked()
{
#if RSG_GEN9
	// command line to manage to switch from this being handled in script to this being handled in code
	if(!PARAM_netScriptHandleNewGameFlow.Get())
		return false;
#endif 

	return CLiveManager::HasPendingProfileChange() && CLiveManager::CanActionProfileChange();
}

bool CommandCheckPrivileges(int, int nPrivilegeTypeBitfield, bool bAttemptResolution)
{
	return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_CHECK_PRIVILEGES")) && rlPrivileges::CheckPrivileges(NetworkInterface::GetLocalGamerIndex(), nPrivilegeTypeBitfield, bAttemptResolution);
}

bool CommandIsPrivilegeCheckResultReady()
{
	return rlPrivileges::IsPrivilegeCheckResultReady();
}

bool CommandIsPrivilegeCheckInProgress()
{
	return rlPrivileges::IsPrivilegeCheckInProgress();
}

bool CommandIsPrivilegeCheckSuccessful()
{
	return rlPrivileges::IsPrivilegeCheckSuccessful();
}

void CommandSetPrivilegeCheckResultNotNeeded()
{
	rlPrivileges::SetPrivilegeCheckResultNotNeeded();
}

bool CommandNetworkResolvePrivilegeOnlineMultiplayer()
{
#if RSG_XBOX
	return CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_MULTIPLAYER_SESSIONS, true);
#else
	return true;
#endif
}

bool CommandNetworkResolvePrivilegeUserContent()
{
#if RSG_XBOX
	return CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_USER_CREATED_CONTENT, true);
#else
	return true;
#endif
}

bool CommandNetworkCanBail()
{
	return NetworkInterface::CanBail(); 
}

void CommandNetworkBail(int nContextParam1, int nContextParam2, int nContextParam3)
{
	scriptDebugf1("%s : NETWORK_BAIL called : ContentParams: %d, %d, %d",
		CTheScripts::GetCurrentScriptNameAndProgramCounter(),
		nContextParam1,
		nContextParam2,
		nContextParam3);

	if(scriptVerifyf(NetworkInterface::CanBail(), "%s : NETWORK_BAIL - Can't Bail", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		CNetwork::Bail(sBailParameters(BAIL_FROM_SCRIPT, BAIL_CTX_NONE, nContextParam1, nContextParam2, nContextParam3));
}

void CommandNetworkOnReturnToSinglePlayer()
{
    CNetwork::GetNetworkSession().OnReturnToSinglePlayer();
}

void CommandNetworkSetMatchmakingValue(int nValueID, int nValue)
{
	if(!scriptVerify(nValueID >= 0 && nValueID < MatchmakingAttributes::MM_ATTR_NUM))
		return;

	MatchmakingAttributes& rAttributes = CNetwork::GetNetworkSession().GetMatchmakingAttributes();
	rAttributes.GetAttribute(static_cast<MatchmakingAttributes::eAttribute>(nValueID))->Apply(static_cast<unsigned>(nValue));
}

void CommandNetworkClearMatchmakingValue(int nValueID)
{
	if(!scriptVerify(nValueID >= 0 && nValueID < MatchmakingAttributes::MM_ATTR_NUM))
		return;

	MatchmakingAttributes& rAttributes = CNetwork::GetNetworkSession().GetMatchmakingAttributes();
	rAttributes.GetAttribute(static_cast<MatchmakingAttributes::eAttribute>(nValueID))->Clear();
}

void CommandNetworkSetMatchmakingQueryState(int nQueryID, bool bEnabled)
{
    CNetwork::GetNetworkSession().SetMatchmakingQueryState(static_cast<MatchmakingQueryType>(nQueryID), bEnabled);
}

bool CommandNetworkGetMatchmakingQueryState(int nQueryID)
{
    return CNetwork::GetNetworkSession().GetMatchmakingQueryState(static_cast<MatchmakingQueryType>(nQueryID));
}

bool CheckMultiplayerAccess(OUTPUT_ONLY(const char* szCommand))
{
#if !__NO_OUTPUT
	const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot()+1;
	const bool statsLoaded = !StatsInterface::CloudFileLoadPending(0) && !StatsInterface::CloudFileLoadPending(selectedSlot);

    // this isn't a failure case - just error and log
    if(!scriptVerify(statsLoaded))
    {
        scriptErrorf("%s::%s - Stats load pending!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand);
    }
#endif

	// bail out if we can't access
	eNetworkAccessCode accessCode;
	if(!scriptVerify(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter, &accessCode)))
	{
		scriptErrorf("%s::%s - Cannot access multiplayer! Code: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand, accessCode);
		return false;
	}

	// bail out if we can't enter
	if(!scriptVerify(CNetwork::GetNetworkSession().CanEnterMultiplayer()))
	{
		scriptErrorf("%s::%s - Cannot enter multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand);
		return false;
	}

	return true;
}

bool CommandNetworkCanAccessMultiplayer(int& nAccessCode)
{
	eNetworkAccessCode accessCode;
    bool bSuccess = CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter, &accessCode);
    nAccessCode = static_cast<int>(accessCode);
    return bSuccess;
}

bool CommandNetworkCheckNetworkAccessAndAlertIfFail()
{
	return CNetwork::CheckNetworkAccessAndAlertIfFail(eNetworkAccessArea::AccessArea_MultiplayerEnter);
}

bool CommandNetworkIsMultiplayerDisabled()
{
    return CNetwork::IsMultiplayerDisabled();
}

bool CommandNetworkCanEnterMultiplayer()
{
	return CNetwork::GetNetworkSession().CanEnterMultiplayer();
}

void CommandNetworkTransitionStart(int nTransitionType, int nContextParam1, int nContextParam2, int nContextParam3)
{
	CNetwork::StartTransitionTracker(nTransitionType, nContextParam1, nContextParam2, nContextParam3);
}

void CommandNetworkTransitionAddStage(int nStageHash, int nStageSlot, int nContextParam1, int nContextParam2, int nContextParam3)
{
	CNetwork::AddTransitionTrackerStage(nStageSlot, nStageHash, nContextParam1, nContextParam2, nContextParam3);
}

void CommandNetworkTransitionFinish(int nContextParam1, int nContextParam2, int nContextParam3)
{
	CNetwork::FinishTransitionTracker(nContextParam1, nContextParam2, nContextParam3);
}

bool CommandNetworkDoFreeroamQuickmatch(const int gameMode, const int maxPlayers, const int matchmakingFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_DO_FREEROAM_QUICKMATCH called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_DO_FREEROAM_QUICKMATCH")))
		return false;

	// this encompasses all of our freeroam queries (platform, friend and crew / social matchmaking)
	return NetworkInterface::DoQuickMatch(gameMode, SCHEMA_GROUPS, maxPlayers, static_cast<unsigned>(matchmakingFlags));
}

bool CommandNetworkQuickmatch(int nGameMode, int, int, int nMaxPlayers, bool, bool bConsiderBlacklisted)
{
	scriptDebugf1("%s : NETWORK_SESSION_ENTER called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// build flags
	unsigned matchmakingFlags = MatchmakingFlags::MMF_Default;
	if(bConsiderBlacklisted)
		matchmakingFlags |= MatchmakingFlags::MMF_AllowBlacklisted;

	return CommandNetworkDoFreeroamQuickmatch(nGameMode, nMaxPlayers, matchmakingFlags);
}

bool CommandNetworkDoFriendMatchmaking(const int gameMode, const int maxPlayers, const int matchmakingFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_DO_FRIEND_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_DO_FRIEND_MATCHMAKING")))
		return false;

	// start matchmaking
	return CNetwork::GetNetworkSession().DoFriendMatchmaking(gameMode, SCHEMA_GROUPS, maxPlayers, static_cast<unsigned>(matchmakingFlags));
}

bool CommandNetworkFriendMatchmaking(int nGameMode, int, int nMaxPlayers, bool)
{
	scriptDebugf1("%s : NETWORK_SESSION_FRIEND_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const unsigned matchmakingFlags = MatchmakingFlags::MMF_Default;
	return CommandNetworkDoFriendMatchmaking(nGameMode, nMaxPlayers, matchmakingFlags);
}

bool CommandNetworkDoSocialMatchmaking(const char* szQuery, const char* szParams, const int gameMode, const int maxPlayers, const int matchmakingFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_DO_SOCIAL_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_DO_SOCIAL_MATCHMAKING")))
		return false;

	// use default flags
	return NetworkInterface::DoSocialMatchmaking(szQuery, szParams, gameMode, SCHEMA_GROUPS, maxPlayers, static_cast<unsigned>(matchmakingFlags));
}

bool CommandNetworkSocialMatchmaking(const char* szQuery, const char* szParams, int nGameMode, int, int nMaxPlayers, bool)
{
	scriptDebugf1("%s : NETWORK_SESSION_SOCIAL_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const unsigned matchmakingFlags = MatchmakingFlags::MMF_Default;
	return CommandNetworkDoSocialMatchmaking(szQuery, szParams, nGameMode, nMaxPlayers, matchmakingFlags);
}

bool CommandNetworkDoCrewMatchmaking(const int crewId_in, const int gameMode, const int maxPlayers, const int matchmakingFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_DO_CREW_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_DO_CREW_MATCHMAKING")))
		return false;

	// need to convert to rlClanId
	rlClanId crewId = static_cast<rlClanId>(crewId_in);

	// if no crew specified, use the primary crew for matchmaking
	NetworkClan& tClan = CLiveManager::GetNetworkClan();
	if(crewId == RL_INVALID_CLAN_ID && tClan.HasPrimaryClan())
		crewId = tClan.GetPrimaryClan()->m_Id;

	// build parameters
	if(crewId != RL_INVALID_CLAN_ID)
	{
		char szParams[256];
		snprintf(szParams, 256, "@crewid,%" I64FMT "d", crewId);

		// use default flags
		return NetworkInterface::DoSocialMatchmaking("CrewmateSessions", szParams, gameMode, SCHEMA_GROUPS, maxPlayers, static_cast<unsigned>(matchmakingFlags));
	}

	// no crew specified and no primary crew
	return false;
}

bool CommandNetworkCrewMatchmaking(int nCrewID, int nGameMode, int, int nMaxPlayers, bool)
{
	scriptDebugf1("%s : NETWORK_SESSION_CREW_MATCHMAKING called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const unsigned matchmakingFlags = MatchmakingFlags::MMF_Default;
	return CommandNetworkDoCrewMatchmaking(nCrewID, nGameMode, nMaxPlayers, matchmakingFlags);
}

bool CommandNetworkDoActivityQuickmatch(const int gameMode, const int maxPlayers, const int activityType, const int activityId, const int matchmakingFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_DO_ACTIVITY_QUICKMATCH called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_DO_ACTIVITY_QUICKMATCH")))
		return false;

	return NetworkInterface::DoActivityQuickmatch(gameMode, maxPlayers, activityType, activityId, static_cast<unsigned>(matchmakingFlags));
}

bool CommandNetworkActivityQuickmatch(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityId)
{
	scriptDebugf1("%s : NETWORK_SESSION_ACTIVITY_QUICKMATCH called", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const unsigned matchmakingFlags = MatchmakingFlags::MMF_Default;
	return CommandNetworkDoActivityQuickmatch(nGameMode, nMaxPlayers, nActivityType, nActivityId, matchmakingFlags);
}

bool CommandNetworkHostSession(int nGameMode, int nMaxPlayers, bool bIsPrivate)
{
	scriptDebugf1("%s : NETWORK_SESSION_HOST called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_HOST")))
		return false;

    const unsigned nHostFlags = bIsPrivate ? HostFlags::HF_IsPrivate : HostFlags::HF_Default;
	return NetworkInterface::HostSession(nGameMode, nMaxPlayers, nHostFlags | HostFlags::HF_ViaScript);
}

bool CommandNetworkHostClosedSession(int nGameMode, int nMaxPlayers)
{
	scriptDebugf1("%s : NETWORK_SESSION_HOST_CLOSED called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_HOST_CLOSED")))
		return false;

    return CNetwork::GetNetworkSession().HostClosed(nGameMode, nMaxPlayers, HostFlags::HF_ViaScript);
}

bool CommandNetworkHostClosedFriendSession(int nGameMode, int nMaxPlayers)
{
	scriptDebugf1("%s : NETWORK_SESSION_HOST_FRIENDS_ONLY called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_HOST_FRIENDS_ONLY")))
		return false;

    return CNetwork::GetNetworkSession().HostClosedFriends(nGameMode, nMaxPlayers, HostFlags::HF_ViaScript);
}

bool CommandNetworkHostClosedCrewSession(int nGameMode, int nMaxPlayers, int nUniqueCrewLimit, int nCrewLimitMaxMembers)
{
	scriptDebugf1("%s : NETWORK_SESSION_HOST_CLOSED_CREW called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_SESSION_HOST_CLOSED_CREW")))
		return false;

    return CNetwork::GetNetworkSession().HostClosedCrew(nGameMode, nMaxPlayers, static_cast<unsigned>(nUniqueCrewLimit), static_cast<unsigned>(nCrewLimitMaxMembers), HostFlags::HF_ViaScript);
}

bool CommandNetworkIsClosedFriendsSession()
{
	return CNetwork::GetNetworkSession().IsClosedFriendsSession(SessionType::ST_Physical);
}

bool CommandNetworkIsClosedCrewSession()
{
	return CNetwork::GetNetworkSession().IsClosedCrewSession(SessionType::ST_Physical);
}

bool CommandNetworkIsSoloSession()
{
	return CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical);
}

bool CommandNetworkIsPrivateSession()
{
	return CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Physical);
}

bool CommandNetworkStartSession()
{
	scriptDebugf1("%s : NETWORK_SESSION_START called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
}

bool CommandNetworkEndSession(bool bReturnToLobby, bool bBlacklist)
{
	scriptDebugf1("%s : NETWORK_SESSION_END called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(scriptVerifyf(NetworkInterface::CanSessionLeave(), "%s : NETWORK_END_SESSION - Can't End", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return NetworkInterface::LeaveSession(bReturnToLobby, bBlacklist);
	return false;
}

bool CommandNetworkLeaveSession(const int leaveFlags)
{
	scriptDebugf1("%s : NETWORK_SESSION_LEAVE called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (scriptVerifyf(NetworkInterface::CanSessionLeave(), "%s : NETWORK_SESSION_LEAVE - Can't Leave", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return NetworkInterface::LeaveSession(leaveFlags);
	return false;
}

void CommandNetworkKickPlayer(const int nPlayerIndex)
{
	scriptDebugf1("%s : NETWORK_SESSION_KICK_PLAYER called. Index: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex);
	scriptAssertf(NetworkInterface::IsHost(), "%s : NETWORK_SESSION_KICK_PLAYER - Only the host can kick players!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(scriptVerifyf(pPlayer, "%s : NETWORK_SESSION_KICK_PLAYER - No player at index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
		CNetwork::GetNetworkSession().KickPlayer(pPlayer, KickReason::KR_VotedOut, 0, RL_INVALID_GAMER_HANDLE);
}

bool CommandNetworkGetKickVote(const int nPlayerIndex)
{
	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(scriptVerifyf(pPlayer, "%s : NETWORK_SESSION_GET_KICK_VOTE - No player at index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
	{
		return CContextMenuHelper::GetKickVote(pPlayer->GetPhysicalPlayerIndex());
	}

	return false;
}

bool CommandReserveSlots(const SessionType sessionType, int& hGamerHandleData, int nGamers, int nReservationTimeout)
{
	rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION - 1];
	unsigned nValidGamers = 0;

	if(nGamers < 0)
		return false; 

	if(!scriptVerify(nGamers <= RL_MAX_GAMERS_PER_SESSION - 1))
	{
		scriptErrorf("%s:NETWORK_SESSION_RESERVE_SLOTS - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), RL_MAX_GAMERS_PER_SESSION - 1, nGamers);
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in gamer handles
	for(int i = 0; i < nGamers; i++)
	{ 
		if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_SESSION_RESERVE_SLOTS")))
			nValidGamers++;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	return CNetwork::GetNetworkSession().ReserveSlots(sessionType, hGamers, nValidGamers, static_cast<unsigned>(nReservationTimeout));
}

bool CommandNetworkReserveSlots(int& hGamerHandleData, int nGamers, int nReservationTimeout)
{
	return CommandReserveSlots(SessionType::ST_Physical, hGamerHandleData, nGamers, nReservationTimeout);
}

bool CommandNetworkReserveSlotsTransition(int& hGamerHandleData, int nGamers, int nReservationTimeout)
{
	return CommandReserveSlots(SessionType::ST_Transition, hGamerHandleData, nGamers, nReservationTimeout);
}

bool CommandNetworkReserveSlotAndLeaveSession(const int)
{
	// deprecated
	scriptAssertf(0, "CommandNetworkReserveSlotAndLeaveSession :: This command is deprecated!");
	return false; 
}

bool CommandNetworkJoinPreviousSession(const bool bMatchmakeOnFailure)
{
    return CNetwork::GetNetworkSession().JoinPreviousSession(bMatchmakeOnFailure ? JoinFailureAction::JFA_Quickmatch : JoinFailureAction::JFA_None);
}

bool CommandNetworkJoinPreviouslyFailedSession()
{
	return CNetwork::GetNetworkSession().JoinPreviouslyFailed();
}

bool CommandNetworkJoinPreviouslyFailedTransition()
{
	return CNetwork::GetNetworkSession().JoinPreviouslyFailedTransition();
}

void CommandNetworkSetMatchmakingGroup(const int nMatchmakingGroup)
{
	// validate index
	if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_SET_MATCHMAKING_GROUP - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// call through to session
	CNetwork::GetNetworkSession().SetMatchmakingGroup(static_cast<eMatchmakingGroup>(nMatchmakingGroup));
}

void CommandNetworkSetMatchmakingGroupMax(const int nMatchmakingGroup, const int nMaximum)
{
	// validate index
	if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_SET_MATCHMAKING_GROUP_MAX - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// validate maximum
	if(!scriptVerifyf(nMaximum >= 0, "%s : NETWORK_SESSION_SET_MATCHMAKING_GROUP_MAX - Invalid Maximum: %d!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMaximum))
		return;

	// call through to session
	CNetwork::GetNetworkSession().SetMatchmakingGroupMax(static_cast<eMatchmakingGroup>(nMatchmakingGroup), static_cast<unsigned>(nMaximum));
}

int CommandNetworkGetMatchmakingGroupMax(const int nMatchmakingGroup)
{
	// validate index
	if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_GET_MATCHMAKING_GROUP_MAX - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return 0;

	// call through to session
	return CNetwork::GetNetworkSession().GetMatchmakingGroupMax(static_cast<eMatchmakingGroup>(nMatchmakingGroup));
}

int CommandNetworkGetMatchmakingGroupNum(const int nMatchmakingGroup)
{
    // validate index
    if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_GET_MATCHMAKING_GROUP_NUM - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return 0;

    // call through to session
    return CNetwork::GetNetworkSession().GetMatchmakingGroupNum(static_cast<eMatchmakingGroup>(nMatchmakingGroup));
}

int CommandNetworkGetMatchmakingGroupFree(const int nMatchmakingGroup)
{
    // validate index
    if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_GET_MATCHMAKING_GROUP_FREE - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return 0;

    // call through to session
    return CNetwork::GetNetworkSession().GetMatchmakingGroupFree(static_cast<eMatchmakingGroup>(nMatchmakingGroup));
}

void CommandNetworkAddActiveMatchmakingGroup(const int nMatchmakingGroup)
{
	// validate index
	if(!scriptVerifyf((nMatchmakingGroup > MM_GROUP_INVALID) && (nMatchmakingGroup < MM_GROUP_MAX), "%s : NETWORK_SESSION_ADD_ACTIVE_MATCHMAKING_GROUP - Invalid Group!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().AddActiveMatchmakingGroup(static_cast<eMatchmakingGroup>(nMatchmakingGroup));
}

void CommandNetworkSetUniqueCrewLimit(const int nUniqueCrewLimit)
{
	CNetwork::GetNetworkSession().SetUniqueCrewLimit(SessionType::ST_Physical, static_cast<unsigned>(nUniqueCrewLimit));
}

void CommandNetworkSetUniqueCrewOnlyCrews(const bool bOnlyCrews)
{
    CNetwork::GetNetworkSession().SetUniqueCrewOnlyCrews(SessionType::ST_Physical, bOnlyCrews);
}

int CommandNetworkGetUniqueCrewLimit()
{
	return static_cast<int>(CNetwork::GetNetworkSession().GetUniqueCrewLimit(SessionType::ST_Physical));
}

void CommandNetworkSetCrewLimitMaxMembers(const int nMaxMembers)
{
    CNetwork::GetNetworkSession().SetCrewLimitMaxMembers(SessionType::ST_Physical, static_cast<unsigned>(nMaxMembers));
}

void CommandNetworkSetUniqueCrewLimitTransition(const int nUniqueCrewLimit)
{
    CNetwork::GetNetworkSession().SetUniqueCrewLimit(SessionType::ST_Transition, static_cast<unsigned>(nUniqueCrewLimit));
}

void CommandNetworkSetUniqueCrewOnlyCrewsTransition(const bool bOnlyCrews)
{
    CNetwork::GetNetworkSession().SetUniqueCrewOnlyCrews(SessionType::ST_Transition, bOnlyCrews);
}

void CommandNetworkSetCrewLimitMaxMembersTransition(const int nMaxMembers)
{
    CNetwork::GetNetworkSession().SetCrewLimitMaxMembers(SessionType::ST_Transition, static_cast<unsigned>(nMaxMembers));
}

int CommandNetworkGetUniqueCrewLimitTransition()
{
	return static_cast<int>(CNetwork::GetNetworkSession().GetUniqueCrewLimit(SessionType::ST_Transition));
}

void CommandNetworkSetMatchmakingPropertyID(const int nPropertyID)
{
	CNetwork::GetNetworkSession().SetMatchmakingPropertyID(static_cast<u8>(nPropertyID));
}

void CommandNetworkSetMatchmakingMentalState(const int nMentalState)
{
	// verify state is within limit
	if(!scriptVerifyf(static_cast<u8>(nMentalState) <= MAX_MENTAL_STATE, "%s:NETWORK_SESSION_SET_MATCHMAKING_MENTAL_STATE - Mental state too high. Given: %d, Maximum: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMentalState, MAX_MENTAL_STATE))
		return;

	CNetwork::GetNetworkSession().SetMatchmakingMentalState(static_cast<u8>(nMentalState));
}

void CommandNetworkSetMatchmakingELO(const int nELO)
{
    CNetwork::GetNetworkSession().SetMatchmakingELO(static_cast<u16>(nELO));
}

void CommandNetworkSetScriptValidateJoin()
{
	NetworkInterface::SetScriptValidateJoin();
}

void CommandNetworkValidateJoin(bool bJoinSuccessful)
{
	NetworkInterface::ValidateJoin(bJoinSuccessful);
}

void CommandNetworkSetEnterMatchmakingAsBoss(bool bEnterMatchmakingAsBoss)
{
	CNetwork::GetNetworkSession().SetEnterMatchmakingAsBoss(bEnterMatchmakingAsBoss);
}

void CommandNetworkSetNumBosses(const int nNumBosses)
{
	CNetwork::GetNetworkSession().SetNumBosses(nNumBosses);
}

void CommandNetworkAddFollowers(int& hGamerHandleData, int nGamers)
{
    rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION];
    unsigned nValidGamers = 0;

    if(!scriptVerify(nGamers <= RL_MAX_GAMERS_PER_SESSION))
    {
        scriptErrorf("%s:NETWORK_ADD_FOLLOWERS - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), RL_MAX_GAMERS_PER_SESSION, nGamers);
        return;
    }

    // buffer to write our info into
    u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

    // skip array size prefix
    pInfoBuffer += sizeof(scrValue);

    // fill in gamer handles
    for(u32 i = 0; i < nGamers; i++)
    { 
        if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_ADD_FOLLOWERS")))
            nValidGamers++;

        pInfoBuffer += GAMER_HANDLE_SIZE;
    }

    CNetwork::GetNetworkSession().AddFollowers(hGamers, nValidGamers);
}

void CommandNetworkRemoveFollowers(int& hGamerHandleData, int nGamers)
{
    rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION];
    unsigned nValidGamers = 0;

    if(!scriptVerify(nGamers <= RL_MAX_GAMERS_PER_SESSION))
    {
        scriptErrorf("%s:NETWORK_REMOVE_FOLLOWERS - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), RL_MAX_GAMERS_PER_SESSION, nGamers);
        return;
    }

    // buffer to write our info into
    u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

    // skip array size prefix
    pInfoBuffer += sizeof(scrValue);

    // fill in gamer handles
    for(u32 i = 0; i < nGamers; i++)
    { 
        if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_REMOVE_FOLLOWERS")))
            nValidGamers++;

        pInfoBuffer += GAMER_HANDLE_SIZE;
    }

    CNetwork::GetNetworkSession().RemoveFollowers(hGamers, nValidGamers);
}

bool CommandNetworkHasFollower(int& hData)
{
    rlGamerHandle hGamer;
    if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_FOLLOWER")))
        return CNetwork::GetNetworkSession().HasFollower(hGamer);

    // invalid rlGamerHandle
    return false;
}

void CommandNetworkClearFollowers()
{
    return CNetwork::GetNetworkSession().ClearFollowers();
}

void CommandNetworkRetainFollowers(bool bRetain)
{
    CNetwork::GetNetworkSession().RetainFollowers(bRetain);
}

void CommandNetworkSetScriptHandlingClockAndWeather(bool bScriptHandling)
{
	CNetwork::GetNetworkSession().SetScriptHandlingClockAndWeather(bScriptHandling);
}

void CommandNetworkApplyClockAndWeather(bool bDoTransition, int nTransitionTime)
{
	scriptDebugf1("CommandNetworkApplyClockAndWeather :: DoTransition: %s, TransitionTime: %d", bDoTransition ? "True" : "False", nTransitionTime);
	CNetwork::ApplyClockAndWeather(bDoTransition, static_cast<unsigned>(nTransitionTime));
}

void CommandNetworkGetGlobalMultiplayerClock(int& nHour, int &nMinute, int& nSecond)
{
	// retrieve global clock settings
	CNetwork::GetGlobalClock(nHour, nMinute, nSecond);
}

void CommandNetworkGetGlobalMultiplayerWeather(int& nCycleIndex, int &nCycleTimer)
{
	// retrieve global clock settings
	unsigned uCycleTimer;
	CNetwork::GetGlobalWeather(nCycleIndex, uCycleTimer);
	nCycleTimer = static_cast<int>(uCycleTimer);
}

void CommandNetworkSessionSetGameMode(int nGameMode)
{
	CNetwork::SetNetworkGameMode(static_cast<u16>(nGameMode));
}

void CommandNetworkSessionSetGameModeState(int nGameModeState)
{
	CNetwork::SetNetworkGameModeState(static_cast<u16>(nGameModeState));
}

int CommandNetworkSessionGetHostAimPreference()
{
    return CNetwork::GetNetworkSession().GetHostAimPreference();
}

int CommandNetworkSessionGetAimPreferenceAsMatchmaking(int nAimPreference)
{
    return static_cast<int>(NetworkBaseConfig::GetMatchmakingAimBucket(nAimPreference));
}

void CommandNetworkSessionSetAimBucketingEnabled(bool bIsEnabled)
{
	NetworkBaseConfig::SetAimBucketingEnabled(bIsEnabled);
}

void CommandNetworkSessionSetAimBucketingEnabledForArcadeGameModes(bool bIsEnabled)
{
	NetworkBaseConfig::SetAimBucketingEnabledForArcadeGameModes(bIsEnabled);
}

bool CommandNetworkFindGamers(const char* szQuery, const char* szParams)
{
	return CLiveManager::FindGamersViaPresence(szQuery, szParams);
}

bool CommandNetworkFindGamersInCrew(int nCrewID)
{
	// need to convert to rlClanId
	rlClanId nRlCrewID = static_cast<rlClanId>(nCrewID);

	// if no crew specified, use the primary crew for matchmaking
	NetworkClan& tClan = CLiveManager::GetNetworkClan();
	if(nRlCrewID == 0 && tClan.HasPrimaryClan())
		nRlCrewID = tClan.GetPrimaryClan()->m_Id;

	static bool bIsRandom = true; 

	// build parameters
	if(nRlCrewID > 0)
	{
		char szParams[256];
		snprintf(szParams, 256, "@crewid,%" I64FMT "d,@isRandom,%d", nRlCrewID, bIsRandom ? 1 : 0);

		// start matchmaking
		return CLiveManager::FindGamersViaPresence("CrewmatesOnline", szParams);
	}

	return false;
}

bool CommandNetworkFindMatchedGamers(int nActivityID, float fSkill, float fLowerLimit, float fUpperLimit)
{
	bool bAttributeSet = SCPRESENCEUTIL.IsActivityAttributeSet(nActivityID);
	
	// validate code
	if(!scriptVerifyf(bAttributeSet, "%s : NETWORK_FIND_MATCHED_GAMERS - Activity Attribute (%d) Not Set!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nActivityID))
		return false;

	atString szParams;
	char szParam[256];

	// add attribute 
	formatf(szParam, 256, "@attr,%d", nActivityID);
	szParams += szParam;

	// add skill
	//formatf(szParam, 256, ",@skill,%f", fSkill);
	//szParams += szParam;

	// define lower limit if not provided
	if(fLowerLimit < 0.0f)
		fLowerLimit = Max(0.0f, fSkill - 0.1f);

	// add lower limit
	formatf(szParam, 256, ",@llimit,%f", fLowerLimit);
	szParams += szParam;

	// define upper limit if not provided
	if(fUpperLimit < 0.0f)
		fUpperLimit = Min(1.0f, fSkill + 0.1f);

	// add upper limit
	formatf(szParam, 256, ",@ulimit,%f", fUpperLimit);
	szParams += szParam;

	// call into find gamers function
	return CLiveManager::FindGamersViaPresence("FindMatchedGamers", szParams);
}

bool CommandNetworkIsFindingGamers()
{
	return CLiveManager::IsFindingGamers();
}

bool CommandNetworkDidFindGamersSucceed()
{
	return CLiveManager::DidFindGamersSucceed();
}

int CommandNetworkGetNumFoundGamers()
{
	return static_cast<int>(CLiveManager::GetNumFoundGamers());
}

bool CommandNetworkGetFoundGamer(int& hData, int nIndex)
{
	rlGamerQueryData hQueryData; 
	bool bSuccess = CLiveManager::GetFoundGamer(nIndex, &hQueryData);
	if(!bSuccess)
		return false;

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// copy in handle
	hQueryData.m_GamerHandle.Export(pInfoBuffer, GAMER_HANDLE_SIZE);
	pInfoBuffer += GAMER_HANDLE_SIZE;

	// copy in name
	safecpy(reinterpret_cast<char*>(pInfoBuffer), hQueryData.GetName(), sizeof(scrTextLabel63));
	pInfoBuffer += sizeof(scrTextLabel63);

	// finished
	return true;
}

int CommandNetworkGetFoundGamers(int& infoData, int sizeOfData)
{
	unsigned nNumGamers = 0;
	rlGamerQueryData* pGamers = CLiveManager::GetFoundGamers(&nNumGamers);
	if(nNumGamers == 0)
		return 0;

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&infoData);

	// script returns size in script words
	sizeOfData *= sizeof(scrValue); 

	// calculate size required
	int nRequiredSize = 0;
	nRequiredSize += sizeof(scrValue);												// member count
	nRequiredSize += sizeof(scrValue);												// array size prefix
	nRequiredSize += GAMER_HANDLE_SIZE * CLiveManager::MAX_FIND_GAMERS;				// gamer handle for each member
	nRequiredSize += sizeof(scrTextLabel63) * CLiveManager::MAX_FIND_GAMERS;		// name string for each member

	// validate
	if(!gnetVerifyf(nRequiredSize == sizeOfData, "%s:NETWORK_GET_FOUND_GAMERS - Invalid size!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return 0;

	// copy in member count
	sysMemCpy(pInfoBuffer, &nNumGamers, sizeof(scrValue));
	pInfoBuffer += sizeof(scrValue);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in party members
	for(u32 i = 0; i < nNumGamers; i++)
	{
		// copy in handle
		pGamers[i].m_GamerHandle.Export(pInfoBuffer, GAMER_HANDLE_SIZE);
		pInfoBuffer += GAMER_HANDLE_SIZE;

		// copy in name
		safecpy(reinterpret_cast<char*>(pInfoBuffer), pGamers[i].GetName(), sizeof(scrTextLabel63));
		pInfoBuffer += sizeof(scrTextLabel63);
	}

	// return value is number of members
	return nNumGamers;
}

void CommandNetworkClearFoundGamers()
{
	CLiveManager::ClearFoundGamers();
}

bool CommandNetworkGetGamerStatus(int& handleData, int nGamers)
{
	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&handleData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in leaderboard handles
	rlGamerHandle ahGamers[CLiveManager::MAX_GAMER_STATUS];
	for(u32 i = 0; i < nGamers; i++)
	{ 
		if(!CTheScripts::ImportGamerHandle(&ahGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_GET_GAMER_STATUS")))
		{
			scriptErrorf("%s:NETWORK_GET_GAMER_STATUS - Invalid ahGamers[%d]!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), i);
			return false;
		}

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	return CLiveManager::GetGamerStatus(ahGamers, nGamers);
}

bool CommandNetworkQueueGamerForStatus(int& handleData)
{
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_GAMER_STATUS")))
	{
		scriptErrorf("%s:NETWORK_QUEUE_GAMER_FOR_STATUS - Invalid handleData!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	return CLiveManager::QueueGamerForStatus(hGamer);
}

bool CommandNetworkGetGamerStatusFromQueue()
{
	return CLiveManager::GetGamerStatusFromQueue();
}

bool CommandNetworkIsGettingGamerStatus()
{
	return CLiveManager::IsGettingGamerStatus();
}

bool CommandNetworkDidGetGamerStatusSucceed()
{
	return CLiveManager::DidGetGamerStatusSucceed();
}

bool CommandNetworkGetGamerStatusResult(int& hData, int nIndex)
{
	eGamerState nState = CLiveManager::GetGamerStatusState(nIndex);
	if(nState == eGamerState_Invalid)
		return false;

	rlGamerHandle hGamer;
	CLiveManager::GetGamerStatusHandle(nIndex, &hGamer);
	if(!hGamer.IsValid())
	{
		scriptErrorf("%s:NETWORK_GET_GAMER_STATUS_RESULT - Invalid handleData!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// copy in handle
	hGamer.Export(pInfoBuffer, GAMER_HANDLE_SIZE);
	pInfoBuffer += GAMER_HANDLE_SIZE;

	// copy in gamer state
	sysMemCpy(pInfoBuffer, &nState, sizeof(scrValue));
	pInfoBuffer += sizeof(scrValue);

	// finished
	return true;
}

int CommandNetworkGetGamerStatusResults(int& hData, int sizeOfData)
{
	unsigned nNumGamers = CLiveManager::GetNumGamerStatusGamers();
	if(nNumGamers == 0)
		return 0;

	// grab states and handles
	eGamerState* pGamerStates = CLiveManager::GetGamerStatusStates();
	rlGamerHandle* pGamerHandles = CLiveManager::GetGamerStatusHandles();

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// script returns size in script words
	sizeOfData *= sizeof(scrValue); 

	// calculate size required
	int nRequiredSize = 0;
	nRequiredSize += sizeof(scrValue);										// array size prefix
	nRequiredSize += GAMER_HANDLE_SIZE * CLiveManager::MAX_GAMER_STATUS;	// gamer handle
	nRequiredSize += sizeof(scrValue) * CLiveManager::MAX_GAMER_STATUS;		// state

	// validate
	if(!gnetVerifyf(nRequiredSize == sizeOfData, "%s:NETWORK_GET_GAMER_STATUS_RESULTS - Invalid size!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return 0;

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in party members
	for(u32 i = 0; i < nNumGamers; i++)
	{
		// copy in handle
		pGamerHandles[i].Export(pInfoBuffer, GAMER_HANDLE_SIZE);
		pInfoBuffer += GAMER_HANDLE_SIZE;

		// copy in gamer state
		sysMemCpy(pInfoBuffer, &pGamerStates[i], sizeof(scrValue));
		pInfoBuffer += sizeof(scrValue);
	}

	// return value is number of members
	return nNumGamers;
}

void CommandNetworkClearGetGamerStatus()
{
	CLiveManager::ClearGetGamerStatus();
}

void CommandNetworkSessionJoinInvite()
{
	CNetwork::GetNetworkSession().JoinInvite();
}

void CommandNetworkSessionClearInvite()
{
    // if script have taken control, kill any wait action
    if(CNetwork::GetNetworkSession().IsWaitingForInvite())
    {
        scriptDebugf1("CommandNetworkSessionClearInvite");
        CNetwork::GetNetworkSession().ClearWaitFlag();
    }

	CLiveManager::GetInviteMgr().ClearInvite(InviteMgr::CF_FromScript);
}

void CommandNetworkSessionCancelInvite()
{
    // if script have taken control, kill any wait action
    if(CNetwork::GetNetworkSession().IsWaitingForInvite())
    {
        scriptDebugf1("CommandNetworkSessionCancelInvite");
        CNetwork::GetNetworkSession().ClearWaitFlag();
    }

    CLiveManager::GetInviteMgr().CancelInvite();
}

bool CommandNetworkHasPendingInvite()
{
	const bool bHasPendingInvite = CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() && !CLiveManager::GetInviteMgr().HasFailedAcceptedInvite();

#if !__NO_OUTPUT
	static bool s_bHasPendingInvite = false;
	if(s_bHasPendingInvite != bHasPendingInvite)
	{
		scriptDebugf1("Tracking - NETWORK_HAS_PENDING_INVITE - Now %s", bHasPendingInvite ? "True" : "False");
		s_bHasPendingInvite = bHasPendingInvite;
	}
#endif

	return bHasPendingInvite;
}

bool CommandNetworkGetPendingInviter(int &handleData)
{
    const rlGamerHandle& tInviter = CLiveManager::GetInviteMgr().GetAcceptedInvite()->GetInviter();
    if(scriptVerifyf(CLiveManager::GetInviteMgr().HasPendingAcceptedInvite(), "%s:NETWORK_GET_PENDING_INVITER - No pending invite!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        CTheScripts::ExportGamerHandle(&tInviter, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_PENDING_INVITER"));
        return true;
    }
    return false;
}

bool CommandNetworkHasConfirmedInvite()
{
	bool bHasConfirmedInvite = CLiveManager::GetInviteMgr().HasConfirmedAcceptedInvite();

#if !__NO_OUTPUT
	static bool s_bHasConfirmedInvite = false;
	if(s_bHasConfirmedInvite != bHasConfirmedInvite)
	{
		scriptDebugf1("Tracking - NETWORK_HAS_CONFIRMED_INVITE - Now %s", bHasConfirmedInvite ? "True" : "False");
		s_bHasConfirmedInvite = bHasConfirmedInvite;
	}
#endif

	return bHasConfirmedInvite;
}

bool CommandNetworkRequestInviteConfirmedEvent()
{
    return CLiveManager::GetInviteMgr().RequestConfirmEvent();
}

bool CommandNetworkSessionWasInvited()
{
	return CNetwork::GetNetworkSession().DidJoinViaInvite();
}

void CommandNetworkSessionGetInviter(int &handleData)
{
	const rlGamerHandle& tInviter = CNetwork::GetNetworkSession().GetInviter();
	if(scriptVerifyf(CNetwork::GetNetworkSession().DidJoinViaInvite(), "%s:NETWORK_SESSION_GET_INVITER - Didn't join session via invite!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		CTheScripts::ExportGamerHandle(&tInviter, handleData, SCRIPT_GAMER_HANDLE_SIZE  ASSERT_ONLY(, "NETWORK_SESSION_GET_INVITER"));
}

bool CommandNetworkIsAwaitingInviteResponse()
{
	bool bIsAwaitingInviteResponse = CLiveManager::GetInviteMgr().IsAwaitingInviteResponse();

#if !__NO_OUTPUT
	static bool s_bIsAwaitingInviteResponse = false;
	if(s_bIsAwaitingInviteResponse != bIsAwaitingInviteResponse)
	{
		scriptDebugf1("Tracking - NETWORK_SESSION_IS_AWAITING_INVITE_RESPONSE - Now %s", bIsAwaitingInviteResponse ? "True" : "False");
		s_bIsAwaitingInviteResponse = bIsAwaitingInviteResponse;
	}
#endif

	return bIsAwaitingInviteResponse;
}

bool CommandNetworkIsDisplayingInviteConfirmation()
{
	return CLiveManager::GetInviteMgr().IsDisplayingConfirmation();
}

void CommandNetworkSuppressInvites(bool bSuppress)
{
	CLiveManager::GetInviteMgr().SuppressProcess(bSuppress);
}

void CommandNetworkBlockInvites(bool bBlocked)
{
    CLiveManager::GetInviteMgr().BlockInvitesFromScript(bBlocked);
}

void CommandNetworkBlockJoinQueueInvites(bool bBlocked)
{
    CLiveManager::GetInviteMgr().BlockJoinQueueInvites(bBlocked);
}

void CommandNetworkSetCanReceiveRsInvites(const bool canReceive)
{
	CLiveManager::GetInviteMgr().SetCanReceiveRsInvites(canReceive);
}

void CommandNetworkStoreInviteThroughRestart()
{
    CLiveManager::GetInviteMgr().StoreInviteThroughRestart();
}

void CommandNetworkAllowInviteProcessInPlayerSwitch(bool bAllow)
{
    CLiveManager::GetInviteMgr().AllowProcessInPlayerSwitch(bAllow);
}

void CommandNetworkSetScriptReadyForEvents(bool bReady)
{
	CNetwork::SetScriptReadyForEvents(bReady);
}

bool CommandNetworkIsOfflineInvitePending()
{
    return CLiveManager::IsOfflineInvitePending();
}

void CommandNetworkClearOfflineInvitePending()
{
    CLiveManager::ClearOfflineInvitePending();
}

void CommandNetworkHostSinglePlayerSession(int nGameMode)
{
	// host a private session with 1 maximum player
    NetworkInterface::HostSession(nGameMode, 1, HostFlags::HF_IsPrivate | HostFlags::HF_SingleplayerOnly | HostFlags::HF_ViaScript);
}

void CommandNetworkLeaveSinglePlayerSession()
{
	CNetwork::GetNetworkSession().LeaveSession(0, LeaveSessionReason::Leave_Normal);
}

bool CommandNetworkIsGameInProgress()
{
	const bool isGameInProgress = NetworkInterface::IsGameInProgress();

#if !__NO_OUTPUT
	static bool s_LastValue = false;
	if(s_LastValue != isGameInProgress)
	{
		scriptDebugf3("NETWORK_IS_GAME_IN_PROGRESS called returning - \"%s\"", isGameInProgress ? "TRUE" : "FALSE");
		s_LastValue = isGameInProgress;
	}
#endif // !__NO_OUTPUT

	return isGameInProgress;
}

bool CommandNetworkIsSessionActive()
{
	return NetworkInterface::IsSessionActive(); 
}

bool CommandNetworkIsInSession()
{
	return NetworkInterface::IsInSession(); 
}

bool CommandNetworkIsSessionInLobby()
{
	// no longer a session flow construct
	return false; 
}

bool CommandNetworkIsSessionStarted()
{
	return NetworkInterface::IsSessionEstablished(); 
}

bool CommandNetworkIsSessionBusy()
{
	return NetworkInterface::IsSessionBusy(); 
}

bool CommandNetworkCanSessionStart()
{
	return NetworkInterface::CanSessionStart(); 
}

bool CommandNetworkCanSessionEnd()
{
	return NetworkInterface::CanSessionLeave(); 
}

bool CommandNetworkIsUnlocked()
{
	return !NetworkInterface::IsMultiplayerLocked(); 
}

int CommandNetworkGetGameMode()
{
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_GET_GAMEMODE - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return -1;

	return NetworkInterface::GetMatchConfig().GetGameMode();
}

void CommandNetworkMarkSessionVisible(const bool bIsVisible)
{
	if(!CNetwork::GetNetworkSession().CanChangeSessionAttributes())
		return;

	if(!scriptVerifyf(NetworkInterface::IsHost(), "%s:NETWORK_SESSION_MARK_VISIBLE - Not network host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().SetSessionVisibility(SessionType::ST_Physical, bIsVisible);
}

bool CommandNetworkIsSessionVisible()
{
	if(!CNetwork::GetNetworkSession().CanChangeSessionAttributes())
		return false;

    return CNetwork::GetNetworkSession().IsSessionVisible(SessionType::ST_Physical);
}

void CommandNetworkSetSessionVisibilityLock(bool bLockVisibility, bool bLockSetting)
{
	if(!CNetwork::GetNetworkSession().CanChangeSessionAttributes())
		return;

    CNetwork::GetNetworkSession().SetSessionVisibilityLock(SessionType::ST_Physical, bLockVisibility, bLockSetting);
}

bool CommandNetworkIsSessionVisibilityLocked()
{
    return CNetwork::GetNetworkSession().IsSessionVisibilityLocked(SessionType::ST_Physical);
}

void CommandNetworkMarkSessionBlockJoinRequests(const bool bBlockJoinRequests)
{
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_SESSION_BLOCK_JOIN_REQUESTS - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(NetworkInterface::IsHost(), "%s:NETWORK_SESSION_BLOCK_JOIN_REQUESTS - Not network host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().SetBlockJoinRequests(bBlockJoinRequests);
}

bool CommandNetworkIsSessionBlockingJoinRequests()
{
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_IS_SESSION_BLOCKING_JOIN_REQUESTS - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	if(!scriptVerifyf(NetworkInterface::IsHost(), "%s:NETWORK_IS_SESSION_BLOCKING_JOIN_REQUESTS - Not network host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	return CNetwork::GetNetworkSession().IsBlockingJoinRequests();
}

void CommandNetworkChangeSessionSlots(const int nPublicSlots, const int nPrivateSlots)
{
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_SESSION_CHANGE_SLOTS - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(NetworkInterface::IsHost(), "%s:NETWORK_SESSION_CHANGE_SLOTS - Not network host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().ChangeSessionSlots(static_cast<unsigned>(nPublicSlots), static_cast<unsigned>(nPrivateSlots));
}

int CommandNetworkGetMaxSlots()
{
	return NetworkInterface::GetMatchConfig().GetMaxSlots();
}

int CommandNetworkGetMaxPrivateSlots()
{
	return NetworkInterface::GetMatchConfig().GetMaxPrivateSlots();
}

bool CommandNetworkVoiceHostSession()
{
	return CNetwork::GetNetworkVoiceSession().Host();
}

bool CommandNetworkVoiceLeaveSession()
{
	return CNetwork::GetNetworkVoiceSession().Leave();
}

void CommandNetworkVoiceConnectToPlayer(int& hGamerData)
{
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, hGamerData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_VOICE_CONNECT_TO_PLAYER")))
		return;

	CNetwork::GetNetworkVoiceSession().InviteGamer(hGamer);
}

void CommandNetworkVoiceRespondToRequest(bool bAccept, int nResponseCode)
{
	CNetwork::GetNetworkVoiceSession().RespondToRequest(bAccept, nResponseCode);
}

void CommandNetworkSetVoiceSessionTimeout(int nTimeout)
{
	CNetwork::GetNetworkVoiceSession().SetTimeout(static_cast<unsigned>(nTimeout));
}

bool CommandNetworkIsInVoiceSession()
{
	return CNetwork::GetNetworkVoiceSession().IsInVoiceSession();
}

bool CommandNetworkIsVoiceSessionActive()
{
	return CNetwork::GetNetworkVoiceSession().IsVoiceSessionActive();
}

bool CommandNetworkIsVoiceSessionBusy()
{
	return CNetwork::GetNetworkVoiceSession().IsBusy();
}

bool CommandNetworkSendTextMessage(const char* szText, int& hData)
{
    // send to this gamer
    rlGamerHandle hGamer;
    if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_TEXT_MESSAGE")))
		CNetwork::GetVoice().SendTextMessage(szText, hGamer);

    // invalid rlGamerHandle
    return false;
}

void CommandSetActivitySpectator(bool bIsSpectator)
{
    CNetwork::GetNetworkSession().SetActivitySpectator(bIsSpectator);
}

bool CommandIsActivitySpectator()
{
    return CNetwork::GetNetworkSession().IsActivitySpectator();
}

void CommandSetActivityPlayerMax(int nPlayerMax)
{
	CNetwork::GetNetworkSession().SetActivityPlayersMax(static_cast<unsigned>(nPlayerMax));
}

void CommandSetActivitySpectatorMax(int nSpectatorMax)
{
    CNetwork::GetNetworkSession().SetActivitySpectatorsMax(static_cast<unsigned>(nSpectatorMax));
}

int CommandGetActivityPlayerMax(bool bSpectator)
{
    return static_cast<int>(CNetwork::GetNetworkSession().GetActivityPlayerMax(bSpectator));
}

int CommandGetActivityPlayerNum(bool bSpectator)
{
    return static_cast<int>(CNetwork::GetNetworkSession().GetActivityPlayerNum(bSpectator));
}

int CommandIsActivitySpectatorFromHandle(int& hData)
{
    rlGamerHandle hGamer;
    if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_ACTIVITY_SPECTATOR_FROM_HANDLE")))
        return CNetwork::GetNetworkSession().IsActivitySpectator(hGamer);

    // invalid rlGamerHandle
    return false;
}

bool CommandNetworkHostTransition(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityID, bool bIsPrivate, bool bIsOpen, bool bFromMatchmaking, int nActivityIsland, int nContentCreator, int nScriptHostFlags)
{
    // bail out if we can't access
    if(!scriptVerifyf(CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

	// build host flags
	unsigned nHostFlags = HostFlags::HF_ViaScript | static_cast<unsigned>(nScriptHostFlags);
	if(bIsPrivate)
		nHostFlags |= HostFlags::HF_IsPrivate;
	if(!bIsOpen)
		nHostFlags |= HostFlags::HF_NoMatchmaking;
	if(bFromMatchmaking)
		nHostFlags |= HostFlags::HF_ViaMatchmaking;

    return CNetwork::GetNetworkSession().HostTransition(nGameMode, nMaxPlayers, nActivityType, static_cast<unsigned>(nActivityID), nActivityIsland, static_cast<unsigned>(nContentCreator), nHostFlags);
}

bool CommandNetworkDoTransitionQuickmatch(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityID, int nMmFlags, int nActivityIsland)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

	return CNetwork::GetNetworkSession().DoTransitionQuickmatch(nGameMode, nMaxPlayers, nActivityType, static_cast<unsigned>(nActivityID), nActivityIsland, static_cast<unsigned>(nMmFlags));
}

bool CommandNetworkDoTransitionQuickmatchAsync(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityID, int nMmFlags, int nActivityIsland)
{
	// bail out if we can't access
	if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	const unsigned matchmakingFlags = static_cast<unsigned>(nMmFlags) | MatchmakingFlags::MMF_Asynchronous;	
	return CNetwork::GetNetworkSession().DoTransitionQuickmatch(nGameMode, nMaxPlayers, nActivityType, static_cast<unsigned>(nActivityID), nActivityIsland, matchmakingFlags);
}

bool CommandNetworkDoTransitionQuickmatchWithGroup(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityID, int& hGamerHandleData, int nGamers, int nMmFlags, int nActivityIsland)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

    rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned nValidGamers = 0;

    if(!scriptVerify(nGamers <= MAX_NUM_PHYSICAL_PLAYERS))
    {
        scriptErrorf("%s:NETWORK_DO_TRANSITION_QUICKMATCH_WITH_GROUP - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS, nGamers);
        return false;
    }

    // buffer to write our info into
    u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

    // skip array size prefix
    pInfoBuffer += sizeof(scrValue);

    // fill in gamer handles
    for(u32 i = 0; i < nGamers; i++)
    { 
        if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_DO_TRANSITION_QUICKMATCH_WITH_GROUP")))
            nValidGamers++;

        pInfoBuffer += GAMER_HANDLE_SIZE;
    }

    // run quickmatch
    return CNetwork::GetNetworkSession().DoTransitionQuickmatch(nGameMode, nMaxPlayers, nActivityType, static_cast<unsigned>(nActivityID), nActivityIsland, static_cast<unsigned>(nMmFlags), hGamers, nValidGamers);
}

bool CommandNetworkDoJoinGroupActivity()
{
    return CNetwork::GetNetworkSession().JoinGroupActivity();
}

void CommandNetworkDoClearGroupActivity()
{
	CNetwork::GetNetworkSession().ClearGroupActivity();
}

void CommandNetworkRetainActivityGroup()
{
    CNetwork::GetNetworkSession().RetainActivityGroupOnQuickmatchFail();
}

bool CommandNetworkHostClosedFriendTransition(int nGameMode, int nMaxPlayers, int nActivityType, int nActivityID, int nActivityIsland, int nContentCreator)
{
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_HOST_TRANSITION_FRIENDS_ONLY")))
		return false;

	return CNetwork::GetNetworkSession().HostTransition(nGameMode, nMaxPlayers, nActivityType, nActivityID, nActivityIsland, static_cast<unsigned>(nContentCreator), HostFlags::HF_ClosedFriends | HostFlags::HF_NoMatchmaking | HostFlags::HF_ViaScript);
}

bool CommandNetworkHostClosedCrewTransition(int nGameMode, int nMaxPlayers, int nUniqueCrewLimit, int nCrewLimitMaxMembers, int nActivityType, int nActivityID, int nActivityIsland, int nContentCreator)
{
	// validate access
	if(!CheckMultiplayerAccess(OUTPUT_ONLY("NETWORK_HOST_TRANSITION_CLOSED_CREW")))
		return false;

	// apply if non-zero
    if(nUniqueCrewLimit > 0)
        CNetwork::GetNetworkSession().SetUniqueCrewLimit(SessionType::ST_Transition, nUniqueCrewLimit);
    if(nCrewLimitMaxMembers > 0)
        CNetwork::GetNetworkSession().SetCrewLimitMaxMembers(SessionType::ST_Transition, nCrewLimitMaxMembers);

	return CNetwork::GetNetworkSession().HostTransition(nGameMode, nMaxPlayers, nActivityType, nActivityID, nActivityIsland, static_cast<unsigned>(nContentCreator), HostFlags::HF_ClosedCrew | HostFlags::HF_NoMatchmaking | HostFlags::HF_ViaScript);
}

bool CommandNetworkIsClosedFriendsTransition()
{
	return CNetwork::GetNetworkSession().IsClosedFriendsSession(SessionType::ST_Transition);
}

bool CommandNetworkIsClosedCrewTransition()
{
	return CNetwork::GetNetworkSession().IsClosedCrewSession(SessionType::ST_Transition);
}

bool CommandNetworkIsSoloTransition()
{
	return CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Transition);
}

bool CommandNetworkIsPrivateTransition()
{
	return CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Transition);
}

int CommandNetworkGetNumTransitionNonAsyncGamers()
{
	return static_cast<int>(CNetwork::GetNetworkSession().GetNumTransitionNonAsyncGamers());
}

void CommandNetworkMarkAsPreferredActivity(bool bIsPreferred)
{
	CNetwork::GetNetworkSession().MarkAsPreferredActivity(bIsPreferred);
}

void CommandNetworkMarkAsWaitingAsync(bool bIsWaitingAsync)
{
	CNetwork::GetNetworkSession().MarkAsWaitingAsync(bIsWaitingAsync);
}

void CommandNetworkSetInPogressFinishTime(int nInProgressFinishTime)
{
	CNetwork::GetNetworkSession().SetInProgressFinishTime(static_cast<u32>(nInProgressFinishTime));
}

void CommandNetworkSetActivityCreatorHandle(int& hData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_TRANSITION_CREATOR_HANDLE")))
		return;

	return CNetwork::GetNetworkSession().SetActivityCreatorHandle(hGamer);
}

void CommandNetworkClearActivityCreatorHandle()
{
	rlGamerHandle hGamer; 
	hGamer.Clear();
	CNetwork::GetNetworkSession().SetActivityCreatorHandle(hGamer);
}

bool CommandNetworkInviteGamerToTransition(int& hData)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
		return 0;

	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_INVITE_GAMER_TO_TRANSITION")))
		return CNetwork::GetNetworkSession().SendTransitionInvites(&hGamer, 1);

	// invalid rlGamerHandle
	return false;
}

bool CommandNetworkInviteGamersToTransition(int& hData, int nGamers)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
		return 0;

	if(!scriptVerify(nGamers <= RL_MAX_GAMERS_PER_SESSION))
	{
		scriptErrorf("%s:NETWORK_INVITE_GAMERS_TO_TRANSITION - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), RL_MAX_GAMERS_PER_SESSION, nGamers);
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// grab 
	rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION];
	for(u32 i = 0; i < nGamers; i++)
	{ 
		if(!CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_INVITE_GAMERS_TO_TRANSITION")))
			return false;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	return CNetwork::GetNetworkSession().SendTransitionInvites(hGamers, nGamers);
}

void CommandNetworkSetGamerInvitedToTransition(int& hData)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
		return;

	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_GAMER_INVITED_TO_TRANSITION")))
		CNetwork::GetNetworkSession().AddTransitionInvites(&hGamer, 1);
}

void CommandNetworkSetGamersInvitedToTransition(int& hData, int nGamers)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
		return;

	if(!scriptVerifyf(nGamers < RL_MAX_GAMERS_PER_SESSION, "%s:NETWORK_INVITE_GAMERS_TO_TRANSITION - Inviting too many gamers!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// grab 
	rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION];
	for(u32 i = 0; i < nGamers; i++)
	{ 
		if(!CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_INVITE_GAMERS_TO_TRANSITION")))
			return;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	CNetwork::GetNetworkSession().AddTransitionInvites(hGamers, nGamers);
}

bool CommandNetworkLeaveTransition()
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionEstablished(), "%s:NETWORK_LEAVE_TRANSITION - Not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	return CNetwork::GetNetworkSession().LeaveTransition();
}

bool CommandNetworkLaunchTransition()
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionHost(), "%s:NETWORK_LAUNCH_TRANSITION - Not transition host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false; 

	return CNetwork::GetNetworkSession().LaunchTransition();
}

void CommandSetDoNotLaunchFromJoinAsMigratedHost(bool bLaunch)
{
	CNetwork::GetNetworkSession().SetDoNotLaunchFromJoinAsMigratedHost(bLaunch);
}

void CommandNetworkCancelTransitionMatchmaking()
{
	scriptDebugf1("%s:NETWORK_CANCEL_TRANSITION_MATCHMAKING", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CNetwork::GetNetworkSession().CancelTransitionMatchmaking();
}

void CommandNetworkBailTransition(int nContextParam1, int nContextParam2, int nContextParam3)
{
	// log script counter and all params
	scriptDebugf1("%s:NETWORK_BAIL_TRANSITION - Params: %d, %d, %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContextParam1, nContextParam2, nContextParam3);
		
	// no pre-requisites - destroy everything (and put it back to together again nicely)
    CNetwork::GetNetworkSession().BailTransition(BAIL_TRANSITION_SCRIPT, nContextParam1, nContextParam2, nContextParam3);
}

void CommandNetworkBailAllTransition(int nContextParam1, int nContextParam2, int nContextParam3)
{
	// cancel matchmaking and bail any active transition
	scriptDebugf1("%s:NETWORK_BAIL_ALL_TRANSITION - Params: %d, %d, %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContextParam1, nContextParam2, nContextParam3);
	CNetwork::GetNetworkSession().CancelTransitionMatchmaking();
	CNetwork::GetNetworkSession().BailTransition(BAIL_TRANSITION_SCRIPT, nContextParam1, nContextParam2, nContextParam3);
}

bool CommandNetworkDoTransitionToGame(bool bWithPlayers, int nMaxPlayers)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

    rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned nGamers = 0;

    if(bWithPlayers && CNetwork::GetNetworkSession().IsHost())
    {
        rlGamerInfo aGamerInfo[MAX_NUM_PHYSICAL_PLAYERS];
        nGamers = CNetwork::GetNetworkSession().GetSessionMembers(aGamerInfo, MAX_NUM_PHYSICAL_PLAYERS);

        for(unsigned i = 0; i < nGamers; i++)
            hGamers[i] = aGamerInfo[i].GetGamerHandle();
    }

	return CNetwork::GetNetworkSession().DoTransitionToGame(MultiplayerGameMode::GameMode_Freeroam, hGamers, nGamers, false, nMaxPlayers, 0);
}

bool CommandNetworkDoTransitionToNewGame(bool bWithPlayers, int nMaxPlayers, bool bIsPrivate)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

    rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned nGamers = 0;

    if(bWithPlayers)
    {
        if(CNetwork::GetNetworkSession().IsHost())
        {
            rlGamerInfo aGamerInfo[MAX_NUM_PHYSICAL_PLAYERS];
            nGamers = CNetwork::GetNetworkSession().GetSessionMembers(aGamerInfo, MAX_NUM_PHYSICAL_PLAYERS);

            for(unsigned i = 0; i < nGamers; i++)
                hGamers[i] = aGamerInfo[i].GetGamerHandle();
        }
        else
            return false;
    }

	const unsigned nHostFlags = (bIsPrivate ? HostFlags::HF_IsPrivate : HostFlags::HF_Default) | HostFlags::HF_ViaScriptToGame;
    return CNetwork::GetNetworkSession().DoTransitionToNewGame(MultiplayerGameMode::GameMode_Freeroam, hGamers, nGamers, nMaxPlayers, nHostFlags, true);
}

bool CommandNetworkCanTransitionToGame()
{
    return CNetwork::GetNetworkSession().CanTransitionToGame();
}

bool CommandNetworkDoTransitionToFreemode(int& hGamerHandleData, int nGamers, bool bSocialMatchmaking, int nMaxPlayers, int nMmFlags)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

    rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned nValidGamers = 0;

    if(!scriptVerify(nGamers <= MAX_NUM_PHYSICAL_PLAYERS))
    {
        scriptErrorf("%s:NETWORK_DO_TRANSITION_TO_FREEMODE - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS, nGamers);
        return false;
    }

    // buffer to write our info into
    u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

    // skip array size prefix
    pInfoBuffer += sizeof(scrValue);

    // fill in gamer handles
    for(u32 i = 0; i < nGamers; i++)
    { 
        if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_DO_TRANSITION_TO_FREEMODE")))
            nValidGamers++;

        pInfoBuffer += GAMER_HANDLE_SIZE;
    }

    return CNetwork::GetNetworkSession().DoTransitionToGame(MultiplayerGameMode::GameMode_Freeroam, hGamers, nValidGamers, bSocialMatchmaking, nMaxPlayers, static_cast<unsigned>(nMmFlags));
}

bool CommandNetworkDoTransitionToNewFreemode(int& hGamerHandleData, int nGamers, int nMaxPlayers, bool bIsPrivate, bool bAllowPreviousJoin, int nHostFlags)
{
    // bail out if we can't access
    if(!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        return false;

    rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned nValidGamers = 0;

    if(!scriptVerify(nGamers <= MAX_NUM_PHYSICAL_PLAYERS))
    {
        scriptErrorf("%s:NETWORK_DO_TRANSITION_TO_NEW_FREEMODE - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS, nGamers);
        return false;
    }

    // buffer to write our info into
    u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

    // skip array size prefix
    pInfoBuffer += sizeof(scrValue);

    // fill in gamer handles
    for(u32 i = 0; i < nGamers; i++)
    { 
        if(CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_DO_TRANSITION_TO_NEW_FREEMODE")))
            nValidGamers++;

        pInfoBuffer += GAMER_HANDLE_SIZE;
    }

	nHostFlags |= (bIsPrivate ? HostFlags::HF_IsPrivate : HostFlags::HF_Default) | HostFlags::HF_ViaScriptToGame;
	return CNetwork::GetNetworkSession().DoTransitionToNewGame(MultiplayerGameMode::GameMode_Freeroam, hGamers, nValidGamers, nMaxPlayers, static_cast<unsigned>(nHostFlags), bAllowPreviousJoin);
}

bool CommandNetworkDoTransitionFromActivity(int nGameMode, int& hGamerHandleData, int nGamers, bool bSocialMatchmaking, int nMaxPlayers, int nMmFlags)
{
	// bail out if we can't access
	if (!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
	unsigned nValidGamers = 0;

	if (!scriptVerify(nGamers <= MAX_NUM_PHYSICAL_PLAYERS))
	{
		scriptErrorf("%s:NETWORK_DO_TRANSITION_FROM_ACTIVITY - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS, nGamers);
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in gamer handles
	for (u32 i = 0; i < nGamers; i++)
	{
		if (CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_DO_TRANSITION_FROM_ACTIVITY")))
			nValidGamers++;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	return CNetwork::GetNetworkSession().DoTransitionToGame(nGameMode, hGamers, nValidGamers, bSocialMatchmaking, nMaxPlayers, static_cast<unsigned>(nMmFlags));
}

bool CommandNetworkDoTransitionFromActivityToNewSession(int nGameMode, int& hGamerHandleData, int nGamers, int nMaxPlayers, bool bIsPrivate, bool bAllowPreviousJoin, int nHostFlags)
{
	// bail out if we can't access
	if (!scriptVerifyf(NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_MultiplayerEnter), "%s : Cannot access multiplayer!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	rlGamerHandle hGamers[MAX_NUM_PHYSICAL_PLAYERS];
	unsigned nValidGamers = 0;

	if (!scriptVerify(nGamers <= MAX_NUM_PHYSICAL_PLAYERS))
	{
		scriptErrorf("%s:NETWORK_DO_TRANSITION_FROM_ACTIVITY_TO_NEW_SESSION - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS, nGamers);
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hGamerHandleData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in gamer handles
	for (u32 i = 0; i < nGamers; i++)
	{
		if (CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_DO_TRANSITION_FROM_ACTIVITY_TO_NEW_SESSION")))
			nValidGamers++;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	nHostFlags |= (bIsPrivate ? HostFlags::HF_IsPrivate : HostFlags::HF_Default) | HostFlags::HF_ViaScriptToGame;;
	return CNetwork::GetNetworkSession().DoTransitionToNewGame(nGameMode, hGamers, nValidGamers, nMaxPlayers, static_cast<unsigned>(nHostFlags), bAllowPreviousJoin);
}

bool CommandNetworkIsTransitionLaunching()
{
	return CNetwork::GetNetworkSession().IsLaunchingTransition();
}

bool CommandNetworkIsTransitionToGame()
{
	return CNetwork::GetNetworkSession().IsInTransitionToGame();
}

int CommandNetworkGetTransitionMembers(int& hData, int nMaxMembers)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
		return 0;

	if(nMaxMembers == 0)
		return 0;

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&hData);

	// grab transition member info
	rlGamerInfo aMemberInfo[RL_MAX_GAMERS_PER_SESSION];
	u32 nMembers = CNetwork::GetNetworkSession().GetTransitionMembers(aMemberInfo, nMaxMembers);

	// copy in member count
	sysMemCpy(pInfoBuffer, &nMembers, sizeof(scrValue));
	pInfoBuffer += sizeof(scrValue);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in transition members
	for(u32 i = 0; i < nMembers; i++)
	{
		// copy in handle
		aMemberInfo[i].GetGamerHandle().Export(pInfoBuffer, GAMER_HANDLE_SIZE);
		pInfoBuffer += GAMER_HANDLE_SIZE;

		// copy in name
		safecpy(reinterpret_cast<char*>(pInfoBuffer), aMemberInfo[i].GetDisplayName(), sizeof(scrTextLabel63));
		pInfoBuffer += sizeof(scrTextLabel63);
	}

	// return value is number of members
	return static_cast<int>(nMembers);
}

void CommandNetworkApplyTransitionParameter(int nID, int nValue)
{
	CNetwork::GetNetworkSession().ApplyTransitionParameter(static_cast<u16>(nID), static_cast<u32>(nValue), false);
}

void CommandNetworkApplyTransitionParameterString(int nIndex, const char* szParameter, bool bForceDirty)
{
	CNetwork::GetNetworkSession().ApplyTransitionParameter(nIndex, szParameter, bForceDirty);
}

bool CommandNetworkSendTransitionGamerInstruction(int& hData, const char* szGamerName, int nInstruction, int nInstructionParam, bool bBroadcast)
{
	if(!CNetwork::GetNetworkSession().IsTransitionActive())
	{
		scriptErrorf("%s:NETWORK_SEND_TRANSITION_GAMER_INSTRUCTION - Not in transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_TRANSITION_GAMER_INSTRUCTION")))
		return CNetwork::GetNetworkSession().SendTransitionGamerInstruction(hGamer, szGamerName, nInstruction, nInstructionParam, bBroadcast);

	return false;
}

bool CommandNetworkMarkTransitionPlayerAsFullyJoined(int& hData)
{
    if(!CNetwork::GetNetworkSession().IsTransitionActive())
        return false;

    rlGamerHandle hGamer;
    if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_MARK_TRANSITION_GAMER_AS_FULLY_JOINED")))
        return CNetwork::GetNetworkSession().MarkTransitionPlayerAsFullyJoined(hGamer);

    return false;
}

bool CommandNetworkIsTransitionHost()
{
	return CNetwork::GetNetworkSession().IsTransitionHost();
}

bool CommandNetworkIsTransitionHostFromHandle(int& hData)
{
    rlGamerHandle hGamer;
    if(!CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_TRANSITION_HOST_FROM_HANDLE")))
        return false;

    rlGamerHandle hHostGamer;
    if(!CNetwork::GetNetworkSession().GetTransitionHost(&hHostGamer))
        return false;

    return hGamer == hHostGamer;
}

bool CommandNetworkGetTransitionHost(int& hData)
{
    rlGamerHandle hHostGamer;
    if(!CNetwork::GetNetworkSession().GetTransitionHost(&hHostGamer))
        return false;

    return CTheScripts::ExportGamerHandle(&hHostGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_TRANSITION_HOST"));
}

bool CommandNetworkIsTransitionActive()
{
	return CNetwork::GetNetworkSession().IsTransitionActive();
}

bool CommandNetworkIsTransitionEstablished()
{
	return CNetwork::GetNetworkSession().IsTransitionEstablished();
}

bool CommandNetworkIsTransitionBusy()
{
	return CNetwork::GetNetworkSession().IsTransitionStatusPending();
}

bool CommandNetworkIsTransitionMatchMaking()
{
    return CNetwork::GetNetworkSession().IsTransitionMatchmaking();
}

bool CommandNetworkIsTransitionLeavePostponed()
{
	return CNetwork::GetNetworkSession().IsTransitionLeavePostponed();
}

void CommandNetworkSetMainInProgress(bool bInProgress)
{
	CNetwork::GetNetworkSession().SetSessionInProgress(SessionType::ST_Physical, bInProgress);
}

bool CommandNetworkGetMainInProgress()
{
	return CNetwork::GetNetworkSession().GetSessionInProgress(SessionType::ST_Physical);
}

void CommandNetworkSetActivityInProgress(bool bInProgress)
{
	CNetwork::GetNetworkSession().SetSessionInProgress(SessionType::ST_Transition, bInProgress);
}

bool CommandNetworkGetActivityInProgress()
{
	return CNetwork::GetNetworkSession().GetSessionInProgress(SessionType::ST_Transition);
}

void CommandNetworkSetMainContentCreator(int nContentCreator)
{
	CNetwork::GetNetworkSession().SetContentCreator(SessionType::ST_Physical, nContentCreator);
}

int CommandNetworkGetMainContentCreator()
{
	return static_cast<int>(CNetwork::GetNetworkSession().GetContentCreator(SessionType::ST_Physical));
}

void CommandNetworkSetTransitionContentCreator(int nContentCreator)
{
	CNetwork::GetNetworkSession().SetContentCreator(SessionType::ST_Transition, nContentCreator);
}

int CommandNetworkGetTransitionContentCreator()
{
	return static_cast<int>(CNetwork::GetNetworkSession().GetContentCreator(SessionType::ST_Transition));
}

void CommandNetworkSetMainActivityIsland(int nActivityIsland)
{
	CNetwork::GetNetworkSession().SetActivityIsland(SessionType::ST_Physical, nActivityIsland);
}

int CommandNetworkGetMainActivityIsland()
{
	return CNetwork::GetNetworkSession().GetActivityIsland(SessionType::ST_Physical);
}

void CommandNetworkSetTransitionActivityIsland(bool nActivityIsland)
{
	CNetwork::GetNetworkSession().SetActivityIsland(SessionType::ST_Transition, nActivityIsland);
}

int CommandNetworkGetTransitionActivityIsland()
{
	return CNetwork::GetNetworkSession().GetActivityIsland(SessionType::ST_Transition);
}

void CommandNetworkOpenTransitionMatchmaking()
{
	CNetwork::GetNetworkSession().SetSessionVisibility(SessionType::ST_Transition, true);
}

void CommandNetworkCloseTransitionMatchmaking()
{
	CNetwork::GetNetworkSession().SetSessionVisibility(SessionType::ST_Transition, false);
}

bool CommandNetworkIsTransitionOpenToMatchmaking()
{
	return CNetwork::GetNetworkSession().IsSessionVisible(SessionType::ST_Transition);
}

void CommandNetworkSetTransitionVisibilityLock(bool bLockVisibility, bool bLockSetting)
{
    CNetwork::GetNetworkSession().SetSessionVisibilityLock(SessionType::ST_Transition, bLockVisibility, bLockSetting);
}

bool CommandNetworkIsTransitionVisibilityLocked()
{
    return CNetwork::GetNetworkSession().IsSessionVisibilityLocked(SessionType::ST_Transition);
}

void CommandNetworkSetTransitionActivityType(int nActivityType)
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionActive(), "%s:NETWORK_SET_TRANSITION_ACTIVITY_TYPE - Not in a transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionHost(), "%s:NETWORK_SET_TRANSITION_ACTIVITY_TYPE - Not transition host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().SetTransitionActivityType(nActivityType);
}

void CommandNetworkSetTransitionActivityID(int nActivityID)
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionActive(), "%s:NETWORK_SET_TRANSITION_ACTIVITY_ID - Not in a transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionHost(), "%s:NETWORK_SET_TRANSITION_ACTIVITY_ID - Not transition host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().SetTransitionActivityID(static_cast<unsigned>(nActivityID));
}

void CommandNetworkChangeTransitionSlots(const int nPublicSlots, const int nPrivateSlots)
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionActive(), "%s:NETWORK_CHANGE_TRANSITION_SLOTS - Not in a transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionHost(), "%s:NETWORK_CHANGE_TRANSITION_SLOTS - Not transition host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().ChangeTransitionSlots(static_cast<unsigned>(nPublicSlots), static_cast<unsigned>(nPrivateSlots));
}

int CommandNetworkTransitionGetMaxSlots()
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionActive(), "%s:NETWORK_TRANSITION_GET_MAXIMUM_SLOTS - Not in a transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return 0;

	return static_cast<int>(CNetwork::GetNetworkSession().GetTransitionMaximumNumPlayers());
}

void CommandNetworkMarkTransitionBlockJoinRequests(const bool bBlockJoinRequests)
{
	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionActive(), "%s:NETWORK_TRANSITION_BLOCK_JOIN_REQUESTS - Not in a transition session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(CNetwork::GetNetworkSession().IsTransitionHost(), "%s:NETWORK_TRANSITION_BLOCK_JOIN_REQUESTS - Not transition host!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetwork::GetNetworkSession().SetBlockTransitionJoinRequests(bBlockJoinRequests);
}

bool CommandNetworkHasPlayerStartedTransition(int nPlayerIndex)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_HAS_PLAYER_STARTED_TRANSITION - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	if(!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_HAS_PLAYER_STARTED_TRANSITION", nPlayerIndex))
		return false;

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(!scriptVerifyf(pPlayer, "%s:NETWORK_HAS_PLAYER_STARTED_TRANSITION - Player %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
		return false;

	return pPlayer->HasStartedTransition();
}

bool CommandNetworkAreTransitionDetailsValid(int nPlayerIndex)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		scriptErrorf("%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	if(!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_ARE_TRANSITION_DETAILS_VALID", nPlayerIndex))
	{
		scriptErrorf("%s:SCRIPT_VERIFY_PLAYER_INDEX - Invalid player index of %d!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex);
		return false;
	}

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(!scriptVerifyf(pPlayer, "%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Player %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
	{
		scriptErrorf("%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Player %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex);
		return false;
	}

	if(!scriptVerifyf(pPlayer->HasStartedTransition(), "%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Player %s has not started transition!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPlayer->GetLogName()))
	{
		scriptErrorf("%s:NETWORK_ARE_TRANSITION_DETAILS_VALID - Player %s has not started transition!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPlayer->GetLogName());
		return false;
	}

	const rlSessionInfo& hInfo = pPlayer->GetTransitionSessionInfo();
	return hInfo.IsValid();
}

bool CommandNetworkJoinTransition(int nPlayerIndex)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_JOIN_TRANSITION - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	if(!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_JOIN_TRANSITION", nPlayerIndex))
		return false;

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(!scriptVerifyf(pPlayer, "%s:NETWORK_JOIN_TRANSITION - Player %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
		return false;

	// join up
	return CNetwork::GetNetworkSession().JoinPlayerTransition(pPlayer);
}

bool CommandNetworkSessionWasInvitedToTransition()
{
	return CNetwork::GetNetworkSession().DidJoinTransitionViaInvite();
}

void CommandNetworkSessionGetTransitionInviter(int &handleData)
{
	const rlGamerHandle& tInviter = CNetwork::GetNetworkSession().GetTransitionInviter();
	if(scriptVerifyf(CNetwork::GetNetworkSession().DidJoinTransitionViaInvite(), "%s:NETWORK_SESSION_GET_TRANSITION_INVITER - Didn't join transition via invite!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		CTheScripts::ExportGamerHandle(&tInviter, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SESSION_GET_TRANSITION_INVITER"));
}

bool CommandNetworkHasInvitedGamerToTransition(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_INVITED_GAMER_TO_TRANSITION")))
		return false;

	// find player in invited list
	return CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().DidInvite(hGamer);
}

bool CommandNetworkHasTransitionInviteBeenAcked(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_TRANSITION_INVITE_BEEN_ACKED")))
		return false;

	// find player in invited list
	return CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().IsAcked(hGamer);
}

bool CommandNetworkIsActivitySession()
{
	if (CNetwork::IsNetworkSessionValid())
	{
		return CNetwork::GetNetworkSession().IsActivitySession();
	}
	
	return false;
}

bool CommandNetworkMarkAsGameSession()
{
	return CNetwork::GetNetworkSession().MarkAsGameSession();
}

void CommandNetworkDisableRealtimeMultiplayer()
{
#if RSG_PROSPERO
	CLiveManager::SetRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_Disable);
#endif
}

void CommandNetworkOverrideRealtimeMultiplayerControlDisable()
{
#if RSG_PROSPERO
	CLiveManager::SetRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_PlayerOverride);
#endif
}

void CommandNetworkDisableRealtimeMultiplayerSpectator()
{
#if RSG_PROSPERO
	CLiveManager::SetRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride);
#endif
}

void CommandNetworkSetPresenceSessionInvitesBlocked(bool bBlocked)
{
    CNetwork::GetNetworkSession().SetPresenceInvitesBlocked(SessionType::ST_Physical, bBlocked);
}

bool CommandNetworkArePresenceSessionInvitesBlocked()
{
	return CNetwork::GetNetworkSession().IsJoinViaPresenceBlocked(SessionType::ST_Physical);
}

bool CommandNetworkSendPresenceInvite(int& handleData, const char* szContentId, int nPlaylistLength, int nPlaylistCurrent)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_PRESENCE_INVITE")))
		return false;

	return CNetwork::GetNetworkSession().AddDelayedPresenceInvite(hGamer, false, false, szContentId, "", 0, 0, static_cast<unsigned>(nPlaylistLength), static_cast<unsigned>(nPlaylistCurrent));
}

bool CommandNetworkSendPresenceTransitionInvite(int& handleData, const char* szContentId, int nPlaylistLength, int nPlaylistCurrent)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_PRESENCE_TRANSITION_INVITE")))
		return false;

	return CNetwork::GetNetworkSession().AddDelayedPresenceInvite(hGamer, false, true, szContentId, "", 0, 0, static_cast<unsigned>(nPlaylistLength), static_cast<unsigned>(nPlaylistCurrent));
}

bool CommandNetworkSendImportantPresenceTransitionInvite(int& handleData, const char* szContentId, int nPlaylistLength, int nPlaylistCurrent)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_PRESENCE_TRANSITION_INVITE")))
		return false;

	return CNetwork::GetNetworkSession().AddDelayedPresenceInvite(hGamer, true, true, szContentId, "", 0, 0, static_cast<unsigned>(nPlaylistLength), static_cast<unsigned>(nPlaylistCurrent));
}

bool CommandNetworkSendRockstarInvite(int& handleData, const int nFlags, const char* szContentId, const char* szUniqueMatchId, int nInviteFrom, int nInviteType, int nPlaylistLength, int nPlaylistCurrent)
{
	// make sure that handle is valid
	rlGamerHandle hGamer;
	if (!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SEND_ROCKSTAR_INVITE")))
		return false;

	enum
	{
		SEND_INVITE_FLAG_TRANSITION = BIT0,
		SEND_INVITE_FLAG_IMPORTANT = BIT1,
	};

	return CNetwork::GetNetworkSession().AddDelayedPresenceInvite(
		hGamer, 
		(static_cast<unsigned>(nFlags) & SEND_INVITE_FLAG_IMPORTANT) != 0,
		(static_cast<unsigned>(nFlags) & SEND_INVITE_FLAG_TRANSITION) != 0,
		szContentId,
		szUniqueMatchId, 
		nInviteFrom, 
		nInviteType, 
		static_cast<unsigned>(nPlaylistLength), 
		static_cast<unsigned>(nPlaylistCurrent));
}

int CommandNetworkGetRsInviteIndexByInviteId(int nInviteId)
{
	return CLiveManager::GetInviteMgr().GetRsInviteIndexByInviteId(static_cast<InviteId>(nInviteId));
}

int CommandNetworkGetNumRsInvites()
{
	return static_cast<int>(CLiveManager::GetInviteMgr().GetNumRsInvites());
}

bool CommandNetworkAcceptRsInvite(int nInviteIndex)
{
	return CLiveManager::GetInviteMgr().AcceptRsInvite(nInviteIndex);
}

bool CommandNetworkRemoveRsInvite(int nInviteIndex)
{
	return CLiveManager::GetInviteMgr().RemoveRsInvite(nInviteIndex, InviteMgr::RemoveReason::RR_Script);
}

const char* CommandNetworkGetRsInviteInviter(int nInviteIndex)
{
	return CLiveManager::GetInviteMgr().GetRsInviteInviter(nInviteIndex);
}

const int CommandNetworkGetRsInviteInviterCrewId(int nInviteIndex)
{
	return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteInviterCrewId(nInviteIndex));
}

int CommandNetworkGetRsInviteId(int nInviteIndex)
{
    return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteId(nInviteIndex));
}

bool CommandNetworkGetRsInviteHandle(int nInviteIndex, int& hData)
{
	rlGamerHandle hInviter;
	CLiveManager::GetInviteMgr().GetRsInviteHandle(nInviteIndex, &hInviter);
	return CTheScripts::ExportGamerHandle(&hInviter, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_PRESENCE_INVITE_HANDLE"));
}

int CommandNetworkGetRsInviteSessionId(int nInviteIndex)
{
    rlSessionInfo hInfo;
	CLiveManager::GetInviteMgr().GetRsInviteSessionInfo(nInviteIndex, &hInfo);
    if(hInfo.IsValid())
    {
        char szToken[64];
        snprintf(szToken, 64, "%" I64FMT "d", hInfo.GetToken().m_Value);
        return static_cast<int>(atDataHash(szToken, strlen(szToken), 0));
    }

    return 0;
}

const char* CommandNetworkGetRsInviteContentId(int nInviteIndex)
{
	return CLiveManager::GetInviteMgr().GetRsInviteContentId(nInviteIndex);
}

int CommandNetworkGetRsInvitePlaylistLength(int nInviteIndex)
{
	return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInvitePlaylistLength(nInviteIndex));
}

int CommandNetworkGetRsInvitePlaylistCurrent(int nInviteIndex)
{
	return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInvitePlaylistCurrent(nInviteIndex));
}

int CommandNetworkGetRsInviteTournamentEventId(int nInviteIndex)
{
    return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteTournamentEventId(nInviteIndex));
}

bool CommandNetworkGetRsInviteScTv(int nInviteIndex)
{
    return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteScTv(nInviteIndex));
}

bool CommandNetworkGetRsInviteFromAdmin(int nInviteIndex)
{
    return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteFromAdmin(nInviteIndex));
}

bool CommandNetworkGetRsInviteTournament(int nInviteIndex)
{
    return static_cast<int>(CLiveManager::GetInviteMgr().GetRsInviteIsTournament(nInviteIndex));
}

bool CommandNetworkHasFollowInvite()
{
    return CLiveManager::GetInviteMgr().HasValidFollowInvite();
}

bool CommandNetworkActionFollowInvite()
{
    return CLiveManager::GetInviteMgr().ActionFollowInvite();
}

bool CommandNetworkClearFollowInvite()
{
    return CLiveManager::GetInviteMgr().ClearFollowInvite();
}

bool CommandNetworkGetFollowInviteHandle(int& hData)
{
    if(!CLiveManager::GetInviteMgr().HasValidFollowInvite())
        return false;

    rlGamerHandle hInviter = CLiveManager::GetInviteMgr().GetFollowInviteHandle();
    return CTheScripts::ExportGamerHandle(&hInviter, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_FOLLOW_INVITE_HANDLE"));
}

bool CommandNetworkCancelInvite(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CANCEL_INVITE")))
		return false;

	return CNetwork::GetNetworkSession().CancelInvites(&hGamer, 1);
}

bool CommandNetworkCancelTransitionInvite(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CANCEL_TRANSITION_INVITE")))
		return false;

	return CNetwork::GetNetworkSession().CancelTransitionInvites(&hGamer, 1);
}

void CommandNetworkRemoveInvite(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_REMOVE_INVITE")))
		return;

	CNetwork::GetNetworkSession().RemoveInvites(&hGamer, 1);
}

void CommandNetworkRemoveAllInvites()
{
    CNetwork::GetNetworkSession().RemoveAllInvites();
}

void CommandNetworkRemoveAndCancelAllInvites()
{
    CNetwork::GetNetworkSession().RemoveAndCancelAllInvites();
}

void CommandNetworkRemoveTransitionInvite(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_REMOVE_TRANSITION_INVITE")))
		return;

	CNetwork::GetNetworkSession().RemoveTransitionInvites(&hGamer, 1);
}

void CommandNetworkRemoveAllTransitionInvites()
{
	CNetwork::GetNetworkSession().RemoveAllTransitionInvites();
}

void CommandNetworkRemoveAndCancelAllTransitionInvites()
{
    CNetwork::GetNetworkSession().RemoveAndCancelAllTransitionInvites();
}

bool CommandNetworkIsSessionInvitable()
{
	// check that we're in a valid session
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_SESSION_IS_INVITABLE - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	return CNetwork::GetNetworkSession().IsInvitable();
}

bool CommandNetworkInviteGamerToSession(int& handleData)
{
	// check that we're in a valid session
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_INVITE_GAMER - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_INVITE_GAMER")))
		return false;

	// send invite
	return CNetwork::GetNetworkSession().SendInvites(&hGamer, 1);
}

bool CommandNetworkInviteGamerToSessionWithMessage(int& handleData, const char* szSubject, const char* szMessage)
{
	// check that we're in a valid session
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_INVITE_GAMER - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_INVITE_GAMER")))
		return false;

	// send invite
	return CNetwork::GetNetworkSession().SendInvites(&hGamer, 1, szSubject, szMessage);
}

bool CommandNetworkInviteGamersToSession(int& handleData, int nGamers, const char* szSubject, const char* szMessage)
{
	if(!scriptVerify(nGamers <= RL_MAX_GAMERS_PER_SESSION))
	{
		scriptErrorf("%s:NETWORK_INVITE_GAMERS - Too many gamers. Max: %d, Num: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), RL_MAX_GAMERS_PER_SESSION, nGamers);
		return false;
	}

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&handleData);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in leaderboard handles
	rlGamerHandle hGamers[RL_MAX_GAMERS_PER_SESSION];
	for(u32 i = 0; i < nGamers; i++)
	{ 
		if(!CTheScripts::ImportGamerHandle(&hGamers[i], pInfoBuffer ASSERT_ONLY(, "NETWORK_INVITE_GAMERS")))
			return false;

		pInfoBuffer += GAMER_HANDLE_SIZE;
	}

	return CNetwork::GetNetworkSession().SendInvites(hGamers, nGamers, szSubject, szMessage);
}

bool CommandNetworkHasInvitedGamer(int& handleData)
{
	// check that we're in a valid session
	if(!scriptVerifyf(NetworkInterface::IsInSession(), "%s:NETWORK_HAS_INVITED_GAMER - Not in a network session!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_INVITED_GAMER")))
		return false;

	// find player in invited list
	return CNetwork::GetNetworkSession().GetInvitedPlayers().DidInvite(hGamer);
}

bool CommandNetworkHasInviteBeenAcked(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_INVITE_BEEN_ACKED")))
		return false;

	// find player in invited list
	return CNetwork::GetNetworkSession().GetInvitedPlayers().IsAcked(hGamer);
}

bool CommandNetworkHasMadeInviteDecision(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_MADE_INVITE_DECISION")))
		return false;

	// find player in invited list
	return CNetwork::GetNetworkSession().IsInviteDecisionMade(hGamer);
}

int CommandNetworkGetInviteReplyStatus(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_INVITE_REPLY_STATUS")))
		return false;

	// find player in invited list
	return static_cast<int>(CNetwork::GetNetworkSession().GetInviteStatus(hGamer));
}

int CommandNetworkGetNumUnacceptedInvites()
{
	return static_cast<int>(CLiveManager::GetInviteMgr().GetNumUnacceptedInvites());
}

const char* CommandNetworkGetUnacceptedInviterName(const int index)
{
	const UnacceptedInvite* pInvite = CLiveManager::GetInviteMgr().GetUnacceptedInvite(index);
	return pInvite ? pInvite->m_InviterName : "INVALID_INDEX";
}

bool CommandNetworkAcceptInvite(const int index)
{
	const UnacceptedInvite* pInvite = CLiveManager::GetInviteMgr().GetUnacceptedInvite(index);
	if(pInvite)
		return CLiveManager::GetInviteMgr().AcceptInvite(index);

	// invalid index
	return false;
}

bool CommandNetworkGetCurrentlySelectedGamerHandleFromInviteMenu(int& handleData)
{
	if(!NetworkInterface::IsNetworkOpen() || !AssertVerify(CPauseMenu::IsActive()))
		return false;

	const rlGamerHandle& rGamerHandle = SPlayerCardManager::GetInstance().GetCurrentGamer();
	return CTheScripts::ExportGamerHandle(&rGamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_CURRENTLY_SELECTED_GAMER_HANDLE_FROM_INVITE_MENU"));
}

bool CommandNetworkSetCurrentlySelectedGamerHandleFromInviteMenu(int& handleData)
{
	if(!NetworkInterface::IsNetworkOpen() || !AssertVerify(CPauseMenu::IsActive()))
		return false;

	rlGamerHandle gamerHandle;
	if(CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_CURRENTLY_SELECTED_GAMER_HANDLE_FROM_INVITE_MENU")))
	{
		SPlayerCardManager::GetInstance().SetCurrentGamer(gamerHandle);
		return true;
	}

	return false;
}

bool CommandNetworkSetCurrentGamerHandleForDataManager(int& handleData)
{
	rlGamerHandle gamerHandle;
	if(CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_CURRENT_DATA_MANAGER_HANDLE")))
	{
		scriptDebugf1("%s:NETWORK_SET_CURRENT_DATA_MANAGER_HANDLE command called for handle='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), NetworkUtils::LogGamerHandle(gamerHandle));
		SPlayerCardManager::GetInstance().SetCurrentGamer(gamerHandle);
		return true;
	}

	return false;
}

void CommandNetworkSetInviteFailedMessageForInviteMenu(int& handleData, const char* pFailedMessage)
{
	if(!NetworkInterface::IsNetworkOpen() || !AssertVerify(CPauseMenu::IsActive()))
		return;

	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return;
	}

	rlGamerHandle gamerHandle;
	if(!CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_CURRENTLY_SELECTED_GAMER_HANDLE_FROM_INVITE_MENU")))
	{
		return;
	}

	scriptAssertf(pFailedMessage, "NETWORK_SET_INVITE_FAILED_MESSAGE_FOR_INVITE_MENU was called with a NULL string passed in.  Need a valid string to display the message.");

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager();
	CScriptedPlayersMenu* pMenu = dynamic_cast<CScriptedPlayersMenu*>(CPauseMenu::GetCurrentScreenData().GetDynamicMenu());

	if(scriptVerifyf(pDataManager, "NETWORK_SET_INVITE_FAILED_MESSAGE_FOR_INVITE_MENU was called when not in a Playerlist."))
	{
		int gamerIndex = pDataManager->GetGamerIndex(gamerHandle);
		if(gamerIndex != -1)
		{
			scriptDebugf3("NETWORK_SET_INVITE_FAILED_MESSAGE_FOR_INVITE_MENU called.");
			pDataManager->GetGamerData(gamerIndex).m_inviteBlockedLabelHash = atStringHash(pFailedMessage);

			if (pMenu )
			{
				pMenu->GetGamerData(gamerIndex).m_inviteBlockedLabelHash = atStringHash(pFailedMessage);
			}
		}
	}
}


void CommandNetworkSetPlayerToOnCallForJoinedMenu(int& handleData)
{
	if(!NetworkInterface::IsNetworkOpen() || !AssertVerify(CPauseMenu::IsActive()))
		return;
	
	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return;
	}

	rlGamerHandle gamerHandle;
	if(!CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_INVITE_ON_CALL_FOR_INVITE_MENU")))
	{
		return;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager();
	if(scriptVerifyf(pDataManager, "NETWORK_SET_INVITE_ON_CALL_FOR_INVITE_MENU was called when not in a Playerlist."))
	{
		int gamerIndex = pDataManager->GetGamerIndex(gamerHandle);
		if(gamerIndex != -1)
		{
			pDataManager->GetGamerData(gamerIndex).m_isOnCall = true;
		}
	}
}

bool CommandsIsHostPlayerAndRockstarDev(int& handleData)
{
	if(!NetworkInterface::IsNetworkOpen() || !AssertVerify(CPauseMenu::IsActive()))
		return false;

	rlGamerHandle gamerHandle;
	if(!CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SET_INVITE_ON_CALL_FOR_INVITE_MENU")))
	{
		return false;
	}

	return CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(gamerHandle);
}

bool CommandNetworkCheckDataManagerStatsQueryFailed(int cardType)
{
	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
		|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);
	scriptAssertf(isValidCard, "%s:NETWORK_CHECK_DATA_MANAGER_STATS_QUERY_FAILED - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);

	if (!isValidCard)
	{
		return false;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType);

	if(scriptVerifyf(pDataManager, "NETWORK_CHECK_DATA_MANAGER_STATS_QUERY_FAILED was called when not in a Playerlist."))
	{
		return pDataManager->HadError();
	}

	return false;
}

bool CommandNetworkCheckDataManagerStatsQueryPending(int cardType)
{
	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
		|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);
	scriptAssertf(isValidCard, "%s:NETWORK_CHECK_DATA_MANAGE_STATS_QUERY_PENDING - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);

	if (!isValidCard)
	{
		return false;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType);

	if(scriptVerifyf(pDataManager, "NETWORK_CHECK_DATA_MANAGE_STATS_QUERY_PENDING was called when not in a Playerlist."))
	{
		if (pDataManager->HadError())
			return false;

		if (pDataManager->HasSucceeded())
			return false;

		return pDataManager->IsStatsQueryPending();
	}

	return false;
}

bool CommandNetworkCheckDataManagerSucceeded(int cardType)
{

	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
								|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);
	scriptAssertf(isValidCard, "%s:NETWORK_CHECK_DATA_MANAGER_SUCCEEDED - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);

	if (!isValidCard)
	{
		return false;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType);

	if(scriptVerifyf(pDataManager, "NETWORK_CHECK_DATA_MANAGER_SUCCEEDED was called when not in a Playerlist."))
	{
		return pDataManager->HasSucceeded();
	}

	return false;
}

bool CommandNetworkCheckDataManagerSucceededForHandle(int cardType, int& handleData)
{

	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	rlGamerHandle gamerHandle;
	if(!CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CHECK_DATA_MANAGER_SUCCEEDED_FOR_HANDLE")))
	{
		scriptAssertf(0, "%s:NETWORK_CHECK_DATA_MANAGER_SUCCEEDED_FOR_HANDLE - Failed to import the Gamerhandle!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
		|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);
	scriptAssertf(isValidCard, "%s:NETWORK_CHECK_DATA_MANAGER_SUCCEEDED_FOR_HANDLE - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);

	if (!isValidCard)
	{
		return false;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType);

	if(scriptVerifyf(pDataManager, "NETWORK_CHECK_DATA_MANAGER_SUCCEEDED_FOR_HANDLE was called when not in a Playerlist."))
	{
		return pDataManager->ArePlayerStatsAccessible(gamerHandle);
	}

	return false;
}

bool CommandNetworkCheckDataManagerForHandle(int cardType, int& handleData)
{

	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	rlGamerHandle gamerHandle;
	if(!CTheScripts::ImportGamerHandle(&gamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CHECK_DATA_MANAGER_FOR_HANDLE")))
	{
		scriptAssertf(0, "%s:NETWORK_CHECK_DATA_MANAGER_FOR_HANDLE - Failed to import the Gamerhandle!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
		|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);
	scriptAssertf(isValidCard, "%s:NETWORK_CHECK_DATA_MANAGER_FOR_HANDLE - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);

	if (!isValidCard)
	{
		return false;
	}

	BasePlayerCardDataManager* pDataManager = SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType);

	if(scriptVerifyf(pDataManager, "NETWORK_CHECK_DATA_MANAGER_FOR_HANDLE was called when not in a Playerlist."))
	{
		return pDataManager->Exists(gamerHandle);
	}

	return false;
}

bool CommandFillOutPMPlayerList(scrArrayBaseAddr& dataBase, const int numGamers, int cardType )
{
	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	u8* pData = scrArray::GetElements<u8>(dataBase);

	if(!scriptVerify(pData))
	{
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
								|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);

	scriptAssertf(isValidCard, "%s:FILLOUT_PM_PLAYER_LIST - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);
	if (!isValidCard)
	{
		return false;
	}

	if (cardType == CPlayerCardManager::CARDTYPE_CORONA_PLAYERS 
		|| cardType == CPlayerCardManager::CARDTYPE_JOINED)
	{
		CScriptedCoronaPlayerCardDataManager* pManager = reinterpret_cast<CScriptedCoronaPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType));
		if(pManager)
		{
			BasePlayerCardDataManager::ValidityRules rules;
			SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);

			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			const unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for(u32 i = 0; i < numGamers; i++, pData += GAMER_HANDLE_SIZE)
			{
				rlGamerHandle handle;
				if(CTheScripts::ImportGamerHandle(&handle, pData ASSERT_ONLY(, "FILLOUT_PM_PLAYER_LIST")))
				{
					for(int i=0; i<nGamers; i++)
					{
						if (aGamers[i].GetGamerHandle() == handle)
						{
							if (pManager->Exists(handle))
							{
								pManager->SetGamerHandleName(aGamers[i].GetGamerHandle(), aGamers[i].GetName());
							}
							else
							{
								if (!CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(handle))
									pManager->AddGamerHandle(aGamers[i].GetGamerHandle(), aGamers[i].GetName());
							}
							break;
						}
					}
				}
			}

			return true;
		}

		return false;
	}

	CScriptedPlayerCardDataManager* pManager = reinterpret_cast<CScriptedPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType));
	if(pManager)
	{
		if (cardType == CPlayerCardManager::CARDTYPE_CORONA_JOINED_PLAYLIST)
		{
			BasePlayerCardDataManager::ValidityRules rules;
			SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);
		}

		pManager->ClearGamerHandles();

		for(u32 i = 0; i < numGamers; i++, pData += GAMER_HANDLE_SIZE)
		{ 
			rlGamerHandle gamer;

			if(CTheScripts::ImportGamerHandle(&gamer, pData ASSERT_ONLY(, "FILLOUT_PM_PLAYER_LIST")))
			{
				if (!CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(gamer))
					pManager->AddGamerHandle(gamer);
			}
		}

		pManager->ValidateNewGamerList();

		return true;
	}

	return false;
}

bool CommandFillOutPMPlayerListWithNames(scrArrayBaseAddr& gamerBase, scrArrayBaseAddr& nameBase, const int numGamers, int cardType )
{
	if(!scriptVerify(SPlayerCardManager::IsInstantiated()))
	{
		return false;
	}

	// TODO: NS - if these names are displayed on screen, we need to use display names (scrTextLabel63)
	u8* pData = scrArray::GetElements<u8>(gamerBase);
	scrTextLabel63* pNameData = scrArray::GetElements<scrTextLabel63>(nameBase);

	if(!scriptVerify(pData) || !scriptVerify(pNameData))
	{
		return false;
	}

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
								|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);

	scriptAssertf(isValidCard, "%s:FILLOUT_PM_PLAYER_LIST_WITH_NAMES - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);
	if (!isValidCard)
	{
		return false;
	}

	if (cardType == CPlayerCardManager::CARDTYPE_CORONA_PLAYERS 
		|| cardType == CPlayerCardManager::CARDTYPE_JOINED)
	{
		CScriptedCoronaPlayerCardDataManager* pManager = reinterpret_cast<CScriptedCoronaPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType));
		if(pManager)
		{
			BasePlayerCardDataManager::ValidityRules rules;
			SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);

			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			const unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for(u32 i = 0; i < numGamers; i++, pData += GAMER_HANDLE_SIZE)
			{
				rlGamerHandle handle;
				if(CTheScripts::ImportGamerHandle(&handle, pData ASSERT_ONLY(, "FILLOUT_PM_PLAYER_LIST_WITH_NAMES")))
				{
					for(int i=0; i<nGamers; i++)
					{
						if (aGamers[i].GetGamerHandle() == handle)
						{
							if (pManager->Exists(handle))
							{
								pManager->SetGamerHandleName(aGamers[i].GetGamerHandle(), pNameData[i]);
							}
							else
							{
								if (!CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(aGamers[i].GetGamerHandle()))
									pManager->AddGamerHandle(aGamers[i].GetGamerHandle(), pNameData[i]);
							}

							break;
						}
					}
				}
			}

			return true;
		}

		return false;
	}

	CScriptedPlayerCardDataManager* pManager = reinterpret_cast<CScriptedPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType));
	if(pManager)
	{
		pManager->ClearGamerHandles();

		for(u32 i = 0; i < numGamers; i++, pData += GAMER_HANDLE_SIZE)
		{
			rlGamerHandle gamer;

			if(CTheScripts::ImportGamerHandle(&gamer, pData ASSERT_ONLY(, "FILLOUT_PM_PLAYER_LIST_WITH_NAMES")))
			{
				if (!CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(gamer))
					pManager->AddGamerHandle(gamer, pNameData[i]);
			}
		}

		pManager->ValidateNewGamerList();
		return true;
	}

	return false;
}

bool CommandRefreshPlayerListStats(int cardType)
{
	bool isUpToDate = false;

	const bool isValidCard = (CPlayerCardManager::CARDTYPE_START_SCRIPTED_TYPES <= cardType 
								|| cardType <= CPlayerCardManager::CARDTYPE_END_SCRIPTED_TYPES);

	scriptAssertf(isValidCard, "%s:REFRESH_PLAYER_LIST_STATS - Invalid card type '%d'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cardType);
	if (!isValidCard)
	{
		return false;
	}

	if(uiVerify(SPlayerCardManager::IsInstantiated()))
	{
		if (cardType == CPlayerCardManager::CARDTYPE_CORONA_PLAYERS
			|| cardType == CPlayerCardManager::CARDTYPE_JOINED)
		{
			BasePlayerCardDataManager::ValidityRules rules;
			SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);
			return true;
		}
		else
		{
			CScriptedPlayerCardDataManager* pManager = reinterpret_cast<CScriptedPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager((CPlayerCardManager::CardTypes) cardType));
			if(uiVerify(pManager))
			{
				isUpToDate = !pManager->IsPlayerListDirty();

				BasePlayerCardDataManager::ValidityRules rules;

				if(isUpToDate)
				{
					SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);
					return pManager->IsValid();
				}
				else
				{

					playercardDebugf1("CommandRefreshPlayerListStats - resetting CScriptedPlayerCardDataManager");
					pManager->Reset();
					SPlayerCardManager::GetInstance().Init((CPlayerCardManager::CardTypes) cardType, rules);
				}
			}
		}
	}

	return isUpToDate;
}

// Needs to match ONLINE_STATUS in commands_network.sch
enum eOnlineStatus
{
	OS_OFFLINE,
	OS_ONLINE_ANOTHER_GAME,
	OS_ONLINE_SP,
	OS_ONLINE_MP,
	OS_ONLINE_IN_LOCAL_SESSION,
	OS_ONLINE_IN_LOCAL_TRANSITION
};

int CommandGetPMPlayerOnlineStatus(int& handleData)
{
	int retVal = OS_OFFLINE;

	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "GET_PM_PLAYER_ONLINE_STATUS")))
	{
		if(CNetwork::GetNetworkSession().IsTransitionActive() && CNetwork::GetNetworkSession().IsTransitionMember(hGamer))
		{
			retVal = OS_ONLINE_IN_LOCAL_TRANSITION;
		}
		else
		{
			BasePlayerCardDataManager* pManager = SPlayerCardManager::GetInstance().GetDataManager();
			if(pManager)
			{
				const BasePlayerCardDataManager::GamerData& rData = pManager->GetGamerData(pManager->GetGamerIndex(hGamer));
				if(rData.m_isInSameSession)
				{
					retVal = OS_ONLINE_IN_LOCAL_SESSION;
				}
				else if(rData.m_isInSameTitleOnline)
				{
					retVal = OS_ONLINE_MP;
				}
				else if(rData.m_isInSameTitle)
				{
					retVal = OS_ONLINE_SP;
				}
				else if(rData.m_isOnline)
				{
					retVal = OS_ONLINE_ANOTHER_GAME;
				}
			}
		}
	}

	return retVal;
}

bool CommandNetworkShowPlatformPartyUI()
{
	return CLiveManager::ShowPlatformPartyUi();
}

bool CommandNetworkInvitePlatformParty()
{
	return CLiveManager::SendPlatformPartyInvites();
}

bool CommandNetworkIsInPlatformParty()
{
	return CLiveManager::IsInPlatformParty();
}

int CommandNetworkGetPlatformPartyMemberCount()
{
	return CLiveManager::GetPlatformPartyMemberCount();
}

int CommandNetworkGetPlatformPartyMembers(int& infoData, int sizeOfData)
{
	// script cannot have a preprocessor define for this so need to use the maximum cross platform
	// value of 32 (Xbox One). Set this array to the same size.
	static const unsigned MAX_SCRIPT_PARTY_MEMBERS = 32;

	rlPlatformPartyMember aPartyMembers[MAX_SCRIPT_PARTY_MEMBERS];
	unsigned nPartyMembers = CLiveManager::GetPlatformPartyInfo(aPartyMembers, MAX_SCRIPT_PARTY_MEMBERS);
	if(nPartyMembers == 0)
		return 0;

	// buffer to write our info into
	u8* pInfoBuffer = reinterpret_cast<u8*>(&infoData);

	// script returns size in script words
	sizeOfData *= sizeof(scrValue);

	// calculate size required
	int nRequiredSize = 0;
	nRequiredSize += sizeof(scrValue);										// member count
	nRequiredSize += sizeof(scrValue);										// array size prefix
	nRequiredSize += GAMER_HANDLE_SIZE * MAX_SCRIPT_PARTY_MEMBERS;		// gamer handle for each member
	nRequiredSize += sizeof(scrTextLabel63) * MAX_SCRIPT_PARTY_MEMBERS;	// name string for each member
	nRequiredSize += sizeof(scrValue);										// is online bool for each member

	// validate
	if(!gnetVerifyf(nRequiredSize == sizeOfData, "%s:NETWORK_GET_PLATFORM_PARTY_MEMBERS - Invalid size!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return 0;

	// copy in member count
	sysMemCpy(pInfoBuffer, &nPartyMembers, sizeof(scrValue));
	pInfoBuffer += sizeof(scrValue);

	// skip array size prefix
	pInfoBuffer += sizeof(scrValue);

	// fill in party members
	for(u32 i = 0; i < nPartyMembers; i++)
	{
		// copy in handle
		rlGamerHandle hGamer;
		aPartyMembers[i].GetGamerHandle(&hGamer);
		hGamer.Export(pInfoBuffer, GAMER_HANDLE_SIZE);
		pInfoBuffer += GAMER_HANDLE_SIZE;

		// copy in name
		safecpy(reinterpret_cast<char*>(pInfoBuffer), aPartyMembers[i].GetDisplayName(), sizeof(scrTextLabel63));
		pInfoBuffer += sizeof(scrTextLabel63);
	}

	// set the session info to false (not used)
	*pInfoBuffer = false;

	// return value is number of members
	return nPartyMembers;
}

bool CommandNetworkJoinPlatformPartyMemberSession(int& handleData)
{
	// validate handle data
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_JOIN_PLATFORM_PARTY_MEMBER_SESSION")))
		return false; 

	// call into live manager
	return CLiveManager::JoinPlatformPartyMemberSession(hGamer);
}

bool CommandNetworkIsInPlatformPartyChat()
{
    // call into live manager
    return CLiveManager::IsInPlatformPartyChat();
}

bool CommandNetworkIsChattingInPlatformParty(int& handleData)
{
    // validate handle data
    rlGamerHandle hGamer;
    if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_CHATTING_IN_PLATFORM_PARTY")))
        return false; 

    // call into live manager
    return CLiveManager::IsChattingInPlatformParty(hGamer);
}

bool CommandNetworkShowPlatformVoiceChannelUI(bool bVoiceRequired)
{
    // call into live manager
    return CLiveManager::ShowPlatformVoiceChannelUI(bVoiceRequired);
}

bool CommandNetworkCanQueueForPreviousSessionJoin()
{
	return CNetwork::GetNetworkSession().CanQueueForPreviousSessionJoin();
}

bool CommandNetworkIsQueuingForSessionJoin()
{
	return CNetwork::GetNetworkSession().GetJoinQueueToken() > 0;
}

void CommandNetworkClearQueuedJoinRequest()
{
	CNetwork::GetNetworkSession().ClearQueuedJoinRequest();
}

void CommandNetworkSendQueuedJoinRequest()
{
	CNetwork::GetNetworkSession().SendQueuedJoinRequest();
}

void CommandNetworkRemoveAllQueuedJoinRequests()
{
	CNetwork::GetNetworkSession().RemoveAllQueuedJoinRequests();
}

static mthRandom gs_ScriptRandom; 

void CommandNetworkSeedRandomNumberGenerator(int nSeed)
{
	gs_ScriptRandom.Reset(nSeed);
}

int CommandNetworkGetRandomInt()
{
	return gs_ScriptRandom.GetInt();
}

int CommandNetworkGetRandomIntRanged(int nMinimum, int nMaximum)
{
	// check equality
	if(nMinimum == nMaximum)
		return nMinimum;

	// otherwise, minimum must be less than maximum
	if(!scriptVerifyf(nMinimum < nMaximum, "%s:NETWORK_GET_RANDOM_RANGED_INT - Minimum (%d) is greater than maximum (%d)!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMinimum, nMaximum))
		return 0;

	return gs_ScriptRandom.GetRanged(nMinimum, nMaximum);
}

void CommandNetworkSetThisScriptIsNetworkScript(int maxNumPlayers, bool activeInSinglePlayer, int instanceId)
{
	Assertf(maxNumPlayers>=1, "%s:NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - maxNumPlayers should be greater than 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(maxNumPlayers<=MAX_NUM_PHYSICAL_PLAYERS, "%s:NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - maxNumPlayers cannot exceed %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS);
	Assertf(CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType != CScriptsForBrains::PED_STREAMED, "%s:NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - ped brains scripts currently cannot be networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType != CScriptsForBrains::SCENARIO_PED, "%s:NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - scenario ped brains scripts currently cannot be networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());

#if !__FINAL
	NETWORK_QUITF(instanceId>=-1 && instanceId<(1<<CGameScriptId::SIZEOF_INSTANCE_ID), "%s:NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - instance id (%d) is invalid. Should be between -1 and %d",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), instanceId, (1<<CGameScriptId::SIZEOF_INSTANCE_ID)-1);
#endif 

	CTheScripts::GetScriptHandlerMgr().SetScriptAsANetworkScript(*CTheScripts::GetCurrentGtaScriptThread(), instanceId);

	if (AssertVerify(CTheScripts::GetCurrentGtaScriptHandlerNetwork()))
	{
		CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetMaxNumParticipants(maxNumPlayers);

		if (activeInSinglePlayer && AssertVerify(CNetwork::IsNetworkOpen()))
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetActiveInSinglePlayer();
		}
	}
}

bool CommandNetworkTryToSetThisScriptIsNetworkScript(int maxNumPlayers, bool activeInSinglePlayer, int instanceId)
{
	Assertf(maxNumPlayers>1, "%s:NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - maxNumPlayers should be greater than 1", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(maxNumPlayers<=MAX_NUM_PHYSICAL_PLAYERS, "%s:NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - maxNumPlayers cannot exceed %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_PHYSICAL_PLAYERS);
	Assertf(CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType != CScriptsForBrains::PED_STREAMED, "%s:NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - ped brains scripts currently cannot be networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType != CScriptsForBrains::SCENARIO_PED, "%s:NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - scenario ped brains scripts currently cannot be networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(instanceId>=-1 && instanceId<(1<<CGameScriptId::SIZEOF_INSTANCE_ID), "%s:NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT - instance id (%d) is invalid. Should be between -1 and %d",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), instanceId, (1<<CGameScriptId::SIZEOF_INSTANCE_ID)-1);

	if (CTheScripts::GetScriptHandlerMgr().TryToSetScriptAsANetworkScript(*CTheScripts::GetCurrentGtaScriptThread(), instanceId))
	{
		if (AssertVerify(CTheScripts::GetCurrentGtaScriptHandlerNetwork()))
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetMaxNumParticipants(maxNumPlayers);

			if (activeInSinglePlayer && AssertVerify(CNetwork::IsNetworkOpen()))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetActiveInSinglePlayer();
			}
		}

		return true;
	}

	return false;
}

bool CommandNetworkGetThisScriptIsNetworkScript()
{
	return (CTheScripts::GetCurrentGtaScriptHandlerNetwork() != NULL);
}

void CommandNetworkSetTeamReservations(int numTeams, int& teamReservations)
{
	u32 *reservationArray = (u32*)(&teamReservations);
	reservationArray++; //  arrays have an extra script word at the start for the array size, which we ignore

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_SET_TEAM_RESERVATIONS"))
	{
		if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandlerNetwork()) && 
			scriptVerify(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
		{
			int myTeam = NetworkInterface::GetLocalPlayer()->GetTeam();

			if (scriptVerifyf (numTeams == MAX_NUM_TEAMS, "NETWORK_SET_TEAM_RESERVATIONS - numTeams differs from max teams defined in code (%d)", MAX_NUM_TEAMS) &&
				SCRIPT_VERIFY (myTeam>=0 && myTeam<numTeams, "NETWORK_SET_TEAM_RESERVATIONS - the local player's team is invalid"))
			{
				scriptAssertf(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_NOT_ACTIVE, "NETWORK_SET_TEAM_RESERVATIONS - this must be called before you start calling NETWORK_GET_SCRIPT_STATUS");

				u32 count = 0;
				u32 otherTeamReservations = 0;

				for (int i=0; i<numTeams; i++)
				{
					count += reservationArray[i];

					if(i!=myTeam)
					{
						otherTeamReservations += reservationArray[i];
					}
				}

				scriptAssertf(count <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants(), "%s:%s - The total number of reserved players in the teamReservations array (%d) is greater than the max number of participants (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "NETWORK_SET_TEAM_RESERVATIONS", count, CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants());
				scriptAssertf(otherTeamReservations < CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants(), "%s:%s - There is no room for our local player to join the script - the number of reserved players in other teams is equal to the max number of participants (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "NETWORK_SET_TEAM_RESERVATIONS", CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants());

				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->SetTeamReservations((u32)numTeams, reservationArray);
			}
		}
	}
}

int CommandNetworkGetMaxNumParticipants()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_GET_MAX_NUM_PARTICIPANTS");

	if (pNetComponent)
	{
		return pNetComponent->GetMaxNumParticipants();
	}

	return 0;
}

int CommandNetworkGetNumParticipants()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_GET_NUM_PARTICIPANTS");

	if (pNetComponent)
    {
		return pNetComponent->GetNumParticipants();
    }

    return 0;
}

int CommandNetworkGetScriptStatus()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_GET_SCRIPT_STATUS"))
	{
		CGameScriptHandlerNetComponent* netComponent = static_cast<CGameScriptHandlerNetComponent*>(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent());

		if (!netComponent)
		{
			return scriptHandlerNetComponent::NETSCRIPT_PLAYING;
		}
		else
		{
			// inform the component it can start the joining process
			netComponent->SetScriptReady();
		}

		scriptHandlerNetComponent::eExternalState state = netComponent->GetState();

		// don't return a playing state until our local player is properly physical
		if (state == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
		{
			int mySlot = netComponent->GetSlotParticipantIsUsing(*NetworkInterface::GetLocalPlayer());

			if (!netComponent->IsParticipantPhysical(mySlot))
			{
				return scriptHandlerNetComponent::NETSCRIPT_JOINING;
			}
		}

	    return static_cast<int>(state);
    }

    return scriptHandlerNetComponent::NETSCRIPT_NOT_ACTIVE;
}


void RegisterHostBroadcastVariables(int &AddressTemp, int Size, bool bRegisterAsHighFrequency, const char* BANK_ONLY(debugArrayName) = 0, const char* ASSERT_ONLY(commandName) = 0)
{
	int *Address = &AddressTemp;
	Size *= sizeof(scrValue);                          // Script returns size in script words

	if (SCRIPT_VERIFY_NETWORK_REGISTERED(commandName) &&
		scriptVerifyf(IS_SCRIPT_NETWORK_ACTIVE(), "%s: The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(Size>0, "%s: %s - invalid size (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, Size))
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, commandName);

		if (bRegisterAsHighFrequency && NetworkInterface::GetArrayManager().HasMaxNumberOfHighFrequencyUpdateArrays())
		{
			gnetAssertf(0, "%s: NETWORK_HIGH_FREQUENCY_REGISTER_HOST_BROADCAST: Only %d broadcast data arrays are allowed to update at a high frequency. This has been exceeded, dropping to normal frequency", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netArrayManager::MAX_HIGH_FREQUENCY_ARRAYS);
			bRegisterAsHighFrequency = false;
		}

		CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->RegisterHostBroadcastData((unsigned*)Address, static_cast<unsigned>(Size), bRegisterAsHighFrequency BANK_ONLY(, debugArrayName));
	}
}

void RegisterPlayerBroadcastVariables(int &AddressTemp, int Size, bool bRegisterAsHighFrequency, const char* BANK_ONLY(debugArrayName) = 0, const char* ASSERT_ONLY(commandName) = 0)
{
	s32 *Address = &AddressTemp;

	if (SCRIPT_VERIFY_NETWORK_REGISTERED(commandName) &&
		scriptVerifyf(IS_SCRIPT_NETWORK_ACTIVE(), "%s: The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(Size>0, "%s: %s - invalid size (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, Size))
	{
		// the size supplied is in script words (4 bytes), arrays have an extra script word for the array size, which we ignore
		Size--;

		Assertf(Size%CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants() == 0, "%s:%s : The size of the array is incorrect - are you using 'sizeof(array)'?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);

		// convert to bytes
		Size *= sizeof(scrValue);

		Assertf(*Address==static_cast<s32>(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants()), "%s:%s : The number of elements in the player broadcast array (%d) does not match the max number of players for the script (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, *Address, CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants());

		// now move the data pointer to the start of the array data (past the size)
		Address++;

		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REGISTER_PLAYER_BROADCAST_VARIABLES");

		if (bRegisterAsHighFrequency && NetworkInterface::GetArrayManager().HasMaxNumberOfHighFrequencyUpdateArrays())
		{
			gnetAssertf(0, "%s: NETWORK_HIGH_FREQUENCY_REGISTER_PLAYER_BROADCAST_VARIABLES : Only %d broadcast data arrays are allowed to update at a high frequency. This has been exceeded, dropping to normal frequency", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netArrayManager::MAX_HIGH_FREQUENCY_ARRAYS);
			bRegisterAsHighFrequency = false;
		}

		// store the broadcast data info in the handler so it can be registered later if a network game starts
		CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->RegisterPlayerBroadcastData((unsigned*)Address, (unsigned)Size, bRegisterAsHighFrequency BANK_ONLY(, debugArrayName));
	}
}

void CommandNetworkRegisterHostBroadcastVariables(int &AddressTemp, int Size, const char* BANK_ONLY(debugArrayName))
{
	RegisterHostBroadcastVariables(AddressTemp, Size, false BANK_ONLY(, debugArrayName) ASSERT_ONLY(, "NETWORK_REGISTER_HOST_BROADCAST_VARIABLES"));
}

void CommandNetworkRegisterPlayerBroadcastVariables(int &AddressTemp, int Size, const char* BANK_ONLY(debugArrayName))
{
	RegisterPlayerBroadcastVariables(AddressTemp, Size, false BANK_ONLY(, debugArrayName) ASSERT_ONLY(, "NETWORK_REGISTER_PLAYER_BROADCAST_VARIABLES"));
}

void CommandNetworkRegisterHighFrequencyHostBroadcastVariables(int &AddressTemp, int Size, const char* BANK_ONLY(debugArrayName))
{
	RegisterHostBroadcastVariables(AddressTemp, Size, true BANK_ONLY(, debugArrayName) ASSERT_ONLY(, "NETWORK_REGISTER_HIGH_FREQUENCY_HOST_BROADCAST_VARIABLES"));
}

void CommandNetworkRegisterHighFrequencyPlayerBroadcastVariables(int &AddressTemp, int Size, const char* BANK_ONLY(debugArrayName))
{
	RegisterPlayerBroadcastVariables(AddressTemp, Size, true BANK_ONLY(, debugArrayName) ASSERT_ONLY(, "NETWORK_REGISTER_HIGH_FREQUENCY_PLAYER_BROADCAST_VARIABLES"));
}

void CommandNetworkTagForDebugRemotePlayerBroadcastVariables(bool bTag)
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_REGISTER_PLAYER_BROADCAST_VARIABLES") &&
		scriptVerifyf(IS_SCRIPT_NETWORK_ACTIVE(), "%s: The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		// Set or remove broadcast data debuggability
		CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->TagForDebugPlayerBroadcastData(bTag);
	}
}


void CommandNetworkFinishBroadcastingData()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_FINISH_BROADCASTING_DATA");

	if (pNetComponent)
	{
		pNetComponent->FinishBroadcastingData();
	}	
}

bool CommandNetworkHasReceivedHostBroadcastData()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_HAS_RECEIVED_HOST_BROADCAST_DATA");

	if (pNetComponent)
	{
		bool isHost = pNetComponent->GetHost() && pNetComponent->GetHost()->IsLocal();
		
		int numData = pNetComponent->GetNumHostBroadcastDatasRegistered();

		if (scriptVerifyf(numData > 0, "%s: NETWORK_HAS_RECEIVED_HOST_BROADCAST_DATA - No host broadcast data has been registered", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			for (int i=0; i<numData; i++)
			{
				netHostBroadcastDataHandlerBase* pHandler = pNetComponent->GetHostBroadcastDataHandler(i);

				if (AssertVerify(pHandler))
				{
					if (!isHost && !pHandler->HasReceivedAnUpdate())
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

void CommandNetworkAllowClientAlterationOfHostBroadcastData()
{
#if __DEV // this code only runs in dev builds because it is only there to prevent an assert when host broadcast data is changed locally by a client
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_ALLOW_CLIENT_ALTERATION_OF_HOST_BROADCAST_DATA");

	if (pNetComponent)
	{
		int numData = pNetComponent->GetNumHostBroadcastDatasRegistered();

		if (scriptVerifyf(numData > 0, "%s: NETWORK_ALLOW_CLIENT_ALTERATION_OF_HOST_BROADCAST_DATA - No host broadcast data has been registered", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			for (int i=0; i<numData; i++)
			{
				netHostBroadcastDataHandlerBase* pHandler = pNetComponent->GetHostBroadcastDataHandler(i);

				if (AssertVerify(pHandler))
				{
					if (scriptVerifyf(!pHandler->HasReceivedAnUpdate(), "NETWORK_ALLOW_CLIENT_ALTERATION_OF_HOST_BROADCAST_DATA - This is not permitted once we have received an update for the data"))
					{
						pHandler->AllowClientAlteration();
					}
				}
			}
		}
	}
#endif // __DEV
}

int CommandNetworkGetParticipantIndex(int playerIndex)
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_GET_PARTICIPANT_INDEX");

	if (pNetComponent)
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

		if (pPlayer)
		{
			if (pPlayer->IsMyPlayer() && !NetworkInterface::IsGameInProgress())
			{
				return 0;
			}
			else
			{
				scriptAssertf(pNetComponent->IsPlayerAParticipant(*pPlayer), "%s:NETWORK_GET_PARTICIPANT_INDEX - Player is not a participant", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return pNetComponent->GetSlotParticipantIsUsing(*pPlayer);
			}
		}
	}

	return -1;
}

int CommandNetworkGetPlayerIndexFromPed(int PedIndex)
{
    int playerIndex = -1;

	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pPed)
	{
		if ( pPed->IsAPlayerPed() && pPed->GetNetworkObject())
		{
			playerIndex = pPed->GetNetworkObject()->GetPhysicalPlayerIndex();
		}
	}

	if (playerIndex == INVALID_PLAYER_INDEX) playerIndex = -1;
    return playerIndex;
}

int CommandNetworkGetPlayerIndex(int participantIndex)
{
	int playerIndex = -1;

	if (SCRIPT_VERIFY_PARTICIPANT_INDEX("NETWORK_GET_PLAYER_INDEX", participantIndex))
	{
		CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_GET_PLAYER_INDEX");

		if (pNetComponent)
		{
			const CNetGamePlayer* pPlayer;

			if (scriptVerifyf(pNetComponent->IsParticipantPhysical(participantIndex, &pPlayer), "%s:NETWORK_GET_PLAYER_INDEX - Participant doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if (scriptVerify(pPlayer))
				{
					playerIndex = pPlayer->GetPhysicalPlayerIndex();
				}
			}
		}
	}

	if (playerIndex == INVALID_PLAYER_INDEX) playerIndex = -1;
	return playerIndex;
}

int CommandNetworkGetNumPlayers()
{
	// includes bots
	return NetworkInterface::GetNumPhysicalPlayers();
}

bool CommandNetworkIsPlayerConnected(int playerIndex)
{
	if(SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_IS_PLAYER_CONNECTED", playerIndex))
	{
		if(!NetworkInterface::IsGameInProgress())
		{
			return (playerIndex==0); // our local player is always in slot 0 when there is no network game running
		}
		else 
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));
			if(pPlayer && pPlayer->IsPhysical())
			{
				return true;
			}
		}
	}

	return false;
}

int CommandNetworkGetTotalNumPlayers()
{
	int num = 0;

	if (!IS_SCRIPT_NETWORK_ACTIVE())
	{
		return 1; // our local player
	}

	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
    const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        const netPlayer *pPlayer = activePlayers[index];

		if(pPlayer->GetPlayerType() != CNetGamePlayer::NETWORK_BOT)
		{
			num++;
		}
	}

    return num;
}

bool CommandNetworkIsParticipantActive(int participantIndex)
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_IS_PARTICIPANT_ACTIVE");

	if (pNetComponent)
	{
		return pNetComponent->IsParticipantPhysical(participantIndex);
	}

	return false;
}

bool CommandNetworkIsPlayerActive(int playerIndex)
{
	bool bPlayerActive = false;

#if __ASSERT
	if(SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_IS_PLAYER_ACTIVE", playerIndex))
#endif
	{
		if (!NetworkInterface::IsGameInProgress())
		{
			bPlayerActive = (playerIndex==0); // our local player is always in slot 0 when there is no network game running
		}
		else
		{
			PlayerFlags physicalPlayers = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();
			
			if ((physicalPlayers & (1<<playerIndex)) != 0)
			{
				bPlayerActive = true;
			}
			else if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
			{
				bPlayerActive = true;
			}
			else
			{
				//possible bot local player - local player bots are not listed in the remotephysicalplayersbitmask
				unsigned                 numLocalPlayers = netInterface::GetPlayerMgr().GetNumLocalPhysicalPlayers();
				const netPlayer * const *localPlayers    = netInterface::GetPlayerMgr().GetLocalPhysicalPlayers();
				for(unsigned index = 0; (!bPlayerActive && (index < numLocalPlayers)); index++)
				{
					const CNetGamePlayer *sourcePlayer = SafeCast(const CNetGamePlayer, localPlayers[index]);
					if ( sourcePlayer && (playerIndex == sourcePlayer->GetPhysicalPlayerIndex()) )
					{
						bPlayerActive = true;
					}
				}
			}

			// the player must also have a ped
			if (bPlayerActive)
			{
				if (!CTheScripts::FindNetworkPlayerPed(playerIndex))
				{
					bPlayerActive = false;
				}
			}
		}
	}

	return bPlayerActive;
}

bool CommandNetworkIsPlayerAParticipant(int playerIndex)
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_IS_PLAYER_A_PARTICIPANT");

	if (pNetComponent)
	{
		return pNetComponent->IsPlayerAParticipant((PhysicalPlayerIndex)playerIndex);
	}

	return false;
}

bool CommandNetworkIsHostOfThisScript()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_IS_HOST_OF_THIS_SCRIPT");

	if (pNetComponent)
	{
		return pNetComponent->IsHostLocal();
	}

	return true;
}

int CommandNetworkGetHostOfThisScript()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("NETWORK_GET_HOST_OF_THIS_SCRIPT");

	if (pNetComponent)
	{
		return pNetComponent->GetHostSlot();
	}

    return -1;
}

bool ValidateScriptName(const char* scriptName, const char* ASSERT_ONLY(commandName))
{
	if (!scriptName)
	{
		scriptAssertf(0, "%s:%s - scriptName string is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		return false;
	}

	if (strlen(scriptName) == 0)
	{
		scriptAssertf(0, "%s:%s - scriptName string is empty", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		return false;
	}

	return true;
}

int CommandNetworkGetHostOfScript(const char* scriptName, int instance, int positionHash)
{
	if (!ValidateScriptName(scriptName, "NETWORK_GET_HOST_OF_SCRIPT"))
	{
		return -1;
	}

	CGameScriptId scrId(scriptName, instance, positionHash);

	const netPlayer* pHost = NULL;

	scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(scrId);

	if (pHandler && pHandler->GetNetworkComponent())
	{
		pHost = pHandler->GetNetworkComponent()->GetHost();
	}
	else 
	{
		CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(scrId, true);

		if (pRemoteInfo)
		{
			pHost = pRemoteInfo->GetHost();
		}
	}

	int playerIndex = -1;
	if (pHost)
	{
		playerIndex = pHost->GetPhysicalPlayerIndex();
	}

	if (playerIndex == INVALID_PLAYER_INDEX) playerIndex = -1;
	return playerIndex;
}

int CommandNetworkGetHostOfThread(s32 threadId)
{
	int hostId = -1;

	GtaThread *pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(threadId)));

	if (scriptVerifyf(pThread && pThread->GetState() != scrThread::ABORTED, "%s:NETWORK_GET_HOST_OF_THREAD - The script is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		scriptHandler* pHandler = pThread->m_Handler;

		if (scriptVerifyf(pHandler, "%s:NETWORK_GET_HOST_OF_THREAD - The thread has no script handler", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent(), "%s:NETWORK_GET_HOST_OF_THREAD - The script is not a network script", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING, "%s:NETWORK_GET_HOST_OF_THREAD - The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const netPlayer* pHost = pHandler->GetNetworkComponent()->GetHost();

			if (pHost)
			{
				hostId = (int)pHost->GetPhysicalPlayerIndex();
			}
		}
	}

	if (hostId == INVALID_PLAYER_INDEX) hostId = -1;
	return hostId;
}

void CommandNetworkAllowPlayersToJoin(bool bJoin)
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_ALLOW_PLAYERS_TO_JOIN"))
	{
		CTheScripts::GetCurrentGtaScriptHandlerNetwork()->AcceptPlayers(bJoin);
	}
}

void CommandNetworkSetMissionFinished()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_SET_MISSION_FINISHED"))
	{
		if (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent())
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->SetScriptHasFinished();
		}
	}
}

bool CommandNetworkIsScriptActive(const char* scriptName, int instance, bool bLocalOnly, int positionHash)
{
	if (!ValidateScriptName(scriptName, "NETWORK_IS_SCRIPT_ACTIVE"))
	{
		return false;
	}

	CGameScriptId scrId(scriptName, instance, positionHash);

	bool bScriptActive = false;

	if (bLocalOnly)
	{
		bScriptActive = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(scrId) != NULL;
	}
	else
	{
		bScriptActive = CTheScripts::GetScriptHandlerMgr().IsScriptActive(scrId);
	}

	return bScriptActive;
}

bool CommandNetworkIsScriptActiveByHash(int scriptName, int instance, bool bLocalOnly, int positionHash)
{
	// We construct the scriptId from the hash value only, the name will not be available in this case.
	// But it's ok in this case, we only use it to look up the active script.
	CGameScriptId scrId(scriptName, instance, positionHash);

	bool bScriptActive = false;

	if (bLocalOnly)
	{
		bScriptActive = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(scrId) != NULL;
	}
	else
	{
		bScriptActive = CTheScripts::GetScriptHandlerMgr().IsScriptActive(scrId);
	}

	return bScriptActive;
}

bool CommandNetworkIsThreadANetworkScript(s32 threadId)
{
	GtaThread *pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(threadId)));

	if (scriptVerifyf(pThread && pThread->GetState() != scrThread::ABORTED, "%s:NETWORK_IS_THREAD_A_NETWORK_SCRIPT - The script is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		scriptHandler* pHandler = pThread->m_Handler;

		if (scriptVerifyf(pHandler, "%s:NETWORK_IS_THREAD_A_NETWORK_SCRIPT - The thread has no script handler", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			return pHandler->GetNetworkComponent() != NULL;
		}
	}

	return false;
}

u32 CommandNetworkGetNumScriptParticipants(const char* scriptName, int instance, int positionHash)
{
	if (!ValidateScriptName(scriptName, "NETWORK_GET_NUM_SCRIPT_PARTICIPANTS"))
	{
		return 0;
	}

	u32 numParticipants = 0;

	CGameScriptId scrId(scriptName, instance, positionHash);

	bool bScriptActive = CTheScripts::GetScriptHandlerMgr().IsScriptActive(scrId);

	if (scriptVerifyf(bScriptActive, "NETWORK_GET_NUM_SCRIPT_PARTICIPANTS - The script %s is not active", scrId.GetLogName()))
	{
		numParticipants = CTheScripts::GetScriptHandlerMgr().GetNumParticipantsOfScript(scrId);
	}

	return numParticipants;
}

int CommandNetworkGetInstanceIdOfThisScript()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_GET_INSTANCE_ID_OF_THIS_SCRIPT"))
	{
		return static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetScriptId()).GetInstanceId();
	}

	return -1;
}

int CommandNetworkGetPositionHashOfThisScript()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_GET_POSITION_HASH_OF_THIS_SCRIPT"))
	{
		return static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetScriptId()).GetPositionHash();
	}

	return 0;
}

bool CommandNetworkIsPlayerAParticipantOnScript(int playerIndex, const char* scriptName, int instance)
{
	if (!ValidateScriptName(scriptName, "NETWORK_IS_PLAYER_A_PARTICIPANT_ON_SCRIPT"))
	{
		return false;
	}

	CGameScriptId scrId(scriptName, instance);

	bool isParticipant = false;

	CNetGamePlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

	if (player)
	{
		isParticipant = CTheScripts::GetScriptHandlerMgr().IsPlayerAParticipant(scrId, *player);
	}

	return isParticipant;
}

void CommandNetworkPreventScriptHostMigration()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_PREVENT_SCRIPT_HOST_MIGRATION"))
	{
		if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent(), "NETWORK_PREVENT_SCRIPT_HOST_MIGRATION - the script is not networked"))
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->PreventHostMigration();
		}
	}
}

void CommandNetworkRequestToBeHostOfThisScript()
{
	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_REQUEST_TO_BE_HOST_OF_THIS_SCRIPT"))
	{
		if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent(), "NETWORK_REQUEST_TO_BE_HOST_OF_THIS_SCRIPT - the script is not networked"))
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->RequestToBeHost();
		}
	}
}

int CommandParticipantId()
{
	CGameScriptHandlerNetComponent* pNetComponent = GetNetComponent("PARTICIPANT_ID");

	if (pNetComponent)
	{
		return pNetComponent->GetLocalSlot();
	}

	return -1;
}

void CommandSetCurrentSpawnLocationOption(int newSpawnLocationOption)
{
	const int currentSpawnLocationOption = CNetworkTelemetry::GetCurrentSpawnLocationOption();
	if (currentSpawnLocationOption != newSpawnLocationOption)
	{
#if !__FINAL
		gnetDebug1("CommandSetCurrentSpawnLocationOption - Spawn location option changed: %d to %d", currentSpawnLocationOption, newSpawnLocationOption);
		scrThread::PrePrintStackTrace();
#endif //!__FINAL

		CNetworkTelemetry::SetCurrentSpawnLocationOption(newSpawnLocationOption);
	}
}

void CommandSetCurrentPublicContentId(const char* newPublicContentId)
{
	const char* currentPublicContentId = CNetworkTelemetry::GetCurrentPublicContentId();
	if (strcmp(currentPublicContentId, newPublicContentId) != 0)
	{
#if !__FINAL
		gnetDebug1("CommandSetCurrentPublicContentId - Public Content Id changed: %s to %s", currentPublicContentId, newPublicContentId);
		scrThread::PrePrintStackTrace();
#endif //!__FINAL

		CNetworkTelemetry::SetCurrentPublicContentId(newPublicContentId);
	}
}

void CommandSetCurrentChatOption(const int newChatOption)
{
	const int currentChatOption = (int)CNetworkTelemetry::GetCurrentChatOption();

	//validation
	if (!scriptVerify(currentChatOption >= (int)ChatOption::Everyone && currentChatOption < (int)ChatOption::Max))
		return;

	if (currentChatOption != newChatOption)
	{
#if !__FINAL
		gnetDebug1("CommandSetCurrentChatOption - Chat Option changed: %d to %d", currentChatOption, newChatOption);
		scrThread::PrePrintStackTrace();
#endif //!__FINAL

		CNetworkTelemetry::SetCurrentChatOption((ChatOption)newChatOption);
	}
}

void CommandSetVehicleDrivenInTestDrive(bool vehicleDrivenInTestDrive)
{
	const bool lastVehicleDrivenInTestDrive = CNetworkTelemetry::GetVehicleDrivenInTestDrive();
	if (lastVehicleDrivenInTestDrive != vehicleDrivenInTestDrive)
	{
#if !__FINAL
		gnetDebug1("CommandSetVehicleDrivenInTestDrive - Last vehicle driven in test drive changed: %s to %s", lastVehicleDrivenInTestDrive ? "true" : "false", vehicleDrivenInTestDrive ? "true" : "false");
		scrThread::PrePrintStackTrace();
#endif //!__FINAL

		CNetworkTelemetry::SetVehicleDrivenInTestDrive(vehicleDrivenInTestDrive);
	}
}

void CommandSetVehicleDrivenLocation(int vehicleDrivenLocation)
{
	const int lastVehicleDrivenlocation = CNetworkTelemetry::GetVehicleDrivenLocation();
	if (lastVehicleDrivenlocation != vehicleDrivenLocation)
	{
#if !__FINAL
		gnetDebug1("CommandSetVehicleDrivenLocation - Last vehicle driven location changed: %d to %d", lastVehicleDrivenlocation, vehicleDrivenLocation);
		scrThread::PrePrintStackTrace();
#endif //!__FINAL

		CNetworkTelemetry::SetVehicleDrivenLocation(vehicleDrivenLocation);
	}
}

void CommandNetworkResurrectLocalPlayer(const scrVector & scrPos_, float Heading, int invincibilityTime, bool leaveDeadPed, bool unpauseRenderPhases, int spawnLocation, int spawnReason)
{
	scrVector scrPos = scrPos_;
	respawnDebugf3("CommandNetworkResurrectLocalPlayer invincibilityTime[%d]",invincibilityTime);

	CPed* player = FindPlayerPed();

	if (player->GetIsInInterior())
	{
		CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(player->GetInteriorLocation());
		if (pProxy->GetGroupId() == 1)
		{
			CPortal::ResetMetroRespawnTimer();		// dying in the metro requires some TLC...
		}
	}

	scriptDebugf1("Command called NETWORK_RESURRECT_LOCAL_PLAYER.");

#if __DEV
	if (NetworkInterface::IsGameInProgress())
	{
		if(player && player->GetPlayerInfo() && player->GetNetworkObject())
		{
			static Vector3 oldPos;
			static u32 s_LastTimeRun     = 0;
			static u32 s_LastTimeCounter = 0;

			u32 currTime = sysTimer::GetSystemMsTime();
			Vector3 newPlayerPos = Vector3(scrPos);
			const float distance = (oldPos - newPlayerPos).Mag2();
			const float MIN_DIST = static_cast<float>(RESURRECT_LOCALPLAYER_DIST * RESURRECT_LOCALPLAYER_DIST);

			if (distance > MIN_DIST || ((currTime-s_LastTimeRun) > RESURRECT_LOCALPLAYER_TIME))
			{
				s_LastTimeCounter = 0;
			}

			if (2 < s_LastTimeCounter)
			{
				scriptAssertf(0, "%s:NETWORK_RESURRECT_LOCAL_PLAYER - Too soon to be calling again this command", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				scriptErrorf("NETWORK_RESURRECT_LOCAL_PLAYER:");
				scriptErrorf("............ script = \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				scriptErrorf("............ player = \"%s\":\"%s\"", player->GetNetworkObject()->GetLogName(), CModelInfo::GetBaseModelInfoName(player->GetModelId()));
				scriptErrorf("...... player state = \"%s\"", player->GetPlayerInfo()->PlayerStateToString());
				scriptErrorf(".......... position = \"%f, %f, %f\"", newPlayerPos.x, newPlayerPos.y, newPlayerPos.z);
				scriptErrorf("............. timer = \"times=%d\":\"last=%u\":\"curr=%u\"", s_LastTimeCounter, s_LastTimeRun, currTime);
				scriptErrorf(".......... distance = \"%f\"", distance);
			}

			s_LastTimeRun = currTime;
			++s_LastTimeCounter;
			oldPos = newPlayerPos;
		}
	}
#endif // __DEV

	const float fSecondSurfaceInterp=0.0f;

	Vector3 vGroundAdjustedPedRootPos = scrPos;
	if (CPedPlacement::FindZCoorForPed(fSecondSurfaceInterp,&vGroundAdjustedPedRootPos, NULL, NULL, NULL, 0.5f, 1.5f, 
										ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, NULL, true ))
	{
		scrPos = vGroundAdjustedPedRootPos;
	}

	scriptAssertf((DtoR * Heading) >= -TWO_PI, "%s:NETWORK_RESURRECT_LOCAL_PLAYER - Invalid heading %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), Heading);
	scriptAssertf((DtoR * Heading) <= TWO_PI, "%s:NETWORK_RESURRECT_LOCAL_PLAYER - Invalid heading %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), Heading);

	Heading = DtoR * Heading;
	if(Heading > TWO_PI)  { Heading = TWO_PI;  }
	if(Heading < -TWO_PI) { Heading = -TWO_PI; }
	const float headingInRadians = fwAngle::LimitRadianAngle(Heading);

	if (player && player->GetNetworkObject())
	{
		static_cast<CNetObjPlayer*>(player->GetNetworkObject())->SetRespawnInvincibilityTimer(static_cast<s16>(invincibilityTime));
	}

	CGameLogic::ResurrectLocalPlayer(scrPos, headingInRadians, leaveDeadPed, true, unpauseRenderPhases, spawnLocation, spawnReason);

}

void CommandNetworkRespawnLocalPlayerTelemetry(int spawnLocation, int spawnReason)
{
	CNetworkTelemetry::PlayerSpawn(spawnLocation, spawnReason);
}

void CommandNetworkSetLocalPlayerInvincibilityTime(int invincibilityTime)
{
	respawnDebugf3("CommandNetworkSetLocalPlayerInvincibilityTime[%d]",invincibilityTime);

	CPed* player = FindPlayerPed();
	if (player && player->GetNetworkObject())
	{
		static_cast<CNetObjPlayer*>(player->GetNetworkObject())->SetRespawnInvincibilityTimer(static_cast<s16>(invincibilityTime));
	}
}

bool CommandNetworkIsLocalPlayerInvincible()
{
	CPed* player = FindPlayerPed();
	if (player && player->GetNetworkObject())
	{
		return (static_cast<CNetObjPlayer*>(player->GetNetworkObject())->GetRespawnInvincibilityTimer() > 0);
	}

	return false;
}

void CommandNetworkDisableInvincibleFlashing(int PlayerIndex, bool disable)
{
    CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if(pPlayerPed)
	{
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(netObjPlayer)
        {
            netObjPlayer->SetDisableRespawnFlashing(disable);
        }
    }
}

void CommandNetworkPatchPostCutsceneHS4F_TUN_ENT(int playerPedIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(playerPedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (scriptVerifyf(pPed, "%s::NETWORK_PATCH_POST_CUTSCENE_HS4F_TUN_ENT - no ped found with index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerPedIndex)
		&& scriptVerifyf(pPed->IsLocalPlayer(), "%s::NETWORK_PATCH_POST_CUTSCENE_HS4F_TUN_ENT - should be called on a local player ped (instead of %s (%d))", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetLogName(), playerPedIndex))
	{
		CNetObjPlayer* pNetObjPlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject());
		if (scriptVerifyf(pNetObjPlayer, "%s::NETWORK_PATCH_POST_CUTSCENE_HS4F_TUN_ENT - no netObject for ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerPedIndex))
		{
			pNetObjPlayer->ForceSendPlayerAppearanceSyncNode();
		}
	}
}

void CommandNetworkSyncLocalPlayerLookAt(bool bSyncLookAt)
{
	CPed* player = FindPlayerPed();
	if (player && player->GetNetworkObject())
	{
		static_cast<CNetObjPlayer*>(player->GetNetworkObject())->SetSyncLookAt(bSyncLookAt);
	}
}

int CommandNetworkGetTimePlayerBeenDeadFor(int PlayerIndex)
{
	CPed * pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (pPlayerPed)
	{
		if (pPlayerPed->GetPlayerInfo()->GetPlayerState() != CPlayerInfo::PLAYERSTATE_HASDIED)
		{
			// Player is still alive.
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_TIME_NETWORK_PLAYER_BEEN_DEAD_FOR - player is still alive");
			return 0;
		}

		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_TIME_NETWORK_PLAYER_BEEN_DEAD_FOR");

		return fwTimer::GetTimeInMilliseconds_NonScaledClipped() - pPlayerPed->GetPlayerInfo()->GetTimeOfPlayerStateChange();
	}

    return 0;
}


//Only Returns a network game object if the killer is a player.
CNetGamePlayer* GetKillerOfPlayer(CNetGamePlayer* player, int &weaponHash)
{
	CPed* playerPed = player->GetPlayerPed();

	if (!playerPed || !playerPed->IsFatallyInjured())
		return NULL;

	CEntity* playerDamageEntity = NetworkUtils::GetPlayerDamageEntity(player, weaponHash);

	CNetGamePlayer* playerKiller = NULL;

	//Check if the damager is a vehicle, and get the driver
	if (playerDamageEntity)
	{
		if (playerDamageEntity->GetIsTypeVehicle())
		{
			CVehicle* vehicle = static_cast< CVehicle* >(playerDamageEntity);
			CPed* driver = vehicle->GetDriver();
			if (!driver && vehicle->GetSeatManager())
			{
				driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
			}

			if (driver)
			{
				playerDamageEntity = driver;
			}
		}
	}

	//Only Returns a network game object 
	//if the killer is a player.
	if (playerDamageEntity && playerDamageEntity->GetIsTypePed())
	{
		playerKiller = NetworkUtils::GetPlayerFromDamageEntity(playerPed, playerDamageEntity);

		if (playerKiller)
		{
			Assert (playerKiller->IsPhysical());

#if __DEV
	        if(playerPed && playerKiller->GetPlayerPed())
	        {
                NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "PLAYER_KILLER", playerKiller->GetPlayerPed()->GetNetworkObject()->GetLogName());
	        }
#endif // __DEV
        }
    }

	return playerKiller; 
}

int CommandNetworkGetEntityKillerOfPlayer(int PlayerIndex, int &weaponHash)
{
	int killerEntityIndex = -1;
	weaponHash = 0;

	CNetGamePlayer* player = CTheScripts::FindNetworkPlayer(PlayerIndex);
	if (player)
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_ENTITY_KILLER_OF_PLAYER");

		CPed* playerPed = player->GetPlayerPed();
		if (!playerPed || !playerPed->IsFatallyInjured())
			return 0;

		CEntity* damageEntity = NetworkUtils::GetPlayerDamageEntity(player, weaponHash);

		//Check if the damager is a vehicle, and get the driver
		if (damageEntity)
		{
			if (damageEntity->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast< CVehicle* >(damageEntity);
				CPed* driver = vehicle->GetDriver();
				if (!driver && vehicle->GetSeatManager())
				{
					driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
				}

				if (driver)
				{
					damageEntity = driver;
				}
			}

			killerEntityIndex = CTheScripts::GetGUIDFromEntity(*damageEntity);
		}
	}

	return killerEntityIndex;
}

int CommandNetworkGetKillerOfPlayer(int PlayerIndex, int &weaponHash)
{
	CNetGamePlayer *player = CTheScripts::FindNetworkPlayer(PlayerIndex);

	int killerIndex = -1;

	weaponHash = 0;

	if (player)
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_KILLER_OF_PLAYER");

		CNetGamePlayer* playerKiller = GetKillerOfPlayer(player, weaponHash);

		if (playerKiller)
		{
			killerIndex = playerKiller->GetPhysicalPlayerIndex();

#if !__NO_OUTPUT
			const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >(weaponHash);
			gnetDebug1("%s : NETWORK_GET_KILLER_OF_PLAYER : frame (%u) : Killer of player %s is %s, weapon (%s)"
									,CTheScripts::GetCurrentScriptNameAndProgramCounter()
									,fwTimer::GetSystemFrameCount()
									,player->GetLogName()
									,playerKiller->GetLogName()
									,(wi && wi->GetName()) ? wi->GetName() : "unknown");
#endif // !__NO_OUTPUT
		}
	}

	if (killerIndex == INVALID_PLAYER_INDEX) killerIndex = -1;
	return killerIndex;
}

const netPlayer* GetDestroyerOfObject(CPhysical* physical, int &weaponHash)
{
    const netPlayer *destroyerPlayer = 0;

    if(physical && physical->GetNetworkObject())
    {
		bool isDestroyed = true;

		if (physical->GetIsTypeVehicle())
			isDestroyed = static_cast<CVehicle*>(physical)->GetStatus() == STATUS_WRECKED;
		else if (physical->GetIsTypePed())
			isDestroyed = static_cast<CPed*>(physical)->IsFatallyInjured();
		else if (physical->GetIsTypeObject())
			isDestroyed = static_cast<CObject*>(physical)->GetHealth() <= 0.0f;

		if (isDestroyed)
		{
			CNetObjPhysical *netobjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

			CEntity *damageEntity = netobjPhysical->GetLastDamageObjectEntity();
			weaponHash = netobjPhysical->GetLastDamageWeaponHash();

			if (damageEntity)
			{
				destroyerPlayer = NetworkUtils::GetPlayerFromDamageEntity(physical, damageEntity);
			}
		}
    }

	return destroyerPlayer;
}

int CommandNetworkGetDestroyerOfNetworkId(int networkId, int &weaponHash)
{
	const netPlayer* pDestroyerPlayer = NULL;

	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_GET_DESTROYER_OF_NETWORK_ID - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(networkId != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_GET_DESTROYER_OF_NETWORK_ID - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(physical, "%s:NETWORK_GET_DESTROYER_OF_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pDestroyerPlayer = GetDestroyerOfObject(physical, weaponHash);

#if !__NO_OUTPUT
			if (pDestroyerPlayer && physical->GetNetworkObject())
			{
				const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >(weaponHash);
				gnetDebug1("%s : NETWORK_GET_DESTROYER_OF_NETWORK_ID : frame (%u) : Killer of player %s is %s, weapon (%s)"
													,CTheScripts::GetCurrentScriptNameAndProgramCounter()
													,fwTimer::GetSystemFrameCount()
													,physical->GetNetworkObject()->GetLogName()
													,pDestroyerPlayer->GetLogName()
													,(wi && wi->GetName()) ? wi->GetName() : "unknown");
			}
#endif // !__NO_OUTPUT
		}

		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_DESTROYER_OF_NETWORK_ID");
	}

	int destroyerIndex = -1;
	if (pDestroyerPlayer)
	{
		destroyerIndex = pDestroyerPlayer->GetPhysicalPlayerIndex();
	}

	if (destroyerIndex == INVALID_PLAYER_INDEX) destroyerIndex = -1;
	return destroyerIndex;
}

int CommandNetworkGetDestroyerOfEntity(int entityId, int &weaponHash)
{
	const netPlayer* pDestroyerPlayer = NULL;

	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		if (scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_GET_DESTROYER_OF_ENTITY - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* pPhysical = SafeCast(CPhysical, const_cast<CEntity*>(pEntity));
			netObject* pNetObj = pPhysical ? pPhysical->GetNetworkObject() : NULL;

			if (scriptVerifyf(pNetObj, "%s:NETWORK_GET_DESTROYER_OF_ENTITY - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pDestroyerPlayer = GetDestroyerOfObject(pPhysical, weaponHash);

#if !__NO_OUTPUT
				if (pDestroyerPlayer && pPhysical->GetNetworkObject())
				{
					const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >(weaponHash);
					gnetDebug1("%s : NETWORK_GET_DESTROYER_OF_ENTITY : frame (%u) : Killer of player %s is %s, weapon (%s)"
														,CTheScripts::GetCurrentScriptNameAndProgramCounter()
														,fwTimer::GetSystemFrameCount()
														,pPhysical->GetNetworkObject()->GetLogName()
														,pDestroyerPlayer->GetLogName()
														,(wi && wi->GetName()) ? wi->GetName() : "unknown");
				}
#endif // !__NO_OUTPUT

				WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_DESTROYER_OF_ENTITY");
			}
		}
	}

	int destroyerIndex = -1;
	if (pDestroyerPlayer)
	{
		destroyerIndex = pDestroyerPlayer->GetPhysicalPlayerIndex();
	}

	if (destroyerIndex == INVALID_PLAYER_INDEX) destroyerIndex = -1;
	return destroyerIndex;
}

bool CommandNetworkGetAssistedKillOfEntity(int playerIndexDamager, int entityDamaged, int& damageDealt)
{
	bool assistedKill = false;

	damageDealt = 0;

	if (!SCRIPT_VERIFY_NETWORK_ACTIVE("NETWORK_GET_ASSISTED_KILL_OF_ENTITY"))
	{
		return assistedKill;
	}

	if (!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_GET_ASSISTED_KILL_OF_ENTITY", playerIndexDamager))
	{
		return assistedKill;
	}

	CNetGamePlayer* damagerPlayer = CTheScripts::FindNetworkPlayer(playerIndexDamager);
	scriptAssertf(damagerPlayer, "NETWORK_GET_ASSISTED_KILL_OF_ENTITY : Invalid Index For Player damager");

	if (!damagerPlayer || !damagerPlayer->GetPlayerPed())
	{
		return assistedKill;
	}

	CNetObjPhysical* damagedNetObj = NULL;

	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityDamaged, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (entity)
	{
		scriptAssertf(entity->GetIsPhysical(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (entity->GetIsPhysical())
		{
			if (entity->GetIsTypePed())
			{
				CPed* ped = SafeCast(CPed, const_cast<CEntity*>(entity));
				scriptAssertf(ped->GetNetworkObject(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity doesnt have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				if (ped->GetNetworkObject())
				{
					scriptAssertf(ped->IsFatallyInjured(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity %s is not dead", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ped->GetNetworkObject()->GetLogName());
				}

				damagedNetObj = ped->IsFatallyInjured() ? static_cast<CNetObjPhysical*>(ped->GetNetworkObject()) : NULL;
			}
			else if (entity->GetIsTypeVehicle())
			{
				CVehicle* vehicle = SafeCast(CVehicle, const_cast<CEntity*>(entity));

				scriptAssertf(vehicle->GetNetworkObject(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity doesnt have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				if (vehicle->GetNetworkObject())
				{
					scriptAssertf(vehicle->GetStatus() == STATUS_WRECKED || vehicle->m_nVehicleFlags.bIsDrowning, "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity %s is not destroyed", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vehicle->GetNetworkObject()->GetLogName());
				}

				damagedNetObj = (vehicle->GetStatus() == STATUS_WRECKED || vehicle->m_nVehicleFlags.bIsDrowning) ? static_cast<CNetObjPhysical*>(vehicle->GetNetworkObject()) : NULL;
			}
			else if (entity->GetIsTypeObject())
			{
				CObject* object = SafeCast(CObject, const_cast<CEntity*>(entity));
				scriptAssertf(object->GetNetworkObject(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity doesnt have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				if (object->GetNetworkObject())
				{
					scriptAssertf(object->GetHealth() <= 0.0f, "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - entity %s is not destroyed", CTheScripts::GetCurrentScriptNameAndProgramCounter(), object->GetNetworkObject()->GetLogName());
				}

				damagedNetObj = object->GetHealth() <= 0.0f ? static_cast<CNetObjPhysical*>(object->GetNetworkObject()) : NULL;
			}
		}
	}

	if (scriptVerifyf(damagedNetObj, "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(damagedNetObj->IsTrackingDamage(), "%s:NETWORK_GET_ASSISTED_KILL_OF_ENTITY - the entity (%s) is not tracking damage", CTheScripts::GetCurrentScriptNameAndProgramCounter(), damagedNetObj->GetLogName()))
		{
			damageDealt = static_cast<int>(damagedNetObj->GetDamageDealtByPlayer(*damagerPlayer));
			assistedKill = (damageDealt > 0);
		}
	}

	return assistedKill;
}

bool CommandNetworkGetAssistedDamageOfEntity(int playerIndexDamager, int entityDamaged, int& damageDealt)
{
	bool assistedDamage = false;

	damageDealt = 0;

	if (!SCRIPT_VERIFY_NETWORK_ACTIVE("NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY"))
	{
		return assistedDamage;
	}

	if (!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY", playerIndexDamager))
	{
		return assistedDamage;
	}

	CNetGamePlayer* damagerPlayer = CTheScripts::FindNetworkPlayer(playerIndexDamager);
	scriptAssertf(damagerPlayer, "NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY : Invalid Index For Player damager");

	if (!damagerPlayer || !damagerPlayer->GetPlayerPed())
	{
		return assistedDamage;
	}

	CNetObjPhysical* damagedNetObj = NULL;

	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityDamaged, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (entity)
	{
		scriptAssertf(entity->GetIsPhysical(), "%s:NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (entity->GetIsPhysical())
		{
			CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(entity));

			damagedNetObj = static_cast<CNetObjPhysical*>(physical->GetNetworkObject());

			scriptAssertf(physical->GetNetworkObject(), "%s:NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY - entity doesn't have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	if (scriptVerifyf(damagedNetObj, "%s:NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(damagedNetObj->IsTrackingDamage(), "%s:NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY - the entity (%s) is not tracking damage", CTheScripts::GetCurrentScriptNameAndProgramCounter(), damagedNetObj->GetLogName()))
		{
			damageDealt = static_cast<int>(damagedNetObj->GetDamageDealtByPlayer(*damagerPlayer));
			assistedDamage = (damageDealt > 0);
		}
	}

	return assistedDamage;
}

bool CommandNetworkHasPlayerCollectedPickup( int PlayerIndex, int PickupID)
{
    bool bResult = false;

	CPed * pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		
	if (pPlayerPed)
	{
		CPed* pCollector = NULL;

		CGameScriptObjInfo objInfo(CGameScriptId(*CTheScripts::GetCurrentGtaScriptThread()), PickupID, 0);

		if (CPickupManager::HasPickupBeenCollected(objInfo, &pCollector) && pCollector==pPlayerPed)
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_PLAYER_COLLECTED_PICKUP returning TRUE");
			bResult = true;
		}
		else
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_PLAYER_COLLECTED_PICKUP returning FALSE");
			bResult = false;
		}
	}

    return(bResult);
}

bool CommandNetworkHasEntityBeenRegisteredWithThisThread(int entityIndex)
{
	const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		return CTheScripts::HasEntityBeenRegisteredWithCurrentThread(pEntity);
	}

	return false;
}

int CommandNetworkGetNetworkIdFromEntity(int entityIndex)
{
    const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
    int networkId = NETWORK_INVALID_OBJECT_ID;

	if (pEntity)
	{
		if (scriptVerifyf(CTheScripts::HasEntityBeenRegisteredWithCurrentThread(pEntity), "%s:NETWORK_GET_NETWORK_ID_FROM_ENTITY - entity is not registered with this script. You must call NETWORK_HAS_ENTITY_BEEN_REGISTERED_WITH_THIS_THREAD first", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			networkId = CTheScripts::GetIdForEntityRegisteredWithCurrentThread(pEntity);
		}
	}
 
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_NETWORK_ID_FROM_ENTITY");

    return networkId;
}

int CommandNetworkGetEntityFromNetworkId(int networkId)
{
	int entityIndex = NULL_IN_SCRIPTING_LANGUAGE;

	Assertf(networkId != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_GET_ENTITY_FROM_NETWORK_ID - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

	if (scriptVerifyf(pPhysical, "%s:NETWORK_GET_ENTITY_FROM_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		entityIndex = CTheScripts::GetGUIDFromEntity(*pPhysical);
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_ENTITY_FROM_NETWORK_ID");

	return entityIndex;
}

bool CommandNetworkGetEntityIsNetworked(int entityId)
{
	netObject* networkObj = NULL;

	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (entity)
	{
		if (scriptVerifyf(entity->GetIsPhysical(), "%s:NETWORK_GET_ENTITY_IS_NETWORKED - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(entity));
			networkObj = physical ? physical->GetNetworkObject() : NULL;
		}
	}

	return (networkObj ? true : false);
}

bool CommandNetworkGetEntityIsLocal(int entityId)
{
	netObject* networkObj = NULL;

	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (entity && entity->GetIsPhysical())
	{
		CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(entity));
		networkObj = physical ? physical->GetNetworkObject() : NULL;

		if (networkObj && (networkObj->IsClone() || networkObj->IsPendingOwnerChange()))
		{
			return false;
		}
	}

	return true;
}

void CommandNetworkRegisterEntityAsNetworked(int entityId)
{
	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (entity)
	{
		if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_REGISTER_ENTITY_AS_NETWORKED - a network game is not in progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerifyf(entity->GetIsPhysical(), "%s:NETWORK_REGISTER_ENTITY_AS_NETWORKED - the entity is not a physical object (ie a ped, vehicle or object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if (scriptVerifyf(!NetworkUtils::GetNetworkObjectFromEntity(entity), "%s:NETWORK_REGISTER_ENTITY_AS_NETWORKED - the entity is already networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					bool bCanRegister = false;

					CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(entity));

					if (physical->GetIsTypeObject())
					{
						CObject* pObject = (CObject*)physical;

						if (scriptVerifyf(CNetObjObject::CanBeNetworked(pObject, false, true), "%s:NETWORK_REGISTER_ENTITY_AS_NETWORKED - this object cannot be networked (Model: %s, Ownedby: %d, Has dummy: %s, Pickup:%s, Door:%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName(), pObject->GetOwnedBy(), pObject->GetRelatedDummy() ? "true" : "false", pObject->m_nObjectFlags.bIsPickUp ? "true" : "false",  pObject->IsADoor() ? "true" : "false"))
						{
							bCanRegister = true;
						}
					}
					else if (physical->GetIsTypePed() || physical->GetIsTypeVehicle())
					{
						bCanRegister = true;
					}

					if (bCanRegister)
					{
						NetworkInterface::RegisterObject(physical);
					}
				}
			}
		}
	}
}

void CommandNetworkUnregisterNetworkedEntity(int entityId)
{
	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (entity)
	{
		netObject *pNetObj = NetworkUtils::GetNetworkObjectFromEntity(entity);

		if (scriptVerifyf(pNetObj, "%s:NETWORK_UNREGISTER_NETWORKED_ENTITY - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CScriptEntityExtension* pExtension = entity->GetExtension<CScriptEntityExtension>();

			if (scriptVerifyf(!pExtension, "%s:NETWORK_UNREGISTER_NETWORKED_ENTITY - you cannot unregister script entities", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if (scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s:NETWORK_UNREGISTER_NETWORKED_ENTITY - the entity is not locally controlled", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					NetworkInterface::GetObjectManager().UnregisterNetworkObject(pNetObj, CNetworkObjectMgr::SCRIPT_UNREGISTRATION, false, false);
				}
			}
		}
	}
}

bool CommandNetworkDoesNetworkIdExist(int networkId)
{
	return CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId)) != 0;
}

bool CommandNetworkDoesEntityExistWithNetworkId(int networkId)
{
	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

	if (pPhysical)
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_DOES_ENTITY_EXIST_WITH_NETWORK_ID returning TRUE");
		return true;
	}
	else
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_DOES_ENTITY_EXIST_WITH_NETWORK_ID returning FALSE");
		return false;
	}
}

bool CommandNetworkRequestControlOfNetworkId(int networkId)
{
    CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

    if (scriptVerifyf(pPhysical, "%s:NETWORK_REQUEST_CONTROL_OF_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
	    netObject *networkObject = pPhysical->GetNetworkObject();

	    if (!networkObject || (!networkObject->IsClone() && !networkObject->IsPendingOwnerChange()))
	    {
		    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_NETWORK_ID returning TRUE");
		    return true;
	    }

	    Assertf(networkObject->GetObjectType() != NET_OBJ_TYPE_PLAYER, "%s:NETWORK_REQUEST_CONTROL_OF_NETWORK_ID - cannot request control of a player", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	    if(networkObject->IsClone())
	    {
		    CRequestControlEvent::Trigger(networkObject  NOTFINAL_ONLY(, "NETWORK_REQUEST_CONTROL_OF_NETWORK_ID"));
#if !__FINAL 
			NetworkInterface::GetEventManager().GetLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptDebugf1("%s: NETWORK_REQUEST_CONTROL_OF_NETWORK_ID requesting control of %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName()); 
			scrThread::PrePrintStackTrace(); 
#endif // !__FINAL

	    }
    }
  
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_NETWORK_ID returning FALSE");
    return false;
}

bool CommandNetworkHasControlOfNetworkId(int networkId)
{
	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

	if (scriptVerifyf(pPhysical, "%s:NETWORK_HAS_CONTROL_OF_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		netObject *networkObject = pPhysical->GetNetworkObject();

		if (!networkObject || (!networkObject->IsClone() && !networkObject->IsPendingOwnerChange() && !networkObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED)))
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_CONTROL_OF_NETWORK_ID returning TRUE");
			return true;
		}
		else
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_CONTROL_OF_NETWORK_ID returning FALSE");
			return false;
		}
	}

	return false;
}

bool CommandIsNetworkIdRemotelyControlled(int networkId)
{
	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

	if (scriptVerifyf(pPhysical, "%s:NETWORK_IS_NETWORK_ID_REMOTELY_CONTROLLED - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		netObject *networkObject = pPhysical->GetNetworkObject();

		if (networkObject && networkObject->IsClone())
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_IS_NETWORK_ID_REMOTELY_CONTROLLED returning TRUE");
			return true;
		}
		else
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_IS_NETWORK_ID_REMOTELY_CONTROLLED returning FALSE");
			return false;
		}
	}

	return false;
}

bool CommandNetworkRequestControlOfEntity(int entityIndex)
{
    if(scriptVerifyf(!NetworkInterface::IsInSpectatorMode(), "%s:NETWORK_REQUEST_CONTROL_OF_ENTITY - This command is not allowed when in spectator mode!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
	    const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	    if (pEntity)
	    {
		    netObject *networkObject = pEntity->GetNetworkObject();

		    if (!networkObject || (!networkObject->IsClone() && !networkObject->IsPendingOwnerChange()))
		    {
			    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_ENTITY returning TRUE");
			    return true;
		    }

		    if (networkObject->IsClone())
		    {
			    if (pEntity->GetIsTypePed() && static_cast<const CPed*>(pEntity)->GetIsInVehicle() && !static_cast<CNetObjPed*>(networkObject)->IsScriptObject())
			    {
				    scriptAssertf(0, "%s:NETWORK_REQUEST_CONTROL_OF_ENTITY - can't request control of ambient peds in vehicles!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			    }
			    else
			    {
				    CRequestControlEvent::Trigger(networkObject NOTFINAL_ONLY(, "NETWORK_REQUEST_CONTROL_OF_ENTITY"));
#if !__FINAL 
					NetworkInterface::GetEventManager().GetLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					scriptDebugf1("%s: NETWORK_REQUEST_CONTROL_OF_ENTITY requesting control of %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName()); 
					scrThread::PrePrintStackTrace(); 
#endif // !__FINAL
			    }
		    }
	    }
    }

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_ENTITY returning FALSE");
	return false;
}

bool CommandNetworkRequestControlOfDoor(int doorEnumHash)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "NETWORK_REQUEST_CONTROL_OF_DOOR : Door hash supplied failed find a door");

	if (pDoor)
	{
		netObject *networkObject = pDoor->GetNetworkObject();

		if (!networkObject || (!networkObject->IsClone() && !networkObject->IsPendingOwnerChange()))
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_DOOR returning TRUE");
			return true;
		}

		if (networkObject->IsClone())
		{
			CRequestControlEvent::Trigger(networkObject  NOTFINAL_ONLY(, "NETWORK_REQUEST_CONTROL_OF_DOOR"));
#if !__FINAL 
			NetworkInterface::GetEventManager().GetLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptDebugf1("%s: NETWORK_REQUEST_CONTROL_OF_DOOR requesting control of %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName()); 
			scrThread::PrePrintStackTrace(); 
#endif // !__FINAL
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_REQUEST_CONTROL_OF_DOOR returning FALSE");
	return false;
}

bool CommandNetworkHasControlOfEntity(int entityIndex)
{
	const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (!networkObject || (!networkObject->IsClone() && !networkObject->IsPendingOwnerChange()))
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_CONTROL_OF_ENTITY returning TRUE");
			return true;
		}
		else
		{
			WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_HAS_CONTROL_OF_ENTITY returning FALSE");
			return false;
		}
	}

	return false;
}

bool CommandNetworkHasControlOfPickup(int PickupID)
{
	CGameScriptObjInfo objInfo(CGameScriptId(*CTheScripts::GetCurrentGtaScriptThread()), PickupID, 0);

	CPickupPlacement* pPlacement = CPickupManager::GetPickupPlacementFromScriptInfo(objInfo);

	if (SCRIPT_VERIFY(pPlacement, "NETWORK_HAS_CONTROL_OF_PICKUP - Pickup does not exist"))
	{
		return !pPlacement->IsNetworkClone();
	}

	return false;
}

bool CommandNetworkHasControlOfDoor(int doorEnumHash)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	if (scriptVerifyf(pDoor, "NETWORK_HAS_CONTROL_OF_DOOR : Door hash supplied (%d) failed find a door", doorEnumHash))
	{
		if (pDoor->GetNetworkObject() && (pDoor->GetNetworkObject()->IsClone() || pDoor->GetNetworkObject()->IsPendingOwnerChange()))
		{
			return false;
		}
	}

	return true;
}

bool CommandIsDoorNetworked(int doorEnumHash)
{
    CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

    if (scriptVerifyf(pDoor, "NETWORK_IS_DOOR_NETWORKED : Door hash supplied (%d) failed find a door", doorEnumHash))
    {
        if(pDoor->GetNetworkObject())
        {
            return true;
        }
    }

    return false;
}

void CommandNetworkGetTeamRGBColour(int team, int &Red, int &Green, int &Blue)
{
    Assertf(team >= 0, "%s:NETWORK_GET_TEAM_RGB_COLOUR - Team num < 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf(team < MAX_NUM_TEAMS, "%s:NETWORK_GET_TEAM_RGB_COLOUR - Team num too high!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    Color32 teamColour;
    
    if(NetworkColours::GetTeamColour(team) == NetworkColours::NETWORK_COLOUR_CUSTOM)
    {
        teamColour = NetworkColours::GetCustomTeamColour(team);
    }
    else
    {
        teamColour = NetworkColours::GetNetworkColour(NetworkColours::GetTeamColour(team));
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_TEAM_RGB_COLOUR");

    Red   = teamColour.GetRed();
    Green = teamColour.GetGreen();
    Blue  = teamColour.GetBlue();
}

void CommandNetworkSetTeamRGBColour(int team, int Red, int Green, int Blue)
{
    Assertf(team >= 0, "%s:NETWORK_SET_TEAM_RGB_COLOUR - Team num < 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf(team < MAX_NUM_TEAMS, "%s:NETWORK_SET_TEAM_RGB_COLOUR - Team num too high!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf((Red >= 0 && Red <= 255),     "%s:NETWORK_SET_TEAM_RGB_COLOUR - Red colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((Green >= 0 && Green <= 255), "%s:NETWORK_SET_TEAM_RGB_COLOUR - Green colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((Blue >= 0 && Blue <= 255),   "%s:NETWORK_SET_TEAM_RGB_COLOUR - Blue colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(team >= 0 && team < MAX_NUM_TEAMS)
    {
        NetworkColours::SetCustomTeamColour(team, Color32(Red, Green, Blue, 255));
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TEAM_RGB_COLOUR");
}

void CommandNetworkGetLocalHandle(int& handleData, int sizeOfData)
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(scriptVerifyf(pInfo, "%s:NETWORK_GET_LOCAL_HANDLE - Local player not signed in! Check this first!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		CTheScripts::ExportGamerHandle(&pInfo->GetGamerHandle(), handleData, sizeOfData ASSERT_ONLY(, "NETWORK_GET_LOCAL_HANDLE"));
}

void CommandNetworkHandleFromUserID(const char* szUserID, int& handleData, int sizeOfData)
{
	rlGamerHandle hGamer;
	hGamer.FromUserId(szUserID);
	CTheScripts::ExportGamerHandle(&hGamer, handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_USER_ID"));
}

void CommandNetworkHandleFromMemberID(const char* szUserID, int& handleData, int sizeOfData)
{
	rlGamerHandle hGamer;
	if(!scriptVerifyf(hGamer.FromUserId(szUserID), "%s:NETWORK_HANDLE_FROM_MEMBER_ID - Invalid User ID: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szUserID))
		return;
	CTheScripts::ExportGamerHandle(&hGamer, handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_MEMBER_ID"));
}

void CommandNetworkHandleFromString(const char* szHandleString, int& handleData, int sizeOfData)
{
	rlGamerHandle hGamer;
	hGamer.FromString(szHandleString);
	CTheScripts::ExportGamerHandle(&hGamer, handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_STRING"));
}

void CommandNetworkHandleFromPlayer(int playerIndex, int& handleData, int sizeOfData)
{
	// when not in a network game, allow this function to retrieve local player
	const rlGamerInfo* pInfo = NULL;
	if(NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		scriptAssertf(pPlayer, "%s:NETWORK_HANDLE_FROM_PLAYER - Player with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);
		if(pPlayer)
			pInfo = &pPlayer->GetGamerInfo();
	}
	else
	{
		// verify that player index is valid and that the local player is signed in
		scriptAssertf(playerIndex <= 0, "%s:NETWORK_HANDLE_FROM_PLAYER - Invalid single player player index of %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);
		
		// use the active gamer info (validate that we're signed in)
		pInfo = NetworkInterface::GetActiveGamerInfo();
		if(!scriptVerifyf(pInfo, "%s:NETWORK_HANDLE_FROM_PLAYER - Local player not signed in! Check this first!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;
	}

	// we should have a valid info
	scriptAssertf(pInfo, "%s:NETWORK_HANDLE_FROM_PLAYER - No valid player info, playerIndex: %d, GameInProgress: %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, NetworkInterface::IsGameInProgress() ? "True" : "False");

	// if we have a valid gamer info
	if(pInfo)
		CTheScripts::ExportGamerHandle(&pInfo->GetGamerHandle(), handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_PLAYER"));
}

int CommandNetworkHashFromPlayerHandle(int playerIndex)
{
	// when not in a network game, allow this function to retrieve local player
	const rlGamerInfo* pInfo = NULL;
	if(NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		scriptAssertf(pPlayer, "%s:NETWORK_HASH_FROM_PLAYER_HANDLE - Player with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);
		if(pPlayer)
			pInfo = &pPlayer->GetGamerInfo();
	}
	else
	{
		// verify that player index is valid and that the local player is signed in
		scriptAssertf(playerIndex <= 0, "%s:NETWORK_HASH_FROM_PLAYER_HANDLE - Invalid single player player index of %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);

		// use the active gamer info (validate that we're signed in)
		pInfo = NetworkInterface::GetActiveGamerInfo();
		if(!scriptVerifyf(pInfo, "%s:NETWORK_HASH_FROM_PLAYER_HANDLE - Local player not signed in! Check this first!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return 0;
	}

	// we should have a valid info
	scriptAssertf(pInfo, "%s:NETWORK_HASH_FROM_PLAYER_HANDLE - No valid player info, playerIndex: %d, GameInProgress: %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, NetworkInterface::IsGameInProgress() ? "True" : "False");

	if(pInfo)
	{
		// convert gamer handle to a user ID
		char szUserID[RL_MAX_USERID_BUF_LENGTH];
		pInfo->GetGamerHandle().ToUserId(szUserID);

		// return hash of the user ID
		return static_cast<int>(atDataHash(szUserID, strlen(szUserID)));
	}

	// no info
	return 0;
}

int CommandNetworkHashFromGamerHandle(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HASH_FROM_GAMER_HANDLE")))
	{
		// convert gamer handle to a user ID
		char szUserID[RL_MAX_USERID_BUF_LENGTH];
		hGamer.ToUserId(szUserID);

		// return hash of the user ID
		return static_cast<int>(atDataHash(szUserID, strlen(szUserID)));
	}
	return 0;
}

void CommandNetworkHandleFromFriend(int friendIndex, int& handleData, int sizeOfData)
{
	rlGamerHandle hGamer;
	if (friendIndex >= CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		scriptAssertf(friendIndex < CLiveManager::GetFriendsPage()->m_NumFriends, "%s:NETWORK_HANDLE_FROM_FRIEND - Friend with index (%d) not found (index not valid).", CTheScripts::GetCurrentScriptNameAndProgramCounter(), friendIndex);
		return;
	}

	CLiveManager::GetFriendsPage()->m_Friends[friendIndex].GetGamerHandle(&hGamer);
	if (hGamer.IsValid())
	{
		CTheScripts::ExportGamerHandle(&hGamer, handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_FRIEND"));
	}
	else
	{
		scriptAssertf(hGamer.IsValid(), "%s:NETWORK_HANDLE_FROM_FRIEND - Friend with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), friendIndex);
	}
}

void CommandNetworkHandleFromMetPlayer(int recentPlayerIndex, int& handleData, int sizeOfData)
{
	CNetworkRecentPlayers& recentPlayers = CNetwork::GetNetworkSession().GetRecentPlayers();
	if(scriptVerifyf(recentPlayerIndex >= 0 && static_cast<unsigned>(recentPlayerIndex) < recentPlayers.GetNumRecentPlayers(), "Invalid index!"))
		CTheScripts::ExportGamerHandle(recentPlayers.GetRecentPlayerHandle(recentPlayerIndex), handleData, sizeOfData ASSERT_ONLY(, "NETWORK_HANDLE_FROM_MET_PLAYER"));
}

bool CommandNetworkIsHandleValid(int& handleData, int sizeOfData)
{
	// script returns size in script words
	sizeOfData *= sizeof(scrValue); 
	if(sizeOfData != (SCRIPT_GAMER_HANDLE_SIZE * sizeof(scrValue)))
	{
		return false;
	}

	// buffer to write our handle into
	u8* pHandleBuffer = reinterpret_cast<u8*>(&handleData);

	// implement a copy of the import function
	datImportBuffer bb;
	bb.SetReadOnlyBytes(pHandleBuffer, sizeOfData * 4);

	u8 nService = rlGamerHandle::SERVICE_INVALID;
	bool bIsValid = bb.ReadUns(nService, 8);

	bool bValidService = bIsValid &&
#if RLGAMERHANDLE_XBL
		(nService == rlGamerHandle::SERVICE_XBL);
#elif RLGAMERHANDLE_NP
		(nService == rlGamerHandle::SERVICE_NP);
#elif RLGAMERHANDLE_SC
		(nService == rlGamerHandle::SERVICE_SC);
#else
		false;
#endif

	// if we have a valid service
	if(bValidService)
	{
		// retrieve gamer handle
		rlGamerHandle hGamer;
		unsigned nImported = 0;
		hGamer.Import(pHandleBuffer, sizeOfData * 4, &nImported);
		return hGamer.IsValid();
	}

	// invalid service
	return false;
}

bool CommandNetworkLogGamerHandle(int& OUTPUT_ONLY(handleData))
{
#if !__NO_OUTPUT
	if(!CommandNetworkIsHandleValid(handleData, SCRIPT_GAMER_HANDLE_SIZE))
	{
		scriptErrorf("%s:NETWORK_LOG_GAMER_HANDLE - INVALID.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		gnetDebug1("[SCRIPT | NETWORK_LOG_GAMER_HANDLE] - INVALID.");
		return false;
	}

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_LOG_GAMER_HANDLE")))
	{
		scriptErrorf("%s:NETWORK_LOG_GAMER_HANDLE - IMPORT FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		gnetDebug1("[SCRIPT | NETWORK_LOG_GAMER_HANDLE] - IMPORT FAILED");
		return false;
	}

	scriptDebugf1("%s:NETWORK_LOG_GAMER_HANDLE - handle='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), NetworkUtils::LogGamerHandle(hGamer));

	gnetDebug1("[SCRIPT | NETWORK_LOG_GAMER_HANDLE] - %s", NetworkUtils::LogGamerHandle(hGamer));

	return true;
#else
	return true;
#endif
}

bool CommandNetworkGamerTagFromHandleStart(int& XBOX_ONLY(handleData) WIN32PC_ONLY(handleData))
{
#if RSG_XBOX || RSG_PC
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_GAMERTAG_FROM_HANDLE_START - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	scriptAssertf(!CLiveManager::GetFindGamerTag().Pending(), "%s:NETWORK_GAMERTAG_FROM_HANDLE_START - Pending operations.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (CLiveManager::GetFindGamerTag().Pending())
		return false;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GAMERTAG_FROM_HANDLE_START")))
		return false;

	scriptAssertf(hGamer.IsValid(), "%s:NETWORK_GAMERTAG_FROM_HANDLE_START - Handle is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!hGamer.IsValid())
		return false;

	return scriptVerifyf(CLiveManager::GetFindGamerTag().Start(hGamer), "%s:NETWORK_GAMERTAG_FROM_HANDLE_START - Failed to start.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#else
	scriptAssertf(0, "%s:NETWORK_GAMERTAG_FROM_HANDLE_START - Command is only valid in 360, Xbox One and PC, use directly NETWORK_GET_GAMERTAG_FROM_HANDLE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool  CommandNetworkGamerTagFromHandlePending()
{
#if RSG_XBOX || RSG_PC
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_GAMERTAG_FROM_HANDLE_PENDING - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	return CLiveManager::GetFindGamerTag().Pending();
#else
	scriptAssertf(0, "%s:NETWORK_GAMERTAG_FROM_HANDLE_PENDING - Command is only valid in 360, Xbox One and PC, use directly NETWORK_GET_GAMERTAG_FROM_HANDLE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool  CommandNetworkGamerTagFromHandleSucceeded()
{
#if RSG_XBOX || RSG_PC
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_GAMERTAG_FROM_HANDLE_SUCCEEDED - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	return CLiveManager::GetFindGamerTag().Succeeded();
#else
	scriptAssertf(0, "%s:NETWORK_GAMERTAG_FROM_HANDLE_SUCCEEDED - Command is only valid in 360, Xbox One and PC, use directly NETWORK_GET_GAMERTAG_FROM_HANDLE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

const char* CommandNetworkGetGamerTagFromHandle(int& handleData)
{
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_GET_GAMERTAG_FROM_HANDLE - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return "";

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_GAMERTAG_FROM_HANDLE")))
		return "";

	scriptAssertf(hGamer.IsValid(), "%s:NETWORK_GET_GAMERTAG_FROM_HANDLE - Handle is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!hGamer.IsValid())
		return "";

#if RSG_NP
	static char s_userIdStr[128];
	s_userIdStr[0] = '\0';

	hGamer.ToUserId(s_userIdStr);

	return s_userIdStr;
#elif RSG_XBOX || RSG_PC
	scriptAssertf(!CLiveManager::GetFindGamerTag().Pending(), "%s:NETWORK_GET_GAMERTAG_FROM_HANDLE - Operation pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (CLiveManager::GetFindGamerTag().Pending())
		return "";

	scriptAssertf(CLiveManager::GetFindGamerTag().Succeeded(), "%s:NETWORK_GET_GAMERTAG_FROM_HANDLE - Operation did not Succeeded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(!CLiveManager::GetFindGamerTag().Succeeded())
		return "";

	return CLiveManager::GetFindGamerTag().GetGamerTag(hGamer);
#else
	return "";
#endif
}

int CommandNetworkDisplayNamesFromHandlesStart(scrArrayBaseAddr& DURANGO_ONLY(gamerHandles)ORBIS_ONLY(gamerHandles), const int DURANGO_ONLY(numGamers)ORBIS_ONLY(numGamers))
{
#if RSG_DURANGO || RSG_ORBIS
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_DISPLAYNAMES_FROM_HANDLES_START - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(!NetworkInterface::IsLocalPlayerOnline())
	{
		return CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}

	scriptAssertf(numGamers <= CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST, "%s:NETWORK_DISPLAYNAMES_FROM_HANDLES_START - Can request at most %u display names at a time.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST);
	if(numGamers > CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST)
	{
		return CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}

	u8* pData = scrArray::GetElements<u8>(gamerHandles);
	if(!scriptVerify(pData))
	{
		return CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}

	rlGamerHandle ahGamers[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST];
	for(u32 i = 0; i < numGamers; i++, pData += GAMER_HANDLE_SIZE)
	{
		if(!CTheScripts::ImportGamerHandle(&ahGamers[i], pData ASSERT_ONLY(, "NETWORK_DISPLAYNAMES_FROM_HANDLES_START")))
		{
			return CDisplayNamesFromHandles::INVALID_REQUEST_ID;
		}
	}

	int requestId = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), ahGamers, numGamers);
	scriptAssertf(requestId != CDisplayNamesFromHandles::INVALID_REQUEST_ID, "%s:NETWORK_DISPLAYNAMES_FROM_HANDLES_START - out of request ids.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return requestId;
#else
	scriptAssertf(0, "%s:NETWORK_DISPLAYNAMES_FROM_HANDLES_START - Command is only valid on Xbox One or PS 4", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return -1;
#endif
}

int CommandNetworkGetDisplayNamesFromHandles(int DURANGO_ONLY(requestId)ORBIS_ONLY(requestId), scrArrayBaseAddr& DURANGO_ONLY(nameBase)ORBIS_ONLY(nameBase), const int DURANGO_ONLY(maxDisplayNames)ORBIS_ONLY(maxDisplayNames))
{
#if RSG_DURANGO || RSG_ORBIS
	// do this first so we clear the pending state even if we early out below
	rlDisplayName displayNames[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST] = {0};
	int result = CLiveManager::GetFindDisplayName().GetDisplayNames(requestId, displayNames, CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST);

	scriptAssertf(requestId != CDisplayNamesFromHandles::INVALID_REQUEST_ID, "%s:NETWORK_GET_DISPLAYNAMES_FROM_HANDLES - invalid request id.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_GET_DISPLAYNAMES_FROM_HANDLES - Local Gamer is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(!NetworkInterface::IsLocalPlayerOnline())
	{
		return CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED;
	}

	scriptAssertf(maxDisplayNames <= CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST, "%s:NETWORK_GET_DISPLAYNAMES_FROM_HANDLES - Can request at most %u display names at a time.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST);
	if(maxDisplayNames > CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST)
	{
		return CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED;
	}
	
	scrTextLabel63* pNameData = scrArray::GetElements<scrTextLabel63>(nameBase);
	if(!scriptVerify(pNameData))
	{
		return CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED;
	}

	CompileTimeAssert(sizeof(scrTextLabel63) >= RL_MAX_DISPLAY_NAME_BUF_SIZE);

	if(result == CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED)
	{
		for(u32 i = 0; i < maxDisplayNames; i++)
		{
			safecpy(pNameData[i], displayNames[i], sizeof(scrTextLabel63));
		}
	}

	return result;
#else
	scriptAssertf(0, "%s:NETWORK_GET_DISPLAYNAMES_FROM_HANDLES - Command is only valid on Xbox One and PlayStation 4.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandNetworkAreHandlesTheSame(int& handleData1, int& handleData2)
{
#if __BANK
	if(!CommandNetworkIsHandleValid(handleData1, SCRIPT_GAMER_HANDLE_SIZE))
	{
		scriptAssertf(0, "%s:NETWORK_ARE_HANDLES_THE_SAME - Handle1 is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}
	if(!CommandNetworkIsHandleValid(handleData2, SCRIPT_GAMER_HANDLE_SIZE))
	{
		scriptAssertf(0, "%s:NETWORK_ARE_HANDLES_THE_SAME - Handle2 is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}
#endif // __BANK

	rlGamerHandle hGamer1, hGamer2;
	if(!CTheScripts::ImportGamerHandle(&hGamer1, handleData1, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_ARE_HANDLES_THE_SAME")))
		return false;
	if(!CTheScripts::ImportGamerHandle(&hGamer2, handleData2, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_ARE_HANDLES_THE_SAME")))
		return false;

	return (hGamer1 == hGamer2);
}

int CommandNetworkGetPlayerIndexFromGamerHandle(int& handleData)
{
	if(!scriptVerifyf(NetworkInterface::IsNetworkOpen(), "%s:NETWORK_GET_PLAYER_FROM_GAMER_HANDLE - Network is not open!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return -1;

	// validate handle data
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_PLAYER_FROM_GAMER_HANDLE")))
		return -1; 

	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();
	
	// find matching player
	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
		if(pPlayer->GetGamerInfo().GetGamerHandle() == hGamer)
		{
			int playerIndex = pPlayer->GetPhysicalPlayerIndex();
			if(playerIndex == INVALID_PLAYER_INDEX) playerIndex = -1;
			return playerIndex;
		}
	}

	// not in session
	return -1; 
}

const char* CommandNetworkMemberIDFromGamerHandle(int& handleData)
{
	// validate handle data
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_MEMBER_ID_FROM_GAMER_HANDLE")))
		return "Invalid"; 

	// convert to member ID
	static const unsigned kMAX_NAME = 32; 
	static char szMemberID[kMAX_NAME];
	hGamer.ToUserId(szMemberID, kMAX_NAME);

	return szMemberID;
}

bool CommandNetworkIsGamerInMySession(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_GAMER_IN_MY_SESSION")))
		return CNetwork::GetNetworkSession().IsSessionMember(hGamer);

	// invalid handle, not in session
	return false;
}

void CommandNetworkShowProfileUI(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SHOW_PROFILE_UI")))
		CLiveManager::ShowGamerProfileUi(hGamer);
}

void CommandNetworkShowFeedbackUI(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SHOW_FEEDBACK_UI")))
		CLiveManager::ShowGamerFeedbackUi(hGamer);
}

int CommandNetworkGetGameRegion()
{
	return rage::rlGetGameRegion();
}

const char* CommandNetworkPlayerGetName(int playerIndex)
{
	if (NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);
		scriptAssertf(player, "%s:NETWORK_PLAYER_GET_NAME - Player with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);

		return player ? player->GetGamerInfo().GetName() : "**Invalid**";
	}
	else if(CGameWorld::GetMainPlayerInfo())
	{
		if (scriptVerifyf(playerIndex == 0, "%s:%s - Player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "NETWORK_PLAYER_GET_NAME"))
		{
			return CGameWorld::GetMainPlayerInfo()->m_GamerInfo.GetName();
		}
	}

	return "**Invalid**";
}

const char* CommandNetworkPlayerGetUserId(int playerIndex, int &AddressTemp)
{
	if (NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);
		scriptAssertf(player, "%s:NETWORK_PLAYER_GET_USERID - Player with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);

		u8* Address = reinterpret_cast< u8* >(&AddressTemp);

		const char* result = player ? player->GetGamerInfo().GetGamerHandle().ToUserId((char*)Address, sizeof(char[24])) : "**Invalid**";

		scriptDebugf1("%s:NETWORK_PLAYER_GET_USERID - Player with index (%d) has user id - %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, result);

		return result;
	}
	else if(CGameWorld::GetMainPlayerInfo())
	{
		if (scriptVerifyf(playerIndex == 0, "%s:%s - Player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "NETWORK_PLAYER_GET_USERID"))
		{
			u8* Address = reinterpret_cast< u8* >(&AddressTemp);

			const char* result = CGameWorld::GetMainPlayerInfo()->m_GamerInfo.GetGamerHandle().ToUserId((char*)Address, sizeof(char[24]));

			scriptDebugf1("%s:NETWORK_PLAYER_GET_USERID - Player with index (%d) has user id - %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, result);

			return result;
		}
	}

	return "**Invalid**";
}

int CommandNetworkEntityGetObjectId(int entityId)
{
	int id = NETWORK_INVALID_OBJECT_ID;

	const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (entity)
	{
		scriptAssertf(entity->GetIsPhysical(), "%s:NETWORK_ENTITY_GET_OBJECT_ID - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (entity->GetIsPhysical())
		{
			CPhysical* physical   = SafeCast(CPhysical, const_cast<CEntity*>(entity));
			netObject* networkObj = physical ? physical->GetNetworkObject() : NULL;

			scriptAssertf(networkObj, "%s:NETWORK_ENTITY_GET_OBJECT_ID - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if (networkObj)
			{
				id = (int)networkObj->GetObjectID();
			}
		}
	}

	return id;
}

int CommandNetworkGetEntityFromObjectId(int objectId)
{
	int id = -1;

	if (NetworkInterface::IsGameInProgress())
	{
		netObject* pNetObj = NetworkInterface::GetNetworkObject((ObjectId)objectId);

		CEntity* pEntity = pNetObj ? pNetObj->GetEntity() : NULL;

		if (pEntity)
		{
			id = CTheScripts::GetGUIDFromEntity(*pEntity);
		}
	}

	return id;
}

bool CommandNetworkPlayerIsRockstarDev(int nPlayerIndex)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PLAYER_IS_ROCKSTAR_DEV - Not in a network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(!scriptVerifyf(pPlayer, "%s:NETWORK_PLAYER_IS_ROCKSTAR_DEV - Invalid player index!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	return pPlayer->IsRockstarDev();
}

#if (RSG_CPU_X86 || RSG_CPU_X64) && RSG_PC
__declspec(noinline) bool CommandNetworkPlayerIsCheater(int nPlayerIndex)
#else
bool CommandNetworkPlayerIsCheater(int nPlayerIndex)
#endif
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PLAYER_INDEX_IS_CHEATER - Not in a network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(nPlayerIndex);
	if(!scriptVerifyf(pPlayer, "%s:NETWORK_PLAYER_INDEX_IS_CHEATER - Invalid player index!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;
	//@@: location NETWORKCOMMAND_COMMANDNETWORKPLAYERISCHEATER_PRE_CALL
	volatile bool isCheater = pPlayer->IsCheater();
	//@@: location NETWORKCOMMAND_COMMANDNETWORKPLAYERISCHEATER_POST_CALL
	return  isCheater;
}

int CommandNetworkGetMaxFriends()
{
	return rlFriendsPage::MAX_FRIEND_PAGE_SIZE;
}

int CommandNetworkGetFriendCount()
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_FRIEND_COUNT");
    return CLiveManager::GetFriendsPage()->m_NumFriends;
}

const char* CommandNetworkGetFriendName(const int friendIndex)
{
	if (friendIndex < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[friendIndex];
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_FRIEND_NAME");
		return f.GetName();
	}

	return "*****";
}

const char* CommandNetworkGetFriendDisplayName(const int friendIndex)
{
	if (friendIndex < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[friendIndex];
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_FRIEND_DISPLAY_NAME");
#if RSG_XBOX
		return f.GetDisplayName();
#else
		return f.GetName();
#endif
	}

	return "*****";
}

bool CommandNetworkIsFriendOnline(const char* friendName)
{
	const int nIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(friendName);
	if(!scriptVerifyf(nIndex >= 0, "%s:NETWORK_IS_FRIEND_ONLINE - No such friend %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), friendName))
		return false; 

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nIndex];
	const bool isOnline = pFriend.IsOnline();
	scriptDebugf1("%s - NETWORK_IS_FRIEND_ONLINE - Friend \"%s\" is \"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFriend.GetName(), isOnline ? "Online" : "Not Online");

	return isOnline;
}

bool CommandNetworkIsFriendHandleOnline(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_FRIEND_HANDLE_ONLINE")))
	{
		rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(hGamer);
		if(!scriptVerifyf(pFriend, "%s:NETWORK_IS_FRIEND_HANDLE_ONLINE - No such friend ", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			return false; 
		}
	
		bool isOnline = pFriend->IsOnline();
#if SESSION_PRESENCE_OVERRIDE
		bool bIsSameSession = CNetwork::GetNetworkSession().IsSessionMember(hGamer);
		bool bIsTransitionSession = CNetwork::GetNetworkSession().IsTransitionMember(hGamer);
		bool bIsInASession = bIsSameSession || bIsTransitionSession || pFriend->IsInSession();
		if (!isOnline && bIsInASession)
		{
			scriptWarningf("%s - NETWORK_IS_FRIEND_HANDLE_ONLINE - Friend \"%s\" is Not Online but in Session. Overriding IsOnline to TRUE.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFriend->GetName());
			isOnline = true;
		}
#endif

		scriptDebugf1("%s - NETWORK_IS_FRIEND_HANDLE_ONLINE - Friend \"%s\" is \"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFriend->GetName(), isOnline ? "Online" : "Not Online");
		return isOnline;

	}
	return false;
}

bool CommandNetworkIsFriendIndexOnline(const int nIndex)
{
	if (CLiveManager::GetFriendsPage()->m_NumFriends > nIndex)
	{
		const rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nIndex];
		scriptDebugf1("NETWORK_IS_FRIEND_INDEX_ONLINE - Friend \"%s\" is \"%s\".", pFriend.GetName(),  pFriend.IsOnline() ? "Online" : "Not Online");
		return pFriend.IsOnline();
	}

	scriptDebugf1("%s:NETWORK_IS_FRIEND_INDEX_ONLINE - Friend with index (%d) not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nIndex);
	return false;
}

bool CommandNetworkIsFriendInSameTitle(const char* friendName)
{
	const int nIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(friendName);
	if(!scriptVerifyf(nIndex >= 0, "%s:NETWORK_IS_FRIEND_IN_SAME_TITLE - No such friend %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), friendName))
		return false; 

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nIndex];
	bool isInSameTitle = pFriend.IsInSameTitle();

#if SESSION_PRESENCE_OVERRIDE
	if (!isInSameTitle && pFriend.IsInSession())
	{
		scriptWarningf("%s - NETWORK_IS_FRIEND_IN_SAME_TITLE - Friend \"%s\" is Not In Title but in Session. Overriding IsInSameTitle to TRUE.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFriend.GetName());
		isInSameTitle = true;
	}
#endif

	return isInSameTitle;
}

bool CommandNetworkIsFriendInMultiplayer(const char* friendName)
{
	const int nIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(friendName);
	if(!scriptVerifyf(nIndex >= 0, "%s:NETWORK_IS_FRIEND_IN_MULTIPLAYER - No such friend %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), friendName))
		return false; 

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nIndex];
	return pFriend.IsInSession();
}

bool CommandNetworkIsInactiveProfile(int& handleData)
{
    // make sure that handle is valid
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_INACTIVE_PROFILE")))
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if((static_cast<int>(i) != NetworkInterface::GetLocalGamerIndex()) && rlPresence::IsSignedIn(i))
            {
                rlGamerHandle hTemp; 
                rlPresence::GetGamerHandle(i, &hTemp);
                if(hGamer == hTemp)
                    return true;
            }
        }
    }

    // invalid handle
    return false; 
}

bool CommandNetworkIsFriend(int& handleData)
{
	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_FRIEND")))
	{
		return (rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), hGamer));
	}

	// invalid handle
	return false; 
}

bool CommandNetworkIsPendingFriend(int& handleData)
{
#if RSG_XBOX
	if (CLiveManager::IsSignedIn() && CLiveManager::IsOnline())
	{
		// make sure that handle is valid
		rlGamerHandle hGamer; 
		if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_PENDING_FRIEND")))
		{
			return rlFriendsManager::IsPendingFriend(NetworkInterface::GetLocalGamerIndex(), hGamer);
		}
	}
#else
	(void) handleData;
	scriptAssertf(0, "%s:NETWORK_IS_PENDING_FRIEND - Does not work on PS3/PS4/PC. Please do not use it", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

	// invalid handle
	return false; 
}

bool CommandNetworkIsAddingFriend()
{
	return CLiveManager::IsAddingFriend();
}

bool CommandNetworkAddFriend(int& handleData, const char* szMessage)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_ADD_FRIEND")))
		return CLiveManager::AddFriend(hGamer, szMessage);

	// invalid handle
	return false;
}

int CommandNetworkGetNumPlayersMet()
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_NUM_PLAYERS_MET");
	return CNetwork::GetNetworkSession().GetRecentPlayers().GetNumRecentPlayers();
}

const char* CommandNetworkGetMetPlayerName(const int recentPlayerIndex)
{
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_MET_PLAYER_NAME");

	CNetworkRecentPlayers& recentPlayers = CNetwork::GetNetworkSession().GetRecentPlayers();
	if(scriptVerifyf(recentPlayerIndex >= 0 && static_cast<unsigned>(recentPlayerIndex) < recentPlayers.GetNumRecentPlayers(), "Invalid index!"))
		return recentPlayers.GetRecentPlayerName(recentPlayerIndex);

	return "INVALID INDEX";
}

void CommandNetworkSetPlayerIsCorrupt(bool bIsCorrupt)
{
	if(!scriptVerifyf(FindPlayerPed(), "%s:NETWORK_SET_PLAYER_IS_CORRUPT - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetObjPlayer* pNetObjPlayer = SafeCast(CNetObjPlayer, FindPlayerPed()->GetNetworkObject());
	if(scriptVerifyf(pNetObjPlayer, "%s:NETWORK_SET_PLAYER_IS_CORRUPT - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		CNetObjPlayer::SetIsCorrupt(bIsCorrupt);
}

void CommandNetworkSetPlayerIsPassive(bool bIsPassive)
{
	if(!scriptVerifyf(FindPlayerPed(), "%s:NETWORK_SET_PLAYER_IS_PASSIVE - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetObjPlayer* pNetObjPlayer = SafeCast(CNetObjPlayer, FindPlayerPed()->GetNetworkObject());
	if(scriptVerifyf(pNetObjPlayer, "%s:NETWORK_SET_PLAYER_IS_PASSIVE - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		pNetObjPlayer->SetPassiveMode(bIsPassive);;
}

bool CommandNetworkGetPlayerOwnsWaypoint(int nPlayerIndex)
{
	CPed* pPlayerPed = CTheScripts::FindNetworkPlayerPed(nPlayerIndex);
	if(!scriptVerifyf(pPlayerPed, "%s:NETWORK_GET_PLAYER_OWNS_WAYPOINT - Player %d does not exist!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
		return false;

	CNetObjPlayer* pNOP = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());
	if(scriptVerifyf(pNOP, "%s:NETWORK_GET_PLAYER_OWNS_WAYPOINT - Player %d does not have a network object!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPlayerIndex))
		return pNOP->HasActiveWaypoint() && pNOP->OwnsWaypoint() && NetworkInterface::IsRelevantToWaypoint(pPlayerPed);

	return false;
}

bool CommandNetworkCanSetWaypoint()
{
	CPed* pPlayerPed = FindPlayerPed();

	if(!scriptVerifyf(pPlayerPed, "%s:NETWORK_CAN_SET_WAYPOINT - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	 return NetworkInterface::CanSetWaypoint(pPlayerPed);
}

void CommandNetworkIgnoreRemoteWaypoints()
{
	NetworkInterface::IgnoreRemoteWaypoints();
}

bool CommandNetworkIsPlayerOnBlocklist(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_PLAYER_ON_BLOCKLIST")))
		return CLiveManager::IsInBlockList(hGamer);

	return false;
}

void CommandNetworkSetScriptAutomuted(bool bIsAutoMuted)
{
    CNetwork::SetScriptAutoMuted(bIsAutoMuted);
}

bool CommandNetworkHasAutomuteOverride()
{
    return CLiveManager::GetSCGamerDataMgr().HasAutoMuteOverride();
}

bool CommandNetworkHasHeadset()
{
	if(!NetworkInterface::IsLocalPlayerSignedIn())
		return false; 

	return NetworkInterface::GetVoice().LocalHasHeadset(NetworkInterface::GetLocalGamerIndex());
}

void CommandSetLookAtTalkers(bool bLookAtTalkers)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_SET_LOOK_AT_TALKERS - Network game not in progress!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// this is a global flag for all players
	CNetObjPlayer::SetLookAtTalkers(bLookAtTalkers);
}

bool CommandNetworkIsLocalTalking()
{
	// check signed in
	if(!NetworkInterface::IsLocalPlayerSignedIn())
		return false;

	return NetworkInterface::GetVoice().IsLocalTalking(NetworkInterface::GetLocalGamerIndex());
}

bool CommandNetworkIsPushToTalkActive()
{
	// check signed in
	if(!NetworkInterface::IsLocalPlayerSignedIn())
		return false;

	return NetworkInterface::GetVoice().IsPushToTalkActive();
}

bool CommandNetworkGamerHasHeadset(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GAMER_HAS_HEADSET")))
        return NetworkInterface::GetVoice().GamerHasHeadset(hGamer);

    return false;
}

bool CommandNetworkIsGamerTalking(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_GAMER_TALKING")))
    {
        rlGamerInfo hInfo;
        if(CNetwork::GetNetworkSession().GetSessionMemberInfo(hGamer, &hInfo))
            return NetworkInterface::GetVoice().IsGamerTalking(hInfo.GetGamerId()); 
        else if(CNetwork::GetNetworkSession().GetTransitionMemberInfo(hGamer, &hInfo))
            return NetworkInterface::GetVoice().IsGamerTalking(hInfo.GetGamerId()); 
    }

    // not valid or not a member of the main / transition sessions
    return false;
}

bool CommandNetworkMuteGamer(int& handleData, const bool bIsMuted)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_MUTE_GAMER")))
        return NetworkInterface::GetVoice().MuteGamer(hGamer, bIsMuted);

    return false;
}

bool CommandNetworkHasGamerRecord(int& handleData)
{
	rlGamerHandle hGamer;
	if (CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_PERMISSIONS_HAS_GAMER_RECORD")))
	{
		CNetworkVoice& v = NetworkInterface::GetVoice();
		return v.HasGamerRecord(hGamer);
	}
	return false;
}

// Deprecated soon - script should move over to calling CanVoiceChatWithGamer and CanTextChatWithGamer depending on their context.
bool CommandNetworkCanCommunicateWithGamer(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_COMMUNICATE_WITH_GAMER")))
	{
		CNetworkVoice& v = NetworkInterface::GetVoice();
		return v.CanCommunicateTextWithGamer(hGamer) || v.CanCommunicateVoiceWithGamer(hGamer);
	}
    return false;
}

bool CommandNetworkCanVoiceChatWithGamer(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_VOICE_CHAT_WITH_GAMER")))
	{
		return NetworkInterface::GetVoice().CanCommunicateVoiceWithGamer(hGamer);
	}
	return false;
}

bool CommandNetworkCanTextChatWithGamer(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_TEXT_CHAT_WITH_GAMER")))
	{
		return NetworkInterface::GetVoice().CanCommunicateTextWithGamer(hGamer);
	}
	return false;
}

bool CommandNetworkIsGamerMutedByMe(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_GAMER_MUTED_BY_ME")))
        return NetworkInterface::GetVoice().IsGamerMutedByMe(hGamer);

    return false;
}

bool CommandNetworkAmIMutedByGamer(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_AM_I_MUTED_BY_GAMER")))
        return NetworkInterface::GetVoice().AmIMutedByGamer(hGamer);

    return false;
}

bool CommandNetworkIsGamerBlockedByMe(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_IS_GAMER_BLOCKED_BY_ME")))
        return NetworkInterface::GetVoice().IsGamerBlockedByMe(hGamer);

    return false;
}

bool CommandNetworkAmIBlockedByGamer(int& handleData)
{
    rlGamerHandle hGamer; 
    if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_AM_I_BLOCKED_BY_GAMER")))
        return NetworkInterface::GetVoice().AmIBlockedByGamer(hGamer);

    return false;
}

bool CommandNetworkCanViewGamerUserContent(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_VIEW_GAMER_USER_CONTENT")))
		return NetworkInterface::GetVoice().CanViewGamerUserContent(hGamer);

	return false;
}

bool CommandNetworkHasViewGamerUserContentResult(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_HAS_VIEW_GAMER_USER_CONTENT_RESULT")))
		return NetworkInterface::GetVoice().HasViewGamerUserContentResult(hGamer);

	return false;
}

bool CommandNetworkCanPlayMultiplayerWithGamer(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_PLAY_MULTIPLAYER_WITH_GAMER")))
		return NetworkInterface::GetVoice().CanPlayMultiplayerWithGamer(hGamer);

	return false;
}

bool CommandNetworkHasPlayMultiplayerWithGamerResult(int& UNUSED_PARAM(handleData))
{
	// we've swapped to using a block / avoid list for this - results always available
	return true;
}

bool CommandNetworkCanGamerPlayMultiplayerWithMe(int& handleData)
{
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_GAMER_PLAY_MULTIPLAYER_WITH_ME")))
		return NetworkInterface::GetVoice().CanGamerPlayMultiplayerWithMe(hGamer);

	return false;
}

bool CommandNetworkCanSendLocalInvite(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_SEND_LOCAL_INVITE")))
		return NetworkInterface::GetVoice().CanSendInvite(hGamer);

	return false;
}

bool CommandNetworkCanReceiveLocalInvite(int& handleData)
{
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CAN_RECEIVE_LOCAL_INVITE")))
		return NetworkInterface::GetVoice().CanReceiveInvite(hGamer);

	return false;
}

bool CommandNetworkIsPlayerTalking(const int playerIndex)
{
    netPlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_IS_PLAYER_TALKING");

    if (player)
    {
        return NetworkInterface::GetVoice().IsGamerTalking(player->GetGamerInfo().GetGamerId());
    }

    return false;
}

bool CommandNetworkPlayerHasHeadset(const int playerIndex)
{
	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().GamerHasHeadset(pPlayer->GetGamerInfo().GetGamerHandle());
    return false;
}

bool CommandNetworkSetPlayerMuted(const int playerIndex, const bool muted)
{
    netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().MuteGamer(pPlayer->GetGamerInfo().GetGamerHandle(), muted);
    return false;
}

// Will deprecate soon, maybe. Script should call CommandNetworkCanVoiceChat/TextChatWithPlayer instead depending on context.
bool CommandNetworkCanCommunicateWithPlayer(const int playerIndex)
{
	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
	{
		const rlGamerHandle& handle = pPlayer->GetGamerInfo().GetGamerHandle();
		CNetworkVoice& v = NetworkInterface::GetVoice();
		return v.CanCommunicateTextWithGamer(handle) || v.CanCommunicateVoiceWithGamer(handle);
	}
    return false;
}

bool CommandNetworkCanTextChatWithPlayer(const int playerIndex)
{
	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if(pPlayer)
	{
		return NetworkInterface::GetVoice().CanCommunicateTextWithGamer(pPlayer->GetGamerInfo().GetGamerHandle());
	}
	return false;
}

bool CommandNetworkCanVoiceChatWithPlayer(const int playerIndex)
{
	netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if(pPlayer)
	{
		return NetworkInterface::GetVoice().CanCommunicateVoiceWithGamer(pPlayer->GetGamerInfo().GetGamerHandle());
	}
	return false;
}

bool CommandNetworkIsPlayerMutedByMe(const int playerIndex)
{
    netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().IsGamerMutedByMe(pPlayer->GetGamerInfo().GetGamerHandle());
    return false;
}

bool CommandNetworkAmIMutedByPlayer(const int playerIndex)
{
    netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().AmIMutedByGamer(pPlayer->GetGamerInfo().GetGamerHandle());
    return false;
}

bool CommandNetworkIsPlayerBlockedByMe(const int playerIndex)
{
    netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().IsGamerBlockedByMe(pPlayer->GetGamerInfo().GetGamerHandle());
    return false;
}

bool CommandNetworkAmIBlockedByPlayer(const int playerIndex)
{
    netPlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
    if(pPlayer)
        return NetworkInterface::GetVoice().AmIBlockedByGamer(pPlayer->GetGamerInfo().GetGamerHandle());
    return false;
}

void CommandNetworkGetMuteCountForPlayer(const int playerIndex, int& out_muteCount, int& out_numTalkers)
{
	out_muteCount = 0;
	out_numTalkers = 0;
	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if(pPlayer)
	{
		u32 mute, talker;
		pPlayer->GetMuteData(mute, talker);
		out_muteCount = (int)mute;
		out_numTalkers = (int)talker;
	}
}

void CommandDebugNetworkSetLocalMuteCounts(const int BANK_ONLY(muteCount), const int BANK_ONLY(numTalkers))
{
#if __BANK
	CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
	if(pMyPlayer)
	{
		//Update our Muted count.
		StatsInterface::SetStatData(STAT_PLAYER_MUTED, muteCount);
		StatsInterface::SetStatData(STAT_PLAYER_MUTED_TALKERS_MET, numTalkers);
		pMyPlayer->SetMuteData(muteCount, numTalkers);
	}
#endif
}

float CommandNetworkGetPlayerLoudness(const int playerIndex)
{
    netPlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_GET_PLAYER_LOUDNESS");

    if (player)
    {
        scriptAssertf(player->IsLocal(), "%s:NETWORK_GET_PLAYER_LOUDNESS - Attempting to get loudness of non-local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        return NetworkInterface::GetVoice().GetPlayerLoudness(player->GetGamerInfo().GetLocalIndex());
    }

    return false;
}

void CommandNetworkSetTalkerFocus(const int playerIndex)
{
    netPlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);

    if (player)
    {
        NetworkInterface::GetVoice().SetTalkerFocus(&player->GetGamerInfo().GetGamerHandle());
    }
    else
    {
        NetworkInterface::GetVoice().SetTalkerFocus(NULL);
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TALKER_FOCUS");
}

void CommandNetworkSetTalkerProximity(const float distance)
{
    NetworkInterface::GetVoice().SetTalkerProximity(distance);
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TALKER_PROXIMITY");
}

void CommandNetworkSetTalkerProximityHeight(const float height)
{
	NetworkInterface::GetVoice().SetTalkerProximityHeight(height);
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TALKER_PROXIMITY_HEIGHT");
}

float CommandNetworkGetTalkerProximity()
{
    return NetworkInterface::GetVoice().GetTalkerProximity();
}

void CommandNetworkSetAllowUniDirectionalOverrideForNegativeProximity(const bool bAllowOverride)
{
	NetworkInterface::GetVoice().SetScriptAllowUniDirectionalOverrideForNegativeProximity(bAllowOverride);
}

void CommandNetworkSetLoudhailerProximity(const float distance)
{
	NetworkInterface::GetVoice().SetLoudhailerProximity(distance);
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_LOUDHAILER_PROXIMITY");
}

void CommandNetworkSetLoudhailerProximityHeight(const float height)
{
	NetworkInterface::GetVoice().SetLoudhailerProximityHeight(height);
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_LOUDHAILER_PROXIMITY_HEIGHT");
}

void CommandNetworkOverrideAllChatRestrictions(const bool bOverride)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_OVERRIDE_ALL_CHAT_RESTRICTIONS");
	NetworkInterface::GetVoice().SetOverrideAllRestrictions(bOverride);
}

void CommandNetworkSetVoiceCommunication(const bool bCommunicationActive)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_VOICE_ACTIVE");
	NetworkInterface::GetVoice().SetCommunicationActive(bCommunicationActive);
}

void CommandNetworkRemainInGameChat(const bool bRemainInGameChat)
{
    NetworkInterface::GetVoice().SetRemainInGameChat(bRemainInGameChat);
}

void CommandNetworkSetScriptControllingTeams(const bool WIN32PC_ONLY(bOverride))
{
	#if RSG_PC
	NetworkInterface::GetTextChat().SetScriptControllingTeams(bOverride);
	#endif
}

bool CommandNetworkSetonSameTeamForText(const int WIN32PC_ONLY(playerIndex), const bool WIN32PC_ONLY(bSameTeam) )
{
#if RSG_PC

	if (!NetworkInterface::GetTextChat().GetScriptControllingTeams())
		return false;

	CNetGamePlayer* myPlayer = NetworkInterface::GetLocalPlayer();
	CNetGamePlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);

    if (!myPlayer)
    {
        mpchatDebugf1("CommandNetworkSetonSameTeamForText:  - failed !myPlayer");
        return false;
    }

	if (!player)
	{
		mpchatDebugf1("CommandNetworkSetonSameTeamForText:  - failed !player");
		return false;
	}

	if (player->IsLocal())
		return false;

	EndpointId endpointId = player->GetEndpointId();

	if (!NetworkInterface::GetTextChat().IsTyperValid(endpointId))
	{
		mpchatDebugf1("CommandNetworkSetonSameTeamForText:  - failed, typer not valid, could be in session transition");
		return false;
	}

	NetworkInterface::GetTextChat().SetOnSameTeam(endpointId, bSameTeam);

	mpchatDebugf1("CommandNetworkSetonSameTeamForText:  - localPlayer: %s Adding/Removing player: %s ", myPlayer->GetLogName(), player->GetLogName());
	return true;
#else
	return false;
#endif
}

void CommandNetworkOverrideTransitionChat(const bool bOverride)
{
    NetworkInterface::GetVoice().SetOverrideTransitionChatScript(bOverride);
}

void CommandNetworkSetTeamOnlyChat(const bool teamOnlyChat)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TEAM_ONLY_CHAT");
	NetworkInterface::GetVoice().SetTeamOnlyChat(teamOnlyChat);
}

void CommandNetworkSetTeamOnlyTextChat(const bool WIN32PC_ONLY(teamOnlyChat))
{
#if RSG_PC
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TEAM_ONLY_TEXT_CHAT");
	NetworkInterface::GetTextChat().SetTeamOnlyChat(teamOnlyChat);
#endif
}

void CommandNetworkSetTeamChatIncludeUnassignedTeams(const bool bIncludeUnassignedTeams)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_TEAM_CHAT_INCLUDE_UNASSIGNED_TEAMS");
	NetworkInterface::GetVoice().SetTeamChatIncludeUnassignedTeams(bIncludeUnassignedTeams);
}

void CommandNetworkOverrideTeamRestrictions(const int nTeam, const bool bOverride)
{
	if(!SCRIPT_VERIFY_TEAM_INDEX("NETWORK_OVERRIDE_TEAM_RESTRICTIONS", nTeam))
		return;

	NetworkInterface::GetVoice().SetOverrideTeam(nTeam, bOverride);
}

void CommandNetworkOverrideSendRestrictions(const int nPlayerID, const bool bOverride)
{
	if(!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_OVERRIDE_SEND_RESTRICTIONS", nPlayerID))
		return;

	NetworkInterface::GetVoice().SetOverrideChatRestrictionsSend(nPlayerID, bOverride);
}

void CommandNetworkOverrideSendRestrictionsAll(const bool bOverride)
{
	NetworkInterface::GetVoice().SetOverrideChatRestrictionsSendAll(bOverride);
}

void CommandNetworkOverrideReceiveRestrictions(const int nPlayerID, const bool bOverride)
{
	if(!SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_OVERRIDE_RECEIVE_RESTRICTIONS", nPlayerID))
		return;

	NetworkInterface::GetVoice().SetOverrideChatRestrictionsReceive(nPlayerID, bOverride);
}

void CommandNetworkOverrideReceiveRestrictionsAll(const bool bOverride)
{
	NetworkInterface::GetVoice().SetOverrideChatRestrictionsReceiveAll(bOverride);
}

void CommandNetworkSetOverrideSpectatorMode(const bool overrideSpectatorMode)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_OVERRIDE_SPECTATOR_MODE");
	NetworkInterface::GetVoice().SetOverrideSpectatorMode(overrideSpectatorMode);
}

void CommandNetworkOverrideChatRestrictions(const int playerIndex, const bool bOverride)
{
	if (SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_OVERRIDE_CHAT_RESTRICTIONS", playerIndex))
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		if (pPlayer && !pPlayer->IsMyPlayer())
		{
			NetworkInterface::GetVoice().SetOverrideChatRestrictions(playerIndex, bOverride);
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_OVERRIDE_CHAT_RESTRICTIONS");
}

void CommandNetworkOverrideChatRestrictionsTeamReceive(const int nTeam, const bool bOverride)
{
	if(!SCRIPT_VERIFY_TEAM_INDEX("NETWORK_OVERRIDE_CHAT_RESTRICTIONS_TEAM_RECEIVE", nTeam))
		return;

	NetworkInterface::GetVoice().SetOverrideTeam(nTeam, bOverride);

	/*unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
	const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

	for(unsigned index = 0; index < numRemoteActivePlayers; index++)
	{
		const netPlayer *player = remoteActivePlayers[index];
		if(!player->IsBot() && team >= 0 && team == player->GetTeam())
			NetworkInterface::GetVoice().SetOverrideChatRestrictionsReceive(player->GetPhysicalPlayerIndex(), bOverride);
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_OVERRIDE_CHAT_RESTRICTIONS_TEAM_RECEIVE");*/
}

void CommandNetworkSetOverrideTutorialSessionChat(const bool bOverride)
{
	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_OVERRIDE_TUTORIAL_SESSION_CHAT");
	NetworkInterface::GetVoice().SetOverrideTutorialSession(bOverride);
}

void CommandNetworkSetProximityAffectsSend(const bool bEnable)
{
	NetworkInterface::GetVoice().SetProximityAffectsSend(bEnable);
}

void CommandNetworkSetProximityAffectsReceive(const bool bEnable)
{
	NetworkInterface::GetVoice().SetProximityAffectsReceive(bEnable);
}

void CommandNetworkSetProximityAffectsTeam(const bool bEnable)
{
	NetworkInterface::GetVoice().SetProximityAffectsTeam(bEnable);
}

void CommandNetworkSetNoSpectatorChat(const bool bEnable)
{
	NetworkInterface::GetVoice().SetNoSpectatorChat(bEnable);
}

void CommandNetworkSetIgnoreSpectatorChatLimitsForSameTeam(const bool bEnable)
{
    NetworkInterface::GetVoice().SetIgnoreSpectatorLimitsForSameTeam(bEnable);
}

void CommandNetworkSetVoiceChannel(int nVoiceChannel)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "NETWORK_SET_VOICE_CHANNEL :: Not in network game!"))
		return;

	CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pMyPlayer, "NETWORK_SET_VOICE_CHANNEL :: Local player invalid!"))
		return;

	// add some logging
	scriptDebugf1("%s:NETWORK_SET_VOICE_CHANNEL - Setting voice channel to %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nVoiceChannel);

	// set voice channel
	pMyPlayer->SetVoiceChannel(nVoiceChannel);
}

void CommandNetworkClearVoiceChannel()
{
	if(!NetworkInterface::IsNetworkOpen())
		return;

	CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pMyPlayer, "NETWORK_CLEAR_VOICE_CHANNEL :: Local player invalid!"))
		return;

	// add some logging
	scriptDebugf1("%s:NETWORK_CLEAR_VOICE_CHANNEL - Clearing voice channel.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	// clear voice channel
	pMyPlayer->ClearVoiceChannel();
}

void CommandNetworkApplyVoiceProximityOverride(const scrVector& vOverridePosition)
{
    if(!NetworkInterface::IsNetworkOpen())
        return;

    CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
    if(!scriptVerifyf(pMyPlayer, "NETWORK_APPLY_VOICE_PROXIMITY_OVERRIDE :: Local player invalid!"))
        return;

    // add some logging
    scriptDebugf1("%s:NETWORK_APPLY_VOICE_PROXIMITY_OVERRIDE - Applying voice proximity override.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    // clear voice channel
    pMyPlayer->ApplyVoiceProximityOverride(Vector3(vOverridePosition));
}

void CommandNetworkClearVoiceProximityOverride()
{
    if(!NetworkInterface::IsNetworkOpen())
        return;

    CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
    if(!scriptVerifyf(pMyPlayer, "NETWORK_APPLY_VOICE_PROXIMITY_OVERRIDE :: Local player invalid!"))
        return;

    // add some logging
    scriptDebugf1("%s:NETWORK_APPLY_VOICE_PROXIMITY_OVERRIDE - Clearing voice proximity override.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    // clear voice channel
    pMyPlayer->ClearVoiceProximityOverride();
}

void CommandNetworkEnableVoiceBandwidthRestriction(const int playerIndex)
{
    if (SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_ENABLE_VOICE_BANDWIDTH_RESTRICTION", playerIndex))
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		if (pPlayer && !pPlayer->IsMyPlayer())
		{
            PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();

            if(SCRIPT_VERIFY(playerIndex != INVALID_PLAYER_INDEX, "NETWORK_ENABLE_VOICE_BANDWIDTH_RESTRICTION: Player is not physical!"))
            {
			    NetworkInterface::GetVoice().EnableBandwidthRestrictions(playerIndex);
            }
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_ENABLE_VOICE_BANDWIDTH_RESTRICTION");
}

void CommandNetworkDisableVoiceBandwidthRestriction(const int playerIndex)
{
    if (SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_DISABLE_VOICE_BANDWIDTH_RESTRICTION", playerIndex))
	{
		CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		if (pPlayer && !pPlayer->IsMyPlayer())
		{
            PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();

            if(SCRIPT_VERIFY(playerIndex != INVALID_PLAYER_INDEX, "NETWORK_DISABLE_VOICE_BANDWIDTH_RESTRICTION: Player is not physical!"))
            {
			    NetworkInterface::GetVoice().DisableBandwidthRestrictions(playerIndex);
            }
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_DISABLE_VOICE_BANDWIDTH_RESTRICTION");
}

void CommandNetworkSetSpectatorToNonSpectatorTextChat(const bool WIN32PC_ONLY(isEnabled))
{
#if RSG_PC
	NetworkInterface::GetTextChat().SetSpectatorToNonSpectatorChat(isEnabled);
#endif // RSG_PC
}

bool CommandNetworkTextChatIsTyping()
{
#if RSG_PC
	return SMultiplayerChat::GetInstance().IsChatTyping();
#else
	return false;
#endif // RSG_PC
}

bool CommandNetworkStoreSinglePlayerGame()
{
#if __PPU
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_STORE_SINGLE_PLAYER_GAME returning TRUE - doesn't actually do anything on PS3");
    return true;
#else
    if (CTheScripts::GetSinglePlayerRestoreHasJustOccurred())
    {
        CTheScripts::SetSinglePlayerRestoreHasJustOccurred(false);
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_STORE_SINGLE_PLAYER_GAME returning FALSE");
        return false;
    }
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_STORE_SINGLE_PLAYER_GAME returning TRUE");
        return true;
    }
#endif
}

void CommandShutdownAndLaunchSinglePlayerGame()
{
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SHUTDOWN_AND_LAUNCH_SINGLE_PLAYER_GAME");
	CLiveManager::OnProfileChangeCommon();
    CGame::StartNewGame(CGameLogic::GetRequestedLevelIndex());
}

void CommandShutdownAndLaunchNetworkGame(int level)
{
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SHUTDOWN_AND_LAUNCH_NETWORK_GAME - Episode Index is %d", level);
	CLiveManager::OnProfileChangeCommon();
    CGame::StartNewGame(level);
}

bool CommandShutdownAndLoadMostRecentSave()
{
//	Copied from CReplayMgrInternal::QuitReplay()
	if (!CGenericGameStorage::QueueLoadMostRecentSave())
	{
//		CGame::SetStateToIdle();

		scriptDisplayf("%s SHUTDOWN_AND_LOAD_MOST_RECENT_SAVE - failed to queue the LoadMostRecentSave operation so just start a new single player session", CTheScripts::GetCurrentScriptNameAndProgramCounter());
//		camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
		CGame::StartNewGame(CGameLogic::GetRequestedLevelIndex());	//	GetCurrentLevelIndex());

		return false;
	}

	scriptDisplayf("%s SHUTDOWN_AND_LOAD_MOST_RECENT_SAVE - the LoadMostRecentSave operation has been successfully queued", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	return true;
}

void CommandNetworkSetFriendlyFireOption(const bool friendlyFire)
{
    CNetwork::SetFriendlyFireAllowed(friendlyFire);
}

void CommandNetworkSetRichPresenceString(const int PPU_ONLY(id) ORBIS_ONLY(id), const char* PPU_ONLY(presence) ORBIS_ONLY(presence))
{
#if RSG_NP

	if (!NetworkInterface::IsLocalPlayerOnline())
	{
		scriptDebugf1("%s:NETWORK_SET_RICH_PRESENCE - Not setting rich presence \"%s:0x%08x\" - Local player is offline", CTheScripts::GetCurrentScriptNameAndProgramCounter(), player_schema::PresenceUtil::GetLabel(id), id);
		return;
	}

	if (!NetworkInterface::GetRichPresenceMgr().IsInitialized())
	{
		scriptDebugf1("%s:NETWORK_SET_RICH_PRESENCE - Not setting rich presence \"%s:0x%08x\" - Manager not initialized", CTheScripts::GetCurrentScriptNameAndProgramCounter(), player_schema::PresenceUtil::GetLabel(id), id);
		return;
	}

	scriptAssertf(player_schema::PresenceUtil::GetIsValid(id), "%s:NETWORK_SET_RICH_PRESENCE_STRING - Invalid Rich Presence Id=\"0x%08x\" ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
	if (!player_schema::PresenceUtil::GetIsValid(id))
		return;

	scriptAssertf(presence, "%s:NETWORK_SET_RICH_PRESENCE_STRING - Failed to set rich presence: String is empty.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!presence)
		return;

	scriptAssertf(TheText.DoesTextLabelExist(presence), "%s : NETWORK_SET_RICH_PRESENCE_STRING - Label \"%s\" doesnt exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), presence);
	if(!TheText.DoesTextLabelExist(presence))
		return;

	char* presencechar = TheText.Get(atStringHash(presence), presence);

	scriptAssertf(CTextConversion::GetCharacterCount(presencechar) <= RL_PRESENCE_MAX_BUF_SIZE, "%s:NETWORK_SET_RICH_PRESENCE_STRING - Failed to set rich presence: Invalid presence size \"%u\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ustrlen(presence));
	if (!(CTextConversion::GetCharacterCount(presencechar) <= RL_PRESENCE_MAX_BUF_SIZE))
		return;

	NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(id, presencechar);

#else

	scriptAssertf(false, "%s:NETWORK_SET_RICH_PRESENCE_STRING - Only valid on PS3 and PS4", CTheScripts::GetCurrentScriptNameAndProgramCounter());

#endif // RSG_NP
}

void CommandNetworkSetRichPresence(const int XENON_ONLY(id) DURANGO_ONLY(id), int& XENON_ONLY(data) DURANGO_ONLY(data), 
								   int XENON_ONLY(sizeOfData) DURANGO_ONLY(sizeOfData), int XENON_ONLY(numFields) DURANGO_ONLY(numFields))
{
#if __XENON || RSG_DURANGO

	if (!NetworkInterface::IsLocalPlayerOnline())
	{
		scriptDebugf1("%s:NETWORK_SET_RICH_PRESENCE - Not setting rich presence \"%s:0x%08x\" - Local player is offline", CTheScripts::GetCurrentScriptNameAndProgramCounter(), player_schema::PresenceUtil::GetLabel(id), id);
		return;
	}

	if (!NetworkInterface::GetRichPresenceMgr().IsInitialized())
	{
		scriptDebugf1("%s:NETWORK_SET_RICH_PRESENCE - Not setting rich presence \"%s:0x%08x\" - Manager not initialized", CTheScripts::GetCurrentScriptNameAndProgramCounter(), player_schema::PresenceUtil::GetLabel(id), id);
		return;
	}

	scriptAssertf(player_schema::PresenceUtil::GetIsValid(id), "%s:NETWORK_SET_RICH_PRESENCE - Invalid Rich Presence Id=\"0x%08x\" ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
	if (!player_schema::PresenceUtil::GetIsValid(id))
		return;

	u8* Address = reinterpret_cast< u8* >(&data);
	sizeOfData *= sizeof(scrValue); // Script returns size in script words
	scriptAssertf(numFields == (int)player_schema::PresenceUtil::GetCountOfFields(id), "%s:NETWORK_SET_RICH_PRESENCE - Presence Id=\"0x%08x\" fields number \"%d\" should be \"%d\" "
																						  , CTheScripts::GetCurrentScriptNameAndProgramCounter()
																						  , id
																						  , numFields
																						  , player_schema::PresenceUtil::GetCountOfFields(id));

	scriptAssertf(sizeOfData == numFields*(int)sizeof(scrValue), "%s:NETWORK_SET_RICH_PRESENCE - Presence Id=\"0x%08x\" data size \"%d\" should be \"%d\" "
															   , CTheScripts::GetCurrentScriptNameAndProgramCounter()
															   , id
															   , sizeOfData
															   , numFields*(int)sizeof(scrValue));

	atArray< int > fieldIds;
	atArray< XENON_ONLY(int) DURANGO_ONLY(richPresenceFieldStat) > fieldData;
	for (int i=0; i<numFields; i++)
	{
		int fieldId = player_schema::PresenceUtil::GetFieldIdFromFieldIndex(id, i);
		fieldIds.PushAndGrow(fieldId);

		scrValue value;
		sysMemCpy(&value, Address, sizeof(scrValue));

#if RSG_DURANGO
		richPresenceFieldStat fieldStat(id, player_schema::PresenceUtil::GetFieldStrFromFieldId(fieldId, value.Int));
		fieldData.PushAndGrow(fieldStat);
#else
		fieldData.PushAndGrow(value.Int);
#endif
	}

	NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(NetworkInterface::GetLocalGamerIndex(), id, fieldIds.GetElements(), fieldData.GetElements(), numFields);

#else

	scriptAssertf(false, "%s:NETWORK_SET_RICH_PRESENCE - Invalid command in PS3/PS4/PC ", CTheScripts::GetCurrentScriptNameAndProgramCounter());

#endif // __XENON
}

int CommandNetworkGetTimeoutTime()
{
	return static_cast<int>(CNetwork::GetTimeoutTime());
}

void CommandSetNetworkIdCanMigrate(int networkId, bool bCanMigrate)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_CAN_MIGRATE - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_CAN_MIGRATE - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_CAN_MIGRATE - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (scriptVerifyf(!networkObject->IsClone(), "%s:SET_NETWORK_ID_CAN_MIGRATE - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
#if !__FINAL
						scriptDebugf1("%s:SET_NETWORK_ID_CAN_MIGRATE - ID: %d, Name: %s, CanMigrate: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkId, networkObject->GetLogName(), bCanMigrate ? "True" : "False");
						scrThread::PrePrintStackTrace();
#endif // !__FINAL
						networkObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, !bCanMigrate);
					}
				}
			}
			else
			{
				// cache can migrate state 
				CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();

				if (scriptVerify(pExtension))
				{
#if !__FINAL
					scriptDebugf1("%s:SET_NETWORK_ID_CAN_MIGRATE - ID: %d, Script: %s, CanMigrate: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkId, pExtension->GetScriptInfo() ? pExtension->GetScriptInfo()->GetScriptId().GetLogName() : "Invalid", bCanMigrate ? "True" : "False");
					scrThread::PrePrintStackTrace();
#endif // !__FINAL
					pExtension->SetCanMigrate(bCanMigrate);
				}
			}
		}
	}
}

void CommandSetNetworkIdExistsOnAllMachines(int networkId, bool bExistsOnAllMachines)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();

			if (!bExistsOnAllMachines && pExtension && pExtension->GetScriptInfo() && pExtension->GetScriptInfo()->IsScriptHostObject())
			{
				scriptAssertf(0, "%s:SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES - this is not permitted for host script entities. They must exist on all machines running the script.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return;
			}

			if (scriptVerify(IS_SCRIPT_NETWORK_ACTIVE()))
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (scriptVerifyf(!networkObject->IsClone(), "%s:SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
                        networkObject->SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT, bExistsOnAllMachines);
					}
				}
			}
		}
	}
}

void CommandSetNetworkIdAlwaysExistsForPlayer(int networkId, int playerIndex, bool bAlwaysExistsForPlayer)
{
    if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerify(IS_SCRIPT_NETWORK_ACTIVE()))
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (scriptVerifyf(!networkObject->IsClone(), "%s:SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
                        CNetObjGame *netObjGame = SafeCast(CNetObjGame, networkObject);
                        netObjGame->SetAlwaysClonedForPlayer(static_cast<PhysicalPlayerIndex>(playerIndex), bAlwaysExistsForPlayer);
                    }
                }
            }
        }
    }
}

void CommandSetNetworkIdStopCloning(int networkId, bool bStopCloning)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_STOP_CLONING - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_STOP_CLONING - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_STOP_CLONING - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (scriptVerifyf(!networkObject->IsClone(), "%s:SET_NETWORK_ID_STOP_CLONING - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						Assertf(!networkObject->HasBeenCloned() && !networkObject->IsPendingCloning(), "%s:SET_NETWORK_ID_STOP_CLONING -  This command cannot be called on a object that is already cloned on another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter());
						Assertf(!networkObject->IsGlobalFlagSet(netObject::GLOBALFLAG_CLONEALWAYS) && !networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT), "%s:SET_NETWORK_ID_STOP_CLONING -  This command cannot be called on a object that has had one of the SET_*_EXISTS_ON_ALL_MACHINES commands called on it", CTheScripts::GetCurrentScriptNameAndProgramCounter());
						networkObject->SetLocalFlag(netObject::LOCALFLAG_NOCLONE, bStopCloning);
					}
				}
			}
			else
			{
				// cache can migrate state 
				CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();

				if (scriptVerify(pExtension))
				{
					pExtension->SetStopCloning(bStopCloning);
				}
			}
		}
	}
}

void CommandSetNetworkIdCanBeReassigned(int networkId, bool bCanBeReassigned)
{
    if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_CAN_BE_REASSIGNED - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_CAN_BE_REASSIGNED - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_CAN_BE_REASSIGNED - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
                    if (scriptVerifyf(bCanBeReassigned || networkObject->IsGlobalFlagSet(netObject::GLOBALFLAG_PERSISTENTOWNER), "%s:SET_NETWORK_ID_CAN_BE_REASSIGNED - the entity is allowed to migrate to other machines! Call SET_NETWORK_ID_CAN_MIGRATE(FALSE) on this object first!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                    {
					    if (scriptVerifyf(!networkObject->IsClone() && !networkObject->IsPendingOwnerChange(), "%s:SET_NETWORK_ID_CAN_BE_REASSIGNED - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					    {
                            CNetObjPhysical *netObjPhysical = static_cast<CNetObjPhysical *>(networkObject);
                            netObjPhysical->SetCanBeReassigned(bCanBeReassigned);
					    }
                    }
				}
			}
		}
	}
}

void CommandSetNetworkIdCanBlend(int networkId, bool bCanBlend)
{
    if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_CAN_BLEND - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_CAN_BLEND - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_CAN_BLEND - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
#if !__FINAL
                    bool bOldCanBlend = networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_DISABLE_BLENDING);

                    if(bOldCanBlend != bCanBlend)
                    {
                        if(bCanBlend)
                        {
                            scriptDebugf1("%s: Enabling network blending for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName());
                        }
                        else
                        {
                            scriptDebugf1("%s: Disabling network blending for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName());
                        }

                        scrThread::PrePrintStackTrace();
                    }
#endif // !__FINAL
                    networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_DISABLE_BLENDING, !bCanBlend);
				}
			}
		}
	}
}

void CommandSetNetworkEntityCanBlend(int entityId, bool bCanBlend)
{
    CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	if(pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:NETWORK_SET_ENTITY_CAN_BLEND - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
#if !__FINAL
            bool bOldCanBlend = networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_DISABLE_BLENDING);

            if(bOldCanBlend != bCanBlend)
            {
                if(bCanBlend)
                {
                    scriptDebugf1("%s: Enabling network blending for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName());
                }
                else
                {
                    scriptDebugf1("%s: Disabling network blending for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName());
                }

                scrThread::PrePrintStackTrace();
            }
#endif // !__FINAL

			networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_DISABLE_BLENDING, !bCanBlend);
		}
	}
}

void CommandSetNetworkObjectCanBlendWhenFixed(int objectId, bool canBlend)
{
    CObject *object = CTheScripts::GetEntityToModifyFromGUID<CObject>(objectId);

    if(object)
    {
        CNetObjObject *netObjObject = SafeCast(CNetObjObject, object->GetNetworkObject());

        if (scriptVerifyf(netObjObject, "%s:NETWORK_SET_OBJECT_CAN_BLEND_WHEN_FIXED - The supplied OBJECT_INDEX is not networked! This command can only be called on network objects", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            netObjObject->SetCanBlendWhenFixed(canBlend);
        }
    }
}

void CommandSetNetworkEntityRemainsWhenUnnetworked(int entityId, bool bSet)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:NETWORK_SET_ENTITY_REMAINS_WHEN_UNNETWORKED - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			static_cast<CNetObjEntity*>(networkObject)->SetLeaveEntityAfterCleanup(bSet);
		}
	}
}

void CommandSetNetworkEntityOnlyExistsForParticipants(int entityId, bool bSet)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId);

	if(pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:NETWORK_SET_ENTITY_ONLY_EXISTS_FOR_PARTICIPANTS - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			networkObject->SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT, bSet);

			CNetwork::GetObjectManager().UpdateAllInScopeStateImmediately(networkObject);
		}
	}
}

void CommandStopNetworkingEntity(int entityId)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId);

	if(pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:STOP_NETWORKING_ENTITY - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(!networkObject->IsClone() && !networkObject->IsPendingOwnerChange(), "%s:STOP_NETWORKING_ENTITY - the entity is not locally controlled", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			NetworkInterface::UnregisterObject(pEntity);
		}
	}
}

void CommandSetNetworkIdVisibleInCutscene(int networkId, bool visible, bool remotelyVisible)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
#if __BANK
					if(networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) != visible || SafeCast(CNetObjPhysical, networkObject)->GetRemotelyVisibleInCutscene() != remotelyVisible)
					{
						gnetDebug1("CommandSetNetworkIdVisibleInCutscene called on %s, visible %d remotelyVisible %d, %s", networkObject->GetLogName(), visible, remotelyVisible, CTheScripts::GetCurrentScriptNameAndProgramCounter());
					}
#endif // __BANK
					networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, visible);
					SafeCast(CNetObjPhysical, networkObject)->SetRemotelyVisibleInCutscene(remotelyVisible);
				}
			}
		}
	}
}

void CommandSetNetworkIdVisibleInCutsceneHack(int networkId, bool visible, bool remotelyVisible)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if(visible)
					{
						pPhysical->EnableCollision();
					}
					else
					{
						pPhysical->DisableCollision();
					}

#if __BANK
					if(networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) != visible || SafeCast(CNetObjPhysical, networkObject)->GetRemotelyVisibleInCutscene() != remotelyVisible)
					{
						gnetDebug1("CommandSetNetworkIdVisibleInCutsceneHack called on %s, visible %d remotelyVisible %d, %s", networkObject->GetLogName(), visible, remotelyVisible, CTheScripts::GetCurrentScriptNameAndProgramCounter());
					}
#endif // __BANK
					
					networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, visible);
					SafeCast(CNetObjPhysical, networkObject)->SetRemotelyVisibleInCutscene(remotelyVisible);
				}
			}
		}
	}
}

void CommandSetNetworkIdVisibleInCutsceneRemainHack(int networkId, bool set)
{
	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				CNetObjEntity* networkObject = SafeCast(CNetObjEntity, pPhysical->GetNetworkObject());
				if (scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					gnetDebug1("%s SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK called on %s, set: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkObject->GetLogName(), set ? "TRUE":"FALSE");
#if !__FINAL
					scrThread::PrePrintStackTrace();
#endif // !__FINAL
#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK", "%s", networkObject->GetLogName());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Set", "%s", set ? "TRUE":"FALSE");
#endif
					networkObject->SetKeepIsVisibleForCutsceneFlagSet(set);
				}
			}
		}
	}
}

void CommandSetNetworkCutsceneEntities(bool bNetwork)
{
	if(scriptVerifyf(NetworkInterface::IsInMPCutscene(), "%s:SET_NETWORK_CUTSCENE_ENTITIES - a MP cutscene is not in progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "SET_NETWORK_CUTSCENE_ENTITIES", "%s", bNetwork ? "TRUE" : "FALSE");
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

		CNetwork::SetNetworkCutsceneEntities(bNetwork);
	}
}

bool CommandAreCutsceneEntitiesNetworked()
{
	return CNetwork::AreCutsceneEntitiesNetworked();
}

void CommandSetNetworkIdPassControlInTutorial(int networkId, bool passControl)
{
	if(scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));
		if(scriptVerifyf(pPhysical, "%s:SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject* networkObject = pPhysical->GetNetworkObject();
				if(scriptVerifyf(networkObject, "%s:SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if(scriptVerifyf(!networkObject->IsGlobalFlagSet(netObject::GLOBALFLAG_PERSISTENTOWNER), "%s:SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL - the entity is not allowed to migrate to other machines! Call SET_NETWORK_ID_CAN_MIGRATE(TRUE) on this object first!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						if(scriptVerifyf(!networkObject->IsClone() && !networkObject->IsPendingOwnerChange(), "%s:SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL - the entity is not controlled by this machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
						{
							CNetObjPhysical *netObjPhysical = static_cast<CNetObjPhysical *>(networkObject);
							netObjPhysical->SetPassControlInTutorial(passControl);
						}
					}
				}
			}
		}
	}
}

bool CommandIsNetworkIdOwnedByParticipant(int networkId)
{
	bool bIsOwnedByParticipant = false;

	if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:IS_NETWORK_ID_OWNED_BY_PARTICIPANT - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

		if (scriptVerifyf(pPhysical, "%s:IS_NETWORK_ID_OWNED_BY_PARTICIPANT - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (IS_SCRIPT_NETWORK_ACTIVE())
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:IS_NETWORK_ID_OWNED_BY_PARTICIPANT - the entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					netPlayer* pOwner = networkObject->GetPlayerOwner();

					if (pOwner && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsPlayerAParticipant(*pOwner))
					{
						bIsOwnedByParticipant = true;
					}
				}
			}
		}
	}

	return bIsOwnedByParticipant;
}

bool CommandSetAlwaysClonePlayerVehicle(bool bAlwaysClone)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SET_ALWAYS_CLONE_PLAYER_VEHICLE - This script command is not allowed in single player game scripts!"))
		return false;

	if(!scriptVerifyf(FindPlayerPed(), "SET_ALWAYS_CLONE_PLAYER_VEHICLE - Local player does not exist!"))
		return false;

	CNetObjPlayer* pNetObjPlayer = SafeCast(CNetObjPlayer, FindPlayerPed()->GetNetworkObject());
	if(!scriptVerifyf(pNetObjPlayer, "SET_ALWAYS_CLONE_PLAYER_VEHICLE - Local player does not have a network object!"))
		return false;

	// log changes
#if !__NO_OUTPUT
	if(bAlwaysClone && !pNetObjPlayer->GetAlwaysClonePlayerVehicle())
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SET_ALWAYS_CLONE_PLAYER_VEHICLE setting to TRUE");
	else if(!bAlwaysClone && pNetObjPlayer->GetAlwaysClonePlayerVehicle())
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SET_ALWAYS_CLONE_PLAYER_VEHICLE setting to FALSE");
#endif

	pNetObjPlayer->SetAlwaysClonePlayerVehicle(bAlwaysClone);
	return true; 
}

void CommandSetRemotePlayerVisibleInCutscene(int playerIndex, bool locallyVisible)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:SET_REMOTE_PLAYER_VISIBLE_IN_CUTSCENE - Network game is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
		CPed* targetPlayerPed = targetPlayer ? targetPlayer->GetPlayerPed() : NULL;

		if (gnetVerifyf(targetPlayerPed, "%s:SET_REMOTE_PLAYER_VISIBLE_IN_CUTSCENE - target player ped not found for index: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex)
		&& gnetVerifyf(targetPlayerPed->GetNetworkObject(), "%s:SET_REMOTE_PLAYER_VISIBLE_IN_CUTSCENE - target player ped has no netobj for index: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex))
		{
			CNetObjPlayer* remotePlayerNetObj = SafeCast(CNetObjPlayer, targetPlayerPed->GetNetworkObject());
#if __BANK
			if (remotePlayerNetObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) != locallyVisible)
			{
				gnetDebug1("CommandSetRemotePlayerVisibleInCutscene called on %s, visible %s from: %s", remotePlayerNetObj->GetLogName(), locallyVisible?"TRUE":"FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
#endif // __BANK
			
			remotePlayerNetObj->SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, locallyVisible);
		}
	}
}

void CommandSetLocalPlayerVisibleInCutscene(bool locallyVisible, bool remotelyVisible)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:SET_LOCAL_PLAYER_VISIBLE_IN_CUTSCENE - Network game is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(FindPlayerPed(), "%s:SET_LOCAL_PLAYER_VISIBLE_IN_CUTSCENE - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPlayer *networkObject = SafeCast(CNetObjPlayer, FindPlayerPed()->GetNetworkObject());

			if (scriptVerifyf(networkObject, "%s:SET_LOCAL_PLAYER_VISIBLE_IN_CUTSCENE - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
#if __BANK
				if(networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) != locallyVisible || SafeCast(CNetObjPhysical, networkObject)->GetRemotelyVisibleInCutscene() != remotelyVisible)
				{
					gnetDebug1("CommandSetLocalPlayerVisibleInCutscene called on %s, visible %d remotelyVisible %d, %s", networkObject->GetLogName(), locallyVisible, remotelyVisible, CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
#endif // __BANK
				
				networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, locallyVisible);
				networkObject->SetRemotelyVisibleInCutscene(remotelyVisible);
			}
		}
	}
}

void CommandSetLocalPlayerInvisibleLocally(bool bIncludePlayersVehicle)
{
	CPed* pPlayer = FindPlayerPed();

	if (scriptVerifyf(pPlayer, "%s:SET_LOCAL_PLAYER_INVISIBLE_LOCALLY - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(!NetworkInterface::IsInMPCutscene(), "%s:SET_LOCAL_PLAYER_INVISIBLE_LOCALLY - cannot call this during a cutscene", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetObjPlayer *networkObject = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());

		if (scriptVerifyf(networkObject, "%s:SET_LOCAL_PLAYER_INVISIBLE_LOCALLY - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			networkObject->SetOverridingLocalVisibility(false, "SET_LOCAL_PLAYER_INVISIBLE_LOCALLY", true, true);
		}

		if (bIncludePlayersVehicle && pPlayer->GetIsInVehicle() && pPlayer->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(pPlayer->GetMyVehicle()->GetNetworkObject());
		
			if (pVehicleObj)
			{
				pVehicleObj->SetOverridingLocalVisibility(false, "SET_LOCAL_PLAYER_INVISIBLE_LOCALLY", true, true);
			}
		}
	}
}

void CommandSetLocalPlayerVisibleLocally(bool bIncludePlayersVehicle)
{
	CPed* pPlayer = FindPlayerPed();

	if (scriptVerifyf(pPlayer, "%s:SET_LOCAL_PLAYER_VISIBLE_LOCALLY - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(!NetworkInterface::IsInMPCutscene(), "%s:SET_LOCAL_PLAYER_VISIBLE_LOCALLY - cannot call this during a cutscene", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetObjPlayer *networkObject = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());

		if (scriptVerifyf(networkObject, "%s:SET_LOCAL_PLAYER_VISIBLE_LOCALLY - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			networkObject->SetOverridingLocalVisibility(true, "SET_LOCAL_PLAYER_VISIBLE_LOCALLY", true, true);
		}

		if (bIncludePlayersVehicle && pPlayer->GetIsInVehicle() && pPlayer->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(pPlayer->GetMyVehicle()->GetNetworkObject());

			if (pVehicleObj)
			{
				pVehicleObj->SetOverridingLocalVisibility(true, "SET_LOCAL_PLAYER_VISIBLE_LOCALLY", true, true);
			}
		}
	}
}

void CommandSetLocalPlayerInvisibleRemotely(bool bIncludePlayersVehicle)
{
	CPed* pPlayer = FindPlayerPed();

	if (scriptVerifyf(pPlayer, "%s:SET_LOCAL_PLAYER_INVISIBLE_REMOTELY - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(!NetworkInterface::IsInMPCutscene(), "%s:SET_LOCAL_PLAYER_INVISIBLE_REMOTELY - cannot call this during a cutscene", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetObjPlayer *networkObject = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());

		if (scriptVerifyf(networkObject, "%s:SET_LOCAL_PLAYER_INVISIBLE_REMOTELY - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
#if ENABLE_NETWORK_LOGGING
			if(!networkObject->GetOverridingRemoteVisibility())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SET_LOCAL_PLAYER_INVISIBLE_REMOTELY", networkObject->GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Include Vehicle: %s", bIncludePlayersVehicle ? "TRUE" : "FALSE");
			}
#endif // ENABLE_NETWORK_LOGGING
			networkObject->SetOverridingRemoteVisibility(true, true, "SET_LOCAL_PLAYER_INVISIBLE_REMOTELY");
		}

		if (bIncludePlayersVehicle && pPlayer->GetIsInVehicle() && pPlayer->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(pPlayer->GetMyVehicle()->GetNetworkObject());

			if (pVehicleObj)
			{
#if ENABLE_NETWORK_LOGGING
				if(!networkObject->GetOverridingRemoteVisibility())
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SET_LOCAL_PLAYER_INVISIBLE_REMOTELY", pVehicleObj->GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Include Vehicle: %s", bIncludePlayersVehicle ? "TRUE" : "FALSE");
				}
#endif // ENABLE_NETWORK_LOGGING
				pVehicleObj->SetOverridingRemoteVisibility(true, false, "SET_LOCAL_PLAYER_INVISIBLE_REMOTELY");
			}
		}
	}
}

void CommandResetLocalPlayerRemoteVisibilityOverride(bool bIncludePlayersVehicle)
{
	CPed* pPlayer = FindPlayerPed();

	if (scriptVerifyf(pPlayer, "%s:RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE - the local player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(!NetworkInterface::IsInMPCutscene(), "%s:RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE - cannot call this during a cutscene", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetObjPlayer *networkObject = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());

		if (scriptVerifyf(networkObject, "%s:RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE - the local player does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
#if ENABLE_NETWORK_LOGGING
			if(networkObject->GetOverridingRemoteVisibility())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE", networkObject->GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Include Vehicle: %s", bIncludePlayersVehicle ? "TRUE" : "FALSE");
			}
#endif // ENABLE_NETWORK_LOGGING
			networkObject->SetOverridingRemoteVisibility(false, false, "RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE");
		}

		if (bIncludePlayersVehicle && pPlayer->GetIsInVehicle() && pPlayer->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(pPlayer->GetMyVehicle()->GetNetworkObject());

			if (pVehicleObj)
			{
#if ENABLE_NETWORK_LOGGING
				if(networkObject->GetOverridingRemoteVisibility())
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE", pVehicleObj->GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Include Vehicle: %s", bIncludePlayersVehicle ? "TRUE" : "FALSE");
				}
#endif // ENABLE_NETWORK_LOGGING
				pVehicleObj->SetOverridingRemoteVisibility(false, false, "RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE");
			}
		}
	}
}

void CommandSetPlayerInvisibleLocally(int playerIndex, bool bIncludePlayersVehicle)
{
	CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	CPed* targetPlayerPed = targetPlayer ? targetPlayer->GetPlayerPed() : NULL;

	if (targetPlayerPed && targetPlayerPed->GetNetworkObject())
	{
		static_cast<CNetObjPlayer*>(targetPlayerPed->GetNetworkObject())->SetOverridingLocalVisibility(false, "SET_PLAYER_INVISIBLE_LOCALLY", true, true);

		if (bIncludePlayersVehicle && targetPlayerPed->GetIsInVehicle() && targetPlayerPed->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(targetPlayerPed->GetMyVehicle()->GetNetworkObject());

			if (pVehicleObj)
			{
				pVehicleObj->SetOverridingLocalVisibility(false, "SET_PLAYER_INVISIBLE_LOCALLY", true, true);
			}
		}
	}
}

void CommandSetPlayerVisibleLocally(int playerIndex, bool bIncludePlayersVehicle)
{
	CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

	CPed* targetPlayerPed = targetPlayer ? targetPlayer->GetPlayerPed() : NULL;

	if (targetPlayerPed && targetPlayerPed->GetNetworkObject())
	{
		static_cast<CNetObjPlayer*>(targetPlayerPed->GetNetworkObject())->SetOverridingLocalVisibility(true, "SET_PLAYER_VISIBLE_LOCALLY", true, true);

		if (bIncludePlayersVehicle && targetPlayerPed->GetIsInVehicle() && targetPlayerPed->GetMyVehicle())
		{
			CNetObjEntity* pVehicleObj = static_cast<CNetObjEntity*>(targetPlayerPed->GetMyVehicle()->GetNetworkObject());

			if (pVehicleObj)
			{
				pVehicleObj->SetOverridingLocalVisibility(true, "SET_PLAYER_VISIBLE_LOCALLY", true, true);
			}
		}
	}
}

bool CommandIsPlayerInCutscene(int playerIndex)
{
	bool bInCutscene = false;

	CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

	if (targetPlayer && targetPlayer->GetPlayerInfo())
	{
		bInCutscene = targetPlayer->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE;
	}

	return bInCutscene;
}

void CommandSetEntityVisibleInCutscene(int entityId, bool visible, bool remotelyVisible)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	
	if (pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:SET_ENTITY_VISIBLE_IN_CUTSCENE - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(networkObject->GetEntity() && networkObject->GetEntity()->GetIsPhysical(), "%s:SET_ENTITY_VISIBLE_IN_CUTSCENE - the entity is not a physical", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
#if __BANK
			if(networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) != visible || SafeCast(CNetObjPhysical, networkObject)->GetRemotelyVisibleInCutscene() != remotelyVisible)
			{
				gnetDebug1("CommandSetEntityVisibleInCutscene called on %s, visible %d remotelyVisible %d, %s", networkObject->GetLogName(), visible, remotelyVisible, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
#endif // __BANK
			
			networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, visible);
			SafeCast(CNetObjPhysical, networkObject)->SetRemotelyVisibleInCutscene(remotelyVisible);
		}
	}
}

void CommandSetEntityLocallyInvisible(int entityId)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	
	if (pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:SET_ENTITY_LOCALLY_INVISIBLE - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			SafeCast(CNetObjEntity, networkObject)->SetOverridingLocalVisibility(false, "SET_ENTITY_LOCALLY_INVISIBLE", true, true);
		}
	}
}

void CommandSetEntityLocallyVisible(int entityId)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pEntity)
	{
		netObject *networkObject = pEntity->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:SET_ENTITY_LOCALLY_VISIBLE - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			SafeCast(CNetObjEntity, networkObject)->SetOverridingLocalVisibility(true, "SET_ENTITY_LOCALLY_VISIBLE", true, true);
		}
	}
}

bool CommandIsDamageTrackerActiveOnNetworkId(int networkId)
{
	if (!CNetwork::IsGameInProgress())
	{
		Assertf(0, "%s:IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID - Can't call this when no network game running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	bool damageTrackerActive = false;

	if (SCRIPT_VERIFY_NETWORK_ACTIVE("IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID"))
	{
		if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

			if (scriptVerifyf(pPhysical, "%s:IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID - entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
			         CNetObjPhysical *netObjPhysical = static_cast<CNetObjPhysical *>(networkObject);

					damageTrackerActive = netObjPhysical->IsTrackingDamage();
				}
			}
		}
    }

    if (damageTrackerActive)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID returning TRUE");
    }
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID returning FALSE");
    }

    return damageTrackerActive;
}

void CommandActivateDamageTrackerOnNetworkId(int networkId, bool bActivate)
{
	if (!CNetwork::IsGameInProgress())
	{
		Assertf(0, "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - Can't call this when no network game running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID");

	if (SCRIPT_VERIFY_NETWORK_ACTIVE("ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID"))
	{
		if (scriptVerifyf(networkId > NETWORK_INVALID_OBJECT_ID, "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - invalid network id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

			if (scriptVerifyf(pPhysical, "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				netObject *networkObject = pPhysical->GetNetworkObject();

				if (scriptVerifyf(networkObject, "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
		            CNetObjPhysical *netObjPhysical = static_cast<CNetObjPhysical *>(networkObject);

					if(bActivate)
					{
						Assertf(!netObjPhysical->IsTrackingDamage(), "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - Damage tracking is already activated on this object", CTheScripts::GetCurrentScriptNameAndProgramCounter());

						scriptDebugf1("%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - Activate Damage Tracking - '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netObjPhysical->GetLogName());

						netObjPhysical->ActivateDamageTracking();
					}
					else
					{
						Assertf(netObjPhysical->IsTrackingDamage(), "%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - Damage tracking is not activated on this object", CTheScripts::GetCurrentScriptNameAndProgramCounter());

						scriptDebugf1("%s:ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID - DeActivate Damage Tracking - '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netObjPhysical->GetLogName());

						netObjPhysical->DeactivateDamageTracking();
					}
				}
			}
        }
    }
}

bool CommandIsDamageTrackerActiveOnPlayer(int playerIndex)
{
	if (!CNetwork::IsGameInProgress())
	{
		Assertf(0, "%s:IS_DAMAGE_TRACKER_ACTIVE_ON_PLAYER - Can't call this when no network game running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	bool damageTrackerActive = false;

	CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

	if (targetPlayer && targetPlayer->GetPlayerPed())
	{
		damageTrackerActive = static_cast<CNetObjPhysical *>(targetPlayer->GetPlayerPed()->GetNetworkObject())->IsTrackingDamage();
	}

	if (damageTrackerActive)
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_DAMAGE_TRACKER_ACTIVE_ON_PLAYER returning TRUE");
	}
	else
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_DAMAGE_TRACKER_ACTIVE_ON_PLAYER returning FALSE");
	}

	return damageTrackerActive;
}

void CommandActivateDamageTrackerOnPlayer(int playerIndex, bool bActivate)
{
	if (!CNetwork::IsGameInProgress())
	{
		Assertf(0, "%s:ACTIVATE_DAMAGE_TRACKER_ON_PLAYER - Can't call this when no network game running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "ACTIVATE_DAMAGE_TRACKER_ON_PLAYER");

	CNetGamePlayer* targetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

	if (targetPlayer && targetPlayer->GetPlayerPed())
	{
		CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, targetPlayer->GetPlayerPed()->GetNetworkObject());

		if (bActivate)
		{
			Assertf(!netObjPhysical->IsTrackingDamage(), "%s:ACTIVATE_DAMAGE_TRACKER_ON_PLAYER - Damage tracking is already activated on this player", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			scriptDebugf1("%s:ACTIVATE_DAMAGE_TRACKER_ON_PLAYER - Activate Damage Tracking.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			netObjPhysical->ActivateDamageTracking();
		}
		else
		{
			Assertf(netObjPhysical->IsTrackingDamage(), "%s:ACTIVATE_DAMAGE_TRACKER_ON_PLAYER - Damage tracking is not activated on this player", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			scriptDebugf1("%s:ACTIVATE_DAMAGE_TRACKER_ON_PLAYER - DeActivate Damage Tracking.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			netObjPhysical->DeactivateDamageTracking();
		}
	}
}

bool CommandIsSphereVisibleToAnotherMachine(const scrVector & scrPosIn, float radius)
{
    // not sure if radius is allowed to be 0, just to be safe:
    if (radius == 0.0f)
        radius = 0.001f;

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_SPHERE_VISIBLE_TO_ANOTHER_MACHINE");

    return NetworkInterface::IsVisibleToAnyRemotePlayer(Vector3(scrPosIn), radius, 100.0f, NULL);
}

bool CommandIsSphereVisibleToPlayer(int playerIndex, const scrVector & scrPosIn, float radius)
{
	// not sure if radius is allowed to be 0, just to be safe:
	if (radius == 0.0f)
		radius = 0.001f;

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_SPHERE_VISIBLE_TO_PLAYER");

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	return NetworkInterface::IsVisibleToPlayer(pPlayer, Vector3(scrPosIn), radius, 100.0f);
}

bool CommandIsSphereVisibleToTeam(int teamId, const scrVector & scrPosIn, float radius)
{
	// not sure if radius is allowed to be 0, just to be safe:
	if (radius == 0.0f)
		radius = 0.001f;

	if (teamId < 0)
		return false;

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_SPHERE_VISIBLE_TO_TEAM");

	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
    const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);

		if((pPlayer->GetTeam() == teamId) && NetworkInterface::IsVisibleToPlayer(pPlayer, Vector3(scrPosIn), radius, 100.0f))
		{
			return true;
		}
	}

	return false;
}

void CommandReserveNetworkMissionObjects(int numObjectsToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_NETWORK_MISSION_OBJECTS - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_NETWORK_MISSION_OBJECTS"))
	{
		u32 maxNetObjObjects = static_cast<s32>(MAX_NUM_NETOBJOBJECTS);
		u32 numLocallyReservedObjects = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedObjects();

		if (scriptVerifyf(numObjectsToReserve >= 0, "%s:RESERVE_NETWORK_MISSION_OBJECTS - Number of objects to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numLocallyReservedObjects+numObjectsToReserve <= maxNetObjObjects, "%s:RESERVE_NETWORK_MISSION_OBJECTS - Number of objects to reserve is too high! (Max:%d, This reservation: %d, Local reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjObjects, numObjectsToReserve, numLocallyReservedObjects))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numObjectsToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedObjects() || 
				scriptVerifyf(numObjectsToReserve+numLocallyReservedObjects+numReservedObjects <= maxNetObjObjects, "%s:RESERVE_NETWORK_MISSION_OBJECTS(%d) - There are too many objects reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numObjectsToReserve, maxNetObjObjects))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveGlobalObjects(numObjectsToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_NETWORK_MISSION_OBJECTS");
}

void CommandReserveNetworkMissionPeds(int numPedsToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_NETWORK_MISSION_PEDS - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_NETWORK_MISSION_PEDS"))
	{
		u32 maxNetObjPeds = static_cast<s32>(MAX_NUM_NETOBJPEDS);
		u32 numLocallyReservedPeds = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedPeds();

		if (scriptVerifyf(numPedsToReserve >= 0, "%s:RESERVE_NETWORK_MISSION_PEDS - Number of peds to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numLocallyReservedPeds+numPedsToReserve <= maxNetObjPeds, "%s:RESERVE_NETWORK_MISSION_PEDS - Number of peds to reserve is too high! (Max:%d, This reservation: %d, Local reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjPeds, numPedsToReserve, numLocallyReservedPeds))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numPedsToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedPeds() || 
				scriptVerifyf(numPedsToReserve+numLocallyReservedPeds+numReservedPeds <= maxNetObjPeds, "%s:RESERVE_NETWORK_MISSION_PEDS(%d) - There are too many peds reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numPedsToReserve, maxNetObjPeds))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveGlobalPeds(numPedsToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_NETWORK_MISSION_PEDS");
}

void CommandReserveNetworkMissionVehicles(int numVehiclesToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_NETWORK_MISSION_VEHICLES - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_NETWORK_MISSION_VEHICLES"))
	{
		u32 maxNetObjVehicles = static_cast<s32>(MAX_NUM_NETOBJVEHICLES);
		u32 numLocallyReservedVehicles = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedVehicles();

		if (scriptVerifyf(numVehiclesToReserve >= 0, "%s:RESERVE_NETWORK_MISSION_VEHICLES - Number of objects to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numLocallyReservedVehicles+numVehiclesToReserve <= maxNetObjVehicles, "%s:RESERVE_NETWORK_MISSION_VEHICLES - Number of objects to reserve is too high! (Max:%d, This reservation: %d, Local reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjVehicles, numVehiclesToReserve, numLocallyReservedVehicles))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numVehiclesToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedVehicles() || 
				scriptVerifyf(numVehiclesToReserve+numLocallyReservedVehicles+numReservedVehicles <= maxNetObjVehicles, "%s:RESERVE_NETWORK_MISSION_VEHICLES(%d) - There are too many objects reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numVehiclesToReserve, maxNetObjVehicles))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveGlobalVehicles(numVehiclesToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_NETWORK_MISSION_VEHICLES");
}

void CommandReserveLocalNetworkMissionObjects(int numObjectsToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_LOCAL_NETWORK_MISSION_OBJECTS - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_LOCAL_NETWORK_MISSION_OBJECTS"))
	{
		u32 maxNetObjObjects = static_cast<s32>(MAX_NUM_NETOBJOBJECTS);
		u32 numGloballyReservedObjects = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedObjects();

		if (scriptVerifyf(numObjectsToReserve >= 0, "%s:RESERVE_LOCAL_NETWORK_MISSION_OBJECTS - Number of objects to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numGloballyReservedObjects+numObjectsToReserve <= maxNetObjObjects, "%s:RESERVE_LOCAL_NETWORK_MISSION_OBJECTS - Number of objects to reserve is too high! (Max:%d, This reservation: %d, Global reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjObjects, numObjectsToReserve, numGloballyReservedObjects))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numObjectsToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedObjects() || 
				scriptVerifyf(numObjectsToReserve+numGloballyReservedObjects+numReservedObjects <= maxNetObjObjects, "%s:RESERVE_LOCAL_NETWORK_MISSION_OBJECTS(%d) - There are too many objects reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numObjectsToReserve, maxNetObjObjects))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveLocalObjects(numObjectsToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_LOCAL_NETWORK_MISSION_OBJECTS");
}

void CommandReserveLocalNetworkMissionPeds(int numPedsToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_LOCAL_NETWORK_MISSION_PEDS - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_LOCAL_NETWORK_MISSION_PEDS"))
	{
		u32 maxNetObjPeds = static_cast<s32>(MAX_NUM_NETOBJPEDS);
		u32 numGloballyReservedPeds = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedPeds();

		if (scriptVerifyf(numPedsToReserve >= 0, "%s:RESERVE_LOCAL_NETWORK_MISSION_PEDS - Number of peds to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numGloballyReservedPeds+numPedsToReserve <= maxNetObjPeds, "%s:RESERVE_LOCAL_NETWORK_MISSION_PEDS - Number of peds to reserve is too high! (Max:%d, This reservation: %d, Global reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjPeds, numPedsToReserve, numGloballyReservedPeds))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numPedsToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedPeds() || 
				scriptVerifyf(numPedsToReserve+numGloballyReservedPeds+numReservedPeds <= maxNetObjPeds, "%s:RESERVE_LOCAL_NETWORK_MISSION_PEDS(%d) - There are too many peds reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numPedsToReserve, maxNetObjPeds))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveLocalPeds(numPedsToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_LOCAL_NETWORK_MISSION_PEDS");
}

void CommandReserveLocalNetworkMissionVehicles(int numVehiclesToReserve)
{
	if(!scriptVerifyf(CNetwork::IsGameInProgress(), "%s:RESERVE_LOCAL_NETWORK_MISSION_VEHICLES - Network Game Not In Progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return;
	}

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("RESERVE_LOCAL_NETWORK_MISSION_VEHICLES"))
	{
		u32 maxNetObjVehicles = static_cast<s32>(MAX_NUM_NETOBJVEHICLES);
		u32 numGloballyReservedVehicles = CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedVehicles();

		if (scriptVerifyf(numVehiclesToReserve >= 0, "%s:RESERVE_LOCAL_NETWORK_MISSION_VEHICLES - Number of objects to reserve is less than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(numGloballyReservedVehicles+numVehiclesToReserve <= maxNetObjVehicles, "%s:RESERVE_LOCAL_NETWORK_MISSION_VEHICLES - Number of vehicles to reserve is too high! (Max:%d, This reservation: %d, Global reservation: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxNetObjVehicles, numVehiclesToReserve, numGloballyReservedVehicles))
		{
			u32 numReservedObjects         = 0;
			u32 numReservedPeds            = 0;
			u32 numReservedVehicles        = 0;

			CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects, CTheScripts::GetCurrentGtaScriptHandler());

			if (numVehiclesToReserve <= CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedVehicles() || 
				scriptVerifyf(numVehiclesToReserve+numGloballyReservedVehicles+numReservedVehicles <= maxNetObjVehicles, "%s:RESERVE_LOCAL_NETWORK_MISSION_VEHICLES(%d) - There are too many vehicles reserved by other scripts. Maximum exceeded (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numVehiclesToReserve, maxNetObjVehicles))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->ReserveLocalVehicles(numVehiclesToReserve);
			}
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESERVE_LOCAL_NETWORK_MISSION_VEHICLES");
}

bool CommandCanRegisterMissionObjects(int numObjects)
{
    bool canRegisterObject = true;
	bool bCanRegister = CNetwork::IsGameInProgress() && (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene() || NetworkInterface::AreCutsceneEntitiesNetworked());
	
	if (bCanRegister && SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_OBJECTS"))
	{
		if (scriptVerifyf(numObjects > 0, "%s:CAN_REGISTER_MISSION_OBJECTS - the amount is not greater than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			canRegisterObject = NetworkInterface::CanRegisterObjects(0, 0, numObjects, 0, 0, true);

			if (canRegisterObject)
			{
				DEV_ONLY(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->MarkCanRegisterMissionEntitiesCalled(0, 0, numObjects));
			}
		}
	}

    if (canRegisterObject)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_OBJECTS returning TRUE");
	}
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_OBJECTS returning FALSE");
    }

    return canRegisterObject;
}

bool CommandCanRegisterMissionPeds(int numPeds)
{
    bool canRegisterPed = true;
	bool bCanRegister = CNetwork::IsGameInProgress() && (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene() || NetworkInterface::AreCutsceneEntitiesNetworked());

	if (bCanRegister && SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_PEDS"))
	{
		if (scriptVerifyf(numPeds > 0, "%s:CAN_REGISTER_MISSION_PEDS - the amount is not greater than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			canRegisterPed = NetworkInterface::CanRegisterObjects(numPeds, 0, 0, 0, 0, true);

			if (canRegisterPed)
			{
				DEV_ONLY(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->MarkCanRegisterMissionEntitiesCalled(numPeds, 0, 0));
			}
		}
	}

    if (canRegisterPed)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_PEDS returning TRUE");
    }
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_PEDS returning FALSE");
    }

    return canRegisterPed;
}

bool CommandCanRegisterMissionVehicles(int numVehicles)
{
    bool canRegisterVehicle = true;
	bool bCanRegister = CNetwork::IsGameInProgress() && (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene() || NetworkInterface::AreCutsceneEntitiesNetworked());

	if (bCanRegister && SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_VEHICLES"))
	{
		if (scriptVerifyf(numVehicles > 0, "%s:CAN_REGISTER_MISSION_VEHICLES - the amount is not greater than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_Script);

			// we just do the test for automobiles - as this will return the same result as calling for any other vehicle type
			canRegisterVehicle = NetworkInterface::CanRegisterObjects(0, numVehicles, 0, 0, 0, true);

			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);

			if (canRegisterVehicle)
			{
				DEV_ONLY(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->MarkCanRegisterMissionEntitiesCalled(0, numVehicles, 0));
			}
		}
	}

    if (canRegisterVehicle)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_VEHICLES returning TRUE");
	}
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_VEHICLES returning FALSE");
    }

    return canRegisterVehicle;
}

bool CommandCanRegisterMissionPickups(int numPickups)
{
    bool canRegisterPickups = true;
	bool bCanRegister = CNetwork::IsGameInProgress() && (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene() || NetworkInterface::AreCutsceneEntitiesNetworked());

	if (bCanRegister && SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_PICKUPS"))
	{
		if (scriptVerifyf(numPickups > 0, "%s:CAN_REGISTER_MISSION_PICKUPS - the amount is not greater than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			canRegisterPickups = NetworkInterface::CanRegisterObjects(0, 0, 0, numPickups, 0, true);
		}
	}

    if (canRegisterPickups)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_PICKUPS returning TRUE");
	}
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_PICKUPS returning FALSE");
    }

    return canRegisterPickups;
}

bool CommandCanRegisterMissionDoors(int numDoors)
{
	bool canRegisterDoors = true;

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_DOORS"))
	{
		if (scriptVerifyf(numDoors > 0, "%s:CAN_REGISTER_MISSION_DOORS - the amount is not greater than 0!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			canRegisterDoors = NetworkInterface::CanRegisterObjects(0, 0, 0, 0, numDoors, true);
		}
	}

	if (canRegisterDoors)
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_DOORS returning TRUE");
	}
	else
	{
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_DOORS returning FALSE");
	}

	return canRegisterDoors;
}

bool CommandCanRegisterMissionEntities(int numPeds, int numVehicles, int numObjects, int numPickups)
{
    bool canRegisterEntities = true;
	bool bCanRegister = CNetwork::IsGameInProgress() && (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene() || NetworkInterface::AreCutsceneEntitiesNetworked());

	if (bCanRegister && SCRIPT_VERIFY_NETWORK_REGISTERED("CAN_REGISTER_MISSION_ENTITIES"))
	{
		if (scriptVerifyf(numPeds >= 0 && numVehicles >= 0 && numObjects >= 0 && numPickups >= 0, "%s:CAN_REGISTER_MISSION_ENTITIES - no negative amounts allowed", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			(numPeds > 0 || numVehicles > 0 || numObjects > 0 || numPickups > 0))
		{
			CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_Script);

			// we just do the test for automobiles - as this will return the same result as calling for any other vehicle type
			canRegisterEntities = NetworkInterface::CanRegisterObjects(numPeds, numVehicles, numObjects, numPickups, 0, true);

			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);

			if (canRegisterEntities)
			{
				DEV_ONLY(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->MarkCanRegisterMissionEntitiesCalled(numPeds, numVehicles, numObjects));
			}
		}
	}

    if (canRegisterEntities)
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_ENTITIES returning TRUE");
	}
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CAN_REGISTER_MISSION_ENTITIES returning FALSE");
    }

    return canRegisterEntities;
}

int CommandGetNumReservedMissionObjects(bool bIncludeAllScripts, int reservationType)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumReservedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numObjects;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_RESERVED_MISSION_OBJECTS"))
	{
		switch (reservationType)
		{
		case RT_ALL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetTotalNumReservedObjects();
		case RT_LOCAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedObjects();
		case RT_GLOBAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedObjects();
		default:
			Assertf(0, "GET_NUM_RESERVED_MISSION_OBJECTS - unrecognised reservation type %d", reservationType);
		}
	}

	return 0;
}

int CommandGetNumReservedMissionPeds(bool bIncludeAllScripts, int reservationType)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumReservedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numPeds;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_RESERVED_MISSION_PEDS"))
	{
		switch (reservationType)
		{
		case RT_ALL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetTotalNumReservedPeds();
		case RT_LOCAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedPeds();
		case RT_GLOBAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedPeds();
		default:
			Assertf(0, "GET_NUM_RESERVED_MISSION_PEDS - unrecognised reservation type %d", reservationType);
		}
	}

	return 0;
}

int CommandGetNumReservedMissionVehicles(bool bIncludeAllScripts, int reservationType)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumReservedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numVehicles;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_RESERVED_MISSION_VEHICLES"))
	{
		switch (reservationType)
		{
		case RT_ALL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetTotalNumReservedVehicles();
		case RT_LOCAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumLocalReservedVehicles();
		case RT_GLOBAL:
			return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumGlobalReservedVehicles();
		default:
			Assertf(0, "GET_NUM_RESERVED_MISSION_VEHICLES - unrecognised reservation type %d", reservationType);
		}
	}

	return 0;
}

int CommandGetNumCreatedMissionObjects(bool bIncludeAllScripts)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumCreatedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numObjects;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_CREATED_MISSION_OBJECTS"))
	{
		return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumCreatedObjects();
	}

	return 0;
}

int CommandGetNumCreatedMissionPeds(bool bIncludeAllScripts)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumCreatedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numPeds;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_CREATED_MISSION_PEDS"))
	{
		return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumCreatedPeds();
	}

	return 0;
}

int CommandGetNumCreatedMissionVehicles(bool bIncludeAllScripts)
{
	if (bIncludeAllScripts)
	{
		u32 numPeds, numVehicles, numObjects;
		CTheScripts::GetScriptHandlerMgr().GetNumCreatedScriptEntities(numPeds, numVehicles, numObjects);
		return (int)numVehicles;
	}
	else if (SCRIPT_VERIFY_NETWORK_REGISTERED("GET_NUM_CREATED_MISSION_VEHICLES"))
	{
		return CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNumCreatedVehicles();
	}

	return 0;
}

void CommandGetReservedMissionEntitiesForThread(int threadId, int& reservedPeds, int& reservedVehicles, int& reservedObjects, int& createdPeds, int& createdVehicles, int& createdObjects)
{
	GtaThread *pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(threadId)));

	reservedPeds = reservedVehicles = reservedObjects = 0;
	createdPeds = createdVehicles = createdObjects = 0;

	if (scriptVerifyf(pThread, "%s:GET_RESERVED_MISSION_ENTITIES_FOR_THREAD - the thread is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CGameScriptId scriptId(*pThread);
	
		CGameScriptHandler* pHandler = SafeCast(CGameScriptHandler, CTheScripts::GetScriptHandlerMgr().GetScriptHandler(scriptId));

		if (scriptVerifyf(pHandler, "%s:GET_RESERVED_MISSION_ENTITIES_FOR_THREAD - couldn't find a script handler for the thread (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptId.GetLogName()))
		{
			if (pHandler->GetNetworkComponent())
			{
				CGameScriptHandlerNetwork* pNetHandler = SafeCast(CGameScriptHandlerNetwork, pHandler);
		
				reservedPeds = pNetHandler->GetTotalNumReservedPeds();
				reservedVehicles = pNetHandler->GetTotalNumReservedVehicles();
				reservedObjects = pNetHandler->GetTotalNumReservedObjects();
				createdPeds = pNetHandler->GetNumCreatedPeds();
				createdVehicles = pNetHandler->GetNumCreatedVehicles();
				createdObjects = pNetHandler->GetNumCreatedObjects();
			}
		}
	}
}

void CommandGetReservedMissionEntitiesInArea(const scrVector &areaCentre, bool bIncludeLocalPlayersScripts, int& reservedPeds, int& reservedVehicles, int& reservedObjects)
{
	Vector3 vAreaCentre = Vector3(areaCentre);

	u32 remotelyReservedPeds, remotelyReservedVehicles, remotelyReservedObjects;
	u32 remotelyCreatedPeds, remotelyCreatedVehicles, remotelyCreatedObjects;

	CTheScripts::GetScriptHandlerMgr().GetRemoteScriptEntityReservations(remotelyReservedPeds, remotelyReservedVehicles, remotelyReservedObjects, remotelyCreatedPeds, remotelyCreatedVehicles, remotelyCreatedObjects, &vAreaCentre);

	u32 locallyReservedPeds = 0;
	u32 locallyReservedVehicles = 0;
	u32 locallyReservedObjects = 0;

	if (bIncludeLocalPlayersScripts)
	{
		CTheScripts::GetScriptHandlerMgr().GetNumReservedScriptEntities(locallyReservedPeds, locallyReservedVehicles, locallyReservedObjects);
	}

	reservedPeds = (int)(remotelyReservedPeds+locallyReservedPeds);
	reservedVehicles = (int)(remotelyReservedVehicles+locallyReservedVehicles);
	reservedObjects = (int)(remotelyCreatedObjects+locallyReservedObjects);
}

int CommandGetMaxNumNetworkObjects()
{
    return MAX_NUM_NETOBJOBJECTS;
}

int CommandGetMaxNumNetworkPeds()
{
    return MAX_NUM_NETOBJPEDS;
}

int CommandGetMaxNumNetworkVehicles()
{
    return MAX_NUM_NETOBJVEHICLES;
}

int CommandGetMaxNumNetworkPickups()
{
    return MAX_NUM_NETOBJPICKUPS;
}

void CommandNetworkSetObjectScopeDistance(int objectId, int dist)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(objectId);

	if (pObj && 
		scriptVerifyf(pObj->IsAScriptEntity(), "%s:NETWORK_SET_OBJECT_SCOPE_DISTANCE - the object is not a script entity", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(pObj->GetNetworkObject(), "%s:NETWORK_SET_OBJECT_SCOPE_DISTANCE - the object is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(dist >= 0, "%s:NETWORK_SET_OBJECT_SCOPE_DISTANCE - scope dist is < 0", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		static_cast<CNetObjObject*>(pObj->GetNetworkObject())->SetScriptAdjustedScopeDistance((u32)dist);
	}
}

void CommandNetworkAllowCloningWhileInTutorial(int networkID, bool allow)
{
	Assertf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));
	if (scriptVerifyf(pPhysical, "%s:NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter())
	&&  scriptVerifyf(pPhysical->GetNetworkObject(), "%s:NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL - no network object with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		netObject* netObj = pPhysical->GetNetworkObject();
		if(scriptVerifyf(IsVehicleObjectType(netObj->GetObjectType()) || netObj->GetObjectType() == NET_OBJ_TYPE_PED, "%s:NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL - not a valid object type. Only vehicles and Peds are supported", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, netObj);
			netPhysical->SetAllowCloneWhileInTutorial(allow);

	#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL", "%s", netPhysical->GetLogName());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Allow", allow ? "TRUE" : "FALSE");
	#endif // ENABLE_NETWORK_LOGGING
		}
	}
}

void CommandNetworkSetTaskCutsceneInScopeMultipler(float multiplier)
{
	if(NetworkInterface::IsGameInProgress())
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "NETWORK_SET_TASK_CUTSCENE_INSCOPE_MULTIPLER", "");
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Multiplier", "%.2f", multiplier);
#endif // ENABLE_NETWORK_LOGGING

		static const float MAX_MULTIPLAYER_VALUE = 5.0f;
		static const float MIN_MULTIPLAYER_VALUE = 1.0f;
		if(multiplier < MIN_MULTIPLAYER_VALUE)
		{
			scriptAssertf(0, "NETWORK_SET_TASK_CUTSCENE_INSCOPE_MULTIPLER trying to set multiplier to invalid value: %.2f. Defaulting it to %.2f", multiplier, MIN_MULTIPLAYER_VALUE);
			multiplier = MIN_MULTIPLAYER_VALUE;
		}
		
		if(multiplier > MAX_MULTIPLAYER_VALUE)
		{
			scriptAssertf(0, "NETWORK_SET_TASK_CUTSCENE_INSCOPE_MULTIPLER trying to set multiplier to larger than allowed value: %.2f. Defaulting it to %.2f", multiplier, MAX_MULTIPLAYER_VALUE);
			multiplier = 5.0f;
		}

		CTaskCutScene::SetScopeDistanceMultiplierOverride(multiplier);
	}
}

struct sNetworkTimeTracker
{
	sNetworkTimeTracker() { Reset(); }

	void Reset()
	{
		m_LastTime = 0;
		m_LastFrame = 0;
		m_IsValid = false;
	}

	unsigned m_LastTime;
	unsigned m_LastFrame;
	bool m_IsValid;
};

sNetworkTimeTracker g_NetworkTimeAccurate; 
sNetworkTimeTracker g_NetworkTimeLocked; 

bool VerifyTime(unsigned nTimeValue, sNetworkTimeTracker& networkTimeTracker OUTPUT_ONLY(, const char* szTag))
{
	// assume valid
	bool isValid = true; 

	// grab current frame 
	unsigned nCurrentFrame = fwTimer::GetSystemFrameCount();

	// work out if time is less than previous value 
	// use sign bit for proper check to handle wrapping
	u32 u = (static_cast<u32>(nTimeValue) - static_cast<u32>(networkTimeTracker.m_LastTime)); 
	bool bTimeNowLessThanBefore = (u & 0x80000000u) != 0;
	
	// if less, not good
	if(bTimeNowLessThanBefore && networkTimeTracker.m_IsValid)
	{
#if !__NO_OUTPUT
		if(!fwTimer::IsUserPaused())
		{
			gnetAssertf(!NetworkInterface::IsHost(), "VerifyTime::%s - Host time value has gone backwards! Shouldn't happen", szTag); 
			if(nCurrentFrame > networkTimeTracker.m_LastFrame)
			{
				isValid = false;
				gnetDebug1("VerifyTime::%s - Network Time has moved backwards - Was: %u, Now: %u", szTag, networkTimeTracker.m_LastTime, nTimeValue);
			}
		}
#endif

		// don't let the time go backwards (confuses the scripts!)
		nTimeValue = networkTimeTracker.m_LastTime;
	}
#if !__NO_OUTPUT
	// also check for stall
	else if(nTimeValue == networkTimeTracker.m_LastTime)
	{
		if(!fwTimer::IsUserPaused() && nCurrentFrame > networkTimeTracker.m_LastFrame)
		{
			isValid = false;
			gnetDebug1("VerifyTime::%s - Network Time has stalled at %u", szTag, nTimeValue);
		}
	}
	
	// check if we've assigned a value of 0 when previously valid
	const unsigned LARGE_TIME_DIFFERENCE = 30000;
	if(networkTimeTracker.m_IsValid && (nTimeValue == 0) && (Abs(0 - static_cast<int>(networkTimeTracker.m_LastTime)) > LARGE_TIME_DIFFERENCE))
	{
		isValid = false;
		gnetDebug1("VerifyTime::%s - Network time assigned to 0, LastTime: %u", szTag, networkTimeTracker.m_LastTime);
	}
#endif

	// track values
	networkTimeTracker.m_LastTime = nTimeValue;
	networkTimeTracker.m_LastFrame = nCurrentFrame;
    networkTimeTracker.m_IsValid = true;

	return isValid;
}

int CommandGetNetworkTime()
{
	Assertf(NetworkInterface::GetNetworkClock().HasStarted(), "%s:GET_NETWORK_TIME - Clock not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(NetworkInterface::GetNetworkClock().HasStarted())
	{
		unsigned nNetworkTime = CNetwork::GetNetworkTimeLocked();

		// validate and return time
		OUTPUT_ONLY(bool bTimeValid =) VerifyTime(nNetworkTime, g_NetworkTimeLocked OUTPUT_ONLY(, "LOCKED"));

#if !__NO_OUTPUT
			if(!bTimeValid && NetworkInterface::IsHost())
				CNetwork::DumpNetworkTimeInfo();
#endif

		return static_cast<int>(nNetworkTime);
	}
	return 0;
}

int CommandGetNetworkTimeAccurate()
{
	Assertf(NetworkInterface::GetNetworkClock().HasStarted(), "%s:GET_NETWORK_TIME_ACCURATE - Clock not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(NetworkInterface::GetNetworkClock().HasStarted())
	{
		unsigned nNetworkTime = CNetwork::GetNetworkTime();

		OUTPUT_ONLY(bool bTimeValid =) VerifyTime(nNetworkTime, g_NetworkTimeAccurate OUTPUT_ONLY(, "ACCURATE"));

#if !__NO_OUTPUT
		if(!bTimeValid && NetworkInterface::IsHost())
			CNetwork::DumpNetworkTimeInfo();
#endif

		return static_cast<int>(nNetworkTime);
	}
	return 0;
}

int CommandGetNetworkTimePausable()
{
    Assertf(NetworkInterface::GetNetworkClock().HasStarted(), "%s:GET_NETWORK_TIME_PAUSABLE - Clock not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if(NetworkInterface::GetNetworkClock().HasStarted())
    {
        unsigned nNetworkTimePausable = CNetwork::GetNetworkTimePausable();

        return static_cast<int>(nNetworkTimePausable);
    }
    return 0;
}

bool CommandHasNetworkClockStarted()
{
    return NetworkInterface::GetNetworkClock().HasStarted();
}

void CommandStopNetworkTimePausable()
{
    Assertf(NetworkInterface::GetNetworkClock().HasStarted(), "%s:STOP_NETWORK_TIME_PAUSABLE - Clock not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if(NetworkInterface::GetNetworkClock().HasStarted())
    {
        CNetwork::StopNetworkTimePausable();
    }
}

void CommandRestartNetworkTimePausable()
{
    Assertf(NetworkInterface::GetNetworkClock().HasStarted(), "%s:RESTART_NETWORK_TIME_PAUSABLE - Clock not started!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if(NetworkInterface::GetNetworkClock().HasStarted())
    {
        CNetwork::RestartNetworkTimePausable();
    }
}

int CommandGetTimeOffset(int nTime, int nOffset)
{
	u32 nNewTime = static_cast<u32>(nTime) + static_cast<u32>(nOffset);
	return static_cast<int>(nNewTime);
}

bool CommandIsTimeLessThan(int nTime1, int nTime2)
{
	u32 u = (static_cast<u32>(nTime1) - static_cast<u32>(nTime2)); 
	return (u & 0x80000000u) != 0;
}

bool CommandIsTimeGreaterThan(int nTime1, int nTime2)
{
	u32 u = (static_cast<u32>(nTime2) - static_cast<u32>(nTime1)); 
	return (u & 0x80000000u) != 0;
}

bool CommandIsTimeEqualTo(int nTime1, int nTime2)
{
	return nTime1 == nTime2; 
}

int CommandGetTimeDifference(int nTime1, int nTime2)
{
	// this works because both are represented as int (and not unsigned)
	// if the ranges start becoming more than half the acceptable range, we'll hit 
	// problems but that would indicate other problems
	return nTime1 - nTime2; 
}

const char* CommandGetTimeAsString(int nTime)
{
	unsigned nMs = nTime % 1000;
	nTime /= 1000;
	unsigned nSeconds = nTime % 60;
	nTime /= 60;
	unsigned nMinutes = nTime % 60;
	nTime /= 60;
	// no boundary to hours in original script function
	unsigned nHours = nTime; // % 24;

	static const u32 MAX_STRING = 32;
	static char szTime[MAX_STRING];

	if(nHours > 0)
	{
		formatf(szTime, MAX_STRING, "%d:%02d:%02d.%03d", nHours, nMinutes, nSeconds, nMs);
	}
	else
	{
		formatf(szTime, MAX_STRING, "%02d:%02d.%03d", nMinutes, nSeconds, nMs);
	}

	return szTime;
}

const char* CommandGetCloudTimeAsString()
{
	// max of 16 chars plus null terminator
	static const unsigned nMaxLength = 17;
	static char szPosixTime[nMaxLength];
	formatf(szPosixTime, nMaxLength, "%" I64FMT "X", rlGetPosixTime());

	return szPosixTime;
}

int CommandGetCloudTimeAsInt()
{
	return static_cast<int>(rlGetPosixTime());
}

void CommandConvertPosixTime(int nPosixTime, int& hData)
{
	time_t t = static_cast<time_t>(nPosixTime);
	struct tm tInfo = { 0 };
#if RSG_PROSPERO
	gmtime_s(&t, &tInfo);
#else
	struct tm* ptIpfo = gmtime(&t);
	if (scriptVerifyf(ptIpfo != nullptr, "%s:CONVERT_POSIX_TIME failed to convert %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPosixTime))
	{
		tInfo = *ptIpfo;
	}
#endif

	scrValue* pData = (scrValue*)&hData;
	int size = 0;
	pData[size++].Int = 1900 + tInfo.tm_year;
	pData[size++].Int = tInfo.tm_mon + 1;
	pData[size++].Int = tInfo.tm_mday;
	pData[size++].Int = tInfo.tm_hour;
	pData[size++].Int = tInfo.tm_min;
	pData[size++].Int = tInfo.tm_sec;
}

void CommandNetworkSetWeather(const char* UNUSED_PARAM(previousTypeName), const int UNUSED_PARAM(nextCycleIndex), bool UNUSED_PARAM(bForce), bool UNUSED_PARAM(bBroadcast))
{
    // deprecated
}

void CommandNetworkSetInSpectatorMode(bool inSpectatorMode, int pedIndex)
{
	if (!inSpectatorMode)
	{
		scriptDebugf1("%s:NETWORK_SET_IN_SPECTATOR_MODE - inSpectatorMode='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inSpectatorMode ? "true":"false");
		NetworkInterface::SetSpectatorObjectId(NETWORK_INVALID_OBJECT_ID);
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_SPECTATOR_MODE - \"FALSE\"");
	}
	else
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(SCRIPT_VERIFY(pPed, "NETWORK_SET_IN_SPECTATOR_MODE: Failed to find ped to add!"))
		{
			netObject* netobj = pPed->GetNetworkObject();
			if(SCRIPT_VERIFY(netobj, "NETWORK_SET_IN_SPECTATOR_MODE: ped is not network ped, does not have a network object!"))
			{
				scriptAssertf(NetworkInterface::CanSpectateObjectId(netobj, false), "%s : NETWORK_SET_IN_SPECTATOR_MODE - Invalid to spectate %s, Players are in different tutorial session.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netobj->GetLogName());

				if (NetworkInterface::CanSpectateObjectId(netobj, false))
				{
					scriptDebugf1("%s:NETWORK_SET_IN_SPECTATOR_MODE - inSpectatorMode='%s', pedIndex='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inSpectatorMode ? "true":"false", netobj->GetLogName());
					NetworkInterface::SetSpectatorObjectId(netobj->GetObjectID());
					WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_SPECTATOR_MODE - \"TRUE\"");
				}
			}
		}
	}
}

void CommandNetworkSetInSpectatorModeExtended(bool inSpectatorMode, int pedIndex, bool skipTutorialCheck)
{
	if (!inSpectatorMode)
	{
		scriptDebugf1("%s:NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED - inSpectatorMode='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inSpectatorMode ? "true":"false");
		NetworkInterface::SetSpectatorObjectId(NETWORK_INVALID_OBJECT_ID);
		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED - \"FALSE\"");
	}
	else
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(SCRIPT_VERIFY(pPed, "NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED: Failed to find ped to add!"))
		{
			netObject* netobj = pPed->GetNetworkObject();
			if(SCRIPT_VERIFY(netobj, "NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED: ped is not network ped, does not have a network object!"))
			{
				scriptAssertf(NetworkInterface::CanSpectateObjectId(netobj, skipTutorialCheck), "%s : NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED - Invalid to spectate %s, Players are in different tutorial session.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netobj->GetLogName());

				if (NetworkInterface::CanSpectateObjectId(netobj, skipTutorialCheck))
				{
					scriptDebugf1("%s:NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED - inSpectatorMode='%s', pedIndex='%s', skipTutorialCheck='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inSpectatorMode ? "true":"false", netobj->GetLogName(), skipTutorialCheck ? "true":"false");
					NetworkInterface::SetSpectatorObjectId(netobj->GetObjectID());
					WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED - \"TRUE\"");
				}
			}
		}
	}
}

void CommandNetworkSetInFreeCamMode(bool inFreeCamMode)
{
    CPed * player = CGameWorld::FindLocalPlayer();

	if (player)
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());

		if(netObjPlayer)
		{
#if !__NO_OUTPUT
			if(netObjPlayer->IsUsingFreeCam() != inFreeCamMode)
			{
				gnetDebug1("%s:NETWORK_SET_IN_FREE_CAM_MODE - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inFreeCamMode ? "ON" : "OFF");
				scrThread::PrePrintStackTrace();
			}
#endif // !__NO_OUTPUT

            netObjPlayer->SetUsingFreeCam(inFreeCamMode);
        }
    }
}

void CommandNetworkSetAntagonisticToPlayer(bool inAntagonisticMode, int playerIndex)
{
	CPed * player = CGameWorld::FindLocalPlayer();
	if (player)
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());
		if(netObjPlayer)
		{
			if(inAntagonisticMode)
			{
				netObjPlayer->SetAntagonisticPlayerIndex(static_cast<PhysicalPlayerIndex>(playerIndex));
				WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_ANTAGONISTIC_TO_PLAYER - \"TRUE\"");
			}
			else
			{
				netObjPlayer->SetAntagonisticPlayerIndex(INVALID_PLAYER_INDEX);
				WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_ANTAGONISTIC_TO_PLAYER - \"FALSE\"");
			}
		}
	}
}

bool CommandNetworkIsInSpectatorMode()
{
    if (NetworkInterface::IsInSpectatorMode())
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_IS_IN_SPECTATOR_MODE returning TRUE");
        return true;
    }
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_IS_IN_SPECTATOR_MODE returning FALSE");
        return false;
    }
}

void CommandNetworkSetInMPTutorial(bool inMPTutorial)
{
    CNetwork::SetInMPTutorial(inMPTutorial);

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_MP_TUTORIAL");
}

void CommandNetworkSetInMPCutscene(bool inMPCutscene, bool makePlayerInvincible)
{
	Assertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_SET_IN_MP_CUTSCENE - Network game is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (NetworkInterface::IsGameInProgress())
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "NETWORK_SET_IN_MP_CUTSCENE", "%s", inMPCutscene ? "TRUE" : "FALSE");
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

		NetworkInterface::SetInMPCutscene(inMPCutscene, makePlayerInvincible);

		WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_SET_IN_MP_CUTSCENE");
	}
}

bool CommandNetworkIsInMPCutscene()
{
	return NetworkInterface::IsInMPCutscene();
}

void CommandNetworkHideProjectileInCutscene()
{
	CProjectileManager::ToggleHideProjectilesInCutscene(true);
}

bool CommandNetworkIsPlayerInMPCutscene(int playerIndex)
{
	bool bIsInCutscene = false;

	CNetGamePlayer * pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);

	if (pPlayer)
	{
		bIsInCutscene = pPlayer->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE;
	}

	return bIsInCutscene;
}

bool CommandNetworkIsInCodeDevelopmentMode()
{
	return PARAM_scriptCodeDev.Get();
}

void CommandDisplayPlayerName(int playerIndex, const char *pPlayerName, const char *pDisplayInfo1, const char *pDisplayInfo2, const char *pDisplayInfo3, const char *pDisplayInfo4, const char *pDisplayInfo5)
{
#define __TEST_PLAYER_NAMES (0)

#if __TEST_PLAYER_NAMES
	pPlayerNameA;  // change the params to match this
	pDisplayInfo1A;  // change the params to match this

	char cPlayerName[50];
	char cPlayerInfo1[50];

	sprintf(cPlayerName, "Test_Player_Name");
	sprintf(cPlayerInfo1, "~wanted_star~~wanted_star~~wanted_star~304");

	const char *pPlayerName = &cPlayerName[0];
	const char *pDisplayInfo1 = &cPlayerInfo1[0];  // new pointers to the temporary strings to test with
#endif // __TEST_PLAYER_NAMES

	scriptAssertf(pPlayerName, "%s:DISPLAY_PLAYER_NAME - Empty Player Name.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	static char cFinalDisplayString[CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME] = "\0";
	sysMemSet(cFinalDisplayString, 0, sizeof(cFinalDisplayString));

	if (pDisplayInfo5 && pDisplayInfo5[0] != '\0')
	{
		safecat(cFinalDisplayString, pDisplayInfo5, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
		safecat(cFinalDisplayString, "~n~", CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	if (pDisplayInfo4 && pDisplayInfo4[0] != '\0')
	{
		safecat(cFinalDisplayString, pDisplayInfo4, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
		safecat(cFinalDisplayString, "~n~", CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	if (pDisplayInfo3 && pDisplayInfo3[0] != '\0')
	{
		safecat(cFinalDisplayString, pDisplayInfo3, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
		safecat(cFinalDisplayString, "~n~", CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	if (pDisplayInfo2 && pDisplayInfo2[0] != '\0')
	{
		safecat(cFinalDisplayString, pDisplayInfo2, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
		safecat(cFinalDisplayString, "~n~", CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	if (pDisplayInfo1 && pDisplayInfo1[0] != '\0')
	{
		safecat(cFinalDisplayString, pDisplayInfo1, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
		safecat(cFinalDisplayString, "~n~", CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	if (pPlayerName && pPlayerName[0] != '\0')
	{
		safecat(cFinalDisplayString, pPlayerName, CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);
	}

	CPed *pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);

	if (pPlayer && cFinalDisplayString[0] != '\0')
	{
		Assertf(strlen(cFinalDisplayString) <= CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME, "%s:DISPLAY_PLAYER_NAME - Player name specified is too long! (Max allowed is %d characters)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);

		static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject())->SetPlayerDisplayName(cFinalDisplayString);
	}
}

void CommandBeginDisplayPlayerName()
{
	if (scriptVerifyf(!CScriptHud::ms_AbovePlayersHeadDisplay.IsConstructing(), "BEGIN_DISPLAY_PLAYER_NAME - BEGIN_DISPLAY_PLAYER_NAME has already been called"))
	{
		CScriptHud::ms_AbovePlayersHeadDisplay.Reset(true);
	}
}

void CommandEndDisplayPlayerName(s32 PlayerIndex)
{
	if (scriptVerifyf(CScriptHud::ms_AbovePlayersHeadDisplay.IsConstructing(), "END_DISPLAY_PLAYER_NAME - BEGIN_DISPLAY_PLAYER_NAME hasn't been called yet"))
	{
		char cFinalDisplayString[CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME];
		CScriptHud::ms_AbovePlayersHeadDisplay.FillStringWithRows(cFinalDisplayString, NELEM(cFinalDisplayString) );
		CScriptHud::ms_AbovePlayersHeadDisplay.Reset(false);

		CPed *pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		if (pPlayer && cFinalDisplayString[0] != '\0')
		{
			Assertf(strlen(cFinalDisplayString) < CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME, "%s:END_DISPLAY_PLAYER_NAME - Player name specified is too long! (Max allowed is %d characters)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME);

			if (scriptVerifyf(pPlayer->GetNetworkObject(), "END_DISPLAY_PLAYER_NAME - couldn't find network object for this player"))
			{
				static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject())->SetPlayerDisplayName(cFinalDisplayString);
			}
		}
	}
}

bool CommandDisplayPlayerNameHasFreeRows()
{
	return CScriptHud::ms_AbovePlayersHeadDisplay.HasFreeRows();
}

void CommandSetNetworkVehicleRespotTimer(int networkID, int timer, bool flashRemotely, bool flashLocally)
{
	Assertf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:SET_NETWORK_VEHICLE_RESPOT_TIMER - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

	if (scriptVerifyf(pPhysical, "%s:SET_NETWORK_VEHICLE_RESPOT_TIMER - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(pPhysical->GetIsTypeVehicle(), "%s:SET_NETWORK_VEHICLE_RESPOT_TIMER - the entity with this id (%d) is not a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkID) &&
			scriptVerifyf(timer >= 0, "%s:SET_NETWORK_VEHICLE_RESPOT_TIMER - the timer cannot be negative", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CVehicle* vehicle = static_cast<CVehicle*>(pPhysical);

			vehicle->SetFlashRemotelyDuringRespotting(flashRemotely);
			vehicle->SetFlashLocallyDuringRespotting(flashLocally);
			vehicle->SetRespotCounter(static_cast<u32>(timer));
        }
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SET_NETWORK_VEHICLE_RESPOT_TIMER");
}

bool CommandIsNetworkVehicleRunningRespotTimer(int networkID)
{
	Assertf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:IS_NETWORK_VEHICLE_RUNNING_RESPOT_TIMER - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

	if (scriptVerifyf(pPhysical, "%s:IS_NETWORK_VEHICLE_RUNNING_RESPOT_TIMER - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(pPhysical->GetIsTypeVehicle(), "%s:IS_NETWORK_VEHICLE_RUNNING_RESPOT_TIMER - the entity with this id (%d) is not a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkID))
		{
			CVehicle* vehicle = static_cast<CVehicle*>(pPhysical);
			return vehicle->IsBeingRespotted() || vehicle->IsRunningSecondaryRespotCounter();
		}
	}

	return false;
}

void CommandSetNetworkVehicleAsGhost(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pVehicle)
	{
		if (scriptVerifyf(pVehicle->GetNetworkObject(), "%s:SET_NETWORK_VEHICLE_AS_GHOST - no vehicle is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->SetAsGhost(bSet BANK_ONLY(, SPG_SCRIPT, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
		}
	}
}

void CommandNetworkSetMaxPositionDeltaMultiplier(int VehicleIndex, float multiplier)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pVehicle)
	{
		if (scriptVerifyf(pVehicle->GetNetworkObject(), "%s:SET_NETWORK_VEHICLE_MAX_POSITION_DELTA_MULTIPLIER - vehicle is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjVehicle* netObjVehicle = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());

			netBlenderLinInterp *netBlender = SafeCast(netBlenderLinInterp, netObjVehicle->GetNetBlender());

			if (scriptVerifyf(netBlender, "%s:SET_NETWORK_VEHICLE_MAX_POSITION_DELTA_MULTIPLIER - vehicle doesn't have a valid netBlenderLinInterp!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
				scriptVerifyf(multiplier >=1.0f && multiplier <= 3.0f, "%s:SET_NETWORK_VEHICLE_MAX_POSITION_DELTA_MULTIPLIER - Multiplier out of safe range (1.0 - 3.0)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				netBlender->SetMaxPositionDeltaMultiplier(multiplier);
				NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "CHANGING_MAX_POSITION_DELTA_MULTIPLIER", netObjVehicle->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Multiplier", "%2.2f", multiplier);
			}
		}
	}
}

void CommandNetworkEnableHighSpeedEdgeFallDetection(int VehicleIndex, bool enable)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pVehicle)
	{
		if (scriptVerifyf(pVehicle->GetNetworkObject(), "%s:SET_NETWORK_ENABLE_HIGH_SPEED_EDGE_FALL_DETECTION - vehicle is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjVehicle* netObjVehicle = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());
			CNetBlenderVehicle *netBlender = SafeCast(CNetBlenderVehicle, netObjVehicle->GetNetBlender());
			if (scriptVerifyf(netBlender, "%s:SET_NETWORK_ENABLE_HIGH_SPEED_EDGE_FALL_DETECTION - vehicle doesn't have a valid CNetBlenderVehicle!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				netBlender->SetHighSpeedEdgeFallDetector(enable);				
				NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "SETTING_NETWORK_ENABLE_HIGH_SPEED_EDGE_FALL_DETECTION ", netObjVehicle->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Setting it to ", "%s", enable ? "True" : "False");
			}
		}
	}
}

void CommandSetInvertGhosting(bool bSet)
{
	CNetObjPhysical::SetInvertGhosting(bSet);

	if (bSet)
	{
		if (CTheScripts::GetCurrentGtaScriptHandler()->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_GHOST_SETTINGS) == 0)
		{
			CScriptResource_GhostSettings ghostSettingsResource;
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(ghostSettingsResource);
		}
	}
}

bool CommandIsEntityInGhostCollision(int EntityIndex)
{
	const CPhysical *pPhysical = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

	if (pPhysical)
	{
		return CNetworkGhostCollisions::IsInGhostCollision(*pPhysical);
	}

	return false;
}

void CommandSetLocalPlayerAsGhost(bool bSet, bool bInvertGhosting)
{
	CPed * player = CGameWorld::FindLocalPlayer();
	if (player)
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());
		if(netObjPlayer)
		{
			if (bSet)
			{
				netObjPlayer->SetGhostPlayerFlags(0 BANK_ONLY(, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
			}
			netObjPlayer->SetAsGhost(bSet BANK_ONLY(, SPG_SCRIPT, CTheScripts::GetCurrentScriptNameAndProgramCounter()));

			CommandSetInvertGhosting(bInvertGhosting);
		}
	}
}

bool CommandIsEntityAGhost(int entityIndex)
{
	bool bIsGhost = false;

	const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity && pEntity->GetNetworkObject())
	{
		bIsGhost = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject())->IsGhost();
	}

	return bIsGhost;
}

void CommandSetNonParticipantsOfThisScriptAsGhosts(bool bSet)
{
	CPed * player = CGameWorld::FindLocalPlayer();
	if (player)
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());
		if(netObjPlayer)
		{
			if (bSet)
			{
				if (scriptVerifyf(netObjPlayer->IsGhost(), "%s:SET_NON_PARTICIPANTS_OF_THIS_SCRIPT_AS_GHOSTS - the local player is already set as a ghost", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					netObjPlayer->SetPlayerGhostedForNonScriptPartipants((CGameScriptId&)CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId() BANK_ONLY(, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
				}
			}
			else
			{
				netObjPlayer->ResetPlayerGhostedForNonScriptPartipants(BANK_ONLY(CTheScripts::GetCurrentScriptNameAndProgramCounter()));
			}
		}
	}
}

void CommandSetRemotePlayerAsGhost(int PlayerId, bool bSet)
{
	CPed* pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerId);

	if (scriptVerifyf(pPlayerPed, "%s:SET_REMOTE_PLAYER_AS_GHOST - the remote player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(PlayerId >=0 && PlayerId < MAX_NUM_PHYSICAL_PLAYERS, "%s:SET_REMOTE_PLAYER_AS_GHOST - invalid player id", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(!pPlayerPed->IsLocalPlayer(), "%s:SET_REMOTE_PLAYER_AS_GHOST - the player is local", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPed * player = CGameWorld::FindLocalPlayer();

		if (scriptVerifyf(player, "%s:SET_REMOTE_PLAYER_AS_GHOST - no local player", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());

			if (scriptVerifyf(netObjPlayer, "%s:SET_REMOTE_PLAYER_AS_GHOST - the local player is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				PlayerFlags playerFlags = netObjPlayer->GetGhostPlayerFlags();
				bool hasChanged = false;

				if (bSet)
				{
					if ((playerFlags & (1 << PlayerId)) == 0)
					{
						hasChanged = true;
					}
					playerFlags |= (1<<PlayerId);
				}
				else
				{
					if ((playerFlags & (1 << PlayerId)) != 0)
					{
						hasChanged = true;
					}
					playerFlags &= ~(1<<PlayerId);
				}

				if (hasChanged)
				{
					netObjPlayer->SetGhostPlayerFlags(playerFlags BANK_ONLY(, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
				}
			}
		}
	}
}

void CommandSetGhostAlpha(int alpha)
{
	if (scriptVerifyf(alpha > 0 && alpha < 255, "%s:SET_GHOST_ALPHA - invalid alpha", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetObjPhysical::SetGhostAlpha((u8)alpha);

		if (CTheScripts::GetCurrentGtaScriptHandler()->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_GHOST_SETTINGS) == 0)
		{
			CScriptResource_GhostSettings ghostSettingsResource;
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(ghostSettingsResource);
		}
	}
}

void CommandSetEntityGhostedForGhostPlayers(int entityIndex, bool bSet)
{
	CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex);

	if (pPhysical && scriptVerifyf(pPhysical->GetNetworkObject(), "%s:SET_ENTITY_GHOSTED_FOR_GHOST_PLAYERS - this can only be called on networked entities", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		SafeCast(CNetObjPhysical, pPhysical->GetNetworkObject())->SetGhostedForGhostPlayers(bSet);
	}
}

void CommandSetParachuteOfEntityGhostedForGhostPlayers(int pedIndex, bool bSet)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if (pPed)
	{
		CTaskParachute* pTask = static_cast<CTaskParachute *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
		if(pTask && pTask->GetParachute())
		{
			CNetObjObject* parachuteNetObject = static_cast<CNetObjObject*>(pTask->GetParachute()->GetNetworkObject());
			if (parachuteNetObject)
			{
				parachuteNetObject->SetGhostedForGhostPlayers(bSet);
			}
		}
	}
}

void CommandResetGhostAlpha()
{
	CNetObjPhysical::ResetGhostAlpha();
}

void CommandSetMsgForLoadingScreen(const char* msg)
{
    Assertf(msg, "%s:SET_MSG_FOR_LOADING_SCREEN - Message is empty!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(!CWarningScreen::IsActive())
    {
        CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, msg);
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SET_MSG_FOR_LOADING_SCREEN");
    }
}

//------------------------------------------------------------------------------------------------------------------------
// Prototype network respawn commands
// Automatic coordinate generation via navmesh queries

bool CommandNetworkFindLargestBunchOfPlayers(bool bFriendly, Vector3& vPosition)
{
	//Find the largest bunch of players.
	return CNetRespawnMgr::FindLargestBunchOfPlayers(bFriendly, vPosition);
}

bool CommandNetworkStartRespawnSearch(const scrVector & vSearchPos, float fSearchRadius, const scrVector & vLookAtPos, s32 iFlags)
{
	CNetRespawnMgr::StartSearch(vSearchPos, vLookAtPos, 0.0f, fSearchRadius, (u16)iFlags | CNetRespawnMgr::FLAG_SCRIPT_COMMAND);
	return true;
}

bool CommandNetworkStartRespawnSearchForPlayer(int iPlayerIndex, const scrVector & vSearchPos, float fSearchRadius, const scrVector & vLookAtPos, s32 iFlags)
{
	CPed* pPlayerPed = CTheScripts::FindNetworkPlayerPed(iPlayerIndex);
	if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_START_RESPAWN_SEARCH_FOR_PLAYER - Player index is invalid."))
	{
		//Create the input.
		CNetRespawnMgr::StartSearchInput input(vSearchPos);
		input.m_pPed = pPlayerPed;
		input.m_vTargetPos = vLookAtPos;
		input.m_fMinDist = 0.0f;
		input.m_fMaxDist = fSearchRadius;
		input.m_uFlags = (u16)iFlags | CNetRespawnMgr::FLAG_SCRIPT_COMMAND;
		
		//Start the search.
		CNetRespawnMgr::StartSearch(input);
		
		return true;
	}
	
	return false;
}

bool CommandNetworkStartRespawnSearchInAngledAreaForPlayer(int iPlayerIndex, const scrVector & vSearchAngledAreaPt1, const scrVector & vSearchAngledAreaPt2, float fSearchAngledAreaWidth, const scrVector & vLookAtPos, s32 iFlags)
{
	CPed* pPlayerPed = CTheScripts::FindNetworkPlayerPed(iPlayerIndex);
	if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_START_RESPAWN_SEARCH_IN_ANGLED_AREA_FOR_PLAYER - Player index is invalid."))
	{
		//Create the input.
		CNetRespawnMgr::StartSearchInput input(vSearchAngledAreaPt1, vSearchAngledAreaPt2, fSearchAngledAreaWidth);
		input.m_pPed = pPlayerPed;
		input.m_vTargetPos = vLookAtPos;
		input.m_uFlags = (u16)iFlags | CNetRespawnMgr::FLAG_SCRIPT_COMMAND;

		//Start the search.
		CNetRespawnMgr::StartSearch(input);

		return true;
	}

	return false;
}

bool CommandNetworkStartRespawnSearchForTeam(int iTeam, const scrVector & vSearchPos, float fSearchRadius, const scrVector & vLookAtPos, s32 iFlags)
{
	//Create the input.
	CNetRespawnMgr::StartSearchInput input(vSearchPos);
	input.m_iTeam = iTeam;
	input.m_vTargetPos = vLookAtPos;
	input.m_fMinDist = 0.0f;
	input.m_fMaxDist = fSearchRadius;
	input.m_uFlags = (u16)iFlags | CNetRespawnMgr::FLAG_SCRIPT_COMMAND;

	//Start the search.
	CNetRespawnMgr::StartSearch(input);

	return true;
}

int CommandNetworkQueryRespawnResults(int & iNumResults)
{
	iNumResults = 0;

	if(CNetRespawnMgr::IsSearchComplete())
	{
		if(CNetRespawnMgr::WasSearchSuccessful())
		{
			iNumResults = CNetRespawnMgr::GetNumSearchResults();
			return CNetRespawnMgr::RESPAWN_QUERY_RESULTS_SUCCEEDED;
		}
		else
		{
			return CNetRespawnMgr::RESPAWN_QUERY_RESULTS_FAILED;
		}
	}
	else if(CNetRespawnMgr::IsSearchActive())
	{
		return CNetRespawnMgr::RESPAWN_QUERY_RESULTS_STILL_WORKING;
	}
	else
	{
		return CNetRespawnMgr::RESPAWN_QUERY_RESULTS_NO_SEARCH_ACTIVE;
	}
}

void CommandNetworkCancelRespawnSearch()
{
	CNetRespawnMgr::CancelSearch();
}

void CommandNetworkGetRespawnResult(int iIndex, Vector3 & vPosition, float & fHeading)
{
	CNetRespawnMgr::GetSearchResults(iIndex, vPosition, fHeading);
}

s32 CommandNetworkGetRespawnResultFlags(int iIndex)
{
	return CNetRespawnMgr::GetSearchResultFlags(iIndex);
}

void CommandNetworkStartSoloTutorialSession()
{
    CPed *pPlayerPed = FindPlayerPed();

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_START_SOLO_TUTORIAL_SESSION - No local player ped!"))
    {
        CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_START_SOLO_TUTORIAL_SESSION - Local player ped is not networked!"))
        {
            netObjPlayer->StartSoloTutorial();
        }
    }
}

void CommandNetworkAllowGangToJoinTutorialSession(int iTeam, int iTutorialInstance)
{
	CPed *pPlayerPed = FindPlayerPed();

	if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_ALLOW_GANG_TO_JOIN_TUTORIAL_SESSION - No local player ped!"))
	{
		CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

		if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_ALLOW_GANG_TO_JOIN_TUTORIAL_SESSION - Local player ped is not networked!"))
		{
			// eTEAM_MAX_GANGS no longer exists due to script using it as a standard team without realizing associations in code - Switching to hard coded value (3) instead.
			const int maxGangs = 3;
			if(SCRIPT_VERIFY(iTeam >= 0 && iTeam < maxGangs, "NETWORK_ALLOW_GANG_TO_JOIN_TUTORIAL_SESSION - Invalid Team Index!"))
			{
                const unsigned MAX_TUTORIAL_INSTANCE = MAX_NUM_PHYSICAL_PLAYERS * 2;
				if(SCRIPT_VERIFY(iTutorialInstance >= 0 && iTutorialInstance < MAX_TUTORIAL_INSTANCE, "NETWORK_ALLOW_GANG_TO_JOIN_TUTORIAL_SESSION - Invalid tutorial instance index!"))
				{
					netObjPlayer->StartGangTutorial(iTeam, iTutorialInstance);
				}
			}
		}
	}
}

void CommandNetworkEndTutorialSession()
{
    CPed *pPlayerPed = FindPlayerPed();

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_END_TUTORIAL_SESSION - No local player ped!"))
    {
        CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_END_TUTORIAL_SESSION - Local player ped is not networked!"))
        {
            if(SCRIPT_VERIFY(netObjPlayer->IsInTutorialSession(), "NETWORK_END_TUTORIAL_SESSION - Local player is not in a tutorial session!"))
            {
                netObjPlayer->EndTutorial();
            }
        }
    }
}

bool CommandNetworkIsInTutorialSession()
{
    bool isInTutorialSession = false;

    CPed *pPlayerPed = FindPlayerPed();

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_IS_IN_TUTORIAL_SESSION - No local player ped!"))
    {
        CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_IS_IN_TUTORIAL_SESSION - Local player ped is not networked!"))
        {
            isInTutorialSession = netObjPlayer->IsInTutorialSession();
        }
    }

    return isInTutorialSession;
}

bool CommandNetworkIsWaitingPopulationClearedInTutorialSession()
{
	bool isWaitingPopulationClearedInTutorialSession = false;

	CPed *pPlayerPed = FindPlayerPed();

	if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_WAITING_POP_CLEAR_TUTORIAL_SESSION - No local player ped!"))
	{
		CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

		if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_WAITING_POP_CLEAR_TUTORIAL_SESSION - Local player ped is not networked!"))
		{
			isWaitingPopulationClearedInTutorialSession = netObjPlayer->WaitingPopulationClearedInTutorialSession();
		}
	}

	return isWaitingPopulationClearedInTutorialSession;
}

bool CommandNetworkIsTutorialSessionChangePending()
{
	bool isTutorialSessionChangePending = false;

	CPed *pPlayerPed = FindPlayerPed();

	if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_IS_TUTORIAL_SESSION_CHANGE_PENDING - No local player ped!"))
	{
		CNetObjPlayer *netObjPlayer = pPlayerPed ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : 0;

		if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_IS_TUTORIAL_SESSION_CHANGE_PENDING - Local player ped is not networked!"))
		{
			isTutorialSessionChangePending = netObjPlayer->IsTutorialSessionChangePending();
		}
	}

	return isTutorialSessionChangePending;
}

int CommandNetworkGetPlayerTutorialSessionInstance(int playerIndex)
{
    int instanceID = -1;

    CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(playerIndex);

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_GET_PLAYER_TUTORIAL_SESSION_INSTANCE - Player doesn't exist!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_GET_PLAYER_TUTORIAL_SESSION_INSTANCE - Player ped is not networked!"))
        {
            if(SCRIPT_VERIFY(netObjPlayer->IsInTutorialSession(), "NETWORK_GET_PLAYER_TUTORIAL_SESSION_INSTANCE - Player is not in a tutorial session!"))
            {
                instanceID = static_cast<int>(netObjPlayer->GetTutorialInstanceID());
            }
        }
    }

    return instanceID;
}

bool CommandNetworkArePlayersInSameTutorialSession(int firstPlayerIndex, int secondPlayerIndex)
{
    CNetGamePlayer *pFirstPlayer  = CTheScripts::FindNetworkPlayer(firstPlayerIndex);
    CNetGamePlayer *pSecondPlayer = CTheScripts::FindNetworkPlayer(secondPlayerIndex);

    bool bPlayersInSameSession = false;

    if(pFirstPlayer && pSecondPlayer)
    {
        bPlayersInSameSession = !NetworkInterface::ArePlayersInDifferentTutorialSessions(*pFirstPlayer, *pSecondPlayer);
    }

    return bPlayersInSameSession;
}

void CommandBlockProxyMigrationBetweenTutorialSessions(bool block)
{
	if(CNetObjProximityMigrateable::IsBlockingProxyMigrationBetweenTutorialSessions() != block)
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "BLOCK_PROXY_MIGRATION_BETWEEN_TUTORIAL_SESSIONS", block?"BLOCK":"UNBLOCK");
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // ENABLE_NETWORK_LOGGING
		CNetObjProximityMigrateable::BlockProxyMigrationBetweenTutorialSessions(block);
	}
}

void CommandNetworkConcealPlayer(int playerIndex, bool conceal, bool allowDamagingWhileConcealed)
{
	CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(playerIndex);

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_CONCEAL_PLAYER - Player doesn't exist!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_CONCEAL_PLAYER - Player ped is not networked!"))
        {
#if ENABLE_NETWORK_LOGGING
            if(conceal)
            {
                NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "CONCEALING_PLAYER", netObjPlayer->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Allow Damage", "%s", allowDamagingWhileConcealed ? "TRUE" : "FALSE");
			}
            else
            {
                NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "UNCONCEALING_PLAYER", netObjPlayer->GetLogName());
            }
#endif // ENABLE_NETWORK_LOGGING
#if !__FINAL
            if(netObjPlayer->IsConcealed() != conceal)
            {
                scriptDebugf1("%s: NETWORK_CONCEAL_PLAYER %s : %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), netObjPlayer->GetLogName(), conceal ? "Concealing" : "Un-concealing");
                scrThread::PrePrintStackTrace();
            }
#endif // !__FINAL

            netObjPlayer->ConcealEntity(conceal, allowDamagingWhileConcealed);
        }
    }
}

void CommandNetworkConcealEntity(int entityId, bool conceal)
{
	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId);

	if(SCRIPT_VERIFY(pEntity, "NETWORK_CONCEAL_ENTITY - Entity doesn't exist!"))
	{
		if(scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_CONCEAL_ENTITY - Entity is not a physical (ped, vehicle, object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(pEntity));

			if(SCRIPT_VERIFY(pNetObj, "NETWORK_CONCEAL_ENTITY - Entity is not networked!"))
			{
#if ENABLE_NETWORK_LOGGING
				if(conceal)
				{
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "CONCEALING_ENTITY", pNetObj->GetLogName());
				}
				else
				{
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "UNCONCEALING_ENTITY", pNetObj->GetLogName());
				}
#endif // ENABLE_NETWORK_LOGGING

	#if !__FINAL
				if(pNetObj->IsConcealed() != conceal)
				{
					scriptDebugf1("%s: NETWORK_CONCEAL_ENTITY %s : %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pNetObj->GetLogName(), conceal ? "Concealing" : "Un-concealing");
					scrThread::PrePrintStackTrace();
				}
	#endif // !__FINAL

				pNetObj->ConcealEntity(conceal);
			}
		}
	}
}

bool CommandNetworkIsPlayerConcealed(int playerIndex)
{
    CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(playerIndex);

    if(SCRIPT_VERIFY(pPlayerPed, "NETWORK_IS_PLAYER_CONCEALED - Player doesn't exist!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject());

        if(SCRIPT_VERIFY(netObjPlayer, "NETWORK_IS_PLAYER_CONCEALED - Player ped is not networked!"))
        {
            return netObjPlayer->IsConcealed();
        }
    }

    return false;
}

bool CommandNetworkIsEntityConcealed(int entityIndex)
{
	const CPhysical *physical = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	if(SCRIPT_VERIFY(physical, "NETWORK_IS_ENTITY_CONCEALED - Entity doesn't exist!"))
	{
		CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

		if(SCRIPT_VERIFY(netObjPhysical, "NETWORK_IS_ENTITY_CONCEALED - Entity is not networked!"))
		{
			return netObjPhysical->IsConcealed();
		}
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------------------

bool CommandIsObjectReassignmentInProgress()
{
    if (CNetwork::GetObjectManager().GetReassignMgr().IsReassignmentInProgress())
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_OBJECT_REASSIGNMENT_IN_PROGRESS returning TRUE");
        return true;
    }
    else
    {
        WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "IS_OBJECT_REASSIGNMENT_IN_PROGRESS returning FALSE");
        return false;
    }
}

// these static variables are used to provide a scratchpad to the script to save and load blocks of data

void CommandSaveScriptArrayInScratchPad(int &AddressTemp, int Size, int scriptID, int scratchPadIndex)
{
    u8 *Address = reinterpret_cast<u8 *>(&AddressTemp);
    Size *= sizeof(scrValue);                          // Script returns size in script words

	Assertf(scratchPadIndex>=0, "%s:SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()))
	{
		CNetworkArrayScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetArrayScratchpad(scratchPadIndex);

		Assertf(netScratchPad.IsFree() || netScratchPad.GetScriptId() == scriptID, "%s:SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD - Another script has already saved data to this array!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		Assertf(Size <= netScratchPad.GetMaxSize(), "%s:SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD - Array size too big - not all data will be saved! (See a programmer if you need to save such a large array)", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		Size = MIN(Size, netScratchPad.GetMaxSize());

		netScratchPad.SaveArray(Address, Size, scriptID);
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD");
}

void CommandRestoreScriptArrayFromScratchPad(int &AddressTemp, int Size, int scriptID, int scratchPadIndex)
{
    u8 *Address = reinterpret_cast<u8 *>(&AddressTemp);
    Size *= sizeof(scrValue);                          // Script returns size in script words

	Assertf(scratchPadIndex>=0, "%s:RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()))
	{
		CNetworkArrayScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetArrayScratchpad(scratchPadIndex);

		Assertf(Size <= netScratchPad.GetMaxSize(), "%s:RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD - Array size too big - not all data will be saved! (See a programmer if you need to save such a large array)", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		Size = MIN(Size, netScratchPad.GetMaxSize());

		Assertf(netScratchPad.GetCurrentSize() == Size, "%s:RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD - Array size does not match array size passed to SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		Assertf(!netScratchPad.IsFree() && netScratchPad.GetScriptId() != scriptID, "%s:RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD - Called by a different script than the one that called SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (netScratchPad.GetScriptId() == scriptID)
		{
			netScratchPad.RestoreArray(Address, Size, scriptID);
		}
	}
	
    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD");
}

void CommandClearScriptArrayFromScratchPad(int scratchPadIndex)
{
	Assertf(scratchPadIndex>=0, "%s:CLEAR_SCRIPT_ARRAY_FROM_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:CLEAR_SCRIPT_ARRAY_FROM_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()))
    {
		CNetworkArrayScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetArrayScratchpad(scratchPadIndex);

		netScratchPad.Clear();
    }

    WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CLEAR_SCRIPT_ARRAY_FROM_SCRATCHPAD");
}

void CommandSaveScriptArrayInPlayerScratchPad(int &AddressTemp, int Size, int scriptID, int playerIndex, int scratchPadIndex)
{
	u8 *Address = reinterpret_cast<u8 *>(&AddressTemp);
	Size *= sizeof(scrValue);                          // Script returns size in script words

	Assertf(scratchPadIndex>=0, "%s:SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()) &&
		SCRIPT_VERIFY_PLAYER_INDEX("SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD", playerIndex))
	{
		CNetworkPlayerScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetPlayerScratchpad(scratchPadIndex);

		Assertf(netScratchPad.IsFree() || netScratchPad.GetScriptId() == scriptID, "%s:SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD - Another script has already saved data to this array!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		Assertf(Size <= netScratchPad.GetMaxSize(), "%s:SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD - Array size too big - not all data will be restored! (See a programmer if you need to save such a large array)", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		Size = MIN(Size, netScratchPad.GetMaxSize());

		netScratchPad.SaveArray(static_cast<PhysicalPlayerIndex>(playerIndex), Address, Size, scriptID);
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD");
}

void CommandRestoreScriptArrayFromPlayerScratchPad(int &AddressTemp, int Size, int scriptID, int playerIndex, int scratchPadIndex)
{
	u8 *Address = reinterpret_cast<u8 *>(&AddressTemp);
	Size *= sizeof(scrValue);                          // Script returns size in script words

	Assertf(scratchPadIndex>=0, "%s:RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()) &&
		SCRIPT_VERIFY_PLAYER_INDEX("RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD", playerIndex))
	{
		CNetworkPlayerScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetPlayerScratchpad(scratchPadIndex);

		Assertf(Size <= netScratchPad.GetMaxSize(), "%s:RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Array size too big - not all data will be restored! (See a programmer if you need to save such a large array)", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		Size = MIN(Size, netScratchPad.GetMaxSize());

		Assertf(netScratchPad.GetCurrentSize(static_cast<PhysicalPlayerIndex>(playerIndex)) == Size, "%s:RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Array size does not match array size passed to SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		Assertf(!netScratchPad.IsFree() && netScratchPad.GetScriptId() != scriptID, "%s:RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Called by a different script than the one that called SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (netScratchPad.GetScriptId() == scriptID)
		{
			netScratchPad.RestoreArray(static_cast<PhysicalPlayerIndex>(playerIndex), Address, Size, scriptID);
		}
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD");
}

void CommandClearScriptArrayFromPlayerScratchPad(int playerIndex, int scratchPadIndex)
{
	Assertf(scratchPadIndex>=0, "%s:CLEAR_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:CLEAR_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()) &&
		SCRIPT_VERIFY_PLAYER_INDEX("CLEAR_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD", playerIndex))
	{
		CNetworkPlayerScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetPlayerScratchpad(scratchPadIndex);

		netScratchPad.ClearForPlayer(static_cast<PhysicalPlayerIndex>(playerIndex));
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CLEAR_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD");
}

void CommandClearScriptArrayFromAllPlayerScratchPads(int scratchPadIndex)
{
	Assertf(scratchPadIndex>=0, "%s:CLEAR_SCRIPT_ARRAY_FROM_ALL_PLAYER_SCRATCHPADS - Scratchpad index is negative!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(scratchPadIndex<NetworkInterface::GetScriptScratchpads().GetNumScratchpads(), "%s:CLEAR_SCRIPT_ARRAY_FROM_ALL_PLAYER_SCRATCHPADS - Scratchpad index is too big!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if((scratchPadIndex >= 0) && (scratchPadIndex < NetworkInterface::GetScriptScratchpads().GetNumScratchpads()))
	{
		CNetworkPlayerScratchpad& netScratchPad = NetworkInterface::GetScriptScratchpads().GetPlayerScratchpad(scratchPadIndex);

		netScratchPad.Clear();
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "CLEAR_SCRIPT_ARRAY_FROM_ALL_PLAYER_SCRATCHPADS");
}

void CommandUsePlayerColourInsteadOfTeamColour(bool bActive)
{
    CScriptHud::bUsePlayerColourInsteadOfTeamColour = bActive;
}

bool CommandNetworkParamNetStartPosExists()
{
    return PARAM_netStartPos.Get();
}

Vector3 CommandNetworkGetParamNetStartPos()
{
    float coords[3] = {0, 0, 0};

    scriptVerifyf(PARAM_netStartPos.GetArray(coords, 3),
            "%s:NETWORK_GET_PARAM_NET_START_POS - Start pos param does not exist",
            CTheScripts::GetCurrentScriptNameAndProgramCounter());

    return Vector3(coords[0], coords[1], coords[2]);
}

bool CommandNetworkParamTeamExists()
{
	return PARAM_netTeam.Get();
}

int CommandNetworkGetParamTeam()
{
	int team = -1;

	scriptVerifyf(PARAM_netTeam.Get(team),
		"%s:NETWORK_GET_PARAM_TEAM - Team param does not exist",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());

	return team;
}

void CommandNetDebug(const char* pText, bool extraInfo)
{
    Assertf(pText, "%s:NET_DEBUG - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptDebugf3("\"%s\" Server: %s", CTheScripts::GetCurrentScriptName(), pText);
            else
                scriptDebugf3("\"%s\" Client: %s", CTheScripts::GetCurrentScriptName(), pText);
        }
        else
        {
            scriptDebugf3("%s", pText);
        }
    }
}

void CommandNetDebug_i(const char* pText, int OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_DEBUG_INT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptDebugf3("\"%s\" Server: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptDebugf3("\"%s\" Client: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptDebugf3("%s \"%d\"", pText, var);
        }
    }
}

void CommandNetDebug_f(const char* pText, float OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_DEBUG_FLOAT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptDebugf3("\"%s\" Server: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptDebugf3("\"%s\" Client: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptDebugf3("%s \"%f\"", pText, var);
        }

    }
}

void CommandNetDebug_v(const char* pText, const scrVector & OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_DEBUG_VECTOR - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptDebugf3("\"%s\" Server: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
            else
                scriptDebugf3("\"%s\" Client: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
        }
        else
        {
            scriptDebugf3("%s << %f, %f, %f >>", pText, var.x, var.y, var.z);
        }

    }
}

void CommandNetDebug_s(const char* pText, const char* s, bool extraInfo)
{
    Assertf(pText && s, "%s:NET_DEBUG_STRING - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText && s)
    {
        Assertf(strlen(pText)+strlen(s) < 256, "%s:NET_SCRIPT_PRINT_DEBUG_s - Strings to long", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptDebugf3("\"%s\" Server: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
            else
                scriptDebugf3("\"%s\" Client: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
        }
        else
        {
            scriptDebugf3("%s %s", pText, s);
        }
    }
}


void CommandNetWarning(const char* pText, bool extraInfo)
{
    Assertf(pText, "%s:NET_WARNING - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptWarningf("\"%s\" Server: %s", CTheScripts::GetCurrentScriptName(), pText);
            else
                scriptWarningf("\"%s\" Client: %s", CTheScripts::GetCurrentScriptName(), pText);
        }
        else
        {
            scriptWarningf("%s", pText);
        }

    }
}

void CommandNetWarning_i(const char* pText, int OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_WARNING_INT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptWarningf("\"%s\" Server: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptWarningf("\"%s\" Client: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptWarningf("%s \"%d\"", pText, var);
        }
    }
}

void CommandNetWarning_f(const char* pText, float OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_WARNING_FLOAT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptWarningf("\"%s\" Server: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptWarningf("\"%s\" Client: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptWarningf("%s \"%f\"", pText, var);
        }
    }
}

void CommandNetWarning_v(const char* pText, const scrVector & OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_WARNING_VECTOR - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptWarningf("\"%s\" Server: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
            else
                scriptWarningf("\"%s\" Client: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
        }
        else
        {
            scriptWarningf("%s << %f, %f, %f >>", pText, var.x, var.y, var.z);
        }
    }
}

void CommandNetWarning_s(const char* pText, const char* s, bool extraInfo)
{
    Assertf(pText && s, "%s:NET_WARNING_STRING - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText && s)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptWarningf("\"%s\" Server: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
            else
                scriptWarningf("\"%s\" Client: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
        }
        else
        {
            scriptWarningf("%s %s", pText, s);
        }
    }
}

void CommandNetError(const char* pText, bool extraInfo)
{
    Assertf(pText, "%s:NET_ERROR - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptErrorf("\"%s\" Server: %s ", CTheScripts::GetCurrentScriptName(), pText);
            else
                scriptErrorf("\"%s\" Client: %s ", CTheScripts::GetCurrentScriptName(), pText);
        }
        else
        {
            scriptErrorf("%s", pText);
        }
    }
}

void CommandNetError_i(const char* pText, int OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_ERROR_INT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptErrorf("\"%s\" Server: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptErrorf("\"%s\" Client: %s \"%d\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptErrorf("%s \"%d\"", pText, var);
        }
    }
}

void CommandNetError_f(const char* pText, float OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_ERROR_FLOAT - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptErrorf("\"%s\" Server: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
            else
                scriptErrorf("\"%s\" Client: %s \"%f\"", CTheScripts::GetCurrentScriptName(), pText, var);
        }
        else
        {
            scriptErrorf("%s \"%f\"", pText, var);
        }
    }
}

void CommandNetError_v(const char* pText, const scrVector & OUTPUT_ONLY(var), bool extraInfo)
{
    Assertf(pText, "%s:NET_ERROR_VECTOR - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptErrorf("\"%s\" Server: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
            else
                scriptErrorf("\"%s\" Client: %s << %f, %f, %f >>", CTheScripts::GetCurrentScriptName(), pText, var.x, var.y, var.z);
        }
        else
        {
            scriptErrorf("%s << %f, %f, %f >>", pText, var.x, var.y, var.z);
        }
    }
}

void CommandNetError_s(const char* pText, const char* s, bool extraInfo)
{
    Assertf(pText && s, "%s:NET_ERROR_STRING - empty debug text", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    if (pText && s)
    {
        if (extraInfo && CNetwork::ManagersAreInitialised() && scriptInterface::IsScriptRegistered(GET_SCRIPT_ID()))
        {
            if (CommandNetworkIsHostOfThisScript())
                scriptErrorf("\"%s\" Server: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
            else
                scriptErrorf("\"%s\" Client: %s %s", CTheScripts::GetCurrentScriptName(), pText, s);
        }
        else
        {
            scriptErrorf("%s %s", pText, s);
        }
    }
}

bool CommandNetworkIsPlayerVisible(int playerIndex)
{
	bool playerVisible = false;

	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);

	if (pPlayer)
	{
		playerVisible = static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject())->IsVisibleOnscreen();
	}

	return playerVisible;
}

bool  CommandIsHeadShotTrackingActive(int ModelHashKey, int typeOfWeapon)
{
	bool isHeadShotTracking = false;

	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
	Assertf( modelId.IsValid(), "%s:IS_HEADSHOT_TRACKING_ACTIVE - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	isHeadShotTracking = CNetworkHeadShotTracker::IsHeadShotTracking(modelId.GetModelIndex(), typeOfWeapon);

	return isHeadShotTracking;
}

void CommandStartHeadShotTracking(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:START_HEADSHOT_TRACKING - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    bool success = CNetworkHeadShotTracker::StartTracking(modelId.GetModelIndex(), TypeOfWeapon);

    if(success == false)
    {
        Assertf(success, "%s:START_HEADSHOT_TRACKING - Failed to start HeadShot tracking!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandStopHeadShotTracking(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:STOP_HEADSHOT_TRACKING - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    bool success = CNetworkHeadShotTracker::StopTracking(modelId.GetModelIndex(), TypeOfWeapon);

    if(success == false)
    {
        Assertf(success, "%s:STOP_HEADSHOT_TRACKING - Failed to stop HeadShot tracking!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

int CommandGetHeadShotTrackingResults(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:GET_HEADSHOT_TRACKING_RESULTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    u32 numHeadShots = CNetworkHeadShotTracker::GetTrackingResults(modelId.GetModelIndex(), TypeOfWeapon);

    return (int)numHeadShots;
}

void CommandResetHeadShotTrackingResults(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:RESET_HEADSHOT_TRACKING_RESULTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf( CNetworkHeadShotTracker::IsHeadShotTracking(modelId.GetModelIndex()), "%s:RESET_HEADSHOT_TRACKING_RESULTS - HeadShot tracking has not been activated for this model index!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkHeadShotTracker::ResetTrackingResults(modelId.GetModelIndex(), TypeOfWeapon);
}

bool  CommandIsKillTrackingActive(int ModelHashKey, int typeOfWeapon)
{
	bool isKillTracking = false;

	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
	Assertf( modelId.IsValid(), "%s:IS_KILL_TRACKING_ACTIVE - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	isKillTracking = CNetworkKillTracker::IsKillTracking(modelId.GetModelIndex(), typeOfWeapon);

	return isKillTracking;
}

void CommandStartKillTracking(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:START_KILL_TRACKING - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    bool success = CNetworkKillTracker::StartTracking(modelId.GetModelIndex(), TypeOfWeapon);

    if(success == false)
    {
        Assertf(success, "%s:START_KILL_TRACKING - Failed to start kill tracking!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandStopKillTracking(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:STOP_KILL_TRACKING - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    bool success = CNetworkKillTracker::StopTracking(modelId.GetModelIndex(), TypeOfWeapon);

    if(success == false)
    {
        Assertf(success, "%s:STOP_KILL_TRACKING - Failed to stop kill tracking!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

int CommandGetKillTrackingResults(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
	scriptAssertf( modelId.IsValid(), "%s:GET_KILL_TRACKING_RESULTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf( CNetworkKillTracker::IsKillTracking(modelId.GetModelIndex(), 0), "%s:GET_KILL_TRACKING_RESULTS - Not tracking kills for that model=%d!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey);

    u32 numKills = CNetworkKillTracker::GetTrackingResults(modelId.GetModelIndex(), TypeOfWeapon);

    return (int)numKills;
}

void CommandResetKillTrackingResults(int ModelHashKey, int TypeOfWeapon)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:RESET_KILL_TRACKING_RESULTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf( CNetworkKillTracker::IsKillTracking(modelId.GetModelIndex()), "%s:RESET_KILL_TRACKING_RESULTS - Kill tracking has not been activated for this model index!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkKillTracker::ResetTrackingResults(modelId.GetModelIndex(), TypeOfWeapon);
}

void CommandRegisterModelForRankPoints(int ModelHashKey)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:REGISTER_MODEL_FOR_RANK_POINTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    bool success = CNetworkKillTracker::RegisterModelIndexForRankPoints(modelId.GetModelIndex());

    if(success == false)
    {
        Assertf(success, "%s:REGISTER_MODEL_FOR_RANK_POINTS - Failed to register model index!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

int CommandGetNumKillsForRankPoints(int ModelHashKey)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:GET_NUM_KILLS_FOR_RANK_POINTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    u32 numKills = CNetworkKillTracker::GetNumKillsForRankPoints(modelId.GetModelIndex());

    return (int)numKills;
}

void CommandResetNumKillsForRankPoints(int ModelHashKey)
{
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &modelId);
    Assertf( modelId.IsValid(), "%s:RESET_NUM_KILLS_FOR_RANK_POINTS - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    Assertf( CNetworkKillTracker::IsTrackingForRankPoints(modelId.GetModelIndex()), "%s:RESET_NUM_KILLS_FOR_RANK_POINTS - Kill tracking for rank points has not been activated for this model index!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkKillTracker::ResetNumKillsForRankPoints(modelId.GetModelIndex());
}

void CommandStartTrackingCivilianKills()
{
    Assertf(!CNetworkKillTracker::IsTrackingCivilianKills(), "%s:START_TRACKING_CIVILIAN_KILLS - Kill tracking for civilians is already active!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkKillTracker::StartTrackingCivilianKills();
}

void CommandStopTrackingCivilianKills()
{
    Assertf(CNetworkKillTracker::IsTrackingCivilianKills(), "%s:STOP_TRACKING_CIVILIAN_KILLS - Kill tracking for civilians is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkKillTracker::StopTrackingCivilianKills();
}

bool CommandAreCivilianKillsBeingTracked()
{
    return CNetworkKillTracker::IsTrackingCivilianKills();
}

int CommandGetNumCivilianKills()
{
    Assertf(CNetworkKillTracker::IsTrackingCivilianKills(), "%s:GET_NUM_CIVILIAN_KILLS - Kill tracking for civilians is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    return (int)CNetworkKillTracker::GetNumCivilianKills();
}

void CommandResetNumCivilianKills()
{
    Assertf(CNetworkKillTracker::IsTrackingCivilianKills(), "%s:RESET_NUM_CIVILIAN_KILLS - Kill tracking for civilians is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    CNetworkKillTracker::ResetNumCivilianKills();
}

//Track Ped kills
void CommandStartTrackingPedKills(int pedClassification)
{
	Assertf(!CNetworkKillTracker::IsTrackingPedKills(pedClassification), "%s:START_TRACKING_PED_KILLS - Kill tracking for pedClassification %d is already active!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedClassification);
	CNetworkKillTracker::StartTrackingPedKills(pedClassification);
}

void CommandStopTrackingPedKills(int pedClassification)
{
	Assertf(CNetworkKillTracker::IsTrackingPedKills(pedClassification), "%s:STOP_TRACKING_PED_KILLS - Kill tracking for pedClassification %d is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedClassification);
	CNetworkKillTracker::StopTrackingPedKills(pedClassification);
}

bool CommandArePedKillsBeingTracked(int pedClassification)
{
	return CNetworkKillTracker::IsTrackingPedKills(pedClassification);
}

int CommandGetNumPedKills(int pedClassification)
{
	Assertf(CNetworkKillTracker::IsTrackingPedKills(pedClassification), "%s:GET_NUM_PED_KILLS - Kill tracking for pedClassification %d is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedClassification);
	return (int)CNetworkKillTracker::GetNumPedKills(pedClassification);
}

void CommandResetNumPedKills(int pedClassification)
{
	Assertf(CNetworkKillTracker::IsTrackingPedKills(pedClassification), "%s:RESET_NUM_PED_KILLS - Kill tracking for pedClassification %d is not active!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedClassification);
	CNetworkKillTracker::ResetNumPedKills(pedClassification);
}

void CommandAddFakePlayerName(int PedIndex, const char *name, int red, int green, int blue, int alpha)
{
    CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	Assertf(name,                         "%s:ADD_FAKE_PLAYER_NAME - Invalid name specified", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf(name && (strlen(name) < 128), "%s:ADD_FAKE_PLAYER_NAME - Name specified is too long", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((red >= 0 && red <= 255),     "%s:ADD_FAKE_PLAYER_NAME - Red colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((green >= 0 && green <= 255), "%s:ADD_FAKE_PLAYER_NAME - Green colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((blue >= 0 && blue <= 255),   "%s:ADD_FAKE_PLAYER_NAME - Blue colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Assertf((alpha >= 0 && alpha <= 255), "%s:ADD_FAKE_PLAYER_NAME - Alpha colour component invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if (ped)
    {
        CFakePlayerNames::AddFakePlayerName(ped, name, Color32(red, green, blue, alpha));
    }
}

void CommandRemoveFakePlayerName(int PedIndex)
{
    CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

    if (ped)
    {
        CFakePlayerNames::RemoveFakePlayerName(ped);
    }
}

void CommandRemoveAllFakePlayerNames()
{
    CFakePlayerNames::RemoveAllFakePlayerNames();
}

bool CommandDoesPedHaveFakePlayerName(int PedIndex)
{
    bool hasFakeName = false;

    CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

    if (ped)
    {
        hasFakeName = CFakePlayerNames::DoesPedHaveFakeName(ped);
    }

    return hasFakeName;
}

int CommandGetNumFakePlayerNames()
{
    return static_cast<int>(CFakePlayerNames::GetNumFakeNames());
}

bool  CommandNetworkCanLeavePedBehind( )
{
	CPed * player = CGameWorld::FindLocalPlayer();
	if (player)
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(player->GetNetworkObject());
		if(netObjPlayer)
		{
			return (netObjPlayer->CanRespawnLocalPlayerPed());
		}
	}

	return false;
}

void CommandRemoveAllStickyBombsFromEntity(int entityIndex, int ownerIndex)
{
	const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		CPed* pOwner = NULL;
		if (ownerIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			pOwner = CTheScripts::GetEntityToModifyFromGUID<CPed>(ownerIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		}

		CProjectileManager::RemoveAllStickyBombsAttachedToEntity(pEntity, true, pOwner);
	}
}

void  CommandNetworkLeavePedBehindBeforeWarp(int playerIndex, const scrVector & scrVecNewCoors, bool killPed, bool keepPickups)
{
	Assertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP - Network Game is not in progress", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    Vector3 vNewCoors(scrVecNewCoors);

	CPed * playerPed = CTheScripts::FindLocalPlayerPed(playerIndex);
	if(playerPed && gnetVerify(!playerPed->IsDead()))
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject());
		if (SCRIPT_VERIFY(netObjPlayer, "NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP - no network ped."))
		{
			if (netObjPlayer->CanRespawnLocalPlayerPed())
			{
				netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
				NetworkLogUtils::WritePlayerText(networkLog, NetworkInterface::GetLocalPhysicalPlayerIndex(), "TEAM_SWAP_LEAVE_PED_BEHIND_BEFORE_WARP", netObjPlayer->GetLogName());

				const bool triggerNetworkRespawnEvent = netObjPlayer->RespawnPlayerPed(vNewCoors, killPed, NETWORK_INVALID_OBJECT_ID);
				gnetAssertf(triggerNetworkRespawnEvent, "NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP - Failed");

				// move any attached pickups to the new player ped
				if (keepPickups)
				{
					CPickupManager::MovePortablePickupsFromOnePedToAnother(*playerPed, *netObjPlayer->GetPlayerPed());
				}
				else
				{
					CPickupManager::DetachAllPortablePickupsFromPed(*playerPed);
				}

				if(triggerNetworkRespawnEvent)
				{
					CRespawnPlayerPedEvent::Trigger(vNewCoors, netObjPlayer->GetRespawnNetObjId(), NetworkInterface::GetNetworkTime(), false, false);
				}

#if __DEV
				static u32 s_LastTimeRun = 0;
				u32 currTime = sysTimer::GetSystemMsTime();
				Assertf((0==s_LastTimeRun) || ((currTime-s_LastTimeRun) > LEAVE_PED_BEHIND_TIME), "%s:NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP - Too soon to be calling again this command, check your script because this command should only be called once.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				s_LastTimeRun = currTime;
#endif // __DEV
			}
		}
	}
}

void CommandNetworkLeavePedBehindBeforeCutscene(int playerIndex, bool keepPickups)
{
    CPed * playerPed = CTheScripts::FindLocalPlayerPed(playerIndex);
	if(playerPed && gnetVerify(!playerPed->IsDead()))
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject());
		if (SCRIPT_VERIFY(netObjPlayer, "NETWORK_LEAVE_PED_BEHIND_BEFORE_CUTSCENE - no network ped."))
		{
			if (netObjPlayer->CanRespawnLocalPlayerPed())
			{
				netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
				NetworkLogUtils::WritePlayerText(networkLog, NetworkInterface::GetLocalPhysicalPlayerIndex(), "TEAM_SWAP_LEAVE_PED_BEFORE_CUTSCENE", netObjPlayer->GetLogName());

				Vector3 pos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
				const bool triggerNetworkRespawnEvent = netObjPlayer->RespawnPlayerPed(pos, false, NETWORK_INVALID_OBJECT_ID);
				gnetAssertf(triggerNetworkRespawnEvent, "NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP - Failed");

				// move any attached pickups to the new player ped
				if (keepPickups)
				{
					CPickupManager::MovePortablePickupsFromOnePedToAnother(*playerPed, *netObjPlayer->GetPlayerPed());
				}
				else
				{
					CPickupManager::DetachAllPortablePickupsFromPed(*playerPed);
				}

				if (triggerNetworkRespawnEvent)
				{
					CRespawnPlayerPedEvent::Trigger(pos, netObjPlayer->GetRespawnNetObjId(), NetworkInterface::GetNetworkTime(), false, true);
				}

#if __DEV
				static u32 s_LastTimeRun = 0;
				u32 currTime = sysTimer::GetSystemMsTime();
				Assertf((0==s_LastTimeRun) || ((currTime-s_LastTimeRun) > LEAVE_PED_BEHIND_TIME), "%s:NETWORK_LEAVE_PED_BEHIND_BEFORE_CUTSCENE - Too soon to be calling again this command, check your script because this command should only be called once.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				s_LastTimeRun = currTime;
#endif // __DEV
			}
		}
	}
}

int  CommandNetworkBotGetPlayerId(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_GET_PLAYER_ID - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();
		if(netGamePlayer && netGamePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
		{
			Assert(netGamePlayer->GetPhysicalPlayerIndex() < MAX_NUM_ACTIVE_PLAYERS);
			return netGamePlayer->GetPhysicalPlayerIndex();
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return -1;
}

bool  CommandNetworkBotExists(int NETWORK_BOTS_ONLY(botPlayerIndex))
{
#if ENABLE_NETWORK_BOTS
	if (botPlayerIndex != -1)
	{
		if (scriptVerifyf(botPlayerIndex >=0 && botPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "%s:NETWORK_BOT_EXISTS - Invalid Bot Player Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botPlayerIndex))
		{
			return CGameWorld::FindPlayer(static_cast<PhysicalPlayerIndex>(botPlayerIndex)) != NULL;
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

bool CommandNetworkBotGetIsLocal(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_IS_LOCAL - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		return networkBot->GetGamerInfo().IsLocal();
	}
#endif // ENABLE_NETWORK_BOTS
	return false;
}

int  CommandNetworkBotGetNumberOfActiveBots()
{
#if ENABLE_NETWORK_BOTS
	return CNetworkBot::GetNumberOfActiveBots();
#else
    return 0;
#endif // ENABLE_NETWORK_BOTS
}

int  CommandNetworkBotGetScriptBotIndex()
{
#if ENABLE_NETWORK_BOTS
	return CNetworkBot::GetBotIndex(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
#else
    return -1;
#endif // ENABLE_NETWORK_BOTS
}

const char* CommandNetworkBotGetName(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_GET_NAME - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_GET_NAME - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			if (scriptVerifyf(networkBot->IsInMatch(), "%s:NETWORK_BOT_GET_NAME - Network Bot is NOT in match name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
			{
				return networkBot->GetGamerInfo().GetName();
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
	return "No-Name";
}

void CommandNetworkBotResurrect(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_RESURRECT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_RESURRECT - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			if (scriptVerifyf(networkBot->IsInMatch(), "%s:NETWORK_BOT_RESURRECT - Network Bot is NOT in match name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
			{
				networkBot->ProcessRespawning();
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
}

int  CommandNetworkBotAdd()
{
#if ENABLE_NETWORK_BOTS
	return CNetworkBot::AddLocalNetworkBot();
#else
    return -1;
#endif // ENABLE_NETWORK_BOTS
}

bool  CommandNetworkBotRemove(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_REMOVE - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_REMOVE - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			if (scriptVerifyf(networkBot->IsInMatch(), "%s:NETWORK_BOT_REMOVE - Network Bot is NOT in match name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
			{
				if (networkBot->GetNetPlayer() && CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent())
				{
					CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->RemovePlayerBot(*networkBot->GetNetPlayer());
				}

				networkBot->LeaveMatch();

				return true;
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

bool  CommandNetworkBotLaunchScript(int NETWORK_BOTS_ONLY(botIndex), const char* NETWORK_BOTS_ONLY(scriptName))
{
#if ENABLE_NETWORK_BOTS
	if (scriptVerifyf(scriptName, "%s:NETWORK_BOT_LAUNCH_SCRIPT - Invalid Null script Name.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
		if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_LAUNCH_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
		{
			if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_LAUNCH_SCRIPT - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
			{
				return networkBot->LaunchScript(scriptName);
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

bool  CommandNetworkBotStopScript(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_STOP_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_STOP_SCRIPT - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			return networkBot->StopLocalScript();
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

bool  CommandNetworkBotPauseScript(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_PAUSE_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_PAUSE_SCRIPT - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			return networkBot->PauseLocalScript();
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

bool  CommandNetworkBotContinue(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_CONTINUE_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_CONTINUE_SCRIPT - Network Bot is not local name=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			return networkBot->ContinueLocalScript();
		}
	}
#endif // ENABLE_NETWORK_BOTS

	return false;
}

void CommandNetworkBotJoinThisScript(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_JOIN_THIS_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_JOIN_THIS_SCRIPT - Network Bot \"%s\" is not local", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			if (scriptVerify(networkBot->GetNetPlayer()))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->AddPlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
}

void CommandNetworkBotLeaveThisScript(int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);
	if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_LEAVE_THIS_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex))
	{
		if (scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_LEAVE_THIS_SCRIPT - Network Bot \"%s\" is not local", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()))
		{
			if (scriptVerify(networkBot->GetNetPlayer()))
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->RemovePlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
}

void CommandNetworkBotJoinScript(int NETWORK_BOTS_ONLY(threadId), int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	GtaThread *pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(threadId)));

	if (scriptVerifyf(pThread && pThread->GetState() != scrThread::ABORTED, "%s:NETWORK_BOT_JOIN_SCRIPT - The script is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		scriptHandler* pHandler = pThread->m_Handler;

		if (scriptVerifyf(pHandler, "%s:NETWORK_BOT_JOIN_SCRIPT - The thread has no script handler", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent(), "%s:NETWORK_BOT_JOIN_SCRIPT - The script is not a network script", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING, "%s:NETWORK_BOT_JOIN_SCRIPT - The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);

			if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_JOIN_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex) &&
				scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_JOIN_SCRIPT - Network Bot \"%s\" is not local", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()) &&
				scriptVerify(networkBot->GetNetPlayer()))
			{
				pHandler->GetNetworkComponent()->AddPlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
}

void CommandNetworkBotLeaveScript(int NETWORK_BOTS_ONLY(threadId), int NETWORK_BOTS_ONLY(botIndex))
{
#if ENABLE_NETWORK_BOTS
	GtaThread *pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(threadId)));

	if (scriptVerifyf(pThread && pThread->GetState() != scrThread::ABORTED, "%s:NETWORK_BOT_LEAVE_SCRIPT - The script is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		scriptHandler* pHandler = pThread->m_Handler;

		if (scriptVerifyf(pHandler, "%s:NETWORK_BOT_LEAVE_SCRIPT - The script is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent(), "%s:NETWORK_BOT_LEAVE_SCRIPT - The script is not a network script", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING, "%s:NETWORK_BOT_LEAVE_SCRIPT - The script is not in a playing state", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetworkBot* networkBot = CNetworkBot::GetNetworkBot(botIndex);

			if (scriptVerifyf(networkBot, "%s:NETWORK_BOT_LEAVE_SCRIPT - Invalid Bot Index=\"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), botIndex) &&
				scriptVerifyf(networkBot->GetGamerInfo().IsLocal(), "%s:NETWORK_BOT_LEAVE_SCRIPT - Network Bot \"%s\" is not local", CTheScripts::GetCurrentScriptNameAndProgramCounter(), networkBot->GetGamerInfo().GetName()) &&
				scriptVerify(networkBot->GetNetPlayer()))
			{
				pHandler->GetNetworkComponent()->RemovePlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
#endif // ENABLE_NETWORK_BOTS
}

void CommandNetworkKeepEntityCollisionDisabledAfterAimScene(int entityIndex, bool keepCollisionDisabled)
{
	CPhysical* physical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	if(scriptVerifyf(physical, "NETWORK_KEEP_ENTITY_COLLISION_DISABLED_AFTER_ANIM_SCENE - %s - no entity", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if(scriptVerifyf(physical->GetNetworkObject(), "NETWORK_KEEP_ENTITY_COLLISION_DISABLED_AFTER_ANIM_SCENE - %s - entity has no network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
			netPhysical->SetKeepCollisionDisabledAfterAnimScene(keepCollisionDisabled);

#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "NETWORK_KEEP_ENTITY_COLLISION_DISABLED_AFTER_ANIM_SCENE", "%s", netPhysical->GetLogName());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Keep Collision Disabled", "%s", keepCollisionDisabled ? "TRUE" : "FALSE");
#endif // ENABLE_NETWORK_LOGGING		
		}
	}
}

#if !__FINAL

int CommandNetworkSoakTestGetNumParameters()
{
    return CNetworkSoakTests::GetNumParameters();
}

int CommandNetworkSoakTestGetIntParameters(int index)
{
    if(SCRIPT_VERIFY(index >= 0 && index < CNetworkSoakTests::GetNumParameters(), "Invalid soak test parameter requested (use -netsoaktestparams=<param list>)"))
    {
        return CNetworkSoakTests::GetParameter(index);
    }

    return 0;
}

#endif // !__FINAL

bool  CommandNetworkIsAnyPlayerNear(int& retPlayerIds, int& retNumber, const scrVector &pos_, float radius, bool in3d)
{
	retPlayerIds = 0;
	retNumber = 0;
	scrVector pos = pos_;

	if (SCRIPT_VERIFY_NETWORK_REGISTERED("NETWORK_IS_ANY_PLAYER_NEAR") && SCRIPT_VERIFY_NETWORK_ACTIVE("NETWORK_IS_ANY_PLAYER_NEAR"))
	{
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			CPed* playerPed = player->GetPlayerPed();

			if (playerPed)
			{
				Vector3 gamerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());

				if (!in3d)
				{
					gamerPos.z = 0.0f;
					pos.z = 0.0f;
				}

				Vector3 vecDiff = Vector3 (pos) - gamerPos;
				const float dist = vecDiff.Mag();

				if (dist < radius)
				{
					retNumber++;
					retPlayerIds |= (1<<player->GetPhysicalPlayerIndex());
				}
			}
		}
	}

	return (0 < retNumber);
}

int  CommandNetworkGetFocusPedLocalId()
{
#if __DEV

	CEntity* focusEntity = CDebugScene::FocusEntities_Get(0);

	if (focusEntity && focusEntity->GetIsTypePed())
	{
		CNetObjGame* netObjGame= static_cast<CPed*>(focusEntity)->GetNetworkObject();

		if (netObjGame && !netObjGame->IsClone())
		{
			CPed* ped = static_cast<CPed*>(focusEntity);

			if (!CTheScripts::HasEntityBeenRegisteredWithCurrentThread(ped) && CommandCanRegisterMissionPeds(1))
			{
				CTheScripts::RegisterEntity(ped);
			}

			return CTheScripts::GetGUIDFromEntity(*ped);
		}
	}

#endif // __DEV

	return NULL_IN_SCRIPTING_LANGUAGE;
}

// Clans

bool CommandNetworkClanServiceIsValid()
{
    scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_SERVICE_IS_VALID - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    return CLiveManager::GetNetworkClan().ServiceIsValid();
}

bool CommandNetworkClanPlayerIsActive(int& handleData)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_PLAYER_IS_ACTIVE - Local Player is offline.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (NetworkInterface::IsLocalPlayerOnline())
	{
		NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
		scriptAssertf(clanMgr.ServiceIsValid(), "%s:NETWORK_CLAN_PLAYER_IS_ACTIVE - Clan service is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (clanMgr.ServiceIsValid())
		{
			rlGamerHandle hGamer;
			if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_PLAYER_IS_ACTIVE")))
			{
				result = clanMgr.HasPrimaryClan(hGamer);
			}
		}
	}

	return result;
}

void Util_rlClanDescToscrClanDesc(const rlClanDesc& inClanDesc, scrClanDesc* out_pScrClanDesc)
{
	out_pScrClanDesc->m_clanId.Int = (int)inClanDesc.m_Id;
	formatf(out_pScrClanDesc->m_ClanName, "%s", inClanDesc.m_ClanName);
	formatf(out_pScrClanDesc->m_ClanTag, "%s", inClanDesc.m_ClanTag);
	out_pScrClanDesc->m_memberCount.Int = inClanDesc.m_MemberCount;
	out_pScrClanDesc->m_CreatedTimePosix.Int = inClanDesc.m_CreatedTimePosix;
	out_pScrClanDesc->m_bSystemClan.Int = inClanDesc.m_IsSystemClan ? 1 : 0;
	out_pScrClanDesc->m_bOpenClan.Int   = inClanDesc.m_IsOpenClan ? 1 : 0;
	
	Color32 clanColor(inClanDesc.m_clanColor);
	out_pScrClanDesc->m_ClanColor_Red.Int = clanColor.GetRed();
	out_pScrClanDesc->m_ClanColor_Green.Int = clanColor.GetGreen();
	out_pScrClanDesc->m_ClanColor_Blue.Int = clanColor.GetBlue();
}

bool Util_rlClanMembershipDataToscrClanDesc(const rlClanMembershipData& inClanDesc, scrClanDesc* out_pScrClanDesc)
{
	if (inClanDesc.IsValid())
	{
		Util_rlClanDescToscrClanDesc(inClanDesc.m_Clan, out_pScrClanDesc);

		formatf(out_pScrClanDesc->m_RankName, "%s", inClanDesc.m_Rank.m_RankName);
		out_pScrClanDesc->m_rankOrder.Int = inClanDesc.m_Rank.m_RankOrder;

		return true;
	}

	return false;
}

void Util_scrClanDescTorlClanDesc(const scrClanDesc* in_pScrClanDesc, rlClanDesc& out_ClanDesc )
{
	out_ClanDesc.m_Id = in_pScrClanDesc->m_clanId.Int;
	formatf(out_ClanDesc.m_ClanName, "%s", in_pScrClanDesc->m_ClanName);
	formatf(out_ClanDesc.m_ClanTag, "%s", in_pScrClanDesc->m_ClanTag);
	out_ClanDesc.m_MemberCount = in_pScrClanDesc->m_memberCount.Int;
	out_ClanDesc.m_IsSystemClan = in_pScrClanDesc->m_bSystemClan.Int == 1;
	out_ClanDesc.m_IsOpenClan =	in_pScrClanDesc->m_bOpenClan.Int == 1;
	out_ClanDesc.m_CreatedTimePosix = in_pScrClanDesc->m_CreatedTimePosix.Int;

	Color32 clanColor(	in_pScrClanDesc->m_ClanColor_Red.Int, 
						in_pScrClanDesc->m_ClanColor_Green.Int, 
						in_pScrClanDesc->m_ClanColor_Blue.Int	);

	out_ClanDesc.m_clanColor = clanColor.GetColor();
}

bool CommandNetworkClanPlayerGetDesc(scrClanDesc* out_pClanDesc, int, int& handleData)
{
	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	// make sure that handle is valid
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_PLAYER_GET_DESC")))
		return false;

	CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	if (pNetPlayer)
	{
		const rlClanMembershipData& rClanMemInfo = pNetPlayer->GetClanMembershipInfo();
		if(rClanMemInfo.IsValid())
		{
			if (Util_rlClanMembershipDataToscrClanDesc(rClanMemInfo, out_pClanDesc))
			{
				return true;
			}
		}
	}
	else // Check if it's the local player
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
		if(pGamerInfo && pGamerInfo->GetGamerHandle() == hGamer)
		{
			const rlClanMembershipData& rClanMemInfo = rlClan::GetPrimaryMembership(localGamerIndex);
			if(rClanMemInfo.IsValid())
			{
				if (Util_rlClanMembershipDataToscrClanDesc(rClanMemInfo, out_pClanDesc))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CommandNetworkClanGetPlayerClanEmblemTXDName(int& handleData, scrTextLabel63* outTXDName)
{
	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	// make sure that handle is valid
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "GetPlayerCrewEmblemTXDName")))
		return false;

	CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	if (pNetPlayer)
	{
		return pNetPlayer->GetClanEmblemTXDName(*outTXDName);
	}
	else if(hGamer.IsLocal())
	{
		const char* emblemName = CLiveManager::GetNetworkClan().GetClanEmblemTXDNameForLocalGamerClan();
		if (emblemName && strlen(emblemName) > 0)
		{
			safecpy(*outTXDName, emblemName);
			return true;
		}
		else
		{
			gnetDebug1("%s:NETWORK_CLAN_GET_EMBLEM_TXD_NAME - emblemName is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (!pNetPlayer)
	{
		gnetDebug1("%s:NETWORK_CLAN_GET_EMBLEM_TXD_NAME - CNetGamePlayer is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	return false;
}

static const int MAX_REQUESTED_EMBLEMS = 2;

static int s_RequestedEmblems[MAX_REQUESTED_EMBLEMS];
static int s_numRequestedEmblems = 0;

bool CommandNetworkClanRequestEmblem(int clanId)
{
	if (s_numRequestedEmblems >= MAX_REQUESTED_EMBLEMS)
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_REQUEST_EMBLEM - Max number of requests (%d) exceeded. Have you forgotten to call NETWORK_CLAN_RELEASE_EMBLEM?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_numRequestedEmblems);
		return false;
	}
	   
	if (CLiveManager::GetNetworkClan().IsEmblemForClanReady(clanId))
	{
		gnetDebug1("%s:NETWORK_CLAN_REQUEST_EMBLEM(%d) - Is Ready", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
		return true;
	}
	if (CLiveManager::GetNetworkClan().IsEmblemForClanPending(clanId))
	{
		gnetDebug1("%s:NETWORK_CLAN_REQUEST_EMBLEM(%d) - Is Pending", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
		return true;
	}

	if (CLiveManager::GetNetworkClan().RequestEmblemForClan((rlClanId)clanId  ASSERT_ONLY(, "CommandNetwork")))
	{
		if (AssertVerify(s_numRequestedEmblems >= 0 && s_numRequestedEmblems < MAX_REQUESTED_EMBLEMS))
		{
			s_RequestedEmblems[s_numRequestedEmblems++] = clanId;
		}
		gnetDebug1("%s:NETWORK_CLAN_REQUEST_EMBLEM(%d) - actually requesting", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
		return true;
	}

	return false;
}

bool CommandNetworkClanIsEmblemReady(int clanId, scrTextLabel63* outTXDName)
{
	int i = 0;

	for (i=0; i<s_numRequestedEmblems; i++)
	{
		if (s_RequestedEmblems[i] == clanId)
		{
			break;
		}
	}
	
	if (i==s_numRequestedEmblems)
	{
		gnetDebug1("%s:NETWORK_CLAN_IS_EMBLEM_READY(%d) not requested by scripts", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
	}

	const char* emblemName;
	bool bReady = CLiveManager::GetNetworkClan().IsEmblemForClanReady((rlClanId)clanId, emblemName);

	if (bReady)
	{
		safecpy(*outTXDName, emblemName);
	}

	gnetDebug1("%s:NETWORK_CLAN_IS_EMBLEM_READY(%d) = %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId, bReady ? "true" : "false");
	return bReady;
}

void CommandNetworkClanReleaseEmblem(int clanId)
{
	int slot = -1;
	int i = 0;

	for (i=0; i<s_numRequestedEmblems; i++)
	{
		if (s_RequestedEmblems[i] == clanId)
		{
			slot = i;
			break;
		}
	}

	if (i==s_numRequestedEmblems)
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_RELEASE_EMBLEM - Clan emblem with id %d has not been requested", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
		return;
	}

	CLiveManager::GetNetworkClan().ReleaseEmblemReference(clanId    ASSERT_ONLY(, "CommandNetwork"));

	if (AssertVerify(s_numRequestedEmblems > 0) && AssertVerify(slot >= 0))
	{
		for (int i=slot; i<s_numRequestedEmblems-1; i++)
		{
			s_RequestedEmblems[i] = s_RequestedEmblems[i+1];
		}

		s_numRequestedEmblems--;
	}

	gnetDebug1("%s:NETWORK_CLAN_RELEASE_EMBLEM() for clanID=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clanId);
}

bool CommandNetworkCrewGetCrewRankTitleForPlayer(int playerindex, scrTextLabel31* outCrewRankTitle)
{
	CNetGamePlayer *pPlayer = CTheScripts::FindNetworkPlayer(playerindex);
	
	(*outCrewRankTitle)[0] = '\0';
	if (pPlayer)
	{
		safecpy((*outCrewRankTitle), pPlayer->GetCrewRankTitle());
	}
	
	return strlen((*outCrewRankTitle)) > 0;
}

void CommandNewtorkCrewSetLocalCrewRankTitleForSync(const char* crewRankTitle)
{
	if(!scriptVerifyf(strlen(crewRankTitle) < RL_CLAN_NAME_MAX_CHARS, "%s is long than max lenght of %d", crewRankTitle, RL_CLAN_NAME_MAX_CHARS))
	{
		return;
	}

	CNetGamePlayer* player = NetworkInterface::GetLocalPlayer();
	if (player && player->GetClanDesc().IsValid())
	{
		player->SetCrewRankTitle(crewRankTitle);
	}
}

bool CommandNetworkClanIsRockstarClan(scrClanDesc* in_pClanDesc, int UNUSED_PARAM(sizeCheckFromScript))
{
	// turns out the only distinguishable characteristic of a Rockstar clan
	// is that they have 'Rockstar' in their name.
	// compare rlClanDesc::IsRockstarClan()
	return rage::stristr(in_pClanDesc->m_ClanName, "Rockstar") != NULL;
}

void CommandNetworkClanGetUiFormattedTag(scrClanDesc* in_pClanDesc, int UNUSED_PARAM(sizeCheckFromScript), scrTextLabel15* outTag )
{
	rlClanDesc tempDesc;
	Util_scrClanDescTorlClanDesc(in_pClanDesc, tempDesc);
	NetworkClan::GetUIFormattedClanTag(tempDesc, -1, *outTag, 16);
}

bool CommandNetworkClanIsMembershipSynched(int& handleData)
{
	rlGamerHandle hGamer;

	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_IS_MEMBERSHIP_SYNCHED")))
	{
		return false;
	}

	CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	if (!pNetPlayer)
	{
		return false;
	}

	// If GetCrewId() == RL_INVALID_CLAN_ID then the GetClanMembershipInfo will never be set so we're synched.
	// The crew id in pNetPlayer is set the first time in CNetworkPlayerMgr::SetCustomPlayerData so that's early enough.
	return pNetPlayer->GetCrewId() == RL_INVALID_CLAN_ID || pNetPlayer->GetClanMembershipInfo().IsValid();
}

int CommandNetworkClanGetLocalMembershipsCount()
{
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_GET_LOCAL_MEMBERSHIPS_COUNT - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(NetworkInterface::HasValidRockstarId(), "%s:NETWORK_CLAN_GET_LOCAL_MEMBERSHIPS_COUNT - Player is not a Social Club Member.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_GET_LOCAL_MEMBERSHIPS_COUNT");

	return CLiveManager::GetNetworkClan().GetLocalGamerMembershipCount();
}

bool CommandNetworkClanGetMembershipDesc(scrClanDesc* out_pClanDesc, int membershipIndex)
{
	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
	scriptAssertf(pGamerInfo && pGamerInfo->GetGamerHandle().IsValid(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_DESC - Invalid local Gamer handle.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if(pGamerInfo && pGamerInfo->GetGamerHandle().IsValid())
	{
		const rlClanMembershipData* membership = CLiveManager::GetNetworkClan().GetMembership(pGamerInfo->GetGamerHandle(), membershipIndex);
		scriptAssertf(membership && membership->IsValid(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_DESC - Invalid membership index %d, count is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), membershipIndex, CLiveManager::GetNetworkClan().GetMembershipCount(pGamerInfo->GetGamerHandle()));

		if (membership && membership->IsValid())
		{
			if (Util_rlClanMembershipDataToscrClanDesc(*membership, out_pClanDesc))
			{
				return true;
			}
		}
	}

	return false;
}

bool CommandNetworkClanDownloadMembershipDesc(int& handleData)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_DOWNLOAD_MEMBERSHIP")))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_DOWNLOAD_MEMBERSHIP");

	result = CLiveManager::GetNetworkClan().RefreshMemberships(hGamer, 0);

#if !__NO_OUTPUT
	char szToString[RL_MAX_GAMER_HANDLE_CHARS];
	gnetDebug1("NETWORK_CLAN_DOWNLOAD_MEMBERSHIP - Start Result %s - For Gamer %s", result?"True":"False", hGamer.ToString(szToString));
#endif // !__NO_OUTPUT

	return result;
}

bool CommandNetworkClanDownloadMembershipDescPending(int& handleData)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING")))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	result = CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer);

	return result;
}

bool CommandNetworkClanAnyDownloadMembershipDescPending()
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_ANY_DOWNLOAD_MEMBERSHIP_PENDING - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_ANY_DOWNLOAD_MEMBERSHIP_PENDING - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	result = CLiveManager::GetNetworkClan().RefreshMembershipsPendingAnyPlayer();

	return result;
}

bool CommandNetworkClanRemoteMembersipsAlreadyDownloaded(int& handleData)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE")))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	result = CLiveManager::GetNetworkClan().HasMembershipsReadyOrRequested(hGamer) && !CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer);

	return result;
}

bool CommandNetworkClanGetMembershipValid(int& handleData, int membershipIndex)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_VALID - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_VALID - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_GET_MEMBERSHIP_VALID")))
		return result;

	scriptAssertf(!CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer), "%s:NETWORK_CLAN_GET_MEMBERSHIP_VALID - Operation Pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_GET_MEMBERSHIP_VALID - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_VALID - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	// TU MAGIC: If index == -1 in a remote, we return the active clan instead, cf NetworkClan::GetMembership
	if(membershipIndex==-1)
	{
		return CLiveManager::GetNetworkClan().IsActiveMembershipValid();
	}

	const rlClanMembershipData* membership = CLiveManager::GetNetworkClan().GetMembership(hGamer, membershipIndex);
	return (membership && membership->IsValid());
}

bool CommandNetworkClanGetMembership(int& handleData, scrClanDesc* out_pClanDesc, int membershipIndex)
{
	bool result = false;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_GET_MEMBERSHIP - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_GET_MEMBERSHIP - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_GET_MEMBERSHIP")))
		return result;

	scriptAssertf(!CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer), "%s:NETWORK_CLAN_GET_MEMBERSHIP - Operation Pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_GET_MEMBERSHIP - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_GET_MEMBERSHIP - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	const rlClanMembershipData* membership = CLiveManager::GetNetworkClan().GetMembership(hGamer, membershipIndex);
	scriptAssertf(membership && membership->IsValid(), "%s:NETWORK_CLAN_GET_MEMBERSHIP - Invalid membership index %d, count is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), membershipIndex, CLiveManager::GetNetworkClan().GetMembershipCount(hGamer));

	if (membership && membership->IsValid())
	{
		if (Util_rlClanMembershipDataToscrClanDesc(*membership, out_pClanDesc))
		{
			scriptDebugf1("%s:NETWORK_CLAN_GET_MEMBERSHIP - membership index %d, count is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), membershipIndex, CLiveManager::GetNetworkClan().GetMembershipCount(hGamer));
			return true;
		}
	}

	return false;
}

int CommandNetworkClanGetMembershipCount(int& handleData)
{
	int result = 0;

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	scriptAssertf(NetworkInterface::IsCloudAvailable(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - Cloud not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsCloudAvailable())
		return result;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_CLAN_GET_MEMBERSHIP_COUNT")))
		return result;

	scriptAssertf(!hGamer.IsLocal(), "%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - Gamer is local player.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (hGamer.IsLocal())
		return result;

	scriptAssertf(!CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer), "%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - Operation Pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (CLiveManager::GetNetworkClan().RefreshMembershipsPending(hGamer))
		return result;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
	scriptAssertf(!pPlayer, "%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - Don't use this command for players in the session - use instead NETWORK_CLAN_PLAYER_GET_DESC.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (pPlayer)
		return result;

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_GET_MEMBERSHIP_COUNT");

	result = CLiveManager::GetNetworkClan().GetMembershipCount(hGamer);

	scriptDebugf1("%s:NETWORK_CLAN_GET_MEMBERSHIP_COUNT - count is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CLiveManager::GetNetworkClan().GetMembershipCount(hGamer));

	return result;
}


int CommandNetworkClanGetReceivedInvitesCount()
{
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_GET_RECEIVED_INVITES_COUNT - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_GET_RECEIVED_INVITES_COUNT");

	return CLiveManager::GetNetworkClan().GetInvitesReceivedCount();
}


bool CommandNetworkClanGetClanInviteData(int inviteIndex, scrCrewRecvdInviteData* outData )
{
	NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
	u32 rcvCount = clanMgr.GetInvitesReceivedCount();
	if(!scriptVerifyf(rcvCount > 0 && inviteIndex < rcvCount, "No invites or invalid index [%d:%d]", inviteIndex, rcvCount))
	{
		return false;
	}
	const rlClanInvite* pInviteData = CLiveManager::GetNetworkClan().GetReceivedInvite(inviteIndex);
	if(!scriptVerifyf(pInviteData, "No invite found for index '%d'", inviteIndex))
	{
		return false;
	}

	Util_rlClanDescToscrClanDesc(pInviteData->m_Clan, &outData->clanDesc);
	outData->gamerTag.String = scrEncodeString(pInviteData->m_Inviter.m_Gamertag);
	outData->msgString.String = scrEncodeString(pInviteData->m_Message);

	return true;

}

bool CommandNetworkClanInvitePlayer(const int playerIndex)
{
	scriptAssertf(CLiveManager::GetNetworkClan().ServiceIsValid(), "%s:NETWORK_CLAN_INVITE_PLAYER - Clan service is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(CLiveManager::GetNetworkClan().HasPrimaryClan(), "%s:NETWORK_CLAN_INVITE_PLAYER - No active clan membership.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_INVITE_PLAYER - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	bool result = false;

	if (SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_CLAN_INVITE_PLAYER", playerIndex))
	{
		result = CLiveManager::GetNetworkClan().InvitePlayer(playerIndex);
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_INVITE_PLAYER");

	return result;
}

bool CommandNetworkClanInviteFriend(const int friendIndex)
{
	scriptAssertf(CLiveManager::GetNetworkClan().ServiceIsValid(), "%s:NETWORK_CLAN_INVITE_FRIEND - Clan service is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(CLiveManager::GetNetworkClan().HasPrimaryClan(), "%s:NETWORK_CLAN_INVITE_FRIEND - No active clan membership.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s:NETWORK_CLAN_INVITE_FRIEND - Player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	bool result = false;

	if (scriptVerifyf(CLiveManager::GetFriendsPage()->m_NumFriends > friendIndex, "%s:NETWORK_CLAN_INVITE_FRIEND - No friend at that index", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		result = CLiveManager::GetNetworkClan().InviteFriend(friendIndex);
	}

	WriteToScriptLogFile(SCRIPTLOG_ALWAYS, "NETWORK_CLAN_INVITE_FRIEND");

	return result;
}

bool CommandNetworkClanjoin(const int clanId)
{
	return CLiveManager::GetNetworkClan().AttemptJoinClan(clanId);
}


void  CommandNetworkResetPlayerDamageTicker()
{
	scriptAssertf(0, "%s:NETWORK_RESET_PLAYER_DAMAGE_TICKER - Deprecated command - Remove", CTheScripts::GetCurrentScriptNameAndProgramCounter());
}

int CommandNetworkGetNetworkIDFromRopeID(int scriptRopeID)
{
    int networkRopeID = -1;

    char error[128];
    sprintf(error, "Invalid rope ID supplied! (ID: %d)", scriptRopeID);
    if(SCRIPT_VERIFY(scriptRopeID >= 0, error))
    {
        networkRopeID = CNetworkRopeWorldStateData::GetNetworkIDFromRopeID(scriptRopeID);

        scriptAssertf(networkRopeID != -1, "Failed to find network ID of rope! (ID: %d)", networkRopeID);
    }
    
    return networkRopeID;
}

int CommandNetworkGetRopeIDFromNetworkID(int networkRopeID)
{
    int scriptRopeID = -1;
    
    char error[128];
    sprintf(error, "Invalid rope ID supplied! (ID: %d)", networkRopeID);
    if(SCRIPT_VERIFY(networkRopeID >= 0, error))
    {
        scriptRopeID = CNetworkRopeWorldStateData::GetRopeIDFromNetworkID(networkRopeID);

        scriptAssertf(scriptRopeID != -1, "Failed to find ID of rope from network ID! (ID: %d)", networkRopeID);
    }

    return scriptRopeID;
}

int CommandNetworkCreateSynchronisedScene(const scrVector & position, const scrVector & orientation, int RotOrder, bool HoldLastFrame, bool Looped, float phaseToStopScene, float phaseToStartScene, float startRate)
{
    Vector3 pos(position);
	Vector3 eulers(orientation);
	eulers*=DtoR;
	Quaternion rot;
	CScriptEulers::QuaternionFromEulers(rot, eulers, static_cast<EulerAngleOrder>(RotOrder));

	if (!scriptVerifyf(NetworkInterface::GetLocalPlayer(), "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Cannot create a synchronised scene when there is no local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return -1;
	}

    if(HoldLastFrame && Looped)
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Hold last frame and looped specified - these options are incompatible. Looping is being removed!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        Looped = false;
    }

    if(phaseToStartScene < 0.0f || phaseToStartScene > 1.0f)
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Phase to start scene is not in the range 0.0f - 1.0f. Setting start phase back to 0.0f!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        phaseToStartScene = 0.0f;
    }

    if(phaseToStopScene < 1.0f && (HoldLastFrame && Looped))
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Phase to stop scene is not 1.0f and Hold last frame or looped specified - these options are incompatible. Setting stop phase back to 1.0f!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        phaseToStopScene = 1.0f;
    }

    if(phaseToStopScene > 1.0f)
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Phase to stop scene is greater than 1.0f - this doesn't make sense. Setting stop phase back to 1.0f!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        phaseToStopScene = 1.0f;
    }

    if(startRate < 0.0f)
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Rate to start scene is < 0.0f - this doesn't make sense. Setting rate back to 1.0f!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        startRate = 1.0f;
    }

    if(startRate > 2.0f)
    {
        scriptAssertf(0, "%s:NETWORK_CREATE_SYNCHRONISED_SCENE - Rate to start scene is > 2.0f - this is the maximum value supported. Setting rate back to 1.0f!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        startRate = 1.0f;
    }

    int sceneID = CNetworkSynchronisedScenes::CreateScene(pos, rot, HoldLastFrame, Looped, phaseToStartScene, phaseToStopScene, startRate, CTheScripts::GetCurrentScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadPC());

#if __BANK
	if(sceneID == -1)
	{
		atString syncedSceneInfo; fwAnimDirectorComponentSyncedScene::DumpSynchronizedSceneDebugInfo(syncedSceneInfo);
		atArray< atString > splitString; syncedSceneInfo.Split(splitString, "\n", true);
		for(int i = 0; i < splitString.GetCount(); i ++)
		{
			scriptDisplayf("%s", splitString[i].c_str());
		}

        gnetDebug1("NETWORK_CREATE_SYNCHRONISED_SCENE - Failed to create synchronised scene (too many already active?)");

		scriptAssertf(sceneID != -1, "NETWORK_CREATE_SYNCHRONISED_SCENE - Failed to create synchronised scene (too many already active?");
	}
#endif // __BANK
#if !__FINAL
    scriptDebugf1("%s: Creating network synchronised scene ID %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sceneID);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

    return sceneID;
}

void CommandNetworkAddPedToSynchronisedScene(int PedIndex, int sceneId, const char *pAnimDictName, const char *pAnimName, float blendIn, float blendOut, int flags, int ragdollBlockingFlags, float fMoverBlendInDelta, int ikFlags)
{

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

    if(SCRIPT_VERIFY(pPed, "NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE: Failed to find ped to add!"))
	{
		scriptAssertf(blendIn>0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE: blend in delta is (%f) should be > 0.0f. Changing to INSTANT_BLEND_IN_DELTA.", blendIn);
		if (blendIn<=0.0f)
			blendIn = INSTANT_BLEND_IN_DELTA;

		scriptAssertf(blendOut<0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE: blend out delta (%f) should be < 0.0. Changing to INSTANT_BLEND_OUT_DELTA.", blendOut);
		if (blendOut>=0.0f)
			blendOut = INSTANT_BLEND_OUT_DELTA;

		scriptAssertf(fMoverBlendInDelta>0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE: mover blend delta (%f) should be > 0.0f. Changing to INSTANT_BLEND_IN_DELTA!", fMoverBlendInDelta);
		if (fMoverBlendInDelta<=0.0f)
			fMoverBlendInDelta = INSTANT_BLEND_IN_DELTA;

        CNetworkSynchronisedScenes::AddPedToScene(pPed, sceneId, pAnimDictName, pAnimName, blendIn, blendOut, flags, ragdollBlockingFlags, fMoverBlendInDelta, ikFlags);
    }
}

void CommandNetworkAddPedToSynchronisedSceneWithIK(int PedIndex, int sceneId, const char *pAnimDictName, const char *pAnimName, float blendIn, float blendOut, int flags, int ragdollBlockingFlags, float fMoverBlendInDelta, int ikFlags)
{
    CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if(SCRIPT_VERIFY(pPed, "NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE_WITH_IK: Failed to find ped to add!"))
    {
		scriptAssertf(blendIn>0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE_WITH_IK: blend in delta is (%f) should be > 0.0f. Changing to INSTANT_BLEND_IN_DELTA.", blendIn);
		if (blendIn<=0.0f)
			blendIn = INSTANT_BLEND_IN_DELTA;

		scriptAssertf(blendOut<0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE_WITH_IK: blend out delta (%f) should be < 0.0. Changing to INSTANT_BLEND_OUT_DELTA.", blendOut);
		if (blendOut>=0.0f)
			blendOut = INSTANT_BLEND_OUT_DELTA;

		scriptAssertf(fMoverBlendInDelta>0.0f ,"NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE_WITH_IK: mover blend delta (%f) should be > 0.0f. Changing to INSTANT_BLEND_IN_DELTA!", fMoverBlendInDelta);
		if (fMoverBlendInDelta<=0.0f)
			fMoverBlendInDelta = INSTANT_BLEND_IN_DELTA;

        CNetworkSynchronisedScenes::AddPedToScene(pPed, sceneId, pAnimDictName, pAnimName, blendIn, blendOut, flags, ragdollBlockingFlags, fMoverBlendInDelta, ikFlags);
    }
}

void CommandNetworkAddEntityToSynchronisedScene(int PedIndex, int sceneId, const char *pAnimDictName, const char *pAnimName, float blendIn, float blendOut, int flags)
{
	CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if(SCRIPT_VERIFY(pPhysical, "NETWORK_ADD_ENTITY_TO_SYNCHRONISED_SCENE: Failed to find entity to add!"))
	{
        if(SCRIPT_VERIFY(!pPhysical->GetIsTypePed(), "NETWORK_ADD_ENTITY_TO_SYNCHRONISED_SCENE: This command cannot be used for peds! Use NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE instead!"))
	    {

			scriptAssertf(blendIn>0.0f ,"NETWORK_ADD_ENTITY_TO_SYNCHRONISED_SCENE: blend in delta is (%f) should be > 0.0f. Changing to INSTANT_BLEND_IN_DELTA.", blendIn);
			if (blendIn<=0.0f)
				blendIn = INSTANT_BLEND_IN_DELTA;

			scriptAssertf(blendOut<0.0f ,"NETWORK_ADD_ENTITY_TO_SYNCHRONISED_SCENE: blend out delta (%f) should be < 0.0. Changing to INSTANT_BLEND_OUT_DELTA.", blendOut);
			if (blendOut>=0.0f)
				blendOut = INSTANT_BLEND_OUT_DELTA;

            CNetworkSynchronisedScenes::AddEntityToScene(pPhysical, sceneId, pAnimDictName, pAnimName, blendIn, blendOut, flags);
        }
    }
}

void CommandsNetworkAddMapEntityToSynchronisedScene(int sceneId, int EntityModelHash, Vector3 EntityPosition, const char *pAnimDictName, const char *pAnimName, float blendIn, float blendOut, int flags)
{
	CNetworkSynchronisedScenes::AddMapEntityToScene(sceneId, (u32)EntityModelHash, EntityPosition, pAnimDictName, pAnimName, blendIn, blendOut, flags);
}

void CommandNetworkAddSynchronisedSceneCamera(int sceneID, const char *pAnimDictName, const char *pAnimName)
{
    char error[128];

#if __FINAL
    sprintf(error, "Invalid scene ID supplied! (ID: %d)", sceneID);
#else
	sprintf(error, "NETWORK_STOP_SYNCHRONISED_SCENE: Invalid scene ID supplied! (ID: %d)", sceneID);
#endif //__FINAL

    if(SCRIPT_VERIFY(sceneID >= 0, error))
    {
        CNetworkSynchronisedScenes::AddCameraToScene(sceneID, pAnimDictName, pAnimName);
    }
}

void CommandNetworkAttachSynchronisedSceneToEntity(int sceneID, int entityIndex, int entityBoneIndex)
{
    char error[128];
#if __FINAL
	sprintf(error, "Invalid scene ID supplied! (ID: %d)", sceneID);
#else
	sprintf(error, "NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY: Invalid scene ID supplied! (ID: %d)", sceneID);
#endif //__FINAL

    if(SCRIPT_VERIFY(sceneID >= 0, error))
    {
        if(SCRIPT_VERIFY(CNetworkSynchronisedScenes::IsSceneActive(sceneID), "NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY: The specified scene is not active!"))
        {
		    const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(entityIndex);
		
		    scriptAssertf(pEntity, "%s NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY. Couldn't find valid entity at entityIndex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), entityIndex);

		    if(pEntity)
		    {
                if(SCRIPT_VERIFY(pEntity->GetNetworkObject(), "NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY: The entity specified is not networked!"))
                {
#if __DEV
			        if(entityBoneIndex>-1)
			        {
				        scriptAssertf(pEntity->GetSkeleton(), "%s NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY. Entity %s does not have a skeleton", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject()->GetLogName());
				        if (pEntity->GetSkeleton())
				        {
					        scriptAssertf(entityBoneIndex < pEntity->GetSkeleton()->GetBoneCount(), "%s ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY. Invalid entityBoneIndex %d entity %s max index is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), entityBoneIndex, pEntity->GetNetworkObject()->GetLogName(), pEntity->GetSkeleton()->GetBoneCount());
				        }
			        }
#endif //__DEV

                    CNetworkSynchronisedScenes::AttachSyncedSceneToEntity(sceneID, pEntity, entityBoneIndex);
                }
            }
		}
	}
}

void CommandNetworkStartSynchronisedScene(int sceneID)
{
    char error[128];

#if __FINAL
	sprintf(error, "Invalid scene ID supplied! (ID: %d)", sceneID);
#else
	sprintf(error, "NETWORK_START_SYNCHRONISED_SCENE: Invalid scene ID supplied! (ID: %d)", sceneID);
#endif // __FINAL

    if(SCRIPT_VERIFY(sceneID >= 0, error))
    {
        if(SCRIPT_VERIFY(CNetworkSynchronisedScenes::IsSceneActive(sceneID), "NETWORK_START_SYNCHRONISED_SCENE: The specified scene is not active!"))
        {
#if !__FINAL
            scriptDebugf1("%s: Starting network synchronised scene ID %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sceneID);
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            CNetworkSynchronisedScenes::StartSceneRunning(sceneID);
        }
    }
}

void CommandNetworkStopSynchronisedScene(int sceneID)
{
    char error[128];
#if __FINAL
	sprintf(error, "Invalid scene ID supplied! (ID: %d)", sceneID);
#else
	sprintf(error, "NETWORK_STOP_SYNCHRONISED_SCENE: Invalid scene ID supplied! (ID: %d)", sceneID);
#endif //__FINAL

    if(SCRIPT_VERIFY(sceneID >= 0, error))
    {
#if !__FINAL
        scriptDebugf1("%s: Stopping network synchronised scene ID %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sceneID);
        scrThread::PrePrintStackTrace();
#endif // !__FINAL

        CNetworkSynchronisedScenes::StopSceneRunning(sceneID LOGGING_ONLY(, "CommandNetworkStopSynchronisedScene"));
    }
}

int CommandNetworkGetLocalSceneIDFromNetworkID(int sceneID)
{
    if(CNetworkSynchronisedScenes::IsSceneActive(sceneID))
    {
        return CNetworkSynchronisedScenes::GetLocalSceneID(sceneID);
    }

    return -1;
}

void CommandNetworkForceLocalUseOfSyncedSceneCamera(int sceneID)
{
    char error[128];
#if __FINAL
	sprintf(error, "Invalid scene ID supplied! (ID: %d)", sceneID);
#else
	sprintf(error, "NETWORK_FORCE_LOCAL_USE_OF_SYNCED_SCENE_CAMERA: Invalid scene ID supplied! (ID: %d)", sceneID);
#endif //__FINAL

    if(SCRIPT_VERIFY(sceneID >= 0, error))
    {
        CNetworkSynchronisedScenes::ForceLocalUseOfCamera(sceneID);
    }
}

void CommandNetworkAllowRemoteSyncSceneLocalPlayerRequests(bool allowRemoteRequests)
{
    if(allowRemoteRequests != CNetworkSynchronisedScenes::AreRemoteSyncSceneLocalPlayerRequestsAllowed())
    {
#if !__FINAL
        scriptDebugf1("%s:NETWORK_ALLOW_REMOTE_SYNCED_SCENE_LOCAL_PLAYER_REQUESTS - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), allowRemoteRequests ? "TRUE" : "FALSE");
        scrThread::PrePrintStackTrace();
#endif // !__FINAL

        CNetworkSynchronisedScenes::SetAllowRemoteSyncSceneLocalPlayerRequests(allowRemoteRequests);
    }
}

void CommandNetworkOverrideClockTime(int hour, int minute, int second)
{
	scriptDebugf1("%s:NETWORK_OVERRIDE_CLOCK_TIME - Current: %02d:%02d:%02d, To: %02d:%02d:%02d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond(), hour, minute, second);
	CNetwork::OverrideClockTime(hour, minute, second, true);

#if !__FINAL
	if (!scriptVerifyf(!CNetwork::IsClockOverrideLocked(), "NETWORK_CLOCK_OVERRIDE_LOCK Violation"))
	{
		scrThread::PrePrintStackTrace();
	}
#endif // !__FINAL
}

void CommandNetworkOverrideClockRate(int msPerGameMinute)
{
	scriptDebugf1("%s:NETWORK_OVERRIDE_CLOCK_RATE - Current: %d, To: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CClock::GetMsPerGameMinute(), msPerGameMinute);
	if (scriptVerifyf(msPerGameMinute > 0, "NETWORK_OVERRIDE_CLOCK_RATE, Per game minuite rate must be greater than zero"))
	{
		CNetwork::OverrideClockRate((u32)msPerGameMinute);
#if !__FINAL
		if (!scriptVerifyf(!CNetwork::IsClockOverrideLocked(), "NETWORK_CLOCK_OVERRIDE_LOCK Violation"))
		{
			scrThread::PrePrintStackTrace();
		}
#endif // !__FINAL
	}
}

void CommandNetworkClearClockTimeOverride()
{
    if(CNetwork::IsClockTimeOverridden() && CNetwork::IsClockOverrideFromScript())
    {
		bool bUseTransition = true;
#if !__FINAL
		bUseTransition &= !CClock::HasDebugModifiedInNetworkGame() || PARAM_netClockAlwaysBlend.Get();
#endif

        scriptDebugf1("%s:NETWORK_CLEAR_CLOCK_TIME_OVERRIDE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        if(bUseTransition)
        {
            scriptDebugf1("NETWORK_CLEAR_CLOCK_TIME_OVERRIDE :: Clearing to global!");
            CNetwork::StartClockTransitionToGlobal(0);
        }
        else
        {
            scriptDebugf1("NETWORK_CLEAR_CLOCK_TIME_OVERRIDE :: Clearing to debug time!");
            CNetwork::ClearClockTimeOverride();
        }

#if !__FINAL
		if (!scriptVerifyf(!CNetwork::IsClockOverrideLocked(), "NETWORK_CLOCK_OVERRIDE_LOCK Violation"))
		{
			scrThread::PrePrintStackTrace();
		}
#endif // !__FINAL
    }
}

void CommandNetworkSyncClockTimeOverride()
{
#if !__FINAL
    if(CNetwork::IsClockTimeOverridden())
    {
        scriptDebugf1("%s:NETWORK_SYNC_CLOCK_TIME_OVERRIDE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_OVERRIDE);
    }
#endif
}

bool CommandNetworkIsClockTimeOverridden()
{
    return NetworkInterface::IsClockTimeOverridden();
}

void CommandNetworkSetClockOverrideLocked(bool bLocked)
{
#if !__FINAL
	if (bLocked != CNetwork::IsClockOverrideLocked())
	{
		scriptDebugf1("%s:NETWORK_SET_CLOCK_OVERRIDE_LOCKED :: Changing from %s to %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetwork::IsClockOverrideLocked() ? "locked" : "unlocked", bLocked ? "locked" : "unlocked");
		scrThread::PrePrintStackTrace();
	}
#endif // !__FINAL
	CNetwork::SetClockOverrideLocked(bLocked);
}

bool CommandNetworkGetGlobalClock(int &nHour, int &nMinute, int &nSecond)
{
	if(!scriptVerifyf(NetworkInterface::IsClockTimeOverridden(), "NETWORK_GET_GLOBAL_CLOCK - Clock not overridden!"))
		return false;

	int nLastMinAdded = 0;
	CNetwork::GetClockTimeStoredForOverride(nHour, nMinute, nSecond, nLastMinAdded);

	return true;
}

int CommandNetworkAddEntityArea(const scrVector & vAreaStart, const scrVector & vAreaEnd)
{
    if(!scriptVerifyf(CTheScripts::GetScriptHandlerMgr().IsNetworkHost(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), "NETWORK_ADD_ENTITY_AREA - Trying to create a host entity area on a client machine - use NETWORK_ADD_CLIENT_ENTITY_AREA instead!"))
        return -1;

	int nAreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), 
																  Vector3(vAreaStart), 
																  Vector3(vAreaEnd),
																  -1.0f,
																  CNetworkEntityAreaWorldStateData::eInclude_Default,
                                                                  true);

#if !__FINAL
    scriptDebugf1("%s: Creating network entity area ID %d: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nAreaID, vAreaStart.x, vAreaStart.y, vAreaStart.z, vAreaEnd.x, vAreaEnd.y, vAreaEnd.z);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

	scriptAssertf(nAreaID >= 0, "%s:Invalid Area ID returned! Box Limits Incorrect!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return nAreaID; 
}

int CommandNetworkAddEntityAngledArea(const scrVector & vAreaStart, const scrVector & vAreaEnd, float AreaWidth)
{
    if(!scriptVerifyf(CTheScripts::GetScriptHandlerMgr().IsNetworkHost(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), "NETWORK_ADD_ENTITY_ANGLED_AREA - Trying to create a host entity area on a client machine - use NETWORK_ADD_CLIENT_ENTITY_ANGLED_AREA instead!"))
        return -1;

	int nAreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), 
		Vector3(vAreaStart), 
		Vector3(vAreaEnd),
		AreaWidth,
		CNetworkEntityAreaWorldStateData::eInclude_Default,
        true);

#if !__FINAL
    scriptDebugf1("%s: Creating network entity angled area ID %d: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) width:%.2f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nAreaID, vAreaStart.x, vAreaStart.y, vAreaStart.z, vAreaEnd.x, vAreaEnd.y, vAreaEnd.z, AreaWidth);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

	scriptAssertf(nAreaID >= 0, "%s:Invalid Area ID returned! Box Limits Incorrect!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return nAreaID; 
}

int CommandNetworkAddClientEntityArea(const scrVector & vAreaStart, const scrVector & vAreaEnd)
{
    int nAreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), 
        Vector3(vAreaStart), 
        Vector3(vAreaEnd),
        -1.0f,
        CNetworkEntityAreaWorldStateData::eInclude_Default,
        false);

#if !__FINAL
    scriptDebugf1("%s: Creating client network entity area ID %d: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nAreaID, vAreaStart.x, vAreaStart.y, vAreaStart.z, vAreaEnd.x, vAreaEnd.y, vAreaEnd.z);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

    scriptAssertf(nAreaID >= 0, "%s:Invalid Area ID returned! Box Limits Incorrect!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    return nAreaID; 
}

int CommandNetworkAddClientEntityAngledArea(const scrVector & vAreaStart, const scrVector & vAreaEnd, float AreaWidth)
{
    int nAreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), 
        Vector3(vAreaStart), 
        Vector3(vAreaEnd),
        AreaWidth,
        CNetworkEntityAreaWorldStateData::eInclude_Default,
        false);

#if !__FINAL
    scriptDebugf1("%s: Creating client network entity angled area ID %d: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) width:%.2f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nAreaID, vAreaStart.x, vAreaStart.y, vAreaStart.z, vAreaEnd.x, vAreaEnd.y, vAreaEnd.z, AreaWidth);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

    scriptAssertf(nAreaID >= 0, "%s:Invalid Area ID returned! Box Limits Incorrect!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    return nAreaID; 
}

bool CommandNetworkRemoveEntityArea(int scriptAreaID)
{
#if !__FINAL
    scriptDebugf1("%s: Removing network entity area ID %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptAreaID);
    scrThread::PrePrintStackTrace();
#endif // !__FINAL

	return CNetworkEntityAreaWorldStateData::RemoveEntityArea(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), scriptAreaID);
}

bool CommandNetworkEntityAreaDoesExist(int scriptAreaID)
{
	return CNetworkEntityAreaWorldStateData::HasWorldState(scriptAreaID);
}

bool CommandNetworkEntityAreaHaveAllReplied(int scriptAreaID)
{
	return CNetworkEntityAreaWorldStateData::HaveAllReplied(scriptAreaID);
}

bool CommandNetworkEntityAreaHasPlayerReplied(int scriptAreaID, int playerID)
{
	scriptAssertf(CTheScripts::FindNetworkPlayer(playerID), "%s:Invalid player index of %d!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerID);
	return CNetworkEntityAreaWorldStateData::HasReplyFrom(scriptAreaID, static_cast<PhysicalPlayerIndex>(playerID));
}

bool CommandNetworkIsEntityAreaOccupied(int scriptAreaID)
{
	return CNetworkEntityAreaWorldStateData::IsOccupied(scriptAreaID);
}

bool CommandNetworkIsEntityAreaOccupiedOn(int scriptAreaID, int playerID)
{
	scriptAssertf(CTheScripts::FindNetworkPlayer(playerID), "%s:Invalid player index of %d!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerID);
	return CNetworkEntityAreaWorldStateData::IsOccupiedOn(scriptAreaID, static_cast<PhysicalPlayerIndex>(playerID));
}

void CommandNetworkUseHighPrecisionBlending(int networkID, bool useHighPrecision)
{
    if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_USE_HIGH_PRECISION_BLENDING - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_USE_HIGH_PRECISION_BLENDING - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

		if (scriptVerifyf(physical, "%s:NETWORK_USE_HIGH_PRECISION_BLENDING - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
            if(scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_USE_HIGH_PRECISION_BLENDING - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                if (scriptVerifyf(physical->GetIsTypeObject(), "%s:NETWORK_USE_HIGH_PRECISION_BLENDING - Only supported for objects currently!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                {
                    CNetObjObject *netObjObject = SafeCast(CNetObjObject, physical->GetNetworkObject());

                    netObjObject->SetUseHighPrecisionBlending(useHighPrecision);
                }
            }
        }
    }
}

void CommandNetworkSetCustomArenaBallParams(int networkID)
{
    if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
        scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

        if (scriptVerifyf(physical, "%s:NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if (scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                if (scriptVerifyf(physical->GetIsTypeObject(), "%s:NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS - Only supported for objects currently!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                {
                    CNetObjObject *netObjObject = SafeCast(CNetObjObject, physical->GetNetworkObject());
                    netObjObject->SetIsArenaBall(true);
                }
            }
        }
    }
}

void CommandNetworkEntityUseHighPrecisionRotation(int networkID, bool useHighPrecision)
{
    if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
        scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

        if (scriptVerifyf(physical, "%s:NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if (scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                if (scriptVerifyf(physical->GetIsTypeObject(), "%s:NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION - Only supported for objects currently!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                {
                    CNetObjObject *netObjObject = SafeCast(CNetObjObject, physical->GetNetworkObject());
                    netObjObject->SetUseHighPrecisionRotation(useHighPrecision);
                }
            }
        }
    }
}

bool CommandNetworkRequestCloudBackgroundScript()
{
    return SBackgroundScripts::GetInstance().RequestCloudFile();
}

int CommandNetworkGetCloudBackgroundScriptModifiedTime()
{
    return static_cast<int>(SBackgroundScripts::GetInstance().GetCloudModifiedTime());
}

bool CommandNetworkIsBackgroundScriptCloudRequestPending()
{
	return SBackgroundScripts::GetInstance().IsCloudRequestPending();
}

void CommandNetworkRequestCloudTunables()
{
    Tunables::GetInstance().StartCloudRequest();
}

bool CommandNetworkIsTunablesCloudRequestPending()
{
	return Tunables::GetInstance().IsCloudRequestPending();
}

int CommandNetworkGetTunableCloudCRC()
{
    if(!Tunables::IsInstantiated())
        return 0;

    return static_cast<int>(Tunables::GetInstance().GetCloudCRC());
}

bool CommandNetworkDoesTunableExist_Deprecated(const char* szContext, const char* szTunableName)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_DOES_TUNABLE_EXIST - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().CheckExists(atStringHash(szContext), atStringHash(szTunableName));
}

bool CommandNetworkAccessTunableInt_Deprecated(const char* szContext, const char* szTunableName, int& nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_ACCESS_TUNABLE_INT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().Access(atStringHash(szContext), atStringHash(szTunableName), nTunable);
}

bool CommandNetworkAccessTunableFloat_Deprecated(const char* szContext, const char* szTunableName, float& fTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_ACCESS_TUNABLE_FLOAT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().Access(atStringHash(szContext), atStringHash(szTunableName), fTunable);
}

bool CommandNetworkAccessTunableBool_Deprecated(const char* szContext, const char* szTunableName)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_ACCESS_TUNABLE_BOOL - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	// bool& not supported by script (see wrapper.h - line 180) so this needs to be implemented
	// differently for bools. A pre-check using CommandNetworkDoesTunableExist would work
	bool bTunable = false;
	Tunables::GetInstance().Access(atStringHash(szContext), atStringHash(szTunableName), bTunable);
	return bTunable; 
}

int CommandNetworkTryAccessTunableInt_Deprecated(const char* szContext, const char* szTunableName, int nDefault)
{
    if(!Tunables::IsInstantiated())
        return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_TRY_ACCESS_TUNABLE_INT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

    return Tunables::GetInstance().TryAccess(atStringHash(szContext), atStringHash(szTunableName), nDefault);
}

float CommandNetworkTryAccessTunableFloat_Deprecated(const char* szContext, const char* szTunableName, float fDefault)
{
    if(!Tunables::IsInstantiated())
        return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_TRY_ACCESS_TUNABLE_FLOAT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

    return Tunables::GetInstance().TryAccess(atStringHash(szContext), atStringHash(szTunableName), fDefault);
}

bool CommandNetworkTryAccessTunableBool_Deprecated(const char* szContext, const char* szTunableName, bool bDefault)
{
    if(!Tunables::IsInstantiated())
        return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_TRY_ACCESS_TUNABLE_BOOL - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

    return Tunables::GetInstance().TryAccess(atStringHash(szContext), atStringHash(szTunableName), bDefault);
}

bool CommandNetworkInsertTunableInt_Deprecated(const char* szContext, const char* szTunableName, int nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_INSERT_TUNABLE_INT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().Insert(TunableHash(atStringHash(szContext), atStringHash(szTunableName)), nTunable, 0, 0, true);
}

bool CommandNetworkInsertTunableFloat_Deprecated(const char* szContext, const char* szTunableName, float fTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_INSERT_TUNABLE_FLOAT - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().Insert(TunableHash(atStringHash(szContext), atStringHash(szTunableName)), fTunable, 0, 0, true);
}

bool CommandNetworkInsertTunableBool_Deprecated(const char* szContext, const char* szTunableName, bool bTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	// validate tunable strings
#if !__NO_OUTPUT
	if(!gnetVerify(!(szContext == NULL || szContext[0] == '\0' || szTunableName == NULL || szTunableName[0] == '\0')))
	{
		gnetError("%s:NETWORK_INSERT_TUNABLE_BOOL - Tunable %s::%s is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContext ? szContext : "INVALID", szTunableName ? szTunableName : 0);
		return false;
	}
#endif

	return Tunables::GetInstance().Insert(TunableHash(atStringHash(szContext), atStringHash(szTunableName)), bTunable, 0, 0, true);
}

bool CommandNetworkDoesTunableExist(int nContextHash, int nTunableHash)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().CheckExists(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash));
}

bool CommandNetworkAccessTunableInt(int nContextHash, int nTunableHash, int& nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;
	return Tunables::GetInstance().Access(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), nTunable);
}

bool CommandNetworkAccessTunableModificationDetectionClear()
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().ModificationDetectionClear();
}

bool CommandNetworkAccessTunableIntModificationDetectionRegistration(int nContextHash, int nTunableHash, int& nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().ModificationDetectionRegistration(nContextHash, nTunableHash, nTunable);
}

bool CommandNetworkAccessTunableFloat(int nContextHash, int nTunableHash, float& fTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().Access(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), fTunable);
}

bool CommandNetworkAccessTunableFloatModificationDetectionRegistration(int nContextHash, int nTunableHash, float& nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().ModificationDetectionRegistration(nContextHash, nTunableHash, nTunable);
}

bool CommandNetworkAccessTunableBool(int nContextHash, int nTunableHash)
{
	if(!Tunables::IsInstantiated())
		return false;

	// bool& not supported by script (see wrapper.h - line 180) so this needs to be implemented
	// differently for bools. A pre-check using CommandNetworkDoesTunableExist would work
	bool bTunable = false;
	Tunables::GetInstance().Access(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), bTunable);
	return bTunable; 
}

bool CommandNetworkAccessTunableBoolModificationDetectionRegistration(int nContextHash, int nTunableHash, int& nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;
	return Tunables::GetInstance().ModificationDetectionRegistration(nContextHash, nTunableHash, nTunable);
}

int CommandNetworkTryAccessTunableInt(int nContextHash, int nTunableHash, int nDefault)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().TryAccess(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), nDefault);
}

float CommandNetworkTryAccessTunableFloat(int nContextHash, int nTunableHash, float fDefault)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().TryAccess(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), fDefault);
}

bool CommandNetworkTryAccessTunableBool(int nContextHash, int nTunableHash, bool bDefault)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().TryAccess(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash), bDefault);
}

bool CommandNetworkInsertTunableInt(int nContextHash, int nTunableHash, int nTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().Insert(TunableHash(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash)), nTunable, 0, 0, true);
}

bool CommandNetworkInsertTunableFloat(int nContextHash, int nTunableHash, float fTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().Insert(TunableHash(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash)), fTunable, 0, 0, true);
}

bool CommandNetworkInsertTunableBool(int nContextHash, int nTunableHash, bool bTunable)
{
	if(!Tunables::IsInstantiated())
		return false;

	return Tunables::GetInstance().Insert(TunableHash(static_cast<unsigned>(nContextHash), static_cast<unsigned>(nTunableHash)), bTunable, 0, 0, true);
}

int CommandNetworkGetContentModifierListID(const int nContentHash)
{
    return Tunables::GetInstance().GetContentListID(nContentHash);
}

int CommandNetworkGetPlayerNetworkObjectID(int playerIndex)
{
    if(SCRIPT_VERIFY_PLAYER_INDEX("NETWORK_GET_PLAYER_NETWORK_OBJECT_ID", playerIndex))
	{
		CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));

		if (player && player->GetPlayerPed() && player->GetPlayerPed()->GetNetworkObject())
		{
			return player->GetPlayerPed()->GetNetworkObject()->GetObjectID();
		}
	}

	return NETWORK_INVALID_OBJECT_ID;
}

bool  CommandNetworkHasBoneBeenHit(int component)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_HAS_BONE_BEEN_HIT - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS, "%s:NETWORK_HAS_BONE_BEEN_HIT - Invalid component %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component);

	if (NetworkInterface::IsGameInProgress() && component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS)
	{
		return CNetworkBodyDamageTracker::GetComponentHit(component);
	}

	return false;
}

int  CommandNetworkGetBoneIdOfFatalHit()
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_GET_BONE_ID_OF_FATAL_HIT - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (NetworkInterface::IsGameInProgress())
	{
		return CNetworkBodyDamageTracker::GetFatalComponent();
	}

	return -1;
}

bool  CommandNetworkHasBoneBeenHitByKiller(int component)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_HAS_BONE_BEEN_HIT_BY_KILLER - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS, "%s:NETWORK_HAS_BONE_BEEN_HIT_BY_KILLER - Invalid component %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component);

	if (NetworkInterface::IsGameInProgress() && component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS)
	{
		CNetworkBodyDamageTracker::ShotInfo info;
		CNetworkBodyDamageTracker::GetFatalShotHistory(info);

		return info.GetComponentHit(component);
	}

	return false;
}

void CommandNetworkResetBodyTracker()
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_RESET_BODY_TRACKER - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (NetworkInterface::IsGameInProgress())
	{
		CNetworkBodyDamageTracker::Reset();
	}
}

int CommandNetworkGetNumberBodyTrackerHits()
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_GET_NUMBER_BODY_TRACKER_HITS - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (NetworkInterface::IsGameInProgress())
	{
		return CNetworkBodyDamageTracker::GetNumComponents();
	}

	return 0;
}

bool  CommandNetworkSetAttributeDamageToPlayer(int PedIndex, int PlayerIndex) 
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	CPed* ped = CTheScripts::GetEntityToModifyFromGUID< CPed >(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_ALL);

	scriptAssertf(ped, "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Invalid ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PedIndex);
	if (!ped)
		return false;

	scriptAssertf(!ped->IsPlayer(), "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Ped index %d is a player.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PedIndex);
	if (ped->IsPlayer())
		return false;

	scriptAssertf(!ped->IsNetworkClone(), "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Ped index %d is a network clone.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PedIndex);
	if (ped->IsNetworkClone())
		return false;

	scriptAssertf(ped->GetNetworkObject(), "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Ped index %d does not have a network object.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PedIndex);
	if (!ped->GetNetworkObject())
		return false;

	CNetGamePlayer* netGamePlayer = CTheScripts::FindNetworkPlayer(PlayerIndex);
	scriptAssertf(netGamePlayer, "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Player index %d Not Found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PlayerIndex);
	if (!netGamePlayer)
		return false;

	scriptAssertf(netGamePlayer->IsLocal(), "%s:NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER - Player index %d is not local.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PlayerIndex);
	if (!netGamePlayer->IsLocal())
		return false;

	CNetObjPed* pedNetObj = SafeCast(CNetObjPed, ped->GetNetworkObject());
	if(pedNetObj)
	{
		pedNetObj->SetAttributeDamageTo(netGamePlayer->GetPhysicalPlayerIndex());
		return true;
	}

	return false;
}

bool  CommandNetworkGetAttributeDamageToPlayer(int PedIndex, int& PlayerIndex) 
{
	PlayerIndex = INVALID_PLAYER_INDEX;

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_GET_ATTRIBUTE_DAMAGE_TO_PLAYER - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	const CPed* ped = CTheScripts::GetEntityToQueryFromGUID< CPed >(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_ALL);
	if (!ped)
		return false;

	scriptAssertf(ped->GetNetworkObject(), "%s:NETWORK_GET_ATTRIBUTE_DAMAGE_TO_PLAYER - Ped index %d does not have a network object.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PedIndex);
	if (!ped->GetNetworkObject())
		return false;

	CNetObjPed* pedNetObj = SafeCast(CNetObjPed, ped->GetNetworkObject());
	if(pedNetObj && pedNetObj->GetAttributeDamageTo() != INVALID_PLAYER_INDEX)
	{
		PlayerIndex = (int)pedNetObj->GetAttributeDamageTo();
		return true;
	}

	return false;
}

void CommandsNetworkTriggerDamageEventForZeroDamage(int EntityIndex, bool shouldTrigger)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_DAMAGE - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return;

	CPhysical *physical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if (SCRIPT_VERIFY(physical, "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_DAMAGE - The entity does not exist") 
	&&  SCRIPT_VERIFY(physical->GetNetworkObject(), "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_DAMAGE - The entity has no network object"))
	{
		CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

	#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_DAMAGE", "%s", netPhysical->GetLogName());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Should Trigger", "%s", shouldTrigger ? "TRUE" : "FALSE");
	#endif // ENABLE_NETWORK_LOGGING

		netPhysical->SetTriggerDamageEventForZeroDamage(shouldTrigger);
	}
}

void CommandsNetworkTriggerDamageEventForZeroWeaponHash(int EntityIndex, bool shouldTrigger)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_WEAPON_HASH - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return;

	CPhysical *physical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if (SCRIPT_VERIFY(physical, "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_WEAPON_HASH - The entity does not exist")
		&& SCRIPT_VERIFY(physical->GetNetworkObject(), "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_WEAPON_HASH - The entity has no network object"))
	{
		CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_WEAPON_HASH", "%s", netPhysical->GetLogName());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Should Trigger", "%s", shouldTrigger ? "TRUE" : "FALSE");
#endif // ENABLE_NETWORK_LOGGING

		netPhysical->SetTriggerDamageEventForZeroWeaponHash(shouldTrigger);
	}
}


#if __XENON && __DEV
#define XENON_DEV_ONLY(...)   __VA_ARGS__
#else
#define XENON_DEV_ONLY(...)
#endif

const char* CommandNetworkGetXUIDFromGamertag(const char* XENON_DEV_ONLY(szGamertag))
{
#if __XENON
#if __DEV
	return NetworkDebug::ConvertGamertagToXUID(szGamertag);
#else
	scriptAssertf(0, "NETWORK_GET_XUID_FROM_GAMERTAG not available in non-dev builds!");
	return "INVALID_CONFIG"; 
#endif
#else
	return "INVALID_PLATFORM"; 
#endif
}

void CommandNetworkSetNoLongerNeeded(int ASSERT_ONLY(entityId), bool ASSERT_ONLY(taggedByScript))
{
#if __ASSERT
	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		if (scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_SET_NO_LONGER_NEEDED - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* pPhysical = SafeCast(CPhysical, const_cast<CEntity*>(pEntity));
			netObject* pNetObj = pPhysical ? pPhysical->GetNetworkObject() : NULL;

			if (scriptVerifyf(pNetObj, "%s:NETWORK_SET_NO_LONGER_NEEDED - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				static_cast<CNetObjPhysical*>(pNetObj)->SetAssertIfRemoved(taggedByScript);
			}
		}
	}
#endif // __ASSERT
}

bool CommandNetworkGetNoLongerNeeded(int ASSERT_ONLY(entityId))
{
#if __ASSERT
	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pEntity)
	{
		if (scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_GET_NO_LONGER_NEEDED - entity is not physical", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPhysical* pPhysical = SafeCast(CPhysical, const_cast<CEntity*>(pEntity));
			netObject* pNetObj = pPhysical ? pPhysical->GetNetworkObject() : NULL;

			if (scriptVerifyf(pNetObj, "%s:NETWORK_GET_NO_LONGER_NEEDED - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return static_cast<CNetObjPhysical*>(pNetObj)->GetAssertIfRemoved();
			}
		}
	}
#endif // __ASSERT

	return false;
}

void CommandNetworkExplodeVehicle(int VehicleIndex, bool bAddExplosion, bool bKeepDamageEntity, int inflictorPlayerId)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		CEntity *pInflictor = NULL;

		if(bKeepDamageEntity)
		{
			scriptDebugf1("%s:NETWORK_EXPLODE_VEHICLE - Command called with bKeepDamageEntity TRUE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			pInflictor = pVehicle->GetWeaponDamageEntity();
		}
		else if(inflictorPlayerId != -1)
		{
			netPlayer *player = CTheScripts::FindNetworkPlayer(inflictorPlayerId);

			scriptDebugf1("%s:NETWORK_EXPLODE_VEHICLE - Command called with a inflictorId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if (player)
			{
				pInflictor = SafeCast(CNetGamePlayer, player)->GetPlayerPed();
			}
		}
		else
		{
			scriptDebugf1("%s:NETWORK_EXPLODE_VEHICLE - Command called - ResetWeaponDamageInfo on '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetLogName());

			//This change should be for SP and MP, but making it a bit safer 
			// this late in the project and only clear on MP.
			if (NetworkInterface::IsNetworkOpen())
			{
				pVehicle->ResetWeaponDamageInfo();
			}
		}
		pVehicle->m_nPhysicalFlags.bExplodeInstantlyWhenChecked =  true;
		pVehicle->BlowUpCar(pInflictor, false, bAddExplosion, false);
	}
}

void CommandNetworkExplodeHeli(int VehicleIndex, bool bAddExplosion, bool bKeepDamageEntity, int inflictorId)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle &&
		pVehicle->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast< CHeli* >( pVehicle );

		CEntity* pInflictor = NULL;

		if( bKeepDamageEntity )
		{
			pInflictor = pVehicle->GetWeaponDamageEntity();
		}
		else if(NETWORK_INVALID_OBJECT_ID != inflictorId)
		{
			CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(inflictorId));

			if (scriptVerifyf(pPhysical, "%s:NETWORK_EXPLODE_HELI - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pInflictor = pPhysical;
			}
		}

		pHeli->SetRearRotorHealth( 0.0f );
		pVehicle->BlowUpCar( pInflictor, false, bAddExplosion, false );
	}
}

void CommandNetworkUseLogarithmicBlendingThisFrame(int entityId)
{
    CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if(scriptVerifyf(pPhysical, "%s:NETWORK_USE_LOGARITHMIC_BLENDING_THIS_FRAME - an entity with the specified ID does not exist!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        if(scriptVerifyf(pPhysical->GetNetworkObject(), "%s:NETWORK_USE_LOGARITHMIC_BLENDING_THIS_FRAME - the entity specified is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if(scriptVerifyf(pPhysical->IsNetworkClone(), "%s:NETWORK_USE_LOGARITHMIC_BLENDING_THIS_FRAME - This command is only allowed to be called on remote entities!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                netBlenderLinInterp *netBlender = SafeCast(netBlenderLinInterp, pPhysical->GetNetworkObject()->GetNetBlender());

                if(netBlender)
                {
                    netBlender->UseLogarithmicBlendingThisFrame();
                }
            }
        }
    }
}

void CommandNetworkOverrideCoordsAndHeading(int entityId, const scrVector & scrVecNewCoors, float NewHeading )
{
	Vector3 vNewCoors(scrVecNewCoors);

	CPhysical* pTarget = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pTarget)
	{
		netObject *networkObject = pTarget->GetNetworkObject();

		if (scriptVerifyf(networkObject, "%s:NETWORK_OVERRIDE_COORDS_AND_HEADING - the entity does not have a network object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (pTarget->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pTarget);

				float fHeadingInRadians = ( DtoR * NewHeading);

				fHeadingInRadians = fwAngle::LimitRadianAngleSafe(fHeadingInRadians);

				if (!networkObject->IsPendingOwnerChange() && scriptVerifyf(networkObject->IsClone(), "%s:NETWORK_OVERRIDE_COORDS_AND_HEADING - the entity is expected to be a clone", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if(SCRIPT_VERIFY(!pPed->GetUsingRagdoll(), "NETWORK_OVERRIDE_COORDS_AND_HEADING - Ped is ragdolling"))
					{
						const unsigned TIME_TO_DELAY_BLENDING = 500;
						NetworkInterface::OverrideNetworkBlenderForTime(pPed, TIME_TO_DELAY_BLENDING);

						if (pPed->GetIsInVehicle())
						{
							pPed->GetPedIntelligence()->FlushImmediately(false);
							pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());

							if(!pPed->GetMovePed().GetTaskNetwork())
							{
								pPed->GetMovePed().SetTaskNetwork(NULL);
							}
	
							pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true);
							pPed->InstantAIUpdate(true);
							pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, false);

							pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
						}
						
						pPed->SetDesiredHeading( fHeadingInRadians );
						pPed->SetHeading(fHeadingInRadians);
						CScriptPeds::SetPedCoordinates(pPed, vNewCoors.x, vNewCoors.y, vNewCoors.z, CScriptPeds::SetPedCoordFlag_AllowClones);	
					}
				}
			}
		}
	}
}

static const unsigned COMMAND_NETWORK_MAX_OVERRIDE_BLENDER_TIME = 2000; 

void CommandNetworkOverrideBlenderForTime(int networkID, int timeToOverrideBlender )
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));
		
		if(timeToOverrideBlender > COMMAND_NETWORK_MAX_OVERRIDE_BLENDER_TIME)
		{
			scriptAssertf(0,  "%s: timeToOverrideBlender %d exceeds and has been clamped to COMMAND_NETWORK_MAX_OVERRIDE_BLENDER_TIME %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), timeToOverrideBlender, COMMAND_NETWORK_MAX_OVERRIDE_BLENDER_TIME);
			timeToOverrideBlender = COMMAND_NETWORK_MAX_OVERRIDE_BLENDER_TIME;
		}

		if (scriptVerifyf(physical, "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerifyf(timeToOverrideBlender>=0, "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME timeToOverrideBlender %d should be positive ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), timeToOverrideBlender))
			{
				if(scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (scriptVerifyf(physical->GetIsTypeObject(), "%s:NETWORK_OVERRIDE_BLENDER_FOR_TIME - Only supported for objects currently!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						NetworkInterface::OverrideNetworkBlenderForTime(physical, timeToOverrideBlender);						
#if !__FINAL 
						scriptDebugf1("%s Setting Collision timer for %s for %d ms", CTheScripts::GetCurrentScriptNameAndProgramCounter(), physical->GetNetworkObject()->GetLogName(), timeToOverrideBlender);
						scrThread::PrePrintStackTrace();
#endif // !__FINAL						
					}
				}
			}
		}
	}
}

void CommandNetworkEnableExtraVehicleOrientationBlendChecks(int networkID, bool bEnable)
{
    if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
        scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

        if (scriptVerifyf(physical, "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if (scriptVerifyf(physical->GetIsTypeVehicle(), "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - This command is only supported for vehicles!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                if(scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                {
                    if(scriptVerifyf(physical->IsNetworkClone(), "%s:NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS - This command can only be called on clone vehicles!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                    {
                        CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, physical->GetNetworkObject()->GetNetBlender());

                        if(gnetVerifyf(netBlenderVehicle, "Vehicle doesn't have a network blender!"))
                        {
                            if(netBlenderVehicle->IsDoingOrientationBlendChecks() != bEnable)
                            {
#if !__FINAL 
                                scriptDebugf1("%s: NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS %s extra orientation checking for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bEnable ? "enabling" : "disabling", physical->GetNetworkObject()->GetLogName());
                                scrThread::PrePrintStackTrace(); 
#endif // !__FINAL

                                netBlenderVehicle->SetDoExtraOrientationBlendChecks(bEnable);
                            }
                        }
                    }
                }
            }
        }
    }
}

void CommandNetworkDisableProximityMigration(int networkId)
{
	CPhysical* pPhysical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkId));

	if (scriptVerifyf(pPhysical, "%s:NETWORK_DISABLE_PROXIMITY_MIGRATION - no entity with this id", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(pPhysical->GetNetworkObject(), "%s:NETWORK_DISABLE_PROXIMITY_MIGRATION - the entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		static_cast<CNetObjPhysical*>(pPhysical->GetNetworkObject())->SetLocalFlag(CNetObjGame::LOCALFLAG_DISABLE_PROXIMITY_MIGRATION, true);
	}
}

void CommandNetworkSetVehicleGarageIndex(int VehicleIndex, int garageIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(scriptVerifyf(pVehicle, "%s:NETWORK_SET_VEHICLE_GARAGE_INDEX - Vehicle does not exist!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
        if(scriptVerifyf(pVehicle->GetNetworkObject(), "%s:NETWORK_SET_VEHICLE_GARAGE_INDEX - Vehicle is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if(scriptVerifyf(garageIndex != INVALID_GARAGE_INDEX, "%s:NETWORK_SET_VEHICLE_GARAGE_INDEX - Garage index is invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
                netObjVehicle->SetGarageInstanceIndex(static_cast<u8>(garageIndex));
            }
        }
    }
}

void CommandNetworkClearVehicleGarageIndex(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(scriptVerifyf(pVehicle, "%s:NETWORK_CLEAR_VEHICLE_GARAGE_INDEX - Vehicle does not exist!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
        if(scriptVerifyf(pVehicle->GetNetworkObject(), "%s:NETWORK_CLEAR_VEHICLE_GARAGE_INDEX - Vehicle is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
            netObjVehicle->SetGarageInstanceIndex(INVALID_GARAGE_INDEX);
        }
    }
}

void CommandNetworkSetPlayerGarageIndex(int garageIndex)
{
    CPed *playerPed = FindPlayerPed();

    if(playerPed && playerPed->GetNetworkObject())
    {
        if(scriptVerifyf(garageIndex != INVALID_GARAGE_INDEX, "%s:NETWORK_SET_PLAYER_GARAGE_INDEX - Garage index is invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            CNetGamePlayer *netGamePlayer = SafeCast(CNetGamePlayer, playerPed->GetNetworkObject()->GetPlayerOwner());

            if(scriptVerifyf(netGamePlayer, "%s:NETWORK_SET_PLAYER_GARAGE_INDEX - Player is invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                netGamePlayer->SetGarageInstanceIndex(static_cast<u8>(garageIndex));
            }
        }
    }
}

void CommandNetworkClearPlayerGarageIndex()
{
    CPed *playerPed = FindPlayerPed();

    if(playerPed && playerPed->GetNetworkObject())
    {
        CNetGamePlayer *netGamePlayer = SafeCast(CNetGamePlayer, playerPed->GetNetworkObject()->GetPlayerOwner());

        if(scriptVerifyf(netGamePlayer, "%s:NETWORK_CLEAR_PLAYER_GARAGE_INDEX - Player is invalid!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            netGamePlayer->SetGarageInstanceIndex(INVALID_GARAGE_INDEX);
        }
    }
}

void CommandNetworkSetPropertyID(int nPropertyID)
{
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pLocalPlayer, "%s:NETWORK_SET_PROPERTY_ID - No local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// verify property ID is not invalid 
	if(!scriptVerifyf(static_cast<u8>(nPropertyID) != NO_PROPERTY_ID, "%s:NETWORK_SET_PROPERTY_ID - Invalid property ID!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// verify property ID is within limit
	if(!scriptVerifyf(static_cast<u8>(nPropertyID) <= MAX_PROPERTY_ID, "%s:NETWORK_SET_PROPERTY_ID - Property ID too high. Given: %d, Maximum: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nPropertyID, MAX_PROPERTY_ID))
		return;

	// apply setting
	pLocalPlayer->SetPropertyID(static_cast<u8>(nPropertyID));
}

void CommandNetworkClearPropertyID()
{
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pLocalPlayer, "%s:NETWORK_CLEAR_PROPERTY_ID - No local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// apply setting
	pLocalPlayer->SetPropertyID(NO_PROPERTY_ID);
}

void CommandNetworkSetPlayerMentalState(int nMentalState)
{
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pLocalPlayer, "%s:NETWORK_SET_PLAYER_MENTAL_STATE- No local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	// verify state is within limit
	if(!scriptVerifyf(static_cast<u8>(nMentalState) <= MAX_MENTAL_STATE, "%s:NETWORK_SET_PLAYER_MENTAL_STATE - Mental state too high. Given: %d, Maximum: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMentalState, MAX_MENTAL_STATE))
		return;

	pLocalPlayer->SetMentalState((u8)nMentalState);
}

void CommandNetworkSetMinimumRankForMission(const int nMinimumRank)
{
	// verify state is within limit
	if(!scriptVerifyf(nMinimumRank >= 0, "%s:NETWORK_SESSION_SET_MINIMUM_RANK_FOR_MISSION - Invalid minimum rank. Given: %d. Must be a positive value.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMinimumRank))
		return;

	CNetwork::GetNetworkSession().SetMatchmakingMinimumRank(static_cast<u32>(nMinimumRank));
}

void CommandNetworkCacheLocalPlayerHeadBlendData()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	if(!scriptVerifyf(pPlayerPed, "%s:NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA - No local player ped", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(NetworkInterface::GetLocalPlayer(), "%s:NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA - A network game is not in progress", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	PhysicalPlayerIndex playerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();

	if(!scriptVerifyf(playerIndex != INVALID_PLAYER_INDEX, "%s:NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA - The local player is not fully accepted into the network session yet", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if (NetworkInterface::HasCachedPlayerHeadBlendData(playerIndex))
	{
		NetworkInterface::ClearCachedPlayerHeadBlendData(playerIndex);
	}

	CPedHeadBlendData *pedHeadBlendData = pPlayerPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

	if(!scriptVerifyf(pedHeadBlendData, "%s:NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA - The local player has no head blend data", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if (pedHeadBlendData)
	{
		scriptDebugf1("NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA() called");
		NetworkInterface::CachePlayerHeadBlendData(playerIndex, *pedHeadBlendData);
	}
}

bool CommandNetworkHasCachedPlayerHeadBlendData(int PlayerIndex)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_HAS_CACHED_PLAYER_HEAD_BLEND_DATA - No network game in progress - this only works in MP", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	if(!scriptVerifyf(PlayerIndex >=0 && PlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "%s:NETWORK_HAS_CACHED_PLAYER_HEAD_BLEND_DATA - Invalid player index (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PlayerIndex))
		return false;

	return NetworkInterface::HasCachedPlayerHeadBlendData((PhysicalPlayerIndex)PlayerIndex);
}

bool CommandNetworkApplyCachedPlayerHeadBlendData(int PedIndex, int PlayerIndex)
{
	bool bApplied = false;

	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_APPLY_CACHED_PLAYER_HEAD_BLEND_DATA - No network game in progress - this only works in MP", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	if(!scriptVerifyf(CommandNetworkHasCachedPlayerHeadBlendData(PlayerIndex), "%s:NETWORK_APPLY_CACHED_PLAYER_HEAD_BLEND_DATA - No cached data for this player", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return false;

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pPed)
	{
		scriptDebugf1("NETWORK_APPLY_CACHED_PLAYER_HEAD_BLEND_DATA(%d (%p), %d) called", PedIndex, pPed, PlayerIndex);
		bApplied = NetworkInterface::ApplyCachedPlayerHeadBlendData(*pPed, (PhysicalPlayerIndex)PlayerIndex);
	}

	return bApplied;
}

void CommandFadeOutLocalPlayer(bool bSet)
{
	if(!scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:FADE_OUT_LOCAL_PLAYER - No network game in progress - this only works in MP", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if(!scriptVerifyf(pLocalPlayer, "%s:FADE_OUT_LOCAL_PLAYER - No local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(pLocalPlayer->GetPlayerPed(), "%s:FADE_OUT_LOCAL_PLAYER - Local player has no player ped!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	if(!scriptVerifyf(pLocalPlayer->GetPlayerPed()->GetNetworkObject(), "%s:FADE_OUT_LOCAL_PLAYER - Player ped is networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	static_cast<CNetObjPlayer*>(pLocalPlayer->GetPlayerPed()->GetNetworkObject())->SetAlphaFading(bSet);

#if ENABLE_NETWORK_LOGGING
	if (bSet)
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SCRIPT_CHANGING_VISIBILITY", "%s", pLocalPlayer->GetPlayerPed()->GetNetworkObject()->GetLogName());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("FADE_OUT_LOCAL_PLAYER", "True");
	}
#endif // ENABLE_NETWORK_LOGGING
}

void CommandNetworkFadeOutEntity(int entityId, bool bFlash, bool bNetwork)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pEntity)
	{
		if(!scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_FADE_OUT_ENTITY - Entity is not a physical (ped, vehicle, object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(pEntity));

		if (!scriptVerifyf(pNetObj, "%s:NETWORK_FADE_OUT_ENTITY - The entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		if (pNetObj->GetEntity() && pNetObj->GetEntity()->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pNetObj->GetEntity();

			if (pPed->GetIsInVehicle())
			{
				return;
			}
		}

#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "NETWORK_FADE_OUT_ENTITY", pNetObj->GetLogName());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // ENABLE_NETWORK_LOGGING
		if (bFlash)
		{
			pNetObj->SetAlphaRampFadeOut(bNetwork);
		}
		else
		{
			pNetObj->SetAlphaFading(true, bNetwork);
		}
	}
}

void CommandNetworkFadeInEntity(int entityId, bool bNetwork, bool flash)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if (pEntity)
	{
		if(!scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_FADE_IN_ENTITY - Entity is not a physical (ped, vehicle, object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(pEntity));

		if (!scriptVerifyf(pNetObj, "%s:NETWORK_FADE_IN_ENTITY - The entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "NETWORK_FADE_IN_ENTITY", pNetObj->GetLogName());

#if __FINAL
		pNetObj->SetIsVisible(true, "net");
#else
		pNetObj->SetIsVisible(true, "NETWORK_FADE_IN_ENTITY");
#endif // __FINAL

		if(flash)
		{
			pNetObj->SetAlphaRampFadeInAndQuit(bNetwork);
		}
		else
		{
			pNetObj->SetAlphaIncreasing(bNetwork);
		}
	}
}

void CommandNetworkFlashFadeOutEntityAdvanced(int entityId, int iDuration, bool bNetwork)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityId, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	s16 iDurationShort = (s16)iDuration;

	if (pEntity)
	{
		if (!scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_FLASH_FADE_OUT_ENTITY_ADVANCED - Entity is not a physical (ped, vehicle, object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(pEntity));

		if (!scriptVerifyf(pNetObj, "%s:NETWORK_FLASH_FADE_OUT_ENTITY_ADVANCED - The entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			return;

		if (pNetObj->GetEntity() && pNetObj->GetEntity()->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pNetObj->GetEntity();

			if (pPed->GetIsInVehicle())
			{
				scriptDisplayf("%s:NETWORK_FLASH_FADE_OUT_ENTITY_ADVANCED - Tried to fade out ped in vehicle, ignoring.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return;
			}
		}

		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "NETWORK_FLASH_FADE_OUT_ENTITY_ADVANCED", pNetObj->GetLogName());

		pNetObj->SetAlphaRampFadeOutOverDuration(bNetwork, iDurationShort);
	}
}

bool CommandNetworkIsPlayerFading(int PlayerIndex)
{
	bool bFading = false;

	CNetGamePlayer *pPlayer = CTheScripts::FindNetworkPlayer(PlayerIndex);

	if(scriptVerifyf(pPlayer, "%s : NETWORK_IS_PLAYER_FADING - No player at index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), PlayerIndex))
	{
		CNetObjPlayer* pNetObj = pPlayer->GetPlayerPed() ? SafeCast(CNetObjPlayer, pPlayer->GetPlayerPed()->GetNetworkObject()) : NULL;

		if (pNetObj)
		{
			bFading = pNetObj->IsAlphaRamping() || pNetObj->IsAlphaFading();
		}
	}

	return bFading;
}

bool CommandNetworkIsEntityFading(int entityId)
{
	bool bFading = false;

	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityId);

	if (pEntity)
	{
		if(scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_IS_ENTITY_FADING - Entity is not a physical (ped, vehicle, object)", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(pEntity));

			if (scriptVerifyf(pNetObj, "%s:NETWORK_IS_ENTITY_FADING - The entity is not networked", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				bFading = pNetObj->IsAlphaRamping() || pNetObj->IsAlphaFading();
			}
		}
	}

	return bFading;
}

int CommandGetNumCommerceItems()
{
    return CLiveManager::GetCommerceMgr().GetNumThinDataRecords();
}

bool CommandIsCommerceDataValid()
{
    return CLiveManager::GetCommerceMgr().IsThinDataPopulated();
}


void CommandTriggerCommerceDataFetch( bool /*forceRefetch*/ )
{
    //This is now handled in code
    //if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress() )
    //{
    //    CLiveManager::GetCommerceMgr().PopulateThinData( forceRefetch );
    //}
    
}


bool CommandIsThinDataFetchInProgress()
{
    return CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress();
}

const char * CommandGetCommerceItemId( int productIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( productIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( productIndex )->m_Id;
}

const char* CommandGetCommerceItemName( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_Name;
}

const char* CommandGetCommerceItemImageName( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_ImagePath;
}

const char* CommandGetCommerceItemDesc( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_ShortDesc;
}

bool CommandGetIsCommerceItemPurchased( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return false;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_IsPurchased;
}

const char* CommandGetCommerceProductPrice( int productIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( productIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( productIndex )->m_Price;
}


int CommandGetCommerceItemNumCats( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return 0;
    }

    if ( CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_Category.GetLength() == 0 )
    {
        return 0;
    }
    else
    {
        return 1;
    }
 }

const char* CommandGetCommerceItemCat( int itemIndex, int /*catIndex*/ )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_Category;
}

void CommandReserveCommerceStorePurchaseLocation(int location)
{
    CNetworkTelemetry::GetPurchaseMetricInformation().ReserveFromLocation((eStorePurchaseLocation)location);
}

void CommandOpenCommerceStore(const char* productID, const char* category, int location = SPL_UNKNOWN GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose = false))
{
    //This is currently a placeholder until we work out the best way to have this function with the gameplay stores.
    if ( category != NULL && stricmp(category,"") != 0 )
    {
        atString baseCat(category);
        
        cStoreScreenMgr::SetBaseCategory(baseCat);
    }
    else
    {
        atString baseCat("TOP_DEFAULT");
        cStoreScreenMgr::SetBaseCategory(baseCat);
    }

    if ( productID != NULL && stricmp(productID,"") != 0 )
    {
        atString productIDString(productID);
        cStoreScreenMgr::SetInitialItemID(productIDString);
    }
    else
    {
        atString productIDString;
        productIDString.Reset();
        cStoreScreenMgr::SetInitialItemID(productIDString);
    }

	//Cache the players cash values at this point.
	cStoreScreenMgr::PopulateDisplayValues();
	CNetworkTelemetry::GetPurchaseMetricInformation().SaveCurrentCash();
	CNetworkTelemetry::GetPurchaseMetricInformation().SetFromLocation((eStorePurchaseLocation)location);

	cStoreScreenMgr::Open(false GEN9_STANDALONE_ONLY(, launchLandingPageOnClose));
}

void CommandCheckoutCommerceProduct(const char* productID, int location GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose))
{
	cStoreScreenMgr::CheckoutCommerceProduct(productID, location GEN9_STANDALONE_ONLY(, launchLandingPageOnClose));
}

bool CommandIsStoreOpen()
{
    return cStoreScreenMgr::IsStoreMenuOpen();
}

void CommandSetStoreEnabled( bool enabled )
{
    cStoreScreenMgr::SetStoreScreenEnabled( enabled );
}

void CommandSetStoreBranding( int aBranding )
{
    cStoreScreenMgr::SetBranding(aBranding);
}

bool CommandRequestStoreImage( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return false;
    }

    const char *pImageName = CommandGetCommerceItemImageName(itemIndex);

    if (cStoreScreenMgr::GetTextureManager() && pImageName)
    {
        atString imagePathString(pImageName);
        return cStoreScreenMgr::GetTextureManager()->RequestTexture( imagePathString );
    }
    else
    {
        return false;
    }
}

void CommandReleaseStoreImage( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return;
    }

    const char *pImageName = CommandGetCommerceItemImageName(itemIndex);

    if (cStoreScreenMgr::GetTextureManager() && pImageName)
    {
        atString imagePathString(pImageName);
        return cStoreScreenMgr::GetTextureManager()->ReleaseTexture( imagePathString );
    }
}

void CommandReleaseAllStoreImages()
{
    if ( cStoreScreenMgr::GetTextureManager() )
    {
        cStoreScreenMgr::GetTextureManager()->FreeAllTextures();
    }
}

const char* CommandGetStoreItemTextureName( int itemIndex )
{
    if ( !CLiveManager::GetCommerceMgr().IsThinDataPopulated() || CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex ) == NULL )
    {
        return NULL;
    }

    return CLiveManager::GetCommerceMgr().GetThinDataByIndex( itemIndex )->m_TextureName;
}

void CommandSetCashCommerceProductsEnabled( bool enabled )
{
	cStoreScreenMgr::SetCashProductsAllowed(enabled);
}

bool CommandIsStoreAvailableToUser()
{
	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();

	if (pGamerInfo == NULL)
	{
		return false;
	}

	if ( !CPauseMenu::IsStoreAvailable() )
	{
		return false;
	}

    if (!rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()))
    {
        return false;
    }

	return true; 
}

void CommandDelayStoreOpen()
{
	cStoreScreenMgr::DelayStoreOpen();
}

int CommandGetUserPremiumAccess()
{
    return static_cast<int>(CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack());
}

int CommandGetUserStarterAccess()
{
    return static_cast<int>(CLiveManager::GetCommerceMgr().IsEntitledToStarterPack());
}

void CommandStoreResetNetworkGameTracking()
{
    cStoreScreenMgr::ResetNetworkGameTracking();
}

void CommandStoreSetLastViewedItem(int itemHash, int itemPrice, int shopNameHash)
{
	CNetworkTelemetry::GetPurchaseMetricInformation().m_lastItemViewed = itemHash;
	CNetworkTelemetry::GetPurchaseMetricInformation().m_lastItemViewedPrice = itemPrice;
	CNetworkTelemetry::GetPurchaseMetricInformation().m_storeId = shopNameHash;
}

bool CommandCheckForStoreAge()
{
	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();

	return (pGamerInfo && rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()));
}

#if (RSG_CPU_X86 || RSG_CPU_X64) && RSG_PC
__declspec(noinline) bool CommandNetworkIsCheater()
#else
bool CommandNetworkIsCheater()
#endif
{
    volatile bool isCheater = CNetwork::IsCheater(); 
	return isCheater;
}

int CommandNetworkGetCheaterReason()
{
    return CLiveManager::GetSCGamerDataMgr().GetCheaterReason();
}

bool CommandNetworkIsBadSport()
{
	scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT),  "%s: NETWORK_PLAYER_IS_BADSPORT - Stats have not been loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return StatsInterface::IsBadSport(true);
}

bool CommandTriggerPlayerCrcHackerCheck(int PlayerIndex, int systemToCheck, int specificSubsystem)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:TRIGGER_PLAYER|TUNING_CRC_HACKER_CHECK - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) 
		&& scriptVerifyf((u32)systemToCheck < NetworkHasherUtil::INVALID_HASHABLE, "%s:TRIGGER_PLAYER|TUNING_CRC_HACKER_CHECK - Invalid system to check [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), systemToCheck)
		&& SCRIPT_VERIFY_PLAYER_INDEX("TRIGGER_PLAYER|TUNING_CRC_HACKER_CHECK", PlayerIndex)
		&& CTheScripts::FindNetworkPlayer(PlayerIndex)
		&& NetworkInterface::GetEventManager().CheckForSpaceInPool())
	{
		CNetworkCrcHashCheckEvent::TriggerRequest((PhysicalPlayerIndex)PlayerIndex, (u8)systemToCheck, (u32)specificSubsystem, NULL);
		return true;
	}
	return false;
}

bool CommandTriggerTuningCrcHackerCheck(int PlayerIndex, const char* tuningGroup, const char* tuningItem)
{
	int tuningGroupIdx, tuningItemIdx = -1;
	CTuningManager::GetTuningIndexesFromHash(atStringHash(tuningGroup), atStringHash(tuningItem), tuningGroupIdx, tuningItemIdx);

	if( scriptVerifyf(tuningGroupIdx >= 0 && tuningItemIdx >= 0, "%s:TRIGGER_TUNING_CRC_HACKER_CHECK - Can't find tuning %s in %s group", CTheScripts::GetCurrentScriptNameAndProgramCounter(), tuningItem, tuningGroup) )
	{
		const int packedTuningIndexes = ( (tuningGroupIdx&MAX_UINT16) << 16 )|( tuningItemIdx&MAX_UINT16 );
		return CommandTriggerPlayerCrcHackerCheck(PlayerIndex, NetworkHasherUtil::CRC_TUNING_SYSTEM, packedTuningIndexes);
	}
	return false;
}

bool CommandTriggerFileCrcHackerCheck(int PlayerIndex, const char* filePath)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:TRIGGER_FILE_CRC_HACKER_CHECK - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) 
		&& SCRIPT_VERIFY_PLAYER_INDEX("TRIGGER_FILE_CRC_HACKER_CHECK", PlayerIndex)
		&& CTheScripts::FindNetworkPlayer(PlayerIndex)
		&& NetworkInterface::GetEventManager().CheckForSpaceInPool())
	{
		const u32 fileHash = atStringHash(filePath);
		const char* pFileName = NULL;
		u8 typeOfCheck = NetworkHasherUtil::INVALID_HASHABLE;
		if(DATAFILEMGR.FindDataFile(fileHash))
		{
			// In this case we sync only the hash
			typeOfCheck = NetworkHasherUtil::CRC_DATA_FILE_CONTENTS;
		}
		else
		{
			// Otherwise need to sync whole filepath instead of hash
			pFileName = filePath;
			typeOfCheck = NetworkHasherUtil::CRC_GENERIC_FILE_CONTENTS;
		}
		CNetworkCrcHashCheckEvent::TriggerRequest((PhysicalPlayerIndex)PlayerIndex, typeOfCheck, fileHash, pFileName);
		return true;
	}
	return false;
}


bool CommandRemoteCheaterPlayerDetected(int PlayerIndex, int scrCheatExtraData1, int scrCheatExtraData2)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:REMOTE_CHEATER_PLAYER_DETECTED - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) 
		&& SCRIPT_VERIFY_PLAYER_INDEX("REMOTE_CHEATER_PLAYER_DETECTED", PlayerIndex))
	{
		if( CNetGamePlayer *pNetPlayer = CTheScripts::FindNetworkPlayer(PlayerIndex) )
		{
			NetworkRemoteCheaterDetector::NotifyScriptDetectedCheat(pNetPlayer, scrCheatExtraData1, scrCheatExtraData2);
			return true;
		}
	}
	return false;
}

bool CommandBadSportPlayerLeftDetected(int& handle, int scrCheatExtraData1, int scrCheatExtraData2)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:BAD_SPORT_PLAYER_LEFT_DETECTED - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		rlGamerHandle hGamer;
		if(scriptVerifyf(CTheScripts::ImportGamerHandle(&hGamer, handle, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "BAD_SPORT_PLAYER_LEFT_DETECTED")), 
			"%s:BAD_SPORT_PLAYER_LEFT_DETECTED - Invalid gamer handle", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			NetworkRemoteCheaterDetector::NotifyPlayerLeftBadSport(hGamer, scrCheatExtraData1, scrCheatExtraData2);
			return true;
		}
	}

	return false;
}

void CommandNetworkAddInvalidObjectModel(int modelHashKey)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);
	if (scriptVerifyf(modelId.IsValid(), "%s:NETWORK_ADD_INVALID_OBJECT_MODEL - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetwork::GetObjectManager().AddInvalidObjectModel(modelHashKey);
	}
}

void CommandNetworkRemoveInvalidObjectModel(int modelHashKey)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);
	if (scriptVerifyf(modelId.IsValid(), "%s:NETWORK_REMOVE_INVALID_OBJECT_MODEL - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetwork::GetObjectManager().RemoveInvalidObjectModel(modelHashKey);
	}
}

void CommandNetworkClearInvalidObjectModels()
{
	CNetwork::GetObjectManager().ClearInvalidObjectModels();
}


void CommandAddModelToInstancedContentAllowList(int modelHashKey)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);
	if (scriptVerifyf(modelId.IsValid(), "%s:ADD_MODEL_TO_INSTANCED_CONTENT_ALLOW_LIST - Invalid model index supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CNetwork::GetObjectManager().AddModelToInstancedContentAllowList(modelHashKey);
	}
}

void CommandRemoveAllModelsFromInstancedContentAllowList()
{
	CNetwork::GetObjectManager().RemoveAllModelsFromInstancedContentAllowList();
}

void  CommandNetworkApplyPedScarData(int pedIndex, int characterSlot)
{
	scriptAssertf(characterSlot >= 0,  "%s: NETWORK_APPLY_PED_SCAR_DATA - Invalid characterSlot %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), characterSlot);
	scriptAssertf(characterSlot < MAX_NUM_MP_CHARS,  "%s: NETWORK_APPLY_PED_SCAR_DATA - Invalid characterSlot %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), characterSlot);

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	scriptAssertf(pPed,  "%s: NETWORK_APPLY_PED_SCAR_DATA - Invalid ped index %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedIndex);

	if (pPed && characterSlot >= 0 && characterSlot < MAX_NUM_MP_CHARS)
	{
		scriptDebugf1("Command called - NETWORK_APPLY_PED_SCAR_DATA");
		StatsInterface::ApplyPedScarData(pPed, (u32)characterSlot);
	}
}

CloudRequestID CommandCloudRequestMemberFile(const char* szFileName, int& hData, bool bCache)
{
	if(!scriptVerifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
		return -1; 

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "CLOUD_REQUEST_MEMBER_FILE")))
		return -1;

	return CloudManager::GetInstance().RequestGetMemberFile(NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(hGamer), szFileName, 1024, (bCache ? eRequest_CacheAddAndEncrypt : 0) | eRequest_FromScript);
}

CloudRequestID CommandCloudDeleteMemberFile(const char* szFileName)
{
	if(!scriptVerifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
		return -1; 

	return CloudManager::GetInstance().RequestDeleteMemberFile(NetworkInterface::GetLocalGamerIndex(), szFileName);
}

CloudRequestID CommandCloudRequestCrewFile(const char* szFileName, int nClanID, bool bCache)
{
	if(!scriptVerifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
		return -1; 

	return CloudManager::GetInstance().RequestGetCrewFile(NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(static_cast<s64>(nClanID)), szFileName, 1024, (bCache ? eRequest_CacheAddAndEncrypt : 0) | eRequest_FromScript);
}

CloudRequestID CommandCloudRequestTitleFile(const char* szFileName, bool bCache)
{
	if(!scriptVerifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
		return -1; 

	return CloudManager::GetInstance().RequestGetTitleFile(szFileName, 1024, (bCache ? eRequest_CacheAddAndEncrypt : 0) | eRequest_FromScript);
}

CloudRequestID CommandCloudRequestGlobalFile(const char* szFileName, bool bCache)
{
	if(!scriptVerifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
		return -1; 

	return CloudManager::GetInstance().RequestGetGlobalFile(szFileName, 1024, (bCache ? eRequest_CacheAddAndEncrypt : 0) | eRequest_FromScript);
}

bool CommandCloudHasRequestCompleted(CloudRequestID nRequestID)
{
	return CloudManager::GetInstance().HasRequestCompleted(nRequestID);
}

bool CommandCloudDidRequestSucceed(CloudRequestID nRequestID)
{
	return CloudManager::GetInstance().DidRequestSucceed(nRequestID);
}

int CommandCloudGetRequestResultCode(CloudRequestID nRequestID)
{
	return CloudManager::GetInstance().GetRequestResult(nRequestID);
}

void CommandCloudCheckAvailability()
{
    CloudManager::GetInstance().CheckCloudAvailability();
}

bool CommandCloudIsCheckingAvailability()
{
    return CloudManager::GetInstance().IsCheckingCloudAvailability();
}

int CommandCloudGetAvailabilityCheckPosix()
{
    return static_cast<int>(CloudManager::GetInstance().GetAvailabilityCheckPosix());
}

bool CommandCloudGetAvailabilityCheckResult()
{
    return CloudManager::GetInstance().GetAvailabilityCheckResult();
}

int CommandGetContentToLoadType()
{
#if RSG_ORBIS
	char buffer[50] = {0};
	const char* paramValue = NULL;

	// If the param exists in the args file use that, otherwise read from the systemService.
	if(PARAM_LiveAreaContentType.Get(paramValue) == false && g_SysService.Args().GetParamValue("-LiveAreaContentType", buffer))
	{
		paramValue = buffer;
	}

	if(paramValue)
	{
		u32 hash = atStringHash(paramValue);

		switch(hash)
		{
		case 0x87cb2033: // Hash of UGC_TYPE_GTA5_MISSION
			return UGC_TYPE_GTA5_MISSION;

		case 0x51272f94: // Hash of UGC_TYPE_GTA5_MISSION_PLAYLIST
			return UGC_TYPE_GTA5_MISSION_PLAYLIST;

		case 0x5316011c: // Hash of UGC_TYPE_GTA5_LIFE_INVADER_POST
			return UGC_TYPE_GTA5_LIFE_INVADER_POST;

		case 0x2dd21f30: // Hash of UGC_TYPE_GTA5_PHOTO
			return UGC_TYPE_GTA5_PHOTO;

		case 0xdba068a4: // Hash of UGC_TYPE_GTA5_CHALLENGE
			return UGC_TYPE_GTA5_CHALLENGE;

		default:
			scriptAssertf(false, "Unknown user generated content type '%s'! This must match the enum names in commands_network.sch!", paramValue);
		}
	}
#endif // RSG_ORBIS

	return UGC_TYPE_GTA5_MISSION;
}

bool CommandGetIsLaunchFromLiveArea()
{
#if RSG_ORBIS
	return g_SysService.HasParam("-FromLiveArea") || PARAM_netFromLiveArea.Get();
#else
	return false;
#endif
}

bool CommandGetIsLiveAreaLaunchWithContent()
{
#if RSG_ORBIS
	return (g_SysService.HasParam("-FromLiveArea") || PARAM_netFromLiveArea.Get()) && g_SysService.HasParam("-LiveAreaLoadContent");
#else
	return false;
#endif
}

bool CommandGetHasLaunchParam(const char* ORBIS_ONLY(szParamName))
{
#if RSG_ORBIS
	return g_SysService.HasParam(szParamName);
#else
	return false;
#endif
}

void CommandClearSystemEventArguments()
{
#if RSG_ORBIS
	g_SysService.ClearArgs();
#endif
}

const char* CommandGetLaunchParamValue(const char* ORBIS_ONLY(szParamName))
{
#if RSG_ORBIS
	static char szSysServiceArg[255] = {0};
	if(g_SysService.Args().GetParamValue(szParamName, szSysServiceArg))
	{
		return szSysServiceArg;
	}
	else
#endif // RSG_ORBIS
	{
		return "";
	}
}

bool CommandUgcCopyContent(const char* szContentId, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().Copy(szContentId, kType);
}

bool CommandUgcIsCreating()
{
	return UserContentManager::GetInstance().IsCreatePending();
}

bool CommandUgcHasCreateFinished()
{
	return UserContentManager::GetInstance().HasCreateFinished();
}

bool CommandUgcDidCreateSucceed()
{
	return UserContentManager::GetInstance().DidCreateSucceed();
}

int CommandUgcGetCreateResult()
{
	return UserContentManager::GetInstance().GetCreateResultCode();
}

const char* CommandUgcGetCreateContentID()
{
	return UserContentManager::GetInstance().GetCreateContentID();
}

void CommandUgcClearCreateResult()
{
	UserContentManager::GetInstance().ClearCreateResult();
}

bool CommandUgcQueryContent(const char* szQueryName, const char* szQueryParams, int nOffset, int nMaxCount, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

    return UserContentManager::GetInstance().QueryContent(kType, szQueryName, szQueryParams, nOffset, nMaxCount, CUserContentManager::eQuery_Invalid);
}

enum eUgcBoolOption
{
    eOption_NotSpecified,
    eOption_True,
    eOption_False,
};

bool CommandUgcQueryBookmarkedContent(int nOffset, int nMaxCount, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    // do not define published
    return UserContentManager::GetInstance().GetBookmarked(nOffset, nMaxCount, kType);
}

bool CommandUgcQueryMyContent(int nOffset, int nMaxCount, const char* szContentType, const int nPublishOption, const int nMissionType, const int nOpenOption)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    bool* pbPublish = NULL;
    bool bPublish = false; 
    if(nPublishOption != eOption_NotSpecified)
    {
        bPublish = (nPublishOption == eOption_True) ? true : false;
        pbPublish = &bPublish;
    }

    bool* pbOpen = NULL;
    bool bOpen = false; 
    if(nOpenOption != eOption_NotSpecified)
    {
        bOpen = (nOpenOption == eOption_True) ? true : false;
        pbOpen = &bOpen;
    }

    return UserContentManager::GetInstance().GetMyContent(nOffset, nMaxCount, kType, pbPublish, nMissionType, pbOpen);
}

bool CommandUgcQueryFriendContent(int nOffset, int nMaxCount, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().GetFriendContent(nOffset, nMaxCount, kType);
}

bool CommandUgcQueryCrewContent(int nClanID, int nOffset, int nMaxCount, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().GetClanContent(static_cast<rlClanId>(nClanID), nOffset, nMaxCount, kType);
}

bool CommandUgcQueryByCategory(int nCategory, int nOffset, int nMaxCount, const char* szContentType, const int nSortType, bool bDescending)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().GetByCategory(static_cast<rlUgcCategory>(nCategory), nOffset, nMaxCount, kType, static_cast<eUgcSortCriteria>(nSortType), bDescending);
}

bool CommandUgcQueryByContentID(const char* szContentID, bool bLatest, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    static const unsigned TEXT_LABEL_SIZE = 24;
    CompileTimeAssert(TEXT_LABEL_SIZE >= RLUGC_MAX_CONTENTID_CHARS);

    // we get 32 character text labels from script
    char szContentIDs[1][RLUGC_MAX_CONTENTID_CHARS];
    safecpy(szContentIDs[0], szContentID, RLUGC_MAX_CONTENTID_CHARS);

    return UserContentManager::GetInstance().GetByContentID(szContentIDs, 1, bLatest, kType);
}

bool CommandUgcQueryByContentIDs(int& hData, int nContentIDs, bool bLatest, const char* szContentType)
{
    if(!scriptVerifyf(nContentIDs > 0, "No content IDs provided!"))
        return false; 

    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

	if(!scriptVerify(nContentIDs <= CUserContentManager::MAX_REQUESTED_CONTENT_IDS))
	{
		scriptErrorf("%s:UGC_QUERY_BY_CONTENT_IDS - Too many Content IDs: %d, Max: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContentIDs, CUserContentManager::MAX_REQUESTED_CONTENT_IDS);
		nContentIDs = CUserContentManager::MAX_REQUESTED_CONTENT_IDS;
	}
	scriptDebugf1("%s:UGC_QUERY_BY_CONTENT_IDS - Called with %d Ids", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContentIDs);

    static const unsigned TEXT_LABEL_SIZE = 24;
    CompileTimeAssert(TEXT_LABEL_SIZE >= RLUGC_MAX_CONTENTID_CHARS);

    // array for content IDs
    char szContentIDs[CUserContentManager::MAX_REQUESTED_CONTENT_IDS][RLUGC_MAX_CONTENTID_CHARS];

    // buffer to read from, skip the start of the content array
    char* pDataBuffer = reinterpret_cast<char*>(&hData);
    pDataBuffer += sizeof(scrValue);

    // stash content IDs from script
    for(int i = 0; i < nContentIDs; i++)
    {
        safecpy(szContentIDs[i], pDataBuffer, RLUGC_MAX_CONTENTID_CHARS);
        pDataBuffer += sizeof(scrTextLabel23);
    }

    return UserContentManager::GetInstance().GetByContentID(szContentIDs, nContentIDs, bLatest, kType);
}

bool CommandUgcQueryMostRecentlyCreatedContent(int nOffset, int nMaxCount, const char* szContentType, int nOpenOption)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    bool* pbOpen = NULL;
    bool bOpen = false; 
    if(nOpenOption != eOption_NotSpecified)
    {
        bOpen = (nOpenOption == eOption_True) ? true : false;
        pbOpen = &bOpen;
    }

    return UserContentManager::GetInstance().GetMostRecentlyCreated(nOffset, nMaxCount, kType, pbOpen);
}

bool CommandUgcQueryMostRecentlyPlayedContent(int nOffset, int nMaxCount, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().GetMostRecentlyPlayed(nOffset, nMaxCount, kType);
}

bool CommandUgcQueryTopRatedContent(int nOffset, int nMaxCount, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    return UserContentManager::GetInstance().GetTopRated(nOffset, nMaxCount, kType);
}

bool CommandUgcGetBookmarkedContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

    // do not define published
	return UserContentManager::GetInstance().GetBookmarked(nOffset, nMaxCount, kType);
}

bool CommandUgcGetMyContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetMyContent(nOffset, nMaxCount, kType, NULL, -1, NULL);
}

bool CommandUgcGetMyContentPublished(int nOffset, int nMaxCount, const char* szContentType, int&)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    // define published as true
    bool bPublished = true; 
    return UserContentManager::GetInstance().GetMyContent(nOffset, nMaxCount, kType, &bPublished, -1, NULL);
}

bool CommandUgcGetMyContentUnpublished(int nOffset, int nMaxCount, const char* szContentType, int&)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    // define published as false
    bool bPublished = false; 
    return UserContentManager::GetInstance().GetMyContent(nOffset, nMaxCount, kType, &bPublished, -1, NULL);
}

bool CommandUgcGetFriendContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetFriendContent(nOffset, nMaxCount, kType);
}

bool CommandUgcGetCrewContent(int nClanID, int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetClanContent(static_cast<rlClanId>(nClanID), nOffset, nMaxCount, kType);
}

bool CommandUgcGetByCategory(int nCategory, int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetByCategory(static_cast<rlUgcCategory>(nCategory), nOffset, nMaxCount, kType, eSort_NotSpecified, false);
}

bool CommandUgcGetByContentID(const char* szContentID, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	static const unsigned TEXT_LABEL_SIZE = 24;
	CompileTimeAssert(TEXT_LABEL_SIZE >= RLUGC_MAX_CONTENTID_CHARS);

	// we get 23 character text labels from script
	char szContentIDs[1][RLUGC_MAX_CONTENTID_CHARS];
	safecpy(szContentIDs[0], szContentID, RLUGC_MAX_CONTENTID_CHARS);

	return UserContentManager::GetInstance().GetByContentID(szContentIDs, 1, false, kType);
}

bool CommandUgcGetByContentIDs(int& hData, int nContentIDs, const char* szContentType)
{
	if(!scriptVerifyf(nContentIDs > 0, "No content IDs provided!"))
		return false; 

	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	if(!scriptVerify(nContentIDs < CUserContentManager::MAX_REQUESTED_CONTENT_IDS))
	{
		scriptErrorf("%s:UGC_GET_GET_BY_CONTENT_IDS - Too many Content IDs: %d, Max: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContentIDs, CUserContentManager::MAX_REQUESTED_CONTENT_IDS);
		nContentIDs = CUserContentManager::MAX_REQUESTED_CONTENT_IDS;
	}
	scriptDebugf1("%s:UGC_GET_GET_BY_CONTENT_IDS - Called with %d Ids", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nContentIDs);

	static const unsigned TEXT_LABEL_SIZE = 24;
	CompileTimeAssert(TEXT_LABEL_SIZE >= RLUGC_MAX_CONTENTID_CHARS);

	// array for content IDs
	char szContentIDs[CUserContentManager::MAX_REQUESTED_CONTENT_IDS][RLUGC_MAX_CONTENTID_CHARS];

	// buffer to read from, skip the start of the content array
	char* pDataBuffer = reinterpret_cast<char*>(&hData);
	pDataBuffer += sizeof(scrValue);

	// stash content IDs from script
	for(int i = 0; i < nContentIDs; i++)
	{
		safecpy(szContentIDs[i], pDataBuffer, RLUGC_MAX_CONTENTID_CHARS);
		pDataBuffer += sizeof(scrTextLabel23);
	}

	return UserContentManager::GetInstance().GetByContentID(szContentIDs, nContentIDs, false, kType);
}

bool CommandUgcGetMostRecentlyCreatedContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetMostRecentlyCreated(nOffset, nMaxCount, kType, NULL);
}

bool CommandUgcGetMostRecentlyPlayedContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetMostRecentlyPlayed(nOffset, nMaxCount, kType);
}

bool CommandUgcGetTopRatedContent(int nOffset, int nMaxCount, const char* szContentType, int&)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetTopRated(nOffset, nMaxCount, kType);
}

void CommandUgcCancelQuery()
{
	UserContentManager::GetInstance().CancelQuery();
}

bool CommandUgcIsQuerying()
{
	return UserContentManager::GetInstance().IsQueryPending();
}

bool CommandUgcHasQueryFinished()
{
	return UserContentManager::GetInstance().HasQueryFinished();
}

bool CommandUgcDidQuerySucceed()
{
	return UserContentManager::GetInstance().DidQuerySucceed();
}

bool CommandUgcWasQueryForceCancelled()
{
    return UserContentManager::GetInstance().WasQueryForceCancelled();
}

int CommandUgcGetQueryResult()
{
	return UserContentManager::GetInstance().GetQueryResultCode();
}

void CommandUgcClearQueryResults()
{
	UserContentManager::GetInstance().ClearQueryResults();
}

int CommandUgcGetContentNum()
{
	return static_cast<int>(UserContentManager::GetInstance().GetContentNum());
}

int CommandUgcGetContentTotal()
{
	return UserContentManager::GetInstance().GetContentTotal();
}

int CommandUgcGetContentHash()
{
    return UserContentManager::GetInstance().GetContentHash();
}

const char* CommandUgcGetContentUserID(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentUserID(nContentIndex);
}

bool CommandUgcGetContentCreatorGamerHandle(int nContentIndex, int& hData)
{
	rlGamerHandle hGamer;
	if(UserContentManager::GetInstance().GetContentCreatorGamerHandle(nContentIndex, &hGamer))
		return CTheScripts::ExportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "UGC_GET_CONTENT_CREATOR_GAMER_HANDLE"));

	return false;
}

bool CommandUgcGetContentCreatedByLocalPlayer(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentCreatedByLocalPlayer(nContentIndex);
}

const char* CommandUgcGetContentUserName(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentUserName(nContentIndex);
}

bool CommandUgcGetContentIsUsingScNickname(int nContentIndex)
{
    return UserContentManager::GetInstance().GetContentIsUsingScNickname(nContentIndex);
}

int CommandUgcGetContentCategory(int nContentIndex)
{
	return static_cast<int>(UserContentManager::GetInstance().GetContentCategory(nContentIndex));
}

const char* CommandUgcGetContentID(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentID(nContentIndex);
}

const char* CommandUgcGetRootContentID(int nContentIndex)
{
    return UserContentManager::GetInstance().GetRootContentID(nContentIndex);
}

const char* CommandUgcGetContentName(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentName(nContentIndex);
}

const char* CommandUgcGetContentDescription(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentDescription(nContentIndex);
}

int CommandUgcGetContentDescriptionHash(int nContentIndex)
{
    return static_cast<int>(UserContentManager::GetInstance().GetContentDescriptionHash(nContentIndex));
}

const char* CommandUgcGetContentPath(int nContentIndex, int nFileIndex)
{
    static char szContentPath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
	UserContentManager::GetInstance().GetContentPath(nContentIndex, nFileIndex, szContentPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS);
    return szContentPath;
}

void CommandUgcGetContentCreatedDate(int nContentIndex, int& hData)
{
    time_t t = static_cast<time_t>(UserContentManager::GetInstance().GetContentCreatedDate(nContentIndex));
    struct tm tInfo = *gmtime(&t);

    scrValue* pData = (scrValue*)&hData;
    int size = 0;
    pData[size++].Int = 1900 + tInfo.tm_year;
    pData[size++].Int = tInfo.tm_mon + 1;
    pData[size++].Int = tInfo.tm_mday;
    pData[size++].Int = tInfo.tm_hour;
    pData[size++].Int = tInfo.tm_min;
    pData[size++].Int = tInfo.tm_sec;
}

void CommandUgcGetContentUpdatedDate(int nContentIndex, int& hData)
{
    time_t t = static_cast<time_t>(UserContentManager::GetInstance().GetContentUpdatedDate(nContentIndex));
    struct tm tInfo = *gmtime(&t);

    scrValue* pData = (scrValue*)&hData;
    int size = 0;
    pData[size++].Int = 1900 + tInfo.tm_year;
    pData[size++].Int = tInfo.tm_mon + 1;
    pData[size++].Int = tInfo.tm_mday;
    pData[size++].Int = tInfo.tm_hour;
    pData[size++].Int = tInfo.tm_min;
    pData[size++].Int = tInfo.tm_sec;
}

int CommandUgcGetContentFileVersion(int nContentIndex, int nFileIndex)
{
    return static_cast<int>(UserContentManager::GetInstance().GetContentFileVersion(nContentIndex, nFileIndex));
}

bool CommandUgcGetContentHasLoResPhoto(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentHasLoResPhoto(nContentIndex);
}

bool CommandUgcGetContentHasHiResPhoto(int nContentIndex)
{
	return UserContentManager::GetInstance().GetContentHasHiResPhoto(nContentIndex);
}

int CommandUgcGetContentLanguage(int nContentIndex)
{
    return static_cast<int>(UserContentManager::GetInstance().GetContentLanguage(nContentIndex));
}

int CommandUgcGetContentVersion(int nContentIndex)
{
    return UserContentManager::GetInstance().GetContentVersion(nContentIndex);
}

bool CommandUgcGetContentIsPublished(int nContentIndex)
{
	return UserContentManager::GetInstance().IsContentPublished(nContentIndex);
}

bool CommandUgcGetContentIsVerified(int nContentIndex)
{
    return UserContentManager::GetInstance().IsContentVerified(nContentIndex);
}

float CommandUgcGetContentRating(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return UserContentManager::GetInstance().GetRating(nContentIndex);
}

float CommandUgcGetContentRatingPositivePct(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return UserContentManager::GetInstance().GetRatingPositivePct(nContentIndex);
}

float CommandUgcGetContentRatingNegativePct(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return UserContentManager::GetInstance().GetRatingNegativePct(nContentIndex);
}

int CommandUgcGetContentRatingCount(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return static_cast<int>(UserContentManager::GetInstance().GetRatingCount(nContentIndex));
}

int CommandUgcGetContentRatingPositiveCount(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return static_cast<int>(UserContentManager::GetInstance().GetRatingPositiveCount(nContentIndex));
}

int CommandUgcGetContentRatingNegativeCount(int nContentIndex, bool UNUSED_PARAM(bXv))
{
	return static_cast<int>(UserContentManager::GetInstance().GetRatingNegativeCount(nContentIndex));
}

bool CommandUgcGetContentHasPlayerRecord(int nContentIndex)
{
	if(PARAM_sc_IvePlayedAllMissions.Get())  // B*2154323, this cannot be handled on script side in release
	{
		return true;
	}
	return UserContentManager::GetInstance().HasPlayerRecord(nContentIndex);
}

bool CommandUgcGetContentHasPlayerRated(int nContentIndex)
{
	return UserContentManager::GetInstance().HasPlayerRated(nContentIndex);
}

float CommandUgcGetContentPlayerRating(int nContentIndex)
{
	return UserContentManager::GetInstance().GetPlayerRating(nContentIndex);
}

bool CommandUgcGetContentHasPlayerBookmarked(int nContentIndex)
{
	return UserContentManager::GetInstance().HasPlayerBookmarked(nContentIndex);
}

int CommandUgcRequestCachedDescription(int nHash)
{
	return static_cast<int>(UserContentManager::GetInstance().RequestCachedDescription(nHash));
}

bool CommandUgcIsDescriptionRequestInProgress(int nHash)
{
    return UserContentManager::GetInstance().IsDescriptionRequestInProgress(nHash);
}

bool CommandUgcHasDescriptionRequestFinished(int nHash)
{
    return UserContentManager::GetInstance().HasDescriptionRequestFinished(nHash);
}

bool CommandUgcDidDescriptionRequestSucceed(int nHash)
{
    return UserContentManager::GetInstance().DidDescriptionRequestSucceed(nHash);
}

const char* CommandUgcGetCachedDescription(int nHash, int nMax)
{
    const char* pDescription = UserContentManager::GetInstance().GetCachedDescription(nHash);
    if(!pDescription)
        return "";

    static char szDescription[CUserContentManager::MAX_DESCRIPTION];
    safecpy(szDescription, pDescription, Min(static_cast<unsigned>(nMax), CUserContentManager::MAX_DESCRIPTION));
    return szDescription;
}

bool CommandUgcReleaseCachedDescription(int nHash)
{
    return UserContentManager::GetInstance().ReleaseCachedDescription(nHash);
}

void CommandUgcReleaseAllCachedDescriptions()
{
    UserContentManager::GetInstance().ReleaseAllCachedDescriptions();
}

CloudRequestID CommandUgcRequestContentDataFromIndex(int nContentIndex, int nFileIndex)
{
    return UserContentManager::GetInstance().RequestContentData(nContentIndex, nFileIndex, true);
}

CloudRequestID CommandUgcRequestContentDataFromParams(const char* szContentType, const char* szContentID, int nFileID, int nFileVersion, int nLanguage)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return -1;

    return UserContentManager::GetInstance().RequestContentData(kType, szContentID, nFileID, nFileVersion, static_cast<rlScLanguage>(nLanguage), true);
}

bool CommandUgcHasPermissionToWrite()
{
#if __BANK
	if (UserContentManager::GetInstance().GetSimulateNoUgcWritePrivilege())
	{
		return false;
	}
#endif

	const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
	return credentials.HasPrivilege(RLROS_PRIVILEGEID_UGCWRITE);
}

bool CommandUgcPublish(const char* szContentID, const char* szBaseContentID, const char* szContentType)
{
    // check that the content type string is valid
    rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
    if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
        return false;

    // account for script passing an empty string when they mean NULL
    const char* pBaseContentID = NULL; 
    if(szBaseContentID && szBaseContentID[0] != '\0')
        pBaseContentID = szBaseContentID;

    return UserContentManager::GetInstance().Publish(szContentID, pBaseContentID, kType, nullptr);
}

bool CommandUgcSetBookmarked(const char* szContentID, bool bIsBookmarked, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().SetBookmarked(szContentID, bIsBookmarked, kType);
}

bool CommandUgcSetDeleted(const char* szContentID, bool bIsDeleted, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().SetDeleted(szContentID, bIsDeleted, kType);
}

bool CommandUgcRateContent(const char* szContentID, float fRating, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().SetPlayerData(szContentID, NULL, &fRating, kType);
}

bool CommandUgcIsModifying()
{
	return UserContentManager::GetInstance().IsModifyPending();
}

bool CommandUgcHasModifyFinished()
{
	return UserContentManager::GetInstance().HasModifyFinished();
}

bool CommandUgcDidModifySucceed()
{
	return UserContentManager::GetInstance().DidModifySucceed();
}

int CommandUgcGetModifyResult()
{
	return UserContentManager::GetInstance().GetModifyResultCode();
}

const char* CommandUgcGetModifyContentID()
{
	return UserContentManager::GetInstance().GetModifyContentID();
}

void CommandUgcClearModifyResult()
{
	UserContentManager::GetInstance().ClearModifyResult();
}

bool CommandUgcGetCreatorsTopRated(int nOffset, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	return UserContentManager::GetInstance().GetCreatorsTopRated(nOffset, kType);
}

bool CommandUgcGetCreatorsByUserID(const char* szUserID, const char* szContentType)
{
	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	char szUsersIDs[1][RLROS_MAX_USERID_SIZE];
	safecpy(szUsersIDs[0], szUserID, RLROS_MAX_USERID_SIZE);

	return UserContentManager::GetInstance().GetCreatorsByUserID(szUsersIDs, 1, kType);
}

void CommandUgcClearCreatorResults()
{
	UserContentManager::GetInstance().ClearCreatorResults();
}

bool CommandUgcGetCreatorsByUserIDs(int& hData, int nUserIDs, const char* szContentType)
{
	if(!scriptVerifyf(nUserIDs > 0, "No user IDs provided!"))
		return false; 

	if(!scriptVerifyf(nUserIDs <= CUserContentManager::MAX_CREATOR_RESULTS, "Too many user IDs provided. Max: %d", CUserContentManager::MAX_CREATOR_RESULTS))
		return false; 

	// check that the content type string is valid
	rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
	if(!scriptVerifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "Invalid content string %s", szContentType))
		return false;

	// array for content IDs
	char szUsersIDs[CUserContentManager::MAX_CREATOR_RESULTS][RLROS_MAX_USERID_SIZE];

	// buffer to read from, skip the start of the content array
	char* pDataBuffer = reinterpret_cast<char*>(&hData);
	pDataBuffer += sizeof(scrValue);

	// stash content IDs from script
	for(int i = 0; i < nUserIDs; i++)
	{
		safecpy(szUsersIDs[i], pDataBuffer, RLROS_MAX_USERID_SIZE);
		pDataBuffer += sizeof(scrTextLabel63);
	}

	return UserContentManager::GetInstance().GetCreatorsByUserID(szUsersIDs, nUserIDs, kType);
}

bool CommandUgcIsQueryingCreators()
{
	return UserContentManager::GetInstance().IsQueryCreatorsPending();
}

bool CommandUgcHasQueryCreatorsFinished()
{
	return UserContentManager::GetInstance().HasQueryCreatorsFinished();
}

bool CommandUgcDidQueryCreatorsSucceed()
{
	return UserContentManager::GetInstance().DidQueryCreatorsSucceed();
}

int CommandUgcGetQueryCreatorsResult()
{
	return UserContentManager::GetInstance().GetQueryCreatorsResultCode();
}

int CommandUgcGetCreatorNum()
{
	return static_cast<int>(UserContentManager::GetInstance().GetCreatorNum());
}

int CommandUgcGetCreatorTotal()
{
	return static_cast<int>(UserContentManager::GetInstance().GetCreatorTotal());
}

const char* CommandUgcGetCreatorUserID(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorUserID(nCreatorIndex);
}

const char* CommandUgcGetCreatorUserName(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorUserName(nCreatorIndex);
}

int CommandUgcGetCreatorNumCreated(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorNumCreated(nCreatorIndex);
}

int CommandUgcGetCreatorNumPublished(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorNumPublished(nCreatorIndex);
}

float CommandUgcGetCreatorRating(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorRating(nCreatorIndex);
}

float CommandUgcGetCreatorRatingPositivePct(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorRatingPositivePct(nCreatorIndex);
}

float CommandUgcGetCreatorRatingNegativePct(int nCreatorIndex)
{
	return UserContentManager::GetInstance().GetCreatorRatingNegativePct(nCreatorIndex);
}

int CommandUgcGetCreatorRatingCount(int nCreatorIndex)
{
	return static_cast<int>(UserContentManager::GetInstance().GetCreatorRatingCount(nCreatorIndex));
}

int CommandUgcGetCreatorRatingPositiveCount(int nCreatorIndex)
{
	return static_cast<int>(UserContentManager::GetInstance().GetCreatorRatingPositiveCount(nCreatorIndex));
}

int CommandUgcGetCreatorRatingNegativeCount(int nCreatorIndex)
{
	return static_cast<int>(UserContentManager::GetInstance().GetCreatorRatingNegativeCount(nCreatorIndex));
}

bool CommandUgcGetCreatorIsUsingScNickname(int nCreatorIndex)
{
    return UserContentManager::GetInstance().GetCreatorIsUsingScNickname(nCreatorIndex);
}

bool CommandUgcLoadOfflineQuery(int nCategory)
{
	return UserContentManager::GetInstance().LoadOfflineQueryData(static_cast<rlUgcCategory>(nCategory));
}

void CommandUgcClearOfflineQuery()
{
	return UserContentManager::GetInstance().ClearOfflineQueryData();
}

void CommandUgcSetQueryDataFromOffline(bool bFromOffine)
{
	UserContentManager::GetInstance().SetContentFromOffline(bFromOffine);
}

void CommandUgcSetUsingOfflineContent(bool bUsingOffline)
{
    UserContentManager::GetInstance().SetUsingOfflineContent(bUsingOffline);
}

bool CommandUgcIsLanguageSupported(int nLanguage)
{
    return CText::IsScLanguageSupported(static_cast<rlScLanguage>(nLanguage));
}

bool CommandNetworkCrewInfoGetStringValue(const char* valueName, scrTextLabel63* outStr)
{
	const NetworkCrewMetadata& md = CLiveManager::GetNetworkClan().GetCrewMetadata();
	const bool result = md.GetCrewInfoValueString(valueName, *outStr);

#if !__NO_OUTPUT
	if (!result)
	{
		scriptWarningf("%s:NETWORK_CLAN_CREWINFO_GET_STRING_VALUE - Failed - valueName='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), valueName);
		scriptAssertf(md.Succeeded(), "%s:NETWORK_CLAN_CREWINFO_GET_STRING_VALUE - Failed - valueName='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), valueName);
	}
#endif

	return result;
	}

bool CommandNetworkCrewInfoGetIntValue(const char* valueName, int& outValue)
{
	const NetworkCrewMetadata& md = CLiveManager::GetNetworkClan().GetCrewMetadata();
	return md.GetCrewInfoValueInt(valueName, outValue);
}

bool CommandNetworkCrewInfoGetCrewRankTitle(int rank, scrTextLabel63* outStr)
{
	const NetworkCrewMetadata& md = CLiveManager::GetNetworkClan().GetCrewMetadata();
	return md.GetCrewInfoCrewRankTitle(rank, *outStr);
}

bool CommandNetworkClanMetadataReceived()
{
	const NetworkCrewMetadata& md = CLiveManager::GetNetworkClan().GetCrewMetadata();
	return md.Succeeded();
}

bool CommandFacebookPostPlayMission( const char* RL_FACEBOOK_ONLY(missionId), int RL_FACEBOOK_ONLY(xpEarned), int RL_FACEBOOK_ONLY(rank) )
{
#if RL_FACEBOOK_ENABLED
	return( CLiveManager::GetFacebookMgr().PostPlayMission( missionId, xpEarned, rank ) );
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_PLAY_MISSION - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostPublishMission( const char* RL_FACEBOOK_ONLY(missionId) )
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().PostPublishMission(missionId));
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_PUBLISH_MISSION - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostLikeMission( const char* RL_FACEBOOK_ONLY(missionId) )
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().PostLikeMission(missionId));
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_LIKE_MISSION - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostPublishPhoto( const char* RL_FACEBOOK_ONLY(screenshotId) )
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().PostPublishPhoto(screenshotId));
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_PUBLISH_PHOTO - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostCompleteHeist( const char* RL_FACEBOOK_ONLY(heistId), int RL_FACEBOOK_ONLY(cashEarned), int RL_FACEBOOK_ONLY(xpEarned) )
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().PostCompleteHeist(heistId, cashEarned, xpEarned));
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_COMPLETED_HEIST - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostCompleteMilestone( int RL_FACEBOOK_ONLY(milestoneId) )
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().PostCompleteMilestone((eMileStoneId)milestoneId));
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_COMPLETED_MILESTONE - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookPostCreateCharacter()
{
#if RL_FACEBOOK_ENABLED
	return (CLiveManager::GetFacebookMgr().PostCreateCharacter());
#else
	scriptAssertf(false, "%s:FACEBOOK_POST_CREATE_CHARACTER - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

bool CommandFacebookHasPostCompleted()
{
#if RL_FACEBOOK_ENABLED
	return(!CLiveManager::GetFacebookMgr().IsBusy());
#else
	scriptAssertf(false, "%s:FACEBOOK_HAS_POST_COMPLETED - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return true;
#endif
}

bool CommandFacebookDidPostSucceed()
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().DidSucceed());
#else
	scriptAssertf(false, "%s:FACEBOOK_DID_POST_SUCCEED - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

int CommandFacebookGetPostResultCode()
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().GetLastError());
#else
	scriptAssertf(false, "%s:FACEBOOK_GET_POST_RESULT_CODE - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return 0;
#endif
}

void CommandFacebookEnableGui( bool RL_FACEBOOK_ONLY(ena) )
{
#if RL_FACEBOOK_ENABLED
	CLiveManager::GetFacebookMgr().EnableGUI(ena);
#else
	scriptAssertf(false, "%s:FACEBOOK_ENABLE_GUI - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
}

void CommandFacebookEnableAutoGetAccessToken( bool RL_FACEBOOK_ONLY(ena) )
{
#if RL_FACEBOOK_ENABLED
	CLiveManager::GetFacebookMgr().EnableAutoGetAccessToken(ena);
#else
	scriptAssertf(false, "%s:FACEBOOK_ENABLE_AUTO_GETACCESSTOKEN - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
}

bool CommandFacebookHasAccessToken()
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().HasAccessToken());
#else
	scriptAssertf(false, "%s:FACEBOOK_HAS_ACCESSTOKEN - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif
}

const char* CommandFacebookGetLastPostId()
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().GetLastPostId());
#else
	scriptAssertf(false, "%s:FACEBOOK_GET_LAST_POSTRETURNID - Facebook support has been deprecated. Please open a bug for script to remove these calls",
		CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return "";
#endif
}

int CommandTextureDownloadRequest(int& hGamerData, const char* szCloudPath, const char* szTextureName, bool bUseCacheWithoutCloudChecks)
{
	if (szCloudPath == NULL || szTextureName == NULL)
	{
		SCRIPT_ASSERT(szCloudPath, "TEXTURE_DOWNLOAD_REQUEST - cloudPath is a NULL string");
		SCRIPT_ASSERT(szTextureName, "TEXTURE_DOWNLOAD_REQUEST - textureName is a NULL string");
		return CScriptDownloadableTextureManager::INVALID_HANDLE;
	}

	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hGamerData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "TEXTURE_DOWNLOAD_REQUEST")) == false)
	{
		return static_cast<int>(CScriptDownloadableTextureManager::INVALID_HANDLE);
	}

	static char s_CloudPath[128];
	scriptAssertf(strlen(szCloudPath) < NELEM(s_CloudPath), "%s : TEXTURE_DOWNLOAD_REQUEST - cloudPath string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCloudPath, NELEM(s_CloudPath));
	safecpy(s_CloudPath, szCloudPath, NELEM(s_CloudPath));

	static char s_TextureName[32];
	scriptAssertf(strlen(szTextureName) < NELEM(s_TextureName), "%s : TEXTURE_DOWNLOAD_REQUEST - textureName string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szTextureName, NELEM(s_TextureName));
	safecpy(s_TextureName, szTextureName, NELEM(s_TextureName));

	return SCRIPTDOWNLOADABLETEXTUREMGR.RequestMemberTexture(hGamer, s_CloudPath, s_TextureName, bUseCacheWithoutCloudChecks);
}

int CommandTextureDownloadRequestTitle(const char* szCloudPath, const char* szTextureName, bool bUseCacheWithoutCloudChecks)
{
    if (szCloudPath == NULL || szTextureName == NULL)
    {
        SCRIPT_ASSERT(szCloudPath, "TITLE_TEXTURE_DOWNLOAD_REQUEST - cloudPath is a NULL string");
        SCRIPT_ASSERT(szTextureName, "TITLE_TEXTURE_DOWNLOAD_REQUEST - textureName is a NULL string");
        return CScriptDownloadableTextureManager::INVALID_HANDLE;
    }

    static char s_CloudPath[128];
    scriptAssertf(strlen(szCloudPath) < NELEM(s_CloudPath), "%s : TITLE_TEXTURE_DOWNLOAD_REQUEST - cloudPath string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCloudPath, NELEM(s_CloudPath));
    safecpy(s_CloudPath, szCloudPath, NELEM(s_CloudPath));

    static char s_TextureName[128];
    scriptAssertf(strlen(szTextureName) < NELEM(s_TextureName), "%s : TITLE_TEXTURE_DOWNLOAD_REQUEST - textureName string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szTextureName, NELEM(s_TextureName));
    safecpy(s_TextureName, szTextureName, NELEM(s_TextureName));

    return SCRIPTDOWNLOADABLETEXTUREMGR.RequestTitleTexture(s_CloudPath, s_TextureName, bUseCacheWithoutCloudChecks);
}

int CommandTextureDownloadRequestUgc(const char* szContentID, int nFileID, int nFileVersion, int nLanguage, const char* szTextureName, bool bUseCacheWithoutCloudChecks)
{
    if (szContentID == NULL || szTextureName == NULL)
    {
        SCRIPT_ASSERT(szContentID, "UGC_TEXTURE_DOWNLOAD_REQUEST - szContentID is a NULL string");
        SCRIPT_ASSERT(szTextureName, "UGC_TEXTURE_DOWNLOAD_REQUEST - textureName is a NULL string");
        return CScriptDownloadableTextureManager::INVALID_HANDLE;
    }

    static char s_ContentID[RLUGC_MAX_CONTENTID_CHARS];
    scriptAssertf(strlen(szContentID) < NELEM(s_ContentID), "%s : UGC_TEXTURE_DOWNLOAD_REQUEST - cloudPath string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szContentID, NELEM(s_ContentID));
    safecpy(s_ContentID, szContentID, NELEM(s_ContentID));

    static char s_TextureName[32];
    scriptAssertf(strlen(szTextureName) < NELEM(s_TextureName), "%s : UGC_TEXTURE_DOWNLOAD_REQUEST - textureName string %s is longer than %d characters", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szTextureName, NELEM(s_TextureName));
    safecpy(s_TextureName, szTextureName, NELEM(s_TextureName));

    return SCRIPTDOWNLOADABLETEXTUREMGR.RequestUgcTexture(szContentID, nFileID, nFileVersion, static_cast<rlScLanguage>(nLanguage), s_TextureName, bUseCacheWithoutCloudChecks);
}

void CommandTextureDownloadRelease(int handle)
{
	SCRIPTDOWNLOADABLETEXTUREMGR.ReleaseTexture(static_cast<ScriptTextureDownloadHandle>(handle));
}

bool CommandTextureDownloadHasFailed(int handle)
{
	return SCRIPTDOWNLOADABLETEXTUREMGR.HasTextureFailed(static_cast<ScriptTextureDownloadHandle>(handle));
}

const char* CommandTextureDownloadGetName(int handle)
{
	return SCRIPTDOWNLOADABLETEXTUREMGR.GetTextureName(static_cast<ScriptTextureDownloadHandle>(handle));
}

s32 CommandGetStatusOfTextureDownload(int handle)
{
	if (SCRIPTDOWNLOADABLETEXTUREMGR.HasTextureFailed(static_cast<ScriptTextureDownloadHandle>(handle)))
	{
		return MEM_CARD_ERROR;
	}
	else
	{
		if (SCRIPTDOWNLOADABLETEXTUREMGR.GetTextureName(static_cast<ScriptTextureDownloadHandle>(handle)) != NULL)
		{
			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_BUSY;
}

bool CommandFacebookCanPostToFacebook()
{
#if RL_FACEBOOK_ENABLED
	return(CLiveManager::GetFacebookMgr().CanReportToFacebook());
#else
	// we don't assert here - script APIs can simply call this before any other facebook call.
	return false;
#endif
}

bool CommandCheckROSLinkWentDownNotNet( )
{
	bool wentdown = false;

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		if( settings.Exists(CProfileSettings::ROS_WENT_DOWN_NOT_NET) ) 
		{
			if( settings.GetInt(CProfileSettings::ROS_WENT_DOWN_NOT_NET) )
			{
				wentdown = true;
				settings.Set(CProfileSettings::ROS_WENT_DOWN_NOT_NET, 0);
			}
		}
	}

	return wentdown;
}

int CommandNetworkGetNatType()
{
	return netHardware::GetNatType();
}

bool CommandShouldShowStrictNatWarning()
{
	// might add more logic to this later
#if RSG_PC
	return (netHardware::GetNatType() == NET_NAT_STRICT) &&
		   netIceTunneler::ShouldShowConnectivityTroubleshooting();
#else
	return false;
#endif
}

int CommandNetworkIsCableConnected()
{
	return netPeerAddress::HasNetwork();
}

bool CommandHaveRosPrivateMsgPriv() 
{
	return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_SCS_PRIVATE_MSG_PRIV")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_PRIVATE_MESSAGING);
}

bool CommandHaveRosSocialClubPriv() 
{
    return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_ROS_SOCIAL_CLUB_PRIV")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_SOCIAL_CLUB);
}

bool CommandHaveRosBannedPriv()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	// Do not use rlRos::HasPrivilege in here because it's too conservative and returns true if no ROS creds available (which in this command would mean player is banned: BAD!)
	return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_ROS_BANNED_PRIV")) && rlRos::GetCredentials(localGamerIndex).HasPrivilege(RLROS_PRIVILEGEID_BANNED);
}

bool CommandHaveRosCreateTicketPriv()
{
    return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_ROS_CREATE_TICKET_PRIV")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_CREATE_TICKET);
}

bool CommandHaveRosMultiplayerPriv()
{
    return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_ROS_MULTIPLAYER_PRIV")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_MULTIPLAYER);
}

bool CommandHaveRosLeaderboardWritePriv()
{
    return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAVE_ROS_LEADERBOARD_WRITE_PRIV")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_LEADERBOARD_WRITE);
}

bool CommandHasRosPrivilegePlayedLastGen()
{
	return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAS_ROS_PRIVILEGE_PLAYED_LAST_GEN")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_PLAYED_LAST_GEN);
}

bool CommandHasRosPrivilegeSpecialEditionContent()
{
	return HasValidLocalGamerIndex(OUTPUT_ONLY("NETWORK_HAS_ROS_PRIVILEGE_SPECIAL_EDITION_CONTENT")) && rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_UNLOCK_SPECIAL_EDITION);
}

bool CommandHasRosPrivilege( int privilegeId )
{
	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s NETWORK_HAS_ROS_PRIVILEGE : Local player is not online", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (NetworkInterface::IsLocalPlayerOnline())
	{
		scriptAssertf(privilegeId>RLROS_PRIVILEGEID_NONE && privilegeId<RLROS_NUM_PRIVILEGEID, "%s NETWORK_HAS_ROS_PRIVILEGE : Invalid ROS privilege=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), privilegeId);
		if (privilegeId>RLROS_PRIVILEGEID_NONE && privilegeId<RLROS_NUM_PRIVILEGEID)
		{
			return rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), static_cast<rlRosPrivilegeId>(privilegeId));
		}
	}

	return true;
}

bool CommandHasRosPrivilegeEndDate( int privilegeId, int& isGranted, int& hData)
{
	scriptAssertf(privilegeId>RLROS_PRIVILEGEID_NONE && privilegeId<RLROS_NUM_PRIVILEGEID, "%s NETWORK_HAS_ROS_PRIVILEGE_END_DATE : Invalid ROS privilege=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), privilegeId);
	if (privilegeId>RLROS_PRIVILEGEID_NONE && privilegeId<RLROS_NUM_PRIVILEGEID)
	{
		u64 endPosixTime = 0;

		bool granted = false;

		const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());

		scriptAssertf(credentials.IsValid(), "%s NETWORK_HAS_ROS_PRIVILEGE_END_DATE : Credentials are invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (credentials.IsValid())
		{
			const bool result = credentials.HasPrivilegeEndDate((rlRosPrivilegeId) privilegeId, &granted, &endPosixTime);

			isGranted = 0;
			if (granted)
				isGranted = 1;

			if (result)
			{
				time_t t = static_cast<time_t>( endPosixTime );
				struct tm tInfo = *gmtime(&t);

				scrValue* pData = (scrValue*)&hData;
				int size = 0;
				pData[size++].Int = 1900 + tInfo.tm_year;
				pData[size++].Int = tInfo.tm_mon + 1;
				pData[size++].Int = tInfo.tm_mday;
				pData[size++].Int = tInfo.tm_hour;
				pData[size++].Int = tInfo.tm_min;
				pData[size++].Int = tInfo.tm_sec;

				scriptDebugf1("Command called NETWORK_HAS_ROS_PRIVILEGE_END_DATE - Date = %d:%d:%d:%d:%d:%d",1900 + tInfo.tm_year
																											,tInfo.tm_mon + 1
																											,tInfo.tm_mday
																											,tInfo.tm_hour
																											,tInfo.tm_min
																											,tInfo.tm_sec);
			}
			else
			{
				scriptDebugf1("Command called NETWORK_HAS_ROS_PRIVILEGE_END_DATE - FALSE - Failed to get credentials.HasPrivilegeEndDate() or there is no END date.");
			}

			return result;
		}
	}

	scriptDebugf1("Command called NETWORK_HAS_ROS_PRIVILEGE_END_DATE - FALSE");

	return false;
}

bool CommandHasReceivedForceSessionEvent()
{
	return CNetwork::GetNetworkSession().GetAndResetForceScriptPresenceUpdate();
}

int CommandStartCommunicationPermissionsCheck(int& DURANGO_ONLY(hData))
{
#if RSG_DURANGO
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_START_COMMUNICATION_PERMISSIONS_CHECK")))
		return CLiveManager::StartPermissionsCheck(IPrivacySettings::PERM_CommunicateUsingVoice, hGamer);
#endif
	return -1;
}

int CommandStartTextCommunicationPermissionsCheck(int& DURANGO_ONLY(hData))
{
#if RSG_DURANGO
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_START_TEXT_COMMUNICATION_PERMISSIONS_CHECK")))
		return CLiveManager::StartPermissionsCheck(IPrivacySettings::PERM_CommunicateUsingText, hGamer);
#endif
	return -1;
}

int CommandStartUserContentPermissionsCheck(int& XBOX_ONLY(hData))
{
#if RSG_XBOX
	rlGamerHandle hGamer;
	if(CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_START_USER_CONTENT_PERMISSIONS_CHECK")))
		return CLiveManager::StartPermissionsCheck(IPrivacySettings::PERM_ViewTargetUserCreatedContent, hGamer);
#endif
	return -1;
}

void CommandSkipRadioResetNextClose()
{
	audDisplayf("Network - SkipRadioResetNextClose: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CNetwork::SkipRadioResetNextClose();
}

bool CommandShouldWarnOfSimpleModCheck()
{
#if RSG_PC
#define SIMPLEMOD_ID 0xC7C09002
	//if(Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("SKIP_RADIO_WARNING", 0xC7036B09), false))
	if(Tunables::GetInstance().TryAccess(0x38CEB237, 0xC7036B09, true))
	{
		bool shouldWarn = CNetwork::GetNetworkSession().GetDinputCount().Get() > 1;
		
		if(shouldWarn)
		{
			// TODO: Insert Metric to be created / flushed here
 		}
		return shouldWarn;
	}
	return false;
#else
	return false;
#endif
}
void CommandSkipRadioResetNextOpen()
{
	audDisplayf("Network - SkipRadioResetNextOpen: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CNetwork::SkipRadioResetNextOpen();
}

void CommandForceLocalPlayerScarSync()
{
	if (NetworkInterface::IsGameInProgress())
	{
		CUpdatePlayerScarsEvent::Trigger();
	}
}

void CommandDisableLeaveRemotePedBehind(bool flag)
{
	scriptDebugf1("%s - NETWORK_DISABLE_LEAVE_REMOTE_PED_BEHIND - Called with flag='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), flag ? "true":"false");

	CPed* player = FindPlayerPed();
	if (player && player->GetNetworkObject())
	{
		scriptDebugf1("%s - NETWORK_DISABLE_LEAVE_REMOTE_PED_BEHIND called on %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), player->GetNetworkObject()->GetLogName());
		static_cast<CNetObjPlayer*>( player->GetNetworkObject() )->SetDisableLeavePedBehind( flag );
	}
}

bool CommandNetworkCanUseMissionCreators()
{
    if(PARAM_netDisableMissionCreators.Get())
    {
        return false;
    }

    return true;
}

void CommandAddPlayerPedDecoration(int CollectionNameHash, int PresetNameHash, int tournamentId)
{
	CPed* player = FindPlayerPed();

	if (player)
	{	
		PEDDECORATIONMGR.AddPedDecoration(player, CollectionNameHash, PresetNameHash, tournamentId);
	}
}

void CommandNetworkAllowRemoteAttachmentModification(int EntityIndex, bool canModify)
{
    CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if (SCRIPT_VERIFY(pPhysical, "NETWORK_ALLOW_REMOTE_ATTACHMENT_MODIFICATION - The entity does not exist"))
    {
		scriptDebugf1("%s - NETWORK_ALLOW_REMOTE_ATTACHMENT_MODIFICATION called on %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPhysical->GetNetworkObject() ? pPhysical->GetNetworkObject()->GetLogName() : "Not Networked!");
        NetworkInterface::SetScriptModifyRemoteAttachment(*pPhysical, canModify);
    }
}

bool CommandNetworkIsRemoteAttachmentModificationAllowed(int EntityIndex)
{
    const CPhysical *pPhysical = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

    if (SCRIPT_VERIFY(pPhysical, "NETWORK_IS_REMOTE_ATTACHMENT_MODIFICATION_REQUIRED - The entity does not exist"))
    {
        return NetworkInterface::CanScriptModifyRemoteAttachment(*pPhysical);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool INIT_GAMER_HANDLES_FROM_SCRIPT_ARRAY(rlGamerHandle* toHandles, scrArrayBaseAddr& fromHandles, const u32 numHandles  ASSERT_ONLY(, const char* command))
{
	u8* pData = scrArray::GetElements<u8>(fromHandles);

	if (pData && numHandles <= MAX_LEADERBOARD_ROWS)
	{
		for (u32 i=0; i<numHandles; i++, pData += GAMER_HANDLE_SIZE)
		{
			if(!CTheScripts::ImportGamerHandle(&toHandles[i], pData ASSERT_ONLY(, command)))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void CommandNetworkGetPrimaryClanDataClear( )
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA_CLEAR - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(!s_leaderboardClanData.Pending(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA_CLEAR - Invalid status %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.GetStatus());

	if (!s_leaderboardClanData.Pending())
		s_leaderboardClanData.Clear();
}

void CommandNetworkGetPrimaryClanDataCancel( )
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA_CANCEL - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(s_leaderboardClanData.Pending(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA_CANCEL - Invalid status %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.GetStatus());

	if (s_leaderboardClanData.Pending())
		s_leaderboardClanData.Cancel();
}

bool CommandNetworkGetPrimaryClanDataStart(scrArrayBaseAddr& array_LBGamerHandles, const int nGamerHandles)
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA_START - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(s_leaderboardClanData.Idle(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA_START - Invalid status %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.GetStatus());
	if (!s_leaderboardClanData.Idle())
		return false;

	scriptAssertf(nGamerHandles <= RL_MAX_GAMERS_PER_SESSION, "%s - NETWORK_GET_PRIMARY_CLAN_DATA_START - Invalid number of gamer %d, max is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nGamerHandles, RL_MAX_GAMERS_PER_SESSION);
	if (nGamerHandles > RL_MAX_GAMERS_PER_SESSION)
		return false;

	rlGamerHandle hGamerHandles[RL_MAX_GAMERS_PER_SESSION];
	if (nGamerHandles > 0)
	{
		scriptAssertf(nGamerHandles <= MAX_LEADERBOARD_ROWS, "%s, NETWORK_GET_PRIMARY_CLAN_DATA_START - Invalid number of GamerHandles='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nGamerHandles);
		if (nGamerHandles > MAX_LEADERBOARD_ROWS)
			return false;

		if (!INIT_GAMER_HANDLES_FROM_SCRIPT_ARRAY(hGamerHandles, array_LBGamerHandles, (u32)nGamerHandles ASSERT_ONLY(, "NETWORK_GET_PRIMARY_CLAN_DATA_START")))
		{
			return false;
		}
	}

	return s_leaderboardClanData.Start(hGamerHandles, nGamerHandles);
}

bool CommandNetworkGetPrimaryClanDataPending()
{
	return s_leaderboardClanData.Pending();
}

bool CommandNetworkGetPrimaryClanDataSuccess()
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA_SUCCESS - Called, result=%s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.Succeeded()?"True":"False");
	return s_leaderboardClanData.Succeeded();
}

bool CommandNetworkGetPrimaryClanDataNew(int& hData, scrClanDesc* pArray_ClanData)
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA_NEW - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(s_leaderboardClanData.Succeeded(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA_NEW - Invalid status %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.GetStatus());
	if (!s_leaderboardClanData.Succeeded())
		return false;

	scriptAssertf(pArray_ClanData, "%s - NETWORK_GET_PRIMARY_CLAN_DATA_NEW - NULL pArray_ClanData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!pArray_ClanData)
		return false;

	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, hData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GET_PRIMARY_CLAN_DATA_NEW")))
		return false;

	for(int i=0; i<s_leaderboardClanData.GetResultSize(); ++i)
	{
		if (hGamer == s_leaderboardClanData[i].m_MemberInfo.m_GamerHandle)
		{
			scriptAssertf(s_leaderboardClanData[i].m_MemberClanInfo.IsValid(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA_NEW - Member Clan Info is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if(s_leaderboardClanData[i].m_MemberClanInfo.IsValid())
			{
				if (Util_rlClanMembershipDataToscrClanDesc(s_leaderboardClanData[i].m_MemberClanInfo, pArray_ClanData))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CommandNetworkGetPrimaryClanData(ScriptClanData* pArray_ClanData, int& numResults)
{
	scriptDebugf1("%s - NETWORK_GET_PRIMARY_CLAN_DATA - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	numResults = 0;

	scriptAssertf(s_leaderboardClanData.Succeeded(), "%s - NETWORK_GET_PRIMARY_CLAN_DATA - Invalid status %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_leaderboardClanData.GetStatus());
	if (!s_leaderboardClanData.Succeeded())
		return false;

	scriptAssertf(pArray_ClanData, "%s - NETWORK_GET_PRIMARY_CLAN_DATA - NULL pArray_ClanData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!pArray_ClanData)
		return false;

	//Setup number of result
	numResults = s_leaderboardClanData.GetResultSize();

	//Account for the padding at the front of the array that holds the length of the array.
	ASSERT_ONLY(int scriptNumOfItems = *((int*)(pArray_ClanData)));
	scriptAssertf(scriptNumOfItems >= numResults, "%s:NETWORK_GET_PRIMARY_CLAN_DATA - Script array isn't big enough (%d) for the maxNumRows (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptNumOfItems, numResults);
	//Now we bounce the pointer over one to point to the actual start of the array
	pArray_ClanData = (ScriptClanData*)((scrValue*)(pArray_ClanData) + 1);

	for(int i=0; i<numResults; ++i)
	{
		pArray_ClanData[i].m_ClanId.Int = (int)s_leaderboardClanData[i].m_MemberClanInfo.m_Clan.m_Id;

		if (s_leaderboardClanData[i].m_MemberInfo.m_GamerHandle.IsValid())
		{
			sysMemSet(pArray_ClanData[i].m_GamerHandle, 0, COUNTOF(pArray_ClanData[i].m_GamerHandle));
			CTheScripts::ExportGamerHandle(&s_leaderboardClanData[i].m_MemberInfo.m_GamerHandle
											,pArray_ClanData[i].m_GamerHandle[0].Int
											,SCRIPT_GAMER_HANDLE_SIZE
											 ASSERT_ONLY(,"NETWORK_GET_PRIMARY_CLAN_DATA"));
		}

		sysMemSet(pArray_ClanData[i].m_ClanName, 0, sizeof(pArray_ClanData[i].m_ClanName));
		safecpy(pArray_ClanData[i].m_ClanName, s_leaderboardClanData[i].m_MemberClanInfo.m_Clan.m_ClanName);

		sysMemSet(pArray_ClanData[i].m_ClanTag, 0, sizeof(pArray_ClanData[i].m_ClanTag));
		safecpy(pArray_ClanData[i].m_ClanTag, s_leaderboardClanData[i].m_MemberClanInfo.m_Clan.m_ClanTag);
	}

	return (numResults>0);
}


//////////////////////////////////////////////////////////////////////////

int   CommandNetworkProfileStatsReaderGetStatus()
{
	enum eStatus {IDLE, PENDING, SUCCEEEDED, FAILED};
	int status = IDLE;

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_GET_STATUS - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return status;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_GET_STATUS - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return status;

	const CProfileStatsRead* reader = s_ScriptProfileStatsReader.GetReader();
	scriptAssertf(reader, "%s:NETWORK_PROFILE_STATS_READER_GET_STATUS - No reader!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!reader)
		return status;

	if (reader->Pending())
		status = PENDING;
	else if(reader->Succeeded())
		status = SUCCEEEDED;
	else if(reader->Failed())
		status = FAILED;

	return status;
}

void   CommandNetworkProfileStatsReaderEnd()
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_END - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_END - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return;

	s_ScriptProfileStatsReader.Clear();
}

bool   CommandNetworkProfileStatsReaderStartRead()
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_START - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_START - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return false;

	return s_ScriptProfileStatsReader.StartRead();
}

bool   CommandNetworkProfileStatsReaderAddGamer(int& handleData)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_ADD_GAMER - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_ADD_GAMER - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return false;

	// validate handle data
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_PROFILE_STATS_READER_ADD_GAMER")))
		return false; 

	scriptAssertf(hGamer.IsLocal(), "%s:NETWORK_PROFILE_STATS_READER_ADD_GAMER - gamer is local!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!hGamer.IsLocal())
		return false;

	return s_ScriptProfileStatsReader.AddGamer( hGamer );
}

bool   CommandNetworkProfileStatsReaderAddStatId(int statid)
{
	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_ADD_STATID - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_ADD_STATID - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return false;

	return s_ScriptProfileStatsReader.AddStatId(statid);
}

bool CommandNetworkProfileStatsReaderGetStatValue(int statid, int& handleData, int& i_value, float& f_value, int& noValue)
{
	i_value = 0;
	f_value = 0.0f;
	noValue = 0;

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE - Not in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(!NetworkInterface::IsGameInProgress())
		return false;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s:NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE - Not in freemode!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(!NetworkInterface::IsInFreeMode())
		return false;

	// validate handle data
	rlGamerHandle hGamer;
	if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE")))
		return false; 

	const CProfileStatsRead* reader = s_ScriptProfileStatsReader.GetReader();
	scriptAssertf(reader, "%s:NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE - No reader!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!reader)
		return false;

	const rlProfileStatsValue* statvalue = reader->GetStatValue(statid, hGamer);
	scriptAssertf(statvalue, "%s:NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE - No satat value for stat '%s'!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), StatsInterface::GetKeyName(statid));
	if (!statvalue)
		return false;

	const rlProfileStatsType type = statvalue->GetType();

	if (RL_PROFILESTATS_TYPE_FLOAT == type)
		f_value = statvalue->GetFloat();
	else if (RL_PROFILESTATS_TYPE_INT32 == type)
		i_value = statvalue->GetInt32();
	else if (RL_PROFILESTATS_TYPE_INT64 == type)
		i_value = (int)statvalue->GetInt64();
	else
	{
		noValue = 1;
		scriptWarningf("%s:NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE - stat '%s' is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), StatsInterface::GetKeyName(statid));
	}

	return true;
}

void CommandNetworkShowChatRestrictionMessage(int UNUSED_PARAM(localGamerIndex))
{
#if RSG_SCE
	//scriptAssertf(RL_VERIFY_LOCAL_GAMER_INDEX(localGamerIndex), "%s:NETWORK_SHOW_CHAT_RESTRICTION_MSC - Invalid localGamerIndex='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), localGamerIndex);
	//
	//if (RL_VERIFY_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		g_rlNp.GetCommonDialog().ShowChatRestrictionMsg( g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex()));
	}

#else
	scriptAssertf(0, "%s:NETWORK_SHOW_CHAT_RESTRICTION_MSC - Invalid command for this platform!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
}

void CommandNetworkShowPsnUgcRestriction( )
{
#if RSG_SCE
	g_rlNp.GetCommonDialog().ShowUGCRestrictionMsg ( g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex()));
#else
	scriptAssertf(0, "%s:NETWORK_SHOW_PSN_UGC_RESTRICTION - Invalid command for this platform!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
}

void CommandNetworkShowPlayerReviewDialog(int PROSPERO_ONLY(mode))
{
#if RSG_PROSPERO
	g_rlNp.GetCommonDialog().ShowPlayerReviewDialog(NetworkInterface::GetLocalGamerIndex(), (rlNpCommonDialog::ePlayerReviewMode)mode);
#endif
}

bool CommandNetworkShowWebBrowser(const char* url)
{
	scriptDebugf1("%s - NETWORK_SHOW_WEB_BROWSER(%s) - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), url);
	scriptAssertf(url && strlen(url)!=0, "%s:NETWORK_SHOW_WEB_BROWSER - Invalid url", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if(RL_VERIFY_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
	{
		return g_SystemUi.ShowWebBrowser(NetworkInterface::GetLocalGamerIndex(), url);
	}
	scriptAssertf(0, "%s:NETWORK_SHOW_WEB_BROWSER - Invalid local gamer index", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
}

bool CommandIsTitleUpdateRequired()
{
#if RSG_ORBIS
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && !g_rlNp.GetNpAuth().IsNpAvailable(localGamerIndex))
	{
		const rlNpAuth::NpUnavailabilityReason reason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason(NetworkInterface::GetLocalGamerIndex());
		return (reason == rlNpAuth::NP_REASON_GAME_UPDATE);
	}
#endif

	return false;
}

void  CommandNetworkQuitMultiplayerToDesktop()
{
	scriptDebugf1("%s:NETWORK_QUIT_MP_TO_DESKTOP - Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if RSG_PC
	NetworkInterface::GetNetworkExitFlow().StartExitSaveFlow();
#else
	scriptAssertf(0, "%s:NETWORK_QUIT_MP_TO_DESKTOP - This function is PC only", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
}

bool CommandNetworkIsConnectedViaRelay(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_IS_CONNECTED_VIA_RELAY - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::IsConnectedViaRelay(player->GetPhysicalPlayerIndex());
    }

    return false;
}

float CommandNetworkGetAverageLatency(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_AVERAGE_LATENCY - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetAverageSyncLatency(player->GetPhysicalPlayerIndex());
    }

    return 0.0f;
}

float CommandNetworkGetAveragePing(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_AVERAGE_PING - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetAverageSyncRTT(player->GetPhysicalPlayerIndex());
    }

    return 0.0f;
}

float CommandNetworkGetAveragePacketLoss(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_AVERAGE_PACKET_LOSS - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetAveragePacketLoss(player->GetPhysicalPlayerIndex());
    }

    return 0.0f;
}

unsigned CommandNetworkGetNumUnackedReliables(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_NUM_UNACKED_RELIABLES - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetNumUnAckedReliables(player->GetPhysicalPlayerIndex());
    }

    return 0;
}

unsigned CommandNetworkGetUnreliableResendCount(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_UNRELIABLE_RESEND_COUNT - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetUnreliableResendCount(player->GetPhysicalPlayerIndex());
    }

    return 0;
}

void CommandNetworkReportCodeTamper( )
{
	scriptDebugf1("%s:NETWORK_REPORT_CODE_TAMPER - cOMMAND CALLED!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::CODE_TAMPERING);
}

scrVector CommandNetworkGetLastEntityPosReceivedOverNetwork(int EntityIndex)
{
    Vector3 lastPosReceived = VEC3_ZERO;

    const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

    if(scriptVerifyf(pEntity, "%s:NETWORK_GET_LAST_ENTITY_POS_RECEIVED_OVER_NETWORK - Invalid Entity!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

        if(scriptVerifyf(networkObject, "%s:NETWORK_GET_LAST_ENTITY_POS_RECEIVED_OVER_NETWORK - Cannot call this command on non-networked entities!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if(scriptVerifyf(networkObject->IsClone(), "%s:NETWORK_GET_LAST_ENTITY_POS_RECEIVED_OVER_NETWORK - This command can only be called on remote entities!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                if(scriptVerifyf(pEntity->GetIsPhysical(), "%s:NETWORK_GET_LAST_ENTITY_POS_RECEIVED_OVER_NETWORK - This command can only be called on physical entities!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
                {
                    lastPosReceived = NetworkInterface::GetLastPosReceivedOverNetwork(static_cast<const CPhysical *>(pEntity));
                }
            }
        }
    }

    return lastPosReceived;
}

scrVector CommandNetworkGetLastPlayerPosReceivedOverNetwork(int playerIndex)
{
    Vector3 lastPosReceived = VEC3_ZERO;

    CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(playerIndex);

    if(scriptVerifyf(pPlayerPed, "%s:NETWORK_GET_LAST_PLAYER_POS_RECEIVED_OVER_NETWORK - Invalid Player!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
        if(scriptVerifyf(pPlayerPed->GetNetworkObject(), "%s:NETWORK_GET_LAST_PLAYER_POS_RECEIVED_OVER_NETWORK - Cannot call this command on non-networked players!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
        {
            if(scriptVerifyf(pPlayerPed->IsNetworkClone(), "%s:NETWORK_GET_LAST_PLAYER_POS_RECEIVED_OVER_NETWORK - This command can only be called on remote players!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
            {
                lastPosReceived = NetworkInterface::GetLastPosReceivedOverNetwork(pPlayerPed);
            }
        }
    }

    return lastPosReceived;
}

scrVector CommandNetworkGetLastVelReceivedOverNetwork(int entityIndex)
{
	 const CEntity* entity =  CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	 if(entity && entity->GetIsDynamic())
	 {
		 return NetworkInterface::GetLastVelReceivedOverNetwork((const CDynamicEntity*)entity);
	 }
	 return VEC3_ZERO;
}

scrVector CommandNetworkGetPredictedVelocity(int entityIndex, const float maxSpeedToPredict)
{
	const CEntity* entity =  CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	if(entity && entity->GetIsDynamic())
	{
		return NetworkInterface::GetPredictedVelocity((const CDynamicEntity*)entity, maxSpeedToPredict);
	}
	return VEC3_ZERO;
}

unsigned CommandNetworkGetHighestReliableResendCount(int playerIndex)
{
    netPlayer *player = CTheScripts::FindNetworkPlayer(playerIndex);

    scriptAssertf(0, "%s:NETWORK_GET_HIGHEST_RELIABLE_RESEND_COUNT - This shouldn't be called on the local player!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

    if(player && !player->IsLocal())
    {
        return NetworkInterface::GetHighestReliableResendCount(player->GetPhysicalPlayerIndex());
    }

    return 0;
}

#if !__NO_OUTPUT && RSG_ORBIS
void CommandNetworkDumpNetIfConfig()
{
	netHardware::DumpNetIfConfig();
}

void CommandNetworkGetNetStatisticsInfo(ORBIS_ONLY(scrSceNetStatisticsInfo* pInfo))
{
	netHardware::rlSceNetStatisticsInfo info; 
	netHardware::GetNetStatisticsInfo(info);

	// populate script structure
	pInfo->m_KernelMemFreeSize.Int = info.m_KernelMemFreeSize;
	pInfo->m_KernelMemFreeMin.Int = info.m_KernelMemFreeMin;
	pInfo->m_PacketCount.Int = info.m_PacketCount;
	pInfo->m_PacketQosCount.Int = info.m_PacketQosCount;
	pInfo->m_LibNetFreeSize.Int = info.m_LibNetFreeSize;
	pInfo->m_LibNetFreeMin.Int = info.m_LibNetFreeMin;
}

void CommandNetworkGetSignallingInfo(ORBIS_ONLY(scrSceNetSignallingInfo* pInfo))
{
	// populate script structure
	pInfo->m_TotalMemSize.Int = 0;
	pInfo->m_CurrentMemUsage.Int = 0;
	pInfo->m_MaxMemUsage.Int = 0;
}
#else
void CommandDoNothing() {}
#endif

int CommandNetworkGetPlayerAccountId(int playerIndex)
{
	CNetGamePlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);
	if (!player) 
	{
		scriptErrorf("NETWORK_GET_PLAYER_ACCOUNT_ID :: Invalid player index %d", playerIndex);
		return 0;
	}
	return player->GetPlayerAccountId();
}

void CommandNetworkUDSActivityStart(const char* UNUSED_PARAM(activityId))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityStartWithActors(const char* UNUSED_PARAM(activityId), const char* UNUSED_PARAM(primaryActorId), scrActivitySecondaryActors* UNUSED_PARAM(pSecondaryActors))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityEnd(const char* UNUSED_PARAM(activityId), int UNUSED_PARAM(iOutcome), int UNUSED_PARAM(iScore))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityResume(const char* UNUSED_PARAM(activityId))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityResumeWithTasks(const char* UNUSED_PARAM(activityId), scrActivityTaskStatus* UNUSED_PARAM(pActivityTaskData))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityAvailabilityChange(scrActivityAvailabilityData* UNUSED_PARAM(pActivityAvailabilityData))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

void CommandNetworkUDSActivityPriorityChange(scrActivityPriorityChangeData* UNUSED_PARAM(pActivityPriorityChangeData))
{
#if GEN9_STANDALONE_ENABLED
	// stub
#else // GEN9_STANDALONE_ENABLED
	// do nothing if gen9 is disabled
#endif // GEN9_STANDALONE_ENABLED
}

bool CommandNetworkHasScMembershipInfo()
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	return rlScMembership::HasMembershipData(NetworkInterface::GetLocalGamerIndex());
#else
	return false;
#endif
}

bool CommandNetworkHasScMembership()
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	return rlScMembership::HasMembership(NetworkInterface::GetLocalGamerIndex());
#else
	return false;
#endif
}

void CommandNetworkGetScMembershipInfo(scrScMembershipInfo* outInfo)
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	const rlScMembershipInfo& info = rlScMembership::GetMembershipInfo(NetworkInterface::GetLocalGamerIndex());

	// populate script structure
	outInfo->m_HasMembership.Int = info.HasMembership();
	outInfo->m_MembershipStartPosix.Int = static_cast<int>(info.GetStartPosix());
	outInfo->m_MembershipEndPosix.Int = static_cast<int>(info.GetExpirePosix());
#else
	(void)outInfo;
#endif
}

bool CommandNetworkCanScMembershipExpire()
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	return rlScMembership::CanMembershipExpire();
#else
	return false;
#endif
}

bool CommandNetworkHasShownMembershipWelcome()
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	return CLiveManager::HasShownMembershipWelcome();
#else
	return false;
#endif
}

void CommandNetworkUgcNav(const int feature, const int location)
{
	MetricUGC_NAV m(feature, location);
	APPEND_METRIC(m);
}

void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_GET_STATUS,0x62b8502e363049f5, CommandNetworkProfileStatsReaderGetStatus);
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_END,0x85735d6be67b5378, CommandNetworkProfileStatsReaderEnd);
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_START,0x56e3f5ef646b5916, CommandNetworkProfileStatsReaderStartRead);
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_ADD_GAMER,0x9c6ae5b6daf4b649, CommandNetworkProfileStatsReaderAddGamer);
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_ADD_STATID,0xddf4b4c93bc9814c, CommandNetworkProfileStatsReaderAddStatId);
	SCR_REGISTER_UNUSED(NETWORK_PROFILE_STATS_READER_GET_STAT_VALUE,0x924911e50692ed38, CommandNetworkProfileStatsReaderGetStatValue);

	// commands
	SCR_REGISTER_UNUSED(NETWORK_AUTO_MULTIPLAYER_LAUNCH,0xb011bc30d2b41385, CommandNetworkAutoMultiplayerLaunch);
	SCR_REGISTER_SECURE(GET_ONLINE_VERSION,0xe1b4508c5d42ba97, CommandGetOnlineVersion);

	// local player queries
	SCR_REGISTER_SECURE(NETWORK_IS_SIGNED_IN,0xaf5dbe95205a49d1, CommandNetworkIsSignedIn);
	SCR_REGISTER_SECURE(NETWORK_IS_SIGNED_ONLINE,0x20e4972cbf3dbe1b, CommandNetworkIsOnline);
	SCR_REGISTER_UNUSED(NETWORK_IS_GUEST,0x6ca1c8618f47d738, CommandNetworkIsGuest);
	SCR_REGISTER_UNUSED(NETWORK_IS_SC_OFFLINE_MODE,0x507e576fd928a23c, CommandNetworkIsScOfflineMode);
    SCR_REGISTER_UNUSED(NETWORK_IS_NP_OFFLINE,0x945bd59469d7fc69, CommandNetworkIsNpOffline);
	SCR_REGISTER_SECURE(NETWORK_IS_NP_AVAILABLE,0x82fc2fd8e4f14d53, CommandNetworkIsNpAvailable);
	SCR_REGISTER_SECURE(NETWORK_IS_NP_PENDING,0x164c5b38c79eef17, CommandNetworkIsNpPending);
	SCR_REGISTER_SECURE(NETWORK_GET_NP_UNAVAILABLE_REASON,0x50628aa00a11e2d9, CommandNetworkGetNpUnavailableReason);
	SCR_REGISTER_SECURE(NETWORK_IS_CONNETED_TO_NP_PRESENCE,0xc725b46872d7da81, CommandNetworkIsConnectedToNpPresence);
	SCR_REGISTER_SECURE(NETWORK_IS_LOGGED_IN_TO_PSN,0x16c2ac4b4740271f, CommandNetworkIsLoggedInToPsn);
	SCR_REGISTER_UNUSED(NETWORK_RECHECK_NP_AVAILABILITY,0x51aa8a489b77bc29, CommandNetworkRecheckNpAvailability);
	SCR_REGISTER_SECURE(NETWORK_HAS_VALID_ROS_CREDENTIALS,0x5b66e9fb7530bb95, CommandNetworkHasValidRosCredentials);
	SCR_REGISTER_SECURE(NETWORK_IS_REFRESHING_ROS_CREDENTIALS,0xafb993c55538f6d3, CommandNetworkIsRefreshingRosCredentials);
	SCR_REGISTER_SECURE(NETWORK_IS_CLOUD_AVAILABLE,0x47c86377aa337cbe, CommandNetworkIsCloudAvailable);
	SCR_REGISTER_SECURE(NETWORK_HAS_SOCIAL_CLUB_ACCOUNT,0xca8cdfe6f0f62c7f, CommandNetworkHasSocialClubAccount);
	SCR_REGISTER_SECURE(NETWORK_ARE_SOCIAL_CLUB_POLICIES_CURRENT,0x0f427066bf044759, CommandNetworkAreSocialClubPoliciesCurrent);
	SCR_REGISTER_UNUSED(NETWORK_LAN_SESSIONS_ONLY,0x3f65cb91b9932dde, CommandNetworkLanSessionsOnly);
	SCR_REGISTER_SECURE(NETWORK_IS_HOST,0x36de74c9549aaef2, CommandNetworkIsHost);
	SCR_REGISTER_SECURE(NETWORK_GET_HOST_PLAYER_INDEX,0x8251fb94dc4fdfc8, CommandNetworkGetHostPlayerIndex);
	SCR_REGISTER_SECURE(NETWORK_WAS_GAME_SUSPENDED,0x7de1a1f794241a4f, CommandNetworkWasGameSuspended);
	SCR_REGISTER_UNUSED(NETWORK_IS_REPUTATION_BAD,0x5a6b549b65c8f0b8, CommandIsReputationBad);

	// privilege checks
	SCR_REGISTER_SECURE(NETWORK_HAVE_ONLINE_PRIVILEGES,0x17c7c36f1fce1ac2, CommandHaveOnlinePrivileges);
	SCR_REGISTER_SECURE(NETWORK_HAS_AGE_RESTRICTIONS,0x74a0cf38086bfa4d, CommandHasAgeRestrictions);
	SCR_REGISTER_SECURE(NETWORK_HAVE_USER_CONTENT_PRIVILEGES,0x818f829545200020, CommandHaveUserContentPrivileges);
	SCR_REGISTER_SECURE(NETWORK_HAVE_COMMUNICATION_PRIVILEGES,0x1a1b1f5dc3f2d868, CommandHaveCommunicationPrivileges);
    
    SCR_REGISTER_SECURE(NETWORK_CHECK_ONLINE_PRIVILEGES,0x3554df15fcdf45a1, CommandCheckOnlinePrivileges);
    SCR_REGISTER_SECURE(NETWORK_CHECK_USER_CONTENT_PRIVILEGES,0x3ee42535a14ba719, CommandCheckUserContentPrivileges);
    SCR_REGISTER_SECURE(NETWORK_CHECK_COMMUNICATION_PRIVILEGES,0x4b719b4a8760b77f, CommandCheckCommunicationPrivileges);
	SCR_REGISTER_SECURE(NETWORK_CHECK_TEXT_COMMUNICATION_PRIVILEGES,0x001b5de156ecd0d5, CommandCheckTextCommunicationPrivileges);

    SCR_REGISTER_SECURE(NETWORK_IS_USING_ONLINE_PROMOTION,0x6b11a0822298fc28, CommandIsUsingOnlinePromotion);
    SCR_REGISTER_SECURE(NETWORK_SHOULD_SHOW_PROMOTION_ALERT_SCREEN,0x57cbb223ba695c48, CommandShouldShowPromotionAlertScreen);

	SCR_REGISTER_SECURE(NETWORK_HAS_SOCIAL_NETWORKING_SHARING_PRIV,0x07a5e5231705659c, CommandHasSocialNetworkingSharingPriv);
	SCR_REGISTER_SECURE(NETWORK_GET_AGE_GROUP,0xf715b6db99db6acc, CommandGetAgeGroup);

	SCR_REGISTER_SECURE(NETWORK_CHECK_PRIVILEGES,0x9b3cc23e5574e8d3, CommandCheckPrivileges);
	SCR_REGISTER_UNUSED(NETWORK_IS_PRIVILEGE_CHECK_RESULT_READY,0x18f114a8b87d7fb1, CommandIsPrivilegeCheckResultReady);
	SCR_REGISTER_SECURE(NETWORK_IS_PRIVILEGE_CHECK_IN_PROGRESS,0x9290519142d56dc3, CommandIsPrivilegeCheckInProgress);
	SCR_REGISTER_UNUSED(NETWORK_IS_PRIVILEGE_CHECK_SUCCESSFUL,0xb52abf2eca045c09, CommandIsPrivilegeCheckSuccessful);
	SCR_REGISTER_SECURE(NETWORK_SET_PRIVILEGE_CHECK_RESULT_NOT_NEEDED,0xfbdf4a6e38fe1d85, CommandSetPrivilegeCheckResultNotNeeded);

	// Xbox only privilege resolution (will show appropriate platform UI)
	SCR_REGISTER_UNUSED(NETWORK_RESOLVE_PRIVILEGE_ONLINE_MULTIPLAYER, 0xc4a318a08181f8c3, CommandNetworkResolvePrivilegeOnlineMultiplayer);
	SCR_REGISTER_SECURE(NETWORK_RESOLVE_PRIVILEGE_USER_CONTENT, 0xde9225854f37bf72, CommandNetworkResolvePrivilegeUserContent);

	// account checks
	SCR_REGISTER_SECURE(NETWORK_HAVE_PLATFORM_SUBSCRIPTION,0x181069b04c4883bd, CommandHavePlatformSubscription);
	SCR_REGISTER_SECURE(NETWORK_IS_PLATFORM_SUBSCRIPTION_CHECK_PENDING,0x6c8e92d4f56b150e, CommandIsPlatformSubscriptionCheckPending);
	SCR_REGISTER_SECURE(NETWORK_SHOW_ACCOUNT_UPGRADE_UI, 0x47026c4989c92f91, CommandShowAccountUpgradeUI);
	SCR_REGISTER_SECURE(NETWORK_IS_SHOWING_SYSTEM_UI_OR_RECENTLY_REQUESTED_UPSELL, 0x7788dfe15016a182, CommandIsShowingSystemUiOrRecentlyRequestedUpsell);

	SCR_REGISTER_SECURE(NETWORK_NEED_TO_START_NEW_GAME_BUT_BLOCKED,0xd5cc151c7c897002, CommandNetworkNeedToStartNewGameButBlocked);

	// bail 
	SCR_REGISTER_SECURE(NETWORK_CAN_BAIL,0x08d888a13c7eae9a, CommandNetworkCanBail);
	SCR_REGISTER_SECURE(NETWORK_BAIL,0x6c5c36a8b5b604a8, CommandNetworkBail);
    SCR_REGISTER_SECURE(NETWORK_ON_RETURN_TO_SINGLE_PLAYER,0xf1cb00675e759704, CommandNetworkOnReturnToSinglePlayer);

	// matchmaking filter values
	SCR_REGISTER_UNUSED(NETWORK_SET_MATCHMAKING_VALUE,0xa34ac257f36126af, CommandNetworkSetMatchmakingValue);
	SCR_REGISTER_UNUSED(NETWORK_CLEAR_MATCHMAKING_VALUE,0xb968149c3e9e32b6, CommandNetworkClearMatchmakingValue);

    SCR_REGISTER_UNUSED(NETWORK_SET_MATCHMAKING_QUERY_STATE,0x8cd957d92c062b66, CommandNetworkSetMatchmakingQueryState);
    SCR_REGISTER_UNUSED(NETWORK_GET_MATCHMAKING_QUERY_STATE,0x5f5bac02f79cd284, CommandNetworkGetMatchmakingQueryState);

	// session actions
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_START,0x28f4de9a81bee3ce, CommandNetworkTransitionStart);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_ADD_STAGE,0x16aeee39d8ef7e7d, CommandNetworkTransitionAddStage);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_FINISH,0x166eeb852973b1db, CommandNetworkTransitionFinish);

	// session actions
	SCR_REGISTER_SECURE(NETWORK_CAN_ACCESS_MULTIPLAYER,0xbe80a802dc72edc6, CommandNetworkCanAccessMultiplayer);
	SCR_REGISTER_UNUSED(NETWORK_CHECK_CAN_ACCESS_AND_ALERT, 0x5d3131436f409031, CommandNetworkCheckNetworkAccessAndAlertIfFail);
    SCR_REGISTER_SECURE(NETWORK_IS_MULTIPLAYER_DISABLED,0x7c92ff03126ca051, CommandNetworkIsMultiplayerDisabled);
	SCR_REGISTER_SECURE(NETWORK_CAN_ENTER_MULTIPLAYER,0x81a7d34a619b9771, CommandNetworkCanEnterMultiplayer);
	SCR_REGISTER_SECURE(NETWORK_SESSION_DO_FREEROAM_QUICKMATCH,0x551ae6e8f7f29f4e, CommandNetworkDoFreeroamQuickmatch);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_ENTER,0x197a91fa3efa1bf6, CommandNetworkQuickmatch);
	SCR_REGISTER_SECURE(NETWORK_SESSION_DO_FRIEND_MATCHMAKING,0x20a4f0120b408c3e, CommandNetworkDoFriendMatchmaking);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_FRIEND_MATCHMAKING,0x416db490205bdf23, CommandNetworkFriendMatchmaking);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_DO_SOCIAL_MATCHMAKING,0x2db15da730179841, CommandNetworkDoSocialMatchmaking);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_SOCIAL_MATCHMAKING,0x390b4fc29cb44876, CommandNetworkSocialMatchmaking);
	SCR_REGISTER_SECURE(NETWORK_SESSION_DO_CREW_MATCHMAKING,0xf89241033fd1be74, CommandNetworkDoCrewMatchmaking);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_CREW_MATCHMAKING,0xf30fea2284e69892, CommandNetworkCrewMatchmaking);
	SCR_REGISTER_SECURE(NETWORK_SESSION_DO_ACTIVITY_QUICKMATCH,0xfc6d3544b7537f63, CommandNetworkDoActivityQuickmatch);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_ACTIVITY_QUICKMATCH,0xbcd7e4351813fd9b, CommandNetworkActivityQuickmatch);
	SCR_REGISTER_SECURE(NETWORK_SESSION_HOST,0x4d16964e44441fde, CommandNetworkHostSession);
    SCR_REGISTER_SECURE(NETWORK_SESSION_HOST_CLOSED,0x4bb69e9e1979380a, CommandNetworkHostClosedSession);
    SCR_REGISTER_SECURE(NETWORK_SESSION_HOST_FRIENDS_ONLY,0x9ef3443c4282410b, CommandNetworkHostClosedFriendSession);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_HOST_CLOSED_CREW,0x78b67fdcd615ae9b, CommandNetworkHostClosedCrewSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_CLOSED_FRIENDS,0x475bc8e76eb32bcf, CommandNetworkIsClosedFriendsSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_CLOSED_CREW,0x71be58f878f44990, CommandNetworkIsClosedCrewSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_SOLO,0x759d0daf85d68e42, CommandNetworkIsSoloSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_PRIVATE,0xae3d31b16ca16e36, CommandNetworkIsPrivateSession);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_START,0x641bd154dc8ae58b, CommandNetworkStartSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_END,0xf6acec1a6287c705, CommandNetworkEndSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_LEAVE,0x50b2810d3b7843fe, CommandNetworkLeaveSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_KICK_PLAYER,0x17e8dd474e0627a5, CommandNetworkKickPlayer);
	SCR_REGISTER_SECURE(NETWORK_SESSION_GET_KICK_VOTE,0xd2fd411ff335d42c, CommandNetworkGetKickVote);

	SCR_REGISTER_UNUSED(NETWORK_SESSION_RESERVE_SLOTS,0x5d99ab2129dfe78d, CommandNetworkReserveSlots);
	SCR_REGISTER_SECURE(NETWORK_SESSION_RESERVE_SLOTS_TRANSITION,0x88da4f663b1c2d66, CommandNetworkReserveSlotsTransition);

	SCR_REGISTER_UNUSED(NETWORK_RESERVE_SLOT_AND_LEAVE_SESSION,0x8aceba35df3d6911, CommandNetworkReserveSlotAndLeaveSession);
	SCR_REGISTER_UNUSED(NETWORK_JOIN_PREVIOUS_SESSION,0x7ae1e3e3f453cd19, CommandNetworkJoinPreviousSession);
	SCR_REGISTER_SECURE(NETWORK_JOIN_PREVIOUSLY_FAILED_SESSION,0x63cda6a59e3d4d76, CommandNetworkJoinPreviouslyFailedSession);
	SCR_REGISTER_SECURE(NETWORK_JOIN_PREVIOUSLY_FAILED_TRANSITION,0x56edc48b6f7744cc, CommandNetworkJoinPreviouslyFailedTransition);

	// session matchmaking groups
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_MATCHMAKING_GROUP,0xe9d48fd08fce8baf, CommandNetworkSetMatchmakingGroup);
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_MATCHMAKING_GROUP_MAX,0x8f670fe2c81e0851, CommandNetworkSetMatchmakingGroupMax);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_MATCHMAKING_GROUP_MAX,0xef89522b3dff3d92, CommandNetworkGetMatchmakingGroupMax);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_MATCHMAKING_GROUP_NUM,0x1d3a9feb9db50aea, CommandNetworkGetMatchmakingGroupNum);
    SCR_REGISTER_SECURE(NETWORK_SESSION_GET_MATCHMAKING_GROUP_FREE,0x22d50123305d6613, CommandNetworkGetMatchmakingGroupFree);
	SCR_REGISTER_SECURE(NETWORK_SESSION_ADD_ACTIVE_MATCHMAKING_GROUP,0xf731eb28bc1b34be, CommandNetworkAddActiveMatchmakingGroup);

	// crew limits
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_UNIQUE_CREW_LIMIT,0x10967a26ceb155b4, CommandNetworkSetUniqueCrewLimit);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_UNIQUE_CREW_ONLY_CREWS,0xbe1e75a620d9430a, CommandNetworkSetUniqueCrewOnlyCrews);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_CREW_LIMIT_MAX_MEMBERS,0x32574b3805251471, CommandNetworkSetCrewLimitMaxMembers);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_UNIQUE_CREW_LIMIT,0xbf6e0b8f14d3ab7a, CommandNetworkGetUniqueCrewLimit);
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_UNIQUE_CREW_LIMIT_TRANSITION,0xfb425d8f7fc7c2d8, CommandNetworkSetUniqueCrewLimitTransition);
    SCR_REGISTER_SECURE(NETWORK_SESSION_SET_UNIQUE_CREW_ONLY_CREWS_TRANSITION,0x4561114f1bf3495e, CommandNetworkSetUniqueCrewOnlyCrewsTransition);
    SCR_REGISTER_SECURE(NETWORK_SESSION_SET_CREW_LIMIT_MAX_MEMBERS_TRANSITION,0x11c17c15b0ef5b54, CommandNetworkSetCrewLimitMaxMembersTransition);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_UNIQUE_CREW_LIMIT_TRANSITION,0x16980cef266b59aa, CommandNetworkGetUniqueCrewLimitTransition);

	// matchmaking parameters
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_MATCHMAKING_PROPERTY_ID,0x064ae0e51db91f84, CommandNetworkSetMatchmakingPropertyID);
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_MATCHMAKING_MENTAL_STATE,0x1f89cf2c5d8c3b0e, CommandNetworkSetMatchmakingMentalState);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_MATCHMAKING_ELO,0x5700a64841dc762a, CommandNetworkSetMatchmakingELO);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_ENTER_MATCHMAKING_AS_BOSS,0x904006f2f56124be, CommandNetworkSetEnterMatchmakingAsBoss);
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_NUM_BOSSES,0xe74f9b0896fa985c, CommandNetworkSetNumBosses);

	// script validation of join action
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_SCRIPT_VALIDATE_JOIN,0x72a439f08c4282ae, CommandNetworkSetScriptValidateJoin);
	SCR_REGISTER_SECURE(NETWORK_SESSION_VALIDATE_JOIN,0x165391fd0245d8d8, CommandNetworkValidateJoin);

    // followers
    SCR_REGISTER_SECURE(NETWORK_ADD_FOLLOWERS,0x07c6df94e0485131, CommandNetworkAddFollowers);
    SCR_REGISTER_UNUSED(NETWORK_REMOVE_FOLLOWERS,0xee334359153b89f8, CommandNetworkRemoveFollowers);
    SCR_REGISTER_UNUSED(NETWORK_HAS_FOLLOWER,0xa984a8f964c5fe82, CommandNetworkHasFollower);
    SCR_REGISTER_SECURE(NETWORK_CLEAR_FOLLOWERS,0x539d32a62a91eda2, CommandNetworkClearFollowers);
    SCR_REGISTER_UNUSED(NETWORK_RETAIN_FOLLOWERS,0x1ed77373d489a89c, CommandNetworkRetainFollowers);

	// global clock and weather
	SCR_REGISTER_UNUSED(NETWORK_SET_SCRIPT_HANDLING_CLOCK_AND_WEATHER,0x8a785147c2a01aea, CommandNetworkSetScriptHandlingClockAndWeather);
	SCR_REGISTER_UNUSED(NETWORK_APPLY_GLOBAL_CLOCK_AND_WEATHER,0x0c8a0dbf6f4666fc, CommandNetworkApplyClockAndWeather);
	SCR_REGISTER_SECURE(NETWORK_GET_GLOBAL_MULTIPLAYER_CLOCK,0x444ba2b7a6430595, CommandNetworkGetGlobalMultiplayerClock);
	SCR_REGISTER_UNUSED(NETWORK_GET_GLOBAL_MULTIPLAYER_WEATHER,0xb26f0a320fc4396c, CommandNetworkGetGlobalMultiplayerWeather);

	// lobby settings
	SCR_REGISTER_SECURE(NETWORK_SESSION_SET_GAMEMODE,0x2f3b1c402bbcd490, CommandNetworkSessionSetGameMode);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_GAMEMODE_STATE,0xb35d46c49fe00b61, CommandNetworkSessionSetGameModeState);
    SCR_REGISTER_SECURE(NETWORK_SESSION_GET_HOST_AIM_PREFERENCE,0x1447e546142312de, CommandNetworkSessionGetHostAimPreference);
    SCR_REGISTER_UNUSED(NETWORK_SESSION_AIM_PREFERENCE_AS_MATCHMAKING,0x7c3cf92d9cdd6c3f, CommandNetworkSessionGetAimPreferenceAsMatchmaking);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_AIM_BUCKETING_ENABLED,0xf9527bb58885c6e6, CommandNetworkSessionSetAimBucketingEnabled);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_SET_AIM_BUCKETING_ENABLED_IN_ARCADE_GAME_MODES,0xb41bd4275b68f437, CommandNetworkSessionSetAimBucketingEnabledForArcadeGameModes);

	// find gamers
	SCR_REGISTER_UNUSED(NETWORK_FIND_GAMERS,0x547c9aaa7abae8ab, CommandNetworkFindGamers);
	SCR_REGISTER_SECURE(NETWORK_FIND_GAMERS_IN_CREW,0xeb25c2d9fb1a598c, CommandNetworkFindGamersInCrew);
	SCR_REGISTER_SECURE(NETWORK_FIND_MATCHED_GAMERS,0xe7477a13549a68b7, CommandNetworkFindMatchedGamers);
	SCR_REGISTER_SECURE(NETWORK_IS_FINDING_GAMERS,0x2e0788b843154db4, CommandNetworkIsFindingGamers);
	SCR_REGISTER_SECURE(NETWORK_DID_FIND_GAMERS_SUCCEED,0xe28005259a662790, CommandNetworkDidFindGamersSucceed);
	SCR_REGISTER_SECURE(NETWORK_GET_NUM_FOUND_GAMERS,0x40aca0ebe122266f, CommandNetworkGetNumFoundGamers);
	SCR_REGISTER_SECURE(NETWORK_GET_FOUND_GAMER,0xd5a75a022ba7e2c9, CommandNetworkGetFoundGamer);
	SCR_REGISTER_UNUSED(NETWORK_GET_FOUND_GAMERS,0x02c58d2f3aa5c013, CommandNetworkGetFoundGamers);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_FOUND_GAMERS,0xba60cc16ced5216f, CommandNetworkClearFoundGamers);

	// get gamer status
	SCR_REGISTER_UNUSED(NETWORK_GET_GAMER_STATUS,0x8a0da3b9add776e1, CommandNetworkGetGamerStatus);
	SCR_REGISTER_SECURE(NETWORK_QUEUE_GAMER_FOR_STATUS,0xc08fbee09cad00b2, CommandNetworkQueueGamerForStatus);
	SCR_REGISTER_SECURE(NETWORK_GET_GAMER_STATUS_FROM_QUEUE,0x80e814e6e16ea989, CommandNetworkGetGamerStatusFromQueue);
	SCR_REGISTER_SECURE(NETWORK_IS_GETTING_GAMER_STATUS,0x4fa16ea51240f436, CommandNetworkIsGettingGamerStatus);
	SCR_REGISTER_SECURE(NETWORK_DID_GET_GAMER_STATUS_SUCCEED,0x50ad211cb2a0913c, CommandNetworkDidGetGamerStatusSucceed);
	SCR_REGISTER_SECURE(NETWORK_GET_GAMER_STATUS_RESULT,0x983e95d315dda7a2, CommandNetworkGetGamerStatusResult);
	SCR_REGISTER_UNUSED(NETWORK_GET_GAMER_STATUS_RESULTS,0x333f26e281eee7ca, CommandNetworkGetGamerStatusResults);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_GET_GAMER_STATUS,0xd80e1a7801342311, CommandNetworkClearGetGamerStatus);

	// invite actions
	SCR_REGISTER_SECURE(NETWORK_SESSION_JOIN_INVITE,0x1ef092ff526f5bb1, CommandNetworkSessionJoinInvite);
	SCR_REGISTER_SECURE(NETWORK_SESSION_CANCEL_INVITE,0xcd428921d8c5716f, CommandNetworkSessionClearInvite);
    SCR_REGISTER_SECURE(NETWORK_SESSION_FORCE_CANCEL_INVITE,0xaa4b996ed190284d, CommandNetworkSessionCancelInvite);
    SCR_REGISTER_SECURE(NETWORK_HAS_PENDING_INVITE,0x037acda09b3dde99, CommandNetworkHasPendingInvite);
    SCR_REGISTER_UNUSED(NETWORK_GET_PENDING_INVITER,0xcc6720dcc4e1b598, CommandNetworkGetPendingInviter);
    SCR_REGISTER_SECURE(NETWORK_HAS_CONFIRMED_INVITE,0xc47bd4b810c44f4d, CommandNetworkHasConfirmedInvite);
    SCR_REGISTER_SECURE(NETWORK_REQUEST_INVITE_CONFIRMED_EVENT,0x6158057329afeffe, CommandNetworkRequestInviteConfirmedEvent);
    SCR_REGISTER_SECURE(NETWORK_SESSION_WAS_INVITED,0xe103dea13a894e9c, CommandNetworkSessionWasInvited);
	SCR_REGISTER_SECURE(NETWORK_SESSION_GET_INVITER,0x6459e246cc88f414, CommandNetworkSessionGetInviter);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_AWAITING_INVITE_RESPONSE,0x11c5877ec40676f2, CommandNetworkIsAwaitingInviteResponse);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_DISPLAYING_INVITE_CONFIRMATION,0x22052c08a1e9a10c, CommandNetworkIsDisplayingInviteConfirmation);
	SCR_REGISTER_SECURE(NETWORK_SUPPRESS_INVITE,0xee3aced204f48810, CommandNetworkSuppressInvites);
    SCR_REGISTER_SECURE(NETWORK_BLOCK_INVITES,0x494b84826e4a775e, CommandNetworkBlockInvites);
    SCR_REGISTER_SECURE(NETWORK_BLOCK_JOIN_QUEUE_INVITES,0x7a4832d97838593a, CommandNetworkBlockJoinQueueInvites);
	SCR_REGISTER_SECURE(NETWORK_SET_CAN_RECEIVE_RS_INVITES, 0x68980414688f7f9d, CommandNetworkSetCanReceiveRsInvites);
    SCR_REGISTER_SECURE(NETWORK_STORE_INVITE_THROUGH_RESTART,0xd225981938e1b6e2, CommandNetworkStoreInviteThroughRestart);
    SCR_REGISTER_SECURE(NETWORK_ALLOW_INVITE_PROCESS_IN_PLAYER_SWITCH,0xf5a922e33c0f5c5f, CommandNetworkAllowInviteProcessInPlayerSwitch);
    SCR_REGISTER_SECURE(NETWORK_SET_SCRIPT_READY_FOR_EVENTS,0x172705bf7bd84016, CommandNetworkSetScriptReadyForEvents);
    SCR_REGISTER_SECURE(NETWORK_IS_OFFLINE_INVITE_PENDING,0xe479c7aee6c63853, CommandNetworkIsOfflineInvitePending);
    SCR_REGISTER_SECURE(NETWORK_CLEAR_OFFLINE_INVITE_PENDING,0x3a4820320cc5c2cf, CommandNetworkClearOfflineInvitePending);

	// single player sessions
	SCR_REGISTER_SECURE(NETWORK_SESSION_HOST_SINGLE_PLAYER,0x801d341952c9af98, CommandNetworkHostSinglePlayerSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_LEAVE_SINGLE_PLAYER,0xee912a00d41f4c46, CommandNetworkLeaveSinglePlayerSession);

	// state queries
	SCR_REGISTER_SECURE(NETWORK_IS_GAME_IN_PROGRESS,0x9315dbf7d972f07a, CommandNetworkIsGameInProgress);
	SCR_REGISTER_SECURE(NETWORK_IS_SESSION_ACTIVE,0x71880ce67ee81f91, CommandNetworkIsSessionActive);
	SCR_REGISTER_SECURE(NETWORK_IS_IN_SESSION,0xdb51af48abe62d4d, CommandNetworkIsInSession);
	SCR_REGISTER_UNUSED(NETWORK_IS_SESSION_IN_LOBBY,0xdbe4fb0f2dd08002, CommandNetworkIsSessionInLobby);
	SCR_REGISTER_SECURE(NETWORK_IS_SESSION_STARTED,0x9a404ee97d1ef300, CommandNetworkIsSessionStarted);
	SCR_REGISTER_SECURE(NETWORK_IS_SESSION_BUSY,0xe72acfddb5ccc601, CommandNetworkIsSessionBusy);
	SCR_REGISTER_UNUSED(NETWORK_CAN_SESSION_START,0x7d18136d29a2ea22, CommandNetworkCanSessionStart);
	SCR_REGISTER_SECURE(NETWORK_CAN_SESSION_END,0x60b8249e57ce7597, CommandNetworkCanSessionEnd);
	SCR_REGISTER_UNUSED(NETWORK_IS_UNLOCKED,0x9563252d97dff191, CommandNetworkIsUnlocked);

	// session query functions
	SCR_REGISTER_SECURE(NETWORK_GET_GAME_MODE,0x0195e751969c01b7, CommandNetworkGetGameMode);
	SCR_REGISTER_SECURE(NETWORK_SESSION_MARK_VISIBLE,0x8f59f9682600ad99, CommandNetworkMarkSessionVisible);
    SCR_REGISTER_SECURE(NETWORK_SESSION_IS_VISIBLE,0x0412b2e191de23cd, CommandNetworkIsSessionVisible);
    SCR_REGISTER_UNUSED(NETWORK_SET_SESSION_VISIBILITY_LOCK,0x87a3511ed11fd087, CommandNetworkSetSessionVisibilityLock);
    SCR_REGISTER_UNUSED(NETWORK_IS_SESSION_VISIBILITY_LOCKED,0x479d1c83409db6f8, CommandNetworkIsSessionVisibilityLocked);
    SCR_REGISTER_SECURE(NETWORK_SESSION_BLOCK_JOIN_REQUESTS,0xe774eb358f283b99, CommandNetworkMarkSessionBlockJoinRequests);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_IS_BLOCKING_JOIN_REQUESTS,0xcf83001bc6ace38e, CommandNetworkIsSessionBlockingJoinRequests);
	SCR_REGISTER_SECURE(NETWORK_SESSION_CHANGE_SLOTS,0x5741bea5adf3bcec, CommandNetworkChangeSessionSlots);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_SLOTS,0xf45ccab7f316d6ce, CommandNetworkGetMaxSlots);
	SCR_REGISTER_SECURE(NETWORK_SESSION_GET_PRIVATE_SLOTS,0xf0cf1e6013de39ca, CommandNetworkGetMaxPrivateSlots);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_IS_INVITABLE,0xd5ab84616a1fdd46, CommandNetworkIsSessionInvitable);

	// voice session functions
	SCR_REGISTER_SECURE(NETWORK_SESSION_VOICE_HOST,0x905b0bf2f73efae6, CommandNetworkVoiceHostSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_VOICE_LEAVE,0x39fc9dedbef0b2ce, CommandNetworkVoiceLeaveSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_VOICE_CONNECT_TO_PLAYER,0x1532f8d9de88d9ac, CommandNetworkVoiceConnectToPlayer);
	SCR_REGISTER_SECURE(NETWORK_SESSION_VOICE_RESPOND_TO_REQUEST,0x5aa72b892713abc8, CommandNetworkVoiceRespondToRequest);
	SCR_REGISTER_SECURE(NETWORK_SESSION_VOICE_SET_TIMEOUT,0xe23fe31c53b11070, CommandNetworkSetVoiceSessionTimeout);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_IN_VOICE_SESSION,0xee03516f1f1a249b, CommandNetworkIsInVoiceSession);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_VOICE_SESSION_ACTIVE,0xc9c5c792162fc9e3, CommandNetworkIsVoiceSessionActive);
	SCR_REGISTER_SECURE(NETWORK_SESSION_IS_VOICE_SESSION_BUSY,0x8d9b0ad025e894ad, CommandNetworkIsVoiceSessionBusy);

    // text message
    SCR_REGISTER_SECURE(NETWORK_SEND_TEXT_MESSAGE,0xde0e57421b7105aa, CommandNetworkSendTextMessage);

    // activity spectator
    SCR_REGISTER_SECURE(NETWORK_SET_ACTIVITY_SPECTATOR,0xdf0dd66cb9bf5605, CommandSetActivitySpectator);
    SCR_REGISTER_SECURE(NETWORK_IS_ACTIVITY_SPECTATOR,0x1c564a7701da1a97, CommandIsActivitySpectator);
	SCR_REGISTER_SECURE(NETWORK_SET_ACTIVITY_PLAYER_MAX,0x6af9c3c7f4e5a1ae, CommandSetActivityPlayerMax);
    SCR_REGISTER_SECURE(NETWORK_SET_ACTIVITY_SPECTATOR_MAX,0x59b41141da370417, CommandSetActivitySpectatorMax);
    SCR_REGISTER_UNUSED(NETWORK_GET_ACTIVITY_PLAYER_MAX,0xc556d9e06bf62917, CommandGetActivityPlayerMax);
    SCR_REGISTER_SECURE(NETWORK_GET_ACTIVITY_PLAYER_NUM,0xb196205f363c4679, CommandGetActivityPlayerNum);
    SCR_REGISTER_SECURE(NETWORK_IS_ACTIVITY_SPECTATOR_FROM_HANDLE,0xa3c3653d4d7611b3, CommandIsActivitySpectatorFromHandle);

	// transition session functions
	SCR_REGISTER_SECURE(NETWORK_HOST_TRANSITION,0x1398d0f9f78f82ec, CommandNetworkHostTransition);
	SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_QUICKMATCH,0xd6f501e6218978f9, CommandNetworkDoTransitionQuickmatch);
	SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_QUICKMATCH_ASYNC,0x6bb7ac70c80fd843, CommandNetworkDoTransitionQuickmatchAsync);
	SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_QUICKMATCH_WITH_GROUP,0x78be45180a6021f8, CommandNetworkDoTransitionQuickmatchWithGroup);
	SCR_REGISTER_SECURE(NETWORK_JOIN_GROUP_ACTIVITY,0xbc3651324870cfe8, CommandNetworkDoJoinGroupActivity);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_GROUP_ACTIVITY,0x9d91ff2076e04630, CommandNetworkDoClearGroupActivity);
	SCR_REGISTER_SECURE(NETWORK_RETAIN_ACTIVITY_GROUP,0x12f1be6efbbf2922, CommandNetworkRetainActivityGroup);

	SCR_REGISTER_UNUSED(NETWORK_HOST_TRANSITION_FRIENDS_ONLY,0xbf753df78efd316e, CommandNetworkHostClosedFriendTransition);
	SCR_REGISTER_UNUSED(NETWORK_HOST_TRANSITION_CLOSED_CREW,0xf3fd646567a12af5, CommandNetworkHostClosedCrewTransition);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_CLOSED_FRIENDS,0x8a29d9dfe80cdf74, CommandNetworkIsClosedFriendsTransition);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_CLOSED_CREW,0xa4fa87f337733327, CommandNetworkIsClosedCrewTransition);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_SOLO,0xf1fe0daefbbd12a8, CommandNetworkIsSoloTransition);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_PRIVATE,0xd6c85d0beb8056cb, CommandNetworkIsPrivateTransition);
	SCR_REGISTER_SECURE(NETWORK_GET_NUM_TRANSITION_NON_ASYNC_GAMERS,0xb5f539de635abecd, CommandNetworkGetNumTransitionNonAsyncGamers);

	SCR_REGISTER_SECURE(NETWORK_MARK_AS_PREFERRED_ACTIVITY,0x2260ad62144304a0, CommandNetworkMarkAsPreferredActivity);
	SCR_REGISTER_SECURE(NETWORK_MARK_AS_WAITING_ASYNC,0xff425aace63d7e9b, CommandNetworkMarkAsWaitingAsync);
	SCR_REGISTER_SECURE(NETWORK_SET_IN_PROGRESS_FINISH_TIME,0x2af716848d922171, CommandNetworkSetInPogressFinishTime);
	SCR_REGISTER_SECURE(NETWORK_SET_TRANSITION_CREATOR_HANDLE,0x524a78491eb1818c, CommandNetworkSetActivityCreatorHandle);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_TRANSITION_CREATOR_HANDLE,0x0904a23cb7a5daad, CommandNetworkClearActivityCreatorHandle);
	
    SCR_REGISTER_UNUSED(NETWORK_INVITE_GAMER_TO_TRANSITION,0x446438906b8f5c41, CommandNetworkInviteGamerToTransition);
	SCR_REGISTER_SECURE(NETWORK_INVITE_GAMERS_TO_TRANSITION,0x288ceca35eaa6050, CommandNetworkInviteGamersToTransition);
	SCR_REGISTER_SECURE(NETWORK_SET_GAMER_INVITED_TO_TRANSITION,0xd8a2ba1ede0272df, CommandNetworkSetGamerInvitedToTransition);
	SCR_REGISTER_UNUSED(NETWORK_SET_GAMERS_INVITED_TO_TRANSITION,0x1b2e6c4f03ec0458, CommandNetworkSetGamersInvitedToTransition);
	
	SCR_REGISTER_SECURE(NETWORK_LEAVE_TRANSITION,0x096db41969983f1e, CommandNetworkLeaveTransition);
	SCR_REGISTER_SECURE(NETWORK_LAUNCH_TRANSITION,0xb26408b455a3878d, CommandNetworkLaunchTransition);
	SCR_REGISTER_SECURE(NETWORK_SET_DO_NOT_LAUNCH_FROM_JOIN_AS_MIGRATED_HOST,0xf5a59877f0e087ca, CommandSetDoNotLaunchFromJoinAsMigratedHost);

    SCR_REGISTER_SECURE(NETWORK_CANCEL_TRANSITION_MATCHMAKING, 0x023782efc70585ee, CommandNetworkCancelTransitionMatchmaking);
	SCR_REGISTER_SECURE(NETWORK_BAIL_TRANSITION, 0x6a687bcff5d10f9d, CommandNetworkBailTransition);
	SCR_REGISTER_UNUSED(NETWORK_BAIL_ALL_TRANSITION, 0x609b45df902cedc6, CommandNetworkBailAllTransition);

    // TO_GAME variants to be deprecated
    SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_TO_GAME,0x9b5ddc2207b89984, CommandNetworkDoTransitionToGame);
    SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_TO_NEW_GAME,0x64aab99998b1902f, CommandNetworkDoTransitionToNewGame);
    SCR_REGISTER_UNUSED(NETWORK_CAN_TRANSITION_TO_FREEMODE,0xf688adf28bd0a65f, CommandNetworkCanTransitionToGame);
    SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_TO_FREEMODE,0x696ac5b55dff5bbf, CommandNetworkDoTransitionToFreemode);
    SCR_REGISTER_SECURE(NETWORK_DO_TRANSITION_TO_NEW_FREEMODE,0x81ffc7159bd4d4e3, CommandNetworkDoTransitionToNewFreemode);
	SCR_REGISTER_UNUSED(NETWORK_DO_TRANSITION_FROM_ACTIVITY,0xdf4c7fa21092b7e0, CommandNetworkDoTransitionFromActivity);
	SCR_REGISTER_UNUSED(NETWORK_DO_TRANSITION_FROM_ACTIVITY_TO_NEW_SESSION,0x0c023ec8d2100834, CommandNetworkDoTransitionFromActivityToNewSession);
    
	SCR_REGISTER_UNUSED(NETWORK_IS_TRANSITION_LAUNCHING,0x246be70e9a31cc24, CommandNetworkIsTransitionLaunching);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_TO_GAME,0xac90bfbb6f69ca39, CommandNetworkIsTransitionToGame);

	SCR_REGISTER_SECURE(NETWORK_GET_TRANSITION_MEMBERS,0x34663e18b60160aa, CommandNetworkGetTransitionMembers);
	SCR_REGISTER_SECURE(NETWORK_APPLY_TRANSITION_PARAMETER,0x8a895889ad10b26b, CommandNetworkApplyTransitionParameter);
	SCR_REGISTER_SECURE(NETWORK_APPLY_TRANSITION_PARAMETER_STRING,0x4f10ea896aa65d46, CommandNetworkApplyTransitionParameterString);
	SCR_REGISTER_SECURE(NETWORK_SEND_TRANSITION_GAMER_INSTRUCTION,0x38a527b48c900279, CommandNetworkSendTransitionGamerInstruction);
    SCR_REGISTER_SECURE(NETWORK_MARK_TRANSITION_GAMER_AS_FULLY_JOINED,0xa6c47230497da3cb, CommandNetworkMarkTransitionPlayerAsFullyJoined);

	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_HOST,0x9d371965ec031ac5, CommandNetworkIsTransitionHost);
    SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_HOST_FROM_HANDLE,0x94a61aceae6f60f0, CommandNetworkIsTransitionHostFromHandle);
    SCR_REGISTER_SECURE(NETWORK_GET_TRANSITION_HOST,0x12b6a0b76221d332, CommandNetworkGetTransitionHost);
    SCR_REGISTER_SECURE(NETWORK_IS_IN_TRANSITION,0xb471dbf7b15b22e6, CommandNetworkIsTransitionActive);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_STARTED,0xe140e76445205b6b, CommandNetworkIsTransitionEstablished);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_BUSY,0x74e92db9cf6fb1ba, CommandNetworkIsTransitionBusy);
    SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_MATCHMAKING,0x6d5974b117818e44, CommandNetworkIsTransitionMatchMaking);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_LEAVE_POSTPONED,0xfb1ba1b14ac51416, CommandNetworkIsTransitionLeavePostponed);

	SCR_REGISTER_UNUSED(NETWORK_MAIN_SET_IN_PROGRESS,0x41d51141e7df04d0, CommandNetworkSetMainInProgress);
	SCR_REGISTER_UNUSED(NETWORK_MAIN_GET_IN_PROGRESS,0xe2dbd27f94b98f89, CommandNetworkGetMainInProgress);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_SET_IN_PROGRESS,0x753a9fc0d9cb1c16, CommandNetworkSetActivityInProgress);
	SCR_REGISTER_UNUSED(NETWORK_TRANSITION_GET_IN_PROGRESS,0xda8954415ba7b803, CommandNetworkGetActivityInProgress);
	SCR_REGISTER_UNUSED(NETWORK_MAIN_SET_CONTENT_CREATOR,0xbda7c6d6164f7584, CommandNetworkSetMainContentCreator);
	SCR_REGISTER_UNUSED(NETWORK_MAIN_GET_CONTENT_CREATOR,0x686ebd5c32a2dd44, CommandNetworkGetMainContentCreator);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_SET_CONTENT_CREATOR,0xc9c88e798ec297ea, CommandNetworkSetTransitionContentCreator);
	SCR_REGISTER_UNUSED(NETWORK_TRANSITION_GET_CONTENT_CREATOR,0xec7d73f21d821adc, CommandNetworkGetTransitionContentCreator);
	SCR_REGISTER_UNUSED(NETWORK_MAIN_SET_ACTIVITY_ISLAND,0x3ebef9ecaaf4ec06, CommandNetworkSetMainActivityIsland);
	SCR_REGISTER_UNUSED(NETWORK_MAIN_GET_ACTIVITY_ISLAND,0x24c16f77a772bbb6, CommandNetworkGetMainActivityIsland);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_SET_ACTIVITY_ISLAND,0x1f3fb25f4779d842, CommandNetworkSetTransitionActivityIsland);
	SCR_REGISTER_UNUSED(NETWORK_TRANSITION_GET_ACTIVITY_ISLAND,0xae8b45905dc88aa4, CommandNetworkGetTransitionActivityIsland);

	SCR_REGISTER_SECURE(NETWORK_OPEN_TRANSITION_MATCHMAKING,0x22721e5038087670, CommandNetworkOpenTransitionMatchmaking);
	SCR_REGISTER_SECURE(NETWORK_CLOSE_TRANSITION_MATCHMAKING,0x6c2d37b816bff43c, CommandNetworkCloseTransitionMatchmaking);
	SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_OPEN_TO_MATCHMAKING,0xc2a0f113377f174e, CommandNetworkIsTransitionOpenToMatchmaking);
    SCR_REGISTER_SECURE(NETWORK_SET_TRANSITION_VISIBILITY_LOCK,0xd8265caaeee57a7c, CommandNetworkSetTransitionVisibilityLock);
    SCR_REGISTER_SECURE(NETWORK_IS_TRANSITION_VISIBILITY_LOCKED,0x675f82e042ad90ca, CommandNetworkIsTransitionVisibilityLocked);

	SCR_REGISTER_UNUSED(NETWORK_SET_TRANSITION_ACTIVITY_TYPE,0x7810a95e183db427, CommandNetworkSetTransitionActivityType);
	SCR_REGISTER_SECURE(NETWORK_SET_TRANSITION_ACTIVITY_ID,0xd49688dfd5f33dd7, CommandNetworkSetTransitionActivityID);
	SCR_REGISTER_SECURE(NETWORK_CHANGE_TRANSITION_SLOTS,0x41dd82d37d2eb027, CommandNetworkChangeTransitionSlots);
	SCR_REGISTER_UNUSED(NETWORK_TRANSITION_GET_MAXIMUM_SLOTS,0xc5763f06698a649b, CommandNetworkTransitionGetMaxSlots);
	SCR_REGISTER_SECURE(NETWORK_TRANSITION_BLOCK_JOIN_REQUESTS,0x03d14e87fdd6b011, CommandNetworkMarkTransitionBlockJoinRequests);

	SCR_REGISTER_SECURE(NETWORK_HAS_PLAYER_STARTED_TRANSITION,0x13ed064734cda7c0, CommandNetworkHasPlayerStartedTransition);
	SCR_REGISTER_SECURE(NETWORK_ARE_TRANSITION_DETAILS_VALID,0x3b9ef7886d600649, CommandNetworkAreTransitionDetailsValid);
	SCR_REGISTER_SECURE(NETWORK_JOIN_TRANSITION,0x10fb8f8068d237c8, CommandNetworkJoinTransition);

	SCR_REGISTER_UNUSED(NETWORK_SESSION_WAS_INVITED_TO_TRANSITION,0xd96e67896fd51382, CommandNetworkSessionWasInvitedToTransition);
	SCR_REGISTER_UNUSED(NETWORK_SESSION_GET_TRANSITION_INVITER,0x75ecc51f54cc6ab5, CommandNetworkSessionGetTransitionInviter);
	SCR_REGISTER_SECURE(NETWORK_HAS_INVITED_GAMER_TO_TRANSITION,0x5005d75cd6530e6f, CommandNetworkHasInvitedGamerToTransition);
	SCR_REGISTER_SECURE(NETWORK_HAS_TRANSITION_INVITE_BEEN_ACKED,0xda5dd4a7d1c628ee, CommandNetworkHasTransitionInviteBeenAcked);

	SCR_REGISTER_SECURE(NETWORK_IS_ACTIVITY_SESSION,0x44859561f653dd4e, CommandNetworkIsActivitySession);
	SCR_REGISTER_UNUSED(NETWORK_MARK_AS_GAME_SESSION,0xb17f2105d81d6f0c, CommandNetworkMarkAsGameSession);

	// real-time multiplayer
	SCR_REGISTER_SECURE(NETWORK_DISABLE_REALTIME_MULTIPLAYER, 0x236905c700fdb54d, CommandNetworkDisableRealtimeMultiplayer);
	SCR_REGISTER_UNUSED(NETWORK_OVERRIDE_REALTIME_MULTIPLAYER_CONTROL_DISABLE, 0xfc060b8213e6cad0, CommandNetworkOverrideRealtimeMultiplayerControlDisable);
	SCR_REGISTER_UNUSED(NETWORK_DISABLE_REALTIME_MULTIPLAYER_SPECTATOR, 0x83369a2f617ea53c, CommandNetworkDisableRealtimeMultiplayerSpectator);

    // presence session
    SCR_REGISTER_SECURE(NETWORK_SET_PRESENCE_SESSION_INVITES_BLOCKED,0x28bbfc5fec1e27bd, CommandNetworkSetPresenceSessionInvitesBlocked);
	SCR_REGISTER_UNUSED(NETWORK_ARE_PRESENCE_SESSION_INVITES_BLOCKED,0xf810a364fa49243b, CommandNetworkArePresenceSessionInvitesBlocked);

	// presence invites
	SCR_REGISTER_SECURE(NETWORK_SEND_INVITE_VIA_PRESENCE,0x6e0ae21aa1b44961, CommandNetworkSendPresenceInvite);
	SCR_REGISTER_SECURE(NETWORK_SEND_TRANSITION_INVITE_VIA_PRESENCE,0x7cad6065a407d7b8, CommandNetworkSendPresenceTransitionInvite);
	SCR_REGISTER_SECURE(NETWORK_SEND_IMPORTANT_TRANSITION_INVITE_VIA_PRESENCE,0xa3456fcb353460d9, CommandNetworkSendImportantPresenceTransitionInvite);
	SCR_REGISTER_UNUSED(NETWORK_SEND_ROCKSTAR_INVITE,0x5cc37e929fe8716e, CommandNetworkSendRockstarInvite);

	SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_INDEX_BY_ID,0xb5edcda1860870a0, CommandNetworkGetRsInviteIndexByInviteId);
	SCR_REGISTER_SECURE(NETWORK_GET_NUM_PRESENCE_INVITES,0x46d4f0814b2a2202, CommandNetworkGetNumRsInvites);
	SCR_REGISTER_SECURE(NETWORK_ACCEPT_PRESENCE_INVITE,0xf7c55939e655699d, CommandNetworkAcceptRsInvite);
	SCR_REGISTER_SECURE(NETWORK_REMOVE_PRESENCE_INVITE,0x69c28c0fd93ca75b, CommandNetworkRemoveRsInvite);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_ID,0x8d83f0c9481a0a12, CommandNetworkGetRsInviteId);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_INVITER,0xe94a813f7c1c7f0a, CommandNetworkGetRsInviteInviter);
	SCR_REGISTER_UNUSED(NETWORK_GET_PRESENCE_INVITE_INVITER_CREW_ID,0xca5f944ffc44f5c7, CommandNetworkGetRsInviteInviterCrewId);
	SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_HANDLE,0xde9430f04af534d9, CommandNetworkGetRsInviteHandle);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_SESSION_ID,0x8d67b99a0e382905, CommandNetworkGetRsInviteSessionId);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_CONTENT_ID,0xac494f7ddecff675, CommandNetworkGetRsInviteContentId);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_PLAYLIST_LENGTH,0x0f55e0a218a6fe26, CommandNetworkGetRsInvitePlaylistLength);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_PLAYLIST_CURRENT,0x86e0c5103b2a4869, CommandNetworkGetRsInvitePlaylistCurrent);
    SCR_REGISTER_UNUSED(NETWORK_GET_PRESENCE_INVITE_TOURNAMENT_EVENT_ID,0x7ca7a5bd9742847d, CommandNetworkGetRsInviteTournamentEventId);
    SCR_REGISTER_UNUSED(NETWORK_GET_PRESENCE_INVITE_SCTV,0xedd6f935b246eb10, CommandNetworkGetRsInviteScTv);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_FROM_ADMIN,0x00acf7948f0868e0, CommandNetworkGetRsInviteFromAdmin);
    SCR_REGISTER_SECURE(NETWORK_GET_PRESENCE_INVITE_IS_TOURNAMENT,0x052e8467ac4fe90e, CommandNetworkGetRsInviteTournament);
	
    SCR_REGISTER_SECURE(NETWORK_HAS_FOLLOW_INVITE,0x1e427e2f45d48a7e, CommandNetworkHasFollowInvite);
    SCR_REGISTER_SECURE(NETWORK_ACTION_FOLLOW_INVITE,0x7261fa840a6d9e98, CommandNetworkActionFollowInvite);
    SCR_REGISTER_SECURE(NETWORK_CLEAR_FOLLOW_INVITE,0x79cd75c6002ea349, CommandNetworkClearFollowInvite);
    SCR_REGISTER_UNUSED(NETWORK_GET_FOLLOW_INVITE_HANDLE,0xe1ccfc92aa315798, CommandNetworkGetFollowInviteHandle);

	// cancel invites
	SCR_REGISTER_UNUSED(NETWORK_CANCEL_INVITE,0x76a4e763d35c5d93, CommandNetworkCancelInvite);
	SCR_REGISTER_UNUSED(NETWORK_CANCEL_TRANSITION_INVITE,0xf829d5efb56aceea, CommandNetworkCancelTransitionInvite);

	// remove tracked invites
	SCR_REGISTER_UNUSED(NETWORK_REMOVE_INVITE,0x2eea894110cd1067, CommandNetworkRemoveInvite);
    SCR_REGISTER_UNUSED(NETWORK_REMOVE_ALL_INVITES,0xd6503b2fb63fbcb7, CommandNetworkRemoveAllInvites);
    SCR_REGISTER_SECURE(NETWORK_REMOVE_AND_CANCEL_ALL_INVITES,0x553ed272c0050733, CommandNetworkRemoveAndCancelAllInvites);
    SCR_REGISTER_SECURE(NETWORK_REMOVE_TRANSITION_INVITE,0x1881c7696f08f713, CommandNetworkRemoveTransitionInvite);
	SCR_REGISTER_SECURE(NETWORK_REMOVE_ALL_TRANSITION_INVITE,0xed075e87511bb187, CommandNetworkRemoveAllTransitionInvites);
    SCR_REGISTER_SECURE(NETWORK_REMOVE_AND_CANCEL_ALL_TRANSITION_INVITES,0x2e036ddbb5de7208, CommandNetworkRemoveAndCancelAllTransitionInvites);

	// invite functions
	SCR_REGISTER_UNUSED(NETWORK_INVITE_GAMER,0xc1498cc0128565fd, CommandNetworkInviteGamerToSession);
	SCR_REGISTER_UNUSED(NETWORK_INVITE_GAMER_WITH_MESSAGE,0xf8eae27ec1167c0c, CommandNetworkInviteGamerToSessionWithMessage);
	SCR_REGISTER_SECURE(NETWORK_INVITE_GAMERS,0xb6a14185ecb203a9, CommandNetworkInviteGamersToSession);
	SCR_REGISTER_SECURE(NETWORK_HAS_INVITED_GAMER,0x0f6d55d87b7c7c9a, CommandNetworkHasInvitedGamer);
	SCR_REGISTER_UNUSED(NETWORK_HAS_INVITE_BEEN_ACKED,0x3a34867d08fb1e2d, CommandNetworkHasInviteBeenAcked);
	SCR_REGISTER_SECURE(NETWORK_HAS_MADE_INVITE_DECISION,0x90e13cbc679b343c, CommandNetworkHasMadeInviteDecision);
	SCR_REGISTER_SECURE(NETWORK_GET_INVITE_REPLY_STATUS,0x67492aee9df67af0, CommandNetworkGetInviteReplyStatus);
	SCR_REGISTER_UNUSED(NETWORK_GET_NUM_UNACCEPTED_INVITES,0xc06952fd99fa2e22, CommandNetworkGetNumUnacceptedInvites);
	SCR_REGISTER_UNUSED(NETWORK_GET_UNACCEPTED_INVITER_NAME,0x4c031a6948fb2fc0, CommandNetworkGetUnacceptedInviterName);
	SCR_REGISTER_UNUSED(NETWORK_ACCEPT_INVITE,0x7bf8101bf0b3847f, CommandNetworkAcceptInvite);
	SCR_REGISTER_SECURE(NETWORK_GET_CURRENTLY_SELECTED_GAMER_HANDLE_FROM_INVITE_MENU,0xbfd55d5bc9d77079, CommandNetworkGetCurrentlySelectedGamerHandleFromInviteMenu);
	SCR_REGISTER_SECURE(NETWORK_SET_CURRENTLY_SELECTED_GAMER_HANDLE_FROM_INVITE_MENU,0xb96e3a4feeae5dcf, CommandNetworkSetCurrentlySelectedGamerHandleFromInviteMenu);
	SCR_REGISTER_SECURE(NETWORK_SET_INVITE_ON_CALL_FOR_INVITE_MENU,0x209b41d9e87fc6ee, CommandNetworkSetPlayerToOnCallForJoinedMenu);
	SCR_REGISTER_UNUSED(NETWORK_IS_HOST_PLAYER_AND_ROCKSTAR_DEV,0xa45e5e2d6610849e, CommandsIsHostPlayerAndRockstarDev);
	SCR_REGISTER_UNUSED(NETWORK_CHECK_DATA_MANAGER_SUCCEEDED,0xe77a37165b148bbf, CommandNetworkCheckDataManagerSucceeded);
	SCR_REGISTER_SECURE(NETWORK_CHECK_DATA_MANAGER_SUCCEEDED_FOR_HANDLE,0x63f0da6449e61d29, CommandNetworkCheckDataManagerSucceededForHandle);
	SCR_REGISTER_SECURE(NETWORK_CHECK_DATA_MANAGER_FOR_HANDLE,0x401fdbf938af752e, CommandNetworkCheckDataManagerForHandle);
	SCR_REGISTER_SECURE(NETWORK_SET_INVITE_FAILED_MESSAGE_FOR_INVITE_MENU,0x59a4980963bc6654, CommandNetworkSetInviteFailedMessageForInviteMenu);
	SCR_REGISTER_SECURE(FILLOUT_PM_PLAYER_LIST,0x460505342225cb7a, CommandFillOutPMPlayerList);
	SCR_REGISTER_SECURE(FILLOUT_PM_PLAYER_LIST_WITH_NAMES,0x00164910305c01eb, CommandFillOutPMPlayerListWithNames);
	SCR_REGISTER_SECURE(REFRESH_PLAYER_LIST_STATS,0x222f2993020d5a70, CommandRefreshPlayerListStats);
	SCR_REGISTER_UNUSED(GET_PM_PLAYER_ONLINE_STATUS,0x19a898145777402f, CommandGetPMPlayerOnlineStatus);
	SCR_REGISTER_UNUSED(NETWORK_CHECK_DATA_MANAGER_STATS_QUERY_FAILED,0x87a8f7d522928ed3, CommandNetworkCheckDataManagerStatsQueryFailed);
	SCR_REGISTER_UNUSED(NETWORK_CHECK_DATA_MANAGE_STATS_QUERY_PENDING,0x4187bb6cf5f20edf, CommandNetworkCheckDataManagerStatsQueryPending);
	SCR_REGISTER_SECURE(NETWORK_SET_CURRENT_DATA_MANAGER_HANDLE,0x459926976bb7464c, CommandNetworkSetCurrentGamerHandleForDataManager);

	// platform party functions
	SCR_REGISTER_UNUSED(NETWORK_SHOW_PLATFORM_PARTY_UI,0xeb91f36d6aaf373c, CommandNetworkShowPlatformPartyUI);
	SCR_REGISTER_UNUSED(NETWORK_INVITE_PLATFORM_PARTY,0x0607bb03f7b40e76, CommandNetworkInvitePlatformParty);
	SCR_REGISTER_SECURE(NETWORK_IS_IN_PLATFORM_PARTY,0x9d176461aebc1fde, CommandNetworkIsInPlatformParty);
	SCR_REGISTER_SECURE(NETWORK_GET_PLATFORM_PARTY_MEMBER_COUNT,0x81ba3c34db29bedd, CommandNetworkGetPlatformPartyMemberCount);
	SCR_REGISTER_SECURE(NETWORK_GET_PLATFORM_PARTY_MEMBERS,0x19f5a68cc42dbec5, CommandNetworkGetPlatformPartyMembers);
	SCR_REGISTER_UNUSED(NETWORK_JOIN_PLATFORM_PARTY_MEMBER_SESSION,0xd869a5434d1f9f08, CommandNetworkJoinPlatformPartyMemberSession);
    SCR_REGISTER_SECURE(NETWORK_IS_IN_PLATFORM_PARTY_CHAT,0xa6a33e7a7c82daea, CommandNetworkIsInPlatformPartyChat);
    SCR_REGISTER_SECURE(NETWORK_IS_CHATTING_IN_PLATFORM_PARTY,0x9d06a67d5ed52b09, CommandNetworkIsChattingInPlatformParty);
    SCR_REGISTER_UNUSED(NETWORK_SHOW_PLATFORM_VOICE_CHANNEL_UI,0x64d388c111354a1f, CommandNetworkShowPlatformVoiceChannelUI);

	// session queues
	SCR_REGISTER_SECURE(NETWORK_CAN_QUEUE_FOR_PREVIOUS_SESSION_JOIN,0xce8f932216fafd09, CommandNetworkCanQueueForPreviousSessionJoin);
	SCR_REGISTER_SECURE(NETWORK_IS_QUEUING_FOR_SESSION_JOIN,0x1dc157fb92983f64, CommandNetworkIsQueuingForSessionJoin);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_QUEUED_JOIN_REQUEST,0xdccf1bd61c7a3788, CommandNetworkClearQueuedJoinRequest);
	SCR_REGISTER_SECURE(NETWORK_SEND_QUEUED_JOIN_REQUEST,0x80518e90f5f86ece, CommandNetworkSendQueuedJoinRequest);
	SCR_REGISTER_SECURE(NETWORK_REMOVE_ALL_QUEUED_JOIN_REQUESTS,0x12a88f4f42a23710, CommandNetworkRemoveAllQueuedJoinRequests);

	// random number
	SCR_REGISTER_SECURE(NETWORK_SEED_RANDOM_NUMBER_GENERATOR,0xce651d36859b942e, CommandNetworkSeedRandomNumberGenerator);
	SCR_REGISTER_SECURE(NETWORK_GET_RANDOM_INT,0xf5492d621b7aedeb, CommandNetworkGetRandomInt);
	SCR_REGISTER_SECURE(NETWORK_GET_RANDOM_INT_RANGED,0x21d864aec6d2d96e, CommandNetworkGetRandomIntRanged);

	// cheater detection
	DLC_SCR_REGISTER_SECURE(NETWORK_PLAYER_IS_CHEATER,0xc8eb486898ddbcf0, CommandNetworkIsCheater);
    DLC_SCR_REGISTER_SECURE(NETWORK_PLAYER_GET_CHEATER_REASON,0xc0f485c2828ff715, CommandNetworkGetCheaterReason);
    DLC_SCR_REGISTER_SECURE(NETWORK_PLAYER_IS_BADSPORT,0x515503e8eff18d25, CommandNetworkIsBadSport);
	DLC_SCR_REGISTER_SECURE(TRIGGER_PLAYER_CRC_HACKER_CHECK,0x096a1c82a39ee942, CommandTriggerPlayerCrcHackerCheck);
	DLC_SCR_REGISTER_SECURE(TRIGGER_TUNING_CRC_HACKER_CHECK,0x0f37c55ff7e6fb97, CommandTriggerTuningCrcHackerCheck);
	DLC_SCR_REGISTER_SECURE(TRIGGER_FILE_CRC_HACKER_CHECK,0x4a20c2b73c123028, CommandTriggerFileCrcHackerCheck);
	DLC_SCR_REGISTER_SECURE(REMOTE_CHEATER_PLAYER_DETECTED,0xc3bda394e3a18b01, CommandRemoteCheaterPlayerDetected);
	DLC_SCR_REGISTER_SECURE(BAD_SPORT_PLAYER_LEFT_DETECTED,0x65ee68ba2c50ae3d, CommandBadSportPlayerLeftDetected);
	  
	DLC_SCR_REGISTER_SECURE(NETWORK_ADD_INVALID_OBJECT_MODEL,0x7f562dbc212e81f9, CommandNetworkAddInvalidObjectModel);
	DLC_SCR_REGISTER_SECURE(NETWORK_REMOVE_INVALID_OBJECT_MODEL,0x791edb5803b2f468, CommandNetworkRemoveInvalidObjectModel);
	DLC_SCR_REGISTER_SECURE(NETWORK_CLEAR_INVALID_OBJECT_MODELS,0x03b2f03a53d85e41, CommandNetworkClearInvalidObjectModels);
	  
	SCR_REGISTER_UNUSED(ADD_MODEL_TO_INSTANCED_CONTENT_ALLOW_LIST,0xbcaaa30292cf0dfd,CommandAddModelToInstancedContentAllowList);
	SCR_REGISTER_UNUSED(REMOVE_ALL_MODELS_FROM_INSTANCED_CONTENT_ALLOW_LIST,0x96c7a61f6d568953,CommandRemoveAllModelsFromInstancedContentAllowList);

	SCR_REGISTER_SECURE(NETWORK_APPLY_PED_SCAR_DATA,0xb7f81eb507abeb80, CommandNetworkApplyPedScarData);

	// script functions
    SCR_REGISTER_SECURE(NETWORK_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT,0xd9b114a8d3a8319f, CommandNetworkSetThisScriptIsNetworkScript);
	SCR_REGISTER_SECURE(NETWORK_TRY_TO_SET_THIS_SCRIPT_IS_NETWORK_SCRIPT,0xfd86bdac70588822, CommandNetworkTryToSetThisScriptIsNetworkScript);
	SCR_REGISTER_SECURE(NETWORK_GET_THIS_SCRIPT_IS_NETWORK_SCRIPT,0xdd50b5a22807bad4, CommandNetworkGetThisScriptIsNetworkScript);
	SCR_REGISTER_UNUSED(NETWORK_SET_TEAM_RESERVATIONS,0xb168680dd1cfe6bd, CommandNetworkSetTeamReservations);
    SCR_REGISTER_SECURE(NETWORK_GET_MAX_NUM_PARTICIPANTS,0x5d79167fed95f0b0, CommandNetworkGetMaxNumParticipants);
    SCR_REGISTER_SECURE(NETWORK_GET_NUM_PARTICIPANTS,0xd4538f501edcd96c, CommandNetworkGetNumParticipants);
    SCR_REGISTER_SECURE(NETWORK_GET_SCRIPT_STATUS,0x0eeb2c2ca48fb650, CommandNetworkGetScriptStatus);
	SCR_REGISTER_SECURE(NETWORK_REGISTER_HOST_BROADCAST_VARIABLES,0x21f2b09183f31d02, CommandNetworkRegisterHostBroadcastVariables);
	SCR_REGISTER_SECURE(NETWORK_REGISTER_PLAYER_BROADCAST_VARIABLES,0x7157b1051528d729, CommandNetworkRegisterPlayerBroadcastVariables);
	SCR_REGISTER_SECURE(NETWORK_REGISTER_HIGH_FREQUENCY_HOST_BROADCAST_VARIABLES,0x2b297170e982e21e, CommandNetworkRegisterHighFrequencyHostBroadcastVariables);
	SCR_REGISTER_SECURE(NETWORK_REGISTER_HIGH_FREQUENCY_PLAYER_BROADCAST_VARIABLES,0x55b917f38ca35331, CommandNetworkRegisterHighFrequencyPlayerBroadcastVariables);
	SCR_REGISTER_UNUSED(NETWORK_TAG_FOR_DEBUG_REMOTE_PLAYER_BROADCAST_VARIABLES,0x792074cbd53965b8, CommandNetworkTagForDebugRemotePlayerBroadcastVariables);
	SCR_REGISTER_SECURE(NETWORK_FINISH_BROADCASTING_DATA,0xf2eee9f362fb9601, CommandNetworkFinishBroadcastingData);
	SCR_REGISTER_SECURE(NETWORK_HAS_RECEIVED_HOST_BROADCAST_DATA,0x72fef2581032d369, CommandNetworkHasReceivedHostBroadcastData);
	SCR_REGISTER_UNUSED(NETWORK_ALLOW_CLIENT_ALTERATION_OF_HOST_BROADCAST_DATA,0x2e154d6ae9845056, CommandNetworkAllowClientAlterationOfHostBroadcastData);
	SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_INDEX,0x6b01934fa503547f, CommandNetworkGetPlayerIndex);
    SCR_REGISTER_SECURE(NETWORK_GET_PARTICIPANT_INDEX,0x32f47fd509bb6d38, CommandNetworkGetParticipantIndex);
    SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_INDEX_FROM_PED,0xff65cdb0eb7ace71, CommandNetworkGetPlayerIndexFromPed);
	SCR_REGISTER_SECURE(NETWORK_GET_NUM_CONNECTED_PLAYERS,0x053e03265870c820, CommandNetworkGetNumPlayers);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_CONNECTED,0x2aaf56a20af61a64, CommandNetworkIsPlayerConnected);
	SCR_REGISTER_SECURE(NETWORK_GET_TOTAL_NUM_PLAYERS,0x21e2c39591974468, CommandNetworkGetTotalNumPlayers);
	SCR_REGISTER_SECURE(NETWORK_IS_PARTICIPANT_ACTIVE,0x81f82ffbed0cacca, CommandNetworkIsParticipantActive);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_ACTIVE,0x0d01086b38ec256f, CommandNetworkIsPlayerActive);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_A_PARTICIPANT,0x976d40337fcb1481, CommandNetworkIsPlayerAParticipant);
	SCR_REGISTER_SECURE(NETWORK_IS_HOST_OF_THIS_SCRIPT,0x54e30a65f4fa4962, CommandNetworkIsHostOfThisScript);
	SCR_REGISTER_SECURE(NETWORK_GET_HOST_OF_THIS_SCRIPT,0x10490c0971778a41, CommandNetworkGetHostOfThisScript);
	SCR_REGISTER_SECURE(NETWORK_GET_HOST_OF_SCRIPT,0xcbe03d1b06a08744, CommandNetworkGetHostOfScript);
	SCR_REGISTER_UNUSED(NETWORK_GET_HOST_OF_THREAD,0x900d5a343be5e45b, CommandNetworkGetHostOfThread);
    SCR_REGISTER_UNUSED(NETWORK_ALLOW_PLAYERS_TO_JOIN,0x5a30949f9b09087c, CommandNetworkAllowPlayersToJoin);
	SCR_REGISTER_SECURE(NETWORK_SET_MISSION_FINISHED,0xe68df5af3a89b195, CommandNetworkSetMissionFinished);
	SCR_REGISTER_SECURE(NETWORK_IS_SCRIPT_ACTIVE,0x7b0a672b0283f03e, CommandNetworkIsScriptActive);
	SCR_REGISTER_SECURE(NETWORK_IS_SCRIPT_ACTIVE_BY_HASH,0xab2eff25cde56451, CommandNetworkIsScriptActiveByHash);
	SCR_REGISTER_SECURE(NETWORK_IS_THREAD_A_NETWORK_SCRIPT,0xa19c6234b5450d1c, CommandNetworkIsThreadANetworkScript);
	SCR_REGISTER_SECURE(NETWORK_GET_NUM_SCRIPT_PARTICIPANTS,0x49a870e8dcc9a2c4, CommandNetworkGetNumScriptParticipants);
	SCR_REGISTER_SECURE(NETWORK_GET_INSTANCE_ID_OF_THIS_SCRIPT,0x5c5ce5291fb79534, CommandNetworkGetInstanceIdOfThisScript);
	SCR_REGISTER_SECURE(NETWORK_GET_POSITION_HASH_OF_THIS_SCRIPT,0x64e70c7c5d6de65c, CommandNetworkGetPositionHashOfThisScript);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_A_PARTICIPANT_ON_SCRIPT,0x715c12ff862f6649, CommandNetworkIsPlayerAParticipantOnScript);
	SCR_REGISTER_SECURE(NETWORK_PREVENT_SCRIPT_HOST_MIGRATION,0xdc0b8c11c11ae5c2, CommandNetworkPreventScriptHostMigration);
	SCR_REGISTER_SECURE(NETWORK_REQUEST_TO_BE_HOST_OF_THIS_SCRIPT,0x2fb25ae288339020, CommandNetworkRequestToBeHostOfThisScript);
	SCR_REGISTER_SECURE(PARTICIPANT_ID,0x74bd5c0c45a40771, CommandParticipantId);
	SCR_REGISTER_SECURE(PARTICIPANT_ID_TO_INT,0xf1354995c6159a78, CommandParticipantId);

	//----- Damage Tracking commands
	SCR_REGISTER_SECURE(NETWORK_GET_KILLER_OF_PLAYER,0x4ad657fdb8fb690e, CommandNetworkGetKillerOfPlayer);
	SCR_REGISTER_SECURE(NETWORK_GET_DESTROYER_OF_NETWORK_ID,0x2aa852e1ebb27a57, CommandNetworkGetDestroyerOfNetworkId);
	SCR_REGISTER_SECURE(NETWORK_GET_DESTROYER_OF_ENTITY,0xeb72efbd6f908c4b, CommandNetworkGetDestroyerOfEntity);
	SCR_REGISTER_SECURE(NETWORK_GET_ASSISTED_KILL_OF_ENTITY,0x3cdd315fa70f2c21, CommandNetworkGetAssistedKillOfEntity);
	SCR_REGISTER_SECURE(NETWORK_GET_ASSISTED_DAMAGE_OF_ENTITY,0x15f05eda59cfdabb, CommandNetworkGetAssistedDamageOfEntity);
	SCR_REGISTER_SECURE(NETWORK_GET_ENTITY_KILLER_OF_PLAYER,0x596e43bc4865f22a, CommandNetworkGetEntityKillerOfPlayer);

	//----- 

	SCR_REGISTER_SECURE(NETWORK_SET_CURRENT_PUBLIC_CONTENT_ID, 0x2c863acdcd12b3db, CommandSetCurrentPublicContentId);
	SCR_REGISTER_SECURE(NETWORK_SET_CURRENT_CHAT_OPTION, 0x179b6b0569a08f3c, CommandSetCurrentChatOption);
	SCR_REGISTER_SECURE(NETWORK_SET_CURRENT_SPAWN_LOCATION_OPTION, 0xaa6d5451dc3448b6, CommandSetCurrentSpawnLocationOption);
	SCR_REGISTER_SECURE(NETWORK_SET_VEHICLE_DRIVEN_IN_TEST_DRIVE, 0x8c70252fc40f320b, CommandSetVehicleDrivenInTestDrive);
	SCR_REGISTER_UNUSED(NETWORK_SET_VEHICLE_DRIVEN_LOCATION, 0xe4f6f71386c075a2, CommandSetVehicleDrivenLocation);
    SCR_REGISTER_SECURE(NETWORK_RESURRECT_LOCAL_PLAYER,0x35d0922793f4b21d, CommandNetworkResurrectLocalPlayer);
	SCR_REGISTER_UNUSED(NETWORK_RESPAWN_LOCAL_PLAYER_TELEMETRY,0xf25a2ab9ae118871, CommandNetworkRespawnLocalPlayerTelemetry);	
	SCR_REGISTER_SECURE(NETWORK_SET_LOCAL_PLAYER_INVINCIBLE_TIME,0x49f5a737e649bb72, CommandNetworkSetLocalPlayerInvincibilityTime);
	SCR_REGISTER_SECURE(NETWORK_IS_LOCAL_PLAYER_INVINCIBLE,0x1c41169943f5d8ae, CommandNetworkIsLocalPlayerInvincible);
    SCR_REGISTER_SECURE(NETWORK_DISABLE_INVINCIBLE_FLASHING,0xfa88a0a3e965437c, CommandNetworkDisableInvincibleFlashing);
	SCR_REGISTER_SECURE(NETWORK_PATCH_POST_CUTSCENE_HS4F_TUN_ENT,0x90df9c4b37bb2e9d, CommandNetworkPatchPostCutsceneHS4F_TUN_ENT);
	SCR_REGISTER_SECURE(NETWORK_SET_LOCAL_PLAYER_SYNC_LOOK_AT,0x962a0c925eda4345, CommandNetworkSyncLocalPlayerLookAt);
	SCR_REGISTER_UNUSED(NETWORK_GET_TIME_PLAYER_HAS_BEEN_DEAD_FOR,0xa92c55cf6ef84e44, CommandNetworkGetTimePlayerBeenDeadFor);
    SCR_REGISTER_UNUSED(NETWORK_HAS_PLAYER_COLLECTED_PICKUP,0x8f94160957344282, CommandNetworkHasPlayerCollectedPickup);
	SCR_REGISTER_SECURE(NETWORK_HAS_ENTITY_BEEN_REGISTERED_WITH_THIS_THREAD,0xd0defe7707874c5b, CommandNetworkHasEntityBeenRegisteredWithThisThread);
	SCR_REGISTER_SECURE(NETWORK_GET_NETWORK_ID_FROM_ENTITY,0x1d03f9bf5cf8ca0a, CommandNetworkGetNetworkIdFromEntity);
	SCR_REGISTER_SECURE(NETWORK_GET_ENTITY_FROM_NETWORK_ID,0x55456dd1219a39df, CommandNetworkGetEntityFromNetworkId);
	SCR_REGISTER_SECURE(NETWORK_GET_ENTITY_IS_NETWORKED,0xd64c90c3f536f963, CommandNetworkGetEntityIsNetworked);
	SCR_REGISTER_SECURE(NETWORK_GET_ENTITY_IS_LOCAL,0x58fdf0b505aa2260, CommandNetworkGetEntityIsLocal);
	SCR_REGISTER_SECURE(NETWORK_REGISTER_ENTITY_AS_NETWORKED,0x799746e3e915923d, CommandNetworkRegisterEntityAsNetworked);
	SCR_REGISTER_SECURE(NETWORK_UNREGISTER_NETWORKED_ENTITY,0x6c46da3280262b06, CommandNetworkUnregisterNetworkedEntity);
	SCR_REGISTER_SECURE(NETWORK_DOES_NETWORK_ID_EXIST,0xccdcd6672dae6835, CommandNetworkDoesNetworkIdExist);
	SCR_REGISTER_SECURE(NETWORK_DOES_ENTITY_EXIST_WITH_NETWORK_ID,0x90f6e2f6488b98ba, CommandNetworkDoesEntityExistWithNetworkId);
	SCR_REGISTER_SECURE(NETWORK_REQUEST_CONTROL_OF_NETWORK_ID,0xc31eeca11005273c, CommandNetworkRequestControlOfNetworkId);
	SCR_REGISTER_SECURE(NETWORK_HAS_CONTROL_OF_NETWORK_ID,0x07b2f8356dc13cf4, CommandNetworkHasControlOfNetworkId);	
	SCR_REGISTER_SECURE(NETWORK_IS_NETWORK_ID_REMOTELY_CONTROLLED,0x10cbf225ed627e4a, CommandIsNetworkIdRemotelyControlled);	
	SCR_REGISTER_SECURE(NETWORK_REQUEST_CONTROL_OF_ENTITY,0xe7dd33d0e2a313f4, CommandNetworkRequestControlOfEntity);
	SCR_REGISTER_SECURE(NETWORK_REQUEST_CONTROL_OF_DOOR,0x03b2da1950c5cd94, CommandNetworkRequestControlOfDoor);
	SCR_REGISTER_SECURE(NETWORK_HAS_CONTROL_OF_ENTITY,0x3a8b0f5b0e3de13a, CommandNetworkHasControlOfEntity);
	SCR_REGISTER_SECURE(NETWORK_HAS_CONTROL_OF_PICKUP,0xf749ecf0c32f9bf6, CommandNetworkHasControlOfPickup);
	SCR_REGISTER_SECURE(NETWORK_HAS_CONTROL_OF_DOOR,0x0c61220886726717, CommandNetworkHasControlOfDoor);
	SCR_REGISTER_SECURE(NETWORK_IS_DOOR_NETWORKED,0x3197d8f164016b52, CommandIsDoorNetworked);
    SCR_REGISTER_UNUSED(NETWORK_GET_TEAM_RGB_COLOUR,0xdf06ebfab5763e98, CommandNetworkGetTeamRGBColour);
    SCR_REGISTER_UNUSED(NETWORK_SET_TEAM_RGB_COLOUR,0x273ffce6bd116c96, CommandNetworkSetTeamRGBColour);

	SCR_REGISTER_SECURE(VEH_TO_NET,0xa08cc064dac564a5, CommandNetworkGetNetworkIdFromEntity);
	SCR_REGISTER_SECURE(PED_TO_NET,0xed28ee4be581a032, CommandNetworkGetNetworkIdFromEntity);
	SCR_REGISTER_SECURE(OBJ_TO_NET,0x2d0f5291e9305922, CommandNetworkGetNetworkIdFromEntity);
	SCR_REGISTER_SECURE(NET_TO_VEH,0xa5677134b9672173, CommandNetworkGetEntityFromNetworkId);
	SCR_REGISTER_SECURE(NET_TO_PED,0x8af984152f749d80, CommandNetworkGetEntityFromNetworkId);
	SCR_REGISTER_SECURE(NET_TO_OBJ,0xe8bbc6cc2c60f24a, CommandNetworkGetEntityFromNetworkId);
	SCR_REGISTER_SECURE(NET_TO_ENT,0xf2d8dacfaebd9629, CommandNetworkGetEntityFromNetworkId);

	SCR_REGISTER_SECURE(NETWORK_GET_LOCAL_HANDLE,0x9edec39ee142c8d5, CommandNetworkGetLocalHandle);
	SCR_REGISTER_SECURE(NETWORK_HANDLE_FROM_USER_ID,0xa996e10b1ecda45e, CommandNetworkHandleFromUserID);
	SCR_REGISTER_SECURE(NETWORK_HANDLE_FROM_MEMBER_ID,0xff9da93a747b9ba0, CommandNetworkHandleFromMemberID);
	SCR_REGISTER_UNUSED(NETWORK_HANDLE_FROM_STRING,0xef4886404593a9a6, CommandNetworkHandleFromString);
	SCR_REGISTER_SECURE(NETWORK_HANDLE_FROM_PLAYER,0x460ffcec183339c6, CommandNetworkHandleFromPlayer);
	SCR_REGISTER_SECURE(NETWORK_HASH_FROM_PLAYER_HANDLE,0x4ae7f6d227ed332c, CommandNetworkHashFromPlayerHandle);
	SCR_REGISTER_SECURE(NETWORK_HASH_FROM_GAMER_HANDLE,0x2c99b177728438dd, CommandNetworkHashFromGamerHandle);
	SCR_REGISTER_SECURE(NETWORK_HANDLE_FROM_FRIEND,0x68527850aed38263, CommandNetworkHandleFromFriend);
	SCR_REGISTER_UNUSED(NETWORK_HANDLE_FROM_MET_PLAYER,0xd5843b707d2c8c83, CommandNetworkHandleFromMetPlayer);
	SCR_REGISTER_UNUSED(NETWORK_LOG_GAMER_HANDLE,0x0e3d0311d135dbb6, CommandNetworkLogGamerHandle);

	SCR_REGISTER_SECURE(NETWORK_GAMERTAG_FROM_HANDLE_START,0x5f0a7b7235c6abed, CommandNetworkGamerTagFromHandleStart);
	SCR_REGISTER_SECURE(NETWORK_GAMERTAG_FROM_HANDLE_PENDING,0x1c80838dc24f06d0, CommandNetworkGamerTagFromHandlePending);
	SCR_REGISTER_SECURE(NETWORK_GAMERTAG_FROM_HANDLE_SUCCEEDED,0x38d0eb5a1b7e5ef3, CommandNetworkGamerTagFromHandleSucceeded);
	SCR_REGISTER_SECURE(NETWORK_GET_GAMERTAG_FROM_HANDLE,0x84a4c35df8ab2edf, CommandNetworkGetGamerTagFromHandle);

	SCR_REGISTER_SECURE(NETWORK_DISPLAYNAMES_FROM_HANDLES_START,0x750877efc42c7771, CommandNetworkDisplayNamesFromHandlesStart);
	SCR_REGISTER_SECURE(NETWORK_GET_DISPLAYNAMES_FROM_HANDLES,0xed9701b5bb864fe4, CommandNetworkGetDisplayNamesFromHandles);

	SCR_REGISTER_SECURE(NETWORK_ARE_HANDLES_THE_SAME,0x3665714316ba3abc, CommandNetworkAreHandlesTheSame);
	SCR_REGISTER_SECURE(NETWORK_IS_HANDLE_VALID,0x66237abafdf8615a, CommandNetworkIsHandleValid);
	SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_FROM_GAMER_HANDLE,0xf6d95ac799ec5ce4, CommandNetworkGetPlayerIndexFromGamerHandle);
	SCR_REGISTER_SECURE(NETWORK_MEMBER_ID_FROM_GAMER_HANDLE,0xfd09a4adc15caae9, CommandNetworkMemberIDFromGamerHandle);
	SCR_REGISTER_SECURE(NETWORK_IS_GAMER_IN_MY_SESSION,0x20cd75cdfaf71fd3, CommandNetworkIsGamerInMySession);

	SCR_REGISTER_SECURE(NETWORK_SHOW_PROFILE_UI,0x396cbc3b89746a9d, CommandNetworkShowProfileUI);
	SCR_REGISTER_UNUSED(NETWORK_SHOW_FEEDBACK_UI,0x07bc9d53f628da19, CommandNetworkShowFeedbackUI);
	SCR_REGISTER_UNUSED(NETWORK_GET_GAME_REGION,0x4def6142d918742c, CommandNetworkGetGameRegion);

	SCR_REGISTER_SECURE(NETWORK_PLAYER_GET_NAME,0xed94d2f6be1e6e57, CommandNetworkPlayerGetName);
	SCR_REGISTER_SECURE(NETWORK_PLAYER_GET_USERID,0x9bf88c3ed22c5680, CommandNetworkPlayerGetUserId);
	SCR_REGISTER_SECURE(NETWORK_PLAYER_IS_ROCKSTAR_DEV,0x62360ee837e523dc, CommandNetworkPlayerIsRockstarDev);
	SCR_REGISTER_SECURE(NETWORK_PLAYER_INDEX_IS_CHEATER,0x3237762f8dbb3fab, CommandNetworkPlayerIsCheater);

	SCR_REGISTER_SECURE(NETWORK_ENTITY_GET_OBJECT_ID,0x9e617051bd76245d, CommandNetworkEntityGetObjectId);
	SCR_REGISTER_SECURE(NETWORK_GET_ENTITY_FROM_OBJECT_ID,0x976d297cb25bfc0b, CommandNetworkGetEntityFromObjectId);

    SCR_REGISTER_SECURE(NETWORK_IS_INACTIVE_PROFILE,0x329832af2d9252af, CommandNetworkIsInactiveProfile);

    SCR_REGISTER_SECURE(NETWORK_GET_MAX_FRIENDS,0xce042f81f8cc55ec, CommandNetworkGetMaxFriends);
	SCR_REGISTER_SECURE(NETWORK_GET_FRIEND_COUNT,0xb9b7c1beadfb5967, CommandNetworkGetFriendCount);
    SCR_REGISTER_SECURE(NETWORK_GET_FRIEND_NAME,0x399fbd01eec6178d, CommandNetworkGetFriendName);
	SCR_REGISTER_SECURE(NETWORK_GET_FRIEND_DISPLAY_NAME,0x2a7e749e640470c6, CommandNetworkGetFriendDisplayName);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND_ONLINE,0x7005a5caca127e31, CommandNetworkIsFriendOnline);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND_HANDLE_ONLINE,0x98e8458382a8bd60, CommandNetworkIsFriendHandleOnline);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND_IN_SAME_TITLE,0xf00204ac7ef3155c, CommandNetworkIsFriendInSameTitle);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND_IN_MULTIPLAYER,0xc74fb9ce8dff14bc, CommandNetworkIsFriendInMultiplayer);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND,0x106c9ee9d83f20df, CommandNetworkIsFriend);
	SCR_REGISTER_SECURE(NETWORK_IS_PENDING_FRIEND,0xd543c9cc737e715d, CommandNetworkIsPendingFriend);
	SCR_REGISTER_SECURE(NETWORK_IS_ADDING_FRIEND,0x89df2f9e72bf257b, CommandNetworkIsAddingFriend);
	SCR_REGISTER_SECURE(NETWORK_ADD_FRIEND,0x3ca6eff7d6332047, CommandNetworkAddFriend);
	SCR_REGISTER_SECURE(NETWORK_IS_FRIEND_INDEX_ONLINE,0xd5ac2a45076e190f, CommandNetworkIsFriendIndexOnline);

    SCR_REGISTER_UNUSED(NETWORK_GET_NUM_PLAYERS_MET,0xf02114ea7a458f22, CommandNetworkGetNumPlayersMet);
    SCR_REGISTER_UNUSED(NETWORK_GET_MET_PLAYER_NAME,0xb330a05489d0597e, CommandNetworkGetMetPlayerName);

	SCR_REGISTER_UNUSED(NETWORK_SET_PLAYER_IS_CORRUPT,0x81d3117f42d78599, CommandNetworkSetPlayerIsCorrupt);
	SCR_REGISTER_SECURE(NETWORK_SET_PLAYER_IS_PASSIVE,0x00f11f13db7b0557, CommandNetworkSetPlayerIsPassive);
	SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_OWNS_WAYPOINT,0xb0000c531d36bf52, CommandNetworkGetPlayerOwnsWaypoint);
	SCR_REGISTER_SECURE(NETWORK_CAN_SET_WAYPOINT,0x2c9a82f641eb2b03, CommandNetworkCanSetWaypoint);
	SCR_REGISTER_SECURE(NETWORK_IGNORE_REMOTE_WAYPOINTS,0x1323701413ee96ed, CommandNetworkIgnoreRemoteWaypoints);

	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_ON_BLOCKLIST,0x4fe96998f5362544, CommandNetworkIsPlayerOnBlocklist);

    SCR_REGISTER_SECURE(NETWORK_SET_SCRIPT_AUTOMUTED,0xf3b37fc3d63a0b2e, CommandNetworkSetScriptAutomuted);
    SCR_REGISTER_SECURE(NETWORK_HAS_AUTOMUTE_OVERRIDE,0x713140fddd107d38, CommandNetworkHasAutomuteOverride);

    SCR_REGISTER_SECURE(NETWORK_HAS_HEADSET,0xe99748d31dcc4193, CommandNetworkHasHeadset);
	SCR_REGISTER_SECURE(NETWORK_SET_LOOK_AT_TALKERS,0x19f0c576b0bb607f, CommandSetLookAtTalkers);
	SCR_REGISTER_UNUSED(NETWORK_IS_LOCAL_TALKING,0xa066f2ce431c1037, CommandNetworkIsLocalTalking);
	SCR_REGISTER_SECURE(NETWORK_IS_PUSH_TO_TALK_ACTIVE,0xabbff2b5d58ab081, CommandNetworkIsPushToTalkActive);

    SCR_REGISTER_SECURE(NETWORK_GAMER_HAS_HEADSET,0x3503dc08fbedea4f, CommandNetworkGamerHasHeadset);
    SCR_REGISTER_SECURE(NETWORK_IS_GAMER_TALKING,0x073cb74794ba9b10, CommandNetworkIsGamerTalking);
    SCR_REGISTER_UNUSED(NETWORK_MUTE_GAMER,0x9657890dcbe16133, CommandNetworkMuteGamer);

	SCR_REGISTER_SECURE(NETWORK_PERMISSIONS_HAS_GAMER_RECORD, 0x559ebf901a8c68e0, CommandNetworkHasGamerRecord);
	SCR_REGISTER_SECURE(NETWORK_CAN_COMMUNICATE_WITH_GAMER, 0x5b54656f67f47385, CommandNetworkCanCommunicateWithGamer);
	SCR_REGISTER_UNUSED(NETWORK_CAN_VOICE_CHAT_WITH_GAMER,0x41e4d9038b40d715, CommandNetworkCanVoiceChatWithGamer);
	SCR_REGISTER_SECURE(NETWORK_CAN_TEXT_CHAT_WITH_GAMER,0x945749f6dcb3d9dd, CommandNetworkCanTextChatWithGamer);
    SCR_REGISTER_SECURE(NETWORK_IS_GAMER_MUTED_BY_ME,0x5ac8e3af6b0e9458, CommandNetworkIsGamerMutedByMe);
    SCR_REGISTER_SECURE(NETWORK_AM_I_MUTED_BY_GAMER,0x3d2c801f928ba9c8, CommandNetworkAmIMutedByGamer);
    SCR_REGISTER_SECURE(NETWORK_IS_GAMER_BLOCKED_BY_ME,0x4882a06b36985dd5, CommandNetworkIsGamerBlockedByMe);
    SCR_REGISTER_SECURE(NETWORK_AM_I_BLOCKED_BY_GAMER,0x644b8fb9f0b8af6e, CommandNetworkAmIBlockedByGamer);

	SCR_REGISTER_SECURE(NETWORK_CAN_VIEW_GAMER_USER_CONTENT,0x3984fbefe07835d4, CommandNetworkCanViewGamerUserContent);
	SCR_REGISTER_SECURE(NETWORK_HAS_VIEW_GAMER_USER_CONTENT_RESULT,0xdbbcdf48b7dc1a3c, CommandNetworkHasViewGamerUserContentResult);
	SCR_REGISTER_SECURE(NETWORK_CAN_PLAY_MULTIPLAYER_WITH_GAMER,0xd437dca500b0f4cc, CommandNetworkCanPlayMultiplayerWithGamer);
	SCR_REGISTER_UNUSED(NETWORK_HAS_PLAY_MULTIPLAYER_WITH_GAMER_RESULT,0x96ffbe55e8fb1b06, CommandNetworkHasPlayMultiplayerWithGamerResult);
	SCR_REGISTER_SECURE(NETWORK_CAN_GAMER_PLAY_MULTIPLAYER_WITH_ME,0xf316f43863f9dc1c, CommandNetworkCanGamerPlayMultiplayerWithMe);
	SCR_REGISTER_SECURE(NETWORK_CAN_SEND_LOCAL_INVITE, 0x021abcbd98ec4320, CommandNetworkCanSendLocalInvite);
	SCR_REGISTER_SECURE(NETWORK_CAN_RECEIVE_LOCAL_INVITE, 0x421e34c55f125964, CommandNetworkCanReceiveLocalInvite);

	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_TALKING,0x4f35a48da4069275, CommandNetworkIsPlayerTalking);
    SCR_REGISTER_SECURE(NETWORK_PLAYER_HAS_HEADSET,0x096cdf67382a3eaf, CommandNetworkPlayerHasHeadset);
	SCR_REGISTER_UNUSED(NETWORK_SET_PLAYER_MUTED,0x36cc9dac389105a5, CommandNetworkSetPlayerMuted);
	SCR_REGISTER_UNUSED(NETWORK_CAN_COMMUNICATE_WITH_PLAYER,0x48b80ad893d1511e, CommandNetworkCanCommunicateWithPlayer);
	SCR_REGISTER_UNUSED(NETWORK_CAN_VOICE_CHAT_WITH_PLAYER,0x8e211ab9d76da9af, CommandNetworkCanVoiceChatWithPlayer);
	SCR_REGISTER_UNUSED(NETWORK_CAN_TEXT_CHAT_WITH_PLAYER,0xdfe2352abfe36577, CommandNetworkCanTextChatWithPlayer);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_MUTED_BY_ME,0x2c9478e81d9b8529, CommandNetworkIsPlayerMutedByMe);
    SCR_REGISTER_SECURE(NETWORK_AM_I_MUTED_BY_PLAYER,0x562b711f727c6a94, CommandNetworkAmIMutedByPlayer);
    SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_BLOCKED_BY_ME,0x9920f898aa3d335e, CommandNetworkIsPlayerBlockedByMe);
    SCR_REGISTER_SECURE(NETWORK_AM_I_BLOCKED_BY_PLAYER,0xe8a860ca60e8825c, CommandNetworkAmIBlockedByPlayer);

    SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_LOUDNESS,0xaeed8da6a6fc1e71, CommandNetworkGetPlayerLoudness);
    SCR_REGISTER_UNUSED(NETWORK_SET_TALKER_FOCUS,0xc18aecadee28bba5, CommandNetworkSetTalkerFocus);
    SCR_REGISTER_SECURE(NETWORK_SET_TALKER_PROXIMITY,0xf2a3015d39de3214, CommandNetworkSetTalkerProximity);
	SCR_REGISTER_UNUSED(NETWORK_SET_TALKER_PROXIMITY_HEIGHT,0x4acccec6475126e0, CommandNetworkSetTalkerProximityHeight);
    SCR_REGISTER_SECURE(NETWORK_GET_TALKER_PROXIMITY,0xb196c3161720011e, CommandNetworkGetTalkerProximity);
	SCR_REGISTER_UNUSED(NETWORK_ALLOW_OVERRIDE_FOR_NEGATIVE_PROXIMITY,0xd9513f44e27818fe, CommandNetworkSetAllowUniDirectionalOverrideForNegativeProximity);
	SCR_REGISTER_UNUSED(NETWORK_SET_LOUDHAILER_PROXIMITY,0x5842e9a35ba70270, CommandNetworkSetLoudhailerProximity);
	SCR_REGISTER_UNUSED(NETWORK_SET_LOUDHAILER_PROXIMITY_HEIGHT,0x6b6be54611f369b4, CommandNetworkSetLoudhailerProximityHeight);
	SCR_REGISTER_UNUSED(NETWORK_OVERRIDE_ALL_CHAT_RESTRICTIONS,0x95660b858df70978, CommandNetworkOverrideAllChatRestrictions);
	SCR_REGISTER_SECURE(NETWORK_SET_VOICE_ACTIVE,0x5e1f7fb772c73d20, CommandNetworkSetVoiceCommunication);
    SCR_REGISTER_SECURE(NETWORK_REMAIN_IN_GAME_CHAT,0x868fd470a00ddbb9, CommandNetworkRemainInGameChat);
    SCR_REGISTER_SECURE(NETWORK_OVERRIDE_TRANSITION_CHAT,0xfdadb87ed6f4ac6e, CommandNetworkOverrideTransitionChat);
    SCR_REGISTER_SECURE(NETWORK_SET_TEAM_ONLY_CHAT,0xeb869caccf04c3b5, CommandNetworkSetTeamOnlyChat);
	SCR_REGISTER_SECURE(NETWORK_SET_SCRIPT_CONTROLLING_TEAMS,0xf0f6808c20c6fc95, CommandNetworkSetScriptControllingTeams);
	SCR_REGISTER_SECURE(NETWORK_SET_SAME_TEAM_AS_LOCAL_PLAYER,0x36379967668bc070, CommandNetworkSetonSameTeamForText);
	SCR_REGISTER_UNUSED(NETWORK_SET_TEAM_ONLY_TEXT_CHAT,0x6bfe2a77c7c672c8, CommandNetworkSetTeamOnlyTextChat);
	SCR_REGISTER_UNUSED(NETWORK_SET_TEAM_CHAT_INCLUDE_UNASSIGNED_TEAMS,0xc31be88ec6be1993, CommandNetworkSetTeamChatIncludeUnassignedTeams);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_TEAM_RESTRICTIONS,0x57814a685f9243d8, CommandNetworkOverrideTeamRestrictions);
	SCR_REGISTER_SECURE(NETWORK_SET_OVERRIDE_SPECTATOR_MODE,0xb83fa1b51fd31083, CommandNetworkSetOverrideSpectatorMode);
	SCR_REGISTER_SECURE(NETWORK_SET_OVERRIDE_TUTORIAL_SESSION_CHAT,0xfba140877b6cf83b, CommandNetworkSetOverrideTutorialSessionChat);
	SCR_REGISTER_UNUSED(NETWORK_SET_PROXIMITY_AFFECTS_SEND,0xe4c66e240df45497, CommandNetworkSetProximityAffectsSend);
	SCR_REGISTER_UNUSED(NETWORK_SET_PROXIMITY_AFFECTS_RECEIVE,0x06fa0b3c50d350d4, CommandNetworkSetProximityAffectsReceive);
	SCR_REGISTER_SECURE(NETWORK_SET_PROXIMITY_AFFECTS_TEAM,0xb299a8f810f36aa2, CommandNetworkSetProximityAffectsTeam);
	SCR_REGISTER_SECURE(NETWORK_SET_NO_SPECTATOR_CHAT,0xc6d350050cadbb6b, CommandNetworkSetNoSpectatorChat);
    SCR_REGISTER_SECURE(NETWORK_SET_IGNORE_SPECTATOR_CHAT_LIMITS_SAME_TEAM,0xd5b849235c9cb53b, CommandNetworkSetIgnoreSpectatorChatLimitsForSameTeam);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_CHAT_RESTRICTIONS,0x468bbc73cbbd8688, CommandNetworkOverrideChatRestrictions);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_SEND_RESTRICTIONS,0x2a27ddfd2a3dacf9, CommandNetworkOverrideSendRestrictions);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_SEND_RESTRICTIONS_ALL,0xd944f164f0a87f7f, CommandNetworkOverrideSendRestrictionsAll);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_RECEIVE_RESTRICTIONS,0xbac548a8263a1db3, CommandNetworkOverrideReceiveRestrictions);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_RECEIVE_RESTRICTIONS_ALL,0xa014caef43997ae0, CommandNetworkOverrideReceiveRestrictionsAll);
	SCR_REGISTER_UNUSED(NETWORK_OVERRIDE_CHAT_RESTRICTIONS_TEAM_RECEIVE,0xeb51713bfb790009, CommandNetworkOverrideChatRestrictionsTeamReceive);
	SCR_REGISTER_SECURE(NETWORK_SET_VOICE_CHANNEL,0x3b0d4839efa8b755, CommandNetworkSetVoiceChannel);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_VOICE_CHANNEL,0xd455cbc68b2b94af, CommandNetworkClearVoiceChannel);
    SCR_REGISTER_SECURE(NETWORK_APPLY_VOICE_PROXIMITY_OVERRIDE,0xb16882aaf7bc5ae2, CommandNetworkApplyVoiceProximityOverride);
    SCR_REGISTER_SECURE(NETWORK_CLEAR_VOICE_PROXIMITY_OVERRIDE,0x60914999f22ac01c, CommandNetworkClearVoiceProximityOverride);
    SCR_REGISTER_SECURE(NETWORK_ENABLE_VOICE_BANDWIDTH_RESTRICTION,0x1f887a325e6c8d4c, CommandNetworkEnableVoiceBandwidthRestriction);
    SCR_REGISTER_SECURE(NETWORK_DISABLE_VOICE_BANDWIDTH_RESTRICTION,0xfa8a97c557528f85, CommandNetworkDisableVoiceBandwidthRestriction);
	SCR_REGISTER_SECURE(NETWORK_GET_MUTE_COUNT_FOR_PLAYER,0x15d53e1db1bcfbd6, CommandNetworkGetMuteCountForPlayer);
	SCR_REGISTER_UNUSED(DEBUG_NETWORK_SET_LOCAL_PLAYER_MUTE_COUNT,0x2c23d6d8ced35f4c, CommandDebugNetworkSetLocalMuteCounts);

	// Text Chat
	SCR_REGISTER_SECURE(NETWORK_SET_SPECTATOR_TO_NON_SPECTATOR_TEXT_CHAT,0x3655ff7918c6aea6, CommandNetworkSetSpectatorToNonSpectatorTextChat);
	SCR_REGISTER_SECURE(NETWORK_TEXT_CHAT_IS_TYPING,0xd199ee48d2842eb1, CommandNetworkTextChatIsTyping);

    SCR_REGISTER_UNUSED(NETWORK_STORE_SINGLE_PLAYER_GAME,0xfd881248993e8ff9, CommandNetworkStoreSinglePlayerGame);
    SCR_REGISTER_SECURE(SHUTDOWN_AND_LAUNCH_SINGLE_PLAYER_GAME,0xc0bcf14f60b6723a, CommandShutdownAndLaunchSinglePlayerGame);
    SCR_REGISTER_UNUSED(SHUTDOWN_AND_LAUNCH_NETWORK_GAME,0xa67137b9e8820402, CommandShutdownAndLaunchNetworkGame);
	SCR_REGISTER_SECURE(SHUTDOWN_AND_LOAD_MOST_RECENT_SAVE,0xad8c8a4fc6466e66, CommandShutdownAndLoadMostRecentSave);
    
    SCR_REGISTER_SECURE(NETWORK_SET_FRIENDLY_FIRE_OPTION,0xae3c2157fd741193, CommandNetworkSetFriendlyFireOption);
    
	SCR_REGISTER_SECURE(NETWORK_SET_RICH_PRESENCE,0x5f540c4c109ab569, CommandNetworkSetRichPresence);
	SCR_REGISTER_SECURE(NETWORK_SET_RICH_PRESENCE_STRING,0x6e34665f75b2ee0a, CommandNetworkSetRichPresenceString);

	SCR_REGISTER_SECURE(NETWORK_GET_TIMEOUT_TIME,0x3d474216c4f0d38b, CommandNetworkGetTimeoutTime);

	SCR_REGISTER_SECURE(NETWORK_LEAVE_PED_BEHIND_BEFORE_WARP,0xd680ad74cb93547f, CommandNetworkLeavePedBehindBeforeWarp);
    SCR_REGISTER_SECURE(NETWORK_LEAVE_PED_BEHIND_BEFORE_CUTSCENE,0xf0ff9641ad2902cc, CommandNetworkLeavePedBehindBeforeCutscene);
	SCR_REGISTER_UNUSED(NETWORK_CAN_LEAVE_PED_BEHIND,0x6b76d2352809bd86, CommandNetworkCanLeavePedBehind);
	SCR_REGISTER_SECURE(REMOVE_ALL_STICKY_BOMBS_FROM_ENTITY,0x72432781eb27d057,	CommandRemoveAllStickyBombsFromEntity);

	//  Network Bots
	SCR_REGISTER_UNUSED(NETWORK_BOT_GET_NAME,0x905e83e8a9686ef1, CommandNetworkBotGetName);
	SCR_REGISTER_UNUSED(NETWORK_BOT_RESURRECT,0x82842b79d03cfbb9, CommandNetworkBotResurrect);
	SCR_REGISTER_UNUSED(NETWORK_BOT_GET_PLAYER_ID,0xba707d8fb514e561, CommandNetworkBotGetPlayerId);
	SCR_REGISTER_UNUSED(NETWORK_BOT_EXISTS,0x6bed444499b7ff02, CommandNetworkBotExists);
	SCR_REGISTER_UNUSED(NETWORK_BOT_IS_LOCAL,0x6b990b3ba12b482d, CommandNetworkBotGetIsLocal);
	SCR_REGISTER_UNUSED(NETWORK_BOT_GET_NUMBER_OF_ACTIVE_BOTS,0xf986d9c0130ec542, CommandNetworkBotGetNumberOfActiveBots);
	SCR_REGISTER_UNUSED(NETWORK_BOT_GET_SCRIPT_BOT_INDEX,0x945a1a16b5193398, CommandNetworkBotGetScriptBotIndex);
	SCR_REGISTER_UNUSED(NETWORK_BOT_ADD,0x572e05c945757b02, CommandNetworkBotAdd);
	SCR_REGISTER_UNUSED(NETWORK_BOT_REMOVE,0x2b9330064a48ffc8, CommandNetworkBotRemove);
	SCR_REGISTER_UNUSED(NETWORK_BOT_LAUNCH_SCRIPT,0xad0e58ee08d3723f, CommandNetworkBotLaunchScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_STOP_SCRIPT,0x913ad33ceeeab80d, CommandNetworkBotStopScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_PAUSE_SCRIPT,0x902569b855cf47bf, CommandNetworkBotPauseScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_CONTINUE_SCRIPT,0xe441ae4311edc2ce, CommandNetworkBotContinue);

	SCR_REGISTER_UNUSED(NETWORK_BOT_JOIN_THIS_SCRIPT,0x2a89f505e47b59d1, CommandNetworkBotJoinThisScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_LEAVE_THIS_SCRIPT,0x745b5d5e82c9167e, CommandNetworkBotLeaveThisScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_JOIN_SCRIPT,0x87c72ac6511ef089, CommandNetworkBotJoinScript);
	SCR_REGISTER_UNUSED(NETWORK_BOT_LEAVE_SCRIPT,0x1e8e496c6d501d64, CommandNetworkBotLeaveScript);

	SCR_REGISTER_SECURE(NETWORK_KEEP_ENTITY_COLLISION_DISABLED_AFTER_ANIM_SCENE,0x4fe6b5a7ac60d232, CommandNetworkKeepEntityCollisionDisabledAfterAimScene);

#if !__FINAL
    // Network Soak tests
    SCR_REGISTER_UNUSED(NETWORK_SOAK_TEST_GET_NUM_PARAMETERS,0x7e22a2a7c4c0807c, CommandNetworkSoakTestGetNumParameters);
    SCR_REGISTER_UNUSED(NETWORK_SOAK_TEST_GET_INT_PARAMETER,0x8baf50c199d6fe3d, CommandNetworkSoakTestGetIntParameters);
#endif // !__FINAL

	SCR_REGISTER_SECURE(NETWORK_IS_ANY_PLAYER_NEAR,0x1c6bc905758240d5, CommandNetworkIsAnyPlayerNear);

	//
	//Clans	
	SCR_REGISTER_SECURE(NETWORK_CLAN_SERVICE_IS_VALID,0xf453de81bdfdae6f, CommandNetworkClanServiceIsValid);
	SCR_REGISTER_SECURE(NETWORK_CLAN_PLAYER_IS_ACTIVE,0x8c992447292d600f, CommandNetworkClanPlayerIsActive);
	SCR_REGISTER_SECURE(NETWORK_CLAN_PLAYER_GET_DESC,0x4ffbf2c0d8249e45, CommandNetworkClanPlayerGetDesc);
	SCR_REGISTER_SECURE(NETWORK_CLAN_IS_ROCKSTAR_CLAN,0x780a8bce22fa646b, CommandNetworkClanIsRockstarClan);
	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_UI_FORMATTED_TAG,0xaf42af0c81b996d0, CommandNetworkClanGetUiFormattedTag);
	SCR_REGISTER_UNUSED(NETWORK_CLAN_IS_MEMBERSHIP_SYNCHED,0xdb557cb3ab834f81, CommandNetworkClanIsMembershipSynched);

	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_LOCAL_MEMBERSHIPS_COUNT,0xbb7a0f7375123a7c, CommandNetworkClanGetLocalMembershipsCount);
	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_MEMBERSHIP_DESC,0xef45f91cee9ca825, CommandNetworkClanGetMembershipDesc);

	SCR_REGISTER_SECURE(NETWORK_CLAN_DOWNLOAD_MEMBERSHIP,0xf30425262d5f8ff0, CommandNetworkClanDownloadMembershipDesc);
	SCR_REGISTER_SECURE(NETWORK_CLAN_DOWNLOAD_MEMBERSHIP_PENDING,0x578851b712f5c31c, CommandNetworkClanDownloadMembershipDescPending);
	SCR_REGISTER_SECURE(NETWORK_CLAN_ANY_DOWNLOAD_MEMBERSHIP_PENDING,0xb32cd3b3788304e9, CommandNetworkClanAnyDownloadMembershipDescPending);
	SCR_REGISTER_SECURE(NETWORK_CLAN_REMOTE_MEMBERSHIPS_ARE_IN_CACHE,0x8ad3afcf48718c59, CommandNetworkClanRemoteMembersipsAlreadyDownloaded);
	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_MEMBERSHIP_COUNT,0x4a765cb3bad82b21, CommandNetworkClanGetMembershipCount);
	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_MEMBERSHIP_VALID,0x7bfa0b3f8046061d, CommandNetworkClanGetMembershipValid);
	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_MEMBERSHIP,0xff87c3c6fb687efd, CommandNetworkClanGetMembership);

	SCR_REGISTER_UNUSED(NETWORK_CLAN_GET_RECEIVED_INVITES_COUNT,0x9995b3eddd4afc5b, CommandNetworkClanGetReceivedInvitesCount);
	SCR_REGISTER_UNUSED(NETWORK_CLAN_GET_RECEIVED_INVITE_DATA,0x425a4f92f6ecc951, CommandNetworkClanGetClanInviteData);
	
	SCR_REGISTER_SECURE(NETWORK_CLAN_JOIN,0x6c0f53dcb3246608, CommandNetworkClanjoin);
	
	SCR_REGISTER_SECURE(NETWORK_CLAN_CREWINFO_GET_STRING_VALUE,0x8c2bfdacd7ed9a3b, CommandNetworkCrewInfoGetStringValue);
	SCR_REGISTER_UNUSED(NETWORK_CLAN_CREWINFO_GET_INT_VALUE,0x5e98ea58473e238c, CommandNetworkCrewInfoGetIntValue);
	SCR_REGISTER_SECURE(NETWORK_CLAN_CREWINFO_GET_CREWRANKTITLE,0x0b5da73f91f42f3a, CommandNetworkCrewInfoGetCrewRankTitle);
	
	SCR_REGISTER_SECURE(NETWORK_CLAN_HAS_CREWINFO_METADATA_BEEN_RECEIVED,0x8847c687db7d17e2, CommandNetworkClanMetadataReceived);

	SCR_REGISTER_SECURE(NETWORK_CLAN_GET_EMBLEM_TXD_NAME,0xf9518f925a4a1894, CommandNetworkClanGetPlayerClanEmblemTXDName);
	SCR_REGISTER_SECURE(NETWORK_CLAN_REQUEST_EMBLEM,0x31060c3326fab3cb, CommandNetworkClanRequestEmblem);
	SCR_REGISTER_SECURE(NETWORK_CLAN_IS_EMBLEM_READY,0xa2f783fae138e2f7, CommandNetworkClanIsEmblemReady);
	SCR_REGISTER_SECURE(NETWORK_CLAN_RELEASE_EMBLEM,0x57c0860a5387f6e1, CommandNetworkClanReleaseEmblem);

	SCR_REGISTER_UNUSED(NETWORK_CLAN_SET_LOCAL_CREWRANKTITLE_FOR_SYNC,0x77baaf26bb88b415, CommandNewtorkCrewSetLocalCrewRankTitleForSync);
	SCR_REGISTER_UNUSED(NETWORK_CLAN_GET_CREWRANKTITLE_FOR_PLAYER,0xd05cfb9feee1d213, CommandNetworkCrewGetCrewRankTitleForPlayer);

	SCR_REGISTER_UNUSED(NETWORK_RESET_PLAYER_DAMAGE_TICKER,0xbe3980d1bba7192d, CommandNetworkResetPlayerDamageTicker);

	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_CLEAR,0x0fa854f129d64245, CommandNetworkGetPrimaryClanDataClear);
	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_CANCEL,0x827a9c646775bb87, CommandNetworkGetPrimaryClanDataCancel);
	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_START,0xb3fd534ed90bc3ea, CommandNetworkGetPrimaryClanDataStart);
	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_PENDING,0xa0d4159bb7ca57e7, CommandNetworkGetPrimaryClanDataPending);
	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_SUCCESS,0x7a6139e192a01fc6, CommandNetworkGetPrimaryClanDataSuccess);
	SCR_REGISTER_UNUSED(NETWORK_GET_PRIMARY_CLAN_DATA,0xcb8d4709cce503ff, CommandNetworkGetPrimaryClanData);
	SCR_REGISTER_SECURE(NETWORK_GET_PRIMARY_CLAN_DATA_NEW,0x711a9ee5baf70932, CommandNetworkGetPrimaryClanDataNew);

	//////////////////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE(SET_NETWORK_ID_CAN_MIGRATE,0xbea5f528349f8f75, CommandSetNetworkIdCanMigrate);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_EXISTS_ON_ALL_MACHINES,0x4c6e9d70687fcdce, CommandSetNetworkIdExistsOnAllMachines);
    SCR_REGISTER_SECURE(SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER,0xaeba172874a3defc, CommandSetNetworkIdAlwaysExistsForPlayer);
	SCR_REGISTER_UNUSED(SET_NETWORK_ID_STOP_CLONING,0x7e95c57fe61e6c48, CommandSetNetworkIdStopCloning);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_CAN_BE_REASSIGNED,0xb3f9294b3d9498b4, CommandSetNetworkIdCanBeReassigned);
    SCR_REGISTER_UNUSED(SET_NETWORK_ID_CAN_BLEND,0x4cd26496757da3e3, CommandSetNetworkIdCanBlend);
	SCR_REGISTER_SECURE(NETWORK_SET_ENTITY_CAN_BLEND,0xf93b5f52f6861a80, CommandSetNetworkEntityCanBlend);
    SCR_REGISTER_SECURE(NETWORK_SET_OBJECT_CAN_BLEND_WHEN_FIXED,0x7c6641b13fa8a476, CommandSetNetworkObjectCanBlendWhenFixed);
	SCR_REGISTER_UNUSED(NETWORK_SET_ENTITY_REMAINS_WHEN_UNNETWORKED,0x69d2855a08b4c5bc, CommandSetNetworkEntityRemainsWhenUnnetworked);
	SCR_REGISTER_SECURE(NETWORK_SET_ENTITY_ONLY_EXISTS_FOR_PARTICIPANTS,0x229dd509edb2fbd4, CommandSetNetworkEntityOnlyExistsForParticipants);
	SCR_REGISTER_UNUSED(STOP_NETWORKING_ENTITY,0xb3995c3b42e62732, CommandStopNetworkingEntity);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_VISIBLE_IN_CUTSCENE,0x16cb19ec6f57d920, CommandSetNetworkIdVisibleInCutscene);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_HACK,0xa630ac70e7b02764, CommandSetNetworkIdVisibleInCutsceneHack);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_VISIBLE_IN_CUTSCENE_REMAIN_HACK,0x2b6a68d9acfe718e, CommandSetNetworkIdVisibleInCutsceneRemainHack);
	SCR_REGISTER_SECURE(SET_NETWORK_CUTSCENE_ENTITIES,0x96aa1b6a2212ecc4, CommandSetNetworkCutsceneEntities);
	SCR_REGISTER_SECURE(ARE_CUTSCENE_ENTITIES_NETWORKED,0x66d6a5e9c511214a, CommandAreCutsceneEntitiesNetworked);
	SCR_REGISTER_SECURE(SET_NETWORK_ID_PASS_CONTROL_IN_TUTORIAL,0x3673eb5a3c81ba8a, CommandSetNetworkIdPassControlInTutorial);
	SCR_REGISTER_SECURE(IS_NETWORK_ID_OWNED_BY_PARTICIPANT,0x3ba0a79b368faa6d, CommandIsNetworkIdOwnedByParticipant);
	SCR_REGISTER_UNUSED(SET_ALWAYS_CLONE_PLAYER_VEHICLE,0xfbacb5bdb38bd11c, CommandSetAlwaysClonePlayerVehicle);
	SCR_REGISTER_UNUSED(SET_REMOTE_PLAYER_VISIBLE_IN_CUTSCENE,0x33796CE71e8f4c75, CommandSetRemotePlayerVisibleInCutscene);
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_VISIBLE_IN_CUTSCENE,0x1555f2f21e8f4c75, CommandSetLocalPlayerVisibleInCutscene);
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_INVISIBLE_LOCALLY,0xe41d3e7a282e576d, CommandSetLocalPlayerInvisibleLocally);
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_VISIBLE_LOCALLY,0xb18e70e8229e0da6, CommandSetLocalPlayerVisibleLocally);
	SCR_REGISTER_UNUSED(RESET_LOCAL_PLAYER_REMOTE_VISIBILITY_OVERRIDE,0x6C4BC4029e0da6, CommandResetLocalPlayerRemoteVisibilityOverride);
	SCR_REGISTER_UNUSED(SET_LOCAL_PLAYER_INVISIBLE_REMOTELY,0xD3D3A6AE229e0da6, CommandSetLocalPlayerInvisibleRemotely);
	SCR_REGISTER_SECURE(SET_PLAYER_INVISIBLE_LOCALLY,0x75886f399fa24e61, CommandSetPlayerInvisibleLocally);
	SCR_REGISTER_SECURE(SET_PLAYER_VISIBLE_LOCALLY,0xb5f13d9832e1f2ae, CommandSetPlayerVisibleLocally);
	SCR_REGISTER_SECURE(FADE_OUT_LOCAL_PLAYER,0x5c53af694de011e8, CommandFadeOutLocalPlayer);
	SCR_REGISTER_SECURE(NETWORK_FADE_OUT_ENTITY,0xb970266897bdb48d, CommandNetworkFadeOutEntity);
	SCR_REGISTER_SECURE(NETWORK_FADE_IN_ENTITY,0x28271fbfa024090b, CommandNetworkFadeInEntity);
	SCR_REGISTER_UNUSED(NETWORK_FLASH_FADE_OUT_ENTITY_ADVANCED,0xa8c1b312724b9b8c, CommandNetworkFlashFadeOutEntityAdvanced);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_FADING,0xf7a68678b96e517c, CommandNetworkIsPlayerFading);
	SCR_REGISTER_SECURE(NETWORK_IS_ENTITY_FADING,0x3f1009a1e4e10c6a, CommandNetworkIsEntityFading);
	SCR_REGISTER_SECURE(IS_PLAYER_IN_CUTSCENE,0xd70f3341fff70270, CommandIsPlayerInCutscene);
	SCR_REGISTER_SECURE(SET_ENTITY_VISIBLE_IN_CUTSCENE,0x176a3f5786237102, CommandSetEntityVisibleInCutscene);
	SCR_REGISTER_SECURE(SET_ENTITY_LOCALLY_INVISIBLE,0xc2c78fe6cde262fe, CommandSetEntityLocallyInvisible);
	SCR_REGISTER_SECURE(SET_ENTITY_LOCALLY_VISIBLE,0x10ce349989cf554b, CommandSetEntityLocallyVisible);
	SCR_REGISTER_SECURE(IS_DAMAGE_TRACKER_ACTIVE_ON_NETWORK_ID,0x4426d33401c3dda7, CommandIsDamageTrackerActiveOnNetworkId);
	SCR_REGISTER_SECURE(ACTIVATE_DAMAGE_TRACKER_ON_NETWORK_ID,0x6f6515f8d9c6f573, CommandActivateDamageTrackerOnNetworkId);
	SCR_REGISTER_SECURE(IS_DAMAGE_TRACKER_ACTIVE_ON_PLAYER,0xe18ea3d7a6a0bca5, CommandIsDamageTrackerActiveOnPlayer);
	SCR_REGISTER_SECURE(ACTIVATE_DAMAGE_TRACKER_ON_PLAYER,0x53ab50c5a3c80ffe, CommandActivateDamageTrackerOnPlayer);
	SCR_REGISTER_SECURE(IS_SPHERE_VISIBLE_TO_ANOTHER_MACHINE,0x9a7f99fb89f853b2, CommandIsSphereVisibleToAnotherMachine);
	SCR_REGISTER_SECURE(IS_SPHERE_VISIBLE_TO_PLAYER,0x0809d748691dca79, CommandIsSphereVisibleToPlayer);
	SCR_REGISTER_UNUSED(IS_SPHERE_VISIBLE_TO_TEAM,0x34555a11b96d6b44, CommandIsSphereVisibleToTeam);
	SCR_REGISTER_SECURE(RESERVE_NETWORK_MISSION_OBJECTS,0x6f2e4667a6039155, CommandReserveNetworkMissionObjects);
	SCR_REGISTER_SECURE(RESERVE_NETWORK_MISSION_PEDS,0xf8b8a6edaa31f196, CommandReserveNetworkMissionPeds);
	SCR_REGISTER_SECURE(RESERVE_NETWORK_MISSION_VEHICLES,0xc1f83f3b5f8e7d3b, CommandReserveNetworkMissionVehicles);
	SCR_REGISTER_SECURE(RESERVE_LOCAL_NETWORK_MISSION_OBJECTS,0x793e115a707d0bf5, CommandReserveLocalNetworkMissionObjects);
	SCR_REGISTER_SECURE(RESERVE_LOCAL_NETWORK_MISSION_PEDS,0x597076e4da5ff2c8, CommandReserveLocalNetworkMissionPeds);
	SCR_REGISTER_SECURE(RESERVE_LOCAL_NETWORK_MISSION_VEHICLES,0x24f74da00e9a5f6f, CommandReserveLocalNetworkMissionVehicles);
	SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_OBJECTS,0x853ffa3d0a54864f, CommandCanRegisterMissionObjects);
	SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_PEDS,0xa777df215ccfcc77, CommandCanRegisterMissionPeds);
	SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_VEHICLES,0x993e56b8150c834f, CommandCanRegisterMissionVehicles);
    SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_PICKUPS,0xd5410f7a2dfbf144, CommandCanRegisterMissionPickups);
	SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_DOORS,0x8920a9ffccdd08a4, CommandCanRegisterMissionDoors);
    SCR_REGISTER_SECURE(CAN_REGISTER_MISSION_ENTITIES,0xaed41619adf56fa1, CommandCanRegisterMissionEntities);
	SCR_REGISTER_SECURE(GET_NUM_RESERVED_MISSION_OBJECTS,0xc162eec794cbb80b, CommandGetNumReservedMissionObjects);
	SCR_REGISTER_SECURE(GET_NUM_RESERVED_MISSION_PEDS,0x8736933282d0483c, CommandGetNumReservedMissionPeds);
	SCR_REGISTER_SECURE(GET_NUM_RESERVED_MISSION_VEHICLES,0xbd7b8099c8298d2f, CommandGetNumReservedMissionVehicles);
	SCR_REGISTER_SECURE(GET_NUM_CREATED_MISSION_OBJECTS,0xd768f3d3e0b175ba, CommandGetNumCreatedMissionObjects);
	SCR_REGISTER_SECURE(GET_NUM_CREATED_MISSION_PEDS,0xd2a20a5254d61849, CommandGetNumCreatedMissionPeds);
	SCR_REGISTER_SECURE(GET_NUM_CREATED_MISSION_VEHICLES,0x0659dfd69713128d, CommandGetNumCreatedMissionVehicles);
	SCR_REGISTER_UNUSED(GET_RESERVED_MISSION_ENTITIES_FOR_THREAD,0x33b7ba889fa48f98, CommandGetReservedMissionEntitiesForThread);
	SCR_REGISTER_SECURE(GET_RESERVED_MISSION_ENTITIES_IN_AREA,0xe3044344abe71c51, CommandGetReservedMissionEntitiesInArea);
    SCR_REGISTER_SECURE(GET_MAX_NUM_NETWORK_OBJECTS,0x18b73745d9f21827, CommandGetMaxNumNetworkObjects);
    SCR_REGISTER_SECURE(GET_MAX_NUM_NETWORK_PEDS,0x99e3ad53c601992b, CommandGetMaxNumNetworkPeds);
    SCR_REGISTER_SECURE(GET_MAX_NUM_NETWORK_VEHICLES,0x7ffc70fd55827ec2, CommandGetMaxNumNetworkVehicles);
	SCR_REGISTER_SECURE(GET_MAX_NUM_NETWORK_PICKUPS,0xa577826f8efd9e96, CommandGetMaxNumNetworkPickups);
	SCR_REGISTER_SECURE(NETWORK_SET_OBJECT_SCOPE_DISTANCE,0x75792f006b6dd7a3, CommandNetworkSetObjectScopeDistance);
	SCR_REGISTER_SECURE(NETWORK_ALLOW_CLONING_WHILE_IN_TUTORIAL,0x9c0542ceb9d1bdb9, CommandNetworkAllowCloningWhileInTutorial);
	SCR_REGISTER_SECURE(NETWORK_SET_TASK_CUTSCENE_INSCOPE_MULTIPLER, 0xC6FCEE21C6FCEE21, CommandNetworkSetTaskCutsceneInScopeMultipler);

	SCR_REGISTER_SECURE(GET_NETWORK_TIME,0x0a89fdfa763dcaed, CommandGetNetworkTime);
	SCR_REGISTER_SECURE(GET_NETWORK_TIME_ACCURATE,0xe75390f3ca208d5e, CommandGetNetworkTimeAccurate);
    SCR_REGISTER_UNUSED(GET_NETWORK_TIME_PAUSABLE,0xd5a4a73aa4f7c470, CommandGetNetworkTimePausable);
    SCR_REGISTER_SECURE(HAS_NETWORK_TIME_STARTED,0x75876798a41dd8fb, CommandHasNetworkClockStarted);
    SCR_REGISTER_UNUSED(STOP_NETWORK_TIME_PAUSABLE,0xcc3b71cbaf5e6d7f, CommandStopNetworkTimePausable);
    SCR_REGISTER_UNUSED(RESTART_NETWORK_TIME_PAUSABLE,0xcb300d54b68be87a, CommandRestartNetworkTimePausable);
    SCR_REGISTER_SECURE(GET_TIME_OFFSET,0x35de445e5254afed, CommandGetTimeOffset);
	SCR_REGISTER_SECURE(IS_TIME_LESS_THAN,0x516fe5ef30df734d, CommandIsTimeLessThan);
	SCR_REGISTER_SECURE(IS_TIME_MORE_THAN,0xe17ff5faec73b872, CommandIsTimeGreaterThan);
	SCR_REGISTER_SECURE(IS_TIME_EQUAL_TO,0x12b09bf1b251174f, CommandIsTimeEqualTo);
	SCR_REGISTER_SECURE(GET_TIME_DIFFERENCE,0x780a854e3a976a66, CommandGetTimeDifference);
	SCR_REGISTER_SECURE(GET_TIME_AS_STRING,0x441086029f1a02ac, CommandGetTimeAsString);
	SCR_REGISTER_SECURE(GET_CLOUD_TIME_AS_STRING,0x8546487b176b3a16, CommandGetCloudTimeAsString);
	SCR_REGISTER_SECURE(GET_CLOUD_TIME_AS_INT,0x48352343bc5a41ae, CommandGetCloudTimeAsInt);
	SCR_REGISTER_SECURE(CONVERT_POSIX_TIME,0x2351ad81ce3040e7, CommandConvertPosixTime);

    SCR_REGISTER_SECURE(NETWORK_SET_IN_SPECTATOR_MODE,0x86998d41f61b3824, CommandNetworkSetInSpectatorMode);
    SCR_REGISTER_SECURE(NETWORK_SET_IN_SPECTATOR_MODE_EXTENDED,0x1fd50aaf9da2199e, CommandNetworkSetInSpectatorModeExtended);
    SCR_REGISTER_SECURE(NETWORK_SET_IN_FREE_CAM_MODE,0x027db13bd5d494cc, CommandNetworkSetInFreeCamMode);
	SCR_REGISTER_SECURE(NETWORK_SET_ANTAGONISTIC_TO_PLAYER,0x88136d6f0b181ac5, CommandNetworkSetAntagonisticToPlayer);
    SCR_REGISTER_SECURE(NETWORK_IS_IN_SPECTATOR_MODE,0xd852449bf2e2ba8d, CommandNetworkIsInSpectatorMode);
	SCR_REGISTER_UNUSED(NETWORK_SET_IN_MP_TUTORIAL,0xf9cbd5d59932efc9, CommandNetworkSetInMPTutorial);
	SCR_REGISTER_SECURE(NETWORK_SET_IN_MP_CUTSCENE,0x3dc5858acd1e01cb, CommandNetworkSetInMPCutscene);
	SCR_REGISTER_SECURE(NETWORK_IS_IN_MP_CUTSCENE,0xc96a605cf3e9295b, CommandNetworkIsInMPCutscene);
	SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_IN_MP_CUTSCENE,0x6ecbf4ab299fabc8, CommandNetworkIsPlayerInMPCutscene);
	SCR_REGISTER_UNUSED(NETWORK_IS_IN_CODE_DEVELOPMENT_MODE,0xf4f4632825caad72, CommandNetworkIsInCodeDevelopmentMode);
	SCR_REGISTER_SECURE(NETWORK_HIDE_PROJECTILE_IN_CUTSCENE,0x93298603b7b45e44, CommandNetworkHideProjectileInCutscene);

	SCR_REGISTER_UNUSED(DISPLAY_PLAYER_NAME,0xcdd939a4b7919690, CommandDisplayPlayerName);
	SCR_REGISTER_UNUSED(BEGIN_DISPLAY_PLAYER_NAME,0xe754e91cb3249589, CommandBeginDisplayPlayerName);
	SCR_REGISTER_UNUSED(END_DISPLAY_PLAYER_NAME,0xb8890aa016b7414b, CommandEndDisplayPlayerName);
	SCR_REGISTER_UNUSED(DISPLAY_PLAYER_NAME_HAS_FREE_ROWS,0x65d9264e3fe41012, CommandDisplayPlayerNameHasFreeRows);

	SCR_REGISTER_SECURE(SET_NETWORK_VEHICLE_RESPOT_TIMER,0x1da01ba2f8b68b2c, CommandSetNetworkVehicleRespotTimer);
	SCR_REGISTER_UNUSED(IS_NETWORK_VEHICLE_RUNNING_RESPOT_TIMER,0x63c922a179fe1872, CommandIsNetworkVehicleRunningRespotTimer);
	SCR_REGISTER_SECURE(SET_NETWORK_VEHICLE_AS_GHOST,0xe06a9b86c360e754, CommandSetNetworkVehicleAsGhost);
	SCR_REGISTER_SECURE(SET_NETWORK_VEHICLE_MAX_POSITION_DELTA_MULTIPLIER,0x17e509655d220fc4, CommandNetworkSetMaxPositionDeltaMultiplier);
	SCR_REGISTER_SECURE(SET_NETWORK_ENABLE_HIGH_SPEED_EDGE_FALL_DETECTION,0xd221ac9d6c50c068, CommandNetworkEnableHighSpeedEdgeFallDetection);
	
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_AS_GHOST,0x6ca6298e3e0086f4, CommandSetLocalPlayerAsGhost);
	SCR_REGISTER_SECURE(IS_ENTITY_A_GHOST,0x1843cf59bb5efd96, CommandIsEntityAGhost);
	SCR_REGISTER_SECURE(SET_NON_PARTICIPANTS_OF_THIS_SCRIPT_AS_GHOSTS,0x42c38ac3d109cfe1, CommandSetNonParticipantsOfThisScriptAsGhosts);
	SCR_REGISTER_SECURE(SET_REMOTE_PLAYER_AS_GHOST,0x6870fb27b885e344, CommandSetRemotePlayerAsGhost);
	SCR_REGISTER_SECURE(SET_GHOST_ALPHA,0xde2669f371bb4158, CommandSetGhostAlpha);
	SCR_REGISTER_SECURE(RESET_GHOST_ALPHA,0x7d11369301267ef0, CommandResetGhostAlpha);
	SCR_REGISTER_SECURE(SET_ENTITY_GHOSTED_FOR_GHOST_PLAYERS,0xbcc4fa7ba265ff12, CommandSetEntityGhostedForGhostPlayers);
	SCR_REGISTER_UNUSED(SET_PARACHUTE_OF_ENTITY_GHOSTED_FOR_GHOST_PLAYERS,0x3c09bc347f7b67c6, CommandSetParachuteOfEntityGhostedForGhostPlayers);	
	SCR_REGISTER_SECURE(SET_INVERT_GHOSTING,0xe6345a881f643908, CommandSetInvertGhosting);
	SCR_REGISTER_SECURE(IS_ENTITY_IN_GHOST_COLLISION,0x0bf4b1193e541c22, CommandIsEntityInGhostCollision);

    SCR_REGISTER_UNUSED(SET_MSG_FOR_LOADING_SCREEN,0x25bc49d8a4ef820c, CommandSetMsgForLoadingScreen);

    SCR_REGISTER_UNUSED(IS_OBJECT_REASSIGNMENT_IN_PROGRESS,0xc6c2ffff48ce1bcd, CommandIsObjectReassignmentInProgress);

    SCR_REGISTER_UNUSED(SAVE_SCRIPT_ARRAY_IN_SCRATCHPAD,0xe146869590d33a8d, CommandSaveScriptArrayInScratchPad);
    SCR_REGISTER_UNUSED(RESTORE_SCRIPT_ARRAY_FROM_SCRATCHPAD,0x01acb610a3df0017, CommandRestoreScriptArrayFromScratchPad);
    SCR_REGISTER_UNUSED(CLEAR_SCRIPT_ARRAY_FROM_SCRATCHPAD,0x2cf3280811ca9ad9, CommandClearScriptArrayFromScratchPad);

	SCR_REGISTER_UNUSED(SAVE_SCRIPT_ARRAY_IN_PLAYER_SCRATCHPAD,0x22fe2d126238a297, CommandSaveScriptArrayInPlayerScratchPad);
	SCR_REGISTER_UNUSED(RESTORE_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD,0xefffdbaf3c83eb0b, CommandRestoreScriptArrayFromPlayerScratchPad);
	SCR_REGISTER_UNUSED(CLEAR_SCRIPT_ARRAY_FROM_PLAYER_SCRATCHPAD,0x3228141a1cf4891b, CommandClearScriptArrayFromPlayerScratchPad);
	SCR_REGISTER_UNUSED(CLEAR_SCRIPT_ARRAY_FROM_ALL_PLAYER_SCRATCHPADS,0x4b53e9ccbe717ec6, CommandClearScriptArrayFromAllPlayerScratchPads);

	SCR_REGISTER_SECURE(USE_PLAYER_COLOUR_INSTEAD_OF_TEAM_COLOUR,0x14d30880e9487ecb, CommandUsePlayerColourInsteadOfTeamColour);

	SCR_REGISTER_UNUSED(NETWORK_PARAM_NETSTARTPOS_EXISTS,0x983b8a6217bc2f23, CommandNetworkParamNetStartPosExists);
	SCR_REGISTER_UNUSED(NETWORK_GET_PARAM_NETSTARTPOS,0x9a9dd0a4195aba58, CommandNetworkGetParamNetStartPos);
	SCR_REGISTER_UNUSED(NETWORK_PARAM_TEAM_EXISTS,0x5a43bdd288977f2a, CommandNetworkParamTeamExists);
	SCR_REGISTER_UNUSED(NETWORK_GET_PARAM_TEAM,0xd1f58c125fea8bd2, CommandNetworkGetParamTeam);

    SCR_REGISTER_UNUSED(NET_DEBUG,0x73ec056a618d7b77, CommandNetDebug);
    SCR_REGISTER_UNUSED(NET_DEBUG_INT,0x0c03ab85c407d127, CommandNetDebug_i);
    SCR_REGISTER_UNUSED(NET_DEBUG_FLOAT,0xd9d74623f2981726, CommandNetDebug_f);
    SCR_REGISTER_UNUSED(NET_DEBUG_VECTOR,0x20ffe9d92a4fb4cb, CommandNetDebug_v);
    SCR_REGISTER_UNUSED(NET_DEBUG_STRING,0x99cb1d222c496a89, CommandNetDebug_s);
    SCR_REGISTER_UNUSED(NET_WARNING,0xa30d8b9a5b97688b, CommandNetWarning);
    SCR_REGISTER_UNUSED(NET_WARNING_INT,0x40c64d5db315586d, CommandNetWarning_i);
    SCR_REGISTER_UNUSED(NET_WARNING_FLOAT,0xe88a89b5b3438670, CommandNetWarning_f);
    SCR_REGISTER_UNUSED(NET_WARNING_VECTOR,0x9df1fb62f55e8789, CommandNetWarning_v);
    SCR_REGISTER_UNUSED(NET_WARNING_STRING,0xb51e31274596e7d2, CommandNetWarning_s);
    SCR_REGISTER_UNUSED(NET_ERROR,0x0e432b96db79bf29, CommandNetError);
    SCR_REGISTER_UNUSED(NET_ERROR_INT,0x9d2d39a40063151c, CommandNetError_i);
    SCR_REGISTER_UNUSED(NET_ERROR_FLOAT,0xf97f9e5a3a80e344, CommandNetError_f);
    SCR_REGISTER_UNUSED(NET_ERROR_VECTOR,0xedd92d9bd774274b, CommandNetError_v);
    SCR_REGISTER_UNUSED(NET_ERROR_STRING,0x77642b4aca2744c4, CommandNetError_s);
	SCR_REGISTER_UNUSED(NETWORK_GET_FOCUS_PED_LOCAL_ID,0x90aa771702364404, CommandNetworkGetFocusPedLocalId);
	SCR_REGISTER_UNUSED(NETWORK_IS_PLAYER_VISIBLE,0x7d465e8e800fdba9, CommandNetworkIsPlayerVisible);

	SCR_REGISTER_UNUSED(IS_HEADSHOT_TRACKING_ACTIVE,0xda5d310f3f00e4a7, CommandIsHeadShotTrackingActive);
    SCR_REGISTER_UNUSED(START_HEADSHOT_TRACKING,0x006bd7eef261c86b, CommandStartHeadShotTracking);
    SCR_REGISTER_UNUSED(STOP_HEADSHOT_TRACKING,0xbdc6b06fe2b01b34, CommandStopHeadShotTracking);
    SCR_REGISTER_UNUSED(GET_HEADSHOT_TRACKING_RESULTS,0x3eb663b8765a0bc1, CommandGetHeadShotTrackingResults);
    SCR_REGISTER_UNUSED(RESET_HEADSHOT_TRACKING_RESULTS,0xae537696e7a2fe18, CommandResetHeadShotTrackingResults);

	SCR_REGISTER_UNUSED(IS_KILL_TRACKING_ACTIVE,0xf0392be8ff226ddb, CommandIsKillTrackingActive);
    SCR_REGISTER_UNUSED(START_KILL_TRACKING,0x2eea39c146024d2a, CommandStartKillTracking);
    SCR_REGISTER_UNUSED(STOP_KILL_TRACKING,0x4ea095a3060d47d7, CommandStopKillTracking);
    SCR_REGISTER_UNUSED(GET_KILL_TRACKING_RESULTS,0x91defd631d3a5d57, CommandGetKillTrackingResults);
    SCR_REGISTER_UNUSED(RESET_KILL_TRACKING_RESULTS,0xb857a5dca39dea34, CommandResetKillTrackingResults);
    SCR_REGISTER_UNUSED(REGISTER_MODEL_FOR_RANK_POINTS,0x6aecd7ab54a56ce8, CommandRegisterModelForRankPoints);
    SCR_REGISTER_UNUSED(GET_NUM_KILLS_FOR_RANK_POINTS,0x0ab7d25707702977, CommandGetNumKillsForRankPoints);
    SCR_REGISTER_UNUSED(RESET_NUM_KILLS_FOR_RANK_POINTS,0x293f644ecaae7cb6, CommandResetNumKillsForRankPoints);
    SCR_REGISTER_UNUSED(START_TRACKING_CIVILIAN_KILLS,0x178aefd2b12744e5, CommandStartTrackingCivilianKills);
    SCR_REGISTER_UNUSED(STOP_TRACKING_CIVILIAN_KILLS,0xb0ce936956432ac5, CommandStopTrackingCivilianKills);
    SCR_REGISTER_UNUSED(ARE_CIVILIAN_KILLS_BEING_TRACKED,0xd4311d782cb1f42f, CommandAreCivilianKillsBeingTracked);
    SCR_REGISTER_UNUSED(GET_NUM_CIVILIAN_KILLS,0xce798fbc830114b8, CommandGetNumCivilianKills);
    SCR_REGISTER_UNUSED(RESET_NUM_CIVILIAN_KILLS,0x6d0885e6fa22cd61, CommandResetNumCivilianKills);
	
	SCR_REGISTER_UNUSED(START_TRACKING_PED_KILLS,0xb380139c3610e57b, CommandStartTrackingPedKills);
	SCR_REGISTER_UNUSED(STOP_TRACKING_PED_KILLS,0xed4b3cae79f223b8, CommandStopTrackingPedKills);
	SCR_REGISTER_UNUSED(ARE_PED_KILLS_BEING_TRACKED,0xa4f353ae88373449, CommandArePedKillsBeingTracked);
	SCR_REGISTER_UNUSED(GET_NUM_PED_KILLS,0xeb00aa5a8b2bb449, CommandGetNumPedKills);
	SCR_REGISTER_UNUSED(RESET_NUM_PED_KILLS,0x9ab030608106e298, CommandResetNumPedKills);

    SCR_REGISTER_UNUSED(ADD_FAKE_PLAYER_NAME,0x61bfa77dd2f1513b, CommandAddFakePlayerName);
    SCR_REGISTER_UNUSED(REMOVE_FAKE_PLAYER_NAME,0x1961e98bc68f36cc, CommandRemoveFakePlayerName);
    SCR_REGISTER_UNUSED(REMOVE_ALL_FAKE_PLAYER_NAMES,0xf781984e87e5bd73, CommandRemoveAllFakePlayerNames);
    SCR_REGISTER_UNUSED(DOES_PED_HAVE_FAKE_PLAYER_NAME,0xcbab6983b095f9eb, CommandDoesPedHaveFakePlayerName);
    SCR_REGISTER_UNUSED(GET_NUM_FAKE_PLAYER_NAMES,0x567fd417e3dc8ea5, CommandGetNumFakePlayerNames);

    SCR_REGISTER_UNUSED(NETWORK_GET_NETWORK_ID_FROM_ROPE_ID,0x76a68f0871651192, CommandNetworkGetNetworkIDFromRopeID);
    SCR_REGISTER_UNUSED(NETWORK_GET_ROPE_ID_FROM_NETWORK_ID,0x5e4474c4f41d0b9f, CommandNetworkGetRopeIDFromNetworkID);

    SCR_REGISTER_SECURE(NETWORK_CREATE_SYNCHRONISED_SCENE,0x497e09037a63d346, CommandNetworkCreateSynchronisedScene);
    SCR_REGISTER_SECURE(NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE,0xff47c397a9269a1a, CommandNetworkAddPedToSynchronisedScene);
    SCR_REGISTER_SECURE(NETWORK_ADD_PED_TO_SYNCHRONISED_SCENE_WITH_IK,0xbd37dcd8f01f7304, CommandNetworkAddPedToSynchronisedSceneWithIK);
    SCR_REGISTER_SECURE(NETWORK_ADD_ENTITY_TO_SYNCHRONISED_SCENE,0xf6ac18061d64c197, CommandNetworkAddEntityToSynchronisedScene);
	SCR_REGISTER_SECURE(NETWORK_ADD_MAP_ENTITY_TO_SYNCHRONISED_SCENE,0x20cec04bd78f2a17, CommandsNetworkAddMapEntityToSynchronisedScene);
    SCR_REGISTER_SECURE(NETWORK_ADD_SYNCHRONISED_SCENE_CAMERA,0x2424cfd501cdb1b4, CommandNetworkAddSynchronisedSceneCamera);
    SCR_REGISTER_SECURE(NETWORK_ATTACH_SYNCHRONISED_SCENE_TO_ENTITY,0xc88dd27ea053f0d3, CommandNetworkAttachSynchronisedSceneToEntity);
    SCR_REGISTER_SECURE(NETWORK_START_SYNCHRONISED_SCENE,0x60e9dd146055c13e, CommandNetworkStartSynchronisedScene);
    SCR_REGISTER_SECURE(NETWORK_STOP_SYNCHRONISED_SCENE,0x0b92578fb47da084, CommandNetworkStopSynchronisedScene);
    SCR_REGISTER_SECURE(NETWORK_GET_LOCAL_SCENE_FROM_NETWORK_ID,0x57736a7b8965a88a, CommandNetworkGetLocalSceneIDFromNetworkID);
    SCR_REGISTER_SECURE(NETWORK_FORCE_LOCAL_USE_OF_SYNCED_SCENE_CAMERA,0x139b73ed8532309a, CommandNetworkForceLocalUseOfSyncedSceneCamera);
    SCR_REGISTER_SECURE(NETWORK_ALLOW_REMOTE_SYNCED_SCENE_LOCAL_PLAYER_REQUESTS,0xb7a821850d99237b, CommandNetworkAllowRemoteSyncSceneLocalPlayerRequests);

    SCR_REGISTER_SECURE(NETWORK_FIND_LARGEST_BUNCH_OF_PLAYERS,0x0535942f1f26d363, CommandNetworkFindLargestBunchOfPlayers);

	SCR_REGISTER_UNUSED(NETWORK_START_RESPAWN_SEARCH,0x40bbace1068c49a6, CommandNetworkStartRespawnSearch);
	SCR_REGISTER_SECURE(NETWORK_START_RESPAWN_SEARCH_FOR_PLAYER,0x93946e27b42aa58e, CommandNetworkStartRespawnSearchForPlayer);
	SCR_REGISTER_SECURE(NETWORK_START_RESPAWN_SEARCH_IN_ANGLED_AREA_FOR_PLAYER,0x20139897f77c3b3f, CommandNetworkStartRespawnSearchInAngledAreaForPlayer);
	SCR_REGISTER_UNUSED(NETWORK_START_RESPAWN_SEARCH_FOR_TEAM,0xeb3d3fb61e0e8798, CommandNetworkStartRespawnSearchForTeam);
	SCR_REGISTER_SECURE(NETWORK_QUERY_RESPAWN_RESULTS,0xfcb1dfefd6237222, CommandNetworkQueryRespawnResults);
	SCR_REGISTER_SECURE(NETWORK_CANCEL_RESPAWN_SEARCH,0x8caea922d207d693, CommandNetworkCancelRespawnSearch);
	SCR_REGISTER_SECURE(NETWORK_GET_RESPAWN_RESULT,0x95cfc066b227f10d, CommandNetworkGetRespawnResult);
	SCR_REGISTER_SECURE(NETWORK_GET_RESPAWN_RESULT_FLAGS,0x821cedaf62c8b557, CommandNetworkGetRespawnResultFlags);

    SCR_REGISTER_SECURE(NETWORK_START_SOLO_TUTORIAL_SESSION,0x5ddf0eb4876633fc, CommandNetworkStartSoloTutorialSession);
    SCR_REGISTER_SECURE(NETWORK_ALLOW_GANG_TO_JOIN_TUTORIAL_SESSION,0x2401ff328f02f86c, CommandNetworkAllowGangToJoinTutorialSession);
    SCR_REGISTER_SECURE(NETWORK_END_TUTORIAL_SESSION,0x68ca8f8b95ddea66, CommandNetworkEndTutorialSession);
    SCR_REGISTER_SECURE(NETWORK_IS_IN_TUTORIAL_SESSION,0x0843570206f71f38, CommandNetworkIsInTutorialSession);
	SCR_REGISTER_SECURE(NETWORK_WAITING_POP_CLEAR_TUTORIAL_SESSION,0x055278a6247d56c0, CommandNetworkIsWaitingPopulationClearedInTutorialSession);
	SCR_REGISTER_SECURE(NETWORK_IS_TUTORIAL_SESSION_CHANGE_PENDING,0xdd19976f3302fb36, CommandNetworkIsTutorialSessionChangePending);
    SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_TUTORIAL_SESSION_INSTANCE,0xc9aa2d3128922353, CommandNetworkGetPlayerTutorialSessionInstance);
    SCR_REGISTER_SECURE(NETWORK_ARE_PLAYERS_IN_SAME_TUTORIAL_SESSION,0xa9e699d3dc7c0b15, CommandNetworkArePlayersInSameTutorialSession);
	SCR_REGISTER_SECURE(NETWORK_BLOCK_PROXY_MIGRATION_BETWEEN_TUTORIAL_SESSIONS,0xfea7a352ddb34d52, CommandBlockProxyMigrationBetweenTutorialSessions);

    SCR_REGISTER_SECURE(NETWORK_CONCEAL_PLAYER,0xc6f4f426f21b03ea, CommandNetworkConcealPlayer);
    SCR_REGISTER_SECURE(NETWORK_IS_PLAYER_CONCEALED,0x4fe8fd62895d0a5a, CommandNetworkIsPlayerConcealed);

	SCR_REGISTER_SECURE(NETWORK_CONCEAL_ENTITY,0x8863e3cffcaed924, CommandNetworkConcealEntity);
	SCR_REGISTER_SECURE(NETWORK_IS_ENTITY_CONCEALED,0x8015438625493270, CommandNetworkIsEntityConcealed);

    SCR_REGISTER_SECURE(NETWORK_OVERRIDE_CLOCK_TIME,0x42ea11f7821d9466, CommandNetworkOverrideClockTime);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_CLOCK_RATE,0x28357fa46a5896e7, CommandNetworkOverrideClockRate);
    SCR_REGISTER_SECURE(NETWORK_CLEAR_CLOCK_TIME_OVERRIDE,0x5f25f942f5f8ef3e, CommandNetworkClearClockTimeOverride);
    SCR_REGISTER_UNUSED(NETWORK_SYNC_CLOCK_TIME_OVERRIDE,0x944ed54b66cfb2bf, CommandNetworkSyncClockTimeOverride);
    SCR_REGISTER_SECURE(NETWORK_IS_CLOCK_TIME_OVERRIDDEN,0xf95eec86c216952d, CommandNetworkIsClockTimeOverridden);
	SCR_REGISTER_UNUSED(NETWORK_SET_CLOCK_OVERRIDE_LOCKED,0x0929cb201fbea500, CommandNetworkSetClockOverrideLocked);
	SCR_REGISTER_UNUSED(NETWORK_GET_GLOBAL_CLOCK,0x9080789dc6e47038, CommandNetworkGetGlobalClock);

	SCR_REGISTER_SECURE(NETWORK_ADD_ENTITY_AREA,0xd3077b6d45cd100e, CommandNetworkAddEntityArea);
	SCR_REGISTER_SECURE(NETWORK_ADD_ENTITY_ANGLED_AREA,0xc2e24b2aedcc8a1a, CommandNetworkAddEntityAngledArea);
    SCR_REGISTER_SECURE(NETWORK_ADD_CLIENT_ENTITY_AREA,0x198d8d2abc1b1c9b, CommandNetworkAddClientEntityArea);
    SCR_REGISTER_SECURE(NETWORK_ADD_CLIENT_ENTITY_ANGLED_AREA,0x47ddb3f2bd4cb6fa, CommandNetworkAddClientEntityAngledArea);
	SCR_REGISTER_SECURE(NETWORK_REMOVE_ENTITY_AREA,0x6e248b3e8cd8823c, CommandNetworkRemoveEntityArea);
	SCR_REGISTER_SECURE(NETWORK_ENTITY_AREA_DOES_EXIST,0xd0c6888814987992, CommandNetworkEntityAreaDoesExist);
	SCR_REGISTER_SECURE(NETWORK_ENTITY_AREA_HAVE_ALL_REPLIED,0x1c2607b54ddc8b70, CommandNetworkEntityAreaHaveAllReplied);
	SCR_REGISTER_UNUSED(NETWORK_ENTITY_AREA_HAS_PLAYER_REPLIED,0x876bb218c32cb935, CommandNetworkEntityAreaHasPlayerReplied);
	SCR_REGISTER_SECURE(NETWORK_ENTITY_AREA_IS_OCCUPIED,0x39d52f5cdfc288e7, CommandNetworkIsEntityAreaOccupied);
	SCR_REGISTER_UNUSED(NETWORK_ENTITY_AREA_IS_OCCUPIED_ON,0xc506fb9c49d2fc35, CommandNetworkIsEntityAreaOccupiedOn);
	SCR_REGISTER_SECURE(NETWORK_USE_HIGH_PRECISION_BLENDING,0x4b3731cb7c1cf225, CommandNetworkUseHighPrecisionBlending);
    SCR_REGISTER_SECURE(NETWORK_SET_CUSTOM_ARENA_BALL_PARAMS,0x74c6fdcb43b8233e, CommandNetworkSetCustomArenaBallParams);
    SCR_REGISTER_SECURE(NETWORK_ENTITY_USE_HIGH_PRECISION_ROTATION,0x68b190dbcb3bf614, CommandNetworkEntityUseHighPrecisionRotation);

	SCR_REGISTER_SECURE(NETWORK_REQUEST_CLOUD_BACKGROUND_SCRIPTS,0x2d1a01516592199a, CommandNetworkRequestCloudBackgroundScript);
	SCR_REGISTER_UNUSED(NETWORK_GET_CLOUD_BACKGROUND_SCRIPT_MODIFIED_TIME,0x4f301abf8d9fd7c4, CommandNetworkGetCloudBackgroundScriptModifiedTime);
	SCR_REGISTER_SECURE(NETWORK_IS_CLOUD_BACKGROUND_SCRIPT_REQUEST_PENDING,0x049d6c87562c80cd, CommandNetworkIsBackgroundScriptCloudRequestPending);

	SCR_REGISTER_SECURE(NETWORK_REQUEST_CLOUD_TUNABLES,0x24da28ab3b6ab1da, CommandNetworkRequestCloudTunables);
	SCR_REGISTER_SECURE(NETWORK_IS_TUNABLE_CLOUD_REQUEST_PENDING,0x7072a36933a02b3f, CommandNetworkIsTunablesCloudRequestPending);
	SCR_REGISTER_SECURE(NETWORK_GET_TUNABLE_CLOUD_CRC,0x2844e705662ae9e3, CommandNetworkGetTunableCloudCRC);

    SCR_REGISTER_SECURE(NETWORK_DOES_TUNABLE_EXIST,0xbd821152194e690f, CommandNetworkDoesTunableExist_Deprecated);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_INT,0xee6ab2ca90b5b970, CommandNetworkAccessTunableInt_Deprecated);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_FLOAT,0xd12fb7896e13d186, CommandNetworkAccessTunableFloat_Deprecated);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_BOOL,0xea13158bcb052874, CommandNetworkAccessTunableBool_Deprecated);
    SCR_REGISTER_UNUSED(NETWORK_TRY_ACCESS_TUNABLE_INT,0xd838202c3e540741, CommandNetworkTryAccessTunableInt_Deprecated);
    SCR_REGISTER_UNUSED(NETWORK_TRY_ACCESS_TUNABLE_FLOAT,0x2c9f1e6d5e7c052f, CommandNetworkTryAccessTunableFloat_Deprecated);
    SCR_REGISTER_UNUSED(NETWORK_TRY_ACCESS_TUNABLE_BOOL,0x39916fb617c155c0, CommandNetworkTryAccessTunableBool_Deprecated);
    SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_INT,0x9031eef364d8b864, CommandNetworkInsertTunableInt_Deprecated);
	SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_FLOAT,0x9da0510ca44c1ce7, CommandNetworkInsertTunableFloat_Deprecated);
	SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_BOOL,0x9495de94d72445f7, CommandNetworkInsertTunableBool_Deprecated);

	SCR_REGISTER_SECURE(NETWORK_DOES_TUNABLE_EXIST_HASH,0x9141a9af147a1ef7, CommandNetworkDoesTunableExist);

	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_MODIFICATION_DETECTION_CLEAR,0xe195a10a5d4be5a1, CommandNetworkAccessTunableModificationDetectionClear);

	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_INT_HASH,0x79b0a08580eba74c, CommandNetworkAccessTunableInt);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_INT_MODIFICATION_DETECTION_REGISTRATION_HASH,0xc5d08122ec634870, CommandNetworkAccessTunableIntModificationDetectionRegistration);

	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_FLOAT_HASH,0xa0d79393a2e01ed3, CommandNetworkAccessTunableFloat);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_FLOAT_MODIFICATION_DETECTION_REGISTRATION_HASH,0x0d334de6bb80ccea, CommandNetworkAccessTunableFloatModificationDetectionRegistration);

	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_BOOL_HASH,0x836cfc11d9e48985, CommandNetworkAccessTunableBool);
	SCR_REGISTER_SECURE(NETWORK_ACCESS_TUNABLE_BOOL_MODIFICATION_DETECTION_REGISTRATION_HASH,0x3a4da2bf180493ee, CommandNetworkAccessTunableBoolModificationDetectionRegistration);

	SCR_REGISTER_UNUSED(NETWORK_TRY_ACCESS_TUNABLE_INT_HASH,0xcfba9e73530de0f7, CommandNetworkTryAccessTunableInt);
	SCR_REGISTER_UNUSED(NETWORK_TRY_ACCESS_TUNABLE_FLOAT_HASH,0x849fb3f6376b2be8, CommandNetworkTryAccessTunableFloat);
	SCR_REGISTER_SECURE(NETWORK_TRY_ACCESS_TUNABLE_BOOL_HASH,0xbb4d0f1297c41e2f, CommandNetworkTryAccessTunableBool);
	SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_INT_HASH,0xefbdd2e953945183, CommandNetworkInsertTunableInt);
	SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_FLOAT_HASH,0x68f0bc3a808c181e, CommandNetworkInsertTunableFloat);
	SCR_REGISTER_UNUSED(NETWORK_INSERT_TUNABLE_BOOL_HASH,0x30e793930ca600e3, CommandNetworkInsertTunableBool);

    SCR_REGISTER_SECURE(NETWORK_GET_CONTENT_MODIFIER_LIST_ID,0x28f06ebd3cac9b0a, CommandNetworkGetContentModifierListID);

    SCR_REGISTER_UNUSED(NETWORK_GET_PLAYER_CODE_DEBUG_ID,0x5e95fe793d22ed20, CommandNetworkGetPlayerNetworkObjectID);

	SCR_REGISTER_UNUSED(NETWORK_HAS_BONE_BEEN_HIT,0x0c4aef0f28b2a78e, CommandNetworkHasBoneBeenHit);
	SCR_REGISTER_SECURE(NETWORK_GET_BONE_ID_OF_FATAL_HIT,0xac57be480ee4bf62, CommandNetworkGetBoneIdOfFatalHit);
	SCR_REGISTER_SECURE(NETWORK_RESET_BODY_TRACKER,0x56f781bb73fb147f, CommandNetworkResetBodyTracker);
	SCR_REGISTER_SECURE(NETWORK_GET_NUMBER_BODY_TRACKER_HITS,0x4b64b165b5c876a4, CommandNetworkGetNumberBodyTrackerHits);
	SCR_REGISTER_SECURE(NETWORK_HAS_BONE_BEEN_HIT_BY_KILLER,0x45094204ed611a06, CommandNetworkHasBoneBeenHitByKiller);

	SCR_REGISTER_SECURE(NETWORK_SET_ATTRIBUTE_DAMAGE_TO_PLAYER,0x0f1f0c82fa94500f, CommandNetworkSetAttributeDamageToPlayer);
	SCR_REGISTER_UNUSED(NETWORK_GET_ATTRIBUTE_DAMAGE_TO_PLAYER,0xd5ccb1fedf2b38b3, CommandNetworkGetAttributeDamageToPlayer);

	SCR_REGISTER_SECURE(NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_DAMAGE,0xe68484a03fb77128, CommandsNetworkTriggerDamageEventForZeroDamage);
	SCR_REGISTER_SECURE(NETWORK_TRIGGER_DAMAGE_EVENT_FOR_ZERO_WEAPON_HASH,0x077379c7e4ea63d3, CommandsNetworkTriggerDamageEventForZeroWeaponHash);

	SCR_REGISTER_UNUSED(NETWORK_GET_XUID_FROM_GAMERTAG,0x6377cd9a3d9e25af, CommandNetworkGetXUIDFromGamertag);

	SCR_REGISTER_SECURE(NETWORK_SET_NO_LONGER_NEEDED,0x5beaad56e722f32e, CommandNetworkSetNoLongerNeeded);
	SCR_REGISTER_UNUSED(NETWORK_GET_NO_LONGER_NEEDED,0x97cbdf816bf9f4a4, CommandNetworkGetNoLongerNeeded);

	SCR_REGISTER_SECURE(NETWORK_EXPLODE_VEHICLE,0x04375aabe1be38ae, CommandNetworkExplodeVehicle);
	SCR_REGISTER_SECURE(NETWORK_EXPLODE_HELI,0x5d0fecef9219b83c, CommandNetworkExplodeHeli);

    SCR_REGISTER_SECURE(NETWORK_USE_LOGARITHMIC_BLENDING_THIS_FRAME,0x624ff48c5cd7ce25, CommandNetworkUseLogarithmicBlendingThisFrame);
	SCR_REGISTER_SECURE(NETWORK_OVERRIDE_COORDS_AND_HEADING,0xbf129fbfc0575920, CommandNetworkOverrideCoordsAndHeading);
	SCR_REGISTER_UNUSED(NETWORK_OVERRIDE_BLENDER_FOR_TIME,0x45eac26960636ade, CommandNetworkOverrideBlenderForTime);
    SCR_REGISTER_SECURE(NETWORK_ENABLE_EXTRA_VEHICLE_ORIENTATION_BLEND_CHECKS,0xb226a538eb9367e1, CommandNetworkEnableExtraVehicleOrientationBlendChecks);
	SCR_REGISTER_SECURE(NETWORK_DISABLE_PROXIMITY_MIGRATION,0x74ca2009185992ec, CommandNetworkDisableProximityMigration);

    SCR_REGISTER_UNUSED(NETWORK_SET_VEHICLE_GARAGE_INDEX,0x458c49b0799610b4, CommandNetworkSetVehicleGarageIndex);
    SCR_REGISTER_UNUSED(NETWORK_CLEAR_VEHICLE_GARAGE_INDEX,0xa7b51ba39ecd82dc, CommandNetworkClearVehicleGarageIndex);
    SCR_REGISTER_UNUSED(NETWORK_SET_PLAYER_GARAGE_INDEX,0x782c4258a29094d2, CommandNetworkSetPlayerGarageIndex);
    SCR_REGISTER_UNUSED(NETWORK_CLEAR_PLAYER_GARAGE_INDEX,0x7e867180cb9f5244, CommandNetworkClearPlayerGarageIndex);

	SCR_REGISTER_SECURE(NETWORK_SET_PROPERTY_ID,0x71e6b83029935f72, CommandNetworkSetPropertyID);
	SCR_REGISTER_SECURE(NETWORK_CLEAR_PROPERTY_ID,0x41d7a90359f93d19, CommandNetworkClearPropertyID);

	SCR_REGISTER_SECURE(NETWORK_SET_PLAYER_MENTAL_STATE,0xd9dc868e619ea7fd, CommandNetworkSetPlayerMentalState);
	SCR_REGISTER_SECURE(NETWORK_SET_MINIMUM_RANK_FOR_MISSION,0x2ce8f2b0d96fcfb8, CommandNetworkSetMinimumRankForMission);

	SCR_REGISTER_SECURE(NETWORK_CACHE_LOCAL_PLAYER_HEAD_BLEND_DATA,0x379ab1a8b2d2cf41, CommandNetworkCacheLocalPlayerHeadBlendData);
	SCR_REGISTER_SECURE(NETWORK_HAS_CACHED_PLAYER_HEAD_BLEND_DATA,0xa19048b0a5b800b2, CommandNetworkHasCachedPlayerHeadBlendData);
	SCR_REGISTER_SECURE(NETWORK_APPLY_CACHED_PLAYER_HEAD_BLEND_DATA,0xc3aa694bb8b65a30, CommandNetworkApplyCachedPlayerHeadBlendData);

	//Register commerce functions
    SCR_REGISTER_SECURE(GET_NUM_COMMERCE_ITEMS,0x6c7efc463dfad900, CommandGetNumCommerceItems);
    SCR_REGISTER_SECURE(IS_COMMERCE_DATA_VALID,0x1b0dbb0f140d5933, CommandIsCommerceDataValid);
    SCR_REGISTER_SECURE(TRIGGER_COMMERCE_DATA_FETCH,0x1e29737690a77055, CommandTriggerCommerceDataFetch);
    SCR_REGISTER_SECURE(IS_COMMERCE_DATA_FETCH_IN_PROGRESS,0xace33ab19f1e4988, CommandIsThinDataFetchInProgress);
    SCR_REGISTER_SECURE(GET_COMMERCE_ITEM_ID,0x65268f667261e774, CommandGetCommerceItemId);
    SCR_REGISTER_SECURE(GET_COMMERCE_ITEM_NAME,0xb56bc582fabe5d03, CommandGetCommerceItemName);
    SCR_REGISTER_UNUSED(GET_COMMERCE_ITEM_IMAGENAME,0x4cdcb72b9777ce2a, CommandGetCommerceItemImageName);
    SCR_REGISTER_UNUSED(GET_COMMERCE_ITEM_DESC,0x5fab8b4b6513166c, CommandGetCommerceItemDesc);
    SCR_REGISTER_UNUSED(IS_COMMERCE_ITEM_PURCHASED,0x6510567e1c0437e5, CommandGetIsCommerceItemPurchased);
    SCR_REGISTER_SECURE(GET_COMMERCE_PRODUCT_PRICE,0x47a3d2989833fe42, CommandGetCommerceProductPrice);
    SCR_REGISTER_SECURE(GET_COMMERCE_ITEM_NUM_CATS,0xb8c8fd440a6ec714, CommandGetCommerceItemNumCats);
    SCR_REGISTER_SECURE(GET_COMMERCE_ITEM_CAT,0x74ae0841bf4b0964, CommandGetCommerceItemCat);
    SCR_REGISTER_UNUSED(RESERVE_COMMERCE_STORE_PURCHASE_LOCATION, 0x0fe78a4aa52c6a19, CommandReserveCommerceStorePurchaseLocation);
    SCR_REGISTER_SECURE(OPEN_COMMERCE_STORE,0xbe25f6f5d34768e7, CommandOpenCommerceStore);
	SCR_REGISTER_UNUSED(CHECKOUT_COMMERCE_PRODUCT,0xc3ec3470bf795d95, CommandCheckoutCommerceProduct);
    SCR_REGISTER_SECURE(IS_COMMERCE_STORE_OPEN,0x06c559386ad19942, CommandIsStoreOpen);
    SCR_REGISTER_SECURE(SET_STORE_ENABLED,0x91792061e06f1a74, CommandSetStoreEnabled);
    SCR_REGISTER_UNUSED(SET_COMMERCE_STORE_BRANDING,0x3e43a0cbd01511bf, CommandSetStoreBranding);
    SCR_REGISTER_SECURE(REQUEST_COMMERCE_ITEM_IMAGE,0x6c4bfff27e4c73e2, CommandRequestStoreImage);
    SCR_REGISTER_UNUSED(RELEASE_COMMERCE_ITEM_IMAGE,0x855b4d2860322b93, CommandReleaseStoreImage);
    SCR_REGISTER_SECURE(RELEASE_ALL_COMMERCE_ITEM_IMAGES,0x924457e1f136ba37, CommandReleaseAllStoreImages);
    SCR_REGISTER_SECURE(GET_COMMERCE_ITEM_TEXTURENAME,0xb49c489a8421a360, CommandGetStoreItemTextureName);
	SCR_REGISTER_UNUSED(SET_CASH_COMMERCE_PRODUCTS_ENABLED,0xd8bd21f54279c784, CommandSetCashCommerceProductsEnabled); 
	SCR_REGISTER_SECURE(IS_STORE_AVAILABLE_TO_USER,0x6ccc428f989dfd8a, CommandIsStoreAvailableToUser);
	SCR_REGISTER_SECURE(DELAY_MP_STORE_OPEN,0x61d4dda88f814946, CommandDelayStoreOpen);
    SCR_REGISTER_SECURE(RESET_STORE_NETWORK_GAME_TRACKING,0xd6ea0ca2391c3180, CommandStoreResetNetworkGameTracking);
	SCR_REGISTER_SECURE(IS_USER_OLD_ENOUGH_TO_ACCESS_STORE,0x4e357c5de898adf0, CommandCheckForStoreAge);
	SCR_REGISTER_SECURE(SET_LAST_VIEWED_SHOP_ITEM,0xdd3a3c508f4e1c08, CommandStoreSetLastViewedItem);
    SCR_REGISTER_SECURE(GET_USER_PREMIUM_ACCESS,0x40aceb5063a03a88, CommandGetUserPremiumAccess);
    SCR_REGISTER_SECURE(GET_USER_STARTER_ACCESS,0x2acfe7dfd4ffdb10, CommandGetUserStarterAccess);

	// cloud
	SCR_REGISTER_UNUSED(CLOUD_REQUEST_MEMBER_FILE,0xfe9a21fa5ed32531, CommandCloudRequestMemberFile);
	SCR_REGISTER_SECURE(CLOUD_DELETE_MEMBER_FILE,0x79d2907ece3668af, CommandCloudDeleteMemberFile);
	SCR_REGISTER_UNUSED(CLOUD_REQUEST_CREW_FILE,0xefcf77aaa4a66bc7, CommandCloudRequestCrewFile);
	SCR_REGISTER_UNUSED(CLOUD_REQUEST_TITLE_FILE,0x0360925bde1623ef, CommandCloudRequestTitleFile);
	SCR_REGISTER_UNUSED(CLOUD_REQUEST_GLOBAL_FILE,0x455b6fea4b18c348, CommandCloudRequestGlobalFile);
	SCR_REGISTER_SECURE(CLOUD_HAS_REQUEST_COMPLETED,0x4e72a404734f96dd, CommandCloudHasRequestCompleted);
	SCR_REGISTER_SECURE(CLOUD_DID_REQUEST_SUCCEED,0xd3d7e67cb9d9ee46, CommandCloudDidRequestSucceed);
	SCR_REGISTER_UNUSED(CLOUD_GET_REQUEST_RESULT_CODE,0xa1d6a9d07c2c5f24, CommandCloudGetRequestResultCode);

    SCR_REGISTER_SECURE(CLOUD_CHECK_AVAILABILITY,0x45a55f26e1745337, CommandCloudCheckAvailability);
    SCR_REGISTER_SECURE(CLOUD_IS_CHECKING_AVAILABILITY,0x7e1eecf1c1475c69, CommandCloudIsCheckingAvailability);
    SCR_REGISTER_UNUSED(CLOUD_GET_AVAILABILITY_CHECK_POSIX,0x677b552bdd78e372, CommandCloudGetAvailabilityCheckPosix);
    SCR_REGISTER_SECURE(CLOUD_GET_AVAILABILITY_CHECK_RESULT,0x99169228e980267d, CommandCloudGetAvailabilityCheckResult);

	// LiveItem content type.
	SCR_REGISTER_SECURE(GET_CONTENT_TO_LOAD_TYPE,0x596ca3c113ec82ce, CommandGetContentToLoadType);
	SCR_REGISTER_SECURE(GET_IS_LAUNCH_FROM_LIVE_AREA,0xced879009d9e18c3, CommandGetIsLaunchFromLiveArea);
	SCR_REGISTER_SECURE(GET_IS_LIVE_AREA_LAUNCH_WITH_CONTENT,0x77f0949bf4301609, CommandGetIsLiveAreaLaunchWithContent);
	SCR_REGISTER_UNUSED(GET_HAS_LAUNCH_PARAM,0x8d43b5912f8aec40, CommandGetHasLaunchParam);
	SCR_REGISTER_UNUSED(GET_LAUNCH_PARAM_VALUE,0x2f2d2dc351da1421, CommandGetLaunchParamValue);
	SCR_REGISTER_SECURE(CLEAR_SERVICE_EVENT_ARGUMENTS,0xbb74f8878bf41a7b, CommandClearSystemEventArguments);

	// ugc
    SCR_REGISTER_SECURE(UGC_COPY_CONTENT,0x35c3cba87df81d03, CommandUgcCopyContent);
    
	SCR_REGISTER_SECURE(UGC_IS_CREATING,0xdc819ef22f7e17f3, CommandUgcIsCreating);
	SCR_REGISTER_SECURE(UGC_HAS_CREATE_FINISHED,0x177da740cdc1ebc4, CommandUgcHasCreateFinished);
	SCR_REGISTER_SECURE(UGC_DID_CREATE_SUCCEED,0x090d542bb6126759, CommandUgcDidCreateSucceed);
	SCR_REGISTER_SECURE(UGC_GET_CREATE_RESULT,0x69ecdf325db9c589, CommandUgcGetCreateResult);
	SCR_REGISTER_SECURE(UGC_GET_CREATE_CONTENT_ID,0x46181aa928699fe3, CommandUgcGetCreateContentID);
	SCR_REGISTER_SECURE(UGC_CLEAR_CREATE_RESULT,0x3a4b6d510ad495d8, CommandUgcClearCreateResult);

	SCR_REGISTER_UNUSED(UGC_QUERY_CONTENT,0x093a809749bc908b, CommandUgcQueryContent);
    SCR_REGISTER_UNUSED(UGC_QUERY_BOOKMARKED_CONTENT,0x69f0c72492f326fc, CommandUgcQueryBookmarkedContent);
    SCR_REGISTER_SECURE(UGC_QUERY_MY_CONTENT,0x2c18a42f18346fa0, CommandUgcQueryMyContent);
    SCR_REGISTER_UNUSED(UGC_QUERY_FRIEND_CONTENT,0x79669804efba998b, CommandUgcQueryFriendContent);
    SCR_REGISTER_UNUSED(UGC_QUERY_CREW_CONTENT,0x1781904d723e55cf, CommandUgcQueryCrewContent);
    SCR_REGISTER_SECURE(UGC_QUERY_BY_CATEGORY,0x1f36263db64b7bfb, CommandUgcQueryByCategory);
    SCR_REGISTER_SECURE(UGC_QUERY_BY_CONTENT_ID,0x1e7c0d06935a3584, CommandUgcQueryByContentID);
    SCR_REGISTER_SECURE(UGC_QUERY_BY_CONTENT_IDS,0xc4b1b0cbd90fe418, CommandUgcQueryByContentIDs);
    SCR_REGISTER_SECURE(UGC_QUERY_MOST_RECENTLY_CREATED_CONTENT,0x3eaf63f4bf02fd1c, CommandUgcQueryMostRecentlyCreatedContent);
    SCR_REGISTER_UNUSED(UGC_QUERY_MOST_RECENTLY_PLAYED_CONTENT,0x01851a2e22eded85, CommandUgcQueryMostRecentlyPlayedContent);
    SCR_REGISTER_UNUSED(UGC_QUERY_TOP_RATED_CONTENT,0x60159e4c53b211ca, CommandUgcQueryTopRatedContent);

    // to be removed
	SCR_REGISTER_SECURE(UGC_GET_BOOKMARKED_CONTENT,0xc7882fe99e70b1c8, CommandUgcGetBookmarkedContent);
	SCR_REGISTER_SECURE(UGC_GET_MY_CONTENT,0xfb43158d2429d92e, CommandUgcGetMyContent);
    SCR_REGISTER_UNUSED(UGC_GET_MY_CONTENT_PUBLISHED,0x9051d22ddb894920, CommandUgcGetMyContentPublished);
    SCR_REGISTER_UNUSED(UGC_GET_MY_CONTENT_UNPUBLISHED,0xdb84ba7a86b7d50b, CommandUgcGetMyContentUnpublished);
    SCR_REGISTER_SECURE(UGC_GET_FRIEND_CONTENT,0xaf6af7e3ecf80083, CommandUgcGetFriendContent);
    SCR_REGISTER_SECURE(UGC_GET_CREW_CONTENT,0xf9d794f30c50a9f1, CommandUgcGetCrewContent);
	SCR_REGISTER_SECURE(UGC_GET_GET_BY_CATEGORY,0xb0a734543397e52f, CommandUgcGetByCategory);
	SCR_REGISTER_SECURE(UGC_GET_GET_BY_CONTENT_ID,0x11daab9c154637e3, CommandUgcGetByContentID);
	SCR_REGISTER_SECURE(UGC_GET_GET_BY_CONTENT_IDS,0xf34e2911670c2363, CommandUgcGetByContentIDs);
	SCR_REGISTER_SECURE(UGC_GET_MOST_RECENTLY_CREATED_CONTENT,0xd8d80f4e817ecbb3, CommandUgcGetMostRecentlyCreatedContent);
	SCR_REGISTER_SECURE(UGC_GET_MOST_RECENTLY_PLAYED_CONTENT,0x969db4f2fd753474, CommandUgcGetMostRecentlyPlayedContent);
	SCR_REGISTER_SECURE(UGC_GET_TOP_RATED_CONTENT,0x8b555ddac77146dd, CommandUgcGetTopRatedContent);

	SCR_REGISTER_SECURE(UGC_CANCEL_QUERY,0x604b7d5bd0ab3773, CommandUgcCancelQuery);
	
	SCR_REGISTER_SECURE(UGC_IS_GETTING,0xc3383a29bd0605fd, CommandUgcIsQuerying);
	SCR_REGISTER_SECURE(UGC_HAS_GET_FINISHED,0x77c220c0b39ffacc, CommandUgcHasQueryFinished);
	SCR_REGISTER_SECURE(UGC_DID_GET_SUCCEED,0x59b715ab2121ba3e, CommandUgcDidQuerySucceed);
    SCR_REGISTER_SECURE(UGC_WAS_QUERY_FORCE_CANCELLED,0x6dbcc00c8455d0aa, CommandUgcWasQueryForceCancelled);
    SCR_REGISTER_SECURE(UGC_GET_QUERY_RESULT,0x4d0fd82ea4bc8c1d, CommandUgcGetQueryResult);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_NUM,0xf3aa45cd776289e6, CommandUgcGetContentNum);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_TOTAL,0x2c013cee3c920c85, CommandUgcGetContentTotal);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_HASH,0xfb5f8a6ea8aae873, CommandUgcGetContentHash);
    SCR_REGISTER_SECURE(UGC_CLEAR_QUERY_RESULTS,0xaef7e3ff512e6708, CommandUgcClearQueryResults);

	SCR_REGISTER_SECURE(UGC_GET_CONTENT_USER_ID,0xf0ab55ff8283fc28, CommandUgcGetContentUserID);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_CREATOR_GAMER_HANDLE,0xb7a103bbd4c9d097, CommandUgcGetContentCreatorGamerHandle);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_CREATED_BY_LOCAL_PLAYER,0x3b5fdb74f543499e, CommandUgcGetContentCreatedByLocalPlayer);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_USER_NAME,0x090ffa33edbab655, CommandUgcGetContentUserName);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_IS_USING_SC_NICKNAME,0x84408d6da0441dc1, CommandUgcGetContentIsUsingScNickname);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_CATEGORY,0x3cf4fd7a9d95ee16, CommandUgcGetContentCategory);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_ID,0xf72292ef34ed9e5c, CommandUgcGetContentID);
    SCR_REGISTER_SECURE(UGC_GET_ROOT_CONTENT_ID,0x799bf71eb6225205, CommandUgcGetRootContentID);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_NAME,0xce98fa0d055b7884, CommandUgcGetContentName);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_DESCRIPTION,0x2a8c27f80a5f3b8b, CommandUgcGetContentDescription);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_DESCRIPTION_HASH,0x1223cfaaa0ac38fe, CommandUgcGetContentDescriptionHash);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_PATH,0xfbf31990d24f2c95, CommandUgcGetContentPath);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_CREATED_DATE,0x33bbd58dc0893801, CommandUgcGetContentCreatedDate);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_UPDATED_DATE,0xf8bbd854abcc35f0, CommandUgcGetContentUpdatedDate);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_FILE_VERSION,0xd84e34e7763a288f, CommandUgcGetContentFileVersion);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_HAS_LO_RES_PHOTO,0x7c9d4d56da841652, CommandUgcGetContentHasLoResPhoto);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_HAS_HI_RES_PHOTO,0xa428761b64babfae, CommandUgcGetContentHasHiResPhoto);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_LANGUAGE,0x1f02bdfd3c4b7fa4, CommandUgcGetContentLanguage);
    SCR_REGISTER_UNUSED(UGC_GET_CONTENT_VERSION,0x80e5b5efe82c4429, CommandUgcGetContentVersion);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_IS_PUBLISHED,0x179fad3a441f20b3, CommandUgcGetContentIsPublished);
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_IS_VERIFIED,0x941b4d709bcc8c10, CommandUgcGetContentIsVerified);
    
    SCR_REGISTER_SECURE(UGC_GET_CONTENT_RATING,0xc52c7e90982ff46a, CommandUgcGetContentRating);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_RATING_POSITIVE_PCT,0x9764c81f93137f04, CommandUgcGetContentRatingPositivePct);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_RATING_NEGATIVE_PCT,0x4d0d5459cd9a3d84, CommandUgcGetContentRatingNegativePct);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_RATING_COUNT,0x1a15e0f70d9f0e28, CommandUgcGetContentRatingCount);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_RATING_POSITIVE_COUNT,0xec359ed95ac57467, CommandUgcGetContentRatingPositiveCount);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_RATING_NEGATIVE_COUNT,0x20022932d1196962, CommandUgcGetContentRatingNegativeCount);

	SCR_REGISTER_SECURE(UGC_GET_CONTENT_HAS_PLAYER_RECORD,0x4f7dbaf2b4ee590f, CommandUgcGetContentHasPlayerRecord);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_HAS_PLAYER_RATED,0x9b3ffb2f1aebb18e, CommandUgcGetContentHasPlayerRated);
	SCR_REGISTER_UNUSED(UGC_GET_CONTENT_PLAYER_RATING,0x88059889b0456ad7, CommandUgcGetContentPlayerRating);
	SCR_REGISTER_SECURE(UGC_GET_CONTENT_HAS_PLAYER_BOOKMARKED,0x9b05b2451539f08d, CommandUgcGetContentHasPlayerBookmarked);

    SCR_REGISTER_SECURE(UGC_REQUEST_CONTENT_DATA_FROM_INDEX,0x171df6a0c07fb3dc, CommandUgcRequestContentDataFromIndex);
    SCR_REGISTER_SECURE(UGC_REQUEST_CONTENT_DATA_FROM_PARAMS,0x7fd2990af016795e, CommandUgcRequestContentDataFromParams);
	
    SCR_REGISTER_SECURE(UGC_REQUEST_CACHED_DESCRIPTION,0xc554b8b09b05cf3f, CommandUgcRequestCachedDescription);
    SCR_REGISTER_SECURE(UGC_IS_DESCRIPTION_REQUEST_IN_PROGRESS,0x1bfdc48299591ee7, CommandUgcIsDescriptionRequestInProgress);
    SCR_REGISTER_SECURE(UGC_HAS_DESCRIPTION_REQUEST_FINISHED,0xc4e6bb4d008b1a0d, CommandUgcHasDescriptionRequestFinished);
    SCR_REGISTER_SECURE(UGC_DID_DESCRIPTION_REQUEST_SUCCEED,0x3cda5f7958322c2b, CommandUgcDidDescriptionRequestSucceed);
    SCR_REGISTER_SECURE(UGC_GET_CACHED_DESCRIPTION,0xbe68e8ef2908ab25, CommandUgcGetCachedDescription);
    SCR_REGISTER_SECURE(UGC_RELEASE_CACHED_DESCRIPTION,0xb1bb0770ddaba505, CommandUgcReleaseCachedDescription);
    SCR_REGISTER_SECURE(UGC_RELEASE_ALL_CACHED_DESCRIPTIONS,0x965ee1ea5dcebc17, CommandUgcReleaseAllCachedDescriptions);

	SCR_REGISTER_UNUSED(UGC_HAS_PERMISSION_TO_WRITE,0x646d0f60e507030b, CommandUgcHasPermissionToWrite);
    SCR_REGISTER_SECURE(UGC_PUBLISH,0xdd7c2159ad30f8a0, CommandUgcPublish);
    SCR_REGISTER_SECURE(UGC_SET_BOOKMARKED,0x810db8b8dfe37018, CommandUgcSetBookmarked);
	SCR_REGISTER_SECURE(UGC_SET_DELETED,0xbccf9c02415cbb67, CommandUgcSetDeleted);
	SCR_REGISTER_UNUSED(UGC_RATE_CONTENT,0x5eae3908b1f86e32, CommandUgcRateContent);

	SCR_REGISTER_SECURE(UGC_IS_MODIFYING,0x412d9a1c024b8983, CommandUgcIsModifying);
	SCR_REGISTER_SECURE(UGC_HAS_MODIFY_FINISHED,0x8f43810a0b1a356b, CommandUgcHasModifyFinished);
	SCR_REGISTER_SECURE(UGC_DID_MODIFY_SUCCEED,0xb72414ad66bce7d2, CommandUgcDidModifySucceed);
	SCR_REGISTER_SECURE(UGC_GET_MODIFY_RESULT,0xa9433df0579e7ac5, CommandUgcGetModifyResult);
	SCR_REGISTER_UNUSED(UGC_GET_MODIFY_CONTENT_ID,0xf6132437d28da363, CommandUgcGetModifyContentID);
	SCR_REGISTER_SECURE(UGC_CLEAR_MODIFY_RESULT,0x9b41481e8e2456e1, CommandUgcClearModifyResult);

	SCR_REGISTER_UNUSED(UGC_GET_CREATORS_TOP_RATED,0x9f4f7ba8b635d80e, CommandUgcGetCreatorsTopRated);
	SCR_REGISTER_SECURE(UGC_GET_CREATORS_BY_USER_ID,0xe00f6b08b8b1b39d, CommandUgcGetCreatorsByUserID);
	SCR_REGISTER_UNUSED(UGC_GET_CREATORS_BY_USER_IDS,0x9b1676a3edc30eb5, CommandUgcGetCreatorsByUserIDs);

	SCR_REGISTER_UNUSED(UGC_CLEAR_CREATOR_RESULTS,0x6ed61fded0826ee9, CommandUgcClearCreatorResults);

	SCR_REGISTER_UNUSED(UGC_IS_QUERYING_CREATORS,0x52a93740fac98df3, CommandUgcIsQueryingCreators);
	SCR_REGISTER_SECURE(UGC_HAS_QUERY_CREATORS_FINISHED,0xa8828f4df76464d0, CommandUgcHasQueryCreatorsFinished);
	SCR_REGISTER_SECURE(UGC_DID_QUERY_CREATORS_SUCCEED,0x924d2c3702fa1571, CommandUgcDidQueryCreatorsSucceed);
	SCR_REGISTER_UNUSED(UGC_GET_QUERY_CREATORS_RESULT,0xf550dc287c67598a, CommandUgcGetQueryCreatorsResult);
	SCR_REGISTER_SECURE(UGC_GET_CREATOR_NUM,0xa1a05a5b5955c3d9, CommandUgcGetCreatorNum);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_TOTAL,0xc89a30e1142d8715, CommandUgcGetCreatorTotal);

	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_USER_ID,0xcaecc99bacb3ec42, CommandUgcGetCreatorUserID);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_USER_NAME,0xdcdbcd5359a0ead8, CommandUgcGetCreatorUserName);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_NUM_CREATED,0x3cd8dfbc595f487f, CommandUgcGetCreatorNumCreated);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_NUM_PUBLISHED,0x93f2a72dbcb213d0, CommandUgcGetCreatorNumPublished);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING,0xed3f2d76221bc551, CommandUgcGetCreatorRating);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING_POSITIVE_PCT,0x02f85e1cbb4c2b70, CommandUgcGetCreatorRatingPositivePct);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING_NEGATIVE_PCT,0x9e425b6b06919440, CommandUgcGetCreatorRatingNegativePct);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING_COUNT,0x26d1d66ba1a5cfbb, CommandUgcGetCreatorRatingCount);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING_POSITIVE_COUNT,0xb4d7ed79c9fe56a1, CommandUgcGetCreatorRatingPositiveCount);
	SCR_REGISTER_UNUSED(UGC_GET_CREATOR_RATING_NEGATIVE_COUNT,0xe95d8e704b5b497a, CommandUgcGetCreatorRatingNegativeCount);
    SCR_REGISTER_UNUSED(UGC_GET_CREATOR_IS_USING_SC_NICKNAME,0xf143a04110d436b5, CommandUgcGetCreatorIsUsingScNickname);

	SCR_REGISTER_SECURE(UGC_LOAD_OFFLINE_QUERY,0x7751c3c783a70f9a, CommandUgcLoadOfflineQuery);
	SCR_REGISTER_SECURE(UGC_CLEAR_OFFLINE_QUERY,0xa16c11d391768742, CommandUgcClearOfflineQuery);
	SCR_REGISTER_SECURE(UGC_SET_QUERY_DATA_FROM_OFFLINE,0xdec7f1514aac76a1, CommandUgcSetQueryDataFromOffline);

    SCR_REGISTER_SECURE(UGC_SET_USING_OFFLINE_CONTENT,0x2dd4b5731b5aaee8, CommandUgcSetUsingOfflineContent);
    SCR_REGISTER_SECURE(UGC_IS_LANGUAGE_SUPPORTED,0x9f9e97c8a7248d6c, CommandUgcIsLanguageSupported);

	// facebook 
	SCR_REGISTER_UNUSED(FACEBOOK_POST_PLAY_MISSION,0xcf5e7911655290a2, CommandFacebookPostPlayMission);
	SCR_REGISTER_UNUSED(FACEBOOK_POST_LIKE_MISSION,0xc7b57b24ece97d3d, CommandFacebookPostLikeMission);
	SCR_REGISTER_UNUSED(FACEBOOK_POST_PUBLISH_MISSION,0x387504917c8ac588, CommandFacebookPostPublishMission);
	SCR_REGISTER_UNUSED(FACEBOOK_POST_PUBLISH_PHOTO,0x9ada1dfab1a3bb4b, CommandFacebookPostPublishPhoto);
	SCR_REGISTER_SECURE(FACEBOOK_POST_COMPLETED_HEIST,0x44946083ae7967c4, CommandFacebookPostCompleteHeist);
	SCR_REGISTER_SECURE(FACEBOOK_POST_CREATE_CHARACTER,0xd69f92c01e2b80d6, CommandFacebookPostCreateCharacter);
	SCR_REGISTER_SECURE(FACEBOOK_POST_COMPLETED_MILESTONE,0x3e772791462c2e1f, CommandFacebookPostCompleteMilestone);
	SCR_REGISTER_SECURE(FACEBOOK_HAS_POST_COMPLETED,0x4d08678e990c28f6, CommandFacebookHasPostCompleted);
	SCR_REGISTER_SECURE(FACEBOOK_DID_POST_SUCCEED,0xbfe3a24077843158, CommandFacebookDidPostSucceed);
	SCR_REGISTER_UNUSED(FACEBOOK_GET_POST_RESULT_CODE,0x9dcfab417e624997, CommandFacebookGetPostResultCode);
	SCR_REGISTER_UNUSED(FACEBOOK_ENABLE_GUI,0xbfd94f76e70a52ea, CommandFacebookEnableGui);
	SCR_REGISTER_UNUSED(FACEBOOK_ENABLE_AUTO_GETACCESSTOKEN,0xde44937ce7cefc1e, CommandFacebookEnableAutoGetAccessToken);
	SCR_REGISTER_UNUSED(FACEBOOK_HAS_ACCESSTOKEN,0x19bbaa873c2b0f22, CommandFacebookHasAccessToken);
	SCR_REGISTER_UNUSED(FACEBOOK_GET_LAST_POSTRETURNID,0xe387aa8e977e9255, CommandFacebookGetLastPostId);
	SCR_REGISTER_SECURE(FACEBOOK_CAN_POST_TO_FACEBOOK,0xc59963203bab59df, CommandFacebookCanPostToFacebook);

	// texture downloads
    SCR_REGISTER_SECURE(TEXTURE_DOWNLOAD_REQUEST,0x1af529be61f700f2, CommandTextureDownloadRequest);
    SCR_REGISTER_SECURE(TITLE_TEXTURE_DOWNLOAD_REQUEST,0x6848bf6959c9e5fa, CommandTextureDownloadRequestTitle);
    SCR_REGISTER_SECURE(UGC_TEXTURE_DOWNLOAD_REQUEST,0x72385362add8885f, CommandTextureDownloadRequestUgc);
    SCR_REGISTER_SECURE(TEXTURE_DOWNLOAD_RELEASE,0x93d704c0cb05bee9, CommandTextureDownloadRelease);
    SCR_REGISTER_SECURE(TEXTURE_DOWNLOAD_HAS_FAILED,0xf108966ca9582f54, CommandTextureDownloadHasFailed);
    SCR_REGISTER_SECURE(TEXTURE_DOWNLOAD_GET_NAME,0xe14fcb457d28c3e8, CommandTextureDownloadGetName);
	SCR_REGISTER_SECURE(GET_STATUS_OF_TEXTURE_DOWNLOAD,0xc9cd4cf726ea8561, CommandGetStatusOfTextureDownload);

	SCR_REGISTER_SECURE(NETWORK_CHECK_ROS_LINK_WENTDOWN_NOT_NET,0xc1f1f0583b10bd60, CommandCheckROSLinkWentDownNotNet);

	SCR_REGISTER_UNUSED(NETWORK_GET_NAT_TYPE,0x4ac96dbd130b6e46, CommandNetworkGetNatType);
	SCR_REGISTER_SECURE(NETWORK_SHOULD_SHOW_STRICT_NAT_WARNING,0x93ff52d531fcf4ef, CommandShouldShowStrictNatWarning);
	
	SCR_REGISTER_SECURE(NETWORK_IS_CABLE_CONNECTED,0x1d2aba18149a4e82, CommandNetworkIsCableConnected);

	SCR_REGISTER_SECURE(NETWORK_HAVE_SCS_PRIVATE_MSG_PRIV,0xf1ab1e47a12aa2e6, CommandHaveRosPrivateMsgPriv);
    SCR_REGISTER_SECURE(NETWORK_HAVE_ROS_SOCIAL_CLUB_PRIV,0x4e8ff61a9663f967, CommandHaveRosSocialClubPriv);
    SCR_REGISTER_SECURE(NETWORK_HAVE_ROS_BANNED_PRIV,0xcd3dc164d0bfbca9, CommandHaveRosBannedPriv);
    SCR_REGISTER_SECURE(NETWORK_HAVE_ROS_CREATE_TICKET_PRIV,0x727b2ff4185a92fe, CommandHaveRosCreateTicketPriv);

    SCR_REGISTER_SECURE(NETWORK_HAVE_ROS_MULTIPLAYER_PRIV,0x67155a7aecd70f8b, CommandHaveRosMultiplayerPriv);
	SCR_REGISTER_SECURE(NETWORK_HAVE_ROS_LEADERBOARD_WRITE_PRIV,0x8d4166b8ec28f8d5, CommandHaveRosLeaderboardWritePriv);
	SCR_REGISTER_SECURE(NETWORK_HAS_ROS_PRIVILEGE,0xba27179576bcb41e, CommandHasRosPrivilege);
	SCR_REGISTER_SECURE(NETWORK_HAS_ROS_PRIVILEGE_END_DATE,0x1400223a788db4c7, CommandHasRosPrivilegeEndDate);
	SCR_REGISTER_SECURE(NETWORK_HAS_ROS_PRIVILEGE_PLAYED_LAST_GEN,0x2f04a4784a70593c, CommandHasRosPrivilegePlayedLastGen);
	SCR_REGISTER_SECURE(NETWORK_HAS_ROS_PRIVILEGE_SPECIAL_EDITION_CONTENT,0x4ce2af6d17d3b116, CommandHasRosPrivilegeSpecialEditionContent);

	SCR_REGISTER_UNUSED(HAS_RECEIVED_FORCE_PRESENCE_UPDATE_SIGNAL,0xe793a96dd70e824e, CommandHasReceivedForceSessionEvent);

	SCR_REGISTER_SECURE(NETWORK_START_COMMUNICATION_PERMISSIONS_CHECK,0x47b1bec47f443304, CommandStartCommunicationPermissionsCheck);
	SCR_REGISTER_UNUSED(NETWORK_START_TEXT_COMMUNICATION_PERMISSIONS_CHECK,0x81066ba1ad07ef57, CommandStartTextCommunicationPermissionsCheck);
	SCR_REGISTER_SECURE(NETWORK_START_USER_CONTENT_PERMISSIONS_CHECK,0xcc34a06b16b1250d, CommandStartUserContentPermissionsCheck);

	SCR_REGISTER_SECURE(NETWORK_SKIP_RADIO_RESET_NEXT_CLOSE,0xc189969327d9bda0,	CommandSkipRadioResetNextClose);
	SCR_REGISTER_SECURE(NETWORK_SKIP_RADIO_RESET_NEXT_OPEN,0x832a85c371e68507,	CommandSkipRadioResetNextOpen);

	SCR_REGISTER_SECURE(NETWORK_SKIP_RADIO_WARNING,0x2dcd9a6218733e97, CommandShouldWarnOfSimpleModCheck);

	SCR_REGISTER_SECURE(NETWORK_FORCE_LOCAL_PLAYER_SCAR_SYNC,0x1bd8dc43a2949227, CommandForceLocalPlayerScarSync);

	SCR_REGISTER_SECURE(NETWORK_DISABLE_LEAVE_REMOTE_PED_BEHIND,0xff472c078825e148, CommandDisableLeaveRemotePedBehind);
    SCR_REGISTER_UNUSED(NETWORK_CAN_USE_MISSION_CREATORS,0xb46cf3b318a0b834, CommandNetworkCanUseMissionCreators);

	SCR_REGISTER_UNUSED(NETWORK_ADD_PLAYER_PED_DECORATION,0x1055997247f64d53,	CommandAddPlayerPedDecoration);

    SCR_REGISTER_SECURE(NETWORK_ALLOW_REMOTE_ATTACHMENT_MODIFICATION,0x9ea64d6bf7698fa0, CommandNetworkAllowRemoteAttachmentModification);
    SCR_REGISTER_UNUSED(NETWORK_IS_REMOTE_ATTACHMENT_MODIFICATION_REQUIRED,0xd4212b10090e2418, CommandNetworkIsRemoteAttachmentModificationAllowed);

	SCR_REGISTER_SECURE(NETWORK_SHOW_CHAT_RESTRICTION_MSC,0x503cfb13397de761, CommandNetworkShowChatRestrictionMessage);
	SCR_REGISTER_SECURE(NETWORK_SHOW_PSN_UGC_RESTRICTION,0x4ebea6157716da7a, CommandNetworkShowPsnUgcRestriction);
	SCR_REGISTER_UNUSED(NETWORK_SHOW_PLAYER_REVIEW,0xe3700a0c2bb5bdfc, CommandNetworkShowPlayerReviewDialog);

	SCR_REGISTER_UNUSED(NETWORK_SHOW_WEB_BROWSER,0xf927a5690fd44650, CommandNetworkShowWebBrowser);
	SCR_REGISTER_SECURE(NETWORK_IS_TITLE_UPDATE_REQUIRED,0x4d0ce705937d9f5c, CommandIsTitleUpdateRequired);

	SCR_REGISTER_SECURE(NETWORK_QUIT_MP_TO_DESKTOP,0x734f2f6840164511, CommandNetworkQuitMultiplayerToDesktop);

    // Connection Metric Commands
    DLC_SCR_REGISTER_SECURE(NETWORK_IS_CONNECTED_VIA_RELAY,0xb06bb95c00f97612, CommandNetworkIsConnectedViaRelay);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_AVERAGE_LATENCY,0xe872127df9da80af, CommandNetworkGetAverageLatency);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_AVERAGE_PING,0xdca10c3f0c7c2585, CommandNetworkGetAverageLatency);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_AVERAGE_PACKET_LOSS,0x30bcef9a45373f3b, CommandNetworkGetAveragePacketLoss);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_NUM_UNACKED_RELIABLES,0x3e660d71ff46a508, CommandNetworkGetNumUnackedReliables);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_UNRELIABLE_RESEND_COUNT,0x4ca7fa6467bc236a, CommandNetworkGetUnreliableResendCount);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_HIGHEST_RELIABLE_RESEND_COUNT,0xd0304299f1734942, CommandNetworkGetHighestReliableResendCount);

	DLC_SCR_REGISTER_SECURE(NETWORK_REPORT_CODE_TAMPER,0x68777339a1ec6068, CommandNetworkReportCodeTamper);

    DLC_SCR_REGISTER_SECURE(NETWORK_GET_LAST_ENTITY_POS_RECEIVED_OVER_NETWORK,0x5c01ec35a233b127, CommandNetworkGetLastEntityPosReceivedOverNetwork);
    DLC_SCR_REGISTER_SECURE(NETWORK_GET_LAST_PLAYER_POS_RECEIVED_OVER_NETWORK,0x4805881668bffd1d, CommandNetworkGetLastPlayerPosReceivedOverNetwork);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_LAST_VEL_RECEIVED_OVER_NETWORK,0x2f9bb68e2516d8e1, CommandNetworkGetLastVelReceivedOverNetwork);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_PREDICTED_VELOCITY,0x8a6ade9f697703bd, CommandNetworkGetPredictedVelocity);

	// PS4 Net Info Commands
#if RSG_ORBIS && !__NO_OUTPUT
	DLC_SCR_REGISTER_SECURE(NETWORK_DUMP_NET_IF_CONFIG,0xb11f288de4ec5dc9, CommandNetworkDumpNetIfConfig);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_SIGNALLING_INFO,0x8a1270c53f7b3a9c, CommandNetworkGetSignallingInfo);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_NET_STATISTICS_INFO,0xf93a1a140a08d563, CommandNetworkGetNetStatisticsInfo);
#else
	DLC_SCR_REGISTER_SECURE(NETWORK_DUMP_NET_IF_CONFIG,0xb11f288de4ec5dc9, CommandDoNothing);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_SIGNALLING_INFO,0x8a1270c53f7b3a9c, CommandDoNothing);
	DLC_SCR_REGISTER_SECURE(NETWORK_GET_NET_STATISTICS_INFO,0xf93a1a140a08d563, CommandDoNothing);
#endif

	SCR_REGISTER_SECURE(NETWORK_GET_PLAYER_ACCOUNT_ID,0xe6dc7ee71bce8000, CommandNetworkGetPlayerAccountId);
	
	//UDS Event Functions
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_START,0xc81704a9680b971d, CommandNetworkUDSActivityStart);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_START_WITH_ACTORS,0x92b3edec07bcd480, CommandNetworkUDSActivityStartWithActors);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_END,0x9faff35541af6351, CommandNetworkUDSActivityEnd);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_RESUME,0xae7d878126959d27, CommandNetworkUDSActivityResume);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_RESUME_WITH_TASKS,0xb6ee0f6133d81273, CommandNetworkUDSActivityResumeWithTasks);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_AVAILABILITY_CHANGE,0x0b0f9169d0200e5c, CommandNetworkUDSActivityAvailabilityChange);
	SCR_REGISTER_UNUSED(NETWORK_POST_UDS_ACTIVITY_PRIORITY_CHANGE,0x1297a10a4fa73c6d, CommandNetworkUDSActivityPriorityChange);

	SCR_REGISTER_UNUSED(NETWORK_HAS_SC_MEMBERSHIP_INFO,0x553c1673b308873f, CommandNetworkHasScMembershipInfo);
	SCR_REGISTER_UNUSED(NETWORK_HAS_SC_MEMBERSHIP,0xdb04a0452c496eb1, CommandNetworkHasScMembership);
	SCR_REGISTER_UNUSED(NETWORK_GET_SC_MEMBERSHIP_INFO,0xb84c3659a0012ec9, CommandNetworkGetScMembershipInfo);
	SCR_REGISTER_UNUSED(NETWORK_CAN_SC_MEMBERSHIP_EXPIRE,0x0d9dd8fcf803b414, CommandNetworkCanScMembershipExpire);
	SCR_REGISTER_UNUSED(NETWORK_HAS_SHOWN_MEMBERSHIP_WELCOME, 0x6c1e73dfcbe2a0ce, CommandNetworkHasShownMembershipWelcome);

	SCR_REGISTER_SECURE(NETWORK_UGC_NAV,0x8b2712081f047817, CommandNetworkUgcNav);
}

void ResetTrackingVariables()
{
	gnetDebug1("ResetTrackingVariables");
	g_NetworkTimeLocked.Reset();
	g_NetworkTimeAccurate.Reset();
}

bool HasValidLocalGamerIndex(OUTPUT_ONLY(const char* fn))
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		gnetDebug1("%s :: HasValidLocalGamerIndex failed, Index: %d, SignedIn: %s", fn, localGamerIndex, CLiveManager::IsSignedIn() ? "True" : "False");
		return false;
	}
	return true; 
}

#undef __script_channel
#define __script_channel script

}
