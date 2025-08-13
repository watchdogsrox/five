//
// name:        NetObjHeli.h
// description: Derived from netObject, this class handles all helicopter-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJHELI_H
#define NETOBJHELI_H

#include "network/objects/entities/NetObjAutomobile.h"

struct CHeliBlenderData;

class CNetObjHeli : public CNetObjAutomobile, public IHeliNodeDataAccessor
{
public:

	struct CHeliScopeData : public CVehicleScopeData
	{
		CHeliScopeData()
		{
			m_scopeDistance		= 500.0f;
			m_syncDistanceFar   = 400.0f;
		}
	};

public:

    CNetObjHeli(class CHeli					*heli,
                const NetworkObjectType		type,
                const ObjectId				objectID,
                const PhysicalPlayerIndex   playerIndex,
                const NetObjFlags			localFlags,
                const NetObjFlags			globalFlags);

    CNetObjHeli() :
    m_LastHeliThrottleReceived(0.0f)
	{
		SetObjectType(NET_OBJ_TYPE_HELI);
	}

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_HELI" : "HELI"; }

    CHeli* GetHeli() const { return (CHeli*)GetGameObject(); }

	static void CreateSyncTree();
    static void DestroySyncTree();

	virtual netScopeData&		GetScopeData()       { return ms_heliScopeData; }
	virtual netScopeData&		GetScopeData() const { return ms_heliScopeData; }
	static  netScopeData&		GetStaticScopeData() { return ms_heliScopeData; }
    static  CProjectSyncTree*   GetStaticSyncTree()  { return ms_heliSyncTree; }
    virtual CProjectSyncTree*   GetSyncTree()        { return ms_heliSyncTree; }
	virtual void				ChangeOwner(const netPlayer& player, eMigrationType migrationTypee);

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool Update();
    virtual bool ProcessControl();
    virtual void CreateNetBlender();
  
    static CHeliBlenderData *ms_heliBlenderData;

    // sync parser setter functions
    void GetHeliHealthData(CHeliHealthDataNode& data);
    void GetHeliControlData(CHeliControlDataNode& data);
 
    // sync parser setter functions
    void SetHeliHealthData(const CHeliHealthDataNode& data);
    void SetHeliControlData(const CHeliControlDataNode& data);
 
	void SetIsCustomHealthHeli(bool val) { m_IsCustomHealthHeli = val; }

protected:

    virtual float GetPlayerDriverScopeDistanceMultiplier() const;

	// important helis always have a high update level
	virtual u8 GetLowestAllowedUpdateLevel() const { return IsImportant() ? CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH : CNetObjAutomobile::GetLowestAllowedUpdateLevel(); }

	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;
    virtual bool CheckBoxStreamerBeforeFixingDueToDistance() const { return true; }

    virtual bool ShouldOverrideBlenderWhenStoppedPredicting() const;

	virtual float GetForcedMigrationRangeWhenPlayerTransitioning();

private:

    static CHeliSyncTree	*ms_heliSyncTree;
	static CHeliScopeData    ms_heliScopeData;

    float m_LastHeliThrottleReceived;
	bool  m_HasActiveAITask;
	bool  m_IsCustomHealthHeli;
};

#endif  // NETOBJHELI_H
