#include "TaskIdle.h"

#include "Peds/Ped.h"
#include "System/controlMgr.h"
#include "Task/General/TaskBasic.h"
#include "Task/Vehicle/TaskRideHorse.h"

AI_OPTIMISATIONS()

// CLASS : CTaskMoveStandStill
// PURPOSE : Movement task which causes ped to stand still for specified time


const int CTaskMoveStandStill::ms_iStandStillTime = 20000;

CTaskMoveStandStill::CTaskMoveStandStill(const int iDuration, const bool bForever)
: CTaskMove(MOVEBLENDRATIO_STILL),
m_iDuration(iDuration),
m_bForever(bForever)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_STAND_STILL);
}

CTaskMoveStandStill::~CTaskMoveStandStill()
{
}

CTask::FSM_Return CTaskMoveStandStill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(Running)
		FSM_OnUpdate
		return StateRunnning_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveStandStill::StateRunnning_OnUpdate(CPed * pPed)
{
	if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing ))
	{
		// Run the CTaskMoveMountedSpurControl subtask, if needed (riding), or set
		// the move blend ratio directly.
		if(!CTaskMoveBase::UpdateRideControlSubTasks(*this, MOVEBLENDRATIO_STILL))
		{
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
		}
	}

	if(pPed->IsLocalPlayer())
	{
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true);
		}
		else if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsLanding ))
		{
			pPed->GetPlayerInfo()->ControlButtonDuck();
		}
	}

	m_vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if(!m_timer.IsSet() && m_iDuration >= 0)
	{
		m_timer.Set(fwTimer::GetTimeInMilliseconds(), m_iDuration);	
	}

	if(m_bForever)
	{
		return FSM_Continue;
	}
	else
	{
		return m_timer.IsOutOfTime() ? FSM_Quit : FSM_Continue;
	}
}







// CLASS : CTaskMoveAchieveHeading
// PURPOSE : Instructs the ped to assume a given heading, task completes when ped has achieved this heading
// within a given threshold

/* static */
const float CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac	=	1.0f;
const float CTaskMoveAchieveHeading::ms_fHeadingTolerance		=	0.02f;
const atHashWithStringNotFinal CTaskMoveAchieveHeading::ms_HeadIkHashKey("TaskAchvHeading",0xEBABBF5A);

/* static */
bool CTaskMoveAchieveHeading::IsHeadingAchieved(const CPed* pPed, float fDesiredHeading, float fHeadingTolerance)
{
	taskAssert(pPed);
	float fDiff = rage::Abs(fDesiredHeading - pPed->GetCurrentHeading());
	fDiff = fwAngle::LimitRadianAngleSafe(fDiff);
	if( rage::Abs(fDiff) <= fHeadingTolerance )
	{
		return true;
	}
	return false;
}

CTaskMoveAchieveHeading::CTaskMoveAchieveHeading(const float fDesiredHeading, const float fHeadingChangeRateFrac, const float fHeadingTolerance, const float fTime)
: CTaskMove(MOVEBLENDRATIO_STILL),
  m_fTimeToTurn(fTime),
  m_bDoingIK(false)
{
    SetHeading(fDesiredHeading,fHeadingChangeRateFrac,fHeadingTolerance,true);

	SetInternalTaskType(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING);
}

CTaskMoveAchieveHeading::~CTaskMoveAchieveHeading()
{

}

#if !__FINAL
void
CTaskMoveAchieveHeading::Debug() const
{

}
#endif

void
CTaskMoveAchieveHeading::QuitIK(CPed * pPed)
{
    // quit IK
	if(m_bDoingIK && pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().AbortLookAt(250, ms_HeadIkHashKey.GetHash());
	}
}

void
CTaskMoveAchieveHeading::SetUpIK(CPed * pPed)
{
	// If we're the player, only do this if we're not in a cutscene
	if(pPed->IsLocalPlayer() && CControlMgr::IsDisabledControl(&CControlMgr::GetMainPlayerControl()))
	{
		return;
	}

	// only apply IK, if the ped is on-screen not already looking at an entity and not doing an avoid task -
	// which requires them to look at their desired destination and not their detour target
	if(!pPed->GetIsVisibleInSomeViewportThisFrame() || pPed->GetIkManager().GetLookAtEntity() )
		return;
	
	if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_COMPLEX_AVOID_ENTITY)
		return;

	Vector3 vDesiredHeading(-rage::Sinf(m_fDesiredHeading), rage::Cosf(m_fDesiredHeading), 0.0f);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 lookAtPos(vPedPos);
	lookAtPos += vDesiredHeading * 2.0f;
	lookAtPos.z += 0.61f;

	pPed->GetIkManager().LookAt(ms_HeadIkHashKey.GetHash(), NULL, 5000, BONETAG_INVALID, &lookAtPos);
	m_bDoingIK = true;
}

CTask::FSM_Return CTaskMoveAchieveHeading::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Running)
			FSM_OnUpdate
				return StateRunning_OnUpdate(pPed);
	FSM_End
}


CTaskInfo* CTaskMoveAchieveHeading::CreateQueriableState() const
{
	return rage_new CClonedAchieveHeadingInfo(m_fDesiredHeading, m_fTimeToTurn);
}

CTask::FSM_Return CTaskMoveAchieveHeading::StateRunning_OnUpdate(CPed* pPed)
{
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);

	// Keep a track of the peds position so we can return a valid value to GetTarget()
	m_vPedsPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return FSM_Quit;
	}
	
    // Multiply the heading change rate.
    pPed->RestoreHeadingChangeRate();
    pPed->SetHeadingChangeRate(pPed->GetHeadingChangeRate() * m_fHeadingChangeRateFrac);

    // Set the desired heading.
    pPed->SetDesiredHeading(m_fDesiredHeading);

    // Decide if the ped is close enough to the desired heading to finish the task.
    float fDiff = rage::Abs(m_fDesiredHeading-pPed->GetCurrentHeading());
    fDiff = fwAngle::LimitRadianAngleSafe(fDiff);

	// If we have a time to turn, quit after the time is up
	// If we don't then quit when we achieve the heading
    if((m_fTimeToTurn > 0.0f && GetTimeRunning() >= m_fTimeToTurn) || (IsClose(m_fTimeToTurn, 0.0f) && rage::Abs(fDiff) <= m_fHeadingTolerance))
    {
        pPed->SetDesiredHeading(pPed->GetCurrentHeading());
        pPed->RestoreHeadingChangeRate();

        // quit IK
		QuitIK(pPed);

        return FSM_Quit;
    }
    // Only do IK if angle is greater than threshold
    else
	{
		SetUpIK(pPed);
	}
    
    return FSM_Continue;
}

void CTaskMoveAchieveHeading::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// quit IK on abort
	QuitIK(pPed);

	pPed->SetDesiredHeading(pPed->GetCurrentHeading());
	pPed->RestoreHeadingChangeRate();

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}

CTask  *CClonedAchieveHeadingInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	CTask* pTask = rage_new CTaskMoveAchieveHeading(m_fDesiredHeading, CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac, CTaskMoveAchieveHeading::ms_fHeadingTolerance, m_fTime);
	return rage_new CTaskComplexControlMovement( pTask );
}

//***************************************
// CTaskMoveFaceTarget
//

CTask::FSM_Return CTaskMoveFaceTarget::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	if (pPed && m_Target.GetIsValid())
	{
		Vector3 position(VEC3_ZERO);
		m_Target.GetPosition(position);

		bool bUpdateDesiredHeading = !m_bLimitUpdateBasedOnDistance;
		if(!bUpdateDesiredHeading)
		{
			static dev_float s_fMinDistance = 1.0f;
			static dev_float s_fMaxDistance = 10.0f;
			static dev_float s_fMinAngleForMinDistance = 0.25f;
			static dev_float s_fMinAngleForMaxDistance = 0.075f;

			float fDist = Dist(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(position)).Getf();
			fDist = Clamp(fDist, s_fMinDistance, s_fMaxDistance);
			float fLerp = (fDist - s_fMinDistance) / (s_fMaxDistance - s_fMinDistance);
			float fMinAngle = Lerp(fLerp, s_fMinAngleForMinDistance, s_fMinAngleForMaxDistance);

			const Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(position.x, position.y, vPos.x, vPos.y);
			float fAngle = Abs(SubtractAngleShorter(fDesiredHeading, pPed->GetCurrentHeading()));
			if(fAngle >= fMinAngle)
			{
				bUpdateDesiredHeading = true;
			}
		}

		if(bUpdateDesiredHeading)
		{
			pPed->SetDesiredHeading(position);
		}

		pPed->GetMotionData()->SetDesiredMoveBlendRatio(GetMoveBlendRatio());

		return FSM_Continue;
	}

	return FSM_Quit;
}

//***************************************
// CTaskComplexTurnToFaceEntityOrCoord
//

CTaskTurnToFaceEntityOrCoord::CTaskTurnToFaceEntityOrCoord(const CEntity* pEntity, const float fHeadingChangeRateFrac, const float fHeadingTolerance, const float fTime)
 : m_pEntity(pEntity),
   m_bFacingEntity(true),
   m_fTimeToTurn(fTime)
{
    m_fHeadingChangeRateFrac = fHeadingChangeRateFrac;
    m_fHeadingTolerance = fHeadingTolerance;
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY);
}

CTaskTurnToFaceEntityOrCoord::CTaskTurnToFaceEntityOrCoord(const Vector3& vTarget, const float fHeadingChangeRateFrac, const float fHeadingTolerance, const float fTime)
: m_pEntity(0),
  m_bFacingEntity(false),
  m_vTarget(vTarget),
  m_fTimeToTurn(fTime)
{
    m_fHeadingChangeRateFrac = fHeadingChangeRateFrac;
    m_fHeadingTolerance = fHeadingTolerance;
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY);
}

CTaskTurnToFaceEntityOrCoord::~CTaskTurnToFaceEntityOrCoord()
{
}


CTask::FSM_Return CTaskTurnToFaceEntityOrCoord::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_AchievingHeading)
			FSM_OnEnter
				return StateAchievingHeading_OnEnter(pPed);
			FSM_OnUpdate
				return StateAchievingHeading_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskTurnToFaceEntityOrCoord::StateAchievingHeading_OnEnter(CPed * pPed)
{
	// Quit if ped is to face entity that has been deleted.
	if(m_bFacingEntity && 0==m_pEntity)
	{
		return FSM_Quit;
	}

	const float fTargetHeading=ComputeTargetHeading(pPed);
	CTask * pMoveTask = rage_new CTaskMoveAchieveHeading(fTargetHeading, m_fHeadingChangeRateFrac, m_fHeadingTolerance, m_fTimeToTurn);	
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));

	return FSM_Continue;
}

CTask::FSM_Return CTaskTurnToFaceEntityOrCoord::StateAchievingHeading_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

	if(m_bFacingEntity)
	{
		// Quit if ped is to face entity that has been deleted.
		if(m_pEntity == NULL)
		{
			return FSM_Quit;
		}

		float fTargetHeading = pPed->GetCurrentHeading();
		if(m_pEntity)
		{	
			fTargetHeading = ComputeTargetHeading(pPed);
		}

		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			Assert(((CTaskComplexControlMovement*)GetSubTask())->GetMoveTaskType() == CTaskTypes::TASK_MOVE_ACHIEVE_HEADING);
			CTask * pMovementTask = ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);
			Assert( pMovementTask == NULL || pMovementTask->GetTaskType() == CTaskTypes::TASK_MOVE_ACHIEVE_HEADING );

			if(pMovementTask && pMovementTask->GetTaskType() == CTaskTypes::TASK_MOVE_ACHIEVE_HEADING)
			{
				((CTaskMoveAchieveHeading*)pMovementTask)->SetHeading(fTargetHeading, m_fHeadingChangeRateFrac, m_fHeadingTolerance);
			}
		}
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskInfo* CTaskTurnToFaceEntityOrCoord::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	if (m_bFacingEntity)
	{
		CEntity* pEntity = NULL;

		if (m_pEntity)
		{
			pEntity = const_cast<CEntity*>(m_pEntity.Get());
		}

		pInfo = rage_new CClonedTurnToFaceEntityOrCoordInfo(pEntity);
	}
	else
	{
		pInfo = rage_new CClonedTurnToFaceEntityOrCoordInfo(m_vTarget);
	}

	return pInfo;
}

float CTaskTurnToFaceEntityOrCoord::ComputeTargetHeading(CPed* pPed) const
{
	Vector3 vTarget;
	if(m_bFacingEntity)
	{
		Assert(m_pEntity);
		vTarget = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition());
	}
	else
	{
		vTarget = m_vTarget;
	}

	Vector3 vDir = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vDir.Normalize();

	float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(vDir.x, vDir.y, 0, 0);
	fTargetHeading = fwAngle::LimitRadianAngle(fTargetHeading);

	return fTargetHeading;
}


CTask* CClonedTurnToFaceEntityOrCoordInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	CTask* pTask = NULL;

	if (m_hasTargetEntity)
	{
		netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntity.GetEntityID());

		// the entity this ped was looking at might have been removed (such as the leaving player)
		if(!pNetObj)
			return NULL; 

		if(aiVerifyf(pNetObj->GetEntity(), "CClonedTurnToFaceEntityOrCoordInfo: Network object %s has no entity", pNetObj->GetLogName()))
		{
			pTask = rage_new CTaskTurnToFaceEntityOrCoord(pNetObj->GetEntity());
		}
	}
	else
	{
		pTask = rage_new CTaskTurnToFaceEntityOrCoord(m_vTargetCoord);
	}

	return pTask;
}
