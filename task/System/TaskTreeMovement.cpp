//Game headers.
#include "animation\AnimDefines.h"
#include "Event\Events.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "peds\PedTaskRecord.h"
#include "Peds\QueriableInterface.h"
#include "Task\System\TaskManager.h"
#include "Task\System\TaskTree.h"
#include "Task\System\TaskTreeMovement.h"
#include "Task\System\Task.h"
#include "Task\System\TaskClassInfo.h"
#include "Task\System\TaskFSMClone.h"
#include "Task\Default\TaskWander.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\TaskGoto.h"
#include "Task\Vehicle\TaskCarCollisionResponse.h"
#include "Task/System/TaskMove.h"

// rage headers
#include "Profile/timebars.h"

AI_OPTIMISATIONS()

#include "system/xtl.h"
//-------------------------------------------------------------------------
// process movement tasks
//-------------------------------------------------------------------------
void CTaskTreeMovement::Process(float fTimeStep)
{
    CPed *pPed = SafeCast(CPed, m_pEntity);

    // we don't want to process movement tasks for network clones
    if(pPed && !pPed->IsNetworkClone())
    {
	    //movement stuff
	    ProcessMoveTasksCheckedThisFrame();
	    UpdateTasks( true, -1, fTimeStep);

	    //if theres no default movement task create one.
	    if(m_iDefaultPriority >= 0)
	    {
		    if(m_Tasks[m_iDefaultPriority] == NULL)
			    SetTask( (CTaskMove*)rage_new CTaskMoveStandStill(), m_iDefaultPriority);
	    }
    }
}

//
//
//
void CTaskTreeMovement::SetTask(aiTask* pTask, const int iTaskPriority, const bool UNUSED_PARAM(bForceNewTask))
{
	Assert(iTaskPriority>=0);
	Assert(iTaskPriority<GetPriorityCount());

#if __ASSERT
	Assertf( !pTask || ((CTask*)pTask)->IsMoveTask(), "Normal task %s is being added to the move task tree!", (const char*)pTask->GetName());
#endif

#if __ASSERT
	Assertf(m_pEntity, "m_pEntity pointer cannot be NULL in CTaskTreeMovement::SetTask()");
	// tasks cannot be started on peds controlled by a remote machine in a network game, or on entities that are
	// in the process of having their control passed to another machine
	bool bIsClone    = m_pPed->IsNetworkClone();
	bool bCanSetTask = (!bIsClone || ((CNetObjPed*)m_pPed->GetNetworkObject())->CanSetTask());

	if(!bCanSetTask)
	{
		static const char* Names[] = {
		"PED_TASK_PRIORITY_PHYSICAL_RESPONSE",
		"PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP",
		"PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP",
		"PED_TASK_PRIORITY_PRIMARY",
		"PED_TASK_PRIORITY_DEFAULT",
		"PED_TASK_PRIORITY_MAX"
		};

		Printf("//-----------------------------------------\n");
		Printf("// B* 248896 START - QA, PLEASE COPY TO THE BUG REPORT :-) \n");
		Printf("//-----------------------------------------\n");
		Printf("Ped name %s \n",m_pPed->GetDebugName());

		for(int p = PED_TASK_PRIORITY_PHYSICAL_RESPONSE; p < PED_TASK_PRIORITY_MAX; ++p)
		{
			Printf("%s\n", Names[p]);
			CTask* task = m_pPed->GetPedIntelligence()->GetTaskAtPriority(p);

			int i = 0;
			while(task)
			{
				Printf("Task : %d : %s\n", i++, TASKCLASSINFOMGR.GetTaskName(task->GetTaskType()));
				task = task->GetSubTask();
			}	
		}

		Printf("//---------------------------------------------------------\n");
		Printf("// B* 248896 END - QA, PLEASE COPY ABOVE TO THE BUG REPORT \n");
		Printf("//---------------------------------------------------------\n");
	}

	Assertf(bCanSetTask, "Can't set a task on this ped!");

#endif // __ASSERT

	if(m_Tasks[iTaskPriority]==pTask)
	{
		return;
	}

	if(m_Tasks[iTaskPriority]) 
	{
		// If the movement task has not finished, abort before removing
		if( !m_Tasks[iTaskPriority]->GetIsFlagSet(aiTaskFlags::HasFinished) )
		{
			// Movement task may not necessarily have been aborted
			// Try to abort with priority 'urgent' firstly - this will not halt the ped
			if(!m_Tasks[iTaskPriority]->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL ))
			{
				// If this fails, we must abort immediately
				m_Tasks[iTaskPriority]->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL );
			}
		}
		DeleteTask(m_Tasks[iTaskPriority]);
		if( iTaskPriority == m_iMovementGeneralPriority )
		{
			// the response task is only valid as long as the main movement task remains
			AbortTasksWithPriority(m_iEventResponsePriority);
		}
	}
	m_Tasks[iTaskPriority]=pTask;

	if(m_Tasks[iTaskPriority])
	{
		m_Tasks[iTaskPriority]->SetEntity(m_pEntity);
	}

	// Ensure the active priority is up to date
	m_iActivePriority = ComputeActiveTaskPriority();
}

//
//
//
void CTaskTreeMovement::ProcessMoveTasksCheckedThisFrame()
{
	for(int i=0;i<GetPriorityCount();i++)
	{

		if(m_Tasks[i])
		{
			CTask *pTask = static_cast<CTask*>(m_Tasks[i].Get());

			Assert( ((CTask*)pTask)->IsMoveTask() );

			// When finished, the top movement task remains for a single frame and is then deleted.
			// Remove any finished tasks
			if( pTask->GetIsFlagSet(aiTaskFlags::HasFinished) )
			{
				DeleteTask(pTask);
				if( i == m_iMovementGeneralPriority )
				{
					// the response task is only valid as long as the main movement task remains
					AbortTasksWithPriority(m_iEventResponsePriority);
				}
			}
			// If the movement task has not been flagged as checked by the corresponding main task last frame.
			// Assume it has terminated and remove the task
			// Not checked last frame, abort the task
			else if( i == m_iMovementGeneralPriority && !pTask->GetMoveInterface()->GetCheckedThisFrame() )
			{
				if( !pTask->GetIsFlagSet(aiTaskFlags::IsAborted) )
				{
					pTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE,NULL );
				}
				DeleteTask(pTask);
				if( i == m_iMovementGeneralPriority )
				{
					// the response task is only valid as long as the main movement task remains
					AbortTasksWithPriority(m_iEventResponsePriority);
				}
			}
			else
			{
				// Mark the movement task as not used for the next frame
				if( pTask->GetMoveInterface() )
					pTask->GetMoveInterface()->SetCheckedThisFrame(false);
				break;
			}
		}
	}
}

//
//
//
aiTask* CTaskTreeMovement::FindTaskByTypeActive(const int iTaskType) const
{
	aiTask* pTaskOfType=0;
	aiTask* pTask;

	// Scan all the movement tasks
	if( !pTaskOfType )
	{
		pTask=GetActiveTask();
		while(pTask && !pTaskOfType)
		{
			if( !pTask->GetIsFlagSet(aiTaskFlags::HasFinished) && pTask->GetTaskType()==iTaskType)
			{
				pTaskOfType=pTask;
			}
			pTask=pTask->GetSubTask();
		}
	}
	return pTaskOfType;
}
