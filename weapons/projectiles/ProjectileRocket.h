#ifndef PROJECTILE_ROCKET_H
#define PROJECTILE_ROCKET_H

// Game headers
#include "Weapons/Projectiles/Projectile.h"

//////////////////////////////////////////////////////////////////////////
// CProjectileRocket
//////////////////////////////////////////////////////////////////////////

class CProjectileRocket : public CProjectile
{
public:

	// Construction
	CProjectileRocket(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript = false);

	// Destruction
	virtual ~CProjectileRocket();

	// Processing
	virtual bool ProcessControl();

	// Physics Processing
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, s32 iTimeSlice);

	// Activate the projectile
	virtual void Fire(const Vector3& vDirection, const f32 fLifeTime = -1.0f, f32 fLaunchSpeedOverride = -1.0f, bool bAllowDamping = true, bool bScriptControlled = false, bool bCommandFireSingleBullet = false, bool bIsDrop = false, const Vector3* pTargetVelocity = NULL, bool bDisableTrail = false, bool bAllowToSetOwnerAsNoCollision = true);

	// Get the target
	const CEntity* GetTarget() const { return m_pTarget; }

	// Set the target
	void SetTarget(const CEntity* pTarget) { m_pTarget = pTarget; }

	// Mark and check whether we've been redirected by a decoy flare
	void SetIsRedirected(bool bRedirected);
	bool GetIsRedirected() { return m_bHasBeenRedirected; }

	// Set miss flag
	void SetInitialLauncherSpeed( float fInitalLauncherSpeed ) { m_fLauncherSpeed = fInitalLauncherSpeed; }
	float GetLauncherSpeed() const { return m_fLauncherSpeed; }
	void SetCachedTargetPosition( const Vector3& vCachedPosition ) { m_vCachedTargetPos = vCachedPosition; }
	void SetInitialLauncherSpeed( CEntity* pOwner );
	Vector3 GetCachedTargetPosition( void ) const { return m_vCachedTargetPos; }  
	void SetIsAccurate( bool bIsAccurate )		{ m_bIsAccurate = bIsAccurate; }
	bool GetIsAccurate( void ) const			{ return m_bIsAccurate; }
	bool GetWasLockedOnWhenFired() const		{ return m_bOnFootHomingWeaponLockedOn; }
	void SetWasLockedOnWhenFired(bool bOnFootHomingWeaponLockedOn ) { weaponAssertf(NetworkInterface::IsNetworkOpen(),"Expected only used for syncing in network games"); m_bOnFootHomingWeaponLockedOn = bOnFootHomingWeaponLockedOn; }

	// Get the data
	const CAmmoRocketInfo* GetInfo() const { return static_cast<const CAmmoRocketInfo*>(CProjectile::GetInfo()); }

	//
	// Types
	//

	virtual const CProjectileRocket* GetAsProjectileRocket() const { return this; }
	virtual CProjectileRocket* GetAsProjectileRocket() { return this; }

#if __BANK
	// Debug rendering
	virtual void RenderDebug() const;
	static void InitWidgets();
#endif // __BANK

	void StopHomingProjectile() { m_bStopHoming = true; }
protected:

	// Do the audio component of firing
	void DoFireAudio();

	// Process the audio
	virtual void ProcessAudio();

	bool IsTargetAngleValid() const;

	void CalcHomingProjectileInputs(const Vector3& vTargetPos);
	void ApplyProjectileInputs(float fTimestep);	

	//
	// Members
	//

	// Miss flag
	Vector3 m_vCachedTargetPos;
	Vector3 m_vLaunchDir;

	// Target
	RegdConstEnt m_pTarget;

	// Flight model inputs
	float m_fPitch;
	float m_fRoll;
	float m_fYaw;

	// If we store a float here it saves on vector magnitude calculations every frame
	float m_fSpeed;

	// Time before we will start homing
	float m_fTimeBeforeHoming;
	float m_fTimeBeforeHomingAngleBreak;

	float m_fLauncherSpeed;
	float m_fTimeSinceLaunch;

	// Audio
	audSound* m_pWhistleSound;

	bool m_bIsAccurate : 1;
	bool m_bLerpToLaunchDir : 1;
	bool m_bApplyThrust : 1;

	bool m_bOnFootHomingWeaponLockedOn : 1;

	bool m_bWasHoming : 1;
	bool m_bStopHoming : 1;

	bool m_bHasBeenRedirected : 1;

	bool m_bTorpHasBeenOutOfWater : 1;

	Vector3 m_vCachedDirection;

	static float CalcHomingInput(float fValueDiff, float fTargetWidth, float fChangeRateMult);

#if __BANK
	static void SpawnRocketOnTail();
	static bool sm_bTargetNetPlayer;
	static bool sm_bTargetVehicle;
	static float sm_fHomingTestDistance;
	static float sm_fLifeTimeOverride;
	static float sm_fSpeedOverride;
	static char sm_ProjectileInfo[64];
	static char sm_WeaponInfo[64];
#endif	// __BANK
};


#endif // PROJECTILE_ROCKET_H
