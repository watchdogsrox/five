#ifndef INC_TASK_WANDERING_IN_RADIUS_SCENARIO_H
#define INC_TASK_WANDERING_IN_RADIUS_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"

class CTaskWanderingInRadiusScenario : public CTaskScenario
{
public:
	CTaskWanderingInRadiusScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint );
	~CTaskWanderingInRadiusScenario();
	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO;}

	virtual bool	IsTaskValidForChatting() const		{ return false; }

	// Basic FSM implementation
	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const	{return State_Wandering;}

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual u32				GetAmbientFlags();

	typedef enum
	{
		State_Wandering = 0,
		State_Finish
	} WanderingScenarioState;
};

#endif//!INC_TASK_WANDERING_IN_RADIUS_SCENARIO_H