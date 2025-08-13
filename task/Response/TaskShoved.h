// FILE :    TaskShoved.h

#ifndef TASK_SHOVED_H
#define TASK_SHOVED_H

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskShoved
////////////////////////////////////////////////////////////////////////////////

class CTaskShoved : public CTask
{

public:

	struct Tunables : CTuning
	{
		Tunables();
	
		PAR_PARSABLE;
	};
	
public:

	enum State
	{
		State_Start = 0,
		State_React,
		State_Finish,
	};
	
public:

	explicit CTaskShoved(const CPed* pShover);
	virtual ~CTaskShoved();

public:

	static bool IsValid();
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	bool		CanInterrupt() const;
	fwMvClipId	ChooseClip() const;
	bool		HasLocalPlayerMadeInput() const;
	bool		ShouldInterrupt() const;

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_SHOVED;
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
	void		React_OnExit();

private:

	RegdConstPed m_pShover;
	
private:

	static Tunables	sm_Tunables;

};

#endif //TASK_SHOVED_H
