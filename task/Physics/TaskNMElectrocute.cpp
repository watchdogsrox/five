// Filename   :	TaskNMElectrocute.cpp

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "fragmentnm\messageparams.h"

// Framework headers

// Game headers
#include "Event/EventDamage.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/System/Task.h"
#include "Task/Movement/TaskAnimatedFallback.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMExplosionInfo
//////////////////////////////////////////////////////////////////////////

CTaskFSMClone *CClonedNMElectrocuteInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMElectrocute(10000, 10000);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMElectrocute ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMElectrocute::Tunables CTaskNMElectrocute::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMElectrocute, 0xb7da733c);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMElectrocute::CTaskNMElectrocute(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 : CTaskNMBehaviour(nMinTime, nMaxTime)
 , m_ElectrocuteFinished(false)
 , m_ContactPositionSpecified(false)
 , m_ApplyInitialForce(false)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_ELECTROCUTE);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMElectrocute::CTaskNMElectrocute(u32 nMinTime, u32 nMaxTime, const Vector3& contactPosition)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
, m_ElectrocuteFinished(false)
, m_ContactPositionSpecified(true)
, m_ContactPosition(contactPosition)
, m_ApplyInitialForce(false)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_ELECTROCUTE);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMElectrocute::~CTaskNMElectrocute()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMElectrocute::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssertf(pPed, "[CTaskNMElectrocute::StartBehaviour()] pPed == NULL");

	AddTuningSet(&sm_Tunables.m_Start);

	if(pPed->GetMotionData()->GetIsWalking())
	{
		AddTuningSet(&sm_Tunables.m_Walking);
	}
	else if(pPed->GetMotionData()->GetIsRunning())
	{
		AddTuningSet(&sm_Tunables.m_Running);
	}
	else if(pPed->GetMotionData()->GetIsSprinting())
	{
		AddTuningSet(&sm_Tunables.m_Sprinting);
	}

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());

	if(pPed && pPed->GetPedAudioEntity())
		pPed->GetPedAudioEntity()->IsBeingElectrocuted(true);

	// We only ever want to apply the initial force if StartBehaviour gets called
	// StartBehaviour won't get called if the ped this task is running on has migrated across the network
	m_ApplyInitialForce = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMElectrocute::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssertf(pPed, "[CTaskNMElectrocute::ControlBehaviour()] pPed == NULL");


	if (m_ApplyInitialForce && m_ContactPositionSpecified && sm_Tunables.m_InitialForce.ShouldApply(m_nStartTime) && !m_ElectrocuteFinished)
	{
		// Calculate the normalized force vector
		Vec3V impulseVec = pPed->GetMatrix().d() - RC_VEC3V(m_ContactPosition);
		impulseVec = NormalizeSafe(impulseVec, Vec3V(V_ZERO));

		// Work out the ped velocity in the direction of the contact
		Vec3V pedVel = VECTOR3_TO_VEC3V(((CPhysical*)pPed)->GetVelocity());
		pedVel = impulseVec*Dot(pedVel, impulseVec);

		// Get impulse from tuning
		sm_Tunables.m_InitialForce.GetImpulseVec(impulseVec, pedVel, pPed);

		// send to nm
		pPed->ApplyImpulse(RCC_VECTOR3(impulseVec), sm_Tunables.m_InitialForceOffset, sm_Tunables.m_InitialForceComponent, true);
	}

	if (!m_ElectrocuteFinished)
	{
		if (fwTimer::GetTimeInMilliseconds()>(m_nStartTime+m_nMinTime))
		{
			sm_Tunables.m_OnElectrocuteFinished.Post(*pPed, this);
			m_ElectrocuteFinished = true;
		}
	}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMElectrocute::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	// Use success message from "catchFall" to switch states from FALLING-->ON_GROUND.
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_CATCHFALL_FB))
	{
		sm_Tunables.m_OnCatchFallSuccess.Post(*pPed, this);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMElectrocute::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	// Use success message from "catchFall" to switch states from FALLING-->ON_GROUND.
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMElectrocute::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int nFlags = 0;
	if(m_ElectrocuteFinished)
		nFlags |= FLAG_VEL_CHECK;

	if (m_ElectrocuteFinished && pPed->GetVelocity().z < -sm_Tunables.m_FallingSpeedForHighFall)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
	}

	bool finished = ProcessFinishConditionsBase(pPed, CTaskNMBehaviour::MONITOR_FALL, nFlags);

	if(finished)
	{
		if(pPed && pPed->GetPedAudioEntity())
			pPed->GetPedAudioEntity()->IsBeingElectrocuted(false);
	}

	return finished;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMElectrocute::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMElectrocuteInfo(); 
}

void CTaskNMElectrocute::CreateAnimatedFallbackTask(CPed* pPed, bool bFalling)
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	CTaskNMControl* pControlTask = GetControlTask();

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

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f));
	}
	else
	{
		Vector3 vTempDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_ContactPosition;
		vTempDir.NormalizeSafe();
		float fHitDir = rage::Atan2f(-vTempDir.x, vTempDir.y);
		fHitDir -= pPed->GetTransform().GetHeading();
		fHitDir = fwAngle::LimitRadianAngle(fHitDir);
		fHitDir += QUARTER_PI;
		if(fHitDir < 0.0f)
		{
			fHitDir += TWO_PI;
		}
		int nHitDirn = int(fHitDir / HALF_PI);

		if (bFalling)
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
				switch(nHitDirn)
				{
				case CEntityBoundAI::FRONT:
					dir = CTaskFallOver::kDirFront;
					break;
				case CEntityBoundAI::LEFT:
					dir = CTaskFallOver::kDirLeft;
					break;
				case CEntityBoundAI::REAR:
					dir = CTaskFallOver::kDirBack;
					break;
				case CEntityBoundAI::RIGHT:
					dir = CTaskFallOver::kDirRight;
					break;
				}

				CTaskFallOver::eFallContext context = CTaskFallOver::ComputeFallContext(WEAPONTYPE_STUNGUN, BONETAG_ROOT);
				CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
			}

			CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 2.0f, fStartPhase);
			pNewTask->SetRunningLocally(true);

			SetNewTask(pNewTask);
		}
		// If this task is starting as the result of a migration then bypass the hit reaction
		else if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) == 0)
		{
			switch(nHitDirn)
			{
			case CEntityBoundAI::FRONT:
				clipId = CLIP_DAM_FRONT;
				break;
			case CEntityBoundAI::LEFT:
				clipId = CLIP_DAM_LEFT;
				break;
			case CEntityBoundAI::REAR:
				clipId = CLIP_DAM_BACK;
				break;
			case CEntityBoundAI::RIGHT:
				clipId = CLIP_DAM_RIGHT;
				break;
			default:
				clipId = CLIP_DAM_FRONT;
			}

			clipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();

			pPed->GetMovePed().GetAdditiveHelper().BlendInClipBySetAndClip(pPed, clipSetId, clipId);
		}
	}
}

void CTaskNMElectrocute::StartAnimatedFallback(CPed* pPed)
{
	CreateAnimatedFallbackTask(pPed, false);
}

bool CTaskNMElectrocute::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
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
	else if (!GetSubTask())
	{
		// If this is running on a clone ped then wait until the owner ped has fallen over before starting the animated fall-back task
		CTaskNMControl* pTaskNMControl = GetControlTask();
		if (pTaskNMControl == NULL || !pPed->IsNetworkClone() || pPed->IsDead() ||
			((pTaskNMControl->GetFlags() & CTaskNMControl::FORCE_FALL_OVER) != 0 || pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
		{
			// Pretend that the balancer failed at this point (if it hasn't already been set) so that clones will know that the owner has 'fallen'
			if (!pPed->IsNetworkClone() && (pTaskNMControl == NULL || !pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
			{
				ARTFeedbackInterfaceGta feedbackInterface;
				feedbackInterface.SetParentInst(pPed->GetRagdollInst());
				strncpy(feedbackInterface.m_behaviourName, NMSTR_PARAM(NM_BALANCE_FB), ART_FEEDBACK_BHNAME_LENGTH);
				feedbackInterface.onBehaviourFailure();
			}

			CreateAnimatedFallbackTask(pPed, true);
		}
		// We won't have started playing a full body animation - so ensure the ped continues playing their idling animations
		else if ((pTaskNMControl->GetFlags() & CTaskNMControl::ON_MOTION_TASK_TREE) == 0)
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		}
	}

	if (GetNewTask() != NULL || GetSubTask() != NULL)
	{
		// disable ped capsule control so the blender can set the velocity directly on the ped
		if (pPed->IsNetworkClone())
		{
			NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
		}
	}

	return true;
}
