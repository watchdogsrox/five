//
// name:		TaskTreeClone.h
// description:	
//
#ifndef TASK_TREE_CLONE_H
#define TASK_TREE_CLONE_H

// Game headers
#include "Task/System/TaskTree.h"
#include "Peds/PedDefines.h"

namespace rage
{
    class netPlayer;
}

class CTask;
class CTaskFSMClone;
class CTaskInfo;

class CTaskTreeClone : public CTaskTree
{
public:

    CTaskTreeClone(fwEntity* pEntity, const s32 iNumberOfPriorities);

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly);

    void RecalculateTasks();
	void UpdateTaskSpecificData();

    bool NetworkAttachmentIsOverridden()     const { return m_OverridingNetworkAttachment; }
	bool NetworkBlenderIsOverridden()        const { return m_OverridingNetworkBlender; }
	bool NetworkHeadingBlenderIsOverridden() const { return m_OverridingNetworkHeadingBlender; }
    bool NetworkCollisionIsOverridden()      const { return m_OverridingNetworkCollision; }
    bool NetworkLocalHitReactionsAllowed()   const { return m_LocalHitReactionsAllowed; }
    bool NetworkUseInAirBlender()            const { return m_UseInAirBlender; }

	bool ControlPassingAllowed(const netPlayer& player, eMigrationType migrationType);

    //PURPOSE
    // Creates a clone task of the relevant type. This will only be done
    // if the queriable interface for the ped contains a task info of the
    // specified type that has not already been used to create a clone task
    CTaskFSMClone *CreateCloneTaskForTaskType(u32 taskType);

    //TO BE REMOVED - this function needs to go - but requires too many changes to other clone tasks to do just now
	CTaskFSMClone *CreateCloneTaskFromInfo(CTaskInfo *taskInfo);

	// Starts a clone task locally
	void StartLocalCloneTask(CTaskFSMClone* pTask, u32 const iPriority = PED_TASK_PRIORITY_DEFAULT);

	CTask* GetActiveCloneTask();

	bool HasPendingLocalCloneTask() const { return (m_PendingLocalCloneTask.Get() != NULL); }

	// PURPOSE: returns true if the tree is a clone tree
	virtual const bool IsCloneTree() const { return true; }
private:

    void UpdateTasks(const bool bDontDeleteHeadTask, const s32 iDefaultTaskPriority, float fTimeStep);

    aiTaskTree::UpdateResult UpdateTask(aiTask* pTask, float fTimeStep);

    void OnNewSubTaskCreated(aiTask* pTask, aiTask* pSubTask);

    CTaskFSMClone *CreateCloneTaskFromTaskInfo(CTaskInfo *taskInfo, bool bIgnoreSequences=false);
    bool           m_UpdatingTasks;
    bool           m_OverridingNetworkAttachment;
    bool           m_OverridingNetworkBlender;
	bool           m_OverridingNetworkHeadingBlender;
    bool           m_OverridingNetworkCollision;
    bool           m_LocalHitReactionsAllowed;
    bool           m_UseInAirBlender;
    taskRegdRef<CTaskFSMClone> m_PendingLocalCloneTask;
};

#endif // TASK_TREE_CLONE_H
