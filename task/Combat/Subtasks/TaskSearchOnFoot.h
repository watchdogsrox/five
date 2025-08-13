// FILE :    TaskSearchOnFoot.h
// PURPOSE : Subtask of search used while on foot

#ifndef TASK_SEARCH_ON_FOOT_H
#define TASK_SEARCH_ON_FOOT_H

// Game headers
#include "task/Combat/Subtasks/TaskSearchBase.h"
#include "task/System/Tuning.h"

//=========================================================================
// CTaskSearchOnFoot
//=========================================================================

class CTaskSearchOnFoot : public CTaskSearchBase
{

public:

	enum State
	{
		State_Start = 0,
		State_Search,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_FleeOffset;
		float m_TargetRadius;
		float m_CompletionRadius;
		float m_SlowDownDistance;
		float m_FleeSafeDistance;
		float m_MoveBlendRatio;

		PAR_PARSABLE;
	};

	CTaskSearchOnFoot(const Params& rParams);
	virtual ~CTaskSearchOnFoot();

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	virtual aiTask* Copy() const { return rage_new CTaskSearchOnFoot(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SEARCH_ON_FOOT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

private:

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Search_OnEnter();
	FSM_Return	Search_OnUpdate();

private:

	static Tunables	sm_Tunables;

};

#endif // TASK_SEARCH_ON_FOOT_H
