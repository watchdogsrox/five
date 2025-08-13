//
// name:        NetworkInterface.cpp
// description: This is the new interface the game should use to access the network code. It is intended to reduce the
//              number of network header files that are included by game systems to an absolute minimum
// written by:  Daniel Yelland
//

// Rage
#include "net/timesync.h"
#include "rline/rlpresence.h"

// framework includes
#include "fwnet/netblenderlininterp.h"
#include "fwnet/netchannel.h"
#include "fwnet/neteventmgr.h"
#include "fwscript/scriptinterface.h"
#include "fwsys/timer.h"

// Game
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/landing_page\LandingPage.h"
#include "frontend/loadingscreens.h"
#include "frontend/MiniMap.h"
#include "network/Arrays/NetworkArrayMgr.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "network/Network.h"
#include "network/Arrays/NetworkArrayMgr.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "Network/Cloud/CloudManager.h"
#include "Network/Cloud/UserContentManager.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkGhostCollisions.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkRichPresence.h"
#include "Network/Objects/Entities/NetObjDoor.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/general/NetworkWorldGridOwnership.h"
#include "network/general/scriptworldstate/worldstates/NetworkCarGenWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPopGroupOverrideWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPopMultiplierAreaWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPtFXWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkRoadNodeWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkRopeWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkScenarioBlockingAreaWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkVehiclePlayerLockingWorldState.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/Party/NetworkParty.h"
#include "network/players/NetworkPlayerMgr.h"
#include "Network/Roaming/RoamingBubbleMgr.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/xlast/Fuzzy.schema.h"
#include "Objects/Door.h"
#include "peds/PlayerInfo.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "scene/DynamicEntity.h"
#include "scene/FocusEntity.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptHandlerNetwork.h"
#include "streaming/populationstreaming.h"
#include "script/script.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Network/Stats/NetworkLeaderboardCommon.h"
#include "script/script_hud.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/vehiclepopulation.h"

#if GEN9_LANDING_PAGE_ENABLED
#include "control/ScriptRouterContext.h"
#include "control/ScriptRouterLink.h"
#endif

#if RSG_GEN9
#include "Frontend/InitialInteractiveScreen.h"
#include "Network/Live/NetworkLandingPageStatsMgr.h"
#endif

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, interface, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_interface

PARAM(isguest, "Fakes the Guest/Sponsored user state (XB1)");
PARAM(useFPIdleMovementOnClones, "Run combined on foot / aiming movement for 1st person idle movement on remote players");
XPARAM(nonetwork);
XPARAM(cloudForceAvailable);
XPARAM(cloudForceNotAvailable);

PARAM(netCompliance, "Enables compliance mode (compliance friendly error handling / messaging");

PARAM(netForceReadyToCheckStats, "Force setting of whether stats can be checked");
PARAM(netForceActiveCharacter, "Force whether we have an active character or not");
PARAM(netForceActiveCharacterIndex, "Force the active character index");
PARAM(netForceCompletedIntro, "Force whether we have completed the MP intro");

static bool s_bSetMPCutsceneDebug = false;
static bool s_ignoreRemoteWaypoints = false;

int NetworkInterface::GetLocalGamerIndex()
{
	return rlPresence::GetActingUserIndex();
}

bool NetworkInterface::IsLocalPlayerOnline(bool bLanCounts)
{
#if __DEV
	if(PARAM_nonetwork.Get())
	{
		return false;
	}
#endif
	return CLiveManager::IsOnline(GAMER_INDEX_LOCAL) || (bLanCounts && CLiveManager::IsSignedIn(GAMER_INDEX_LOCAL) && LanSessionsOnly());
}

bool NetworkInterface::IsLocalPlayerGuest()
{
#if __DEV
	if(PARAM_isguest.Get())
	{
		return true;
	}
#endif
	return CLiveManager::IsGuest(GAMER_INDEX_LOCAL);
}

bool NetworkInterface::IsLocalPlayerSignedIn()
{
	return CLiveManager::IsSignedIn(GAMER_INDEX_LOCAL);
}

bool NetworkInterface::IsCloudAvailable(int localGamerIndex)
{
#if __BANK
	bool bIsAvailable = false;
	if(CloudManager::GetInstance().GetOverriddenCloudAvailability(bIsAvailable))
		return bIsAvailable;
#endif

#if !__FINAL
	if(PARAM_cloudForceAvailable.Get())
		return true;
	else if(PARAM_cloudForceNotAvailable.Get())
		return false;
#endif

	if(!rlCloud::IsInitialized())
		return false;

	bool bHasValidRosCredentials = HasValidRosCredentials(localGamerIndex);
	bool bHasCloudReadPrivilege = HasRosPrivilege(RLROS_PRIVILEGEID_CLOUD_STORAGE_READ, localGamerIndex);
	if(localGamerIndex == GAMER_INDEX_ANYONE)
	{
		return bHasValidRosCredentials && bHasCloudReadPrivilege;
	}
	else
	{
		return bHasValidRosCredentials && bHasCloudReadPrivilege && IsLocalPlayerOnline();
	}
}

bool NetworkInterface::IsOnlineRos()
{
	return CLiveManager::IsOnlineRos(GAMER_INDEX_LOCAL);
}

bool NetworkInterface::HasValidRosCredentials(const int localGamerIndex)
{
	return CLiveManager::HasValidRosCredentials(localGamerIndex);
}

bool NetworkInterface::HasCredentialsResult(const int localGamerIndex)
{
	return CLiveManager::HasCredentialsResult(localGamerIndex);
}

bool NetworkInterface::IsRefreshingRosCredentials(const int localGamerIndex)
{
	return CLiveManager::IsRefreshingRosCredentials(localGamerIndex);
}

bool NetworkInterface::HasValidRockstarId()
{
	return CLiveManager::HasValidRockstarId();
}

bool NetworkInterface::HasRosPrivilege(rlRosPrivilegeId privilegeId /* = RLROS_PRIVILEGEID_NONE */, const int localGamerIndex /* = GAMER_INDEX_LOCAL */)
{
	return CLiveManager::HasRosPrivilege(privilegeId, localGamerIndex);
}

int NetworkInterface::GetGamerIndexFromCLiveManager()
{
	return NetworkInterface::GetLocalGamerIndex();
}

bool NetworkInterface::IsRockstarQA()
{
	return CLiveManager::GetSCGamerDataMgr().IsRockstarQA();
}

const char* NetworkInterface::GetLocalGamerName()
{
	const rlGamerInfo* pInfo = GetActiveGamerInfo();
	if(!pInfo)
		return nullptr;

	return pInfo->GetName();
}

const rlGamerHandle& NetworkInterface::GetLocalGamerHandle()
{
    const rlGamerInfo* pInfo = GetActiveGamerInfo();
	if(pInfo != nullptr)
	    return pInfo->GetGamerHandle();
    else
        return RL_INVALID_GAMER_HANDLE;
}

const rlGamerInfo* NetworkInterface::GetActiveGamerInfo()
{
	return IsLocalPlayerSignedIn() ? rlPresence::GetActingGamerInfoPtr() : nullptr;
}

bool NetworkInterface::CheckOnlinePrivileges()
{
    return CLiveManager::CheckOnlinePrivileges();
}

bool NetworkInterface::HadOnlinePermissionsPromotionWithinMs(const unsigned nWithinMs)
{
    return CLiveManager::HadOnlinePermissionsPromotionWithinMs(nWithinMs);
}

bool NetworkInterface::HasNetworkAccess()
{
	return netHardware::IsLinkConnected();
}

bool NetworkInterface::IsReadyToCheckNetworkAccess(const eNetworkAccessArea accessArea, IsReadyToCheckAccessResult* pResult)
{
	return CNetwork::IsReadyToCheckNetworkAccess(accessArea, pResult);
}

bool NetworkInterface::CanAccessNetworkFeatures(const eNetworkAccessArea accessArea, eNetworkAccessCode* pAccessCode)
{
	return CNetwork::CanAccessNetworkFeatures(accessArea, pAccessCode);
}

eNetworkAccessCode NetworkInterface::CheckNetworkAccess(const eNetworkAccessArea accessArea, u64* endPosixTime)
{
	return CNetwork::CheckNetworkAccess(accessArea, endPosixTime);
}

bool NetworkInterface::IsUsingCompliance()
{
#if RSG_FINAL
	// Always assume XR/TRC compliance in final
	return true;
#elif RSG_BANK
	// Bank builds can opt-in via RAG toggle (defaults to the results of -netCompliance)
	return NetworkDebug::ShouldForceNetworkCompliance();
#else
	// Release, Profile, etc can use -netCompliance
	return PARAM_netCompliance.Get();
#endif
}

#if RSG_SCE

bool NetworkInterface::HasRawPlayStationPlusPrivileges()
{
    return g_rlNp.GetNpAuth().IsPlusAuthorized(NetworkInterface::GetLocalGamerIndex());
}

bool NetworkInterface::HasActivePlayStationPlusEvent()
{
	return CLiveManager::HasActivePlayStationPlusEvent();
}

bool NetworkInterface::IsPlayStationPlusPromotionEnabled()
{
	return CLiveManager::IsPlayStationPlusPromotionEnabled();
}

bool NetworkInterface::IsPlayStationPlusCustomUpsellEnabled()
{
	return CLiveManager::IsPlayStationPlusCustomUpsellEnabled();
}

bool NetworkInterface::DoesTunableDisablePlusLoadingButton()
{
    return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_PROMOTION_DISABLE_LOADING_BUTTON", 0x703e3b7e), false);
}

bool NetworkInterface::DoesTunableDisablePlusEntryPrompt()
{
    return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_PROMOTION_DISABLE_ENTRY_PROMPT", 0xe488b451), false);
}

bool NetworkInterface::DoesTunableDisablePauseStoreRedirect()
{
    return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_PROMOTION_DISABLE_PAUSE_STORE_REDIRECT", 0x4ae5b4d0), true);
}

bool NetworkInterface::IsPlayingInBackCompatOnPS5()
{
	return CNetwork::IsPlayingInBackCompatOnPS5();
}
#endif

bool NetworkInterface::IsActivitySession()
{
    return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsActivitySession(); 
}

bool NetworkInterface::IsTransitioning()
{ 
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsTransitioning(); 
}

bool NetworkInterface::IsLaunchingTransition()
{ 
    return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsLaunchingTransition(); 
}

bool NetworkInterface::IsTransitionActive()
{ 
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsTransitionActive(); 
}

bool NetworkInterface::CanPause()
{
	// multiplayer has kicked off
    if(IsNetworkOpen())
		return false;

    // content query is active
    if(UserContentManager::GetInstance().IsQueryPending())
        return false; 

	// in multiplayer - but transitioning between sessions
	if(IsTransitioning())
		return false;

	return true;
}

bool NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather()
{
	return CNetwork::IsApplyingMultiplayerGlobalClockAndWeather();
}

bool NetworkInterface::CanBail()
{
	return IsAnySessionActive();
}

void NetworkInterface::Bail(eBailReason nBailReason, int nBailContext)
{
	CNetwork::Bail(sBailParameters(nBailReason, nBailContext));
}

bool NetworkInterface::IsSessionActive()
{
    return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsSessionActive(); 
}

bool NetworkInterface::IsInSession()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsSessionActive(); 
}

bool NetworkInterface::IsSessionEstablished()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsSessionEstablished(); 
}

bool NetworkInterface::IsSessionLeaving()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsSessionLeaving(); 
}

bool NetworkInterface::IsSessionBusy()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsBusy(); 
}

bool NetworkInterface::IsAnySessionActive()
{
    if(!CNetwork::IsNetworkSessionValid())
        return false;

    return CNetwork::GetNetworkSession().IsSessionActive() 
        || CNetwork::GetNetworkSession().IsTransitionActive()
        || CNetwork::GetNetworkSession().IsTransitionMatchmaking()
        || CNetwork::GetNetworkSession().IsRunningDetailQuery();
}

bool NetworkInterface::IsInAnyMultiplayer()
{
	return IsAnySessionActive() || IsNetworkOpen();
}

bool NetworkInterface::IsInOrTranitioningToAnyMultiplayer()
{
	return IsInAnyMultiplayer() || IsTransitioningToMultiPlayer();
}

bool NetworkInterface::IsTransitioningToMultiPlayer()
{
#if GEN9_LANDING_PAGE_ENABLED
	if(ScriptRouterLink::HasPendingScriptRouterLink())
	{
		ScriptRouterContextData contextData;
		if(ScriptRouterLink::ParseScriptRouterLink(contextData) && contextData.m_ScriptRouterMode.Int == eScriptRouterContextMode::SRCM_FREE)
			return true;
	}
#endif

	return false;
}

bool NetworkInterface::IsTransitioningToSinglePlayer()
{
#if GEN9_LANDING_PAGE_ENABLED
	if(ScriptRouterLink::HasPendingScriptRouterLink())
	{
		ScriptRouterContextData contextData;
		if(ScriptRouterLink::ParseScriptRouterLink(contextData) && contextData.m_ScriptRouterMode.Int == eScriptRouterContextMode::SRCM_STORY)
			return true;
	}
#endif

	return false;
}

bool NetworkInterface::IsInSoloMultiplayer()
{
	return CNetwork::IsNetworkSessionValid() ? CNetwork::GetNetworkSession().IsSoloMultiplayer() : false;
}

bool NetworkInterface::IsInClosedSession()
{
	return CNetwork::IsNetworkSessionValid() ? CNetwork::GetNetworkSession().IsClosedSession(SessionType::ST_Physical) : false;
}

bool NetworkInterface::IsInPrivateSession()
{
	return CNetwork::IsNetworkSessionValid() ? CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Physical) : false;
}

bool NetworkInterface::AreAllSessionsNonVisible()
{
	return CNetwork::IsNetworkSessionValid() ? CNetwork::GetNetworkSession().AreAllSessionsNonVisible() : false;
}

bool NetworkInterface::CanSessionStart()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().CanStart(); 
}

bool NetworkInterface::CanSessionLeave()
{
	return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().CanLeave(); 
}

SessionTypeForTelemetry NetworkInterface::GetSessionTypeForTelemetry()
{
	if(!CNetwork::IsNetworkSessionValid())
		return SessionTypeForTelemetry::STT_Invalid;
	
	return CNetwork::GetNetworkSession().GetSessionTypeForTelemetry();
}

bool NetworkInterface::IsHost()
{
    return CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsHost();
}

bool NetworkInterface::IsHost(const rlSessionInfo& hInfo)
{
    if(!CNetwork::IsNetworkSessionValid())
        return false;

    return CNetwork::GetNetworkSession().IsSessionHost(hInfo);
}

bool NetworkInterface::IsASpectatingPlayer(CPed *ped)
{
    bool isASpectatingPlayer = false;

    if(ped && ped->IsPlayer() && ped->GetNetworkObject() && static_cast<CNetObjPlayer *>(ped->GetNetworkObject())->IsSpectating())
    {
        isASpectatingPlayer = true;
    }

    return isASpectatingPlayer;
}

bool NetworkInterface::IsSpectatingLocalPlayer(const PhysicalPlayerIndex playerIdx)
{
	bool isSpectatingLocalPlayer = false;

	if (gnetVerify(NetworkInterface::IsGameInProgress()))
	{
		CPed* myplayerped = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		CNetObjPlayer* myPlayer = (myplayerped && myplayerped->GetNetworkObject()) ? SafeCast(CNetObjPlayer, myplayerped->GetNetworkObject()) : 0;

		CNetGamePlayer* othernetplayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerIdx);
		CPed* otherplayerped = gnetVerify(othernetplayer) ? othernetplayer->GetPlayerPed() : 0;
		CNetObjPlayer* otherPlayer = otherplayerped && otherplayerped->GetNetworkObject() ? SafeCast(CNetObjPlayer, otherplayerped->GetNetworkObject()) : 0;

		if (myPlayer && otherPlayer)
		{
			isSpectatingLocalPlayer = (otherPlayer->IsSpectating() && otherPlayer->GetSpectatorObjectId() == myPlayer->GetObjectID());
		}
	}

	return isSpectatingLocalPlayer;
}

bool  NetworkInterface::IsPlayerPedSpectatingPlayerPed(const CPed* spectatorPlayerPed, const CPed* otherplayerped)
{
	bool isEntitySpectatingOtherPlayer = false;

	if (gnetVerify(NetworkInterface::IsNetworkOpen()) &&
		gnetVerify(spectatorPlayerPed) &&
		gnetVerify(spectatorPlayerPed->IsPlayer()) &&
		gnetVerify(otherplayerped) &&
		gnetVerify(otherplayerped->IsPlayer()) )
	{
		CNetObjPlayer* spectatorPlayer	= (spectatorPlayerPed && spectatorPlayerPed->GetNetworkObject()) ? SafeCast(CNetObjPlayer, spectatorPlayerPed->GetNetworkObject()) : 0;
		CNetObjPlayer* otherPlayer		= (otherplayerped && otherplayerped->GetNetworkObject()) ? SafeCast(CNetObjPlayer, otherplayerped->GetNetworkObject()) : 0;

		if (spectatorPlayer && otherPlayer )
		{
			isEntitySpectatingOtherPlayer = (spectatorPlayer->IsSpectating() && spectatorPlayer->GetSpectatorObjectId() == otherPlayer->GetObjectID());
		}
	}

	return isEntitySpectatingOtherPlayer;
}

bool  NetworkInterface::IsSpectatingPed(const CPed* otherplayerped)
{
	bool isSpectatingEntity = false;

	if (NetworkInterface::IsGameInProgress() && gnetVerify(otherplayerped) && gnetVerify(otherplayerped->IsPlayer()))
	{
		CPed* myplayerped = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		CNetObjPlayer* myPlayer = (myplayerped && myplayerped->GetNetworkObject()) ? SafeCast(CNetObjPlayer, myplayerped->GetNetworkObject()) : 0;

		CNetObjPlayer* otherPlayer = otherplayerped && otherplayerped->GetNetworkObject() ? SafeCast(CNetObjPlayer, otherplayerped->GetNetworkObject()) : 0;

		if (myPlayer && otherPlayer)
		{
			isSpectatingEntity = (myPlayer->IsSpectating() && myPlayer->GetSpectatorObjectId() == otherPlayer->GetObjectID());
		}
	}

	return isSpectatingEntity;
}

bool NetworkInterface::IsInSpectatorMode()
{
	bool isSpectating = false;

	if (NetworkInterface::IsGameInProgress())
	{
		CPed* myplayerped       = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		CNetObjPlayer* myPlayer = (myplayerped && myplayerped->GetNetworkObject()) ? SafeCast(CNetObjPlayer, myplayerped->GetNetworkObject()) : 0;
		isSpectating = myPlayer ? myPlayer->IsSpectating() : false;
	}

    return isSpectating;
}

bool NetworkInterface::IsEntityHiddenForMPCutscene(const CEntity* pEntity)
{
	if (IsInMPCutscene())
	{
		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

		if (pNetObj && !pNetObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
		{
			return true;
		}
	}

	return false;
}

bool NetworkInterface::IsEntityHiddenNetworkedPlayer(const CEntity* pEntity)
{
#if __ASSERT
	if( gnetVerifyf(NetworkInterface::IsGameInProgress(),"Only expect to use this in network games") &&
		gnetVerifyf(pEntity,"Null entity") &&
		gnetVerifyf(pEntity->GetIsTypePed(),"%s expected to be a ped",pEntity->GetDebugName()) )
#else
	if( NetworkInterface::IsGameInProgress() &&
		pEntity &&
		pEntity->GetIsTypePed() )
#endif
	{
		const CPed* attachPed = static_cast<const CPed*>(pEntity);
		if( attachPed->IsPlayer() && attachPed->IsNetworkClone())
		{
			//Must be spectating a player
			CNetObjPlayer const* attachNetObjPlayer = static_cast<CNetObjPlayer*>(attachPed->GetNetworkObject());
			if(attachNetObjPlayer->WasHiddenForTutorial())
			{
				return true;
			}
		}
	}

	return false;
}

bool NetworkInterface::LanSessionsOnly()
{
    return CNetwork::LanSessionsOnly();
}

rlNetworkMode NetworkInterface::GetNetworkMode()
{
	return CNetwork::LanSessionsOnly() ? RL_NETMODE_LAN : RL_NETMODE_ONLINE;
}

bool NetworkInterface::AllowNaturalMotion()
{
    return true;
}

bool NetworkInterface::NetworkClockHasSynced()
{
    return CNetwork::GetNetworkClock().HasSynched();
}

bool NetworkInterface::IsTeamGame()
{
    return false;
}

bool NetworkInterface::IsLocalPlayerOnSCTVTeam()
{
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if (pLocalPlayer)
	{
		return (pLocalPlayer->GetTeam() == eTEAM_SCTV);
	}

	return false;
}

bool NetworkInterface::IsOverrideSpectatedVehicleRadioSet()
{
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if (pLocalPlayer)
		return pLocalPlayer->IsOverrideSpectatedVehicleSet();

	return false;
}

void NetworkInterface::SetOverrideSpectatedVehicleRadio(bool bValue)
{
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if (pLocalPlayer)
		return pLocalPlayer->SetOverrideSpectatedVehicleRadio(bValue);
}

bool NetworkInterface::FriendlyFireAllowed()
{
    return CNetwork::IsFriendlyFireAllowed();
}

bool NetworkInterface::IsFriendlyFireAllowed(const CPed* victimPed, const CEntity* inflictorEntity)
{
	bool bFriendlyFireAllowed = true;

	if (inflictorEntity && victimPed && victimPed->GetNetworkObject() && victimPed->IsPlayer() && (victimPed != inflictorEntity)) //added (victimPed != inflictorEntity) because a player should always have friendlyfire allowed vs themselves.
	{
		CPed* inflictorPed = NULL;
		if (inflictorEntity)
		{
			if (inflictorEntity->GetIsTypePed())
			{
				inflictorPed = (CPed*) inflictorEntity;
			}
			else if (inflictorEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = (CVehicle*) inflictorEntity;
				if (pVehicle)
				{
					inflictorPed = pVehicle->GetDriver();
				}
			}
		}

		CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(victimPed->GetNetworkObject());
		if (pNetObjPlayer && !pNetObjPlayer->IsFriendlyFireAllowed())
		{
			weaponDebugf3("NetworkInterface::IsFriendlyFireAllowed--victimPed[%p][%s][%s]--(pNetObjPlayer && !pNetObjPlayer->IsFriendlyFireAllowed())-->initial set bFriendlyFireAllowed = false",victimPed,victimPed->GetModelName(),victimPed->GetNetworkObject()->GetLogName());
			bFriendlyFireAllowed = false;
		}

		if (bFriendlyFireAllowed)
		{
			if (inflictorPed && inflictorPed->IsPlayer())
			{
				pNetObjPlayer = static_cast<CNetObjPlayer*>(inflictorPed->GetNetworkObject());
				if (pNetObjPlayer && !pNetObjPlayer->IsFriendlyFireAllowed())
				{
					weaponDebugf3("NetworkInterface::IsFriendlyFireAllowed--inflictorPed[%p][%s][%s]--(pNetObjPlayer && !pNetObjPlayer->IsFriendlyFireAllowed())-->initial set bFriendlyFireAllowed = false",inflictorPed,inflictorPed->GetModelName(),inflictorPed->GetNetworkObject()->GetLogName());
					bFriendlyFireAllowed = false;
				}
			}
		}

		if(!bFriendlyFireAllowed && inflictorPed)
		{
			bool bEvaluteIsFriendlyWith = true;

			//if the inflictorped is a player we don't want to evaluate IsFriendlyWith if the relationship is INVALID 
			//if the inflictorped is an AI we always want to evaluate IsFriendlyWith
			if (inflictorPed->IsPlayer())
			{
				bEvaluteIsFriendlyWith = false;
				if (victimPed->GetPedIntelligence() && victimPed->GetPedIntelligence()->GetRelationshipGroup() && inflictorPed->GetPedIntelligence())
				{
					bool bValidRelationship = !victimPed->GetPedIntelligence()->GetRelationshipGroup()->CheckRelationship(ACQUAINTANCE_TYPE_INVALID,inflictorPed->GetPedIntelligence()->GetRelationshipGroupIndex());
					bEvaluteIsFriendlyWith = bValidRelationship;
				}
			}

			weaponDebugf3("NetworkInterface::IsFriendlyFireAllowed--(!bFriendlyFireAllowed && inflictorPed)--bEvaluteIsFriendlyWith[%d]",bEvaluteIsFriendlyWith);

			//need to evaluate relationship groups - if the peds are !IsFriendlyWith by each other then override bFriendlyFireAllowed setting it to true
			if (bEvaluteIsFriendlyWith)
				bFriendlyFireAllowed = !victimPed->GetPedIntelligence()->IsFriendlyWith(*inflictorPed,true);
		}
	}

	weaponDebugf3("NetworkInterface::IsFriendlyFireAllowed-->return bFriendlyFireAllowed[%d]",bFriendlyFireAllowed);
	return bFriendlyFireAllowed;
}

bool NetworkInterface::IsFriendlyFireAllowed(const CVehicle* victimVehicle, const CEntity* inflictorEntity)
{
	if (victimVehicle)
	{
		const CPed* victimInOrEnteringDriverOrPassengerSeat = FindDriverPedForVehicle(victimVehicle);

		if (!victimInOrEnteringDriverOrPassengerSeat && victimVehicle->InheritsFromTrailer())
		{
			const CTrailer* pTrailer = static_cast<const CTrailer*>(victimVehicle);
			const CVehicle* pAttachedToVeh = pTrailer->GetAttachedToParent();

			if (pAttachedToVeh)
				victimInOrEnteringDriverOrPassengerSeat = FindDriverPedForVehicle(pAttachedToVeh);
		}

		if (victimInOrEnteringDriverOrPassengerSeat)
		{
			return IsFriendlyFireAllowed(victimInOrEnteringDriverOrPassengerSeat,inflictorEntity);
		}
	}

	return true;
}

const CPed* NetworkInterface::FindDriverPedForVehicle(const CVehicle* victimVehicle)
{
	CPed* victimInOrEnteringDriverOrPassengerSeat = NULL;
	if (victimVehicle->IsSeatIndexValid(victimVehicle->GetDriverSeat()))
	{
		victimInOrEnteringDriverOrPassengerSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*victimVehicle, victimVehicle->GetDriverSeat());
	}

	if (!victimInOrEnteringDriverOrPassengerSeat && victimVehicle->IsSeatIndexValid(1))
	{
		victimInOrEnteringDriverOrPassengerSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*victimVehicle, 1);
	}

	return victimInOrEnteringDriverOrPassengerSeat;
}

bool NetworkInterface::IsPedAllowedToDamagePed(const CPed& inflictorPed, const CPed& victimPed)
{
	if(inflictorPed.IsPlayer() && victimPed.IsPlayer() && &inflictorPed != &victimPed)
	{
		CNetObjPlayer *netObjPlayerInflictor	= static_cast<CNetObjPlayer *>(inflictorPed.GetNetworkObject());
		CNetObjPlayer *netObjPlayerVictim		= static_cast<CNetObjPlayer *>(victimPed.GetNetworkObject());

		if((netObjPlayerInflictor && netObjPlayerVictim && netObjPlayerInflictor->IsAntagonisticToThisPlayer(netObjPlayerVictim)) || inflictorPed.GetPedConfigFlag(CPED_CONFIG_FLAG_CanAttackFriendly))
		{
			return true;
		}
		else
		{
			if (NetworkInterface::IsGameInProgress())
			{
				if (inflictorPed.GetPlayerInfo() && netObjPlayerVictim && !netObjPlayerVictim->GetIsDamagableByPlayer(inflictorPed.GetPlayerInfo()->GetPhysicalPlayerIndex()))
				{
					return false;
				}

				//MP - always allow vehicle based damage - regardless of friendly state or passivity
				if (inflictorPed.GetIsDrivingVehicle())
					return true;
				else
					return !inflictorPed.GetPedIntelligence()->IsFriendlyWith(victimPed);
			}
			else
			{
				//SP
				return !inflictorPed.GetPedIntelligence()->IsFriendlyWith(victimPed);
			}
		}
	}
	return true;
}

bool NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected(const CPed* pSourcePed, const CEntity* pTargetEntity)
{
	if (NetworkInterface::IsGameInProgress() && pSourcePed)
	{
		if (pSourcePed->IsPlayer() && pSourcePed->GetNetworkObject())
		{
			CNetObjPlayer* pPlayer = static_cast<CNetObjPlayer*>(pSourcePed->GetNetworkObject());
			if (pPlayer)
			{
				static const u32 scu_TimePostResurrectDisregardThreatResponseProcessing = 20000;
				u32 uLastResurrectionTime = pPlayer->GetLastResurrectionTime();
				u32 uDeltaResurrectionTime = NetworkInterface::GetSyncedTimeInMilliseconds() - uLastResurrectionTime;
				u32 uWeaponDamageTimeByEntity = pTargetEntity && (pTargetEntity->GetIsTypePed() || pTargetEntity->GetIsTypeVehicle()) ? static_cast<const CPhysical*>(pTargetEntity)->GetWeaponDamagedTimeByEntity((CEntity*) pSourcePed) : 0;
				u32 uDeltaDamageTime = fwTimer::GetTimeInMilliseconds() - uWeaponDamageTimeByEntity;

				wantedDebugf3("NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected -- Note pSourcePed[%p] pTargetEntity[%p] uLastResurrectionTime[%u] uDeltaResurrectionTime[%u] uWeaponDamageTimeByEntity[%u] uDeltaDamageTime[%u] scu_TimePostResurrectDisregardThreatResponseProcessing[%u]",pSourcePed,pTargetEntity,uLastResurrectionTime,uDeltaResurrectionTime,uWeaponDamageTimeByEntity,uDeltaDamageTime,scu_TimePostResurrectDisregardThreatResponseProcessing);

				if (uLastResurrectionTime == 0)
				{
					wantedDebugf3("NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected -- (uLastResurrectionTime == 0) -- continue processing -- allow --> return true");
					return true;
				}
				else
				{
					if ((uWeaponDamageTimeByEntity > 0) && (uDeltaResurrectionTime < uDeltaDamageTime))
					{
						wantedDebugf3("NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected -- time since inflictor ped resurrection[%u] < time since inflictor damage invoked[%u] -- disregard crime or task threat response processing --> return false",uDeltaResurrectionTime,uDeltaDamageTime);
						return false;
					}

					if ((uWeaponDamageTimeByEntity == 0) && (uDeltaResurrectionTime < scu_TimePostResurrectDisregardThreatResponseProcessing))
					{
						//fallback - sometimes the information about the player invoking damage on the victim isn't set
						//so, in that case if we are within a window from respawn of the source ped then disregard.
						wantedDebugf3("NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected -- uWeaponDamageTimeByEntity == 0 -- time since inflictor ped resurrection[%u] < scu_TimePostResurrectDisregardThreatResponseProcessing[%u] -- disregard crime or task threat response processing --> return false",uDeltaResurrectionTime,scu_TimePostResurrectDisregardThreatResponseProcessing);
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool NetworkInterface::IsInFreeMode()
{
	if(CNetwork::IsNetworkOpen())
		return (CNetwork::GetNetworkGameMode() == MultiplayerGameMode::GameMode_Freeroam);

	return false;
}

bool NetworkInterface::IsInArcadeMode()
{
	if(CNetwork::IsNetworkOpen())
	{
		const unsigned gameMode = CNetwork::GetNetworkGameMode();
		return (gameMode >= MultiplayerGameMode::GameMode_Arcade_Start) && (gameMode <= MultiplayerGameMode::GameMode_Arcade_Last);
	}

	return false;
}

bool NetworkInterface::IsInCopsAndCrooks()
{
#if CNC_MODE_ENABLED
	if(CNetwork::IsNetworkOpen())
		return (CNetwork::GetNetworkGameMode() == MultiplayerGameMode::GameMode_Arcade_CnC);
#endif	// CNC_MODE_ENABLED

	return false;
}

bool NetworkInterface::IsInFakeMultiplayerMode()
{
	if (CNetwork::IsNetworkOpen())
		return (CNetwork::GetNetworkGameMode() == MultiplayerGameMode::GameMode_FakeMultiplayer);

	return false;
}

u16 NetworkInterface::GetNetworkGameMode()
{
	return CNetwork::GetNetworkGameMode();
}

bool NetworkInterface::CanRegisterLocalAmbientPeds(const unsigned numPeds)
{
    int	numAmbientVehicles    = CVehiclePopulation::ms_numRandomCars;
    int ambientVehicleDeficit = static_cast<int>(CVehiclePopulation::GetDesiredNumberAmbientVehicles()) - numAmbientVehicles;

    if(ambientVehicleDeficit > 0)
    {
        float    localAmbientVehicleDeficitMultiplier = static_cast<float>(CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles()) / MAX_NUM_NETOBJVEHICLES;
        unsigned localAmbientVehicleDeficit           = static_cast<unsigned >(ambientVehicleDeficit * localAmbientVehicleDeficitMultiplier);
        unsigned numPedsToCheck = numPeds + localAmbientVehicleDeficit;

        return CNetworkObjectPopulationMgr::CanRegisterObjects(numPedsToCheck, 0, 0, 0, 0, false);
    }

    return true;
}

bool NetworkInterface::CanRegisterObject(const unsigned objectType, const bool isMissionObject)
{
    return CNetworkObjectPopulationMgr::CanRegisterObject(static_cast<NetworkObjectType>(objectType), isMissionObject);
}

bool NetworkInterface::CanRegisterObjects(const unsigned numPeds, const unsigned numVehicles, const unsigned numObjects, const unsigned numPickups, const unsigned numDoors, const bool areMissionObjects)
{
    return CNetworkObjectPopulationMgr::CanRegisterObjects(numPeds, numVehicles, numObjects, numPickups, numDoors, areMissionObjects);
}

bool NetworkInterface::CanRegisterRemoteObjectOfType(const unsigned objectType, const bool isMissionObject, bool isReservedObject)
{
    return CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(static_cast<NetworkObjectType>(objectType), isMissionObject, isReservedObject);
}

bool NetworkInterface::IsCloseToAnyPlayer(const Vector3& position, const float radius, const netPlayer*&pClosestPlayer)
{
    return NetworkUtils::IsCloseToAnyPlayer(position, radius, pClosestPlayer);
}

bool NetworkInterface::IsCloseToAnyRemotePlayer(const Vector3& position, const float radius, const netPlayer*&pClosestPlayer)
{
    return NetworkUtils::IsCloseToAnyRemotePlayer(position, radius, pClosestPlayer);
}

bool NetworkInterface::IsVisibleOrCloseToAnyPlayer(const Vector3& position, const float radius, const float distance, const float minDistance, 
												   const netPlayer** ppVisiblePlayer, const bool predictFuturePosition, const bool bIgnoreExtendedRange)
{
    return NetworkUtils::IsVisibleOrCloseToAnyPlayer(position, radius, distance, minDistance, ppVisiblePlayer, predictFuturePosition, bIgnoreExtendedRange);
}

bool NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, 
														 const float minDistance, const netPlayer** ppVisiblePlayer, 
														 const bool predictFuturePosition, const bool bIgnoreExtendedRange)
{
    return NetworkUtils::IsVisibleOrCloseToAnyRemotePlayer(position, radius, distance, minDistance, ppVisiblePlayer, predictFuturePosition, bIgnoreExtendedRange);
}

bool NetworkInterface::IsVisibleToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const netPlayer** ppVisiblePlayer)
{
    return NetworkUtils::IsVisibleToAnyRemotePlayer(position, radius, distance, ppVisiblePlayer);
}

bool NetworkInterface::IsVisibleToPlayer(const CNetGamePlayer* pPlayer, const Vector3& position, const float radius, const float distance, const bool bIgnoreExtendedRange)
{
	return NetworkUtils::IsVisibleToPlayer(pPlayer, position, radius, distance, bIgnoreExtendedRange);
}

unsigned NetworkInterface::GetClosestRemotePlayers(const Vector3& position, const float radius, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], const bool sortResults, bool alwaysGetPlayerPos)
{
    return NetworkUtils::GetClosestRemotePlayers(position, radius, closestPlayers, sortResults, alwaysGetPlayerPos);
}

unsigned NetworkInterface::GetNumActivePlayers()
{
    return GetPlayerMgr().GetNumActivePlayers();
}

unsigned NetworkInterface::GetNumPhysicalPlayers()
{
    return GetPlayerMgr().GetNumPhysicalPlayers();
}

CNetGamePlayer* NetworkInterface::GetPlayerFromConnectionId(const int cxnId)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPlayerFromConnectionId(cxnId);
}

CNetGamePlayer* NetworkInterface::GetPlayerFromEndpointId(const EndpointId endpointId)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPlayerFromEndpointId(endpointId);
}

CNetGamePlayer* NetworkInterface::GetPlayerFromGamerId(const rlGamerId& gamerId)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPlayerFromGamerId(gamerId);
}

ActivePlayerIndex NetworkInterface::GetActivePlayerIndexFromGamerHandle(const rlGamerHandle& gamerHandle)
{
	for(ActivePlayerIndex i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
	{
		CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
		if(pPlayer && pPlayer->GetGamerInfo().GetGamerHandle() == gamerHandle)
		{
			return i;
		}
	}

	return MAX_NUM_ACTIVE_PLAYERS;
}

PhysicalPlayerIndex NetworkInterface::GetPhysicalPlayerIndexFromGamerHandle(const rlGamerHandle& gamerHandle)
{
	for(PhysicalPlayerIndex i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
	{
		CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(i);
		if(pPlayer && pPlayer->GetGamerInfo().GetGamerHandle() == gamerHandle)
		{
			return i;
		}
	}

	return MAX_NUM_PHYSICAL_PLAYERS;
}

CNetGamePlayer* NetworkInterface::GetPlayerFromGamerHandle(const rlGamerHandle& gamerHandle)
{
	for(ActivePlayerIndex i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
	{
		CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
		if(pPlayer && pPlayer->GetGamerInfo().GetGamerHandle() == gamerHandle)
		{
			return pPlayer;
		}
	}

	return NULL;
}

CNetGamePlayer* NetworkInterface::GetPlayerFromPeerId(const u64 peerId)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPlayerFromPeerId(peerId);
}

CNetGamePlayer* NetworkInterface::GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetActivePlayerFromIndex(playerIndex);
}

CNetGamePlayer* NetworkInterface::GetPhysicalPlayerFromIndex(const PhysicalPlayerIndex playerIndex)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPhysicalPlayerFromIndex(playerIndex);
}

CNetGamePlayer *NetworkInterface::GetLocalPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex)
{
    CNetGamePlayer *player      = 0;
    CNetGamePlayer *localPlayer = GetLocalPlayer();

    if(localPlayer)
    {
        player = GetPlayerFromPeerPlayerIndex(peerPlayerIndex, localPlayer->GetGamerInfo().GetPeerInfo());
    }

    return player;
}

CNetGamePlayer *NetworkInterface::GetPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex, const rlPeerInfo &peerInfo)
{
    return (CNetGamePlayer *)GetPlayerMgr().GetPlayerFromPeerPlayerIndex(peerPlayerIndex, peerInfo);
}

CNetGamePlayer* NetworkInterface::GetHostPlayer()
{
    return CNetwork::IsNetworkSessionValid() ? (CNetGamePlayer*) CNetwork::GetNetworkSession().GetHostPlayer() : 0;
}

const char* NetworkInterface::GetHostName()
{
	return CNetwork::GetNetworkSession().GetHostPlayer() ? CNetwork::GetNetworkSession().GetHostPlayer()->GetLogName() : "";
}

const char* NetworkInterface::GetFreemodeHostName()
{
	CGameScriptId scrId("Freemode", -1, 0);

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

	return pHost ? pHost->GetLogName() : "";
}

Vector3 NetworkInterface::GetPlayerFocusPosition(const netPlayer& player, bool* bPlayerPedAtFocus, bool alwaysGetPlayerPos, unsigned* LOGGING_ONLY(playerFocus))
{
	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&player));

	const CNetGamePlayer& checkPlayer = static_cast<const CNetGamePlayer &>(player);
	Vector3 position = NetworkUtils::GetNetworkPlayerPosition(checkPlayer);

	bool bAtFocus = true;

    LOGGING_ONLY(if (playerFocus) *playerFocus = PLAYER_FOCUS_AT_PED;)

	CNetObjPlayer* netObjPlayer = NULL;

	//Retrieve the player Network Object.
	if (checkPlayer.GetPlayerPed() && checkPlayer.GetPlayerPed()->GetNetworkObject())
	{
		netObjPlayer = SafeCast(CNetObjPlayer, checkPlayer.GetPlayerPed()->GetNetworkObject());
	}

    if(netObjPlayer)
    {
		if (alwaysGetPlayerPos == true && netObjPlayer->GetPlayerPed())
		{
			position = VEC3V_TO_VECTOR3(netObjPlayer->GetPlayerPed()->GetTransform().GetPosition());
			bAtFocus = false;			
		}
		else
		{
			if(Unlikely(netObjPlayer->IsUsingExtendedPopulationRange()))
			{
				// check for extended pop range before freecam (a player in an apartment looking though a telescope is classed as using freecam)
				position = netObjPlayer->GetExtendedPopulationRangeCenter();
				bAtFocus = false;
                LOGGING_ONLY(if (playerFocus) *playerFocus = PLAYER_FOCUS_AT_EXTENDED_POPULATION_RANGE_CENTER;)
			}
			else if(netObjPlayer->IsClone() && netObjPlayer->IsUsingFreeCam() && !netObjPlayer->IsUsingCinematicVehicleCam() && gnetVerifyf(netObjPlayer->GetViewport(), "Clone player doesn't have a viewport setup!"))
			{
				position = VEC3V_TO_VECTOR3(netObjPlayer->GetViewport()->GetCameraPosition());
				bAtFocus = false;
                LOGGING_ONLY(if (playerFocus) *playerFocus = PLAYER_FOCUS_AT_CAMERA_POSITION;)
			}
			else if(netObjPlayer->IsSpectating())
			{
				//Check to see if the Player is spectating.
				netObject* obj = NetworkInterface::GetNetworkObject(netObjPlayer->GetSpectatorObjectId());
				if (obj && (gnetVerify(NET_OBJ_TYPE_PLAYER == obj->GetObjectType() || NET_OBJ_TYPE_PED == obj->GetObjectType())))
				{
					CPed* ped = static_cast<CPed*>(static_cast<CNetObjPed*>(obj)->GetPed());
					if (gnetVerify(ped))
					{
						position = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
						bAtFocus = false;
                        LOGGING_ONLY(if (playerFocus) *playerFocus = PLAYER_FOCUS_SPECTATING;)
					}
				}
			}
			else if (!netObjPlayer->IsClone())
			{
				bool usingGameplayCamera = camInterface::IsRenderingDirector(camInterface::GetGameplayDirector());
				bool usingDebugCamera    = camInterface::GetDebugDirector().IsFreeCamActive();
				if (!usingGameplayCamera && !usingDebugCamera)
				{
					//local player using free camera - use the local viewport
					position = VEC3V_TO_VECTOR3(gVpMan.GetGameViewport()->GetNonConstGrcViewport().GetCameraPosition());
					bAtFocus = false;
                    LOGGING_ONLY(if (playerFocus) *playerFocus = PLAYER_FOCUS_AT_LOCAL_CAMERA_POSITION;)
				}
			}		
		}		
    }

	if (bPlayerPedAtFocus)
	{
		*bPlayerPedAtFocus = bAtFocus;
	}

	return position;
}

Vector3 NetworkInterface::GetPlayerCameraPosition(const netPlayer& player)
{
	bool bPlayerPedAtFocus = false;
	Vector3 position = GetPlayerFocusPosition(player, &bPlayerPedAtFocus);

	if (bPlayerPedAtFocus)
	{
		gnetAssert(dynamic_cast<const CNetGamePlayer *>(&player));

		const CNetGamePlayer& checkPlayer = static_cast<const CNetGamePlayer &>(player);

		CNetObjPlayer* netObjPlayer = NULL;

		//Retrieve the player Network Object.
		if (checkPlayer.GetPlayerPed() && checkPlayer.GetPlayerPed()->GetNetworkObject())
		{
			netObjPlayer = SafeCast(CNetObjPlayer, checkPlayer.GetPlayerPed()->GetNetworkObject());
		}

		if(netObjPlayer && netObjPlayer->IsClone() && netObjPlayer->IsUsingFreeCam() && gnetVerifyf(netObjPlayer->GetViewport(), "Clone player doesn't have a viewport setup!"))
		{
			// Check if the player is using free cam
			position = VEC3V_TO_VECTOR3(netObjPlayer->GetViewport()->GetCameraPosition());
		}
	}

	return position;
}

#if __ASSERT

bool NetworkInterface::IsPedAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject)
{
	if(gnetVerify(&pGameObject))
	{
		CDynamicEntity const* dynamicEntity = pGameObject.GetGameObject();
		if(dynamicEntity->GetIsTypePed())
		{
			CPed const* ped = static_cast<CPed const*>(dynamicEntity);

			if(ped)
			{
				return ped->IsPedAllowedInMultiplayer();		
			}
		}
	}

	return false;
}

bool NetworkInterface::IsVehicleAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject)
{
	if(gnetVerify(&pGameObject))
	{
		CDynamicEntity const* dynamicEntity = pGameObject.GetGameObject();
		if(dynamicEntity->GetIsTypeVehicle())
		{
			CVehicle const* vehicle = static_cast<CVehicle const*>(pGameObject.GetGameObject());

			if(vehicle)
			{
				return vehicle->IsVehicleAllowedInMultiplayer();
			}
		}
	}

	return false;
}

void NetworkInterface::IsGameObjectAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject)
{
	if(gnetVerify(&pGameObject))
	{
		if(pGameObject.GetGameObject())
		{
			bool allowed = true;
			switch(pGameObject.GetGameObject()->GetType())
			{
				case ENTITY_TYPE_VEHICLE:
					allowed = IsVehicleAllowedInMultiplayer(pGameObject);
					break;
				case ENTITY_TYPE_PED:		
					allowed = IsPedAllowedInMultiplayer(pGameObject);
					break;
				default: 
					break;
			}

			if(!allowed)
			{
				Vector3 pos = VEC3V_TO_VECTOR3(pGameObject.GetGameObject()->GetTransform().GetPosition());

				gnetAssertf(false, "ERROR : Object %s %s at %f %f %f is not supported in MP!", 
				pGameObject.GetGameObject()->GetDebugName(),
				pGameObject.GetGameObject()->GetModelName(),
				pos.x, pos.y, pos.z);
			}
		}
		else
		{
			gnetAssertf(false, "ERROR : Null game object passed to %s", __FUNCTION__);
		}
	}
}

#endif /* __ASSERT */

bool NetworkInterface::RegisterObject(netGameObjectWrapper<CDynamicEntity> pGameObject,
                    const NetObjFlags localFlags,
                    const NetObjFlags globalFlags,
                    const bool        makeSpaceForObject)
{
#if __ASSERT
	NetworkInterface::IsGameObjectAllowedInMultiplayer(pGameObject);
#endif /* __ASSERT */

   return CNetwork::GetObjectManager().RegisterGameObject(&pGameObject, localFlags, globalFlags, makeSpaceForObject);
}

void NetworkInterface::UnregisterObject(netGameObjectWrapper<CDynamicEntity> pGameObject, bool bForce)
{
    CNetwork::GetObjectManager().UnregisterObject(&pGameObject, bForce);
}

netObject *NetworkInterface::GetNetworkObject(const ObjectId objectID)
{
    return CNetwork::GetObjectManager().GetNetworkObject(objectID);
}

void NetworkInterface::ForceSortNetworkObjectMap()
{
	CNetwork::GetObjectManager().ForceSortNetworkObjectMap();
}

void NetworkInterface::AllowOtherThreadsAccessToNetworkManager(bool bAllowAccess)
{
	CNetwork::GetObjectManager().AllowOtherThreadsAccess(bAllowAccess);
}

void NetworkInterface::ForceVeryHighUpdateLevelOnVehicle(CVehicle* vehicle, u16 timer)
{
	if (vehicle && vehicle->GetNetworkObject())
	{
		CNetObjVehicle* netObjVehicle = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());
		netObjVehicle->ForceVeryHighUpdateLevel(timer);
	}
}

void NetworkInterface::SetSuperDummyLaunchedIntoAir(CVehicle* vehicle, u16 timer)
{
	if (vehicle && vehicle->GetNetworkObject())
	{
		CNetObjVehicle* netObjVehicle = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());
		netObjVehicle->SetSuperDummyLaunchedIntoAir(timer);
	}
}

bool NetworkInterface::IsSuperDummyStillLaunchedIntoAir(const CVehicle* vehicle)
{
	if (vehicle && vehicle->GetNetworkObject())
	{
		const CNetObjVehicle* netObjVehicle = SafeCast(const CNetObjVehicle, vehicle->GetNetworkObject());
		return netObjVehicle->IsSuperDummyStillLaunchedIntoAir();
	}

	return false;
}

bool NetworkInterface::CanSpectateObjectId(netObject* netobj, bool skipTutorialCheck)
{
	bool validToSpectate = false;

	if (NetworkInterface::IsGameInProgress() && gnetVerify(netobj))
	{
		validToSpectate = true; //if we are in a network game and have a valid network object - then by default until proven different it is valid to spectate it

        if(!skipTutorialCheck)
        {
		    CPed* playerped = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		    if (playerped && playerped->GetNetworkObject())
		    {
			    //check valid for players viewing other players only
			    if (NET_OBJ_TYPE_PLAYER == netobj->GetObjectType())
			    {
				    CNetObjPlayer* myPlayer    = SafeCast(CNetObjPlayer, playerped->GetNetworkObject());
				    CNetObjPlayer* otherPlayer = SafeCast(CNetObjPlayer, netobj);

                    if(myPlayer->GetPlayerOwner() && otherPlayer->GetPlayerOwner())
                    {
				        validToSpectate = ( !NetworkInterface::ArePlayersInDifferentTutorialSessions(*myPlayer->GetPlayerOwner(), *otherPlayer->GetPlayerOwner()) );
                    }
                    else
                    {
                        validToSpectate = false;
                    }
			    }
		    }
        }
	}

	return validToSpectate;
}

void NetworkInterface::SetSpectatorObjectId(const ObjectId objectId)
{
	gnetDebug1("Changing spectator pedId \"oldpedId = %d, newPedId = %d\")\n", NetworkInterface::GetSpectatorObjectId(), objectId);

	//Update the id of the spectator
	if (NetworkInterface::IsGameInProgress())
	{
		CPed* playerped = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		if (playerped && playerped->GetNetworkObject())
		{
			CNetObjPlayer* myPlayer = SafeCast(CNetObjPlayer, playerped->GetNetworkObject());

			netObject* oldSpectatorNetObj = NetworkInterface::GetNetworkObject(myPlayer->GetSpectatorObjectId());
			if (oldSpectatorNetObj)
			{
				NetworkObjectType objtype = oldSpectatorNetObj->GetObjectType();
				if (AssertVerify(objtype == NET_OBJ_TYPE_PLAYER || objtype == NET_OBJ_TYPE_PED))
				{
					CPed* ped = static_cast<CNetObjPed*>(oldSpectatorNetObj)->GetPed();
					if (AssertVerify(ped))
					{
						CPortalTracker::RemoveFromActivatingTrackerList(ped->GetPortalTracker());
					}
				}
			}

			const bool bWasSpectating = myPlayer->IsSpectating();
			myPlayer->SetSpectatorObjectId(objectId);

			netObject* newSpectatorNetObj = NetworkInterface::GetNetworkObject(objectId);
			if(newSpectatorNetObj)
			{
				NetworkObjectType objtype = newSpectatorNetObj->GetObjectType();
				if (AssertVerify(objtype == NET_OBJ_TYPE_PLAYER || objtype == NET_OBJ_TYPE_PED))
				{
					CPed* ped = static_cast<CNetObjPed*>(newSpectatorNetObj)->GetPed();
					if (AssertVerify(ped))
					{
						CPortalTracker::AddToActivatingTrackerList(ped->GetPortalTracker());
					}
				}
			}

			if(bWasSpectating != myPlayer->IsSpectating())
			{
				CNetwork::GetNetworkSession().UpdateSCPresenceInfo();
			}
		}
	}
}

ObjectId  NetworkInterface::GetSpectatorObjectId()
{
	ObjectId objectId = NETWORK_INVALID_OBJECT_ID;

	if (NetworkInterface::IsGameInProgress())
	{
		CPed* playerped = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
		if (playerped && playerped->GetNetworkObject())
		{
			CNetObjPlayer* myPlayer = SafeCast(CNetObjPlayer, playerped->GetNetworkObject());
			objectId = myPlayer->GetSpectatorObjectId();
		}
	}

	return objectId;
}

void NetworkInterface::SetInMPCutscene(bool bInCutscene, bool bMakePlayerInvincible)
{
    CNetGamePlayer* pMyPlayer = GetPlayerMgr().GetMyPlayer();

	static bool bLocalPlayerDamageable = false;

	if (gnetVerify(pMyPlayer) && gnetVerify(pMyPlayer->GetPlayerInfo()))
	{
		CPlayerInfo::State currState = pMyPlayer->GetPlayerInfo()->GetPlayerState();

		if (bInCutscene)
		{
			if (gnetVerifyf(currState == CPlayerInfo::PLAYERSTATE_PLAYING || currState == CPlayerInfo::PLAYERSTATE_RESPAWN, "SetInMPCutscene(true) - the local player is not in a playing state (%d)", pMyPlayer->GetPlayerInfo()->GetPlayerState()))
			{
				pMyPlayer->GetPlayerInfo()->SetPlayerState(CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE);
				bLocalPlayerDamageable = pMyPlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything;
				pMyPlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything = bMakePlayerInvincible;
				if(pMyPlayer->GetPlayerPed()->GetNetworkObject())
				{
					CNetObjPlayer* netObjPlayer = SafeCast(CNetObjPlayer, pMyPlayer->GetPlayerPed()->GetNetworkObject());
					netObjPlayer->ForceSendOfCutsceneState();
				}
			}

			CNetwork::SetInMPCutscene(true);

			GetObjectManager().HideAllObjectsForCutscene();

			if(CProjectileManager::GetHideProjectilesInCutscene())
			{
				CProjectileManager::ToggleVisibilityOfAllProjectiles(false);
			}
		}
		else 
		{
			if (currState == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
			{
				pMyPlayer->GetPlayerInfo()->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
				pMyPlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything = bLocalPlayerDamageable;
			}

			CNetwork::SetInMPCutscene(false);

			//Ensure this is processed otherwise a local player might have m_RemoteVisibilityOverride set active previously and it won't be cleared and then they will be invisible to 
			//all other players because IsLocalEntityVisibleOverNetwork will report false because the remote active has it still set to false. lavalley.
			GetObjectManager().ExposeAllObjectsAfterCutscene();

			if(CProjectileManager::GetHideProjectilesInCutscene())
			{
				CProjectileManager::ToggleVisibilityOfAllProjectiles(true);
				CProjectileManager::ToggleHideProjectilesInCutscene(false);
			}
		}

		if (bInCutscene)
		{
			if (CTheScripts::GetCurrentGtaScriptHandlerNetwork())
			{
				CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetInMPCutscene(bInCutscene);
			}
		}
		else
		{
			// clear the cutscene flags for all scripts as this command may have been called from a different script
			CTheScripts::GetScriptHandlerMgr().SetNotInMPCutscene();
		}
	}
}

void NetworkInterface::SetInMPCutsceneDebug(bool bInCutscene)
{
	// set a bool here, so that the cutscene is started after the scripts are updated - this simulates a cutscene being set by script
	if (bInCutscene != CNetwork::IsInMPCutscene())
	{
		s_bSetMPCutsceneDebug = true;
	}
}

InviteMgr& NetworkInterface::GetInviteMgr()					
{ 
	return CLiveManager::GetInviteMgr(); 
}

richPresenceMgr& NetworkInterface::GetRichPresenceMgr()			
{ 
	return CLiveManager::GetRichPresenceMgr(); 
}

void NetworkInterface::SetProcessLocalPopulationLimits(bool processLimits)
{
    CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(processLimits);
}

void NetworkInterface::SetUsingExtendedPopulationRange(const Vector3& center)
{
	CNetObjPlayer* pPlayerObj = FindPlayerPed() ? static_cast<CNetObjPlayer*>(FindPlayerPed()->GetNetworkObject()) : NULL;

	if (pPlayerObj)
	{
		pPlayerObj->SetUsingExtendedPopulationRange(center);
	}
}

void NetworkInterface::ClearUsingExtendedPopulationRange()
{
	CNetObjPlayer* pPlayerObj = FindPlayerPed() ? static_cast<CNetObjPlayer*>(FindPlayerPed()->GetNetworkObject()) : NULL;

	if (pPlayerObj)
	{
		pPlayerObj->ClearUsingExtendedPopulationRange();
	}
}

const NetworkGameConfig& NetworkInterface::GetMatchConfig()
{
    return CNetwork::GetNetworkSession().GetMatchConfig();
}

bool NetworkInterface::DoQuickMatch(const int nGameMode, const int nQueryID, const unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "EnterSession :: Network session not valid!"))
		return false;

	return CNetwork::GetNetworkSession().DoQuickMatch(nGameMode, nQueryID, nMaxPlayers, matchmakingFlags);
}

bool NetworkInterface::DoSocialMatchmaking(const char* szQuery, const char* szParams, const int nGameMode, const int nQueryID, const unsigned nMaxPlayers, unsigned matchmakingFlags)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "DoSocialMatchmaking :: Network session not valid!"))
		return false;

	return CNetwork::GetNetworkSession().DoSocialMatchmaking(szQuery, szParams, nGameMode, nQueryID, nMaxPlayers, matchmakingFlags);
}

bool NetworkInterface::DoActivityQuickmatch(const int nGameMode, const unsigned nMaxPlayers, const int nActivityType, const int nActivityId, unsigned matchmakingFlags)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "DoActivityQuickmatch :: Network session not valid!"))
		return false;

	return CNetwork::GetNetworkSession().DoActivityQuickmatch(nGameMode, nMaxPlayers, nActivityType, nActivityId, matchmakingFlags);
}

bool NetworkInterface::HostSession(const int nGameMode, const int nMaxPlayers, const unsigned nHostFlags)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "HostSession :: Network session not valid!"))
		return false;

	return CNetwork::GetNetworkSession().HostSession(nGameMode, nMaxPlayers, nHostFlags);
}

bool NetworkInterface::LeaveSession(bool bReturnToLobby, bool bBlacklist)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "LeaveSession :: Network session not valid!"))
		return false;

	// build flags
	unsigned leaveFlags = 0;
    if(bReturnToLobby) leaveFlags |= LeaveFlags::LF_ReturnToLobby;
	if(bBlacklist) leaveFlags |= LeaveFlags::LF_BlacklistSession;

	// exit session
	return CNetwork::GetNetworkSession().LeaveSession(leaveFlags, LeaveSessionReason::Leave_Normal);
}

bool NetworkInterface::LeaveSession(const unsigned leaveFlags)
{
	return CNetwork::GetNetworkSession().LeaveSession(leaveFlags, LeaveSessionReason::Leave_Normal);
}

void NetworkInterface::SetScriptValidateJoin()
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "SetScriptValidateJoin :: Network session not valid!"))
		return;

	CNetwork::GetNetworkSession().SetScriptValidateJoin();
}

void NetworkInterface::ValidateJoin(bool bJoinSuccessful)
{
	if(!gnetVerifyf(CNetwork::IsNetworkSessionValid(), "ValidateJoin :: Network session not valid!"))
		return;

	CNetwork::GetNetworkSession().ValidateJoin(bJoinSuccessful);
}

bool NetworkInterface::IsMigrating()
{
	return CNetwork::GetNetworkSession().IsMigrating();
}

void NetworkInterface::NewHost()
{
	netLoggingInterface &log = netInterface::GetArrayManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "NEW_HOST", "%s", GetHostPlayer() ? GetHostPlayer()->GetLogName() : "NONE");
	log = netInterface::GetEventManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "NEW_HOST", "%s", GetHostPlayer() ? GetHostPlayer()->GetLogName() : "NONE");
	log = netInterface::GetPlayerMgr().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "NEW_HOST", "%s", GetHostPlayer() ? GetHostPlayer()->GetLogName() : "NONE");

	GetArrayManager().NewHost();
}

/********************************** PARTY **********************************************/
bool NetworkInterface::HostParty()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().HostParty();
}

bool NetworkInterface::LeaveParty()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().LeaveParty();
}

bool NetworkInterface::JoinParty(const rlSessionInfo& sessionInfo)
{
	return !IsInParty() && CNetwork::GetNetworkParty().JoinParty(sessionInfo);
}

bool NetworkInterface::IsPartyLeader()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().IsPartyLeader();
}

bool NetworkInterface::IsPartyMember(const rlGamerHandle* pGamerHandle)
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().IsPartyMember(*pGamerHandle);
}

bool NetworkInterface::IsPartyMember(const CPed* pPed)
{
	if(!gnetVerifyf(CNetwork::IsNetworkPartyValid(), "IsPartyMember :: Network party invalid!"))
		return false;

	// if the local player is not in party... bail
	if(!CNetwork::GetNetworkParty().IsInParty())
		return false;

	// if the target ped is not a player... bail
	if(!pPed->IsPlayer())
		return false;

	// if we have no player info... bail
	const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!pPlayerInfo)
		return false;

	// check player gamer handle against party
	return CNetwork::GetNetworkParty().IsPartyMember(pPlayerInfo->m_GamerInfo.GetGamerHandle());
}

bool NetworkInterface::IsInParty()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().IsInParty();
}

unsigned NetworkInterface::GetPartyMemberCount()
{
	return CNetwork::IsNetworkPartyValid() ? CNetwork::GetNetworkParty().GetPartyMemberCount() : 0;
}

unsigned NetworkInterface::GetPartyMembers(rlGamerInfo* pInfoBuffer, const unsigned maxMembers)
{
	return CNetwork::IsNetworkPartyValid() ? CNetwork::GetNetworkParty().GetPartyMembers(pInfoBuffer, maxMembers) : 0;
}

bool NetworkInterface::IsPartyInvitable()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().IsPartyInvitable();
}

bool NetworkInterface::InviteToParty(const rlGamerHandle* pHandles, const unsigned numGamers)
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().SendPartyInvites(pHandles, numGamers);
}

bool NetworkInterface::IsPartyStatusPending()
{
	return CNetwork::IsNetworkPartyValid() && CNetwork::GetNetworkParty().IsPartyStatusPending();
}

bool NetworkInterface::NotifyPartyEnteringGame(const rlSessionInfo& sessionInfo)
{
	return CNetwork::GetNetworkParty().NotifyEnteringGame(sessionInfo);
}

bool NetworkInterface::NotifyPartyLeavingGame()
{
	return CNetwork::GetNetworkParty().NotifyLeavingGame();
}
/********************************** /PARTY **********************************************/

/***************************** EXTERNAL CHECKS ******************************************/
bool NetworkInterface::IsOnLoadingScreen()
{
	return CLoadingScreens::AreActive();
}

bool NetworkInterface::IsOnLandingPage()
{
#if GEN9_LANDING_PAGE_ENABLED
	return CLandingPage::IsActive();
#else
	return false;
#endif
}

bool NetworkInterface::IsOnInitialInteractiveScreen()
{
#if GEN9_LANDING_PAGE_ENABLED
	return CInitialInteractiveScreen::IsActive();
#else
	return false;
#endif
}

bool NetworkInterface::IsOnBootUi()
{
	return IsOnLandingPage() || IsOnInitialInteractiveScreen();
}

bool NetworkInterface::IsOnUnskippableBootUi()
{
	// on RDR, we have the legals and the IIS - this check was used to know when we settled on the landing page
	// so that we could proceed with invite flow or alerting
	// not currently any equivalent checks on V but here if we need it (currently, all screens are skipped automatically)
	return false;
}

bool NetworkInterface::IsInPlayerSwitch()
{
	return g_PlayerSwitch.IsActive();
}

void NetworkInterface::CloseAllUiForInvites()
{
	const bool isPauseMenuActive = CPauseMenu::IsActive();
	if(!isPauseMenuActive)
		return;

	const bool isLobbyActive = CPauseMenu::IsLobby();
	const bool isProcessingScripts = CTheScripts::ShouldBeProcessed();

	gnetDebug1("CloseAllUiForInvites :: ProcessScript: %s, PauseActive: %s, LobbyActive: %s",
		isProcessingScripts ? "True" : "False",
		isPauseMenuActive ? "True" : "False",
		isLobbyActive ? "True" : "False");

	// if script are not able to process, we need to close any pause menu activity on their behalf
	// so that they can process the invite and take action to join a session
	// exclude the lobby (script should process here anyway, but added as a failsafe)
	if(!isProcessingScripts && isPauseMenuActive && !isLobbyActive)
	{
		gnetDebug1("CloseAllUiForInvites :: Closing Pause Menu");
		CPauseMenu::BackOutOfSaveGameList();
		CPauseMenu::Close();
	}
}

bool NetworkInterface::IsReadyToCheckLandingPageStats()
{
#if RSG_GEN9
	return NETWORK_LANDING_PAGE_STATS_MGR.IsReady() && !NETWORK_LANDING_PAGE_STATS_MGR.IsWaitingForOnlineServices();
#else
	// for Gen8, just return true here to advance any flow state that waits on this
	return true;
#endif
}

bool NetworkInterface::IsReadyToCheckProfileStats()
{
	return 
		!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) &&
		!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT) &&
		CLiveManager::GetProfileStatsMgr().GetMPSynchIsDone();
}

bool NetworkInterface::IsReadyToCheckStats()
{
#if RSG_GEN9
#if !__FINAL
	int value = 0;
	if(PARAM_netForceReadyToCheckStats.Get(value))
		return value != 0;
#endif

	return IsReadyToCheckLandingPageStats() || IsReadyToCheckProfileStats();
#else
	// for Gen8, just return true here to advance any flow state that waits on this
	return true;
#endif
}

bool NetworkInterface::HasActiveCharacter()
{
#if RSG_GEN9
#if !__FINAL
	int value = 0;
	if(PARAM_netForceActiveCharacter.Get(value))
		return value != 0;
#endif

	if(!IsReadyToCheckStats())
		return false;

	return NETWORK_LANDING_PAGE_STATS_MGR.HasCharacterActive();
#else
	// for Gen8, assume true in code (script will sort this out)
	return true;
#endif
}

int NetworkInterface::GetActiveCharacterIndex()
{
#if !__FINAL
	int value = 0;
	if (PARAM_netForceActiveCharacterIndex.Get(value))
		return value;
#endif

	// this isn't in the landing page stats, needs profile stats
	if(IsReadyToCheckProfileStats())
		return StatsInterface::GetCurrentMultiplayerCharaterSlot();

	return -1;
}

bool NetworkInterface::HasCompletedMultiplayerIntro()
{
#if RSG_GEN9
#if !__FINAL
	int value = 0;
	if(PARAM_netForceCompletedIntro.Get(value))
		return value != 0;
#endif

	if(!IsReadyToCheckStats())
		return false;

	const int activeCharacter = GetActiveCharacterIndex();

	const bool bChar0InIntro = 
		NETWORK_LANDING_PAGE_STATS_MGR.IsCharacterActive(eCharacterIndex::CHAR_MP0) &&
		NETWORK_LANDING_PAGE_STATS_MGR.IsWindfallChar(eCharacterIndex::CHAR_MP0) &&
		!NETWORK_LANDING_PAGE_STATS_MGR.HasCompletedIntro(eCharacterIndex::CHAR_MP0);

	const bool bChar1InIntro =
		NETWORK_LANDING_PAGE_STATS_MGR.IsCharacterActive(eCharacterIndex::CHAR_MP1) &&
		NETWORK_LANDING_PAGE_STATS_MGR.IsWindfallChar(eCharacterIndex::CHAR_MP1) &&
		!NETWORK_LANDING_PAGE_STATS_MGR.HasCompletedIntro(eCharacterIndex::CHAR_MP1);

	if(activeCharacter == 0)
		return !bChar0InIntro;
	else if(activeCharacter == 1)
		return !bChar1InIntro;

	// if we don't have an active character (i.e. we don't know what character we will use)
	// need to return if either character is in the intro
	return !bChar0InIntro && !bChar1InIntro;
#else
	// for Gen8, assume true in code (script will sort this out)
	return true;
#endif
}

/***************************** /EXTERNAL CHECKS ******************************************/

void NetworkInterface::ClearLastDamageObject(netObject *networkObject)
{
    if(AssertVerify(networkObject) &&
       AssertVerify(networkObject->GetEntity()) &&
       AssertVerify(networkObject->GetEntity()->GetIsPhysical()))
    {
        CNetObjPhysical *netobjPhysical = SafeCast(CNetObjPhysical, networkObject);

        netobjPhysical->ClearLastDamageObject();
    }
}

void NetworkInterface::SetRoadNodesToOriginalOverNetwork(const CGameScriptId &scriptID,
														 const Vector3       &regionMin,
														 const Vector3       &regionMax)
{
	if(scriptInterface::IsNetworkScript(scriptID))
	{
		CNetworkRoadNodeWorldStateData::SetNodesToOriginal(scriptID, regionMin, regionMax, false);
	}
}

void NetworkInterface::SetRoadNodesToOriginalOverNetwork(const CGameScriptId &scriptID,
														 const Vector3       &regionMin,
														 const Vector3       &regionMax,
														 float               regionWidth)
{
	if(scriptInterface::IsNetworkScript(scriptID))
	{
		CNetworkRoadNodeWorldStateData::SetNodesToOriginalAngled(scriptID, regionMin, regionMax, regionWidth, false);
	}
}

void NetworkInterface::SwitchRoadNodesOnOverNetwork(const CGameScriptId &scriptID,
                                  const Vector3       &regionMin,
                                  const Vector3       &regionMax)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRoadNodeWorldStateData::SwitchOnNodes(scriptID, regionMin, regionMax, false);
    }
}

void NetworkInterface::SwitchRoadNodesOnOverNetwork(const CGameScriptId &scriptID,
                                  const Vector3       &regionStart,
                                  const Vector3       &regionEnd,
                                  float                regionWidth)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRoadNodeWorldStateData::SwitchOnNodesAngled(scriptID, regionStart, regionEnd, regionWidth, false);
    }
}

void NetworkInterface::SwitchRoadNodesOffOverNetwork(const CGameScriptId &scriptID,
                                   const Vector3       &regionMin,
                                   const Vector3       &regionMax)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRoadNodeWorldStateData::SwitchOffNodes(scriptID, regionMin, regionMax, false);
    }
}

void NetworkInterface::SwitchRoadNodesOffOverNetwork(const CGameScriptId &scriptID,
                                   const Vector3       &regionStart,
                                   const Vector3       &regionEnd,
                                   float                regionWidth)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRoadNodeWorldStateData::SwitchOffNodesAngled(scriptID, regionStart, regionEnd, regionWidth, false);
    }
}

void NetworkInterface::RegisterScriptDoor(CDoorSystemData& doorSystemData, bool network)
{
	gnetAssert(!doorSystemData.GetNetworkObject());

	if (doorSystemData.GetPersistsWithoutNetworkObject())
	{
		netObject* objects[MAX_NUM_NETOBJECTS];

		// try and find an existing network object for this door
		unsigned numObjects = GetObjectManager().GetAllObjects(objects, MAX_NUM_NETOBJECTS, true, false, true);

		for (unsigned i=0; i<numObjects; i++)
		{
			netObject* pObject = objects[i];

			if (pObject->GetObjectType() == NET_OBJ_TYPE_DOOR && !pObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
			{
				CNetObjDoor* pDoorObj = static_cast<CNetObjDoor*>(pObject);

				if (!pDoorObj->GetDoorSystemData() && pDoorObj->GetDoorSystemEnumHash() == (u32)doorSystemData.GetEnumHash())
				{
#if ENABLE_NETWORK_LOGGING
					netLogSplitter log(NetworkInterface::GetObjectManager().GetLog(), *CTheScripts::GetScriptHandlerMgr().GetLog());
					NetworkLogUtils::WriteLogEvent(log, "ASSIGNING_DOOR_ENTRY", "%s", pDoorObj->GetLogName());
					if (doorSystemData.GetScriptInfo())
					{
						log.WriteDataValue("Script", "%s", doorSystemData.GetScriptInfo()->GetScriptId().GetLogName());
					}
					log.WriteDataValue("Door system hash", "%u", doorSystemData.GetEnumHash());
#endif
					pDoorObj->SetDoorSystemData(&doorSystemData);
					return;
				}
			}
		}
	}

	CNetObjDoor* pNewDoorObj = NULL;

	if (network && NetworkInterface::IsGameInProgress())
	{
		CDoor* pDoor = doorSystemData.GetDoor();

		if (pDoor)
		{
			pNewDoorObj = static_cast<CNetObjDoor*>(pDoor->GetNetworkObject());

			if (pNewDoorObj)
			{
				// ambient doors converting to script doors is currently switched off (we can activate this later if needed).
				// if this is allowed, the enum hash for the door system data needs moved out of the door create node and into the script state node
				gnetAssertf(0, "Trying to register a script door that already has a network object (%s)", pNewDoorObj->GetLogName());
				return;
				/*
				if (pDoorObj->IsScriptObject())
				{
#if __ASSERT
					if (AssertVerify(pDoorObj->GetScriptObjInfo()) && pHandler->GetScriptId() != pDoorObj->GetScriptObjInfo()->GetScriptId())
					{
						Vector3 doorPos = doorSystemData.GetPosition();
						gnetAssertf(0, "The door at %f, %f, %f (%s) is already registered with script %s", doorPos.x, doorPos.y, doorPos.z, pDoorObj->GetLogName(), pDoorObj->GetScriptObjInfo()->GetScriptId().GetLogName());
					}
#endif
					bRegisterWithScript = false;
				}
				else
				{
					NetworkInterface::GetObjectManager().ConvertRandomObjectToScriptObject(pDoorObj);
				}*/
			}
			else 
			{
				// create a new network object for the door
				if(!CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_DOOR, true))
				{
					gnetAssertf(0, "Failed to register a script door! (ran out of networked doors or no free network id available)");
					return;
				}

				if (!RegisterObject(pDoor, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true))
				{
					gnetAssertf(0, "Failed to register script door (physical door exists)!");
				}
				else
				{
					pNewDoorObj = static_cast<CNetObjDoor*>(pDoor->GetNetworkObject());
				}
			}
		}
		
		if (!pNewDoorObj)
		{
			if(!CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_DOOR, true))
			{
				gnetAssertf(0, "Failed to register a script door! (ran out of networked doors or no free network id available)");
				return;
			}

			pNewDoorObj = static_cast<CNetObjDoor*>(CDoor::StaticCreateNetworkObject( GetObjectManager().GetObjectIDManager().GetNewObjectId(), 
														GetLocalPlayer()->GetPhysicalPlayerIndex(), 
														CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, 
														MAX_NUM_PHYSICAL_PLAYERS));

			pNewDoorObj->SetDoorSystemData(&doorSystemData);	

			GetObjectManager().RegisterNetworkObject(pNewDoorObj);
		}

		if (pNewDoorObj)
		{
			// set to clone on all machines running the script that registered it
			pNewDoorObj->SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT, true);

			// set to only clone on machines running the script that registered it
			pNewDoorObj->SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT, true);

			// allow the door to only migrate to script participants
			pNewDoorObj->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION, true);
		}
	}
}

void NetworkInterface::UnregisterScriptDoor(CDoorSystemData& doorSystemData)
{
	if (doorSystemData.GetNetworkObject())
	{
		CNetObjDoor* pDoorObj = static_cast<CNetObjDoor*>(doorSystemData.GetNetworkObject());

		GetObjectManager().UnregisterNetworkObject(pDoorObj, netObjectMgrBase::GAME_UNREGISTRATION);
	}
}

void NetworkInterface::SwitchCarGeneratorsOnOverNetwork(const CGameScriptId &scriptID,
                                      const Vector3       &regionMin,
                                      const Vector3       &regionMax)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkCarGenWorldStateData::SwitchOnCarGenerators(scriptID, regionMin, regionMax, false);
    }
}

void NetworkInterface::SwitchCarGeneratorsOffOverNetwork(const CGameScriptId &scriptID,
                                       const Vector3       &regionMin,
                                       const Vector3       &regionMax)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkCarGenWorldStateData::SwitchOffCarGenerators(scriptID, regionMin, regionMax, false);
    }
}

void NetworkInterface::OverridePopGroupPercentageOverNetwork(const CGameScriptId &scriptID,
                                           int                  popSchedule,
                                           u32                  popGroupHash,
                                           u32                  percentage)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkPopGroupOverrideWorldStateData::OverridePopGroupPercentage(scriptID, popSchedule, popGroupHash, percentage);
    }
}

void NetworkInterface::RemovePopGroupPercentageOverrideOverNetwork(const CGameScriptId &scriptID,
                                                 int                  popSchedule,
                                                 u32                  popGroupHash,
                                                 u32                  percentage)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkPopGroupOverrideWorldStateData::RemovePopGroupPercentageOverride(scriptID, popSchedule, popGroupHash, percentage);
    }
}

void NetworkInterface::AddPopMultiplierAreaOverNetwork(	const CGameScriptId &_scriptID,
										const Vector3		&_minWS,
										const Vector3		&_maxWS,
										const float			_pedDensity,
										const float			_trafficDensity,
										const bool			sphere,
										const bool			bCameraGlobalMultiplier
									)
{
    if(scriptInterface::IsNetworkScript(_scriptID))
    {
		CNetworkPopMultiplierAreaWorldStateData::AddArea(_scriptID, _minWS, _maxWS, _pedDensity, _trafficDensity, false, sphere, bCameraGlobalMultiplier);
	}
}

bool NetworkInterface::FindPopMultiplierAreaOverNetwork(const CGameScriptId &_scriptID, 
														const Vector3		&_minWS,
														const Vector3		&_maxWS,
														const float			 pedDensity,
														const float			 trafficDensity,
														const bool			 sphere,
														const bool			 bCameraGlobalMultiplier)
{
    if(scriptInterface::IsNetworkScript(_scriptID))
    {
		return CNetworkPopMultiplierAreaWorldStateData::FindArea(_minWS, _maxWS, pedDensity, trafficDensity, sphere, bCameraGlobalMultiplier);
	}
	
	return false;
}

void NetworkInterface::RemovePopMultiplierAreaOverNetwork(const CGameScriptId &_scriptID,
										const Vector3		&_minWS,
										const Vector3		&_maxWS,
										const float			_pedDensity,
										const float			_trafficDensity,
										const bool			_sphere,
										const bool			 bCameraGlobalMultiplier)
{
    if(scriptInterface::IsNetworkScript(_scriptID))
    {
		CNetworkPopMultiplierAreaWorldStateData::RemoveArea(_scriptID, _minWS, _maxWS, _pedDensity, _trafficDensity, false, _sphere, bCameraGlobalMultiplier);
	}
}

void NetworkInterface::SetModelHashPlayerLock_OverNetwork(const CGameScriptId &scriptID,
																		int const PlayerIndex, 
																		u32 const ModelId,
																		bool const Lock,
																		u32 const index)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
		CNetworkVehiclePlayerLockingWorldState::SetModelIdPlayerLock(scriptID, false, PlayerIndex, ModelId, Lock, index);
	}
}

void NetworkInterface::SetModelInstancePlayerLock_OverNetwork(const CGameScriptId &scriptID,
																				int const PlayerIndex, 
																				int const VehicleIndex, 
																				bool const Lock,
																				u32 const index)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
		CNetworkVehiclePlayerLockingWorldState::SetModelInstancePlayerLock(scriptID, false, PlayerIndex, VehicleIndex, Lock, index);
	}
}

void NetworkInterface::ClearModelHashPlayerLockForPlayer_OverNetwork(const CGameScriptId& scriptID, u32 const PlayerIndex, u32 const ModelHash, bool const lock, u32 const index)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
		CNetworkVehiclePlayerLockingWorldState::ClearModelIdPlayerLock(scriptID, false, PlayerIndex, ModelHash, lock, index);
	}
}

void NetworkInterface::ClearModelInstancePlayerLockForPlayer_OverNetwork(const CGameScriptId& scriptID, u32 const PlayerIndex, u32 const InstanceIndex, bool const lock, u32 const index)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
		CNetworkVehiclePlayerLockingWorldState::ClearModelInstancePlayerLock(scriptID, false, PlayerIndex, InstanceIndex, lock, index);
	}
}

void NetworkInterface::CreateRopeOverNetwork(const CGameScriptId &scriptID,
                           int                  ropeID,
                           const Vector3       &position,
                           const Vector3       &rotation,
                           float                maxLength,
                           int                  type,
                           float                initLength,
                           float                minLength,
                           float                lengthChangeRate,
                           bool                 ppuOnly,
                           bool                 collisionOn,
                           bool                 lockFromFront,
                           float                timeMultiplier,
                           bool                 breakable,
						   bool					pinned
						   )
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRopeWorldStateData::RopeParameters parameters(position, rotation, maxLength, type, initLength, minLength, lengthChangeRate,
                                                              ppuOnly, collisionOn, lockFromFront, timeMultiplier, breakable, pinned);

        CNetworkRopeWorldStateData::CreateRope(scriptID, ropeID, parameters);
    }
}

void NetworkInterface::DeleteRopeOverNetwork(const CGameScriptId &scriptID, int ropeID)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkRopeWorldStateData::DeleteRope(scriptID, ropeID);
    }
}

void NetworkInterface::StartPtFXOverNetwork(const CGameScriptId &scriptID,
                          u32                  ptFXHash,
                          u32                  ptFXAssetHash,
                          const Vector3       &fxPos,
                          const Vector3       &fxRot,
                          float                scale,
                          u8                   invertAxes,
                          int                  scriptPTFX,
						  CEntity*			   pEntity,
						  int				   boneIndex,
						  float				   r,
						  float				   g,
						  float				   b,
						  bool                 terminateOnOwnerLeave,
						  u64                  ownerPeerID)
{
    if(scriptInterface::IsNetworkScript(scriptID) && scriptPTFX!=0)
    {
        CNetworkPtFXWorldStateData::AddScriptedPtFX(scriptID, ptFXHash, ptFXAssetHash, fxPos, fxRot, scale, invertAxes, scriptPTFX, pEntity, boneIndex, r, g, b, terminateOnOwnerLeave, ownerPeerID);
    }
}

void NetworkInterface::ModifyEvolvePtFxOverNetwork(const CGameScriptId &scriptID, u32 evoHash, float evoVal, int scriptPTFX)
{
	if(scriptInterface::IsNetworkScript(scriptID))
	{
		CNetworkPtFXWorldStateData::ModifyScriptedEvolvePtFX(scriptID, evoHash, evoVal, scriptPTFX);
	}
}

void NetworkInterface::ModifyPtFXColourOverNetwork(const CGameScriptId &scriptID, float r, float g, float b, int scriptPTFX)
{
	if(scriptInterface::IsNetworkScript(scriptID))
	{
		CNetworkPtFXWorldStateData::ModifyScriptedPtFXColour(scriptID, r, g, b, scriptPTFX);
	}
}

void NetworkInterface::StopPtFXOverNetwork(const CGameScriptId &scriptID,
                         int                  scriptPTFX)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkPtFXWorldStateData::RemoveScriptedPtFX(scriptID, scriptPTFX);
    }
}

void NetworkInterface::AddScenarioBlockingAreaOverNetwork(const CGameScriptId &scriptID,
                                        const Vector3       &regionMin,
                                        const Vector3       &regionMax,
                                        const int            blockingAreaID,
										bool				bCancelActive,
										bool				bBlocksPeds,
										bool				bBlocksVehicles)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkScenarioBlockingAreaWorldStateData::AddScenarioBlockingArea(scriptID, regionMin, regionMax, blockingAreaID, bCancelActive, bBlocksPeds, bBlocksVehicles);
    }
}

void NetworkInterface::RemoveScenarioBlockingAreaOverNetwork(const CGameScriptId &scriptID,
                                           const int            blockingAreaID)
{
    if(scriptInterface::IsNetworkScript(scriptID))
    {
        CNetworkScenarioBlockingAreaWorldStateData::RemoveScenarioBlockingArea(scriptID, blockingAreaID);
    }
}

int NetworkInterface::GetLocalRopeIDFromNetworkID(int networkRopeID)
{
    int localRopeID = -1;

    if(IsGameInProgress())
    {
        localRopeID = CNetworkRopeWorldStateData::GetRopeIDFromNetworkID(networkRopeID);
    }

    return localRopeID;
}

int NetworkInterface::GetNetworkRopeIDFromLocalID(int localRopeID)
{
    int networkRopeID = -1;

    if(IsGameInProgress())
    {
        networkRopeID = CNetworkRopeWorldStateData::GetNetworkIDFromRopeID(localRopeID);
    }

    return networkRopeID;
}

bool NetworkInterface::IsPosLocallyControlled(const Vector3 &position)
{
    bool usingGameplayCamera = camInterface::IsRenderingDirector(camInterface::GetGameplayDirector());
    bool usingDebugCamera    = camInterface::GetDebugDirector().IsFreeCamActive();

	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	bool usingDroneCamera	 = pLocalPlayer ? pLocalPlayer->GetPedResetFlag(CPED_RESET_FLAG_UsingDrone) : false;

    if((!usingGameplayCamera && !usingDebugCamera && !usingDroneCamera) || IsInSpectatorMode())
    {
        // we don't want to be controlling positions when using non-gameplay cameras as
        // the players focus position may be far away from the ped position, this can lead
        // to issues where objects are created by the local machine and immediately passed on to another player,
        // and then removed due to being out of scope - giving the possibility of multiple objects created on top of another
        return false;
    }
    else
    {
		if(!CNetwork::IsInTutorialSession())
        {
            return CNetworkWorldGridManager::IsPosLocallyControlled(position);
        }
        else
        {
            return CNetwork::IsNetworkTutorialSessionTurn();
        }
    }
}

bool NetworkInterface::AreThereAnyConcealedPlayers()
{
	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
		const CPed* pPlayerPed = player ? player->GetPlayerPed() : NULL;
		const CNetObjPhysical* pNetObjPhysical = pPlayerPed ? static_cast<const CNetObjPhysical*>(pPlayerPed->GetNetworkObject()) : NULL;

		if(pNetObjPhysical && pNetObjPhysical->IsConcealed())
			return true;
	}

	return false;
}

void NetworkInterface::ForceSynchronisation(CDynamicEntity *entity)
{
    if(gnetVerifyf(entity, "Calling ForceSynchronisation on a NULL entity!"))
    {
        if(!entity->IsNetworkClone())
        {
            netObject *networkObject = entity->GetNetworkObject();

            if(gnetVerifyf(networkObject, "Calling ForceSynchronisation on an entity with no network object!"))
            {
                if(!networkObject->GetSyncData() && networkObject->GetEntity())
                {
                    networkObject->StartSynchronising();
                }
                else
                {
                    // ensure the object doesn't stop syncing immediately
                    // after this command was called
                    networkObject->ResetSyncDataChangeTimer();
                }
            }
        }
    }
}

bool NetworkInterface::IsPedInVehicleOnOwnerMachine(CPed *ped)
{
    if(gnetVerifyf(ped, "Calling IsPedInVehicleOnOwnerMachine on a NULL ped!"))
    {
        if(ped->IsNetworkClone())
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());

            if(netObjPed->GetPedsTargetVehicleId() == NETWORK_INVALID_OBJECT_ID)
            {
                return false;
            }
            else
            {
                return true;
            }
        }

        return ped->GetIsInVehicle();
    }

    return false;
}

bool NetworkInterface::IsVehicleLockedForPlayer(const CVehicle* pVeh, const CPed* pPlayerPed)
{
	if (!NetworkInterface::IsGameInProgress())
		return false;

	if (!pVeh || !pPlayerPed || !pPlayerPed->IsPlayer() || !pPlayerPed->GetNetworkObject())
		return false;

	const CNetObjVehicle* pNetObjVeh = static_cast<const CNetObjVehicle*>(pVeh->GetNetworkObject());
	if (!pNetObjVeh)
		return false;

	const CNetGamePlayer* pPlayer = pPlayerPed->GetNetworkObject()->GetPlayerOwner();
	if (!pPlayer)
		return false;

	PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();
	if (playerIndex == INVALID_PLAYER_INDEX)
		return false;

	return pNetObjVeh->IsLockedForPlayer(playerIndex) ? true : false;
}

void NetworkInterface::SetTargetLockOn(const CVehicle* vehicle, unsigned lockOnState)
{
	if(NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
		if(localPlayer)
		{
			CPed* localPed = localPlayer->GetPlayerPed();
			if(localPed && localPed->GetNetworkObject())
			{
				CNetObjPlayer* netLocalPed = SafeCast(CNetObjPlayer, localPed->GetNetworkObject());
				
				if(vehicle && vehicle->GetNetworkObject())
				{
					if(lockOnState == (unsigned)CEntity::HLOnS_NONE)
					{
						netLocalPed->SetScriptAimTargetObjectId(NETWORK_INVALID_OBJECT_ID);
					}
					else
					{
						netLocalPed->SetScriptAimTargetObjectId(vehicle->GetNetworkObject()->GetObjectID());
					}

					netLocalPed->SetScriptAimTargetLockOnState(lockOnState);
				}
				else
				{
					netLocalPed->SetScriptAimTargetObjectId(NETWORK_INVALID_OBJECT_ID);
				}
			}
		}
	}
}

bool NetworkInterface::IsInvalidVehicleForDriving(const CPed* pPed, const CVehicle* pVehicle)
{
	bool bInvalidVehicleForDriving = false;

	if (IsGameInProgress() && pPed && pVehicle)
	{
		if (pPed->GetNetworkObject() && pVehicle->GetNetworkObject())
		{
			//We dont care is the ped is a player.
			if (pPed->IsPlayer())
				return bInvalidVehicleForDriving;

			//We dont care is the ped is a clone.
			if (pPed->IsNetworkClone())
				return bInvalidVehicleForDriving;

			//We only care for Random wandering Peds.
			if (pPed->PopTypeGet() == POPTYPE_RANDOM_AMBIENT)
			{
				//We dont care about helis, planes or trains.
				if(pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() || pVehicle->InheritsFromTrain())
					return bInvalidVehicleForDriving;

				//We dont care if the is not driving the vehicle.
				const CPed* pVehicleDriver = pVehicle->GetDriver();
				if (pVehicleDriver && pVehicleDriver != pPed)
					return bInvalidVehicleForDriving;

				CNetObjPed*     pedNetworkObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
				CNetObjVehicle* vehNetworkObj = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());

				//Respawn peds can not drive any vehicles.
				if (pedNetworkObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_RESPAWNPED))
				{
					bInvalidVehicleForDriving = true;
				}
				//If the Vehicle is locked for any player then abandon it.
				else if (vehNetworkObj->IsLockedForAnyPlayer())
				{
					//! TO DO - should we set warp flag here to force a get out?
					bInvalidVehicleForDriving = true;
				}
				// can't do this as it makes script peds get out of their vehicles when the scripts mark them as no longer needed
				/*else if (vehNetworkObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
				{
					setPedOutOfVehicle = true;
				}*/
			}
		}
	}

	return bInvalidVehicleForDriving;
}

bool NetworkInterface::IsInvalidVehicleForAmbientPedDriving(const CVehicle* pVehicle)
{
	bool bInvalidVehicleForAmbientPedDriving = false;

	if (IsGameInProgress() && pVehicle)
	{
		//We dont care about helis, planes or trains.
		if(pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() || pVehicle->InheritsFromTrain())
			return bInvalidVehicleForAmbientPedDriving;

		CNetObjVehicle* vehNetworkObj = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());

		//If the Vehicle is locked for any player then abandon it.
		if (vehNetworkObj && vehNetworkObj->IsLockedForAnyPlayer())
		{
			//! TO DO - should we set warp flag here to force a get out?
			bInvalidVehicleForAmbientPedDriving = true;
		}
		// can't do this as it makes script peds get out of their vehicles when the scripts mark them as no longer needed
		/*else if (vehNetworkObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
		{
			setPedOutOfVehicle = true;
		}*/
	}

	return bInvalidVehicleForAmbientPedDriving;
}

bool NetworkInterface::IsVehicleInterestingToAnyPlayer(const CVehicle *pVehicle)
{
    if(pVehicle)
    {
        if(pVehicle == CVehiclePopulation::GetInterestingVehicle())
        {
            return true;
        }

        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(player->GetPlayerPed() && player->GetPlayerPed()->GetNetworkObject())
            {
                CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, player->GetPlayerPed()->GetNetworkObject());

                if(netObjPlayer->IsMyVehicleMyInteresting() && player->GetPlayerPed()->GetMyVehicle() == pVehicle)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void NetworkInterface::IgnoreVehicleUpdates(CPed *ped, u16 timeToIgnore)
{
    if(gnetVerifyf(ped, "Calling IgnoreVehicleUpdates on a NULL ped!"))
    {
        if(gnetVerifyf(ped->IsNetworkClone(), "Calling IgnoreVehicleUpdates on local ped!"))
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());
            netObjPed->IgnoreVehicleUpdates(timeToIgnore);
        }
    }
}

void NetworkInterface::ForceVehicleForPedUpdate(CPed *ped)
{
    if(gnetVerifyf(ped, "Calling ForceVehicleForPedUpdate on a NULL ped!"))
    {
        if(gnetVerifyf(ped->IsNetworkClone(), "Calling ForceVehicleForPedUpdate on local ped!"))
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());

            if(netObjPed->GetPedsTargetVehicleId() != NETWORK_INVALID_OBJECT_ID && !ped->GetIsInVehicle())
            {
                netObject *netObjVehicle = NetworkInterface::GetObjectManager().GetNetworkObject(netObjPed->GetPedsTargetVehicleId());
                CVehicle  *vehicle       = NetworkUtils::GetVehicleFromNetworkObject(netObjVehicle);

                if(vehicle)
                {
                    if(ped->GetUsingRagdoll())
                    {
						nmEntityDebugf(ped, "NetworkInterface::ForceVehicleForPedUpdate switching to animated");
	                    ped->SwitchToAnimated();
                    }

                    netObjPed->SetClonePedInVehicle(vehicle, netObjPed->GetPedsTargetSeat());
                }
            }
            else if(netObjPed->GetPedsTargetVehicleId() == NETWORK_INVALID_OBJECT_ID && ped->GetIsInVehicle())
            {
                netObjPed->SetClonePedOutOfVehicle(false, 0);
            }
        }
    }
}

bool NetworkInterface::CanUseRagdoll(CPed *ped, eRagdollTriggerTypes UNUSED_PARAM(trigger), CEntity *UNUSED_PARAM(entityResponsible), float UNUSED_PARAM(pushValue))
{
    // prevent network clones on top of network objects from ragdolling until the owner ragdolls
    if(ped && ped->IsNetworkClone())
    {
        CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, ped->GetNetworkObject()->GetNetBlender());

        ObjectId objectStoodOnID = NETWORK_INVALID_OBJECT_ID;
        Vector3  objectStoodOnOffset;

        bool isRagdolling       = netBlenderPed && netBlenderPed->IsRagdolling();
        bool isStandingOnObject = netBlenderPed && netBlenderPed->IsStandingOnObject(objectStoodOnID, objectStoodOnOffset);

        if(isStandingOnObject && !isRagdolling)
        {
            return false;
        }
    }

    return true;
}

void NetworkInterface::OnSwitchToRagdoll(CPed &ped, bool pedSetOutOfVehicle)
{
    CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped.GetNetworkObject());

    if(netObjPed && !ped.IsNetworkClone() && netObjPed->GetSyncData())
    {
        netObjPed->RequestForceSendOfRagdollState();

        if(pedSetOutOfVehicle)
        {
            netObjPed->RequestForceSendOfTaskState();
        }
    }
}

void NetworkInterface::ForceMotionStateUpdate(CPed &ped)
{
    CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped.GetNetworkObject());

    if(netObjPed && !ped.IsNetworkClone() && netObjPed->GetSyncData())
    {
        netObjPed->RequestForceSendOfRagdollState();
        netObjPed->RequestForceSendOfTaskState();
    }
}

void NetworkInterface::ForceTaskStateUpdate(CPed &ped)
{
	CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped.GetNetworkObject());

	if(netObjPed && !ped.IsNetworkClone() && netObjPed->GetSyncData())
	{
		netObjPed->RequestForceSendOfTaskState();
	}
}

void NetworkInterface::ForceHealthNodeUpdate(CVehicle &vehicle)
{
	CNetObjVehicle *pNetObjVehicle = SafeCast(CNetObjVehicle, vehicle.GetNetworkObject());

	if(pNetObjVehicle && !vehicle.IsNetworkClone() && pNetObjVehicle->GetSyncData())
	{
		pNetObjVehicle->RequestForceSendOfHealthNode();
	}
}

void NetworkInterface::ForceScriptGameStateNodeUpdate(CVehicle &vehicle)
{
	CNetObjVehicle *pNetObjVehicle = SafeCast(CNetObjVehicle, vehicle.GetNetworkObject());

	if(pNetObjVehicle && !vehicle.IsNetworkClone() && pNetObjVehicle->GetSyncData())
	{
		pNetObjVehicle->RequestForceScriptGameStateNodeUpdate();
	}
}


void NetworkInterface::ForceCameraUpdate(CPed &ped)
{
    if(gnetVerifyf(ped.IsLocalPlayer(), "Trying to force a camera update for a non-player ped!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, ped.GetNetworkObject());

        if(netObjPlayer && !ped.IsNetworkClone() && netObjPlayer->GetSyncData())
        {
            CPlayerSyncTree *playerSyncTree = static_cast<CPlayerSyncTree *>(netObjPlayer->GetSyncTree());
			playerSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, netObjPlayer->GetActivationFlags(), netObjPlayer, *playerSyncTree->GetPlayerCameraNode());
        }
    }
}

void NetworkInterface::OnEntitySmashed(CPhysical *physical, s32 component, bool smashed)
{
    if(physical && physical->GetNetworkObject())
    {
        if(physical->GetIsTypeVehicle())
        {
            CVehicle *vehicle = SafeCast(CVehicle, physical);
			int boneID = -1;

			for( int i = (int)VEH_FIRST_WINDOW; i <= (int)VEH_WINDOW_RR; i++ )
			{
				boneID = vehicle->GetBoneIndex( (eHierarchyId)( i ) );
				int componentFromBone = vehicle->GetVehicleFragInst()->GetComponentFromBoneIndex( boneID );

				if( component == componentFromBone )
				{
					CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, physical->GetNetworkObject());
					netObjVehicle->SetWindowState( (eHierarchyId)i, smashed );
					break;
				}
			}
        }
    }
}

void NetworkInterface::OnExplosionImpact(CPhysical *physical, float impulseMag)
{
    if(physical && physical->GetNetworkObject())
    {
        CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

        netObjPhysical->RegisterExplosionImpact(impulseMag);
    }
}

void NetworkInterface::ForceHighUpdateLevel(CPhysical* physical)
{
	if(physical && physical->GetNetworkObject())
	{
		CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
		netObjPhysical->ForceHighUpdateLevel();
	}
}

void NetworkInterface::OnVehiclePartDamage(CVehicle *vehicle, eHierarchyId ePart, u8 state)
{
    if(vehicle && !vehicle->IsNetworkClone() && vehicle->GetNetworkObject())
    {
        CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());
        netObjVehicle->SetVehiclePartDamage(ePart, state);
    }
}

bool NetworkInterface::ShouldTriggerDamageEventForZeroDamage(CPhysical* physical)
{
	if(physical && physical->GetNetworkObject())
	{
		CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
		return netPhysical->GetTriggerDamageEventForZeroDamage();
	}
	return false;
}

void NetworkInterface::GivePedScriptedTask(CPed *ped, aiTask *task, const char* NET_ASSERTS_ONLY(commandName))
{
	if(ped && gnetVerifyf(ped->GetNetworkObject(), "Trying to give a non-networked ped a scripted task!"))
	{
		// we don't give remote peds control movement tasks directly, we give them the subtasks
		if(task && task->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			CTaskComplexControlMovement *moveTask = SafeCast(CTaskComplexControlMovement, task);
			task = moveTask->GetBackupCopyOfMovementSubtask() ? moveTask->GetBackupCopyOfMovementSubtask()->Copy() : 0;

			// sub task not currently synced
			aiAssertf(moveTask->GetMainSubTaskType() == CTaskTypes::TASK_NONE, "%s : %s (%s) - CTaskComplexControlMovement - sub task not currently supported in MP", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, task ? task->GetName().c_str() : "??");
			
			delete moveTask;
		}

		if (task)
		{
			if (gnetVerifyf(CQueriableInterface::CanTaskBeGivenToNonLocalPeds(task->GetTaskType()), "%s : %s (%s) - This script command cannot be used on remotely owned ped in network game scripts!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, task->GetName().c_str()))
			{
				CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());
				netObjPed->AddPendingScriptedTask(SafeCast(CTask, task));
			}
			else
			{
				delete task;
			}
		}
	}
}

int NetworkInterface::GetPendingScriptedTaskStatus(const CPed *ped)
{
    int taskStatus = -1;

    if(ped && ped->GetNetworkObject())
    {
        CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());
        taskStatus = netObjPed->GetPendingScriptedTaskStatus();
    }

    return taskStatus;
}

Vector3 NetworkInterface::GetLastPosReceivedOverNetwork(const CDynamicEntity *entity)
{
    if(gnetVerifyf(entity, "Trying to get the last position received over the network for an invalid entity!") &&
       gnetVerifyf(entity->GetNetworkObject(), "Trying to get the last position received over the network for a non-networked entity!"))
    {
        netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, entity->GetNetworkObject()->GetNetBlender());

        if(blender)
        {
            return blender->GetLastPositionReceived();
        }
    }

    return VEC3_ZERO;
}

Vector3 NetworkInterface::GetLastVelReceivedOverNetwork (const CDynamicEntity *entity)
{
    if(gnetVerifyf(entity, "Trying to get the last velocity received over the network for an invalid entity!") &&
       gnetVerifyf(entity->GetNetworkObject(), "Trying to get the last velocity received over the network for a non-networked entity!"))
    {
        netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, entity->GetNetworkObject()->GetNetBlender());
        return blender->GetLastVelocityReceived();
    }

    return VEC3_ZERO;
}

Matrix34 NetworkInterface::GetLastMatrixReceivedOverNetwork(const CVehicle *vehicle)
{
    if(gnetVerifyf(vehicle, "Trying to get the last velocity received over the network for an invalid vehicle!") &&
        gnetVerifyf(vehicle->GetNetworkObject(), "Trying to get the last velocity received over the network for a non-networked vehicle!"))
    {
        CNetBlenderPhysical *blender = SafeCast(CNetBlenderPhysical, vehicle->GetNetworkObject()->GetNetBlender());
        return blender->GetLastMatrixReceived();
    }

    return M34_IDENTITY;
}


Vector3 NetworkInterface::GetPredictedVelocity(const CDynamicEntity *entity, const float maxSpeedToPredict)
{
    if(gnetVerifyf(entity, "Trying to get a predicted velocity for an invalid entity!") &&
       gnetVerifyf(entity->IsNetworkClone(), "Trying to get a predicted velocity for a non-network clone!"))
    {
        netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, entity->GetNetworkObject()->GetNetBlender());
        return blender->GetPredictedVelocity(maxSpeedToPredict);
    }

    return VEC3_ZERO;
}

Vector3 NetworkInterface::GetPredictedAccelerationVector(const CDynamicEntity *entity)
{
    if(gnetVerifyf(entity, "Trying to get a predicted acceleration for an invalid entity!") &&
       gnetVerifyf(entity->IsNetworkClone(), "Trying to get a predicted acceleration for a non-network clone!"))
    {
        netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, entity->GetNetworkObject()->GetNetBlender());
        return blender->GetPredictedAccelerationVector();
    }

    return VEC3_ZERO;
}

float NetworkInterface::GetLastHeadingReceivedOverNetwork(const CPed *ped)
{
    if(gnetVerifyf(ped, "Trying to get heading for an invalid ped!") &&
       gnetVerifyf(ped->IsNetworkClone(), "Trying to get heading for a non-network clone!"))
    {
        CNetBlenderPed *blender = SafeCast(CNetBlenderPed, ped->GetNetworkObject()->GetNetBlender());
        return blender->GetLastHeadingReceived();
    }

    return 0.0f;
}

void NetworkInterface::UpdateHeadingBlending(const CPed &ped)
{
    if(gnetVerifyf(ped.IsNetworkClone(), "Trying to blend heading for a non-network clone!"))
    {
        CNetBlenderPed *blender = SafeCast(CNetBlenderPed, ped.GetNetworkObject()->GetNetBlender());
        return blender->UpdateHeadingBlending();
    }
}

void NetworkInterface::UseAnimatedRagdollFallbackBlending(const CPed &ped)
{
    if(gnetVerifyf(ped.IsNetworkClone(), "Trying to set animated ragdoll fallback blending on a non-network clone!"))
    {
        CNetBlenderPed *blender = SafeCast(CNetBlenderPed, ped.GetNetworkObject()->GetNetBlender());
        blender->UseAnimatedRagdollFallbackBlendingThisFrame();
    }
}

void NetworkInterface::OnDesiredVelocityCalculated(const CPed &ped)
{
    if(ped.IsNetworkClone())
    {
        CNetBlenderPed *blender = SafeCast(CNetBlenderPed, ped.GetNetworkObject()->GetNetBlender());
        blender->OnDesiredVelocityCalculated();
    }
}

void NetworkInterface::OverrideMoveBlendRatiosThisFrame(CPed &ped, float moveBlendRatio, float lateralMoveBlendRatio)
{
    CNetObjPed *netObjPed = SafeCast(CNetObjPed, ped.GetNetworkObject());

    if(gnetVerifyf(netObjPed, "Trying to override move blend ratios for a non-networked ped!") &&
       gnetVerifyf(netObjPed->IsClone(), "Trying to override move blend ratios for a locally controlled ped!"))
    {
        ped.GetMotionData()->SetDesiredMoveBlendRatio(moveBlendRatio, lateralMoveBlendRatio);
        netObjPed->FlagOverridingMoveBlendRatios();
    }
}

void NetworkInterface::GoStraightToTargetPosition(const CDynamicEntity *entity)
{
    if(gnetVerifyf(entity, "Trying to move an invalid entity to it's target position!") &&
       gnetVerifyf(entity->GetNetworkObject(), "Trying to move a non-networked entity to it's target position!"))
    {
        netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, entity->GetNetworkObject()->GetNetBlender());
        return blender->GoStraightToTarget();
    }
}

void NetworkInterface::OverrideNetworkBlenderForTime(const CPhysical *physical, unsigned timeToOverrideBlender)
{
    if(gnetVerifyf(physical, "Trying to override network blending for an invalid entity!") &&
       gnetVerifyf(physical->GetNetworkObject(), "Trying to override network blending for a non-networked entity!"))
    {
        CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
        netObjPhysical->SetCollisionTimer(timeToOverrideBlender);
    }
}

void NetworkInterface::OverrideNetworkBlenderUntilAcceptingObjects(const CPhysical *physical)
{
    if(gnetVerifyf(physical, "Trying to override network blending for an invalid entity!") &&
       gnetVerifyf(physical->GetNetworkObject(), "Trying to override network blending for a non-networked entity!"))
    {
        CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
        netObjPhysical->OverrideBlenderUntilAcceptingObjects();
    }
}

//void NetworkInterface::OverrideNetworkBlenderTargetPosition(const CPed *ped, const Vector3 &position)
//{
//    if(gnetVerifyf(ped, "Trying to override network blender position for an invalid entity!") &&
//       gnetVerifyf(ped->IsNetworkClone(), "Trying to override network blender position for a non-clone!"))
//    {
//        CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, ped->GetNetworkObject()->GetNetBlender());
//
//        if(netBlenderPed)
//        {
//            netBlenderPed->OverrideBlenderTargetPos(position);
//        }
//    }
//}

void NetworkInterface::DisableNetworkZBlendingForRagdolls(const CPed &ped)
{
	if(gnetVerifyf(ped.GetNetworkObject(), "Trying to disable Z blending for a non-networked ped!") &&
		gnetVerifyf(ped.GetNetworkObject()->GetNetBlender(), "Trying to disable Z blending for a ped that has no network blender!"))
	{
		static_cast<CNetBlenderPed*>(ped.GetNetworkObject()->GetNetBlender())->DisableRagdollZBlending();
	}
}

bool NetworkInterface::IsInSinglePlayerPrivateSession()
{
	if (CScriptHud::bUsingMissionCreator)
	{
		return true;
	}

	if (CNetwork::IsGameInProgress() && NetworkInterface::IsSessionActive())
	{
		return (CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) && 0 == NetworkInterface::GetMatchConfig().GetMaxPrivateSlots());
	}

	return false;
}

bool NetworkInterface::IsInTutorialSession()
{
	CNetGamePlayer* pLocal = NetworkInterface::GetLocalPlayer();
	if(pLocal)
		return IsPlayerInTutorialSession(*pLocal);

	return false;
}

bool NetworkInterface::IsTutorialSessionChangePending()
{
	CNetGamePlayer* pLocal = NetworkInterface::GetLocalPlayer();
	if(pLocal)
		return IsPlayerTutorialSessionChangePending(*pLocal);

	return false;
}

bool NetworkInterface::IsPlayerInTutorialSession(const CNetGamePlayer& player)
{
	if(!NetworkInterface::IsGameInProgress())
		return false;

	// verify player ped
	CPed* pPlayerPed = player.GetPlayerPed();
	if(!pPlayerPed)
		return false;

	// verify network object
	CNetObjPlayer* pPlayerNetObj = pPlayerPed->GetNetworkObject() ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : NULL;
	if(!pPlayerNetObj)
		return false;

	return pPlayerNetObj->IsInTutorialSession();
}

u8 NetworkInterface::GetPlayerTutorialSessionIndex(const CNetGamePlayer &player)
{
	if(!NetworkInterface::IsGameInProgress())
		return CNetObjPlayer::INVALID_TUTORIAL_INDEX;

	// verify player ped
	CPed* pPlayerPed = player.GetPlayerPed();
	if(!pPlayerPed)
		return CNetObjPlayer::INVALID_TUTORIAL_INDEX;

	// verify network object
	CNetObjPlayer* pPlayerNetObj = pPlayerPed->GetNetworkObject() ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : NULL;
	if(!pPlayerNetObj)
		return CNetObjPlayer::INVALID_TUTORIAL_INDEX;

	return pPlayerNetObj->GetTutorialIndex();
}

bool NetworkInterface::IsPlayerTutorialSessionChangePending(const CNetGamePlayer& player)
{
	if(!NetworkInterface::IsGameInProgress())
		return false;

	// verify player ped
	CPed* pPlayerPed = player.GetPlayerPed();
	if(!pPlayerPed)
		return false;

	// verify network object
	CNetObjPlayer* pPlayerNetObj = pPlayerPed->GetNetworkObject() ? SafeCast(CNetObjPlayer, pPlayerPed->GetNetworkObject()) : NULL;
	if(!pPlayerNetObj)
		return false;

	return pPlayerNetObj->IsTutorialSessionChangePending();
}

bool NetworkInterface::ArePlayersInDifferentTutorialSessions(const CNetGamePlayer &firstPlayer, const CNetGamePlayer &secondPlayer)
{
    bool inDifferentSessions = false;

    if(&firstPlayer != &secondPlayer)
    {
        if(firstPlayer.GetPlayerPed()  && firstPlayer.GetPlayerPed()->GetNetworkObject() &&
           secondPlayer.GetPlayerPed() && secondPlayer.GetPlayerPed()->GetNetworkObject())
        {
            const CNetObjPlayer *firstNetObjPlayer  = SafeCast(const CNetObjPlayer, firstPlayer.GetPlayerPed()->GetNetworkObject());
            const CNetObjPlayer *secondNetObjPlayer = SafeCast(const CNetObjPlayer, secondPlayer.GetPlayerPed()->GetNetworkObject());

            if((firstNetObjPlayer->GetTutorialIndex()      == CNetObjPlayer::SOLO_TUTORIAL_INDEX)          ||
               (secondNetObjPlayer->GetTutorialIndex()     == CNetObjPlayer::SOLO_TUTORIAL_INDEX)          ||
               (firstNetObjPlayer->GetTutorialIndex()      != secondNetObjPlayer->GetTutorialIndex())      ||
               (firstNetObjPlayer->GetTutorialInstanceID() != secondNetObjPlayer->GetTutorialInstanceID()))
            {
                inDifferentSessions = true;
            }
        }
    }

    return inDifferentSessions;
}

bool NetworkInterface::IsEntityConcealed(const CEntity &entity)
{
    if(entity.GetIsPhysical())
    {
        const CNetObjPhysical *netObjPhysical = SafeCast(const CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(&entity));

        if(netObjPhysical && netObjPhysical->IsConcealed())
        {
            return true;
        }
    }

    return false;
}

bool NetworkInterface::IsClockTimeOverridden()
{
    return CNetwork::IsClockTimeOverridden();
}

bool NetworkInterface::RequestNetworkAnimRequest(CPed const* pPed, u32 const animHash, bool bRemoveExistingRequests)
{
	if( (pPed && pPed->GetNetworkObject() ) && (animHash != 0) )
	{
		return CNetworkStreamingRequestManager::RequestNetworkAnimRequest(pPed, animHash, bRemoveExistingRequests);
	}

	return false;
}

bool NetworkInterface::RemoveAllNetworkAnimRequests(CPed const* pPed)
{
	if(pPed && pPed->GetNetworkObject())
	{
		return CNetworkStreamingRequestManager::RemoveAllNetworkAnimRequests(pPed);
	}

	return false;
}

bool NetworkInterface::IsRelevantToWaypoint(CPed* pPed)
{
	// this should always be good at this point
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if(!pLocalPlayer)
	{
		gnetDebug1("IsRelevantToGPS :: No local player! Not relevant!");
		return false;
	}

	// check if local player
	CPed* pLocalPed = CGameWorld::FindLocalPlayer();
	if(pLocalPed == pPed)
	{
		gnetDebug1("IsRelevantToGPS :: Local ped! Relevant!");
		return true; 
	}

	// not in network game, previous checks should have caught this
	if(!IsGameInProgress())
	{
		gnetDebug1("IsRelevantToGPS :: Not in network game! Not relevant!");
		return false;
	}

	if (s_ignoreRemoteWaypoints)
	{
		gnetDebug1("IsRelevantToGPS :: Ignoring remote waypoints! Not relevant!");
		return false; 
	}

	if(SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject())->GetWaypointOverrideOwner())
	{
		ObjectId playerId = pPed->GetNetworkObject()?pPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
		if(playerId != NETWORK_INVALID_OBJECT_ID && SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject())->GetWaypointOverrideOwner() == playerId)
		{
			return true;
		}
	}

	// local player not in vehicle, not relevant
	CVehicle* pLocalVehicle = CGameWorld::FindLocalPlayerVehicle();
	if(!pLocalVehicle)
	{
		gnetDebug1("IsRelevantToGPS :: No local vehicle! Not relevant!");
		return false;
	}

	// remote player not in vehicle, not relevant
	if(!pPed->GetIsInVehicle())
	{
		gnetDebug1("IsRelevantToGPS :: No remote (%s) vehicle! Not relevant!", pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown player");
		return false;
	}

	// remote player vehicle is not my vehicle, not relevant
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(pLocalVehicle != pVehicle)
	{
		gnetDebug1("IsRelevantToGPS :: Local and remote (%s) vehicles different! Not relevant!", pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown player");
		return false;
	}
	else
	{
		// if we're in a taxi and the driver is not a player
		CPed* pDriver = pVehicle->GetDriver();
		if(pDriver && !pDriver->IsPlayer() && pLocalVehicle->IsTaxi())
			return false;
	}

	//!IsFriendlyWith - not relevant -- takes the place of different teams - players may be on different teams but still friendly with each other
	if (!pLocalPed->GetPedIntelligence()->IsFriendlyWith(*pPed))
	{
		gnetDebug1("IsRelevantToGPS :: Remote ped (%s) !IsFriendlyWith -- Not relevant!", pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown player");
		return false;
	}

	// check local arrested status
	if(pLocalPed->GetIsArrested())
	{
		gnetDebug1("IsRelevantToGPS :: Local ped arrested! Not relevant!");
		return false;
	}

	// check remote arrested status
	if(pPed->GetIsArrested())
	{
		gnetDebug1("IsRelevantToGPS :: Remote ped (%s) arrested! Not relevant!", pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown player");
		return false;
	}

	// all checks passed - relevant
	return true; 
}

void NetworkInterface::IgnoreRemoteWaypoints()
{
	s_ignoreRemoteWaypoints = true;
}

bool NetworkInterface::CanSetWaypoint(CPed* pPed)
{
	// bail out if ped does not exist
	if(!pPed)
	{
		gnetDebug1("CanSetWaypoint :: Invalid ped! Cannot set waypoint!");
		return false;
	}

	// bail out if ped is not a player
	if(!pPed->IsPlayer())
	{
		gnetDebug1("CanSetWaypoint :: Non-player ped! Cannot set waypoint!");
		return false;
	}

	// is this the local player
	bool bIsLocalPed = pPed->IsLocalPlayer();

	if (s_ignoreRemoteWaypoints)
	{
		gnetDebug1("CanSetWaypoint :: Ignoring remote waypoints. %s set waypoint!", bIsLocalPed ? "Can" : "Cannot");
		return bIsLocalPed;
	}

	// in local games, we can always set a waypoint
	if(!IsGameInProgress())
	{
		gnetDebug1("CanSetWaypoint :: Not in network game. %s set waypoint!", bIsLocalPed ? "Can" : "Cannot");
		return bIsLocalPed;
	}

	// if we own the waypoint, we can override it if local
	if(bIsLocalPed && CMiniMap::IsWaypointLocallyOwned())
	{
		gnetDebug1("CanSetWaypoint :: Local ped %s %s waypoint!", pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown Player", CMiniMap::IsWaypointLocallyOwned() ? "owns" : "does not own");
		return true; 
	}

	// check ped network object
	CNetObjPlayer* pNetObjPed = SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject());
	if(pNetObjPed)
	{
		ObjectId waypointOverrideOwner = pNetObjPed->GetWaypointOverrideOwner();
		if(waypointOverrideOwner != NETWORK_INVALID_OBJECT_ID)
		{
			CNetObjPlayer* overridePlayer = SafeCast(CNetObjPlayer, netInterface::GetNetworkObject(waypointOverrideOwner));
			if(overridePlayer && overridePlayer->HasActiveWaypoint() && overridePlayer->OwnsWaypoint())
			{
				if(pPed->GetNetworkObject() && waypointOverrideOwner == pPed->GetNetworkObject()->GetObjectID())
				{
					gnetDebug1("CanSetWaypoint :: Waypoint Overriden by %d Can override!", waypointOverrideOwner);
					return true;
				}
				else
				{
					gnetDebug1("CanSetWaypoint :: Waypoint Overriden by %d Can not override!", waypointOverrideOwner);
					return false;
				}
			}
		}
	}

	// can always set a waypoint when not in a vehicle
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if(!pVehicle)
	{
		gnetDebug1("CanSetWaypoint :: Not in vehicle. %s set waypoint!", bIsLocalPed ? "Can" : "Cannot");
		return bIsLocalPed;
	}
	else
	{
		// if we're in a taxi and the driver is not a player
		CPed* pDriver = pVehicle->GetDriver();
		if(pDriver && !pDriver->IsPlayer() && pVehicle->IsTaxi())
			return true;

		// If the local player has waypoint overrider set
		if(SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject())->GetWaypointOverrideOwner())
		{
			// if the waypoint setting comes from the player who we set the overrider to be than return true as they can always set waypoint
			ObjectId playerId = pPed->GetNetworkObject()? pPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
			if(playerId != NETWORK_INVALID_OBJECT_ID && SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject())->GetWaypointOverrideOwner() == playerId)
			{
				return true;
			}
			else
			{
				// the waypoint setting comes from the local player so further checks are needed
				if(bIsLocalPed)
				{
					// if the overrider the local player has set currently has waypoint set than the local player can't set their own waypoint so return false
					ObjectId waypointOverridePlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject())->GetWaypointOverrideOwner();
					if(waypointOverridePlayer && SafeCast(CNetObjPlayer, NetworkInterface::GetNetworkObject(waypointOverridePlayer))->HasActiveWaypoint() && SafeCast(CNetObjPlayer, NetworkInterface::GetNetworkObject(waypointOverridePlayer))->OwnsWaypoint())
					{
						return false;
					}
					else
					{
						// the waypoint overrider doesn't currently have waypoint set so the local player can set their own
						return true;
					}
				}
			}
		}
	}

	// check ped network object
	if(!pNetObjPed)
	{
		gnetDebug1("CanSetWaypoint :: Invalid network object! Cannot set waypoint!");
		return false;
	}

	// manage team - only players in the same team can share waypoints
	int nTeam = pNetObjPed->GetPlayerOwner() ? pNetObjPed->GetPlayerOwner()->GetTeam() : -1;
	
	// check vs. driver
	CPed* pDriver = pVehicle->GetDriver();
	if(pDriver && pDriver->IsPlayer() && (pDriver != pPed) && pDriver->GetNetworkObject() && !pDriver->GetIsArrested())
	{
		// if the driver has set a GPS and the driver isn't this ped, bail out
		CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pDriver->GetNetworkObject());
		if(pNetObj && pNetObj->HasActiveWaypoint() && pNetObj->OwnsWaypoint())
		{
			// check that players are same team and have same corrupt value
			CNetGamePlayer* pPlayer = pNetObj->GetPlayerOwner();
			if(pPlayer && (pPlayer->GetTeam() == nTeam))
			{
				gnetDebug1("CanSetWaypoint :: Driver (%s) owns waypoint. (%s) cannot override!", pPlayer->GetLogName(), pNetObjPed->GetPlayerOwner() ? pNetObjPed->GetPlayerOwner()->GetLogName() : "Unknown Player");
				return false;
			}
		}
	}

	// find this players seat index
	s32 nHighSeatId = -1;
	s32 nPedSeatId = -1;
	for(s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
	{
		// check vs. passengers
		CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(i);
		if(pPassenger)
		{
			if(pPed == pPassenger)
			{
				nPedSeatId = i;
			}
			else if(pPassenger->IsPlayer() && pPassenger->GetNetworkObject() && !pPassenger->GetIsArrested())
			{
				CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pPassenger->GetNetworkObject());
				if(pNetObj && pNetObj->HasActiveWaypoint() && pNetObj->OwnsWaypoint() && nHighSeatId == -1)
				{
					CNetGamePlayer* pPlayer = pNetObj->GetPlayerOwner();
					if(pPlayer && (pPlayer->GetTeam() == nTeam))
						nHighSeatId = i;
				}
			}
		}
	}

	// if a passenger with greater priority than the passed ped exists, bail out
	if(nHighSeatId != -1 && nPedSeatId > nHighSeatId)
	{
		gnetDebug1("CanSetWaypoint :: Higher priority passenger (%d/%d) owns waypoint. (%s) cannot override %s!", nPedSeatId, nHighSeatId, pNetObjPed->GetPlayerOwner() ? pNetObjPed->GetPlayerOwner()->GetLogName() : "Unknown Player", pVehicle->GetSeatManager()->GetPedInSeat(nHighSeatId)->GetNetworkObject()->GetPlayerOwner() ? pVehicle->GetSeatManager()->GetPedInSeat(nHighSeatId)->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "Unknown player");
		return false;
	}

	// tests passed
	gnetDebug1("CanSetWaypoint :: %s can set waypoint!", pNetObjPed->GetPlayerOwner() ? pNetObjPed->GetPlayerOwner()->GetLogName() : "Unknown player");
	return true; 
}

eHUD_COLOURS NetworkInterface::GetWaypointOverrideColor()
{
	// this should always be good at this point
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if(!pLocalPlayer)
		return eHUD_COLOURS::HUD_COLOUR_WAYPOINT;

	CPed* pOwner = NULL;

	CNetObjPlayer* localPlayerPed = SafeCast(CNetObjPlayer, pLocalPlayer->GetPlayerPed()->GetNetworkObject());
	if(localPlayerPed)
	{
		if(localPlayerPed->GetWaypointOverrideOwner() != NETWORK_INVALID_OBJECT_ID)
		{
			CNetObjPlayer* overridePlayer = SafeCast(CNetObjPlayer, GetNetworkObject(localPlayerPed->GetWaypointOverrideOwner()));
			if(overridePlayer && overridePlayer->HasActiveWaypoint() && overridePlayer->OwnsWaypoint())
			{
				pOwner = overridePlayer->GetPed();
				return localPlayerPed->GetWaypointOverrideColor();
			}
		}
	}

	return eHUD_COLOURS::HUD_COLOUR_WAYPOINT;
}

void NetworkInterface::ResolveWaypoint()
{
	// 
	if(!IsGameInProgress())
	{
		// network game not active, assume ownership
		if(CMiniMap::IsWaypointActive())
			CMiniMap::MarkLocallyOwned(true);

		return;
	}

	// this should always be good at this point
	CNetGamePlayer* pLocalPlayer = GetLocalPlayer();
	if(!pLocalPlayer)
		return; 

	if (s_ignoreRemoteWaypoints)
		return;

	// no vehicle? nothing to resolve
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if(!pVehicle)
		return; 
	else
	{
		// if we're in a taxi and the driver is not a player
		CPed* pDriver = pVehicle->GetDriver();
		if(pDriver && !pDriver->IsPlayer() && pVehicle->IsTaxi())
			return;
	}

	bool bPedWantsToShuffle = false;
	CPedIntelligence* pPedLocalPlayerIntelligence = pLocalPlayer->GetPlayerPed() ? pLocalPlayer->GetPlayerPed()->GetPedIntelligence() : NULL;
	if (pPedLocalPlayerIntelligence)
	{
		if (pPedLocalPlayerIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskEnterVehicle *task = (CTaskEnterVehicle*)pPedLocalPlayerIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
			if(task && task->GetWantsToShuffle())
			{
				bPedWantsToShuffle = true;
				gnetDebug1("ResolveWaypoint :: Localplayer is jacking the vehicle Entering bPedWantsToShuffle=true");
			}
		}
	}

	// work out owner
	// find a suitable owner
	CPed* pOwner = NULL;

	// only players friendly with each other can share waypoints
	CPed* pLocalPlayerPed = pLocalPlayer->GetPlayerPed();

	if (!bPedWantsToShuffle && pLocalPlayerPed)
	{
		// check vs. driver
		CPed* pDriver = pVehicle->GetDriver();
		if(pDriver && pDriver->IsPlayer() && pDriver->GetNetworkObject())
		{
			CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pDriver->GetNetworkObject());
			if(pNetObj && pNetObj->HasActiveWaypoint() && pNetObj->OwnsWaypoint() && !pDriver->GetIsArrested())
			{
				if (pDriver->GetPedIntelligence()->IsFriendlyWith(*pLocalPlayerPed,true))
				{
					gnetDebug1("ResolveWaypoint :: Driver (%s) is waypoint owner!", pNetObj->GetLogName());
					pOwner = pDriver;
				}
			}
		}
	}

	// check vs. passengers
	if(!bPedWantsToShuffle && !pOwner && pLocalPlayerPed)
	{
		for(s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(i);
			if(pPassenger && pPassenger->IsPlayer() && pPassenger->GetNetworkObject())
			{
				// accept highest index
				CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pPassenger->GetNetworkObject());
				if(pNetObj && pNetObj->HasActiveWaypoint() && pNetObj->OwnsWaypoint() && !pPassenger->GetIsArrested())
				{
					if (pPassenger->GetPedIntelligence()->IsFriendlyWith(*pLocalPlayerPed,true))
					{
						gnetDebug1("ResolveWaypoint :: Passenger (%s) is waypoint owner!", pNetObj->GetLogName());
						pOwner = pPassenger;
						break;
					}
				}
			}
		}
	}

	eHUD_COLOURS overrideColor = eHUD_COLOURS::HUD_COLOUR_INVALID;
	CNetObjPlayer* localPlayerPed = SafeCast(CNetObjPlayer, pLocalPlayer->GetPlayerPed()->GetNetworkObject());
	if(localPlayerPed)
	{
		if(localPlayerPed->GetWaypointOverrideOwner() != NETWORK_INVALID_OBJECT_ID)
		{
			CNetObjPlayer* overridePlayer = SafeCast(CNetObjPlayer, GetNetworkObject(localPlayerPed->GetWaypointOverrideOwner()));
			if(overridePlayer && overridePlayer->HasActiveWaypoint())
			{
				pOwner = overridePlayer->GetPed();
			}

			overrideColor = localPlayerPed->GetWaypointOverrideColor();
		}
	}

	if(pOwner)
	{
		CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pOwner->GetNetworkObject());
		if(!CMiniMap::IsActiveWaypoint(Vector2(pNetObj->GetWaypointX(), pNetObj->GetWaypointY()), pNetObj->GetWaypointObjectId()))
		{
			uiDisplayf("NetworkInterface::ResolveWaypoint -  Clearing Waypoint");
			CMiniMap::SwitchOffWaypoint();

			if(overrideColor == eHUD_COLOURS::HUD_COLOUR_INVALID)
			{
				CMiniMap::SwitchOnWaypoint(pNetObj->GetWaypointX(), pNetObj->GetWaypointY(), pNetObj->GetWaypointObjectId());
			}
			else
			{
				CMiniMap::SwitchOnWaypoint(pNetObj->GetWaypointX(), pNetObj->GetWaypointY(), pNetObj->GetWaypointObjectId(), false, true, overrideColor);
			}
		}

		// update ownership
		CMiniMap::MarkLocallyOwned(pOwner->IsLocalPlayer());
	}
}

void NetworkInterface::RegisterHeadShotWithNetworkTracker(CPed const * ped)
{
    if(gnetVerifyf(ped, "Trying to register headshot over the network for an invalid ped!") &&
       gnetVerifyf(ped->GetNetworkObject(), "Trying to register headshot over the network for a non-networked ped!"))	
	{
		CNetObjPed *netobjPed = SafeCast(CNetObjPed, ped->GetNetworkObject());

		netobjPed->RegisterHeadShotWithNetworkTracker();
	}
}

void NetworkInterface::RegisterKillWithNetworkTracker(CPhysical const * physical)
{
    if(gnetVerifyf(physical, "Trying to register kill over the network for an invalid physical!") &&
       gnetVerifyf(physical->GetNetworkObject(), "Trying to register kill over the network for a non-networked physical!"))	
	{
		CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());

		netObjPhysical->RegisterKillWithNetworkTracker();		
	}	
}

bool NetworkInterface::HasBeenDamagedBy(netObject *networkObject, const CPhysical *physical)
{
    bool hasBeenDamagedBy = false;

    if(AssertVerify(networkObject) &&
       AssertVerify(networkObject->GetEntity()) &&
       AssertVerify(networkObject->GetEntity()->GetIsPhysical()) &&
       AssertVerify(physical))
    {
        CNetObjPhysical *netobjPhysical = SafeCast(CNetObjPhysical, networkObject);

        hasBeenDamagedBy = netobjPhysical->HasBeenDamagedBy(physical);
    }

    return hasBeenDamagedBy;
}

bool NetworkInterface::IsRemotePlayerInNonClonedVehicle(const CPed *ped)
{
    bool inNonClonedVehicle = false;

    if(ped && ped->IsNetworkPlayer() && !ped->GetIsInVehicle())
    {
        CNetObjPed *netObjPed = ped->GetNetworkObject() ? SafeCast(CNetObjPed, ped->GetNetworkObject()) : 0;

        if(netObjPed && netObjPed->GetPedsTargetVehicleId() != NETWORK_INVALID_OBJECT_ID)
        {
            inNonClonedVehicle = GetNetworkObject(netObjPed->GetPedsTargetVehicleId()) == 0;
        }
    }

    return inNonClonedVehicle;
}

void NetworkInterface::GetPlayerPositionForBlip(const CPed &playerPed, Vector3 &blipPosition)
{
    if(gnetVerifyf(playerPed.IsAPlayerPed(), "Trying to return player position for a blip for a non-player ped!"))
    {
        if(playerPed.GetIsInVehicle())
        {
            blipPosition = VEC3V_TO_VECTOR3(playerPed.GetMyVehicle()->GetTransform().GetPosition());
        }
        else
        {
            if(playerPed.IsNetworkClone() && IsRemotePlayerInNonClonedVehicle(&playerPed))
            {
                // for players in non-cloned vehicles we return the player position of the player in the car with the lowest
                // physical player index. This ensures we return the same position for all players in the same car
                const CNetObjPed *netObjPed = SafeCast(const CNetObjPed, playerPed.GetNetworkObject());

                if(netObjPed)
                {
                    ObjectId vehicleID = netObjPed->GetPedsTargetVehicleId();

                    for(PhysicalPlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
                    {
                        const CNetGamePlayer *netGamePlayer = static_cast<const CNetGamePlayer*>(NetworkInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex(playerIndex));

                        if(netGamePlayer && netGamePlayer->IsRemote())
                        {
                            const CPed           *remotePlayerPed     = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;
                            const CNetObjPed     *playerNetworkObject = remotePlayerPed ? SafeCast(const CNetObjPed, remotePlayerPed->GetNetworkObject()) : 0;

                            if(playerNetworkObject && playerNetworkObject->GetPedsTargetVehicleId() == vehicleID)
                            {
                                blipPosition   = VEC3V_TO_VECTOR3(remotePlayerPed->GetTransform().GetPosition());
                                blipPosition.z = GetLastPosReceivedOverNetwork(remotePlayerPed).z;

                                return;
                            }
                        }
                    }
                }
            }

            blipPosition = VEC3V_TO_VECTOR3(playerPed.GetTransform().GetPosition());
        }
    }
}

u32 NetworkInterface::GetNumVehiclesOutsidePopulationRange()
{
    return CNetObjVehicle::GetNumVehiclesOutsidePopulationRange();
}

bool NetworkInterface::GetLowestPedModelStartOffset(u32 &lowestStartOffset, const char **playerName)
{
    bool remotePlayerInPopZone = false;

    unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

        if(player->GetPlayerPed() && player->GetPlayerPed()->GetNetworkObject())
        {
            CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, player->GetPlayerPed()->GetNetworkObject());

            u32 allowedModelStartOffset = netObjPlayer->GetAllowedPedModelStartOffset();

            if(!remotePlayerInPopZone)
            {
                lowestStartOffset = allowedModelStartOffset;

                if(playerName)
                {
                    *playerName = netObjPlayer->GetLogName();
                }

                remotePlayerInPopZone = true;
            }
            else
            {
                if(gPopStreaming.IsNetworkModelOffsetLower(allowedModelStartOffset, lowestStartOffset))
                {
                    lowestStartOffset = allowedModelStartOffset;

                    if(playerName)
                    {
                        *playerName = netObjPlayer->GetLogName();
                    }
                }
            }
        }
    }

    return remotePlayerInPopZone;
}

bool NetworkInterface::GetLowestVehicleModelStartOffset(u32 &lowestStartOffset, const char **playerName)
{
    bool remotePlayerInPopZone = false;

    unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

        if(player->GetPlayerPed() && player->GetPlayerPed()->GetNetworkObject())
        {
            CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, player->GetPlayerPed()->GetNetworkObject());

            u32 allowedModelStartOffset = netObjPlayer->GetAllowedVehicleModelStartOffset();

            if(!remotePlayerInPopZone)
            {
                lowestStartOffset = allowedModelStartOffset;
                
                if(playerName)
                {
                    *playerName = netObjPlayer->GetLogName();
                }

                remotePlayerInPopZone = true;
            }
            else
            {
                if(gPopStreaming.IsNetworkModelOffsetLower(allowedModelStartOffset, lowestStartOffset))
                {
                    lowestStartOffset = allowedModelStartOffset;

                    if(playerName)
                    {
                        *playerName = netObjPlayer->GetLogName();
                    }
                }
            }
        }
    }

    return remotePlayerInPopZone;
}

void NetworkInterface::OnQueriableStateBuilt(CPed &ped) 
{
    CNetObjPed *netObjPed = static_cast<CNetObjPed *>(ped.GetNetworkObject());

    if(netObjPed && !netObjPed->IsClone())
    {
        netObjPed->UpdateLocalTaskSlotCache();
    }
}

void NetworkInterface::RegisterCollision(CPhysical &physical, CEntity *hitEntity, float impulseMag)
{
    CNetObjPhysical *netObjPhysical = static_cast<CNetObjPhysical *>(physical.GetNetworkObject());

    if(netObjPhysical)
    {
        if(hitEntity->GetIsTypeBuilding() || (hitEntity->GetIsTypeObject() && ((CObject*)hitEntity)->IsADoor()))
        {
            netObjPhysical->RegisterCollisionWithBuilding(impulseMag);
        }
        else if(hitEntity->GetIsPhysical())
        {
            CNetObjPhysical *collisionObject = static_cast<CNetObjPhysical *>(NetworkUtils::GetNetworkObjectFromEntity(hitEntity));

            if(collisionObject)
            {
                netObjPhysical->RegisterCollision(collisionObject, impulseMag);
            }
        }
    }
}

bool NetworkInterface::IsRemotePlayerStandingOnObject(const CPed &ped, const CPhysical &physical)
{
    if(ped.IsNetworkClone() && ped.IsPlayer() && ped.GetNetworkObject() && physical.GetNetworkObject())
    {
        ObjectId objectStoodOnID = NETWORK_INVALID_OBJECT_ID;
        Vector3  objectStoodOnOffset(VEC3_ZERO);

        CNetBlenderPed *netBlender = static_cast<CNetBlenderPed *>(ped.GetNetworkObject()->GetNetBlender());

        if(netBlender && netBlender->IsStandingOnObject(objectStoodOnID, objectStoodOnOffset))
        {
            return physical.GetNetworkObject()->GetObjectID() == objectStoodOnID;
        }
    }

    return false;
}

void NetworkInterface::UpdateBeforeScripts()
{
	s_ignoreRemoteWaypoints = false;
	CLiveManager::UpdateBeforeScripts();
}

void NetworkInterface::UpdateAfterScripts()
{
	if (s_bSetMPCutsceneDebug)
	{
		SetInMPCutscene(!CNetwork::IsInMPCutscene(), true);
		s_bSetMPCutsceneDebug = false;
	}

	GetObjectManager().UpdateAfterScripts();

	CloudManager::GetInstance().ClearCompletedRequests();
	UserContentManager::GetInstance().ClearCompletedRequests();

    if(CNetwork::IsNetworkSessionValid())
    {
        CNetwork::GetNetworkSession().SendDelayedPresenceInvites();
	}

	CLiveManager::UpdateAfterScripts();
}

void NetworkInterface::AddBoxStreamerSearches(atArray<fwBoxStreamerSearch>& searchList)
{
    CNetObjPhysical::AddBoxStreamerSearches(searchList);
}

void NetworkInterface::DelayMigrationForTime(const CPhysical *physical, unsigned timeToDelayMigration)
{
    if(physical && physical->GetNetworkObject() && !physical->IsNetworkClone())
    {
        CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical->GetNetworkObject());
        netObjPhysical->SetScriptMigrationTimer(fwTimer::GetSystemTimeInMilliseconds() + timeToDelayMigration);
    }
}

void NetworkInterface::GetRemotePlayerCameraMatrix(CPed *playerPed, Matrix34 &cameraMatrix)
{
    if(gnetVerifyf(playerPed, "Trying to get a camera matrix for an invalid player ped!") &&
       gnetVerifyf(playerPed->IsPlayer(), "Trying to get a camera matrix for a non player ped!") &&
       gnetVerifyf(playerPed->GetNetworkObject(), "Trying to get a camera matrix for a non-networked player!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, playerPed->GetNetworkObject());
        grcViewport   *viewport     = netObjPlayer->GetViewport();

        if(gnetVerifyf(viewport, "Remote player doesn't have a viewport set up!"))
        {
            Matrix34 viewportMatrix = RCC_MATRIX34(viewport->GetCameraMtx());
            
            // viewport returns matrix in rage coordinate system, so need to convert to game here
            cameraMatrix.a = viewportMatrix.a;
	        cameraMatrix.b = -viewportMatrix.c;
	        cameraMatrix.c = viewportMatrix.b;
	        cameraMatrix.d = viewportMatrix.d;
        }
    }
}

float NetworkInterface::GetRemotePlayerCameraFov(CPed *playerPed)
{
    if(gnetVerifyf(playerPed, "Trying to get a camera Fov for an invalid player ped!") &&
       gnetVerifyf(playerPed->IsPlayer(), "Trying to get a camera Fov for a non player ped!") &&
       gnetVerifyf(playerPed->GetNetworkObject(), "Trying to get a camera Fov for a non-networked player!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, playerPed->GetNetworkObject());
        grcViewport   *viewport     = netObjPlayer->GetViewport();

        if(gnetVerifyf(viewport, "Remote player doesn't have a viewport set up!"))
        {
            // we have to divide by 2 here as the network player viewports FOV is doubled, for a reason
            // lost in the mists of time. Maybe as remote player cameras are generally used to prevent
            // objects being spawned in view of other players this is an attempt to make this less likely
            return viewport->GetFOVY() / 2.0f;
        }
    }

    return g_DefaultFov;
}

bool  NetworkInterface::IsRemotePlayerUsingFreeCam(const CPed *playerPed)
{
	if (playerPed && playerPed->IsAPlayerPed() && playerPed->GetNetworkObject())
	{
		return static_cast<CNetObjPlayer*>(playerPed->GetNetworkObject())->IsUsingFreeCam();
	}

	return false;
}

void  NetworkInterface::LocalPlayerVoteToKickPlayer(netPlayer& player)
{
	if (gnetVerify(!player.IsMyPlayer()) && gnetVerify(player.IsRemote()) && gnetVerify(!player.IsBot()))
	{
		player.AddKickVote(*NetworkInterface::GetLocalPlayer());

		PlayerFlags playerFlags = 0;
		for (PhysicalPlayerIndex p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
		{
			netPlayer* pSendToPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(p);
			if (pSendToPlayer && pSendToPlayer->IsRemote() && !pSendToPlayer->IsBot())
			{
				playerFlags |= (1<<pSendToPlayer->GetPhysicalPlayerIndex());
			}
		}

		CSendKickVotesEvent::Trigger(netPlayer::GetLocalPlayerKickVotes(), playerFlags);
	}
}

void  NetworkInterface::LocalPlayerUnvoteToKickPlayer(netPlayer& player)
{
	if (gnetVerify(!player.IsMyPlayer()) && gnetVerify(player.IsRemote()) && gnetVerify(!player.IsBot()))
	{
		player.RemoveKickVote(*NetworkInterface::GetLocalPlayer());

		PlayerFlags playerFlags = 0;
		for (PhysicalPlayerIndex p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
		{
			netPlayer* pSendToPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(p);
			if (pSendToPlayer && pSendToPlayer->IsRemote() && !pSendToPlayer->IsBot())
			{
				playerFlags |= (1<<pSendToPlayer->GetPhysicalPlayerIndex());
			}
		}

		CSendKickVotesEvent::Trigger(netPlayer::GetLocalPlayerKickVotes(), playerFlags);
	}
}

bool NetworkInterface::IsLocalPlayersTurn(Vec3V_In vPosition, float fMaxDistance, float fDurationOfTurns, float fTimeBetweenTurns)
{
	return NetworkUtils::IsLocalPlayersTurn(vPosition, fMaxDistance, fDurationOfTurns, fTimeBetweenTurns);
}

CPed* NetworkInterface::FindClosestWantedPlayer(const Vector3& vPosition, const int minWantedLevel)
{
	CPed* pClosestWantedPlayer = NULL;

	float fClosestDist2 = 0.f;

	unsigned numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer* const *physicalPlayers = netInterface::GetAllPhysicalPlayers();
	
	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *netGamePlayer = SafeCast(const CNetGamePlayer, physicalPlayers[index]);
		CPed* pTestPlayerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : NULL;

		if(pTestPlayerPed && pTestPlayerPed->GetPlayerWanted() && (pTestPlayerPed->GetPlayerWanted()->GetWantedLevel() >= minWantedLevel))
		{
			Vector3 diff = VEC3V_TO_VECTOR3(pTestPlayerPed->GetTransform().GetPosition()) - vPosition;

			float dist2 = diff.Mag2();

			if (fClosestDist2 <= 0.f || dist2 <= fClosestDist2)
			{
				pClosestWantedPlayer = pTestPlayerPed;
				fClosestDist2 = dist2;
			}
		}
	}

	return pClosestWantedPlayer;
}

void NetworkInterface::OnAddedToGameWorld(netObject *networkObject)
{
    if(networkObject)
    {
        // we need to update the scope state for all players immediately when a network object
        // is added to the game world
        // update the object immediately so there is not a delay before the object is cloned on other machines
        if(!networkObject->IsClone())
        {
            CNetwork::GetObjectManager().UpdateAllInScopeStateImmediately(networkObject);
        }
        else if(networkObject->GetObjectType() == NET_OBJ_TYPE_PICKUP)
        {
            // pickups can be created without their physics being added - in which case
            // any velocity update received over the network will not have been applied correctly.
            // reapply the last received velocity here
            CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, networkObject);
            netObjPhysical->ReapplyLastVelocityUpdate();
        }
    }
}

void NetworkInterface::OnClonePedDetached(netObject *networkObject)
{
    if(gnetVerifyf(networkObject && networkObject->IsClone(), "Invalid network object!"))
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLONE_PED_DETACHED", networkObject->GetLogName());

        CNetBlenderPed *netBlender = SafeCast(CNetBlenderPed, networkObject->GetNetBlender());
        netBlender->OnPedDetached();
    }
}

bool NetworkInterface::AreCarGeneratorsDisabled()
{
    return CNetwork::AreCarGeneratorsDisabled();
}

bool NetworkInterface::IsPlayerUsingLeftTriggerAimMode(const CPed *pPed)
{
    if(gnetVerifyf(pPed, "Checking left trigger aim mode for an invalid ped!") &&
       gnetVerifyf(pPed->IsPlayer(), "Checking left trigger aim mode for a ped is not a player!") &&
       gnetVerifyf(pPed->IsNetworkClone(), "Checking left trigger aim mode for the local player!"))
    {
        CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject());

        return netObjPlayer->IsRemoteUsingLeftTriggerAimMode();
    }

    return false;
}

bool NetworkInterface::IsDamageDisabledInMP(const CEntity& entity1, const CEntity& entity2)
{
	if (NetworkInterface::IsGameInProgress() && entity1.GetIsPhysical() && entity2.GetIsPhysical())
	{
		CPhysical& physical1 = (CPhysical&)entity1;
		CPhysical& physical2 = (CPhysical&)entity2;

		CNetObjPhysical* netObj1 = static_cast<CNetObjPhysical*>(physical1.GetNetworkObject());
		CNetObjPhysical* netObj2 = static_cast<CNetObjPhysical*>(physical2.GetNetworkObject());

		if (netObj1 && netObj2)
		{
			if (CNetworkGhostCollisions::IsAGhostCollision(physical1, physical2))
			{
				return true;
			}
			if ((!netObj1->GetIsAllowDamagingWhileConcealed() && netObj1->IsConcealed()) || (!netObj2->GetIsAllowDamagingWhileConcealed() && netObj2->IsConcealed()))
			{
				return true;
			}
		}
	}

	return false;
}

bool NetworkInterface::AreInteractionsDisabledInMP(const CEntity& entity1, const CEntity& entity2)
{
	if (NetworkInterface::IsGameInProgress() && entity1.GetIsPhysical() && entity2.GetIsPhysical())
	{
		CPhysical& physical1 = (CPhysical&)entity1;
		CPhysical& physical2 = (CPhysical&)entity2;

		CNetObjPhysical* netObj1 = static_cast<CNetObjPhysical*>(physical1.GetNetworkObject());
		CNetObjPhysical* netObj2 = static_cast<CNetObjPhysical*>(physical2.GetNetworkObject());

		if (netObj1 && netObj2)
		{
			if (CNetworkGhostCollisions::IsAGhostCollision(physical1, physical2))
			{
				return true;
			}
			if (netObj1->IsConcealed() || netObj2->IsConcealed())
			{
				return true;
			}
		}
	}

	return false;
}

bool NetworkInterface::AreBulletsImpactsDisabledInMP(const CEntity& firingEntity, const CEntity& hitEntity)
{
	if (NetworkInterface::IsGameInProgress())
	{
		if (IsDamageDisabledInMP(firingEntity, hitEntity))
		{
			return true;
		}

		if (firingEntity.GetIsTypePed() && hitEntity.GetIsPhysical())
		{
			netObject* netObj = NetworkUtils::GetNetworkObjectFromEntity(&hitEntity);

			if (netObj)
			{
				const CPed& firingPed = (const CPed&)firingEntity;

				// other peds can fire through ghost entities if their target is colliding with the entity (this allows cops to shoot at players standing inside ghost vehicles)
				const CPed* pCurrentTarget = NULL;
			
				if (firingPed.IsAPlayerPed())
				{
					const CPlayerPedTargeting & rPlayerTargeting = firingPed.GetPlayerInfo()->GetTargeting();

					const CEntity* pTarget = rPlayerTargeting.GetLockOnTarget();

					if (!pTarget)
					{
						pTarget = rPlayerTargeting.GetFreeAimTarget();
					}

					pCurrentTarget = (pTarget && pTarget->GetIsTypePed()) ? (const CPed*)pTarget : NULL;
				}
				else
				{
					pCurrentTarget = firingPed.GetPedIntelligence()->GetCurrentTarget();
				}

				if (pCurrentTarget && pCurrentTarget->IsAPlayerPed() && CNetworkGhostCollisions::IsInGhostCollision((const CPhysical&)hitEntity, *pCurrentTarget))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void NetworkInterface::RegisterDisabledCollisionInMP(const CEntity& entity1, const CEntity& entity2)
{
	if (entity1.GetIsPhysical() && entity2.GetIsPhysical())
	{
		CNetworkGhostCollisions::RegisterGhostCollision((const CPhysical&)entity1, (const CPhysical&)entity2);
	}
}

bool NetworkInterface::IsAGhostVehicle(const CVehicle& vehicle, bool bIsFullyGhosted)
{
	bool bIsGhost = false;

	netObject* netObj = NetworkUtils::GetNetworkObjectFromEntity(&vehicle);

	if (netObj)
	{
		if (static_cast<CNetObjVehicle*>(netObj)->IsGhost())
		{
			CPed* pDriver = vehicle.GetDriver();

			if (!pDriver || !pDriver->IsAPlayerPed() || !pDriver->GetNetworkObject())
			{
				bIsGhost = true;
			}
			else 
			{
				CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, pDriver->GetNetworkObject());

				if (pDriver->IsLocalPlayer())
				{
					if (bIsFullyGhosted)
					{
						bIsGhost = pPlayerObj->GetGhostPlayerFlags() == 0;
					}
					else
					{
						bIsGhost = pPlayerObj->GetGhostPlayerFlags() != 0;
					}
				}
				else
				{
					CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
					if (pLocalPlayer && NetworkInterface::AreInteractionsDisabledInMP(*pDriver, *pLocalPlayer))
					{
						bIsGhost = true;
					}
				}
			}
		}
	}

	return bIsGhost;
}

bool NetworkInterface::CanSwitchToSwimmingMotion(CPed *pPed)
{
    // don't allow clones standing on objects to switch to swimming if their owner has not. This
    // prevents clones trying to swim when on the back on boats crashing through large waves
	//
	// also fixes problems where remote is trying to swim and the local isn't swimming.
	if(pPed->IsNetworkClone())
	{
		CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, pPed->GetNetworkObject()->GetNetBlender());

		ObjectId objectStoodOnID = NETWORK_INVALID_OBJECT_ID;
		Vector3  objectStoodOnOffset;

		if(netBlenderPed && netBlenderPed->IsStandingOnObject(objectStoodOnID, objectStoodOnOffset))
		{
			CNetObjPed *netObjPed = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			if(!netObjPed->GetIsOwnerSwimming())
			{
				return false;
			}
		}

		//If the water isn't deep enough to be in the water on remote - then we can't be swimming
		float fWaterDepth;
		pPed->m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &fWaterDepth, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed);
		fWaterDepth -= pPed->GetGroundPos().z;
		static dev_float s_fMinWaterDepthDelta = 0.9f;
		if (fWaterDepth<s_fMinWaterDepthDelta)
		{
			return false;
		}
	}

    return true;
}

bool NetworkInterface::CanMakeVehicleIntoDummy(CVehicle* pVehicle)
{
	// once a vehicle has its collision overridden during a cutscene it cannot be allowed to dummify as this can potentially end up with it falling through the map
	if (IsInMPCutscene() && pVehicle && pVehicle->GetNetworkObject() && static_cast<CNetObjEntity*>(pVehicle->GetNetworkObject())->GetOverridingLocalCollision())
	{
		return false;
	}

	return true;
}

bool NetworkInterface::CanScriptModifyRemoteAttachment(const CPhysical &physical)
{
    CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical.GetNetworkObject());

    if(netObjPhysical && netObjPhysical->IsClone())
    {
        return netObjPhysical->CanScriptModifyRemoteAttachment();
    }

    return false;
}

void NetworkInterface::SetScriptModifyRemoteAttachment(const CPhysical &physical, bool canModify)
{
    CNetObjPhysical *netObjPhysical = SafeCast(CNetObjPhysical, physical.GetNetworkObject());

    if(netObjPhysical && netObjPhysical->IsClone())
    {
        netObjPhysical->SetScriptModifyRemoteAttachment(canModify);
    }
}

void NetworkInterface::OnAIEventAdded(const fwEvent& event)
{
	if (IsGameInProgress())
	{
		CNetObjPed::OnAIEventAdded(event);
	}
}

bool NetworkInterface::IsEntityPendingCloning(const CEntity* pEntity)
{
	netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

	if (pNetObj)
	{
		return pNetObj->IsPendingCloning();
	}

	return false;
}

bool NetworkInterface::AreEntitiesClonedOnTheSameMachines(const CEntity* pEntity1, const CEntity* pEntity2)
{
	netObject* pNetObj1 = NetworkUtils::GetNetworkObjectFromEntity(pEntity1);
	netObject* pNetObj2 = NetworkUtils::GetNetworkObjectFromEntity(pEntity2);

	if (pNetObj1 && pNetObj2)
	{
		return (pNetObj1->GetClonedState() == pNetObj2->GetClonedState() && pNetObj1->GetPendingCloningState() == pNetObj2->GetPendingCloningState());
	}

	return (!pNetObj1 && !pNetObj2);
}

void NetworkInterface::LocallyHideEntityForAFrame(const CEntity& entity, bool bIncludeCollision, const char* reason)
{
	CNetObjEntity* pNetObj = static_cast<CNetObjEntity*>(NetworkUtils::GetNetworkObjectFromEntity(&entity));

	if (pNetObj)
	{
		pNetObj->SetOverridingLocalVisibility(false, reason);

		if (bIncludeCollision)
		{
			pNetObj->SetOverridingLocalCollision(false, reason);
		}
	}
}

void NetworkInterface::CutsceneStartedOnEntity(CEntity& entity)
{
	if (IsGameInProgress() && entity.GetIsPhysical() && (entity.GetIsTypePed() || entity.GetIsTypeVehicle() || entity.GetIsTypeObject()))
	{
		// don't bother with doors and pickups
		if (entity.GetIsTypeObject())
		{
			CObject* pObject = SafeCast(CObject, &entity);

			if (pObject->m_nObjectFlags.bIsPickUp || pObject->m_nObjectFlags.bIsDoor)
			{
				return;
			}
		}

		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(&entity));

		if (pNetObj)
		{
			NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "CUTSCENE_STARTED", pNetObj->GetLogName());
			pNetObj->SetIsInCutsceneLocally(true);
				
			if (!pNetObj->IsClone())
			{
				pNetObj->ForceSendOfCutsceneState();
			}
		}
	}
}

void NetworkInterface::CutsceneFinishedOnEntity(CEntity& entity)
{
	if (IsGameInProgress() && entity.GetIsPhysical())
	{
		// don't bother with doors and pickups
		if (entity.GetIsTypeObject())
		{
			CObject* pObject = SafeCast(CObject, &entity);

			if (pObject->m_nObjectFlags.bIsPickUp || pObject->m_nObjectFlags.bIsDoor)
			{
				return;
			}
		}

		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, NetworkUtils::GetNetworkObjectFromEntity(&entity));

		if (pNetObj)
		{
			NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "CUTSCENE_FINISHED", pNetObj->GetLogName());
			pNetObj->SetIsInCutsceneLocally(false);

			if (!pNetObj->IsClone())
			{
				pNetObj->ForceSendOfCutsceneState();
			}
		}
	}
}

void NetworkInterface::SetLocalDeadPlayerWeaponPickupAndObject(CPickup* pWeaponPickup, CObject* pWeaponObject)
{
	CPed* pLocalPed = CGameWorld::FindLocalPlayer();

	if (pLocalPed)
	{
		CNetObjPlayer* pNetObj = SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject());

		if (pNetObj)
		{
			pNetObj->SetLocalPlayerWeaponPickupAndObject(pWeaponPickup, pWeaponObject);
		}
	}
}

void NetworkInterface::NetworkedPedPlacedOutOfVehicle(CPed& ped, CVehicle* pVehicle)
{
	CNetObjPed* pPedObj = static_cast<CNetObjPed*>(ped.GetNetworkObject());

	if (gnetVerify(pPedObj))
	{
		// reset alpha in case the ped was in a vehicle that was fading / alpha ramping
		pPedObj->ResetAlphaActive();

		if (pPedObj->GetHideWhenInvisible())
		{
			pPedObj->SetHideWhenInvisible(false);
		}

		if (pVehicle)
		{
			// if the vehicle is in ghost collision with anything else, we need to make the ped in ghost collision with the same entities. This is to prevent the player getting out inside another vehicle
			// and getting stuck inside it, or punted under the map.
			const CPhysical* ghostCollisions[CNetworkGhostCollisions::MAX_GHOST_COLLISIONS];
			u32 numCollisions = 0;

			CNetworkGhostCollisions::GetAllGhostCollisions(*pVehicle, ghostCollisions, numCollisions);

			for (u32 collision=0; collision<numCollisions; collision++)
			{
				const CPhysical* pPhysical = ghostCollisions[collision];

				if (pPhysical != pVehicle)
				{
					CNetworkGhostCollisions::RegisterGhostCollision(ped, *pPhysical);
				}
			}
		}
	}
}

bool NetworkInterface::HasCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex)
{
	return CNetObjPlayer::HasCachedPlayerHeadBlendData(playerIndex);
}

void NetworkInterface::CachePlayerHeadBlendData(PhysicalPlayerIndex playerIndex, CPedHeadBlendData& headBlendData)
{
	CNetObjPlayer::CachePlayerHeadBlendData(playerIndex, headBlendData);

	if (playerIndex == GetLocalPhysicalPlayerIndex())
	{
		CCachePlayerHeadBlendDataEvent::Trigger(headBlendData);
	}
}

bool NetworkInterface::ApplyCachedPlayerHeadBlendData(CPed& ped, PhysicalPlayerIndex playerIndex)
{
	return CNetObjPlayer::ApplyCachedPlayerHeadBlendData(ped, playerIndex);
}

void NetworkInterface::ClearCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex)
{
	CNetObjPlayer::ClearCachedPlayerHeadBlendData(playerIndex);
}

void NetworkInterface::BroadcastCachedLocalPlayerHeadBlendData(const netPlayer *pPlayerToBroadcastTo)
{
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "BROADCAST_LOCAL_PLAYER_HEAD_BLEND_DATA", "");

	if (NetworkInterface::IsGameInProgress())
	{
		const CPedHeadBlendData* pHeadBlendData = CNetObjPlayer::GetCachedPlayerHeadBlendData(GetLocalPhysicalPlayerIndex());

		if (pHeadBlendData)
		{
			if (pPlayerToBroadcastTo)
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Send to", "%s", pPlayerToBroadcastTo->GetLogName());
			}
			else
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Send to", "All players");
			}

			CCachePlayerHeadBlendDataEvent::Trigger(*pHeadBlendData, pPlayerToBroadcastTo);
		}
		else
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Fail", "No local cached head blend data");
		}
	}
	else
	{
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Fail", "Network game not in progress");
	}
}

bool NetworkInterface::IsPlayerUsingScopedWeapon(const netPlayer& player, bool* lookingThroughScope, bool* scopeZoomed, float* weaponRange)
{
	static float ZOOMED_THRESHOLD = 20.0f;

	const CNetGamePlayer &netgameplayer = static_cast<const CNetGamePlayer &>(player);

	if (CPed* pPlayerPed = netgameplayer.GetPlayerPed())
	{
		if (const CPedWeaponManager* pWeaponManager = pPlayerPed->GetWeaponManager())
		{
			if (const CObject* pObject = pWeaponManager->GetEquippedWeaponObject())
			{
				if (const CWeapon* pWeapon = pObject->GetWeapon())
				{
					if (const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo())
					{
						bool bIsScopedWeapon = pWeapon->GetScopeComponent() || pWeapon->GetHasFirstPersonScope();

						if (bIsScopedWeapon)
						{
							if (weaponRange)
							{
								*weaponRange = pWeaponInfo->GetRange();
							}
				
							if (lookingThroughScope && pPlayerPed->GetNetworkObject())
							{
								if (player.IsRemote())
								{
									*lookingThroughScope = static_cast<CNetObjPlayer*>(pPlayerPed->GetNetworkObject())->IsRemoteUsingLeftTriggerAimMode();
								}
								else
								{
									const CTaskMotionAiming* pAimingTask = static_cast<const CTaskMotionAiming *>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
									*lookingThroughScope = (pAimingTask && pAimingTask->UsingLeftTriggerAimMode());
								}
							}

							if (scopeZoomed)
							{
								grcViewport *viewport = player.IsRemote() ? NetworkUtils::GetNetworkPlayerViewPort(netgameplayer) : &gVpMan.GetGameViewport()->GetNonConstGrcViewport();
								*scopeZoomed = viewport->GetFOVY() < ZOOMED_THRESHOLD;
							}

							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

// Connection Metrics
bool  NetworkInterface::IsConnectedViaRelay(PhysicalPlayerIndex playerIndex)
{
    CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(playerIndex);

    if(player)
    {
		return CNetwork::GetConnectionManager().GetAddress(player->GetEndpointId()).IsRelayServerAddr();
    }

    return false;
}

float NetworkInterface::GetAverageSyncLatency(PhysicalPlayerIndex playerIndex)
{
    return GetObjectManager().GetAverageLatency(playerIndex);
}

float NetworkInterface::GetAverageSyncRTT(PhysicalPlayerIndex playerIndex)
{
    return GetObjectManager().GetAveragePing(playerIndex);
}

float NetworkInterface::GetAveragePacketLoss(PhysicalPlayerIndex playerIndex)
{
    return GetObjectManager().GetPacketLoss(playerIndex);
}

unsigned NetworkInterface::GetNumUnAckedReliables(PhysicalPlayerIndex playerIndex)
{
    const netPlayer *player = netInterface::GetPhysicalPlayerFromIndex(playerIndex);

    if(player)
    {
        int connectionID = player->GetConnectionId();

        if(CNetwork::GetConnectionManager().IsOpen(connectionID))
        {
            return CNetwork::GetConnectionManager().GetUnAckedCount(connectionID);
        }
    }

    return 0;
}

unsigned NetworkInterface::GetUnreliableResendCount(PhysicalPlayerIndex playerIndex)
{
    const netPlayer *player = netInterface::GetPhysicalPlayerFromIndex(playerIndex);

    if(player)
    {
        return CNetwork::GetBandwidthManager().GetUnreliableResendCount(*player);
    }

    return 0;
}

unsigned NetworkInterface::GetHighestReliableResendCount(PhysicalPlayerIndex playerIndex)
{
    const netPlayer *player = netInterface::GetPhysicalPlayerFromIndex(playerIndex);

    if(player)
    {
        int connectionID = player->GetConnectionId();

        if(CNetwork::GetConnectionManager().IsOpen(connectionID))
        {
            return CNetwork::GetConnectionManager().GetOldestResendCount(connectionID);
        }
    }

    return 0;
}

#if FPS_MODE_SUPPORTED
bool NetworkInterface::IsRemotePlayerInFirstPersonMode(const CPed& playerPed, bool* bInFirstPersonIdle, bool* bStickWithinStrafeAngle, bool* bOnSlope)
{
	if (playerPed.IsAPlayerPed() && playerPed.IsNetworkClone())
	{
		CNetObjPlayer* pNetObj = static_cast<CNetObjPlayer*>(playerPed.GetNetworkObject());

		if (pNetObj->IsUsingFirstPersonCamera())
		{
			if (bInFirstPersonIdle)
			{
				*bInFirstPersonIdle = pNetObj->IsInFirstPersonIdle();
			}

			if (bStickWithinStrafeAngle)
			{
				*bStickWithinStrafeAngle = pNetObj->IsStickWithinStrafeAngle();
			}

			if (bOnSlope)
			{
				*bOnSlope = pNetObj->IsOnSlope();
			}

			return true;
		}
	}

	return false;
}

bool NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(const CPed& playerPed)
{
	bool bUseFPIdleMovement = false;
	bool bInFirstPersonIdle = false;
	bool bStickWithinStrafePosition = false;

	if (PARAM_useFPIdleMovementOnClones.Get() || CNetObjPlayer::ms_bUseFPIdleMovementOnClones)
	{
		if (NetworkInterface::IsRemotePlayerInFirstPersonMode(playerPed, &bInFirstPersonIdle, &bStickWithinStrafePosition))
		{
			if (bInFirstPersonIdle)	
			{
				float moveBlendX = 0.0f;
				float moveBlendY = 0.0f;

				playerPed.GetMotionData()->GetDesiredMoveBlendRatio(moveBlendX, moveBlendY);

				if (!bStickWithinStrafePosition || (Abs(moveBlendX) < 0.001f && Abs(moveBlendY) < 0.001f))
				{
					bUseFPIdleMovement = true;
				}
			}
		}
	}

	return bUseFPIdleMovement;
}
#endif // FPS_MODE_SUPPORTED

#if RSG_ORBIS
void NetworkInterface::EnableLiveStreaming(bool bEnable)
{
	CNetwork::EnableLiveStreaming(bEnable);
}

bool NetworkInterface::IsLiveStreamingEnabled()
{
	return CNetwork::IsLiveStreamingEnabled();
}

void NetworkInterface::EnableSharePlay(bool bEnable)
{
#if (SCE_ORBIS_SDK_VERSION > 0x01700601u)
	gnetAssertf(false, "NetworkInterface::EnableSharePlay - SDK greater than 1.7XX - Need to add sceSharePlaySetProhibition. Ask Andy Keeble, if he's around");
#else
	// if SDK lower than 2.000, LiveStreaming also enables and disables Share Play
	CNetwork::EnableLiveStreaming(bEnable);
#endif
}

bool NetworkInterface::IsSharePlayEnabled()
{
#if (SCE_ORBIS_SDK_VERSION > 0x01700601u)
	gnetAssertf(false, "NetworkInterface::IsSharePlayEnabled - SDK greater than 1.7XX - Need to check via SharePlay API. Ask Andy Keeble, if he's around");
#else
	// if SDK lower than 2.000, LiveStreaming also enables and disables Share Play, so check that
	return CNetwork::IsLiveStreamingEnabled();
#endif
}

#endif // RSG_ORBIS

#if __DEV
CPed *NetworkInterface::GetCameraTargetPed()
{
    return NetworkDebug::GetCameraTargetPed();
}
#endif // __DEV

void NetworkInterface::RenderNetworkInfo()
{
    CNetwork::RenderNetworkInfo();
}

#if __BANK
void NetworkInterface::RegisterNodeWithBandwidthRecorder(CProjectBaseSyncDataNode &node, const char *recorderName)
{
    RecorderId recorderID = GetObjectManager().GetBandwidthStatistics().GetBandwidthRecorder(recorderName);

    if(recorderID == INVALID_RECORDER_ID)
    {
        recorderID = GetObjectManager().GetBandwidthStatistics().AllocateBandwidthRecorder(recorderName);
    }

    node.SetBandwidthRecorderID(recorderID);
}

void NetworkInterface::PrintNetworkInfo()
{
    NetworkDebug::PrintNetworkInfo();
}

bool NetworkInterface::ShouldForceCloneTasksOutOfScope(const CPed *ped)
{
    return NetworkDebug::ShouldForceCloneTasksOutOfScope(ped);
}

const char *NetworkInterface::GetLastRegistrationFailureReason()
{
    return CNetwork::GetObjectManager().GetLastRegistrationFailureReason();
}

#if __DEV
void NetworkInterface::DisplayNetworkingVectorMap()
{
    CNetwork::GetRoamingBubbleMgr().DisplayBubblesOnVectorMap();
    CNetworkWorldGridManager::DisplayOnVectorMap();
    NetworkDebug::DisplayPredictionFocusOnVectorMap();
}
#endif // __DEV
#endif // __BANK

bool NetworkInterface::SendResponseBuffer(const netTransactionInfo& txInfo,
                        const void* buffer,
                        const unsigned numBytes)
{
    return GetPlayerMgr().SendResponseBuffer(txInfo,
                                            buffer,
                                            numBytes);
}

bool NetworkInterface::SendRequestBuffer(const netPlayer* player,
                        const void* buffer,
                        const unsigned numBytes,
                        const unsigned timeout,
                        netResponseHandler* handler)
{
    return GetPlayerMgr().SendRequestBuffer(player,
                                            buffer,
                                            numBytes,
                                            timeout,
                                            handler);
}

bool NetworkInterface::PendingPopulationTransition()
{
	return CNetworkObjectPopulationMgr::PendingTransitionalObjects();
}


#if ENABLE_NETWORK_LOGGING
void NetworkInterface::WriteDataPopulationLog(const char* generatorname
							,const char* functionname
							,const char* eventName
							,const char* extraData, ...)
{
	static char s_PopLastfunction[netLog::MAX_LOG_STRING];
	static char s_PopLastbuffer[netLog::MAX_LOG_STRING];
	static u32  s_PopLastFrame;

	if (NetworkDebug::LogExtraDebugNetworkPopulation())
	{
		char buffer[netLog::MAX_LOG_STRING];
		va_list args;
		va_start(args,extraData);
		vsprintf(buffer,extraData,args);
		va_end(args);

		if ((s_PopLastFrame != fwTimer::GetFrameCount()) || ((strcmp(s_PopLastfunction, functionname) != 0) && (strcmp(s_PopLastbuffer, buffer) != 0)))
		{
			s_PopLastFrame = fwTimer::GetFrameCount();
			formatf(s_PopLastbuffer,   netLog::MAX_LOG_STRING, buffer);
			formatf(s_PopLastfunction, netLog::MAX_LOG_STRING, functionname);

			if (NetworkInterface::IsGameInProgress())
			{
				NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), generatorname, functionname);
				CNetwork::GetObjectManager().GetLog().WriteDataValue(eventName, extraData, buffer);
			}
			
			gnetDebug3("%s [%s] - %s %s", generatorname, functionname, eventName, buffer);
		}
	}
	else
	{
		s_PopLastFrame = fwTimer::GetFrameCount();
		sysMemSet(s_PopLastbuffer,   0, netLog::MAX_LOG_STRING);
		sysMemSet(s_PopLastfunction, 0, netLog::MAX_LOG_STRING);
	}
}
#endif // ENABLE_NETWORK_LOGGING

u64 NetworkInterface::GetExeSize( )
{
	static u64 s_exeSize = 0;

	if (s_exeSize > 0)
	{
		return s_exeSize;
	}

	const fiDevice* dev = fiDevice::GetDevice(sysParam::GetProgramName());
	if (dev)
	{
		// note: if something goes wrong (e.g. somehow exe name doesn't match) this will return 0,
		// don't use 0 as a valid size
		s_exeSize = dev->GetFileSize(sysParam::GetProgramName());

		return s_exeSize;
	}

    return 0;
}

bool NetworkInterface::UsedVoiceChat()
{
	return CNetwork::GetVoice().IsEnabled() && CNetwork::GetVoice().IsLocalTalking(GetLocalGamerIndex());
}

void NetworkInterface::HACK_CheckAllPlayersInteriorFlag()
{
	unsigned numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer* const *physicalPlayers = netInterface::GetAllPhysicalPlayers();

	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *netGamePlayer = SafeCast(const CNetGamePlayer, physicalPlayers[index]);
		CPed* ped = netGamePlayer->GetPlayerPed();
		if(ped && ped->GetNetworkObject())
		{
			bool bRetainedInInterior = ped->GetIsRetainedByInteriorProxy();
			if(!bRetainedInInterior)
				SafeCast(CNetObjPlayer, ped->GetNetworkObject())->SetRetainedInInterior(bRetainedInInterior);
		}
	}
}

#if !__FINAL
bool NetworkInterface::DebugIsRockstarDev()
{
	return CLiveManager::GetSCGamerDataMgr().IsRockstarDev();
}



#endif
