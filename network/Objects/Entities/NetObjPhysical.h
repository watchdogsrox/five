//
// name:        NetObjPhysical.h
// description: Derived from netObject, this class handles all physical-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJPHYSICAL_H
#define NETOBJPHYSICAL_H

#include "vector/quaternion.h"

#include "network/General/NetworkGhostCollisions.h"
#include "network/objects/entities/NetObjDynamicEntity.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/synchronisation/syncnodes/physicalsyncnodes.h"

// framework includes
#include "entity/attachmententityextension.h"

class CNetworkDamageTracker;

struct CPhysicalBlenderData;

namespace rage
{
    class fwBoxStreamerSearch;
}

// a wrapper class that contains a pool of pending script obj infos
class CPendingScriptObjInfo
{
public:
	CPendingScriptObjInfo(CGameScriptObjInfo& objInfo) : m_info(objInfo) {}

	CGameScriptObjInfo m_info;

	FW_REGISTER_CLASS_POOL(CPendingScriptObjInfo);
};

class CNetObjPhysical : public CNetObjDynamicEntity, public IPhysicalNodeDataAccessor
{
public:
	//Network Damage information
	struct NetworkDamageInfo
	{
	public:
		NetworkDamageInfo(CEntity* inflictor
							,const float healthLost
							,const float armourLost
							,const float enduranceLost
							,const bool incapacitatesVictim
							,const u32 weaponHash
							,const int weaponComponent
							,const bool headShoot
							,const bool killsVictim
							,const bool withMeleeWeapon
							,phMaterialMgr::Id hitMaterial = 0)
			: m_Inflictor(inflictor)
			, m_HealthLost(healthLost)
			, m_ArmourLost(armourLost)
			, m_EnduranceLost(enduranceLost)
			, m_IncapacitatesVictim(incapacitatesVictim)
			, m_WeaponHash(weaponHash)
			, m_WeaponComponent(weaponComponent)
			, m_HeadShoot(headShoot)
			, m_KillsVictim(killsVictim)
			, m_WithMeleeWeapon(withMeleeWeapon)
			, m_HitMaterial(hitMaterial)
		{
		}

		~NetworkDamageInfo()
		{
			Clear();
		}

		void Clear()
		{
			m_Inflictor       = 0;
			m_HealthLost      = 0.0f;
			m_ArmourLost      = 0.0f;
			m_EnduranceLost	  = 0.0f;
			m_IncapacitatesVictim = false;
			m_HitMaterial	  = 0;
			m_WeaponHash      = 0;
			m_WeaponComponent = -1;
			m_HeadShoot       = false;
			m_KillsVictim     = false;
			m_WithMeleeWeapon = false;
		}

	public:
		CEntity*  m_Inflictor;
		float     m_HealthLost;
		float     m_ArmourLost;
		float	  m_EnduranceLost;
		bool	  m_IncapacitatesVictim;
		phMaterialMgr::Id m_HitMaterial;
		u32       m_WeaponHash;
		int       m_WeaponComponent;
		bool      m_HeadShoot;
		bool      m_KillsVictim;
		bool      m_WithMeleeWeapon;
	};

public:

    CNetObjPhysical(class CPhysical				*physical,
                    const NetworkObjectType		type,
                    const ObjectId				objectID,
                    const PhysicalPlayerIndex   playerIndex,
                    const NetObjFlags			localFlags,
                    const NetObjFlags			globalFlags);

    CNetObjPhysical() :
    m_isInWater(false),
    m_collisionTimer(0),
    m_damageTracker(0),
    m_lastDamageObjectID(NETWORK_INVALID_OBJECT_ID),
	m_lastDamageWeaponHash(0),
	m_alphaRampingTimeRemaining(0),
    m_pendingAttachmentObjectID(NETWORK_INVALID_OBJECT_ID),
    m_pendingOtherAttachBone(-1),
    m_pendingMyAttachBone(-1),
    m_pendingAttachFlags(0),
    m_pendingAllowInitialSeparation(true),
    m_pendingInvMassScaleA(1.0f),
    m_pendingInvMassScaleB(1.0f),
	m_pendingScriptObjInfo(0),
	m_pendingDetachmentActivatePhysics(false),
	m_pendingAttachStateMismatch(false),   
    m_LastTimeFixed(0),
    m_InAirCheckPerformed(false),
    m_ZeroVelocityLastFrame(false),
    m_TimeToForceLowestUpdateLevel(0),
    m_LowestUpdateLevelToForce(CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW),
    m_LastCloneStateCheckedForPredictErrors(0),
    m_bWasHiddenForTutorial(false),
    m_bPassControlInTutorial(false),
	m_bAlphaRamping(false),
	m_bAlphaIncreasing(false),
	m_bAlphaIncreasingNoFlash(false),
	m_iAlphaOverride(255),
	m_bHideWhenInvisible(false),
	m_bInCutsceneLocally(false),
	m_bInCutsceneRemotely(false),
	m_bGhost(false),
	m_bGhostedForGhostPlayers(false),
	m_GhostCollisionSlot(CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT),
	m_TriggerDamageEventForZeroDamage(false),
	m_TriggerDamageEventForZeroWeaponHash(false),
	m_LastReceivedVisibilityFlag(true),
	m_KeepCollisionDisabledAfterAnimScene(false)
#if __BANK
	,m_lastDamageWasSet(false)
#endif
    , m_bOverrideBlenderUntilAcceptingObjects(false)
    , m_bAttachedToLocalObject(false)
	, m_bForceSendPhysicalAttachVelocityNode	(false)
	, m_FrameForceSendRequested(0)
	, m_BlenderDisabledAfterDetachingFrame(0)
	, m_BlenderDisabledAfterDetaching(false)
	, m_entityConcealed(false)
	, m_entityConcealedLocally(false)
	, m_AllowMigrateToSpectator(false)
	, m_allowDamagingWhileConcealed(false)
	, m_hiddenUnderMap(false)
	, m_entityPendingConcealment(false)
	, m_MaxHealthSetByScript(false)
#if ENABLE_NETWORK_LOGGING
    , m_LastFixedByNetworkChangeReason(FBN_NONE)
	, m_LastNpfbnChangeReason(NPFBN_NONE)
	, m_LastFailedAttachmentReason(CPA_SUCCESS)
#endif // ENABLE_NETWORK_LOGGING
	, m_AllowCloneWhileInTutorial(false)
    {
#if __DEV
        m_scriptExtension = FindScriptExtension();
#endif
   }

    ~CNetObjPhysical();

    CPhysical* GetPhysical() const { return (CPhysical*)GetEntity(); }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    // called when this object collides with another
    virtual void RegisterCollision(CNetObjPhysical* pCollider, float impulseMag);

    // called when this object collides with a building
    void RegisterCollisionWithBuilding(float impulseMag);

    // called when this object is hit by an explosion
    void RegisterExplosionImpact(float impulseMag);

	// used to update to high update level for a short amount of time
	void ForceHighUpdateLevel();

    // returns the current value of the collision timer
    s32 GetCollisionTimer() const { return m_collisionTimer; }

    // returns the current value of the collision timer
    void SetCollisionTimer(s32 timer) { m_collisionTimer = timer; }

    // overrides the network blender until the local player is accepting objects
    virtual void OverrideBlenderUntilAcceptingObjects() { m_bOverrideBlenderUntilAcceptingObjects = true; }

    bool CanScriptModifyRemoteAttachment() const { return m_bAttachedToLocalObject; }
    void SetScriptModifyRemoteAttachment(bool canModify) { m_bAttachedToLocalObject = canModify; } 

    // returns whether this object is consider attached for syncing purposes
    virtual bool GetIsAttachedForSync() const;
	
	// returns whether migration is not allowed when this object is physically attached
	virtual bool PreventMigrationWhenPhysicallyAttached(const netPlayer& player, eMigrationType migrationType) const;

	// returns whether this clone object should be attached
	bool GetIsCloneAttached() const;

	// returns the entity this object is attached to, or should be attached to
	CEntity* GetEntityAttachedTo() const;

    // returns whether this network object is in water
    bool IsNetworkObjectInWater() { return m_isInWater; }
	bool IsPhysicallyAttached() const;

    virtual bool       IsAttachedOrDummyAttached() const;
    virtual CPhysical *GetAttachOrDummyAttachParent() const;

    void SetCanBeReassigned(bool canBeReassigned);
	void SetPassControlInTutorial(bool passControl) { m_bPassControlInTutorial = passControl; }
	virtual bool GetPassControlInTutorial() const { return m_bPassControlInTutorial; }

#if __ASSERT
	void SetAssertIfRemoved(const bool assert) { m_bScriptAssertIfRemoved = assert; }
	bool GetAssertIfRemoved() const { return m_bScriptAssertIfRemoved; }
#endif // __ASSERT

    // accessors for checking the last entity that damaged this physical object
    bool HasBeenDamagedBy(const CPhysical *physical) const;
    void ClearLastDamageObject();
	CEntity *GetLastDamageObjectEntity() const;
	u32 GetLastDamageWeaponHash() const { return m_lastDamageWeaponHash; }
	phMaterialMgr::Id GetLastDamagedMaterialId() { return m_lastDamagedMaterialId; }

    // see NetworkObject.h for a description of these functions
    virtual scriptObjInfoBase*          GetScriptObjInfo();
    virtual const scriptObjInfoBase*    GetScriptObjInfo() const;
    virtual void                        SetScriptObjInfo(scriptObjInfoBase* info);
    virtual scriptHandlerObject*		GetScriptHandlerObject() const;

    void        SetScriptExtension(CScriptEntityExtension* ext);

    static  void    AddBoxStreamerSearches(atArray<fwBoxStreamerSearch>& searchList);

    virtual bool    Update();
    virtual void    UpdateAfterPhysics();
	virtual void    UpdateAfterScripts();
    virtual void    CreateNetBlender();
    virtual u32     CalcReassignPriority() const;
    virtual bool    CanBlend(unsigned *resultCode = 0) const;
    virtual bool    NetworkBlenderIsOverridden(unsigned *resultCode = 0) const;
    virtual void    CleanUpGameObject(bool bDestroyObject);
    virtual bool    IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual bool	CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual void    ChangeOwner(const netPlayer& player, eMigrationType migrationType);
	virtual void    PlayerHasLeft(const netPlayer& player);
	virtual void	OnRegistered();
    virtual bool    OnReassigned();
    virtual void    PostSync();
	virtual bool	ShouldStopPlayerJitter() {return false;}
	virtual void	SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate = false);
	virtual void	OnUnregistered();
	virtual bool	PreventMigrationDuringAttachmentStateMismatch() const { return false;}
	virtual bool	CanApplyNodeData() const;
	virtual void	OnConversionToAmbientObject();

	// Tries to pass control of this object to another machine if a player from that machine is closer
	virtual void TryToPassControlProximity();

	bool RecursiveAreAllChildrenCloned(const CPhysical* parent, const netPlayer& player) const;
	bool RecursiveAreAnyChildrenOwnedByPlayer(const CPhysical* parent, const netPlayer& player, int &counter) const;

	// damage tracking
    void ActivateDamageTracking();
    void DeactivateDamageTracking();

    bool  IsTrackingDamage();
    virtual void  UpdateDamageTracker(const NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision = false);
	float GetDamageDealtByPlayer(const netPlayer& player);
	void  ResetDamageDealtByAllPlayers();

    virtual void ConvertToAmbientObject();

    virtual bool CanOverrideLocalCollision() const { return !m_bInCutsceneLocally; }

	virtual bool ShouldObjectBeHiddenForCutscene() const;
	virtual void HideForCutscene();
	virtual void ExposeAfterCutscene();

	virtual bool CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;

	const CGameScriptObjInfo* GetPendingScriptObjInfo() const { return m_pendingScriptObjInfo ? &m_pendingScriptObjInfo->m_info : 0; }

	void SetPendingScriptObjInfo(CGameScriptObjInfo& objInfo);
	void ApplyPendingScriptObjInfo();
	void ClearPendingScriptObjInfo();

    void ReapplyLastVelocityUpdate();

	void HideUnderMap();

	static CPhysicalBlenderData *ms_physicalBlenderData;

    static u32 ms_collisionSyncIgnoreTimeVehicle; // syncs are ignored for this period of time after a collision with a vehicle
    static u32 ms_collisionSyncIgnoreTimeMap;     // syncs are ignored for this period of time after a collision with the map
    static float ms_collisionMinImpulse;          // the minimum impulse that will trigger the ignoring of syncs for a while
    static u32 ms_timeBetweenCollisions;          // the minimum time allowed between the allowed setting of the collision timer

    // sync parser migration functions
    void GetPhysicalMigrationData(CPhysicalMigrationDataNode& data);
    void GetPhysicalScriptMigrationData(CPhysicalScriptMigrationDataNode& data);
    void SetPhysicalMigrationData(const CPhysicalMigrationDataNode& data);
    void SetPhysicalScriptMigrationData(const CPhysicalScriptMigrationDataNode& data);

   // sync parser getter functions
    void GetPhysicalGameStateData(CPhysicalGameStateDataNode& data);
    void GetPhysicalScriptGameStateData(CPhysicalScriptGameStateDataNode& data);
    void GetVelocity(s32 &packedVelocityX, s32 &packedVelocityY, s32 &packedVelocityZ);
    void GetAngularVelocity(CPhysicalAngVelocityDataNode& data);
    void GetPhysicalHealthData(CPhysicalHealthDataNode& data);
    void GetPhysicalAttachmentData(bool       &attached,
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
								   bool		  &activatePhysicsWhenDetached,
                                   bool       &isCargoVehicle) const;

    // sync parser setter functions
    void SetPhysicalGameStateData(const CPhysicalGameStateDataNode& data);
    void SetPhysicalScriptGameStateData(const CPhysicalScriptGameStateDataNode& data);
    void SetVelocity(const s32 packedVelocityX, const s32 packedVelocityY, const s32 packedVelocityZ);
    void SetAngularVelocity(const CPhysicalAngVelocityDataNode& data);
    void SetPhysicalHealthData(const CPhysicalHealthDataNode& data);
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
								   const bool		 activatePhysicsWhenDetached,
                                   const bool        isCargoVehicle);

	// If the physical object has been killed, register it with the kill tracker....
	virtual void RegisterKillWithNetworkTracker() {};

	bool IsPendingAttachment() const;

	// sets an alpha ramp which evenly fades in and out continously until told to stop
	void SetAlphaRampingContinuous(bool bNetwork) { SetAlphaRampingInternal(false, false, false, bNetwork); }
	// sets an alpha ramp which fades in with an increasing ramp and holds until told to stop
	void SetAlphaRampFadeInAndWait(bool bNetwork) { SetAlphaRampingInternal(true, false, false, bNetwork); }
	// sets an alpha ramp which fades in with an increasing ramp and quits once timed out
	void SetAlphaRampFadeInAndQuit(bool bNetwork) { SetAlphaRampingInternal(true, false, true, bNetwork); }
	// sets an alpha ramp which fades out with a diminishing ramp and leaves the entity invisible
	void SetAlphaRampFadeOut(bool bNetwork) { SetAlphaRampingInternal(true, true, true, bNetwork); }
	// sets an alpha ramp which fades out with a diminishing ramp and leaves the entity invisible
	void SetAlphaRampFadeOutOverDuration(bool bNetwork, s16 iDuration) { SetAlphaRampingInternal(true, true, true, bNetwork, iDuration); }
	
	void StopAlphaRamping();
	bool IsAlphaRamping() const { return m_bAlphaRamping; }
	bool IsAlphaRampingOut() const { return m_bAlphaRamping && m_bAlphaRampOut; }
	void GetAlphaRamping(s16& iAlphaOverride, bool& bIncreasing) { iAlphaOverride = m_iAlphaOverride; bIncreasing = m_bAlphaIncreasing; }
	s16 GetAlphaRampingTimeRemaining() const {return m_alphaRampingTimeRemaining; }

	void SetAlphaFading(bool bValue, bool bNetwork = true);
	bool IsAlphaFading() const { return m_bAlphaFading; }
	void SetAlphaIncreasing(bool bNetwork = true);

	bool IsAlphaActive();
	virtual void SetAlpha(u32 alpha);
	virtual void ResetAlphaActive();
	virtual void SetInvisibleAfterFadeOut() { SetIsVisible(false, "SetInvisibleAfterFadeOut"); SetRemotelyVisibleInCutscene(false); }
	virtual void SetHideWhenInvisible(bool bSet);
	bool GetHideWhenInvisible() const { return m_bHideWhenInvisible; }

	bool IsFadingOut() const { return (m_bAlphaFading || (m_bAlphaRamping && m_bAlphaRampOut)); }

	virtual void SetAsGhost(bool bSet BANK_ONLY(, unsigned reason, const char* scriptInfo = ""));
	bool IsGhost() const { return m_bGhost; }
	virtual bool CanShowGhostAlpha() const;
	void SetGhostedForGhostPlayers(bool bSet) { m_bGhostedForGhostPlayers = bSet; }
	bool IsGhostedForGhostPlayers() const { return m_bGhostedForGhostPlayers; }

	bool IsInGhostCollision() const { return m_GhostCollisionSlot != CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT; }
	void SetGhostCollisionSlot(int slot) { m_GhostCollisionSlot = (s8)slot; }
	void ClearGhostCollisionSlot() { m_GhostCollisionSlot = CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT; }
	int GetGhostCollisionSlot() const { return m_GhostCollisionSlot; }

	virtual bool IsGhostCollision(CNetObjPhysical& otherPhysical) { return IsGhost() && otherPhysical.IsGhost();}

	static void SetGhostAlpha(u8 alpha)			{ ms_ghostAlpha = alpha; }
	static void ResetGhostAlpha()				{ ms_ghostAlpha = DEFAULT_GHOST_ALPHA; }
	static void SetInvertGhosting(bool bSet);
	static bool GetInvertGhosting()				{ return ms_invertGhosting;}

	virtual void OnAttachedToPickUpHook();
	virtual void OnDetachedFromPickUpHook();

	void RequestForceSendOfPhysicalAttachVelocityState() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_bForceSendPhysicalAttachVelocityNode = true; }

	virtual void ForceSendOfCutsceneState();

	void SetIsInCutsceneLocally(bool bSet) { m_bInCutsceneLocally = bSet; }
	virtual void SetIsInCutsceneRemotely(bool bSet);
	bool IsInCutsceneLocally() const { return m_bInCutsceneLocally; }
	bool IsInCutsceneRemotely() const { return m_bInCutsceneRemotely; }
	bool IsInCutsceneLocallyOrRemotely() const { return m_bInCutsceneLocally||m_bInCutsceneRemotely; }

	bool WasHiddenForTutorial() const { return m_bWasHiddenForTutorial; }
	void DisableBlenderForOneFrameAfterDetaching(u32 frame) { m_BlenderDisabledAfterDetachingFrame = frame; m_BlenderDisabledAfterDetaching = true; }

	virtual bool IsConcealed() const { return m_entityConcealed || m_entityConcealedLocally; }
	void ConcealEntity(bool conceal, bool allowDamagingWhileConcealed = false);
	bool GetIsAllowDamagingWhileConcealed() { return m_allowDamagingWhileConcealed; }

	void SetAllowMigrateToSpectator(bool ignore) { m_AllowMigrateToSpectator = ignore; }
	virtual bool GetAllowMigrateToSpectator() const { return m_AllowMigrateToSpectator; }

	void SetTriggerDamageEventForZeroDamage(bool trigger) { m_TriggerDamageEventForZeroDamage = trigger; }
	bool GetTriggerDamageEventForZeroDamage() const { return m_TriggerDamageEventForZeroDamage; }
	void SetTriggerDamageEventForZeroWeaponHash(bool trigger) { m_TriggerDamageEventForZeroWeaponHash = trigger; }
	bool GetTriggerDamageEventForZeroWeaponHash() const { return m_TriggerDamageEventForZeroWeaponHash; }

	void SetMaxHealthAlteredByScript() { m_MaxHealthSetByScript = true; }

	void SetKeepCollisionDisabledAfterAnimScene(bool keep) { m_KeepCollisionDisabledAfterAnimScene = keep; }
	bool GetKeepCollisionDisabledAfterAnimScene() const { return m_KeepCollisionDisabledAfterAnimScene; }

	void SetAllowCloneWhileInTutorial(bool allowCloneWhileInTutorial) { m_AllowCloneWhileInTutorial = allowCloneWhileInTutorial; }

private:

	void SetAlphaRampingInternal(bool bTimedRamp, bool bRampOut, bool bQuitWhenTimerExpired, bool bNetwork, s16 iCustomDuration  = 0);

	// Update all damage trackers and trigger damage events.
	void UpdateDamageTrackers(const CPhysicalHealthDataNode& data);

	void ProcessAlpha();

protected:

    enum
    {
		NETWORK_ATTACH_FLAGS_START = rage::NUM_EXTERNAL_ATTACH_FLAGS // this is the start of the network specific physical attachment flags, used for syncing additional attachment information
    };

    void GetVelocitiesForLogOutput(Vector3 &velocity, Vector3 &angularVelocity) const;

    void UpdateBlenderState(bool positionUpdated, bool orientationUpdated,
                            bool velocityUpdated, bool angVelocityUpdated);

    CScriptEntityExtension* FindScriptExtension() const;

	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;
    virtual bool AllowFixByNetwork() const { return true; }
    virtual bool UseBoxStreamerForFixByNetworkCheck() const { return false; }
    virtual bool CheckBoxStreamerBeforeFixingDueToDistance() const;
    virtual bool ShouldDoFixedByNetworkInAirCheck() const { return IsScriptObject(); }
	virtual void SetFixedByNetwork(bool fixObject, unsigned reason, unsigned npfbnReason);
    virtual u8   GetDefaultUpdateLevel() const;
    virtual u8   GetLowestAllowedUpdateLevel() const;
	virtual bool HasCollisionLoadedUnderEntity() const;
	virtual void UpdateBlenderData() { };

    void SetLowestAllowedUpdateLevel(u8 lowestUpdateLevelToForce, u16 timeToForce);

	// ATTACHMENT METHODS
	void ProcessPendingAttachment();

	fwAttachmentEntityExtension* GetAttachmentExtension() const;
	CPhysical*	GetCurrentAttachmentEntity() const;
	bool		IsPendingAttachmentPhysical() const;

    virtual bool IsAttachmentStateSentOverNetwork() const { return true; }
	virtual bool CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const;
	virtual bool AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason = nullptr);
	virtual void UpdatePendingAttachment();
	virtual void AttemptPendingDetachment(CPhysical* pEntityToAttachTo);
	virtual void ClearPendingAttachmentData();

	// Functions to check for and hide objects when the local player or the object is in a tutorial
	virtual bool ShouldObjectBeHiddenForTutorial() const;
	virtual void HideForTutorial();
	virtual bool CanResetAlpha() const { return true; }

	virtual void ProcessFixedByNetwork(bool bForceForCutscene = false);

#if __BANK
	virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement);
#endif // __BANK

    virtual bool CanBlendWhenAttached() const { return false; }
    virtual bool CanBlendWhenFixed() const;
    virtual void ResetBlenderDataWhenAttached();

    virtual void UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason);

	virtual bool ShowGhostAlpha();
	virtual void ProcessGhosting();

	virtual bool ShouldAllowCloneWhileInTutorial() const { return m_AllowCloneWhileInTutorial; }

	void EnableCameraCollisions();
	void DisableCameraCollisionsThisFrame(); 

    // Keep track of the position and velocity at a specified network time. This is used
    // to locally predict from this data to decide when to send updates to remote machines
    struct SnapshotData
    {
        SnapshotData()
        {
            Reset(VEC3_ZERO, VEC3_ZERO, 0);
        }

        void Reset(const Vector3 &position, const Vector3 &velocity, u32 timestamp)
        {
            m_Position  = position;
            m_Velocity  = velocity;
            m_Timestamp = timestamp;
        }

        Vector3 m_Position;
        Vector3 m_Velocity;
        u32     m_Timestamp;
        bool    m_RequiresUpdate;
    };

    virtual bool          SendUpdatesBasedOnPredictedPositionErrors() const { return false;   }
    virtual float         GetPredictedPosErrorThresholdSqr (unsigned UNUSED_PARAM(updateLevel)) const { return 1.0f; }
    virtual SnapshotData *GetSnapshotData(unsigned UNUSED_PARAM(updateLevel)) { return 0; }

	Vector3		 m_posBeforeHide;
	Vector3      m_pendingAttachOffset;
	Quaternion   m_pendingAttachQuat;
	Vector3      m_pendingAttachParentOffset;
    s32          m_collisionTimer;
    s32          m_timeSinceLastCollision;
	u32          m_lastDamageWeaponHash;
	phMaterialMgr::Id m_lastDamagedMaterialId;
	s16			 m_alphaRampingTimeRemaining;
    ObjectId     m_lastDamageObjectID;
    ObjectId     m_pendingAttachmentObjectID;
    s16          m_pendingOtherAttachBone;
    s16          m_pendingMyAttachBone;
    u32          m_pendingAttachFlags;
    float        m_pendingInvMassScaleA;
    float        m_pendingInvMassScaleB;
	bool         m_pendingAllowInitialSeparation;
	bool		 m_pendingDetachmentActivatePhysics;
	mutable bool m_pendingAttachStateMismatch;

	s16			 m_iAlphaOverride;
	s16			 m_iCustomFadeDuration;

    u16          m_TimeToForceLowestUpdateLevel; // a timer that can be set to force the lowest update level for the specified duration
    u8           m_LowestUpdateLevelToForce;     // the lowest update level that is allowed for the object while the above timer is non-zero

	u8			 m_GhostCollisionSlot;			 // the current slot this object is occupying in the ghostCollisions array
	u32          m_BlenderDisabledAfterDetachingFrame;

	bool         m_isInWater              : 1;   // this is the last value of the in water flag received from the network - we need this so IS_IN_WATER commands are correct for far away clones
    bool         m_bWasHiddenForTutorial  : 1;
    bool         m_bPassControlInTutorial : 1;
	bool		 m_hiddenUnderMap			  : 1;

	bool		 m_bAlphaRamping          : 1;
	bool		 m_bAlphaFading           : 1;
	bool		 m_bAlphaRampOut          : 1;
	bool		 m_bAlphaRampTimerQuit    : 1;
	bool		 m_bNetworkAlpha          : 1;
	bool		 m_bHideWhenInvisible     : 1;
	bool		 m_bHasHadAlphaAltered    : 1;
	bool		 m_bInCutsceneLocally	  : 1;
	bool		 m_bInCutsceneRemotely	  : 1;
	
	bool		 m_bForceSendPhysicalAttachVelocityNode	 : 1;
	bool		 m_bGhost						         : 1; // entity is uncollideable with other players & ghosts  
	bool         m_bGhostedForGhostPlayers				 : 1; // if true, this entity will appear ghosted for players that are a ghost
	bool		 m_BlenderDisabledAfterDetaching         : 1; // If this is true, then don't blend this frame. This is disabled in the update function the next frame after it was set.
	bool         m_CameraCollisionDisabled				 : 1; // set when camera collisions have been disabled for a frame with this entity (used in ghost mode)
	bool         m_entityConcealed                       : 1; // Has this vehicle been concealed by script?
	bool         m_entityConcealedLocally                : 1; // Has this local vehicle been concealed by script?
	bool		 m_allowDamagingWhileConcealed			 : 1;
	bool		 m_entityPendingConcealment				 : 1;

	bool		 m_AllowMigrateToSpectator				 : 1; // Temporary flag to disable CanAcceptControl for script objects

	bool		 m_TriggerDamageEventForZeroDamage		 : 1; // If set when damaged by anything if the damage value is 0 or invinsible, damage event will still trigger locally only
	bool		 m_TriggerDamageEventForZeroWeaponHash	 : 1; // If set when damaged by anything if the gamage weapon hash is 0, the damage event will still trigger locally
	bool		 m_LastReceivedVisibilityFlag			 : 1; // Used for clones. Contains the last received visibility state
	bool		 m_MaxHealthSetByScript					 : 1; // Set when script alters the max health
	bool		 m_KeepCollisionDisabledAfterAnimScene	 : 1;

	bool		 m_AllowCloneWhileInTutorial			 : 1;

#if __ASSERT
	bool         m_bScriptAssertIfRemoved : 1; //Script can set/unset this flag to debug if a entity is removed
#endif

#if __DEV
    CScriptEntityExtension* m_scriptExtension;  // a ptr to the extension held in the game object (if it exists). Used to help with debugging
#endif

private:

    void UpdateFixedByNetworkInAirChecks(bool &fixObject, unsigned &reason);
    void CheckPredictedPositionErrors();

#if __BANK
    virtual void GetOrientationDisplayString(char *buffer) const;
    virtual void GetHealthDisplayString(char *buffer) const;
#endif // __BANK

	void UpdateForceSendRequests();
	u8   m_FrameForceSendRequested;

    bool     m_ZeroVelocityLastFrame;
    // fixed by network state - used to ensure objects don't get stuck in the air
    bool     m_InAirCheckPerformed;

	bool	 m_bAlphaIncreasing;
	bool	 m_bAlphaIncreasingNoFlash;
    bool     m_bOverrideBlenderUntilAcceptingObjects;
    bool     m_bAttachedToLocalObject;

    unsigned m_LastTimeFixed;

    struct FixedByNetworkInAirCheckData
    {
        FixedByNetworkInAirCheckData()
        {
            Reset();
        }

        void Reset()
        {
            m_ObjectID             = NETWORK_INVALID_OBJECT_ID;
            m_ProbeSubmitted       = false;
            m_TargetZ              = 0.0f;
            m_TimeZMovedSinceProbe = 0;
        }

        ObjectId m_ObjectID;
        bool     m_ProbeSubmitted;
        float    m_TargetZ;
        unsigned m_TimeZMovedSinceProbe;
    };

    static FixedByNetworkInAirCheckData m_FixedByNetworkInAirCheckData;

    CNetworkDamageTracker *m_damageTracker;

	// ambient entities that are getting converted into script entities store their script obj info, so that if they become local this is applied when they are converted
	CPendingScriptObjInfo* m_pendingScriptObjInfo;

    PlayerFlags m_LastCloneStateCheckedForPredictErrors;

    Vector3  m_LastVelocityReceived;
#if __BANK
	void DebugDesiredVelocity();
	Vector3 m_vDebugPosition;
#endif

#if ENABLE_NETWORK_LOGGING
    unsigned m_LastFixedByNetworkChangeReason;
	unsigned m_LastNpfbnChangeReason;
	int		 m_LastFailedAttachmentReason; 
#endif // ENABLE_NETWORK_LOGGING

	static const unsigned DEFAULT_GHOST_ALPHA = 128;
	static u8 ms_ghostAlpha;
	static bool ms_invertGhosting;

#if __BANK
public:
    static bool ms_bOverrideBlenderForFocusEntity;
	static bool ms_bDebugDesiredVelocity;
#endif

protected:
		BANK_ONLY( bool m_lastDamageWasSet; )
};

#endif  // NETOBJPHYSICAL_H
