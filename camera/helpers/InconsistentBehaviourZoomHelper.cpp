//
// camera/helpers/InconsistentBehaviourZoomHelper.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/InconsistentBehaviourZoomHelper.h"

#include "fwsys/timer.h"

#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/Frame.h"
#include "profile/group.h"
#include "profile/page.h"
#include "vehicles/vehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camInconsistentBehaviourZoomHelper, 0xdaa0a9e0)

PF_PAGE(camInconsistentBehaviourZoomHelperPage, "Camera: Inconsistent Behaviour Zoom Helper");

PF_GROUP(camInconsistentBehaviourZoomHelperMetrics);
PF_LINK(camInconsistentBehaviourZoomHelperPage, camInconsistentBehaviourZoomHelperMetrics);

PF_VALUE_FLOAT(vehicleAccelerationMag,	camInconsistentBehaviourZoomHelperMetrics);
PF_VALUE_FLOAT(headingSpeed,			camInconsistentBehaviourZoomHelperMetrics);
PF_VALUE_FLOAT(pitchSpeed,				camInconsistentBehaviourZoomHelperMetrics);
PF_VALUE_FLOAT(timeVehicleAirborne,		camInconsistentBehaviourZoomHelperMetrics);
PF_VALUE_FLOAT(timeSinceLostLos,		camInconsistentBehaviourZoomHelperMetrics);

#if __BANK
float camInconsistentBehaviourZoomHelper::ms_DebugMinZoomDuration				= 1000.0f;
float camInconsistentBehaviourZoomHelper::ms_DebugMaxZoomDuration				= 1400.0f;
float camInconsistentBehaviourZoomHelper::ms_DebugReactionTime					= 200.0f;
float camInconsistentBehaviourZoomHelper::ms_DebugBoundsRadiusScale				= 3.0f;
bool camInconsistentBehaviourZoomHelper::ms_DebugShouldOverrideIsInconsistent	= false;
bool camInconsistentBehaviourZoomHelper::ms_DebugIsInconsistent					= false;
#endif // __BANK

camInconsistentBehaviourZoomHelper::camInconsistentBehaviourZoomHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camInconsistentBehaviourZoomHelperMetadata&>(metadata))
, m_VehicleVelocityOnPreviousUpdate(VEC3_ZERO)
, m_TimeAtInconsistentBehaviour(0)
, m_TimeElapsedSinceVehicleAirborne(0)
, m_TimeElapsedSinceLostLos(0)
, m_ReactionTime(MAX_UINT32)
, m_MinZoomDuration(0)
, m_MaxZoomDuration(0)
, m_NumUpdatesPerformed(0)
, m_HeadingOnPreviousUpdate(0.0f)
, m_PitchOnPreviousUpdate(0.0f)
, m_BoundsRadiusScaleToUse(0.0f)
, m_WasReactingToInconsistencyLastUpdate(false)
{
#if __BANK
	ms_DebugIsInconsistent = false;
#endif // __BANK
}

bool camInconsistentBehaviourZoomHelper::UpdateShouldReactToInconsistentBehaviour(const CEntity* lookAtTarget, const camFrame& frame, const bool hasLosToTarget)
{
	if (m_WasReactingToInconsistencyLastUpdate)
	{
		PF_SET(vehicleAccelerationMag,	0.0f);
		PF_SET(headingSpeed,			0.0f);
		PF_SET(pitchSpeed,				0.0f);
		PF_SET(timeVehicleAirborne,		0.0f);
		PF_SET(timeSinceLostLos,		0.0f);
		return true;
	}

	const bool isBehavingInconsistently = CalculateIsTargetBehavingInconsistentlyThisFrame(lookAtTarget, frame, hasLosToTarget);

	//record the time when we first see an inconsistency
	const u32 currentTime = fwTimer::GetCamTimeInMilliseconds();
	if (isBehavingInconsistently && m_TimeAtInconsistentBehaviour == 0)
	{
		m_TimeAtInconsistentBehaviour = currentTime;
	}

	//don't do anything if we've never seen an inconsistency
	if (m_TimeAtInconsistentBehaviour == 0)
	{
		return false;
	}

	//report inconsistency after time has passed
	const u32 timeSinceInconsistentBehaviour	= currentTime - m_TimeAtInconsistentBehaviour;
	const bool reactToInconsistency				= (timeSinceInconsistentBehaviour >= m_ReactionTime);
	m_WasReactingToInconsistencyLastUpdate		= reactToInconsistency;
	
	return reactToInconsistency;
}

bool camInconsistentBehaviourZoomHelper::CalculateIsTargetBehavingInconsistentlyThisFrame(const CEntity* lookAtTarget, const camFrame& frame, const bool hasLosToTarget)
{
	if (!lookAtTarget || !lookAtTarget->GetIsTypeVehicle())
	{
		return false;
	}

	const CVehicle* targetVehicle = static_cast<const CVehicle*>(lookAtTarget);

	const float invTimeStep = fwTimer::GetInvTimeStep();

	bool isBehavingInconsistently = false;

	//detect a sudden stop
	if (m_Metadata.m_DetectSuddenMovementSettings.m_ShouldDetect)
	{
		const Vector3 vehicleVelocity			= targetVehicle->GetVelocity();
		if (m_NumUpdatesPerformed == 0)
		{
			m_VehicleVelocityOnPreviousUpdate	= vehicleVelocity;
		}

		const Vector3 vehicleAcceleration		= (vehicleVelocity - m_VehicleVelocityOnPreviousUpdate) * invTimeStep;
		const float vehicleAccelerationMag		= vehicleAcceleration.Mag();
		PF_SET(vehicleAccelerationMag, vehicleAccelerationMag);

		if (vehicleAccelerationMag > m_Metadata.m_DetectSuddenMovementSettings.m_MinVehicleAcceleration)
		{
			m_ReactionTime						= m_Metadata.m_DetectSuddenMovementSettings.m_ReactionTime;
			m_BoundsRadiusScaleToUse			= m_Metadata.m_DetectSuddenMovementSettings.m_BoundsRadiusScale;
			m_MinZoomDuration					= m_Metadata.m_DetectSuddenMovementSettings.m_MinZoomDuration;
			m_MaxZoomDuration					= m_Metadata.m_DetectSuddenMovementSettings.m_MaxZoomDuration;
			isBehavingInconsistently			= true;
		}

		m_VehicleVelocityOnPreviousUpdate		= vehicleVelocity;
	}

	//detect fast camera turn
	if (m_Metadata.m_DetectFastCameraTurnSettings.m_ShouldDetect)
	{
		float heading;
		float pitch;
		camFrame::ComputeHeadingAndPitchFromMatrix(frame.GetWorldMatrix(), heading, pitch);
		if (m_NumUpdatesPerformed == 0)
		{
			m_HeadingOnPreviousUpdate	= heading; 
			m_PitchOnPreviousUpdate		= pitch;
		}

		const float headingSpeed		= fwAngle::LimitRadianAngle(heading - m_HeadingOnPreviousUpdate) * invTimeStep;
		const float pitchSpeed			= fwAngle::LimitRadianAngle(pitch - m_PitchOnPreviousUpdate) * invTimeStep;
		PF_SET(headingSpeed, headingSpeed);
		PF_SET(pitchSpeed, pitchSpeed);

		if (Abs(headingSpeed) > m_Metadata.m_DetectFastCameraTurnSettings.m_AbsMaxHeadingSpeed
			|| Abs(pitchSpeed) > m_Metadata.m_DetectFastCameraTurnSettings.m_AbsMaxPitchSpeed)
		{
			m_ReactionTime				= m_Metadata.m_DetectFastCameraTurnSettings.m_ReactionTime;
			m_BoundsRadiusScaleToUse	= m_Metadata.m_DetectFastCameraTurnSettings.m_BoundsRadiusScale;
			m_MinZoomDuration			= m_Metadata.m_DetectFastCameraTurnSettings.m_MinZoomDuration;
			m_MaxZoomDuration			= m_Metadata.m_DetectFastCameraTurnSettings.m_MaxZoomDuration;
			isBehavingInconsistently	= true;
		}

		m_HeadingOnPreviousUpdate		= heading; 
		m_PitchOnPreviousUpdate			= pitch;
	}

	//detect airborne
	if (m_Metadata.m_DetectAirborneSettings.m_ShouldDetect && !targetVehicle->GetIsAircraft())
	{
		const bool isVehicleInAir			= targetVehicle->IsInAir();

		const u32 timeElapsed				= fwTimer::GetTimeStepInMilliseconds();
		m_TimeElapsedSinceVehicleAirborne	= isVehicleInAir ? m_TimeElapsedSinceVehicleAirborne + timeElapsed : 0;
		PF_SET(timeVehicleAirborne, (float)(m_TimeElapsedSinceVehicleAirborne/1000));

		if (m_TimeElapsedSinceVehicleAirborne > m_Metadata.m_DetectAirborneSettings.m_MinTimeSinceAirborne)
		{
			m_ReactionTime					= m_Metadata.m_DetectAirborneSettings.m_ReactionTime;
			m_BoundsRadiusScaleToUse		= m_Metadata.m_DetectAirborneSettings.m_BoundsRadiusScale;
			m_MinZoomDuration				= m_Metadata.m_DetectAirborneSettings.m_MinZoomDuration;
			m_MaxZoomDuration				= m_Metadata.m_DetectAirborneSettings.m_MaxZoomDuration;
			isBehavingInconsistently		= true;
		}
	}

	//line of sight
	if (m_Metadata.m_DetectLineOfSightSettings.m_ShouldDetect)
	{
		const u32 timeElapsed				= fwTimer::GetTimeStepInMilliseconds();
		m_TimeElapsedSinceLostLos			= !hasLosToTarget ? m_TimeElapsedSinceLostLos + timeElapsed : 0;
		PF_SET(timeSinceLostLos, (float)(m_TimeElapsedSinceLostLos/1000));

		if (m_TimeElapsedSinceLostLos > m_Metadata.m_DetectLineOfSightSettings.m_MinTimeSinceLostLos)
		{
			m_ReactionTime					= m_Metadata.m_DetectLineOfSightSettings.m_ReactionTime;
			m_BoundsRadiusScaleToUse		= m_Metadata.m_DetectLineOfSightSettings.m_BoundsRadiusScale;
			m_MinZoomDuration				= m_Metadata.m_DetectLineOfSightSettings.m_MinZoomDuration;
			m_MaxZoomDuration				= m_Metadata.m_DetectLineOfSightSettings.m_MaxZoomDuration;
			isBehavingInconsistently		= true;
		}
	}

#if __BANK
	if (ms_DebugShouldOverrideIsInconsistent)
	{
		m_MinZoomDuration			= (u32)ms_DebugMinZoomDuration;
		m_MaxZoomDuration			= (u32)ms_DebugMaxZoomDuration;
		m_ReactionTime				= (u32)ms_DebugReactionTime;
		m_BoundsRadiusScaleToUse	= ms_DebugBoundsRadiusScale;
		isBehavingInconsistently	= ms_DebugIsInconsistent;
	}
#endif // __BANK

	++m_NumUpdatesPerformed;

	return isBehavingInconsistently;
}

float camInconsistentBehaviourZoomHelper::GetBoundsRadiusScale() const
{
	return m_BoundsRadiusScaleToUse;
}

float camInconsistentBehaviourZoomHelper::GetMinFovDifference() const
{
	return m_Metadata.m_MinFovDifference;
}

float camInconsistentBehaviourZoomHelper::GetMaxFov() const
{
	return m_Metadata.m_MaxFov;
}

u32 camInconsistentBehaviourZoomHelper::GetZoomDuration() const
{
	const u32 randomZoomDuration = (u32)fwRandom::GetRandomNumberInRange((int)m_MinZoomDuration, (int)m_MaxZoomDuration);
	return randomZoomDuration;
}

#if __BANK
void camInconsistentBehaviourZoomHelper::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Inconsistent Behaviour Zoom helper");
	{
		bank.AddToggle("Should override inconsistency", &ms_DebugShouldOverrideIsInconsistent);
		bank.AddToggle("Is behaving inconsistent", &ms_DebugIsInconsistent);
		bank.AddSlider("Min zoom duration", &ms_DebugMinZoomDuration, 0.0f, 60000.0f, 1.0f);
		bank.AddSlider("Max zoom duration", &ms_DebugMaxZoomDuration, 0.0f, 60000.0f, 1.0f);
		bank.AddSlider("Reaction time", &ms_DebugReactionTime, 0.0f, 60000.0f, 1.0f);
		bank.AddSlider("Bounds Radius Scale", &ms_DebugBoundsRadiusScale, 0.0f, 1000.0f, 0.1f);
	}
	bank.PopGroup(); //Inconsistent Behaviour Zoom helper
}
#endif // __BANK