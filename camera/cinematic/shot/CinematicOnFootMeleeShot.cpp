//
// camera/cinematic/shot/camCinematicOnFootMeleeShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicOnFootMeleeShot.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"

#include "task/Combat/TaskCombatMelee.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootMeleeShot,0x40ED6443)

CAMERA_OPTIMISATIONS()

camCinematicOnFootMeleeShot::camCinematicOnFootMeleeShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootMeleeShotMetadata&>(metadata))
{
}

void camCinematicOnFootMeleeShot::InitCamera()
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypePed())
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicAnimatedCamera::GetStaticClassId()))
		{
			const CPed* followPed = static_cast<const CPed*>(m_AttachEntity.Get()); 

			//get the action result and the melee task
			const CTaskMeleeActionResult* pMeleeActionResult = followPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 

			const CTaskMelee* pMelee = followPed->GetPedIntelligence()->GetTaskMelee(); 

			if(pMelee && pMeleeActionResult && pMeleeActionResult->GetState() == CTaskMeleeActionResult::State_PlayClip)
			{
				camCinematicAnimatedCamera* pAnimCinematic = static_cast<camCinematicAnimatedCamera*>(m_UpdatedCamera.Get()); 

				pAnimCinematic->Init();

				//get the clip set from the action result task
				const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pMeleeActionResult->GetActionResult()->GetClipSet());

				//get the clip id from the task for the correct camera anim Id
				fwMvClipId ClipId(pMelee->GetTakeDownCameraAnimId()); 

				const crClip* pClip = pClipSet->GetClip(ClipId); 

				//set the start time to be the current time of the anim
				float StartTime = pMeleeActionResult->GetCurrentClipTime(); 

				Vector3 vPedPos = VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition());

				if(pAnimCinematic->SetCameraClip(vPedPos, pClipSet->GetClipDictionaryName(), pClip, StartTime))
				{
					//calculate where the ped will be at the start of the animation
					Vector3 vTargetPos = VEC3V_TO_VECTOR3( pMelee->GetStealthKillWorldOffset() );
					float fHeading = pMelee->GetStealthKillHeading();
					Matrix34 targetMat(M34_IDENTITY);

					targetMat.d = vTargetPos; 
					targetMat.MakeRotateZ(fHeading); 

					targetMat.d.z -= 1.0f; 

					pAnimCinematic->SetSceneMatrix(targetMat); 
				}
			}
		}
	}
}

bool camCinematicOnFootMeleeShot::CanCreate()
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypePed())
	{
		const CPed* followPed = static_cast<const CPed*>(m_AttachEntity.Get()); 
		if(followPed)
		{
			CTaskMeleeActionResult*	pMeleeActionResult = followPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 

			CTaskMelee* pTask = followPed->GetPedIntelligence()->GetTaskMelee(); 

			//check the task for a valid camera anim id
			if(pTask && pTask->GetTakeDownCameraAnimId() > 0)
			{
				if(pMeleeActionResult && pMeleeActionResult->GetState() == CTaskMeleeActionResult::State_PlayClip)
				{
					return true;  
				}
			}
		}
	}
	return false;
}

bool camCinematicOnFootMeleeShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}
	
	if(m_AttachEntity && m_AttachEntity->GetIsTypePed())
	{
		const CPed* followPed = static_cast<const CPed*>(m_AttachEntity.Get()); 
		if(followPed)
		{
			CTaskMeleeActionResult*	pMeleeActionResult = followPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 

			CTaskMelee* pTask = followPed->GetPedIntelligence()->GetTaskMelee(); 

			//check the task for a valid camera anim id
			if(pTask && pTask->GetTakeDownCameraAnimId() > 0)
			{
				if(pMeleeActionResult && pMeleeActionResult->GetState() == CTaskMeleeActionResult::State_PlayClip)
				{
					return true;  
				}
			}
		}
	}
	return false;
}

void camCinematicOnFootMeleeShot::Update()
{
	if (m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicAnimatedCamera::GetStaticClassId()))
	{
		if(m_AttachEntity && m_AttachEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(m_AttachEntity.Get()); 

			CTaskMeleeActionResult*	pMeleeActionResult = pPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 

			if(pMeleeActionResult && pMeleeActionResult->GetState() == CTaskMeleeActionResult::State_PlayClip)
			{				
				camCinematicAnimatedCamera* pAnimCinematic = static_cast<camCinematicAnimatedCamera*>(m_UpdatedCamera.Get());
				
				float CurrentTime = pMeleeActionResult->GetCurrentClipTime(); 
				
				pAnimCinematic->SetCurrentTime(CurrentTime); 

			}
		}
	}
}