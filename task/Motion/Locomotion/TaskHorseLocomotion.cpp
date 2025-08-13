#include "TaskHorseLocomotion.h"

#if ENABLE_HORSE

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"

AI_OPTIMISATIONS()

#include "Animation/MovePed.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "event/ShockingEvents.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Horse/horseComponent.h"
#include "Peds/PedIntelligence.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "task/Movement/TaskIdle.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskSlopeScramble.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskPlayerOnHorse.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/System/MotionTaskData.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "task/Movement/TaskIdle.h"

//////////////////////////////////////////////////////////////////////////
//	Horse Locomotion
//////////////////////////////////////////////////////////////////////////
// Instance of tunable task params
CTaskHorseLocomotion::Tunables CTaskHorseLocomotion::sm_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskHorseLocomotion, 0x5feda31d);

// Instance of tunable task params
CTaskRiderRearUp::Tunables CTaskRiderRearUp::sm_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskRiderRearUp, 0x3b496c10);

// States
const fwMvStateId CTaskHorseLocomotion::ms_LocomotionStateId("Locomotion",0xB65CBA9D);
const fwMvStateId CTaskHorseLocomotion::ms_IdleStateId("Idles",0xF29E80A8);
const fwMvStateId CTaskHorseLocomotion::ms_TurnInPlaceStateId("TurnInPlace",0x8F54E5AD);
const fwMvStateId CTaskHorseLocomotion::ms_NoClipStateId("NoClip",0x23B4087C);

const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterStopId("OnEnterStop",0x66d6af9f);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterLocomotionId("OnEnterLocomotion",0x96273A49);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterQuickStopId("OnEnterQuickStop",0x26E1E521);
const fwMvBooleanId CTaskHorseLocomotion::ms_QuickStopClipEndedId("QuickStopEnded",0x8ECB3922);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterRearUpId("OnEnterRearUp",0xF912052);
const fwMvBooleanId CTaskHorseLocomotion::ms_RearUpClipEndedId("RearUpEnded",0xD51F9C9F);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterTurnInPlaceId("OnEnterTurnInPlace", 0xA7F587A6);
const fwMvBooleanId CTaskHorseLocomotion::ms_OnEnterNoClipId("OnEnterNoClip",0x4F96D612);

const fwMvRequestId CTaskHorseLocomotion::ms_startIdleId("StartIdle",0x2A8F6981);
const fwMvRequestId CTaskHorseLocomotion::ms_startLocomotionId("StartLocomotion",0x15F666A1);
const fwMvRequestId CTaskHorseLocomotion::ms_startStopId("StartStop",0x54e62062);
const fwMvRequestId CTaskHorseLocomotion::ms_TurninPlaceId("TurnInPlace",0x8F54E5AD);
const fwMvRequestId CTaskHorseLocomotion::ms_QuickStopId("QuickStop",0x951809DB);
const fwMvRequestId CTaskHorseLocomotion::ms_RearUpId("RearUp",0xACB2E6B);
const fwMvRequestId CTaskHorseLocomotion::ms_OverSpurId("OverSpur",0xF7A389E);
const fwMvRequestId CTaskHorseLocomotion::ms_CancelQuickStopId("CancelQuickStop",0x619123);
const fwMvRequestId CTaskHorseLocomotion::ms_InterruptAnimId("InterruptAnim",0xD50E0DF9);
const fwMvRequestId CTaskHorseLocomotion::ms_StartNoClipId("StartNoClip",0xBC7581E7);

const fwMvFloatId CTaskHorseLocomotion::ms_MoveDesiredSpeedId("DesiredSpeed",0xCF7BA842);
const fwMvFloatId CTaskHorseLocomotion::ms_AnimPhaseId("AnimPhase",0x3B80902B);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveTurnPhaseId("TurnPhase",0x9B1DE516);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveSlopePhaseId("SlopePhase",0x1F6A5972);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveWalkPhaseId("WalkPhase",0x347BDDC4);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveTrotPhaseId("TrotPhase",0x7E9788A4);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveCanterPhaseId("CanterPhase",0x65352AC3);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveGallopPhaseId("GallopPhase",0xE1B31487);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveWalkRateId("WalkRate",0xE5FE5F4C);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveTrotRateId("TrotRate",0xB89DC56F);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveCanterRateId("CanterRate",0x8626FE39);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveGallopRateId("GallopRateX",0x1F6549DB);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveLeftToRightControlId("LeftToRightControl",0xA4629E89);
const fwMvFloatId CTaskHorseLocomotion::ms_MoveQuickStopDirectionId("QuickStopDirection",0xB5E10884);
const fwMvFloatId CTaskHorseLocomotion::ms_IntroBlendId("IntroBlend",0x6923c910);

const fwMvClipId CTaskHorseLocomotion::ms_IntroClip0Id("IntroClip0",0x79b9576f);
const fwMvClipId CTaskHorseLocomotion::ms_IntroClip1Id("IntroClip1",0x8bfb7bf3);

const fwMvFlagId CTaskHorseLocomotion::ms_NoIntroFlagId("NoIntro", 0x4d314f59);

CTaskHorseLocomotion::CTaskHorseLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_HorseGetupReqHelper()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
, m_CurrentSpeed(0)
, m_TurnPhase(0.5f)
, m_SlopePhase(0.5f)
, m_TurnPhaseGoal(0.5f)
, m_fTurnHeadingLerpRate(1.0f)
, m_fNextGait(0)
, m_fCurrentGait(0)
, m_fDesiredSpeed(0)
, m_fTransitionPhase(1.0f)
, m_fSustainedRunTime(0)
, m_fInitialDirection(0.0f)
, m_fRumbleDelay(0.0f)
, m_bOverSpurRequested(false)
, m_bQuickStopRequested(false)
, m_bRearUpRequested(false)
, m_bRearUpBuckRiderRequested(false)
, m_bCancelQuickStopRequested(false)
, m_bPursuitMode(false)
, m_bMovingForForcedMotionState(false)
, m_eQuickStopDirection(QUICKSTOP_FORWARD)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_HORSE);
}

//////////////////////////////////////////////////////////////////////////

CTaskHorseLocomotion::~CTaskHorseLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskHorseLocomotion::ProcessPreFSM()
{
	// Request the getup animations.
	if (! m_HorseGetupReqHelper.IsLoaded())
	{
		m_HorseGetupReqHelper.Request(GetPed()->GetPedModelInfo()->GetGetupSet());
	}

	float fCurrentMbrX, fCurrentMbrY;
	GetMotionData().GetCurrentMoveBlendRatio(fCurrentMbrX, fCurrentMbrY);

	float fDesiredMbrX, fDesiredMbrY;
	GetMotionData().GetGaitReducedDesiredMoveBlendRatio(fDesiredMbrX, fDesiredMbrY);

	TUNE_GROUP_FLOAT (HORSE_MOVEMENT, fHorseAccelRate, 2.0f, 0.0f, 10.0f, 0.0001f);

	InterpValue(fCurrentMbrX, fDesiredMbrX, fHorseAccelRate);
	InterpValue(fCurrentMbrY, fDesiredMbrY, fHorseAccelRate);

	if (fCurrentMbrY < 0.0f && fDesiredMbrY >=0.0f ) //if no longer reversing, skip back to forward movement 
		fCurrentMbrY = fDesiredMbrY;

	GetMotionData().SetCurrentMoveBlendRatio(fCurrentMbrY, fCurrentMbrX);

	UpdateSlopePhase();

	//! DMKH. Set slope normal. This is used in various tests. Note: To prevent doing further tests, just use ground normal. We might
	//! want to average this out using a couple of probes (as the player ped does).
	CPed* pPed = GetPed(); 
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	pPed->SetSlopeNormal(pPed->GetGroundNormal());

	if (GetIsUsingPursuitMode())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
	}

	// For humans with AL_LodMotionTask, CTaskMotionBasicLocomotionLowLod would be running
	// and enable timeslicing (unblocking AL_LodTimesliceIntelligenceUpdate). For animals,
	// we don't use that task, so we manually turn on timeslicing here.
	if (CanBeTimesliced())
	{
		CPedAILod& lod = pPed->GetPedAiLod();
		lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}

	if (ShouldForceNoTimeslicing())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	return FSM_Continue;
}

bool CTaskHorseLocomotion::ShouldUseTighterTurns() const
{
	const CPed* pPed = GetPed();

	// Require the movement task flag.
	if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings))
	{
		return false;
	}

	// Formations involving the player need to be smooth.
	bool bInFormation = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION) != NULL;
	bool bCarryingPlayer = GetIsCarryingPlayer();
	if (bInFormation && bCarryingPlayer)
	{
		return false;
	}

	bool bIsFleeing = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE) != NULL;
	if (bIsFleeing)
	{
		return false;
	}

	return true;
}

void CTaskHorseLocomotion::UpdateSlopePhase()
{
	if (m_networkHelper.IsNetworkActive()) {
		//Update slope
		float slopePhase =0;

		//Ground Normal method
#if 0
		Vector3 groundNormal = GetPed()->GetGroundNormal();
		groundNormal.z=0; 
		if ( groundNormal.IsNonZero() ) 
		{
			/*groundNormal.Normalize();
			float phi = GetPed()->GetGroundNormal().Angle( ZAXIS );
			const float modulate = VEC3V_TO_VECTOR3( GetPed()->GetTransform().GetB() ).Dot( groundNormal );
			slopePhase = modulate * phi;
			slopePhase=-slopePhase;*/
			dev_float inclineRange = 30.0f*DtoR;
			//slopePhase = Min (inclineRange, slopePhase)/inclineRange;

			Vector3 rotation;
			MAT34V_TO_MATRIX34( GetPed()->GetMatrix()).ToEulersFastXYZ(rotation);
			Displayf ("X Y Z = %f, %f, %f", rotation.x, rotation.y, rotation.z );
			slopePhase = Min (inclineRange, rotation.x)/inclineRange;
		}
#else	//Matrix extraction
		dev_float inclineRange = 30.0f*DtoR;
		Vector3 rotation;
		MAT34V_TO_MATRIX34( GetPed()->GetMatrix()).ToEulersFastXYZ(rotation);
		//Displayf ("X Y Z = %f, %f, %f", rotation.x, rotation.y, rotation.z );
		slopePhase = Min (inclineRange, rotation.x)/inclineRange;
#endif
		m_SlopePhase = Clamp((slopePhase+1.0f)*0.5f, 0.0f, 1.0f);

		m_networkHelper.SetFloat(ms_MoveSlopePhaseId, m_SlopePhase);
		//Displayf("Slope: %f ", slopePhase);
	}

}

bool CTaskHorseLocomotion::GetIsCarryingPlayer() const
{
	const CPed* pPed = GetPed();	
	return pPed->GetHorseComponent() && pPed->GetHorseComponent()->GetRider() && pPed->GetHorseComponent()->GetRider()->IsPlayer();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskHorseLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	UpdateGaitReduction();
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

	FSM_State(State_TurnInPlace)
		FSM_OnEnter
		return StateTurnInPlace_OnEnter(); 
	FSM_OnUpdate
		return StateTurnInPlace_OnUpdate();

	FSM_State(State_Locomotion)
		FSM_OnEnter
		return StateLocomotion_OnEnter(); 
	FSM_OnUpdate
		return StateLocomotion_OnUpdate(); 

	FSM_State(State_Stop)
		FSM_OnEnter
		return StateStop_OnEnter(); 
	FSM_OnUpdate
		return StateStop_OnUpdate(); 

	FSM_State(State_QuickStop)
		FSM_OnEnter
		return StateQuickStop_OnEnter(); 
	FSM_OnUpdate
		return StateQuickStop_OnUpdate(); 

	FSM_State(State_RearUp)
		FSM_OnEnter
		return StateRearUp_OnEnter(); 
	FSM_OnUpdate
		return StateRearUp_OnUpdate(); 

	FSM_State(State_Slope_Scramble)
		FSM_OnEnter
			StateSlopeScramble_OnEnter(); 
	FSM_OnUpdate
		return StateSlopeScramble_OnUpdate(); 

	FSM_State(State_Dead)
		FSM_OnEnter
		StateDead_OnEnter(); 
	FSM_OnUpdate
		return StateDead_OnUpdate(); 

	FSM_State(State_DoNothing)
		FSM_OnEnter
		StateDead_OnEnter(); 
	FSM_OnUpdate
		return StateDead_OnUpdate(); 

	FSM_End
}

//////////////////////////////////////////////////////////////////////////
bool CTaskHorseLocomotion::ProcessMoveSignals()
{
	s32 iState = GetState();

	// Tell the task to call ProcessPhysics so the animated velocity can be manipulated.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

	switch(iState)
	{
	case State_Initial:
		{
			return true;
		}
// 	case State_Idle:
// 		{
// 			StateIdle_OnProcessMoveSignals();
// 			return true;
// 		}
// 	case State_Locomotion:
// 		{
// 			StateLocomotion_OnProcessMoveSignals();
// 			return true;
// 		}
// 	case State_Stop:
// 		{
// 			StateStopLocomotion_OnProcessMoveSignals();
// 			return true;
// 		}
// 	case State_TurnInPlace:
// 		{
// 			StateTurnInPlace_OnProcessMoveSignals();
// 			return true;
// 		}
	case State_DoNothing:
	case State_Dead:
		{
			StateDead_OnProcessMoveSignals();
			return true;
		}
	default:
		{
			return false;
		}
	}
}

// PURPOSE: Manipulate the animated velocity to achieve more precise turning than the animation alone would allow.
bool	CTaskHorseLocomotion::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{

	if (GetPed()->GetHorseComponent() && GetPed()->GetHorseComponent()->GetRider())
		return false;

	s32 iState = GetState();
	CPed* pPed = GetPed();
	float fRotateSpeed = pPed->GetDesiredAngularVelocity().z;
	const float fDesiredDirection = CalcDesiredDirection();
	float fDesiredHeading = pPed->GetDesiredHeading();
	float fCurrentHeading = pPed->GetCurrentHeading();

	// Compute the "ideal" angular velocity - what would get us to the desired heading this frame.
	const float fDesiredAngularVelocity = fDesiredDirection / fTimeStep;

// 	if (iState == State_Idle && GetPreviousState() == State_TurnInPlace)
// 	{
// 		// Note if the turn is blending out.
// 		// The IgnoreMoverBlendRotationOnly_filter seems to be causing pops with quadrupeds, so we are hacking this in code.
// 		if (m_bTurnInPlaceIsBlendingOut && !pPed->GetIsAttached())
// 		{
// 			fRotateSpeed = 0.0f;
// 			pPed->SetDesiredAngularVelocity(Vec3V(0.0f, 0.0f, fRotateSpeed));
// 			pPed->SetHeading(fDesiredHeading);
// 			return true;
// 		}
// 	}
// 	else 
	if (iState == State_TurnInPlace)
	{
		float fCurrentHeadingAfterThisFrame = fwAngle::LimitRadianAngleSafe((fRotateSpeed * fTimeStep) + pPed->GetCurrentHeading());
		float fPredictedDesiredDirection = SubtractAngleShorter(fDesiredHeading, fCurrentHeadingAfterThisFrame);

		// If about to overshoot the desired heading, set the heading to be the desired heading.
		if (Sign(fDesiredDirection) != Sign(fPredictedDesiredDirection)  && fabs(fDesiredDirection - fPredictedDesiredDirection) < HALF_PI  && !pPed->GetIsAttached())
		{
			pPed->SetHeading(fDesiredHeading);
			fRotateSpeed = 0.0f;

			// Force the angular velocity to be the new value calculated above.
			pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));

			return true;
		}
	}
	else if (iState == State_Locomotion )
	{
		// Figure out how much velocity is needed in the clip to do tighter turning.
		float fVelocityTolerance = sm_Tunables.m_InMotionAlignmentVelocityTolerance;

		// When sprinting and other tasks have noted that tighter turning is desired, enforce a lower velocity tolerance than normal.
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettingsForScript))
		{
			// Only when sprinting.
			if (m_fCurrentGait == MOVEBLENDRATIO_SPRINT)
			{
				fVelocityTolerance = sm_Tunables.m_InMotionTighterTurnsVelocityTolerance;
			}
		}

		// If there is enough turning already going on in the clip, then we can probably get away with adding a bit more to achieve tighter turns.
		if (fabs(fRotateSpeed) > fVelocityTolerance)
		{
			// Check if the turn velocity should be scaled to achieve tighter turning.
			if (ShouldScaleTurnVelocity(fDesiredDirection, fRotateSpeed))
			{
				const float fScaleFactor = GetTurnVelocityScaleFactor();
				fRotateSpeed = fScaleFactor * fRotateSpeed;
			}

			// Lerp towards the desired angular velocity from the extracted angular velocity.
			float fRate = GetIsUsingPursuitMode() ? sm_Tunables.m_PursuitModeExtraHeadingRate : sm_Tunables.m_ProcessPhysicsApproachRate;
			Approach(fRotateSpeed, fDesiredAngularVelocity, fRate, fTimeStep);
		}

		float fCurrentHeadingAfterThisFrame = fwAngle::LimitRadianAngleSafe((fRotateSpeed * fTimeStep) + fCurrentHeading);
		float fPredictedDesiredDirection = SubtractAngleShorter(fDesiredHeading, fCurrentHeadingAfterThisFrame);

		// If about to overshoot the desired heading, set the heading to be the desired heading.
		if (Sign(fDesiredDirection) != Sign(fPredictedDesiredDirection) && fabs(fDesiredDirection - fPredictedDesiredDirection) < HALF_PI  && !pPed->GetIsAttached())
		{
			pPed->SetHeading(fDesiredHeading);
			fRotateSpeed = 0.0f;
		}

		// Force the angular velocity to be the new value calculated above.
		pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
CTask * CTaskHorseLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskHorseLocomotion::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}

	if (m_IdleNetworkHelper.GetNetworkPlayer())
	{
		m_IdleNetworkHelper.ReleaseNetworkPlayer();
	}

	if (m_HorseGetupReqHelper.IsLoaded())
	{
		m_HorseGetupReqHelper.Release();
	}

	ResetGaitReduction();
}

//////////////////////////////////////////////////////////////////////////

void CTaskHorseLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (clipSet)
	{
		static const fwMvClipId s_walkClip("walk",0x83504C9C);
		static const fwMvClipId s_runClip("trot",0x9EB9D90);
		static const fwMvClipId s_SprintClip("gallop_r",0x3A7BB298);

		const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_SprintClip };

		RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
Vec3V_Out CTaskHorseLocomotion::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
{
	if (GetState() == State_QuickStop)
	{
		Vector3 vDesiredVelocity  = VEC3V_TO_VECTOR3(CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep));
		float desiredSpeed = vDesiredVelocity.Mag();
		if (desiredSpeed > m_fQuickStopMaxSpeed)
		{
			taskAssert(desiredSpeed>0);
			vDesiredVelocity.Scale(m_fQuickStopMaxSpeed/desiredSpeed);
			return VECTOR3_TO_VEC3V(vDesiredVelocity);
		} else
			return VECTOR3_TO_VEC3V(vDesiredVelocity);
	}
	else
		return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);
}

//////////////////////////////////////////////////////////////////////////
Vec3V_Out CTaskHorseLocomotion::CalcDesiredAngularVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{	
	if (GetState()==State_Locomotion && GetPed()->GetHorseComponent() && GetPed()->GetHorseComponent()->GetRider())
	{
		Vector3 vAngVel = VEC3_ZERO;				
		float fTotalHeadingVel = GetPed()->GetHorseComponent()->GetSpeedCtrl().GetTurnRate();
		if (m_TurnPhase>=0.5f)
			fTotalHeadingVel = fTotalHeadingVel * (m_TurnPhase-0.5f) * 2.0f;
		else
			fTotalHeadingVel = -fTotalHeadingVel * (1.0f-m_TurnPhase/0.5f);

		vAngVel += VEC3V_TO_VECTOR3(updatedPedMatrixIn.c()) * (fTotalHeadingVel);
		return VECTOR3_TO_VEC3V(vAngVel);
	}
	else
		return CTaskMotionBase::CalcDesiredAngularVelocity(updatedPedMatrixIn, fTimestep);
}


//////////////////////////////////////////////////////////////////////////

void CTaskHorseLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	static bank_float sf_PitchLimitTuning = 1.0f;
	fMinOut = -sf_PitchLimitTuning;
	fMaxOut =  sf_PitchLimitTuning;
}


//////////////////////////////////////////////////////////////////////////
// TODO - make this dependent on the stop locomotion clip.
float CTaskHorseLocomotion::GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* bOutIsClipActive)
{
	if (HasQuickStops())
	{
		if (bOutIsClipActive)
		{
			*bOutIsClipActive = GetState() == State_Stop;
		}

		float fGait = m_fCurrentGait;
		if (fGait <= MOVEBLENDRATIO_WALK)
		{
			return GetTunables().m_StoppingDistanceWalkMBR;
		}
		else if (fGait <= MOVEBLENDRATIO_RUN)
		{
			return GetTunables().m_StoppingDistanceRunMBR;
		}
		else
		{
			return GetTunables().m_StoppingDistanceGallopMBR;
		}
	}
	else
	{
		//horse style
		u32 state = GetState();
		if ((state == State_Initial) || (state == State_Idle) || (state == State_TurnInPlace))
			return 0.f;

		TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fVelocityToStoppingDistanceSprintCoeff, 0.25f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fVelocityToStoppingDistanceRunCoeff, 0.25f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fVelocityToStoppingDistanceWalkCoeff, 0.2f, 0.0f, 10.0f, 0.01f);

		float fCoeff = 1.0f;

		if (GetMotionData().GetIsSprinting())
		{
			fCoeff = fVelocityToStoppingDistanceSprintCoeff;
		} else if (GetMotionData().GetIsRunning())
		{
			fCoeff = fVelocityToStoppingDistanceRunCoeff;
		} else
		{
			fCoeff = fVelocityToStoppingDistanceWalkCoeff;
		}
		return fCoeff * GetMotionData().GetCurrentMbrY();
	}
}


//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateInitial_OnEnter()
{

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateInitial_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(!m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHorse);

		// Check if clip sets are streamed in
		bool bStreamedIn = false;
		fwClipSet* pMovementClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssertf(pMovementClipSet, "Movement clip set [%s] does not exist", m_clipSetId.GetCStr());
		bStreamedIn = pMovementClipSet->Request_DEPRECATED();

		if(bStreamedIn && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHorse))
		{
			m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootHorse);

			AddIdleNetwork(pPed);
		}
	}

	if(m_networkHelper.IsNetworkActive())
	{
		if(!m_networkHelper.IsNetworkAttached())
		{
			m_networkHelper.DeferredInsert();
		}

		m_networkHelper.SetClipSet( m_clipSetId );

		//Set MoVE Flags
		fwClipSet *pClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
		if (pClipSet)
		{			
			//See if this one supports turn in place stops
			if (pClipSet->GetClip(fwMvClipId("idl_rgt_tur_stop",0x723d8358)) != NULL)
			{
				m_networkHelper.SetFlag(true, fwMvFlagId("UseTIPStops", 0xcffb5372));				
			} 

			//Old or new turns (TODO ax this when all clipsets are using new naming convention
			if (pClipSet->GetClip(fwMvClipId("walk_turn_r4",0x2de2d094)) == NULL)
			{
				m_networkHelper.SetFlag(true, fwMvFlagId("UseOldTurnNames", 0x4b5cb659));				
			} 
		}

		const CMotionTaskDataSet* pSet = GetPed()->GetMotionTaskDataSet();
		if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
		{
			const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
			m_networkHelper.SetFlag(pMotionStruct->m_HasDualGaits, fwMvFlagId("UseDualGaits", 0x5c8a6f00));
			m_networkHelper.SetFlag(pMotionStruct->m_HasSlopeGaits, fwMvFlagId("UseSlopes", 0xac26f9aa));
			m_networkHelper.SetFlag(pMotionStruct->m_HasCmplxTurnGaits, fwMvFlagId("UseComplexTurns", 0xdae8998f));
			m_networkHelper.SetFlag(pMotionStruct->m_CanTrot, fwMvFlagId("CanTrot",0x2CD3E75C));			
			m_networkHelper.SetFlag(pMotionStruct->m_CanCanter, fwMvFlagId("CanCanter",0xf6d8a162));
			m_networkHelper.SetFlag(pMotionStruct->m_CanReverse, fwMvFlagId("CanReverse",0x59047cec));
		}

		switch(GetMotionData().GetForcedMotionStateThisFrame())
		{
		case CPedMotionStates::MotionState_Idle:
			{
				m_networkHelper.ForceStateChange(ms_IdleStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
				SetState( State_Idle );
				break;
			}
		case CPedMotionStates::MotionState_Walk:
			{
				m_networkHelper.ForceStateChange(ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_WALK);
				m_fCurrentGait = MOVEBLENDRATIO_WALK;
				m_bMovingForForcedMotionState = true;
				SetState(State_Locomotion);
				break;
			}
		case CPedMotionStates::MotionState_Run:
			{
				m_networkHelper.ForceStateChange(ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_RUN);
				m_fCurrentGait = MOVEBLENDRATIO_RUN;
				m_bMovingForForcedMotionState = true;
				SetState(State_Locomotion);
				break;
			}
		case CPedMotionStates::MotionState_Sprint:
			{
				m_networkHelper.ForceStateChange(ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				m_fCurrentGait = MOVEBLENDRATIO_SPRINT;
				m_bMovingForForcedMotionState = true;
				SetState(State_Locomotion);
				break;
			}
		case CPedMotionStates::MotionState_Dead:
			{
				m_networkHelper.ForceStateChange(ms_NoClipStateId);
				SetState(State_Dead);
				break;
			}
		case CPedMotionStates::MotionState_DoNothing:
			{
				m_networkHelper.ForceStateChange(ms_NoClipStateId);
				SetState(State_DoNothing);
				break;
			}
		default:
			{
				if (abs(pPed->GetMotionData()->GetCurrentMbrY()) > 0.0f)
				{
					//enter the basic locomotion state by default
					SetState(State_Locomotion);
				}
				else
				{
					SetState(State_Idle);
					m_networkHelper.ForceNextStateReady();
				}
				break;
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateLocomotion_OnEnter()
{
	m_networkHelper.SendRequest( ms_startLocomotionId);
	SetIntroClipandBlend();
	m_networkHelper.WaitForTargetState(ms_OnEnterLocomotionId);
	m_TurnPhase = m_TurnPhaseGoal = m_SlopePhase = 0.5f; m_LeftToRightGaitBlendPhase = 0;
	UpdateSlopePhase();
	m_fCurrentGait = GetMotionData().GetDesiredMbrY() < 0 ? -1.0f : 0.0f;
	m_fTransitionPhase = 1.0f;
	m_fSustainedRunTime = 0;		

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();

	// Done with forcing the motion state now that the qped is in locomotion.
	m_bMovingForForcedMotionState = false;

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskHorseLocomotion::StateLocomotion_OnUpdate()
{

	CPed* horse = GetPed();
	CHorseComponent* horseComponent = horse->GetHorseComponent(); 	

 	float delta = fwTimer::GetTimeStep();

	ProcessGaits();
	SendLocomotionMoveSignals(horse);

	float headingDelta = CalcDesiredDirection() * 2.0f;//HACK turn intensity
	if (headingDelta<-PI) 
		headingDelta = -PI;
	else if (headingDelta > PI)
		headingDelta = PI;

	static dev_float GallopTurnApproachRate = 1.5f;	 
	static dev_float WalkTurnApproachRate = 1.5f;	 
	static dev_float ReverseTurnApproachRate = 1.5f;	 
	float prevTurnPhaseGoal = m_TurnPhaseGoal;
	m_TurnPhaseGoal = (headingDelta/PI + 1.0f)/2.0f;
	bool bIsOnRail = horse->GetPedResetFlag(CPED_RESET_FLAG_IsOnAssistedMovementRoute);
	if (bIsOnRail || ShouldUseTighterTurns())
	{		
		static dev_float headingChangeRate = 3.0f;
		horse->GetMotionData()->SetExtraHeadingChangeThisFrame(CalcDesiredDirection() * headingChangeRate * fwTimer::GetTimeStep());		
	} else if (horseComponent && horseComponent->GetAvoidanceCtrl().GetAvoiding()) {		
		//apply extra Yaw
		static dev_float headingChangeRate = PI/3.0f;
		horse->GetMotionData()->SetExtraHeadingChangeThisFrame(horseComponent->GetAvoidanceCtrl().GetHeadingScalar() * Sign(headingDelta) * Min(Abs(headingDelta), headingChangeRate * fwTimer::GetTimeStep()));
	} else
		horse->GetMotionData()->SetExtraHeadingChangeThisFrame(0);

	float currentTurnApproachRate = GallopTurnApproachRate + ((1.0f-(m_CurrentSpeed -0.25f)/0.75f) * (WalkTurnApproachRate-GallopTurnApproachRate));	

	TUNE_GROUP_FLOAT (HORSE_MOVEMENT, ms_fTurnHeadingMaxLerpRate, 2.00f,0.0f,20.0f,0.001f);
	TUNE_GROUP_FLOAT (HORSE_MOVEMENT, ms_fTurnHeadingMinLerpRate, 1.00f,0.0f,20.0f,0.001f);	

	float currentMBR = GetMotionData().GetCurrentMbrY()/3.0f;	
	if (m_fCurrentGait>1.0f) 
	{
		Approach(m_TurnPhase, m_TurnPhaseGoal, (currentMBR<0.0f) ? ReverseTurnApproachRate : currentTurnApproachRate, delta);		
		m_fTurnHeadingLerpRate = ms_fTurnHeadingMinLerpRate;
	}
	else 
	{
		if (fabs(m_TurnPhaseGoal-prevTurnPhaseGoal) > 0.3f)
		{
			if (m_TurnPhaseGoal < prevTurnPhaseGoal && m_TurnPhase > m_TurnPhaseGoal)
				m_fTurnHeadingLerpRate = ms_fTurnHeadingMinLerpRate; //flip
			else if (m_TurnPhaseGoal > prevTurnPhaseGoal && m_TurnPhase < m_TurnPhaseGoal)
				m_fTurnHeadingLerpRate = ms_fTurnHeadingMinLerpRate; //flip 
		}

		bool bRidingStraight = (m_TurnPhaseGoal < 0.6f && m_TurnPhaseGoal > 0.4f);
		static dev_float sf_RateRate = 3.0f;
		Approach(m_fTurnHeadingLerpRate, bRidingStraight ? ms_fTurnHeadingMinLerpRate : ms_fTurnHeadingMaxLerpRate, sf_RateRate,  delta);		
		//Displayf( "m_fTurnHeadingLerpRate:%f ", m_fTurnHeadingLerpRate);
		Approach(m_TurnPhase, m_TurnPhaseGoal, m_fTurnHeadingLerpRate, delta);	
	}
		
	
	
	m_networkHelper.SetFloat( ms_MoveTurnPhaseId, m_TurnPhase);

	if (!m_networkHelper.IsInTargetState())
	{
#if HACK_RDR3
		m_networkHelper.SendRequest( ms_startLocomotionId);
#endif
		return FSM_Continue;
	}

	//speed so riders can use	
	m_CurrentSpeed = m_fDesiredSpeed < 0.0f ? m_fDesiredSpeed : Max(0.25f, m_fDesiredSpeed); //not standing still unless in State_Idle, and rider needs a non zero


	//Update LeftToRightGaitBlendPhase
	bool isJumping = horse->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || horse->GetPedResetFlag(CPED_RESET_FLAG_IsLanding);
	static dev_float LRTransitionBuffer = 0.1f; 
	static dev_float LRApproachRate = 5.0f;
	if (m_LeftToRightGaitBlendPhase > 0.0f && m_LeftToRightGaitBlendPhase < 1.0f)
	{		
		Approach(m_LeftToRightGaitBlendPhase, (m_TurnPhase < 0.5f || isJumping) ? 1.0f : 0.0f, LRApproachRate, delta); //force right for jumping
	} 
	else if (m_LeftToRightGaitBlendPhase==0.0f && (m_TurnPhase<=(0.5f-LRTransitionBuffer) || isJumping))
		m_LeftToRightGaitBlendPhase = 0.01f; //transition right!
	else if (m_LeftToRightGaitBlendPhase==1.0f && !isJumping && m_TurnPhase>=(0.5f+LRTransitionBuffer))
		m_LeftToRightGaitBlendPhase = 0.99f; //transition left!

	//Displayf("Speed:%f Delta/Turn: %f/%f, LtoR: %f", m_CurrentSpeed, headingDelta, m_TurnPhase, m_LeftToRightGaitBlendPhase);

	m_networkHelper.SetFloat(ms_MoveLeftToRightControlId, m_LeftToRightGaitBlendPhase);
	
	if ( GetMotionData().GetCurrentMbrY() > MOVEBLENDRATIO_WALK )
	{
		m_fSustainedRunTime += fwTimer::GetTimeStep();
	} else
		m_fSustainedRunTime = 0;

	static const float MIN_SUSTAINED_RUN_TIME = 1.5f;
	if (m_bQuickStopRequested || (horseComponent && horseComponent->GetBrakeCtrl().GetHardBrake()))
	{
		m_bQuickStopRequested = false;
		m_bCancelQuickStopRequested = false;
		if (m_fSustainedRunTime > MIN_SUSTAINED_RUN_TIME)
		{
			SetState(State_QuickStop); 
			return FSM_Continue;
		}
	}

	if (m_bRearUpRequested)
	{
		SetState(State_RearUp);
		return FSM_Continue;
	}

	//Switch to turn in place?
	static dev_float sf_MaxTurnInPlaceMBR = .15f;
	bool bWantsToStartMoving = GetMotionData().GetGaitReducedDesiredMbrY() >= 0.75f;
	if (!bWantsToStartMoving && currentMBR <= sf_MaxTurnInPlaceMBR)
	{
		float headingDelta = CalcDesiredDirection();
		TUNE_GROUP_FLOAT (HORSE_MOVEMENT, fMinWalkToIdleRadius, 100.0f, 0.0f, 180.0f, 0.1f);
		if (fabs(headingDelta) >= (fMinWalkToIdleRadius*DtoR))
		{
			SetState(State_TurnInPlace);
		}
	}
	
	if (horseComponent)
	{
		//If horse is sliding show agitation:
		static dev_float sf_SlidePanicTrigger = 1.5f;
		if (horse->GetSlideSpeed() >= sf_SlidePanicTrigger)
		{
			m_networkHelper.SendRequest(ms_OverSpurId);
		}


		//Riders over working their horses cause irritation:
		static dev_float OVERSPUR_AGITATION_LIMIT = 0.75f;
		float fCurrAgitation = horseComponent->GetAgitationCtrl().GetCurrAgitation();
		if (m_fRumbleDelay > 0.0f )
		{
			m_fRumbleDelay-=TIME.GetSeconds();
		}

		if ((horseComponent->GetSpurredThisFrame() && fCurrAgitation > OVERSPUR_AGITATION_LIMIT) || m_bOverSpurRequested)
		{		
			m_networkHelper.SendRequest(ms_OverSpurId);

			static dev_u32 su_Duration = 500;
			static dev_float sf_MinIntensity = 0.5f;
			static dev_float sf_MaxIntensity = 1.0f;
			static dev_float sf_Delay = 1.5f;

			if (horseComponent)
			{
				CPed* rider = horseComponent->GetRider();
				if (rider && rider->IsLocalPlayer() && m_fRumbleDelay <= 0.0f)
				{
					float t = (fCurrAgitation - OVERSPUR_AGITATION_LIMIT)/(1.0f - OVERSPUR_AGITATION_LIMIT);
					CControlMgr::StartPlayerPadShakeByIntensity(su_Duration, Lerp(t, sf_MinIntensity, sf_MaxIntensity));
					m_fRumbleDelay = sf_Delay;
				}
			}			

			// Reset overspur if this was due to an external request.
			if (m_bOverSpurRequested)
			{
				m_bOverSpurRequested = false;
			}

			// TODO (horse agitation): seems like there is no need for second set of bind pose verts on the reins at this time 
			// Keep this here for reference please
			// -Svetli
			// 		CHorseComponent* horseComponent = horse->GetHorseComponent();
			// 		if( horseComponent )
			// 		{
			// 			CReins* reins = horseComponent->GetReins();
			// 			if( reins )
			// 			{
			// 				reins->SetNumBindFrames( 20 );
			// 			}
			// 		}

		}
	}
	
	
	if( CTaskSlopeScramble::CanDoSlopeScramble(horse)  && false) // DISABLED UNTIL ASSETS READY per Phil
	{
		SetState(State_Slope_Scramble);
	}

// OLD!
// 	if (GetMotionData().GetGaitReducedDesiredMbrY() == 0.0f)
// 	{	
// 		SetState(State_Stop);
// 	}

	if (ShouldStopMoving())
	{
		if (HasStops())// && m_fAnimPhase > GetTunables().m_StartToIdleDirectlyPhaseThreshold)TODO
		{
			SetState(State_Stop);
		}
		else
		{
			SetState(State_Idle);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateQuickStop_OnEnter()
{
	m_networkHelper.SendRequest( ms_QuickStopId );	
	m_networkHelper.WaitForTargetState(ms_OnEnterQuickStopId);
	m_networkHelper.SetFloat(ms_MoveQuickStopDirectionId, (float)m_eQuickStopDirection);

	CPed* pDriver = GetPed()->GetSeatManager()->GetDriver();
	if (pDriver )
	{
		CTaskMotionBase* pTask = pDriver->GetCurrentMotionTask(false);
		if (pTask->GetTaskType() == CTaskTypes::TASK_MOTION_RIDE_HORSE) {
			static_cast<CTaskMotionRideHorse*>(pTask)->RequestQuickStop();
		}
	}
	//save off current velocity
	m_fQuickStopMaxSpeed = Mag(VECTOR3_TO_VEC3V(GetPed()->GetVelocity())).Getf();
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskHorseLocomotion::StateQuickStop_OnUpdate()
{	
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;
	
	CHorseComponent* pHorseComponent = GetPed()->GetHorseComponent();
	if (pHorseComponent)
		pHorseComponent->ApplyAgitationScale(0.0f);

	if (m_bCancelQuickStopRequested)
	{
		m_bCancelQuickStopRequested = false;
		m_bQuickStopRequested = false;
		m_networkHelper.SendRequest( ms_CancelQuickStopId );	
		SetState(State_Locomotion);
	} 
	else if ( m_networkHelper.GetBoolean(ms_QuickStopClipEndedId)) 
	{		
		SetState(State_Idle);
	}
	else
	{
		float fAnimPhase = m_networkHelper.GetFloat(ms_AnimPhaseId);
		if (fAnimPhase > 0.9f && pHorseComponent) 
		{
			GetPed()->GetHorseComponent()->SetForceHardBraking();
		}
		if (fAnimPhase > 0.95f || fAnimPhase < 0) 
		{
			m_networkHelper.SetBoolean(ms_QuickStopClipEndedId, true);				
			SetState(State_Idle);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateRearUp_OnEnter()
{
	// Reset the request now that we are in the rear up state.
	m_bRearUpRequested = false;	

	m_networkHelper.SendRequest( ms_RearUpId );	
	m_networkHelper.WaitForTargetState(ms_OnEnterRearUpId);

	CPed* pDriver = GetPed()->GetSeatManager()->GetDriver();
	if (pDriver) 
	{
		pDriver->GetPedIntelligence()->ClearTasks();
		pDriver->GetPedIntelligence()->AddTaskDefault(rage_new CTaskRiderRearUp(m_bRearUpBuckRiderRequested));

		// Add a shocking event for ambient peds to react against.
		//CEventShockingHorseReared ev(*pDriver);
		//CShockingEventsManager::Add(ev);
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskHorseLocomotion::StateRearUp_OnUpdate()
{
	taskAssertf(GetPed()->GetHorseComponent()!=NULL, "Must have a horse component to rear up!");
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	CPed* pPed = GetPed();
	pPed->GetHorseComponent()->SetForceHardBraking();
	pPed->GetHorseComponent()->ApplyAgitationScale(0.0f);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsJumping, true);

	Matrix34 mat;
	pPed->GetBoneMatrix(mat, BONETAG_ROOT);				
	float fPitch = mat.c.Angle(MAT34V_TO_MATRIX34( pPed->GetMatrix()).c /*ZAXIS*/); 								
	pPed->SetDesiredBoundPitch(fabs(fPitch));

	float fAnimPhase = m_networkHelper.GetFloat(ms_AnimPhaseId);
	bool bInterrupted = false;
	static dev_float sfRearUpSlopeInterruptPhase = 0.6f;
	if ((m_SlopePhase <= 0.4f || m_SlopePhase>= 0.6f) && fAnimPhase >= sfRearUpSlopeInterruptPhase)
	{
		m_networkHelper.SendRequest(ms_InterruptAnimId);
		bInterrupted = true;
	}

	if ( m_networkHelper.GetBoolean(ms_RearUpClipEndedId) || bInterrupted) 
	{		
		if (m_bRearUpBuckRiderRequested)
		{
			m_bRearUpBuckRiderRequested=false;	
			if (pPed->GetHorseComponent()->GetRider())
				pPed->GetHorseComponent()->GetRider()->SetPedOffMount(); //for now...
		}
		pPed->SetDesiredBoundPitch(0.0f);	
		GetPed()->GetHorseComponent()->Reset(); // Reset hard braking when done
		SetState(State_Idle);
	}
	pPed->SetBoundPitch(pPed->GetDesiredBoundPitch());

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateIdle_OnEnter()
{
	GetMotionData().SetExtraHeadingChangeThisFrame(0.0f);
	CHorseComponent* pHorseComponent = GetPed()->GetHorseComponent();
	if (pHorseComponent)
	{
		pHorseComponent->SetCurrentSpeed(0); //reset speed to avoid speed burst after walk start.
		pHorseComponent->GetSpeedCtrl().SetTgtSpeed(0);
	}	
	m_networkHelper.SendRequest( ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);
	m_CurrentSpeed = 0;
	UpdateSlopePhase();

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateIdle_OnUpdate()
{
	TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fAIMinTurnDelta, 25.0f, 0.0f, 90.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fAIIdleHeadingSlideRate, 2.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fAIMaxTimeToIdleSlide, 1.0f, 0.0f, 10.0f, 0.01f);

	bool bCarryingPlayer = GetIsCarryingPlayer();
	float headingDelta = CalcDesiredDirection();

	if (!bCarryingPlayer)
	{
		// Slide towards small angles if the AI horse hasn't been in the state for long.
		// This is done to prevent going back into the turn in place state if the desired heading is only off by a small bit.
		if ((GetTimeInState() < fAIMaxTimeToIdleSlide && GetPreviousState() == State_TurnInPlace)
			&& fabs(headingDelta) < fAIMinTurnDelta * DtoR)
		{
			float fExtraHeadingChange = Clamp(GetTimeStep() * Sign(headingDelta) * fAIIdleHeadingSlideRate, fabs(headingDelta) * -1.0f, fabs(headingDelta));
			GetMotionData().SetExtraHeadingChangeThisFrame(fExtraHeadingChange);
		}
	}

	if (!m_networkHelper.IsInTargetState())
	{
		m_networkHelper.SendRequest( ms_startIdleId );
		return FSM_Continue;
	}
	else if (!m_bIdlesRunning)
	{
		AddIdleNetwork(GetPed());
	}

	if (abs(GetMotionData().GetGaitReducedDesiredMbrY()) > 0.0)
	{			
		TUNE_GROUP_FLOAT (HORSE_MOVEMENT, fMinIdleToWalkRadius, 45.0f, 0.0f, 180.0f, 0.1f);
		if (fabs(headingDelta) >= (fMinIdleToWalkRadius*DtoR))
		{
			SetState(State_TurnInPlace);
		}
		else 
			SetState(State_Locomotion);
	} else { //can turn in place at 0 speed (at a cliff)

		if (bCarryingPlayer)
		{
			// Player driven horses only try to turn in place if the heading delta is large. 
			if (fabs(headingDelta) >= QUARTER_PI)
			{
				SetState(State_TurnInPlace);
			}
		}
		else
		{
			// AI horses much be able to achieve up to the tolerance defined in CTaskMoveAchieveHeading.
			// Slide to make up the heading delta if it is small and you haven't been in this state for very long
			// (presumably because you just transitioned from turn in place)

			if (fabs(headingDelta) > CTaskMoveAchieveHeading::ms_fHeadingTolerance && GetTimeInState() > fAIMaxTimeToIdleSlide)
			{
				SetState(State_TurnInPlace);
			}
		}
	}

	if (m_bRearUpRequested)
	{
		SetState(State_RearUp); 		
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateTurnInPlace_OnEnter()
{
	m_fCurrentGait = 1.0f;
	m_networkHelper.SendRequest( ms_TurninPlaceId);
	m_networkHelper.WaitForTargetState(ms_OnEnterTurnInPlaceId);

	m_fInitialDirection = CalcDesiredDirection();
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskHorseLocomotion::StateTurnInPlace_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
	{
		m_networkHelper.SendRequest( ms_TurninPlaceId);
		return FSM_Continue;
	}
	float headingDelta = CalcDesiredDirection();
	if (headingDelta<-PI) 
		headingDelta = -PI;
	else if (headingDelta > PI)
		headingDelta = PI;

	static dev_float TurnApproachRate = 3.0f;
	m_TurnPhaseGoal = (headingDelta/PI + 1.0f)/2.0f;	
	float delta = GetTimeStep();
	Approach(m_TurnPhase, m_TurnPhaseGoal, TurnApproachRate, delta);
	m_networkHelper.SetFloat( ms_MoveTurnPhaseId, m_TurnPhase);

	if (GetPed()->GetHorseComponent())
		GetPed()->GetHorseComponent()->ApplyAgitationScale(0.0f);

	TUNE_GROUP_FLOAT (HORSE_MOVEMENT, fMinIdleToWalkRadius, 45.0f, 0.0f, 180.0f, 0.1f);

	bool bCarryingPlayer = GetIsCarryingPlayer();
	bool bWantsToStartMoving = GetMotionData().GetGaitReducedDesiredMbrY() >= 0.75f;

	// AI horses must achieve up to the tolerance defined in CTaskMoveAchieveHeading.
	if (bCarryingPlayer)
	{
		if (fabs(headingDelta)<( fMinIdleToWalkRadius*DtoR ) || bWantsToStartMoving)
		{
			if (GetMotionData().GetGaitReducedDesiredMbrY() == 0.0f)
			{
				SetState(State_Idle);
				m_networkHelper.ForceNextStateReady();		
			} else
				SetState(State_Locomotion);
		}
	}
	else
	{
		TUNE_GROUP_FLOAT(HORSE_MOVEMENT, fAIStopIdleTurnDegrees, 5.0f, 0.0f, 90.0f, 0.1f);
		if (bWantsToStartMoving)
		{
			SetState(State_Locomotion);
		}
		else if (fabs(headingDelta) < fAIStopIdleTurnDegrees * DtoR || Sign(m_fInitialDirection) != Sign(headingDelta))
		{
			// Close enough to the right heading to blend out and slide the rest.
			// Or we overshot the target.
			SetState(State_Idle);
			m_networkHelper.ForceNextStateReady();
		}
	}

	if (m_bRearUpRequested)
	{
		SetState(State_RearUp);
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////
void CTaskHorseLocomotion::StateSlopeScramble_OnEnter()
{
	GetPed()->GetPedIntelligence()->AddTaskDefault(rage_new CTaskSlopeScramble());
}

CTask::FSM_Return CTaskHorseLocomotion::StateSlopeScramble_OnUpdate()
{
	if ( GetPed()->GetPedIntelligence()->GetTaskActive() && GetPed()->GetPedIntelligence()->GetTaskActive()->GetTaskType()!=CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE ) {
		SetState(State_Idle);
	}	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskHorseLocomotion::StateStop_OnEnter()
{
	GetMotionData().SetExtraHeadingChangeThisFrame(0.0f);
	CHorseComponent* pHorseComponent = GetPed()->GetHorseComponent();
	if (pHorseComponent)
	{
		pHorseComponent->SetCurrentSpeed(0); //reset speed to avoid speed burst after walk start.
		pHorseComponent->GetSpeedCtrl().SetTgtSpeed(0);
	}
	
	m_networkHelper.SendRequest( ms_startStopId);
	m_networkHelper.WaitForTargetState(ms_OnEnterStopId);
	m_CurrentSpeed = 0.25f; //keep at walk pace, so we can play brake anims.
	UpdateSlopePhase();
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskHorseLocomotion::StateStop_OnUpdate()
{
	float headingDelta = CalcDesiredDirection();

	if (abs(GetMotionData().GetGaitReducedDesiredMbrY()) > 0.0)
	{                 
		TUNE_GROUP_FLOAT (HORSE_MOVEMENT, fMinIdleToWalkRadius, 45.0f, 0.0f, 180.0f, 0.1f);
		if (fabs(headingDelta) >= (fMinIdleToWalkRadius*DtoR))
		{
			SetState(State_TurnInPlace);
		}
		else 
			SetState(State_Locomotion);
	} 

	if (!m_networkHelper.IsInTargetState())
	{
		m_networkHelper.SendRequest( ms_startStopId );
		return FSM_Continue;
	}

	if (m_networkHelper.GetBoolean(ms_QuickStopClipEndedId))
		SetState(State_Idle);         

	if (m_bRearUpRequested)
	{
		SetState(State_RearUp);             
	}

	return FSM_Continue;
}

void CTaskHorseLocomotion::StateDead_OnEnter()
{
	m_networkHelper.SendRequest(ms_StartNoClipId);
	m_networkHelper.WaitForTargetState(ms_OnEnterNoClipId);

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

// Stay dead unless it stops being forced.
CTask::FSM_Return CTaskHorseLocomotion::StateDead_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead)
	{
		SetState(State_Initial);
	}

	return FSM_Continue;
}

void CTaskHorseLocomotion::StateDead_OnProcessMoveSignals()
{
	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void CTaskHorseLocomotion::ProcessGaits()
{
	float currentMBR = GetMotionData().GetCurrentMbrY()/3.0f;
	float desiredMBR = GetMotionData().GetDesiredMbrY()/3.0f;

	// bool bIsOnRail = horse->GetPedResetFlag(CPED_RESET_FLAG_IsOnAssistedMovementRoute);
	// removing this limiter for RDR, may need something tunable later.  B* 1697587
	// 	if (bIsOnRail) {
	// 		desiredMBR = Min(0.75f, desiredMBR);  // no galloping while on a rail
	// 		currentMBR = Min(0.75f, desiredMBR);  
	// 	}

	float delta = fwTimer::GetTimeStep();
	float gaitThisFrame = currentMBR*4.0f;
	if (m_fTransitionPhase>=1.0f)
	{
		m_fNextGait = desiredMBR < 0 ? -1.0f : (float)rage::round(gaitThisFrame);
		if (m_fNextGait!=m_fCurrentGait)
		{
			m_fTransitionPhase=0;

			CHorseComponent* pHorseComponent = GetPed()->GetHorseComponent();
			if( pHorseComponent )
			{
				CReins* reins = pHorseComponent->GetReins();
				if( reins )
				{
					const int expectedAttachState = m_fNextGait > 1.0f ? 2: 1;
					reins->SetExpectedAttachState(expectedAttachState);

					CPed* rider = pHorseComponent->GetRider();
					if( rider && !reins->GetIsRiderAiming() )
					{						
						Assert( rider->GetPedIntelligence() );
						Assert( rider->GetPedIntelligence()->GetQueriableInterface() );

						const bool isPedHoldingPhone =	rider->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOBILE_PHONE)
							||	rider->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOBILE_PHONE_CHAT_SCENARIO);
						if( !isPedHoldingPhone && expectedAttachState == 2 )
							reins->AttachTwoHands( rider );
						else
							reins->AttachOneHand( rider );
					}					
				}
			}
		}
	}


	m_fDesiredSpeed = m_fCurrentGait;
	if (m_fTransitionPhase<1.0f) {
		m_fDesiredSpeed += (m_fNextGait-m_fCurrentGait)*m_fTransitionPhase;

		//update Transition phase
		// Decide how long the transition is going to take.
		float fTimeBetweenGaits = rage::Max(VERY_SMALL_FLOAT, GetTimeBetweenGaits(m_fCurrentGait, m_fNextGait));

		if (fTimeBetweenGaits<=0.0f)
		{
			m_fTransitionPhase=1.0f;
		}
		else
		{
			m_fTransitionPhase = Min (1.0f, m_fTransitionPhase+delta*(1.0f/fTimeBetweenGaits)); //TODO possibly scale off clip length like RDR
		}		

		if (m_fTransitionPhase>=1.0f)
			m_fCurrentGait=m_fNextGait;		
	} 
}

//////////////////////////////////////////////////////////////////////////

void CTaskHorseLocomotion::SendLocomotionMoveSignals(CPed* pPed)
{
	bool bReloadIntros = false;
	m_networkHelper.GetNetworkPlayer()->GetBoolean(fwMvBooleanId("ReloadIntros", 0x28808634), bReloadIntros);
	if (bReloadIntros)
		SetIntroClipandBlend();

	float currentMBR = GetMotionData().GetCurrentMbrY()/3.0f;
	float gaitThisFrame = Min(currentMBR*4.0f, 4.0f);

	// Send the blended DesiredSpeed to MoVE (expects it as a float from 0-1).
	m_fDesiredSpeed *= 0.25f;
	m_networkHelper.SetFloat( ms_MoveDesiredSpeedId, m_fDesiredSpeed);

	float speedFactor = 1.0f;
	float gaitSpeedScale = 1.0f;
	float speedBoostFactor = 1.0f;
	float fMinGaitRateClamp = 0.8f;
	float fMaxGaitRateClamp = 1.2f;
	CHorseComponent* horseComponent = pPed->GetHorseComponent();
	if (horseComponent)
	{
		if (pPed->IsNetworkClone())
		{
			speedFactor = horseComponent->GetReplicatedSpeedFactor();
			gaitSpeedScale = horseComponent->GetSpeedCtrl().GetGaitSpeedScale(); //TODO		
		}
		else
		{
			speedFactor = horseComponent->GetSpeedCtrl().GetSpeedFactor();
			gaitSpeedScale = horseComponent->GetSpeedCtrl().GetGaitSpeedScale();			
		}
		speedBoostFactor = horseComponent->GetSpeedCtrl().GetSpeedBoostFactor();
		horseComponent->GetSpeedCtrl().GetGaitRateClampRange(fMinGaitRateClamp, fMaxGaitRateClamp);
	}

	// Respect the script override.
	float fScriptMoveOverride = GetPed()->GetMotionData()->GetCurrentMoveRateOverride();
	float fWalkRate = fScriptMoveOverride;
	float fTrotRate = fScriptMoveOverride;
	float fCanterRate = fScriptMoveOverride;
	float fGallopRate = fScriptMoveOverride;

	if (fScriptMoveOverride==1.0f) //Maybe script wants to override with a 1.0. this needs re-examined...
	{
		fWalkRate = (1.0f + gaitThisFrame-1.0f) * speedFactor;
		fTrotRate = (1.0f + gaitThisFrame-2.0f) * speedFactor;
		fCanterRate = (1.0f + gaitThisFrame-3.0f) * speedFactor;
		fGallopRate = (1.0f + gaitThisFrame-4.0f) * speedFactor;
	}

	// Check if the quad is chasing down a target and needs to get there fast. 
	if (GetIsUsingPursuitMode() && m_fCurrentGait == MOVEBLENDRATIO_SPRINT)
	{
		fGallopRate *= GetTunables().m_PursuitModeGallopRateFactor;
	}

	m_networkHelper.SetFloat(ms_MoveWalkRateId, Clamp(fWalkRate, fMinGaitRateClamp, fMaxGaitRateClamp) * gaitSpeedScale * speedBoostFactor );
	m_networkHelper.SetFloat(ms_MoveTrotRateId, Clamp(fTrotRate, fMinGaitRateClamp, fMaxGaitRateClamp) * gaitSpeedScale * speedBoostFactor );
	m_networkHelper.SetFloat(ms_MoveCanterRateId, Clamp(fCanterRate, fMinGaitRateClamp, fMaxGaitRateClamp) * gaitSpeedScale * speedBoostFactor );
	m_networkHelper.SetFloat(ms_MoveGallopRateId, Max(fMinGaitRateClamp, fGallopRate) * gaitSpeedScale * speedBoostFactor );
}

//////////////////////////////////////////////////////////////////////////
// Can only be timesliced if in low lod motion.
bool CTaskHorseLocomotion::CanBeTimesliced() const
{
	// Check if an AI update needs to be forced.
	if (CPedAILodManager::IsTimeslicingEnabled())
	{
		return GetPed()->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask);
	}

	return false;
}

// Refuse to be timesliced if the heading delta is too large.
bool CTaskHorseLocomotion::ShouldForceNoTimeslicing() const
{
	// Check if an AI update needs to be forced.
	if (CPedAILodManager::IsTimeslicingEnabled())
	{
		float fTarget = CalcDesiredDirection();

		// Check how far away the quad is from facing their target.
		if (fabs(fTarget) > GetTunables().m_DisableTimeslicingHeadingThresholdD * DtoR)
		{
			return true;
		}

	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

float	CTaskHorseLocomotion::GetTimeBetweenGaits(float fCurrentGait, float fNextGait) const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (fCurrentGait == MOVEBLENDRATIO_WALK && fNextGait == MOVEBLENDRATIO_RUN)
		{
			return pMotionStruct->m_QuadWalkRunTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_RUN && fNextGait == MOVEBLENDRATIO_WALK)
		{
			return pMotionStruct->m_QuadRunWalkTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_RUN && fNextGait == MOVEBLENDRATIO_SPRINT)
		{
			return pMotionStruct->m_QuadRunSprintTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_SPRINT && fNextGait == MOVEBLENDRATIO_RUN)
		{
			return pMotionStruct->m_QuadSprintRunTransitionTime;
		}
	}
	return 0.25f;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskHorseLocomotion::HasStops() 
{	
	fwClipSet *pClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (pClipSet)
	{		
		//horse style
		if (pClipSet->GetClip(fwMvClipId("walk_stop_r",0x2a56de3d)) != NULL)
		{			
			return true;
		} 
	}

	return HasQuickStops();
}

bool CTaskHorseLocomotion::HasQuickStops() 
{	
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (pMotionStruct->m_HasQuickStops)
		{
			m_networkHelper.SetFlag(true, fwMvFlagId("UseQuickStops", 0xf622aea6));
			return true;
		} 		
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CTaskHorseLocomotion::ShouldScaleTurnVelocity(float fDesiredHeadingChange, float fAnimationRotateSpeed) const
{
	// Only scale while moving.
	if (GetState() != State_Locomotion)
	{
		return false;
	}

	// Only scale in the direction the ped needs to turn.
	return Sign(fDesiredHeadingChange) == Sign(fAnimationRotateSpeed);
}

float CTaskHorseLocomotion::GetTurnVelocityScaleFactor() const
{
	float fMBRY = GetMotionData().GetCurrentMbrY();
	float fWalkScale = 1.0f;
	float fRunScale = 1.0f;
	float fSprintScale = 1.0f;

	const CPed* pPed = GetPed();

	// Acquire the scaling factor from the motion set.
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (Verifyf(pMotionStruct, "Invalid on foot motion task data set!"))
		{
			fWalkScale = pMotionStruct->m_QuadWalkTurnScaleFactor;
			fRunScale = pMotionStruct->m_QuadRunTurnScaleFactor;
			fSprintScale = pMotionStruct->m_QuadSprintTurnScaleFactor;
		}
	}

	// Check which move blend ratio is in use and use that scaling factor.
	if (fMBRY <= MOVEBLENDRATIO_WALK)
	{
		return fWalkScale;
	}
	else if (fMBRY <= MOVEBLENDRATIO_RUN)
	{
		return fRunScale;
	}
	else
	{
		return fSprintScale;
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskHorseLocomotion::SetIntroClipandBlend()
{
	float fBlend = 0;
	if (m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.SetFlag(false, ms_NoIntroFlagId);
		fwClipSet *pClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
		if (pClipSet)
		{

			const crClip* clip0 = NULL;
			const crClip* clip1 = NULL;
			
			// We have no active clips, estimate the distance from what we will play
			enum Foot { FOOT_LEFT, FOOT_RIGHT, NUM_FEET, };
			enum StartClips { WALK_90, WALK_180, TROT_90, TROT_180, CANTER_90, CANTER_180, GALLOP_90, GALLOP_180, NUM_START_CLIPS };
			static const fwMvClipId stopClips[NUM_FEET][NUM_START_CLIPS] =
			{
				{ fwMvClipId("walk_start_l_-90",0x1cbf7e2), fwMvClipId("walk_start_l_-180",0xd9b8e243), fwMvClipId("trot_start_l_-90",0xd90f519e), fwMvClipId("trot_start_l_-180",0x18e15954), fwMvClipId("canter_start_l_-90",0xbe6debd9), fwMvClipId("canter_start_l_-180",0xc17d4eda), fwMvClipId("gallop_start_l_-90",0xe78ccae4), fwMvClipId("gallop_start_l_-180",0x33b303e1) },
				{ fwMvClipId("walk_start_r_90",0x58042fab), fwMvClipId("walk_start_r_180",0x2123e746), fwMvClipId("trot_start_r_90",0xecea750e), fwMvClipId("trot_start_r_180",0x49377a5b), fwMvClipId("canter_start_r_90",0xb812f308), fwMvClipId("canter_start_r_180",0x2dd9107d), fwMvClipId("gallop_start_r_90",0x276b26ea), fwMvClipId("gallop_start_r_180",0xa79eeac0) }
			};

			const CMotionTaskDataSet* pSet = GetPed()->GetMotionTaskDataSet();					
			const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();

			int speed = 0; 
			bool downgrade = false;

			float desiredMBR = GetMotionData().GetDesiredMbrY()/3.0f;

			//gallop?
			if (desiredMBR >0.75f)
			{
				clip0 = pClipSet->GetClip(fwMvClipId("gallop_start_0",0x112b28db));
				downgrade = clip0 == NULL;
				speed = 3;
			}
			//canter?
			if (downgrade || (clip0==NULL && desiredMBR >0.5f))
			{
				if (pMotionStruct->m_CanCanter)
				{
					clip0 = pClipSet->GetClip(fwMvClipId("canter_start_0",0x1c3b4751));
					downgrade = clip0 == NULL;
					speed = 2;
				}
				else
				{
					downgrade =true;
				}
			}
			//trot?
			if (downgrade || (clip0==NULL && desiredMBR >0.25f))
			{
				if (pMotionStruct->m_CanTrot)
				{
					clip0 = pClipSet->GetClip(fwMvClipId("trot_start_0",0xc8ebe197));
					downgrade = clip0 == NULL;
					speed = 1;
				}
				else
				{
					downgrade =true;
				}
			}

			//walk?
			if (downgrade || clip0==NULL)
			{
				clip0 = pClipSet->GetClip(fwMvClipId("walk_start_0",0xf52a1c6d));
				if (clip0 != NULL) //some still use this instead
					pClipSet->GetClip(fwMvClipId("walk_start_0_L",0x21047335));
				speed = 0;
			}

			//Handle turns
			float headingDelta = CalcDesiredDirection();
			if (headingDelta<-PI) 
				headingDelta = -PI;
			else if (headingDelta > PI)
				headingDelta = PI;
			int left = Sign(headingDelta) > 0 ? 0 : 1;
			float directionT = fabs(headingDelta)/PI;
			if (directionT>=0)
			{
				clip1 =  pClipSet->GetClip(stopClips[left][(directionT > 0.5f) ? (speed*2 + 1) : (speed*2)]);
				if (clip1 != NULL)
				{
					if (directionT > 0.5f) 
						clip0 = pClipSet->GetClip(stopClips[left][speed*2]);
				}
				else
				{
					if (directionT > 0.5f)
					{
						clip1 = pClipSet->GetClip(stopClips[left][speed*2]);
					}
				}

				if (clip1 != NULL)
				{
					fBlend = ((directionT > 0.5f) ? (directionT-0.5f)/0.5f : directionT/0.5f);
				}
			}

			if (clip0 != NULL)
			{
				m_networkHelper.SetClip(clip0, ms_IntroClip0Id);
			}
			else					
				m_networkHelper.SetFlag(true, ms_NoIntroFlagId);
											

			//if clip1 was never set just use 0 again
			if (clip1==NULL)
			{
				m_networkHelper.SetClip(clip0, ms_IntroClip1Id);				
			}
			else
			{
				m_networkHelper.SetClip(clip1, ms_IntroClip1Id);				
			}
		}
		
		m_networkHelper.SetFloat(ms_IntroBlendId, fBlend);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTaskHorseLocomotion::ShouldStopMoving()
{
 	if (m_bMovingForForcedMotionState) 
 	{
 		return false;
 	}

	if (GetMotionData().GetGaitReducedDesiredMbrY() == 0.0f)
	{
		const CTaskMoveGoToPointAndStandStill* pGototask = CTaskHumanLocomotion::GetGotoPointAndStandStillTask(GetPed());
		if (pGototask)
		{
			const float fStopDistance = GetTunables().m_StoppingGotoPointRemainingDist;
			const float fDistanceRemaining = pGototask->GetDistanceRemaining();
			if (fDistanceRemaining <= fStopDistance)
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

float CTaskHorseLocomotion::CalcDesiredDirection() const
{
	const CPed * pPed = GetPed();
	bool reversing = m_fCurrentGait<0.0f;
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe( reversing ? (pPed->GetCurrentHeading() + PI) : pPed->GetCurrentHeading());
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe( reversing ? (pPed->GetDesiredHeading() + PI) : pPed->GetDesiredHeading());

	float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
	if (reversing) fHeadingDelta=-fHeadingDelta;

	return fHeadingDelta;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskHorseLocomotion::GetHorseGaitPhaseAndSpeed(Vector4& phase, float& speed, float &turnPhase, float &slopePhase) 
{
	//Ensure the task has made it past the initialization state.
	//If not, the network helper is invalid.
	if(!m_networkHelper.GetNetworkPlayer() || (State_Initial == GetState()))
		return false;
	
	phase.x = m_networkHelper.GetFloat(ms_MoveWalkPhaseId);	
	phase.y = m_networkHelper.GetFloat(ms_MoveTrotPhaseId);	
	phase.z = m_networkHelper.GetFloat(ms_MoveCanterPhaseId);	
	phase.w = m_networkHelper.GetFloat(ms_MoveGallopPhaseId);

	speed = m_CurrentSpeed;
	turnPhase = m_TurnPhaseGoal;
	slopePhase = m_SlopePhase;
	
	return true;
}

bool CTaskHorseLocomotion::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	switch(state)
	{
	case CPedMotionStates::MotionState_Idle:
		return GetState() == State_Idle;
	case CPedMotionStates::MotionState_Walk:
		return (GetState() == State_Locomotion) && GetMotionData().GetIsWalking();
	case CPedMotionStates::MotionState_Run:
		return (GetState() == State_Locomotion) && GetMotionData().GetIsRunning();
	case CPedMotionStates::MotionState_Sprint:
		return (GetState() == State_Locomotion) && GetMotionData().GetIsSprinting();
	case CPedMotionStates::MotionState_Dead:
		return GetState() == State_Dead;
	case CPedMotionStates::MotionState_DoNothing:
		return GetState() == State_DoNothing;
	default:
		return false;
	}
}

void CTaskHorseLocomotion::AddIdleNetwork(CPed* pPed)
{
	if(!m_IdleNetworkHelper.IsNetworkActive())
	{
		fwMvNetworkDefId ms_NetworkHorseIdles("HorseIdles",0x9e050c05);
		m_IdleNetworkHelper.RequestNetworkDef(ms_NetworkHorseIdles);
		m_IdleNetworkHelper.CreateNetworkPlayer(pPed, ms_NetworkHorseIdles);
		m_IdleNetworkHelper.SetClipSet( m_clipSetId );
	}

	m_bIdlesRunning = true;
	static const fwMvNetworkId IdleNetworkId("IdleNetwork",0xedd708ef);	
	m_networkHelper.SetSubNetwork(m_IdleNetworkHelper.GetNetworkPlayer(), IdleNetworkId);
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskHorseLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Idle: return "Idle";
	case State_TurnInPlace: return "TurnInPlace";
	case State_Locomotion: return "Locomotion";
	case State_Stop: return "Stop";
	case State_QuickStop: return "QuickStop";
	case State_RearUp: return "RearUp";
	case State_Slope_Scramble: return "SlopeScramble";
	case State_Dead: return "Dead";
	case State_DoNothing: return "DoNothing";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskHorseLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskRiderRearUp
//////////////////////////////////////////////////////////////////////////

CTaskRiderRearUp::CTaskRiderRearUp(bool bBuckOff) 
: m_bBuckOff(bBuckOff)
{
	SetInternalTaskType(CTaskTypes::TASK_RIDER_REARUP);
}
	
CTask::FSM_Return CTaskRiderRearUp::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskRiderRearUp::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_RearUp)	
		FSM_OnUpdate
		return RearUp_OnUpdate();

	FSM_State(State_DetachedFromHorse)	
		FSM_OnEnter
			DetachedFromHorse_OnEnter();
		FSM_OnUpdate
			return DetachedFromHorse_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskRiderRearUp::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	return UpdateFSM(iState, iEvent); 
}

CTaskInfo* CTaskRiderRearUp::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTask::FSM_Return CTaskRiderRearUp::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();
	int iSeatIndex = pMount->GetSeatManager()->GetPedsSeatIndex(pPed); 
	const CVehicleSeatAnimInfo* pSeatClipInfo = pMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(iSeatIndex);	
	taskFatalAssertf(pSeatClipInfo, "NULL Clip Info for seat index %i", iSeatIndex);	
	taskAssertf(fwClipSetManager::GetClipSet(pSeatClipInfo->GetSeatClipSetId()), "Couldn't find clipset %s", pSeatClipInfo->GetSeatClipSetId().GetCStr());	

	static const fwMvClipId REARUP_CLIP("idle_rup",0x5C634127);
	static const fwMvClipId BUCKOFF_CLIP("buc",0xB0E6003B);	
	StartClipBySetAndClip(pPed,pSeatClipInfo->GetSeatClipSetId(), m_bBuckOff ? BUCKOFF_CLIP : REARUP_CLIP, NORMAL_BLEND_IN_DELTA);
	SetState(State_RearUp);

	if (pPed->IsLocalPlayer())
	{
		static dev_u32 RUMBLE_DURATION = 2000;
		static dev_float RUMBLE_INTENSITY = 1.0;
		CControlMgr::StartPlayerPadShakeByIntensity(RUMBLE_DURATION, RUMBLE_INTENSITY);
	}
	//m_pRiderTaskMountAnimal->StartMountAnimation();
	return FSM_Continue;
}

CTask::FSM_Return CTaskRiderRearUp::RearUp_OnUpdate()
{
	if (!GetClipHelper())//anim done		
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	const crClip* pClip = GetClipHelper()->GetClip();
	if (m_bBuckOff && pClip)
	{
		float fClipPhase = rage::Clamp(GetClipHelper()->GetPhase(), 0.0f, 1.0f);
		float fEventPhase = 1.0f;
		if (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase) && fClipPhase >= fEventPhase)
		{
			SetState(State_DetachedFromHorse);
		}
	}

	return FSM_Continue;
}

void CTaskRiderRearUp::DetachedFromHorse_OnEnter()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();

	// Switch the ped to ragdoll
	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT) && pPed && pMount)
	{
		// Disconnect the ped from the mount without clearing the horse's animation state
		pPed->SetPedOffMount(false);

		pPed->GetRagdollInst()->SetDontZeroMatricesOnActivation();

		// Run an appropriate NM behavior
		const u32 uMinTime = (u32)(sm_Tunables.m_ThrownRiderNMTaskTimeMin * 1000.f);
//		const u32 uMaxTime = (u32)(sm_Tunables.m_ThrownRiderNMTaskTimeMax * 1000.f);
		aiTask* pTaskNM = rage_new CTaskNMHighFall(uMinTime, NULL, CTaskNMHighFall::HIGHFALL_FROM_CAR_HIT);
		CEventSwitch2NM event(4000, pTaskNM);
		pPed->SwitchToRagdoll(event);
	}
}

CTask::FSM_Return CTaskRiderRearUp::DetachedFromHorse_OnUpdate()
{
	SetState(State_Finish);
	return FSM_Continue;
}

bool CTaskRiderRearUp::ProcessPostMovement()
{	
	if (m_bBuckOff && GetState() > State_Start)
	{
		CPed* pPed = GetPed();
		CPed* pMount = pPed->GetMyMount();
		if (pMount && GetState()<State_Finish && pPed->GetIsAttached())
		{
			// Here we calculate the initial situation, which in the case of exiting is the seat situation
			// Note we don't use the helper to do this calculation since we assume the ped starts exactly on the seat node
			Vector3 vNewPedPosition(Vector3::ZeroType);
			Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);
			const crClip* pClip = GetClipHelper()->GetClip();
			float fClipPhase = GetClipHelper()->GetPhase();
			fClipPhase = rage::Clamp(fClipPhase, 0.0f, 1.0f);

			CTaskDismountSeat::UpdateDismountAttachment(pPed, pMount, pClip, fClipPhase, vNewPedPosition, qNewPedOrientation, 0 /*hack*/, m_PlayAttachedClipHelper, false);					
			return true;
		}
	}	
	return false;
}

#if !__FINAL
const char * CTaskRiderRearUp::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_RearUp:							return "State_RearUp";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif //!__FINAL
//////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE