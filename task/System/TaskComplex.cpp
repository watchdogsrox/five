//
// filename:	TaskComplex.cpp
// description:	Base class for all complex tasks. Complex tasks don't control the ped 
//				directly, so there is no ProcessPed function for complex tasks. A complex 
//				task can generate a simple task to control the ped or create another 
//				complex task which might then generate a simple or complex task and so on.
//				But the final task should always be a simple task.

// Framework headers
#include "fwmaths\random.h"


// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/DynamicEntity.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoTo.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// CTaskComplex
//-------------------------------------------------------------------------
CTaskComplex::CTaskComplex()
: m_pNewTaskToSet(NULL)
{
}

CTaskComplex::~CTaskComplex()
{
	Assert(m_pNewTaskToSet == NULL);
}

void CTaskComplex::CleanUp()
{
	if (m_pNewTaskToSet)
	{
		delete m_pNewTaskToSet;
        m_pNewTaskToSet = NULL;
	}
	tBaseClass::CleanUp();
}

bool CTaskComplex::IsValidForMotionTask(CTaskMotionBase & task) const
{
	bool isValid = tBaseClass::IsValidForMotionTask(task);
	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}

	return isValid;
}

//
//
// Basic FSM implementation, for the inherited class
CTask::FSM_Return CTaskComplex::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_CreateFirstSubTask)
			FSM_OnEnter
				return StateCreateFirstSubTask_OnEnter(pPed);
			FSM_OnUpdate
				return StateCreateFirstSubTask_OnUpdate(pPed);

		FSM_State(State_ControlSubTask)
			FSM_OnEnter
				return StateControlSubTask_OnEnter(pPed);
			FSM_OnUpdate
				return StateControlSubTask_OnUpdate(pPed);

		FSM_State(State_CreateNextSubTask)
			FSM_OnEnter
				return StateCreateNextSubTask_OnEnter(pPed);
			FSM_OnUpdate
				return StateCreateNextSubTask_OnUpdate(pPed);

		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//
//
//
CTask::FSM_Return CTaskComplex::StateCreateFirstSubTask_OnEnter(CPed* pPed)
{
	m_pNewTaskToSet = CreateFirstSubTask(pPed);
	if(m_pNewTaskToSet)
	{
		return FSM_Continue;
	}
	return FSM_Quit;
}

//
//
//
CTask::FSM_Return CTaskComplex::StateCreateFirstSubTask_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_ControlSubTask);
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskComplex::StateControlSubTask_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	if(m_pNewTaskToSet)//is a new task ready to be set, if so set it
	{
		SetNewTask(m_pNewTaskToSet);
		m_pNewTaskToSet = NULL;
	}
	
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskComplex::StateControlSubTask_OnUpdate(CPed* pPed)
{
	if(taskVerifyf(GetSubTask(), "GetSubTask is NULL"))
	{
		//If the subtask has ended or doesn't exit go to the CreateNextSubTask state.
		if( GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			SetState(State_CreateNextSubTask);
			return FSM_Continue;
		}

		aiTask* pTask = ControlSubTask(pPed);

		//is there a new task?
		if(pTask && pTask != GetSubTask())
		{
			m_pNewTaskToSet = pTask;
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else if( pTask == NULL )//no task, so just quit
		{		
			return FSM_Quit;
		}

		return FSM_Continue;
	}
	else
	{
#if !__FINAL
		pPed->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_ASSERT);
#endif // !__FINAL
	}

	return FSM_Quit;
}

//
//
//
CTask::FSM_Return CTaskComplex::StateCreateNextSubTask_OnEnter(CPed* pPed)
{
	m_pNewTaskToSet = CreateNextSubTask(pPed);

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskComplex::StateCreateNextSubTask_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(m_pNewTaskToSet)//if there is a new task go to the control sub task state.
	{
		SetState(State_ControlSubTask);
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;//no new task so quit
	}

}


#if !__FINAL
const char * CTaskComplex::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] = 
	{
		"State_CreateFirstSubTask",
		"State_ControlSubTask",
		"State_CreateNextSubTask",
		"Quit",
	};

	return sStateNames[iState];
}
#endif
