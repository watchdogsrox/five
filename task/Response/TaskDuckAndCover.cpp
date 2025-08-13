#include "TaskDuckAndCover.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"

#include "animation/Move.h"
#include "animation/MovePed.h"

#include "System\Control.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Duck AND Cover
//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskDuckAndCover::ms_OnEnterDuckId("OnEnterDuck",0x42F2BA34);					// We've entered the enter duck state

const fwMvRequestId CTaskDuckAndCover::ms_EnterDuckCompleteId("StartDuckComplete",0x467BCF4B);		// Move to the next state "Doing Duck" (crap name selection on my part, should be "StartDoingDuck")

const fwMvBooleanId CTaskDuckAndCover::ms_OnDoingDuckId("OnDoingDuck",0x343710AB);					// We've entered the Doing duck state

const fwMvRequestId CTaskDuckAndCover::ms_DoingDuckCompleteId("DoingDuckComplete",0x309CBF29);		// Move to the next state "Exit Duck"

const fwMvBooleanId CTaskDuckAndCover::ms_OnExitDuckId("OnEndDuck",0x8DE1A30A);						// We've entered the Exit duck state

const fwMvBooleanId CTaskDuckAndCover::ms_OnCompletedId("OnCompleted",0x6CEB62FB);					// Reused for exit also

const fwMvBooleanId CTaskDuckAndCover::ms_InterruptibleId("Interruptible",0x550A14CF);

//////////////////////////////////////////////////////////////////////////

CTaskDuckAndCover::CTaskDuckAndCover()
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_DUCK_AND_COVER);
}

//////////////////////////////////////////////////////////////////////////

CTaskDuckAndCover::~CTaskDuckAndCover()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDuckAndCover::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Initial)
		FSM_OnEnter
		return StateInitial_OnEnter();
	FSM_OnUpdate
		return StateInitial_OnUpdate();

	FSM_State(State_EnterDuck)
		FSM_OnEnter
		return StateEnterDuck_OnEnter(); 
	FSM_OnUpdate
		return StateEnterDuck_OnUpdate();

	FSM_State(State_Ducking)
		FSM_OnEnter
		return StateDucking_OnEnter(); 
	FSM_OnUpdate
		return StateDucking_OnUpdate(); 

	FSM_State(State_ExitDuck)
		FSM_OnEnter
		return StateExitDuck_OnEnter(); 
	FSM_OnUpdate
		return StateExitDuck_OnUpdate(); 
	FSM_End

}


//////////////////////////////////////////////////////////////////////////

void	CTaskDuckAndCover::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_networkHelper, NORMAL_BLEND_DURATION);
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDuckAndCover::StateInitial_OnEnter()
{
	m_clipSetId	= CLIP_SET_DUCK_AND_COVER;

	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDuckAndCover);

	return FSM_Continue;
}

CTask::FSM_Return	CTaskDuckAndCover::StateInitial_OnUpdate()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(clipSet); 

	if (clipSet && clipSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDuckAndCover))
	{	
		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootDuckAndCover);
		m_networkHelper.SetClipSet(m_clipSetId);

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();
		move.SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);		//move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);

		// Move into the enter duck state
		SetState(State_EnterDuck);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDuckAndCover::StateEnterDuck_OnEnter()
{
	// Set to wait for Enter
	m_networkHelper.WaitForTargetState(ms_OnEnterDuckId);
	return FSM_Continue;
}

CTask::FSM_Return	CTaskDuckAndCover::StateEnterDuck_OnUpdate()
{
	// Await move into Enter
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// Monitor playback of Enter clip until we get the "OnCompleted" Signal/Event?
	// When the clip has completed, decide what to do next
	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() )
		{
			if (	m_networkHelper.GetBoolean( ms_OnCompletedId ) )
			{
				SetState( State_Ducking );
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDuckAndCover::StateDucking_OnEnter()
{
	// Tell the MoVE system to begin the Ducking Looping Clip?
	m_networkHelper.SendRequest( ms_EnterDuckCompleteId );
	m_networkHelper.WaitForTargetState( ms_OnDoingDuckId );
	return FSM_Continue;
}

CTask::FSM_Return	CTaskDuckAndCover::StateDucking_OnUpdate()
{
	// Await move into "Doing"
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() )
		{
			// Check if the button to duck for cover has been "unpressed", then move onto the next state "ExitDuck"
			CControl* pControl = pPed->GetControlFromPlayer();
			if( pControl && !pControl->GetPedDuckAndCover().IsDown() )
			{
				SetState( State_ExitDuck );
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDuckAndCover::StateExitDuck_OnEnter()
{
	// Begin the "Exit" clip.
	m_networkHelper.SendRequest( ms_DoingDuckCompleteId );
	m_networkHelper.WaitForTargetState( ms_OnExitDuckId );
	return FSM_Continue;
}

CTask::FSM_Return	CTaskDuckAndCover::StateExitDuck_OnUpdate()
{
	// Await "OnCompleted" signal/event from the ExitDuck clip, then exit (FSM_Quit)?
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	if (m_networkHelper.IsNetworkActive() )
	{
		if (m_networkHelper.GetBoolean( ms_OnCompletedId ) ||
			m_networkHelper.GetBoolean( ms_InterruptibleId ) )
		{
			return FSM_Quit;		// DONE!
		}
	}


	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskDuckAndCover::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_EnterDuck: return "EnterDuck";
	case State_Ducking: return "Ducking";
	case State_ExitDuck: return "ExitDuck";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskDuckAndCover::Debug() const
{

}


#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////