//
// Task/Motion/Locomotion/TaskMotionAiming.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "TaskMotionAiming.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "math/angmath.h"

// Game Headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "Animation/AnimManager.h"
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/FirstPersonPedAimCamera.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "ModelInfo/PedModelInfo.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Objects/Prediction/NetBlenderPed.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedIntelligence.h"
#include "Scene/playerswitch/PlayerSwitchInterface.h"
#include "Task/Default/TaskPlayer.h"
#include "task/Combat/TaskCombatMelee.h"
#include "Task/Motion/Locomotion/TaskCombatRoll.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Info/WeaponSwapData.h"
#include "Task/Movement/TaskTakeOffPedVariation.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Weapons/TaskWeaponBlocked.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

CTaskMotionAiming::Tunables CTaskMotionAiming::ms_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskMotionAiming, 0x5e9029bd);

NOSTRIP_PC_XPARAM(mouse_sensitivity_override);

////////////////////////////////////////////////////////////////////////////////

dev_float CTaskMotionAiming::MIN_ANGLE = 0.25f;
dev_float CTaskMotionAiming::BWD_FWD_BORDER_MIN = -PI * 0.75f;
dev_float CTaskMotionAiming::BWD_FWD_BORDER_MAX = CTaskMotionAiming::BWD_FWD_BORDER_MIN + PI;

// OnFootStrafingTagSync signals
const fwMvRequestId CTaskMotionAiming::ms_IdleIntroId("IdleIntro",0x822A9119);
const fwMvRequestId CTaskMotionAiming::ms_IdleId("Idle",0x71C21326);
const fwMvRequestId CTaskMotionAiming::ms_MovingIntroId("MovingIntro",0xDF5FCCD1);
const fwMvRequestId CTaskMotionAiming::ms_StartStrafingId("Start",0x84DC271F);
const fwMvRequestId CTaskMotionAiming::ms_MovingId("Moving",0xDF719C6C);
const fwMvRequestId CTaskMotionAiming::ms_StopStrafingId("Stop",0x930CA7F2);
const fwMvRequestId CTaskMotionAiming::ms_CoverStepId("CoverStep",0x91AF1AFD);
const fwMvRequestId CTaskMotionAiming::ms_RollId("Roll",0xCB2448C6);
const fwMvRequestId CTaskMotionAiming::ms_Turn180Id("Turn180",0x4fdcf090);
const fwMvRequestId CTaskMotionAiming::ms_MovingFwdId("MovingFwd",0x479a38bb);
const fwMvRequestId CTaskMotionAiming::ms_MovingBwdId("MovingBwd",0xa3fa10f2);
const fwMvRequestId CTaskMotionAiming::ms_IndependentMoverRequestId("IndependentMoverExpression");
const fwMvBooleanId CTaskMotionAiming::ms_IdleIntroOnEnterId("OnEnterIdleIntro",0xBFAFAB7F);
const fwMvBooleanId CTaskMotionAiming::ms_IdleOnEnterId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskMotionAiming::ms_MovingIntroOnEnterId("OnEnterMovingIntro",0x30709793);
const fwMvBooleanId CTaskMotionAiming::ms_StartStrafingOnEnterId("OnEnterStart",0xA45362DF);
const fwMvBooleanId CTaskMotionAiming::ms_StartStrafingOnExitId("OnExitStart",0xCA86311D);
const fwMvBooleanId CTaskMotionAiming::ms_MovingOnEnterId("OnEnterMoving",0x23289D8);
const fwMvBooleanId CTaskMotionAiming::ms_MovingOnExitId("OnExitMoving",0x2B165D43);
const fwMvBooleanId CTaskMotionAiming::ms_MovingTransitionOnEnterId("OnEnterMovingTransition",0x59b56f9b);
const fwMvBooleanId CTaskMotionAiming::ms_MovingTransitionOnExitId("OnExitMovingTransition",0x582ec363);
const fwMvBooleanId CTaskMotionAiming::ms_StopStrafingOnEnterId("OnEnterStop",0x66D6AF9F);
const fwMvBooleanId CTaskMotionAiming::ms_RollOnEnterId("OnEnterRoll",0xF4E3661D);
const fwMvBooleanId CTaskMotionAiming::ms_Turn180OnEnterId("OnEnterTurn180",0xdd79c67a);
const fwMvBooleanId CTaskMotionAiming::ms_BlendOutIdleIntroId("BLEND_OUT_IDLE_INTRO",0x895645CC);
const fwMvBooleanId CTaskMotionAiming::ms_BlendOutStartId("BLEND_OUT_START",0x6A82789F);
const fwMvBooleanId CTaskMotionAiming::ms_BlendOutStopId("BLEND_OUT_STOP",0xFC86B701);
const fwMvBooleanId CTaskMotionAiming::ms_BlendOutTurn180Id("BLEND_OUT_180",0x5988ce8b);
const fwMvBooleanId CTaskMotionAiming::ms_CanEarlyOutForMovementId("CAN_EARLY_OUT_FOR_MOVEMENT",0x7E1C8464);
const fwMvBooleanId CTaskMotionAiming::ms_CoverStepOnEnterId("OnEnterCoverStep",0xA7234732);
const fwMvBooleanId CTaskMotionAiming::ms_CoverStepClipFinishedId("CoverStepClipFinished",0xF73D2AB7);
const fwMvBooleanId CTaskMotionAiming::ms_LeftFootStrafeTransitionId("USE_LEFT_FOOT_STRAFE_TRANSITION",0xC9AF24C2);
const fwMvBooleanId CTaskMotionAiming::ms_WalkingId("Walking",0x36EFDB93);
const fwMvBooleanId CTaskMotionAiming::ms_RunningId("Running",0x68AB84B5);
const fwMvFloatId CTaskMotionAiming::ms_DesiredStrafeDirectionId("DesiredStrafeDirection",0x3C9615E9);
const fwMvFloatId CTaskMotionAiming::ms_StrafeDirectionId("StrafeDirection",0xCF6AA9C6);
const fwMvFloatId CTaskMotionAiming::ms_DesiredStrafeSpeedId("DesiredStrafeSpeed",0x8E945F2D);
const fwMvFloatId CTaskMotionAiming::ms_StrafeSpeedId("StrafeSpeed",0x59A5FA46);
const fwMvFloatId CTaskMotionAiming::ms_DesiredHeadingId("DesiredHeading",0x5B256E29);
const fwMvFloatId CTaskMotionAiming::ms_IdleTurnRateId("TurnRate",0x58AF9DCD);
const fwMvFloatId CTaskMotionAiming::ms_AnimRateId("AnimRate",0x77F5D0B5);
const fwMvFloatId CTaskMotionAiming::ms_IdleIntroAnimRateId("IdleIntroAnimRate",0x3773AB23);
const fwMvFloatId CTaskMotionAiming::ms_IdleTurnAnimRateId("IdleTurnAnimRate",0xF486DE23);
const fwMvFloatId CTaskMotionAiming::ms_AimingTransitionRateId("AimingTransitionRate",0x4F294F5C);
const fwMvFloatId CTaskMotionAiming::ms_CoverStepClipPhaseId("CoverStepClipPhase",0x6550CE5B);
const fwMvFloatId CTaskMotionAiming::ms_MovementRateId("MovementRate",0xE654C106);
const fwMvFloatId CTaskMotionAiming::ms_Turn180PhaseId("Turn180Phase",0xd37d52f4);
const fwMvFlagId CTaskMotionAiming::ms_SkipIdleIntroId("SkipIdleIntro",0x364ABF91);
const fwMvFlagId CTaskMotionAiming::ms_SkipStartsAndStopsId("SkipStartsAndStops",0xEC756D02);
const fwMvFlagId CTaskMotionAiming::ms_SkipTurn180Id("SkipTurn180",0xc68c27d4);
const fwMvFlagId CTaskMotionAiming::ms_SkipMovingTransitionsId("SkipMovingTransitions",0x41808889);
const fwMvFlagId CTaskMotionAiming::ms_LastFootLeftId("LastFootLeft",0x7A2EB41B);
const fwMvFlagId CTaskMotionAiming::ms_BlockMovingTransitionsId("BlockMovingTransitions",0x2362901c);
const fwMvFlagId CTaskMotionAiming::ms_IsPlayerId("IsPlayer",0x691c1a48);
const fwMvFlagId CTaskMotionAiming::ms_IsRemotePlayerUsingFPSModeId("IsRemotePlayerUsingFPSMode", 0x6d5fdc05);
#if FPS_MODE_SUPPORTED
const fwMvFlagId CTaskMotionAiming::ms_FirstPersonMode("FirstPersonMode",0x8BB6FFFA);
const fwMvFlagId CTaskMotionAiming::ms_FirstPersonModeIK("FirstPersonModeIK",0xA523AEC9);
const fwMvFlagId CTaskMotionAiming::ms_FirstPersonAnimatedRecoil("FirstPersonAnimatedRecoil",0xF18888A0);
const fwMvFlagId CTaskMotionAiming::ms_FirstPersonSecondaryMotion("FirstPersonSecondaryMotion",0x6287D547);
const fwMvFlagId CTaskMotionAiming::ms_BlockAdditives("BlockAdditives",0xE6BDCD2D);
const fwMvFlagId CTaskMotionAiming::ms_FPSUseGripAttachment("FPSUseGripAttachment", 0x2D22C028);
const fwMvFlagId CTaskMotionAiming::ms_SwimmingId("FPSSwimming", 0xf83afe2f);	//Used in OnFootAiming.mxtf to toggle MovingSM->Swimming
const fwMvBooleanId CTaskMotionAiming::ms_FPSAimIntroTransitionId("FPSAimIntroTransition", 0x7CA6884C);
#endif // FPS_MODE_SUPPORTED
const fwMvNetworkId CTaskMotionAiming::ms_MovingIntroNetworkId("MovingIntroNetwork",0x1E5C4877);
const fwMvNetworkId CTaskMotionAiming::ms_RollNetworkId("RollNetwork",0xB4B9C087);
const fwMvClipId CTaskMotionAiming::ms_Walk0ClipId("Walk_Fwd_0_Loop",0x90FA019A);
const fwMvClipId CTaskMotionAiming::ms_Run0ClipId("Run_Fwd_0_Loop",0x5794E6CE);
const fwMvClipId CTaskMotionAiming::ms_CoverStepClipId("CoverStepClip",0x5A44C304);
const fwMvClipId CTaskMotionAiming::ms_Turn180ClipId("Turn180Clip",0x6fec2ba2);
// MotionAiming stop signals
const fwMvClipId CTaskMotionAiming::ms_StopClipBwd_N_45Id("StopClipBwd_N_45",0x43F95935);
const fwMvClipId CTaskMotionAiming::ms_StopClipBwd_N_90Id("StopClipBwd_N_90",0x45A5DC0A);
const fwMvClipId CTaskMotionAiming::ms_StopClipBwd_N_135Id("StopClipBwd_N_135",0x2EADE840);
const fwMvClipId CTaskMotionAiming::ms_StopClipBwd_180Id("StopClipBwd_180",0x8696881F);
const fwMvClipId CTaskMotionAiming::ms_StopClipBwd_135Id("StopClipBwd_135",0xCE551ACB);
const fwMvClipId CTaskMotionAiming::ms_StopClipFwd_N_45Id("StopClipFwd_N_45",0x40FEB07F);
const fwMvClipId CTaskMotionAiming::ms_StopClipFwd_0Id("StopClipFwd_0",0xA6138F8D);
const fwMvClipId CTaskMotionAiming::ms_StopClipFwd_90Id("StopClipFwd_90",0xB15BDADE);
const fwMvClipId CTaskMotionAiming::ms_StopClipFwd_135Id("StopClipFwd_135",0xBB99528E);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseBwd_N_45Id("StopPhaseBwd_N_45",0x46F674D6);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseBwd_N_90Id("StopPhaseBwd_N_90",0xD7370D91);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseBwd_N_135Id("StopPhaseBwd_N_135",0xC2290C71);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseBwd_180Id("StopPhaseBwd_180",0xF50222E6);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseBwd_135Id("StopPhaseBwd_135",0x58034CA3);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseFwd_N_45Id("StopPhaseFwd_N_45",0x4686D818);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseFwd_0Id("StopPhaseFwd_0",0xBF4E33BF);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseFwd_90Id("StopPhaseFwd_90",0xC8C31073);
const fwMvFloatId CTaskMotionAiming::ms_StopPhaseFwd_135Id("StopPhaseFwd_135",0x8D6FE1C8);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendBwd_N_45Id("StopBlendBwd_N_45",0x810BC933);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendBwd_N_90Id("StopBlendBwd_N_90",0x2D9EA27A);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendBwd_N_135Id("StopBlendBwd_N_135",0x4720996B);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendBwd_180Id("StopBlendBwd_180",0xC7A68906);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendBwd_135Id("StopBlendBwd_135",0x80D4EEE0);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendFwd_N_45Id("StopBlendFwd_N_45",0xF00F339F);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendFwd_0Id("StopBlendFwd_0",0x6E1625B6);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendFwd_90Id("StopBlendFwd_90",0x4C23D298);
const fwMvFloatId CTaskMotionAiming::ms_StopBlendFwd_135Id("StopBlendFwd_135",0x87F8E97);
// TaskAimGunOnFoot signals
const fwMvRequestId CTaskMotionAiming::ms_AimId("Aim",0xB01E2F36);
const fwMvRequestId CTaskMotionAiming::ms_UseAimingIntroId("UseAimingIntro",0xE16945B2);
const fwMvRequestId CTaskMotionAiming::ms_UseAimingIntroFPSId("UseAimingIntroFPS",0x473a4847);
const fwMvRequestId CTaskMotionAiming::ms_UseAimingFidgetFPSId("UseAimingFidgetFPS",0x6dab4680);
const fwMvRequestId CTaskMotionAiming::ms_AimingIntroFinishedId("AimingIntroFinished",0x5E7C6C3E);
const fwMvBooleanId CTaskMotionAiming::ms_FireLoop1("FireLoop1Finished",0x4C274860);
const fwMvBooleanId CTaskMotionAiming::ms_FireLoop2("FireLoop2Finished",0x572B2974);
const fwMvBooleanId CTaskMotionAiming::ms_CustomFireLoopFinished("CustomFireLoopFinished",0x786B8E90);
const fwMvBooleanId CTaskMotionAiming::ms_CustomFireIntroFinished("FireIntroFinished",0xC296CA42);
const fwMvBooleanId CTaskMotionAiming::ms_CustomFireOutroFinishedId("FireOutroFinished",0xD4ED7176);
const fwMvBooleanId CTaskMotionAiming::ms_CustomFireOutroBlendOut("FireOutroBlendOut",0x273797D9);
const fwMvBooleanId CTaskMotionAiming::ms_DontAbortFireIntroId("DontAbortFireIntro",0xF254FC66);
const fwMvFloatId CTaskMotionAiming::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvFloatId CTaskMotionAiming::ms_NormalizedPitchId("NormalizedPitch",0xfc36cc6b);
const fwMvFloatId CTaskMotionAiming::ms_AimIntroDirectionId("AimIntroDirection",0xE184226);
const fwMvFloatId CTaskMotionAiming::ms_AimIntroRateId("AimIntroRate",0xE4706446);
const fwMvFloatId CTaskMotionAiming::ms_AimIntroPhaseId("AimIntroPhase",0xC21FBDCC);
const fwMvFlagId CTaskMotionAiming::ms_HasIntroId("HasIntro",0x3AE3C1C0);
// PedMotion signals
const fwMvRequestId CTaskMotionAiming::ms_AimingTransitionId("AimingTransition",0xA5328A39);
const fwMvRequestId CTaskMotionAiming::ms_StationaryAimingPoseId("StationaryAimingPose",0x1AC8930D);
const fwMvRequestId CTaskMotionAiming::ms_AimTurnId("AimTurn",0xFD5DF6B1);
const fwMvBooleanId CTaskMotionAiming::ms_AimTurnAnimFinishedId("AimTurnAnimFinished",0xC2CA0A10);
const fwMvFloatId CTaskMotionAiming::ms_StrafeSpeed_AdditiveId("StrafeSpeed_Additive",0xb4f01ffa);
const fwMvFloatId CTaskMotionAiming::ms_StrafeDirection_AdditiveId("StrafeDirection_Additive",0xf72f7955);
const fwMvBooleanId CTaskMotionAiming::ms_TransitionAnimFinishedId("TransitionAnimFinished",0x96E8AE49);
const fwMvBooleanId CTaskMotionAiming::ms_OnEnterAimingTransitionId("OnEnter_AimingTransition",0x1B8EC547);
const fwMvFloatId CTaskMotionAiming::ms_AimTurnBlendFactorId("AimTurnBlendFactor",0xC72C7929);
const fwMvFloatId CTaskMotionAiming::ms_AimingBlendFactorId("AimingBlendFactor",0x33E2269E);
const fwMvFloatId CTaskMotionAiming::ms_AimingBlendDurationId("AimingBlendDuration",0xB53FA88);
const fwMvClipId CTaskMotionAiming::ms_AimingTransitionClipId("AimingTransitionClip",0xE606EB3C);
const fwMvClipId CTaskMotionAiming::ms_StationaryAimingPoseClipId("StationaryAimingPoseClip",0xB011E072);
// Aiming Underwater signals:
//OnFootAiming network:
const fwMvRequestId CTaskMotionAiming::ms_SwimIdleId("SwimIdle",0xcc974700);
const fwMvBooleanId CTaskMotionAiming::ms_SwimIdleOnEnterId("OnEnterSwimIdle",0x85065b20);
const fwMvRequestId CTaskMotionAiming::ms_SwimStrafeId("SwimStrafe",0x5277bb5d);
const fwMvFloatId CTaskMotionAiming::ms_SwimStrafePitchId("SwimStrafePitch",0xf5f3c5a7);
const fwMvFloatId CTaskMotionAiming::ms_SwimStrafeRollId("SwimStrafeRoll",0x04f52bc9);
//TaskAimGunOnFoot network:
const fwMvRequestId CTaskMotionAiming::ms_UseSwimIdleIntroId("UseSwimIdleIntro",0x572b420d);
const fwMvRequestId CTaskMotionAiming::ms_UseSwimIdleOutroId("UseSwimIdleOutro",0xc049f399);
const fwMvClipId CTaskMotionAiming::ms_SwimIdleIntroTransitionClipId("SwimIdleIntroTransitionClip",0x41df12c0);
const fwMvFloatId CTaskMotionAiming::ms_SwimIdleTransitionPhaseId("SwimIdleTransitionPhase", 0xf062cbe9);
const fwMvFloatId CTaskMotionAiming::ms_SwimIdleTransitionOutroPhaseId("SwimIdleOutroTransitionPhase", 0x8f4e4a4e);
const fwMvBooleanId CTaskMotionAiming::ms_SwimIdleTransitionFinishedId("SwimIdleTransitionFinished",0x37fc82e1);

const fwMvClipSetVarId CTaskMotionAiming::ms_FPSAimingIntroClipSetId("FPSAimingIntroClipSet", 0xaaae5666);
const fwMvClipSetVarId CTaskMotionAiming::ms_FPSAdditivesClipSetId("FPSAdditivesClipSet", 0x820A6952);

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::CTaskMotionAiming(const bool bIsCrouched, const bool bLastFootLeft, const bool bUseLeftFootStrafeTransition, const bool bBlockTransition, CTaskMotionAimingTransition::Direction direction, const float fIdleDir, const bool bBlockIndependentMover, const float fPreviousVelocityChange, const float fHeightBelowWater, const float fHeightBelowWaterGoal, const float FPS_MODE_SUPPORTED_ONLY(fNormalisedVel), const bool bBlendInSetHeading)
: 
#if FPS_MODE_SUPPORTED
m_vLastSlopePosition(Vector3::ZeroType),
#endif // FPS_MODE_SUPPORTED
m_VelDirection(0.f, 0.f)
, m_AimingClipSetId(CLIP_SET_ID_INVALID)
, m_FPSAimingIntroClipSetId(CLIP_SET_ID_INVALID)
, m_fFPSAimingIntroBlendOutTime(0.15f)
, m_AimingClipId(CLIP_ID_INVALID)
, m_ParentClipSetId(CLIP_SET_ID_INVALID)
, m_fDesiredPitch(-1.0f)
, m_fCurrentPitch(-1.0f)
, m_AimClipSetId(CLIP_SET_ID_INVALID)
, m_fStrafeSpeed(-1.0f)
, m_fDesiredStrafeDirection(-1.0f)
, m_fStrafeDirection(FLT_MAX)
, m_fStartDirection(0.0f)
, m_fStartDesiredHeading(0.0f)
, m_fStopDirection(0.0f)
, m_fStandingIdleExtraTurnRate(0.0f)
, m_OverwriteMaxPitchChange(-1.0f)
, m_fAimIntroInitialAimHeading(FLT_MAX)
, m_fAimIntroInitialDirection(FLT_MAX)
, m_fAimIntroInterpolatedDirection(0.0f)
, m_fAimIntroDirection(FLT_MAX)
, m_fAimIntroPhase(0.0f)
, m_fAimIntroRate(0.0f)
, m_fMovementRate(1.0f)
, m_fWalkMovementRate(1.0f)
, m_fPreviousAimingBlendFactor(FLT_MAX)
, m_fPitchLerp(0.0f)
, m_fStrafeSpeedAdditive(0.f)
, m_fStrafeDirectionAdditive(0.f)
, m_iMovingStopCount(0)
, m_fStrafeDirectionAtStartOfTransition(0.f)
, m_TransitionDirection(direction)
, m_fExtraHeadingChange(0.f)
, m_fInitialIdleDir(fIdleDir)
, m_fVelLerp(0.f)
#if FPS_MODE_SUPPORTED
, m_fNormalisedVel(fNormalisedVel)
, m_fNormalisedStopVel(0.f)
#endif // FPS_MODE_SUPPORTED
, m_fWalkVel(0.f)
, m_fRunVel(0.f)
#if FPS_MODE_SUPPORTED
, m_fOnFootWalkVel(0.f)
, m_fOnFootRunVel(0.f)
, m_fFPSVariableRateIntroBlendInDuration(-1.0f)
, m_fFPSVariableRateIntroBlendInTimer(-1.0f)
, m_uLastSlopeScanTime(0)
, m_uStairsDetectedTime(0)
, m_fSmoothedSnowDepth(0.f)
, m_fSnowDepthSignal(0.f)
#endif // FPS_MODE_SUPPORTED
, m_fCameraTurnSpeed(0.f)
, m_uCameraHeadingLerpTime(UINT_MAX)
, m_bEnteredAimState(false)
, m_bEnteredFiringState(false)
, m_bEnteredAimIntroState(false)
, m_bSkipTransition(false)
, m_bUsesFireEvents(false)
, m_bIsCrouched(bIsCrouched)
, m_bNeedsRestart(false)
, m_bSkipIdleIntro(false)
, m_bUpdateStandingIdleIntroFromPostCamera(false)
, m_bUseLeftFootStrafeTransition(bUseLeftFootStrafeTransition)
, m_bBlockTransition(bBlockTransition)
, m_bAimIntroFinished(false)
, m_bSuppressHasIntroFlag(false)
, m_bAimIntroTimedOut(false)
, m_bWasRunning(false)
, m_bIsRunning(false)
, m_bLastFootLeft(bLastFootLeft)
, m_bFullyInStop(false)
, m_bMoveTransitionAnimFinished(false)
, m_bMoveAimOnEnterState(false)
, m_bMoveFiringOnEnterState(false)
, m_bMoveAimIntroOnEnterState(false)
, m_bMoveAimingClipEnded(false)
, m_bMoveWalking(false)
, m_bMoveRunning(false)
, m_bMoveCoverStepClipFinished(false)
, m_bMoveBlendOutIdleIntro(false)
, m_bMoveCanEarlyOutForMovement(false)
, m_bMoveBlendOutStart(false)
, m_bMoveBlendOutStop(false)
, m_bMoveBlendOutTurn180(false)
, m_bMoveStartStrafingOnExit(false)
, m_bMoveMovingOnExit(false)
, m_bIsPlayingCustomAimingClip(false)
, m_bNeedToSetAimClip(true)
, m_bSnapMovementWalkRate(false)
, m_bInStrafingTransition(false)
, m_bDoPostMovementIndependentMover(false)
, m_bSwitchActiveAtStart(false)
, m_bDoVelocityNormalising(false)
, m_bBlockIndependentMover(bBlockIndependentMover)
, m_PreviousVelocityChange(fPreviousVelocityChange)
, m_fHeightBelowWater(fHeightBelowWater)
, m_fHeightBelowWaterGoal(fHeightBelowWaterGoal)
, m_bSwimIntro(false)
, m_bPlayOutro(false)
, m_bOutroFinished(false)
, m_fSwimStrafePitch(0.0f)
, m_fSwimStrafeRoll(0.0f)
, m_vSwimStrafeSpeed(VEC3_ZERO)
, m_uLastFpsPitchUpdateTime(UINT_MAX)
, m_uLastDisableNormaliseDueToCollisionTime(0)
, m_bMoveFPSFidgetClipEnded(false)
, m_bMoveFPSFidgetClipNonInterruptible(false)
, m_bUsingMotionAimingWhileSwimmingFPS(false)
, m_bResetExtractedZ(false)
, m_fLastWaveDelta(0.0f)
#if FPS_MODE_SUPPORTED
, m_fDesiredSwimStrafeDirection(0.0f)
, m_fPrevPitch(0.0f)
, m_bWasUnarmed(false)
, m_fSwimStrafeGroundClearanceVelocity(0.0f)
, m_bFPSPlayToUnarmedTransition(false)
#endif	//FPS_MODE_SUPPORTED
, m_bBlendInSetHeading(bBlendInSetHeading)
ASSERT_ONLY(, m_bCanPedMoveForVerifyClips(false))
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_AIMING);

	m_pPreviousDesiredDirections = rage_new CPreviousDirections();
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::~CTaskMotionAiming()
{
	delete m_pPreviousDesiredDirections;
	m_pPreviousDesiredDirections = NULL;
}

//////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::GetStandingSpringStrength()
{
	// Taken from CTaskHumanLocomotion::GetStandingSpringStrength

	static dev_u32 TIME_AFTER_STAIRS_DETECTED = 250;

	float fSpringStrength = CTaskMotionBase::GetStandingSpringStrength();

	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
		{
			fSpringStrength *= GetPed()->GetIkManager().GetSpringStrengthForStairs();
		}
		else if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fSlopeSpringMultiplier, 3.0f, 0.01f, 8.0f, 0.1f);
			fSpringStrength *= fSlopeSpringMultiplier;
		}
		else if((m_uStairsDetectedTime + TIME_AFTER_STAIRS_DETECTED) > fwTimer::GetTimeInMilliseconds())
		{
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fAfterStairsSpringStrengthMultiplier, 6.0f, 0.01f, 10.0f, 0.1f);	
			fSpringStrength *= fAfterStairsSpringStrengthMultiplier;
		}
	}
	return fSpringStrength;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsValidForUnarmedIK(const CPed& ped)
{
	bool bPlayingIdleFidget = ped.GetMotionData()->GetIsFPSIdle() && ped.GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets);
	if(!ped.GetPedResetFlag(CPED_RESET_FLAG_FPSAllowAimIKForThrownProjectile) && !ped.GetMotionData()->GetIsFPSLT() && !ped.GetMotionData()->GetIsUsingStealthInFPS() && !bPlayingIdleFidget)
	{
		return false;
	}

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::CleanUp()
{
	ResetGaitReduction();
	
	if (GetPed()->GetUseExtractedZ() && m_bResetExtractedZ)
	{
		GetPed()->SetUseExtractedZ( false );
	}

#if FPS_MODE_SUPPORTED
	bool bFPSRestartToSwap = GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetPed()->GetMotionData()->GetWasFPSUnholster();
	if(!bFPSRestartToSwap)
	{
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, false);
	}

	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_UseFPSUnholsterTransitionDuringCombatRoll, false);

	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		CTaskHumanLocomotion::ResetSlopesAndStairs(GetPed(), m_vLastSlopePosition);
	}

	// Clear diving variation data
	CTaskMotionSwimming::OnPedNoLongerDiving(*GetPed());
#endif // FPS_MODE_SUPPORTED
}

//////////////////////////////////////////////////////////////////////////

// In this case we can query the locomotion clips directly, and possibly cache the results.
void CTaskMotionAiming::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	taskAssert(GetClipSet() != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(GetClipSet());
	taskAssert(clipSet);

	static fwMvClipId s_walkClip("Walk_Fwd_0_Loop",0x90FA019A);
	static fwMvClipId s_runClip("Run_Fwd_0_Loop",0x5794E6CE);
	static fwMvClipId s_sprintClip("Run_Fwd_0_Loop",0x5794E6CE);

	fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*clipSet, speeds, clipNames);

	return;
}

//////////////////////////////////////////////////////////////////////////

CTask* CTaskMotionAiming::CreatePlayerControlTask() 
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

////////////////////////////////////////////////////////////////////////////////


void CTaskMotionAiming::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	// By default don't allow any pitching
	fMinOut = 0.0f;
	fMaxOut = 0.0f;

#if FPS_MODE_SUPPORTED
	// FPS Swim: Lerp constraint limits so we don't get a pop switching from dive/swim->strafe
	if (GetPed()->GetIsFPSSwimming())
	{
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fPitchLerpRate, 0.33f, 0.0f, 1.0f, 0.01f);
		if (m_fPrevPitch < fMinOut)
		{
			m_fPrevPitch = Lerp(fPitchLerpRate, m_fPrevPitch, fMinOut);
		}
		else if (m_fPrevPitch > fMaxOut)
		{
			m_fPrevPitch = Lerp(fPitchLerpRate, m_fPrevPitch, fMaxOut);
		}
		fMaxOut = m_fPrevPitch;
	}
#endif	//FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* bOutIsClipActive)
{
	if(bOutIsClipActive)
	{
		*bOutIsClipActive = false;
	}

#if FPS_MODE_SUPPORTED
	// B*2075470: Early out if we're FPS swimming (should probably also check CTaskMotionAiming::GetUseStartsAndStops here but just fixing this isolated case for now so we don't break anything).
	if (GetPed() && GetPed()->GetIsFPSSwimming())
	{
		return 0.0f;
	}
#endif	//FPS_MODE_SUPPORTED

	// Early out if we're in the idle state
	switch(GetState())
	{
	case State_StartStrafing:
	case State_Strafing:
	case State_StopStrafing:
		break;
	default:
		return 0.f;
	}

	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		// If we are currently stopping, interrogate the anims for the stopping distance
		if(GetState() == State_StopStrafing)
		{
			static const int iClipsToBlend = 9;
			const crClip* pMoveStopClip[iClipsToBlend] = 
			{
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipBwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipBwd_N_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipBwd_N_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipBwd_180Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipBwd_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipFwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipFwd_0Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipFwd_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetClip(ms_StopClipFwd_135Id),
			};

			const float fMoveStopPhase[iClipsToBlend] = 
			{
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseBwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseBwd_N_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseBwd_N_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseBwd_180Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseBwd_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseFwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseFwd_0Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseFwd_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopPhaseFwd_135Id),
			};

			const float fMoveStopBlend[iClipsToBlend] = 
			{
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendBwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendBwd_N_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendBwd_N_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendBwd_180Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendBwd_135Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendFwd_N_45Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendFwd_0Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendFwd_90Id),
				m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_StopBlendFwd_135Id),
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

		const fwMvClipSetId clipSetId = m_MoveGunStrafingMotionNetworkHelper.GetClipSetId();
		if(clipSetId != CLIP_SET_ID_INVALID)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
			if(pClipSet)
			{
				// We have no active clips, estimate the distance from what we will play
				enum Foot { FOOT_LEFT, FOOT_RIGHT, NUM_FEET, };
				enum StopClips { WALK_BWD_N_45, WALK_BWD_N_90, WALK_BWD_N_135, WALK_BWD_180, WALK_BWD_135, WALK_FWD_N_45, WALK_FWD_0, WALK_FWD_90, WALK_FWD_135, NUM_STOP_CLIPS };

				static const fwMvClipId stopClips[NUM_FEET][NUM_STOP_CLIPS] =
				{
					{ fwMvClipId("WALK_BWD_-45_STOP_L",0x13EB6D7E), fwMvClipId("WALK_BWD_-90_STOP_L",0xCA176F49), fwMvClipId("WALK_BWD_-135_STOP_L",0xFFA236D8), fwMvClipId("WALK_BWD_180_STOP_L",0xA0CE6DCA), fwMvClipId("WALK_BWD_135_STOP_L",0x9AAF7977), fwMvClipId("WALK_FWD_-45_STOP_L",0x165D0E3C), fwMvClipId("WALK_FWD_0_STOP_L",0x675F8F79), fwMvClipId("WALK_FWD_90_STOP_L",0x9B596F0B), fwMvClipId("WALK_FWD_135_STOP_L",0x6F60C2C6) },
					{ fwMvClipId("WALK_BWD_-45_STOP_R",0x314AA848), fwMvClipId("WALK_BWD_-90_STOP_R",0xA5ADA676), fwMvClipId("WALK_BWD_-135_STOP_R",0xC59DC2D0), fwMvClipId("WALK_BWD_180_STOP_R",0x4FB4CB94), fwMvClipId("WALK_BWD_135_STOP_R",0x36233060), fwMvClipId("WALK_FWD_-45_STOP_R",0x258A2CA2), fwMvClipId("WALK_FWD_0_STOP_R",0xEA4E1540), fwMvClipId("WALK_FWD_90_STOP_R",0x17E3B57), fwMvClipId("WALK_FWD_135_STOP_R",0xB64D50A2) },
				};

				const int iFoot = (CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelLId) || !m_bLastFootLeft) ? FOOT_RIGHT : FOOT_LEFT;

				taskAssert(m_fStrafeDirection != FLT_MAX);

				fwMvClipId clip1, clip2;
				float fWeight;
				bool bUse45DegAssumption = true;
				if(m_fStrafeDirection > -(HALF_PI+QUARTER_PI) && m_fStrafeDirection < QUARTER_PI)
				{
					// Fwd
					if(m_fStrafeDirection < -HALF_PI)
					{
						fWeight = (m_fStrafeDirection + HALF_PI) / -QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_FWD_135];
						clip2 = stopClips[iFoot][WALK_FWD_90];
					}
					else if(m_fStrafeDirection < 0.f)
					{
						fWeight = m_fStrafeDirection / -HALF_PI;
						clip1 = stopClips[iFoot][WALK_FWD_90];
						clip2 = stopClips[iFoot][WALK_FWD_0];
						bUse45DegAssumption = false;
					}
					else
					{
						fWeight = m_fStrafeDirection / QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_FWD_N_45];
						clip2 = stopClips[iFoot][WALK_FWD_0];
					}
				}
				else
				{
					// Bwd
					if(m_fStrafeDirection < -(HALF_PI+QUARTER_PI))
					{
						fWeight = (m_fStrafeDirection + (HALF_PI+QUARTER_PI)) / -QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_BWD_180];
						clip2 = stopClips[iFoot][WALK_BWD_135];
					}
					else if(m_fStrafeDirection < HALF_PI)
					{
						fWeight = (m_fStrafeDirection - QUARTER_PI) / QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_BWD_N_90];
						clip2 = stopClips[iFoot][WALK_BWD_N_45];
					}
					else if(m_fStrafeDirection < (HALF_PI+QUARTER_PI))
					{
						fWeight = (m_fStrafeDirection - HALF_PI) / QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_BWD_N_135];
						clip2 = stopClips[iFoot][WALK_BWD_N_90];
					}
					else
					{
						fWeight = (m_fStrafeDirection - (HALF_PI+QUARTER_PI)) / QUARTER_PI;
						clip1 = stopClips[iFoot][WALK_BWD_180];
						clip2 = stopClips[iFoot][WALK_BWD_N_135];
					}
				}

				crClip* pClip1 = pClipSet->GetClip(clip1);
				crClip* pClip2 = pClipSet->GetClip(clip2);

				// We attempt to access cached data first as it is very expensive to poll the animation for this information
				float* pfDist1 = pClipSet->GetProperties().Access(clip1.GetHash());
				float* pfDist2 = pClipSet->GetProperties().Access(clip2.GetHash());

				float fDist1 = 0.f;
				if(pfDist1)
					fDist1 = *pfDist1;
				else if(taskVerifyf(pClip1, "[%s] Does not exist in ClipSet [%s]", clip1.TryGetCStr(), clipSetId.TryGetCStr()))
				{
					fDist1 = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip1, 0.f, 1.f).XYMag();
					pClipSet->GetProperties().Insert(clip1.GetHash(), fDist1);
				}

				float fDist2 = 0.f;

				if(pfDist2)
					fDist2 = *pfDist2;
				else if(taskVerifyf(pClip2, "[%s] Does not exist in ClipSet [%s]", clip2.TryGetCStr(), clipSetId.TryGetCStr()))
				{
					fDist2 = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip2, 0.f, 1.f).XYMag();
					pClipSet->GetProperties().Insert(clip2.GetHash(), fDist2);
				}

				float fStoppingDistance;
				if (bUse45DegAssumption)
				{
					// These anims are not perpendicular so we rotate one axis 45deg
					const float fDeltaY = fDist2 * (1.f - fWeight) * 0.707f;	// 45Deg
					const float fDeltaX = fDist1 * fWeight + fDeltaY;			// 45Deg
					fStoppingDistance = sqrt((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY));
				}
				else
				{
					// Perpendicular
					const float fDeltaX = fDist1 * fWeight;
					const float fDeltaY = fDist2 * (1.f - fWeight);
					fStoppingDistance = sqrt((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY));
				}

				static dev_float fStoppingDistExtra = 0.15f;

				// Assuming we got some stop distance to make this tolerance a reasonable value
				if(fStoppingDistance > 0.5f)
					return fStoppingDistance + fStoppingDistExtra;

				return fStoppingDistance;
			}
		}
	}

	return 0.f;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ClampVelocityChange(const CPed * pPed, Vector3& vChangeInAndOut, float fTimestep, float fChangeLimit)
{	
	if(!pPed->GetUseExtractedZ())
		vChangeInAndOut.z = 0.0f;

	if(vChangeInAndOut.Mag2() > fChangeLimit*fChangeLimit*fTimestep*fTimestep)
	{
#if DEBUG_PED_ACCELERATION_CLAMPING
		const Vector3 vOriginalChange = vChangeInAndOut;
#endif
		vChangeInAndOut *= fChangeLimit*fTimestep / vChangeInAndOut.Mag();
#if DEBUG_PED_ACCELERATION_CLAMPING
		Printf( "Acc capped : (%f, %f, %f) to (%f, %f, %f)\n", vOriginalChange.x, vOriginalChange.y, vOriginalChange.z, vChangeInAndOut.x, vChangeInAndOut.y, vChangeInAndOut.z );
		Printf( "Frame time (%f) change limit (%f) sq (%f)\n", fwTimer::GetTimeStep(), fChangeLimit*fwTimer::GetTimeStep(), fChangeLimit*fChangeLimit*fTimestep*fTimestep );
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::RestartUpperBodyAimNetwork(const fwMvClipSetId clipSetId)
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s RestartUpperBodyAimNetwork\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
	AI_LOG_STACK_TRACE(8);

	taskAssertf(clipSetId != CLIP_SET_ID_INVALID, "Invalid clipset");
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.ReleaseNetworkPlayer();
		if(taskVerifyf(SetUpAimingNetwork(), "Couldn't setup aiming network"))
		{
			SetAimClipSet(clipSetId);

#if FPS_MODE_SUPPORTED
			// FPS: Set the intro variable clipset
			if (GetPed()->UseFirstPersonUpperBodyAnims(false))
			{
				m_MoveGunOnFootNetworkHelper.SetClipSet(m_FPSAimingIntroClipSetId, ms_FPSAimingIntroClipSetId);
			}

			// B*2182691: Update MoVE network signals to ensure we don't get a 1 frame anim pop when unholstering a new weapon.
			if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition))
			{
				ComputeAndSendStrafeAimSignals();
			}
#endif	//FPS_MODE_SUPPORTED
		}
	}

	m_bEnteredAimState = false;
	m_bEnteredFiringState = false;
	m_bEnteredAimIntroState = false;

	m_bNeedToSetAimClip = true;

	// Restart the gun task if it exists
	CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
	if(pAimGunTask)
	{
		// Disable time slicing for next frame, so gun task has a chance to update
		GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		pAimGunTask->SetForceRestart(true);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::RestartLowerBodyAimNetwork()
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s RestartLowerBodyAimNetwork\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
	AI_LOG_STACK_TRACE(8);
	m_bNeedsRestart = true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::EnteredAimState() const
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive() && m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_AimOnEnterStateId))
	{
		// If we are queried before we have been processed, check the MoVE buffer on demand
		return true;
	}
	else if(m_bMoveAimOnEnterState)
	{
		return true;
	}

	return m_bEnteredAimState;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::EnteredFiringState() const
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive() && m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_FiringOnEnterStateId))
	{
		// If we are queried before we have been processed, check the MoVE buffer on demand
		return true;
	}
	else if(m_bMoveFiringOnEnterState)
	{
		return true;
	}

	return m_bEnteredFiringState;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetMoveFlag(const bool bVal, const fwMvFlagId flagId)
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkAttached())
	{
		m_MoveGunOnFootNetworkHelper.SetFlag(bVal, flagId);
	}
}
//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::SetMoveClip(const crClip* pClip, const fwMvClipId clipId)
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetClip(pClip, clipId);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetMoveFloat(const float fVal, const fwMvFloatId floatId)
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(floatId, fVal);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetMoveBoolean(const bool bVal, const fwMvBooleanId boolId)
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetBoolean(boolId, bVal);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetMoveBoolean(const fwMvBooleanId boolId) const
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		return m_MoveGunOnFootNetworkHelper.GetBoolean(boolId);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::RequestAim()
{
	m_bNeedToSetAimClip = true;

	if(m_MoveGunOnFootNetworkHelper.IsNetworkAttached())
	{
		m_MoveGunOnFootNetworkHelper.SendRequest(ms_AimId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetAimClipSet(const fwMvClipSetId& aimClipSet)
{
	if(m_AimClipSetId != aimClipSet)
	{
		m_bNeedToSetAimClip = true;
	}
	m_AimClipSetId = aimClipSet;
	taskAssert(m_AimClipSetId != CLIP_SET_ID_INVALID);

	// Clear use fire events flag
	m_bUsesFireEvents = false;

	if(taskVerifyf(fwClipSetManager::IsStreamedIn_DEPRECATED(m_AimClipSetId), "%s is not streamed in", m_AimClipSetId.GetCStr()))
	{
		if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
		{
			m_MoveGunOnFootNetworkHelper.SetClipSet(m_AimClipSetId);
			if(m_bSuppressHasIntroFlag)
				m_MoveGunOnFootNetworkHelper.SetFlag(false, ms_HasIntroId);

			SetFPSAdditivesClipSet(m_AimClipSetId);
		}

		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_AimClipSetId);	
		if(pSet)
		{
			static const fwMvClipId s_FireClipId("FIRE_MED",0xB71CA63A);
#if FPS_MODE_SUPPORTED
			static const fwMvClipId s_FireRecoilClipId("FIRE_RECOIL",0x16156B9F);
#endif // FPS_MODE_SUPPORTED

			fwMvClipId fireClipId = s_FireClipId;

#if FPS_MODE_SUPPORTED
			CPed* pPed = GetPed();
			if (pPed->UseFirstPersonUpperBodyAnims(false))
			{
				const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
				if (pWeaponInfo && pWeaponInfo->GetUseFPSAimIK() && !pWeaponInfo->GetUseFPSAnimatedRecoil())
				{
					fireClipId = s_FireRecoilClipId;
				}
			}
#endif // FPS_MODE_SUPPORTED

			crClip* pClip = pSet->GetClip(fireClipId);
			if(pClip && (CountFiringEventsInClip(*pClip) > 0))
			{
				m_bUsesFireEvents = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetFPSAdditivesClipSet(const fwMvClipSetId& fallbackClipSetId)
{
	CPed* pPed = GetPed();
	fwMvClipSetId additivesClipSetId = fallbackClipSetId;

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets))
	{
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);

		if (pWeaponInfo)
		{
			// First person idle fidget clipsets do not contain additives so revert to appropriate non-fidget clipset
			pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, false);
			fwMvClipSetId clipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, true);

			if (fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId))
			{
				additivesClipSetId = clipSetId;
			}
		}
	}

	if (m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetClipSet(additivesClipSetId, ms_FPSAdditivesClipSetId);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::CanPlayTransition() const
{
	//Ensure the state is valid.
	switch(GetState())
	{
		case State_StandingIdleIntro:
		case State_StandingIdle:
		case State_SwimIdleIntro:
		case State_SwimIdle:
		case State_SwimIdleOutro:
		case State_SwimStrafe:
		case State_StationaryPose:
		{
			break;
		}
		case State_Initial:
		case State_StreamTransition:
		case State_PlayTransition:
		case State_StreamStationaryPose:
		case State_StartMotionNetwork:
		case State_StrafingIntro:
		case State_StartStrafing:
		case State_Strafing:
		case State_StopStrafing:
		case State_Turn:
		case State_Finish:
		{
			return false;
		}
		default:
		{
			taskAssertf(false, "The state is invalid: %d.", GetState());
			return false;
		}
	}

	//Ensure we are not firing.
	if(EnteredFiringState())
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsInStrafingState() const
{
	switch (GetState())
	{
	case State_StrafingIntro:
	case State_StartStrafing:
	case State_Strafing:
	case State_Roll:
	case State_Turn180:
		return true;
	case State_Initial:
	case State_StreamTransition:
	case State_PlayTransition:
	case State_StreamStationaryPose:
	case State_StationaryPose:
	case State_StartMotionNetwork:
	case State_StandingIdleIntro:
	case State_StandingIdle:
	case State_SwimIdleIntro:
	case State_SwimIdle:
	case State_SwimIdleOutro:
	case State_SwimStrafe:
	case State_Turn:
	case State_CoverStep:
	case State_Finish:
	case State_StopStrafing:
		break;
	default:
		taskAssert(0);
		break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsPlayingTransition() const
{
	//Check the state.
	switch(GetState())
	{
		case State_StreamTransition:
		case State_PlayTransition:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsTurning() const
{
	return (GetState() == State_Turn);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::ProcessPreFSM()
{
	m_bSuppressHasIntroFlag = false;

	UpdateGaitReduction();
	ProcessMovement();
	UpdateAdditives();

	CPed* pPed = GetPed();

	// Flags
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false);

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if(!pWeaponInfo || pWeaponInfo->GetAppropriateWeaponClipSetId(pPed) == CLIP_SET_ID_INVALID)
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s quitting due to invalid clipset or weapon info (%p)\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), pWeaponInfo);
		return FSM_Quit;
	}

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		// Quit task if we we were swim strafing but no longer swimming
		if (m_bUsingMotionAimingWhileSwimmingFPS && !pPed->GetIsSwimming())
		{
			m_bUsingMotionAimingWhileSwimmingFPS = false;
			return FSM_Quit;
		}
	}

	float fBlendDuration;
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition) && ShouldTurnOffUpperBodyAnimations(fBlendDuration))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, false);
	}
#endif	//FPS_MODE_SUPPORTED

#if FPS_MODE_SUPPORTED
	const CTaskGun* pTask = (CTaskGun*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
	const bool bCheckStrafe = (pTask && pTask->ShouldDelayAfterSwap()) ? false : true;
#endif // FPS_MODE_SUPPORTED

	if(pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() || pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring()
#if FPS_MODE_SUPPORTED
		|| (pPed->UseFirstPersonUpperBodyAnims(pPed->GetIsSwimming() ? false : bCheckStrafe) && pWeaponInfo && !pWeaponInfo->GetIsThrownWeapon() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile))
#endif // FPS_MODE_SUPPORTED
		)
	{
		// Try to start the aiming network if its not already attached
		StartAimingNetwork();
	}

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		// Stream the alternate aiming animations.
		if(!m_ClipSetRequestHelper.IsLoaded())
		{
			// Grab the alternate aiming clip set.
			fwMvClipSetId clipSetId = GetAlternateAimingClipSetId();
			if(clipSetId != CLIP_SET_ID_INVALID)
			{
				// Load the animations.
				m_ClipSetRequestHelper.Request(clipSetId);
			}
		}
	}

	if(m_bMoveAimOnEnterState)
	{
		m_bEnteredAimState = true;
		m_bEnteredFiringState = false;
	}
	if(m_bMoveFiringOnEnterState)
	{
		m_bEnteredAimState = false;
		m_bEnteredFiringState = true;
	}
	if(m_bMoveAimIntroOnEnterState)
	{
		m_bEnteredAimIntroState = true;
	}

	if(!m_bEnteredAimState && !m_bEnteredFiringState)
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	//Check if we should choose a new aiming clip.
	if(ShouldChooseNewAimingClip() && ChooseNewAimingClip())
	{
		// Send a request that the aiming clip has changed
		m_MoveGunOnFootNetworkHelper.SendRequest(CTaskAimGunOnFoot::ms_RestartAimId);
	}
	
	// fix for B* 4698920, where the aim clip gets lost for clone players occasionally
	if (pPed->IsNetworkClone() && pPed->IsPlayer())
	{
		m_bNeedToSetAimClip = true;
	}

	// Set the aiming clip, if needed (this is somewhat expensive).
	if(m_bNeedToSetAimClip)
	{
		SetAimingClip();
	}

	// Update running flags
	m_bWasRunning = m_bIsRunning;
	if(m_bMoveWalking)
	{
		m_bIsRunning = false;
	}
	else if(m_bMoveRunning)
	{
		m_bIsRunning = true;
	}

	//Reset the common move signals.
	ResetCommonMoveSignals();

	// Stream the transition clip set.
	if(m_AimPoseTransition.IsClipValid())
	{
		m_AimPoseTransitionClipSetRequestHelper.Request(m_AimPoseTransition.GetClipSetId());
	}
	else
	{
		m_AimPoseTransitionClipSetRequestHelper.Release();
	}
	
	// Stream the stationary pose clip set.
	if(m_StationaryAimPose.IsLoopClipValid())
	{
		m_StationaryAimPoseClipSetRequestHelper.Request(m_StationaryAimPose.GetLoopClipSetId());
	}
	else
	{
		m_StationaryAimPoseClipSetRequestHelper.Release();
	}

#if __ASSERT
	//! Detect if GetCanMove has changed and try and verify clips again if so.
	if( (m_bCanPedMoveForVerifyClips != GetCanMove(pPed)) && 
		m_MoveGunStrafingMotionNetworkHelper.IsNetworkAttached() && 
		m_MoveGunStrafingMotionNetworkHelper.GetClipSetId() != CLIP_SET_ID_INVALID)
	{
		VerifyMovementClipSet();
		m_bCanPedMoveForVerifyClips = GetCanMove(pPed);
	}
#endif

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ProcessMovement()
{
	CPed* pPed = GetPed();

	if(m_fStrafeDirection == FLT_MAX)
	{
		m_fStrafeDirection = 0.f;
		GetCurrentStrafeDirection(m_fStrafeDirection);

		m_VelDirection = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	}

	if(GetCanMove(pPed))
	{
		if(pPed->GetMotionData()->GetIsStrafing())
		{
#if FPS_MODE_SUPPORTED
			bool bIsFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true);
			if(bIsFPSMode)
			{
				// Process snow
				CTaskHumanLocomotion::ProcessSnowDepth(pPed, GetTimeStep(), m_fSmoothedSnowDepth, m_fSnowDepthSignal);

				// Process slopes/stairs
				CTaskHumanLocomotion::ProcessSlopesAndStairs(pPed, m_uLastSlopeScanTime, m_uStairsDetectedTime, m_vLastSlopePosition, true);
			}
			else
			{
				m_fSmoothedSnowDepth = 0.f;
				m_fSnowDepthSignal = 0.f;

				CTaskHumanLocomotion::ResetSlopesAndStairs(pPed, m_vLastSlopePosition);
			}
#endif // FPS_MODE_SUPPORTED

			//******************************************************************************************
			// This is the work which was done inside CPedMoveBlendOnFoot to model player/ped movement

			// Interpolate the current move blend ratio towards the desired
			Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
			Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();

			float fCurrentMag = vCurrentMBR.Mag();
			float fDesiredMag = vDesiredMBR.Mag();

			if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
			{
				// While the independent mover is active, reinterpret the desired MBR
				float fHeadingDelta = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();

				Quaternion q;
				q.FromEulers(Vector3(0.f, 0.f, fHeadingDelta));

				Vector3 v(vDesiredMBR.x, vDesiredMBR.y, 0.f);
				q.Transform(v);
				vDesiredMBR.x = v.x;
				vDesiredMBR.y = v.y;
			}

			// Store desired strafe direction
			if(fDesiredMag > 0.f)
			{
				m_fDesiredStrafeDirection = fwAngle::LimitRadianAngleSafe(rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y));
			}
			else if(GetState() != State_Strafing)
			{
				m_fDesiredStrafeDirection = 0.f;
			}

			if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0)
			{
				if(m_iMovingStopCount == 0)
				{
					float fAngAccel;
#if FPS_MODE_SUPPORTED
					if(bIsFPSMode)
					{
						if(camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() &&
							camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->IsDoingLookBehindTransition())
						{
							fAngAccel = m_bIsRunning ? ms_Tunables.m_RunAngAccelLookBehindTransition : ms_Tunables.m_WalkAngAccelLookBehindTransition;
						}
						else
						{
							fAngAccel = m_bIsRunning ? ms_Tunables.m_RunAngAccelDirectBlend : ms_Tunables.m_WalkAngAccelDirectBlend;
						}

						// In FPS Mode and Swimming: dampen change in direction 
						if (pPed->GetIsFPSSwimming())
						{
							TUNE_GROUP_FLOAT(FPS_SWIMMING, fSwimStrafeDirectionLerpRate, 0.1f, 0.01f, 5.0f, 0.01f);
							m_fDesiredSwimStrafeDirection = fwAngle::Lerp(m_fDesiredSwimStrafeDirection, m_fDesiredStrafeDirection, fSwimStrafeDirectionLerpRate );
							m_fDesiredStrafeDirection = m_fDesiredSwimStrafeDirection;
						}
					}
					else
#endif // FPS_MODE_SUPPORTED
					{
						fAngAccel = m_bIsRunning ? ms_Tunables.m_RunAngAccel : ms_Tunables.m_WalkAngAccel;
					}

					m_fStrafeDirection = fwAngle::Lerp(m_fStrafeDirection, m_fDesiredStrafeDirection, fAngAccel * GetTimeStep());

					const float FWD_BORDER_MIN = BWD_FWD_BORDER_MIN * 0.99f;
					const float FWD_BORDER_MAX = BWD_FWD_BORDER_MAX * 0.99f;
					const float BWD_BORDER_MIN = BWD_FWD_BORDER_MIN * 1.01f;
					const float BWD_BORDER_MAX = BWD_FWD_BORDER_MAX * 1.01f;
					if(GetState() == State_StartStrafing)
					{
						// Restrict it to the boundaries of fwd/bwd
						if(m_fStartDirection >= BWD_FWD_BORDER_MIN && m_fStartDirection <= BWD_FWD_BORDER_MAX)
						{
							m_fStrafeDirection = Clamp(m_fStrafeDirection, FWD_BORDER_MIN, FWD_BORDER_MAX);
						}
						else
						{
							if(m_fStrafeDirection >= BWD_BORDER_MIN && m_fStrafeDirection <= BWD_BORDER_MAX)
							{
								float fDiff1 = SubtractAngleShorter(m_fStrafeDirection, BWD_BORDER_MIN);
								float fDiff2 = SubtractAngleShorter(m_fStrafeDirection, BWD_BORDER_MAX);
								if(Abs(fDiff1) < Abs(fDiff2))
								{
									m_fStrafeDirection = BWD_BORDER_MIN;
								}
								else
								{
									m_fStrafeDirection = BWD_BORDER_MAX;
								}
							}
						}
					}
					else if(GetState() == State_StrafingIntro)
					{
						if(m_fStrafeDirection >= BWD_BORDER_MIN && m_fStrafeDirection <= BWD_BORDER_MAX)
						{
							float fDiff1 = SubtractAngleShorter(m_fStrafeDirection, BWD_BORDER_MIN);
							float fDiff2 = SubtractAngleShorter(m_fStrafeDirection, BWD_BORDER_MAX);
							if(Abs(fDiff1) < Abs(fDiff2))
							{
								m_fStrafeDirection = BWD_BORDER_MIN;
							}
							else
							{
								m_fStrafeDirection = BWD_BORDER_MAX;
							}
						}
					}
				}

				const bool bIsPlayer = pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame();
				const float fMBRMod = GetState() == State_StrafingIntro ? ms_Tunables.m_FromOnFootAccelerationMod : 1.f;
				if(fDesiredMag > fCurrentMag)
				{
					const float fAccel = (bIsPlayer ? ms_Tunables.m_PlayerMoveAccel : ms_Tunables.m_PedMoveAccel) * fMBRMod;
					Approach(fCurrentMag, fDesiredMag, fAccel, GetTimeStep());
				}
				else
				{
					const float fDecel = (bIsPlayer ? ms_Tunables.m_PlayerMoveDecel : ms_Tunables.m_PedMoveDecel) * fMBRMod;
					Approach(fCurrentMag, fDesiredMag, fDecel, GetTimeStep());
				}

				bool bUsingHeavyWeapon = false;
				if(pPed->GetWeaponManager())
				{
					CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					if(pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy())
					{
						bUsingHeavyWeapon = true;
					}
				}

				// Clamp the crouched strafe to walk speed
#if FPS_MODE_SUPPORTED
				TUNE_GROUP_BOOL(PED_MOVEMENT, FPS_AimLTWalk, false)
#endif // FPS_MODE_SUPPORTED
				if(pPed->GetMotionData()->GetIsCrouching() || pPed->HasHurtStarted() || (bUsingHeavyWeapon FPS_MODE_SUPPORTED_ONLY(&& (!bIsFPSMode || !pPed->GetMotionData()->GetIsFPSIdle())))
#if FPS_MODE_SUPPORTED
					|| (FPS_AimLTWalk && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && (pPed->GetMotionData()->GetIsFPSLT() || pPed->GetMotionData()->GetIsFPSScope()))
#endif // FPS_MODE_SUPPORTED
					)
				{
					if(fCurrentMag > MOVEBLENDRATIO_WALK)
					{
						fCurrentMag = MOVEBLENDRATIO_WALK;
					}
				}

				vCurrentMBR.x = fCurrentMag * -Sinf(m_fStrafeDirection);
				vCurrentMBR.y = fCurrentMag * Cosf(m_fStrafeDirection);

				// Copy variables back into move blender.  This is a temporary measure.
				pPed->GetMotionData()->SetCurrentMoveBlendRatio(vCurrentMBR.y, vCurrentMBR.x);

				// Vel dir
				TUNE_GROUP_FLOAT(PED_MOVEMENT, VEL_DIR_LERP_RATE, 5.f, 0.f, 10.f, 0.01f);
				Approach(m_VelDirection.x, vDesiredMBR.x, VEL_DIR_LERP_RATE, GetTimeStep());
				Approach(m_VelDirection.y, vDesiredMBR.y, VEL_DIR_LERP_RATE, GetTimeStep());
			}
		}
	}
	else
	{
		pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f, 0.0f);
	}

	// Speed
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	m_fStrafeSpeed = vCurrentMBR.Mag() / MOVEBLENDRATIO_SPRINT;

	pPed->GetMotionData()->SetCurrentTurnVelocity(0.0f);

	if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_CoverOutroRunning) && 
		!(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetMovePed().GetTaskNetwork() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)))
	{
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return Initial_OnEnter();
			FSM_OnUpdate
				return Initial_OnUpdate();
		FSM_State(State_StreamTransition)
			FSM_OnUpdate
				return StreamTransition_OnUpdate();
		FSM_State(State_PlayTransition)
			FSM_OnEnter
				return PlayTransition_OnEnter();
			FSM_OnUpdate
				return PlayTransition_OnUpdate();
			FSM_OnExit
				return PlayTransition_OnExit();
		FSM_State(State_StreamStationaryPose)
			FSM_OnUpdate
				return StreamStationaryPose_OnUpdate();
		FSM_State(State_StationaryPose)
			FSM_OnEnter
				return StationaryPose_OnEnter();
			FSM_OnUpdate
				return StationaryPose_OnUpdate();
			FSM_OnExit
				return StationaryPose_OnExit();
		FSM_State(State_CoverStep)
			FSM_OnEnter
				return CoverStep_OnEnter();
			FSM_OnUpdate
				return CoverStep_OnUpdate();
			FSM_OnExit
				return CoverStep_OnExit();
		FSM_State(State_StartMotionNetwork)
			FSM_OnUpdate
				return StartMotionNetwork_OnUpdate();
		FSM_State(State_StandingIdleIntro)
			FSM_OnEnter
				return StandingIdleIntro_OnEnter();
			FSM_OnUpdate
				return StandingIdleIntro_OnUpdate();
			FSM_OnExit
				return StandingIdleIntro_OnExit();
		FSM_State(State_StandingIdle)
			FSM_OnEnter
				return StandingIdle_OnEnter();
			FSM_OnUpdate
				return StandingIdle_OnUpdate();
			FSM_OnExit
				return StandingIdle_OnExit();
		FSM_State(State_SwimIdleIntro)
			FSM_OnEnter
				return SwimIdleIntro_OnEnter();
			FSM_OnUpdate
				return SwimIdleIntro_OnUpdate();
			FSM_OnExit
				return SwimIdleIntro_OnExit();
		FSM_State(State_SwimIdle)
			FSM_OnEnter
				return SwimIdle_OnEnter();
			FSM_OnUpdate
				return SwimIdle_OnUpdate();
			FSM_OnExit
				return SwimIdle_OnExit();
		FSM_State(State_SwimIdleOutro)
			FSM_OnEnter
				return SwimIdleOutro_OnEnter();
			FSM_OnUpdate
				return SwimIdleOutro_OnUpdate();
			FSM_OnExit
				return SwimIdleOutro_OnExit();
		FSM_State(State_SwimStrafe)
			FSM_OnEnter
				return SwimStrafe_OnEnter();
			FSM_OnUpdate
				return SwimStrafe_OnUpdate();
			FSM_OnExit
				return SwimStrafe_OnExit();
		FSM_State(State_StrafingIntro)
			FSM_OnEnter
				return StrafingIntro_OnEnter();
			FSM_OnUpdate
				return StrafingIntro_OnUpdate();
		FSM_State(State_StartStrafing)
			FSM_OnEnter
				return StartStrafing_OnEnter();
			FSM_OnUpdate
				return StartStrafing_OnUpdate();
		FSM_State(State_Strafing)
			FSM_OnEnter
				return Strafing_OnEnter();
			FSM_OnUpdate
				return Strafing_OnUpdate();
			FSM_OnExit
				return Strafing_OnExit();
		FSM_State(State_StopStrafing)
			FSM_OnEnter
				return StopStrafing_OnEnter();
			FSM_OnUpdate
				return StopStrafing_OnUpdate();
			FSM_OnExit
				return StopStrafing_OnExit();
		FSM_State(State_Turn)
			FSM_OnEnter
				return Turn_OnEnter();
			FSM_OnUpdate
				return Turn_OnUpdate();
			FSM_OnExit
				return Turn_OnExit();
		FSM_State(State_Roll)
			FSM_OnEnter
				return Roll_OnEnter();
			FSM_OnUpdate
				return Roll_OnUpdate();
			FSM_OnExit
				return Roll_OnExit();
		FSM_State(State_Turn180)
			FSM_OnEnter
				return Turn180_OnEnter();
			FSM_OnUpdate
				return Turn180_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::ProcessPostFSM()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0 && !pPed->GetPedResetFlag(CPED_RESET_FLAG_CoverOutroRunning) && 
		!(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetMovePed().GetTaskNetwork() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)))
	{
		// If we are setting the heading, don't apply any extra heading
		if(!DoPostCameraSetHeading())
		{
			float fTimeStep = CalculateUnWarpedTimeStep(GetTimeStep());

			switch(GetState())
			{
			case State_StandingIdleIntro:
			case State_StrafingIntro:
			case State_StartStrafing:
			case State_Strafing:
			case State_StopStrafing:
				{
					if(pPed->GetMotionData()->GetIsStrafing())
					{
						// Apply some extra steering here
						float fCurrentHeading = pPed->GetCurrentHeading();
						float fDesiredHeading = pPed->GetDesiredHeading();

						TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_AIM_HEADING_LERP, 4.0f, 0.0f, 20.0f, 0.1f);
						TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_AIM_HEADING_CLAMP, 11.f * DtoR, 0.0f, PI, 0.1f);
						m_fExtraHeadingChange = InterpValue(fCurrentHeading, fDesiredHeading, STRAFE_AIM_HEADING_LERP, true, fTimeStep);
						m_fExtraHeadingChange = Clamp(m_fExtraHeadingChange, -STRAFE_AIM_HEADING_CLAMP, STRAFE_AIM_HEADING_CLAMP);
						m_fExtraHeadingChange += GetMotionData().GetExtraHeadingChangeThisFrame();
						GetMotionData().SetExtraHeadingChangeThisFrame(m_fExtraHeadingChange);
					}
				}
				break;
			case State_StandingIdle:
			case State_SwimIdleIntro:
			case State_SwimIdle:
				{
					const float fDesiredHeading = CalcDesiredDirection();

					static dev_float MAX_EXTRA_HEADING_CHANGE = 0.2f;
					static dev_float MAX_EXTRA_HEADING_CHANGE_SWIM_INTRO = 0.4f;
					float fMaxExtraHeadingChange = m_bSwimIntro ? MAX_EXTRA_HEADING_CHANGE_SWIM_INTRO : MAX_EXTRA_HEADING_CHANGE;
 
					float fExtraHeading = Clamp(fDesiredHeading, -fMaxExtraHeadingChange, fMaxExtraHeadingChange);
					fExtraHeading += GetMotionData().GetExtraHeadingChangeThisFrame();

					if(Abs(fExtraHeading) < Abs(m_fExtraHeadingChange))
						m_fExtraHeadingChange = fExtraHeading;
					else
					{
						static dev_float RATE = 0.25f;
						static dev_float RATE_SWIM_INTRO = 2.0f;
						float fRate = m_bSwimIntro ? RATE_SWIM_INTRO : RATE;
						Approach(m_fExtraHeadingChange, fExtraHeading, fRate, fTimeStep);
					}
					GetMotionData().SetExtraHeadingChangeThisFrame(m_fExtraHeadingChange);
				}
				break;
			case State_SwimIdleOutro:
			case State_SwimStrafe:
			case State_PlayTransition:
				{
					const float fDesiredHeading = CalcDesiredDirection();

					static dev_float MAX_EXTRA_HEADING_CHANGE = 0.2f;
					float fExtraHeading = Clamp(fDesiredHeading, -MAX_EXTRA_HEADING_CHANGE, MAX_EXTRA_HEADING_CHANGE);
					fExtraHeading += GetMotionData().GetExtraHeadingChangeThisFrame();
			
					if(Abs(fExtraHeading) < Abs(m_fExtraHeadingChange))
						m_fExtraHeadingChange = fExtraHeading;
					else
					{
						static dev_float RATE = 0.25f;
						Approach(m_fExtraHeadingChange, fExtraHeading, RATE, fTimeStep);
					}
					GetMotionData().SetExtraHeadingChangeThisFrame(m_fExtraHeadingChange);
				}
				break;
			default:
				// Reset
				m_fExtraHeadingChange = 0.f;
				break;
			}
		}
		else
		{
			// Reset
			m_fExtraHeadingChange = 0.f;
		}
	}

	// Parent signals
	SendParentSignals();

	// Send the anim rate
	float fAnimRate = 1.f;
	if(!pPed->IsPlayer())
	{
		fAnimRate = ComputeRandomAnimRate();
	}

	if(GetState() == State_Strafing
#if FPS_MODE_SUPPORTED
		|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && GetState() == State_StartStrafing)
#endif // FPS_MODE_SUPPORTED
		)
	{
		float fDesiredAnimRate = 1.f;
		if(!m_bIsRunning && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
		{
			float fNormalisedMBR = m_fStrafeSpeed * MOVEBLENDRATIO_SPRINT;
			fNormalisedMBR = Min(fNormalisedMBR, MOVEBLENDRATIO_WALK);

			static const float ONE_OVER_MBR_WALK = 1.f / MOVEBLENDRATIO_WALK;
			fNormalisedMBR *= ONE_OVER_MBR_WALK;

			fDesiredAnimRate = Lerp(fNormalisedMBR, ms_Tunables.m_MovingWalkAnimRateMin, ms_Tunables.m_MovingWalkAnimRateMax);
		}

		if(m_bSnapMovementWalkRate)
			m_fWalkMovementRate = fDesiredAnimRate;
		else
			Approach(m_fWalkMovementRate, fDesiredAnimRate, ms_Tunables.m_MovingWalkAnimRateAcceleration, GetTimeStep());
		m_bSnapMovementWalkRate = false;

		// Apply rate
		fAnimRate *= m_fWalkMovementRate;

		// Apply the move rate override rate if applicable to ensure anims look ok when the velocity is similarly increased
		fAnimRate *= pPed->GetMotionData()->GetCurrentMoveRateOverride();
	}

	// Send any global params
#if FPS_MODE_SUPPORTED
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	bool bFPSModeEnabled = pPed->UseFirstPersonUpperBodyAnims(false);
	bool bFPSModeEnabledLocal = bFPSModeEnabled && pPed->IsLocalPlayer();
	bool bFPSModeIKEnabled = pWeaponInfo && pWeaponInfo->GetUseFPSAimIK() && bFPSModeEnabledLocal;
	bool bFPSAnimatedRecoil = pWeaponInfo && pWeaponInfo->GetUseFPSAnimatedRecoil() && bFPSModeEnabledLocal;
	bool bFPSSecondaryMotion = pWeaponInfo && pWeaponInfo->GetUseFPSSecondaryMotion() && bFPSModeEnabledLocal;
	bool bFPSUseGripAttachment = bFPSModeEnabledLocal && pWeapon && pWeapon->HasGripAttachmentComponent();
#endif // FPS_MODE_SUPPORTED

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFlag(NetworkInterface::IsRemotePlayerInFirstPersonMode(*pPed), ms_IsRemotePlayerUsingFPSModeId);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_AnimRateId, fAnimRate);

#if FPS_MODE_SUPPORTED
		bool bBlockAdditives = false;

		if (bFPSModeEnabled)
		{
			// Block when firing
			const CTaskGun* gunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
			bBlockAdditives |= gunTask && gunTask->GetIsFiring();
		}

		m_MoveGunOnFootNetworkHelper.SetFlag(bFPSModeEnabledLocal, ms_FirstPersonMode);
		m_MoveGunOnFootNetworkHelper.SetFlag(bFPSModeIKEnabled, ms_FirstPersonModeIK);
		m_MoveGunOnFootNetworkHelper.SetFlag(bFPSAnimatedRecoil, ms_FirstPersonAnimatedRecoil);
		m_MoveGunOnFootNetworkHelper.SetFlag(bFPSSecondaryMotion, ms_FirstPersonSecondaryMotion);
		m_MoveGunOnFootNetworkHelper.SetFlag(bBlockAdditives, ms_BlockAdditives);
		m_MoveGunOnFootNetworkHelper.SetFlag(bFPSUseGripAttachment, ms_FPSUseGripAttachment);

		if (!pPed->GetIsFPSSwimming() && pWeaponInfo && pWeaponInfo->GetIsMeleeFist())
		{
			fwMvClipSetId weaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());
			if (weaponClipSetId != CLIP_SET_ID_INVALID)
			{
				crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(weaponClipSetId);
				if (pFilter)
				{
					m_MoveGunOnFootNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
				}
			}
		}
#endif // FPS_MODE_SUPPORTED
	}

	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_MovementRateId, m_fMovementRate * fAnimRate);
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_AnimRateId, fAnimRate);
#if FPS_MODE_SUPPORTED
		m_MoveGunStrafingMotionNetworkHelper.SetFlag(bFPSModeEnabled, ms_FirstPersonMode);
		// Use FPS Swimming Idle/Moving state machines in MoVE network
		m_MoveGunStrafingMotionNetworkHelper.SetFlag((pPed->GetIsFPSSwimming() && m_bUsingMotionAimingWhileSwimmingFPS), ms_SwimmingId);

		// Set weapon clipset. Needed to ensure melee grip gets used when using knuckle dusters in FPS. Don't do this for swimming, SendFPSSwimmingMoveSignals handles swimming signals.
		if (!pPed->GetIsFPSSwimming())
		{
			if (pWeaponInfo && pWeaponInfo->GetIsMeleeFist())
			{
				fwMvClipSetId weaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());
				if (weaponClipSetId != CLIP_SET_ID_INVALID && fwClipSetManager::IsStreamedIn_DEPRECATED(weaponClipSetId))
				{
					m_MoveGunStrafingMotionNetworkHelper.SetClipSet(weaponClipSetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);

					crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(weaponClipSetId);
					if (pFilter)
					{
						m_MoveGunStrafingMotionNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
					}
				}
			}
			else
			{
				// Clear weapon clipset.
				m_MoveGunStrafingMotionNetworkHelper.SetClipSet(CLIP_SET_ID_INVALID, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);
			}
		}
			
#endif // FPS_MODE_SUPPORTED
		m_MoveGunStrafingMotionNetworkHelper.SetFlag(pPed->IsLocalPlayer(), ms_IsPlayerId);
	}

	if(!m_bAimIntroFinished)
	{
		// Need to disable time slicing while we are doing the aim intro, as this sets anim phase directly
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ProcessPostCamera()
{
	CPed* pPed = GetPed();

	if(m_bUpdateStandingIdleIntroFromPostCamera)
	{
		taskAssert(GetState() == State_StandingIdleIntro);

		// Update the variables
		const float fFacingHeading = fwAngle::LimitRadianAngleSafe(pPed->GetFacingDirectionHeading());
		const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
		m_fStartDirection = SubtractAngleShorter(fDesiredHeading, fFacingHeading);
		m_fStartDesiredHeading = fDesiredHeading;
		if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
		{
			// Send the desired heading signal
			m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredHeadingId, ConvertRadianToSignal(m_fStartDirection));
		}

		m_bUpdateStandingIdleIntroFromPostCamera = false;
	}

	if(DoPostCameraAnimUpdate())
	{
		// Update the player's clips instantly for aim state to prevent the clip lagging behind
		ComputeAndSendStrafeAimSignals();

		if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);

			bool bUseZeroTimeStep = true;
#if FPS_MODE_SUPPORTED
			bUseZeroTimeStep = !pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPed->GetMotionData()->GetWasFPSUnholster();
#endif

			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, bUseZeroTimeStep);
		}
	}

	if(DoPostCameraSetHeading())
	{
        if(!pPed->IsNetworkClone())
        {
			// B*2603073: Use heading lerp when trying to look behind when weapon blocked.
			bool bLerpHeadingAsPedIsBlockedAndLookingBehind = false;
			if (pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED)
				&& pPed->GetControlFromPlayer() && pPed->GetControlFromPlayer()->GetPedLookBehind().IsPressed())
			{
				bLerpHeadingAsPedIsBlockedAndLookingBehind = true;
			}
		   
			if(m_uCameraHeadingLerpTime == UINT_MAX || bLerpHeadingAsPedIsBlockedAndLookingBehind)
		    {
			    m_uCameraHeadingLerpTime = fwTimer::GetTimeInMilliseconds();
		    }

		    const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
		    float fAimHeading = aimCameraFrame.ComputeHeading();

		    bool bDoingLookBehindTransition = false;
#if FPS_MODE_SUPPORTED
 		    if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true))
 		    {
 			    fAimHeading = camInterface::GetPlayerControlCamHeading(*pPed);
 
			    const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
 			    if(pFpsCam)
 			    {
 				    fAimHeading += pFpsCam->GetRelativeHeading();
				    bDoingLookBehindTransition = pFpsCam->IsDoingLookBehindTransition();
 			    }
 		    }
#endif // FPS_MODE_SUPPORTED

#if RSG_PC
		    fAimHeading = fwAngle::LimitRadianAngleSafe(fAimHeading);
#else
		    fAimHeading = fwAngle::LimitRadianAngle(fAimHeading);
#endif
		
		    const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());

		    TUNE_GROUP_INT(PED_MOVEMENT, LERP_HEADING_AFTER_START_TIME_MS, 1000, 0, 10000, 1);

		    if(m_bBlendInSetHeading && !bDoingLookBehindTransition)
		    {
			    u32 uLerpTime = fwTimer::GetTimeInMilliseconds() - m_uCameraHeadingLerpTime;

			    if(uLerpTime < LERP_HEADING_AFTER_START_TIME_MS)
			    {
				    const float fLerpRatio = (float)uLerpTime / (float)LERP_HEADING_AFTER_START_TIME_MS;
				    fAimHeading = fwAngle::LerpTowards(fCurrentHeading, fAimHeading, fLerpRatio);
			    }
		    }

		    if(!bDoingLookBehindTransition)
		    {
			    const float fHeadingDelta = SubtractAngleShorter(fAimHeading, fCurrentHeading);
			    m_fCameraTurnSpeed = fHeadingDelta / fwTimer::GetTimeStep();
		    }
		    else
		    {
			    m_fCameraTurnSpeed = 0.f;
		    }

		    // Set the heading directly, as the camera moves too fast during a look behind for us to keep up with, and the camera processing is done after physics
		    const bool c_HeadingLimitedSniperCamera = camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera() &&
										    camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera()->GetWasRelativeHeadingLimitsOverriddenThisUpdate();
		    if( !c_HeadingLimitedSniperCamera )
		    {
			    pPed->SetHeading(fAimHeading);
		    }
        }

		if(GetState() == State_StandingIdle)
		{
			// Update the anim signals
			StandingIdle_OnProcessMoveSignals();
		}
	}
	else
	{
		m_uCameraHeadingLerpTime = UINT_MAX;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ProcessPreRender2()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	bool bFPSMode = pPed->UseFirstPersonUpperBodyAnims(false);
	bool bFPSIdleFidget = pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) && pPed->GetMotionData()->GetIsFPSIdle();
	bool bFPSWaitForRightHandIK = false;

	if(bFPSMode && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition) || bFPSIdleFidget))
	{
		float fCHWeight;
		Vec3V vCHTranslation;
		QuatV qCHRotation;
		const bool bValid = pPed->GetConstraintHelperDOFs(true, fCHWeight, vCHTranslation, qCHRotation);
		const bool bCHWeightInValid = fCHWeight < 1.0f;

		bFPSWaitForRightHandIK = !bValid || bCHWeightInValid;
	}
#endif

	if(IsStateValidForIK() && !fwTimer::IsGamePaused() FPS_MODE_SUPPORTED_ONLY(&& !bFPSWaitForRightHandIK))
	{
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
		if(pWeaponInfo)
		{
			static dev_float IK_BLEND_IN_DURATION = PEDIK_ARMS_FADEIN_TIME;
			static dev_float IK_BLEND_OUT_DURATION = 0.2f;

			float fBlendInDuration = IK_BLEND_IN_DURATION;
			float fBlendOutDuration = IK_BLEND_OUT_DURATION;

#if FPS_MODE_SUPPORTED
			if(bFPSMode)
			{
				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition))
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSLeftHandIKUnholsterBlendInTime, 0.001f, 0.0f, 2.0f, 0.001f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSLeftHandIKUnholsterBlendOutTime, 0.001f, 0.0f, 2.0f, 0.001f);

					fBlendInDuration = fFPSLeftHandIKUnholsterBlendInTime;
					fBlendOutDuration = fFPSLeftHandIKUnholsterBlendOutTime;
				}
				else
				{
					// Match first person right arm IK
					fBlendInDuration = CArmIkSolver::GetBlendDuration(ARMIK_BLEND_RATE_FAST);
					fBlendOutDuration = CArmIkSolver::GetBlendDuration(ARMIK_BLEND_RATE_FASTEST);
				}
			}
#endif
			ProcessOnFootAimingLeftHandGripIk(*pPed, *pWeaponInfo, false, fBlendInDuration, fBlendOutDuration);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ProcessPostPreRender()
{
	CPed* pPed = GetPed();

	if(pPed && !pPed->m_nDEflags.bFrozen)
	{
		if(!DoPostCameraAnimUpdate())
		{
			ComputeAndSendStrafeAimSignals();
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	//Ensure we should process physics.
	if(!ShouldProcessPhysics())
	{
		return false;
	}

	const bool bDoPostCameraSetHeading = DoPostCameraSetHeading();

	CPed* pPed = GetPed();
	float fDesiredHeading = pPed->GetDesiredHeading();
	bool bStayActive = false;

	switch(GetState())
	{
	case State_CoverStep:
		{
			CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
			if (pInCoverTask && pPed->GetCoverPoint())
			{
				s32 iCurrentCoverStepNodeIndex = pInCoverTask->GetCurrentCoverStepNodeIndex();
				if (iCurrentCoverStepNodeIndex > -1)
				{
					const bool bFacingLeft = pInCoverTask->IsFacingLeft();
					bool bHigh = pInCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint);
					bool bInHighCoverNotAtEdge = pInCoverTask->GetForcedStepBack() || (!pInCoverTask->IsCoverFlagSet(CTaskCover::CF_AtCorner) && bHigh);
					if (pInCoverTask->IsCoverFlagSet(CTaskCover::CF_CloseToPossibleCorner))
					{
						bInHighCoverNotAtEdge = false;
					}
					Vector3 vStepOutPosition = CTaskAimGunFromCoverIntro::ComputeCurrentStepOutPosition(bFacingLeft, *pPed, iCurrentCoverStepNodeIndex, bInHighCoverNotAtEdge, pInCoverTask->IsCoverFlagSet(CTaskCover::CF_CloseToPossibleCorner));
					Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

					const float fStepPhase = rage::Clamp(m_MoveGunStrafingMotionNetworkHelper.GetFloat(ms_CoverStepClipPhaseId), 0.0f, 1.0f);
					TUNE_GROUP_FLOAT(COVER_STEP_TUNE, MIN_PHASE_TO_APPLY_EXTRA_VELOCITY, 0.25f, 0.0f, 1.0f, 0.01f);
					float fMultiplier = 1.0f;
					float fExtraVelocity = 0.0f;
					if (fStepPhase >= MIN_PHASE_TO_APPLY_EXTRA_VELOCITY)
					{
						TUNE_GROUP_FLOAT(COVER_STEP_TUNE, MAX_EXTRA_VELOCITY, 1.0f, 0.0f, 10.0f, 0.01f);
						const CTaskAimGunFromCoverIntro::AimStepInfoSet& aimStepInfoSet = CTaskAimGunFromCoverIntro::ms_Tunables.GetAimStepInfoSet(bFacingLeft);
						float fExtraDistance = pInCoverTask->GetStepClipExtraTranslation();
						fExtraVelocity = rage::Clamp(fExtraDistance / fTimeStep, -MAX_EXTRA_VELOCITY, MAX_EXTRA_VELOCITY);
						fExtraDistance -= Abs(fExtraVelocity * fTimeStep);
						pInCoverTask->SetStepClipExtraTranslation(fExtraDistance);
						fMultiplier = 1.0f + aimStepInfoSet.m_StepInfos[iCurrentCoverStepNodeIndex].m_NextTransitionExtraScalar;
					}

					Vector3 vToTarget = vStepOutPosition - vPedPosition;
					vToTarget.z = 0.0f;
					vToTarget.Normalize();
					float fClipSpeed = fMultiplier *pPed->GetDesiredVelocity().Mag() + fExtraVelocity;
					vToTarget.Scale(fClipSpeed);
					pPed->SetDesiredVelocityClamped(vToTarget, CTaskMotionBase::ms_fAccelLimitHigher*fTimeStep);

					TUNE_GROUP_BOOL(COVER_AIMING_TUNE, APPROACH_ROTATION, true);
					if (APPROACH_ROTATION)
					{
						pPed->SetAngVelocity(VEC3_ZERO);
						float fCurrentHeading = pPed->GetCurrentHeading();
						float fDelta = fwAngle::LimitRadianAngle(fDesiredHeading - fCurrentHeading);
						float fNewDelta = 0.0f;
						rage::Approach(fNewDelta, fDelta, CTaskAimGunFromCoverIntro::ms_Tunables.m_SteppingHeadingApproachRate, fTimeStep);
						pPed->SetHeading(fwAngle::LimitRadianAngle(fCurrentHeading + fNewDelta));
					}
					return true;
				}
			}
		}
		break;
	case State_StandingIdle:
		{
			if(!bDoPostCameraSetHeading)
			{
				// If the velocity is going to make us overshoot, clamp it
				Vector3 vAngVel(pPed->GetDesiredAngularVelocity());

				const float fRotateSpeed = vAngVel.z;
				const float fHeadingDiff = Abs(CalcDesiredDirection()) / fTimeStep;
				if(fRotateSpeed < -fHeadingDiff || fRotateSpeed > fHeadingDiff)
				{
					vAngVel.z = Clamp(fRotateSpeed, -fHeadingDiff, fHeadingDiff);
					pPed->SetDesiredAngularVelocity(vAngVel);
					return true;
				}
			}
		}
		break;
	case State_SwimIdle:
		{
			if(!bDoPostCameraSetHeading)
			{
				// If the velocity is going to make us overshoot, clamp it
				Vector3 vAngVel(pPed->GetDesiredAngularVelocity());

				const float fRotateSpeed = vAngVel.z;
				const float fHeadingDiff = Abs(CalcDesiredDirection()) / fTimeStep;
				if(fRotateSpeed < -fHeadingDiff || fRotateSpeed > fHeadingDiff)
				{
					vAngVel.z = Clamp(fRotateSpeed, -fHeadingDiff, fHeadingDiff);
					pPed->SetDesiredAngularVelocity(vAngVel);
					return true;
				}
				break;
			}
		}
	default:
		break;
	}

	if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0 && !bDoPostCameraSetHeading)
	{
		const float fCurrentHeading = pPed->GetCurrentHeading();	
		float fHeadingDelta = fwAngle::LimitRadianAngle(fDesiredHeading - fCurrentHeading);

		// Get the animated angular velocity about to be applied
		Vector3 vAngVel = pPed->GetDesiredAngularVelocity();

		// Scale the animated velocity based on how large the delta is
		//static bank_float s_fMinHeadingDeltaToApplyScaling = 0.3f; //QUARTER_PI;
		if(GetState() == State_PlayTransition)
		{
			TUNE_GROUP_FLOAT(PLAYER_TARGETING, MAX_EXTRA_HEADING_RATE, 1.0f, 0.0f, 20.f, 0.01f);

			static bank_float s_fMaxHeadingDelta = PI;
			const float fScaleModifier = Abs(fHeadingDelta / s_fMaxHeadingDelta);

			// CS: I've removed the / fTimeStep here, as it was essentially snapping the heading.
			// We still get extra heading if it is too slow, but it is more gradual than before, and can be controlled by the tune floats
			const float fExtraAngVelocity = (MAX_EXTRA_HEADING_RATE * fScaleModifier);

			vAngVel.z += Sign(fHeadingDelta)*fExtraAngVelocity;
		}

		// Compute how much this will change our heading
		float fHeadingMovedThisTimeStep = vAngVel.z * fTimeStep;

		// If this change will exceed the amount we need to turn, just set the velocity to 
		// be the velocity required to turn the required delta
		if(Abs(fHeadingMovedThisTimeStep) > Abs(fHeadingDelta))
		{
			vAngVel.z = fHeadingDelta / fTimeStep;
			taskAssertf(rage::FPIsFinite(vAngVel.z), "Computed angular velocity was invalid, heading delta : %.4f, timestep : %.4f", fHeadingDelta, fTimeStep);
		}
		
		static dev_float MIN_ANG_VEL_DIFF = 0.1f;
		if(pPed->GetDesiredAngularVelocity().Dist2(vAngVel) > square(MIN_ANG_VEL_DIFF))
		{
			pPed->SetDesiredAngularVelocity(vAngVel);
			bStayActive = true;
		}
	}

	if(bDoPostCameraSetHeading)
	{
		// Zero any angular vel
		pPed->SetDesiredAngularVelocity(VEC3_ZERO);
		bStayActive = true;
	}

	//
	// Turn the velocity to be more the direction we want to face and normalise it
	//
	
	if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		const bool bUsingLeftTrigger = UsingLeftTriggerAimMode();
		Vector3 vDesiredVelocity = pPed->GetDesiredVelocity();

		//
		// Remove the ground physical vel - doing the inverse of what is done to the animated velocity in CTaskMotionBase::CalcDesiredVelocity
		//

		Matrix34 matAngularRot(Matrix34::IdentityType);
		// Rotate this by the angular velocity
		if(pPed->GetDesiredAngularVelocity().IsNonZero())
		{
			// Get the ground angular velocity (minus angular velocity).
			Vector3 vDesiredAngVel = pPed->GetDesiredAngularVelocity();
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

		vDesiredVelocity -= VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
		matAngularRot.UnTransform(vDesiredVelocity);

#if FPS_MODE_SUPPORTED
		bool bFirstPersonModeEnabled = pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true);
		if(!bFirstPersonModeEnabled)
#endif // FPS_MODE_SUPPORTED
		{
			float fDesiredHeading = GetState() == State_StopStrafing ? m_fStopDirection : fwAngle::LimitRadianAngleSafe(rage::Atan2f(-m_VelDirection.x, m_VelDirection.y));
			fDesiredHeading += pPed->GetCurrentHeading();
			fDesiredHeading  = fwAngle::LimitRadianAngle(fDesiredHeading);

			Matrix34 m;
			m.MakeRotateZ(fDesiredHeading);

			TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFING_EXTRA_VEL_BLEND_ANGLE, 0.15f, 0.f, 10.f, 0.01f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFING_EXTRA_VEL_BLEND_ANGLE_L2_MODE, 0.3f, 0.f, 10.f, 0.01f);
			float fExtraVelBlendAngle = bUsingLeftTrigger ? STRAFING_EXTRA_VEL_BLEND_ANGLE_L2_MODE : STRAFING_EXTRA_VEL_BLEND_ANGLE;
			Vector2 desiredVelocityV2;
			vDesiredVelocity.GetVector2XY(desiredVelocityV2);

			float fAngle = !desiredVelocityV2.IsClose(Vector2(0.f, 0.f), 0.001f) ? vDesiredVelocity.AngleZ(m.b) : 0.0f;
			if(fAngle > 0.0f)
			{
				float fRange = ClampRange(fAngle, 0.0f, fExtraVelBlendAngle);
				vDesiredVelocity.RotateZ(fExtraVelBlendAngle * square(fRange));
			}
			else
			{
				float fRange = 1.0f - ClampRange(fAngle, -fExtraVelBlendAngle, 0.0f);
				vDesiredVelocity.RotateZ(-fExtraVelBlendAngle * square(fRange));
			}
		}
		
		//
		// Normalise the velocity
		//

		bool bDisableNormaliseDueToCollision = false;
		const CCollisionHistory* pCollisionHistory = pPed->GetFrameCollisionHistory();
		if(pCollisionHistory)
		{
			const CCollisionRecord* pMostSignificantRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
			if(pMostSignificantRecord)
			{
				const float fDesiredStrafeDirection = fwAngle::LimitRadianAngleSafe(m_fDesiredStrafeDirection + pPed->GetCurrentHeading());
				Vector3 vDesiredStrafeDirection(YAXIS);
				vDesiredStrafeDirection.RotateZ(fDesiredStrafeDirection);
				if(pMostSignificantRecord->m_MyCollisionNormal.Dot(vDesiredStrafeDirection) <= 0.f)
				{
					m_uLastDisableNormaliseDueToCollisionTime = fwTimer::GetTimeInMilliseconds();
					bDisableNormaliseDueToCollision = true;
				}
			}
		}

		TUNE_GROUP_INT(PED_MOVEMENT, DISABLE_AIMING_VELOCITY_NORMALISE_DUE_TO_COLLISION_TIMEOUT, 100, 0, 10000, 1);
		if(!bDisableNormaliseDueToCollision && (m_uLastDisableNormaliseDueToCollisionTime + DISABLE_AIMING_VELOCITY_NORMALISE_DUE_TO_COLLISION_TIMEOUT) > fwTimer::GetTimeInMilliseconds())
		{
			bDisableNormaliseDueToCollision = true;
		}

		if( pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
		{
			const CTaskJump* jumpTask = static_cast<const CTaskJump*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP));
			if (jumpTask && jumpTask->GetIsDoingSuperJump())
			{
				bDisableNormaliseDueToCollision = true;
			}
		}

		TUNE_GROUP_BOOL(PED_MOVEMENT, DO_AIMING_VELOCITY_NORMALISE, true);
		if((m_bDoVelocityNormalising FPS_MODE_SUPPORTED_ONLY(|| bFirstPersonModeEnabled)) && DO_AIMING_VELOCITY_NORMALISE && !bDisableNormaliseDueToCollision)
		{
 			if(GetState() == State_Strafing
#if FPS_MODE_SUPPORTED
				|| GetState() == State_StartStrafing || GetState() == State_Turn180 || GetState() == State_StrafingIntro
#endif // FPS_MODE_SUPPORTED
				)
			{
#if FPS_MODE_SUPPORTED
				if(bFirstPersonModeEnabled)
				{
					m_fVelLerp = 1.f;
				}
				else
#endif // FPS_MODE_SUPPORTED
				{
					TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_VEL_TVAL_NOT_IN_L2_MODE, 0.5f, 0.f, 1.f, 0.01f);
					TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_VEL_TVAL_APPROACH_RATE, 1.f, 0.f, 1.f, 0.01f);

					float fLerpTarget = 1.f;
					if(!bUsingLeftTrigger)
					{
						fLerpTarget = STRAFE_VEL_TVAL_NOT_IN_L2_MODE;
					}

					Approach(m_fVelLerp, fLerpTarget, STRAFE_VEL_TVAL_APPROACH_RATE, fTimeStep);
				}

				float fMagSquared = vDesiredVelocity.XYMag2();
				if(fMagSquared > 0.001f)
				{
					float fMag = sqrtf(fMagSquared);
					float fVel = CalculateNormaliseVelocity(FPS_MODE_SUPPORTED_ONLY(bFirstPersonModeEnabled));
					fVel = Lerp(m_fVelLerp, fMag, fVel);

#if FPS_MODE_SUPPORTED
					if(bFirstPersonModeEnabled)
					{
						if (!pPed->GetIsSwimming())
						{
							if(m_fNormalisedVel == FLT_MAX)
								m_fNormalisedVel = fVel;

							TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_NormalisedVelRate, 10.f, 0.f, 100.f, 0.01f);
							Approach(m_fNormalisedVel, fVel, FPS_NormalisedVelRate, fTimeStep);

							float fScale = (1.f/fMag) * m_fNormalisedVel;
							vDesiredVelocity.x *= fScale;
							vDesiredVelocity.y *= fScale;
						}
					}
					else
#endif // FPS_MODE_SUPPORTED
					{
						// Normalise XY and apply the vel
						float fScale = (1.f/fMag) * fVel;
						vDesiredVelocity.x *= fScale;
						vDesiredVelocity.y *= fScale;
					}
				}
			}
#if FPS_MODE_SUPPORTED
			else if(bFirstPersonModeEnabled && GetState() == State_StopStrafing)
			{
				if (!pPed->GetIsSwimming())
				{
					float fMagSquared = vDesiredVelocity.XYMag2();
					if(fMagSquared > 0.001f)
					{
						if(m_fNormalisedStopVel == FLT_MAX)
							m_fNormalisedStopVel = 0.f;

						float fMag = sqrtf(fMagSquared);
						TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_NormalisedVelStopRate, 10.f, 0.f, 100.f, 0.01f);
						Approach(m_fNormalisedStopVel, 0.f, FPS_NormalisedVelStopRate, fTimeStep);

						float fScale = (1.f/fMag) * m_fNormalisedStopVel;
						vDesiredVelocity.x *= fScale;
						vDesiredVelocity.y *= fScale;
					}
				}
			}
#endif // FPS_MODE_SUPPORTED
			else
			{
				m_fVelLerp = 0.f;
			}
		}

		// Re-add the ground physical vel
		matAngularRot.Transform(vDesiredVelocity);
		vDesiredVelocity += VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());

		// If we are colliding with something, ensure to clamp the velocity
		if(bDisableNormaliseDueToCollision)
		{
			const Vector3 vCurrentPedVelocity(pPed->GetVelocity());
			Vector3 vVelChange = vDesiredVelocity - vCurrentPedVelocity;

			CPhysical* pPhysical = pPed->GetGroundPhysical();
			if(!pPhysical && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))	// Check reset flag to avoid the GetClimbPhysical() call in the common case.
			{
				pPhysical = pPed->GetClimbPhysical();
			}

			const ScalarV timeStepV(fTimeStep);

			// Clamp the velocity change if we are colliding
			vVelChange = VEC3V_TO_VECTOR3(CalcVelChangeLimitAndClamp(*pPed, VECTOR3_TO_VEC3V(vVelChange), timeStepV, pPhysical));

			vDesiredVelocity = vCurrentPedVelocity + vVelChange;
		}

		static dev_float MIN_VEL_DIFF = 0.1f;
		if(pPed->GetDesiredVelocity().Dist2(vDesiredVelocity) > square(MIN_VEL_DIFF))
		{
            // if we are a network clone don't let this desired velocity adjustment stomp over
            // the blender corrections, so reapply any velocity change away from the correction vector
            if(pPed->IsNetworkClone())
            {
                CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, pPed->GetNetworkObject()->GetNetBlender());

                if(netBlenderPed)
                {
                    Vector3 vDesiredVelChange  = pPed->GetDesiredVelocity() - vDesiredVelocity;
                    Vector3 vBlenderCorrection = netBlenderPed->GetCorrectionVector();
                    vBlenderCorrection.Normalize();
                    float magRelativeToCorrection = vDesiredVelChange.Dot(vBlenderCorrection);
                    vBlenderCorrection.Scale(magRelativeToCorrection);

                    vDesiredVelocity += vBlenderCorrection;
                }
            }

			pPed->SetDesiredVelocity(vDesiredVelocity);
			bStayActive = true;
		}
	}
	else
	{
		m_fVelLerp = 0.f;
	}

	return bStayActive || pPed->GetGroundPhysical();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ProcessPostMovement()
{
	if(m_bDoPostMovementIndependentMover)
	{
		CPed* pPed = GetPed();
		float fCurrentHeading = pPed->GetCurrentHeading();
		float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
		float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
		SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverRequestId, true);
		m_bDoPostMovementIndependentMover = false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ProcessMoveSignals()
{
	ReadCommonMoveSignals();

	//Check the state.
	const int iState = GetState();
	switch(iState)
	{
		case State_PlayTransition:
			{
				PlayTransition_OnProcessMoveSignals();
			}
			break;
		case State_CoverStep:
			{
				CoverStep_OnProcessMoveSignals();
			}
			break;
		case State_StandingIdleIntro:
			{
				StandingIdleIntro_OnProcessMoveSignals();
			}
			break;
		case State_StandingIdle:
			{
				if(!DoPostCameraSetHeading())
				{
					StandingIdle_OnProcessMoveSignals();
				}
			}
			break;
		case State_StartStrafing:
			{
				StartStrafing_OnProcessMoveSignals();
			}
			break;
		case State_Strafing:
			{
				Strafing_OnProcessMoveSignals();
			}
			break;
		case State_StopStrafing:
			{
				StopStrafing_OnProcessMoveSignals();
			}
			break;
		case State_Turn:
			{
				Turn_OnProcessMoveSignals();
			}
			break;
		case State_Turn180:
			{
				Turn180_OnProcessMoveSignals();
			}
			break;
		default:
			break;
	}

	//We have things that need to be done every frame, so never turn this callback off.
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsGoingToStopThisFrame(const CTaskMoveGoToPointAndStandStill* pGotoTask) const
{
	taskAssert(pGotoTask);
	static dev_float FORCE_STOP_DIST = 0.2f;
	if(IsFootDown() || pGotoTask->GetDistanceRemaining() < FORCE_STOP_DIST)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL 
const char* CTaskMotionAiming::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
		case State_Initial:					return "State_Initial";
		case State_StreamTransition:		return "State_StreamTransition";
		case State_PlayTransition:			return "State_PlayTransition";
		case State_StreamStationaryPose:	return "State_StreamStationaryPose";
		case State_StationaryPose:			return "State_StationaryPose";
		case State_CoverStep:				return "State_CoverStep";
		case State_StartMotionNetwork:		return "State_StartMotionNetwork";
		case State_StandingIdleIntro:		return "State_StandingIdleIntro";
		case State_StandingIdle:			return "State_StandingIdle";
		case State_SwimIdleIntro:			return "State_SwimIdleIntro";
		case State_SwimIdle:				return "State_SwimIdle";
		case State_SwimIdleOutro:			return "State_SwimIdleOutro";
		case State_SwimStrafe:				return "State_SwimStrafe";
		case State_StrafingIntro:			return "State_StrafingIntro";
		case State_StartStrafing:			return "State_StartStrafing";
		case State_Strafing:				return "State_Strafing";
		case State_StopStrafing:			return "State_StopStrafing";
		case State_Turn:					return "State_Turn";
		case State_Roll:					return "State_Roll";
		case State_Turn180:					return "State_Turn180";
		case State_Finish:					return "State_Finish";
		default:							taskAssert(0);		
	}
	return "State_Invalid";
}
#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////

#if !__FINAL 
void CTaskMotionAiming::Debug() const
{
#if DEBUG_DRAW
	const CPed* pPed = GetPed();

	Vec3V vStart, vEnd;
	vStart = pPed->GetTransform().GetPosition();

	// Render the desired velocity
	Vector3 vVel(pPed->GetDesiredVelocity());
	if(vVel.XYMag2() > square(SMALL_FLOAT))
	{
		vVel.Normalize();
		vEnd = Add(vStart, VECTOR3_TO_VEC3V(vVel));
		grcDebugDraw::Line(vStart, vEnd, Color_HotPink);
	}

	// Render the m_fStrafeDirection
	float fStrafeDirection = fwAngle::LimitRadianAngleSafe(m_fStrafeDirection + pPed->GetCurrentHeading());
	Matrix34 m;
	m.MakeRotateZ(fStrafeDirection);
	Vector3 vStrafeDirection(YAXIS);
	m.Transform3x3(vStrafeDirection);
	vEnd = Add(vStart, VECTOR3_TO_VEC3V(vStrafeDirection));
	grcDebugDraw::Line(vStart, vEnd, Color_pink);

	// Render the m_fDesiredStrafeDirection
	float fDesiredStrafeDirection = fwAngle::LimitRadianAngleSafe(m_fDesiredStrafeDirection + pPed->GetCurrentHeading());
	m.MakeRotateZ(fDesiredStrafeDirection);
	Vector3 vDesiredStrafeDirection(YAXIS);
	m.Transform3x3(vDesiredStrafeDirection);
	vEnd = Add(vStart, VECTOR3_TO_VEC3V(vDesiredStrafeDirection));
	grcDebugDraw::Line(vStart, vEnd, Color_blue2);

	// Base class
	CTaskMotionBase::Debug();
#endif // DEBUG_DRAW
}
#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Initial_OnEnter()
{
	CPed* pPed = GetPed();

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	taskAssert(pWeaponInfo);

	fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());
	if(fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateWeaponClipSetId))
	{
		SetAimClipSet(appropriateWeaponClipSetId);
	}

	if(pPed && !pPed->IsNetworkClone())
	{
		if(pPed->GetNetworkObject())
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			// Since motion state is InFrequent make sure that the remote gets set at the same time as the task state
			pPedObj->ForceResendAllData();

            if(pPed->IsLocalPlayer())
            {
                NetworkInterface::ForceCameraUpdate(*pPed);
            }
		}
	}

	// Request the turn clip set.
	fwMvClipSetId turnClipSetId = ChooseTurnClipSet();
	if(turnClipSetId != CLIP_SET_ID_INVALID)
	{
		m_TurnClipSetRequestHelper.RequestClipSet(turnClipSetId);
	}

	RequestProcessMoveSignalCalls();

	m_bSwitchActiveAtStart = g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Initial_OnUpdate()
{
	//Choose the correct state.
	if(m_AimPoseTransition.IsClipValid())
	{
		//Stream the transition.
		SetState(State_StreamTransition);
		m_fInitialIdleDir = FLT_MAX;
		m_bBlockIndependentMover = false;
	}
	else if(GetIsStationary())
	{
		//Stream the stationary pose.
		SetState(State_StreamStationaryPose);
		m_fInitialIdleDir = FLT_MAX;
		m_bBlockIndependentMover = false;
	}
	else
	{
		//Start the motion network.
		SetState(State_StartMotionNetwork);
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StreamTransition_OnUpdate()
{
	//Wait for the transition clip set to stream in.
	if(m_AimPoseTransitionClipSetRequestHelper.IsLoaded())
	{
		//Play the transition.
		SetState(State_PlayTransition);
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::PlayTransition_OnEnter()
{
	// Initialize the skip transition flag.
	m_bSkipTransition = true;

	// Grab the parent move network.
	CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();
	if(!pParentNetworkHelper)
	{
		// Something has gone wrong...
		AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s does not have parent move network\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Grab the clip.
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_AimPoseTransition.GetClipSetId(), m_AimPoseTransition.GetClipId());
	if(!pClip)
	{
		// Something has gone wrong...
		AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s could not find aimPoseTransition clip, clipId - %s, clipsetID - %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), m_AimPoseTransition.GetClipId() == CLIP_SET_ID_INVALID ? "invalid" : "valid", m_AimPoseTransition.GetClipSetId() == CLIP_SET_ID_INVALID ? "invalid" : "valid");
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Do not skip the transition.
	m_bSkipTransition = false;

	// Send the aiming transition request.
	AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s ms_AimingTransitionId requested, pParentNetworkHelper is in target state = %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), AILogging::GetBooleanAsString(pParentNetworkHelper->IsInTargetState()));
	pParentNetworkHelper->SendRequest(ms_AimingTransitionId);

	// Wait for the state to be entered.
	pParentNetworkHelper->WaitForTargetState(ms_OnEnterAimingTransitionId);
	
	// Set the rate.
	pParentNetworkHelper->SetFloat(ms_AimingTransitionRateId, m_AimPoseTransition.GetRate());

	// Set the clip.
	pParentNetworkHelper->SetClip(pClip, ms_AimingTransitionClipId);

	m_bMoveTransitionAnimFinished = false;
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::PlayTransition_OnUpdate()
{
	// No time slicing in this state, as move network will progress and leave us in a t-pose
	GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	// Check if the transition should be skipped.
	if(m_bSkipTransition)
	{
		// Check if the aiming is stationary.
		if(GetIsStationary())
		{
			// Stream the stationary pose.
			SetState(State_StreamStationaryPose);
		}
		else
		{
			// Skip the idle intro.
			m_bSkipIdleIntro = true;
			
			// Start the motion network.
			SetState(State_StartMotionNetwork);
		}
	}
	else
	{
		// Grab the parent move network.
		CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();
		if(pParentNetworkHelper)
		{
			// Check if the animation has started.
			if(pParentNetworkHelper->IsInTargetState())
			{
				// Check if the animation finished.
				if(m_bMoveTransitionAnimFinished)
				{
					// Check if the pose is stationary.
					if(GetIsStationary())
					{
						// Stream the stationary pose.
						SetState(State_StreamStationaryPose);
					}
					else
					{
						// Skip the idle intro.
						m_bSkipIdleIntro = true;
						
						// Start the motion network.
						SetState(State_StartMotionNetwork);
					}
				}
				else if (GetTimeInState() >= 5.0f)
				{
					// Something has gone wrong...
					AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s has been transitioning for longer than 5 seconds, bailing out\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
					SetState(State_Finish);
					return FSM_Continue;
				}
			}
			else if (GetTimeInState() >= 1.0f)
			{
				// Something has gone wrong...
				AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s has been transitioning for longer than a second, bailing out\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
				SetState(State_Finish);
				return FSM_Continue;
			}
			else
			{
				AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s waiting for on enter event %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), pParentNetworkHelper->GetNetworkPlayer() ? pParentNetworkHelper->GetNetworkPlayer()->GetWatchedEvent().GetCStr() : "NULL");
			}
		}
		else
		{
			// Something has gone wrong...
			AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s has no parent network helper\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
			SetState(State_Finish);
		}
	}
		
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::PlayTransition_OnExit()
{
	// We are done with the clip set, release it.
	m_AimPoseTransitionClipSetRequestHelper.Release();

	// Clear the aim pose transition.
	m_AimPoseTransition.Clear();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::PlayTransition_OnProcessMoveSignals()
{
	//Ensure the move network helper is valid.
	CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();
	if(pParentNetworkHelper)
	{
		m_bMoveTransitionAnimFinished |= pParentNetworkHelper->GetBoolean(ms_TransitionAnimFinishedId);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StreamStationaryPose_OnUpdate()
{
	// Wait for the stationary pose clip set to stream in.
	if(m_StationaryAimPoseClipSetRequestHelper.IsLoaded())
	{
		// Play the stationary pose.
		SetState(State_StationaryPose);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StationaryPose_OnEnter()
{
	// Grab the parent move network.
	CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();
	if(!pParentNetworkHelper)
	{
		return FSM_Continue;
	}

	// Grab the clip.
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_StationaryAimPose.GetLoopClipSetId(), m_StationaryAimPose.GetLoopClipId());
	if(!pClip)
	{
		return FSM_Continue;
	}

	// Send the stationary pose request.
	pParentNetworkHelper->SendRequest(ms_StationaryAimingPoseId);

	// Set the clip.
	pParentNetworkHelper->SetClip(pClip, ms_StationaryAimingPoseClipId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StationaryPose_OnUpdate()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StationaryPose_OnExit()
{
	// We are done with the clip set, release it.
	m_StationaryAimPoseClipSetRequestHelper.Release();

	// Clear the stationary aim pose.
	m_StationaryAimPose.Clear();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::CoverStep_OnEnter()
{
	const CPed& ped = *GetPed();
	CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (!pInCoverTask)
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s quitting due to null task in cover\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		return FSM_Quit;
	}
	
	static const fwMvClipSetId coverSteppingClipSetId("cover@weapon@core",0xD8BD0F2E);
	const crClip* pClip = fwClipSetManager::GetClip(coverSteppingClipSetId, pInCoverTask->GetCoverStepClipId());		
	taskAssertf(pClip, "Couldn't Find Cover Step Clip");
	m_MoveGunStrafingMotionNetworkHelper.SetClip(pClip, ms_CoverStepClipId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_CoverStepId);
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_CoverStepOnEnterId);

	m_bMoveCoverStepClipFinished = false;

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::CoverStep_OnUpdate()
{
	if (!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
		return FSM_Continue;

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (GetPed()->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	if (m_bMoveCoverStepClipFinished)
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::CoverStep_OnExit()
{
	const CPed& ped = *GetPed();
	CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (pInCoverTask)
	{
		pInCoverTask->SetWantsToTriggerStep(false);
	}
	return FSM_Continue;
}

void CTaskMotionAiming::CoverStep_OnProcessMoveSignals()
{
	m_bMoveCoverStepClipFinished |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_CoverStepClipFinishedId);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StartMotionNetwork_OnUpdate()
{
	CPed* pPed = GetPed();

	if(StartMotionNetwork())
	{
		bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
		{
			bFPSMode = true;
		}
#endif	//FPS_MODE_SUPPORTED

		if (pPed->GetIsSwimming() && bFPSMode)
		{
			SetState(State_Strafing);
		}
		else if (pPed->GetIsSwimming() && !pPed->GetIsInCover() && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsUnderwaterGun())
		{
			SetState(State_SwimIdleIntro);
		}
		else if(WillStartInMotion(pPed))
		{
			if(WillStartInStrafingIntro(pPed) && !m_bBlockIndependentMover)
			{
				SetState(State_StrafingIntro);
			}
			else
			{
				SetState(State_Strafing);
			}
		}
		else if(GetUseIdleIntro() && !GetPed()->GetIsInCover() && !m_bBlockIndependentMover)
		{
			SetState(State_StandingIdleIntro);
		}
		else
		{
			SetState(State_StandingIdle);
		}

		if(GetState() != State_StandingIdle)
		{
			m_fInitialIdleDir = FLT_MAX;
		}

		m_bBlockIndependentMover = false;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdleIntro_OnEnter()
{
	CPed* pPed = GetPed();

	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_IdleIntroOnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_IdleIntroId);

	// Store the initial direction
	const float fFacingHeading = fwAngle::LimitRadianAngleSafe(pPed->GetFacingDirectionHeading());
	const float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);//fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
	m_fStartDirection = SubtractAngleShorter(fDesiredHeading, fFacingHeading);
	// Store the initial desired heading
	m_fStartDesiredHeading = pPed->GetDesiredHeading();

	if(pPed->IsLocalPlayer())
	{
		m_bUpdateStandingIdleIntroFromPostCamera = true;
	}

	m_bMoveBlendOutIdleIntro = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdleIntro_OnUpdate()
{
	CPed* pPed = GetPed();

#if __BANK
	if(GetTimeInState() >= 4.5f)
	{
		aiDisplayf("CTaskMotionAiming - Ped %s stuck in State_StandingIdleIntro for longer than >= 4.5 seconds. OnFoot_IsNetworkActive - %s, Strafe_IsNetworkActive - %s, Strafe_IsInTargetState - %s",pPed->GetDebugName(),
			AILogging::GetBooleanAsString(m_MoveGunOnFootNetworkHelper.IsNetworkActive()),
			AILogging::GetBooleanAsString(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive()),
			AILogging::GetBooleanAsString(m_MoveGunStrafingMotionNetworkHelper.IsInTargetState()));
	}
#endif //__BANK

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, ConvertRadianToSignal(m_fStrafeDirection));
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

	// Send the desired heading signal
	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredHeadingId, ConvertRadianToSignal(m_fStartDirection));

		float fAnimRate = 1.f;
		if(!pPed->IsPlayer())
		{
			fAnimRate = ComputeRandomAnimRate();
		}
		else
		{
			fAnimRate = ms_Tunables.m_PlayerIdleIntroAnimRate;
		}

		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_IdleIntroAnimRateId, fAnimRate);
	}

	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	const float fFacingDirectionHeading = pPed->GetFacingDirectionHeading();
	const float fCurrentHeading = pPed->GetCurrentHeading();
	const float	fDesiredDirection = SubtractAngleShorter(fFacingDirectionHeading, fCurrentHeading);
	static dev_float CAN_EXIT_ANGLE = HALF_PI;
	if(Abs(fDesiredDirection) < CAN_EXIT_ANGLE)
	{
		// Combat roll
		if(GetMotionData().GetCombatRoll())
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;

			SetState(State_Roll);
			return FSM_Continue;
		}

		if(GetWantsToMove(pPed))
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;

			if(GetUseStartsAndStops() 
#if FPS_MODE_SUPPORTED
				|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && ms_Tunables.m_FPSForceUseStarts && !pPed->GetIsSwimming())
#endif // FPS_MODE_SUPPORTED
				)
			{
				SetState(State_StartStrafing);
			}
			else
			{
				SetState(State_Strafing);
			}
			return FSM_Continue;
		}
	}
#if __BANK
	else if(GetTimeInState() >= 4.5f)
	{
		aiDisplayf("CTaskMotionAiming - Ped %s stuck in State_StandingIdleIntro for longer than >= 4.5 seconds. fFacingDirectionHeading - %.2f, fCurrentHeading - %.2f, fDesiredDirection - %.2f",pPed->GetDebugName(), fFacingDirectionHeading, fCurrentHeading, fDesiredDirection);
	}
#endif //__BANK

	if(m_bMoveBlendOutIdleIntro)
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdleIntro_OnExit()
{
	m_bUpdateStandingIdleIntroFromPostCamera = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::StandingIdleIntro_OnProcessMoveSignals()
{
	m_bMoveBlendOutIdleIntro |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_BlendOutIdleIntroId);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdle_OnEnter()
{
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_IdleOnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_IdleId);

	if(m_fInitialIdleDir != FLT_MAX)
	{
		m_fStartDirection = m_fInitialIdleDir;
		m_fInitialIdleDir = FLT_MAX;
	}
	else
	{
		m_fStartDirection = 0.f;
	}
	m_fStandingIdleExtraTurnRate = 0.f;

// 	CPed* pPed = GetPed();
// 	float fStartPhase = ((float)pPed->GetRandomSeed())/((float)RAND_MAX_16);
// 	static fwMvFloatId s_IdleStartPhaseId("IdleStartPhase",0xD61168B2);
// 	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
// 	{
// 		m_MoveGunStrafingMotionNetworkHelper.SetFloat(s_IdleStartPhaseId, fStartPhase);
// 	}

	// Send signals when first entering state, as ProcessMoveSignals will not get called this frame
	if(!DoPostCameraSetHeading())
		StandingIdle_OnProcessMoveSignals();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdle_OnUpdate()
{
	CPed* pPed = GetPed();

	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] %s Ped %s Move network is not in target state - in State_StandingIdle\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif // FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	// Combat roll
	if(GetMotionData().GetCombatRoll())
	{
		SetState(State_Roll);
		return FSM_Continue;
	}

	if (GetWantsToCoverStep())
	{
		SetState(State_CoverStep);
		return FSM_Continue;
	}

	if(GetWantsToMove(pPed))
	{
		static dev_s32 STOP_COUNT = 1;
		if(m_iMovingStopCount >= STOP_COUNT
#if FPS_MODE_SUPPORTED
			|| bFPSMode
#endif // FPS_MODE_SUPPORTED
			)
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;
			if(GetUseStartsAndStops() 
#if FPS_MODE_SUPPORTED
				|| (bFPSMode && ms_Tunables.m_FPSForceUseStarts && !pPed->GetIsSwimming())
#endif // FPS_MODE_SUPPORTED
				)
			{
				SetState(State_StartStrafing);
				return FSM_Continue;
			}
			else
			{
				SetState(State_Strafing);
				return FSM_Continue;
			}
		}
		m_iMovingStopCount++;
	}
	else
	{
		m_iMovingStopCount = 0;
	}

	if(ShouldTurn())
	{
		SetState(State_Turn);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StandingIdle_OnExit()
{
	m_iMovingStopCount = 0;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::StandingIdle_OnProcessMoveSignals()
{
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);
	ComputeStandingIdleSignals();
	SendStandingIdleSignals();
#if FPS_MODE_SUPPORTED
	if (GetPed()->GetIsFPSSwimming())
	{
		SendFPSSwimmingMoveSignals();
	}
#endif	//FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::SwimIdleIntro_OnEnter()
{
	if (taskVerifyf(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive(), "SwimIdleIntro_OnEnter: Strafing network is not active!"))
	{
		m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_SwimIdleId);
	}

	//Fallback
	if(NetworkInterface::IsGameInProgress() && !m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		SetUpAimingNetwork();
	}

	if (taskVerifyf(m_MoveGunOnFootNetworkHelper.IsNetworkActive(), "SwimIdleIntro_OnEnter: OnFoot network is not active!"))
	{
		m_MoveGunOnFootNetworkHelper.SendRequest(ms_UseSwimIdleIntroId);
	}
	//m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_SwimIdleOnEnterId);

	const crClip *pClip = NULL;
	
	//Initialize water physics parameters:
	if (GetParent() && (GetParent()->GetPreviousState() == CTaskMotionPed::State_Diving) && GetPreviousState() != CTaskMotionAiming::State_StandingIdle)
	{
		m_bWasDiving = true;
		const fwMvClipSetId HARPOON_CLIPSET_ID("harpoon", 0xd95388c2);
		const fwMvClipId HARPOON_CLIP("TRANS_IDLE2AIM_UNDERW", 0x70445420);
		pClip = fwAnimManager::GetClipIfExistsBySetId(HARPOON_CLIPSET_ID, HARPOON_CLIP);
	}
	else 
	{
		m_bWasDiving = false;
		const fwMvClipSetId HARPOON_CLIPSET_ID("harpoon", 0xd95388c2);
		const fwMvClipId HARPOON_CLIP("TRANS_IDLE2AIM_SURFACE", 0xb12aee44);
		pClip = fwAnimManager::GetClipIfExistsBySetId(HARPOON_CLIPSET_ID, HARPOON_CLIP);
	}

	if (pClip && m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetClip(pClip, ms_SwimIdleIntroTransitionClipId);
	}

	GetPed()->SetIsSwimming(true);
	m_bSwimIntro = true;

	//Update the ped's desired heading (as it isn't updated on the first frame)
	CPed *pPed = GetPed();
	if (pPed)
	{
		const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
		const bool bIsAssistedAiming = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
		CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
		Vector3 vTargetPos;
		if (bIsAssistedAiming)
		{
			vTargetPos = targeting.GetLockonTargetPos();
			pPed->SetDesiredHeading(vTargetPos);
		}
		else
		{
			float aimHeading = aimCameraFrame.ComputeHeading();
			pPed->SetDesiredHeading(aimHeading);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdleIntro_OnUpdate()
{
	//If clip phase >= 0.75, switch to SwimIdle so anim blends with normal aim anims
	if (taskVerifyf(m_MoveGunOnFootNetworkHelper.IsNetworkActive(), "SwimIdleIntro_OnUpdate: OnFoot network is not active!"))
	{
		float fPhase = m_MoveGunOnFootNetworkHelper.GetFloat(ms_SwimIdleTransitionPhaseId);
		if (fPhase >= 0.75f)
		{	
			SetState(State_SwimIdle);
			return FSM_Continue;
		}
	}

	if(m_bSwimIntro)
	{
		static dev_float AIMING_FORWARD = 0.1f;
		if(Abs(CalcDesiredDirection()) < AIMING_FORWARD)
		{
			m_bSwimIntro = false;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdleIntro_OnExit()
{
	if (GetPed()->GetUseExtractedZ())
	{
		GetPed()->SetUseExtractedZ( false );
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::SwimIdle_OnEnter()
{
	//Send swim idle request if we didn't enter from the intro state (ie from strafing)
	if (GetPreviousState() != State_SwimIdleIntro)
	{
		if (taskVerifyf(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive(), "SwimIdle_OnEnter: Strafing network is not active!"))
		{
			m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_SwimIdleId);
		}
	}
	//m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_SwimIdleId);
	//m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_SwimIdleOnEnterId);
	GetPed()->SetIsSwimming(true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdle_OnUpdate()
{
	// Wait for Move network to be in sync
	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// If we're not swimming, break back into idle state
	if (!GetPed()->GetIsSwimming())
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	//Gets set in CTaskAimGunOnFoot::AimOutro_OnUpdate
	if (m_bPlayOutro)
	{
		m_bOutroFinished = false;
		SetState(State_SwimIdleOutro);
		return FSM_Continue;
	}

	if (!m_bPlayOutro)
	{
		m_bOutroFinished = true;
	}

	//Switch to strafe state
	CPed *pPed = GetPed();
	if(GetWantsToMove(pPed))
	{
		SetState(State_SwimStrafe);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdle_OnExit()
{
	if (GetPed()->GetUseExtractedZ())
	{
		GetPed()->SetUseExtractedZ( false );
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::SwimIdleOutro_OnEnter()
{
	if (taskVerifyf(m_MoveGunOnFootNetworkHelper.IsNetworkActive(), "SwimIdleOutro_OnEnter: OnFoot network is not active!"))
	{
		m_MoveGunOnFootNetworkHelper.SendRequest(ms_UseSwimIdleOutroId);
	}

	const fwMvClipSetId HARPOON_CLIPSET_ID("harpoon", 0xd95388c2);
	const fwMvClipId HARPOON_CLIP("TRANS_AIM2IDLE_UNDERW", 0x53470e12);
	const crClip *pClip = fwAnimManager::GetClipIfExistsBySetId(HARPOON_CLIPSET_ID, HARPOON_CLIP);
	if (pClip && m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetClip(pClip, ms_SwimIdleIntroTransitionClipId);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdleOutro_OnUpdate()
{
	if (taskVerifyf(m_MoveGunOnFootNetworkHelper.IsNetworkActive(), "SwimIdleOutro_OnUpdate: OnFoot network is not active!"))
	{
		static dev_float fTransitionThreshold = 0.5f;
		float fPhase = m_MoveGunOnFootNetworkHelper.GetFloat(ms_SwimIdleTransitionOutroPhaseId);
		if(fPhase >= fTransitionThreshold)
		{
			m_bOutroFinished = true;
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimIdleOutro_OnExit()
{
	GetPed()->SetUseExtractedZ( false );
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimStrafe_OnEnter()
{
	if (taskVerifyf(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive(), "SwimIdleIntro_OnEnter: Strafing network is not active!"))
	{
		m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_SwimStrafeId);
	}

	m_fSwimStrafePitch = 0.5f;
	m_fSwimStrafeRoll = 0.5f;
	m_vSwimStrafeSpeed = VEC3_ZERO;

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimStrafe_OnUpdate()
{
	CPed *pPed = GetPed();

	// If we're not swimming, break back into idle state
	if (!GetPed()->GetIsSwimming())
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	if (taskVerifyf(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive(), "SwimIdleIntro_OnEnter: Strafing network is not active!"))
	{
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_SwimStrafePitchId, m_fSwimStrafePitch);
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_SwimStrafeRollId, m_fSwimStrafeRoll);
		float fStrafeDirectionSignal = ConvertRadianToSignal(m_fStrafeDirection);
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
	}

	//Gets set in CTaskAimGunOnFoot::AimOutro_OnUpdate
	if (m_bPlayOutro)
	{
		m_bOutroFinished = false;
		SetState(State_SwimIdleOutro);
		return FSM_Continue;
	}

	//Switch to idle state if not moving
	if(!GetWantsToMove(pPed) && m_vSwimStrafeSpeed.Mag() < 0.25f)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAiming::SwimStrafe_OnExit()
{
	if (GetPed()->GetUseExtractedZ())
	{
		GetPed()->SetUseExtractedZ( false );
	}
	return FSM_Continue;
}

Vec3V_Out CTaskMotionAiming::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{
	CPed* pPed = GetPed();
	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	// Force into strafing if we're swimming in FPS mode
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->GetIsSwimming())
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (!pPed->GetIsSwimming() && !bFPSMode)
	{
		return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrixIn, fTimestep);
	}

	// FPS Strafe: calc base velocity for strafe movement
	Vector3 vBaseVelocity(VEC3_ZERO);
	if (pPed->GetIsSwimming() && bFPSMode)
	{
		vBaseVelocity = VEC3V_TO_VECTOR3(CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrixIn, fTimestep));
	}

	// We're swimming, so process swimming physics:
	Vector3	velocity(VEC3_ZERO);

	// Determine whether to use Swimming or Diving velocity calculations based on distance to water surface
	bool bDiving = false;
	CTaskMotionBase* pPrimaryTask = pPed->GetPrimaryMotionTask();
	if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
	{
		CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
		if (pTask)
		{
			bDiving = pTask->CheckForDiving();	
		}
	}

	// Add in swimming resistance
	Vector3 vSwimmingResistanceVelChange = bDiving ? ProcessDivingResistance(fTimestep) : ProcessSwimmingResistance(fTimestep);
	velocity += vSwimmingResistanceVelChange;

	// Clamp the velocity change to avoid large changes
	Vector3 vDesiredVelChange = bFPSMode ? velocity : velocity - pPed->GetVelocity();
	vDesiredVelChange+=pPed->m_Buoyancy.GetLastRiverVelocity();

	// don't clamp the XY velocity change for network clones, the network blending code prevents
	// large velocity changes in a frame but needs to know the target velocity for the ped. No blending is
	// done to the ped's Z position when it is water, so this still needs to be clamped here
	if(!pPed->IsNetworkClone())
	{
		if (bFPSMode)
		{
			float fVelocityZBeforeClamp = vDesiredVelChange.GetZ();
			ClampVelocityChange(pPed, vDesiredVelChange,fTimestep, 30.0f);	//dev_float MBIW_FRICTION_VEL_CHANGE_LIMIT_AIM	= 30.0f;
			vDesiredVelChange.z = fVelocityZBeforeClamp;
			// Clamp this vel change. Note this is done separately to the MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT so that wave intensity wont affect the XY swim speed
			vDesiredVelChange.z = Clamp(vDesiredVelChange.z, -20.0f, 20.0f);
		}
		else
		{
			ClampVelocityChange(pPed, vDesiredVelChange,fTimestep, 30.0f);	//dev_float MBIW_FRICTION_VEL_CHANGE_LIMIT_AIM	= 30.0f;
			// Clamp this vel change. Note this is done separately to the MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT so that wave intensity wont affect the XY swim speed
			vDesiredVelChange.z = Clamp(vDesiredVelChange.z, -4.0f, 4.0f);
		}
	}
	else
	{
		Vector3 velBeforeClamp = vDesiredVelChange;
		ClampVelocityChange(pPed,vDesiredVelChange,fTimestep, 30.0f);	//dev_float MBIW_FRICTION_VEL_CHANGE_LIMIT_AIM	= 30.0f;
		vDesiredVelChange.x = velBeforeClamp.x;
		vDesiredVelChange.y = velBeforeClamp.y;
	}

	velocity = bFPSMode ? vDesiredVelChange : pPed->GetVelocity() + vDesiredVelChange;

	if (GetState() == State_SwimStrafe)
	{
		CalculateSwimStrafingSpeed(updatedPedMatrixIn, velocity);
	}

#if FPS_MODE_SUPPORTED
	// FPS Swim: Store velocity if we've stopped input so we can transition to strafing smoothly
	if (pPed->GetIsFPSSwimming() && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		if (pControl)
		{
			Vector2 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
			TUNE_GROUP_FLOAT(FPS_SWIMMING, fInputThresholdToCacheSwimStrafeVelocity, 0.1f, 0.0f, 1.0f, 0.01f);
			if (vStickInput.Mag() < fInputThresholdToCacheSwimStrafeVelocity)
			{
				CTaskMotionPed* pTaskMotionPed = static_cast<CTaskMotionPed*>(GetParent());
				// Only set the velocity if it's not been initialized (zero)
				if (pTaskMotionPed && pTaskMotionPed->GetCachedSwimStrafeVelocity() == VEC3_ZERO && pTaskMotionPed->GetCachedSwimVelocity() == VEC3_ZERO)
				{
					pTaskMotionPed->SetCachedSwimStrafeVelocity(velocity + vBaseVelocity);
				}
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

    NetworkInterface::OnDesiredVelocityCalculated(*pPed);

	return VECTOR3_TO_VEC3V(velocity + vBaseVelocity);
}

void CTaskMotionAiming::CalculateSwimStrafingSpeed(Mat34V_ConstRef updatedPedMatrixIn, Vector3 &vVelocity)
{
	vVelocity.x = 0.0f;
	vVelocity.y = 0.0f;
	CControl *pControl = GetPed()->GetControlFromPlayer();
	if (pControl)
	{
		float fRollInput = pControl->GetPedWalkLeftRight().GetNorm();
		float fPitchInput = -pControl->GetPedWalkUpDown().GetNorm();

		const float fTimeStep = CalculateUnWarpedTimeStep(fwTimer::GetTimeStep());
		static dev_float fAccel = 0.75f;

		Approach(m_vSwimStrafeSpeed.x, fRollInput, fAccel, fTimeStep);
		Approach(m_vSwimStrafeSpeed.y, fPitchInput, fAccel, fTimeStep);

		// Set parameters for strafing anims:
		float fPitchNormalised = (fPitchInput + 1.0f) * 0.5f;
		float fRollNormalised = (fRollInput + 1.0f) * 0.5f;
		Approach(m_fSwimStrafePitch, fPitchNormalised, fAccel, fTimeStep);
		Approach(m_fSwimStrafeRoll, fRollNormalised, fAccel, fTimeStep);

		//Scale velocity relative to ped's heading
		const Vec3V mtrxAV = updatedPedMatrixIn.GetCol0();
		const Vec3V mtrxBV = updatedPedMatrixIn.GetCol1();

		const ScalarV extractedXV = ScalarV(m_vSwimStrafeSpeed.GetX());
		const ScalarV extractedYV = ScalarV(m_vSwimStrafeSpeed.GetY());

		vVelocity = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vVelocity), mtrxAV, extractedXV));
		vVelocity = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vVelocity), mtrxBV, extractedYV));

		// Up/Down input
		float fAscendDescendInput = 0.0f;
		if (pControl->GetVehicleSubAscend().IsDown() || pControl->GetVehicleSubDescend().IsDown())
		{
			float fAscend = pControl->GetVehicleSubAscend().GetNorm();
			float fDescend = pControl->GetVehicleSubDescend().GetNorm();
			fAscendDescendInput = fAscend - fDescend;
		}

		Approach(m_vSwimStrafeSpeed.z, fAscendDescendInput, fAccel, fTimeStep);
		static dev_float fZVelClamp = 0.5f;
		m_vSwimStrafeSpeed.z = Clamp(m_vSwimStrafeSpeed.z, -fZVelClamp, fZVelClamp); 
		const Vec3V mtrxCV = updatedPedMatrixIn.GetCol2();
		const ScalarV extractedZV = ScalarV(m_vSwimStrafeSpeed.GetZ());
		vVelocity = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vVelocity), mtrxCV, extractedZV));
		
	}
}

Vector3 CTaskMotionAiming::ProcessSwimmingResistance(float fTimestep)
{
	Vector3 desiredVelChange(VEC3_ZERO);
	Vector3 vExtractedVel;

	CPed* pPed = GetPed();

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// B*2257378: FPS: Get water level at camera X/Y position as it's slightly in front of the ped position (particularly when idle).
	if (bFPSMode)
	{
		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
		{
			Vector3 vFPSCamPos = camInterface::GetPos();
			vPedPosition.x = vFPSCamPos.x;
			vPedPosition.y = vFPSCamPos.y;
		}
	}
	
	Vector3 newPos = vPedPosition + ((pPed->GetVelocity() + desiredVelChange) * fTimestep);

	float fWaterLevel = 0.0f;
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(newPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
	{
#if __DEV
		Vector3 wavePos = newPos; wavePos.z = fWaterLevel;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.1f, Color_yellow, 1, 0, true);	
#endif

		static dev_float WATERHEIGHT_APPROACH_RATE = 0.125f; 	
		static dev_float MAX_WAVE_HEIGHT = 1.0f; 	
		Vector3 vRiverHitPos, vRiverHitNormal;
		bool bInRiver = pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal) && (vRiverHitPos.z+REJECTIONABOVEWATER) > newPos.z;
		
		// B*2257378: FPS: Test water level at several points around ped position to check for large incoming waves. Helps ensure we always keep our head above water.
		Vector3 vHighestWaterLevelPosition = vPedPosition;
		if (bFPSMode)
		{
			fWaterLevel = CTaskMotionSwimming::GetHighestWaterLevelAroundPos(pPed, vPedPosition, fWaterLevel, vHighestWaterLevelPosition);
		}

		m_fLastWaveDelta = 0.0f;
		float waterHeight = 0.475f;	// dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_NOWAVES_AIM		= 0.475f;
		if (GetPed()->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL && !bFPSMode)
			waterHeight = 0.6f;	//dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_SURFACING_AIM	= 0.6f;
		else if (!bInRiver) //scale based on wave height (not in FPS mode, we want to keep at a consistent height above water).
		{
			Water::GetWaterLevel(vHighestWaterLevelPosition, &m_fLastWaveDelta, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL, NULL, pPed->GetCanUseHighDetailWaterLevel());
			float fNoWaves = m_fLastWaveDelta;
			Water::GetWaterLevelNoWaves(vHighestWaterLevelPosition, &fNoWaves, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
			m_fLastWaveDelta -= fNoWaves;

			if (!bFPSMode)
			{
				float t = Min(1.0f, m_fLastWaveDelta/MAX_WAVE_HEIGHT);
				waterHeight = waterHeight + (t*(0.525f-waterHeight));	//dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_AIM = 0.525f;
			}
		}

		static dev_float FEMALE_CLIP_HEIGHT_OFFSET = -0.075f;
		if (!pPed->IsMale())
			waterHeight+=FEMALE_CLIP_HEIGHT_OFFSET;

		// Increase ped height above water in FPS mode (from B*1959239/CL 5910767)
		if (bFPSMode)
		{
			waterHeight += CTaskMotionSwimming::ms_fFPSClipHeightOffset;
		}

		// Lerp to the water height goal in FPS mode (this may be passed in from the swimming task and we don't want to instantly pop).
		if (bFPSMode && m_fHeightBelowWaterGoal != waterHeight && m_fHeightBelowWaterGoal != CTaskMotionSwimming::s_fHeightBelowWaterGoal)
		{		
			static dev_float WATERHEIGHT_GOAL_APPROACH_RATE_FPS = 0.33f;
			Approach(m_fHeightBelowWaterGoal, waterHeight, WATERHEIGHT_GOAL_APPROACH_RATE_FPS, fTimestep);
		}
		else
		{
			m_fHeightBelowWaterGoal = waterHeight; // rage::Max(waterHeight, fWaterLevel - vPedPosition.z);		
		}

		// Increase approach rate in FPS mode (from B*1959239/CL 5910767)
		if (bFPSMode)
		{
			m_fHeightBelowWater = m_fHeightBelowWaterGoal;	
		}
		else
		{
			Approach(m_fHeightBelowWater, m_fHeightBelowWaterGoal, WATERHEIGHT_APPROACH_RATE, fTimestep);
		}

#if __DEV
		wavePos.z = m_fHeightBelowWater + vPedPosition.z;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.05f, Color_green, 1, 0, false);	
		wavePos.z = m_fHeightBelowWaterGoal + vPedPosition.z;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.05f, Color_red, 1, 0, false);	

#endif

		// AS we are floating on waters surface, track waves
		// Try to match peds vertical velocity with the water's
		// Desired displacement = water height - Height below water - currentPosition.z
		//Displayf("m_fHeightBelowWaterGoal = %f, Actual = %f, PedPos = %f,  fWaterLevel = %f",m_fHeightBelowWaterGoal, m_fHeightBelowWater, vPedPosition.z, fWaterLevel);
		fWaterLevel -= m_fHeightBelowWater + vPedPosition.z;	

		float reqZSpeed = fWaterLevel / fTimestep;

		// Smooth Z speed if changes are small.
		if (bFPSMode)
		{
			float fCurrentZSpeed = pPed->GetVelocity().z;
			static dev_float fZSpeedDeltaThreshold = 0.1f;
			if (Abs(reqZSpeed - fCurrentZSpeed) < fZSpeedDeltaThreshold)
			{
				static dev_float fRate = 0.6f;
				Approach(fCurrentZSpeed, reqZSpeed, fRate, fTimestep);
				reqZSpeed = fCurrentZSpeed;
			}
		}

		desiredVelChange.z = reqZSpeed;// - pPed->GetVelocity().z;

		// Clamp this vel change. Note this is done separately to the MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT so that wave intensity wont affect the XY swim speed
		if (bFPSMode)
		{
			static dev_float MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM_FPS = 8.0f;
			desiredVelChange.z = Clamp(desiredVelChange.z, -MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM_FPS, MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM_FPS);
		}
		else
		{
			static dev_float MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM = 4.0f;
			desiredVelChange.z = Clamp(desiredVelChange.z, -MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM, MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM);
		}

		float storedVel = desiredVelChange.z;
		desiredVelChange.z = m_PreviousVelocityChange;		
		static float sfRiverVelApproachRate = 2.0f;


		if (bInRiver)
			Approach(desiredVelChange.z, storedVel, sfRiverVelApproachRate, fTimestep);
		else 
			desiredVelChange.z = storedVel;
		m_PreviousVelocityChange = desiredVelChange.z;
		//Displayf("desiredVelChange = %f, Actual = %f, fWaterLevel = %f",storedVel, desiredVelChange.z, fWaterLevel);
	}	// End in-water check

	pPed->m_Buoyancy.SetShouldStickToWaterSurface(true);

	pPed->SetUseExtractedZ( true );
	m_bResetExtractedZ = true;

	// In case pPed is in rag doll (shouldn't really happen)
	if(!pPed->GetUsingRagdoll() && pPed->GetCurrentPhysicsInst()->IsInLevel())
	{
		// Apply new speed to character by adding force to centre of gravity
		return desiredVelChange;
	}

	return VEC3_ZERO;
}

Vector3 CTaskMotionAiming::ProcessDivingResistance(float fTimestep)
{
	CPed * pPed = GetPed();

	Vector3 desiredVelChange(VEC3_ZERO);
	Vector3 vExtractedVel;

	pPed->m_Buoyancy.SetShouldStickToWaterSurface(false);

	pPed->SetUseExtractedZ(true);
	m_bResetExtractedZ = true;

	//If ped gets close to water surface, switch to surface physics
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 newPos = vPedPosition + ((pPed->GetVelocity() + desiredVelChange) * fTimestep);

	float fBuoyancyMult = 1.0f;
	float fWaterLevel = 0.0f;
	if (pPed->m_Buoyancy.GetWaterLevelIncludingRivers(newPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
	{
		// Check if we are close to water surface and force ourselves downwards if we're too close
		TUNE_GROUP_FLOAT(DIVE_AIM_TUNE, SURFACE_DIST_LIMIT, 1.5f, 0.0f, 5.0f, 0.01f); 
		float fPedDistToSurface = fWaterLevel - vPedPosition.z;

#if __DEV
		Vector3 vWavePosition = vPedPosition; vWavePosition.z = fWaterLevel;
		Color32 color = Color_green;
		if (fPedDistToSurface < SURFACE_DIST_LIMIT)
		{
			color = Color_red;
		}
		CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vWavePosition), color, 100);
#endif
		
		//Don't set buoyancy multiplier while diving down (to avoid bouncing back up to surface as soon as we dive)
		bool bIsDivingDown = false;
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_DIVING && GetSubTask()->GetState() == CTaskMotionDiving::State_DivingDown)
		{
			bIsDivingDown = true;
		}
		
		TUNE_GROUP_FLOAT(DIVE_AIM_TUNE, fMinDiveClearance, 2.0f, 0.0f, 5.0f, 0.01f);
		float fGroundClearance = CTaskMotionDiving::TestGroundClearance(pPed, fMinDiveClearance, 0.5f);

		// If we aren't diving down and aren't too close to the water surface, decide on whether to force us down or up
		if (!bIsDivingDown && fPedDistToSurface <= SURFACE_DIST_LIMIT)
		{
			// If we have enough space below us and we aren't too close to the surface, force us downwards
			TUNE_GROUP_FLOAT(DIVE_AIM_TUNE, FORCE_UP_DIST, 0.1f, 0.0f, 1.0f, 0.01f); 
			if (fGroundClearance < 0.0f && fPedDistToSurface > fWaterLevel - FORCE_UP_DIST)
			{
				TUNE_GROUP_FLOAT(DIVE_AIM_TUNE, WATERHEIGHT_APPROACH_RATE, 1.0f, 0.0f, 10.0f, 0.01f); 
				//Scale the approach rate by current distance to surface
				float fDistanceModifier = SURFACE_DIST_LIMIT / fPedDistToSurface;
				const float fApproachRate = WATERHEIGHT_APPROACH_RATE * fDistanceModifier;

				m_fHeightBelowWater = fPedDistToSurface;
				Approach(m_fHeightBelowWater, SURFACE_DIST_LIMIT, fApproachRate, fTimestep);
				
				// Try try to match peds vertical (downwards) velocity with the water's
				// Desired displacement = water height - Height below water - currentPosition.z
				fWaterLevel -= m_fHeightBelowWater + vPedPosition.z;	
				desiredVelChange.z = fWaterLevel / fTimestep;

				// Clamp this velocity change
				dev_float MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM = 4.0f;
				desiredVelChange.z = Clamp(desiredVelChange.z, -MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM, MBIW_SWIM_BUOYANCY_ACCEL_LIMIT_AIM);
			}
			else	// We don't have enough ground clearance to move down or we are too close to surface, so force us upwards
			{
				// Restore buoyancy multiplier to 1.0f so swimming physics work as normal
				fBuoyancyMult = 1.0f;
				m_bWasDiving = false;
			}
		}
	}

#if FPS_MODE_SUPPORTED
	// FPS Swim:  Ensure we don't clip with the ground - if we are, force us upwards
	if (m_bUsingMotionAimingWhileSwimmingFPS)
	{
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fMinGroundClearanceDiving, 1.5f, 0.0f, 10.0f, 0.01f); 
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fMinGroundClearanceScuba, 1.75f, 0.0f, 10.0f, 0.01f); 
		float fMinGroundClearance = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba) ? fMinGroundClearanceScuba : fMinGroundClearanceDiving;
		float fGroundClearance = CTaskMotionDiving::TestGroundClearance(pPed, fMinGroundClearance);
		if (fGroundClearance != -1.0f && fGroundClearance < fMinGroundClearance)
		{
			TUNE_GROUP_FLOAT(FPS_SWIMMING, fGroundClearanceApproachRate, 1.0f, 0.0f, 10.0f, 0.01f); 

			//Scale the approach rate by current distance to the ground
			float fDistanceModifier = fMinGroundClearance / fGroundClearance;
			const float fApproachRate = fGroundClearanceApproachRate * fDistanceModifier;
			
			Approach(m_fSwimStrafeGroundClearanceVelocity, fMinGroundClearance, fApproachRate, fTimestep);
			desiredVelChange.z += m_fSwimStrafeGroundClearanceVelocity;
		}
		else
		{
			m_fSwimStrafeGroundClearanceVelocity = 0.0f;
		}
	}
#endif	//FPS_MODE_SUPPORTED

	// In case pPed is in rag doll (shouldn't really happen)
	if(!pPed->GetUsingRagdoll() && pPed->GetCurrentPhysicsInst()->IsInLevel())
	{ 
		// If diving in rapids force to the surface!
		if(CTaskMotionSwimming::CheckForRapids(pPed))
		{
			static dev_float sfUnderwaterRapidsBuoyancyMult = 3.0f;
			pPed->m_Buoyancy.m_fForceMult = sfUnderwaterRapidsBuoyancyMult;
		}
		else
		{
			pPed->m_Buoyancy.m_fForceMult = fBuoyancyMult;	//Set to 0 so we don't float to the surface while aiming
		}

		// Apply new speed to character by adding force to centre of gravity
		return desiredVelChange;
	}

	return VEC3_ZERO;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StrafingIntro_OnEnter()
{
	CPed* pPed = GetPed();

	// Start the subtask
	CTaskMotionAimingTransition* pTask = rage_new CTaskMotionAimingTransition(CClipNetworkMoveInfo::ms_NetworkTaskMotionOnFootToAimingTransition, pPed->GetPedModelInfo()->GetMovementToStrafeClipSet(), m_bUseLeftFootStrafeTransition, false, true, FLT_MAX, m_TransitionDirection);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_MovingIntroId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveGunStrafingMotionNetworkHelper, ms_MovingIntroOnEnterId, ms_MovingIntroNetworkId);
	SetNewTask(pTask);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StrafingIntro_OnUpdate()
{
	CPed* pPed = GetPed();

	// Speed
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);

	// Desired speed
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
	float fDesiredStrafeSpeed = vDesiredMBR.Mag() / MOVEBLENDRATIO_SPRINT;
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeSpeedId, fDesiredStrafeSpeed);

	// Direction
	float fStrafeDirectionSignal = ConvertRadianToSignal(m_fStrafeDirection);
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
	// Desired direction
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeDirectionId, ConvertRadianToSignal(m_fDesiredStrafeDirection));

	if (m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_MovingIntroId);
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	const float fFacingDirectionHeading = pPed->GetFacingDirectionHeading();
	const float fCurrentHeading = pPed->GetCurrentHeading();
	const float	fDesiredDirection = SubtractAngleShorter(fFacingDirectionHeading, fCurrentHeading);
	static dev_float CAN_EXIT_ANGLE = 2.35f;
	if(Abs(fDesiredDirection) < CAN_EXIT_ANGLE)
	{
		// Combat roll
		if(GetMotionData().GetCombatRoll())
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;

			SetState(State_Roll);
			return FSM_Continue;
		}
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Once transition finished, continue to strafing
		SetState(State_Strafing);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StartStrafing_OnEnter()
{
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_StartStrafingOnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_StartStrafingId);

	// Initialise the rate
	m_fMovementRate = GetFloatPropertyFromClip(m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(), ms_Walk0ClipId, "StartRate");

	// Reset flags
	m_bIsRunning = false;
	m_bMoveCanEarlyOutForMovement = false;
	m_bMoveBlendOutStart = false;

	// Init the move blend ratio to the desired
	m_fStrafeDirection = m_fDesiredStrafeDirection;

	bool bClonePlayerInFirstPerson = GetPed()->IsNetworkClone() && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true);

	//B* 2370552 stops clone player feet shuffling
	if(!bClonePlayerInFirstPerson)
	{
		m_fStartDirection = m_fDesiredStrafeDirection;
	}

	// All strafe starts start on right foot
	m_bLastFootLeft = false;
	m_MoveGunStrafingMotionNetworkHelper.SetFlag(m_bLastFootLeft, ms_LastFootLeftId);

#if FPS_MODE_SUPPORTED
	if(GetPreviousState() != State_Roll)
	{
		m_fNormalisedVel = 0.f;
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StartStrafing_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Strafing signals
	// Speed	
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);

	// Desired speed
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
	float fDesiredStrafeSpeed = vDesiredMBR.Mag();
	// Clamp if holding a heavy weapon
	if(pPed->GetWeaponManager())
	{
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy())
		{
			fDesiredStrafeSpeed = Min(fDesiredStrafeSpeed, MOVEBLENDRATIO_WALK);
		}
	}
	// Normalise
	fDesiredStrafeSpeed /= MOVEBLENDRATIO_SPRINT;
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeSpeedId, fDesiredStrafeSpeed);

	// Direction
	float fStrafeDirectionSignal = ConvertRadianToSignal(m_fStrafeDirection);
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);

	// Desired direction
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeDirectionId, ConvertRadianToSignal(m_fDesiredStrafeDirection));

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

	// If we've switched to running, update the rate
	if(m_bIsRunning && !m_bWasRunning)
	{
		m_fMovementRate = GetFloatPropertyFromClip(m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(), ms_Run0ClipId, "StartRate");
	}

	// Wait for Move network to be in sync
	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = IsTransitionBlocked();

	if(!bTransitionBlocked)
	{
		// Check for stopping
		if(!GetWantsToMove(pPed))
		{
			if(GetUseStartsAndStops() 
#if FPS_MODE_SUPPORTED
				|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && ms_Tunables.m_FPSForceUseStops && !pPed->GetIsSwimming() && GetTimeInState() >= ms_Tunables.m_FPSForceUseStopsMinTimeInStartState)
#endif // FPS_MODE_SUPPORTED
				)
			{
				const CTaskMoveGoToPointAndStandStill* pGotoTask = CTaskHumanLocomotion::GetGotoPointAndStandStillTask(pPed);
				if(pGotoTask)
				{
					if(IsGoingToStopThisFrame(pGotoTask))
					{
						SetState(State_StopStrafing);
					}
				}
				else
				{
					SetState(State_StopStrafing);
				}
			}
			else
			{
				SetState(State_StandingIdle);
			}
			return FSM_Continue;
		}

		// Check if we can early out if we've changed our mind
		if(m_bMoveCanEarlyOutForMovement)
		{
			float fHeadingDiff = SubtractAngleShorter(m_fStrafeDirection, m_fDesiredStrafeDirection);
			if(Abs(fHeadingDiff) > MIN_ANGLE)
			{
				SetState(State_Strafing);
				return FSM_Continue;
			}
		}

		// Combat roll
		if(GetMotionData().GetCombatRoll())
		{
			SetState(State_Roll);
			return FSM_Continue;
		}

		// Should we start blending out?
		if(m_bMoveBlendOutStart)
		{
			SetState(State_Strafing);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::StartStrafing_OnProcessMoveSignals()
{
	m_bMoveCanEarlyOutForMovement	|= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
	m_bMoveBlendOutStart			|= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_BlendOutStartId);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Strafing_OnEnter()
{
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_MovingOnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_MovingId);

	m_bSnapMovementWalkRate = true;
	m_bInStrafingTransition = false;

	m_pPreviousDesiredDirections->ResetDirections(m_fDesiredStrafeDirection, fwTimer::GetTimeInMilliseconds());

	Strafing_OnProcessMoveSignals();

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		// This matches the transition condition in MoVE
		if(m_fStrafeSpeed > 0.5f)
		{
			m_bIsRunning = true;
		}

		// Reset this, unless we have come through start strafing
		if(GetPreviousState() != State_StartStrafing && GetPreviousState() != State_StartMotionNetwork && GetPreviousState() != State_Roll && GetPreviousState() != State_Turn180 && GetPreviousState() != State_StrafingIntro)
		{
			m_fNormalisedVel = 0.f;
		}
	}

	// Set ped diving variation if underwater
	if (GetPed()->GetIsFPSSwimmingUnderwater())
	{
		CTaskMotionSwimming::OnPedDiving(*GetPed());
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Strafing_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Speed
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);

	// Desired direction
	float fDesiredStrafeDirectionSignal = ConvertRadianToSignal(m_fDesiredStrafeDirection);
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeDirectionId, fDesiredStrafeDirectionSignal);

	// Strafe direction
	taskAssert(m_fStrafeDirection != FLT_MAX);
	float fStrafeDirectionSignal = ConvertRadianToSignal(m_fStrafeDirection);	
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

// 	if(UsingLeftTriggerAimMode() || CTaskHumanLocomotion::CheckClipSetForMoveFlag(m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(), ms_SkipMovingTransitionsId))
// 	{
// 		m_MoveGunStrafingMotionNetworkHelper.SetFlag(true, ms_BlockMovingTransitionsId);
// 
// 		if(m_fDesiredStrafeDirection >= BWD_FWD_BORDER_MIN && m_fDesiredStrafeDirection <= BWD_FWD_BORDER_MAX)
// 		{
// 			m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_MovingFwdId);
// 		}
// 		else
// 		{
// 			m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_MovingBwdId);
// 		}
// 	}
// 	else
// 	{
// 		m_MoveGunStrafingMotionNetworkHelper.SetFlag(false, ms_BlockMovingTransitionsId);
// 	}

	// Wait for Move network to be in sync
	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	// Can we transition
	const bool bTransitionBlocked = IsTransitionBlocked() 
#if FPS_MODE_SUPPORTED
		&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true)
#endif // FPS_MODE_SUPPORTED
		;

	bool bUseTurn180 = (GetUseTurn180() FPS_MODE_SUPPORTED_ONLY(|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && ms_Tunables.m_FPSForceUse180s) && !pPed->GetIsSwimming()));

	if(!bTransitionBlocked)
	{
		// Combat roll
		if(GetMotionData().GetCombatRoll())
		{
			SetState(State_Roll);
			return FSM_Continue;
		}

		static dev_s32 STOP_COUNT = 125;

		// Check for stopping
		if(!GetWantsToMove(pPed))
		{
			if(GetUseStartsAndStops() 
#if FPS_MODE_SUPPORTED
				|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && ms_Tunables.m_FPSForceUseStops && !pPed->GetIsSwimming())
#endif // FPS_MODE_SUPPORTED
				)
			{
				const CTaskMoveGoToPointAndStandStill* pGotoTask = CTaskHumanLocomotion::GetGotoPointAndStandStillTask(pPed);
				if(pGotoTask)
				{
					if(IsGoingToStopThisFrame(pGotoTask))
					{
						SetState(State_StopStrafing);
						return FSM_Continue;
					}
				}
				else
				{
					if(m_iMovingStopCount >= STOP_COUNT)
					{
						SetState(State_StopStrafing);
						return FSM_Continue;
					}
				}
			}
			else
			{
				if(m_iMovingStopCount >= STOP_COUNT || !bUseTurn180)
				{
					SetState(State_StandingIdle);
					return FSM_Continue;
				}
			}

			m_iMovingStopCount += fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			m_iMovingStopCount = 0;

			if(bUseTurn180)
			{
				// Check for 180 turns
				if(GetTimeRunning() > 1.f && GetTimeInState() > 0.2f)
				{
					float fPreviousDesiredDirection;
					if(m_pPreviousDesiredDirections->GetDirectionForTime(STOP_COUNT, fPreviousDesiredDirection))
					{
						float fLastPreviousDesiredDirection;
						m_pPreviousDesiredDirections->GetDirectionForTime(0, fLastPreviousDesiredDirection);

						if(!IsNearZero(pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2()) && 
							(Abs(SubtractAngleShorter(m_fDesiredStrafeDirection, fPreviousDesiredDirection)) > ms_Tunables.Turn180ActivationAngle) && 
							(Abs(SubtractAngleShorter(m_fDesiredStrafeDirection, fLastPreviousDesiredDirection)) < ms_Tunables.Turn180ConsistentAngleTolerance))
						{
							SetState(State_Turn180);
							return FSM_Continue;
						}
					}
				}
			}
		}
	}

	// Update the previous direction
	if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0)
	{
		m_pPreviousDesiredDirections->RecordDirection(m_fDesiredStrafeDirection);
	}
	else
	{
		m_pPreviousDesiredDirections->ResetDirections(0.f, fwTimer::GetTimeInMilliseconds());
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Strafing_OnExit()
{
	CPed* pPed = GetPed();

	m_bInStrafingTransition = false;
	m_iMovingStopCount = 0;

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

void CTaskMotionAiming::Strafing_OnProcessMoveSignals()
{
	if(m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_MovingTransitionOnExitId))
	{
		m_bInStrafingTransition = false;
	}
	
	if(m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_MovingTransitionOnEnterId))
	{
		m_bInStrafingTransition = true;

		// Restrict it to the boundaries of fwd/bwd
		float fDiff1 = SubtractAngleShorter(m_fStrafeDirection, BWD_FWD_BORDER_MIN);
		float fDiff2 = SubtractAngleShorter(m_fStrafeDirection, BWD_FWD_BORDER_MAX);
		
		if(Abs(fDiff1) < Abs(fDiff2))
		{
			m_fStrafeDirectionAtStartOfTransition = BWD_FWD_BORDER_MIN;
		}
		else
		{
			m_fStrafeDirectionAtStartOfTransition = BWD_FWD_BORDER_MAX;
		}
	}

	if(m_bInStrafingTransition)
	{
		GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

#if FPS_MODE_SUPPORTED
	if (GetPed()->GetIsFPSSwimming())
	{
		SendFPSSwimmingMoveSignals();
	}
#endif	//FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StopStrafing_OnEnter()
{
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_StopStrafingOnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_StopStrafingId);

	// Initialise the rate
	if(m_bIsRunning)
	{
		m_fMovementRate = GetFloatPropertyFromClip(m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(), ms_Run0ClipId, "StopRate");
	}
	else
	{
		m_fMovementRate = GetFloatPropertyFromClip(m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(), ms_Walk0ClipId, "StopRate");
	}

	m_bMoveCanEarlyOutForMovement = false;
	m_bMoveBlendOutStop = false;
	m_bMoveStartStrafingOnExit = false;
	m_bMoveMovingOnExit = false;

	// Clear flag
	m_bFullyInStop = false;

	m_fStopDirection = m_fStrafeDirection;

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if(m_fNormalisedVel != FLT_MAX)
			m_fNormalisedStopVel = m_fNormalisedVel;
		else
			m_fNormalisedStopVel = CalculateNormaliseVelocity(true);
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StopStrafing_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Strafing signals	
	// Direction
	float fStopDirectionSignal = ConvertRadianToSignal(m_fStopDirection);	
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStopDirectionSignal);

	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, fStopDirectionSignal);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

	// Check if we have fully transitioned
	if(m_bMoveStartStrafingOnExit || m_bMoveMovingOnExit)
	{
		m_bFullyInStop = true;
	}

	// Wait for Move network to be in sync
	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	// Combat roll
	if(GetMotionData().GetCombatRoll())
	{
		SetState(State_Roll);
		return FSM_Continue;
	}

	// Check for starting
	if(m_bMoveCanEarlyOutForMovement && GetWantsToMove(pPed))
	{
		static dev_s32 STOP_COUNT = 1;
		if(m_iMovingStopCount >= STOP_COUNT)
		{
			m_fStrafeDirection = m_fDesiredStrafeDirection;
			if(GetUseStartsAndStops()
#if FPS_MODE_SUPPORTED
				|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && ms_Tunables.m_FPSForceUseStarts && !pPed->GetIsSwimming())
#endif // FPS_MODE_SUPPORTED
				)
			{
				SetState(State_StartStrafing);
				return FSM_Continue;
			}
			else
			{
				SetState(State_Strafing);
				return FSM_Continue;
			}
		}

		m_iMovingStopCount++;
	}
	else
	{
		m_iMovingStopCount = 0;
	}

	// Should we start blending out?
	if(m_bMoveBlendOutStop FPS_MODE_SUPPORTED_ONLY(|| (bFPSMode && pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSPlacingProjectile))))
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::StopStrafing_OnExit()
{
	m_bFullyInStop = false;
	m_iMovingStopCount = 0;
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::StopStrafing_OnProcessMoveSignals()
{
	m_bMoveCanEarlyOutForMovement |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_CanEarlyOutForMovementId);
	m_bMoveBlendOutStop |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_BlendOutStopId);
	m_bMoveStartStrafingOnExit |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_StartStrafingOnExitId);
	m_bMoveMovingOnExit |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_MovingOnExitId);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Turn_OnEnter()
{
	// Assert that the turn clip set is valid and loaded.
	fwMvClipSetId turnClipSetId = m_TurnClipSetRequestHelper.GetClipSetId();
	taskAssertf(turnClipSetId != CLIP_SET_ID_INVALID, "The turn clip set is invalid.");
	taskAssertf(m_TurnClipSetRequestHelper.IsLoaded(), "The turn clip set is not loaded.");
	
	// Ensure the parent move network helper is valid.
	CMoveNetworkHelper* pParentMoveNetworkHelper = GetParent()->GetMoveNetworkHelper();
	if(!taskVerifyf(pParentMoveNetworkHelper, "The parent move network helper is invalid."))
	{
		return FSM_Continue;
	}
	
	// Set the clip set.
	m_ParentClipSetId = pParentMoveNetworkHelper->GetClipSetId();
	pParentMoveNetworkHelper->SetClipSet(turnClipSetId);

	// Set the blend factor.
	float fTurnBlendFactor = CalculateTurnBlendFactor();
	pParentMoveNetworkHelper->SetFloat(ms_AimTurnBlendFactorId, fTurnBlendFactor);

	// Send the request.
	pParentMoveNetworkHelper->SendRequest(ms_AimTurnId);
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s PED %s requesting AimTurn in parent MoVE network. IsCrouched - %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetIsCrouched() ? "TRUE":"FALSE");

	// Wait for the turn to finish.
	pParentMoveNetworkHelper->WaitForTargetState(ms_AimTurnAnimFinishedId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Turn_OnUpdate()
{
	// No time slicing in this state, as move network will progress and leave us in a t-pose
	GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	// Ensure the parent move network helper is valid.
	CMoveNetworkHelper* pParentMoveNetworkHelper = GetParent()->GetMoveNetworkHelper();
	if(taskVerifyf(pParentMoveNetworkHelper, "The parent move network helper is invalid."))
	{
		// Check if the target state has been reached.
		if(pParentMoveNetworkHelper->IsInTargetState())
		{
			// This task needs a restart.  This appears to be how the sub-networks are re-inserted after the state changes.
			m_bNeedsRestart = true;
		}
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (GetPed()->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionAiming::Turn_OnExit()
{
	// Ensure the parent move network helper is valid.
	CMoveNetworkHelper* pParentMoveNetworkHelper = GetParent()->GetMoveNetworkHelper();
	if(!taskVerifyf(pParentMoveNetworkHelper, "The parent move network helper is invalid."))
	{
		return FSM_Continue;
	}
	
	// Restore the clip set.
	pParentMoveNetworkHelper->SetClipSet(m_ParentClipSetId);
	m_ParentClipSetId = CLIP_SET_ID_INVALID;
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::Turn_OnProcessMoveSignals()
{
	if (GetPed()->IsNetworkClone())
	{
		if (CMoveNetworkHelper* pParentMoveNetworkHelper = GetParent()->GetMoveNetworkHelper())
		{
			if (pParentMoveNetworkHelper->GetBoolean(ms_AimTurnAnimFinishedId))
			{
				m_bNeedsRestart = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::FSM_Return CTaskMotionAiming::Roll_OnEnter()
{
	// Start the subtask
	CTaskCombatRoll* pTask = rage_new CTaskCombatRoll();
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_RollId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveGunStrafingMotionNetworkHelper, ms_RollOnEnterId, ms_RollNetworkId);
	SetNewTask(pTask);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::FSM_Return CTaskMotionAiming::Roll_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Once transition finished, continue to strafing
		SetState(State_Strafing);
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if (GetPed()->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::FSM_Return CTaskMotionAiming::Roll_OnExit()
{
	CPed* pPed = GetPed();

	pPed->GetMotionData()->SetCombatRoll(false);

	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMBAT_ROLL)
	{
		m_fStrafeDirection = static_cast<const CTaskCombatRoll*>(GetSubTask())->GetDirection();
		m_fStrafeDirectionAdditive = m_fStrafeDirection;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::FSM_Return CTaskMotionAiming::Turn180_OnEnter()
{
	m_MoveGunStrafingMotionNetworkHelper.WaitForTargetState(ms_Turn180OnEnterId);
	m_MoveGunStrafingMotionNetworkHelper.SendRequest(ms_Turn180Id);

	m_fStrafeDirection = m_fDesiredStrafeDirection;
	m_bMoveBlendOutTurn180 = false;

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionAiming::FSM_Return CTaskMotionAiming::Turn180_OnUpdate()
{
	CPed* pPed = GetPed();

	// Compute Strafing Params
	// Speed
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	m_fStrafeSpeed = vCurrentMBR.Mag() / MOVEBLENDRATIO_SPRINT;
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);

	// Desired Direction
	float fDesiredStrafeDirectionSignal = ConvertRadianToSignal(m_fDesiredStrafeDirection);
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredStrafeDirectionId, fDesiredStrafeDirectionSignal);

	// Direction
	GetCurrentStrafeDirection(m_fStrafeDirection);
	float fStrafeDirectionSignal = ConvertRadianToSignal(m_fStrafeDirection);	
	m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);

	if (m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, fStrafeDirectionSignal);
	}

	if(!m_MoveGunStrafingMotionNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if(pPed->GetIsSwimming() && !bFPSMode)
	{
		SetState(State_SwimIdle);
		return FSM_Continue;
	}

	if(bFPSMode && !GetWantsToMove(pPed))
	{
		SetState(State_StandingIdle);
		return FSM_Continue;
	}

	if(m_bMoveBlendOutTurn180)
	{
		SetState(State_Strafing);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::Turn180_OnProcessMoveSignals()
{
	m_bMoveBlendOutTurn180 |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_BlendOutTurn180Id);
}

void CTaskMotionAiming::SetState(s32 iState)
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s:%p] - %s %s changed state from %s to %s\n", GetTaskName(), this, AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStateName(GetState()), GetStateName(iState));
	aiTask::SetState(iState);
}

bool CTaskMotionAiming::StartMotionNetwork()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	const bool bIsInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true);
#endif // FPS_MODE_SUPPORTED

	CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();

	if(pParentNetworkHelper)
	{
		if(!m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
		{
			fwMvNetworkDefId networkType = CClipNetworkMoveInfo::ms_NetworkTaskOnFootAiming;
			if(!m_MoveGunStrafingMotionNetworkHelper.IsNetworkDefStreamedIn(networkType))
			{
				return false;
			}

			if(taskVerifyf(m_MoveGunStrafingMotionNetworkHelper.RequestNetworkDef(networkType) && m_MoveGunStrafingMotionNetworkHelper.IsNetworkDefStreamedIn(networkType), "Network def wasn't streamed in"))
			{
				m_MoveGunStrafingMotionNetworkHelper.CreateNetworkPlayer(pPed, networkType);
			}
		}

		if(!m_MoveGunStrafingMotionNetworkHelper.IsNetworkAttached())
		{
			const fwMvClipSetId useStrafeClipSet = GetClipSet();
			if(!fwClipSetManager::IsStreamedIn_DEPRECATED(useStrafeClipSet))
			{
				return false;
			}

			static fwMvFlagId s_BlockVelNormalise("BlockVelNormalise", 0xd8fc3267);
			if(CTaskHumanLocomotion::CheckClipSetForMoveFlag(useStrafeClipSet, s_BlockVelNormalise))
			{
				m_bDoVelocityNormalising = false;
			}
			else
			{
				m_bDoVelocityNormalising = true;

				const fwClipSet* pClipSet;
#if FPS_MODE_SUPPORTED
				if(bIsInFPSMode && GetOverrideStrafingClipSet() == CLIP_SET_ID_INVALID)
				{
					pClipSet = fwClipSetManager::GetClipSet(ms_AimingStrafingFirstPersonClipSetId);
				}
				else
#endif // FPS_MODE_SUPPORTED
				{
					pClipSet = fwClipSetManager::GetClipSet(useStrafeClipSet);
				}

				if(pClipSet)
				{
					static fwMvClipId s_WalkClip("walk_fwd_0_loop",0x90fa019a);
					crClip* pWalkClip = pClipSet->GetClip(s_WalkClip);
					if(pWalkClip)
					{
						Vector3 v = fwAnimHelpers::GetMoverTrackTranslationDiff(*pWalkClip, 0.f, 1.f);
						m_fWalkVel = v.Mag() / pWalkClip->GetDuration();
					}
					else
					{
						TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_DEFAULT_WALK_VEL, 1.43f, 0.f, 10.f, 0.01f);
						m_fWalkVel = STRAFE_DEFAULT_WALK_VEL;
					}
					
					static fwMvClipId s_RunClip("run_fwd_0_loop",0x5794e6ce);
					crClip* pRunClip = pClipSet->GetClip(s_RunClip);
					if(pRunClip)
					{
						Vector3 v = fwAnimHelpers::GetMoverTrackTranslationDiff(*pRunClip, 0.f, 1.f);
						m_fRunVel = v.Mag() / pRunClip->GetDuration();
					}
					else
					{
						TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_DEFAULT_RUN_VEL, 3.21f, 0.f, 10.f, 0.01f);
						m_fRunVel = STRAFE_DEFAULT_RUN_VEL;
					}
				}

#if FPS_MODE_SUPPORTED
				if(GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
				{
					fwMvClipSetId onFootClipSetId = static_cast<const CTaskMotionPed*>(GetParent())->GetDefaultOnFootClipSet();
					const fwClipSet* pOnFootClipSet = fwClipSetManager::GetClipSet(onFootClipSetId);
					if(pOnFootClipSet)
					{
						static fwMvClipId s_OnFootWalkClip("walk",0x83504c9c);
						crClip* pWalkClip = pOnFootClipSet->GetClip(s_OnFootWalkClip);
						if(pWalkClip)
						{
							Vector3 v = fwAnimHelpers::GetMoverTrackTranslationDiff(*pWalkClip, 0.f, 1.f);
							m_fOnFootWalkVel = v.Mag() / pWalkClip->GetDuration();
						}
						else
						{
							TUNE_GROUP_FLOAT(PED_MOVEMENT, ONFOOT_DEFAULT_WALK_VEL, 1.43f, 0.f, 10.f, 0.01f);
							m_fWalkVel = ONFOOT_DEFAULT_WALK_VEL;
						}

						static fwMvClipId s_OnFootRunClip("run",0x1109b569);
						crClip* pRunClip = pOnFootClipSet->GetClip(s_OnFootRunClip);
						if(pRunClip)
						{
							Vector3 v = fwAnimHelpers::GetMoverTrackTranslationDiff(*pRunClip, 0.f, 1.f);
							m_fOnFootRunVel = v.Mag() / pRunClip->GetDuration();
						}
						else
						{
							TUNE_GROUP_FLOAT(PED_MOVEMENT, ONFOOT_DEFAULT_RUN_VEL, 3.21f, 0.f, 10.f, 0.01f);
							m_fOnFootRunVel = ONFOOT_DEFAULT_RUN_VEL;
						}
					}
				}
#endif // FPS_MODE_SUPPORTED
  			}

			ASSERT_ONLY(VerifyMovementClipSet());
			ASSERT_ONLY(m_bCanPedMoveForVerifyClips = GetCanMove(pPed));

			m_MoveGunStrafingMotionNetworkHelper.SetClipSet(useStrafeClipSet);
			m_MoveGunStrafingMotionNetworkHelper.DeferredInsert();
			m_MoveGunStrafingMotionNetworkHelper.SetFlag(m_bLastFootLeft, ms_LastFootLeftId);
		}

		// Send signals
		bool bComputeStrafeSignals = false;
#if FPS_MODE_SUPPORTED
		if(bIsInFPSMode)
		{
			bComputeStrafeSignals = true;
		}
#endif

		if(bComputeStrafeSignals)	
		{
			ComputeAndSendStrafeAimSignals(0.0f);
		}
		else
		{
			SendStrafeAimSignals();
		}

	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::StartAimingNetwork()
{
	CPed* pPed = GetPed();
	if(!m_MoveGunOnFootNetworkHelper.IsNetworkAttached())
	{
		CMoveNetworkHelper* pParentNetworkHelper = m_MoveGunStrafingMotionNetworkHelper.GetDeferredInsertParent();

		if(pParentNetworkHelper)
		{
			if(!m_MoveGunOnFootNetworkHelper.IsNetworkActive())
			{
				if(!SetUpAimingNetwork())
				{
					return false;
				}
			}

#if FPS_MODE_SUPPORTED
			bool bAllowFPSAimingStart = true;
			bool bIsFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
			if(bIsFPSMode)
			{
				const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
				if(pPed->GetMotionData()->GetCombatRoll())
				{
					bAllowFPSAimingStart = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition) || (pWeaponInfo && !pWeaponInfo->GetDisableFPSScope() && pPed->GetMotionData()->GetIsFPSScope());
				}
				else if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro))
				{
					bAllowFPSAimingStart = false;
				}					
			}
#endif
			if(!m_MoveGunOnFootNetworkHelper.IsNetworkAttached() FPS_MODE_SUPPORTED_ONLY( && bAllowFPSAimingStart))
			{
				const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
				if(!pWeaponInfo)
				{
					return false;
				}

				FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault;
#if FPS_MODE_SUPPORTED
				const bool bWeaponHasFirstPersonScope = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
				bool bInScopeState = (pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->GetInFPSScopeState()) || bWeaponHasFirstPersonScope;
				if(bIsFPSMode && pPed->GetMotionData()->GetCombatRoll() && 
				   (!pWeaponInfo->GetDisableFPSScope() && bInScopeState && pPed->GetMotionData()->GetIsFPSLT()))
				{
					fpsStreamingFlags = FPS_StreamRNG;
				}
#endif
				fwMvClipSetId aimClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed, fpsStreamingFlags);

#if __ASSERT
#if FPS_MODE_SUPPORTED
				const CTaskGun* pTask = (CTaskGun*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
				const bool bCheckStrafe = (pTask && pTask->ShouldDelayAfterSwap()) ? false : true;
#endif // FPS_MODE_SUPPORTED
				taskAssertf(pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() || pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring() FPS_MODE_SUPPORTED_ONLY(|| pPed->UseFirstPersonUpperBodyAnims(bCheckStrafe) || (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->GetIsSwimming())), "%s ped %s has weapon info %s and cannot be fired like a gun", pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetDebugName(), pWeaponInfo->GetName());
#endif // __ASSERT

				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(aimClipSetId);
				if(pClipSet)
				{
					pClipSet->Request_DEPRECATED();
				}

				if(!fwClipSetManager::IsStreamedIn_DEPRECATED(aimClipSetId))
				{
					return false;
				}

				SetAimClipSet(aimClipSetId);
				pParentNetworkHelper->SetSubNetwork(m_MoveGunOnFootNetworkHelper.GetNetworkPlayer(), GetIsCrouched() ? CTaskMotionPed::ms_CrouchedAimingNetworkId : CTaskMotionPed::ms_AimingNetworkId);

				if(GetMotionData().GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Aiming)
				{
					ComputeAndSendStrafeAimSignals();
				}

				bool bUseSettle = true;
				bool bForceFPSIntro = false;
#if FPS_MODE_SUPPORTED
				// FPS mode: Force us to do an intro if we're restarting the FPS aim network and we're not in cover
				// make an exception for aiming directly in cover once we're in that state for a bit as we stay aiming in this case
				TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, FORCE_ALLOW_AIMING_INTRO_TRANS_IN_COVER, true);
				bool bShouldUseAimingIntroTransitions = true;

				CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));

				if (bIsFPSMode)
				{
					if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceSkipFPSAimIntro))
					{
						bShouldUseAimingIntroTransitions = false;
					}
					else if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER)) // check cover task is running instead of the reset flag. B* 2091371.
					{
						if (!FORCE_ALLOW_AIMING_INTRO_TRANS_IN_COVER)
						{
							bShouldUseAimingIntroTransitions = false;
						}
						else if (!CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPed))
						{
							bShouldUseAimingIntroTransitions = false;
						}
						else
						{
							// This is a bit hacky, we need to disable the aim intros when initially transitioning to aiming directly,
							// but enable them when we're safely in the state
							TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_TIME_IN_STATE_TO_IGNORE_AIM_INTROS, 0.1f, 0.0f, 1.0f, 0.01f);
							const CTaskGun* pGunTask = static_cast<const CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
							if (pGunTask && pGunTask->GetTimeRunning() < MAX_TIME_IN_STATE_TO_IGNORE_AIM_INTROS && !pPed->GetPlayerInfo()->IsFiring())
							{
								bShouldUseAimingIntroTransitions = false;
							}
						}
					}
					// If we've explicitly asked for idle transitions to be skipped in melee then ensure we don't use them
					else if (pPed->GetPedIntelligence()->IsPedInMelee() && pGunTask && pGunTask->GetGunFlags().IsFlagSet(GF_InstantBlendToAim))
					{
						bShouldUseAimingIntroTransitions = false;
					}
				}

				CTaskAimGunOnFoot* pAimGunOnFootTask = static_cast<CTaskAimGunOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
				const bool bWeaponBlocked = pAimGunOnFootTask && pAimGunOnFootTask->GetIsWeaponBlocked();
				bShouldUseAimingIntroTransitions  = bShouldUseAimingIntroTransitions && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading) && !bWeaponBlocked;

				if (pPed->UseFirstPersonUpperBodyAnims(false) && bShouldUseAimingIntroTransitions)
				{
					bForceFPSIntro = true;
					if(m_FPSAimingIntroClipSetId != CLIP_SET_ID_INVALID)
					{
						m_MoveGunOnFootNetworkHelper.SetClipSet(m_FPSAimingIntroClipSetId, ms_FPSAimingIntroClipSetId);

						// Play a weapon animation if it exists
						const CWeaponSwapData* pWeaponSwapData = pWeaponInfo->GetSwapWeaponData();
						if (pWeaponSwapData)
						{
							s32 iFlags = CWeaponSwapData::SF_UnHolster;
							fwMvClipId weaponClipId = CLIP_ID_INVALID;
							pWeaponSwapData->GetSwapClipIdForWeapon(iFlags, weaponClipId);
							if (weaponClipId != CLIP_ID_INVALID && fwAnimManager::GetClipIfExistsBySetId(m_FPSAimingIntroClipSetId, weaponClipId))
							{
								CWeapon* pNewEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
								if (pNewEquippedWeapon)
								{
									pNewEquippedWeapon->StartAnim(m_FPSAimingIntroClipSetId, weaponClipId, INSTANT_BLEND_DURATION, 1.0f, 0.0f, false, false, true);
								}
							}
						}

					}

					//Clear idle transition flag if we are forcing transitions in FPS mode
					if(pGunTask && pGunTask->GetGunFlags().IsFlagSet(GF_SkipIdleTransitions))
					{
						pGunTask->GetGunFlags().ClearFlag(GF_SkipIdleTransitions);
					}
				}
#endif	// FPS_MODE_SUPPORTED

				static dev_float BLEND_DURATION = 0.25f;
				if(pGunTask) 
				{
					if((pGunTask->GetGunFlags().IsFlagSet(GF_InstantBlendToAim) || 
						pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim) ||
						pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimFromScript) ||
						pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimNoSettle) ||
						pPed->GetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning) ||
						pGunTask->GetGunFlags().IsFlagSet(GF_SkipIdleTransitions)) && !bForceFPSIntro)
					{
						CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
						if(pWeaponInfo->GetOnlyAllowFiring())
						{
							m_MoveGunOnFootNetworkHelper.ForceStateChange(CTaskAimGunOnFoot::ms_FireOnlyStateId, CTaskAimGunOnFoot::ms_AimStateId);
						}
						else if(pGunTask->GetGunFlags().IsFlagSet(GF_FireAtLeastOnce))
						{
							taskAssertf(!pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo), "Weapon is out of ammo, but we are forcing firing state");
							m_MoveGunOnFootNetworkHelper.ForceStateChange(CTaskAimGunOnFoot::ms_FiringStateId, CTaskAimGunOnFoot::ms_AimStateId);	
							// Make sure we are sending the right signals
							if(pAimGunTask)
							{
								// Inform the gun task we are forcing the firing state
								pAimGunTask->SetupToFire(true);
								// Ensure the gun task runs next frame
								pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
							}
						}
						else
						{
							m_MoveGunOnFootNetworkHelper.ForceStateChange(CTaskAimGunOnFoot::ms_AimingStateId, CTaskAimGunOnFoot::ms_AimStateId);
						}

						if(pAimGunTask)
						{
							// Ensure the task is in sync
							pAimGunTask->SetForceRestart(true);
						}

						m_bAimIntroFinished = true;

						// Ensure we haven't sent the UseAimingIntro request also
						m_bSuppressHasIntroFlag = true;
						m_MoveGunOnFootNetworkHelper.SetFlag(false, ms_HasIntroId);

						// Reset flag
						pGunTask->GetGunFlags().ClearFlag(GF_SkipIdleTransitions);

						if (CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPed))
						{
							pGunTask->SetBlockFiringTime(fwTimer::GetTimeInMilliseconds() + (u32)(BLEND_DURATION * 1000.0f));
						}
					}
					else if(!m_bAimIntroFinished || bForceFPSIntro)
					{
#if FPS_MODE_SUPPORTED
						if (pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets))
						{
							m_MoveGunOnFootNetworkHelper.SendRequest(ms_UseAimingFidgetFPSId);
						}
						else if (bForceFPSIntro)
						{
							// Only do the intro if we have a valid clipset (invalid when equipping a new weapon).
							if (m_FPSAimingIntroClipSetId != CLIP_SET_ID_INVALID)
							{
								m_MoveGunOnFootNetworkHelper.SendRequest(ms_UseAimingIntroFPSId);
							}
							else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_BlendingOutFPSIdleFidgets) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
							{
								// Disable settle when blending out of a fidget and staying in the same FPS state
								bUseSettle = false;
							}
						}
#endif	//FPS_MODE_SUPPORTED
						if (!bForceFPSIntro FPS_MODE_SUPPORTED_ONLY(&& !bIsFPSMode))
						{
							m_MoveGunOnFootNetworkHelper.SendRequest(ms_UseAimingIntroId);
						}
						else
						{
							// Flag aim intro as finished if not sending the request
							m_bAimIntroFinished = true;
						}

						// Ensure the HasIntro flag is set if supposed to be
						if(m_AimClipSetId != CLIP_SET_ID_INVALID)
						{
							fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_AimClipSetId);
							if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", m_AimClipSetId.TryGetCStr()))
							{
								const atArray<atHashString>& clipSetFlags = pClipSet->GetMoveNetworkFlags();
								for(s32 i = 0; i < clipSetFlags.GetCount(); i++)
								{
									if(clipSetFlags[i].GetHash() == ms_HasIntroId.GetHash())
									{
										m_MoveGunOnFootNetworkHelper.SetFlag(true, ms_HasIntroId);
										break;
									}
								}
							}
						}

						u32 uBlockFiringTime = fwTimer::GetTimeInMilliseconds() + (u32)(BLEND_DURATION * 1000.0f);
						
						// B*2270721: Skip the block firing timer if we're restarting in FPS mode and we're already firing.
						if (bIsFPSMode && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsFiring) && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON))
						{
							uBlockFiringTime = 0;
						}

						pGunTask->SetBlockFiringTime(uBlockFiringTime);
					}
				}

				if (bUseSettle && !pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimNoSettle))
				{
					m_MoveGunOnFootNetworkHelper.SendRequest(CTaskAimGunOnFoot::ms_UseSettleId); 
				}

				//Choose a new aiming clip.
				ChooseNewAimingClip();
				
				//Set the aiming clip.
				SetAimingClip();

				return true;
			}	
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::SetUpAimingNetwork()
{
	fwMvNetworkDefId networkType = CClipNetworkMoveInfo::ms_NetworkTaskAimGunOnFoot;

	if(!m_MoveGunOnFootNetworkHelper.IsNetworkDefStreamedIn(networkType) )
	{
		return false;
	}

	if(taskVerifyf(m_MoveGunOnFootNetworkHelper.RequestNetworkDef(networkType) && m_MoveGunOnFootNetworkHelper.IsNetworkDefStreamedIn(networkType), "Network def wasn't streamed in"))
	{
		m_MoveGunOnFootNetworkHelper.CreateNetworkPlayer(GetPed(), networkType);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ComputeAndSendStrafeAimSignals(float fUseTimeStep)
{
	UpdateStrafeAimSignals(fUseTimeStep);
	SendStrafeAimSignals();
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::UpdateStrafeAimSignals(float fUseTimeStep)
{
	ComputePitchSignal(fUseTimeStep);
	ComputeAimIntroSignals(fUseTimeStep);
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SendParentSignals()
{
	CPed* pPed = GetPed();

	//Assert that the parent move network helper is valid.
	CMoveNetworkHelper* pParentMoveNetworkHelper = GetParent()->GetMoveNetworkHelper();
	if(!taskVerifyf(pParentMoveNetworkHelper, "The parent move network helper is invalid."))
	{
		return;
	}
	
	//Set the aiming blend factor.
	float fAimingBlendFactor;
	float fBlendDuration;
	bool bShouldTurnOffUpperBody = ShouldTurnOffUpperBodyAnimations(fBlendDuration);

	if(bShouldTurnOffUpperBody)
	{
		fAimingBlendFactor = 0.f;
	}
	else
	{
		fAimingBlendFactor = 1.f;
	}

	// Restart task
	if(fAimingBlendFactor != m_fPreviousAimingBlendFactor)
	{
		bool bFPSRestartForUnarmedTransition = false;
#if FPS_MODE_SUPPORTED
		bool bFPSMode = pPed->UseFirstPersonUpperBodyAnims(false);
		if(bFPSMode)
		{
			const CWeaponInfo *pWeaponInfo = pPed->GetEquippedWeaponInfo();
			//This is the case when aiming is turned on when swapping from unarmed to gun
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition) && !bShouldTurnOffUpperBody)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSAimingOnToUnholsterTransitionBlendTime, 0.15f, 0.0f, 2.0f, 0.01f);
				fBlendDuration = fFPSAimingOnToUnholsterTransitionBlendTime;

				m_fFPSVariableRateIntroBlendInDuration = fBlendDuration;
				m_fFPSVariableRateIntroBlendInTimer = 0.0f;
			}
			if(pWeaponInfo && pWeaponInfo->GetIsUnarmed() && (pPed->GetMotionData()->GetIsFPSLT() || pPed->GetMotionData()->GetIsUsingStealthInFPS()) && !bShouldTurnOffUpperBody)
			{
				fwMvClipSetId clipsetId = pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed);
				if(clipsetId != CLIP_SET_ID_INVALID)
				{
					m_FPSAimingIntroClipSetId = clipsetId;
					bFPSRestartForUnarmedTransition = true;
				}
			}
			else if(pWeaponInfo && pPed->GetMotionData() && pPed->GetMotionData()->GetCombatRoll() && !bShouldTurnOffUpperBody)
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseFPSUnholsterTransitionDuringCombatRoll, true);

				const bool bWeaponHasFirstPersonScope = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
				bool bInScopeState = (pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->GetInFPSScopeState()) || bWeaponHasFirstPersonScope;
				bool bFPSScopeDuringRoll = !pWeaponInfo->GetDisableFPSScope() && pPed->GetMotionData()->GetIsFPSLT() && bInScopeState;

				// Use RNG transitions when in iron sights. Scope transitions cause gun clipping through camera.
				FPSStreamingFlags fpsStreamingFlag = (bFPSScopeDuringRoll) ? FPS_StreamRNG : FPS_StreamDefault;
				
				fwMvClipSetId clipsetId = pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed, CPedMotionData::FPS_UNHOLSTER, fpsStreamingFlag);
				m_fFPSAimingIntroBlendOutTime = CTaskMotionPed::SelectFPSBlendTime(pPed, false, CPedMotionData::FPS_UNHOLSTER);

				if(clipsetId != CLIP_SET_ID_INVALID)
				{
					m_FPSAimingIntroClipSetId = clipsetId;
					m_AimClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed, fpsStreamingFlag);
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, true);
				}

				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSAimingOnAndOffDuringRollBlendTime, 0.001f, 0.0f, 2.0f, 0.01f);
				fBlendDuration = fFPSAimingOnAndOffDuringRollBlendTime;
			}
		}
#endif

		pParentMoveNetworkHelper->SetFloat(ms_AimingBlendFactorId, fAimingBlendFactor);
		pParentMoveNetworkHelper->SetFloat(ms_AimingBlendDurationId, fBlendDuration);

		// No need to restart task if already in Start state
		CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
		if(pAimGunTask && (pAimGunTask->GetState() != CTaskAimGunOnFoot::State_Start || bFPSRestartForUnarmedTransition))
		{
			fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
			if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
			{
				clipSetId = m_MoveGunOnFootNetworkHelper.GetClipSetId();
			}

			if(clipSetId == CLIP_SET_ID_INVALID)
			{
				const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
				if(pWeaponInfo)
				{
					clipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
					if(clipSetId == CLIP_SET_ID_INVALID || !fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId))
					{
						// Default to base weapon clip set, as this should be streamed
						clipSetId = pWeaponInfo->GetWeaponClipSetId(*pPed);
					}
				}
			}

			if(clipSetId != CLIP_SET_ID_INVALID && (m_fPreviousAimingBlendFactor != FLT_MAX || clipSetId != m_AimClipSetId || bFPSRestartForUnarmedTransition))
			{
				RestartUpperBodyAimNetwork(clipSetId);
			}
		}

		m_fPreviousAimingBlendFactor = fAimingBlendFactor;
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetMaxAndMinPitchBetween0And1(CPed* pPed, float& fMinPitch, float& fMaxPitch)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if (pWeapon && pWeapon->GetWeaponInfo())
	{
		const CAimingInfo* pAimingInfo = pWeapon->GetWeaponInfo()->GetAimingInfo();
		if (pAimingInfo)
		{
			// Just update pitch parameter, yaw is handled by torso ik

			// Get the sweep ranges from the weapon info
			const float fSweepPitchMin = pAimingInfo->GetSweepPitchMin() * DtoR;
			const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;
			// Get the pitch real limits
			static dev_float MIN_PITCH_OFFSET = 0.34755385f; // Seems like the min torso pitch is not completely accurate
			const float fTorsoMinPitch = pPed->GetIkManager().GetTorsoMinPitch() + MIN_PITCH_OFFSET; 
			const float fTorsoMaxPitch = pPed->GetIkManager().GetTorsoMaxPitch();

			// Map the angle of the minimum pitch to the range 0.0->1.0
			if (fTorsoMinPitch < 0.0f)
				fMinPitch = ((fSweepPitchMin - fTorsoMinPitch) / fSweepPitchMin) * 0.5f;
			else
				fMinPitch = (fTorsoMinPitch / fSweepPitchMax) * 0.5f + 0.5f;

			static dev_float EXTRA_PITCH = 0.0225f;
			if(fMinPitch < 0.5f)
				fMinPitch += fMinPitch * 2.f * EXTRA_PITCH;
			else
				fMinPitch += (1.f - (fMinPitch - 0.5f) * 2.f) * EXTRA_PITCH;

			// Map the angle of the maximum pitch to the range 0.0->1.0
			if (fTorsoMaxPitch < 0.0f)
				fMaxPitch = ((fSweepPitchMin - fTorsoMaxPitch) / fSweepPitchMin) * 0.5f;
			else
				fMaxPitch = (fTorsoMaxPitch / fSweepPitchMax) * 0.5f + 0.5f;

			if(fMaxPitch < 0.5f)
				fMaxPitch += fMaxPitch * 2.f * EXTRA_PITCH;
			else
				fMaxPitch += (1.f - (fMaxPitch - 0.5f) * 2.f) * EXTRA_PITCH;
			
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SendStrafeAimSignals()
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		CPed *pPed = GetPed();

		// Send the pitch 
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitch);

		// Calculate and send the pitch for the fidgets
		float fMinPitch, fMaxPitch;
		if( GetMaxAndMinPitchBetween0And1(pPed, fMinPitch, fMaxPitch) )
		{
			const float fMaxNormPitch = fMaxPitch - fMinPitch;
			const float fCurrentNormPitch = m_fCurrentPitch - fMinPitch;

			m_MoveGunOnFootNetworkHelper.SetFloat(ms_NormalizedPitchId, fCurrentNormPitch / fMaxNormPitch);
		}
		else
		{
			m_MoveGunOnFootNetworkHelper.SetFloat(ms_NormalizedPitchId, m_fCurrentPitch);
		}

		bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
		
		if (pPed->UseFirstPersonUpperBodyAnims(false))
		{
			float fVariableRateIntroBlendInWeight = 1.0f;
			bool bUseVariableIntroPlayRate = false;

			bool bHasPairedWeaponUnholsterAnimation = false;
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
			if (pWeaponInfo && pWeaponInfo->GetPairedWeaponHolsterAnimation())
			{
				bHasPairedWeaponUnholsterAnimation = true;
			}

			if(m_fFPSVariableRateIntroBlendInTimer >= 0.0f && m_fFPSVariableRateIntroBlendInTimer <= m_fFPSVariableRateIntroBlendInDuration && !bHasPairedWeaponUnholsterAnimation)
			{
				m_fFPSVariableRateIntroBlendInTimer += fwTimer::GetTimeStep();
				fVariableRateIntroBlendInWeight = m_fFPSVariableRateIntroBlendInTimer/m_fFPSVariableRateIntroBlendInDuration;
				fVariableRateIntroBlendInWeight *= fVariableRateIntroBlendInWeight;
				bUseVariableIntroPlayRate = true;
			}

			//Freeze the unholster transition till the getup anims have fully blended out
			if((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult))
				 && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition))
			{
				fVariableRateIntroBlendInWeight = 0.0f;
			}

			if(!bUseVariableIntroPlayRate)
			{
				m_fFPSVariableRateIntroBlendInTimer = -1.0f;
				m_fFPSVariableRateIntroBlendInDuration = -1.0f;
			}

			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSIntroPlaySpeed, 2.0f, 0.0f, 5.0f, 0.1f);
			float fFPSIntroPlaySpeedForWeapon = bHasPairedWeaponUnholsterAnimation ? 1.0f : fFPSIntroPlaySpeed;
			float fIntroPlayRate = fVariableRateIntroBlendInWeight * fFPSIntroPlaySpeedForWeapon; 

			m_MoveGunOnFootNetworkHelper.SetFloat(CTaskAimGunOnFoot::ms_FPSIntroPlayRate, fIntroPlayRate);
			m_MoveGunOnFootNetworkHelper.SetFloat(CTaskAimGunOnFoot::ms_FPSIntroBlendOutTime, m_fFPSAimingIntroBlendOutTime);

			bFPSMode = true;
		}
#endif	//FPS_MODE_SUPPORTED

		if(!m_bAimIntroFinished && !bFPSMode)
		{
			// Send the aim intro signals
			if(m_fAimIntroDirection != FLT_MAX)
			{
				// Only send the updated direction if not doing the standing idle intro
				m_MoveGunOnFootNetworkHelper.SetFloat(ms_AimIntroDirectionId, ConvertRadianToSignal(-m_fAimIntroDirection));
			}

			m_MoveGunOnFootNetworkHelper.SetFloat(ms_AimIntroRateId, m_fAimIntroRate);
			if(m_fAimIntroRate == 0.f)
			{
				m_MoveGunOnFootNetworkHelper.SetFloat(ms_AimIntroPhaseId, m_fAimIntroPhase);
			}

			static dev_float FINISHED_PHASE = 0.99f;
			if((m_fAimIntroPhase >= FINISHED_PHASE || m_bAimIntroTimedOut) && m_bEnteredAimIntroState)
			{
				m_MoveGunOnFootNetworkHelper.SendRequest(ms_AimingIntroFinishedId);
				m_MoveGunOnFootNetworkHelper.SendRequest(CTaskAimGunOnFoot::ms_UseSettleId);
				m_bAimIntroFinished = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
void CTaskMotionAiming::SendFPSSwimmingMoveSignals()
{
	// For FPS swimming: set pitch so we can blend high/med/low strafes
	if (m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		CPed *pPed = GetPed();

		bool bDiving = false;

		CTaskMotionBase* pPrimaryTask = pPed->GetPrimaryMotionTask();
		if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
		{
			CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
			if (pTask)
			{
				bDiving = pTask->CheckForDiving();	
			}
		}

		if (bDiving)
		{
			m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitch);
		}
		else
		{
			// Set pitch to be level if not diving
			m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_PitchId, 0.5f);
		}

		// B*2106117: Set weapon holding clipset and filter
		bool bSetWeaponClipAndFilter = false;
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if (pWeaponInfo && pWeaponInfo->GetUsableUnderwater())
		{
			fwMvClipSetId weaponClipSetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);

			static fwMvFilterId s_filter("BothArms_filter", 0x16F1D420);
			fwMvFilterId filterId = s_filter;
			crFrameFilterMultiWeight* pWeaponFilter = CGtaAnimManager::FindFrameFilter(filterId.GetHash(), pPed);

			if (weaponClipSetId != CLIP_SET_ID_INVALID && pWeaponFilter)
			{
				// Clipsets are different, we need to restart the weapon holding state
				if (weaponClipSetId != m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId))
				{
					m_MoveGunStrafingMotionNetworkHelper.SendRequest(CTaskHumanLocomotion::ms_RestartWeaponHoldingId);
				}

				m_MoveGunStrafingMotionNetworkHelper.SetFlag(true, CTaskHumanLocomotion::ms_UseWeaponHoldingId);
				m_MoveGunStrafingMotionNetworkHelper.SetClipSet(weaponClipSetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);
				m_MoveGunStrafingMotionNetworkHelper.SetFilter(pWeaponFilter, CTaskHumanLocomotion::ms_WeaponHoldingFilterId);
				bSetWeaponClipAndFilter = true;
			}
		}

		if (!bSetWeaponClipAndFilter)
		{
			m_MoveGunStrafingMotionNetworkHelper.SetFlag(false, CTaskHumanLocomotion::ms_UseWeaponHoldingId);
		}
	}
}
#endif	//FPS_MODE_SUPPORTED

//////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionAiming::GetClipSet() const
{
	return GetDefaultAimingStrafingClipSet(GetIsCrouched());
}

//////////////////////////////////////////////////////////////////////////

const CAimingInfo* CTaskMotionAiming::GetAimingInfo() const
{
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(GetPed());
	if(pWeaponInfo)
	{
		return pWeaponInfo->GetAimingInfo();
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsStateValidForPostCamUpdate() const
{
	switch (GetState())
	{
	case State_StandingIdleIntro:
	case State_StandingIdle:
	case State_SwimIdleIntro:
	case State_SwimIdle:
	case State_SwimIdleOutro:
	case State_SwimStrafe:
	case State_StrafingIntro:
	case State_StartStrafing:
	case State_Strafing:
	case State_StopStrafing:
	case State_StreamTransition:
	case State_PlayTransition:
	case State_StreamStationaryPose:
	case State_StationaryPose:
	case State_Turn:
	case State_CoverStep:
	case State_Roll:
	case State_Turn180:
		return true;
	case State_Initial:
	case State_StartMotionNetwork:
	case State_Finish:
		break;
	default:
		taskAssert(0);
		break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsStateValidForIK() const
{
	const CPed& ped = *GetPed();

	if(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN))
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	bool bFPSMode = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
	//If we are performing a swap in FPS mode and not switching to a melee weapon
	bool bFPSNonMelee = bFPSMode && ped.GetEquippedWeaponInfo() && (!ped.GetEquippedWeaponInfo()->GetIsMelee() || ped.GetEquippedWeaponInfo()->GetIsUnarmed());

	if (ped.GetIsFPSSwimming() && ped.GetEquippedWeaponInfo() && ped.GetEquippedWeaponInfo()->GetIsMelee())
	{
		return false;
	}

	if(bFPSMode && ped.GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		return false;
	}

	if(bFPSMode && ped.GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile))
	{
		return false;
	}
#endif

	if(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SWAP_WEAPON) FPS_MODE_SUPPORTED_ONLY( && !bFPSNonMelee))
	{
		return false;
	}

	if (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_HELMET))
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if (ped.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE))
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	// Cover handles grip IK itself.
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint))
	{
		return false;
	}

	CTaskCombat* pCombatTask = static_cast<CTaskCombat*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask && pCombatTask->GetIsPlayingAmbientClip())
	{
		return false;
	}

	CTaskTakeOffPedVariation* pTakeOffVariation = static_cast<CTaskTakeOffPedVariation*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_PED_VARIATION));
	if(pTakeOffVariation)
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(&ped);
	if(!pWeaponInfo)
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if(ped.UseFirstPersonUpperBodyAnims(false))
	{
		if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup) || ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult))
		{
			return false;
		}

		if(pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsMeleeFist())
		{
			if(!IsValidForUnarmedIK(ped))
			{
				return false;
			}
		}

		if ((ped.GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || ped.GetPedResetFlag(CPED_RESET_FLAG_IsLanding)))
		{
			CTaskJump* pTaskJump = (CTaskJump*)( ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP) );
			if(pTaskJump && (pTaskJump->GetIsStateFall() || pTaskJump->GetIsDivingLaunch() || pTaskJump->GetIsSkydivingLaunch()))
			{
				return false;
			}

			if(pWeaponInfo->GetIsGun1Handed() || pWeaponInfo->GetIsUnarmed())
			{
				TUNE_GROUP_BOOL(BUG_2048930, FPS_USE_LEFT_ARM_IK_FOR_ONE_HANDED_OR_UNARMED_WEAPONS_WHEN_JUMPING, false);
				return FPS_USE_LEFT_ARM_IK_FOR_ONE_HANDED_OR_UNARMED_WEAPONS_WHEN_JUMPING;
			}
		}

		// B*2326817: Keep IK on if we're running the weapon blocked task and IK was enabled when we started the task.
		bool bNotBlockedOrBlockedAndDoesntNeedIK = !ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED) || (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED) && !static_cast<CTaskWeaponBlocked*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED))->IsUsingFPSLeftHandGripIK());
		
		// B*2169498: Don't try to IK left hand when using 1h weapons (while not in stealth - stealth anims use IK to grip the weapon) (...and not for the digiscanner - url:bugstar:2247585).
		if (ped.GetMotionData()->GetIsFPSIdle() && pWeaponInfo->GetIsGun1Handed() && !ped.IsUsingStealthMode() && (pWeaponInfo->GetHash() != ATSTRINGHASH("WEAPON_DIGISCANNER", 0xfdbadced)) && (pWeaponInfo->GetHash() != ATSTRINGHASH("WEAPON_METALDETECTOR", 0xdba2e809)) && bNotBlockedOrBlockedAndDoesntNeedIK)
		{
			return false;
		}
	}
#endif

	if (ped.IsPlayer() && pWeaponInfo->GetIsGun1Handed() && ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro))
	{
		return false;
	}

	if(ped.GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
	{
		return false;
	}

	switch (GetState())
	{
	case State_StandingIdleIntro:
	case State_StandingIdle:
	case State_SwimIdleIntro:
	case State_SwimIdle:
	case State_SwimIdleOutro:
	case State_SwimStrafe:
	case State_StrafingIntro:
	case State_StartStrafing:
	case State_Strafing:
	case State_StopStrafing:
	case State_Turn180:
	case State_StreamTransition:
	case State_PlayTransition:
	case State_StreamStationaryPose:
	case State_StationaryPose:
	case State_CoverStep:
		return true;
	case State_Initial:
	case State_StartMotionNetwork:
	case State_Turn:
	case State_Finish:
		break;
	case State_Roll:
#if FPS_MODE_SUPPORTED
		if (GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMBAT_ROLL) && ped.IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			const float fCombatRollEntryKeepLeftIkOnTime = 0.075f;
			if(GetState() == State_Roll && GetTimeInState() < fCombatRollEntryKeepLeftIkOnTime)
			{
				return true;
			}
			else
			{
				return !static_cast<const CTaskCombatRoll*>(GetSubTask())->ShouldTurnOffUpperBodyAnimations();
			}
		}
#endif // FPS_MODE_SUPPORTED
		break;
	default:
		taskAssert(0);
		break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

u32 CTaskMotionAiming::CountFiringEventsInClip(const crClip& rClip)
{
	u32 uCount = 0;
	
	crTagIterator it(*rClip.GetTags());
	while (*it)
	{
		const crTag* pTag = *it;			
		if(pTag->GetKey() == CClipEventTags::MoveEvent)
		{
			static const crProperty::Key MoveEventKey = crProperty::CalcKey("MoveEvent", 0x7EA9DFC4);
			const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute(MoveEventKey);
			if(pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
			{
				static const atHashString s_FireHash("Fire",0xD30A036B);
				if(s_FireHash.GetHash() == static_cast<const crPropertyAttributeHashString*>(pAttrib)->GetHashString().GetHash())
				{
					++uCount;
				}
			}
		}
		++it;
	}
	
	return uCount;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::WillStartInMotion(const CPed* pPed)
{
	if(GetCanMove(pPed))
	{
		static dev_float MIN_VELOCITY_IF_WANTS_TO_MOVE = 1.f;
		static dev_float MIN_VELOCITY = 5.0f;
		// Adjust vel by the ground physical, so its local vel
		Vector3 vVel = pPed->GetVelocity();
		vVel -= VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
		float fSpeedSq = vVel.Mag2();
		return ((GetWantsToMove(pPed) && fSpeedSq > square(MIN_VELOCITY_IF_WANTS_TO_MOVE)) || fSpeedSq > square(MIN_VELOCITY));
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ProcessOnFootAimingLeftHandGripIk(CPed& rPed, const CWeaponInfo& rWeaponInfo, const bool bAllow2HandedAttachToStockGrip, float fOverrideBlendInDuration, float fOverrideBlendOutDuration)
{
	if(!rWeaponInfo.GetBlockLeftHandIKWhileAiming(rPed) || rPed.GetMotionData()->GetUsingStealth())
	{
		bool bAllow1HandedAttachToRightGrip = true;
		if(rWeaponInfo.GetUseLeftHandIkWhenAiming())
		{
			bAllow1HandedAttachToRightGrip = false;
		}
		rPed.ProcessLeftHandGripIk(bAllow1HandedAttachToRightGrip, bAllow2HandedAttachToStockGrip, fOverrideBlendInDuration, fOverrideBlendOutDuration);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ComputePitchSignal(float fUseTimeStep)
{
	CPed* pPed = GetPed();

	//Don't update the pitch whilst switching players as it makes the player aim down.
	bool bSwitchActive = g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT);
	if(pPed->IsLocalPlayer() && !m_bSwitchActiveAtStart && bSwitchActive)
	{
		return;
	}

	if(m_bSwitchActiveAtStart)
		m_bSwitchActiveAtStart = bSwitchActive;

	// Calculate the desired pitch.
	float fOverrideDesiredPitch = 0.0f;
	bool bOverrideDesiredPitch = (m_bIsPlayingCustomAimingClip || (GetState() == State_Turn)); // Some animations need a flat pitch.
	
	// B*1868368: Don't assume Z axis is up when calculating desired pitch if we're attached to an aircraft
	bool bUntransformRelativeToPed = false;
	if (pPed->GetAttachParent() && ((CPhysical*)pPed->GetAttachParentForced())->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetAttachParent());
		if (pVehicle && pVehicle->GetIsAircraft())
		{
			bUntransformRelativeToPed = true;
		}
	}

	float fDesiredPitch = CUpperBodyAimPitchHelper::ComputeDesiredPitchSignal(*pPed, bOverrideDesiredPitch ? &fOverrideDesiredPitch : NULL, false, bUntransformRelativeToPed);
	if(fDesiredPitch >= 0.0f)
	{
		m_fDesiredPitch = fDesiredPitch;
	}
	else if(m_fDesiredPitch < 0.0f)
	{
		// Point straight ahead if we don't have a proper pitch signal
		m_fDesiredPitch = 0.5f;
	}

	// Smooth the input if we've exited the intro and not forcing param changes, otherwise, force the current heading equal to the desired
	if(m_fCurrentPitch >= 0.0f)
	{
		float fPitchChangeRate = ms_Tunables.m_PitchChangeRate;

		// url:bugstar:5518472 - Speed up pick change rate so that weapons firing laser tracers don't lag behind and appear to be firing in the wrong direction
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
		if (pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_RAYMINIGUN", 0xB62D1F67))
		{
			TUNE_GROUP_FLOAT(AIMING_TUNE, RAYMINIGUN_PITCH_CHANGE_RATE_OVERRIDE, 1.5f, 0.0f, 5.0f, 0.01f);
			fPitchChangeRate = RAYMINIGUN_PITCH_CHANGE_RATE_OVERRIDE;
		}

		const bool bIsAssistedAiming = GetPed()->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
		TUNE_GROUP_FLOAT(AIMING_TUNE, PITCH_CHANGE_RATE_ASSISTED_AIM, 0.5f, 0.0f, 1.0f, 0.01f);

		float fMaxRateChange = bIsAssistedAiming ? PITCH_CHANGE_RATE_ASSISTED_AIM : fPitchChangeRate;

		bool bUsingFpsPitch = false;

		if(pPed->IsLocalPlayer())
		{
#if KEYBOARD_MOUSE_SUPPORT
			const CControl* pControl = pPed->GetControlFromPlayer();
			if(pControl && pControl->UseRelativeLook())
			{
				TUNE_GROUP_FLOAT(AIMING_TUNE, PITCH_CHANGE_RATE_MOUSE_MIN, 25.0f,  1.0f,  100.0f, 0.01f);
				TUNE_GROUP_FLOAT(AIMING_TUNE, PITCH_CHANGE_RATE_MOUSE_MAX, 100.0f, 15.0f, 200.0f, 0.01f);
				taskAssert(PITCH_CHANGE_RATE_MOUSE_MIN < PITCH_CHANGE_RATE_MOUSE_MAX);
				float fMouseSensitivityOverride = 0.0f;
				PARAM_mouse_sensitivity_override.Get(fMouseSensitivityOverride);

			#if __BANK
				// A bit hacky but this is only for debugging, TASK_AIMING_MOUSE_SENSITIVITY_OVERRIDE should match MOUSE_SENSITIVITY_OVERRIDE.
				TUNE_GROUP_FLOAT(MOUSE_TUNE, TASK_AIMING_MOUSE_SENSITIVITY_OVERRIDE, -1.0f, -1.0f, 5000.0f, 1.0f);
				if(TASK_AIMING_MOUSE_SENSITIVITY_OVERRIDE > 0.0f)
				{
					fMouseSensitivityOverride = TASK_AIMING_MOUSE_SENSITIVITY_OVERRIDE;
				}
			#endif // __BANK

				const float fMouseSensitivity = (fMouseSensitivityOverride > 0.0f) ? fMouseSensitivityOverride : (float)CPauseMenu::GetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE);
				const float fScaleSensitivity = pControl->WasSteamControllerLastKnownSource() ? 1.0f : fMouseSensitivity;
				fMaxRateChange = RampValue(fScaleSensitivity, 0.0f, 2000.0f, PITCH_CHANGE_RATE_MOUSE_MIN, PITCH_CHANGE_RATE_MOUSE_MAX);
			}
			else
#endif // KEYBOARD_MOUSE_SUPPORT
#if FPS_MODE_SUPPORTED
			{
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
				{
					if(m_uLastFpsPitchUpdateTime == UINT_MAX)
					{
						m_uLastFpsPitchUpdateTime = fwTimer::GetTimeInMilliseconds();
					}

					TUNE_GROUP_FLOAT(AIMING_TUNE, PITCH_CHANGE_RATE_FPS, 10.0f, 0.0f, 100.0f, 0.01f);

					TUNE_GROUP_INT(PED_MOVEMENT, LERP_PITCH_AFTER_START_TIME_MS, 250, 0, 10000, 1);
					u32 uLerpTime = fwTimer::GetTimeInMilliseconds() - m_uLastFpsPitchUpdateTime;
					if(m_bBlendInSetHeading && uLerpTime < LERP_PITCH_AFTER_START_TIME_MS)
					{
						const float fLerpRatio = (float)uLerpTime / (float)LERP_PITCH_AFTER_START_TIME_MS;
						fMaxRateChange = Lerp(fLerpRatio, fMaxRateChange, PITCH_CHANGE_RATE_FPS);
					}
					else
					{
						fMaxRateChange = PITCH_CHANGE_RATE_FPS;
					}

					bUsingFpsPitch = true;
				}
			}
#endif // FPS_MODE_SUPPORTED
		}

		if(!bUsingFpsPitch)
		{
			m_uLastFpsPitchUpdateTime = fwTimer::GetTimeInMilliseconds();
		}

		if(GetOverwriteMaxPitchChange() != -1.0f)
		{
			float diff = Abs(m_fCurrentPitch - m_fDesiredPitch);

			fMaxRateChange = diff* fPitchChangeRate + (1-diff)*GetOverwriteMaxPitchChange();
		}

		const float fTimeStep = (fUseTimeStep >= 0.0f) ? fUseTimeStep: CalculateUnWarpedTimeStep(fwTimer::GetTimeStep());

		static dev_float TINY_FLOAT = 0.001f;
		float fDiff = Abs(m_fDesiredPitch - m_fCurrentPitch);
		if(fDiff > TINY_FLOAT)
		{
			Approach(m_fPitchLerp, 1.f, ms_Tunables.m_PitchChangeRateAcceleration, fTimeStep);
		}
		else
		{
			Approach(m_fPitchLerp, 0.f, ms_Tunables.m_PitchChangeRateAcceleration, fTimeStep);
		}

		float fPitchRate = Lerp(m_fPitchLerp, 0.f, fMaxRateChange);

		// The desired heading is smoothed so it doesn't jump too much in a timestep
		Approach(m_fCurrentPitch, m_fDesiredPitch, fPitchRate, fTimeStep);
	}
	else
	{
		m_fCurrentPitch = m_fDesiredPitch;
	}

	SetOverwriteMaxPitchChange(-1.0f);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ComputeAimIntroSignals(float fUseTimeStep)
{
	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (GetPed()->UseFirstPersonUpperBodyAnims(false))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	if(!m_bAimIntroFinished && !bFPSMode)
	{
		if(GetState() == State_Initial)
			return;

		CPed* pPed = GetPed();

		const CTaskMotionAimingTransition* pTransitionTask = 
			GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION ? static_cast<const CTaskMotionAimingTransition*>(GetSubTask()) : NULL;

		// Work out the initial direction, this will calculate the weight for the aim intro anims, and form a starting point for the phase calculation
		if(m_fAimIntroDirection == FLT_MAX)
		{
			// If moving, use the strafe direction
			if(GetState() == State_StrafingIntro || GetState() == State_Strafing || GetState() == State_StartStrafing || GetState() == State_StopStrafing)
			{
				m_fAimIntroDirection = m_fStrafeDirection;
			}
			else if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0 && !IsClose(pPed->GetMotionData()->GetIndependentMoverHeadingDelta(), 0.f, 0.01f))
			{
				m_fAimIntroDirection = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();
			}
			else
			{
				const float fFacingDirection = pPed->GetFacingDirectionHeading();
				const float fTargetHeading = pPed->GetCurrentHeading();
				m_fAimIntroDirection = SubtractAngleShorter(fFacingDirection, fTargetHeading);
			}

			if(pTransitionTask)
			{
				if(pTransitionTask->GetDirection() == CTaskMotionAimingTransition::D_CW)
				{
					// Going clockwise, clamp to right direction
					if(m_fAimIntroDirection < 0.f)
						m_fAimIntroDirection = PI;
				}
				else
				{
					// Going clockwise, clamp to right direction
					if(m_fAimIntroDirection > 0.f)
						m_fAimIntroDirection = -PI;
				}
			}
		}

		// Work out the difference between our facing direction, and our target direction, 
		// In the case of independent mover, should be the result of the independent mover unless blending multiple strafe transitions.  
		// This is mainly because the mover pop has not yet happened
		float fHeadingDiff;
		if(pPed->GetMotionData()->GetIndependentMoverTransition() == 0 || IsClose(pPed->GetMotionData()->GetIndependentMoverHeadingDelta(), 0.f, 0.01f))
		{
			const float fFacingDirection = pPed->GetFacingDirectionHeading();
			const float fTargetHeading = pPed->GetCurrentHeading();
			fHeadingDiff = SubtractAngleShorter(fFacingDirection, fTargetHeading);
		}
		else
		{
			fHeadingDiff = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();
		}

		float fMovingDirection = Abs(m_fAimIntroDirection);
		fHeadingDiff = Abs(fHeadingDiff);
		fHeadingDiff = Min(fHeadingDiff, fMovingDirection);

		// Update phase based on difference between fHeadingDiff and fMovingDirection
		if(IsClose(fMovingDirection, 0.f, 0.01f) || IsClose(fHeadingDiff, 0.f, 0.01f))
		{
			// Just animate if aiming forward, as there will be no angle change
			static dev_float PLAYBACK_SPEED = 5.f;
			float fTimeStep = fUseTimeStep >= 0.0f ? fUseTimeStep : fwTimer::GetTimeStep();
			m_fAimIntroPhase += fTimeStep * PLAYBACK_SPEED;
		}
		else
		{
			m_fAimIntroPhase = 1.f - Clamp(Abs(fHeadingDiff) / Abs(fMovingDirection), 0.f, 1.f);
		}

		m_fAimIntroPhase = Clamp(m_fAimIntroPhase, 0.f, 1.f);
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::CalculateTurnBlendFactor() const
{
	// Grab the ped.
	const CPed* pPed = GetPed();
	
	// Grab the current heading.
	float fCurrentHeading = pPed->GetCurrentHeading();
	
	// Grab the desired heading.
	float fDesiredHeading = pPed->GetDesiredHeading();
	
	// Calculate the direction value.
	return CDirectionHelper::CalculateDirectionValue(fCurrentHeading, fDesiredHeading);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ChooseDefaultAimingClip(fwMvClipSetId& clipSetId, fwMvClipId& clipId) const
{
	// Assign the clip set id.
	clipSetId = m_MoveGunOnFootNetworkHelper.GetClipSetId();

#if FPS_MODE_SUPPORTED
	const CPed* pPed = GetPed();
	// Chose the fidget clip if we're in FPS mode and flagged to play fidgets
	if (pPed->UseFirstPersonUpperBodyAnims(false) && pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets))
	{
		static const fwMvClipId CLIP_FIDGET_MED_LOOP("FIDGET_MED_LOOP",0x85cff9f9);
		
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(pClipSet && pClipSet->GetClip(CLIP_FIDGET_MED_LOOP))
		{
			clipId = CLIP_FIDGET_MED_LOOP;
			return;
		}
	}
#endif // FPS_MODE_SUPPORTED

	// Assign the clip id.
	clipId = /*GetIsCrouched() ? CLIP_AIM_MED_LOOP_CROUCH :*/ CLIP_AIM_MED_LOOP;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ChooseNewAimingClip()
{
	//Check if we are playing a custom aiming clip.
	if(m_bIsPlayingCustomAimingClip)
	{
		//Release the clip set.
		m_ClipSetRequestHelperForCustomAimingClip.ReleaseClipSet();
	}

	//Choose an aiming clip.
	fwMvClipSetId clipSetId;
	fwMvClipId clipId;
	if(!ChooseVariedAimingClip(clipSetId, clipId))
	{
		ChooseDefaultAimingClip(clipSetId, clipId);
	}

	//Assert that the clip is valid.
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	taskAssert(clipId != CLIP_ID_INVALID);

	//Check if we are playing a custom aiming clip.
	m_bIsPlayingCustomAimingClip = m_ClipSetRequestHelperForCustomAimingClip.IsActive() &&
		(clipSetId == m_ClipSetRequestHelperForCustomAimingClip.GetClipSetId());

	//Check if we are not playing a custom aiming clip, and a request has been made.
	if(!m_bIsPlayingCustomAimingClip && m_ClipSetRequestHelperForCustomAimingClip.IsActive())
	{
		//Check if the clip set should be streamed out, or is streamed in and should be streamed in.
		//Either way, the clip set should be released -- because we either aren't going to use it,
		//or because we had a chance to choose it for our new aiming clip, and didn't.
		if(m_ClipSetRequestHelperForCustomAimingClip.ShouldClipSetBeStreamedOut() ||
			(m_ClipSetRequestHelperForCustomAimingClip.IsClipSetStreamedIn() && m_ClipSetRequestHelperForCustomAimingClip.ShouldClipSetBeStreamedIn()))
		{
			//Release the clip set.
			m_ClipSetRequestHelperForCustomAimingClip.ReleaseClipSet();
		}
	}

	//Ensure the aiming clip has changed.
	if((clipSetId == m_AimingClipSetId) && (clipId == m_AimingClipId))
	{
		return false;
	}

	//Set the aiming clip.
	m_AimingClipSetId	= clipSetId;
	m_AimingClipId		= clipId;

	m_bNeedToSetAimClip = true;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionAiming::ChooseTurnClipSet() const
{
	// Grab the ped.
	const CPed* pPed = GetPed();
	
	// Ensure the aim pose is not stationary.
	// The stationary aim poses do not match up with the turn animations.
	if(GetIsStationary())
	{
		return CLIP_SET_ID_INVALID;
	}
	
	// Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return CLIP_SET_ID_INVALID;
	}
	
	// Ensure the equipped weapon info is valid.
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	return m_bIsCrouched ? pWeaponInfo->GetAimTurnCrouchingClipSetId(*pPed) : pWeaponInfo->GetAimTurnStandingClipSetId(*pPed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ChooseVariedAimingClip(fwMvClipSetId& clipSetId, fwMvClipId& clipId) const
{
	//Choose the clip set.
	if(!m_bIsPlayingCustomAimingClip && m_ClipSetRequestHelperForCustomAimingClip.ShouldUseClipSet())
	{
		clipSetId = m_ClipSetRequestHelperForCustomAimingClip.GetClipSetId();
	}
	else
	{
		clipSetId = GetAlternateAimingClipSetId();
	}

	//Ensure the clip set is valid.
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	// Get the clip set.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!pClipSet)
	{
		return false;
	}

	// Ensure the clip set is streamed in.
	if(!pClipSet->IsStreamedIn_DEPRECATED())
	{
		return false;
	}

	// Grab the count.
	const int iCount = pClipSet->GetClipItemCount();
	if(iCount <= 0)
	{
		return false;
	}

	//Choose an index.
	const int iIndex = fwRandom::GetRandomNumberInRange(0, iCount);

	//Ensure the clip id is valid.
	clipId = pClipSet->GetClipItemIdByIndex(iIndex);
	if(clipId == CLIP_ID_INVALID)
	{
		return false;
	}

	if(GetPed()->HasHurtStarted())
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionAiming::GetAlternateAimingClipSetId() const
{
	// Grab the ped.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	// Grab the weapon info.
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	return (GetIsCrouched() ? pWeaponInfo->GetAlternateAimingCrouchingClipSetId(*pPed) : pWeaponInfo->GetAlternateAimingStandingClipSetId(*pPed));
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SetAimingClip()
{
	// Grab the clip.
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_AimingClipSetId, m_AimingClipId);
	if(!pClip)
	{
		return;
	}
	
	if(SetMoveClip(pClip, CTaskAimGunOnFoot::ms_AimingClipId))
	{
		// If we are in the aim state, we shouldn't have to keep calling this, which
		// is fairly expensive.
		if(m_bEnteredAimState)
		{
			m_bNeedToSetAimClip = false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ShouldChooseNewAimingClip() const
{
	//Check if the aiming clip has ended.
	if(m_bMoveAimingClipEnded || m_bMoveFPSFidgetClipEnded)
	{
		return true;
	}

	//Check if we aren't playing a custom aiming clip, and we should use it.
	if(!m_bIsPlayingCustomAimingClip && m_ClipSetRequestHelperForCustomAimingClip.ShouldUseClipSet())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ShouldProcessPhysics() const
{
	// Grab the ped.
	const CPed* pPed = GetPed();

	// We don't want to scale the angular velocity the first frame of the cover outro,
	// as we can't force into animated velocity. We need the aiming pose the first frame.
	// The motion aiming task will get removed the next frame once we get a pose from the aim outro anims
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_CoverOutroRunning))
	{
		return false;
	}
	
	// Also don't muck about with the velocities during ai cover entry
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_AICoverEntryRunning))
	{
		return false;
	}
	
	// Ignore turning while we are doing an animated turn.
	if(GetState() == State_Turn || GetState() == State_StandingIdleIntro)
	{
		return false;
	}

	if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE))
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ShouldTurnOffUpperBodyAnimations(float& fBlendDuration) const
{
#if FPS_MODE_SUPPORTED
	const CPed* pPed = GetPed();
	if(pPed->UseFirstPersonUpperBodyAnims(false))
	{
		// B*2070206: Disable upper body anims if we're swimming
		if (pPed->GetIsFPSSwimming())
		{
			return true;
		}

		if(pPed->GetWeaponManager())
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon && (pWeapon->GetWeaponInfo()->GetIsUnarmed() || pWeapon->GetWeaponInfo()->GetIsMeleeFist()) && !GetPed()->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) && !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
			{
				fBlendDuration = 0.25f;
				m_bWasUnarmed = true;

				// B*2102745: Don't use FPS unarmed aiming anims when using the phone.
				if (GetPed()->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
				{
					return true;
				}
				//We want to play FPS aiming unarmed animations
				else if(pPed->GetMotionData()->GetIsFPSLT() || pPed->GetMotionData()->GetIsUsingStealthInFPS())
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fMeleeIdleToAim, 0.1f, 0.0f, 2.0f, 0.001f);
					fBlendDuration = fMeleeIdleToAim;
					return false;
				}
				else if(pPed->GetMotionData()->GetWasFPSLT() && pPed->GetMotionData()->GetIsFPSIdle())
				{
					m_bFPSPlayToUnarmedTransition = true;
					return false;
				}
				else if(pPed->GetMotionData()->GetToggledStealthWhileFPSIdle() && pPed->GetMotionData()->GetWasUsingStealthInFPS())
				{
					m_bFPSPlayToUnarmedTransition = true;
					return false;
				}
				else if(m_bFPSPlayToUnarmedTransition)
				{
					return false;
				}
				else
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fMeleeAimToIdle, 0.35f, 0.0f, 2.0f, 0.001f);
					m_bFPSPlayToUnarmedTransition = false;
					fBlendDuration = fMeleeAimToIdle;
					return true;
				}
			}
			else if(m_bWasUnarmed)
			{
				fBlendDuration = 0.25f;
				m_bWasUnarmed = false;
				m_bFPSPlayToUnarmedTransition = false;
				return false;
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	//Check if we are turning.
	if(IsTurning())
	{
		fBlendDuration = 0.f;
		return true;
	}
	else if(GetState() == State_Roll)
	{
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMBAT_ROLL)
		{
			fBlendDuration = 0.25f;
			if(static_cast<const CTaskCombatRoll*>(GetSubTask())->ShouldTurnOffUpperBodyAnimations())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	else if(GetPreviousState() == State_Roll)
	{
		fBlendDuration = 0.25f;
		return false;
	}

	fBlendDuration = 0.f;
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::ShouldTurn() const
{
	// Grab the ped.
	const CPed* pPed = GetPed();
	
	// Ensure the ped is not in cover.
	if(pPed->GetIsInCover())
	{
		return false;
	}

	// Ensure the clip set request helper is active.
	if(!m_TurnClipSetRequestHelper.IsActive())
	{
		return false;
	}

	// Ensure the clip set should not be streamed out.
	if(m_TurnClipSetRequestHelper.ShouldClipSetBeStreamedOut())
	{
		return false;
	}

	// Ensure the clip set should be streamed in.
	if(!m_TurnClipSetRequestHelper.ShouldClipSetBeStreamedIn())
	{
		return false;
	}

	// Ensure the clip set is streamed in.
	if(!m_TurnClipSetRequestHelper.IsClipSetStreamedIn())
	{
		return false;
	}
	
	// Ensure the current pitch has not exceeded the threshold.
	float fMaxVariationForCurrentPitch = ms_Tunables.m_Turn.m_MaxVariationForCurrentPitch;
	float fVariationForCurrentPitch = Abs(m_fCurrentPitch - 0.5f);
	if(fVariationForCurrentPitch > fMaxVariationForCurrentPitch)
	{
		return false;
	}
	
	// Ensure the desired pitch has not exceeded the threshold.
	float fMaxVariationForDesiredPitch = ms_Tunables.m_Turn.m_MaxVariationForDesiredPitch;
	float fVariationForDesiredPitch = Abs(m_fDesiredPitch - 0.5f);
	if(fVariationForDesiredPitch > fMaxVariationForDesiredPitch)
	{
		return false;
	}

	// Calculate the turn blend factor.
	float fValue = CalculateTurnBlendFactor();
	
	// Ensure the value has surpassed the threshold.
	float fThreshold = GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatAimTurnThreshold);
	if(Abs(fValue - 0.5f) < fThreshold)
	{
		return false;
	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetCanMove(const CPed* pPed)
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(pPed->IsPlayer() && pPed->IsNetworkClone())
		{
			return ((CNetObjPlayer*)pPed->GetNetworkObject())->GetCanOwnerMoveWhileAiming();
		}
	}

	if(pPed->GetIsInCover())
	{
		return false;
	}
	else if(!CPauseMenu::GetMenuPreference(PREF_SNIPER_ZOOM) && pPed->IsPlayer() && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
	{
		const bool bIsScopedWeapon = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();

		bool bUsingKeyboardAndMouseOnPC = false;
#if RSG_PC
		// B*2214445: Ignore sniper movement preference if using mouse and keyboard controls (always allow movement).
		if (pPed->IsPlayer())
		{
			const CControl *pControl = pPed->GetControlFromPlayer();
			if (pControl && pControl->WasKeyboardMouseLastKnownSource())
			{
				bUsingKeyboardAndMouseOnPC = true;
			}
		}
#endif	// RSG_PC

		if(bIsScopedWeapon && !bUsingKeyboardAndMouseOnPC
#if FPS_MODE_SUPPORTED
			&& (!pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true) || pPed->GetMotionData()->GetIsFPSScope())
#endif // FPS_MODE_SUPPORTED
			)
		{
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetWantsToMove(const CPed* pPed)
{
	static dev_float MOVING_SPEED = 0.1f;
	static dev_float FPS_MOVING_SPEED = 0.01f;
	const float fMovingSpeed = FPS_MODE_SUPPORTED_ONLY(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) ? FPS_MOVING_SPEED :) MOVING_SPEED;
	return pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(fMovingSpeed) && GetCanMove(pPed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetWantsToCoverStep() const
{
	const CPed& ped = *GetPed();
	CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (pInCoverTask && pInCoverTask->GetWantsToTriggerStep())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetUseIdleIntro() const
{
	// Check if we should skip the idle intro.
	if(m_bSkipIdleIntro)
	{
		return false;
	}

	if(!m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		return false;
	}

	const fwClipSet* pClipSet = m_MoveGunStrafingMotionNetworkHelper.GetClipSet();
	if(pClipSet)
	{
		const atArray<atHashString>& moveFlags = pClipSet->GetMoveNetworkFlags();
		for(s32 i = 0; i < moveFlags.GetCount(); i++)
		{
			if(moveFlags[i] == ms_SkipIdleIntroId)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetUseStartsAndStops() const
{
	const CPed* pPed = GetPed();
	if(UsingLeftTriggerAimMode())
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	// Don't use starts/stops in FPS Swim-Strafe
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		return false;
	}
#endif	//FPS_MODE_SUPPORTED

	static dev_float MOVING_VEL = 1.f;
	if(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetVelocity().Mag2() > square(MOVING_VEL))
	{
		return false;
	}

	const fwMvClipSetId clipSetId = m_MoveGunStrafingMotionNetworkHelper.GetClipSetId();
	if(CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, ms_SkipStartsAndStopsId))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetUseTurn180() const
{
	const CPed* pPed = GetPed();
	if(UsingLeftTriggerAimMode())
	{
		return false;
	}

	static dev_float MOVING_VEL = 1.f;
	if(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetVelocity().Mag2() > square(MOVING_VEL))
	{
		return false;
	}

	const fwMvClipSetId clipSetId = m_MoveGunStrafingMotionNetworkHelper.GetClipSetId();
	if(CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, ms_SkipTurn180Id))
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && !ms_Tunables.m_FPSForceUse180s)
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetDesiredStrafeDirection(float& fStrafeDirection) const
{
	const CPed* pPed = GetPed();
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
	if(vDesiredMBR.Mag2() > 0.f)
	{
		fStrafeDirection = fwAngle::LimitRadianAngleSafe(rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y));
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetCurrentStrafeDirection(float& fStrafeDirection) const
{
	const CPed* pPed = GetPed();
	Vector2 vMBR;
#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && GetState() != State_Turn180)
	{
		vMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		vMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	}
	if(vMBR.Mag2() > 0.f)
	{
		//vCurrentMBR.Normalize();
		fStrafeDirection = fwAngle::LimitRadianAngleSafe(rage::Atan2f(-vMBR.x, vMBR.y));
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsLastFootLeft() const
{
	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		if(CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelLId))
		{
			return true;
		}
		else if(CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelRId))
		{
			return false;
		}
	}

	return m_bLastFootLeft;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::UseLeftFootStrafeTransition() const
{
	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
		return CTaskHumanLocomotion::CheckBooleanEvent(this, ms_LeftFootStrafeTransitionId);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::GetAimIntroFinished() const
{
	static dev_float PHASE = 0.75f;
	return m_bAimIntroFinished || m_bAimIntroTimedOut || m_fAimIntroPhase >= PHASE;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::IsTransitionBlocked() const
{
#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true) && GetPed()->GetIsSwimming())
	{
		return true;
	}
#endif	//FPS_MODE_SUPPORTED

	static const fwMvBooleanId s_Block("BLOCK_TRANSITION",0x70ABE2B4);
	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive() && CTaskHumanLocomotion::CheckBooleanEvent(this, s_Block) && !IsFootDown())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::WillStartInStrafingIntro(const CPed* pPed) const
{
	static dev_float MOVING_VEL = 1.f;
	return !m_bBlockTransition && (!pPed->GetGroundPhysical() || pPed->GetGroundPhysical()->GetVelocity().Mag2() < square(MOVING_VEL)) && CTaskMotionAimingTransition::UseMotionAimingTransition(CTaskGun::GetTargetHeading(*pPed), pPed->GetFacingDirectionHeading(), pPed);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::StopAimIntro()
{
	static dev_float AIM_INTRO_QUIT_PHASE = 0.8f;
	if(m_fAimIntroPhase >= AIM_INTRO_QUIT_PHASE)
	{
		m_bAimIntroTimedOut = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::PlayCustomAimingClip(fwMvClipSetId clipSetId)
{
	//Request the clip.
	m_ClipSetRequestHelperForCustomAimingClip.Request(clipSetId);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::DoPostCameraAnimUpdate() const
{
	const CPed* pPed = GetPed();
	if(!fwTimer::IsGamePaused() && ms_Tunables.m_DoPostCameraClipUpdateForPlayer && pPed->IsLocalPlayer() 
#if FPS_MODE_SUPPORTED
		&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true)
#endif // FPS_MODE_SUPPORTED
		)
	{
		// Update the player's clips instantly for aim state to prevent the clip lagging behind
		if(IsStateValidForPostCamUpdate())
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ReadCommonMoveSignals()
{
	//Check if the move network is active.
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_bMoveAimOnEnterState		|= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_AimOnEnterStateId);
		m_bMoveFiringOnEnterState	|= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_FiringOnEnterStateId);
		m_bMoveAimIntroOnEnterState	|= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_AimIntroOnEnterStateId);
		m_bMoveAimingClipEnded		|= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_AimingClipEndedId);

#if FPS_MODE_SUPPORTED
		if (GetPed()->UseFirstPersonUpperBodyAnims(false))
		{
			m_bMoveFPSFidgetClipEnded |= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_FidgetClipEndedId);
			m_bMoveFPSFidgetClipNonInterruptible |= m_MoveGunOnFootNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_FidgetClipNonInterruptibleId);

			if(m_MoveGunOnFootNetworkHelper.GetBoolean(ms_FPSAimIntroTransitionId))
			{
				GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, false);
				m_bFPSPlayToUnarmedTransition = false;
			}

		}
#endif	//FPS_MODE_SUPPORTED
	}

	//Check if the move network is active.
	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		m_bMoveWalking |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_WalkingId);
		m_bMoveRunning |= m_MoveGunStrafingMotionNetworkHelper.GetBoolean(ms_RunningId);

		if(CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelLId))
		{
			m_bLastFootLeft = true;
		}
		else if(CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelRId))
		{
			m_bLastFootLeft = false;
		}
		m_MoveGunStrafingMotionNetworkHelper.SetFlag(m_bLastFootLeft, ms_LastFootLeftId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ResetCommonMoveSignals()
{
	m_bMoveAimOnEnterState			= false;
	m_bMoveFiringOnEnterState		= false;
	m_bMoveAimingClipEnded			= false;
	m_bMoveWalking					= false;
	m_bMoveRunning					= false;
	m_bMoveAimIntroOnEnterState		= false;
	m_bMoveFPSFidgetClipEnded		= false;
	m_bMoveFPSFidgetClipNonInterruptible = false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::ComputeRandomAnimRate() const
{
	static dev_float MIN_RATE = 0.9f;
	static dev_float MAX_RATE = 1.1f;
	float fAnimRate = MIN_RATE + (MAX_RATE-MIN_RATE)*((float)GetPed()->GetRandomSeed())/((float)RAND_MAX_16);
	return fAnimRate;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::UpdateAdditives()
{
	float fDesiredStrafeSpeed = m_fStrafeSpeed;
#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, REMAP_MBR_TO_ANALOGUE_CONTROL, true);
		if(REMAP_MBR_TO_ANALOGUE_CONTROL)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, WALK_THRESHOLD, 0.01f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, RUN_THRESHOLD, 0.55f, 0.0f, 1.0f, 0.01f);
			if(GetState() == State_StandingIdle || GetState() == State_StopStrafing)
			{
				fDesiredStrafeSpeed = 0.f;
			}
			else if(m_bIsRunning)//fDesiredStrafeSpeed > RUN_THRESHOLD)
			{
				fDesiredStrafeSpeed = 0.66f;
			}
			else if(fDesiredStrafeSpeed > 0.f)
			{
				fDesiredStrafeSpeed = 0.33f;
			}
			else
			{
				fDesiredStrafeSpeed = 0.f;
			}
		}

		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, ADDITIVE_SPEED_LERP_RATE_DEC, 2.f, 0.f, 100.f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, ADDITIVE_SPEED_LERP_RATE_ACC, 1.7f, 0.f, 100.f, 0.01f);

		float fLerpRate = fDesiredStrafeSpeed >= m_fStrafeSpeedAdditive ? ADDITIVE_SPEED_LERP_RATE_ACC : ADDITIVE_SPEED_LERP_RATE_DEC;
		Approach(m_fStrafeSpeedAdditive, fDesiredStrafeSpeed, fLerpRate, GetTimeStep());
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		static dev_float ADD_SPEED_LERP_RATE = 1.f;
		Approach(m_fStrafeSpeedAdditive, fDesiredStrafeSpeed, ADD_SPEED_LERP_RATE, GetTimeStep());
	}

	static dev_float ADD_DIR_LERP_RATE = 3.f;
	InterpValue(m_fStrafeDirectionAdditive, m_fStrafeDirection, ADD_DIR_LERP_RATE, true, GetTimeStep());
	m_fStrafeDirectionAdditive = fwAngle::LimitRadianAngle(m_fStrafeDirectionAdditive);
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::CalculateUnWarpedTimeStep(float fTimeStep) const
{
	const CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer())
	{
		const CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility();
		if(pSpecialAbility && pSpecialAbility->IsActive() && pSpecialAbility->GetType() == SAT_BULLET_TIME)
		{
			fTimeStep /= fwTimer::GetTimeWarpActive();
		}
	}

	return fTimeStep;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::ComputeStandingIdleSignals()
{
	CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer())
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, PLAYER_STRAFE_IDLE_TURN_RATE, 7.5f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, PLAYER_STRAFE_IDLE_TURN_MINVEL, 0.05f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, PLAYER_STRAFE_IDLE_TURN_MAXVEL, 1.0f, 0.0f, 10.0f, 0.1f);

		float fIdleTurnTarget = 0.f;
		if(Abs(m_fCameraTurnSpeed) > PLAYER_STRAFE_IDLE_TURN_MINVEL)
		{
			fIdleTurnTarget = Sign(m_fCameraTurnSpeed) * Clamp(Abs(m_fCameraTurnSpeed), PLAYER_STRAFE_IDLE_TURN_MINVEL, PLAYER_STRAFE_IDLE_TURN_MAXVEL);
			fIdleTurnTarget = fIdleTurnTarget / PLAYER_STRAFE_IDLE_TURN_MAXVEL;
			// Convert to -PI PI range, so we can still do ConvertRadianToSignal
			fIdleTurnTarget *= PI;
		}

		Approach(m_fStartDirection, fIdleTurnTarget, PLAYER_STRAFE_IDLE_TURN_RATE, fwTimer::GetTimeStep());
	}
	else
	{
		float fDesiredHeading = CalcDesiredDirection();
		
		// Turn Rate
		TUNE_GROUP_FLOAT(PED_MOVEMENT, STRAFE_IDLE_TURN_RATE, 0.2f, 0.0f, 10.0f, 0.1f);
		static dev_float ANGLE_CLAMP = DtoR * 5.4f;
		float fIdleTurnTarget = Clamp(fDesiredHeading, -ANGLE_CLAMP, ANGLE_CLAMP);
		m_fStartDirection = fwAngle::Lerp(m_fStartDirection, fIdleTurnTarget, STRAFE_IDLE_TURN_RATE * fwTimer::GetTimeStep());
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionAiming::SendStandingIdleSignals()
{
	if(m_MoveGunOnFootNetworkHelper.IsNetworkActive())
	{
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirectionId, ConvertRadianToSignal(m_fStrafeDirection));
		m_MoveGunOnFootNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, ConvertRadianToSignal(m_fStrafeDirectionAdditive));
	}

	if(m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive())
	{
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_StrafeSpeedId, m_fStrafeSpeed);

		// Desired Direction
		float fDesiredHeading = CalcDesiredDirection();
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_DesiredHeadingId, ConvertRadianToSignal(fDesiredHeading));

		// Turn Rate
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_IdleTurnRateId, ConvertRadianToSignal(m_fStartDirection));

		// If the desired angle is too big, then increase the animation playback
		float fIdleTurnRate = 1.f;
		if(!GetPed()->IsLocalPlayer())
		{
			fIdleTurnRate = ComputeRandomAnimRate();
		}
		m_MoveGunStrafingMotionNetworkHelper.SetFloat(ms_IdleTurnAnimRateId, fIdleTurnRate);
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionAiming::CalculateNormaliseVelocity(const bool FPS_MODE_SUPPORTED_ONLY(bFirstPersonModeEnabled)) const
{
#if FPS_MODE_SUPPORTED
	const CPed* pPed = GetPed();
	float fSlopePitch;

	bool bInFPSIdle = false;
	bool bStickWithinStrafeAngleThreshold = false;
	bool bOnSlope = CTaskHumanLocomotion::IsOnSlopesAndStairs(pPed, false, fSlopePitch);

	if (bFirstPersonModeEnabled)
	{
		bInFPSIdle = GetMotionData().GetIsFPSIdle();
		bStickWithinStrafeAngleThreshold = pPed->IsFirstPersonShooterModeStickWithinStrafeAngleThreshold();

		if (pPed->IsNetworkClone())
		{
			NetworkInterface::IsRemotePlayerInFirstPersonMode(*pPed, &bInFPSIdle, &bStickWithinStrafeAngleThreshold, &bOnSlope);
		}
	}

	if(bFirstPersonModeEnabled)
	{
		if(bInFPSIdle && !bStickWithinStrafeAngleThreshold && IsClose(m_fSnowDepthSignal, 0.f, SMALL_FLOAT) && !bOnSlope)
		{
			float fCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();
			float fPercentage = Clamp((fCurrentMBR-MOVEBLENDRATIO_WALK)/(MOVEBLENDRATIO_RUN-MOVEBLENDRATIO_WALK), 0.0f, 1.0f);
			float fLerpResult = Lerp(fPercentage, m_fOnFootWalkVel * m_fWalkMovementRate, m_fOnFootRunVel);
			return fLerpResult *= pPed->GetMotionData()->GetCurrentMoveRateOverride();			
		}
		else
		{
			float fCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();
			fCurrentMBR = Min(fCurrentMBR, MOVEBLENDRATIO_RUN);
			float fPercentage = Clamp((fCurrentMBR-MOVEBLENDRATIO_WALK)/(MOVEBLENDRATIO_RUN-MOVEBLENDRATIO_WALK), 0.0f, 1.0f);
			float fLerpResult = Lerp(fPercentage, m_fWalkVel * m_fWalkMovementRate, m_fRunVel);
			return fLerpResult *= pPed->GetMotionData()->GetCurrentMoveRateOverride();
		}
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		return m_bIsRunning ? m_fRunVel : (m_fWalkVel * m_fWalkMovementRate);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionAiming::DoPostCameraSetHeading() const
{
	TUNE_GROUP_BOOL(PED_MOVEMENT, bDoPostCameraSetHeading, true);

    const CPed* pPed = GetPed();
	if(bDoPostCameraSetHeading && 
		pPed->IsPlayer() && 
		pPed->GetPlayerInfo() &&
		pPed->GetMotionData()->GetIsStrafing() && 
		pPed->GetMotionData()->GetIndependentMoverTransition() == 0 &&
		!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride) && 
		!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange) &&
		!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro) &&
		!pPed->GetUsingRagdoll() &&
		!pPed->GetPlayerInfo()->AreControlsDisabled() &&
		!pPed->GetIsAttached())
	{
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////v/////////////////////////

bool CTaskMotionAiming::UsingLeftTriggerAimMode() const
{
	const CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer() && 
		pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() && 
		!pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && 
		!pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
	{
		return true;
	}

    if(pPed->IsPlayer() && pPed->IsNetworkClone())
    {
        if(NetworkInterface::IsPlayerUsingLeftTriggerAimMode(pPed))
        {
            return true;
        }
    }

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskMotionAiming::VerifyMovementClipSet()
{
	fwMvClipSetId clipSetId = GetClipSet();
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", clipSetId.TryGetCStr()))
		{
			atArray<CTaskHumanLocomotion::sVerifyClip> clips;
			// Idles 5
			//clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle",0x71c21326), CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle_turn_left_fast",0x6928b262), CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle_turn_left_slow",0x864b7117), CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle_turn_right_fast",0x31a4acd7), CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle_turn_right_slow",0xa36744d9), CTaskHumanLocomotion::VCF_MovementCycle));

			aiDisplayf("clipSetId = %s", clipSetId.GetCStr());
			const CPed* pPed = GetPed();
			if(GetCanMove(pPed))
			{
				static const fwMvClipSetId coverWeaponClipSetId("cover@weapon@core",0xD8BD0F2E);
				aiDisplayf("coverWeaponClipSetId = %s", coverWeaponClipSetId.GetCStr());
				if (coverWeaponClipSetId == clipSetId)
				{
					pPed->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
					taskWarningf("%s Ped %s thinks they can move when using cover strafing clipset %s, is in cover ? %s, please add a bug with console log to default ai", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), coverWeaponClipSetId.GetCStr(), AILogging::GetBooleanAsString(pPed->GetIsInCover()));
				}

				// Cycles 20
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-45_loop",0x46e14aa0), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-90_loop",0x71d22130), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-135_loop",0x86f882d5), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_180_loop",0xf1e9acaf), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_135_loop",0xc1aadb03), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_-45_loop",0x775b4924), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_0_loop",0x90fa019a), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_45_loop",0xaf4128ba), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_90_loop",0xd8272eb8), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_135_loop",0xc6f4f444), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-45_loop",0x07deed8a), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-90_loop",0x82082e9b), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-135_loop",0x919493a9), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_180_loop",0xf284ad6d), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_135_loop",0x350f5215), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_-45_loop",0x06021b04), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_0_loop",0x5794e6ce), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_45_loop",0x12d03489), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_90_loop",0x732556f2), CTaskHumanLocomotion::VCF_MovementCycle));
				clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_135_loop",0xfa37070b), CTaskHumanLocomotion::VCF_MovementCycle));

				if(!CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, ms_SkipMovingTransitionsId))
				{
					// Transitions 16
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_-45_lf_fwd_-45",0x3f5cf88d), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_-45_rf_fwd_-45",0x87e6f55d), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_-45_lf_bwd_-45",0x660eadab), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_-45_rf_bwd_-45",0x22ed1a34), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_135_lf_fwd_135",0xd5dd44e7), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_135_rf_fwd_135",0x274db5cd), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_135_lf_bwd_135",0x6b52bd74), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_135_rf_bwd_135",0x48baf531), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_-45_lf_fwd_-45",0x78dd9e75), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_-45_rf_fwd_-45",0xfa9d2350), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_-45_lf_bwd_-45",0x9ffaf02e), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_-45_rf_bwd_-45",0x83c2e74a), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_135_lf_fwd_135",0x5c9ebf13), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_135_rf_fwd_135",0xff1cbc4e), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_135_lf_bwd_135",0x2817e39b), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_135_rf_bwd_135",0x2bdfd65f), CTaskHumanLocomotion::VCF_MovementOneShotRight));
				}

				if(!CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, ms_SkipStartsAndStopsId))
				{
					// Starts 18
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-45_start",0x34a9281e), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-90_start",0x5f655b0d), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-135_start",0x5c827387), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_180_start",0xb0ad4d31), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_135_start",0xc37d75c0), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_-45_start",0xd0bc6bf1), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_0_start",0x104f2ca5), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_90_start",0xd2e4cd04), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_135_start",0xc43001a1), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-45_start",0xd00bb239), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-90_start",0x5b11346b), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_-135_start",0x96877f39), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_180_start",0x38d5419d), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_bwd_135_start",0x9e9f28cd), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_-45_start",0x91816e9a), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_0_start",0x6fd1effb), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_90_start",0xf2290986), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_fwd_135_start",0x88e329aa), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					// Stop Lefts 9
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-45_stop_l",0x13eb6d7e), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-90_stop_l",0xca176f49), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-135_stop_l",0xffa236d8), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_180_stop_l",0xa0ce6dca), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_135_stop_l",0x9aaf7977), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_-45_stop_l",0x165d0e3c), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_0_stop_l",0x675f8f79), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_90_stop_l",0x9b596f0b), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_135_stop_l",0x6f60c2c6), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					// Stop Rights 9
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-45_stop_r",0x314aa848), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-90_stop_r",0xa5ada676), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_-135_stop_r",0xc59dc2d0), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_180_stop_r",0x4fb4cb94), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_bwd_135_stop_r",0x36233060), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_-45_stop_r",0x258a2ca2), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_0_stop_r",0xea4e1540), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_90_stop_r",0x017e3b57), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_fwd_135_stop_r",0xb64d50a2), CTaskHumanLocomotion::VCF_MovementOneShotRight));
				}

				if(!CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, ms_SkipTurn180Id))
				{
					// 180s 16
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_-90_lf_fwd_90",0xc9055145), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_-90_rf_fwd_90",0x301ec18d), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_180_lf_fwd_0",0x5c522e63), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_bwd_180_rf_fwd_0",0xb482f5c5), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_0_lf_bwd_180",0xf01b97c1), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_0_rf_bwd_180",0x08afab83), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_90_lf_bwd_-90",0x9f23ea6e), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk_trans_fwd_90_rf_bwd_-90",0xc0af978a), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_-90_lf_fwd_90",0x3efac6a8), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_-90_rf_fwd_90",0xf7407348), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_180_lf_fwd_0",0xd2b9fa33), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_bwd_180_rf_fwd_0",0xba959673), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_0_lf_bwd_180",0x96ee8e58), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_0_rf_bwd_180",0x72a9f580), CTaskHumanLocomotion::VCF_MovementOneShotRight));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_90_lf_bwd_-90",0xba44e7df), CTaskHumanLocomotion::VCF_MovementOneShotLeft));
					clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run_trans_fwd_90_rf_bwd_-90",0x211ef233), CTaskHumanLocomotion::VCF_MovementOneShotRight));
				}
			}
			// Don't verify the clips in Swim FPS mode
			bool bFPSSwimming = false;
#if FPS_MODE_SUPPORTED
			if (GetPed() && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true) && GetPed()->GetIsSwimming())
			{
				bFPSSwimming = true;
			}
#endif	//FPS_MODE_SUPPORTED
			if (!bFPSSwimming)
			{
				CTaskHumanLocomotion::VerifyClips(clipSetId, clips, GetEntity() ? GetPed() : NULL);
			}
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

CPreviousDirections::CPreviousDirections()
{
}

////////////////////////////////////////////////////////////////////////////////

void CPreviousDirections::RecordDirection(float fDirection)
{
	for(s32 i = MAX_PREVIOUS_DIRECTION_FRAMES-1; i > 0; i--)
	{
		m_PreviousDirections[i].fPreviousDirection = m_PreviousDirections[i-1].fPreviousDirection;
		m_PreviousDirections[i].uTime = m_PreviousDirections[i-1].uTime;
	}

	m_PreviousDirections[0].fPreviousDirection = fDirection;
	m_PreviousDirections[0].uTime = fwTimer::GetTimeInMilliseconds();
}

////////////////////////////////////////////////////////////////////////////////

void CPreviousDirections::ResetDirections(float fDirection, u32 uTime)
{
	for(s32 i = 0; i < MAX_PREVIOUS_DIRECTION_FRAMES; i++)
	{
		m_PreviousDirections[i].fPreviousDirection = fDirection;
		m_PreviousDirections[i].uTime = uTime;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPreviousDirections::GetDirectionForTime(const u32 uElapsedTime, float& fDirection) const
{
	for(s32 i = 0; i < MAX_PREVIOUS_DIRECTION_FRAMES; i++)
	{
		u32 uDirectionElapsedTime = fwTimer::GetTimeInMilliseconds() - m_PreviousDirections[i].uTime;
		if(uElapsedTime < uDirectionElapsedTime)
		{
			fDirection = m_PreviousDirections[i].fPreviousDirection;
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
