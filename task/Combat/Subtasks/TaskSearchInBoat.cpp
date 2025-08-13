// FILE :    TaskSearchInBoat.h
// PURPOSE : Subtask of search used while in a boat

// File header
#include "Task/Combat/Subtasks/TaskSearchInBoat.h"

// Game headers
#include "Peds/Ped.h"
#include "task/Combat/Subtasks/TaskSearchInAutomobile.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSearchInBoat
//=========================================================================

//TODO: To make this task more general, it should go to the position before launching the search behavior.
//		However, this task is currently only launched from CTaskSearch, which takes care of going to the position.

CTaskSearchInBoat::Tunables CTaskSearchInBoat::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearchInBoat, 0xcd73e216);

CTaskSearchInBoat::CTaskSearchInBoat(const Params& rParams)
: CTaskSearchInVehicleBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_SEARCH_IN_BOAT);
}

CTaskSearchInBoat::~CTaskSearchInBoat()
{
}

#if !__FINAL
const char* CTaskSearchInBoat::GetStaticStateName(s32 iState)
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

CTask::FSM_Return CTaskSearchInBoat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

CTask::FSM_Return CTaskSearchInBoat::Start_OnUpdate()
{
	//Search for the target.
	SetState(State_Search);
	
	return FSM_Continue;
}

void CTaskSearchInBoat::Search_OnEnter()
{
	//It turns out automobile searches work for boats... for now.
	
	//Create the task.
	CTask* pTask = rage_new CTaskSearchInAutomobile(m_Params);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearchInBoat::Search_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
