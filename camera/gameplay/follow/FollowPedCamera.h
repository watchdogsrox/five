//
// camera/gameplay/follow/FollowPedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FOLLOW_PED_CAMERA_H
#define FOLLOW_PED_CAMERA_H

#include "camera/gameplay/follow/FollowCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/system/CameraMetadata.h"

class camEnvelope;

class camFollowPedCamera : public camFollowCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFollowPedCamera, camFollowCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camFollowPedCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camFollowPedCamera();

	void			SetScriptForceLadder(bool flag) { m_ScriptForceLadder = flag; }
	
	static bool		ComputeShouldApplyRagdollBehavour(const CEntity &Entity, bool shouldIncludeAllGetUpStates = false);

protected:
	virtual bool	Update();
	virtual void	UpdateLensParameters();
	void			UpdateRagdollBlendLevel();
	void			UpdateVaultBlendLevel();
	bool			ComputeShouldApplyVaultBehavour(bool& isBonnetSlide) const;
	void			UpdateClimbUpBehaviour();
	bool			ComputeShouldApplyClimbUpBehaviour() const;
	void			UpdateMeleeBlendLevel();
	void			UpdateSuperJumpBlendLevel();
	void			UpdateCustomVehicleEntryExitBehaviour();
	void			UpdateCoverExitBehaviour();
	void			UpdateAssistedMovementAlignment();
	void			UpdateLadderAlignment();
	void			UpdateRappellingAlignment();
	void			UpdateRunningShake();
	void			UpdateSwimmingShake();
	void			UpdateDivingShake();
	void			UpdateHighFallShake();
	virtual void	UpdateControlHelper();
	void			UpdateCustomViewModeBlendLevels(float* viewModeBlendLevels);
	float			ComputeDesiredCustomNearToMediumViewModeBlendLevel(bool shouldCut);
	bool			ComputeShouldBlendNearToMediumViewModes() const;
	void			ComputeBlendedCustomViewModeSettings(const camFollowPedCameraMetadataCustomViewModeSettings* customFramingForViewModes,
						const float* viewModeBlendLevels, camFollowPedCameraMetadataCustomViewModeSettings& blendedCustomViewModeSettings) const;
	virtual void	UpdateAttachParentMatrix();
	bool			ComputeAttachPedRootBoneMatrix(Matrix34& matrix) const;
	virtual void	UpdateBaseAttachPosition();
	virtual float	ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const;
	virtual float	ComputeAttachPedPelvisOffsetForShoes() const;
	virtual float	ComputeExtraOffsetForHatsAndHelmets() const;
	virtual void	UpdateOrbitPitchLimits();
	virtual void	ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const;
	virtual float	ComputeBaseOrbitPitchOffset() const;
	virtual bool	ComputeIsAttachParentInAir() const;
	virtual float	ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const;
	virtual float	ComputeCustomFollowBlendLevel() const;
	virtual void	ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const;
	virtual void	ComputeAttachParentVelocityToConsiderForPullAround(Vector3& attachParentVelocityToConsider) const;
	virtual void	ComputeDesiredPullAroundOrientation(const Vector3& attachParentVelocityToConsider, float orbitHeading, float orbitPitch,
						float& desiredHeading, float& desiredPitch) const;
	virtual float	ComputeIdealOrbitHeadingForLimiting() const;
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	virtual bool	ComputeShouldPushBeyondAttachParentIfClipping() const;
	virtual float	ComputeMaxSafeCollisionRadiusForAttachParent() const;
	virtual void	UpdateCollisionOrigin(float& collisionTestRadius);
	virtual void	ComputeFallBackForCollisionOrigin(Vector3& fallBackPosition) const;
	void			ComputeValidNearbyVehiclesInRagdoll(float collisionTestRadius, atArray<const CEntity*>& nearbyVehicleList) const;

	const camFollowPedCameraMetadata& m_Metadata;

	atArray<tNearbyVehicleSettings> m_NearbyVehiclesInRagdollList;
	camFollowPedCameraMetadataCustomViewModeSettings m_CustomViewModeSettings;
	camFollowPedCameraMetadataCustomViewModeSettings m_CustomViewModeSettingsInTightSpace;
	camFollowPedCameraMetadataCustomViewModeSettings m_CustomViewModeSettingsAfterAiming;
	camFollowPedCameraMetadataCustomViewModeSettings m_CustomViewModeSettingsAfterAimingInTightSpace;
	camDampedSpring	m_OrbitPitchLimitsForOverheadCollisionSpring;
	camDampedSpring	m_ClimbUpBlendSpring;
	camDampedSpring	m_RunRatioSpring;
	camDampedSpring	m_CustomNearToMediumViewModeBlendSpring;
	camDampedSpring	m_CustomViewModeBlendForCutsceneBlendOutSpring;
 	Vector3			m_RelativeAttachParentMatrixPositionForVehicleEntry;
	Vector3			m_RelativeBaseAttachPositionForRagdoll;
	Vector3			m_AssistedMovementLookAheadRouteDirection;
	Vector3			m_LadderMountTargetPosition;
	camEnvelope*	m_AssistedMovementAlignmentEnvelope;
	camEnvelope*	m_LadderAlignmentEnvelope;
	camEnvelope*	m_RagdollBlendEnvelope;
	camEnvelope*	m_CustomNearToMediumViewModeBlendEnvelope;
	camEnvelope*	m_VaultBlendEnvelope;
	camEnvelope*	m_MeleeBlendEnvelope;
	float			m_RagdollBlendLevel;
	float			m_VaultBlendLevel;
	float			m_MeleeBlendLevel;
	float			m_SuperJumpBlendLevel;
	float			m_AssistedMovementAlignmentBlendLevel;
	float			m_AssistedMovementLocalRouteCurvature;
	float			m_LadderAlignmentBlendLevel;
	float			m_LadderHeading;
	float			m_DiveHeight;
	float			m_CustomNearToMediumViewModeBlendLevel;
	bool			m_IsAttachPedMountingLadder					: 1;
	bool			m_WasAttachPedDismountingLadder				: 1;
	bool			m_ShouldAttachToLadderMountTargetPosition	: 1;
	bool			m_ShouldInhibitPullAroundOnLadder			: 1;
	bool			m_ShouldInhibitCustomPullAroundOnLadder		: 1;
	bool			m_ScriptForceLadder							: 1;
	bool			m_ShouldUseCustomViewModeSettings			: 1;

	//Forbid copy construction and assignment.
	camFollowPedCamera(const camFollowPedCamera& other);
	camFollowPedCamera& operator=(const camFollowPedCamera& other);
};

#endif // FOLLOW_PED_CAMERA_H
