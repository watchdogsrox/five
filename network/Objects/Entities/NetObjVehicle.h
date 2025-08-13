//
// name:        NetObjVehicle.h
// description: Derived from netObject, this class handles all vehicle-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJVEHICLE_H
#define NETOBJVEHICLE_H

#include "fwnet/netlog.h"

#include "network/objects/entities/NetObjPhysical.h"
#include "network/objects/synchronisation/syncnodes/vehiclesyncnodes.h"
#include "task/system/TaskHelpers.h"
#include "vehicles/vehicle.h"
#include "vehicleai/VehicleNodeList.h"

#define LARGEST_NETOBJVEHICLE_CLASS (ALIGN_TO_16(sizeof(CNetObjPlane))) // must be aligned to 16!

class CTaskVehicleMissionBase;
struct CVehicleBlenderData;
struct CBoatBlenderData;
struct CHeliBlenderData;
struct CLargeSubmarineBlenderData;

class CNetObjVehicle : public CNetObjPhysical, public IVehicleNodeDataAccessor
{
public:

    FW_REGISTER_CLASS_POOL(CNetObjVehicle);

    static const unsigned SIZEOF_EXTRAS        =  11;

    static float ms_PredictedPosErrorThresholdSqr [CNetworkSyncDataULBase::NUM_UPDATE_LEVELS];

#if __BANK
    // debug update rate that can be forced on local player via the widgets
    static s32 ms_updateRateOverride;
#endif

    struct CVehicleScopeData : public netScopeData
    {
        CVehicleScopeData()
        {
            m_scopeDistance     = 300.0f;
            m_syncDistanceNear  = 10.0;
            m_syncDistanceFar   = 60.0f;
			m_scopeDistanceInterior = 300.0f;
        }
    };

    static const int VEHICLE_SYNC_DATA_NUM_NODES = 25;
    static const int VEHICLE_SYNC_DATA_BUFFER_SIZE = 736;

    class CVehicleSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, VEHICLE_SYNC_DATA_NUM_NODES, VEHICLE_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
    {
    public:
       FW_REGISTER_CLASS_POOL(CVehicleSyncData);
    };

private:

    static const unsigned NUM_PLAYERS_TO_UPDATE_IMPORTANT = 4; // the number of players to update per frame for important objects for operations batched by player
    static const unsigned NUM_PLAYERS_TO_UPDATE_NORMAL    = 2; // the number of players to update per frame for normal objects for operations batched by player

public:

    CNetObjVehicle(class CVehicle* pVehicle,
                  const NetworkObjectType type,
                  const ObjectId objectID,
                  const PhysicalPlayerIndex playerIndex,
                  const NetObjFlags localFlags,
                  const NetObjFlags globalFlags);

    CNetObjVehicle() :
    m_playerLocks(0),
    m_teamLocks(0),
    m_teamLockOverrides(0),
    m_deformationDataFL(0),
	m_deformationDataFR(0),
	m_deformationDataML(0),
	m_deformationDataMR(0),
    m_deformationDataRL(0),
    m_deformationDataRR(0),
    m_WindowsBroken(0),
    m_alarmSet(false),
    m_alarmActivated(false),
    m_frontWindscreenSmashed(false),
    m_rearWindscreenSmashed(false),
    m_registeredWithKillTracker(false),
    m_hasUnregisteringOccupants(false),
    m_lockedForNonScriptPlayers(false),
    m_PedInSeatStateFullySynced(true),
	m_bRemoteIsInAir(false),
	m_CloneIsResponsibleForCollision(false),
    m_LastPositionWithWheelContacts(VEC3_ZERO),
	m_fixedCount(0),
	m_extinguishedFireCount(0),
    m_PreventCloneMigrationCountdown(0),
    m_FrontBumperDamageState(0),
    m_RearBumperDamageState(0),
    m_LastThrottleReceived(0.0f),
    m_GarageInstanceIndex(INVALID_GARAGE_INDEX),
    m_IsCargoVehicle(false),
    m_FixIfNotDummyBeforePhysics(false),
    m_HasPendingLockState(false),
    m_HasPendingDontTryToEnterIfLockedForPlayer(false),
    m_PendingDontTryToEnterIfLockedForPlayer(false),
    m_PendingPlayerLocks(0),
    m_bHasPendingExclusiveDriverID(false),
	m_bHasPendingLastDriver(false),
	m_uLastProcessRemoteDoors(0),
	m_bHasHydraulicBouncingSound(false),
	m_bHasHydraulicActivationSound(false),
	m_bHasHydraulicDeactivationSound(false),
	m_ForceSendHealthNode(false),
	m_usingBoatBlenderData(false),
	m_usingPlaneBlenderData(false),
	m_disableCollisionUponCreation(false),
	m_FrameForceSendRequested(0),
	m_PlayersUsedExtendedRange(0),
	m_LaunchedInAirTimer(0),
	m_bLockedToXY(false),
	m_attemptedAnchorTime(0),
	m_pendingParachuteObjectId(NETWORK_INVALID_OBJECT_ID),
	m_cloneGliderState(0),
	m_lastReceivedVerticalFlightModeRatio(0.0f),
	m_detachedTombStone(false)
    {
		for (u32 i=0;i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS;++i)
		{
			m_PendingExclusiveDriverID[i] = NETWORK_INVALID_OBJECT_ID;
		}
		
		m_LastDriverID = NETWORK_INVALID_OBJECT_ID;
		m_nLastDriverTime = 0;

        DEV_ONLY(m_timeCreated = 0);
        ResetRoadNodeHistory();

		ClearWeaponImpactPointInformation();

		for(int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
		{
			m_pVehicleBadgeData[i] = NULL;
		}
    }

	~CNetObjVehicle();

    CVehicle* GetVehicle() const { return (CVehicle*)GetGameObject(); }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    static u32  GetNumVehiclesOutsidePopulationRange()   { return ms_numVehiclesOutsidePopulationRange; }
    static void ResetNumVehiclesOutsidePopulationRange() { ms_numVehiclesOutsidePopulationRange = 0; }

	void LockForPlayer(PhysicalPlayerIndex playerIndex, bool bLock);
	void LockForAllPlayers(bool bLock);
	void LockForNonScriptPlayers(bool bLock);
	bool IsLockedForNonScriptPlayers() const { return m_lockedForNonScriptPlayers; }
	bool IsLockedForPlayer(PhysicalPlayerIndex playerIndex) const ;
	bool IsLockedForAnyPlayer() const { return m_playerLocks != 0; }
    void SetPlayerLocks(PlayerFlags playerLocks) { m_playerLocks = playerLocks; } 
	PlayerFlags GetPlayerLocks() const { return m_playerLocks; }
	TeamFlags GetTeamLocks() const { return m_teamLocks; }
	PlayerFlags GetTeamLockOverrides() const { return m_teamLockOverrides; }

    void LockForTeam(u8 teamIndex, bool bLock);
	void LockForAllTeams(bool bLock);
    bool IsLockedForTeam(u8 teamIndex) const;
    void SetTeamLockOverrideForPlayer(PhysicalPlayerIndex playerIndex, bool bOverride);

    bool        HasPendingPlayerLocks() const { return m_HasPendingLockState; }
    PlayerFlags GetPendingPlayerLocks() const { return m_PendingPlayerLocks;  }
    void SetPendingPlayerLocks(PlayerFlags playerLocks);
    void SetPendingDontTryToEnterIfLockedForPlayer(bool tryToEnter);
    void SetPendingExclusiveDriverID(ObjectId exclusiveDriverID, u32 exclusiveDriverIndex);

	const CTaskVehicleMissionBase* GetCloneAITask() const { return m_pCloneAITask; }

    CVehicleFollowRouteHelper &GetFollowRouteHelper() { return m_FollowRouteHelper; }
    CVehicleNodeList          &GetVehicleNodeList()   { return m_NodeList; }

    void EvictAllPeds();

    void SetFrontWindscreenSmashed() { m_frontWindscreenSmashed = true; }
    void SetRearWindscreenSmashed() { m_rearWindscreenSmashed = true; }

    void SetWindowState(eHierarchyId id, bool isBroken);
    void SetVehiclePartDamage(eHierarchyId ePart, u8 state);

    void SetHasUnregisteringOccupants() { m_hasUnregisteringOccupants = true; }

	void SetTimedExplosion(ObjectId nCulpritID, u32 nExplosionTime);
	void DetonatePhoneExplosive(ObjectId nCulpritID);

    void SetPreventCloneMigrationCountdown(u8 countdown) { m_PreventCloneMigrationCountdown = countdown; };

    void SetGarageInstanceIndex(u8 garageInstanceIndex) { m_GarageInstanceIndex = garageInstanceIndex; }

    virtual bool       IsAttachedOrDummyAttached() const;
    virtual CPhysical *GetAttachOrDummyAttachParent() const;

    virtual netScopeData&      GetScopeData()       { return ms_vehicleScopeData; }
    virtual netScopeData&      GetScopeData() const { return ms_vehicleScopeData; }
    static  netScopeData&      GetStaticScopeData() { return ms_vehicleScopeData; }
    virtual netSyncDataBase*   CreateSyncData()     { return rage_new CVehicleSyncData(); }

    // see NetworkObject.h for a description of these functions
    virtual bool        Update();
    virtual void        UpdateBeforePhysics();
    virtual bool        ProcessControl();
    virtual void        CreateNetBlender();
    virtual bool        CanDelete(unsigned* reason = nullptr);
    virtual bool        CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool        CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool		CanBlend(unsigned *resultCode) const;
    virtual bool		NetworkBlenderIsOverridden(unsigned *resultCode) const;
    virtual bool        CanBlendWhenFixed() const;
    virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationTypee);
    virtual bool        CheckPlayerHasAuthorityOverObject(const netPlayer& player);
    virtual void        PlayerHasLeft(const netPlayer& player);
	virtual void		PrepareForScriptTermination();
    virtual void        CleanUpGameObject(bool bDestroyObject);
    virtual bool        IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
    virtual void        ManageUpdateLevel(const netPlayer& player);
	virtual bool		IsImportant() const;
    virtual u8          GetNumPlayersToUpdatePerBatch() const { return IsImportant() ? NUM_PLAYERS_TO_UPDATE_IMPORTANT : NUM_PLAYERS_TO_UPDATE_NORMAL; }
	virtual bool        NeedsPlayerTeleportControl() const { return true; }
    virtual bool		OnReassigned();
	virtual void		PostCreate();
    virtual void        PostSync();
	virtual void		PostMigrate(eMigrationType migrationType);
    virtual void		OnConversionToAmbientObject();
	virtual void		SetInvisibleAfterFadeOut();
	virtual bool		CanShowGhostAlpha() const;

	virtual float		GetScopeDistance(const netPlayer* pRelativePlayer = NULL) const;
    virtual float		GetPlayerDriverScopeDistanceMultiplier() const;

	virtual bool		CanDeleteWhenFlaggedForDeletion(bool useLogging);

	static void LogTaskData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits);
	static void LogTaskMigrationData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits);
	static void LogGadgetData(netLoggingInterface *log, IVehicleNodeDataAccessor::GadgetData& gadgetData);

	virtual void SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate = false);
	
	void RecursiveDetachmentOfChildren(CPhysical* entityToDetach);

	void FindAndDetachAttachedPlayer(CEntity* entity);

	void ForceVeryHighUpdateLevel(u16 timer);
	// Custom fix to make distant vehicles real when they collide with a dune4/dune5/phantom2 (sloped cars that launch other vehicles in air)
	void SetSuperDummyLaunchedIntoAir(u16 timer);
	bool IsSuperDummyStillLaunchedIntoAir() const;

	//Called on the vehicle that the player was in when he swaps team.
	void  PostSetupForRespawnPed(CPed* ped);

	//Manage network horn events
	void SetHornAppliedThisFrame() { m_bHornAppliedThisFrame = true; }
	bool WasHornAppliedThisFrame() const { return m_bHornAppliedThisFrame; }
	void SetHornOffPending() { m_bHornOffPending = true; }
	void SetHornOnFromNetwork(bool bFromNetwork) { m_bHornOnFromNetwork = bFromNetwork; }
	bool IsHornOnFromNetwork() { return m_bHornOnFromNetwork; }

	bool IsRemoteInAir() const { return m_bRemoteIsInAir; }

	bool HasBeenCountedByDispatch()	const	{ return m_CountedByDispatch != 0; }
	void SetCountedByDispatch(bool counted) { m_CountedByDispatch = counted; }

	u8   GetCloneGliderState() const { return m_cloneGliderState; }

	void RequestNetworkHydraulicBounceSound() {	m_bHasHydraulicBouncingSound = true; }
	void RequestNetworkHydraulicActivationSound() { m_bHasHydraulicActivationSound = true; }
	void RequestNetworkHydraulicDeactivationSound() { m_bHasHydraulicDeactivationSound = true; }
#if __BANK
	static void  AddDebugVehicleDamage(bkBank *bank);
	static void  AddDebugVehiclePopulation(bkBank *bank);
#endif // __BANK

#if __DEV
    u32 m_timeCreated;
#endif

	static CVehicleBlenderData        *ms_vehicleBlenderData;
	static CBoatBlenderData           *ms_boatBlenderData;
	static CHeliBlenderData           *ms_planeBlenderData;
	static CLargeSubmarineBlenderData *ms_largeSubmarineBlenderData;

    // sync parser migration functions
    void GetVehicleCreateData(CVehicleCreationDataNode& data);
    void GetVehicleMigrationData(CVehicleProximityMigrationDataNode& data);
    void SetVehicleCreateData(const CVehicleCreationDataNode& data);
    void SetVehicleMigrationData(const CVehicleProximityMigrationDataNode& data);

    // sync parser getter functions
    void GetVehicleAngularVelocity(CVehicleAngVelocityDataNode& data);
    void GetVehicleGameState(CVehicleGameStateDataNode& data);
	void GetVehicleScriptGameState(CVehicleScriptGameStateDataNode& data);
    void GetVehicleHealth(CVehicleHealthDataNode& data);
    void GetSteeringAngle(CVehicleSteeringDataNode& data);
    void GetVehicleControlData(CVehicleControlDataNode& data);
    void GetVehicleAppearanceData(CVehicleAppearanceDataNode& data);
	void GetVehicleDamageStatusData(CVehicleDamageStatusDataNode& data);
	void GetVehicleAITask(CVehicleTaskDataNode& data);	
    void GetComponentReservations(CVehicleComponentReservationDataNode& data);
	void GetVehicleGadgetData(CVehicleGadgetDataNode& data);

	void DirtyVehicleDamageStatusDataSyncNode();

    // sync parser setter functions
    void SetVehicleAngularVelocity(CVehicleAngVelocityDataNode& data);
    void SetVehicleGameState(const CVehicleGameStateDataNode& data);
    void SetVehicleScriptGameState(const CVehicleScriptGameStateDataNode& data);
    void SetVehicleHealth(const CVehicleHealthDataNode& data);
    void SetSteeringAngle(const CVehicleSteeringDataNode& data);
    void SetVehicleControlData(const CVehicleControlDataNode& data);
    void SetVehicleAppearanceData(const CVehicleAppearanceDataNode& data);
	void SetVehicleDamageStatusData(const CVehicleDamageStatusDataNode& data);
	void SetVehicleAITask(const CVehicleTaskDataNode& data);
	void SetComponentReservations(const CVehicleComponentReservationDataNode& data);
	void SetVehicleGadgetData(const CVehicleGadgetDataNode& data);

	bool GetIsAttachedForSync() const;

	void GetPhysicalAttachmentData(	bool       &attached,
									ObjectId   &attachedObjectID,
									Vector3    &attachmentOffset,
									Quaternion &attachmentQuat,
									Vector3    &attachmentParentOffset,
									s16        &attachmentOtherBone,
									s16        &attachmentMyBone,
									u32        &attachmentFlags,
									bool       &allowInitialSeparation,
									float      &invMassScaleA,
									float      &invMassScaleB,
									bool	   &activatePhysicsWhenDetached,
                                    bool       &isCargoVehicle) const;

    void SetPhysicalAttachmentData(const bool        attached,
								   const ObjectId    attachedObjectID,
								   const Vector3    &attachmentOffset,
								   const Quaternion &attachmentQuat,
								   const Vector3    &attachmentParentOffset,
								   const s16         attachmentOtherBone,
								   const s16         attachmentMyBone,
								   const u32         attachmentFlags,
                                   const bool        allowInitialSeparation,
                                   const float       invMassScaleA,
                                   const float       invMassScaleB,
								   const bool		  activatePhysicsWhenDetached,
                                   const bool        isCargoVehicle);

	void RegisterKillWithNetworkTracker();

	void  CacheCollisionFault(CEntity* pInflictor, u32 nWeaponHash);

	bool GetWasBlownUpByTimedExplosion() const { return m_bWasBlownUpByTimedExplosion; }

	void OnVehicleFixed();
	void OnVehicleFireExtinguished();

	void SetVehicleBadge(const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, u32 badgeIndex, u8 alpha);
	void ResetVehicleBadge(int index);

	void StoreWeaponImpactPointInformation(const Vector3& vHitPosition, eWeaponEffectGroup weaponEffectGroup, bool bAllowOnClone = false);
	void ClearWeaponImpactPointInformation();
	void ApplyRemoteWeaponImpactPointInformation();

	void GetWeaponImpactPointInformation(CVehicleDamageStatusDataNode& data);
	void SetWeaponImpactPointInformation(const CVehicleDamageStatusDataNode& data);

	void ProcessFailmark();

	static u32					GetSizeOfVehicleGadgetData(eVehicleGadgetType netGadgetType);

	virtual void SetAlpha(u32 alpha);
	virtual void ResetAlphaActive();
	virtual void SetHideWhenInvisible(bool bSet);

    void RequestForceSendOfHealthNode() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendHealthNode = true; }
	void RequestForceScriptGameStateNodeUpdate() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendScriptGameStateNode = true; }
	
	

	static class CVehicleGadgetTowArm     *GetTowArmGadget(CPhysical* pPhysical);
	static class CVehicleGadgetPickUpRope *GetPickupRopeGadget(CPhysical* pPhysical);

	void SetPendingParachuteObjectId(ObjectId id) { m_pendingParachuteObjectId = id; }

	ObjectId GetPendingParachuteObjectId() { return m_pendingParachuteObjectId; }

	void SetDisableCollisionUponCreation(bool val) { m_disableCollisionUponCreation = val; }
	bool GetDisableCollisionUponCreation() { return m_disableCollisionUponCreation; }

	void ForceSendVehicleDamageNode();

	void SetDetachedTombStone(bool detached) { m_detachedTombStone = detached;}

	void SetExtraFragsBreakable(bool breakable);
protected:

    virtual bool IsCloserToRemotePlayerForProximity(const CNetGamePlayer &remotePlayer, const float distToCurrentOwnerSqr, const float distToRemotePlayerSqr);

	virtual bool CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const;
    virtual bool AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason = nullptr);
	virtual void AttemptPendingDetachment(CPhysical* pEntityToAttachTo);

    virtual bool UseBoxStreamerForFixByNetworkCheck() const;
    virtual bool CheckBoxStreamerBeforeFixingDueToDistance() const { return true; }
	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;
    virtual void UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason);
    virtual bool ShouldDoFixedByNetworkInAirCheck() const;
    virtual void SetFixedByNetwork(bool fixObject, unsigned reason, unsigned npfbnReason);
    virtual u8   GetLowestAllowedUpdateLevel() const;
	virtual bool HasCollisionLoadedUnderEntity() const;
	virtual bool AllowFixByNetwork() const;
	virtual bool GetCachedEntityCollidableState(void);
	virtual bool CanEnableCollision() const;
	virtual void HideForCutscene();

    virtual bool          SendUpdatesBasedOnPredictedPositionErrors() const { return true; }
    virtual float         GetPredictedPosErrorThresholdSqr (unsigned updateLevel) const;
    virtual SnapshotData *GetSnapshotData(unsigned updateLevel);

    virtual bool ShouldPreventDummyModesThisFrame() const;
    virtual bool ShouldOverrideBlenderWhenStoppedPredicting() const;

	virtual void ProcessGhosting();

#if ENABLE_NETWORK_LOGGING
	virtual const char *GetCannotDeleteReason(unsigned reason);
#endif // ENABLE_NETWORK_LOGGING
private:
	// Functions to check for and hide objects when the local player or the object is in a tutorial
	virtual bool ShouldObjectBeHiddenForTutorial() const;
	virtual void HideForTutorial();

	void ApplyNetworkDeformation(bool bCheckDistance);
    bool IsTooCloseToApplyDeformationData();

    const CNodeAddress *EstimateOldRoadNode   (const CNodeAddress &roadNode);
    const CNodeAddress *EstimateFutureRoadNode(const CNodeAddress &roadNode, const bool bIsWandering);

    bool AddSkippedRoadNodesToHistory(const CNodeAddress &roadNode);
    void AddRoadNodeToHistory(const CNodeAddress &roadNode);
    void ResetRoadNodeHistory();

    void UpdateRouteForClone();

	bool PreventMigrationWhenPhysicallyAttached(const netPlayer& player, eMigrationType migrationType) const;

    void UpdateHasUnregisteringOccupants();

    void ResetLastPedsInSeats();
    void UpdateLastPedsInSeats();

	void ProcessBadging(CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, const bool bVehicleBadge, const CVehicleAppearanceDataNode::CVehicleBadgeData& badgeData, int index);

	static CVehicleGadget*	GetDummyVehicleGadget(eVehicleGadgetType netGadgetType);
	static CVehicleGadget*	AddVehicleGadget(CVehicle* pVehicle, eVehicleGadgetType netGadgetType);

	void ProcessRemoteDoors( bool forceUpdate );

    void UpdatePendingLockState();
    void UpdatePendingExclusiveDriver();
	void UpdateLastDriver();

	enum eNetworkEffectGroup
	{
		WEAPON_NETWORK_EFFECT_GROUP_BULLET = 0,
		WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN,
		WEAPON_NETWORK_EFFECT_GROUP_MAX
	};

	u8 ExtractWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup, bool bFromApplyArray = false);

	void IncrementWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup);

	void DecrementApplyWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup);

	void UpdateForceSendRequests();

	static u32 ms_numVehiclesOutsidePopulationRange;

	PlayerFlags m_playerLocks;	     // flags indicating which players the vehicle is locked for    
    PlayerFlags m_teamLockOverrides; // flags indicating players which are not subject to team locking checks
    TeamFlags   m_teamLocks;         // flags indicating which teams the vehicle is locked for
    u8          m_WindowsBroken;     // flags indicating which flags are broken

	taskRegdRef<CTaskVehicleMissionBase>  m_pCloneAITask;	// an inactive task stored for clones, mirroring the task running on the master vehicle.

    u8			m_deformationDataFL:2;
    u8			m_deformationDataFR:2;
    u8			m_deformationDataML:2;
    u8			m_deformationDataMR:2;
    u8			m_deformationDataRL:2;
    u8			m_deformationDataRR:2;

    bool   m_alarmSet                       : 1;
    bool   m_alarmActivated                 : 1;
    bool   m_frontWindscreenSmashed         : 1;
    bool   m_rearWindscreenSmashed          : 1;
    bool   m_registeredWithKillTracker      : 1;
	bool   m_lockedForNonScriptPlayers      : 1;
    bool   m_hasUnregisteringOccupants      : 1;
	bool   m_CloneIsResponsibleForCollision : 1;
	bool   m_CountedByDispatch              : 1;
	bool   m_bHornAppliedThisFrame          : 1;
	bool   m_bHornOffPending                : 1;
	bool   m_bHornOnFromNetwork             : 1;
	bool   m_bRemoteIsInAir                 : 1;
	bool   m_bTimedExplosionPending         : 1;
	bool   m_bForceProcessApplyNetworkDeformation : 1;
    bool   m_bHasPendingExclusiveDriverID   : 1;
	bool   m_bHasPendingLastDriver			: 1;
	bool   m_bHasHydraulicBouncingSound		: 1;
	bool   m_bHasHydraulicActivationSound	: 1;
	bool   m_bHasHydraulicDeactivationSound	: 1;
	bool   m_ForceSendHealthNode            : 1;
	bool   m_ForceSendScriptGameStateNode   : 1;
	bool   m_usingBoatBlenderData           : 1;
	bool   m_usingPlaneBlenderData          : 1;
	bool   m_disableCollisionUponCreation   : 1;  // Disable collision upon creation on remote machines for 1 frame only! 

	u8     m_FrameForceSendRequested;

	mutable PlayerFlags m_PlayersUsedExtendedRange;

	u32	   m_uLastProcessRemoteDoors;

    ObjectId    m_PendingExclusiveDriverID[CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS];
    ObjectId	m_TimedExplosionCulpritID;
    u32			m_nTimedExplosionTime;
	ObjectId	m_PhoneExplosionCulpritID;
	u32			m_nLastDriverTime;
	ObjectId	m_LastDriverID;

    bool		m_bWasBlownUpByTimedExplosion : 1;	
	bool		m_bPhoneExplosionPending      : 1;

	bool        m_vehicleBadgeInProcess       : 1;
    bool        m_FixIfNotDummyBeforePhysics  : 1;

	u8		    m_fixedCount;

	u8			m_extinguishedFireCount;

	u8			m_cloneGliderState;

    // node address history - used for more accurate results for dummy network clones when placed on the,
    // virtual road surface. All road nodes older than the newest (target) node are stored
    static const unsigned NUM_FUTURE_NODES_TO_CACHE = 3;
    static const unsigned ROAD_NODE_HISTORY_SIZE = NODE_NEW + 1 + NUM_FUTURE_NODES_TO_CACHE;
    CNodeAddress m_RoadNodeHistory[ROAD_NODE_HISTORY_SIZE];

	u8 m_ApplyWeaponImpactPointLocationProcessCount;							//keep a count of the # of application passes we have made, if we exceed # bail

	bool m_weaponImpactPointLocationSet			: 1;							//whether or not data is set for send or receive processing
	bool m_ApplyWeaponImpactPointLocationSet	: 1;							//whether or not the data has been applied on the remote
	bool m_weaponImpactPointAllowOneShotgunPerFrame : 1;						//updated every frame to ensure that we can only store one shotgun impact point (out of several) per given frame

    // vehicle AI helper classes for use by clone vehicles (it's possible this could be refactored to replace m_RoadNodeHistory)
    CVehicleFollowRouteHelper m_FollowRouteHelper;
    CVehicleNodeList          m_NodeList;

	u8 m_weaponImpactPointLocationCounts[NUM_NETWORK_DAMAGE_DIRECTIONS];		//data used to store and send the impact counts to remotes
	u8 m_ApplyWeaponImpactPointLocationCounts[NUM_NETWORK_DAMAGE_DIRECTIONS];	//data used when received on a remote over multiple frames applying the decals.

    // keep track of peds who were in the seats of this vehicle
    ObjectId m_PedLastInSeat[MAX_VEHICLE_SEATS];
    bool     m_PedInSeatStateFullySynced;

    // this is a countdown frame timer that allows preventing the vehicle from migrating between two remote machines
    // it's possible for this to cause issues if this occurs at critical times (such as when a clone ped is still entering
    // the vehicle but has not been set into the seat yet)
    u8 m_PreventCloneMigrationCountdown;

    // cached damage states for the front and rear bumpers - it's expensive to query the vehicle for this info
    u8 m_FrontBumperDamageState;
    u8 m_RearBumperDamageState;

    // cached last throttle value received
    float m_LastThrottleReceived;

	u32 m_AllowCloneOverrideCheckDistanceTime;

	u8	m_remoteDoorOpen;
	u8	m_remoteDoorOpenRatios[ SC_DOOR_NUM ];

    // used to restrict a vehicle in a garage to only be cloned on players
    // currently viewing that garage
    u8 m_GarageInstanceIndex;

    // last position of this vehicle when it's wheels had contact with the ground
    Vector3 m_LastPositionWithWheelContacts;

    SnapshotData m_SnapshotData[CNetworkSyncDataULBase::NUM_UPDATE_LEVELS];

	CVehicleAppearanceDataNode::CVehicleBadgeData* m_pVehicleBadgeData[CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES];
	CVehicleBadgeDesc  m_VehicleBadgeDesc;

	bool				m_bHasBadgeData;

	bool				m_detachedTombStone : 1;

    bool                m_IsCargoVehicle			: 1;

    // pending lock state
    bool        m_HasPendingLockState : 1;
    bool        m_HasPendingDontTryToEnterIfLockedForPlayer : 1;
    bool        m_PendingDontTryToEnterIfLockedForPlayer    : 1;
    PlayerFlags m_PendingPlayerLocks;

	// Lowrider suspension.
	bool		m_bModifiedLowriderSuspension  : 1;
	bool		m_bAllLowriderHydraulicsRaised : 1;
    float		m_fLowriderSuspension[NUM_VEH_CWHEELS_MAX];

	u16         m_LaunchedInAirTimer;
	ObjectId    m_pendingParachuteObjectId;

protected:

#if __BANK
    // displays debug info above the network object
    virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement);
    virtual void DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const;
    virtual void GetHealthDisplayString(char *buffer) const;
    virtual void GetExtendedFlagsDisplayString(char *buffer) const;
	
    void ValidateDriverAndPassengers();
	u32 m_validateFailTimer;
#endif // __BANK

	virtual void UpdateBlenderData();

    static CVehicleScopeData ms_vehicleScopeData;

	u32 m_attemptedAnchorTime;
	float m_lastReceivedVerticalFlightModeRatio;

	// cached locked to XY flag - m_nBoatFlags.bLockedToXY must always be false for clones
	// as it causes problems with the prediction code - but we need to restore the correct value
	// if we gain ownership of an anchored boat
	bool m_bLockedToXY;
};

#endif  // NETOBJVEHICLE_H
