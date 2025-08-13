//
// camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_PED_AIM_IN_COVER_CAMERA_H
#define THIRD_PERSON_PED_AIM_IN_COVER_CAMERA_H

#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/DampedSpring.h"

class camThirdPersonPedAimInCoverCameraMetadata;

class camThirdPersonPedAimInCoverCamera : public camThirdPersonPedAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonPedAimInCoverCamera, camThirdPersonPedAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	static void		InitClass();
#if __BANK
	static void		AddWidgets(bkBank& bank);
#endif // __BANK

private:
	camThirdPersonPedAimInCoverCamera(const camBaseObjectMetadata& metadata);

	virtual bool	Update();
	void			UpdateFacingDirection();
	void			UpdateCornerBehaviour();
	void			UpdateAimingBehaviour();
	void			UpdateLowCoverBehaviour();
	void			UpdateInitialCoverAlignment();
	virtual void	UpdateBasePivotPosition();
	virtual float	ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const;
	virtual void	ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const;
	virtual float	ComputeBaseFov() const;
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);
	bool			ComputeShouldAlignWithCover() const;
	virtual float	ComputeIdealOrbitHeadingForLimiting() const;
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	virtual void	ComputeCollisionFallBackPosition(const Vector3& cameraPosition, float& collisionTestRadius,
						WorldProbe::CShapeTestCapsuleDesc& capsuleTest, WorldProbe::CShapeTestResults& shapeTestResults,
						Vector3& collisionFallBackPosition);
	virtual float	ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const;
	virtual float	ComputeDesiredFov();
	virtual bool	ComputeShouldUseLockOnAiming() const;
	virtual float	ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const;
	virtual void	UpdateReticuleSettings();
	virtual bool	ComputeShouldDisplayReticuleAsInterpolationSource() const;
	bool			ComputeShouldHideReticuleDueToSpringMovement() const;
	virtual void	UpdateDof();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	const camThirdPersonPedAimInCoverCameraMetadata& m_Metadata;

	camDampedSpring	m_FacingDirectionScalingSpring;
	camDampedSpring	m_HeadingAlignmentSpring;
	camDampedSpring	m_PitchAlignmentSpring;
	camDampedSpring	m_StepOutPositionBlendSpring;
	camDampedSpring	m_CoverRelativeStepOutPositionXSpring;
	camDampedSpring	m_CoverRelativeStepOutPositionYSpring;
	camDampedSpring	m_BlindFiringOnHighCornerBlendSpring;
	camDampedSpring	m_LowCoverFramingBlendSpring;
	camDampedSpring	m_AimingBlendSpring;
	Vector3			m_DampedStepOutPosition;
	u32				m_MostRecentTimeTurningOnNarrowCover;
	u32				m_NumFramesAimingWithNoCinematicShooting;
	bool			m_ShouldAlignToCorner			: 1;
	bool			m_ShouldApplyStepOutPosition	: 1;
	bool			m_ShouldApplyAimingBehaviour	: 1;
	bool			m_WasCinematicShootingLast		: 1;

	static bool		ms_ShouldLimitHeadingOnCorners;

	//Forbid copy construction and assignment.
	camThirdPersonPedAimInCoverCamera(const camThirdPersonPedAimInCoverCamera& other);
	camThirdPersonPedAimInCoverCamera& operator=(const camThirdPersonPedAimInCoverCamera& other);
};

#endif // THIRD_PERSON_PED_AIM_IN_COVER_CAMERA_H
