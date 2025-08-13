//
// name:		TaskTreePed.h
// description:	
//
#ifndef TASK_TREE_PED_H
#define TASK_TREE_PED_H

#include "Debug/Debug.h"
#include "Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskManager.h"
#include "Task/System/TaskTree.h"


class CPed;
class CTask;
class CTaskSimple;
class CEvent;
class CQueriableInterface;
class CTaskFSMClone;

class CTaskTreePed : public CTaskTree
{
public:
	CTaskTreePed(s32 iNumberOfPriorities, fwEntity *pPed, s32 iEventResponsePriority, s32 iDefaultPriority = -1);
	virtual ~CTaskTreePed()
	{
	}

	// Sets a new task in a specified slot. It takes as parameters the task you want
	// to set, the task priority (slot) and if you want to force the task to be set.
	// This function will delete the previous task. If NULL is sent as the new
	// task then this function just deletes the old task. If the task is the same
	// type then the new task is ignored unless bForceNewTask is set to true.
	// NB A PED HAS TO HAVE A DEFAULT TASK. The game will assert if 
	// SetTask deletes the default task but doesn't replace it.
	virtual void SetTask(aiTask* pTaskPrimary, const int iTaskPriority, const bool bForceNewTask = false);

	// Manages the tasks attached to this task tree. If a Task is finished then its 
	// parent finds a new task and so on. If the active task finishes then the 
	// task is deleted completely.
	virtual void Process(float fTimeStep);

	// Write out the current main tasks to the queriable interface
	void WriteTasksToQueriableInterface( CQueriableInterface* pInterface );

protected:
	s32 m_iDefaultPriority; //default task priority, no default if set to -1
	s32 m_iEventResponsePriority; //event response priority, ignore if set to -1

	CPed *m_pPed;

	//returns the default task for m_pEntity
	aiTask* GetDefaultTask();
};

#endif
