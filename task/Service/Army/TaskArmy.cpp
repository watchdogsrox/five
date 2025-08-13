// FILE :    TaskArmy.cpp

// File header
#include "Task/Service/Army/TaskArmy.h"

// Game headers
#include "game/dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Default/TaskUnalerted.h"
#include "task/Service/Police/TaskPolicePatrol.h"
#include "task/Service/Swat/TaskSwat.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/driverpersonality.h"
#include "Vehicles/vehiclepopulation.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskArmy
////////////////////////////////////////////////////////////////////////////////

CTaskArmy::CTaskArmy(CTaskUnalerted* pTaskUnalerted)
: m_pTaskUnalerted(pTaskUnalerted)
{
	SetInternalTaskType(CTaskTypes::TASK_ARMY);
}

CTaskArmy::~CTaskArmy()
{
	//Free the unalerted task.
	CTask* pUnalertedTask = m_pTaskUnalerted.Get();
	delete pUnalertedTask;
}

#if !__FINAL
const char * CTaskArmy::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:					return "State_Start";
		case State_Unalerted:				return "State_Unalerted";
		case State_WanderInVehicle:			return "State_WanderInVehicle";
		case State_PassengerInVehicle:		return "State_PassengerInVehicle";
		case State_RespondToPoliceOrder:	return "State_RespondToPoliceOrder";
		case State_RespondToSwatOrder:		return "State_RespondToSwatOrder";
		case State_Combat:					return "State_Combat";
		case State_Finish:					return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

bool CTaskArmy::CheckForOrder()
{
	//Ensure the order is valid.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return false;
	}
	else if(!pOrder->GetIsValid())
	{
		return false;
	}

	//Check the order type.
	if(pOrder->IsPoliceOrder())
	{
		SetState(State_RespondToPoliceOrder);
	}
	else if(pOrder->GetType() == COrder::OT_Swat)
	{
		SetState(State_RespondToSwatOrder);
	}
	else
	{
		//Ensure the target for combat is valid.
		if(!GetTargetForCombat())
		{
			return false;
		}

		SetState(State_Combat);
	}

	return true;
}

CPed* CTaskArmy::GetTargetForCombat() const
{
	//Ensure the order is valid.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return NULL;
	}

	//Ensure the incident is valid.
	CIncident* pIncident = pOrder->GetIncident();
	if(!pIncident)
	{
		return NULL;
	}

	//Ensure the entity is valid.
	CEntity* pEntity = pIncident->GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<CPed *>(pEntity);
}

aiTask* CTaskArmy::Copy() const
{
	//Copy the unalerted task.
	CTaskUnalerted* pTaskUnalerted = m_pTaskUnalerted ? static_cast<CTaskUnalerted *>(m_pTaskUnalerted->Copy()) : NULL;
	taskAssert(!pTaskUnalerted || (pTaskUnalerted->GetTaskType() == CTaskTypes::TASK_UNALERTED));

	//Create the task.
	CTask* pTask = rage_new CTaskArmy(pTaskUnalerted);

	return pTask;
}

CScenarioPoint* CTaskArmy::GetScenarioPoint() const
{
	if(m_pTaskUnalerted)
	{
		return m_pTaskUnalerted->GetScenarioPoint();
	}
	else
	{
		return NULL;
	}
}

int CTaskArmy::GetScenarioPointType() const
{
	if(m_pTaskUnalerted)
	{
		return m_pTaskUnalerted->GetScenarioPointType();
	}
	else
	{
		return -1;
	}
}

bool CTaskArmy::ProcessMoveSignals()
{
	const s32 iState = GetState();
	if (iState == State_RespondToPoliceOrder
		|| iState == State_RespondToSwatOrder
		|| iState == State_Combat)
	{
		CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
		}
	}

	return true;
}

CTask::FSM_Return CTaskArmy::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Unalerted)
			FSM_OnEnter
				Unalerted_OnEnter();
			FSM_OnUpdate
				return Unalerted_OnUpdate();
			FSM_OnExit
				Unalerted_OnExit();

		FSM_State(State_WanderInVehicle)
			FSM_OnEnter
				WanderInVehicle_OnEnter();
			FSM_OnUpdate
				return WanderInVehicle_OnUpdate();

		FSM_State(State_PassengerInVehicle)
			FSM_OnEnter
				PassengerInVehicle_OnEnter();
			FSM_OnUpdate
				return PassengerInVehicle_OnUpdate();

		FSM_State(State_RespondToPoliceOrder)
			FSM_OnEnter
				RespondToPoliceOrder_OnEnter();
			FSM_OnUpdate
				return RespondToPoliceOrder_OnUpdate();

		FSM_State(State_RespondToSwatOrder)
			FSM_OnEnter
				RespondToSwatOrder_OnEnter();
			FSM_OnUpdate
				return RespondToSwatOrder_OnUpdate();

		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter();
			FSM_OnUpdate
				return Combat_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskArmy::Start_OnUpdate()
{
	RequestProcessMoveSignalCalls();

	//Check if we have an unalerted task.
	if(m_pTaskUnalerted)
	{
		SetState(State_Unalerted);
	}
	else
	{
		//Check if we have a valid order.
		if(CheckForOrder())
		{
			//The state has changed.
		}
		//Check if we are in a vehicle.
		else if(GetPed()->GetIsInVehicle())
		{
			if(GetPed()->GetIsDrivingVehicle())
			{
				SetState(State_WanderInVehicle);
			}
			else
			{
				SetState(State_PassengerInVehicle);
			}
		}
		else
		{
			SetState(State_Unalerted);
		}
	}

	return FSM_Continue;
}

void CTaskArmy::Unalerted_OnEnter()
{
	if(NetworkInterface::IsGameInProgress() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		GetPed()->SetRemoveAsSoonAsPossible(true);
	}

	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_UNALERTED))
	{
		return;
	}
	//Create the task.
	CTask* pTask = m_pTaskUnalerted.Get() ? m_pTaskUnalerted.Get() : rage_new CTaskUnalerted();
	m_pTaskUnalerted = NULL;
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::Unalerted_OnUpdate()
{
	//Check if we have a valid order.
	if(CheckForOrder())
	{
		//The state has been changed.
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}
	
	return FSM_Continue;
}

void CTaskArmy::Unalerted_OnExit()
{
	if(NetworkInterface::IsGameInProgress() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		GetPed()->SetRemoveAsSoonAsPossible(false);
	}
}

void CTaskArmy::WanderInVehicle_OnEnter()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	//Create the task.
	s32 iFlags = DMode_StopForCars;
	CVehiclePopulation::AddDrivingFlagsForAmbientVehicle(iFlags, pVehicle->InheritsFromBike());
	CTask* pTask = rage_new CTaskCarDriveWander(pVehicle, iFlags, CDriverPersonality::FindMaxCruiseSpeed(GetPed(), pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE), true);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::WanderInVehicle_OnUpdate()
{
	//Check if we have a valid order.
	if(CheckForOrder())
	{
		//The state has changed.
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}

void CTaskArmy::PassengerInVehicle_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskInVehicleBasic(GetPed()->GetVehiclePedInside(), false);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::PassengerInVehicle_OnUpdate()
{
	//Check if we have a valid order.
	if(CheckForOrder())
	{
		//The state has changed.
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}

void CTaskArmy::RespondToPoliceOrder_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskPoliceOrderResponse();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::RespondToPoliceOrder_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}

void CTaskArmy::RespondToSwatOrder_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSwatOrderResponse();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::RespondToSwatOrder_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}

void CTaskArmy::Combat_OnEnter()
{
	//Create the task.
	CPed* pTarget = GetTargetForCombat();
	taskAssert(pTarget);
	CTask* pTask = rage_new CTaskCombat(pTarget);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskArmy::Combat_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}
