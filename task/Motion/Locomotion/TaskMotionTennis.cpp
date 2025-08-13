#include "TaskMotionTennis.h"

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
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	CTaskMotionTennis methods
//	This is the top level ped movement task. Within it run individual
//	tasks for basic movement / strafing , etc
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskMotionTennis, 0x5aa4e568);
CTaskMotionTennis::Tunables CTaskMotionTennis::ms_Tunables;

const fwMvFloatId CTaskMotionTennis::ms_StrafeDirectionId("StrafeDirection",0xCF6AA9C6);
const fwMvFloatId CTaskMotionTennis::ms_StrafeSpeedId("StrafeSpeed",0x59A5FA46);
const fwMvFloatId CTaskMotionTennis::ms_BackwardsStrafeBlend("BackwardsBlend",0x71143DAE);
const fwMvFloatId CTaskMotionTennis::ms_DesiredStrafeSpeedId("DesiredStrafeSpeed",0x8E945F2D);
const fwMvFloatId CTaskMotionTennis::ms_StrafingRateId("StrafingRate",0xB000921B);
const fwMvFloatId CTaskMotionTennis::ms_DesiredStrafeDirectionId("DesiredStrafeDirection",0x3C9615E9);
const fwMvFloatId CTaskMotionTennis::ms_StrafeStopDirectionId("StrafeStopDirection",0x9E0F20B3);
const fwMvFloatId CTaskMotionTennis::ms_StrafeStartDirectionId("StrafeStartDirection",0x7B68EA97);

const fwMvRequestId CTaskMotionTennis::ms_LocoStartId("LocoStart",0xC048ABAC);
const fwMvRequestId CTaskMotionTennis::ms_LocoStopId("LocoStop",0x80FC3B31);

const fwMvBooleanId CTaskMotionTennis::ms_InterruptMoveId("InterruptMove",0x4E0A2023);
const fwMvBooleanId CTaskMotionTennis::ms_InterruptStopId("InterruptStop",0xB1DF2121);
const fwMvBooleanId CTaskMotionTennis::ms_InterruptSwingId("InterruptSwing",0xFBFEC0E9);
const fwMvBooleanId CTaskMotionTennis::ms_SwungId("Swung",0xFCED5ADF);

const fwMvClipId CTaskMotionTennis::ms_SwingClipId("ShotAnimClip",0xC1F30649);
const fwMvRequestId CTaskMotionTennis::ms_SwingRequestId("Swing",0xA488B638);
const fwMvFloatId CTaskMotionTennis::ms_PhaseFloatId("ShotStartPhase",0xE7BD1051);
const fwMvFloatId CTaskMotionTennis::ms_PlayRateFloatId("ShotPlayRate",0x3F26788A);

const fwMvFlagId CTaskMotionTennis::ms_DiveBHId("DiveBH",0xFE3F9BAC);
const fwMvFlagId CTaskMotionTennis::ms_DiveFHId("DiveFH",0xD80F4AB8);
const fwMvFloatId CTaskMotionTennis::ms_DiveHorizontalFloatId("DiveHorizontal",0xF87CAEFE);
const fwMvFloatId CTaskMotionTennis::ms_DiveVerticalFloatId("DiveVertical",0x6EA8921C);

const fwMvFlagId CTaskMotionTennis::ms_SlowBlendId("SlowBlend",0x67EFE4F4);

fwMvClipSetId CTaskMotionTennis::defaultClipSetId(fwMvClipSetId("TENNIS_LOCOMOTION",0xD80F58C9));
fwMvClipSetId CTaskMotionTennis::defaultClipSetIdFemale(fwMvClipSetId("TENNIS_LOCOMOTION_FEMALE",0x71E42D58));

CTaskMotionTennis::CTaskMotionTennis(bool bUseFemaleClipSet, bool bAllowCloneOverride)
	: CTaskMotionBase()
, m_initialState( 0 )
, m_fDesiredBackwardsBlend( -1.0f )
, m_fCurrentBackwardsBlend( -1.0f )
, m_fStrafeSpeed( -1.0f )
, m_fDesiredStrafeDirection( -1.0f )
, m_fStrafeDirection( -1.0f )
, m_fStrafeStartDirection( -1.0f)
, m_fStrafeStopDirection( -1.0f)
, m_bOverrideStrafeDirectionWithStopDirection(false)
, m_bUseDampening(false)
, m_bAllowDampen(true)
, m_bCWStart(false)
, m_bRequestStart(false)
, m_bRequestStop(false)
, m_bRequestSwing(false)
, m_CloneStartPhase(-1.0f)
, m_ClonePlayRate(-1.0f)
, m_bDiveMode(false)
, m_bDiveDir(false)
, m_bCloneSlowBlend(false)
, m_bAllowOverrideCloneUpdate(bAllowCloneOverride)
, m_bForceResendSyncData(false)
, m_bControlOutOfDeadZone(false)
, m_fCloneDiveHorizontal(-1.0f)
, m_fCloneDiveVertical(-1.0f)
, m_bUseFemaleClipSet(bUseFemaleClipSet)
{
	//The tennis motion task currently always considers the ped "on foot".
	//This means that the clip set used in network serialization is going to be the default on foot clip set.
	//Set the default on foot clip set here, to ensure a good clip set is serialized when the tennis motion task is active.
	SetDefaultOnFootClipSet(GetClipSetId());
	
	SetInternalTaskType(CTaskTypes::TASK_MOTION_TENNIS);
}

CTaskMotionTennis::~CTaskMotionTennis()
{
	ResetOnFootClipSet(true);
}

CTask::FSM_Return CTaskMotionTennis::ProcessPreFSM( void )
{
	ProcessMovement();
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::ProcessPostFSM( void )
{
	if(m_bForceResendSyncData)
	{
		if(!GetPed()->IsNetworkClone() && GetPed()->GetNetworkObject())
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, GetPed()->GetNetworkObject());
			//Since motion state is updated via "InFrequent" make sure that any recently set new dive/swing data get sent straightaway
			CPedSyncTreeBase *syncTree = SafeCast(CPedSyncTreeBase, pPedObj->GetSyncTree());
			if(syncTree)
			{
				syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, pPedObj->GetActivationFlags(), pPedObj, *syncTree->GetPedMovementGroupDataNode());
			}
		}		
		m_bForceResendSyncData = false;
	}

	return FSM_Continue;
}
void CTaskMotionTennis::ProcessMovement( void )
{
	CPed * pPed = GetPed();

	//******************************************************************************************
	// This is the work which was done inside CPedMoveBlendOnFoot to model player/ped movement

	const bool bIsPlayer = pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() || (NetworkInterface::IsGameInProgress() && pPed->IsNetworkPlayer());

	// Interpolate the current moveblendratio towards the desired
	const float fAccel = bIsPlayer ? CTaskMotionBasicLocomotion::ms_fPlayerMoveAccel : CTaskMotionBasicLocomotion::ms_fPedMoveAccel;
	const float fDecel = bIsPlayer ? CTaskMotionBasicLocomotion::ms_fPlayerMoveDecel : CTaskMotionBasicLocomotion::ms_fPedMoveDecel;
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();
	
	if( vDesiredMBR.x > vCurrentMBR.x )
	{
		vCurrentMBR.x += fAccel * fwTimer::GetTimeStep();
		vCurrentMBR.x = MIN( vCurrentMBR.x, MIN( vDesiredMBR.x, MOVEBLENDRATIO_SPRINT ) );
	}
	else if( vDesiredMBR.x < vCurrentMBR.x )
	{
		vCurrentMBR.x -= fDecel * fwTimer::GetTimeStep();
		vCurrentMBR.x = MAX( vCurrentMBR.x, MAX( vDesiredMBR.x, -MOVEBLENDRATIO_SPRINT ) );
	}
	else
	{
		vCurrentMBR.x = vDesiredMBR.x;
	}

	if( vDesiredMBR.y > vCurrentMBR.y )
	{
		vCurrentMBR.y += fAccel * fwTimer::GetTimeStep();
		vCurrentMBR.y = MIN( vCurrentMBR.y, MIN( vDesiredMBR.y, MOVEBLENDRATIO_SPRINT ) );
	}
	else if(vDesiredMBR.y < vCurrentMBR.y)
	{
		vCurrentMBR.y -= fDecel * fwTimer::GetTimeStep();
		vCurrentMBR.y = MAX( vCurrentMBR.y, MAX( vDesiredMBR.y, -MOVEBLENDRATIO_SPRINT ) );
	}
	else
	{
		vCurrentMBR.y = vDesiredMBR.y;
	}

	// Copy variables back into moveblender.  This is a temporary measure.
	pPed->GetMotionData()->SetCurrentMoveBlendRatio( vCurrentMBR.y, vCurrentMBR.x );
	pPed->GetMotionData()->SetCurrentTurnVelocity( 0.0f );
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( 0.0f );
}

CTask::FSM_Return CTaskMotionTennis::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	if(iEvent == OnUpdate)
	{
		if( SyncToMoveState() )
			return FSM_Continue;
	}

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
			FSM_OnExit
			{
				FSM_Return iRet = StateIdle_OnExit();
				return iRet;
			}

		FSM_State(State_Start)
			FSM_OnEnter
				return StateStart_OnEnter();
			FSM_OnUpdate
				return StateStart_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateStart_OnExit();
				return iRet;
			}

		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateMoving_OnExit();
				return iRet;
			}

		FSM_State(State_Stop)
			FSM_OnEnter
				return StateStop_OnEnter();
			FSM_OnUpdate
				return StateStop_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateStop_OnExit();
				return iRet;
			}
		FSM_State(State_Swing)
			FSM_OnEnter
				return StateSwing_OnEnter();
			FSM_OnUpdate
				return StateSwing_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateSwing_OnExit();
				return iRet;
			}
	FSM_End
}

CTask* CTaskMotionTennis::CreatePlayerControlTask( void ) 
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

// In this case we can query the locomotion clips directly, and possibly cache the results.
void CTaskMotionTennis::GetMoveSpeeds( CMoveBlendRatioSpeeds &speeds )
{	

	speeds.Zero();
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(GetClipSetId());
	taskAssert( clipSet );

	static const fwMvClipId s_walkClip("walk_fwd_0_loop",0x90FA019A);
	static const fwMvClipId s_runClip("run_fwd_0_loop",0x5794E6CE);
	static const fwMvClipId s_sprintClip("run_fwd_0_loop",0x5794E6CE);

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };
	RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	return;
}

void CTaskMotionTennis::GetNMBlendOutClip( fwMvClipSetId& outClipSet, fwMvClipId& outClip )
{
	outClipSet = m_moveNetworkHelper.GetClipSetId();
	outClip = CLIP_IDLE;
}

//////////////////////////////////////////////////////////////////////////
// MoVE interface helper methods
//////////////////////////////////////////////////////////////////////////
bool CTaskMotionTennis::SyncToMoveState( void )
{
	s32 iState = -1;
	if( m_moveNetworkHelper.IsNetworkActive() )
	{

		enum {
			onExitStop,
			onExitSwing,
			exitEventCount
		};

		static const fwMvId s_exitEventIds[ exitEventCount ] = {			
			fwMvId( "onExitStop",0x91B1BF5 ),			
			fwMvId( "onExitSwing",0x47DB1691 ),
		};

		CompileTimeAssert( NELEM( s_exitEventIds ) == exitEventCount );

		fwMvId stateEventId = m_moveNetworkHelper.GetNetworkPlayer()->GetLastEventThisFrame( s_exitEventIds, exitEventCount );

		if( stateEventId == s_exitEventIds[onExitSwing] )
		{
			m_bRequestSwing = false;
		}		
		else if( stateEventId == s_exitEventIds[ onExitStop ] )
		{
			m_bOverrideStrafeDirectionWithStopDirection = false;
		}


		enum {
			onEnterIdle,
			onEnterMoving,
			onEnterStop,
			onEnterStart,
			onEnterSwing,
			eventCount
		};

		static const fwMvId s_eventIds[ eventCount ] = {
			fwMvId( "OnEnterIdle",0xF110481F ),
			fwMvId( "OnEnterMoving",0x23289D8 ),
			fwMvId( "onEnterStop",0x66D6AF9F ),
			fwMvId( "onEnterStart",0xA45362DF ),
			fwMvId( "onEnterSwing",0xDCAFC747 ),
		};

		CompileTimeAssert( NELEM( s_eventIds ) == eventCount );

		stateEventId = m_moveNetworkHelper.GetNetworkPlayer()->GetLastEventThisFrame( s_eventIds, eventCount );

		if( stateEventId == s_eventIds[onEnterSwing] )
		{
			iState = State_Swing;
		}
		else if( stateEventId == s_eventIds[ onEnterIdle ] )
		{
			iState = State_Idle;
		}
		else if( stateEventId == s_eventIds[ onEnterMoving ] )
		{
			iState = State_Moving;
		}
		else if( stateEventId == s_eventIds[ onEnterStart ] )
		{
			iState = State_Start;
			m_bOverrideStrafeDirectionWithStopDirection = false;
		}
		else if( stateEventId == s_eventIds[ onEnterStop ] )
		{
			iState = State_Stop;
			m_bOverrideStrafeDirectionWithStopDirection = true;
		}

		SendParams();
	}

	if( iState != -1 && iState != GetState() )
	{
		SetState( iState );
		return true;
	}

	return false;
}

void CTaskMotionTennis::SendParams( void )
{
	//calculate the strafing direction
	CPed* pPed = GetPed();

	
	// Compute Strafing Params
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	bool bControlOutOfDeadZone = true;

	// Dead zone check. Used to adjust sensitivity.
	if(pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame())
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		Vector2 vecStickInput;
		vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
		vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();

		static dev_float fDeadZone = 0.2f;
		if(vecStickInput.Mag2() < fDeadZone*fDeadZone)
		{
			bControlOutOfDeadZone = false;
			vDesiredMBR.x = 0.0f;
			vDesiredMBR.y = 0.0f;
			vCurrentMBR.x = 0.0f;
			vCurrentMBR.y = 0.0f;
		}
	}

	if(NetworkInterface::IsGameInProgress())
	{
		if(!pPed->IsNetworkClone())
		{
			if(m_bControlOutOfDeadZone != bControlOutOfDeadZone)
			{
				//Dead zone status has change
				m_bControlOutOfDeadZone = bControlOutOfDeadZone;
				m_bForceResendSyncData = true;
			}
		}
		else
		{
			if(!m_bControlOutOfDeadZone)
			{
				vDesiredMBR.x = 0.0f;
				vDesiredMBR.y = 0.0f;
				vCurrentMBR.x = 0.0f;
				vCurrentMBR.y = 0.0f;
			}
		}
	}


	float fDesiredStrafeSpeed = vDesiredMBR.Mag() / MOVEBLENDRATIO_SPRINT;
	m_fStrafeSpeed = vDesiredMBR.Mag() / MOVEBLENDRATIO_SPRINT;

	if( fDesiredStrafeSpeed > 0.0f )
	{
		m_fDesiredStrafeDirection = fwAngle::LimitRadianAngleSafe( atan2( vDesiredMBR.y, vDesiredMBR.x ) - HALF_PI );

		const float SIXTH_OF_A_PI = PI / 6.0f;	// 30 degs
		const float THIRD_OF_A_PI = PI / 3.0f;  // 60 degs
		if( m_fDesiredStrafeDirection < ( HALF_PI - SIXTH_OF_A_PI ) && m_fDesiredStrafeDirection > ( -HALF_PI + SIXTH_OF_A_PI ) )
		{
			m_fDesiredBackwardsBlend = 0.0f;
		}
		else if( Abs( m_fDesiredStrafeDirection ) <= ( HALF_PI + SIXTH_OF_A_PI ) )
		{
			m_fDesiredBackwardsBlend = Abs( m_fDesiredStrafeDirection );
			m_fDesiredBackwardsBlend -= ( HALF_PI - SIXTH_OF_A_PI );
			taskAssert( Abs( m_fDesiredBackwardsBlend ) <= THIRD_OF_A_PI );
			m_fDesiredBackwardsBlend /= THIRD_OF_A_PI;
		}
		else 
		{
			m_fDesiredBackwardsBlend = 1.0f;
		}

		TUNE_GROUP_FLOAT( PED_MOVEMENT, BACKWARDS_BLEND_LERP_RATE, 3.0f, 0.0f, 20.0f, 0.001f );
		InterpValue( m_fCurrentBackwardsBlend, m_fDesiredBackwardsBlend, BACKWARDS_BLEND_LERP_RATE, false );
	}

	m_moveNetworkHelper.SetFloat( ms_BackwardsStrafeBlend, m_fCurrentBackwardsBlend );

	float fStrafeDirectionLerpRate = -1.0f;

	if( m_fStrafeSpeed > 0.0f )
	{
		TUNE_GROUP_FLOAT( PED_MOVEMENT, STRAFE_DIRECTION_LERP_RATE_MAX, 3.0f, 0.0f, 20.0f, 0.001f );
		fStrafeDirectionLerpRate = STRAFE_DIRECTION_LERP_RATE_MAX * ( 1.0f / m_fStrafeSpeed );
	}

	// If moving from idle, just set direction directly
	if( fStrafeDirectionLerpRate < 0.0f )
	{
		m_fStrafeDirection = m_fDesiredStrafeDirection;
	}
	else
	{
		// Check if the ped is doing a 180 turn.
		bool b180Turn = false;
		if(GetState() == State_Moving && !m_bRequestSwing)
		{
			static dev_float f180TurnCone = 40.0f;
			float fDiff = fwAngle::LimitRadianAngleSafe(m_fDesiredStrafeDirection - m_fStrafeDirection);
			if(Abs(fDiff) >= (PI - DtoR*f180TurnCone))
			{
				b180Turn = true;
			}
		}

		if(b180Turn)
		{
			m_bOverrideStrafeDirectionWithStopDirection = true;

			// Force to stop in 180 turn and set strafe stop direction.
			m_bRequestStop = true;
			m_fStrafeSpeed = 0.0f;
			m_fStrafeStopDirection = m_fStrafeDirection;

			// No dampening for 180 turn.
			m_fStrafeDirection = m_fDesiredStrafeDirection;
		}
		else if(m_bAllowDampen && m_bUseDampening)
		{
			const Tunables& tunables = GetTunables();
			float fStrafeDirectionLerpRateMin = pPed->IsAPlayerPed() ? tunables.m_StrafeDirectionLerpRateMinPlayer : tunables.m_StrafeDirectionLerpRateMinAI;
			float fStrafeDirectionLerpRateMax = pPed->IsAPlayerPed() ? tunables.m_StrafeDirectionLerpRateMaxPlayer : tunables.m_StrafeDirectionLerpRateMaxAI;
			float fDiff = fwAngle::LimitRadianAngleSafe(m_fDesiredStrafeDirection - m_fStrafeDirection);
			fStrafeDirectionLerpRate = Lerp(Abs(fDiff/PI), fStrafeDirectionLerpRateMin, fStrafeDirectionLerpRateMax);
			InterpValue(m_fStrafeDirection, m_fDesiredStrafeDirection, fStrafeDirectionLerpRate, true);
			m_fStrafeDirection = fwAngle::LimitRadianAngleSafe(m_fStrafeDirection);
		}
		else
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;
		}
	}

	// Limit angle during start state so it does not cross warp point
	if(GetState() == State_Start)
	{
		if(m_bCWStart)
		{
			// wrap point is -HALF_PI for CW start
			if(m_fStrafeStartDirection > -PI && m_fStrafeStartDirection < -HALF_PI)
			{
				m_fStrafeStartDirection = Min(-HALF_PI - 0.1f, m_fStrafeDirection);
			}
			else if(m_fStrafeStartDirection > -HALF_PI && m_fStrafeStartDirection < 0.0f)
			{
				m_fStrafeStartDirection = Max(-HALF_PI + 0.1f, m_fStrafeDirection);
			}
			else
			{
				m_fStrafeStartDirection = m_fStrafeDirection;
			}
		}
		else
		{
			// wrap point is HALF_PI for CCW start
			if(m_fStrafeStartDirection > 0.0f && m_fStrafeStartDirection < HALF_PI)
			{
				m_fStrafeStartDirection = Min(HALF_PI - 0.1f, m_fStrafeDirection);
			}
			else if(m_fStrafeStartDirection > HALF_PI && m_fStrafeStartDirection < PI)
			{
				m_fStrafeStartDirection = Max(HALF_PI + 0.1f, m_fStrafeDirection);
			}
			else
			{
				m_fStrafeStartDirection = m_fStrafeDirection;
			}
		}
	}

	float fStrafeStartSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( m_fStrafeStartDirection );
	m_moveNetworkHelper.SetFloat( ms_StrafeStartDirectionId, fStrafeStartSignal ) ;

	// Set strafe stop direction
	if(m_bOverrideStrafeDirectionWithStopDirection)
	{
		m_fStrafeDirection = m_fStrafeStopDirection;
		//m_moveNetworkHelper.SetFloat( ms_StrafeDirectionId, fStrafeStopSignal ) ;
	}
	float fStrafeStopSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( m_fStrafeStopDirection );
	m_moveNetworkHelper.SetFloat( ms_StrafeStopDirectionId, fStrafeStopSignal );

	float fStrafeSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( m_fStrafeDirection );
	m_moveNetworkHelper.SetFloat( ms_StrafeDirectionId, fStrafeSignal ) ;
	
	//Displayf("Strafing signal: %f", fStrafeSignal);
	float fDesiredStrafeDirectionSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal( m_fDesiredStrafeDirection );
	m_moveNetworkHelper.SetFloat( ms_DesiredStrafeDirectionId, fDesiredStrafeDirectionSignal );

#if DEBUG_DRAW
	static bool bDebugDraw = false;
	if(bDebugDraw && pPed->IsAPlayerPed())
	{
		Vector3 pPlayerPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		Matrix34 mat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		mat.RotateLocalZ(m_fDesiredStrafeDirection);
		grcDebugDraw::Line(pPlayerPos, pPlayerPos + mat.b * 2.0f, Color_red);
		mat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		mat.RotateLocalZ(m_fStrafeDirection);
		grcDebugDraw::Line(pPlayerPos, pPlayerPos + mat.b * 2.0f, Color_green);
		//mat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		//mat.RotateLocalZ(m_fStrafeStopDirection);
		//grcDebugDraw::Line(pPlayerPos, pPlayerPos + mat.b * 2.0f, Color_blue);
	}
#endif //  DEBUG_DRAW

	TUNE_GROUP_FLOAT( NEW_GUN_TASK, STRAFE_WALK_RATE, 1.4f, 0.0f, 4.0f, 0.1f );
	if (!pPed->GetMotionData()->GetIsCrouching())
	{
		m_moveNetworkHelper.SetFloat( ms_StrafeSpeedId,  m_fStrafeSpeed );
		m_moveNetworkHelper.SetFloat( ms_DesiredStrafeSpeedId,  fDesiredStrafeSpeed );
	}
	else // Don't move if crouched
	{
		m_moveNetworkHelper.SetFloat( ms_StrafeSpeedId,  0.0f );
		m_moveNetworkHelper.SetFloat( ms_DesiredStrafeSpeedId,  0.0f );
	}
	m_moveNetworkHelper.SetFloat( ms_StrafingRateId, STRAFE_WALK_RATE );

	if(m_bRequestSwing && GetState() != State_Swing)
	{
		if(m_bSendingDiveRequest)
		{
			PlayDiveAnim(m_DiveDirection, m_fDiveHorizontal, m_fDiveVertical, m_fPlayRate, m_bSlowBlend);
		}
		else
		{
			PlayTennisSwingAnim(m_swingAnimDictName, m_swingAnimName, m_fSwingStartPhase, m_fPlayRate, m_bSlowBlend);
		}

		m_bRequestStart = false;
		m_bRequestStop = false;
		
		// Hack: Since the ped won't go into swing state if swinging at the start of stop state.
		// For some reason, 'OnEnterSwing' event got lost in this case.
		SetState(State_Swing);
	}

	if(m_bRequestStart)
	{
		m_moveNetworkHelper.SendRequest(ms_LocoStartId);
		m_bRequestStart = false;		
	} 
	
	if(m_bRequestStop)
	{
		m_moveNetworkHelper.SendRequest(ms_LocoStopId);
		m_bRequestStop = false;
	}
}

bool CTaskMotionTennis::StartMoveNetwork( void )
{
	if( !m_moveNetworkHelper.IsNetworkActive() )
	{
		// request the network def
		m_moveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskMotionTennis );		
		
		// stream in the movement clip clipSetId
		fwClipSet* pSet = fwClipSetManager::GetClipSet( GetClipSetId() );
		taskAssert( pSet );

		if( pSet->Request_DEPRECATED() && m_moveNetworkHelper.IsNetworkDefStreamedIn( GetMoveNetworkDefId() ))
		{
			// Start the on-foot move network
			m_moveNetworkHelper.CreateNetworkPlayer( GetPed(), GetMoveNetworkDefId() );
			m_moveNetworkHelper.SetClipSet( GetClipSetId() );
			GetPed()->GetMovePed().SetMotionTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), 0.0f, CMovePed::MotionTask_TagSyncTransition);
			//m_moveNetworkHelper.DeferredInsert();

			if( m_initialState > State_Initial )
			{
				static const fwMvStateId s_moveStateIds[State_Moving+1] = 
				{
					fwMvStateId(),
					fwMvStateId("Idle",0x71C21326),
					fwMvStateId("Moving",0xDF719C6C)
				};

				static fwMvStateId stateMachine("strafing",0x1C181AC3);
				SetState( m_initialState );
				m_moveNetworkHelper.ForceStateChange( s_moveStateIds[ m_initialState ], stateMachine );
			}
			else
			{
				SetState( State_Idle );
			}

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
#if !__FINAL

const char * CTaskMotionTennis::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Idle: return "Idle";
	case State_Moving: return "Moving";
	case State_Start: return "Start";
	case State_Stop: return "Stop";
	case State_Swing: return "Swing";
	default: { Assert(false); return "Unknown"; }
	}
}

void CTaskMotionTennis::Debug() const
{
}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionTennis::StateInitial_OnEnter( void )
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateInitial_OnUpdate( void )
{
	if( StartMoveNetwork() )
	{
		// Reset Motion Params
		m_fStrafeSpeed = -1.0f;
		m_fDesiredStrafeDirection = -1.0f;
		m_fStrafeDirection = -1.0f;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	State Idle
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionTennis::StateIdle_OnEnter( void )
{	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateIdle_OnUpdate( void )
{
	UpdateDesiredHeading( GetPed() );

	if(m_fStrafeSpeed > 0.01f)
	{
		m_bUseDampening = false;
		m_bOverrideStrafeDirectionWithStopDirection = false;
		m_fStrafeStartDirection = m_fDesiredStrafeDirection;
		m_fStrafeDirection = m_fDesiredStrafeDirection;
		m_bRequestStart = true;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateIdle_OnExit( void )
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	State Start
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionTennis::StateStart_OnEnter( void )
{	
	m_bUseDampening = true;
	if(m_fDesiredStrafeDirection >= 0.5f)
	{
		m_bCWStart = true;
	}
	else
	{
		m_bCWStart = false;
	}
	m_fStrafeStartDirection = m_fDesiredStrafeDirection;
	m_fStrafeDirection = m_fDesiredStrafeDirection;
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateStart_OnUpdate( void )
{
	if(m_fStrafeSpeed < 0.01f && m_moveNetworkHelper.GetBoolean(ms_InterruptStopId) && !m_bRequestSwing)
	{
		m_fStrafeStopDirection = m_fStrafeDirection;
		m_bRequestStop = true;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateStart_OnExit( void )
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	Strafing
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionTennis::StateMoving_OnEnter( void )
{
	m_bUseDampening = true;
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateMoving_OnUpdate( void )
{
	if(m_fStrafeSpeed < 0.01f && !m_bRequestSwing)
	{
		m_fStrafeStopDirection = m_fStrafeDirection;
		m_bRequestStop = true;
	}

	UpdateDesiredHeading( GetPed() );
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateMoving_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	State Stop
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionTennis::StateStop_OnEnter( void )
{	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateStop_OnUpdate( void )
{
	if(m_fStrafeSpeed > 0.01f && m_moveNetworkHelper.GetBoolean(ms_InterruptMoveId) && !m_bRequestSwing)
	{
		m_bUseDampening = false;
		m_bOverrideStrafeDirectionWithStopDirection = false;
		m_fStrafeStartDirection = m_fDesiredStrafeDirection;
		m_bRequestStart = true;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateStop_OnExit( void )
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
//	State Swing
//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMotionTennis::StateSwing_OnEnter( void )
{	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateSwing_OnUpdate( void )
{
	// Swing can be interrupted only after 'InterruptSwing' tag.
	if(m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId))
	{
		if(m_fStrafeSpeed >= 0.01f && m_moveNetworkHelper.GetBoolean(ms_InterruptMoveId))
		{
			m_bUseDampening = false;
			m_bOverrideStrafeDirectionWithStopDirection = false;
			m_fStrafeStartDirection = m_fDesiredStrafeDirection;
			m_bRequestStart = true;
		}
		m_bRequestSwing = false;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionTennis::StateSwing_OnExit( void )
{
	m_moveNetworkHelper.SetFlag(false, ms_DiveBHId);
	m_moveNetworkHelper.SetFlag(false, ms_DiveFHId);

	ClearNetSyncData();

	return FSM_Continue;
}

void CTaskMotionTennis::UpdateDesiredHeading( CPed* pPed )
{
	Assertf( pPed, "CTaskMotionTennis::UpdateDesiredHeading - Invalid ped." );

	// Apply some extra steering here
	float fCurrentHeading = pPed->GetCurrentHeading();
	float fDesiredHeading = pPed->GetDesiredHeading();

	//Update heading to camera
	if (!pPed->GetPedIntelligence()->IsPedInMelee()) 
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
				//if scripts are forcing strafe, check for scripted cameras.  (see tennis)				
				if ( pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePedToUseScripCamHeading) && pPed->IsPlayer() && camInterface::GetScriptDirector().IsRendering()  ) 
				{
					pPed->SetDesiredHeading(camInterface::GetScriptDirector().GetFrame().ComputeHeading());
				} 
				else 
				{
					const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
					pPed->SetDesiredHeading(gameplayDirector.GetFrame().ComputeHeading());
				}				
			}

			fDesiredHeading	= pPed->GetDesiredHeading();
		}
	}

	TUNE_GROUP_FLOAT( PED_MOVEMENT, fStrafeHeadingChangeMultiplier, 4.0f, 0.0f, 20.0f, 0.001f );

	float fHeadingMultiplier = fStrafeHeadingChangeMultiplier;
	float fHeadingDelta = InterpValue( fCurrentHeading, fDesiredHeading, fHeadingMultiplier, true);
	fHeadingDelta += GetMotionData().GetExtraHeadingChangeThisFrame();
	GetMotionData().SetExtraHeadingChangeThisFrame( fHeadingDelta );
}

//////////////////////////////////////////////////////////////////////////

const fwMvNetworkDefId& CTaskMotionTennis::GetMoveNetworkDefId( void ) const
{
	return CClipNetworkMoveInfo::ms_NetworkTaskMotionTennis;
}

void CTaskMotionTennis::PlayTennisSwingAnim(const char *pAnimDictName, const char *pAnimName, float fStartPhase, float fPlayRate, bool bSlowBlend)
{
	atHashWithStringNotFinal animDictName = pAnimDictName;
	atHashWithStringNotFinal animName = pAnimName;

	PlayTennisSwingAnim(animDictName, animName, fStartPhase, fPlayRate, bSlowBlend);
}

void CTaskMotionTennis::PlayTennisSwingAnim(atHashWithStringNotFinal animDictName, atHashWithStringNotFinal animName, float fStartPhase, float fPlayRate, bool bSlowBlend)
{
	s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(animDictName.GetHash()).Get();
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, animName.GetHash());
	Assertf(pClip, "Couldn't find the animation. DictName: %s. AnimName: %s", animDictName.GetCStr(), animName.GetCStr());

	//Displayf("%s: Request swing animation. DictName: %s. AnimName: %s", GetPed()->IsAPlayerPed() ? "Player":"AI", pAnimDictName, pAnimName);

	if(GetState() != State_Swing || m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId))
	{
		m_swingAnimDictName = animDictName;
		m_swingAnimName = animName;
		m_fSwingStartPhase = fStartPhase;
		m_fPlayRate = fPlayRate;
		m_bSlowBlend = bSlowBlend;

		m_moveNetworkHelper.SetClip(pClip, ms_SwingClipId);
		m_moveNetworkHelper.SetFloat( ms_PhaseFloatId, fStartPhase );
		m_moveNetworkHelper.SetFlag(false, ms_DiveBHId);
		m_moveNetworkHelper.SetFlag(false, ms_DiveFHId);
		SetSwingAnimPlayRate(fPlayRate );
		m_moveNetworkHelper.SetFlag(bSlowBlend, ms_SlowBlendId);

		m_moveNetworkHelper.SendRequest(ms_SwingRequestId);

		m_bRequestSwing = true;
		m_bRequestStart = false;
		m_bRequestStop = false;
		m_bSendingDiveRequest = false;

		if(!m_bAllowOverrideCloneUpdate)
		{
			SetNetSyncTennisSwingAnimData( animDictName, animName, fStartPhase, fPlayRate, bSlowBlend);
		}
	}
}

void CTaskMotionTennis::PlayDiveAnim(bool bDirection, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend)
{
	eDiveDirection direction = Dive_FH;
	if(bDirection)
	{
		direction = Dive_BH;
	}
	PlayDiveAnim( direction, fDiveHorizontal, fDiveVertical, fPlayRate, bSlowBlend);
}

void CTaskMotionTennis::PlayDiveAnim(eDiveDirection direction, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend)
{
	Assert(direction == Dive_BH || direction == Dive_FH);
	if(GetState() != State_Swing || m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId))
	{
		m_DiveDirection = direction;
		m_fDiveHorizontal = fDiveHorizontal;
		m_fDiveVertical = fDiveVertical;
		m_fPlayRate = fPlayRate;
		m_bSlowBlend = bSlowBlend;
		
		m_moveNetworkHelper.SetFloat( ms_DiveHorizontalFloatId, fDiveHorizontal );
		m_moveNetworkHelper.SetFloat( ms_DiveVerticalFloatId, fDiveVertical );
		
		if(direction == Dive_BH)
		{
			m_moveNetworkHelper.SetFlag(true, ms_DiveBHId);
			m_moveNetworkHelper.SetFlag(false, ms_DiveFHId);
		}
		else
		{
			m_moveNetworkHelper.SetFlag(false, ms_DiveBHId);
			m_moveNetworkHelper.SetFlag(true, ms_DiveFHId);
		}
		SetSwingAnimPlayRate(fPlayRate );
		m_moveNetworkHelper.SetFlag(bSlowBlend, ms_SlowBlendId);

		m_moveNetworkHelper.SendRequest(ms_SwingRequestId);
		m_bRequestSwing = true;
		m_bRequestStart = false;
		m_bRequestStop = false;
		m_bSendingDiveRequest = true;

		if(!m_bAllowOverrideCloneUpdate)
		{
			SetNetSyncTennisDiveAnimData( direction,  fDiveHorizontal,  fDiveVertical,  fPlayRate, bSlowBlend);
		}
	}
}

void CTaskMotionTennis::SetSwingAnimPlayRate(float fPlayRate)
{
	m_moveNetworkHelper.SetFloat( ms_PlayRateFloatId, fPlayRate );
}

// Used by script to check if the swing anim is completed.
bool CTaskMotionTennis::GetTennisSwingAnimComplete()
{
	return !m_bRequestSwing && GetState() != State_Swing;
}

// If the swing anim can be interrupted for the next move.
bool CTaskMotionTennis::GetTennisSwingAnimCanBeInterrupted()
{
	return GetState() == State_Swing && m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId);
}

bool CTaskMotionTennis::GetTennisSwingAnimSwung()
{
	return GetState() == State_Swing && m_moveNetworkHelper.GetBoolean(ms_SwungId);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// For clone syncing
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Ensure that interrupt is not valid while in non swing state on clones
//and check for enter swing or enter idle events not being valid before allowing setting swing or dive
bool CTaskMotionTennis::GetCloneAllowApplySwing()
{
	if( taskVerifyf(m_moveNetworkHelper.IsNetworkActive(),"%s Expect active m_moveNetworkHelper. State %d, Last State %d", GetEntity()?GetPed()->GetDebugName():"Null ped", GetState(), GetPreviousState() ) )
	{
		static const fwMvId s_eventIdOnEnterIdle	= fwMvId( "OnEnterIdle",0xF110481F );
		static const fwMvId s_eventIdOnEnterSwing	= fwMvId( "onEnterSwing",0xDCAFC747 );

		if( s_eventIdOnEnterIdle == m_moveNetworkHelper.GetNetworkPlayer()->GetLastEventThisFrame( &s_eventIdOnEnterIdle, 1 ) ||
			s_eventIdOnEnterSwing == m_moveNetworkHelper.GetNetworkPlayer()->GetLastEventThisFrame( &s_eventIdOnEnterSwing, 1 ) )
		{
			return false;
		}
	}

	bool bIsInterruptValidDuringNonSwingState = m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId) && GetState() != State_Swing;
	bool bAllowApplySwingToClone = GetTennisSwingAnimCanBeInterrupted() && !bIsInterruptValidDuringNonSwingState;
	
	bAllowApplySwingToClone|=GetTennisSwingAnimComplete();


	return bAllowApplySwingToClone;
}

void CTaskMotionTennis::SetNetSyncTennisSwingAnimData(atHashWithStringNotFinal animDictName, atHashWithStringNotFinal animName, float fStartPhase, float fPlayRate, bool bSlowBlend)
{  //set the data we want to send
	Assertf((GetState() != State_Swing || m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId)), 
		"Unexpected state %d, ms_InterruptSwingId %s",GetState(), m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId)?"true":"false");

	ClearNetSyncData();

	m_CloneAnimDictName = animDictName;
	m_CloneAnimName		= animName;
	m_CloneStartPhase	= fStartPhase;
	m_ClonePlayRate		= fPlayRate;
	m_bDiveMode			= false;
	m_bCloneSlowBlend		= bSlowBlend;
	
	m_bForceResendSyncData = true;
}
void CTaskMotionTennis::SetNetSyncTennisDiveAnimData(eDiveDirection direction, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend)
{
	Assertf((GetState() != State_Swing || m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId)), 
		"Unexpected state %d, ms_InterruptSwingId %s",GetState(), m_moveNetworkHelper.GetBoolean(ms_InterruptSwingId)?"true":"false");
	
	ClearNetSyncData();

	m_bDiveDir			=	(direction == Dive_BH);
	m_bDiveMode			=	true;
	m_fCloneDiveHorizontal	=	fDiveHorizontal;
	m_fCloneDiveVertical		=	fDiveVertical;
	m_ClonePlayRate		=	fPlayRate;
	m_bCloneSlowBlend		=	bSlowBlend;
	
	m_bForceResendSyncData = true;
}

bool CTaskMotionTennis::IsNetSyncTennisSwingAnimDataValid()
{
	if( m_CloneAnimDictName.GetHash()!=0 &&
		m_CloneAnimName.GetHash()!=0 &&
		taskVerifyf(m_CloneStartPhase!=-1.0f,"m_CloneStartPhase not set. m_CloneAnimDictName 0x%x, m_CloneAnimName 0x%x",m_CloneAnimDictName.GetHash(),m_CloneAnimName.GetHash()) &&
		taskVerifyf(m_ClonePlayRate!=-1.0f,"m_ClonePlayRate not set. m_CloneAnimDictName 0x%x, m_CloneAnimName 0x%x",m_CloneAnimDictName.GetHash(),m_CloneAnimName.GetHash()))
	{
		return true;
	}

	return false;
}

bool CTaskMotionTennis::IsNetSyncTennisDiveAnimDataValid()
{
	if( m_bDiveMode &&
		taskVerifyf(m_fCloneDiveHorizontal!=-1.0f,"m_fCloneDiveHorizontal not set.") &&
		taskVerifyf(m_fCloneDiveVertical!=-1.0f,"m_fCloneDiveVertical not set. ") )
	{
		return true;
	}

	return false;
}

void CTaskMotionTennis::ClearNetSyncData()
{
	m_CloneAnimDictName.Clear();
	m_CloneAnimName.Clear();
	m_CloneStartPhase	= -1.0f;
	m_ClonePlayRate		= -1.0f;
	m_bDiveMode			= false;
	m_bDiveDir			= false;
	m_fCloneDiveHorizontal	= -1.0f;
	m_fCloneDiveVertical		= -1.0f;
}

void CTaskMotionTennis::GetNetSyncTennisDiveAnimData(bool &diveDirection, float &fDiveHorizontal, float &fDiveVertical, float &fPlayRate, bool &bSlowBlend)
{
#if __ASSERT
	CPed * pPed = GetPed();
	Assertf(m_moveNetworkHelper.IsNetworkActive(), "Expect valid network");
	Assertf(!pPed->IsNetworkClone(), "Expect to get data only from local");
	Assertf(IsNetSyncTennisDiveAnimDataValid(), 
		"Expect valid data for dive syncing here:  m_bDiveMode %s, m_fCloneDiveHorizontal %f, m_fCloneDiveVertical %f",
		m_bDiveMode?"True":"false",m_fCloneDiveHorizontal, m_fCloneDiveVertical);
#endif

	diveDirection	= m_bDiveDir;
	fDiveHorizontal	= m_fCloneDiveHorizontal;
	fDiveVertical	= m_fCloneDiveVertical;
	fPlayRate		= m_ClonePlayRate;
	bSlowBlend		= m_bCloneSlowBlend;
}

void CTaskMotionTennis::GetNetSyncTennisSwingAnimData(u32 &dictHash, u32 &clipHash, float &fStartPhase, float &fPlayRate, bool &bSlowBlend)
{
#if __ASSERT
	CPed * pPed = GetPed();
	Assertf(m_moveNetworkHelper.IsNetworkActive(), "Expect valid network");
	Assertf(!pPed->IsNetworkClone(), "Expect to get data only from local");
	Assertf(IsNetSyncTennisSwingAnimDataValid(), 
		"Expect valid data for syncing here: m_CloneAnimDictName 0x%x, m_CloneAnimName 0x%x, m_CloneStartPhase %f, m_ClonePlayRate %f",
		m_CloneAnimDictName.GetHash(),m_CloneAnimName.GetHash(),m_CloneStartPhase, m_ClonePlayRate);
#endif

	dictHash	= m_CloneAnimDictName.GetHash();
	clipHash	= m_CloneAnimName.GetHash();
	//fStartPhase = m_moveNetworkHelper.GetFloat( ms_PhaseFloatId );
	//fPlayRate	= m_moveNetworkHelper.GetFloat( ms_PlayRateFloatId );
	fStartPhase = m_CloneStartPhase;
	fPlayRate	= m_ClonePlayRate;
	bSlowBlend	= m_bCloneSlowBlend;
}

fwMvClipSetId CTaskMotionTennis::GetClipSetId() const
{
	return m_bUseFemaleClipSet ? defaultClipSetIdFemale : defaultClipSetId;
}

void CTaskMotionTennis::SetSignalFloat( const char* szSignal, float fSignal )
{
	fwMvFloatId floatSignalId(szSignal);
	m_moveNetworkHelper.SetFloat(floatSignalId, fSignal);
}

#if __BANK
bool	CTaskMotionTennis::ms_bankOverrideCloneUpdate	= false;
bool	CTaskMotionTennis::ms_bankDiveLeft				= false;
bool	CTaskMotionTennis::ms_ForceSend					= false;
char	CTaskMotionTennis::ms_NameOfSwing[MAX_SWING_NAME_LEN];
float	CTaskMotionTennis::ms_fPlayRate					= 1.0f;
float	CTaskMotionTennis::ms_fStartPhase				= 0.0f;
float	CTaskMotionTennis::ms_fDiveHoriz				= 0.5f;
float	CTaskMotionTennis::ms_fDiveVert					= 0.5f;

void CTaskMotionTennis::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		strcpy(ms_NameOfSwing,"forehand_ts_hi_far");

		pBank->PushGroup("Task Tennis Motion", false);
		pBank->AddSeparator();
		pBank->AddToggle("Override Clone Update", &ms_bankOverrideCloneUpdate, SetAllowOverrideCloneUpdate);
		pBank->AddSeparator();
		pBank->AddToggle("Dive Dive_BH", &ms_bankDiveLeft, NullCB);
		pBank->AddSeparator();
		pBank->AddToggle("Force send dive/swing without checking complete", &ms_ForceSend, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Play Rate", &ms_fPlayRate,  0.0f, 2.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Start phase", &ms_fStartPhase,  0.0f, 1.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Dive Horizontal", &ms_fDiveHoriz,  0.0f, 2.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Dive vertical", &ms_fDiveVert,  0.0f, 2.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddText("Swing anim name", ms_NameOfSwing,	MAX_SWING_NAME_LEN, false);
		pBank->AddSeparator();
		pBank->AddButton("Start Swing", SendSwing);
		pBank->AddSeparator();
		pBank->AddButton("Start Dive", SendDive);
		pBank->AddSeparator();

		pBank->PopGroup();
	}
}

CTaskMotionTennis* CTaskMotionTennis::GetActiveTennisMotionTaskOnFocusPed()
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		const CPed* pPed = (CPed*)pFocusEntity1;
		if (pPed && pPed->GetPedIntelligence())
		{
			CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
			return pTask;
		}
	}
		
	return NULL;
}

void CTaskMotionTennis::SetAllowOverrideCloneUpdate()
{
	CTaskMotionTennis* pTaskTennisMotion = CTaskMotionTennis::GetActiveTennisMotionTaskOnFocusPed();

	if(pTaskTennisMotion)
	{
		pTaskTennisMotion->SetAllowOverrideCloneUpdate(ms_bankOverrideCloneUpdate);
	}
}

void CTaskMotionTennis::SendSwing()
{
	CTaskMotionTennis* pTaskTennisMotion = CTaskMotionTennis::GetActiveTennisMotionTaskOnFocusPed();

	if(pTaskTennisMotion)
	{
		if(ms_ForceSend || pTaskTennisMotion->GetTennisSwingAnimComplete() || pTaskTennisMotion->GetTennisSwingAnimCanBeInterrupted())
		{
			pTaskTennisMotion->PlayTennisSwingAnim("mini@tennis", ms_NameOfSwing, ms_fStartPhase, ms_fPlayRate, false);
		}
	}
}
void CTaskMotionTennis::SendDive()
{
	CTaskMotionTennis* pTaskTennisMotion = CTaskMotionTennis::GetActiveTennisMotionTaskOnFocusPed();

	if(pTaskTennisMotion)
	{
		if(ms_ForceSend || pTaskTennisMotion->GetTennisSwingAnimComplete() || pTaskTennisMotion->GetTennisSwingAnimCanBeInterrupted())
		{
			pTaskTennisMotion->PlayDiveAnim(ms_bankDiveLeft, ms_fDiveHoriz, ms_fDiveVert,  ms_fPlayRate, false);
		}
	}
}

#endif
