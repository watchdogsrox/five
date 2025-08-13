//
// name:		TaskManager.h
// description:	This is the class that manages the tasks given to a ped. It's
//				Process() function is called every frame for each ped. It contains
//				an array of TaskTrees
//
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

// Framework headers
#include "ai/task/taskmanager.h"
#include "fwnet/netplayer.h"

#include "TaskTree.h"

// Forward declarations
class CPed;
class CVehicle;
class CTask;
class CTaskFSMClone;
class CTaskInfo;

class CTaskManager : public aiTaskManager
{
public:

	CTaskManager(int iNumberOfTaskTreesRequired, bool treesAreExternallyOwned = false);
	virtual ~CTaskManager();

	// Calls ProcessPhysics on all tasks
	bool ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly = false);

	// Calls ProcessPostMovement on all tasks
	void ProcessPostMovement(int iTaskTree);

	// Calls ProcessPostCamera on all tasks
	void ProcessPostCamera(int iTaskTree);

	// Calls ProcessPreRender2 on all tasks
	void ProcessPreRender2(int iTaskTree);

	// Calls ProcessPostPreRender on all tasks
	void ProcessPostPreRender(int iTaskTree);

	// Calls ProcessPostPreRenderAfterAttachments on all tasks
	void ProcessPostPreRenderAfterAttachments(int iTaskTree);

	// Calls ProcessMoveSignals on all tasks
	bool ProcessMoveSignals(int iTaskTree);

#if __ASSERT
	bool HandlesRagdoll(int iTaskTree, CPed* pPed);
#endif //__ASSERT
};

///////////////////////////////////////////

class CPedTaskManager : public CTaskManager
{
public:

	CPedTaskManager(CPed* pPed, int iNumberOfTaskTreesRequired);
	virtual ~CPedTaskManager();

protected:

	virtual void OnTaskChanged();

private:
	
	CPed* m_pPed;
};

///////////////////////////////////////////

class CVehicleTaskManager : public CTaskManager
{
public:

	CVehicleTaskManager(CVehicle* pVehicle);
	virtual ~CVehicleTaskManager();

protected:

	virtual void OnTaskChanged();

private:
	
	CTaskTree m_PrimaryTaskTree;
	CTaskTree m_SecondaryTaskTree;
	CVehicle* m_pVehicle;
};

#endif
