//
// name:        BoatSyncNodes.h
// description: Network sync nodes used by CNetObjBoats
// written by:    John Gurney
//

#ifndef BOAT_SYNC_NODES_H
#define BOAT_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

class CBoatGameStateDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IBoatNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the boat nodes.
// Any class that wishes to send/receive data contained in the boat nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IBoatNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IBoatNodeDataAccessor);

    // getter functions
    virtual void GetBoatGameState(CBoatGameStateDataNode& data) = 0;

    // setter functions
    virtual void SetBoatGameState(const CBoatGameStateDataNode& data) = 0;

protected:

    virtual ~IBoatNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CBoatGameStateDataNode
//
// Handles the serialization of a boat's game state
///////////////////////////////////////////////////////////////////////////////////////
class CBoatGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Boat Game State", CBoatGameStateDataNode, IBoatNodeDataAccessor);

private:

    void ExtractDataForSerialising(IBoatNodeDataAccessor &boatNodeData)
    {
        boatNodeData.GetBoatGameState(*this);
    }

    void ApplyDataFromSerialising(IBoatNodeDataAccessor &boatNodeData)
    {
        boatNodeData.SetBoatGameState(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const; 
	virtual void SetDefaultState();

public:

	float m_AnchorLodDistance;					// anchor buoyancy lod distance
	u32   m_boatWreckedAction;					// what action does this boat take when it is wrecked?
	bool  m_lockedToXY;							// is this boat locked in the XY plane (anchored)
    bool  m_ForceLowLodMode;					// force the low lod mode for the boat
	bool  m_UseWidestToleranceBoundingBoxTest;	// should we be considering a higher tolerance on the test for being near a river?
	bool  m_interiorLightOn;					// if the interior light is allowed to be on/off - default on
	float m_BuoyancyForceMultiplier;            // shows us how much the boat wants to float back up. 0 when the boat is sinking the fastest.
};

#endif  // BOAT_SYNC_NODES_H
