// File header
#include "Task/Movement/Jumping/TaskJump.h"

// Game headers
#include "audio/speechaudioentity.h"
#include "audio/speechmanager.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/System/MotionTaskData.h"

#include "math/angmath.h"

#include "Event/Event.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "animation/MovePed.h"
#include "modelinfo/PedModelInfo.h"
#include "event/EventReactions.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskFall.h"
#include "Debug/debugscene.h"
#include "Camera/CamInterface.h"

#include "GapHelper.h"

// Disables optimisations if active
AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

static const float MAX_JUMP_ACCEL_LIMIT = 140.0f;

//////////////////////////////////////////////////////////////////////////
// CTaskJumpVault
//////////////////////////////////////////////////////////////////////////

CTaskJumpVault::CTaskJumpVault(fwFlags32 iFlags)
: m_iFlags(iFlags)
, m_fRate(1.f)
{
	SetInternalTaskType(CTaskTypes::TASK_JUMPVAULT);
}

void CTaskJumpVault::CleanUp()
{
	m_ClipSetRequestHelperForBase.Release();
	m_ClipSetRequestHelperForLowLod.Release();
	m_ClipSetRequestHelperForSkydiving.Release();
	m_ClipSetRequestHelperForParachuting.Release();

	m_ModelRequestHelper.Release();

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		const CWeaponInfo* pWeaponInfo = GetPed()->GetEquippedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
		{
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_WasFPSJumpingWithProjectile, true);
		}
	}
#endif
}

CTaskJumpVault::FSM_Return CTaskJumpVault::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	ProcessStreamingForParachute();

	// Notify the ped we are jumping
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);

	return FSM_Continue;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit();
		FSM_State(State_Jump)
			FSM_OnEnter
				return StateJumpOnEnter();
			FSM_OnUpdate
				return StateJumpOnUpdate();
		FSM_State(State_Vault)
			FSM_OnEnter
				return StateVaultOnEnter();
			FSM_OnUpdate
				return StateVaultOnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}



#if !__FINAL
const char * CTaskJumpVault::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Jump",
		"Vault",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskJumpVault::WillVault(CPed* pPed, fwFlags32 iFlags)
{
	if(!iFlags.IsFlagSet(JF_DisableVault) && pPed)
	{
		bool bUseHandhold = false;

		if(iFlags.IsFlagSet(JF_UseCachedHandhold))
		{
			bUseHandhold = true;
		}
		// If we are being forced to vault, but are not doing detection, force detection now
		else if(iFlags.IsFlagSet(JF_ForceVault) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb))
		{
			// Force check for climb if we are not currently detecting
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
			pPed->GetPedIntelligence()->GetClimbDetector().Process();
			bUseHandhold = true;
		}

		if(bUseHandhold)
		{
			CClimbHandHoldDetected handHoldDetected;
			if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
			{
				return true;
			}
		}
	}

	return false;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::StateInit()
{
	// Get the ped ptr.
	CPed* pPed = GetPed();

	if( WillVault(pPed, m_iFlags) )
	{
		SetState(State_Vault);
		return FSM_Continue;
	}

	// If jumping is disabled then quit
	if(m_iFlags.IsFlagSet(JF_DisableJumpingIfNoClimb))
	{
		SetState(State_Quit);	
		return FSM_Continue;
	}

	// Otherwise default to jump
	SetState(State_Jump);
	return FSM_Continue;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::StateJumpOnEnter()
{
	SetNewTask(rage_new CTaskJump(m_iFlags));
	return FSM_Continue;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::StateJumpOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::StateVaultOnEnter()
{
	// Get the ped ptr.
	CPed* pPed = GetPed();

	CClimbHandHoldDetected handHoldDetected;
	if(taskVerifyf(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected), "No hand hold for vault task"))
	{
		fwFlags8 iVaultFlags;
		if(m_iFlags.IsFlagSet(JF_AutoJump))
		{
			iVaultFlags.SetFlag(VF_RunningJumpVault);
		}

		if(pPed->GetPedIntelligence()->GetClimbDetector().IsDetectedHandholdFromAutoVault())
		{
			iVaultFlags.SetFlag(VF_AutoVault);
		}

		if(!pPed->GetPedIntelligence()->GetClimbDetector().CanOrientToDetectedHandhold())
		{
			iVaultFlags.SetFlag(VF_DontOrientToHandhold);
		}

		if(m_iFlags.IsFlagSet(JF_FromCover))
		{
			iVaultFlags.SetFlag(VF_VaultFromCover);
		}

		CTaskVault* pTaskVault = rage_new CTaskVault(handHoldDetected, iVaultFlags);
		pTaskVault->SetRateMultiplier(m_fRate);
		SetNewTask(pTaskVault);
		return FSM_Continue;
	}

	return FSM_Quit;
}

CTaskJumpVault::FSM_Return CTaskJumpVault::StateVaultOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// If we are not standing, jump
		return FSM_Quit;
	}

	return FSM_Continue;
}

bool CTaskJumpVault::GetIsFalling() {
	CTask* pJumpTask = GetSubTask();
	if (pJumpTask && pJumpTask->GetTaskType() == CTaskTypes::TASK_JUMP)
	{
		return static_cast<CTaskJump*>(pJumpTask)->GetIsFalling();
	}
	return false;
}

bool CTaskJumpVault::CanBreakoutToMovementTask() const
{
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_JUMP)
		{
			return static_cast<const CTaskJump*>(pSubTask)->CanBreakoutToMovementTask();
		}
		else if(pSubTask->GetTaskType() == CTaskTypes::TASK_VAULT) 
		{
			return static_cast<const CTaskVault*>(pSubTask)->CanBreakoutToMovementTask();
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskJumpVault::ProcessStreamingForParachute()
{
	//Ensure the ped is the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Stream in if we are forcing a parachute jump.
	bool bStreamIn = (m_iFlags.IsFlagSet(JF_ForceParachuteJump));

	//Stream in if we're doing a clamber vault.
	if(!bStreamIn)
	{
		bStreamIn = (GetState() == State_Vault) && GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_VAULT) &&
			(GetSubTask()->GetState() == CTaskVault::State_ClamberVault);
	}

	//Don't stream in if we're parachuting -- the task handles more complex streaming.
	if(bStreamIn)
	{
		if(GetPed()->GetIsParachuting())
		{
			bStreamIn = false;
		}
	}

	if(bStreamIn)
	{
		m_ClipSetRequestHelperForBase.Request(CLIP_SET_SKYDIVE_BASE);
		m_ClipSetRequestHelperForLowLod.Request(CLIP_SET_SKYDIVE_LOW_LOD);
		m_ClipSetRequestHelperForSkydiving.Request(CLIP_SET_SKYDIVE_FREEFALL);
		m_ClipSetRequestHelperForParachuting.Request(CLIP_SET_SKYDIVE_PARACHUTE);

		if(!m_ModelRequestHelper.IsValid())
		{
			fwModelId iModelId;
			CModelInfo::GetBaseModelInfoFromHashKey(CTaskParachute::GetModelForParachute(GetPed()), &iModelId);
			if(taskVerifyf(iModelId.IsValid(), " Parachute ModelId is invalid"))
			{
				strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
				m_ModelRequestHelper.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
			}
		}
	}
	else
	{
		m_ClipSetRequestHelperForBase.Release();
		m_ClipSetRequestHelperForLowLod.Release();
		m_ClipSetRequestHelperForSkydiving.Release();
		m_ClipSetRequestHelperForParachuting.Release();

		m_ModelRequestHelper.Release();
	}
}

//////////////////////////////////////////////////////////////////////////
// CTaskJump - Started out life as CTaskJumpLaunch
//////////////////////////////////////////////////////////////////////////

// NOTE: Launch and In-Air are now contained within the same clip 

const fwMvRequestId CTaskJump::ms_EnterLandId("EnterLand",0xA05ECA5B);					// Set true when we enter Land
const fwMvBooleanId CTaskJump::ms_ExitLaunchId("ExitLaunch",0xC5DDBCB4);				// Set true when the launch state has exited
const fwMvBooleanId CTaskJump::ms_AssistedJumpLandPhaseId("AssistedJumpLandPhase",0x442572d1);	// Set true when ideal land is hit.
const fwMvBooleanId CTaskJump::ms_ExitLandId("ExitLand",0x57C11B06);					// Set true when we exit Land
const atHashString	CTaskJump::ms_RagdollOnCollisionId("RagdollOnCollision",0xFA11B561); // Set true when we can ragdoll on collision
const fwMvBooleanId CTaskJump::ms_TakeOffId("TakeOff",0x210E0F0B);						// Set true when leave the ground
const fwMvBooleanId CTaskJump::ms_EnableLegIKId("EnableLegIK",0x864F50D5);				// Set to true to re-enable leg IK at an appropriate point.

const fwMvFlagId	CTaskJump::ms_StandingLaunchFlagId("bStandingLaunch",0x834F3021);	// A Flag to tell the network to use the standing launch clip (otherwise it's the moving launch)
const fwMvFlagId	CTaskJump::ms_StandingLandFlagId("bStandingLand",0xFAA107A3);
const fwMvFlagId	CTaskJump::ms_SkydiveLaunchFlagId("bSkydiveLaunch",0x2EEC9787);	// A Flag to tell the network whether to use the skydive launch
const fwMvFlagId	CTaskJump::ms_DivingLaunchFlagId("bDivingLaunch",0x85295A80);		// A Flag to tell the network whether to use the diving launch
const fwMvFlagId	CTaskJump::ms_LeftFootLaunchFlagId("bLeftFootLaunch",0xE4031C6);	// A Flag to tell the network which moving launch to use
const fwMvFlagId	CTaskJump::ms_FallingFlagId("bFalling",0x35840204);
const fwMvFlagId	CTaskJump::ms_SkipLaunchFlagId("bSkipLaunch",0x2D691230);
const fwMvFlagId	CTaskJump::ms_AutoJumpFlagId("bAutoJump",0x38F7D3C3);
const fwMvFlagId	CTaskJump::ms_ForceRightFootLandFlagId("bForceRightFootLand",0x010c0301);
const fwMvFlagId	CTaskJump::ms_UseProjectileGripFlagId("bProjectileGrip",0xbb5a470f);
const fwMvBooleanId CTaskJump::ms_InterruptibleId("Interruptible",0x550A14CF);
const fwMvBooleanId CTaskJump::ms_SlopeBreakoutId("SlopeBreakout",0x17a750c7);
const fwMvBooleanId CTaskJump::ms_AimInterruptId("AimInterrupt",0x12398EF2);
const fwMvBooleanId CTaskJump::ms_StandLandWalkId("StandLandWalk",0xE2ECACA4);
const fwMvClipId	CTaskJump::ms_JumpClipId("JumpClip",0xB0C5C4BB);
const fwMvFloatId	CTaskJump::ms_LaunchPhaseId("JumpLaunchPhase",0x75D18CB0);
const fwMvFloatId	CTaskJump::ms_MoveSpeedId("Speed",0xF997622B);

const fwMvFloatId	CTaskJump::ms_TurnPhaseLaunchId("TurnPhaseLaunch",0xa05de367);
const fwMvFloatId	CTaskJump::ms_TurnPhaseId("TurnPhase",0x9b1de516);

const fwMvFloatId	CTaskJump::ms_PredictedJumpDistanceId("PredictedJumpDistance",0x2E17C49B);
const fwMvFlagId	CTaskJump::ms_PredictingJumpId("bPredictingJump",0xE3FF6FAD);
const fwMvFlagId	CTaskJump::ms_JumpingUpHillId("bJumpingUpHill",0x263AA42);
const fwMvFlagId	CTaskJump::ms_ScalingJumpId("bScalingJump",0x6C826C3C);

const fwMvClipSetVarId	CTaskJump::ms_UpperBodyClipSetId("UpperBodyClipSet",0x49BB9318);

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskJump, 0x4a3f4351)

CTaskJump::Tunables CTaskJump::sm_Tunables;

#define TASKNEWJUMP_BLEND_IN_DURATION	(NORMAL_BLEND_DURATION)
#define TASKNEWJUMP_BLEND_OUT_DURATION	(0.35f)
#define TASKNEWJUMP_BLEND_OUT_DURATION_STANDING	(0.2f)
#define TASKNEWJUMP_QUADRUPED_BLEND_OUT_DURATION	(SLOW_BLEND_DURATION)

bool CTaskJump::IS_FOOTSYNC_ENABLED()
{
	TUNE_GROUP_BOOL(PED_JUMPING, bJumpLaunchFootSync ,false);
	return bJumpLaunchFootSync;
}

CTaskJump::CTaskJump(fwFlags32 iFlags)
: m_iFlags(iFlags)
, m_IsQuadrupedJump(false)
, m_bSkydiveLaunch(false)
, m_bDivingLaunch(false)
, m_bLeftFootLaunch(false)
, m_bIsFalling(false)
, m_bStandingJump(false)
, m_bStandingLand(false)
, m_fBackDist(0.0f)
, m_fClipLastPhase(0.0f)
, m_bScalingVelocity(false)
, m_fScaleVelocityStartPhase(0.0f)
, m_vScaleDistance(Vector3::ZeroType)
, m_vHandHoldPoint(Vector3::ZeroType)
, m_vInitialFallVelocity(Vector3::ZeroType)
, m_vAnimRemainingDist(Vector3::ZeroType)
, m_vGroundPosStart(Vector3::ZeroType)
, m_vOwnerAssistedJumpStartPosition(Vector3::ZeroType)
, m_vOwnerAssistedJumpStartPositionOffset(Vector3::ZeroType)
, m_vMaxScalePhase(1.0f, 1.0f, 1.0f)
, m_fQuadrupedJumpDistance(0.0f)
, m_fPedLandPosZ(0.0f)
, m_RagdollStartTime(0)
, m_bReactivateLegIK(false)
, m_bWaitingForJumpRelease(false)
, m_bTwoHandedJump(false)
, m_bUseLeftFoot(false)
, m_bTagSyncOnExit(true)
, m_bStuntJumpRequested(false)
, m_bJumpingUpwards(false)
, m_UpperBodyClipSetID(CLIP_SET_ID_INVALID)
, m_fApexClipPhase(0.5f)
, m_fInitialPedZ(0.0f)
, m_fJumpAngle(0.0f)
, m_fTurnPhaseLaunch(0.0f)
, m_fTurnPhase(0.0f)
, m_bUseProjectileGrip(false)
#if FPS_MODE_SUPPORTED
, m_fFPSInitialDesiredHeading(FLT_MAX)
, m_fFPSExtraHeadingChange(0.0f)
, m_bFPSAchievedInitialDesiredHeading(false)
#endif // FPS_MODE_SUPPORTED
, m_bStayStrafing(false)
, m_bUseMaximumSuperJumpVelocity(false)
, m_bHasAppliedSuperJumpVelocityLocally(false)
, m_bHasAppliedSuperJumpVelocityRemotely(false)
{
	SetInternalTaskType(CTaskTypes::TASK_JUMP);
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::ProcessPreFSM()
{
	CPed *pPed = GetPed();

    if(pPed && pPed->IsNetworkClone())
    {
        if(m_iFlags.IsFlagSet(JF_SuperJump))
        {
            if(GetState() >= State_Launch)
            {
                bool bFallLanding = false;
                if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_FALL)
                {
                    CTaskFall *pTaskFall = static_cast<CTaskFall*>(GetSubTask());
                    bFallLanding = pTaskFall->GetState()==CTaskFall::State_Landing || pTaskFall->GetState()==CTaskFall::State_GetUp;
                }

                if(!bFallLanding)
                {
                    NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
                }
            }
        }
    }

	if(m_iFlags.IsFlagSet(JF_SuperJump))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableJumpRagdollOnCollision, true);
	}

	static dev_u8 s_uNumFramesSetEntityZ = 5;

	if(m_IsQuadrupedJump)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeHorseMbr, true);
	}

	// Disallow jumping again till at least next frame.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePlayerJumping, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);
	// No gait reduction - capsule gets caught on small steps, stopping ped jumping forwards. 
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableGaitReduction, true);

	bool bInFpsMode = false;
#if FPS_MODE_SUPPORTED
	bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	if(bInFpsMode)
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableIndependentMoverFrame, true);

	bool isPredictiveBracing = false;

	bool bFallLanding = false;
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_FALL)
	{
		CTaskFall *pTaskFall = static_cast<CTaskFall*>(GetSubTask());
		bFallLanding = pTaskFall->GetState()==CTaskFall::State_Landing || pTaskFall->GetState()==CTaskFall::State_GetUp;
	}

	// Notify the ped we are jumping or landing.

	// Note: Don't enter ragdoll if we have re-entered fall from a land for a small period of time.
	bool bFallStateValidForRagdoll = GetState() == State_Fall && (GetPreviousState() != State_Land || GetTimeInState() > 2.0f) && !bFallLanding;

	if(GetState() == State_Launch || bFallStateValidForRagdoll)
	{
		if (!pPed->GetUsingRagdoll() && 
			pPed->IsLocalPlayer() && 
			GetTimeRunning()>Min(sm_Tunables.m_PredictiveRagdollStartDelay, sm_Tunables.m_PredictiveBraceStartDelay) && 
			(NetworkInterface::IsGameInProgress() ? sm_Tunables.m_bEnableJumpCollisionsMp : sm_Tunables.m_bEnableJumpCollisions))
		{
			float distance = 0.0f;
			CEntity* pObstruction = NULL;
			Vec3V normal;
			if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL) && TestForObstruction(Max(sm_Tunables.m_PredictiveBraceProbeLength, sm_Tunables.m_PredictiveRagdollProbeLength), sm_Tunables.m_PredictiveRagdollProbeRadius, distance, &pObstruction, normal) )
			{
				float fVelocity = pPed->GetVelocity().Mag();
				
				//! Take relative velocity into account.
				if(pPed->GetGroundPhysical())
				{
					fVelocity -= pPed->GetGroundPhysical()->GetVelocity().Mag();
				}

				bool isPed = pObstruction && pObstruction->GetIsTypePed();
				bool bPassedRagdollVelTest = fVelocity > (sm_Tunables.m_PredictiveRagdollRequiredVelocityMag);

				// Don't brace/ragdoll if doing super jump (url:bugstar:2577123).
				if((isPed || bPassedRagdollVelTest) && !m_iFlags.IsFlagSet(JF_SuperJump))
				{
					if (distance < sm_Tunables.m_PredictiveRagdollProbeLength && GetTimeRunning()>sm_Tunables.m_PredictiveRagdollStartDelay && !pPed->GetPedIntelligence()->GetEventGroup().HasRagdollEvent())
					{
						// start the ragdoll behaviour
						bool bRagdoll = true;
						if (isPed && sm_Tunables.m_bBlockJumpCollisionAgainstRagdollBlocked)
						{ 
							CPed* pOtherPed = static_cast<CPed*>(pObstruction);
							// Don't ragdoll against peds who block ragdoll against you. Stops us bouncing off ragdoll blocked peds in odd ways.
							if (!CTaskNMBehaviour::CanUseRagdoll(pOtherPed, RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL, pPed))
							{
								bRagdoll = false;
							}
						}

						if (bRagdoll)
						{
							CTaskNMHighFall* pTask = rage_new CTaskNMHighFall(100, NULL, isPed ? CTaskNMHighFall::HIGHFALL_STUNT_JUMP : CTaskNMHighFall::HIGHFALL_JUMP_COLLISION);
							CEventSwitch2NM event(10000, pTask, false, 100);
							pPed->SwitchToRagdoll(event);

							// When the player bumps into another ped like this, fire a melee shocking event so other peds nearby will react.
							if (isPed)
							{
								CEventShockingPedKnockedIntoByPlayer shockingEvent(*pPed, pObstruction);
								CShockingEventsManager::Add(shockingEvent);
							}
							else
							{
								pPed->GetPedAudioEntity()->HandleJumpingImpactRagdoll();
							}
						}

						return FSM_Continue;
					}
					else if (distance < sm_Tunables.m_PredictiveBraceProbeLength  && GetTimeRunning()>sm_Tunables.m_PredictiveBraceStartDelay && !pPed->GetWeaponManager()->GetIsArmed2Handed() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
					{
						bool doBrace = true;
						if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SlopeDetected))
						{
							// don't trigger when jumping up slopes
							Vec3V up(0.0f, 0.0f, 1.0f);
							if (Dot(normal, up).Getf()>sm_Tunables.m_PredictiveBraceMaxUpDotSlope)
							{
								doBrace = false;
							}
						}

						if (doBrace)
						{
							isPredictiveBracing = true;

							// blend in the secondary brace anim
							if (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()==NULL &&!m_BraceClipHelper.IsAnimating())
							{
								CMovePed& move = pPed->GetMovePed();

								// get the collision brace clip set
								fwClipSet* pSet = fwClipSetManager::GetClipSet(CLIP_SET_JUMP_COLLISION_BRACE);
								if (pSet)
								{
									// pick a random clip from the set
									fwMvClipId clipId;
									if (pSet->PickRandomClip(clipId))
									{
										// Grab the requested filter from the clip item
										fwMvFilterId filter(BONEMASK_ALL);
										const fwClipItem* pItem = pSet->GetClipItem(clipId);
										if (pItem && pItem->IsClipItemWithProps())
										{
											const fwClipItemWithProps* pItemWithProps = static_cast<const fwClipItemWithProps*>(pItem);
											filter.SetHash(pItemWithProps->GetBoneMask().GetHash());
										}

										// note blend in and out times in this call are ignored as we're not using the autoInsert option.
										m_BraceClipHelper.BlendInClipBySetAndClip(pPed, CLIP_SET_JUMP_COLLISION_BRACE, clipId, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, true, false, false);
										move.SetSecondaryTaskNetwork(m_BraceClipHelper, sm_Tunables.m_PredictiveBraceBlendInDuration, CMovePed::Secondary_None, filter);
									}
								}
							}
						}					
					}
				}
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsJumping, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

		if(m_bDivingLaunch)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_IsDiving, true);
		}
	}
	else if(GetState() == State_Land)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_IsLanding, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

		//! Force ped to process steep slope calculation.
		pPed->GetMotionData()->SetForceSteepSlopeTestThisFrame(true);
	}
	else if(GetState() == State_Init && bInFpsMode)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsJumping, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);
	}

	if (!isPredictiveBracing && m_BraceClipHelper.IsNetworkActive())
	{
		// blend out the secondary brace anim
		CMovePed& move = pPed->GetMovePed();
		move.ClearSecondaryTaskNetwork(m_BraceClipHelper, sm_Tunables.m_PredictiveBraceBlendOutDuration);
		m_BraceClipHelper.ReleaseNetworkPlayer();
	}

	m_BraceClipHelper.Update();

	if( GetState() == State_Launch )
	{
		// Tell the task to call ProcessPhysics
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
		
		if(m_IsQuadrupedJump)
		{
			if(HasReachedApex() && m_iFlags.IsFlagSet(JF_ScaleQuadrupedJump) && IsPedNearGround(pPed, GetJumpLandThreshold()))
			{
				pPed->m_PedResetFlags.SetEntityZFromGround( s_uNumFramesSetEntityZ );
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessProbesWhenExtractingZ, true);
			}
		}
		else
		{
			Vector3 vOffset(Vector3::ZeroType);
			float fLandZ = 0.0f;
			if(HasReachedApex() && !m_iFlags.IsFlagSet(JF_SteepHill) && IsPedNearGround(pPed, GetJumpLandThreshold(), vOffset, fLandZ))
			{

				//@@: location CTASKJUMP_PROCESSPREFSM_HAS_REACHED_APEX
				if(CanSnapToGround(pPed, fLandZ))
				{
					pPed->m_PedResetFlags.SetEntityZFromGroundZHeight(fLandZ, GetEntitySetZThreshold());
					pPed->m_PedResetFlags.SetEntityZFromGround( s_uNumFramesSetEntityZ );
				}
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessProbesWhenExtractingZ, true);
			}

			if(pPed->GetMovePed().IsTaskNetworkFullyBlended() && pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
			}
		}

		if(m_bScalingVelocity) 
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
		}

		//! Force ped to process steep slope calculation.
		pPed->GetMotionData()->SetForceSteepSlopeTestThisFrame(true);
	}

	if(m_IsQuadrupedJump && GetState() == State_Fall)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

		if(IsPedNearGround(pPed, GetJumpLandThreshold()))
		{
			pPed->m_PedResetFlags.SetEntityZFromGround( s_uNumFramesSetEntityZ );
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessProbesWhenExtractingZ, true);
		}
	}

#if __BANK
	//! Ensure we aren't receiving a land event out of land state.
	if( GetState() != State_Land && m_networkHelper.IsNetworkActive() )
	{
		if( m_networkHelper.GetBoolean(ms_ExitLandId) == true )
		{
			taskAssert(0);
			return FSM_Continue;
		}
	}
#endif

	const CControl* pControl = GetControlFromPlayer(pPed);
	if(pControl && pControl->GetPedJump().IsReleased()) 
	{
		m_bWaitingForJumpRelease = false;
	}

	//Prevent scooting up steep slopes.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_EnableSteepSlopePrevention, true);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnEnter
				return StateInit_OnEnter(pPed);
			FSM_OnUpdate
				return StateInit_OnUpdate(pPed);
		FSM_State(State_Launch)
			FSM_OnEnter
				return StateLaunch_OnEnter(pPed);
			FSM_OnUpdate
				return StateLaunch_OnUpdate(pPed);
			FSM_OnExit
				return StateLaunch_OnExit(pPed);
		FSM_State(State_Land)
			FSM_OnEnter
				return StateLand_OnEnter(pPed);
			FSM_OnUpdate
				return StateLand_OnUpdate(pPed);
			FSM_OnExit
				return StateLand_OnExit(pPed);
		FSM_State(State_Fall)
			FSM_OnEnter
				return StateFall_OnEnter(pPed);
			FSM_OnUpdate
				return StateFall_OnUpdate(pPed);
			FSM_OnExit
				return StateFall_OnExit(pPed);

		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::ProcessPostFSM()
{
	CPed *pPed = GetPed();

	bool bFirstPerson = false;
#if FPS_MODE_SUPPORTED
	bFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true);
	if(bFirstPerson && GetState() != State_Fall)
	{
		pPed->SetIsStrafing(m_bStayStrafing);
	}
#endif // FPS_MODE_SUPPORTED

	if(m_networkHelper.GetBoolean(ms_EnableLegIKId))
	{
		m_bReactivateLegIK = true;
	}

	if(GetState() != State_Fall)
	{
		//! Note: Don't disable IK. Let Fall handle it.
		if(m_bOffGround)
		{
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

			if(!m_bReactivateLegIK)
			{
				pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);
			}
			else
			{
				pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
			}
		}

		TUNE_GROUP_FLOAT(PED_JUMPING,fJumpLegIKBlend,0.1f, 0.0f, 5.0f, 0.01f);
		TUNE_GROUP_FLOAT(PED_JUMPING,fJumpLegIKBlendUpward,0.2f, 0.0f, 5.0f, 0.01f);
		pPed->GetIkManager().SetOverrideLegIKBlendTime((HasReachedApex() || GetState() == State_Land) ?  fJumpLegIKBlend : fJumpLegIKBlendUpward);

		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpPreTakeoffHeadingModifier, 0.5f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpPreApexHeadingModifier, 0.5f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpPostApexHeadingModifier, 0.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpMovingLandHeadingModifier, 2.5f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_JUMPING, fJumpStandingLandHeadingModifier, 0.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(PED_JUMPING, fFirstPersonJumpStandingLandHeadingModifier, 0.35f, 0.0f, 10.0f, 0.1f);

		float fHeadingModifier = 0.0f;

		if(GetState() == State_Land)
		{
			if(bFirstPerson)
			{
				fHeadingModifier = fFirstPersonJumpStandingLandHeadingModifier;
			}
			else
			{
				fHeadingModifier = m_bStandingLand ? fJumpStandingLandHeadingModifier : fJumpMovingLandHeadingModifier;
			}
		}
		else if(!m_bDivingLaunch && !m_bSkydiveLaunch)
		{
			if(HasReachedApex())
			{
				fHeadingModifier = fJumpPostApexHeadingModifier;
			}
			else
			{
				fHeadingModifier = m_bOffGround ? fJumpPreApexHeadingModifier : fJumpPreTakeoffHeadingModifier;
			}
		}

		if(m_iFlags.IsFlagSet(JF_SuperJump))
		{
			fHeadingModifier = CTaskFall::sm_Tunables.m_InAirHeadingRate;
		}

		CTaskFall::UpdateHeading(pPed, GetControlFromPlayer(pPed), fHeadingModifier);

#if FPS_MODE_SUPPORTED
		if(bFirstPerson && GetTimeRunning() > GetTimeStep())
		{
			if(!pPed->IsNetworkClone())
			{
				if(m_fFPSInitialDesiredHeading == FLT_MAX)
				{
					m_fFPSInitialDesiredHeading = pPed->GetDesiredHeading();
				}
			}

			// Apply some extra steering here
			float fCurrentHeading = pPed->GetCurrentHeading();
			float fDesiredHeading = pPed->GetDesiredHeading();

			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_JUMP_HEADING_LERP, 1.0f, 0.0f, 20.0f, 0.1f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_JUMP_HEADING_LERP_INITIAL, 4.0f, 0.0f, 20.0f, 0.1f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_JUMP_HEADING_INITIAL_TIME, 1.f, 0.0f, PI, 0.1f);
			TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_JUMP_HEADING_INITIAL_CLOSE_ENOUGH, 0.05f, 0.0f, PI, 0.1f);

			if(GetTimeRunning() < FPS_JUMP_HEADING_INITIAL_TIME && !m_bFPSAchievedInitialDesiredHeading)
			{
				float fToDesired = SubtractAngleShorter(fCurrentHeading, fDesiredHeading);
				float fToInitialDesired = SubtractAngleShorter(fCurrentHeading, m_fFPSInitialDesiredHeading);
				if(Abs(fToInitialDesired) < FPS_JUMP_HEADING_INITIAL_CLOSE_ENOUGH)
				{
					m_bFPSAchievedInitialDesiredHeading = true;
				}

				if(Sign(fToDesired) != Sign(fToInitialDesired))
				{
					m_bFPSAchievedInitialDesiredHeading = true;
				}

				m_fFPSExtraHeadingChange = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, FPS_JUMP_HEADING_LERP_INITIAL, true, GetTimeStep());
			}
			else
			{
				m_fFPSExtraHeadingChange = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, FPS_JUMP_HEADING_LERP, true, GetTimeStep());
			}

			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(m_fFPSExtraHeadingChange);

			// Stop the locomotion task from overriding the angular vel
			pPed->SetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity, true);
		}
#endif // FPS_MODE_SUPPORTED
	}

#if ENABLE_HORSE
	if(m_IsQuadrupedJump)
	{
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE)
		{
			m_fTurnPhase = static_cast<CTaskHorseLocomotion*>(pTask)->GetTurnPhase();
		}
	}
#endif

	// Send Move signals
	UpdateMoVE();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskJump::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed();

	if(GetState() == State_Launch)
	{
		bool bOverridingPhysics = false;

		bool bScaledVelocity = ScaleDesiredVelocity(pPed, fwTimer::GetTimeStep());

		// Handle if we are attached
		fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();
		if(extension && extension->GetIsAttached() && extension->GetAttachParent())
		{
			bOverridingPhysics = true;

			// Get the animated velocity of the ped
			Vector3 vAttachOffset;
			if(bScaledVelocity)
			{
				vAttachOffset = pPed->GetDesiredVelocity();
				vAttachOffset = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAttachOffset)));
			}
			else
			{
				vAttachOffset = pPed->GetAnimatedVelocity();
			}

			Vec3V vPositionalDelta = VECTOR3_TO_VEC3V(vAttachOffset * fTimeStep);

			Vec3V vPosAttachOffset = VECTOR3_TO_VEC3V(extension->GetAttachOffset());
			QuatV qRotAttachOffset = QUATERNION_TO_QUATV(extension->GetAttachQuat());

			vPosAttachOffset += Transform(qRotAttachOffset, vPositionalDelta);

			// Set the attach offset
			extension->SetAttachOffset(RCC_VECTOR3(vPosAttachOffset));
		}
			
		pPed->CacheGroundVelocityForJump();

		//! Try and straighten horse up whilst collision is disabled. The collision is disabled whilst we are scaling the velocity vertically.
		if(m_IsQuadrupedJump && !HasReachedApex())
		{
			static float fPitchSpeedMul = 2.5f;
			const float fPitchDelta = pPed->GetDesiredPitch() - pPed->GetCurrentPitch();
			pPed->GetMotionData()->SetExtraPitchChangeThisFrame(fPitchDelta * fwTimer::GetTimeStep() * fPitchSpeedMul);
		}

		return bOverridingPhysics;
	}
	else if(GetState() == State_Fall)
	{
		if(m_IsQuadrupedJump)
		{
			Vector3 vDesired(m_vInitialFallVelocity.x, m_vInitialFallVelocity.y, m_vInitialFallVelocity.z);
			pPed->SetDesiredVelocityClamped(vDesired, MAX_JUMP_ACCEL_LIMIT);

			static dev_float s_fVelocityFalloff = 0.5f;
			m_vInitialFallVelocity.x *= (1.0f-(s_fVelocityFalloff*fTimeStep));
			m_vInitialFallVelocity.y *= (1.0f-(s_fVelocityFalloff*fTimeStep));
			m_vInitialFallVelocity.z += GRAVITY*fTimeStep;
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::CleanUp()
{
	CPed* pPed = GetPed();

	// Blend out the move network
	TUNE_GROUP_FLOAT(PED_JUMPING, fQuadrupedBlendOutDuration, SLOW_BLEND_DURATION, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fBlendOutDuration, TASKNEWJUMP_BLEND_OUT_DURATION, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fBlendOutDurationStanding, TASKNEWJUMP_BLEND_OUT_DURATION_STANDING, 0.0f, 5.0f, 0.01f);

	float blendTime = 0.0f;
	if(m_IsQuadrupedJump)
	{
		blendTime = fQuadrupedBlendOutDuration;
	}
	else
	{
		blendTime = m_bStandingJump ? fBlendOutDurationStanding : fBlendOutDuration;
	}

	//! Do footsync on transition out.
	pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, blendTime, m_bTagSyncOnExit ? CMovePed::Task_TagSyncTransition : 0);

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );

	pPed->SetUseExtractedZ(false);

	// Enable collision
	pPed->EnableCollision();

	pPed->GetIkManager().ResetOverrideLegIKBlendTime();

	pPed->ResetGroundVelocityForJump();

	pPed->GetMotionData()->SetGaitReductionBlockTime(); //block gait reduction while we ramp back up

	// Detach if we are attached
	if(pPed->GetIsAttached())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
	{
		CTaskHumanLocomotion *pHumanLocomotion = static_cast<CTaskHumanLocomotion*>(pTask);
		pHumanLocomotion->SetUseLeftFootTransition(m_bUseLeftFoot);
	}

	m_UpperBodyClipSetRequestHelper.Release();

	// Show weapon.
	if(m_bDivingLaunch)
	{
		CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pWeapon)
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}

	pPed->GetMovePed().ClearSecondaryTaskNetwork(m_BraceClipHelper, sm_Tunables.m_PredictiveBraceBlendOutDuration);

	ResetRagdollOnCollision();

	// Clear this when we quit
	bool bInFpsMode = false;
#if FPS_MODE_SUPPORTED
	bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED
	if(bInFpsMode)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, false);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsJumping, false);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipFPSUnHolsterTransition, true);
	}

	// Base class
	CTask::CleanUp();
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::IsMovingUpSlope(const Vector3 &vSlopeNormal) const
{
	float fSlopeHeading = rage::Atan2f(-vSlopeNormal.x, vSlopeNormal.y);
	if(abs(fSlopeHeading)>0.001f)
	{
		float fHeadingToSlope = GetPed()->GetCurrentHeading() - fSlopeHeading;
		fHeadingToSlope = fwAngle::LimitRadianAngleSafe(fHeadingToSlope);

		if (fHeadingToSlope < -HALF_PI ||  fHeadingToSlope > HALF_PI)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::CalculateJumpAngle()
{
	Vector3 vLandPosition;
	if(GetDetectedLandPosition(GetPed(), vLandPosition) && m_JumpHelper.WasNonAssistedSlopeFound())
	{
		Vector3 vSlope(vLandPosition - m_vGroundPosStart);
		vSlope.NormalizeFast();

		Vector3 vSlopeNormal;
		Vector3 vTempRight;
		vTempRight.Cross(vSlope, ZAXIS);
		vSlopeNormal.Cross(vTempRight, vSlope);
		vSlopeNormal.Normalize();
	
		m_fJumpAngle = acosf(vSlopeNormal.z) * RtoD;

		if(m_fJumpAngle > 0.0f)
		{
			if(IsMovingUpSlope(vSlopeNormal))
			{
				m_bJumpingUpwards = true;

				TUNE_GROUP_FLOAT(PED_JUMPING, fUpHillJumpAngleMin, 12.5f, 0.0f, 90.0f, 0.1f);
				TUNE_GROUP_FLOAT(PED_JUMPING, fUpHillJumpAngleMax, 47.5f, 0.0f, 90.0f, 0.1f);
				
				if(m_fJumpAngle > fUpHillJumpAngleMax)
				{
					m_iFlags.SetFlag(JF_SteepHill);
				}
				else if(m_fJumpAngle > fUpHillJumpAngleMin)
				{
					m_iFlags.SetFlag(JF_JumpingUpHill);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void	CTaskJump::UpdateMoVE()
{
	if(m_networkHelper.IsNetworkActive())
	{
		// Set any relevant MoVE signals here
		m_networkHelper.SetFlag(m_bStandingJump, ms_StandingLaunchFlagId);
		m_networkHelper.SetFlag(m_bStandingLand, ms_StandingLandFlagId);
		m_networkHelper.SetFlag(m_bSkydiveLaunch, ms_SkydiveLaunchFlagId);
		m_networkHelper.SetFlag(m_bDivingLaunch, ms_DivingLaunchFlagId);		
		m_networkHelper.SetFlag(m_bLeftFootLaunch, ms_LeftFootLaunchFlagId);
		m_networkHelper.SetFlag(m_bIsFalling, ms_FallingFlagId);		

		m_networkHelper.SetFlag(m_JumpHelper.WasLandingFound(), ms_PredictingJumpId);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_JumpingUpHill), ms_JumpingUpHillId);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_ScaleQuadrupedJump), ms_ScalingJumpId);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_ForceRunningLand), ms_SkipLaunchFlagId);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_AutoJump), ms_AutoJumpFlagId);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_ForceRightFootLand), ms_ForceRightFootLandFlagId);

		m_networkHelper.SetFloat(ms_PredictedJumpDistanceId, m_JumpHelper.GetHorizontalJumpDistance());

		float fSpeed = GetPed()->GetMotionData()->GetCurrentMbrY() / MOVEBLENDRATIO_SPRINT; // normalize the speed values between 0.0f and 1.0f
		m_networkHelper.SetFloat( ms_MoveSpeedId, fSpeed);

		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(JF_AutoJump), ms_AutoJumpFlagId);
		
		m_networkHelper.SetFloat(ms_TurnPhaseLaunchId, m_fTurnPhaseLaunch);
		m_networkHelper.SetFloat(ms_TurnPhaseId, m_fTurnPhase);

		if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsMeleeFist())
		{
			crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(m_UpperBodyClipSetID);
			if (pFilter)
			{
				m_networkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateInit_OnEnter(CPed* pPed)
{
	m_bOffGround = false;

	// Get the motion task data set to get our clipset for this pedtype
	CPedModelInfo *pModelInfo = pPed->GetPedModelInfo();
	const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pModelInfo->GetMotionTaskDataSetHash().GetHash());
	m_clipSetId	= fwMvClipSetId(pMotionTaskDataSet->GetOnFootData()->m_JumpClipSetHash.GetHash());	//CLIP_SET_NEW_PED_JUMP

	if(m_iFlags.IsFlagSet(JF_BeastJump))
	{
		const fwMvClipSetId BeastJumpClipSetId("MOVE_JUMP@BeastJump",0x25de4b09);
		m_clipSetId	= BeastJumpClipSetId; 
	}

	if( !taskVerifyf( m_clipSetId, "CTaskJump - No Jump clip set defined in motiontaskdata.meta for this pedtype: %s", pModelInfo->GetMotionTaskDataSetHash().GetCStr()) )
	{
		return FSM_Quit;
	}

	if(pPed->GetIsSwimming())
	{
		return FSM_Quit;
	}

	if(pPed->GetCurrentMotionTask() && pPed->GetCurrentMotionTask()->IsUnderWater())
	{
		return FSM_Quit;
	}

	if (pMotionTaskDataSet->GetOnFootData()->m_UseQuadrupedJump) 
	{
		m_IsQuadrupedJump = true;
	}

	if(IS_FOOTSYNC_ENABLED() || m_IsQuadrupedJump)
	{
		m_iFlags.SetFlag(JF_JumpLaunchFootSync);
	}

	m_networkHelper.RequestNetworkDef( fwMvNetworkDefId(pMotionTaskDataSet->GetOnFootData()->m_JumpNetworkName.c_str()));

	const CControl* pControl = GetControlFromPlayer(pPed);
	if(pControl && pControl->GetPedJump().IsPressed()) 
	{
		m_bWaitingForJumpRelease = true;
	}

	const CWeapon * pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pWeapon && pWeapon->GetWeaponInfo())
	{
		m_UpperBodyClipSetID = pWeapon->GetWeaponInfo()->GetJumpUpperbodyClipSetId(FPS_MODE_SUPPORTED_ONLY(*pPed));

		if(pWeapon->GetWeaponInfo()->GetIsProjectile() && m_UpperBodyClipSetID == CLIP_SET_ID_INVALID)
		{
			m_UpperBodyClipSetID = pWeapon->GetWeaponInfo()->GetWeaponClipSetId(); 
			m_bUseProjectileGrip = true;
		}

		m_UpperBodyClipSetRequestHelper.Request(m_UpperBodyClipSetID);
	}

	m_fInitialPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;

	m_vGroundPosStart = pPed->GetGroundPos();

	if(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING))
	{
		m_bStayStrafing = true;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateInit_OnUpdate(CPed* pPed)
{
	if(!m_networkHelper.IsNetworkActive())
	{
		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssertf(pSet, "Can't find clip set! See DMKH. Clip ID: %s", m_clipSetId.GetCStr());

		const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pPed->GetPedModelInfo()->GetMotionTaskDataSetHash().GetHash());
		taskAssert(pMotionTaskDataSet);

		fwMvNetworkDefId jumpNetwork(pMotionTaskDataSet->GetOnFootData()->m_JumpNetworkName.c_str());
		if (pSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(jumpNetwork))
		{
			m_networkHelper.CreateNetworkPlayer(pPed, jumpNetwork);
			m_networkHelper.SetClipSet(m_clipSetId);

			if(m_UpperBodyClipSetID != CLIP_SET_ID_INVALID && m_UpperBodyClipSetRequestHelper.Request(m_UpperBodyClipSetID))
			{
				m_networkHelper.SetClipSet(m_UpperBodyClipSetID, ms_UpperBodyClipSetId);

				if(m_bUseProjectileGrip)
				{
					static fwMvClipId fpClip("idle",0x71c21326);
					const crClip* pProjectileClip = fwClipSetManager::GetClip( m_UpperBodyClipSetID, fpClip );
					if(pProjectileClip)
					{
						m_networkHelper.SetFlag(true, ms_UseProjectileGripFlagId);
					}
				}
			}

			// Attach it to an empty precedence slot in fwMove
			Assert(pPed->GetAnimDirector());

			TUNE_GROUP_FLOAT(PED_JUMPING, fBlendInDuration, TASKNEWJUMP_BLEND_IN_DURATION, 0.0f, 5.0f, 0.01f);

			if(m_iFlags.IsFlagSet(JF_JumpLaunchFootSync))
			{
				pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), fBlendInDuration, CMovePed::Task_TagSyncTransition);
			}
			else
			{
				pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), fBlendInDuration);
			}
		}
	}

	if(m_networkHelper.IsNetworkActive())
	{
		if(m_iFlags.IsFlagSet(JF_ForceRunningLand))
		{
			// Perform a moving jump
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION && static_cast<CTaskHumanLocomotion*>(pTask)->UseLeftFootTransition())
			{
				// The last foot down was right, so we need to launch from the left foot
				m_bLeftFootLaunch = true;
			}
			else
			{
				// The last foot down was left, so we need to launch from the right foot
				m_bLeftFootLaunch = false;
			}

			SetState(State_Land);
		}
		else
		{
			SetState(State_Launch);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

REGISTER_TUNE_GROUP_BOOL(bDoRagdollOnCollision, true);
REGISTER_TUNE_GROUP_FLOAT(fRagdollOnCollisionAllowedSlope, -0.65f);

CTaskJump::FSM_Return CTaskJump::StateLaunch_OnEnter(CPed* pPed)
{
	m_bStandingJump = false;

	if (m_IsQuadrupedJump)
	{			
		if(!GetIsStandingJumpBlocked(pPed)) 
		{
			const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
			const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
			const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
			if ( fHeadingDelta > PI*0.25f )
				m_bLeftFootLaunch = true;
			else if ( fHeadingDelta > -PI*0.25f )
				m_bStandingJump = true;
			//else right jump
		}
		else
		{
			// Not enough room for a standing jump
			return FSM_Quit;
		}

		//! get auto jump info from climb detector.
		if(m_iFlags.IsFlagSet(JF_AutoJump))
		{
			CClimbHandHoldDetected handHoldDetected;
			if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
			{
				m_iFlags.SetFlag(JF_ScaleQuadrupedJump);
				m_fQuadrupedJumpDistance = handHoldDetected.GetDepth();
				if(handHoldDetected.GetPhysical())
				{
					m_vHandHoldPoint = VEC3V_TO_VECTOR3(handHoldDetected.GetPhysical()->GetTransform().Transform(VECTOR3_TO_VEC3V(handHoldDetected.GetPhysicalHandHoldOffset())));
				}
				else
				{
					m_vHandHoldPoint = handHoldDetected.GetHandHold().GetPoint();
				}

				// Choose highest Z - this means we jump the appropriate height.
				m_vHandHoldPoint.z = handHoldDetected.GetHighestZ();
			}
		}
		
#if ENABLE_HORSE
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE)
		{
			m_fTurnPhaseLaunch = static_cast<CTaskHorseLocomotion*>(pTask)->GetTurnPhase();
		}
#endif
	} 
	else
	{
// #if FPS_MODE_SUPPORTED
// 		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true))
// 		{
// 			const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
// 			float fAimHeading = aimCameraFrame.ComputeHeading();
// 
// 			fAimHeading = camInterface::GetPlayerControlCamHeading(*pPed);
// 
// 			const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
// 			if(pFpsCam)
// 			{
// 				fAimHeading += pFpsCam->GetRelativeHeading();
// 			}
// 			fAimHeading = fwAngle::LimitRadianAngle(fAimHeading);
// 
// 			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(SubtractAngleShorter(fAimHeading, pPed->GetCurrentHeading()));
// 
// 			// Stop the locomotion task from overriding the angular vel
// 			pPed->SetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity, true);
// 		}
// #endif // FPS_MODE_SUPPORTED

		// Perform a moving jump
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION && static_cast<CTaskHumanLocomotion*>(pTask)->UseLeftFootTransition())
		{
			// The last foot down was right, so we need to launch from the left foot
			m_bLeftFootLaunch = true;
		}
		else
		{
			// The last foot down was left, so we need to launch from the right foot
			m_bLeftFootLaunch = false;
		}

		//! Detection might not happen quickly enough, so prevent ped from falling.
		if(m_iFlags.IsFlagSet(JF_ForceParachuteJump) || m_iFlags.IsFlagSet(JF_ForceDiveJump))
		{
			pPed->SetUseExtractedZ(true);
		}

		TUNE_GROUP_BOOL(PED_JUMPING, bEnableAssistedJump, true);
		if(bEnableAssistedJump && pPed->IsPlayer())
		{
			if(m_iFlags.IsFlagSet(JF_SuperJump))
			{
				if(pPed->GetSpeechAudioEntity())
				{
					pPed->GetSpeechAudioEntity()->TriggerPainFromPainId(AUD_PAIN_ID_JUMP, 0);
				}
			}
			else
			{
				//! Clones receive this information over network.
				if(!pPed->IsNetworkClone())
				{
					DoJumpGapTest(pPed, m_JumpHelper, m_iFlags.IsFlagSet(JF_ShortJump));
				}
				else
				{
					if(m_JumpHelper.WasLandingFound())
					{
						TUNE_GROUP_FLOAT(VAULTING, fClonePedPositionTolerance, 8.0f, 0.0f, 8.0f, 0.1f);
						TUNE_GROUP_FLOAT(VAULTING, fClonePedOrientationTolerance, PI*0.33f, 0.0f, PI, 0.1f);

						Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				
						float fDesiredHeading = pPed->GetCurrentHeading();
						Vector3 vDir = vPedPos - m_JumpHelper.GetLandingPosition(true);
						if(vDir.Mag2() > 0.0f)
						{
							vDir.NormalizeFast();
							fDesiredHeading = rage::Atan2f(vDir.x, -vDir.y);
							fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);

							float	fHeadingDiff = SubtractAngleShorter(fDesiredHeading, pPed->GetCurrentHeading());
							if(fabs(fHeadingDiff) > fClonePedOrientationTolerance)
							{
								pPed->SetHeading(fDesiredHeading);
							}
						}

						Vector3 vOwnerPos;
						if(m_JumpHelper.GetLandingPhysical())
						{
							Matrix34 mPhysical = MAT34V_TO_MATRIX34(m_JumpHelper.GetLandingPhysical()->GetMatrix());
							mPhysical.Transform(m_vOwnerAssistedJumpStartPositionOffset);
							vOwnerPos = m_vOwnerAssistedJumpStartPositionOffset;
						}
						else
						{
							vOwnerPos = m_vOwnerAssistedJumpStartPosition;
						}

						//Vector3 vLandingPosition = m_JumpHelper.GetLandingPosition(true);

						if(!vPedPos.IsClose(vOwnerPos, fClonePedPositionTolerance))
						{
							pPed->SetPosition(vOwnerPos);
						}
					}
				}
			}

			if(m_JumpHelper.WasLandingFound())
			{
				// Tell the ped to use the Z from the clip straight away - this prevents us initially falling off ledges when trying to scale the jump.
				pPed->SetUseExtractedZ(true);

				// Attach to landing physical if land physical is traveling fast enough.
				TUNE_GROUP_BOOL(PED_JUMPING, bEnableJumpAttachment, true);
				if(m_JumpHelper.GetLandingPhysical() && bEnableJumpAttachment)
				{
					//! Check we have a sane velocity 
					bool bVelocityOk = (m_JumpHelper.GetLandingPhysical()->GetVelocity().Mag2() > 1.0f) && 
						(abs(pPed->GetVelocity().Mag() - m_JumpHelper.GetLandingPhysical()->GetVelocity().Mag()) < 7.5f);

					//! Clones skip vel check as owner will reset jump helper if it failed.
					if(bVelocityOk || pPed->IsNetworkClone())
					{
						// Position offset
						Vector3 vAttachOffset = VEC3V_TO_VECTOR3(m_JumpHelper.GetLandingPhysical()->GetTransform().UnTransform(pPed->GetTransform().GetPosition()));

						Vector3 vDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_JumpHelper.GetLandingPosition(true);
						vDir.NormalizeFast();
						float fNewHeading = rage::Atan2f(vDir.x, -vDir.y);
						fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);

						// Rotation offset
						Matrix34 m;
						m.MakeRotateZ(fNewHeading);
						Matrix34 other = MAT34V_TO_MATRIX34(m_JumpHelper.GetLandingPhysical()->GetMatrix());
						m.Dot3x3Transpose(other);
						Quaternion q;
						m.ToQuaternion(q);

						// Attach
						u32 flags = ATTACH_STATE_BASIC | 
							ATTACH_FLAG_INITIAL_WARP | 
							ATTACH_FLAG_DONT_NETWORK_SYNC | 
							ATTACH_FLAG_COL_ON | 
							ATTACH_FLAG_AUTODETACH_ON_DEATH | 
							ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;

						pPed->AttachToPhysicalBasic(m_JumpHelper.GetLandingPhysical(), -1, flags, &vAttachOffset, &q);

						if(!pPed->IsNetworkClone())
						{
							ActivateRagdollOnCollision(m_JumpHelper.GetLandingPhysical(), 0.1f, -1.0f, true);
						}

						pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
					}
					else
					{
						m_JumpHelper.ResetLandingFound();
					}
				}
			}
			else
			{
				//Do a skydive launch if the ped can parachute from the end position.			
				m_bSkydiveLaunch = m_iFlags.IsFlagSet(JF_ForceParachuteJump) ? true : CanDoParachuteJumpFromPosition(pPed, true);
				if(!m_bSkydiveLaunch)
				{
					//diving launch?
					m_bDivingLaunch = m_iFlags.IsFlagSet(JF_ForceDiveJump) ? true : CanDoDiveFromPosition(pPed, m_bSkydiveLaunch);
				}

				if (m_bDivingLaunch) //start streaming dive anims
				{
					fwMvClipSetId swimmingClipSetId;
					CTaskMotionBase* pedTask = pPed->GetPrimaryMotionTask();
					if (pedTask)
					{

						fwClipSet *clipSet = fwClipSetManager::GetClipSet(pedTask->GetDefaultDivingClipSet());
						if (clipSet)
							clipSet->Request_DEPRECATED();				
					}

					//! Hide weapon when diving.
					CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
					if (pWeapon)
					{
						pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
					}
				}
			}
		}
	
		if(!pPed->IsNetworkClone() && !m_iFlags.IsFlagSet(JF_SuperJump))
		{
			CalculateJumpAngle();
		}
	}

	//! Disable jump if it is on steep stairs.
	if(sm_Tunables.m_DisableJumpOnSteepStairs && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
	{
		if(m_bJumpingUpwards && (m_fJumpAngle > sm_Tunables.m_MaxStairsJumpAngle))
		{
			return FSM_Quit;
		}
	}

	// Kick out of stealth if you jump.
	if (pPed->GetMotionData() && pPed->GetMotionData()->GetUsingStealth())
	{
		pPed->GetMotionData()->SetUsingStealth(false);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateLaunch_OnUpdate(CPed* pPed)
{
	if(pPed->GetIsAttached())
	{
		if(pPed->GetAttachParent())
		{
			static dev_float PITCHED            = 0.9f;
			float fDotZ = DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetC()), ZAXIS);

			if(( (fDotZ < PITCHED) ) )
			{
				pPed->DetachFromParent(0);
				m_bScalingVelocity = false;
			}
		}
	}

	if( m_networkHelper.GetBoolean(ms_TakeOffId) == true )
	{
		// Notify the ped we have launched
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsStanding, false );

		// set to in air.
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, true );

		m_bOffGround = true;

		m_bIsFalling=true; //not technically, but need to ensure we are on ground before MoVE starts land

		CalculateHeightAdjustedDistance(pPed);

		InitVelocityScaling(pPed);

		if(m_iFlags.IsFlagSet(JF_ScaleQuadrupedJump))
		{
			pPed->DisableCollision();
		}

		// Tell the ped to use the Z from the clip
		pPed->SetUseExtractedZ(true);

        if(!pPed->IsNetworkClone())
        {
		    if(m_iFlags.IsFlagSet(JF_SuperJump))
		    {
                if(pPed->IsLocalPlayer())
                {
			        const CControl* pControl = GetControlFromPlayer(pPed);

                    m_bUseMaximumSuperJumpVelocity = pControl && pControl->GetPedJump().IsDown();

					TUNE_GROUP_INT(PED_JUMPING, uSuperJumpMinDuration, 0, 0, 1000, 1);
					TUNE_GROUP_INT(PED_JUMPING, uSuperJumpMaxDuration, 0, 0, 1000, 1);
					TUNE_GROUP_FLOAT(PED_JUMPING, fSuperJumpMinIntensity, 0.0f, 0.0f, 100.0f, 0.01f);
					TUNE_GROUP_FLOAT(PED_JUMPING, fSuperJumpMaxIntensity, 0.0f, 0.0f, 100.0f, 0.01f);
					CControlMgr::StartPlayerPadShakeByIntensity(m_bUseMaximumSuperJumpVelocity ? (u32)uSuperJumpMaxDuration : (u32)uSuperJumpMinDuration,
						m_bUseMaximumSuperJumpVelocity ? fSuperJumpMaxIntensity : fSuperJumpMinIntensity);
                }
				else
				{
					m_bUseMaximumSuperJumpVelocity = m_iFlags.IsFlagSet(JF_AIUseFullForceBeastJump);
				}

			    float fSuperJumpVel;
				if(m_iFlags.IsFlagSet(JF_BeastJump))
				{
					fSuperJumpVel = m_bUseMaximumSuperJumpVelocity ? sm_Tunables.m_MaxBeastJumpInitialVelocity : sm_Tunables.m_MinBeastJumpInitialVelocity;
				}
				else
				{
					fSuperJumpVel = m_bUseMaximumSuperJumpVelocity ? sm_Tunables.m_MaxSuperJumpInitialVelocity : sm_Tunables.m_MinSuperJumpInitialVelocity;
				}
				
				TUNE_GROUP_FLOAT(PED_JUMPING, fSuperJumpMinMult, 2.0f, 0.0f, 100.0f, 0.01f);
				TUNE_GROUP_FLOAT(PED_JUMPING, fSuperJumpMaxMult, 4.0f, 0.0f, 100.0f, 0.01f);
				float fMBR = pPed->GetMotionData()->GetCurrentMbrY();
				float fScale = fSuperJumpMinMult + ((fMBR / MOVEBLENDRATIO_SPRINT) * (fSuperJumpMaxMult - fSuperJumpMinMult));

			    // Experiment with test jump
				Vector3 vCurrentVelocity = pPed->GetVelocity();
				vCurrentVelocity.z = 0.0f;
			    vCurrentVelocity *= fScale;
			    vCurrentVelocity.z += fSuperJumpVel;
			    pPed->SetVelocity(vCurrentVelocity);
			    pPed->SetDesiredVelocity(vCurrentVelocity);

                m_bHasAppliedSuperJumpVelocityLocally = true;
            }
		}

		TUNE_GROUP_BOOL(PED_JUMPING, bDoHighJumpFixup, true);
		if(m_iFlags.IsFlagSet(JF_JumpingUpHill) && bDoHighJumpFixup && !pPed->IsNetworkClone())
		{
			float fJumpAngle = m_fJumpAngle;
			fJumpAngle = Clamp(fJumpAngle, sm_Tunables.m_HighJumpMinAngleForVelScale, sm_Tunables.m_HighJumpMaxAngleForVelScale);
			float fAngleWeight = (fJumpAngle - sm_Tunables.m_HighJumpMinAngleForVelScale) / (sm_Tunables.m_HighJumpMaxAngleForVelScale-sm_Tunables.m_HighJumpMinAngleForVelScale);
			float fHighJumpVelocity = sm_Tunables.m_HighJumpMinVelScale + ((sm_Tunables.m_HighJumpMaxVelScale-sm_Tunables.m_HighJumpMinVelScale) * fAngleWeight);

			Vector3 vCurrentVelocity = pPed->GetVelocity();
			vCurrentVelocity.z += fHighJumpVelocity;
			pPed->SetVelocity(vCurrentVelocity);
			pPed->SetDesiredVelocity(vCurrentVelocity);
		}
	}

    if(m_iFlags.IsFlagSet(JF_SuperJump))
    {
        if(pPed->IsNetworkClone() && m_bOffGround && !m_bHasAppliedSuperJumpVelocityLocally && m_bHasAppliedSuperJumpVelocityRemotely)
        {
            float fSuperJumpVel = m_bUseMaximumSuperJumpVelocity ? sm_Tunables.m_MaxSuperJumpInitialVelocity : sm_Tunables.m_MinSuperJumpInitialVelocity;

            Vector3 vCurrentVelocity = pPed->GetVelocity();
            vCurrentVelocity.z = fSuperJumpVel;
            pPed->SetVelocity(vCurrentVelocity);
            pPed->SetDesiredVelocity(vCurrentVelocity);

            m_bHasAppliedSuperJumpVelocityLocally = true;
        }
    }

	//! Re-enable collision once we've applied scale in z (which is at the peak height of animation).
	if(m_iFlags.IsFlagSet(JF_ScaleQuadrupedJump))
	{
		const crClip* pClip = GetJumpClip();
		if(pClip && HasReachedApex())
		{
			pPed->EnableCollision();
		}
	}

	if(pPed->IsNetworkClone() && !IsRunningLocally())
	{
		if(GetStateFromNetwork() > State_Launch)
		{
			SetState( GetStateFromNetwork() );
		}
		return FSM_Continue;
	}

	if (m_bDivingLaunch) //start the Animated Velocity
	{
		if (pPed->GetMovePed().IsTaskNetworkFullyBlended() && pPed->GetMotionData()->GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Diving_Idle) 
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);			
	}

	float fStartPhase = 0.0f;
	float fEndPhase = 0.0f;
	bool bCanLand = false;
	bool bCanFall= false;
	const crClip* pClip = GetJumpClip();

	float	fCurrentPhase = m_networkHelper.GetFloat(ms_LaunchPhaseId);
		
	// The jump clip has a window in which we search for a land point. During this time, we don't allow the ped to fall. At the end of
	// this window, if we still haven't found a land point, then allow it then.
	if( (m_bDivingLaunch && pPed->GetIsInWater() && (pPed->m_Buoyancy.GetAbsWaterLevel() > VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z) ) )
	{
		bCanFall = true;
	}
	else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_ExitLaunchId.GetHash(), fStartPhase, fEndPhase))
	{
		if(fCurrentPhase >= fStartPhase)
		{
			if (!m_bDivingLaunch)
				bCanLand = true;

			// If parachuting or diving, allow to fall immediately. 
			if(m_bSkydiveLaunch || m_bDivingLaunch)
			{
				bCanFall = true;
			}
		}

		if(fCurrentPhase >= fEndPhase )
		{
			bCanFall = true;
		}
	}

	INSTANTIATE_TUNE_GROUP_BOOL(PED_JUMPING, bDoRagdollOnCollision);
	TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollOnCollisionAllowedPenetrationJump, 0.125f, 0.0f, 100.0f, 0.01f);
	INSTANTIATE_TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollOnCollisionAllowedSlope, -1.0f, 0.0f, 0.01f);

	// If we want to ragdoll on collision, we aren't currently set to ragdoll on collision, ragdolling on collision hasn't been disabled, we're not auto-jumping, we're
	// not doing a quadruped jump, we're the player and we're within the allowed window in the animation...
	// Turn off in MP games per B* 1037938.
	if(!NetworkInterface::IsGameInProgress())
	{
		if(bDoRagdollOnCollision && !pPed->GetActivateRagdollOnCollision() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableJumpRagdollOnCollision) && 
			!m_iFlags.IsFlagSet(JF_AutoJump) && !m_IsQuadrupedJump && pPed->IsPlayer() && CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_RagdollOnCollisionId, fStartPhase, fEndPhase) &&
			fCurrentPhase >= fStartPhase && fCurrentPhase < fEndPhase)
		{
			ActivateRagdollOnCollision(NULL, fRagdollOnCollisionAllowedPenetrationJump, fRagdollOnCollisionAllowedSlope);
		}
	}

	bool bHasJetpack = pPed->GetHasJetpackEquipped(); 

	if(m_bOffGround && m_iFlags.IsFlagSet(JF_SuperJump) && pPed->GetVelocity().GetZ() < 0.0f)
	{
		bCanLand = true;
		bCanFall = true;
	}

	if(bCanLand || bCanFall)
	{
		Vector3 vRemainingDistance(Vector3::ZeroType);
		bool bOnGround = IsPedNearGround(pPed, GetJumpLandThreshold(), vRemainingDistance, m_fPedLandPosZ);

		//! Don't go to fall if owner has already decided to land.
		if(pPed->IsNetworkClone() && GetStateFromNetwork()==State_Land)
		{
			SetState( State_Land );
		}
		else if(bOnGround && !m_bDivingLaunch)
		{
			if(bCanLand)
			{
				// We've completed the launch
				SetState( State_Land );
			}
		}
		else if(bCanFall)
		{
			// We're off the ground, so can only fall now			
			SetState(State_Fall);
		}
	}

#if __BANK
	// TEMP, draw the landing spot while doing the jump
	TUNE_GROUP_BOOL(PED_JUMPING, bRenderCalculatedLandingSpot, false);
	if( bRenderCalculatedLandingSpot && m_JumpHelper.WasLandingFound())
	{
		Vector3 vLandPos = m_JumpHelper.GetLandingPosition(pPed->GetIsAttached());
		grcDebugDraw::Sphere( vLandPos, 0.13f, Color32(255, 0, 0, 255), true, 240);
	}
#endif	//__BANK

	TUNE_GROUP_BOOL(PED_JUMPING, bDoStuntJump, true);
	TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollJumpTestMinTime, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollJumpActivationMinTime, 0.2f, 0.0f, 1.0f, 0.01f);
	if(!NetworkInterface::IsGameInProgress() && bDoStuntJump && !m_iFlags.IsFlagSet(JF_AutoJump) && !m_IsQuadrupedJump)
	{
		if(!m_bWaitingForJumpRelease && GetTimeRunning() >= fRagdollJumpTestMinTime)
		{
			const CControl* pControl = GetControlFromPlayer(pPed);
			if(pControl && pControl->GetMeleeAttackLight().IsPressed()) 
			{
				m_bStuntJumpRequested = true;
			}
		}

		if(m_bStuntJumpRequested && GetTimeRunning() >= fRagdollJumpActivationMinTime && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
		{
			if(pPed->GetSpeechAudioEntity())
				pPed->GetSpeechAudioEntity()->SetBlockFallingVelocityCheck();
			CEventSwitch2NM event(10000, rage_new CTaskNMHighFall(100, NULL, CTaskNMHighFall::HIGHFALL_STUNT_JUMP));
			pPed->GetPedIntelligence()->AddEvent(event);
		}
	}

	//! Do a test for a high fall once we have passed normal last height.
	if(m_bOffGround && fCurrentPhase > GetAssistedJumpLandPhase(pClip) && !m_iFlags.IsFlagSet(JF_SuperJump) && !bHasJetpack )
	{
		float fFallTime = 0.0f;
		if(CTaskFall::GetIsHighFall(pPed, true, fFallTime, CTaskFall::sm_Tunables.m_JumpFallTestAngle, 0.0f, GetTimeStep()))
		{	
			Vector3 vRemainingDistance(Vector3::ZeroType);

			if(pClip)
			{
				vRemainingDistance = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, fCurrentPhase, 1.0f );
				vRemainingDistance = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vRemainingDistance)));
			}

			//! Grab the ped position.
			Vector3	pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			
			Vector3 vEnd = pedPos;
			vEnd += vRemainingDistance;

#if __DEV
			CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(pedPos), VECTOR3_TO_VEC3V(vEnd), 0.25f, Color_green, 2500, 0, false);	
#endif

			//! Check for solid objects between the ped and the position. If we hit something, this isn't a high fall.
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetCapsule(pedPos, vEnd, 0.2f);
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			capsuleDesc.SetExcludeEntity(GetPed());

			if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
			{
				m_iFlags.SetFlag(JF_ForceNMFall);
				SetState(State_Fall);
				return FSM_Continue;	
			}
		}
	}

	if(!m_bDivingLaunch && pPed->GetIsSwimming())
	{
		SetState(State_Quit);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateLaunch_OnExit(CPed* pPed)
{
//#if __ASSERT
	//	const crClip* pClip = GetJumpClip();
//	//! Ensure takeoff event has been processed.
//	taskAssertf(m_bOffGround, "Take Off Event missing? Clip name = %s", pClip ? pClip->GetName() : "Unknown");
//#endif

	// Re-enable collision if it's been turned off.
	pPed->EnableCollision();

	ResetRagdollOnCollision();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateLand_OnEnter(CPed* pPed)
{
	if(!pPed->IsNetworkClone())
	{
		// Prevent doing running lands on stairs as you end up sprinting up them unrealistically.
		bool bLandingOnSteepStairs = false;
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
		{
			TUNE_GROUP_FLOAT(PED_JUMPING, fLandAngleToForceWalk, 20.0f, 0.0f, 90.0f, 0.1f);
			float fLandAngle = acosf(pPed->GetSlopeNormal().z) * RtoD;
			bLandingOnSteepStairs = fLandAngle > fLandAngleToForceWalk && IsMovingUpSlope(pPed->GetSlopeNormal());
		}

		// See if we need to do a standing land
		m_bStandingLand = !IsPedTryingToMove() || CheckForBlockedJump(pPed) || bLandingOnSteepStairs;
	}

	// Snap the peds entity to the correct height above the ground. Note: Don't do this is we are jumping up steep hills.
	if(!m_iFlags.IsFlagSet(JF_SteepHill) && CanSnapToGround(pPed, m_fPedLandPosZ))
	{
		if(!m_IsQuadrupedJump)
		{
			static dev_u8 s_uNumFrames = 25;
			pPed->m_PedResetFlags.SetEntityZFromGround( s_uNumFrames ); //this breaks stirrups in quads, if we need it, will need addressed
			pPed->m_PedResetFlags.SetEntityZFromGroundZHeight(m_fPedLandPosZ, GetEntitySetZThreshold());
		}
	}

	m_bIsFalling=false;

	// We can't be off ground if we started the land, the first frame of land is the foot reconnecting with the land
	m_bOffGround = false;

	// Reset the useZ
	pPed->SetUseExtractedZ(false);

	// Notify that we are no longer in the air
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );

	m_networkHelper.SendRequest(ms_EnterLandId);

	CTaskFall::GenerateLandingNoise(*pPed);

	if(pPed->GetIsAttached())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		GetPed()->SetRagdollOnCollisionIgnorePhysical(NULL);
	}

#if __BANK
	// TEMP, draw the landing spot
	TUNE_GROUP_BOOL(PED_JUMPING, bRenderJumpLandingSpot, false);
	if(bRenderJumpLandingSpot)
	{
		Vector3	vLandingSpot = pPed->GetGroundPos();
		grcDebugDraw::Sphere( vLandingSpot, 0.13f, Color32(0, 255, 0, 255), true, 240);
	}
#endif	//__BANK

	// Determine which land clip we want to play and set appropriate signal.
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateLand_OnUpdate(CPed *pPed)
{	
	if(m_networkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelLId))
	{
		m_bUseLeftFoot = false;
	}
	else if(m_networkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelRId))
	{
		m_bUseLeftFoot = true;
	}

	// See if we were pressing the jump button at this time and interrupt manually if so?
	bool	bReJump = false;
	
	// If predictive jump, allow to jump again quickly.
	if(m_JumpHelper.WasLandingFound() && pPed->IsLocalPlayer())
	{
		const CControl* pControl = GetControlFromPlayer(pPed);
		if(pControl) 
		{
			if( pControl->GetPedJump().IsPressed() )
			{
				bReJump = true;
			}
		}
	}

	if(pPed->IsNetworkClone() && !IsRunningLocally())
	{
		if(GetStateFromNetwork() > State_Land)
		{
			SetState( GetStateFromNetwork() );
		}
		return FSM_Continue;
	}

	//! Retest to see if ped can fall. Note: If bit higher than our land threshold, so we don't oscillate quickly between the two.
	TUNE_GROUP_FLOAT(PED_JUMPING, fFallFromLandExtraZ, 0.05f, 0.0f, 100.0f, 0.01f);
	static dev_float s_fMinTimeInState = 0.2f;

	bool bOnGround = pPed->GetGroundPos().z > PED_GROUNDPOS_RESET_Z;

	if(!pPed->IsNetworkClone() && (GetTimeInState() >= s_fMinTimeInState) && !bOnGround && !IsPedNearGround(pPed, GetJumpLandThreshold()+fFallFromLandExtraZ))
	{
		SetState(State_Fall);
	}
	else if( m_networkHelper.GetBoolean(ms_ExitLandId) == true || GetHasInterruptedClip(pPed) || bReJump )
	{
		return FSM_Quit;
	}

	if(pPed->GetIsSwimming())
	{
		SetState(State_Quit);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateLand_OnExit(CPed *pPed)
{
	if(!m_IsQuadrupedJump)
	{
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bStayStrafing && GetState() != State_Fall)
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming, true); 
		}
		else if(m_bStandingLand) 
		{
			//! Move to idle animation from a standing land, this ensures we get rid of any locomotion
			//! states underneath.
			if(IsPedTryingToMove(true) && m_networkHelper.GetBoolean( ms_StandLandWalkId ) )
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Walk, true); 
			}
			else
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true); 
				m_bTagSyncOnExit = false;
			}
		}
		else
		{
			//! if moving land, go to run state (which matches the land clip).
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Run, true); 
		}
	}

	if (!pPed->IsNetworkClone() && pPed->GetNetworkObject())
	{
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

		//Since motion state is InFrequent make sure that the remote gets set at the same time as the task state
		pPedObj->ForceResendAllData();
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateFall_OnEnter(CPed* pPed)
{
	m_bOffGround = true;

	m_vInitialFallVelocity = pPed->GetDesiredVelocity();
	//don't inherit positive z velocities.
	if(m_vInitialFallVelocity.z > 0.0f)
		m_vInitialFallVelocity.z = 0.0f;

	// B*2277022: Set the desired heading to the current heading if they're different. 
	// Fixes issues where a large desired heading was set in ProcessPostFSM (CTaskFall::UpdateHeading) but the current wasn't being updated properly.
	if (pPed->GetDesiredHeading() != pPed->GetCurrentHeading())
	{
		pPed->SetDesiredHeading(pPed->GetCurrentHeading());
	}

	fwFlags32 iFlags = m_bLeftFootLaunch ? FF_ForceJumpFallLeft : FF_ForceJumpFallRight;
	
	if(m_bSkydiveLaunch)
	{
		iFlags.SetFlag(FF_ForceSkyDive);
	} 
	else if (m_bDivingLaunch)
	{
		iFlags.SetFlag(FF_ForceDive);
	}
	
	if(m_iFlags.IsFlagSet(JF_ForceNMFall))
	{
		iFlags.SetFlag(FF_ForceHighFallNM);
	}
	else if(m_iFlags.IsFlagSet(JF_SuperJump))
	{
		iFlags.SetFlag(FF_DisableNMFall);
		iFlags.SetFlag(FF_HasAirControl);
	}

	if(m_iFlags.IsFlagSet(JF_BeastJump) && !m_bDivingLaunch)
	{
		iFlags.SetFlag(FF_BeastFall);
	}
	
	CTaskFall *pNewTask = rage_new CTaskFall(iFlags);
	SetNewTask(pNewTask);
	if(m_JumpHelper.WasLandingFound())
	{
		pNewTask->SetHighFallNMDelay(0.25f);
	}

	//! Inform fall task of our starting fall height if it is greater than our current z position. This allows fall task
	//! to know correct distance fallen from a jump.
	if(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z < m_fInitialPedZ)
	{
		pNewTask->SetInitialPedZ(m_fInitialPedZ);
	}

	if(pPed->GetIsAttached())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		GetPed()->SetRagdollOnCollisionIgnorePhysical(NULL);
	}

	pPed->SetVelocity(pPed->GetDesiredVelocity());
	pPed->SetUseExtractedZ(false);

	INSTANTIATE_TUNE_GROUP_BOOL(PED_JUMPING, bDoRagdollOnCollision);
	TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollOnCollisionAllowedPenetrationFall, 0.05f, 0.0f, 100.0f, 0.01f);
	INSTANTIATE_TUNE_GROUP_FLOAT(PED_JUMPING, fRagdollOnCollisionAllowedSlope, -1.0f, 0.0f, 0.01f);

	if(!pPed->IsNetworkClone() && bDoRagdollOnCollision && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableJumpRagdollOnCollision) && !m_iFlags.IsFlagSet(JF_AutoJump) && !m_IsQuadrupedJump && pPed->IsPlayer())
	{
		ActivateRagdollOnCollision(NULL, fRagdollOnCollisionAllowedPenetrationFall, fRagdollOnCollisionAllowedSlope);
	}

	if(pPed->GetSpeechAudioEntity())
		pPed->GetSpeechAudioEntity()->SetBlockFallingVelocityCheck();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateFall_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Fall handles it's own land.
		return FSM_Quit;
	}
	else
	{	
		if(GetSubTask())
		{
			taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
			CTaskFall *pTaskFall = static_cast<CTaskFall*>(GetSubTask());

			if(m_iFlags.IsFlagSet(JF_SuperJump))
			{
				const CControl* pControl = GetControlFromPlayer(pPed);
				if(pControl && !pControl->GetPedJump().IsDown())
				{
					pTaskFall->SetDampZVelocity();
				}
			}

			//! If we detect a land, reset ragdoll on collision.
			bool bFallLanding = pTaskFall->GetState()==CTaskFall::State_Landing || pTaskFall->GetState()==CTaskFall::State_GetUp;
			if(bFallLanding)
			{
				ResetRagdollOnCollision();
			}
		}
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskJump::FSM_Return CTaskJump::StateFall_OnExit(CPed* UNUSED_PARAM(pPed))
{
	ResetRagdollOnCollision();
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

float CTaskJump::GetJumpLandThreshold() const
{
	TUNE_GROUP_FLOAT(PED_JUMPING,fJumpLandThreshold,0.1f, 0.01f, 5.0f, 0.01f);	
	TUNE_GROUP_FLOAT(HORSE_JUMPING,fHorseJumpLandThreshold,1.0f, 0.01f, 5.0f, 0.01f);	

	return m_IsQuadrupedJump ? fHorseJumpLandThreshold : fJumpLandThreshold;
}

//////////////////////////////////////////////////////////////////////////

float CTaskJump::GetEntitySetZThreshold() const
{
	TUNE_GROUP_FLOAT(PED_JUMPING,fJumpEntitySetEntityZThreshold,0.5f, 0.01f, 5.0f, 0.01f);	
	return fJumpEntitySetEntityZThreshold;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::CanSnapToGround(CPed* pPed, float fLandPosZ) const
{
	if(m_IsQuadrupedJump)
	{
		return true;
	}

	TUNE_GROUP_BOOL(PED_JUMPING,bEnableSnapToGround,false);
	if(bEnableSnapToGround)
	{
		return true;
	}

	//! Don't snap if using full IK. If we are, we attempt to IK legs on landing to prevent penetration.
	u32 legIkMode = pPed->m_PedConfigFlags.GetPedLegIkMode();
	if((legIkMode == LEG_IK_MODE_FULL) || (legIkMode == LEG_IK_MODE_FULL_MELEE))
	{
		return false;
	}

	// if the detected ground position is above a set height. This is so we do not snap upwards. Just allow probe to push
	// character out in this instance.
	TUNE_GROUP_FLOAT(PED_JUMPING,fSnapToGroundLandThreshold,0.75f, 0.00f, 5.0f, 0.01f);	
	Vector3	pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if(pPed->IsOnGround())
	{
		return ((pedPos.z - pPed->GetGroundPos().z) > fSnapToGroundLandThreshold) && 
			((pedPos.z - fLandPosZ) > fSnapToGroundLandThreshold);
	}

	return ((pedPos.z - fLandPosZ) > fSnapToGroundLandThreshold);
}

//////////////////////////////////////////////////////////////////////////

float CTaskJump::GetAssistedJumpLandPhase(const crClip* pClip) const 
{
	static const crProperty::Key moveEventKey = crTag::CalcKey("MoveEvent", 0x7EA9DFC4);

	//Find abbreviated jump tag.
	if(pClip)
	{
		crTagIterator moveIt(*pClip->GetTags(),0.0f, 1.0f, moveEventKey);
		while(*moveIt)
		{
			const crTag* pTag = *moveIt;
			{
				const crPropertyAttribute* attrib = pTag->GetProperty().GetAttribute(moveEventKey);
				if(attrib && attrib->GetType() == crPropertyAttribute::kTypeHashString)
				{
					if (ms_AssistedJumpLandPhaseId.GetHash() == static_cast<const crPropertyAttributeHashString*>(attrib)->GetHashString().GetHash())
					{
						return pTag->GetStart();
					}	
				}
			}
			++moveIt;
		}
	}

	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::ProcessPostMovement()
{
	if(GetState() == State_Launch)
	{
		m_fClipLastPhase = m_networkHelper.GetFloat(ms_LaunchPhaseId);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

const crClip *CTaskJump::GetJumpClip() const
{
	const crClip* pClip = m_networkHelper.GetClip(ms_JumpClipId);
	return pClip;
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::InitVelocityScaling(CPed* pPed)
{
	const crClip* pClip = GetJumpClip();
	if(pClip)
	{
		m_fScaleVelocityStartPhase = m_networkHelper.GetFloat(ms_LaunchPhaseId);
		float fExitPhase = GetAssistedJumpLandPhase(pClip);

		//! HEIGHT.
		//! TO DO - Mark 'APEX' in anim, so that we don't need to do this.
		static dev_u32 nIterations = 25;
		float fCurrentClipPhase = m_fScaleVelocityStartPhase, fEndApexClipPhase = 0.0f;
		float fCurrentApexZ = 0.0f;
		float fIncrement = ((1.0f-m_fScaleVelocityStartPhase)/(float)nIterations);
		for(int i = 0; i < nIterations; ++i)
		{
			fEndApexClipPhase = Min(1.0f, fCurrentClipPhase + (i * fIncrement));
			Vector3 vAnimRemainingDist(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, fCurrentClipPhase, fEndApexClipPhase));

			//! Once we start going down, break.
			if(vAnimRemainingDist.z >= fCurrentApexZ)
				fCurrentApexZ = vAnimRemainingDist.z;
			else
				break;
		}

		m_fApexClipPhase = fEndApexClipPhase;

		if(m_JumpHelper.WasLandingFound())
		{
			m_vMaxScalePhase.Set(fExitPhase, fExitPhase, fExitPhase);

			Vector3	vLandPos = m_JumpHelper.GetLandingPosition(pPed->GetIsAttached());	
			vLandPos.z += PED_HUMAN_GROUNDTOROOTOFFSET;
			Vector3 vDirectionToLand = vLandPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vDirectionToLand.Normalize();
			vLandPos -= (vDirectionToLand * m_fBackDist);
			m_vScaleDistance = vLandPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			m_vAnimRemainingDist = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, m_fScaleVelocityStartPhase, fExitPhase );
			m_vAnimRemainingDist = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vAnimRemainingDist)));

			//! Don't scale the jump backwards, it looks pretty daft.
			Vector3 vDistanceDiff(m_vScaleDistance - m_vAnimRemainingDist);
			vDistanceDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDistanceDiff)));
			if(vDistanceDiff.y > 0.0f)
			{
				m_bScalingVelocity = true;
			}
		}
		else if(m_IsQuadrupedJump && m_iFlags.IsFlagSet(JF_ScaleQuadrupedJump))
		{
			//! Do all out scaling before we hit max height.
			TUNE_GROUP_FLOAT(HORSE_JUMPING,fHorseCollisionOffset,0.5f, 0.0f, 10.0f, 0.01f);	
			TUNE_GROUP_FLOAT(HORSE_JUMPING,s_fHorseDistanceFromObstacle,2.0f, 0.0f, 10.0f, 0.01f);	

			Vector3 vDistanceToLandPoint = m_vHandHoldPoint - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float fHeightDifference = vDistanceToLandPoint.z + fHorseCollisionOffset;

			float fDifference = fHeightDifference - fCurrentApexZ;
			if(fDifference >= 0.0f)
				m_vScaleDistance.z = fDifference;

			//! DISTANCE
			Vector3 vAnimRemainingDist(fwAnimHelpers::GetMoverTrackDisplacement(*pClip, m_fScaleVelocityStartPhase, fExitPhase ));
			float fDistance = vDistanceToLandPoint.XYMag() + m_fQuadrupedJumpDistance + s_fHorseDistanceFromObstacle;
			m_vScaleDistance.x = 0.0f;
			m_vScaleDistance.y = Max(0.0f, fDistance - vAnimRemainingDist.y);
			m_vScaleDistance = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vScaleDistance)));

			m_bScalingVelocity = true;
			m_vMaxScalePhase.Set(fExitPhase, fExitPhase, fEndApexClipPhase);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::ScaleDesiredVelocity(CPed *pPed, float fTimeStep)
{
	if(fTimeStep < 0.0f)
		return false;

	if(m_bScalingVelocity)
	{
		const crClip* pClip = GetJumpClip();
		if(pClip)
		{
			// Get the distance remaining on the animation at this time
			float	fCurrentPhase = m_networkHelper.GetFloat(ms_LaunchPhaseId);

			if(m_bOffGround)
			{
				Vector3 vDistanceDiff(m_vScaleDistance - m_vAnimRemainingDist);

				// Add to the desired velocity
				Vector3 newVel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity())));

				for(s32 i = 0; i < 3; i++)
				{
					// Adjust the velocity based on the current phase and the start/end phases. Makes it frame rate independent.
					float fFrameAdjust = Min(fCurrentPhase, m_vMaxScalePhase[i]) - Max(m_fClipLastPhase, m_fScaleVelocityStartPhase);
					if(fFrameAdjust > 0.0f && fCurrentPhase <= m_vMaxScalePhase[i])
					{
						float fClipDuration = pClip->GetDuration();

						fFrameAdjust *= fClipDuration;
						fFrameAdjust /= fTimeStep;

						fFrameAdjust *= 1.0f/(m_vMaxScalePhase[i] - m_fScaleVelocityStartPhase);
						newVel[i] += (vDistanceDiff[i] / fClipDuration) * fFrameAdjust;
					}
				}
				
				if(!pPed->GetIsAttached())
				{
					newVel += pPed->GetGroundVelocityForJump();
				}

				pPed->SetDesiredVelocityClamped(newVel, MAX_JUMP_ACCEL_LIMIT);

				if(m_JumpHelper.WasLandingFound())
				{
					// Adjust direction to try to hit the target position
					// Get the vector to the land point
					Vector3	vDir = m_JumpHelper.GetLandingPosition(pPed->GetIsAttached()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					// Only do it if we're heading towards the point (because we can overshoot)
					Vector3 vPedDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
					float vDot = DotProduct(vPedDir, vDir);
					if( vDot > rage::Cosf(DtoR * 10.0f) )
					{
						float	angToLandPoint = rage::Atan2f( -vDir.x, vDir.y );
						float fDesiredHeadingChange =  SubtractAngleShorter( fwAngle::LimitRadianAngleSafe(angToLandPoint), fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading()) );
						pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( fDesiredHeadingChange );
					}
				}

				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::GetDetectedLandPosition(const CPed *pPed, Vector3 &vLandPosition) const
{
	if(m_JumpHelper.WasLandingFound())
	{
		vLandPosition = m_JumpHelper.GetLandingPosition(pPed->GetIsAttached());
		return true;
	}
	else if(m_JumpHelper.WasNonAssistedLandingFound())
	{
		vLandPosition = m_JumpHelper.GetNonAssistedLandingPosition();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskJump::CalculateHeightAdjustedDistance(CPed *pPed)
{
	// Calculates a new landing position based on the position that comes back from m_JumpHelper this reduces the distance 
	// to the point depending on height to account for falling while moving forward
	if(m_JumpHelper.WasLandingFound())
	{
		// Get the height difference between launch and land
		Vector3	landPos = m_JumpHelper.GetLandingPosition(pPed->GetIsAttached());	
		landPos.z += PED_HUMAN_GROUNDTOROOTOFFSET;
		Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Get the height delta
		float fJumpHeightDelta = landPos.z - pedPos.z;

		if( fJumpHeightDelta >= 0.0f )
		{
			m_fBackDist = 0;
		}
		else
		{
#define	MIN_DISTANCE (2.0f)
			TUNE_GROUP_FLOAT(PED_JUMPING, fBackAdjustAngle, 20.0f, 0.0f, 90.0f, 0.1f);

			float	fAbsDelta = Clamp( abs(fJumpHeightDelta), 1.5f, 3.5f );
			fAbsDelta -= 1.5f;
			float	scaler = ((45.0f - 60.0f)/2.0f);
			float	fHeightAdjustAngle = (fAbsDelta * scaler) + fBackAdjustAngle;

			// Our land point is below us
			// Use the delta in height to adjust the distance we want to travel
			m_fBackDist = -fJumpHeightDelta * rage::Tanf(DtoR * fHeightAdjustAngle);

			float originalDistance = (landPos - pedPos).XYMag();
			if ( originalDistance < MIN_DISTANCE )
			{
				m_fBackDist = 0.0f;
			}
			else
			{
				if ( originalDistance - m_fBackDist < MIN_DISTANCE )
				{
					m_fBackDist = originalDistance - MIN_DISTANCE;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::IsPedNearGround(const CPed *pPed, float fLandHeightThreshold, const Vector3 &vOffset, float &fOutLandZ)
{
	TUNE_GROUP_FLOAT(PED_JUMPING,fJumpLandLandRadius,0.075f, 0.01f, 5.0f, 0.01f);	
	TUNE_GROUP_FLOAT(PED_JUMPING,fJumpLandLandRadiusClone,0.025f, 0.01f, 5.0f, 0.01f);	

	//! Reduce radius for clones. This makes it less likely that they will diverge from owner when landing on edges.
	float fLandRadius = pPed->IsNetworkClone() ? fJumpLandLandRadiusClone : fJumpLandLandRadius;

	return IsPedNearGroundIncRadius(pPed, fLandHeightThreshold, fLandRadius, vOffset, fOutLandZ);
}

bool CTaskJump::IsPedNearGround(const CPed *pPed, float fLandHeightThreshold, const Vector3 &vOffset) 
{
	float fLandZ = 0.0f;
	return IsPedNearGround(pPed, fLandHeightThreshold, vOffset, fLandZ);
}

bool CTaskJump::IsPedNearGroundIncRadius(const CPed *pPed, float fLandHeightThreshold, float fLandRadius, const Vector3 &vOffset, float &fOutLandZ)
{
	Vector3	curPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	curPos += vOffset;
	Vector3 endPos = curPos;
	endPos.z -= pPed->GetCapsuleInfo()->GetGroundToRootOffset();
	endPos.z -= fLandHeightThreshold;
	curPos.z += fLandRadius * 2.0f;

	const CEntity* ppExceptions[1] = { NULL };
	s32 iNumExceptions = 1;
	ppExceptions[0] = pPed;
	iNumExceptions = 1;

	const s32 iTypeFlags    = ArchetypeFlags::GTA_PED_TYPE;

	static const u32 s_NumTests = 1;
	WorldProbe::CShapeTestHitPoint hitPoint[s_NumTests];
	for(int i = 0; i < s_NumTests; ++i)
	{
		Vector3 vTestOffset = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * (float)i * (fLandRadius * 2.0f);
		curPos += vTestOffset;
		endPos += vTestOffset;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestResults probeResults(hitPoint[i]);
		capsuleDesc.SetCapsule(curPos, endPos, fLandRadius);
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		capsuleDesc.SetExcludeEntities(ppExceptions, iNumExceptions, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		capsuleDesc.SetTypeFlags(iTypeFlags);
		capsuleDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

		bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

#if __BANK
		TUNE_GROUP_BOOL(PED_JUMPING, bRenderPedNearOnGround,false);					
		if(bRenderPedNearOnGround)
		{
			static dev_s32 s_timeToLive = 10;
			const Color32 debugColor = bHit ? Color32(0, 255, 0, 255) : Color32(255, 0, 0, 255);
			grcDebugDraw::Sphere(RCC_VEC3V(curPos), fLandRadius, debugColor, false, s_timeToLive);
			grcDebugDraw::Sphere(RCC_VEC3V(endPos), fLandRadius, debugColor, false, s_timeToLive);
			grcDebugDraw::Line(curPos, endPos, debugColor, debugColor, s_timeToLive );
		}
#endif	//__BANK

		if(bHit)
		{
			if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_IsLanding))
			{
				TUNE_GROUP_FLOAT(PED_JUMPING, fPedNearGroundIntersectionDot, 0.15f, -1.0f, 1.0f, 0.01f);
				Vector3 vHitNormal = VEC3V_TO_VECTOR3(hitPoint[i].GetIntersectedPolyNormal());
				float fDot = vHitNormal.Dot(ZAXIS);
				if(fDot < fPedNearGroundIntersectionDot)
				{
					return false;
				}
			}
		}
		else
		{
			return false;
		}
	}

	fOutLandZ = hitPoint[0].GetHitPosition().z;

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::IsPedTryingToMove(bool bCheckForwardDirection) const
{
	const CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer()) 
	{
		// Interrupt if the control is pushed forward
		const CControl* pControl = GetControlFromPlayer(pPed);
		Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
		float fStickMagSq = vStickInput.Mag2();
		if(fStickMagSq > 0.0f)
		{
			if(bCheckForwardDirection)
			{
				float fCamOrient = camInterface::GetHeading();
				vStickInput.RotateZ(fCamOrient);
				vStickInput.NormalizeFast();
				Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
				vPlayerForward.z = 0.0f;
				vPlayerForward.NormalizeFast();

				float fDot = vPlayerForward.Dot(vStickInput);
				static dev_float s_fForwardDot = -0.25f;
				if(fDot > s_fForwardDot)
				{
					return true;
				}
			}
			else
			{
				return true;
			}
		}
	}
	else 
	{
		static dev_float s_fMoveEpsilon = 0.1f;
		if(pPed->GetMotionData()->GetDesiredMbrY() >= s_fMoveEpsilon)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::GetIsStandingJumpBlocked(const CPed* pPed)
{
	// If we are reacting to the stuck in air event, do not perform test
	if(pPed->GetPedIntelligence()->GetCurrentEventType() == EVENT_STUCK_IN_AIR)
	{
		return false;
	}

	//
	// Do a sphere test
	//

	static dev_float SPHERE_RADIUS		= 0.2f;
	static dev_float HEIGHT_ABOVE_PED	= 1.0f;

	Vector3 vEstimatedHeadPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	vEstimatedHeadPos.z += HEIGHT_ABOVE_PED;

	WorldProbe::CShapeTestSphereDesc sphereDesc;
	sphereDesc.SetSphere(vEstimatedHeadPos, SPHERE_RADIUS);
	sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////

float CTaskJump::GetMinHorizJumpDistance()
{
	TUNE_GROUP_FLOAT(PED_JUMPING, fMinHorizJumpDistance, 1.0f, 0.0f, 10.0f, 0.1f);
	return fMinHorizJumpDistance;
}

//////////////////////////////////////////////////////////////////////////

float CTaskJump::GetMaxHorizJumpDistance(const CPed *pPed)
{
	TUNE_GROUP_FLOAT(PED_JUMPING, fWalkSpeedHorizJumpDistance, 5.0f, 0.01f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fRunSpeedHorizJumpDistance, 5.0f, 0.01f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fHighVelocityHorizJumpDistance, 8.0f, 0.01f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fHighVelocityJump, 7.5f, 0.01f, 10.0f, 0.1f);

	if(pPed->GetVelocity().Mag2() > (fHighVelocityJump * fHighVelocityJump))
	{
		return fHighVelocityHorizJumpDistance;
	}
	else if(pPed->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_WALK)
	{
		return fWalkSpeedHorizJumpDistance;
	}

	return fRunSpeedHorizJumpDistance;
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::DoJumpGapTest(const CPed* pPed, JumpAnalysis &jumpHelper, bool bShortJump, float fOverideMaxDist)
{
	TUNE_GROUP_INT(PED_JUMPING, iNumberOfProbes, 14, 1, JUMP_ANALYSIS_MAX_PROBES, 1);

	float fMinHorizJumpDistance = (bShortJump ? 0.5f : GetMinHorizJumpDistance());
	float fMaxHorizJumpDistance = (bShortJump ? 1.0f : GetMaxHorizJumpDistance(pPed));

	if(fOverideMaxDist >= 0.0f)
	{
		fMaxHorizJumpDistance = fOverideMaxDist;
	}

	jumpHelper.AnalyseTerrain(pPed, fMaxHorizJumpDistance, fMinHorizJumpDistance, iNumberOfProbes);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::CanDoParachuteJumpFromPosition(CPed* pPed, bool bDoCanReachTest)
{
	//Do a skydive launch if the ped can parachute from the end position.			
	return CTaskParachute::CanPedParachuteFromPosition(*pPed, GetJumpTestEndPosition(pPed), bDoCanReachTest, NULL, 20.0f);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::CanDoDiveFromPosition(CPed* pPed, bool bCanSkyDive)
{
	TUNE_GROUP_FLOAT(PED_JUMPING, fJumpDiveBlockDist, 3.0f, 0.01f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fJumpDiveBlockRad, 0.4f, 0.01f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fJumpTestZOffset, 0.5f, 0.01f, 10.0f, 0.1f);

	return CTaskMotionSwimming::CanDiveFromPosition(*pPed, GetJumpTestEndPosition(pPed), bCanSkyDive) &&
		!CheckForBlockedJump(pPed, fJumpDiveBlockDist, fJumpDiveBlockRad, fJumpTestZOffset);
}

//////////////////////////////////////////////////////////////////////////

Vec3V_Out CTaskJump::GetJumpTestEndPosition(const CPed* pPed)
{
	//Calculate the end position.		
	TUNE_GROUP_FLOAT(PED_JUMPING, fJumpTestDistance, 4.0f, 0.01f, 10.0f, 0.1f);
	return AddScaled(pPed->GetTransform().GetPosition(), pPed->GetTransform().GetForward(), ScalarV(fJumpTestDistance) );
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::CanBreakoutToMovementTask() const
{
	if(GetSubTask() && GetState() == State_Fall)
	{
		taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
		const CTaskFall *pTaskFall = static_cast<const CTaskFall*>(GetSubTask());
		return pTaskFall->CanBreakoutToMovementTask();
	}
	return GetState() == State_Land;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::GetHasInterruptedClip(const CPed* pPed) const
{
	// break out on slopes.
	if(m_networkHelper.GetBoolean( ms_SlopeBreakoutId ))
	{
		if(m_iFlags.IsFlagSet(JF_JumpingUpHill))
		{
			return true;
		}
	}

	if(m_networkHelper.GetBoolean( ms_InterruptibleId ))
	{
		if(m_bStandingLand)
		{
			if(!pPed->GetSteepSlopePos().IsZero())
			{
				return true;
			}
			else if(IsPedTryingToMove() || 
				pPed->IsUsingActionMode() || 
				pPed->GetPedResetFlag( CPED_RESET_FLAG_FollowingRoute ) || 
				pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE) ||
				pPed->GetMotionData()->GetUsingStealth())
			{
				return true;
			}

			// Allow AI to break out.
			if(!pPed->IsAPlayerPed())
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	// Interrupt if the Player is aiming a weapon
	if(m_networkHelper.GetBoolean( ms_AimInterruptId ) )
	{
		if (pPed->GetPlayerInfo() && (pPed->GetPlayerInfo()->IsHardAiming() || pPed->GetPlayerInfo()->IsFiring()) )
		{
			return true;
		}
	}

	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, FIRST_PERSON_LAND_INTERRUPT_TIME, 0.0f, 0.f, 10.f, 0.1f);
		if(GetTimeInState() >= FIRST_PERSON_LAND_INTERRUPT_TIME)
		{
			return true;
		}
	}

	return false;
}

const CControl *CTaskJump::GetControlFromPlayer(const CPed* pPed) const
{
	if(m_IsQuadrupedJump)
	{
		CPed* pDriver = pPed->GetSeatManager() ? pPed->GetSeatManager()->GetDriver() : NULL;
		if (pDriver && pDriver->IsLocalPlayer())
		{
			return pDriver->GetControlFromPlayer();
		}
	}

	return pPed->GetControlFromPlayer();
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::HasReachedApex() const
{
	if(m_fApexClipPhase > 0.0f)
	{
		return m_fClipLastPhase >= m_fApexClipPhase;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::CheckForBlockedJump(const CPed* pPed, float fTestDistance, float fTestRadius, float fTestZOffset)
{
	Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vStart.z += fTestZOffset;
	Vector3 vEnd = vStart + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()) * fTestDistance);

	//Check for solid objects between the ped and the position.
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
	capsuleDesc.SetResultsStructure(&shapeTestResults);
	capsuleDesc.SetCapsule(vStart, vEnd, fTestRadius);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	capsuleDesc.SetExcludeEntity(pPed);
	
#if __DEV
	CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), fTestRadius, Color_green, 2500, 0, false);	
#endif

	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::TestForObstruction(float fTestDistance, float fTestRadius, float& distanceToObstruction, CEntity** pObstructionEntity, Vec3V& normal) const
{
	const CPed *pPed = GetPed();

	Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vStart.z += sm_Tunables.m_PredictiveProbeZOffset;
	Vector3 vEnd = vStart + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()) * fTestDistance);

	//Check for solid objects between the ped and the position.
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
	capsuleDesc.SetResultsStructure(&shapeTestResults);
	capsuleDesc.SetCapsule(vStart, vEnd, fTestRadius);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER | ArchetypeFlags::GTA_RAGDOLL_TYPE);
	capsuleDesc.SetExcludeEntity(GetPed());
	capsuleDesc.SetIsDirected(true);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
		if(PGTAMATERIALMGR->GetPolyFlagStairs(shapeTestResults[0].GetMaterialId())) 
		{ 
			return false; 
		}

		//! Check angle of indidence. Don't want to ragdoll when we brush against wall.
		Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		Vector3 vNormal2D = shapeTestResults[0].GetHitPolyNormal();
		vNormal2D.z = 0.0f;
		vNormal2D.NormalizeSafe();
		float fIntersectionDot = DotProduct(vPlayerForward, vNormal2D);
		if(fIntersectionDot > sm_Tunables.m_PredictiveRagdollIntersectionDot)
		{
			return false;
		}

		CEntity* pHitEnt = shapeTestResults[0].GetHitEntity();
		if (pHitEnt)
		{
			*pObstructionEntity = pHitEnt;
		}
		Vec3V hitPos = VECTOR3_TO_VEC3V(shapeTestResults[0].GetHitPosition());
		normal = VECTOR3_TO_VEC3V(shapeTestResults[0].GetHitPolyNormal());
		distanceToObstruction = Mag((hitPos - RC_VEC3V(vStart))).Getf();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

// Exclude extremities for determine if the player should auto-ragdoll
REGISTER_TUNE_GROUP_INT(iRagdollOnCollisionAllowedPartsMask, 0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT) | 
						BIT(RAGDOLL_LOWER_ARM_LEFT) | BIT(RAGDOLL_LOWER_ARM_RIGHT) | BIT(RAGDOLL_UPPER_ARM_LEFT) | BIT(RAGDOLL_UPPER_ARM_RIGHT)));

void CTaskJump::ActivateRagdollOnCollision(CPhysical *pPhysical, float fPenetration, float fSlope, bool bAllowFixed)
{
#if __BANK
	if(!hasAddedBankItem_iRagdollOnCollisionAllowedPartsMask)
	{
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");
		if(pBank)
		{
			bkGroup* pGroup = GetOrCreateGroup(pBank, "PED_JUMPING");
			pBank->SetCurrentGroup(*pGroup);
			pBank->PushGroup("iRagdollOnCollisionAllowedPartsMask", false);
			extern parEnumData parser_RagdollComponent_Data;
			for(int i = 0; i < parser_RagdollComponent_Data.m_NumEnums; i++)
			{
				pBank->AddToggle(parser_RagdollComponent_Data.m_Names[i], reinterpret_cast<u32*>(&iRagdollOnCollisionAllowedPartsMask), 1 << i);
			}
			pBank->PopGroup();
			pBank->UnSetCurrentGroup(*pGroup);
			hasAddedBankItem_iRagdollOnCollisionAllowedPartsMask = true;
		}
	}
#endif

	fwFlags32 iFlags = (m_bLeftFootLaunch ? FF_ForceJumpFallLeft : FF_ForceJumpFallRight) | FF_ForceHighFallNM;
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskFall(iFlags), false, E_PRIORITY_GIVE_PED_TASK, true);
	GetPed()->SetActivateRagdollOnCollisionEvent(event);
	GetPed()->SetActivateRagdollOnCollision(true, bAllowFixed);
	GetPed()->SetRagdollOnCollisionAllowedPenetration(fPenetration);
	GetPed()->SetRagdollOnCollisionAllowedSlope(fSlope);

	// Don't bother activating the ragdoll for collisions with only the hands/feet...
	GetPed()->SetRagdollOnCollisionAllowedPartsMask(iRagdollOnCollisionAllowedPartsMask);

	GetPed()->SetRagdollOnCollisionIgnorePhysical(pPhysical);
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::ResetRagdollOnCollision()
{
	CPed *pPed = GetPed();

	// If we didn't collide with anything that activated our ragdoll then clear the event
	if(pPed->GetActivateRagdollOnCollision() && pPed->GetActivateRagdollOnCollisionEvent() != NULL && pPed->GetActivateRagdollOnCollisionEvent()->GetEventType() == EVENT_GIVE_PED_TASK &&
	  static_cast<const CEventGivePedTask*>(pPed->GetActivateRagdollOnCollisionEvent())->GetTask() != NULL && 
	   static_cast<const CEventGivePedTask*>(pPed->GetActivateRagdollOnCollisionEvent())->GetTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		GetPed()->SetActivateRagdollOnCollision(false);
		GetPed()->ClearActivateRagdollOnCollisionEvent();
		GetPed()->SetRagdollOnCollisionIgnorePhysical(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskJump::CreateQueriableState() const
{
	Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	CTaskInfo* pInfo = rage_new CClonedTaskJumpInfo(GetState(), 
		m_iFlags.GetAllFlags(), 
		m_vHandHoldPoint, 
		m_fQuadrupedJumpDistance, 
		m_bStandingLand, 
		m_fJumpAngle,
		m_JumpHelper.WasLandingFound(),
		m_JumpHelper.GetLandingPositionForSync(),
		vPedPos,
		m_JumpHelper.GetLandingPhysical(),
		m_JumpHelper.GetHorizontalJumpDistance()
#if FPS_MODE_SUPPORTED
        , m_fFPSInitialDesiredHeading
#endif // FPS_MODE_SUPPORTED
		, GetPed()->GetVelocity()
		, GetPed()->GetDesiredHeading()
        , m_bUseMaximumSuperJumpVelocity
        , m_bHasAppliedSuperJumpVelocityLocally
        );
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskJump::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_JUMP);

	CClonedTaskJumpInfo *jumpInfo = static_cast<CClonedTaskJumpInfo*>(pTaskInfo);

	m_vSyncedVelocity = jumpInfo->GetPedVelocity();
	m_SynchedHeading = jumpInfo->GetPedHeading();
	m_vHandHoldPoint = jumpInfo->GetHandholdPoint();
	m_fQuadrupedJumpDistance = jumpInfo->GetQuadrupedJumpDistance();
	m_bStandingLand = jumpInfo->GetStandingLand();
	m_fJumpAngle = jumpInfo->GetJumpAngle();

	CEntity *pAssistJumpEntity = jumpInfo->GetAssistedJumpEntity();
	CPhysical *pAssistJumpPhysical = pAssistJumpEntity && pAssistJumpEntity->GetIsPhysical() ? static_cast<CPhysical*>(pAssistJumpEntity) : NULL;

	m_JumpHelper.ForceLandData(jumpInfo->IsAssistedJump(), jumpInfo->GetAssistedJumpPosition(), pAssistJumpPhysical, jumpInfo->GetAssistedJumpDistance());

	m_vOwnerAssistedJumpStartPosition = jumpInfo->GetAssistedJumpStartPosition();
	m_vOwnerAssistedJumpStartPositionOffset = jumpInfo->GetAssistedJumpStartPositionOffset();

#if FPS_MODE_SUPPORTED
    m_fFPSInitialDesiredHeading = jumpInfo->GetFPSInitialDesiredHeading();
#endif // FPS_MODE_SUPPORTED

    m_bUseMaximumSuperJumpVelocity         = jumpInfo->IsUsingMaximumSuperJumpVelocity();
    m_bHasAppliedSuperJumpVelocityRemotely =jumpInfo->HasAppliedSuperJumpVelocityRemotely();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskJump::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	if(GetStateFromNetwork() == State_Quit)
	{
		if(GetState()==State_Land)
		{
			StateLand_OnExit(GetPed());
		}
		return FSM_Quit;
	}
	if(GetStateFromNetwork()== State_Land)
	{
		//! Correct fall if we get it wrong.
		if(GetState() == State_Fall)
		{
			SetState(State_Land);
		}
	}

	return UpdateFSM(iState, iEvent);
}

//////////////////////////////////////////////////////////////////////////

void CTaskJump::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Quit);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::IsInScope(const CPed* pPed)
{
	if(GetSubTask()  && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		if(GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_PARACHUTE)
		{
			return true;
		}
	}
	else if(pPed && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetQueriableInterface() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
	{
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_PARACHUTE, PED_TASK_PRIORITY_MAX))
		{
			return true;
		}
	}

	return CTaskFSMClone::IsInScope(pPed);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskJump::OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) 
{
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		return false;
	}

	if(GetPed()->GetIsAttached() || m_bScalingVelocity)
	{
		return true;
	}

	if(GetPed()->GetGroundVelocityForJump().Mag2() > rage::square(2.0f))
	{
		return true;
	}

	static dev_bool s_bOverrideNetBlender = false;
	return s_bOverrideNetBlender; 
}

//////////////////////////////////////////////////////////////////////////

CTaskFSMClone*	CTaskJump::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskJump *newTask = NULL;
	
	//! DMKH. Don't support migration yet.
	//newTask = rage_new CTaskJump(m_iFlags);
	
	return newTask;
}

//////////////////////////////////////////////////////////////////////////

CTaskFSMClone*	CTaskJump::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskJump::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"State_Init",
		"State_Launch",
		"State_Land",
		"State_Fall",
		"State_Quit",
	};
	return aStateNames[iState];
}

void CTaskJump::Debug() const
{
#if DEBUG_DRAW	// TODO: Clean up all the DEBUG_DRAW_ONLY calls in this block.
	if (m_IsQuadrupedJump)
	{			
		//! get auto jump info from climb detector.
		if(m_iFlags.IsFlagSet(JF_AutoJump))
		{
			// Render the target point
			DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vHandHoldPoint, 0.1f, Color_red));
		}
	}
#endif


	CTask::Debug();
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Task serialization info for TaskJump2
//////////////////////////////////////////////////////////////////////////

CClonedTaskJumpInfo::CClonedTaskJumpInfo(s32 state, 
										 u32 flags, 
										 const Vector3 &vHandHoldPoint, 
										 float fQuadrupedJumpDistance, 
										 bool bStandingLand, 
										 float fJumpAngle,
										 bool bAssistedJump,
										 const Vector3 &vAssistedJumpPosition,
										 const Vector3 &vAssistedJumpStartPosition,
										 CPhysical *pAssistedJumpPhysical,
										 float fAssistedJumpDistance
#if FPS_MODE_SUPPORTED
                                         , float fFPSInitialDesiredHeading
#endif // FPS_MODE_SUPPORTED
										 , Vector3 velocity
										 , float heading
                                         , bool bUseMaximumSuperJumpVelocity
                                         , bool bHasAppliedSuperJumpVelocityRemotely
                                         ) 
: m_Flags(flags)
, m_vHandHoldPoint(vHandHoldPoint)
, m_fQuadrupedJumpDistance(fQuadrupedJumpDistance)
, m_bStandingLand(bStandingLand)
, m_fJumpAngle(fJumpAngle)
, m_bAssistedJump(bAssistedJump)
, m_vAssistedJumpPosition(vAssistedJumpPosition)
, m_vAssistedJumpStartPosition(vAssistedJumpStartPosition)
, m_vAssistedJumpStartPositionOffset(vAssistedJumpStartPosition)
, m_fAssistedJumpDistance(fAssistedJumpDistance)
#if FPS_MODE_SUPPORTED
, m_fFPSInitialDesiredHeading(fFPSInitialDesiredHeading)
#endif // FPS_MODE_SUPPORTED
, m_pedVelocity(velocity)
, m_pedHeading(heading)
, m_bUseMaximumSuperJumpVelocity(bUseMaximumSuperJumpVelocity)
, m_bHasAppliedSuperJumpVelocityRemotely(bHasAppliedSuperJumpVelocityRemotely)
{
	SetStatusFromMainTaskState(state);

	m_pAssistedJumpPhysical.SetEntity(pAssistedJumpPhysical);

	netObject *networkObject = pAssistedJumpPhysical ? NetworkUtils::GetNetworkObjectFromEntity(pAssistedJumpPhysical) : 0;
	if(networkObject)
	{
		m_pAssistedJumpPhysical.SetEntity(pAssistedJumpPhysical);
		
		//! If attached to jump physical, store position as offset from it.
		Matrix34 mPhysical = MAT34V_TO_MATRIX34(pAssistedJumpPhysical->GetMatrix());
		mPhysical.UnTransform(m_vAssistedJumpStartPositionOffset);
	}
	else
	{
		m_pAssistedJumpPhysical.Invalidate();
	}
}

CClonedTaskJumpInfo::CClonedTaskJumpInfo() 
: m_Flags(0)
, m_vHandHoldPoint(VEC3_ZERO)
, m_fQuadrupedJumpDistance(0.0f)
, m_bStandingLand(false)
, m_fJumpAngle(0.0f)
, m_bAssistedJump(false)
, m_vAssistedJumpPosition(VEC3_ZERO)
, m_vAssistedJumpStartPosition(VEC3_ZERO)
, m_vAssistedJumpStartPositionOffset(VEC3_ZERO)
, m_fAssistedJumpDistance(0.0f)
#if FPS_MODE_SUPPORTED
, m_fFPSInitialDesiredHeading(FLT_MAX)
#endif // FPS_MODE_SUPPORTED
, m_pedVelocity(VEC3_ZERO)
, m_pedHeading(0.0f)
, m_bUseMaximumSuperJumpVelocity(false)
, m_bHasAppliedSuperJumpVelocityRemotely(false)
{
	m_pAssistedJumpPhysical.Invalidate();
}

CTaskFSMClone * CClonedTaskJumpInfo::CreateCloneFSMTask()
{
	CTaskJump* pTaskJump = NULL;

	pTaskJump =rage_new CTaskJump(m_Flags);

	return pTaskJump;
}

//////////////////////////////////////////////////////////////////////////
