#ifndef INC_TASK_COUPLE_SCENARIO_H
#define INC_TASK_COUPLE_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"
#include "Task/System/Tuning.h"

class CTaskMoveFollowEntityOffset;

//-------------------------------------------------------------------------
// Couple scenario
//-------------------------------------------------------------------------
class CTaskCoupleScenario : public CTaskScenario
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Leader resume distance, squared.
		float m_ResumeDistSq;

		// PURPOSE:	Leader stop distance, squared.
		float m_StopDistSq;

		// PURPOSE:	Min distance before coupled ped is allowed to stand still.
		float m_TargetDistance;

		PAR_PARSABLE;
	};

	enum
	{
		CF_Leader		= BIT0,
		CF_UseScenario	= BIT1
	};

	enum
	{
		State_Start = 0,
		State_UsingScenario,
		State_Leader_Walks,
		State_Leader_Waits,
		State_Follower_Walks,
		State_Follower_Waits,
		State_Finish
	};

	CTaskCoupleScenario(s32 scenarioType, CScenarioPoint* pScenarioPoint, CPed* pPartnerPed, u32 flags);
	~CTaskCoupleScenario();
	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COUPLE_SCENARIO;}

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual	void				Debug() const;
#endif

	// Basic FSM implementation
	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);

	virtual bool				ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	// Task updates that are called before the FSM update
	virtual FSM_Return			ProcessPreFSM	();

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const		{ return GetState(); }

	virtual void				CleanUp();

	bool						GetIsLeader() const { return m_Flags.IsFlagSet(CF_Leader); }

	void						SetPartnerPed(CPed* pPed) { m_pPartnerPed = pPed; }

#if __BANK
	static Vector3				ms_vOverridenVector;
	static bool					ms_bUseOverridenTargetPos;
	static bool					ms_bStopFollower;
#endif 

protected:

	FSM_Return					Start_OnUpdate();
	void						UsingScenario_OnEnter();
	FSM_Return					UsingScenario_OnUpdate();
	void						Leader_Walks_OnEnter();
	FSM_Return					Leader_Walks_OnUpdate();
	void						Leader_Waits_OnEnter();
	FSM_Return					Leader_Waits_OnUpdate();
	void						Follower_Walks_OnEnter();
	FSM_Return					Follower_Walks_OnUpdate();
	void						Follower_Waits_OnEnter();
	FSM_Return					Follower_Waits_OnUpdate();

	void						PickWalkingState();

	// Checks to see if we are deliberately waiting.  We return the signal by NULL or
	// a valid follow entity offset (as it is checked).  This allows us to re-use it to
	// tell it to stop.  A very case specific, and therefore, private function
	CTaskMoveFollowEntityOffset *IsFollowerDeliberatelyWaiting		( const CPed &ped ) const;
	bool						IsLeaderDeliberatelyWaiting			() const;

	RegdPed						m_pPartnerPed;
	Vector3						m_vOffset;

	fwFlags32					m_Flags;

	bool						m_KeepMovingWhenFindingPath;

	static Tunables				sm_Tunables;
};

#endif//!INC_TASK_COUPLE_SCENARIO_H