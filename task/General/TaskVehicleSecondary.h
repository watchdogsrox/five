//
// task/General/TaskVehicleSecondary.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_VEHICLE_SECONDARY_H
#define TASK_VEHICLE_SECONDARY_H

// Game headers
#include "Event/Event.h"
#include "Task/System/Task.h"

//***************************************
//Class:  CTaskAggressiveRubberneck
//Purpose:  Have a ped in a vehicle look at the event and also become an aggressive driver.
//***************************************

class CTaskAggressiveRubberneck : public CTask
{

public:

	enum 
	{
		State_Look = 0,
		State_Finish,
	};

	CTaskAggressiveRubberneck(const CEvent* pEventToWatch);
	virtual ~CTaskAggressiveRubberneck();

	virtual aiTask*	Copy() const { return rage_new CTaskAggressiveRubberneck(m_pEvent); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool MakeAbortable(const AbortPriority priority, const aiEvent* pEvent);

#if !__FINAL
	static const char*		GetStaticStateName(s32 iState);
#endif // !__FINAL

private:

	// States
	void						Look_OnEnter();
	FSM_Return					Look_OnUpdate();

private:

	// Member variables

	RegdConstEvent					m_pEvent;
};

#endif //TASK_VEHICLE_SECONDARY_H