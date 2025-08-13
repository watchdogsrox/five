//
// name:		NetworkScriptWorldStateManager.h
// description:	The script world state manager supports state changes to the game world made
//              by the host a script in the network game, ensuring these changes are applied to
//              newly joining changes and persist when the host leaves the session. The state changes
//              will also be reverted once the script terminates on all machines. This is useful for
//              world state that needs to be synchronised on all machines, regardless of which players
//              are taking part in the script.
// written by:	Daniel Yelland
//

#ifndef NETWORK_SCRIPT_WORLD_STATE_MANAGER_H
#define NETWORK_SCRIPT_WORLD_STATE_MANAGER_H

#include "script/handlers/GameScriptIds.h"

namespace rage
{
    class netLoggingInterface;
    class netPlayer;
}

//PURPOSE
// Base class for world state data that can be changed by the host of a script
class CNetworkWorldStateData
{
public:

    //PURPOSE
    // Class constructor
    CNetworkWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID     - The ID of the script that has changed the world state
    // locallyOwned - Indicates whether world state has been changed by the local machine
    CNetworkWorldStateData(const CGameScriptId &scriptID,
                           bool                 locallyOwned);

    //PURPOSE
    // Class destructor
    virtual ~CNetworkWorldStateData() {}

    CGameScriptId       &GetScriptID()          { return m_ScriptID;     }
    const CGameScriptId &GetScriptID()    const { return m_ScriptID;     }
    bool                 IsLocallyOwned() const { return m_LocallyOwned; }
    unsigned             GetTimeCreated() const { return m_TimeCreated;  }
    
	//PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const = 0;

    //PURPOSE
    // Per frame update call
    virtual void Update() {}

    //PURPOSE
    // Indicates whether world states of a given type can be updated after
    // they are created. The default behaviour is to support just creating/reverting
    // world state
    virtual bool CanBeUpdated() const { return false; }

    //PURPOSE
    // Indicates whether this world state has expired, allows world state that has become no
    // longer relevant to clean up automatically
    virtual bool HasExpired() const { return false; }

	//PURPOSE
	// Called when a new player joins the network session
	//PARAMS
	// player - The player joining the session
	virtual void PlayerHasJoined(const netPlayer&) {}

	//PURPOSE
	// Called when a player leaves the network session
	//PARAMS
	// player - The player leaving the session
	virtual void PlayerHasLeft(const netPlayer&) {}

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    virtual CNetworkWorldStateData *Clone() const = 0;

    //PURPOSE
    // Returns whether this world state data is equal to the specified world state data
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const = 0;

    //PURPOSE
    // Changes the world state specified by the host of the script
    virtual void ChangeWorldState() = 0;

    //PURPOSE
    // Revert the world state specified by the host of the script
    virtual void RevertWorldState() = 0;

    //PURPOSE
    // When a player that is not the host of a network script changes some world state,
    // they have to inform the server. When the server approves and sends the new world state
    // the client may need to patch some local information into the world state stored by the manager.
    // An example of this is world state that stores a local handle to an object as well as a network handle.
    // Once the world state is added to the system the handle will need updating.
    //PARAMS
    // hostWorldState - The world state created in response to the client request
    virtual void UpdateHostState(CNetworkWorldStateData &UNUSED_PARAM(hostWorldState)) {}

    //PURPOSE
    // Sets whether the local machine owns the data in this instance. This
    // can change when the host of a network script migrates to the local machine
    // after the original host leaves
    void SetLocallyOwned(bool locallyOwned) { m_LocallyOwned = locallyOwned; } 

    //PURPOSE
    // Returns the name of this world state data
    virtual const char *GetName() const = 0;

    //PURPOSE
    // Logs the world state data
    //PARAMS
    // log - log file to use
    virtual void LogState(netLoggingInterface &log) const = 0;

    //PURPOSE
    // Reads the contents of an instance of this class via a read serialiser
    //PARAMS
    // serialiser - The serialiser to use
    virtual void Serialise(CSyncDataReader &serialiser) = 0;

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    virtual void Serialise(CSyncDataWriter &serialiser) = 0;

    //PURPOSE
    // Reads update data for an instance of this class via a read serialiser
    //PARAMS
    // serialiser - The serialiser to use
    virtual void SerialiseUpdate(CSyncDataReader &UNUSED_PARAM(serialiser)) {}

    //PURPOSE
    // Write update data for an instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    virtual void SerialiseUpdate(CSyncDataWriter &UNUSED_PARAM(serialiser)) {}

    //PURPOSE
    // Called after the world state has been serialised
    virtual void PostSerialise() {}

#if __DEV
    //PURPOSE
    // Returns whether this world state data is conflicting with the specified world state data.
    // This is for debug purposes to catch conflicting world states being added that relate to
    // the same world state (e.g. two world states referring to locking/unlocking the same door)
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsConflicting(const CNetworkWorldStateData &worldState) const = 0;
#endif // __DEV

private:

    CGameScriptId m_ScriptID;       // The ID of the script that has changed the world state
    bool          m_LocallyOwned;   // Indicates whether this world state has been changed by the local machine
    unsigned      m_TimeCreated;    // System time this world state was created, used to prevent world states from being removed immediately after being added
};

//PURPOSE
// Manager class for handling world state changes by scripts in a network game.
// This data is synchronised to the other machines in the session to ensure the
// world state is consistent across machines even if they are not running
// the script that has changed it.
class CNetworkScriptWorldStateMgr
{
public:

    //PURPOSE
    // Initialises the manager at the start of the network game
    static void Init();

    //PURPOSE
    // Shuts down the manager at the end of the network game
    static void Shutdown();

    //PURPOSE
    // Updates the manager. This should be called once per frame when the network game is running
    static void Update();

    //PURPOSE
    // Called when a new player joins the network session
    //PARAMS
    // player - The player joining the session
    static void PlayerHasJoined(const netPlayer& player);

    //PURPOSE
    // Called when a player leaves the network session
    //PARAMS
    // player - The player leaving the session
    static void PlayerHasLeft(const netPlayer& player);

    //PURPOSE
    // Changes the specified world state.
    // If this is called locally, the state will already have
    // been changed. If called based on a network update,
    // this function changes the state.
    //PARAMS
    // worldStateData - The world state to change
    // fromNetwork    - Indicates whether this call has originated from a network update
    static bool ChangeWorldState(CNetworkWorldStateData &worldStateData,
                                 bool                    fromNetwork);

    //PURPOSE
    // Reverts the specified world state.
    // If this is called locally, the state will already have
    // been revert. If called based on a network update,
    // this function reverts the state.
    //PARAMS
    // worldStateData - The world state to revert
    // fromNetwork    - Indicates whether this call has originated from a network update
    static void RevertWorldState(CNetworkWorldStateData &worldStateData,
                                 bool                    fromNetwork);

private:

    //PURPOSE
    // Stores world state changed by a non-host player while a host migration
    // is in progress (when there is no host player to send the request to
    struct PendingWorldStateChange
    {
        PendingWorldStateChange() : m_WorldState(0), m_Change(false) {}

        void Reset()
        {
            if(m_WorldState)
            {
                delete m_WorldState;
                m_WorldState = 0;
            }

            m_Change = false;
        }

        CNetworkWorldStateData *m_WorldState; // world state to be changed/reverted
        bool                    m_Change;     // indicates whether this world state is be changed or reverted
    };

    //PURPOSE
    // Checks the list of world states pending a host response for a matching world state. This is necessary
    // as the world state will already have been changed on the local machine, but cannot be added until the
    // determined by the host
    //PARAMS
    // hostWorldStateData - The world state received from the host of the script, for comparing against
    //                      entries in the pending host response list
    static CNetworkWorldStateData *FindDataPendingResponse(const CNetworkWorldStateData &hostWorldState);

    //PURPOSE
    // Adds some world state changed by a non-host player to a list until a response is received from the host
    //PARAMS
    // worldStateData - The world state data to change (a copy will be made to add to the list)
    static void AddToHostResponsePendingList(const CNetworkWorldStateData &worldStateData);

    //PURPOSE
    // Adds some world state changed by a non-host player while the script host is migrating
    // for sending when the new host is determined
    //PARAMS
    // worldStateData - The world state data to change
    // change         - Indicates whether the world state is being changed or reverted
    static void AddToHostMigrationPendingList(CNetworkWorldStateData &worldStateData, bool change);

#if __DEV
    //PURPOSE
    // Checks whether the specified world state conflicts with existing world state
    // changes within the system. Used for detecting conflicts between script requests.
    //PARAMS
    // worldStateData - The world state data to check for conflicts
    static void CheckForConflictingWorldState(const CNetworkWorldStateData &worldStateData);
#endif // __DEV

    static const unsigned MAX_WORLD_STATE_DATA = 128;
    static CNetworkWorldStateData *ms_WorldStateData[MAX_WORLD_STATE_DATA];
    static CNetworkWorldStateData *ms_PendingHostResponseData[MAX_WORLD_STATE_DATA];
    static PendingWorldStateChange ms_PendingHostMigrationData[MAX_WORLD_STATE_DATA];
};

#endif  // NETWORK_SCRIPT_WORLD_STATE_MANAGER_H
