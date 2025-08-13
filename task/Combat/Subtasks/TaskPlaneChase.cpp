//
// PlaneChase
// Designed for following some sort of entity
//

// File header
#include "Task/Combat/Subtasks/TaskPlaneChase.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoToPlane.h"
#include "vehicleAi/task/TaskVehiclePlaneChase.h"
#include "Vehicles/planes.h"
#include "Vehicles/vehicle.h"

AI_NAVIGATION_OPTIMISATIONS()
AI_OPTIMISATIONS()

//=========================================================================
// CClonedPlaneChaseInfo
//=========================================================================

CClonedPlaneChaseInfo::CClonedPlaneChaseInfo()
{}

CClonedPlaneChaseInfo::CClonedPlaneChaseInfo(u32 const state, CEntity const* target)
:
	m_target(const_cast<CEntity*>(target))
{
	SetStatusFromMainTaskState(state);
}

CClonedPlaneChaseInfo::~CClonedPlaneChaseInfo()
{}

CTaskFSMClone* CClonedPlaneChaseInfo::CreateCloneFSMTask()
{
	CAITarget aiTarget(m_target.GetEntity());
	return rage_new CTaskPlaneChase(aiTarget);
}

//=========================================================================
// CTaskPlaneChase
//=========================================================================

CTaskPlaneChase::Tunables CTaskPlaneChase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskPlaneChase, 0xd7804926);

CTaskPlaneChase::CTaskPlaneChase(const CAITarget& rTarget)
: m_Target(rTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_PLANE_CHASE);
}

CTaskPlaneChase::~CTaskPlaneChase()
{
}

#if !__FINAL
void CTaskPlaneChase::Debug() const
{
	CTask::Debug();
}

const char* CTaskPlaneChase::GetStaticStateName(s32 iState)
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


CPlane* CTaskPlaneChase::GetPlane() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a plane.
	if(!pVehicle->InheritsFromPlane())
	{
		return NULL;
	}

	return static_cast<CPlane *>(pVehicle);
}

const CEntity* CTaskPlaneChase::GetTargetEntity() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}
	
	return m_Target.GetEntity();
}

CTask::FSM_Return CTaskPlaneChase::ProcessPreFSM()
{
	//Ensure the plane is valid.
	if(!GetPlane())
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

CTask::FSM_Return CTaskPlaneChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

CTask::FSM_Return CTaskPlaneChase::Start_OnUpdate()
{
	//Move to the pursue state.
	TaskSetState(State_Pursue);
	
	return FSM_Continue;
}

void CTaskPlaneChase::Pursue_OnEnter()
{

	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity *>(GetTargetEntity()));
	params.SetTargetPosition(m_Target);
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved|DF_TargetPositionOverridesEntity;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = 60.0f;

	CTaskVehiclePlaneChase* pVehicleTask = rage_new CTaskVehiclePlaneChase(params, m_Target);
	pVehicleTask->SetMaxChaseSpeed(200.0f);
	pVehicleTask->SetMinChaseSpeed(20.0f);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPlane(), pVehicleTask);

	//Start the task.
	SetNewTask(pTask);

}

CTask::FSM_Return CTaskPlaneChase::Pursue_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		TaskSetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskPlaneChase::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskInfo* CTaskPlaneChase::CreateQueriableState() const
{
	return rage_new CClonedPlaneChaseInfo(GetState(), m_Target.GetEntity());
}

void CTaskPlaneChase::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_PLANE_CHASE );
    CClonedPlaneChaseInfo *planeChaseInfo = static_cast<CClonedPlaneChaseInfo*>(pTaskInfo);

	m_Target = planeChaseInfo->GetTarget();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

void CTaskPlaneChase::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_Pursue:
			expectedTaskType = CTaskTypes::TASK_CONTROL_VEHICLE; 
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		/*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

CTask::FSM_Return CTaskPlaneChase::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnUpdate)
	{
		if(GetState() != State_Finish)
		{		
			int iStateFromNetwork = GetStateFromNetwork();
			if(iStateFromNetwork != -1)
			{
				if(iStateFromNetwork != iState)
				{
					TaskSetState(iStateFromNetwork);
					return FSM_Continue;
				}
			}
		}

		UpdateClonedSubTasks(GetPed(), GetState());
	}

	FSM_Begin
	
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_Start)
		FSM_State(State_Pursue)

	FSM_End
}

CTaskFSMClone* CTaskPlaneChase::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskPlaneChase(m_Target);
}

CTaskFSMClone* CTaskPlaneChase::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskPlaneChase(m_Target);
}

void CTaskPlaneChase::TaskSetState(u32 const iState)
{
	aiTask::SetState(iState);
}