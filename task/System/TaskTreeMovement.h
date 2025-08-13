//
// name:		TaskTreeMovement.h
// description:	
//
#ifndef TASK_TREE_MOVEMENT_H
#define TASK_TREE_MOVEMENT_H

#include "Debug/Debug.h"
#include "Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskManager.h"
#include "Task/System/TaskTree.h"
#include "Task/System/TaskTreePed.h"


class CPed;
class CTask;
class CTaskSimple;
class CEvent;

class CTaskTreeMovement : public CTaskTreePed
{
public:
	CTaskTreeMovement(s32 iNumberOfPriorities, CPed *pPed, s32 iMovementGeneralPriority, s32 iEventResponsePriority, s32 iDefaultPriority) : 
		CTaskTreePed( iNumberOfPriorities, pPed, iEventResponsePriority, iDefaultPriority),
		m_iMovementGeneralPriority(iMovementGeneralPriority)
	{
	}

	// Manages the tasks attached to this task tree. 
	void Process(float fTimeStep);

	//custom SetTask for MovementTaskTree
	void SetTask(aiTask* pTaskPrimary, const int iTaskPriority, const bool bForceNewTask = false);

	// Returns the active movement task. The Active task is the task with the highest
	// priority with event response being the highest priority
	inline aiTask* GetActiveTask() const;

	// Find task within active tasks, task chain
	aiTask* FindTaskByTypeActive(const int iTaskType) const;

private:
	s32 m_iMovementGeneralPriority;

	void ProcessMoveTasksCheckedThisFrame();
};


//-------------------------------------------------------------------------
// Gets the current active movement task
//-------------------------------------------------------------------------
aiTask* CTaskTreeMovement::GetActiveTask() const
{
	int i;
	for(i=0;i<GetPriorityCount();i++)
	{
		if(m_Tasks[i]) 
		{
			if( m_Tasks[i]->GetIsFlagSet(aiTaskFlags::HasFinished) )
			{
				// JB : (18/5/06) Changed this to prevent the single frame of PEDMOVE_NONE, when GetMoveStateFromGoToTask()
				// calls this function and this slot's movement task has just finished.
				//return NULL;
				continue;
			}
			return m_Tasks[i];
		}
	}
	return NULL;
}

#endif
