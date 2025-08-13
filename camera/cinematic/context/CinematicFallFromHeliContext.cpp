//
// camera/cinematic/CinematicFallFromHeliContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicFallFromHeliContext.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "vehicles/metadata/VehicleSeatInfo.h"

INSTANTIATE_RTTI_CLASS(camCinematicFallFromHeliContext,0x13E7C9E9)

CAMERA_OPTIMISATIONS()

camCinematicFallFromHeliContext::camCinematicFallFromHeliContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicFallFromHeliContextMetadata&>(metadata))
, m_HaveActiveTask(false)
{
}

void camCinematicFallFromHeliContext::PreUpdate()
{
	if(m_AttachVehicle == NULL)
	{
		const CPed* pFollowPed = camInterface::FindFollowPed(); 
		if(pFollowPed &&  pFollowPed->GetPedIntelligence())
		{
			const CQueriableInterface* PedQueriableInterface = pFollowPed->GetPedIntelligence()->GetQueriableInterface();

			if(PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD))
			{
				const CTaskDyingDead* DyingTask	= static_cast<const CTaskDyingDead*>(pFollowPed->GetPedIntelligence()->FindTaskActiveByType
					(CTaskTypes::TASK_DYING_DEAD));
				if(DyingTask && DyingTask->GetState() == CTaskDyingDead::State_FallOutOfVehicle)
				{
					CVehicle* pPlayerVehicle = pFollowPed->GetMyVehicle(); 

					if(pPlayerVehicle && pPlayerVehicle->InheritsFromHeli())
					{
						m_AttachVehicle = pPlayerVehicle; 
					}
				}
			}
		}
	}
}

bool camCinematicFallFromHeliContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	//NOTE: The special cinematic context, shot and the associated camera for falling from a heli dead have fallen into disrepair.
	//This is being disabled at the highest level in order to avoid rendering a bad cinematic shot in these circumstances.
	return false; 
}

bool camCinematicFallFromHeliContext::Update()
{
	if(IsValid())
	{ 
		if(UpdateContext())
		{
			m_HaveActiveTask = true; 
			m_AttachVehicle = NULL; 
			return true; 
		}
		else
		{
			m_HaveActiveTask = false; 
			m_AttachVehicle = NULL; 
		}
	}

	return false; 
}
