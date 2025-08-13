// FILE :    TaskReactToExplosion.h
// CREATED : 11-4-2011

// File header
#include "Network/NetworkInterface.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Subtasks/TaskReactToBuddyShot.h"
#include "Task/Combat/Subtasks/TaskShellShocked.h"
#include "Task/Combat/Subtasks/TaskVariedAimPose.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "task/Response/TaskFlee.h"
#include "task/Response/TaskReactAndFlee.h"
#include "Task/Response/TaskReactToExplosion.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToExplosion
////////////////////////////////////////////////////////////////////////////////

CTaskReactToExplosion::Tunables CTaskReactToExplosion::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskReactToExplosion, 0x761cd2c7);

CTaskReactToExplosion::CTaskReactToExplosion(Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags)
: m_vPosition(vPosition)
, m_pOwner(pOwner)
, m_fRadius(fRadius)
, m_nDurationOverrideForShellShocked(CTaskShellShocked::Duration_Invalid)
, m_uConfigFlags(uConfigFlags)
, m_uConfigFlagsForShellShocked(0)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_EXPLOSION);
}

CTaskReactToExplosion::~CTaskReactToExplosion()
{

}

#if !__FINAL
void CTaskReactToExplosion::Debug() const
{
	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskReactToExplosion::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_ShellShocked",
		"State_Flinch",
		"State_ThreatResponse",
		"State_Flee",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskReactToExplosion::IsTemporary(const CPed& rPed, CEntity* pOwner, fwFlags8 uConfigFlags)
{
	return (ChooseStateForThreatResponse(rPed, pOwner, uConfigFlags) == State_Finish);
}

bool CTaskReactToExplosion::IsValid(const CPed& rPed, Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags, const CAITarget& rAimTarget)
{
	//Ensure the ped is not crouching.
	if(rPed.GetIsCrouching())
	{
		return false;
	}
	//Ensure the ped is not a ragdoll.
	else if(rPed.GetUsingRagdoll())
	{
		return false;
	}

	//Check if the ped is in cover.
	CTaskInCover* pTaskInCover = static_cast<CTaskInCover *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if(pTaskInCover)
	{
		//Ensure the ped is aiming.
		if(pTaskInCover->GetState() != CTaskInCover::State_Aim)
		{
			return false;
		}
	}

	//Check if the ped is using a varied aim pose.
	CTaskVariedAimPose* pTaskVariedAimPose = static_cast<CTaskVariedAimPose *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VARIED_AIM_POSE));
	if(pTaskVariedAimPose)
	{
		//Ensure the ped's current pose is not crouching.
		if(pTaskVariedAimPose->IsCurrentPoseCrouching())
		{
			return false;
		}
	}

	//Ensure the ped is not diving to the ground.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DIVE_TO_GROUND, true))
	{
		return false;
	}
	//Ensure the ped is not getting up.
	else if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true))
	{
		return false;
	}

	return (ChooseState(rPed, vPosition, pOwner, fRadius, uConfigFlags, rAimTarget) != State_Finish);
}

aiTask* CTaskReactToExplosion::Copy() const
{
	CTaskReactToExplosion* pTask = rage_new CTaskReactToExplosion(m_vPosition, m_pOwner, m_fRadius, m_uConfigFlags);
	pTask->SetAimTarget(m_AimTarget);
	pTask->SetDurationOverrideForShellShocked(m_nDurationOverrideForShellShocked);
	pTask->SetConfigFlagsForShellShocked(m_uConfigFlagsForShellShocked);

	return pTask;
}

CTask::FSM_Return CTaskReactToExplosion::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToExplosion::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_ShellShocked)
			FSM_OnEnter
				ShellShocked_OnEnter();
			FSM_OnUpdate
				return ShellShocked_OnUpdate();

		FSM_State(State_Flinch)
			FSM_OnEnter
				Flinch_OnEnter();
			FSM_OnUpdate
				return Flinch_OnUpdate();

		FSM_State(State_ThreatResponse)
			FSM_OnEnter
				ThreatResponse_OnEnter();
			FSM_OnUpdate
				return ThreatResponse_OnUpdate();

		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo* CTaskReactToExplosion::CreateQueriableState() const
{
	return rage_new CClonedReactToExplosionInfo(GetState(), VEC3V_TO_VECTOR3(m_vPosition), m_pOwner, m_uConfigFlagsForShellShocked);
}

void CTaskReactToExplosion::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo && pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_REACT_TO_EXPLOSION );
	CClonedReactToExplosionInfo *reactToExplosionInfo = static_cast<CClonedReactToExplosionInfo*>(pTaskInfo);

	m_vPosition = VECTOR3_TO_VEC3V(reactToExplosionInfo->GetExplosionPosition());
	m_pOwner = reactToExplosionInfo->GetExplosionOwner();
	m_uConfigFlagsForShellShocked = reactToExplosionInfo->GetConfigFlagsForShellShocked();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return	CTaskReactToExplosion::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

void CTaskReactToExplosion::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTask::FSM_Return CTaskReactToExplosion::Start_OnUpdate()
{
	//Choose the state.
	if (GetPed()->IsNetworkClone())
	{
		SetState(GetStateFromNetwork());
	}
	else
	{
		SetState(ChooseState(*GetPed(), m_vPosition, m_pOwner, m_fRadius, m_uConfigFlags, m_AimTarget));
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToExplosion::Resume_OnUpdate()
{
	//Set the state to resume to.
	bool bKeepSubTask = false;
	int iStateToResumeTo = ChooseStateToResumeTo(bKeepSubTask);
	SetState(iStateToResumeTo);

	//Check if we should keep the sub-task.
	if(bKeepSubTask)
	{
		//Keep the sub-task across the transition.
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

void CTaskReactToExplosion::ShellShocked_OnEnter()
{
	//Choose the duration.
	CTaskShellShocked::Duration nDuration = m_nDurationOverrideForShellShocked;
	if(nDuration == CTaskShellShocked::Duration_Invalid)
	{
		fwRandom::SetRandomSeed(GetPed()->GetRandomSeed());
		nDuration = fwRandom::GetRandomTrueFalse() ? CTaskShellShocked::Duration_Short : CTaskShellShocked::Duration_Long;
	}
	
	//Create the task.
	CTask* pTask = rage_new CTaskShellShocked(m_vPosition, nDuration, m_uConfigFlagsForShellShocked);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToExplosion::ShellShocked_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsSubtaskFinished(CTaskTypes::TASK_SHELL_SHOCKED))
	{
		//Set the state for threat response.
		SetStateForThreatResponse();
	}

	return FSM_Continue;
}

void CTaskReactToExplosion::Flinch_OnEnter()
{
	//Create the task. Clones don't assert - some of this info won't sync correctly and can be different.
	if(!GetPed()->IsNetworkClone())
	{
		taskAssert(CTaskReactToBuddyShot::IsValid(*GetPed(), CAITarget(VEC3V_TO_VECTOR3(m_vPosition)), m_AimTarget));
	}

	CTask* pTask = rage_new CTaskReactToBuddyShot(CAITarget(VEC3V_TO_VECTOR3(m_vPosition)), m_AimTarget);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToExplosion::Flinch_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsSubtaskFinished(CTaskTypes::TASK_REACT_TO_BUDDY_SHOT))
	{
		//Set the state for threat response.
		SetStateForThreatResponse();
	}

	return FSM_Continue;
}

void CTaskReactToExplosion::ThreatResponse_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return;
	}

	taskAssert(CanUseThreatResponse(*GetPed(), m_pOwner));

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(GetOwnerPed());
	pTask->GetConfigFlags().SetFlag(CTaskThreatResponse::CF_ThreatIsIndirect);

	//Check if we can't flee.
	if(!CanFlee(*GetPed()))
	{
		//Force us to fight.
		pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
	}

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToExplosion::ThreatResponse_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskReactToExplosion::Flee_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_REACT_AND_FLEE))
	{
		return;
	}

	taskAssert(CanFlee(*GetPed()));

	//Get the target.
	CAITarget target;
	GetTargetToFleeFrom(target);
	taskAssert(target.GetIsValid());

	//Create the task.
	CTaskReactAndFlee* pTask = rage_new CTaskReactAndFlee(target, CTaskReactAndFlee::BA_Gunfire);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToExplosion::Flee_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
s32 CTaskReactToExplosion::ChooseStateForThreatResponse() const
{
	if (GetPed()->IsNetworkClone())
	{
		return GetStateFromNetwork();
	}

	return (ChooseStateForThreatResponse(*GetPed(), m_pOwner, m_uConfigFlags));
}

int CTaskReactToExplosion::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Keep the sub-task.
	bKeepSubTask = true;

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_THREAT_RESPONSE:
			{
				return State_ThreatResponse;
			}
			case CTaskTypes::TASK_REACT_AND_FLEE:
			{
				return State_Flee;
			}
			default:
			{
				break;
			}
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Finish;
}

CPed* CTaskReactToExplosion::GetOwnerPed() const
{
	return (GetOwnerPed(m_pOwner));
}

void CTaskReactToExplosion::GetTargetToFleeFrom(CAITarget& rTarget) const
{
	//Check if the owner ped is valid.
	CPed* pOwnerPed = GetOwnerPed();
	if(pOwnerPed)
	{
		rTarget.SetEntity(pOwnerPed);
	}
	else
	{
		rTarget.SetPosition(VEC3V_TO_VECTOR3(m_vPosition));
	}
}

void CTaskReactToExplosion::SetStateForThreatResponse()
{
	//Set the state.
	s32 iState = ChooseStateForThreatResponse();
	SetState(iState);
}

bool CTaskReactToExplosion::CanFlee(const CPed& rPed)
{
	//Ensure the ped is not law enforcement.
	if(rPed.IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the ped is not security.
	if(rPed.IsSecurityPed())
	{
		return false;
	}

	//Ensure the flag is set.
	if(!rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_RunFromFiresAndExplosions))
	{
		return false;
	}

	return true;
}

bool CTaskReactToExplosion::CanUseThreatResponse(const CPed& rPed, CEntity* pOwner)
{
	//Ensure the owner ped is valid.
	CPed* pOwnerPed = GetOwnerPed(pOwner);
	if(!pOwnerPed)
	{
		return false;
	}

	//Check if we shouldn't flee.
	if(!CanFlee(rPed))
	{
		//Ensure we would have made a decision to fight (we will force this in threat response when we can't flee).
		CTaskThreatResponse::ThreatResponse nThreatResponse = CTaskThreatResponse::DetermineThreatResponseState(rPed, pOwnerPed);
		if(nThreatResponse != CTaskThreatResponse::TR_Fight)
		{
			return false;
		}
	}

	return true;
}

s32 CTaskReactToExplosion::ChooseState(const CPed& rPed, Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags, const CAITarget& rAimTarget)
{
	//Generate the distance between the ped and the explosion.
	ScalarV scDist = Dist(rPed.GetTransform().GetPosition(), vPosition);
	scDist = Subtract(scDist, ScalarVFromF32(fRadius));

	//Check if we can be shell-shocked.
	if(CTaskShellShocked::IsValid(rPed, vPosition))
	{
		//Check if we are inside the shell-shocked distance.
		ScalarV scMaxShellShockedDist = ScalarVFromF32(sm_Tunables.m_MaxShellShockedDistance);
		if(IsLessThanAll(scDist, scMaxShellShockedDist))
		{
			return State_ShellShocked;
		}
	}

	//Check if we can flinch.
	if(CTaskReactToBuddyShot::IsValid(rPed, CAITarget(VEC3V_TO_VECTOR3(vPosition)), rAimTarget))
	{
		//Check if we are inside the flinch distance.
		ScalarV scMaxFlinchDist = ScalarVFromF32(sm_Tunables.m_MaxFlinchDistance);
		if(IsLessThanAll(scDist, scMaxFlinchDist))
		{
			return State_Flinch;
		}
	}

	//Choose the state for threat response.
	s32 iState = ChooseStateForThreatResponse(rPed, pOwner, uConfigFlags);

	return iState;
}

s32 CTaskReactToExplosion::ChooseStateForThreatResponse(const CPed& rPed, CEntity* pOwner, fwFlags8 uConfigFlags)
{
	//Ensure the flag is not set.
	if(uConfigFlags.IsFlagSet(CF_DisableThreatResponse))
	{
		return State_Finish;
	}

	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return State_Finish;
	}

	//Check if we can use threat response.
	if(CanUseThreatResponse(rPed, pOwner))
	{
		return State_ThreatResponse;
	}
	//Check if we can flee.
	else if(CanFlee(rPed))
	{
		return State_Flee;
	}

	return State_Finish;
}

CPed* CTaskReactToExplosion::GetOwnerPed(CEntity* pOwner)
{
	//Ensure the owner is valid.
	if(!pOwner)
	{
		return NULL;
	}

	//Check if the owner is a ped.
	if(pOwner->GetIsTypePed())
	{
		return static_cast<CPed *>(pOwner);
	}
	//Check if the owner is a vehicle.
	else if(pOwner->GetIsTypeVehicle())
	{
		return static_cast<CVehicle *>(pOwner)->GetDriver();
	}

	return NULL;
}
