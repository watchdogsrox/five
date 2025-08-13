// File header
#include "Weapons/Gadgets/GadgetParachute.h"

// Game includes
#include "Event/Events.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/TaskParachute.h"
#include "Weapons/Info/WeaponInfoManager.h"

WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CGadgetParachute
//////////////////////////////////////////////////////////////////////////

bool CGadgetParachute::Created(CPed* pPed)
{
	if(!pPed->GetIsStanding())
	{
		// Already running parachute task?
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE))
		{
			// Gadget can be equipped
			return true;
		}
	}

	return false;
}

bool CGadgetParachute::Deleted(CPed* pPed)
{
	//Check if the intelligence is valid.
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if(pPedIntelligence)
	{
		//Check if the parachute task is valid.
		CTaskParachute* pTask = static_cast<CTaskParachute *>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
		if(pTask)
		{
			//Check if the task is in the parachuting state.
			if(pTask->GetState() == CTaskParachute::State_Parachuting)
			{
				//Remove the parachute.
				pTask->RemoveParachuteForScript();
			}
		}
	}

	return true;
}