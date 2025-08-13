//
// name:        NetworkPlayerMgr.cpp
// description: Project specific player management code
//
NETWORK_OPTIMISATIONS()

#include "Network/Players/NetworkPlayerMgr.h"

#include "system/param.h"

#include "fwnet/netutils.h"

#include "Network/Network.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Debug/NetworkBot.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Live/LiveManager.h"
#include "Network/Players/NetGamePlayerMessages.h"
#include "Peds/Ped.h"
#include "Scene/ExtraContent.h"
#include "scene/world/GameWorld.h"

RAGE_DEFINE_SUBCHANNEL(net, playermgr, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_playermgr

PARAM(forgePlayedWithData, "Fills the PlayedWith player collection with artificial data, to ease testing");

CNetworkPlayerMgr::CNetworkPlayerMgr(netBandwidthMgr &bandwidthMgr)
: netPlayerMgrBase(bandwidthMgr),
  m_PlayedWithMetricInterval(DEFAULT_PLAYED_WITH_METRIC_INTERVAL),
  m_PlayedWithMetricLastWrite(0),
  m_ShouldSendPlayedWithMetric(false),
  m_LoadedFromTunables(false)
{
	m_PlayersPlayedWith.Reset();
}

CNetworkPlayerMgr::~CNetworkPlayerMgr()
{
    ShutdownPrivate();
}

bool CNetworkPlayerMgr::Init(netConnectionManager* cxnMgr,
                             const unsigned channelId,
                             PhysicalOnRemoteCallback physicalOnRemoteCallback)
{
    CNetwork::UseNetworkHeap(NET_MEM_PLAYER_MGR);
	CNonPhysicalPlayerData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    // This needs to be done before the player manager base class is initialised
    if(!IsInitialized())
    {
        gnetAssert(m_PlayerPool.empty());

        //Build the pool of players.
        for(PhysicalPlayerIndex i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; ++i)
        {
            m_PlayerPool.push_back(&m_PlayerPile[i]);
			m_TempPlayerPool.push_back(&m_TempPlayerPile[i]);
        }
    }

    bool success = netPlayerMgrBase::Init(cxnMgr, channelId, physicalOnRemoteCallback);

    if(gnetVerify(success))
    {
        // update the local player with the game specific data
        if (CGameWorld::FindLocalPlayer())
        {
			GetMyPlayer()->SetPlayerInfo(CGameWorld::FindLocalPlayer()->GetPlayerInfo());
			gnetDebug2("Init :: Creating new player info (%p). Grabbing from local player", GetMyPlayer()->GetPlayerInfo());
        }
        else
        {
			GetMyPlayer()->SetPlayerInfo(rage_new CPlayerInfo(NULL, NetworkInterface::GetActiveGamerInfo()));
			gnetDebug2("Init :: Creating new player info (%p). Name = %s", GetMyPlayer()->GetPlayerInfo(), NetworkInterface::GetActiveGamerInfo() ? NetworkInterface::GetActiveGamerInfo()->GetName() : "");
        }
		
#if !__NO_OUTPUT
		if ((Channel_net.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_net.TtyLevel >= DIAG_SEVERITY_DEBUG2))
		{
			sysStack::PrintStackTrace();
		}
#endif

        GetLog().WriteDataValue("Player Name", "%s", GetMyPlayer()->GetLogName());

		if(!TryLoadTunables())
		{
			gnetDebug1("Failed to load Tunables to populate NetworkPlayerMgr");
		}
    }

	return success;
}

void CNetworkPlayerMgr::SendPlayedWithMetric(OUTPUT_ONLY(const char* WIN32PC_ONLY(reason)))
{
#if RSG_PC
	const u64 sessionId = CNetworkTelemetry::GetCurMpSessionId();
	gnetDebug1("SendPlayedWithMetric: %s session: %" I64FMT "u", reason, sessionId);

	if (sessionId == 0)
	{
		return;
	}

	if(PARAM_forgePlayedWithData.Get())
		FillPlayedWithCollection();
	MetricPlayedWith m(m_PlayersPlayedWith, sessionId);
	APPEND_METRIC(m);
#endif
}

void CNetworkPlayerMgr::Shutdown()
{
	gnetDebug1("Shutdown :: NonPhysicalAllocations: %d", GetNumNonPhysicalAllocations());

	RemoveTemporaryPlayers();

    ShutdownPrivate();

    gnetAssert(m_PlayerPool.size() == COUNTOF(m_PlayerPile));
    m_PlayerPool.clear();

	gnetAssert(m_TempPlayerPool.size() == COUNTOF(m_TempPlayerPile));
	m_TempPlayerPool.clear();
	m_TempPlayers.clear();

    netPlayerMgrBase::Shutdown();

    CNetwork::UseNetworkHeap(NET_MEM_PLAYER_MGR);
	CNonPhysicalPlayerData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();
}

void CNetworkPlayerMgr::Update()
{
	netPlayerMgrBase::Update();
	UpdatePlayedWithMetric();
}

bool CNetworkPlayerMgr::ShouldSendPlayedWithMetric()
{
	if(!m_ShouldSendPlayedWithMetric)
		return false;
	if(!sysTimer::HasElapsedIntervalMs(m_PlayedWithMetricLastWrite, m_PlayedWithMetricInterval))
		return false;
	if(!NetworkInterface::IsSessionEstablished())
		return false;
	return true;
}
void CNetworkPlayerMgr::UpdatePlayedWithMetric()
{
#if RSG_PC
	if(!m_LoadedFromTunables)
	{
		if(!TryLoadTunables())
		{
			return;
		}
	}
	if(ShouldSendPlayedWithMetric())
	{
		SendPlayedWithMetric(OUTPUT_ONLY("UpdatePlayedWithMetric"));
		m_PlayedWithMetricLastWrite = sysTimer::GetSystemMsTime();
	}
	
#endif
}

bool CNetworkPlayerMgr::TryLoadTunables()
{
#if RSG_PC
	if(Tunables::GetInstance().HaveTunablesLoaded() && Tunables::GetInstance().HasCloudRequestFinished())
	{
		m_PlayedWithMetricInterval = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("PLAYED_WITH_METRIC_INTERVAL", 0x438A79C), DEFAULT_PLAYED_WITH_METRIC_INTERVAL);
		m_ShouldSendPlayedWithMetric = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("ENABLE_PLAYED_WITH_METRIC", 0xD8135951), false);
		m_PlayedWithMetricLastWrite = sysTimer::GetSystemMsTime();
		m_LoadedFromTunables = true;
	}
#endif
	return m_LoadedFromTunables;
}
netPlayer *CNetworkPlayerMgr::AddPlayer(const rlGamerInfo& gamerInfo,
                                        const EndpointId endpointId,
                                        const playerDataMsg& playerData,
                                        nonPhysicalPlayerDataBase *nonPhysicalPlayerData)
{
    netPlayer *pPlayer = netPlayerMgrBase::AddPlayer(gamerInfo, endpointId, playerData, nonPhysicalPlayerData);

	gnetDebug1("AddPlayer :: Adding %s - NonPhysicalAllocations: %d", gamerInfo.GetName(), GetNumNonPhysicalAllocations());

    if(gnetVerify(pPlayer))
    {
		CNetGamePlayer* pGamePlayer = static_cast<CNetGamePlayer*>(pPlayer);
        pGamePlayer->SetPlayerType(CNetGamePlayer::NETWORK_PLAYER);
		pGamePlayer->SetShouldAllocateBubble(NetworkInterface::IsHost());
	}

    return pPlayer;
}

netPlayer* CNetworkPlayerMgr::AddTemporaryPlayer(const rlGamerInfo &gamerInfo, const EndpointId endpointId,
												 const playerDataMsg &playerData, nonPhysicalPlayerDataBase *nonPhysicalPlayerData)
{
	gnetDebug1("AddTemporaryPlayer :: Adding %s - NonPhysicalAllocations: %d", gamerInfo.GetName(), GetNumNonPhysicalAllocations());

	// add the base temporary player
	CNetGamePlayer *pPlayer = static_cast<CNetGamePlayer *>(netPlayerMgrBase::AddTemporaryPlayer(gamerInfo, endpointId, playerData, nonPhysicalPlayerData));

    // if successful, we need to set the player type and info.
	if(gnetVerify(pPlayer))
	{
		((CNetGamePlayer*)pPlayer)->SetPlayerType(CNetGamePlayer::NETWORK_PLAYER);

		const CNetGamePlayerDataMsg *gamePlayerData = SafeCast(const CNetGamePlayerDataMsg, &playerData);

		// validate matchmaking group
		gnetAssert(gamePlayerData->m_MatchmakingGroup != MM_GROUP_INVALID);

		SetCustomPlayerData(pPlayer, gamerInfo, gamePlayerData);

		GetLog().WriteDataValue("Matchmaking Group", "%d", gamePlayerData->m_MatchmakingGroup);
		GetLog().WriteDataValue("Is Spectator", "%d", (gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR) != 0);
		GetLog().WriteDataValue("Team", "%d", gamePlayerData->m_Team);
		GetLog().WriteDataValue("Crew ID", "%" I64FMT "d", gamePlayerData->m_ClanId);
		GetLog().WriteDataValue("Boss", "%d", (gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS) != 0);
	}

	return pPlayer;
}


#if ENABLE_NETWORK_BOTS

netPlayer *CNetworkPlayerMgr::AddNetworkBot(const rlGamerInfo         &gamerInfo,
                                            nonPhysicalPlayerDataBase *nonPhysicalPlayerData)
{
	gnetDebug1("AddNetworkBot :: Adding %s - NonPhysicalAllocations: %d", gamerInfo.GetName(), GetNumNonPhysicalAllocations());

	CNetGamePlayer *newPlayer = static_cast<CNetGamePlayer *>(AllocatePlayer());

    if(gnetVerify(newPlayer))
    {
        newPlayer->Init(NET_NAT_OPEN);
        newPlayer->SetPlayerType(CNetGamePlayer::NETWORK_BOT);

        newPlayer->SetGamerInfo(gamerInfo);
		newPlayer->SetIsLocal(gamerInfo.IsLocal());

        m_ExistingPlayers.push_back(newPlayer);
		m_PendingPlayers.push_back(newPlayer);

        NetworkLogUtils::WriteLogEvent(GetLog(), "ADDING_NETWORK_BOT", "");
        GetLog().WriteDataValue("Player Name", "%s",  newPlayer->GetLogName());
        GetLog().WriteDataValue("Active player index", "%d",  newPlayer->GetActivePlayerIndex());

        gnetAssert(nonPhysicalPlayerData);
        gnetAssert(newPlayer->GetNonPhysicalData() == 0);
        newPlayer->SetNonPhysicalData(nonPhysicalPlayerData);

        // associate this player with the network bot
        CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(gamerInfo);

        if(gnetVerify(networkBot))
        {
            // the peer player index must be set before the player is associated with the network bot,
            // as this is used during the SetNetPlayer call
            unsigned localIndex = 0;
            if(gnetVerifyf(networkBot->GetLocalIndex(localIndex), "Failed to get local index"))
            {
                newPlayer->SetPeerPlayerIndex(localIndex);
            }

            if(gamerInfo.IsLocal())
            {
                CNetGamePlayer *hostPlayer = NetworkInterface::GetHostPlayer();

                if(gnetVerifyf(hostPlayer, "Attempting to add a local network bot when we have no host!") && 
                   gnetVerifyf(hostPlayer->GetRoamingBubbleMemberInfo().IsValid(), "Attempting to add a local network bot with no valid local bubble info!"))
                {
                    networkBot->SetPendingBubbleID(hostPlayer->GetRoamingBubbleMemberInfo().GetBubbleID());
                }
            }

            networkBot->SetNetPlayer(newPlayer);

            // hook up parameters from the parent player of this bot
            CNetGamePlayer *parentPlayer = SafeCast(CNetGamePlayer, GetPlayerFromPeerId(newPlayer->GetRlPeerId()));
			if(gnetVerify(parentPlayer && !parentPlayer->IsBot()))
			{
				newPlayer->SetMatchmakingGroup(parentPlayer->GetMatchmakingGroup());
				if(gamerInfo.IsRemote())
				{
					newPlayer->SetConnectionId(parentPlayer->GetConnectionId())
					newPlayer->SetEndpointId(parentPlayer->GetEndpointId()); 
				}
            }

			// update the state of the player in the lists
			UpdatePlayerListsForPlayer(newPlayer);
        }
    }

    return newPlayer;
}

#endif // ENABLE_NETWORK_BOTS

void CNetworkPlayerMgr::ActivatePlayer(netPlayer* player)
{
    gnetDebug1("ActivatePlayer :: Activating %s", player->GetLogName());
    CNetwork::PlayerHasJoinedSession(*player);
}

void CNetworkPlayerMgr::RemovePlayer(const rlGamerInfo& gamerInfo)
{
    gnetDebug1("RemovePlayer :: Removing %s", gamerInfo.GetName());
    
    if(GetMyPlayer() && GetMyPlayer()->GetRlGamerId() != gamerInfo.GetGamerId())
    {
        netPlayer* pPlayer = GetPlayerFromGamerId(gamerInfo.GetGamerId());

        if(!pPlayer)
        {
            //This case occurs when we've restarted the game to
            //transition to the next game mode or return to the
            //rendezvous session and players who are members of the
            //session are leaving and restarting their games to do the same.
            //Because we've restarted, we don't have references to
            //these players in the player mgr, but they are still members
            //of the underlying session.
        }
		else
		{
			RemovePlayedWith(pPlayer);
		}
    }

    netPlayerMgrBase::RemovePlayer(gamerInfo);
}

void CNetworkPlayerMgr::RemoveTemporaryPlayers()
{
	// build a list of temporary players to remove
	rlGamerInfo playersToRemove[MAX_NUM_ACTIVE_PLAYERS];
	unsigned numPlayersToRemove = 0;
	PlayerList::const_iterator tempIt   = m_TempPlayers.begin();
	PlayerList::const_iterator tempEnd = m_TempPlayers.end();
	for(; tempIt != tempEnd; ++tempIt)
	{
		if(gnetVerifyf(numPlayersToRemove < MAX_NUM_ACTIVE_PLAYERS, "Adding too many players to the removal array!"))
		{
			CNetGamePlayer* player = (CNetGamePlayer*)*tempIt;
			if (player)
			{
				playersToRemove[numPlayersToRemove] = player->GetGamerInfo();
				numPlayersToRemove++;
			}
		}
	}

	// remove the temporary player
	gnetDebug1("%u temporary players to remove", numPlayersToRemove);
	for (int i = 0; i < numPlayersToRemove; i++)
	{
		RemoveTemporaryPlayer(playersToRemove[i]);
	}
}

CNetGamePlayer *CNetworkPlayerMgr::GetPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex, const rlPeerInfo &peerInfo)
{
    CNetGamePlayer *player = 0;

    if(peerPlayerIndex == 0)
    {
        return SafeCast(CNetGamePlayer, GetPlayerFromPeerId(peerInfo.GetPeerId()));
    }
#if ENABLE_NETWORK_BOTS
    else
    {
        // peer player indices greater than 0 indicate network bots
        CNetGamePlayer *mainPlayerForPeer = SafeCast(CNetGamePlayer, GetPlayerFromPeerId(peerInfo.GetPeerId()));

        if(mainPlayerForPeer)
        {
            CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(*mainPlayerForPeer, peerPlayerIndex);

            if(networkBot)
            {
                player = networkBot->GetNetPlayer();
            }
        }
    }
#endif // ENABLE_NETWORK_BOTS

    return player;
}

CNetGamePlayer *CNetworkPlayerMgr::GetMyPlayer()
{
    return static_cast<CNetGamePlayer*>(netPlayerMgrBase::GetMyPlayer());
}

const CNetGamePlayer *CNetworkPlayerMgr::GetMyPlayer() const
{
    return static_cast<const CNetGamePlayer*>(netPlayerMgrBase::GetMyPlayer());
}

CNetGamePlayer* CNetworkPlayerMgr::GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex)
{
	if (IsInitialized() &&
        gnetVerify(playerIndex < int(MAX_NUM_ACTIVE_PLAYERS)) &&
		m_PlayerPile[playerIndex].IsValid())
	{
		return &m_PlayerPile[playerIndex];
	}

	return NULL;
}

const CNetGamePlayer* CNetworkPlayerMgr::GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex) const
{
	return const_cast<CNetworkPlayerMgr*>(this)->GetActivePlayerFromIndex(playerIndex);
}

bool
CNetworkPlayerMgr::GetPlayerData(const rlGamerId& gamerId,
                                 CNetGamePlayerDataMsg* playerData) const
{
    bool success = false;

    CNetGamePlayer* pPlayer = (CNetGamePlayer*)GetPlayerFromGamerId(gamerId);

    // if the player does not exist, check for the temporary player
	if (!pPlayer)
	{
		pPlayer = (CNetGamePlayer*)GetTempPlayer(gamerId);
	}

    if(gnetVerify(pPlayer))
    {
		unsigned nPlayerFlags = 0;
		if(pPlayer->IsSpectator()) 
			nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR;
		if(pPlayer->IsRockstarDev())
			nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_ROCKSTAR_DEV;
		if(pPlayer->IsBoss())
			nPlayerFlags |= CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS;

        playerData->Reset(CNetwork::GetVersion(),
						  pPlayer->GetNatType(),
						  pPlayer->GetPlayerType(),
						  pPlayer->GetMatchmakingGroup(),
                          nPlayerFlags,
                          pPlayer->GetTeam(),
						  NetworkGameConfig::GetAimPreference(),
						  pPlayer->GetClanDesc().m_Id,
                          pPlayer->GetCharacterRank(),
                          0,
						  NetworkGameConfig::GetMatchmakingRegion());
		success = true;
    }

    return success;
}

unsigned
CNetworkPlayerMgr::GetPlayerData(const rlGamerId& gamerId,
                                void* buf,
                                const unsigned maxSizeofBuf) const
{
    CNetGamePlayerDataMsg playerData;
    unsigned sizeofData = 0;

    if(!this->GetPlayerData(gamerId, &playerData)
        || !playerData.Export(buf, maxSizeofBuf, &sizeofData))
    {
        sizeofData = 0;
    }

    return sizeofData;
}

#if !__NO_OUTPUT
s32 CNetworkPlayerMgr::GetNumNonPhysicalAllocations()
{
	return CNonPhysicalPlayerData::GetPool() ? CNonPhysicalPlayerData::GetPool()->GetNoOfUsedSpaces() : 0;
}
#endif

netPlayer *CNetworkPlayerMgr::AllocateTempPlayer()
{
	CNetGamePlayer *newPlayer = 0;

	// ensure that the temporary player pool is not empty
	if(gnetVerify(!m_TempPlayerPool.empty()))
	{
        // retrieve the first player from the temp pool
		newPlayer = (CNetGamePlayer *)*m_TempPlayerPool.begin();
		m_TempPlayerPool.pop_front();

		// add to the temp players list
		m_TempPlayers.push_back(newPlayer);

		// set the active player index
		if(newPlayer)
		{
			newPlayer->SetActivePlayerIndex(static_cast<ActivePlayerIndex>(newPlayer - m_TempPlayerPile));
            gnetDebug1("AllocateTempPlayer :: ActivePlayerIndex: %d, NumAllocated: %u", newPlayer->GetActivePlayerIndex(), static_cast<unsigned>(m_TempPlayers.size()));
		}
	}

	return newPlayer;
}

void CNetworkPlayerMgr::DeallocateTempPlayer(netPlayer *player)
{
	if(gnetVerify(player))
	{
        m_TempPlayers.erase(player);
		m_TempPlayerPool.push_back(player);
		gnetDebug1("DeallocateTempPlayer :: ActivePlayerIndex: %d, NumAllocated: %u", player->GetActivePlayerIndex(), static_cast<unsigned>(m_TempPlayers.size()));
	}
}

netPlayer* CNetworkPlayerMgr::GetTempPlayer(const rlGamerInfo& info) const
{
	return GetTempPlayer(info.GetGamerId());
}

unsigned CNetworkPlayerMgr::GetNumTempPlayers() const
{
	return static_cast<unsigned>(m_TempPlayers.size());
}

netPlayer* CNetworkPlayerMgr::GetTempPlayer(const rlGamerId& gamerId) const
{
	CNetGamePlayer* pPlayer = NULL;

	PlayerList::const_iterator existIt   = m_TempPlayers.begin();
	PlayerList::const_iterator existStop = m_TempPlayers.end();
	for(; existIt != existStop; ++existIt)
	{
		CNetGamePlayer* player = (CNetGamePlayer*)*existIt;

		if(player->GetRlGamerId() == gamerId)
		{
			pPlayer = player;
			break;
		}
	}

	return pPlayer;
}

netPlayer *CNetworkPlayerMgr::AllocatePlayer()
{
    CNetGamePlayer *newPlayer = 0;

    if(gnetVerify(!m_PlayerPool.empty()))
    {
        newPlayer = (CNetGamePlayer *)*m_PlayerPool.begin();
        m_PlayerPool.pop_front();

        if(newPlayer)
        {
            newPlayer->SetActivePlayerIndex(static_cast<ActivePlayerIndex>(newPlayer - m_PlayerPile));
            gnetDebug1("AllocatePlayer :: ActivePlayerIndex: %d, NumAllocated: %u", newPlayer->GetActivePlayerIndex(), GetNumActivePlayers() + 1);
        }
    }

    return newPlayer;
}

void CNetworkPlayerMgr::DeallocatePlayer(netPlayer *player)
{
    if(gnetVerify(player))
    {
        gnetDebug1("DeallocatePlayer :: ActivePlayerIndex: %d, NumAllocated: %u", player->GetActivePlayerIndex(), GetNumActivePlayers() - 1);
        m_PlayerPool.push_back(player);
    }
}

void CNetworkPlayerMgr::ShutdownPrivate()
{
    if(netPlayerMgrBase::IsInitialized())
    {
        netPlayerMgrBase::Shutdown();
		m_PlayersPlayedWith.Reset();
    }
}

void CNetworkPlayerMgr::SetCustomPlayerData(CNetGamePlayer* player, const rlGamerInfo& gamerInfo, const CNetGamePlayerDataMsg *gamePlayerData)
{
    // WARNING: IF changing this code, please make corresponding changes to the temporary player copy in CNetworkPlayerMgr::AddRemotePlayer
	player->SetGamerInfo(gamerInfo);
	player->SetMatchmakingGroup(gamePlayerData->m_MatchmakingGroup);
	player->SetTeam(gamePlayerData->m_Team);
	player->SetCrewId(gamePlayerData->m_ClanId);
	player->SetSpectator((gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR) != 0);
	player->SetIsBoss((gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS) != 0);
}

netPlayer *CNetworkPlayerMgr::AddRemotePlayer(const rlGamerInfo& gamerInfo, const playerDataMsg& playerData, const EndpointId endpointId, nonPhysicalPlayerDataBase *nonPhysicalPlayerData)
{
    CNetGamePlayer *newPlayer = static_cast<CNetGamePlayer *>(netPlayerMgrBase::AddRemotePlayer(gamerInfo, playerData, endpointId, nonPhysicalPlayerData));

    if(gnetVerify(newPlayer))
    {
        const CNetGamePlayerDataMsg *gamePlayerData = SafeCast(const CNetGamePlayerDataMsg, &playerData);

        // validate matchmaking group
        gnetAssert(gamePlayerData->m_MatchmakingGroup != MM_GROUP_INVALID);

		// if a temporary player exists
		CNetGamePlayer* tempPlayer = (CNetGamePlayer*)GetTempPlayer(gamerInfo);
		if (tempPlayer)
		{
            gnetDebug1("AddRemotePlayer :: Adding %s (from temporary player)", gamerInfo.GetName());
            
            newPlayer->SetMatchmakingGroup(tempPlayer->GetMatchmakingGroup());
			newPlayer->SetSpectator(tempPlayer->IsSpectator());
			newPlayer->SetGamerInfo(gamerInfo);
			newPlayer->SetTeam(tempPlayer->GetTeam());
			newPlayer->SetCrewId(tempPlayer->GetCrewId());
			newPlayer->SetIsBoss(tempPlayer->IsBoss());

			GetLog().WriteDataValue("Matchmaking Group", "%d", tempPlayer->GetMatchmakingGroup());
			GetLog().WriteDataValue("Is Spectator", "%d", tempPlayer->IsSpectator());
			GetLog().WriteDataValue("Team", "%d", tempPlayer->GetTeam());
			GetLog().WriteDataValue("Crew ID", "%" I64FMT "d", tempPlayer->GetCrewId());
			GetLog().WriteDataValue("Boss", "%d", tempPlayer->IsBoss());
		}
		else
		{
            gnetDebug1("AddRemotePlayer :: Adding %s", gamerInfo.GetName());
            
            SetCustomPlayerData(newPlayer, gamerInfo, gamePlayerData);

			GetLog().WriteDataValue("Matchmaking Group", "%d", gamePlayerData->m_MatchmakingGroup);
			GetLog().WriteDataValue("Is Spectator", "%d", (gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_SPECTATOR) != 0);
			GetLog().WriteDataValue("Team", "%d", gamePlayerData->m_Team);
			GetLog().WriteDataValue("Crew ID", "%" I64FMT "d", gamePlayerData->m_ClanId);
			GetLog().WriteDataValue("Boss", "%d", (gamePlayerData->m_PlayerFlags & CNetGamePlayerDataMsg::PLAYER_MSG_FLAG_BOSS) != 0);
		}

		AddPlayedWith(newPlayer);
    }

    return newPlayer;
}


//PURPOSE
//	Extracts a netPlayer's gamer handle into an atString. Empty if nullptr
void CNetworkPlayerMgr::FillPlayedWithCollection()
{
#if RSG_PC && !RSG_FINAL
	static const char num_chars[] = "0123456789";
	const u32 numPlayers = 32;
	for(int i = 0; i < numPlayers; i++)
	{
		atString handle("SC ");
		for(int j = 0; j < RL_MAX_GAMER_HANDLE_CHARS; j++)
		{
			handle += num_chars[rand() % (sizeof(num_chars) - 1)];
		}
		m_PlayersPlayedWith.Insert(handle, 1);
	}
#endif

}

void CNetworkPlayerMgr::AddPlayedWith(netPlayer * WIN32PC_ONLY(newPlayer))
{
#if RSG_PC
	char newPlayerHandle[RL_MAX_GAMER_HANDLE_CHARS] = {0};
	newPlayer->GetGamerInfo().GetGamerHandle().ToString(newPlayerHandle, RL_MAX_GAMER_HANDLE_CHARS);
	if(m_PlayersPlayedWith.Access(atString(newPlayerHandle)) == nullptr)
	{
		gnetDebug2("AddPlayedWith :: Adding %s", newPlayer->GetGamerInfo().GetName());
		m_PlayersPlayedWith.Insert(atString(newPlayerHandle), 1);
	}
#endif
}

void CNetworkPlayerMgr::RemovePlayedWith(netPlayer * WIN32PC_ONLY(playerLeft))
{
#if RSG_PC
	char playerLeftHandle[RL_MAX_GAMER_HANDLE_CHARS] = {0};
	playerLeft->GetGamerInfo().GetGamerHandle().ToString(playerLeftHandle, RL_MAX_GAMER_HANDLE_CHARS);

	if(m_PlayersPlayedWith.Access(atString(playerLeftHandle)) != nullptr)
	{
		m_PlayersPlayedWith.Delete(atString(playerLeftHandle));
		gnetDebug2("RemovePlayedWith :: Removing %s", playerLeft->GetGamerInfo().GetName());

		if(m_ShouldSendPlayedWithMetric) { SendPlayedWithMetric(OUTPUT_ONLY("RemovePlayedWith")); }
	}
#endif
}

void CNetworkPlayerMgr::AddLocalPlayer(const rlGamerInfo& gamerInfo,
                                       const playerDataMsg& playerData,
                                       nonPhysicalPlayerDataBase *nonPhysicalPlayerData)
{
    gnetDebug1("AddLocalPlayer :: Adding %s", gamerInfo.GetName());
    netPlayerMgrBase::AddLocalPlayer(gamerInfo, playerData, nonPhysicalPlayerData);
}

void CNetworkPlayerMgr::RemovePlayer(netPlayer *player)
{
	if(!netPlayerMgrBase::IsShuttingDown() && !player->IsMyPlayer())
    {
        CNetwork::PlayerHasLeftSession(*player);
		RemovePlayedWith(player);
    }

    netPlayerMgrBase::RemovePlayer(player);

	gnetDebug1("RemovePlayer :: NonPhysicalAllocations: %d", GetNumNonPhysicalAllocations());
}
