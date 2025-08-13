// FILE :    TaskReactInDirection.h

// File header
#include "Task/Response/TaskReactInDirection.h"

// Game headers
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "Peds/ped.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskReactInDirection
////////////////////////////////////////////////////////////////////////////////

CTaskReactInDirection::Tunables CTaskReactInDirection::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskReactInDirection, 0x481a394c);

fwMvBooleanId CTaskReactInDirection::sm_ClipEnded("ClipEnded",0x9ACF9318);
fwMvBooleanId CTaskReactInDirection::sm_Interruptible("Interruptible",0x550A14CF);
fwMvBooleanId CTaskReactInDirection::sm_React_OnEnter("React_OnEnter",0xFDFDC33E);

fwMvFlagId CTaskReactInDirection::sm_BackLeft("BackLeft",0x7A25E632);
fwMvFlagId CTaskReactInDirection::sm_BackRight("BackRight",0xF557C96C);
fwMvFlagId CTaskReactInDirection::sm_Blend360("Blend360",0x1C56A546);
fwMvFlagId CTaskReactInDirection::sm_FrontLeft("FrontLeft",0xB654884);
fwMvFlagId CTaskReactInDirection::sm_FrontRight("FrontRight",0x7B90ED3D);
fwMvFlagId CTaskReactInDirection::sm_Left("Left",0x6FA34840);
fwMvFlagId CTaskReactInDirection::sm_Right("Right",0xB8CCC339);
fwMvFlagId CTaskReactInDirection::sm_Use4Directions("Use4Directions",0x2900FB04);
fwMvFlagId CTaskReactInDirection::sm_Use5Directions("Use5Directions",0x2D75A27E);
fwMvFlagId CTaskReactInDirection::sm_Use6Directions("Use6Directions",0x33DAA3A7);

fwMvFloatId CTaskReactInDirection::sm_Phase("Phase",0xA27F482B);

fwMvRequestId CTaskReactInDirection::sm_React("React",0x13A3EFDC);

CTaskReactInDirection::CTaskReactInDirection(fwMvClipSetId clipSetId, const CAITarget& rTarget, fwFlags8 uConfigFlags)
: m_ClipSetId(clipSetId)
, m_Target(rTarget)
, m_nDirection(CDirectionHelper::D_Invalid)
, m_fBlendOutDuration(NORMAL_BLEND_DURATION)
, m_uConfigFlags(uConfigFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_IN_DIRECTION);

	//Assert that the task is valid.
	taskAssertf(IsValid(m_ClipSetId, m_Target),"clipSetId %s : streamed %s : m_ReactTarget %d, %p", m_ClipSetId.GetCStr(), fwClipSetManager::IsStreamedIn_DEPRECATED(m_ClipSetId) ? "true" : "false", m_Target.GetIsValid(), m_Target.GetEntity());
}

CTaskReactInDirection::~CTaskReactInDirection()
{

}

bool CTaskReactInDirection::IsValid(fwMvClipSetId clipSetId, const CAITarget& rTarget)
{
	//Ensure the clip set is valid.
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	//Ensure the clip set is streamed in.
	if(!fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId))
	{
		return false;
	}

	//Ensure the target is valid.
	if(!rTarget.GetIsValid())
	{
		return false;
	}

	return true;
}

#if !__FINAL
void CTaskReactInDirection::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskReactInDirection::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_React",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

bool CTaskReactInDirection::CanInterrupt() const
{
	//Ensure the event is valid.
	if(!m_MoveNetworkHelper.GetBoolean(sm_Interruptible))
	{
		return false;
	}

	return true;
}

bool CTaskReactInDirection::HasClipEnded() const
{
	//Check if the event has come through.
	if(m_MoveNetworkHelper.GetBoolean(sm_ClipEnded))
	{
		return true;
	}

	//Check if the phase is complete.
	if(m_MoveNetworkHelper.GetFloat(sm_Phase) >= 0.99f)
	{
		return true;
	}

	return false;
}

void CTaskReactInDirection::SetFlagForDirection()
{
	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

	//Check the direction.
	m_nDirection = CDirectionHelper::CalculateClosestDirection(GetPed()->GetTransform().GetPosition(),
		GetPed()->GetCurrentHeading(), vTargetPosition);
	switch(m_nDirection)
	{
		case CDirectionHelper::D_FrontLeft:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_FrontLeft);
			break;
		}
		case CDirectionHelper::D_FrontRight:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_FrontRight);
			break;
		}
		case CDirectionHelper::D_BackLeft:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_BackLeft);
			break;
		}
		case CDirectionHelper::D_BackRight:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_BackRight);
			break;
		}
		case CDirectionHelper::D_Left:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_Left);
			break;
		}
		case CDirectionHelper::D_Right:
		{
			m_MoveNetworkHelper.SetFlag(true, sm_Right);
			break;
		}
		default:
		{
			break;
		}
	}
}

void CTaskReactInDirection::SetFlagForNumDirections()
{
	//Check the flags.
	if(m_uConfigFlags.IsFlagSet(CF_Use4Directions))
	{
		m_MoveNetworkHelper.SetFlag(true, sm_Use4Directions);
	}
	else if(m_uConfigFlags.IsFlagSet(CF_Use5Directions))
	{
		m_MoveNetworkHelper.SetFlag(true, sm_Use5Directions);
	}
	else if(m_uConfigFlags.IsFlagSet(CF_Use6Directions))
	{
		m_MoveNetworkHelper.SetFlag(true, sm_Use6Directions);
	}
}

void CTaskReactInDirection::SetFlags()
{
	//Set the flags.
	m_MoveNetworkHelper.SetFlag(m_uConfigFlags.IsFlagSet(CF_Blend360), sm_Blend360);

	//Set the flag for the number of directions.
	SetFlagForNumDirections();

	//Set the flag for the direction.
	SetFlagForDirection();
}

bool CTaskReactInDirection::ShouldInterrupt() const
{
	//Check if we can interrupt.
	if(CanInterrupt())
	{
		//Check if the flag is set.
		if(m_uConfigFlags.IsFlagSet(CF_Interrupt))
		{
			return true;
		}
	}

	return false;
}

void CTaskReactInDirection::CleanUp()
{
	//Check if the network is active.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		//Blend out the network.
		GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, m_fBlendOutDuration);
	}
}

aiTask* CTaskReactInDirection::Copy() const
{
	//Create the task.
	CTaskReactInDirection* pTask = rage_new CTaskReactInDirection(m_ClipSetId, m_Target, m_uConfigFlags);
	pTask->SetBlendOutDuration(m_fBlendOutDuration);

	return pTask;
}

CTask::FSM_Return CTaskReactInDirection::ProcessPreFSM()
{
	//Ensure the task is valid.
	if(!IsValid(m_ClipSetId, m_Target))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactInDirection::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_React)
			FSM_OnEnter
				React_OnEnter();
			FSM_OnUpdate
				return React_OnUpdate();
			FSM_OnExit
				React_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskReactInDirection::Start_OnUpdate()
{
	//React.
	SetState(State_React);

	return FSM_Continue;
}

void CTaskReactInDirection::React_OnEnter()
{
	//Create the network.
	taskAssert(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskReactInDirection));
	m_MoveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskReactInDirection);

	//Blend in the network.
	GetPed()->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), NORMAL_BLEND_DURATION);

	//Set the flags.
	SetFlags();

	//Set the clip set.
	m_MoveNetworkHelper.SetClipSet(m_ClipSetId);

	//Send the request.
	m_MoveNetworkHelper.SendRequest(sm_React);

	//Wait for the target state.
	m_MoveNetworkHelper.WaitForTargetState(sm_React_OnEnter);
}

CTask::FSM_Return CTaskReactInDirection::React_OnUpdate()
{
	//Check if we have reached the target state.
	if(m_MoveNetworkHelper.IsInTargetState())
	{
		//Check if the clip has ended.
		if(HasClipEnded())
		{
			//Finish the task.
			SetState(State_Finish);
		}
		//Check if we should interrupt.
		else if(ShouldInterrupt())
		{
			//Finish the task.
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskReactInDirection::React_OnExit()
{
	//Check if the network is active.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		//Blend out the network.
		GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, m_fBlendOutDuration);
	}
}
