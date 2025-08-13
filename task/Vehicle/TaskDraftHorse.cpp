#include "Task/Vehicle/TaskDraftHorse.h"

#if ENABLE_HORSE

#include "animation/MovePed.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Horse/horseComponent.h"
#include "Peds/PedIntelligence.h"
#include "Task/Vehicle/TaskPlayerOnHorse.h"
#include "Task/default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "task/Movement/Jumping/TaskJump.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//
//#if AI_RIDE_DBG
//#define AI_RIDE_DBG_MSG(x) DebugMsgf x
//#else
//#define AI_RIDE_DBG_MSG(x)
//#endif
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//float CTaskMoveMountedSpurControl::sm_ClipFilterSpurThreshold = 0.1f;
//float CTaskMoveMountedSpurControl::sm_ClipFilterSpurThresholdRandomness = 0.5f;
//float CTaskMoveMountedSpurControl::sm_ClipFilterBrakeThreshold = 0.5f;
//bool CTaskMoveMountedSpurControl::sm_ClipFilterEnabled = true;
//#endif
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateSpurring_OnUpdate()
//{
//	if(!ShouldUseHorseSim())
//	{
//		SetState(State_NoSim);
//		return FSM_Continue;
//	}
//
//	const Tuning &tuning = m_Tuning;
//
//	const CHorseComponent& hrs = *GetPed()->GetHorseComponent();
//
//	const float currentNormSpeed = Max(hrs.GetSpeedCtrl().GetCurrSpeed(), 0.0f);
//
//	const float desiredNormSpeed = CalcTargetNormSpeed();
//
//	if((currentNormSpeed > desiredNormSpeed + tuning.Get(Tuning::kAiRideNormSpeedStartBrakeThreshold)) || desiredNormSpeed <= SMALL_FLOAT)
//	{
//		AI_RIDE_DBG_MSG((1, "Entering braking state."));
//		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
//		SetState(State_Braking);
//		return FSM_Continue;
//	}
//
//	const float dt = TIME.GetSeconds();
//
//	const float err = desiredNormSpeed - currentNormSpeed;
//	const float stickYP = tuning.Get(Tuning::kAiRideStickYProportionalConstant);
//	const float newStick = Clamp(m_CurrentStickY + stickYP*err*dt, 0.0f, 1.0f);		// Should -1.0f or 0.0f be the minimum here? /FF
//	m_CurrentStickY = newStick;
//
//	bool spur = false;
//
//	if((currentNormSpeed > desiredNormSpeed - tuning.Get(Tuning::kAiRideNormSpeedSpurThreshold)) && (desiredNormSpeed < SMALL_FLOAT || currentNormSpeed >= SMALL_FLOAT))
//	{
//		AI_RIDE_DBG_MSG((3, "No spur - speed high enough."));
//	}
//	else if(TIME.GetElapsedTime() < m_LastSpurTime + tuning.Get(Tuning::kAiRideMinTimeBetweenSpurs))
//	{
//		AI_RIDE_DBG_MSG((3, "No spur - not enough time has elapsed."));
//	}
//	else if(hrs.GetFreshnessCtrl().GetCurrFreshnessRelativeToMax() < tuning.Get(Tuning::kAiRideMinFreshnessForSpur))
//	{
//		AI_RIDE_DBG_MSG((3, "No spur - freshness too low."));
//	}
//	else if(hrs.GetAgitationCtrl().GetCurrAgitation() > tuning.Get(Tuning::kAiRideMaxAgitationForSpur))
//	{
//		AI_RIDE_DBG_MSG((3, "No spur - agitation too high."));
//	}
//	else
//	{
//		spur = true;
//
//		m_LastSpurTime = TIME.GetElapsedTime();
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//		// See if the speed increase we desire is large enough to exceed a threshold, which
//		// includes some random variation. If large enough, we let clips play. If this
//		// is really just a smaller speed increase we are spurring for, it tends to look better
//		// to just not play the spur clips, even if we would have done so if the player
//		// were riding. /FF
//		const float rand = g_ReplayRand.GetFloat()*sm_ClipFilterSpurThresholdRandomness;
//		if(!sm_ClipFilterEnabled || err > sm_ClipFilterSpurThreshold + rand)
//		{
//			msgOut.m_bAllowBrakeAndSpurAnimation = true;
//		}
//		else
//		{
//			msgOut.m_bAllowBrakeAndSpurAnimation = false;
//		}
//
//		AI_RIDE_DBG_MSG((1, "Spurring (%d)", (int)msgOut.m_bAllowBrakeAndSpurAnimation));
//#endif
//	}
//
//	ApplyInput(spur, 0.0f, newStick);
//	return FSM_Continue;
//}
//
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateBraking_OnUpdate()
//{
//	if(!ShouldUseHorseSim())
//	{
//		SetState(State_NoSim);
//		return FSM_Continue;
//	}
//
//	const Tuning &tuning = m_Tuning;
//
//	const float desiredNormSpeed = CalcTargetNormSpeed();
//
//	const CHorseComponent& hrs = *GetPed()->GetHorseComponent();
//
//	const float currentNormSpeed = Max(hrs.GetSpeedCtrl().GetCurrSpeed(), 0.0f);
//
//	const bool still = (currentNormSpeed <= SMALL_FLOAT);
//	if((currentNormSpeed < desiredNormSpeed + tuning.Get(Tuning::kAiRideNormSpeedStopBrakeThreshold)) || (desiredNormSpeed > SMALL_FLOAT && still))
//	{
//		AI_RIDE_DBG_MSG((1, "Entering spurring state"));
//		m_CurrentStickY = 0.0f;		// Start over at 0, I guess. /FF
//		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
//		SetState(State_Spurring);
//		return FSM_Continue;
//	}
//
//	float brake = (!still ? 1.0f : 0.0f);
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//	// Decide if we should allow brake clips to play or not. This is done
//	// if the desired slowdown exceeds a threshold, or if we are trying to
//	// slow down to a complete stop and are not already moving very slowly, or
//	// if we allowed the clip to play last frame. /FF
//	if(!sm_ClipFilterEnabled
//			|| currentNormSpeed > desiredNormSpeed + sm_ClipFilterBrakeThreshold
//			|| (desiredNormSpeed <= 0.01f && currentNormSpeed >= 0.1f)	// MAGIC!
//			|| (m_FlagClipatingBrake))
//	{
//		msgOut.m_bAllowBrakeAndSpurAnimation = true;
//	}
//	else
//	{
//		msgOut.m_bAllowBrakeAndSpurAnimation = false;
//	}
//#endif
//
//#if AI_RIDE_DBG
//	if(brake > 0.0f)
//	{
//		AI_RIDE_DBG_MSG((1, "Braking"));
//	}
//#endif
//
//	ApplyInput(false, brake, 0.0f);
//
//	return FSM_Continue;
//}
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateNoSim_OnUpdate()
//{
//	if(ShouldUseHorseSim())
//	{
//		SetState(State_Start);
//		return FSM_Continue;
//	}
//
//	GetPed()->GetMotionData()->SetDesiredMoveBlendRatio(m_InputTargetMoveBlendRatio);
//
//	return FSM_Continue;
//}
//
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateFinish_OnUpdate()
//{
//	return FSM_Quit;
//}
//
//#if AI_RIDE_DBG
//
//void CTaskMoveMountedSpurControl::DebugMsgf(int level, const char *fmt, ...) const
//{
//	if(m_DebugLevel < level)
//	{
//		return;
//	}
//
//	va_list args;
//	va_start(args, fmt);
//
//	char buff[256];
//	vformatf(buff, sizeof(buff), fmt, args);
//
//	Displayf("%d: %p %s", TIME.GetFrameCount(), (const void*)GetPed(), buff);
//}
//
//#endif
//
//CTaskMoveMountedSpurControl::Tuning::Tuning()
//{
//	Reset();
//}
//
//
//void CTaskMoveMountedSpurControl::Tuning::Reset()
//{
//	m_Params[kAiRideMaxAgitationForSpur] = 0.1f;
//	m_Params[kAiRideMinFreshnessForSpur] = 0.8f;
//	m_Params[kAiRideMinTimeBetweenSpurs] = 0.5f;
//	m_Params[kAiRideNormSpeedSpurThreshold] = 0.1f;
//	m_Params[kAiRideNormSpeedStartBrakeThreshold] = 0.2f;
//	m_Params[kAiRideNormSpeedStopBrakeThreshold] = 0.0f;
//	m_Params[kAiRideStickYProportionalConstant] = 5.0f;
//
//	// If you add new parameters, make sure to initialize them to
//	// reasonable values above, then update the count here. /FF
//	CompileTimeAssert(kNumAiRideParams == 7);
//}
//
////////////////////////////////////////////////////////////////////////////
//
//CTaskMoveMountedSpurControl::CTaskMoveMountedSpurControl()
//		: CTaskMove(MOVEBLENDRATIO_STILL)
//		, m_InputTargetMoveBlendRatio(0.0f)
//		, m_CurrentStickY(0.0f)
//		, m_LastSpurTime(-LARGE_FLOAT)
//#if AI_RIDE_DBG
//		, m_DebugLevel(0)
//#endif
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//		, m_FlagClipatingBrake(false)
//#endif
//{
//#if !__FINAL
//	m_eTaskType = CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL;
//#endif // !__FINAL
//}
//
//
//aiTask* CTaskMoveMountedSpurControl::Copy() const
//{
//	CTaskMoveMountedSpurControl* pTask = rage_new CTaskMoveMountedSpurControl();
//	pTask->m_InputTargetMoveBlendRatio = m_InputTargetMoveBlendRatio;
//	return pTask;
//}
//
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//{
//	FSM_Begin
//		FSM_State(State_Start)
//			FSM_OnEnter
//				return StateStart_OnEnter();
//			FSM_OnUpdate
//				return StateStart_OnUpdate();
//		FSM_State(State_Spurring)
//			FSM_OnUpdate
//				return StateSpurring_OnUpdate();
//		FSM_State(State_Braking)
//			FSM_OnUpdate
//				return StateBraking_OnUpdate();
//		FSM_State(State_NoSim)
//			FSM_OnUpdate
//				return StateNoSim_OnUpdate();
//		FSM_State(State_Finish)
//			FSM_OnUpdate
//				return StateFinish_OnUpdate();
//	FSM_End
//}
//
//
//float CTaskMoveMountedSpurControl::CalcTargetNormSpeed() const
//{
//	float desiredNormSpeed = m_InputTargetMoveBlendRatio/MOVEBLENDRATIO_SPRINT;
//
//	// If the desired speed is very small, make it 0. This is to prevent a problem where
//	// the state machine could potentially oscillate between the spurring and braking states,
//	// as both switch conditions would be fulfilled if
//	//	desiredNormSpeed <= SMALL_FLOAT && currentNormSpeed < desiredNormSpeed
//	// This was rarely happening in practice, but I did see the safety mechanism trip once,
//	// probably because of this. /FF
//	desiredNormSpeed = Selectf(desiredNormSpeed - (1.5f*SMALL_FLOAT), desiredNormSpeed, 0.0f);
//
//	return desiredNormSpeed;
//}
//
//#if !__FINAL
//
//const char * CTaskMoveMountedSpurControl::GetStaticStateName( s32 iState )
//{
//	static const char *s_StateNames[] =
//	{
//		"State_Start",
//		"State_Spurring",
//		"State_Braking",
//		"State_NoSim",
//		"State_Finish"
//	};
//
//	if(iState >= 0 && iState < kNumStates)
//	{
//		CompileTimeAssert(NELEM(s_StateNames) == kNumStates);
//		return s_StateNames[iState];
//	}
//	else
//	{
//		return "Invalid";
//	}
//}
//
//#endif	// !__FINAL
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateStart_OnEnter()
//{
//#if AI_RIDE_DBG
//	m_DebugLevel = 0;
//#endif
//
//	m_LastSpurTime = -LARGE_FLOAT;
//	m_CurrentStickY = 0.0f;
//
//	m_Tuning.Reset();
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//	m_FlagClipatingBrake = false;
//#endif
//
//
//	return FSM_Continue;
//}
//
//
//CTask::FSM_Return CTaskMoveMountedSpurControl::StateStart_OnUpdate()
//{
//	if(ShouldUseHorseSim())
//	{
//		CTaskMoveMounted* pTask = rage_new CTaskMoveMounted;
//		SetNewTask(pTask);
//
//		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
//		SetState(State_Spurring);
//	}
//	else
//	{
//		SetState(State_NoSim);
//	}
//
//	return FSM_Continue;
//}
//
//
//void CTaskMoveMountedSpurControl::ApplyInput(bool spur, float brake, float stickY)
//{
//	CTask* pSubTask = GetSubTask();
//	if(!Verifyf(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED, "Expected task not running"))
//	{
//		return;
//	}
//
//	CPed* pPed = GetPed();
//
//	CTaskMoveMounted* pMoveMountedTask = static_cast<CTaskMoveMounted*>(pSubTask);
//	float outputMatchSpeed = -1.0f;
//	Vector2 stick(0.0f, stickY);
//	pMoveMountedTask->ApplyInput(pPed->GetDesiredHeading(), spur, false, brake, false, false, stick, outputMatchSpeed);
//
//#if AI_RIDE_DBG
//	if(m_DebugLevel >= 2)
//	{
//		const CHorseComponent& hrs = *GetPed()->GetHorseComponent();
//
//		const int state = GetState();
//		Assert(state >= 0 && state < kNumStates);
//		DebugMsgf(2, "st: %-9s cur: %.2f des: %.2f ag: %.2f fr: %.2f y: %.2f %s%s", GetStateName(state),
//				hrs.GetSpeedCtrl().GetCurrSpeed(), CalcTargetNormSpeed(),
//				hrs.GetAgitationCtrl().GetCurrAgitation(),
//				hrs.GetFreshnessCtrl().GetCurrFreshnessRelativeToMax(),
//				m_CurrentStickY,
//				spur ? "Spur " : "",
//				(brake > 0.0f) ? "Brake" : "");
//	}
//#endif
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//	// Remember if we are braking and letting the brake clip play, so that we can
//	// make sure to allow this to happen on the next frame, assuming that we still want
//	// to slow down. /FF
//	m_FlagClipatingBrake = msg.m_bAllowBrakeAndSpurAnimation && (brake > 0.0f);
//#endif
//}
//
//
//bool CTaskMoveMountedSpurControl::ShouldUseHorseSim() const
//{
//	//	TUNE_BOOL(USE_HORSE_SIM, true);
//	//	return USE_HORSE_SIM;
//	return true;
//}
//
//#undef AI_RIDE_DBG_MSG

//////////////////////////////////////////////////////////////////////////

//
CTaskMoveDraftAnimal::CTaskMoveDraftAnimal()
:	CTaskMove(MOVEBLENDRATIO_STILL)
, m_DesiredHeading(0)
, m_fTimeSinceSpur(0.0f)
, m_bReverseRequested(false)
, m_fReverseDelay(0)
{
	ResetInput();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_DRAFT_ANIMAL);
}

CTaskMoveDraftAnimal::~CTaskMoveDraftAnimal()
{
}

CTask::FSM_Return CTaskMoveDraftAnimal::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(AcceptingInput)
			FSM_OnUpdate
				return StateAcceptingInput_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveDraftAnimal::StateAcceptingInput_OnUpdate(CPed* pPed)
{	

	float dt = TIME.GetSeconds();	
	static dev_float MIN_TIME_BETWEEN_SPUR = 0.15f;
	if(m_bSpurredThisFrame) {
		if (m_fTimeSinceSpur < MIN_TIME_BETWEEN_SPUR)
		{
			m_bSpurredThisFrame=false; //too soon		
		}
		else
			m_fTimeSinceSpur = 0.0f;
	} 
	else
		m_fTimeSinceSpur += dt;

	// Let the movement system know that the player is in direct control of this ped
	Assert(pPed->GetHorseComponent());
	pPed->GetHorseComponent()->UpdateInput(m_bSpurredThisFrame, m_fTimeSinceSpur, m_bHardBrake, m_fSoftBrake, m_bMaintainSpeed, m_bIsAiming, m_Stick, m_CurrentMatchSpeed);

	float speed = GetPed()->GetHorseComponent()->GetSpeedCtrl().GetCurrSpeed();
	//Displayf("Speed: %f", speed);
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(speed*3.0f, 0.0f);	
	if (speed !=0) //don't adjust heading when not moving (turn in place)
		pPed->GetHorseComponent()->AdjustHeading(m_DesiredHeading);
	pPed->SetDesiredHeading(m_DesiredHeading);
	

	//reverse logic
	static const float REVERSE_TIMER = 0.5f;	
	float currentMBR=pPed->GetMotionData()->GetCurrentMbrY();
	if (m_bReverseRequested)
	{
		if (currentMBR<=0.0f)
			m_fReverseDelay-=dt;
		else 
			m_fReverseDelay = REVERSE_TIMER;

		if (m_fReverseDelay<=0.0f)
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_REVERSE, 0.0f);		
	} else if (currentMBR<=0.0f)
		m_fReverseDelay = 0;

	CPed* rider = pPed->GetHorseComponent()->GetRider();
	if (rider && pPed->GetHorseComponent()->GetAgitationCtrl().GetCurrAgitation() >= 0.9f) {
		//kick my rider
		if (pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) 
		{
			static_cast<CTaskHorseLocomotion*>(pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestRearUp(true);							
		}		
	}

	//Reset input states
	ResetInput();

	return FSM_Continue;
}



//////////////////////////////////////////////////////////////////////////
void CTaskMoveDraftAnimal::ApplyInput(float fDesiredHeading, bool bSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bIsAiming, Vector2& vStick, float fMatchSpeed) {
	m_bSpurredThisFrame = bSpur;	
	m_CurrentMatchSpeed = fMatchSpeed;
	m_Stick.Set(vStick);	
	m_bMaintainSpeed = bMaintainSpeed;
	m_bIsAiming = bIsAiming;
	m_bHardBrake = bHardBrake;
	m_fSoftBrake = fBrake;
	m_DesiredHeading = fDesiredHeading;
}

void CTaskMoveDraftAnimal::ResetInput() {
	m_bSpurredThisFrame = false;	
	m_CurrentMatchSpeed = -1.0f;
	m_Stick.Zero();	
	m_bMaintainSpeed = false;
	m_bIsAiming = false;
	m_bHardBrake = false;
	m_fSoftBrake = 0;
	m_bReverseRequested=false;;
}

void CTaskMoveDraftAnimal::RequestReverse()
{
	m_bReverseRequested=true;
}
//////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE