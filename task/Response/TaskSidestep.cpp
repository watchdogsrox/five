//
// task/Response/TaskSidestep.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

//rage headers
#include "math/angmath.h"

// game headers
#include "Animation/AnimDefines.h"
#include "Event/Events.h"
#include "Peds/ped.h"
#include "Task/Response/TaskSidestep.h"


AI_OPTIMISATIONS()

CTaskSidestep::CTaskSidestep(Vec3V_ConstRef vBumpDirection)
: m_ClipSetHelper()
, m_vBumperPosition(vBumpDirection)
, m_ClipSet(CLIP_SET_ID_INVALID)
{
	SetInternalTaskType(CTaskTypes::TASK_SIDESTEP);
}

CTaskSidestep::~CTaskSidestep()
{
}


#if !__FINAL
const char* CTaskSidestep::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Stream: return "Streaming";
		case State_Play: return "Playing";
		case State_Finish: return "Finishing";
		default: return "NULL";
	}
}
#endif // !__FINAL

CTask::FSM_Return CTaskSidestep::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Stream)
			FSM_OnEnter
				return Stream_OnEnter();
			FSM_OnUpdate
				return Stream_OnUpdate();
		FSM_State(State_Play)
			FSM_OnEnter
				return Play_OnEnter();
			FSM_OnUpdate
				return Play_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

// Pick the clipset based on peds.meta
CTask::FSM_Return CTaskSidestep::Stream_OnEnter()
{
	m_ClipSet = GetPed()->GetPedModelInfo()->GetSidestepClipSet();
	return FSM_Continue;
}

// If the clip is valid, try and stream it in.
CTask::FSM_Return CTaskSidestep::Stream_OnUpdate()
{
	if (m_ClipSet != CLIP_SET_ID_INVALID)
	{
		//start streaming it in
		if (m_ClipSetHelper.Request(m_ClipSet))
		{
			SetState(State_Play);
		}
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

// Pick the clip to play from the clipset and start playing.
CTask::FSM_Return CTaskSidestep::Play_OnEnter()
{
	fwMvClipId clipHash;

	PickSidestepClip(clipHash);

	if (m_ClipSet.GetHash() != 0 && clipHash.GetHash() != 0)
	{
		StartClipBySetAndClip(GetPed(),m_ClipSet, clipHash, 0, 0, 0, SLOW_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}
	return FSM_Continue;	
}

// When the clip is finished, transition to finish.
CTask::FSM_Return CTaskSidestep::Play_OnUpdate()
{
	// Update the ped's desired heading to be it's current heading - otherwise the ped will try and rotate back to the way it was looking.
	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());

	if ( !GetClipHelper() || IsClipConsideredFinished() )
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

// Pick a reaction clip opposite to the direction the ped was pushed from.
void CTaskSidestep::PickSidestepClip(fwMvClipId& clipHash)
{
	CPed* pPed = GetPed();
	Vec3V vPed = pPed->GetTransform().GetPosition();

	// Get the angle from the bumper to the bumpee 
	float fAngle = fwAngle::GetRadianAngleBetweenPoints(m_vBumperPosition.GetXf(), m_vBumperPosition.GetYf(), vPed.GetXf(), vPed.GetYf());
	//subtract angle of ped's heading
	fAngle = SubtractAngleShorterFast(fAngle, pPed->GetCurrentHeading());

	// get heading into interval 0->2Pi
	if(fAngle < 0.0f)
	{
		fAngle += TWO_PI;
	}
	// then reverse direction
	fAngle = TWO_PI - fAngle;
	// subtract half an interval of the node heading range
	fAngle = fAngle + QUARTER_PI;
	if(fAngle >= TWO_PI)
	{
		fAngle -= TWO_PI;
	}

	// Round to the nearest integer division of HALF/PI
	const int iDirection = static_cast<int>(rage::Floorf(fAngle/HALF_PI));

	// Which quad it is in = iDirection.
	// Set the clip used based on this direction.
	// 0 = Forward, increment clockwise.
	switch(iDirection)
	{
		case 0:
			//in response to ped from FWD, use step backward etc.
			clipHash = CLIP_STEP_BWD;
			break;
		case 1:
			clipHash = CLIP_STEP_LFT;
			break;
		case 2:
			clipHash = CLIP_STEP_FWD;
			break;
		case 3:
			clipHash = CLIP_STEP_RGT;
			break;
		default:
			Assertf(0, "Error, invalid direction quadrant!");
			break;
	}
}
