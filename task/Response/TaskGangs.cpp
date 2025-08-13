#include "task/Response/TaskGangs.h"

#include "Peds/Ped.h"
#include "Vehicles/vehicle.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/Vehicle/TaskEnterVehicle.h"

CTaskGangPatrol::CTaskGangPatrol(CPed* pPrimaryTarget)
: m_pPrimaryTarget(pPrimaryTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_GANG_PATROL);
}

CTaskGangPatrol::~CTaskGangPatrol()
{

}

bool CTaskGangPatrol::ProcessMoveSignals()
{
	CPed* pPed = GetPed();

	if (pPed)
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
			pVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
		}
	}

	return true;
}

CTask::FSM_Return CTaskGangPatrol::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_AttackTarget)
			FSM_OnEnter
				AttackTarget_OnEnter();
			FSM_OnUpdate
				return AttackTarget_OnUpdate();
		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter();
			FSM_OnUpdate
				return EnterVehicle_OnUpdate();
		FSM_State(State_WaitingForVehicleOccupants)
			FSM_OnEnter
				WaitingForVehicleOccupants_OnEnter();
			FSM_OnUpdate
				return WaitingForVehicleOccupants_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskGangPatrol::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_AttackTarget:					return "State_AttackTarget";
	case State_EnterVehicle:					return "State_EnterVehicle";
	case State_WaitingForVehicleOccupants:		return "State_WaitingForVehicleOccupants";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

CTask::FSM_Return CTaskGangPatrol::Start_OnUpdate()
{
	RequestProcessMoveSignalCalls();

	//Attack the target.
	SetState(State_AttackTarget);

	return FSM_Continue;
}

void CTaskGangPatrol::AttackTarget_OnEnter()
{
	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(m_pPrimaryTarget);
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
	pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_RequiresOrder);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGangPatrol::AttackTarget_OnUpdate()
{
	//Check if the task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if the last used vehicle is close, and the driver is not dead.
		if(IsLastUsedVehicleClose() && !IsDriverDead())
		{
			SetState(State_EnterVehicle);	
		}
		else
		{
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskGangPatrol::EnterVehicle_OnEnter()
{
	//Assert that the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	taskAssert(pVehicle);

	//Create the task.
	s32 iSeatIndex = pVehicle->GetSeatManager()->GetLastPedsSeatIndex(GetPed());
	SeatRequestType srType = (iSeatIndex >= 0) ? SR_Specific : SR_Any;
	CTaskEnterVehicle* pTask = rage_new CTaskEnterVehicle(pVehicle, srType, iSeatIndex);

	//Start the task.
	SetNewTask(pTask);
}

aiTask::FSM_Return CTaskGangPatrol::EnterVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are driving the vehicle.
		if(GetPed()->GetIsDrivingVehicle())
		{
			SetState(State_WaitingForVehicleOccupants);
		}
		else
		{
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskGangPatrol::WaitingForVehicleOccupants_OnEnter()
{
	//Assert that we are driving the vehicle.
	taskAssert(GetPed()->GetIsDrivingVehicle());
}

aiTask::FSM_Return CTaskGangPatrol::WaitingForVehicleOccupants_OnUpdate()
{
	//Check if we should not wait for vehicle occupants.
	if(!ShouldWaitForVehicleOccupants())
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

bool CTaskGangPatrol::IsDriverDead() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the driver is valid.
	const CPed* pDriver = (pVehicle->GetDriver()) ? pVehicle->GetDriver() :
		pVehicle->GetSeatManager()->GetLastPedInSeat(Seat_driver);
	if(!pDriver)
	{
		return false;
	}

	//Ensure the driver is dead.
	if(!pDriver->IsInjured())
	{
		return false;
	}

	return true;
}

bool CTaskGangPatrol::IsLastUsedVehicleClose() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pVehicle->GetTransform().GetPosition());
	static dev_float s_fMaxDistance = 30.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskGangPatrol::ShouldWaitForVehicleOccupant(const CPed& rPed) const
{
	//Ensure the ped is not dead.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the ped is not in a vehicle.
	if(rPed.GetIsInVehicle())
	{
		return false;
	}

	//Check if the ped is getting in our vehicle.
	if(rPed.GetVehiclePedEntering() == GetPed()->GetVehiclePedInside())
	{
		return true;
	}

	//Check if we have been in the state for a small amount of time.
	static dev_float s_fMaxTime = 3.0f;
	if(GetTimeInState() <= s_fMaxTime)
	{
		return true;
	}

	return false;
}

bool CTaskGangPatrol::ShouldWaitForVehicleOccupants() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}

	//Iterate over the seats.
	for(s32 i = 1; i < pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
	{
		//Check if we should wait for the last ped in the seat.
		const CPed* pLastPedInSeat = pVehicle->GetSeatManager()->GetLastPedInSeat(i);
		if(pLastPedInSeat && ShouldWaitForVehicleOccupant(*pLastPedInSeat))
		{
			return true;
		}
	}

	return false;
}
