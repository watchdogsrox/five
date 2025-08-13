//
// name:		CNetObjSubmarine.h
// description:	Derived from netObject, this class handles all plane-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	
//

#ifndef NETOBJSUBMARINE_H
#define NETOBJSUBMARINE_H

#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

class CNetObjSubmarine : public CNetObjVehicle, public ISubmarineNodeDataAccessor
{
public:

	CNetObjSubmarine(class CSubmarine*          submarine
					,const NetworkObjectType    type
					,const ObjectId             objectID
					,const PhysicalPlayerIndex  playerIndex
					,const NetObjFlags          localFlags
					,const NetObjFlags          globalFlags);

    CNetObjSubmarine() { SetObjectType(NET_OBJ_TYPE_SUBMARINE); }

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_SUBMARINE" : "SUBMARINE"; }

	//Return the submersible.
	CSubmarine* GetSubmarine() const { return (CSubmarine*)GetGameObject(); }

	//Creates a new sync data for this object.
	static void CreateSyncTree();
	//Destroys the sync data for this object.
	static void DestroySyncTree();
	//Returns the sync tree used by the network object
	virtual CProjectSyncTree*  GetSyncTree()        { return ms_submarineSyncTree; }
	static  CProjectSyncTree*  GetStaticSyncTree()  { return ms_submarineSyncTree; }

	//Returns a reference to the data accessor for this class.
	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	//Sync tree functions
	virtual void GetSubmarineControlData(CSubmarineControlDataNode& data);
	virtual void SetSubmarineControlData(const CSubmarineControlDataNode& data);
    virtual void GetSubmarineGameState(CSubmarineGameStateDataNode& data);
    virtual void SetSubmarineGameState(const CSubmarineGameStateDataNode& data);

	//Handles process control on game object
	virtual bool  ProcessControl();
    virtual void  ChangeOwner(const netPlayer& player, eMigrationType migrationType);

private:

    // cached anchored flag - Clones are never anchored as it causes problems with the prediction code
    // - but we need to restore the correct value if we gain ownership of an anchored submarine
    bool m_IsAnchored;

	//Our submarine sync Tree
	static CSubmarineSyncTree*  ms_submarineSyncTree;
};

#endif  // NETOBJSUBMARINE_H
