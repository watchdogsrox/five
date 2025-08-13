#include "TaskFlightlessBirdLocomotion.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"


#include "animation/MovePed.h"
#include "Debug/DebugScene.h"
#include "Task/Default/TaskPlayer.h"
#include "task/Motion/Locomotion/TaskBirdLocomotion.h"



AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Flightless Bird Locomotion
//////////////////////////////////////////////////////////////////////////
const fwMvRequestId CTaskFlightlessBirdLocomotion::ms_startIdleId("StartIdle",0x2A8F6981);
const fwMvBooleanId CTaskFlightlessBirdLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);

const fwMvRequestId CTaskFlightlessBirdLocomotion::ms_startWalkId("StartWalk",0xD999A89E);
const fwMvBooleanId CTaskFlightlessBirdLocomotion::ms_OnEnterWalkId("OnEnterWalk",0x229ED4B);

const fwMvRequestId CTaskFlightlessBirdLocomotion::ms_startRunId("StartRun",0xDAB5D4EB);
const fwMvBooleanId CTaskFlightlessBirdLocomotion::ms_OnEnterRunId("OnEnterRun",0x1AFFF503);

const fwMvFloatId CTaskFlightlessBirdLocomotion::ms_DirectionId("Direction",0x34DF9828);

const fwMvFloatId CTaskFlightlessBirdLocomotion::ms_IdleRateId("Rate",0x7E68C088);

dev_float CTaskFlightlessBirdLocomotion::ms_fMaxPitch = HALF_PI * 0.75f;

CTaskFlightlessBirdLocomotion::CTaskFlightlessBirdLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
, m_fTurnDiscrepancyTurnRate(0.12f)
, m_fDesiredTurnForHeadingLerp(0.001f) 
, m_bMoveInTargetState(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_FLIGHTLESS_BIRD);
}

//////////////////////////////////////////////////////////////////////////

CTaskFlightlessBirdLocomotion::~CTaskFlightlessBirdLocomotion()
{

}


//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFlightlessBirdLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				StateIdle_OnEnter(); 
			FSM_OnUpdate
				return StateIdle_OnUpdate();

		FSM_State(State_Walk)
			FSM_OnEnter
				StateWalk_OnEnter(); 
			FSM_OnUpdate
				return StateWalk_OnUpdate(); 

		FSM_State(State_Run)
			FSM_OnEnter
				StateRun_OnEnter(); 
			FSM_OnUpdate
				return StateRun_OnUpdate(); 
	FSM_End
}

bool CTaskFlightlessBirdLocomotion::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_Idle:
		{
			StateIdle_OnProcessMoveSignals();
			return true;
		}
		case State_Walk:
		{
			StateWalk_OnProcessMoveSignals();
			return true;
		}
		case State_Run:
		{
			StateRun_OnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFlightlessBirdLocomotion::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskFlightlessBirdLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (clipSet)
	{
		static const fwMvClipId s_walkClip("walk",0x83504C9C);
		static const fwMvClipId s_runClip("run",0x1109B569);

		const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_runClip };

		RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	}

	return;
}

void CTaskFlightlessBirdLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	if (GetPed()->IsAPlayerPed())
	{
		fMinOut = -ms_fMaxPitch; 
		fMaxOut = ms_fMaxPitch;
	}
	else
	{
		CTaskMotionBase::GetPitchConstraintLimits(fMinOut, fMaxOut);
	}
}


// Will clipSetId our current heading based on our desired heading and max heading turn rate
void CTaskFlightlessBirdLocomotion::SetNewHeading( const float assistanceAngle )
{
	TUNE_GROUP_FLOAT (FLIGHTLESS_BIRD_MOVEMENT, fChickenTurnDiscrepancyTurnRate, 0.12f, 0.0f, 10.0f, 0.0001f);
	TUNE_GROUP_FLOAT (FLIGHTLESS_BIRD_MOVEMENT, fMaxDesiredChickenTurnForHeadingLerp, 0.001f, 0.0f, PI, 0.0001f);


	CPed* pPed = GetPed();

	if( pPed && !pPed->GetUsingRagdoll() && !pPed->IsDead() )
	{
		// Get the delta
		float fHeadingDelta = SubtractAngleShorter( pPed->GetDesiredHeading(), pPed->GetCurrentHeading() );

		// Do we need assistance? - if so subtract the dead zone (e.g. add assistance angle in -ve domain) and 
		// scale as heading increases
		if ( fHeadingDelta < -assistanceAngle )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( ( fHeadingDelta + assistanceAngle ) * fChickenTurnDiscrepancyTurnRate );
		}
		// Else did we enter the dead zone
		else if ( fHeadingDelta >= -fMaxDesiredChickenTurnForHeadingLerp && fHeadingDelta <= fMaxDesiredChickenTurnForHeadingLerp )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );

			// Pretend we are bang on target
			fHeadingDelta = 0.0f;

			// Direct pointing

			if (!pPed->GetIsAttached())
			{
				pPed->SetHeading(pPed->GetDesiredHeading()); 
			}
		}
		else if ( fHeadingDelta > assistanceAngle )
		{
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( ( fHeadingDelta - assistanceAngle ) * fChickenTurnDiscrepancyTurnRate );
		}
		else
		{
			// Default behaviour is just to make sure there is no extra heading change.
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );
		}

		// Hack to increase the desired angle to force the animation to turn more dramatically.
		fHeadingDelta *= 2.0f;	//MAGIC
		fHeadingDelta = Clamp(fHeadingDelta, -PI, PI);

		// Always drive with a signal
		m_networkHelper.SetFloat( ms_DirectionId, ConvertRadianToSignal( fHeadingDelta ) ); 
	}
}

bool CTaskFlightlessBirdLocomotion::IsInWaterWhileLowLod() const
{
	const CPed* pPed = GetPed();
	return pPed->GetIsSwimming() && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics);
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

void	CTaskFlightlessBirdLocomotion::StateInitial_OnEnter()
{
	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootFlightlessBird);
	// stream in the movement clip clipSetId

	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFlightlessBirdLocomotion::StateInitial_OnUpdate()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(clipSet); 

	if (clipSet && clipSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootFlightlessBird))
	{	
		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootFlightlessBird);

		m_networkHelper.SetClipSet( m_clipSetId); 

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();
		move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);

		CPed* pPed = GetPed(); 

		if (abs(pPed->GetMotionData()->GetCurrentMbrY())>0.0f)
		{
			//enter the basic locomotion state by default
			SetState(State_Walk);
		}
		else
		{
			SetState(State_Idle);
			m_networkHelper.ForceNextStateReady();
		}
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFlightlessBirdLocomotion::StateWalk_OnEnter()
{
	m_networkHelper.SendRequest(ms_startWalkId);
	m_networkHelper.WaitForTargetState(ms_OnEnterWalkId);

	// Reset cached MoVE variables.
	m_bMoveInTargetState = false;

	// Request that MoVE send signals back to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFlightlessBirdLocomotion::StateWalk_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	AllowTimeslicing();

	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		CTaskBirdLocomotion::SetNewGroundPitchForBird(*pPed, GetTimeStep());
	}

	if (abs(pPed->GetMotionData()->GetDesiredMbrY()) == 0.0f)
	{	
		SetState(State_Idle);
	}
	else
	{
		if(abs(pPed->GetMotionData()->GetDesiredMbrY()) > 1.5f)
		{
			SetState(State_Run);
		}
	}

	if (IsInWaterWhileLowLod())
	{
		// Block the low physics flag so the qped can drown, etc.
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
	}

	return FSM_Continue;
}

void CTaskFlightlessBirdLocomotion::StateWalk_OnProcessMoveSignals()
{
	// Send heading signals.
	SetNewHeading(PI * 0.25f);

	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFlightlessBirdLocomotion::StateRun_OnEnter()
{
	m_networkHelper.SendRequest(ms_startRunId);
	m_networkHelper.WaitForTargetState(ms_OnEnterRunId);

	// Reset cached MoVE variables.
	m_bMoveInTargetState = false;

	// Request that MoVE send signals back to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskFlightlessBirdLocomotion::StateRun_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	AllowTimeslicing();

	CPed* pPed = GetPed();
	if (pPed->IsAPlayerPed())
	{
		CTaskBirdLocomotion::SetNewGroundPitchForBird(*pPed, GetTimeStep());
	}

	if(abs(pPed->GetMotionData()->GetDesiredMbrY()) < 1.5f)
	{	
		SetState(State_Walk);
	}

	if (IsInWaterWhileLowLod())
	{
		// Block the low physics flag so the qped can drown, etc.
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
	}


	return FSM_Continue;
}

void CTaskFlightlessBirdLocomotion::StateRun_OnProcessMoveSignals()
{
	// Send heading signals.
	SetNewHeading(PI * 0.25f);

	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFlightlessBirdLocomotion::StateIdle_OnEnter()
{
	GetPed()->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	// Adjust the clip rate of idle, so pecking isn't in sync across multiple hens
	////////////////////////////////////////////////////////////////////////////////
	const float fRandomAmount = ((((float)GetPed()->GetRandomSeed()) / (float)RAND_MAX_16) * 0.2f) - 0.1f;
	m_networkHelper.SetFloat( ms_IdleRateId, 1.0f + fRandomAmount );
	////////////////////////////////////////////////////////////////////////////////

	m_networkHelper.SendRequest(ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);

	// Reset cached MoVE variables.
	m_bMoveInTargetState = false;

	// Request that MoVE send signals back to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFlightlessBirdLocomotion::StateIdle_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	AllowTimeslicing();

	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		CTaskBirdLocomotion::SetNewGroundPitchForBird(*pPed, GetTimeStep());
	}

	if (abs(pPed->GetMotionData()->GetDesiredMbrY()) > 0.0f)
	{	
		SetState(State_Walk);
	}	
	return FSM_Continue;
}

void CTaskFlightlessBirdLocomotion::StateIdle_OnProcessMoveSignals()
{
	// We have no idle turns at moment so apply assistance if an angle is required.
	SetNewHeading(PI/180.0f);

	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

CTask * CTaskFlightlessBirdLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFlightlessBirdLocomotion::AllowTimeslicing()
{
	// For humans with AL_LodMotionTask, CTaskMotionBasicLocomotionLowLod would be running
	// and enable timeslicing (unblocking AL_LodTimesliceIntelligenceUpdate). For animals,
	// we don't use that task, so we manually turn on timeslicing here.
	CPed* pPed = GetPed();
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskFlightlessBirdLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Idle: return "Idle";
	case State_Walk: return "Walk";
	case State_Run: return "Run";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskFlightlessBirdLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
