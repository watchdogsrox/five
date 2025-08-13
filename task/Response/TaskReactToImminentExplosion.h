// FILE :    TaskReactToImminentExplosion.h

#ifndef TASK_REACT_TO_IMMINENT_EXPLOSION_H
#define TASK_REACT_TO_IMMINENT_EXPLOSION_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToImminentExplosion
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToImminentExplosion : public CTask
{

private:

	enum State
	{
		State_Start = 0,
		State_Flinch,
		State_Escape,
		State_Dive,
		State_Finish
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_MaxEscapeDistance;
		float m_MaxFlinchDistance;

		PAR_PARSABLE;
	};

public:

	explicit CTaskReactToImminentExplosion(const CAITarget& rExplosionTarget, float fRadius, u32 uTimeOfExplosion);
	virtual ~CTaskReactToImminentExplosion();

public:

	void SetAimTarget(const CAITarget& rTarget) { m_AimTarget = rTarget; }

public:

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

public:

	static void ResetLastTimeEscapeUsedOnScreen() { s_uLastTimeEscapeUsedOnScreen = 0; }

public:

	static bool IsValid(const CPed& rPed, const CAITarget& rExplosionTarget, float fRadius, const CAITarget& rAimTarget = CAITarget(), bool bIgnoreCover = false);

private:

	bool MustTargetBeValid() const;
	void ProcessTimeOfExplosion();

private:

	static bool	CanEscape(const CPed& rPed);
	static bool	CanFlinch(const CPed& rPed, const CAITarget& rExplosionTarget, const CAITarget& rAimTarget);
	static s32	ChooseState(const CPed& rPed, const CAITarget& rExplosionTarget, float fRadius, const CAITarget& rAimTarget);

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_REACT_TO_IMMINENT_EXPLOSION;
	}

private:

	virtual aiTask*		Copy() const;
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Flinch_OnEnter();
	FSM_Return	Flinch_OnUpdate();
	void		Escape_OnEnter();
	FSM_Return	Escape_OnUpdate();
	void		Dive_OnEnter();
	FSM_Return	Dive_OnUpdate();

private:

	CAITarget	m_ExplosionTarget;
	CAITarget	m_AimTarget;
	float		m_fRadius;
	u32			m_uTimeOfExplosion;

private:

	static u32		s_uLastTimeEscapeUsedOnScreen;
	static Tunables	sm_Tunables;

};

#endif //TASK_REACT_TO_IMMINENT_EXPLOSION_H
