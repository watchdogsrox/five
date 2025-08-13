//
// task/Default/TaskFlyingWander.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "Scene/World/GameWorldHeightMap.h"
#include "Task/Default/TaskFlyingWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskFlyToPoint.h"


CTaskFlyingWander::Tunables CTaskFlyingWander::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskFlyingWander, 0x931552b4);

AI_OPTIMISATIONS()

CTask::FSM_Return CTaskFlyingWander::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Decide)
			FSM_OnUpdate
				return StateDecide_OnUpdate();
		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();
	FSM_End
}


//Decide where to wander to
CTask::FSM_Return CTaskFlyingWander::StateDecide_OnUpdate()
{
	CPed* pPed = GetPed();
	m_vTarget = pPed->GetTransform().GetPosition();
	
	// Randomly offset heading for new steer point
	float newHeading = pPed->GetCurrentHeading() + (fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sm_Tunables.m_HeadingWanderChange);
	newHeading = fwAngle::LimitRadianAngle(newHeading);


	// Set the new target as that position on the X/Y grid
	m_vTarget.SetXf(-rage::Sinf(newHeading) * sm_Tunables.m_RangeOffset + m_vTarget.GetXf());
	m_vTarget.SetYf(rage::Cosf(newHeading) * sm_Tunables.m_RangeOffset + m_vTarget.GetYf());

	// We don't need to worry about the Z, because FlyToPoint calculates target distance only from XY

	SetState(State_Moving);
	return FSM_Continue;
}

// Start a FlyToPoint task to move towards the target point
CTask::FSM_Return CTaskFlyingWander::StateMoving_OnEnter()
{
	CTaskFlyToPoint* pGoToTask = rage_new CTaskFlyToPoint(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(m_vTarget), 1.0f, true);
	SetNewTask(rage_new CTaskComplexControlMovement(pGoToTask));
	return FSM_Continue;
}

//Check if we reached the target, if so go back to decide
CTask::FSM_Return CTaskFlyingWander::StateMoving_OnUpdate()
{
	const ScalarV vZero(V_ZERO);
	Vec3V vPed = GetPed()->GetTransform().GetPosition();
	vPed.SetZ(vZero);

	Vec3V vFlatTarget = m_vTarget;
	vFlatTarget.SetZ(vZero);

	// Keep moving until the bird gets close to their destination, then switch it for some other point.
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || IsLessThanAll(DistSquared(vPed, vFlatTarget), ScalarV(sm_Tunables.m_TargetRadius * sm_Tunables.m_TargetRadius)))
	{
		SetState(State_Decide);
	}
	return FSM_Continue;
}


//name debug info
#if !__FINAL
const char * CTaskFlyingWander::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Decide: return "Deciding";
	case State_Moving: return "Moving";
	default: return "NULL";
	}
}
#endif //!__FINAL
