// FILE :    TaskReactToBuddyShot.h

#ifndef TASK_REACT_TO_BUDDY_SHOT_H
#define TASK_REACT_TO_BUDDY_SHOT_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "ai/AITarget.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToBuddyShot
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToBuddyShot : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_React,
		State_Finish,
	};

private:

	enum RunningFlags
	{
		RF_HasProcessedDirection = BIT0,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();
	
		PAR_PARSABLE;
	};
	
public:

	explicit CTaskReactToBuddyShot(const CAITarget& rReactTarget, const CAITarget& rAimTarget);
	virtual ~CTaskReactToBuddyShot();

public:

	static void ResetLastTimeUsedOnScreen() { s_uLastTimeUsedOnScreen = 0; }

public:

	static fwMvClipSetId	ChooseClipSet(const CPed& rPed);
	static bool				HasBeenReactedTo(const CPed& rPed, const CPed& rTarget);
	static bool				IsValid(const CPed& rPed, const CAITarget& rReactTarget, const CAITarget& rAimTarget);
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	const CPed* GetTargetPed() const;
	bool		MustTargetBeValid() const;

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_REACT_TO_BUDDY_SHOT;
	}
	
private:

	virtual	void		CleanUp();
	virtual aiTask*		Copy() const;
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		React_OnEnter();
	FSM_Return	React_OnUpdate();

private:

	CAITarget	m_ReactTarget;
	CAITarget	m_AimTarget;
	fwFlags8	m_uRunningFlags;
	
private:

	static u32		s_uLastTimeUsedOnScreen;
	static Tunables	sm_Tunables;

};

#endif //TASK_REACT_TO_BUDDY_SHOT_H
