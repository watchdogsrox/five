//
// camera/cinematic/CinematicOnFootIdleContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicOnFootIdleContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "tools/SectorTools.h"
#include "peds/PedIntelligence.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootIdleContext,0x91B0873C)

CAMERA_OPTIMISATIONS()

PARAM(noIdleCam, "[CinematicOnFootIdleContext] Disable the idle camera");

camCinematicOnFootIdleContext::camCinematicOnFootIdleContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicOnFootIdleContextMetadata&>(metadata))
, m_FollowPedPositionOnPreviousUpdate(VEC3_ZERO)
, m_FollowPedHealthOnPreviousUpdate(-1.0f)
, m_PreviousIdleCameraInvalidationTime(0)
, m_IsPedIdle(false)
{

}

void camCinematicOnFootIdleContext::Invalidate()
{
	m_PreviousIdleCameraInvalidationTime = fwTimer::GetTimeInMilliseconds();
}

bool camCinematicOnFootIdleContext::Update()
{
	const bool succeded = camBaseCinematicContext::Update();

	//Cache the position of the target ped this frame to see if it has moved next frame
	const CPed* followPed = camInterface::GetGameplayDirector().GetFollowPed(); 
	if(followPed)
	{
		m_FollowPedPositionOnPreviousUpdate	= VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition());
		m_FollowPedHealthOnPreviousUpdate	= followPed->GetHealth();
	}

	return succeded; 
}

bool camCinematicOnFootIdleContext::IsPlayerInATrain(const CPed* followPed) const
{
	const CEntity* pEntity = followPed->GetGroundPhysical();

	if(pEntity && pEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity); 
		
		return pVehicle && pVehicle->InheritsFromTrain(); 
	}
	return false; 
}

bool camCinematicOnFootIdleContext::IsPedIdle() const
{
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();

	const CPed* followPed =gameplayDirector.GetFollowPed(); 

	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();

	if(followPed == NULL)
	{
		return false; 
	}

	const camThirdPersonAimCamera* thirdPersonAimCamera	= camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	const bool isAssistedAiming							= (thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId(
		camThirdPersonPedAssistedAimCamera::GetStaticClassId()));

	//NOTE: We derive an effective follow ped move speed using the position delta in order to detect teleports in addition to regular movement.
	const float gameTimeStep			= fwTimer::GetTimeStep();
	const float followPedMoveSpeed		= VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition()).Dist(m_FollowPedPositionOnPreviousUpdate) / gameTimeStep;
	const bool isFollowPedMovingTooFast	= followPedMoveSpeed > m_Metadata.m_MaxPedSpeedToActivateIdleCamera;
	const bool isFollowPedDead			= followPed->ShouldBeDead();
	const CWanted* followPedWanted		= followPed->GetPlayerWanted();
	const bool isFollowPedWanted		= followPedWanted && (followPedWanted->GetWantedLevel() > WANTED_CLEAN);
	const bool isFollowPedArrested		= followPed->GetIsArrested();
	const bool isFollowPedSwimming		= followPed->GetIsSwimming();
	const bool isFollowPedSkiing		= followPed->GetIsSkiing();
	const CPlayerInfo* playerInfo		= followPed->GetPlayerInfo();
	const bool isPlayerControlDisabled	= playerInfo ? playerInfo->AreControlsDisabled() : false;
	const bool isAScriptCameraRendering	= camInterface::GetScriptDirector().IsRendering();
	const bool isAiming					= camInterface::GetGameplayDirector().IsAiming(followPed);
	const bool isInCover				= (followPed->GetCoverPoint() != NULL);
	const bool isClimbing				= followPed->GetPedIntelligence()->IsPedClimbing(); 
	const bool inVehicle				= (vehicleEntryExitState != camGameplayDirector::OUTSIDE_VEHICLE);
	const float followPedHealth			= followPed->GetHealth();
	const float followPedHealthDelta	= (m_FollowPedHealthOnPreviousUpdate >= 0.0f) ? (followPedHealth - m_FollowPedHealthOnPreviousUpdate) : 0.0f;
	const bool wasFollowPedDamaged		= (followPedHealthDelta <= -SMALL_FLOAT);

	bool IsIdle							= false; 
	CTaskMotionBase* pPrimaryTask = followPed->GetPrimaryMotionTask();
	if (pPrimaryTask && pPrimaryTask->IsInMotionState(CPedMotionStates::MotionState_Idle))
	{
		IsIdle = true;
	}

#if FPS_MODE_SUPPORTED
//	if( !IsIdle && camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() &&
//		pPrimaryTask->GetSubTask() && pPrimaryTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING )
//	{
//		if( pPrimaryTask->GetSubTask()->GetState() == CTaskMotionAiming::State_StandingIdle )
//		{
//			IsIdle = true; 
//		}
//	}
	// Disable the idle cam in first person mode
	if( camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() )
	{
		IsIdle = false;
	}
#endif
	
	bool HasControlInputChanged = false; 

	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	if((control != NULL) && (control->InputHowLongAgo() <= 1000)) //Invalid if there was control input in the last second.
	{
		HasControlInputChanged = true; 
	}

#if SECTOR_TOOLS_EXPORT
	const bool isRunningSectorTools		= CSectorTools::GetEnabled();
#else
	const bool isRunningSectorTools		= false;
#endif // SECTOR_TOOLS_EXPORT

	if(!isFollowPedMovingTooFast && !isFollowPedDead && !isFollowPedWanted && !isFollowPedArrested && !isFollowPedSwimming &&
		!isFollowPedSkiing && !isPlayerControlDisabled && !isAScriptCameraRendering && !camInterface::GetGameplayDirector().IsHintActive() &&
		!isAiming && !isInCover && !isRunningSectorTools && !PARAM_noIdleCam.Get() && !isClimbing && !inVehicle &&
		IsIdle && !isAssistedAiming && !HasControlInputChanged && !IsPlayerInATrain(followPed) && !wasFollowPedDamaged)
	{
		return true; 
	}
	return false; 
}

bool camCinematicOnFootIdleContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	if(camInterface::IsDeathFailEffectActive() && !NetworkInterface::IsInSpectatorMode())
	{
		return false;
	}

	if(m_IsPedIdle)
	{
		u32 time = fwTimer::GetTimeInMilliseconds();	
		//Has enough idle time passed to activate the idle camera?
		u32 idleTimeExpired	= time - m_PreviousIdleCameraInvalidationTime;
		return (idleTimeExpired > m_Metadata.m_MinIdleTimeToActivateIdleCamera); //NOTE: We don't update m_PreviousIdleCameraInvalidationTime!
	}
	return false;
}

void camCinematicOnFootIdleContext::PreUpdate()
{
	m_IsPedIdle = IsPedIdle(); 
	
	if(!m_IsPedIdle)
	{
		m_PreviousIdleCameraInvalidationTime = fwTimer::GetTimeInMilliseconds(); 
	}
}

void camCinematicOnFootIdleContext::ClearContext()
{
	
	camBaseCinematicContext::ClearContext(); 
}
