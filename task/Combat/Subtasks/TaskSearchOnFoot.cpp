// FILE :    TaskSearchOnFoot.h
// PURPOSE : Subtask of search used while on foot

// File header
#include "Task/Combat/Subtasks/TaskSearchOnFoot.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "task/Combat/TaskCombat.h"
#include "task/General/TaskBasic.h"
#include "task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSearchOnFoot
//=========================================================================

//TODO: To make this task more general, it should go to the position before launching the search behavior.
//		However, this task is currently only launched from CTaskSearch, which takes care of going to the position.

CTaskSearchOnFoot::Tunables CTaskSearchOnFoot::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearchOnFoot, 0x4da52b95);

CTaskSearchOnFoot::CTaskSearchOnFoot(const Params& rParams)
: CTaskSearchBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_SEARCH_ON_FOOT);
}

CTaskSearchOnFoot::~CTaskSearchOnFoot()
{
}

#if !__FINAL
const char* CTaskSearchOnFoot::GetStaticStateName(s32 iState)
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

CTask::FSM_Return CTaskSearchOnFoot::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

CTask::FSM_Return CTaskSearchOnFoot::Start_OnUpdate()
{
	//Search for the target.
	SetState(State_Search);

	return FSM_Continue;
}

void CTaskSearchOnFoot::Search_OnEnter()
{
	//Calculate the flee offset.
	Vector3 vFleeOffset(VEC3_ZERO);
	vFleeOffset.x = ANGLE_TO_VECTOR_X(RtoD * m_Params.m_fDirection);
	vFleeOffset.y = ANGLE_TO_VECTOR_Y(RtoD * m_Params.m_fDirection);
	vFleeOffset.Scale(-sm_Tunables.m_FleeOffset);
	
	//Calculate the flee position.
	Vector3 vFleePosition = VEC3V_TO_VECTOR3(m_Params.m_vPosition) + vFleeOffset;
	
	//Create the params.
	CNavParams params;
	params.m_vTarget = vFleePosition;
	params.m_fTargetRadius = sm_Tunables.m_TargetRadius;
	params.m_fMoveBlendRatio = sm_Tunables.m_MoveBlendRatio;
	params.m_bFleeFromTarget = true;
	params.m_fCompletionRadius = sm_Tunables.m_CompletionRadius;
	params.m_fSlowDownDistance = sm_Tunables.m_SlowDownDistance;
	params.m_fFleeSafeDistance = sm_Tunables.m_FleeSafeDistance;
	
	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);
	pMoveTask->SetStopExactly(false);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_DefaultJustClips, NULL, VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), 0.0f);
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearchOnFoot::Search_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
