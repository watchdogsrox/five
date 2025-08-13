//
// name:        BikeSyncNodes.h
// description: Network sync nodes used by CNetObjBikes
// written by:    John Gurney
//

#ifndef BIKE_SYNC_NODES_H
#define BIKE_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

///////////////////////////////////////////////////////////////////////////////////////
// IBikeNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the bike nodes.
// Any class that wishes to send/receive data contained in the bike nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IBikeNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IBikeNodeDataAccessor);

    // getter functions
    virtual void GetBikeGameState(bool &isOnSideStand) = 0;

    // setter functions
    virtual void SetBikeGameState(const bool isOnSideStand) = 0;

protected:

    virtual ~IBikeNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CBikeGameStateDataNode
//
// Handles the serialization of a bike's game state
///////////////////////////////////////////////////////////////////////////////////////
class CBikeGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Bike Game State", CBikeGameStateDataNode, IBikeNodeDataAccessor);

private:

    void ExtractDataForSerialising(IBikeNodeDataAccessor &bikeNodeData)
    {
        bikeNodeData.GetBikeGameState(m_onSideStand);
    }

    void ApplyDataFromSerialising(IBikeNodeDataAccessor &bikeNodeData)
    {
        bikeNodeData.SetBikeGameState(m_onSideStand);
    }

	void Serialise(CSyncDataBase& serialiser)
    {
        SERIALISE_BOOL(serialiser, m_onSideStand, "On Side Stand");
    }

private:

    bool m_onSideStand; // is the bike on it's side stand?
};

#endif  // BIKE_SYNC_NODES_H
