#ifndef INC_TASK_MOVE_BETWEEN_POINTS_SCENARIO_H
#define INC_TASK_MOVE_BETWEEN_POINTS_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"

class CTaskMoveBetweenPointsScenario : public CTaskScenario
{
public:
	typedef enum
	{
		State_Stationary = 0,
		State_SearchingForNewPoint,
		State_MovingToNewPoint,
		State_Finish
	} MoveBetweenPointsState;

	CTaskMoveBetweenPointsScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint );
	~CTaskMoveBetweenPointsScenario();
	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO;}

	virtual bool	IsTaskValidForChatting() const		{ return false; }

	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);
	// Task updates that are called before the FSM update
	virtual FSM_Return			ProcessPreFSM	();

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const {return State_Stationary;}

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual u32					GetAmbientFlags();

protected:
	// State Functions
	void						Start_OnEnter (CPed* pPed);
	FSM_Return					Start_OnUpdate (CPed* pPed);

	void						Stationary_OnEnter (CPed* pPed);
	FSM_Return					Stationary_OnUpdate (CPed* pPed);

	void						SearchingForNewPoint_OnEnter (CPed* pPed);
	FSM_Return					SearchingForNewPoint_OnUpdate (CPed* pPed);

	void						MovingToNewPoint_OnEnter (CPed* pPed);
	FSM_Return					MovingToNewPoint_OnUpdate (CPed* pPed);

	FSM_Return					Finish_OnUpdate (CPed* pPed);

private:

	float					m_fTimer;
	TPathHandle				m_hPathHandle;
	Vector3					m_vNewPointPos;
};

#endif//!INC_TASK_MOVE_BETWEEN_POINTS_SCENARIO_H