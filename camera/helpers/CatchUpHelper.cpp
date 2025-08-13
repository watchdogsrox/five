//
// camera/helpers/CatchUpHelper.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/CatchUpHelper.h"

#include "fwsys/timer.h"

#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCatchUpHelper,0x504FE2F5)

//B*1816708:This asserts regularly when catching up to the near third-person view, this minimum means we don't
//			have to change metadata. A more robust fix has been implemented in NG.
extern const float g_MinimumCatchUpDistance = 5.0f;

camCatchUpHelper::camCatchUpHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camCatchUpHelperMetadata&>(metadata))
, m_OrbitRelativePivotOffset(VEC3_ZERO)
, m_OrbitRelativeCameraPositionDelta(VEC3_ZERO)
, m_RelativeLookAtPositionDelta(VEC3_ZERO)
, m_MaxDistanceToBlend(0.0f)
, m_DistanceTravelled(0.0f)
, m_BlendLevel(1.0f)
, m_BlendType(SLOW_IN_OUT)
, m_ShouldInitBehaviour(true)
{
}

void camCatchUpHelper::Init(const camFrame& sourceFrame, float maxDistanceToBlend, int blendType)
{
	m_SourceFrame.CloneFrom(sourceFrame);
	m_MaxDistanceToBlend = maxDistanceToBlend;

	if (blendType < 0 || blendType >= (int)NUM_BLEND_TYPES)
	{
		// Set default behaviour for invalid value.
		blendType = (int)SLOW_IN_OUT;
	}
	m_BlendType = (eBlendType)blendType;
}

bool camCatchUpHelper::Update(const camThirdPersonCamera& thirdPersonCamera, float maxOrbitDistance, bool hasCut, const Vector3& baseAttachVelocity)
{
	const u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds();

	if(m_ShouldInitBehaviour)
	{
		const bool isSourceFrameValid = ComputeIsCatchUpSourceFrameValid(thirdPersonCamera);
		if(!cameraVerifyf(isSourceFrameValid,
			"A camera catch-up helper (name: %s, hash: %u) aborted as the catch-up frame points away from the base pivot position", GetName(), GetNameHash()))
		{
			//Abort catch-up.
			return false;
		}

		//Sanity-check the distance of the source frame from the attach parent is not too great.
		const Vector3& sourcePosition		= m_SourceFrame.GetPosition();
		const Vector3& basePivotPosition	= thirdPersonCamera.GetBasePivotPosition();
		const float sourceToBasePivotDist2	= sourcePosition.Dist2(basePivotPosition);
		const float maxCatchUpDistance		= (maxOrbitDistance * Max(m_Metadata.m_MaxOrbitDistanceScalingForMaxValidCatchUpDistance, g_MinimumCatchUpDistance));
		if(!cameraVerifyf(sourceToBasePivotDist2 <= (maxCatchUpDistance * maxCatchUpDistance),
			"A camera catch-up helper (name: %s, hash: %u) aborted as the catch-up position was too far away (%fm, limit=%fm) from the base pivot position",
			GetName(), GetNameHash(), Sqrtf(sourceToBasePivotDist2), maxCatchUpDistance))
		{
			//Abort catch-up.
			return false;
		}

		//NOTE: We do not use the metadata settings if a valid bespoke distance was specified on initialisation.
		if(m_MaxDistanceToBlend < SMALL_FLOAT)
		{
			m_MaxDistanceToBlend = m_Metadata.m_MaxOrbitDistanceScalingToBlend * maxOrbitDistance;
		}

		m_StartTime = timeInMilliseconds;

		//Allow the initial update. The blend level is already initialised.
		return true;
	}

	if(hasCut)
	{
		//Abort catch-up if the camera cuts (after initialisation.)
		return false;
	}

	//NOTE: The blend level will default to 1.0 for zero blend duration.
	if(m_Metadata.m_BlendDuration > 0)
	{
		const u32 timeSinceStart	= timeInMilliseconds - m_StartTime;
		m_BlendLevel				= 1.0f - (static_cast<float>(timeSinceStart) / static_cast<float>(m_Metadata.m_BlendDuration));
	}
	else
	{
		const float baseAttachSpeed				= baseAttachVelocity.Mag();
		const float timeStep					= fwTimer::GetTimeStep();
		const float distanceTravelledThisUpdate	= baseAttachSpeed * timeStep;
		m_DistanceTravelled						+= distanceTravelledThisUpdate;
		m_BlendLevel							= RampValueSafe(m_DistanceTravelled, 0.0f, m_MaxDistanceToBlend, 1.0f, 0.0f);
	}

	if(m_Metadata.m_ShouldApplyCustomBlendCurveType)
	{
		m_BlendLevel						= ApplyCustomBlendCurve(m_BlendLevel, m_Metadata.m_CustomBlendCurveType);
	}
	else if (m_BlendType == SLOW_IN_OUT)
	{
		m_BlendLevel						= SlowInOut(m_BlendLevel);
	}
	else if (m_BlendType == SLOW_IN)
	{
		m_BlendLevel						= SlowIn(m_BlendLevel);
	}
	else if (m_BlendType == SLOW_OUT)
	{
		m_BlendLevel						= SlowOut(m_BlendLevel);
	}

	m_BlendLevel = Clamp(m_BlendLevel, 0.0f, 1.0f);

	return (m_BlendLevel >= SMALL_FLOAT);
}

float camCatchUpHelper::ApplyCustomBlendCurve(float sourceLevel, s32 curveType) const
{
	float blendLevel;

	switch(curveType)
	{
	case CURVE_TYPE_EXPONENTIAL:
		{
			//Make the release an exponential (rather than linear) decay, as this looks better.
			//NOTE: exp(-7x) gives a result of less than 0.001 at x=1.0.
			blendLevel = exp(-7.0f * (1.0f - sourceLevel));
		}
		break;

	case CURVE_TYPE_SLOW_IN:
		{
			blendLevel = SlowIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_SLOW_OUT:
		{
			blendLevel = SlowOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_SLOW_IN_OUT:
		{
			blendLevel = SlowInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN:
		{
			blendLevel = SlowIn(SlowIn(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT:
		{
			blendLevel = SlowIn(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowInOut(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_EASE_IN:
		{
			blendLevel = camBaseSplineCamera::EaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_EASE_OUT:
		{
			blendLevel = camBaseSplineCamera::EaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_OUT:
		{
			blendLevel = QuadraticEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_IN_OUT:
		{		
			blendLevel = QuadraticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN:
		{
			blendLevel = CubicEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_OUT:
		{
			blendLevel = CubicEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN_OUT:
		{		
			blendLevel = CubicEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN:
		{
			blendLevel = QuarticEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_OUT:
		{
			blendLevel = QuarticEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN_OUT:
		{
			blendLevel = QuarticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN:
		{
			blendLevel = QuinticEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_OUT:
		{
			blendLevel = QuinticEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN_OUT:
		{
			blendLevel = QuinticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN:
		{
			blendLevel = CircularEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_OUT:
		{
			blendLevel = CircularEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN_OUT:
		{
			blendLevel =  CircularEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_LINEAR:
	default:
		{
			blendLevel = sourceLevel;
		}
	}

	return blendLevel;
}

void camCatchUpHelper::ComputeDesiredOrbitDistance(const camThirdPersonCamera& thirdPersonCamera, float& desiredOrbitDistance) const
{
	if(!m_ShouldInitBehaviour)
	{
		return;
	}

	const float distanceToBasePivotPlane = ComputeDistanceAlongCatchUpSourceFrontToBasePivotPlane(thirdPersonCamera);
	if(distanceToBasePivotPlane >= SMALL_FLOAT)
	{
		desiredOrbitDistance = distanceToBasePivotPlane;
	}
}

bool camCatchUpHelper::ComputeOrbitOrientation(const camThirdPersonCamera& thirdPersonCamera, float& orbitHeading, float& orbitPitch) const
{
	if(!m_ShouldInitBehaviour)
	{
		return false;
	}

	const bool isSourceFrameValid = ComputeIsCatchUpSourceFrameValid(thirdPersonCamera);
	if(!isSourceFrameValid)
	{
		return false;
	}

	ComputeDesiredOrbitOrientation(thirdPersonCamera, orbitHeading, orbitPitch);

	return true;
}

void camCatchUpHelper::ComputeOrbitRelativePivotOffset(const camThirdPersonCamera& thirdPersonCamera, Vector3& orbitRelativeOffset)
{
	if(m_ShouldInitBehaviour)
	{
		const float distanceToBasePivotPlane = ComputeDistanceAlongCatchUpSourceFrontToBasePivotPlane(thirdPersonCamera);
		if(distanceToBasePivotPlane < SMALL_FLOAT)
		{
			return;
		}

		//Project the catch-up source position along the catch-up front onto the base pivot plane. This will describe the pivot position.
		const Vector3& catchUpPosition	= m_SourceFrame.GetPosition();
		const Vector3& catchUpFront		= m_SourceFrame.GetFront();
		const Vector3 pivotPosition		= catchUpPosition + (distanceToBasePivotPlane * catchUpFront);

		//Now derive the desired orbit relative offset that would generate this pivot position.
		//TODO: Fix catch-up for parent relative orbiting.

		float orbitHeading;
		float orbitPitch;
		ComputeDesiredOrbitOrientation(thirdPersonCamera, orbitHeading, orbitPitch);

		Matrix34 orbitMatrix;
		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(orbitHeading, orbitPitch, 0.0f, orbitMatrix);

		orbitMatrix.d = thirdPersonCamera.GetBasePivotPosition();

		orbitMatrix.UnTransform(pivotPosition, m_OrbitRelativePivotOffset);
	}

	orbitRelativeOffset.Lerp(m_BlendLevel, m_OrbitRelativePivotOffset);
}

void camCatchUpHelper::UpdateCameraPositionDelta(const Vector3& orbitFront, Vector3& cameraPosition)
{
	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromFront(orbitFront, orbitMatrix);

	Vector3 cameraPositionDelta;
	if(m_ShouldInitBehaviour)
	{
		//Cache the camera position delta (relative to the orbit orientation) that remains after we got as close to the catch-up frame as
		//possible using the normal camera behaviour.
		const Vector3& catchUpPosition	= m_SourceFrame.GetPosition();
		cameraPositionDelta				= catchUpPosition - cameraPosition;

		orbitMatrix.UnTransform3x3(cameraPositionDelta, m_OrbitRelativeCameraPositionDelta);
	}
	else
	{
		orbitMatrix.Transform3x3(m_OrbitRelativeCameraPositionDelta, cameraPositionDelta);
	}

	cameraPosition.AddScaled(cameraPositionDelta, m_BlendLevel);
}

bool camCatchUpHelper::UpdateLookAtOrientationDelta(const Vector3& basePivotPosition, const Vector3& orbitFront, const Vector3& cameraPosition,
	Vector3& lookAtFront)
{
	//NOTE: While it seems like catch-up of look-at should be achieved by blending an orientation offset, in practice this doesn't work well,
	//as the change in framing due to catch-up appears to increase, rather than decrease, at greater orbit distance. So we instead catch-up
	//based upon a offset to the look-at position.

	Vector3 desiredLookAtPosition;
	camThirdPersonCamera::ComputeLookAtPositionFromFront(lookAtFront, basePivotPosition, orbitFront, cameraPosition, desiredLookAtPosition);

	Matrix34 lookAtMatrix;
	camFrame::ComputeWorldMatrixFromFront(lookAtFront, lookAtMatrix);

	Quaternion lookAtOrientation;
	lookAtMatrix.ToQuaternion(lookAtOrientation);

	Vector3 catchUpFront;
	bool isCatchUpFrontValid = true;

	if(m_ShouldInitBehaviour)
	{
		catchUpFront = m_SourceFrame.GetFront();

		Vector3 catchUpLookAtPosition;
		camThirdPersonCamera::ComputeLookAtPositionFromFront(catchUpFront, basePivotPosition, orbitFront, cameraPosition, catchUpLookAtPosition);

		const Vector3 lookAtPositionDelta = catchUpLookAtPosition - desiredLookAtPosition;

		lookAtOrientation.UnTransform(lookAtPositionDelta, m_RelativeLookAtPositionDelta);
	}
	else
	{
		Vector3 lookAtPositionDelta;
		lookAtOrientation.Transform(m_RelativeLookAtPositionDelta, lookAtPositionDelta);

		const Vector3 catchUpLookAtPosition	= desiredLookAtPosition + lookAtPositionDelta;
		const Vector3 catchUpDelta			= catchUpLookAtPosition - cameraPosition;
		const float catchUpDeltaMag2		= catchUpDelta.Mag2();
		isCatchUpFrontValid					= (catchUpDeltaMag2 >= VERY_SMALL_FLOAT);
		if(isCatchUpFrontValid)
		{
			const float catchUpDeltaMag		= Sqrtf(catchUpDeltaMag2);
			catchUpFront					= catchUpDelta / catchUpDeltaMag;
		}
	}

	if(isCatchUpFrontValid)
	{
		Vector3 lookAtFrontToApply;
		camFrame::SlerpOrientation(m_BlendLevel, lookAtFront, catchUpFront, lookAtFrontToApply);

		lookAtFront.Set(lookAtFrontToApply);
	}

	return isCatchUpFrontValid;
}

void camCatchUpHelper::ComputeDesiredRoll(float& desiredRoll) const
{
	const float catchUpRoll	= m_SourceFrame.ComputeRoll();
	desiredRoll				= Lerp(m_BlendLevel, desiredRoll, catchUpRoll);
}

void camCatchUpHelper::ComputeLensParameters(camFrame& destinationFrame) const
{
	//Blend from the catch-up lens parameters to the desired destination parameters.

	const float catchUpFov			= m_SourceFrame.GetFov();
	const float desiredFov			= destinationFrame.GetFov();
	const float fovToApply			= Lerp(m_BlendLevel, desiredFov, catchUpFov);
	destinationFrame.SetFov(fovToApply);

	const float catchUpNearClip		= m_SourceFrame.GetNearClip();
	const float desiredNearClip		= destinationFrame.GetNearClip();
	const float nearClipToApply		= Lerp(m_BlendLevel, desiredNearClip, catchUpNearClip);
	destinationFrame.SetNearClip(nearClipToApply);

	const float catchUpFarClip		= m_SourceFrame.GetFarClip();
	const float desiredFarClip		= destinationFrame.GetFarClip();
	const float farClipToApply		= Lerp(m_BlendLevel, desiredFarClip, catchUpFarClip);
	destinationFrame.SetFarClip(farClipToApply);

	const float catchUpMbStrength	= m_SourceFrame.GetMotionBlurStrength();
	const float desiredMbStrength	= destinationFrame.GetMotionBlurStrength();
	const float mbStrengthToApply	= Lerp(m_BlendLevel, desiredMbStrength, catchUpMbStrength);
	destinationFrame.SetMotionBlurStrength(mbStrengthToApply);

	//NOTE: The depth of field settings are interpolated separately in ComputeDofParameters, which is performed in the post-update of the camera.
}

void camCatchUpHelper::ComputeDofParameters(camFrame& destinationFrame) const
{
	destinationFrame.InterpolateDofSettings(m_BlendLevel, destinationFrame, m_SourceFrame);
}

bool camCatchUpHelper::ComputeIsCatchUpSourceFrameValid(const camThirdPersonCamera& thirdPersonCamera) const
{
	const float distance	= ComputeDistanceAlongCatchUpSourceFrontToBasePivotPlane(thirdPersonCamera);
	const bool isValid		= (distance >= SMALL_FLOAT);

	return isValid;
}

float camCatchUpHelper::ComputeDistanceAlongCatchUpSourceFrontToBasePivotPlane(const camThirdPersonCamera& thirdPersonCamera) const
{
	const Vector3& catchUpFront	= m_SourceFrame.GetFront();

	float orbitHeading;
	float orbitPitch;
	ComputeDesiredOrbitOrientation(thirdPersonCamera, orbitHeading, orbitPitch);

	Vector3 orbitFront;
	camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, orbitFront);

	const float denominator = catchUpFront.Dot(orbitFront);
	if(IsNearZero(denominator, SMALL_FLOAT))
	{
		return 0.0f;
	}

	const Vector3& basePivotPosition		= thirdPersonCamera.GetBasePivotPosition();
	const Vector3& catchUpPosition			= m_SourceFrame.GetPosition();
	const Vector3 catchUpToBasePivotDelta	= basePivotPosition - catchUpPosition;

	const float numerator		= catchUpToBasePivotDelta.Dot(orbitFront);
	const float distanceToPlane	= numerator / denominator;

	return distanceToPlane;
}

void camCatchUpHelper::ComputeDesiredOrbitOrientation(const camThirdPersonCamera& thirdPersonCamera, float& orbitHeading, float& orbitPitch) const
{
	const Vector3& sourceFront = m_SourceFrame.GetFront();
	camFrame::ComputeHeadingAndPitchFromFront(sourceFront, orbitHeading, orbitPitch);

	//Compensate for the orbit pitch offset.
	const float orbitPitchOffset	= thirdPersonCamera.ComputeOrbitPitchOffset();
	orbitPitch						-= orbitPitchOffset;

	thirdPersonCamera.LimitOrbitPitch(orbitPitch);
}

bool camCatchUpHelper::WillFinishThisUpdate() const
{
	return (fwTimer::GetCamTimeInMilliseconds() - m_StartTime) > m_Metadata.m_BlendDuration; 
}
