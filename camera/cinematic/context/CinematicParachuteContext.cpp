//
// camera/cinematic/CinematicParachuteContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicParachuteContext.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/system/CameraMetadata.h"

#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/Movement/TaskParachute.h"

INSTANTIATE_RTTI_CLASS(camCinematicParachuteContext,0x68E71EEC)

CAMERA_OPTIMISATIONS()

camCinematicParachuteContext::camCinematicParachuteContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicParachuteContextMetadata&>(metadata))
{
}

bool camCinematicParachuteContext::IsValid(bool shouldConsiderControlInput, bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	if(camInterface::IsDeathFailEffectActive() && !NetworkInterface::IsInSpectatorMode())
	{
		return false;
	}

	const CPed* pPed = camInterface::GetGameplayDirector().GetFollowPed(); 
	if(pPed)
	{
		CQueriableInterface* pedQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
		if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
		{
			s32 taskState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_PARACHUTE);

			if(taskState == CTaskParachute::State_Parachuting)
			{
				if(!shouldConsiderControlInput)
				{
					//Bypass all further checks, relating to control input.
					return true;
				}

				const camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 
				if(cinematicDirector.IsCinematicInputActive())
				{
					return true;
				}
			}
		}
	}

	return false; 
}

//bool camCinematicDirector::UpdateParachuting(const CPed& followPed)
//{
//	const bool hasSucceeded = UpdateParachutingViewMode(followPed);
//	if(!hasSucceeded)
//	{
//		CleanUpCinematicContext();
//		m_TargetEntity = NULL;
//	}
//
//	return hasSucceeded;
//}
//
//bool camCinematicDirector::UpdateParachutingShotSelection(const CPed& followPed)
//{
//	m_TargetEntity = &followPed; 
//
//	const bool isHappy = m_UpdatedCamera && IsActiveCameraHappy(); //We have a camera - is it still happy?
//
//	u32 time = fwTimer::GetTimeInMilliseconds();
//
//	bool shouldChangeCamera;
//
//	if(isHappy)
//	{
//		u32 cameraDuration = m_Metadata.m_DefaultCameraDuration;
//		if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_ParachuteCameraManCameraRef)
//		{
//			cameraDuration = m_Metadata.m_CameraManDuration;
//		}
//
//		u32 timeSinceLastChange	= time - m_PreviousCameraChangeTime;
//		shouldChangeCamera			= (timeSinceLastChange > cameraDuration);
//	}
//	else
//	{
//		//We either don't have an active camera, or it's not happy if we do, so we must change to a new camera.
//		shouldChangeCamera	= true;
//	}
//
//	if(shouldChangeCamera)
//	{
//		if(m_UpdatedCamera)
//		{
//			delete m_UpdatedCamera;
//		}
//
//		//Search for a good camera.
//		bool hasFoundAGoodCamera = false;
//		u32 maxIterationsToFindAGoodCamera = m_ProfileCamera.GetCount() * 2;
//		for(u32 i=0; i<maxIterationsToFindAGoodCamera; i++)
//		{	
//			UpdateCameraSelector(); 
//
//			if(m_CurrentCinematicCamera.CameraRef != NULL)
//			{
//				hasFoundAGoodCamera = CreateAndTestCamera(m_CurrentCinematicCamera.CameraRef);
//			}
//
//			if(hasFoundAGoodCamera)
//			{
//				break;
//			}
//		}
//
//		if(!hasFoundAGoodCamera)
//		{
//			//Can no longer create a safe fall-back camera, as I've removed the vehicle offset cameras.
//			return false;
//		}
//
//		m_PreviousCameraChangeTime = time;
//	}
//
//	return true;
//}
//
//void camCinematicDirector::ComputeHeliCamPathHeading(float currentHeading)
//{
//	float minHeadingBetweenShots = DtoR * m_Metadata.m_MinHeadingBetweenHeliCamShots; 
//	const float HeadingRange = 2.0f * ( PI - minHeadingBetweenShots );
//
//	const float HeadingDeltaWithinRange = fwRandom::GetRandomNumberInRange( 0.0f, HeadingRange );
//	float desiredHeading = currentHeading + minHeadingBetweenShots + HeadingDeltaWithinRange;
//	m_CinematicHeliDesiredPathHeading = fwAngle::LimitRadianAngle( desiredHeading );
//}
//
//bool camCinematicDirector::UpdateParachutingViewMode(const CPed& followPed)
//{
//	if(ShouldSwitchToParachuteCinematicMode())
//	{
//		UpdateCinematicViewModeControl();
//
//		if(!m_UpdatedCamera || !IsActiveCameraHappy() || m_ModeStepSize != 0)
//		{
//			float currentHeading = 0.0f;
//			if(m_UpdatedCamera)
//			{
//				currentHeading = m_Frame.ComputeHeading();
//				delete m_UpdatedCamera; 
//			}
//			else
//			{
//				CreateShotProfile(CC_PARACHUTE_CAMERA_MAN|CC_PARACHUTE_HELI, CSP_IN_PARACHUTE);
//
//				currentHeading = camInterface::GetGameplayDirector().GetFrame().ComputeHeading(); 
//			}
//
//			ComputeHeliCamPathHeading(currentHeading);
//
//			if (!UpdateParachutingShotSelection(followPed))
//			{
//				return false; 
//			}
//		}
//		return true;
//	}
//	return false;
//}
//
//bool camCinematicDirector::ShouldSwitchToParachuteCinematicMode()
//{
//	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
//	if(control)
//	{
//		const bool isCinematicButtonDown = control->GetVehicleCinematicCam().IsDown();
//
//		if(isCinematicButtonDown && !m_IsCinematicButtonDisabledByScript)
//		{
//			return true;
//		}
//	}
//
//	return false;
//}