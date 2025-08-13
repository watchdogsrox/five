//
// camera/cinematic/CinematicBustedContext.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicBustedContext.h"

#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/Service/Police/TaskPolicePatrol.h"

INSTANTIATE_RTTI_CLASS(camCinematicBustedContext,0x480DEE3C)

CAMERA_OPTIMISATIONS()


camCinematicBustedContext::camCinematicBustedContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicBustedContextMetadata&>(metadata))
{
}

void camCinematicBustedContext::PreUpdate()
{
	const CPed* followPed = camInterface::FindFollowPed();
	if(!followPed)
	{
		m_ArrestingPed = NULL;

		return;
	}

	if(m_ArrestingPed)
	{
		//We have already cached an arresting ped, so check first that they are still valid.
		const bool isArrestingFollowPed	= ComputeIsPedArrestingPed(*m_ArrestingPed, *followPed);
		if(isArrestingFollowPed)
		{
			return;
		}
		else
		{
			m_ArrestingPed = NULL;
		}
	}

	const CPedIntelligence* followPedIntelligence = followPed->GetPedIntelligence();
	if(!followPedIntelligence)
	{
		return;
	}

	//Iterate over the nearby peds and check if any are arresting the follow ped.
	const CEntityScannerIterator nearbyPedInterator = followPedIntelligence->GetNearbyPeds();
	for(const CEntity* nearbyEntity = nearbyPedInterator.GetFirst(); nearbyEntity; nearbyEntity = nearbyPedInterator.GetNext())
	{
		const CPed* nearbyPed			= static_cast<const CPed*>(nearbyEntity);
		const bool isArrestingFollowPed	= ComputeIsPedArrestingPed(*nearbyPed, *followPed);
		if(isArrestingFollowPed)
		{
			m_ArrestingPed = nearbyPed;
			break;
		}
	}
}

bool camCinematicBustedContext::ComputeIsPedArrestingPed(const CPed& arrestingPed, const CPed& pedToBeArrested) const
{
	const CPedIntelligence* arrestingPedIntelligence = arrestingPed.GetPedIntelligence();
	if(!arrestingPedIntelligence)
	{
		return false;
	}

	const CQueriableInterface* arrestingPedQueriableInterface = arrestingPedIntelligence->GetQueriableInterface();
	if(!arrestingPedQueriableInterface)
	{
		return false;
	}

	bool isPedArrestingPed = false;

	if(arrestingPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ARREST_PED))
	{
		const CTaskArrestPed* arrestTask	= static_cast<const CTaskArrestPed*>(arrestingPedIntelligence->FindTaskActiveByType(
												CTaskTypes::TASK_ARREST_PED));
		isPedArrestingPed					= (arrestTask && (arrestTask->GetPedToArrest() == &pedToBeArrested) && arrestTask->GetWillArrest() &&
												arrestTask->IsTargetBeingBusted() && pedToBeArrested.GetIsArrested());
	}

	return isPedArrestingPed;
}
