//
// name:        PickupPlacementSyncNodes.h
// description: Network sync nodes used by CNetObjPickupPlacements
// written by:    John Gurney
//

#ifndef PICKUP_PLACEMENT_SYNC_NODES_H
#define PICKUP_PLACEMENT_SYNC_NODES_H

#include "pickups/Data/PickupIds.h"
#include "script/Handlers/GameScriptIds.h"
#include "Network/Objects/Synchronisation/SyncNodes/BaseSyncNodes.h"

//framework includes
#include "fwscript/scriptid.h" // for USE_SCRIPT_NAME

class CPickupPlacementCreationDataNode;
class CPickupPlacementStateDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IPickupNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the pickup nodes.
// Any class that wishes to send/receive data contained in the object nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPickupPlacementNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IPickupPlacementNodeDataAccessor);

    // sync parser getter functions
    virtual void GetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data) const = 0;
    virtual void GetPickupPlacementStateData(CPickupPlacementStateDataNode& data) const = 0;

    // sync parser setter functions
    virtual void SetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data) = 0;
    virtual void SetPickupPlacementStateData(const CPickupPlacementStateDataNode& data) = 0;

protected:

    virtual ~IPickupPlacementNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPickupPlacementCreationDataNode
//
// Handles the serialization of an pickup's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CPickupPlacementCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Pickup Placement Creation", CPickupPlacementCreationDataNode, IPickupPlacementNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData);

    void ApplyDataFromSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned int SIZEOF_TEAM_PERMITS;

    bool                    m_mapPlacement;         // indicates whether this is a map placement or not
    Vector3                 m_pickupPosition;       // the pickup position
    Vector3                 m_pickupOrientation;    // the pickup orientation in eulers
    u32                     m_pickupHash;           // the hash of the pickup type
    u32                     m_placementFlags;       // the placement flags
	u32						m_amount;				// a variable amount used by some pickup types (eg money).
	u32						m_customModelHash;		// a custom model, if specified by script
	u32						m_customRegenTime;		// a custom regeneration time, if specified by script
	u32						m_teamPermits;			// which teams are allowed to collect this pickup
    CGameScriptObjInfo      m_scriptObjInfo;        // the script obj info for the placement
};

///////////////////////////////////////////////////////////////////////////////////////
// CPickupPlacementStateDataNode
//
// Handles the serialization of an placement's state.
///////////////////////////////////////////////////////////////////////////////////////
class CPickupPlacementStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Pickup Placement State", CPickupPlacementStateDataNode, IPickupPlacementNodeDataAccessor);

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData)
    {
        pickupNodeData.GetPickupPlacementStateData(*this);
    }

    void ApplyDataFromSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData)
    {
        pickupNodeData.SetPickupPlacementStateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

	bool     m_collected;                   // has this pickup been collected?
	bool     m_destroyed;                   // has this pickup been destroyed?
	ObjectId m_collector;					// object ID of the ped who collected the pickup
	bool	 m_regenerates;
	u32		 m_regenerationTime;            // the time at which the placement regenerates its pickup
};

#endif  // PICKUP_PLACEMENT_SYNC_NODES_H
