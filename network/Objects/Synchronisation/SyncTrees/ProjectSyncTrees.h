//
// name:        ProjectSyncTrees.h
// description: The network sync trees used in this project
// written by:    John Gurney
//

#ifndef PROJECT_SYNC_TREES_H
#define PROJECT_SYNC_TREES_H

#include "network/objects/synchronisation/syncnodes/AutomobileSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/BikeSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/BoatSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/DoorSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/SubmarineSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/HeliSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/ObjectSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/PedSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/PickupSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/PickupPlacementSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/GlassPaneSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/PlayerSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/ProximityMigrateableSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/TrainSyncNodes.h"
#include "network/objects/synchronisation/syncnodes/PlaneSyncNodes.h"
#include "network/objects/synchronisation/synctrees/ProjectSyncTree.h"

// activation flags
enum
{
	ACTIVATIONFLAG_SCRIPTOBJECT				= (1<<0),
	ACTIVATIONFLAG_AMBIENTOBJECT			= (1<<1),
	ACTIVATIONFLAG_PROXIMITY_MIGRATE		= (1<<2)
};

////////////////////////////////////////////////////////////////////////////////
// CProximityMigrateableSyncTreeBase - contains nodes common to all proximity migrateables
////////////////////////////////////////////////////////////////////////////////
class CProximityMigrateableSyncTreeBase : public CProjectSyncTree
{
public:

	CProximityMigrateableSyncTreeBase(u32 sizeOfInitialStateBuffer);

	// node accessors (assert if not inserted into tree)
    CGlobalFlagsDataNode	        *GetGlobalFlagsNode()		{ gnetAssert(m_globalFlagsNode.GetParentTree()); return &m_globalFlagsNode; }
	CSectorDataNode                 *GetSectorNode()			{ gnetAssert(m_sectorNode.GetParentTree());      return &m_sectorNode; }
	CSectorPositionDataNode         *GetSectorPositionNode()	{ gnetAssert(m_positionNode.GetParentTree());      return &m_positionNode; }
	CMigrationDataNode              *GetMigrationDataNode()		{ gnetAssert(m_migrationNode.GetParentTree());   return &m_migrationNode; }

	const CGlobalFlagsDataNode	  *GetGlobalFlagsNode() const { gnetAssert(m_globalFlagsNode.GetParentTree()); return &m_globalFlagsNode; }
	const CSectorDataNode         *GetSectorNode()      const { gnetAssert(m_sectorNode.GetParentTree());      return &m_sectorNode; }

    virtual bool GetPositionWasUpdated() const;
    virtual void DirtyPositionNodes(netSyncTreeTargetObject &targetObject);
    virtual void ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);
    virtual void ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player);
    virtual bool IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime);

protected:

	// parent nodes
	CProjectBaseSyncParentNode				m_rootNode;
	CProjectBaseSyncParentNode				m_createParentNode;		// parent of create nodes
	CProjectBaseSyncParentNode				m_migrateParentNode;	// parent of migrate nodes

	// data nodes
	CMigrationDataNode						m_migrationNode;
	CGlobalFlagsDataNode					m_globalFlagsNode;
	CSectorDataNode							m_sectorNode;
	CSectorPositionDataNode					m_positionNode;
};

////////////////////////////////////////////////////////////////////////////
// CEntitySyncTreeBase - contains nodes common to all game entities
////////////////////////////////////////////////////////////////////////////
class CEntitySyncTreeBase : public CProximityMigrateableSyncTreeBase
{
public:

	CEntitySyncTreeBase(u32 sizeOfInitialStateBuffer);

	// node accessors (assert if not inserted into tree)
	netSyncParentNode*			GetEntityGameStateParentNode()     { gnetAssert(m_gameStateParentNode.GetParentTree()); return &m_gameStateParentNode; }

	virtual netSyncDataNode*	     GetOrientationNode()			{ return NULL; }
	CEntityScriptInfoDataNode*	     GetScriptInfoNode()			{ gnetAssert(m_entityScriptInfoNode.GetParentTree()); return &m_entityScriptInfoNode; }
	CEntityScriptGameStateDataNode*	 GetScriptGameStateNode()       { gnetAssert(m_entityScriptGameStateNode.GetParentTree()); return &m_entityScriptGameStateNode; }

protected:

	// parent nodes
	CProjectBaseSyncParentNode				m_infrequentParentNode; // parent of nodes that are dirtied infrequently
	CProjectBaseSyncParentNode				m_frequentParentNode;	// parent of nodes that are dirtied frequently
	CProjectBaseSyncParentNode				m_gameStateParentNode;
	CProjectBaseSyncParentNode				m_commonGameStateParentNode;
	CProjectBaseSyncParentNode				m_scriptGameStateParentNode;

	// data nodes
	CEntityScriptInfoDataNode				m_entityScriptInfoNode;
	CEntityScriptGameStateDataNode			m_entityScriptGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CDynamicEntitySyncTreeBase - contains nodes common to dynamic entities
////////////////////////////////////////////////////////////////////////////
class CDynamicEntitySyncTreeBase : public CEntitySyncTreeBase
{
public:

	CDynamicEntitySyncTreeBase(u32 sizeOfInitialStateBuffer);

protected:

	// data nodes
	CDynamicEntityGameStateDataNode			m_dynamicEntityGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CPhysicalSyncTreeBase - comtains nodes common to objects & vehicles
////////////////////////////////////////////////////////////////////////////
class CPhysicalSyncTreeBase : public CDynamicEntitySyncTreeBase
{
public:

	CPhysicalSyncTreeBase(u32 sizeOfInitialStateBuffer);

	// node accessors (assert if not inserted into tree)
	virtual CProjectBaseSyncDataNode*	    GetOrientationNode()				{  gnetAssert(m_orientationNode.GetParentTree()); return &m_orientationNode; }
	virtual netSyncDataNode*				GetVelocityNode()					{  gnetAssert(m_velocityNode.GetParentTree()); return &m_velocityNode; }
	CPhysicalAngVelocityDataNode*			GetAngVelocityNode()				{  gnetAssert(m_angVelocityNode.GetParentTree()); return &m_angVelocityNode; }
	CPhysicalGameStateDataNode*				GetPhysicalGameStateNode()			{  gnetAssert(m_physicalGameStateNode.GetParentTree()); return &m_physicalGameStateNode; }
	CPhysicalScriptGameStateDataNode*		GetPhysicalScriptGameStateNode()	{  gnetAssert(m_physicalScriptGameStateNode.GetParentTree()); return &m_physicalScriptGameStateNode; }
	CPhysicalAttachDataNode*				GetPhysicalAttachNode()				{  gnetAssert(m_attachNode.GetParentTree()); return &m_attachNode; }

protected:

	// data nodes
	CPhysicalMigrationDataNode				m_physicalMigrationNode;
	CPhysicalScriptMigrationDataNode		m_physicalScriptMigrationNode;
	CEntityOrientationDataNode				m_orientationNode;
	CPhysicalVelocityDataNode				m_velocityNode;
	CPhysicalAngVelocityDataNode			m_angVelocityNode;
	CPhysicalHealthDataNode					m_healthNode;
	CPhysicalAttachDataNode					m_attachNode;
	CPhysicalGameStateDataNode				m_physicalGameStateNode;
	CPhysicalScriptGameStateDataNode		m_physicalScriptGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CPedSyncTreeBase - contains nodes common to peds and players
////////////////////////////////////////////////////////////////////////////
class CPedSyncTreeBase : public CDynamicEntitySyncTreeBase
{
public:

	CPedSyncTreeBase(u32 sizeOfInitialStateBuffer);

    CPedMovementGroupDataNode*  GetPedMovementGroupDataNode()   { gnetAssert(m_movementGroupNode.GetParentTree()); return &m_movementGroupNode; }
    CPedMovementDataNode*       GetPedMovementDataNode()        { gnetAssert(m_movementNode.GetParentTree()); return &m_movementNode; }
	CPedGameStateDataNode*		GetPedGameStateNode()	        { gnetAssert(m_pedGameStateNode.GetParentTree()); return &m_pedGameStateNode; }
	CPedTaskTreeDataNode*		GetPedTaskTreeNode()	        { gnetAssert(m_taskTreeNode.GetParentTree()); return &m_taskTreeNode; }
	CPedHealthDataNode*			GetPedHealthNode()				{ gnetAssert(m_pedHealthNode.GetParentTree()); return &m_pedHealthNode; }
	CPedOrientationDataNode*	GetPedOrientationNode()			{ gnetAssert(m_orientationNode.GetParentTree()); return &m_orientationNode; }
	CPhysicalGameStateDataNode*	GetPhysicalGameStateNode()		{ gnetAssert(m_physicalGameStateNode.GetParentTree()); return &m_physicalGameStateNode; }

	CPedTaskSpecificDataNode*	GetPedTaskSpecificDataNode(u32 n) 
	{ 
		CPedTaskSpecificDataNode* pNode = NULL;

		if (AssertVerify(n<IPedNodeDataAccessor::NUM_TASK_SLOTS))
		{
			gnetAssert(m_taskSpecificDataNodes[n].GetParentTree()); 
			pNode = &m_taskSpecificDataNodes[n]; 
		}

		return pNode;
	}		  

	unsigned GetNumTaskSpecificDataNodes() const { return IPedNodeDataAccessor::NUM_TASK_SLOTS; }

	void ForceResendOfAllTaskData(netSyncTreeTargetObject* pObj);

protected:

	//PURPOSE
	//	Called after all nodes have had their data applied
	virtual void PostApplyData(netSyncTreeTargetObject* pObj);

protected:

	// parent nodes
	CProjectBaseSyncParentNode		        m_tasksParentNode;

	// data nodes
	CPhysicalMigrationDataNode				m_physicalMigrationNode;
	CPhysicalScriptMigrationDataNode		m_physicalScriptMigrationNode;
	CPhysicalGameStateDataNode				m_physicalGameStateNode;

	CPedOrientationDataNode					m_orientationNode;
	CPedMovementDataNode					m_movementNode;
	CPedAttachDataNode						m_attachNode;
	CPedMovementGroupDataNode				m_movementGroupNode;
	CPedAIDataNode							m_aiNode;
	CPedGameStateDataNode					m_pedGameStateNode;
	CPedComponentReservationDataNode		m_componentReservationNode;
	CPedHealthDataNode						m_pedHealthNode;
	CPedTaskTreeDataNode					m_taskTreeNode;
	CPedTaskSpecificDataNode				m_taskSpecificDataNodes[IPedNodeDataAccessor::NUM_TASK_SLOTS];
};

////////////////////////////////////////////////////////////////////////////
// CPedSyncTree
////////////////////////////////////////////////////////////////////////////
class CPedSyncTree : public CPedSyncTreeBase
{
public:

	CPedSyncTree(u32 sizeOfInitialStateBuffer);

    CPedSectorPosMapNode				*GetMapPositionNode()					{ gnetAssert(m_pedSectorPosMapNode.GetParentTree());     return &m_pedSectorPosMapNode;     }
    CPedSectorPosNavMeshNode			*GetNavMeshPositionNode()				{ gnetAssert(m_pedSectorPosNavMeshNode.GetParentTree()); return &m_pedSectorPosNavMeshNode; }
	CPhysicalScriptGameStateDataNode	*GetPhysicalScriptGameStateNode()		{ gnetAssert(m_physicalScriptGameStateNode.GetParentTree()); return &m_physicalScriptGameStateNode; }

    virtual bool GetPositionWasUpdated() const;
    virtual void DirtyPositionNodes(netSyncTreeTargetObject &targetObject);
    virtual void ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);
    virtual void ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player);
    virtual bool IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime);

	virtual netSyncDataNode*	     GetCreateNode() { return &m_pedCreateNode; }

protected:

	// data nodes
	CPedCreationDataNode					m_pedCreateNode;
    CPedScriptCreationDataNode              m_pedScriptCreateNode;
    CPedSectorPosMapNode                    m_pedSectorPosMapNode;
    CPedSectorPosNavMeshNode                m_pedSectorPosNavMeshNode;
	CPhysicalScriptGameStateDataNode		m_physicalScriptGameStateNode;
	CPedScriptGameStateDataNode				m_pedScriptGameStateNode;
    CPedAppearanceDataNode					m_pedAppearanceNode;
	CPedInventoryDataNode                   m_pedInventoryMigrationNode;
	CPedTaskSequenceDataNode				m_pedTaskSequenceMigrationNode;
};

////////////////////////////////////////////////////////////////////////////
// CPlayerSyncTree
////////////////////////////////////////////////////////////////////////////
class CPlayerSyncTree : public CPedSyncTreeBase
{
public:

	CPlayerSyncTree(u32 sizeOfInitialStateBuffer);

	// node accessors (assert if not inserted into tree)
    CPlayerSectorPosNode*               GetPlayerSectorPosNode()	{ gnetAssert(m_playerSectorPosNode.GetParentTree()); return &m_playerSectorPosNode; }
    CPlayerCameraDataNode*              GetPlayerCameraNode()		{ gnetAssert(m_cameraNode.GetParentTree()); return &m_cameraNode; }
	CPlayerAppearanceDataNode*			GetPlayerAppearanceNode()	{ gnetAssert(m_playerAppearanceNode.GetParentTree()); return &m_playerAppearanceNode; }
	CPlayerPedGroupDataNode*			GetPedGroupNode()			 { gnetAssert(m_playerPedGroupNode.GetParentTree()); return &m_playerPedGroupNode; }
	CPlayerGamerDataNode*				GetPlayerGamerDataNode()	 { gnetAssert(m_playerGamerDataNode.GetParentTree()); return &m_playerGamerDataNode; }
	CPlayerGameStateDataNode*			GetPlayerGameStateDataNode() { gnetAssert(m_playerGameStateNode.GetParentTree()); return &m_playerGameStateNode; }
	
    virtual bool GetPositionWasUpdated() const;
    virtual void DirtyPositionNodes(netSyncTreeTargetObject &targetObject);
    virtual void ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);
    virtual void ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player);
    virtual bool IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime);
	virtual void ForceSendOfGameStateDataNode(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);

	virtual netSyncDataNode*	     GetCreateNode() { return &m_playerCreateNode; }

protected:

	// data nodes
	CPlayerCreationDataNode					m_playerCreateNode;
    CPlayerSectorPosNode                    m_playerSectorPosNode;
	CPlayerCameraDataNode					m_cameraNode;
	CPlayerGameStateDataNode				m_playerGameStateNode;
	CPlayerAppearanceDataNode				m_playerAppearanceNode;
	CPlayerPedGroupDataNode					m_playerPedGroupNode;
	CPlayerWantedAndLOSDataNode				m_playerWantedAndLOSDataNode;
    CPlayerAmbientModelStreamingNode        m_playerAmbientModelStreamingNode;
	CPlayerGamerDataNode					m_playerGamerDataNode;
	CPlayerExtendedGameStateNode			m_playerExtendedGameStateNode;
};


////////////////////////////////////////////////////////////////////////////
// CObjectSyncTree
////////////////////////////////////////////////////////////////////////////
class CObjectSyncTree : public CPhysicalSyncTreeBase
{
public:

	CObjectSyncTree(u32 sizeOfInitialStateBuffer);

	virtual netSyncDataNode*	        GetCreateNode() { return &m_createNode; }
    virtual CProjectBaseSyncDataNode*   GetOrientationNode() { gnetAssert(m_objectOrientationNode.GetParentTree()); return &m_objectOrientationNode; }

    // node accessors (assert if not inserted into tree)
	CObjectSectorPosNode *GetObjectSectorPosNode()  { gnetAssert(m_objectSectorPosNode.GetParentTree()); return &m_objectSectorPosNode; }
	CObjectGameStateDataNode *GetObjectGameStateNode()  { gnetAssert(m_objectGameStateNode.GetParentTree()); return &m_objectGameStateNode; }

    virtual bool GetPositionWasUpdated() const;
    virtual void DirtyPositionNodes(netSyncTreeTargetObject &targetObject);
    virtual void ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);
    virtual void ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player);
    virtual bool IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime);
    void ForceSendOfScriptGameStateData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);

protected:

	// data nodes
	CObjectCreationDataNode					m_createNode;
    CObjectSectorPosNode                    m_objectSectorPosNode;
    CObjectOrientationNode                  m_objectOrientationNode;
	CObjectGameStateDataNode				m_objectGameStateNode;
	CObjectScriptGameStateDataNode			m_objectScriptGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CPickupSyncTree
////////////////////////////////////////////////////////////////////////////
class CPickupSyncTree : public CPhysicalSyncTreeBase
{
public:

	CPickupSyncTree(u32 sizeOfInitialStateBuffer);

	virtual netSyncDataNode*	     GetCreateNode()			{ return &m_createNode; }
	CPickupSectorPosNode*			 GetPickupSectorPosNode()   { gnetAssert(m_pickupSectorPosNode.GetParentTree()); return &m_pickupSectorPosNode; }

	virtual bool GetPositionWasUpdated() const;
	virtual void DirtyPositionNodes(netSyncTreeTargetObject &targetObject);
	virtual void ForceSendOfPositionData(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags);
	virtual void ForceSendOfPositionDataToPlayer(netSyncTreeTargetObject &targetObject, const ActivationFlags activationFlags, const PhysicalPlayerIndex player);
	virtual bool IsTimeToSendPositionUpdateToPlayer(netSyncTreeTargetObject *targetObject, const PhysicalPlayerIndex player, const unsigned currTime);

protected:

	// data nodes
	CPickupCreationDataNode					m_createNode;
	CPickupSectorPosNode                    m_pickupSectorPosNode;
	CPickupScriptGameStateNode				m_pickupScriptGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CPickupPlacementSyncTree
////////////////////////////////////////////////////////////////////////////
class CPickupPlacementSyncTree : public CProximityMigrateableSyncTreeBase
{
public:

	CPickupPlacementSyncTree();

	// node accessors (assert if not inserted into tree)
	CPickupPlacementStateDataNode*	GetStateNode()		{ gnetAssert(m_stateNode.GetParentTree()); return &m_stateNode; }

	virtual netSyncDataNode*	     GetCreateNode() { return &m_createNode; }

protected:

	// data nodes
	CPickupPlacementCreationDataNode					m_createNode;
	CPickupPlacementStateDataNode						m_stateNode;
};

////////////////////////////////////////////////////////////////////////////
// CGlassPaneSyncTree
////////////////////////////////////////////////////////////////////////////
class CGlassPaneSyncTree : public CProximityMigrateableSyncTreeBase
{
public:

	CGlassPaneSyncTree();

	virtual netSyncDataNode*	     GetCreateNode() { return &m_createNode; }

protected:

	// data nodes
	CGlassPaneCreationDataNode					m_createNode;
};

////////////////////////////////////////////////////////////////////////////
// CVehicleSyncTree - contains nodes common to all vehicles
////////////////////////////////////////////////////////////////////////////
class CVehicleSyncTree : public CPhysicalSyncTreeBase
{
public:

	CVehicleSyncTree(u32 sizeOfInitialStateBuffer);

	CVehicleProximityMigrationDataNode *GetProximityMigrationNode()     { gnetAssert(m_vehicleProximityMigrationNode.GetParentTree()); return &m_vehicleProximityMigrationNode; }
	CVehicleAngVelocityDataNode        *GetAngVelocityNode()            { gnetAssert(m_vehicleAngVelocityNode.GetParentTree());  return &m_vehicleAngVelocityNode; }
	CVehicleControlDataNode            *GetVehicleControlNode()         { gnetAssert(m_vehicleControlNode.GetParentTree()); return &m_vehicleControlNode; }
	CVehicleTaskDataNode               *GetVehicleTaskNode()            { gnetAssert(m_vehicleTaskNode.GetParentTree()); return &m_vehicleTaskNode; }
	CVehicleGameStateDataNode		   *GetVehicleGameStateNode()	    { gnetAssert(m_vehicleGameStateNode.GetParentNode()); return &m_vehicleGameStateNode; }
	CVehicleDamageStatusDataNode	   *GetVehicleDamageStatusNode()    { gnetAssert(m_damageStatusNode.GetParentNode()); return &m_damageStatusNode; }
	CVehicleGadgetDataNode             *GetVehicleGadgetNode()          { gnetAssert(m_vehicleGadgetNode.GetParentNode()); return &m_vehicleGadgetNode; }
	CVehicleHealthDataNode             *GetVehicleHealthNode()          { gnetAssert(m_vehicleHealthNode.GetParentNode()); return &m_vehicleHealthNode; }
	CVehicleScriptGameStateDataNode    *GetVehicleScriptGameStateNode() { gnetAssert(m_vehicleScriptGameStateNode.GetParentNode()); return &m_vehicleScriptGameStateNode; }
	

	virtual netSyncDataNode*	        GetCreateNode()                 { return &m_vehicleCreateNode; }

protected:

	// parent nodes
	CProjectBaseSyncParentNode				m_controlParentNode;

	// data nodes
	CVehicleCreationDataNode				m_vehicleCreateNode;
	CVehicleProximityMigrationDataNode		m_vehicleProximityMigrationNode;
    CVehicleAngVelocityDataNode             m_vehicleAngVelocityNode;
	CVehicleSteeringDataNode				m_steeringNode;
	CVehicleControlDataNode				    m_vehicleControlNode;
	CVehicleAppearanceDataNode				m_appearanceNode;
	CVehicleDamageStatusDataNode			m_damageStatusNode;
	CVehicleGameStateDataNode				m_vehicleGameStateNode;
    CVehicleHealthDataNode                  m_vehicleHealthNode;
	CVehicleScriptGameStateDataNode			m_vehicleScriptGameStateNode;
	CVehicleComponentReservationDataNode	m_componentReservationNode;
	CVehicleTaskDataNode					m_vehicleTaskNode;
	CVehicleGadgetDataNode					m_vehicleGadgetNode;
};

////////////////////////////////////////////////////////////////////////////
// CAutomobileSyncTreeBase
////////////////////////////////////////////////////////////////////////////
class CAutomobileSyncTreeBase : public CVehicleSyncTree
{
public:

	CAutomobileSyncTreeBase(u32 sizeOfInitialStateBuffer);

	virtual netSyncDataNode*	     GetCreateNode() { return &m_automobileCreateNode; }

protected:

	// data nodes
	CAutomobileCreationDataNode				m_automobileCreateNode;
};

////////////////////////////////////////////////////////////////////////////
// CAutomobileSyncTree
////////////////////////////////////////////////////////////////////////////
class CAutomobileSyncTree : public CAutomobileSyncTreeBase
{
public:

	CAutomobileSyncTree(u32 sizeOfInitialStateBuffer);
};

////////////////////////////////////////////////////////////////////////////
// CBikeSyncTree
////////////////////////////////////////////////////////////////////////////
class CBikeSyncTree : public CVehicleSyncTree
{
public:

	CBikeSyncTree(u32 sizeOfInitialStateBuffer);

private:

	// data nodes
	CBikeGameStateDataNode					m_bikeGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CBoatSyncTree
////////////////////////////////////////////////////////////////////////////
class CBoatSyncTree : public CVehicleSyncTree
{
public:

	CBoatSyncTree(u32 sizeOfInitialStateBuffer);

private:

	// data nodes
	CBoatGameStateDataNode					m_boatGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CHeliSyncTree
////////////////////////////////////////////////////////////////////////////
class CHeliSyncTree : public CAutomobileSyncTreeBase
{
public:

	CHeliSyncTree(u32 sizeOfInitialStateBuffer);

private:

	// data nodes
    CHeliHealthDataNode                     m_heliHealthDataNode;
    CHeliControlDataNode                    m_heliControlDataNode;
};

////////////////////////////////////////////////////////////////////////////
// CSubmarineSyncTree
////////////////////////////////////////////////////////////////////////////
class CSubmarineSyncTree : public CVehicleSyncTree
{
public:

	CSubmarineSyncTree(u32 sizeOfInitialStateBuffer);

private:

	//Data nodes
	CSubmarineControlDataNode        m_submarineControlNode;
    CSubmarineGameStateDataNode      m_submarineGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CTrainSyncTree
////////////////////////////////////////////////////////////////////////////
class CTrainSyncTree : public CVehicleSyncTree
{
public:

	CTrainSyncTree(u32 sizeOfInitialStateBuffer);

private:

    CTrainGameStateDataNode m_TrainGameStateNode;
};

////////////////////////////////////////////////////////////////////////////
// CPlaneSyncTree
////////////////////////////////////////////////////////////////////////////
class CPlaneSyncTree : public CVehicleSyncTree
{
public:

	CPlaneSyncTree(u32 sizeOfInitialStateBuffer);

private:

	CPlaneGameStateDataNode	 m_PlaneGameStateNode;
	CPlaneControlDataNode	 m_PlaneControlDataNode;
};

////////////////////////////////////////////////////////////////////////////
// CDoorSyncTree
////////////////////////////////////////////////////////////////////////////
class CDoorSyncTree : public CPhysicalSyncTreeBase
{
public:

	CDoorSyncTree();

	virtual netSyncDataNode *GetVelocityNode() { return NULL; }

	virtual netSyncDataNode*	     GetCreateNode() { return &m_createNode; }

protected:

	CDoorCreationDataNode				m_createNode;
	CDoorMovementDataNode				m_doorMovementNode;
	CDoorScriptInfoDataNode				m_doorScriptInfoNode;
	CDoorScriptGameStateDataNode		m_doorScriptGameStateNode;

};



#endif  // PROJECT_SYNC_TREES_H
