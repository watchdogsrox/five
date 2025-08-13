//
// Task/Motion/Locomotion/TaskMotionAimingTransition.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"

// Rage headers
#include "math/angmath.h"

// Game headers
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

const fwMvFloatId CTaskMotionAimingTransition::ms_DirectionId("Direction",0x34DF9828);
const fwMvFloatId CTaskMotionAimingTransition::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskMotionAimingTransition::ms_RateId("Rate",0x7E68C088);
const fwMvBooleanId CTaskMotionAimingTransition::ms_BlendOutId("BLEND_OUT",0xAB120D43);
const fwMvBooleanId CTaskMotionAimingTransition::ms_StartAimIntroId("StartAimIntro",0x90BFEE79);
const fwMvBooleanId CTaskMotionAimingTransition::ms_BlendBackToStrafe("BLEND_BACK_TO_STRAFE",0xB902F6A6);
const fwMvFlagId CTaskMotionAimingTransition::ms_LeftFootId("LeftFoot",0x6D1BDCF4);
const fwMvFlagId CTaskMotionAimingTransition::ms_UseWalkId("UseWalk",0xDD4719AF);
const fwMvFlagId CTaskMotionAimingTransition::ms_CWId("CW",0x3524faa2);
const fwMvRequestId CTaskMotionAimingTransition::ms_IndependentMoverExpressionId("IndependentMoverExpression",0x14DA9474);

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::UseMotionAimingTransition(const float fHeading1, const float fHeading2, const CPed* pPed)
{
	const float fHeading1Safe = fwAngle::LimitRadianAngleSafe(fHeading1);
	const float fHeading2Safe = fwAngle::LimitRadianAngleSafe(fHeading2);
	const float	fHeadingDelta = SubtractAngleShorter(fHeading1Safe, fHeading2Safe);
	return UseMotionAimingTransition(fHeadingDelta, pPed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::UseMotionAimingTransition(const float fHeadingDelta, const CPed* pPed)
{
#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	if (pPed->IsNetworkClone())
	{
		return false;
	}

	static dev_float MIN_ANG = DtoR * -45.f;
	static dev_float MAX_ANG = DtoR * 135.f;
	if(!(fHeadingDelta >= MIN_ANG && fHeadingDelta <= MAX_ANG))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::CTaskMotionAimingTransition(const fwMvNetworkDefId& networkDefId, const fwMvClipSetId& clipSetId, const bool bLeftFoot, const bool bDoIndependentMoverExpression, const bool bStrafeTransition, const float fMovingDirection, const Direction direction)
: m_NetworkDefId(networkDefId)
, m_ClipSetId(clipSetId)
, m_fMovingDirection(FLT_MAX)
, m_fInitialMovingDirection(fMovingDirection)
, m_Direction(direction)
, m_bLeftFoot(bLeftFoot)
, m_bDoIndependentMoverExpression(bDoIndependentMoverExpression)
, m_bBlockWalk(false)
, m_bMoveBlendOut(false)
, m_bMoveStartAimIntro(false)
, m_bStrafeTransition(bStrafeTransition)
, m_bQuitDueToInput(false)
, m_bForceQuit(false)
, m_bWantsToDoIndependentMover(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_AIMING_TRANSITION);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::~CTaskMotionAimingTransition()
{
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskMotionAimingTransition::Debug() const
{
#if DEBUG_DRAW
	const CPed* pPed = GetPed();

	Vector3 vPedPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	Vector3 vCurrentMBR(pPed->GetMotionData()->GetCurrentMoveBlendRatio().x, pPed->GetMotionData()->GetCurrentMoveBlendRatio().y, 0.f);
	vCurrentMBR.Normalize();
	MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix()).Transform3x3(vCurrentMBR);
	grcDebugDraw::Line(vPedPos, vPedPos + vCurrentMBR, Color_orange, -1);

	Vector3 vDesiredMBR(pPed->GetMotionData()->GetDesiredMoveBlendRatio().x, pPed->GetMotionData()->GetDesiredMoveBlendRatio().y, 0.f);
	vDesiredMBR.Normalize();
	MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix()).Transform3x3(vDesiredMBR);
	grcDebugDraw::Line(vPedPos, vPedPos + vDesiredMBR, Color_purple, -1);
#endif // DEBUG_DRAW

	CTaskMotionBase::Debug();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char* CTaskMotionAimingTransition::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
	case State_Initial:					return "State_Initial";
	case State_Transition:				return "State_Transition";
	case State_Finish:					return "State_Finish";
	default:							taskAssert(0);		
	}
	return "State_Invalid";
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::SupportsMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state))
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::IsInMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) const
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAimingTransition::GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds)
{
	taskAssert(m_ClipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	taskAssert(pClipSet);

	static fwMvClipId s_walkClip("Walk_Switch_To_Strafe_bwd_cw_lf_-45",0x76A55A82);
	static fwMvClipId s_runClip("Run_Switch_To_Strafe_bwd_cw_lf_-45",0xA66A482A);
	static fwMvClipId s_sprintClip("Run_Switch_To_Strafe_bwd_cw_lf_-45",0xA66A482A);

	fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*pClipSet, speeds, clipNames);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::IsInMotion(const CPed* UNUSED_PARAM(pPed)) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAimingTransition::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	static fwMvClipId s_outClip("Walk_Switch_To_Strafe_bwd_cw_lf_-45",0x76A55A82);
	outClipSet = m_ClipSetId;
	outClip    = s_outClip;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CTaskMotionAimingTransition::CreatePlayerControlTask()
{
	return rage_new CTaskPlayerOnFoot();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::ShouldStartAimIntro() const
{
	if(m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.GetBoolean(ms_StartAimIntroId))
	{
		return true;
	}
	return m_bMoveStartAimIntro;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::ShouldBlendBackToStrafe() const
{
	if(m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.GetBoolean(ms_BlendBackToStrafe))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::Direction CTaskMotionAimingTransition::GetDirection() const
{
	return m_Direction;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::FSM_Return CTaskMotionAimingTransition::ProcessPreFSM()
{
	// Need to disable time slicing while we are doing the transition, as this can cause big anim pops
	GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::FSM_Return CTaskMotionAimingTransition::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Transition)
			FSM_OnEnter
				return StateTransition_OnEnter();
			FSM_OnUpdate
				return StateTransition_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAimingTransition::CleanUp()
{
	CPed* pPed = GetPed();
	if(pPed)
	{
		// Clear flag
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InStrafeTransition, false);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InToStrafeTransition, false);

		// Force us to start moving
		if(GetMotionData().GetIsLessThanRun())
		{
			pPed->GetMotionData()->SetForcedMotionStateThisFrame(CPedMotionStates::MotionState_Walk);
		}
		else
		{
			pPed->GetMotionData()->SetForcedMotionStateThisFrame(CPedMotionStates::MotionState_Run);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::ProcessMoveSignals()
{
	//Check the state.
	switch(GetState())
	{
		case State_Transition:
		{
			return StateTransition_OnProcessMoveSignals();
		}
		default:
		{
			break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::FSM_Return CTaskMotionAimingTransition::StateInitial_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Set flag for quick access
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InStrafeTransition, true);
	if(m_bStrafeTransition)
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InToStrafeTransition, true);

	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.RequestNetworkDef(m_NetworkDefId);

		// Check if clip sets are streamed in
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
		taskAssertf(pClipSet, "Clip set [%s] does not exist", m_ClipSetId.GetCStr());
		if(pClipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkDefStreamedIn(m_NetworkDefId))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, m_NetworkDefId);
			m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
			m_MoveNetworkHelper.SetFlag(m_bLeftFoot, ms_LeftFootId);

			TUNE_GROUP_BOOL(PED_MOVEMENT, RESTRICT_AIMING_TRANSITION_TO_RUN, false);
			if(RESTRICT_AIMING_TRANSITION_TO_RUN)
			{
				m_bBlockWalk = true;
				m_MoveNetworkHelper.SetFlag(false, ms_UseWalkId);
			}
			else
			{
				m_bBlockWalk = false;
				m_MoveNetworkHelper.SetFlag(true, ms_UseWalkId);
			}

			ASSERT_ONLY(VerifyClipSet(!m_bBlockWalk));
		}
	}

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(!m_MoveNetworkHelper.IsNetworkAttached())
		{
			m_MoveNetworkHelper.DeferredInsert();
		}

		SetState(State_Transition);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::FSM_Return CTaskMotionAimingTransition::StateTransition_OnEnter()
{
	RequestProcessMoveSignalCalls();
	m_bMoveBlendOut = false;
	m_bMoveStartAimIntro = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAimingTransition::FSM_Return CTaskMotionAimingTransition::StateTransition_OnUpdate()
{
	bool bBlendOut = m_bMoveBlendOut || m_bForceQuit;
	m_bMoveBlendOut = false;
	bool bMoveStartAimIntro = m_bMoveStartAimIntro;
	m_bMoveStartAimIntro = false;

	CPed* pPed = GetPed();

	float fVelHeading = FLT_MAX;
	pPed->GetVelocityHeading(fVelHeading);

	Vector3 vCurrentMBR(pPed->GetMotionData()->GetCurrentMoveBlendRatio().x, pPed->GetMotionData()->GetCurrentMoveBlendRatio().y, 0.f);
	float fMovingDirection = rage::Atan2f(-vCurrentMBR.x, vCurrentMBR.y);

	if(m_bStrafeTransition)
	{
		Vector3 vDesiredMBR(pPed->GetMotionData()->GetCurrentMoveBlendRatio().x, pPed->GetMotionData()->GetCurrentMoveBlendRatio().y, 0.f);
		vDesiredMBR.Normalize();
		fMovingDirection = rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y);
	}
	else
	{		
		fMovingDirection = SubtractAngleShorter(m_fInitialMovingDirection, pPed->GetCurrentHeading());
		m_fInitialMovingDirection = fwAngle::Lerp(m_fInitialMovingDirection, pPed->GetMotionData()->GetDesiredHeading(), PI * GetTimeStep());
	}

	// First frame, just set it
	if(m_fMovingDirection == FLT_MAX)
	{
		m_fMovingDirection = fMovingDirection;
	}
	else
	{
		float fPreviousMovingDirection = m_fMovingDirection;

		// Otherwise interpolate
		m_fMovingDirection = fwAngle::Lerp(m_fMovingDirection, fMovingDirection, PI * GetTimeStep());

		// Attempt to stop the direction going across 0.f
		float fMovingDelta = m_fMovingDirection - fPreviousMovingDirection;
		if(Abs(fMovingDelta) < HALF_PI)
		{
			static dev_float BOUNDARY = 0.1f;
			if(fPreviousMovingDirection <= -BOUNDARY && fPreviousMovingDirection + fMovingDelta > -BOUNDARY)
			{
				m_fMovingDirection = -BOUNDARY;
			}
			else if(fPreviousMovingDirection >= BOUNDARY && fPreviousMovingDirection + fMovingDelta < BOUNDARY)
			{
				m_fMovingDirection = BOUNDARY;
			}
		}
	}

	float fDirectionSignal = ConvertRadianToSignal(m_fMovingDirection);
	m_MoveNetworkHelper.SetFloat(ms_DirectionId, fDirectionSignal);

	// Forced initial direction?
	if(m_Direction == D_Invalid)
	{
		static dev_float DIRECTION_SIGNAL = 0.875f;
		if(m_bStrafeTransition)
		{
			m_Direction = fDirectionSignal >= DIRECTION_SIGNAL ? D_CCW : D_CW;
		}
		else
		{
			m_Direction = fDirectionSignal >= DIRECTION_SIGNAL ? D_CW : D_CCW;
		}
	}

	m_MoveNetworkHelper.SetFlag(m_Direction == D_CW, ms_CWId);

	TUNE_GROUP_FLOAT(PED_MOVEMENT, MIN_AIMING_TRANSITION_RUN_RATE, 0.7f, 0.f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, MAX_AIMING_TRANSITION_RUN_RATE, 1.f, 0.f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, AIMING_TRANSITION_STEALTH_RATE_MOD, 0.8f, 0.f, 2.f, 0.01f);

	// Send the speed
	const float fMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();
	if(m_bBlockWalk)
	{
		if(fMBR < MOVEBLENDRATIO_RUN)
		{
			// Adjust the rate
			float fRatio = Max(fMBR - MOVEBLENDRATIO_WALK, 0.f);
			float fRate = MIN_AIMING_TRANSITION_RUN_RATE + fRatio * (MAX_AIMING_TRANSITION_RUN_RATE - MIN_AIMING_TRANSITION_RUN_RATE);
			if(pPed->GetMotionData()->GetUsingStealth())
			{
				fRate *= AIMING_TRANSITION_STEALTH_RATE_MOD;
			}
			m_MoveNetworkHelper.SetFloat(ms_RateId, fRate);
		}
		
		m_MoveNetworkHelper.SetFloat(ms_SpeedId, MOVEBLENDRATIO_RUN);
	}
	else
	{
		float fSpeedSignal = fMBR / MOVEBLENDRATIO_SPRINT;
		m_MoveNetworkHelper.SetFloat(ms_SpeedId, fSpeedSignal);

		float fRate = 1.f;
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			fRate *= AIMING_TRANSITION_STEALTH_RATE_MOD;
		}
		m_MoveNetworkHelper.SetFloat(ms_RateId, fRate);
	}

	float fBorderMin = CTaskMotionAiming::BWD_FWD_BORDER_MIN;
	float fBorderMax = CTaskMotionAiming::BWD_FWD_BORDER_MAX;

	if(!m_bStrafeTransition && GetTimeRunning() > CTaskMotionPed::AIMING_TRANSITION_DURATION)
	{
		if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() <= square(MBR_WALK_BOUNDARY))
		{
			m_bQuitDueToInput = true;
			bBlendOut = true;
		}
		else
		{
			float fHeadingDiff = CalcDesiredDirection();
			switch(m_Direction)
			{
			case D_CW:
				fBorderMax = PI * 0.95f;
				break;
			case D_CCW:
				fBorderMin = -PI * 0.95f;
				break;
			default:
				break;
			}
			
			if(fHeadingDiff >= fBorderMin && fHeadingDiff <= fBorderMax)
			{
				m_bQuitDueToInput = true;
				bBlendOut = true;
			}
		}
	}

	// Blend out
	if(bBlendOut && !bMoveStartAimIntro)
	{
		if(m_bDoIndependentMoverExpression)
		{
			// Don't do if an expression is already active
// 			if(GetMotionData().GetIndependentMoverTransition() == 0)
// 			{
// 				float fCurrentHeading = pPed->GetCurrentHeading();
// 				float fDesiredHeading = pPed->GetDesiredHeading();
// 				float fFacingHeading  = pPed->GetFacingDirectionHeading();
// 
// 				// Allow the heading to move a bit from the facing to the desired
// 				static dev_float EXTRA_HEADING = QUARTER_PI;
// 				float fExtraHeading = Clamp(SubtractAngleShorter(fDesiredHeading, fFacingHeading), -EXTRA_HEADING, EXTRA_HEADING);
// 				float fNewDesiredHeading = fwAngle::LimitRadianAngle(fFacingHeading + fExtraHeading);
// 
// 				float fHeadingDelta = SubtractAngleShorter(fNewDesiredHeading, fCurrentHeading);
// 				SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionId, true);
// 
// 				// Inform the CTaskMotionPed to block transitions to aiming while the independent mover is active
// 				// Unfortunately this is not easily called from elsewhere
// 				if(GetParent() && GetParent()->GetParent() && GetParent()->GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
// 				{
// 					if(Abs(fHeadingDelta) > HALF_PI)
// 					{
// 						static_cast<CTaskMotionPed*>(GetParent()->GetParent())->SetBlockAimingTransitionTime(0.375f);
// 					}
// 				}
// 			}
			m_bWantsToDoIndependentMover = true;
		}
		else
		{
			// Finish
			return FSM_Quit;
		}
	}

	if(m_fMovingDirection >= fBorderMin && m_fMovingDirection <= fBorderMax)
	{
		float fHeadingDiff = SubtractAngleShorter(m_fMovingDirection, pPed->GetDesiredHeading());

		static dev_float MAX_HEADING_CHANGE = 0.2f;
		fHeadingDiff = Clamp(fHeadingDiff, -MAX_HEADING_CHANGE, MAX_HEADING_CHANGE);

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingDiff * GetTimeStep());
	}

	return FSM_Continue;	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAimingTransition::StateTransition_OnProcessMoveSignals()
{
	m_bMoveBlendOut |= m_MoveNetworkHelper.GetBoolean(ms_BlendOutId);
	m_bMoveStartAimIntro |= m_MoveNetworkHelper.GetBoolean(ms_StartAimIntroId);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskMotionAimingTransition::VerifyClipSet(const bool bCheckWalkAnims)
{
	if(m_ClipSetId != CLIP_SET_ID_INVALID)
	{
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", m_ClipSetId.TryGetCStr()))
		{
			atArray<CTaskHumanLocomotion::sVerifyClip> clips;
			// Run To Strafe
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CCW_LF_135",0x8c304854), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CCW_LF_180",0x01d6172e), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_LF_-45",0xa66a482a), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_LF_-90",0xf3fadde4), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_LF_180",0x457fe71d), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CCW_RF_135",0xc842bd2e), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CCW_RF_180",0xe2ebeccc), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_RF_-45",0x1f683ef8), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_RF_-90",0x27b2c9c4), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("RUN_SWITCH_TO_STRAFE_BWD_CW_RF_180",0xd5c3faa1), CTaskHumanLocomotion::VCF_MovementOneShot));
			// Strafe to Run
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_LF_-45",0xc0a13bcb), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_LF_-90",0xbcc33c57), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_LF_180",0x54a31a43), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CW_LF_180",0x5f26fef2), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CW_LF_135",0x7507b23b), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_RF_-45",0xc375350d), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_RF_-90",0x9a7ed685), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CCW_RF_180",0x5eddb31b), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CW_RF_135",0x5b30005a), CTaskHumanLocomotion::VCF_MovementOneShot));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_RUN_CW_RF_180",0xcef5e91c), CTaskHumanLocomotion::VCF_MovementOneShot));

			if(bCheckWalkAnims)
			{
				// Walk To Strafe
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CCW_LF_135",0xa0befb37), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CCW_LF_180",0xcfd20924), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_LF_-45",0x76a55a82), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_LF_-90",0x36da1638), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_LF_180",0xdfc455a7), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CCW_RF_135",0x25dd408c), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CCW_RF_180",0x9b0e2a64), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_RF_-45",0x85e4480b), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_RF_-90",0x902dae9d), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("WALK_SWITCH_TO_STRAFE_BWD_CW_RF_180",0x87a0b2c3), CTaskHumanLocomotion::VCF_MovementOneShot));
				// Strafe to Walk
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_LF_-45",0x85e4bf20), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_LF_-90",0xbfc6af2f), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_LF_180",0x3dcaa7c3), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CW_LF_180",0x270425e0), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CW_LF_135",0x959e903b), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_RF_-45",0x0b21a789), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_RF_-90",0xd94ec0b8), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CCW_RF_180",0xaa4bd4ff), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CW_RF_135",0xff56e816), CTaskHumanLocomotion::VCF_MovementOneShot));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("STRAFE_BWD_SWITCH_TO_WALK_CW_RF_180",0x8a07f9d2), CTaskHumanLocomotion::VCF_MovementOneShot));
			}

			CTaskHumanLocomotion::VerifyClips(m_ClipSetId, clips, GetEntity() ? GetPed() : NULL);
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////
