// FILE :    TaskBoatStrafe.h
// PURPOSE : Subtask of boat combat used to strafe a target

// File header
#include "Task/Combat/Subtasks/TaskBoatStrafe.h"

// Game headers
#include "Peds/Ped.h"
#include "renderer/Water.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Vehicles/boat.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskBoatStrafe
//=========================================================================

CTaskBoatStrafe::Tunables CTaskBoatStrafe::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskBoatStrafe, 0xa80255d0);

CTaskBoatStrafe::CTaskBoatStrafe(const CAITarget& rTarget, float fRadius)
: m_Target(rTarget)
, m_fRadius(fRadius)
{
	SetInternalTaskType(CTaskTypes::TASK_BOAT_STRAFE);
}

CTaskBoatStrafe::~CTaskBoatStrafe()
{
}

#if !__FINAL
void CTaskBoatStrafe::Debug() const
{
	CTask::Debug();
}

const char* CTaskBoatStrafe::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Approach",
		"State_Strafe",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CBoat* CTaskBoatStrafe::GetBoat() const
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

bool CTaskBoatStrafe::ShouldApproach() const
{
	//Grab the boat.
	const CBoat* pBoat = GetBoat();

	//Grab the boat position.
	Vec3V vBoatPosition = pBoat->GetTransform().GetPosition();

	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = DistSquared(vBoatPosition, vTargetPosition);
	ScalarV scMinDistSq = ScalarVFromF32(square(m_fRadius + sm_Tunables.m_AdditionalDistanceForApproach));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskBoatStrafe::ShouldStrafe() const
{
	//Grab the boat.
	const CBoat* pBoat = GetBoat();
	
	//Grab the boat position.
	Vec3V vBoatPosition = pBoat->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));
	
	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(vBoatPosition, vTargetPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(m_fRadius + sm_Tunables.m_AdditionalDistanceForStrafe));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}
	
	return true;
}

CTask::FSM_Return CTaskBoatStrafe::ProcessPreFSM()
{
	//Ensure the boat is valid.
	if(!GetBoat())
	{
		return FSM_Quit;
	}

	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskBoatStrafe::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Approach)
			FSM_OnEnter
				Approach_OnEnter();
			FSM_OnUpdate
				return Approach_OnUpdate();

		FSM_State(State_Strafe)
			FSM_OnEnter
				Strafe_OnEnter();
			FSM_OnUpdate
				return Strafe_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskBoatStrafe::Start_OnUpdate()
{
	//Move to the approach state.
	SetState(State_Approach);

	return FSM_Continue;
}

void CTaskBoatStrafe::Approach_OnEnter()
{
	//Grab the boat.
	CBoat* pBoat = GetBoat();
	
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity *>(m_Target.GetEntity()));
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = pBoat->m_Transmission.GetDriveMaxVelocity();
	
	//Create the vehicle task.
	CTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_Target);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pBoat, pVehicleTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatStrafe::Approach_OnUpdate()
{
	//Check if we should strafe.
	if(ShouldStrafe())
	{
		//Move to the strafe state.
		SetState(State_Strafe);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskBoatStrafe::Strafe_OnEnter()
{
	//Grab the boat.
	CBoat* pBoat = GetBoat();

	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetPosition(VEC3V_TO_VECTOR3(CalculateTargetPositionForStrafe()));
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = sm_Tunables.m_CruiseSpeedForStrafe;

	//Create the vehicle task.
	CTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_Target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pBoat, pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatStrafe::Strafe_OnUpdate()
{
	//Update the target position for strafe.
	UpdateTargetPositionForStrafe();
	
	//Check if we should approach.
	if(ShouldApproach())
	{
		//Move to the approach state.
		SetState(State_Approach);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Restart the state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	
	return FSM_Continue;
}

Vec3V_Out CTaskBoatStrafe::CalculateTargetPositionForStrafe() const
{
	//Grab the boat.
	const CBoat* pBoat = GetBoat();
	
	//Grab the boat position.
	Vec3V vBoatPosition = pBoat->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));
	
	//Calculate the direction from the target to the boat.
	Vec3V vTargetToBoat = Subtract(vBoatPosition, vTargetPosition);
	Vec3V vTargetToBoatDirection = NormalizeFastSafe(vTargetToBoat, Vec3V(V_ZERO));
	
	//Rotate the direction.
	float fAmountToRotate = sm_Tunables.m_RotationLookAhead * DtoR;
	ScalarV scAmountToRotate = ScalarVFromF32(fAmountToRotate);
	Vec3V vRotatedDirection = RotateAboutZAxis(vTargetToBoatDirection, scAmountToRotate);
	
	//Calculate the difference between the distance and the radius.
	ScalarV scDistance = Dist(vBoatPosition, vTargetPosition);
	ScalarV scRadius = ScalarVFromF32(m_fRadius);
	ScalarV scDifference = Subtract(scRadius, scDistance);
	ScalarV scMaxAdjustmentLookAhead = ScalarVFromF32(sm_Tunables.m_MaxAdjustmentLookAhead);
	ScalarV scAdjustmentLookAhead = Clamp(scDifference, Negate(scMaxAdjustmentLookAhead), scMaxAdjustmentLookAhead);
	
	//Calculate the target radius.
	ScalarV scTargetRadius = Add(scDistance, scAdjustmentLookAhead);
	
	//Scale the rotated direction by the target radius to get the target offset.
	Vec3V vTargetOffset = Scale(vRotatedDirection, scTargetRadius);
	
	//Calculate the target position for the boat.
	Vec3V vBoatTargetPosition = Add(vTargetPosition, vTargetOffset);
	
	//Ensure the water level is valid.
	float fWaterZ;
	if(!Water::GetWaterLevelNoWaves(VEC3V_TO_VECTOR3(vBoatTargetPosition), &fWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		return vBoatPosition;
	}
	
	//Set the water level.
	vBoatTargetPosition.SetZf(fWaterZ);
	
	return vBoatTargetPosition;
}

void CTaskBoatStrafe::UpdateTargetPositionForStrafe()
{
	//Grab the boat.
	CBoat* pBoat = GetBoat();
	
	//Ensure the vehicle task is valid.
	CTaskVehicleMissionBase* pVehicleTask = pBoat->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return;
	}
	
	//Calculate the target position.
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(CalculateTargetPositionForStrafe());
	
	//Set the target position.
	pVehicleTask->SetTargetPosition(&vTargetPosition);
}
