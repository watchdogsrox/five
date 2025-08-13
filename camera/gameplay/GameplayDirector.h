//
// camera/gameplay/GameplayDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef GAMEPLAY_DIRECTOR_H
#define GAMEPLAY_DIRECTOR_H

#include "atl/map.h"
#include "fwutil/Flags.h"
#include "spatialdata/transposedplaneset.h"

#include "camera/base/BaseDirector.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/helpers/Frame.h"
#include "peds/pedDefines.h"								// include for FPS_MODE_SUPPORTED define

#include "task/Motion/Vehicle/TaskMotionInTurret.h"

extern const u32 g_SelfieAimCameraHash;
extern const u32 g_ValkyrieTurretCameraHash;
extern const u32 g_InsurgentTurretCameraHash;
extern const u32 g_InsurgentTurretAimCameraHash;
extern const u32 g_TechnicalTurretCameraHash;
extern const u32 g_TechnicalTurretAimCameraHash;

extern const float g_TurretShakeWeaponPower;
extern const float g_TurretShakeMaxAmplitude;
extern const float g_TurretShakeThirdPersonScaling;
extern const float g_TurretShakeFirstPersonScaling;

class camBaseCamera;
class camBaseSwitchHelper;
class camEnvelope; 
class camFirstPersonAimCamera;
class camFirstPersonPedAimCamera;
class camFirstPersonShooterCamera;
class camCinematicMountedCamera;
class camFirstPersonHeadTrackingAimCamera;
class camFollowCamera;
class camFollowPedCamera;
class camFollowVehicleCamera;
class camFrameShaker;
class camGameplayDirectorMetadata;
class camTableGamesCamera;
class camThirdPersonCamera;
class camThirdPersonAimCamera;
class camThirdPersonVehicleAimCameraMetadata;
class camVehicleCustomSettingsMetadata;
class CControl;
class CPed;
class CTrailer;
class CVehicle;
class CWeaponInfo;

const u32 g_MaxDebugCameraNameLength = 64;

enum eGameplayCameraSettingsFlags
{
	Flag_ShouldClonePreviousOrientation				= BIT0,
	Flag_ShouldClonePreviousControlSpeeds			= BIT1,
	Flag_ShouldFallBackToIdealHeading				= BIT2,
	Flag_ShouldAllowNextCameraToCloneOrientation	= BIT3,
	Flag_ShouldAllowInterpolationToNextCamera		= BIT4
};

struct tGameplayCameraSettings
{
	tGameplayCameraSettings(const CEntity* attachEntity = NULL, u32 cameraHash = 0, s32 interpolationDuration = -1,
		float maxOrientationDeltaToInterpolate = -1.0f, u8 flags = 0xff)
		: m_AttachEntity(attachEntity)
		, m_CameraHash(cameraHash)
		, m_InterpolationDuration(interpolationDuration)
		, m_MaxOrientationDeltaToInterpolate(maxOrientationDeltaToInterpolate)
		, m_Flags(flags)
	{
	}

	void Reset()
	{
		m_AttachEntity						= NULL;
		m_CameraHash						= 0;
		m_InterpolationDuration				= -1;
		m_MaxOrientationDeltaToInterpolate	= -1.0f;
		m_Flags.SetAllFlags();
	}

	const CEntity*	m_AttachEntity;
	u32				m_CameraHash;
	s32				m_InterpolationDuration;
	float			m_MaxOrientationDeltaToInterpolate;
	fwFlags8		m_Flags;
};

class camGameplayDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camGameplayDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum eStates
	{
		DOING_NOTHING,
		START_FOLLOWING_PED,
		FOLLOWING_PED,
		START_FOLLOWING_VEHICLE,
		FOLLOWING_VEHICLE,
		START_VEHICLE_AIMING,
		VEHICLE_AIMING,
		START_MOUNTING_TURRET,
		TURRET_MOUNTED,
		START_TURRET_AIMING,
		START_TURRET_FIRING,
		TURRET_AIMING,
		TURRET_FIRING,
		START_HEAD_TRACKING,
		HEAD_TRACKING
	};

	enum eVehicleEntryExitState
	{
		ENTERING_VEHICLE,		
		INSIDE_VEHICLE,
		EXITING_VEHICLE,
		OUTSIDE_VEHICLE,
		NUM_ENTRY_EXIT_STATES
	};

	struct tCoverSettings
	{
		tCoverSettings() { Reset(); }

		void Reset()
		{
			m_CoverMatrix.Identity();
			m_StepOutPosition.Zero();
			m_CoverUsage				= 0;
			m_CoverToCoverPhase			= 0.0f;
			m_IsInCover					= false;
			m_IsInLowCover				= false;
			m_IsTurning					= false;
			m_IsMovingAroundCornerCover	= false;
			m_IsMovingFromCoverToCover	= false;
			m_CanFireRoundCornerCover	= false;
			m_IsFacingLeftInCover		= false;
			m_IsIdle					= false;
			m_IsPeeking					= false;
			m_IsTransitioningToAiming	= false;
			m_IsAiming					= false;
			m_IsTransitioningFromAiming	= false;
			m_IsAimingHoldActive		= false;
			m_IsBlindFiring				= false;
			m_IsReloading				= false;
			m_ShouldAlignToCornerCover	= false;
			m_ShouldIgnoreStepOut		= false;
		}

		Matrix34	m_CoverMatrix;
		Vector3		m_StepOutPosition;
		s32			m_CoverUsage;
		float		m_CoverToCoverPhase;
		bool		m_IsInCover;
		bool		m_IsInLowCover;
		bool		m_IsTurning;
		bool		m_IsMovingAroundCornerCover;
		bool		m_IsMovingFromCoverToCover;
		bool		m_CanFireRoundCornerCover;
		bool		m_IsFacingLeftInCover;
		bool		m_IsIdle;
		bool		m_IsPeeking;
		bool		m_IsTransitioningToAiming;
		bool		m_IsAiming;
		bool		m_IsTransitioningFromAiming;
		bool		m_IsAimingHoldActive;
		bool		m_IsBlindFiring;
		bool		m_IsReloading;
		bool		m_ShouldAlignToCornerCover;
		bool		m_ShouldIgnoreStepOut;
	};

	struct tExplosionSettings
	{
		tExplosionSettings()
		: m_Position(VEC3_ZERO)
		, m_Strength(0.0f)
		, m_RollOffScaling(1.0f)
		{
		}

		tExplosionSettings(float strength, float rollOffScaling, const Vector3& position)
			: m_Position(position)
			, m_Strength(strength)
			, m_RollOffScaling(rollOffScaling)
		{
		}

		Vector3					m_Position;
		RegdCamBaseFrameShaker	m_FrameShaker;
		float					m_Strength;
		float					m_RollOffScaling;
	};

private:
	camGameplayDirector(const camBaseObjectMetadata& metadata);

public:
	~camGameplayDirector();
	bool 	IsHighAltitudeFovScalingEnabled() const	{ return m_IsHighAltitudeFovScalingPersistentlyEnabled && !m_IsHighAltitudeFovScalingDisabledThisUpdate; }
	bool	IsLookingBehind() const;

	bool	IsUsingVehicleTurret(bool bCheckForValidThirdPersonAimCamera = false) const;
	static bool				IsPedInOrEnteringTurretSeat(const CVehicle* followVehicle, const CPed& followPed, bool bSkipEnterVehicleTestForFirstPerson=true, bool bIsCameraCheckForEntering=false);
	static bool				IsTurretSeat(const CVehicle* followVehicle, int seatIndex, bool bIsCameraCheckForEntering=false);
	static const CTurret*	GetTurretForPed(const CVehicle* followVehicle, const CPed& followPed);

	bool	IsCameraInterpolating(const camBaseCamera* sourceCamera = NULL) const;
	bool	IsFollowing(const CEntity* attachEntity = NULL) const			{ return GetFollowCamera(attachEntity) != NULL; }
	bool	IsFollowingPed(const CEntity* attachEntity = NULL) const		{ return GetFollowPedCamera(attachEntity) != NULL; }
	bool	IsFollowingVehicle(const CEntity* attachEntity = NULL) const	{ return GetFollowVehicleCamera(attachEntity) != NULL; }
	bool	IsTableGamesCameraRunning() const;
	bool	WasTableGamesCameraRunning() const;
	u32		GetTableGamesCameraName() const;
	u32		GetBlendingOutTableGamesCameraName() const; //Returns the name of any table games camera we're currently blending out from

#if FPS_MODE_SUPPORTED
	void	SetSwitchCameraHasTerminated(bool hasFinished) { m_HasSwitchCameraTerminatedToForceFPSCamera = hasFinished; }
	bool	IsAiming(const CEntity* attachEntity = NULL) const				{ return (IsFirstPersonAiming(attachEntity) || IsThirdPersonAiming(attachEntity) || IsFirstPersonShooterAiming(attachEntity)); }
	bool	IsFirstPersonShooterAiming(const CEntity* attachEntity = NULL) const;
#else
	bool	IsAiming(const CEntity* attachEntity = NULL) const				{ return (IsFirstPersonAiming(attachEntity) || IsThirdPersonAiming(attachEntity)); }
#endif
	bool	IsFirstPersonAiming(const CEntity* attachEntity = NULL) const	{ return GetFirstPersonPedAimCamera(attachEntity) != NULL; }
	bool	IsThirdPersonAiming(const CEntity* attachEntity = NULL) const	{ return GetThirdPersonAimCamera(attachEntity) != NULL; }

	bool	IsActiveCamera(const camBaseCamera* camera) const { return (m_ActiveCamera == camera); }

	//NOTE: The gameplay director's persistent frame is affected by post-effects, such as shakes and frame interpolation.
	camThirdPersonCamera*			GetThirdPersonCamera(const CEntity* attachEntity = NULL);
	const camThirdPersonCamera*		GetThirdPersonCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camThirdPersonCamera*>(const_cast<camGameplayDirector*>(this)->GetThirdPersonCamera(attachEntity)); }
	camFollowCamera*				GetFollowCamera(const CEntity* attachEntity = NULL);
	const camFollowCamera*			GetFollowCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFollowCamera*>(const_cast<camGameplayDirector*>(this)->GetFollowCamera(attachEntity)); }
	camFollowPedCamera*				GetFollowPedCamera(const CEntity* attachEntity = NULL);
	const camFollowPedCamera*		GetFollowPedCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFollowPedCamera*>(const_cast<camGameplayDirector*>(this)->GetFollowPedCamera(attachEntity)); }
	camFollowVehicleCamera*			GetFollowVehicleCamera(const CEntity* attachEntity = NULL);
	const camFollowVehicleCamera*	GetFollowVehicleCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFollowVehicleCamera*>(const_cast<camGameplayDirector*>(this)->GetFollowVehicleCamera(attachEntity)); }
	camFirstPersonAimCamera*		GetFirstPersonAimCamera(const CEntity* attachEntity = NULL);
	const camFirstPersonAimCamera*	GetFirstPersonAimCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFirstPersonAimCamera*>(const_cast<camGameplayDirector*>(this)->GetFirstPersonAimCamera(attachEntity)); }
	camFirstPersonPedAimCamera*		GetFirstPersonPedAimCamera(const CEntity* attachEntity = NULL);
	const camFirstPersonPedAimCamera*	GetFirstPersonPedAimCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFirstPersonPedAimCamera*>(const_cast<camGameplayDirector*>(this)->GetFirstPersonPedAimCamera(attachEntity)); }
#if FPS_MODE_SUPPORTED
	camFirstPersonShooterCamera*	GetFirstPersonShooterCamera(const CEntity* attachEntity = NULL);
	const camFirstPersonShooterCamera*	GetFirstPersonShooterCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFirstPersonShooterCamera*>(const_cast<camGameplayDirector*>(this)->GetFirstPersonShooterCamera(attachEntity)); }
#endif // FPS_MODE_SUPPORTED

	camCinematicMountedCamera*	GetFirstPersonVehicleCamera(const CEntity* attachEntity = NULL);
	const camCinematicMountedCamera*	GetFirstPersonVehicleCamera(const CEntity* attachEntity = NULL) const
	{ return const_cast<const camCinematicMountedCamera*>(const_cast<camGameplayDirector*>(this)->GetFirstPersonVehicleCamera(attachEntity)); }

	camFirstPersonHeadTrackingAimCamera*	GetFirstPersonHeadTrackingAimCamera(const CEntity* attachEntity = NULL);
	const camFirstPersonHeadTrackingAimCamera*	GetFirstPersonHeadTrackingAimCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camFirstPersonHeadTrackingAimCamera*>(const_cast<camGameplayDirector*>(this)->GetFirstPersonHeadTrackingAimCamera(attachEntity)); }
	camThirdPersonAimCamera*		GetThirdPersonAimCamera(const CEntity* attachEntity = NULL);
	const camThirdPersonAimCamera*	GetThirdPersonAimCamera(const CEntity* attachEntity = NULL) const
									{ return const_cast<const camThirdPersonAimCamera*>(const_cast<camGameplayDirector*>(this)->GetThirdPersonAimCamera(attachEntity)); }

#if FPS_MODE_SUPPORTED
	bool	IsFirstPersonModeEnabled() const						{ return m_IsFirstPersonModeEnabled; }
	bool	IsFirstPersonModeAllowed() const						{ return m_IsFirstPersonModeAllowed; }
	bool	IsFirstPersonShooterLeftStickControl() const			{ return m_IsFirstPersonShooterLeftStickControl; }
	bool	IsFirstPersonUseAnimatedDataInVehicle() const			{ return m_bFirstPersonAnimatedDataInVehicle; }

    u32     GetFirstPersonAnimatedDataStartTime() const             { return m_FirstPersonAnimatedDataStartTime; }

	bool	IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch() const { return m_IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch; }

	void	DisableFirstPersonThisUpdate(const CPed* ped = NULL, bool fromScript = false);
	bool	IsFirstPersonDisabledThisUpdate() const					{ return m_DisableFirstPersonThisUpdate; }
#if __BANK
	bool	DebugWasFirstPersonDisabledThisUpdate() const			{ return m_DebugWasFirstPersonDisabledThisUpdate; }
	bool	DebugWasFirstPersonDisabledThisUpdateFromScript() const	{ return m_DebugWasFirstPersonDisabledThisUpdateFromScript; }
#endif // __BANK

	bool	ShouldUse1stPersonControls() const;

	bool	ComputeShouldUseFirstPersonDriveBy() const;

	// TODO: remove these, only for debugging (allowing features to be turned on/off)
	bool	ShouldUseThirdPersonCameraForFirstPersonMode() const	{ return false; }
	bool	IsFirstPersonAnimatedDataEnabled() const				{ return m_EnableFirstPersonUseAnimatedData; }
	bool	IsFirstPersonAnimatedDataRelativeToMover() const		{ return m_FirstPersonAnimatedDataRelativeToMover; }
	bool	IsApplyRelativeOffsetInCameraSpace() const				{ return m_bApplyRelativeOffsetInCameraSpace; }
	bool	IsForcingFirstPersonBonnet() const						{ return m_ForceFirstPersonBonnetCamera; }
	void	OverrideFPCameraThisUpdate()							{ m_OverrideFirstPersonCameraThisUpdate = true;  }
#endif // FPS_MODE_SUPPORTED

	void	GetThirdPersonToFirstPersonFovParameters(float& fovDelta, u32& duration) const;

	bool	ShouldDisplayReticule() const		{ return m_ShouldDisplayReticule; }
	bool	ShouldDimReticule() const			{ return m_ShouldDimReticule; }

	const CPed* GetFollowPed() const			{ return m_FollowPed; }
	const CVehicle* GetFollowVehicle() const	{ return m_FollowVehicle; }
	const CVehicle* GetAttachedTrailer() const	{ return m_AttachedTrailer; }
	const CVehicle* GetTowedVehicle() const		{ return m_TowedVehicle; }

	s32		GetVehicleEntryExitState() const					{ return m_VehicleEntryExitState; }
	s32		GetVehicleEntryExitStateOnPreviousUpdate() const	{ return m_VehicleEntryExitStateOnPreviousUpdate; }
	void	SetScriptOverriddenVehicleExitEntryState(CVehicle* pVehicle, s32 ExitEntryState); 

	bool	ShouldDelayVehiclePovCamera() const					{ return m_ShouldDelayVehiclePovCamera; }
	bool	ShouldDelayVehiclePovCameraOnPreviousUpdate() const	{ return m_ShouldDelayVehiclePovCameraOnPreviousUpdate; }

	CControl* GetActiveControl() const;

	s32		GetActiveViewModeContext() const;
	s32		GetActiveViewMode(s32 context) const;

	void	RegisterWeaponFire(const CWeapon& weapon, const CEntity& parentEntity);
	void	RegisterPedKill(const CPed& inflictor, const CPed& victim, u32 weaponHash, bool isHeadShot);
	void	RegisterVehicleDamage(const CVehicle* pVehicle, float damage);
	void	RegisterVehiclePartBroken(const CVehicle& vehicle, eHierarchyId vehiclePartId, float amplitude = 1.0f);
	void	RegisterExplosion(u32 shakeNameHash, float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion);
	void	RegisterEnduranceDamage(float damageAmount);

	void 	SetPersistentHighAltitudeFovScalingState(bool state)					{ m_IsHighAltitudeFovScalingPersistentlyEnabled = state; }
	void 	DisableHighAltitudeFovScalingThisUpdate()								{ m_IsHighAltitudeFovScalingDisabledThisUpdate = true; }

	bool	SetScriptOverriddenFollowPedCameraSettingsThisUpdate(const tGameplayCameraSettings& settings);
	bool	SetScriptOverriddenFollowVehicleCameraSettingsThisUpdate(const tGameplayCameraSettings& settings);
    bool	SetScriptOverriddenTableGamesCameraSettingsThisUpdate(const tGameplayCameraSettings& settings);
	
	void SetScriptOverriddenFollowVehicleCameraHighAngleModeEveryUpdate(bool shouldOverride, bool state)
	{
		m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeEveryUpdate = shouldOverride; 
		m_ScriptOverriddenFollowVehicleCameraHighAngleModeEveryUpdate = state; 
	}
	
	void SetScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate(bool state)
	{
		m_ScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate = state;
		m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate = true;
	}

	bool GetScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate(bool& state)
	{
		if(m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeEveryUpdate)
		{
			state = m_ScriptOverriddenFollowVehicleCameraHighAngleModeEveryUpdate; 
			return true; 
		}
			
		if(m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate)
		{
			state = m_ScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate;
		}

		return m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate;
	}

	void	SetShouldAllowCustomVehicleDriveByCamerasThisUpdate(bool state) { m_ShouldAllowCustomVehicleDriveByCamerasThisUpdate = state; }

	void	ScriptForceUseTightSpaceCustomFramingThisUpdate() { m_ScriptForceUseTightSpaceCustomFramingThisUpdate = true; }
	bool	IsScriptForcingUseTightSpaceCustomFramingThisUpdate() const { return m_ScriptForceUseTightSpaceCustomFramingThisUpdate; }

	void	SetScriptControlledMotionBlurScalingThisUpdate(float scaling)			{ m_ScriptControlledMotionBlurScalingThisUpdate = scaling; }
	void	SetScriptControlledMaxMotionBlurStrengthThisUpdate(float maxStrength)	{ m_ScriptControlledMaxMotionBlurStrengthThisUpdate = maxStrength; }
	float	GetRelativeCameraHeading() const;
	void	SetRelativeCameraHeading(float heading = 0.0f, float smoothRate = 1.0f);
	float	GetRelativeCameraPitch() const;
	void	SetRelativeCameraPitch(float pitch = 0.0f, float smoothRate = 1.0f);
	void 	ResurrectPlayer();

	const CWeaponInfo* GetFollowPedWeaponInfo() const							{ return m_FollowPedWeaponInfo; }
	const tCoverSettings& GetFollowPedCoverSettings() const						{ return m_FollowPedCoverSettings; }
	const tCoverSettings& GetFollowPedCoverSettingsOnPreviousUpdate() const		{ return m_FollowPedCoverSettingsOnPreviousUpdate; }
	const tCoverSettings& GetFollowPedCoverSettingsForMostRecentCover() const	{ return m_FollowPedCoverSettingsForMostRecentCover; }

	float	GetFollowPedCoverFacingDirectionScaling() const	{ return m_FollowPedCoverFacingDirectionScalingSpring.GetResult(); }

	u32		GetFollowPedTimeSpentSwimming() const { return m_FollowPedTimeSpentSwimming; }
	u32		GetLastTimeAttachParentWasInWater() const { return m_LastTimeAttachParentWasInWater; }

	float	GetTightSpaceLevel() const			{ return m_TightSpaceLevel; }
	float	GetTrailerBlendLevel() const		{ return m_TrailerBlendLevel; }
	float	GetTowedVehicleBlendLevel() const	{ return m_TowedVehicleBlendLevel; }

	void	SetUseScriptHeading(float Heading)				{m_UseScriptRelativeHeading = true; m_CachedScriptHeading = Heading; }
	void	SetUseScriptPitch(float Pitch, float PitchRate)	{m_UseScriptRelativePitch = true; m_CachedScriptPitch = Pitch; m_CachedScriptPitchRate = PitchRate;}
	void	SetFirstPersonShooterCameraHeading(float heading) { m_UseScriptFirstPersonShooterHeading = true; m_CachedScriptFirstPersonShooterHeading = heading; }
	void	SetFirstPersonShooterCameraPitch(float pitch)     { m_UseScriptFirstPersonShooterPitch = true;   m_CachedScriptFirstPersonShooterPitch = pitch; }
	void	SetUseFirstPersonFallbackHeading(float Heading)				{m_UseFirstPersonFallbackRelativeHeading = true; m_CachedFirstPersonFallbackHeading = Heading; }
	void	SetUseFirstPersonFallbackPitch(float Pitch, float PitchRate)	{m_UseFirstPersonFallbackRelativePitch = true; m_CachedFirstPersonFallbackPitch = Pitch; m_CachedFirstPersonFallbackPitchRate = PitchRate;}
	

	void	SetScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate(const Vector2& limits)
	{
		m_ScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate		= limits;
		m_ShouldScriptOverrideFirstPersonAimCameraRelativeHeadingLimitsThisUpdate	= true;
	}
	void	SetScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate(const Vector2& limits)
	{
		m_ScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate			= limits;
		m_ShouldScriptOverrideFirstPersonAimCameraRelativePitchLimitsThisUpdate		= true;
	}
	
	void	SetScriptOverriddenFirstPersonAimCameraNearClipThisUpdate(float NearClip)
	{
		m_ScriptOverriddenFirstPersonAimCameraNearClipThisUpdate		= NearClip;
		m_ShouldScriptOverrideFirstPersonAimCameraNearClipThisUpdate		= true;
	}
	
	void	SetScriptOverriddenThirdPersonAimCameraNearClipThisUpdate(float NearClip)
	{
		m_ScriptOverriddenThirdPersonAimCameraNearClipThisUpdate		= NearClip;
		m_ShouldScriptOverrideThirdPersonAimCameraNearClipThisUpdate		= true;
	}

	void	SetScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate(const Vector2& limits)
	{
		m_ScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate		= limits;
		m_ShouldScriptOverrideThirdPersonCameraRelativeHeadingLimitsThisUpdate	= true;
	}
	void	SetScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate(const Vector2& limits)
	{
		m_ScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate		= limits;
		m_ShouldScriptOverrideThirdPersonCameraRelativePitchLimitsThisUpdate	= true;
	}
	void	SetScriptOverriddenThirdPersonCameraOrbitDistanceLimitsThisUpdate(const Vector2& limits)
	{
		m_ScriptOverriddenThirdPersonOrbitDistanceLimitsThisUpdate		= limits;
		m_ShouldScriptOverrideThirdPersonCameraOrbitDistanceLimitsThisUpdate	= true;
	}

	bool	GetScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate(Vector2& limits) const
	{
		if (m_ShouldScriptOverrideThirdPersonCameraRelativeHeadingLimitsThisUpdate)
		{
			limits = m_ScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate;
			return true;
		}

		return false;
	}
	bool	GetScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate(Vector2& limits) const
	{
		if (m_ShouldScriptOverrideThirdPersonCameraRelativePitchLimitsThisUpdate)
		{
			limits = m_ScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate;
			return true;
		}

		return false;
	}
	bool	GetScriptOverriddenThirdPersonCameraOrbitDistanceLimitsThisUpdate(float& minDistance, float& maxDistance) const
	{
		if (m_ShouldScriptOverrideThirdPersonCameraOrbitDistanceLimitsThisUpdate)
		{
			minDistance = m_ScriptOverriddenThirdPersonOrbitDistanceLimitsThisUpdate.x;
			maxDistance = m_ScriptOverriddenThirdPersonOrbitDistanceLimitsThisUpdate.y;
			return true;
		}

		return false;
	}

	void	SetScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate() { m_ScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate = true; }
	bool	GetScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate() const { return m_ScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate; }

	void	SetScriptEntityToIgnoreCollisionThisUpdate(const CEntity* entity)				{ m_ScriptEntityToIgnoreCollisionThisUpdate = entity; }
	void	SetScriptEntityToLimitFocusOverBoundingSphereThisUpdate(const CEntity* entity)	{ m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate = entity; }

	void	SetShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript(bool shouldIgnore = true) { m_ShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript = shouldIgnore; }

	void	CatchUpFromFrame(const camFrame& sourceFrame, float maxDistanceToBlend = 0.0f, int blendType = -1);

	void	StartSwitchBehaviour(s32 switchType, bool isIntro, const CPed* oldPed = NULL, const CPed* newPed = NULL, s32 shortSwitchStyle = 0, float directionSign = 1.0f);
	void	StopSwitchBehaviour();
	void	SetSwitchBehaviourPauseState(bool state);
	bool	IsSwitchBehaviourFinished() const;
	const camBaseSwitchHelper* GetSwitchHelper() const { return m_SwitchHelper; }
	s32		ComputeShortSwitchStyle(const CPed& oldPed, const CPed& newPed, float& directionSign) const;
	bool	IsShortRangePlayerSwitchFromFirstPerson()  { return m_ShortRangePlayerSwitchFromFirstPerson; }
	bool	ShouldUseFirstPersonSwitch(const CPed& pedToConsider);

	void	ScriptRequestVehicleCamStuntSettingsThisUpdate();
	void	ScriptForceVehicleCamStuntSettingsThisUpdate();
	void	ScriptRequestDedicatedStuntCameraThisUpdate(const char* stuntCameraName);

    void    ScriptOverrideFollowVehicleCameraSeatThisUpdate(int seatIndex) { m_ScriptOverriddenFollowVehicleSeatThisUpdate = seatIndex; }
    int     GetScriptOverriddenFollowVehicleCameraSeat(int seatIndex) const { return m_ScriptOverriddenFollowVehicleSeatThisUpdate >= 0 ? m_ScriptOverriddenFollowVehicleSeatThisUpdate : seatIndex; }     

	bool	IsScriptRequestingVehicleCamStuntSettingsThisUpdate() const { return m_ScriptRequestedVehicleCamStuntSettingsThisUpdate; }
	bool	IsScriptForcingVehicleCamStuntSettingsThisUpdate() const { return m_ScriptRequestedVehicleCamStuntSettingsThisUpdate; }
	bool	IsScriptRequestingDedicatedStuntCameraThisUpdate() const { return m_ScriptRequestDedicatedStuntCameraThisUpdate; }
	bool	IsVehicleCameraUsingStuntSettingsThisUpdate(const camBaseCamera* pTargetCamera) const;
	bool	IsAttachVehicleMiniTank() const;

	//Hint 
	void	StartHinting(const Vector3& HintTarget, u32 Attack, s32 hold, u32 Release, u32 HintType);
	void	StartHinting(const CEntity* pEnt, const Vector3& Offset, bool IsRelative, u32 Attack, s32 hold, u32 Release, u32 HintType); 
	void	StopHinting(bool StopImmediately);
	const float GetHintBlend() const { return m_HintEnvelopeLevel; } 
	const camEnvelope* GetHintEnvelope() const { return m_HintEnvelope; }
	const Vector3& UpdateHintPosition(); 
	bool	IsHintActive() const;  
	bool	IsCodeHintActive() { return m_IsCodeHintActive; }
	const atHashWithStringNotFinal& GetHintHelperToCreate() const { return m_HintType; }
	void	BlockCodeHintThisUpdate() { m_IsScriptBlockingCodeHints = true; }
	const CEntity* GetHintEntity() const { return m_HintEnt; } 
	
	void	SetScriptHintFov(float FovScalar){ m_ScriptOverriddenHintFovScalar = FovScalar; m_IsScriptOverridingHintFovScalar = true;}
	void	SetScriptHintDistanceScalar(float DistanceScalar){ m_ScriptOverriddenHintDistanceScalar = DistanceScalar; m_IsScriptOverridingHintDistanceScalar = true;}
	void	SetScriptHintBaseOrbitPitchOffset(float BaseOrbitPitchOffset){ m_ScriptOverriddenHintBaseOrbitPitchOffset = BaseOrbitPitchOffset; m_IsScriptOverridingHintBaseOrbitPitchOffset = true;}
	void	SetScriptHintCameraRelativeSideOffsetAdditive(float SideOffsetAdditive){ m_ScriptOverriddenHintCameraRelativeSideOffsetAdditive = SideOffsetAdditive; m_IsScriptOverridingHintCameraRelativeSideOffsetAdditive = true;}
	void	SetScriptHintCameraRelativeVerticalOffsetAdditive(float VerticalOffsetAdditive){ m_ScriptOverriddenHintCameraRelativeVerticalOffsetAdditive = VerticalOffsetAdditive; m_IsScriptOverridingHintCameraRelativeVerticalOffsetAdditive = true;}
	void	SetScriptHintCameraBlendToFollowPedMediumViewMode(bool state) { m_IsScriptForcingHintCameraBlendToFollowPedMediumViewMode = state; }

	bool	IsScriptOverridingHintFov(){ return m_IsScriptOverridingHintFovScalar; }
	bool	IsScriptOverridingScriptHintDistanceScalar(){ return m_IsScriptOverridingHintDistanceScalar; }
	bool	IsScriptOverridingScriptHintBaseOrbitPitchOffset(){ return m_IsScriptOverridingHintBaseOrbitPitchOffset; }
	bool	IsScriptOverridingScriptHintCameraRelativeSideOffsetAdditive(){ return m_IsScriptOverridingHintCameraRelativeSideOffsetAdditive; }
	bool	IsScriptOverridingScriptHintCameraRelativeVerticalOffsetAdditive(){ return m_IsScriptOverridingHintCameraRelativeVerticalOffsetAdditive; }
	bool	IsScriptForcingHintCameraBlendToFollowPedMediumViewMode() const { return m_IsScriptForcingHintCameraBlendToFollowPedMediumViewMode; }

	float	GetScriptOverriddenHintFov(){ return m_ScriptOverriddenHintFovScalar; }
	float	GetScriptOverriddenHintDistanceScalar(){ return m_ScriptOverriddenHintDistanceScalar; }
	float	GetScriptOverriddenHintBaseOrbitPitchOffset(){ return m_ScriptOverriddenHintBaseOrbitPitchOffset; }
	float	GetScriptOverriddenHintCameraRelativeSideOffsetAdditive(){ return m_ScriptOverriddenHintCameraRelativeSideOffsetAdditive; }
	float	GetScriptOverriddenHintCameraRelativeVerticalOffsetAdditive(){ return m_ScriptOverriddenHintCameraRelativeVerticalOffsetAdditive; }

	bool	IsSphereVisible(const Vector3& centre, const float radius) const;
#if FPS_MODE_SUPPORTED
	bool	GetJustSwitchedFromSniperScope() const { return m_JustSwitchedFromSniperScope; }
#endif // FPS_MODE_SUPPORTED

	void ForceAfterAimingBlendThisUpdate()		{ m_ForceAfterAimingBlend = true; }
	void DisableAfterAimingBlendThisUpdate()	{ m_DisableAfterAimingBlend = true; }
	bool IsAfterAimingBlendForced() const		{ return m_ForceAfterAimingBlend; }
	bool IsAfterAimingBlendDisabled() const		{ return m_DisableAfterAimingBlend; }

	const camVehicleCustomSettingsMetadata* FindVehicleCustomSettings(u32 vehicleModelNameHash) const;

	bool	SetDeathFailEffectState(s32 state);

	const atArray<tExplosionSettings>& GetExplosionSettings() const { return m_ExplosionSettings; }

#if __BANK
	virtual void AddWidgetsForInstance();
	void	DebugSetRelativeOrbitOrientation();
	void	DebugFlipRelativeOrbitOrientation();
	void	DebugSpinRelativeOrbitOrientation();
	void	DebugHintTowardsPickerSelection();
	void	DebugStopHinting();
	void	DebugStopHintingImmediately();
	void	DebugCatchUpFromRenderedFrame();
	void	DebugCatchUpFromGameplayFrame();
	void	DebugAbortCatchUp();

	virtual void DebugGetCameras(atArray<camBaseCamera*> &cameraList) const;

	void	DebugSetOverrideMotionBlurThisUpdate(const float blurStrength);
	void	DebugSetOverrideDofThisUpdate(const float nearDof, const float farDof);

	bool	IsDebugHintActive() const { return m_DebugHintActive; }
#endif // __BANK

	const Vector3& GetPreviousAimCameraForward()  const { return m_PreviousAimCameraForward; }
	const Vector3& GetPreviousAimCameraPosition() const { return m_PreviousAimCameraPosition; }

#if FPS_MODE_SUPPORTED
	bool	ComputeIsFirstPersonModeAllowed(const CPed* followPed, const bool bCheckViewMode, bool& shouldFallBackForDirectorBlendCatchUpOrSwitch, bool* pToggleForceBonnetCameraView = NULL, bool* pDisableForceBonnetCameraView = NULL) const;
#endif // FPS_MODE_SUPPORTED

	static const CWeaponInfo* ComputeWeaponInfoForPed(const CPed& ped);
	static const CWeaponInfo* GetTurretWeaponInfoForSeat(const CVehicle& followVehicle, s32 seatIndex);
    static s32 ComputeSeatIndexPedIsIn(const CVehicle& followVehicle, const CPed& followPed, bool bSkipEnterVehicleTestForFirstPerson = false, bool bOnlyUseEnterSeatIfIsDirectEntry = false);

	void UseSwitchCustomFovBlend() { m_UseSwitchCustomFovBlend = true; }

	void SetShouldPerformCustomTransitionAfterWarpFromVehicle() { m_ShouldPerformCustomTransitionAfterWarpFromVehicle = true; }

private:
	virtual bool Update();
	virtual void PostUpdate();

	void	UpdateVehicleEntryExitState(const CPed& followPed);	
	void	UpdateHeliTurretCollision(const CPed& followPed);
	void	UpdateFollowPedWeaponSettings(const CPed& followPed);
	void	UpdateFollowPedCoverSettings(const CPed& followPed, bool shouldResetBlendLevels);
	void	UpdateFollowPedBlendLevels(bool shouldResetBlendLevels);
	void	UpdateTightSpaceSettings(const CPed& followPed);

	void	UpdateStartFollowingPed(const CVehicle* followVehicle, const CPed& followPed);
	void	UpdateBlendInTableGamesCamera(const tGameplayCameraSettings& cameraSettings, camBaseCamera& tableGamesCamera, camBaseCamera& previousCamera);
	void	UpdateBlendOutTableGamesCamera(const tGameplayCameraSettings& cameraSettings, camBaseCamera& activeCamera, camBaseCamera& previousCamera);
	bool	UpdateFollowingPed(const CPed& followPed);

	void	UpdateStartFollowingVehicle(const CVehicle* followVehicle);
	bool	UpdateFollowingVehicle(const CVehicle* followVehicle, const CPed& followPed);

	void	UpdateStartHeadTracking(const CVehicle* followVehicle, const CPed& followPed);
	bool	UpdateHeadTracking(const CPed& followPed);

	void	UpdateStartVehicleAiming(const CVehicle* followVehicle, const CPed& followPed);
	bool	UpdateVehicleAiming(const CVehicle* followVehicle, const CPed& followPed);

	void	UpdateStartMountingTurret(const CVehicle* followVehicle, const CPed& followPed);
	bool	UpdateTurretMounted(const CVehicle* followVehicle, const CPed& followPed);

	void	UpdateStartTurretAiming(const CVehicle* followVehicle, const CPed& followPed);
	void	UpdateStartTurretFiring(const CVehicle* followVehicle, const CPed& followPed);
	bool	UpdateTurretAiming(const CVehicle* followVehicle, const CPed& followPed);
	bool	UpdateTurretFiring(const CVehicle* followVehicle, const CPed& followPed);

	bool	ComputeShouldUseVehicleStuntSettings();
	bool	ComputeVehicleProximityToGround();
	void	ComputeFollowPedCameraSettings(const CPed& followPed, tGameplayCameraSettings& settings) const;
	bool	ComputeCoverEntryAlignmentHeading(const camBaseCamera& previousCamera, float& alignmentHeading) const;

	void	ComputeFollowVehicleCameraSettings(const CVehicle& followVehicle, tGameplayCameraSettings& settings) const;
	void	ComputeVehicleAimCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const;
	bool	ComputeTurretCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const;
	bool	ComputeTurretAimCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const;
	bool	ComputeTurretFireCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const;

	bool	ComputeFrontForVehicleEntryExit(const CPed& followPed, const CVehicle* followVehicle, const camFollowVehicleCamera& followVehicleCamera,
				const camFollowCamera& previousCamera, Vector3& front) const;

	bool	ComputeShouldInterpolateIntoAiming(const CPed& followPed, const camFollowCamera& followCamera, const camThirdPersonAimCamera& aimCamera,
				s32& interpolationDuration, bool isAimingDuringVehicleExit) const;

	bool	ComputeIsVehicleAiming(const CVehicle& followVehicle, const CPed& followPed) const;
	bool	ComputeShouldVehicleLockOnAim(const CVehicle& followVehicle, const CPed& followPed) const;

	void	CachePreviousCameraOrientation(const camFrame& previousCamFrame);

#if FPS_MODE_SUPPORTED
	void	RemapInputsForFirstPersonShooter();
	u32		ComputeFirstPersonShooterCameraRef() const;
	bool	IsDelayTurretEntryForFpsCamera() const;
	void	UpdateBonnetCameraViewToggle(bool bToggledForceBonnetCameraView, bool bDisableForceBonnetCameraView);
#endif // FPS_MODE_SUPPORTED

	bool	CreateActiveCamera(u32 cameraHash);

	void	Reset();

	void	DestroyAllCameras();

	bool	ComputeShouldRecreateFollowPedCamera(const CPed& followPed) const;
	bool	ComputeShouldRecreateFollowVehicleCamera(const CVehicle& followVehicle) const;
	bool	ComputeShouldRecreateVehicleAimCamera(const CVehicle& followVehicle, const CPed& followPed) const;

	void	UpdateMotionBlur(const CPed& followPed);

	void	UpdateDepthOfField();

	void	UpdateReticuleSettings(const CPed& followPed);

	void	ApplyCameraOverridesForScript();

	void	UpdateCatchUp();

	void	UpdateSwitchBehaviour();

	void	UpdateFirstPersonAnimatedDataState(const CPed& followPed);

	void	CheckForMissileHint(); 

	void	UpdateHint(); 
	void	CreateHintHelperEnvelope(u32 Attack, s32 hold, u32 Release);
	void	CreateRagdollLookAtEnvelope(); 
	void	InitHint(); 

	void	UpdateMissileHintSelection(); 
	void	ComputeMissileHintTarget(); 
	bool	CanHintAtMissile(); 
	void	UpdateMissileHint(); 
	void	UpdateInputForQuickToggleActivationMode(); 
	void    ToggleMissileHint(); 

	void	UpdateAimAccuracyOffsetShake();
	float	ComputeDesiredAimAccuracyOffsetAmplitude() const;

	void	UpdateTrailerStatus();
	bool	ComputeCanVehiclePullTrailer(const CVehicle& vehicle) const;
	float	ComputeDistanceBetweenVehicleAndTrailerAttachPoints(const CVehicle& vehicle, const CVehicle& trailer) const;
	void	ResetAttachedTrailerStatus();
	void	UpdateTowedVehicleStatus();
	void	ResetTowedVehicleStatus();
	void	UpdateVehiclePovCameraConditions();
	bool	ComputeShouldDelayVehiclePovCamera() const;

	float	ComputeShakeAmplitudeForExplosion(float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion) const;
	float	ComputeBaseAmplitudeForExplosion(float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion) const;
	void	StopExplosionShakes();
	void	RegisterWeaponFireRemote(const CWeapon& weapon, const CEntity& firingEntity);
	void	RegisterWeaponFireLocal(const CWeapon& weapon, const CEntity& firingEntity);

	bool	ComputeIsSwitchBehaviourValidForPed(s32 switchType, const CPed& ped);
	bool	ComputeAttachPedRootBoneMatrix(Matrix34& matrix) const;
	void	UpdateRagdollBlendLevel(); 
	
	void		UpdateTurretHatchCollision(const CPed& followPed) const;
	static void SetCarDoorCameraCollision(const CVehicle* vehicle, s32 seatIndex, bool enabled);
	void		SetCachedHeliTurretCameraCollision(bool enabled);	

	void		UpdateAttachParentInWater();

	const camGameplayDirectorMetadata& m_Metadata;

	atMap<u32, const camVehicleCustomSettingsMetadata*> m_VehicleCustomSettingsMap;	

	atArray<tExplosionSettings> m_ExplosionSettings;

	spdTransposedPlaneSet8 m_TransposedPlaneSet;
	
	camDampedSpring m_FollowPedCoverNormalHeadingSpring;
	camDampedSpring m_FollowPedCoverFacingDirectionScalingSpring;
	camDampedSpring m_AimAccuracyOffsetAmplitudeSpring;
	camDampedSpring m_TowedVehicleBlendSpring;

	RegdCamBaseCamera	m_ActiveCamera;
	RegdCamBaseCamera	m_PreviousCamera;

	RegdCamBaseFrameShaker m_DeathFailEffectFrameShaker;

	RegdConstPed m_FollowPed;
	RegdConstVeh m_FollowVehicle;
	RegdConstVeh m_AttachedTrailer;
	RegdConstVeh m_TowedVehicle;
	RegdConstVeh m_ScriptOverriddenFollowVehicle;

	RegdConstVeh				m_HeliVehicleForTurret;
	s32							m_HeliVehicleSeatIndexForTurret;	
	s32							m_HeliTurretBaseComponentIndex;
	s32							m_HeliTurretBarrelComponentIndex;
	s32							m_HeliTurretWeaponComponentIndex;

	RegdConstWeaponInfo m_FollowPedWeaponInfo;
	RegdConstWeaponInfo m_FollowPedWeaponInfoOnPreviousUpdate;
	
	RegdConstEnt m_ScriptEntityToIgnoreCollisionThisUpdate; 
	RegdConstEnt m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate;

	tCoverSettings m_FollowPedCoverSettings;
	tCoverSettings m_FollowPedCoverSettingsOnPreviousUpdate;
	tCoverSettings m_FollowPedCoverSettingsForMostRecentCover;

	tGameplayCameraSettings m_ScriptOverriddenFollowPedCameraSettingsThisUpdate;
	tGameplayCameraSettings m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate;
	tGameplayCameraSettings m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate;
	tGameplayCameraSettings m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate;
    tGameplayCameraSettings m_ScriptOverriddenTableGamesCameraSettingsThisUpdate;
    tGameplayCameraSettings m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate;

	Vector3 m_FollowPedPositionOnPreviousUpdate;
	Vector3 m_PreviousAimCameraForward;
	Vector3 m_PreviousAimCameraPosition;

	eStates	m_State;
	eStates	m_PreviousState;

	u32		m_FollowPedCoverAimingHoldStartTime;
	u32		m_FollowPedTimeSpentSwimming;

	u32		m_CachedPreviousCameraTableGamesCameraHash;

	s32		m_VehicleEntryExitState;
	s32		m_VehicleEntryExitStateOnPreviousUpdate;
	s32		m_ScriptOverriddenVehicleEntryExitState; 

    s32     m_ScriptOverriddenFollowVehicleSeatThisUpdate;

	float	m_TightSpaceLevel;
	float	m_TrailerBlendLevel;
	float	m_TowedVehicleBlendLevel;

	float	m_ScriptControlledMotionBlurScalingThisUpdate;
	float	m_ScriptControlledMaxMotionBlurStrengthThisUpdate;

	float	m_CachedScriptHeading; 
	float	m_CachedScriptPitch;
	float	m_CachedScriptPitchRate;
	float	m_CachedScriptFirstPersonShooterHeading;
	float	m_CachedScriptFirstPersonShooterPitch;
	float	m_CachedFirstPersonFallbackHeading; 
	float	m_CachedFirstPersonFallbackPitch;
	float	m_CachedFirstPersonFallbackPitchRate;
	
	float	m_ScriptOverriddenHintFovScalar; 
	float	m_ScriptOverriddenHintDistanceScalar; 
	float	m_ScriptOverriddenHintBaseOrbitPitchOffset; 
	float	m_ScriptOverriddenHintCameraRelativeSideOffsetAdditive; 
	float	m_ScriptOverriddenHintCameraRelativeVerticalOffsetAdditive; 
	float	m_RagdollBlendLevel; 

	float m_ScriptOverriddenFirstPersonAimCameraNearClipThisUpdate; 
	float m_ScriptOverriddenThirdPersonAimCameraNearClipThisUpdate; 

	bool	m_UseScriptRelativeHeading;
	bool	m_UseScriptRelativePitch;
	bool	m_UseScriptFirstPersonShooterHeading;
	bool	m_UseScriptFirstPersonShooterPitch;
	bool	m_UseFirstPersonFallbackRelativeHeading;
	bool	m_UseFirstPersonFallbackRelativePitch;

	Vector2	m_ScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate;
	Vector2	m_ScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate;
	Vector2	m_ScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate;
	Vector2	m_ScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate;
	Vector2	m_ScriptOverriddenThirdPersonOrbitDistanceLimitsThisUpdate;
	bool	m_ShouldScriptOverrideFirstPersonAimCameraRelativeHeadingLimitsThisUpdate;
	bool	m_ShouldScriptOverrideFirstPersonAimCameraRelativePitchLimitsThisUpdate;
	bool	m_ShouldScriptOverrideFirstPersonAimCameraNearClipThisUpdate; 
	bool	m_ShouldScriptOverrideThirdPersonAimCameraNearClipThisUpdate; 
	bool	m_ShouldScriptOverrideThirdPersonCameraRelativeHeadingLimitsThisUpdate;
	bool	m_ShouldScriptOverrideThirdPersonCameraRelativePitchLimitsThisUpdate;
	bool	m_ShouldScriptOverrideThirdPersonCameraOrbitDistanceLimitsThisUpdate;

	bool	m_ScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate;
	bool	m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate;
	bool	m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeEveryUpdate;
	bool	m_ScriptOverriddenFollowVehicleCameraHighAngleModeEveryUpdate;

	bool	m_ScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate;

	bool	m_ShouldAllowCustomVehicleDriveByCamerasThisUpdate;

	bool	m_ScriptForceUseTightSpaceCustomFramingThisUpdate;

	bool	m_ShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript;
	
	bool	m_IsScriptOverridingHintFovScalar; 
	bool	m_IsScriptOverridingHintDistanceScalar; 
	bool	m_IsScriptOverridingHintBaseOrbitPitchOffset; 
	bool	m_IsScriptOverridingHintCameraRelativeSideOffsetAdditive; 
	bool	m_IsScriptOverridingHintCameraRelativeVerticalOffsetAdditive; 
	bool	m_IsScriptForcingHintCameraBlendToFollowPedMediumViewMode;
	
	bool	m_UseVehicleCamStuntSettings;
	bool	m_PreviousUseVehicleCamStuntSettings;
	bool	m_ScriptRequestedVehicleCamStuntSettingsThisUpdate;
	bool	m_ScriptForcedVehicleCamStuntSettingsThisUpdate;
	bool	m_ScriptRequestDedicatedStuntCameraThisUpdate;

	atHashString m_DedicatedStuntCameraName;

	float	m_VehicleCamStuntSettingsTestTimer;

	camFrame m_CatchUpSourceFrame;
	float	m_CatchUpMaxDistanceToBlend;
	int		m_CatchUpBlendType;
	bool	m_ShouldInitCatchUpFromFrame;

#if __BANK
	float	m_DebugOverrideMotionBlurStrengthThisUpdate;
	float	m_DebugOverrideNearDofThisUpdate;
	float	m_DebugOverrideFarDofThisUpdate;
	bool	m_DebugOverridingDofThisUpdate;

	float	m_DebugOverriddenRelativeOrbitHeadingDegrees;
	float	m_DebugOverriddenRelativeOrbitPitchDegrees;

	Vector3	m_DebugHintOffset;
	u32		m_DebugHintAttackDuration;
	u32		m_DebugHintReleaseDuration;
	s32		m_DebugHintSustainDuration;
	bool	m_DebugIsHintOffsetRelative;

	float	m_DebugCatchUpDistance;

	char	m_DebugOverriddenFollowPedCameraName[g_MaxDebugCameraNameLength];
	char	m_DebugOverriddenFollowVehicleCameraName[g_MaxDebugCameraNameLength];
	s32		m_DebugOverriddenFollowPedCameraInterpolationDuration;
	s32		m_DebugOverriddenFollowVehicleCameraInterpolationDuration;
	bool	m_DebugShouldOverrideFollowPedCamera;
	bool	m_DebugShouldOverrideFollowVehicleCamera;

	bool	m_DebugShouldReportOutsideVehicle;

	bool	m_DebugShouldLogTightSpaceSettings;
	bool	m_DebugShouldLogActiveViewModeContext;

	bool	m_DebugForceUsingStuntSettings;

	bool	m_DebugForceUsingTightSpaceCustomFraming;

	bool	m_DebugHintActive;
#endif

	bool	m_IsHighAltitudeFovScalingPersistentlyEnabled;
	bool	m_IsHighAltitudeFovScalingDisabledThisUpdate;

	bool	m_ShouldDisplayReticule;
	bool	m_ShouldDimReticule;

	bool	m_WasRenderingMobilePhoneCameraOnPreviousUpdate;

	bool	m_TurretAligned;

	bool	m_ShouldDelayVehiclePovCamera;
	bool	m_ShouldDelayVehiclePovCameraOnPreviousUpdate;

	camBaseSwitchHelper* m_SwitchHelper;

	//Hint
	camEnvelope* m_RagdollBlendEnvelope;
	camEnvelope* m_HintEnvelope;
	Vector3	m_HintPos; 
	RegdConstEnt m_HintEnt; 	
	bool	m_IsRelativeHint;
	Vector3	m_HintOffset; 
	float	m_HintEnvelopeLevel; 
	atHashWithStringNotFinal m_HintType; 

	Matrix34 m_HintLookAtMat; 

	bool	m_IsScriptBlockingCodeHints; 
	bool 	m_HasUserSelectedCodeHint; 
	bool	m_IsCodeHintActive; 
	bool	m_CanActivateCodeHint; 
	bool	m_IsHintFirstUpdate;
	RegdConstPhy m_CodeHintEntity; 
	u32		m_CodeHintTimer; 

	u32		m_LastTimeAttachParentWasInWater;

	u32 m_TimeHintButtonInitalPressed; 
	bool m_IsCodeHintButtonDown; 
	bool m_LatchedState; 

	bool m_ShortRangePlayerSwitchLastUpdate;
	bool m_ShortRangePlayerSwitchFromFirstPerson;
	bool m_CustomFovBlendEnabled;
	bool m_UseSwitchCustomFovBlend;

	bool m_ForceAfterAimingBlend;
	bool m_DisableAfterAimingBlend;

	bool m_PreventVehicleEntryExit;

	bool m_ShouldPerformCustomTransitionAfterWarpFromVehicle;
	RegdConstEnt m_VehicleForCustomTransitionAfterWarp;

#if FPS_MODE_SUPPORTED
    u32  m_FirstPersonAnimatedDataStartTime;
	bool m_JustSwitchedFromSniperScope;
	bool m_IsFirstPersonModeEnabled;
	bool m_IsFirstPersonModeAllowed;
	bool m_DisableFirstPersonThisUpdate;
	bool m_DisableFirstPersonThisUpdateFromScript;
	bool m_bFirstPersonAnimatedDataInVehicle;
	bool m_bPovCameraAnimatedDataBlendoutPending;
	bool m_IsFirstPersonShooterLeftStickControl;
	bool m_WasFirstPersonModeEnabledPreviousFrame;
	bool m_AllowThirdPersonCameraFallbacks;
	bool m_ForceThirdPersonCameraForRagdollAndGetUps;
	bool m_ForceThirdPersonCameraWhenInputDisabled;
	bool m_EnableFirstPersonUseAnimatedData;
	bool m_bIsUsingFirstPersonControls;
	bool m_FirstPersonAnimatedDataRelativeToMover;
	bool m_bApplyRelativeOffsetInCameraSpace;
	bool m_IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch;
	bool m_OverrideFirstPersonCameraThisUpdate;
	bool m_ForceFirstPersonBonnetCamera;
	bool m_HasSwitchCameraTerminatedToForceFPSCamera; 
#if __BANK
	bool m_DebugWasFirstPersonDisabledThisUpdate;
	bool m_DebugWasFirstPersonDisabledThisUpdateFromScript;
#endif // __BANK
#endif // FPS_MODE_SUPPORTED

	//Forbid copy construction and assignment.
	camGameplayDirector(const camGameplayDirector& other);
	camGameplayDirector& operator=(const camGameplayDirector& other);
};

#endif // GAMEPLAY_DIRECTOR_H

