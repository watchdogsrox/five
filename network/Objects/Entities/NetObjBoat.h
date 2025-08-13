//
// name:        CNetObjBoat.h
// description: Derived from netObject, this class handles all boat-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJBOAT_H
#define NETOBJBOAT_H

#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

struct CBoatBlenderData;

class CNetObjBoat : public CNetObjVehicle, public IBoatNodeDataAccessor
{
public:

    struct CBoatScopeData : public netScopeData
    {
        CBoatScopeData()
        {
			m_scopeDistance				= 300.0f;
			m_scopeDistanceInterior     = 50.0f;
            m_syncDistanceNear			= 10.0;
            m_syncDistanceFar			= 60.0f;
        }
    };

    CNetObjBoat(class CBoat				*boat,
                const NetworkObjectType type,
                const ObjectId          objectID,
                PhysicalPlayerIndex     playerIndex,
                NetObjFlags             localFlags,
                const NetObjFlags       globalFlags);

    CNetObjBoat() {SetObjectType(NET_OBJ_TYPE_BOAT);}

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_BOAT" : "BOAT"; }

    CBoat* GetBoat() const { return (CBoat*)GetGameObject(); }

    bool IsAnchored() const { return m_bLockedToXY; }

    static void CreateSyncTree();
    static void DestroySyncTree();

    static  CProjectSyncTree*       GetStaticSyncTree()  { return ms_boatSyncTree; }
    virtual CProjectSyncTree*       GetSyncTree()        { return ms_boatSyncTree; }
    virtual netScopeData&      GetScopeData()       { return ms_boatScopeData; }
    static  netScopeData&      GetStaticScopeData() { return ms_boatScopeData; }
    virtual netScopeData&      GetScopeData() const { return ms_boatScopeData; }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool        ProcessControl();
    virtual void        CreateNetBlender();
    virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationType);
    virtual bool        IsOnGround();

    static CBoatBlenderData *ms_boatBlenderData;

    // sync parser getter functions
    void GetBoatGameState(CBoatGameStateDataNode& data);

    // sync parser setter functions
    void SetBoatGameState(const CBoatGameStateDataNode& data);

private:
	
    static CBoatSyncTree   *ms_boatSyncTree;
    static CBoatScopeData   ms_boatScopeData;
};

#endif  // NETOBJBOAT_H
