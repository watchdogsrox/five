//
// camera/cinematic/shot/camCinematicOnFootAssistedAimingShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicOnFootAssistedAimingShot.h"
#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTwoShotCamera.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootAssistedAimingKillShot,0x476D8FD2)
INSTANTIATE_RTTI_CLASS(camCinematicOnFootAssistedAimingReactionShot,0x95AB8297)

CAMERA_OPTIMISATIONS()

camCinematicOnFootAssistedAimingKillShot::camCinematicOnFootAssistedAimingKillShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootAssistedAimingKillShotMetadata&>(metadata))
{
}

void camCinematicOnFootAssistedAimingKillShot::PreUpdate(bool IsCurrentShot)
{
	camBaseCinematicShot::PreUpdate(IsCurrentShot);

	if (IsCurrentShot)
	{
		if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicTwoShotCamera::GetStaticClassId()))
		{
			const camThirdPersonAimCamera* thirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
			if (thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
			{
				const camThirdPersonPedAssistedAimCamera* assistedAimCamera = static_cast<const camThirdPersonPedAssistedAimCamera*>(thirdPersonAimCamera);
				camCinematicTwoShotCamera* cinematicCamera					= static_cast<camCinematicTwoShotCamera*>(m_UpdatedCamera.Get());

				Vector3 attachPosition;
				assistedAimCamera->GetCinematicMomentTargetPosition(attachPosition);
				cinematicCamera->SetAttachEntityPosition(attachPosition);
			}
		}
	}
}

bool camCinematicOnFootAssistedAimingKillShot::IsValid() const
{	
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}

	if(!m_LookAtEntity)
	{
		return false; 
	}

	const camThirdPersonAimCamera* thirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();

	if (!thirdPersonAimCamera || !thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
	{
		return false;
	}

	return true; 
}

void camCinematicOnFootAssistedAimingKillShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicTwoShotCamera::GetStaticClassId()))
		{
			const camThirdPersonAimCamera* thirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
			if (thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
			{
				const camThirdPersonPedAssistedAimCamera* assistedAimCamera = static_cast<const camThirdPersonPedAssistedAimCamera*>(thirdPersonAimCamera);
				camCinematicTwoShotCamera* cinematicCamera					= static_cast<camCinematicTwoShotCamera*>(m_UpdatedCamera.Get());

				Vector3 attachPosition;
				assistedAimCamera->GetCinematicMomentTargetPosition(attachPosition);
				cinematicCamera->SetAttachEntityPosition(attachPosition);

				cinematicCamera->LookAt(m_LookAtEntity);
				cinematicCamera->AttachTo(assistedAimCamera->GetCinematicMomentTargetEntity());
				cinematicCamera->SetCameraIsOnRight(assistedAimCamera->GetIsCinematicMomentCameraOnRight());
			}
		}
	}
}

const CEntity* camCinematicOnFootAssistedAimingKillShot::ComputeLookAtEntity() const
{
	const camThirdPersonCamera* thirdPersonCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
	const CEntity* attachParent = thirdPersonCamera ? thirdPersonCamera->GetAttachParent() : NULL;

	if(attachParent && attachParent->GetIsTypePed())
	{
		return attachParent;
	}

	return NULL;
}

bool camCinematicOnFootAssistedAimingKillShot::CanCreate()
{
	if(m_LookAtEntity && m_LookAtEntity->GetIsTypePed())
	{
		return true;
	}	

	return false; 
}



camCinematicOnFootAssistedAimingReactionShot::camCinematicOnFootAssistedAimingReactionShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootAssistedAimingReactionShotMetadata&>(metadata))
{
}

void camCinematicOnFootAssistedAimingReactionShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPedCloseUpCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
		}
	}
}

bool camCinematicOnFootAssistedAimingReactionShot::CanCreate()
{
	const camThirdPersonAimCamera* thirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	const CEntity* lockOnTarget	= (thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId(
		camThirdPersonPedAssistedAimCamera::GetStaticClassId())) ? thirdPersonAimCamera->GetLockOnTarget() : NULL;

	if(!lockOnTarget)
	{
		return false; 
	}
	
	const u32 time = fwTimer::GetTimeInMilliseconds();

	if((time - m_ShotStartTime) > m_Metadata.m_MinTimeBetweenReactionShots)
	{
		return true; 
	}
	return false; 
}

bool camCinematicOnFootAssistedAimingReactionShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}

	if(m_LookAtEntity)	
	{	
		if(m_UpdatedCamera)
		{	
			const u32 time = fwTimer::GetTimeInMilliseconds();

			if((time - m_ShotStartTime) > m_Metadata.m_ShotDurationLimits.y)
			{
				return false; 
			}
		}

		const CPed* targetPed = static_cast<const CPed*>(m_LookAtEntity.Get());
		
		if(targetPed && targetPed->GetPedIntelligence())	
		{
			//Only trigger a reaction shot when the follow ped is firing and not reloading.
			const CTaskGun* gunTask = static_cast<const CTaskGun*>(targetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
			
			if(gunTask)
			{
				const bool isFollowPedFiring	= gunTask->GetIsFiring();
				const bool isFollowPedReloading	= gunTask->GetIsReloading();
				if(isFollowPedFiring && !isFollowPedReloading)
				{
					return true; 
				}
			}
		}
	}
	return false;
}

