#ifndef PROJECTILE_H
#define PROJECTILE_H

// Game headers
#include "Objects/Object.h"
#include "Network/General/NetworkUtil.h"

// Forward declarations
class CAmmoProjectileInfo;

#define DR_PROJECTILE DR_ENABLED && 0
#if DR_PROJECTILE
#define DR_PROJECTILE_ONLY(x) x
#define DR_PROJECTILE_ENABLED 1
#else
#define DR_PROJECTILE_ONLY(x)
#define DR_PROJECTILE_ENABLED 0
#endif

//////////////////////////////////////////////////////////////////////////
// CProjectile
//////////////////////////////////////////////////////////////////////////

class CProjectile : public CObject
{
	DECLARE_RTTI_DERIVED_CLASS(CProjectile, CObject);

public:

	static bool IsEntityInvisibleCardboardBoxAttachedToVehicle(const CEntity& rEnt);
	static s16 GetHitBoneIndexFromFrag(const CEntity* pStickEntity, s32 iStickComponent);

	// Construction
	CProjectile(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CNetFXIdentifier* pNetIdentifier = NULL, bool bCreatedFromScript = false);

	// Destruction
	virtual ~CProjectile();

	static void InitTunables(); 

	// Processing
	virtual bool RequiresProcessControl() const { return true; }
	virtual bool ProcessControl();

	virtual void ProcessImpact(phIntersection& intersection);
	virtual void ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact);

	virtual void OnActivate(phInst* pInst, phInst* pOtherInst);

	void PostPreRender();

	// Physics Processing
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, s32 iTimeSlice);

	// Called when we collide with something
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

	virtual void ProcessPostPhysics( void );

	// Called after each physics update
	virtual void PostSimUpdate();

	// Set the position
	virtual void SetPosition(const Vector3& vec, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);

	// Set the matrix
	virtual void SetMatrix(const Matrix34& mat, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);

	// Activate the projectile
	virtual void Fire(const Vector3& vDirection, const f32 fLifeTime = -1.0f, f32 fLaunchSpeedOverride = -1.0f, bool bAllowDamping = true, bool bScriptControlled = false, bool bCommandFireSingleBullet = false, bool bIsDrop = false, const Vector3* vTargetVelocity = NULL, bool bDisableTrail = false, bool bAllowToSetOwnerAsNoCollision = true);

	// Destroy the projectile - used for getting rid of projectiles discretely
	virtual void Destroy();
	
	// Fades the projectile out
	bool FadeOutProjectile();

	// Explode
	void TriggerExplosion(const u32 iRandomDelayMax = 0);

	// Set the entity that launched the projectile - 
	// A bit dubious, but it's used to change the player owned projectiles when the player gets respawned, so they still attribute the player with any kills
	void SetOwner(CEntity* pNewOwner)		{ m_pOwner = pNewOwner; }
	
	// Set the net fx identifier
	void SetNetFXIdentifier(CNetFXIdentifier& identifier) { m_networkIdentifier = identifier; }

	// Set that the projectile in an ordnance (attached to the end of a weapon or a hand) rather than a soon to be exploding projectile
	void SetOrdnance(bool isOrdanance)		{ m_iFlags.ChangeFlag(PF_Ordnance, isOrdanance); }
	bool GetIsOrdnance() const				{ return m_iFlags.IsFlagSet(PF_Ordnance); }

	void SetPrimed(bool isPrimed)		{ m_iFlags.ChangeFlag(PF_Priming, isPrimed); }
	void SetFiredFromCover(bool isInCover)			{ m_iFlags.ChangeFlag(PF_FiredFromCover, isInCover); }

	// Whether or not this projectile flight is managed by script
	void SetIsScriptControlled(bool bScriptControlled)	{ m_iFlags.ChangeFlag(PF_ScriptControlled, bScriptControlled); }
	bool GetIsScriptControlled(void)					{ return m_iFlags.IsFlagSet(PF_ScriptControlled); }

	// Set if the impact explosion should be disabled
	void SetDisableImpactExplosion(bool bDisableExplosion) { m_iFlags.ChangeFlag(PF_DisableImpactExplosion, bDisableExplosion); }

	// Whether or not the resulting explosion can report crimes
	void SetDisableExplosionCrimes(bool bDisableCrimes)	{ m_iFlags.ChangeFlag(PF_DisableExplosionCrimes, bDisableCrimes); }
	bool GetDisableExplosionCrimes(void)				{ return m_iFlags.IsFlagSet(PF_DisableExplosionCrimes); }

	// Whether this projectile is active
	bool GetIsActive(void) const						{ return m_iFlags.IsFlagSet(PF_Active); }

	bool GetIsStuckToPed(void) const					{ return m_iFlags.IsFlagSet(PF_StuckToPed); }

	// Sends out a probe to determine if we are currently attached to anything
	void ProcessStickyShapeTest(void);
	void ProcessStickyShapeTest(const Vector3 &vStart, const Vector3 &vEnd);

	// Initialise lifetime values
	void InitLifetimeValues(const f32 fLifeTime = -1.0f);

	inline bool CheckOwner()
	{
		if (!m_bCreatedWithoutOwnerFromScript)
		{
			if (!m_pOwner)
			{
				Displayf("CProjectile::ProcessPostPhysics: Destroy(). Identifier: %d:%d Reason: !m_pOwner.", m_networkIdentifier.GetPlayerOwner(), m_networkIdentifier.GetFXId());
				Destroy();
				return true;
			}
		}
		return false;
	}
	//
	// Types
	//

	virtual const CProjectile* GetAsProjectile() const { return this; }
	virtual CProjectile* GetAsProjectile() { return this; }

	//
	// Accessors
	//

	// Get the entity that launched the projectile
	CEntity* GetOwner() const { return m_pOwner; }

	// Get the hash
	u32 GetHash() const { return m_uHash; }

	// Get the data
	const CAmmoProjectileInfo* GetInfo() const { weaponFatalAssertf(m_pInfo, "CProjectile::GetInfo: m_pInfo is NULL"); return m_pInfo; }

	// Get the weapon hash we were fired from
	u32 GetWeaponFiredFromHash() const { return m_uWeaponFiredFromHash; }

	// Get the effect group
	eWeaponEffectGroup GetEffectGroup() { return m_effectGroup; }
	void SetWeaponTintIndex(u8 weaponTintIndex) { m_weaponTintIndex = weaponTintIndex; }
	u8 GetWeaponTintIndex() { return m_weaponTintIndex; }

	// Get the network identifier
	CNetFXIdentifier& GetNetworkIdentifier() { return m_networkIdentifier; }
	// Get the network identifier
	const CNetFXIdentifier& GetNetworkIdentifier() const { return m_networkIdentifier; }


	// Get the life time after impact value
	float GetLifeTimeAfterImpact() const { return m_fLifeTimeAfterImpact; }

	// the sequence id of the task that created the projectile
	s32 GetTaskSequenceId() const { return m_taskSequenceId; }
	void SetTaskSequenceId(s32 seqId) { m_taskSequenceId = seqId; }

	// Get how long this projectile has been processed.
	float GetAge() const { return m_fAge; }

	// If successful, destroy the projectile by calling explode.
	virtual bool ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity *pEntityResponsible, u32 nWeaponUsedHash, phMaterialMgr::Id hitMaterial = 0);

	// Set in CommandCreateWeaponObject
	void SetCreatedWithoutOwnerByScript(bool bValue) { m_bCreatedWithoutOwnerFromScript = bValue; }
	bool GetCreatedWithoutOwnerByScript() { return m_bCreatedWithoutOwnerFromScript; }

#if __BANK
	// Debug rendering
	virtual void RenderDebug() const;
#endif // __BANK

	const CEntity* GetStickEntity() const { return m_pStickEntity; }

	const Vector3& GetStickPosition() const { return m_vStickPos; }
	const Vector3& GetStickNormal() const { return m_vStickNormal; }

	bool CalculateTimeUntilExplosion(float& fTime) const;

	// Sticky interface to allow outside systems to define a stick
	static u32 GetMineSoundsetHash(const u32 weaponNameHash);
	static bool ShouldStickToEntity( const CEntity* pHitEntity, const CEntity* pOwner, const float fProjectileWidth, Vec3V_In vStickPos, s32 iStickComponent, phMaterialMgr::Id iStickMaterialId, bool bIgnoreWheelSphereExclusion = false, bool bShouldStickToPeds = false, bool bIsHitEntityFriendly = false );
	void StickToEntity( CEntity* pStickEntity, Vec3V_In vStickPosition, Vec3V_In vStickNormal, s32 iStickComponent, phMaterialMgr::Id iStickMaterialId);
	static bool FindAveragedStickySurfaceNormal(const CEntity* pThrower, const CEntity &rEntity, Vec3V_In vHitPos, Vec3V_InOut rNormalInAndOut, float fProjectileWidth);
	bool NetworkStick(bool bStickEntity, CEntity* pStickEntity, Vector3& vStickPosition, Quaternion& stickQuatern, s32 iStickComponent, u32 iStickMaterialId);

	void ApplyDeformation(const CVehicle *pAttachedVehicle, const void* basePtr, bool bInit = false);

	void SetIgnoreDamageEntity(CEntity* pIgnoreDamageEntity) { m_pIgnoreDamageEntity = pIgnoreDamageEntity; }
	const CEntity* GetIgnoreDamageEntity() const { return m_pIgnoreDamageEntity; }

	s32 GetStickComponent() const { return m_iStickComponent; } 
	u32 GetStickMaterial() const { return (u32)m_iStickMaterialId; } 

	void SetCanDetonateInstantly(bool bCanDetonate) { m_bCanDetonateInstantly = bCanDetonate; }
	bool GetCanDetonateInstantly() const { return m_bCanDetonateInstantly; }

	void FlagAsThrownFromOutsideVehicle() { m_bThrownFromOutsideOfVehicle = true; } 

	void SetIgnoreDamageEntityAttachParent( bool bIgnore ) { m_ignoreDamageEntityAttachParent = bIgnore; }

	u32 GetTimeProjectileWasFiredMS() const { return m_iTimeProjectileWasFired; }
	Vector3 GetPositionFiredFrom() const { return m_vPositionFiredFrom; }

protected:

	// Explode the projectile
	virtual void Explode(const Vector3& vPosition, const Vector3& vNormal, const CEntity* pHitEntity, bool bHasCollided, const u32 iRandomDelayMax = 0);

	void RestoreDamping();

	// called when the projectile is attached
	void OnAttachment();

	//
	// Processing API
	//

	virtual void ProcessHomingAttractor();

	// Process the buoyancy
	virtual void ProcessInWater(const bool bUseExplosionPos = false);

	// Process the effects
	virtual void ProcessEffects();

	// Process the audio
	virtual void ProcessAudio();

	//
	// Sticky
	//

	// 
	bool StickToEntity();
	void DetachFromStickEntity();

	// 
	s16 GetHitBoneIndexFromFrag() const;

	//
	void GetStartAndEndVectorsForProjectile(Vector3& vStart, Vector3& vEnd) const;

	//
	// Querying
	//

	// Are we exploding in the air?
	bool GetIsExplodingInAir(bool bCollided, const CEntity* pHitEntity) const;

	//when we touch the ground we need to make events for peds to react against
	void CreateImpactEvents();

	void CreatePotentialBlastEvent(float fTimeUntilExplosion);

	void ProcessWhizzByEvents(bool bForceUpdate=false);

	bool IsComponentPartOfSpine(const CPed& rPed, s32 iComponent) const;

	bool GetIsEntityPartOfTrailer(const CEntity* pTargetEntity, const CEntity* pTrailerEntity) const;

	bool ProcessProximityMine();
	void ProcessProximityMineActivation();
	void ProcessProximityMineTrigger();

	//
	// Members
	//

	Mat34V m_matPrevious;
	Vector3 m_vLastWhizzByPosition;
	Vector3 m_vExplodePos;
	Vector3 m_vExplodeNormal;
	Vector3 m_vOldSpeed;
	Vector3 m_vStickPos;
	Vector3 m_vStickNormal;
	//
	// Network
	//

	// used to identify the projectile across the network
	CNetFXIdentifier m_networkIdentifier;

	// The hash of the projectile data
	u32 m_uHash;

	// Pointer to the data
	RegdAmmoProjectileInfo m_pInfo;

	// The hash of the weapon that fired the projectile
	u32 m_uWeaponFiredFromHash;

	// Entity that launched projectile
	RegdEnt m_pOwner;

	// the sequence id of the task that created the projectile
	s32 m_taskSequenceId;

	//how long has this projectile been active
	float m_fAge;

	// Lifetime
	float m_fLifeTime;
	float m_fLifeTimeAfterImpact;
	float m_fLifeTimeAfterExplosion;
	float m_fExplosionTime;
	float m_fLightIntensityMult;
	
	// Timer used for flashing light speed
	float m_fTimeStepTimer;

	// Water level - the water level height for our position
	float m_fWaterLevel;

	//
	// Damage
	//

	// How much damage to inflict (if not inflicted from explosion)
	float m_fDamage;

	// Damage type
	eDamageType m_damageType;

	//
	// Effects
	//

	// Effect group
	eWeaponEffectGroup m_effectGroup;

	// Trail evolution
	float m_trailEvo;

	// Fade out evaluation
	u32 m_FadeTime;

	s32	m_lightBone;
	
	//
	// Audio
	//

	// Looping projectile sound
	audSound* m_pSound;

	RegdEnt m_pHitEntity;
	phInst* m_pOtherInst;
	RegdEnt m_pIgnoreDamageEntity;
	s32 m_iOtherComponent;
	phMaterialMgr::Id m_iOtherMaterialId;

	RegdEnt m_pStickEntity;
	s32 m_iStickComponent;
	phMaterialMgr::Id m_iStickMaterialId;
	Vector3 m_vStickDeformation;

	RegdPed m_pHitPed;

	u32 m_uDestroyTime;

	u32 m_uExplodeTime;
	Vector3 m_vDir;
	RegdEnt m_pCollisionEntity; 

	u32 m_uLastWhizzByEventCheckTimeMS;

	bool m_bThrownFromOutsideOfVehicle;
	bool m_bHitPedFriendly;

	bool m_bAppliedImpactDamage;

	bool m_bCreatedWithoutOwnerFromScript;

	bool m_bNetworkHasHitPlayer;

	u8 m_weaponTintIndex;

	bool m_bCanDetonateInstantly;

	bool	m_bProximityMineTriggered;
	float	m_fProximityExplodeTimer;
	float	m_bProximityMineTriggeredByVehicleSpeed;
	float	m_fProximityMineStuckTime;
	bool	m_bProximityMineTriggeredByVehicle;
	bool	m_bProximityMineActive;
	bool	m_bProximityMineRepeatingDetonation;
	bool	m_bProximityMineActivationSafeMode;
	bool	m_bHasTriggeredAttachSound;

	bool	m_bHideStuckProjectileInVehicle;

	bool	m_bUseAirDefenceExplosion;
	u32		m_uTimeAirDefenceExplosionTriggered;
	Vector3 m_vAirDefenceFireDirection;

	bool	m_ignoreDamageEntityAttachParent;

	u32		m_iLastTimeProcessedHomingAttractor;
	u32		m_iTimeProjectileWasFired;
	Vector3 m_vPositionFiredFrom;

	// Projectile flags
	enum ProjectileFlags
	{
		PF_Active					= BIT0,
		PF_UsingLifeTimer			= BIT1,
		PF_UsingLifeTimeAfterImpact	= BIT2,
		PF_TrailInactive			= BIT3,
		PF_Exploded					= BIT4,
		PF_Impacted					= BIT5,
		PF_Ordnance					= BIT6,	// Attached to muzzle or ped's hand
		PF_FiredFromCover			= BIT7,
		PF_Priming					= BIT8,
		PF_DisableImpactExplosion	= BIT9,

		PF_MadeSoundEvent			= BIT10, // Whether this projectile made a sound event when it impacted yet or not
		PF_AnyImpactDetected		= BIT11, // Do we hit anything at all during pre compute impacts
		PF_HitPedThisFrame			= BIT12, // Need to keep track if the impact was from the ped to not confuse NMBalance
		PF_HitPedLastFrame			= BIT13,
		PF_ExplodeNextFrame			= BIT14, // When this object is damaged, set this boolean.  Then the next frame this object will be destroyed.
		PF_ScriptControlled			= BIT15,
		PF_ExplosionTriggered		= BIT16,
		PF_UsingDestroyTimer		= BIT17,
		PF_Sticked					= BIT18,
		PRF_LightIntensityGrowing	= BIT19,
		PRF_TriggerBeep				= BIT20,
		PRF_TriggerSplash			= BIT24,
		PF_ProcessCollisionProbe	= BIT25,
		PF_ForceExplosion			= BIT26, // Forces CExplosionManager::AddExplosion to explode the projectile
		PF_NoStickyBombOwnership	= BIT27, // The current sticky bomb owner wasn't the original owner.
		PF_DisableExplosionCrimes	= BIT28,
		PF_StuckToPed				= BIT29,
		PF_StuckToSpectatorPedOrGhostVeh		= BIT30,
		PF_UsingExplodeTimer		= BIT31, // Used to stagger explosions if a delay time is passed in to TriggerExplosions.

		PRF_ResetMask				= PRF_TriggerSplash,
	};

	// Flags
	fwFlags32 m_iFlags;

	static const float sm_fMinDistSqForProjectileSonarBlip;

	// Cloud tunables
	static bool sm_bProximityMineUseActivationSafeMode;
	static bool sm_bProximityMineUseBetterVehicleDetection;
	static float sm_fOppressor2MissilePitchYawRollClampOverride;
	static float sm_fOppressor2MissileTurnRateModifierOverride;

#if __BANK
	Vector3 m_vLastWhizzByDebugHeadPosition;
	Vector3 m_vLastWhizzByDebugTailPosition;
	bool m_bWhizzByEventCheckDebugDrawPending;
#endif // __BANK
};

#endif // PROJECTILE_H
