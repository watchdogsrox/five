//
// name:        DynamicEntitySyncNodes.h
// description: Network sync nodes used by CNetObjDynamicEntitys
// written by:    John Gurney
//

#ifndef DYNAMIC_ENTITY_SYNC_NODES_H
#define DYNAMIC_ENTITY_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/EntitySyncNodes.h"

class CDynamicEntityGameStateDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IDynamicEntityNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the boat nodes.
// Any class that wishes to send/receive data contained in the boat nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IDynamicEntityNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IDynamicEntityNodeDataAccessor);

	// Decorator sync
	struct decEnt
	{
		u32	Type;
		u32	Key;
		union
		{	
			float   Float;
			bool	Bool;
	        int		Int;
			u32		String;
	    };
	};

    static const unsigned int MAX_DECOR_ENTRIES = 11;

	virtual void GetDynamicEntityGameState(CDynamicEntityGameStateDataNode& data) const = 0;
	virtual void SetDynamicEntityGameState(const CDynamicEntityGameStateDataNode& data) = 0;

protected:

    virtual ~IDynamicEntityNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CDynamicEntityGameStateDataNode
//
// Handles the serialization of a dynamic entity's game state
///////////////////////////////////////////////////////////////////////////////////////
class CDynamicEntityGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Dynamic Entity Game State", CDynamicEntityGameStateDataNode, IDynamicEntityNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

	void ExtractDataForSerialising(IDynamicEntityNodeDataAccessor& dynEntityNodeData)
    {
		dynEntityNodeData.GetDynamicEntityGameState(*this);
    }

    void ApplyDataFromSerialising(IDynamicEntityNodeDataAccessor& dynEntityNodeData)
    {
		dynEntityNodeData.SetDynamicEntityGameState(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const; 
	virtual void SetDefaultState();

public:

	u32  m_InteriorProxyLoc;
	bool m_loadsCollisions;
	bool m_retained;
	u32  m_decoratorListCount; // count of decorator extensions ( the scripted ones )
	IDynamicEntityNodeDataAccessor::decEnt m_decoratorList[IDynamicEntityNodeDataAccessor::MAX_DECOR_ENTRIES];  // array of decorator extensions ( the scripted ones )
};

#endif  // DYNAMIC_ENTITY_SYNC_NODES_H
