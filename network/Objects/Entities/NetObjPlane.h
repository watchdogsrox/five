//
// name:		NetObjPlane.h
// description:	Derived from netObject, this class handles all plane-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#ifndef NETOBJPLANE_H
#define NETOBJPLANE_H

#include "network/objects/entities/NetObjAutomobile.h"

struct CHeliBlenderData;

class CNetObjPlane : public CNetObjAutomobile, public IPlaneNodeDataAccessor
{
public:

	struct CPlaneScopeData : public CVehicleScopeData
	{
		CPlaneScopeData()
		{
			m_scopeDistance		= 500.0f;
			m_syncDistanceFar   = 400.0f;
		}
	};

public:

	CNetObjPlane(class CPlane					*plane,
					const NetworkObjectType		type,
					const ObjectId				objectID,
					const PhysicalPlayerIndex   playerIndex,
					const NetObjFlags			localFlags,
					const NetObjFlags			globalFlags);

    CNetObjPlane() :
    m_LockOnTargetID(NETWORK_INVALID_OBJECT_ID)
    , m_LockOnState(0)
    , m_LastPlaneThrottleReceived(0.0f)
	, m_CloneDoorsBroken(0)
	, m_planeBroken(0)
	, m_brake(0.0f)
    {
        SetObjectType(NET_OBJ_TYPE_PLANE);
    }

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_PLANE" : "PLANE"; }

	CPlane* GetPlane() const { return (CPlane*)GetGameObject(); }

	virtual netScopeData&      GetScopeData()       { return ms_planeScopeData; }
	static  netScopeData&      GetStaticScopeData() { return ms_planeScopeData; }
	virtual netScopeData&      GetScopeData() const { return ms_planeScopeData; }
    
	static void CreateSyncTree();
    static void DestroySyncTree();

    static  CProjectSyncTree*       GetStaticSyncTree() { return ms_planeSyncTree; }
    virtual CProjectSyncTree*       GetSyncTree()       { return ms_planeSyncTree; }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool Update();
	virtual bool ProcessControl();
	virtual void CreateNetBlender();
    virtual void ChangeOwner(const netPlayer& player, eMigrationType migrationType);

	// uses heli blender for now
	static CHeliBlenderData *ms_planeBlenderData;
   
	// sync parser get functions
    void GetPlaneGameState(CPlaneGameStateDataNode& data);

    void GetPlaneControlData(CPlaneControlDataNode& data);

		// sync parser setter functions
    void SetPlaneGameState(const CPlaneGameStateDataNode& data);

	void SetPlaneControlData(const CPlaneControlDataNode& data);

	void SetCloneDoorsBroken (const u8 doorsBroken) { m_CloneDoorsBroken = doorsBroken; }
	u8   GetCloneDoorsBroken () const { return m_CloneDoorsBroken; }
	void UpdateCloneDoorsBroken ();


protected:

    virtual float GetPlayerDriverScopeDistanceMultiplier() const;

	// important planes always have a high update level
	virtual u8 GetLowestAllowedUpdateLevel() const { return IsImportant() ? CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH : CNetObjAutomobile::GetLowestAllowedUpdateLevel(); }

	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;
    virtual bool CheckBoxStreamerBeforeFixingDueToDistance() const { return true; }

    virtual bool ShouldOverrideBlenderWhenStoppedPredicting() const;

	virtual float GetForcedMigrationRangeWhenPlayerTransitioning();

public:
	static const unsigned MAX_NUM_SYNCED_PROPELLERS = 8;

private:

    void UpdateLockOnTarget();

	static CPlaneScopeData   ms_planeScopeData;
	    
	static CPlaneSyncTree *ms_planeSyncTree;

    ObjectId m_LockOnTargetID;
    unsigned m_LockOnState;
    float    m_LastPlaneThrottleReceived;
	float	 m_brake;
	u8		 m_CloneDoorsBroken;  //B*1931220 planes need to store the synced door broken state so they can reapplied after the health node is applied
	u8		 m_planeBroken;
	bool	 m_HasActiveAITask;
};

#endif  // NETOBJPLANE_H
