//
// task/Response/TaskSidestep.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SIDESTEP_H
#define TASK_SIDESTEP_H

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"

//***************************************
//Class:  CTaskSidestep
//Purpose:  Play a stepping reaction clip in one of the four cardinal directions based on the direction of the bumper.
//Picks which clipset to play from based on peds.meta
//***************************************

class CTaskSidestep : public CTask
{

public:

	enum 
	{
		State_Stream = 0,
		State_Play,
		State_Finish
	};

	CTaskSidestep(Vec3V_ConstRef vBumperPosition);
	virtual ~CTaskSidestep();

	virtual aiTask*	Copy() const { return rage_new CTaskSidestep(m_vBumperPosition); }
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_SIDESTEP; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	static const char*		GetStaticStateName(s32 iState);
#endif // !__FINAL

private:

	// States
	FSM_Return					Stream_OnEnter();
	FSM_Return					Stream_OnUpdate();

	FSM_Return					Play_OnEnter();
	FSM_Return					Play_OnUpdate();

	// Helper functions
	void						PickSidestepClip(fwMvClipId& clipHash);

private:

	// PURPOSE:  Helper for streaming in the clipset
	fwClipSetRequestHelper	m_ClipSetHelper;

	// PURPOSE:  Where the bump came from
	Vec3V					m_vBumperPosition;

	// PURPOSE:  The clipset from which to play the animation
	fwMvClipSetId			m_ClipSet;
};

#endif //TASK_DIVE_TO_GROUND_H