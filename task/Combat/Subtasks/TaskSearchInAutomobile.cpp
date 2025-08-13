// FILE :    TaskSearchInAutomobile.h
// PURPOSE : Subtask of search used while in an automobile

// File header
#include "Task/Combat/Subtasks/TaskSearchInAutomobile.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSearchInAutomobile
//=========================================================================

//TODO: To make this task more general, it should go to the position before launching the search behavior.
//		However, this task is currently only launched from CTaskSearch, which takes care of going to the position.

CTaskSearchInAutomobile::Tunables CTaskSearchInAutomobile::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearchInAutomobile, 0x7cc34232);

CTaskSearchInAutomobile::CTaskSearchInAutomobile(const Params& rParams)
: CTaskSearchInVehicleBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_SEARCH_IN_AUTOMOBILE);
}

CTaskSearchInAutomobile::~CTaskSearchInAutomobile()
{
}

#if !__FINAL
const char* CTaskSearchInAutomobile::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Search",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

CTask::FSM_Return CTaskSearchInAutomobile::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Search)
			FSM_OnEnter
				Search_OnEnter();
		FSM_OnUpdate
				return Search_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSearchInAutomobile::Start_OnUpdate()
{
	//Search for the target.
	SetState(State_Search);

	return FSM_Continue;
}

void CTaskSearchInAutomobile::Search_OnEnter()
{
	//Calculate the flee offset.
	Vector3 vFleeOffset(VEC3_ZERO);
	vFleeOffset.x = ANGLE_TO_VECTOR_X(RtoD * m_Params.m_fDirection);
	vFleeOffset.y = ANGLE_TO_VECTOR_Y(RtoD * m_Params.m_fDirection);
	vFleeOffset.Scale(-sm_Tunables.m_FleeOffset);
	
	//Calculate the flee position.
	Vector3 vFleePosition = VEC3V_TO_VECTOR3(m_Params.m_vPosition) + vFleeOffset;
	
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetPosition(vFleePosition);
	params.m_iDrivingFlags = DMode_AvoidCars|DF_DontTerminateTaskWhenAchieved|DF_AvoidTargetCoors;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = sm_Tunables.m_CruiseSpeed;
	
	//Create the vehicle task.
	CTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetVehicle(), pVehicleTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearchInAutomobile::Search_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
