//
// name:		NetObjDoor.h
// description:	Derived from netObject, this class handles network of the free moving hinged DOOR_TYPE_STD type
//				door objects. See NetworkObject.h for a full description of all the methods.
//

#ifndef NETOBJDOOR_H
#define NETOBJDOOR_H

#include "fwnet/netobject.h"
#include "network/objects/entities/NetObjObject.h"
#include "network/objects/entities/NetObjPhysical.h"
#include "network/objects/entities/NetObjProximityMigrateable.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "script/Handlers/GameScriptEntity.h"

// framework includes
#include "fwscript/scripthandler.h"

class CDoor;
class CDoorSystemData;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetObjDoor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CNetObjDoor : public CNetObjObject, public IDoorNodeDataAccessor
{
public:
	FW_REGISTER_CLASS_POOL(CNetObjDoor);

	struct CDoorScopeData : public netScopeData
	{
		CDoorScopeData()
		{
			m_scopeDistance			= OBJECT_POPULATION_RESET_RANGE-5.0f;
			m_scopeDistanceInterior = 50.0f;
			m_syncDistanceNear		= 10.0;
			m_syncDistanceFar		= 35.0f;
		}
	};

	static const int DOOR_SYNC_DATA_NUM_NODES = 4;  
	static const int DOOR_SYNC_DATA_BUFFER_SIZE = 41;  

    class CDoorSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, DOOR_SYNC_DATA_NUM_NODES, DOOR_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
	{
	public:
		FW_REGISTER_CLASS_POOL(CDoorSyncData);
	};

	static const u32 TIME_BETWEEN_OUT_OF_SCOPE_MIGRATION_ATTEMPTS = 1000;

	class CDoorGameObjectWrapper
	{
	public:
		CDoorGameObjectWrapper() : m_bHasDoor(false), m_door(NULL) {}

		void SetDoor(CDoor& door)
		{
			m_door = &door;
			m_bHasDoor = true;
		}

		void SetDoorData(CDoorSystemData& doorData)
		{
			m_doorData = &doorData;
			m_bHasDoor = false;
		}

		void Clear() { m_bHasDoor = false; m_door = NULL; }

		CDoor* GetDoor() const					{ return m_bHasDoor ? m_door : NULL; }
		CDoorSystemData* GetDoorData() const	{ return m_bHasDoor ? NULL : m_doorData; }

	private:

		bool m_bHasDoor;

		union 
		{ 
			CDoorSystemData* m_doorData;
			CDoor* m_door;
		};
	};

	CNetObjDoor(	class CDoor *				door,                             
					const NetworkObjectType		type,
					const ObjectId				objectID,
					const PhysicalPlayerIndex	playerIndex,
					const NetObjFlags			localFlags,
					const NetObjFlags			globalFlags);

	CNetObjDoor() { SetObjectType(NET_OBJ_TYPE_DOOR); }
	~CNetObjDoor();

	virtual const char *GetObjectTypeName() const { return IsScriptObject() ? "SCRIPT_DOOR" : "DOOR"; }

	virtual CEntity* GetEntity() const { return (CEntity*)m_gameObjectWrapper.GetDoor(); }

	CDoor* GetDoor() const { return m_gameObjectWrapper.GetDoor(); }
	
	void AssignDoor(CDoor* pDoor);

	virtual Vector3 GetPosition() const { return GetEntity() ? CNetObjObject::GetPosition() : m_Position; }
	float GetTargetOpenRatio() const	{ return m_fTargetOpenRatio; }

	bool IsOpening() const		{ return m_bOpening; }
	bool IsFullyOpen() const;
	bool IsClosed() const;

	void SetDoorSystemData(CDoorSystemData* data);
	CDoorSystemData* GetDoorSystemData() const		{ return m_pDoorSystemData; }
	u32 GetDoorSystemEnumHash() const { return m_doorSystemHash; }

	virtual bool Update();
	virtual void UpdateBeforePhysics();

	void  ValidateGameObject(void*) const {}

	static void CreateSyncTree();
	static void DestroySyncTree();

	// sync parser getter functions
	void GetDoorCreateData(CDoorCreationDataNode& data) const;
	void GetDoorScriptInfo(CDoorScriptInfoDataNode& data) const;
	void GetDoorScriptGameState(CDoorScriptGameStateDataNode& data) const;
	void GetDoorMovementData(CDoorMovementDataNode& data) const;

	// sync parser setter functions
	void SetDoorCreateData(const CDoorCreationDataNode& data);
	void SetDoorScriptInfo(CDoorScriptInfoDataNode& data);
	void SetDoorScriptGameState(const CDoorScriptGameStateDataNode& data);
	void SetDoorMovementData(const CDoorMovementDataNode& data);

	virtual scriptObjInfoBase*          GetScriptObjInfo();
	virtual const scriptObjInfoBase*    GetScriptObjInfo() const;
	virtual void                        SetScriptObjInfo(scriptObjInfoBase* info);
	virtual scriptHandlerObject*        GetScriptHandlerObject() const { return (scriptHandlerObject*) m_pDoorSystemData; }

	//Returns the sync tree used by the network object
	virtual CProjectSyncTree*  GetSyncTree()        { return ms_doorSyncTree; }
	static  CProjectSyncTree*  GetStaticSyncTree()  { return ms_doorSyncTree; }
	virtual netScopeData&      GetScopeData()       { return ms_doorScopeData; }
	static  netScopeData&      GetStaticScopeData() { return ms_doorScopeData; }
	virtual netScopeData&      GetScopeData() const { return ms_doorScopeData; }

	virtual netSyncDataBase*   CreateSyncData()		{ return rage_new CDoorSyncData(); }
	virtual bool			   CanSynchronise(bool bOnRegistration) const { return CNetObjGame::CanSynchronise(bOnRegistration); }
	virtual void		 	   StartSynchronising() { CNetObjGame::StartSynchronising(); }
	virtual void			   ChangeOwner(const netPlayer& player, eMigrationType migrationType);
	virtual void			   CreateNetBlender()   {} 
	virtual void			   PostSync() {}
	virtual void			   OnRegistered();
	virtual void			   OnUnregistered();
	virtual void			   CleanUpGameObject(bool bDestroyObject);
	virtual void			   LogAdditionalData(netLoggingInterface &UNUSED_PARAM(log)) const {}
	virtual void			   UpdateAfterAllMovement() {}
	virtual void			   ConvertToAmbientObject();
	virtual bool			   CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual void			   TryToPassControlProximity() {} // doors do not migrate by proximity
	virtual bool			   TryToPassControlOutOfScope();
	virtual void			   HideForCutscene() {} 
	virtual void			   ExposeAfterCutscene() {}
	virtual bool			   OnReassigned();

	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	// displays debug info above the network object
	virtual void	DisplayNetworkInfo();
/*
#if __BANK

#define		MAX_DOOR_NAME_LEN	 128
	static char		m_NameOfDoor[MAX_DOOR_NAME_LEN];
	static Vector3	m_UnstreamedDoorPoint;
	static bool		ms_SetUnstreamedInDoorLocked;
	static float	ms_AutomaticDist;
	static float	ms_AutomaticRate;

	//PURPOSE
	// Adds debug widgets to test locked doors
	static void AddDebugWidgets();

	//PURPOSE
	// Displays debug text showing all locked doors
	static void DisplayDebugText();
#endif // __BANK
*/
protected:

	virtual bool AllowFixByNetwork() const { return false; }
	virtual void ProcessFixedByNetwork(bool) {}

	static CDoorSyncTree*  ms_doorSyncTree;
	static CDoorScopeData  ms_doorScopeData;

	bool ValidateDoorSystemData(const char* reason, bool assertOnNoNetObject = true, bool fullValidation = false);

private:

	u32		m_doorSystemHash;	// The hash of the door system entry for this door
	u32     m_ModelHash;		// Hash of the model name for the door
	Vector3 m_Position;			// Position of the door on the map
	float	m_fTargetOpenRatio;	
	u32		m_closedTimer;		// how long the door has been closed for
	int		m_migrationTimer;	// used to pass control out of scope

	CDoorGameObjectWrapper m_gameObjectWrapper; // contains ptrs to either a CDoor or a CDoorSystemData

	CDoorSystemData* m_pDoorSystemData;	// a ptr to the door's data in the door system

	bool	m_bFlagForUnregistration : 1;
	bool    m_bOpening : 1;					// true if the door is opening (only used by garage and sliding doors)
	bool    m_bFullyOpen : 1;				// true if the door is fully open (only used by garage and sliding doors)
	bool    m_bClosed : 1;					// true if the door is closed(only used by garage and sliding doors)
};

#endif  //#NETOBJDOOR_H

