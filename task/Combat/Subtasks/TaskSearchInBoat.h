// FILE :    TaskSearchInBoat.h
// PURPOSE : Subtask of search used while in a boat

#ifndef TASK_SEARCH_IN_BOAT_H
#define TASK_SEARCH_IN_BOAT_H

// Game headers
#include "task/Combat/Subtasks/TaskSearchBase.h"
#include "task/System/Tuning.h"

//=========================================================================
// CTaskSearchInBoat
//=========================================================================

class CTaskSearchInBoat : public CTaskSearchInVehicleBase
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
		
		PAR_PARSABLE;
	};

	CTaskSearchInBoat(const Params& rParams);
	virtual ~CTaskSearchInBoat();

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	virtual aiTask* Copy() const { return rage_new CTaskSearchInBoat(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SEARCH_IN_BOAT; }
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

#endif // TASK_SEARCH_IN_BOAT_H
