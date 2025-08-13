// FILE :    TaskSubmarineChase.h
// PURPOSE : Subtask of sub combat used to chase a target

// File header
#include "Task/Combat/Subtasks/TaskSubmarineChase.h"

// Game headers
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoToSubmarine.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/vehicle.h"

AI_VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()

//=========================================================================
// CTaskSubmarineChase
//=========================================================================

CTaskSubmarineChase::Tunables CTaskSubmarineChase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSubmarineChase, 0x48f6a878);

CTaskSubmarineChase::CTaskSubmarineChase(const CAITarget& rTarget, Vec3V_In vTargetOffset)
: m_vTargetOffset(vTargetOffset)
, m_Target(rTarget)
, m_OffsetRelative(OffsetRelative_Local)
{
	SetInternalTaskType(CTaskTypes::TASK_SUBMARINE_CHASE);
}

CTaskSubmarineChase::~CTaskSubmarineChase()
{
}

CVehicle* CTaskSubmarineChase::GetTargetVehicle() const
{
	//Ensure the target entity is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}

	//Ensure the entity is valid.
	CEntity* pEntity = const_cast<CEntity*>(m_Target.GetEntity());
	if(!pEntity)
	{
		return NULL;
	}

	//Check if the entity is a ped.
	if(pEntity->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pPed = static_cast<const CPed *>(pEntity);

		//Check if the ped is in a vehicle.
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			return pVehicle;
		}
	}
	//Check if the entity is a vehicle.
	else if(pEntity->GetIsTypeVehicle())
	{
		return static_cast<CVehicle *>(pEntity);
	}

	return NULL;
}

CSubmarine* CTaskSubmarineChase::GetSub() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a sub.
	if(!(pVehicle->InheritsFromSubmarine() || pVehicle->InheritsFromSubmarineCar()))
	{
		return NULL;
	}

	return static_cast<CSubmarine *>(pVehicle);
}

CTask::FSM_Return CTaskSubmarineChase::ProcessPreFSM()
{
	//Ensure we are valid.
	if(!GetSub())
	{
		return FSM_Quit;
	}

	//Ensure the target is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskSubmarineChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

CTask::FSM_Return CTaskSubmarineChase::Start_OnUpdate()
{
	//Move to the pursue state.
	SetState(State_Pursue);
	
	return FSM_Continue;
}

void CTaskSubmarineChase::Pursue_OnEnter()
{
	CSubmarine* pSub = GetSub();
	
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(GetTargetVehicle());
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved|DF_TargetPositionOverridesEntity;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = pSub->m_Transmission.GetDriveMaxVelocity();

	//Create the vehicle task.
	CTaskVehicleGoToSubmarine* pVehicleTask = rage_new CTaskVehicleGoToSubmarine(params, 0);
	
	// initialize value to target forward
	m_vTargetVelocitySmoothed = GetTargetVehicle()->GetTransform().GetForward();
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pSub, pVehicleTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSubmarineChase::Pursue_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	UpdateMissionParams();

	return FSM_Continue;
}

Vec3V_Out CTaskSubmarineChase::CalculateTargetPosition() const
{
	//Grab the target.
	const CPhysical* pTarget = GetTargetVehicle();

	//Grab the target position.
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
	Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(GetSub()->GetTransform().GetPosition());
	Vector3 toTarget = vTargetPosition - vCurrentPosition;
	float distanceToTarget = toTarget.Mag2();
	if(distanceToTarget < sm_Tunables.m_TargetBufferDistance*sm_Tunables.m_TargetBufferDistance)
	{
		return VECTOR3_TO_VEC3V(vTargetPosition);
	}
	else
	{
		distanceToTarget = sqrt(distanceToTarget);
		toTarget /= distanceToTarget;

		return VECTOR3_TO_VEC3V(vCurrentPosition + (toTarget * (distanceToTarget - sm_Tunables.m_TargetBufferDistance)));
	}
}

float CTaskSubmarineChase::CalculateSpeed() const
{
	//Calculate the base cruise speed.
	float fCruiseSpeed = GetSub()->m_Transmission.GetDriveMaxVelocity();
	return fCruiseSpeed;
}

void CTaskSubmarineChase::UpdateMissionParams()
{
	CSubmarine* pSub = GetSub();

	if(pSub)
	{
		//Ensure the vehicle task is valid.
		CTaskVehicleMissionBase* pVehicleTask = pSub->GetIntelligence()->GetActiveTask();
		if(!pVehicleTask || pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE)
		{
			return;
		}

		CTaskVehicleGoToSubmarine* pTask = static_cast<CTaskVehicleGoToSubmarine *>(pVehicleTask);

		static float s_Smooth = 5.0f;
		//Calculate the target position.
		Vec3V vDesiredTarget = CalculateTargetPosition();
		Vec3V vCurrentTarget = VECTOR3_TO_VEC3V(*pTask->GetTargetPosition());
		Vec3V vSmoothTarget = Lerp(ScalarV(s_Smooth * GetTimeStep()), vCurrentTarget, vDesiredTarget);

		//Set the target position.
		vSmoothTarget = Clamp( vSmoothTarget, Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX));
		pTask->SetTargetPosition(&RCC_VECTOR3(vSmoothTarget));

		//set speeds
		float fTargetSpeed = GetTargetVehicle()->GetVelocity().Mag();
		float fMaxSpeed = 15.0f; // arbitrary 
		float t = Min( fTargetSpeed / fMaxSpeed, 1.0f);
		float fSlowDownDistance = Lerp(t,  sm_Tunables.m_SlowDownDistanceMax, sm_Tunables.m_SlowDownDistanceMin );
		pTask->SetSlowDownDistance( fSlowDownDistance );
		pTask->SetCruiseSpeed(CalculateSpeed());
	}
}


#if !__FINAL
void CTaskSubmarineChase::Debug() const
{
	CTask::Debug();
}

const char* CTaskSubmarineChase::GetStaticStateName(s32 iState)
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