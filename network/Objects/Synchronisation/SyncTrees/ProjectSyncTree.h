//
// name:		ProjectSyncTree.h
// description: The network sync tree class for this project
// written by:    John Gurney
//

#ifndef PROJECT_SYNC_TREE_H
#define PROJECT_SYNC_TREE_H

#include "network/NetworkInterface.h"
#include "fwnet/netlog.h"
#include "fwnet/netsynctree.h"

class CProjectBaseSyncDataNode;

class CProjectSyncTree : public netSyncTree
{
public:

	CProjectSyncTree(u32 sizeOfInitialStateBuffer = 0) :
    m_initialStateBuffer(0),
	m_sizeOfInitialStateBuffer(sizeOfInitialStateBuffer),
	m_bInitialStateBufferInitialised(false)
	{ 
		gnetAssertf(sizeOfInitialStateBuffer <= SIZEOF_BACKUP_BUFFER, "CProjectSyncTree backup buffer is too small, increase to %d", sizeOfInitialStateBuffer);

		if (m_sizeOfInitialStateBuffer>0) 
			m_initialStateBuffer = rage_new u8[m_sizeOfInitialStateBuffer]; 
	}

	virtual ~CProjectSyncTree()
	{
		if (m_initialStateBuffer)
			delete m_initialStateBuffer;
	}

	virtual void ApplyNodeData(netSyncTreeTargetObject* pObj, DataNodeFlags* updatedNodes = NULL);
	virtual bool Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, const unsigned currTime, 
		netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags = NULL);

	// resets the state of all script nodes and applies the data to the target object. Used when the target object changes from a script to
	// an ambient entity
	void ResetScriptState(netSyncTreeTargetObject* pObj);

	// required for CNetObjPhysical::PostSync()
	virtual netSyncDataNode*	     GetOrientationNode()   { return NULL; }
	virtual netSyncDataNode*	     GetVelocityNode()      { return NULL; }
	virtual netSyncDataNode*	     GetAngVelocityNode()   { return NULL; }

	// require for accessing creation node
	virtual netSyncDataNode*	     GetCreateNode() = 0;

	virtual bool GetPositionWasUpdated() const { return false; }

	bool HasInitialStateBufferBeenInitialised() const { return m_bInitialStateBufferInitialised; }

	// initialises the initial state buffer
	void InitialiseInitialStateBuffer(netSyncTreeTargetObject* pObj, bool bForce = false);

	// uses the static initial state buffer held in the net obj to determine which nodes should be dirtied for the given players. If 'player' is
	// specified, the nodes are only dirtied for that player, otherwise they are dirtied for all players
	virtual void DirtyNodesThatDifferFromInitialState(netSyncTreeTargetObject* pObj, ActivationFlags actFlags, PhysicalPlayerIndex player = INVALID_PLAYER_INDEX);

protected:

    virtual void WriteActivationFlags(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog);
    virtual void ReadActivationFlags(SerialiseModeFlags serMode, ActivationFlags &actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog);

	static unsigned const SIZEOF_BACKUP_BUFFER = 2400;

	// used when grabbing the initial state of an entity to backup the current state of the tree before writing to the initial state buffer
	static u8 ms_backupBuffer[SIZEOF_BACKUP_BUFFER];

	// a buffer containing the initial state on an entity of the type which uses this tree
	u8*  m_initialStateBuffer;
	u32  m_sizeOfInitialStateBuffer;
	bool m_bInitialStateBufferInitialised;
};

#endif  // PROJECT_SYNC_TREE_H
