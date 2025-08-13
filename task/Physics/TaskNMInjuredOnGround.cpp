// Filename   :	TaskNMInjuredOnGround.cpp
// Description:	GTA-side of the NM InjuredOnGround behavior

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated\articulatedbody.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "event/Events.h"
#include "Peds/ped.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/NmDebug.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

CTaskNMInjuredOnGround::Tunables CTaskNMInjuredOnGround::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMInjuredOnGround, 0xcd8c88f5);

u16 CTaskNMInjuredOnGround::ms_NumInjuredOnGroundAgents = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMInjuredOnGround //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMInjuredOnGround::CTaskNMInjuredOnGround(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
{
	ms_NumInjuredOnGroundAgents++;

#if !__FINAL
	m_strTaskName = "NMInjuredOnGround";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_INJURED_ON_GROUND);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMInjuredOnGround::~CTaskNMInjuredOnGround()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ms_NumInjuredOnGroundAgents--;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMInjuredOnGround::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//Allow dragged tasks to interrupt this task.
	const CEventGivePedTask* pGiveEvent = (pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_GIVE_PED_TASK) ? static_cast<const CEventGivePedTask *>(pEvent) : NULL;
	if(pGiveEvent && pGiveEvent->GetTask() && (pGiveEvent->GetTask()->GetTaskType() == CTaskTypes::TASK_DRAGGED_TO_SAFETY))
	{
		return true;
	}
	
	return CTaskNMBehaviour::ShouldAbort(iPriority, pEvent);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMInjuredOnGround::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Enable looseness
	if (pPed->GetRagdollInst()->GetCached())
	{
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->SetBodyMinimumStiffness(0.0f);
		static float ankleStiffness = 0.8f;
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->GetJoint(RAGDOLL_ANKLE_LEFT_JOINT).SetMinStiffness(ankleStiffness);
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->GetJoint(RAGDOLL_ANKLE_RIGHT_JOINT).SetMinStiffness(ankleStiffness);
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->GetJoint(RAGDOLL_WRIST_LEFT_JOINT).SetMinStiffness(ankleStiffness);
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->GetJoint(RAGDOLL_WRIST_RIGHT_JOINT).SetMinStiffness(ankleStiffness);
	}

	// Send Initial Messages
	HandleInjuredOnGround(pPed);

	ControlBehaviour(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMInjuredOnGround::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::ControlBehaviour(pPed);

	// Update the attacker position
	if (pPed->GetAttacker())
	{
		Vector3 attackerPos = VEC3V_TO_VECTOR3(pPed->GetAttacker()->GetTransform().GetPosition());
		ART::MessageParams msg;
		msg.addVector3(NMSTR_PARAM(NM_INJURED_ON_GROUND_ATTACKER_POS), attackerPos.x, attackerPos.y, attackerPos.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_INJURED_ON_GROUND_MSG), &msg);
	}

	// Don't let the ped fall asleep during this performance
	pPed->GetCollider()->GetSleep()->Reset();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMInjuredOnGround::HandleInjuredOnGround(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bool dontReachWithLeft = false;
	bool dontReachWithRight = false;

	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), true);
	int numInjuries = 0;
	if (pPed->GetPrimaryWoundData()->valid)
	{
		numInjuries++;
		msg.addInt(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_1_COMPONENT), pPed->GetPrimaryWoundData()->component);
		msg.addVector3(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_1_LOCAL_POS), pPed->GetPrimaryWoundData()->localHitLocation.x, pPed->GetPrimaryWoundData()->localHitLocation.y, pPed->GetPrimaryWoundData()->localHitLocation.z);
		msg.addVector3(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_1_LOCAL_NORM), pPed->GetPrimaryWoundData()->localHitNormal.x, pPed->GetPrimaryWoundData()->localHitNormal.y, pPed->GetPrimaryWoundData()->localHitNormal.z);
	
		// Check whether the wound is in an awkward location to reach with each hand
		if (pPed->GetPrimaryWoundData()->component == RAGDOLL_CLAVICLE_LEFT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_UPPER_ARM_LEFT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_LOWER_ARM_LEFT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_HAND_LEFT)
		{
			dontReachWithLeft = true;
		}
		if (pPed->GetPrimaryWoundData()->component == RAGDOLL_CLAVICLE_RIGHT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_UPPER_ARM_RIGHT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_LOWER_ARM_RIGHT ||
			pPed->GetPrimaryWoundData()->component == RAGDOLL_HAND_RIGHT)
		{
			dontReachWithRight = true;
		}
	}
	if (pPed->GetSecondaryWoundData()->valid)
	{
		Assert(pPed->GetPrimaryWoundData()->valid);
		numInjuries++;
		msg.addInt(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_2_COMPONENT), pPed->GetSecondaryWoundData()->component);
		msg.addVector3(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_2_LOCAL_POS), pPed->GetSecondaryWoundData()->localHitLocation.x, pPed->GetSecondaryWoundData()->localHitLocation.y, pPed->GetSecondaryWoundData()->localHitLocation.z);
		msg.addVector3(NMSTR_PARAM(NM_INJURED_ON_GROUND_INJURY_2_LOCAL_NORM), pPed->GetSecondaryWoundData()->localHitNormal.x, pPed->GetSecondaryWoundData()->localHitNormal.y, pPed->GetSecondaryWoundData()->localHitNormal.z);
	}
	msg.addInt(NMSTR_PARAM(NM_INJURED_ON_GROUND_NUM_INJURIES), numInjuries);
	msg.addBool(NMSTR_PARAM(NM_INJURED_ON_GROUND_DONT_REACH_WITH_LEFT), dontReachWithLeft);
	msg.addBool(NMSTR_PARAM(NM_INJURED_ON_GROUND_DONT_REACH_WITH_RIGHT), dontReachWithRight);
	msg.addBool(NMSTR_PARAM(NM_INJURED_ON_GROUND_STRONG_ROLL_FORCE), pPed->GetRagdollInst()->GetARTAssetID() == 0); // male

	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_INJURED_ON_GROUND_MSG), &msg);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMInjuredOnGround::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static float fallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < fallingSpeed)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;

		// Ensure that injured on ground stops
		ART::MessageParams msg;
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_RELAX, FLAG_SKIP_DEAD_CHECK | FLAG_QUIT_IN_WATER);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMInjuredOnGround::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMInjuredOnGroundInfo(); 
}


CTaskFSMClone *CClonedNMInjuredOnGroundInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMInjuredOnGround(4000, 30000);
}


