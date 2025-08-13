//
// camera/gameplay/aim/ThirdPersonAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonAimCamera.h"

#include "input/headtracking.h"
#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/StickyAimHelper.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "weapons/info/WeaponInfo.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonAimCamera,0xC6239E4D)

PF_PAGE(camThirdPersonAimCameraPage, "Camera: Third-Person Aim Camera");

PF_GROUP(camThirdPersonAimCameraMetrics);
PF_LINK(camThirdPersonAimCameraPage, camThirdPersonAimCameraMetrics);

PF_VALUE_FLOAT(lockOnTargetBlendLevel, camThirdPersonAimCameraMetrics);


camThirdPersonAimCamera::camThirdPersonAimCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonAimCameraMetadata&>(metadata))
, m_LockOnTargetPosition(g_InvalidPosition)
, m_CachedRelativeLockOnTargetPosition(g_InvalidRelativePosition)
, m_LockOnFineAimOffset(VEC3_ZERO)
, m_LockOnEnvelope(NULL)
, m_LockOnTargetBlendStartTime(0)
, m_LockOnTargetBlendDuration(0)
, m_LockOnEnvelopeLevel(0.0f)
, m_LockOnTargetBlendLevelOnPreviousUpdate(0.0f)
, m_CachedLockOnFineAimLookAroundSpeed(0.0f)
, m_OverriddenNearClip(m_Metadata.m_BaseNearClip)
, m_IsLockedOn(false)
, m_ShouldBlendLockOnTargetPosition(false)
, m_ShouldDisplayReticule(m_Metadata.m_ShouldDisplayReticule)
, m_ShouldOverrideNearClipThisUpdate(false)
, m_StickyAimHelper(nullptr)
{
	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LockOnEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_LockOnEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_LockOnEnvelope, "A third-person aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_LockOnEnvelope->SetUseGameTime(true);
		}
	}

	if (CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(camInterface::FindFollowPed()))
	{
		const camStickyAimHelperMetadata* stickyAimMetadata = camFactory::FindObjectMetadata<camStickyAimHelperMetadata>(m_Metadata.m_StickyAimHelperRef.GetHash());
		if(stickyAimMetadata)
		{
			m_StickyAimHelper = camFactory::CreateObject<camStickyAimHelper>(*stickyAimMetadata);
			cameraAssertf(m_StickyAimHelper, "A third-person aim camera (name: %s, hash: %u) failed to create a sticky aim helper (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(stickyAimMetadata->m_Name.GetCStr()), stickyAimMetadata->m_Name.GetHash());
		}	
	}
}

camThirdPersonAimCamera::~camThirdPersonAimCamera()
{
	if(m_LockOnEnvelope)
	{
		delete m_LockOnEnvelope;
	}

	if (m_StickyAimHelper)
	{
		delete m_StickyAimHelper;
	}
}

float camThirdPersonAimCamera::GetRecoilShakeAmplitudeScaling() const
{
	return m_Metadata.m_RecoilShakeAmplitudeScaling;
}

bool camThirdPersonAimCamera::Update()
{
	if(!m_WeaponInfo)
	{
		//Cache the active weapon info for the follow ped, as we do not want this to change during the lifetime of this camera.
		m_WeaponInfo = camInterface::GetGameplayDirector().GetFollowPedWeaponInfo();

		if(!cameraVerifyf(m_WeaponInfo, "A third-person aim camera (name: %s, hash: %u) failed to find valid weapon info for the follow ped",
			GetName(), GetNameHash()))
		{
			return false;
		}
	}

	if(m_ControlHelper)
	{
		//Force the control helper to apply the original maximum aim sensitivity limit for thrown weapons and projectiles, so that we do not
		//introduce additional ped animation problems at higher look-around speeds.
		const bool isThrownWeapon	= m_WeaponInfo->GetIsThrownWeapon();
		const bool isProjectile		= m_WeaponInfo->GetIsProjectile();
		if(isThrownWeapon || isProjectile)
		{
			m_ControlHelper->SetShouldUseOriginalMaxControllerAimSensitivity(true);
		}
	}

	const bool hasSucceeded = camThirdPersonCamera::Update();
	
	if(m_ShouldOverrideNearClipThisUpdate)
	{
		m_Frame.SetNearClip(m_OverriddenNearClip);
	}
#if RSG_PC
	else if(camInterface::IsTripleHeadDisplay() && m_Metadata.m_TripleHeadNearClip > SMALL_FLOAT)
	{
		m_Frame.SetNearClip(m_Metadata.m_TripleHeadNearClip);
	}
#endif

	UpdateReticuleSettings();
	
	//reset values
	m_ShouldOverrideNearClipThisUpdate				= false; 

	if (m_StickyAimHelper)
	{
		m_StickyAimHelper->PostCameraUpdate(m_BasePivotPosition, m_BaseFront);
	}

	return hasSucceeded;
}

void camThirdPersonAimCamera::FlagCutsForOrbitOverrides()
{
	//NOTE: Orbit override requests are ignored when locked on, as we must respect the lock-on orientation.
	if(!m_IsLockedOn)
	{
		camThirdPersonCamera::FlagCutsForOrbitOverrides();
	}
}

void camThirdPersonAimCamera::UpdateLockOn()
{
	const CEntity* desiredLockOnTarget	= NULL;
	Vector3 desiredLockOnTargetPosition	= g_InvalidPosition;
	Vector3 fineAimOffsetOnPreviousUpdate(m_LockOnFineAimOffset);

	m_IsLockedOn = ComputeLockOnTarget(desiredLockOnTarget, desiredLockOnTargetPosition, m_LockOnFineAimOffset);

	UpdateLockOnEnvelopes(desiredLockOnTarget);

	if(m_IsLockedOn && !rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		const bool hasChangedLockOnTargetThisUpdate = (desiredLockOnTarget != m_LockOnTarget);
		if(hasChangedLockOnTargetThisUpdate)
		{
			if(desiredLockOnTarget && (m_NumUpdatesPerformed > 0))
			{
				if(!m_LockOnTarget || m_LockOnTargetPosition.IsClose(g_InvalidPosition, SMALL_FLOAT))
				{
					//We don't currently have a valid lock-on target, so base the starting lock-on position for any blend on the previous camera
					//frame, using a roughly similar projected distance.

					const float previousPivotToDesiredLockOnTargetDistance	= m_PivotPosition.Dist(desiredLockOnTargetPosition);
					const Vector3& cameraFrontOnPreviousUpdate				= m_Frame.GetFront();

					m_LockOnTargetPosition = m_PivotPosition + (previousPivotToDesiredLockOnTargetDistance * cameraFrontOnPreviousUpdate);
				}

				m_LockOnTargetBlendStartTime				= fwTimer::GetCamTimeInMilliseconds();
				m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
				m_ShouldBlendLockOnTargetPosition			= true;

				//Determine a suitable blend duration base upon the angle between the current and desired lock-on target positions.

				Vector3 desiredLockOnTargetFront = desiredLockOnTargetPosition - m_BasePivotPosition;
				desiredLockOnTargetFront.Normalize();
				Vector3 currentLockOnTargetFront = m_LockOnTargetPosition - m_BasePivotPosition;
				currentLockOnTargetFront.NormalizeSafe(desiredLockOnTargetFront);

				const u32 minBlendDuration	= (m_LockOnTarget.Get() != NULL) ? m_Metadata.m_MinBlendDurationForLockOnSwitch :
													m_Metadata.m_MinBlendDurationForInitialLockOn;
				const u32 maxBlendDuration	= (m_LockOnTarget.Get() != NULL) ? m_Metadata.m_MaxBlendDurationForLockOnSwitch :
													m_Metadata.m_MaxBlendDurationForInitialLockOn;

				const float angleDelta		= currentLockOnTargetFront.FastAngle(desiredLockOnTargetFront);
				const float durationTValue	= (m_Metadata.m_MaxAngleDeltaForLockOnSwitchBlendScaling < SMALL_FLOAT) ? 1.0f :
												Min(angleDelta / (m_Metadata.m_MaxAngleDeltaForLockOnSwitchBlendScaling * DtoR), 1.0f);
				m_LockOnTargetBlendDuration	= Lerp(durationTValue, minBlendDuration, maxBlendDuration);
			}

			m_LockOnTarget = desiredLockOnTarget;
		}

		UpdateLockOnTargetDamping(hasChangedLockOnTargetThisUpdate, desiredLockOnTargetPosition);

		if(m_ShouldBlendLockOnTargetPosition)
		{
			UpdateLockOnTargetBlend(desiredLockOnTargetPosition);
		}
		else
		{
			m_LockOnTargetPosition = desiredLockOnTargetPosition;
		}

		//Cache the effective look-around speed for the current fine-aim input so that we may use this to smooth the transition out of lock-on.
		const Vector3 basePivotToCurrentFineAimTarget		= m_LockOnTargetPosition - m_BasePivotPosition;
		const Vector3 basePivotToPreviousFineAimTarget		= basePivotToCurrentFineAimTarget - m_LockOnFineAimOffset +
																fineAimOffsetOnPreviousUpdate;
		const float angleDelta								= basePivotToPreviousFineAimTarget.Angle(basePivotToCurrentFineAimTarget);
		const float camInvTimeStep							= fwTimer::GetCamInvTimeStep();
		m_CachedLockOnFineAimLookAroundSpeed				= angleDelta * camInvTimeStep;
	}
	else
	{
		m_LockOnTarget								= NULL;
		m_LockOnTargetPosition						= g_InvalidPosition;
		m_LockOnFineAimOffset.Zero();
		m_LockOnTargetBlendStartTime				= 0;
		m_LockOnTargetBlendDuration					= 0;
		m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
		m_ShouldBlendLockOnTargetPosition			= false;
	}

	if (m_StickyAimHelper)
	{
		m_StickyAimHelper->Update(m_BasePivotPosition, m_BaseFront, m_Frame.GetWorldMatrix(), m_IsLockedOn);
	}
}

bool camThirdPersonAimCamera::ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const
{
	bool isLockedOn = false;

	fineAimOffset.Zero();

	const bool shouldUseLockOnAiming = ComputeShouldUseLockOnAiming();
	if(shouldUseLockOnAiming)
	{
		const CPed* followPed = camInterface::FindFollowPed();
		if(followPed)
		{
			const CPlayerInfo* followPedPlayerInfo = followPed->GetPlayerInfo();
			if(followPedPlayerInfo)
			{
				CPlayerPedTargeting &targeting			= const_cast<CPlayerPedTargeting &>(followPedPlayerInfo->GetTargeting());
				const CEntity* prospectiveLockOnTarget	= targeting.GetLockOnTarget();

				const CWeaponInfo *weaponInfo = followPed->GetWeaponManager() ? followPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
				bool bUsingOnFootHomingWeapon = weaponInfo && weaponInfo->GetIsOnFootHoming();

				if((prospectiveLockOnTarget != NULL) && !bUsingOnFootHomingWeapon)
				{
					Vector3 prospectiveLockOnTargetPosition;
					if(m_Metadata.m_ShouldLockOnToTargetEntityPosition)
					{
						prospectiveLockOnTargetPosition	= VEC3V_TO_VECTOR3(prospectiveLockOnTarget->GetTransform().GetPosition());
					}
					else
					{
						prospectiveLockOnTarget->GetLockOnTargetAimAtPos(prospectiveLockOnTargetPosition);
					}

					const bool isPositionValid = ComputeIsProspectiveLockOnTargetPositionValid(prospectiveLockOnTargetPosition);
					if(isPositionValid)
					{
						Vector3 tempFineAimOffset;
						if(targeting.GetFineAimingOffset(tempFineAimOffset))
						{
							//Scale down the fine-aim offset at close range to prevent instability.
							const float lockOnDistance		= m_BasePivotPosition.Dist(prospectiveLockOnTargetPosition);
							const float fineAimScaling		= RampValueSafe(lockOnDistance, m_Metadata.m_MinDistanceForFineAimScaling,
																m_Metadata.m_MaxDistanceForFineAimScaling, 0.0f, 1.0f);
							fineAimOffset					= tempFineAimOffset * fineAimScaling;

							// Script-requested ability to invert look controls as a MP pickup power
							if (followPed->GetPedResetFlag(CPED_RESET_FLAG_InvertLookAroundControls))
							{
								fineAimOffset *= -1.0f;
							}

							prospectiveLockOnTargetPosition	+= fineAimOffset;
						}

						lockOnTarget			= prospectiveLockOnTarget;
						lockOnTargetPosition	= prospectiveLockOnTargetPosition;
						isLockedOn				= true;
					}
					else
					{
						//We cannot safely handle this setup, so break lock-on.
						targeting.ClearLockOnTarget();
					}
				}
			}
		}
	}

	return isLockedOn;
}

bool camThirdPersonAimCamera::ComputeIsProspectiveLockOnTargetPositionValid(const Vector3& targetPosition) const
{
	if(!m_Metadata.m_ShouldValidateLockOnTargetPosition)
	{
		return true;
	}

	bool isValid = false;

	//Ensure that the lock-on distance and pitch are with safe limits.

	const Vector3 lockOnDelta	= targetPosition - m_BasePivotPosition;
	const float lockOnDistance2	= lockOnDelta.Mag2();
	if(lockOnDistance2 >= (m_Metadata.m_MinDistanceForLockOn * m_Metadata.m_MinDistanceForLockOn))
	{
		Vector3 lockOnFront(lockOnDelta);
		lockOnFront.Normalize();

		const float lockOnPitch	= camFrame::ComputePitchFromFront(lockOnFront);
		isValid					= ((lockOnPitch >= m_OrbitPitchLimits.x) && (lockOnPitch <= m_OrbitPitchLimits.y));
	}

	return isValid;
}

bool camThirdPersonAimCamera::ComputeShouldUseLockOnAiming() const
{
	return (m_Metadata.m_ShouldUseLockOnAiming && !rage::ioHeadTracking::IsMotionTrackingEnabled());
}

void camThirdPersonAimCamera::UpdateLockOnEnvelopes(const CEntity* UNUSED_PARAM(desiredLockOnTarget))
{
	if(m_LockOnEnvelope)
	{
		m_LockOnEnvelope->AutoStartStop(m_IsLockedOn);

		m_LockOnEnvelopeLevel = m_LockOnEnvelope->Update();
		m_LockOnEnvelopeLevel = SlowInOut(m_LockOnEnvelopeLevel);
	}
	else
	{
		m_LockOnEnvelopeLevel = m_IsLockedOn ? 1.0f : 0.0f;
	}

	const float desiredBlendLevel	= m_IsLockedOn ? 1.0f : 0.0f;
	const bool shouldCut			= (m_IsLockedOn || (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		=  shouldCut ? 0.0f : m_Metadata.m_FineAimBlendSpringConstant;

	m_FineAimBlendSpring.Update(desiredBlendLevel, springConstant, m_Metadata.m_FineAimBlendSpringDampingRatio, true);
}

void camThirdPersonAimCamera::UpdateLockOnTargetBlend(const Vector3& desiredLockOnTargetPosition)
{
	const u32 currentTime			= fwTimer::GetCamTimeInMilliseconds();
	float lockOnTargetBlendLevel	= static_cast<float>(currentTime - m_LockOnTargetBlendStartTime) /
										static_cast<float>(m_LockOnTargetBlendDuration);
	lockOnTargetBlendLevel			= SlowInOut(lockOnTargetBlendLevel);
	lockOnTargetBlendLevel			= SlowOut(lockOnTargetBlendLevel);

	PF_SET(lockOnTargetBlendLevel, lockOnTargetBlendLevel);

	if(lockOnTargetBlendLevel >= (1.0f - SMALL_FLOAT))
	{
		//Clamp to the desired lock-on target position and clean up the blend.
		m_LockOnTargetPosition						= desiredLockOnTargetPosition;
		m_LockOnTargetBlendStartTime				= 0;
		m_LockOnTargetBlendDuration					= 0;
		m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
		m_ShouldBlendLockOnTargetPosition			= false;

		return;
	}

	if(lockOnTargetBlendLevel > SMALL_FLOAT)
	{
		const float errorRatioToApplyThisUpdate	= (lockOnTargetBlendLevel < m_LockOnTargetBlendLevelOnPreviousUpdate) ? 0.0f :
			((lockOnTargetBlendLevel - m_LockOnTargetBlendLevelOnPreviousUpdate) /
			(1.0f - m_LockOnTargetBlendLevelOnPreviousUpdate));

		Vector3 desiredLockOnTargetDelta = desiredLockOnTargetPosition - m_BasePivotPosition;
		Vector3 desiredLockOnTargetFront(desiredLockOnTargetDelta);
		desiredLockOnTargetFront.Normalize();

		Vector3 currentLockOnTargetDelta = m_LockOnTargetPosition - m_BasePivotPosition;
		Vector3 currentLockOnTargetFront(currentLockOnTargetDelta);
		currentLockOnTargetFront.NormalizeSafe(desiredLockOnTargetFront);

		Vector3 axis;
		float angle;
		camFrame::ComputeAxisAngleRotation(currentLockOnTargetFront, desiredLockOnTargetFront, axis, angle);

		angle *= errorRatioToApplyThisUpdate;

		Quaternion orientationDeltaToApply;
		orientationDeltaToApply.FromRotation(axis, angle);

		Vector3 lockOnTargetFrontToApply;
		orientationDeltaToApply.Transform(currentLockOnTargetFront, lockOnTargetFrontToApply);

		//Lerp the distance to the lock-on target commensurate with the angular (s)lerp.
		const float currentLockOnTargetDistance	= currentLockOnTargetDelta.Mag();
		const float desiredLockOnTargetDistance	= desiredLockOnTargetDelta.Mag();

		const float lockOnTargetDistanceToApply	= Lerp(errorRatioToApplyThisUpdate, currentLockOnTargetDistance, desiredLockOnTargetDistance);

		m_LockOnTargetPosition = m_BasePivotPosition + (lockOnTargetDistanceToApply * lockOnTargetFrontToApply);

		m_LockOnTargetBlendLevelOnPreviousUpdate = lockOnTargetBlendLevel;
	}
	else
	{
		m_LockOnTargetBlendLevelOnPreviousUpdate = 0.0f;
	}
}

float camThirdPersonAimCamera::ComputeBaseFov() const
{
	//Use the base FOV associated with the cached weapon info, if valid.
	const float baseFov = (m_Metadata.m_ShouldApplyWeaponFov && m_WeaponInfo) ? m_WeaponInfo->GetCameraFov() : camThirdPersonCamera::ComputeBaseFov();

	return baseFov;
}

void camThirdPersonAimCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	if(m_IsLockedOn)
	{
		const Vector3 lockOnDelta	= m_LockOnTargetPosition - m_BasePivotPosition;
		const float lockOnDistance	= lockOnDelta.Mag();

		//Only update the lock-on orientation if there is sufficient distance between the base pivot position and the target positions,
		//otherwise the lock-on behavior can become unstable.
		if(lockOnDistance >= m_Metadata.m_MinDistanceForLockOn)
		{
			m_BaseFront = lockOnDelta;
			m_BaseFront.Normalize();

			//Rotate the base front such that it points directly at the lock-on target position from the desired pivot position.

			Matrix34 orbitMatrix(Matrix34::IdentityType);
			camFrame::ComputeWorldMatrixFromFront(m_BaseFront, orbitMatrix);

			Vector3 orbitRelativePivotOffset;
			ComputeOrbitRelativePivotOffset(m_BaseFront, orbitRelativePivotOffset);

			const float sinHeadingDelta		= orbitRelativePivotOffset.x / lockOnDistance;
			const float headingDelta		= AsinfSafe(sinHeadingDelta);

			const float sinPitchDelta		= -orbitRelativePivotOffset.z / lockOnDistance;
			float pitchDelta				= AsinfSafe(sinPitchDelta);

			const float orbitPitchOffset	= ComputeOrbitPitchOffset();
			pitchDelta						-= orbitPitchOffset;

			orbitMatrix.RotateLocalZ(headingDelta);
			orbitMatrix.RotateLocalX(pitchDelta);

			m_BaseFront = orbitMatrix.b;
		}

		//Cache the lock-on target position relative to our new orbit, so that it can track the movement of the attach parent whilst blending out.

		Matrix34 orbitMatrix;
		camFrame::ComputeWorldMatrixFromFront(m_BaseFront, orbitMatrix);
		orbitMatrix.d = m_BasePivotPosition;

		orbitMatrix.UnTransform(m_LockOnTargetPosition, m_CachedRelativeLockOnTargetPosition);
	}

	camThirdPersonCamera::ComputeOrbitOrientation(orbitHeading, orbitPitch);

	if (m_StickyAimHelper)
	{
		m_StickyAimHelper->UpdateOrientation(orbitHeading, orbitPitch, m_BasePivotPosition, m_Frame.GetWorldMatrix());
	}
}

void camThirdPersonAimCamera::ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const
{
	if(m_IsLockedOn)
	{
		//NOTE: When locked-on, look-around is handled via manipulation of the lock-on target position.
		headingOffset	= 0.0f;
		pitchOffset		= 0.0f;
	}
	else
	{
		camThirdPersonCamera::ComputeLookAroundOrientationOffsets(headingOffset, pitchOffset);

		//Scale the offsets based up the current FOV, as this allows the look-around input to be responsive normally without being too fast
		//when zoomed in, such as in accurate mode.

		const float fovOrientationScaling = m_Frame.GetFov() / m_Metadata.m_BaseFov;

		headingOffset	*= fovOrientationScaling;
		pitchOffset		*= fovOrientationScaling;

		const float fineAimBlendLevel = m_FineAimBlendSpring.GetResult();
		if(fineAimBlendLevel >= SMALL_FLOAT)
		{
			//Blend from the effective look-around speed used for fine aiming in order to smooth the transition out of lock-on.

			const float lookAroundMag	= Sqrtf((headingOffset * headingOffset) + (pitchOffset * pitchOffset));
			const float camInvTimeStep	= fwTimer::GetCamInvTimeStep();
			const float lookAroundSpeed	= lookAroundMag * camInvTimeStep;
			if(lookAroundSpeed >= SMALL_FLOAT)
			{
				const float lookAroundScalingAtMaxBlendLevel	= m_CachedLockOnFineAimLookAroundSpeed / lookAroundSpeed;
				const float lookAroundScalingToApply			= Lerp(fineAimBlendLevel, 1.0f, lookAroundScalingAtMaxBlendLevel);

				headingOffset	*= lookAroundScalingToApply;
				pitchOffset		*= lookAroundScalingToApply;
			}
		}
	}
}

void camThirdPersonAimCamera::ApplyOverriddenOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	if(m_IsLockedOn)
	{
		//Clear any existing orientation override requests, as we must respect the lock-on orientation and we do not wish to apply this request
		//when lock-on clears.
		m_ShouldApplyRelativeHeading	= false;
		m_ShouldApplyRelativePitch		= false;
	}
	else
	{
		camThirdPersonCamera::ApplyOverriddenOrbitOrientation(orbitHeading, orbitPitch);
	}
}

void camThirdPersonAimCamera::ComputeBaseLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition,
	const Vector3& baseFront, const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition,
	const Vector3& postCollisionCameraPosition, float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const
{
	camThirdPersonCamera::ComputeBaseLookAtFront(collisionRootPosition, basePivotPosition, baseFront, pivotPosition, preCollisionCameraPosition,
		postCollisionCameraPosition, preToPostCollisionBlendValue, wasCameraConstrainedOffAxis, lookAtFront);

	if(m_LockOnEnvelopeLevel < SMALL_FLOAT)
	{
		return;
	}

	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromFront(baseFront, orbitMatrix);
	orbitMatrix.d = basePivotPosition;

	Vector3 lockOnTargetPosition;
	orbitMatrix.Transform(m_CachedRelativeLockOnTargetPosition, lockOnTargetPosition);

	Vector3 lookAtFrontForLockOn = lockOnTargetPosition - postCollisionCameraPosition;
	lookAtFrontForLockOn.NormalizeSafe(lookAtFront);

	//Limit the lock-on pitch.

	float lockOnHeading;
	float lockOnPitch;
	camFrame::ComputeHeadingAndPitchFromFront(lookAtFrontForLockOn, lockOnHeading, lockOnPitch);

	LimitOrbitPitch(lockOnPitch);

	camFrame::ComputeFrontFromHeadingAndPitch(lockOnHeading, lockOnPitch, lookAtFrontForLockOn);

	//Blend between free-aim and lock-on look-at fronts.
	Vector3 interpolatedLookAtFront;
	camFrame::SlerpOrientation(m_LockOnEnvelopeLevel, lookAtFront, lookAtFrontForLockOn, interpolatedLookAtFront);

	lookAtFront = interpolatedLookAtFront;
}

float camThirdPersonAimCamera::ComputeDesiredFov()
{
	float desiredFov = camThirdPersonCamera::ComputeDesiredFov();

	//Use the zoom settings associated with the cached weapon info, if valid.
	float desiredZoomFactorForWeapon = 1.0f;
	if(m_Metadata.m_ShouldApplyWeaponFov && m_WeaponInfo)
	{
		const float baseFov				= ComputeBaseFov();
		const float desiredFovForWeapon = m_WeaponInfo->GetCameraFov();
		desiredZoomFactorForWeapon		= (desiredFovForWeapon >= SMALL_FLOAT) ? (baseFov / desiredFovForWeapon) : 1.0f;

		const bool isInAccurateMode = IsInAccurateMode();
		if(isInAccurateMode)
		{
			const float zoomFactorForAccurateMode	= ComputeDesiredZoomFactorForAccurateMode(*m_WeaponInfo);
			desiredZoomFactorForWeapon				*= zoomFactorForAccurateMode;
		}
	}

	const bool shouldCut			= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		= shouldCut ? 0.0f : m_Metadata.m_WeaponZoomFactorSpringConstant;
	const float zoomFactorToApply	= m_WeaponZoomFactorSpring.Update(desiredZoomFactorForWeapon, springConstant,
										m_Metadata.m_WeaponZoomFactorSpringDampingRatio, true);

	if(zoomFactorToApply >= SMALL_FLOAT)
	{
		desiredFov /= zoomFactorToApply;
	}

	return desiredFov;
}

float camThirdPersonAimCamera::ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const
{
	const float zoomFactor = weaponInfo.GetZoomFactorForAccurateMode();

	return zoomFactor;
}

void camThirdPersonAimCamera::UpdateReticuleSettings()
{
	m_ShouldDisplayReticule = m_Metadata.m_ShouldDisplayReticule;
	if(m_ShouldDisplayReticule)
	{
		if(m_FrameInterpolator)
		{
			m_ShouldDisplayReticule	= ComputeShouldDisplayReticuleDuringInterpolation(*m_FrameInterpolator);
		}

		const CPed* followPed = camInterface::FindFollowPed();
		if(followPed)
		{
			// Hide the reticule for sniper rifles as soon as the firing is finished.
			const CWeaponInfo* weaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*followPed);
			if(followPed->GetWeaponManager() && followPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope() && GetNameHash() == weaponInfo->GetRunAndGunCameraHash() && !followPed->GetPlayerInfo()->IsRunAndGunning())
			{
				m_ShouldDisplayReticule = false;
			}
		}
	}
}

bool camThirdPersonAimCamera::ComputeShouldDisplayReticuleDuringInterpolation(const camFrameInterpolator& frameInterpolator) const
{
	if(m_Metadata.m_ShouldDisplayReticuleDuringInterpolation)
	{
		return true;
	}

	//NOTE: Display of the reticule is typically blocked during interpolation into this camera, unless specifically flagged to defer to the source
	//camera.
	const camBaseCamera* sourceCamera = frameInterpolator.GetSourceCamera();
	if(m_Metadata.m_ShouldAllowInterpolationSourceCameraToPersistReticule && sourceCamera &&
		sourceCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()) &&
		static_cast<const camThirdPersonAimCamera*>(sourceCamera)->ComputeShouldDisplayReticuleAsInterpolationSource())
	{
		return true;
	}

	return false;
}

void camThirdPersonAimCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	camThirdPersonCamera::ComputeDofOverriddenFocusSettings(focusPosition, blendLevel);

	if(!m_Metadata.m_ShouldFocusOnLockOnTarget)
	{
		return;
	}

	blendLevel = m_LockOnEnvelopeLevel;
	if(blendLevel < SMALL_FLOAT)
	{
		return;
	}

	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromFront(m_BaseFront, orbitMatrix);
	orbitMatrix.d = m_BasePivotPosition;

	Vector3 lockOnTargetPositionToConsider;
	orbitMatrix.Transform(m_CachedRelativeLockOnTargetPosition, lockOnTargetPositionToConsider);

	focusPosition.Lerp(m_Metadata.m_FocusParentToTargetBlendLevel, m_BasePivotPosition, lockOnTargetPositionToConsider);
}

void camThirdPersonAimCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camThirdPersonCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	if(m_Metadata.m_BaseFovToEmulateWithFocalLengthMultiplier >= g_MinFov)
	{
		//Compensate for the variation in base FOV between different weapons/cameras by using the focal length multiplier to emulate a specific FOV.

		const float baseFov					= ComputeBaseFov();
		const float fovRatio				= baseFov / m_Metadata.m_BaseFovToEmulateWithFocalLengthMultiplier;
		settings.m_FocalLengthMultiplier	*= fovRatio;
	}

	if((overriddenFocusDistanceBlendLevel < SMALL_FLOAT) || (m_Metadata.m_SecondaryFocusParentToTargetBlendLevel <= -SMALL_FLOAT))
	{
		return;
	}

	//Modify the lens aperture to ensure that both the attach parent and the lock-on target remain in acceptable focus, whilst aiming for shallow DOF.

	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromFront(m_BaseFront, orbitMatrix);
	orbitMatrix.d = m_BasePivotPosition;

	Vector3 lockOnTargetPositionToConsider;
	orbitMatrix.Transform(m_CachedRelativeLockOnTargetPosition, lockOnTargetPositionToConsider);

	const float secondaryFocusToLockOnTargetDistance = m_BasePivotPosition.Dist(lockOnTargetPositionToConsider) * (1.0f - m_Metadata.m_SecondaryFocusParentToTargetBlendLevel);

	const Vector3 cameraPosition			= m_PostEffectFrame.GetPosition();
	const float distanceToLockOnTarget		= cameraPosition.Dist(lockOnTargetPositionToConsider);
	float distanceToSecondaryFocus			= distanceToLockOnTarget - secondaryFocusToLockOnTargetDistance;
	float focusToSecondaryFocusDistance		= distanceToSecondaryFocus - overriddenFocusDistance;

	float revisedHyperfocalDistance;
	if(focusToSecondaryFocusDistance >= 0)
	{
		if(focusToSecondaryFocusDistance < m_Metadata.m_MinFocusToSecondaryFocusDistance)
		{
			focusToSecondaryFocusDistance	= m_Metadata.m_MinFocusToSecondaryFocusDistance;
			distanceToSecondaryFocus		= overriddenFocusDistance + focusToSecondaryFocusDistance;
		}

		revisedHyperfocalDistance = distanceToSecondaryFocus * overriddenFocusDistance / focusToSecondaryFocusDistance;
	}
	else
	{
		if(focusToSecondaryFocusDistance > -m_Metadata.m_MinFocusToSecondaryFocusDistance)
		{
			focusToSecondaryFocusDistance	= -m_Metadata.m_MinFocusToSecondaryFocusDistance;
			distanceToSecondaryFocus		= overriddenFocusDistance + focusToSecondaryFocusDistance;
		}

		revisedHyperfocalDistance = distanceToSecondaryFocus * overriddenFocusDistance / -focusToSecondaryFocusDistance;
	}

	const float fov						= m_PostEffectFrame.GetFov();
	const float focalLengthOfLens		= settings.m_FocalLengthMultiplier * g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * fov * DtoR));
	const float desiredFNumberOfLens	= square(focalLengthOfLens) / (revisedHyperfocalDistance * g_CircleOfConfusionFor35mm);

	settings.m_FNumberOfLens			= camFrame::InterpolateFNumberOfLens(overriddenFocusDistanceBlendLevel, settings.m_FNumberOfLens, desiredFNumberOfLens);

// 	cameraDisplayf("desiredFNumberOfLens = %f, blendedFNumberOfLens = %f", desiredFNumberOfLens, settings.m_FNumberOfLens);
}

#if __BANK
//Render the camera so we can see it.
void camThirdPersonAimCamera::DebugRender()
{
	camThirdPersonCamera::DebugRender();

	if(camManager::GetSelectedCamera() == this)
	{
		//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.
		static float radius = 0.125f;
		grcDebugDraw::Sphere(m_LockOnTargetPosition, radius, Color_orange);
	}
}
#endif // __BANK
