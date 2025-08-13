/////////////////////////////////////////////////////////////////////////////////
// FILE :		TaskScenarioAnimDebug.h
// PURPOSE :	Task used for debugging scenario animations
// AUTHOR :		Jason Jurecka.
// CREATED :	4/30/2012
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_TASK_SCENARIO_ANIM_DEBUG_H
#define INC_TASK_SCENARIO_ANIM_DEBUG_H

// framework headers

// game includes
#include "task/System/Task.h"

#if __BANK
// Forward declarations
class CScenarioAnimDebugHelper;

class CTaskScenarioAnimDebug : public CTask
{
public:

	enum
	{
		State_WaitForAnimQueueStart,
		State_RequestQueueAnims,
		State_ProcessAnimQueue,
	};

	CTaskScenarioAnimDebug(CScenarioAnimDebugHelper& helper);
	virtual ~CTaskScenarioAnimDebug();

	virtual CTask*		Copy() const { return rage_new CTaskScenarioAnimDebug(*m_ScenarioAnimDebugHelper); }
	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_SCENARIO_ANIM_DEBUG; }
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32			GetDefaultStateAfterAbort() const { return State_WaitForAnimQueueStart; }

	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_WaitForAnimQueueStart : return "WaitForAnimQueueStart";
		case State_RequestQueueAnims : return "RequestQueueAnims";
		case State_ProcessAnimQueue : return "ProcessAnimQueue";
		default: Assert(0); return NULL;
		}
	}

protected:

	FSM_Return StateWaitForAnimQueueStart_OnUpdate();
	FSM_Return StateRequestQueueAnims_OnEnter();
	FSM_Return StateRequestQueueAnims_OnUpdate();
	FSM_Return StateProcessAnimQueue_OnEnter();
	FSM_Return StateProcessAnimQueue_OnUpdate();

	// PURPOSE:	Helper function to make the ped stand still, shared between states.
	void DoStandStill();
	void StartUseScenario();

	// Clip requester helper object
	static const int NUM_CLIP_SETS = 10;
	CMultipleClipSetRequestHelper<NUM_CLIP_SETS> m_RequestHelper;
	CScenarioAnimDebugHelper* m_ScenarioAnimDebugHelper;
	bool m_UsedMoveExit;
};

#endif // __BANK

#endif // INC_TASK_SCENARIO_ANIM_DEBUG_H
