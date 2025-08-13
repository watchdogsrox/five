//
// name:        SubmarineSyncNodes.h
// description: Network sync nodes used by CNetObjSubmarine
// written by:  
//

#ifndef SUBMARINE_SYNC_NODES_H
#define SUBMARINE_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

class CSubmarineControlDataNode;
class CSubmarineGameStateDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// ISubmarineNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the Submarine nodes.
// Any class that wishes to send/receive data contained in the Submarine nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class ISubmarineNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(ISubmarineNodeDataAccessor);

	virtual void GetSubmarineControlData(CSubmarineControlDataNode& data) = 0;
	virtual void SetSubmarineControlData(const CSubmarineControlDataNode& data) = 0;
    virtual void GetSubmarineGameState(CSubmarineGameStateDataNode& data) = 0;
    virtual void SetSubmarineGameState(const CSubmarineGameStateDataNode& data) = 0;

protected:

    virtual ~ISubmarineNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CSubmarineGameStateDataNode
//
// Handles the serialization of a Submarine's game state
///////////////////////////////////////////////////////////////////////////////////////
class CSubmarineControlDataNode : public CVehicleControlDataNode
{
protected:

	NODE_COMMON_IMPL("Submarine Control Node", CSubmarineControlDataNode, ISubmarineNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(ISubmarineNodeDataAccessor &submarineNodeData)
    {
		submarineNodeData.GetSubmarineControlData(*this);
    }

    void ApplyDataFromSerialising(ISubmarineNodeDataAccessor &submarineNodeData)
    {
		submarineNodeData.SetSubmarineControlData(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	float m_yaw;   //the current value of the yaw control.
	float m_pitch; //the current value of the pitch control.
	float m_dive;  //the current value of the dive control.
};

///////////////////////////////////////////////////////////////////////////////////////
// CSubmarineGameStateDataNode
//
// Handles the serialization of a submarine's game state
///////////////////////////////////////////////////////////////////////////////////////
class CSubmarineGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Submarine Game State", CSubmarineGameStateDataNode, ISubmarineNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(ISubmarineNodeDataAccessor &submarineNodeData)
    {
        submarineNodeData.GetSubmarineGameState(*this);
    }

    void ApplyDataFromSerialising(ISubmarineNodeDataAccessor &submarineNodeData)
    {
        submarineNodeData.SetSubmarineGameState(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const; 
	virtual void SetDefaultState();

public:

    bool m_IsAnchored;        // is this submarine anchored?
};

#endif  //SUBMARINE_SYNC_NODES_H
