//
// task/Default/TaskTaskFlyingWander
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_FLYING_WANDER_H
#define TASK_FLYING_WANDER_H

#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

//***************************************
//Class:  CTaskFlyingWander
//Purpose:  High level wander task for birds.
//***************************************

class CTaskFlyingWander : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Range offset for wandering
		float		m_RangeOffset;

		// PURPOSE:  Heading to make when wandering.
		float		m_HeadingWanderChange;

		// PURPOSE:  Target radius to use for FlyToPoint subtask
		float		m_TargetRadius;

		PAR_PARSABLE;
	};

	CTaskFlyingWander() : m_vTarget(VEC3_ZERO) { SetInternalTaskType(CTaskTypes::TASK_FLYING_WANDER);};

	~CTaskFlyingWander() {};

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_FLYING_WANDER;}

	virtual aiTask* Copy() const
	{
		CTaskFlyingWander * pClone = rage_new CTaskFlyingWander();
		return pClone;
	}


protected:

	// FSM FUNCTIONS
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateDecide_OnUpdate();

	FSM_Return StateMoving_OnEnter();
	FSM_Return StateMoving_OnUpdate();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Decide; }

	enum
	{
		State_Decide = 0,
		State_Moving,
	};

	//name debug info
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// PURPOSE:  The point to wander towards.
	Vec3V m_vTarget;

	static Tunables sm_Tunables;

};

//-----------------------------------------------------------------------------

#endif	// TASK_FLYING_WANDER_H
