//
// camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h"

#include "fwmaths/angle.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "scene/Entity.h"
#include "task/Combat/Cover/cover.h"
#include "task/Combat/Cover/TaskCover.h"
#include "weapons/info/WeaponInfo.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonPedAimInCoverCamera,0x87D260CC)

PARAM(limitCamInCornerCover, "[camThirdPersonPedAimInCoverCamera] Apply custom relative orbit heading limits in high corner cover");

bool camThirdPersonPedAimInCoverCamera::ms_ShouldLimitHeadingOnCorners = false;


camThirdPersonPedAimInCoverCamera::camThirdPersonPedAimInCoverCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonPedAimCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonPedAimInCoverCameraMetadata&>(metadata))
, m_DampedStepOutPosition(VEC3_ZERO)
, m_MostRecentTimeTurningOnNarrowCover(0)
, m_NumFramesAimingWithNoCinematicShooting(0)
, m_ShouldAlignToCorner(false)
, m_ShouldApplyStepOutPosition(false)
, m_ShouldApplyAimingBehaviour(false)
, m_WasCinematicShootingLast(false)
{
	//Bypass the default tendency of the third-person camera to start directly behind the attach parent.
	// - We instead align to cover on the first update, so long as no other requests for a specific relative orientation have been received.
	m_ShouldApplyRelativeHeading	= false;
	m_ShouldApplyRelativePitch		= false;
}

void camThirdPersonPedAimInCoverCamera::InitClass()
{
	ms_ShouldLimitHeadingOnCorners = PARAM_limitCamInCornerCover.Get();
}

bool camThirdPersonPedAimInCoverCamera::Update()
{
	if(!m_WeaponInfo)
	{
		//Cache the active weapon info for the follow ped, as we do not want this to change during the lifetime of this camera.
		m_WeaponInfo = camInterface::GetGameplayDirector().GetFollowPedWeaponInfo();

		if(!cameraVerifyf(m_WeaponInfo, "A third-person aim in cover camera (name: %s, hash: %u) failed to find valid weapon info for the follow ped",
			GetName(), GetNameHash()))
		{
			return false;
		}
	}

	const camBaseCamera* interpolatedCamera = m_FrameInterpolator ? m_FrameInterpolator->GetSourceCamera() : NULL;
	const bool isInterpolatingFromCinematicShootingCamera = interpolatedCamera && interpolatedCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()) &&
		static_cast<const camThirdPersonPedAssistedAimCamera*>(interpolatedCamera)->GetIsCinematicShootingCamera();

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed				= static_cast<const CPed*>(m_AttachParent.Get());
	const CPlayerInfo* playerInfo		= attachPed->GetPlayerInfo();
	const bool assistedAimingAvailable	= playerInfo && !playerInfo->IsAiming(false) && attachPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
	const bool isCinematicShootingRelevant = ((assistedAimingAvailable && CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING)) || isInterpolatingFromCinematicShootingCamera);

	//if we're cinematic shooting or interpolating from a cinematic shooting camera, we need to flag this
	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettings();
	if (coverSettings.m_IsIdle)
	{
		m_WasCinematicShootingLast = false;
	}
	else if (isCinematicShootingRelevant)
	{
		m_WasCinematicShootingLast = true;
	}

	//NOTE: We want to allow the aiming behaviour to turn on when we're not interpolating with a cinematic shooting camera, and stay off when we are.
	//However, there is a one frame difference between the ped aiming and the cinematic shooting camera kicking in, meaning we can't stop the aiming
	//behaviour running for a frame. This method ensures the player must have been aiming with no interpolation for at least a frame before we allow
	//the aiming behaviour to start. This has no effect when cinematic shooting isn't switched on.
	if (!isCinematicShootingRelevant && coverSettings.m_IsAiming)
	{
		++m_NumFramesAimingWithNoCinematicShooting;
	}
	else
	{
		m_NumFramesAimingWithNoCinematicShooting = 0;
	}

	UpdateFacingDirection();
	UpdateCornerBehaviour();
	UpdateAimingBehaviour();
	UpdateLowCoverBehaviour();

	if(m_NumUpdatesPerformed == 0)
	{
		UpdateInitialCoverAlignment();
	}

	//Ignore occlusion with select collision when not aiming or blind-firing, as this improves camera continuity when we are not required to
	//rigorously avoid all occlusion.
	if(m_Collision)
	{
		m_Collision->SetShouldIgnoreOcclusionWithSelectCollision(!m_ShouldApplyAimingBehaviour && !coverSettings.m_IsBlindFiring);
	}

	if(NetworkInterface::IsInSpectatorMode() && GetControlHelper())
	{
		GetControlHelper()->SetOverrideViewModeForSpectatorModeThisUpdate(); 
	}

	const bool hasSucceeded = camThirdPersonPedAimCamera::Update();

	return hasSucceeded;
}

void camThirdPersonPedAimInCoverCamera::UpdateFacingDirection()
{
	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	const float desiredFacingDirectionScaling	= coverSettings.m_IsFacingLeftInCover ? -1.0f : 1.0f;
	const float springConstant					= (m_NumUpdatesPerformed == 0) ? 0.0f : (coverSettings.m_IsMovingFromCoverToCover ?
													m_Metadata.m_CoverToCoverFacingDirectionScalingSpringConstant :
													m_Metadata.m_FacingDirectionScalingSpringConstant);
	const float springDampingRatio				= coverSettings.m_IsMovingFromCoverToCover ?
													m_Metadata.m_CoverToCoverFacingDirectionScalingSpringDampingRatio :
													m_Metadata.m_FacingDirectionScalingSpringDampingRatio;
	m_FacingDirectionScalingSpring.Update(desiredFacingDirectionScaling, springConstant, springDampingRatio);
}

void camThirdPersonPedAimInCoverCamera::UpdateCornerBehaviour()
{
	camGameplayDirector& gameplayDirector							= camInterface::GetGameplayDirector();
	const camGameplayDirector::tCoverSettings& currentCoverSettings	= gameplayDirector.GetFollowPedCoverSettings();

	//NOTE: We always attempt to align to the corner throughout cover-to-cover transitions.
	m_ShouldAlignToCorner	= (currentCoverSettings.m_IsMovingFromCoverToCover || (m_Metadata.m_ShouldAlignToCorners &&
								currentCoverSettings.m_ShouldAlignToCornerCover));

	const camGameplayDirector::tCoverSettings& coverSettings = gameplayDirector.GetFollowPedCoverSettingsForMostRecentCover();

	const bool isPeekingOrAimIntroOrHold	= coverSettings.m_IsPeeking || coverSettings.m_IsTransitioningToAiming || coverSettings.m_IsAimingHoldActive;
	const bool isTurningOnNarrowCover		= coverSettings.m_IsTurning && (coverSettings.m_CoverUsage == CCoverPoint::COVUSE_WALLTONEITHER);
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const bool isAttachPedNetworkClone		= static_cast<const CPed*>(m_AttachParent.Get())->IsNetworkClone();

	//NOTE: We explicitly ignore the step-out mover position for blind firing and thrown weapons, as the player movement is not the same as for
	//regular weapon aiming/firing - the player does not step out a variable distance on corners based upon the aim direction.
	//NOTE: We don't step-out for network clones, as the look-around orientation can be controlled by the local user.
	m_ShouldApplyStepOutPosition	= (m_Metadata.m_ShouldApplyStepOutPosition && coverSettings.m_CanFireRoundCornerCover && isPeekingOrAimIntroOrHold &&
										!coverSettings.m_IsBlindFiring && !coverSettings.m_IsMovingFromCoverToCover &&
										!isTurningOnNarrowCover && !coverSettings.m_ShouldIgnoreStepOut && !isAttachPedNetworkClone);
	if(m_ShouldApplyStepOutPosition)
	{
		//Ensure the ped is actually facing the (high) corner.
		if(((coverSettings.m_CoverUsage == CCoverPoint::COVUSE_WALLTOLEFT) && coverSettings.m_IsFacingLeftInCover) ||
			((coverSettings.m_CoverUsage == CCoverPoint::COVUSE_WALLTORIGHT) && !coverSettings.m_IsFacingLeftInCover))
		{
			m_ShouldApplyStepOutPosition = false;
		}
	}

	const float stepOutPositionBlendLevelOnPreviousUpdate = m_StepOutPositionBlendSpring.GetResult();

	const float desiredStepOutPositionBlendLevel = m_ShouldApplyStepOutPosition ? 1.0f : 0.0f;

	float stepOutPositionBlendSpringConstant;
	if(m_NumUpdatesPerformed == 0)
	{
		stepOutPositionBlendSpringConstant	= 0.0f;
	}
	else if(coverSettings.m_IsAiming)
	{
		stepOutPositionBlendSpringConstant	= m_Metadata.m_StepOutPositionBlendSpringConstantForAiming;
	}
	else if(m_ShouldApplyStepOutPosition)
	{
		stepOutPositionBlendSpringConstant	= coverSettings.m_IsAimingHoldActive ? m_Metadata.m_StepOutPositionBlendInSpringConstantForAimingHold :
												m_Metadata.m_StepOutPositionBlendInSpringConstant;
	}
	else
	{
		stepOutPositionBlendSpringConstant	= m_Metadata.m_StepOutPositionBlendOutSpringConstant;
	}
														
	m_StepOutPositionBlendSpring.Update(desiredStepOutPositionBlendLevel, stepOutPositionBlendSpringConstant, m_Metadata.m_StepOutPositionBlendSpringDampingRatio);

	//Apply a damped spring to the x and y components of the step-out position relative to the cover matrix, as this position can pop when
	//transitioning between the distinct step-out arcs positions.
	//NOTE: We only perform this damping and update the step-out position to be applied when the cover state suggests it is safe to apply.
	//This prevents bad offsets being pushed into the springs.
	if(m_ShouldApplyStepOutPosition)
	{
		Vector3 desiredCoverRelativeStepOutPosition;
		coverSettings.m_CoverMatrix.UnTransform(coverSettings.m_StepOutPosition, desiredCoverRelativeStepOutPosition);

		Vector3 dampedCoverRelativeStepOutPosition;

		const bool shouldCutStepOutPosition		= (m_NumUpdatesPerformed == 0) || (stepOutPositionBlendLevelOnPreviousUpdate < SMALL_FLOAT);
		const float springConstant				= shouldCutStepOutPosition ? 0.0f : (coverSettings.m_IsTransitioningToAiming ?
													m_Metadata.m_CoverRelativeStepOutPositionSpringConstantForTransitionToAiming :
													m_Metadata.m_CoverRelativeStepOutPositionSpringConstant);
		dampedCoverRelativeStepOutPosition.x	= m_CoverRelativeStepOutPositionXSpring.Update(desiredCoverRelativeStepOutPosition.x, springConstant,
													m_Metadata.m_CoverRelativeStepOutPositionSpringDampingRatio);
		dampedCoverRelativeStepOutPosition.y	= m_CoverRelativeStepOutPositionYSpring.Update(desiredCoverRelativeStepOutPosition.y, springConstant,
													m_Metadata.m_CoverRelativeStepOutPositionSpringDampingRatio);
		dampedCoverRelativeStepOutPosition.z	= desiredCoverRelativeStepOutPosition.z;

		coverSettings.m_CoverMatrix.Transform(dampedCoverRelativeStepOutPosition, m_DampedStepOutPosition);
	}

	//NOTE: We apply a blind firing offset when peeking/aiming with cameras/weapons that don't support the step-out position that is otherwise
	//applied. Likewise for network clones when peeking.
	const bool isInHighCornerCover							= coverSettings.m_CanFireRoundCornerCover && !coverSettings.m_IsInLowCover;
	const bool shouldApplyBlindFiringOffset					= isInHighCornerCover && (coverSettings.m_IsBlindFiring ||
																(isAttachPedNetworkClone && coverSettings.m_IsPeeking) ||
																(!m_Metadata.m_ShouldApplyStepOutPosition &&
																(coverSettings.m_IsPeeking || coverSettings.m_IsTransitioningToAiming ||
																coverSettings.m_IsAiming || coverSettings.m_IsAimingHoldActive)));
	const float desiredBlindFiringOnHighCornerBlendLevel	= shouldApplyBlindFiringOffset ? 1.0f : 0.0f;
	const float blindFiringOnHighCornerBlendSpringConstant	= (m_NumUpdatesPerformed == 0) ? 0.0f : (shouldApplyBlindFiringOffset ?
																m_Metadata.m_BlindFiringOnHighCornerBlendInSpringConstant :
																m_Metadata.m_BlindFiringOnHighCornerBlendOutSpringConstant);
	m_BlindFiringOnHighCornerBlendSpring.Update(desiredBlindFiringOnHighCornerBlendLevel, blindFiringOnHighCornerBlendSpringConstant,
		m_Metadata.m_BlindFiringOnHighCornerBlendSpringDampingRatio);

	if(ms_ShouldLimitHeadingOnCorners && isInHighCornerCover)
	{
		//Use custom cover-relative orbit heading limits for high corners.
		if(coverSettings.m_IsFacingLeftInCover)
		{
			m_RelativeOrbitHeadingLimits.Set(m_Metadata.m_RelativeOrbitHeadingLimitsForHighCornerCover);
		}
		else
		{
			m_RelativeOrbitHeadingLimits.Set(-m_Metadata.m_RelativeOrbitHeadingLimitsForHighCornerCover.y,
				-m_Metadata.m_RelativeOrbitHeadingLimitsForHighCornerCover.x);
		}
	}
	else
	{
		m_RelativeOrbitHeadingLimits.Set(m_Metadata.m_RelativeOrbitHeadingLimits);
	}

	m_RelativeOrbitHeadingLimits.Scale(DtoR);
}

void camThirdPersonPedAimInCoverCamera::UpdateAimingBehaviour()
{
	//We don't apply the aiming behaviour for network clones, as the look-around orientation can be controlled by the local user.
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	if(attachPed->IsNetworkClone())
	{
		return;
	}

	const camGameplayDirector::tCoverSettings& coverSettings	= camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	m_ShouldApplyAimingBehaviour								= m_Metadata.m_AimingSettings.m_ShouldApply && 
																	((m_NumFramesAimingWithNoCinematicShooting > 1) ||
																	(!m_WasCinematicShootingLast &&
																	(coverSettings.m_IsTransitioningToAiming || coverSettings.m_IsAimingHoldActive || coverSettings.m_IsAiming)));

	const float desiredAimingBlendLevel	= m_ShouldApplyAimingBehaviour ? 1.0f : 0.0f;
	const float springConstant			= (m_NumUpdatesPerformed == 0) ? 0.0f : (m_ShouldApplyAimingBehaviour ?
											m_Metadata.m_AimingSettings.m_AimingBlendInSpringConstant :
											m_Metadata.m_AimingSettings.m_AimingBlendOutSpringConstant);
	const float springDampingRatio		= m_ShouldApplyAimingBehaviour ? m_Metadata.m_AimingSettings.m_AimingBlendInSpringDampingRatio :
											m_Metadata.m_AimingSettings.m_AimingBlendOutSpringDampingRatio;

	m_AimingBlendSpring.Update(desiredAimingBlendLevel, springConstant, springDampingRatio);

	//Only allow accurate aim mode to be toggled when aiming.
	if(!m_ShouldApplyAimingBehaviour && m_ControlHelper)
	{
		m_ControlHelper->IgnoreAccurateModeInputThisUpdate();
	}
}

void camThirdPersonPedAimInCoverCamera::UpdateLowCoverBehaviour()
{
	//We don't apply low cover framing for network clones, as it is cleaner to maintain default (high cover) framing.
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	if(attachPed->IsNetworkClone())
	{
		return;
	}

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	//Blend out of low cover framing when peeking, blind-firing or aiming (unless crouched aiming.)
	//NOTE: GetIsCrouching() only appears to be valid when aiming directly in low cover.
	const bool isAttachPedCrouching					= attachPed->GetIsCrouching();
	const bool shouldApplyLowCoverFraming			= coverSettings.m_IsInLowCover && !coverSettings.m_IsPeeking && !coverSettings.m_IsBlindFiring &&
														(isAttachPedCrouching || (!coverSettings.m_IsTransitioningToAiming && !coverSettings.m_IsAiming));
	const float desiredLowCoverFramingBlendLevel	= shouldApplyLowCoverFraming ? 1.0f : 0.0f;
	const float springConstant						= (m_NumUpdatesPerformed == 0) ? 0.0f : (shouldApplyLowCoverFraming ?
														m_Metadata.m_LowCoverSettings.m_BlendInSpringConstant :
														m_Metadata.m_LowCoverSettings.m_BlendOutSpringConstant);
	const float springDampingRatio					= shouldApplyLowCoverFraming ? m_Metadata.m_LowCoverSettings.m_BlendInSpringDampingRatio :
														m_Metadata.m_LowCoverSettings.m_BlendOutSpringDampingRatio;
	m_LowCoverFramingBlendSpring.Update(desiredLowCoverFramingBlendLevel, springConstant, springDampingRatio);
}

void camThirdPersonPedAimInCoverCamera::UpdateInitialCoverAlignment()
{
	//Align to cover, but only if no other requests for a specific relative orientation have been received.

	if(m_ShouldApplyRelativeHeading && m_ShouldApplyRelativePitch)
	{
		return;
	}

	//if we're interpolating from an assisted aim camera, we don't want to reset this camera
	if (m_FrameInterpolator)
	{
		const camBaseCamera* sourceCamera = m_FrameInterpolator->GetSourceCamera();
		if (sourceCamera && sourceCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
		{
			return;
		}
	}

	const camGameplayDirector::tCoverSettings& currentCoverSettings	= camInterface::GetGameplayDirector().GetFollowPedCoverSettings();

	float coverHeading;
	float coverPitch;
	camFrame::ComputeHeadingAndPitchFromFront(currentCoverSettings.m_CoverMatrix.b, coverHeading, coverPitch);

	//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix, when overriding a relative orientation. This
	//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
	const Matrix34 attachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	float attachParentHeading;
	float attachParentPitch;
	camFrame::ComputeHeadingAndPitchFromMatrix(attachParentMatrix, attachParentHeading, attachParentPitch);

	if(!m_ShouldApplyRelativeHeading)
	{
		m_RelativeHeadingToApply		= coverHeading - attachParentHeading;
		m_RelativeHeadingToApply		= fwAngle::LimitRadianAngle(m_RelativeHeadingToApply);
		m_ShouldApplyRelativeHeading	= true;
	}

	if(!m_ShouldApplyRelativePitch)
	{
		m_RelativePitchToApply			= coverPitch - attachParentPitch;
		m_RelativePitchToApply			= fwAngle::LimitRadianAngle(m_RelativePitchToApply);
		m_ShouldApplyRelativePitch		= true;
	}
}

void camThirdPersonPedAimInCoverCamera::UpdateBasePivotPosition()
{
	camThirdPersonPedAimCamera::UpdateBasePivotPosition();

	const float stepOutPositionBlendLevel = m_StepOutPositionBlendSpring.GetResult();
	if(stepOutPositionBlendLevel >= SMALL_FLOAT)
	{
		//Blend the x and y components of base pivot position to the cover-relative position predicted by the cover task.
		//This ensures that peeking gives an identical camera frame, and hence aim vector, to firing.
		Vector3 customBasePivotPosition(m_DampedStepOutPosition);
		//NOTE: We maintain the default z component for consistency.
		customBasePivotPosition.z = m_BasePivotPosition.z;
		m_BasePivotPosition.Lerp(stepOutPositionBlendLevel, customBasePivotPosition);
	}

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	//Add a fixed offset in the facing direction.
	const float facingDirectionScaling	= m_FacingDirectionScalingSpring.GetResult();
	const Vector3 offsetToApply			= facingDirectionScaling * m_Metadata.m_ExtraBasePivotOffsetInFacingDirection *
											coverSettings.m_CoverMatrix.a;
	m_BasePivotPosition					+= offsetToApply;
}

float camThirdPersonPedAimInCoverCamera::ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const
{
	//Blend to a custom ratio when appropriate in low cover.
	float attachParentHeightRatioToAttain	= camThirdPersonPedAimCamera::ComputeAttachParentHeightRatioToAttainForBasePivotPosition();
	const float lowCoverFramingBlendLevel	= m_LowCoverFramingBlendSpring.GetResult();
	attachParentHeightRatioToAttain			= Lerp(lowCoverFramingBlendLevel, attachParentHeightRatioToAttain,
												m_Metadata.m_LowCoverSettings.m_AttachParentHeightRatioToAttainForBasePivotPosition);

	return attachParentHeightRatioToAttain;
}

void camThirdPersonPedAimInCoverCamera::ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const
{
	camThirdPersonPedAimCamera::ComputeScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);

	//Blend to custom settings when appropriate in low cover.
	const float lowCoverFramingBlendLevel	= m_LowCoverFramingBlendSpring.GetResult();
	screenRatioForMinFootRoom				= Lerp(lowCoverFramingBlendLevel, screenRatioForMinFootRoom, m_Metadata.m_LowCoverSettings.m_ScreenRatioForMinFootRoom);
	screenRatioForMaxFootRoom				= Lerp(lowCoverFramingBlendLevel, screenRatioForMaxFootRoom, m_Metadata.m_LowCoverSettings.m_ScreenRatioForMaxFootRoom);

	//Blend to custom settings when aiming.
	const float aimingBlendLevel	= m_AimingBlendSpring.GetResult();
	screenRatioForMinFootRoom		= Lerp(aimingBlendLevel, screenRatioForMinFootRoom, m_Metadata.m_AimingSettings.m_ScreenRatioForMinFootRoom);
	screenRatioForMaxFootRoom		= Lerp(aimingBlendLevel, screenRatioForMaxFootRoom, m_Metadata.m_AimingSettings.m_ScreenRatioForMaxFootRoom);
}

float camThirdPersonPedAimInCoverCamera::ComputeBaseFov() const
{
	//Only apply the weapon-specific FOV settings when aiming.
	const float aimingBlendLevel		= m_AimingBlendSpring.GetResult();
	const float baseFov					= camThirdPersonCamera::ComputeBaseFov();
	const float weaponSpecificBaseFov	= camThirdPersonPedAimCamera::ComputeBaseFov();

	const float baseFovToApply			= Lerp(aimingBlendLevel, baseFov, weaponSpecificBaseFov);

	return baseFovToApply;
}

void camThirdPersonPedAimInCoverCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	camThirdPersonPedAimCamera::ComputeOrbitOrientation(orbitHeading, orbitPitch);

	const bool shouldAlignWithCover = ComputeShouldAlignWithCover();
	if(!shouldAlignWithCover)
	{
		m_HeadingAlignmentSpring.Reset();
		m_PitchAlignmentSpring.Reset();

		return;
	}

	const camGameplayDirector& gameplayDirector					= camInterface::GetGameplayDirector();
	const camGameplayDirector::tCoverSettings& coverSettings	= gameplayDirector.GetFollowPedCoverSettings();

	float desiredOrbitHeading;
	float desiredOrbitPitch;
	camFrame::ComputeHeadingAndPitchFromFront(coverSettings.m_CoverMatrix.b, desiredOrbitHeading, desiredOrbitPitch);

	if(!m_ShouldAlignToCorner)
	{
		//Bias the target orientation in the attach ped's facing direction when not aligning to a corner.
		const float facingDirectionScaling	= m_FacingDirectionScalingSpring.GetResult();
		const float headingDelta			= -facingDirectionScaling * m_Metadata.m_DefaultAlignmentHeadingDeltaToLookAhead * DtoR;
		desiredOrbitHeading					= fwAngle::LimitRadianAngle(desiredOrbitHeading + headingDelta);
	}

	//Ensure that we blend to the target orientation over the shortest angle.
	const float desiredOrbitHeadingDelta = desiredOrbitHeading - orbitHeading;
	if(desiredOrbitHeadingDelta > PI)
	{
		desiredOrbitHeading -= TWO_PI;
	}
	else if(desiredOrbitHeadingDelta < -PI)
	{
		desiredOrbitHeading += TWO_PI;
	}

	const float alignmentSpringConstant	= m_ShouldAlignToCorner ? m_Metadata.m_CornerAlignmentSpringConstant :
		(coverSettings.m_IsMovingAroundCornerCover ? m_Metadata.m_MovingAroundCornerAlignmentSpringConstant :
		m_Metadata.m_DefaultAlignmentSpringConstant);

	//NOTE: We must override the spring results with the current orientation on each update, otherwise the alignment springs will dampen other
	//(unrelated) orientation changes.
	m_HeadingAlignmentSpring.OverrideResult(orbitHeading);
	m_HeadingAlignmentSpring.Update(desiredOrbitHeading, alignmentSpringConstant, m_Metadata.m_DefaultAlignmentSpringDampingRatio);

	m_PitchAlignmentSpring.OverrideResult(orbitPitch);
	m_PitchAlignmentSpring.Update(desiredOrbitPitch, alignmentSpringConstant, m_Metadata.m_DefaultAlignmentSpringDampingRatio);

	orbitHeading	= m_HeadingAlignmentSpring.GetResult();
	orbitPitch		= m_PitchAlignmentSpring.GetResult();
}

bool camThirdPersonPedAimInCoverCamera::ComputeShouldAlignWithCover() const
{
	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettings();
	if(!coverSettings.m_IsInCover || coverSettings.m_IsTransitioningToAiming || coverSettings.m_IsAiming ||
		coverSettings.m_IsTransitioningFromAiming || coverSettings.m_IsAimingHoldActive || coverSettings.m_IsBlindFiring)
	{
		//We don't align to cover when exiting, aiming or firing.
		return false;
	}

	if(m_ShouldAlignToCorner)
	{
		return true;
	}

	if(m_ShouldApplyRelativeHeading || m_ShouldApplyRelativePitch)
	{
		return false;
	}

	const float lookAroundSpeed = m_ControlHelper->GetLookAroundSpeed();
	if(Abs(lookAroundSpeed) >= SMALL_FLOAT)
	{
		return false;
	}

	//NOTE: We bypass the velocity test when the attach ped is moving around a corner.
	if(!coverSettings.m_IsMovingAroundCornerCover)
	{
		Vector3 attachParentLocalXYVelocity;
		ComputeAttachParentLocalXYVelocity(attachParentLocalXYVelocity);

		const float attachParentLocalXYSpeed = attachParentLocalXYVelocity.Mag();
		if(attachParentLocalXYSpeed < m_Metadata.m_MinMoveSpeedForDefaultAlignment)
		{
			return false;
		}
	}

	return true;
}

float camThirdPersonPedAimInCoverCamera::ComputeIdealOrbitHeadingForLimiting() const
{
	//Limit with respect to the cover left/right vector, based upon the attach ped's facing direction.

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	const float facingDirectionScaling	= m_FacingDirectionScalingSpring.GetResult();
	const Vector3 idealFront			= (facingDirectionScaling < 0.0f) ? -coverSettings.m_CoverMatrix.a : coverSettings.m_CoverMatrix.a;
	const float idealHeading			= camFrame::ComputeHeadingFromFront(idealFront);

	return idealHeading;
}

void camThirdPersonPedAimInCoverCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	camThirdPersonPedAimCamera::ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	//Add an additional side offset when blind firing (or using a thrown weapon) on a high corner.
	const float blindFiringBlendLevel	= m_BlindFiringOnHighCornerBlendSpring.GetResult();
	orbitRelativeOffset.x				+= blindFiringBlendLevel * m_Metadata.m_AdditionalCameraRelativeSideOffsetForBlindFiring;

	//We must ensure that the side offset is applied in the facing direction.
	const float facingDirectionScaling	= m_FacingDirectionScalingSpring.GetResult();
	orbitRelativeOffset.x				*= facingDirectionScaling;
}

void camThirdPersonPedAimInCoverCamera::ComputeCollisionFallBackPosition(const Vector3& cameraPosition, float& collisionTestRadius,
	WorldProbe::CShapeTestCapsuleDesc& capsuleTest, WorldProbe::CShapeTestResults& shapeTestResults, Vector3& collisionFallBackPosition)
{
	camThirdPersonPedAimCamera::ComputeCollisionFallBackPosition(cameraPosition, collisionTestRadius, capsuleTest, shapeTestResults,
		collisionFallBackPosition);

	Vector3	desiredCollisionFallBackPosition(collisionFallBackPosition);

	const float stepOutPositionBlendLevel = m_StepOutPositionBlendSpring.GetResult();
	if(stepOutPositionBlendLevel >= SMALL_FLOAT)
	{
		//Blend the x and y components of collision fall back position to the cover-relative position predicted by the cover task.
		//This ensures that peeking gives an identical camera frame, and hence aim vector, to firing.
		Vector3 customCollisionFallBackPosition(m_DampedStepOutPosition);
		//NOTE: We maintain the default z component for consistency.
		customCollisionFallBackPosition.z = collisionFallBackPosition.z;
		desiredCollisionFallBackPosition.Lerp(stepOutPositionBlendLevel, customCollisionFallBackPosition);
	}

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();

	//Add a fixed offset in the facing direction.
	const float facingDirectionScaling	= m_FacingDirectionScalingSpring.GetResult();
	Vector3 offsetToApply				= facingDirectionScaling * m_Metadata.m_ExtraCollisionFallBackOffsetInFacingDirection *
											coverSettings.m_CoverMatrix.a;
	desiredCollisionFallBackPosition	+= offsetToApply;

	//Add an offset that is scaled based upon the orbit heading relative to the cover matrix.
	//This allows the collision fall back position to slide along the cover plane as the camera rotates around the follow-ped,
	//which can aid the ability to rotate the camera around corners without negatively affecting the normal collision behaviour.

	Vector3 orbitFrontXY	= m_PivotPosition - cameraPosition;
	orbitFrontXY.z			= 0.0f;
	orbitFrontXY.NormalizeSafe(coverSettings.m_CoverMatrix.b);

	const float orbitFrontDotCoverLeft	= orbitFrontXY.Dot(-coverSettings.m_CoverMatrix.a);
	offsetToApply						= orbitFrontDotCoverLeft * m_Metadata.m_ExtraCollisionFallBackOffsetOrbitFrontDotCoverLeft *
											coverSettings.m_CoverMatrix.a;
	desiredCollisionFallBackPosition	+= offsetToApply;

	const float fallBackToDesiredFallBackDistance2 = collisionFallBackPosition.Dist2(desiredCollisionFallBackPosition);
	if(fallBackToDesiredFallBackDistance2 > VERY_SMALL_FLOAT)
	{
		if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
			"Unable to compute a valid collision fall back position due to an invalid test radius."))
		{
			return;
		}

		//Ensure any offset applied does not push the fall back position into collision.
		shapeTestResults.Reset();
		capsuleTest.SetCapsule(collisionFallBackPosition, desiredCollisionFallBackPosition, collisionTestRadius);

		//Reduce the test radius in readiness for use in a consecutive test.
		collisionTestRadius		-= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

		const bool hasHit		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float hitTValue	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

		//TODO: Apply damping when the pushing away from the default fall back position.

		collisionFallBackPosition.Lerp(hitTValue, desiredCollisionFallBackPosition);
	}
}

float camThirdPersonPedAimInCoverCamera::ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const
{
	//Blend to a custom ratio when appropriate in low cover.
	float attachParentHeightRatioToAttain	= camThirdPersonPedAimCamera::ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition();
	const float lowCoverFramingBlendLevel	= m_LowCoverFramingBlendSpring.GetResult();
	attachParentHeightRatioToAttain			= Lerp(lowCoverFramingBlendLevel, attachParentHeightRatioToAttain,
												m_Metadata.m_LowCoverSettings.m_AttachParentHeightRatioToAttainForCollisionFallBackPosition);

	return attachParentHeightRatioToAttain;
}

float camThirdPersonPedAimInCoverCamera::ComputeDesiredFov()
{
	//Only apply the weapon-specific FOV settings when aiming.
	const float aimingBlendLevel			= m_AimingBlendSpring.GetResult();
	const float desiredFov					= camThirdPersonCamera::ComputeDesiredFov();
	const float weaponSpecificDesiredFov	= camThirdPersonPedAimCamera::ComputeDesiredFov();

	const float desiredFovToApply			= Lerp(aimingBlendLevel, desiredFov, weaponSpecificDesiredFov);

	return desiredFovToApply;
}

bool camThirdPersonPedAimInCoverCamera::ComputeShouldUseLockOnAiming() const
{
	//Only use lock-on aiming when actually aiming in cover (not idling, peeking, etc.)
	const bool shouldUseLockOnAiming = m_ShouldApplyAimingBehaviour && camThirdPersonPedAimCamera::ComputeShouldUseLockOnAiming();

	return shouldUseLockOnAiming;
}

float camThirdPersonPedAimInCoverCamera::ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const
{
	const float zoomFactorForAiming		= camThirdPersonPedAimCamera::ComputeDesiredZoomFactorForAccurateMode(weaponInfo);
	const float aimingBlendLevel		= m_AimingBlendSpring.GetResult();
	const float zoomFactorToConsider	= Lerp(aimingBlendLevel, 1.0f, zoomFactorForAiming);

	return zoomFactorToConsider;
}

void camThirdPersonPedAimInCoverCamera::UpdateReticuleSettings()
{
	camThirdPersonPedAimCamera::UpdateReticuleSettings();

	if(m_ShouldDisplayReticule)
	{
		//We don't show the reticule or apply aiming behaviours for network clones, as the look-around orientation can be controlled by the
		//local user.
		if(m_AttachParent && m_AttachParent->GetIsTypePed() && static_cast<const CPed*>(m_AttachParent.Get())->IsNetworkClone())
		{
			m_ShouldDisplayReticule = false;

			return;
		}

		const camGameplayDirector& gameplayDirector					= camInterface::GetGameplayDirector();
		const camGameplayDirector::tCoverSettings& coverSettings	= gameplayDirector.GetFollowPedCoverSettingsForMostRecentCover();

		//Only show the reticule if the ped is in an appropriate cover state.
		m_ShouldDisplayReticule	= coverSettings.m_IsAiming ||
									(!m_WasCinematicShootingLast && 
									(coverSettings.m_IsPeeking || coverSettings.m_IsTransitioningToAiming || coverSettings.m_IsAimingHoldActive));
		if(m_ShouldDisplayReticule)
		{
			m_ShouldDisplayReticule = !ComputeShouldHideReticuleDueToSpringMovement();
		}
	}
}

bool camThirdPersonPedAimInCoverCamera::ComputeShouldDisplayReticuleAsInterpolationSource() const
{
	//Ensure that the follow-ped was facing right in cover, as we do not want to show the reticule whilst the camera switches sides in the
	//interpolation.
	const camGameplayDirector::tCoverSettings& coverSettings	= camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();
	const bool shouldDisplayReticuleAsInterpolationSource		= m_ShouldDisplayReticule && !coverSettings.m_IsFacingLeftInCover;

	return shouldDisplayReticuleAsInterpolationSource;
}

bool camThirdPersonPedAimInCoverCamera::ComputeShouldHideReticuleDueToSpringMovement() const
{
	//Hide the reticule if it is moving independently of the ped due to a spring response.

	float springSpeed = Abs(m_FacingDirectionScalingSpring.GetVelocity());
	if(springSpeed > m_Metadata.m_MaxSpringSpeedToDisplayReticule)
	{
		return true;
	}

	springSpeed = Abs(m_HeadingAlignmentSpring.GetVelocity());
	if(springSpeed > m_Metadata.m_MaxSpringSpeedToDisplayReticule)
	{
		return true;
	}

	springSpeed = Abs(m_PitchAlignmentSpring.GetVelocity());
	if(springSpeed > m_Metadata.m_MaxSpringSpeedToDisplayReticule)
	{
		return true;
	}

	springSpeed = Abs(m_LowCoverFramingBlendSpring.GetVelocity());
	if(springSpeed > m_Metadata.m_MaxSpringSpeedToDisplayReticule)
	{
		return true;
	}

	springSpeed = Abs(m_BlindFiringOnHighCornerBlendSpring.GetVelocity());
	if(springSpeed > m_Metadata.m_MaxSpringSpeedToDisplayReticule)
	{
		return true;
	}

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();
	if(coverSettings.m_IsPeeking || coverSettings.m_IsTransitioningToAiming)
	{
		const float stepOutPositionBlendLevel			= m_StepOutPositionBlendSpring.GetResult();
		const float desiredStepOutPositionBlendLevel	= m_ShouldApplyStepOutPosition ? 1.0f : 0.0f;
		const float absBlendLevelError					= Abs(desiredStepOutPositionBlendLevel - stepOutPositionBlendLevel);
		if(absBlendLevelError > m_Metadata.m_MaxStepOutPositionBlendErrorToDisplayReticule)
		{
			return true;
		}
	}

	return false;
}

void camThirdPersonPedAimInCoverCamera::UpdateDof()
{
	camThirdPersonPedAimCamera::UpdateDof();

	//Lock the measured focus distance for the adaptive DOF behaviour if the attach ped is switching sides on narrow cover.
	//Otherwise, this cover can influence the focus distance as the camera slides past it. Note that we apply a hold to this behaviour,
	//as ped stops turning before the camera has settled.

	const u32 gameTime = fwTimer::GetTimeInMilliseconds();

	const camGameplayDirector::tCoverSettings& coverSettings	= camInterface::GetGameplayDirector().GetFollowPedCoverSettingsForMostRecentCover();
	bool shouldConsiderAsTurningOnNarrowCover					= coverSettings.m_IsTurning && (coverSettings.m_CoverUsage == CCoverPoint::COVUSE_WALLTONEITHER);
	if(shouldConsiderAsTurningOnNarrowCover)
	{
		m_MostRecentTimeTurningOnNarrowCover = gameTime;
	}
	if(gameTime < (m_MostRecentTimeTurningOnNarrowCover + m_Metadata.m_MinTimeToLockFocusDistanceWhenTurningOnNarrowCover))
	{
		shouldConsiderAsTurningOnNarrowCover = true;
	}

	if(shouldConsiderAsTurningOnNarrowCover)
	{
		m_PostEffectFrame.GetFlags().SetFlag(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);
	}
}

void camThirdPersonPedAimInCoverCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camThirdPersonPedAimCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	const camDepthOfFieldSettingsMetadata* tempCustomSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_AimingSettings.m_DofSettings.GetHash());
	if(!tempCustomSettings)
	{
		return;
	}

	//Blend to custom DOF settings when aiming.

	camDepthOfFieldSettingsMetadata customSettings(*tempCustomSettings);

	if(m_Metadata.m_BaseFovToEmulateWithFocalLengthMultiplier >= g_MinFov)
	{
		//Compensate for the variation in base FOV between different weapons/cameras by using the focal length multiplier to emulate a specific FOV.

		const float baseFov						= ComputeBaseFov();
		const float fovRatio					= baseFov / m_Metadata.m_BaseFovToEmulateWithFocalLengthMultiplier;
		customSettings.m_FocalLengthMultiplier	*= fovRatio;
	}

	float aimingBlendLevel	= m_AimingBlendSpring.GetResult();
	aimingBlendLevel		= Clamp(aimingBlendLevel, 0.0f, 1.0f);

	BlendDofSettings(aimingBlendLevel, settings, customSettings, settings);

	//NOTE: We only actually measure post-alpha pixel depth when flagged *and* when aiming at a vehicle.
	//- This allows us to avoid blurring out a vehicle when aiming through its windows without introducing more general problems with focusing on things like the windows of buildings.
	if(settings.m_ShouldMeasurePostAlphaPixelDepth)
	{
		bool isAimingAtVehicle = false;
		if(m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
			const CPlayerInfo* playerInfo	= attachPed->GetPlayerInfo();
			isAimingAtVehicle				= playerInfo && playerInfo->GetTargeting().IsFreeAimTargetingVehicleForCamera();
		}

		settings.m_ShouldMeasurePostAlphaPixelDepth = isAimingAtVehicle;
	}
}

#if __BANK
void camThirdPersonPedAimInCoverCamera::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Third-person ped aim in cover camera");
	{
		bank.AddToggle("Limit heading on corners", &ms_ShouldLimitHeadingOnCorners);
	}
	bank.PopGroup(); //Third-person ped aim in cover camera
}
#endif // __BANK
