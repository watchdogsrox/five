//
// camera/helpers/FrameInterpolator.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/FrameInterpolator.h"

#include "camera/camera_channel.h"
#include "camera/base/BaseCamera.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFrameInterpolator,0xE3607A47)


camFrameInterpolator::camFrameInterpolator(camBaseCamera& sourceCamera, const camBaseCamera& destinationCamera, u32 duration,
	bool shouldDeleteSourceCamera, u32 timeInMilliseconds)
: m_SourceCamera(&sourceCamera)
, m_StartTime(timeInMilliseconds)
, m_Duration(duration)
, m_BlendLevel(0.0f)
, m_MaxDistanceToTravel(-1.0f)
, m_MaxOrientationDelta(-1.0f)
, m_ExtraMotionBlurStrength(0.0f)
, m_ShouldDeleteSourceCamera(shouldDeleteSourceCamera)
, m_IsActive(true)
, m_IsSourceFrameCachedRelativeToDestination(false)
{
	const camFrame& destinationFrame = destinationCamera.GetFrame();
	CacheSourceCameraRelativeToDestinationInternal(sourceCamera, destinationFrame);

	m_ResultFrame.CloneFrom(sourceCamera.GetFrame());

	for(s32 i=0; i<camFrame::CAM_FRAME_MAX_COMPONENTS; i++)
	{
		m_ComponentCurveTypes[i] = SIN_ACCEL_DECEL;
	}
}

camFrameInterpolator::camFrameInterpolator(const camFrame& sourceFrame, u32 duration, u32 timeInMilliseconds)
: m_SourceCamera(NULL)
, m_StartTime(timeInMilliseconds)
, m_Duration(duration)
, m_BlendLevel(0.0f)
, m_MaxDistanceToTravel(-1.0f)
, m_MaxOrientationDelta(-1.0f)
, m_ExtraMotionBlurStrength(0.0f)
, m_ShouldDeleteSourceCamera(false)
, m_IsActive(true)
, m_IsSourceFrameCachedRelativeToDestination(false)
{
	m_CachedSourceFrame.CloneFrom(sourceFrame);
	m_ResultFrame.CloneFrom(sourceFrame);

	for(s32 i=0; i<camFrame::CAM_FRAME_MAX_COMPONENTS; i++)
	{
		m_ComponentCurveTypes[i] = SIN_ACCEL_DECEL;
	}
}

camFrameInterpolator::~camFrameInterpolator()
{
	Finish();
}

void camFrameInterpolator::SetCurveTypeForFrameComponent(camFrame::eComponentType frameComponent, eCurveTypes curveType)
{
	if(cameraVerifyf((frameComponent > camFrame::CAM_FRAME_COMPONENT_BAD) && (frameComponent < camFrame::CAM_FRAME_MAX_COMPONENTS),
		"An invalid component type was specified to a camera frame interpolation helper") &&
		cameraVerifyf((curveType >= LINEAR) && (curveType < NUM_CURVE_TYPES),
		"An invalid curve type was specified to a camera frame interpolation helper"))
	{
		m_ComponentCurveTypes[frameComponent] = curveType;
	}
}

const camFrame& camFrameInterpolator::Update(const camBaseCamera& destinationCamera, u32 timeInMilliseconds)
{
	const camFrame& destinationFrame	= destinationCamera.GetFrame();
	const camFrame& resultFrame			= Update(destinationFrame, timeInMilliseconds, &destinationCamera);

	return resultFrame;
}

const camFrame& camFrameInterpolator::Update(const camFrame& destinationFrame, u32 timeInMilliseconds, const camBaseCamera* destinationCamera)
{
	if(m_IsActive)
	{
		camFrame sourceFrame;

		if(m_SourceCamera)
		{
			sourceFrame.CloneFrom(m_SourceCamera->GetFrame());

			CacheSourceCameraRelativeToDestination(*m_SourceCamera, destinationFrame, destinationCamera);
		}
		else
		{
			sourceFrame.CloneFrom(m_CachedSourceFrame);

			if(m_IsSourceFrameCachedRelativeToDestination)
			{
				Matrix34 sourceWorldMatrix = sourceFrame.GetWorldMatrix();
				sourceWorldMatrix.Dot(destinationFrame.GetWorldMatrix()); //Transform to world space.
				sourceFrame.SetWorldMatrix(sourceWorldMatrix, false);
			}
		}

		const bool hasCut = UpdateBlendLevel(sourceFrame, destinationFrame, timeInMilliseconds, destinationCamera);

		InterpolateFrames(sourceFrame, destinationFrame, destinationCamera);

		if(hasCut)
		{
			//We are cutting the frame, so report this.
			m_ResultFrame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
		}

		if(m_BlendLevel >= (1.0f - SMALL_FLOAT))
		{
			m_BlendLevel = 1.0f;

			Finish();
			m_IsActive = false;
		}
	}

	return m_ResultFrame;
}

void camFrameInterpolator::CacheSourceCameraRelativeToDestinationInternal(const camBaseCamera& sourceCamera, const camFrame& destinationFrame)
{
	m_CachedSourceFrame.CloneFrom(sourceCamera.GetFrame());

	const Matrix34& destinationWorldMatrix = destinationFrame.GetWorldMatrix();
	if(!g_CameraWorldLimits_AABB.ContainsPoint(VECTOR3_TO_VEC3V(destinationWorldMatrix.d)))
	{
		//The destination frame is invalid and was most likely not initialised yet, so do not attempt to cache the source frame relative to this.
		return;
	}

	//Cache the source world matrix w.r.t. the destination world matrix.
	Matrix34 sourceWorldMatrix = m_CachedSourceFrame.GetWorldMatrix();
	sourceWorldMatrix.DotTranspose(destinationWorldMatrix);
	m_CachedSourceFrame.SetWorldMatrix(sourceWorldMatrix, false);
	m_IsSourceFrameCachedRelativeToDestination = true;
}

bool camFrameInterpolator::UpdateBlendLevel(const camFrame& sourceFrame, const camFrame& destinationFrame, u32 timeInMilliseconds,
	const camBaseCamera* UNUSED_PARAM(destinationCamera))
{
	const u32 timeSinceStart	= timeInMilliseconds - m_StartTime;
	m_BlendLevel				= (float)timeSinceStart / (float)m_Duration;
	m_BlendLevel				= Clamp(m_BlendLevel, 0.0f, 1.0f);

	if(m_MaxDistanceToTravel >= 0.0f)
	{
		//If the distance between the source and destination frames exceeds the limit, skip to the end of the interpolation and finish.
		const Vector3& sourcePosition		= sourceFrame.GetPosition();
		const Vector3& destinationPosition	= destinationFrame.GetPosition();
		const float distanceToTravel		= sourcePosition.Dist(destinationPosition);
		if(distanceToTravel > m_MaxDistanceToTravel)
		{
			const bool hasCut	= (m_BlendLevel < (1.0f - SMALL_FLOAT));
			m_BlendLevel		= 1.0f;

			return hasCut;
		}
	}

	if(m_MaxOrientationDelta >= 0.0f)
	{
		//If the orientation delta between the source and destination frames exceeds the limit, skip to the end of the interpolation and finish.
		const Vector3& sourceFront		= sourceFrame.GetFront();
		const Vector3& destinationFront	= destinationFrame.GetFront();
		const float orientationDelta	= sourceFront.FastAngle(destinationFront);
		if(orientationDelta > m_MaxOrientationDelta)
		{
			const bool hasCut	= (m_BlendLevel < (1.0f - SMALL_FLOAT));
			m_BlendLevel		= 1.0f;

			return hasCut;
		}
	}

	return false;
}

void camFrameInterpolator::InterpolateFrames(const camFrame& sourceFrame, const camFrame& destinationFrame, const camBaseCamera* destinationCamera)
{
	InterpolatePosition(sourceFrame, destinationFrame, destinationCamera);

	InterpolateOrientation(sourceFrame, destinationFrame, destinationCamera);

	//NOTE: The other frame components used to inherit the curve type of the orientation/matrix component, they all now use their own curve types.

	InterpolateFov(sourceFrame, destinationFrame);

	InterpolateNearClip(sourceFrame, destinationFrame);

	InterpolateFarClip(sourceFrame, destinationFrame);

	InterpolateCameraTimeScale(sourceFrame, destinationFrame);

	InterpolateDofSettings(sourceFrame, destinationFrame);

	InterpolateMotionBlurStrength(sourceFrame, destinationFrame);

	//Apply any extra motion blur for this interpolation.
	if(m_ExtraMotionBlurStrength > 0.0f)
	{
		float motionBlurStrength	= m_ResultFrame.GetMotionBlurStrength();
		motionBlurStrength			+= m_ExtraMotionBlurStrength;
		m_ResultFrame.SetMotionBlurStrength(motionBlurStrength);
	}

	InterpolateFlags(sourceFrame, destinationFrame);
}

void camFrameInterpolator::InterpolatePosition(const camFrame& sourceFrame, const camFrame& destinationFrame,
	const camBaseCamera* destinationCamera)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_POS];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	Vector3 resultPosition;
	InterpolatePosition(scaledBlendLevel, sourceFrame, destinationFrame, destinationCamera, resultPosition);

	m_ResultFrame.SetPosition(resultPosition);
}

void camFrameInterpolator::InterpolatePosition(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
	const camBaseCamera* UNUSED_PARAM(destinationCamera), Vector3& resultPosition)
{
	//Perform a simple linear interpolation.
	const Vector3& sourcePosition		= sourceFrame.GetPosition();
	const Vector3& destinationPosition	= destinationFrame.GetPosition();
	resultPosition.Lerp(blendLevel, sourcePosition, destinationPosition);
}

void camFrameInterpolator::InterpolateOrientation(const camFrame& sourceFrame, const camFrame& destinationFrame, const camBaseCamera* destinationCamera)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_MATRIX];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	Matrix34 resultWorldMatrix;
	InterpolateOrientation(scaledBlendLevel, sourceFrame, destinationFrame, destinationCamera, resultWorldMatrix);

	m_ResultFrame.SetWorldMatrix(resultWorldMatrix);
}

void camFrameInterpolator::InterpolateOrientation(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
	const camBaseCamera* UNUSED_PARAM(destinationCamera), Matrix34& resultWorldMatrix)
{
	//Perform a simple SLERP.
	const Matrix34& sourceWorldMatrix		= sourceFrame.GetWorldMatrix();
	const Matrix34& destinationWorldMatrix	= destinationFrame.GetWorldMatrix();
	camFrame::SlerpOrientation(blendLevel, sourceWorldMatrix, destinationWorldMatrix, resultWorldMatrix);
}

void camFrameInterpolator::InterpolateFov(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_FOV];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	float sourceValue		= sourceFrame.GetFov();
	float destinationValue	= destinationFrame.GetFov();

	float result = Lerp(scaledBlendLevel, sourceValue, destinationValue);

	m_ResultFrame.SetFov(result);
}

void camFrameInterpolator::InterpolateNearClip(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_NEARCLIP];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	float sourceValue		= sourceFrame.GetNearClip();
	float destinationValue	= destinationFrame.GetNearClip();

	float result = Lerp(scaledBlendLevel, sourceValue, destinationValue);

	m_ResultFrame.SetNearClip(result);
}

void camFrameInterpolator::InterpolateFarClip(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_FARCLIP];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	float sourceValue		= sourceFrame.GetFarClip();
	float destinationValue	= destinationFrame.GetFarClip();

	float result = Lerp(scaledBlendLevel, sourceValue, destinationValue);

	m_ResultFrame.SetFarClip(result);
}

void camFrameInterpolator::InterpolateCameraTimeScale(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	float sourceValue		= sourceFrame.GetCameraTimeScaleThisUpdate();
	float destinationValue	= destinationFrame.GetCameraTimeScaleThisUpdate();

	float result = Lerp(m_BlendLevel, sourceValue, destinationValue);

	m_ResultFrame.SetCameraTimeScaleThisUpdate(result);
}

void camFrameInterpolator::InterpolateDofSettings(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_DOF];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	m_ResultFrame.InterpolateDofSettings(scaledBlendLevel, sourceFrame, destinationFrame);
}

void camFrameInterpolator::InterpolateMotionBlurStrength(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	eCurveTypes curveType			= m_ComponentCurveTypes[camFrame::CAM_FRAME_COMPONENT_MOTION_BLUR];
	const float scaledBlendLevel	= ApplyCurve(curveType, m_BlendLevel);

	float sourceValue		= sourceFrame.GetMotionBlurStrength();
	float destinationValue	= destinationFrame.GetMotionBlurStrength();

	float result = Lerp(scaledBlendLevel, sourceValue, destinationValue);

	m_ResultFrame.SetMotionBlurStrength(result);
}

void camFrameInterpolator::InterpolateFlags(const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	u16 sourceFlags		= sourceFrame.GetFlags().GetAllFlags();
	u16 destinationFlags	= destinationFrame.GetFlags().GetAllFlags();

	u16 resultFlags;
	if(m_BlendLevel <= SMALL_FLOAT)
	{
		resultFlags = sourceFlags;
	}
	else if(m_BlendLevel >= (1.0f - SMALL_FLOAT))
	{
		resultFlags = destinationFlags;
	}
	else
	{
		resultFlags = (sourceFlags | destinationFlags);
	}

	m_ResultFrame.GetFlags().SetAllFlags(resultFlags);
}

float camFrameInterpolator::ApplyCurve(eCurveTypes type, float value)
{
	float result;
	switch(type)
	{
	case SIN_ACCEL_DECEL:
		result = SlowInOut(value);
		break;

	case ACCELERATION:
		result = SlowIn(value);
		break;

	case DECELERATION:
		result = SlowOut(value);
		break;

	case SLOW_IN:
		{
			result = SlowIn(value);
		}
		break;

	case SLOW_OUT:
		{
			result = SlowOut(value);
		}
		break;

	case SLOW_IN_OUT:
		{
			result = SlowInOut(value);
		}
		break;

	case VERY_SLOW_IN:
		{
			result = SlowIn(SlowIn(value));
		}
		break;

	case VERY_SLOW_OUT:
		{
			result = SlowOut(SlowOut(value));
		}
		break;

	case VERY_SLOW_IN_SLOW_OUT:
		{
			result = SlowIn(SlowInOut(value));
		}
		break;

	case SLOW_IN_VERY_SLOW_OUT:
		{
			result = SlowOut(SlowInOut(value));
		}
		break;

	case VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			result = SlowInOut(SlowInOut(value));
		}
		break;

	case QUADRATIC_EASE_IN:
		{
			result = QuadraticEaseIn(value);
		}
		break;

	case QUADRATIC_EASE_OUT:
		{
			result = QuadraticEaseOut(value);
		}
		break;

	case QUADRATIC_EASE_IN_OUT:
		{		
			result = QuadraticEaseInOut(value);
		}
		break;

	case CUBIC_EASE_IN:
		{
			result = CubicEaseIn(value);
		}
		break;

	case CUBIC_EASE_OUT:
		{
			result = CubicEaseOut(value);
		}
		break;

	case CUBIC_EASE_IN_OUT:
		{		
			result = CubicEaseInOut(value);
		}
		break;

	case QUARTIC_EASE_IN:
		{
			result = QuarticEaseIn(value);
		}
		break;

	case QUARTIC_EASE_OUT:
		{
			result = QuarticEaseOut(value);
		}
		break;

	case QUARTIC_EASE_IN_OUT:
		{
			result = QuarticEaseInOut(value);
		}
		break;

	case QUINTIC_EASE_IN:
		{
			result = QuinticEaseIn(value);
		}
		break;

	case QUINTIC_EASE_OUT:
		{
			result = QuinticEaseOut(value);
		}
		break;

	case QUINTIC_EASE_IN_OUT:
		{
			result = QuinticEaseInOut(value);
		}
		break;

	case CIRCULAR_EASE_IN:
		{
			result = CircularEaseIn(value);
		}
		break;

	case CIRCULAR_EASE_OUT:
		{
			result = CircularEaseOut(value);
		}
		break;

	case CIRCULAR_EASE_IN_OUT:
		{
			result =  CircularEaseInOut(value);
		}
		break;
		//case LINEAR:
		//case NUM_CURVE_TYPES:
	default:
		result = value;
		break;
	}

	result = Clamp(result, 0.0f, 1.0f);

	return result;
}

void camFrameInterpolator::Finish()
{
	if(m_SourceCamera)
	{
		if(m_ShouldDeleteSourceCamera)
		{
			delete m_SourceCamera;
		}
		else
		{
			m_SourceCamera = NULL; //Simply NULL our (registered) reference to the source camera.
		}
	}
}
