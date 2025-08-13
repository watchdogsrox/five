//
// camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"

#include "fwmaths/angle.h"

#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicTwoShotCamera.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/viewports/ViewportManager.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/Weapons/TaskWeapon.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "camera/CamInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonPedAssistedAimCamera,0x35769867)

#if __BANK
s32 camThirdPersonPedAssistedAimCamera::ms_DebugOverriddenCinematicMomentDuration = -1;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForceFullShootingFocus = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForceFullRunningShake = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForceFullWalkingShake = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugDrawTargetPosition = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForcePlayerFraming = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForceCinematicKillShotOff = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugForceCinematicKillShotOn = false;
bool camThirdPersonPedAssistedAimCamera::ms_DebugAlwaysTriggerCinematicMoments = false;
#endif // __BANK

u32 camThirdPersonPedAssistedAimCamera::m_TimeAtStartOfLastCinematicMoment = 0; 

extern const float g_DefaultDeadPedLockOnDuration;

camThirdPersonPedAssistedAimCamera::camThirdPersonPedAssistedAimCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonPedAimCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonPedAssistedAimCameraMetadata&>(metadata))
, m_PivotPositionOnPreviousUpdate(g_InvalidPosition)
, m_TargetPositionForCinematicMoment(g_InvalidPosition)
, m_CinematicMomentBlendedBaseFront(VEC3_ZERO)
, m_BaseFrontWithoutOrientationChanges(VEC3_ZERO)
, m_CinematicMomentBlendEnvelope(NULL)
, m_TargetPedOnPreviousUpdateForShootingFocus(NULL)
, m_LastRejectedTargetPedForCinematicMoment(NULL)
, m_LastValidTargetPedForCinematicMoment(NULL)
, m_FirstUpdateOfCinematicCut(0)
, m_CinematicMomentDuration(0)
, m_CinematicMomentBlendLevel(0.0f)
, m_CinematicMomentBlendLevelOnPreviousUpdate(0.0f)
, m_OrbitHeadingToApply(0.0f)
, m_OrbitPitchToApply(0.0f)
, m_OrbitHeadingForFramingPlayer(0.0f)
, m_OrbitPitchForFramingPlayer(0.0f)
, m_CinematicCutBlendLevel(0.0f)
, m_CinematicCutBlendLevelOnPreviousUpdate(0.0f)
, m_CinematicMomentType(FOCUS_TARGET)
, m_HadLosToLockOnTargetOnPreviousUpdate(true)
, m_IsSideOffsetOnRight(true)
, m_IsCameraOnRightForCinematicMoment(false)
, m_IsCinematicMomentAKillShot(false)
, m_HasSetDesiredOrientationForFramingPlayer(false)
{
	m_CinematicMomentSettings	= m_Metadata.m_CinematicMomentSettings;
	m_ShootingFocusSettings		= m_Metadata.m_ShootingFocusSettings;

	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_CinematicMomentSettings.m_BlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_CinematicMomentBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		cameraAssertf(m_CinematicMomentBlendEnvelope, "A third person ped assisted aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash());
	}

	//Apply any shake specified in metadata.
	const camShakeMetadata* shakeMetadata = camFactory::FindObjectMetadata<camShakeMetadata>(m_Metadata.m_IdleShakeRef.GetHash());
	if(shakeMetadata)
	{
		Shake(*shakeMetadata);
	}
}

camThirdPersonPedAssistedAimCamera::~camThirdPersonPedAssistedAimCamera()
{
	if(m_CinematicMomentBlendEnvelope)
	{
		delete m_CinematicMomentBlendEnvelope;
	}
}

float camThirdPersonPedAssistedAimCamera::GetRecoilShakeAmplitudeScaling() const
{
	const camThirdPersonPedAssistedAimCameraRecoilShakeScalingSettings& settings = m_Metadata.m_RecoilShakeScalingSettings;

	const float baseAmplitudeScaling = camThirdPersonPedAimCamera::GetRecoilShakeAmplitudeScaling();
	
	if (!settings.m_ShouldScaleRecoilShake || !m_AttachParent || !m_AttachParent->GetIsPhysical() || !m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return baseAmplitudeScaling;
	}

	const Vector3& attachParentPosition			= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
	const Vector3& cameraPosition				= m_Frame.GetPosition();
	const float distanceToAttachPed				= (attachParentPosition - cameraPosition).Mag(); //ramping, so need square root

	float distanceScaledAmplitudeScaling		= RampValueSafe(distanceToAttachPed, settings.m_MinDistanceToAttachParent, settings.m_MaxDistanceToAttachParent,
													settings.m_MaxRecoilShakeAmplitudeScaling, settings.m_MinRecoilShakeAmplitudeScaling);
	distanceScaledAmplitudeScaling				*= (1.0f - m_CinematicMomentBlendLevel);

	return distanceScaledAmplitudeScaling;
}

bool camThirdPersonPedAssistedAimCamera::GetIsCinematicShootingCamera() const
{
	return m_Metadata.m_ShouldApplyCinematicShootingBehaviour;
}

bool camThirdPersonPedAssistedAimCamera::Update()
{
	UpdateCoverFraming();

	UpdateShootingFocus();

	UpdateCinematicMoment();

	UpdateCinematicCut();

	//we force the cinematic moment blend level behaviors to full while we're running a cinematic cut.
	//the kill shot and reaction shots have been engineered so they don't trigger before a cinematic moment has finished its attack anyway,
	//so this really only has an effect in the delay in the release stage.
	if (m_CinematicCutBlendLevel >= SMALL_FLOAT || (m_CinematicCutBlendLevelOnPreviousUpdate >= SMALL_FLOAT && m_CinematicMomentType == FOCUS_PLAYER))
	{
		//player reaction shots need a frame's delay on this in order to retain the cinematic moment's orientation.
		m_CinematicMomentBlendLevel = 1.0f;
	}

	const bool hasSucceeded = camThirdPersonPedAimCamera::Update();

	//Override the base front
	if (m_CinematicMomentBlendLevel < SMALL_FLOAT)
	{
		const Vector3& baseFront				= camThirdPersonPedAimCamera::GetBaseFront();

		m_CinematicMomentBlendedBaseFront		= baseFront;
		m_BaseFrontWithoutOrientationChanges	= baseFront;
	}
	else
	{
		float baseHeading;
		float basePitch;
		camFrame::ComputeHeadingAndPitchFromFront(m_BaseFrontWithoutOrientationChanges, baseHeading, basePitch);

		Vector3 cinematicMomentFront = m_TargetPositionForCinematicMoment - m_BasePivotPosition;
		cinematicMomentFront.NormalizeSafe(m_BaseFrontWithoutOrientationChanges);

		const float cinematicMomentHeading	= camFrame::ComputeHeadingFromFront(cinematicMomentFront);

		camFrame::ComputeFrontFromHeadingAndPitch(cinematicMomentHeading, basePitch, m_CinematicMomentBlendedBaseFront);
	}

	const float runningShakeActivityScalingFactor = UpdateRunningShakeActivityScalingFactor();

	UpdateMovementShake(m_Metadata.m_RunningShakeSettings, runningShakeActivityScalingFactor BANK_ONLY(, ms_DebugForceFullRunningShake));

	const float walkingShakeActivityScalingFactor = UpdateWalkingShakeActivityScalingFactor();

	UpdateMovementShake(m_Metadata.m_WalkingShakeSettings, walkingShakeActivityScalingFactor BANK_ONLY(, ms_DebugForceFullWalkingShake));

	m_HadLosToLockOnTargetOnPreviousUpdate = ComputeHasLosToTarget();

	m_FrameForAiming.CloneFrom(m_Frame);
	m_FrameForAiming.SetWorldMatrixFromFront(m_CinematicMomentBlendedBaseFront);
	m_FrameForAiming.SetPosition(m_PivotPosition);

	return hasSucceeded;
}

void camThirdPersonPedAssistedAimCamera::UpdateCoverFraming()
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettings();
	if (m_NumUpdatesPerformed == 0 || m_CinematicMomentBlendLevel < SMALL_FLOAT)
	{
		m_CoverSettingsToConsider = coverSettings;
	}

	const camThirdPersonPedAssistedAimCameraInCoverSettings& inCoverSettings = m_Metadata.m_InCoverSettings;

	const bool shouldCutSpring	= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant	= shouldCutSpring ? 0.0f :	inCoverSettings.m_SpringConstant;
	const float coverBlendLevel = m_CoverFramingSpring.Update(m_CoverSettingsToConsider.m_IsInCover ? 1.0f : 0.0f, springConstant, inCoverSettings.m_SpringDampingRatio);

	//get low or regular cover framing settings
	float lockOnAlignmentPivotPositionRightOffset		= inCoverSettings.m_LockOnAlignmentPivotPositionRightSideOffset;
	float lockOnAlignmentPivotPositionLeftOffset		= inCoverSettings.m_LockOnAlignmentPivotPositionLeftSideOffset;
	float lockOnAlignmentPivotPositionVerticalOffset	= inCoverSettings.m_LockOnAlignmentPivotPositionVerticalOffset;
	float playerFramingPivotPositionRightOffset			= inCoverSettings.m_PlayerFramingPivotPositionRightSideOffset;
	float playerFramingPivotPositionLeftOffset			= inCoverSettings.m_PlayerFramingPivotPositionLeftSideOffset;
	float playerFramingPivotPositionVerticalOffset		= inCoverSettings.m_PlayerFramingPivotPositionVerticalOffset;
	float playerFramingDesiredPitch						= m_Metadata.m_CinematicMomentSettings.m_PlayerFramingSettings.m_DesiredPitch;

	if (m_CoverSettingsToConsider.m_IsInLowCover)
	{
		lockOnAlignmentPivotPositionRightOffset		= inCoverSettings.m_LowCoverLockOnAlignmentPivotPositionRightSideOffset;
		lockOnAlignmentPivotPositionLeftOffset		= inCoverSettings.m_LowCoverLockOnAlignmentPivotPositionLeftSideOffset;
		lockOnAlignmentPivotPositionVerticalOffset	= inCoverSettings.m_LowCoverLockOnAlignmentPivotPositionVerticalOffset;
		playerFramingPivotPositionRightOffset		= inCoverSettings.m_LowCoverPlayerFramingPivotPositionRightSideOffset;
		playerFramingPivotPositionLeftOffset		= inCoverSettings.m_LowCoverPlayerFramingPivotPositionLeftSideOffset;
		playerFramingPivotPositionVerticalOffset	= inCoverSettings.m_LowCoverPlayerFramingPivotPositionVerticalOffset;
		playerFramingDesiredPitch					= inCoverSettings.m_LowCoverPlayerFramingDesiredPitch;
	}

	m_CinematicMomentSettings = m_Metadata.m_CinematicMomentSettings;

	m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionRightSideOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionRightSideOffset, lockOnAlignmentPivotPositionRightOffset);
	m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionLeftSideOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionLeftSideOffset, lockOnAlignmentPivotPositionLeftOffset);
	m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionVerticalOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_PivotPositionVerticalOffset, playerFramingPivotPositionVerticalOffset);

	m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionRightSideOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionRightSideOffset, playerFramingPivotPositionRightOffset);
	m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionLeftSideOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionLeftSideOffset, playerFramingPivotPositionLeftOffset);
	m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionVerticalOffset
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_PlayerFramingSettings.m_PivotPositionVerticalOffset, playerFramingPivotPositionVerticalOffset);

	m_CinematicMomentSettings.m_PlayerFramingSettings.m_DesiredPitch
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_PlayerFramingSettings.m_DesiredPitch, playerFramingDesiredPitch);

	m_CinematicMomentSettings.m_ScreenRatioForMinFootRoom
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMinFootRoom, inCoverSettings.m_CinematicMomentScreenRatioForMinFootRoom);
	m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoom
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoom, inCoverSettings.m_CinematicMomentScreenRatioForMaxFootRoom);
	m_CinematicMomentSettings.m_ScreenRatioForMinFootRoomInTightSpace
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMinFootRoomInTightSpace, inCoverSettings.m_CinematicMomentScreenRatioForMinFootRoomInTightSpace);
	m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoomInTightSpace
		= Lerp(coverBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoomInTightSpace, inCoverSettings.m_CinematicMomentScreenRatioForMaxFootRoomInTightSpace);

	m_ShootingFocusSettings = m_Metadata.m_ShootingFocusSettings;

	m_ShootingFocusSettings.m_ScreenRatioForMinFootRoom
		= Lerp(coverBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMinFootRoom, inCoverSettings.m_ShootingFocusScreenRatioForMinFootRoom);
	m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoom
		= Lerp(coverBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoom, inCoverSettings.m_ShootingFocusScreenRatioForMaxFootRoom);
	m_ShootingFocusSettings.m_ScreenRatioForMinFootRoomInTightSpace
		= Lerp(coverBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMinFootRoomInTightSpace, inCoverSettings.m_ShootingFocusScreenRatioForMinFootRoomInTightSpace);
	m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoomInTightSpace
		= Lerp(coverBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoomInTightSpace, inCoverSettings.m_ShootingFocusScreenRatioForMaxFootRoomInTightSpace);

}

void camThirdPersonPedAssistedAimCamera::UpdateShootingFocus()
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return;
	}

	float desiredFocusBlendLevel			= 0.0f;

	if (m_LockOnTarget && m_LockOnTarget->GetIsTypePed())
	{
		const CPed* targetPed				= static_cast<const CPed*>(m_LockOnTarget.Get());
		const float currentHealth			= targetPed->GetHealth();
		const float injuredHealth			= targetPed->GetInjuredHealthThreshold();
		const float maxHealth				= targetPed->GetMaxHealth();
		const float healthRatio				= ClampRange(currentHealth, injuredHealth, maxHealth);

		if (targetPed == m_TargetPedOnPreviousUpdateForShootingFocus || healthRatio > m_ShootingFocusSettings.m_MinHealthRatioToConsiderForNewTargets)
		{
			const float invHealthRatio		= 1.0f - healthRatio;
			if (m_ShootingFocusSettings.m_ShouldUseExponentialCurve)
			{
				desiredFocusBlendLevel		= 1.0f - exp(-m_ShootingFocusSettings.m_ExponentialCurveRate * invHealthRatio);
			}
			else
			{
				desiredFocusBlendLevel		= SlowInOut(invHealthRatio);
			}

			m_TargetPedOnPreviousUpdateForShootingFocus	= targetPed;
		}
	}
	else if (m_TargetPedOnPreviousUpdateForShootingFocus)
	{
		m_TargetPedOnPreviousUpdateForShootingFocus		= NULL;
	}
	
#if __BANK
	if (ms_DebugForceFullShootingFocus)
	{
		desiredFocusBlendLevel				= 1.0f;
	}
#endif // __BANK
	
	const bool shouldCutSpring				= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float blendLevelOnPreviousUpdate	= m_ShootingFocusSpring.GetResult();
	const float springConstant				= shouldCutSpring ? 0.0f :
												desiredFocusBlendLevel >= (blendLevelOnPreviousUpdate - SMALL_FLOAT) ?
												m_ShootingFocusSettings.m_BlendInSpringConstant : m_ShootingFocusSettings.m_BlendOutSpringConstant;

	m_ShootingFocusSpring.Update(desiredFocusBlendLevel, springConstant, m_ShootingFocusSettings.m_SpringDampingRatio);
}

void camThirdPersonPedAssistedAimCamera::UpdateCinematicMoment()
{
	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return;
	}

	UpdateTargetPedForCinematicMoment();

	ComputeTargetPositionForCinematicMoment(m_LastValidTargetPedForCinematicMoment.Get(), m_TargetPositionForCinematicMoment);

	const float blendLevelOnPreviousUpdate	= m_CinematicMomentBlendLevel;
	const u32 currentTime					= fwTimer::GetCamTimeInMilliseconds();

	//start cinematic moment if we haven't been running and we now have a valid target
	const bool envelopeIsInAttackPhase		= m_CinematicMomentBlendEnvelope ? m_CinematicMomentBlendEnvelope->IsInAttackPhase() : false;
	if (m_LastValidTargetPedForCinematicMoment && blendLevelOnPreviousUpdate < SMALL_FLOAT && !envelopeIsInAttackPhase)
	{
		if (m_CoverSettingsToConsider.m_IsInCover)
		{
			m_IsCameraOnRightForCinematicMoment = !m_CoverSettingsToConsider.m_IsFacingLeftInCover;
		}
		else
		{
			m_IsCameraOnRightForCinematicMoment = ComputeIsCameraOnRightOfActionLine();
		}

		m_TimeAtStartOfLastCinematicMoment	= currentTime;
		const u32 deadPedLockOnDuration		= (u32)(g_DefaultDeadPedLockOnDuration * 1000.0f);
		const float durationScale			= m_CinematicMomentType == FOCUS_TARGET ? m_CinematicMomentSettings.m_TargetFocusDeadPedLockOnDurationScale : m_CinematicMomentSettings.m_PlayerFocusDeadPedLockOnDurationScale;
		m_CinematicMomentDuration			= BANK_ONLY(ms_DebugOverriddenCinematicMomentDuration > -1 ? (u32)ms_DebugOverriddenCinematicMomentDuration : )
												(u32)floor(deadPedLockOnDuration * durationScale) - 250;

		if (m_CinematicMomentBlendEnvelope)
		{
			m_CinematicMomentBlendEnvelope->OverrideHoldDuration(m_CinematicMomentDuration);
			if(m_NumUpdatesPerformed == 0)
			{
				m_CinematicMomentBlendEnvelope->Start(1.0f);
			}
			else
			{
				m_CinematicMomentBlendEnvelope->AutoStartStop(true);
			}
		}
		else
		{
			m_CinematicMomentBlendLevel = 1.0f;
		}

		//determine if this cinematic moment should be a kill shot
		const float randomFloat = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		m_IsCinematicMomentAKillShot = BANK_ONLY(ms_DebugForceCinematicKillShotOff ? false : ms_DebugForceCinematicKillShotOn ? true : )
										(randomFloat < m_CinematicMomentSettings.m_LikelihoodToTriggerCinematicCut);

		//determine what type of cinematic moment to run as a backup plan
		bool isReactionShotAllowed = true;
		if (m_CoverSettingsToConsider.m_IsInCover)
		{
			isReactionShotAllowed = false;

			if (m_CoverSettingsToConsider.m_CanFireRoundCornerCover && m_Metadata.m_InCoverSettings.m_ShouldAllowPlayerReactionShotsIfCanShootAroundCorner)
			{
				isReactionShotAllowed = true;
			}
		}

		const int maxNumCinematicMomentTypes	= !isReactionShotAllowed ? NUM_CINEMATIC_MOMENT_TYPES - 1 : NUM_CINEMATIC_MOMENT_TYPES; //cover doesn't allow FOCUS_PLAYER
		const int randomInt						= fwRandom::GetRandomNumberInRange(0, maxNumCinematicMomentTypes);
		m_CinematicMomentType					= BANK_ONLY(ms_DebugForcePlayerFraming && isReactionShotAllowed ? FOCUS_PLAYER : ) static_cast<eCinematicMomentType>(randomInt);

		m_HasSetDesiredOrientationForFramingPlayer = false;
	}

	//update everything
	if (m_CinematicMomentBlendEnvelope)
	{
		m_CinematicMomentBlendLevel = m_CinematicMomentBlendEnvelope->Update();
		m_CinematicMomentBlendLevel = Clamp(m_CinematicMomentBlendLevel, 0.0f, 1.0f);
		m_CinematicMomentBlendLevel = SlowInOut(m_CinematicMomentBlendLevel);
	}
	else
	{
		const u32 timeSinceLastCinematicMomentStarted = currentTime - m_TimeAtStartOfLastCinematicMoment;
		if (timeSinceLastCinematicMomentStarted > m_CinematicMomentDuration)
		{
			m_CinematicMomentBlendLevel = 0.0f;
		}
	}

	UpdateCinematicMomentLookAroundBlocking();
	
	//see if the cinematic moment has just ended
	if (blendLevelOnPreviousUpdate >= SMALL_FLOAT && m_CinematicMomentBlendLevel < SMALL_FLOAT)
	{
		//clean up
		m_LastValidTargetPedForCinematicMoment = NULL;
		m_TargetPositionForCinematicMoment = g_InvalidPosition;
	}

	if (m_CinematicMomentBlendLevel < SMALL_FLOAT)
	{
		return; //no more work to do
	}

	//slow time
	const float timeScale = Lerp(m_CinematicMomentBlendLevel, 1.0f, m_CinematicMomentSettings.m_MaxSlowTimeScaling);
	m_Frame.SetCameraTimeScaleThisUpdate(timeScale);
}

void camThirdPersonPedAssistedAimCamera::UpdateTargetPedForCinematicMoment()
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour || m_NumUpdatesPerformed == 0)
	{
		return;
	}

	if (m_LastValidTargetPedForCinematicMoment && m_LastValidTargetPedForCinematicMoment->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
	{
		m_LastValidTargetPedForCinematicMoment = NULL;
	}

	const bool cinematicMomentHasStarted = m_CinematicMomentBlendEnvelope ? m_CinematicMomentBlendEnvelope->IsActive() : (m_CinematicMomentBlendLevel >= SMALL_FLOAT);
	if (cinematicMomentHasStarted)
	{
		return; //don't change anything
	}

	const u32 currentTime							= fwTimer::GetCamTimeInMilliseconds();
	const u32 timeSinceStartOfLastCinematicMoment	= currentTime - m_TimeAtStartOfLastCinematicMoment;

	if (timeSinceStartOfLastCinematicMoment < m_CinematicMomentSettings.m_MinTimeBetweenTriggers BANK_ONLY(&& !ms_DebugAlwaysTriggerCinematicMoments) )
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	if (!m_LockOnTarget || !m_LockOnTarget->GetIsTypePed())
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	//Note: m_AttachParent and m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed					= static_cast<const CPed*>(m_AttachParent.Get());
	const CPedWeaponManager* weaponManager	= attachPed->GetWeaponManager();
	const CWeapon* equippedWeapon			= weaponManager ? weaponManager->GetEquippedWeapon() : NULL;
	const s32 numBulletsRemaining			= equippedWeapon ? equippedWeapon->GetAmmoInClip() : 0;

	if (numBulletsRemaining < (s32)m_CinematicMomentSettings.m_MinNumBulletsRemaining)
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	const CPedIntelligence* attachPedIntelligence				= attachPed->GetPedIntelligence();
	const CQueriableInterface* attachPedQueriableIntelligence	= attachPedIntelligence ? attachPedIntelligence->GetQueriableInterface() : NULL;
	if (attachPedQueriableIntelligence && attachPedQueriableIntelligence->IsTaskCurrentlyRunning(CTaskTypes::TASK_RELOAD_GUN))
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	const CPed* desiredTargetPed = static_cast<const CPed*>(m_LockOnTarget.Get());
	if (desiredTargetPed == m_LastRejectedTargetPedForCinematicMoment)
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	const CPedIntelligence* targetPedIntelligence	= desiredTargetPed->GetPedIntelligence();
	const CQueriableInterface* queriableInterface	= targetPedIntelligence ? targetPedIntelligence->GetQueriableInterface() : NULL;
	const bool isTargetPedBeingDraggedToSafety		= queriableInterface && queriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_DRAGGED_TO_SAFETY);

	if(isTargetPedBeingDraggedToSafety)
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	const float currentHealth				= desiredTargetPed->GetHealth();
	const float injuredHealth				= desiredTargetPed->GetInjuredHealthThreshold();
	const float maxHealth					= desiredTargetPed->GetMaxHealth();
	const float desiredTargetPedHealthRatio	= ClampRange(currentHealth, injuredHealth, maxHealth);

	if (desiredTargetPedHealthRatio > m_CinematicMomentSettings.m_MaxTargetHealthRatio)
	{
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	const float randomFloat			= fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	//ramp likelihood to trigger to 1.0f as the time since last cinematic moment reaches max time between triggers
	const float likelihoodToTrigger	= RampValueSafe((float)timeSinceStartOfLastCinematicMoment, (float)m_CinematicMomentSettings.m_MinTimeBetweenTriggers, (float)m_CinematicMomentSettings.m_MaxTimeBetweenTriggers,
										m_CinematicMomentSettings.m_MinLikelihoodToTrigger, m_CinematicMomentSettings.m_MaxLikelihoodToTrigger);

	if (randomFloat > likelihoodToTrigger BANK_ONLY(&& !ms_DebugAlwaysTriggerCinematicMoments) )
	{
		//we don't want to keep checking this ped because he's unlucky, even though his health is low enough
		m_LastRejectedTargetPedForCinematicMoment	= desiredTargetPed;
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	bool hasLineOfSight = true;
	if (!desiredTargetPed->GetIsInVehicle())
	{
		//do line test to torso to make sure we can see the selected ped
		Vector3 targetPedLockOnPosition;
		ComputeTargetPositionForCinematicMoment(desiredTargetPed, targetPedLockOnPosition);

		hasLineOfSight = ComputeHasLineOfSight(m_Frame.GetPosition(), targetPedLockOnPosition, desiredTargetPed);
	}

	if (!hasLineOfSight)
	{
		//we don't want to keep checking this ped because we don't have line of sight, even though his health is low enough
		m_LastRejectedTargetPedForCinematicMoment	= desiredTargetPed;
		m_LastValidTargetPedForCinematicMoment		= NULL;
		return;
	}

	//this ped has passed all the tests. if we get here, there must not be a cinematic moment running already and this is a new ped
	m_LastValidTargetPedForCinematicMoment = desiredTargetPed; 
}

void camThirdPersonPedAssistedAimCamera::ComputeTargetPositionForCinematicMoment(const CPed* targetPed, Vector3& position) const
{
	if(!targetPed)
	{
		return;
	}

	position = VEC3V_TO_VECTOR3(targetPed->GetTransform().GetPosition()); //fallback

	const crSkeleton* pedSkeleton = targetPed->GetSkeleton();
	if(!pedSkeleton)
	{
		return;
	}

	const s32 rootBoneIndex = targetPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
	if(rootBoneIndex == -1)
	{
		return;
	}

	//NOTE: We manually transform the local bone matrix into world space here, as the ped's skeleton may not yet have been updated.
	const Matrix34& localBoneMatrix	= RCC_MATRIX34(pedSkeleton->GetLocalMtx(rootBoneIndex));
	const Matrix34 pedMatrix		= MAT34V_TO_MATRIX34(targetPed->GetTransform().GetMatrix());

	Matrix34 matrix;
	matrix.Dot(localBoneMatrix, pedMatrix);

	position = matrix.d;
}

bool camThirdPersonPedAssistedAimCamera::ComputeHasLineOfSight(const Vector3& cameraPosition, const Vector3& lookAtPosition, const CEntity* targetEntity) const
{
	WorldProbe::CShapeTestProbeDesc lineTest;
	lineTest.SetIsDirected(true);
	lineTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER |  ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE);
	lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	lineTest.SetContext(WorldProbe::LOS_Camera);
	const CEntity* excludeEntities[2] = {targetEntity, m_AttachParent.Get()};
	lineTest.SetExcludeEntities(excludeEntities, 2);

	lineTest.SetStartAndEnd(cameraPosition, lookAtPosition);
	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);

	return !hasHit;
}

bool camThirdPersonPedAssistedAimCamera::ComputeIsCameraOnRightOfActionLine() const
{
	//NOTE: m_LastValidTargetPedForCinematicMoment and m_AttachParent have already been validated.

	const Vector3 targetPedPosition		= VEC3V_TO_VECTOR3(m_LastValidTargetPedForCinematicMoment->GetTransform().GetPosition());
	const Vector3 attachPedPosition		= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
	Vector3 actionLine					= attachPedPosition - targetPedPosition;
	actionLine.NormalizeSafe();

	const Vector3& cameraPosition		= m_Frame.GetPosition();
	Vector3 targetPedToCamera			= cameraPosition - targetPedPosition;
	targetPedToCamera.NormalizeSafe();

	const float cameraAngleToActionLine	= camFrame::ComputeHeadingDeltaBetweenFronts(actionLine, targetPedToCamera);
	
	return (cameraAngleToActionLine > 0.0f);
}

void camThirdPersonPedAssistedAimCamera::UpdateCinematicMomentLookAroundBlocking()
{
	if (m_CinematicMomentBlendEnvelope && m_CinematicMomentBlendEnvelope->IsInAttackPhase())
	{
		m_LookAroundBlendLevelSpring.OverrideResult(0.0f);
	}
	else
	{
		const camThirdPersonPedAssistedAimCameraLockOnAlignmentSettings& settings = m_CinematicMomentSettings.m_LockOnAlignmentSettings;

		const float cinematicMomentLookAroundBlendLevel = IsRunningFocusPlayerCinematicMoment() ? 0.0f : 
															Lerp(m_CinematicMomentBlendLevel, 1.0f, settings.m_MaxLookAroundBlendLevel);
		m_LookAroundBlendLevelSpring.Update(cinematicMomentLookAroundBlendLevel, settings.m_LookAroundBlockedInputSpringConstant, settings.m_LookAroundBlockedInputSpringDampingRatio);
	}
}

bool camThirdPersonPedAssistedAimCamera::IsRunningFocusPlayerCinematicMoment() const
{
	const bool isRunning = (m_CinematicMomentType == FOCUS_PLAYER && (m_CinematicCutBlendLevel >= SMALL_FLOAT) && !m_IsCinematicMomentAKillShot);
	return isRunning;
}

void camThirdPersonPedAssistedAimCamera::UpdateCinematicCut()
{
	//if a cinematic kill shot is supposed to be running, and it is still valid after a frame, force cinematic type to be default (FOCUS_PLAYER is a cut)
	if ((m_CinematicCutBlendLevel >= SMALL_FLOAT) && m_IsCinematicMomentAKillShot && m_NumUpdatesPerformed == m_FirstUpdateOfCinematicCut + 1)
	{
		const camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		const camBaseCamera* renderedCinematicCamera	= cinematicDirector.GetRenderedCamera();
		const bool isRenderingTwoShotCinematicCamera	= renderedCinematicCamera && renderedCinematicCamera->GetIsClassId(camCinematicTwoShotCamera::GetStaticClassId());
		if (isRenderingTwoShotCinematicCamera)
		{
			m_CinematicMomentType = FOCUS_TARGET;
		}
		else
		{
			//use the backup m_CinematicMomentType
			m_IsCinematicMomentAKillShot = false;
			m_HasSetDesiredOrientationForFramingPlayer = false;
		}
	}

	const bool isCinematicMomentACut = m_IsCinematicMomentAKillShot || m_CinematicMomentType == FOCUS_PLAYER;

	if (!isCinematicMomentACut)
	{
		m_CinematicCutBlendLevel = 0.0f;
		m_CinematicCutBlendLevelOnPreviousUpdate = 0.0f;
		return;
	}

	m_CinematicCutBlendLevelOnPreviousUpdate		= m_CinematicCutBlendLevel;

	const u32 currentTime							= fwTimer::GetCamTimeInMilliseconds();
	const u32 elapsedTimeSinceCinematicMomentStart	= currentTime - m_TimeAtStartOfLastCinematicMoment;
	const bool shouldCutCinematicMoment				= m_CinematicMomentType == FOCUS_PLAYER && m_CoverSettingsToConsider.m_IsInCover ? true : false;
	const u32 cinematicMomentAttackDuration			= shouldCutCinematicMoment || !m_CinematicMomentBlendEnvelope ? 0 : m_CinematicMomentBlendEnvelope->GetAttackDuration();

	const u32 cinematicMomentDurationAtRelease		= m_CinematicMomentDuration + cinematicMomentAttackDuration;

	if (elapsedTimeSinceCinematicMomentStart < cinematicMomentDurationAtRelease)
	{
		//delay the start of the cinematic cut
		if (m_IsCinematicMomentAKillShot)
		{
			const s32 timeUntilCinematicMomentHold	= (s32)cinematicMomentAttackDuration - (s32)elapsedTimeSinceCinematicMomentStart; //-ve = time since hold happened
			const s32 timeUntilKillShot				= (s32)m_CinematicMomentSettings.m_DelayTimeBeforeStartingCinematicCut + timeUntilCinematicMomentHold;
			m_CinematicCutBlendLevel				= timeUntilKillShot > 0.0f ? 0.0f : 1.0f;
		}
		else
		{
			if (m_CinematicMomentSettings.m_PlayerFramingSettings.m_AttackDuration == 0 && m_CinematicCutBlendLevelOnPreviousUpdate < SMALL_FLOAT)
			{
				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}

			const float elapsedTimeAfterDelay	= (float)elapsedTimeSinceCinematicMomentStart - (float)m_CinematicMomentSettings.m_PlayerFramingSettings.m_AttackDelay;
			m_CinematicCutBlendLevel			= (m_CinematicMomentSettings.m_PlayerFramingSettings.m_AttackDuration > 0) ?
													(elapsedTimeAfterDelay / (float)m_CinematicMomentSettings.m_PlayerFramingSettings.m_AttackDuration) : 1.0f;
			m_CinematicCutBlendLevel			= camBaseSplineCamera::EaseOut(m_CinematicCutBlendLevel); //EaseIn
			m_CinematicCutBlendLevel			= Clamp(m_CinematicCutBlendLevel, 0.0f, 1.0f);
		}
	}
	else
	{
		const u32 elapsedTimeSinceCinematicMomentRelease = elapsedTimeSinceCinematicMomentStart - cinematicMomentDurationAtRelease;

		u32 delayTimeBeforeEndingCinematicCut;
		if (m_IsCinematicMomentAKillShot)
		{
			delayTimeBeforeEndingCinematicCut = m_CinematicMomentSettings.m_DelayTimeBeforeEndingCinematicCut;
		}
		else
		{
			delayTimeBeforeEndingCinematicCut = m_CinematicMomentSettings.m_PlayerFramingSettings.m_ReleaseDelay;
		}

		//delay the end of the cinematic cut
		if (elapsedTimeSinceCinematicMomentRelease < delayTimeBeforeEndingCinematicCut)
		{
			m_CinematicCutBlendLevel = 1.0f;
		}
		else
		{
			if (m_CinematicCutBlendLevelOnPreviousUpdate >= SMALL_FLOAT)
			{
				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}

			m_CinematicCutBlendLevel = 0.0f;
		}
	}

	//if this is the first frame of a cinematic kill shot, note the frame number
	if ((m_CinematicCutBlendLevel >= SMALL_FLOAT) && (m_CinematicCutBlendLevelOnPreviousUpdate < SMALL_FLOAT) && m_IsCinematicMomentAKillShot)
	{
		m_FirstUpdateOfCinematicCut = m_NumUpdatesPerformed;
	}
}

bool camThirdPersonPedAssistedAimCamera::ComputeShouldForceAttachPedPelvisOffsetToBeConsidered() const
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return camThirdPersonPedAimCamera::ComputeShouldForceAttachPedPelvisOffsetToBeConsidered();
	}

	return true;
}

bool camThirdPersonPedAssistedAimCamera::ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const
{
	//NOTE: Inhibit lock-on during the initial update to ensure we have a valid starting orientation prior to damped lock-on.
	const bool isLockedOn	= (m_NumUpdatesPerformed > 0) && camThirdPersonPedAimCamera::ComputeLockOnTarget(lockOnTarget, lockOnTargetPosition,
								fineAimOffset);

	return isLockedOn;
}

void camThirdPersonPedAssistedAimCamera::ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const
{
	camThirdPersonPedAimCamera::ComputeScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);

	float shootingScreenRatioForMinFootRoom = m_ShootingFocusSettings.m_ScreenRatioForMinFootRoom;
	float shootingScreenRatioForMaxFootRoom = m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoom;

	float cinematicScreenRatioForMinFootRoom = m_CinematicMomentSettings.m_ScreenRatioForMinFootRoom;
	float cinematicScreenRatioForMaxFootRoom = m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoom;

	if(ShouldUseTightSpaceCustomFraming())
	{
		const float tightSpaceBlendLevel	= m_TightSpaceSpring.GetResult();
		shootingScreenRatioForMinFootRoom	= Lerp(tightSpaceBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMinFootRoom, m_ShootingFocusSettings.m_ScreenRatioForMinFootRoomInTightSpace);
		shootingScreenRatioForMaxFootRoom	= Lerp(tightSpaceBlendLevel, m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoom, m_ShootingFocusSettings.m_ScreenRatioForMaxFootRoomInTightSpace);

		cinematicScreenRatioForMinFootRoom	= Lerp(tightSpaceBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMinFootRoom, m_CinematicMomentSettings.m_ScreenRatioForMinFootRoomInTightSpace);
		cinematicScreenRatioForMaxFootRoom	= Lerp(tightSpaceBlendLevel, m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoom, m_CinematicMomentSettings.m_ScreenRatioForMaxFootRoomInTightSpace);

		//the default third person ped aim in cover camera doesn't change with tight space
	}

	const float coverFramingBlendLevel		= m_CoverFramingSpring.GetResult();
	screenRatioForMinFootRoom				= Lerp(coverFramingBlendLevel, screenRatioForMinFootRoom, m_Metadata.m_InCoverSettings.m_ScreenRatioForMinFootRoom);
	screenRatioForMaxFootRoom				= Lerp(coverFramingBlendLevel, screenRatioForMaxFootRoom, m_Metadata.m_InCoverSettings.m_ScreenRatioForMaxFootRoom);

	const float shootingFocusBlendLevel		= m_ShootingFocusSpring.GetResult();
	screenRatioForMinFootRoom				= Lerp(shootingFocusBlendLevel, screenRatioForMinFootRoom, shootingScreenRatioForMinFootRoom);
	screenRatioForMaxFootRoom				= Lerp(shootingFocusBlendLevel, screenRatioForMaxFootRoom, shootingScreenRatioForMaxFootRoom);

	screenRatioForMinFootRoom				= Lerp(m_CinematicMomentBlendLevel, screenRatioForMinFootRoom, cinematicScreenRatioForMinFootRoom);
	screenRatioForMaxFootRoom				= Lerp(m_CinematicMomentBlendLevel, screenRatioForMaxFootRoom, cinematicScreenRatioForMaxFootRoom);
}

void camThirdPersonPedAssistedAimCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	const Vector3 originalBaseFront = m_BaseFront;

	camThirdPersonPedAimCamera::ComputeOrbitOrientation(orbitHeading, orbitPitch);

	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed && followPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON))
	{
		return;
	}

	m_BaseFront = originalBaseFront;

	camFrame::ComputeHeadingAndPitchFromFront(m_BaseFront, orbitHeading, orbitPitch);

	float desiredLockOnAlignmentHeading;
	float desiredLockOnAlignmentPitch;

	const float maxZoomFactor			= Max(m_ShootingFocusSettings.m_MaxZoomFactor, m_CinematicMomentSettings.m_ZoomFactor);
	const float baseVerticalFov			= ComputeBaseFov();
	const float maxZoomedVerticalFov	= baseVerticalFov / maxZoomFactor;
	ComputeDesiredLockOnOrientation(maxZoomedVerticalFov, desiredLockOnAlignmentHeading, desiredLockOnAlignmentPitch);

	//Blend out the lock-on alignment behaviour when look-around input is present.
	//NOTE: m_ControlHelper has already been validated.
	const float lookAroundInputEnvelopeLevel		= m_ControlHelper->GetLookAroundInputEnvelopeLevel();
	const float lookAroundInputEnvelopeLevelToUse	= m_CinematicMomentBlendEnvelope && m_CinematicMomentBlendEnvelope->IsInAttackPhase() ?
														0.0f : lookAroundInputEnvelopeLevel; //no look around when in attack phase
	const float lockOnAlignmentBlendLevel			= SlowInOut(1.0f - lookAroundInputEnvelopeLevelToUse);

	//If we are blending in the lock-on orientation based upon the overall cinematic moment blend, determine the blend level required this update to allow an
	//appropriate blend in response given that the orientation change is applied cumulatively.
	//Simply blending in the desired lock-on orientation conventionally results in a skewed blend.
	float blendLevelToApply;
	if((m_CinematicMomentBlendLevel >= m_CinematicMomentBlendLevelOnPreviousUpdate) && ((1.0f - m_CinematicMomentBlendLevelOnPreviousUpdate) >= SMALL_FLOAT))
	{
		blendLevelToApply = (m_CinematicMomentBlendLevel - m_CinematicMomentBlendLevelOnPreviousUpdate) / (1.0f - m_CinematicMomentBlendLevelOnPreviousUpdate);
		blendLevelToApply *= lockOnAlignmentBlendLevel;

		m_OrbitHeadingToApply	= fwAngle::LerpTowards(orbitHeading, desiredLockOnAlignmentHeading, blendLevelToApply); 
		orbitHeading			= m_OrbitHeadingToApply;

		m_OrbitPitchToApply		= fwAngle::LerpTowards(orbitPitch, desiredLockOnAlignmentPitch, blendLevelToApply);
		orbitPitch				= m_OrbitPitchToApply;
	}
	else if (m_CinematicMomentBlendLevel < m_CinematicMomentBlendLevelOnPreviousUpdate)
	{
		blendLevelToApply = m_CinematicMomentBlendLevel;
		blendLevelToApply *= lockOnAlignmentBlendLevel;

		const float orbitHeadingToApply	= fwAngle::LerpTowards(orbitHeading, m_OrbitHeadingToApply, blendLevelToApply); 
		orbitHeading					= orbitHeadingToApply;

		const float orbitPitchToApply	= fwAngle::LerpTowards(orbitPitch, m_OrbitPitchToApply, blendLevelToApply);
		orbitPitch						= orbitPitchToApply;
	}
	else
	{
		blendLevelToApply = m_CinematicMomentBlendLevel;
		blendLevelToApply *= lockOnAlignmentBlendLevel;

		m_OrbitHeadingToApply	= fwAngle::LerpTowards(orbitHeading, desiredLockOnAlignmentHeading, blendLevelToApply); 
		orbitHeading			= m_OrbitHeadingToApply;

		m_OrbitPitchToApply		= fwAngle::LerpTowards(orbitPitch, desiredLockOnAlignmentPitch, blendLevelToApply);
		orbitPitch				= m_OrbitPitchToApply;
	}

	m_CinematicMomentBlendLevelOnPreviousUpdate = m_CinematicMomentBlendLevel;
}

void camThirdPersonPedAssistedAimCamera::ComputeDesiredLockOnOrientation(const float verticalFov, float& desiredOrbitHeading, float& desiredOrbitPitch)
{
	float orbitHeading;
	float orbitPitch;
	camFrame::ComputeHeadingAndPitchFromFront(m_BaseFront, orbitHeading, orbitPitch);

	desiredOrbitHeading = orbitHeading;
	desiredOrbitPitch = orbitPitch;

	if (m_CinematicMomentBlendLevel < SMALL_FLOAT)
	{
		return;
	}

	//if we're blending to a player focus cinematic moment we blend the desired orientation

	if (IsRunningFocusPlayerCinematicMoment())
	{
		float playerFramingDesiredHeading	= desiredOrbitHeading;
		float playerFramingDesiredPitch		= desiredOrbitPitch;
		ComputeDesiredLockOnOrientationFramingPlayer(playerFramingDesiredHeading, playerFramingDesiredPitch);

		if (m_CinematicCutBlendLevel < (1.0f - SMALL_FLOAT))
		{
			float targetFramingDesiredHeading	= desiredOrbitHeading;
			float targetFramingDesiredPitch		= desiredOrbitPitch;
			ComputeDesiredLockOnOrientationFramingTarget(verticalFov, targetFramingDesiredHeading, targetFramingDesiredPitch);

			desiredOrbitHeading = fwAngle::LerpTowards(targetFramingDesiredHeading, playerFramingDesiredHeading, m_CinematicCutBlendLevel);
			desiredOrbitPitch	= fwAngle::LerpTowards(targetFramingDesiredPitch, playerFramingDesiredPitch, m_CinematicCutBlendLevel);
		}
		else
		{
			desiredOrbitHeading = playerFramingDesiredHeading;
			desiredOrbitPitch	= playerFramingDesiredPitch;
		}
	}
	else
	{
		ComputeDesiredLockOnOrientationFramingTarget(verticalFov, desiredOrbitHeading, desiredOrbitPitch);
	}
}

void camThirdPersonPedAssistedAimCamera::ComputeDesiredLockOnOrientationFramingTarget(const float verticalFov, float& desiredOrbitHeading, float& desiredOrbitPitch) const
{
	const camThirdPersonPedAssistedAimCameraLockOnAlignmentSettings& settings = m_CinematicMomentSettings.m_LockOnAlignmentSettings;

	float orbitHeading;
	float orbitPitch;
	camFrame::ComputeHeadingAndPitchFromFront(m_BaseFront, orbitHeading, orbitPitch);

	const bool targetPositionIsValid = !m_TargetPositionForCinematicMoment.IsClose(g_InvalidPosition, SMALL_FLOAT);
	if (!targetPositionIsValid)
	{
		return;
	}

	const Vector3 lockOnOrbitDelta	= m_TargetPositionForCinematicMoment - m_BasePivotPosition;
	const float distanceToTarget	= lockOnOrbitDelta.Mag();
	if(distanceToTarget < m_Metadata.m_MinDistanceForLockOn)
	{
		return;
	}

	Vector3 lockOnOrbitFront;
	lockOnOrbitFront.Normalize(lockOnOrbitDelta);

	Matrix34 lockOnOrbitMatrix(Matrix34::IdentityType);
	camFrame::ComputeWorldMatrixFromFront(lockOnOrbitFront, lockOnOrbitMatrix);

	Vector3 orbitRelativePivotOffset;
	ComputeOrbitRelativePivotOffset(m_BaseFront, orbitRelativePivotOffset);

	const float sinHeadingDelta		= orbitRelativePivotOffset.x / distanceToTarget;
	const float headingDelta		= AsinfSafe(sinHeadingDelta);

	const float sinPitchDelta		= -orbitRelativePivotOffset.z / distanceToTarget;
	float pitchDelta				= AsinfSafe(sinPitchDelta);

	const float orbitPitchOffset	= ComputeOrbitPitchOffset();
	pitchDelta						-= orbitPitchOffset;

	lockOnOrbitMatrix.RotateLocalZ(headingDelta);
	lockOnOrbitMatrix.RotateLocalX(pitchDelta);

	float lockOnPitch;
	float lockOnHeading;
	camFrame::ComputeHeadingAndPitchFromMatrix(lockOnOrbitMatrix, lockOnHeading, lockOnPitch);

	Vector2 orbitPitchLimitsToApply;
	orbitPitchLimitsToApply.SetScaled(m_Metadata.m_OrbitPitchLimits, DtoR);

	lockOnPitch					= Clamp(lockOnPitch, orbitPitchLimitsToApply.x, orbitPitchLimitsToApply.y);

	const CViewport* viewport	= gVpMan.GetGameViewport();
	const float aspectRatio		= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const float horizontalFov	= verticalFov * aspectRatio;

	//Apply a  bias in screen space.
	lockOnPitch					+= DtoR * verticalFov * settings.m_ScreenHeightRatioBiasForSafeZone;
	lockOnHeading				+= DtoR * horizontalFov * settings.m_ScreenWidthRatioBiasForSafeZone * (m_IsCameraOnRightForCinematicMoment ? 1.0f : -1.0f);

	const float headingError	= fwAngle::LimitRadianAngle(lockOnHeading - orbitHeading);
	const float absHeadingError	= Abs(headingError);

	if(absHeadingError >= SMALL_FLOAT)
	{
		//Shrink the safe zone when we don't have LOS to the lock-on target.
		float maxAbsHeadingError	= 0.0f;
		if(m_HadLosToLockOnTargetOnPreviousUpdate)
		{
			maxAbsHeadingError		= 0.5f * DtoR * horizontalFov * settings.m_ScreenWidthRatioForSafeZone;
		}

		const float lerpT	= Clamp((absHeadingError - maxAbsHeadingError) / absHeadingError, 0.0f, 1.0f);
		desiredOrbitHeading	= fwAngle::LerpTowards(orbitHeading, lockOnHeading, lerpT);

		//Ensure that we blend to the desired orientation over the shortest angle.
		const float desiredOrbitHeadingDelta = desiredOrbitHeading - orbitHeading;
		if(desiredOrbitHeadingDelta > PI)
		{
			desiredOrbitHeading -= TWO_PI;
		}
		else if(desiredOrbitHeadingDelta < -PI)
		{
			desiredOrbitHeading += TWO_PI;
		}
	}

	const float pitchError		= fwAngle::LimitRadianAngle(lockOnPitch - orbitPitch);
	const float absPitchError	= Abs(pitchError);

	if(absPitchError >= SMALL_FLOAT)
	{
		//Shrink the safe zone when we don't have LOS to the lock-on target.
		const float maxAbsPitchError	 = m_HadLosToLockOnTargetOnPreviousUpdate ? (0.5f * DtoR * verticalFov * settings.m_ScreenHeightRatioForSafeZone) : 0.0f;
		const float lerpT				= Clamp((absPitchError - maxAbsPitchError) / absPitchError, 0.0f, 1.0f);
		desiredOrbitPitch				= fwAngle::LerpTowards(orbitPitch, lockOnPitch, lerpT);
	}
}

void camThirdPersonPedAssistedAimCamera::ComputeDesiredLockOnOrientationFramingPlayer(float& desiredOrbitHeading, float& desiredOrbitPitch)
{
	const bool targetPositionIsValid = !m_TargetPositionForCinematicMoment.IsClose(g_InvalidPosition, SMALL_FLOAT);
	if (!targetPositionIsValid)
	{
		return;
	}

	const Vector3 lockOnOrbitDelta	= m_TargetPositionForCinematicMoment - m_BasePivotPosition;
	const float distanceToTarget2	= lockOnOrbitDelta.Mag2();
	if(distanceToTarget2 < m_Metadata.m_MinDistanceForLockOn * m_Metadata.m_MinDistanceForLockOn)
	{
		return;
	}

	if (!m_HasSetDesiredOrientationForFramingPlayer)
	{
		const Vector3 attachParentFront	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetForward());
		camFrame::ComputeHeadingAndPitchFromFront(-attachParentFront, m_OrbitHeadingForFramingPlayer, m_OrbitPitchForFramingPlayer);

		const float desiredHeading		= m_CinematicMomentSettings.m_PlayerFramingSettings.m_DesiredRelativeHeading * DtoR * (m_IsCameraOnRightForCinematicMoment ? -1.0f : 1.0f);
		m_OrbitHeadingForFramingPlayer	+= desiredHeading;

		m_OrbitPitchForFramingPlayer	= m_CinematicMomentSettings.m_PlayerFramingSettings.m_DesiredPitch * DtoR;

		m_HasSetDesiredOrientationForFramingPlayer = true;
	}

	desiredOrbitHeading	= m_OrbitHeadingForFramingPlayer;
	desiredOrbitPitch	= m_OrbitPitchForFramingPlayer;
}

void camThirdPersonPedAssistedAimCamera::ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const
{
	//Bypass the aim-specific look-around behaviour.
	camThirdPersonCamera::ComputeLookAroundOrientationOffsets(headingOffset, pitchOffset);

	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	const float lookAroundBlendLevel	= m_LookAroundBlendLevelSpring.GetResult();

	headingOffset						*= lookAroundBlendLevel;
	pitchOffset							*= lookAroundBlendLevel;
}

void camThirdPersonPedAssistedAimCamera::UpdatePivotPosition(const Matrix34& orbitMatrix)
{
	camThirdPersonPedAimCamera::UpdatePivotPosition(orbitMatrix);

	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	if (m_NumUpdatesPerformed == 0)
	{
		m_PivotPositionOnPreviousUpdate	= m_PivotPosition;
	}

	const camThirdPersonPedAssistedAimCameraPivotScalingSettings& settings = m_Metadata.m_PivotScalingSettings;

	const Vector3 pivotPositionDelta	= m_PivotPosition - m_PivotPositionOnPreviousUpdate;
	const float horizontalError			= pivotPositionDelta.Dot(orbitMatrix.a);
	const float accumulatedError		= m_PivotPositionSpring.GetResult() + horizontalError;
	m_PivotPositionSpring.OverrideResult(accumulatedError);

	const float cameraInvTimeStep		= fwTimer::GetCamInvTimeStep();
	const float horizontalErrorSpeed	= horizontalError * cameraInvTimeStep;
	const bool isMoving					= Abs(horizontalErrorSpeed) >= settings.m_ErrorThreshold;
	if (isMoving)
	{
		m_IsSideOffsetOnRight = horizontalError < 0.0f;
	}

	if (m_CoverSettingsToConsider.m_IsInCover)
	{
		m_IsSideOffsetOnRight = !m_CoverSettingsToConsider.m_IsFacingLeftInCover;
	}

	const bool sideOffsetOnRightToUse		= (m_CinematicMomentBlendLevel >= SMALL_FLOAT && !isMoving) ? m_IsCameraOnRightForCinematicMoment : m_IsSideOffsetOnRight;

	const bool shouldCutSpring				= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

	const float desiredError				= isMoving ? 0.0f : settings.m_SideOffset * (sideOffsetOnRightToUse ? -1.0f : 1.0f);
	const float dampedDesiredError			= m_DesiredPivotErrorSpring.Update(desiredError,
												shouldCutSpring ? 0.0f : settings.m_DesiredErrorSpringConstant, settings.m_DesiredErrorSpringDampingRatio);

	const float desiredFovWithoutZoom		= camThirdPersonCamera::ComputeDesiredFov();
	const float desiredFov					= ComputeDesiredFov();
	const float currentZoomFactor			= desiredFovWithoutZoom / desiredFov;

	const float dampedErrorMovingSC			= RampValueSafe(currentZoomFactor, 1.0f, settings.m_MaxZoomFactor,
												settings.m_SpringConstant, settings.m_SpringConstantForMaxZoomFactor);
	const float dampedErrorNotMovingSC		= RampValueSafe(currentZoomFactor, 1.0f, settings.m_MaxZoomFactor,
												settings.m_SpringConstantWhenNotMoving, settings.m_SpringConstantWhenNotMovingForMaxZoomFactor);

	const float desiredDampedErrorSC		= shouldCutSpring ? 0.0f : isMoving ? dampedErrorMovingSC : dampedErrorNotMovingSC;
	const float dampedDampedErrorSC			= m_SpringConstantSpring.Update(desiredDampedErrorSC,
												shouldCutSpring ? 0.0f : settings.m_SpringConstantSpringConstant, settings.m_SpringConstantSpringDampingRatio);
	
	const float dampedError					= m_PivotPositionSpring.Update(dampedDesiredError, dampedDampedErrorSC, settings.m_SpringDampingRatio);

	const Vector3 dampedPivotPositionError	= orbitMatrix.a * dampedError;

	m_PivotPositionOnPreviousUpdate			= m_PivotPosition;

	m_PivotPosition							-= (dampedPivotPositionError * (1.0f - m_CinematicMomentBlendLevel));
}

void camThirdPersonPedAssistedAimCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	camThirdPersonPedAimCamera::ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	//we're undoing the side offset since we're calculating our own pivot position independently of this
	orbitRelativeOffset.x -= m_Metadata.m_PivotPosition.m_CameraRelativeSideOffset;

	const camThirdPersonPedAssistedAimCameraLockOnAlignmentSettings& lockOnAlignmentSettings = m_CinematicMomentSettings.m_LockOnAlignmentSettings;
	const camThirdPersonPedAssistedAimCameraPlayerFramingSettings& playerFramingSettings = m_CinematicMomentSettings.m_PlayerFramingSettings;

	const float defaultSideOffset		= m_IsCameraOnRightForCinematicMoment ? lockOnAlignmentSettings.m_PivotPositionLeftSideOffset : lockOnAlignmentSettings.m_PivotPositionRightSideOffset;

	const float focusPlayerSideOffset	= m_IsCameraOnRightForCinematicMoment ? playerFramingSettings.m_PivotPositionLeftSideOffset : playerFramingSettings.m_PivotPositionRightSideOffset;

	const float playerFocusBlendLevel	= IsRunningFocusPlayerCinematicMoment() ? m_CinematicCutBlendLevel : 0.0f;

	const float desiredSideOffset		= Lerp(playerFocusBlendLevel, defaultSideOffset, focusPlayerSideOffset);

	const float desiredVerticalOffset	= Lerp(playerFocusBlendLevel, lockOnAlignmentSettings.m_PivotPositionVerticalOffset, playerFramingSettings.m_PivotPositionVerticalOffset);

	const float sideOffset				= desiredSideOffset * m_CinematicMomentBlendLevel;
	const float verticalOffset			= desiredVerticalOffset * m_CinematicMomentBlendLevel;

	orbitRelativeOffset.x				-= sideOffset;
	orbitRelativeOffset.z				-= verticalOffset;
}

float camThirdPersonPedAssistedAimCamera::ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const
{
	float attachParentHeightRatioToAttain = camThirdPersonPedAimCamera::ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition();

	if (m_CinematicMomentType == FOCUS_PLAYER && !m_IsCinematicMomentAKillShot)
	{
		attachParentHeightRatioToAttain = Lerp(m_CinematicCutBlendLevel, attachParentHeightRatioToAttain,
											m_Metadata.m_CinematicMomentSettings.m_PlayerFramingSettings.m_CollisionFallBackAttachParentHeightRatioToAttain);
	}

	return attachParentHeightRatioToAttain;
}

float camThirdPersonPedAssistedAimCamera::ComputeDesiredFov()
{
	//Bypass the weapon-specific FOV and simply use the value specified in metadata.
	float desiredFov = camThirdPersonCamera::ComputeDesiredFov();
	
	if (m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		const float shootingFocusBlendLevel	= m_ShootingFocusSpring.GetResult();
		const float shootingFocusZoomFactor	= Powf(m_ShootingFocusSettings.m_MaxZoomFactor, shootingFocusBlendLevel);
		const float cinematicZoomFactor		= Powf(m_CinematicMomentSettings.m_ZoomFactor, m_CinematicMomentBlendLevel);

		const float desiredZoomFactor		= Max(shootingFocusZoomFactor, cinematicZoomFactor);

		const bool shouldCutSpring			= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
													m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

		const float dampedZoomFactor		= m_FOVScalingSpring.Update(desiredZoomFactor, shouldCutSpring ? 0.0f : m_Metadata.m_FovScalingSpringConstant,
													m_Metadata.m_FovScalingSpringDampingRatio, true);

		if(dampedZoomFactor >= SMALL_FLOAT)
		{
			desiredFov /= dampedZoomFactor;
		}

		desiredFov = Clamp(desiredFov, g_MinFov, g_MaxFov);
	}

	return desiredFov;
}

float camThirdPersonPedAssistedAimCamera::ComputeCollisionRootPositionFallBackToPivotBlendValue() const
{
	//NOTE: We use a custom blend value when the attach-ped is actually assisted aiming.
	const float blendValue	= m_Metadata.m_ShouldApplyCinematicShootingBehaviour ?
								m_Metadata.m_CollisionRootPositionFallBackToPivotBlendValueForAssistedAiming :
								camThirdPersonPedAimCamera::ComputeCollisionRootPositionFallBackToPivotBlendValue();

	return blendValue;
}

void camThirdPersonPedAssistedAimCamera::UpdateReticuleSettings()
{
	camThirdPersonPedAimCamera::UpdateReticuleSettings();

	//NOTE: We explicitly hide the reticule when the attach-ped is actually assisted aiming.
	m_ShouldDisplayReticule = m_ShouldDisplayReticule && !m_Metadata.m_ShouldApplyCinematicShootingBehaviour;
}

bool camThirdPersonPedAssistedAimCamera::ComputeHasLosToTarget() const
{
	if(!m_IsLockedOn || !m_LockOnTarget)
	{
		return true;
	}

	const Vector3& cameraPosition = m_Frame.GetPosition();

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	capsuleTest.SetResultsStructure(NULL);
	//Consider only map weapon-type and vehicle bounds.
	capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetCapsule(cameraPosition, m_LockOnTargetPosition, m_Metadata.m_LosCapsuleTestRadius);

	const bool hasLosToTarget = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

	return hasLosToTarget;
}

float camThirdPersonPedAssistedAimCamera::UpdateRunningShakeActivityScalingFactor()
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return 0.0f;
	}

	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return 0.0f;
	}

	const camThirdPersonPedAssistedAimCameraShakeActivityScalingSettings& settings = m_Metadata.m_RunningShakeActivityScalingSettings;

	bool hasValidTarget = false;

	if (m_LockOnTarget && m_LockOnTarget->GetIsTypePed())
	{
		hasValidTarget = true;
	}

	const float desiredScalingFactor = hasValidTarget ? settings.m_AmplitudeScale : 1.0f;

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

	const float scalingFactorOnPreviousUpdate = m_RunningShakeActivitySpring.GetResult();
	const float springConstant		= shouldCutSpring ? 0.0f :
										scalingFactorOnPreviousUpdate <= desiredScalingFactor ? settings.m_BlendInSpringConstant : settings.m_BlendOutSpringConstant;

	float dampedScalingFactor		= m_RunningShakeActivitySpring.Update(desiredScalingFactor, springConstant, settings.m_SpringDampingRatio);
	dampedScalingFactor				= Lerp(m_CinematicMomentBlendLevel, dampedScalingFactor, 0.0f);

	return dampedScalingFactor;
}

float camThirdPersonPedAssistedAimCamera::UpdateWalkingShakeActivityScalingFactor()
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return 0.0f;
	}

	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return 0.0f;
	}

	return (1.0f - m_CinematicMomentBlendLevel);
}

void camThirdPersonPedAssistedAimCamera::UpdateMovementShake(const camThirdPersonPedAssistedAimCameraRunningShakeSettings& settings, const float activityShakeScalingFactor BANK_ONLY(, bool shouldForceOn))
{
	if (!m_Metadata.m_ShouldApplyCinematicShootingBehaviour)
	{
		return;
	}

	if (!settings.m_ShakeRef)
	{
		return;
	}

	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return;
	}

	const CPed* attachPed						= static_cast<const CPed*>(m_AttachParent.Get());
	const CPedMotionData* attachPedMotionData	= attachPed->GetMotionData();

	if (!attachPedMotionData)
	{
		return;
	}

	camBaseFrameShaker* shaker	= FindFrameShaker(settings.m_ShakeRef);
	const float moveBlendRatio	= BANK_ONLY(shouldForceOn ? 1.0f :) attachPedMotionData->GetCurrentMoveBlendRatio().Mag();
	const float maxBlendRatio	= settings.m_ShouldScaleWithWalk ? MOVEBLENDRATIO_WALK :
									settings.m_ShouldScaleWithRun ? MOVEBLENDRATIO_RUN :
									settings.m_ShouldScaleWithSprint ? MOVEBLENDRATIO_SPRINT :
									moveBlendRatio;
	const float movementRatio	= Clamp(moveBlendRatio / maxBlendRatio, 0.0f, 1.0f);

	if (!shaker && movementRatio >= SMALL_FLOAT)
	{
		shaker = Shake(settings.m_ShakeRef, movementRatio);
	}

	if (shaker)
	{
		float amplitude	= Lerp(movementRatio, settings.m_MinAmplitude, settings.m_MaxAmplitude);
		amplitude		*= activityShakeScalingFactor;
		shaker->SetAmplitude(amplitude);
	}

}

void camThirdPersonPedAssistedAimCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	camThirdPersonPedAimCamera::ComputeDofOverriddenFocusSettings(focusPosition, blendLevel);

	Vector3 cinematicMomentFocusPosition = m_TargetPositionForCinematicMoment;

	const bool targetPositionIsValid = !cinematicMomentFocusPosition.IsClose(g_InvalidPosition, SMALL_FLOAT);
	if (!targetPositionIsValid)
	{
		return;
	}

	if (IsRunningFocusPlayerCinematicMoment() && m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const Vector3 attachParentPosition	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		cinematicMomentFocusPosition		= Lerp(m_CinematicCutBlendLevel, cinematicMomentFocusPosition, attachParentPosition);
	}

	if (blendLevel >= SMALL_FLOAT && m_CinematicMomentBlendLevel >= SMALL_FLOAT)
	{
		focusPosition.Lerp(m_CinematicMomentBlendLevel, cinematicMomentFocusPosition);
	}
	else if (m_CinematicMomentBlendLevel >= SMALL_FLOAT)
	{
		focusPosition.Set(cinematicMomentFocusPosition);
	}

	blendLevel = Lerp(m_CinematicMomentBlendLevel, blendLevel, 1.0f);
}

void camThirdPersonPedAssistedAimCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camThirdPersonPedAimCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	const camDepthOfFieldSettingsMetadata* tempCustomSettings =
		camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_CinematicMomentSettings.m_LockOnAlignmentSettings.m_DofSettings.GetHash());
	if(!tempCustomSettings)
	{
		return;
	}

	camDepthOfFieldSettingsMetadata customSettings(*tempCustomSettings);

	const float blendLevelToUse = Lerp(m_CinematicMomentBlendLevel, m_LockOnEnvelopeLevel, 1.0f);

	BlendDofSettings(blendLevelToUse, settings, customSettings, settings);
}

#if __BANK
void camThirdPersonPedAssistedAimCamera::DebugRender()
{
	camThirdPersonPedAimCamera::DebugRender();

	if(ms_DebugDrawTargetPosition)
	{
		const bool targetPositionIsValid = !m_TargetPositionForCinematicMoment.IsClose(g_InvalidPosition, SMALL_FLOAT);
		if (targetPositionIsValid)
		{
			const float radius = 0.3f;
			const Color32 colour = m_LastValidTargetPedForCinematicMoment ? Color_green : Color_red;
			grcDebugDraw::Sphere(m_TargetPositionForCinematicMoment, radius, colour);
		}
	}
}

void camThirdPersonPedAssistedAimCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Third person ped assisted aim camera", false);
	{
		bank.AddToggle("Force full shooting focus", &ms_DebugForceFullShootingFocus);
		bank.AddToggle("Force full running shake", &ms_DebugForceFullRunningShake);
		bank.AddToggle("Force full walking shake", &ms_DebugForceFullWalkingShake);
		bank.AddSlider("Overridden cinematic moment duration", &ms_DebugOverriddenCinematicMomentDuration, -1, 30000, 1);
		bank.AddToggle("Draw target position", &ms_DebugDrawTargetPosition);
		bank.AddToggle("Force player reaction shot", &ms_DebugForcePlayerFraming);
		bank.AddToggle("Force cinematic kill shot off", &ms_DebugForceCinematicKillShotOff);
		bank.AddToggle("Force cinematic kill shot on", &ms_DebugForceCinematicKillShotOn);
		bank.AddToggle("Always trigger cinematic moments", &ms_DebugAlwaysTriggerCinematicMoments);
	}
	bank.PopGroup();
}
#endif // __BANK
