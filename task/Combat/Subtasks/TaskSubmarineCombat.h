#ifndef TASK_SUBMARINE_COMBAT_H
#define TASK_SUBMARINE_COMBAT_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CSubmarine;

//=========================================================================
// CTaskSubmarineCombat
//=========================================================================

class CTaskSubmarineCombat : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Chase,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();

		PAR_PARSABLE;
	};

	CTaskSubmarineCombat(const CAITarget& rTarget);
	virtual ~CTaskSubmarineCombat();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	Vec3V_Out		CalculateTargetOffsetForChase() const;
	Vec3V_Out		CalculateTargetVelocity() const;
	CSubmarine*		GetSub() const;
	const CEntity*	GetTargetEntity() const;
	const CPed*		GetTargetPed() const;
	CVehicle*		GetTargetVehicle() const;
	bool			ShouldChase() const;

	void			UpdateTargetOffsetForChase();

private:

	virtual aiTask* Copy() const { return rage_new CTaskSubmarineCombat(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SUBMARINE_COMBAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Chase_OnEnter();
	FSM_Return	Chase_OnUpdate();

private:

	CAITarget m_Target;
	CTaskTimer m_ChangeStateTimer;

private:

	static Tunables	sm_Tunables;

};

#endif
