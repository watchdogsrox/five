//
// name:        CNetObjBike.h
// description: Derived from netObject, this class handles all bike-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJBIKE_H
#define NETOBJBIKE_H

#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

class CNetObjBike : public CNetObjVehicle, public IBikeNodeDataAccessor
{
public:

    CNetObjBike(class CBike					*bike,
                const NetworkObjectType		type,
                const ObjectId				objectID,
                const PhysicalPlayerIndex   playerIndex,
                const NetObjFlags			localFlags,
                const NetObjFlags			globalFlags);

    CNetObjBike() {SetObjectType(NET_OBJ_TYPE_BIKE);}

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_BIKE" : "BIKE"; }

    CBike* GetBike() const { return (CBike*)GetGameObject(); }

    float GetLeanAngle() const;

    static void CreateSyncTree();
    static void DestroySyncTree();

    static  CProjectSyncTree*       GetStaticSyncTree() { return ms_bikeSyncTree; }
    virtual CProjectSyncTree*       GetSyncTree()       { return ms_bikeSyncTree; }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool        ProcessControl();

    void GetBikeGameState(bool &isOnSideStand);
    void SetBikeGameState(const bool onSideStand);

private:

    static CBikeSyncTree *ms_bikeSyncTree;
};

#endif  // NETOBJBIKE_H
