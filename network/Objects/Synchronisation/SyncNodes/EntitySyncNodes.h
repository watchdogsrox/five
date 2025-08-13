//
// name:        EntitySyncNodes.h
// description: Network sync nodes used by CNetObjEntities
// written by:    John Gurney
//

#ifndef ENTITY_SYNC_NODES_H
#define ENTITY_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/BaseSyncNodes.h"
#include "fwnet/netobject.h"
#include "script/handlers/GameScriptIds.h"

class CEntityScriptGameStateDataNode;
class CEntityOrientationDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IEntityNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the entity nodes.
// Any class that wishes to send/receive data contained in the entity nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IEntityNodeDataAccessor : public netINodeDataAccessor
{
public:

	DATA_ACCESSOR_ID_DECL(IEntityNodeDataAccessor);

	// getter functions
	virtual void   GetEntityScriptInfo(bool &hasScriptInfo, CGameScriptObjInfo& scriptInfo) const = 0;
	virtual void   GetEntityScriptGameState(CEntityScriptGameStateDataNode& data) = 0;
	virtual void   GetEntityOrientation(CEntityOrientationDataNode& data) = 0;

	// setter functions
	virtual void   SetEntityScriptInfo(bool hasScriptInfo, CGameScriptObjInfo& scriptInfo) = 0;
	virtual void   SetEntityScriptGameState(const CEntityScriptGameStateDataNode& data) = 0;
	virtual void   SetEntityOrientation(const CEntityOrientationDataNode& data) = 0;

protected:

	virtual ~IEntityNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CEntityScriptInfoDataNode
//
// Handles the serialization of an entity's script info.
///////////////////////////////////////////////////////////////////////////////////////
class CEntityScriptInfoDataNode : public CSyncDataNodeInfrequent
{
#if __ASSERT
	virtual bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime ) const;
#endif 

protected:

	NODE_COMMON_IMPL("Entity Script Info", CEntityScriptInfoDataNode, IEntityNodeDataAccessor);

	virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IEntityNodeDataAccessor &entityNodeData)
    {
        entityNodeData.GetEntityScriptInfo(m_hasScriptInfo, m_scriptInfo);
    }

    void ApplyDataFromSerialising(IEntityNodeDataAccessor &entityNodeData)
    {
        entityNodeData.SetEntityScriptInfo(m_hasScriptInfo, m_scriptInfo);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

private:

	bool				m_hasScriptInfo;
	CGameScriptObjInfo	m_scriptInfo;
};

///////////////////////////////////////////////////////////////////////////////////////
// CEntityScriptGameStateDataNode
//
// Handles the serialization of an entity's script game state.
///////////////////////////////////////////////////////////////////////////////////////
class CEntityScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Entity Script Game State", CEntityScriptGameStateDataNode, IEntityNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

	void ExtractDataForSerialising(IEntityNodeDataAccessor &entityNodeData)
	{
		entityNodeData.GetEntityScriptGameState(*this);
	}

	void ApplyDataFromSerialising(IEntityNodeDataAccessor &entityNodeData)
	{
		entityNodeData.SetEntityScriptGameState(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	bool m_isFixed;                    // gamestate flag indicating whether the object is using fixed physics
	bool m_usesCollision;              // gamestate flag indicating whether the object is using collision
	bool m_disableCollisionCompletely;
};

///////////////////////////////////////////////////////////////////////////////////////
// CEntityOrientationDataNode
//
// Handles the serialization of an entity's orientation
///////////////////////////////////////////////////////////////////////////////////////
class CEntityOrientationDataNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Entity Orientation", CEntityOrientationDataNode, IEntityNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IEntityNodeDataAccessor &entityNodeData)
    {
        entityNodeData.GetEntityOrientation(*this);
    }

    void ApplyDataFromSerialising(IEntityNodeDataAccessor &entityNodeData)
    {
        entityNodeData.SetEntityOrientation(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

public:

    Matrix34 m_orientation; // current orientation of the object
};

#endif  // ENTITY_SYNC_NODES_H
