//Game headers.
#include "animation\AnimDefines.h"
#include "Event\Events.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "peds\PedTaskRecord.h"
#include "Peds\QueriableInterface.h"
#include "Task\System\TaskManager.h"
#include "Task\System\TaskTree.h"
#include "Task\System\TaskTreePed.h"
#include "Task\System\Task.h"
#include "Task\System\TaskClassInfo.h"
#include "Task\System\TaskFSMClone.h"
#include "Task\Default\TaskWander.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\TaskGoto.h"
#include "Task\Vehicle\TaskCarCollisionResponse.h"
#include "Task/System/TaskMove.h"
#include "Task/System/Task.h"

// rage headers
#include "Profile/timebars.h"

AI_OPTIMISATIONS()

//
//
//
CTaskTreePed::CTaskTreePed(s32 iNumberOfPriorities, fwEntity *pPed, s32 iEventResponsePriority, s32 iDefaultPriority/* = -1*/) : 
CTaskTree( pPed, iNumberOfPriorities ),
m_iEventResponsePriority(iEventResponsePriority),
m_iDefaultPriority(iDefaultPriority)
{
	Assert(pPed);
	Assert(dynamic_cast<CPed*>(pPed));
	m_pPed = (static_cast<CPed*>(pPed));
}


//
//
//
void CTaskTreePed::SetTask(aiTask* pTask, const int iPriority, const bool UNUSED_PARAM(bForceNewTask))
{   
	taskFatalAssertf(iPriority >= 0 && iPriority < m_Tasks.GetCount(), "aiTaskTree::SetTask: iPriority [%d] out of range [0,%d]", iPriority, m_Tasks.GetCount());

#if !__FINAL 
	if(pTask)
	{
		for(s32 i = MAX_TASK_HISTORY-1; i > 0; i--)
		{
			m_TaskHistory[i] = m_TaskHistory[i-1];
		}

		m_TaskHistory[0] = pTask->GetTaskType();

#if AI_OPTIMISATIONS_OFF
		taskAssertf( pTask == NULL || TASKCLASSINFOMGR.TaskNameExists(pTask->GetTaskType()), "Task (%s) doesn't have a definition", pTask->GetName().c_str());
#endif
	}
#endif // !__FINAL

	if(m_iDefaultPriority >= 0)
	{
		//don't allow default tasks to be replaced when a default task is being processed
		//don't allow default tasks to be replaced within makeabortable
		if( iPriority == m_iDefaultPriority &&
			m_Tasks[iPriority] != NULL && 
			( m_iPriorityBeingProcessed == m_iDefaultPriority || m_Tasks[iPriority]->GetIsFlagSet(aiTaskFlags::InMakeAbortable) ) )
		{
			delete pTask;
			return;
		}
	}

	// Don't allow SetTask to be called when the we are currently in MakeAbortable, 
	// and this will cause a crash as we'll be operating on a deleted pointer
	taskFatalAssertf(!m_Tasks[iPriority] || !m_Tasks[iPriority]->GetIsFlagSet(aiTaskFlags::InMakeAbortable), "Replacing task [%s] with [%s] while it is in MakeAbortable which will cause a crash", m_Tasks[iPriority]->GetName().c_str(), pTask->GetName().c_str());

#if __ASSERT
	if(pTask && ((CTask*)pTask)->IsMoveTask())
	{
		//TODO
		//m_pEntity->GetPedIntelligence()->PrintTasks();
		Assertf( !pTask || !((CTask*)pTask)->IsMoveTask(), "Move task %s is being added to the main task tree! Please include the debug spew with the bug report, it will help us find out how this happened.", (const char*)pTask->GetName());
	}
#endif

	// tasks cannot be started on peds controlled by a remote machine in a network game, or on peds that are
	// in the process of having their control passed to another machine
	bool canAddTask = true;

	if(m_pPed->IsNetworkClone())
	{
		CNetObjPed *netObjPed = static_cast<CNetObjPed *>(m_pPed->GetNetworkObject());

		if(netObjPed && !netObjPed->CanSetTask())
		{
			canAddTask = false;
		}
	}

	if(!canAddTask)
	{
		if(pTask)
			delete pTask;
		return;
	}

	// Make sure any lower priority tasks are aborted:
	for( s32 i = iPriority; i < GetPriorityCount(); i++ )
	{
		CEventNewTask event(pTask, iPriority);
		if( m_Tasks[i] && !m_Tasks[i]->GetIsFlagSet(aiTaskFlags::IsAborted) )
		{
			if( !m_Tasks[i]->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, &event ) )
				m_Tasks[i]->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, &event );
#if __DEBUG
			Assertf(m_Tasks[i]->GetIsFlagSet(aiTaskFlags::IsAborted),"Task not aborted before a new one is added!");
#endif
		}
	}

	//If the new task is a null task then delete the current task
	//and set the current task to null.
	if(!pTask && m_Tasks[iPriority])
	{
		DeleteTask(m_Tasks[iPriority]);
		m_Tasks[iPriority]=0;
		return;
	}

	//Don't set the same task.
	if(m_Tasks[iPriority]==pTask)
	{
		return;
	} 

	if(m_Tasks[iPriority]) 
	{
		DeleteTask(m_Tasks[iPriority]);
	}
	m_Tasks[iPriority]=pTask;

	//make sure there is always a default task
	if( m_Tasks[iPriority] == NULL && iPriority == m_iDefaultPriority )
	{
		SetTask( GetDefaultTask(), m_iDefaultPriority );
	}

	m_Tasks[iPriority]->SetEntity(m_pEntity);

	// Ensure the active priority is up to date
	m_iActivePriority = ComputeActiveTaskPriority();
}

//-------------------------------------------------------------------------
// process tasks
//-------------------------------------------------------------------------
void CTaskTreePed::Process(float fTimeStep)
{
	//if there is no default main task create one.
	if(m_iDefaultPriority >= 0)//is the default priority set
	{
		if( m_Tasks[m_iDefaultPriority] == NULL )
			SetTask( GetDefaultTask(), m_iDefaultPriority);
	}

	//main tasks
	UpdateTasks( false, m_iDefaultPriority, fTimeStep);

}

//-------------------------------------------------------------------------
// Write out the current main tasks to the queriable interface
//-------------------------------------------------------------------------
void CTaskTreePed::WriteTasksToQueriableInterface( CQueriableInterface* pInterface )
{
    // we should never be writing tasks to a cloned ped!
    Assert(!(m_pPed && m_pPed->IsNetworkClone()));

	// First reset the existing task list
	pInterface->ResetTaskInfo();

	// Set if this set of tasks is running
	bool bTaskActive = true;
	bool bNonTemporaryTaskTreeFound = false;

	//Add in tasks from the physical and temporary response tasks, also the first nonp-temporary
	// slot (i.e. non-temp event response or primary or default, whichever is first
	CTask* pTask = NULL;
	for( s32 i=0; i<GetPriorityCount(); i++ )
	{
		if(m_Tasks[i])
		{
			pTask = static_cast<CTask*>(m_Tasks[i].Get());

			// Add task information in for each task
			while( pTask )
			{
				pInterface->AddTaskInformation( pTask, bTaskActive, i, m_pEntity );
				pTask = pTask->GetSubTask();

				// Only add the top task of any subsequent task priorities
				if( bNonTemporaryTaskTreeFound )
				{
					break;
				}
			}

			// Only add one full tree of non-temporary event responses
			if(m_iEventResponsePriority >= 0)
			{
				if( i > m_iEventResponsePriority )
				{
					bNonTemporaryTaskTreeFound = true;
				}
			}


			// Any task below the top level isn't active
			bTaskActive = false;
		}
	}
}



//
//
//
aiTask* CTaskTreePed::GetDefaultTask()
{
	if(m_pPed)
	{
		return m_pPed->ComputeDefaultTask(*m_pPed);
	}

	return NULL;
}
