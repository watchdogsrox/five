#include "TaskDogLocomotion.h"

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

#define MIN_ANGLE_TO_FACING	(45)	// Degree's (shouldn't be higher than the turn clip allows, which is currently 90)

//////////////////////////////////////////////////////////////////////////
//	Dog Locomotion
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskDogLocomotion::ms_startTurnLId("StartTurnL");
const fwMvBooleanId CTaskDogLocomotion::ms_OnEnterTurnLId("OnEnterTurnL");

const fwMvRequestId CTaskDogLocomotion::ms_startTurnRId("StartTurnR");
const fwMvBooleanId CTaskDogLocomotion::ms_OnEnterTurnRId("OnEnterTurnR");

const fwMvBooleanId CTaskDogLocomotion::ms_OnExitTurnId("OnExitTurn");

const fwMvRequestId CTaskDogLocomotion::ms_startIdleId("StartIdle");
const fwMvBooleanId CTaskDogLocomotion::ms_OnEnterIdleId("OnEnterIdle");

const fwMvRequestId CTaskDogLocomotion::ms_startLocomotionId("StartLocomotion");
const fwMvBooleanId CTaskDogLocomotion::ms_OnEnterLocomotionId("OnEnterLocomotion");

const fwMvFloatId CTaskDogLocomotion::ms_MoveDesiredSpeedId("DesiredSpeed");
const fwMvFloatId CTaskDogLocomotion::ms_MoveDirectionId("Direction");

CTaskDogLocomotion::CTaskDogLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_DOG);
}

//////////////////////////////////////////////////////////////////////////

CTaskDogLocomotion::~CTaskDogLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDogLocomotion::ProcessPreFSM()
{
	float fCurrentMbrX, fCurrentMbrY;
	GetMotionData().GetCurrentMoveBlendRatio(fCurrentMbrX, fCurrentMbrY);

	float fDesiredMbrX, fDesiredMbrY;
	GetMotionData().GetDesiredMoveBlendRatio(fDesiredMbrX, fDesiredMbrY);

	TUNE_GROUP_FLOAT (DOG_MOVEMENT, fDogAccelRate, 2.0f, 0.0f, 10.0f, 0.0001f);

	InterpValue(fCurrentMbrX, fDesiredMbrX, fDogAccelRate);
	InterpValue(fCurrentMbrY, fDesiredMbrY, fDogAccelRate);

	GetMotionData().SetCurrentMoveBlendRatio(fCurrentMbrY, fCurrentMbrX);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDogLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

bool	CTaskDogLocomotion::ProcessPostMovement()
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
CTask * CTaskDogLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskDogLocomotion::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDogLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds)
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



// Will clipSetId our current heading based on our desired heading and max heading turn rate
void CTaskDogLocomotion::SetNewHeading( const float assistanceAngle )
{
	TUNE_GROUP_FLOAT (DOG_MOVEMENT, fDogTurnDiscrepancyTurnRate, 0.12f, 0.0f, 10.0f, 0.0001f);
	TUNE_GROUP_FLOAT (DOG_MOVEMENT, fMaxDesiredDogTurnForHeadingLerp, 0.001f, 0.0f, PI, 0.0001f);

	CPed* pPed = GetPed();

	if( pPed && !pPed->GetUsingRagdoll() && !pPed->IsDead() )
	{
		// Get the delta
		float fHeadingDelta = SubtractAngleShorter( pPed->GetDesiredHeading(), pPed->GetCurrentHeading() );

		// Do we need assistance? - if so subtract the dead zone (e.g. add assistance angle in -ve domain) and 
		// scale as heading increases
		if ( fHeadingDelta < -assistanceAngle )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( ( fHeadingDelta + assistanceAngle ) * fDogTurnDiscrepancyTurnRate );
		}
		// Else did we enter the dead zone
		else if ( fHeadingDelta >= -fMaxDesiredDogTurnForHeadingLerp && fHeadingDelta <= fMaxDesiredDogTurnForHeadingLerp )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );

			// Pretend we are bang on target
			fHeadingDelta = 0.0f;

			// Direct pointing
			pPed->SetHeading(pPed->GetDesiredHeading()); 
		}
		else if ( fHeadingDelta > assistanceAngle )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( ( fHeadingDelta - assistanceAngle ) * fDogTurnDiscrepancyTurnRate );
		}
		else
		{
			// Default behaviour is just to make sure there is no 
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );
		}

		// Always drive with a signal
		m_networkHelper.SetFloat( ms_MoveDirectionId, ConvertRadianToSignal( fHeadingDelta ) ); 
	}
}



//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateInitial_OnEnter()
{
	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDog);

	// stream in the movement clip set
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDogLocomotion::ShouldBeMoving()
{
	return GetMotionData().GetCurrentMbrY() > 0.0f;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateInitial_OnUpdate()
{
	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(pSet); 

	if (pSet && pSet->Request() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootDog))
	{	
		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootDog);
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

CTask::FSM_Return	CTaskDogLocomotion::StateLocomotion_OnEnter()
{
	m_networkHelper.SendRequest( ms_startLocomotionId);
	m_networkHelper.WaitForTargetState(ms_OnEnterLocomotionId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateLocomotion_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// Start to apply assistance after 45 degrees
	SetNewHeading( PI * 0.25f );

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

CTask::FSM_Return	CTaskDogLocomotion::StateTurnL_OnEnter()
{
	m_networkHelper.SendRequest( ms_startTurnLId);
	m_networkHelper.WaitForTargetState(ms_OnEnterTurnLId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateTurnL_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// When the clip has completed, decide what to do next
	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() && !pPed->GetIsAnyFixedFlagSet() )
		{
			const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetDesiredHeading(), GetPed()->GetCurrentHeading());

			if( (fHeadingDelta < (DtoR * MIN_ANGLE_TO_FACING)) ||	// Within our facing delta
				(fHeadingDelta < 0.0f) ||							// Overshoot
				(m_networkHelper.GetBoolean( ms_OnExitTurnId )) )	// Has completed
			{
				SetState( State_Idle );
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateTurnR_OnEnter()
{
	m_networkHelper.SendRequest( ms_startTurnRId );
	m_networkHelper.WaitForTargetState( ms_OnEnterTurnRId );
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateTurnR_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// When the clip has completed, decide what to do next
	CPed *pPed = GetPed();
	if (pPed)
	{
		if (m_networkHelper.IsNetworkActive() && !pPed->GetIsAnyFixedFlagSet())
		{
			const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetDesiredHeading(), GetPed()->GetCurrentHeading());

			if( (fHeadingDelta > (DtoR * -MIN_ANGLE_TO_FACING)) ||  // Within delta
				(fHeadingDelta > 0.0f) ||							// Overshoot
				(m_networkHelper.GetBoolean( ms_OnExitTurnId ) ) )	// Completed
			{
				SetState(State_Idle);
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateIdle_OnEnter()
{
	m_networkHelper.SendRequest( ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskDogLocomotion::StateIdle_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	GetPed()->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );

	if ( ShouldBeMoving() )
	{	
		const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetDesiredHeading(), GetPed()->GetCurrentHeading());

		// 90 degree turn stuff
		if ( fHeadingDelta < ( DtoR * -MIN_ANGLE_TO_FACING ) )
		{
			SetState(State_TurnR);
		}
		else 
			if ( fHeadingDelta > ( DtoR * MIN_ANGLE_TO_FACING ) )
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

void CTaskDogLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	static bank_float sf_PitchLimitTuning = 1.0f;
	fMinOut = -sf_PitchLimitTuning;
	fMaxOut =  sf_PitchLimitTuning;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char* CTaskDogLocomotion::GetStateName(s32 iState) const
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

void CTaskDogLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////