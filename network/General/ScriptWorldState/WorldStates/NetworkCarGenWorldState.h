//
// name:		NetworkCarGenWorldState.h
// description:	Support for network scripts to switch car generators off via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_CAR_GEN_WORLD_STATE_H
#define NETWORK_CAR_GEN_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a region of the map in which road nodes have been disabled by a script
class CNetworkCarGenWorldStateData : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkCarGenWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkCarGenWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID     - The ID of the script that has disabled the car generators
    // locallyOwned - Indicates whether these generators have been disabled by the local machine
    // regionMin    - Minimum values of the region bounding box
    // regionMax    - Maximum values of the region bounding box
    CNetworkCarGenWorldStateData(const CGameScriptId &scriptID,
                                 bool                 locallyOwned,
                                 const Vector3       &regionMin,
                                 const Vector3       &regionMax);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_CAR_GEN; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkCarGenWorldStateData *Clone() const;

    //PURPOSE
    // Returns the bounding box region of disabled car generators
    //PARAMS
    // min - Minimum values of the region bounding box
    // max - Maximum values of the region bounding box
    void          GetRegion(Vector3 &min, 
                            Vector3 &max) const;

    //PURPOSE   
    // Returns whether the specified region of disabled car generators
    // disabled by a script is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Switches off the car generators within the region stored in this instance
    void ChangeWorldState();

    //PURPOSE
    // Switches on the car generators within the region stored in this instance
    void RevertWorldState();

    //PURPOSE
    // Switches on the car generators in the specified region.
    // If this is called locally, the generators will already have
    // been enabled. If called based on a network update,
    // this function enables the generators.
    //PARAMS
    // scriptID    - The ID of the script that has enabled the road nodes
    // regionMin   - Minimum values of the region bounding box
    // regionMax   - Maximum values of the region bounding box
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOnCarGenerators(const CGameScriptId &scriptID,
                                      const Vector3       &regionMin,
                                      const Vector3       &regionMax,
                                      bool                 fromNetwork);

    //PURPOSE
    // Switches off the car generators in the specified region.
    // If this is called locally, the generators will already have
    // been disabled. If called based on a network update,
    // this function disables the generators.
    //PARAMS
    // scriptID    - The ID of the script that has disabled the road nodes
    // regionMin   - Minimum values of the region bounding box
    // regionMax   - Maximum values of the region bounding box
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOffCarGenerators(const CGameScriptId &scriptID,
                                       const Vector3       &regionMin,
                                       const Vector3       &regionMax,
                                       bool                 fromNetwork);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "CAR_GENERATORS"; };

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

#if __DEV
    //PURPOSE
    // Returns whether this world state data is conflicting with the specified world state data.
    // We don't allow bounding boxes of disabled car generators to overlap otherwise when one
    // world state is reverted it would enable car generators still within the bounding box of
    // another potentially active world state
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
	static const unsigned MINMAX_POSITION_PRECISION = 20; // Need higher precision for these positions since they get bounced between players

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

    Vector3 m_NodeRegionMin; // Minimum values of the region bounding box
    Vector3 m_NodeRegionMax; // Maximum values of the region bounding box
};

#endif  // NETWORK_CAR_GEN_WORLD_STATE_H
