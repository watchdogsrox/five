#include "TaskDeerLocomotion.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"

#include "Debug/DebugScene.h"
#include "animation/MovePed.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Default/TaskPlayer.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Deer Locomotion
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskDeerLocomotion::ms_startTurnLId("StartTurnL");
const fwMvBooleanId CTaskDeerLocomotion::ms_OnEnterTurnLId("OnEnterTurnL");

const fwMvRequestId CTaskDeerLocomotion::ms_startTurnRId("StartTurnR");
const fwMvBooleanId CTaskDeerLocomotion::ms_OnEnterTurnRId("OnEnterTurnR");

const fwMvBooleanId CTaskDeerLocomotion::ms_OnExitTurnId("OnExitTurn");

const fwMvRequestId CTaskDeerLocomotion::ms_startIdleId("StartIdle");
const fwMvBooleanId CTaskDeerLocomotion::ms_OnEnterIdleId("OnEnterIdle");

const fwMvRequestId CTaskDeerLocomotion::ms_startLocomotionId("StartLocomotion");
const fwMvBooleanId CTaskDeerLocomotion::ms_OnEnterLocomotionId("OnEnterLocomotion");

const fwMvFloatId CTaskDeerLocomotion::ms_MoveDesiredSpeedId("DesiredSpeed");
const fwMvFloatId CTaskDeerLocomotion::ms_MoveDirectionId("Direction");

CTaskDeerLocomotion::CTaskDeerLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_DEER);
}

//////////////////////////////////////////////////////////////////////////

CTaskDeerLocomotion::~CTaskDeerLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDeerLocomotion::ProcessPreFSM()
{
	float fCurrentMbrX, fCurrentMbrY;
	GetMotionData().GetCurrentMoveBlendRatio(fCurrentMbrX, fCurrentMbrY);

	float fDesiredMbrX, fDesiredMbrY;
	GetMotionData().GetDesiredMoveBlendRatio(fDesiredMbrX, fDesiredMbrY);

	TUNE_GROUP_FLOAT (Deer_MOVEMENT, fDeerAccelRate, 2.0f, 0.0f, 10.0f, 0.0001f);

	InterpValue(fCurrentMbrX, fDesiredMbrX, fDeerAccelRate);
	InterpValue(fCurrentMbrY, fDesiredMbrY, fDeerAccelRate);

	GetMotionData().SetCurrentMoveBlendRatio(fCurrentMbrY, fCurrentMbrX);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDeerLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Initial)
		FSM_OnEnter
		return StateInitial_OnEnter();
		FSM_OnUpdate
		return StateInitial_OnUpdate();

	FSM_State(State_Idle)
		FSM_OnEnter
		return StateIdle_OnEnter(); 
		FSM_OnUpdate
		return StateIdle_OnUpdate();

	FSM_State(State_Locomotion)
		FSM_OnEnter
		return StateLocomotion_OnEnter(); 
		FSM_OnUpdate
		return StateLocomotion_OnUpdate(); 

	FSM_State(State_TurnL)
		FSM_OnEnter
		return StateTurnL_OnEnter(); 
		FSM_OnUpdate
		return StateTurnL_OnUpdate(); 

	FSM_State(State_TurnR)
		FSM_OnEnter
		return StateTurnR_OnEnter(); 
		FSM_OnUpdate
		return StateTurnR_OnUpdate(); 

	FSM_End
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskDeerLocomotion::ProcessPostMovement()
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
CTask * CTaskDeerLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskDeerLocomotion::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDeerLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (pSet)
	{
		static const fwMvClipId s_walkClip("walk");
		static const fwMvClipId s_runClip("run");
		static const fwMvClipId s_sprintClip("sprint");

		const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

		RetrieveMoveSpeeds(*pSet, speeds, clipNames);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateInitial_OnEnter()
{
	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDeer);

	// stream in the movement clip set
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDeerLocomotion::ShouldBeMoving()
{
	return GetMotionData().GetCurrentMbrY() > 0.0f;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateInitial_OnUpdate()
{
	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(pSet); 

	if (pSet && pSet->Request() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDeer))
	{	
		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootDeer);

		m_networkHelper.SetClipSet( m_clipSetId); 

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();
		move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);

		SetState( State_Idle );
		m_networkHelper.ForceNextStateReady(); // Needed?
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateLocomotion_OnEnter()
{
	m_networkHelper.SendRequest( ms_startLocomotionId);
	m_networkHelper.WaitForTargetState(ms_OnEnterLocomotionId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateLocomotion_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetDesiredHeading(), GetPed()->GetCurrentHeading());

	// clip is +/- 90 degree turn
	float desiredAngle = Clamp( fHeadingDelta/HALF_PI, -1.0f, 1.0f);	
	desiredAngle = (-desiredAngle * 0.5f) + 0.5f;

	m_networkHelper.SetFloat( ms_MoveDirectionId, desiredAngle );

	float currentMBR = GetMotionData().GetCurrentMbrY();
	currentMBR = Clamp( currentMBR, MOVEBLENDRATIO_WALK , MOVEBLENDRATIO_SPRINT );
	float fDesiredSpeed = (  currentMBR - MOVEBLENDRATIO_WALK ) * 0.5f;

	m_networkHelper.SetFloat( ms_MoveDesiredSpeedId, fDesiredSpeed);

	if ( !ShouldBeMoving() )
	{	
		SetState(State_Idle);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateTurnL_OnEnter()
{
	m_networkHelper.SendRequest( ms_startTurnLId);
	m_networkHelper.WaitForTargetState(ms_OnEnterTurnLId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateTurnL_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// When the clip has completed, decide what to do next
	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() && !pPed->GetIsAnyFixedFlagSet() )
		{
			if (m_networkHelper.GetBoolean( ms_OnExitTurnId ) )
			{
				SetState( State_Idle );
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateTurnR_OnEnter()
{
	m_networkHelper.SendRequest( ms_startTurnRId );
	m_networkHelper.WaitForTargetState( ms_OnEnterTurnRId );
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateTurnR_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// When the clip has completed, decide what to do next
	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() && !pPed->GetIsAnyFixedFlagSet())
		{
			if (m_networkHelper.GetBoolean( ms_OnExitTurnId ))
			{
				SetState(State_Idle);
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateIdle_OnEnter()
{
	m_networkHelper.SendRequest( ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDeerLocomotion::StateIdle_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	if (GetPed()->GetDesiredHeading() != GetPed()->GetCurrentHeading())
	{
		Matrix34 mPedMatrix = MAT34V_TO_MATRIX34(GetPed()->GetMatrix()); 
		mPedMatrix.RotateLocalZ(GetPed()->GetDesiredHeading()); 
	}

	if ( ShouldBeMoving() )
	{	
		const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetDesiredHeading(), GetPed()->GetCurrentHeading());
		const float MIN_ANGLE_FOR_TURN = 90.0f;

		// 90 degree turn stuff
		if ( fHeadingDelta < ( DtoR * -MIN_ANGLE_FOR_TURN ) )
		{
			SetState(State_TurnR);
		}
		else 
			if ( fHeadingDelta > ( DtoR * MIN_ANGLE_FOR_TURN ) )
			{
				SetState(State_TurnL);
			}
			else
			{
				SetState(State_Locomotion);
			}
	}	

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDeerLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	static bank_float sf_PitchLimitTuning = 1.0f;
	fMinOut = -sf_PitchLimitTuning;
	fMaxOut =  sf_PitchLimitTuning;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char* CTaskDeerLocomotion::GetStateName(s32 iState) const
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Idle: return "Idle";
	case State_Locomotion: return "Locomotion";
	case State_TurnL: return "TurnL";
	case State_TurnR: return "TurnR";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskDeerLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////