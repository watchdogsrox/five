#ifndef OBJECT_INTELLIGENCE_H
#define OBJECT_INTELLIGENCE_H

// Framework includes
#include "fwtl/Pool.h"

// Game includes
#include "task/System/Task.h"
#include "task/System/TaskManager.h"

// Forward declarations
class CObject;


class CObjectIntelligence
{
public:

	//Task tree enums
	enum eObjectTaskTrees
	{
		OBJECT_TASK_TREE_PRIMARY		= 0,
		OBJECT_TASK_TREE_SECONDARY		= 1,
		OBJECT_TASK_TREE_MAX
	};

	// enum for task priorities
	enum eObjectTaskPrimaryPriorities
	{
		OBJECT_TASK_PRIORITY_PRIMARY	= 0,
		OBJECT_TASK_PRIORITY_MAX
	};

	enum eObjectTaskSecondaryPriorities
	{
		OBJECT_TASK_SECONDARY_ANIM		= 0,
		OBJECT_TASK_SECONDARY_MAX
	};

public:
	FW_REGISTER_CLASS_POOL(CObjectIntelligence);

							CObjectIntelligence(CObject* pObject);
	virtual					~CObjectIntelligence();
	void					Init();

	// -----------------------------------------------------------------------------
	// TASK MANAGER INTERFACE
	CTaskManager*			GetTaskManager()						{ return(&m_taskManager); }
	const CTaskManager*		GetTaskManager() const					{ return(&m_taskManager); }

	// CLEAR TASKS AND RESPONSES ---------------------------------
	void					ClearTasks() { m_taskManager.ClearTask(OBJECT_TASK_TREE_PRIMARY, OBJECT_TASK_PRIORITY_PRIMARY); }

	// ADD TASKS --------------------------------------------------
	void					SetTask(aiTask* pTask, u32 const tree = OBJECT_TASK_TREE_PRIMARY, u32 const priority = OBJECT_TASK_PRIORITY_PRIMARY);

	// QUERY/GET TASKS IN SLOTS -----------------------------------
	// Get all the types of task.
	s32						GetActiveTaskPriority(u32 const tree = OBJECT_TASK_TREE_PRIMARY) const;
	CTask*					GetTaskActive(u32 const tree = OBJECT_TASK_TREE_PRIMARY) const;					//Get the task that is being processed.
	CTask*					GetTaskActiveSimplest(u32 const tree = OBJECT_TASK_TREE_PRIMARY) const;			//Get the leaf of the task chain of the task that is being processed.
	CTask*					FindTaskByType(const int iType) const;
	CTask*					FindTaskSecondaryByType(const int iType) const;
	// -----------------------------------------------------------------------------
	// CORE UPDATE FUNCTIONS
	void					Process();											// main intelligence process
	bool					ProcessPhysics(float fTimeStep, int nTimeSlice);	// called for each physics update timeslice, so simple tasks can continuously apply forces, returns true if task wants to keep object awake
	void					ProcessPostMovement();								// needs processed AFTER all movement (physics and then script)
	void					ProcessPostCamera();								// needs processed AFTER camera update
	void					ForcePostCameraTaskUpdate() { m_bForcePostCameraTaskUpdate = true; }

	// -----------------------------------------------------------------------------
	// ACCESSORS
	const CObject*			GetObject() const { return m_pObject; }

private:
	void					Process_Tasks();
#if __DEV
	void					Process_Debug();
	void					PrintTasks();
#endif // __DEV

private:

	CObject*				m_pObject;
	CTaskManager			m_taskManager;
	bool					m_bForcePostCameraTaskUpdate;
};

#if __DEV
// Forward declare pool full callback so we don't get two versions of it
namespace rage { template<> void fwPool<CObjectIntelligence>::PoolFullCallback(); }
#endif // __DEV

//
// Inline functions
//

inline s32 CObjectIntelligence::GetActiveTaskPriority(u32 const tree) const
{
	return m_taskManager.GetActiveTaskPriority(tree);
}

inline CTask* CObjectIntelligence::GetTaskActive(u32 const tree) const
{
	aiTask *pTask = m_taskManager.GetActiveTask(tree);
	return static_cast<CTask*>(pTask);
}

inline CTask* CObjectIntelligence::GetTaskActiveSimplest(u32 const tree) const
{
	aiTask *pTask = m_taskManager.GetActiveLeafTask(tree);
	return static_cast<CTask*>(pTask);
}

inline CTask* CObjectIntelligence::FindTaskByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(OBJECT_TASK_TREE_PRIMARY, iType, OBJECT_TASK_PRIORITY_PRIMARY);
	return static_cast<CTask*>(pTask);
}

inline CTask* CObjectIntelligence::FindTaskSecondaryByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(OBJECT_TASK_TREE_SECONDARY, iType, OBJECT_TASK_SECONDARY_ANIM);
	return static_cast<CTask*>(pTask);
}




#endif // OBJECT_INTELLIGENCE_H
