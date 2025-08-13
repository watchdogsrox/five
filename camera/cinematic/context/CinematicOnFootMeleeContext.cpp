//
// camera/cinematic/CinematicOnFootMeleeContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicOnFootMeleeContext.h"
#include "camera/system/CameraMetadata.h"

#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/combat/TaskCombatMelee.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootMeleeContext,0x97F7F58E)

CAMERA_OPTIMISATIONS()

camCinematicOnFootMeleeContext::camCinematicOnFootMeleeContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicOnFootMeleeContextMetadata&>(metadata))
{

}

bool camCinematicOnFootMeleeContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	const CPed* followPed = camInterface::FindFollowPed();
	
	if(followPed)
	{
		CTaskMelee* pTask = followPed->GetPedIntelligence()->GetTaskMelee(); 

		if(pTask)
		{	
			return true; 
		}
	}
	return false; 
}
