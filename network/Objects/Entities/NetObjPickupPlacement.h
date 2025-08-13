//
// name:		NetObjPickupPlacement.h
// description:	Derived from netObject, this class handles all pickup placement related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#ifndef NETOBJPICKUPPLACEMENT_H
#define NETOBJPICKUPPLACEMENT_H

#include "network/objects/entities/NetObjProximityMigrateable.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

class CPickupPlacement;

class CNetObjPickupPlacement : public CNetObjProximityMigrateable, public IPickupPlacementNodeDataAccessor
{
public:

	FW_REGISTER_CLASS_POOL(CNetObjPickupPlacement);

	struct CPickupPlacementScopeData : public netScopeData
	{
		CPickupPlacementScopeData()
		{
			m_scopeDistance			= 150.0f; // CPickupPlacement::PICKUP_GENERATION_RANGE + 15.0f;
			m_scopeDistanceInterior = 50.0f;
			m_syncDistanceNear		= 10.0;
			m_syncDistanceFar		= 60.0f;
		}
	};

	static const int PLACEMENT_SYNC_DATA_NUM_NODES = 2;
	static const int PLACEMENT_SYNC_DATA_BUFFER_SIZE = 9;

	class CPickupPlacementSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, PLACEMENT_SYNC_DATA_NUM_NODES, PLACEMENT_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
	{
	public:
        FW_REGISTER_CLASS_POOL(CPickupPlacementSyncData);
    };

public:

	CNetObjPickupPlacement(class CPickupPlacement		*placement,
					       const NetworkObjectType		type,
					       const ObjectId				objectID,
					       const PhysicalPlayerIndex    playerIndex,
					       const NetObjFlags			localFlags,
					       const NetObjFlags			globalFlags);

    virtual const char *GetObjectTypeName() const { return IsScriptObject() ? "SCRIPT_PICKUP_PLACEMENT" : "PICKUP_PLACEMENT"; }

	CPickupPlacement* GetPickupPlacement() const		{ return (CPickupPlacement*)GetGameObject(); }

	virtual bool IsInInterior() const;
	virtual Vector3 GetPosition() const;
	void SetPosition( const Vector3& pos );

    static void CreateSyncTree();
    static void DestroySyncTree();

	static  CProjectSyncTree*	GetStaticSyncTree()  { return ms_pickupPlacementSyncTree; }
	virtual CProjectSyncTree*	GetSyncTree()		 { return ms_pickupPlacementSyncTree; }
	virtual netScopeData&		GetScopeData()		 { return ms_pickupPlacementScopeData; }
	virtual netScopeData&		GetScopeData() const { return ms_pickupPlacementScopeData; }
    static  netScopeData&		GetStaticScopeData() { return ms_pickupPlacementScopeData; }
	virtual netSyncDataBase*	CreateSyncData()	 { return rage_new CPickupPlacementSyncData(); }

	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	virtual const scriptObjInfoBase*	GetScriptObjInfo() const;
	virtual void						SetScriptObjInfo(scriptObjInfoBase* info);
	virtual scriptHandlerObject*		GetScriptHandlerObject() const;

	virtual	bool	Update();
	virtual float	GetScopeDistance(const netPlayer* pRelativePlayer = NULL) const;
	virtual bool	IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual void    CleanUpGameObject(bool bDestroyObject);
	virtual bool	CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual bool    CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual void    ChangeOwner(const netPlayer& player, eMigrationType migrationType);
    virtual void    LogAdditionalData(netLoggingInterface &log) const;
	virtual bool	CanPassControlWithNoGameObject() const { return m_MapPickup; } 
	virtual bool	TryToPassControlOutOfScope();
	virtual void	ConvertToAmbientObject();
	virtual bool    IsInSameInterior(netObject&) const { return true; } // this isn't used for pickups
	virtual bool	CanDelete(unsigned* reason = nullptr);

	// don't migrate to host so collection works better
	virtual bool	FavourMigrationToHost() const { return false; }

	// sets the collector of this placement
	void SetCollector(CPed* pCollector);

	// sync parser getter functions
	void   GetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data)  const;
	void   GetPickupPlacementStateData(CPickupPlacementStateDataNode& data) const;

    // sync parser setter functions
	void   SetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data);
	void   SetPickupPlacementStateData(const CPickupPlacementStateDataNode& data);

	// specify what player machines we want this pickup cloned on 
	//void SetPlayersAvailability(int players) { m_CloneToPlayersList = players; }

	// displays debug info above the network object
	virtual void DisplayNetworkInfo();

protected:

	static CPickupPlacementSyncTree	   *ms_pickupPlacementSyncTree;
	static CPickupPlacementScopeData	ms_pickupPlacementScopeData;

	ObjectId	m_collector;			// object ID of the ped who collected the pickup
	u32			m_regenerationTime;	// the cached regen time, used if the placement becomes local
	bool		m_MapPickup;
	//PlayerFlags m_CloneToPlayersList;	// specifies what player machines we want this pickup cloned on 

};

#endif  // NETOBJPICKUPPLACEMENT_H
