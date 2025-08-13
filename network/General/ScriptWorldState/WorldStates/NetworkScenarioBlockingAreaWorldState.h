//
// name:		NetworkScenarioBlockingAreaWorldState.h
// description:	Support for network scripts to set up scenario blocking areas via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_SCENARIO_BLOCKING_AREA_WORLD_STATE_H
#define NETWORK_SCENARIO_BLOCKING_AREA_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a region of the map in which scenarios have been blocked by a script
class CNetworkScenarioBlockingAreaWorldStateData : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkScenarioBlockingAreaWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkScenarioBlockingAreaWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID       - The ID of the script that has blocked scenarios in the area
    // locallyOwned   - Indicates whether these scenarios have been blocked by the local machine
    // regionMin      - Minimum values of the region bounding box
    // regionMax      - Maximum values of the region bounding box
    // blockingAreaID - The scenario blocking area ID
	// bCancelActive  - Whether this blocking area causes peds already actively using scenarios in the bounds to exit.
	// bBlocksPeds    - True if it blocks peds.
	// bBlocksVehicles - True if it blocks vehicles.
    CNetworkScenarioBlockingAreaWorldStateData(const CGameScriptId &scriptID,
                                               bool                 locallyOwned,
                                               const Vector3       &regionMin,
                                               const Vector3       &regionMax,
                                               const int            blockingAreaID,
											   bool					bCancelActive,
											   bool					bBlocksPeds,
											   bool					bBlocksVehicles);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_SCENARIO_BLOCKING_AREA; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkScenarioBlockingAreaWorldStateData *Clone() const;

    //PURPOSE
    // Returns the bounding box region of disabled car generators
    //PARAMS
    // min - Minimum values of the region bounding box
    // max - Maximum values of the region bounding box
    void          GetRegion(Vector3 &min, 
                            Vector3 &max) const;

    //PURPOSE   
    // Returns whether the specified scenario blocking area
    // is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Blocks scenarios within the region stored in this instance
    void ChangeWorldState();

    //PURPOSE
    // Removes the block on scenarios within the region stored in this instance
    void RevertWorldState();

    //PURPOSE
    // Blocks scenarios in the specified region.
    // If this is called locally, the region will already have
    // been blocked. If called based on a network update,
    // this function adds the blocking region.
    //PARAMS
    // scriptID       - The ID of the script that has blocked the scenarios
    // regionMin      - Minimum values of the region bounding box
    // regionMax      - Maximum values of the region bounding box
    // blockingAreaID - The scenario blocking area ID
	// bBlocksPeds    - True if it blocks peds.
	// bBlocksVehicles - True if it blocks vehicles.
    static void AddScenarioBlockingArea(const CGameScriptId &scriptID,
                                        const Vector3       &regionMin,
                                        const Vector3       &regionMax,
                                        const int            blockingAreaID,
										bool				bCancelActive,
										bool				bBlocksPeds,
										bool				bBlocksVehicles);

    //PURPOSE
    // Removes the scenario blocking area.
    // If this is called locally, the area will already have
    // been removed. If called based on a network update,
    // this function removes the blocking region.
    //PARAMS
    // scriptID       - The ID of the script that has disabled the road nodes
    // blockingAreaID - The scenario blocking area ID
    static void RemoveScenarioBlockingArea(const CGameScriptId &scriptID,
                                           const int            blockingAreaID);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "SCENARIO_BLOCKING_AREA"; };

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
    // We don't allow bounding boxes of scenario blocking areas to overlap otherwise when one
    // world state is reverted it would unblock areas still within the bounding box of
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

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

    Vector3 m_NodeRegionMin;  // Minimum values of the region bounding box
    Vector3 m_NodeRegionMax;  // Maximum values of the region bounding box
    int     m_BlockingAreaID; // ID of a scenario blocking area added by this world state
	bool	m_bCancelActive;  // Whether this scenario blocking area causes peds already using the point to leave when set.
	bool	m_bBlocksPeds;    // True if it blocks peds.
	bool	m_bBlocksVehicles;// True if it blocks vehicles.
};

#endif  // NETWORK_SCENARIO_BLOCKING_AREA_WORLD_STATE_H
