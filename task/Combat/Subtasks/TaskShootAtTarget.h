// FILE :    TaskShootAtTarget.h
// PURPOSE : Combat subtask in control of weapon firing
// CREATED : 08-10-2009

#ifndef TASK_SHOOT_AT_TARGET_H
#define TASK_SHOOT_AT_TARGET_H

// Game headers
#include "AI/AITarget.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"

class CTaskShootAtTarget : public CTask
{
public:
	enum ShootState
	{
		State_Start = 0,
		State_ShootAtTarget,
		State_AimAtTarget,

		State_Finish
	};

	enum ShootFlags
	{
		Flag_aimOnly = BIT0,
		Flag_taskTimed = BIT1
	};

	CTaskShootAtTarget(const CAITarget& target, const s32 iFlags, float fTaskTimer = -1.0f);
	~CTaskShootAtTarget();

	virtual aiTask*	Copy() const
	{
		CTaskShootAtTarget* pTask = rage_new CTaskShootAtTarget(m_target, m_iFlags.GetAllFlags(), m_fTaskTimer);
		pTask->SetFiringPatternHash(m_uFiringPatternHash);
		return pTask;
	}
	virtual s32	GetTaskTypeInternal() const {return CTaskTypes::TASK_SHOOT_AT_TARGET;}
	virtual s32	GetDefaultStateAfterAbort()	const {return State_Finish;}
	virtual bool MayDeleteOnAbort() const { return true; }	// This task doesn't resume, OK to delete if aborted.

	void SetFiringPatternHash(u32 uFiringPatternHash) { m_uFiringPatternHash = uFiringPatternHash; }
	void SetTarget(const CAITarget& target);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
private:
	// Start
	FSM_Return					Start_OnUpdate				(CPed* pPed);
	// Fire when standing
	void						ShootAtTarget_OnEnter		(CPed* pPed);
	FSM_Return					ShootAtTarget_OnUpdate		(CPed* pPed);
	// Aim at target
	void						AimAtTarget_OnEnter			(CPed* pPed);
	FSM_Return					AimAtTarget_OnUpdate		(CPed* pPed);

	ShootState					ChooseState					(CPed* pPed);

	CAITarget	m_target;
	RegdEnt		m_pAlternateTarget;
	fwFlags32	m_iFlags;
	float		m_fTaskTimer;
	u32			m_uFiringPatternHash;
};



#endif //TASK_SHOOT_AT_TARGET_H

