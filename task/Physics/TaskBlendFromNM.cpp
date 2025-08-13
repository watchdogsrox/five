// Filename   :	TaskBlendFromNM.cpp
// Description:	Natural Motion blend from natural motion class (FSM version)
//
// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers

#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "math/angmath.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "Animation/moveped.h"
#include "Animation\debug\AnimViewer.h"
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "Peds/Ped.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "Physics\RagdollConstraints.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/default/taskplayer.h"
#include "Task\General\TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"

#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

#include "weapons/inventory/PedWeaponManager.h"
#include "weapons/Weapon.h"

AI_OPTIMISATIONS()

#define __USE_POINT_CLOUD 1

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskBlendFromNM::CTaskBlendFromNM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Init();
	SetInternalTaskType(CTaskTypes::TASK_BLEND_FROM_NM);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskBlendFromNM::CTaskBlendFromNM(eNmBlendOutSet forceBlendOutSet)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Init();
	m_forcedSet = forceBlendOutSet;
	SetInternalTaskType(CTaskTypes::TASK_BLEND_FROM_NM);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskBlendFromNM::~CTaskBlendFromNM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskBlendFromNM::Init()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_bHasAborted = false;
	m_bClearAllBehaviours = false;
	m_bRunningAsMotionTask = false;
	m_bResetDesiredHeadingOnSwitchToAnimated = true;
	m_bMaintainDamageEntity = false;

	m_pAttachToPhysical = NULL;
	m_vecAttachOffset.Zero();

	m_pBumpedByEntity = NULL;
	m_bAllowBumpReaction = false;

	m_forcedSet = NMBS_INVALID;
	m_forcedSetTarget.Clear();
	m_pForcedSetMoveTask = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskBlendFromNM::CleanUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_pForcedSetMoveTask)
	{
		delete m_pForcedSetMoveTask;
		m_pForcedSetMoveTask = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskBlendFromNM::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	bool bAllowAbort = false;

	// always abort here if the event requires abort for ragdoll
	if (pEvent && ( ((const CEvent*)pEvent)->RequiresAbortForRagdoll() || ((const CEvent*)pEvent)->RequiresAbortForMelee(pPed) ) )
	{
		bAllowAbort = true;
	}

	int nEventType = pEvent ? ((CEvent*)pEvent)->GetEventType() : -1;
	if( nEventType==EVENT_NEW_TASK || ( nEventType==EVENT_DAMAGE && GetSubTask() && GetSubTask()->GetState()==CTaskGetUp::State_PlayReactionClip ) )
		bAllowAbort = true;

	if( !bAllowAbort )
	{
		const aiTask* pTaskResponse = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();
		if(pTaskResponse==NULL)
			pTaskResponse = pPed->GetPedIntelligence()->GetEventResponseTask();

		if(pTaskResponse)
		{
			if(CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pTaskResponse))
				bAllowAbort = true;
		}
	}

	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL && (SafeCast(CTaskNMControl, GetParent())->GetFlags() & CTaskNMControl::CLONE_LOCAL_SWITCH))
	{
		nmTaskDebugf(this, "Aborting for clone to local switch");
		bAllowAbort = true;
	}

	if(bAllowAbort)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskBlendFromNM::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		// make sure we switch back to animation unless we're a clone which will handle the switch back when it's ready 
		if(pPed->GetUsingRagdoll() && !pPed->IsNetworkClone())
		{
			if (!m_bRunningAsMotionTask || !pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL))
			{
				nmTaskDebugf(this, "CTaskBlendFromNM::DoAbort switching to animated");
				pPed->SwitchToAnimated();
			}
		}
	}

	m_bHasAborted = true;

	CTask::DoAbort(iPriority, pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskBlendFromNM::ProcessPreFSM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed();

	// Maintain the damage entity during the blend-from-NM.  This will prevent the ped from re-entering NM during the blend and for 
	// CCollisionEventScanner::RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT ms afterwards
	if (m_bMaintainDamageEntity && pPed->GetRagdollInst() != NULL && pPed->GetRagdollInst()->GetLastImpactDamageEntity() != NULL && 
		pPed->GetRagdollInst()->GetTimeSinceImpactDamage() < CCollisionEventScanner::RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT)
	{
		// Note the new entity and damage taken from it
		pPed->GetRagdollInst()->SetImpactDamageEntity(pPed->GetRagdollInst()->GetLastImpactDamageEntity());
		pPed->GetRagdollInst()->SetAccumulatedImpactDamageFromLastEntity(1000.0f);
	}

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskBlendFromNM::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_DoingBlendOut)
			FSM_OnEnter
				DoingBlendOut_OnEnter(pPed);
			FSM_OnUpdate
				return DoingBlendOut_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskBlendFromNM::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	SetState(State_DoingBlendOut);

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskBlendFromNM::DoingBlendOut_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// start the get up task
	CTaskGetUp* pTask = rage_new CTaskGetUp(m_forcedSet);

	if (m_forcedSet!=NMBS_INVALID)
	{
		if (m_forcedSetTarget.GetIsValid())
		{
			pTask->SetTarget(m_forcedSetTarget);
		}

		if (m_pForcedSetMoveTask)
		{
			pTask->SetMoveTask((CTask*)m_pForcedSetMoveTask->Copy());
		}
	}

	pTask->SetAllowBumpedReaction(m_bAllowBumpReaction);
	pTask->SetBumpedByEntity(m_pBumpedByEntity);
	pTask->SetRunningAsMotionTask(m_bRunningAsMotionTask);
	pTask->SetLocallyCreated();

	SetNewTask(pTask);

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskBlendFromNM::DoingBlendOut_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Don't attempt to get up if the ped is should die.
	if(pPed->GetIsDeadOrDying())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Let the ped know that a valid task is in control of the ragdoll.
	if (pPed->GetUsingRagdoll())
		pPed->TickRagdollStateFromTask(*this);

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskBlendFromNM::Finish_OnUpdate()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return FSM_Quit;
}


void CTaskBlendFromNM::DoDirectBlendOut()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	// do a quick blend out to the appropriate a.i. behaviour
	// Tell the a.i. system that we're doing a direct blend from nm
	pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
}
