//
// camera/gameplay/aim/ThirdPersonPedMeleeAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonPedMeleeAimCamera.h"

#include "fwmaths/angle.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonPedMeleeAimCamera,0x437C678)


camThirdPersonPedMeleeAimCamera::camThirdPersonPedMeleeAimCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonPedAimCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonPedMeleeAimCameraMetadata&>(metadata))
{
}

bool camThirdPersonPedMeleeAimCamera::Update()
{
	const bool hasSucceeded = camThirdPersonPedAimCamera::Update();

	return hasSucceeded;
}

void camThirdPersonPedMeleeAimCamera::UpdateLockOn()
{
	camThirdPersonPedAimCamera::UpdateLockOn();

	if(m_IsLockedOn)
	{
		//Compute and apply damping to the base pivot to lock-on target distance.

		Vector3 lockOnDelta			= m_LockOnTargetPosition - m_BasePivotPosition;
		const float desiredDistance	= lockOnDelta.Mag();

		if((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
			m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
		{
			m_LockOnDistanceSpring.Reset(desiredDistance);
		}
		else
		{
			m_LockOnDistanceSpring.Update(desiredDistance, m_Metadata.m_LockOnDistanceSpringConstant);
		}
	}
}

void camThirdPersonPedMeleeAimCamera::UpdateLockOnEnvelopes(const CEntity* UNUSED_PARAM(desiredLockOnTarget))
{
	//NOTE: The melee aim camera does not apply a lock-on envelope, but instead simply latches on once locked-on.
	if(m_IsLockedOn)
	{
		m_LockOnEnvelopeLevel = 1.0f;
	}
}

float camThirdPersonPedMeleeAimCamera::UpdateOrbitDistance()
{
	//Compute the orbit distance to apply based upon the current (damped) lock-on distance.

	UpdateOrbitDistanceLimits();

	const float lockOnDistance		= m_LockOnDistanceSpring.GetResult();
	const float minOrbitDistance	= m_MinOrbitDistanceSpring.GetResult();
	const float maxOrbitDistance	= m_MaxOrbitDistanceSpring.GetResult();

	float orbitDistanceRatio		= RampValueSafe(lockOnDistance, m_Metadata.m_LockOnDistanceLimitsForOrbitDistance.x,
										m_Metadata.m_LockOnDistanceLimitsForOrbitDistance.y, 0.0f, 1.0f);
	orbitDistanceRatio				= SlowInOut(orbitDistanceRatio);
	const float orbitDistance		= Lerp(orbitDistanceRatio, minOrbitDistance, maxOrbitDistance);

	return orbitDistance;
}

void camThirdPersonPedMeleeAimCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	camFrame::ComputeHeadingAndPitchFromFront(m_BaseFront, orbitHeading, orbitPitch);

	float desiredOrbitHeading	= orbitHeading;
	float desiredOrbitPitch		= orbitPitch;

	float dampedLockOnDistance	= m_LockOnDistanceSpring.GetResult();
	dampedLockOnDistance		= Max(dampedLockOnDistance, m_Metadata.m_MinDistanceForLockOn);

	if(m_IsLockedOn)
	{
		Vector3 lockOnDelta			= m_LockOnTargetPosition - m_BasePivotPosition;
		const float lockOnDistance	= lockOnDelta.Mag();

		//Only update the lock-on orientation if there is sufficient distance between the base pivot position and the target positions,
		//otherwise the lock-on behavior can become unstable.
		if(lockOnDistance >= m_Metadata.m_MinDistanceForLockOn)
		{
			Vector3 desiredOrbitFront(lockOnDelta);
			desiredOrbitFront.Normalize();

			//Rotate the base front to bias it towards pointing at the lock-on target position from the desired pivot position.

			Matrix34 desiredOrbitMatrix(Matrix34::IdentityType);
			camFrame::ComputeWorldMatrixFromFront(desiredOrbitFront, desiredOrbitMatrix);

			Vector3 orbitRelativePivotOffset;
			ComputeOrbitRelativePivotOffset(desiredOrbitFront, orbitRelativePivotOffset);

			const float sinHeadingDelta		= orbitRelativePivotOffset.x / dampedLockOnDistance;
			const float headingDelta		= AsinfSafe(sinHeadingDelta);

			const float sinPitchDelta		= -orbitRelativePivotOffset.z / dampedLockOnDistance;
			float pitchDelta				= AsinfSafe(sinPitchDelta);

			const float orbitPitchOffset	= ComputeOrbitPitchOffset();
			pitchDelta						-= orbitPitchOffset;

			const float headingDeltaToApply	= headingDelta * m_Metadata.m_BaseOrbitPivotRotationBlendLevel;
			const float pitchDeltaToApply	= pitchDelta * m_Metadata.m_BaseOrbitPivotRotationBlendLevel;

			desiredOrbitMatrix.RotateLocalZ(headingDeltaToApply);
			desiredOrbitMatrix.RotateLocalX(pitchDeltaToApply);

			camFrame::ComputeHeadingAndPitchFromFront(desiredOrbitMatrix.b, desiredOrbitHeading, desiredOrbitPitch);

			desiredOrbitPitch *= m_Metadata.m_BaseOrbitPitchScaling;
		}
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

	if((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		m_LockOnHeadingSpring.Reset(desiredOrbitHeading);
		m_LockOnPitchSpring.Reset(desiredOrbitPitch);
	}
	else
	{
		m_LockOnHeadingSpring.OverrideResult(orbitHeading);
		m_LockOnHeadingSpring.Update(desiredOrbitHeading, m_Metadata.m_OrbitHeadingSpringConstant, m_Metadata.m_OrbitHeadingSpringDampingRatio);
		m_LockOnPitchSpring.OverrideResult(orbitPitch);
		m_LockOnPitchSpring.Update(desiredOrbitPitch, m_Metadata.m_OrbitPitchSpringConstant, m_Metadata.m_OrbitPitchSpringDampingRatio);
	}

	orbitHeading	= m_LockOnHeadingSpring.GetResult();
	orbitPitch		= m_LockOnPitchSpring.GetResult();

	camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, m_BaseFront);

	if(m_IsLockedOn)
	{
		//Cache the lock-on target position relative to our new orbit, so that it can track the movement of the attach parent whilst blending out.

		//Compute a custom lock-on target position that will cause this camera to look at damped position that is between the base pivot and actual
		//lock-on target positions.
		//NOTE: This is not actually correct, as we should be interpolating along the lock-on delta, rather than the orbit front, which points elsewhere.
		//However, the lock-on delta/direction is not presently damped, so we'll live with the inconsistency for now and retune the scaling factor.
		const Vector3 dampedTargetPositionForLookAt	= m_BasePivotPosition + (m_BaseFront * dampedLockOnDistance * m_Metadata.m_LockOnDistanceScalingForLookAt);

		Matrix34 orbitMatrix;
		camFrame::ComputeWorldMatrixFromFront(m_BaseFront, orbitMatrix);
		orbitMatrix.d = m_BasePivotPosition;

		orbitMatrix.UnTransform(dampedTargetPositionForLookAt, m_CachedRelativeLockOnTargetPosition);
	}
}
