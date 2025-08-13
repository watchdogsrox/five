// File header
#include "Weapons/Gadgets/GadgetSkis.h"
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskPlayer.h"

//////////////////////////////////////////////////////////////////////////
// CGadgetSkis
//////////////////////////////////////////////////////////////////////////

bool CGadgetSkis::Created(CPed* pPed)
{
	// Don't allow skiing when swimming, surrendering or when the player is in or moving to cover
	if(!pPed->GetIsSwimming())
	{
		if (pPed->IsAPlayerPed())
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
			if (pTask && pTask->GetState() == CTaskPlayerOnFoot::STATE_USE_COVER)
			{
				return false;
			}
		}
		
		pPed->SetIsSkiing(true);
		return true;
	}
	return false;
}

bool CGadgetSkis::Deleted(CPed* pPed)
{
	pPed->SetIsSkiing(false);
	return true;
}
