//
// name:		TaskTreeMotion.h
// description:	
//
#ifndef TASK_TREE_MOTION_H
#define TASK_TREE_MOTION_H

// Game headers
#include "Task/System/TaskTree.h"

namespace rage
{
    class netPlayer;
}

class CTaskTreeMotion : public CTaskTree
{
public:

    CTaskTreeMotion(fwEntity* pEntity, const s32 iNumberOfPriorities);

	virtual void SetTask(aiTask* pTask, const s32 iPriority, const bool bForceNewTask = false);

private:

    virtual void OnNewSubTaskCreated(aiTask* pTask, aiTask* pSubTask);
	virtual void OnTaskDeleted(aiTask* UNUSED_PARAM(pTask));
};

#endif // TASK_TREE_MOTION_H
