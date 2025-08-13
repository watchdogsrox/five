// FILE :    TaskArmy.h

#ifndef INC_TASK_ARMY_H
#define INC_TASK_ARMY_H

// Game headers
#include "Task/Default/TaskUnalerted.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

// Game forward declarations

////////////////////////////////////////////////////////////////////////////////
// CTaskArmy
////////////////////////////////////////////////////////////////////////////////

class CTaskArmy : public CTask
{

public:

	enum
	{
		State_Start = 0,
		State_Unalerted,
		State_WanderInVehicle,
		State_PassengerInVehicle,
		State_RespondToPoliceOrder,
		State_RespondToSwatOrder,
		State_Combat,
		State_Finish,
	};

public:

	CTaskArmy(CTaskUnalerted* pUnalertedTask = NULL);
	virtual ~CTaskArmy();

	CTaskUnalerted* GetTaskUnalerted() { return m_pTaskUnalerted && m_pTaskUnalerted->GetTaskType() == CTaskTypes::TASK_UNALERTED ? static_cast<CTaskUnalerted*>(m_pTaskUnalerted.Get()) : NULL; }

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	bool	CheckForOrder();
	CPed*	GetTargetForCombat() const;

private:

	virtual s32	GetDefaultStateAfterAbort() const
	{
		return (GetState() == State_Unalerted) ? State_Unalerted : State_Start;
	}

	virtual int GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_ARMY;
	}

private:

	virtual aiTask*			Copy() const;
	virtual	CScenarioPoint*	GetScenarioPoint() const;
	virtual int				GetScenarioPointType() const;

private:
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool ProcessMoveSignals();

private:

	FSM_Return	 Start_OnUpdate();
	void		 Unalerted_OnEnter();
	FSM_Return	 Unalerted_OnUpdate();
	void		 Unalerted_OnExit();
	void		 WanderInVehicle_OnEnter();
	FSM_Return	 WanderInVehicle_OnUpdate();
	void		 PassengerInVehicle_OnEnter();
	FSM_Return	 PassengerInVehicle_OnUpdate();
	void		 RespondToPoliceOrder_OnEnter();
	FSM_Return	 RespondToPoliceOrder_OnUpdate();
	void		 RespondToSwatOrder_OnEnter();
	FSM_Return	 RespondToSwatOrder_OnUpdate();
	void		 Combat_OnEnter();
	FSM_Return	 Combat_OnUpdate();

private:

	RegdTask m_pTaskUnalerted;

};

#endif // INC_TASK_ARMY_H
