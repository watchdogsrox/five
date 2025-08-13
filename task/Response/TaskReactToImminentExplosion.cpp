// FILE :    TaskReactToImminentExplosion.h

// File header
#include "Task/Response/TaskReactToImminentExplosion.h"

// Game headers
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/Cover/TaskCover.h"
#include "task/Combat/Subtasks/TaskReactToBuddyShot.h"
#include "task/Combat/Subtasks/TaskVariedAimPose.h"
#include "task/General/TaskBasic.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Response/TaskDiveToGround.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToImminentExplosion
////////////////////////////////////////////////////////////////////////////////

u32 CTaskReactToImminentExplosion::s_uLastTimeEscapeUsedOnScreen = 0;

CTaskReactToImminentExplosion::Tunables CTaskReactToImminentExplosion::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskReactToImminentExplosion, 0xb3727566);

CTaskReactToImminentExplosion::CTaskReactToImminentExplosion(const CAITarget& rExplosionTarget, float fRadius, u32 uTimeOfExplosion)
: m_ExplosionTarget(rExplosionTarget)
, m_fRadius(fRadius)
, m_uTimeOfExplosion(uTimeOfExplosion)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_IMMINENT_EXPLOSION);
}

CTaskReactToImminentExplosion::~CTaskReactToImminentExplosion()
{

}

#if !__FINAL
void CTaskReactToImminentExplosion::Debug() const
{
	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskReactToImminentExplosion::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Flinch",
		"State_Escape",
		"State_Dive",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskReactToImminentExplosion::IsValid(const CPed& rPed, const CAITarget& rExplosionTarget, float fRadius, const CAITarget& rAimTarget, bool bIgnoreCover)
{
	//Ensure the explosion target is valid.
	if(!rExplosionTarget.GetIsValid())
	{
		return false;
	}

	//Ensure the ped is on foot.
	if(!rPed.GetIsOnFoot())
	{
		return false;
	}

	//Ensure the ped is not crouching.
	if(rPed.GetIsCrouching())
	{
		return false;
	}

	//Ensure the ped is not a ragdoll.
	if(rPed.GetUsingRagdoll())
	{
		return false;
	}

	//Check if we are not ignoring cover.
	if(!bIgnoreCover)
	{
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

	//Ensure the ped is not getting up.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true))
	{
		return false;
	}

	return (ChooseState(rPed, rExplosionTarget, fRadius, rAimTarget) != State_Finish);
}

bool CTaskReactToImminentExplosion::MustTargetBeValid() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Flinch:
		case State_Dive:
		{
			return false;
		}
		default:
		{
			break;
		}
	}

	return true;
}

void CTaskReactToImminentExplosion::ProcessTimeOfExplosion()
{
	//Check if the entity is valid.
	const CEntity* pEntity = m_ExplosionTarget.GetEntity();
	if(pEntity)
	{
		//Calculate the time until the explosion.
		float fTime;
		if(CExplosionHelper::CalculateTimeUntilExplosion(*pEntity, fTime))
		{
			m_uTimeOfExplosion = fwTimer::GetTimeInMilliseconds() + (u32)(fTime * 1000.0f);
		}
	}
}

bool CTaskReactToImminentExplosion::CanEscape(const CPed& rPed)
{
	//Ensure the ped can run from fires and explosions.
	if(!rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_RunFromFiresAndExplosions))
	{
		return false;
	}

	//Ensure the ped is not in an interior.
	if(rPed.GetIsInInterior())
	{
		return false;
	}

	//Ensure the ped is not on stairs.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs))
	{
		return false;
	}

	//Ensure we have not used escape recently.
	static dev_u32 s_uMinTime = 150;
	if(CTimeHelpers::GetTimeSince(s_uLastTimeEscapeUsedOnScreen) < s_uMinTime)
	{
		return false;
	}

	return true;
}

bool CTaskReactToImminentExplosion::CanFlinch(const CPed& rPed, const CAITarget& rExplosionTarget, const CAITarget& rAimTarget)
{
	//Ensure the task is valid.
	if(!CTaskReactToBuddyShot::IsValid(rPed, rExplosionTarget, rAimTarget))
	{
		return false;
	}

	return true;
}

s32 CTaskReactToImminentExplosion::ChooseState(const CPed& rPed, const CAITarget& rExplosionTarget, float fRadius, const CAITarget& rAimTarget)
{
	//Grab the target position.
	Vec3V vTargetPosition;
	rExplosionTarget.GetPosition(RC_VECTOR3(vTargetPosition));

	//Generate the distance between the ped and the imminent explosion.
	ScalarV scDist = Dist(rPed.GetTransform().GetPosition(), vTargetPosition);
	scDist = Subtract(scDist, ScalarVFromF32(fRadius));

	//Check if we can escape.
	if(CanEscape(rPed))
	{
		//Check if we are inside the max distance.
		ScalarV scMaxDiveDist = ScalarVFromF32(sm_Tunables.m_MaxEscapeDistance);
		if(IsLessThanAll(scDist, scMaxDiveDist))
		{
			return State_Escape;
		}
	}

	//Check if we can flinch.
	if(CanFlinch(rPed, rExplosionTarget, rAimTarget))
	{
		//Check if we are inside the max distance.
		ScalarV scMaxFlinchDist = ScalarVFromF32(sm_Tunables.m_MaxFlinchDistance);
		if(IsLessThanAll(scDist, scMaxFlinchDist))
		{
			return State_Flinch;
		}
	}

	return State_Finish;
}

aiTask* CTaskReactToImminentExplosion::Copy() const
{
	//Create the task.
	CTaskReactToImminentExplosion* pTask = rage_new CTaskReactToImminentExplosion(m_ExplosionTarget, m_fRadius, m_uTimeOfExplosion);
	pTask->SetAimTarget(m_AimTarget);

	return pTask;
}

CTask::FSM_Return CTaskReactToImminentExplosion::ProcessPreFSM()
{
	//Check if the target must be valid.
	if(MustTargetBeValid())
	{
		//Ensure the target is valid.
		if(!m_ExplosionTarget.GetIsValid())
		{
			return FSM_Quit;
		}
	}

	//Process the time of explosion.
	ProcessTimeOfExplosion();

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToImminentExplosion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Flinch)
			FSM_OnEnter
				Flinch_OnEnter();
			FSM_OnUpdate
				return Flinch_OnUpdate();

		FSM_State(State_Escape)
			FSM_OnEnter
				Escape_OnEnter();
			FSM_OnUpdate
				return Escape_OnUpdate();

		FSM_State(State_Dive)
			FSM_OnEnter
				Dive_OnEnter();
			FSM_OnUpdate
				return Dive_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskReactToImminentExplosion::Start_OnUpdate()
{
	//Set the state.
	SetState(ChooseState(*GetPed(), m_ExplosionTarget, m_fRadius, m_AimTarget));

	return FSM_Continue;
}

void CTaskReactToImminentExplosion::Flinch_OnEnter()
{
	//Create the task.
	taskAssert(CTaskReactToBuddyShot::IsValid(*GetPed(), m_ExplosionTarget, m_AimTarget));
	CTask* pTask = rage_new CTaskReactToBuddyShot(m_ExplosionTarget, m_AimTarget);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToImminentExplosion::Flinch_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskReactToImminentExplosion::Escape_OnEnter()
{
	//Check if the ped is on-screen.
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		//Set the time.
		s_uLastTimeEscapeUsedOnScreen = fwTimer::GetTimeInMilliseconds();
	}

	//Grab the target position.
	Vector3 vTargetPosition;
	m_ExplosionTarget.GetPosition(vTargetPosition);

	//Create the nav params.
	CNavParams params;
	params.m_vTarget = vTargetPosition;
	params.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	params.m_bFleeFromTarget = true;
	params.m_fFleeSafeDistance = m_fRadius + 3.0f;

	//Create the move task.
	CTaskMoveFollowNavMesh* pTaskMove = rage_new CTaskMoveFollowNavMesh(params);
	pTaskMove->SetQuitTaskIfRouteNotFound(true);

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pTaskMove);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToImminentExplosion::Escape_OnUpdate()
{
	//Check if the explosion is about to occur.
	static const u32 s_uTimeToDiveBeforeExplosion = 1000;
	if((fwTimer::GetTimeInMilliseconds() + s_uTimeToDiveBeforeExplosion) > m_uTimeOfExplosion)
	{
		//Move to the dive state.
		SetState(State_Dive);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskReactToImminentExplosion::Dive_OnEnter()
{
	//Grab the target position.
	Vec3V vTargetPosition;
	m_ExplosionTarget.GetPosition(RC_VECTOR3(vTargetPosition));

	//Calculate the direction.
	Vec3V vDirection = Subtract(GetPed()->GetTransform().GetPosition(), vTargetPosition);
	vDirection = NormalizeFastSafe(vDirection, GetPed()->GetTransform().GetForward());

	//Create the task.
	static fwMvClipSetId s_ClipSetId("explosions",0x87941A0F);
	CTaskDiveToGround* pTask = rage_new CTaskDiveToGround(vDirection, s_ClipSetId);
	pTask->SetBlendInDelta(SLOW_BLEND_IN_DELTA);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToImminentExplosion::Dive_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsSubtaskFinished(CTaskTypes::TASK_DIVE_TO_GROUND))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
