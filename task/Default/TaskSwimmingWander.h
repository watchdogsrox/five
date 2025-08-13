//
// task/Default/TaskSwimmingWander
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SWIMMING_WANDER_H
#define TASK_SWIMMING_WANDER_H

#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

//***************************************
//Class:  CTaskSwimmingWander
//Purpose:  High level wander task for fish.
//***************************************

class CTaskSwimmingWander : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE: How deep below the surface a surface-skimmer ped should be.
		float		m_SurfaceSkimmerDepth;

		// PURPOSE: How deep below the surface a normal swimming ped should be.
		float		m_NormalPreferredDepth;

		// PURPOSE: How long the avoidance probe should be.
		float		m_AvoidanceProbeLength;

		// PURPOSE: How far by default to pullback from the swimmer when launching a probe.
		float		m_AvoidanceProbePullback;

		// PURPOSE: How long between avoidance probe checks (ms).
		u32			m_AvoidanceProbeInterval;

		// PURPOSE: How much to change a fish's swim angle by in degrees if you discovered that you will hit something in the near future.
		float		m_AvoidanceSteerAngleDegrees;

		// PURPOSE: Min time for the duration to be when resetting it to 0 during "instant" probe checks when the swimmer hits a wall.
		u32			m_InstantProbeDurationMin;

		PAR_PARSABLE;
	};

	CTaskSwimmingWander() : m_fPreferredDepth(0.0f), m_fWanderDirection(0.0f), m_bCanForceProbe(true), m_bSkimSurface(false) { SetInternalTaskType(CTaskTypes::TASK_SWIMMING_WANDER);};

	~CTaskSwimmingWander() {};

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SWIMMING_WANDER;}

	virtual aiTask* Copy() const
	{
		CTaskSwimmingWander * pClone = rage_new CTaskSwimmingWander();
		return pClone;
	}


protected:

	// FSM FUNCTIONS

	FSM_Return	ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateInitial_OnUpdate();

	void StateMoving_OnEnter();
	FSM_Return StateMoving_OnUpdate();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	enum
	{
		State_Initial = 0,
		State_Moving,
	};

	//name debug info
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// Return the angle the fish should turn to avoid the obstacle.
	float AvoidObstacles();

	// Return the angle the fish should turn to avoid the obstacle.
	float AvoidNonCreationZones();

private:

	// LoS probe for object collision avoidance
	WorldProbe::CShapeTestSingleResult		m_CapsuleResult;

	// Timer used to control when probe tests are issued
	CTaskGameTimer							m_Timer;

	// PURPOSE:  How far below the waterline the swimmer prefers to be.
	float									m_fPreferredDepth;

	// The direction the fish wants to swim.
	float									m_fWanderDirection;

	// Whether or not a collision can trigger an immediate probe test
	bool									m_bCanForceProbe;

	// Whether or not this swimmer wants to skim the surface;
	bool									m_bSkimSurface;

	static bool								sm_bRenderProbeChecks;

	static Tunables							sm_Tunables;

};

//-----------------------------------------------------------------------------

#endif	// TASK_SWIMMING_WANDER_H
