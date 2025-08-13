// FILE :    TaskDraggedToSafety.h
// PURPOSE : Combat subtask in control of a ped being dragged to safety
// CREATED : 08-29-20011

#ifndef TASK_DRAGGED_TO_SAFETY_H
#define TASK_DRAGGED_TO_SAFETY_H

// Game headers
#include "Task/System/Task.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskDraggedToSafety
////////////////////////////////////////////////////////////////////////////////

class CTaskDraggedToSafety : public CTask
{

public:

	enum DraggedToSafetyState
	{
		State_Start = 0,
		State_Pickup,
		State_Dragged,
		State_Putdown,
		State_Finish
	};
	
public:

	CTaskDraggedToSafety(CPed* pDragger);
	~CTaskDraggedToSafety();
	
public:
	
	virtual	void CleanUp();

	virtual aiTask*	Copy() const { return rage_new CTaskDraggedToSafety(GetDragger()); }
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_DRAGGED_TO_SAFETY; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }
	
	// PURPOSE: Called by ped event scanner when checking dead peds tasks if this returns true a damage event is not generated if a dead ped runs this task
	virtual bool HandlesDeadPed() { return true; }
	virtual bool HandlesRagdoll(const CPed*) const { return true; }
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	CPed* GetDragger() const { return m_pDragger; }
	
private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	void		Pickup_OnEnter();
	FSM_Return	Pickup_OnUpdate();
	void		Pickup_OnExit();
	void		Dragged_OnEnter();
	FSM_Return	Dragged_OnUpdate();
	void		Dragged_OnExit();
	void		Putdown_OnEnter();
	FSM_Return	Putdown_OnUpdate();
	
private:
	
	void		BlendFromNMToAnimation(const fwMvClipSetId clipSetId, const fwMvClipId& clipId, const Vector3& vOffset);
	fwMvClipId	ChoosePickupClip() const;
	void		SyncState();
	
private:

	RegdPed	m_pDragger;
	bool	m_bWasRunningInjuredOnGroundBehavior;
		
};

#endif //TASK_DRAGGED_TO_SAFETY_H