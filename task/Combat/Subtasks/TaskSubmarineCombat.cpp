// FILE :    TaskSubmarineCombat.h
// PURPOSE : Subtask of combat used for submarine combat

// File header
#include "Task/Combat/Subtasks/TaskSubmarineCombat.h"

// Game headers
#include "Peds/Ped.h"
#include "task/Combat/TaskCombat.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/task/TaskVehicleGoToSubmarine.h"
#include "task/Combat/Subtasks/TaskSubmarineChase.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSubmarineCombat
//=========================================================================

CTaskSubmarineCombat::Tunables CTaskSubmarineCombat::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSubmarineCombat, 0x53c0ecbb);

CTaskSubmarineCombat::CTaskSubmarineCombat(const CAITarget& rTarget)
	: m_Target(rTarget)
	, m_ChangeStateTimer(1.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_SUBMARINE_COMBAT);
}

CTaskSubmarineCombat::~CTaskSubmarineCombat()
{
}

CTask::FSM_Return CTaskSubmarineCombat::ProcessPreFSM()
{
	m_ChangeStateTimer.Tick(GetTimeStep());

	//Ensure the sub is valid.
	if(!GetSub())
	{
		return FSM_Quit;
	}

	//Ensure the target entity is valid.
	if(!GetTargetEntity())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskSubmarineCombat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_Chase)
		FSM_OnEnter
		Chase_OnEnter();
	FSM_OnUpdate
		return Chase_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

Vec3V_Out CTaskSubmarineCombat::CalculateTargetOffsetForChase() const
{
	Vector3 vTargetPosition;
	m_Target.GetPosition(vTargetPosition);

	return VECTOR3_TO_VEC3V(vTargetPosition);
}

CSubmarine* CTaskSubmarineCombat::GetSub() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a submarine.
	if(!(pVehicle->InheritsFromSubmarine() || pVehicle->InheritsFromSubmarineCar()))
	{
		return NULL;
	}

	return static_cast<CSubmarine *>(pVehicle);
}

const CEntity* CTaskSubmarineCombat::GetTargetEntity() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}

	return m_Target.GetEntity();
}

const CPed* CTaskSubmarineCombat::GetTargetPed() const
{
	//Grab the target entity.
	const CEntity* pEntity = GetTargetEntity();

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<const CPed *>(pEntity);
}

CVehicle* CTaskSubmarineCombat::GetTargetVehicle() const
{
	//Ensure the target ped is valid.
	const CPed* pPed = GetTargetPed();
	if(!pPed)
	{
		return NULL;
	}

	return pPed->GetVehiclePedInside();
}

Vec3V_Out CTaskSubmarineCombat::CalculateTargetVelocity() const
{
	Vec3V vVelocity = Vec3V(0.0f, 0.0f, 0.0f);
	const CPed* pPed = GetTargetPed();
	if ( pPed )
	{
		if ( pPed->GetVehiclePedInside() )
		{
			vVelocity = VECTOR3_TO_VEC3V(pPed->GetVehiclePedInside()->GetVelocity());
		}
		else
		{
			if ( MagSquared(pPed->GetGroundVelocityIntegrated()).Getf() > pPed->GetVelocity().Mag2() )
			{
				vVelocity = pPed->GetGroundVelocityIntegrated();
			}
			else
			{
				vVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
			}
		}
	}

	return vVelocity;
}

bool CTaskSubmarineCombat::ShouldChase() const
{
	return true;
}

void CTaskSubmarineCombat::UpdateTargetOffsetForChase()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the task type is right.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_SUBMARINE_CHASE, "The sub-task type is invalid: %d.", pSubTask->GetTaskType()))
	{
		return;
	}

	//Grab the task.
	CTaskSubmarineChase* pTask = static_cast<CTaskSubmarineChase *>(pSubTask);

	// just keep computing this
	pTask->SetTargetOffset(CalculateTargetOffsetForChase());
}


CTask::FSM_Return CTaskSubmarineCombat::Start_OnUpdate()
{
	//Check if we should chase.
	if(ShouldChase())
	{
		//Move to the chase state.
		SetState(State_Chase);
	}
	else
	{
		taskAssertf(false, "The sub does not want to chase.");
	}

	return FSM_Continue;
}

void CTaskSubmarineCombat::Chase_OnEnter()
{
	//Calculate the target offset.
	Vec3V vTargetOffset = CalculateTargetOffsetForChase();

	//Create the task.
	CTaskSubmarineChase* pTask = rage_new CTaskSubmarineChase(m_Target, vTargetOffset);

	m_ChangeStateTimer.SetTime(1.0f);
	m_ChangeStateTimer.Reset();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSubmarineCombat::Chase_OnUpdate()
{
	//Update the target offset.
	UpdateTargetOffsetForChase();

// 	//Check if we should strafe.
// 	if(ShouldStrafe())
// 	{
// 		if ( m_ChangeStateTimer.IsFinished() )
// 		{
// 			//Move to the strafe state.
// 			SetState(State_Strafe);
// 		}
// 	}
// 	else
// 	{
// 		m_ChangeStateTimer.Reset();
// 	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

#if !__FINAL
const char* CTaskSubmarineCombat::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Chase",
		"State_Finish"
	};

	return aStateNames[iState];
}

void CTaskSubmarineCombat::Debug() const
{
	CTask::Debug();
}

#endif // !__FINAL