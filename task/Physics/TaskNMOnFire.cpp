// Filename   :	TaskNMOnFire.cpp
// Description:	Natural Motion on fire class (FSM version)

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

#include "Animation\FacialData.h"
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
#include "Task\Combat\TaskWrithe.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "task/Physics/TaskNMRelax.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMOnFireInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMOnFireInfo::CClonedNMOnFireInfo(float fLeanAmount, const Vector3* pVecLeanTarget, u32 onFireType)
: m_vecLeanTarget(Vector3(0.0f, 0.0f, 0.0f))
, m_fLeanAmount(fLeanAmount)
, m_onFireType(onFireType)
, m_bHasLeanTarget(false)
{
	if (pVecLeanTarget)
	{
		m_vecLeanTarget = *pVecLeanTarget;
		m_bHasLeanTarget = true;
	}
}

CClonedNMOnFireInfo::CClonedNMOnFireInfo()
: m_vecLeanTarget(Vector3(0.0f, 0.0f, 0.0f))
, m_fLeanAmount(0.0f)
, m_onFireType(0)
, m_bHasLeanTarget(false)
{

}

CTaskFSMClone *CClonedNMOnFireInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMOnFire(10000, 10000, m_fLeanAmount, m_bHasLeanTarget ? &m_vecLeanTarget : NULL, static_cast<CTaskNMOnFire::eOnFireType>(m_onFireType));
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMOnFire
//////////////////////////////////////////////////////////////////////////

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMOnFire::Tunables CTaskNMOnFire::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMOnFire, 0x87215326);
//////////////////////////////////////////////////////////////////////////

const char* CTaskNMOnFire::ms_TypeToString[NUM_ONFIRE_TYPES] = 
{
	"DEFAULT", "WEAK"
};


CTaskNMOnFire::CTaskNMOnFire(u32 nMinTime, u32 nMaxTime, float UNUSED_PARAM(fLeanAmount), const Vector3* UNUSED_PARAM(pVecLeanTarget), eOnFireType eType)
:	CTaskNMBehaviour(nMinTime, nMaxTime),
m_bBalanceFailed(false),
m_eType(eType),
m_pWritheClipSetRequestHelper(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_ONFIRE);
}

CTaskNMOnFire::~CTaskNMOnFire()
{
}

void CTaskNMOnFire::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		m_bBalanceFailed = true;
		sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
	} 
}
void CTaskNMOnFire::StartBehaviour(CPed* UNUSED_PARAM(pPed))
{
	AddTuningSet(&sm_Tunables.m_Start);

	if (m_eType == ONFIRE_WEAK)
		AddTuningSet(&sm_Tunables.m_Weak);

	if (GetControlTask())
		GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);

}
void CTaskNMOnFire::ControlBehaviour(CPed* pPed)
{
	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(pFacialData)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}

	CTaskNMBehaviour::ControlBehaviour(pPed);

	AddTuningSet(&sm_Tunables.m_Update);

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());
}
bool CTaskNMOnFire::FinishConditions(CPed* pPed)
{
	CFire* pFire = NULL;
	if (g_fireMan.IsEntityOnFire(pPed))
	{
		pFire = g_fireMan.GetEntityFire(pPed);
	}

	// check if dead or fire has gone out
	if(pPed->IsDead() || (pFire==NULL && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime))
	{
		if(pPed->IsDead() || m_bBalanceFailed)
		{
			if (pPed->IsLocalPlayer() && !pPed->ShouldBeDead() && GetControlTask())
			{
				 // force a shorter relax in this case
				GetControlTask()->ForceNewSubTask(rage_new CTaskNMRelax(300, 10000));
			}
			else
			{
				m_nSuggestedNextTask = CTaskTypes::TASK_NM_RELAX;
			}
		}
		else
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		return true;
	}

	//// check velocity
	//Matrix34 matRoot;
	//pPed->GetBoneMatrix(matRoot, BONETAG_ROOT);
	//Vector3 vecRootSpeed = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 0);

	//float fVelThreshhold = CTaskNMBehaviour::sm_Tunables.m_StandardBlendOutThresholds.PickBlendOut(pPed).m_MaxLinearVelocity;

	//if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime 
	//	&& vecRootSpeed.Mag2() < fVelThreshhold*fVelThreshhold
	//	&& !pPed->m_nPhysicalFlags.bWasInWater)
	//{
	//	if(!m_bBalanceFailed)
	//	{
	//		m_bBalanceFailed = true;
	//		sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
	//	}
	//	else
	//	{
	//		static float HEALTH_THRESHOLD = 20.0f;

	//		if((pFire == NULL) || (pPed->GetHealth() < HEALTH_THRESHOLD)) {
	//			m_nSuggestedNextTask = CTaskTypes::TASK_NM_RELAX;
	//			return true;
	//		}
	//	}
	//}

	// check end timer 
	if(m_bBalanceFailed)
	{
		if(m_nMaxTime > 0 && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMaxTime)
		{
			m_nSuggestedNextTask = CTaskTypes::TASK_NM_RELAX;
			return true;
		}
	}
	else
	{
		// reserve 20% of the time for writhing on the ground
		if(m_nMaxTime > 0 && fwTimer::GetTimeInMilliseconds() > m_nStartTime + (m_nMaxTime - m_nMaxTime / 5))
		{
			m_bBalanceFailed = true;
			sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
		}
	}

	return false;
}

void CTaskNMOnFire::CreateAnimatedFallbackTask(CPed* pPed)
{
	bool bFromGetUp = false;

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);

	CTaskNMControl* pControlTask = GetControlTask();

	// If the ped is already prone or if this task is starting as the result of a migration then start the writhe from the ground
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf() || (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0))
	{
		bFromGetUp = true;
	}
	
	fwMvClipSetId loopClipSetId = CTaskWrithe::GetIdleClipSet(pPed);
	s32 loopClipHash;
	CTaskWrithe::GetRandClipFromClipSet(loopClipSetId, loopClipHash);
	s32 startClipHash;
	CTaskWrithe::GetRandClipFromClipSet(CTaskWrithe::m_ClipSetIdToWrithe, startClipHash);
	CClonedWritheInfo::InitParams writheInfoInitParams(CTaskWrithe::State_Loop, NULL, bFromGetUp, false, false, false, 2.0f, NETWORK_INVALID_OBJECT_ID, 0, 
		loopClipSetId, loopClipHash, startClipHash, false);
	CClonedWritheInfo writheInfo(writheInfoInitParams);
	CTaskFSMClone* pNewTask = writheInfo.CreateCloneFSMTask();
	pNewTask->ReadQueriableState(&writheInfo);
	pNewTask->SetRunningLocally(true);

	SetNewTask(pNewTask);
}

bool CTaskNMOnFire::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	if (m_pWritheClipSetRequestHelper == NULL)
	{
		m_pWritheClipSetRequestHelper = CWritheClipSetRequestHelperPool::GetNextFreeHelper();
	}

	int iOldHurtEndTime = pPed->GetHurtEndTime();
	if (iOldHurtEndTime <= 0)
	{
		pPed->SetHurtEndTime(1);
	}

	bool bStreamed = false;
	if (m_pWritheClipSetRequestHelper != NULL)
	{
		bStreamed = CTaskWrithe::StreamReqResourcesIn(m_pWritheClipSetRequestHelper, pPed, CTaskWrithe::NEXT_STATE_COLLAPSE | CTaskWrithe::NEXT_STATE_WRITHE | CTaskWrithe::NEXT_STATE_DIE);
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
		if (!pPed->GetIsDeadOrDying())
		{
			if (pPed->ShouldBeDead())
			{
				CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
				fwMvClipSetId clipSetId;
				fwMvClipId clipId;
				CTaskWrithe::GetDieClipSetAndRandClip(clipSetId, clipId);
				deathEvent.SetClipSet(clipSetId, clipId);
				pPed->GetPedIntelligence()->AddEvent(deathEvent);
			}
			else
			{
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			}
		}
		return false;
	}
	else if (bStreamed && GetSubTask() == NULL)
	{
		CreateAnimatedFallbackTask(pPed);
	}

	pPed->SetHurtEndTime(iOldHurtEndTime);

	if (GetNewTask() != NULL || GetSubTask() != NULL)
	{
		// disable ped capsule control so the blender can set the velocity directly on the ped
		if (pPed->IsNetworkClone())
		{
			NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
		}
		pPed->OrientBoundToSpine();
	}

	return true;
}
