//
// camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_PED_ASSISTED_AIM_CAMERA_H
#define THIRD_PERSON_PED_ASSISTED_AIM_CAMERA_H

#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/system/CameraMetadata.h"

class camEnvelope;
class camThirdPersonPedAssistedAimCameraMetadata;
class camThirdPersonPedAssistedAimCameraRunningShakeSettings;

class camThirdPersonPedAssistedAimCamera : public camThirdPersonPedAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonPedAssistedAimCamera, camThirdPersonPedAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual const Vector3&	GetBaseFront() const								{ return m_CinematicMomentBlendedBaseFront; }
	bool					ShouldAffectAiming() const							{ return IsRunningFocusPlayerCinematicMoment(); }
	const camFrame&			GetFrameForAiming() const							{ return m_FrameForAiming; }

	virtual float	GetRecoilShakeAmplitudeScaling() const;
	virtual void	ComputeBaseLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition, const Vector3& baseFront,
											const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition, const Vector3& postCollisionCameraPosition,
											float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const
	{
		//Bypass the lock-on-specific look-at behaviour.
		camThirdPersonCamera::ComputeBaseLookAtFront(collisionRootPosition, basePivotPosition, baseFront, pivotPosition, preCollisionCameraPosition,
			postCollisionCameraPosition, preToPostCollisionBlendValue, wasCameraConstrainedOffAxis, lookAtFront);
	}

	bool			GetIsCinematicShootingCamera() const;

	bool			GetIsCinematicKillShotRunning() const						{ return (m_CinematicCutBlendLevel >= SMALL_FLOAT) && m_IsCinematicMomentAKillShot; }
	bool			GetIsCinematicReactionShotRunning() const					{ return IsRunningFocusPlayerCinematicMoment(); }
	bool			GetIsCinematicMomentRunning() const							{ return (m_CinematicMomentBlendLevel >= SMALL_FLOAT); }
	float			GetCinematicMomentBlendLevel() const						{ return m_CinematicMomentBlendLevel; }

	void			GetCinematicMomentTargetPosition(Vector3& position) const	{ position = m_TargetPositionForCinematicMoment; }
	const CPed*		GetCinematicMomentTargetEntity() const						{ return m_LastValidTargetPedForCinematicMoment; }
	bool			GetIsCinematicMomentCameraOnRight() const					{ return m_IsCameraOnRightForCinematicMoment; }

#if __BANK
	virtual void	DebugRender();
	static void		AddWidgets(bkBank &bank);
#endif // __BANK

private:
	camThirdPersonPedAssistedAimCamera(const camBaseObjectMetadata& metadata);
	~camThirdPersonPedAssistedAimCamera();

	virtual bool	Update();
	void			UpdateCoverFraming();
	void			UpdateShootingFocus();
	void			UpdateCinematicMoment();
	void			UpdateTargetPedForCinematicMoment();
	void			ComputeTargetPositionForCinematicMoment(const CPed* targetPed, Vector3& position) const;
	bool			ComputeHasLineOfSight(const Vector3& cameraPosition, const Vector3& lookAtPosition, const CEntity* targetEntity) const;
	bool			ComputeIsCameraOnRightOfActionLine() const;
	void			UpdateCinematicMomentLookAroundBlocking();
	bool			IsRunningFocusPlayerCinematicMoment() const;
	void			UpdateCinematicCut();
	virtual bool	ComputeShouldForceAttachPedPelvisOffsetToBeConsidered() const;
	//Bypass the aim-specific limits and simply use the values specified in metadata.
	virtual void	UpdateOrbitPitchLimits() { camThirdPersonCamera::UpdateOrbitPitchLimits(); }
	virtual bool	ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const;
	virtual void	ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const;
	//Bypass the weapon-specific FOV and simply use the value specified in metadata.
	virtual float	ComputeBaseFov() const { return camThirdPersonCamera::ComputeBaseFov(); }
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);
	void			ComputeDesiredLockOnOrientation(const float verticalFov, float& desiredOrbitHeading, float& desiredOrbitPitch);
	void			ComputeDesiredLockOnOrientationFramingTarget(const float verticalFov, float& desiredOrbitHeading, float& desiredOrbitPitch) const;
	void			ComputeDesiredLockOnOrientationFramingPlayer(float& desiredOrbitHeading, float& desiredOrbitPitch);
	virtual void	ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const;
	virtual void	UpdatePivotPosition(const Matrix34& orbitMatrix);
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	virtual float	ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const;
	virtual float	ComputeDesiredFov();
	virtual float	ComputeCollisionRootPositionFallBackToPivotBlendValue() const;
	virtual void	UpdateReticuleSettings();
	bool			ComputeHasLosToTarget() const;
	float			UpdateRunningShakeActivityScalingFactor();
	float			UpdateWalkingShakeActivityScalingFactor();
	void			UpdateMovementShake(const camThirdPersonPedAssistedAimCameraRunningShakeSettings& settings, const float activityShakeScalingFactor BANK_ONLY(, bool shouldForceOn = false));
	virtual void	ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	enum eCinematicMomentType
	{
		FOCUS_TARGET = 0,
		FOCUS_PLAYER,
		NUM_CINEMATIC_MOMENT_TYPES
	};

	const camThirdPersonPedAssistedAimCameraMetadata& m_Metadata;

	camGameplayDirector::tCoverSettings	m_CoverSettingsToConsider;
	camThirdPersonPedAssistedAimCameraCinematicMomentSettings m_CinematicMomentSettings;
	camThirdPersonPedAssistedAimCameraShootingFocusSettings m_ShootingFocusSettings;
	camFrame		m_FrameForAiming;
	camDampedSpring	m_HeadingAlignmentSpring;
	camDampedSpring	m_PitchAlignmentSpring;
	camDampedSpring m_PivotPositionSpring;
	camDampedSpring m_DesiredPivotErrorSpring;
	camDampedSpring m_SpringConstantSpring;
	camDampedSpring m_ShootingFocusSpring;
	camDampedSpring m_RunningShakeActivitySpring;
	camDampedSpring m_FOVScalingSpring;
	camDampedSpring m_LookAroundBlendLevelSpring;
	camDampedSpring m_CoverFramingSpring;
	Vector3			m_PivotPositionOnPreviousUpdate;
	Vector3			m_TargetPositionForCinematicMoment;
	Vector3			m_CinematicMomentBlendedBaseFront;
	Vector3			m_BaseFrontWithoutOrientationChanges;
	camEnvelope*	m_CinematicMomentBlendEnvelope;
	RegdConstPed	m_TargetPedOnPreviousUpdateForShootingFocus;
	RegdConstPed	m_LastRejectedTargetPedForCinematicMoment;
	RegdConstPed	m_LastValidTargetPedForCinematicMoment;
	u32				m_FirstUpdateOfCinematicCut;
	u32				m_CinematicMomentDuration;
	float			m_CinematicMomentBlendLevel;
	float			m_CinematicMomentBlendLevelOnPreviousUpdate;
	float			m_OrbitHeadingToApply;
	float			m_OrbitPitchToApply;
	float			m_OrbitHeadingForFramingPlayer;
	float			m_OrbitPitchForFramingPlayer;
	float			m_CinematicCutBlendLevel;
	float			m_CinematicCutBlendLevelOnPreviousUpdate;
	eCinematicMomentType m_CinematicMomentType;
	bool			m_HadLosToLockOnTargetOnPreviousUpdate;
	bool			m_IsSideOffsetOnRight;
	bool			m_IsCameraOnRightForCinematicMoment;
	bool			m_IsCinematicMomentAKillShot;
	bool			m_HasSetDesiredOrientationForFramingPlayer;

	static u32		m_TimeAtStartOfLastCinematicMoment;

#if __BANK
	static s32		ms_DebugOverriddenCinematicMomentDuration;
	static bool		ms_DebugForceFullShootingFocus;
	static bool		ms_DebugForceFullRunningShake;
	static bool		ms_DebugForceFullWalkingShake;
	static bool		ms_DebugDrawTargetPosition;
	static bool		ms_DebugForcePlayerFraming;
	static bool		ms_DebugForceCinematicKillShotOff;
	static bool		ms_DebugForceCinematicKillShotOn;
	static bool		ms_DebugAlwaysTriggerCinematicMoments;
#endif // __BANK

	//Forbid copy construction and assignment.
	camThirdPersonPedAssistedAimCamera(const camThirdPersonPedAssistedAimCamera& other);
	camThirdPersonPedAssistedAimCamera& operator=(const camThirdPersonPedAssistedAimCamera& other);
};

#endif // THIRD_PERSON_PED_ASSISTED_AIM_CAMERA_H
