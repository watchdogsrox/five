//
// name:        NetObjAutomobile.h
// description: Derived from netObject, this class handles all automobile-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJAUTOMOBILE_H
#define NETOBJAUTOMOBILE_H

#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

class CNetObjAutomobile : public CNetObjVehicle, public IAutomobileNodeDataAccessor
{
public:

    CNetObjAutomobile(class CAutomobile			*automobile,
                      const NetworkObjectType	type,
                      const ObjectId			objectID,
                      const PhysicalPlayerIndex playerIndex,
                      const NetObjFlags			localFlags,
                      const NetObjFlags			globalFlags);

    CNetObjAutomobile() {SetObjectType(NET_OBJ_TYPE_AUTOMOBILE);}

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_AUTOMOBILE" : "AUTOMOBILE"; }

    CAutomobile* GetAutomobile() const { return (CAutomobile*)GetGameObject(); }

    static void CreateSyncTree();
    static void DestroySyncTree();

    // see NetworkObject.h for a description of these functions
    static  CProjectSyncTree*       GetStaticSyncTree() { return ms_autoSyncTree; }
    virtual CProjectSyncTree*       GetSyncTree()       { return ms_autoSyncTree; }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool        ProcessControl();

    // sync parser creation functions
    void GetAutomobileCreateData(CAutomobileCreationDataNode& data);
    void SetAutomobileCreateData(const CAutomobileCreationDataNode& data);


protected:

    static CAutomobileSyncTree  *ms_autoSyncTree;
};

#endif  // NETOBJAUTOMOBILE_H
