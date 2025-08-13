//
// name:		TaskTree.h
// description:	
//
#ifndef TASK_TREE_H
#define TASK_TREE_H

// Framework headers
#include "ai/task/tasktree.h"


class CTaskTree : public aiTaskTree
{
public:

	CTaskTree(fwEntity* pEntity, const s32 iNumberOfPriorities) : aiTaskTree(pEntity, iNumberOfPriorities) {}

	// Manages the tasks attached to this task tree. If a Task is finished then its 
	// parent finds a new task and so on. If the active task finishes then the 
	// task is deleted completely.
	virtual void Process(float fTimeStep);

	// Calls ProcessPhysics on all tasks
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly);

	// Calls ProcessPostMovement on all tasks
	void ProcessPostMovement();

	// Calls ProcessPostCamera on all tasks
	void ProcessPostCamera();

	// Calls ProcessPreRender2 on all tasks
	void ProcessPreRender2();

	// Calls ProcessPostPreRender on all tasks
	void ProcessPostPreRender();

	// Calls ProcessPostPreRenderAfterAttachments on all tasks
	void ProcessPostPreRenderAfterAttachments();

	// Calls ProcessMoveSignals on all tasks
	bool ProcessMoveSignals();
};

#endif // TASK_TREE_H
