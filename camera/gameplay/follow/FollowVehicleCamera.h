//
// camera/gameplay/follow/FollowVehicleCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FOLLOW_VEHICLE_CAMERA_H
#define FOLLOW_VEHICLE_CAMERA_H

#include "camera/gameplay/follow/FollowCamera.h"

#include "camera/helpers/DampedSpring.h"

class camEnvelope;
class camFollowVehicleCameraMetadata;
class camVehicleCustomSettingsMetadata;
class camVehicleCustomSettingsMetadataDoorAlignmentSettings;

class camFollowVehicleCamera : public camFollowCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFollowVehicleCamera, camFollowCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camFollowVehicleCamera(const camBaseObjectMetadata& metadata);

public:
	~camFollowVehicleCamera();

	void			SetVehicleCustomSettings(const camVehicleCustomSettingsMetadata* settings) { m_VehicleCustomSettingsMetadata = settings; }

	const camVehicleCustomSettingsMetadataDoorAlignmentSettings& GetVehicleEntryExitSettings() const;

	bool			ShouldForceFirstPersonViewMode() const { return m_ShouldForceFirstPersonViewMode; }

	bool			ShouldUseStuntSettings() const { return m_ShouldUseStuntSettings; }
	void			SetShouldUseStuntSettings(bool val) { m_ShouldUseStuntSettings = val; }

	static void		InitTunables();

#if __BANK
	static void		AddWidgets(bkBank &bank);
    static bank_bool ms_ShouldDisablePullAround;
#endif // __BANK

private:
	virtual bool	Update();
	virtual void	GetAttachParentVelocity(Vector3& velocity) const;
	void			UpdateEnforcedFirstPersonViewMode();
	bool			ComputeIsVehicleAttachedToCargobob(const CVehicle& vehicle) const;
	const CEntity*	ComputeIsVehicleAttachedToMagnet(const CVehicle& vehicle) const;
	void			UpdateVerticalFlightModeBlendLevel();
	bool			ComputeIsVerticalFlightModeAvailable() const;
	void			UpdateCustomVehicleEntryExitBehaviour();
	void			UpdateHighSpeedShake();
	void			UpdateWaterEntryShake();
	float			ComputeWaterLevelForBoatWaterEntrySample() const;
	virtual bool	ComputeIsAttachParentInAir() const;
	virtual float	ComputeMinAttachParentApproachSpeedForPitchLock() const;
	virtual bool	ComputeShouldCutToDesiredOrbitDistanceLimits() const;
	virtual void	UpdateOrbitPitchLimits();
	virtual void	ApplyPullAroundOrientationOffsets(float& followHeading, float& followPitch);
	virtual void	ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const;
	virtual float	ComputeOrbitPitchOffset() const;
	virtual void	UpdateCollision(Vector3& cameraPosition, float &orbitHeadingOffset, float &orbitPitchOffset);
	virtual void	UpdateCollisionRootPosition(const Vector3& cameraPosition, float& collisionTestRadius);
	void			UpdateDuckingUnderOverheadCollision(const Vector3& collisionRootPositionOnPreviousUpdate, float preCollisionOrbitDistance,
						float collisionTestRadius);
	float			ComputeDuckUnderOverheadCollisionLevel(const Vector3& collisionRootPositionOnPreviousUpdate, float preCollisionOrbitDistance,
						float collisionTestRadius) const;
	virtual void	ComputeOrbitDistanceLimitsForBasePosition(Vector2& orbitDistanceLimitsForBasePosition) const;
	virtual void	ComputeLookBehindPullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual void	ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual bool	ComputeShouldPullAroundToBasicAttachParentMatrix() const;
	virtual float	ComputeDesiredFov();
	virtual float	ComputeMotionBlurStrength();
	virtual float	ComputeMotionBlurStrengthForImpulse();
	void			UpdateParachuteBehavior();
	bool			ShouldBlockHighSpeedEffects() const;

	const camFollowVehicleCameraMetadata& m_Metadata;

	const camVehicleCustomSettingsMetadata* m_VehicleCustomSettingsMetadata;

	camDampedSpring m_HandBrakeEffectSpring;
	camDampedSpring	m_HighSpeedShakeSpring;
	camDampedSpring	m_HighSpeedZoomSpring;
	camDampedSpring	m_HighSpeedZoomCutsceneBlendSpring;
	camDampedSpring	m_DuckUnderOverheadCollisionSpring;
	Vector3			m_CollisionRootPositionForMostRecentOverheadCollision;
	camEnvelope*	m_HandBrakeInputEnvelope;
	camEnvelope*	m_DuckUnderOverheadCollisionEnvelope;
	float			m_DuckLevelForMostRecentOverheadCollision;
	float			m_DuckUnderOverheadCollisionBlendLevel;
	float			m_WaterLevelForBoatWaterEntrySample;
	float			m_VerticalFlightModeBlendLevel;
	bool			m_ShouldForceFirstPersonViewMode;

	float			m_MotionBlurForAttachParentImpulseStartTime;
	float			m_MotionBlurPreviousAttachParentImpuse;
	float			m_MotionBlurPeakAttachParentImpulse;

	bool			m_ShouldUseStuntSettings;

	static bool		ms_ConsiderMetadataFlagForIgnoringBikeCollision;

	static bank_bool ms_ShouldCollide;

	//Forbid copy construction and assignment.
	camFollowVehicleCamera(const camFollowVehicleCamera& other);
	camFollowVehicleCamera& operator=(const camFollowVehicleCamera& other);
};

#endif // FOLLOW_VEHICLE_CAMERA_H
