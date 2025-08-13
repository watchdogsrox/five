#include "TaskMotionStrafing.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"

#include "Debug/DebugScene.h"
#include "Animation/MovePed.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/CamInterface.h"
#include "Camera/scripted/ScriptDirector.h"
#include "Peds/Action/BrawlingStyle.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	CTaskMotionStrafing methods
//	This is the top level ped movement task. Within it run individual
//	tasks for basic movement / strafing , etc
//////////////////////////////////////////////////////////////////////////

const fwMvFloatId CTaskMotionStrafing::ms_StrafeDirectionId("StrafeDirection",0xCF6AA9C6);
const fwMvFloatId CTaskMotionStrafing::ms_StrafeSpeedId("StrafeSpeed",0x59A5FA46);
const fwMvFloatId CTaskMotionStrafing::ms_DesiredStrafeSpeedId("DesiredStrafeSpeed",0x8E945F2D);
const fwMvFloatId CTaskMotionStrafing::ms_StrafingIdleTurnRateId("TurnRate",0x58AF9DCD);
const fwMvFloatId CTaskMotionStrafing::ms_DesiredHeadingId("DesiredHeading",0x5B256E29);
const fwMvFloatId CTaskMotionStrafing::ms_InstantPhaseSyncId("InstantPhaseSync",0x2F41A132);
const fwMvBooleanId CTaskMotionStrafing::ms_IdleIntroOnEnterId("OnEnterIdleIntro",0xBFAFAB7F);
const fwMvBooleanId CTaskMotionStrafing::ms_IdleOnEnterId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskMotionStrafing::ms_MovingOnEnterId("OnEnterMoving",0x023289d8);
const fwMvBooleanId CTaskMotionStrafing::ms_BlendOutIdleIntroId("OnIdleIntroEnd",0x24A95AB9);
const fwMvRequestId CTaskMotionStrafing::ms_IdleRequestId("Idle",0x71C21326);
const fwMvRequestId CTaskMotionStrafing::ms_MovingRequestId("Moving",0xDF719C6C);
const fwMvRequestId CTaskMotionStrafing::ms_RestartUpperbodyAnimId("RestartUpperbodyAnim",0xD6B42FAD);
const fwMvFlagId CTaskMotionStrafing::ms_UseUpperbodyAnimId("UseUpperbodyAnim",0x30FD1A87);
const fwMvClipSetVarId CTaskMotionStrafing::ms_UpperbodyAnimClipSetId("UpperbodyAnim",0xA5DD33A3);

CTaskMotionStrafing::CTaskMotionStrafing( u16 initialState )
: CTaskMotionBase()
, m_initialState( initialState )
, m_fTurnIdleSignal( FLT_MAX )
, m_fDesiredStrafeDirection( FLT_MAX )
, m_fStrafeDirection( FLT_MAX )
, m_fInstantPhaseSync( 0.0f )
, m_UpperbodyClipSetId(CLIP_SET_ID_INVALID)
, m_bUpperbodyClipSetChanged(false)
, m_bShouldPhaseSync(false)
, m_bIsInStealthMode(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_STRAFING);

	for(s32 i = 0; i < MAX_ACTIVE_STATE_TRANSITIONS; i++)
	{
		m_uStateTransitionTimes[i] = 0;
	}
}

CTaskMotionStrafing::~CTaskMotionStrafing()
{
}

CTask::FSM_Return CTaskMotionStrafing::ProcessPreFSM( void )
{
	ProcessMovement();
	UpdateDesiredHeading( GetPed() );
	return FSM_Continue;
}

void CTaskMotionStrafing::ProcessMovement( void )
{
	CPed * pPed = GetPed();

	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();

	if(m_fStrafeDirection == FLT_MAX)
	{
		// Compute current strafe direction
		m_fStrafeDirection = 0.f;
		if(vCurrentMBR.Mag2() > 0.f)
		{
			m_fStrafeDirection = fwAngle::LimitRadianAngleSafe(rage::Atan2f(-vCurrentMBR.x, vCurrentMBR.y));
		}
	}

	// Compute desired strafe direction
	m_fDesiredStrafeDirection = 0.f;
	if(vDesiredMBR.Mag2() > 0.f)
	{
		m_fDesiredStrafeDirection = fwAngle::LimitRadianAngleSafe(rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y));
	}

	// Move current to desired
	static dev_float ANG_ACCEL = 4.f;
	m_fStrafeDirection = fwAngle::Lerp(m_fStrafeDirection, m_fDesiredStrafeDirection, ANG_ACCEL * GetTimeStep());

	float fCurrentMag = vCurrentMBR.Mag();
	float fDesiredMag = vDesiredMBR.Mag();

	static dev_float MOVE_ACCEL = 6.f;
	static dev_float MOVE_DECEL = 2.f;
	if(fDesiredMag > fCurrentMag)
	{
		Approach(fCurrentMag, fDesiredMag, MOVE_ACCEL, GetTimeStep());
	}
	else
	{
		Approach(fCurrentMag, fDesiredMag, MOVE_DECEL, GetTimeStep());
	}

	vCurrentMBR.x = fCurrentMag * -Sinf(m_fStrafeDirection);
	vCurrentMBR.y = fCurrentMag * Cosf(m_fStrafeDirection);

	// Copy variables back into moveblender.  This is a temporary measure.
	pPed->GetMotionData()->SetCurrentMoveBlendRatio( vCurrentMBR.y, vCurrentMBR.x );
	pPed->GetMotionData()->SetCurrentTurnVelocity( 0.0f );
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );
}

CTask::FSM_Return CTaskMotionStrafing::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	if(iEvent == OnEnter)
	{
		RecordStateTransition();
	}

	FSM_Begin

		FSM_State(State_NetworkSetup)
			FSM_OnUpdate
				return StateNetworkSetup_OnUpdate();

		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_IdleIntro)
			FSM_OnEnter
				return StateIdleIntro_OnEnter();
			FSM_OnUpdate
				return StateIdleIntro_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				return StateIdle_OnEnter();
			FSM_OnUpdate
				return StateIdle_OnUpdate();

		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();

	FSM_End
}

CTaskMotionStrafing::FSM_Return	CTaskMotionStrafing::ProcessPostFSM()
{
	SendParams();

	// Send signals valid for all states
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		if(m_bUpperbodyClipSetChanged)
		{
			if(m_UpperbodyClipSetId == CLIP_SET_ID_INVALID)
			{
				SendUpperbodyClipSet();
			}
			else
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_UpperbodyClipSetId);
				if(pClipSet)
				{
					if(pClipSet->Request_DEPRECATED())
					{
						SendUpperbodyClipSet();
					}
				}
			}
		}
	}

	return FSM_Continue;
}

CTask* CTaskMotionStrafing::CreatePlayerControlTask( void ) 
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

// In this case we can query the locomotion clips directly, and possibly cache the results.
void CTaskMotionStrafing::GetMoveSpeeds( CMoveBlendRatioSpeeds &speeds )
{
	taskAssert( GetClipSet() != CLIP_SET_ID_INVALID );

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(GetClipSet());
	taskAssert( clipSet );

	static const fwMvClipId s_walkClip( "walk",0x83504C9C );
	static const fwMvClipId s_runClip( "run",0x1109B569 );
	static const fwMvClipId s_sprintClip( "run",0x1109B569 );

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };
	RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	return;
}

void CTaskMotionStrafing::GetNMBlendOutClip( fwMvClipSetId& outClipSet, fwMvClipId& outClip )
{
	outClipSet = m_moveNetworkHelper.GetClipSetId();
	outClip = CLIP_IDLE;
}

void CTaskMotionStrafing::SendParams( void )
{
	if( m_moveNetworkHelper.IsNetworkActive() )
	{
		//calculate the strafing direction
		CPed* pPed = GetPed();

		const float fHeadingDelta = CalcDesiredDirection();

		// Idle Turn Params
		m_moveNetworkHelper.SetFloat(ms_StrafingIdleTurnRateId, ConvertRadianToSignal(m_fTurnIdleSignal));

		// Strafing Params
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();

		taskAssert( m_fStrafeDirection != FLT_MAX );
		float fStrafeSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( m_fStrafeDirection );
		m_moveNetworkHelper.SetFloat( ms_StrafeDirectionId, fStrafeSignal ) ;

		float fStrafeSpeed = vCurrentMBR.Mag() / MOVEBLENDRATIO_SPRINT;
		m_moveNetworkHelper.SetFloat( ms_StrafeSpeedId, fStrafeSpeed );

		float fDesiredStrafeSpeed = vDesiredMBR.Mag() / MOVEBLENDRATIO_SPRINT;
		m_moveNetworkHelper.SetFloat( ms_DesiredStrafeSpeedId, fDesiredStrafeSpeed );

		if( ShouldPhaseSync() )
		{
			m_moveNetworkHelper.SetFloat( ms_InstantPhaseSyncId, m_fInstantPhaseSync );
			ResetPhaseSync();
			
			// Force update the clip to play immediately - saves a frame delay
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		}

		float fDesiredHeadingSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( fHeadingDelta );
		m_moveNetworkHelper.SetFloat( ms_DesiredHeadingId, fDesiredHeadingSignal );


		// Set melee grip filter.
		if (pPed->GetEquippedWeaponInfo() && (pPed->GetEquippedWeaponInfo()->GetIsMeleeFist() || pPed->GetEquippedWeaponInfo()->GetPairedWeaponHolsterAnimation()))
		{
			crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(m_moveNetworkHelper.GetClipSetId());
			if (pFilter)
			{
				m_moveNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
			}
		}
	}
}

bool CTaskMotionStrafing::StartMoveNetwork( void )
{
	if( !m_moveNetworkHelper.IsNetworkActive() )
	{
		// request the network def
		m_moveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskOnFootStrafing );
		aiAssert( GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_MOTION_PED );
		
		// stream in the movement clip clipSetId
		fwClipSet* pSet = fwClipSetManager::GetClipSet( GetClipSet() );
		taskAssert( pSet );

		if( pSet->Request_DEPRECATED() && m_moveNetworkHelper.IsNetworkDefStreamedIn( GetMoveNetworkDefId() ) )
		{
			// Start the on-foot move network
			m_moveNetworkHelper.CreateNetworkPlayer( GetPed(), GetMoveNetworkDefId() );

#if __ASSERT
			static u32 sMaleGenericClipSetHash = ATSTRINGHASH("move_m@generic", 0x7E333C8);
			taskAssertf(GetClipSet().GetHash() != sMaleGenericClipSetHash, "Setting incorrect clip set in CTaskMotionStrafing::StartMoveNetwork");
#endif
			m_moveNetworkHelper.SetClipSet( GetClipSet() );
		}
	}

	if ( m_moveNetworkHelper.IsNetworkActive() )
	{
		if (!m_moveNetworkHelper.IsNetworkAttached())
		{
			m_moveNetworkHelper.DeferredInsert();
			SendParams();
		}

		if( m_initialState > State_NetworkSetup )
		{
			static const fwMvStateId s_moveStateIds[State_Moving+1] = 
			{
				fwMvStateId(),
				fwMvStateId("Initial",0x8C56D5FD),
				fwMvStateId("IdleIntro",0x822A9119),
				fwMvStateId("Idle",0x71C21326),
				fwMvStateId("Moving",0xDF719C6C)
			};

			static fwMvStateId stateMachine("OnFootStrafingTagSync",0xBDF615A3);
			SetState( m_initialState );
			m_moveNetworkHelper.ForceStateChange( s_moveStateIds[ m_initialState ], stateMachine );
		}
		else
		{
			SetState( State_Initial );
		}

		return true;		
	}
	
	return false;
}

void CTaskMotionStrafing::SetUpperbodyClipSet(const fwMvClipSetId& clipSetId)
{
	if(m_UpperbodyClipSetId != clipSetId )
	{
		// Flag the change
		m_bUpperbodyClipSetChanged = true;

		// Store the new clip set
		m_UpperbodyClipSetId = clipSetId;
	}
}

//////////////////////////////////////////////////////////////////////////
#if !__FINAL

const char * CTaskMotionStrafing::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_NetworkSetup: return "NetworkSetup";
	case State_Initial: return "Initial";
	case State_IdleIntro: return "IdleIntro";
	case State_Idle: return "Idle";
	case State_Moving: return "Moving";
	default: { Assert(false); return "Unknown"; }
	}
}

#endif //!__FINAL

CTask::FSM_Return CTaskMotionStrafing::StateNetworkSetup_OnUpdate( void )
{
	if( StartMoveNetwork() )
	{
		// Reset Motion Params
		m_fDesiredStrafeDirection = FLT_MAX;
		m_fStrafeDirection = FLT_MAX;
		// Reset moving params
		ProcessMovement();
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionStrafing::StateInitial_OnUpdate( void )
{
	if( m_moveNetworkHelper.GetBoolean( ms_IdleIntroOnEnterId ) )
	{
		SetState( State_IdleIntro );
	}
	else if( m_moveNetworkHelper.GetBoolean( ms_IdleOnEnterId ) )
	{
		SetState( State_Idle );
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	IdleIntro
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionStrafing::StateIdleIntro_OnEnter( void )
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionStrafing::StateIdleIntro_OnUpdate( void )
{
	if( GetWantsToMove() )
	{
		SetState( State_Moving );
		return FSM_Continue;
	}
	
	// Should we start blending out?
	if( m_moveNetworkHelper.GetBoolean( ms_BlendOutIdleIntroId ) )
	{
		SetState( State_Idle );
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	Basic locomotion
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionStrafing::StateIdle_OnEnter( void )
{
	m_moveNetworkHelper.SendRequest( ms_IdleRequestId );
	m_moveNetworkHelper.WaitForTargetState( ms_IdleOnEnterId );
	m_fTurnIdleSignal = 0.0f;
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionStrafing::StateIdle_OnUpdate( void )
{
	const float fHeadingDelta = CalcDesiredDirection();

	TUNE_GROUP_FLOAT(PED_MOVEMENT, MELEE_STRAFE_IDLE_TURN_RATE, 6.28f, 0.0f, 10.0f, 0.1f);
	static dev_float ANGLE_CLAMP = DtoR * 36.f;
	float fIdleTurnTarget = Clamp(fHeadingDelta, -ANGLE_CLAMP, ANGLE_CLAMP);
	m_fTurnIdleSignal = fwAngle::Lerp(m_fTurnIdleSignal, fIdleTurnTarget, MELEE_STRAFE_IDLE_TURN_RATE * GetTimeStep());

	if( !m_moveNetworkHelper.IsInTargetState() )
	{
		return FSM_Continue;
	}

	const bool bCanChangeState = CanChangeState();
	if( bCanChangeState )
	{
		if( GetWantsToMove() )
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;
			SetState( State_Moving );
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	Strafing
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionStrafing::StateMoving_OnEnter( void )
{
	m_moveNetworkHelper.SendRequest( ms_MovingRequestId );
	m_moveNetworkHelper.WaitForTargetState( ms_MovingOnEnterId );
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionStrafing::StateMoving_OnUpdate( void )
{
	if( !m_moveNetworkHelper.IsInTargetState() )
	{
		return FSM_Continue;
	}

	const bool bCanChangeState = CanChangeState();
	if( bCanChangeState )
	{
		if( !GetWantsToMove() )
		{
			SetState( State_Idle );
		}
	}

	return FSM_Continue;
}

void CTaskMotionStrafing::UpdateDesiredHeading( CPed* pPed )
{
	taskAssertf( pPed, "CTaskMotionStrafing::UpdateDesiredHeading - Invalid ped." );

	// Apply some extra steering here
	float fDesiredHeading = pPed->GetDesiredHeading();

	//Update heading to camera
	if( !pPed->GetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning ) )
	{
		const bool bIsAssistedAiming = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

		if (pPed->IsLocalPlayer())
		{
			// Update the ped's (desired) heading
			CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
			if (bIsAssistedAiming && targeting.GetLockOnTarget())
			{
				Vector3 vLockOnPos(Vector3::ZeroType);
				targeting.GetLockOnTarget()->GetLockOnTargetAimAtPos(vLockOnPos);
				pPed->SetDesiredHeading(vLockOnPos);	
			}
			else
			{
				const camFrame& frame = camInterface::GetPlayerControlCamAimFrame();
				pPed->SetDesiredHeading(frame.ComputeHeading());			
			}

			fDesiredHeading	= pPed->GetDesiredHeading();
		}
	}

	if( pPed->GetPedResetFlag( CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading ) )
	{
		TUNE_GROUP_FLOAT( PED_MOVEMENT, STRAFE_IDLE_HEADING_CHANGE_RATE, 1.0f, 0.0f, 20.0f, 0.001f );
		TUNE_GROUP_FLOAT( PED_MOVEMENT, STRAFE_MOVE_HEADING_CHANGE_RATE, 4.0f, 0.0f, 20.0f, 0.001f );

		float fHeadingChangeRate = STRAFE_MOVE_HEADING_CHANGE_RATE;
		if( GetState() == State_Idle )
			fHeadingChangeRate = STRAFE_IDLE_HEADING_CHANGE_RATE;

		float fCurrentHeading = pPed->GetCurrentHeading();
		float fHeadingDelta = InterpValue( fCurrentHeading, fDesiredHeading, fHeadingChangeRate, true );
		fHeadingDelta += GetMotionData().GetExtraHeadingChangeThisFrame();	
		GetMotionData().SetExtraHeadingChangeThisFrame( fHeadingDelta );
	}
	else
		GetMotionData().SetExtraHeadingChangeThisFrame( 0.0f );
}

bool CTaskMotionStrafing::CanRestartTask( CPed* pPed )
{
	switch(GetState())
	{
	case(State_Idle):
	case(State_Moving):
		return HasStealthModeChanged(pPed);
	default:
		return false;
	}
}

float CTaskMotionStrafing::GetStandingSpringStrength() 
{
	float fSpringStrength = CTaskMotionBase::GetStandingSpringStrength();
	
	//! Make it harder for peds to leave the ground when a ped is melee homing. As it was, the ped could increase their speed a lot, which could cause lift off
	//! in some situations (e.g. on a ramp, stairs etc)
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning))
	{
		CTaskMeleeActionResult* pMeleeActionResult = GetPed()->GetPedIntelligence() ? GetPed()->GetPedIntelligence()->GetTaskMeleeActionResult() : NULL;
		if( pMeleeActionResult && pMeleeActionResult->DidAllowDistanceHoming())
		{
			TUNE_GROUP_FLOAT(MELEE_TEST, fMeleeSpringMultiplier, 6.0f, 0.01f, 8.0f, 0.1f);
			fSpringStrength *= fMeleeSpringMultiplier;	
		}
	}
	
	return fSpringStrength;
}

bool CTaskMotionStrafing::HasStealthModeChanged( CPed* pPed )
{
	bool bStealthMode = ShouldPedBeInStealthMode( pPed );
	if( bStealthMode != IsInStealthMode() )
	{
		return true;
	}

	return false;
}

bool CTaskMotionStrafing::ShouldPedBeInStealthMode( CPed* pPed )
{
	if (pPed->IsLocalPlayer())
	{
		// Update the ped's (desired) heading
		CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
		CEntity* pTargetEntity = targeting.GetLockOnTarget();
		
		if( pTargetEntity )
		{
			Vector3 vPlayerToTarget = VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition() );
			vPlayerToTarget.NormalizeFast();
			float fPlayerToTargetDirection = DotProduct( vPlayerToTarget, VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetB() ) );

			// Are we in front of the ped?
			if( fPlayerToTargetDirection > CTaskMelee::sm_fFrontTargetDotThreshold )
			{
				return pPed->GetMotionData()->GetUsingStealth();
			}
		}
		else if(pPed->GetMotionData()->GetUsingStealth())
		{
			return true;
		}
	}
	//! Clones just use stealth flag.
	else if(pPed->IsNetworkClone())
	{
		return pPed->GetMotionData()->GetUsingStealth();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

const fwMvNetworkDefId& CTaskMotionStrafing::GetMoveNetworkDefId( void ) const
{
	return CClipNetworkMoveInfo::ms_NetworkTaskOnFootStrafing;
}

fwMvClipSetId CTaskMotionStrafing::GetClipSet( void )
{
	CPed* pPed = GetPed();
	m_bIsInStealthMode = ShouldPedBeInStealthMode( pPed );

	// now have to query the task tree itself here because the queriable interface isn't up to date quickly enough
	// (we've eliminated the frame delay brought about by starting the move network)
	if( pPed->GetPedIntelligence()->IsPedInMelee() || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMeleeStrafingAnims) || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE))
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( pWeaponInfo )
		{
			if( IsInStealthMode() && pWeaponInfo->GetPedMotionStrafingStealthClipSetId(*pPed) != CLIP_SET_ID_INVALID )
				return pWeaponInfo->GetPedMotionStrafingStealthClipSetId(*pPed);
			else if( pWeaponInfo->GetPedMotionStrafingClipSetId(*pPed) != CLIP_SET_ID_INVALID )
				return pWeaponInfo->GetPedMotionStrafingClipSetId(*pPed);
		}

		return fwMvClipSetId( CLIP_SET_MOVE_MELEE );
	}

	if( pPed->GetMotionData()->GetIsCrouching() )
	{
		return fwMvClipSetId( "move_ped_crouched_strafing",0x92B35B93 );
	}

	CTaskMotionBase* pedTask = GetPed()->GetPrimaryMotionTask();
	if( pedTask )
	{
		fwMvClipSetId clipSetId = pedTask->GetOverrideStrafingClipSet();
		if( clipSetId != CLIP_SET_ID_INVALID )
		{
			return clipSetId;
		}
	}

	return fwMvClipSetId( "move_ped_strafing",0x92EC5666 );
}

void CTaskMotionStrafing::SendUpperbodyClipSet()
{
	m_moveNetworkHelper.SetClipSet(m_UpperbodyClipSetId, ms_UpperbodyAnimClipSetId);
	m_moveNetworkHelper.SetFlag(m_UpperbodyClipSetId == CLIP_SET_ID_INVALID ? false : true, ms_UseUpperbodyAnimId);
	m_moveNetworkHelper.SendRequest(ms_RestartUpperbodyAnimId);
	m_bUpperbodyClipSetChanged = false;
}

bool CTaskMotionStrafing::GetWantsToMove() const
{
	static dev_float MOVING_SPEED = 0.1f;
	return GetPed()->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square( MOVING_SPEED );
}

void CTaskMotionStrafing::RecordStateTransition()
{
	for(s32 i = MAX_ACTIVE_STATE_TRANSITIONS-1; i > 0; i--)
	{
		m_uStateTransitionTimes[i] = m_uStateTransitionTimes[i-1];
	}
	m_uStateTransitionTimes[0] = fwTimer::GetTimeInMilliseconds();
}

bool CTaskMotionStrafing::CanChangeState() const
{
	static dev_u32 TIME_BEFORE_STATE_CHANGE = 300;
	return (m_uStateTransitionTimes[MAX_ACTIVE_STATE_TRANSITIONS-1] + TIME_BEFORE_STATE_CHANGE) <= fwTimer::GetTimeInMilliseconds();
}
