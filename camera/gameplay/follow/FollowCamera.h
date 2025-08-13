//
// camera/gameplay/follow/FollowCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FOLLOW_CAMERA_H
#define FOLLOW_CAMERA_H

#include "camera/gameplay/ThirdPersonCamera.h"

#include "camera/helpers/DampedSpring.h"

class camEnvelope;
class camFollowCameraMetadata;
class camFollowCameraMetadataPullAroundSettings;

class camFollowCamera : public camThirdPersonCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFollowCamera, camThirdPersonCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camFollowCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camFollowCamera();

	void			IgnoreAttachParentMovementForOrientation()				{ m_ShouldIgnoreAttachParentMovementForOrientation = true; }
	void			IgnoreAttachParentMovementForOrientationThisUpdate()	{ m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate = true; }

	void			PersistAimBehaviour();

#if __BANK
	void			DebugComputeAttachParentVelocityToConsiderForPullAround(Vector3& attachParentVelocityToConsider) const { ComputeAttachParentVelocityToConsiderForPullAround(attachParentVelocityToConsider); }
#endif // __BANK

protected:
	virtual bool	Update();
	void			UpdateAttachParentInAirBlendLevel();
	void			UpdateAimBehaviourBlendLevel();
	void			UpdateWaterBobShake();
	virtual bool	ComputeIsAttachParentInAir() const { return false; }
	virtual float	ComputeAttachParentRollForBasePivotPosition();
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);
	virtual float	ComputeMinAttachParentApproachSpeedForPitchLock() const;
	void			UpdateAttachParentUpwardSpeedScalingOnGroundBlendLevel();
	void			UpdateAttachParentUpwardSpeedScalingInAirBlendLevel();
	float			ComputeAttachParentVerticalMoveSpeedToIgnore(const Vector3& attachParentVelocity) const;
	virtual float	ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const;
	virtual float	ComputeCustomFollowBlendLevel() const { return 1.0f; }
	void			ApplyConeLimitToOrbitOrientation(Vector3& orbitFront);
	virtual void	ApplyPullAroundOrientationOffsets(float& orbitHeading, float& orbitPitch);
	void			ComputePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual void	ComputeLookBehindPullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual void	ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual void	ComputeAttachParentVelocityToConsiderForPullAround(Vector3& attachParentVelocityToConsider) const;
	virtual void	ComputeDesiredPullAroundOrientation(const Vector3& attachParentVelocityToConsider, float orbitHeading, float orbitPitch,
						float& desiredHeading, float& desiredPitch) const;
	void			ComputeTurretEntryOrientation(float& desiredHeading, float& desiredPitch) const;
	bool			ComputeTurretEntryOrientationBlendLevel(float& UNUSED_PARAM(blendLevelForTurretEntry)) const;
	virtual bool	ComputeShouldPullAroundToBasicAttachParentMatrix() const;
	virtual float	ComputeDesiredRoll(const Matrix34& orbitMatrix);
	virtual float	ComputeDesiredFov();
	virtual const CEntity* ComputeDofOverriddenFocusEntity() const;
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	static void		BlendPullAroundSettings(float blendLevel, const camFollowCameraMetadataPullAroundSettings& a,
						const camFollowCameraMetadataPullAroundSettings& b, camFollowCameraMetadataPullAroundSettings& result);

	const camFollowCameraMetadata& m_Metadata;

	camDampedSpring	m_HighAltitudeZoomSpring;
	camDampedSpring m_HeadingPullAroundSpring;
	camDampedSpring m_PitchPullAroundSpring;
	camDampedSpring	m_RollSpring;
	camEnvelope*	m_AttachParentInAirEnvelope;
	camEnvelope*	m_AttachParentUpwardSpeedScalingOnGroundEnvelope;
	camEnvelope*	m_AttachParentUpwardSpeedScalingInAirEnvelope;
	camEnvelope*	m_AimBehaviourEnvelope;
	float			m_AttachParentInAirBlendLevel;
	float			m_AttachParentUpwardSpeedScalingOnGroundBlendLevel;
	float			m_AttachParentUpwardSpeedScalingInAirBlendLevel;
	float			m_AimBehaviourBlendLevel;
	bool			m_ShouldIgnoreAttachParentMovementForOrientation;
	bool			m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate;
	bool			m_ShouldCacheOrientationForTurretOnPreviousUpdate;
	float			m_CachedHeadingForTurret;
	float			m_CachedPitchForTurret;

private:
	//Forbid copy construction and assignment.
	camFollowCamera(const camFollowCamera& other);
	camFollowCamera& operator=(const camFollowCamera& other);
};

#endif // FOLLOW_CAMERA_H
