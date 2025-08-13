// FILE :    TaskSearchInHeli.h
// PURPOSE : Subtask of search used while in a heli

#ifndef TASK_SEARCH_IN_HELI_H
#define TASK_SEARCH_IN_HELI_H

// Game headers
#include "task/Combat/Subtasks/TaskSearchBase.h"
#include "task/System/Tuning.h"

//=========================================================================
// CTaskSearchInHeli
//=========================================================================

class CTaskSearchInHeli : public CTaskSearchInVehicleBase
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
		
		float	m_FleeOffset;
		float	m_CruiseSpeed;
		int		m_MinHeightAboveTerrain;

		PAR_PARSABLE;
	};

	CTaskSearchInHeli(const Params& rParams);
	virtual ~CTaskSearchInHeli();

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	virtual aiTask* Copy() const { return rage_new CTaskSearchInHeli(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SEARCH_IN_HELI; }
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

#endif // TASK_SEARCH_IN_HELI_H
