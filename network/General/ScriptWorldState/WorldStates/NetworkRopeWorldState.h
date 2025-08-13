//
// name:		NetworkRopeWorldState.h
// description:	Support for network scripts to create ropes via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_ROPE_WORLD_STATE_H
#define NETWORK_ROPE_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a rope which has been created by script
class CNetworkRopeWorldStateData : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkRopeWorldStateData);

    //PURPOSE
    // Structure containing parameters for the rope represented by this world state
    struct RopeParameters
    {
        RopeParameters() {}

        RopeParameters(const Vector3 &position,
                       const Vector3 &rotation,
                       float          maxLength,
                       int            type,
                       float          initLength,
                       float          minLength,
                       float          lengthChangeRate,
                       bool           ppuOnly,
                       bool           collisionOn,
                       bool           lockFromFront,
                       float          timeMultiplier,
                       bool           breakable,
					   bool			  pinned) :
        m_Position(position),
        m_Rotation(rotation),
        m_MaxLength(maxLength),
        m_Type(type),
        m_InitLength(initLength),
        m_MinLength(minLength),
        m_LengthChangeRate(lengthChangeRate),
        m_PpuOnly(ppuOnly),
        m_CollisionOn(collisionOn),
        m_LockFromFront(lockFromFront),
        m_TimeMultiplier(timeMultiplier),
        m_Breakable(breakable),
		m_Pinned(pinned)
        {
        }

        RopeParameters(const RopeParameters &other) :
        m_Position(other.m_Position),
        m_Rotation(other.m_Rotation),
        m_MaxLength(other.m_MaxLength),
        m_Type(other.m_Type),
        m_InitLength(other.m_InitLength),
        m_MinLength(other.m_MinLength),
        m_LengthChangeRate(other.m_LengthChangeRate),
        m_PpuOnly(other.m_PpuOnly),
        m_CollisionOn(other.m_CollisionOn),
        m_LockFromFront(other.m_LockFromFront),
        m_TimeMultiplier(other.m_TimeMultiplier),
        m_Breakable(other.m_Breakable),
		m_Pinned(other.m_Pinned)
        {
        }

        bool IsEqualTo(const RopeParameters &other) const
        {
            const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

            if(m_Position.IsClose(other.m_Position, epsilon) &&
               m_Rotation.IsClose(other.m_Rotation, epsilon) &&
               IsClose(m_MaxLength, other.m_MaxLength, epsilon) &&
               m_Type == other.m_Type &&
               IsClose(m_InitLength, other.m_InitLength, epsilon) &&
               IsClose(m_MinLength, other.m_MinLength, epsilon) &&
               IsClose(m_LengthChangeRate, other.m_LengthChangeRate, epsilon) &&
               m_PpuOnly == other.m_PpuOnly &&
               m_CollisionOn == other.m_CollisionOn &&
               m_LockFromFront == other.m_LockFromFront &&
               IsClose(m_TimeMultiplier, other.m_TimeMultiplier, epsilon) &&
               m_Breakable == other.m_Breakable &&
			   m_Pinned == other.m_Pinned
			  )
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        Vector3 m_Position;         // world position of the rope
        Vector3 m_Rotation;         // world rotation of the rope
        float   m_MaxLength;        // maximum length of the rope
        int     m_Type;             // type of rope
        float   m_InitLength;       // initial length
        float   m_MinLength;        // minimum length
        float   m_LengthChangeRate; // length change rate
        float   m_TimeMultiplier;   // time multiplier        
		bool    m_PpuOnly;          // should this rope be simulated on the PPU only?
		bool    m_CollisionOn;      // collision enabled?
		bool    m_LockFromFront;    // lock rope from the front?
		bool    m_Breakable;        // is the rope breakable
		bool	m_Pinned;			// is vertex 0 pinned on creation
    };

    //PURPOSE
    // Class constructor
    CNetworkRopeWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID      - The ID of the script that has created the rope
    // locallyOwned  - Indicates whether this rope has been created by the local machine
    // networkRopeID - The network ID of the rope created (synced across machines)
    // scriptRopeID  - The script ID of the rope created (not synced, used to call rope script commands)
    // parameters    - The parameters for creating the rope
    CNetworkRopeWorldStateData(const CGameScriptId  &scriptID,
                               bool                  locallyOwned,
                               int                   networkRopeID,
                               int                   scriptRopeID,
                               const RopeParameters &parameters);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_ROPE; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkRopeWorldStateData *Clone() const;

    //PURPOSE   
    // Returns whether the specified rope created by a script
    // is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Creates a rope using the data stored in this instance
    void ChangeWorldState();

    //PURPOSE
    // Deletes a rope created using the data stored in this instance
    void RevertWorldState();

    //PURPOSE
    // Updates newly created world state data with any local only info stored 
    // in this object instance
    //PARAMS
    // hostWorldState - The world state to update from after a host response
    void UpdateHostState(CNetworkWorldStateData &hostWorldState);

    //PURPOSE
    // Creates a network rope. If this is called locally, the rope
    // will already have been created. If called based on a network update,
    // this function creates the rope.
    //PARAMS
    // scriptID     - The ID of the script that has created the rope
    // scriptRopeID - The script ID of the created rope
    // parameters   - The parameters of the rope to create
    static void CreateRope(const CGameScriptId  &scriptID,
                           int                   scriptRopeID,
                           const RopeParameters &parameters);

    //PURPOSE
    // Deletes a network rope. If this is called locally, the rope
    // will already have been deleted. If called based on a network update,
    // this function deletes the rope.
    //PARAMS
    // scriptID     - The ID of the script that created the rope
    // scriptRopeID - The script ID of the created rope
    static void DeleteRope(const CGameScriptId  &scriptID,
                           int                   scriptRopeID);

    //PURPOSE
    // Returns the unique rope identifier for ropes created by network scripts
    // for the rope with the specified script ID
    //PARAMS
    // scriptRopeID - The script ID of the rope
    static int GetNetworkIDFromRopeID(int scriptRopeID);

    //PURPOSE
    // Returns script ID for ropes created by network scripts
    // for the rope with the specified network ID
    //PARAMS
    // networkRopeID - The network ID of the rope
    static int GetRopeIDFromNetworkID(int networkRopeID);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "ROPE"; };

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
    // Due to the nature of rope creation they are never marked as conflicting.
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsConflicting(const CNetworkWorldStateData &UNUSED_PARAM(worldState)) const { return false; }
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

    //PURPOSE
    // Serialises the contents of this instance of the class
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

    RopeParameters       m_RopeParameters; // parameters for creating the rope
    int                  m_NetworkRopeID;  // network ID for this rope (synced to other machines)
    int                  m_ScriptRopeID;   // ID of the locally created rope for this world state (this is not synced to other machines)

    static int           ms_NextNetworkID; // next network ID to allocate for script created ropes
};

#endif  // NETWORK_ROPE_WORLD_STATE_H
