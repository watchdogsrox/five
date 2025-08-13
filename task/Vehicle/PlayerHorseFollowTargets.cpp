#include "Task/Vehicle/PlayerHorseFollowTargets.h"

#if ENABLE_HORSE

#include "optimisations.h"
#include "Peds/ped.h"
#include "scene/Physical.h"
#include "Peds/Ped.h" 

AI_OPTIMISATIONS()

void CPlayerHorseFollowTargets::Reset()
{
	ClearAllTargets();
}

void CPlayerHorseFollowTargets::AddTarget(const CPhysical* followTarget)
{
	//add a guy with default parameters
	AddTarget(followTarget, -1.0f, -1.0f, kModeAuto, kPriorityNormal, false);
}

void CPlayerHorseFollowTargets::AddTarget(const CPhysical* followTarget, float offsX, float offsY, 
		int followMode, int followPriority, bool disableWhenNotMounted, bool isPosse)
{
	if (!followTarget)
	{
		return;
	}

	if (followMode >= kNumFollowModes || followMode < 0)
	{
		followMode = kModeAuto;
	}
	if (followPriority >= kNumPriorities || followPriority < 0)
	{
		followPriority = kPriorityNormal;
	}

	FollowTargetEntry tempEntry;
	tempEntry.target = followTarget;
	tempEntry.offsetX = offsX;
	tempEntry.offsetY = offsY;
	tempEntry.disableWhenNotMounted = disableWhenNotMounted;
	tempEntry.mode = (FollowMode)followMode;
	tempEntry.priority = (FollowPriority)followPriority;
	tempEntry.isPosse = isPosse;

	//if the target already has an entry in the list, delete it
	//and add the new target.  this is so the designers can 
	//modify parameters by simply re-adding a target instead of having to
	//delete then add -JM
	RemoveTarget(followTarget);

	m_FollowTargets.Push(tempEntry);
}

void CPlayerHorseFollowTargets::RemoveTarget(int i)
{
	m_FollowTargets.DeleteFast(i);
}

void CPlayerHorseFollowTargets::RemoveTarget(const CPhysical* followTarget)
{
	//avoid using the atFixedArray's Find() function since we don't
	//want to have to populate a FollowTargetData struct to use as a key

	for (int i = 0; i < m_FollowTargets.GetCount(); ++i)
	{
		if (m_FollowTargets[i].target == followTarget)
		{
			m_FollowTargets.DeleteFast(i);
			break;
		}
	}
}

bool CPlayerHorseFollowTargets::ContainsTarget(const CPhysical* followTarget) const
{
	//avoid using the atFixedArray's Find() function since we don't
	//want to have to populate a FollowTargetData struct to use as a key

	for (int i = 0; i < m_FollowTargets.GetCount(); ++i)
	{
		if (m_FollowTargets[i].target == followTarget)
		{
			return true;
		}
	}

	return false;
}

bool CPlayerHorseFollowTargets::TargetIsPosseLeader(const CPhysical* followTarget) const
{
	//avoid using the atFixedArray's Find() function since we don't
	//want to have to populate a FollowTargetData struct to use as a key

	for (int i = 0; i < m_FollowTargets.GetCount(); ++i)
	{
		if (m_FollowTargets[i].target == followTarget)
		{
			return m_FollowTargets[i].isPosse;
		}
	}

	return false;
}

CPlayerHorseFollowTargets::FollowTargetEntry& CPlayerHorseFollowTargets::GetTarget(int i)
{
	return m_FollowTargets[i];
}

void CPlayerHorseFollowTargets::ClearAllTargets()
{
	m_FollowTargets.Reset();
}


bool HelperIsDismounted(const CPhysical* targetActor)
{
	//Assumptions:
	//targetActor has already been checked for validity

	Assert(targetActor);

	if (targetActor)
	{
		//non humans should always return that they are "mounted"
		//to keep the following going on

		if (!targetActor->GetIsTypePed())
		{
			return false;
		}

		const CPed* targetPed = static_cast<const CPed*>(targetActor);
		CPedModelInfo* pPMI = targetPed->GetPedModelInfo();
		if (!pPMI || !pPMI->GetPersonalitySettings().GetIsHuman())
		{
			return false;
		}

		if (targetPed->GetMountPedOn() || targetPed->GetVehiclePedEntering() || targetPed->GetVehiclePedInside())
		{
			//we are riding
			return false;
		}
	}

	return true;
}

#endif // ENABLE_HORSE