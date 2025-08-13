//
// weapons/weapon.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_H
#define WEAPON_H

// Rage headers
#include "fwanimation/boneids.h"

// Game headers
#include "Audio/WeaponAudioEntity.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/DynamicEntity.h"
#include "Task/system/TaskHelpers.h"
#include "Weapons/Components/WeaponComponentClip.h"
#include "Weapons/Components/WeaponComponentFlashLight.h"
#include "Weapons/Components/WeaponComponentProgrammableTargeting.h"
#include "Weapons/Components/WeaponComponentSuppressor.h"
#include "Weapons/Components/WeaponComponentScope.h"
#include "Weapons/Components/WeaponComponentVariantModel.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/WeaponChannel.h"
#include "Weapons/WeaponEnums.h"
#include "Weapons/WeaponObserver.h"
#include "Weapons/WeaponDamage.h"

// Forward declarations
class CWeaponComponentLaserSight;
namespace rage 
{
	class fwCustomShaderEffect;
}

const u8 INVALID_FAKE_SEQUENCE_ID = (u8)~0;
////////////////////////////////////////////////////////////////////////////////

class CWeapon : public fwRefAwareBase
{
	friend class CPacketWeaponMuzzleFlashFx;
public:
	FW_REGISTER_CLASS_POOL(CWeapon);

	// Static constants
	static const s32 INFINITE_AMMO = -1;
	static const s32 LOTS_OF_AMMO = 25000;
	static const u32 PROP_END_OF_LIFE_TIMEOUT_IN_MS = 60000;
	static const f32 STANDARD_WEAPON_ACCURACY_RANGE;
	static const float sf_OffscreenRandomPerceptionOverrideRange;

	// Static functions
	static bool RefillPedsWeaponAmmoInstant(CPed& rPed);

	// Weapon state
	enum State
	{
		STATE_READY = 0,
		STATE_WAITING_TO_FIRE,
		STATE_RELOADING,
		STATE_OUT_OF_AMMO,
		STATE_SPINNING_UP,
		STATE_SPINNING_DOWN,
	};

	// Fire flags
	enum FireFlags
	{
		FF_FireBlank								= BIT0,
		FF_SetPerfectAccuracy						= BIT1,
		FF_AllowDamageToVehicle						= BIT2,
		FF_AllowDamageToVehicleOccupants			= BIT3,
		FF_ForceBulletTrace							= BIT4,
		FF_ForceNoBulletTrace						= BIT5,
		FF_BlindFire								= BIT6,
		FF_DropProjectile							= BIT7,
		FF_DisableProjectileTrail					= BIT8,
		FF_PassThroughOwnVehicleBulletProofGlass	= BIT9,
		
		// Bit sets
		DEFAULT_FIRE_FLAGS					= 0,
	};

	enum eWeaponLodState 
	{
		WLS_HD_NA			= 0, //	- cannot go into HD at all
		WLS_HD_NONE			= 1, //	- no HD resources currently Loaded
		WLS_HD_REQUESTED	= 2, // - HD resource requests are in flight
		WLS_HD_AVAILABLE	= 3, // - HD resources available & can be used
		WLS_HD_REMOVING		= 4	 // - HD not available & scheduled for removal
	};

	// Construction
	CWeapon(u32 uWeaponHash, s32 iAmmoTotal = INFINITE_AMMO, CDynamicEntity* pDrawable = NULL, bool isScriptWeapon = false, u8 tintIndex = 0);

	// Destruction
	virtual ~CWeapon();

	// Cloud tunables
	static void InitTunables(); 

	// Fire parameters
	struct sFireParams
	{
		sFireParams(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3* pvStart = NULL, const Vector3* pvEnd = NULL)
			: pFiringEntity(pFiringEntity)
			, pTargetEntity(NULL)
			, pIgnoreDamageEntity(NULL)
			, pIgnoreCollisionEntity(NULL)
			, weaponMatrix(weaponMatrix)
			, iVehicleWeaponBoneIndex(-1)
			, iVehicleWeaponIndex(0)
			, pvStart(pvStart)
			, pvEnd(pvEnd)
			, iFireFlags(DEFAULT_FIRE_FLAGS)
			, pObjOverride(NULL)
			, fApplyDamage(-1.0f)
			, fDesiredTargetDistance(-1.0f)
			, fOverrideLifeTime(-1.0f)
			, fFireAnimRate(1.0f)
			, fInitialVelocity(-1.0f)
			, bScriptControlled(false)
			, bCommandFireSingleBullet(false)
			, bAllowRumble(true)
			, bCreateNewProjectileObject(false)
			, bDisablePlayerCoverStartAdjustment(false)
			, bProjectileCreatedFromGrenadeThrow(false)
			, bProjectileCreatedByScriptWithNoOwner(false)
			, bFiredFromAirDefence(false)
			, bIgnoreDamageEntityAttachParent(false)
			, bFreezeProjectileWaitingOnCollision(false)
			, bIgnoreCollisionResetNoBB(false)
			, fOverrideLaunchSpeed(-1.0f)
			, fakeStickyBombSequenceId(INVALID_FAKE_SEQUENCE_ID)
		{
		}

		CEntity* pFiringEntity;
		RegdConstEnt pTargetEntity;
		RegdEnt pIgnoreDamageEntity;
		RegdEnt pIgnoreCollisionEntity;
		const Matrix34& weaponMatrix;
		s32 iVehicleWeaponBoneIndex;
		s32 iVehicleWeaponIndex;
		const Vector3* pvStart;
		const Vector3* pvEnd;
		fwFlags32 iFireFlags;
		CObject* pObjOverride; 
		f32 fApplyDamage; 
		float fDesiredTargetDistance;
		float fOverrideLifeTime;
		float fFireAnimRate;
		float fInitialVelocity;
		u8	 fakeStickyBombSequenceId;
		bool bScriptControlled;
		bool bCommandFireSingleBullet;
		bool bAllowRumble;
		bool bCreateNewProjectileObject;
		bool bDisablePlayerCoverStartAdjustment;
		bool bProjectileCreatedFromGrenadeThrow;
		bool bProjectileCreatedByScriptWithNoOwner;
		bool bFiredFromAirDefence;
		bool bIgnoreDamageEntityAttachParent;
		bool bFreezeProjectileWaitingOnCollision;
		bool bIgnoreCollisionResetNoBB;
		float fOverrideLaunchSpeed;
	};

	// Fire the weapon
	virtual bool Fire(const sFireParams& params);

	// Perform update
	void Process(CEntity* pFiringEntity, u32 uTimeInMilliseconds);

	// Process after pre render
	void PostPreRender();

	// Get the current state
	State GetState() const;

	// Set the current state externally
	void SetState(State state);

	// Get the model entity
	const CEntity* GetEntity() const;

	// Get the muzzle entity
	const CEntity* GetMuzzleEntity() const;

	// Get the muzzle entity
	s32 GetMuzzleBoneIndex() const;

	void StoppedFiring();

	//
	// Weapon Info API
	//

	// Get the weapon descriptor
	const CWeaponInfo* GetWeaponInfo() const;

	// Get the weapon descriptor hash
	u32 GetWeaponHash() const;

	// Gets the loc text label for the weapon name.
	u32 GetHumanNameHash() const;

	// Get the weapon model info, if any
	const CWeaponModelInfo* GetWeaponModelInfo() const;

	// Get the damage type
	eDamageType GetDamageType() const;

	// Get the weapon range
	f32 GetRange() const;

	// Get the time before the next shot
	f32 GetTimeBeforeNextShot() const;

	u32 GetTimeOfNextShot() const;

	//
	// Firing API
	//

	// Calculate firing vector from a given target position
	bool CalcFireVecFromPos(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, const Vector3& vTarget) const;

	// Calculate firing vector from a given target entity, with optional offset and/or bone (use bone index so it works for anything with a skeleton)
	bool CalcFireVecFromEnt(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, CEntity* pTargetEnt, Vector3* pvOffset = NULL, s32 iBoneIndex = -1) const;

	// Calculate firing vector when player is free aiming - i.e. from weapon camera (be that 1st or 3rd person)
	bool CalcFireVecFromAimCamera(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, bool bSkipAimCheck = false) const;

	// Calculate firing vector along the barrel of the gun
	bool CalcFireVecFromGunOrientation(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd) const;

	// Calculate firing vector using weapon matrix 
	bool CalcFireVecFromWeaponMatrix(const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd) const;

	// Trigger any spin up required
	void SpinUp(CEntity* pFiringEntity);
	float ProcessWeaponSpin(const CEntity* pFiringEntity, s32 boneIndex = -1);
	bool GetNeedsToSpinDown() const { return m_bNeedsToSpinDown; }


	void GetMuzzlePosition(const Matrix34& weaponMatrix, Vector3& vBonePos) const;
	void GetMuzzleMatrix(const Matrix34& weaponMatrix, Matrix34& muzzleMatrix) const;

	void SetMuzzlePosition(const Vector3& muzzlePos);

	// Force the silenced state
	void SetIsSilenced(const bool bSilenced) { m_bSilenced = bSilenced; }

	//
	// Accuracy
	//

	// Get the combined accuracy of the weapon and the firing entity
	void GetAccuracy(const CPed* pFiringPed, float fDesiredTargetDist, sWeaponAccuracy& accuracy) const;

	//
	// Ammo API
	//

	// Set the total ammo
	void SetAmmoTotal(s32 iAmmoTotal);

	// Set the total ammo to infinite for this weapon
	void SetAmmoTotalInfinite();

	// Add to the total ammo
	void AddToAmmoTotal(s32 iAmmoAdd);

	// Get the total ammo
	s32 GetAmmoTotal() const;

	// Set the ammo in clip
	void SetAmmoInClip(s32 iAmmoInClip);

	// Get the ammo in clip
	s32 GetAmmoInClip() const;

	// Checks if we have a clip, and if so check that we have ammo in the clip.
	// This should replace lots of GetAmmoInClip() > 0 usages throughout code
	bool GetIsClipEmpty() const;

	// Get the max size of the clip
	s32 GetClipSize() const;

	// Get the number of bullets fired in a single shot
	u32 GetBulletsInBatch() const;

	// Can we reload?
	bool GetCanReload() const;

	// Do we need to reload?
	bool GetNeedsToReload(bool bHasLosToTarget) const;

	// Start reload
	void StartReload(CEntity* pFiringEntity);

	//Cancel reload
	void CancelReload();

	// Perform reload
	void DoReload(bool bInit = false);

	// Are we using infinite clips?
	bool GetHasInfiniteClips(const CEntity* pFiringEntity) const;

	//
	// Animation
	//

	// Start animation
	void StartAnim(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, f32 fBlendDuration = NORMAL_BLEND_DURATION, f32 fRate = 1.f, f32 fPhase = 0.f, bool bLoop = false, bool bPhaseSynch = false, bool bForceAnimUpdate = false);
	// Stop animation
	void StopAnim(f32 fBlendDuration = NORMAL_BLEND_DURATION);

	// Additional animations
	void ProcessGripAnim(CEntity* pFiringEntity);
	void ProcessIdleAnim(CEntity* pFiringEntity);
	
	CMoveNetworkHelper* GetMoveNetworkHelper() {return m_pMoveNetworkHelper;}

	//FPS mode MoVE network helper
	CMoveNetworkHelper* GetFPSMoveNetworkHelper() {return m_pFPSMoveNetworkHelper;}

	void CreateFPSMoveNetworkHelper();

	void CreateFPSWeaponNetworkPlayer();

	//
	// Observers
	//

	// Set the observer
	void SetObserver(CWeaponObserver* pObserver);

	const CPed* GetOwner() const;

	//
	// Network
	//

	void MarkAsTemporaryNetworkWeapon();
	bool GetIsTemporaryNetworkWeapon() const;

	// Send a network message
	void SendFireMessage(CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestResults& results, s32 iNumIntersections, bool bUseDamageOverride, float fDamageOverride, const u32 damageFlags, const u32 actionResultId = 0, const u16 meleeId = 0, u32 nForcedReactionDefinitionID = 0, CWeaponDamage::NetworkWeaponImpactInfo * const networkWeaponImpactInfo = NULL, const float fRecoilAccuracyWhenFired = 1.f, const float fFallOffRangeModifier = 1.f, const float fFallOffDamageModifier = 1.f, const Vector3* const forceDirection = 0);

	// Handle a network message
	static bool ReceiveFireMessage(CEntity* pFiringEntity, CEntity* pHitEntity, const u32 uWeaponHash, const f32 fDamage, const Vector3& vWorldHitPos, const s32 iComponent, const s32 iTyreIndex = -1, const s32 iSuspensionIndex = -1, const u32 damageFlags = 0, const u32 actionResultId = 0, const u16 meleeId = 0, const u32 nForcedReactionDefinitionID = 0, CWeaponDamage::NetworkWeaponImpactInfo * const networkWeaponImpactInfo = NULL, const bool silenced = false, const Vector3* forceDirection = 0, u8 damageAggregationCount = 0);

	void SetWeaponIdleFlag(bool bUseIdle);

	//
	// Audio
	//

	const audWeaponAudioComponent& GetAudioComponent() const;
	audWeaponAudioComponent& GetAudioComponent();

	// Is the weapon silenced?
	bool GetIsSilenced() const;

	//
	// Cooking
	//
	void StartCookTimer(u32 uTimeInMillisecond, CEntity *pCookEntity = NULL)  {m_uCookTime = uTimeInMillisecond; m_bCooking = true; m_pCookEntity = pCookEntity; }
	void CancelCookTimer() {m_bCooking=false; m_pCookEntity = NULL; }
	u32  GetCookTime() const {return m_uCookTime;}
	float GetCookTimeLeft(CPed* pFiringPed, u32 uTimeInMilliseconds)  const; 
	bool GetIsCooking()  const {return m_bCooking;}

	//
	// Components
	//

	typedef atFixedArray<CWeaponComponent*, CWeaponComponentPoint::MAX_WEAPON_COMPONENTS> Components;

	// Access the current components
	const Components& GetComponents() const;

	// Add a component
	void AddComponent(CWeaponComponent* pComponent, bool bDoReload = true);

	// Release a component
	bool ReleaseComponent(CWeaponComponent* pComponent);

	// Get the clip component
	const CWeaponComponentClip* GetClipComponent() const;
	CWeaponComponentClip* GetClipComponent();

	// Get the scope component
	const CWeaponComponentScope* GetScopeComponent() const;
	CWeaponComponentScope* GetScopeComponent();

	// Get the suppressor component
	const CWeaponComponentSuppressor* GetSuppressorComponent() const;

	// Get the targeting component
	const CWeaponComponentProgrammableTargeting* GetTargetingComponent() const;

	// Get the flashlight component
	CWeaponComponentFlashLight* GetFlashLightComponent();

	// Get the laser sight component
	const CWeaponComponentLaserSight* GetLaserSightComponent() const;

	// Get the variant model component
	const CWeaponComponentVariantModel* GetVariantModelComponent() const;

#if FPS_MODE_SUPPORTED
	bool HasGripAttachmentComponent() const;
#endif

	//
	// Camera
	//

	// Get the camera to use
	u32 GetDefaultCameraHash() const;

	// Get the recoil shake hash
	u32 GetFirstPersonRecoilShakeHash() const;

	// Get the recoil amount
	f32 GetRecoilShakeAmplitude() const;

	// Do we use a scope?
	bool GetHasFirstPersonScope() const;

	// Override functions that determine if we can lock on
	bool GetCanLockonOnFoot() const;
	bool GetCanLockonInVehicle() const;

	//
	// Shaders
	//

	// Update shader variables
	void UpdateShaderVariables(u8 uTintIndex);
	void UpdateShaderVariablesForDrawable(CDynamicEntity* pDrawableEntity, u8 uTintIndex);

	// Get the tint index
	u8 GetTintIndex() const;

	u8 GetDrawableEntityTintIndex() const;

	u32 GetCamoDiffuseTexIdx() const;
	void SetCamoDiffuseTexIdx(u32 uIdx);

	//
	// Timer
	//
	void SetTimer(u32 uTimer);

	//
	// Prop lifetime
	//
	u32 GetPropEndOfLifeTimeoutMS() const { return m_PropEndOfLifeTimeoutMS; }
	void SetPropEndOfLifeTimeoutMS(u32 uNewTimeoutInMs) { m_PropEndOfLifeTimeoutMS = uNewTimeoutInMs; }

	//
	// Debug
	//

#if __BANK
	// Debug rendering
	void RenderDebug(CEntity* pFiringEntity) const;
	static void AddWidgets(bkBank &rBank);
#endif // __BANK

	// For preserving Network peds Shot interval time
	void SetNextShotAllowedTime( u32 uNextShotAllowedTime ) {m_uNextShotAllowedTime = uNextShotAllowedTime; }
	u32  GetNextShotAllowedTime() { return m_uNextShotAllowedTime;}
	
	void Update_HD_Models(CEntity* pFiringEntity);	// handle requests for high detail models
	void ShaderEffect_HD_CreateInstance();
	void ShaderEffect_HD_DestroyInstance();

	void RequestHdAssets() { m_bExplicitHdRequest = true; }

	inline bool GetIsCurrentlyHD() const { return m_weaponLodState == WLS_HD_AVAILABLE; }
	inline void SetWeaponLodState(eWeaponLodState state) { m_weaponLodState = (u8)state; }

	// Handle w_idle animation by creating move network and setting clipset
	void CreateWeaponIdleAnim(const fwMvClipSetId& ClipSetId, bool bInVehicle);

	// Handle the visibility of the gun feed
	void ProcessGunFeed(bool visible);

	//
	// Ammo
	//
	void UpdateAmmoAfterFiring(CEntity* pFiringEntity, const CWeaponInfo* pWeaponInfo, s32 iAmmoToDeplete);

	void SetNoSpinUp(bool bNoSpinUp) {m_bNoSpinUp = bNoSpinUp;}

	void ProcessAnimation(CEntity* pFiringEntity);

	void SetWeaponRechargeAnimation();

private:

	//
	// Initialization
	//

	void CreateMoveNetworkHelper();

	//
	// States
	//

	// Perform state update
	void ProcessState(CEntity* pFiringEntity, u32 uTimeInMilliseconds);

	// Update the animation
	void UpdateAnimationAfterFiring(CEntity* pFiringEntity, f32 fFireAnimRate);

	// Decide which state to set after firing - based on ammo
	void UpdateStateAfterFiring(CEntity* pFiringEntity);

	// States
	void StateReadyUpdate(CEntity* pFiringEntity);
	void StateWaitingToFireUpdate(CEntity* pFiringEntity);
	void StateReloadingUpdate(CEntity* pFiringEntity);
	void StateOutOfAmmoUpdate(CEntity* pFiringEntity);
	void StateSpinUp(CEntity* pFiringEntity);
	void StateSpinDown(CEntity* pFiringEntity);

	//
	// Firing
	//

	// Firing
	bool FireDelayedHit(const sFireParams& params, const Vector3& vStart, const Vector3& vEnd);
	bool FireProjectile(const sFireParams& params, const Vector3& vStart, const Vector3& vEnd);
	bool FireVolumetric(CEntity* pFiringEntity, const Matrix34& weaponMatrix, s32 iVehicleWeaponIndex, s32 iVehicleWeaponBoneIndex, const Vector3& vStart, const Vector3& vEnd, float fFireAnimRate);
	
	// Do everything that needs to be done when a gun is successfully fired - effects, ai etc.
	void DoWeaponFire(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd, s32 iVehicleWeaponIndex, s32 iVehicleWeaponBoneIndex, s32 iAmmoToDeplete, f32 fFireAnimRate, bool bFirstShot, bool bBlank, bool bAllowRumble, bool bFiredFromAirDefence = false);
	void DoWeaponFirePlayer(CEntity* pFiringEntity, bool bAllowRumble);
	void DoWeaponFirePhysics();
	void DoWeaponFireAI(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd, bool bFirstShot);
	void DoWeaponFireAudio(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd);

	public:
	void DoWeaponFireKillShotAudio(CEntity* pFiringEntity, bool isKillShot);

	// PURPOSE: Given a position, a forward vector and the up vector of the world
	// this method builds a matrix that faces the direction given by the forward
	// vector.
	// PARAMS:
	// - vPosition: Position where we want the matrix to be set
	// - vForwardVector: Direction that we want the matrix to face
	// - vWorldUpVector: World's up vector
	// RETURNS: The matrix that is going to be built.
	Matrix34 BuildViewMatrix(const Vector3& vPosition, Vector3 vForwardVector) const;

	private:
	//
	// Accuracy
	//

	// Calculate an adjusted end point based on the firing vector and the aim assist target
	bool ComputeBulletBending(CEntity* pFiringEntity, const Vector3& vStart, Vector3& vAdjustedEnd, float fWeaponRange) const;

	// Adjust the end point based on accuracy calculations
	bool ComputeAccuracy(CEntity* pFiringEntity, const CEntity* pTargetEntity, Vec3V_In vStart, Vec3V_InOut vEnd, sWeaponAccuracy& weaponAccuracy, bool bSetPerfectAccuracy = false, float fDesiredTargetDist = -1.0f, bool bIsBlindFire=false, eAnimBoneTag nForcedRandomHitBone = BONETAG_INVALID);

	//
	// Processing
	//

	void ProcessFpsSightDof(CEntity* pFiringEntity);
	void ProcessStats(CEntity* pFiringEntity);
	void ProcessCookTimer(CEntity* pFiringEntity, u32 uTimeInMilliseconds);
	void ProcessMuzzleSmoke();

	//
	// Misc.
	//
	void DoWeaponMuzzle(CEntity* pFiringEntity, s32 iVehicleWeaponBoneIndex=-1);
	void DoWeaponGunshell(CEntity* pFiringEntity, s32 iVehicleWeaponIndex=0, s32 iVehicleWeaponBoneIndex=-1) const;
	bool ShouldDoBulletTrace(CEntity* pFiringEntity);
	bool GetIsAccurate(const Vector3& vEnd, const Vector3& vAdjustedEnd) const;
	bool GetUsesBulletPenetration(const CEntity* pFiringEntity) const;
	s32 GetReloadTime(const CEntity* pFiringEntity) const;
	void CreateSpinUpSonarBlip(CEntity* pFiringEntity);

	//
	// Components
	//

	void InitComponent(CWeaponComponent* pComponent, const bool bDoReload = true);
	void ShutdownComponent(CWeaponComponent* pComponent);

	//
	// Debug
	//

#if __BANK
	// Get the state name
	static const char* GetStateName(State state);
#endif // __BANK

	//
	// Members
	//

	// Variables that need alignment at the top
	// The cached muzzle matrix, stored as a Quat/Vec for space
	QuatV m_muzzleRot;
	Vec3V m_muzzlePos;
	// The previous shot position, so we can base the next shot on it
	Vec3V m_LastOffsetPosition;

	// The info that describes the basic weapon attributes
	RegdConstWeaponInfo m_pWeaponInfo;

	// Global timer
	u32 m_uGlobalTime;

	// State timer
	u32 m_uTimer;

	// Shot time
	u32 m_uNextShotAllowedTime;

	// Ammo - packed
	// Total ammo
	s16 m_iAmmoTotal;
	// Current ammo in clip
	s16 m_iAmmoInClip;

	// Entity that represents our drawable/skeleton etc.
	// Used to look up bones or play animations on
	RegdDyn m_pDrawableEntity;

	// Observer
	fwRegdRef<CWeaponObserver> m_pObserver;

	// Audio
	audWeaponAudioComponent m_WeaponAudioComponent;

	// Components
	Components m_Components;
	// Could ditch these by re-writing if need be
	CWeaponComponentClip* m_pClipComponent;
	CWeaponComponentScope* m_pScopeComponent;
	CWeaponComponentSuppressor* m_pSuppressorComponent;
	CWeaponComponentProgrammableTargeting* m_pTargetingComponent;
	CWeaponComponentFlashLight* m_pFlashLightComponent;
	CWeaponComponentLaserSight* m_pLaserSightComponent;
	CWeaponComponentVariantModel* m_pVariantModelComponent;

	// Move network helper
	CMoveNetworkHelper* m_pMoveNetworkHelper;

	//FPS Mode specific weapon animations
	CMoveNetworkHelper* m_pFPSMoveNetworkHelper;

	// For weapons with timers
	u32 m_uCookTime;

	// muzzle
	RegdEnt m_pMuzzleEntity;
	s32 m_iMuzzleBoneIndex;
	f32 m_muzzleSmokeLevel;
	Vector3 m_vMuzzleOffset;

	// For not triggering multiple stealth blips in a frame
	u32 m_uLastStealthBlipTime;

	// Current barrel spin
	f32 m_fBarrelSpin;

	// Rotate the barrel of grenade launcher
	bool m_bNeedsToRotateBarrel;
	f32 m_fBarrelHasRotated;
	f32 m_fBarrelNeedsToRotate;

	u32 m_FiringStartTime;
	u32 m_uLastShotTimer;

	// Current idle anim phase
	float m_fIdleAnimPhase;
	float m_fPrevIdleAnimPhase;
	
	// End of life timer override for props
	u32 m_PropEndOfLifeTimeoutMS;

	fwCustomShaderEffect* m_pStandardDetailShaderEffect;

	// Grip clipset/clip
	fwMvClipSetId m_GripClipSetId;
	fwMvClipId m_GripClipId;

	RegdEnt m_pCookEntity;

	// the current value of the fps sight dof (for interpolation purposes).
	bool m_bFpsSightDofFirstUpdate;
	bool m_bFpsSightDofPlayerWasFirstPersonLastUpdate;
	float m_FpsSightDofValue;

	// Packed variables
	// State
	u8 m_State;
	// Lod state
	u8 m_weaponLodState;
	// Tint palette index
	u8 m_uTintIndex;
	// Flags
	u32 m_bTemporaryNetworkWeapon : 1;		// Indicates this weapon is from a temporary network event
	u32 m_bNeedsToSpinDown : 1;
	u32 m_bValidMuzzleMatrix : 1;
	u32 m_bCooking : 1;
	u32 m_bCanActivateMoveNetwork : 1;
	u32 m_bExplicitHdRequest : 1;
	u32 m_bReloadClipNotEmpty : 1;
	u32 m_bSilenced : 1;
	u32 m_bNoSpinUp : 1;	// Ignore spin up state if set
	u32 m_bHasWeaponIdleAnim : 1;
	u32 m_bHasInVehicleWeaponIdleAnim : 1;
	u32 m_bOutOfAmmoAnimIsPlaying : 1;

#if FPS_MODE_SUPPORTED
	u32 m_bHasGripAttachmentComponent : 1;
#endif

	// Cloud tunable
	static float sm_fDamageOverrideForAPPistol;
	static bool	 sm_bDisableMarksmanRecoilFix;

private:

	// Static ids
	static const fwMvFlagId ms_WeaponIdleFlagId;
	static const fwMvFlagId ms_WeaponInVehicleIdleFlagId;
	static const fwMvFlagId ms_DisableIdleFilterFlagId;
	static const fwMvFlagId ms_HasGripAnimId;
	static const fwMvClipId ms_WeaponFireClipId;
	static const fwMvClipId ms_GripClipId;
	static const fwMvFloatId ms_GripBlendDurationId;
	static const fwMvFloatId ms_WeaponIdleBlendDuration;
	static const fwMvFloatId ms_WeaponIdlePhase;
	static const fwMvFloatId ms_WeaponIdleRate;
	static const fwMvRequestId ms_RestartGripId;
	static const fwMvBooleanId ms_UseActionModeGripId;
	static const fwMvBooleanId ms_WeaponFireEndedId;

	static dev_float ms_fBulletMissModifier;
	static dev_float ms_fProjectileMissModifier;
	static dev_float ms_fBlindFireSpreadModifier;
	static dev_float ms_fBlindFireBatchSpreadModifier;
	static dev_float ms_fMinBlindFireSpread;

public:
	static const fwMvRequestId ms_InterruptFireId;

#if __BANK
	static bool bEnableHiDetail;
	static bool bForceLoDetail;

	// Allocate debug data on demand
	struct sDebug
	{
		sDebug()
			: m_fFiringRate(0.f)
			, m_iFirstShotTime(-1)
			, m_iLastShotTime(-1)
			, m_iShotsFired(0)
			, m_iAccuracyShotsFired(0)
			, m_iAccuracyShotsHit(0)
			, m_iAccuracyAccurateShots(0)
			, m_iAccuracyRestrictedShots(0)
			, m_iAccuracyBlanksFired(0)
		{
		}

		// Calculate the approximate firing rate
		f32 m_fFiringRate;
		s32 m_iFirstShotTime;
		s32 m_iLastShotTime;
		s32 m_iShotsFired;

		// Accuracy
		s32 m_iAccuracyShotsFired;
		s32 m_iAccuracyShotsHit;
		s32 m_iAccuracyAccurateShots;
		s32 m_iAccuracyRestrictedShots;
		s32 m_iAccuracyBlanksFired;
	};
	mutable sDebug* m_pDebugData;
#endif //__BANK
};

////////////////////////////////////////////////////////////////////////////////

inline CWeapon::State CWeapon::GetState() const
{
	return (CWeapon::State)m_State;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::SetState(State state)
{
	m_State = (u8)state;
}

////////////////////////////////////////////////////////////////////////////////

inline const CEntity* CWeapon::GetEntity() const
{
	return m_pDrawableEntity;
}

////////////////////////////////////////////////////////////////////////////////

inline const CEntity* CWeapon::GetMuzzleEntity() const
{
	return m_pMuzzleEntity;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeapon::GetMuzzleBoneIndex() const
{
	return m_iMuzzleBoneIndex;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponInfo* CWeapon::GetWeaponInfo() const
{
	weaponFatalAssertf(m_pWeaponInfo, "m_pWeaponInfo is NULL - should always point to something");
	return m_pWeaponInfo;
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeapon::GetWeaponHash() const
{
	return m_pWeaponInfo->GetHash();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeapon::GetHumanNameHash() const
{
	return weaponVerifyf(m_pWeaponInfo, "m_pWeaponInfo is NULL - should always point to something") ? m_pWeaponInfo->GetHumanNameHash() : 0;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponModelInfo* CWeapon::GetWeaponModelInfo() const
{
	const CBaseModelInfo* pModelInfo = m_pDrawableEntity ? m_pDrawableEntity->GetBaseModelInfo() : NULL;
	if(pModelInfo && pModelInfo->GetModelType() == MI_TYPE_WEAPON)	// Might be an ambient prop which is not a weapon model now
	{
		//weaponFatalAssertf(pModelInfo->GetModelType() == MI_TYPE_WEAPON, "NULL Model Index, or not a Weapon Model Index");
		return static_cast<const CWeaponModelInfo*>(pModelInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline eDamageType CWeapon::GetDamageType() const
{
	return GetWeaponInfo()->GetDamageType();
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeapon::GetRange() const
{
	return GetWeaponInfo()->GetRange();
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::SetAmmoTotal(s32 iAmmoTotal)
{
	if(!m_pObserver || m_pObserver->NotifyAmmoChange(GetWeaponHash(), iAmmoTotal))
	{
		m_iAmmoTotal = (s16)Clamp(iAmmoTotal, 0, SHRT_MAX);
		// If we can't reload, update the clip as well
		if(!GetWeaponInfo()->GetCanReload() || m_iAmmoTotal < m_iAmmoInClip)
		{
			SetAmmoInClip((s32)m_iAmmoTotal);
		}

		if(m_iAmmoTotal == 0 && m_pWeaponInfo->GetUsesAmmo())
		{
			SetState(STATE_OUT_OF_AMMO);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::SetAmmoTotalInfinite()
{
	m_iAmmoTotal = INFINITE_AMMO;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::AddToAmmoTotal(s32 iAmmoAdd)
{
	if(GetAmmoTotal() != INFINITE_AMMO)
	{
		SetAmmoTotal(GetAmmoTotal() + iAmmoAdd);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeapon::GetAmmoTotal() const
{
	return (s32)m_iAmmoTotal;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::SetAmmoInClip(s32 iAmmoInClip)
{
	iAmmoInClip = Min(iAmmoInClip, SHRT_MAX);
	m_iAmmoInClip = (s16)Clamp(iAmmoInClip, 0, GetClipSize());
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeapon::GetAmmoInClip() const
{
	return (s32)m_iAmmoInClip;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetIsClipEmpty() const
{
	return GetClipSize() > 0 && m_iAmmoInClip <= 0;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeapon::GetClipSize() const
{
	return m_pClipComponent ? m_pClipComponent->GetClipSize() : GetWeaponInfo()->GetClipSize();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeapon::GetBulletsInBatch() const
{
	if (m_pClipComponent && m_pClipComponent->GetInfo()->GetBulletsInBatch() > 0)
	{
		return m_pClipComponent->GetInfo()->GetBulletsInBatch();
	}
	return GetWeaponInfo()->GetBulletsInBatch();
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::SetObserver(CWeaponObserver* pObserver)
{
	m_pObserver = pObserver;
}

////////////////////////////////////////////////////////////////////////////////

inline const CPed* CWeapon::GetOwner() const
{
	if (m_pObserver)
		return m_pObserver->GetOwner();
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeapon::MarkAsTemporaryNetworkWeapon()
{
	m_bTemporaryNetworkWeapon = true;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetIsTemporaryNetworkWeapon() const
{
	return m_bTemporaryNetworkWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline const audWeaponAudioComponent& CWeapon::GetAudioComponent() const 
{
	return m_WeaponAudioComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline audWeaponAudioComponent& CWeapon::GetAudioComponent()
{
	return m_WeaponAudioComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetIsSilenced() const
{
	return m_bSilenced;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeapon::Components& CWeapon::GetComponents() const
{
	return m_Components;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentClip* CWeapon::GetClipComponent() const
{
	return m_pClipComponent;
}

inline CWeaponComponentClip* CWeapon::GetClipComponent()
{
	return m_pClipComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentScope* CWeapon::GetScopeComponent() const
{
	return m_pScopeComponent;
}

inline CWeaponComponentScope* CWeapon::GetScopeComponent() 
{
	return m_pScopeComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentSuppressor* CWeapon::GetSuppressorComponent() const
{
	return m_pSuppressorComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentProgrammableTargeting* CWeapon::GetTargetingComponent() const
{
	return m_pTargetingComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline CWeaponComponentFlashLight* CWeapon::GetFlashLightComponent()
{
	return m_pFlashLightComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentLaserSight* CWeapon::GetLaserSightComponent() const
{
	return m_pLaserSightComponent;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentVariantModel* CWeapon::GetVariantModelComponent() const
{
	return m_pVariantModelComponent;
}

////////////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
inline bool CWeapon::HasGripAttachmentComponent() const
{
	return m_bHasGripAttachmentComponent;
}
#endif

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeapon::GetDefaultCameraHash() const
{
	return m_pScopeComponent ? m_pScopeComponent->GetCameraHash() : GetWeaponInfo()->GetDefaultCameraHash();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeapon::GetFirstPersonRecoilShakeHash() const
{
	// Allow us to restore broken behaviour using a server tunable
	if (sm_bDisableMarksmanRecoilFix && GetWeaponHash() == ATSTRINGHASH("WEAPON_MARKSMANRIFLE", 0xc734385a))
	{
		return 0;
	}

	return GetWeaponInfo()->GetFirstPersonRecoilShakeHash();
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeapon::GetRecoilShakeAmplitude() const
{
	float fRecoilShakeAmp = m_pScopeComponent ? m_pScopeComponent->GetRecoilShakeAmplitude() : GetWeaponInfo()->GetRecoilShakeAmplitude();
	
	if (m_pSuppressorComponent)
	{
		fRecoilShakeAmp *= m_pSuppressorComponent->GetInfo()->GetRecoilShakeAmplitudeModifier();
	}

	return fRecoilShakeAmp;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetHasFirstPersonScope() const
{
	// With this flag enabled, we will always use full-screen scope rendering 
	if (m_pWeaponInfo && m_pWeaponInfo->GetHasFirstPersonScope())
	{
		return true;
	}
	// Otherwise, check individually for scope components with valid reticule hash entries
	else if (m_pScopeComponent && m_pScopeComponent->GetInfo()->GetReticuleHash() != 0)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetCanLockonOnFoot() const
{
	// Don't allow weapons with full-screen scopes to lock on
	if (GetHasFirstPersonScope())
	{
		return false;
	}

	return m_pWeaponInfo && m_pWeaponInfo->GetCanLockOnOnFoot();
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeapon::GetCanLockonInVehicle() const
{
	return m_pWeaponInfo && m_pWeaponInfo->GetCanLockOnInVehicle();
}

////////////////////////////////////////////////////////////////////////////////

inline u8 CWeapon::GetTintIndex() const
{
	return m_uTintIndex;
}

inline u8 CWeapon::GetDrawableEntityTintIndex() const
{
	if(m_pDrawableEntity)
	{
		return (u8) m_pDrawableEntity->GetTintIndex();
	}

	return 255;
}

inline void CWeapon::SetTimer(u32 uTimer)
{
	if(!m_pObserver || m_pObserver->NotifyTimerChange(GetWeaponHash(), uTimer))
	{
		m_uTimer = uTimer;
	}
}

////////////////////////////////////////////////////////////////////////////////
#if __BANK
// forward declare pool full callback so we don't get two versions of it
template<> void fwPool<CWeapon>::PoolFullCallback();
#endif // __BANK
////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_H
