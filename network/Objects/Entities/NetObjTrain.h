//
// name:        NetObjTrain.h
// description: Derived from netObject, this class handles all train-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJTRAIN_H
#define NETOBJTRAIN_H

#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

class CNetObjTrain : public CNetObjVehicle, public ITrainNodeDataAccessor
{
public:

	struct CTrainScopeData : public CVehicleScopeData
	{
		CTrainScopeData()
		{
			m_scopeDistance				= 800.0f;
			m_scopeDistanceInterior     = m_scopeDistance;
			m_syncDistanceNear			= 100.0;
			m_syncDistanceFar			= 300.0f;
		}
	};

    CNetObjTrain(class CTrain* pTrain,
                    const NetworkObjectType type,
                    const ObjectId objectID,
                    const PhysicalPlayerIndex playerIndex,
                    const NetObjFlags localFlags,
                    const NetObjFlags globalFlags);

    CNetObjTrain() :
    m_PendingEngineID(NETWORK_INVALID_OBJECT_ID),
	m_PendingLinkedToBackwardID(NETWORK_INVALID_OBJECT_ID),
	m_PendingLinkedToForwardID(NETWORK_INVALID_OBJECT_ID),
	m_nextReportingTime(0)
    { SetObjectType(NET_OBJ_TYPE_TRAIN);}

	virtual ~CNetObjTrain();

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_TRAIN" : "TRAIN"; }

    CTrain* GetTrain() const { return (CTrain*)GetGameObject(); }

    //Creates a new sync data for this object.
	static void CreateSyncTree();
	//Destroys the sync data for this object.
	static void DestroySyncTree();
	//Returns the sync tree used by the network object
	virtual CProjectSyncTree *GetSyncTree()        { return ms_TrainSyncTree; }
	static  CProjectSyncTree *GetStaticSyncTree()  { return ms_TrainSyncTree; }

	virtual netScopeData&      GetScopeData()       { return ms_trainScopeData; }
	static  netScopeData&      GetStaticScopeData() { return ms_trainScopeData; }
	virtual netScopeData&      GetScopeData() const { return ms_trainScopeData; }

    //Returns a reference to the data accessor for this class.
	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool Update();
	virtual void CreateNetBlender();
	virtual void UpdateBlenderData();
	virtual bool ProcessControl();
    virtual bool CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual void PostCreate();

	virtual void ChangeOwner(const netPlayer& player, eMigrationType migrationType);

	virtual void PlayerHasJoined(const netPlayer& player);

	virtual void TryToPassControlProximity();
	virtual bool TryToPassControlOutOfScope();

    virtual void GetTrainGameState(CTrainGameStateDataNode& data) const;

	virtual void SetTrainGameState(const CTrainGameStateDataNode& data);

	void SetUseHighPrecisionBlending(bool useHighPrecisionBlending);

	// network blender data
	static CVehicleBlenderData *ms_trainBlenderData;
	static CVehicleBlenderData *ms_trainHighPrecisionBlenderData;

private:

    virtual bool CanBlendWhenFixed() const;
	virtual bool IsInHeightScope(const netPlayer&, const Vector3&, const Vector3&, unsigned*) const { return true; }

    void UpdatePendingEngine();

	static CTrainSyncTree *ms_TrainSyncTree;

	static CTrainScopeData   ms_trainScopeData;

    ObjectId m_PendingEngineID;
	ObjectId m_PendingLinkedToBackwardID;
	ObjectId m_PendingLinkedToForwardID;
	u32      m_nextReportingTime;
	bool     m_UseHighPrecisionBlending;
};

#endif  // NETOBJTRAIN_H
