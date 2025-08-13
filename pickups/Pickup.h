#ifndef PICKUP_H
#define PICKUP_H

// Game headers
#include "network/Objects/entities/NetObjPickup.h"
#include "Objects/Object.h"
#include "Pickups/Data/PickupIds.h"

class CPickupData;
class CPickupPlacement;

namespace rage
{
	class phArchetypeDamp;
};

#define ENABLE_GLOWS 0

//////////////////////////////////////////////////////////////////////////
// CPickup
//////////////////////////////////////////////////////////////////////////

class CPickup : public CObject
{
	DECLARE_RTTI_DERIVED_CLASS(CPickup, CObject);

	friend class CNetObjPickup;
	friend class CPickupPlacement;
	friend class CPickupRewardAmmo;
	friend class CPickupManager;
	friend class CNetworkObjectMgr;

public:

	struct ExtendedProbeAreas
	{
		ExtendedProbeAreas() : m_Position(0.0f, 0.0f, 0.0f), m_RadiusSqrd(0.0f)
		{}
		ExtendedProbeAreas(Vector3 pos, float radius) : m_Position(pos), m_RadiusSqrd(radius*radius)
		{}

		void Reset()
		{
			m_RadiusSqrd = 0.0f;
			m_Position = Vector3(0.0f, 0.0f, 0.0f);
		}

		float m_RadiusSqrd;
		Vector3 m_Position;
	};

	// the amount of time in millisecs an ambient pickup exists for before being removed
	static const u32 AMBIENT_PICKUP_LIFETIME = 180000; // 3 minutes

	// the length of time an ambient pickup fades out before being removed at the end of its lifetime
	static const u32 PICKUP_FADEOUT_TIME = 2000;

	// The distance from the player at which pickups will be removed when out of scope
	static const s32 PICKUP_DEFAULT_SCOPE_RANGE = 90;

	// The distance from the player at which pickups will be removed when out of scope
	static const s32 PICKUP_EXTENDED_SCOPE_RANGE = 300;

	// the world limits for portable pickups, they are not allowed to be dropped outwith these
	static const Vector2 PORTABLE_PICKUP_WORLD_LIMITS_MIN;
	static const Vector2 PORTABLE_PICKUP_WORLD_LIMITS_MAX;

	// The alpha of the pickups when transparent
	static u32 PICKUP_ALPHA_WHEN_TRANSPARENT;

public:
	
	enum PickupFlags
	{
		// SYNCED SCRIPT FLAGS:
		PF_Portable	= 0,						// set if the pickup can be carried by a ped
		PF_TeamPermitsSet,						// team permits have been set
		PF_PlaceOnGround,						// force pickup onto the ground when created/detached
		PF_OrientToGround,						// orientate to ground when placed on ground 
		PF_UprightOnGround,						// place upright on ground when placed on ground 
		PF_LyingOnFixedObject,					// the pickup may be lying on a fixed object - if so include objects in the probes when placing on ground 
		PF_CollectionProhibited,				// used to prevent an individual pickup from being collected on all machines
		PF_OffsetGlow,							// offset glow position by half radius of model's bound.
		PF_DroppedInWater,						// lets us decide whether to force the pickup to be collideable or to be fixed
		PF_DroppedByPed,						// set when the pickup was dropped by a dead non-player ped
		PF_DebugCreated,						// this pickup was created for debug purposes
		PF_GlowInSameTeam,						// even if PF_TeamPermitsSet is set, still glow
		PF_AddArrowMarker,						// if set this pickup has an arrow marker above it
		PF_TransparentWhenUncollectable,		// if set this pickup is transparent when uncollectable by script
		PF_KeepPortablePickupAtDropoff,			
		PF_UnderwaterPickup,					// Set when the pickup was created underwater (for underwater missions), and therefore must subsequently be dropped underwater
		PF_ForceActivatePhysicsOnPickup,		// Forces portable pickup to turn on physics when dropped
		PF_ForceActivatePhysicsOnDropNearSubmarine, // Forces portable pickup to turn on physics when dropped if near a submarine
		PF_SuppressPickupAudio,					// if set on a pickup, we won't play a generic pickup sound when the pickup is collected (useful if eg. script wish to do something custom)
		PF_AllowCollectionInVehicle,			// if set, the pickup can be collected when in a vehicle(if collectable on foot)
		PF_ForceWaitForFullySyncedBeforeDestruction,
		PF_GlowOnProhibitCollection,
		PF_AllowArrowMarkerWhenUncollectable,	// Allows the AddArrowMarker flag to work when the pickup in uncollectable
		NUM_SYNCED_PICKUP_FLAGS,	

		// NON-SYNCED FLAGS:
		PF_Collected,							// set when the pickup is collected
		PF_HasCustomModel,						// set when the pickup has a custom model 
		PF_GotCustomArchetype,					// set when the pickup is given it's custom archetype
		PF_Initialised,							// set when the pickup has got its archetype and placed in the world properly
		PF_Destroyed,							// set when the pickup is destroyed. Used to sanity check that the pickup is being removed properly.
		PF_Inaccessible,						// set when a portable pickup may be in an inaccessible location
		PF_WarpToAccessibleLocation,			// set when a portable pickup has to be warped to an accessible location
		PF_SearchingForAccessibleLocation,		// set when we are waiting for the network respawn manager to find us an accessible location
		PF_CreateDefaultWeaponAttachments,		// create default weapon attachments
		PF_PlacedOnGround,						// set once the pickup has been placed on the ground
		PF_LocalPlayerCollision,				// set when ProcessPreComputeImpacts is called and the local player is colliding with the pickup
		PF_HelpTextDisplayed,					// set once the pickup has come into collectable range with the local player and the collection help text has been displayed 
		PF_LocalCollectionProhibited,			// prohibits the pickup being collected locally
		PF_DetachWhenLocal,						// flag the pickup to be detached when it becomes local
		PF_SleepThresholdsInitialized,			// special sleep thresholds for pickups
		PF_HideInPhotos,						// if set the pickup is not rendered when using the camera phone
		PF_DontGlow, 							// if set the pickup will not be glowing.
		PF_DontFadeOut, 						// if set the pickup will not fade out and be removed after a certain amount of time
		PF_DroppedFromLocalPlayer, 				// if set the pickup will not allow itself to be collected by the local player until he moves away and back again
		PF_DroppedInInterior,					// if set a portable pickup was dropped in an interior
		PF_DisabledCollision, 					// if set this pickup had its collision disabled when hidden
		PF_LastAccessiblePosHasValidGround, 	// set when a portable pickup being carried has last accessible location which is a valid navmesh location
		PF_PlayerGift,							// if set this pickup was dropped by a player for another player to collect
		PF_HideWhenDetached,					// if set this portable pickup is hidden when detached
		PF_UseExtendedScope, 					// if set this ambient pickup uses an extended scope range. Used for script created ambient pickups.
		PF_CollisionsWithPedDisabled, 			// set on the pickup when dropped from a dead ped, to prevent it subsequently colliding with it
		PF_CollisionsWithPedVehicleDisabled, 	// if set this pickup was dropped by a dead ped in a vehicle, and collisions disabled with that vehicle
		PF_HiddenByScript,						// if set this pickup has been forcibly hidden by script
		PF_LyingOnUnFixedObject,				// if set this portable pickup is attached to a ped standing or climbing on a non-fixed physical object
		PF_AutoEquipOnCollect,					// if set on a weapon pickup, it will auto equip the picked up weapon. It will ignore autoswap logic		
		PF_RequiresLineOfSight,					// if set on a pickup, the pickup can only be collected if we have line of sight to it
	};

	static const u64 SYNCED_PICKUP_FLAGS_MASK = (1<<NUM_SYNCED_PICKUP_FLAGS)-1;

	// different collection types:
	enum eCollectionType
	{
		COLLECT_INVALID, 
		COLLECT_ONFOOT,
		COLLECT_INCAR,
		COLLECT_ONSHOT,
		COLLECT_REMOTE,
		COLLECT_TEAM_TRANSFER,
		COLLECT_CLONE,
		COLLECT_NUM_TYPES
	};

	// CanCollect() failure reasons
	enum
	{
		PCC_NONE,
		PCC_COLLECTABLE_ON_FOOT,				// the pickup is only collectable on foot and the ped trying to collect it is in a vehicle
		PCC_COLLECTABLE_IN_VEHICLE,				// the pickup is only collectable in a vehicle and the ped trying to collect it is on foot
		PCC_NOT_DRIVER,							// the ped trying to collect the pickup is not driving his vehicle
		PCC_WRONG_VEHICLE,						// the ped trying to collect the pickup is in the wrong type of vehicle
		PCC_ALREADY_COLLECTED,					// the pickup is already flagged as collected
		PCC_PED_ENTERING_VEHICLE,				// the ped trying to collect is entering a vehicle
		PCC_FADED_OUT_AMBIENT,					// the pickup is an ambient pickup that has faded out and is about to be removed
		PCC_PED_IS_DEAD,						// the ped trying to collect is dead or dying
		PCC_PENDING_COLLECTION,					// the pickup is already pending collection
		PCC_HIDDEN,								// the pickup is hidden
		PCC_WRONG_TEAM,							// the player trying to collect is in a team that is blocked from collecting the pickup
		PCC_PORTABLE_PICKUP_PENDING,			// the player trying to collect is already pending collection of another portable pickup
		PCC_PORTABLE_PICKUPS_BLOCKED,			// the player trying to collect has been prevented from collecting portable pickups by the scripts
		PCC_MAX_PORTABLE_PICKUPS,				// the player trying to collect has already collected the maximum amount of portable pickups allowed by the script
		PCC_NON_PARTICIPANT,					// the player trying to collect is not a script participant
		PCC_DETACHED_FROM_SCRIPT,				// the pickup is not registered with a script anymore
		PCC_COLLECTION_INSTANCE_PROHIBITED,		// this individual pickup has been set to be uncollectable by the script
		PCC_COLLECTION_TYPE_PROHIBITED,			// the player trying to collect has been blocked from collecting these types of pickup by the script
		PCC_PICKUP_MODEL_TYPE_PROHIBITED,		// the scripts have blocked the local player from collecting pickups with this custom model type
		PCC_ATTACHED,							// the pickup is attached
		PCC_WEAPON_SWITCHING_BLOCKED,			// the ped is temporarily blocking weapon switching
		PCC_BUTTON_NOT_PRESSED,					// the pickup requires a button press to pickup and the button is not pressed
		PCC_REWARDS_CANT_BE_GIVEN,				// none of the pickup rewards cannot be given to the ped
		PCC_CLONE_REQUEST_SENT,					// the pickup is a clone and a collection request has been sent
		PCC_MAP_REQUEST_SENT,					// the pickup is a map pickup and a collection request has been sent
		PCC_DOING_STEALTH_KILL,					// the ped is doing a stealth kill / takedown
		PCC_IN_SYNCED_SCENE,					// the pickup is part of a synchronised scene
		PCC_ON_THE_PHONE,						// the ped is on the phone. 
		PCC_DROPPED_FROM_LOCAL_PLAYER,			// the pickup has just been dropped from the local player 
		PCC_NOT_INITIALISED,					// the pickup has not finished initialising itself
		PCC_DIFFERENT_INTERIORS,				// the pickup is in a different interior than the collecting ped
		PCC_DESTROYED,							// the pickup is destroyed (has 0 health)
		PCC_INVALID_PED_MODEL,					// the ped model is not setup to use pickups (it's probably an animal)
		PCC_PENDING_COLLECTIONS_EXCEEDED,		// the max number of pending collections has been exceeeded
		PCC_NOT_ALLOWED_FOR_THIS_PLAYER,		// script set un collectable by this player
		PCC_FAR_FOR_ON_FOOT_COLLECTION,			// pickup types: PICKUP_PORTABLE_CRATE_UNFIXED_INAIRVEHICLE_WITH_PASSENGERS can't be collected on foot from far
		PCC_NUM_FAILURE_REASONS
	};

#if __BANK && ENABLE_GLOWS
	// custom glow tweakable through rag widgets, applied to all pickups if set
	static float ms_customGlowR;
	static float ms_customGlowG;
	static float ms_customGlowB;
	static float ms_customGlowI;
	static float ms_customGlowRange;
	static float ms_customGlowFadeDist;
	static float ms_customGlowOffset;
#endif

public:

	CPickup(const eEntityOwnedBy ownedBy, u32 hash, u32 customModelIndex = fwModelId::MI_INVALID, bool bCreateDefaultWeaponAttachments = true);
	virtual ~CPickup();

	FW_REGISTER_CLASS_POOL(CPickup);

	u32						GetPickupHash() const							{ return m_pickupHash; }
	const CPickupData*		GetPickupData() const							{ return m_pPickupData; }
	CPickupPlacement*		GetPlacement() const							{ return m_pPlacement; }
	void					SetAmount(u32 amount)    						{ m_amount = amount; }
	u32						GetAmount() const								{ return m_amount; }
	u32						GetSyncedFlags() const							{ return (u32)(m_flags.GetAllFlags() & SYNCED_PICKUP_FLAGS_MASK); }
	bool					IsFlagSet(PickupFlags flag) const				{ return m_flags.IsFlagSet(static_cast<u64>(1)<<flag);}
	bool					GetIsPendingCollection() const					{ return m_pendingCollectionType != COLLECT_INVALID; }
	CPed*					GetPendingCarrier() const						{ return m_pendingCarrier; }
	const Vector3&			GetLastAccessibleLocation() const				{ return m_lastAccessibleLocation; }
	void					ForceSetLastAccessibleLocation();
	void					SetDebugCreated()								{ SetFlag(PF_DebugCreated); }
	u32						GetLifeTime() const								{ return m_lifeTime; }
	void					SetLifeTime(u32 lt)								{ m_lifeTime = lt; }
	float					GetGlowOffset() const							{ return m_glowOffset; }
	void					SetGlowOffset(float f)							{ m_glowOffset = f; }
	void					SetGlowWhenInSameTeam()							{ SetFlag(CPickup::PF_GlowInSameTeam); }
	bool					GetGlowWhenInSameTeam()	const					{ return IsFlagSet(CPickup::PF_GlowInSameTeam); }
	void					SetWeaponHash(int weaponHash)					{ 
																				m_customWeaponHash = weaponHash;
																				SetFlag(PF_CreateDefaultWeaponAttachments);
																			}
	int						GetWeaponHash()									{ return m_customWeaponHash; }
	void					SetAmountAndAmountCollected(u32 amount, u32 amountCollected);
	u32						GetAmountCollected() const						{ return m_amountCollected; }
	
	void					SetSyncedFlags(u32 flags);
	TeamFlags				GetTeamPermits() const							{ return m_teamPermits; }
	void					SetTeamPermits(TeamFlags teams)					{ m_teamPermits = teams; SetFlag(PF_TeamPermitsSet); }
	void					AddTeamPermit(u8 team)							{ m_teamPermits |= (1<<team); SetFlag(PF_TeamPermitsSet); }
	void					RemoveTeamPermit(u8 team)						{ m_teamPermits &= ~(1<<team); SetFlag(PF_TeamPermitsSet); }
	bool					GetProhibitCollection(bool local)				{ if(local) return IsFlagSet(PF_LocalCollectionProhibited); else return IsFlagSet(PF_CollectionProhibited); }
	void					SetProhibitCollection(bool prevent)				{ if (prevent) SetFlag(PF_CollectionProhibited); else ClearFlag(PF_CollectionProhibited); }
	void					SetProhibitLocalCollection(bool prevent)		{ if (prevent) SetFlag(PF_LocalCollectionProhibited); else ClearFlag(PF_LocalCollectionProhibited); }
	void					SetOffsetGlow(bool b)							{ ChangeFlag(PF_OffsetGlow, b) ; }
	bool					GetOffsetGlow() const							{ return IsFlagSet(PF_OffsetGlow); }
	void					SetArrowMarker(bool b)							{ ChangeFlag(PF_AddArrowMarker, b) ; }
	void					SetDontFadeOut()								{ SetFlag(PF_DontFadeOut); }
	void					SetDroppedFromLocalPlayer()						{ SetFlag(PF_DroppedFromLocalPlayer); }
	void					SetDroppedByPed()								{ SetFlag(PF_DroppedByPed); }
	void					SetPlayerGift()									{ SetFlag(PF_PlayerGift); }
	void					SetLyingOnFixedObject()							{ SetFlag(PF_LyingOnFixedObject); }
	void					SetSuppressPickupAudio(bool suppress)			{ if (suppress) SetFlag(PF_SuppressPickupAudio); else ClearFlag(PF_SuppressPickupAudio); }
	bool					ShouldSuppressPickupAudio() const				{ return IsFlagSet(PF_SuppressPickupAudio); }
	bool					IsPlayerGift() const							{ return IsFlagSet(PF_PlayerGift); }
	void					SetDontGlow(bool bDontGlow)						{ ChangeFlag(PF_DontGlow, bDontGlow); }
	void					SetGlowOnProhibitCollection(bool bGlow)			{ ChangeFlag(PF_GlowOnProhibitCollection, bGlow); }
	void					SetTransparentWhenUncollectable(bool bSet)		{ ChangeFlag(PF_TransparentWhenUncollectable, bSet); }
	void					SetHideWhenDetached(bool b);
	void					SetAllowArrowMarkerWhenUncollectable(bool b)	{ ChangeFlag(PF_AllowArrowMarkerWhenUncollectable, b); }

	void					SetPortablePickupPersist(bool enable)			
	{ 
		if(enable)
			SetFlag(PF_KeepPortablePickupAtDropoff);
		else
			ClearFlag(PF_KeepPortablePickupAtDropoff);
	}

	void					SetAllowCollectionInVehicle()					{ SetFlag(PF_AllowCollectionInVehicle); }
	bool					GetAllowCollectionInVehicle() const;

	void					SetForceWaitForFullySyncBeforeDestruction(bool force) { if(force) SetFlag(PF_ForceWaitForFullySyncedBeforeDestruction); else ClearFlag(PF_ForceWaitForFullySyncedBeforeDestruction); }
	bool					GetForceWaitForFullySyncBeforeDestruction()			 { return IsFlagSet(PF_ForceWaitForFullySyncedBeforeDestruction); }

	void					SetPlacement(CPickupPlacement* pData);
	void					SetLastOwner(CPed* pPed);
	void					SetCollected();
	void					SetPortable();

	void					SetDroppedInWater()								{ SetFlag(PF_DroppedInWater); }
	void					ClearDroppedInWater();
	bool					WasDroppedInWater() const						{ return IsFlagSet(PF_DroppedInWater); }

	void					SetClonePortablePickupData(bool bInaccessible, const Vector3& lastAccessibleLoc, bool bLastAccessiblePosHasValidGround);

	void					Init();
	void					Update();

	bool					HasGlow() const;
	bool					HasLifetime() const;

	virtual void			OnActivate(phInst* pInst, phInst* pOtherInst);

	virtual bool			CreateDrawable();
	virtual bool			 RequiresProcessControl() const;
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void			 PreRender2(const bool bIsVisibleInMainViewport = true);
	virtual void			 Teleport(const Vector3& vecSetCoors, float fSetHeading=-10.0f, bool bCalledByPedTask=false, bool bTriggerPortalRescan = true, bool bCalledByPedTask2=false, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);
	virtual void			 SetFixedPhysics(bool bFixed, bool bNetwork = false);
	virtual u32				 GetDefaultInstanceIncludeFlags(phInst* pInst=NULL) const;

#if __BANK
	virtual void			 DebugAttachToPhysicalBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone = -1);
#else
	virtual void			 AttachToPhysicalBasic(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone = -1);
#endif
	virtual void			 DetachFromParent(u16 nDetachFlags);	

	virtual bool			 ShouldAvoidCollision(const CPhysical*) { return false; } 
	virtual bool			 TestNoCollision(const phInst *pOtherInst);

	virtual bool			 CanLeaveInteriorRetainList() const		{ return !GetRequiresACustomArchetype() || IsFlagSet(PF_GotCustomArchetype); }

#if ENABLE_NETWORK_LOGGING
	// Called when ShouldFindImpacts() is called on the pickup's physics inst
	void ShouldFindImpactsCalled();
	void ShouldFindImpactsFailed(const char* reason);
	void ShouldFindImpactsSuccess();
#endif // ENABLE_NETWORK_LOGGING

	// called when an entity collides with this pickup
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

		// processes a gunshot, etc. Returns true if the impact is to be ignored.
	bool ProcessWeaponImpact(CEntity* pParentEntity);

	// Pickups out of scope are too far away from the player and have to be removed
	bool GetInScope(const CPed& playerPed) const;

	// Returns true if the pickup can be collected 
	bool CanCollect(const CPed* pPed, eCollectionType collectionType, unsigned *failureCode = 0);

	// Called when the pickup is collected on foot by the given ped
	void CollectOnFoot(CPed* pPed);

	// Called when the pickup is collected in a vehicle by the given ped
	void CollectInCar(CPed* pPed);

	// Called when the pickup is collected when shot by the given ped
	void CollectOnShot(CPed* pPed);

	// Gives the goodies in the pickup to the given ped
	void GiveRewards(CPed* pPed);

	// Gives the goodies in the pickup to the passengers in the vehicle of the given ped
	void GiveRewardsToPassengers(CPed* pPed);

	// Called when the pickup is collected by any method (on foot, in vehicle, on shot)
	void Collect(CPed* pPed);

	// Called when a remote machine is trying to collect the pickup
	void SetRemotePendingCollection(CPed& ped);

	// called from a network event when we get a reply for a collection request from the machine which controls the pickup
	void ProcessCollectionResponse(const netPlayer* fromPlayer, bool bCollected);

	void SetStartingLinearVelocity(const Vector3 &linVel) { m_StartingLinearVelocity = linVel; }
	void SetStartingAngularVelocity(const Vector3 &linVel) { m_StartingAngularVelocity = linVel; }

	// attaches a portable pickup to a ped
	void AttachPortablePickupToPed(CPed* pPed, const char* reason);
	void DetachPortablePickupFromPed(const char* reason, bool bPlaceOnGround = true);

	void OnPedAttachment(CPed* pPed);
	void OnPedDetachment(CPed* pPed);

	virtual bool PlaceOnGroundProperly(float fMaxRange = 10.0f, bool bAlign = true, float heightOffGround = 0.0f, bool bIncludeWater = false, bool bUpright = false, bool *pInWater = NULL, bool bIncludeObjects = false, bool useExtendedProbe = false);

	void MoveToAccessibleLocation(bool bUseLastLocation = false);

	bool IsCustomColliderReady() const { if (GetRequiresACustomArchetype()) return IsFlagSet(PF_GotCustomArchetype); else return true; }
	virtual bool GetIsReadyForInsertion(void) { 	if (GetRequiresACustomArchetype() && !IsFlagSet(PF_GotCustomArchetype)) return false; else return true; }

	void SetPlaceOnGround(bool bOrient = true, bool bUpright = true)
	{ 
		SetFlag(PF_PlaceOnGround); 
		ClearFlag(PF_PlacedOnGround);

		if (bOrient) SetFlag(PF_OrientToGround); 
		if (bUpright) SetFlag(PF_UprightOnGround); 
	}

	void ClearPlaceOnGround()
	{
		ClearFlag(PF_PlaceOnGround); 
	}

	// destroys the pickup, but leaves any corresponding placement
	void Destroy();
	void AddExplosion(CEntity* explosionOwner=NULL);
	bool CanBeDamagedByFire(CFire* pFire);
	bool CanPhysicalBeDamaged(const CEntity* pInflictor, u32 nWeaponUsedHash, bool bDoNetworkCloneCheck = true, bool bDisableDamagingOwner = false NOTFINAL_ONLY(, u32* uRejectionReason = NULL)) const;
	void SetIncludeProjectileFlag( bool includeProjectiles );
	bool GetIncludeProjectileFlag() { return m_includeProjectiles; }

	static void SetVehicleWeaponsSharedAmongstPassengers(bool b) { ms_shareVehicleWeaponPickupsAmongstPassengers = b; }
	static bool GetVehicleWeaponsSharedAmongstPassengers()		 { return ms_shareVehicleWeaponPickupsAmongstPassengers; }

#if ENABLE_NETWORK_LOGGING
	static const char *GetCanCollectFailureString(unsigned failureCode);
#endif	

	static u32 GetNumPickupsPendingCollection() { return ms_numPickupsPendingLocalCollection; }
	static CPickup* GetPickupPendingCollection(u32 i) { if (AssertVerify(i<MAX_PENDING_COLLECTIONS)) return ms_pickupsPendingLocalCollection[i]; return NULL; }

public:
	// Sets rendering flags for the pick up.
	void SetForceAlphaAndUseAmbientScale();
	void SetUseLightOverrideForGlow();
	void ClearUseLightOverrideForGlow();
	void SetForceActivatePhysicsOnUnfixedPickup(bool force) { force ? SetFlag(PF_ForceActivatePhysicsOnPickup) : ClearFlag(PF_ForceActivatePhysicsOnPickup); }

	void SetForceActivatePhysicsOnDropNearSubmarine(bool force) { force ? SetFlag(PF_ForceActivatePhysicsOnDropNearSubmarine) : ClearFlag(PF_ForceActivatePhysicsOnDropNearSubmarine); }
	bool GetForceActivatePhysicsOnDropNearSubmarine() const { return IsFlagSet(PF_ForceActivatePhysicsOnDropNearSubmarine); }

	inline void SetAllowNonScriptParticipantCollect(bool allow)	{ m_allowNonScriptParticipantCollection = allow; }
	inline bool GetAllowNonScriptParticipantCollect()			{ return m_allowNonScriptParticipantCollection; }

	float GetHeightOffGround() const { return m_heightOffGround; }
private:
	static void SetForceAlphaAndUseAmbientScaleTraverseHierarchy(CEntity *pEntity, bool bRenderDeferred);
	static void SetUseLightOverrideTraverseHierarchy(CEntity *pEntity, bool bValue);

	bool IsPickupDroppedNearSubmarine(Vector3 pickupPos);
public:
	// Returns the pick if the passed object is a child of.
	static CPickup *GetParentPickUp(CObject *pObject);

protected:

    void SetFlag(PickupFlags flag)				{ m_flags.SetFlag(static_cast<u64>(1)<<flag); }
    void ChangeFlag(PickupFlags flag, bool b)	{ m_flags.ChangeFlag(static_cast<u64>(1)<<flag, b); }
    void ClearFlag(PickupFlags flag)			{ m_flags.ClearFlag(static_cast<u64>(1)<<flag); }


	netLoggingInterface* GetNetworkLog() const;

private:

	// critical collection checks, used for both local and remote players
	bool CanCollectCritical(const CPed* pPed, eCollectionType collectionType, unsigned *failureCode);

	// script collection checks only
	bool CanCollectScript(const CPed* pPed, unsigned *failureCode = 0) const;

	// tries to assign the custom archetype
	void AssignCustomArchetype();

	// tries to move the pickup into its designated interior
	void MoveIntoInterior();

	// handles fading out and removal of ambient pickups
	void HandleFade();

	// returns true if the pickup is always fixed and has no physics
	bool IsAlwaysFixed() const;

	// returns true if the pickup never collides with other entities
	bool IsCollideable() const;

	// If true, then we add an additional sphere bound to the bounds for this pickup, to detect collection collisions
	bool GetRequiresACustomArchetype()	const;

	// creates a custom physics archetype, containing an additional sphere bound used to detect collection
	phArchetypeDamp* CreateCustomArchetype();

	// scales the pickup model
	void SetPickupScale();

	// updates any portable pickup related stuff
	void UpdatePortablePickup();

	// updates m_lastAccessibleLocation for portable pickups
	void UpdateLastAccessibleLocation(CPed& attachPed, bool bForceValid = false);

	// places the pickup on the ground
	bool PlaceOnGround();

	// finds a suitable location to drop a portable pickup, close to the given position and not intersecting other pickups or entities
	void FindSuitablePortablePickupDropLocation(Vector3& initialPos);

	// portable pickups are normally fixed, this will unfix 
	void ActivatePhysicsOnPortablePickup();

	// returns true if a portable pickup is considered to be in water when attached to the given ped
	bool IsInWater(CPed& ped) const;

	// drops a portable pickup in water
	void DropInWater(bool bFindWaterLevel = true);

	void SetLocalPendingCollection(CPed& pendingCollector, eCollectionType collectionType);

	void ClearPendingCollection(bool bExpose = true);

	// hides the pickup (makes it uncollideable and invisible)
	void Hide(bool bMakeUncollideable = false);

	// exposes the pickup again, after Hide() has been called
	void Expose();

	// forces the network object to update all important state when a portable pickup is dropped
	void ForceUpdateOfDroppedPortablePickupData();

	// Checks if pickup is networked local has been dropped from plane and sets the matrix to identity to ensure it isn't left with plane orientation 
	// and checks for best position with respect to nearness to buildings and ground and reposition the pickup if necessary
	void NetworkSetPositionOfPlaneDroppedPortablePickup(const CPed* pPed, const CVehicle* pVehicle);

	// returns true if the pickup is underwater
	bool IsUnderWater() const;

	bool IsPositionInsidePrologueArea(Vector3& pos);
	bool IsPositionInsideBlockedBuildingArea(Vector3 pos);

#if ENABLE_NETWORK_LOGGING
	enum HasGlowFailReason
	{
		None,
		PickupData,
		DoNotGlowFlag,
		CanNotCollectScript,
		NotParticipant,
		NoScriptExtension,
		NotForTeam,
		ProhibitCollection,
	};

	const char* GetCannotGlowReason(HasGlowFailReason reason) const;
	
	mutable HasGlowFailReason m_LastHasGlowFailReason;
#endif // ENABLE_NETWORK_LOGGING

	// used by portable pickups: this is the last accessible location on the map that the ped carrying a pickup was at. This is used when the ped dies and
	// drops a pickup in a location that other players cannot reach. In this case it is warped to the last accessible position
	Vector3 m_lastAccessibleLocation;

	Vector3 m_StartingLinearVelocity;
	Vector3 m_StartingAngularVelocity;

	// a ptr to the ped who dropped this pickup, if it was dropped
	RegdPed m_lastOwner;

	// The hash of the pickup
	u32 m_pickupHash;

	// Pointer to the data
	const CPickupData* m_pPickupData;

	// a ptr to the placement data that generated this pickup
	CPickupPlacement* m_pPlacement;

	// a variable amount used by some pickup types (eg money). This is used to get around the hard coded values specified in the Rave data. 
	u32 m_amount; 

	// amount collected by player after pickup.
	u32 m_amountCollected; 

	// pickup flags
	fwFlags<u64> m_flags;

	// set when a pickup is trying to be collected but has to wait on approval from another machine (which owns the pickup)
	eCollectionType m_pendingCollectionType;

	// the ped trying to collect the pickup
	RegdPed m_pendingCollector;

	// the ped trying to carry the pickup
	RegdPed m_pendingCarrier;

	// the height the pickup needs to be placed above the ground to lie on it properly. Grabbed before the custom archetype is set up
	float m_heightOffGround;

	// the amount of time the pickup has existed (only used by pickups with no placements)
	u32 m_lifeTime;

	float m_glowOffset;

	u32 m_customWeaponHash;

	// only used by pickups collectable by boat
	float m_waterLevelNoWaves;
	
	// flags indicating which teams are allowed to collect this pickup
	TeamFlags m_teamPermits;

	// a timer used when the pickup is pending remote collection
	s16 m_pendingCollectionTimer;

	// the bound radius before adding the sphere bound
	float m_originalBoundRadius;
	
	// weapon streaming helper
	CWeaponItem* m_pWeaponItemForStreaming;

	bool m_includeProjectiles;

	bool m_allowNonScriptParticipantCollection;

	// a bool set by script which specifies whether to give passengers in your vehicle any weapons you collect in the vehicle
	static bool ms_shareVehicleWeaponPickupsAmongstPassengers;

	// a record of all pickups pending local collection
	static const unsigned MAX_PENDING_COLLECTIONS = 10;
	static CPickup* ms_pickupsPendingLocalCollection[MAX_PENDING_COLLECTIONS];
	static u32 ms_numPickupsPendingLocalCollection;

	// Allow updating last accessible location on yachts
	static const float ACCESSIBLE_DISTANCE_FROM_YACHT;
	static const int SIZE_OF_ALL_YACHT_LOCATIONS = 36;
	static const Vector3 ALL_YACHT_LOCATIONS[SIZE_OF_ALL_YACHT_LOCATIONS];

	static unsigned m_ExtendedProbeCount;
	static const int MAX_NUM_EXTENDED_PROBE_AREAS = 10;
	static atRangeArray<ExtendedProbeAreas, MAX_NUM_EXTENDED_PROBE_AREAS> m_AllExtendedProbeAreas;

#if __ASSERT
	static const int MAX_CALL_TIMES = 3;
	atFixedArray<u32, MAX_CALL_TIMES> m_ResetAccessiblePositionCallTimes;
#endif // __ASSERT

public:
	static bool AddExtendedProbeArea(Vector3 pos, float radius);
	static void ClearExtendedProbeAreas();

	// network hooks
	NETWORK_OBJECT_TYPE_DECL( CNetObjPickup, NET_OBJ_TYPE_PICKUP );
};

#if __DEV
// Forward declare pool full callback so we don't get two versions of it
namespace rage { template<> void fwPool<CPickup>::PoolFullCallback(); }
#endif // __DEV

#endif // PICKUP_H
