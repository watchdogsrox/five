//
// name:        AutomobileSyncNodes.h
// description: Network sync nodes used by CNetObjAutomobiles
// written by:    John Gurney
//

#ifndef AUTOMOBILE_SYNC_NODES_H
#define AUTOMOBILE_SYNC_NODES_H

#include "fwmaths/Angle.h"
#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

class CAutomobileCreationDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IAutomobileNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the automobile nodes.
// Any class that wishes to send/receive data contained in the automobile nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IAutomobileNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IAutomobileNodeDataAccessor);

    // sync parser setter functions
    virtual void GetAutomobileCreateData(CAutomobileCreationDataNode& data) = 0;

    // sync parser setter functions
    virtual void SetAutomobileCreateData(const CAutomobileCreationDataNode& data) = 0;

protected:

    virtual ~IAutomobileNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CAutomobileCreationDataNode
//
// Handles the serialization of an automobile's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CAutomobileCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Automobile Creation", CAutomobileCreationDataNode, IAutomobileNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IAutomobileNodeDataAccessor &automobileNodeData)
    {
        automobileNodeData.GetAutomobileCreateData(*this);
    }

    void ApplyDataFromSerialising(IAutomobileNodeDataAccessor &automobileNodeData)
    {
		automobileNodeData.SetAutomobileCreateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    bool m_allDoorsClosed;
    bool m_doorsClosed[NUM_VEH_DOORS_MAX];
};

#endif  // AUTOMOBILE_SYNC_NODES_H
