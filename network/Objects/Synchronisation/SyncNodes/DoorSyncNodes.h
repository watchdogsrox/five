//
// name:        DoorSyncNodes.h
// description: Network sync nodes used by CNetObjDoor
//

#ifndef DOOR_SYNC_NODES_H
#define DOOR_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/PhysicalSyncNodes.h"

class CDoorCreationDataNode;
class CDoorScriptInfoDataNode;
class CDoorScriptGameStateDataNode;
class CDoorMovementDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IDoorNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the Door nodes.
// Any class that wishes to send/receive data contained in the object nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IDoorNodeDataAccessor : public netINodeDataAccessor
{
public:

	DATA_ACCESSOR_ID_DECL(IDoorNodeDataAccessor);

	// sync parser getter functions
	virtual void GetDoorCreateData(CDoorCreationDataNode& data) const = 0;
	virtual void GetDoorScriptInfo(CDoorScriptInfoDataNode& data) const = 0;
	virtual void GetDoorScriptGameState(CDoorScriptGameStateDataNode& data) const = 0;
	virtual void GetDoorMovementData(CDoorMovementDataNode& data) const = 0;

	// sync parser setter functions
	virtual void SetDoorCreateData(const CDoorCreationDataNode& data) = 0;
	virtual void SetDoorScriptInfo(CDoorScriptInfoDataNode& data) = 0;
	virtual void SetDoorScriptGameState(const CDoorScriptGameStateDataNode& data) = 0;
	virtual void SetDoorMovementData(const CDoorMovementDataNode& data) = 0;

protected:

	virtual ~IDoorNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CDoorCreationDataNode
//
// Handles the serialization of an door's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CDoorCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Door Creation", CDoorCreationDataNode, IDoorNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

	virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

	void ExtractDataForSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.GetDoorCreateData(*this);
	}

	void ApplyDataFromSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.SetDoorCreateData(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

public:

	u32					m_ModelHash;			// Hash of the model name for the door
	Vector3				m_Position;				// Position of the door on the map
	mutable CDoor*		m_pDoorObject;			// the door object that exists at the dummy position
	bool				m_scriptDoor;			// is this door registered as a script door?
	bool				m_playerWantsControl;	// does the creating player want control of this object
};

///////////////////////////////////////////////////////////////////////////////////////
// CDoorScriptInfoDataNode
//
// Handles the serialization of an door's script info.
///////////////////////////////////////////////////////////////////////////////////////
class CDoorScriptInfoDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Door Script Info", CDoorScriptInfoDataNode, IDoorNodeDataAccessor);

	virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

	bool IsAlwaysSentWithCreate() const { return true; }

private:

	void ExtractDataForSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.GetDoorScriptInfo(*this);
	}

	void ApplyDataFromSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.SetDoorScriptInfo(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	bool				m_hasScriptInfo;
	CGameScriptObjInfo	m_scriptInfo;
	u32					m_doorSystemHash;		 // the hash identifying the door in the door system
	bool				m_existingScriptDoor;    // if true, the door system entry for this door should already exist non-networked, otherwise a new one needs to be created for this door
};

///////////////////////////////////////////////////////////////////////////////////////
// CDoorScriptGameStateDataNode
//
// Handles the serialization of a doors script game state
///////////////////////////////////////////////////////////////////////////////////////
class CDoorScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Door Script Game State", CDoorScriptGameStateDataNode, IDoorNodeDataAccessor);

	bool IsAlwaysSentWithCreate() const { return true; }

private:

	void ExtractDataForSerialising(IDoorNodeDataAccessor &doorNodeData);

	void ApplyDataFromSerialising(IDoorNodeDataAccessor &doorNodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

	u32		m_DoorSystemState;	// the state held in the door system
	float	m_AutomaticDist;	// distance at which an automatic sliding door or barrier opens, or 0.0f for default
	float   m_AutomaticRate;	// rate an automatic sliding door or barrier moves, uses default value for door type if this is 0.0f
	bool    m_Broken;			// if true the door is fragmented
	u32		m_BrokenFlags;		// flags specifying which door fragments are broken
	bool	m_Damaged;			// true means any component is damaged out
	u32		m_DamagedFlags;		// flags indicating components that have damaged out
	bool	m_HoldOpen;			// true means the door is held open

};

///////////////////////////////////////////////////////////////////////////////////////
// CDoorMovementDataNode
//
// Handles the serialization of a doors movement data
///////////////////////////////////////////////////////////////////////////////////////
class CDoorMovementDataNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Door Movement", CDoorMovementDataNode, IDoorNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const;

private:

	void ExtractDataForSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.GetDoorMovementData(*this);
	}

	void ApplyDataFromSerialising(IDoorNodeDataAccessor &doorNodeData)
	{
		doorNodeData.SetDoorMovementData(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	bool	m_bHasOpenRatio;
	float	m_fOpenRatio;
	bool	m_bOpening;
	bool	m_bFullyOpen;
	bool	m_bClosed;
};

#endif  // DOOR_SYNC_NODES_H
