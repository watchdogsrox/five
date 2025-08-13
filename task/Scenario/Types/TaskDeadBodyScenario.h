#ifndef TASK_DEAD_BODY_SCENARIO_H
#define TASK_DEAD_BODY_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"

//*************************************************************************************
//Class:  CTaskDeadBodyScenario
//Purpose:  Kill the ped in a way for them to be spawned as a dead body in the world.
//*************************************************************************************

class CTaskDeadBodyScenario : public CTaskScenario
{
public:
	CTaskDeadBodyScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint );
	~CTaskDeadBodyScenario();
	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_DEAD_BODY_SCENARIO;}

	virtual bool	IsTaskValidForChatting() const		{ return false; }

	enum
	{
		State_Setup = 0,
		State_Finish
	};

	// Basic FSM implementation
	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const	{return State_Finish;}

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 iState);
#endif

private:

	// FSM State Functions
	void						Setup_OnEnter();
	FSM_Return					Setup_OnUpdate();

	bool m_bHasSentKillEvent;
};

#endif //!TASK_DEAD_BODY_SCENARIO_H