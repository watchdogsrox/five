//
// weapons/weaponinfo.h
//
// Copyright (C) 1999-2014 Rockstar North.  All Rights Reserved. 
//

#ifndef WEAPON_INFO_H
#define WEAPON_INFO_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "vector/color32.h"
#include "vector/vector3.h"

// Framework headers
#include "fwanimation/animdefines.h"
#include "fwutil/flags.h"

// Game headers
#include "Animation/animdefines.h"
#include "animation/AnimBones.h"
#include "pickups/data/PickupIds.h"
#include "Renderer/HierarchyIds.h"
#include "Scene/Entity.h"
#include "Stats/StatsTypes.h"
#include "Weapons/Helpers/WeaponHelpers.h"
#include "Weapons/Info/ItemInfo.h"
#include "Weapons/Info/weaponinfoflags.h"
#include "Weapons/WeaponChannel.h"
#include "Weapons/WeaponEnums.h"

// Forward declarations
class CAmmoInfo;
class CPed;
class CVehicleWeaponInfo;
class CWeaponComponentInfo;
class CWeaponSwapData;

// Typedefs
typedef CWeaponInfoFlags::FlagsBitSet WeaponInfoFlags;

////////////////////////////////////////////////////////////////////////////////

enum eWeaponWheelSlot
{
	WEAPON_WHEEL_SLOT_PISTOL = 0,
	WEAPON_WHEEL_SLOT_SMG,
	WEAPON_WHEEL_SLOT_RIFLE,
	WEAPON_WHEEL_SLOT_SNIPER,
	WEAPON_WHEEL_SLOT_UNARMED_MELEE,
	WEAPON_WHEEL_SLOT_SHOTGUN,
	WEAPON_WHEEL_SLOT_HEAVY,
	WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL,

	MAX_WHEEL_SLOTS
};

// Flags used to load specified FPS weapon anims
enum FPSStreamingFlags
{
	FPS_StreamDefault = -1,
	FPS_StreamIdle,
	FPS_StreamRNG,
	FPS_StreamLT,
	FPS_StreamScope,
	FPS_StreamThirdPerson,

	// Max
	FPS_StreamCount,
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponTintSpecValues
{
public:

	struct CWeaponSpecValue
	{
		f32 SpecFresnel;
		f32 SpecFalloffMult;
		f32 SpecIntMult;
		f32 Spec2Factor;
		f32 Spec2ColorInt;
		Color32 Spec2Color;
		PAR_SIMPLE_PARSABLE;
	};

	CWeaponTintSpecValues() {}
	~CWeaponTintSpecValues() {}

	u32 GetHash() const { return m_Name.GetHash(); }
	s32 GetCount() const { return m_Tints.GetCount(); }

	const CWeaponSpecValue* GetSpecValuesForTint(s32 iTintIndex) const
	{
		if(iTintIndex >= 0 && iTintIndex < m_Tints.GetCount())
		{
			return &m_Tints[iTintIndex];
		}
		return NULL;
	}

	static const char* GetNameFromPointer(const CWeaponTintSpecValues* p);
	static const CWeaponTintSpecValues* GetPointerFromName(const char* name);

#if !__FINAL
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

private:

	atHashWithStringNotFinal m_Name;
	atArray<CWeaponSpecValue> m_Tints;
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponFiringPatternAliases
{
public:

	struct CFiringPatternAlias
	{
		atHashWithStringNotFinal FiringPattern;
		atHashWithStringNotFinal Alias;
		PAR_SIMPLE_PARSABLE;
	};

	CWeaponFiringPatternAliases() {}
	~CWeaponFiringPatternAliases() {}

	u32 GetHash() const { return m_Name.GetHash(); }

	const u32 GetAlias(const atHashWithStringNotFinal& firingPattern) const
	{
		for(s32 i = 0; i < m_Aliases.GetCount(); i++)
		{
			if(m_Aliases[i].FiringPattern.GetHash() == firingPattern.GetHash())
			{
				return m_Aliases[i].Alias.GetHash();
			}
		}
		return 0;
	}

	static const char* GetNameFromPointer(const CWeaponFiringPatternAliases* p);
	static const CWeaponFiringPatternAliases* GetPointerFromName(const char* name);

#if !__FINAL
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

private:

	atHashWithStringNotFinal m_Name;
	atArray<CFiringPatternAlias> m_Aliases;
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponUpperBodyFixupExpressionData
{
public:

	enum Type
	{
		Normal,
		Action,
		Stealth,
		Aiming,
		Max,
	};

	struct Data
	{
		float GetFixupWeight(const CPed& ped) const;
		float Idle;
		float Walk;
		float Run;
		PAR_SIMPLE_PARSABLE;
	};

	CWeaponUpperBodyFixupExpressionData() {}
	~CWeaponUpperBodyFixupExpressionData() {}

	u32 GetHash() const { return m_Name.GetHash(); }

	float GetFixupWeight(const CPed& ped) const;

	static const char* GetNameFromPointer(const CWeaponUpperBodyFixupExpressionData* p);
	static const CWeaponUpperBodyFixupExpressionData* GetPointerFromName(const char* name);

#if !__FINAL
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

#if __BANK
	// Add widgets
	void AddWidgets(bkBank& bank);
#endif // __BANK


private:

	atHashWithStringNotFinal m_Name;
	Data m_Data[Max];
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CAimingInfo
{
public:

	// Constructor
	CAimingInfo();

	// Destructor
	~CAimingInfo();

	//
	// Accessors
	//

	// Get the item hash
	u32 GetHash() const { return m_Name.GetHash(); }
	f32 GetHeadingLimit() const		{ return m_HeadingLimit; }
	f32 GetSweepPitchMin() const	{ return m_SweepPitchMin; }
	f32 GetSweepPitchMax() const	{ return m_SweepPitchMax; }

	// PURPOSE: Used by the PARSER.  Gets a name from an gun info pointer
	static const char* GetNameFromInfo(const CAimingInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a gun info pointer from a name
	static const CAimingInfo* GetInfoFromName(const char* name);

	// Function to sort the array so we can use a binary search
	static s32 PairSort(CAimingInfo* const* a, CAimingInfo* const* b) { return ((*a)->m_Name.GetHash() < (*b)->m_Name.GetHash() ? -1 : 1); }

#if !__FINAL
	// Validate the data structure after loading
	bool Validate() const;
#endif

#if !__NO_OUTPUT
	// Get the name
	const char* GetName() const { return m_Name.TryGetCStr(); }
#endif // !__FINAL

private:

	// Called after the data has been loaded
	void OnPostLoad();

	//
	// Members
	//

	// The hash of the name
	atHashWithStringNotFinal m_Name;
	f32 m_HeadingLimit;
	f32 m_SweepPitchMin;
	f32 m_SweepPitchMax;

	PAR_SIMPLE_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CAimingInfo(const CAimingInfo& other);
	CAimingInfo& operator=(const CAimingInfo& other);
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentPoint
{
public:

	struct sComponent
	{
		sComponent() : m_Default(false) {}
		atHashWithStringNotFinal m_Name;
		bool m_Default;
		PAR_SIMPLE_PARSABLE;
	};

	static const s32 MAX_WEAPON_COMPONENTS = 11;
	typedef atFixedArray<sComponent, MAX_WEAPON_COMPONENTS> Components;

	// Constructor
	CWeaponComponentPoint();

	// Get the attach bone
	u32 GetAttachBoneHash() const { return m_AttachBone.GetHash(); }
#if !__FINAL
	const char* GetAttachBoneName() const { return m_AttachBone.GetCStr(); }
#endif // !__FINAL

	// Get the attach bone
	eHierarchyId GetAttachBoneId() const { return m_AttachBoneId; }
	void SetAttachBoneId(eHierarchyId id) { m_AttachBoneId = id; }

	// Get the components
	const Components& GetComponents() const { return m_Components; }

private:

	// Called after the data has been loaded
	void OnPostLoad();

	// The bone we use for attachment
	atHashWithStringNotFinal m_AttachBone;

	// The corresponding hierarchy id for the attach bone
	eHierarchyId m_AttachBoneId;

	// Possible components
	Components m_Components;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponInfo : public CItemInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CWeaponInfo, CItemInfo);
public:

	static bool ShouldPedUseCoverReloads(const CPed& ped, const CWeaponInfo& weaponInfo);

	static const s32 MAX_ATTACH_POINTS = 7;
	typedef atFixedArray<CWeaponComponentPoint, MAX_ATTACH_POINTS> AttachPoints;
	static const s32 MAX_WEAPON_INFO_FLAGS = CWeaponInfoFlags::Flags_NUM_ENUMS;

	// Effects
	struct sFx
	{
		eWeaponEffectGroup EffectGroup;
		atHashWithStringNotFinal FlashFx;
		atHashWithStringNotFinal FlashFxAlt;
		atHashWithStringNotFinal FlashFxFP;
		atHashWithStringNotFinal FlashFxFPAlt;
		atHashWithStringNotFinal MuzzleSmokeFx;
		atHashWithStringNotFinal MuzzleSmokeFxFP;
		f32 MuzzleSmokeFxMinLevel;
		f32 MuzzleSmokeFxIncPerShot;
		f32 MuzzleSmokeFxDecPerSec;
		Vector3 MuzzleOverrideOffset;
		bool MuzzleUseProjTintColour;
		atHashWithStringNotFinal ShellFx;
		atHashWithStringNotFinal ShellFxFP;
		atHashWithStringNotFinal TracerFx;
		atHashWithStringNotFinal PedDamageHash;
		f32 TracerFxChanceSP;
		f32 TracerFxChanceMP;
		bool TracerFxIgnoreCameraIntersection;
		f32 FlashFxChanceSP;
		f32 FlashFxChanceMP;
		f32 FlashFxAltChance;
		f32 FlashFxScale;
		bool FlashFxLightEnabled;
		bool FlashFxLightCastsShadows;
		f32 FlashFxLightOffsetDist;
		Vector3 FlashFxLightRGBAMin;
		Vector3 FlashFxLightRGBAMax;
		Vector2 FlashFxLightIntensityMinMax;
		Vector2 FlashFxLightRangeMinMax;
		Vector2 FlashFxLightFalloffMinMax;
		bool GroundDisturbFxEnabled;
		f32 GroundDisturbFxDist;
		atHashWithStringNotFinal GroundDisturbFxNameDefault;
		atHashWithStringNotFinal GroundDisturbFxNameSand;
		atHashWithStringNotFinal GroundDisturbFxNameDirt;
		atHashWithStringNotFinal GroundDisturbFxNameWater;
		atHashWithStringNotFinal GroundDisturbFxNameFoliage;
		PAR_SIMPLE_PARSABLE;
	};

	// HitBoneForceOverride structure
	struct sHitBoneForceOverride
	{
		eAnimBoneTag BoneTag;
		float ForceFront;
		float ForceBack;
		PAR_SIMPLE_PARSABLE;
	};

	// Explosion structure
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

	struct sFrontClearTestParams
	{
		bool ShouldPerformFrontClearTest;
		float ForwardOffset;
		float VerticalOffset;
		float HorizontalOffset;
		float CapsuleRadius;
		float CapsuleLength;
		PAR_SIMPLE_PARSABLE;
	};

	struct sFirstPersonScopeAttachmentData
	{
		atHashWithStringNotFinal m_Name;
		float m_FirstPersonScopeAttachmentFov;
		Vector3 m_FirstPersonScopeAttachmentOffset;
		Vector3 m_FirstPersonScopeAttachmentRotationOffset;
		PAR_SIMPLE_PARSABLE;
	};

	typedef atArray<sFirstPersonScopeAttachmentData> FPScopeAttachData;

	// Constructor
	CWeaponInfo();

	//
	// Accessors
	//

	eDamageType GetDamageType() const { return m_DamageType; }
	eExplosionTag GetExplosionTag() const { return m_Explosion.Default; } 
	eExplosionTag GetExplosionTag(const CEntity* pHitEntity) const;
	eFireType GetFireType() const { return m_FireType; }
	eWeaponWheelSlot GetWeaponWheelSlot() const { return m_WheelSlot; }
	u32 GetGroup() const { return m_Group.GetHash(); }
	const CAmmoInfo* GetAmmoInfo() const { return m_AmmoInfo; }
	const CAmmoInfo* GetAmmoInfo(const CPed* pPed) const;
	const CAimingInfo* GetAimingInfo() const { return m_AimingInfo; }
	s32 GetClipSize() const { return m_ClipSize; }
	f32 GetAccuracySpread() const { return m_AccuracySpread; }
	f32 GetAccurateModeAccuracyModifier() const { return m_AccurateModeAccuracyModifier; }
	f32 GetRunAndGunAccuracyModifier() const { return m_RunAndGunAccuracyModifier; }
	f32 GetRunAndGunAccuracyMinOverride() const { return m_RunAndGunAccuracyMinOverride; }
	f32 GetRecoilAccuracyMax() const { return m_RecoilAccuracyMax; }
    f32 GetRecoilErrorTime() const { return m_RecoilErrorTime; }
	f32 GetRecoilRecoveryRate() const { return m_RecoilRecoveryRate; }
	f32 GetRecoilAccuracyToAllowHeadShotAI() const { return m_RecoilAccuracyToAllowHeadShotAI; }
	f32 GetMinHeadShotDistanceAI() const { return m_MinHeadShotDistanceAI; }
	f32 GetMaxHeadShotDistanceAI() const { return m_MaxHeadShotDistanceAI; }
	f32 GetHeadShotDamageModifierAI() const { return m_HeadShotDamageModifierAI; }
	f32 GetRecoilAccuracyToAllowHeadShotPlayer() const { return m_RecoilAccuracyToAllowHeadShotPlayer * m_RecoilAccuracyToAllowHeadShotPlayerModifier; }
	f32 GetMinHeadShotDistancePlayer() const { return m_MinHeadShotDistancePlayer * m_MinHeadShotDistancePlayerModifier; }
	f32 GetMaxHeadShotDistancePlayer() const { return m_MaxHeadShotDistancePlayer * m_MaxHeadShotDistancePlayerModifier; }
	f32 GetHeadShotDamageModifierPlayer() const { return m_HeadShotDamageModifierPlayer * m_HeadShotDamageModifierPlayerModifier; }
	f32 GetDamage() const { return m_Damage * m_WeaponDamageModifier; }
	f32 GetDamageTime() const { return m_DamageTime * m_EffectDurationModifier; }
	f32 GetDamageTimeInVehicle() const { return m_DamageTimeInVehicle * m_EffectDurationModifier; }
	f32 GetDamageTimeInVehicleHeadShot() const { return m_DamageTimeInVehicleHeadShot * m_EffectDurationModifier; }
	f32 GetEnduranceDamage() const { return m_EnduranceDamage * m_WeaponDamageModifier; }
	u32 GetPlayerDamageOverTimeWeapon() const { return m_PlayerDamageOverTimeWeapon.GetHash(); }
	f32 GetHitLimbsDamageModifier() const { return m_HitLimbsDamageModifier; }
	f32 GetNetworkHitLimbsDamageModifier() const { return m_NetworkHitLimbsDamageModifier; }
	f32 GetLightlyArmouredDamageModifier() const { return m_LightlyArmouredDamageModifier; }
	f32 GetVehicleDamageModifier() const { return m_VehicleDamageModifier; }
	f32 GetForce() const { return m_Force; }
	f32 GetForceHitPed() const { return m_ForceHitPed; }
	f32 GetForceHitPed(eAnimBoneTag boneTagHit, bool front, float distance) const;
	f32 GetForceHitVehicle() const { return m_ForceHitVehicle; }
	f32 GetForceHitFlyingHeli() const { return m_ForceHitFlyingHeli; }
	f32 GetForceMaxStrengthMult() const { return m_ForceMaxStrengthMult; }
	f32 GetForceDistanceModifier(float fDistance) const;
	f32 GetProjectileForce() const { return m_ProjectileForce; }
	f32 GetFragImpulse() const { return m_FragImpulse; }
	f32 GetPenetration() const { return m_Penetration; }
	f32 GetSpeed() const { return m_Speed; }
	f32 GetVerticalLaunchAdjustment() const { return m_VerticalLaunchAdjustment; }
	f32 GetDropForwardVelocity() const { return m_DropForwardVelocity; }
	u32 GetBulletsInBatch() const { return m_BulletsInBatch; }
	f32 GetBatchSpread() const { return m_BatchSpread; }
	f32 GetReloadTime() const;
	f32 GetVehicleReloadTime() const { return m_VehicleReloadTime; }
	f32 GetAnimReloadRate() const { return m_AnimReloadRate; }
	s32 GetBulletsPerAnimLoop() const { return m_BulletsPerAnimLoop; }
	f32 GetTimeBetweenShots() const { return m_TimeBetweenShots; }
	f32 GetTimeLeftBetweenShotsWhereShouldFireIsCached() const { return m_TimeLeftBetweenShotsWhereShouldFireIsCached; }
	f32 GetSpinUpTime() const { return m_SpinUpTime; }
	f32 GetSpinTime() const { return m_SpinTime; }
	f32 GetSpinDownTime() const { return m_SpinDownTime; }
	f32 GetAlternateWaitTime() const { return m_AlternateWaitTime; }
	f32 GetBulletBendingNearRadius() const { return m_BulletBendingNearRadius; }
	f32 GetBulletBendingFarRadius() const { return m_BulletBendingFarRadius; }
	f32 GetBulletBendingZoomedRadius() const { return m_BulletBendingZoomedRadius; }
	f32 GetFirstPersonBulletBendingNearRadius() const { return m_FirstPersonBulletBendingNearRadius; }
	f32 GetFirstPersonBulletBendingFarRadius() const { return m_FirstPersonBulletBendingFarRadius; }
	f32 GetFirstPersonBulletBendingZoomedRadius() const { return m_FirstPersonBulletBendingZoomedRadius; }
	s32 GetInitialRumbleDuration() const { return m_InitialRumbleDuration; }
	f32 GetInitialRumbleIntensity() const { return m_InitialRumbleIntensity; }
	f32 GetInitialRumbleIntensityTrigger() const { return m_InitialRumbleIntensityTrigger; }
	s32 GetRumbleDuration() const { return m_RumbleDuration; }
	f32 GetRumbleIntensity() const { return m_RumbleIntensity; }
	f32 GetRumbleIntensityTrigger() const { return m_RumbleIntensityTrigger; }
	f32 GetRumbleDamageIntensity() const { return m_RumbleDamageIntensity; }	
	s32 GetFirstPersonInitialRumbleDuration() const { return m_InitialRumbleDurationFps; }
	f32 GetFirstPersonInitialRumbleIntensity() const { return m_InitialRumbleIntensityFps; }
	s32 GetFirstPersonRumbleDuration() const { return m_RumbleDurationFps; }
	f32 GetFirstPersonRumbleIntensity() const { return m_RumbleIntensityFps; }
	f32 GetNetworkPlayerDamageModifier() const { return m_NetworkPlayerDamageModifier; }
	f32 GetNetworkPedDamageModifier() const { return m_NetworkPedDamageModifier; }
	f32 GetLockOnRange() const { return m_LockOnRange; }
	f32 GetRange() const { return m_WeaponRange; }
	f32 GetAiSoundRange() const { return m_AiSoundRange; }
	f32 GetPotentialBlastEventRange() const { return m_AiPotentialBlastEventRange; }
	
	// Damage fall off
	f32 GetDamageFallOffRangeMin() const { return m_DamageFallOffRangeMin * m_DamageFallOffRangeModifier; }
	f32 GetDamageFallOffRangeMax() const { return m_DamageFallOffRangeMax * m_DamageFallOffRangeModifier; }
	f32 GetDamageFallOffRangeModifier() const { return m_DamageFallOffRangeModifier; }
	void SetDamageFallOffRangeModifier(f32 newModifier) { m_DamageFallOffRangeModifier = newModifier; }
	f32 GetDamageFallOffTransientModifier() const { return m_DamageFallOffTransientModifier; }
	void SetDamageFallOffTransientModifier(f32 newModifier) { m_DamageFallOffTransientModifier = newModifier; }
	f32 GetDamageFallOffModifier() const { return m_DamageFallOffModifier * m_DamageFallOffTransientModifier; }

	f32 GetAoEModifier() const { return m_AoEModifier; }
	void SetAoEModifier(f32 newModifier) { m_AoEModifier = newModifier; }

	f32 GetEffectDurationModifier() const { return m_EffectDurationModifier; }
	void SetEffectDurationModifier(f32 newModifier) { m_EffectDurationModifier = newModifier; }

	u32 GetDefaultCameraHash() const { return m_DefaultCameraHash.GetHash(); }
	u32 GetAimCameraHash() const { return m_AimCameraHash.GetHash(); }
	u32 GetFireCameraHash() const { return m_FireCameraHash.GetHash(); }
	u32 GetCoverCameraHash() const { return m_CoverCameraHash.GetHash(); }
	u32 GetCoverReadyToFireCameraHash() const { return m_CoverReadyToFireCameraHash.GetHash(); }
	u32 GetRunAndGunCameraHash() const { return m_RunAndGunCameraHash.GetHash(); }
	u32 GetCinematicShootingCameraHash() const { return m_CinematicShootingCameraHash.GetHash(); }
	u32 GetAlternativeOrScopedCameraHash() const { return m_AlternativeOrScopedCameraHash.GetHash(); }
	u32 GetRunAndGunAlternativeOrScopedCameraHash() const { return m_RunAndGunAlternativeOrScopedCameraHash.GetHash(); }
	u32 GetCinematicShootingAlternativeOrScopedCameraHash() const { return m_CinematicShootingAlternativeOrScopedCameraHash.GetHash(); }
	u32 GetPovTurretCameraHash() const { return m_PovTurretCameraHash.GetHash(); }
	u32 GetRecoilShakeHash() const { return m_RecoilShakeHash.GetHash(); }
	u32 GetFirstPersonRecoilShakeHash() const { return m_RecoilShakeHashFirstPerson.GetHash(); }
	u32 GetAccuracyOffsetShakeHash() const { return m_AccuracyOffsetShakeHash.GetHash(); }
	u32 GetMinTimeBetweenRecoilShakes() const { return m_MinTimeBetweenRecoilShakes; }
	f32 GetRecoilShakeAmplitude() const { return m_RecoilShakeAmplitude; }
	f32 GetExplosionShakeAmplitude() const { return m_ExplosionShakeAmplitude; }
	f32 GetCameraFov() const { return m_CameraFov; }
	f32 GetFirstPersonAimFovMin() const { return m_FirstPersonAimFovMin; }
	f32 GetFirstPersonAimFovMax() const { return m_FirstPersonAimFovMax; }
	f32 GetFirstPersonScopeFov() const { return m_FirstPersonScopeFov; }
	f32 GetFirstPersonScopeAttachmentFov() const { return m_FirstPersonScopeAttachmentFov; }
	const Vector3& GetFirstPersonDrivebyIKOffset() const { return m_FirstPersonDrivebyIKOffset; }
	const Vector3& GetFirstPersonRNGOffset() const { return m_FirstPersonRNGOffset; }
	const Vector3& GetFirstPersonRNGRotationOffset() const { return m_FirstPersonRNGRotationOffset; }
	const Vector3& GetFirstPersonLTOffset() const { return m_FirstPersonLTOffset; }
	const Vector3& GetFirstPersonLTRotationOffset() const { return m_FirstPersonLTRotationOffset; }
	const Vector3& GetFirstPersonScopeOffset() const { return m_FirstPersonScopeOffset; }
	const Vector3& GetFirstPersonScopeAttachmentOffset() const { return m_FirstPersonScopeAttachmentOffset; }
	const Vector3& GetFirstPersonScopeRotationOffset() const { return m_FirstPersonScopeRotationOffset; }
	const Vector3& GetFirstPersonScopeAttachmentRotationOffset() const { return m_FirstPersonScopeAttachmentRotationOffset; }
	const Vector3& GetFirstPersonAsThirdPersonIdleOffset() const { return m_FirstPersonAsThirdPersonIdleOffset; }
	const Vector3& GetFirstPersonAsThirdPersonRNGOffset() const { return m_FirstPersonAsThirdPersonRNGOffset; }
	const Vector3& GetFirstPersonAsThirdPersonLTOffset() const { return m_FirstPersonAsThirdPersonLTOffset; }
	const Vector3& GetFirstPersonAsThirdPersonScopeOffset() const { return m_FirstPersonAsThirdPersonScopeOffset; }
	const Vector3& GetFirstPersonAsThirdPersonWeaponBlockedOffset() const { return m_FirstPersonAsThirdPersonWeaponBlockedOffset; }
	f32 GetFirstPersonDofSubjectMagnificationPowerFactorNear() const { return m_FirstPersonDofSubjectMagnificationPowerFactorNear; }
	f32 GetFirstPersonDofMaxNearInFocusDistance() const { return m_FirstPersonDofMaxNearInFocusDistance; }
	f32 GetFirstPersonDofMaxNearInFocusDistanceBlendLevel() const { return m_FirstPersonDofMaxNearInFocusDistanceBlendLevel; }
	const FPScopeAttachData& GetFirstPersonScopeAttachmentData() const { return m_FirstPersonScopeAttachmentData; }
	f32 GetZoomFactorForAccurateMode() const { return m_ZoomFactorForAccurateMode; }

	s8 GetHudDamage() const { return m_HudDamage; }
	s8 GetHudSpeed() const { return m_HudSpeed; }
	s8 GetHudCapacity() const { return m_HudCapacity; }
	s8 GetHudAccuracy() const { return m_HudAccuracy; }
	s8 GetHudRange() const { return m_HudRange;}

	Vector3 GetAimOffset( float fPitchRatio = 0.5f, CPedMotionData::eFPSState fpsState = CPedMotionData::FPS_MAX ) const;
	const Vector3& GetAimOffsetMin() const { return m_AimOffsetMin; }
	const Vector3& GetAimOffsetMax() const { return m_AimOffsetMax; }
	float GetAimProbeLengthMin() const { return m_AimProbeLengthMin; }
	float GetAimProbeLengthMax() const { return m_AimProbeLengthMax; }
	void GetTorsoAimOffsetForPed(Vector3& vAimOffset, const CPed& ped) const;
	const Vector2& GetTorsoAimOffset() const { return m_TorsoAimOffset; }
	const Vector2& GetTorsoCrouchedAimOffset() const { return m_TorsoCrouchedAimOffset; }
	const Vector3& GetLeftHandIkOffset() const { return m_LeftHandIkOffset; }
	// FPS aim offsets
	Vector3 GetAimOffsetEndPosFPSIdle( float fPitchRatio = 0.5f ) const;
	Vector3 GetAimOffsetEndPosFPSLT( float fPitchRatio = 0.5f ) const;
	const Vector3& GetAimOffsetMinFPSIdle() const { return m_AimOffsetMinFPSIdle; }
	const Vector3& GetAimOffsetMedFPSIdle() const { return m_AimOffsetMedFPSIdle; }
	const Vector3& GetAimOffsetMaxFPSIdle() const { return m_AimOffsetMaxFPSIdle; }
	const Vector3& GetAimOffsetMinFPSLT() const { return m_AimOffsetMinFPSLT; }
	const Vector3& GetAimOffsetMaxFPSLT() const { return m_AimOffsetMaxFPSLT; }
	const Vector3& GetAimOffsetMinFPSRNG() const { return m_AimOffsetMinFPSRNG; }
	const Vector3& GetAimOffsetMaxFPSRNG() const { return m_AimOffsetMaxFPSRNG; }
	const Vector3& GetAimOffsetMinFPSScope() const { return m_AimOffsetMinFPSScope; }
	const Vector3& GetAimOffsetMaxFPSScope() const { return m_AimOffsetMaxFPSScope; }
	const Vector3& GetAimOffsetEndPosMinFPSIdle() const { return m_AimOffsetEndPosMinFPSIdle; }
	const Vector3& GetAimOffsetEndPosMedFPSIdle() const { return m_AimOffsetEndPosMedFPSIdle; }
	const Vector3& GetAimOffsetEndPosMaxFPSIdle() const { return m_AimOffsetEndPosMaxFPSIdle; }
	const Vector3& GetAimOffsetEndPosMinFPSLT() const { return m_AimOffsetEndPosMinFPSLT; }
	const Vector3& GetAimOffsetEndPosMedFPSLT() const { return m_AimOffsetEndPosMedFPSLT; }
	const Vector3& GetAimOffsetEndPosMaxFPSLT() const { return m_AimOffsetEndPosMaxFPSLT; }
	float GetAimProbeRadiusOverrideFPSIdle() const { return m_AimProbeRadiusOverrideFPSIdle; }
	float GetAimProbeRadiusOverrideFPSIdleStealth() const { return m_AimProbeRadiusOverrideFPSIdleStealth; }
	float GetAimProbeRadiusOverrideFPSLT() const { return m_AimProbeRadiusOverrideFPSLT; }
	float GetAimProbeRadiusOverrideFPSRNG() const { return m_AimProbeRadiusOverrideFPSRNG; }
	float GetAimProbeRadiusOverrideFPSScope() const { return m_AimProbeRadiusOverrideFPSScope; }
	float GetAimProbeRadiusOverride(CPedMotionData::eFPSState fpsState = CPedMotionData::FPS_MAX, bool bUseFPSStealth = false ) const;

	float GetIkRecoilDisplacement() const { return m_IkRecoilDisplacement; }
	float GetIkRecoilDisplacementScope() const { return m_IkRecoilDisplacementScope; }
	float GetIkRecoilDisplacementScaleBackward() const { return m_IkRecoilDisplacementScaleBackward; }
	float GetIkRecoilDisplacementScaleVertical() const { return m_IkRecoilDisplacementScaleVertical; }

	const Vector2& GetReticuleHudPosition() const { return m_ReticuleHudPosition; }
	const Vector2& GetReticuleHudPositionOffsetForPOVTurret() const { return m_ReticuleHudPositionOffsetForPOVTurret; }
	f32 GetReticuleStanding() const { return m_ReticuleMinSizeStanding; }
	f32 GetReticuleCrouched() const { return m_ReticuleMinSizeCrouched; }
	f32 GetReticuleScale() const { return m_ReticuleScale; }
	u32 GetReticuleStyleHash() const { return m_ReticuleStyleHash.GetHash(); }
	u32 GetFirstPersonReticuleStyleHash() const { return m_FirstPersonReticuleStyleHash.GetHash(); }
	u32 GetPickupHash() const { return m_PickupHash.GetHash(); }
	u32 GetAudioCollisionHash() const { return m_AudioCollisionHash.GetHash(); }
	u32 GetMPPickupHash() const { return m_MPPickupHash.GetHash(); }
	u32 GetHumanNameHash() const { return m_HumanNameHash.GetHash(); }
	u32 GetMovementModeConditionalIdleHash() const { return m_MovementModeConditionalIdle.GetHash(); }
	u32 GetFiringPatternAlias(u32 uFiringPattern) const;
	const CWeaponUpperBodyFixupExpressionData* GetReloadUpperBodyFixupExpressionData() const { return m_ReloadUpperBodyFixupExpressionData; }
	u8 GetAmmoDiminishingRate() const { return m_AmmoDiminishingRate; }
	float GetAimingBreathingAdditiveWeight() const { return m_AimingBreathingAdditiveWeight; }
	float GetFiringBreathingAdditiveWeight() const { return m_FiringBreathingAdditiveWeight; }
	float GetStealthAimingBreathingAdditiveWeight() const { return m_StealthAimingBreathingAdditiveWeight; }
	float GetStealthFiringBreathingAdditiveWeight() const { return m_StealthFiringBreathingAdditiveWeight; }
	float GetAimingLeanAdditiveWeight() const { return m_AimingLeanAdditiveWeight; }
	float GetFiringLeanAdditiveWeight() const { return m_FiringLeanAdditiveWeight; }
	float GetStealthAimingLeanAdditiveWeight() const { return m_StealthAimingLeanAdditiveWeight; }
	float GetStealthFiringLeanAdditiveWeight() const { return m_StealthFiringLeanAdditiveWeight; }
	float GetBulletDirectionOffset() const { return m_BulletDirectionOffsetInDegrees; }
	float GetBulletDirectionPitchOffset() const { return m_BulletDirectionPitchOffset; }
	float GetBulletDirectionPitchHomingOffset() const { return m_BulletDirectionPitchHomingOffset; }
	float GetExpandPedCapsuleRadius() const { return m_ExpandPedCapsuleRadius; }
	float GetVehicleAttackAngle() const { return m_VehicleAttackAngle; }
	float GetTorsoIKAngleLimit() const { return m_TorsoIKAngleLimit; }
	float GetMeleeDamageMultiplier() const { return m_MeleeDamageMultiplier; }
	float GetMeleeRightFistTargetHealthDamageScaler() const { return m_MeleeRightFistTargetHealthDamageScaler; }
	float GetAirborneAircraftLockOnMultiplier() const { return m_AirborneAircraftLockOnMultiplier; }
	float GetArmouredVehicleGlassDamageOverride() const { return m_ArmouredVehicleGlassDamageOverride; }

	// Damage modifiers
	float GetWeaponDamageModifier() const { return m_WeaponDamageModifier; }
	void SetWeaponDamageModifier(float newModifier) { m_WeaponDamageModifier = newModifier; }
	float GetRecoilAccuracyToAllowHeadShotPlayerModifier() const { return m_RecoilAccuracyToAllowHeadShotPlayerModifier; }
	void SetRecoilAccuracyToAllowHeadShotPlayerModifier(float newModifier) { m_RecoilAccuracyToAllowHeadShotPlayerModifier = newModifier; }
	float GetMinHeadShotDistancePlayerModifier() const { return m_MinHeadShotDistancePlayerModifier; }
	void SetMinHeadShotDistancePlayerModifier(float newModifier)  { m_MinHeadShotDistancePlayerModifier = newModifier; }
	float GetMaxHeadShotDistancePlayerModifier() const { return m_MaxHeadShotDistancePlayerModifier; }
	void SetMaxHeadShotDistancePlayerModifier(float newModifier) { m_MaxHeadShotDistancePlayerModifier = newModifier; }
	float GetHeadShotDamageModifierPlayerModifier() const { return m_HeadShotDamageModifierPlayerModifier; }
	void SetHeadShotDamageModifierPlayerModifier(float newModifier) { m_HeadShotDamageModifierPlayerModifier = newModifier; }

	// Flags
	bool GetAllowEarlyExitFromFireAnimAfterBulletFired() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowEarlyExitFromFireAnimAfterBulletFired); }
	bool GetIsHoming() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Homing); }
	bool GetCanCrouchFire() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AnimCrouchFire); }
	bool GetCanFreeAim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanFreeAim); }
	bool GetCanLockOnOnFoot() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanLockonOnFoot); }
	bool GetCanLockOnInVehicle() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanLockonInVehicle); }
	bool GetCanReload() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AnimReload); }
	bool GetCreateVisibleOrdnance() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CreateVisibleOrdnance); }
	bool GetDoesApplyBulletForce() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ApplyBulletForce); }
	bool GetHasFirstPersonScope() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::FirstPersonScope); }
	bool GetIs2HandedInCover() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::TreatAsTwoHandedInCover); }
	bool GetIs1HandedInCover() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::TreatAsOneHandedInCover); }
	bool GetIsArmourPenetrating() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ArmourPenetrating); }
	bool GetIsAutomatic() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Automatic); }
	bool GetIsCarriedInHand() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CarriedInHand); }
	bool GetCookWhileAiming() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CookWhileAiming); }
	bool GetDropWhenCooked() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DropWhenCooked); }	
	bool GetIsSilenced() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Silenced); }
    bool GetDoesRevivableDamage() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DoesRevivableDamage); }
    bool GetNoFriendlyFireDamage() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NoFriendlyFireDamage); }
    bool GetDisplayRechargeTimeHUD() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisplayRechargeTimeHUD); }
    bool GetOnlyFireOneShot() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnlyFireOneShot); }
	bool GetOnlyFireOneShotPerTriggerPress() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnlyFireOneShotPerTriggerPress); }
    bool GetUseLegDamageVoice() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseLegDamageVoice); }
    bool GetNoLeftHandIK() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NoLeftHandIK); }
	bool GetNoLeftHandIKWhenBlocked() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NoLeftHandIKWhenBlocked); }
	bool GetShouldEnforceAimingRestrictions() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::EnforceAimingRestrictions); }
	bool GetForceEjectShellAfterFiring() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ForceEjectShellAfterFiring); }
	bool GetCanPerformArrest() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanPerformArrest); }
	bool GetThrowOnly() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ThrowOnly); }	
	bool GetNoAutoRunWhenFiring() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NoAutoRunWhenFiring); }	
	bool GetDontBlendFireOutro() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DontBlendFireOutro); }	
	bool GetDelayedFiringAfterAutoSwap() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DelayedFiringAfterAutoSwap); }	
	bool GetDelayedFiringAfterAutoSwapPreviousWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DelayedFiringAfterAutoSwapPreviousWeapon); }	
	bool GetIsUnarmed() const { return GetModelHash() == 0 && !GetNotUnarmed(); }
	bool GetIsArmed() const { return !GetIsUnarmed(); }
	bool GetIsTwoHanded() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::TwoHanded); }
	bool GetIsMelee() const { return GetDamageType() == DAMAGE_TYPE_MELEE; }
	bool GetIsMelee2Handed() const { return GetIsMelee() && GetIsTwoHanded(); }
	bool GetIsLaunched() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Launched); }
	bool GetIsBlade() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeBlade); }
	bool GetIsClub() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeClub); }
	bool GetIsGun() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Gun); }
	bool GetIsGunOrCanBeFiredLikeGun() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Gun) || m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanBeFiredLikeGun); }
	bool GetIsGun1Handed() const { return GetIsGun() && !GetIsTwoHanded(); }
	bool GetIsGun2Handed() const { return GetIsGun() && GetIsTwoHanded(); }
	bool GetIsProjectile() const { return GetFireType() == FIRE_TYPE_PROJECTILE; }
	bool GetIsThrownWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Thrown); }
	bool GetIsHeavy() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Heavy); }
	bool GetIsBomb() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Bomb); }
	bool GetIsDetonator() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Detonator); }
#if LASSO_WEAPON_EXISTS
	bool GetIsLasso() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Lasso); }
#endif
	bool GetIsRiotShield() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::RiotShield); }
	bool GetIgnoreStrafing() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IgnoreStrafing); }
	bool GetIsVehicleWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Vehicle); }
	bool GetUsableOnFoot() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsableOnFoot); }
	bool GetUsableUnderwater() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsableUnderwater); }
	bool GetUsableClimbing() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsableClimbing); }
	bool GetUsableInCover() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsableInCover); }
	bool GetUseRightHandIk() const { return !m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableRightHandIk); }
	bool GetUseLeftHandIkInCover() const { return !m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableLeftHandIkInCover); }
	bool GetDontSwapWeaponIfNoAmmo() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DontSwapWeaponIfNoAmmo); }
	bool GetUseLoopedReloadAnim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseLoopedReloadAnim); }
	bool GetOnlyAllowFiring() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnlyAllowFiring); }
	bool GetIsNonViolent() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NonViolent); }
	bool GetIsNonLethal() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NonLethal); }
	bool GetIsScary() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Scary); }
	bool GetAllowCloseQuarterKills() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowCloseQuarterKills); }
	bool GetDisablePlayerBlockingInMP() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisablePlayerBlockingInMP); }
	bool GetIsStaticReticulePosition() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::StaticReticulePosition); }
	bool GetDiscardWhenSwapped() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DiscardWhenSwapped); }
	bool GetAllowMeleeIntroAnim() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowMeleeIntroAnim); }
	bool GetIsManualDetonation() const {return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ManualDetonation); }
	bool GetSuppressGunshotEvent() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::SuppressGunshotEvent); }
	bool GetHiddenFromWeaponWheel() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HiddenFromWeaponWheel); }
	bool GetAllowDriverLockOnToAmbientPeds() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowDriverLockOnToAmbientPeds); } 
	bool GetNeedsGunCockingInCover() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NeedsGunCockingInCover); }
	bool GetDisableIdleVariations() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableIdleVariations); }
	bool GetHasLowCoverReloads() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HasLowCoverReloads); }
	bool GetHasLowCoverSwaps() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HasLowCoverSwaps); }
	bool GetDontBreakRopes() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DontBreakRopes); }
	bool GetUseLeftHandIkWhenAiming() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseLeftHandIkWhenAiming); }
	bool GetIsNotAWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NotAWeapon); }
	bool GetRemoveEarlyWhenEnteringVehicles() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::RemoveEarlyWhenEnteringVehicles); }
	bool GetDiscardWhenOutOfAmmo() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DiscardWhenOutOfAmmo); }
	bool GetEnforceFiringAngularThreshold() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::EnforceFiringAngularThreshold); }
	bool GetForcesActionMode() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ForcesActionMode); }
	bool CreatesAPotentialExplosionEventWhenFired() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CreatesAPotentialExplosionEventWhenFired); }
	bool GetCreateBulletExplosionWhenOutOfTime() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CreateBulletExplosionWhenOutOfTime); }
	bool GetDisableCombatRoll() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableCombatRoll); }
	bool GetDisableStealth() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableStealth); }
	bool GetIsDangerousLookingMeleeWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DangerousLookingMeleeWeapon); }
	bool GetIsRpg() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Rpg); }
	bool GetTorsoIKForWeaponBlock() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::TorsoIKForWeaponBlock); }
	bool GetIsLongWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::LongWeapon); }
	bool GetUsableUnderwaterOnly() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsableUnderwaterOnly); }
	bool GetIsUnderwaterGun() const { return GetIsGun() && GetUsableUnderwater(); }
	bool GetIsMeleeKungFu() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeKungFu); }
	bool GetForcingDriveByAssistedAim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ForceDriveByAssistedAim); }
	bool IsAssistedAimVehicleWeapon() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AssistedAimVehicleWeapon); }
	bool CanBlowUpVehicleAtZeroBodyHealth() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanBlowUpVehicleAtZeroBodyHealth); }
	bool GetIgnoreAnimReloadRateModifiers() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IgnoreAnimReloadRateModifiers); }
	bool GetIsIdleAnimationFilterDisabled() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableIdleAnimationFilter); }
	bool GetIsIdleAnimationFilterDisabledWhenReloading() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableIdleAnimationFilterWhenReloading); }
	bool GetPenetrateVehicles() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::PenetrateVehicles); }
	bool GetScalesDamageBasedOnMaxVehicleHealth() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ScalesDamageBasedOnMaxVehicleHealth); }
	bool GetCanHomingToggle() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HomingToggle); }
	bool GetApplyVehicleDamageToEngine() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ApplyVehicleDamageToEngine); }
	bool GetIsTurret() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::Turret); }
	bool GetDisableAimAngleChecksForReticule() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableAimAngleChecksForReticule); }
	bool GetAllowMovementDuringFirstPersonScope() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowMovementDuringFirstPersonScope); }
	bool GetIsDriveByMPOnly() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DriveByMPOnly); }
	bool GetCreateWeaponWithNoModel() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CreateWeaponWithNoModel); }
	bool GetRemoveWhenUnequipped() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::RemoveWhenUnequipped); }
	bool GetBlockAmbientIdles() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::BlockAmbientIdles); }
	bool GetNotUnarmed() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NotUnarmed); }
	bool GetUseFPSAimIK() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseFPSAimIK); }
	bool GetDisableFPSScope() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableFPSScope); }
	bool GetDisableFPSAimForScope() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableFPSAimForScope); }
	bool GetEnableFPSRNGOnly() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::EnableFPSRNGOnly); }
	bool GetEnableFPSIdleOnly() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::EnableFPSIdleOnly); }
	bool GetIsHatchet() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeHatchet); }
	bool GetUseAlternateFPDrivebyClipset() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseAlternateFPDrivebyClipset); }
	bool GetAttachFPSLeftHandIKToRight() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AttachFPSLeftHandIKToRight); }
	bool GetOnlyUseAimingInfoInFPS() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnlyUseAimingInfoInFPS); }
	bool GetUseFPSAnimatedRecoil() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseFPSAnimatedRecoil); }
	bool GetUseFPSSecondaryMotion() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseFPSSecondaryMotion); }
	bool GetHasFPSProjectileWeaponAnims() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HasFPSProjectileWeaponAnims); }
	bool GetAllowMeleeBlock() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowMeleeBlock); }
	bool GetDontPlayDryFireAnim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DontPlayDryFireAnim); }
	bool GetSwapToUnarmedWhenOutOfThrownAmmo() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::SwapToUnarmedWhenOutOfThrownAmmo); }
	bool GetPlayOutOfAmmoAnim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::PlayOutOfAmmoAnim); }
	bool GetIsOnFootHoming() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnFootHoming); }
	bool GetDamageCausesDisputes() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DamageCausesDisputes); }
	bool GetUsePlaneExplosionDamageCapInMP() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UsePlaneExplosionDamageCapInMP); }
	bool GetFPSOnlyExitFireAnimAfterRecoilEnds() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::FPSOnlyExitFireAnimAfterRecoilEnds); }
	bool GetShouldSkipVehiclePetrolTankDamage() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::SkipVehiclePetrolTankDamage); }
	bool GetDisableAutoSwapOnPickUp() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DontAutoSwapOnPickUp); }
	bool GetShouldDisableTorsoIKAboveAngleThreshold() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableTorsoIKAboveAngleThreshold); }
	bool GetIsMeleeFist() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeFist); }
	bool GetNotAllowedForDriveby() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::NotAllowedForDriveby); }
	bool GetAttachReloadObjectToRightHand() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AttachReloadObjectToRightHand); }
	bool GetCanBeAimedLikeGunWithoutFiring() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanBeAimedLikeGunWithoutFiring); }
	bool GetIsMachete() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::MeleeMachete); }
	bool GetHideReticule() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HideReticule); }
	bool GetUseHolsterAnimation() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseHolsterAnimation); }
	bool GetBlockFirstPersonStateTransitionWhileFiring() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::BlockFirstPersonStateTransitionWhileFiring); }
	bool GetForceFullFireAnimation() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ForceFullFireAnimation); }
	bool GetDisableLeftHandIkInDriveby() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableLeftHandIkInDriveby); }
	bool GetCanBeUsedInVehicleMelee() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::CanUseInVehMelee); }
	bool GetUseVehicleWeaponBoneForward() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseVehicleWeaponBoneForward); }
	bool GetUseManualTargetingMode() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseManualTargetingMode); }
	bool GetIgnoreTracerVfxMuzzleDirectionCheck() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IgnoreTracerVfxMuzzleDirectionCheck); }
	bool GetIgnoreHomingCloseThresholdCheck() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IgnoreHomingCloseThresholdCheck); }
	bool GetLockOnRequiresAim() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::LockOnRequiresAim); }
	bool GetDisableCameraPullAround() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DisableCameraPullAround); }
	bool GetIsVehicleChargedLaunch() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::VehicleChargedLaunch); }
	bool GetForcePedAsFiringEntity() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ForcePedAsFiringEntity); }
	bool GetFiringEntityIgnoresExplosionDamage() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::FiringEntityIgnoresExplosionDamage); }
	bool GetIdlePhaseBasedOnTrigger() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IdlePhaseBasedOnTrigger); }
	bool GetUsesHighSpinRate() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::HighSpinRate); }
	bool GetShouldBeEnabledOnlyWhenVehTransformed() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::EnabledOnlyWhenVehTransformed); }
	bool GetIncendiaryGuaranteedChance() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::IncendiaryGuaranteedChance); }
	bool GetUseCameraHeadingForHomingTargetCheck() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseCameraHeadingForHomingTargetCheck); }
	bool GetUseWeaponRangeForTargetProbe() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::UseWeaponRangeForTargetProbe); }
	bool GetSkipProjectileLOSCheck() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::SkipProjectileLOSCheck); }
	bool GetPairedWeaponHolsterAnimation() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::PairedWeaponHolsterAnimation); }
	bool GetBlockRagdollResponseTaskForPlayer() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::BlockRagdollResponseTaskForPlayer); }
	bool GetFireAnimRateScaledToTimeBetweenShots() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::FireAnimRateScaledToTimeBetweenShots); }
	bool GetDamageOverTimeShockedEffect() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DamageOverTimeShockedEffect); }
	bool GetDamageOverTimeChokingEffect() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DamageOverTimeChokingEffect); }
	bool GetDamageOverTimeAllowStacking() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::DamageOverTimeAllowStacking); }
	bool GetBoatWeaponIsNotSearchLight() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::BoatWeaponIsNotSearchLight); }
	bool GetAllowFireInterruptWhenReady() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::AllowFireInterruptWhenReady); }
	bool GetResponsiveRecoilRecovery() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::ResponsiveRecoilRecovery); }
	bool GetOnlyLockOnToAquaticVehicles() const { return m_WeaponFlags.BitSet().IsSet(CWeaponInfoFlags::OnlyLockOnToAquaticVehicles); }

	bool GetIsSwitchblade() const { return GetHash() == ATSTRINGHASH("WEAPON_SWITCHBLADE", 0xdfe37640); }

	bool GetFlag(CWeaponInfoFlags::Flags thisFlag) const { return m_WeaponFlags.BitSet().IsSet(thisFlag);}
	//
	// Animation
	//

	bool		  ShouldPedUseAlternateCoverClipSet(const CPed& ped) const;
	fwMvClipSetId GetCoverMovementClipSetHashForPed(const CPed& ped, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId GetAppropriateWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetAppropriateReloadWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetAppropriateSwapWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetAppropriateFromStrafeWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId	GetAimTurnCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetAimTurnStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetAlternateAimingCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetAlternateAimingStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetFiringVariationsCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetFiringVariationsStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetInjuredWeaponClipSetId() const;
	fwMvClipSetId	GetInjuredWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetStealthWeaponClipSetId() const;
	fwMvClipSetId	GetStealthWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetHiCoverWeaponClipSetId() const;
	fwMvClipSetId	GetHiCoverWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;	
	fwMvClipSetId	GetMeleeBaseClipSetId() const;
	fwMvClipSetId	GetMeleeBaseClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetMeleeClipSetId() const;
	fwMvClipSetId	GetMeleeClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetMeleeSupportTauntClipSetId() const;
	fwMvClipSetId	GetMeleeSupportTauntClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetMeleeStealthClipSetId() const;
	fwMvClipSetId	GetMeleeStealthClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetMeleeTauntClipSetId() const;
	fwMvClipSetId	GetMeleeTauntClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetMeleeVariationClipSetId() const;
	fwMvClipSetId	GetMeleeVariationClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetJumpUpperbodyClipSetId() const;
	fwMvClipSetId	GetJumpUpperbodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetFallUpperbodyClipSetId() const;
	fwMvClipSetId	GetFallUpperbodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetFromStrafeTransitionUpperBodyClipSetId() const;
	fwMvClipSetId	GetFromStrafeTransitionUpperBodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedCoverAlternateMovementClipSetId() const;
	fwMvClipSetId	GetPedCoverAlternateMovementClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedCoverMovementClipSetId() const;
	fwMvClipSetId	GetPedCoverMovementClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedCoverMovementExtraClipSetId() const;
	fwMvClipSetId	GetPedCoverMovementExtraClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedCoverWeaponClipSetId() const;
	fwMvClipSetId	GetPedCoverWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedMotionClipSetId() const;
	fwMvClipSetId	GetPedMotionClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedMotionCrouchClipSetId() const;
	fwMvClipSetId	GetPedMotionCrouchClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvFilterId	GetPedMotionFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedMotionStrafingClipSetId() const;
	fwMvClipSetId	GetPedMotionStrafingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedMotionStrafingStealthClipSetId() const;
	fwMvClipSetId	GetPedMotionStrafingStealthClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetPedMotionStrafingUpperBodyClipSetId() const;
	fwMvClipSetId	GetPedMotionStrafingUpperBodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetShellShockedClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetWeaponClipSetId() const;
	fwMvClipSetId	GetWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetWeaponClipSetStreamedId() const;
	fwMvClipSetId	GetWeaponClipSetStreamedId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetAlternativeClipSetWhenBlockedId() const;
	fwMvClipSetId	GetAlternativeClipSetWhenBlockedId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetScopeWeaponClipSetId() const;
	fwMvClipSetId	GetScopeWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvFilterId	GetSwapWeaponFilterId() const;
	fwMvFilterId	GetSwapWeaponFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvFilterId	GetSwapWeaponInLowCoverFilterId() const;
	fwMvFilterId	GetSwapWeaponInLowCoverFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	const CWeaponSwapData* GetSwapWeaponData() const;
	const CWeaponSwapData* GetSwapWeaponData(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetSwapWeaponClipSetId() const;
	fwMvClipSetId	GetSwapWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetAimGrenadeThrowClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId	GetGestureBeckonOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetGestureOverThereOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetGestureHaltOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetGestureGlancesOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId	GetCombatReactionOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId GetAppropriateFPSTransitionClipsetId(const CPed &rPed,  CPedMotionData::eFPSState fpsPrevStateOverride = CPedMotionData::FPS_MAX, FPSStreamingFlags streamFromFPSSet = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromIdleClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromRNGClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromLTClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromScopeClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromUnholsterClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionFromStealthClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionToStealthClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSTransitionToStealthFromUnholsterClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId GetAppropriateFPSFidgetClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetFPSFidgetClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault, int iIndex = 0) const;

	fwMvClipSetId GetMovementClipSetOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	fwMvClipSetId GetWeaponClipSetIdForClone(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	fwMvClipSetId GetPedMotionClipSetIdForClone(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	float GetBlindFireRateModifier() const;
	float GetFireRateModifier() const;
	float GetWantingToShootFireRateModifier() const;
	bool GetUseFromStrafeUpperBodyAimNetwork() const;
	bool GetAimingDownTheBarrel(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	bool GetUseLeftHandIKAllowTags(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;
	bool GetBlockLeftHandIKWhileAiming(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	//
	// Effects
	//

	eWeaponEffectGroup GetEffectGroup() const { return m_Fx.EffectGroup; }
	atHashWithStringNotFinal GetMuzzleFlashPtFxHashName() const { return m_Fx.FlashFx; }
	atHashWithStringNotFinal GetMuzzleFlashAlternatePtFxHashName() const { return m_Fx.FlashFxAlt; }
	atHashWithStringNotFinal GetMuzzleFlashFirstPersonPtFxHashName() const { return m_Fx.FlashFxFP; }
	atHashWithStringNotFinal GetMuzzleFlashFirstPersonAlternatePtFxHashName() const { return m_Fx.FlashFxFPAlt; }
	atHashWithStringNotFinal GetMuzzleSmokePtFxHashName() const { return m_Fx.MuzzleSmokeFx; }
	atHashWithStringNotFinal GetMuzzleSmokeFirstPersonPtFxHashName() const { return m_Fx.MuzzleSmokeFxFP; }
	f32 GetMuzzleSmokePtFxMinLevel() const { return m_Fx.MuzzleSmokeFxMinLevel; }
	f32 GetMuzzleSmokePtFxIncPerShot() const { return m_Fx.MuzzleSmokeFxIncPerShot; }
	f32 GetMuzzleSmokePtFxDecPerSec() const { return m_Fx.MuzzleSmokeFxDecPerSec; }
	const Vector3& GetMuzzleOverrideOffset() const { return m_Fx.MuzzleOverrideOffset; }
	bool GetMuzzleUseProjTintColour() const { return m_Fx.MuzzleUseProjTintColour; }
	atHashWithStringNotFinal GetGunShellPtFxHashName() const { return m_Fx.ShellFx; }
	atHashWithStringNotFinal GetGunShellFirstPersonPtFxHashName() const { return m_Fx.ShellFxFP; }
	atHashWithStringNotFinal GetBulletTracerPtFxHashName() const { return m_Fx.TracerFx; }
	atHashWithStringNotFinal GetPedDamageId() const { return m_Fx.PedDamageHash; }
	f32 GetBulletTracerPtFxChanceSP() const { return m_Fx.TracerFxChanceSP; }
	f32 GetBulletTracerPtFxChanceMP() const { return m_Fx.TracerFxChanceMP; }
	f32 GetBulletTracerPtFxIgnoreCameraIntersection() const { return m_Fx.TracerFxIgnoreCameraIntersection; }
	f32 GetBulletFlashPtFxChanceSP() const { return m_Fx.FlashFxChanceSP; }
	f32 GetBulletFlashPtFxChanceMP() const { return m_Fx.FlashFxChanceMP; }
	f32 GetBulletFlashAltPtFxChance() const { return m_Fx.FlashFxAltChance; }
	f32 GetMuzzleFlashPtFxScale() const { return m_Fx.FlashFxScale; }
	bool GetMuzzleFlashLightEnabled() const { return m_Fx.FlashFxLightEnabled; }
	bool GetMuzzleFlashLightCastsShadows() const { return m_Fx.FlashFxLightCastsShadows; }
	f32 GetMuzzleFlashLightOffsetDist() const { return m_Fx.FlashFxLightOffsetDist; }
	const Vector3& GetMuzzleFlashLightRGBAMin() const { return m_Fx.FlashFxLightRGBAMin; }
	const Vector3& GetMuzzleFlashLightRGBAMax() const { return m_Fx.FlashFxLightRGBAMax; }
	const Vector2& GetMuzzleFlashLightIntensityMinMax() const { return m_Fx.FlashFxLightIntensityMinMax; }
	const Vector2& GetMuzzleFlashLightRangeMinMax() const { return m_Fx.FlashFxLightRangeMinMax; }
	const Vector2& GetMuzzleFlashLightFalloffMinMax() const { return m_Fx.FlashFxLightFalloffMinMax; }
	bool GetGroundDisturbFxEnabled() const { return m_Fx.GroundDisturbFxEnabled; }
	f32 GetGroundDisturbFxDist() const { return m_Fx.GroundDisturbFxDist; }
	atHashWithStringNotFinal GetGroundDisturbFxNameDefault() const { return m_Fx.GroundDisturbFxNameDefault; }
	atHashWithStringNotFinal GetGroundDisturbFxNameSand() const { return m_Fx.GroundDisturbFxNameSand; }
	atHashWithStringNotFinal GetGroundDisturbFxNameDirt() const { return m_Fx.GroundDisturbFxNameDirt; }
	atHashWithStringNotFinal GetGroundDisturbFxNameWater() const { return m_Fx.GroundDisturbFxNameWater; }
	atHashWithStringNotFinal GetGroundDisturbFxNameFoliage() const { return m_Fx.GroundDisturbFxNameFoliage; }

	//
	// Ammo
	//

	bool GetUsesAmmo() const { return GetAmmoInfo() != NULL; }
	u32 GetAmmoMax(const CPed* pPed) const;
	f32 GetProjectileFuseTime(bool bInVehicle = false) const;
	f32 GetProjectileImpactThreshold() const;
	f32 GetProjectileImpactVehicleThreshold() const;
	eExplosionTag GetProjectileExplosionTag() const;
	bool GetProjectileAddSmokeOnExplosion() const;
	bool GetProjectileCanBePlaced() const;
	bool GetProjectileFixedAfterExplosion() const;

	//
	// Vehicle
	//

	bool CanBeUsedAsDriveBy(const CPed* pPed) const;
	const CVehicleWeaponInfo* GetVehicleWeaponInfo() const;

	//
	// NM
	//
	s32 GetKnockdownCount() const { return m_KnockdownCount; };
	f32 GetKillShotImpulseScale() const { return m_KillshotImpulseScale; };
	atHashWithStringNotFinal GetNmShotTuningSet() const {return m_NmShotTuningSet; }

	//
	// Components
	//

	const AttachPoints& GetAttachPoints() const { return m_AttachPoints; }
	const CWeaponComponentPoint* GetAttachPoint(const CWeaponComponentInfo* pComponentInfo) const;
	const bool GetIsWeaponComponentDefault(atHashString weaponComponentName) const ;

	//
	// Stats
	//

	const char* GetStatName() const { return m_StatName.c_str(); }

	//
	// Prescripted target sequences
	//

	atHashWithStringNotFinal GetTargetSequenceGroup() const { return m_TargetSequenceGroup; }

	//
	// Graphics
	//

	s32 GetSpecValuesCount() const { return m_TintSpecValues ? m_TintSpecValues->GetCount() : 0; }
	const CWeaponTintSpecValues::CWeaponSpecValue* GetSpecValues(s32 iTintIndex) const { return m_TintSpecValues ? m_TintSpecValues->GetSpecValuesForTint(iTintIndex) : NULL; }

	// Accuracy
	bool GetDoesRecoilAccuracyAllowHeadShotModifier(const CPed* pHitPed, const float fRecoilAccuracyWhenFired, const float fDistance) const;

	//
	// Gun feed bone
	//
	const CWeaponBoneId& GetGunFeedBone() const {return m_GunFeedBone;}

	// Bloodied textures for melee weapons (see B*2044262/2066768).
	bool GetNextCamoDiffuseTexIdx	(const CWeapon& rWeapon, u32& uNext) const;
	int GetNumCammoDiffuseTextures() const { return m_CamoDiffuseTexIdxs.GetCount(); }

	// Rotate barrel bones
	const CWeaponBoneId& GetRotateBarrelBone() const {return m_RotateBarrelBone;}
	const CWeaponBoneId& GetRotateBarrelBone2() const {return m_RotateBarrelBone2;}

	// Front Clear Test Params
	bool  GetShouldPerformFrontClearTest()	const {return m_FrontClearTestParams.ShouldPerformFrontClearTest;}
	float GetFrontClearForwardOffset()		const {return m_FrontClearTestParams.ForwardOffset;}
	float GetFrontClearVerticalOffset()		const {return m_FrontClearTestParams.VerticalOffset;}
	float GetFrontClearHorizontalOffset()	const {return m_FrontClearTestParams.HorizontalOffset;}
	float GetFrontClearCapsuleRadius()		const {return m_FrontClearTestParams.CapsuleRadius;}
	float GetFrontClearCapsuleLength()		const {return m_FrontClearTestParams.CapsuleLength;}

	//
	// Debug
	//

#if !__FINAL
	// Validate the data structure after loading
	virtual bool Validate() const;
#endif // !__FINAL

#if __BANK
	// Add tuning widgets
	virtual void AddTuningWidgets(bkBank& bank);

	// Additional data for DumpWeaponStats
	const char* GetGroupName() const { return m_Group.GetCStr(); }
#endif // __BANK

private:

	// Called after the data has been loaded
	void OnPostLoad();

	//
	// Members
	//

	eDamageType m_DamageType;
	sExplosion m_Explosion;
	sFrontClearTestParams m_FrontClearTestParams;
	eFireType m_FireType;
	eWeaponWheelSlot m_WheelSlot;
	atHashWithStringNotFinal m_Group;
	const CAmmoInfo* m_AmmoInfo;
	const CAimingInfo* m_AimingInfo;
	u32 m_ClipSize;
	f32 m_AccuracySpread;
	f32 m_AccurateModeAccuracyModifier;
	f32 m_RunAndGunAccuracyModifier;
	f32 m_RunAndGunAccuracyMinOverride;
	f32 m_RecoilAccuracyMax;
    f32 m_RecoilErrorTime;
	f32 m_RecoilRecoveryRate;
	f32 m_RecoilAccuracyToAllowHeadShotAI;
	f32 m_MinHeadShotDistanceAI;
	f32 m_MaxHeadShotDistanceAI;
	f32 m_HeadShotDamageModifierAI;
	f32 m_RecoilAccuracyToAllowHeadShotPlayer;
	f32 m_MinHeadShotDistancePlayer;
	f32 m_MaxHeadShotDistancePlayer;
	f32 m_HeadShotDamageModifierPlayer;
	f32 m_Damage;
	f32 m_DamageTime;
	f32 m_DamageTimeInVehicle;
	f32 m_DamageTimeInVehicleHeadShot;
	f32 m_EnduranceDamage;
	atHashString m_PlayerDamageOverTimeWeapon;
	f32 m_HitLimbsDamageModifier;
	f32 m_NetworkHitLimbsDamageModifier;
	f32 m_LightlyArmouredDamageModifier;
	f32 m_VehicleDamageModifier;

	f32 m_Force;
	f32 m_ForceHitPed;
	f32 m_ForceHitVehicle;
	f32 m_ForceHitFlyingHeli;
	atArray<sHitBoneForceOverride> m_OverrideForces;
	f32 m_ForceMaxStrengthMult;
	f32 m_ForceFalloffRangeStart;
	f32 m_ForceFalloffRangeEnd;
	f32 m_ForceFalloffMin;

	f32 m_ProjectileForce;
	f32 m_FragImpulse;

	f32 m_Penetration;

	f32 m_VerticalLaunchAdjustment;
	f32 m_DropForwardVelocity;

	f32 m_Speed;

	u32 m_BulletsInBatch;
	f32 m_BatchSpread;

	f32 m_ReloadTimeMP;
	f32 m_ReloadTimeSP;
	f32 m_VehicleReloadTime;
	f32 m_AnimReloadRate;
	s32 m_BulletsPerAnimLoop;

	f32 m_TimeBetweenShots;
	f32 m_TimeLeftBetweenShotsWhereShouldFireIsCached;
	f32 m_SpinUpTime;
	f32 m_SpinTime;
	f32 m_SpinDownTime;
	f32 m_AlternateWaitTime;

	f32 m_BulletBendingNearRadius;
	f32 m_BulletBendingFarRadius;
	f32 m_BulletBendingZoomedRadius;

	f32 m_FirstPersonBulletBendingNearRadius;
	f32 m_FirstPersonBulletBendingFarRadius;
	f32 m_FirstPersonBulletBendingZoomedRadius;

	// Effects
	sFx m_Fx;

	// Rumble
	s32 m_InitialRumbleDuration;		// initial rumble setting, when duration runs out the settings below get applied and stay in effect
	f32 m_InitialRumbleIntensity;
	f32 m_InitialRumbleIntensityTrigger;
	s32 m_RumbleDuration;
	f32 m_RumbleIntensity;
	f32 m_RumbleIntensityTrigger;
	f32 m_RumbleDamageIntensity;
    s32 m_InitialRumbleDurationFps;
    f32 m_InitialRumbleIntensityFps;
    s32 m_RumbleDurationFps;
    f32 m_RumbleIntensityFps;

	// Network
	f32 m_NetworkPlayerDamageModifier;
	f32 m_NetworkPedDamageModifier;
	f32 m_NetworkHeadShotPlayerDamageModifier;

	// Ranges
	f32 m_LockOnRange;
	f32 m_WeaponRange;
	f32	m_AiSoundRange;
	f32 m_AiPotentialBlastEventRange;

	// Damage falloff
	f32 m_DamageFallOffRangeMin;
	f32 m_DamageFallOffRangeMax;
	f32 m_DamageFallOffRangeModifier;
	f32 m_DamageFallOffTransientModifier;
	f32 m_DamageFallOffModifier;

	//Area of Effect
	f32 m_AoEModifier;

	//Effect duration
	f32 m_EffectDurationModifier;

	// Vehicle weapon component
	atHashWithStringNotFinal m_VehicleWeaponHash;

	// Cameras
	atHashWithStringNotFinal m_DefaultCameraHash;
	atHashWithStringNotFinal m_AimCameraHash;
	atHashWithStringNotFinal m_FireCameraHash;
	atHashWithStringNotFinal m_CoverCameraHash;
	atHashWithStringNotFinal m_CoverReadyToFireCameraHash;
	atHashWithStringNotFinal m_RunAndGunCameraHash;
	atHashWithStringNotFinal m_CinematicShootingCameraHash;
	atHashWithStringNotFinal m_AlternativeOrScopedCameraHash;
	atHashWithStringNotFinal m_RunAndGunAlternativeOrScopedCameraHash;
	atHashWithStringNotFinal m_CinematicShootingAlternativeOrScopedCameraHash;
	atHashWithStringNotFinal m_PovTurretCameraHash;
	atHashWithStringNotFinal m_RecoilShakeHash;
	atHashWithStringNotFinal m_RecoilShakeHashFirstPerson;
	atHashWithStringNotFinal m_AccuracyOffsetShakeHash;
	u32 m_MinTimeBetweenRecoilShakes;
	float m_RecoilShakeAmplitude;
	float m_ExplosionShakeAmplitude;
	float m_CameraFov;
	float m_FirstPersonAimFovMin;
	float m_FirstPersonAimFovMax;
	float m_FirstPersonScopeFov;
	float m_FirstPersonScopeAttachmentFov;
	Vector3 m_FirstPersonDrivebyIKOffset;
	Vector3 m_FirstPersonRNGOffset;
	Vector3 m_FirstPersonRNGRotationOffset;
	Vector3 m_FirstPersonLTOffset;
	Vector3 m_FirstPersonLTRotationOffset;
	Vector3 m_FirstPersonScopeOffset;
	Vector3 m_FirstPersonScopeAttachmentOffset;
	Vector3 m_FirstPersonScopeRotationOffset;
	Vector3 m_FirstPersonScopeAttachmentRotationOffset;
	Vector3 m_FirstPersonAsThirdPersonIdleOffset;
	Vector3 m_FirstPersonAsThirdPersonRNGOffset;
	Vector3 m_FirstPersonAsThirdPersonLTOffset;
	Vector3 m_FirstPersonAsThirdPersonScopeOffset;
	Vector3 m_FirstPersonAsThirdPersonWeaponBlockedOffset;
	float m_FirstPersonDofSubjectMagnificationPowerFactorNear;
	float m_FirstPersonDofMaxNearInFocusDistance;
	float m_FirstPersonDofMaxNearInFocusDistanceBlendLevel;
	FPScopeAttachData m_FirstPersonScopeAttachmentData;
	float m_ZoomFactorForAccurateMode;

	// Aim offsets
	Vector3 m_AimOffsetMin;
	Vector3 m_AimOffsetMax;
	Vector2 m_TorsoAimOffset;
	Vector2 m_TorsoCrouchedAimOffset;
	float m_AimProbeLengthMin;
	float m_AimProbeLengthMax;
	Vector3 m_AimOffsetMinFPSIdle;
	Vector3 m_AimOffsetMedFPSIdle;
	Vector3 m_AimOffsetMaxFPSIdle;
	Vector3 m_AimOffsetMinFPSLT;
	Vector3 m_AimOffsetMaxFPSLT;
	Vector3 m_AimOffsetMinFPSRNG;
	Vector3 m_AimOffsetMaxFPSRNG;
	Vector3 m_AimOffsetMinFPSScope;
	Vector3 m_AimOffsetMaxFPSScope;
	Vector3 m_AimOffsetEndPosMinFPSIdle;
	Vector3 m_AimOffsetEndPosMedFPSIdle;
	Vector3 m_AimOffsetEndPosMaxFPSIdle;
	Vector3 m_AimOffsetEndPosMinFPSLT;
	Vector3 m_AimOffsetEndPosMedFPSLT;
	Vector3 m_AimOffsetEndPosMaxFPSLT;
	float m_AimProbeRadiusOverrideFPSIdle;
	float m_AimProbeRadiusOverrideFPSIdleStealth;
	float m_AimProbeRadiusOverrideFPSLT;
	float m_AimProbeRadiusOverrideFPSRNG;
	float m_AimProbeRadiusOverrideFPSScope;

	// Ik offsets
	Vector3 m_LeftHandIkOffset;
	float m_IkRecoilDisplacement;
	float m_IkRecoilDisplacementScope;
	float m_IkRecoilDisplacementScaleBackward;
	float m_IkRecoilDisplacementScaleVertical;

	// Reticule
	Vector2 m_ReticuleHudPosition;
	Vector2 m_ReticuleHudPositionOffsetForPOVTurret;
	f32 m_ReticuleMinSizeStanding;
	f32 m_ReticuleMinSizeCrouched;
	f32 m_ReticuleScale;
	atHashWithStringNotFinal m_ReticuleStyleHash;
	atHashWithStringNotFinal m_FirstPersonReticuleStyleHash;

	// Pickup
	atHashWithStringNotFinal m_PickupHash;
	atHashWithStringNotFinal m_MPPickupHash;
	atHashWithStringNotFinal m_HumanNameHash;

	//Audio
	atHashWithStringNotFinal m_AudioCollisionHash;

	// Movement Mode Idle
	atHashWithStringNotFinal m_MovementModeConditionalIdle;

	// Ai ammo diminishing rate
	u8 m_AmmoDiminishingRate;

	// UI bars
	s8 m_HudDamage;
	s8 m_HudSpeed;
	s8 m_HudCapacity;
	s8 m_HudAccuracy;
	s8 m_HudRange;

	// 
	float m_AimingBreathingAdditiveWeight;
	float m_FiringBreathingAdditiveWeight;
	float m_StealthAimingBreathingAdditiveWeight;
	float m_StealthFiringBreathingAdditiveWeight;
	float m_AimingLeanAdditiveWeight;
	float m_FiringLeanAdditiveWeight;
	float m_StealthAimingLeanAdditiveWeight;
	float m_StealthFiringLeanAdditiveWeight;

	// Stat name
	ConstString m_StatName;

	// NM 
	s32 m_KnockdownCount;
	f32 m_KillshotImpulseScale;
	atHashString m_NmShotTuningSet;

	// Allowed components
	AttachPoints m_AttachPoints;

	//Gun Feed bone
	CWeaponBoneId m_GunFeedBone;

	// Flags
	WeaponInfoFlags m_WeaponFlags;

	// Tint values
	CWeaponTintSpecValues* m_TintSpecValues;

	// Firing pattern aliases
	CWeaponFiringPatternAliases* m_FiringPatternAliases;

	// Upper body fixup data
	CWeaponUpperBodyFixupExpressionData* m_ReloadUpperBodyFixupExpressionData;

	// Prescribed target sequence group
	atHashWithStringNotFinal m_TargetSequenceGroup;

	// Bullet direction offset. It's used to angle the helicopter bullet/rockets vectors. 
	float m_BulletDirectionOffsetInDegrees;
	float m_BulletDirectionPitchOffset;
	float m_BulletDirectionPitchHomingOffset;

	float m_WeaponDamageModifier;
	float m_RecoilAccuracyToAllowHeadShotPlayerModifier;
	float m_MinHeadShotDistancePlayerModifier;
	float m_MaxHeadShotDistancePlayerModifier;
	float m_HeadShotDamageModifierPlayerModifier;

	// Expand the ped capsule radius whilst in TaskAimGunOnFoot
	float m_ExpandPedCapsuleRadius;

	float m_VehicleAttackAngle;

	float m_TorsoIKAngleLimit;

	float m_MeleeDamageMultiplier;
	float m_MeleeRightFistTargetHealthDamageScaler;
	float m_AirborneAircraftLockOnMultiplier;
	float m_ArmouredVehicleGlassDamageOverride;
	atBinaryMap<atBinaryMap<u32, u32>, atHashWithStringNotFinal>	m_CamoDiffuseTexIdxs;

	// Rotate barrel bones
	CWeaponBoneId m_RotateBarrelBone;
	CWeaponBoneId m_RotateBarrelBone2;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CWeaponInfo(const CWeaponInfo& other);
	CWeaponInfo& operator=(const CWeaponInfo& other);
};

#endif // WEAPON_INFO_H
