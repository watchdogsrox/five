//
// netplayer.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORKPLAYERMGR_H
#define NETWORKPLAYERMGR_H

#include "avchat/textchat.h"
#include "fwnet/netplayermgrbase.h"


#include "Network/Players/NetGamePlayer.h"
#include "Network/Chat/NetworkTextChat.h"

class CNetGamePlayer;
class CNetGamePlayerDataMsg;
class CNetGamePlayer;

//PURPOSE
// The network player manager is responsible for managing functionality relating
// to the different players in a network session
class CNetworkPlayerMgr : public netPlayerMgrBase
{
public:

    //PURPOSE
    // Class constructor
    CNetworkPlayerMgr(netBandwidthMgr &bandwidthMgr);

    //PURPOSE
    // Class destructor
    ~CNetworkPlayerMgr();

    //PURPOSE
    // Initialises the network player manager
    //PARAMS
    // connectionMgr            - The connection manager used to communicate between network players
    // channelID                - The channel ID for sending player manager network messages
    // physicalOnRemoteCallback - Callback function for determining whether the local player is physical/non-physical
    //                            on other players machines
    virtual bool Init(netConnectionManager    *connectionMgr,
                      const unsigned           channelId,
                      PhysicalOnRemoteCallback physicalOnRemoteCallback);

    //PURPOSE
    // Shuts down the network player manager
    virtual void Shutdown();

    //PURPOSE
    // Performs any per frame processing required by the network player manager; this should
    // be called once per frame by your application
    virtual void Update();

    //PURPOSE
    //  Adds a player to the player manager
    //PARAMS
    // gamerInfo             - The gamer info of the new player
    // endpointId            - The network endpointId of the new player
	// playerData            - The network player data for the new player
    // nonPhysicalPlayerData - The non-physical data for the new player
    netPlayer* AddPlayer(const rlGamerInfo         &gamerInfo,
						 const EndpointId		   endpointId,
                         const playerDataMsg       &playerData,
                         nonPhysicalPlayerDataBase *nonPhysicalPlayerData);

	//PURPOSE
	// Adds a temporary player to open a connection and contain gamer data
    // during an asynchronous join on XB1
	//PARAMS
	// gamerInfo             - The gamer info of the new player
	// endpointId            - The network endpoint id of the new player
	// playerData            - The network player data for the new player
	// nonPhysicalPlayerData - The non-physical data for the new player
	virtual netPlayer* AddTemporaryPlayer(const rlGamerInfo         &gamerInfo,
											const EndpointId		 endpointId,
											const playerDataMsg      &playerData,
											nonPhysicalPlayerDataBase *nonPhysicalPlayerData);

	// PURPOSE
	// Retrieves a netPlayer from the temporary array
	virtual netPlayer *GetTempPlayer(const rlGamerInfo& info) const;
	virtual netPlayer *GetTempPlayer(const rlGamerId& gamerId) const;

	//PURPOSE
	// Returns number of temporary players
	unsigned GetNumTempPlayers() const;

#if ENABLE_NETWORK_BOTS
    //PURPOSE
    //  Adds a network bot to the player manager
    //PARAMS
    // gamerInfo             - The gamer info of the new player
    // nonPhysicalPlayerData - The non-physical data for the new player
    netPlayer* AddNetworkBot(const rlGamerInfo         &gamerInfo,
                             nonPhysicalPlayerDataBase *nonPhysicalPlayerData);
#endif // ENABLE_NETWORK_BOTS

    //PURPOSE
    // Removes a player from the player manager
    //PARAMS
    // gamerInfo - The gamer info of the player to remove
    void RemovePlayer(const rlGamerInfo& gamerInfo);

    //PURPOSE
    // Returns a network player controlled by the specified peer. Peers can support more
    // than one local player.
    //PARAMS
    // peerPlayerIndex - The player index of the player on the specified remote peer
    // peerInfo        - Description of the remote peer the player is associated with
    CNetGamePlayer* GetPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex, const rlPeerInfo &peerInfo);

    //PURPOSE
    // Returns the player object for the local player
    CNetGamePlayer       *GetMyPlayer();
    const CNetGamePlayer *GetMyPlayer() const;

    //PURPOSE
    // Returns the player object associated with the specified player index only if they are active
    //PARAMS
    // playerIndex - The player index
    CNetGamePlayer*       GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex);
    const CNetGamePlayer* GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex) const;

	//PURPOSE
    // Copies player data for the player with the specified gamer ID into the specified player data message
    //PARAMS
    // gamerID    - The gamer ID of the player to copy the data for
    // playerData - The message to copy the player data into
    bool GetPlayerData(const rlGamerId& gamerId,
                       CNetGamePlayerDataMsg* playerData) const;

    //PURPOSE
    // Copies player data to the given buffer and returns the size of the data.
    //PARAMS
    // gamerID    - The gamer ID of the player to copy the data for
    // buffer     - The buffer to copy the player data into
    // bufferSize - The size of the buffer supplied
    unsigned GetPlayerData(const rlGamerId& gamerID,
                           void* buffer,
                           const unsigned bufferSize) const;


	//PURPOSSE
	// Debug functionality (PC only) to populate the collection of PlayedWith with garbo data for widget testing
	void FillPlayedWithCollection();
private:

#if !__NO_OUTPUT
	s32 GetNumNonPhysicalAllocations(); 
#endif

	//PURPOSE
	// Allocates a new network player for use by the player manager
	virtual netPlayer *AllocateTempPlayer();

	//PURPOSE
	// Deallocates a network player for reuse by the player manager
	virtual void DeallocateTempPlayer(netPlayer *player);

    //PURPOSE
    // Allocates a new network player for use by the player manager
    virtual netPlayer *AllocatePlayer();

    //PURPOSE
    // Deallocates a network player for reuse by the player manager
    virtual void DeallocatePlayer(netPlayer *player);

    //PURPOSE
    // Private shutdown function for this class
    void ShutdownPrivate();

    //PURPOSE
    // Adds a remote player to the player manager
    //PARAMS
    // gamerInfo             - The gamer info for the new player
    // playerData            - The player data for the new player
	// endpointId			 - The endpoint id for the new player
	// nonPhysicalPlayerData - The non-physical player data for the new player
    virtual netPlayer* AddRemotePlayer(const rlGamerInfo& gamerInfo,
                                       const playerDataMsg& playerData,
                                       const EndpointId endpointId,
                                       nonPhysicalPlayerDataBase *nonPhysicalPlayerData);

    //PURPOSE
    // Adds the local player to the player manager
    //PARAMS
    // gamerInfo             - The gamer info for the new player
    // playerData            - The player data for the new player
    // nonPhysicalPlayerData - The non-physical player data for the new player
    void AddLocalPlayer(const rlGamerInfo& gamerInfo,
                        const playerDataMsg& playerData,
                        nonPhysicalPlayerDataBase *nonPhysicalPlayerData);

	// PURPOSE
	//	Sets the custom player data for the player
	// player         - The player to set
	// gamerInfo      - The game info
	// gamePlayerData - The player data for the player
	void SetCustomPlayerData(CNetGamePlayer* player, const rlGamerInfo& gamerInfo, const CNetGamePlayerDataMsg *gamePlayerData);

	//PURPOSE
	// Called when a player moves onto the active list
	//PARAMS
	// player - The player becoming active
	virtual void ActivatePlayer(netPlayer* player);

	//PURPOSE
    // Removes the specified player from the player manager
    //PARAMS
    // player - The player to remove
    void RemovePlayer(netPlayer* player);

	// PURPOSE
	//	Remove all temporary players
	void RemoveTemporaryPlayers();

    // The players representing remote machines
    CNetGamePlayer m_PlayerPile[MAX_NUM_ACTIVE_PLAYERS];
    PlayerList m_PlayerPool;

	// temporary players for async joins
	CNetGamePlayer m_TempPlayerPile[MAX_NUM_ACTIVE_PLAYERS];
	PlayerList m_TempPlayers;
	PlayerList m_TempPlayerPool;

	//PURPOSE
	//	Adds remote players to the set of players that the local player has seen
	void AddPlayedWith(netPlayer * newPlayer);

	//PURPOSE
	//	Adds remote players to the set of players that the local player has seen
	void RemovePlayedWith(netPlayer * newPlayer);

	// PURPOSE
	//	Checks for a tunable, builds a metric, and appends it
	void SendPlayedWithMetric(OUTPUT_ONLY(const char* reason));

	// PURPOSE
	//	Sends the PlayedWithMetric at the appropriate intervals
	void UpdatePlayedWithMetric();

	// PURPOSE
	//	Conditional logic as to whether a send should occur or not 
	bool ShouldSendPlayedWithMetric();

	// PURPOSE
	//  Load the played with conditional values from tunables
	//  Return: True if Tunables are loaded and Cloud Tunables loaded; false otherwise
	bool TryLoadTunables();

	// All of the players that this user has seen between starting the game and exiting the game (can span multiple sessions)
	atMap<atString, int> m_PlayersPlayedWith;

	// Time since last write of the telemetry
	u32 m_PlayedWithMetricLastWrite;

	// Interval to send telemetry
	u32 m_PlayedWithMetricInterval;

	// Default interval to send telemetry (1 minute)
	static const u32 DEFAULT_PLAYED_WITH_METRIC_INTERVAL = 1000 * 60;

	bool m_ShouldSendPlayedWithMetric;

	bool m_LoadedFromTunables;

};

#endif  // NETWORKPLAYERMGR_H
