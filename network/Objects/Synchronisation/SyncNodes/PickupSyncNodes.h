//
// name:        PickupSyncNodes.h
// description: Network sync nodes used by CNetObjPickups
// written by:    John Gurney
//

#ifndef PICKUP_SYNC_NODES_H
#define PICKUP_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/PedSyncNodes.h"
#include "network/objects/synchronisation/SyncNodes/PhysicalSyncNodes.h"

#include "pickups/Data/PickupIds.h"

class CObject;
class CPickupCreationDataNode;
class CPickupSectorPosNode;
class CPickupScriptGameStateNode;

///////////////////////////////////////////////////////////////////////////////////////
// IPickupNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the pickup nodes.
// Any class that wishes to send/receive data contained in the object nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPickupNodeDataAccessor : public netINodeDataAccessor
{
public:

	DATA_ACCESSOR_ID_DECL(IPickupNodeDataAccessor);

	// sync parser getter functions
    virtual void GetPickupCreateData(CPickupCreationDataNode& data) const = 0;
	virtual void GetPickupSectorPosData(CPickupSectorPosNode& data) const = 0;
	virtual void GetPickupScriptGameStateData(CPickupScriptGameStateNode& data) const = 0;

	// sync parser setter functions
	virtual void SetPickupCreateData(const CPickupCreationDataNode& data) = 0;
	virtual void SetPickupSectorPosData(const CPickupSectorPosNode& data) = 0;
	virtual void SetPickupScriptGameStateData(const CPickupScriptGameStateNode& data) = 0;

protected:

    virtual ~IPickupNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPickupCreationDataNode
//
// Handles the serialization of an pickup's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CPickupCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Pickup Creation", CPickupCreationDataNode, IPickupNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

	virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IPickupNodeDataAccessor &pickupNodeData);

    void ApplyDataFromSerialising(IPickupNodeDataAccessor &pickupNodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

	bool					m_bHasPlacement;		// set if the network object has a corresponding CPickup object
	CGameScriptObjInfo		m_placementInfo;		// the script info of the pickup placement
	u32						m_pickupHash;			// the type of the pickup
	u32					    m_amount;				// the amount held in the pickup (only used for money pickups currently)
	u32						m_customModelHash;		// a custom model, if specified by script
	u32						m_lifeTime;				// how long the pickup has existed (only used for ambient pickups)
	u32						m_weaponComponents[IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS]; // for modded weapons dropped by players
	u32						m_numWeaponComponents;
	u32						m_weaponTintIndex;		// for modded weapons dropped by players
	bool					m_bPlayerGift;			// set if this is an ambient pickup dropped for another player to collect
	bool					m_bHasPlayersBlockingList; // Are there any blocked players for this pickup
	PlayerFlags				m_PlayersToBlockList;	// List of blocked players for this pickup
	u32						m_LODdistance;			// LOD distance of pickup
	bool                    m_includeProjectiles;  // Allow projectiles to collide with this pickup
};

///////////////////////////////////////////////////////////////////////////////////////
// CPickupSectorPosNode
//
// Handles the serialization of a CPickup's sector position. This syncs the position 
// with a higher precision than other entities.
///////////////////////////////////////////////////////////////////////////////////////
class CPickupSectorPosNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Pickup Sector Pos", CPickupSectorPosNode, IPickupNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

	bool IsAlwaysSentWithCreate() const { return true; }

private:

	void ExtractDataForSerialising(IPickupNodeDataAccessor &nodeData);

	void ApplyDataFromSerialising(IPickupNodeDataAccessor &nodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

#if __DEV
	static RecorderId ms_bandwidthRecorderId;
#endif

	float    m_SectorPosX;       // X position of this object within the current sector
	float    m_SectorPosY;       // Y position of this object within the current sector
	float    m_SectorPosZ;       // Z position of this object within the current sector
};

///////////////////////////////////////////////////////////////////////////////////////
// CPickupScriptGameStateNode
//
// Handles the serialization of a pickup's script game state data
///////////////////////////////////////////////////////////////////////////////////////
class CPickupScriptGameStateNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Pickup Script Game State", CPickupScriptGameStateNode, IPickupNodeDataAccessor);

private:

	void ExtractDataForSerialising(IPickupNodeDataAccessor &pickupNodeData);

	void ApplyDataFromSerialising(IPickupNodeDataAccessor &pickupNodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned int SIZEOF_TEAM_PERMITS;

	u32						m_flags;							// pickup flags
	bool					m_bFloating;						// used to unfix portable pickups
	u32						m_teamPermits;						// which teams are allowed to collect this pickup (only used for pickups without a placement)
	float					m_offsetGlow;						// some pickups have a script specified glow offset

	// portable pickup stuff:
	bool					m_portable;
	bool					m_inAccessible;						// used by portable pickups, indicating whether they are in an inaccessible location
	Vector3					m_lastAccessibleLoc;				// the last accessible location (used by portable pickups only)
	bool					m_lastAccessibleLocHasValidGround;	// used by portable pickups, indicating whether the last accessible location has valid ground
	bool					m_allowNonScriptParticipantCollect; // Allows non script participants to pick this up
};
#endif  // PICKUP_SYNC_NODES_H
