//
// name:		NetworkEntityAreaWorldState.h
// description:	Support for network scripts to register an area in which entities might be created. This area will
//				be marked as available or unavailable by other players
// written by:	John Hynd
//

#ifndef NETWORK_ENTITY_AREA_WORLD_STATE_H
#define NETWORK_ENTITY_AREA_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes an area that can be reserved by script
class CNetworkEntityAreaWorldStateData : public CNetworkWorldStateData
{
public:

	enum IncludeFlags
	{
		eInclude_Vehicles = BIT0,
		eInclude_Peds = BIT1,
		eInclude_Objects = BIT2,
		eInclude_Default = eInclude_Vehicles | eInclude_Peds,
	};

	static const int MAX_NUM_NETWORK_IDS = 1024;

public:

	FW_REGISTER_CLASS_POOL(CNetworkEntityAreaWorldStateData);

	//PURPOSE
	// Class constructor
	CNetworkEntityAreaWorldStateData();

	//PURPOSE
	// Class constructor
	//PARAMS
	// scriptID     - The ID of the script that has disabled the road nodes
	// locallyOwned - Indicates whether this nodes have been disabled by the local machine
	// vAreaStart   - Start of the requested area
	// vAreaEnd     - End of the requested area
	CNetworkEntityAreaWorldStateData(const CGameScriptId &scriptID,
		bool                 locallyOwned,
        bool                 clientCreated,
        u64                  clientPeerID,
		int					 networkID,
		int					 scriptAreaID,
		IncludeFlags		 nFlags,
		const Vector3       &vAreaStart,
		const Vector3		&vAreaEnd,
		const float			areaWidth);

	//PURPOSE
	// Returns the type of this world state
	virtual unsigned GetType() const { return NET_WORLD_STATE_ENTITY_AREA; }

	//PURPOSE
	// Per frame update call
	virtual void Update();

	//PURPOSE
	// Indicates whether world states of a given type can be updated after
	// they are created. The default behaviour is to support just creating/reverting
	// world state
	virtual bool CanBeUpdated() const { return true; }

    //PURPOSE
    // Indicates when this entity area needs to be cleaned up. Client created entity
    // areas are automatically removed when the player leaves the session
    virtual bool HasExpired() const;

	//PURPOSE
	// Called when a new player joins the network session
	//PARAMS
	// player - The player joining the session
	virtual void PlayerHasJoined(const netPlayer& player);

	//PURPOSE
	// Called when a player leaves the network session
	//PARAMS
	// player - The player leaving the session
	virtual void PlayerHasLeft(const netPlayer& player);

	//PURPOSE
	// Makes a copy of the world state data, required by the world state data manager
	CNetworkEntityAreaWorldStateData *Clone() const;

    //PURPOSE
    // Returns whether this was created by a client of the script creating it
    bool IsClientCreated() const { return m_bClientCreated; }

    //PURPOSE
    // Returns the peer ID of the client that created this world state
    u64 GetClientPeerID() const { return m_ClientPeerID; }

	//PURPOSE
	// Returns the bounding box region of the requested area
	//PARAMS
	// vAreaStart   - Start of the requested area
	// vAreaEnd     - End of the requested area
	// fWidth		- The width if used otherwise will return -1.0f
	void GetArea(Vector3& vStart, Vector3& vEnd, float& fWidth) const;

	//PURPOSE   
	// Returns whether the specified region of disabled nodes 
	// disabled by a script is equal to the data stored in this instance
	//PARAMS
	// worldState - The world state to compare
	virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

	//PURPOSE
	// Switches off the road nodes within the region stored in this instance
	void ChangeWorldState();

	//PURPOSE
	// Switches on the road nodes within the region stored in this instance
	void RevertWorldState();

	//PURPOSE
	// Updates newly created world state data with any local only info stored 
	// in this object instance
	//PARAMS
	// hostWorldState - The world state to update from after a host response
	void UpdateHostState(CNetworkWorldStateData &hostWorldState);

	//PURPOSE
	// Adds an entity request area at the specified area start and end
	//PARAMS
	// scriptID		- The ID of the script that has disabled the road nodes
	// vAreaStart   - Start of the requested area
	// vAreaEnd     - End of the requested area
	// areaWidth	- If an angled area this will have a non -1.0f value - otherwise axis-aligned
	static int AddEntityArea(const CGameScriptId &scriptID, const Vector3& vAreaStart, const Vector3& vAreaEnd, const float areaWidth, const IncludeFlags nFlags, const bool bHostArea);

	//PURPOSE
	// Removes an entity request area at the specified area start and end
	//PARAMS
	// scriptID		- The ID of the script that has disabled the road nodes
	// vAreaStart   - Start of the requested area
	// vAreaEnd     - End of the requested area
	static bool RemoveEntityArea(const CGameScriptId &scriptID, const int scriptAreaID);

	//PURPOSE
	// Marks whether this area is occupied or not in a player's world view. Only called when changed.
	//PARAMS
	// networkID	- Network ID of the requested area
	// playerID		- Player ID that has updated the occupied state
	// bIsOccupied	- Is the requested area occupied on the player's machine
	static bool UpdateOccupiedState(const CGameScriptId &scriptID, const int networkID, const PhysicalPlayerIndex playerID, bool bIsOccupied);

	//PURPOSE
	// Checks whether all active players have provided a reply
	static bool HasWorldState(const int scriptAreaID);

	//PURPOSE
	// Gets a world state from script and network ID
	static CNetworkEntityAreaWorldStateData* GetWorldState(const CGameScriptId &scriptID, const int networkID);

	//PURPOSE
	// Gets a world state from local ID
	static CNetworkEntityAreaWorldStateData* GetWorldState(const int scriptAreaID);

	//PURPOSE
	// Checks whether all active players have provided a reply
	static bool HaveAllReplied(const int scriptAreaID);

	//PURPOSE
	// Checks whether we have a reply from a particular player ID
	//PARAMS
	// playerIDD - Player ID to check we have a reply from
	static bool HasReplyFrom(const int scriptAreaID, const PhysicalPlayerIndex playerID);

	//PURPOSE
	// Checks if this area is occupied on any machine
	static bool IsOccupied(const int scriptAreaID);

	//PURPOSE
	// Marks whether this area is occupied or not in a player's world view. Only called when changed.
	//PARAMS
	// playerIDD - Player ID to check occupied state on
	static bool IsOccupiedOn(const int scriptAreaID, const PhysicalPlayerIndex playerID);

	//PURPOSE
	// Returns the name of this world state data
	const char *GetName() const { return "ENTITY_AREA"; };

	//PURPOSE
	// Logs the world state data
	//PARAMS
	// log - log file to use
	void LogState(netLoggingInterface &log) const;

	//PURPOSE
	// Reads the contents of an instance of this class via a read serialiser
	//PARAMS
	// serialiser - The serialiser to use
	void Serialise(CSyncDataReader &serialiser);

	//PURPOSE
	// Write the contents of this instance of the class via a write serialiser
	//PARAMS
	// serialiser - The serialiser to use
	void Serialise(CSyncDataWriter &serialiser);

	//PURPOSE
	// Called after the world state has been serialised
	void PostSerialise();

#if __DEV
	//PURPOSE
	// Returns whether this world state data is conflicting with the specified world state data.
	// We don't allow the same area to be referenced with different parameters
	//PARAMS
	// worldState - The world state to compare
	virtual bool IsConflicting(const CNetworkWorldStateData &worldState) const;
#endif // __DEV

#if __BANK
	//PURPOSE
	// Adds debug widgets to test this world state
	static void AddDebugWidgets();

	//PURPOSE
	// Displays debug text describing this world state
	static void DisplayDebugText();
#endif // __BANK

private:

	void InitializeBoundingBox();

	//PURPOSE
	// Write the contents of this instance of the class via a write serialiser
	//PARAMS
	// serialiser - The serialiser to use
	template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

	static int ms_NextNetworkID; // next network ID to allocate (synced)
	static int ms_NextScriptID; // next script ID to allocate (local only)

	Matrix34 m_boxMatrix;
	Vector3  m_boxSize;

	Vector3      m_AreaStart;     // point indicating start of requested area
	Vector3      m_AreaEnd;       // point indicating end of requested area
	float        m_AreaWidth;     // the width of the area
	PlayerFlags  m_IsOccupied;    // occupied state per player
	PlayerFlags  m_HasReplyFrom;  // has reply per player
	int          m_NetworkID;     // network ID for this rope (synced to other machines)
	IncludeFlags m_nIncludeFlags; // what entities to include in our checks

	int  m_ScriptAreaID;     // local variable indicating index from script
	u32  m_uLastCheckTimeMs; // the last time check for this area - previously was static but that was only working when you had a single area world state
	bool m_bIsAngledArea;    // whether the area is axis aligned or angled.
	bool m_bIsOccupied;      // local variable indicating state
	bool m_bIsDirty;         // local variable indicating that we should check this area
    bool m_bClientCreated;   // indicates whether this world state was created by a client (non-host player) of the script creating it
    u64  m_ClientPeerID;     // the peer ID of the client that created this world state

public:

#if __BANK
	static bool gs_bDrawEntityAreas;
#endif
};

#endif  // NETWORK_ENTITY_AREA_WORLD_STATE_H
