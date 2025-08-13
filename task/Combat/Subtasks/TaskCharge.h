// FILE : TaskCharge.h
//
// AUTHORS : Deryck Morales
//

#ifndef TASK_CHARGE_H
#define TASK_CHARGE_H

// Rage headers
#include "atl/array.h"
#include "fwutil/Flags.h"

// Game headers
#include "ai/AITarget.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"

//=========================================================================
// CTaskCharge
//=========================================================================

class CTaskCharge : public CTask
{
public:

	enum State
	{
		State_Start = 0,
		State_Charge,
		State_Finish,
	};

	CTaskCharge(Vec3V_In vChargeGoalPos, Vec3V_In vTargetInitialPos, float fCompletionRadius);
	virtual ~CTaskCharge();

	void SetSubTaskToUseDuringMovement(CTask* pTask);

	const Vec3V GetTargetInitialPosition() const { return m_vTargetInitialPosition; }

#if !__FINAL
	void Debug() const;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_CHARGE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Start; }

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return	Start_OnUpdate();
	void		Charge_OnEnter();
	FSM_Return	Charge_OnUpdate();
	void		Charge_OnExit();

	void ClearSubTaskToUseDuringMovement();
	CTask*	CreateSubTaskToUseDuringMovement() const;
	
	Vec3V		m_vChargeGoalPosition; // Position to charge to
	Vec3V		m_vTargetInitialPosition; // Stores the target's position when we started charging
	RegdTask	m_pSubTaskToUseDuringMovement;
	float		m_fCompletionRadius;
	bool		m_bPrev_CanShootWithoutLOS;
};

#endif // TASK_CHARGE_H
