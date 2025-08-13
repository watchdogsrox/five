// FILE :    TaskCombatSubtask.h
// PURPOSE : Combat subtask abstract class, each subtask inherits from this to keep
//			 the interface standard
// AUTHOR :  Phil H
// CREATED : 25-06-2008


#ifndef TASK_COMBAT_SUBTASK_H


#define TASK_COMBAT_SUBTASK_H

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"

class CEntity;
class CTaskSet;
class CWeightedTaskSet;


typedef enum
{
	TSR_TargetSwitched = 0,
	TSR_RequestRestart,
	TSR_TargetHeld,
	TSR_RequstTermination
} TargetSwitchResponse;

//-------------------------------------------------------------------------
// Combat subtask abstract class, each subtask inherits from this to keep
// the interface standard
//-------------------------------------------------------------------------
class CTaskComplexCombatSubtask : public CTaskComplex
{
public:
	// Constructor
	CTaskComplexCombatSubtask( CPed* pPrimaryTarget );
	// Destructor
	virtual ~CTaskComplexCombatSubtask();
	
    virtual s32 GetTaskTypeInternal() const = 0;
    
    virtual aiTask* CreateNextSubTask(CPed* pPed)=0;
    virtual aiTask* CreateFirstSubTask(CPed* pPed);
    virtual aiTask* ControlSubTask(CPed* pPed)=0;
	virtual aiTask* Terminate(CPed* UNUSED_PARAM(pPed), CPed* UNUSED_PARAM(pTargetPed)) { return NULL; }



protected:
	// Queries the decision maker for the next combat task
	virtual s32 GetTaskFromDecisionMaker	( CPed* pPed ) = 0;
	// Sanity checks the task as decided by the decision maker and changes
	// the task if necessary
	virtual s32 OverrideTask				( CPed* pPed, s32 iDecidedTask ) = 0;
	// Creates a subtask of the given type
	virtual aiTask* CreateSubTask(s32 iSubTaskType, CPed *pPed) = 0;

	RegdPed	m_pPrimaryTarget;

	bool m_bMadeAbortable;
	bool m_bTaskFailed:1; // Notice that the task failed to succeed in whatever objective
	s32 m_iSpecificTask;

	u32 m_bEndTaskAtNextUpdate;

	unsigned int m_bHasVector;
	Vector3 m_vNextTaskRelatedVector;
};


#endif // TASK_COMBAT_SUBTASK_H
