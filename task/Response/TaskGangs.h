#ifndef TASK_GANGS_H
#define	TASK_GANGS_H

//Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"

class CTaskGangPatrol : public CTask
{
public:
	// FSM states
	enum 
	{
		State_Start,
		State_AttackTarget,
		State_EnterVehicle,
		State_WaitingForVehicleOccupants,
		State_Finish,
	};

	CTaskGangPatrol(CPed* pPrimaryTarget);
	virtual ~CTaskGangPatrol();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskGangPatrol(m_pPrimaryTarget); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_GANG_PATROL; }

	// FSM required implementations

	virtual bool ProcessMoveSignals();
	virtual FSM_Return UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort() const { return State_Start; }

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	FSM_Return	Start_OnUpdate();
	void		AttackTarget_OnEnter();
	FSM_Return	AttackTarget_OnUpdate();
	void		EnterVehicle_OnEnter();
	FSM_Return	EnterVehicle_OnUpdate();
	void		WaitingForVehicleOccupants_OnEnter();
	FSM_Return	WaitingForVehicleOccupants_OnUpdate();

private:

	bool IsDriverDead() const;
	bool IsLastUsedVehicleClose() const;
	bool ShouldWaitForVehicleOccupant(const CPed& rPed) const;
	bool ShouldWaitForVehicleOccupants() const;

private:

	RegdPed m_pPrimaryTarget;

};

#endif // TASK_GANGS_H
