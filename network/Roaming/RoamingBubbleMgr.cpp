//
// name:        RoamingBubbleMgr.cpp
// description: The roaming bubble manager is responsible for keeping track of the different bubbles the players in
//              a roaming session are part of and managing the migration of the local player to different bubbles
//              based on different criteria.
// written by:  Daniel Yelland
//
NETWORK_OPTIMISATIONS()

#include "RoamingBubbleMgr.h"

#include "fwnet/netutils.h"

#include "debug/VectorMap.h"
#include "network/NetworkInterface.h"
#include "network/debug/NetworkBot.h"
#include "network/general/NetworkUtil.h"
#include "network/Network.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetGamePlayer.h"
#include "network/roaming/RoamingMessages.h"

#if __BANK
static bool     g_DisplayActiveBubbleInfo   = false;
static bool     g_DisplayBubblesOnVectorMap = false;
static unsigned g_DebugRoamingBubbleID      = 0;
static bool     g_UseDebugRoamingBubble     = false;
#endif // __BANK

RAGE_DEFINE_SUBCHANNEL(net, bubble, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_bubble

CRoamingBubble::CRoamingBubble() :
m_BubbleID(INVALID_BUBBLE_ID)
, m_NumPlayers(0)
, m_PlayerPositionCentroid(VEC3_ZERO)
, m_BubbleRadius(0.0f)
, m_LocalBubble(false)
{
    Init();
}

CRoamingBubble::~CRoamingBubble()
{
}

void CRoamingBubble::Init()
{
    for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
    {
        m_Players[index] = 0;
    }

    m_NumPlayers   = 0;
    m_BubbleRadius = 0.0f;
}

void CRoamingBubble::Shutdown()
{
    for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
    {
        m_Players[index] = 0;
    }

    m_NumPlayers = 0;
}

const Vector3 &CRoamingBubble::GetPlayerPositionCentroid()
{
    return m_PlayerPositionCentroid;
}

float CRoamingBubble::GetDistanceFromPlayerPositionCentroid(const Vector3 &position)
{
    float distance = (position - m_PlayerPositionCentroid).XYMag2();

    return distance;
}

float CRoamingBubble::GetBubbleRadius()
{
    return m_BubbleRadius;
}

unsigned CRoamingBubble::GetBubbleID()
{
    return m_BubbleID;
}

unsigned CRoamingBubble::GetNumPlayers()
{
    return m_NumPlayers;
}

CNetGamePlayer *CRoamingBubble::GetBubbleLeader()
{
    CNetGamePlayer *bubbleLeader = 0;

    for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
    {
        if(m_Players[index])
        {
            if(bubbleLeader == 0 || (bubbleLeader->GetRlPeerId() < m_Players[index]->GetRlPeerId()))
            {
                bubbleLeader = m_Players[index];
            }
        }
    }

    return bubbleLeader;
}

bool CRoamingBubble::IsFull()
{
    return m_NumPlayers == MAX_PLAYERS_PER_BUBBLE;
}

void CRoamingBubble::SetBubbleID(unsigned bubbleID)
{
    m_BubbleID = bubbleID;
}

void CRoamingBubble::AddPlayer(CNetGamePlayer *player, unsigned playerID)
{
	gnetDebug1("AddPlayer :: Adding %s, PlayerID: %u. BubbleID: %u", player ? player->GetLogName() : "Invalid", playerID, m_BubbleID);

	if(gnetVerifyf(player, "Invalid player specified!"))
	{
		if(gnetVerifyf(m_NumPlayers < MAX_PLAYERS_PER_BUBBLE, "AddPlayer :: This bubble already has the maximum number of players!"))
		{
			bool addedPlayer = false;

			if (playerID != INVALID_BUBBLE_PLAYER_ID)
			{
				if(gnetVerifyf(playerID < MAX_PLAYERS_PER_BUBBLE, "AddPlayer :: Invalid PlayerId specified!"))
				{
					// remove any existing player in this slot
					CNetGamePlayer *pExistingPlayer = m_Players[playerID];

					if (pExistingPlayer && gnetVerifyf(pExistingPlayer != player, "AddPlayer :: Added player already exists in this slot!"))
					{
						gnetDebug1("AddPlayer :: Resolving conflict. Removing existing player %s with the same PlayerID: %u, BubbleID: %u as newly added player %s",
									pExistingPlayer->GetLogName(), playerID, m_BubbleID, player->GetLogName());

						RemovePlayer(pExistingPlayer);
						gnetAssert(m_Players[playerID] == NULL);
					}

					m_Players[playerID] = player;
                    m_NumPlayers++;
					addedPlayer = true;
				}
			}
			else
			{
				// find a free slot for the new player 
				for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE && !addedPlayer; index++)
				{
					if(m_Players[index] == 0)
					{
						m_Players[index] = player;
						m_NumPlayers++;
						addedPlayer = true;
						playerID = index;
						gnetDebug1("AddPlayer :: Assigned free PlayerId: %u", playerID);
					}
				}
			}

			gnetAssertf(addedPlayer, "AddPlayer :: Failed to add player to the bubble!");

			if (addedPlayer)
			{
				CRoamingBubbleMemberInfo memberInfo(m_BubbleID, playerID);
				player->SetRoamingBubbleMemberInfo(memberInfo);

				if (player->IsMyPlayer())
					m_LocalBubble = true;

				if (m_LocalBubble)
				{
					// the physical players array matches the player array of the local bubble. This ensures that all machines agree on what 
					// physical slot each player in the bubble occupies.
					NetworkInterface::GetPlayerMgr().SetPhysicalPlayerIndex(*player, static_cast<PhysicalPlayerIndex>(playerID));

                    // when our player joins a bubble make sure any existing players in the bubble
                    // have their physical player indices set correctly
                    if(player->IsMyPlayer())
                    {
                        for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
                        {
                            CNetGamePlayer *existingPlayer = m_Players[index];

                            if(existingPlayer && player != existingPlayer)
                            {
                                PhysicalPlayerIndex existingID = static_cast<PhysicalPlayerIndex>(existingPlayer->GetRoamingBubbleMemberInfo().GetPlayerID());

                                if(existingID != existingPlayer->GetPhysicalPlayerIndex())
                                {
                                    gnetAssertf(NetworkInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex(existingID) == 0, "Adding a player at an already full bubble slot!");

                                    if(gnetVerifyf(existingID != INVALID_PLAYER_INDEX, "A player is in a bubble without a valid PlayerId!"))
                                    {
										gnetDebug1("AddPlayer :: Correcting PlayerId for %s. Was: %u, Now: %u", existingPlayer->GetLogName(), existingPlayer->GetPhysicalPlayerIndex(), existingID);
										NetworkInterface::GetPlayerMgr().SetPhysicalPlayerIndex(*existingPlayer, existingID);
                                    }
                                }
                            }
                        }
                    }

					// inform the network managers about the player joining
					CNetwork::PlayerHasJoinedBubble(*player);
				}
			}
		}
	}
}

void CRoamingBubble::RemovePlayer(CNetGamePlayer *player)
{
	gnetDebug1("[%u] RemovePlayer :: Removing %s, PlayerID: %u", m_BubbleID, player ? player->GetLogName() : "Invalid", player ? player->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX);

	if(gnetVerifyf(player, "Invalid player specified!"))
    {
        bool foundPlayer = false;

        for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE && !foundPlayer; index++)
        {
            if(m_Players[index] == player && gnetVerifyf(m_NumPlayers > 0, "Trying to remove a player from an empty bubble!"))
            {
                m_Players[index] = 0;
                m_NumPlayers--;

				if (m_LocalBubble)
				{
					// inform the network managers about the player leaving
					CNetwork::PlayerHasLeftBubble(*player);

					// the physical players array matches the player array of the local bubble. This ensures that all machines agree on what 
					// physical slot each player in the bubble occupies.
					NetworkInterface::GetPlayerMgr().SetPhysicalPlayerIndex(*player, INVALID_PLAYER_INDEX);

					if (player->IsMyPlayer())
						m_LocalBubble = false;
				}

				foundPlayer = true;
            }
        }

        gnetAssertf(foundPlayer, "Player is not a member of the bubble!");
    }
}

void CRoamingBubble::Update()
{
    if(m_NumPlayers > 0)
    {
        m_PlayerPositionCentroid = CalculatePlayerPositionCentroid(&m_BubbleRadius, false);
    }
}

#if __BANK

void CRoamingBubble::DisplayDebugText()
{
    if(m_NumPlayers > 0)
    {
        grcDebugDraw::AddDebugOutput("Bubble %d : Num Players %d", GetBubbleID(), GetNumPlayers());

        CNetGamePlayer *bubbleLeader = GetBubbleLeader();

        for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
        {
            if(m_Players[index])
            {
                bool isBubbleLeader = (bubbleLeader == m_Players[index]);
                grcDebugDraw::AddDebugOutput("\t%s%s", m_Players[index]->GetLogName(), isBubbleLeader ? " (Leader)" : "");
            }
        }
    }
}

#if __DEV

void CRoamingBubble::DisplayOnVectorMap()
{
    if(m_NumPlayers > 0)
    {
        float bubbleRadius = 0.0f;
        Vector3 bubbleCentroid = CalculatePlayerPositionCentroid(&bubbleRadius, true);

        static const float BUBBLE_RADIUS_OFFSET = 5.0f; // increase the radius of the circle rendered so the furthest player is not on the circle edge

        CVectorMap::DrawCircle(bubbleCentroid, bubbleRadius + BUBBLE_RADIUS_OFFSET, Color32(1.0f, 1.0f, 1.0f), false, true);
    }
}

#endif // __DEV
#endif // __BANK

Vector3 CRoamingBubble::CalculatePlayerPositionCentroid(float *bubbleRadius, bool includeLocalPlayers)
{
    Vector3 centroid;

    if(m_NumPlayers > 0)
    {
        // update the player centroid values (only interested in the X-Y plane)
        float    xPos       = 0.0f;
        float    yPos       = 0.0f;
        unsigned numPlayers = 0;

        for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE && (numPlayers < m_NumPlayers); index++)
        {
            if(m_Players[index] && (!m_Players[index]->IsLocal() || includeLocalPlayers))
            {
                Vector3 playerPos = NetworkUtils::GetNetworkPlayerPosition(*m_Players[index]);

                xPos += playerPos.x;
                yPos += playerPos.y;

                numPlayers++;
            }
        }

        xPos /= numPlayers;
        yPos /= numPlayers;

        centroid.x = xPos;
        centroid.y = yPos;

        // update the bubble radius if it was supplied
        if(bubbleRadius)
        {
            *bubbleRadius = 0.0f;

            for(unsigned index = 0; index < MAX_PLAYERS_PER_BUBBLE; index++)
            {
                if(m_Players[index])
                {
                    Vector3 playerPos = NetworkUtils::GetNetworkPlayerPosition(*m_Players[index]);

                    float distance = (playerPos - centroid).XYMag2();

                    if(distance > *bubbleRadius)
                    {
                        *bubbleRadius = distance;
                    }
                }
            }

            if(*bubbleRadius > 0.0f)
            {
                *bubbleRadius = sqrtf(*bubbleRadius);
            }
        }
    }

    return centroid;
}

// CRoamingBubbleMgr
CRoamingBubbleMgr::CRoamingBubbleMgr()
 : m_TimeOfLastBubbleSwap(0)
 , m_CheckBubbleRequirementTimestamp(0)
 , m_AlwaysCheckBubbleRequirement(false)
 , m_NeverCheckBubbleRequirement(false)
#if __BANK
 , m_NumRoamingBubbleMessagesSent(0)
#endif // __BANK
{
    m_Dlgt.Bind(this, &CRoamingBubbleMgr::OnMessageReceived);

    for(unsigned index = 0; index < MAX_BUBBLES; index++)
    {
        m_Bubbles[index].SetBubbleID(index);
    }
}

CRoamingBubbleMgr::~CRoamingBubbleMgr()
{
}

void CRoamingBubbleMgr::Init()
{
    NetworkInterface::GetPlayerMgr().AddDelegate(&m_Dlgt);

    for(unsigned index = 0; index < MAX_BUBBLES; index++)
    {
        m_Bubbles[index].Init();
    }

    m_BubbleMigrationTarget = INVALID_BUBBLE_ID;
    m_TimeOfLastBubbleSwap  = sysTimer::GetSystemMsTime();

    BANK_ONLY(m_NumRoamingBubbleMessagesSent = 0);

}

void CRoamingBubbleMgr::Shutdown()
{
    NetworkInterface::GetPlayerMgr().RemoveDelegate(&m_Dlgt);

    for(unsigned index = 0; index < MAX_BUBBLES; index++)
    {
        m_Bubbles[index].Shutdown();
    }
}

void CRoamingBubbleMgr::AllocatePlayerInitialBubble(CNetGamePlayer *player)
{
	gnetDebug1("AllocatePlayerInitialBubble :: For %s", player ? player->GetLogName() : "Invalid");

	if(gnetVerifyf(player, "Invalid player specified"))
	{
		if(gnetVerifyf(NetworkInterface::IsHost(), "Only the host can allocate a player's initial bubble!"))
		{
			for(unsigned index = 0; index < MAX_BUBBLES; index++)
			{
				if(!m_Bubbles[index].IsFull())
				{
				   m_Bubbles[index].AddPlayer(player);
				   break;
				}
			}

			gnetAssertf(player->GetRoamingBubbleMemberInfo().IsValid(), "Failed to add new player to an initial bubble!");
		}
    }
}

void CRoamingBubbleMgr::AddPlayerToBubble(CNetGamePlayer *player, const CRoamingBubbleMemberInfo& newMemberInfo)
{
	gnetDebug1("AddPlayerToBubble :: For %s, Bubble: %u, Player: %u", player ? player->GetLogName() : "Invalid", newMemberInfo.GetBubbleID(), newMemberInfo.GetPlayerID());

	if(gnetVerifyf(player, "Invalid player specified"))
    {
        if(!player->GetRoamingBubbleMemberInfo().IsEqualTo(newMemberInfo))
        {
            // remove the player from their old bubble if necessary
            if(player->GetRoamingBubbleMemberInfo().IsValid())
            {
                RemovePlayerFromBubble(player);
            }

            if(gnetVerifyf(newMemberInfo.GetBubbleID() != INVALID_BUBBLE_ID, "Invalid bubble ID specified"))
            {
                m_Bubbles[newMemberInfo.GetBubbleID()].AddPlayer(player, newMemberInfo.GetPlayerID());
            }
        }
    }
}

void CRoamingBubbleMgr::RemovePlayerFromBubble(CNetGamePlayer *player)
{
	if(gnetVerifyf(player, "Invalid player specified"))
    {
		gnetDebug1("RemovePlayerFromBubble :: For %s, Bubble: %u", player->GetLogName(), player->GetRoamingBubbleMemberInfo().GetBubbleID());

		if(gnetVerifyf(player->GetRoamingBubbleMemberInfo().IsValid(), "Invalid bubble member info specified"))
        {
            m_Bubbles[player->GetRoamingBubbleMemberInfo().GetBubbleID()].RemovePlayer(player);

            player->GetRoamingBubbleMemberInfo().Invalidate();
        }
    }
}

void CRoamingBubbleMgr::RequestBubble()
{
	CRoamingRequestBubbleMsg requestBubbleMsg;

	CNetGamePlayer* pHostPlayer = NetworkInterface::GetHostPlayer();
	if(gnetVerifyf(pHostPlayer, "RequestBubble :: No host player!"))
	{
		gnetDebug1("RequestBubble :: Requesting from %s", pHostPlayer->GetLogName());

		netSequence seq = 0;
		NetworkInterface::SendReliableMessage(pHostPlayer, requestBubbleMsg, &seq);
		requestBubbleMsg.WriteToLogFile(false, seq, *pHostPlayer);

        BANK_ONLY(m_NumRoamingBubbleMessagesSent++);
	}
}

void CRoamingBubbleMgr::Update()
{
#if __BANK
    UpdateDebug();
#endif

    for(unsigned index = 0; index < MAX_BUBBLES; index++)
    {
        m_Bubbles[index].Update();
    }

    CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();
    UpdateLocalPlayerBubble(localPlayer);

	if(NetworkInterface::IsHost() || !NetworkInterface::IsMigrating())
	{
		UpdateRemotePlayerBubbles();
	}
}

void CRoamingBubbleMgr::PlayerHasJoined(const netPlayer &player)
{
	gnetDebug1("PlayerHasJoined :: %s, Host: %s", player.GetLogName(), NetworkInterface::IsHost() ? "True" : "False");

	if(NetworkInterface::IsHost())
	{
		CNetGamePlayer& netGamePlayer = (CNetGamePlayer&)(player);
		
		gnetDebug1("PlayerHasJoined :: ShouldAllocate: %s", netGamePlayer.GetShouldAllocateBubble() ? "True" : "False");

        if(!player.IsLocal() && netGamePlayer.GetShouldAllocateBubble())
		{
			// we might need to check with a remote player if they require a bubble
			if(ShouldCheckIfBubbleRequired())
			{
				gnetDebug1("PlayerHasJoined :: Checking if bubble required");

				CRoamingRequestBubbleRequiredCheckMsg bubbleMsg;
				netSequence seq = 0;
				NetworkInterface::SendReliableMessage(&netGamePlayer, bubbleMsg, &seq);
				bubbleMsg.WriteToLogFile(false, seq, netGamePlayer);
				BANK_ONLY(m_NumRoamingBubbleMessagesSent++);
			}
			else
			{
				gnetDebug1("PlayerHasJoined :: Allocating bubble");
				AllocateRemoteBubble(&netGamePlayer);
			}
		}
	}
}

void CRoamingBubbleMgr::PlayerHasLeft(const netPlayer &player)
{
	gnetDebug1("PlayerHasLeft :: %s", player.GetLogName());

	CNetGamePlayer& netGamePlayer = (CNetGamePlayer&)(player);
    if(netGamePlayer.GetRoamingBubbleMemberInfo().IsValid())
    {
        RemovePlayerFromBubble(&netGamePlayer);
    }
}

void CRoamingBubbleMgr::SetCheckBubbleRequirementTime(const unsigned nTime_Ms)
{
	m_CheckBubbleRequirementTimestamp = sysTimer::GetSystemMsTime() + nTime_Ms;
	gnetDebug1("SetCheckBubbleRequirementTime :: TimePeriod: %ums, Expires: %ums", nTime_Ms, m_CheckBubbleRequirementTimestamp);
}

void CRoamingBubbleMgr::SetAlwaysCheckBubbleRequirement(const bool bCheck)
{
	if(m_AlwaysCheckBubbleRequirement != bCheck)
	{
		gnetDebug1("SetAlwaysCheckBubbleRequirement :: %s", bCheck ? "True" : "False");
		m_AlwaysCheckBubbleRequirement = bCheck;
	}
}

void CRoamingBubbleMgr::SetNeverCheckBubbleRequirement(const bool bCheck)
{
	if(m_NeverCheckBubbleRequirement != bCheck)
	{
		gnetDebug1("SetNeverCheckBubbleRequirement :: %s", bCheck ? "True" : "False");
		m_NeverCheckBubbleRequirement = bCheck;
	}
}

bool CRoamingBubbleMgr::ShouldCheckIfBubbleRequired()
{
	// check never before always
	if(m_NeverCheckBubbleRequirement)
		return false;

	if(m_AlwaysCheckBubbleRequirement)
		return true;

	return (m_CheckBubbleRequirementTimestamp != 0) && (m_CheckBubbleRequirementTimestamp > sysTimer::GetSystemMsTime()); 
}

#if __BANK

void CRoamingBubbleMgr::DisplayDebugText()
{
    if(g_DisplayActiveBubbleInfo)
    {
        for(unsigned index = 0; index < MAX_BUBBLES; index++)
        {
            m_Bubbles[index].DisplayDebugText();
        }
    }
}

void CRoamingBubbleMgr::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug Roaming", false);
            bank->AddToggle("Display Active Bubble Info", &g_DisplayActiveBubbleInfo);
            bank->AddToggle("Display Bubbles On Vector Map", &g_DisplayBubblesOnVectorMap);
            bank->AddSlider("Debug Roaming Bubble ID", &g_DebugRoamingBubbleID, 0, MAX_BUBBLES - 1, 1);
            bank->AddToggle("Use Debug Roaming Bubble", &g_UseDebugRoamingBubble);
        bank->PopGroup();
    }
}

void CRoamingBubbleMgr::UpdateDebug()
{
    unsigned targetBubbleID = 0;

    if(g_UseDebugRoamingBubble)
    {
        targetBubbleID = g_DebugRoamingBubbleID;
    }

    CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();

    if(localPlayer->GetRoamingBubbleMemberInfo().IsValid())
    {
        if(localPlayer->GetRoamingBubbleMemberInfo().GetBubbleID() != targetBubbleID)
        {
            if(CanSwapBubble(localPlayer))
            {
                CNetGamePlayer *bubbleLeader = m_Bubbles[targetBubbleID].GetBubbleLeader();

                if(bubbleLeader == 0)
                {
                    bubbleLeader = NetworkInterface::GetHostPlayer();
                }

                if(bubbleLeader)
                {
                    if(bubbleLeader->IsLocal())
                    {
                        CRoamingBubbleMemberInfo newMemberInfo(targetBubbleID, INVALID_BUBBLE_PLAYER_ID);
                        AddPlayerToBubble(localPlayer, newMemberInfo);
                    }
                    else
                    {
                        CRoamingJoinBubbleMsg joinBubbleMsg(targetBubbleID);
                        netSequence seq = 0;
                        NetworkInterface::SendReliableMessage(bubbleLeader, joinBubbleMsg, &seq);
                        joinBubbleMsg.WriteToLogFile(false, seq, *bubbleLeader);

                        BANK_ONLY(m_NumRoamingBubbleMessagesSent++);

                        m_BubbleMigrationTarget = targetBubbleID;
                    }
                }
            }
        }
    }
}

#if __DEV

void CRoamingBubbleMgr::DisplayBubblesOnVectorMap()
{
    if(g_DisplayBubblesOnVectorMap)
    {
        for(unsigned index = 0; index < MAX_BUBBLES; index++)
        {
            m_Bubbles[index].DisplayOnVectorMap();
        }

        CVectorMap::GoodSettings();
    }
}

#endif // __DEV

#endif // __BANK

void CRoamingBubbleMgr::AllocateRemoteBubble(CNetGamePlayer *remotePlayer)
{
	gnetDebug1("AllocateRemoteBubble :: %s", remotePlayer ? remotePlayer->GetLogName() : "Invalid");

	// allocate the joiner to an initial roaming bubble
	if(gnetVerifyf(NetworkInterface::IsHost(), "AllocateRemoteBubble :: Not host!"))
	{
		AllocatePlayerInitialBubble(remotePlayer);

		if(remotePlayer->IsRemote() && gnetVerifyf(NetworkInterface::GetLocalPlayer(), "No local player!"))
		{
			CRoamingInitialBubbleMsg initialBubbleMsg(remotePlayer->GetRoamingBubbleMemberInfo(),
				NetworkInterface::GetLocalPlayer()->GetRoamingBubbleMemberInfo());

			netSequence seq = 0;
			NetworkInterface::SendReliableMessage(remotePlayer, initialBubbleMsg, &seq);
			initialBubbleMsg.WriteToLogFile(false, seq, *remotePlayer);

            BANK_ONLY(m_NumRoamingBubbleMessagesSent++);
		}
	}
}

void CRoamingBubbleMgr::UpdateLocalPlayerBubble(CNetGamePlayer *localPlayer)
{
    if(CanSwapBubble(localPlayer))
    {
        Vector3 playerPos = NetworkUtils::GetNetworkPlayerPosition(*localPlayer);

        if(gnetVerifyf(localPlayer->GetRoamingBubbleMemberInfo().IsValid(), "Local player has invalid bubble member info!"))
        {
			unsigned bubbleID = localPlayer->GetRoamingBubbleMemberInfo().GetBubbleID();

			float distFromCurrentBubble    = m_Bubbles[bubbleID].GetDistanceFromPlayerPositionCentroid(playerPos);
            float distToClosestOtherBubble = FLT_MAX;
            unsigned closestOtherBubbleID  = INVALID_BUBBLE_ID;

            for(unsigned index = 0; index < MAX_BUBBLES; index++)
            {
                if(index != bubbleID && m_Bubbles[index].GetNumPlayers() > 0)
                {
                    float dist = m_Bubbles[index].GetDistanceFromPlayerPositionCentroid(playerPos);

                    if(dist < distToClosestOtherBubble)
                    {
                        distToClosestOtherBubble = dist;
                        closestOtherBubbleID     = index;
                    }
                }
            }

            if(closestOtherBubbleID != INVALID_BUBBLE_ID)
            {
                static const float BUBBLE_SWAP_THRESHOLD = 50.0f;

                if(distToClosestOtherBubble < distFromCurrentBubble && ((distFromCurrentBubble - distToClosestOtherBubble) > BUBBLE_SWAP_THRESHOLD))
                {
                    CNetGamePlayer *bubbleLeader = m_Bubbles[closestOtherBubbleID].GetBubbleLeader();

                    if(gnetVerifyf(bubbleLeader, "Trying to migrate to a bubble with no leader") &&
                        !bubbleLeader->IsLocal())
                       //gnetVerifyf(!bubbleLeader->IsLocal(), "Trying to migrate to a bubble with a local bubble leader!"))
                    {
                        CRoamingJoinBubbleMsg joinBubbleMsg(closestOtherBubbleID);
                        netSequence seq = 0;
                        NetworkInterface::SendReliableMessage(bubbleLeader, joinBubbleMsg, &seq);
                        joinBubbleMsg.WriteToLogFile(false, seq, *bubbleLeader);

                        BANK_ONLY(m_NumRoamingBubbleMessagesSent++);

                        m_BubbleMigrationTarget = closestOtherBubbleID;
                    }
                }
            }
        }
    }
}

void CRoamingBubbleMgr::UpdateRemotePlayerBubbles()
{
    // we only need to update the remote player bubbles for non-physical players
    // physical players are by definition in our local players current bubble
    unsigned           numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
    netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
    
    for(unsigned index = 0; index < numRemoteActivePlayers; index++)
    {
        CNetGamePlayer *player = SafeCast(CNetGamePlayer, remoteActivePlayers[index]);

        if(!player->IsPhysical())
        {
            CNonPhysicalPlayerData *nonPhysicalData = player ? static_cast<CNonPhysicalPlayerData *>(player->GetNonPhysicalData()) : 0;

		    if(gnetVerifyf(player, "Invalid player!") && gnetVerifyf(nonPhysicalData, "Player has no non-physical data!"))
            {
                if(player->GetRoamingBubbleMemberInfo().GetBubbleID() != nonPhysicalData->GetRoamingBubbleMemberInfo().GetBubbleID())
                {
                    if(nonPhysicalData->GetRoamingBubbleMemberInfo().IsValid())
                    {
						gnetDebug1("UpdateRemotePlayerBubbles :: Adding %s to bubble", player->GetLogName());
						AddPlayerToBubble(player, nonPhysicalData->GetRoamingBubbleMemberInfo());
                    }
                }
            }
        }
        else
        {
            // handle any physical players that had not sent a non-physical update before
            // cloning their player on our machine
            if(!player->GetRoamingBubbleMemberInfo().IsValid())
            {
                CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();

                if(localPlayer && localPlayer->GetRoamingBubbleMemberInfo().IsValid())
                {
					gnetDebug1("UpdateRemotePlayerBubbles :: Adding %s to local bubble", player->GetLogName());
					AddPlayerToBubble(player, localPlayer->GetRoamingBubbleMemberInfo());
                }
            }
        }
	}
}

bool CRoamingBubbleMgr::CanSwapBubble(CNetGamePlayer *localPlayer)
{
    bool canSwapBubble = false;

    if(gnetVerifyf(localPlayer, "Invalid player specified!") && gnetVerifyf(localPlayer->IsLocal(), "Non-local player specified!"))
    {
        // don't try and swap bubbles until we have been assigned our initial bubble
        if(localPlayer->GetRoamingBubbleMemberInfo().IsValid())
        {
            // don't try and swap bubbles if we are in the process of migrating to another bubble
            if(m_BubbleMigrationTarget == INVALID_BUBBLE_ID)
            {
                // only try to swap bubbles after we have been in the current bubble for the minimum duration
                unsigned currTime = sysTimer::GetSystemMsTime();

                const unsigned MIN_DURATION_BETWEEN_BUBBLE_SWAPS = 10000;

                if((currTime - m_TimeOfLastBubbleSwap) >= MIN_DURATION_BETWEEN_BUBBLE_SWAPS)
                {
                    canSwapBubble = true;
                }
            }
        }
    }

    return canSwapBubble;
}

void CRoamingBubbleMgr::OnMessageReceived(const ReceivedMessageData &messageData)
{
    unsigned msgId = 0;
    if(netMessage::GetId(&msgId, messageData.m_MessageData, messageData.m_MessageDataSize))
    {
        if(msgId == CRoamingInitialBubbleMsg::MSG_ID())
        {
            HandleInitialBubbleMsg(messageData);
        }
		if(msgId == CRoamingRequestBubbleMsg::MSG_ID())
		{
			HandleRequestBubbleMsg(messageData);
		}
		else if(msgId == CRoamingJoinBubbleMsg::MSG_ID())
        {
            HandleJoinBubbleMsg(messageData);
        }
        else if(msgId == CRoamingJoinBubbleAckMsg::MSG_ID())
        {
            HandleJoinBubbleAckMsg(messageData);
        }
		else if(msgId == CRoamingRequestBubbleRequiredCheckMsg::MSG_ID())
		{
			HandleBubbleRequiredCheckMsg(messageData);
		}
		else if(msgId == CRoamingRequestBubbleRequiredResponseMsg::MSG_ID())
		{
			HandleBubbleRequiredResponseMsg(messageData);
		}
    }
}

void CRoamingBubbleMgr::HandleInitialBubbleMsg(const ReceivedMessageData &messageData)
{
    if(!gnetVerify(messageData.IsValid()))
    {
        return;
    }

    CRoamingInitialBubbleMsg initialBubbleMsg;
    if(!gnetVerify(initialBubbleMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
    {
        return;
    }

	gnetDebug1("HandleInitialBubbleMsg :: Local - Bubble: %u, PlayerId: %u, Host - Bubble: %u, PlayerId: %u", 
			  initialBubbleMsg.GetJoinerBubbleMemberInfo().GetBubbleID(),
			  initialBubbleMsg.GetJoinerBubbleMemberInfo().GetPlayerID(),
			  initialBubbleMsg.GetHostBubbleMemberInfo().GetBubbleID(),
			  initialBubbleMsg.GetHostBubbleMemberInfo().GetPlayerID());

    initialBubbleMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

    if(messageData.m_FromPlayer->IsHost())
    {
        if(gnetVerifyf(initialBubbleMsg.GetJoinerBubbleMemberInfo().IsValid(), "Invalid bubble member info received!"))
        {
            CNetGamePlayer *localPlayer = SafeCast(CNetGamePlayer, messageData.m_ToPlayer);
            AddPlayerToBubble(localPlayer, initialBubbleMsg.GetJoinerBubbleMemberInfo());

            CNetGamePlayer *hostPlayer = SafeCast(CNetGamePlayer, messageData.m_FromPlayer);
            AddPlayerToBubble(hostPlayer, initialBubbleMsg.GetHostBubbleMemberInfo());
        }
    }
#if !__NO_OUTPUT
    else
    {
        // check if this has come from a player we don't have a physical index for yet and this is during a migration
        // this indicates that the player has elected themselves as host (not yet knowing enough about the session to
        // migrate to existing players). We can safely ignore this. 
        bool fromNonPhysicalPlayerDuringMigration = (messageData.m_FromPlayer->GetPhysicalPlayerIndex() == INVALID_PLAYER_INDEX) && NetworkInterface::IsMigrating();
        
        if(NetworkInterface::IsSessionEstablished() && !fromNonPhysicalPlayerDuringMigration)
        {
            // this should not happen if we have started a session
            gnetAssertf(0, "Received an initial bubble message from a non-host player");
        }
        else
        {
            // this can happen during the join process if the session migrates after we 
            // join the session but before we receive the bubble information from the host
            // we joined (and get it from the new host)
            gnetWarning("Received an initial bubble message from a non-host player");
        }
    }
#endif
}

void CRoamingBubbleMgr::HandleRequestBubbleMsg(const ReceivedMessageData &messageData)
{
	if(!gnetVerify(messageData.IsValid()))
	{
		return;
	}

	CRoamingRequestBubbleMsg requestBubbleMsg;
	if(!gnetVerify(requestBubbleMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
	{
		return;
	}

	gnetDebug1("HandleRequestBubbleMsg :: From: %s", messageData.m_FromPlayer->GetLogName());

	requestBubbleMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

	// make sure this didn't come from the host
	if(gnetVerifyf(!messageData.m_FromPlayer->IsHost(), "Received a bubble request message from a host player"))
	{
		if(NetworkInterface::IsHost())
		{
			CNetGamePlayer *netGamePlayer = SafeCast(CNetGamePlayer, messageData.m_FromPlayer);
			AllocateRemoteBubble(netGamePlayer);
		}
	}
}

void CRoamingBubbleMgr::HandleJoinBubbleMsg(const ReceivedMessageData &messageData)
{
    if(!gnetVerify(messageData.IsValid()))
    {
        return;
    }

    CRoamingJoinBubbleMsg joinBubbleMsg;
    if(!gnetVerify(joinBubbleMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
    {
        return;
    }

	joinBubbleMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

	unsigned bubbleID = joinBubbleMsg.GetBubbleID();

    CRoamingJoinBubbleAckMsg::AckCode ackCode = CRoamingJoinBubbleAckMsg::ACKCODE_UNKNOWN_FAIL;

    if(gnetVerifyf(bubbleID < MAX_BUBBLES, "Invalid Bubble ID received!"))
    {
        CNetGamePlayer *bubbleOwner = SafeCast(CNetGamePlayer, messageData.m_ToPlayer);

        if(gnetVerifyf(bubbleOwner, "Bubble owner doesn't exist!"))
        {
            gnetAssertf(bubbleOwner->IsLocal(), "Bubble owner is not owned by this machine!");

            if(bubbleID != bubbleOwner->GetRoamingBubbleMemberInfo().GetBubbleID())
            {
                ackCode = CRoamingJoinBubbleAckMsg::ACKCODE_NOT_LEADER;
            }
            else if(bubbleOwner != m_Bubbles[bubbleID].GetBubbleLeader())
            {
                ackCode = CRoamingJoinBubbleAckMsg::ACKCODE_NOT_LEADER;
            }
            else if(m_Bubbles[bubbleID].IsFull())
            {
                ackCode = CRoamingJoinBubbleAckMsg::ACKCODE_BUBBLE_FULL;
            }
            else
            {
				CRoamingBubbleMemberInfo memberInfo(bubbleID, INVALID_BUBBLE_PLAYER_ID);

                AddPlayerToBubble(static_cast<CNetGamePlayer *>(messageData.m_FromPlayer), memberInfo);

                ackCode = CRoamingJoinBubbleAckMsg::ACKCODE_SUCCESS;
            }
        }
    }

	gnetDebug1("HandleRequestBubbleMsg :: From: %s, Bubble: %u, AckCode: %u", messageData.m_FromPlayer->GetLogName(), bubbleID, ackCode);

	CRoamingJoinBubbleAckMsg joinBubbleAckMsg(ackCode, static_cast<CNetGamePlayer*>(messageData.m_FromPlayer)->GetRoamingBubbleMemberInfo());
    netSequence seq = 0;
	NetworkInterface::SendReliableMessage(messageData.m_FromPlayer, joinBubbleAckMsg, &seq);
    joinBubbleAckMsg.WriteToLogFile(false, seq, *messageData.m_FromPlayer);

    BANK_ONLY(m_NumRoamingBubbleMessagesSent++);
}

void CRoamingBubbleMgr::HandleJoinBubbleAckMsg(const ReceivedMessageData &messageData)
{
    if(gnetVerify(messageData.IsValid()))
    {
        CRoamingJoinBubbleAckMsg joinBubbleAckMsg;
        if(gnetVerify(joinBubbleAckMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
        {
			gnetDebug1("HandleJoinBubbleAckMsg :: From: %s, Bubble: %u, PlayerId: %u, AckCode: %u", 
						messageData.m_FromPlayer->GetLogName(),
						joinBubbleAckMsg.GetBubbleMemberInfo().GetBubbleID(), 
						joinBubbleAckMsg.GetBubbleMemberInfo().GetPlayerID(),
						joinBubbleAckMsg.GetAckCode());

            joinBubbleAckMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

            if(m_BubbleMigrationTarget == joinBubbleAckMsg.GetBubbleMemberInfo().GetBubbleID())
            {
                if(joinBubbleAckMsg.GetAckCode() == CRoamingJoinBubbleAckMsg::ACKCODE_SUCCESS)
                {
                    CNetGamePlayer *playerToAdd = SafeCast(CNetGamePlayer, messageData.m_ToPlayer);
                    AddPlayerToBubble(playerToAdd, joinBubbleAckMsg.GetBubbleMemberInfo());
                }

                m_BubbleMigrationTarget = INVALID_BUBBLE_ID;
                m_TimeOfLastBubbleSwap  = sysTimer::GetSystemMsTime();
            }
        }
    }
}

void CRoamingBubbleMgr::HandleBubbleRequiredCheckMsg(const ReceivedMessageData &messageData)
{
	if(gnetVerify(messageData.IsValid()))
	{
		CRoamingRequestBubbleRequiredCheckMsg bubbleMsg;
		if(gnetVerify(bubbleMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
		{
			gnetDebug1("HandleBubbleRequiredCheckMsg :: From: %s", messageData.m_FromPlayer->GetLogName());

			bubbleMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

			// check this came from the host
			if(messageData.m_FromPlayer->IsHost())
			{
				CRoamingBubbleMemberInfo memberInfo; 
				if(NetworkInterface::GetLocalPlayer())
				{
					memberInfo.Copy(NetworkInterface::GetLocalPlayer()->GetRoamingBubbleMemberInfo());
				}

				gnetDebug1("HandleBubbleRequiredCheckMsg :: Required: %s", memberInfo.IsValid() ? "True" : "False");

				CRoamingRequestBubbleRequiredResponseMsg responseMsg(memberInfo.IsValid(), memberInfo);
				netSequence seq = 0;
				NetworkInterface::SendReliableMessage(messageData.m_FromPlayer, responseMsg, &seq);
				responseMsg.WriteToLogFile(false, seq, *messageData.m_FromPlayer);

				BANK_ONLY(m_NumRoamingBubbleMessagesSent++);
			}
#if !__NO_OUTPUT
			else
			{
				// check if this has come from a player we don't have a physical index for yet and this is during a migration
				// this indicates that the player has elected themselves as host (not yet knowing enough about the session to
				// migrate to existing players). We can safely ignore this. 
				bool fromNonPhysicalPlayerDuringMigration = (messageData.m_FromPlayer->GetPhysicalPlayerIndex() == INVALID_PLAYER_INDEX) && NetworkInterface::IsMigrating();

				if(NetworkInterface::IsSessionEstablished() && !fromNonPhysicalPlayerDuringMigration)
				{
					// this should not happen if we have started a session
					gnetAssertf(0, "Received a check bubble message from a non-host player");
				}
				else
				{
					// this can happen during the join process if the session migrates after we 
					// join the session but before we receive the bubble information from the host
					// we joined (and get it from the new host)
					gnetWarning("Received a check bubble message from a non-host player");
				}
			}
#endif
		}
	}
}

void CRoamingBubbleMgr::HandleBubbleRequiredResponseMsg(const ReceivedMessageData &messageData)
{
	if(!gnetVerify(messageData.IsValid()))
	{
		return;
	}

	CRoamingRequestBubbleRequiredResponseMsg responseMsg;
	if(!gnetVerify(responseMsg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
	{
		return;
	}

	gnetDebug1("HandleBubbleRequiredResponseMsg :: From: %s, Required: %s, BubbleId: %u, PlayerId: %u", 
		messageData.m_FromPlayer->GetLogName(), 
		responseMsg.IsRequired() ? "True" : "False",
		responseMsg.GetBubbleMemberInfo().GetBubbleID(),
		responseMsg.GetBubbleMemberInfo().GetPlayerID());

	responseMsg.WriteToLogFile(true, messageData.m_NetSequence, *messageData.m_FromPlayer);

	// make sure this didn't come from the host
	if(gnetVerifyf(!messageData.m_FromPlayer->IsHost(), "Received a bubble required response message from a host player"))
	{
		if(NetworkInterface::IsHost() && responseMsg.IsRequired())
		{
			CNetGamePlayer *netGamePlayer = SafeCast(CNetGamePlayer, messageData.m_FromPlayer);
			AllocateRemoteBubble(netGamePlayer);
		}
	}
}
