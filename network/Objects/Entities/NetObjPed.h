//
// name:        NetObjPed.h
// description:    Derived from netObject, this class handles all ped-related network object
//                calls. See NetworkObject.h for a full description of all the methods.
// written by:    John Gurney
//

#ifndef NETOBJPED_H
#define NETOBJPED_H

#include "network/objects/entities/NetObjPhysical.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "scene/RegdRefTypes.h"
#include "task/general/TaskBasic.h"
#include "task/system/TaskTypes.h"

#define LARGEST_NETOBJPED_CLASS (ALIGN_TO_16(sizeof(CNetObjPlayer))) // must be aligned to 16!

// Forward delcarations
class CMotionTaskDataSet;
struct CPedBlenderData;

class CEventGunShotBulletImpact;
class CEventGunShotWhizzedBy;
class CGameWorld;

class CNetObjPed : public CNetObjPhysical, public IPedNodeDataAccessor
{
public:

    FW_REGISTER_CLASS_POOL(CNetObjPed);

    friend class CGameWorld; // required for restoring local AI state when changing player model/ped

#if __BANK
    // debug update rate that can be forced on local player via the widgets
    static s32 ms_updateRateOverride;
#endif

    static const float MAX_STANDING_OBJECT_OFFSET_POS;
    static const float MAX_STANDING_OBJECT_OFFSET_HEIGHT;

    struct CPedScopeData : public netScopeData
    {
        CPedScopeData()
        {
            m_scopeDistance			= 120.0f; // A little bit bigger than PED_POPULATION_ADD_RANGE_MAX
			m_scopeDistanceInterior = 50.0f;
            m_syncDistanceNear		= 10.0;
            m_syncDistanceFar		= 35.0f;
        }
    };
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//  CNetTennisMotionData is used to store and serialize data for actions needed when peds are running TASK_MOTION_TENNIS 
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class CNetTennisMotionData 
	{
	public:

		FW_REGISTER_CLASS_POOL(CNetTennisMotionData);
		CNetTennisMotionData(const CSyncedTennisMotionData& tennisData) : m_TennisMotionData(tennisData) {}

		CSyncedTennisMotionData m_TennisMotionData;
		bool					m_bNewTennisData;
	};


	static const int PED_SYNC_DATA_NUM_NODES = 29;
	static const int PED_SYNC_DATA_BUFFER_SIZE = 2016;

    class CPedSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, PED_SYNC_DATA_NUM_NODES, PED_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
    {
    public:
        FW_REGISTER_CLASS_POOL(CPedSyncData);
    };

	static const u32 INVALID_PENDING_MOTION_STATE = 0;
	static const u32 INVALID_PENDING_MOTION_TYPE  = 0;

protected:

	struct TaskSlotCache
	{
		TaskSlotCache() :
	m_lastFrameUpdated(0)
	{
		Reset();
	}

	void Reset()
	{
		m_lastFrameUpdated = 0;

		for(u32 index = 0; index < IPedNodeDataAccessor::NUM_TASK_SLOTS; index++)
		{
			m_taskSlots[index].m_taskType		= CTaskTypes::MAX_NUM_TASK_TYPES;
			m_taskSlots[index].m_taskPriority	= 0;
			m_taskSlots[index].m_taskTreeDepth	= 0;
			m_taskSlots[index].m_taskSequenceId = 0;
			m_taskSlots[index].m_taskActive		= false;
		}
	}

	TaskSlotCache& operator= (const TaskSlotCache& other)
	{
		m_lastFrameUpdated = other.m_lastFrameUpdated;

		for(u32 index = 0; index < IPedNodeDataAccessor::NUM_TASK_SLOTS; index++)
		{
			m_taskSlots[index].m_taskType		= other.m_taskSlots[index].m_taskType;
			m_taskSlots[index].m_taskPriority	= other.m_taskSlots[index].m_taskPriority;
			m_taskSlots[index].m_taskTreeDepth	= other.m_taskSlots[index].m_taskTreeDepth;
			m_taskSlots[index].m_taskSequenceId = other.m_taskSlots[index].m_taskSequenceId;
			m_taskSlots[index].m_taskActive		= other.m_taskSlots[index].m_taskActive;
		}

		return *this;
	}


	u32 m_lastFrameUpdated;
	IPedNodeDataAccessor::TaskSlotData m_taskSlots[IPedNodeDataAccessor::NUM_TASK_SLOTS];
	};


public:

    CNetObjPed(class CPed					*ped,
                const NetworkObjectType		type,
                const ObjectId				objectID,
                const PhysicalPlayerIndex   playerIndex,
                const NetObjFlags			localFlags,
                const NetObjFlags			globalFlags);

    CNetObjPed() :
    m_bAllowSetTask(false),
    m_CanAlterPedInventoryData(false),
    m_weaponObjectVisible(false),
	m_weaponObjectAttachLeft(false),
    m_pendingMotionSetId(rage::CLIP_SET_ID_INVALID),
    m_pendingAttachHeading(0.0f),
    m_pendingAttachHeadingLimit(0.0f),
 	m_PendingMoveBlenderState(-1),
	m_PendingMoveBlenderType(-1),
    m_registeredWithKillTracker(false),
	m_registeredWithHeadshotTracker(false),
	m_TaskOverridingPropsWeapons(false),
	m_TaskOverridingProps(false),
    m_DesiredHeading(0.0f),
    m_DesiredMoveBlendRatioX(0.0f),
    m_DesiredMoveBlendRatioY(0.0f),
	m_DesiredMaxMoveBlendRatio(0.0f),
    m_weaponObjectTintIndex(0),
	m_IsSwimming(false),
	m_IsDivingToSwim(false),
    m_IsDiving(false),
	m_targetVehicleId(NETWORK_INVALID_OBJECT_ID),
	m_myVehicleId(NETWORK_INVALID_OBJECT_ID),
	m_targetVehicleSeat(-1),
	m_targetVehicleTimer(0),
	m_targetMountId(NETWORK_INVALID_OBJECT_ID),
	m_targetMountTimer(0),
    m_UsesCollisionBeforeTaskOverride(true),
    m_weaponObjectFlashLightOn(false),
	m_RespawnFailedFlags(0),
	m_RespawnAcknowledgeFlags(0),
	m_RespawnRemovalTimer(0),
	m_lastRequestedSyncScene(0),
    m_NeedsNavmeshFixup(false),
	m_pendingLastDamageEvent(true),
	m_CacheUsingInfiniteAmmo(false),
    m_UpdateBlenderPosAfterVehicleUpdate(false),
    m_Wandering(false),
    m_FrameForceSendRequested(0),
    m_ForceSendRagdollState(false),
    m_ForceSendTaskState(false),
    m_ForceSendGameState(false),
	m_ForceRecalculateTasks(false),
	m_AttDamageToPlayer(INVALID_PLAYER_INDEX),
	m_PhoneMode(0),
	m_bNewPhoneMode(false),
	m_pNetTennisMotionData(NULL),
	m_ForceCloneTransitionChangeOwner(false),
    m_OverrideScopeBiggerWorldGridSquare(false),
    m_OverridingMoveBlendRatios(false),
	m_killedWithHeadShot(false),
	m_killedWithMeleeDamage(false),
	m_ClearDamageCount(0),
	m_PreventSynchronise(false),
    m_RunningSyncedScene(false),
    m_ZeroMBRLastFrame(true),
    m_PreventTaskUpdateThisFrame(false),
	m_pendingPropUpdate(false),
	m_bIsOwnerWearingAHelmet(false),
	m_bIsOwnerRemovingAHelmet(false),
	m_bIsOwnerAttachingAHelmet(false),
	m_bIsOwnerSwitchingVisor(false),
	m_nTargetVisorState(-1),
	m_HasValidWeaponClipset(false),
	m_numRemoteWeaponComponents(0),
	m_pMigrateTaskHelper(NULL),
	m_RappellingPed(false),
    m_HasStopped(true),
    m_HiddenUnderMapDueToVehicle(false),
	m_PRF_disableInVehicleActions(false),
	m_PRF_IgnoreCombatManager(false),
	m_Vaulting(false),
	m_changenextpedhealthdatasynch(0),
	m_lastSyncedWeaponHash(0),
	m_hasDroppedWeapon(false),
	m_blockBuildQueriableState(false)
	{
		m_taskSlotCache.Reset();
		m_CachedWeaponInfo.Reset();
        SetObjectType(NET_OBJ_TYPE_PED);

#if __BANK
		m_NumInventoryItems = 0;
		m_PedUsingInfiniteAmmo = false;
#endif
    }

    virtual ~CNetObjPed();

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_PED" : "PED"; }

    CPed* GetPed() const { return (CPed*)GetGameObject(); }

    // functions determining whether tasks can be set on network clones
    void AllowTaskSetting()     { m_bAllowSetTask = true; }
    void DisallowTaskSetting()  { m_bAllowSetTask = false; }
    bool CanSetTask();

    bool CanAlterPedInventoryData() const { return m_CanAlterPedInventoryData; }

	void        SetTaskOverridingPropsWeapons(bool bTaskOverridingPropsWeapons)	  {m_TaskOverridingPropsWeapons = bTaskOverridingPropsWeapons; }
	bool        GetTaskOverridingPropsWeapons()							const { return m_TaskOverridingPropsWeapons; }

	void        SetTaskOverridingProps(bool bTaskOverridingProps)			{m_TaskOverridingProps = bTaskOverridingProps; }
	bool        GetTaskOverridingProps()							const	{ return m_TaskOverridingProps; }

	void        SetOverrideScopeBiggerWorldGridSquare(bool bOverrideScopeBiggerWorldGridSquare)			{m_OverrideScopeBiggerWorldGridSquare = bOverrideScopeBiggerWorldGridSquare; }
	bool        GetOverrideScopeBiggerWorldGridSquare()							const	{ return m_OverrideScopeBiggerWorldGridSquare; }

    bool        GetIsDiving() const { return m_IsDiving; }
	bool		GetIsOwnerDiveToSwimming() const { return m_IsDivingToSwim; }
	bool		GetIsOwnerSwimming() const { return m_IsSwimming; }

	s32			GetTemporaryTaskSequenceId() const { return (s32)m_temporaryTaskSequenceId; }
	void		RemoveTemporaryTaskSequence();

    bool        IsNavmeshTrackerValid() const;

    void        GetDesiredMoveBlendRatios(float &desiredMBRX, float &desiredMBRY) { desiredMBRX = m_DesiredMoveBlendRatioX; desiredMBRY = m_DesiredMoveBlendRatioY; }
    void        FlagOverridingMoveBlendRatios() { m_OverridingMoveBlendRatios = true; }
	void		ForceHighUpdateLevel();

    void        PreventTaskUpdateThisFrame()  { m_PreventTaskUpdateThisFrame = true; }
    bool        IsTaskUpdatePrevented() const { return m_PreventTaskUpdateThisFrame; }

	void		SetRappellingPed() { m_RappellingPed = true; }

    bool        HasPedStopped() const { return m_HasStopped; }

    bool        IsHiddenUnderMapDueToVehicle() const { return m_HiddenUnderMapDueToVehicle; }

	// put the network ped out of a car
    void SetPedOutOfVehicle(const u32 flags);

	// returns true if the ped will migrate with the vehicle it is in
	bool MigratesWithVehicle() const;

    // force send request
    bool ForceRagdollStateRequestPending() const { return m_ForceSendRagdollState; }
    bool ForceTaskStateRequestPending()    const { return m_ForceSendTaskState;    }
    bool ForceGameStateRequestPending() const { return m_ForceSendGameState; }
    void RequestForceSendOfRagdollState() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendRagdollState = true; }
    void RequestForceSendOfTaskState()    { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendTaskState    = true; }
    void RequestForceSendOfGameState() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendGameState = true; }

	bool ForceRecalculateTasks() const { return m_ForceRecalculateTasks; }
	void RequestForceRecalculateTasks() { m_ForceRecalculateTasks = true; }

	virtual void ForceSendOfCutsceneState();
	virtual void SetIsInCutsceneRemotely(bool bSet);

    static void CreateSyncTree();
    static void DestroySyncTree();

    static  CProjectSyncTree*  GetStaticSyncTree()     { return ms_pedSyncTree; }
    virtual CProjectSyncTree*  GetSyncTree()           { return ms_pedSyncTree; }
    virtual netScopeData&      GetScopeData()          { return ms_pedScopeData; }
    virtual netScopeData&      GetScopeData() const    { return ms_pedScopeData; }
    static netScopeData&       GetStaticScopeData()    { return ms_pedScopeData; }
    virtual netSyncDataBase*   CreateSyncData()        { if (CPedSyncData::GetPool()->GetNoOfFreeSpaces() > 0) return rage_new CPedSyncData(); return NULL; }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	virtual bool        Update();
	virtual void        UpdateAfterScripts();
	virtual void        StartSynchronising();
    virtual bool        ProcessControl();
    virtual void        CreateNetBlender();
	virtual bool        CanDelete(unsigned* reason = nullptr);
	virtual void        CleanUpGameObject(bool bDestroyObject);
    virtual bool        CanClone(const netPlayer& player, unsigned *resultCode = 0) const;
    virtual bool        CanSynchronise(bool bOnRegistration) const;
	virtual bool		CanStopSynchronising() const;
    virtual bool        CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool        CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual u32			CalcReassignPriority() const;
    virtual bool        CanBlend(unsigned *resultCode = 0) const;
	virtual bool        NetworkBlenderIsOverridden(unsigned *resultCode = 0) const;
	virtual bool        NetworkHeadingBlenderIsOverridden(unsigned *resultCode = 0) const;
	virtual bool        NetworkAttachmentIsOverridden() const;
    virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationType);
    virtual bool        IsInScope(const netPlayer& player, unsigned* scopeReason = NULL) const;
    virtual void        ManageUpdateLevel(const netPlayer& player);
    virtual u8          GetLowestAllowedUpdateLevel() const;
	virtual void		SetGameObject(void* gameObject);
    virtual void        OnUnregistered();
	virtual bool		OnReassigned();
    virtual void		PostCreate();
    virtual void        PostSync();
    virtual void        PostMigrate(eMigrationType migrationType);
	virtual void		ConvertToAmbientObject();
	virtual void		OnConversionToAmbientObject();
	virtual void        PlayerHasJoined(const netPlayer& player);
	virtual void        PlayerHasLeft(const netPlayer& player);
	virtual void        UpdateDamageTracker(const NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision = false);
	virtual void		SetHideWhenInvisible(bool bSet);
	virtual bool		CanResetAlpha() const;

	virtual bool        NeedsPlayerTeleportControl() const { return true; }

	virtual bool		CanEnableCollision() const;

	virtual float		GetScopeDistance(const netPlayer* pRelativePlayer = NULL) const;

    virtual Vector3     GetPosition() const;

	//Called when the ped changes the GameObject. This can happen when the local player wants a new ped and the 
	//   command CHANGE_PLAYER_PED is called.
	void  ChangePed (CPed* ped, const bool recreateBlender = false, const bool setupPedAIAsClone = true);

	//Called when a player ped is converted to a respawn ped.
	void  PostSetupForRespawnPed(CVehicle* vehicle, const u32 seat, bool killPed, TaskSlotCache& taskSlotData);
	void  CreateLocalTasksForRespawnPed();
	void  SetPlayerRespawnFlags(const bool failed, const netPlayer& player);
	bool  IsRespawnInProgress() const;
	bool  IsRespawnFailed(const netPlayer& player) const;
	bool  IsRespawnUnacked(const netPlayer& player) const;
	bool  IsRespawnFailed() const;
	bool  IsRespawnUnacked() const;
	bool  IsRespawnPendingCloneRemove() const;
	void  RemoteVaultMbrFix();

    // Tries to pass control of this entity when it is going out of scope with our players.
    virtual void TryToPassControlProximity();
    virtual bool TryToPassControlOutOfScope();

    // Tries to pass control of this object to another machine when the player enters a tutorial session
	virtual void TryToPassControlFromTutorial();

    bool IsInScopeNoVehicleChecks(const netPlayer& player, unsigned *scopeReason = NULL) const;
    bool IsInScopeVehicleChecks(const netPlayer& player, unsigned *scopeReason = NULL) const;
	
	virtual bool CanPassControlWithNoSyncData() const { return false; }

	// TaskWrithe, TaskGun and TaskGetUp use a task target - this code removes the duplication for updating the task target or setting the out of scope ID for a clone.
	static void	UpdateNonPlayerCloneTaskTarget(CPed const* ped, CAITarget& target, ObjectId& objectId);

    // these functions set the correct states for cloned peds being added or removed from vehicles
    bool SetClonePedInVehicle(class CVehicle* pVehicle, const s32 iSeat);
    void SetClonePedOutOfVehicle(bool bClearMyVehiclePtr, const u32 flags);

    void	  SetClonePedOnMount(CPed* pMount);
    void      SetClonePedOffMount();
    s32       GetPedsTargetSeat() const { return m_targetVehicleSeat; }
    void      SetPedsTargetSeat(s32 seat) { m_targetVehicleSeat = seat; UpdateVehicleForPed(); }
    const ObjectId &GetPedsTargetVehicleId() const { return m_targetVehicleId; }
    CVehicle* GetPedsTargetVehicle(s32* pSeat = NULL);
    void      IgnoreVehicleUpdates(u16 timeToIgnore) { m_targetVehicleTimer = timeToIgnore; }
    void      IgnoreTargetMountUpdatesForAWhile() { m_targetMountTimer = 30; }

	u32   GetLastRequestedSyncSceneTime() const { return m_lastRequestedSyncScene; }
	void  SetLastRequestedSyncSceneTime(u32 timeStamp) { m_lastRequestedSyncScene = timeStamp; }

    // forces all task data to be sent in the next send to each player
    void ForceResendOfAllTaskData();

	// forces a full resync of all nodes
	void ForceResendAllData();

    // functions for giving network peds scripted tasks (even if controlled by remote machines)
    void AddPendingScriptedTask(CTask *task);
    void UpdatePendingScriptedTask();
    void GivePedScriptedTask(CTask *task, int scriptedTaskType, const char* pszScriptName = "", int iScriptThreadPC = -1);
    int  GetPendingScriptedTaskStatus() const;

    static CPedBlenderData *ms_pedBlenderDataInWater;
    static CPedBlenderData *ms_pedBlenderDataInAir;
    static CPedBlenderData *ms_pedBlenderDataTennis;
    static CPedBlenderData *ms_pedBlenderDataOnFoot;
    static CPedBlenderData *ms_pedBlenderDataFirstPerson;
    static CPedBlenderData *ms_pedBlenderDataHiddenUnderMapForVehicle;

    bool IsUsingRagdoll() const { return m_bUsingRagdoll; };

	// sync parser getter functions
    void GetPedCreateData(CPedCreationDataNode& data);

    void GetScriptPedCreateData(CPedScriptCreationDataNode& data);

	void GetPedGameStateData(CPedGameStateDataNode& data);

	void GetPedScriptGameStateData(CPedScriptGameStateDataNode& data);

    void GetSectorPosMapNode(CPedSectorPosMapNode &data);
    void GetSectorNavmeshPosition(CPedSectorPosNavMeshNode& data);
    void GetPedHeadings(CPedOrientationDataNode& data);
    void GetWeaponData(u32 &weapon, bool &weaponObjectExists, bool &weaponObjectVisible, u8 &tintIndex, bool &flashLightOn, bool &hasAmmo, bool &attachLeft, bool *doingWeaponSwap = NULL);
    u32  GetGadgetData(u32 equippedGadgets[MAX_SYNCED_GADGETS]);
	u32	 GetWeaponComponentData(u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS], u8 weaponComponentTints[MAX_SYNCED_WEAPON_COMPONENTS]);
    void GetVehicleData(ObjectId &vehicleID, u32 &seat);
	void GetMountData(ObjectId& mountID);
    void GetGroupLeader(ObjectId &groupLeaderID);

    void GetPedHealthData(CPedHealthDataNode& data);

    void GetAttachmentData(CPedAttachDataNode& data);

    void GetMotionGroup(CPedMovementGroupDataNode& data);
    void GetPedMovementData(CPedMovementDataNode& data);

    void GetPedAppearanceData(CPackedPedProps &pedProps, CSyncedPedVarData& variationData, u32 &phoneMode, u8& parachuteTintIndex, u8& parachutePackTintIndex, fwMvClipSetId& facialClipSetId, u32 &facialIdleAnimOverrideClipNameHash, u32 &facialIdleAnimOverrideClipDictHash, bool& isAttachingHelmet, bool& isRemovingHelmet, bool& isWearingHelmet, u8& helmetTexture, u16& helmetProp, u16& visorPropUp, u16& visorPropDown, bool& visorIsUp, bool& supportsVisor, bool& isVisorSwitching, u8& targetVisorState);

    void GetAIData(CPedAIDataNode& data);
	void GetTaskTreeData(CPedTaskTreeDataNode& data);
	void GetTaskSpecificData(CPedTaskSpecificDataNode& data);

	void GetPedInventoryData(CPedInventoryDataNode& data);
                             
    void GetPedComponentReservations(CPedComponentReservationDataNode& data);

	void GetTaskSequenceData(bool& hasSequence, u32& sequenceResourceId, u32& numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32& repeatMode);

   // sync parser setter functions
    void SetPedCreateData(const CPedCreationDataNode& data);

    void SetScriptPedCreateData(const CPedScriptCreationDataNode& data);

    void SetPedGameStateData(const CPedGameStateDataNode& data);

    void SetPedScriptGameStateData(const CPedScriptGameStateDataNode& data);

    void SetSectorPosMapNode(const CPedSectorPosMapNode &data);
    void SetSectorPosition(const float sectorPosX, const float sectorPosY, const float sectorPosZ);
    void SetSectorNavmeshPosition(const CPedSectorPosNavMeshNode& data);
    void SetPedHeadings(const CPedOrientationDataNode& data);
    void SetWeaponData(u32 weapon, bool weaponObjectExists, bool weaponObjectVisible, const u8 tintIndex, const bool flashLightOn, const bool hasAmmo, const bool attachLeft, bool forceUpdate = false);
    void SetGadgetData(const u32 equippedGadgets[MAX_SYNCED_GADGETS], const u32 numGadgets);
    void SetWeaponComponentData(u32 weapon, const u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS], const u8 weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS], const u32 numWeaponComponents);
	const u32 GetWeaponComponentsCount(u32 weapon) const;
    void SetVehicleData(const ObjectId vehicleID, u32 seat);
	void SetMountData(const ObjectId mountID);
    void SetGroupLeader(const ObjectId groupLeaderID);

	virtual void SetPedHealthData(const CPedHealthDataNode& data);
    void SetAttachmentData(const CPedAttachDataNode& data);
	void SetMotionGroup(const CPedMovementGroupDataNode& data);
    void SetPedMovementData(const CPedMovementDataNode& data);
    
    void SetPedAppearanceData(const CPackedPedProps pedProps, const CSyncedPedVarData& variationData, const u32 phoneMode, const u8 parachuteTintIndex, const u8 parachutePackTintIndex, const fwMvClipSetId& facialClipSetId, const u32 facialIdleAnimOverrideClipHash, const u32 facialIdleAnimOverrideClipDictHash, const bool isAttachingHelmet, const bool isRemovingHelmet, const bool isWearingHelmet, const u8 helmetTexture, const u16 helmetProp, const u16 visorUpPropId, const u16 visorDownPropId, const bool visorIsUp, const bool supportsVisor, const bool isVisorSwitching, const u8 targetVisorState, bool forceApplyAppearance = false);

    void SetAIData(const CPedAIDataNode& data);
	virtual void SetTaskTreeData(const CPedTaskTreeDataNode& data);

	virtual void SetTaskSpecificData(const CPedTaskSpecificDataNode& data);

	void SetPedInventoryData(const CPedInventoryDataNode& data);
                             
    void SetPedComponentReservations(const CPedComponentReservationDataNode& data);

	void SetTaskSequenceData(u32 numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32 repeatMode);

	void PostTasksUpdate();
	void UpdateTaskSpecificData();

	void SetIsTargettableByTeam(int team, bool bTargettable);
	bool GetIsTargettableByTeam(int team) const;

	void SetIsTargettableByPlayer(int player, bool bTargettable);
	bool GetIsTargettableByPlayer(int player) const;

	void SetIsDamagableByPlayer(int player, bool bTargettable);
	bool GetIsDamagableByPlayer(int player) const;
	bool GetArePlayerDamagableFlagsSet() const { return m_isDamagableByPlayer != rage::PlayerFlags(~0); }
	u32 GetPlayerDamagableFlags() const { return m_isDamagableByPlayer; }

    void SelectCurrentClipSetForClone();

	// If the ped has been killed, register that kill with the network damage tracker....
	void RegisterKillWithNetworkTracker();

	// registers the ped being head shotted to death with the network tracker....
	void RegisterHeadShotWithNetworkTracker();

    // this function maintains a cache of currently running tasks on this ped to ensure they remain in the same task slot
    void UpdateLocalTaskSlotCache();

	//Called on the clone when the main player has resurrected
	virtual void  ResurrectClone();

	CTaskAmbientMigrateHelper*		GetMigrateTaskHelper() const	{ return m_pMigrateTaskHelper;}
	CTaskAmbientMigrateHelper*		GetMigrateTaskHelper()			{ return m_pMigrateTaskHelper;}
	void							ClearMigrateTaskHelper(bool bAtCleanUp=false);

	static void ExtractTaskSequenceData(ObjectId objectID, const CTaskSequenceList& taskSequenceList, u32& numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32& repeatMode);

	void StoreLocalOffset(Vector3& offset, Vector3& velocityDelta) { m_localOffset = offset; m_localVelocityDelta = velocityDelta; }

	const Vector3& GetLocalOffset() { return m_localOffset; }
	const Vector3& GetLocalVelocityDelta() { return m_localVelocityDelta; }

#if !__FINAL
	void DumpTaskInformation(const char* callFrom);
#endif // !__FINAL

	bool IsFadingOut();
	void FadeOutPed(bool fadeOut);

	// some script peds drop a script specified amount of ammo when they are killed
	void SetAmmoToDrop(u32 ammo) { m_ammoToDrop = (u8) ammo; }
	u32  GetAmmoToDrop() const   { return (u32) m_ammoToDrop; }

	// some script peds damage will be given to a certain player.
	const PhysicalPlayerIndex& GetAttributeDamageTo() const { return m_AttDamageToPlayer; }
	void SetAttributeDamageTo(const PhysicalPlayerIndex& obj) { m_AttDamageToPlayer=obj; }

	void SetForceCloneTransitionChangeOwner() { gnetDebug3("Setting ped: %s for force clone transition owner", GetLogName()); sysStack::PrintStackTrace(); m_ForceCloneTransitionChangeOwner = true; }

	void TakeControlOfOrphanedMoveNetworkTaskSubNetwork( fwMoveNetworkPlayer* orphanMoveNetworkPlayer, float const blendOutDuration = NORMAL_BLEND_DURATION, u32 const flags = 0);

	void HandleUnmanagedCloneRagdoll();

	float GetDesiredHeading() const { return m_DesiredHeading; }

	void ProcessFailmark();
	void ProcessFailmarkInventory();
	void ProcessFailmarkHeadBlend();
	void ProcessFailmarkDecorations();
	void ProcessFailmarkVariations();
	void ProcessFailmarkColors();

    // network ped handling of AI events related to gun shots
    static void OnAIEventAdded(const fwEvent& event);
    static void OnInformSilencedGunShot   (CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle);

	void SetHasDroppedWeapon(bool hasDropped) { m_hasDroppedWeapon = hasDropped; }

public:

	// TaskParachute needs to be able to wipe the parachute attachment if the clone task finishes earlier than the owner task....
	virtual void ClearPendingAttachmentData();

	//---

	void GetHelmetAttachmentData(bool& isAttachingHelmet, bool& isRemovingHelmet, bool& isWearingHelmet, bool& isVisorSwitching, u8& targetVisorState);
	void SetHelmetAttachmentData(const bool& isAttachingHelmet, const bool& isRemovingHelmet, const bool& isWearingHelmet, const bool& isVisorSwitching, const u8& targetVisorState);
	
	void GetHelmetComponentData(u8& textureId, u16& prop, u16& visorPropUp, u16& visorPropDown, bool& visorIsUp, bool& supportsVisor);
	void SetHelmetComponentData(u8 const textureId, u16 const prop, u16 const visorUpropId, u16 const visorDownPropId, const bool visorIsUp, const bool supportsVisor);

	void UpdateHelmet(void);
	
	inline bool IsOwnerWearingAHelmet(void) const	{ return m_bIsOwnerWearingAHelmet;	 }
	inline bool IsOwnerAttachingAHelmet(void) const { return m_bIsOwnerAttachingAHelmet; }
	inline bool IsOwnerRemovingAHelmet(void)  const { return m_bIsOwnerRemovingAHelmet;  }
	inline bool IsOwnerSwitchingVisor(void)  const  { return m_bIsOwnerSwitchingVisor;  }
	inline int GetOwnerTargetVisorState(void)  const  { return m_nTargetVisorState;  }

	bool IsAttachingAHelmet(void) const;
	bool IsRemovingAHelmet(void) const;

	//---

	void GetPhoneModeData(u32& phoneMode);
	void SetPhoneModeData(const u32& phoneMode);
	void UpdatePhoneMode();

	void GetTennisMotionData(CSyncedTennisMotionData& tennisData);
	void SetTennisMotionData(const CSyncedTennisMotionData& tennisData);
	void UpdateTennisMotionData();

	void ClearNetTennisMotionData();
	void SetNetTennisMotionData(const CSyncedTennisMotionData& tennisData);

	void OnPedClearDamage();

public:
	bool IsLocalEntityCollidableOverNetwork(void);
protected:

    // updates the ped blender data to use based on various parameters
    virtual void UpdateBlenderData();

	void UpdateOrphanedMoveNetworkTaskSubNetwork();
	void FreeOrphanedMoveNetworkTaskSubNetwork();

	bool GetCachedEntityCollidableState(void);

    static const unsigned int MAX_TREE_DEPTH = 8; // maximum tree depth that can be synced

	virtual u8 GetHighestAllowedUpdateLevel() const;

    virtual bool CanBlendWhenAttached() const;

    virtual bool IsAttachmentStateSentOverNetwork() const;
	virtual bool CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const;
	virtual bool AttemptPendingAttachment(CPhysical* pPhysical, unsigned* reason = nullptr);
    virtual void AttemptPendingDetachment(CPhysical* pEntityToAttachTo);
	virtual void UpdatePendingAttachment();

	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;
    virtual bool UseBoxStreamerForFixByNetworkCheck() const { return m_NeedsNavmeshFixup; }
    virtual bool CheckBoxStreamerBeforeFixingDueToDistance() const;
    virtual bool ShouldDoFixedByNetworkInAirCheck() const;
    virtual void UpdateFixedByNetwork(bool fixByNetwork, unsigned fixedReason, unsigned npfbnReason);

	virtual int GetVehicleWeaponIndex() { return -1; }

    // setup the ped AI as locally controlled
    void SetupPedAIAsLocal();

    // setup the ped AI as a network clone
    void SetupPedAIAsClone();

	// updates the vehicle for this ped
	virtual void UpdateVehicleForPed(bool skipTaskChecks = false);

    // update the ped's weapon slots
    void UpdateWeaponSlots(u32 chosenWeaponType, bool forceUpdate = false);

	// description: this function is responsible to update the local task slot cache before a force sync.
	void  UpdateLocalTaskSlotCacheForSync();

	//Retrieve the task slot data
	//  - Due to the task slot cache for a respawn ped being empty when it receives a task specific update.
	void GetTaskSlotData(TaskSlotCache& taskSlotData) const
	{
		taskSlotData = m_taskSlotCache;
	}

	void ResetTaskSlotData()
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "RESET_TASK_SLOT_CACHE", GetLogName());
		m_taskSlotCache.Reset();
	}

    void GetStandingOnObjectData(ObjectId &standingOnNetworkObjectID, Vector3 &localOffset) const;

public:

	struct CachedWeaponInfo
	{
		CachedWeaponInfo()
		{
			Reset();
		}

		void Reset()
		{
			m_pending               = false;
			m_weapon                = 0;
			m_weaponObjectExists    = false;
			m_weaponObjectVisible   = false;
            m_weaponObjectTintIndex = 0;
			m_numWeaponComponents   = 0;
			m_numGadgets            = 0;

			for(u32 index = 0; index < MAX_SYNCED_WEAPON_COMPONENTS; index++)
			{
				m_weaponComponentsTint[index] = 0;
			}
			for(u32 index = 0; index < MAX_SYNCED_WEAPON_COMPONENTS; index++)
			{
				m_weaponComponents[index] = 0;
			}
			for(u32 index = 0; index < MAX_SYNCED_GADGETS; index++)
			{
				m_equippedGadgets[index] = 0;
			}
		}

		bool		m_pending;
		u32			m_weapon;
		bool		m_weaponObjectExists;
		bool		m_weaponObjectVisible;
		bool        m_weaponObjectFlashLightOn;
		bool		m_weaponObjectHasAmmo;
		bool		m_weaponObjectAttachLeft;
        u8          m_weaponObjectTintIndex;
		u8			m_weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS];
		u32			m_weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS];
		u32			m_numWeaponComponents;
		u32			m_equippedGadgets[MAX_SYNCED_GADGETS];
		u32			m_numGadgets;
	};

	bool GetIsVaulting()	{ return m_Vaulting; }

	void UpdateCachedWeaponInfo(bool const forceUpdate = false);
	bool CacheWeaponInfo();
	CachedWeaponInfo const& GetCachedWeaponInfo(void) { return m_CachedWeaponInfo;	}

	void SetCachedWeaponInfo(   const u32      weapon,
								const bool     weaponObjectExists,
								const bool     weaponObjectVisible,
                                const u8       weaponObjectTintIndex,
								const u8	   weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS],
								const u32      weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS],
								const u32      numWeaponComponents,
								const u32      equippedGadgets[MAX_SYNCED_GADGETS],
								const u32      numGadgets,
								const bool	   weaponObjectFlashLightOn,
								const bool	   weaponObjectHasAmmo,
								const bool	   weaponObjectAttachLeft);

	void SetChangeNextPedHealthDataSynch(const u32 hash) {m_changenextpedhealthdatasynch = hash;}

	void SetHasValidWeaponClipset(bool val)				 { m_HasValidWeaponClipset = val; }
	bool GetHasValidWeaponClipset()                      { return m_HasValidWeaponClipset; }

	void SetMaxEnduranceAlteredByScript() { m_MaxEnduranceSetByScript = true; }

private:

    //PURPOSE
    // Stores information about scripted tasks that have been given to this ped.
    // There are two purposes for data to be cached in this structure - if a task
    // is given to the ped by script while it is locally owned but pending owner change
    // the data must be cached until the ownership is resolved.
    // When a task has been given to remote controlled ped the data in this structure is
    // used to wait for the response from the owner of the ped.
    struct CachedScriptedTaskInfo
    {
        CachedScriptedTaskInfo() :
        m_TaskInfo(0)
        , m_TaskType(CTaskTypes::MAX_NUM_TASK_TYPES)
        , m_TaskUpdateTimer(0)
        , m_WaitingForNetworkEvent(false)
        {
            Reset();
        }

        void Reset()
        {
            if(m_TaskInfo)
            {
                delete m_TaskInfo;
                m_TaskInfo = 0;
            }

            m_TaskType               = CTaskTypes::MAX_NUM_TASK_TYPES;
            m_TaskUpdateTimer        = 0;
            m_WaitingForNetworkEvent = false;
        }

        CTaskInfo *m_TaskInfo;               // The task info describing the data for the given task
        u16        m_TaskType;               // The task type of the given task
        u16        m_TaskUpdateTimer;        // A timer to waiting for a task update to arrive with the given task
        bool       m_WaitingForNetworkEvent; // Flag indicating there is a pending network event giving this ped a task
    };

	void	SetMigrateTaskHelper(CTaskAmbientMigrateHelper* pMigrateTaskHelper) 
	{  
		m_frameMigrateHelperSet = fwTimer::GetSystemFrameCount();
		m_pMigrateTaskHelper = pMigrateTaskHelper;
	}
	void	CheckPedsCurrentTasksForMigrateData();

    // Create clones communication messages locally so they don't need to be broadcast
    void CreateCloneCommunication();

	bool TaskIsOverridingProp();

    // sets m_requiredProp if a play random ambients task is running
    void SetRequiredProp();

    // adds the required prop if a play random ambients task is running
    void AddRequiredProp();

	// update the currently equipped weapon for ped
	void UpdateWeaponForPed();

    // update weapon status from the ped
    void UpdateWeaponStatusFromPed();

	// update the weapon visibility from the ped
	void UpdateWeaponVisibilityFromPed();

	// reorders the peds task infos based on their tree depths
    void ReorderTasksBasedOnTreeDepths();

    void UpdateMountForPed();

    // creates clone tasks from the local tasks running on the ped
	void CreateCloneTasksFromLocalTasks();

    // creates local tasks from the clone tasks running on the ped
	void CreateLocalTasksFromCloneTasks();

    // handle ownership changes on clone tasks (called when ped changes owner between two remote machines)
    void HandleCloneTaskOwnerChange(eMigrationType migrationType);

    // updates pending movement data
    void UpdatePendingMovementData();

	// update the ped props
	void UpdatePendingPropData();

	// updates based on movement type for peds....
	bool IsPendingMovementStateLocallyControlledByClone();
	bool UpdatePedPendingMovementData();
	bool UpdatePedLowLodPendingMovementData();
	bool UpdateBirdPendingMovementData(s32 primaryMotionTaskType); // If they turned into a bird :)

	// Updates peds that have local flag LOCALFLAG_RESPAWNPED set:
	//   Once all players have all acknowledge the Respawn event we wait until this ped is not cloned 
	//   on failed players and unset the flag.
	void  UpdateLocalRespawnFlagForPed();

	// switches on and deals with low lod physics
	void ProcessLowLodPhysics();

	// Update all damage trackers and trigger damage events.
	void UpdateDamageTrackers(const CPedHealthDataNode& data);

    // Updates a ped position with a heavily quantised z position (that is sent for peds
    // on the navmesh as a bandwidth optimisation) to a valid ground position
    bool FixupNavmeshPosition(Vector3 &position);

	void SetCacheUsingInfiniteAmmo(const bool bValue) { m_CacheUsingInfiniteAmmo = bValue; };

    void UpdateForceSendRequests();

    void CheckForPedStopping();

    void DestroyPed();

#if __DEV
    void AssertPedTaskInfosValid();
#endif // __DEV

protected:

#if __BANK
    virtual u32 GetNetworkInfoStartYOffset();
    virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement);
    virtual void DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const;
    virtual void GetOrientationDisplayString(char *buffer) const;
    virtual void GetHealthDisplayString(char *buffer) const;
    virtual const char *GetNetworkBlenderIsOverridenString(unsigned resultCode) const;
    virtual const char *GetCanPassControlErrorString(unsigned cpcFailReason, eMigrationType migrationType) const;
#endif

    static void OnGunShotBulletImpactEvent(const CEventGunShotBulletImpact &aiEvent);
    static void OnGunShotWhizzedByEvent   (const CEventGunShotWhizzedBy    &aiEvent);

    static CPedSyncTree    *ms_pedSyncTree;
    static CPedScopeData   ms_pedScopeData;

    TaskSlotCache		   m_taskSlotCache;

    CachedScriptedTaskInfo m_CachedTaskToGive;          // Stores information about a task being given to a ped controlled by another machine

	CachedWeaponInfo	   m_CachedWeaponInfo;

	fwMvClipSetId          m_pendingMotionSetId;
	fwMvClipSetId          m_pendingOverriddenWeaponSetId;
    fwMvClipSetId          m_pendingOverriddenStrafeSetId;
	s32	    		  	   m_PendingMoveBlenderState;	
	s32					   m_PendingMoveBlenderType;	// Ped, Ped Low Lod, Horse etc    strStreamingObjectName m_requiredProp;
	strStreamingObjectName m_requiredProp;
	u32                    m_pendingRelationshipGroup;
    float                  m_pendingAttachHeading;
    float                  m_pendingAttachHeadingLimit;

	u32					   m_pendingLookAtFlags;

	u32					   m_lastSyncedWeaponHash;
	u32                    m_changenextpedhealthdatasynch;

    float                  m_DesiredHeading;
    float                  m_DesiredMoveBlendRatioX;
    float                  m_DesiredMoveBlendRatioY;
	float				   m_DesiredMaxMoveBlendRatio;

    bool                   m_bAllowSetTask                      : 1;
    bool                   m_CanAlterPedInventoryData           : 1;
    bool                   m_bUsingRagdoll                      : 1;
	bool                   m_weaponObjectExists                 : 1;
	bool                   m_weaponObjectVisible                : 1;
	bool                   m_inVehicle                          : 1;
    bool                   m_registeredWithKillTracker          : 1;
	bool                   m_registeredWithHeadshotTracker      : 1;
	bool				   m_onMount					        : 1;
	bool				   m_TaskOverridingPropsWeapons	        : 1;
	bool				   m_TaskOverridingProps		        : 1;
	bool				   m_ForceTaskUpdate			        : 1;
    bool                   m_IsDiving                           : 1;
	bool				   m_IsDivingToSwim						: 1;
	bool				   m_IsSwimming							: 1;
    bool                   m_UsesCollisionBeforeTaskOverride    : 1;
	bool				   m_weaponObjectFlashLightOn		    : 1;
	bool				   m_CacheUsingInfiniteAmmo			    : 1;
	bool				   m_weaponObjectHasAmmo			    : 1;
	bool				   m_weaponObjectAttachLeft				: 1;
    bool                   m_UpdateBlenderPosAfterVehicleUpdate : 1;
    bool                   m_Wandering                          : 1;
	bool				   m_Vaulting							: 1;
	bool				   m_ForceCloneTransitionChangeOwner	: 1;
	bool				   m_OverrideScopeBiggerWorldGridSquare	: 1;
	bool				   m_PreventSynchronise					: 1;
    bool                   m_RunningSyncedScene                 : 1;
    bool                   m_ZeroMBRLastFrame                   : 1;
    bool                   m_PreventTaskUpdateThisFrame         : 1;
	bool                   m_RappellingPed						: 1;  
    bool                   m_HasStopped                         : 1;
    bool                   m_HiddenUnderMapDueToVehicle         : 1;
	bool				   m_hasDroppedWeapon					: 1;
	
	//PRF
	bool				   m_PRF_disableInVehicleActions		: 1;
	bool				   m_PRF_IgnoreCombatManager			: 1;

    u8                     m_weaponObjectTintIndex;

	u8					   m_ClearDamageCount;

	TeamFlags              m_isTargettableByTeam;	// flags for each team, indicating whether this ped can be targetted by a team
	u32				       m_isTargettableByPlayer;	// flags for each player, indicating whether this ped can be targetted by a player
	u32				       m_isDamagableByPlayer;	// flags for each player, indicating whether this ped can be damaged by a player

	s16					   m_temporaryTaskSequenceId;	// used when a script ped is running a temporary sequence without the associated script that created it
	s16					   m_scriptSequenceId;			// the cached sequence id of the script sequence that should be running on the ped which we do not have

	u8					   m_ammoToDrop;			// some script peds drop a script specified amount of ammo when they are killed

	u8					   m_numRemoteWeaponComponents;

	u8					   m_uMaxNumFriendsToInform;
	float				   m_fMaxInformFriendDistance;

	ObjectId			   m_pendingLookAtObjectId;

	u32					   m_PhoneMode;
	bool				   m_bNewPhoneMode;

	bool				   m_bIsOwnerWearingAHelmet:1;			// Is the owner wearing a helmet?
	bool				   m_bIsOwnerRemovingAHelmet:1;			// Is the owner taking a helmet off?
	bool				   m_bIsOwnerAttachingAHelmet:1;		// Is the owner putting a helmet on?
	bool				   m_bIsOwnerSwitchingVisor:1;

	int					   m_nTargetVisorState;

	bool                   m_HasValidWeaponClipset:1;           // do we have a valid weapon clip set? Used as part of a hack fix for when pointing while holding a weapon

	bool                   m_MaxEnduranceSetByScript : 1;

	//This is used to know if the last damage event was done using a headshoot
	bool  m_killedWithHeadShot;
	//This is used to know if the last damage event was done using a melee damage.
	bool  m_killedWithMeleeDamage;

	CNetTennisMotionData*	m_pNetTennisMotionData;

	Vector3				   m_localOffset;
	Vector3				   m_localVelocityDelta;

private:

#if __ASSERT && __BANK

	void					ValidateMotionGroup(CPed* pPed);	

#endif /* __ASSERT && __BANK*/

	inline void				SetForceMotionTaskUpdate(bool const flag)	{ m_ForceTaskUpdate = flag;		}
	inline bool				GetForceMotionTaskUpdate(void) const		{ return m_ForceTaskUpdate;		}
	
	void					SetDeadPedRespawnRemovalTimer();

	fwClipSetRequestHelper  m_motionSetClipSetRequestHelper;
	fwClipSetRequestHelper  m_overriddenWeaponSetClipSetRequestHelper;
    fwClipSetRequestHelper  m_overriddenStrafeSetClipSetRequestHelper;

	ObjectId               m_targetVehicleId;
	ObjectId               m_myVehicleId;		// this is shite but its too late to hack around with the existing target vehicle stuff
    s32                    m_targetVehicleSeat;
    u16                    m_targetVehicleTimer;

	ObjectId			   m_targetMountId;
	s8					   m_targetMountTimer;

	bool m_NeedsNavmeshFixup;

	bool m_OverridingMoveBlendRatios;

	//Locally Track if last damage event has been set.
	bool  m_pendingLastDamageEvent;

	PlayerFlags					   m_RespawnFailedFlags;				// Flag respawn ped with players who have failed the respawn.
	PlayerFlags					   m_RespawnAcknowledgeFlags;			// Flag respawn ped with players who have acknowledge the respawn event.
	u32							   m_RespawnRemovalTimer;				// Remove dead peds after this times
	
	u32							   m_lastRequestedSyncScene;            // the timestamp of the last sync scene request from this player
	CTaskAmbientMigrateHelper*	   m_pMigrateTaskHelper;
	u32							   m_frameMigrateHelperSet;		        // The time the migrate task helper was set
	//----

	CPackedPedProps m_pendingPropUpdateData; 
	bool			m_pendingPropUpdate;

	// Ragdoll/vehicle force send - This is a bit of a hack to solve a nasty problem. Peds can be switched to
	// ragdoll and forced off a vehicle based on a weapon damage event. This forces a task update in response
	// to the ped switching to ragdoll, but the new task has not been given yet as the AI will not be updated
	// until after the network code has force sent the task data. This allows us to delay sending of this state
	// until after the AI update
	u8   m_FrameForceSendRequested;
	bool m_ForceSendRagdollState;
	bool m_ForceSendTaskState;

	bool m_ForceRecalculateTasks;

	//----

	bool m_blockBuildQueriableState;

	// On owners, an incoming AI task will receive a call to UpdateFSM( ) before the outgoing AI task has CleanUp( ) called.
	// This is so the incoming move network can replace the outgoing movenetwork without a gap where the ped will T-pose.
	// This doesn't happen on clones (outgoing task is ended / CleanUp( ) before new task gets a call to UpdateClonedFSM( ) 
	// so we occasionally need to leave the move network on the tree and keep an eye on it for a few frames. If it's still 
	// in the tree after X frames when it should have been replaced, we get rid of it
	struct OrphanMoveNetworkInfo
	{
		OrphanMoveNetworkInfo()
		:
			m_moveNetworkPlayer(NULL),
			m_blendDuration(NORMAL_BLEND_DURATION),
			m_flags(0),
			m_frameNum(0)
		{}

		void Reset(void)
		{
			SetNetworkPlayer(NULL);
			m_blendDuration		= NORMAL_BLEND_DURATION;
			m_flags				= 0;
			m_frameNum			= 0;
		}

		fwMoveNetworkPlayer const* GetNetworkPlayer(void) const
		{
			return m_moveNetworkPlayer;
		}

		fwMoveNetworkPlayer* GetNetworkPlayer(void)
		{
			return m_moveNetworkPlayer;
		}

		void SetNetworkPlayer(fwMoveNetworkPlayer* pPlayer)
		{
			if (m_moveNetworkPlayer)
			{
				m_moveNetworkPlayer->Release();
			}
			m_moveNetworkPlayer = pPlayer;
			if (m_moveNetworkPlayer)
			{
				m_moveNetworkPlayer->AddRef();
			}
		}

		inline float	GetBlendDuration(void)	const { return m_blendDuration;			}
		inline u32		GetFlags(void)			const { return m_flags;					}
		inline u32		GetFrameNum(void)		const { return m_frameNum;				}

		void			SetBlendDuration(float const blend) { m_blendDuration = blend;	}
		void			SetFlags(u32 const flags)			{ m_flags = flags;			}
		void			SetFrameNum(u32 const frameNum)		{ m_frameNum = frameNum;	}

	private:
		
		fwMoveNetworkPlayer* m_moveNetworkPlayer;	
		float				 m_blendDuration;
		u32					 m_flags;

		u32					 m_frameNum;
	};
	
	OrphanMoveNetworkInfo m_orphanMoveNetwork;

	//----

	// Ragdoll/vehicle force send - This is a bit of a hack to solve a nasty problem. Peds can be switched to
	// ragdoll and forced off a vehicle based on a weapon damage event. This forces a task update in response
	// to the ped switching to ragdoll, but the new task has not been given yet as the AI will not be updated
	// until after the network code has force sent the task data. This allows us to delay sending of this state
	// until after the AI update
    bool m_ForceSendGameState;

	// Scripted peds can have their damage be attributed to a player in the session
	PhysicalPlayerIndex   m_AttDamageToPlayer;

    // static list of peds not responding to clone bullets directly, we need to tell the owners of these peds
    // where our bullets have impacted as accuracy is important to them. This list is updated each frame, so
    // when bullets hit we don't have to loop through all peds a separate time to determine who needs to be informed
    static ObjectId ms_PedsNotRespondingToCloneBullets[MAX_NUM_NETOBJPEDS];
    static unsigned ms_NumPedsNotRespondingToCloneBullets;
    static unsigned ms_PedsNotRespondingToCloneBulletsUpdateFrame;

#if __BANK
	u32 m_NumInventoryItems;
	bool m_PedUsingInfiniteAmmo;
#endif

public:
	void  SetLastDamageEventPending(const bool value);
	bool  GetLastDamageEventPending() const {return m_pendingLastDamageEvent;}

	static bool ms_playerMaxHealthPendingLastDamageEventOverride; // override for pendingLastDamageEvent to be reset even if not set to full health

#if __ASSERT
	void   CheckDamageEventOnDie();
#endif // __ASSERT
};

#endif  // NETOBJPED_H
