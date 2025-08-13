
#include "task/motion/Locomotion/TaskMotionPed.h"
#include "task/motion/Locomotion/TaskMotionStrafing.h"
#include "task/motion/Locomotion/TaskMotionAiming.h"

// rage includes


// game includes
#include "ai/debug/system/AIDebugLogManager.h"
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "peds/ped.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/PedIntelligence.h"
#include "modelinfo/PedModelInfo.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Streaming/PrioritizedClipSetStreamer.h"
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"
#include "task/motion/Locomotion/TaskMotionDrunk.h"
#include "task/motion/locomotion/TaskHumanLocomotion.h"
#include "task/motion/locomotion/TaskInWater.h"
#include "task/motion/locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionParachuting.h"
#include "task/Movement/TaskParachute.h"
#include "Task/Motion/Locomotion/TaskMotionJetpack.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Physics/TaskNM.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/default/TaskPlayer.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/TaskProjectile.h"
#include "renderer/Water.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Weapons/Info/WeaponAnimationsManager.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

const float CTaskMotionPed::AIMING_TRANSITION_DURATION = 0.f;

const fwMvStateId CTaskMotionPed::ms_AimingStateId("Aiming",0x9A5B381);
const fwMvStateId CTaskMotionPed::ms_ParachutingStateId("Parachuting",0x2FEB27EB);
const fwMvStateId CTaskMotionPed::ms_OnFootParentStateId("OnFootNetworks_SM",0xb0fdf753);
const fwMvStateId CTaskMotionPed::ms_OnFootParentParentStateId("OnFootNetworks",0x2be3daad);
const fwMvRequestId CTaskMotionPed::ms_DivingId("Diving",0x7F6CA80C);
const fwMvRequestId CTaskMotionPed::ms_DivingFromFallId("DivingFromFall",0x84350FE);
const fwMvRequestId CTaskMotionPed::ms_SwimmingId("Swimming",0x5DA2A411);
const fwMvRequestId CTaskMotionPed::ms_SwimToRunId("SwimToRun",0x26CFF917);
const fwMvRequestId CTaskMotionPed::ms_DiveToSwimId("DiveToSwim",0x1E0DC3B8);
const fwMvRequestId CTaskMotionPed::ms_CrouchId("Crouch",0x2751A740);
const fwMvRequestId CTaskMotionPed::ms_StealthId("Stealth",0x74A0BCB7);
const fwMvRequestId CTaskMotionPed::ms_ActionModeId("ActionMode",0x2E74C622);
const fwMvRequestId CTaskMotionPed::ms_OnFootId("OnFoot",0xF5A638B9);
const fwMvRequestId CTaskMotionPed::ms_AimingId("Aiming",0x9A5B381);
const fwMvRequestId CTaskMotionPed::ms_StrafingId("Strafing",0x1C181AC3);
const fwMvRequestId CTaskMotionPed::ms_InVehicleId("InVehicle",0x579FFAC8);
const fwMvRequestId CTaskMotionPed::ms_DoNothingId("DoNothing",0x1D5579BC);
const fwMvRequestId CTaskMotionPed::ms_DeadId("Dead",0xC9B954EA);
const fwMvRequestId CTaskMotionPed::ms_AnimatedVelocityId("AnimatedVelocity",0xD64C91A4);
const fwMvRequestId CTaskMotionPed::ms_StandToCrouchId("StandToCrouch",0x3013EB5F);
const fwMvRequestId CTaskMotionPed::ms_CrouchToStandId("CrouchToStand",0x90B6A440);
const fwMvRequestId CTaskMotionPed::ms_StrafeToOnFootId("StrafeToOnFoot",0x4FF23D60);
const fwMvRequestId CTaskMotionPed::ms_ParachutingId("Parachuting",0x2FEB27EB);
#if ENABLE_DRUNK
const fwMvRequestId CTaskMotionPed::ms_DrunkId("Drunk",0x3FDBA102);
#endif // ENABLE_DRUNK
const fwMvBooleanId CTaskMotionPed::ms_OnEnterDoNothing("OnEnter_DoNothing", 0x0352d52e);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterCrouchId("OnEnter_Crouch",0xBA5FAD97);
const fwMvBooleanId CTaskMotionPed::ms_OnExitCrouchId("OnExit_Crouch",0xD5B1C9CE);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterStealthId("OnEnter_Stealth",0x192858E7);
const fwMvBooleanId CTaskMotionPed::ms_OnExitStealthId("OnExit_Stealth",0x35DAE61);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterActionModeId("OnEnter_ActionMode",0x19F05A94);
const fwMvBooleanId CTaskMotionPed::ms_OnExitActionModeId("OnExit_ActionMode",0x82be57ef);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterDivingId("OnEnter_Diving",0x57C6ECC8);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterInVehicleId("OnEnter_InVehicle",0xF606CE6F);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterOnFootId("OnEnter_OnFoot",0xD850B358);
const fwMvBooleanId CTaskMotionPed::ms_OnExitOnFootId("OnExit_OnFoot",0xD36B5B15);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterStrafingId("OnEnter_Strafing",0x350E23C0);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterSwimmingId("OnEnter_Swimming",0x912B6EC2);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterCrouchedStrafingId("OnEnter_CrouchedStrafing",0xB36828F2);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterStandStrafingId("OnEnter_StandStrafing",0xFEF0052C);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterStandToCrouchId("OnEnter_StandToCrouch",0x6985F409);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterCrouchToStandId("OnEnter_CrouchToStand",0x9DF58B75);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterStrafeToOnFootId("OnEnter_StrafeToOnFoot",0x60E926C8);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterSwimToRunId("OnEnter_SwimToRun",0x24435B26);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterDiveToSwimId("OnEnter_DiveToSwim",0x885E5FE6);
const fwMvBooleanId CTaskMotionPed::ms_OnEnterParachutingId("OnEnter_Parachuting",0xCEC0D483);
const fwMvBooleanId CTaskMotionPed::ms_TransitionBreakPointId("Transition_Breakpoint",0x98E80FC1);
const fwMvBooleanId CTaskMotionPed::ms_TransitionClipFinishedId("TransitionAnimFinished",0x96E8AE49);
const fwMvBooleanId CTaskMotionPed::ms_ActionModeBreakoutId("ActionModeBreakout",0xAD177E30);
const fwMvBooleanId CTaskMotionPed::ms_LongerBlendId("LongerBlend",0x3b348088);
const fwMvBooleanId CTaskMotionPed::ms_NoTagSyncId("NoTagSync",0x4fe4edaa);
const fwMvBooleanId CTaskMotionPed::ms_IgnoreMoverRotationId("IgnoreMoverRotation",0x7e944e47);
const fwMvBooleanId CTaskMotionPed::ms_TransitionAnimFinishedId("TransitionAnimFinished", 0x96e8ae49);
const fwMvFlagId CTaskMotionPed::ms_IsIdleId("IsIdle",0x8988A76A);
const fwMvFlagId CTaskMotionPed::ms_IsCrouchedId("IsCrouched",0x236D194B);
const fwMvFlagId CTaskMotionPed::ms_IsStationaryId("IsStationary",0x369DDFD1);
const fwMvFlagId CTaskMotionPed::ms_SkipTransitionId("SkipTransition",0x5F61CA6F);
const fwMvNetworkId CTaskMotionPed::ms_CrouchedStrafingNetworkId("CrouchedStrafingNetwork",0x52F578F1);
const fwMvNetworkId CTaskMotionPed::ms_StandStrafingNetworkId("StandStrafingNetwork",0x73ABFA95);
const fwMvNetworkId CTaskMotionPed::ms_CrouchedAimingNetworkId("CrouchedAimingNetwork",0x86A7BA18);
const fwMvNetworkId CTaskMotionPed::ms_AimingNetworkId("AimingNetwork",0xD7543DF5);
const fwMvNetworkId CTaskMotionPed::ms_CrouchNetworkId("CrouchNetwork",0x6086ACB);
const fwMvNetworkId CTaskMotionPed::ms_StealthNetworkId("StealthNetwork",0xB6177A);
const fwMvNetworkId CTaskMotionPed::ms_ActionModeNetworkId("ActionModeNetwork",0x1FF191E0);
const fwMvNetworkId CTaskMotionPed::ms_DivingNetworkId("DivingNetwork",0x8DB1451E);
const fwMvNetworkId CTaskMotionPed::ms_InVehicleNetworkId("InVehicleNetwork",0x209C511B);
const fwMvNetworkId CTaskMotionPed::ms_OnFootNetworkId("OnFootNetwork",0xE40B89C2);
const fwMvNetworkId CTaskMotionPed::ms_StrafingNetworkId("StrafingNetwork",0x656C9058);
const fwMvNetworkId CTaskMotionPed::ms_SwimmingNetworkId("SwimmingNetwork",0xC419414C);
#if ENABLE_DRUNK
const fwMvNetworkId CTaskMotionPed::ms_DrunkNetworkId("DrunkNetwork",0xCB836744);
const fwMvRequestId CTaskMotionPed::ms_DrunkClearRequestId("DrunkClearRequest",0x39437C3E);
const fwMvFloatId CTaskMotionPed::ms_DrunkClearDurationId("DrunkClearDuration",0xE2E84C9D);
#endif // ENABLE_DRUNK
const fwMvFloatId CTaskMotionPed::ms_DiveUnderPhaseId("DiveUnderPhase",0x2FE24007);
const fwMvFloatId CTaskMotionPed::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskMotionPed::ms_VelocityId("Velocity",0x8FF7E5A7);
const fwMvFloatId CTaskMotionPed::ms_StrafeDurationOverrideId("StrafeDurationOverride",0xAECFABDE);
const fwMvFloatId CTaskMotionPed::ms_SwimToRunPhaseId("SwimToRunPhase",0xF8309FCF);
const fwMvFloatId CTaskMotionPed::ms_TransitionPhaseId("TransitionPhase",0x5DD0E7EB);
const fwMvFloatId CTaskMotionPed::ms_InVehicleRestartBlendDurationId("InVehicleRestartBlendDuration",0xCD46B96C);
const fwMvFloatId CTaskMotionPed::ms_OnFootRestartBlendDurationId("OnFootRestartBlendDuration",0xbf7be3d9);
const fwMvFloatId CTaskMotionPed::ms_AimToSwimDiveBlendDurationId("AimToSwimDiveBlendDuration", 0x3c81d670);
const fwMvNetworkId CTaskMotionPed::ms_ParachutingNetworkId("ParachutingNetwork",0xA35FEA9B);
const fwMvFlagId CTaskMotionPed::ms_HasDesiredVelocityId("HasDesiredVelocity",0x75190293);
const fwMvFlagId CTaskMotionPed::ms_WantsToAimId("WantsToAim",0xC5A1BE76);
const fwMvRequestId CTaskMotionPed::ms_RestartOnFootId("RestartOnFoot",0x3DEF0221);
const fwMvRequestId CTaskMotionPed::ms_RestartOnFootNoTagSyncId("RestartOnFootNoTagSync",0xB39ED646);
const fwMvRequestId CTaskMotionPed::ms_RestartInVehicleId("RestartInVehicle",0x2CA261C3);
const fwMvRequestId CTaskMotionPed::ms_RestartAimingId("RestartAiming",0xFC1D2D95);
const fwMvRequestId CTaskMotionPed::ms_RestartUpperBodyAimingId("RestartUpperBodyAiming",0x55784ED9);
const fwMvRequestId CTaskMotionPed::ms_RestartStrafingId("RestartStrafing",0xF0C1DC9F);
const fwMvRequestId CTaskMotionPed::ms_RestartActionModeId("RestartActionMode",0x77671746);
const fwMvRequestId CTaskMotionPed::ms_RestartStealthId("RestartStealth",0x51CC9BC);
const fwMvRequestId CTaskMotionPed::ms_IndependentMoverExpressionOnFootId("IndependentMoverExpressionOnFoot",0x3DFB07D7);
const fwMvRequestId CTaskMotionPed::ms_IndependentMoverExpressionAimingId("IndependentMoverExpressionAiming",0xC3E958B4);
const fwMvFloatId CTaskMotionPed::ms_AimingRestartTransitionDuration("AimingRestartTransitionDuration", 0xf4464cf1);
const fwMvFlagId CTaskMotionPed::ms_UseFirstPersonSwapTransition("UseFirstPersonSwapTransition", 0xE8AB4FC6);

#if __LOG_MOTIONPED_RESTART_REASONS
bool CTaskMotionPed::ms_LogRestartReasons = false;
#endif // __LOG_MOTIONPED_RESTART_REASONS

CTaskMotionPed::CTaskMotionPed()
: m_iFlags(0)
, m_forcedInitialSubState(0)
, m_isIdle(false)
, m_isUsingDiveUnderClip(false)
, m_restartCurrentStateNextFrame(false)
, m_restartAimingUpperBodyState(false)
, m_blockAimingActive(0)
, m_blockAimingIndependentMoverActive(0)
, m_blockOnFootActive(0)
, m_OnFootIndependentMover(false)
, m_fActualElapsedTimeInState(0.0f)
, m_fRestartDuration(0.0f)
, m_fBlockAimingTransitionTimer(0.0f)
, m_fBlockAimingIndependentMoverTimer(0.0f)
, m_fBlockOnFootTransitionTimer(0.0f)
, m_DiveImpactSpeed(0.0f)
, m_networkInsertDuration(0.0f)
, m_subtaskInsertDuration(0.0f)
, m_fInstantStrafePhaseSync(0.0f)
, m_fStrafeDurationOverride(0.25f)
, m_fActionModeTimeInSprint(0.0f)
, m_fBeginMoveStartPhase(0.0f)
, m_bDoAimingIndependentMover(true)
, m_fPostMovementIndependentMoverTargetHeading(0.0f)
, m_uLastOnFootRestartTime(0)
, m_fFPSFidgetIdleCountdownTime(0.0f)
, m_bPlayingIdleFidgets(false)
#if __DEV
, m_ForcedMotionStateThisFrame(false)
#endif 
#if FPS_MODE_SUPPORTED
, m_iFPSFidgetVariation(0)
, m_iFPSFidgetVariationLoading(0)
, m_fRandomizedTimeToStartFidget(0.0f)
, m_vCachedSwimVelocity(VEC3_ZERO)
, m_vCachedSwimStrafeVelocity(VEC3_ZERO)
, m_UseFPSIntroAfterAimingRestart(CLIP_SET_ID_INVALID)
, m_fFPSIntroAfterAimingRestartBlendOutTime (0.15f)
, m_bFPSPreviouslySprinting(false)
, m_bFPSPreviouslyCombatRolling(false)
, m_iDivingInitialState(-1)
, m_bCloneUsingFPAnims(false)
, m_bFPSInitialStateForCombatRollUnholsterTransition(-1)
#endif	//FPS_MODE_SUPPORTED
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_PED);
}

CTaskMotionPed::~CTaskMotionPed()
{
	ReleaseDiveUnderClip();
}

void CTaskMotionPed::TaskSetState(u32 const iState)
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s changing task state to %s, current state %s, previous state %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(iState), GetStaticStateName(GetState()), GetStaticStateName(GetPreviousState()));
	AI_LOG_STACK_TRACE(8);

#if __ASSERT
	if (iState == State_Stealth && GetPed() && !GetPed()->HasStreamedStealthModeClips())
	{
		taskAssertf(0, "Trying to set State_Stealth, when the stealth anims are not streamed in");
	}
	if (iState == State_ActionMode && GetPed() && (GetPed()->GetMovementModeClipSet() == CLIP_SET_ID_INVALID))
	{
		taskAssertf(0, "Trying to set State_ActionMode, when the action mode anims are not set!");
	}

#endif
	aiTask::SetState(iState);
}

bool CTaskMotionPed::ProcessPostMovement()
{
	CPed* pPed = GetPed();
	if (m_MoveNetworkHelper.IsNetworkActive() && pPed && !pPed->m_nDEflags.bFrozen)
	{
		CTask* pTask = GetSubTask();
		if (pTask)
		{
			switch(pTask->GetTaskType())
			{
			case CTaskTypes::TASK_HUMAN_LOCOMOTION: 
				switch(pTask->GetState())
				{
				case CTaskHumanLocomotion::State_Idle:
					{
						m_isIdle = 1;
					}
					break;
				case CTaskHumanLocomotion::State_Initial:
					{
						//do nothing. it's too early to tell whats going on here
					}
					break;
				default:
					{
						m_isIdle = 0;
					}
					break;
				}
				break;
			case CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION: 
				switch(pTask->GetState())
				{
				case CTaskMotionBasicLocomotion::State_Idle:
					{
						m_isIdle = 1;
					}
					break;
				case CTaskMotionBasicLocomotion::State_Initial:
					{
						//do nothing. it's too early to tell whats going on here
					}
					break;
				default:
					{
						m_isIdle = 0;
					}
					break;
				}
				break;
			default:
				break;
			}
		}

		// Force not idle flag to skip idle crouch/stand transitions when in cover
		if (pPed->GetIsInCover())
		{
			m_isIdle = 0;
		}

		m_MoveNetworkHelper.SetFlag(IsIdle(), ms_IsIdleId);
	}

	// Should do independent mover?
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover))
	{
		const float fCurrentHeading = pPed->GetCurrentHeading();
		float fDesiredHeading;
		if(m_fPostMovementIndependentMoverTargetHeading == FLT_MAX)
		{
			fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
		}
		else
		{
			fDesiredHeading = m_fPostMovementIndependentMoverTargetHeading;
		}

		const float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

		fwMvRequestId requestId;
		if(m_OnFootIndependentMover)
		{
			requestId = ms_IndependentMoverExpressionOnFootId;
		}
		else
		{
			requestId = ms_IndependentMoverExpressionAimingId;
		}

		SetIndependentMoverFrame(fHeadingDelta, requestId, true);
	}

	return false;
}

#if __DEV 
void CTaskMotionPed::SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId)
{
	animAssert(clipSetId != CLIP_SET_ID_INVALID); 
	ASSERT_ONLY(CTaskHumanLocomotion::VerifyMovementClipSet(clipSetId, 0, CLIP_SET_ID_INVALID, GetEntity() ? GetPed() : NULL));
	CTaskMotionBase::SetDefaultOnFootClipSet(clipSetId);
}
#endif

// Need a way to properly query the locomotion clips here
void CTaskMotionPed::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	if(GetSubTask())
	{
		taskAssert(dynamic_cast<CTaskMotionBase*>(GetSubTask()));
		static_cast<CTaskMotionBase*>(GetSubTask())->GetMoveSpeeds(speeds);
	}
	else
	{
		speeds.SetWalkSpeed(0.0f);
		speeds.SetRunSpeed(0.0f);
		speeds.SetSprintSpeed(0.0f);
	}
}

//*************************************************************************************************************
// Use the animated velocity here directly
Vec3V_Out CTaskMotionPed::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{
	if (IsTaskFlagSet(CTaskMotionPed::PMF_UseMotionBaseCalcDesiredVelocity))	
	{
		ClearTaskFlag(CTaskMotionPed::PMF_UseMotionBaseCalcDesiredVelocity);
		return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrixIn, fTimestep);
	}

	const Matrix34& updatedPedMatrix = RCC_MATRIX34(updatedPedMatrixIn);

	CPed* pPed = GetPed();

	Vector3	velocity(VEC3_ZERO);

	switch( GetState() )
	{
	case State_AnimatedVelocity:
		{
			const Vector3 vecExtractelVel(pPed->GetAnimatedVelocity());

			velocity += vecExtractelVel.y * updatedPedMatrix.b;
			velocity += vecExtractelVel.x * updatedPedMatrix.a;
			if(pPed->GetUseExtractedZ())
				velocity += vecExtractelVel.z * updatedPedMatrix.c;
				
			//Account for the ground.
			velocity += VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());

			Assertf(velocity.Mag2() < DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, 
				"Animated velocity was invalid.  Extracted Velocity:  (%f, %f, %f), GroundVelocityIntegrated Magnitude:  (%f), Timestep:  (%f).",
				vecExtractelVel.x, vecExtractelVel.y, vecExtractelVel.z,
				Mag(pPed->GetGroundVelocityIntegrated()).Getf(), fTimestep);

            NetworkInterface::OnDesiredVelocityCalculated(*pPed);
		}
		break;
	case State_Dead:
	case State_Start:
	case State_StrafeToOnFoot:
	case State_StandToCrouch:
	case State_CrouchToStand:
		velocity = VEC3V_TO_VECTOR3(CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrixIn, fTimestep));
		Assertf(velocity.Mag2() < DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, 
			"Start/Strafe/Crouch velocity was invalid:  (%f, %f, %f), State:  (%d), Timestep:  (%f).",
			velocity.x, velocity.y, velocity.z, GetState(), fTimestep);
		break;
	default:
		velocity = pPed->GetVelocity();
		Assertf(velocity.Mag2() < DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, "Default velocity was invalid:  (%f, %f, %f).  Timestep:  (%f).",
			velocity.x, velocity.y, velocity.z, fTimestep);
        NetworkInterface::OnDesiredVelocityCalculated(*pPed);
		break;
	}

	return VECTOR3_TO_VEC3V(velocity);
}
const crClip* CTaskMotionPed::GetBaseIdleClip()
{
#if __DEV
	if (GetState()==State_AnimatedVelocity)
	{
		return NULL;
	}
#endif //__DEV
	return CTaskMotionBase::GetBaseIdleClip();
}

//***********************************************************************************************
// This function returns whether the ped is allowed to fly through the air in its current state
bool CTaskMotionPed::CanFlyThroughAir()
{

	if (GetSubTask())
	{
		CTaskMotionBase* pSubTask = static_cast<CTaskMotionBase*>(GetSubTask());

		return pSubTask->CanFlyThroughAir();
	}
	return false;
}


//********************************************************************************************
// This function returns the default main-task-tree player control task for this motion task
CTask * CTaskMotionPed::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//*************************************************************************************
// Whether the ped should be forced to stick to the floor based upon the movement mode
bool CTaskMotionPed::ShouldStickToFloor()
{

	if (GetSubTask())
	{
		CTaskMotionBase* pSubTask = static_cast<CTaskMotionBase*>(GetSubTask());

		return pSubTask->ShouldStickToFloor();
	}

	return true;
}

//*********************************************************************************
// Returns whether the ped is currently moving based upon internal movement forces
bool CTaskMotionPed::IsInMotion(const CPed * pPed) const
{
	const CTask* pSubTask = GetSubTask();

	if (pSubTask)
	{
		return static_cast<const CTaskMotionBase*>(pSubTask)->IsInMotion(pPed);
	}

	return false;
}

bool CTaskMotionPed::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	switch (state)
	{
	case CPedMotionStates::MotionState_DoNothing:
		return GetState()==State_DoNothing;
	case CPedMotionStates::MotionState_AnimatedVelocity:
		return GetState()==State_AnimatedVelocity;
	case CPedMotionStates::MotionState_Dead:
		return GetState()==State_Dead;
	case CPedMotionStates::MotionState_Idle:
	case CPedMotionStates::MotionState_Walk:
	case CPedMotionStates::MotionState_Run:
	case CPedMotionStates::MotionState_Sprint:
		if(GetState()!=State_OnFoot)
			return false;
		break;
	case CPedMotionStates::MotionState_ActionMode_Idle:
	case CPedMotionStates::MotionState_ActionMode_Walk:
	case CPedMotionStates::MotionState_ActionMode_Run:
		if(GetState()!=State_ActionMode)
			return false;
		break;
	case CPedMotionStates::MotionState_Stealth_Idle:
	case CPedMotionStates::MotionState_Stealth_Walk:
	case CPedMotionStates::MotionState_Stealth_Run:
		if(GetState()!=State_Stealth)
			return false;
		break;
	case CPedMotionStates::MotionState_Crouch_Idle:
	case CPedMotionStates::MotionState_Crouch_Walk:
	case CPedMotionStates::MotionState_Crouch_Run:
		if(GetState()!=State_Crouch)
			return false;
		break;
	default:
		break;
	};
	
	CTaskMotionBase* childTask = (CTaskMotionBase*)GetSubTask();
	if (childTask)
	{
		return childTask->IsInMotionState(state);
	}

	return false;
}

#if __LOG_MOTIONPED_RESTART_REASONS
void CTaskMotionPed::SetRestartCurrentStateThisFrame(bool bRestart)
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s restarting current state %s - %s, is in target state %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), AILogging::GetBooleanAsString(bRestart), GetStaticStateName(GetState()), AILogging::GetBooleanAsString(IsMoveNetworkHelperInTargetState()) );

	if(ms_LogRestartReasons && bRestart)
	{
		taskDisplayf("LOG_MOTIONPED_RESTART_REASONS********************************************");
		taskDisplayf("%d: this:0x%p, Clone:%d", fwTimer::GetTimeInMilliseconds(), this, GetPed()->IsNetworkClone());

		static const s32 STACKENTRIES = 32;
		size_t trace[STACKENTRIES];
		sysStack::CaptureStackTrace(trace, STACKENTRIES, 1);
		sysStack::OpenSymbolFile();

		s32 entries = STACKENTRIES;
		for (const size_t *tp=trace; *tp && entries--; tp++)
		{
			char symname[256] = "unknown";
			sysStack::ParseMapFileSymbol(symname, sizeof(symname), *tp);
			taskDisplayf("%s", symname);
		}
		sysStack::CloseSymbolFile();

		taskDisplayf("LOG_MOTIONPED_RESTART_REASONS********************************************");
	}

	m_restartCurrentStateNextFrame = bRestart;
}
#endif // __LOG_MOTIONPED_RESTART_REASONS

void CTaskMotionPed::StealthClipSetChanged(const fwMvClipSetId &clipSetId)
{
	if (IsOnFoot() && !IsStrafing() && GetSubTask())
	{
		if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
		{
			CTaskHumanLocomotion* pTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());
			pTask->SetMovementClipSet(clipSetId);

			// flag the tag to restart the current state in the next update
			SetRestartCurrentStateThisFrame(true);
		}
		else
		{
			taskAssert( GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION );
			CTaskMotionBasicLocomotion* pTask = static_cast<CTaskMotionBasicLocomotion*>(GetSubTask());

			taskAssert(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId));
			pTask->SetMovementClipSet(clipSetId);

			// flag the tag to restart the current state in the next update
			SetRestartCurrentStateThisFrame(true);
		}
	}
}

void CTaskMotionPed::InVehicleContextChanged(u32 UNUSED_PARAM(inVehicleContextHash))
{
	if (IsInVehicle() && GetSubTask())
	{
		if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_VEHICLE)
		{
			// flag the tag to restart the current state in the next update
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fInVehicleChangeClipSetBlendDuration, 0.4f, INSTANT_BLEND_DURATION, REALLY_SLOW_BLEND_DURATION, 0.001f);
			m_subtaskInsertDuration = fInVehicleChangeClipSetBlendDuration;
			SetRestartCurrentStateThisFrame(true);
		}
	}
}

void CTaskMotionPed::OverrideWeaponClipSetCleared()
{
	if (GetSubTask()&& GetSubTask()->GetTaskType()!=CTaskTypes::TASK_HUMAN_LOCOMOTION)
	{
		if (IsOnFoot() && !IsStrafing() && GetSubTask())
		{
			// flag the tag to restart the current state in the next update
			SetRestartCurrentStateThisFrame(true);
		}
	}
}

void CTaskMotionPed::AlternateClipChanged(AlternateClipType type)
{
	if (GetSubTask()) 
	{	
		if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION)
		{
			CTaskMotionBasicLocomotion* pTask = static_cast<CTaskMotionBasicLocomotion*>(GetSubTask());

			if (m_alternateClips[type].IsValid())
			{
				pTask->StartAlternateClip(type, m_alternateClips[type], GetAlternateClipLooping(type));
			}
			else
			{
				pTask->EndAlternateClip(type, m_alternateClips[type].fBlendDuration);
			}
		}
		else if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION)
 		{
 			CTaskHumanLocomotion* pTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());
	 
 			if (m_alternateClips[type].IsValid())
 			{
 				pTask->StartAlternateClip(type, m_alternateClips[type], GetAlternateClipLooping(type));
 			}
 			else
 			{
 				pTask->EndAlternateClip(type, m_alternateClips[type].fBlendDuration);
 			}
 		}
		else if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_DIVING)
		{
			CTaskMotionDiving* pTask = static_cast<CTaskMotionDiving*>(GetSubTask());

			if (m_alternateClips[type].IsValid())
			{
				pTask->StartAlternateClip(type, m_alternateClips[type], GetAlternateClipLooping(type));
			}
			else
			{
				pTask->EndAlternateClip(type, m_alternateClips[type].fBlendDuration);
			}
		}
	}
}

bool CTaskMotionPed::ProcessMoveSignals()
{
#if __BANK
	if (GetPed()->IsNetworkClone() && m_MoveNetworkHelper.IsNetworkActive())
	{
		if (m_MoveNetworkHelper.GetBoolean(ms_TransitionAnimFinishedId))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] %s Ped %s Hit 'TransitionAnimFinished' MoVE event\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		}
		if (m_MoveNetworkHelper.GetBoolean(ms_OnEnterStandStrafingId))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] %s Ped %s Hit 'OnEnter_StandStrafing' MoVE event\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		}
		if (m_MoveNetworkHelper.GetBoolean(ms_OnEnterCrouchedStrafingId))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] %s Ped %s Hit 'OnEnter_CrouchedStrafing' MoVE event\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		}
		if (m_MoveNetworkHelper.GetBoolean(CTaskMotionAiming::ms_OnEnterAimingTransitionId))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] %s Ped %s Hit 'OnEnter_AimingTransition' MoVE event\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		}
		if (m_MoveNetworkHelper.GetBoolean(ms_OnEnterOnFootId))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] %s Ped %s Hit 'OnEnter_OnFoot' MoVE event\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		}
	}
#endif

	switch(GetState())
	{
		case State_Aiming:
			if( !m_isFullyAiming )
			{
				if( m_MoveNetworkHelper.GetBoolean(ms_OnExitOnFootId) || 
					m_MoveNetworkHelper.GetBoolean(ms_OnExitActionModeId) || 
					m_MoveNetworkHelper.GetBoolean(ms_OnExitStealthId) ||
					m_MoveNetworkHelper.GetBoolean(ms_OnExitCrouchId) )
				{
					m_isFullyAiming = true;
				}
				else // still waiting for boolean signal
				{
					return true;
				}
			}
			break;
		case State_InVehicle:
			return InVehicle_OnProcessMoveSignals();
		default:
			break;
	}
	return false;
}

#if ENABLE_DRUNK
void CTaskMotionPed::InsertDrunkNetwork(CMoveNetworkHelper& helper)
{
	taskAssert(helper.IsNetworkActive());
	taskAssert(GetState()==State_Drunk);

	CPed* pPed = GetPed();

	if (m_MoveNetworkHelper.GetNetworkPlayer()->HasExpired() && pPed)
	{
		// restart the move network and force to the drunk state
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionPed );
		pPed->GetMovePed().SetMotionTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f);
		ForceStateChange(ms_DrunkId);
	}
	
	m_MoveNetworkHelper.SetSubNetwork(helper.GetNetworkPlayer(), ms_DrunkNetworkId);
}

void CTaskMotionPed::ClearDrunkNetwork(float duration)
{
	if (GetState()==State_Drunk && GetPed() && GetPed()->IsDrunk())
	{
		m_MoveNetworkHelper.SetFloat(ms_DrunkClearDurationId, duration);
		m_MoveNetworkHelper.SendRequest(ms_DrunkClearRequestId);
	}
}
#endif // ENABLE_DRUNK

CTask::FSM_Return CTaskMotionPed::ProcessPreFSM()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		ProcessFPSIdleFidgets();
	}

	bool bTaskNetworkBlended = pPed->GetMovePed().IsTaskNetworkFullyBlendedOut() || pPed->GetMovePed().IsTaskNetworkFullyBlended();

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup) && (bTaskNetworkBlended || !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup, false);
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult) && (bTaskNetworkBlended || !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult, false);
	}

	if(m_MoveNetworkHelper.IsNetworkActive() && (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition)))
	{
		m_MoveNetworkHelper.SetFlag(false, ms_UseFirstPersonSwapTransition);
	}
#endif

	if(GetOverrideWeaponClipSet() != CLIP_SET_ID_INVALID)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableActionMode, true);
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	// Decrement the block timer
	if(m_blockAimingActive)
	{
		if(m_blockAimingActive == 2)
			m_blockAimingActive = 1;
		else
		{
			m_fBlockAimingTransitionTimer -= GetTimeStep();
		}

		if(m_fBlockAimingTransitionTimer <= 0.0f)
		{
			m_blockAimingActive = 0;
		}
	}

	if(m_blockAimingIndependentMoverActive)
	{
		if(m_blockAimingIndependentMoverActive == 2)
			m_blockAimingIndependentMoverActive = 1;
		else
		{
			m_fBlockAimingIndependentMoverTimer -= GetTimeStep();
		}

		if(m_fBlockAimingIndependentMoverTimer <= 0.0f)
		{
			m_blockAimingIndependentMoverActive = 0;
		}
	}

	if(m_blockOnFootActive)
	{
		if(m_blockOnFootActive == 2)
			m_blockOnFootActive = 1;
		else
		{
			m_fBlockOnFootTransitionTimer -= GetTimeStep();
		}

		if(m_fBlockOnFootTransitionTimer <= 0.0f)
		{
			m_blockOnFootActive = 0;
		}
	}

#if __DEV
	m_ForcedMotionStateThisFrame = false;
#endif

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionPed::ProcessPostFSM()
{
	m_forcedInitialSubState = 0;

	CPed* pPed = GetPed();

	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		// Pass the network a signal to tell it if the ped is trying to move (useful for varying transitions)
		float mbrX = 0.0f;
		float mbrY = 0.0f;
	
		pPed->GetMotionData()->GetDesiredMoveBlendRatio(mbrX, mbrY);
	
		if (!IsNearZero(abs(mbrX)) || !IsNearZero(abs(mbrY)))
		{
			m_MoveNetworkHelper.SetFlag(true, ms_HasDesiredVelocityId);
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(false, ms_HasDesiredVelocityId);
		}

		if (pPed->IsLocalPlayer())
		{
			m_MoveNetworkHelper.SetFlag(pPed->GetPlayerInfo()->IsAiming(), ms_WantsToAimId);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionPed::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* ped = GetPed();
	m_WaitingForTargetState = false;

	bool bCloneStateChange = true;
	if(ped && ped->IsNetworkClone())
	{
		if(iEvent == OnUpdate)
		{
			if (m_requestedCloneStateChange != -1 && m_requestedCloneStateChange != GetState() && GetState() != State_Exit)
			{
				switch(GetState())
				{
				case State_StrafeToOnFoot:		
					{
						//! Go to on foot via action mode to get transition.
						if(m_requestedCloneStateChange==State_OnFoot && GetPed()->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.IsLoaded() && m_MoveNetworkHelper.IsInTargetState() && (GetPed()->GetMovementModeClipSet() != CLIP_SET_ID_INVALID) )
						{
							if(!GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
							{
								TaskSetState( State_ActionMode );
								bCloneStateChange = false;
							}
						}
					}
					break;
				default:
					break;
				}

				// Am I in a suitable state to change state?
				if (bCloneStateChange && IsClonePedNetworkStateChangeAllowed(m_requestedCloneStateChange))
				{
					if(IsClonePedMoveNetworkStateTransitionAllowed(m_requestedCloneStateChange) && m_MoveNetworkHelper.IsInTargetState())
					{
						AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s performing state change to %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(m_requestedCloneStateChange));

						TaskSetState(m_requestedCloneStateChange);
						m_requestedCloneStateChange = -1;
						return FSM_Continue;
					}
					else
					{
						bool bCanForceState = true;
						if (!m_MoveNetworkHelper.IsInTargetState() && GetPreviousState() == State_Start)
						{
							//taskAssertf(0, "Clone ped %s would have gotten out of sync forcing state to %s", AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(m_requestedCloneStateChange));
#if __BANK
							TUNE_GROUP_BOOL(CLONE_SYNC_FIX, ENABLE_FIX, true);
							if (ENABLE_FIX)
#endif // __BANK
							{
								bCanForceState = false;
							}
						}

						if (bCanForceState)
						{
							AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s forcing state change to %s is in target state ? %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(m_requestedCloneStateChange), AILogging::GetBooleanAsString(m_MoveNetworkHelper.IsInTargetState()));
							//taskAssertf(m_MoveNetworkHelper.IsInTargetState(), "%s Ped %s trying to force motion state to state %s without being in the correct target state", AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName((m_requestedCloneStateChange)));

							switch(m_requestedCloneStateChange)
							{
							case State_OnFoot:
								ForceStateChange(ms_OnFootParentParentStateId);
								ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
								break;
							case State_Strafing:
								ForceStateChange(ms_StrafingId);
								break;
							case State_StrafeToOnFoot:
								ForceStateChange(ms_StrafeToOnFootId);
								break;
							case State_Swimming:
								ForceStateChange(ms_SwimmingId);
								break;
							case State_SwimToRun:
								ForceStateChange(ms_SwimToRunId);
								break;
							case State_DiveToSwim:
								ForceStateChange(ms_DiveToSwimId);
								break;
							case State_Diving:
								ForceStateChange(ms_DivingId);
								break;
							case State_DoNothing:
								ForceStateChange(ms_DoNothingId);
								break;
							case State_AnimatedVelocity:
								ForceStateChange(ms_AnimatedVelocityId);
								break;
							case State_Crouch:
								ForceStateChange(ms_CrouchId);
								break;
							case State_Stealth:
								ForceStateChange(ms_OnFootParentParentStateId);
								ForceSubStateChange(ms_StealthId, ms_OnFootParentStateId);
								break;
							case State_InVehicle:
								ForceStateChange(ms_InVehicleId);
								break;
							case State_Aiming:
								ForceStateChange(ms_AimingId);
								break;
							case State_StandToCrouch:
								ForceStateChange(ms_StandToCrouchId);
								break;
							case State_CrouchToStand:
								ForceStateChange(ms_CrouchToStandId);
								break;
							case State_ActionMode:
								ForceStateChange(ms_OnFootParentParentStateId);
								ForceSubStateChange(ms_ActionModeId, ms_OnFootParentStateId);
								break;
							case State_Parachuting:
								ForceStateChange(ms_ParachutingId);
								break;
							case State_Jetpack:
								ForceStateChange(ms_ParachutingId);
								break;
#if ENABLE_DRUNK
							case State_Drunk:
								ForceStateChange(ms_DrunkId);
								break;
#endif // ENABLE_DRUNK
							case State_Dead:
								ForceStateChange(ms_DeadId);
								break;
							case State_Start:
							case State_Exit:
							default:
								Assertf(0, "CTaskMotionPed::UpdateFSM : Cannot find move network request for state %s", GetStaticStateName(m_requestedCloneStateChange));
								break;
							}	

							AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s Ped %s forced state change to %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(m_requestedCloneStateChange));
							TaskSetState(m_requestedCloneStateChange);
							m_requestedCloneStateChange = -1;
							return FSM_Continue;
						}
					}
				}

				m_requestedCloneStateChange = -1;
			}
		}
	
		if (ped->IsAPlayerPed() && ped->GetIsSwimming() && ped->IsFirstPersonShooterModeEnabledForPlayer(false, true))
		{
			// mimic code that runs in TaskPlayer on the local side, so that swimming works correctly
			if (static_cast<CNetObjPlayer*>(ped->GetNetworkObject())->IsUsingSwimMotionTask())
			{
				ped->SetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask, true);
			}
			else
			{
				ped->SetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseAimingMotionTask, true);
				// Force into strafing 
				ped->SetIsStrafing(true);
			}
		}
	}

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_OnFoot)
			FSM_OnEnter
				OnFoot_OnEnter();
			FSM_OnUpdate
				return OnFoot_OnUpdate();
			FSM_OnExit
				OnFoot_OnExit();
		FSM_State(State_Crouch)
			FSM_OnEnter
				Crouch_OnEnter();
			FSM_OnUpdate
				return Crouch_OnUpdate();
			FSM_OnExit
				Crouch_OnExit();
		FSM_State(State_Stealth)
			FSM_OnEnter
				Stealth_OnEnter();
			FSM_OnUpdate
				return Stealth_OnUpdate();
		FSM_State(State_ActionMode)
			FSM_OnEnter
				ActionMode_OnEnter();
			FSM_OnUpdate
				return ActionMode_OnUpdate();
			FSM_OnExit
				ActionMode_OnExit();
		FSM_State(State_Strafing)
			FSM_OnEnter
				Strafing_OnEnter();
			FSM_OnUpdate
				return Strafing_OnUpdate();
			FSM_OnExit
				Strafing_OnExit();
		FSM_State(State_StrafeToOnFoot)
			FSM_OnEnter
				StrafingToOnFoot_OnEnter();
			FSM_OnUpdate
				return StrafingToOnFoot_OnUpdate();
			FSM_OnExit
				StrafingToOnFoot_OnExit();
		FSM_State(State_Swimming)
			FSM_OnEnter
				Swimming_OnEnter();
			FSM_OnUpdate
				return Swimming_OnUpdate();
			FSM_OnExit
				Swimming_OnExit();
		FSM_State(State_SwimToRun)
			FSM_OnEnter
				SwimToRun_OnEnter();
			FSM_OnUpdate
				return SwimToRun_OnUpdate();
		FSM_State(State_DiveToSwim)
			FSM_OnEnter
				DiveToSwim_OnEnter();
		FSM_OnUpdate
				return DiveToSwim_OnUpdate();
		FSM_State(State_Diving)
			FSM_OnEnter
				Diving_OnEnter();
			FSM_OnUpdate
				return Diving_OnUpdate();
			FSM_OnExit
				Diving_OnExit();
		FSM_State(State_DoNothing)
			FSM_OnEnter
				DoNothing_OnEnter();
			FSM_OnUpdate
				return DoNothing_OnUpdate();
			FSM_OnExit
				DoNothing_OnExit();
		FSM_State(State_AnimatedVelocity)
			FSM_OnEnter
				AnimatedVelocity_OnEnter();
			FSM_OnUpdate
				return AnimatedVelocity_OnUpdate();
			FSM_OnExit
				AnimatedVelocity_OnExit();
		FSM_State(State_InVehicle)
			FSM_OnEnter
				InVehicle_OnEnter();
			FSM_OnUpdate
				return InVehicle_OnUpdate();
			FSM_OnExit
				InVehicle_OnExit();
		FSM_State(State_Aiming)
			FSM_OnEnter
				Aiming_OnEnter();
			FSM_OnUpdate
				return Aiming_OnUpdate();
			FSM_OnExit
				Aiming_OnExit();
		FSM_State(State_StandToCrouch)
			FSM_OnEnter
				StandToCrouch_OnEnter();
			FSM_OnUpdate
				return StandToCrouch_OnUpdate();
		FSM_State(State_CrouchToStand)
			FSM_OnEnter
				CrouchToStand_OnEnter();
			FSM_OnUpdate
				return CrouchToStand_OnUpdate();
		FSM_State(State_Parachuting)
			FSM_OnEnter
				Parachuting_OnEnter();
			FSM_OnUpdate
				return Parachuting_OnUpdate();
			FSM_OnExit
				Parachuting_OnExit();	
		FSM_State(State_Jetpack)
			FSM_OnEnter
				Jetpack_OnEnter();
			FSM_OnUpdate
				return Jetpack_OnUpdate();
			FSM_OnExit
				Jetpack_OnExit();	
#if ENABLE_DRUNK
		FSM_State(State_Drunk)
			FSM_OnEnter
				Drunk_OnEnter();
			FSM_OnUpdate
				return Drunk_OnUpdate();
#endif // ENABLE_DRUNK
		FSM_State(State_Dead)
			FSM_OnEnter
				Dead_OnEnter();
			FSM_OnUpdate
				return Dead_OnUpdate();
			FSM_OnExit
				Dead_OnExit();
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
const char * CTaskMotionPed::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Start:					return "State_Start";
	case State_OnFoot:					return "State_OnFoot";
	case State_Strafing:				return "State_Strafing";
	case State_StrafeToOnFoot:			return "State_StrafeToOnFoot";
	case State_Swimming:				return "State_Swimming";
	case State_SwimToRun:				return "State_SwimToRun";
	case State_DiveToSwim:				return "State_DiveToSwim";
	case State_Diving:					return "State_Diving";
	case State_DoNothing:				return "State_DoNothing";
	case State_AnimatedVelocity:		return "State_AnimatedVelocity";
	case State_Crouch:					return "State_Crouch";
	case State_Stealth:					return "State_Stealth";
	case State_InVehicle:				return "State_InVehicle";
	case State_Aiming:					return "State_Aiming";
	case State_StandToCrouch:			return "State_StandToCrouch";
	case State_CrouchToStand:			return "State_CrouchToStand";
	case State_ActionMode:				return "State_ActionMode";
	case State_Parachuting:				return "State_Parachuting";
	case State_Jetpack:					return "State_Jetpack";
#if ENABLE_DRUNK
	case State_Drunk:					return "State_Drunk";
#endif // ENABLE_DRUNK
	case State_Dead:					return "State_Dead";
	case State_Exit:					return "State_Exit";
	default:
		Assertf(false, "CTaskMotionPed::GetStaticStateName : Invalid State %d", iState);
		return "Unknown";
	}
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////

void	CTaskMotionPed::CleanUp()
{
	CPed* pPed = GetPed();

	if(pPed)
	{
		if(m_CleanupMotionTaskNetworkOnQuit)
		{
			if (m_MoveNetworkHelper.IsNetworkActive())
			{
				GetPed()->GetMovePed().ClearMotionTaskNetwork(m_MoveNetworkHelper);
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, false);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, false);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile, false);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionPed::Start_OnUpdate()
{
	//stream assets and start the primary motion move network
	CPed* pPed = GetPed();
	if (fwAnimDirector::RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkMotionPed ))
	{
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionPed );
		pPed->GetMovePed().SetMotionTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), m_networkInsertDuration, CMovePed::MotionTask_TagSyncTransition);

		bool leaveDesiredMbr = pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMotionStateLeaveDesiredMBR) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true));

		// Is something forcing a particular motion state this frame?
		switch (GetMotionData().GetForcedMotionStateThisFrame())
		{
		case CPedMotionStates::MotionState_Idle:
			TaskSetState(State_OnFoot);
			ForceStateChange(ms_OnFootParentParentStateId);
			ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			pPed->SetIsCrouching(false);
			break;
		case CPedMotionStates::MotionState_Walk:
			TaskSetState(State_OnFoot);
			ForceStateChange(ms_OnFootParentParentStateId);
			ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
			pPed->SetIsCrouching(false);
			if (pPed->GetPlayerInfo())
			{
				// stops the ped from remembering old player control info
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
			}
			break;
		case CPedMotionStates::MotionState_Run:
			TaskSetState(State_OnFoot);
			ForceStateChange(ms_OnFootParentParentStateId);
			ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
			pPed->SetIsCrouching(false);
			break;
		case CPedMotionStates::MotionState_Sprint:
			TaskSetState(State_OnFoot);
			ForceStateChange(ms_OnFootParentParentStateId);
			ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			GetMotionData().SetCurrentMoveBlendRatio(3.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(3.0f,0.0f);
			pPed->SetIsCrouching(false);
			break;
		case CPedMotionStates::MotionState_Crouch_Idle:
			TaskSetState(State_Crouch);
			ForceStateChange(ms_CrouchId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			pPed->SetIsCrouching(true);
			break;
		case CPedMotionStates::MotionState_Crouch_Walk:
			TaskSetState(State_Crouch);
			ForceStateChange(ms_CrouchId);
			m_forcedInitialSubState = BANK_ONLY(!CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask ? CTaskMotionBasicLocomotion::State_Locomotion :) CTaskHumanLocomotion::State_Moving;
			GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
			pPed->SetIsCrouching(true);
			break;
		case CPedMotionStates::MotionState_Crouch_Run:
			TaskSetState(State_Crouch);
			ForceStateChange(ms_CrouchId);
			m_forcedInitialSubState = BANK_ONLY(!CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask ? CTaskMotionBasicLocomotion::State_Locomotion :) CTaskHumanLocomotion::State_Moving;
			GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
			pPed->SetIsCrouching(true);
			break;
		case CPedMotionStates::MotionState_AnimatedVelocity:
			TaskSetState(State_AnimatedVelocity);
			ForceStateChange(ms_AnimatedVelocityId);
			break;
		case CPedMotionStates::MotionState_DoNothing:
			TaskSetState(State_DoNothing);
			ForceStateChange(ms_DoNothingId);
			break;
		case CPedMotionStates::MotionState_InVehicle:
			TaskSetState(State_InVehicle);
			ForceStateChange(ms_InVehicleId);
			break;
		case CPedMotionStates::MotionState_Aiming:
			if(RequestAimingClipSet())
			{
				TaskSetState(State_Aiming);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);
				ForceStateChange(ms_AimingStateId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			break;
		case CPedMotionStates::MotionState_Diving_Idle:
			TaskSetState(State_Diving);
			ForceStateChange(ms_DivingId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			if (!leaveDesiredMbr)  
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			break;
		case CPedMotionStates::MotionState_Diving_Swim:
			TaskSetState(State_Diving);
			ForceStateChange(ms_DivingId);
			GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
			break;
		case CPedMotionStates::MotionState_Swimming_TreadWater:
			TaskSetState(State_Swimming);
			ForceStateChange(ms_SwimmingId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			break;
		case CPedMotionStates::MotionState_Dead:
			TaskSetState(State_Dead);
			ForceStateChange(ms_DeadId);
			break;
		case CPedMotionStates::MotionState_Stealth_Idle:
			if(pPed->HasStreamedStealthModeClips())
			{
				TaskSetState(State_Stealth);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_StealthId, ms_OnFootParentStateId);
				GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
				if (!leaveDesiredMbr) 
					GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetUsingStealth(true);
			break;
		case CPedMotionStates::MotionState_Stealth_Walk:
			if(pPed->HasStreamedStealthModeClips())
			{
				TaskSetState(State_Stealth);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_StealthId, ms_OnFootParentStateId);
				m_forcedInitialSubState = BANK_ONLY(!CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask ? CTaskMotionBasicLocomotion::State_Locomotion :) CTaskHumanLocomotion::State_Moving;
				GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
				if (!leaveDesiredMbr)  
					GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetUsingStealth(true);
			break;
		case CPedMotionStates::MotionState_Stealth_Run:
			if(pPed->HasStreamedStealthModeClips())
			{
				TaskSetState(State_Stealth);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_StealthId, ms_OnFootParentStateId);
				m_forcedInitialSubState = BANK_ONLY(!CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask ? CTaskMotionBasicLocomotion::State_Locomotion :) CTaskHumanLocomotion::State_Moving;
				GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
				if (!leaveDesiredMbr) 
					GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetUsingStealth(true);
			break;
		case CPedMotionStates::MotionState_Parachuting:
			TaskSetState(State_Parachuting);
			ForceStateChange(ms_ParachutingStateId);
			break;
		case CPedMotionStates::MotionState_Jetpack:
			TaskSetState(State_Jetpack);
			ForceStateChange(ms_ParachutingStateId);
			break;
		case CPedMotionStates::MotionState_ActionMode_Idle:
			if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
			{
				TaskSetState(State_ActionMode);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_ActionModeId, ms_OnFootParentStateId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			pPed->SetIsCrouching(false);
			break;
		case CPedMotionStates::MotionState_ActionMode_Walk:
			if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
			{
				TaskSetState(State_ActionMode);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_ActionModeId, ms_OnFootParentStateId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
			if (!leaveDesiredMbr)  
				GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
			pPed->SetIsCrouching(false);
			if(pPed->GetPlayerInfo())
			{
				// Stops the ped from remembering old player control info
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
			}
			break;
		case CPedMotionStates::MotionState_ActionMode_Run:
			if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
			{
				TaskSetState(State_ActionMode);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_ActionModeId, ms_OnFootParentStateId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
			GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
			pPed->SetIsCrouching(false);
			break;
		default:
			{
#if FPS_MODE_SUPPORTED
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && 
					ShouldStrafe() && ShouldUseAimingStrafe() && RequestAimingClipSet()) // Logic matches OnFoot_OnUpdate decision to enter aiming
				{
					TaskSetState(State_Aiming);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);
					ForceStateChange(ms_AimingStateId);
				}
				else
#endif // FPS_MODE_SUPPORTED
				{
					TaskSetState(State_OnFoot);
				}
			}
			break;
		}

		if( pPed->GetPedResetFlag(CPED_RESET_FLAG_SpawnedThisFrameByAmbientPopulation) )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame, true);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextTaskUpdate);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextAnimUpdate);
		}
		else if( pPed->PopTypeIsMission() || pPed->IsPlayer() )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// On foot behaviour

void CTaskMotionPed::OnFoot_OnEnter()
{
	CPed* pPed = GetPed();

	// Create the subtask
	CTaskHumanLocomotion* pTask = CreateOnFootTask(GetDefaultOnFootClipSet());

	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s ped %s sending OnFoot Mode request in PedMotion network\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
	m_MoveNetworkHelper.SendRequest(ms_OnFootId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterOnFootId, ms_OnFootNetworkId);

	// if we have cached alternate clips, apply them here.
	for(int i = 0; i < ACT_MAX; ++i)
	{
 		if(m_alternateClips[i].IsValid())
 		{
			AlternateClipType alternateType = static_cast<AlternateClipType>(i);
 			pTask->StartAlternateClip(alternateType, m_alternateClips[i], GetAlternateClipLooping(alternateType));
 		}
	}

	// If we have just come from State_Aiming, set the idle intro transition anim unless we are bringing up the phone
	if((GetPreviousState() == State_Aiming) && !CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed))
	{
		static const fwMvClipId s_TransitionToIdleIntro("stand_outro",0x437AF285);
		const fwMvClipSetId strafeClipSetId = GetDefaultAimingStrafingClipSet(pPed->GetMotionData()->GetIsCrouching());
		if(fwAnimManager::GetClipIfExistsBySetId(strafeClipSetId, s_TransitionToIdleIntro))
		{
			pTask->SetTransitionToIdleIntro(GetPed(), strafeClipSetId, s_TransitionToIdleIntro);
		}

		if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
		{
			SetBlockAimingTransitionTime(AIMING_TRANSITION_DURATION);
		}

		if(pPed->GetVelocity().Mag2() < 0.25f)
		{
			m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
		}
	}
	else if(GetPreviousState() == State_ActionMode || GetPreviousState() == State_Stealth)
	{
		static const fwMvClipId s_Action2IdleIntro("Action2Idle",0x6B0D291D);
		static const fwMvClipId s_StealthToIdleIntro("Stealth2Idle",0x9DBC9C57);

		fwMvClipId TransitionToIdleIntro;
		float fIdleTransitionBlendOutTime;
		if(GetPreviousState() == State_ActionMode)
		{
			TransitionToIdleIntro = s_Action2IdleIntro;
			fIdleTransitionBlendOutTime = CTaskHumanLocomotion::ms_Tunables.IdleTransitionBlendTimeFromActionMode;
		}
		else
		{
			TransitionToIdleIntro = s_StealthToIdleIntro;
			fIdleTransitionBlendOutTime = CTaskHumanLocomotion::ms_Tunables.IdleTransitionBlendTimeFromStealth;
		}

		const fwMvClipSetId movementModeClipSetId = pPed->GetMovementModeIdleTransitionClipSet();
		if(fwAnimManager::GetClipIfExistsBySetId(movementModeClipSetId, TransitionToIdleIntro))
		{
			bool bSuppressWeaponHolding = true;
			const CWeaponInfo* pWeaponInfo = GetWeaponInfo(pPed);
			if(pWeaponInfo)
			{
				if (pWeaponInfo->GetIsThrownWeapon() || (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsMelee2Handed()))
					bSuppressWeaponHolding = false;
			}

			pTask->SetTransitionToIdleIntro(GetPed(), 
				movementModeClipSetId, 
				TransitionToIdleIntro, 
				bSuppressWeaponHolding,
				fIdleTransitionBlendOutTime);
			pTask->SetSkipIdleIntroFromTransition(true);
		}
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		m_bFPSPreviouslySprinting = false;
	}
#endif

	aiDebugf1("Frame : %i, ped %s (%p) starting on foot state", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed());
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMotionPed::OnFoot_OnUpdate()
{
	CPed* pPed = GetPed();

	taskAssert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION);
	CTaskHumanLocomotion* pChildTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());

	// Speed
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, pPed->GetMotionData()->GetCurrentMbrY());	
	m_MoveNetworkHelper.SetFloat(ms_VelocityId, pPed->GetVelocity().Mag());

	// Make sure the correct weapon clip clipSetId is applied to the ped
	if(pChildTask)
	{
		ApplyWeaponClipSet(pChildTask);
	}

	//early out if move hasn't caught up yet
	if(!IsMoveNetworkHelperInTargetState())
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s ped %s not in move target state %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), GetStaticStateName(GetState()));

#if !__NO_OUTPUT
		fwSceneUpdateExtension* pExtension = pPed->GetExtension<fwSceneUpdateExtension>();
		aiDebugf1("Frame : %i, ped %s (%p) not in on foot state, in ragdoll %d, clone %d, frozen %d, scene update ext flags %d", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed(), pPed->GetUsingRagdoll(), pPed->IsNetworkClone(), CGameWorld::IsEntityFrozen(pPed), pExtension ? pExtension->m_sceneUpdateFlags : -1);
#endif // !__NO_OUTPUT

		if (!(GetTimeInState() < 2.0f || pPed->GetUsingRagdoll()))
		{
			NOTFINAL_ONLY(aiDebugf1("Frame : %i, ped %s (%p) stuck in on foot state in incorrect move state (previous ai state %s), restarting, see log for further details", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed(), GetStaticStateName(GetPreviousState())));
			TaskSetState(State_Start);
		}
		return FSM_Continue;
	}

	if(DoAimingTransitionIndependentMover(pChildTask))
	{
		return FSM_Continue;
	}

	if(m_restartCurrentStateNextFrame)
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		SetRestartCurrentStateThisFrame(false);
		m_MoveNetworkHelper.SendRequest(ms_RestartOnFootId);
		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
		if(pPed->GetMotionData() && pPed->GetMotionData()->GetIsDesiredSprinting(pPed->GetMotionData()->GetFPSCheckRunInsteadOfSprint()))
		{
			m_bFPSPreviouslySprinting = true;
		}
	}
	if (pPed->IsNetworkClone() && pPed->IsAPlayerPed())
	{
		ProcessCloneFPSIdle(pPed);
	}
#endif	//FPS_MODE_SUPPORTED

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		return FSM_Continue;
	}
#if ENABLE_DRUNK
	else if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
#endif // ENABLE_DRUNK
	else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
	{
		TaskSetState(State_InVehicle);
		return FSM_Continue;
	}
	else if(pPed->GetIsSwimming() || GetMotionData().GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Diving_Idle || m_DiveImpactSpeed != 0.0f)
	{
		// In FPS Mode: Go to Aiming state to strafe underwater
		if (bFPSMode)
		{
			// Reset m_DiveImpactSpeed here, otherwise it wont get reset
			m_DiveImpactSpeed = 0.f;
			TaskSetState(State_Aiming);
			return FSM_Continue;
		}

		if(m_DiveImpactSpeed != 0.0f) // Jumped into water
		{
			// If swimming clipset is streamed in, play a special animation
			fwMvClipSetId swimmingClipSetId = GetDefaultDivingClipSet();										
			fwClipSet *clipSet = fwClipSetManager::GetClipSet(swimmingClipSetId);
			if (clipSet!=NULL && clipSet->IsStreamedIn_DEPRECATED())
			{
				ForceStateChange(ms_DivingFromFallId);
			}
			else
			{
				ForceStateChange(ms_DivingId);
			}
			TaskSetState(State_Diving);
		}
		else
		{
			if(CheckForDiving())
			{
				TaskSetState(State_Diving);
				ForceStateChange(ms_DivingId);
			}
			else
            {
                if(!pPed->IsNetworkClone() || NetworkInterface::CanSwitchToSwimmingMotion(pPed))
                {
				    TaskSetState(State_Swimming);
                }
            }
		}
		return FSM_Continue;
	}
	else if(IsParachuting())
	{
		TaskSetState(State_Parachuting);
		return FSM_Continue;
	}
	else if(IsUsingJetpack())
	{
		TaskSetState(State_Jetpack);
		return FSM_Continue;
	}
	else if(ShouldStrafe())
	{
		if(ShouldUseAimingStrafe())
		{
			if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
			{
				bool bFPSProjectileWaitToExitSprint = false;

#if FPS_MODE_SUPPORTED
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSPreviouslySprinting)
				{
					const CWeaponInfo *pWeaponInfo = GetWeaponInfo(pPed);
					if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
					{
						CTaskAimAndThrowProjectile *pProjectileTask = static_cast<CTaskAimAndThrowProjectile*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE));
						TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fSprintBlendOutDelay, 0.20f, 0.0f, 1.0f, 0.01f);

						if(pProjectileTask && pProjectileTask->GetTimeRunning() > (fSprintBlendOutDelay))
							bFPSProjectileWaitToExitSprint = false;
						else
							bFPSProjectileWaitToExitSprint = true;
					}
				}
#endif
				if(bFPSProjectileWaitToExitSprint)
				{
					return FSM_Continue;
				}

				if(IsAimingIndependentMoverNotBlocked())
				{
					float fCurrentHeading = pPed->GetCurrentHeading();
					float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
					float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
					SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
					m_fPostMovementIndependentMoverTargetHeading = FLT_MAX;
					m_OnFootIndependentMover = true;
				}
				TaskSetState(State_Aiming);

                NetworkInterface::ForceMotionStateUpdate(*pPed);
			}
			return FSM_Continue;
		}
		else if (!pPed->IsNetworkClone())
		{
			TaskSetState(State_Strafing);
			return FSM_Continue;
		}
	}

	if(IsOnFootTransitionNotBlocked())
	{
		// Are we crouching
		if(GetMotionData().GetIsCrouching())
		{
			if(IsIdle() && !pPed->GetIsInCover() && CGameConfig::Get().AllowCrouchedMovement())
			{
				TaskSetState(State_StandToCrouch);
				return FSM_Continue;
			}
			else
			{
				TaskSetState(State_Crouch);
				return FSM_Continue;
			}
		}
		else if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling))
		{
			if(GetMotionData().GetUsingStealth() && pPed->CanPedStealth() && pPed->IsUsingStealthMode())
			{
				if(pPed->HasStreamedStealthModeClips())
				{
					TaskSetState(State_Stealth);
					return FSM_Continue;
				}
			}
			else if(pPed->IsUsingActionMode() && !IsInMotionState(CPedMotionStates::MotionState_Sprint))
			{
				if(m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
				{
					TaskSetState(State_ActionMode);
					return FSM_Continue;
				}
			}
			else
			{
				// Check for restarts
				static dev_u32 MIN_TIME_BETWEEN_RESTARTS = 250;
				if((m_uLastOnFootRestartTime + MIN_TIME_BETWEEN_RESTARTS) < fwTimer::GetTimeInMilliseconds())
				{
					if(pChildTask)
					{
						fwMvClipSetId defaultClipSetId = GetDefaultOnFootClipSet();
						if(defaultClipSetId != pChildTask->GetMovementClipSetId())
						{
							fwClipSet* pClipSet = fwClipSetManager::GetClipSet(defaultClipSetId);
							if(pClipSet && pClipSet->Request_DEPRECATED())
							{
								if(pChildTask->GetState() != CTaskHumanLocomotion::State_Start)
								{
									// Restart
									SetFlag(aiTaskFlags::RestartCurrentState);
									SetRestartCurrentStateThisFrame(false);

									fwMvRequestId restartRequest = ms_RestartOnFootId;
									switch(pChildTask->GetState())
									{
									case CTaskHumanLocomotion::State_Idle:
									case CTaskHumanLocomotion::State_IdleTurn:
									case CTaskHumanLocomotion::State_Stop:
										restartRequest = ms_RestartOnFootNoTagSyncId;
										break;
									default:
										break;
									}

									m_MoveNetworkHelper.SendRequest(restartRequest);
									m_MoveNetworkHelper.SetFloat(ms_OnFootRestartBlendDurationId, m_onFootBlendDuration);
									m_onFootBlendDuration = SLOW_BLEND_DURATION;
									m_uLastOnFootRestartTime = fwTimer::GetTimeInMilliseconds();
#if __LOG_MOTIONPED_RESTART_REASONS
									if(ms_LogRestartReasons)
									{
										taskDisplayf("LOG_MOTIONPED_RESTART_REASONS********************************************");
										taskDisplayf("%d: this:0x%p, Clone:%d", fwTimer::GetTimeInMilliseconds(), this, pPed->IsNetworkClone());
										taskDisplayf("%s: Old ClipSet:%s, New ClipSet:%s", GetStaticStateName(GetState()), pChildTask->GetMovementClipSetId().TryGetCStr(), defaultClipSetId.TryGetCStr());
										taskDisplayf("LOG_MOTIONPED_RESTART_REASONS********************************************");
									}
#endif // __LOG_MOTIONPED_RESTART_REASONS
									return FSM_Continue;
								}
							}
						}
					}
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskMotionPed::OnFoot_OnExit()
{
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::IsAimingOrNotBlocked() const
{
	if(GetState() == State_Aiming)
		return true;

	return IsAimingTransitionNotBlocked();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::IsAimingTransitionNotBlocked() const
{
	const CTaskMotionBase* pSubTask = static_cast<const CTaskMotionBase*>(GetSubTask());
	while(pSubTask && pSubTask->GetSubTask() && !pSubTask->ForceLeafTask())
	{
		pSubTask = static_cast<const CTaskMotionBase*>(pSubTask->GetSubTask());
	}

	if(pSubTask)
	{
		switch(pSubTask->GetTaskType())
		{
		case CTaskTypes::TASK_HUMAN_LOCOMOTION:
			{
				const CTaskHumanLocomotion* pLocoTask = static_cast<const CTaskHumanLocomotion*>(pSubTask);
				if(pLocoTask->IsTransitionBlocked())
				{
					return false;
				}

				if(pLocoTask->GetState() == State_Start && !pLocoTask->CanEarlyOutForMovement())
				{
					return false;
				}
			}
			break;
		case CTaskTypes::TASK_MOTION_AIMING:
			{
				const CTaskMotionAiming* pAimingTask = static_cast<const CTaskMotionAiming*>(pSubTask);
				if(pAimingTask->IsTransitionBlocked())
				{
					return false;
				}

// 				if(pAimingTask->GetSubTask() && pAimingTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
// 				{
// 					return false;
// 				}
			}
			break;
		default:
			break;
		}
	}

	if(m_blockAimingActive)
	{
		if(m_fBlockAimingTransitionTimer > 0.f)
		{
			return false;
		}
	}

	if(GetPed()->GetMotionData()->GetIndependentMoverTransition() == 1)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::IsAimingIndependentMoverNotBlocked() const
{
#if FPS_MODE_SUPPORTED
	if(GetPed() && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	if(m_blockAimingIndependentMoverActive)
	{
		if(m_fBlockAimingIndependentMoverTimer > 0.f)
		{
			return false;
		}
	}

	if(GetState() == State_Swimming || GetState() == State_Diving)
	{
		return false;
	}

	if (GetPed() && GetPed()->IsNetworkClone())
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::IsOnFootTransitionNotBlocked() const
{
	const CTaskMotionBase* pSubTask = static_cast<const CTaskMotionBase*>(GetSubTask());
	while(pSubTask && pSubTask->GetSubTask() && !pSubTask->ForceLeafTask())
	{
		pSubTask = static_cast<const CTaskMotionBase*>(pSubTask->GetSubTask());
	}

	if(pSubTask)
	{
		switch(pSubTask->GetTaskType())
		{
		case CTaskTypes::TASK_HUMAN_LOCOMOTION:
			{
				const CTaskHumanLocomotion* pLocoTask = static_cast<const CTaskHumanLocomotion*>(pSubTask);
				if(pLocoTask->GetState() == CTaskHumanLocomotion::State_FromStrafe)
				{
					return false;
				}
			}
			break;
		default:
			break;
		}
	}

	if(m_blockOnFootActive)
	{
		if(m_fBlockOnFootTransitionTimer > 0.f)
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

const CWeaponInfo* CTaskMotionPed::GetWeaponInfo(const CPed* pPed)
{
	if(pPed->GetWeaponManager())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			return pWeapon->GetWeaponInfo();
		}
		else
		{
			return pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionPed::ProcessFPSIdleFidgets()
{
	CPed *pPed = GetPed();

	bool bInvalidState = !camInterface::IsRenderingFirstPersonShooterCamera() ||
						 pPed->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock) || pPed->HasHurtStarted() ||
						 pPed->GetMotionData()->GetUsingStealth() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) ||
						 pPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone) ||
					     pPed->GetIsFPSSwimming() || 
						 pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged) ||
						 pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION);

	int iNumWeaponFidgets = pPed->GetEquippedWeaponInfo() ? CWeaponAnimationsManager::GetInstance().GetNumFPSFidgets(*pPed, atHashWithStringNotFinal(pPed->GetEquippedWeaponInfo()->GetHash())) : 0;

	// These flags currently take precedence in GetAppropriateWeaponClipSetId over the fidget flag, so don't bother processing
	if(bInvalidState || iNumWeaponFidgets == 0)
	{
		m_fFPSFidgetIdleCountdownTime = 0.0f;
		m_bPlayingIdleFidgets = false;
		m_fRandomizedTimeToStartFidget = 0.0f;
		m_IdleClipSetRequestHelper.Release();
		m_iFPSFidgetVariation = -1;
		return;
	}

	if(iNumWeaponFidgets > 0)
	{
		const CPedMotionData* pMotionData = pPed->GetMotionData();
		const CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();

		const bool bBlockedOnMotion = pPed->IsInMotion();
		const bool bBlockedOnFire = pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsFiring();
		const bool bBlockedOnTransition = pMotionData->GetFPSState() != pMotionData->GetPreviousFPSState();
		const bool bBlockedOnReload = pIntelligence && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN);
		const bool bBlockedOnAnim = pIntelligence && ((pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION) != NULL) || (pIntelligence->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION) != NULL));
		const bool bBlocked = bBlockedOnMotion || bBlockedOnFire || bBlockedOnTransition || bBlockedOnReload || bBlockedOnAnim || !CanSafelyPlayFidgetsInFPS(pPed);
		
		// B*2060488 - Hack to shoot right after restarting the aiming network
		if( m_bPlayingIdleFidgets && bBlockedOnFire && pPed->GetEquippedWeaponInfo()->GetIsGunOrCanBeFiredLikeGun() )
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FPSFidgetsAbortedOnFire, true);

		if(!bBlocked)
		{
			// Initialise randomised start/duration values
			if(m_fRandomizedTimeToStartFidget == 0.0f)
			{
				TUNE_GROUP_FLOAT(AIMING_DEBUG, fMinTimeToStartFidgetFPS, 20.0f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(AIMING_DEBUG, fMaxTimeToStartFidgetFPS, 30.0f, 0.0f, 30.0f, 0.01f);

				m_fRandomizedTimeToStartFidget = fwRandom::GetRandomNumberInRange(fMinTimeToStartFidgetFPS, fMaxTimeToStartFidgetFPS);
			}

			m_fFPSFidgetIdleCountdownTime += fwTimer::GetTimeStep();

			// If idle timer has surpassed time to start fidgets, set the flag
			if(m_fFPSFidgetIdleCountdownTime > m_fRandomizedTimeToStartFidget)
			{
				if(!m_bPlayingIdleFidgets)
				{
					// Stream in a new random anim
					if(m_IdleClipSetRequestHelper.IsInvalid())
					{
						if(iNumWeaponFidgets == 1)
						{
							m_iFPSFidgetVariationLoading = 0;
						}
						else
						{
							static const s32 s_iMaxRands = 3;
							s32 iNumRands = 0;
							s32 iLastFPSFidgetVariation = m_iFPSFidgetVariationLoading;
							while(iLastFPSFidgetVariation == m_iFPSFidgetVariationLoading && iNumRands < s_iMaxRands)
							{
								m_iFPSFidgetVariationLoading = fwRandom::GetRandomNumberInRange(0, iNumWeaponFidgets);			
								++iNumRands;
							}
						}

						fwMvClipSetId idleClipSetId = pPed->GetEquippedWeaponInfo()->GetFPSFidgetClipsetId(*pPed, FPS_StreamDefault, m_iFPSFidgetVariationLoading);
						if(idleClipSetId != CLIP_SET_ID_INVALID)
						{
							m_IdleClipSetRequestHelper.Request(idleClipSetId);
						}
					}

					if(m_IdleClipSetRequestHelper.Request())
					{
						m_iFPSFidgetVariation = m_iFPSFidgetVariationLoading;
						m_bPlayingIdleFidgets = true;
						pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, true);
					}					
				}
				else
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, true);
				}
			}
		}
		else
		{
			m_fFPSFidgetIdleCountdownTime = 0.0f;
			m_fRandomizedTimeToStartFidget = 0.0f;
		}

		// Check if fidget needs to be blended out
		if(m_bPlayingIdleFidgets)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, true);

			bool bFidgetFinished = false;
			bool bFidgetNonInterruptible = false;

			CTaskAimAndThrowProjectile *pProjectileTask = static_cast<CTaskAimAndThrowProjectile*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE));

			if(pProjectileTask)
			{
				bFidgetFinished = pProjectileTask->GetFPSFidgetClipFinished();
			}
			else if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
			{
				CTaskMotionAiming *pSubTask = static_cast<CTaskMotionAiming*>(GetSubTask());
				if(pSubTask)
				{
					bFidgetFinished = pSubTask->GetFPSFidgetClipFinished();
					bFidgetNonInterruptible = pSubTask->GetFPSFidgetClipNonInterruptible();
				}
			}

			if(bFidgetFinished || bBlockedOnFire || bBlockedOnTransition || bBlockedOnReload || bBlockedOnAnim)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets, false);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_BlendingOutFPSIdleFidgets, true);

				m_fFPSFidgetIdleCountdownTime = 0.0f;
				m_bPlayingIdleFidgets = false;
				m_fRandomizedTimeToStartFidget = 0.0f;
				m_IdleClipSetRequestHelper.Release();
				m_iFPSFidgetVariation = -1;
			}

		}
	}

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) != pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile, pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) );
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::CanSafelyPlayFidgetsInFPS(const CPed* pPed)
{
	// Get the dimensions of the bounding capsule
	TUNE_GROUP_FLOAT( AIMING_DEBUG, fFidgetsBoundingCapsuleLength, 0.000f, 0.0f, 20.0f, 0.1f );
	TUNE_GROUP_FLOAT( AIMING_DEBUG, fFidgetsBoundingCapsuleRadius, 0.320f, 0.0f, 20.0f, 0.1f );
	// Offsets of the bounding capsule
	TUNE_GROUP_FLOAT( AIMING_DEBUG, fBoundingCapsuleZOffset, 0.180f, -10.0f, 10.0f, 0.1f );
	TUNE_GROUP_FLOAT( AIMING_DEBUG, fBoundingCapsuleYOffset, 0.280f, -10.0f, 10.0f, 0.1f );

	// When the test is ready check the results
	if( m_FidgetsFPSProbe.GetResultsReady() )
	{
		// Get the results of the test, did we hit something?
		m_bSafeToPlayFidgetsInFps = m_FidgetsFPSProbe.GetNumHits() == 0;
		// Reset the probe state for the next try
		m_FidgetsFPSProbe.Reset();
	}
	else if( !m_FidgetsFPSProbe.GetWaitingOnResults() ) 
	{
		// Half height of the capsule
		float fCapsuleHalfHeight = (fFidgetsBoundingCapsuleLength / 2.0f) + fFidgetsBoundingCapsuleRadius;

		// Get the matrix of the ped to properly create and position the capsule
		const Mat34V pedMatrix = pPed->GetTransform().GetMatrix();
		// Get the position of the capsule (peds position) and offset it 
		Vector3 vCapsulePos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
		vCapsulePos += ( fBoundingCapsuleZOffset * VEC3V_TO_VECTOR3(pedMatrix.b()) ) + ( fBoundingCapsuleYOffset * VEC3V_TO_VECTOR3(pedMatrix.c()) );
		// Get the beginning and end points of the capsule to set its size
		Vector3 vCapsuleStart = vCapsulePos - (VEC3V_TO_VECTOR3(pedMatrix.c()) * fCapsuleHalfHeight);
		Vector3 vCapsuleEnd = vCapsulePos + (VEC3V_TO_VECTOR3(pedMatrix.c()) * fCapsuleHalfHeight);

		// We are just interested in knowing if we hit something, but
		// since we are going to use excludeEntity we need to set the
		// results structure, otherwise it doesn't work
		WorldProbe::CShapeTestCapsuleDesc shapeTestDesc;
		shapeTestDesc.SetResultsStructure(&m_FidgetsFPSProbe);
		shapeTestDesc.SetCapsule(vCapsuleStart, vCapsuleEnd, fFidgetsBoundingCapsuleRadius);
		shapeTestDesc.SetExcludeEntity(pPed);
		shapeTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_PED_INCLUDE_TYPES);
		shapeTestDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		// Only sweep queries are allowed to be asynchronous
		shapeTestDesc.SetIsDirected(true);
		// If we are close to a wall we need to detect the first hit
		// since we are doing a sweep query
		shapeTestDesc.SetDoInitialSphereCheck(true);

		// Submit the test and see if there is anything nearby
		WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}

#if DEBUG_DRAW
	// Draw the capsule to see what's going on
	TUNE_GROUP_BOOL( AIMING_DEBUG, bShowFidgetsBoundingCapsule, false );
	if(bShowFidgetsBoundingCapsule)
	{
		float fCapsuleHalfHeight = (fFidgetsBoundingCapsuleLength / 2.0f) + fFidgetsBoundingCapsuleRadius;

		const Mat34V pedMatrix = pPed->GetTransform().GetMatrix();
		Vector3 vCapsulePos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
		vCapsulePos += ( fBoundingCapsuleZOffset * VEC3V_TO_VECTOR3(pedMatrix.b()) ) + ( fBoundingCapsuleYOffset * VEC3V_TO_VECTOR3(pedMatrix.c()) );
		Vector3 vCapsuleStart = vCapsulePos - (VEC3V_TO_VECTOR3(pedMatrix.c()) * fCapsuleHalfHeight);
		Vector3 vCapsuleEnd = vCapsulePos + (VEC3V_TO_VECTOR3(pedMatrix.c()) * fCapsuleHalfHeight);

		// Change the color value depending on hits
		Color32 colour = m_bSafeToPlayFidgetsInFps ? Color_blue : Color_yellow;

		// Draw the bounding capsule
		grcDebugDraw::Capsule( VECTOR3_TO_VEC3V(vCapsuleStart), VECTOR3_TO_VEC3V(vCapsuleEnd), fFidgetsBoundingCapsuleRadius, colour, false );
	}
#endif

	// It's safe to play the fidgets if there is nothing around
	return m_bSafeToPlayFidgetsInFps;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::ProcessCloneFPSIdle(CPed* pPed)
{
	bool bInFPSIdle = false;

	if (aiVerify(pPed) && aiVerify(pPed->IsAPlayerPed()) && aiVerify(pPed->IsNetworkClone()))
	{
		if (NetworkInterface::IsRemotePlayerInFirstPersonMode(*pPed) &&
			NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(*pPed))
		{
			pPed->GetMotionData()->SetIsStrafing(false);

			CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
	
			float moveBlendRatioX, moveBlendRatioY;

			pNetObjPlayer->GetDesiredMoveBlendRatios(moveBlendRatioX, moveBlendRatioY);

			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, false );

			// determine the correct heading for the ped when running forwards
			if (moveBlendRatioX != 0.0f)
			{
				Vector3 vNormal( moveBlendRatioX, moveBlendRatioY, 0.0f );

				pPed->GetMotionData()->SetDesiredMoveBlendRatio(vNormal.Mag(), 0.0f);

				vNormal.NormalizeFast();

				float desiredHeading = pNetObjPlayer->GetDesiredHeading();

				desiredHeading += rage::Atan2f(-vNormal.x, vNormal.y);
				desiredHeading = fwAngle::LimitRadianAngle(desiredHeading);

				pPed->SetDesiredHeading(desiredHeading);
			}

			bInFPSIdle = true;
		}
	}

	return bInFPSIdle;
}

float CTaskMotionPed::SelectFPSBlendTime(CPed *pPed, bool bStateToTransition, CPedMotionData::eFPSState prevFPSStateOverride, CPedMotionData::eFPSState currFPSStateOverride)
{
	float fBlendTime = 0.0f;

	// -- State To Transition Tunables (bStateToTransition == true) -- 

	// Idle tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNG_To_RNGToIdle_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLT_To_LTToIdle_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScope_To_ScopeToIdle_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholster_To_UnholsterToIdle_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);

	// RNG tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdle_To_IdleToRNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLT_To_LTToRNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScope_To_ScopeToRNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholster_To_UnholsterToRNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);

	// LT tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdle_To_IdleToLT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNG_To_RNGToLT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScope_To_ScopeToLT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholster_To_UnholsterToLT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);

	// Scope tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdle_To_IdleToScope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNG_To_RNGToScope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLT_To_LTToScope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholster_To_UnholsterToScope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);

	// -- Transition To State Tunables (bStateToTransition == false) -- 

	// Idle tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNGToIdle_To_Idle_BlendTime, 0.28f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLTToIdle_To_Idle_BlendTime, 0.28f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScopeToIdle_To_Idle_BlendTime, 0.28f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholsterToIdle_To_Idle_BlendTime, 0.4f, 0.0f, 2.0f, 0.01f);

	// RNG tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdleToRNG_To_RNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLTToRNG_To_RNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScopeToRNG_To_RNG_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholsterToRNG_To_RNG_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	// LT tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdleToLT_To_LT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNGToLT_To_LT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fScopeToLT_To_LT_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholsterToLT_To_LT_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	// Scope tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fIdleToScope_To_Scope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fRNGToScope_To_Scope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fLTToScope_To_Scope_BlendTime, 0.12f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fUnholsterToScope_To_Scope_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	// -- End --

	// Fidget tunables
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSIdleFidgetBlendTime, 0.25f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSIdleFidgetToFireBlendTime, 0.067f, 0.0f, 2.0f, 0.01f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSStealthToggleBlendTime, 0.20f, 0.0f, 2.0f, 0.01f);

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets))
	{
		return fFPSIdleFidgetBlendTime;
	}
	else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_BlendingOutFPSIdleFidgets))
	{
		// Faster blend time when transitioning to fire
		const bool bFiring = pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsFiring();
		return bFiring ? fFPSIdleFidgetToFireBlendTime : fFPSIdleFidgetBlendTime;
	}

	int iCurrentState = (currFPSStateOverride == CPedMotionData::FPS_MAX) ? pPed->GetMotionData()->GetFPSState() : currFPSStateOverride;
	int iPrevState = (prevFPSStateOverride == CPedMotionData::FPS_MAX) ? pPed->GetMotionData()->GetPreviousFPSState() : prevFPSStateOverride;

	if(iCurrentState == CPedMotionData::FPS_IDLE && iCurrentState == iPrevState && pPed->GetMotionData()->GetToggledStealthWhileFPSIdle())
	{
		return fFPSStealthToggleBlendTime;
	}

	switch (iCurrentState)
	{
	case CPedMotionData::FPS_IDLE:
		switch (iPrevState)
		{
		case CPedMotionData::FPS_RNG:
			fBlendTime = (bStateToTransition) ? fRNG_To_RNGToIdle_BlendTime : fRNGToIdle_To_Idle_BlendTime;
			break;
		case CPedMotionData::FPS_LT:
			fBlendTime = (bStateToTransition) ? fLT_To_LTToIdle_BlendTime : fLTToIdle_To_Idle_BlendTime;
			break;
		case CPedMotionData::FPS_SCOPE:
			fBlendTime = (bStateToTransition) ? fScope_To_ScopeToIdle_BlendTime : fScopeToIdle_To_Idle_BlendTime;
			break;
		case CPedMotionData::FPS_UNHOLSTER:
			fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToIdle_BlendTime: fUnholsterToIdle_To_Idle_BlendTime;
			break;
		default:
			break;
		}
		break;
	case CPedMotionData::FPS_RNG:
		switch (iPrevState)
		{
		case CPedMotionData::FPS_IDLE:
			fBlendTime = (bStateToTransition) ? fIdle_To_IdleToRNG_BlendTime : fIdleToRNG_To_RNG_BlendTime;
			break;
		case CPedMotionData::FPS_LT:
			fBlendTime = (bStateToTransition) ? fLT_To_LTToRNG_BlendTime : fLTToRNG_To_RNG_BlendTime;
			break;
		case CPedMotionData::FPS_SCOPE:
			fBlendTime = (bStateToTransition) ? fScope_To_ScopeToRNG_BlendTime: fScopeToRNG_To_RNG_BlendTime;
			break;
		case CPedMotionData::FPS_UNHOLSTER:
			fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToRNG_BlendTime : fUnholsterToRNG_To_RNG_BlendTime;
			break;
		default:
			break;
		}
		break;
	case CPedMotionData::FPS_LT:
		switch (iPrevState)
		{
		case CPedMotionData::FPS_IDLE:
			{
				fBlendTime = (bStateToTransition) ? fIdle_To_IdleToLT_BlendTime: fIdleToLT_To_LT_BlendTime;
				const CWeaponInfo* pWeapInfo = pPed->GetEquippedWeaponInfo();
				if (pWeapInfo && pWeapInfo->GetCanBeAimedLikeGunWithoutFiring())
					fBlendTime = 0.250f; // Shameful hack for Halloween torch
				break;
			}
		case CPedMotionData::FPS_RNG:
			fBlendTime = (bStateToTransition) ? fRNG_To_RNGToLT_BlendTime : fRNGToLT_To_LT_BlendTime;
			break;
		case CPedMotionData::FPS_SCOPE:
			fBlendTime = (bStateToTransition) ? fScope_To_ScopeToLT_BlendTime : fScopeToLT_To_LT_BlendTime;
			break;
		case CPedMotionData::FPS_UNHOLSTER:
			fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToLT_BlendTime : fUnholsterToLT_To_LT_BlendTime;
			break;
		default:
			break;
		}
		break;
	case CPedMotionData::FPS_SCOPE:
		switch (iPrevState)
		{
		case CPedMotionData::FPS_IDLE:
			fBlendTime = (bStateToTransition) ? fIdle_To_IdleToScope_BlendTime : fIdleToScope_To_Scope_BlendTime;
			break;
		case CPedMotionData::FPS_RNG:
			fBlendTime = (bStateToTransition) ? fRNG_To_RNGToScope_BlendTime : fRNGToScope_To_Scope_BlendTime;
			break;
		case CPedMotionData::FPS_LT:
			fBlendTime = (bStateToTransition) ? fLT_To_LTToScope_BlendTime : fLTToScope_To_Scope_BlendTime;
			break;
		case CPedMotionData::FPS_UNHOLSTER:
			fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToScope_BlendTime : fUnholsterToScope_To_Scope_BlendTime;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return fBlendTime;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionPed::ApplyWeaponClipSet(CTaskMotionBasicLocomotion* pSubTask)
{
	animAssert(pSubTask);

	const CPed* pPed = GetPed();
	const CWeapon * pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;

	static fwMvFilterId s_filter("BothArms_filter",0x16F1D420);
	fwMvFilterId filterId = s_filter;

	if (m_overrideWeaponClipSet != CLIP_SET_ID_INVALID)
	{
		if(m_overrideWeaponClipSetFilter != FILTER_ID_INVALID)
		{
			filterId = m_overrideWeaponClipSetFilter;
		}

		pSubTask->SetWeaponClipSet(m_overrideWeaponClipSet, filterId, pPed);
	}
	else if(GetState() == State_ActionMode)
	{
		if(pPed->GetMovementModeData().m_WeaponClipFilterId != FILTER_ID_INVALID)
		{
			filterId = pPed->GetMovementModeData().m_WeaponClipFilterId;
		}

		pSubTask->SetWeaponClipSet(pPed->GetMovementModeData().m_WeaponClipSetId, filterId, pPed);
	}
	else if(pWeapon && pWeapon->GetWeaponInfo())
	{
		if(pPed->GetMotionData()->GetIsCrouching())
		{
			pSubTask->SetWeaponClipSet(pWeapon->GetWeaponInfo()->GetPedMotionCrouchClipSetId(*pPed), pWeapon->GetWeaponInfo()->GetPedMotionFilterId(*pPed), pPed);
		}
		else
		{
			pSubTask->SetWeaponClipSet(pWeapon->GetWeaponInfo()->GetPedMotionClipSetId(*pPed), pWeapon->GetWeaponInfo()->GetPedMotionFilterId(*pPed), pPed);
		}
	}
	else
	{
		pSubTask->SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID, pPed);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionPed::ApplyWeaponClipSet(CTaskHumanLocomotion* pSubTask)
{
	taskAssert(pSubTask);

	CPed* pPed = GetPed();

	const CWeaponInfo* pWeaponInfo = NULL;
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon))
	{
		// If swapping weapon, use the weapon we are swapping to
		pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	}
	else
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

		if (!pPed->IsLocalPlayer())
		{
			CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
			if (netObjPlayer && pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim() && !netObjPlayer->GetHasValidWeaponClipset())
			{
				pWeaponInfo = NULL;
			}
		}
	}

	static fwMvFilterId s_filter("BothArms_filter",0x16F1D420);
	static fwMvFilterId s_Gripfilter("Grip_R_Only_filter",0xB455BA3A);

	fwMvFilterId filterId = s_filter;

	if(m_overrideWeaponClipSet != CLIP_SET_ID_INVALID)
	{
		if(m_overrideWeaponClipSetFilter != FILTER_ID_INVALID)
		{
			filterId = m_overrideWeaponClipSetFilter;
		}

		pSubTask->SetWeaponClipSet(m_overrideWeaponClipSet, filterId, m_overrideUsesUpperBodyShadowExpression);
	}
	else if((GetState() == State_ActionMode || GetState() == State_Stealth) && !pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true))
	{
		if(pPed->GetMovementModeData().m_WeaponClipSetId != CLIP_SET_ID_INVALID)
		{
			if(pPed->GetMovementModeData().m_WeaponClipFilterId != FILTER_ID_INVALID)
			{
				filterId = pPed->GetMovementModeData().m_WeaponClipFilterId;
			}

			const CWeaponInfo* pWeaponInfo = GetWeaponInfo(pPed);
			if(pWeaponInfo)
			{
				if (pWeaponInfo->GetHash() == pSubTask->GetIdleTransitionWeaponHash() && (pWeaponInfo->GetIsThrownWeapon() || (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsMelee2Handed())))
				{
					filterId = s_Gripfilter;
				}
			}

			pSubTask->SetWeaponClipSet(pPed->GetMovementModeData().m_WeaponClipSetId, filterId, pPed->GetMovementModeData().m_UpperBodyShadowExpressionEnabled);
		}
		else
		{
			if(pWeaponInfo && pPed->GetMovementModeData().m_UseWeaponAnimsForGrip)
			{
				filterId = s_Gripfilter;
				pSubTask->SetWeaponClipSet(pWeaponInfo->GetPedMotionClipSetId(*pPed), filterId, pPed->GetMovementModeData().m_UpperBodyShadowExpressionEnabled);
			}
		}
	}
	else
	{
		if(pWeaponInfo)
		{
			if(GetState() == State_Crouch)
			{
				pSubTask->SetWeaponClipSet(pWeaponInfo->GetPedMotionCrouchClipSetId(*pPed), pWeaponInfo->GetPedMotionFilterId(*pPed));
			}
			else
			{
				if (pWeaponInfo->GetIsUnderwaterGun())
				{
					fwMvClipSetId clipsetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);//pClipSet->GetFallbackId();
					fwClipSet *pClipSet = fwClipSetManager::GetClipSet(clipsetId);
					if (pClipSet)
					{
						clipsetId = pClipSet->GetFallbackId();
						pSubTask->SetWeaponClipSet(clipsetId, pWeaponInfo->GetPedMotionFilterId(*pPed));
					}
				}
				else
				{
					pSubTask->SetWeaponClipSet(pWeaponInfo->GetPedMotionClipSetId(*pPed), pWeaponInfo->GetPedMotionFilterId(*pPed));
				}
			}
		}
		else
		{			
			pSubTask->SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID);
		}

		if (pPed->IsLocalPlayer() && pPed->GetNetworkObject())
		{
			CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
			if (netObjPlayer)
			{
				netObjPlayer->SetHasValidWeaponClipset(pWeaponInfo ? true : false);
			}
		}
	}
}

void CTaskMotionPed::ApplyWeaponClipSet(CTaskMotionStrafing* pSubTask)
{
	taskAssert(pSubTask);

	bool bSetUpperbodyClipSet = false;
	const CPed* pPed = GetPed();
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( pWeaponInfo )
	{
		if( pWeaponInfo->GetPedMotionStrafingUpperBodyClipSetId(*pPed) != CLIP_SET_ID_INVALID )
		{
			pSubTask->SetUpperbodyClipSet( pWeaponInfo->GetPedMotionStrafingUpperBodyClipSetId(*pPed) );
			bSetUpperbodyClipSet = true;
		}
	}

	if( !bSetUpperbodyClipSet )
		pSubTask->SetUpperbodyClipSet( CLIP_SET_ID_INVALID );
}

//////////////////////////////////////////////////////////////////////////
// Generic version
void CTaskMotionPed::ApplyWeaponClipSet(CTask* pSubTask)
{
	taskAssert(pSubTask);

	CPed* pPed = GetPed();
	CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;

	fwMvClipSetId clipsetId = CLIP_SET_ID_INVALID;

	static fwMvFilterId s_filter("BothArms_filter",0x16F1D420);
	fwMvFilterId filterId = s_filter;

	bool bUpperBodyShadowExpression = false;

	if(m_overrideWeaponClipSet != CLIP_SET_ID_INVALID)
	{
		if(m_overrideWeaponClipSetFilter != FILTER_ID_INVALID)
		{
			filterId = m_overrideWeaponClipSetFilter;
		}
		clipsetId = m_overrideWeaponClipSet;
		bUpperBodyShadowExpression = m_overrideUsesUpperBodyShadowExpression;		
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = NULL;
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon))
		{
			// If swapping weapon, use the weapon we are swapping to
			pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		}
		else
		{
			pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
		}

		//Only set clipset if weapon is visible
		if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed() && pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetIsVisible())
		{
			//Set R only filter if we aren't diving and haven't got a gun equipped
			if (!pWeaponInfo->GetIsGun() && pSubTask->GetTaskType() != CTaskTypes::TASK_MOTION_DIVING)
			{
				static fwMvFilterId s_Gripfilter("Grip_R_Only_filter",0xB455BA3A);
				filterId = s_Gripfilter;
			}
			if (pWeaponInfo->GetIsUnderwaterGun() && pPed->GetIsSwimming())
			{
				if (pPed->GetIsSwimming())
				{
					clipsetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);
				}
				else
				{
					// Use the fallback clip (rifle) if we aren't in water
					clipsetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);
					fwClipSet *pClipSet = fwClipSetManager::GetClipSet(clipsetId);
					if (pClipSet)
					{
						clipsetId = pClipSet->GetFallbackId();
					}
				}
			}
			else
			{
				clipsetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);
			}
		}
		else
		{
			filterId = FILTER_ID_INVALID;			
		}
	}

	switch(pSubTask->GetTaskType())
	{
		case CTaskTypes::TASK_MOTION_DIVING:
			if (m_overrideWeaponClipSet == CLIP_SET_ID_INVALID) //disabling custom weaponClipSets while swimming B* 1552970
				static_cast<CTaskMotionDiving*>(pSubTask)->SetWeaponClipSet(clipsetId, filterId, bUpperBodyShadowExpression);
			break;
		case CTaskTypes::TASK_MOTION_SWIMMING:
			if (m_overrideWeaponClipSet == CLIP_SET_ID_INVALID)
				static_cast<CTaskMotionSwimming*>(pSubTask)->SetWeaponClipSet(clipsetId, filterId, bUpperBodyShadowExpression);
			break;
		default:
#if !__NO_OUTPUT
			taskAssertf(false, "Task type %s, not supported by CTaskMotionPed::ApplyWeaponClipSet(CTask* pSubTask)", pSubTask->GetTaskName());
#endif
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Crouching behaviour

void CTaskMotionPed::Crouch_OnEnter()
{
	taskAssertf(CGameConfig::Get().AllowCrouchedMovement(), "Crouched movement in disabled, shouldn't be getting in here");

#if __BANK
	if (CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask)
#endif // __BANK
	{
		CTaskHumanLocomotion::State initialState = CTaskHumanLocomotion::State_Idle;

		s32 lastTaskType = -1;
		bool bLastFootLeft = false;

		if (m_forcedInitialSubState)
		{
			initialState = (CTaskHumanLocomotion::State)m_forcedInitialSubState;
		}
		else if (GetSubTask())
		{
			lastTaskType = GetSubTask()->GetTaskType();
			if (lastTaskType==CTaskTypes::TASK_HUMAN_LOCOMOTION)
			{
				CTaskHumanLocomotion* pTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());
				initialState = (CTaskHumanLocomotion::State)pTask->GetState();
				bLastFootLeft = pTask->IsLastFootLeft();
			}
		}

		// start the subtask
		CTaskHumanLocomotion* pTask = rage_new CTaskHumanLocomotion( initialState,  GetCrouchClipSet(), true, lastTaskType == CTaskTypes::TASK_MOTION_AIMING, bLastFootLeft, false, false, CLIP_SET_ID_INVALID, FLT_MAX );

		// Make sure the correct weapon clip clipSetId is applied to the ped
		ApplyWeaponClipSet(pTask);

		m_MoveNetworkHelper.SendRequest(ms_CrouchId);
		pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterCrouchId, ms_CrouchNetworkId);

		SetNewTask( pTask );
	}
#if __BANK
	else
	{
		u16 initialState = CTaskMotionBasicLocomotion::State_Idle;

		if (m_forcedInitialSubState)
		{
			initialState = m_forcedInitialSubState;
		}
		else if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION)
		{
			CTaskMotionBasicLocomotion* pTask = static_cast<CTaskMotionBasicLocomotion*>(GetSubTask());
			initialState = (u16)pTask->GetState();
		}

		// start the subtask
		CTaskMotionBasicLocomotion* pTask = rage_new CTaskMotionBasicLocomotion( initialState,  GetCrouchClipSet(), true );

		m_MoveNetworkHelper.SendRequest(ms_CrouchId);
		pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterCrouchId, ms_CrouchNetworkId);

		SetNewTask( pTask );
	}
#endif // __BANK
}

CTask::FSM_Return CTaskMotionPed::Crouch_OnUpdate()
{
	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

	// handle transition logic to other states here
	CPed* pPed = GetPed();

	taskAssert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION);
	CTaskHumanLocomotion* pChildTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());
	// Make sure the correct weapon clip clipSetId is applied to the ped
	ApplyWeaponClipSet(pChildTask);

#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
	else
#endif // ENABLE_DRUNK
	if(pPed->GetIsSwimming())
	{
		TaskSetState(State_Swimming);
		return FSM_Continue;
	}
	else if(GetMotionData().GetIsStrafing())
	{
		if(CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || pPed->GetIsInCover() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading)))
		{
			if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
			{
				TaskSetState(State_Aiming);
			}
			return FSM_Continue;
		}
		else if (!pPed->IsNetworkClone())
		{
			TaskSetState(State_Strafing);
			return FSM_Continue;
		}
	}
	
	if( pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning) )
	{
		pPed->SetIsCrouching(false);
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}
	else
	{
		// are we crouching
		if (!GetMotionData().GetIsCrouching())
		{
			if (IsIdle() && !pPed->GetIsInCover() && CGameConfig::Get().AllowCrouchedMovement())
			{
				TaskSetState(State_CrouchToStand);
				return FSM_Continue;
			}
			else
			{
				TaskSetState(State_OnFoot);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskMotionPed::Crouch_OnExit()
{
	//TODO - Abort the subtask here
}

void CTaskMotionPed::Stealth_OnEnter()
{
	CPed* pPed = GetPed();
	if(taskVerifyf(pPed->HasStreamedStealthModeClips(), "Stealth anims not streamed in "))
	{
		// Create the subtask
		CTaskHumanLocomotion* pTask = CreateOnFootTask(pPed->GetMovementModeClipSet());

		m_MoveNetworkHelper.SendRequest(ms_StealthId);
		pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper,  ms_OnEnterStealthId, ms_StealthNetworkId);

		if(GetPreviousState() == State_OnFoot || GetPreviousState() == State_ActionMode)
		{
			static const fwMvClipId s_Idle2StealthIntro("Idle2Stealth",0x6EF28997);
			static const fwMvClipId s_Action2StealthIntro("Action2Stealth",0xD742EA4E);

			fwMvClipId TransitionToIdleIntro = GetPreviousState() == State_OnFoot ? s_Idle2StealthIntro : s_Action2StealthIntro;

			const fwMvClipSetId idleTransitionClipSetId = pPed->GetMovementModeIdleTransitionClipSet();
			if(fwAnimManager::GetClipIfExistsBySetId(idleTransitionClipSetId, TransitionToIdleIntro))
			{
				bool bSuppressWeaponHolding = true;
				const CWeaponInfo* pWeaponInfo = GetWeaponInfo(pPed);
				if(pWeaponInfo)
				{
					if (pWeaponInfo->GetIsThrownWeapon() || (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsMelee2Handed()))
						bSuppressWeaponHolding = false;
				}
				pTask->SetTransitionToIdleIntro(pPed,idleTransitionClipSetId, 
					TransitionToIdleIntro, 
					bSuppressWeaponHolding,
					pPed->GetMovementModeData().m_IdleTransitionBlendOutTime);
			}
		}
		else if(GetPreviousState() == State_Aiming)
		{
			if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
			{
				SetBlockAimingTransitionTime(AIMING_TRANSITION_DURATION);
			}

			if(pPed->GetVelocity().Mag2() < 0.25f)
			{
				m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
			}
		}

		SetNewTask(pTask);
	}
}

CTaskMotionPed::FSM_Return CTaskMotionPed::Stealth_OnUpdate()
{
	CPed* pPed = GetPed();

	taskAssert(!GetSubTask() || GetSubTask()->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION);
	CTaskHumanLocomotion* pChildTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());

	// Speed
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, GetPed()->GetMotionData()->GetCurrentMbrY());	
	m_MoveNetworkHelper.SetFloat(ms_VelocityId, pPed->GetVelocity().Mag());

	if (!pChildTask)
	{
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}

	// Make sure the correct weapon clip clipSetId is applied to the ped
	ApplyWeaponClipSet(pChildTask);

	if (!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	if(DoAimingTransitionIndependentMover(pChildTask))
	{
		return FSM_Continue;
	}

	//! Restart state if action mode anims change from under us.
	if(GetMotionData().GetUsingStealth() && IsOnFootTransitionNotBlocked() && pChildTask->GetMovementClipSetId() != pPed->GetMovementModeClipSet() && pPed->GetMovementModeClipSet() != CLIP_SET_ID_INVALID)
	{
		pChildTask->SetMovementClipSet(pPed->GetMovementModeClipSet());

		//! Restart state.
		SetRestartCurrentStateThisFrame(true);
	}

	if (m_restartCurrentStateNextFrame)
	{
		SetFlag( aiTaskFlags::RestartCurrentState );
		SetRestartCurrentStateThisFrame(false);
		m_MoveNetworkHelper.SendRequest( ms_RestartStealthId );
		return FSM_Continue;
	}

#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
	else 
#endif // ENABLE_DRUNK
	if(pPed->GetIsSwimming())
	{
		TaskSetState(State_Swimming);
	}
	else if(IsParachuting())
	{
		TaskSetState(State_Parachuting);
		return FSM_Continue;
	}
	else if(IsUsingJetpack())
	{
		TaskSetState(State_Jetpack);
		ForceStateChange(ms_ParachutingId);
	}
	else if (GetMotionData().GetIsStrafing())
	{
		if (CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || pPed->GetIsInCover() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading)))
		{
			if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
			{
				if(IsAimingIndependentMoverNotBlocked())
				{
					float fCurrentHeading = pPed->GetCurrentHeading();
					float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
					float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
					SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
					m_OnFootIndependentMover = true;
					m_fPostMovementIndependentMoverTargetHeading = FLT_MAX;
				}
				TaskSetState(State_Aiming);
			}
			return FSM_Continue;
		}
		else if (!pPed->IsNetworkClone())
		{
			TaskSetState(State_Strafing);
			return FSM_Continue;
		}
	}
	
	if(IsOnFootTransitionNotBlocked())
	{
		// Are we no longer using stealth?
		if (!GetMotionData().GetUsingStealth() || !pPed->CanPedStealth() || !pPed->IsUsingStealthMode())
		{
			bool bWantsToUseActionMode = pPed->WantsToUseActionMode();
			
			if (bWantsToUseActionMode)
			{
				// If we've quit stealth and are wanting to go to action mode, but holding a weapon that doesn't support action mode, allow transition to OnFoot in order to prevent getting stuck in Stealth
				const CPedModelInfo::PersonalityMovementModes* pMovementModes = pPed->GetPersonalityMovementMode();
				const u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;
				if (pMovementModes && uWeaponHash != 0)
				{
					s32 index;
					const CPedModelInfo::PersonalityMovementModes::MovementMode* pMode = pMovementModes->FindMovementMode(pPed, CPedModelInfo::PersonalityMovementModes::MM_Action, uWeaponHash, index);
					if(!pMode)
					{
						bWantsToUseActionMode = false;
					}
				}
			}

			if(!bWantsToUseActionMode || !pPed->IsStreamingActionModeClips())
			{
				if (bWantsToUseActionMode)
				{
					if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet()))
						TaskSetState(State_ActionMode);
				}
				else
				{
					TaskSetState(State_OnFoot);
				}
			}
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// action mode behaviour

void CTaskMotionPed::ActionMode_OnEnter()
{
	CPed* pPed = GetPed();

	taskAssertf(pPed->GetMovementModeClipSet() != CLIP_SET_ID_INVALID, "PreviousState %s", GetStaticStateName(GetPreviousState()));
	if(m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s ped %s requested action mode clipset, sending Action Mode request in PedMotion network\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
		m_fActionModeTimeInSprint = 0.0f;

		// Start the subtask
		CTaskHumanLocomotion* pTask = CreateOnFootTask(pPed->GetMovementModeClipSet());

		m_MoveNetworkHelper.SendRequest(ms_ActionModeId);
		pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterActionModeId, ms_ActionModeNetworkId);

		if(GetPreviousState() == State_OnFoot || GetPreviousState() == State_Stealth)
		{
			static const fwMvClipId s_Idle2ActionIntro("Idle2Action",0xEE567049);
			static const fwMvClipId s_Stealth2ActionIntro("Stealth2Action",0xE2CBAD09);

			fwMvClipId TransitionToIdleIntro = GetPreviousState() == State_OnFoot ? s_Idle2ActionIntro : s_Stealth2ActionIntro;

			const fwMvClipSetId idleTransitionClipSetId = pPed->GetMovementModeIdleTransitionClipSet();
			if(fwAnimManager::GetClipIfExistsBySetId(idleTransitionClipSetId, TransitionToIdleIntro))
			{
				bool bSuppressWeaponHolding = true;
				
				const CWeaponInfo* pWeaponInfo = NULL;
				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon))
				{
					// If swapping weapon, use the weapon we are swapping to
					pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
				}
				else
				{
					pWeaponInfo = GetWeaponInfo(pPed);
				}
				if(pWeaponInfo)
				{
					if (pWeaponInfo->GetIsThrownWeapon() || (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsMelee2Handed()))
						bSuppressWeaponHolding = false;
				}
				pTask->SetTransitionToIdleIntro(pPed,
					idleTransitionClipSetId, 
					TransitionToIdleIntro, 
					bSuppressWeaponHolding,
					pPed->GetMovementModeData().m_IdleTransitionBlendOutTime);
			}
		}
		else if(GetPreviousState() == State_Aiming)
		{
			if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
			{
				SetBlockAimingTransitionTime(AIMING_TRANSITION_DURATION);
			}

			if(pPed->GetVelocity().Mag2() < 0.25f)
			{
				m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
			}
		}

		SetNewTask(pTask);
	}
	else
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s ped %s FAILED to request action mode clipset", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
	}
}

CTaskMotionPed::FSM_Return CTaskMotionPed::ActionMode_OnUpdate()
{
	// handle transition logic to other states here
	CPed* pPed = GetPed();

	// Do we still want to use action mode?
	const bool bUsingActionMode = pPed->IsUsingActionMode() || (pPed->IsStreamingActionModeClips() && pPed->WantsToUseActionMode());

	taskAssert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION);
	CTaskHumanLocomotion* pChildTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());

	// Speed
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, GetPed()->GetMotionData()->GetCurrentMbrY());	
	m_MoveNetworkHelper.SetFloat(ms_VelocityId, pPed->GetVelocity().Mag());

	// Make sure the correct weapon clip clipSetId is applied to the ped
	if(pChildTask && bUsingActionMode)
	{
		ApplyWeaponClipSet(pChildTask);

		//! Restart state if action mode anims change from under us.
		if(IsOnFootTransitionNotBlocked() && pPed->IsUsingActionMode() && pChildTask->GetMovementClipSetId() != pPed->GetMovementModeClipSet() && pPed->GetMovementModeClipSet() != CLIP_SET_ID_INVALID)
		{
			pChildTask->SetMovementClipSet(pPed->GetMovementModeClipSet());

			//! Restart state.
			SetRestartCurrentStateThisFrame(true);
		}
	}

	if(!IsMoveNetworkHelperInTargetState())
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s ped %s not in move target state %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), GetStaticStateName(GetState()));
		return FSM_Continue;
	}

	if(DoAimingTransitionIndependentMover(pChildTask))
	{
		return FSM_Continue;
	}

	if (m_restartCurrentStateNextFrame && pPed->GetMovementModeClipSet() != CLIP_SET_ID_INVALID)
	{
		SetFlag( aiTaskFlags::RestartCurrentState );
		SetRestartCurrentStateThisFrame(false);
		m_MoveNetworkHelper.SendRequest( ms_RestartActionModeId );
		return FSM_Continue;
	}

	if(!m_ActionModeClipSetRequestHelper.IsLoaded())
	{
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}

	bool bSprintTransition = false;
	if(pChildTask->IsInMotionState(CPedMotionStates::MotionState_Sprint))
	{
		m_fActionModeTimeInSprint += GetTimeStep();
		static dev_float s_TimeToBlendIntoOnFoot = 0.5f;
		if(m_fActionModeTimeInSprint >= s_TimeToBlendIntoOnFoot)
		{
			bSprintTransition = true;
		}
	}

#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
	else 
#endif // ENABLE_DRUNK
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
	{
		TaskSetState(State_InVehicle);
		return FSM_Continue;
	}
	else if(pPed->GetIsSwimming())
	{
		TaskSetState(State_Swimming);
		return FSM_Continue;
	}
	else if(IsUsingJetpack())
	{
		TaskSetState(State_Jetpack);
		ForceStateChange(ms_ParachutingId);
	}
	else if(IsParachuting())
	{
		TaskSetState(State_Parachuting);
		return FSM_Continue;
	}
	else if (GetMotionData().GetIsStrafing())
	{
		if(CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || pPed->GetIsInCover() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading)))
		{	
			if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
			{
				if(IsAimingIndependentMoverNotBlocked())
				{
					float fCurrentHeading = pPed->GetCurrentHeading();
					float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
					float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
					SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
					m_OnFootIndependentMover = true;
					m_fPostMovementIndependentMoverTargetHeading = FLT_MAX;
				}
				TaskSetState(State_Aiming);
			}
			return FSM_Continue;
		}
		else if (!pPed->IsNetworkClone())
		{
			TaskSetState(State_Strafing);
			return FSM_Continue;
		}
	}
	
	if(IsOnFootTransitionNotBlocked())
	{
		if(bSprintTransition)
		{
			TaskSetState(State_OnFoot);
			return FSM_Continue;
		}
		else if (!GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping) || GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling))
		{
			// Are we no longer using action mode?
			if(!bUsingActionMode)
			{
				if(GetMotionData().GetUsingStealth() && pPed->CanPedStealth() && pPed->IsUsingStealthMode())
				{
					if(pPed->HasStreamedStealthModeClips())
						TaskSetState(State_Stealth);
				}
				else
				{
					TaskSetState(State_OnFoot);
				}
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskMotionPed::ActionMode_OnExit()
{
	// No longer need stealth clipset
	m_ActionModeClipSetRequestHelper.Release();
}

//////////////////////////////////////////////////////////////////////////
// strafing behaviour

void CTaskMotionPed::Strafing_OnEnter()
{
	u16 initialState = CTaskMotionStrafing::State_Initial;

	// We want to skip the on foot to strafe transition
	if( IsTaskFlagSet( CTaskMotionPed::PMF_SkipStrafeIntroAnim ) )
	{
		initialState = CTaskMotionStrafing::State_Idle;
	}
	// Otherwise lets check to see if we are already moving
	else
	{
		CPed* pPed = GetPed();
		float fCurrentMbrSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
		if( fCurrentMbrSq > MBR_WALK_BOUNDARY )
			initialState = CTaskMotionStrafing::State_Moving;
	}

	// We no longer have a use for this flag so clear it out
	ClearTaskFlag( CTaskMotionPed::PMF_SkipStrafeIntroAnim );

	// start the subtask
	CTaskMotionStrafing* pTask = rage_new CTaskMotionStrafing( initialState );
	pTask->SetInstantPhaseSync( m_fInstantStrafePhaseSync );
	m_fInstantStrafePhaseSync = 0.0f;

	m_MoveNetworkHelper.SetFloat( ms_StrafeDurationOverrideId, m_fStrafeDurationOverride );
	m_fStrafeDurationOverride = 0.25f;

	m_MoveNetworkHelper.SendRequest(ms_StrafingId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterStrafingId, ms_StrafingNetworkId);

	ApplyWeaponClipSet( pTask );

	SetNewTask( pTask );
}

CTask::FSM_Return	CTaskMotionPed::Strafing_OnUpdate()
{
	// If we have restarted, IsMoveNetworkHelperInTargetState will return true due to a bug in the anim system,
	// So prevent updating for a frame to allow MoVE to get in the right state
	if (!IsMoveNetworkHelperInTargetState() || (GetPreviousState() == State_Strafing && GetTimeInState() == 0.f))
		return FSM_Continue;

	bool bRestartSubTask = false;
	if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_STRAFING)
	{
		ApplyWeaponClipSet( static_cast<CTaskMotionStrafing*>(GetSubTask()) );
	
		bRestartSubTask = static_cast<CTaskMotionStrafing*>(GetSubTask())->CanRestartTask(GetPed());
	}

	if (m_restartCurrentStateNextFrame || bRestartSubTask)
	{
		SetFlag( aiTaskFlags::RestartCurrentState );
		SetTaskFlag( CTaskMotionPed::PMF_SkipStrafeIntroAnim );
		SetRestartCurrentStateThisFrame(false);
		m_MoveNetworkHelper.SendRequest( ms_RestartStrafingId );
		return FSM_Continue;
	}

	// handle transition logic to other states here
	CPed* pPed = GetPed();

#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
	else 
#endif // ENABLE_DRUNK
	if(pPed->GetIsSwimming())
	{
		TaskSetState(State_Swimming);
	}
	else if ( !GetMotionData().GetIsStrafing() )
	{
		// Let the owner ped tell us clones what to do...
		if(!pPed->IsNetworkClone())
		{
			if (pPed->GetMotionData()->GetIsCrouching())
			{
				TaskSetState(State_Crouch);
			}
			else if(GetMotionData().GetUsingStealth() && pPed->CanPedStealth() && pPed->IsUsingStealthMode())
			{
				if(pPed->HasStreamedStealthModeClips())
				{
					TaskSetState(State_Stealth);
					return FSM_Continue;
				}
			}
			else if( IsTaskFlagSet( PMF_PerformStrafeToOnFootTransition ) )
			{
				ClearTaskFlag( PMF_PerformStrafeToOnFootTransition );
				TaskSetState(State_StrafeToOnFoot);
			}
			else if(pPed->IsUsingActionMode() && !IsInMotionState(CPedMotionStates::MotionState_Sprint))
			{
				if(m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet()))
				{
					TaskSetState(State_ActionMode);
					return FSM_Continue;
				}
			}
			else
			{
				TaskSetState( State_OnFoot );
			}
		}
	}
	else if ( CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT)
		     FPS_MODE_SUPPORTED_ONLY( || (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE))) ))
	{
		if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
		{
			TaskSetState(State_Aiming);
		}
		return FSM_Continue;
	}
	else
	{
		// handle switching from crouch to standing
		if (GetSubTask() && pPed->GetMotionData()->GetIsCrouching()!= ((bool)pPed->GetMotionData()->GetWasCrouching()))
		{
			// the crouch toggle has been changed. Trigger a network reset in the strafing task
			// (this will automatically change the clip set to the srafing variant)
			fwMvFloatId subnetworkBlendDurationId(mvSubNetwork::ms_BlendDurationId);
			m_MoveNetworkHelper.SetFloat( subnetworkBlendDurationId, 0.5f);
			return FSM_Continue;
		}

	}
	return FSM_Continue;
}

void CTaskMotionPed::Strafing_OnExit()
{
	// Clear this flag in case we were not able to perform it
	ClearTaskFlag( PMF_PerformStrafeToOnFootTransition );
}

void CTaskMotionPed::StrafingToOnFoot_OnEnter()
{
	// Set the clip set be the peds movement clip set
	CPed* pPed = GetPed();
	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	fwMvClipSetId clipSetId = CLIP_SET_MELEE_UNARMED_BASE;

	if( pWeaponInfo )
	{
		//! Get base from weapon.
		if(pWeaponInfo->GetMeleeBaseClipSetId() != CLIP_SET_ID_INVALID)
			clipSetId = pWeaponInfo->GetMeleeBaseClipSetId();

		if(!pWeaponInfo->GetIsUnarmed() && (pWeaponInfo->GetMeleeClipSetId( *pPed ) != CLIP_SET_ID_INVALID))
		{
			fwMvClipSetId tempClipSet( pWeaponInfo->GetMeleeClipSetId( *pPed ) );
			const u32 outroHash = ATSTRINGHASH("melee_outro", 0xf4adbae5);
			s32 iDictIndex = -1;
			u32	clipId = 0;
			bool bFoundClip = CVehicleSeatAnimInfo::FindAnimInClipSet(tempClipSet, outroHash, iDictIndex, clipId );
			if(bFoundClip)
			{
				clipSetId = tempClipSet;
			}
		}
	}

	m_MoveNetworkHelper.SetClipSet( clipSetId );

	taskAssertf(m_MoveNetworkHelper.GetClipSetId() != CLIP_SET_ID_INVALID, "Choosing invalid clip for StrafingToOnFoot transition");

	m_MoveNetworkHelper.SendRequest( ms_StrafeToOnFootId );
	m_MoveNetworkHelper.WaitForTargetState( ms_OnEnterStrafeToOnFootId );
}

CTask::FSM_Return CTaskMotionPed::StrafingToOnFoot_OnUpdate() 
{
	CPed* pPed = GetPed();

	taskAssertf(m_MoveNetworkHelper.GetClipSetId() != CLIP_SET_ID_INVALID, "Choosing invalid clip for StrafingToOnFoot transition");

	// Ensure action mode enabled and streaming in
	pPed->SetUsingActionMode(true, CPed::AME_MeleeTransition, -1.0f, false);

	m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet());

	if( !IsMoveNetworkHelperInTargetState() )
	{
		return FSM_Continue;
	}

	// Set melee grip filter.
	if (pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsMeleeFist())
	{
		crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(m_MoveNetworkHelper.GetClipSetId());
		if (pFilter)
		{
			m_MoveNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
		}
	}

	if( pPed->GetMotionData()->GetIsStrafing() )
	{
		TaskSetState( State_Strafing );
		return FSM_Continue;
	}

	// Determine if the ped is attempting to go in any direction
	CControl* pControl = pPed->GetControlFromPlayer();
	if( pControl && !CControlMgr::IsDisabledControl( pControl ) )
	{
		const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
		static dev_float MIN_MOTION_BLEND_RATIO = 0.25f;
		if( vecStickInput.Mag2() > rage::square( MIN_MOTION_BLEND_RATIO ) )
		{
			if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet()))
			{
				// Only exit if action mode is available
				TaskSetState( State_ActionMode );
				return FSM_Continue;
			}
		}
	}

	// If we are trying to enter a vehicle then force the on foot state
	if( pPed->GetPedResetFlag( CPED_RESET_FLAG_IsEnteringVehicle ) )
	{
		TaskSetState( State_OnFoot );
		return FSM_Continue;
	}

	if( m_MoveNetworkHelper.GetBoolean( ms_TransitionClipFinishedId ) || m_MoveNetworkHelper.GetBoolean( ms_ActionModeBreakoutId ) )
	{
		// Ensure action mode enabled
		if(pPed->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet()))
			TaskSetState( State_ActionMode );
		else
			TaskSetState( State_OnFoot ); // Go to onfoot if we have finished and action mode isn't streamed.
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

void CTaskMotionPed::StrafingToOnFoot_OnExit()
{
	CPed* pPed = GetPed();
	if(pPed)
		pPed->SetUsingActionMode(false, CPed::AME_MeleeTransition);
}

//////////////////////////////////////////////////////////////////////////
// swimming behaviour

void	CTaskMotionPed::Swimming_OnEnter()
{
	CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
	if (pWepMgr)
	{
		const CWeaponInfo* pWeaponInfo = pWepMgr->GetEquippedWeaponInfo();
		if (!pWeaponInfo || !pWeaponInfo->GetUsableUnderwater())
		{
			CWeapon* pWeapon = pWepMgr->GetEquippedWeapon();			
			if (pWeapon && pWeapon->GetIsCooking()) //drop it
			{
				CPed* pPed = GetPed();
				float fOverrideLifeTime = -1.0f;
				if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
					fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

				const s32 nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
				Assert(nBoneIndex != BONETAG_INVALID);
				Matrix34 boneMat;
				pPed->GetGlobalMtx(nBoneIndex, boneMat);
				Vector3 v = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				CWeapon::sFireParams params(pPed, boneMat, NULL, &v);					
				params.fOverrideLifeTime = fOverrideLifeTime;			
				pWeapon->Fire(params);
			}

			pWepMgr->BackupEquippedWeapon();
			pWepMgr->DestroyEquippedWeaponObject(false);
			pWepMgr->EquipBestWeapon();
		}		
	}

	//Water physics variables
	float fPreviousVelocityChange = CTaskMotionSwimming::s_fPreviousVelocityChange;
	float fHeightBelowWater = CTaskMotionSwimming::s_fHeightBelowWater;
	float fHeightBelowWaterGoal = CTaskMotionSwimming::s_fHeightBelowWaterGoal;

	if(GetSubTask())
	{
		switch(GetSubTask()->GetTaskType())
		{
		case CTaskTypes::TASK_MOTION_SWIMMING:
			{
				//Initialize swimming parameters so we don't have a sudden jump in velocity
				const CTaskMotionSwimming* pTask = static_cast<const CTaskMotionSwimming*>(GetSubTask());

				//Locally store water physics values from swim task and pass through aim task constructor
				fPreviousVelocityChange = pTask->GetPreviousVelocityChange();
				fHeightBelowWater = pTask->GetHeightBelowWater();
				fHeightBelowWaterGoal = pTask->GetHeightBelowWaterGoal();
			}
			break;
		case CTaskTypes::TASK_MOTION_AIMING:
			{
				//Initialize swimming parameters so we don't have a sudden jump in velocity
				const CTaskMotionAiming* pTask = static_cast<const CTaskMotionAiming*>(GetSubTask());

				//Locally store water physics values from swim task and pass through aim task constructor
				fPreviousVelocityChange = pTask->GetPreviousVelocityChange();
				fHeightBelowWater = pTask->GetHeightBelowWater();
				fHeightBelowWaterGoal = pTask->GetHeightBelowWaterGoal();
			}
			break;
		default:
			break;
		}
	}

	// start the subtask
	CTaskMotionSwimming* pTask = rage_new CTaskMotionSwimming( CTaskMotionSwimming::State_Idle, true, fPreviousVelocityChange, fHeightBelowWater, fHeightBelowWaterGoal);	

	// Default blend duration to 0.25f
	m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, 0.25f);

#if FPS_MODE_SUPPORTED
	if (GetPed()->GetIsFPSSwimming())
	{
		m_vCachedSwimVelocity = VEC3_ZERO;

		// Increase blend duration
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fToSwimBlendDuration, 0.25f, 0.01f, 5.0f, 0.01f);
		m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, fToSwimBlendDuration);
	}

	// If we've swapped from FPS->TPS, do an instant blend
	if (GetPed()->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !GetPed()->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON))
	{
		m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, 0.0f);
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAnimUpdate, true);
	}
#endif	// FPS_MODE_SUPPORTED

	m_MoveNetworkHelper.SendRequest(ms_SwimmingId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterSwimmingId, ms_SwimmingNetworkId);

	SetNewTask( pTask );
	m_wantsToDiveUnderwater = false;
}

CTask::FSM_Return	CTaskMotionPed::Swimming_OnUpdate()
{
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, GetPed()->GetMotionData()->GetCurrentMbrY());	
	CPed* pPed = GetPed();

	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

#if FPS_MODE_SUPPORTED
	// Restart the state if we've changed camera mode between FPS and TPS
	CTaskMotionSwimming* pTask = static_cast<CTaskMotionSwimming*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_SWIMMING));			
	if (pTask)
	{
		fwMvClipSetId currentClipsetId = pTask->GetCurrentSwimmingClipsetId();
		fwMvClipSetId desiredClipsetId = pTask->GetSwimmingClipSetId();
		if (currentClipsetId != CLIP_SET_ID_INVALID && desiredClipsetId != CLIP_SET_ID_INVALID)
		{
			// If camera mode is different to selected clipset, restart the state/subtask
			if (currentClipsetId != desiredClipsetId)
			{
				static const fwMvRequestId s_RestartSwimmingNetwork("RestartSwimmingNetwork",0x88af161b);
				m_MoveNetworkHelper.SendRequest(s_RestartSwimmingNetwork);
				TaskSetState(State_Swimming);
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	// Go to Aiming to swim-strafe if flagged to do so
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseAimingMotionTask) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute))
	{
		TaskSetState(State_Aiming);
		return FSM_Continue;
	}
#endif	//FPS_MODE_SUPPORTED 

	// Make sure the correct weapon clip clipSetId is applied to the ped
	if(GetSubTask())
	{
		ApplyWeaponClipSet(GetSubTask());
	}

	bool isSwimming = pPed->GetIsSwimming();
	if (NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone())
	{
		isSwimming &= pPed->GetIsInWater();
	}

	if(!isSwimming)
	{
		// the clones don't run the buoyancy tests if off screen so will switch to on foot at inappropriate times.
		// cheaper to ignore results of buoyancy and just follow the network in this instance...
		if(pPed->IsNetworkClone())
		{
			Vector3 camPos				= camInterface::GetPos();
			Vector3 clonePos			= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			bool	riverProbeLaunched	= pPed->GetIsVisibleInSomeViewportThisFrame() || ((camPos - clonePos).XYMag() < (20.0f * 20.0f));

			// if we're off screen or on screen and waiting for the results of a buoyancy test...
			if(!riverProbeLaunched || !pPed->m_Buoyancy.GetWaterHelperRiverHitStored())
			{
				// owner is still swimming...
				CNetObjPed* netObjPed = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
				if(netObjPed && netObjPed->GetIsOwnerSwimming())
				{
					// just follow the owner's lead...
					return FSM_Continue;
				}
			}
		}

		// need to switch to an on foot state		
		if ( GetMotionData().GetIsStrafing() )
		{
			if (CTaskAimGun::ShouldUseAimingMotion(pPed) && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
			{
				if (RequestAimingClipSet() && IsAimingTransitionNotBlocked())
				{
					TaskSetState(State_Aiming);
				}
				return FSM_Continue;
			}
			else
			{
				TaskSetState(State_Strafing);
			}
		}
		else	// Otherwise we must be walking around on-foot
		{
			// are we still crouching?
			if (GetMotionData().GetIsCrouching())
			{
				TaskSetState(State_Crouch);
			}
			else if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER)  
					|| (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
					    pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE) == CTaskEnterVehicle::State_ClimbUp))
			{
				// For climb task (and climb when entering vehicle), we want to skip the ground transition check below
				TaskSetState(State_OnFoot);
			}
			else
			{
				// B*2001443: Ensure ped is actually on ground before transitioning to SwimToRun/OnFoot.
				if (pPed->GetGroundOffsetForPhysics() > PED_GROUNDPOS_RESET_Z || pPed->IsNetworkClone())
				{
					if ( pPed->GetMotionData()->GetCurrentMbrY()>MOVEBLENDRATIO_WALK ) 
					{
						float fWaterDepth = 0.0f;
						pPed->m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &fWaterDepth, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed, pPed->GetCanUseHighDetailWaterLevel());
						fWaterDepth -= pPed->GetGroundPos().z;					
						static dev_float s_fMinWaterDepth = 0.9f;
						if (fWaterDepth >= s_fMinWaterDepth)
						{
							fwMvClipSetId swimmingClipSetId = GetDefaultSwimmingClipSet();  																				
							if (m_SwimClipSetRequestHelper.Request(swimmingClipSetId)) 
							{
								TaskSetState(State_SwimToRun);					
								return FSM_Continue;
							}
						}					
					}
					TaskSetState(State_OnFoot);
				}
			}
		}
	}
	else
	{	
		CPed * pPed = GetPed();		

		bool bTaskSetState = false;

		//check if the ped wants to dive under
		if (m_wantsToDiveUnderwater)
		{
			// only gets set in TaskMovePlayer...
			Assert(!pPed->IsNetworkClone());

			m_wantsToDiveUnderwater = false;
			// Keep a reference to the dive under clip clipSetId, since the subtask is about to release its reference when it exits
			RefDiveUnderClip();
			TaskSetState(State_Diving);
			pPed->m_Buoyancy.SetShouldStickToWaterSurface(false);

			bTaskSetState = true;				
		}
		//check if we're suddenly really deep (maybe we got teleported, or just got out of a car and defaulted to here
		else if(CheckForDiving() && ( !pPed->IsNetworkClone() || !((CNetObjPed*)pPed->GetNetworkObject())->GetIsOwnerSwimming() ) )
		{
			TaskSetState(State_Diving);
			ForceStateChange(ms_DivingId);

			bTaskSetState = true;
		}
		// Check if we're aiming while swimming (don't need to check for weapon UsableUnderwater as we won't be able to equip it)
		if(CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming)) && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsUnderwaterGun())
		{
			if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
			{
				if(IsAimingIndependentMoverNotBlocked())
				{
					float fCurrentHeading = pPed->GetCurrentHeading();
					float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
					float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
					SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
					m_OnFootIndependentMover = true;
					m_fPostMovementIndependentMoverTargetHeading = FLT_MAX;
				}
				TaskSetState(State_Aiming);

				bTaskSetState = true;
			}
		}

		//If the water isn't deep enough on the remote then we cannot be in the water and switch to onfoot state
		if (!bTaskSetState && pPed->IsNetworkClone())
		{
			float fWaterDepth;
			pPed->m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &fWaterDepth, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed, pPed->GetCanUseHighDetailWaterLevel());
			fWaterDepth -= pPed->GetGroundPos().z;
			static dev_float s_fMinWaterDepthDelta = 0.9f;
			if (fWaterDepth<s_fMinWaterDepthDelta)
			{
				TaskSetState(State_OnFoot);
			}
		}
	}

	return FSM_Continue;
}


void	CTaskMotionPed::Swimming_OnExit()
{
	CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
	if (pWepMgr)
	{
		pWepMgr->RestoreBackupWeapon();
	}
}

void	CTaskMotionPed::SwimToRun_OnEnter()
{
	if (m_SwimClipSetRequestHelper.IsLoaded())
	{
		m_MoveNetworkHelper.SendRequest(ms_SwimToRunId);	
		m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterSwimToRunId);
	}	
	m_fActualElapsedTimeInState = 0.0f;
}

CTask::FSM_Return	CTaskMotionPed::SwimToRun_OnUpdate()
{
	if (!m_SwimClipSetRequestHelper.IsLoaded())
	{
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}

	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

	CPed* pPed = GetPed();
	float swimToRunPhase = 0;
	static dev_float MIN_BREAKOUT_PHASE = 0.25f;	
	swimToRunPhase = m_MoveNetworkHelper.GetFloat(ms_SwimToRunPhaseId);

	static dev_float BAD_TERRAIN_EARLY_ABORT_PHASE = 0.15f;
	if (swimToRunPhase <= BAD_TERRAIN_EARLY_ABORT_PHASE)
	{		
		float fWaterDepth;
		pPed->m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &fWaterDepth, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed, pPed->GetCanUseHighDetailWaterLevel());
		fWaterDepth -= pPed->GetGroundPos().z;
		//Displayf("fWaterDepth = %f swimToRunPhase = %f ", fWaterDepth, swimToRunPhase);
		static dev_float s_fMinWaterDepthDelta = 0.8f;
		if (fWaterDepth<s_fMinWaterDepthDelta)
		{
			SetState(State_OnFoot);
			return FSM_Continue;
		}
	}
	
	// B*2001443: Break back to swimming if we are submerged and been in this state for 0.1ms
	m_fActualElapsedTimeInState += fwTimer::GetTimeStep();
	static dev_float fTimeLimit = 0.1f;
	if (pPed->GetIsSwimming() && m_fActualElapsedTimeInState > fTimeLimit)
	{
		m_fActualElapsedTimeInState = 0.0f;
		SetState(State_Swimming);
		return FSM_Continue;
	}
	
	if ( pPed->GetMotionData()->GetCurrentMbrY()>MOVEBLENDRATIO_WALK ) 
	{
		float fHeadingDiff = SubtractAngleShorter(pPed->GetMotionData()->GetCurrentHeading(), pPed->GetMotionData()->GetDesiredHeading());
		static dev_float MIN_ALLOWED_HEADING_CHANGE = 0.15f;

		if(swimToRunPhase>0.6f || (swimToRunPhase > MIN_BREAKOUT_PHASE && Abs(fHeadingDiff) > MIN_ALLOWED_HEADING_CHANGE) ) //break for heading change
		{
			m_MoveNetworkHelper.SetBoolean(ms_TransitionClipFinishedId, true);	
		} 
		else 
		{
			if (!m_MoveNetworkHelper.GetBoolean(ms_TransitionClipFinishedId) ) 
			{		
				return FSM_Continue;
			}			
		}
	} 
	else 
	{
		if ( swimToRunPhase > MIN_BREAKOUT_PHASE )
			m_MoveNetworkHelper.SetBoolean(ms_TransitionClipFinishedId, true);							
		else 
			return FSM_Continue;
	}
	TaskSetState(State_OnFoot);
	return FSM_Continue;
}


void	CTaskMotionPed::DiveToSwim_OnEnter()
{
	// start the subtask
	CTaskMotionSwimming* pTask = rage_new CTaskMotionSwimming( CTaskMotionSwimming::State_Idle);	
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterSwimmingId, ms_SwimmingNetworkId);
	SetNewTask( pTask );

	m_MoveNetworkHelper.SendRequest(ms_DiveToSwimId);	
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterDiveToSwimId);

	//lock stick heading for player surfacing by pressing down
	CPed* pPed = GetPed();
	if (pPed->IsLocalPlayer())
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl && !CControlMgr::IsDisabledControl( pControl ) )
		{
			const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
			if (vecStickInput.y < -0.5f)
			{
				CTaskMovePlayer* pPlayerMoveTask = static_cast<CTaskMovePlayer*>(pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_PLAYER));
				if (pPlayerMoveTask)
					pPlayerMoveTask->LockStickHeading(true);
			}
		}
	}
}

CTask::FSM_Return CTaskMotionPed::DiveToSwim_OnUpdate()
{
	CPed* pPed = GetPed();
	m_MoveNetworkHelper.SetFloat(ms_SpeedId, GetPed()->GetMotionData()->GetCurrentMbrY());	
	pPed->m_Buoyancy.SetShouldStickToWaterSurface(true);

	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

	float transitionPhase = 0;
	static dev_float MIN_BREAKOUT_PHASE = 0.4f;	
	transitionPhase = m_MoveNetworkHelper.GetFloat(ms_TransitionPhaseId);

	if(transitionPhase >= MIN_BREAKOUT_PHASE || m_MoveNetworkHelper.GetBoolean(ms_TransitionClipFinishedId)) 
	{
		TaskSetState(State_Swimming);
		return FSM_Continue;
	}

	if(!pPed->GetIsSwimming())
	{
		// need to switch to an on foot state		
		if ( pPed->GetMotionData()->GetCurrentMbrY()>MOVEBLENDRATIO_WALK ) 
		{
			fwMvClipSetId swimmingClipSetId = GetDefaultSwimmingClipSet();  																				
			if (m_SwimClipSetRequestHelper.Request(swimmingClipSetId)) 
			{
				TaskSetState(State_SwimToRun);					
				return FSM_Continue;				
			}
		}
		TaskSetState(State_OnFoot);
	}
	else
	{	
		if (m_wantsToDiveUnderwater && transitionPhase >= MIN_BREAKOUT_PHASE)
		{
			m_wantsToDiveUnderwater = false;
			// Keep a reference to the dive under clip clipSetId, since the subtask is about to release its reference when it exits
			RefDiveUnderClip();
			TaskSetState(State_Diving);
			pPed->m_Buoyancy.SetShouldStickToWaterSurface(false);

		}
	}
		
	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////
// Diving behaviour

void	CTaskMotionPed::Diving_OnEnter()
{
	CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
	if (pWepMgr)
	{
		const CWeaponInfo* pWeaponInfo = pWepMgr->GetEquippedWeaponInfo();
		if (!pWeaponInfo || !pWeaponInfo->GetUsableUnderwater())
		{
			pWepMgr->BackupEquippedWeapon();
			pWepMgr->DestroyEquippedWeaponObject(false);
			pWepMgr->EquipBestWeapon();
		}
	}

	u32 initialState = CTaskMotionDiving::State_DivingDown;
	if (GetMotionData().GetForcedMotionStateThisFrame()==CPedMotionStates::MotionState_Diving_Swim)
		initialState = CTaskMotionDiving::State_Swimming;
	//If we're coming from aiming, start in idle state (to avoid dipping down into a glide anim)
	if (GetPreviousState() == State_Aiming && GetSubTask() && (GetSubTask()->GetState() == CTaskMotionAiming::State_SwimIdleIntro || (GetSubTask()->GetState() == CTaskMotionAiming::State_SwimIdle) || (GetSubTask()->GetState() == CTaskMotionAiming::State_SwimIdleOutro)))
		initialState = CTaskMotionDiving::State_Idle;
#if FPS_MODE_SUPPORTED
	// Start diving in the previously cached state if set
	if (m_iDivingInitialState != -1)
	{
		initialState = m_iDivingInitialState;
		m_iDivingInitialState = -1;
	}
#endif	//FPS_MODE_SUPPORTED

	// start the subtask
	CTaskMotionDiving* pTask = rage_new CTaskMotionDiving( initialState );

	// Default blend duration to 0.25f
	m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, 0.25f);

#if FPS_MODE_SUPPORTED
	if (GetPed()->GetIsFPSSwimming())
	{
		m_vCachedSwimVelocity = VEC3_ZERO;

		// Increase blend duration
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fToDiveBlendDuration, 0.5f, 0.01f, 5.0f, 0.01f);
		m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, fToDiveBlendDuration);
	}
	
	// If we've swapped from FPS->TPS, do an instant blend
	if (GetPed()->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !GetPed()->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON))
	{
		m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, 0.0f);
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAnimUpdate, true);
	}
#endif	// FPS_MODE_SUPPORTED

	m_MoveNetworkHelper.SendRequest(ms_DivingId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterDivingId, ms_DivingNetworkId);	

	m_MoveNetworkHelper.SetFlag( false, ms_SkipTransitionId);
	SetNewTask( pTask );
}

CTask::FSM_Return	CTaskMotionPed::Diving_OnUpdate(){
	CPed * pPed = GetPed();

	if (m_requestedCloneStateChange==State_OnFoot)
	{
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}

	if (!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;		
	}

#if FPS_MODE_SUPPORTED
	// Restart the state if we've changed camera mode between FPS and TPS
	CTaskMotionDiving* pTask = static_cast<CTaskMotionDiving*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_DIVING));			
	if (pTask)
	{
		fwMvClipSetId currentClipsetId = pTask->GetCurrentDivingClipsetId();
		fwMvClipSetId desiredClipsetId = pTask->GetDivingClipSetId();
		if (currentClipsetId != CLIP_SET_ID_INVALID && desiredClipsetId != CLIP_SET_ID_INVALID)
		{
			// If camera mode is different to selected clipset, restart the state/subtask
			if (currentClipsetId != desiredClipsetId)
			{
				m_iDivingInitialState = pTask->GetState();	// Restart diving in the same state that we left it in
				static const fwMvRequestId s_RestartDivingNetwork("RestartDivingNetwork",0x001202c7);
				m_MoveNetworkHelper.SendRequest(s_RestartDivingNetwork);
				TaskSetState(State_Diving);
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseAimingMotionTask) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute))
	{
		TaskSetState(State_Aiming);
		return FSM_Continue;
	}
#endif	//FPS_MODE_SUPPORTED

	// Make sure the correct weapon clip clipSetId is applied to the ped
	if(GetSubTask())
	{
		ApplyWeaponClipSet(GetSubTask());
	}

	static dev_float fFreeSwimDepth = 0.8f;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float fWaterLevel = vPedPos.z + fFreeSwimDepth + 0.01f;

	if (m_isUsingDiveUnderClip)
	{
		ReleaseDiveUnderClip();
	}

	bool bDivingFromFall = (m_DiveImpactSpeed!=0.0f && GetTimeInState() < 0.75f);
	if (bDivingFromFall ||  //if falling in water give the dive time to dive
		( (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling) || (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))) && 				
		pPed->m_Buoyancy.GetWaterLevelIncludingRivers(vPedPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed, pPed->GetCanUseHighDetailWaterLevel())))
	{
		if(bDivingFromFall || vPedPos.z < fWaterLevel - fFreeSwimDepth)
		{
			// Check if we're aiming while diving (don't need to check for weapon UsableUnderwater as we won't be able to equip it)
			if(CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading)))
			{
				if(RequestAimingClipSet() && IsAimingTransitionNotBlocked())
				{
					if(IsAimingIndependentMoverNotBlocked())
					{
						float fCurrentHeading = pPed->GetCurrentHeading();
						float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
						float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
						SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
						pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
						m_OnFootIndependentMover = true;
						m_fPostMovementIndependentMoverTargetHeading = FLT_MAX;
					}
					TaskSetState(State_Aiming);
				}
			}
			
			return FSM_Continue;
		}
		else
		{
			CTaskMotionDiving* pDivingTask = static_cast<CTaskMotionDiving*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_DIVING));			
			if (pDivingTask!=NULL )
			{
// 				float fGroundClearance = pDivingTask->GetGroundClearance(); //removed per B* 1109060
// 				if (fGroundClearance>0 && fGroundClearance < 2.0f) 
// 				{
// 					m_MoveNetworkHelper.SetFlag( true, ms_SkipTransitionId);
// 					TaskSetState(State_Swimming);	
// 					return FSM_Continue;
// 				} 

				//Check for animation streamed in B* 489172
				fwMvClipSetId swimmingClipSetId = GetDefaultDivingClipSet();										
				fwClipSet *clipSet = fwClipSetManager::GetClipSet(swimmingClipSetId);
				if (clipSet==NULL || !clipSet->IsStreamedIn_DEPRECATED())
				{
					m_MoveNetworkHelper.SetFlag( true, ms_SkipTransitionId);
					TaskSetState(State_Swimming);	
					return FSM_Continue;
				}
			}

			// we're back to surface depth
			// change to the swimming state			
			TaskSetState(State_DiveToSwim);	
		}
	}
	else
	{	
		// We're no longer in the water at all!
		// make a sensible transition (at the moment we only support on foot)
		TaskSetState(State_OnFoot);
	}

	return FSM_Continue;
}

void	CTaskMotionPed::Diving_OnExit()
{
	m_DiveImpactSpeed=0;
	m_DiveFromFall = false;
	if(!GetPed()->GetIsDeadOrDying())
	{
		CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
		if (pWepMgr)
		{
			pWepMgr->RestoreBackupWeapon();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Do nothing behaviour

void	CTaskMotionPed::DoNothing_OnEnter()
{

	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterDoNothing);
}

CTask::FSM_Return	CTaskMotionPed::DoNothing_OnUpdate()
{
	if(!IsMoveNetworkHelperInTargetState())
	{
		CPed* pPed = GetPed();

		if (GetTimeInState() >= 1.0f && pPed->IsControlledByNetwork() && m_lastRequestedCloneState != GetState() && m_lastRequestedCloneState == State_InVehicle)
		{
			pPed->GetMotionData()->SetForcedMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
			TaskSetState(State_Start);
			aiDebugf1("Frame : %i, network clone ped %s (%p) stuck in DoNothing motion state, restarting to State_Start", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed());
		}

		return FSM_Continue;
	}

	// The do nothing state must be forced every frame, or we'll revertbn to an appropriate state
	if ( GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_DoNothing )
	{
		CPed* pPed = GetPed();

		if (GetMotionData().GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Diving_Idle || m_DiveImpactSpeed!=0.0f) 
		{
			if (m_DiveImpactSpeed!=0.0f)
			{
				//if swimming clipset is streamed in, play a special animation
				fwMvClipSetId swimmingClipSetId = GetDefaultDivingClipSet();										
				fwClipSet *clipSet = fwClipSetManager::GetClipSet(swimmingClipSetId);
				if (clipSet!=NULL && clipSet->IsStreamedIn_DEPRECATED())
				{					
					if (m_DiveFromFall)
						ForceStateChange(ms_DivingFromFallId);
					else
						ForceStateChange(ms_DivingId);					
				} else
					ForceStateChange(ms_DivingId);					
			} else
				ForceStateChange(ms_DivingId);

			TaskSetState(State_Diving);			
		}
		else if((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle()))
		{
			TaskSetState(State_InVehicle);
			ForceStateChange(ms_InVehicleId);
		}
		else if(pPed->GetIsSwimming())
		{
			TaskSetState(State_Swimming);
			ForceStateChange(ms_SwimmingId);
		}
		else if(IsParachuting())
		{
			TaskSetState(State_Parachuting);
			ForceStateChange(ms_ParachutingId);
		}
		else if(IsUsingJetpack())
		{
			TaskSetState(State_Jetpack);
			ForceStateChange(ms_ParachutingId);
		}
		else if ( GetMotionData().GetIsStrafing() )
		{
			if (CTaskAimGun::ShouldUseAimingMotion(pPed) && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
			{
				if (RequestAimingClipSet())
				{
					TaskSetState(State_Aiming);
					ForceStateChange(ms_AimingId);
				}
			}
			else
			{
				TaskSetState(State_Strafing);
				ForceStateChange(ms_StrafingId);				
			}
		}
		else	// Otherwise we must be walking around on-foot
		{
			// are we crouching
			if (GetMotionData().GetIsCrouching())
			{
				TaskSetState(State_Crouch);
				ForceStateChange(ms_CrouchId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
		}
			
	}

	return FSM_Continue;
}

void	CTaskMotionPed::DoNothing_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// AnimatedVelocity

void	CTaskMotionPed::AnimatedVelocity_OnEnter()
{

}

CTask::FSM_Return	CTaskMotionPed::AnimatedVelocity_OnUpdate()
{
	// The animated velocity state must be forced every frame, or we'll default back to on foot
	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_AnimatedVelocity)
	{
		if( m_DiveImpactSpeed != 0.0f)
		{		
			// If swimming clipset is streamed in, play a special animation
			fwMvClipSetId swimmingClipSetId = GetDefaultDivingClipSet();										
			fwClipSet *clipSet = fwClipSetManager::GetClipSet(swimmingClipSetId);
			if (clipSet!=NULL && clipSet->IsStreamedIn_DEPRECATED())
			{
				ForceStateChange(ms_DivingFromFallId);
			}
			else
			{
				ForceStateChange(ms_DivingId);
			}
			TaskSetState(State_Diving);
		} 
		else
		{
			TaskSetState(State_OnFoot);
			ForceStateChange(ms_OnFootParentParentStateId);
			ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
		}		
	}
	return FSM_Continue;
}

void	CTaskMotionPed::AnimatedVelocity_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// In vehicle state definition

void	CTaskMotionPed::InVehicle_OnEnter()
{
	// start the subtask
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[PedMotion] Ped %s starting in vehicle state\n", AILogging::GetDynamicEntityNameSafe(GetPed()));
	CTaskMotionInVehicle* pTask = rage_new CTaskMotionInVehicle( );

	m_MoveNetworkHelper.SendRequest(ms_InVehicleId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterInVehicleId, ms_InVehicleNetworkId);

	m_isFullyInVehicle = false;

	RequestProcessMoveSignalCalls();
	m_fActualElapsedTimeInState = 0.0f;

	SetNewTask( pTask );
}

CTask::FSM_Return	CTaskMotionPed::InVehicle_OnUpdate()
{
	if (m_restartCurrentStateNextFrame)
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		m_MoveNetworkHelper.SetFloat(ms_InVehicleRestartBlendDurationId, m_fRestartDuration);
		m_MoveNetworkHelper.SendRequest(ms_RestartInVehicleId);
		SetRestartCurrentStateThisFrame(false);
		aiDebugf1("Frame : %i, ped %s (%p) restarting in vehicle state", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed());
		return FSM_Continue;
	}

	if (!IsMoveNetworkHelperInTargetState())
	{
		if (GetTimeInState() > 1.0f)
		{	
			CPed* pPed = GetPed();
			if (pPed->IsControlledByNetwork() && m_lastRequestedCloneState == State_InVehicle)
			{
				pPed->GetMotionData()->SetForcedMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
				TaskSetState(State_Start);
				aiDebugf1("Frame : %i, network clone ped %s (%p) stuck in InVehicle motion state, restarting to State_Start", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed());
			}

			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[PedMotion] Ped %s isn't in target state, current state %s, previous state %s\n", AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(GetState()), GetStaticStateName(GetPreviousState()));
		}

		return FSM_Continue;
	}

	// Add some emergency transition logic here in case we're suddenly out of a vehicle, or the subtask has
	// aborted, this can happen for network clones in laggy network conditions
	CPed* pPed = GetPed();
	if((!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || !pPed->GetMyVehicle()) || !GetSubTask())
	{
		aiDebugf1("Frame : %i, ped %s (%p) transitioning to on foot state", fwTimer::GetFrameCount(), GetPed()->GetModelName(), GetPed());
		TaskSetState(State_OnFoot);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionPed::InVehicle_OnProcessMoveSignals()
{
	m_fActualElapsedTimeInState += fwTimer::GetTimeStep();

	m_fRestartDuration = m_subtaskInsertDuration;

	CPed* pPed = GetPed();
	const float fInitialDelaySeconds = 0.5f;
	if (IsMoveNetworkHelperInTargetState() && GetTimeInState() > fInitialDelaySeconds)
	{
		if (pPed->GetMyVehicle() && GetSubTask() && GetSubTask()->GetState() > CTaskMotionInVehicle::State_StreamAssets)
		{
			const CVehicle& veh = *pPed->GetMyVehicle();
			s32 iSeatIndex = veh.GetSeatManager()->GetPedsSeatIndex(pPed);

			if (veh.IsSeatIndexValid(iSeatIndex))
			{
				bool bShouldRestart = false;

				// Search for overriden in vehicle clipset being set on the ped
				const u32 uOverrideInVehicleContext = GetOverrideInVehicleContextHash();
				if (uOverrideInVehicleContext != 0 && GetSubTask() && !pPed->IsInFirstPersonVehicleCamera())
				{
					CMoveNetworkHelper* pNetworkHelper = GetSubTask()->GetMoveNetworkHelper();
					if (pNetworkHelper && pNetworkHelper->IsNetworkAttached())
					{
						CTaskMotionInAutomobile* pInAutomobile = static_cast<CTaskMotionInAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
						if (pInAutomobile && pInAutomobile->GetSeatOverrideAnimInfo())
						{
							const fwMvClipSetId overrideClipsetId = pInAutomobile->GetSeatOverrideAnimInfo()->GetSeatClipSet();
							if (overrideClipsetId != pNetworkHelper->GetClipSetId())
							{
								bShouldRestart = true;
							}
						}
					}
				}

				// Hang on to idles for peds that are close and on screen to avoid pops
				const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(*pPed, &veh, iSeatIndex);
				if (!bShouldRestart)
				{
					if (pPed->GetIsOnScreen())
					{
						TUNE_GROUP_FLOAT(IN_VEHICLE_LOW_LOD_TUNE, MAX_DISTANCE_TO_KEEP_LOW_LOD_ANIMS_IN_BICYCLE, 80.0f, 0.0f, 200.0f, 0.1f);
						TUNE_GROUP_FLOAT(IN_VEHICLE_LOW_LOD_TUNE, MAX_DISTANCE_TO_KEEP_LOW_LOD_ANIMS_IN, 15.0f, 0.0f, 25.0f, 0.1f);
						const Vector3 vCamPosition = camInterface::GetGameplayDirector().GetFrame().GetPosition();
						const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						const float fMaxDistToKeepLowLodAnimsIn = veh.GetVehicleType() == VEHICLE_TYPE_BICYCLE ? MAX_DISTANCE_TO_KEEP_LOW_LOD_ANIMS_IN_BICYCLE : MAX_DISTANCE_TO_KEEP_LOW_LOD_ANIMS_IN;
						if (vCamPosition.Dist2(vPedPosition) > square(fMaxDistToKeepLowLodAnimsIn))
						{
							bShouldRestart = CTaskVehicleFSM::ShouldClipSetBeStreamedOut(inVehicleClipsetId);
						}
					}
				}

				if (bShouldRestart)
				{
					SetRestartCurrentStateThisFrame(true);
					m_fRestartDuration = NORMAL_BLEND_DURATION;
					// ensure OnUpdate call next frame
					pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
					return true;
				}
				else
				{			
					eStreamingPriority priority = (pPed->IsLocalPlayer() || pPed->PopTypeIsMission()) ? SP_High : SP_Invalid;
					CTaskVehicleFSM::RequestVehicleClipSet(inVehicleClipsetId, priority);
				}
			}
		}
	}

	// If we went straight to the vehicle state, we were probably warped into the vehicle
	if (!m_isFullyInVehicle)
	{
		if (GetPreviousState() == State_Start || GetPreviousState() == State_DoNothing || m_MoveNetworkHelper.GetBoolean(ms_OnExitOnFootId))
		{
			m_isFullyInVehicle = true;
		}
	}

	// always need processing in this state
	return true;
}

void	CTaskMotionPed::InVehicle_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// Dead state definition

void	CTaskMotionPed::Dead_OnEnter()
{
	// nothing to see here...
}

CTask::FSM_Return	CTaskMotionPed::Dead_OnUpdate()
{
	CPed* pPed = GetPed();

	// The do nothing state must be forced every frame, or we'll revert to an appropriate state
	if ( GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead )
	{
		if (GetMotionData().GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Diving_Idle) 
		{
			TaskSetState(State_Diving);
			ForceStateChange(ms_DivingId);			
		}
		else if((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle()))
		{
			TaskSetState(State_InVehicle);
			ForceStateChange(ms_InVehicleId);
		}
		else if(pPed->GetIsSwimming())
		{
			TaskSetState(State_Swimming);
			ForceStateChange(ms_SwimmingId);
		}
		else if(IsParachuting())
		{
			TaskSetState(State_Parachuting);
			ForceStateChange(ms_ParachutingId);
		}
		else if(IsUsingJetpack())
		{
			TaskSetState(State_Jetpack);
			ForceStateChange(ms_ParachutingId);
		}
		else if ( GetMotionData().GetIsStrafing() )
		{
			if (CTaskAimGun::ShouldUseAimingMotion(pPed) && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
			{
				if (RequestAimingClipSet())
				{
					TaskSetState(State_Aiming);
					ForceStateChange(ms_AimingId);
				}
			}
			else
			{
				TaskSetState(State_Strafing);
				ForceStateChange(ms_StrafingId);				
			}
		}
		else	// Otherwise we must be walking around on-foot
		{
			// are we crouching
			if (GetMotionData().GetIsCrouching())
			{
				TaskSetState(State_Crouch);
				ForceStateChange(ms_CrouchId);
			}
			else
			{
				TaskSetState(State_OnFoot);
				ForceStateChange(ms_OnFootParentParentStateId);
				ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
			}
		}

	}
#if ENABLE_MOTION_IN_TURRET_TASK
	else
	{
		if (pPed->GetIsInVehicle())
		{
			CVehicle& rVeh = *pPed->GetMyVehicle();
			const s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(pPed);
			if (CTaskMotionInTurret::IsSeatWithHatchProtection(iSeatIndex, rVeh))
			{
				CTaskMotionInTurret::KeepTurretHatchOpen(iSeatIndex, rVeh);
			}
		}
	}
#endif //ENABLE_MOTION_IN_TURRET_TASK

	return FSM_Continue;
}

void	CTaskMotionPed::Dead_OnExit()
{
	// nothing to see here...
}

void	CTaskMotionPed::Aiming_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	TUNE_GROUP_BOOL(MOTION_AIMING_DEBUG, SEND_TRANSITION_FINISHED_EVENT, true);
	if (SEND_TRANSITION_FINISHED_EVENT)
	{
		s32 uPreviousState = GetPreviousState();
		bool bCloneSwimming = pPed->IsNetworkClone() && (uPreviousState == State_SwimToRun || uPreviousState == State_Diving || uPreviousState == State_DiveToSwim);
		if (!bCloneSwimming)
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] %s Ped %s sending 'TransitionAnimFinished' MoVE event in CTaskMotionPed::Aiming_OnEnter() \n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(pPed));
			m_MoveNetworkHelper.SetBoolean(ms_TransitionAnimFinishedId, true);
		}
	}
	//Grab the motion data.
	CPedMotionData* pMotionData = pPed->GetMotionData();
	
	//Grab the crouching state.
	const bool bIsCrouching = pMotionData->GetIsCrouching();
	const bool bSwitchedToOrFromCrouching = bIsCrouching != pMotionData->GetWasCrouching();
	
	//Grab the stationary state.
	const CPedMotionData::StationaryAimPose& rStationaryAimPose = pMotionData->GetStationaryAimPose();
	bool bIsStationary = rStationaryAimPose.IsLoopClipValid();
	
	//Grab the aim pose transition.
	CPedMotionData::AimPoseTransition& rAimPoseTransition = pMotionData->GetAimPoseTransition();
	
	bool bLastFootLeft = false;
	bool bUseLeftFootStrafeTransition = false;
	bool bBlockTransition = GetMotionData().GetForcedMotionStateThisFrame()==CPedMotionStates::MotionState_Aiming;
	CTaskMotionAimingTransition::Direction direction = CTaskMotionAimingTransition::D_Invalid;
	float fStartDir = FLT_MAX;

	//Water physics variables
	float fPreviousVelocityChange = CTaskMotionSwimming::s_fPreviousVelocityChange;
	float fHeightBelowWater = CTaskMotionSwimming::s_fHeightBelowWater;
	float fHeightBelowWaterGoal = CTaskMotionSwimming::s_fHeightBelowWaterGoal;
	float fNormalisedVel = FLT_MAX;
	bool bBlendInSetHeading = true;

	if(GetSubTask())
	{
		switch(GetSubTask()->GetTaskType())
		{
		case CTaskTypes::TASK_HUMAN_LOCOMOTION:
			{
				const CTaskHumanLocomotion* pTask = static_cast<const CTaskHumanLocomotion*>(GetSubTask());
				bLastFootLeft = pTask->IsLastFootLeft();
				bUseLeftFootStrafeTransition = pTask->UseLeftFootStrafeTransition();

				if(pTask->GetSubTask() && pTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
				{
					const CTaskMotionAimingTransition* pTransitionTask = static_cast<const CTaskMotionAimingTransition*>(pTask->GetSubTask());
					bBlockTransition = pTransitionTask->ShouldBlendBackToStrafe();
					// Reverse the direction
					direction = pTransitionTask->GetDirection() == CTaskMotionAimingTransition::D_CW ? CTaskMotionAimingTransition::D_CCW : CTaskMotionAimingTransition::D_CW;
				}

				// Block transition while independent mover active
				if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
				{
					SetBlockAimingTransitionTime(AIMING_TRANSITION_DURATION);
				}
			}
			break;
		case CTaskTypes::TASK_MOTION_AIMING:
			{
				const CTaskMotionAiming* pTask = static_cast<const CTaskMotionAiming*>(GetSubTask());
				bLastFootLeft = pTask->IsLastFootLeft();
				bUseLeftFootStrafeTransition = pTask->UseLeftFootStrafeTransition();

				if(pTask->GetSubTask() && pTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
				{
					const CTaskMotionAimingTransition* pTransitionTask = static_cast<const CTaskMotionAimingTransition*>(pTask->GetSubTask());
					bBlockTransition = pTransitionTask->ShouldBlendBackToStrafe();
					// Reverse the direction
					direction = pTransitionTask->GetDirection() == CTaskMotionAimingTransition::D_CW ? CTaskMotionAimingTransition::D_CCW : CTaskMotionAimingTransition::D_CW;
				}

				// Block transition while independent mover active
				if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
				{
					SetBlockAimingTransitionTime(AIMING_TRANSITION_DURATION);
				}

				fStartDir = pTask->GetStartDirection();

#if FPS_MODE_SUPPORTED
				fNormalisedVel = pTask->GetNormalisedVel();
#endif // FPS_MODE_SUPPORTED

				bBlendInSetHeading = false;
			}
			break;
		case CTaskTypes::TASK_MOTION_SWIMMING:
			{
				//Initialize swimming parameters so we don't have a sudden jump in velocity
				const CTaskMotionSwimming* pTask = static_cast<const CTaskMotionSwimming*>(GetSubTask());

				//Locally store water physics values from swim task and pass through aim task constructor
				fPreviousVelocityChange = pTask->GetPreviousVelocityChange();
				fHeightBelowWater = pTask->GetHeightBelowWater();
				fHeightBelowWaterGoal = pTask->GetHeightBelowWaterGoal();
			}
			break;
		default:
			break;
		}
	}

	//Create the aiming task.
	CTaskMotionAiming* pTask = rage_new CTaskMotionAiming(bIsCrouching, bLastFootLeft, bUseLeftFootStrafeTransition, bBlockTransition, direction, fStartDir, !IsAimingIndependentMoverNotBlocked(), fPreviousVelocityChange, fHeightBelowWater, fHeightBelowWaterGoal, fNormalisedVel, bBlendInSetHeading);
	
#if FPS_MODE_SUPPORTED
	if (pTask && pPed->UseFirstPersonUpperBodyAnims(false) && !bSwitchedToOrFromCrouching && !pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipFPSUnHolsterTransition) )
	{
		const bool bSwitchedToFirstPersonFromThirdPersonCover = (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetSwitchedToFirstPersonFromThirdPersonCoverCount() > 0) ? true : false;
		if(m_UseFPSIntroAfterAimingRestart != CLIP_SET_ID_INVALID)
		{
			if(pPed->GetMotionData()->GetWasFPSUnholster())
			{
				m_MoveNetworkHelper.SetFlag(true, ms_UseFirstPersonSwapTransition);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, true);
			}

			pTask->SetFPSIntroTransitionClip(m_UseFPSIntroAfterAimingRestart);
			pTask->SetFPSIntroBlendOutTime(m_fFPSIntroAfterAimingRestartBlendOutTime);
			m_UseFPSIntroAfterAimingRestart = CLIP_SET_ID_INVALID;
			m_fFPSIntroAfterAimingRestartBlendOutTime = 0.15f;
		}
		else if(!pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !bSwitchedToFirstPersonFromThirdPersonCover)
		{
			// Use unholster transitions as the default for most cases in FPS mode
			const CWeapon* pWeaponFromObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponFromObject() : NULL;
			const CWeaponInfo* pWeaponInfo = pWeaponFromObj ? pWeaponFromObj->GetWeaponInfo() : pPed->GetEquippedWeaponInfo();
			
			bool bJustSwitchedToFPSMode = !pPed->GetMotionData()->GetWasUsingFPSMode();
			bool bSkipTransition = pWeaponInfo && ((pWeaponInfo->GetIsUnarmed() && !pPed->GetMotionData()->GetIsFPSLT()) || (pWeaponInfo->GetIsThrownWeapon()) ||(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetRequiresWeaponSwitch()));
			if(pWeaponInfo && (pWeaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN || pWeaponInfo->GetPairedWeaponHolsterAnimation()) && !pPed->GetMotionData()->GetWasFPSUnholster())
			{
				// only play transition from unholster
				bSkipTransition = true;
			}

			if(pWeaponInfo && !bSkipTransition && !bJustSwitchedToFPSMode && !m_bFPSPreviouslySprinting && !m_bFPSPreviouslyCombatRolling && !pPed->GetMotionData()->GetUsingStealth() && !pPed->GetMotionData()->GetWasUsingStealthInFPS() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro))
			{
				fwMvClipSetId clipSetId =  pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed, CPedMotionData::FPS_UNHOLSTER);
				if(clipSetId != CLIP_SET_ID_INVALID)
				{
					pTask->SetFPSIntroTransitionClip(clipSetId);
					pTask->SetFPSIntroBlendOutTime(SelectFPSBlendTime(pPed, false, CPedMotionData::FPS_UNHOLSTER));
					// Not cover, we do our own transitions here and it causes a pop
					// Check cover task is running instead of the reset flag. B* 2091371.
					const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
					if (!pCoverTask)
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, true);
				}
			}	
		}

		m_bFPSPreviouslySprinting = false;
		m_bFPSInitialStateForCombatRollUnholsterTransition = -1;
		m_bFPSPreviouslyCombatRolling = false;
	}
#endif

	//Check if we should skip the idle intro.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro))
	{
		pTask->SetSkipIdleIntro(true);
	}
	
	//Set the stationary aim pose and transition.
	pTask->SetStationaryAimPose(rStationaryAimPose);
	pTask->SetAimPoseTransition(rAimPoseTransition);

	// Default blend duration to 0.25f
	m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, 0.25f);

#if FPS_MODE_SUPPORTED
	// Override strafing clipset if swim-aiming/strafing in FPS mode (swimming@first_person)
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->GetIsSwimming())
	{
		pTask->SetUsingMotionAimingWhileSwimming(true);
		m_vCachedSwimStrafeVelocity = VEC3_ZERO;
		// Initialize the previous pitch value so we don't snap to 0.0f pitch when tranisitoning from swimming
		pTask->SetPreviousPitch(pPed->GetCurrentPitch());

		// Increase blend duration
		TUNE_GROUP_FLOAT(FPS_SWIMMING, fToStrafeBlendDuration, 0.5f, 0.01f, 5.0f, 0.01f);
		m_MoveNetworkHelper.SetFloat(ms_AimToSwimDiveBlendDurationId, fToStrafeBlendDuration);


		// Don't select a new weapon if we came from swimming/diving
		if (GetPreviousState() != State_Swimming && GetPreviousState() != State_Diving)
		{
			// Equip best weapon when underwater 
			CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
			if (pWepMgr)
			{
				const CWeaponInfo* pWeaponInfo = pWepMgr->GetEquippedWeaponInfo();
				if (!pWeaponInfo || !pWeaponInfo->GetUsableUnderwater())
				{
					pWepMgr->BackupEquippedWeapon();
					pWepMgr->DestroyEquippedWeaponObject(false);
					pWepMgr->EquipBestWeapon();
				}
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

	// Copy the default clip sets
	pTask->SetOverrideStrafingClipSet(GetOverrideStrafingClipSet());
	pTask->SetOverrideCrouchedStrafingClipSet(GetOverrideCrouchedStrafingClipSet());
	
	//Clear the aim pose transition -- it is only valid once.
	rAimPoseTransition.Clear();

	CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));

	if (GetPreviousState() == State_Aiming && pGunTask)
	{
#if FPS_MODE_SUPPORTED
		bool bTransitionFromFPSIdle = false;
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
			{
				if(pPed->GetMotionData() && (pPed->GetMotionData()->GetWasFPSIdle() || pPed->GetMotionData()->GetIsFPSIdle()))
				{
					bTransitionFromFPSIdle = true;
				}
			}
		}

		if(!bTransitionFromFPSIdle)
#endif // FPS_MODE_SUPPORTED
		{
			// Skip the intro when going crouch to stand/stand to crouch
			pGunTask->GetGunFlags().SetFlag(GF_SkipIdleTransitions);
		}

	}

	if(CTaskMotionAiming::WillStartInMotion(pPed))
	{
		if(!pTask->WillStartInStrafingIntro(pPed))
		{
			const float	fHeadingDelta = SubtractAngleShorter(CTaskGun::GetTargetHeading(*pPed), pPed->GetFacingDirectionHeading());

			static dev_float LONGER_BLEND_DELTA = HALF_PI;
			if(Abs(fHeadingDelta) > LONGER_BLEND_DELTA)
			{
				m_MoveNetworkHelper.SetBoolean(ms_LongerBlendId, true);
			}
		}
		else
		{
			m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
		}
	}
	else
	{
		m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
		m_MoveNetworkHelper.SetBoolean(ms_IgnoreMoverRotationId, true);
	}

	m_MoveNetworkHelper.SendRequest(ms_AimingId);

	//Set the crouched flag.
	m_MoveNetworkHelper.SetFlag(bIsCrouching, ms_IsCrouchedId);
	
	//Set the stationary flag.
	m_MoveNetworkHelper.SetFlag(bIsStationary, ms_IsStationaryId);
	
	AI_LOG_WITH_ARGS_IF_SCRIPT_PED(pPed, "%s Ped %s entering aiming state, bIsCrouching = %s, bIsStationary= %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bIsCrouching), AILogging::GetBooleanAsString(bIsStationary));
	pTask->GetMotionMoveNetworkHelper().SetupDeferredInsert(&m_MoveNetworkHelper, bIsCrouching ? ms_OnEnterCrouchedStrafingId : ms_OnEnterStandStrafingId, bIsCrouching ? ms_CrouchedStrafingNetworkId : ms_StandStrafingNetworkId);

	if(GetPreviousState() == State_OnFoot || 
		GetPreviousState() == State_ActionMode ||
		GetPreviousState() == State_Stealth ||
		GetPreviousState() == State_Crouch)
	{
		m_isFullyAiming = false;
	}
	else
	{
		// If not coming from OnFoot/Crouch count as fully aiming
		m_isFullyAiming = true;
	}

	m_restartAimingUpperBodyState = false;

	if(GetPreviousState() == State_OnFoot || GetPreviousState() == State_Crouch || GetPreviousState() == State_ActionMode || GetPreviousState() == State_Stealth)
	{
		float fHeadingDelta = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();
		// If we are currently doing a strafe transition, use the velocity instead as this will result in a better blend
		if(GetSubTask() && GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
		{
			float fVelocityHeading;
			pPed->GetVelocityHeading(fVelocityHeading);
			float fDesiredHeading = CTaskGun::GetTargetHeading(*pPed);
			fHeadingDelta = SubtractAngleShorter(fVelocityHeading, fDesiredHeading);
		}
		
		Quaternion q;
		q.FromEulers(Vector3(0.f, 0.f, fHeadingDelta));

		const Vector2& vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector3 v(vCurrentMBR.x, vCurrentMBR.y, 0.f);
		q.Transform(v);
		pPed->GetMotionData()->SetCurrentMoveBlendRatio(v.y, v.x);
	}

	SetNewTask(pTask);

	RequestProcessMoveSignalCalls();

	SetDoAimingIndependentMover(true);
}

CTask::FSM_Return	CTaskMotionPed::Aiming_OnUpdate()
{
	// handle transition logic to other states here
	CPed* pPed = GetPed();

	m_MoveNetworkHelper.SetFloat(ms_SpeedId, pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag());

	float fVelocity = pPed->GetVelocity().Mag();
	m_MoveNetworkHelper.SetFloat(ms_VelocityId, fVelocity);

	if (!pPed->IsNetworkClone() && (!GetSubTask() || (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING && GetSubTask()->GetState() == CTaskMotionAiming::State_StandingIdleIntro)))
	{
		TUNE_GROUP_FLOAT(AIMING_TUNE, MAX_TIME_IN_IDLE_INTRO_STATE, 5.0f, 0.0f, 10.0f, 0.01f);
		if(!taskVerifyf(GetTimeInState() < MAX_TIME_IN_IDLE_INTRO_STATE, "Frame %u: Ped %s stuck in standing idle intro state for longer than %.2f seconds, restarting motion task. GetSubTask() - %s", fwTimer::GetFrameCount(), pPed->GetDebugName(), MAX_TIME_IN_IDLE_INTRO_STATE,AILogging::GetBooleanAsString(GetSubTask() != NULL)))
		{
			TaskSetState(State_Start);
			return FSM_Continue;
		}
	}

	// url:bugstar:4157956 - failsafe to stop long tposes on clones when aiming motion task has quit but we're still in aiming state
	if (pPed->IsNetworkClone() && GetState() == State_Aiming && !GetSubTask())
	{
		TUNE_GROUP_FLOAT(AIMING_TUNE, MAX_TIME_IN_AIMING_STATE_WITH_NO_SUBTASK, 3.0f, 0.0f, 10.0f, 0.01f);
		if(!taskVerifyf(GetTimeInState() < MAX_TIME_IN_AIMING_STATE_WITH_NO_SUBTASK, "Frame %u: Ped %s stuck in aiming state for longer than %.2f seconds without aiming subtask, restarting motion task.", fwTimer::GetFrameCount(), pPed->GetDebugName(), MAX_TIME_IN_AIMING_STATE_WITH_NO_SUBTASK))
		{
			TaskSetState(State_Start);
			return FSM_Continue;
		}
	}

	if(!IsMoveNetworkHelperInTargetState())
	{
		if (pPed->IsNetworkClone() && GetTimeInState() > 5.0f)
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] - %s PED %s MoVE network hasn't reached target state in 5 seconds, restarting motion task\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
			TaskSetState(State_Start);
			return FSM_Continue;
		}

		return FSM_Continue;
	}

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFPSMode = true;
		
		if(pPed->GetMotionData() && pPed->GetMotionData()->GetCombatRoll())
		{
			m_bFPSPreviouslyCombatRolling = true;
		}
		
		// FPS Swim: go to appropriate swim/dive state if swimming and pressing forwards on left stick or if ped has been tasked to follow a route.
		if (pPed->GetIsSwimming() && (pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask) || pPed->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute)))
		{
			if (CheckForDiving())
			{
				m_iDivingInitialState = CTaskMotionDiving::State_Idle;	// Start the diving task in the idle state (so we don't holster/destroy our weapon in CTaskMotionDiving::DivingDown_OnEnter).
				TaskSetState(State_Diving);
			}
			else
			{
				TaskSetState(State_Swimming);
			}
			return FSM_Continue;
		}
	}

	if (pPed->IsNetworkClone() && pPed->IsAPlayerPed())
	{
		ProcessCloneFPSIdle(pPed);
	}
#endif	//FPS_MODE_SUPPORTED

	const CWeaponInfo* pEquippedWeaponInfo = GetWeaponInfo(pPed);
#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		TaskSetState(State_Drunk);
		return FSM_Continue;
	}
	else 
#endif // ENABLE_DRUNK
	if(pPed->GetIsSwimming() && pEquippedWeaponInfo && !pEquippedWeaponInfo->GetIsUnderwaterGun() && !bFPSMode)
	{
		// If we're going from Aiming->Diving we're transitioning from FPS swim to diving, so set the diving initial state parameter so we don't dive down
		if (CheckForDiving())
		{
			m_iDivingInitialState = CTaskMotionDiving::State_Idle;	// Start the diving task in Idle as we have no TPS strafe states to transition to
			TaskSetState(State_Diving);
		}
		else
		{
			TaskSetState(State_Swimming);
		}
		return FSM_Continue;
	}
	else if ( (!GetMotionData().GetIsStrafing() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DontQuitMotionAiming)) ||
			  GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (!GetSubTask() && GetTimeInState() > 1.0f))
	{
		if (!GetSubTask() || GetSubTask()->GetState() > CTaskMotionAiming::State_Initial)
		{
			if (pPed->GetMotionData()->GetIsCrouching())
			{
				TaskSetState(State_Crouch);
				return FSM_Continue;
			}
			else if (pPed->GetIsSwimming())
			{
				if ((GetPreviousState() == State_Diving || State_Swimming) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning))
				{
					//Have to check subtask to see if we are diving/swimming
					//Can't check previous motion state as we may have forced the ped to surface from dive depending on water depth
					if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
					{
						CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming*>(GetSubTask());
						if (pAimingTask)
						{
							if (pAimingTask->GetWasDiving())
							{
								// Only return to diving if the outro has finished
								if (pAimingTask->GetSwimIdleOutroFinished())
								{
									TaskSetState(State_Diving);
								}
							}
							else 
							{
								TaskSetState(State_Swimming);
							}
						}
					}
				}
				return FSM_Continue;
			}
			else
			{
				if(!GetSubTask() || IsAimingTransitionNotBlocked())
				{
					s32 newState = GetState();

					if(GetMotionData().GetUsingStealth() && pPed->CanPedStealth() && pPed->IsUsingStealthMode())
					{
						if(pPed->HasStreamedStealthModeClips())
						{
							newState = State_Stealth;
						}
					}
					else if(pPed->IsUsingActionMode() && !IsInMotionState(CPedMotionStates::MotionState_Sprint) && m_ActionModeClipSetRequestHelper.Request(GetPed()->GetMovementModeClipSet()))
					{
						newState = State_ActionMode;
					}
					else
					{
						newState = State_OnFoot;
					}

					if(newState != GetState())
					{
#if FPS_MODE_SUPPORTED
						if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
						{
							m_bDoAimingIndependentMover = false;
						}
#endif // FPS_MODE_SUPPORTED

						if (pPed->IsNetworkClone())
						{
							m_bDoAimingIndependentMover = false;
						}

						if(m_bDoAimingIndependentMover)
						{
							if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
							{
								const CTaskMotionAiming* pAimingTask = static_cast<const CTaskMotionAiming*>(GetSubTask());

								float fCurrentHeading = pPed->GetMotionData()->GetCurrentHeading();
								m_fPostMovementIndependentMoverTargetHeading = fwAngle::LimitRadianAngleSafe(pAimingTask->GetStrafeDirection() + pPed->GetCurrentHeading());

								float fHeadingDelta = 0.f;
								if(pAimingTask->IsInMotion(pPed) && GetMotionData().GetGaitReducedDesiredMbrY() > 0.f && !CTaskMotionAimingTransition::UseMotionAimingTransition(fCurrentHeading, m_fPostMovementIndependentMoverTargetHeading, pPed))
								{
									fHeadingDelta = SubtractAngleShorter(m_fPostMovementIndependentMoverTargetHeading, fCurrentHeading);
								}
								else
								{
									// Pop the mover to the current heading, just so we have an independent mover in the tree, for further adjustments
									m_fPostMovementIndependentMoverTargetHeading = fCurrentHeading;
								}
							
								SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionAimingId, true);
								pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
								m_OnFootIndependentMover = false;
							}
						}

						TaskSetState(newState);

                        NetworkInterface::ForceMotionStateUpdate(*pPed);

						return FSM_Continue;
					}
				}
			}
		}
	}
	else
	{
		fwMvClipSetId appropriateClipSetId;
		//Grab the weapon info.
		const CWeaponInfo* pWeaponInfo = GetWeaponInfo(pPed);
		if(pWeaponInfo)
		{
			appropriateClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
			if(!CTaskAimGun::ShouldUseAimingMotion(*pWeaponInfo, appropriateClipSetId, pPed))
			{
				TaskSetState(State_Strafing);
				return FSM_Continue;
			}
		}
		else
		{
			appropriateClipSetId = CLIP_SET_ID_INVALID;
		}

		// Quit out if running melee as it was causing IK issues
		bool bRunningMeleeTask = pPed->GetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning );
		bool bUsingUnderwaterGun = pWeaponInfo ? pWeaponInfo->GetIsUnderwaterGun() : false;
		bool bFPSModeAndSwimmingOrAiming = bFPSMode && ( pPed->GetIsSwimming() || pPed->GetPedResetFlag( CPED_RESET_FLAG_IsAiming ) );
		if( bRunningMeleeTask && !bUsingUnderwaterGun && !bFPSModeAndSwimmingOrAiming )
		{
			TaskSetState(State_Strafing);
			return FSM_Continue;
		}

		//Check if the task should restart.
		bool bRestart = false;

		//Grab the sub-task.
		CTask* pSubTask = GetSubTask();

		//Grab the aiming task.
		CTaskMotionAiming* pAimingTask = pSubTask ? static_cast<CTaskMotionAiming *>(pSubTask) : NULL;
#if FPS_MODE_SUPPORTED
		bool bPlayFPSIntroTransition = false;
		bool bSwitchFPSUpperBodyAim = false;

		const bool bWeaponHasFirstPersonScope = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
		bool bInScopeState = (pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->GetInFPSScopeState()) || bWeaponHasFirstPersonScope;

		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseFPSUnholsterTransitionDuringCombatRoll))
		{
			m_bFPSInitialStateForCombatRollUnholsterTransition = pPed->GetMotionData()->GetFPSState();
			if(pWeaponInfo && !pWeaponInfo->GetDisableFPSScope() && pPed->GetMotionData()->GetIsFPSLT() && bInScopeState)
			{
				m_bFPSInitialStateForCombatRollUnholsterTransition =  CPedMotionData::FPS_RNG;
			}
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseFPSUnholsterTransitionDuringCombatRoll, false);
		}

		if(pAimingTask && pAimingTask->GetState() == CTaskMotionAiming::State_Roll && pPed->GetPlayerInfo() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false)  && pPed->GetMotionData()->GetCombatRoll())
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fCombatRollExitSwitchToScopeAnimTime, 0.25f, 0.0f, 5.0f, 0.01f);
			bool bFPSScopeDuringRoll = pWeaponInfo && !pWeaponInfo->GetDisableFPSScope() && bInScopeState && pPed->GetMotionData()->GetIsFPSScope() &&
									   pPed->GetPlayerInfo()->GetExitCombatRollToScopeTimerInFPS() > fCombatRollExitSwitchToScopeAnimTime && 
									   m_bFPSInitialStateForCombatRollUnholsterTransition == CPedMotionData::FPS_RNG;
	
			if(bFPSScopeDuringRoll)
			{
				CPedMotionData::eFPSState prevFPSState = (CPedMotionData::eFPSState) m_bFPSInitialStateForCombatRollUnholsterTransition;

				fwMvClipSetId clipsetId = pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed, prevFPSState);

				if(clipsetId != CLIP_SET_ID_INVALID && appropriateClipSetId != CLIP_SET_ID_INVALID && fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateClipSetId))
				{
					pAimingTask->SetFPSIntroTransitionClip(clipsetId);
					pAimingTask->SetFPSIntroBlendOutTime(SelectFPSBlendTime(pPed, false, prevFPSState));

					m_MoveNetworkHelper.SetFloat(ms_AimingRestartTransitionDuration, FAST_BLEND_DURATION);
					m_MoveNetworkHelper.SendRequest(ms_RestartUpperBodyAimingId);
					pAimingTask->RestartUpperBodyAimNetwork(appropriateClipSetId);

					m_bFPSInitialStateForCombatRollUnholsterTransition = FPS_StreamScope;
				}
			}
		}

#endif
		if(pAimingTask && pAimingTask->GetState() != CTaskMotionAiming::State_Roll)
		{
			bool bInFP = pPed->UseFirstPersonUpperBodyAnims(false);

			//The sub-task should be a motion aiming task.
			taskAssertf(pSubTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING, "Sub-task has an unexpected type.");

			CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
#if FPS_MODE_SUPPORTED
			if(!bInFP || !pAimGunTask || pAimGunTask->GetState() != CTaskAimGunOnFoot::State_FireOutro)
#endif // FPS_MODE_SUPPORTED
			{
				//Grab the crouching state.
				const bool bIsCrouching = pPed->GetMotionData()->GetIsCrouching();

				//If our aim task isn't in the correct crouch state, restart the task.
				if(bIsCrouching != pAimingTask->GetIsCrouched())
				{
					//Notify the aim gun on foot task that the crouch state changed.
					if(pAimGunTask)
					{
						pAimGunTask->SetCrouchStateChanged(true);
					}

					//Restart the task.
					bRestart = true;
				}

				if (pPed->IsNetworkClone())
				{
					// don't restart if the ped is swapping weapons
					u32 heldWeaponHash = pPed->GetWeaponManager()->GetEquippedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() : 0;
					u32 selectedWeaponHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();

					if (heldWeaponHash != selectedWeaponHash)
					{
						return FSM_Continue;
					}
				}

				// restart if the clone has switched FP aim anims
				if (!bRestart)
				{
					if(pPed->IsNetworkClone())
					{
						if(m_bCloneUsingFPAnims != bInFP && appropriateClipSetId != CLIP_SET_ID_INVALID && fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateClipSetId))
						{
							m_MoveNetworkHelper.SendRequest(ms_RestartAimingId);
							AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "%s Ped %s ms_RestartAimingId requested FP aim anims\n", AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
							m_MoveNetworkHelper.SendRequest(ms_RestartUpperBodyAimingId);
							pAimingTask->RestartUpperBodyAimNetwork(appropriateClipSetId);
							bRestart = true;
						}

						m_bCloneUsingFPAnims = bInFP;
					}
					
					//Check if the aim clip set has been set on the aiming task.
					fwMvClipSetId clipSetId = pAimingTask->GetAimClipSet();
					if(clipSetId != CLIP_SET_ID_INVALID)
					{
						bool bFirstPersonSwap = false;
#if FPS_MODE_SUPPORTED
						bFirstPersonSwap = bInFP && pPed->GetMotionData() && pPed->GetMotionData()->GetWasFPSUnholster();
#endif
						//If the clip set is different, restart the task...UNLESS we are doing a grenade throw from gun aim
						if(appropriateClipSetId != CLIP_SET_ID_INVALID && (clipSetId != appropriateClipSetId || m_restartAimingUpperBodyState || bFirstPersonSwap) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
						{
							// Ensure clip set is streamed in
							if(fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateClipSetId))
							{
#if FPS_MODE_SUPPORTED
								if(bFirstPersonSwap && !pPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone) && !pWeaponInfo->GetIsThrownWeapon())
								{
									pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition, true);
								}
								
								// Set the transition clip in FPS mode
								if (bInFP)
								{
									bSwitchFPSUpperBodyAim = true;
									// Don't play FPS transition anims if we're just doing a fidget or for thrown weapons (these are handle by the projectile task)
									if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) && !pWeaponInfo->GetIsThrownWeapon())
									{
										bPlayFPSIntroTransition = true;
									}
								}
#endif	//FPS_MODE_SUPPORTED

#if FPS_MODE_SUPPORTED
								// In FPS mode, we handle upper body restarts AFTER we have figured out if we need to restart the lower body strafe network as well
								if (!bSwitchFPSUpperBodyAim)
#endif
								{
									m_MoveNetworkHelper.SendRequest(ms_RestartUpperBodyAimingId);
									pAimingTask->RestartUpperBodyAimNetwork(appropriateClipSetId);
									m_restartAimingUpperBodyState = false;
								}

							}
						}
					}

					fwMvClipSetId moveClipSetId = pAimingTask->GetMoveClipSet();
					if (moveClipSetId != CLIP_SET_ID_INVALID)
					{
						fwMvClipSetId appropriateClipSetId = GetDefaultAimingStrafingClipSet(pPed->GetMotionData()->GetIsCrouching());
						if (appropriateClipSetId != CLIP_SET_ID_INVALID && appropriateClipSetId != moveClipSetId)
						{
							pAimingTask->RestartLowerBodyAimNetwork();
						}
					}
				}

				if(!bRestart)
				{
					//Check if the task needs a restart.
					if(pAimingTask->GetNeedsRestart() && appropriateClipSetId != CLIP_SET_ID_INVALID && fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateClipSetId))
					{
						const CPedMotionData* pMotionData = pPed->GetMotionData();
						const CPedMotionData::AimPoseTransition& rAimPoseTransition = pMotionData->GetAimPoseTransition();
						if (rAimPoseTransition.IsClipValid() && NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone() && GetPreviousState() == State_Aiming && pAimingTask->GetState() == CTaskMotionAiming::State_PlayTransition)
						{
							AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "[%s] - %s Ped %s didn't restart lower body aim network because they were already playing a transition\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
						}
						else
						{
							pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);
							m_MoveNetworkHelper.SendRequest(ms_RestartAimingId);
							m_MoveNetworkHelper.SendRequest(ms_RestartUpperBodyAimingId);
							pAimingTask->RestartUpperBodyAimNetwork(appropriateClipSetId);

							AI_LOG_WITH_ARGS_IF_SCRIPT_PED(GetPed(), "%s Ped %s ms_RestartAimingId requested GetNeedsRestart\n", AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));

							//Restart the task.
							bRestart = true;
						}
					}
				}

				if(!bRestart)
				{
					//Check if the stationary aim pose has changed.
					if(pPed->GetMotionData()->GetStationaryAimPose() != pAimingTask->GetStationaryAimPose())
					{
						//Restart the task.
						bRestart = true;
					}
				}
			}

#if FPS_MODE_SUPPORTED
			bool bPlayTransitionAfterCombatRollUnholster = (bInFP) ? (m_bFPSPreviouslyCombatRolling && m_bFPSInitialStateForCombatRollUnholsterTransition != -1 && 
															m_bFPSInitialStateForCombatRollUnholsterTransition != pPed->GetMotionData()->GetFPSState()) : false;
			if(bInFP && (bSwitchFPSUpperBodyAim || bPlayTransitionAfterCombatRollUnholster))
			{
				appropriateClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
				CPedMotionData::eFPSState prevFPSState = CPedMotionData::FPS_MAX;

				if(appropriateClipSetId != CLIP_SET_ID_INVALID)
				{
					if(bPlayTransitionAfterCombatRollUnholster)
					{
						bPlayFPSIntroTransition = true;
						prevFPSState = (CPedMotionData::eFPSState) m_bFPSInitialStateForCombatRollUnholsterTransition;
						m_bFPSInitialStateForCombatRollUnholsterTransition = -1;
					}

					if(bPlayFPSIntroTransition)
					{
						fwMvClipSetId clipsetId = pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed, prevFPSState);
						if(bRestart)
						{
							m_UseFPSIntroAfterAimingRestart = clipsetId;
							m_fFPSIntroAfterAimingRestartBlendOutTime = SelectFPSBlendTime(pPed, false, prevFPSState);
						}
						else
						{
							pAimingTask->SetFPSIntroTransitionClip(clipsetId);
							pAimingTask->SetFPSIntroBlendOutTime(SelectFPSBlendTime(pPed, false, prevFPSState));
						}

						if (!pPed->IsNetworkClone())
						{
							m_MoveNetworkHelper.SetFloat(ms_AimingRestartTransitionDuration, SelectFPSBlendTime(pPed, true, prevFPSState));
						}
					}

					if(!bRestart && fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateClipSetId))
					{
						if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition))
						{
							m_MoveNetworkHelper.SetFlag(true, ms_UseFirstPersonSwapTransition);
							// B*2182691: Force a post-camera anim update to ensure the new weapon unholster anims start on the next frame.
							//pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
						}
						m_MoveNetworkHelper.SendRequest(ms_RestartUpperBodyAimingId);
						pAimingTask->RestartUpperBodyAimNetwork(appropriateClipSetId);
						m_restartAimingUpperBodyState = false;	
					}
				}
			}
#endif
			if(bRestart)
			{
				// Set the state so the previous state is set
				TaskSetState(State_Aiming);
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
	}
	return FSM_Continue;
}

void CTaskMotionPed::Aiming_OnExit()
{
	//Check if we are not restarting the aiming state.
	if(!GetIsFlagSet(aiTaskFlags::RestartCurrentState))
	{
		//Grab the motion data.
		CPedMotionData* pMotionData = GetPed()->GetMotionData();
		
		//Clear the aim pose transition.
		pMotionData->GetAimPoseTransition().Clear();
		
		//Clear the stationary aim pose.
		pMotionData->GetStationaryAimPose().Clear();
	}

	//Clear the fully aiming flag
	m_isFullyAiming = false;
}

void CTaskMotionPed::StandToCrouch_OnEnter()
{
	// Set the clip set be the peds movement clip set
	taskAssert(GetDefaultOnFootClipSet() != CLIP_SET_ID_INVALID);
	m_MoveNetworkHelper.SetClipSet(GetDefaultOnFootClipSet());
	m_MoveNetworkHelper.SendRequest(ms_StandToCrouchId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterStandToCrouchId);
}

CTask::FSM_Return CTaskMotionPed::StandToCrouch_OnUpdate()
{
	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

	if (CheckForTransitionBreakout())
	{
		TaskSetState(State_Crouch);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskMotionPed::CrouchToStand_OnEnter()
{
	// Set the clip set be the peds movement clip set
	taskAssert(GetCrouchClipSet() != CLIP_SET_ID_INVALID);
	m_MoveNetworkHelper.SetClipSet(GetCrouchClipSet());
	m_MoveNetworkHelper.SendRequest(ms_CrouchToStandId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterCrouchToStandId);
}

CTask::FSM_Return CTaskMotionPed::CrouchToStand_OnUpdate()
{
	if (!IsMoveNetworkHelperInTargetState())
		return FSM_Continue;

	if (CheckForTransitionBreakout())
	{
		TaskSetState(State_OnFoot);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskMotionPed::Parachuting_OnEnter()
{
	//Set up the motion task.
	CTaskMotionParachuting* pTask = rage_new CTaskMotionParachuting();

	//Send the request to the move network.
	m_MoveNetworkHelper.SendRequest(ms_ParachutingId);
	
	//TODO: This is not necessary until the MoVE network for parachuting is converted to the motion move network.
	//pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterParachutingId, ms_ParachutingNetworkId);
	m_MoveNetworkHelper.WaitForTargetState( ms_OnEnterParachutingId );

	//Start the motion task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMotionPed::Parachuting_OnUpdate()
{
	//TODO: This is not necessary until the MoVE network for parachuting is converted to the motion move network.
	//Ensure the move network is in the correct state.
	if(!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if the ped is no longer parachuting.
	if(!IsParachuting())
	{
		//Parachuting can transition to either swimming or no foot.
		//This is defined in the move network transitions.
		if(pPed->GetIsSwimming())
		{
			TaskSetState(State_Swimming);
		}
		else
		{
			TaskSetState(State_OnFoot);
		}
	}

	return FSM_Continue;
}

void CTaskMotionPed::Parachuting_OnExit()
{
}

void CTaskMotionPed::Jetpack_OnEnter()
{
#if ENABLE_JETPACK
	//Set up the motion task.
	CTaskMotionJetpack* pTask = rage_new CTaskMotionJetpack();

	//Send the request to the move network.
	m_MoveNetworkHelper.SendRequest(ms_ParachutingId);

	//TODO: This is not necessary until the MoVE network for parachuting is converted to the motion move network.
	//pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterParachutingId, ms_ParachutingNetworkId);
	m_MoveNetworkHelper.WaitForTargetState( ms_OnEnterParachutingId );

	//Start the motion task.
	SetNewTask(pTask);
#endif
}

CTask::FSM_Return CTaskMotionPed::Jetpack_OnUpdate()
{
#if ENABLE_JETPACK
	//TODO: This is not necessary until the MoVE network for parachuting is converted to the motion move network.
	//Ensure the move network is in the correct state.
	if(!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if the ped is no longer parachuting.
	if(!IsUsingJetpack())
	{
		//Parachuting can transition to either swimming or no foot.
		//This is defined in the move network transitions.
		if(pPed->GetIsSwimming())
		{
			TaskSetState(State_Swimming);
		}
		else
		{
			TaskSetState(State_OnFoot);
		}
	}
#endif

	return FSM_Continue;
}

void CTaskMotionPed::Jetpack_OnExit()
{
}

#if ENABLE_DRUNK
void CTaskMotionPed::Drunk_OnEnter()
{
	// start the subtask
	CTaskMotionDrunk* pTask = rage_new CTaskMotionDrunk( );

	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMotionPed::Drunk_OnUpdate()
{
	CPed* pPed = GetPed();
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (pPed->IsDrunk() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DRUNK))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else
		{
					// force back to an appropriate state
			if((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle()))
			{
				TaskSetState(State_InVehicle);
				ForceStateChange(ms_InVehicleId);
			}
			else if(pPed->GetIsSwimming())
			{
				TaskSetState(State_Swimming);
				ForceStateChange(ms_SwimmingId);
			}
			else if ( GetMotionData().GetIsStrafing() )
			{
				if (CTaskAimGun::ShouldUseAimingMotion(pPed) && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
				{
					if (RequestAimingClipSet() && IsAimingTransitionNotBlocked())
					{
						TaskSetState(State_Aiming);
						ForceStateChange(ms_AimingId);
					}
				}
				else
				{
					TaskSetState(State_Strafing);
					ForceStateChange(ms_StrafingId);				
				}
			}
			else	// Otherwise we must be walking around on-foot
			{
				// are we crouching
				if (GetMotionData().GetIsCrouching())
				{
					TaskSetState(State_Crouch);
					ForceStateChange(ms_CrouchId);
				}
				else
				{
					TaskSetState(State_OnFoot);
					ForceStateChange(ms_OnFootParentParentStateId);
					ForceSubStateChange(ms_OnFootId, ms_OnFootParentStateId);
				}
			}	
		}
	}
	else
	{
		ForceStateChange(ms_DrunkId);
	}

	return FSM_Continue;
}
#endif // ENABLE_DRUNK

void CTaskMotionPed::RefDiveUnderClip()
{
	if (!m_isUsingDiveUnderClip)
	{
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(GetDefaultSwimmingClipSet());

		if (clipSet && clipSet->IsStreamedIn_DEPRECATED())
		{
			clipSet->AddRef_DEPRECATED();
			m_isUsingDiveUnderClip = 1;
		}
	}
}
void CTaskMotionPed::ReleaseDiveUnderClip()
{
	if (m_isUsingDiveUnderClip)
	{
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(GetDefaultSwimmingClipSet());

		if (clipSet && clipSet->IsStreamedIn_DEPRECATED())
		{
			clipSet->Release_DEPRECATED();
			m_isUsingDiveUnderClip = 0;
		}
	}
}

bool CTaskMotionPed::CheckForTransitionBreakout()
{
	float mbrX = 0.0f;
	float mbrY = 0.0f;

	CPed* pPed = GetPed();
	pPed->GetMotionData()->GetDesiredMoveBlendRatio(mbrX, mbrY);

	const bool bHasDesiredVelocity = !IsNearZero(abs(mbrX)) || !IsNearZero(abs(mbrY));

	bool bWantsToAim = false;
	bool bWantsToStand = false;

	if (pPed->IsLocalPlayer())
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		bWantsToAim = pPed->GetPlayerInfo()->IsAiming();
		bWantsToStand = pControl->GetPedDuck().IsPressed();
	}

	return pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning) ||
		   m_MoveNetworkHelper.GetBoolean(ms_TransitionClipFinishedId) ||
		   (m_MoveNetworkHelper.GetBoolean(ms_TransitionBreakPointId) && (bHasDesiredVelocity || bWantsToStand)) ||
		   bWantsToAim; 
}

CTaskHumanLocomotion* CTaskMotionPed::CreateOnFootTask(const fwMvClipSetId& clipSetId)
{
	CPed* pPed = GetPed();

	// make initial state decisions here
	CTaskHumanLocomotion::State initialState = CTaskHumanLocomotion::State_Idle;

	s32 lastTaskType = -1;
	bool bLastFootLeft = false;
	bool bUseLeftFootStrafeTransition = false;
	float fInitialMovingDirection = pPed->GetCurrentHeading();

	// Is another task forcing a particular sub state?
	if(m_forcedInitialSubState)
	{
		initialState = (CTaskHumanLocomotion::State)m_forcedInitialSubState;
	}
	// Are we transitioning from another basic locomotion state?
	// If so we should try to sync them up
	else if(GetSubTask())
	{
		lastTaskType = GetSubTask()->GetTaskType();
		if(lastTaskType == CTaskTypes::TASK_HUMAN_LOCOMOTION)
		{
			CTaskHumanLocomotion* pTask = static_cast<CTaskHumanLocomotion*>(GetSubTask());
			switch(pTask->GetState())
			{
			case CTaskHumanLocomotion::State_Start:
				{
					if(pTask->GetTimeInState() < CTaskHumanLocomotion::BLEND_TO_IDLE_TIME)
					{
						initialState = CTaskHumanLocomotion::State_Idle;

					}
					else
					{
						initialState = CTaskHumanLocomotion::State_Moving;
					}
				}
				break;
			case CTaskHumanLocomotion::State_Moving:
			case CTaskHumanLocomotion::State_Turn180:
			case CTaskHumanLocomotion::State_FromStrafe:
				initialState = CTaskHumanLocomotion::State_Moving;
				break;
			default:
				break;
			}
			bLastFootLeft = pTask->IsLastFootLeft();
		}
		else if(lastTaskType == CTaskTypes::TASK_MOTION_STRAFING)
		{
			GetMotionData().SetCurrentMoveBlendRatio(0.f);
			m_MoveNetworkHelper.SetBoolean(ms_NoTagSyncId, true);
		}
		else if(lastTaskType == CTaskTypes::TASK_MOTION_AIMING)
		{
			// We need to convert from strafing mbr to normal mbr
 			const Vector2& vCurrentMoveBlendRatio = GetMotionData().GetCurrentMoveBlendRatio();
			float fMBR = vCurrentMoveBlendRatio.Mag();
			GetMotionData().SetCurrentMoveBlendRatio(fMBR);

			const bool bInFpsMode = FPS_MODE_SUPPORTED_ONLY(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) ? true :) false;

			// Are we moving?
			// Always moving in FPS mode - bypass stop strafing state being classed as not moving
			CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming*>(GetSubTask());
			if((pTask->IsInMotion(pPed) || bInFpsMode) && GetMotionData().GetGaitReducedDesiredMbrY() > 0.f)
			{
				initialState = CTaskHumanLocomotion::State_Moving;

#if FPS_MODE_SUPPORTED
				if(bInFpsMode && GetMotionData().GetGaitReducedDesiredMbrY() >= MOVEBLENDRATIO_WALK)
				{
					GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
				}
#endif // FPS_MODE_SUPPORTED
			}

			bLastFootLeft = pTask->IsLastFootLeft();
			bUseLeftFootStrafeTransition = pTask->UseLeftFootStrafeTransition();
			fInitialMovingDirection = fwAngle::LimitRadianAngleSafe(pTask->GetStrafeDirection() + pPed->GetCurrentHeading());
		}
	}
	else
	{
		// If we have a move blend ratio already, go straight into moving
		if(GetMotionData().GetCurrentMbrY() > 0.f)
		{
			initialState = CTaskHumanLocomotion::State_Moving;
		}
	}

	fwMvClipSetId onFootClipSetId(clipSetId);
	fwMvClipSetId baseModelClipSetId(GetModelInfoOnFootClipSet(*pPed));
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(onFootClipSetId);
	Assertf(pClipSet, "Failed to GetClipSet([%s, %u])", onFootClipSetId.GetCStr(), onFootClipSetId.GetHash());
	bool bClipSetStreamedIn = pClipSet && pClipSet->IsStreamedIn_DEPRECATED();
	if(!bClipSetStreamedIn)
	{
		// If our clipset isn't streamed, revert to the modelinfo one, as that should be a dependency.
		onFootClipSetId = baseModelClipSetId;
		taskWarningf("Temporarily using model info clipset [%s] rather than specific clipset [%s]", onFootClipSetId.TryGetCStr(), clipSetId.TryGetCStr());
	}

	CTaskHumanLocomotion* pTask = rage_new CTaskHumanLocomotion(initialState, onFootClipSetId, false, lastTaskType == CTaskTypes::TASK_MOTION_AIMING, bLastFootLeft, bUseLeftFootStrafeTransition, GetState() == State_ActionMode && pPed->IsLocalPlayer(), baseModelClipSetId, fInitialMovingDirection);
	pTask->SetBeginMoveStartPhase(m_fBeginMoveStartPhase);

	// Reset the start phase after it is used.
	m_fBeginMoveStartPhase = 0.0f;

	return pTask;
}

bool CTaskMotionPed::DoAimingTransitionIndependentMover(const CTaskHumanLocomotion* pTask)
{
	if(pTask && pTask->GetSubTask() && pTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
	{
		const CTaskMotionAimingTransition* pTransitionTask = static_cast<const CTaskMotionAimingTransition*>(pTask->GetSubTask());
		if(pTransitionTask->WantsToDoIndependentMover())
		{
			CPed* pPed = GetPed();

			if(pPed->GetMotionData()->GetIndependentMoverTransition() != 1)
			{
				float fCurrentHeading = pPed->GetCurrentHeading();
				float fDesiredHeading = pPed->GetDesiredHeading();
				float fFacingHeading  = pPed->GetFacingDirectionHeading();

				// Allow the heading to move a bit from the facing to the desired
				static dev_float EXTRA_HEADING = QUARTER_PI;
				float fExtraHeading = Clamp(SubtractAngleShorter(fDesiredHeading, fFacingHeading), -EXTRA_HEADING, EXTRA_HEADING);
				float fNewDesiredHeading = fwAngle::LimitRadianAngle(fFacingHeading + fExtraHeading);

				float fHeadingDelta = SubtractAngleShorter(fNewDesiredHeading, fCurrentHeading);
				SetIndependentMoverFrame(fHeadingDelta, ms_IndependentMoverExpressionOnFootId, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_MotionPedDoPostMovementIndependentMover, true);
				m_fPostMovementIndependentMoverTargetHeading = fNewDesiredHeading;
				m_OnFootIndependentMover = true;

				if((pPed->IsUsingActionMode() || (pPed->IsStreamingActionModeClips() && pPed->WantsToUseActionMode())) && m_ActionModeClipSetRequestHelper.Request(pPed->GetMovementModeClipSet()))
				{
					if(GetState() == State_ActionMode)
						SetFlag(aiTaskFlags::RestartCurrentState);
					else
						TaskSetState(State_ActionMode);
				}
				else if(GetMotionData().GetUsingStealth() && pPed->CanPedStealth() && pPed->IsUsingStealthMode() && pPed->HasStreamedStealthModeClips())
				{
					if(GetState() == State_Stealth)
						SetFlag(aiTaskFlags::RestartCurrentState);
					else
						TaskSetState(State_Stealth);
				}
				else
				{
					if(GetState() == State_OnFoot)
						SetFlag(aiTaskFlags::RestartCurrentState);
					else
						TaskSetState(State_OnFoot);
				}

				SetRestartCurrentStateThisFrame(false);

				static const fwMvRequestId s_RestartOnFootNetworksId("RestartOnFootNetworks",0x2f94a183);
				m_MoveNetworkHelper.SendRequest(s_RestartOnFootNetworksId);
				return true;
			}
		}
	}
	return false;
}

bool CTaskMotionPed::CheckForDiving()
{
	float fWaterLevel;
	CPed * pPed = GetPed();
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(vPedPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
	{
		static const float fFreeSwimDepth = 1.0f;
		if(vPedPos.z < fWaterLevel - fFreeSwimDepth)
		{
			return true;
		}
	}
	return false;
}

bool CTaskMotionPed::IsParachuting() const
{
	//Ensure the ped is parachuting.
	if(!GetPed()->GetIsParachuting())
	{
		return false;
	}

	//Ensure the ped is not landing.
	const CTaskParachute* pTask = static_cast<CTaskParachute *>(
		GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
	if(pTask && pTask->IsLanding())
	{
		return false;
	}

	return true;
}

bool CTaskMotionPed::IsUsingJetpack() const
{
	//Ensure the ped is parachuting.
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return true;
	}

	return false;
}

bool CTaskMotionPed::ShouldStrafe() const
{
	const CPed* pPed = GetPed();
	return (GetMotionData().GetIsStrafing() && (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling)));
}

bool CTaskMotionPed::ShouldUseAimingStrafe() const
{
	const CPed* pPed = GetPed();
	return (CTaskAimGun::ShouldUseAimingMotion(pPed) && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || pPed->GetIsInCover() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading)));
}

bool CTaskMotionPed::RequestAimingClipSet()
{
	CPed* pPed = GetPed();
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo(pPed);
	if(pWeaponInfo)
	{
		fwMvClipSetId strafeClipSetId = GetDefaultAimingStrafingClipSet(false);
		if(taskVerifyf(strafeClipSetId != CLIP_SET_ID_INVALID, "strafeClipSetId is invalid"))
		{
			bool bStreamed = false;
			fwClipSet* pStrafeClipSet = fwClipSetManager::GetClipSet(strafeClipSetId);
			if(taskVerifyf(pStrafeClipSet, "strafeClipSetId [%s] doesn't exist in clipsets", strafeClipSetId.TryGetCStr()))
			{
				bStreamed = pStrafeClipSet->Request_DEPRECATED();
				const fwMvClipSetId weaponClipSet = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
				if(weaponClipSet != CLIP_SET_ID_INVALID)
				{
					bStreamed &= m_WeaponClipSetRequestHelper.Request(weaponClipSet);
				}
			}
			return bStreamed;
		}
	}
	return true;
}

s32  CTaskMotionPed::ConvertMoveNetworkStateAndClipSetLod(u32 const state, fwMvClipSetId& UNUSED_PARAM(pendingClipSetId)) const
{
	Assert(state <= CTaskMotionPedLowLod::State_Exit);

	switch(state)
	{
		case CTaskMotionPedLowLod::State_Start:			return CTaskMotionPed::State_OnFoot;		break;
		case CTaskMotionPedLowLod::State_Exit:			return CTaskMotionPed::State_OnFoot;		break;
		case CTaskMotionPedLowLod::State_OnFoot:		return CTaskMotionPed::State_OnFoot;		break;
		case CTaskMotionPedLowLod::State_Swimming:		return CTaskMotionPed::State_Swimming;		break;
		case CTaskMotionPedLowLod::State_InVehicle:		return CTaskMotionPed::State_InVehicle;		break;
		default:
			break;
	}	

	return -1;
}

bool CTaskMotionPed::IsClonePedNetworkStateChangeAllowed(u32 const stateFromNetwork)
{
	CPed const* ped = GetPed();
	Assert(ped && ped->IsNetworkClone());

	if(GetState() == State_Start)
	{
		if(!m_MoveNetworkHelper.GetNetworkPlayer())
		{
			return false;
		}
	}

	if(stateFromNetwork == State_Stealth)
	{
		if(!ped->HasStreamedStealthModeClips())
		{
			return false;
		}
	}

	if(stateFromNetwork == State_Aiming)
	{
		if (NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(*ped))
		{
			return false;
		}

		if(!RequestAimingClipSet())
		{
			return false;
		}
	}

	if(stateFromNetwork == State_ActionMode)
	{
		if(!(ped->IsUsingActionMode() && m_ActionModeClipSetRequestHelper.Request(ped->GetMovementModeClipSet())))
		{
			return false;
		}
	}

	if(stateFromNetwork == State_OnFoot)
	{
		//! If we are still using action mode, don't transition back to on foot.
		if(ped->IsUsingActionMode() && GetState() == State_ActionMode && m_ActionModeClipSetRequestHelper.Request(ped->GetMovementModeClipSet()))
		{
			return false;
		}
	}

	if(stateFromNetwork == State_Aiming && GetState() == State_InVehicle)
	{
		return false;
	}

	return true;
}

bool CTaskMotionPed::IsClonePedMoveNetworkStateTransitionAllowed(u32 const targetState) const
{
	// Based on anim\move\networks\pedmotion.mxtf

	u32 const currentState = GetState();

	switch(currentState)
	{
	case State_Start:				return ((targetState == State_OnFoot) || (targetState == State_InVehicle));
	case State_OnFoot:				return ((targetState == State_Parachuting) || (targetState == State_Swimming) || (targetState == State_Diving) || (targetState == State_Stealth) || (targetState == State_Crouch) || (targetState == State_Strafing) || (targetState == State_Aiming) || (targetState == State_ActionMode));
	case State_Strafing:			return ((targetState == State_StrafeToOnFoot) || (targetState == State_Swimming) || (targetState == State_OnFoot) || (targetState == State_Stealth) || (targetState == State_Crouch) || (targetState == State_Aiming));
	case State_StrafeToOnFoot:		return ((targetState == State_OnFoot) || (targetState == State_ActionMode) || (targetState == State_Strafing) || (targetState == State_Stealth));
	case State_Swimming:			return ((targetState == State_OnFoot) || (targetState == State_SwimToRun) || (targetState == State_Crouch) || (targetState == State_Stealth) || (targetState == State_Strafing));
	case State_SwimToRun:			return (targetState == State_OnFoot);
	case State_DiveToSwim:			return ((targetState == State_Swimming) || (targetState == State_SwimToRun) || (targetState == State_OnFoot));
	case State_Diving:				return ((targetState == State_DiveToSwim) || (targetState == State_Swimming) || (targetState == State_OnFoot));
	case State_DoNothing:			return false;
	case State_AnimatedVelocity:	return false;
	case State_Crouch:				return ((targetState == State_Aiming) || (targetState == State_Strafing) || (targetState == State_Swimming) || (targetState == State_OnFoot));
	case State_Stealth:				return ((targetState == State_Swimming) || (targetState == State_Strafing) || (targetState == State_Aiming) || (targetState == State_OnFoot) || (targetState == State_ActionMode));
	case State_InVehicle:			return (targetState == State_OnFoot);
	case State_Aiming:				return ((targetState == State_OnFoot) || (targetState == State_ActionMode) || (targetState == State_Strafing) || (targetState == State_Crouch) || (targetState == State_Stealth) || (targetState == State_Swimming));
	case State_StandToCrouch:		return false;
	case State_CrouchToStand:		return false;
	case State_ActionMode:			return ((targetState == State_Stealth) || (targetState == State_Aiming) || (targetState == State_Strafing) || (targetState == State_Swimming) || (targetState == State_OnFoot));
	case State_Parachuting:			return ((targetState == State_Swimming) || (targetState == State_OnFoot));
	case State_Jetpack:				return ((targetState == State_Swimming) || (targetState == State_OnFoot));
#if ENABLE_DRUNK
	case State_Drunk:				return false;
#endif // ENABLE_DRUNK
	case State_Dead:				return false;
	case State_Exit:				return false;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// CTaskMotionPedLowLod
////////////////////////////////////////////////////////////////////////////////

// Statics
const fwMvNetworkId CTaskMotionPedLowLod::ms_OnFootNetworkId("OnFootNetwork",0xE40B89C2);
const fwMvNetworkId CTaskMotionPedLowLod::ms_SwimmingNetworkId("SwimmingNetwork",0xC419414C);
const fwMvNetworkId CTaskMotionPedLowLod::ms_InVehicleNetworkId("InVehicleNetwork",0x209C511B);
const fwMvRequestId CTaskMotionPedLowLod::ms_OnFootId("OnFoot",0xF5A638B9);
const fwMvRequestId CTaskMotionPedLowLod::ms_SwimmingId("Swimming",0x5DA2A411);
const fwMvRequestId CTaskMotionPedLowLod::ms_InVehicleId("InVehicle",0x579FFAC8);
const fwMvBooleanId CTaskMotionPedLowLod::ms_OnEnterOnFootId("OnEnter_OnFoot",0xD850B358);
const fwMvBooleanId CTaskMotionPedLowLod::ms_OnExitOnFootId("OnExit_OnFoot",0xD36B5B15);
const fwMvBooleanId CTaskMotionPedLowLod::ms_OnEnterSwimmingId("OnEnter_Swimming",0x912B6EC2);
const fwMvBooleanId CTaskMotionPedLowLod::ms_OnEnterInVehicleId("OnEnter_Swimming",0x912B6EC2);
const fwMvRequestId CTaskMotionPedLowLod::ms_RestartOnFootId("RestartOnFoot",0x3DEF0221);
const fwMvRequestId CTaskMotionPedLowLod::ms_RestartInVehicleId("RestartInVehicle",0x2CA261C3);

CTaskMotionPedLowLod::CTaskMotionPedLowLod()
: m_fNetworkInsertDuration(INSTANT_BLEND_DURATION)
, m_uLastOnFootRestartTime(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_PED_LOW_LOD);
}

CTaskMotionPedLowLod::~CTaskMotionPedLowLod()
{
}

#if __DEV 
void CTaskMotionPedLowLod::SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId)
{
	animAssert(clipSetId != CLIP_SET_ID_INVALID); 
	ASSERT_ONLY(CTaskMotionBasicLocomotionLowLod::VerifyLowLodMovementClipSet(clipSetId, GetEntity() ? GetPed() : NULL));
	CTaskMotionBase::SetDefaultOnFootClipSet(clipSetId);
}
#endif

#if !__FINAL
const char * CTaskMotionPedLowLod::GetStaticStateName( s32 iState )
{
	Assert((iState >= State_Start) && (iState <= State_Exit));
	switch(iState)
	{
	case State_Start:     return "Start";
	case State_OnFoot:    return "OnFoot";
	case State_Swimming:  return "Swimming";
	case State_InVehicle: return "InVehicle";
	case State_Exit:      return "Exit";
	default:
		Assertf(false, "CTaskMotionPedLowLod::GetStaticStateName : Invalid State %d", iState);
		return "Unknown";
	}
}
#endif // !__FINAL

bool CTaskMotionPedLowLod::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		taskAssert(dynamic_cast<const CTaskMotionBase*>(pSubTask));
		return static_cast<const CTaskMotionBase*>(pSubTask)->IsInMotionState(state);
	}

	return false;
}

void CTaskMotionPedLowLod::GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds)
{
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		taskAssert(dynamic_cast<CTaskMotionBase*>(pSubTask));
		static_cast<CTaskMotionBase*>(pSubTask)->GetMoveSpeeds(speeds);
	}
	else
	{
		speeds.SetWalkSpeed(0.0f);
		speeds.SetRunSpeed(0.0f);
		speeds.SetSprintSpeed(0.0f);
	}
}

void CTaskMotionPedLowLod::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		taskAssert(dynamic_cast<CTaskMotionBase*>(pSubTask));
		static_cast<CTaskMotionBase*>(pSubTask)->GetNMBlendOutClip(outClipSet, outClip);
	}
	else
	{
		outClipSet = m_defaultOnFootClipSet;
		outClip = CLIP_IDLE;
	}
}

bool CTaskMotionPedLowLod::IsInMotion(const CPed* pPed) const
{
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		taskAssert(dynamic_cast<const CTaskMotionBase*>(pSubTask));
		return static_cast<const CTaskMotionBase*>(pSubTask)->IsInMotion(pPed);
	}

	return false;
}

CTask* CTaskMotionPedLowLod::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

bool CTaskMotionPedLowLod::ShouldStickToFloor()
{
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		taskAssert(dynamic_cast<CTaskMotionBase*>(pSubTask));
		return static_cast<CTaskMotionBase*>(pSubTask)->ShouldStickToFloor();
	}
	return false;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed const* ped = GetPed();
	m_WaitingForTargetState = false;
	if(ped && ped->IsNetworkClone())
	{
		if(iEvent == OnUpdate)
		{
			if(GetState() != State_Exit) 
			{
				if(m_requestedCloneStateChange != -1)
				{
					if(m_requestedCloneStateChange != GetState())
					{
						// Am I in a suitable state to change state?
						if(IsClonePedNetworkStateChangeAllowed(m_requestedCloneStateChange))
						{
							if(IsClonePedMoveNetworkStateTransitionAllowed(m_requestedCloneStateChange) && m_MoveNetworkHelper.IsInTargetState())
							{
								TaskSetState(m_requestedCloneStateChange);
								m_requestedCloneStateChange = -1;
								return FSM_Continue;
							}
							else
							{
								fwMvRequestId stateRequestId;

								Verifyf(GetStateMoveNetworkRequestId(m_requestedCloneStateChange, stateRequestId), "CTaskMotionPed::UpdateFSM : Cannot find move network request for state %s", GetStaticStateName(m_requestedCloneStateChange));

								m_MoveNetworkHelper.ForceStateChange(stateRequestId);
								TaskSetState(m_requestedCloneStateChange);
								m_requestedCloneStateChange = -1;
								return FSM_Continue;
							}
						}
					}

					m_requestedCloneStateChange = -1;
				}
			}
		}
	}

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_OnFoot)
			FSM_OnEnter
				return OnFoot_OnEnter();
			FSM_OnUpdate
				return OnFoot_OnUpdate();
		FSM_State(State_Swimming)
			FSM_OnEnter
				return Swimming_OnEnter();
			FSM_OnUpdate
				return Swimming_OnUpdate();
		FSM_State(State_InVehicle)
			FSM_OnEnter
				return InVehicle_OnEnter();
			FSM_OnUpdate
				return InVehicle_OnUpdate();
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskMotionPedLowLod::CleanUp()
{
	if(m_CleanupMotionTaskNetworkOnQuit)
	{
		if(m_MoveNetworkHelper.IsNetworkActive())
		{
			GetPed()->GetMovePed().ClearMotionTaskNetwork(m_MoveNetworkHelper);
		}
	}
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::Start_OnUpdate()
{
	if(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkMotionPedLowLod))
	{
		CPed* pPed = GetPed();

		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionPedLowLod);
		pPed->GetMovePed().SetMotionTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), m_fNetworkInsertDuration, CMovePed::MotionTask_TagSyncTransition);

		// Crouching is unsupported in low lod
		pPed->SetIsCrouching(false);

		// Is something forcing a particular motion state this frame?
		switch(GetMotionData().GetForcedMotionStateThisFrame())
		{
		case CPedMotionStates::MotionState_Idle:
			TaskSetState(State_OnFoot);
			m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(0.0f, 0.0f);
			break;

		case CPedMotionStates::MotionState_Walk:
			TaskSetState(State_OnFoot);
			m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
			GetMotionData().SetCurrentMoveBlendRatio(1.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(1.0f,  0.0f);
			break;
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
			TaskSetState(State_OnFoot);
			m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
			GetMotionData().SetCurrentMoveBlendRatio(2.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(2.0f, 0.0f);
			break;
		case CPedMotionStates::MotionState_Swimming_TreadWater:
			TaskSetState(State_Swimming);
			m_MoveNetworkHelper.ForceStateChange(ms_SwimmingId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
			break;
		case CPedMotionStates::MotionState_InVehicle:
			TaskSetState(State_InVehicle);
			m_MoveNetworkHelper.ForceStateChange(ms_InVehicleId);
			break;
		default:
			TaskSetState(State_OnFoot);
			m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
			break;
		}

		if( pPed->GetPedResetFlag(CPED_RESET_FLAG_SpawnedThisFrameByAmbientPopulation) )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame, true);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextTaskUpdate);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextAnimUpdate);
		}
		else if( pPed->PopTypeIsMission() || pPed->IsPlayer() )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
	}

	return FSM_Continue;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::OnFoot_OnEnter()
{
	fwMvClipSetId onFootClipSetId(GetDefaultOnFootClipSet());
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(onFootClipSetId);
	taskAssertf(pClipSet, "pClipSet is NULL, clip set id: %s", onFootClipSetId.TryGetCStr());
	bool bClipSetStreamedIn = pClipSet->IsStreamedIn_DEPRECATED();
	if(!bClipSetStreamedIn)
	{
		fwMvClipSetId baseModelClipSetId(GetModelInfoOnFootClipSet(*GetPed()));

		taskWarningf("CTaskMotionPedLowLod::OnFoot_OnEnter: Temporarily using model info clipset [%s] rather than specific clipset [%s]", baseModelClipSetId.TryGetCStr(), onFootClipSetId.TryGetCStr());
		// If our clipset isn't streamed, revert to the modelinfo one, as that should be a dependency.
		onFootClipSetId = baseModelClipSetId;
	}

	// Start the subtask
	CTaskMotionBasicLocomotionLowLod* pTask = rage_new CTaskMotionBasicLocomotionLowLod(CTaskMotionBasicLocomotionLowLod::State_Idle, onFootClipSetId);

	m_MoveNetworkHelper.SendRequest(ms_OnFootId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterOnFootId, ms_OnFootNetworkId);

	SetNewTask(pTask);
	return FSM_Continue;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::OnFoot_OnUpdate()
{
	CPed* pPed = GetPed();

	taskAssert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION_LOW_LOD);
	CTaskMotionBasicLocomotionLowLod* pChildTask = static_cast<CTaskMotionBasicLocomotionLowLod*>(GetSubTask());

	// Make sure the correct weapon clip clipSetId is applied to the ped
	if(pChildTask)
	{
		ApplyWeaponClipSet(pChildTask);
	}

	// Early out if move hasn't caught up yet
	if(!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
	{
		TaskSetState(State_InVehicle);
		return FSM_Continue;
	}
	else if(pPed->GetIsSwimming())
	{
		TaskSetState(State_Swimming);
		return FSM_Continue;
	}
	else
	{
		// Check for restarts
		static dev_u32 MIN_TIME_BETWEEN_RESTARTS = 250;
		if((m_uLastOnFootRestartTime + MIN_TIME_BETWEEN_RESTARTS) < fwTimer::GetTimeInMilliseconds())
		{
			if(pChildTask)
			{
				fwMvClipSetId defaultClipSetId = GetDefaultOnFootClipSet();
				if(defaultClipSetId != pChildTask->GetMovementClipSetId())
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(defaultClipSetId);
					if(pClipSet && pClipSet->Request_DEPRECATED())
					{
						// Restart
						SetFlag(aiTaskFlags::RestartCurrentState);

						m_MoveNetworkHelper.SendRequest(ms_RestartOnFootId);
						m_uLastOnFootRestartTime = fwTimer::GetTimeInMilliseconds();
						return FSM_Continue;
					}
				}
			}
		}
	}

	return FSM_Continue;
}


void CTaskMotionPedLowLod::ApplyWeaponClipSet(CTaskMotionBasicLocomotionLowLod* pSubTask)
{
	taskAssert(pSubTask);

	const CPed* pPed = GetPed();

	//static fwMvFilterId s_filter("BothArms_filter",0x16F1D420);
	//fwMvFilterId filterId = s_filter;

	const CWeapon * pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pWeapon && pWeapon->GetWeaponInfo())
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
		pSubTask->SetWeaponClipSet(pWeaponInfo->GetPedMotionClipSetId(*pPed), pWeaponInfo->GetPedMotionFilterId(*pPed), pPed);
	}
	else
	{
		pSubTask->SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID, pPed);
	}
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::Swimming_OnEnter()
{
	CPedWeaponManager* pWepMgr = GetPed()->GetWeaponManager();
	if(pWepMgr)
	{
		pWepMgr->BackupEquippedWeapon();
		pWepMgr->DestroyEquippedWeaponObject(false);
		pWepMgr->EquipBestWeapon();
	}

	// Start the subtask
	CTaskMotionSwimming* pTask = rage_new CTaskMotionSwimming(CTaskMotionSwimming::State_Idle);	

	m_MoveNetworkHelper.SendRequest(ms_SwimmingId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterSwimmingId, ms_SwimmingNetworkId);

	m_wantsToDiveUnderwater = false;

	SetNewTask(pTask);
	return FSM_Continue;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::Swimming_OnUpdate()
{
	if(!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	bool isSwimming = pPed->GetIsSwimming();
	if(!isSwimming)
	{
		// Need to switch to an on foot state		
		TaskSetState(State_OnFoot);
	}

	return FSM_Continue;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::InVehicle_OnEnter()
{
	// Start the subtask
	CTaskMotionInVehicle* pTask = rage_new CTaskMotionInVehicle();

	m_MoveNetworkHelper.SendRequest(ms_InVehicleId);
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterInVehicleId, ms_InVehicleNetworkId);

	m_isFullyInVehicle = false;

	SetNewTask(pTask);
	return FSM_Continue;
}

CTaskMotionPedLowLod::FSM_Return CTaskMotionPedLowLod::InVehicle_OnUpdate()
{
	// If we went straight to the vehicle state, we were probably warped into the vehicle
	if(!m_isFullyInVehicle)
	{
		if(GetPreviousState() == State_Start || m_MoveNetworkHelper.GetBoolean(ms_OnExitOnFootId))
		{
			m_isFullyInVehicle = true;
		}
	}

	if(!IsMoveNetworkHelperInTargetState())
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Add some emergency transition logic here in case we're suddenly out of a vehicle, or the subtask has
	// aborted, this can happen for network clones in laggy network conditions
	if((!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || !pPed->GetMyVehicle()) || !GetSubTask())
	{
		TaskSetState(State_OnFoot);
	}

	return FSM_Continue;
}

bool CTaskMotionPedLowLod::IsClonePedNetworkStateChangeAllowed(u32 const UNUSED_PARAM(state))
{
	if(GetState() == State_Start)
	{
		if(!m_MoveNetworkHelper.GetNetworkPlayer())
		{
			return false;
		}
	}

	return true;
}

bool CTaskMotionPedLowLod::IsClonePedMoveNetworkStateTransitionAllowed(u32 const targetState) const
{
	// Based on anim\move\networks\pedmotionlowlod.mxtf

	u32 const currentState = GetState();

	switch(currentState)
	{
	case State_Start:				return ((targetState == State_OnFoot) || (targetState == State_Swimming) || (targetState == State_InVehicle));
	case State_OnFoot:				return ((targetState == State_Swimming) || (targetState == State_InVehicle) || (targetState == State_OnFoot));
	case State_Swimming:			return (targetState == State_OnFoot);
	case State_InVehicle:			return (targetState == State_OnFoot);
	case State_Exit:				return false;
	}

	return false;
}

bool CTaskMotionPedLowLod::GetStateMoveNetworkRequestId(u32 const state, fwMvRequestId& requestId) const
{
	switch(state)
	{
		case State_OnFoot:		requestId = ms_OnFootId;	return true;		break;
		case State_Swimming:	requestId = ms_SwimmingId;	return true;		break;
		case State_InVehicle:	requestId = ms_InVehicleId;	return true;		break;
		case State_Start:				
		case State_Exit:	
		default:
			break;
	}	

	return false;
}

s32  CTaskMotionPedLowLod::ConvertMoveNetworkStateAndClipSetLod(u32 const state, fwMvClipSetId& pendingClipSetId) const
{
	Assert(state <= CTaskMotionPed::State_Exit);

	// if the state is State_Diving, the pendingClipSet is move_ped_diving and we don't want to set
	// it on low lod as the clip set doesn't contain anims used by OnFootHumanLowLod so we null it.
	if(state == CTaskMotionPed::State_Diving)
	{
		pendingClipSetId = CLIP_SET_ID_INVALID;
	}

	switch(state)
	{
		case CTaskMotionPed::State_Start:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Exit:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_OnFoot:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Strafing:		return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_StrafeToOnFoot:	return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_SwimToRun:		return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_DoNothing:		return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_AnimatedVelocity:return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Crouch:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Stealth:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Aiming:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_StandToCrouch:	return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_CrouchToStand:	return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_ActionMode:		return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Parachuting:		return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Jetpack:			return CTaskMotionPedLowLod::State_OnFoot;		break;
#if ENABLE_DRUNK
		case CTaskMotionPed::State_Drunk:			return CTaskMotionPedLowLod::State_OnFoot;		break;
#endif // ENABLE_DRUNK
		case CTaskMotionPed::State_Dead:			return CTaskMotionPedLowLod::State_OnFoot;		break;
		case CTaskMotionPed::State_Diving:			return CTaskMotionPedLowLod::State_Swimming;	break;
		case CTaskMotionPed::State_Swimming:		return CTaskMotionPedLowLod::State_Swimming;	break;
		case CTaskMotionPed::State_DiveToSwim:		return CTaskMotionPedLowLod::State_Swimming;	break;
		case CTaskMotionPed::State_InVehicle:		return CTaskMotionPedLowLod::State_InVehicle;	break;
		default:
			break;
	}	

	return -1;
}

void CTaskMotionPedLowLod::TaskSetState(u32 const iState)
{
	aiTask::SetState(iState);
}
