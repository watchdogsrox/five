// Filename   :	TaskNMRelax.h
// Description:	Natural Motion relax class (FSM version)

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
#include "Task\Movement\Jumping\TaskFallGetUp.h"
#include "Task\Movement\Jumping\TaskInAir.h"

#include "Task/Physics/TaskNMRelax.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMRelaxInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMRelaxInfo::CClonedNMRelaxInfo(float fStiffness, float fDamping, bool bHoldPose)
: m_fStiffness(fStiffness)
, m_fDamping(fDamping)
, m_bHasStiffness(fStiffness > 0.0f)
, m_bHasDamping(fDamping > 0.0f)
, m_bHoldPose(bHoldPose)
{
}

CClonedNMRelaxInfo::CClonedNMRelaxInfo()
: m_fStiffness(-1.0f)
, m_fDamping(-1.0f)
, m_bHasStiffness(false)
, m_bHasDamping(false)
, m_bHoldPose(false)
{
}

CTaskFSMClone *CClonedNMRelaxInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMRelax(10000, 10000, m_fStiffness, m_fDamping, m_bHoldPose);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMRelax
//////////////////////////////////////////////////////////////////////////

CTaskNMRelax::CTaskNMRelax(u32 nMinTime, u32 nMaxTime, float fStiffness, float fDamping, bool bHoldPose)
:	CTaskNMBehaviour(nMinTime, nMaxTime),
m_fStiffness(fStiffness), m_fDamping(fDamping), m_bHoldPose(bHoldPose)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_RELAX);
}

CTaskNMRelax::~CTaskNMRelax()
{
}

dev_float DEFAULT_RELAXATION = 80.0f;
//
void CTaskNMRelax::SendRelaxMessage(CPed* pPed, bool bStart)
{
	// Fix for peds getting stuck in "fatally injured" state with zero health.
	// GS - 26/07/2012 Had to remove this as it was stopping the dead ped scanner from
	// kicking off the dead task, which caused an unmanaged ragdoll.
	//if(pPed->ShouldBeDead() && pPed->GetDeathState()==DeathState_Alive)
	//{
	//	pPed->SetDeathState(DeathState_Dead);
	//}

	ART::MessageParams msg;

	if(bStart)
		msg.addBool(NMSTR_PARAM(NM_START), true);

	msg.addBool(NMSTR_PARAM(NM_RELAX_HOLD_POSE), m_bHoldPose);

	float fRelaxation = (m_fStiffness >= 0.0f ? rage::Max(10.0f, 100.0f - m_fStiffness) : DEFAULT_RELAXATION);
	msg.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), fRelaxation);

	msg.addInt("priority", 100);

	if(m_fDamping >= 0.0f && m_fDamping <= 1.0f)
		msg.addFloat(NMSTR_PARAM(NM_RELAX_DAMPING), m_fDamping);

	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msg);
}


void CTaskNMRelax::StartBehaviour(CPed* pPed)
{
	if (pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetCurrentPhysicsLOD()==fragInstNM::RAGDOLL_LOD_HIGH)
	{
		// Set a minimum stiffness on the ankles to increase stability
		static float ankleStiffness = 0.8f;
		((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_ANKLE_LEFT_JOINT).SetMinStiffness(ankleStiffness);
		((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_ANKLE_RIGHT_JOINT).SetMinStiffness(ankleStiffness);
		((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_WRIST_LEFT_JOINT).SetMinStiffness(ankleStiffness);
		((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_WRIST_RIGHT_JOINT).SetMinStiffness(ankleStiffness);
	}

	SendRelaxMessage(pPed, true);
}

void CTaskNMRelax::ControlBehaviour(CPed* UNUSED_PARAM(pPed))
{
}

bool CTaskNMRelax::FinishConditions(CPed* pPed)
{
	return ProcessFinishConditionsBase(pPed, MONITOR_RELAX, FLAG_VEL_CHECK|FLAG_SKIP_DEAD_CHECK);
}

void CTaskNMRelax::StartAnimatedFallback(CPed* pPed)
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

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f));
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

		CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 1.0f, fStartPhase);
		pNewTask->SetRunningLocally(true);

		SetNewTask(pNewTask);
	}

	// Pretend that the balancer failed at this point (if it hasn't already been set) so that clones will know that the owner has 'fallen'
	CTaskNMControl* pTaskNMControl = GetControlTask();
	if (!pPed->IsNetworkClone() && (pTaskNMControl == NULL || !pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
	{
		ARTFeedbackInterfaceGta feedbackInterface;
		feedbackInterface.SetParentInst(pPed->GetRagdollInst());
		strncpy(feedbackInterface.m_behaviourName, NMSTR_PARAM(NM_BALANCE_FB), ART_FEEDBACK_BHNAME_LENGTH);
		feedbackInterface.onBehaviourFailure();
	}
}

bool CTaskNMRelax::ControlAnimatedFallback(CPed* pPed)
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