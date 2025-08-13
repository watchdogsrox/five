// FILE :    TaskReactInDirection.h

#ifndef TASK_REACT_IN_DIRECTION_H
#define TASK_REACT_IN_DIRECTION_H

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskReactInDirection
////////////////////////////////////////////////////////////////////////////////

class CTaskReactInDirection : public CTask
{

public:

	struct Tunables : CTuning
	{
		Tunables();
	
		PAR_PARSABLE;
	};
	
public:

	enum ConfigFlags
	{
		CF_Blend360			= BIT0,
		CF_Use4Directions	= BIT1,
		CF_Use5Directions	= BIT2,
		CF_Use6Directions	= BIT3,
		CF_Interrupt		= BIT4,
	};

	enum State
	{
		State_Start = 0,
		State_React,
		State_Finish,
	};
	
public:

	explicit CTaskReactInDirection(fwMvClipSetId clipSetId, const CAITarget& rTarget, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskReactInDirection();

public:

	float						GetBlendOutDuration()	const { return m_fBlendOutDuration; }
	CDirectionHelper::Direction GetDirection()			const { return m_nDirection; }

public:

	void SetBlendOutDuration(float fValue) { m_fBlendOutDuration = fValue; }

public:

	static bool IsValid(fwMvClipSetId clipSetId, const CAITarget& rTarget);
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	bool CanInterrupt() const;
	bool HasClipEnded() const;
	void SetFlagForDirection();
	void SetFlagForNumDirections();
	void SetFlags();
	bool ShouldInterrupt() const;

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_REACT_IN_DIRECTION;
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

	CMoveNetworkHelper			m_MoveNetworkHelper;
	fwMvClipSetId				m_ClipSetId;
	CAITarget					m_Target;
	CDirectionHelper::Direction m_nDirection;
	float						m_fBlendOutDuration;
	fwFlags8					m_uConfigFlags;
	
private:

	static Tunables	sm_Tunables;

private:

	static fwMvBooleanId sm_ClipEnded;
	static fwMvBooleanId sm_Interruptible;
	static fwMvBooleanId sm_React_OnEnter;

	static fwMvFlagId sm_BackLeft;
	static fwMvFlagId sm_BackRight;
	static fwMvFlagId sm_Blend360;
	static fwMvFlagId sm_FrontLeft;
	static fwMvFlagId sm_FrontRight;
	static fwMvFlagId sm_Left;
	static fwMvFlagId sm_Right;
	static fwMvFlagId sm_Use4Directions;
	static fwMvFlagId sm_Use5Directions;
	static fwMvFlagId sm_Use6Directions;

	static fwMvFloatId sm_Phase;

	static fwMvRequestId sm_React;

};

#endif //TASK_REACT_IN_DIRECTION_H
