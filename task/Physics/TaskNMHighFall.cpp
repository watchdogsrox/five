// Filename   :	TaskNMHighFall.cpp
// Description:	Natural Motion high fall class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\TaskAnimatedFallback.h"
#include "Task\Movement\TaskFall.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task\Movement\Jumping\TaskJump.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMSimple.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMHighFall::Tunables CTaskNMHighFall::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMHighFall, 0x6681565a);

#if __BANK
extern const char* parser_RagdollComponent_Strings[];

void CTaskNMHighFall::Tunables::PreAddWidgets(bkBank& bank)
{
	bank.PushGroup("Ragdoll Component Air Resistance Force", false);
	for (int i = 0; i < RAGDOLL_NUM_COMPONENTS; i++)
	{
		bank.AddSlider(parser_RagdollComponent_Strings[i], &m_RagdollComponentAirResistanceForce[i], -10000.0f, 10000.0f, 0.1f);
	}
	bank.PopGroup();
	bank.PushGroup("Ragdoll Component Air Resistance Min Stiffness", false);
	for (int i = 0; i < RAGDOLL_NUM_JOINTS; i++)
	{
		bank.AddSlider(parser_RagdollComponent_Strings[i + (RAGDOLL_NUM_COMPONENTS - RAGDOLL_NUM_JOINTS)], &m_RagdollComponentAirResistanceMinStiffness[i], 0.0f, 1.0f, 0.01f);
	}
	bank.PopGroup();
}
#endif

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CClonedNMHighFallInfo
//////////////////////////////////////////////////////////////////////////
CClonedNMHighFallInfo::CClonedNMHighFallInfo(u32 highFallType) 
: m_highFallType(highFallType) 
{
}

CClonedNMHighFallInfo::CClonedNMHighFallInfo() 
: m_highFallType(0) 
{
}

CTaskFSMClone *CClonedNMHighFallInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMHighFall(10000, NULL, static_cast<CTaskNMHighFall::eHighFallEntryType>(m_highFallType));
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMHighFall
//////////////////////////////////////////////////////////////////////////
CTaskNMHighFall::CTaskNMHighFall(u32 nMinTime, const CGrabHelper* pGrabHelper, eHighFallEntryType eType, const Vector3 *pitchDirOverride, bool blockEarlyGetup)
:	CTaskNMBehaviour(nMinTime, MAX_UINT16)
,	m_GrabHelper(pGrabHelper)
,	m_SideStabilizationApplied(false)
,	m_foundgrab(false)
,	m_eType(eType)
,	m_BlockEarlyGetup(blockEarlyGetup)
,	m_bUsePitchInDirectionForce(false)
,	m_bDoingHighHighFall(false)
,	m_bDoingSuperHighFall(false)
,   m_bNetworkBlenderOffForHighFall(false)
{
#if !__FINAL
	m_strTaskAndStateName = "NMHighFall";
#endif // !__FINAL
		
	if (pitchDirOverride)
		m_PitchDirOverride = *pitchDirOverride;

	if (m_eType == HIGHFALL_IN_AIR || m_eType== HIGHFALL_VAULT)
		m_bUsePitchInDirectionForce=true;

	if (m_eType==HIGHFALL_SPRINT_EXHAUSTED)
		m_bBalanceFailed = false;
	else
		m_bBalanceFailed = true;

	SetInternalTaskType(CTaskTypes::TASK_NM_HIGH_FALL);
}

CTaskNMHighFall::CTaskNMHighFall(const CTaskNMHighFall& otherTask)
: CTaskNMBehaviour(otherTask)
, m_bBalanceFailed(otherTask.m_bBalanceFailed)
, m_GrabHelper(otherTask.m_GrabHelper)
, m_SideStabilizationApplied(otherTask.m_SideStabilizationApplied)
, m_foundgrab(otherTask.m_foundgrab)
, m_eType(otherTask.m_eType)
, m_BlockEarlyGetup(otherTask.m_BlockEarlyGetup)
, m_PitchDirOverride(otherTask.m_PitchDirOverride)
, m_bUsePitchInDirectionForce(otherTask.m_bUsePitchInDirectionForce)
, m_bDoingHighHighFall(otherTask.m_bDoingHighHighFall)
, m_bDoingSuperHighFall(otherTask.m_bDoingSuperHighFall)
{
#if !__FINAL
	m_strTaskAndStateName = "NMHighFall";
#endif // !__FINAL

	SetInternalTaskType(CTaskTypes::TASK_NM_HIGH_FALL);
}

CTaskNMHighFall::~CTaskNMHighFall()
{
}

CTaskInfo* CTaskNMHighFall::CreateQueriableState() const
{
	return rage_new CClonedNMHighFallInfo(m_eType);
}


void CTaskNMHighFall::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	//	m_bHasFailed = true;
	//	m_nSuggestedNextTask = CTaskTypes::TASK_SIMPLE_NM_RELAX;

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB) && GetEntryType()==HIGHFALL_SPRINT_EXHAUSTED)
	{
		sm_Tunables.m_OnBalanceFailedSprintExhausted.Post(*pPed, this);
		m_bBalanceFailed = true;
	}

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_HIGHFALL_FB))
	{
		m_bBalanceFailed = true;
		//		GetGrabHelper().SetStatusFlag(CGrabHelper::BALANCE_STATUS_FAILED);

	}
	// pass the message on to the grab helper
	GetGrabHelper().BehaviourFailure(pPed, pFeedbackInterface);

}

void CTaskNMHighFall::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	bool successCompString = CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_HIGHFALL_FB);

	// check min time and then flag success
	if(successCompString && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
	}
	// pass the message on to the grab helper
	if(!successCompString)
		GetGrabHelper().BehaviourSuccess(pPed, pFeedbackInterface);

}

void CTaskNMHighFall::BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFinish(pPed, pFeedbackInterface);

	// pass the message on to the grab helper
	GetGrabHelper().BehaviourFinish(pPed, pFeedbackInterface);
}

void CTaskNMHighFall::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourEvent(pPed, pFeedbackInterface);

	/*if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)
	{
		if(CTaskNMBehaviour::ProcessBalanceBehaviourEvent(pPed, pFeedbackInterface))
		{
			m_bHasSucceeded = true;
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
		}
	}*/
}

void CTaskNMHighFall::AddStartMessages(CPed* pPed, CNmMessageList& UNUSED_PARAM(list))
{
	if (!sm_Tunables.m_DisableStartMessageForSprintExhausted || m_eType!=HIGHFALL_SPRINT_EXHAUSTED)
		AddTuningSet(&sm_Tunables.m_Start);

	switch (m_eType)
	{
	case HIGHFALL_IN_AIR: AddTuningSet(&sm_Tunables.m_InAir); break;
	case HIGHFALL_VAULT: AddTuningSet(&sm_Tunables.m_Vault); break;
	case HIGHFALL_FROM_CAR_HIT: AddTuningSet(&sm_Tunables.m_FromCarHit); break;
	case HIGHFALL_SLOPE_SLIDE: AddTuningSet(&sm_Tunables.m_SlopeSlide); break;
	case HIGHFALL_TEETER_EDGE: AddTuningSet(&sm_Tunables.m_TeeterEdge); break;
	case HIGHFALL_SPRINT_EXHAUSTED: AddTuningSet(&sm_Tunables.m_SprintExhausted); break;
	case HIGHFALL_STUNT_JUMP: AddTuningSet(&sm_Tunables.m_StuntJump); break;
	case HIGHFALL_JUMP_COLLISION: AddTuningSet(&sm_Tunables.m_JumpCollision); break;
	default: break;// do nothing 
	}

	if(m_eType != HIGHFALL_SLOPE_SLIDE && pPed && pPed->GetSpeechAudioEntity())
		pPed->GetSpeechAudioEntity()->RegisterFallingStart();
}

void CTaskNMHighFall::StartBehaviour(CPed* pPed)
{
#if !__FINAL
	// On restarting the task, reset the task name display for debugging the state.
	m_strTaskAndStateName = "NMHighFall";
	nmTaskDebugf(this, "starting task - entry type:%s", GetEntryTypeName(GetEntryType()));
#endif // !__FINAL

	m_StartingHealth = pPed->GetHealth();

	if (pPed->GetHasJustLeftVehicle())
	{
		// In some cases legitimate bails from land vehicles are triggered in a HighFall state. 
		CStatsMgr::StartBailVehicle(pPed);
	}	

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsFalling, true);
}

void CTaskNMHighFall::CleanUp()
{
	CPed* pPed  = GetPed();

	if (pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);
	}
}

dev_float OFFSET_FOR_PROBE=0.2f;	
dev_float PROBE_RADIUS=0.1f;	

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMHighFall::ControlBehaviour(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&sm_Tunables.m_Update);

	Vector3 vVelocity = pPed->GetVelocity();

	if (m_bUsePitchInDirectionForce && sm_Tunables.m_PitchInDirectionForce.ShouldApply(m_nStartTime))
	{
		// Calculate the normalized force vector (a force in the direction of the peds xy movement
		Vec3V impulseVec = VECTOR3_TO_VEC3V(m_eType == HIGHFALL_VAULT ? m_PitchDirOverride : vVelocity);
		impulseVec.SetZ(ScalarV(V_ZERO));
		Vec3V velVec = impulseVec;
		impulseVec = NormalizeSafe(impulseVec, Vec3V(V_ZERO));

		if (MagSquared(impulseVec).Getf()>0.0f)
		{
			// If this is a vault-fall, only allow linear velocity in the override pitch direction
			if (m_eType == HIGHFALL_VAULT && !m_SideStabilizationApplied)
			{
				phArticulatedBody *body = pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body;
				body->EnsureVelocitiesFullyPropagated();
				m_PitchDirOverride.z = 0.0f;
				m_PitchDirOverride.NormalizeSafe();
				for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
				{
					body->SetAngularVelocity(iLink, Vec3V(V_ZERO));
					Vector3 linVel = VEC3V_TO_VECTOR3(body->GetLinearVelocity(iLink));
					float oldZ = linVel.z;
					Vector3 vecMotion = m_PitchDirOverride * m_PitchDirOverride.Dot(linVel);
					vecMotion.z = oldZ;
					body->SetLinearVelocity(iLink, VECTOR3_TO_VEC3V(vecMotion));
				}
				body->ResetPropagationVelocities();

				m_SideStabilizationApplied = true;
			}

			// Get impulse from tuning
			sm_Tunables.m_PitchInDirectionForce.GetImpulseVec(impulseVec, velVec, pPed);

			// send to nm
			pPed->ApplyImpulse(RCC_VECTOR3(impulseVec), VEC3_ZERO, sm_Tunables.m_PitchInDirectionComponent, true);
		}
	}

	if (m_eType==HIGHFALL_STUNT_JUMP && sm_Tunables.m_StuntJumpPitchInDirectionForce.ShouldApply(m_nStartTime))
	{
		// Calculate the normalized force vector (a force in the direction of the peds xy movement
		Vec3V impulseVec = VECTOR3_TO_VEC3V(m_eType == HIGHFALL_VAULT ? m_PitchDirOverride : vVelocity);
		impulseVec.SetZ(ScalarV(V_ZERO));
		Vec3V velVec = impulseVec;
		impulseVec = NormalizeSafe(impulseVec, Vec3V(V_ZERO));

		if (MagSquared(impulseVec).Getf()>0.0f)
		{
			// Get impulse from tuning
			sm_Tunables.m_StuntJumpPitchInDirectionForce.GetImpulseVec(impulseVec, velVec, pPed);

			// send to nm
			pPed->ApplyImpulse(RCC_VECTOR3(impulseVec), VEC3_ZERO, sm_Tunables.m_StuntJumpPitchInDirectionComponent, true);
		}
	}

	// Update the peds in air status for other ai systems

	bool bTouchingGround = false;
	if(pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		const CCollisionRecord* pCollisionRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfTypes(ENTITY_TYPE_VEHICLE | ENTITY_TYPE_BUILDING | ENTITY_TYPE_ANIMATED_BUILDING);
		if (pCollisionRecord && pCollisionRecord->m_MyCollisionNormal.z > 0.7f)
		{
			bTouchingGround = true;
		}
	}
	if (!bTouchingGround)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, true);

		// Don't scream if we're already screaming, or jumping from a sailboat.
		if(pPed->GetSpeechAudioEntity() 
			&& !pPed->GetSpeechAudioEntity()->GetIsFallScreaming()
			&& !(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasJustLeftCar) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BOAT))
		{
			// Perform VO
			audDamageStats damageStats;
			damageStats.DamageReason = AUD_DAMAGE_REASON_FALLING;
			pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
		}
	}
	else
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);
	}

	float fDistanceZ = WORLDLIMITS_ZMIN;
	m_bNetworkBlenderOffForHighFall |= !CTaskJump::IsPedNearGround(pPed, sm_Tunables.m_DistanceZThresholdForHighHighFall, VEC3_ZERO, fDistanceZ);

	fDistanceZ -= pPed->GetTransform().GetPosition().GetZf();

	if(fDistanceZ < -sm_Tunables.m_DistanceZThresholdForHighHighFall)
	{
		if(vVelocity.z < sm_Tunables.m_VelocityZThresholdForSuperHighFall)
		{
			if(!m_bDoingSuperHighFall)
			{
				AddTuningSet(&sm_Tunables.m_SuperHighFallStart);
				m_bDoingSuperHighFall = true;
				m_bDoingHighHighFall = false;
			}
		}
		else if(vVelocity.z < sm_Tunables.m_VelocityZThresholdForHighHighFall)
		{
			if(!m_bDoingHighHighFall)
			{
				AddTuningSet(&sm_Tunables.m_HighHighFallStart);
				m_bDoingSuperHighFall = false;
				m_bDoingHighHighFall = true;
			}
		}
		else if(m_bDoingHighHighFall || m_bDoingSuperHighFall)
		{
			AddTuningSet(&sm_Tunables.m_HighHighFallEnd);
			m_bDoingSuperHighFall = false;
			m_bDoingHighHighFall = false;
		}
	}
	else if(m_bDoingHighHighFall || m_bDoingSuperHighFall)
	{
		AddTuningSet(&sm_Tunables.m_HighHighFallEnd);
		m_bDoingSuperHighFall = false;
		m_bDoingHighHighFall = false;
	}

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());
	
	if(sm_Tunables.m_AirResistanceOption == 1)
	{
		ART::MessageParams msgHealth;
		msgHealth.addFloat("characterHealth", 1.0f);
		pPed->GetRagdollInst()->PostARTMessage("setCharacterHealth", &msgHealth);
	}

	// Don't roll on the ground in pain after a high fall
	if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + sm_Tunables.m_HighFallTimeToBlockInjuredOnGround)
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().DisableInjuredOnGroundBehaviour();
	}
}

bool CTaskNMHighFall::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	return true;
}

bool CTaskNMHighFall::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	CPed* pPed = GetPed();
	if(pPed != NULL && pPed->GetUsingRagdoll())
	{
		if((m_bDoingHighHighFall || m_bDoingSuperHighFall) && (sm_Tunables.m_AirResistanceOption != 3 || pPed->IsLocalPlayer()) && (sm_Tunables.m_AirResistanceOption != 2 || !pPed->IsDead()))
		{
			float fTotalForce = 0.0f;
			for(int i = 0; i < RAGDOLL_NUM_COMPONENTS; i++)
			{
				if(sm_Tunables.m_RagdollComponentAirResistanceForce[i] != 0.0f)
				{
					fTotalForce += sm_Tunables.m_RagdollComponentAirResistanceForce[i];
				}
			}

			// The air resistance forces we're applying need to equal out to zero overall
			// If there was extra force applied (either positively or negatively) then we counteract it by applying some extra impulse to the spine
			float fExtraForcePerSpineComponent = 0.0f;
			if(fTotalForce != 0.0f)
			{
				fExtraForcePerSpineComponent = -(fTotalForce / (RAGDOLL_SPINE3 - RAGDOLL_SPINE0 + 1));
			}

			ASSERT_ONLY(fTotalForce = 0.0f;)
			for(int i = 0; i < RAGDOLL_NUM_COMPONENTS; i++)
			{
				float fForce = sm_Tunables.m_RagdollComponentAirResistanceForce[i];
				if(i >= RAGDOLL_SPINE0 && i <= RAGDOLL_SPINE3)
				{
					fForce += fExtraForcePerSpineComponent;
				}

				if(sm_Tunables.m_AirResistanceOption == 4)
				{
					// Get the peds current health and convert it to appropriate nm health
					fForce *= RampValueSafe(pPed->GetHealth(), pPed->GetInjuredHealthThreshold(), pPed->GetMaxHealth(), 0.0f, 1.0f);
				}

				if(fForce != 0.0f)
				{
					ASSERT_ONLY(fTotalForce += fForce;)
					pPed->ApplyImpulse(Vector3(0.0f, 0.0f, fForce * fTimeStep), VEC3_ZERO, i, true);
				}
			}

			nmAssertf(IsClose(fTotalForce, 0.0f, 1.0f), "CTaskNMHighFall::ProcessPhysics: Air resistance forces don't cancel each other out (%f != 0.0)", fTotalForce);

			phArticulatedCollider* pCollider = ((phArticulatedCollider*)pPed->GetCollider());
			if (pCollider)
			{
				phArticulatedBody* pArticulatedBody = pCollider->GetBody();
				if(pArticulatedBody)
				{
					for(int i = 0; i < RAGDOLL_NUM_JOINTS; i++)
					{
						pArticulatedBody->GetJoint(i).SetMinStiffness(sm_Tunables.m_RagdollComponentAirResistanceMinStiffness[i]);
					}
				}
			}
		}
	}

	return CTaskNMBehaviour::ProcessPhysics(fTimeStep, nTimeSlice);
}

dev_float snHighFallPlayerTimeoutBeforeVelCheck = 10000;
//
bool CTaskNMHighFall::FinishConditions(CPed* pPed)
{
	int nFlags = 0;

	nFlags |= FLAG_QUIT_IN_WATER;

	if(m_bBalanceFailed && !GetGrabHelper().GetFlag(CGrabHelper::GRAB_SUCCEEDED_LHAND) && !GetGrabHelper().GetFlag(CGrabHelper::GRAB_SUCCEEDED_RHAND))
		nFlags |= FLAG_VEL_CHECK;

	if (pPed && (!pPed->IsPlayer() || pPed->GetHealth()<=pPed->GetInjuredHealthThreshold()))
		nFlags |= FLAG_RELAX_AP_LOW_HEALTH;

	// for the player we want to use a vel check even if the balance hasn't failed (or succeeded)
	// after a certain amount of time, so you can't get stuck in a pose where you're unable to balance
	// (wedgied on a car door for example)
	if(pPed->IsPlayer() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + snHighFallPlayerTimeoutBeforeVelCheck)
		nFlags |= FLAG_VEL_CHECK;

	nFlags |= FLAG_SKIP_DEAD_CHECK;

	const CTaskNMBehaviour::Tunables::BlendOutThreshold* pThreshold = NULL;
	if (m_BlockEarlyGetup)
	{
		pThreshold = &CTaskNMBrace::sm_Tunables.m_HighVelocityBlendOut.PickBlendOut(pPed);
	}
	else if (pPed->IsPlayer())
	{
		if (NetworkInterface::IsGameInProgress())
		{
			if (( (pPed->GetHealth() - m_StartingHealth) < sm_Tunables.m_MpMaxHealthLossForQuickGetup ) && (pPed->GetHealth()>sm_Tunables.m_MpMinHealthForQuickGetup))
			{
				pThreshold = &sm_Tunables.m_MpPlayerQuickBlendOut;
			}
			else
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, true);
				pThreshold = &sm_Tunables.m_BlendOut.m_PlayerMp;
			}
		}
		else
		{
			if (( (m_StartingHealth - pPed->GetHealth()) < sm_Tunables.m_MaxHealthLossForQuickGetup ) && (pPed->GetHealth()>sm_Tunables.m_MinHealthForQuickGetup))
			{
				pThreshold = &sm_Tunables.m_PlayerQuickBlendOut;
			}
			else
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, true);
				pThreshold = &sm_Tunables.m_BlendOut.m_Player;
			}
		}
	}
	else
	{
		pThreshold = &sm_Tunables.m_BlendOut.m_Ai;
	}

	bool bResult = ProcessFinishConditionsBase(pPed, MONITOR_FALL, nFlags, pThreshold);

	// if we can't blend out because of the minimum time, don't lie around like a corpse.
	// Switch to a ground writhe behaviour instead
	if (!bResult 
		&& !pPed->ShouldBeDead() 
		&& !pPed->IsFatallyInjured() 
		&& (fwTimer::GetTimeInMilliseconds()+ sm_Tunables.m_MinTimeRemainingForGroundWrithe) < (m_nStartTime + m_nMinTime )
		&& (fwTimer::GetTimeInMilliseconds() > (m_nStartTime + sm_Tunables.m_MinTimeElapsedForGroundWrithe))
		&& pThreshold 
		&& CheckBlendOutThreshold(pPed, *pThreshold))
	{
		const CTaskNMSimple::Tunables::Tuning* pSimpleTuningSet = CTaskNMSimple::sm_Tunables.GetTuning(atHashString("HighFall_TimeoutGroundWrithe"));
		if (pSimpleTuningSet)
		{
			u32 timeRemaining = (m_nStartTime + m_nMinTime )  - fwTimer::GetTimeInMilliseconds();
			CTaskNMSimple* pNewTask = NULL;
			
			if (sm_Tunables.m_UseRemainingMinTimeForGroundWrithe)
			{
				pNewTask = rage_new CTaskNMSimple(*pSimpleTuningSet, Min(timeRemaining, static_cast<u32>(pSimpleTuningSet->m_iMaxTime)), pSimpleTuningSet->m_iMaxTime);
			}
			else
			{
				pNewTask = rage_new CTaskNMSimple(*pSimpleTuningSet);
			}

			if (pNewTask)
			{
				GetControlTask()->ForceNewSubTask(pNewTask);
				return true;
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMHighFall::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
//////////////////////////////////////////////////////////////////////////
{
	// Handles any new vehicle impacts
	if (pEntity != NULL && pEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pEntity)->GetVelocity().Mag2() < 0.3f)
	{
		return true;
	}

	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}

void CTaskNMHighFall::StartAnimatedFallback(CPed* pPed)
{
	CTask* pNewTask = NULL;
	if (CTaskJump::IsPedNearGround(pPed, CTaskFall::GetMaxPedFallHeight(pPed)))
	{
		fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
		fwMvClipId clipId = CLIP_ID_INVALID;

		Vector3 vecHeadPos(0.0f,0.0f,0.0f);
		pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
		// If the ped is already prone then use an on-ground damage reaction
		if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
		{
			const EstimatedPose pose = pPed->EstimatePose();
			if (pose == POSE_ON_BACK)
			{
				clipId = CLIP_DAM_FLOOR_BACK;
			}
			else
			{
				clipId = CLIP_DAM_FLOOR_FRONT;
			}

			Matrix34 rootMatrix;
			if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
			{
				float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
				if (fAngle < -QUARTER_PI)
				{
					clipId = CLIP_DAM_FLOOR_LEFT;
				}
				else if (fAngle > QUARTER_PI)
				{
					clipId = CLIP_DAM_FLOOR_RIGHT;
				}
			}

			clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();

			pNewTask = rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f);
		}
		else
		{
			float fStartPhase = 0.0f;
			// If this task is starting as the result of a migration then use the suggested clip and start phase
			CTaskNMControl* pControlTask = GetControlTask();
			if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0 && 
				m_suggestedClipSetId != CLIP_SET_ID_INVALID && m_suggestedClipId != CLIP_SET_ID_INVALID)
			{
				clipSetId = m_suggestedClipSetId;
				clipId = m_suggestedClipId;
				fStartPhase = m_suggestedClipPhase;
			}
			else
			{
				CTaskFallOver::eFallDirection dir = CTaskFallOver::kDirFront;
				CTaskFallOver::eFallContext context = CTaskFallOver::kContextShotPistol;
				CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
			}

			pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 2.0f, fStartPhase);
			static_cast<CTaskFallOver*>(pNewTask)->SetRunningLocally(true);
		}
	}
	else
	{
		fwFlags32 fallFlags = FF_DisableNMFall;

		//Don't let TaskFall create parachute task if there's one created already
		if (pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PARACHUTE))
		{
			fallFlags.SetFlag(FF_DisableParachuteTask);
		}

		pNewTask = rage_new CTaskFall(fallFlags);
		static_cast<CTaskFall*>(pNewTask)->SetRunningLocally(true);
	}

	if (nmVerifyf(pNewTask, "CTaskNMHighFall::StartAnimatedFallback: Couldn't create an animated fallback task"))
	{
		SetNewTask(pNewTask);
	}

	// Pretend that the balancer failed at this point so that clones will know that the owner has 'fallen'
	if (!pPed->IsNetworkClone())
	{
		ARTFeedbackInterfaceGta feedbackInterface;
		feedbackInterface.SetParentInst(pPed->GetRagdollInst());
		strncpy(feedbackInterface.m_behaviourName, NMSTR_PARAM(NM_BALANCE_FB), ART_FEEDBACK_BHNAME_LENGTH);
		feedbackInterface.onBehaviourFailure();
	}
}

bool CTaskNMHighFall::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	// disable ped capsule control so the blender can set the velocity directly on the ped
	if (pPed->IsNetworkClone())
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
		if (!pPed->GetIsDeadOrDying())
		{
			if (pPed->ShouldBeDead())
			{
				CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
				pPed->GetPedIntelligence()->AddEvent(deathEvent);
			}
			else
			{
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			}
		}
		return false;
	}

	return true;
}

const char* CTaskNMHighFall::ms_TypeToString[NUM_HIGHFALL_TYPES] =
{
	"HIGHFALL_IN_AIR",
	"HIGHFALL_VAULT",
	"HIGHFALL_FROM_CAR_HIT",
	"HIGHFALL_SLOPE_SLIDE",
	"HIGHFALL_TEETER_EDGE",
	"HIGHFALL_SPRINT_EXHAUSTED",
	"HIGHFALL_STUNT_JUMP"
};
