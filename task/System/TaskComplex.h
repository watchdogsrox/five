//
// filename:	TaskCOMPLEX.h
// description:	Base class for all complex tasks. Complex tasks don't control the ped 
//				directly, so there is no ProcessPed function for complex tasks. A complex 
//				task can generate a simple task to control the ped or create another 
//				complex task which might then generate a simple or complex task and so on.
//				But the final task should always be a simple task.
#ifndef TASK_COMPLEX_H
#define TASK_COMPLEX_H

// Game headers
#include "Task/System/Task.h"


//
// name:		CTaskComplex
// description:	Base class for all complex tasks. Complex tasks don't control the ped 
//				directly, so there is no ProcessPed function for complex tasks. A complex 
//				task can generate a simple task to control the ped or create another 
//				complex task which might then generate a simple or complex task and so on.
//				But the final task should always be a simple task.
class CTaskComplex : public CTask
{
private:
	// NOTE:
	//	If you change our base class, change this type def.  That way the majority of 
	//  downward calls will change automatically
	typedef CTask			tBaseClass;

protected:
	enum
	{
		State_CreateFirstSubTask,
		State_ControlSubTask,
		State_CreateNextSubTask,
		State_Quit,
	};

public:

	CTaskComplex();
	virtual ~CTaskComplex();

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual void CleanUp();

	// Return that this is not a simple task
	virtual bool IsSimpleTask() const {return false;}

	// Returns the next child task. This is called if the previous child task has 
	// finished. It uses the previous child task to make a decision on what the next 
	// task should be
	virtual aiTask*	CreateNextSubTask(CPed* pPed)=0;
	// Returns the first child task. The first time this task is processed it creates a 
	// child task. Although complex tasks can create other complex tasks as children. The
	// last task should always be a simple task. Returns NULL when the complex task has
	// finished.
	virtual aiTask*	CreateFirstSubTask(CPed* pPed)=0;
	// In some cases the complex task might want to alter the child tasks it has generated.
	// This function is called every frame for complex tasks and allows them to alter or 
	// remove child tasks. It returns a pointer to a task. If this pointer is different
	// to the current child task then the new task is set as the child task. NB IT IS VERY
	// IMPORTANT NOT TO RETURN NEW TASKS IF THE CURRENT CHILD TASK HASN'T ABORTED
	// COMPLETELY.
	virtual aiTask* ControlSubTask(CPed* pPed)=0;

	// Search sub tasks for the first instance of the specified task type
	virtual bool	IsValidForMotionTask(CTaskMotionBase& pTask) const;

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	s32				GetDefaultStateAfterAbort() const {return State_Quit;}

	virtual bool MayDeleteSubTaskOnAbort() const {return GetDefaultStateAfterAbort() == State_Quit;}	// If a subclass has support for resuming, we probably shouldn't let the subtask be deleted, since it's usually needed by CreateNextSubTask(), etc.
	virtual bool MayDeleteOnAbort() const {return GetDefaultStateAfterAbort() == State_Quit;}			// Unless a subclass says it wants to resume, we can delete this task.

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// Basic FSM implementation, for the inherited class
	FSM_Return	UpdateFSM(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent));

	FSM_Return	StateCreateFirstSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateCreateFirstSubTask_OnUpdate(CPed* pPed);

	FSM_Return	StateControlSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateControlSubTask_OnUpdate(CPed* pPed);

	FSM_Return	StateCreateNextSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateCreateNextSubTask_OnUpdate(CPed* pPed);

private:

	RegdaiTask m_pNewTaskToSet;
};


#endif // TASK_COMPLEX_H
