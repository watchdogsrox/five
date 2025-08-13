// File header
#include "Task/Motion/Locomotion/TaskRepositionMove.h"

// Rage headers
#include "math/angmath.h"

// Game headers
#include "Animation/Move.h"

AI_OPTIMISATIONS()

const fwMvFloatId CTaskRepositionMove::ms_DesiredHeading("DesiredHeading",0x5B256E29);
const fwMvFloatId CTaskRepositionMove::ms_Distance("Distance",0xD6FF9582);
const fwMvFloatId CTaskRepositionMove::ms_Direction("Direction",0x34DF9828);
const fwMvRequestId CTaskRepositionMove::ms_RepositionMoveRequestId("RepositionMove",0x45D8361B);
const fwMvBooleanId CTaskRepositionMove::ms_RepositionMoveOnEnterId("RepositionMove_OnEnter",0x262349C8);
const fwMvBooleanId CTaskRepositionMove::ms_BlendOutId("BLEND_OUT",0xAB120D43);

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::CTaskRepositionMove()
: m_vTargetPos(Vector3::ZeroType)
, m_vStartPos(Vector3::ZeroType)
, m_fDesiredHeading(0.f)
, m_fDistance(0.f)
, m_fDirection(0.f)
{
	SetInternalTaskType(CTaskTypes::TASK_REPOSITION_MOVE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::~CTaskRepositionMove()
{
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskRepositionMove::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Sphere(GetPed()->GetMotionData()->GetRepositionMoveTarget(), 0.1f, Color_pink, true, -1);
#endif // DEBUG_DRAW
	CTaskMotionBase::Debug();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char* CTaskRepositionMove::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
	case State_Initial:    return "Initial";
	case State_Reposition: return "Reposition";
	case State_Quit:       return "Quit";
	default:               taskAssertf(0, "Unhandled state %d", iState); return "Unknown";
	}
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::FSM_Return CTaskRepositionMove::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Reposition)
			FSM_OnEnter
				return StateReposition_OnEnter();
			FSM_OnUpdate
				return StateReposition_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

void CTaskRepositionMove::CleanUp()
{
	CPed* pPed = GetPed();

	// Clear the reposition move data
	pPed->GetMotionData()->SetRepositionMoveTarget(false);
}

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::FSM_Return CTaskRepositionMove::StateInitial_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskRepositionMove);

		// Make sure the clip set is streamed in
		static const fwMvClipSetId s_ClipSetId("move_strafe@reposition",0x31642590);
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(s_ClipSetId);
		taskAssertf(pClipSet, "Clip set [%s] does not exist", s_ClipSetId.GetCStr());

		if(pClipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskRepositionMove))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskRepositionMove);
			m_MoveNetworkHelper.SetClipSet(s_ClipSetId);
		}
	}

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(!m_MoveNetworkHelper.IsNetworkAttached())
		{
			m_MoveNetworkHelper.DeferredInsert();
		}

		if(m_MoveNetworkHelper.IsNetworkAttached())
		{
			SetState(State_Reposition);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::FSM_Return CTaskRepositionMove::StateReposition_OnEnter()
{
	CPed* pPed = GetPed();

	// Store the target pos
	m_vTargetPos = pPed->GetMotionData()->GetRepositionMoveTarget();

	// Store the start pos
	m_vStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Store the desired heading
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
	m_fDesiredHeading = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
	m_fDesiredHeading = ConvertRadianToSignal(m_fDesiredHeading);
	
	// Store the distance
	Vector3 vDiff(m_vTargetPos);
	vDiff -= m_vStartPos;
	m_fDistance = vDiff.XYMag();
	m_fDistance = Min(m_fDistance, 1.f);

	// Store the direction
	float fDirectionHeading = fwAngle::GetRadianAngleBetweenPoints(m_vTargetPos.x, m_vTargetPos.y, m_vStartPos.x, m_vStartPos.y);
	m_fDirection = SubtractAngleShorter(fDirectionHeading, fCurrentHeading);
	m_fDirection = ConvertRadianToSignal(m_fDirection);

	m_MoveNetworkHelper.WaitForTargetState(ms_RepositionMoveOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_RepositionMoveRequestId);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskRepositionMove::FSM_Return CTaskRepositionMove::StateReposition_OnUpdate()
{
	CPed* pPed = GetPed();

	m_MoveNetworkHelper.SetFloat(ms_DesiredHeading, m_fDesiredHeading);
	m_MoveNetworkHelper.SetFloat(ms_Distance, m_fDistance);
	m_MoveNetworkHelper.SetFloat(ms_Direction, m_fDirection);

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(m_MoveNetworkHelper.GetBoolean(ms_BlendOutId))
	{
		static dev_float ARRIVAL_TOLERANCE = 0.5f;
		if((VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - pPed->GetMotionData()->GetRepositionMoveTarget()).XYMag2() > square(ARRIVAL_TOLERANCE))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}

		// Finished
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////
