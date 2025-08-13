//
// name:        RoamingBubbleMgr.h
// description: The roaming bubble manager is responsible for keeping track of the different bubbles the players in
//              a roaming session are part of and managing the migration of the local player to different bubbles
//              based on different criteria.
// written by:  Daniel Yelland
//

#ifndef ROAMING_BUBBLE_MGR_H
#define ROAMING_BUBBLE_MGR_H

// rage includes
#include "net/connectionmanager.h"
#include "net/event.h"
#include "vector/vector3.h"

// framework includes
#include "fwnet/netinterface.h"
#include "fwnet/nettypes.h"

// game includes
#include "network/Roaming/RoamingTypes.h"

class CNetGamePlayer;

namespace rage
{
    class netPlayer;
}

class CRoamingBubble
{
public:

    CRoamingBubble();
    ~CRoamingBubble();

    void Init();
    void Shutdown();

    const Vector3 &GetPlayerPositionCentroid();
    float GetDistanceFromPlayerPositionCentroid(const Vector3 &position);

    float           GetBubbleRadius();
    unsigned        GetBubbleID();
    unsigned        GetNumPlayers();
    CNetGamePlayer *GetBubbleLeader();
    bool            IsFull();

    void SetBubbleID(unsigned bubbleID);

	void AddPlayer(CNetGamePlayer *player, unsigned playerID = INVALID_BUBBLE_PLAYER_ID);
    void RemovePlayer(CNetGamePlayer *player);

    void Update();

#if __BANK
    void DisplayDebugText();
#if __DEV
    void DisplayOnVectorMap();
#endif // __DEV
#endif // __BANK

private:

    CRoamingBubble(const CRoamingBubble &);
    CRoamingBubble &operator=(const CRoamingBubble &);

    static const unsigned MAX_PLAYERS_PER_BUBBLE = MAX_NUM_PHYSICAL_PLAYERS;

    Vector3 CalculatePlayerPositionCentroid(float *bubbleRadius, bool includeLocalPlayers);

    Vector3         m_PlayerPositionCentroid;
    float           m_BubbleRadius;
    unsigned        m_BubbleID;
    unsigned        m_NumPlayers;
    CNetGamePlayer *m_Players[MAX_PLAYERS_PER_BUBBLE];
	bool			m_LocalBubble;	// true if this is the bubble our local player is a member of
};

class CRoamingBubbleMgr
{
public:

    CRoamingBubbleMgr();
    ~CRoamingBubbleMgr();

    void Init();
    void Shutdown();

    void AllocatePlayerInitialBubble(CNetGamePlayer *player);

    void AddPlayerToBubble(CNetGamePlayer *player, const CRoamingBubbleMemberInfo& newMemberInfo);
    void RemovePlayerFromBubble(CNetGamePlayer *player);
	void RequestBubble(); 

    void Update();

    // tells the manager about players joining/leaving the game
    void PlayerHasJoined(const netPlayer& player);
    void PlayerHasLeft(const netPlayer& player);

	void SetCheckBubbleRequirementTime(const unsigned nTime_Ms); 
	void SetAlwaysCheckBubbleRequirement(const bool bCheck); 
	void SetNeverCheckBubbleRequirement(const bool bCheck); 

#if __BANK
    static void AddDebugWidgets();
    void DisplayDebugText();

    void UpdateDebug();

    //PURPOSE
    // Returns the number of roaming messages sent since the last call to ResetMessageCounts()
    unsigned GetNumRoamingBubbleMessagesSent() const { return m_NumRoamingBubbleMessagesSent; }

    //PURPOSE
    // Reset the counts of roaming messages sent
    void ResetMessageCounts() { m_NumRoamingBubbleMessagesSent = 0; }

#if __DEV
    void DisplayBubblesOnVectorMap();
#endif // __DEV

#endif // __BANK

private:

	bool ShouldCheckIfBubbleRequired(); 
	void AllocateRemoteBubble(CNetGamePlayer *remotePlayer); 
	
    //PURPOSE
    // Handles an incoming network message
    //PARAMS
    // messageData - Data describing the incoming message
    void OnMessageReceived(const ReceivedMessageData &messageData);

    void HandleInitialBubbleMsg(const ReceivedMessageData &messageData);
	void HandleRequestBubbleMsg(const ReceivedMessageData &messageData);
	void HandleJoinBubbleMsg   (const ReceivedMessageData &messageData);
    void HandleJoinBubbleAckMsg(const ReceivedMessageData &messageData);
	void HandleBubbleRequiredCheckMsg   (const ReceivedMessageData &messageData);
	void HandleBubbleRequiredResponseMsg(const ReceivedMessageData &messageData);

	void UpdateLocalPlayerBubble(CNetGamePlayer *localPlayer);
    void UpdateRemotePlayerBubbles();
    bool CanSwapBubble(CNetGamePlayer *localPlayer);

    // Event handler wrapper object for connection manager events
    NetworkPlayerEventDelegate m_Dlgt;

    unsigned       m_BubbleMigrationTarget;
	unsigned       m_TimeOfLastBubbleSwap;
	CRoamingBubble m_Bubbles[MAX_BUBBLES];

	bool		   m_AlwaysCheckBubbleRequirement;
	bool		   m_NeverCheckBubbleRequirement;
	unsigned       m_CheckBubbleRequirementTimestamp;

#if __BANK
    unsigned m_NumRoamingBubbleMessagesSent; // the number or roaming bubble messages sent since the last call to ResetMessageCounts()
#endif // __BANK
};

#endif // ROAMING_BUBBLE_MGR_H
