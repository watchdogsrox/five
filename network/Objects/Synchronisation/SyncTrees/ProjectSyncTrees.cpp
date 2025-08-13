//
// name:        ProjectSyncTrees.cpp
// description: The network sync trees used in this project
// written by:    John Gurney
//

#include "network/Objects/Synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "Network/Objects/NetworkObjectMgr.h"

NETWORK_OPTIMISATIONS()

#define CREATE_NODE					SERIALISEMODE_CREATE, 0, 0
#define SCRIPT_CREATE_NODE			SERIALISEMODE_CREATE, 0, ACTIVATIONFLAG_SCRIPTOBJECT
#define MIGRATE_NODE				SERIALISEMODE_MIGRATE, 0, 0
#define CREATE_MIGRATE_NODE			SERIALISEMODE_CREATE|SERIALISEMODE_MIGRATE, 0, 0
#define SCRIPT_MIGRATE_NODE			SERIALISEMODE_MIGRATE, 0, ACTIVATIONFLAG_SCRIPTOBJECT
#define AMBIENT_MIGRATE_NODE		SERIALISEMODE_MIGRATE, 0, ACTIVATIONFLAG_AMBIENTOBJECT
#define UPDATE_NODE					SERIALISEMODE_UPDATE|SERIALISEMODE_FORCE_SEND_OF_DIRTY|SERIALISEMODE_VERIFY|SERIALISEMODE_MIGRATE, SERIALISEMODE_UPDATE|SERIALISEMODE_FORCE_SEND_OF_DIRTY|SERIALISEMODE_VERIFY|SERIALISEMODE_MIGRATE, 0
#define CRITICAL_NODE				SERIALISEMODE_ALL, SERIALISEMODE_ALL, 0
#define SCRIPT_CRITICAL_NODE		SERIALISEMODE_ALL, SERIALISEMODE_ALL, ACTIVATIONFLAG_SCRIPTOBJECT
#define UPDATE_CREATE_NODE			SERIALISEMODE_CREATE|SERIALISEMODE_UPDATE|SERIALISEMODE_FORCE_SEND_OF_DIRTY|SERIALISEMODE_VERIFY|SERIALISEMODE_MIGRATE, SERIALISEMODE_CREATE|SERIALISEMODE_UPDATE|SERIALISEMODE_FORCE_SEND_OF_DIRTY|SERIALISEMODE_VERIFY|SERIALISEMODE_MIGRATE, 0

////////////////////////////////////////////////////////////////////////////
// CProximityMigrateableSyncTreeBase
////////////////////////////////////////////////////////////////////////////
CProximityMigrateableSyncTreeBase::CProximityMigrateableSyncTreeBase(u32 sizeOfInitialStateBuffer) :
CProjectSyncTree(sizeOfInitialStateBuffer),
m_rootNode("[PARENT] Root"),
m_createParentNode("[PARENT] Create"),
m_migrateParentNode("[PARENT] Migrate")
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_sectorNode, "Node Sector");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_positionNode, "Node Position");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_globalFlagsNode, "Node Game State");
#endif // __BANK
}

bool CProximityMigrateableSyncTreeBase::GetPositionWasUpdated() const
{
    return m_positionNode.GetWasUpdated();
}

void CProximityMigrateableSyncTreeBase::DirtyPositionNodes(netSyncTreeTargetObject &targetObject)
{
    DirtyNode(&targetObject, m_sectorNode);
	DirtyNode(&targetObject, m_positionNode);
}

void CProximityMigrateableSyncTreeBase::ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
    ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_positionNode);
}

void CProximityMigrateableSyncTreeBase::ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player)
{
    ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_positionNode);
}

bool CProximityMigrateableSyncTreeBase::IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime)
{
    if(targetObject->GetSyncData())
    {
        if (m_positionNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
// CEntitySyncTreeBase
////////////////////////////////////////////////////////////////////////////
CEntitySyncTreeBase::CEntitySyncTreeBase(u32 sizeOfInitialStateBuffer) :
CProximityMigrateableSyncTreeBase(sizeOfInitialStateBuffer),
m_infrequentParentNode("[PARENT] Infrequent"),
m_frequentParentNode("[PARENT] Frequent"),
m_gameStateParentNode("[PARENT] Game State"),
m_commonGameStateParentNode("[PARENT] Common Game State"),
m_scriptGameStateParentNode("[PARENT] Script Game State")
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_entityScriptInfoNode, "Node Script Info");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_entityScriptGameStateNode, "Node Script Game State");
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////
// CDynamicEntitySyncTreeBase - comtains nodes common to dynamic entities
////////////////////////////////////////////////////////////////////////////
CDynamicEntitySyncTreeBase::CDynamicEntitySyncTreeBase(u32 sizeOfInitialStateBuffer) :
CEntitySyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_dynamicEntityGameStateNode, "Node Game State");
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////
// CPhysicalSyncTreeBase - comtains nodes common to objects & vehicles
////////////////////////////////////////////////////////////////////////////
CPhysicalSyncTreeBase::CPhysicalSyncTreeBase(u32 sizeOfInitialStateBuffer) :
CDynamicEntitySyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_orientationNode,				"Node Entity Orientation");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_velocityNode,					"Node Velocity");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_angVelocityNode,				"Node Angular Velocity");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_healthNode,					"Node Health");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_attachNode,					"Node Attach");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_physicalGameStateNode,		"Node Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_physicalScriptGameStateNode,  "Node Script Game State");
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////
// CPedSyncTreeBase
////////////////////////////////////////////////////////////////////////////
CPedSyncTreeBase::CPedSyncTreeBase(u32 sizeOfInitialStateBuffer) :
CDynamicEntitySyncTreeBase(sizeOfInitialStateBuffer),
m_tasksParentNode("[PARENT] Tasks")
{
	// set up task specific data nodes
	for (u32 i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
	{
		CPedTaskSpecificDataNode* pTaskNode = &m_taskSpecificDataNodes[i];

		pTaskNode->SetTaskIndex(i);

		// make the task specific data dependent on the task tree node. The task specific data will always be sent out when the task tree data changes
		pTaskNode->SetNodeDependency(m_taskTreeNode);

		DEV_ONLY(NetworkInterface::RegisterNodeWithBandwidthRecorder(*pTaskNode, "Node Ped Task Specific Data"));
	}

#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_orientationNode,				"Node Ped Orientation");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_movementNode,					"Node Ped Movement");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_attachNode,					"Node Attach");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_movementGroupNode,			"Node Ped Movement");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_aiNode,						"Node Ped AI");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedGameStateNode,				"Node Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_componentReservationNode,     "Node Ped Comp Reservation");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_physicalGameStateNode,		"Node Game State");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedHealthNode,				"Node Health");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_taskTreeNode,					"Node Ped Task Tree");
#endif // __BANK

	// build tree common to peds & players
	PushParent(m_rootNode);
		PushParent(m_createParentNode);
			// + ped or player creation node
		PopParent();
		PushParent(m_infrequentParentNode);
			// + ped or player appearance node
			// + player ped group node
			PushParent(m_gameStateParentNode);
				PushParent(m_commonGameStateParentNode);
					AddChild(m_globalFlagsNode,				CRITICAL_NODE);
					AddChild(m_dynamicEntityGameStateNode,  CRITICAL_NODE);
					AddChild(m_physicalGameStateNode,		CRITICAL_NODE);
					AddChild(m_pedGameStateNode,			CRITICAL_NODE);
					AddChild(m_componentReservationNode,	CRITICAL_NODE);
				PopParent();
				PushParent(m_scriptGameStateParentNode);
					AddChild(m_entityScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					// + player game state node
				PopParent();
			PopParent();
            // attach node has to appear after entity script game state, otherwise entity can be detached incorrectly
            AddChild(m_attachNode,							SCRIPT_CRITICAL_NODE); // IF YOU CHANGE THIS FROM SCRIPT OBJECTS ONLY UPDATE CNetObjPed::IsAttachmentStateSentOverNetwork()
			AddChild(m_pedHealthNode,						CRITICAL_NODE);
			AddChild(m_movementGroupNode,					UPDATE_CREATE_NODE);
			AddChild(m_aiNode,								SCRIPT_CRITICAL_NODE);
		PopParent();
		PushParent(m_frequentParentNode);
			AddChild(m_orientationNode,						UPDATE_CREATE_NODE);
			AddChild(m_movementNode,						UPDATE_CREATE_NODE);
            PushParent(m_tasksParentNode);
				AddChild(m_taskTreeNode,					CRITICAL_NODE);

				for (u32 i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
				{
					AddChild(m_taskSpecificDataNodes[i],	UPDATE_CREATE_NODE);
				}

			PopParent();
			// + player camera node
		PopParent();
		PushParent(m_migrateParentNode);
			AddChild(m_migrationNode,						MIGRATE_NODE);
			AddChild(m_physicalMigrationNode,				MIGRATE_NODE);
			AddChild(m_physicalScriptMigrationNode,			SCRIPT_MIGRATE_NODE);
		PopParent();
	PopParent();

#if !__NO_OUTPUT
	// update the task node names to include the task slot ID for debugging purposes
	char name[30];
	for (u32 i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
	{
		sprintf(name, "Ped Task Specific Data %d", i);
		m_taskSpecificDataNodes[i].SetNodeName(name);
	}
#endif // __BANK

}

void CPedSyncTreeBase::ForceResendOfAllTaskData(netSyncTreeTargetObject* pObj)
{
	SetTargetObject(pObj);

	DirtyNode(pObj, m_taskTreeNode);

	SetTargetObject(NULL);
}

void CPedSyncTreeBase::PostApplyData(netSyncTreeTargetObject* pObj)
{
	if (m_taskTreeNode.GetWasUpdated())
	{
		gnetAssert(dynamic_cast<IPedNodeDataAccessor *>(pObj->GetDataAccessor(IPedNodeDataAccessor::DATA_ACCESSOR_ID())));

		IPedNodeDataAccessor *dataAccessor = static_cast<IPedNodeDataAccessor *>(pObj->GetDataAccessor(IPedNodeDataAccessor::DATA_ACCESSOR_ID()));

		dataAccessor->PostTasksUpdate();
	}
	else 
	{
		bool bTaskDataUpdated = false;

		for (int i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
		{
			if (m_taskSpecificDataNodes[i].GetWasUpdated())
			{
				bTaskDataUpdated = true;
				break;
			}
		}

		if (bTaskDataUpdated)
		{
			IPedNodeDataAccessor *dataAccessor = static_cast<IPedNodeDataAccessor *>(pObj->GetDataAccessor(IPedNodeDataAccessor::DATA_ACCESSOR_ID()));

			dataAccessor->UpdateTaskSpecificData();
		}		
	}
}

////////////////////////////////////////////////////////////////////////////
// CPedSyncTree
////////////////////////////////////////////////////////////////////////////
CPedSyncTree::CPedSyncTree(u32 sizeOfInitialStateBuffer) :
CPedSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedSectorPosMapNode,          "Node Ped Map Sector Pos");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedSectorPosNavMeshNode,		"Node Ped Navmesh Sector Pos");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_physicalScriptGameStateNode,  "Node Script Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedScriptGameStateNode,		"Node Script Game State");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedAppearanceNode,			"Node Ped Appearance");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedInventoryMigrationNode,	"Node Ped Inventory");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pedTaskSequenceMigrationNode,	"Node Ped Sequence Data");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_pedSectorPosMapNode.SetNodeDependency(m_sectorNode);
    m_pedSectorPosNavMeshNode.SetNodeDependency(m_sectorNode);

	// make the global flags node always go out with the script info node
	m_globalFlagsNode.SetNodeDependency(m_entityScriptInfoNode);

#if __BANK
	SetTreeName("Ped Sync Tree");
#endif

	SetCurrentParent(m_createParentNode);
	AddChild(m_pedCreateNode,								CREATE_NODE);
    AddChild(m_pedScriptCreateNode,							SCRIPT_CREATE_NODE);

	SetCurrentParent(m_scriptGameStateParentNode);
	AddChild(m_physicalScriptGameStateNode,					SCRIPT_CRITICAL_NODE);
	AddChild(m_pedScriptGameStateNode,						SCRIPT_CRITICAL_NODE);
	AddChild(m_entityScriptInfoNode,						SCRIPT_CRITICAL_NODE);

    SetCurrentParent(m_frequentParentNode);
	AddChild(m_sectorNode,							        UPDATE_CREATE_NODE);
    AddChild(m_pedSectorPosMapNode,							UPDATE_CREATE_NODE);
	AddChild(m_pedSectorPosNavMeshNode,						UPDATE_CREATE_NODE);

    SetCurrentParent(m_infrequentParentNode);
    AddChild(m_pedAppearanceNode,						    UPDATE_CREATE_NODE);

	SetCurrentParent(m_migrateParentNode);
	AddChild(m_pedInventoryMigrationNode,                   CREATE_MIGRATE_NODE);
	AddChild(m_pedTaskSequenceMigrationNode,				SERIALISEMODE_MIGRATE, SERIALISEMODE_MIGRATE, ACTIVATIONFLAG_SCRIPTOBJECT); // a conditional nod only used by script entities

	InitialiseTree();
}

bool CPedSyncTree::GetPositionWasUpdated() const
{
    if(m_pedSectorPosMapNode.GetWasUpdated() || m_pedSectorPosNavMeshNode.GetWasUpdated())
    {
        return true;
    }

    return false;
}

void CPedSyncTree::DirtyPositionNodes(netSyncTreeTargetObject &targetObject)
{
    DirtyNode(&targetObject, *GetSectorNode());
	DirtyNode(&targetObject, m_pedSectorPosMapNode);
    DirtyNode(&targetObject, m_pedSectorPosNavMeshNode);
}

void CPedSyncTree::ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
    ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pedSectorPosMapNode);
    ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pedSectorPosNavMeshNode);
}

void CPedSyncTree::ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player)
{
    ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pedSectorPosMapNode);
    ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pedSectorPosNavMeshNode);
}

bool CPedSyncTree::IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime)
{
    if(targetObject->GetSyncData())
    {
        if (m_pedSectorPosMapNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime) ||
            m_pedSectorPosNavMeshNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
// CPlayerSyncTree
////////////////////////////////////////////////////////////////////////////
CPlayerSyncTree::CPlayerSyncTree(u32 sizeOfInitialStateBuffer) :
CPedSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_cameraNode,					   "Node Player Camera");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerSectorPosNode,			   "Player Sector Pos");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerAppearanceNode,			   "Node Player Appearance");
 	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerGameStateNode,			   "Node Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerPedGroupNode,			   "Node Player Ped Group");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerWantedAndLOSDataNode,      "Node Player Wanted and LOS");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerAmbientModelStreamingNode, "Node Player Ambient Model Streaming");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerGamerDataNode,			   "Node Player Gamer Data");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_playerExtendedGameStateNode,	   "Node Player Extended Game State");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_playerSectorPosNode.SetNodeDependency(m_sectorNode);
	// make the task data dependent on the player appearance. The task data will always be sent with the player appearance data.
	m_taskTreeNode.SetNodeDependency(m_playerAppearanceNode);
	
#if __BANK
	SetTreeName("Player Sync Tree");
#endif

	SetCurrentParent(m_createParentNode);
	AddChild(m_playerCreateNode,							CREATE_NODE);

	SetCurrentParent(m_infrequentParentNode);
	AddChild(m_playerAppearanceNode,						UPDATE_CREATE_NODE);
 	AddChild(m_playerPedGroupNode,							UPDATE_NODE);
    AddChild(m_playerAmbientModelStreamingNode,				UPDATE_NODE);
	AddChild(m_playerGamerDataNode,							UPDATE_NODE);
	AddChild(m_playerExtendedGameStateNode,					UPDATE_NODE);

	SetCurrentParent(m_frequentParentNode);
    AddChild(m_sectorNode,							        UPDATE_CREATE_NODE);
    AddChild(m_playerSectorPosNode,					        UPDATE_CREATE_NODE);
	AddChild(m_cameraNode,									UPDATE_NODE);
	AddChild(m_playerWantedAndLOSDataNode,					UPDATE_NODE);

	SetCurrentParent(m_scriptGameStateParentNode);
	AddChild(m_playerGameStateNode,							UPDATE_CREATE_NODE);

    InitialiseTree();
}

bool CPlayerSyncTree::GetPositionWasUpdated() const
{
    return m_playerSectorPosNode.GetWasUpdated();
}

void CPlayerSyncTree::DirtyPositionNodes(netSyncTreeTargetObject &targetObject)
{
    DirtyNode(&targetObject, *GetSectorNode());
	DirtyNode(&targetObject, m_playerSectorPosNode);
}

void CPlayerSyncTree::ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
	ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_playerSectorPosNode);
}

void CPlayerSyncTree::ForceSendOfGameStateDataNode(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
	ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_playerGameStateNode);
}

void CPlayerSyncTree::ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player)
{
    ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_playerSectorPosNode);
}


bool CPlayerSyncTree::IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime)
{
    if(targetObject->GetSyncData())
    {
        if(m_playerSectorPosNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
// CObjectSyncTreeBase
////////////////////////////////////////////////////////////////////////////
CObjectSyncTree::CObjectSyncTree(u32 sizeOfInitialStateBuffer) :
CPhysicalSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_objectSectorPosNode,       "Object Sector Pos");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_objectOrientationNode,     "Object Orientation");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_objectGameStateNode      , "Node Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_objectScriptGameStateNode, "Node Script Game State");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_objectSectorPosNode.SetNodeDependency(m_sectorNode);

	// make the global flags node always go out with the script info node
	m_globalFlagsNode.SetNodeDependency(m_entityScriptInfoNode);

#if __BANK
	SetTreeName("Object Sync Tree");
#endif

	PushParent(m_rootNode);
		PushParent(m_createParentNode);
			AddChild(m_createNode,							CREATE_NODE);
		PopParent();
		PushParent(m_infrequentParentNode);
			PushParent(m_gameStateParentNode);
				PushParent(m_commonGameStateParentNode);
					AddChild(m_globalFlagsNode,				CRITICAL_NODE);
					AddChild(m_dynamicEntityGameStateNode,	CRITICAL_NODE);
					AddChild(m_physicalGameStateNode,		CRITICAL_NODE);
					AddChild(m_objectGameStateNode,			CRITICAL_NODE);
				PopParent();
				PushParent(m_scriptGameStateParentNode);
					AddChild(m_entityScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_physicalScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_objectScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_entityScriptInfoNode,		SCRIPT_CRITICAL_NODE);
				PopParent();
			PopParent();
            // attach node has to appear after entity script game state, otherwise entity can be detached incorrectly
            AddChild(m_attachNode,							CRITICAL_NODE); // IF YOU CHANGE THIS TO ONLY BE SENT FOR SCRIPT OBJECTS ADD CNetObjObject::IsAttachmentStateSentOverNetwork()
			AddChild(m_healthNode,							CRITICAL_NODE);
		PopParent();
		PushParent(m_frequentParentNode);
			AddChild(m_sectorNode,							UPDATE_CREATE_NODE);
			AddChild(m_objectSectorPosNode,					UPDATE_CREATE_NODE);
            AddChild(m_objectOrientationNode,               UPDATE_CREATE_NODE);
			AddChild(m_velocityNode,						UPDATE_CREATE_NODE);
			AddChild(m_angVelocityNode,						UPDATE_CREATE_NODE);
		PopParent();
		PushParent(m_migrateParentNode);
			AddChild(m_migrationNode,						MIGRATE_NODE);
			AddChild(m_physicalMigrationNode,				MIGRATE_NODE);
			AddChild(m_physicalScriptMigrationNode,			SCRIPT_MIGRATE_NODE);
		PopParent();
	PopParent();

	InitialiseTree();
}

bool CObjectSyncTree::GetPositionWasUpdated() const
{
    return m_objectSectorPosNode.GetWasUpdated();
}

void CObjectSyncTree::DirtyPositionNodes(netSyncTreeTargetObject &targetObject)
{
    DirtyNode(&targetObject, *GetSectorNode());
	DirtyNode(&targetObject, m_objectSectorPosNode);
}

void CObjectSyncTree::ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
	ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_objectSectorPosNode);
}

void CObjectSyncTree::ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player)
{
    ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_objectSectorPosNode);
}


bool CObjectSyncTree::IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime)
{
    if(targetObject->GetSyncData())
    {
        if(m_objectSectorPosNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime))
        {
            return true;
        }
    }

    return false;
}

void CObjectSyncTree::ForceSendOfScriptGameStateData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
    ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_objectScriptGameStateNode);
}

////////////////////////////////////////////////////////////////////////////
// CPickupSyncTree
////////////////////////////////////////////////////////////////////////////
CPickupSyncTree::CPickupSyncTree(u32 sizeOfInitialStateBuffer) :
CPhysicalSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pickupScriptGameStateNode, "Node Pickup Script Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_pickupSectorPosNode,       "Node Pickup Sector Pos");
#endif // __BANK

	// make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_pickupSectorPosNode.SetNodeDependency(m_sectorNode);

	// make the global flags node always go out with the script info node
	m_globalFlagsNode.SetNodeDependency(m_entityScriptInfoNode);

	m_attachNode.SyncPhysicsActivation();

#if __BANK
	SetTreeName("Pickup Sync Tree");
#endif

	PushParent(m_rootNode);
		PushParent(m_createParentNode);
			AddChild(m_createNode,							CREATE_NODE);
		PopParent();
		PushParent(m_infrequentParentNode);
			PushParent(m_commonGameStateParentNode);
				AddChild(m_globalFlagsNode,					CRITICAL_NODE);
				AddChild(m_dynamicEntityGameStateNode,		CRITICAL_NODE);
			PopParent();
			PushParent(m_scriptGameStateParentNode);
				AddChild(m_pickupScriptGameStateNode,		SCRIPT_CRITICAL_NODE);
				AddChild(m_physicalGameStateNode,			SCRIPT_CRITICAL_NODE); 
				AddChild(m_entityScriptGameStateNode,		SCRIPT_CRITICAL_NODE);
				AddChild(m_physicalScriptGameStateNode,		SCRIPT_CRITICAL_NODE);
				AddChild(m_entityScriptInfoNode,			SCRIPT_CRITICAL_NODE);
				AddChild(m_healthNode,						SCRIPT_CRITICAL_NODE); 
			PopParent();
			AddChild(m_attachNode,							SCRIPT_CRITICAL_NODE); // IF YOU CHANGE THIS FROM SCRIPT OBJECTS ONLY UPDATE CNetObjPickup::IsAttachmentStateSentOverNetwork()
		PopParent();
		PushParent(m_frequentParentNode);
			AddChild(m_sectorNode,							UPDATE_CREATE_NODE);
			AddChild(m_pickupSectorPosNode,					UPDATE_CREATE_NODE);
			AddChild(m_orientationNode,						UPDATE_CREATE_NODE);
			AddChild(m_velocityNode,						UPDATE_CREATE_NODE);
			AddChild(m_angVelocityNode,						UPDATE_CREATE_NODE);
		PopParent();
		PushParent(m_migrateParentNode);
			AddChild(m_migrationNode,						MIGRATE_NODE);
			AddChild(m_physicalMigrationNode,				SCRIPT_MIGRATE_NODE);	
			AddChild(m_physicalScriptMigrationNode,			SCRIPT_MIGRATE_NODE);
		PopParent();
	PopParent();

	InitialiseTree();
}


bool CPickupSyncTree::GetPositionWasUpdated() const
{
	return m_pickupSectorPosNode.GetWasUpdated();
}

void CPickupSyncTree::DirtyPositionNodes(netSyncTreeTargetObject &targetObject)
{
	DirtyNode(&targetObject, *GetSectorNode());
	DirtyNode(&targetObject, m_pickupSectorPosNode);
}

void CPickupSyncTree::ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags)
{
	ForceSendOfNodeData(SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pickupSectorPosNode);
}

void CPickupSyncTree::ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player)
{
	ForceSendOfNodeDataToPlayer(player, SERIALISEMODE_UPDATE, activationFlags, &targetObject, m_pickupSectorPosNode);
}

bool CPickupSyncTree::IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime)
{
	if(targetObject->GetSyncData())
	{
		if(m_pickupSectorPosNode.IsTimeToSendUpdateToPlayer(targetObject, player, currTime))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////
// CPickupPlacementSyncTree
////////////////////////////////////////////////////////////////////////////
CPickupPlacementSyncTree::CPickupPlacementSyncTree() :
CProximityMigrateableSyncTreeBase(0)
{
#if __BANK
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_stateNode, "Node Game State");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_positionNode.SetNodeDependency(m_sectorNode);

#if __BANK
	SetTreeName("Pickup Placement Sync Tree");
#endif

	PushParent(m_rootNode);
		AddChild(m_createNode,							CREATE_NODE);
		AddChild(m_migrationNode,						MIGRATE_NODE);
		AddChild(m_globalFlagsNode,						CRITICAL_NODE);
		AddChild(m_stateNode,							CRITICAL_NODE);
	PopParent();

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CGlassPaneSyncTree
////////////////////////////////////////////////////////////////////////////
CGlassPaneSyncTree::CGlassPaneSyncTree() :
CProximityMigrateableSyncTreeBase(0)
{
#if __BANK
//	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_stateNode, "Node Game State");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_positionNode.SetNodeDependency(m_sectorNode);

#if __BANK
	SetTreeName("Glass Pane Sync Tree");
#endif

	PushParent(m_rootNode);
		AddChild(m_createNode,							CREATE_NODE);
		AddChild(m_migrationNode,						MIGRATE_NODE);
		AddChild(m_globalFlagsNode,						CRITICAL_NODE);
	PopParent();

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CVehicleSyncTree
////////////////////////////////////////////////////////////////////////////
CVehicleSyncTree::CVehicleSyncTree(u32 sizeOfInitialStateBuffer) :
CPhysicalSyncTreeBase(sizeOfInitialStateBuffer),
m_controlParentNode("[PARENT] Control")
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_steeringNode,               "Node Vehicle Steering");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleControlNode,         "Node Vehicle Control Node");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_appearanceNode,             "Node Appearance");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_damageStatusNode,           "Node Damage Status");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleAngVelocityNode,     "Node Vehicle Angular Velocity");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleGameStateNode,       "Node Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleScriptGameStateNode, "Node Script Game State");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleHealthNode,          "Node Vehicle Health");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_componentReservationNode,   "Node Vehicle Comp Reservation");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleTaskNode,			  "Node Vehicle Task");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_vehicleGadgetNode,		  "Node Vehicle Gadget");
#endif // __BANK

    // make the sector position dependent on the sector. The position data will always be sent with the sector data.
	m_positionNode.SetNodeDependency(m_sectorNode);

	// make the global flags node always go out with the script info node
	m_globalFlagsNode.SetNodeDependency(m_entityScriptInfoNode);

#if __BANK
	SetTreeName("Vehicle Sync Tree");
#endif

	// build tree common to all vehicles:
	PushParent(m_rootNode);
		PushParent(m_createParentNode);
			AddChild(m_vehicleCreateNode,					CREATE_NODE);
		PopParent();
		PushParent(m_infrequentParentNode);
			PushParent(m_gameStateParentNode);
				PushParent(m_commonGameStateParentNode);
					AddChild(m_globalFlagsNode,				CRITICAL_NODE);
					AddChild(m_dynamicEntityGameStateNode,	CRITICAL_NODE);
					AddChild(m_physicalGameStateNode,		CRITICAL_NODE);
					AddChild(m_vehicleGameStateNode,		CRITICAL_NODE);
					// + auto, bike, boat or submarine game state node
				PopParent();
				PushParent(m_scriptGameStateParentNode);
					AddChild(m_entityScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_physicalScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_vehicleScriptGameStateNode,	SCRIPT_CRITICAL_NODE);
					AddChild(m_entityScriptInfoNode,		SCRIPT_CRITICAL_NODE);
				PopParent();
			PopParent();
            // attach node has to appear after entity script game state, otherwise entity can be detached incorrectly
            AddChild(m_attachNode,							CRITICAL_NODE); // IF YOU CHANGE THIS TO ONLY BE SENT FOR SCRIPT OBJECTS ADD CNetObjVehicle::IsAttachmentStateSentOverNetwork()
			AddChild(m_appearanceNode,						CRITICAL_NODE);
			AddChild(m_damageStatusNode,					CRITICAL_NODE);
			AddChild(m_componentReservationNode,			CRITICAL_NODE);
			AddChild(m_vehicleHealthNode,                   CRITICAL_NODE);
			AddChild(m_vehicleTaskNode,						CRITICAL_NODE);
		PopParent();
		PushParent(m_frequentParentNode);
			AddChild(m_sectorNode,							UPDATE_CREATE_NODE);
			AddChild(m_positionNode,						UPDATE_CREATE_NODE);
			AddChild(m_orientationNode,						UPDATE_CREATE_NODE);
			AddChild(m_velocityNode,						UPDATE_CREATE_NODE);
			AddChild(m_vehicleAngVelocityNode,				UPDATE_CREATE_NODE);
			PushParent(m_controlParentNode);
				AddChild(m_steeringNode,					UPDATE_NODE);
				AddChild(m_vehicleControlNode,				UPDATE_CREATE_NODE);
				AddChild(m_vehicleGadgetNode,				CRITICAL_NODE);
				// + auto or heli gun data node
			PopParent();
		PopParent();
		PushParent(m_migrateParentNode);
			AddChild(m_migrationNode,						MIGRATE_NODE);
			AddChild(m_physicalMigrationNode,				MIGRATE_NODE);
			AddChild(m_physicalScriptMigrationNode,			SCRIPT_MIGRATE_NODE);
			AddChild(m_vehicleProximityMigrationNode,		MIGRATE_NODE);
		PopParent();
	PopParent();
}

////////////////////////////////////////////////////////////////////////////
// CAutomobileSyncTreeBase
////////////////////////////////////////////////////////////////////////////
CAutomobileSyncTreeBase::CAutomobileSyncTreeBase(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
	SetCurrentParent(m_createParentNode);
	AddChild(m_automobileCreateNode,						CREATE_NODE);
}

////////////////////////////////////////////////////////////////////////////
// CAutomobileSyncTree
////////////////////////////////////////////////////////////////////////////
CAutomobileSyncTree::CAutomobileSyncTree(u32 sizeOfInitialStateBuffer) :
CAutomobileSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	SetTreeName("Automobile Sync Tree");
#endif

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CBikeSyncTree
////////////////////////////////////////////////////////////////////////////
CBikeSyncTree::CBikeSyncTree(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_bikeGameStateNode, "Node Game State");
#endif // __BANK

#if __BANK
	SetTreeName("Bike Sync Tree");
#endif

	SetCurrentParent(m_commonGameStateParentNode);
	AddChild(m_bikeGameStateNode,							CRITICAL_NODE);

    InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CBoatSyncTree
////////////////////////////////////////////////////////////////////////////
CBoatSyncTree::CBoatSyncTree(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_boatGameStateNode, "Node Game State");
#endif // __BANK

#if __BANK
	SetTreeName("Boat Sync Tree");
#endif

	SetCurrentParent(m_commonGameStateParentNode);
	AddChild(m_boatGameStateNode,							UPDATE_CREATE_NODE);

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CHeliSyncTree
////////////////////////////////////////////////////////////////////////////
CHeliSyncTree::CHeliSyncTree(u32 sizeOfInitialStateBuffer) :
CAutomobileSyncTreeBase(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_heliHealthDataNode,   "Node Heli Health");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_heliControlDataNode, "Node Heli Control");
#endif // __BANK

#if __BANK
	SetTreeName("Heli Sync Tree");
#endif

	SetCurrentParent(m_controlParentNode);
	AddChild(m_heliControlDataNode,                         UPDATE_NODE);
 
    SetCurrentParent(m_infrequentParentNode);
    AddChild(m_heliHealthDataNode,                          UPDATE_CREATE_NODE);

    InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CSubmarineSyncTree
////////////////////////////////////////////////////////////////////////////
CSubmarineSyncTree::CSubmarineSyncTree(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_submarineControlNode,   "Node Submarine Control Node");
    NetworkInterface::RegisterNodeWithBandwidthRecorder(m_submarineGameStateNode, "Node Submarine Game State");
#endif // __BANK

#if __BANK
	SetTreeName("Submarine Sync Tree");
#endif

	SetCurrentParent(m_controlParentNode);
	AddChild(m_submarineControlNode,							UPDATE_NODE);

    SetCurrentParent(m_commonGameStateParentNode);
	AddChild(m_submarineGameStateNode,							UPDATE_CREATE_NODE);

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CTrainSyncTree
////////////////////////////////////////////////////////////////////////////
CTrainSyncTree::CTrainSyncTree(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_TrainGameStateNode, "Node Train Game State");
#endif // __BANK

#if __BANK
	SetTreeName("Train Sync Tree");
#endif

    SetCurrentParent(m_commonGameStateParentNode);
	AddChild(m_TrainGameStateNode,  CRITICAL_NODE);

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CPlaneSyncTree
////////////////////////////////////////////////////////////////////////////
CPlaneSyncTree::CPlaneSyncTree(u32 sizeOfInitialStateBuffer) :
CVehicleSyncTree(sizeOfInitialStateBuffer)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_PlaneGameStateNode,	"Node Plane Game State");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_PlaneControlDataNode, "Node Plane Control Data");
#endif // __BANK

#if __BANK
	SetTreeName("Plane Sync Tree");
#endif
	
	SetCurrentParent(m_controlParentNode);
	AddChild(m_PlaneControlDataNode,                        UPDATE_NODE);

	SetCurrentParent(m_infrequentParentNode);
	AddChild(m_PlaneGameStateNode,                          CRITICAL_NODE);

	InitialiseTree();
}

////////////////////////////////////////////////////////////////////////////
// CDoorSyncTree
////////////////////////////////////////////////////////////////////////////
CDoorSyncTree::CDoorSyncTree() :
CPhysicalSyncTreeBase(0)
{
#if __BANK
	// setup the bandwidth recorders for the data nodes
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_doorMovementNode,			"Node Door Movement");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_doorScriptInfoNode,		"Node Door Script Info");
	NetworkInterface::RegisterNodeWithBandwidthRecorder(m_doorScriptGameStateNode,	"Node Door Script Game State");
#endif // __BANK

#if __BANK
	SetTreeName("Door Sync Tree");
#endif

	PushParent(m_rootNode);
		PushParent(m_createParentNode);
			AddChild(m_createNode,							CREATE_NODE);
		PopParent();
		PushParent(m_infrequentParentNode);
			AddChild(m_globalFlagsNode,						CRITICAL_NODE);
			AddChild(m_doorScriptInfoNode,					SCRIPT_CRITICAL_NODE);
			AddChild(m_doorScriptGameStateNode,				SCRIPT_CRITICAL_NODE);
		PopParent();
		AddChild(m_doorMovementNode,						UPDATE_NODE);
		PushParent(m_migrateParentNode);
			AddChild(m_migrationNode,						MIGRATE_NODE);
			AddChild(m_physicalScriptMigrationNode,			SCRIPT_MIGRATE_NODE);
		PopParent();
	PopParent();

	InitialiseTree();
}

