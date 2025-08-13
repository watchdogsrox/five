//
// weapons/ammoinfo.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef AMMO_INFO_H
#define AMMO_INFO_H

// Framework headers
#include "fwutil/flags.h"

// Game headers
#include "Weapons/Explosion.h"
#include "Weapons/Info/ItemInfo.h"
#include "weapons/helpers/weaponhelpers.h"

////////////////////////////////////////////////////////////////////////////////

class CAmmoInfo : public CItemInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CAmmoInfo, CItemInfo);
public:

	// Flags
	enum Flags
	{
		InfiniteAmmo					= BIT0,
		AddSmokeOnExplosion				= BIT1,
		Fuse							= BIT2,
		FixedAfterExplosion				= BIT3,
	};

	// Special Type
	enum SpecialType
	{
		None = 0,
		ArmorPiercing,
		Explosive,
		FMJ,
		HollowPoint,
		Incendiary,
		Tracer
	};

	// Constructor
	CAmmoInfo();

	//
	// Accessors
	//

	// Get the max amount of this type of ammo
	virtual s32 GetAmmoMax(const CPed* pPed) const;

	// Is the ammo considered infinite?
	bool GetIsUsingInfiniteAmmo() const { return m_AmmoFlags.IsFlagSet(InfiniteAmmo); }

	// Do we add smoke on explosion?
	bool GetAddSmokeOnExplosion() const { return m_AmmoFlags.IsFlagSet(AddSmokeOnExplosion); }

	// Do we have a fuse type mechanic?
	bool GetHasFuse() const { return m_AmmoFlags.IsFlagSet(Fuse); }

	// Do we set the project fixed physics after explosion?
	bool GetFixedAfterExplosion() const { return m_AmmoFlags.IsFlagSet(FixedAfterExplosion); }

	// Does this bullet have a special type property?
	CAmmoInfo::SpecialType GetAmmoSpecialType() const { return m_AmmoSpecialType; }
	bool GetIsSpecialType() const	{ return m_AmmoSpecialType > None; }
	bool GetIsArmorPiercing() const { return m_AmmoSpecialType == ArmorPiercing; }
	bool GetIsExplosive() const		{ return m_AmmoSpecialType == Explosive; }
	bool GetIsFMJ() const			{ return m_AmmoSpecialType == FMJ; }
	bool GetIsHollowPoint() const	{ return m_AmmoSpecialType == HollowPoint; }
	bool GetIsIncendiary() const	{ return m_AmmoSpecialType == Incendiary; }
	bool GetIsTracer() const		{ return m_AmmoSpecialType == Tracer; }

	// PURPOSE: Used by the PARSER.  Gets a name from an ammo pointer
	static const char* GetNameFromInfo(const CAmmoInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets an ammo pointer from a name
	static const CAmmoInfo* GetInfoFromName(const char* name);

private:

	//
	// Members
	//

	// Maximum ammo of this type
	s32 m_AmmoMax;		// for default
	s32 m_AmmoMax50;	// for shooting ability 50
	s32 m_AmmoMax100;	// for shooting ability 100
	s32 m_AmmoMaxMP;
	s32 m_AmmoMax50MP;	
	s32 m_AmmoMax100MP;	

	// Flags
	fwFlags8 m_AmmoFlags;

	// Special Type
	CAmmoInfo::SpecialType m_AmmoSpecialType;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CAmmoInfo(const CAmmoInfo& other);
	CAmmoInfo& operator=(const CAmmoInfo& other);
};

////////////////////////////////////////////////////////////////////////////////

class CAmmoProjectileInfo : public CAmmoInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CAmmoProjectileInfo, CAmmoInfo);
public:

	// Flags
	enum Flags
	{
		Sticky							= BIT0,
		DestroyOnImpact					= BIT1,
		ProcessImpacts					= BIT2,
		HideDrawable					= BIT3,
		TrailFxInactiveOnceWet			= BIT4,
		TrailFxRemovedOnImpact			= BIT5,
		DoGroundDisturbanceFx			= BIT6,
		CanBePlaced						= BIT7,
		NoPullPin						= BIT8,
		DelayUntilSettled 				= BIT9,
		CanBeDestroyedByDamage			= BIT10,
		CanBounce						= BIT11,
		DoubleDamping					= BIT12,
		StickToPeds						= BIT13,
		DontAlignOnStick				= BIT14,
		ThrustUnderwater				= BIT15,
		ApplyDamageOnImpact				= BIT16,
		SetOnFireOnImpact				= BIT17,
		DontFireAnyEvents				= BIT18,
		AlignWithTrajectory				= BIT19,
		ExplodeAtTrailFxPos				= BIT20,
		ProximityDetonation				= BIT21,
		AlignWithTrajectoryYAxis		= BIT22,
		HomingAttractor					= BIT23,
		Cluster							= BIT24,
		PreventMaxProjectileHelpText	= BIT25,
		ProximityRepeatedDetonation		= BIT26,
		UseGravityOutOfWater			= BIT27
	};

	// Explosion config
	struct sExplosion
	{
		eExplosionTag Default;
		eExplosionTag HitCar;
		eExplosionTag HitTruck;
		eExplosionTag HitBike;
		eExplosionTag HitBoat;
		eExplosionTag HitPlane;
		PAR_SIMPLE_PARSABLE;
	};

	struct sHomingRocketParams
	{
		bool ShouldUseHomingParamsFromInfo;
		bool ShouldIgnoreOwnerCombatBehaviour;
		float TimeBeforeStartingHoming;
		float TimeBeforeHomingAngleBreak;
		float TurnRateModifier;
		float PitchYawRollClamp;
		float DefaultHomingRocketBreakLockAngle;
		float DefaultHomingRocketBreakLockAngleClose;
		float DefaultHomingRocketBreakLockCloseDistance;
		PAR_SIMPLE_PARSABLE;
	};

	// Constructor
	CAmmoProjectileInfo();

	//
	// Accessors
	//

	// Get the amount of damping applied to this projectile over time
	f32 GetDamping() const { return m_Damping; }

	// Get the amount of gravity applied to this projectile over time
	f32 GetGravityFactor() const { return m_GravityFactor; }

	// Get the amount of damage caused on impact
	f32 GetDamage() const { return m_Damage; }

	// How long till the projectile expires?
	f32 GetLifeTime() const;

	// How long till the projectile expires?
	f32 GetFromVehicleLifeTime() const;

	// Life time of the projectile that will start the first successful impact
	f32 GetLifeTimeAfterImpact() const { return m_LifeTimeAfterImpact; }

	// Life time of the projectile after it explodes
	f32 GetLifeTimeAfterExplosion() const { return m_LifeTimeAfterExplosion; }	
	
	// How long till the projectile explode?
	f32 GetExplosionTime() const { return m_ExplosionTime; }

	// Get the force that this projectile will be launched at
	f32 GetLaunchSpeed() const { return m_LaunchSpeed; }

	// Get the time we allow to sperate, we have no thrust during this time. 
	f32 GetSeparationTime() const { return m_SeparationTime; }

	// How long it takes, when launched, to reach its target
	f32 GetTimeToReachTarget() const { return m_TimeToReachTarget; }

	// Get the ricochet tolerance - this determines if the projectile will explode on impact
	f32 GetRicochetTolerance() const { return m_RicochetTolerance; }

	// Get the ricochet tolerance if we have hit a ped
	f32 GetPedRicochetTolerance() const { return m_PedRicochetTolerance; }

	// Get the ricochet tolerance if we have hit a vehicle
	f32 GetVehicleRicochetTolerance() const { return m_VehicleRicochetTolerance; }

	// Get a multiplier value that will be applied to the friction coefficient for the projectile
	f32 GetFrictionMultiplier() const { return m_FrictionMultiplier; }

	// Get the default explosion tag of the projectile
	eExplosionTag GetExplosionTag() const { return m_Explosion.Default; } 

	// Get the explosion tag of the projectile based on what it has hit
	eExplosionTag GetExplosionTag(const CEntity* pHitEntity) const;

	// Get the hash of the fuse effect
	atHashWithStringNotFinal GetFuseFxHashName() const { return m_FuseFx; }

	// Get the hash of the proximity effect
	atHashWithStringNotFinal GetProximityFxHashName() const { return m_ProximityFx; }

	// Get the hash of the trail effect
	atHashWithStringNotFinal GetTrailFxHashName() const { return m_TrailFx; }

	// Get the hash of the primed effect
	atHashWithStringNotFinal GetPrimedFxHashName() const { return m_PrimedFx; }

	// Get the hash of the underwater trail effect
	atHashWithStringNotFinal GetTrailFxUnderWaterHashName() const { return m_TrailFxUnderWater; }

	// Get the trail effect fade in time
	f32 GetTrailFxFadeInTime() const { return m_TrailFxFadeInTime; }

	// Get the trail effect fade out time
	f32 GetTrailFxFadeOutTime() const { return m_TrailFxFadeOutTime; }

	// Get the hash of the fuse effect first person version
	atHashWithStringNotFinal GetFuseFirstPersonFxHashName() const { return m_FuseFxFP; }

	// Get the hash of the fuse effect first person version
	atHashWithStringNotFinal GetPrimedFirstPersonFxHashName() const { return m_PrimedFxFP; }

	// Get the hash of the default disturbance effect
	atHashWithStringNotFinal GetDisturbFxDefaultHashName() const { return m_DisturbFxDefault; }

	// Get the hash of the sand disturbance effect
	atHashWithStringNotFinal GetDisturbFxSandHashName() const { return m_DisturbFxSand; }

	// Get the hash of the water disturbance effect
	atHashWithStringNotFinal GetDisturbFxWaterHashName() const { return m_DisturbFxWater; }

	// Get the hash of the dirt disturbance effect
	atHashWithStringNotFinal GetDisturbFxDirtHashName() const { return m_DisturbFxDirt; }
	// Get the hash of the foliage disturbance effect
	atHashWithStringNotFinal GetDisturbFxFoliageHashName() const { return m_DisturbFxFoliage; }

	// Get the disturbance effect probe distance
	f32 GetDisturbFxProbeDist() const { return m_DisturbFxProbeDist; }

	// Get the disturbance effect scale
	f32 GetDisturbFxScale() const { return m_DisturbFxScale; }

	// Get the distance of the probe check used to determine whether this explosion occurs in the air or on the ground
	f32 GetGroundFxProbeDistance() const { return m_GroundFxProbeDistance; }

	// Get whether we should use the alternate set of tint colours for ptfx (proj trail / muzzle)
	f32 GetFxUseAlternateTintColor() const { return m_FxAltTintColour; }

	// Lights/Coronas
	bool GetLightOnlyActiveWhenStuck() const {return m_LightOnlyActiveWhenStuck;}
	bool GetLightFlickers() const {return m_LightFlickers;}
	bool GetLightSpeedsUp() const {return m_LightSpeedsUp;}
	const CWeaponBoneId& GetLightBone() const {return m_LightBone;}
	Vec3V GetLightColour() const {return m_LightColour;}
	float GetLightIntensity() const {return m_LightIntensity;}
	float GetLightRange() const {return m_LightRange;}
	float GetLightFalloffExp() const {return m_LightFalloffExp;}
	float GetLightFrequency() const {return m_LightFrequency;}
	float GetLightPower() const {return m_LightPower;}
	float GetCoronaSize() const {return m_CoronaSize;}
	float GetCoronaIntensity() const {return m_CoronaIntensity;}
	float GetCoronaZBias() const {return m_CoronaZBias;}

	// Proximity Trigger
	bool GetProximityAffectsFiringPlayer() const { return m_ProximityAffectsFiringPlayer; }
	bool GetProximityCanBeTriggeredByPeds() const { return m_ProximityCanBeTriggeredByPeds; }
	float GetProximityActivationTime() const;
	float GetProximityRepeatedDetonationActivationTime() const { return m_ProximityRepeatedDetonationActivationTime; }
	float GetProximityTriggerRadius() const {return m_ProximityTriggerRadius;}
	float GetProximityFuseTimePed() const {return m_ProximityFuseTimePed;}
	float GetProximityFuseTimeVehicleMin() const {return m_ProximityFuseTimeVehicleMin;}
	float GetProximityFuseTimeVehicleMax() const {return m_ProximityFuseTimeVehicleMax;}
	float GetProximityFuseTimeVehicleSpeed() const {return m_ProximityFuseTimeVehicleSpeed;}
	Vec3V GetProximityLightColourUntriggered() const { return m_ProximityLightColourUntriggered; }
	float GetProximityLightFrequencyMultiplierTriggered() const {return m_ProximityLightFrequencyMultiplierTriggered;}
	float GetTimeToIgnoreOwner() const {return m_TimeToIgnoreOwner;}
	
	// Charged Launch
	float GetChargedLaunchTime() const {return m_ChargedLaunchTime;}
	float GetChargedLaunchSpeedMult() const {return m_ChargedLaunchSpeedMult;}

	// Cluster Bomb
	eExplosionTag GetClusterExplosionTag() const {return m_ClusterExplosionTag;}
	u32 GetClusterExplosionCount() const {return m_ClusterExplosionCount;}
	float GetClusterMinRadius() const {return m_ClusterMinRadius;}
	float GetClusterMaxRadius() const {return m_ClusterMaxRadius;}
	float GetClusterInitialDelay() const {return m_ClusterInitialDelay;}
	float GetClusterInbetweenDelay() const {return m_ClusterInbetweenDelay;}

	//
	// Flags
	//

	bool GetIsSticky() const { return m_ProjectileFlags.IsFlagSet(Sticky); }
	bool GetShouldBeDestroyedOnImpact() const { return m_ProjectileFlags.IsFlagSet(DestroyOnImpact); }
	bool GetShouldProcessImpacts() const { return m_ProjectileFlags.IsFlagSet(ProcessImpacts); }
	bool GetShouldHideDrawable() const { return m_ProjectileFlags.IsFlagSet(HideDrawable); }
	bool GetTrailFxInactiveOnceWet() const { return m_ProjectileFlags.IsFlagSet(TrailFxInactiveOnceWet); }
	bool GetTrailFxRemovedOnImpact() const { return m_ProjectileFlags.IsFlagSet(TrailFxRemovedOnImpact); }
	bool GetDoesGroundDisturbanceFx() const { return m_ProjectileFlags.IsFlagSet(DoGroundDisturbanceFx); }
	bool GetCanBePlaced() const { return m_ProjectileFlags.IsFlagSet(CanBePlaced); }
	bool GetNoPullPin() const { return m_ProjectileFlags.IsFlagSet(NoPullPin); }	
	bool GetDelayUntilSettled() const { return m_ProjectileFlags.IsFlagSet(DelayUntilSettled); }
	bool GetCanBeDestroyedByDamage() const { return m_ProjectileFlags.IsFlagSet(CanBeDestroyedByDamage); }
	bool GetCanBounce() const { return m_ProjectileFlags.IsFlagSet(CanBounce); }
	bool GetDoubleDamping() const { return m_ProjectileFlags.IsFlagSet(DoubleDamping); }
	bool GetShouldStickToPeds() const { return m_ProjectileFlags.IsFlagSet(StickToPeds); }
	bool GetDontAlignOnStick() const { return m_ProjectileFlags.IsFlagSet(DontAlignOnStick); }
	bool GetShouldThrustUnderwater() const { return m_ProjectileFlags.IsFlagSet(ThrustUnderwater); }
	bool GetShouldApplyDamageOnImpact() const { return m_ProjectileFlags.IsFlagSet(ApplyDamageOnImpact); }
	bool GetShouldSetOnFireOnImpact() const { return m_ProjectileFlags.IsFlagSet(SetOnFireOnImpact); }
	bool GetDontFireAnyEvents() const { return m_ProjectileFlags.IsFlagSet(DontFireAnyEvents); }
	bool GetAlignWithTrajectory() const { return m_ProjectileFlags.IsFlagSet(AlignWithTrajectory); }
	bool GetExplodeAtTrailFxPos() const { return m_ProjectileFlags.IsFlagSet(ExplodeAtTrailFxPos); }
	bool GetIsProximityDetonation() const { return m_ProjectileFlags.IsFlagSet(ProximityDetonation); }
	bool GetIsProximityRepeatedDetonation() const { return m_ProjectileFlags.IsFlagSet(ProximityRepeatedDetonation); }
	bool GetAlignWithTrajectoryYAxis() const { return m_ProjectileFlags.IsFlagSet(AlignWithTrajectoryYAxis); }
	bool GetIsHomingAttractor() const { return m_ProjectileFlags.IsFlagSet(HomingAttractor); }
	bool GetIsCluster() const { return m_ProjectileFlags.IsFlagSet(Cluster); }
	bool GetPreventMaxProjectileHelpText() const { return m_ProjectileFlags.IsFlagSet(PreventMaxProjectileHelpText); }
	bool GetUseGravityOutOfWater() const { return m_ProjectileFlags.IsFlagSet(UseGravityOutOfWater); }

private:

	// Called after the data has been loaded
	void OnPostLoad();

	//
	// Members
	//

	f32 m_Damage;
	f32 m_LifeTime;
	f32 m_FromVehicleLifeTime;
	f32 m_LifeTimeAfterImpact;
	f32 m_LifeTimeAfterExplosion;
	f32 m_ExplosionTime;	// When to trigger explosion.
	f32 m_LaunchSpeed;
	f32 m_SeparationTime;
	f32 m_TimeToReachTarget;
	f32 m_Damping;
	f32 m_GravityFactor;
	f32 m_RicochetTolerance;
	f32 m_PedRicochetTolerance;
	f32 m_VehicleRicochetTolerance;
	f32 m_FrictionMultiplier;

	// Explosion
	sExplosion m_Explosion;

	// Effects
	atHashWithStringNotFinal m_FuseFx;
	atHashWithStringNotFinal m_ProximityFx;
	atHashWithStringNotFinal m_TrailFx;
	atHashWithStringNotFinal m_TrailFxUnderWater;
	atHashWithStringNotFinal m_PrimedFx;
	atHashWithStringNotFinal m_FuseFxFP;
	atHashWithStringNotFinal m_PrimedFxFP;
	f32 m_TrailFxFadeInTime;
	f32 m_TrailFxFadeOutTime;

	atHashWithStringNotFinal m_DisturbFxDefault;
	atHashWithStringNotFinal m_DisturbFxSand;
	atHashWithStringNotFinal m_DisturbFxWater;
	atHashWithStringNotFinal m_DisturbFxDirt;
	atHashWithStringNotFinal m_DisturbFxFoliage;
	f32 m_DisturbFxProbeDist;
	f32 m_DisturbFxScale;
	f32 m_GroundFxProbeDistance;
	bool m_FxAltTintColour;

	// Lights/Coronas	
	bool m_LightOnlyActiveWhenStuck;
	bool m_LightFlickers;
	bool m_LightSpeedsUp;
	CWeaponBoneId m_LightBone;	
	Vec3V m_LightColour;
	float m_LightIntensity;
	float m_LightRange;
	float m_LightFalloffExp;
	float m_LightFrequency;
	float m_LightPower;
	float m_CoronaSize;
	float m_CoronaIntensity;
	float m_CoronaZBias;

	// Proximity Trigger
	bool m_ProximityAffectsFiringPlayer;
	bool m_ProximityCanBeTriggeredByPeds;
	float m_ProximityActivationTime;
	float m_ProximityRepeatedDetonationActivationTime;
	float m_ProximityTriggerRadius;
	float m_ProximityFuseTimePed;
	float m_ProximityFuseTimeVehicleMin;
	float m_ProximityFuseTimeVehicleMax;
	float m_ProximityFuseTimeVehicleSpeed;
	Vec3V m_ProximityLightColourUntriggered;
	float m_ProximityLightFrequencyMultiplierTriggered;
	float m_TimeToIgnoreOwner;

	// Charged Launch
	float m_ChargedLaunchTime;
	float m_ChargedLaunchSpeedMult;

	// Cluster Bomb
	eExplosionTag m_ClusterExplosionTag;
	s32 m_ClusterExplosionCount;
	float m_ClusterMinRadius;
	float m_ClusterMaxRadius;
	float m_ClusterInitialDelay;
	float m_ClusterInbetweenDelay;

	// Flags
	fwFlags32 m_ProjectileFlags;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CAmmoProjectileInfo(const CAmmoProjectileInfo& other);
	CAmmoProjectileInfo& operator=(const CAmmoProjectileInfo& other);
};

////////////////////////////////////////////////////////////////////////////////

class CAmmoRocketInfo : public CAmmoProjectileInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CAmmoRocketInfo, CAmmoProjectileInfo);
public:

	// Constructor
	CAmmoRocketInfo();

	//
	// Accessors
	//

	// Returns the rockets forward and side drag coeff, use to control turning rate etc
	f32 GetForwardDragCoeff() const { return m_ForwardDragCoeff; }
	f32 GetSideDragCoeff() const { return m_SideDragCoeff; }

	// Get the time we will spend before homing in on the target
	f32 GetTimeBeforeHoming() const { return m_TimeBeforeHoming; }

	// Get the time before switching targets
	f32 GetTimeBeforeSwitchTargetMin() const { return m_TimeBeforeSwitchTargetMin; }
	f32 GetTimeBeforeSwitchTargetMax() const { return m_TimeBeforeSwitchTargetMax; }

	// Get the proximity radius - if we are this close to our target we will explode
	f32 GetProximityRadius() const { return m_ProximityRadius; }

	// Rocket homing turn rates
	f32 GetPitchChangeRate() const { return m_PitchChangeRate; }
	f32 GetRollChangeRate() const { return m_RollChangeRate; }
	f32 GetYawChangeRate() const { return m_YawChangeRate; }

	// How much does the rocket roll over when turning
	// Range 0->1
	f32 GetMaxRollAngleSin() const { return m_MaxRollAngleSin; }

	// Used to override rocket lifetime for the player in MP if they have a lock on target (see CProjectile::InitLifetimeValues).
	f32 GetLifeTimePlayerVehicleLockedOverrideMP() const { return m_LifeTimePlayerVehicleLockedOverrideMP; }

	// Homing params
	bool GetShouldUseHomingParamsFromInfo() const {return m_HomingRocketParams.ShouldUseHomingParamsFromInfo;}
	bool GetShouldIgnoreOwnerCombatBehaviour() const { return m_HomingRocketParams.ShouldIgnoreOwnerCombatBehaviour; }
	float GetTimeBeforeStartingHoming() const {return m_HomingRocketParams.TimeBeforeStartingHoming;}
	float GetTimeBeforeHomingAngleBreak() const { return m_HomingRocketParams.TimeBeforeHomingAngleBreak; }
	float GetTurnRateModifier() const {return m_HomingRocketParams.TurnRateModifier;}
	float GetPitchYawRollClamp() const {return m_HomingRocketParams.PitchYawRollClamp;}
	float GetDefaultHomingRocketBreakLockAngle() const {return m_HomingRocketParams.DefaultHomingRocketBreakLockAngle;}
	float GetDefaultHomingRocketBreakLockAngleClose() const {return m_HomingRocketParams.DefaultHomingRocketBreakLockAngleClose;}
	float GetDefaultHomingRocketBreakLockCloseDistance() const {return m_HomingRocketParams.DefaultHomingRocketBreakLockCloseDistance;}
private:

	//
	// Members
	//

	f32 m_ForwardDragCoeff;
	f32 m_SideDragCoeff;
	f32 m_TimeBeforeHoming;
	f32 m_TimeBeforeSwitchTargetMin;
	f32 m_TimeBeforeSwitchTargetMax;
	f32 m_ProximityRadius;
	f32 m_PitchChangeRate;
	f32 m_YawChangeRate;
	f32 m_RollChangeRate;
	f32 m_MaxRollAngleSin;
	f32 m_LifeTimePlayerVehicleLockedOverrideMP;
	sHomingRocketParams m_HomingRocketParams;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CAmmoRocketInfo(const CAmmoRocketInfo& other);
	CAmmoRocketInfo& operator=(const CAmmoRocketInfo& other);
};

////////////////////////////////////////////////////////////////////////////////

class CAmmoThrownInfo : public CAmmoProjectileInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CAmmoThrownInfo, CAmmoProjectileInfo);
public:

	// Constructor
	CAmmoThrownInfo();

	//
	// Accessors
	//

	// Get the max amount of this type of ammo
	s32 GetAmmoMax(const CPed* pPed) const;

	// Get the force that this projectile will be thrown at
	f32 GetThrownForce() const { return m_ThrownForce; }

	// Get the force that this projectile will be thrown at from a vehicle, e.g. grenades dropped from moving cars
	f32 GetThrownForceFromVehicle() const { return m_ThrownForceFromVehicle; }

private:

	//
	// Members
	//

	f32 m_ThrownForce;
	f32 m_ThrownForceFromVehicle;

	// An additional amount of ammo to add to the maximum when unlocked by script
	s32 m_AmmoMaxMPBonus;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CAmmoThrownInfo(const CAmmoThrownInfo& other);
	CAmmoThrownInfo& operator=(const CAmmoThrownInfo& other);
};

////////////////////////////////////////////////////////////////////////////////

// Regd pointer typedef
typedef	fwRegdRef<const CAmmoInfo> RegdAmmoInfo;
typedef	fwRegdRef<const CAmmoProjectileInfo> RegdAmmoProjectileInfo;

#endif // AMMO_INFO_H
