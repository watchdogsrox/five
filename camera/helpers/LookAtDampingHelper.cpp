//
// camera/helpers/LookAtDampingHelper.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/LookAtDampingHelper.h"

#include "profile/group.h"
#include "profile/page.h"

#include "fwmaths/angle.h"

#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "Network/NetworkInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camLookAtDampingHelper, 0xb442603d)

PF_PAGE(camLookAtDampingHelperPage, "Camera: Look At Damping Helper");

PF_GROUP(camLookAtDampingHelperMetrics);
PF_LINK(camLookAtDampingHelperPage, camLookAtDampingHelperMetrics);

PF_VALUE_FLOAT(desiredHeading, camLookAtDampingHelperMetrics);
PF_VALUE_FLOAT(dampedHeading, camLookAtDampingHelperMetrics);
PF_VALUE_FLOAT(desiredHeadingSpeed, camLookAtDampingHelperMetrics);
PF_VALUE_FLOAT(dampedHeadingSpeed, camLookAtDampingHelperMetrics);

camLookAtDampingHelper::camLookAtDampingHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camLookAtDampingHelperMetadata&>(metadata))
, m_IsSecondUpdate(false)
{
}

void camLookAtDampingHelper::Reset()
{
	m_PitchSpring.Reset(); 
	m_HeadingSpring.Reset();
	m_HeadingSpeedSpring.Reset();
	m_PitchSpeedSpring.Reset();
}

void camLookAtDampingHelper::Update(Matrix34& LookAtMat, float Fov, bool IsFirstUpdate)
{
	const float verticalFov			= Fov;
	const CViewport* viewport		= gVpMan.GetGameViewport();
	const float aspectRatio			= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const float horizontalFov		= Fov * aspectRatio;

	const float MaxHeadingError = 0.5f * m_Metadata.m_MaxHeadingError * horizontalFov * DtoR;
	const float MaxPitchError = 0.5f *  m_Metadata.m_MaxPitchError * verticalFov * DtoR;

	float desiredHeading;
	float desiredPitch;
	float desiredRoll; 
	
	camFrame::ComputeHeadingPitchAndRollFromMatrix(LookAtMat, desiredHeading, desiredPitch, desiredRoll); 
	
	if(IsFirstUpdate)
	{
		m_DesiredHeadingOnPreviousUpdate = desiredHeading; 
		m_DesiredPitchOnPreviousUpdate = desiredPitch;
	}

	float unWrappedDesiredHeading = desiredHeading; 

	//angular velocity update
	float dampedHeadingOnPreviousUpdate =  m_HeadingSpring.GetResult(); 
	float dampedPitchOnPreviousUpdate = m_PitchSpring.GetResult(); 

	//unwrap the desired heading w.r.t. the desired heading on the previous update
	float desiredHeadingDelta = desiredHeading - m_DesiredHeadingOnPreviousUpdate;
	
	if(desiredHeadingDelta > PI)
	{
		desiredHeading -= TWO_PI;
	}
	else if(desiredHeadingDelta < -PI)
	{
		desiredHeading += TWO_PI;
	}
	
	float timeStep = fwTimer::GetTimeStep();

	const float desiredHeadingSpeed = (desiredHeading - m_DesiredHeadingOnPreviousUpdate) / timeStep;
	const float desiredPitchSpeed = (desiredPitch - m_DesiredPitchOnPreviousUpdate) / timeStep; 

	float dampedSpeedHead = m_HeadingSpeedSpring.Update(desiredHeadingSpeed, IsFirstUpdate || m_IsSecondUpdate ? 0.0f : m_Metadata.m_SpeedHeadingSpringConstant, m_Metadata.m_SpeedHeadingSpringDampingRatio, true);
	float dampedSpeedPitch = m_PitchSpeedSpring.Update(desiredPitchSpeed, IsFirstUpdate || m_IsSecondUpdate ? 0.0f : m_Metadata.m_SpeedPitchSpringConstant, m_Metadata.m_SpeedPitchSpringDampingRatio, true);

	PF_SET(desiredHeadingSpeed, desiredHeadingSpeed);
	PF_SET(dampedHeadingSpeed, dampedSpeedHead);

	float speedDampedHeading = dampedHeadingOnPreviousUpdate + (dampedSpeedHead * timeStep); 
	float speedDampedPitch = dampedPitchOnPreviousUpdate + (dampedSpeedPitch * timeStep); 

	float desiredHeadingError =	fwAngle::LimitRadianAngleSafe(desiredHeading - speedDampedHeading); 

	desiredHeadingError = Clamp(desiredHeadingError, -MaxHeadingError, MaxHeadingError); 
	
	speedDampedHeading = desiredHeading - desiredHeadingError; 

	speedDampedPitch = Clamp(speedDampedPitch, desiredPitch - MaxPitchError, desiredPitch + MaxPitchError); 

	speedDampedHeading = fwAngle::LimitRadianAngleSafe(speedDampedHeading);
	speedDampedPitch = fwAngle::LimitRadianAngleSafe(speedDampedPitch);

	m_DesiredHeadingOnPreviousUpdate = unWrappedDesiredHeading; 
	m_DesiredPitchOnPreviousUpdate = desiredPitch; 

	//unwrap the desired heading w.r.t. the damped heading
	desiredHeading = unWrappedDesiredHeading;
	desiredHeadingDelta = desiredHeading - speedDampedHeading;

	//unwrap the desired heading w.r.t. the damped heading on the previous update
	if(desiredHeadingDelta > PI)
	{
		desiredHeading -= TWO_PI;
	}
	else if(desiredHeadingDelta < -PI)
	{
		desiredHeading += TWO_PI;
	}

	m_HeadingSpring.OverrideResult(speedDampedHeading); 
	m_PitchSpring.OverrideResult(speedDampedPitch); 

	//Pull the camera towards the look at target. 
	float dampedHeading = m_HeadingSpring.Update(desiredHeading, IsFirstUpdate ? 0.0f : m_Metadata.m_HeadingSpringConstant, m_Metadata.m_HeadingSpringDampingRatio, true);
	float dampedPitch = m_PitchSpring.Update(desiredPitch, IsFirstUpdate ? 0.0f : m_Metadata.m_PitchSpringConstant, m_Metadata.m_PitchSpringDampingRatio, true); 

	dampedHeading = fwAngle::LimitRadianAngleSafe(dampedHeading);
	dampedPitch = fwAngle::LimitRadianAngleSafe(dampedPitch);

	m_HeadingSpring.OverrideResult(dampedHeading);
	m_PitchSpring.OverrideResult(dampedPitch); 

	PF_SET(desiredHeading, desiredHeading);
	PF_SET(dampedHeading, dampedHeading);

	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(dampedHeading, dampedPitch, desiredRoll, LookAtMat); 

	m_IsSecondUpdate = IsFirstUpdate;
}
