//
// camera/gameplay/ThirdPersonCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_CAMERA_H
#define THIRD_PERSON_CAMERA_H

#include "vector/quaternion.h"

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "physics/WorldProbe/worldprobe.h"

class camCatchUpHelper;
class camControlHelper;
class camEnvelope;
class camHintHelper;
class camThirdPersonCameraMetadata;
class camThirdPersonCameraMetadataBasePivotPosition;
class camVehicleCustomSettingsMetadata; 

class camThirdPersonCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper;	//Allow the factory helper class to access the protected constructor.
	friend class camCatchUpHelper;						//Allow the catch-up helper class to access protected helper functions.

protected:
	struct tNearbyVehicleSettings
	{
		RegdConstVeh	m_Vehicle;
		u32				m_DetectionTime;
	};

	camThirdPersonCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camThirdPersonCamera();

	const camControlHelper* GetControlHelper() const	{ return m_ControlHelper; }
	camControlHelper*		GetControlHelper()			{ return m_ControlHelper; }

	const camHintHelper*	GetHintHelper() const		{ return m_HintHelper; }
	camHintHelper*			GetHintHelper()				{ return m_HintHelper; }

	const camCatchUpHelper*	GetCatchUpHelper() const	{ return m_CatchUpHelper; }
	camCatchUpHelper*		GetCatchUpHelper()			{ return m_CatchUpHelper; }

	virtual const Vector3& GetBaseFront() const			{ return m_BaseFront; }
	const Vector3&	GetBaseAttachPosition() const		{ return m_BaseAttachPosition; }
	const Vector3&	GetBasePivotPosition() const		{ return m_BasePivotPosition; }
	const Vector3&	GetPivotPosition() const			{ return m_PivotPosition; }
	const Vector3&	GetCollisionRootPosition() const	{ return m_CollisionRootPosition; }
	const Vector3&  GetPreCollisionCameraPosition() const { return m_PreCollisionCameraPosition; }
	const Vector3&  GetBoundingBoxMax() const			{ return m_AttachParentBoundingBoxMax; }
	const Vector3&  GetBoundingBoxMin() const			{ return m_AttachParentBoundingBoxMin; }
	float GetBoundingBoxExtensionForOrbitDistance() const { return m_BoundingBoxExtensionForOrbitDistance; }
	const Vector3&	GetLookAtPosition() const			{ return m_LookAtPosition; }
	const Vector2&	GetOrbitPitchLimits() const			{ return m_OrbitPitchLimits; }

	const Vector3&	GetBaseAttachVelocityToConsider() const	 { return m_BaseAttachVelocityToConsider; }

	bool			IsBuoyant() const					{ return m_IsBuoyant; }
	void			BypassBuoyancyThisUpdate();

	virtual void	InterpolateFromCamera(camBaseCamera& sourceCamera, u32 duration, bool shouldDeleteSourceCamera = false);

	void			SetRelativeHeading(float heading, float smoothRate = 1.0f, bool shouldIgnoreInput = true, bool shouldApplyRelativeToIdealHeading = false, bool shouldFlagAsCut = true)
						{ m_RelativeHeadingToApply = heading; m_RelativeHeadingSmoothRate = smoothRate; m_ShouldIgnoreInputForRelativeHeading = shouldIgnoreInput; m_ShouldApplyRelativeHeadingWrtIdeal = shouldApplyRelativeToIdealHeading; m_ShouldFlagRelativeHeadingChangeAsCut = shouldFlagAsCut; m_ShouldApplyRelativeHeading = true; }
	void			SetRelativePitch(float pitch, float smoothRate = 1.0f, bool shouldIgnoreInput = true, bool shouldFlagAsCut = true)
						{ m_RelativePitchToApply = pitch; m_RelativePitchSmoothRate = smoothRate; m_ShouldIgnoreInputForRelativePitch = shouldIgnoreInput; m_ShouldFlagRelativePitchChangeAsCut = shouldFlagAsCut; m_ShouldApplyRelativePitch = true; }

	void			ResetFullAttachParentMatrixForRelativeOrbitTimer() { m_ShouldResetFullAttachParentMatrixForRelativeOrbitTimer = true; }

	void			CloneBaseOrientation(const camBaseCamera& sourceCamera, bool shouldFlagAsCut = true, bool shouldUseInterpolatedBaseFront = false, bool ignoreCurrentControlHelperLookBehindInput = false);
	void			CloneBasePosition(const camBaseCamera& sourceCamera);
	void			CloneControlSpeeds(const camBaseCamera& sourceCamera);

#if ! __FINAL
    bool            PreCatchupFromFrameCheck(const camFrame& sourceFrame) const;
#endif //!__FINAL

	bool			CatchUpFromFrame(const camFrame& sourceFrame, float maxDistanceToBlend = 0.0f, int blendType = -1);
	void			AbortCatchUp();

	void			ComputeDesiredOrbitDistanceLimits(float& minOrbitDistance, float& maxOrbitDistance, float orbitPitchOffset,
						bool ShouldIgnoreVerticalPivotOffsetForFootRoom, bool includeAttachments = true, s32 overriddenViewMode = -1) const;
	float			ComputePreToPostCollisionLookAtOrientationBlendValue() const;
	void			ComputeLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition, const Vector3& baseFront,
						const Vector3& orbitFront, const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition, const Vector3& postCollisionCameraPosition,
						float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const;
	virtual void	ComputeBaseLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition, const Vector3& baseFront,
						const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition, const Vector3& postCollisionCameraPosition,
						float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const;

	void			StartCustomFovBlend(float fovDelta, u32 duration);

#if __BANK
	virtual void	DebugRender();
#endif // __BANK

	static void		ComputeLookAtPositionFromFront(const Vector3& lookAtFront, const Vector3& basePivotPosition, const Vector3& orbitFront,
						const Vector3& postCollisionCameraPosition, Vector3& lookAtPosition);

protected:
	virtual bool	Update();
	virtual void	UpdateControlHelper();
	virtual void	FlagCutsForOrbitOverrides();
	void			CreateOrDestroyHintHelper();
	virtual void	UpdateAttachParentMatrix();
	void			UpdateAttachParentMatrixToConsiderForRelativeOrbit();
	bool			ComputeShouldApplyFullAttachParentMatrixForRelativeOrbit() const;
	bool			UpdateAttachParentBoundingBox();
	bool			ComputeHighestPositionOnEntityRelativeToOtherEntity(const CEntity& entity, const CEntity& otherEntity,
						Vector3& highestRelativePosition) const;
	void			UpdateAttachPedPelvisOffsetSpring();
	virtual bool	ComputeShouldForceAttachPedPelvisOffsetToBeConsidered() const { return false; }
	virtual void	UpdateBaseAttachPosition();
	void			UpdateBaseAttachVelocities();
	virtual void	GetAttachParentVelocity(Vector3& velocity) const;
	const CEntity*	ComputeAttachParentAttachEntity() const;
	const CEntity*	ComputeAttachParentGroundEntity() const;
	void			UpdateTightSpaceSpring();
	void			UpdateStealthZoom();
	virtual void	UpdateBasePivotPosition();
	float			ComputeBaseAttachPositionOffsetForBasePivotPosition() const;
	virtual float	ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const;
	virtual float	ComputeAttachPedPelvisOffsetForShoes() const { return 0.0f; }
	virtual float	ComputeExtraOffsetForHatsAndHelmets() const { return 0.0f; }
	virtual float	ComputeAttachParentRollForBasePivotPosition();
	virtual void	UpdateOrbitPitchLimits();
	virtual void	UpdateLockOn() {}
	virtual float	UpdateOrbitDistance();
	void			UpdateOrbitDistanceLimits();
	void			ComputeCustomScreenRatiosForFootRoomLimits(float& screenRatioMin, float& screenRatioMax) const;
	virtual void	ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const;
	virtual float	ComputeBasePivotHeightScalingForFootRoom() const;
	virtual float	ComputeBaseFov() const;
	virtual bool	ComputeShouldCutToDesiredOrbitDistanceLimits() const;
	float			ComputeOrbitDistanceLimitsSpringConstant() const;
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);
	void			ComputeAttachParentLocalXYVelocity(Vector3& attachParentLocalXYVelocity) const;
	virtual void	ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const;
	virtual void	ApplyOverriddenOrbitOrientation(float& orbitHeading, float& orbitPitch);
	void			LimitOrbitHeading(float& heading) const;
	virtual float	ComputeIdealOrbitHeadingForLimiting() const;
	void			LimitOrbitPitch(float& pitch) const;
	virtual float	ComputeOrbitPitchOffset() const;
	virtual float	ComputeBaseOrbitPitchOffset() const;
	virtual void	UpdatePivotPosition(const Matrix34& orbitMatrix);
    void            UpdateBasePivotSpring();
	void			ComputeOrbitRelativePivotOffset(const Vector3& orbitFront, Vector3& orbitRelativeOffset) const;
	void			ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const;
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	void			UpdateBuoyancyState(float orbitDistance);
	bool			ComputeIsBuoyant(float orbitDistance) const;
	virtual void	UpdateCollision(Vector3& cameraPosition, float &orbitHeadingOffset, float &orbitPitchOffset);
	virtual bool	ComputeShouldPushBeyondAttachParentIfClipping() const;
	void			UpdateVehicleOnTopOfVehicleCollisionBehaviour();
	void			ComputeVehiclesOnTopOfVehicle(const CVehicle& vehicleToConsiderAsBottom, atArray<const CVehicle*>& vehiclesOnTopList) const;
	bool			ComputeIsVehicleOnTopOfVehicle(const CVehicle& vehicleToConsiderAsBottom, const CVehicle& vehicleToConsiderOnTop) const;
	virtual float	ComputeMaxSafeCollisionRadiusForAttachParent() const;
	virtual void	UpdateCollisionOrigin(float& collisionTestRadius);
	virtual void	ComputeFallBackForCollisionOrigin(Vector3& fallBackPosition) const;
	virtual void	UpdateCollisionRootPosition(const Vector3& cameraPosition, float& collisionTestRadius);
	virtual void	ComputeCollisionFallBackPosition(const Vector3& cameraPosition, float& collisionTestRadius,
						WorldProbe::CShapeTestCapsuleDesc& capsuleTest, WorldProbe::CShapeTestResults& shapeTestResults,
						Vector3& collisionFallBackPosition);
	virtual float	ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const;
	virtual float	ComputeCollisionRootPositionFallBackToPivotBlendValue() const;
	virtual void	ComputeOrbitDistanceLimitsForBasePosition(Vector2& orbitDistanceLimitsForBasePosition) const;
	void			UpdateLookAt(const Vector3& orbitFront);
	void			UpdateRoll(const Matrix34& orbitMatrix);
	virtual float	ComputeDesiredRoll(const Matrix34& orbitMatrix);
	virtual void	UpdateLensParameters();
	void			UpdateFov();
	virtual float	ComputeDesiredFov();
	void			UpdateMotionBlur();
	virtual float	ComputeMotionBlurStrength();
	float			ComputeMotionBlurStrengthForMovement();
	float			ComputeMotionBlurStrengthForDamage();
	void			CacheOrbitHeadingLimitSettings(float heading);
	void			UpdateFirstPersonCoverSettings();
	void			InitVehicleCustomSettings(); 
	virtual void	UpdateDof();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;
	void			UpdateCustomFovBlend(float fCurrentFov);
	void			UpdateDistanceTravelled();
	float			ComputeAimBlendBasedOnDistanceTravelled() const;
	virtual bool	ReorientCameraToPreviousAimDirection();
	virtual bool	GetIsTurretCam() const { return false; }
	virtual bool	ShouldPreventAnyInterpolation() const;
	bool			ShouldUseTightSpaceCustomFraming() const;

	float GetAttachParentHeightScalingForCameraRelativeVerticalOffset() const;
	float GetAttachParentMatrixForRelativeOrbitSpringConstant() const;
	float GetAttachParentMatrixForRelativeOrbitSpringDampingRatio() const;
	bool ShouldOrbitRelativeToAttachParentOrientation() const;
	bool ShouldPersistOrbitOrientationRelativeToAttachParent() const;
	bool ShouldApplyInAttachParentLocalSpace() const;

	bool ShouldApplyCollisionSettingsForTripleHeadInInteriors() const;
	bool IsArrestingBehaviorActive() const;

	const camThirdPersonCameraMetadata& m_Metadata;

	atArray<tNearbyVehicleSettings> m_VehiclesOnTopOfAttachVehicleList;
	camDampedSpring m_AttachParentMatrixForRelativeOrbitSpring;
	camDampedSpring m_TightSpaceSpring;
	camDampedSpring m_TightSpaceHeightSpring;
	camDampedSpring	m_AttachPedPelvisOffsetSpring;
	camDampedSpring	m_BasePivotPositionRollSpring;
	camDampedSpring	m_MinOrbitDistanceSpring;
	camDampedSpring	m_MaxOrbitDistanceSpring;
	camDampedSpring	m_CollisionTestRadiusSpring;
	camDampedSpring	m_CollisionFallBackSpring;
	camDampedSpring	m_CollisionRootSpring;
	camDampedSpring	m_AttachParentRollSpring;
	camDampedSpring	m_StealthZoomSpring;
    camDampedSpring m_BasePivotSpring;
	Matrix34		m_AttachParentMatrix;
	Matrix34		m_AttachParentMatrixToConsiderForRelativeOrbit;
	Quaternion		m_PreviousAttachParentOrientationDeltaForRelativeOrbit;
	Quaternion		m_AttachParentOrientationOnPreviousUpdate;
	Vector3			m_AttachParentBoundingBoxMin;
	Vector3			m_AttachParentBoundingBoxMax;
	Vector3			m_BaseAttachPositionOnPreviousUpdate;
	Vector3			m_BaseAttachPosition;
	Vector3			m_BaseAttachVelocityToConsider;
	Vector3			m_BaseAttachVelocityToIgnoreForMostRecentEntity;
	Vector3			m_BaseAttachVelocityToIgnore;
	Vector3			m_BasePivotPosition;
	Vector3			m_BaseFront;
	Vector3			m_BasePosition;
	Vector3			m_PivotPosition;
	Vector3			m_PreCollisionCameraPosition;
	Vector3			m_CollisionOrigin;
	Vector3			m_CollisionRootPosition;
	Vector3			m_LookAtPosition;
	Vector2			m_RelativeOrbitHeadingLimits;
	Vector2			m_OrbitPitchLimits;
	camControlHelper* m_ControlHelper;
	camHintHelper*	m_HintHelper;
	camCatchUpHelper* m_CatchUpHelper;
	camEnvelope*	m_BaseAttachVelocityToIgnoreEnvelope;
	const camVehicleCustomSettingsMetadata* m_CustomVehicleSettings; 
	u32				m_MostRecentTimeFullAttachParentMatrixBlockedForRelativeOrbit;
	u32				m_DesiredBuoyancyStateTransitionTime;
	u32				m_BuoyancyStateTransitionTime;
	u32				m_MotionBlurForAttachParentDamageStartTime;
	float			m_BoundingBoxExtensionForOrbitDistance;
	float			m_OrbitDistanceExtensionForTowedVehicle;
	float			m_RelativeHeadingToApply;
	float			m_RelativePitchToApply;
	float			m_RelativeHeadingSmoothRate;
	float			m_RelativePitchSmoothRate;
	float			m_RelativeHeadingForLimitingOnPreviousUpdate;
	float			m_MotionBlurPreviousAttachParentHealth;
	float			m_MotionBlurPeakAttachParentDamage;
	float			m_BlendFovDelta;
	u32				m_BlendFovDeltaStartTime;
	u32				m_BlendFovDeltaDuration;
	float			m_DistanceTravelled;
	u32				m_AfterAimingActionModeBlendStartTime;
	bool			m_ShouldApplyFullAttachParentMatrixForRelativeOrbit			: 1;
	bool			m_ShouldApplyRelativeHeading								: 1;
	bool			m_ShouldApplyRelativePitch									: 1;
	bool			m_ShouldResetFullAttachParentMatrixForRelativeOrbitTimer	: 1;
	bool			m_ShouldIgnoreInputForRelativeHeading						: 1;
	bool			m_ShouldIgnoreInputForRelativePitch							: 1;
	bool			m_ShouldApplyRelativeHeadingWrtIdeal						: 1;
	bool			m_ShouldFlagRelativeHeadingChangeAsCut						: 1;
	bool			m_ShouldFlagRelativePitchChangeAsCut						: 1;
	bool			m_DesiredBuoyancyState										: 1;
	bool			m_IsBuoyant													: 1;
	bool			m_ShouldBypassBuoyancyStateThisUpdate						: 1;
	bool			m_WasBypassingBuoyancyStateOnPreviousUpdate					: 1;	

private:
	//Forbid copy construction and assignment.
	camThirdPersonCamera(const camThirdPersonCamera& other);
	camThirdPersonCamera& operator=(const camThirdPersonCamera& other);
};

#endif // THIRD_PERSON_CAMERA_H
