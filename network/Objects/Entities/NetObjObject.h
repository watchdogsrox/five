//
// name:		NetObjObject.h
// description:	Derived from netObject, this class handles all object-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#ifndef NETOBJOBJECT_H
#define NETOBJOBJECT_H

#include "network/objects/entities/NetObjPhysical.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "objects/objectpopulation.h"
#include "shaders/CustomShaderEffectTint.h"

class CNetObjObject : public CNetObjPhysical, public IObjectNodeDataAccessor
{
public:

	FW_REGISTER_CLASS_POOL(CNetObjObject);

	struct CObjectScopeData : public netScopeData
	{
		CObjectScopeData()
		{
			m_scopeDistance			= OBJECT_POPULATION_RESET_RANGE; 
			m_scopeDistanceInterior = 50.0f;
            m_scopeDistanceHeight   = 150.0f;
			m_syncDistanceNear		= 10.0;
			m_syncDistanceFar		= 60.0f;
		}

        float m_scopeDistanceHeight;
	};

	struct sUnassignedAmbientObject
	{
		CNetObjObject* pObject;
		Vector3		   position;
	};

#if __BANK
	// debug update rate that can be forced on local player via the widgets
	static s32 ms_updateRateOverride;
#endif

	static const int OBJECT_SYNC_DATA_NUM_NODES = 16;
	static const int OBJECT_SYNC_DATA_BUFFER_SIZE = 274;

	class CObjectSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, OBJECT_SYNC_DATA_NUM_NODES, OBJECT_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
	{
	public:
        FW_REGISTER_CLASS_POOL(CObjectSyncData);
    };

	static const bool ALLOW_MULTIPLE_NETWORKED_FRAGS_PER_OBJECT = false;

	CNetObjObject(class CObject				*object,
                  const NetworkObjectType	type,
                  const ObjectId			objectID,
                  const PhysicalPlayerIndex playerIndex,
                  const NetObjFlags         localFlags,
                  const NetObjFlags         globalFlags);

    CNetObjObject() :
    m_registeredWithKillTracker(false)
    , m_bCloneAsTempObject(false)
    , m_UseHighPrecisionBlending(false)
	, m_UseParachuteBlending(false)
    , m_UsingScriptedPhysicsParams(false)
	, m_DisableBreaking(false)
	, m_DisableDamage(false)
    , m_BlenderModeDirty(false)
	, m_UnregisterAsleepCheck(false)
    , m_ScriptGrabbedFromWorld(false)
    , m_ScriptGrabPosition(VEC3_ZERO)
    , m_ScriptGrabRadius(0.0f)
	, m_UnregisteringDueToOwnershipConflict(false)
    , m_HasBeenPickedUpByHook(false)
	, m_GameObjectBeingDestroyed(false)
	, m_HasExploded(false)
	, m_bDelayedCreateDrawable(false)
	, m_bDontAddToUnassignedList(false)
	, m_ScriptAdjustedScopeDistance(0)
    , m_bCanBlendWhenFixed(false)
	, m_bHasLoadedDroneModelIn(false)
    , m_bUseHighPrecisionRotation(false)
    , m_ArenaBallBlenderOverrideTimer(0)
    , m_bIsArenaBall(false)
    , m_fragParentVehicle(NETWORK_INVALID_OBJECT_ID)
	, m_fragSettledOverrideBlender(false)
    , m_bDriveSyncPending(false)
    , m_SyncedJointToDriveIndex(0)
    , m_bSyncedDriveToMinAngle(false)
    , m_bSyncedDriveToMaxAngle(false)
    {
        SetObjectType(NET_OBJ_TYPE_OBJECT);
    }

	~CNetObjObject();

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_OBJECT" : "OBJECT"; }

	CObject* GetCObject() const { return (CObject*)GetEntity(); }

    static void CreateSyncTree();
    static void DestroySyncTree();

	static  CProjectSyncTree*	GetStaticSyncTree()  { return ms_objectSyncTree; }
	virtual CProjectSyncTree*	GetSyncTree()		 { return ms_objectSyncTree; }
	virtual netScopeData&		GetScopeData()		 { return ms_objectScopeData; }
	virtual netScopeData&		GetScopeData() const { return ms_objectScopeData; }
    static  netScopeData&		GetStaticScopeData() { return ms_objectScopeData; }
	virtual netSyncDataBase*	CreateSyncData()	 { return rage_new CObjectSyncData(); }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    void SetUseHighPrecisionBlending(bool useHighPrecisionBlending);
	void SetUseParachuteBlending(bool useParachuteBlending);
    void SetUsingScriptedPhysicsParams(bool usingScriptedPhysicsParams) { m_UsingScriptedPhysicsParams = usingScriptedPhysicsParams; }
	void SetDisableBreaking(bool disableBreaking) { m_DisableBreaking = disableBreaking; }
	void SetDisableDamage(bool disableDamage) { m_DisableDamage = disableDamage; }
	void SetUnregisteringDueToOwnershipConflict() { m_UnregisteringDueToOwnershipConflict = true; }
	void SetGameObjectBeingDestroyed() { m_GameObjectBeingDestroyed = true; }
	void SetDontAddToUnassignedList() { m_bDontAddToUnassignedList = true; }
	void SetScriptAdjustedScopeDistance(u32 dist) { m_ScriptAdjustedScopeDistance = (u16) dist; }
    void SetCanBlendWhenFixed(bool canBlend) { m_bCanBlendWhenFixed = canBlend; }

    void SetUseHighPrecisionRotation(bool bUseHightPrecisionRotation);

    void SetIsArenaBall(bool bIsArenaBall);
    bool IsArenaBall() const { return m_bIsArenaBall; }
	bool IsObjectDamaged() const { return m_bObjectDamaged; }
    void SetArenaBallBlenderOverrideTimer(s32 timer) { m_ArenaBallBlenderOverrideTimer = timer; }

    virtual void RegisterCollision(CNetObjPhysical* pCollider, float impulseMag);

	inline void SetAmbientProp(void)								{ m_AmbientProp = true; }

	void SetDummyPositionAndFragGroupIndex(const Vector3& dummyPos, s8 fragGroupIndex) { m_DummyPosition = dummyPos; m_FragGroupIndex = fragGroupIndex; }

    void SetScriptGrabParameters(const Vector3 &scriptGrabPos, const float scriptGrabRadius);
	
	// Are we forcing a scope or minimum update rate level this frame?
	virtual bool        ForceManageUpdateLevelThisFrame();	

	virtual bool        Update();
	virtual void		SetGameObject(void* gameObject);
    virtual void        CreateNetBlender();
	virtual bool        ProcessControl();
	virtual u32			CalcReassignPriority() const;
	virtual void        CleanUpGameObject(bool bDestroyObject);
	virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationType);
	virtual bool        IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual void        ManageUpdateLevel(const netPlayer& player);
    virtual float	    GetScopeDistance(const netPlayer* pRelativePlayer = NULL) const;
	virtual Vector3		GetScopePosition() const;
	virtual Vector3		GetPosition() const;
    virtual bool        CanDelete(unsigned* reason = nullptr);
	virtual bool		CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual bool		CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual bool		CanPassControlWithNoGameObject() const { return m_AmbientProp; } 
	virtual bool		CanReassignWithNoGameObject() const { return m_AmbientProp; } 
	virtual bool		CanSyncWithNoGameObject() const { return true; }
    virtual void		PostCreate();
	virtual void		PostMigrate(eMigrationType migrationType);
	virtual void		OnUnregistered();
	virtual bool        CanSynchronise(bool bOnRegistration) const;
	virtual bool		CanBlend(unsigned *resultCode) const;
    virtual bool        CanBlendWhenFixed() const;
	virtual bool		NetworkBlenderIsOverridden(unsigned *resultCode) const;
	virtual bool        NetworkAttachmentIsOverridden() const;
	virtual void	    ValidateGameObject(void* gameObject) const;
	virtual void		DisplayNetworkInfo();
	virtual bool		RestrictsBroadcastDataWhenUncloned() const { return !m_ScriptGrabbedFromWorld; }
	virtual void		OnConversionToAmbientObject();
	virtual bool		CanShowGhostAlpha() const;
	virtual CPhysical *GetAttachOrDummyAttachParent() const;
	// returns false if the object is a type not able to be networked as a CNetObjObject 
	static bool CanBeNetworked(CObject* pObject, bool bIsBroken = false, bool bIsScript = false);

	// decides whether an object needs to be cloned on other machines when initially moved
	static bool HasToBeCloned(CObject* pObject, bool bIsBroken = false);

	void KeepRegistered() { m_KeepRegistered = true; }

	void BelongToVehicleFragment(ObjectId vehicleId)
	{
		const int FORCE_SCOPE_TIMER = 750;
		KeepRegistered();
		m_fragParentVehicle = vehicleId;
		m_forceScopeCheckUntilFirstUpdateTimer = FORCE_SCOPE_TIMER;
	}

	ObjectId GetFragParentVehicleId()
	{
		return m_fragParentVehicle;
	}

	// sync parser create functions
	void GetObjectCreateData(CObjectCreationDataNode& data)  const;

	void SetObjectCreateData(const CObjectCreationDataNode& data);

    // sync parser getter functions
    void GetObjectSectorPosData(CObjectSectorPosNode& data) const;

    void GetObjectGameStateData(CObjectGameStateDataNode& data) const;

    void GetObjectScriptGameStateData(CObjectScriptGameStateDataNode& data) const;

    // sync parser setter functions
    void SetObjectSectorPosData(CObjectSectorPosNode& data);
    
    void SetObjectOrientationData(CObjectOrientationNode& data);
    void GetObjectOrientationData(CObjectOrientationNode& data) const;

    void SetObjectGameStateData(const CObjectGameStateDataNode& data);

    void SetObjectScriptGameStateData(const CObjectScriptGameStateDataNode& data);

	bool IsActiveVehicleFrag() const;

	static bool IsAmbientObjectType(u32 type);
	static bool IsFragmentObjectType(u32 type);
	static bool IsScriptObjectType(u32 type);

	static void LogTaskData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits);

	void RegisterKillWithNetworkTracker();

	virtual void OnAttachedToPickUpHook() { m_HasBeenPickedUpByHook = true; CNetObjPhysical::OnAttachedToPickUpHook(); }

    void ForceSendOfScriptGameStateData();

    // network blender data
    static CPhysicalBlenderData *ms_ObjectBlenderDataStandard;
    static CPhysicalBlenderData *ms_ObjectBlenderDataHighPrecision;
	static CPhysicalBlenderData *ms_ObjectBlenderDataParachute;
    static CPhysicalBlenderData *ms_ObjectBlenderDataArenaBall;

    static u32 ms_collisionSyncIgnoreTimeArenaBall; // syncs are ignored for this period of time after a collision with a vehicle
    static float ms_collisionMinImpulseArenaBall; // the minimum impulse that will trigger the ignoring of syncs for a while

	// unassigned prop objects: 
	static bool HasSpaceForUnassignedAmbientObject() { return (ms_numUnassignedAmbientObjects < MAX_UNASSIGNED_AMBIENT_OBJECTS); }
	static void AssignAmbientObject(CObject& object, bool bBreakIfNecessary);
	static bool UnassignAmbientObject(CObject& object);
	static bool AddUnassignedAmbientObject(CNetObjObject& object, const char* reason);
	static void RemoveUnassignedAmbientObject(CNetObjObject& object, const char* reason, bool bUnregisterObject, int objectSlot = -1);
	static void RemoveUnassignedAmbientObjectsInArea(const Vector3& centre, float radius);

	static CObject* FindAssociatedAmbientObject(const Vector3& dummyPos, int fragGroupIndex, bool bOnlyIncludeNetworkedObjects = false);
	static CNetObjObject* FindUnassignedAmbientObject(const Vector3& dummyPos, int fragGroupIndex);
	static CObject* GetFragObjectFromParent(CObject& object, u32 fragGroupIndex, bool bDeleteChildren);

	static PhysicalPlayerIndex GetObjectConflictPrecedence(PhysicalPlayerIndex playerIndex1, PhysicalPlayerIndex playerIndex2) { return (playerIndex1 < playerIndex2) ? playerIndex1 : playerIndex2; }
	static bool IsSameDummyPos(const Vector3& dummyPos1, const Vector3& dummyPos2, float* pDist = NULL);

	static void TryToRegisterAmbientObject(CObject& object, bool weWantOwnership, const char* reason);

	// VEHICLE FRAGS
	// Called when the physical object was created locally. Used to linked the network object to the gameobject
	static void AssignVehicleFrags(CObject& object);
	// Called when network object for fragment has been cloned but gameobject doesn't exist yet
	static bool AddUnassignedVehicleFrag(CNetObjObject& object);
	// Tries to find an already cached network object for the same vehicle id. Called on creation
	static CNetObjObject* FindUnassignedVehicleFragment(ObjectId parentVehicleId);
	// Tries to find an already existing gameobject for the vehicle. Called on creation
	static CObject* FindAssociatedVehicleFrag(ObjectId parentVehicleId);
	// Removes the network object from the unassigned array. Called on network object destruction
	static void RemoveUnassignedVehicleFragment(CNetObjObject& object);


protected:

    virtual bool IsInHeightScope(const netPlayer& playerToCheck, const Vector3& entityPos, const Vector3& playerPos, unsigned* scopeReason = NULL) const;

	virtual bool AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason = nullptr);

    virtual u8 GetLowestAllowedUpdateLevel() const;
    virtual bool UseBoxStreamerForFixByNetworkCheck() const;
	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;

private:

    void UpdateBlenderData();

	void GetObjectAITask(u32 &taskType, u32 &taskDataSize, u8 *taskSpecificData) const;
	void SetObjectAITask(u32 const taskType, u32 const taskDataSize, u8 const* taskSpecificData);	

	bool UpdateUnassignedObject();

	bool IsBroken() const;
	bool IsAmbientFence() const;
	
	void TryToBreakObject();

	void SetTintIndexOnObjectHelper(u8 tintIndex);

#if __BANK
    virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const;
#endif // __BANK

    u8 m_SyncedJointToDriveIndex;

    mutable bool	m_bCloneAsTempObject					: 1;
    bool			m_registeredWithKillTracker				: 1;
    bool			m_UseHighPrecisionBlending				: 1;
	bool			m_UseParachuteBlending					: 1;
    bool			m_UsingScriptedPhysicsParams			: 1;
	bool			m_DisableBreaking						: 1;
	bool			m_DisableDamage							: 1;
	bool			m_BlenderModeDirty						: 1;
	bool			m_UnregisterAsleepCheck					: 1;
    bool            m_ScriptGrabbedFromWorld				: 1;
	bool			m_AmbientProp							: 1;
	bool			m_UnassignedAmbientObject				: 1;
	bool			m_UnassignedVehicleFragment				: 1;
	bool			m_AmbientFence							: 1;
	bool			m_HasExploded							: 1;
	bool			m_KeepRegistered						: 1;
	bool			m_DestroyFrags							: 1;
	bool			m_UnregisteringDueToOwnershipConflict	: 1;
	bool            m_HasBeenPickedUpByHook                 : 1;
	bool            m_GameObjectBeingDestroyed              : 1;
	bool			m_bDelayedCreateDrawable				: 1;
	bool			m_bDontAddToUnassignedList				: 1;
    bool            m_bCanBlendWhenFixed                    : 1;
	bool			m_bHasLoadedDroneModelIn				: 1;
    bool            m_bUseHighPrecisionRotation             : 1;
    bool			m_bIsArenaBall                          : 1;
    bool            m_bDriveSyncPending                     : 1;
    bool            m_bSyncedDriveToMaxAngle                : 1;
    bool            m_bSyncedDriveToMinAngle                : 1;
	bool            m_fragSettledOverrideBlender            : 1;
	bool            m_bObjectDamaged                        : 1;

	ObjectId		m_fragParentVehicle;

	u32				m_brokenFlags;

    Vector3         m_ScriptGrabPosition;
    float           m_ScriptGrabRadius;

	u32				m_lastTimeBroken;
	Vector3         m_DummyPosition;	
	s8				m_FragGroupIndex;
	u16				m_ScriptAdjustedScopeDistance;

	int             m_forceScopeCheckUntilFirstUpdateTimer;

    s32             m_ArenaBallBlenderOverrideTimer;

	SyncGroupFlags	m_groupFlags;

	static CObjectSyncTree *ms_objectSyncTree;
	static CObjectScopeData	ms_objectScopeData;

	// static array of CNetObjObjects that have no game objects, for quick look up
	static const int	  MAX_UNASSIGNED_AMBIENT_OBJECTS = 20;
	static CNetObjObject* ms_unassignedAmbientObjects[MAX_UNASSIGNED_AMBIENT_OBJECTS];
	static u32			  ms_numUnassignedAmbientObjects;
	static u32		      ms_peakUnassignedAmbientObjects;
	static bool			  ms_assigningAmbientObject;
	static bool			  ms_gettingFragObjectFromParent;

	static const int	  MAX_UNASSIGNED_VEHICLE_FRAGS = 10;
	static ObjectId		  ms_unassignedVehicleFrags[MAX_UNASSIGNED_VEHICLE_FRAGS];

};

#endif  // NETOBJOBJECT_H
