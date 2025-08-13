// FILE :    TaskBoatChase.h
// PURPOSE : Subtask of boat combat used to chase a target

// File header
#include "Task/Combat/Subtasks/TaskBoatChase.h"

// Game headers
#include "Peds/Ped.h"
#include "task/Combat/Subtasks/BoatChaseDirector.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehiclePursue.h"
#include "Vehicles/boat.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskBoatChase
//=========================================================================

CTaskBoatChase::Tunables CTaskBoatChase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskBoatChase, 0xfa16028e);

CTaskBoatChase::CTaskBoatChase(const CAITarget& rTarget)
: m_Target(rTarget)
, m_fDesiredOffsetForPursue(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_BOAT_CHASE);
}

CTaskBoatChase::~CTaskBoatChase()
{
}

void CTaskBoatChase::SetDesiredOffsetForPursue(float fOffset)
{
	//Set the offset.
	m_fDesiredOffsetForPursue = fOffset;

	//Note that the offset changed.
	OnDesiredOffsetForPursueChanged();
}

#if !__FINAL
void CTaskBoatChase::Debug() const
{
	CTask::Debug();
}

const char* CTaskBoatChase::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Pursue",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskBoatChase::AddToDirector()
{
	//Remove from the director.
	RemoveFromDirector();

	//Find the director for the target vehicle.
	CBoatChaseDirector* pDirector = CBoatChaseDirector::Find(*GetTargetVehicle());
	if(!pDirector)
	{
		return;
	}

	//Add to the director.
	pDirector->Add(this);
}

CBoat* CTaskBoatChase::GetBoat() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a boat.
	if(!pVehicle->InheritsFromBoat())
	{
		return NULL;
	}

	return static_cast<CBoat *>(pVehicle);
}

CVehicle* CTaskBoatChase::GetTargetVehicle() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}
	
	//Ensure the entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!pEntity)
	{
		return NULL;
	}
	
	//Ensure the target is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}
	
	//Grab the ped.
	const CPed* pPed = static_cast<const CPed *>(pEntity);
	
	return pPed->GetVehiclePedInside();
}

CTaskVehiclePursue* CTaskBoatChase::GetVehicleTaskForPursue() const
{
	//Ensure we are pursuing.
	if(GetState() != State_Pursue)
	{
		return NULL;
	}

	//Ensure the boat is valid.
	const CBoat* pBoat = GetBoat();
	if(!pBoat)
	{
		return NULL;
	}

	//Ensure the vehicle task is valid.
	CTask* pVehicleTask = pBoat->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return NULL;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_PURSUE)
	{
		return NULL;
	}

	return static_cast<CTaskVehiclePursue *>(pVehicleTask);
}

void CTaskBoatChase::OnDesiredOffsetForPursueChanged()
{
	//Ensure the task is valid.
	CTaskVehiclePursue* pTask = GetVehicleTaskForPursue();
	if(!pTask)
	{
		return;
	}

	//Set the desired offset.
	pTask->SetDesiredOffset(m_fDesiredOffsetForPursue);
}

void CTaskBoatChase::RemoveFromDirector()
{
	//Ensure the target is valid.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle)
	{
		return;
	}

	//Find the director for the target vehicle.
	CBoatChaseDirector* pDirector = CBoatChaseDirector::Find(*pTargetVehicle);
	if(!pDirector)
	{
		return;
	}

	//Remove from the director.
	pDirector->Remove(this);
}

void CTaskBoatChase::CleanUp()
{
	//Remove from the director.
	RemoveFromDirector();
}

CTask::FSM_Return CTaskBoatChase::ProcessPreFSM()
{
	//Ensure the boat is valid.
	if(!GetBoat())
	{
		return FSM_Quit;
	}

	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskBoatChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Pursue)
			FSM_OnEnter
				Pursue_OnEnter();
			FSM_OnUpdate
				return Pursue_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskBoatChase::Start_OnUpdate()
{
	//Add to the director.
	AddToDirector();

	//Move to the pursue state.
	SetState(State_Pursue);
	
	return FSM_Continue;
}

void CTaskBoatChase::Pursue_OnEnter()
{
	//Grab the boat.
	CBoat* pBoat = GetBoat();
	
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(GetTargetVehicle());
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = pBoat->m_Transmission.GetDriveMaxVelocity();
	
	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehiclePursue(params, sm_Tunables.m_IdealDistanceForPursue, m_fDesiredOffsetForPursue);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_Target);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pBoat, pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatChase::Pursue_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
