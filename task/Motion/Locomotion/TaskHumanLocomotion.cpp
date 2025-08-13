// File header
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"

// Rage headers
#include "fwdebug/picker.h"
#include "math/angmath.h"

// Game headers
#include "ai\debug\system\AIDebugLogManager.h"
#include "Animation/AnimManager.h"
#include "Animation/Move.h"
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Debug/DebugScene.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/Weather.h"
#include "ik/solvers/LegSolver.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskRepositionMove.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Info/ScenarioTypes.h"
#include "task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

namespace AIStats
{
	EXT_PF_TIMER(ProcessSlopesAndStairs);
}
using namespace AIStats;

dev_float MIN_ANGLE = 0.075f;

dev_float CTaskHumanLocomotion::BLEND_TO_IDLE_TIME = 0.5f;

// Instance of tunable task params
CTaskHumanLocomotion::Tunables CTaskHumanLocomotion::ms_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskHumanLocomotion, 0x9a6ea233);

const fwMvBooleanId CTaskHumanLocomotion::ms_AefFootHeelLId("AEF_FOOT_HEEL_L",0x1C2B5199);
const fwMvBooleanId CTaskHumanLocomotion::ms_AefFootHeelRId("AEF_FOOT_HEEL_R",0xD97C4C50);
// Networks
const fwMvNetworkId CTaskHumanLocomotion::ms_RepositionMoveNetworkId("RepositionMove",0x45D8361B);
const fwMvNetworkId CTaskHumanLocomotion::ms_FromStrafeNetworkId("FromStrafeNetwork",0x81404DD0);
// States
const fwMvStateId CTaskHumanLocomotion::ms_LocomotionStateId("Locomotion",0xB65CBA9D);
const fwMvStateId CTaskHumanLocomotion::ms_IdleStateId("Idle",0x71C21326);
const fwMvStateId CTaskHumanLocomotion::ms_MovingStateId("Moving",0xDF719C6C);
const fwMvStateId CTaskHumanLocomotion::ms_FromStrafeStateId("FromStrafe",0xDE712697);
// Variable clipsets
const fwMvClipSetVarId CTaskHumanLocomotion::ms_GenericMovementClipSetVarId("GenericMovement",0x7BBE09EE);
const fwMvClipSetVarId CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId("DefaultWeaponHolding",0xFA599A03);
const fwMvClipSetVarId CTaskHumanLocomotion::ms_SnowMovementClipSetVarId("SnowMovement",0xC97BE8B9);
const fwMvClipSetVarId CTaskHumanLocomotion::ms_FromStrafeTransitionUpperBodyClipSetVarId("FromStrafeTransitionUpperBody",0xD90143DA);
// Clips
const fwMvClipId CTaskHumanLocomotion::ms_WalkClipId("Walk",0x83504C9C);
const fwMvClipId CTaskHumanLocomotion::ms_RunClipId("Run",0x1109B569);
const fwMvClipId CTaskHumanLocomotion::ms_MoveStopClip_N_180Id("MoveStopClip_N_180",0x60891DCE);
const fwMvClipId CTaskHumanLocomotion::ms_MoveStopClip_N_90Id("MoveStopClip_N_90",0x581240BA);
const fwMvClipId CTaskHumanLocomotion::ms_MoveStopClipId("MoveStopClip",0xDA0897FD);
const fwMvClipId CTaskHumanLocomotion::ms_MoveStopClip_90Id("MoveStopClip_90",0xFC1B0D90);
const fwMvClipId CTaskHumanLocomotion::ms_MoveStopClip_180Id("MoveStopClip_180",0x19118FE0);
const fwMvClipId CTaskHumanLocomotion::ms_TransitionToIdleIntroId("TransitionToIdleIntro",0x64551DDE);
const fwMvClipId CTaskHumanLocomotion::ms_Turn180ClipId("Turn180Clip",0x6FEC2BA2);
const fwMvClipId CTaskHumanLocomotion::ms_CustomIdleClipId("CustomIdleClip",0x21F6E813);
const fwMvClipId CTaskHumanLocomotion::ms_ClipEmpty("IDLE_EMPTY",0x47510322);
#if FPS_MODE_SUPPORTED
const fwMvClipId CTaskHumanLocomotion::ms_RunHighClipId("run_high",0x12e28c0a);
#endif // FPS_MODE_SUPPORTED
// Filters
const fwMvFilterId CTaskHumanLocomotion::ms_WeaponHoldingFilterId("WeaponHolding_Filter",0x59273C50);
const fwMvFilterId CTaskHumanLocomotion::ms_MeleeGripFilterId("MeleeGrip_Filter", 0xe0a7d066);
// Floats
const fwMvFloatId CTaskHumanLocomotion::ms_MovementRateId("Movement_Rate",0x6D2F67D0);
const fwMvFloatId CTaskHumanLocomotion::ms_MovementTurnRateId("Movement_Turn_Rate",0x2D3004B6);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleTurnDirectionId("IdleTurnDirection",0xE14E88B0);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleTurnRateId("IdleTurn_Rate",0x6930AF4F);
const fwMvFloatId CTaskHumanLocomotion::ms_StartDirectionId("StartDirection",0xE4825C7C);
const fwMvFloatId CTaskHumanLocomotion::ms_DesiredDirectionId("DesiredDirection",0xAC040D40);
const fwMvFloatId CTaskHumanLocomotion::ms_DirectionId("Direction",0x34DF9828);
const fwMvFloatId CTaskHumanLocomotion::ms_StopDirectionId("StopDirection",0x7001EF25);
const fwMvFloatId CTaskHumanLocomotion::ms_SlopeDirectionId("SlopeDirection",0xFA0F8DA2);
const fwMvFloatId CTaskHumanLocomotion::ms_SnowDepthId("SnowDepth",0x8A92EDD1);
const fwMvFloatId CTaskHumanLocomotion::ms_DesiredSpeedId("DesiredSpeed",0xCF7BA842);
const fwMvFloatId CTaskHumanLocomotion::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopPhase_N_180Id("MoveStopPhase_N_180",0xA96A7D2B);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopPhase_N_90Id("MoveStopPhase_N_90",0x8973DB9A);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopPhaseId("MoveStopPhase",0x3C16BB51);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopPhase_90Id("MoveStopPhase_90",0x4297CA8);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopPhase_180Id("MoveStopPhase_180",0xA58DFC89);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopBlend_N_180Id("MoveStopBlend_N_180",0x285BA321);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopBlend_N_90Id("MoveStopBlend_N_90",0x178B35A8);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopBlendId("MoveStopBlend",0x6C340B39);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopBlend_90Id("MoveStopBlend_90",0x26E47D55);
const fwMvFloatId CTaskHumanLocomotion::ms_MoveStopBlend_180Id("MoveStopBlend_180",0x26A4D5B5);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleIntroStartPhaseId("IdleIntroStartPhase",0xD7DA98B9);
const fwMvFloatId CTaskHumanLocomotion::ms_BeginMoveStartPhaseId("BeginMoveStartPhase",0xE3005046);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleStartPhaseId("IdleStartPhase",0xD61168B2);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleRateId("IdleRate",0xDDE7C19C);
const fwMvFloatId CTaskHumanLocomotion::ms_Turn180PhaseId("Turn180Phase",0xD37D52F4);
const fwMvFloatId CTaskHumanLocomotion::ms_MovingInitialPhaseId("Moving_InitialPhase",0x2350EDA7);
const fwMvFloatId CTaskHumanLocomotion::ms_GenericMoveForwardWeightId("GenericMoveForwardWeight",0x8DCC2A1D);
const fwMvFloatId CTaskHumanLocomotion::ms_WeaponHoldingBlendDurationId("WeaponHoldingBlendDuration",0xCFA34AEA);
const fwMvFloatId CTaskHumanLocomotion::ms_FromStrafeTransitionUpperBodyWeightId("FromStrafeTransitionUpperBodyWeight",0x67C1C19A);
const fwMvFloatId CTaskHumanLocomotion::ms_FromStrafeTransitionUpperBodyDirectionId("FromStrafeTransitionUpperBodyDirection",0x744750A3);
const fwMvFloatId CTaskHumanLocomotion::ms_IdleTransitionBlendOutDurationId("IdleTransitionBlendOutDuration",0xf19d5657);
#if FPS_MODE_SUPPORTED
const fwMvFloatId CTaskHumanLocomotion::ms_PitchId("Pitch",0x3f4bb8cc);
#endif // FPS_MODE_SUPPORTED
// Booleans/Events
// State enters
const fwMvBooleanId CTaskHumanLocomotion::ms_IdleOnEnterId("State_Idle",0xDD0BC665);
const fwMvBooleanId CTaskHumanLocomotion::ms_IdleTurnOnEnterId("State_IdleTurn",0x61A9DE8D);
const fwMvBooleanId CTaskHumanLocomotion::ms_IdleStandardEnterId("Idle_Standard_Enter",0xF23A8571);
const fwMvBooleanId CTaskHumanLocomotion::ms_StartOnEnterId("State_Start",0x1EB8BCF9);
const fwMvBooleanId CTaskHumanLocomotion::ms_MovingOnEnterId("State_Moving",0xC2AF6911);
const fwMvBooleanId CTaskHumanLocomotion::ms_Turn180OnEnterId("State_Turn180",0x685AE5D7);
const fwMvBooleanId CTaskHumanLocomotion::ms_StopOnEnterId("State_Stop",0x5D6AE99A);
const fwMvBooleanId CTaskHumanLocomotion::ms_RepositionMoveOnEnterId("State_RepositionMove",0x551F650F);
const fwMvBooleanId CTaskHumanLocomotion::ms_FromStrafeOnEnterId("State_FromStrafe",0xAD517C3F);
const fwMvBooleanId CTaskHumanLocomotion::ms_CustomIdleOnEnterId("State_CustomIdle",0xB40FDA7C);
// State exits
const fwMvBooleanId CTaskHumanLocomotion::ms_StartOnExitId("State_Exit_Start",0x7E637248);
const fwMvBooleanId CTaskHumanLocomotion::ms_MovingOnExitId("State_Exit_Moving",0xD02749C6);
// Running flags
const fwMvBooleanId CTaskHumanLocomotion::ms_WalkingId("Walking",0x36EFDB93);
const fwMvBooleanId CTaskHumanLocomotion::ms_RunningId("Running",0x68AB84B5);
const fwMvBooleanId CTaskHumanLocomotion::ms_UseLeftFootStrafeTransitionId("USE_LEFT_FOOT_STRAFE_TRANSITION",0xC9AF24C2);
#if FPS_MODE_SUPPORTED
const fwMvBooleanId CTaskHumanLocomotion::ms_UsePitchedWeaponHoldingId("UsePitchedWeaponHolding",0xe1aacea5);
#endif // FPS_MODE_SUPPORTED
// Early outs
const fwMvBooleanId CTaskHumanLocomotion::ms_BlendOutIdleTurnId("BLEND_OUT_IDLE_TURN",0x2E312A0);
const fwMvBooleanId CTaskHumanLocomotion::ms_BlendOutStartId("BLEND_OUT_START",0x6A82789F);
const fwMvBooleanId CTaskHumanLocomotion::ms_BlendOut180Id("BLEND_OUT_180",0x5988CE8B);
const fwMvBooleanId CTaskHumanLocomotion::ms_BlendOutStopId("BLEND_OUT_STOP",0xFC86B701);
const fwMvBooleanId CTaskHumanLocomotion::ms_CanEarlyOutForMovementId("CAN_EARLY_OUT_FOR_MOVEMENT",0x7E1C8464);
const fwMvBooleanId CTaskHumanLocomotion::ms_CanResumeMovingId("CAN_RESUME_MOVING",0xEF310EB);
const fwMvBooleanId CTaskHumanLocomotion::ms_ActionModeBlendOutStartId("ACTION_MODE_BLEND_OUT_START",0xFFC2FA86);
const fwMvBooleanId CTaskHumanLocomotion::ms_ActionModeBlendOutStopId("ACTION_MODE_BLEND_OUT_STOP",0x238DFA58);
//
const fwMvBooleanId CTaskHumanLocomotion::ms_IdleTransitionFinishedId("Idle_Transition_Finished",0xF23FE554);
const fwMvBooleanId CTaskHumanLocomotion::ms_IdleTransitionBlendInWeaponHoldingId("IdleTransitionBlendInWeaponHolding",0x504F2951);
const fwMvBooleanId CTaskHumanLocomotion::ms_CustomIdleFinishedId("CustomIdleFinished",0xDEE7E59D);
//
const fwMvBooleanId CTaskHumanLocomotion::ms_StartLeftId("Start_Left",0x1072fbbc);
const fwMvBooleanId CTaskHumanLocomotion::ms_StartRightId("Start_Right",0xa95eb7b3);
//
const fwMvBooleanId CTaskHumanLocomotion::ms_AlternateFinishedId[ACT_MAX] = { fwMvBooleanId("Alternate_Idle_Finished",0xB1F84ACF), fwMvBooleanId("Alternate_Walk_Finished",0x42FFEE42), fwMvBooleanId("Alternate_Run_Finished",0x396B387C), fwMvBooleanId("Alternate_Sprint_Finished",0xCE30CAC9) };
// Requests
const fwMvRequestId CTaskHumanLocomotion::ms_RestartWeaponHoldingId("RestartWeaponHolding",0x16F3EF2E);
const fwMvRequestId CTaskHumanLocomotion::ms_StoppedId("Stopped",0xE18018B8);
const fwMvRequestId CTaskHumanLocomotion::ms_IdleTurnId("IdleTurn",0xFA5D76E5);
const fwMvRequestId CTaskHumanLocomotion::ms_StartMovingId("Start_Moving",0x56A46BD3);
const fwMvRequestId CTaskHumanLocomotion::ms_MovingId("Moving",0xDF719C6C);
const fwMvRequestId CTaskHumanLocomotion::ms_Turn180Id("Turn180",0x4FDCF090);
const fwMvRequestId CTaskHumanLocomotion::ms_StopMovingId("Stop_Moving",0x969B7C1E);
const fwMvRequestId CTaskHumanLocomotion::ms_RepositionMoveId("RepositionMove",0x45D8361B);
const fwMvRequestId CTaskHumanLocomotion::ms_CustomIdleId("CustomIdle",0x33E86E47);
// Flags
const fwMvFlagId CTaskHumanLocomotion::ms_OnStairsId("OnStairs",0x7D80D01C);
const fwMvFlagId CTaskHumanLocomotion::ms_OnSteepStairsId("OnSteepStairs",0x4AE24C07);
const fwMvFlagId CTaskHumanLocomotion::ms_OnSlopeId("OnSlope",0xF5557EC4);
const fwMvFlagId CTaskHumanLocomotion::ms_OnSnowId("OnSnow",0x30DD7767);
const fwMvFlagId CTaskHumanLocomotion::ms_ForceSuddenStopId("ForceSuddenStop",0x931EBB02);
const fwMvFlagId CTaskHumanLocomotion::ms_SkipWalkRunTransitionId("SkipWalkRunTransition",0x66C1FD51);
const fwMvFlagId CTaskHumanLocomotion::ms_UseWeaponHoldingId("UseWeaponHolding",0xA4C5BC51);
const fwMvFlagId CTaskHumanLocomotion::ms_UseTransitionToIdleIntroId("UseTransitionToIdleIntro",0xC6734178);
const fwMvFlagId CTaskHumanLocomotion::ms_EnableIdleTransitionsId("UseAlternateIdleTransitions",0xF8191E25);
const fwMvFlagId CTaskHumanLocomotion::ms_EnableUpperBodyShadowExpressionId("EnableUpperbodyExpression",0x2549880);
const fwMvFlagId CTaskHumanLocomotion::ms_UseSuddenStopsId("UseSuddenStops",0x24B2DF64);
const fwMvFlagId CTaskHumanLocomotion::ms_NoSprintId("NoSprint",0xE9976614);
const fwMvFlagId CTaskHumanLocomotion::ms_IsRunningId("Running",0x68AB84B5);
const fwMvFlagId CTaskHumanLocomotion::ms_BlockGenericFallbackId("BlockGenericFallback",0x85FA6DE3);
const fwMvFlagId CTaskHumanLocomotion::ms_SkipIdleIntroFromTransitionId("SkipIdleIntroFromTransition",0x637D7414);
const fwMvFlagId CTaskHumanLocomotion::ms_LastFootLeftId("LastFootLeft",0x7A2EB41B);
const fwMvFlagId CTaskHumanLocomotion::ms_InStealthId("InStealth",0x29EAC9CE);
const fwMvFlagId CTaskHumanLocomotion::ms_UseFromStrafeTransitionUpperBodyId("UseFromStrafeTransitionUpperBody",0xE395BCB3);
const fwMvFlagId CTaskHumanLocomotion::ms_EnableUpperBodyFeatheredLeanId("EnableUpperBodyFeatheredLean",0x6D128F6);
const fwMvFlagId CTaskHumanLocomotion::ms_WeaponEmptyId("WeaponEmpty",0x8F1EEFC8);
const fwMvFlagId CTaskHumanLocomotion::ms_UseRunSprintId("UseRunSprint",0xf2907ff6);
const fwMvFlagId CTaskHumanLocomotion::ms_SkipIdleIntroId("SkipIdleIntro",0x364abf91);
const fwMvFlagId CTaskHumanLocomotion::ms_IsFatId("IsFat",0xdbedc678);

////////////////////////////////////////////////////////////////////////////////

const CTaskMoveGoToPointAndStandStill* CTaskHumanLocomotion::GetGotoPointAndStandStillTask(const CPed* pPed)
{
	return static_cast<const CTaskMoveGoToPointAndStandStill*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL));
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::CTaskHumanLocomotion(State initialState, const fwMvClipSetId& clipSetId, const bool bIsCrouching, const bool bFromStrafe, const bool bLastFootLeft, const bool bUseLeftFootStrafeTransition, const bool bUseActionModeRateOverride, const fwMvClipSetId& baseModelClipSetId, const float fInitialMovingDirection)
: m_vLastSlopePosition(Vector3::ZeroType)
, m_ClipSet(NULL)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_GenericMovementClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipSet(NULL)
, m_WeaponFilterId(FILTER_ID_INVALID)
, m_pWeaponFilter(NULL)
, m_TransitionToIdleIntroClipSetId(CLIP_SET_ID_INVALID)
, m_TransitionToIdleIntroClipId(CLIP_ID_INVALID)
, m_InitialState(initialState)
, m_fStartDirection(0.0f)
, m_fStartDirectionTarget(0.0f)
, m_fStartDesiredDirection(0.0f)
, m_fStopDirection(0.0f)
, m_fMovingDirection(0.0f)
, m_fMovingDirectionRate(0.0f)
, m_fMovingForwardTimer(0.0f)
, m_fPreviousMovingDirectionDiff(0.0f)
, m_fSmoothedSlopeDirection(0.0f)
, m_fSmoothedSnowDepth(0.0f)
, m_fSnowDepthSignal(0.0f)
, m_fMovementRate(1.0f)
, m_fMovementTurnRate(1.0f)
, m_fFoliageMovementRate(1.0f)
, m_fStealthMovementRate(1.0f)
, m_iMovingStopCount(0)
, m_fGaitReductionBlockTimer(0.0f)
, m_iIdleTurnCounter(0)
, m_uLastSlopeScanTime(0)
, m_fExtraHeadingRate(0.0f)
, m_fBeginMoveStartPhase(0.0f)
, m_fTimeBetweenAddingRunningShockingEvents(0.0f)
, m_fGenericMoveForwardBlendWeight(0.0f)
, m_fFromStrafeInitialFacingDirection(FLT_MAX)
, m_fFromStrafeTransitionUpperBodyWeight(0.0f)
, m_fFromStrafeTransitionUpperBodyDirection(0.0f)
, m_fInitialMovingDirection(fInitialMovingDirection)
, m_pActionModeRateData(NULL)
, m_uStairsDetectedTime(0)
, m_Flags(0)
, m_pUpperBodyAimPitchHelper(NULL)
, m_uSlowDownInWaterTime(0)
#if FPS_MODE_SUPPORTED
, m_fCurrentPitch(0.5f)
#endif // FPS_MODE_SUPPORTED
, m_uCameraHeadingLerpTime(UINT_MAX)
, m_MoveIdleOnEnter(false)
, m_MoveIdleTurnOnEnter(false)
, m_MoveIdleStandardEnter(false)
, m_MoveStartOnEnter(false)
, m_MoveMovingOnEnter(false)
, m_MoveTurn180OnEnter(false)
, m_MoveStopOnEnter(false)
, m_MoveRepositionMoveOnEnter(false)
, m_MoveFromStrafeOnEnter(false)
, m_MoveCustomIdleOnEnter(false)
, m_MoveStartOnExit(false)
, m_MoveMovingOnExit(false)
, m_MoveUseLeftFootStrafeTransition(false)
, m_MoveBlendOutIdleTurn(false)
, m_MoveBlendOutStart(false)
, m_MoveBlendOut180(false)
, m_MoveBlendOutStop(false)
, m_MoveCanEarlyOutForMovement(false)
, m_MoveCanResumeMoving(false)
, m_MoveActionModeBlendOutStart(false)
, m_MoveActionModeBlendOutStop(false)
, m_MoveIdleTransitionFinished(false)
, m_MoveIdleTransitionBlendInWeaponHolding(false)
, m_uIdleTransitionWeaponHash(0)
, m_fTransitionToIdleBlendOutTime(0.25f)
, m_MoveCustomIdleFinished(false)
, m_CheckForAmbientIdle(false)
, m_DoIdleTurnPostMovementUpdate(false)
, m_SkipNextMoveIdleStandardEnter(false)
#if FPS_MODE_SUPPORTED
, m_FirstTimeRun(true)
#endif // FPS_MODE_SUPPORTED
{
	SetInternalTaskType(CTaskTypes::TASK_HUMAN_LOCOMOTION);

	// Set flags
	if(bIsCrouching)
	{
		m_Flags.SetFlag(Flag_Crouching);
	}
	if(bFromStrafe)
	{
		m_Flags.SetFlag(Flag_FromStrafe);
	}
	if(bLastFootLeft)
	{
		m_Flags.SetFlag(Flag_StartsOnLeftFoot);
	}
	if(bUseLeftFootStrafeTransition)
	{
		m_Flags.SetFlag(Flag_UseLeftFootStrafeTransition);
	}
	if(bUseActionModeRateOverride)
	{
		m_pActionModeRateData = rage_new sActionModeRateData();
		m_pActionModeRateData->BaseModelClipSetId = baseModelClipSetId;
	}

	// Initialise the movement clip set
	SetMovementClipSet(clipSetId);

	// Initialise the weapon clip set
	SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID);

	for(s32 i = 0; i < MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES; i++)
	{
		m_fPreviousDesiredDirection[i] = 0.f;
	}

	for(s32 i = 0; i < ACT_MAX; i++)
	{
		m_MoveAlternateFinished[i] = false;
	}

	for(s32 i = 0; i < MAX_ACTIVE_STATE_TRANSITIONS; i++)
	{
		m_uStateTransitionTimes[i] = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::~CTaskHumanLocomotion()
{
	delete m_pActionModeRateData;
	m_pActionModeRateData = NULL;
	delete m_pUpperBodyAimPitchHelper;
	m_pUpperBodyAimPitchHelper = NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskHumanLocomotion::Debug() const
{
#if DEBUG_DRAW
	const CPed* pPed = GetPed();

	float fMovingDirection = fwAngle::LimitRadianAngleSafe(m_fMovingDirection + pPed->GetCurrentHeading());
	Matrix34 m;
	m.MakeRotateZ(fMovingDirection);

	const Vector3 vPedPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

	grcDebugDraw::Line(vPedPos, vPedPos + m.b, Color_pink);
#endif // DEBUG_DRAW

	CTaskMotionBase::Debug();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskHumanLocomotion::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
	case State_Initial:        return "Initial";
	case State_Idle:           return "Idle";
	case State_IdleTurn:       return "IdleTurn";
	case State_Start:          return "Start";
	case State_Moving:         return "Moving";
	case State_Turn180:        return "Turn180";
	case State_Stop:           return "Stop";
	case State_RepositionMove: return "RepositionMove";
	case State_FromStrafe:     return "FromStrafe";
	case State_CustomIdle:	   return "CustomIdle";
	default:                   taskAssertf(0, "Unhandled state %d", iState); return "Unknown";
	}
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed* pPed = GetPed();

	switch(GetState())
	{
	case State_Moving:
		{
			if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0 && !pPed->GetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity))
			{
				const float fDesiredDir = CalcDesiredDirection();
				const float fAbsDesiredDir = Abs(fDesiredDir);
				if(fAbsDesiredDir < HALF_PI)
				{
					// If the velocity is going to make us overshoot, clamp it
					Vector3 vAngVel(pPed->GetDesiredAngularVelocity());

					const float fRotateSpeed = vAngVel.z;
					const float fRotateSpeedSign = Sign(fRotateSpeed);
					const float fDesiredDirSign = Sign(fDesiredDir);
					const float fHeadingDiff = fAbsDesiredDir / fTimeStep;
					if(fRotateSpeed < -fHeadingDiff || fRotateSpeed > fHeadingDiff || fRotateSpeedSign != fDesiredDirSign)
					{
						float fNewAngVel = Clamp(fRotateSpeed, -fHeadingDiff, fHeadingDiff);
						if(fRotateSpeedSign != fDesiredDirSign)
							fNewAngVel = 0.f;

						float fAngVelDiff = vAngVel.z - fNewAngVel;

						vAngVel.z = fNewAngVel;
						pPed->SetDesiredAngularVelocity(vAngVel);

						//
						// Rotate the vel
						//

						Vector3 vVel = pPed->GetDesiredVelocity();

						//
						// Remove the ground physical vel - doing the inverse of what is done to the animated velocity in CTaskMotionBase::CalcDesiredVelocity
						//

						Matrix34 matAngularRot(Matrix34::IdentityType);
						// Rotate this by the angular velocity
						if(vAngVel.IsNonZero())
						{
							// Get the ground angular velocity (minus angular velocity).
							Vector3 vDesiredAngVel = vAngVel;
							if(pPed->GetGroundPhysical())
							{
								vDesiredAngVel -= VEC3V_TO_VECTOR3(pPed->GetGroundAngularVelocity());
							}

							float fTurnAmount = vDesiredAngVel.Mag();
							if(fTurnAmount > 0.00001f)	
							{
								// Normalize it
								Vector3 vTurnAxis;
								vTurnAxis.InvScale(vDesiredAngVel, fTurnAmount);
								fTurnAmount *= fTimeStep;
								matAngularRot.RotateUnitAxis(vTurnAxis, fTurnAmount);
							}
						}

						vVel -= VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
						matAngularRot.UnTransform(vVel);

						Matrix34 m;
						m.MakeRotateZ(-fAngVelDiff * fTimeStep);
						m.Transform3x3(vVel);

						// Re-add the ground physical vel
						matAngularRot.Transform(vVel);
						vVel += VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());

						pPed->SetDesiredVelocity(vVel);

						return true;
					}
				}
			}
		}
		break;
	case State_Turn180:
		{
			if(m_MoveNetworkHelper.IsNetworkActive() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover))
			{
				// Modify ang velocity so the 180 goes more in the direction we want it to go
				const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_Turn180ClipId);
				if(pClip)
				{
					float fPhase = m_MoveNetworkHelper.GetFloat(ms_Turn180PhaseId);

					// Get the remaining yaw rotation
					Quaternion q = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip,  fPhase, 1.0f);
					Vector3 v;
					q.ToEulers(v);
					if(Abs(v.z) > 0.f)
					{
						// Calculate the extra angle we will apply
						float f = SubtractAngleShorter(CalcDesiredDirection(), v.z);
						static dev_float MAX_CHANGE = DtoR * 2.5f;
						f = Clamp(f, -MAX_CHANGE, MAX_CHANGE);

						// Add the extra vel
						float fAngVelocity = pPed->GetAnimatedAngularVelocity();
						fAngVelocity += (f / fTimeStep);
						pPed->SetDesiredAngularVelocity(Vector3(0.f, 0.f, fAngVelocity));
						return true;
					}
				}
			}
		}
		break;
	default:
		break;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ProcessPostCamera()
{
#if FPS_MODE_SUPPORTED
	CPed* pPed = GetPed();

	camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if(pFpsCamera)
	{
		bool bSetHeading = pFpsCamera->IsSprintBreakOutActive();

		if(pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsPlayerStateValidForFPSManipulation())
		{
			bSetHeading = true;
		}

		const bool bShouldBlockCameraHeadingUpdate = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsClimbingLadder);

		if(bSetHeading && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover) && !pPed->GetUsingRagdoll() && !bShouldBlockCameraHeadingUpdate)
		{
			if(m_uCameraHeadingLerpTime == UINT_MAX)
			{
				m_uCameraHeadingLerpTime = fwTimer::GetTimeInMilliseconds();
			}

			const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
			float fAimHeading = aimCameraFrame.ComputeHeading();
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true))
			{
				fAimHeading = camInterface::GetPlayerControlCamHeading(*pPed) + pFpsCamera->GetRelativeHeading();
			}
			fAimHeading = fwAngle::LimitRadianAngle(fAimHeading);

			u32 uLerpTime = fwTimer::GetTimeInMilliseconds() - m_uCameraHeadingLerpTime;

			TUNE_GROUP_INT(PED_MOVEMENT, LERP_LOCO_HEADING_AFTER_START_TIME_MS, 1000, 0, 10000, 1);
			if(uLerpTime < LERP_LOCO_HEADING_AFTER_START_TIME_MS)
			{
				const float fLerpRatio = (float)uLerpTime / (float)LERP_LOCO_HEADING_AFTER_START_TIME_MS;

				const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
				fAimHeading = fwAngle::LerpTowards(fCurrentHeading, fAimHeading, fLerpRatio);
			}

#if FPS_MODE_SUPPORTED
			// B*2248699: Cache ped hand position before setting new heading. 
			// Used in CTaskPlayerOnFoot::IsStateValidForIK to update weapon grip position based on this change in position.
			// Need to do this as attachments aren't updated before we call CTaskPlayerOnFoot::IsStateValidForIK.
			if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetPlayerInfo())
			{
				const crSkeleton* pPlayerSkeleton = pPed->GetSkeleton();
				if (pPlayerSkeleton)
				{
					// Get left hand bone index.
					s32 boneIdxHandLeft = -1;
					pPlayerSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_IK_HAND, boneIdxHandLeft);

					if (boneIdxHandLeft >= 0)
					{
						// Get left hand position.
						Vec3V vLeftHandPos(V_ZERO);
						Mat34V mtxHandLeft;
						pPlayerSkeleton->GetGlobalMtx(boneIdxHandLeft, mtxHandLeft);
						vLeftHandPos = mtxHandLeft.GetCol3();

						// Cache in player info.
						pPed->GetPlayerInfo()->SetLeftHandPositionFPS(vLeftHandPos);

					}
				}
			}
#endif	// FPS_MODE_SUPPORTED

			pPed->SetHeading(fAimHeading);
		}
		else
		{
			m_uCameraHeadingLerpTime = UINT_MAX;
		}
	}
#endif // FPS_MODE_SUPPORTED

	return CTaskMotionBase::ProcessPostCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ProcessMoveSignals()
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		//
		// Every state move signals
		//

		CPed* pPed = GetPed();

		// If we have just changed movement speed in the animation, make sure our move blend ratio matches (done after move blend ratio calculations)
		if(m_MoveNetworkHelper.GetBoolean(ms_RunningId))
		{
			m_Flags.SetFlag(Flag_Running);
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(Max(pPed->GetMotionData()->GetCurrentMbrY(), MOVEBLENDRATIO_RUN));
		}
		else if(m_MoveNetworkHelper.GetBoolean(ms_WalkingId))
		{
			m_Flags.ClearFlag(Flag_Running);
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(Min(pPed->GetMotionData()->GetCurrentMbrY(), MOVEBLENDRATIO_WALK));
		}

		// Reset 'this' frame flags
		m_Flags.ClearFlag(Flag_LastFootLeftThisFrame);
		m_Flags.ClearFlag(Flag_LastFootRightThisFrame);

		// Update foot down flags
		if(CheckBooleanEvent(this, ms_AefFootHeelLId))
		{
			m_Flags.SetFlag(Flag_LastFootLeft);
			m_Flags.SetFlag(Flag_LastFootLeftThisFrame);
			m_Flags.ClearFlag(Flag_FlipLastFootLeft);
			m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
		}
		else if(CheckBooleanEvent(this, ms_AefFootHeelRId))
		{
			m_Flags.ClearFlag(Flag_LastFootLeft);
			m_Flags.SetFlag(Flag_LastFootRightThisFrame);
			m_Flags.ClearFlag(Flag_FlipLastFootLeft);
			m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
		}

#if FPS_MODE_SUPPORTED
		ComputeAndSendPitchedWeaponHolding();
#endif // FPS_MODE_SUPPORTED

		for(s32 i = 0; i < ACT_MAX; i++)
		{
			m_MoveAlternateFinished[i] |= m_MoveNetworkHelper.GetBoolean(ms_AlternateFinishedId[i]);
		}

		switch(GetState())
		{
		case State_Initial:
			break;
		case State_Idle:
			StateIdle_OnProcessMoveSignals();
			break;
		case State_IdleTurn:
			StateIdleTurn_OnProcessMoveSignals();
			break;
		case State_Start:
			StateStart_OnProcessMoveSignals();
			break;
		case State_Turn180:
			StateTurn180_OnProcessMoveSignals();
			break;
		case State_Stop:
			StateStop_OnProcessMoveSignals();
			break;
		case State_CustomIdle:
			StateCustomIdle_OnProcessMoveSignals();
			break;
		default:
			break;
		}
	}

	//We have things that need to be done every frame, so never turn this callback off.
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::SupportsMotionState(CPedMotionStates::eMotionState state)
{
	if(m_Flags.IsFlagSet(Flag_Crouching))
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Crouch_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Crouch_Walk:
		case CPedMotionStates::MotionState_Crouch_Run:
			return true;
		default:
			return false;
		}
	}
	else
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
			return true;
		default:
			return false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	if(m_Flags.IsFlagSet(Flag_Crouching))
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Crouch_Idle:
			return GetState() == State_Idle;
		case CPedMotionStates::MotionState_Crouch_Walk:
			return GetState() == State_Moving && GetMotionData().GetIsWalking();
		case CPedMotionStates::MotionState_Crouch_Run:
			return GetState() == State_Moving && GetMotionData().GetIsRunning();
		default:
			return false;
		}
	}
	else
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Idle:
			return GetState() == State_Idle;
		case CPedMotionStates::MotionState_Walk:
			return GetState() == State_Moving && GetMotionData().GetIsWalking();
		case CPedMotionStates::MotionState_Run:
			return GetState() == State_Moving && GetMotionData().GetIsRunning();
		case CPedMotionStates::MotionState_Sprint:
			return GetState() == State_Moving && GetMotionData().GetIsSprinting();
		default:
			return false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds)
{
	speeds.Zero();

	fwClipSet* pSet = m_ClipSet;
	if(pSet)
	{
		static const fwMvClipId s_walkClip("walk",0x83504C9C);
		static const fwMvClipId s_runClip("run",0x1109B569);
		static const fwMvClipId s_sprintClip("sprint",0xBC29E48);	
		CPed* pPed = GetPed();
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected)) 
		{
			static const fwMvClipId s_walkDownClip("walk_down",0x3F401750);
			static const fwMvClipId s_runDownClip("run_down",0xAAB0A37);
			static const fwMvClipId s_walkUpClip("walk_up",0x6368028C);
			static const fwMvClipId s_runUpClip("run_up",0x686E6ACB);
			float slopePhase = ConvertRadianToSignal(m_fSmoothedSlopeDirection);
			if (slopePhase >= 0.55f) 
			{
				const fwMvClipId clipNames[3] = { s_walkDownClip, s_runDownClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}			
			else if (slopePhase <= 0.45f)	
			{
				const fwMvClipId clipNames[3] = { s_walkUpClip, s_runUpClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}					
			else 
			{
				const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}
		}
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
		{
			static const fwMvClipId s_walkDownSlopeClip("walk_down_slope",0xBD500C67);
			static const fwMvClipId s_runDownSlopeClip("run_down_slope",0xE69649A2);
			static const fwMvClipId s_walkUpSlopeClip("walk_up_slope",0x716E9AAE);
			static const fwMvClipId s_runUpSlopeClip("run_up_slope",0x2DAC118A);
			float slopePhase = ConvertRadianToSignal(m_fSmoothedSlopeDirection);
			if (slopePhase >= 0.55f)			
			{
				const fwMvClipId clipNames[3] = { s_walkDownSlopeClip, s_runDownSlopeClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}				
			else if (slopePhase <= 0.45f)	
			{
				const fwMvClipId clipNames[3] = { s_walkUpSlopeClip, s_runUpSlopeClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}	
			else 
			{
				const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };
				RetrieveMoveSpeeds(*pSet, speeds, clipNames);
			}
		}
		else 
		{
			const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };
			RetrieveMoveSpeeds(*pSet, speeds, clipNames);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsInMotion(const CPed* pPed) const
{
	switch(GetState())
	{
	case State_Initial:
	case State_Idle:
	case State_IdleTurn:
	case State_CustomIdle:
		return false;

	case State_Moving:
	case State_Turn180:
	case State_FromStrafe:
		return true;

	case State_Start:
	case State_Stop:
	case State_RepositionMove:
		{
			static dev_float TREATED_AS_STOPPED_SPEED = 1.f;
			return pPed->GetVelocity().Mag2() > square(TREATED_AS_STOPPED_SPEED);
		}

	default:
		taskAssertf(0, "Unhandled state %s", GetStateName(GetState()));
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	outClipSet = m_ClipSetId;
	outClip    = CLIP_IDLE;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CTaskHumanLocomotion::CreatePlayerControlTask()
{
	return rage_new CTaskPlayerOnFoot();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ShouldDisableSlowingDownForCornering() const
{
	return GetState()==CTaskHumanLocomotion::State_Initial || GetState()==CTaskHumanLocomotion::State_Idle || (GetState()==CTaskHumanLocomotion::State_Start && GetTimeInState() < 0.5f);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::AllowLadderRunMount() const
{
	return GetState()==CTaskHumanLocomotion::State_Moving;
}


////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SetMovementClipSet(const fwMvClipSetId& clipSetId)
{
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	if(m_ClipSetId != clipSetId)
	{
		taskAssertf(m_ClipSetId != fwMvClipSetId("cover@weapon@core"), "%s ped %s is setting the movement clipset with a cover clipset, this is wrong!", GetPed()->IsNetworkClone() ? "CLONE" : "LOCAL", GetPed()->GetDebugName());
		// Store the new clip set
		m_ClipSetId = clipSetId;
		m_ClipSet = fwClipSetManager::GetClipSet(clipSetId);

		// Set the generic movement clip set id
		m_GenericMovementClipSetId = m_ClipSetId;

		bool bClipSetStreamed = false;
		fwClipSet* pMovementClipSet = m_ClipSet;
		if(taskVerifyf(pMovementClipSet, "Movement clip set [%s] doesn't exist", m_ClipSetId.GetCStr()))
		{
			// Should we use the generic fall back?
			if(!CheckClipSetForMoveFlag(m_ClipSetId, ms_BlockGenericFallbackId))
			{
				if(pMovementClipSet->GetFallbackId() != CLIP_SET_ID_INVALID)
				{
					m_GenericMovementClipSetId = pMovementClipSet->GetFallbackId();
				}
			}
			else
			{
				m_Flags.SetFlag(Flag_BlockGenericFallback);
			}

			if(pMovementClipSet->Request_DEPRECATED())
			{
				// Clip set streamed
				bClipSetStreamed = true;
			}

			if(m_pActionModeRateData)
			{
				taskAssert(m_pActionModeRateData->BaseModelClipSetId != CLIP_SET_ID_INVALID);
				// Store the action mode movement rates
				if(m_ClipSetId != m_pActionModeRateData->BaseModelClipSetId)
				{
					fwClipSet* pBaseClipSet = fwClipSetManager::GetClipSet(m_pActionModeRateData->BaseModelClipSetId);
					if(taskVerifyf(pBaseClipSet, "Base clip set [%s] doesn't exist", m_pActionModeRateData->BaseModelClipSetId.GetCStr()))
					{
						static const fwMvClipId s_Clips[sActionModeRateData::ACTION_MODE_SPEEDS] = { fwMvClipId("walk",0x83504C9C), fwMvClipId("run",0x1109B569) };
						static const atHashWithStringNotFinal s_Properties[sActionModeRateData::ACTION_MODE_SPEEDS] = { atHashWithStringNotFinal("moving_speed_walk",0x406FBAA), atHashWithStringNotFinal("moving_speed_run",0x74B75981) };
						for(s32 i = 0; i < sActionModeRateData::ACTION_MODE_SPEEDS; i++)
						{
							float* pBaseSpeed = pBaseClipSet->GetProperties().Access(s_Properties[i].GetHash());
							float* pMovementSpeed = pMovementClipSet->GetProperties().Access(s_Properties[i].GetHash());

							float fBaseSpeed = pBaseSpeed ? *pBaseSpeed : fwAnimHelpers::GetMoverTrackVelocity(m_pActionModeRateData->BaseModelClipSetId, s_Clips[i]).Mag();
							if (!pBaseSpeed)
								pBaseClipSet->GetProperties().Insert(s_Properties[i].GetHash(), fBaseSpeed);

							float fMovementSpeed = pMovementSpeed ? *pMovementSpeed : fwAnimHelpers::GetMoverTrackVelocity(m_ClipSetId, s_Clips[i]).Mag();
							if (!pMovementSpeed)
								pMovementClipSet->GetProperties().Insert(s_Properties[i].GetHash(), fMovementSpeed);
							
							m_pActionModeRateData->ActionModeSpeedRates[i] = Max(fBaseSpeed / fMovementSpeed, 1.f);
						}
					}
				}
			}

			// Store if clipset is flagged as fat
			m_Flags.ChangeFlag(Flag_IsFat, CheckClipSetForMoveFlag(m_ClipSetId, ms_IsFatId));
		}

		if(m_MoveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendMovementClipSet();
		}
		else
		{
			ASSERT_ONLY(VerifyMovementClipSet(m_ClipSetId, m_Flags, m_GenericMovementClipSetId, GetEntity() ? GetPed() : NULL));
			// Flag the change
			m_Flags.SetFlag(Flag_MovementClipSetChanged);
			taskAssertf(GetState() == State_Initial, "Movement clip set [%s] not streamed in when starting motion task, will cause a t-pose", m_ClipSetId.GetCStr());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	if(m_WeaponClipSetId != clipSetId)
	{
		m_Flags.ClearFlag(Flag_HasEmptyClip);

		// Store the new clip set
		m_WeaponClipSetId = clipSetId;

		bool bClipSetStreamed = false;
		if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
		{
			// Clip set doesn't need to be streamed
			bClipSetStreamed = true;
			m_WeaponClipSet = NULL;
		}
		else
		{
			m_WeaponClipSet = fwClipSetManager::GetClipSet(m_WeaponClipSetId);
			if(taskVerifyf(m_WeaponClipSet, "Weapon clip set [%s] doesn't exist", m_WeaponClipSetId.GetCStr()))
			{
				if(m_WeaponClipSet->Request_DEPRECATED())
				{
					// Clip set streamed
					bClipSetStreamed = true;
				}

				if(m_WeaponClipSet->GetClipItem(ms_ClipEmpty))
				{
					m_Flags.SetFlag(Flag_HasEmptyClip);
				}

#if FPS_MODE_SUPPORTED
				if(m_WeaponClipSet->GetClip(ms_RunHighClipId))
				{
					m_Flags.SetFlag(Flag_HasPitchedWeaponHolding);
				}
				else if (m_Flags.IsFlagSet(Flag_HasPitchedWeaponHolding))
				{
					m_Flags.ClearFlag(Flag_HasPitchedWeaponHolding);
					m_MoveNetworkHelper.SetBoolean(ms_UsePitchedWeaponHoldingId, false);
				}
#endif // FPS_MODE_SUPPORTED
			}
		}

		if(m_MoveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendWeaponClipSet();
		}
		else
		{
			// Flag the change
			m_Flags.SetFlag(Flag_WeaponClipSetChanged);
		}
	}

	if(m_WeaponFilterId != filterId)
	{
		// Store the new filter
		m_WeaponFilterId = filterId;

		// Flag the change
		m_Flags.SetFlag(Flag_WeaponFilterChanged);
	}

	if(bUpperBodyShadowExpression != m_Flags.IsFlagSet(Flag_UpperBodyShadowExpression))
	{
		if(bUpperBodyShadowExpression)
		{
			m_Flags.SetFlag(Flag_UpperBodyShadowExpression);
		}
		else
		{
			m_Flags.ClearFlag(Flag_UpperBodyShadowExpression);
		}
		m_Flags.SetFlag(Flag_UpperBodyShadowExpressionChanged);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	if(data.IsValid())
	{
		m_alternateClips[type] = data;
		SetAlternateClipLooping(type, bLooping);

		if(m_MoveNetworkHelper.IsNetworkActive())
		{
			static const fwMvRequestId restartAlternateId("RestartAlternate",0xB90899C2);
			m_MoveNetworkHelper.SendRequest(restartAlternateId);
			SendAlternateParams(type);
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::EndAlternateClip(AlternateClipType type, float fBlendDuration)
{
	StopAlternateClip(type, fBlendDuration);

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		SendAlternateParams(type);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::UseLeftFootTransition() const
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(CheckBooleanEvent(this, ms_AefFootHeelLId))
		{
			return false;
		}
		else if(CheckBooleanEvent(this, ms_AefFootHeelRId))
		{
			return true;
		}
	}

	if(m_Flags.IsFlagSet(Flag_LastFootLeft))
	{
		return false;
	}
	else
	{
		return true;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::UseLeftFootStrafeTransition() const
{
	return m_MoveNetworkHelper.IsNetworkActive() && CheckBooleanEvent(this, ms_UseLeftFootStrafeTransitionId);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsLastFootLeft() const
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(CheckBooleanEvent(this, ms_AefFootHeelLId))
		{
			return true;
		}
		else if(CheckBooleanEvent(this, ms_AefFootHeelRId))
		{
			return false;
		}
	}

	return m_Flags.IsFlagSet(Flag_LastFootLeft);
}

////////////////////////////////////////////////////////////////////////////////

float CTaskHumanLocomotion::GetStandingSpringStrength() 
{
	static dev_u32 TIME_AFTER_STAIRS_DETECTED = 250;

	float fSpringStrength = CTaskMotionBase::GetStandingSpringStrength();
	const CPed* pPed = GetPed();

	// Increase spring strength when dismounting ladders, we don't want the feet to clip through the ground.
	if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsClimbingLadder) )
	{
		TUNE_GROUP_FLOAT(LADDER_DEBUG, fLadderDismountSpringMultiplier, 0.05f, 0.01f, 10.0f, 0.1f);	
		fSpringStrength *= fLadderDismountSpringMultiplier;
	}
	//! DMKH. Increase spring strength when jumping. The character will intersect the geometry otherwise.
	// The same goes for dismounting ladders
	else if( pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) )
	{
		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpLandSpringMultiplier, 3.5f, 0.01f, 5.0f, 0.1f);	
		fSpringStrength *= fJumpLandSpringMultiplier;
	}
	//! DMKH. Similarly, if we are forcing z to ground, make sure we increase spring strength to get ped
	//! back quicker.
	else if(pPed->m_PedResetFlags.GetEntityZFromGround() > 0)
	{
		TUNE_GROUP_FLOAT(PED_JUMPING, fSetEntityZSpringMultiplier, 5.0f, 0.01f, 10.0f, 0.1f);	
		fSpringStrength *= fSetEntityZSpringMultiplier;
	}
	else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
 	{
		fSpringStrength *= GetPed()->GetIkManager().GetSpringStrengthForStairs();
	}
	else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeSpringMultiplier, 3.0f, 0.01f, 8.0f, 0.1f);
		fSpringStrength *= fSlopeSpringMultiplier;
	}
	else if((m_uStairsDetectedTime + TIME_AFTER_STAIRS_DETECTED) > fwTimer::GetTimeInMilliseconds())
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, fAfterStairsSpringStrengthMultiplier, 6.0f, 0.01f, 10.0f, 0.1f);	
		fSpringStrength *= fAfterStairsSpringStrengthMultiplier;
	}

	return fSpringStrength;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskHumanLocomotion::GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive)
{
	if(bOutIsClipActive)
	{
		*bOutIsClipActive = false;
	}

	// No walk / runstops in lowlod
	if(GetPed()->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask))
	{
		return 0.f;
	}

	// Early out if we're in the idle state
	switch(GetState())
	{
	case State_Initial:
	case State_Idle:
	case State_IdleTurn:
		return 0.0f;
	default:
		break;
	}

	// If we are currently stopping, interrogate the anims for the stopping distance
	if(GetState() == State_Stop)
	{
		static const int iClipsToBlend = 5;
		const crClip* pMoveStopClip[iClipsToBlend] = 
		{
			m_MoveNetworkHelper.GetClip(ms_MoveStopClip_N_180Id),
			m_MoveNetworkHelper.GetClip(ms_MoveStopClip_N_90Id),
			m_MoveNetworkHelper.GetClip(ms_MoveStopClipId),
			m_MoveNetworkHelper.GetClip(ms_MoveStopClip_90Id),
			m_MoveNetworkHelper.GetClip(ms_MoveStopClip_180Id)
		};

		const float fMoveStopPhase[iClipsToBlend] = 
		{
			m_MoveNetworkHelper.GetFloat(ms_MoveStopPhase_N_180Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopPhase_N_90Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopPhaseId),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopPhase_90Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopPhase_180Id)
		};

		const float fMoveStopBlend[iClipsToBlend] = 
		{
			m_MoveNetworkHelper.GetFloat(ms_MoveStopBlend_N_180Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopBlend_N_90Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopBlendId),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopBlend_90Id),
			m_MoveNetworkHelper.GetFloat(ms_MoveStopBlend_180Id)
		};

		// See how many anims we are running, if only one just assume blend is 1.f
		int iBlendedAnims = 0;
		for(int i = 0; i < iClipsToBlend; ++i)
		{
			if(pMoveStopClip[i])
			{
				++iBlendedAnims;
			}
		}

		Vector3 vRemainingOffset(Vector3::ZeroType);
		for(int i = 0; i < iClipsToBlend; ++i)
		{
			float fBlendValue = (iBlendedAnims == 1) ? 1.f : fMoveStopBlend[i];
			if(pMoveStopClip[i] && fBlendValue > 0.f)
			{
				float fPhase = fMoveStopPhase[i];

				// If we have flipped the foot tags to force start one footstep in, the phase we get back from the tag synching is before the tag sync, 
				// which means it starts off at 0 and then jumps to the next foot tag, so we find the phase of the next foot tag and start from their
				if(fPhase < 0.1f)
				{
					if(m_Flags.IsFlagSet(Flag_FlipLastFootLeft))
					{
						if(m_Flags.IsFlagSet(Flag_LastFootLeft))
						{
							CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pMoveStopClip[i], CClipEventTags::Foot, CClipEventTags::Left, true, fPhase);
						}
						else
						{
							CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pMoveStopClip[i], CClipEventTags::Foot, CClipEventTags::Right, true, fPhase);
						}
					}
				}

				// If the anims started in origo or got an "initial offset" stored we wouldn't need to use the Diff function that makes two calls
				vRemainingOffset += fwAnimHelpers::GetMoverTrackTranslationDiff(*pMoveStopClip[i], fPhase, 1.0f) * fBlendValue;
			}
		}

		if(iBlendedAnims > 0)
		{
			if(bOutIsClipActive)
			{
				*bOutIsClipActive = true;
			}

			return vRemainingOffset.XYMag();
		}
	}

	if(m_ClipSetId != CLIP_SET_ID_INVALID)
	{
		// We have no active clips, estimate the distance from what we will play
		enum Foot { FOOT_LEFT, FOOT_RIGHT, NUM_FEET, };
		enum StopClips { WALK_N_180, WALK_N_90, WALK_0, WALK_90, WALK_180, RUN_0, NUM_STOP_CLIPS };

		static const fwMvClipId stopClips[NUM_FEET][NUM_STOP_CLIPS] =
		{
			{ fwMvClipId("wstop_l_-180",0xBC56B178), fwMvClipId("wstop_l_-90",0x84AF46C6), fwMvClipId("wstop_l_0",0xED71B76C), fwMvClipId("wstop_l_90",0x5BA7D542), fwMvClipId("wstop_l_180",0x7B394199), fwMvClipId("rstop_l",0xE53DB8E9) },
			{ fwMvClipId("wstop_r_-180",0x2B0CA6DA), fwMvClipId("wstop_r_-90",0xA68C84A0), fwMvClipId("wstop_r_0",0x757B7872), fwMvClipId("wstop_r_90",0xC9D4D418), fwMvClipId("wstop_r_180",0xACED7232), fwMvClipId("rstop_r",0x3FC66DF9) },
		};

		fwClipSet* pSet = m_ClipSet;
		taskAssert(pSet);
		if(!pSet)
		{
			return 0.f;
		}

		const int iFoot = (m_Flags.IsFlagSet(Flag_LastFootLeftThisFrame) || !m_Flags.IsFlagSet(Flag_LastFootLeft)) ? FOOT_RIGHT : FOOT_LEFT;
		if(!m_Flags.IsFlagSet(Flag_Running))
		{
			CPed* pPed = GetPed();
			const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
			const float fStopHeading = ((fStopDirection < DEFAULT_NAVMESH_FINAL_HEADING) ? fwAngle::LimitRadianAngleSafe(fStopDirection) : fCurrentHeading);
			const float fHeadingDelta = SubtractAngleShorter(fCurrentHeading, fStopHeading);

			fwMvClipId clip1, clip2;
			float fWeight;
			if(fHeadingDelta < -HALF_PI)
			{
				fWeight = (fHeadingDelta + HALF_PI) / -HALF_PI;
				clip1 = stopClips[iFoot][WALK_N_180];
				clip2 = stopClips[iFoot][WALK_N_90];
			}
			else if(fHeadingDelta < 0.f)
			{
				fWeight = fHeadingDelta / -HALF_PI;
				clip1 = stopClips[iFoot][WALK_N_90];
				clip2 = stopClips[iFoot][WALK_0];
			}
			else if(fHeadingDelta < HALF_PI)
			{
				fWeight = fHeadingDelta / HALF_PI;
				clip1 = stopClips[iFoot][WALK_90];
				clip2 = stopClips[iFoot][WALK_0];
			}
			else
			{
				fWeight = (fHeadingDelta - HALF_PI) / HALF_PI;
				clip1 = stopClips[iFoot][WALK_180];
				clip2 = stopClips[iFoot][WALK_90];
			}

			crClip* pClip1 = pSet->GetClip(clip1);
			crClip* pClip2 = pSet->GetClip(clip2);

			// We attempt to access cached data first as it is very expensive to poll the animation for this information
			float* pfDist1 = pSet->GetProperties().Access(clip1.GetHash());
			float* pfDist2 = pSet->GetProperties().Access(clip2.GetHash());

			float fDist1 = 0.f;
			if(pfDist1)
				fDist1 = *pfDist1;
			else if(taskVerifyf(pClip1, "[%s] Does not exist in ClipSet [%s]", clip1.TryGetCStr(), m_ClipSetId.TryGetCStr()))
			{
				fDist1 = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip1, 0.f, 1.f).XYMag();
				pSet->GetProperties().Insert(clip1.GetHash(), fDist1);
			}

			float fDist2 = 0.f;
			if(pfDist2)
				fDist2 = *pfDist2;
			else if(taskVerifyf(pClip2, "[%s] Does not exist in ClipSet [%s]", clip2.TryGetCStr(), m_ClipSetId.TryGetCStr()))
			{
				fDist2 = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip2, 0.f, 1.f).XYMag();
				pSet->GetProperties().Insert(clip2.GetHash(), fDist2);
			}

			// We can always assume that these two anims are perpendicular
			// Perpendicular
			const float fDeltaX = fDist1 * fWeight;
			const float fDeltaY = fDist2 * (1.f - fWeight);
			const float fStoppingDistance = sqrt((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY));

			const float fStoppingDistExtra = 0.45f;				//
			const float fStoppingDistExtraOrientation = 0.35f;	// For oriented stops we might slide more sideways if we raise this even more
				
			// If we are playing a non oriented exact stop we accept a longer tolerance
			const float fStoppingDistanceTolerance = abs(fHeadingDelta) < 0.1f ? fStoppingDistExtra : fStoppingDistExtraOrientation;

			// Assuming we got some stop distance to make this tolerance a reasonable value
			if (fStoppingDistance > 0.5f)
				return fStoppingDistance + fStoppingDistanceTolerance;

			return fStoppingDistance;

		}
		else
		{
			fwMvClipId clip = stopClips[iFoot][RUN_0];
			crClip* pClip = pSet->GetClip(clip);

			float* pfDist = pSet->GetProperties().Access(clip.GetHash());
	
			float fDist = 0.f;
			if(pfDist)
				fDist = *pfDist;
			else if(taskVerifyf(pClip, "[%s] Does not exist in ClipSet [%s]", clip.TryGetCStr(), m_ClipSetId.TryGetCStr()))
			{
				fDist = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.f, 1.f).XYMag();
				pSet->GetProperties().Insert(clip.GetHash(), fDist);
			}

			return fDist;
		}
	}

	return 0.f;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsGoingToStopThisFrame(const CTaskMoveGoToPointAndStandStill* pGotoTask) const
{
	taskAssert(pGotoTask);
	static dev_float FORCE_STOP_DIST = 0.2f;
	if(IsFootDown() || pGotoTask->GetDistanceRemaining() < FORCE_STOP_DIST)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsOkToSetDesiredStopHeading(const CTaskMoveGoToPointAndStandStill* pGotoTask) const
{
	taskAssert(pGotoTask);
	if(GetState() == State_Stop)
	{
		if(m_Flags.IsFlagSet(Flag_OrientatedStop))
		{
			return true;
		}
		else
		{
			// Only allow when we can break out of movement
			if(m_MoveCanEarlyOutForMovement)
			{
				return true;
			}
		}
	}
	else if(IsGoingToStopThisFrame(pGotoTask))
	{
		// If we are running, we don't have orientated stops
		if(m_Flags.IsFlagSet(Flag_Running))
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SetTransitionToIdleIntro(const CPed *pPed, const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, bool bSupressWeaponHolding, float fBlendOutTime)
{
	if(m_TransitionToIdleIntroClipSetId != clipSetId)
	{
		m_TransitionToIdleIntroClipSetId = clipSetId;

		if(m_TransitionToIdleIntroClipSetId == CLIP_SET_ID_INVALID)
		{
			m_TransitionToIdleClipSetRequestHelper.Release();
		}
	}

	if(bSupressWeaponHolding)
	{
		m_Flags.SetFlag(Flag_SuppressWeaponHoldingForIdleTransition);
		m_Flags.SetFlag(Flag_WeaponClipSetChanged);
	}

	m_uIdleTransitionWeaponHash = pPed->GetMovementModeWeaponHash();
	m_TransitionToIdleIntroClipId = clipId;
	m_fTransitionToIdleBlendOutTime = fBlendOutTime;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ClearTransitionToIdleIntro()
{
	m_TransitionToIdleIntroClipSetId = CLIP_SET_ID_INVALID;
	m_TransitionToIdleIntroClipId = CLIP_ID_INVALID;
	m_MoveNetworkHelper.SetFlag(false, ms_UseTransitionToIdleIntroId);
	m_TransitionToIdleClipSetRequestHelper.Release();
	if(m_Flags.IsFlagSet(Flag_SuppressWeaponHoldingForIdleTransition))
	{
		m_Flags.ClearFlag(Flag_SuppressWeaponHoldingForIdleTransition);
		m_Flags.SetFlag(Flag_WeaponClipSetChanged);
	}
	m_uIdleTransitionWeaponHash = 0;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsTransitionToIdleIntroActive() const
{
	if(GetState() == State_Initial || GetState() == State_Idle)
	{
		return m_TransitionToIdleIntroClipSetId != CLIP_SET_ID_INVALID && m_TransitionToIdleIntroClipId != CLIP_ID_INVALID;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::CheckClipSetForMoveFlag(const fwMvClipSetId& clipSetId, const fwMvFlagId& flagId)
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", clipSetId.TryGetCStr()))
		{
			const atArray<atHashString>& clipSetFlags = pClipSet->GetMoveNetworkFlags();
			for(s32 i = 0; i < clipSetFlags.GetCount(); i++)
			{
				if(clipSetFlags[i].GetHash() == flagId.GetHash())
				{
					return true;
				}
			}
		}
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::CheckBooleanEvent(const CTask* pTask, const fwMvBooleanId& id)
{
	taskAssert(pTask);
	taskAssert(pTask->GetMoveNetworkHelper());
	taskAssert(pTask->GetMoveNetworkHelper()->IsNetworkActive());
	
	bool bBool = pTask->GetMoveNetworkHelper()->GetBoolean(id);
	while(!bBool && pTask->GetSubTask())
	{
		if(pTask->GetSubTask()->GetMoveNetworkHelper())
		{
			bBool = pTask->GetSubTask()->GetMoveNetworkHelper()->GetBoolean(id);
		}
		pTask = pTask->GetSubTask();
	}
	return bBool;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	ProcessMovement();

	// Process AI logic
	ProcessAI();

	m_fGaitReductionBlockTimer -= GetTimeStep();
	if(m_fGaitReductionBlockTimer > 0.0f)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableGaitReduction, true);
	}

	if(GetOverrideWeaponClipSet() != CLIP_SET_ID_INVALID)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableActionMode, true);
	}

	UpdateGaitReduction();


	for(s32 i = 0; i < ACT_MAX; i++)
	{
		if(m_MoveAlternateFinished[i])
		{
			if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
			{
				CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(GetParent());
				pTask->StopAlternateClip(static_cast<AlternateClipType>(i), m_alternateClips[i].fBlendDuration);
			}
			else
			{
				EndAlternateClip(static_cast<AlternateClipType>(i), m_alternateClips[i].fBlendDuration);
			}
		}
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	}
#endif // FPS_MODE_SUPPORTED

//	CPedAILod& lod = GetPed()->GetPedAiLod();
//	lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnEnter)
	{
		RecordStateTransition();
	}

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Idle)
			FSM_OnEnter
				return StateIdle_OnEnter();
			FSM_OnUpdate
				return StateIdle_OnUpdate();
			FSM_OnExit
				return StateIdle_OnExit();
		FSM_State(State_IdleTurn)
			FSM_OnEnter
				return StateIdleTurn_OnEnter();
			FSM_OnUpdate
				return StateIdleTurn_OnUpdate();
			FSM_OnExit
				return StateIdleTurn_OnExit();
		FSM_State(State_Start)
			FSM_OnEnter
				return StateStart_OnEnter();
			FSM_OnUpdate
				return StateStart_OnUpdate();
			FSM_OnExit
				return StateStart_OnExit();
		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();
			FSM_OnExit
				return StateMoving_OnExit();
		FSM_State(State_Turn180)
			FSM_OnEnter
				return StateTurn180_OnEnter();
			FSM_OnUpdate
				return StateTurn180_OnUpdate();
			FSM_OnExit
				return StateTurn180_OnExit();
		FSM_State(State_Stop)
			FSM_OnEnter
				return StateStop_OnEnter();
			FSM_OnUpdate
				return StateStop_OnUpdate();
			FSM_OnExit
				return StateStop_OnExit();
		FSM_State(State_RepositionMove)
			FSM_OnEnter
				return StateRepositionMove_OnEnter();
			FSM_OnUpdate
				return StateRepositionMove_OnUpdate();
		FSM_State(State_FromStrafe)
			FSM_OnEnter
				return StateFromStrafe_OnEnter();
			FSM_OnUpdate
				return StateFromStrafe_OnUpdate();
			FSM_OnExit
				return StateFromStrafe_OnExit();
		FSM_State(State_CustomIdle)
			FSM_OnEnter
				return StateCustomIdle_OnEnter();
			FSM_OnUpdate
				return StateCustomIdle_OnUpdate();
			FSM_OnExit
				return StateCustomIdle_OnExit();
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::ProcessPostFSM()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Track if we were running
	m_Flags.ChangeFlag(Flag_WasRunning, m_Flags.IsFlagSet(Flag_Running));

	// Send signals valid for all states
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(m_Flags.IsFlagSet(Flag_MovementClipSetChanged))
		{
			if(m_ClipSet)
			{
				if(m_ClipSet->Request_DEPRECATED())
				{
					SendMovementClipSet();
				}
			}
		}

		if(m_Flags.IsFlagSet(Flag_WeaponClipSetChanged))
		{
			if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
			{
				SendWeaponClipSet();
			}
			else
			{
				if(m_WeaponClipSet)
				{
					if(m_WeaponClipSet->Request_DEPRECATED())
					{
						SendWeaponClipSet();
					}
				}
			}
		}

		// Only do these if we are not waiting for the weapon clipset to change
		if(!m_Flags.IsFlagSet(Flag_WeaponClipSetChanged))
		{
			if(m_Flags.IsFlagSet(Flag_WeaponFilterChanged))
			{
				StoreWeaponFilter();
				// Restart the state
				m_MoveNetworkHelper.SendRequest(ms_RestartWeaponHoldingId);
				m_Flags.ClearFlag(Flag_WeaponFilterChanged);
			}
		
			if(m_Flags.IsFlagSet(Flag_UpperBodyShadowExpressionChanged))
			{
				m_MoveNetworkHelper.SetFlag(m_Flags.IsFlagSet(Flag_UpperBodyShadowExpression), ms_EnableUpperBodyShadowExpressionId);
				// Restart the state
				m_MoveNetworkHelper.SendRequest(ms_RestartWeaponHoldingId);
				m_Flags.ClearFlag(Flag_UpperBodyShadowExpressionChanged);
			}
		}

		if(m_pWeaponFilter)
		{
			m_MoveNetworkHelper.SetFilter(m_pWeaponFilter, ms_WeaponHoldingFilterId);
		}
	
		for(s32 i = 0; i < ACT_MAX; i++)
		{
			SendAlternateParams(static_cast<AlternateClipType>(i));
		}

		// Send if the weapon is empty
		if(!m_Flags.IsFlagSet(Flag_WeaponClipSetChanged))
		{
			if(m_Flags.IsFlagSet(Flag_HasEmptyClip) && pPed->GetWeaponManager() && pPed->GetInventory())
			{
				s32 iAmmoRemaining = INT_MAX;
				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon))
				{
					// If swapping weapon, use the weapon we are swapping to
					const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
					if(pWeaponInfo && pWeaponInfo->GetUsesAmmo())
					{
						iAmmoRemaining = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo->GetHash());
					}
				}
				else
				{
					const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					if(pWeapon && pWeapon->GetWeaponInfo()->GetUsesAmmo())
					{
						iAmmoRemaining = pWeapon->GetAmmoTotal();
					}
				}

				if(iAmmoRemaining == 0)
				{
					taskAssertf(!m_Flags.IsFlagSet(Flag_WeaponClipSetChanged), "Weapon clipset [%s] changed but not set, enabling left grip", m_WeaponClipSetId.TryGetCStr());
					m_MoveNetworkHelper.SetFlag(true, ms_WeaponEmptyId);
				}
				else
				{
					m_MoveNetworkHelper.SetFlag(false, ms_WeaponEmptyId);
				}
			}
			else
			{
				m_MoveNetworkHelper.SetFlag(false, ms_WeaponEmptyId);
			}
		}
	
		// Persistent flags
		if(IsGaitReduced() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true)))
		{
			m_MoveNetworkHelper.SetFlag(true, ms_SkipWalkRunTransitionId);
		}
		else
		{
			bool bSkipwalkRunTransition = CheckClipSetForMoveFlag(m_ClipSetId, ms_SkipWalkRunTransitionId) || CheckClipSetForMoveFlag(m_GenericMovementClipSetId, ms_SkipWalkRunTransitionId);	
			m_MoveNetworkHelper.SetFlag(bSkipwalkRunTransition, ms_SkipWalkRunTransitionId );
		}
		
		//
		UpdateActionModeMovementRate();

		// Movement rate
		// Scripted rate
		float fMovementRate = pPed->GetMotionData()->GetCurrentMoveRateOverride();
		// Player rate
		if(pPed->GetPlayerInfo())
		{
			fMovementRate *= pPed->GetPlayerInfo()->GetRunSprintSpeedMultiplier();
		}
		// Movement rate
		fMovementRate *= m_fMovementRate;
		// Foliage rate
		fMovementRate *= m_fFoliageMovementRate;
		// Stealth rate
		fMovementRate *= m_fStealthMovementRate;
		// Action rate
		if(m_pActionModeRateData)
			fMovementRate *= m_pActionModeRateData->ActionModeMovementRate;
		// CNC Stair/Slope increased movement rate
		if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
		{
			if (m_Flags.IsFlagSet(Flag_WasOnStairs))
			{
				TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fStairMovementRateModifier_Up, 1.25f, 1.0f, 5.0f, 0.01f);
				TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fStairMovementRateModifier_Down, 1.125f, 1.0f, 5.0f, 0.01f);
				const float fNormalizedSlopeDirection = CNC_GetNormalizedSlopeDirectionForAdjustedMovementRate();
				const float fStairMovementRateModifier = Lerp(fNormalizedSlopeDirection, fStairMovementRateModifier_Up, fStairMovementRateModifier_Down);
				fMovementRate *= fStairMovementRateModifier;
			}
			else if (m_Flags.IsFlagSet(Flag_WasOnSlope) && !m_Flags.IsFlagSet(Flag_WasOnSnow))	// Don't set the movement rate if on snow (snow loco takes priority over slope loco in the MoVE network).
			{
				TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fSlopeMovementRateModifier_Uphill, 1.125f, 1.0f, 5.0f, 0.01f);
				TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fSlopeMovementRateModifier_Downhill, 1.0f, 1.0f, 5.0f, 0.01f);
				const float fNormalizedSlopeDirection = CNC_GetNormalizedSlopeDirectionForAdjustedMovementRate();
				const float fSlopeMovementRateModifier = Lerp(fNormalizedSlopeDirection, fSlopeMovementRateModifier_Uphill, fSlopeMovementRateModifier_Downhill);
				fMovementRate *= fSlopeMovementRateModifier;
			}
		}
		// Send signal
		m_MoveNetworkHelper.SetFloat(ms_MovementRateId, fMovementRate);
		// Send the turn rate
		m_MoveNetworkHelper.SetFloat(ms_MovementTurnRateId, m_fMovementTurnRate * fMovementRate);

		// Ensure the Running flag is up to date
		m_MoveNetworkHelper.SetFlag(m_Flags.IsFlagSet(Flag_Running), ms_IsRunningId);

		bool bSkipIdleIntroFromTransition = m_Flags.IsFlagSet(Flag_SkipIdleIntroFromTransition);
		if(bSkipIdleIntroFromTransition)
		{
			m_MoveNetworkHelper.SetFlag(bSkipIdleIntroFromTransition, ms_SkipIdleIntroFromTransitionId);
		}

		// Stealth
		m_MoveNetworkHelper.SetFlag(GetMotionData().GetUsingStealth(), ms_InStealthId);

		// Action mode
		if(pPed->IsUsingActionMode() && pPed->GetMovementModeData().m_UpperBodyFeatheredLeanEnabled)
		{
			m_MoveNetworkHelper.SetFlag(true, ms_EnableUpperBodyFeatheredLeanId);
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(false, ms_EnableUpperBodyFeatheredLeanId);
		}

		if(m_Flags.IsFlagSet(Flag_IsFat))
		{
			const CWeaponInfo* pWeaponInfo = NULL;
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon))
			{
				if(pPed->GetWeaponManager())
				{
					// If swapping weapon, use the weapon we are swapping to
					pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
				}
			}
			else
			{
				if(pPed->GetWeaponManager())
				{
					const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					if(pWeapon)
					{
						pWeaponInfo = pWeapon->GetWeaponInfo();
					}
				}
			}

			if(pWeaponInfo && pWeaponInfo->GetIsTwoHanded())
			{
				m_MoveNetworkHelper.SetFlag(false, ms_IsFatId);
			}
			else
			{
				m_MoveNetworkHelper.SetFlag(true, ms_IsFatId);
			}
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(false, ms_IsFatId);
		}
#if FPS_MODE_SUPPORTED
		if(m_FirstTimeRun)
		{
			ComputeAndSendPitchedWeaponHolding();
		}
#endif // FPS_MODE_SUPPORTED
	}

#if FPS_MODE_SUPPORTED
	m_FirstTimeRun = false;
#endif // FPS_MODE_SUPPORTED
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::CleanUp()
{
	// Clear any old filter
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}

	// Clear slope values
	ResetSlopesAndStairs(GetPed(), m_vLastSlopePosition);

	ResetGaitReduction();

	delete m_pUpperBodyAimPitchHelper;
	m_pUpperBodyAimPitchHelper = NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ProcessPostMovement()
{
	if(m_DoIdleTurnPostMovementUpdate)
	{
		taskAssert(GetState() == State_IdleTurn);
		// Store the initial directions after physics
		m_fStartDirection = CalcDesiredDirection();
		m_fStartDirectionTarget = m_fStartDirection;
		m_fStartDesiredDirection = GetPed()->GetMotionData()->GetDesiredHeading();
		SendIdleTurnSignals();
		m_DoIdleTurnPostMovementUpdate = false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateInitial_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHuman);

		// Check if clip sets are streamed in
		taskAssertf(m_ClipSet, "Movement clip set [%s] does not exist", m_ClipSetId.GetCStr());
		if(taskVerifyf(m_ClipSet->Request_DEPRECATED(), "Movement clip set [%s] not streamed in when starting motion task, will cause a t-pose PS(%s), TR(%.2f), TIS(%.2f)", m_ClipSetId.GetCStr(), GetStaticStateName(GetPreviousState()), GetTimeRunning(), GetTimeInState()) && 
			taskVerifyf(m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHuman), "ms_NetworkTaskOnFootHuman not streamed"))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootHuman);
		}
	}

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		// AI use a blended run/sprint
		m_MoveNetworkHelper.SetFlag(!pPed->IsPlayer(), ms_UseRunSprintId);

		if(!m_MoveNetworkHelper.IsNetworkAttached())
		{
			m_MoveNetworkHelper.DeferredInsert();
		}

		bool leaveDesiredMbr = pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMotionStateLeaveDesiredMBR) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));

		if(m_Flags.IsFlagSet(Flag_Crouching))
		{
			switch(GetMotionData().GetForcedMotionStateThisFrame())
			{		
			case CPedMotionStates::MotionState_Crouch_Idle:
				m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
				SetState(State_Idle);
				break;
			case CPedMotionStates::MotionState_Crouch_Walk:
				m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_WALK);
				SetState(State_Moving);
				break;
			case CPedMotionStates::MotionState_Crouch_Run:
				m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_RUN);
				m_Flags.SetFlag(Flag_Running);
				SetState(State_Moving);
				break;
			default:
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DoFPSSprintBreakOut, true);
				if(m_InitialState == State_Moving)
				{
					m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
					SetState(State_Moving);
				}
				else
				{
					m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId, ms_LocomotionStateId);
					SetState(State_Idle);
				}
				break;
			}
		}
		else
		{
			switch(GetMotionData().GetForcedMotionStateThisFrame())
			{
			case CPedMotionStates::MotionState_Idle:
				m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
				SetState(State_Idle);
				break;
			case CPedMotionStates::MotionState_Walk:
				m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_WALK);
				m_Flags.SetFlag(Flag_SetInitialMovingPhase);
				SetState(State_Moving);
				break;
			case CPedMotionStates::MotionState_Run:
				m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_RUN);
				m_Flags.SetFlag(Flag_Running);
				m_Flags.SetFlag(Flag_SetInitialMovingPhase);
				SetState(State_Moving);
				break;
			case CPedMotionStates::MotionState_Sprint:
				m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
				GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				if (!leaveDesiredMbr)
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				m_Flags.SetFlag(Flag_Running);
				m_Flags.SetFlag(Flag_SetInitialMovingPhase);
				SetState(State_Moving);
				break;
			default:
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DoFPSSprintBreakOut, true);
				if(m_InitialState == State_Moving)
				{
					m_MoveNetworkHelper.ForceStateChange(ms_MovingStateId, ms_LocomotionStateId);
					m_Flags.SetFlag(Flag_SetInitialMovingPhase);
					SetState(State_Moving);
				}
				else
				{
					m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId, ms_LocomotionStateId);
					SetState(State_Idle);
				}
				break;
			}
		}

		if(!pPed->IsNetworkClone() && pPed->GetNetworkObject())
		{
			taskAssertf(GetState()!=State_Initial, "Expect a new state to be set by here.");
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			if (!pPed->IsPlayer() || !static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsSpectating())
			{   
				//For peds or non-spectating players force send here:
				//Since motion state is InFrequent make sure that the remote gets set at the same time as the task state
				pPedObj->RequestForceSendOfRagdollState();  //CPedMovementGroupDataNode
				pPedObj->RequestForceSendOfTaskState();	//CPedTaskTreeDataNode
			}
		}

		if(GetState() != State_Idle)
		{
			// Clear the idle transition anim, as it will no longer be valid
			ClearTransitionToIdleIntro();

			if(GetState() == State_Moving && !pPed->GetMotionData()->GetIsLessThanRun())
			{
				m_Flags.SetFlag(Flag_Running);
			}
		}

		if(GetState() == State_Moving)
		{
			// If we are coming from strafe, see if we should go to the strafe transition state instead
			if(WillDoStrafeTransition(pPed))
			{
				m_MoveNetworkHelper.ForceStateChange(ms_FromStrafeStateId, ms_LocomotionStateId);
				SetState(State_FromStrafe);
				m_Flags.ClearFlag(Flag_FromStrafe);
			}

			// If going straight to moving, init the rate so it starts correctly
			if(m_pActionModeRateData && ShouldUseActionModeMovementRate())
			{
				if(m_Flags.IsFlagSet(Flag_Running))
				{
					m_pActionModeRateData->ActionModeMovementRate = m_pActionModeRateData->ActionModeSpeedRates[1];
				}
				else
				{
					m_pActionModeRateData->ActionModeMovementRate = m_pActionModeRateData->ActionModeSpeedRates[0];
				}
			}

			// Setup left foot flag
			if(m_Flags.IsFlagSet(Flag_StartsOnLeftFoot))
			{
				m_Flags.SetFlag(Flag_LastFootLeft);
				m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
				m_Flags.ClearFlag(Flag_StartsOnLeftFoot);
			}
		}
		else
		{
			m_Flags.ClearFlag(Flag_FromStrafe);
		}
	}

	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdle_OnEnter()
{
	CPed* pPed = GetPed();

	m_MoveNetworkHelper.WaitForTargetState(ms_IdleOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_StoppedId);

	// Reset the idle counter
	m_iIdleTurnCounter = 0;
	
	// Randomize the start phase and rate of idles
	m_MoveNetworkHelper.SetFloat(ms_IdleStartPhaseId, fwRandom::GetRandomNumberInRange(0.0f, 1.0f));

	// B*1434308 - Disable rate randomization on Player idles [6/3/2013 mdawe]
	if(!pPed->IsPlayer())
	{
		m_MoveNetworkHelper.SetFloat(ms_IdleRateId, fwRandom::GetRandomNumberInRange(0.85f, 1.15f));
	}

	// Clear flags
	m_Flags.ClearFlag(Flag_FullyInIdle);
	m_Flags.ClearFlag(Flag_Running);

	// If tired, pick it now! 
	if(!TransitionToIdleIntroSet())
	{
		CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
		if ( (pPed->IsPlayer() && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetSprintEnergyRatio() <= 0.5f)
			|| (pIntelligence && pIntelligence->GetLastGetUpTime() > 0 && (pIntelligence->GetLastGetUpTime() + CTaskAmbientClips::GetMaxTimeSinceGetUpForAmbientIdles()) > fwTimer::GetTimeInMilliseconds()) )
		{
			float fDesiredHeading = CalcDesiredDirection();
			if(Abs(fDesiredHeading) <= MIN_ANGLE)
			{
				m_Flags.SetFlag(Flag_FullyInIdle);
				CTaskAmbientClips* pTaskAmbient = static_cast<CTaskAmbientClips*>(pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
				if (pTaskAmbient)
				{
					pTaskAmbient->ForcePickNewIdleAnim();
				}
			}
		}
	}

	bool bSkipIdleIntro = pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipOnFootIdleIntro) || CheckClipSetForMoveFlag(m_ClipSetId, ms_SkipIdleIntroId);
	m_MoveNetworkHelper.SetFlag(bSkipIdleIntro, ms_SkipIdleIntroId);

	m_SkipNextMoveIdleStandardEnter = false;
	if(bSkipIdleIntro && GetMotionData().GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Idle)
	{
		// Skip the next true result from m_MoveIdleStandardEnter
		m_SkipNextMoveIdleStandardEnter = true;
	}

	StateIdle_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdle_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(m_MoveIdleStandardEnter)
	{
		if(!m_SkipNextMoveIdleStandardEnter)
		{
			// Randomize the start phase and rate of idles
			m_MoveNetworkHelper.SetFloat(ms_IdleStartPhaseId, fwRandom::GetRandomNumberInRange(0.0f, 1.0f));
		
			// B*1434308 - Disable rate randomization on Player idles [6/3/2013 mdawe]
			if(!pPed->IsPlayer())
			{
				m_MoveNetworkHelper.SetFloat(ms_IdleRateId, fwRandom::GetRandomNumberInRange(0.85f, 1.15f));
			}
		}
		m_SkipNextMoveIdleStandardEnter = false;

		m_Flags.SetFlag(Flag_FullyInIdle);

		m_MoveIdleStandardEnter = false;
		m_CheckForAmbientIdle = true;
	}

	if(m_CheckForAmbientIdle)
	{
		// Check if the Ped is wet or recently got up and force a new idle to play.
		if(!TransitionToIdleIntroSet())
		{
			CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
			CTaskAmbientClips* pTaskAmbient = static_cast<CTaskAmbientClips*>(pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pTaskAmbient && !pTaskAmbient->IsStreamingOrPlayingIdle())
			{
				bool bDustOff = pIntelligence->GetLastGetUpTime() > 0 && (pIntelligence->GetLastGetUpTime() + CTaskAmbientClips::GetMaxTimeSinceGetUpForAmbientIdles()) > fwTimer::GetTimeInMilliseconds();			
				if(  pPed->GetTimeSincePedInWater() < CTaskAmbientClips::GetSecondsSinceInWaterThatCountsAsWet()
					|| bDustOff )
				{
					pTaskAmbient->ForcePickNewIdleAnim();
					if (bDustOff)
					{
						m_Flags.SetFlag(Flag_FullyInIdle);
						pPed->SetDesiredHeading(pPed->GetCurrentHeading()); //cancel idle turn
					}
				} 
				m_CheckForAmbientIdle = false;
			}
		}
	}

	// Do we have an idle intro transition?
	if(TransitionToIdleIntroSet())
	{
		if(m_TransitionToIdleClipSetRequestHelper.Request(m_TransitionToIdleIntroClipSetId))
		{
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_TransitionToIdleIntroClipSetId, m_TransitionToIdleIntroClipId);
			if(pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_TransitionToIdleIntroId);
				m_MoveNetworkHelper.SetFlag(true, ms_UseTransitionToIdleIntroId);
				m_MoveNetworkHelper.SetFloat(ms_IdleTransitionBlendOutDurationId, m_fTransitionToIdleBlendOutTime);
			}
		}
	}

	if(m_MoveIdleTransitionFinished)
	{
		ClearTransitionToIdleIntro();
		m_MoveIdleTransitionFinished = false;
	}

	if(TransitionToIdleIntroSet())
	{
		//! If we change weapons during idle transition, stop suppressing upper body.
		bool bWeaponChanged = false;
		if(GetPed()->GetWeaponManager())
		{
			u32 uCurrentWeaponHash = pPed->GetMovementModeWeaponHash();
			bWeaponChanged = (uCurrentWeaponHash != m_uIdleTransitionWeaponHash);

			if(bWeaponChanged)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uCurrentWeaponHash);
				bool bQuitTransitionToIdleIntroOnWeaponChange = pWeaponInfo ? pWeaponInfo->GetFlag(CWeaponInfoFlags::QuitTransitionToIdleIntroOnWeaponChange) : false;
				
				//B*1735563: Cancel transition if we switch between 1h/2h weapons
				const CWeaponInfo* pPrevWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uIdleTransitionWeaponHash);		
				bool bQuitTransitionIfNotSameHanded = false;
				if (pWeaponInfo && pPrevWeaponInfo)
				{
					bool bIsPrevWeapon2Handed = pPrevWeaponInfo->GetIsGun2Handed() || pPrevWeaponInfo->GetIsMelee2Handed();
					bool bIsCurrentWeapon2Handed = pWeaponInfo->GetIsGun2Handed() || pWeaponInfo->GetIsMelee2Handed();
					bQuitTransitionIfNotSameHanded = bIsPrevWeapon2Handed != bIsCurrentWeapon2Handed ? true : false;
				}

				if(bQuitTransitionToIdleIntroOnWeaponChange || bQuitTransitionIfNotSameHanded)
				{
					m_MoveNetworkHelper.SetFlag(false, ms_UseTransitionToIdleIntroId);
				}
			}
		}

		if(m_MoveIdleTransitionBlendInWeaponHolding || bWeaponChanged)
		{
			if(m_Flags.IsFlagSet(Flag_SuppressWeaponHoldingForIdleTransition))
			{
				m_Flags.ClearFlag(Flag_SuppressWeaponHoldingForIdleTransition);
				m_Flags.SetFlag(Flag_WeaponClipSetChanged);

				// If we have no valid weapon clip set & have been suppressing the upper body, then we don't have any clips to blend
				// back to. In this case, we need to terminate transition early.
				if(bWeaponChanged && (m_WeaponClipSetId == CLIP_SET_ID_INVALID))
				{
					m_MoveNetworkHelper.SetFlag(false, ms_UseTransitionToIdleIntroId);
				}
			}

			m_MoveIdleTransitionBlendInWeaponHolding = false;
		}
	}

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	const bool bCanChangeState = CanChangeState();
	if(bCanChangeState)
	{
		if(pPed->GetMotionData()->IsCustomIdleClipRequested())
		{
			SetState(State_CustomIdle);
			return FSM_Continue;
		}

		if(pPed->GetMotionData()->GetUseRepositionMoveTarget())
		{
			SetState(State_RepositionMove);
			return FSM_Continue;
		}

		// Do we want to start moving?
		if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY) && pPed->GetMotionData()->GetForcedMotionStateThisFrame()!=CPedMotionStates::MotionState_Idle)
		{
			SetState(State_Start);
			return FSM_Continue;
		}
	}

	// Check for idle turns
	float fDesiredHeading = CalcDesiredDirection();
	float fDesiredHeadingABS = Abs(fDesiredHeading);
	// Don't orient on spot if we are time warping
	if(fDesiredHeadingABS <= MIN_ANGLE && IsClose(fwTimer::GetTimeWarpActive(), 1.f, 0.01f))
	{
		float fEnable = (pPed->GetMovePed().GetTaskNetwork() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)) ? 0.0f : 1.0f;
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange))
		{
			fEnable = 0.0f;
		}
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDesiredHeading*GetTimeStep()*ms_Tunables.IdleHeadingLerpRate*fEnable);
	}
	else
	{
		static dev_float MIN_HEADING_FOR_TURN = 0.001f;
		static dev_float MIN_HEADING_FOR_TURN_TIME_WARP = 0.0025f;
		float fMinHeadingForTurn = IsClose(fwTimer::GetTimeWarpActive(), 1.f, 0.01f) ? MIN_HEADING_FOR_TURN : MIN_HEADING_FOR_TURN_TIME_WARP;
		static dev_float MIN_HEADING_FOR_TURN_DURING_TRANSITION = QUARTER_PI;

		if((fDesiredHeadingABS > fMinHeadingForTurn && (GetUsingAIMotion() || !TransitionToIdleIntroSet())) || fDesiredHeadingABS > MIN_HEADING_FOR_TURN_DURING_TRANSITION)
		{
			m_iIdleTurnCounter++;
			if(m_iIdleTurnCounter >= 2 || GetUsingAIMotion())
			{
				if(bCanChangeState)
				{
					// Idle turn
					SetState(State_IdleTurn);
					return FSM_Continue;
				}
			}
		}
		else
		{
			// Reset the idle counter
			m_iIdleTurnCounter = 0;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdle_OnExit()
{
	ClearTransitionToIdleIntro();
	m_Flags.ClearFlag(Flag_FullyInIdle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdleTurn_OnEnter()
{
	CPed* pPed = GetPed();

	m_MoveNetworkHelper.WaitForTargetState(ms_IdleTurnOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_IdleTurnId);

	// Trigger the post movement call
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);
	m_DoIdleTurnPostMovementUpdate = true;

	// Store the initial directions
	m_fStartDirection = CalcDesiredDirection();
	m_fStartDirectionTarget = m_fStartDirection;
	m_fStartDesiredDirection = pPed->GetMotionData()->GetDesiredHeading();

	// In this case we want to zero the current velocity and has desired velocity flag, otherwise we can wind up doing a walk start by accident
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f);

	// Reset the idle counter
	m_iIdleTurnCounter = 0;
	
	StateIdleTurn_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdleTurn_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	const bool bCanChangeState = CanChangeState();
	if(bCanChangeState)
	{
		// Do we want to start moving?
		if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
		{
			SetState(State_Start);
			return FSM_Continue;
		}
	}

	bool bIsClonePlayer = pPed->IsPlayer() && !pPed->IsLocalPlayer();
	if(!GetUsingAIMotion() && !bIsClonePlayer)
	{
		// If the angle changes too much, restart the idle turn
		float fHeadingDiff = SubtractAngleShorter(m_fStartDesiredDirection, pPed->GetMotionData()->GetDesiredHeading());
		if(Abs(fHeadingDiff) > MIN_ANGLE)
		{
			m_iIdleTurnCounter++;

			if(m_iIdleTurnCounter >= 2)
			{
				if(bCanChangeState)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}
			}
		}
		else
		{
			// Reset the idle counter
			m_iIdleTurnCounter = 0;
		}
	}
	else
	{
		float fHeadingDiff = SubtractAngleShorter(m_fStartDesiredDirection, pPed->GetMotionData()->GetDesiredHeading());
		if(Abs(fHeadingDiff) > MIN_ANGLE)
		{
			// If the angle has changed, try and adjust the blend
			m_fStartDirectionTarget = CalcDesiredDirection();
			m_fStartDesiredDirection = GetPed()->GetMotionData()->GetDesiredHeading();
		}
	}

	if(bCanChangeState)
	{
		// Should we start blending out?
		if(m_MoveBlendOutIdleTurn || (GetUsingAIMotion() && m_MoveCanEarlyOutForMovement))
		{
			// If we've not quite achieved our heading, try again
			float fDesiredHeading = CalcDesiredDirection();
			if(Abs(fDesiredHeading) > MIN_ANGLE)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				// Otherwise we are good
				SetState(State_Idle);
			}
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateIdleTurn_OnExit()
{
	//! Reset - ensure we don't trigger an idle turn if we manage to leave state before this is consumed in ProcessPostMovement.
	m_DoIdleTurnPostMovementUpdate = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStart_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_StartOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_StartMovingId);
	
	// Store the initial directions
	m_fStartDirection = CalcDesiredDirection();
	m_fStartDesiredDirection = GetPed()->GetMotionData()->GetDesiredHeading();

	static dev_float MIN_ANGLE_FOR_ALTERING_PHASE = DtoR * 10.f;
	if(m_fBeginMoveStartPhase > 0.f && Abs(m_fStartDirection) < MIN_ANGLE_FOR_ALTERING_PHASE)
	{
		m_MoveNetworkHelper.SetFloat(ms_BeginMoveStartPhaseId, m_fBeginMoveStartPhase);
	}
	m_fBeginMoveStartPhase = 0.f;

	// Initialise the rate
	m_fMovementRate = GetFloatPropertyFromClip(m_ClipSetId, ms_WalkClipId, "StartRate");

	// Clear flags
	m_Flags.ClearFlag(Flag_Running);
	m_Flags.ClearFlag(Flag_WasRunning);

	StateStart_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStart_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Send the signals
	m_MoveNetworkHelper.SetFloat(ms_StartDirectionId, ConvertRadianToSignal(m_fStartDirection));
	m_MoveNetworkHelper.SetFloat(ms_DesiredSpeedId, pPed->GetMotionData()->GetGaitReducedDesiredMbrY() / MOVEBLENDRATIO_SPRINT);

	if(m_Flags.IsFlagSet(Flag_Running) && !m_Flags.IsFlagSet(Flag_WasRunning))
	{
		m_fMovementRate = GetFloatPropertyFromClip(m_ClipSetId, ms_RunClipId, "StartRate");

		static dev_u32 SKIP_START_TIME = 250;
		const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if(pPlayerInfo && pPlayerInfo->GetTimeExitVehicleTaskFinished() != 0 && (pPlayerInfo->GetTimeExitVehicleTaskFinished() + SKIP_START_TIME) > fwTimer::GetTimeInMilliseconds())
		{
			static dev_float SKIP_ANGLE = 0.4f;
			if(Abs(m_fStartDirection) < SKIP_ANGLE)
			{
				static dev_float SKIP_PHASE = 0.25f;
				m_MoveNetworkHelper.SetFloat(ms_BeginMoveStartPhaseId, SKIP_PHASE);
			}
		}
	}

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = !CanChangeState() || IsTransitionBlocked();
	const bool bInStateForLongEnoughForTagSyncTransition = GetTimeInState() > 0.25f;

	// Check for stopping
	if(!bTransitionBlocked)
	{
		if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() <= square(MBR_WALK_BOUNDARY))
		{
			if(GetTimeInState() < BLEND_TO_IDLE_TIME && !GetUsingAIMotion() && !m_Flags.IsFlagSet(Flag_Running))
			{
				SetState(State_IdleTurn);
				return FSM_Continue;
			}
			else
			{
				const CTaskMoveGoToPointAndStandStill* pGotoTask = GetGotoPointAndStandStillTask(pPed);
				if(pGotoTask)
				{
					if(IsGoingToStopThisFrame(pGotoTask))
					{
						FlipFootFlagsForStopping(pGotoTask);
						SetState(State_Stop);
						return FSM_Continue;
					}
				}
				else if(bInStateForLongEnoughForTagSyncTransition)
				{
					SetState(State_Moving);
					return FSM_Continue;
				}
			}
		}

		if(bInStateForLongEnoughForTagSyncTransition)
		{
			// If in snow - go straight into moving state
			if(m_fSnowDepthSignal > 0.f)
			{
				SetState(State_Moving);
				return FSM_Continue;
			}

			// Check if we can early out if we've changed our mind
			if(m_MoveCanEarlyOutForMovement)
			{
				float fHeadingDiff = SubtractAngleShorter(m_fStartDesiredDirection, pPed->GetMotionData()->GetDesiredHeading());

				static dev_float MIN_START_ANGLE = 0.45f;
				if(Abs(fHeadingDiff) > MIN_START_ANGLE || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
				{
					SetState(State_Moving);
					return FSM_Continue;
				}
				else if(m_Flags.IsFlagSet(Flag_Running) && pPed->GetMotionData()->GetIsLessThanRun(pPed->GetMotionData()->GetGaitReducedDesiredMbrY()))
				{
					SetState(State_Moving);
					return FSM_Continue;
				}
				else if(!m_Flags.IsFlagSet(Flag_Running) && !pPed->GetMotionData()->GetIsWalking(pPed->GetMotionData()->GetGaitReducedDesiredMbrY()))
				{
					SetState(State_Moving);
					return FSM_Continue;
				}
			}

			// Should we start blending out?
			if(m_MoveBlendOutStart)
			{
				SetState(State_Moving);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStart_OnExit()
{
	CPed* pPed = GetPed();

	// Only do if not strafing (has been aborted)
	if(pPed && !pPed->GetMotionData()->GetIsStrafing())
	{
		if(m_Flags.IsFlagSet(Flag_Running))
		{
			// If we are flagged as running, ensure our speed is appropriate
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(Max(MOVEBLENDRATIO_RUN, pPed->GetMotionData()->GetCurrentMbrY()));
		}
		else
		{
			// Otherwise we are walking, ensure our speed is appropriate
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(Max(MOVEBLENDRATIO_WALK, pPed->GetMotionData()->GetCurrentMbrY()));
		}
	}

	// Reset the rate
	m_fMovementRate = 1.f;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateMoving_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_MovingOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_MovingId);

	// Reset the moving direction, this should essentially be 0.f, meaning we are heading forward
	m_fMovingDirection = 0.f;
	m_fMovingDirectionRate = 0.f;
	m_fMovingForwardTimer = 0.f;
	m_fPreviousMovingDirectionDiff = 0.f;

	// Store the initial directions
	float f = CalcDesiredDirection();
	for(s32 i = 0; i < MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES; i++)
	{
		m_fPreviousDesiredDirection[i] = f;
	}

	// Reset the extra heading rate
	m_fExtraHeadingRate = 0.f;

	// Reset the slope direction
	m_fSmoothedSlopeDirection = 0.f;

	// Reset the rate
	m_fMovementRate = 1.f;

	// Initialise the rate
	if(m_Flags.IsFlagSet(Flag_Running))
	{
		m_fMovementTurnRate = GetFloatPropertyFromClip(m_ClipSetId, ms_RunClipId, "TurnRate");
	}
	else
	{
		m_fMovementTurnRate = GetFloatPropertyFromClip(m_ClipSetId, ms_WalkClipId, "TurnRate");
	}

	// Reset the move forward blend weight
	if(m_Flags.IsFlagSet(Flag_BlockGenericFallback))
	{
		m_fGenericMoveForwardBlendWeight = 1.f;
	}
	else
	{
		m_fGenericMoveForwardBlendWeight = 0.f;
	}

	// Check for the upper body from strafe transition anim
	if(m_Flags.IsFlagSet(Flag_FromStrafe))
	{
		InitFromStrafeUpperBodyTransition();
		m_Flags.ClearFlag(Flag_FromStrafe);
	}
	else if(m_fFromStrafeInitialFacingDirection != FLT_MAX)
	{
		// Resend the signal 
		m_MoveNetworkHelper.SetFloat(ms_FromStrafeTransitionUpperBodyDirectionId, ConvertRadianToSignal(m_fFromStrafeTransitionUpperBodyDirection));
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateMoving_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Time step
	float fTimeStep = GetTimeStep();

	// Get the desired direction
	const float fDesiredDirection = CalcDesiredDirection();

	// If we are time slicing, and turning, disable time slicing
	if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
	{
		static dev_float HEADING_DIFF_TOO_BIG = 0.1f;
		if(Abs(fDesiredDirection) > HEADING_DIFF_TOO_BIG)
		{
			// Revert the time step
			fTimeStep = fwTimer::GetTimeStep();

			// Disable time slicing
			pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		}
	}

	StateMoving_MovingDirection(fTimeStep, fDesiredDirection);

	DEV_ONLY(LogMovingVariables());

	// Decide whether to use generic walk or custom walk
	static dev_float FORWARD_DIR = 0.1f;
	if(Abs(m_fMovingDirection) < FORWARD_DIR)
	{
		m_fMovingForwardTimer += fTimeStep;
	}
	else
	{
		m_fMovingForwardTimer = 0.f;
	}

	if(!m_Flags.IsFlagSet(Flag_BlockGenericFallback))
	{
		// If time spent moving forward is small, then stick to using generic forward anim
		static dev_float GENERIC_MOVE_FORWARD_WEIGHT_BLEND_RATE = 1.f / 0.25f;
		static dev_float MOVING_FORWARD_TIME = 0.2f;
		if(m_fMovingForwardTimer < MOVING_FORWARD_TIME)
		{
			Approach(m_fGenericMoveForwardBlendWeight, 0.f, GENERIC_MOVE_FORWARD_WEIGHT_BLEND_RATE, GetTimeStep());
		}
		else
		{
			Approach(m_fGenericMoveForwardBlendWeight, 1.f, GENERIC_MOVE_FORWARD_WEIGHT_BLEND_RATE, GetTimeStep());
		}
	}
	m_MoveNetworkHelper.SetFloat(ms_GenericMoveForwardWeightId, m_fGenericMoveForwardBlendWeight);

	// Send direction signals
	m_MoveNetworkHelper.SetFloat(ms_DesiredDirectionId, ConvertRadianToSignal(fDesiredDirection));
	m_MoveNetworkHelper.SetFloat(ms_DirectionId, ConvertRadianToSignal(m_fMovingDirection));
	
	// Send speed signals
	const float fDesiredSpeed = pPed->GetMotionData()->GetGaitReducedDesiredMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_DesiredSpeedId, fDesiredSpeed);

	// Normalize the speed values between 0.0f and 1.0f
	if(!m_Flags.IsFlagSet(Flag_BlockSpeedChangeWhilstMoving))
	{
		const float fSpeed = pPed->GetMotionData()->GetCurrentMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
		m_MoveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);
	}

	// Slopes/stairs
	StateMoving_SlopesAndStairs(fTimeStep);

	// Snow movement
	StateMoving_Snow();

	// Movement rate
	StateMoving_MovementRate();

	// Upper body from strafe anim
	if(UpdateFromStrafeUpperBodyTransition())
	{
		if(GetTimeInState() >= ms_Tunables.FromStrafe_MovingBlendOutTime)
		{
			m_MoveNetworkHelper.SetFlag(false, ms_UseFromStrafeTransitionUpperBodyId);
			m_fFromStrafeInitialFacingDirection = FLT_MAX;
		}
	}

	bool bInTargetState = m_MoveNetworkHelper.IsInTargetState();

	if(m_Flags.IsFlagSet(Flag_SetInitialMovingPhase) && !bInTargetState)
	{
		m_MoveNetworkHelper.SetFloat(ms_MovingInitialPhaseId, (float)(pPed->GetRandomSeed()/(float)RAND_MAX_16));
	}
	else
	{
		// Clear flag now we are in the correct state, so we don't repeatedly send the signal
		m_Flags.ClearFlag(Flag_SetInitialMovingPhase);
	}

#if __BANK
	if (GetTimeInState() > 2.0f && !bInTargetState)
	{
		AI_LOG_WITH_ARGS("[Human Loco] CTaskHumanLocomotion::StateMoving_OnUpdate() - Ped %s isn't in target state after 2 seconds\n", AILogging::GetDynamicEntityNameSafe(pPed));
		static bool sbHavePrintedVisualisers = false;
		if (!sbHavePrintedVisualisers)
		{
			CAILogManager::PrintDebugInfosForPedToLog(*pPed);
			sbHavePrintedVisualisers = true;
		}
	}
#endif // __BANK

	// Wait for Move network to be in sync
	if(!bInTargetState)
	{
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = !CanChangeState() || IsTransitionBlocked();

	// Check for 180 turns
	if(!bTransitionBlocked && m_fSnowDepthSignal == 0.f)
	{
		if(!GetMotionData().GetUsingStealth() && !pPed->IsUsingActionMode() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_Block180Turns))
		{
			if(GetTimeRunning() > 1.f && GetTimeInState() > 0.2f)
			{
				if(!IsNearZero(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2()) && 
					(Abs(SubtractAngleShorter(fDesiredDirection, m_fPreviousDesiredDirection[MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES-1])) > ms_Tunables.Turn180ActivationAngle) && 
					(Abs(SubtractAngleShorter(fDesiredDirection, m_fPreviousDesiredDirection[0])) < ms_Tunables.Turn180ConsistentAngleTolerance))
				{
					SetState(State_Turn180);
					return FSM_Continue;
				}
			}
		}
	}

	// Check for stopping
	if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() <= square(MBR_WALK_BOUNDARY))
	{
		if(!bTransitionBlocked)
		{
			const CTaskMoveGoToPointAndStandStill* pGotoTask = GetGotoPointAndStandStillTask(pPed);
			if(pGotoTask)
			{
				if(IsGoingToStopThisFrame(pGotoTask))
				{
					FlipFootFlagsForStopping(pGotoTask);
					SetState(State_Stop);
					return FSM_Continue;
				}
			}
			else
			{
				// We have to have been stopping the previous frame as well to stop
				// This helps when doing 180's
				if(m_iMovingStopCount > 0)
				{
					SetState(State_Stop);
					return FSM_Continue;
				}
			}
		}

		++m_iMovingStopCount;
	}
	else
	{
		m_iMovingStopCount = 0;
	}

	// Update the previous direction
	if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0)
	{
		for(s32 i = MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES-1; i > 0; i--)
		{
			m_fPreviousDesiredDirection[i] = m_fPreviousDesiredDirection[i-1];
		}
		m_fPreviousDesiredDirection[0] = fDesiredDirection;
	}
	else
	{
		for(s32 i = 0; i < MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES; i++)
		{
			m_fPreviousDesiredDirection[i] = 0.f;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateMoving_OnExit()
{
	CPed* pPed = GetPed();

	// Reset the rate
	m_fMovementRate = 1.f;
	m_fMovementTurnRate = 1.f;
	// Clear any flags
	m_Flags.ClearFlag(Flag_SetInitialMovingPhase);
	m_Flags.ClearFlag(Flag_BlockSpeedChangeWhilstMoving);
	//
	CleanUpFromStrafeUpperBodyTransition();

	if(pPed && !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl)
		{
			pControl->ClearToggleRun();
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateTurn180_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_Turn180OnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_Turn180Id);

	StateTurn180_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateTurn180_OnUpdate()
{
	bool bCanEarlyOutForMovement = m_MoveCanEarlyOutForMovement;
	m_MoveCanEarlyOutForMovement = false;

	// Cache the ped
	CPed* pPed = GetPed();
	
	// Normalize the speed values between 0.0f and 1.0f
	float fSpeed = pPed->GetMotionData()->GetCurrentMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = !CanChangeState() || IsTransitionBlocked();

	if(!bTransitionBlocked)
	{
		// Check for 180 turns
		if(bCanEarlyOutForMovement && m_fSnowDepthSignal == 0.f)
		{
			if(!GetMotionData().GetUsingStealth() && !pPed->IsUsingActionMode())
			{
				const float fDesiredHeading = pPed->GetDesiredHeading();
				const float fCurrentHeading = pPed->GetCurrentHeading();

				if(!IsNearZero(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2()) && 
					Abs(SubtractAngleShorter(fDesiredHeading, fCurrentHeading)) > ms_Tunables.Turn180ActivationAngle)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}
			}
		}

		// Check for blending out
		if(m_MoveBlendOut180)
		{
			SetState(State_Moving);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateTurn180_OnExit()
{
	if(!GetIsFlagSet(aiTaskFlags::InMakeAbortable))
	{
		if(m_Flags.IsFlagSet(Flag_Running))
		{
			// If we are flagged as running, ensure our speed is appropriate
			GetPed()->GetMotionData()->SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
		}
		else
		{
			// Otherwise we are walking, ensure our speed is appropriate
			GetPed()->GetMotionData()->SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStop_OnEnter()
{
	bool bSupportsSuddenStops = CheckClipSetForMoveFlag(m_ClipSetId, ms_UseSuddenStopsId) || (!m_Flags.IsFlagSet(Flag_BlockGenericFallback) && CheckClipSetForMoveFlag(m_GenericMovementClipSetId, ms_UseSuddenStopsId));
	if(bSupportsSuddenStops)
	{
		// Prevent sudden stops if we use stop exactly yes
		const CTaskMoveGoToPointAndStandStill* pGotoTask = GetGotoPointAndStandStillTask(GetPed());
		if(pGotoTask && pGotoTask->GetStopExactly())
		{
			m_MoveNetworkHelper.SetFlag(false, ms_UseSuddenStopsId);
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(true, ms_UseSuddenStopsId);
		}
	}

	m_MoveNetworkHelper.WaitForTargetState(ms_StopOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_StopMovingId);

	// Store the initial directions
	m_fStopDirection = CalcDesiredDirection();

	if(m_Flags.IsFlagSet(Flag_Running))
	{
		// If we are running, we have no orientated stop
		if(GetUsingAIMotion())
		{
			m_fStartDesiredDirection = GetPed()->GetMotionData()->GetCurrentHeading();
		}
		else
		{
			m_fStartDesiredDirection = GetPed()->GetMotionData()->GetDesiredHeading();
			
			if(m_Flags.IsFlagSet(Flag_LastFootLeft))
			{
				m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
			}
			else
			{
				m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
			}
			m_Flags.SetFlag(Flag_FlipLastFootLeft);
		}

		// Init the rate
		m_fMovementRate = GetFloatPropertyFromClip(m_ClipSetId, ms_RunClipId, "StopRate");
	}
	else
	{
		// Store desired heading, to see if we want to abort our orientated stop
		m_fStartDesiredDirection = GetPed()->GetMotionData()->GetDesiredHeading();
		m_Flags.SetFlag(Flag_OrientatedStop);

		// Init the rate
		m_fMovementRate = GetFloatPropertyFromClip(m_ClipSetId, ms_WalkClipId, "StopRate");
	}

	// Set whether we came from the start state
	m_MoveNetworkHelper.SetFlag(GetPreviousState() == State_Start, ms_ForceSuddenStopId);

	if(IsGaitReduced() || m_Flags.IsFlagSet(Flag_PreferSuddenStops))
	{
		m_MoveNetworkHelper.SetFlag(true, ms_ForceSuddenStopId);
	}

	// Clear flag
	m_Flags.ClearFlag(Flag_FullyInStop);

	StateStop_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStop_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Send the signals
	m_MoveNetworkHelper.SetFloat(ms_StopDirectionId, ConvertRadianToSignal(m_fStopDirection));

	// Normalize the speed values between 0.0f and 1.0f
	float fSpeed = pPed->GetMotionData()->GetCurrentMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);

	// Check if we have fully transitioned
	if(m_MoveStartOnExit || m_MoveMovingOnExit)
	{
		m_Flags.SetFlag(Flag_FullyInStop);
		m_MoveStartOnExit = false;
		m_MoveMovingOnExit = false;
	}

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = !CanChangeState() || IsTransitionBlocked();
	if(!bTransitionBlocked)
	{
		// If in snow - don't do stops
		if(m_fSnowDepthSignal > 0.f)
		{
			if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
			{
				// If we have speed, go straight to start
				SetState(State_Start);
				return FSM_Continue;
			}
			else
			{
				SetState(State_Idle);
				return FSM_Continue;
			}
		}

		bool bStopExactly = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping);
		bool bCanStartIdle = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling);

		// Have we changed our minds?
		if(m_MoveCanEarlyOutForMovement)
		{
			if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
			{
				// If we have speed, go straight to start
				SetState(State_Start);
				return FSM_Continue;
			}
			else
			{
				// If we have heading change, go into idle (then idle turn)
				float fHeadingDiff = SubtractAngleShorter(m_fStartDesiredDirection, pPed->GetMotionData()->GetDesiredHeading());
				if(Abs(fHeadingDiff) > MIN_ANGLE)
				{
					// Don't do this when we do perfect walkstops during the walkstop
					// Only if we failed to nail desired heading by normal means
					if(!bStopExactly || bCanStartIdle || (!m_Flags.IsFlagSet(Flag_OrientatedStop) && !m_Flags.IsFlagSet(Flag_Running)))
					{
						SetState(State_Idle);
						return FSM_Continue; 
					}
				}
			}
		}
		else if(m_MoveCanResumeMoving)
		{
			if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
			{
				SetState(State_Moving);
				return FSM_Continue;
			}
		}

		// Should we blend out?
		if(m_MoveBlendOutStop)
		{
			SetState(State_Idle);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateStop_OnExit()
{
	// Reset the rate
	m_fMovementRate = 1.f;

	// Clear the move start override heading, incase it was set while we were stopping
	m_fBeginMoveStartPhase = 0.f;

	// Clear flags
	m_Flags.ClearFlag(Flag_FullyInStop);
	m_Flags.ClearFlag(Flag_OrientatedStop);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateRepositionMove_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_RepositionMoveOnEnterId);
	m_MoveNetworkHelper.SendRequest(ms_RepositionMoveId);

	CTaskRepositionMove* pTask = rage_new CTaskRepositionMove();
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_RepositionMoveOnEnterId, ms_RepositionMoveNetworkId);
	SetNewTask(pTask);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateRepositionMove_OnUpdate()
{
	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Idle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateFromStrafe_OnEnter()
{
	CPed* pPed = GetPed();

	// Start the subtask
	CTaskMotionAimingTransition* pTask = rage_new CTaskMotionAimingTransition(CClipNetworkMoveInfo::ms_NetworkTaskMotionAimingToOnFootTransition, pPed->GetPedModelInfo()->GetMovementToStrafeClipSet(), m_Flags.IsFlagSet(Flag_UseLeftFootStrafeTransition), true, false, m_fInitialMovingDirection);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_FromStrafeOnEnterId, ms_FromStrafeNetworkId);
	SetNewTask(pTask);

	// Check for the upper body transition anim
	InitFromStrafeUpperBodyTransition();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateFromStrafe_OnUpdate()
{
	CPed* pPed = GetPed();

	// Get the desired direction
	const float fDesiredDirection = CalcDesiredDirection();

	// Send direction signals
	m_MoveNetworkHelper.SetFloat(ms_DesiredDirectionId, ConvertRadianToSignal(fDesiredDirection));
	m_MoveNetworkHelper.SetFloat(ms_DirectionId, ConvertRadianToSignal(m_fMovingDirection));

	// Upper body anim
	UpdateFromStrafeUpperBodyTransition();
	
	if(!pPed->GetMotionData()->GetIsLessThanRun())
	{
		m_Flags.SetFlag(Flag_Running);
	}
	else
	{
		m_Flags.ClearFlag(Flag_Running);
	}

	// Normalize the speed values between 0.0f and 1.0f
	const float fSpeed = pPed->GetMotionData()->GetCurrentMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);

	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Once transition finished, continue to moving
		SetState(State_Moving);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateFromStrafe_OnExit()
{
	bool bAborted = false;
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
	{
		bAborted = static_cast<CTaskMotionAimingTransition*>(GetSubTask())->HasQuitDueToInput();
	}

	if(!bAborted)
	{
		CleanUpFromStrafeUpperBodyTransition();
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateCustomIdle_OnEnter()
{
	m_Flags.ClearFlag(Flag_CustomIdleLoaded);

	StateCustomIdle_InitMoveSignals();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateCustomIdle_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();
	fwMvClipSetId customSet;	
	fwMvClipId customClip;
	pPed->GetMotionData()->GetCustomIdleClip(customSet, customClip);
	if(!m_Flags.IsFlagSet(Flag_CustomIdleLoaded) && m_CustomIdleClipSetRequestHelper.Request(customSet))
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(customSet, customClip);
		if(pClip)
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_CustomIdleClipId);
			m_MoveNetworkHelper.WaitForTargetState(ms_CustomIdleOnEnterId);
			m_MoveNetworkHelper.SendRequest(ms_CustomIdleId);
			m_Flags.SetFlag(Flag_CustomIdleLoaded);
		}
	}

	// Wait for Move network to be in sync
	if(m_Flags.IsFlagSet(Flag_CustomIdleLoaded) && !m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}


	if(m_MoveCustomIdleFinished)
	{
		SetState(State_Idle);
		return FSM_Continue;
	}

	if(pPed->GetMotionData()->GetUseRepositionMoveTarget())
	{
		SetState(State_RepositionMove);
		return FSM_Continue;
	}

	// Do we want to start moving?
	if(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	// Check for idle turns
	TUNE_GROUP_FLOAT(PED_MOVEMENT, ms_fIdleHeadingLerpRate, 7.5f, 0.0f, 20.0f, 0.001f);
	float fDesiredHeading = CalcDesiredDirection();
	if(Abs(fDesiredHeading) <= MIN_ANGLE)
	{
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDesiredHeading*GetTimeStep()*ms_fIdleHeadingLerpRate*((pPed->GetMovePed().GetTaskNetwork() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)) ? 0.0f : 1.0f));
	}
	else
	{
		if(Abs(fDesiredHeading) > 0.f)
		{
			m_iIdleTurnCounter++;
			if(m_iIdleTurnCounter >= 2 || GetUsingAIMotion())
			{
				// Idle turn
				SetState(State_IdleTurn);
				return FSM_Continue;
			}
		}
		else
		{
			// Reset the idle counter
			m_iIdleTurnCounter = 0;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHumanLocomotion::FSM_Return CTaskHumanLocomotion::StateCustomIdle_OnExit()
{
	if(m_Flags.IsFlagSet(Flag_CustomIdleLoaded))
	{
		m_CustomIdleClipSetRequestHelper.Release();
	}
	GetPed()->GetMotionData()->ResetCustomIdleClipSet();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateIdle_InitMoveSignals()
{
	m_MoveIdleStandardEnter = m_MoveNetworkHelper.GetBoolean(ms_IdleStandardEnterId);
	m_MoveIdleTransitionFinished = m_MoveNetworkHelper.GetBoolean(ms_IdleTransitionFinishedId);
	m_MoveIdleTransitionBlendInWeaponHolding = m_MoveNetworkHelper.GetBoolean(ms_IdleTransitionBlendInWeaponHoldingId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateIdle_OnProcessMoveSignals()
{
	m_MoveIdleStandardEnter |= m_MoveNetworkHelper.GetBoolean(ms_IdleStandardEnterId);
	m_MoveIdleTransitionFinished |= m_MoveNetworkHelper.GetBoolean(ms_IdleTransitionFinishedId);
	m_MoveIdleTransitionBlendInWeaponHolding |= m_MoveNetworkHelper.GetBoolean(ms_IdleTransitionBlendInWeaponHoldingId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateIdleTurn_InitMoveSignals()
{
	SendIdleTurnSignals();

	m_MoveBlendOutIdleTurn = m_MoveNetworkHelper.GetBoolean(ms_BlendOutIdleTurnId);
	m_MoveCanEarlyOutForMovement = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateIdleTurn_OnProcessMoveSignals()
{
	ComputeIdleTurnSignals();
	SendIdleTurnSignals();

	m_MoveBlendOutIdleTurn |= m_MoveNetworkHelper.GetBoolean(ms_BlendOutIdleTurnId);
	m_MoveCanEarlyOutForMovement |= m_MoveNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateStart_InitMoveSignals()
{
	m_MoveBlendOutStart = m_MoveNetworkHelper.GetBoolean(ms_BlendOutStartId);
	m_MoveCanEarlyOutForMovement = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateStart_OnProcessMoveSignals()
{
	if(m_MoveNetworkHelper.GetBoolean(ms_StartLeftId))
	{
		m_Flags.ClearFlag(Flag_LastFootLeft);
		m_Flags.SetFlag(Flag_LastFootRightThisFrame);
		m_Flags.ClearFlag(Flag_FlipLastFootLeft);
		m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
	}

	if(m_MoveNetworkHelper.GetBoolean(ms_StartRightId))
	{
		m_Flags.SetFlag(Flag_LastFootLeft);
		m_Flags.SetFlag(Flag_LastFootLeftThisFrame);
		m_Flags.ClearFlag(Flag_FlipLastFootLeft);
		m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
	}

	m_MoveBlendOutStart |= m_MoveNetworkHelper.GetBoolean(ms_BlendOutStartId);
	m_MoveCanEarlyOutForMovement |= m_MoveNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateTurn180_InitMoveSignals()
{
	m_MoveBlendOut180 = false;
	m_MoveCanEarlyOutForMovement = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateTurn180_OnProcessMoveSignals()
{
	m_MoveBlendOut180 |= m_MoveNetworkHelper.GetBoolean(ms_BlendOut180Id);
	m_MoveCanEarlyOutForMovement |= m_MoveNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateStop_InitMoveSignals()
{
	m_MoveStartOnExit = m_MoveNetworkHelper.GetBoolean(ms_StartOnExitId);
	m_MoveMovingOnExit = m_MoveNetworkHelper.GetBoolean(ms_MovingOnExitId);
	m_MoveCanResumeMoving = m_MoveNetworkHelper.GetBoolean(ms_CanResumeMovingId);
	m_MoveBlendOutStop = m_MoveNetworkHelper.GetBoolean(ms_BlendOutStopId);
	m_MoveCanEarlyOutForMovement = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateStop_OnProcessMoveSignals()
{
	m_MoveStartOnExit |= m_MoveNetworkHelper.GetBoolean(ms_StartOnExitId);
	m_MoveMovingOnExit |= m_MoveNetworkHelper.GetBoolean(ms_MovingOnExitId);
	m_MoveCanResumeMoving |= m_MoveNetworkHelper.GetBoolean(ms_CanResumeMovingId);
	m_MoveBlendOutStop |= m_MoveNetworkHelper.GetBoolean(ms_BlendOutStopId);
	m_MoveCanEarlyOutForMovement |= m_MoveNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateCustomIdle_InitMoveSignals()
{
	m_MoveCustomIdleFinished = m_MoveNetworkHelper.GetBoolean(ms_CustomIdleFinishedId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateCustomIdle_OnProcessMoveSignals()
{
	m_MoveCustomIdleFinished |= m_MoveNetworkHelper.GetBoolean(ms_CustomIdleFinishedId);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SendMovementClipSet()
{
	m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
	m_MoveNetworkHelper.SetClipSet(m_GenericMovementClipSetId, ms_GenericMovementClipSetVarId);

	// Do some basic verifications on the clips
	ASSERT_ONLY(VerifyMovementClipSet(m_ClipSetId, m_Flags, m_GenericMovementClipSetId, GetEntity() ? GetPed() : NULL));

	// Don't do this again
	m_Flags.ClearFlag(Flag_MovementClipSetChanged);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SendWeaponClipSet()
{
	m_MoveNetworkHelper.SetClipSet(m_WeaponClipSetId, ms_WeaponHoldingClipSetVarId);

	// Do some basic verifications on the clips
	ASSERT_ONLY(VerifyWeaponClipSet());

	if(m_Flags.IsFlagSet(Flag_InstantBlendWeaponClipSet))
	{
		m_MoveNetworkHelper.SetFloat(ms_WeaponHoldingBlendDurationId, INSTANT_BLEND_DURATION);
		m_Flags.ClearFlag(Flag_InstantBlendWeaponClipSet);
	}

	bool bUseWeaponHolding = m_WeaponClipSetId != CLIP_SET_ID_INVALID && !IsSuppressingWeaponHolding();

	m_MoveNetworkHelper.SetFlag(bUseWeaponHolding, ms_UseWeaponHoldingId);
	m_MoveNetworkHelper.SendRequest(ms_RestartWeaponHoldingId);

	// Don't do this again
	m_Flags.ClearFlag(Flag_WeaponClipSetChanged);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StoreWeaponFilter()
{
	// Clear any old filter
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}

	if(m_WeaponFilterId != FILTER_ID_INVALID)
	{
		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(m_WeaponFilterId.GetHash(), GetPed());
		if(taskVerifyf(m_pWeaponFilter, "Failed to get filter [%s]", m_WeaponFilterId.GetCStr()))	
		{
			m_pWeaponFilter->AddRef();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SendAlternateParams(AlternateClipType type)
{
	struct sParams
	{
		sParams(const fwMvClipId& alternateClipId, const fwMvBooleanId& alternateLoopedId, const fwMvFloatId& alternateTransitionBlendId, const fwMvFlagId& useAlternateId)
			: alternateClipId(alternateClipId)
			, alternateLoopedId(alternateLoopedId)
			, alternateTransitionBlendId(alternateTransitionBlendId)
			, useAlternateId(useAlternateId)
		{
		}

		fwMvClipId alternateClipId;
		fwMvBooleanId alternateLoopedId;
		fwMvFloatId alternateTransitionBlendId;
		fwMvFlagId useAlternateId;
	};

	static const sParams s_Params[ACT_MAX] = 
	{
		sParams(fwMvClipId("AlternateIdle_Clip",0x8E8B6E76),		fwMvBooleanId("AlternateIdle_Looped",0x9DDC2F27),		fwMvFloatId("AlternateIdle_TransitionBlend",0x88F4620B),   fwMvFlagId("UseAlternateIdle",0x8120358)),
		sParams(fwMvClipId("AlternateWalk_Clip",0x3A9CB752),		fwMvBooleanId("AlternateWalk_Looped",0x328321A9),		fwMvFloatId("AlternateWalk_TransitionBlend",0x5D30FA91),   fwMvFlagId("UseAlternateWalk",0xDE7A9B0D)),
		sParams(fwMvClipId("AlternateRun_Clip",0x8A2EF7F),		fwMvBooleanId("AlternateRun_Looped",0xE6A860AB),		fwMvFloatId("AlternateRun_TransitionBlend",0x7632B5E9),    fwMvFlagId("UseAlternateRun",0x250022C6)),
		sParams(fwMvClipId("AlternateSprint_Clip",0x5AB0D8F5),	fwMvBooleanId("AlternateSprint_Looped",0x1D91B6F2),	fwMvFloatId("AlternateSprint_TransitionBlend",0xE04FB0CE), fwMvFlagId("UseAlternateSprint",0xA9D402F4)),
	};

	bool bSendParams = false;
	switch(GetState())
	{
	case State_Initial:
	case State_RepositionMove:
	case State_FromStrafe:
	case State_CustomIdle:
		break;
	case State_Idle:
	case State_IdleTurn:
		bSendParams = type == ACT_Idle;
		break;
	case State_Start:
	case State_Moving:
	case State_Turn180:
		bSendParams = type == ACT_Walk || type == ACT_Run || (NetworkInterface::IsInCopsAndCrooks() && type == ACT_Sprint);	// CNC: enable alternate sprint clip (looks like a general bug here but too risky to change globally!)
		break;
	case State_Stop:
		bSendParams = type == ACT_Idle;
		break;
	default:
		taskAssertf(0, "Unhandled state %s", GetStateName(GetState()));
		break;
	}

	const crClip* pClip = NULL;
	if(bSendParams && m_alternateClips[type].IsValid())
	{
		pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_alternateClips[type].iDictionaryIndex, m_alternateClips[type].uClipHash.GetHash());
	}

	if(pClip)
	{
		m_MoveNetworkHelper.SetClip(pClip, s_Params[type].alternateClipId);
		if(s_Params[type].alternateLoopedId.IsNotNull())
		{
			m_MoveNetworkHelper.SetBoolean(s_Params[type].alternateLoopedId, GetAlternateClipLooping(type));
		}
		m_MoveNetworkHelper.SetFloat(s_Params[type].alternateTransitionBlendId, m_alternateClips[type].fBlendDuration);
		m_MoveNetworkHelper.SetFlag(true, s_Params[type].useAlternateId);
	}
	else
	{
		m_MoveNetworkHelper.SetClip(NULL, s_Params[type].alternateClipId);
		m_MoveNetworkHelper.SetFlag(false, s_Params[type].useAlternateId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ProcessMovement()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(!pPed->GetMotionData()->GetIsStrafing())
	{
		// Process snow depth
		ProcessSnowDepth(pPed, GetTimeStep(), m_fSmoothedSnowDepth, m_fSnowDepthSignal);

		// Process slopes/stairs
		ProcessSlopesAndStairs(pPed, m_uLastSlopeScanTime, m_uStairsDetectedTime, m_vLastSlopePosition);

		//******************************************************************************************
		// This is the work which was done inside CPedMoveBlendOnFoot to model player/ped movement

		const bool bIsPlayer = !GetUsingAIMotion();

		const float fMBRMod = GetState() == State_FromStrafe ? ms_Tunables.FromStrafeAccelerationMod : 1.f;

		// Interpolate the current moveblendratio towards the desired
		float fAccel, fDecel;
#if FPS_MODE_SUPPORTED
		const bool bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		if(bInFpsMode)
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_LocoMBRAccel, 1000.f, 0.f, 1000.f, 0.01f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_LocoMBRDecel, 1000.f, 0.f, 1000.f, 0.01f);
			fAccel = FPS_LocoMBRAccel;
			fDecel = FPS_LocoMBRDecel;
		}
		else
#endif // FPS_MODE_SUPPORTED
		if(bIsPlayer)
		{
			fAccel = ms_Tunables.Player_MBRAcceleration * fMBRMod;
			fDecel = ms_Tunables.Player_MBRDeceleration * fMBRMod;
		}
		else
		{
			fAccel = ms_Tunables.AI_MBRAcceleration * fMBRMod;
			fDecel = ms_Tunables.AI_MBRDeceleration * fMBRMod;
		}

		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();

		//******************************************************************************************

		// Should the local player slow down when running towards a ladder to prevent him flying over the edge and killing himself?
		if((bIsPlayer) && NetworkInterface::IsGameInProgress())
		{
			ProcessMPLadders(vCurrentMBR, vDesiredMBR);
		}

		//******************************************************************************************

		// Stealth
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);

			if(pPed->GetPlayerInfo())
			{
				m_fStealthMovementRate = pPed->GetPlayerInfo()->GetStealthRate();
			}
		}

		// Slopes
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected) || m_fSnowDepthSignal > 0.f || UsingHeavyWeapon())
		{
			// Clamp to run on slopes/snow
			vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);
		}

		// In Water
		if(pPed->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
		{
			if(pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
			{
				static dev_float SUBMERGED_LEVEL_TO_SLOW = 0.2f;
				if(pPed->m_Buoyancy.GetSubmergedLevel() > SUBMERGED_LEVEL_TO_SLOW)
				{
					// Clamp to run in water
					vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);

					// Store time
					m_uSlowDownInWaterTime = fwTimer::GetTimeInMilliseconds();
				}
			}
		}

		static dev_u32 MIN_SLOW_DOWN_TIME = 250;
		if((m_uSlowDownInWaterTime + MIN_SLOW_DOWN_TIME) > fwTimer::GetTimeInMilliseconds())
		{
			// Clamp to run if we have been in water
			vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);
		}

		const float fTimeStep = GetTimeStep();

		// Foliage
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage))
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fFoliageMovementRateInFoliage, 0.85f,  0.1f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fFoliageMovementRateInFoliageRate, 0.05f,  0.0f, 10.0f, 0.01f);
			Approach(m_fFoliageMovementRate, fFoliageMovementRateInFoliage, fFoliageMovementRateInFoliageRate, fTimeStep);

			// Clamp to run in foliage
			vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);
		}
		else if(!pPed->GetCurrentPhysicsInst() || !pPed->GetCurrentPhysicsInst()->IsInLevel() || CPhysics::GetLevel()->IsActive(pPed->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fFoliageMovementRateNormalRate, 0.6f,  0.0f, 10.0f, 0.01f);
			Approach(m_fFoliageMovementRate, 1.0f, fFoliageMovementRateNormalRate, fTimeStep);
		}

#if FPS_MODE_SUPPORTED
		if(bInFpsMode && pPed->GetIsInInterior() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) && !pPed->GetPortalTracker()->IsAllowedToRunInInterior())
		{
			// Clamp to run in interiors
			vDesiredMBR.y = Min(vDesiredMBR.y, MOVEBLENDRATIO_RUN);
		}
#endif // FPS_MODE_SUPPORTED

		// No lateral movement
		vCurrentMBR.x = 0.0f;

		if(vDesiredMBR.y > vCurrentMBR.y)
		{
			vCurrentMBR.y += fAccel * fTimeStep;
			vCurrentMBR.y  = Min(vCurrentMBR.y, Min(vDesiredMBR.y, MOVEBLENDRATIO_SPRINT));
		}
		else if(vDesiredMBR.y < vCurrentMBR.y)
		{
			vCurrentMBR.y -= fDecel * fTimeStep;
			vCurrentMBR.y  = Max(vCurrentMBR.y, Max(vDesiredMBR.y, MOVEBLENDRATIO_STILL));
		}
		else
		{
			vCurrentMBR.y = vDesiredMBR.y;
		}

		// Don't go backwards
		vCurrentMBR.y = Max(0.f, vCurrentMBR.y);

		// Give the animated velocity change as the current turn velocity
		float fCurrentTurnVelocity = pPed->GetAnimatedAngularVelocity();

		// B*228237: Ignore scripted MBR clamp if running TASK_MOVE_STAND_STILL.
		bool bIgnoreScriptedMBR = false;
		CTask *pMoveTask = pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetGeneralMovementTask() : NULL;
		if (pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_STAND_STILL)
		{
			bIgnoreScriptedMBR = true;
		}

		// Copy variables back into motion data.  This is a temporary measure.
		pPed->GetMotionData()->SetCurrentMoveBlendRatio(vCurrentMBR.y, vCurrentMBR.x, bIgnoreScriptedMBR);
		pPed->GetMotionData()->SetCurrentTurnVelocity(fCurrentTurnVelocity);

		// Speed override lerp
		TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplier, 2.0, 1.0f, 20.0f, 0.5f);
		float fDeltaOverride = pPed->GetMotionData()->GetDesiredMoveRateOverride() - pPed->GetMotionData()->GetCurrentMoveRateOverride();
		fDeltaOverride *= (fTendToDesiredMoveRateOverrideMultiplier * fTimeStep);
		fDeltaOverride += pPed->GetMotionData()->GetCurrentMoveRateOverride();
		pPed->GetMotionData()->SetCurrentMoveRateOverride(fDeltaOverride);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ProcessSnowDepth(const CPed* pPed, const float fTimeStep, float& fSmoothedSnowDepth, float& fSnowDepthSignal)
{
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMinSnowDepth, 0.05f, 0.0f, PI, 0.001f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMaxSnowDepth, 0.3f, 0.0f, PI, 0.001f);

	float fSnowDepth = pPed->GetDeepSurfaceInfo().GetDepth();

#if __DEV
	static dev_float SNOW_DEPTH = 0.0f;
	if(SNOW_DEPTH > 0.f)
		fSnowDepth = SNOW_DEPTH;
#endif // __DEV

	bool bInHackedArea = false;
	static const spdAABB shoreline(Vec3V(2991.3f, 2030.5f, -2.1f), Vec3V(3068.4f, 2129.3f, 26.6f));
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	if (shoreline.ContainsPoint(vPedPos))
	{
		bInHackedArea = true;
	}

	if((fSnowDepth < fMinSnowDepth) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableDeepSurfaceAnims) || bInHackedArea)
	{
		fSmoothedSnowDepth = 0.f;
		fSnowDepthSignal = 0.f;
		return;
	}

	// Clamp
	fSnowDepth = Min(fSnowDepth, fMaxSnowDepth);

	TUNE_GROUP_FLOAT(PED_MOVEMENT, fLocomotionSnowDepthSmoothing, 6.0f, 0.0f, 20.0f, 0.001f);
	Approach(fSmoothedSnowDepth, fSnowDepth, fLocomotionSnowDepthSmoothing, fTimeStep);

	// Compute a 0 to 1 signal
	fSnowDepthSignal = ((fSmoothedSnowDepth - fMinSnowDepth) / (fMaxSnowDepth - fMinSnowDepth)) + fMinSnowDepth;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ProcessSlopesAndStairs(CPed* pPed, u32& uLastSlopeScanTime, u32& uStairsDetectedTime, Vector3& vLastSlopePosition, const bool bIgnoreStrafeCheck)
{
	PF_FUNC(ProcessSlopesAndStairs);

	TUNE_GROUP_BOOL(PED_MOVEMENT, bSlopeScanEnable, true);
	TUNE_GROUP_BOOL(PED_MOVEMENT, bDebugSlopeScan, false);
	TUNE_GROUP_BOOL(PED_MOVEMENT, bSlopeForceNonProbeMethod, false);
	TUNE_GROUP_INT(PED_MOVEMENT, iSlopeScanFreqMs, 0, 0, 1000, 1);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeBlockingProbeZOffset, -0.6f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeScanStartZOffset, 0.35f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeScanZOffset, -3.0f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeScanAheadMod, 0.15f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeSecondProbeDist, 0.55f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeNormal, 0.975f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeNormalRejection, 0.3f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeXYDelta, 0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMidPointTestSlopeDot, 0.965f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMidPointTestNormalDiff, 0.175f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMidPointTestMaxSlopeDiff, 0.15f, 0.0f, 1.0f, 0.01f);

	bool bWasOnStair = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected);
	if(!bSlopeScanEnable || (pPed->IsStrafing() && !bIgnoreStrafeCheck) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing) || pPed->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_STILL)
	{
		// Use the ground normal
		pPed->SetSlopeNormal(pPed->GetGroundNormal());
	}
	else
	{
		//***************************************************************************************
		//	If this is the player, we scan ahead to see if we are about to run onto some slope/stairs.
		if((pPed->IsControlledByLocalPlayer() || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseProbeSlopeStairsDetection)) && !bSlopeForceNonProbeMethod)
		{
			PF_FUNC(ProcessSlopesAndStairs);

			const u32 uDeltaTime = fwTimer::GetTimeInMilliseconds() - uLastSlopeScanTime;
			if(uDeltaTime > iSlopeScanFreqMs)
			{
				uLastSlopeScanTime = fwTimer::GetTimeInMilliseconds();

				// Cache whether an edge has already been detected
				const bool bEdgeDetected = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_EdgeDetected);

				// Reset the variables
				ResetSlopesAndStairs(pPed, vLastSlopePosition);

				// Forward vector
				const Vector3& vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

				// Velocity
				float fXYVel = pPed->GetVelocity().XYMag();
				fXYVel = Max(1.f, fXYVel);

				bool bRejected = false;
				WorldProbe::CShapeTestHitPoint probeIsect;
				WorldProbe::CShapeTestResults probeResult(probeIsect);
				WorldProbe::CShapeTestProbeDesc probeDesc;

				Vector3 vSlopeNormal(pPed->GetGroundNormal());
				bool bSlope = false;
				bool bStairs = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs);
				if(!bStairs)
				{
					// Do a probe to see if we are blocked
					Vector3 vTestStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					vTestStart.z += fSlopeBlockingProbeZOffset;

					Vector3 vTestEnd = vTestStart;
					vTestEnd += vForward * ((fXYVel * fSlopeScanAheadMod) + fSlopeSecondProbeDist);
					DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Line(vTestStart, vTestEnd, Color_yellow, Color_yellow, -1));

					probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
					probeDesc.SetResultsStructure(&probeResult);
					probeDesc.SetExcludeEntity(pPed);
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);
					probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
					{
						if(!CPed::IsStairs(&probeResult[0], NULL, 0, 0) && Abs(probeResult[0].GetHitNormal().z) < fSlopeNormalRejection)
						{
							bRejected = true; 
							DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Sphere(probeResult[0].GetHitPosition(), 0.05f, Color_red, true, -1));
						}
					}
				}

				if(!bRejected)
				{
					Vector3 vTestStart1 = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					vTestStart1 += vForward * (fXYVel * fSlopeScanAheadMod);
					vTestStart1.z += fSlopeScanStartZOffset;

					Vector3 vTestEnd1 = vTestStart1;
					vTestEnd1.z += fSlopeScanZOffset;
					DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Line(vTestStart1, vTestEnd1, Color_yellow, Color_yellow, -1));

					// Perform probe
					probeDesc.SetStartAndEnd(vTestStart1, vTestEnd1);
					probeDesc.SetResultsStructure(&probeResult);
					probeDesc.SetExcludeEntity(pPed);
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);
					probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
					{
						Vector3 vHitPos1(probeResult[0].GetHitPosition());
						Vector3 vHitNormal1(probeResult[0].GetHitPolyNormal());
						DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Sphere(vHitPos1, 0.05f, Color_green, true, -1));

						Vector3 vNextProbeOffset = vForward * fSlopeSecondProbeDist;
						Vector3 vTestStart2 = vTestStart1 + vNextProbeOffset;
						Vector3 vTestEnd2 = vTestEnd1 + vNextProbeOffset;
						DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Line(vTestStart2, vTestEnd2, Color_yellow, Color_yellow, -1));

						probeDesc.SetStartAndEnd(vTestStart2, vTestEnd2);
						probeDesc.SetResultsStructure(&probeResult);
						probeDesc.SetExcludeEntity(pPed);
						probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);
						probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
						if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
						{
							Vector3 vHitPos2(probeResult[0].GetHitPosition());
							Vector3 vHitNormal2(probeResult[0].GetHitPolyNormal());
							DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Sphere(vHitPos2, 0.05f, Color_green, true, -1));

							Vector3 vDiff(vHitPos2 - vHitPos1);

							if(vDiff.Mag2() > 0.f)
							{
								Vector3 vSlopeNormalAvg;

								vDiff.NormalizeFast();

								Vector3 vTempRight;
								vTempRight.Cross(vDiff, ZAXIS);
								vSlopeNormal.Cross(vTempRight, vDiff);
								vSlopeNormal.NormalizeFast();

								bStairs = bStairs || CPed::IsStairs(&probeResult[0], NULL, 0, 0);

								bSlope = vSlopeNormal.GetZ() < fSlopeNormal;
								bool bFoundEdge = false;
								if(bSlope && !bStairs && abs(vSlopeNormal.z) < fMidPointTestSlopeDot)
								{
									//! If our normals, don't represent slope, do an extra test.
									vSlopeNormalAvg = (vHitNormal1 + vHitNormal2)*0.5f;

									if((Abs(vSlopeNormalAvg.z - vSlopeNormal.z) > fMidPointTestNormalDiff) || ((vHitNormal1.z > fSlopeNormal) && (vHitNormal2.z > fSlopeNormal)))
									{
										Vector3 vMidOffset = vForward * (fSlopeSecondProbeDist * 0.5f);
										Vector3 vTestStartMid = vTestStart1 + vMidOffset;
										Vector3 vTestEndMid = vTestEnd1 + vMidOffset;

										DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Line(vTestStart2, vTestEnd2, Color_yellow, Color_yellow, -1));

										probeDesc.SetStartAndEnd(vTestStartMid, vTestEndMid);
										probeDesc.SetResultsStructure(&probeResult);
										probeDesc.SetExcludeEntity(pPed);
										probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);
										probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
										if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
										{
											Vector3 vHitPosMid(probeResult[0].GetHitPosition());
											Vector3 vHitNormalMid(probeResult[0].GetHitPolyNormal());

											DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Sphere(vHitPosMid, 0.05f, Color_green, true, -1));

											Vector3 vDiff1(vHitPosMid - vHitPos1);
											if(vDiff1.Mag2() > 0.f)
											{
												vDiff1.NormalizeFast();

												vTempRight.Cross(vDiff1, ZAXIS);
												vSlopeNormalAvg.Cross(vTempRight, vDiff1);
												vSlopeNormalAvg.NormalizeFast();

												if((Abs(vSlopeNormalAvg.z - vSlopeNormal.z) > fMidPointTestMaxSlopeDiff) || ((vHitNormalMid.z > fSlopeNormal) && (vHitNormal1.z > fSlopeNormal)))
												{
													bFoundEdge = true;
													bSlope = false;
												}
											}

											Vector3 vDiff2(vHitPos2 - vHitPosMid);
											if(vDiff2.Mag2() > 0.f)
											{
												vDiff2.NormalizeFast();

												vTempRight.Cross(vDiff2, ZAXIS);
												vSlopeNormalAvg.Cross(vTempRight, vDiff2);
												vSlopeNormalAvg.NormalizeFast();

												if((Abs(vSlopeNormalAvg.z - vSlopeNormal.z) > fMidPointTestMaxSlopeDiff) || ((vHitNormalMid.z > fSlopeNormal) && (vHitNormal2.z > fSlopeNormal)))
												{
													bFoundEdge = true;
													bSlope = false;
												}
											}
										}
										else
										{
											bSlope = false;
										}
									}
								}

								if(bEdgeDetected && !bStairs)
								{
									// Verify edge still exists by comparing vHitPos1, vHitPos2 to ground position since probes may be far enough ahead but ped capsule hasn't reached the edge yet
									Vector3 vGroundPos(pPed->GetGroundPos());
									vDiff = vHitPos2 - vGroundPos;

									if(vDiff.Mag2() > 0.f)
									{
										vDiff.NormalizeFast();

										vTempRight.Cross(vDiff, ZAXIS);
										vSlopeNormal.Cross(vTempRight, vDiff);
										vSlopeNormal.NormalizeFast();

										if(abs(vSlopeNormal.z) < fSlopeNormal)
										{
											vDiff = vHitPos1 - vGroundPos;

											if(vDiff.Mag2() > 0.f)
											{
												vDiff.NormalizeFast();

												vTempRight.Cross(vDiff, ZAXIS);
												vSlopeNormalAvg.Cross(vTempRight, vDiff);
												vSlopeNormalAvg.NormalizeFast();

												if((Abs(vSlopeNormalAvg.z - vSlopeNormal.z) > fMidPointTestMaxSlopeDiff) || ((pPed->GetGroundNormal().z > fSlopeNormal) && (vHitNormal1.z > fSlopeNormal)))
												{
													bFoundEdge = true;
													bSlope = false;
												}
											}
										}
									}
								}

								if(bFoundEdge)
								{
									pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_EdgeDetected, true);
									vSlopeNormal = pPed->GetGroundNormal();

									DEBUG_DRAW_ONLY(if(bDebugSlopeScan)grcDebugDraw::Line(vTestStart1 - Vector3(0.f, 0.f, (fSlopeScanStartZOffset - fSlopeBlockingProbeZOffset + 0.1f)), vTestStart2 - Vector3(0.f, 0.f, (fSlopeScanStartZOffset - fSlopeBlockingProbeZOffset + 0.1f)), Color_cyan, Color_cyan, -1));
								}
							}
						}
					}

					if(bStairs || bSlope)
					{
						pPed->SetSlopeNormal(vSlopeNormal);	
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected, true);
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected, bStairs);
						if(bStairs)
						{
							uStairsDetectedTime = fwTimer::GetTimeInMilliseconds();
						}
					}
				}
			}
		}
		else
		{
			bool bStairs = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs);
			if(bStairs)
			{
				if(vLastSlopePosition != VEC3_ZERO)
				{
					const Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vLastSlopePosition;
					if(vDiff.XYMag2() > square(fSlopeXYDelta) || (!bStairs && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected)))
					{
						Vector3 vTempRight;
						vTempRight.Cross(vDiff, ZAXIS);
						Vector3 vSlopeNormal;
						vSlopeNormal.Cross(vTempRight, vDiff);
						if(vSlopeNormal.Mag2() > 0.f)
						{
							vSlopeNormal.NormalizeFast();
						}

						if(vSlopeNormal.GetZ() > 0.f && (vSlopeNormal.GetZ() < fSlopeNormal || bStairs))
						{
							pPed->SetSlopeNormal(vSlopeNormal);
						
							pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected, true);
							pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected, bStairs);
							if(bStairs)
							{
								uStairsDetectedTime = fwTimer::GetTimeInMilliseconds();
							}
						}
						else
						{
							ResetSlopesAndStairs(pPed, vLastSlopePosition);
						}

						vLastSlopePosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					}
				}
				else
				{
					vLastSlopePosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				}
			}
			else
			{
				if(pPed->GetGroundNormal().GetZ() > 0.f && (pPed->GetGroundNormal().GetZ() < fSlopeNormal))
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected, true);
					pPed->SetSlopeNormal(pPed->GetGroundNormal());
				}
				else
				{
					ResetSlopesAndStairs(pPed, vLastSlopePosition);
				}
			}
		}
	}

	if(bWasOnStair && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
	{
		pPed->BlockCapsuleResize(1000);
	}

#if DEBUG_DRAW
	if(bDebugSlopeScan)
	{
		Vec3V vStart = pPed->GetTransform().GetPosition();
		Vec3V vEnd = vStart;
		vEnd += VECTOR3_TO_VEC3V(pPed->GetSlopeNormal());
		grcDebugDraw::Line(vStart, vEnd, Color_pink, -1);
	}
#endif // DEBUG_DRAW
}

////////////////////////////////////////////////////////////////////////////////
// When player decides to slow down, if ladder is free at that point, 
// player will get on even if ladder becomes blocked
////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ProcessMPLadders(Vector2 const& vCurrentMBR, Vector2& vDesiredMBR)
{
	// MP Ladders - we slow players down if they are running towards a ladder they can't get on to stop them running over an edge and killing themselves...
	if(NetworkInterface::IsGameInProgress())
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, minimumHeading_MPLadderMBR,			0.7f,  0.0f, 0.99f, 0.01f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, maximumSprintDistance_MPLadderMBR,	8.5f,  0.0f, 16.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, maximumRunDistance_MPLadderMBR,		6.0f,  0.0f, 16.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, targetMBR_MPLadderMBR,				MOVEBLENDRATIO_WALK,  MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_RUN, 0.1f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, ladderRescanDist_MPLadderMBR,		1.5f, 0.0f, 5.0f, 0.1f);

		CPed* pPed = GetPed();

		if(pPed->IsPlayer() /*&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableLadderClimbing)*/)
		{
			// if we're moving fast enough where we might want to be slowed down...
			if(vCurrentMBR.y > targetMBR_MPLadderMBR)
			{
				bool ladderBlock = false;

				// Generate the ladder data needed if we've moved far enough away from the last time we generated it...
				Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				if(((pedPos - m_MPOnlyLadderInfoCache.m_playerPos).Mag2()) > (ladderRescanDist_MPLadderMBR * ladderRescanDist_MPLadderMBR))
				{
					CEntity* ladder  = NULL;
					s32 effectIndex  = -1;
					Vector3 getOnPos(VEC3_ZERO);
					ladder = CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pPed, effectIndex, getOnPos, true /*ignoreDistanceCulling - get the nearest ladder to us*/);

					// different ladder or different part of a ladder
					if((ladder != m_MPOnlyLadderInfoCache.m_ladder) || (effectIndex != m_MPOnlyLadderInfoCache.m_effectIndex))
					{
						m_MPOnlyLadderInfoCache.Clear();

						m_MPOnlyLadderInfoCache.m_ladder		= ladder;
						m_MPOnlyLadderInfoCache.m_effectIndex	= effectIndex;
						m_MPOnlyLadderInfoCache.m_vGetOnPosition= getOnPos;
						m_MPOnlyLadderInfoCache.m_playerPos		= pedPos;
									
						if(m_MPOnlyLadderInfoCache.m_ladder)
						{
							m_MPOnlyLadderInfoCache.m_bGettingOnAtBottom = CTaskGoToAndClimbLadder::IsGettingOnBottom(m_MPOnlyLadderInfoCache.m_ladder, *pPed, m_MPOnlyLadderInfoCache.m_effectIndex);

							if(!m_MPOnlyLadderInfoCache.m_bGettingOnAtBottom)
							{
								CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(m_MPOnlyLadderInfoCache.m_ladder, m_MPOnlyLadderInfoCache.m_effectIndex, m_MPOnlyLadderInfoCache.m_vTop, m_MPOnlyLadderInfoCache.m_vBottom, m_MPOnlyLadderInfoCache.m_vNormal, m_MPOnlyLadderInfoCache.m_bCanGetOffAtTop);
							}
						}
					}

					//---

					// if we've got a ladder and we would get on it's top if we went to it....
					if( ( m_MPOnlyLadderInfoCache.m_ladder ) && ( ! m_MPOnlyLadderInfoCache.m_bGettingOnAtBottom ) )
					{
						// When a ped starts running towards a ladder it may be free to get on, however another player may climb up and block the ladder when the player is very close
						// which will produce the 'flying over the edge' problem we're trying to solve - because the players can climb up ladders in MP extremely quickly
						// we just test to see if any other player is on or near the ladder when we're running towards it 

						// First check to see if the local player is on a ladder...
						if(pPed->IsNetworkClone())
						{
							CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
							CTaskClimbLadder* ladderTask = static_cast<CTaskClimbLadder*>(pLocalPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
							if(ladderTask)	
							{
								// the local ped is climbing same ladder (and same part of ladder) as the one we're nearest too, definitely block...
								if( ( ladderTask->GetLadder() == m_MPOnlyLadderInfoCache.m_ladder ) && (ladderTask->GetEffectIndex() == m_MPOnlyLadderInfoCache.m_effectIndex) )
								{
									ladderBlock = true;
								}
							}
						}
						
						if(!ladderBlock)
						{
							ladderBlock = CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_MPOnlyLadderInfoCache.m_ladder, m_MPOnlyLadderInfoCache.m_effectIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN);

							if(!ladderBlock)
							{
								CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
								for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
								{
									CPed* pNearbyPed = static_cast<CPed*>(pEnt);
									if( (pNearbyPed) && ( pNearbyPed != pPed ) )
									{
										Vector3 nearPlayerPos = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());	

										// if the ped within a 30m cylinder of us at any height (could be at the bottom of a very tall ladder)
										if((pedPos - nearPlayerPos).XYMag2() < (30.0f * 30.0f))
										{
											CTaskClimbLadder* nearbyLadderTask = static_cast<CTaskClimbLadder*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
											if(nearbyLadderTask)	
											{
												// another ped is climbing same ladder and same part of ladder as the one we're nearest too, definitely block...
												if(( nearbyLadderTask->GetLadder() == m_MPOnlyLadderInfoCache.m_ladder ) && ( nearbyLadderTask->GetEffectIndex() == m_MPOnlyLadderInfoCache.m_effectIndex))
												{
													// tell the player to slow to walk speed.
													ladderBlock = true;
													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}

				//---

				Vector3 heading = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

				Vector3 posToTopDir = m_MPOnlyLadderInfoCache.m_vTop - pedPos;
				posToTopDir.Normalize();

				float dot = posToTopDir.Dot(heading);
				// dot = ((dot + 1.0f) / 2.0f); // conv to 0 - 1.0f, multiple by target MBR, more we're running towards it, more we slow down?
				// mbr based on distance to ladder?

				bool headingAndDistBlock = false;

				float distance = maximumRunDistance_MPLadderMBR;
				if(vCurrentMBR.y >= MOVEBLENDRATIO_SPRINT)
				{
					distance = maximumSprintDistance_MPLadderMBR;
				}

				if((dot > minimumHeading_MPLadderMBR) && ( (pedPos - m_MPOnlyLadderInfoCache.m_vTop).XYMag2() < (distance * distance) ) )
				{
					 headingAndDistBlock = true;
				}
				else
				{
					m_MPOnlyLadderInfoCache.m_bForceSlowDown = false;
				}

				//---

				if((ladderBlock && headingAndDistBlock) || m_MPOnlyLadderInfoCache.m_bForceSlowDown )
				{
					m_MPOnlyLadderInfoCache.m_bForceSlowDown = true;

					// grcDebugDraw::Line(pedPos, m_MPOnlyLadderInfoCache.m_vTop, Color_blue, Color_red);

					vDesiredMBR.y = MIN(vDesiredMBR.y, targetMBR_MPLadderMBR);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ProcessAI()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if( m_fTimeBetweenAddingRunningShockingEvents > 0.0f )
	{
		m_fTimeBetweenAddingRunningShockingEvents -= fwTimer::GetTimeStep();
	}

	// Do we want to start moving?
	if((IsInMotionState(CPedMotionStates::MotionState_Run) || IsInMotionState(CPedMotionStates::MotionState_Sprint)) && m_fTimeBetweenAddingRunningShockingEvents <= 0.0f )
	{
		bool bPedsGetScaredByRunningPeds = !pPed->IsNetworkClone();

		if(bPedsGetScaredByRunningPeds)
		{
			CTask * pTask = pPed->GetPedIntelligence()->GetTaskActive();
			// Don't scare peds if all we are doing is running to get out of the rain.
			const bool bRunningForShelter = (pTask && pTask->GetTaskType()==CTaskTypes::TASK_WANDER && g_weather.IsRaining());
			// Likewise don't scare peds if we're just running to get across the road.
			const bool bRunningAcrossRoad = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS) != NULL;
			// Don't allow jogging peds to create a shocking event.
			bool bJoggingPed = false;

			s32 scenarioType = pPed->GetPedIntelligence()->GetQueriableInterface()->GetRunningScenarioType();
			if (scenarioType != Scenario_Invalid)
			{
				const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
				if(pScenarioInfo->GetIsClass<CScenarioJoggingInfo>())
				{
					bJoggingPed = true;
				}
			}

			if(!bRunningForShelter && !bRunningAcrossRoad && !bJoggingPed)
			{
				CEventShockingRunningPed ev(*pPed);
				CShockingEventsManager::Add(ev);
				m_fTimeBetweenAddingRunningShockingEvents = fwRandom::GetRandomNumberInRange(1.5f, 2.0f);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::FlipFootFlagsForStopping(const CTaskMoveGoToPointAndStandStill* pGotoTask)
{
	taskAssert(pGotoTask);

	// Only flip the foot flags if we are on a foot step this frame, as if we are not, we can't guarantee that we'll just skip one step
	if(m_Flags.IsFlagSet(Flag_LastFootLeftThisFrame) || m_Flags.IsFlagSet(Flag_LastFootRightThisFrame))
	{
		static dev_float FLIP_DIST_MOD = 0.25f;
		static dev_float FLIP_DIST_MIN = 0.25f;
		static dev_float FLIP_DIST_MIN_STOP = 0.5f;
		float fDist = pGotoTask->GetDistanceRemaining();
		bool bShouldRunStopFlip = (m_Flags.IsFlagSet(Flag_Running) && fDist < GetStoppingDistance(pGotoTask->GetTargetHeading()) * FLIP_DIST_MOD);
		bool bShouldWalkStopFlip = (!m_Flags.IsFlagSet(Flag_Running) && fDist < FLIP_DIST_MIN && GetStoppingDistance(pGotoTask->GetTargetHeading()) > FLIP_DIST_MIN_STOP);
		if(bShouldRunStopFlip || bShouldWalkStopFlip)
		{
			if(m_MoveNetworkHelper.IsNetworkActive())
			{
				if(m_Flags.IsFlagSet(Flag_LastFootLeft))
				{
					m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
				}
				else
				{
					m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
				}
			}
			m_Flags.SetFlag(Flag_FlipLastFootLeft);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ResetSlopesAndStairs(CPed* pPed, Vector3& vLastSlopePosition)
{
	// Clear values
	pPed->SetSlopeNormal(ZAXIS);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_EdgeDetected, false);
	vLastSlopePosition = VEC3_ZERO;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsOnSlopesAndStairs(const CPed* pPed, const bool bCrouching, float& fSlopePitch)
{
	fSlopePitch = FLT_MAX;
	if(!bCrouching && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
	{
		Vector3 vNormal = pPed->GetSlopeNormal();
		fSlopePitch = rage::Atan2f(vNormal.XYMag(), vNormal.z);

		vNormal.z = 0.f;
		if(vNormal.Mag2() > 0.f)
		{
			vNormal.NormalizeFast();
		}

		// Invert pitch if needed
		float fSlopeHeadingDot = -vNormal.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
		fSlopePitch *= fSlopeHeadingDot;
	}

	TUNE_GROUP_FLOAT(PED_MOVEMENT, MinPitchForSlopes, -0.85f, -PI, PI, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, MaxPitchForSlopes, 0.85f, -PI, PI, 0.01f);
	TUNE_GROUP_FLOAT(PED_MOVEMENT, MinVelocityForSlopes, 0.5f, 0.f, 10.f, 0.01f);
	if(fSlopePitch > MinPitchForSlopes && fSlopePitch < MaxPitchForSlopes && pPed->GetVelocity().Mag2() > square(MinVelocityForSlopes))
	{
		return true;
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::GetUsingAIMotion() const
{
	return (!GetPed()->GetMotionData()->GetPlayerHasControlOfPedThisFrame() && !GetPed()->IsNetworkPlayer());
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::UsingHeavyWeapon()
{
	CPed* pPed = GetPed();

	bool bUsingHeavyWeapon = false;
	if(pPed->GetWeaponManager())
	{
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
		if(pWeaponInfo && pWeaponInfo->GetIsHeavy())
		{
			bUsingHeavyWeapon = true;
		}
	}

	return bUsingHeavyWeapon;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::InitFromStrafeUpperBodyTransition()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return;
	}
#endif // FPS_MODE_SUPPORTED

	// Check for the upper body transition anim
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if(pWeaponInfo)
	{
		fwMvClipSetId upperBodyClipSetId = pWeaponInfo->GetAppropriateFromStrafeWeaponClipSetId(pPed);
		if(upperBodyClipSetId != CLIP_SET_ID_INVALID)
		{
			//B*1714120 - Ensure clipset is streamed in to avoid "Object is not in memory" assert
			const fwClipSet* pUpperBodyClipSet = fwClipSetManager::GetClipSet(upperBodyClipSetId);
			if (pUpperBodyClipSet->IsStreamedIn_DEPRECATED())
			{
				m_MoveNetworkHelper.SetFlag(true, ms_UseFromStrafeTransitionUpperBodyId);
				m_MoveNetworkHelper.SetClipSet(upperBodyClipSetId, ms_FromStrafeTransitionUpperBodyClipSetVarId);
			}
			// Init variable
			m_fFromStrafeInitialFacingDirection = pPed->GetFacingDirectionHeading();
			m_fFromStrafeTransitionUpperBodyWeight = 1.f;

			if(pPed->GetMotionData()->GetDesiredHeadingSetThisFrame())
			{
				m_fFromStrafeTransitionUpperBodyDirection = CalcDesiredDirection();
			}
			else
			{
				float fDesiredHeading = pPed->GetMotionData()->GetDesiredHeading();
				pPed->GetVelocityHeading(fDesiredHeading);
				m_fFromStrafeTransitionUpperBodyDirection = SubtractAngleShorter(fDesiredHeading, pPed->GetCurrentHeading());
			}

			m_MoveNetworkHelper.SetFloat(ms_FromStrafeTransitionUpperBodyDirectionId, ConvertRadianToSignal(m_fFromStrafeTransitionUpperBodyDirection));

			if(pWeaponInfo->GetUseFromStrafeUpperBodyAimNetwork())
			{
				const fwMvClipSetId& weaponClipSet = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
				if(weaponClipSet != CLIP_SET_ID_INVALID)
				{
					const fwClipSet* pWeaponClipSet = fwClipSetManager::GetClipSet(weaponClipSet);
					if(taskVerifyf(pWeaponClipSet, "ClipSet [%s] does not exist", weaponClipSet.TryGetCStr()))
					{
						if(pWeaponClipSet->IsStreamedIn_DEPRECATED())
						{
							// Init the upper body aim
							m_pUpperBodyAimPitchHelper = rage_new CUpperBodyAimPitchHelper();
							m_pUpperBodyAimPitchHelper->SetClipSet(weaponClipSet);
							m_pUpperBodyAimPitchHelper->BlendInUpperBodyAim(m_MoveNetworkHelper, *pPed, 0.f);
						}
						else
						{
							taskWarningf("ClipSet [%s] not streamed in, Ped [0x%p], Clone [%d]", weaponClipSet.TryGetCStr(), pPed, pPed ? pPed->IsNetworkClone() : 0);
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::UpdateFromStrafeUpperBodyTransition()
{
	if(m_fFromStrafeInitialFacingDirection != FLT_MAX)
	{
		CPed* pPed = GetPed();

		float fFromStrafeTransitionUpperBodyWeight;
		float fInitialFacingDirectionDiff = Abs(SubtractAngleShorter(pPed->GetDesiredHeading(), m_fFromStrafeInitialFacingDirection));
		if(fInitialFacingDirectionDiff > 0.f)
		{
			float fFacingDirectionDiff = Abs(SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetFacingDirectionHeading()));
			fFacingDirectionDiff = Min(fInitialFacingDirectionDiff, fFacingDirectionDiff);
			fFromStrafeTransitionUpperBodyWeight = fFacingDirectionDiff / fInitialFacingDirectionDiff;
		}
		else
		{
			fFromStrafeTransitionUpperBodyWeight = 0.f;
		}

		Approach(m_fFromStrafeTransitionUpperBodyWeight, fFromStrafeTransitionUpperBodyWeight, ms_Tunables.FromStrafe_WeightRate, GetTimeStep());			
		m_MoveNetworkHelper.SetFloat(ms_FromStrafeTransitionUpperBodyWeightId, m_fFromStrafeTransitionUpperBodyWeight);

		// Aim network
		if(m_pUpperBodyAimPitchHelper)
		{
			m_pUpperBodyAimPitchHelper->Update(*pPed, true);
		}

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::CleanUpFromStrafeUpperBodyTransition()
{
	delete m_pUpperBodyAimPitchHelper;
	m_pUpperBodyAimPitchHelper = NULL;
	m_fFromStrafeInitialFacingDirection = FLT_MAX;
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(false, ms_UseFromStrafeTransitionUpperBodyId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::UpdateActionModeMovementRate()
{
	static dev_float ACTION_MODE_MOVEMENT_APROACH_RATE = 1.f;

	if(m_pActionModeRateData)
	{
		if(ShouldUseActionModeMovementRate())
		{
			if(m_Flags.IsFlagSet(Flag_Running))
			{
				Approach(m_pActionModeRateData->ActionModeMovementRate, m_pActionModeRateData->ActionModeSpeedRates[1], ACTION_MODE_MOVEMENT_APROACH_RATE, GetTimeStep());
			}
			else
			{
				Approach(m_pActionModeRateData->ActionModeMovementRate, m_pActionModeRateData->ActionModeSpeedRates[0], ACTION_MODE_MOVEMENT_APROACH_RATE, GetTimeStep());
			}
		}
		else
		{
			Approach(m_pActionModeRateData->ActionModeMovementRate, 1.f, ACTION_MODE_MOVEMENT_APROACH_RATE, GetTimeStep());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::ShouldUseActionModeMovementRate() const
{
	const CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer())
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl && pControl->GetPedSprintIsDown())
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsSuppressingWeaponHolding() const
{
	return GetState() == State_Idle && m_Flags.IsFlagSet(Flag_SuppressWeaponHoldingForIdleTransition);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateMoving_MovingDirection(const float fTimeStep, const float fDesiredDirection)
{
	CPed* pPed = GetPed();

	// Cache the difference in angle between desired and actual
	const float fDirectionDiff = SubtractAngleShorter(fDesiredDirection, m_fMovingDirection);

	// Check for change in direction
	bool bDirectionChange = false;
	bool bUsingAIMotion = GetUsingAIMotion();
	if (!bUsingAIMotion)
	{
		if(IsClose(fDirectionDiff, 0.f, 0.01f))
		{
			// Reset variables if going straight ahead
			bDirectionChange = true;
		}
		else if(Sign(fDirectionDiff) != Sign(m_fPreviousMovingDirectionDiff))
		{
			bDirectionChange = true;
		}
	}

	if(bDirectionChange)
	{
		// Reset
		m_fMovingDirectionRate = 0.f;
		m_fExtraHeadingRate = 0.f;
		// Store the old direction
		m_fPreviousMovingDirectionDiff = fDirectionDiff;
	}

	Tunables::sMovingVarsSet* pVarsSet;
	if(pPed->GetMotionData()->GetUsingStealth())
	{
		pVarsSet = &ms_Tunables.MovingVarsSet[Tunables::MVST_Stealth];
	}
	else if(pPed->IsUsingActionMode())
	{
		pVarsSet = &ms_Tunables.MovingVarsSet[Tunables::MVST_Action];
	}
	else
	{
		pVarsSet = &ms_Tunables.MovingVarsSet[Tunables::MVST_Normal];
	}

	Tunables::sMovingVars* pVars;
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettingsForScript))
	{
		pVars = &pVarsSet->TighterTurn;
	}
	else if(bUsingAIMotion)
	{
		pVars = &pVarsSet->StandardAI;
	}
	else
	{
		pVars = &pVarsSet->Standard;
	}

	// Convert to 0..1 range
	const float fDirectionSmoothingAngleRange = pVars->MovingDirectionSmoothingAngleMax - pVars->MovingDirectionSmoothingAngleMin;
	float fDirectionSmoothingAngleDiff = Min(Max(Abs(fDirectionDiff) - pVars->MovingDirectionSmoothingAngleMin, 0.f), fDirectionSmoothingAngleRange);
	fDirectionSmoothingAngleDiff /= fDirectionSmoothingAngleRange;

	const float fDirectionSmoothingRateTargetMax = !m_Flags.IsFlagSet(Flag_Running) ? pVars->MovingDirectionSmoothingRateMaxWalk : pVars->MovingDirectionSmoothingRateMaxRun;
	float fDirectionSmoothingRateTarget = pVars->MovingDirectionSmoothingRateMin + ((fDirectionSmoothingRateTargetMax - pVars->MovingDirectionSmoothingRateMin) * fDirectionSmoothingAngleDiff);
	float fDirectionSmoothingRateAcceleration = pVars->MovingDirectionSmoothingRateAccelerationMin + ((pVars->MovingDirectionSmoothingRateAccelerationMax - pVars->MovingDirectionSmoothingRateAccelerationMin) * fDirectionSmoothingAngleDiff);

	// If our angle is within the forward angle, apply the modifiers
	// Don't apply this if we have just changed direction
	const float fDirectionSmoothingForwardAngle = !m_Flags.IsFlagSet(Flag_Running) ? pVars->MovingDirectionSmoothingForwardAngleWalk : pVars->MovingDirectionSmoothingForwardAngleRun;
	if(Abs(fDesiredDirection) < fDirectionSmoothingForwardAngle && !bDirectionChange)
	{
		fDirectionSmoothingRateTarget *= pVars->MovingDirectionSmoothingForwardRateMod;
		fDirectionSmoothingRateAcceleration *= pVars->MovingDirectionSmoothingForwardRateAccelerationMod;
	}

	// Approach the desired direction
#if FPS_MODE_SUPPORTED
	if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
#endif // FPS_MODE_SUPPORTED
	{
		Approach(m_fMovingDirectionRate, fDirectionSmoothingRateTarget, fDirectionSmoothingRateAcceleration, fTimeStep);
		Approach(m_fMovingDirection, fDesiredDirection, m_fMovingDirectionRate, fTimeStep);
	}

	if(pVars->UseExtraHeading)
	{
		StateMoving_ExtraHeading(fTimeStep, pVars->UseMovingDirectionDiff ? fDirectionDiff : fDesiredDirection, *pVars);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateMoving_ExtraHeading(const float fTimeStep, const float fDesiredDirection, const Tunables::sMovingVars& vars)
{
	CPed* pPed = GetPed();

	// Apply extra heading - only if we are not turning quickly enough
	static dev_float DONT_DO_EXTRA_HEADING_VEL = 7.5f;
	if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0 && pPed->GetAngVelocity().Mag2() < square(DONT_DO_EXTRA_HEADING_VEL))
	{
		// Convert to a -1..1 range
		const float fExtraHeadingAngleRange = vars.MovingExtraHeadingChangeAngleMax - vars.MovingExtraHeadingChangeAngleMin;
		float fExtraHeadingAngleDiff = Min(Max(Abs(fDesiredDirection) - vars.MovingExtraHeadingChangeAngleMin, 0.f), fExtraHeadingAngleRange);
		fExtraHeadingAngleDiff /= fExtraHeadingAngleRange;
		fExtraHeadingAngleDiff *= Sign(fDesiredDirection);

		float fExtraHeadingRateTarget;
#if FPS_MODE_SUPPORTED
		const bool bFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		if(bFpsMode)
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_MovingExtraHeadingChangeRate, 10.f, 0.f, 100.f, 0.01f);
			fExtraHeadingRateTarget = fExtraHeadingAngleDiff * FPS_MovingExtraHeadingChangeRate;
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			fExtraHeadingRateTarget = fExtraHeadingAngleDiff * (!m_Flags.IsFlagSet(Flag_Running) ? vars.MovingExtraHeadingChangeRateWalk : vars.MovingExtraHeadingChangeRateRun);
		}

		float fExtraHeadingRateAcceleration = vars.MovingExtraHeadingChangeRateAccelerationMin + Abs(fExtraHeadingAngleDiff) * (vars.MovingExtraHeadingChangeRateAccelerationMax - vars.MovingExtraHeadingChangeRateAccelerationMin);
		Approach(m_fExtraHeadingRate, fExtraHeadingRateTarget, fExtraHeadingRateAcceleration, fTimeStep);

		if((!pPed->GetMovePed().GetTaskNetwork() ||
			pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)
#if FPS_MODE_SUPPORTED
			|| (bFpsMode && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_RELOAD_GUN))
#endif // FPS_MODE_SUPPORTED
			)
#if FPS_MODE_SUPPORTED
			&& (!bFpsMode || !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))
#endif // FPS_MODE_SUPPORTED
			)
		{
			float fExtraHeading = m_fExtraHeadingRate * fTimeStep;
			if(Abs(fExtraHeading) > 0.0f)
			{
				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fExtraHeading);
			}
		}
	}
	else
	{
		m_fExtraHeadingRate = 0.f;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateMoving_SlopesAndStairs(const float fTimeStep)
{
	m_Flags.ClearFlag(Flag_WasOnStairs);

	CPed* pPed = GetPed();

	float fSlopePitch;
	if(IsOnSlopesAndStairs(pPed, m_Flags.IsFlagSet(Flag_Crouching), fSlopePitch) && !pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if(!m_Flags.IsFlagSet(Flag_WasOnSlope))
		{
			m_fSmoothedSlopeDirection = fSlopePitch;
		}
		else
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fLocomotionSlopeDirectionSmoothing, 10.f, 0.0f, 20.0f, 0.001f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fLocomotionSlopeDirectionSmoothingLimit, 0.2f, 0.0f, 20.0f, 0.001f);

			float f = fSlopePitch - m_fSmoothedSlopeDirection;
			f = Clamp(f, -fLocomotionSlopeDirectionSmoothingLimit, fLocomotionSlopeDirectionSmoothingLimit);
			f += m_fSmoothedSlopeDirection;
			f  = Clamp(f, -PI, PI);
			m_fSmoothedSlopeDirection = Lerp(fLocomotionSlopeDirectionSmoothing*fTimeStep, m_fSmoothedSlopeDirection, f);
			m_fSmoothedSlopeDirection = fwAngle::LimitRadianAngleSafe(m_fSmoothedSlopeDirection);
		}

		m_MoveNetworkHelper.SetFloat(ms_SlopeDirectionId, ConvertRadianToSignal(m_fSmoothedSlopeDirection));

		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
		{
			m_MoveNetworkHelper.SetFlag(true, ms_OnStairsId);

			TUNE_GROUP_FLOAT(PED_MOVEMENT, fSteepStairsThreshold, QUARTER_PI, 0.0f, HALF_PI, 0.01f);
			const bool bSteepStairs = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairSlope) && (fabsf(fSlopePitch) > fSteepStairsThreshold);
			m_MoveNetworkHelper.SetFlag(bSteepStairs, ms_OnSteepStairsId);

			m_Flags.SetFlag(Flag_WasOnStairs);
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(true, ms_OnSlopeId);
			m_MoveNetworkHelper.SetFlag(false, ms_OnSteepStairsId);
		}

		m_Flags.SetFlag(Flag_WasOnSlope);
	}
	else
	{
		m_fSmoothedSlopeDirection = 0.0f;
		m_MoveNetworkHelper.SetFlag(false, ms_OnStairsId);
		m_MoveNetworkHelper.SetFlag(false, ms_OnSteepStairsId);
		m_MoveNetworkHelper.SetFlag(false, ms_OnSlopeId);
		if (m_Flags.IsFlagSet(Flag_WasOnSlope))
		{
			// Block gait reduction during slope->nonslope transitions
			SetGaitReductionBlock();
		}
		m_Flags.ClearFlag(Flag_WasOnSlope);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateMoving_Snow()
{
	const bool bCrouched = m_Flags.IsFlagSet(Flag_Crouching);
	if(!bCrouched && m_fSnowDepthSignal > 0.f && !GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		static const fwMvClipSetId SNOW_MOVEMENT("move_snow",0x33A6CC13);
		fwClipSet* pSnowMovementClipSet = fwClipSetManager::GetClipSet(SNOW_MOVEMENT);
		if(pSnowMovementClipSet && pSnowMovementClipSet->Request_DEPRECATED())
		{
			m_MoveNetworkHelper.SetClipSet(SNOW_MOVEMENT, ms_SnowMovementClipSetVarId);
			m_MoveNetworkHelper.SetFloat(ms_SnowDepthId, m_fSnowDepthSignal);
			m_MoveNetworkHelper.SetFlag(true, ms_OnSnowId);
			m_Flags.SetFlag(Flag_WasOnSnow);
		}
	}
	else
	{
		m_fSmoothedSnowDepth = 0.0f;
		m_MoveNetworkHelper.SetFlag(false, ms_OnSnowId);
		m_Flags.ClearFlag(Flag_WasOnSnow);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::StateMoving_MovementRate()
{
	CPed* pPed = GetPed();

	// Update the rate
	if(m_Flags.IsFlagSet(Flag_Running) && !m_Flags.IsFlagSet(Flag_WasRunning))
	{
		m_fMovementTurnRate = GetFloatPropertyFromClip(m_ClipSetId, ms_RunClipId, "TurnRate");
	}
	else if(!m_Flags.IsFlagSet(Flag_Running) && m_Flags.IsFlagSet(Flag_WasRunning))
	{
		m_fMovementTurnRate = GetFloatPropertyFromClip(m_ClipSetId, ms_WalkClipId, "TurnRate");
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_fMovementRate = 1.f;
	}
	else
#endif // FPS_MODE_SUPPORTED
	// Fast walk
	if(pPed->GetMotionData()->GetCurrentMbrY() < MBR_RUN_BOUNDARY)
	{
		const float fRange = (MBR_RUN_BOUNDARY - MOVEBLENDRATIO_WALK);
		m_fMovementRate = Max((pPed->GetMotionData()->GetCurrentMbrY() - MOVEBLENDRATIO_WALK), 0.f) / fRange;
		m_fMovementRate = ms_Tunables.FastWalkRateMin + m_fMovementRate * (ms_Tunables.FastWalkRateMax - ms_Tunables.FastWalkRateMin);
	}
	// Slow run
	else if(pPed->GetMotionData()->GetCurrentMbrY() < MOVEBLENDRATIO_RUN)
	{
		const float fRange = (MOVEBLENDRATIO_RUN - MBR_RUN_BOUNDARY);
		m_fMovementRate = (Min(pPed->GetMotionData()->GetCurrentMbrY(), MOVEBLENDRATIO_RUN) - MBR_RUN_BOUNDARY) / fRange;
		m_fMovementRate = ms_Tunables.SlowRunRateMin + m_fMovementRate * (ms_Tunables.SlowRunRateMax - ms_Tunables.SlowRunRateMin);
	}
	// Fast run
	else if(pPed->GetMotionData()->GetCurrentMbrY() < MBR_SPRINT_BOUNDARY)
	{
		const float fRange = (MBR_SPRINT_BOUNDARY - MOVEBLENDRATIO_RUN);
		m_fMovementRate = Max((pPed->GetMotionData()->GetCurrentMbrY() - MOVEBLENDRATIO_RUN), 0.f) / fRange;
		m_fMovementRate = ms_Tunables.FastRunRateMin + m_fMovementRate * (ms_Tunables.FastRunRateMax - ms_Tunables.FastRunRateMin);
	}
	else
	{
		m_fMovementRate = 1.f;
	}
}

float CTaskHumanLocomotion::CNC_GetNormalizedSlopeDirectionForAdjustedMovementRate() const
{
	// Normalize slope direction within min/max range.
	TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fMinSlopeDirection, 0.3f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fMaxSlopeDirection, 0.7f, 0.0f, 1.0f, 0.01f);
	float fNormalizedSlopeDirection = ConvertRadianToSignal(m_fSmoothedSlopeDirection);
	const float fDelta		= fMaxSlopeDirection - fMinSlopeDirection;
	const float fRelative	= fNormalizedSlopeDirection - fMinSlopeDirection;
	fNormalizedSlopeDirection = ((fDelta != 0.0f) && (fRelative != 0.0f)) ? Clamp(fRelative / fDelta, 0.0f, 1.0f) : 0.5f;
	return fNormalizedSlopeDirection;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SetUseLeftFootTransition(bool bUseLeftFoot) 
{
	// Update foot down flags
	if(bUseLeftFoot)
	{
		m_Flags.ClearFlag(Flag_LastFootLeft);
		m_Flags.SetFlag(Flag_LastFootRightThisFrame);
		m_Flags.ClearFlag(Flag_FlipLastFootLeft);
		m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
	}
	else
	{
		m_Flags.SetFlag(Flag_LastFootLeft);
		m_Flags.SetFlag(Flag_LastFootLeftThisFrame);
		m_Flags.ClearFlag(Flag_FlipLastFootLeft);
		m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::IsTransitionBlocked() const
{
	static const fwMvBooleanId s_Block("BLOCK_TRANSITION",0x70ABE2B4);
	if(m_MoveNetworkHelper.IsNetworkActive() && CheckBooleanEvent(this, s_Block) && !IsFootDown())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::WillDoStrafeTransition(const CPed* pPed) const
{
	return m_Flags.IsFlagSet(Flag_FromStrafe) && CTaskMotionAimingTransition::UseMotionAimingTransition(pPed->GetMotionData()->GetCurrentHeading(), m_fInitialMovingDirection, pPed);
}

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskHumanLocomotion::VerifyClip(const crClip* pClip, const s32 iFlags, const char* clipName, const char* dictionaryName, const CPed *pPed)
{
	if(taskVerifyf(pClip, "Ped [%s] Missing movement clip [%s] from dictionary [%s], add bug for Default Anim In-Game", pPed ? pPed->GetDebugName() : "None", clipName, dictionaryName))
	{
		if(taskVerifyf(pClip->GetTags(), "Movement clip [%s] from dictionary [%s] has no tags, add bug for Default Anim In-Game", clipName, dictionaryName))
		{
			u32 uFootTagCount = 0;
			bool bLastFootRight = false;
			bool bFirstFootTag = true;

			crTagIterator it(*pClip->GetTags());
			while(*it)
			{
				const crTag* pTag = *it;			
				if(pTag->GetKey() == CClipEventTags::Foot)
				{
					bool bFootRight = false;
					const crPropertyAttribute* pRightAttr = pTag->GetProperty().GetAttribute(CClipEventTags::Right);
					if(taskVerifyf(pRightAttr && pRightAttr->GetType() == crPropertyAttribute::kTypeBool, "Movement clip [%s] from dictionary [%s]: Foot tag does not have a right property, add bug for Default Anim In-Game", clipName, dictionaryName))
					{
						bFootRight = static_cast<const crPropertyAttributeBool*>(pRightAttr)->GetBool();
					}

					if(iFlags & VCF_HasHeelProperty)
					{
						const crPropertyAttribute* pHeelAttr = pTag->GetProperty().GetAttribute(CClipEventTags::Heel);
						if(!pHeelAttr || pHeelAttr->GetType() != crPropertyAttribute::kTypeBool || !static_cast<const crPropertyAttributeBool*>(pHeelAttr)->GetBool())
						{
							taskAssertf(0, "Movement clip [%s] from dictionary [%s]: Foot tag is not a heel tag, add bug for Default Anim In-Game", clipName, dictionaryName);
						}
					}

					if(iFlags & VCF_ZeroDuration)
					{
						if(!IsClose(pTag->GetDuration(), 0.f, 0.0001f))
						{
							taskAssertf(0, "Movement clip [%s] from dictionary [%s]: Foot tag has duration (%f -> %f = %f), add bug for Default Anim In-Game", clipName, dictionaryName, pTag->GetStart(), pTag->GetEnd(), pTag->GetDuration());
						}
					}

					if(iFlags & VCF_HasAlternatingFootTags)
					{
						if(uFootTagCount != 0 && bFootRight == bLastFootRight)
						{
							taskAssertf(0, "Movement clip [%s] from dictionary [%s]: Foot tags are not alternating left/right, add bug for Default Anim In-Game", clipName, dictionaryName);
						}

						bLastFootRight = bFootRight;
					}

					if(bFirstFootTag)
					{
						if(iFlags & VCF_FootTagOnPhase0)
						{
							if(!IsClose(pTag->GetStart(), 0.f, 0.01f))
							{
								taskAssertf(0, "Movement clip [%s] from dictionary [%s]: First foot tag does not start on phase 0 [%.2f] - essentially wasted memory, add bug for Default Anim In-Game", clipName, dictionaryName, pTag->GetStart());
							}
						}

						if(iFlags & VCF_FirstFootTagRight)
						{
							if(!bFootRight)
							{
								taskAssertf(0, "Movement clip [%s] from dictionary [%s]: First foot tag is not a right foot, add bug for Default Anim In-Game", clipName, dictionaryName);
							}
						}

						if(iFlags & VCF_FirstFootTagLeft)
						{
							if(bFootRight)
							{
								taskAssertf(0, "Movement clip [%s] from dictionary [%s]: First foot tag is not a left foot, add bug for Default Anim In-Game", clipName, dictionaryName);
							}
						}

						bFirstFootTag = false;
					}

					++uFootTagCount;
				}
				++it;
			}

			if(iFlags & VCF_HasFootTags)
			{
				if(uFootTagCount == 0)
				{
					taskAssertf(0, "Movement clip [%s] from dictionary [%s]: No Foot tags, add bug for Default Anim In-Game", clipName, dictionaryName);
				}
			}

			if(iFlags & VCF_HasEvenNumberOfFootTags)
			{
				if((uFootTagCount % 2) != 0)
				{
					taskAssertf(0, "Movement clip [%s] from dictionary [%s]: Uneven number of Foot tags, add bug for Default Anim In-Game", clipName, dictionaryName);
				}
			}
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskHumanLocomotion::VerifyClips(const fwMvClipSetId& clipSetId, const atArray<sVerifyClip>& clips, const CPed *pPed)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(pClipSet)
	{
		s32 iNumClips = clips.GetCount();
		for(s32 i = 0; i < iNumClips; i++)
		{
			const crClip* pClip = NULL;
			// Try to work out which dictionary the clip is in
			const fwClipSet* pSet = pClipSet;
			while(pSet)
			{
				pClip = pSet->GetClip(clips[i].clipId, CRO_NONE);
				if(pClip)
					break;
				pSet = pSet->GetFallbackSet();
			}
			if(!pSet)
				pSet = pClipSet;

			VerifyClip(pClip, clips[i].iFlags, clips[i].clipId.TryGetCStr(), pSet->GetClipDictionaryName().TryGetCStr(), pPed);
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskHumanLocomotion::VerifyMovementClipSet(const fwMvClipSetId clipSetId, fwFlags32 flags, fwMvClipSetId genericMovementClipSetId, const CPed *pPed)
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", clipSetId.TryGetCStr()))
		{
			atArray<sVerifyClip> clips;
			// Idles
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_l_-180",0x403DED9D), VCF_MovementOneShot));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_l_-90",0xFC901F13), VCF_MovementOneShot));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_l_0",0x8FA77A49), VCF_MovementOneShot));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_r_0",0x48C18863), VCF_MovementOneShot));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_r_90",0xE993902E), VCF_MovementOneShot));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("idle_turn_r_180",0x352AA3F0), VCF_MovementOneShot));
			// Cycles
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk",0x83504C9C), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_l3",0xB20D5197), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_l4",0xC2F7F36C), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_r3",0xDF98B401), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_r4",0x2DE2D094), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run",0x1109B569), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_l2",0xDE3E7A4D), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_l3",0x5E6AFAB4), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_r2",0xF481A063), VCF_MovementCycle));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_r3",0x40E73929), VCF_MovementCycle));
			// Starts
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_l_-180",0x48498A76), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_l_-90",0x50272C1B), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_l_0",0x3FF9220B), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_r_0",0xCF6EC504), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_r_90",0x6F55D73E), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstart_r_180",0x68FBDBE6), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_l_-180",0x2E0F4E6E), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_l_-90",0x5DF2D952), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_l_0",0xB08AA2E8), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_r_0",0x60DC11D8), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_r_90",0xD7F8937), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstart_r_180",0x3657B38C), VCF_MovementOneShotRight));
			// Stops
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_l_-180",0xBC56B178), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_l_-90",0x84AF46C6), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_l_0",0xED71B76C), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_l_90",0x5BA7D542), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_l_180",0x7B394199), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_r_-180",0x2B0CA6DA), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_r_-90",0xA68C84A0), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_r_0",0x757B7872), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_r_90",0xC9D4D418), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_r_180",0xACED7232), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstop_l",0xE53DB8E9), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("rstop_r",0x3FC66DF9), VCF_MovementOneShotRight));
			// Transitions
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walktorun_left",0x9B24E8F), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walktorun_right",0x89225824), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("runtowalk_left",0xBBA97AC3), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("runtowalk_right",0x5BCA1366), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_180_l",0xBB5A7F9D), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("walk_turn_180_r",0xA481D7B), VCF_MovementOneShotRight));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_180_l",0x35B3141), VCF_MovementOneShotLeft));
			clips.PushAndGrow(sVerifyClip(fwMvClipId("run_turn_180_r",0x2703F892), VCF_MovementOneShotRight));

			if(!CheckClipSetForMoveFlag(clipSetId, ms_NoSprintId))
			{
				clips.PushAndGrow(sVerifyClip(fwMvClipId("sprint",0xBC29E48), VCF_MovementCycle));
				clips.PushAndGrow(sVerifyClip(fwMvClipId("sprint_turn_l",0x6E3229E3), VCF_MovementCycle));
				clips.PushAndGrow(sVerifyClip(fwMvClipId("sprint_turn_r",0x340CB599), VCF_MovementCycle));
			}

			if(CheckClipSetForMoveFlag(clipSetId, ms_UseSuddenStopsId) || (!flags.IsFlagSet(Flag_BlockGenericFallback) && CheckClipSetForMoveFlag(genericMovementClipSetId, ms_UseSuddenStopsId)))
			{
				clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_quick_l_0",0x41C5E881), VCF_MovementOneShotLeft));
				clips.PushAndGrow(sVerifyClip(fwMvClipId("wstop_quick_r_0",0xF50AB709), VCF_MovementOneShotRight));
				clips.PushAndGrow(sVerifyClip(fwMvClipId("rstop_quick_l",0xD4D7399E), VCF_MovementOneShotLeft));
				clips.PushAndGrow(sVerifyClip(fwMvClipId("rstop_quick_r",0x67855EF8), VCF_MovementOneShotRight));
			}

			VerifyClips(clipSetId, clips, pPed);
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskHumanLocomotion::VerifyWeaponClipSet()
{
	if(m_WeaponClipSetId != CLIP_SET_ID_INVALID)
	{
		atArray<sVerifyClip> clips;
		// Cycles
		clips.PushAndGrow(sVerifyClip(fwMvClipId("walk",0x83504C9C), VCF_MovementCycle));
		clips.PushAndGrow(sVerifyClip(fwMvClipId("run",0x1109B569), VCF_MovementCycle));
		VerifyClips(m_WeaponClipSetId, clips, GetEntity() ? GetPed() : NULL);
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

#if __DEV
void CTaskHumanLocomotion::LogMovingVariables()
{
	TUNE_GROUP_BOOL(PED_MOVEMENT, LOG_MOVING_PARAMS, false);
	if(LOG_MOVING_PARAMS)
	{
		if(g_PickerManager.GetNumberOfEntities() > 0)
		{
			CPed* pPed = GetPed();
			if(static_cast<CEntity*>(g_PickerManager.GetEntity(0)) == pPed)
			{
				const float fDesiredDirection = CalcDesiredDirection();
				const float fDirectionDiff = SubtractAngleShorter(fDesiredDirection, m_fMovingDirection);

				Warningf("%d: m_fMovingDirection %.2f, m_fMovingDirectionRate %.2f, fDesiredDirection %.2f, fDirectionDiff %.2f, m_fExtraHeadingRate %.2f, ExtraHeadingChangeThisFrame %.2f", 
					fwTimer::GetFrameCount(), 
					m_fMovingDirection, 
					m_fMovingDirectionRate, 
					fDesiredDirection, 
					fDirectionDiff, 
					m_fExtraHeadingRate, 
					pPed->GetMotionData()->GetExtraHeadingChangeThisFrame());
			}
		}
	}
}
#endif // __DEV

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::RecordStateTransition()
{
	for(s32 i = MAX_ACTIVE_STATE_TRANSITIONS-1; i > 0; i--)
	{
		m_uStateTransitionTimes[i] = m_uStateTransitionTimes[i-1];
	}
	m_uStateTransitionTimes[0] = fwTimer::GetTimeInMilliseconds();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskHumanLocomotion::CanChangeState() const
{
	static dev_u32 TIME_BEFORE_STATE_CHANGE = 500;
	return (m_uStateTransitionTimes[MAX_ACTIVE_STATE_TRANSITIONS-1] + TIME_BEFORE_STATE_CHANGE) <= fwTimer::GetTimeInMilliseconds();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::ComputeIdleTurnSignals()
{
	static dev_float IDLE_TURN_HEADING_RATE = PI;
	Approach(m_fStartDirection, m_fStartDirectionTarget, IDLE_TURN_HEADING_RATE, fwTimer::GetTimeStep());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHumanLocomotion::SendIdleTurnSignals()
{
	const CPed* pPed = GetPed();

	// Send the signals
	m_MoveNetworkHelper.SetFloat(ms_IdleTurnDirectionId, ConvertRadianToSignal(m_fStartDirection));
	m_MoveNetworkHelper.SetFloat(ms_DesiredDirectionId, ConvertRadianToSignal(CalcDesiredDirection()));

	// Anim rate
	float fAnimRate;
	if(pPed->IsPlayer())
	{
		fAnimRate = ms_Tunables.Player_IdleTurnRate;

		if(CAnimSpeedUps::ShouldUseMPAnimRates())
		{
			fAnimRate *= CAnimSpeedUps::ms_Tunables.m_MultiplayerIdleTurnRateModifier;
		}
	}
	else
	{
		fAnimRate = ms_Tunables.AI_IdleTurnRate;

		static dev_float MIN_RATE = 0.9f;
		static dev_float MAX_RATE = 1.1f;
		fAnimRate *= MIN_RATE + (MAX_RATE-MIN_RATE)*((float)pPed->GetRandomSeed())/((float)RAND_MAX_16);
	}
	m_MoveNetworkHelper.SetFloat(ms_IdleTurnRateId, fAnimRate);
}

////////////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
void CTaskHumanLocomotion::ComputeAndSendPitchedWeaponHolding()
{
	CPed* pPed = GetPed();
	if(m_Flags.IsFlagSet(Flag_HasPitchedWeaponHolding) && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_MoveNetworkHelper.SetBoolean(ms_UsePitchedWeaponHoldingId, true);

		float fDesiredPitch = CUpperBodyAimPitchHelper::ComputeDesiredPitchSignal(*pPed);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, fPitchLerp, 3.0f, 0.01f, 8.0f, 0.1f);
		Approach(m_fCurrentPitch, fDesiredPitch, fPitchLerp, fwTimer::GetTimeStep());
		m_MoveNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitch);
	}
}
#endif // FPS_MODE_SUPPORTED

////////////////////////////////////////////////////////////////////////////////
