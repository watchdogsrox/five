//
// name:		NetworkPopGroupOverrideWorldState.h
// description:	Support for network scripts to override pop group percentages via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_POP_GROUP_OVERRIDE_WORLD_STATE_H
#define NETWORK_POP_GROUP_OVERRIDE_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a pop schedule that has been had it's population group percentages overridden
class CNetworkPopGroupOverrideWorldStateData : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkPopGroupOverrideWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkPopGroupOverrideWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID     - The ID of the script that has disabled the car generators
    // locallyOwned - Indicates whether these generators have been disabled by the local machine
    // popSchedule  - The pop schedule being overridden
    // popGroupHash - The hash of the population group in the schedule that is being given an overridden percentage
    // percentage   - The percentage to use for the population group
    CNetworkPopGroupOverrideWorldStateData(const CGameScriptId &scriptID,
                                           bool                 locallyOwned,
                                           int                  popSchedule,
                                           u32                  popGroupHash,
                                           u32                  percentage);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_POP_GROUP_OVERRIDE; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkPopGroupOverrideWorldStateData *Clone() const;

    //PURPOSE   
    // Returns whether the specified override data is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Applies the overridden pop group percentage to the pop schedule
    void ChangeWorldState();

    //PURPOSE
    // Removes the overridden pop group percentage from the pop schedule
    void RevertWorldState();

    //PURPOSE
    // Applies the overridden pop group percentage to the specified pop schedule.
    //PARAMS
    // scriptID    - The ID of the script that has enabled the road nodes
    // popSchedule  - The pop schedule being overridden
    // popGroupHash - The hash of the population group in the schedule that is being given an overridden percentage
    // percentage   - The percentage to use for the population group
    static void OverridePopGroupPercentage(const CGameScriptId &scriptID,
                                           int                  popSchedule,
                                           u32                  popGroupHash,
                                           u32                  percentage);

    //PURPOSE
    // Removes the overridden pop group percentage from the specified pop schedule.
    //PARAMS
    // scriptID    - The ID of the script that has enabled the road nodes
    // popSchedule  - The pop schedule being overridden
    // popGroupHash - The hash of the population group in the schedule that is being given an overridden percentage
    // percentage   - The percentage to use for the population group
    static void RemovePopGroupPercentageOverride(const CGameScriptId &scriptID,
                                                 int                  popSchedule,
                                                 u32                  popGroupHash,
                                                 u32                  percentage);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "POP_GROUP_OVERRIDE"; };

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
    // We don't allow more than one population group to be overridden per population schedule index
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

    int m_PopSchedule;  // Pop schedule to override
    u32 m_PopGroupHash; // Population group to override the percentage for
    u32 m_Percentage;   // New percentage to use for the pop group
};

#endif  // NETWORK_POP_GROUP_OVERRIDE_WORLD_STATE_H
