#include "Task/Scenario/Types/TaskCoupleScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/info/ScenarioInfo.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Event/Event.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "Scene/EntityIterator.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Couple scenario
//-------------------------------------------------------------------------

CTaskCoupleScenario::Tunables CTaskCoupleScenario::sm_Tunables;

IMPLEMENT_SCENARIO_TASK_TUNABLES(CTaskCoupleScenario, 0xb4719972);

#if __BANK
Vector3 CTaskCoupleScenario::ms_vOverridenVector = Vector3::ZeroType;
bool CTaskCoupleScenario::ms_bUseOverridenTargetPos = false;
bool CTaskCoupleScenario::ms_bStopFollower = false;
#endif

CTaskCoupleScenario::CTaskCoupleScenario(s32 scenarioType, CScenarioPoint* pScenarioPoint, CPed* pPartnerPed, u32 flags)
		: CTaskScenario(scenarioType,pScenarioPoint)
		, m_pPartnerPed(pPartnerPed)
		, m_vOffset(Vector3::ZeroType)
		, m_Flags(flags)
		, m_KeepMovingWhenFindingPath(false)
{
	SetInternalTaskType(CTaskTypes::TASK_COUPLE_SCENARIO);
}

CTaskCoupleScenario::~CTaskCoupleScenario()
{
}

bool CTaskCoupleScenario::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Maybe do some paired reactions here
	// We need a way to tell the damage done, if its bump, the reaction
	// should be less than say someone attacking us
	if (pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_GUN_AIMED_AT)
	{
		((CEvent*)pEvent)->CommunicateEvent(pPed);
	}

	if (GetSubTask())
	{
		return GetSubTask()->MakeAbortable(iPriority,pEvent);
	}
	return true;
}


aiTask* CTaskCoupleScenario::Copy() const
{
	return rage_new CTaskCoupleScenario(m_iScenarioIndex, GetScenarioPoint(), m_pPartnerPed, m_Flags.GetAllFlags());
}

void CTaskCoupleScenario::CleanUp()
{
	GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, true );
}

CTask::FSM_Return CTaskCoupleScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_UsingScenario)
			FSM_OnEnter
				UsingScenario_OnEnter();
			FSM_OnUpdate
				return UsingScenario_OnUpdate();

		FSM_State(State_Leader_Walks)
			FSM_OnEnter
				Leader_Walks_OnEnter();
			FSM_OnUpdate
				return Leader_Walks_OnUpdate();

		FSM_State(State_Leader_Waits)
			FSM_OnEnter
				Leader_Waits_OnEnter();
			FSM_OnUpdate
				return Leader_Waits_OnUpdate();

		FSM_State(State_Follower_Walks)
			FSM_OnEnter
				Follower_Walks_OnEnter();
			FSM_OnUpdate
				return Follower_Walks_OnUpdate();

		FSM_State(State_Follower_Waits)
			FSM_OnEnter
				Follower_Waits_OnEnter();
			FSM_OnUpdate
				return Follower_Waits_OnUpdate();	

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCoupleScenario::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

#if __BANK
	if (CMoveFollowEntityOffsetDebug::ms_bRenderDebug)
	{
		if (GetIsLeader())
		{
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),Color_red,0,0,"Leader");
		}
		else
		{
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),Color_red,0,0,"Follower");
		}
	}
#endif // __BANK

	s32 iState = GetState();
	if (iState != State_Start)
	{
		if (!m_pPartnerPed)
		{
			return FSM_Quit;
		}

		// 		aiTask* pTask = m_pPartnerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COUPLE_SCENARIO);
		// 		if (!pTask)
		// 		{
		// 			return FSM_Quit;
		// 		}

		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
		taskAssert(pScenarioInfo->GetIsClass<CScenarioCoupleInfo>());
		// If we're the follower and the leader is looking, look at them if we're not looking 
		if (static_cast<const CScenarioCoupleInfo*>(pScenarioInfo)->GetUseHeadLookAt())
		{
			if (!GetIsLeader() && m_pPartnerPed->GetIkManager().IsLooking())
			{
				if (!pPed->GetIkManager().IsLooking())
				{
					pPed->GetIkManager().LookAt(0, m_pPartnerPed, fwRandom::GetRandomNumberInRange(1000,5000), BONETAG_HEAD, NULL, LF_WIDE_YAW_LIMIT);
				}
			}
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskCoupleScenario::Start_OnUpdate()
{
	if(m_Flags.IsFlagSet(CF_UseScenario) && GetScenarioPoint())
	{
		SetState(State_UsingScenario);
		return FSM_Continue;
	}

	PickWalkingState();

	return FSM_Continue;
}


void CTaskCoupleScenario::UsingScenario_OnEnter()
{
	u32 flags = CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_SkipEnterClip | CTaskUseScenario::SF_CheckConditions;
	if(GetIsLeader())
	{
		flags |= CTaskUseScenario::SF_SynchronizedMaster;
	}
	else
	{
		flags |= CTaskUseScenario::SF_SynchronizedSlave;
	}

	const int scenarioType = GetScenarioType();

	// Create the task.
	CTaskUseScenario* pTask = rage_new CTaskUseScenario(scenarioType, GetScenarioPoint(), flags);
	pTask->SetSynchronizedPartner(m_pPartnerPed);
	SetNewTask(pTask);
}


CTask::FSM_Return CTaskCoupleScenario::UsingScenario_OnUpdate()
{
	CTask* pSubTask = GetSubTask();
	if(!pSubTask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Only use the scenario once, in case we get back to the Start state later,
		// we should just wander.
		m_Flags.ClearFlag(CF_UseScenario);

		PickWalkingState();
		return FSM_Continue;
	}

	// Check if the scenario sub-task ends in a walking exit.
	if(!m_KeepMovingWhenFindingPath && pSubTask)
	{
		// Ensure the sub-task is a use scenario task.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			//Grab the task.
			const CTaskUseScenario* pTask = static_cast<const CTaskUseScenario*>(pSubTask);
	
			//Ensure the task is in the correct state.
			if(pTask->GetState() == CTaskUseScenario::State_Exit)
			{
				if(pTask->GetExitClipsEndsInWalk())
				{
					m_KeepMovingWhenFindingPath = true;
				}
			}
		}
	}

	return FSM_Continue;
}


void CTaskCoupleScenario::Leader_Walks_OnEnter()
{
	CPed *pPed = GetPed();

	taskAssert(GetIsLeader());
#if __BANK
	if (ms_bUseOverridenTargetPos)
	{
		SetNewTask(rage_new CTaskGoToPointAnyMeans(MOVEBLENDRATIO_WALK, ms_vOverridenVector));
	}
	else
#endif
	{
		CTaskWander* pWanderTask = rage_new CTaskWander(MOVEBLENDRATIO_WALK, pPed->GetTransform().GetHeading());

		if(m_KeepMovingWhenFindingPath)
		{
			pWanderTask->KeepMovingWhilstWaitingForFirstPath(pPed);
		}
		SetNewTask(pWanderTask);
	}
	m_KeepMovingWhenFindingPath = false;
}


CTask::FSM_Return CTaskCoupleScenario::Leader_Walks_OnUpdate()
{
	CPed *pPed = GetPed();

#if __BANK
	TUNE_GROUP_BOOL(COUPLE_DEBUG,coupleOverrridePos, false);

	if (coupleOverrridePos)
	{
		Vector3 vMousePosition(Vector3::ZeroType);
		if (CPhysics::GetMeasuringToolPos(0, vMousePosition))
		{			
			if (vMousePosition.x != ms_vOverridenVector.x || vMousePosition.y != ms_vOverridenVector.y || vMousePosition.z != ms_vOverridenVector.z)
			{
				ms_bUseOverridenTargetPos = true;
				ms_vOverridenVector = vMousePosition;
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	if (ms_bUseOverridenTargetPos && !pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS))
	{
		ms_bUseOverridenTargetPos = false;
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
#endif

	// Check on state of follower - if they're trying to catch up, lets slow down
	aiTask* pTask = m_pPartnerPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET);

	if (pTask)
	{
		CTaskMoveFollowEntityOffset* pMoveTask = static_cast<CTaskMoveFollowEntityOffset*>(pTask);

		CTaskMoveWander* pTask = static_cast<CTaskMoveWander*>(pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_WANDER));

		if (pTask)
		{
			if (pMoveTask->GetState() == CTaskMoveFollowEntityOffset::State_CatchingUp)
			{
				// Get the speed of the entity we're following
				Vector3 vTargetSpeed = static_cast<CPhysical*>(m_pPartnerPed.Get())->GetVelocity();
				float fMoveBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, vTargetSpeed.XYMag());
				fMoveBlend += CTaskMoveFollowEntityOffset::ms_fSlowDownDelta;

				// Continually set the MBR of the goto task
				fMoveBlend = Max(Clamp(fMoveBlend,0.0f,3.0f), MOVEBLENDRATIO_WALK * 0.75f);

				if (pTask->IsMoveTask())
				{
					static_cast<CTaskMove*>(pTask)->SetMoveBlendRatio(fMoveBlend);
				}

				const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
				taskAssert(pScenarioInfo->GetIsClass<CScenarioCoupleInfo>());
				if (static_cast<const CScenarioCoupleInfo*>(pScenarioInfo)->GetUseHeadLookAt())
				{
					if (!pPed->GetIkManager().IsLooking())
					{
						pPed->GetIkManager().LookAt(0, m_pPartnerPed, fwRandom::GetRandomNumberInRange(1000,5000), BONETAG_HEAD, NULL, LF_WIDE_YAW_LIMIT);
					}
				}
			}
			else
			{
				if (pTask->IsMoveTask())
				{
					static_cast<CTaskMove*>(pTask)->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
				}
			}
		}
	}

	if (IsLeaderDeliberatelyWaiting())
	{
		SetState(State_Leader_Waits);
		return FSM_Continue;
	}

	// Partner has been left behind, stop and wait
	const float fDistBetweenSqd = DistSquared(pPed->GetTransform().GetPosition(), m_pPartnerPed->GetTransform().GetPosition()).Getf();
	if (fDistBetweenSqd > sm_Tunables.m_StopDistSq)
	{
		SetState(State_Leader_Waits);
		return FSM_Continue;
	}

#if __BANK
	if (!ms_bUseOverridenTargetPos)
#endif
	{
		if (GetIsSubtaskFinished(CTaskTypes::TASK_WANDER))
		{	
			return FSM_Quit;
		}
	}
	return FSM_Continue;
}

void CTaskCoupleScenario::Leader_Waits_OnEnter()
{
	// 	Vector3 vDiff = m_pPartnerPed->GetPosition()-pPed->GetPosition();
	// 	const float fNewHeading = rage::Atan2f(vDiff.y,vDiff.x);
	// 	SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveAchieveHeading(fNewHeading, 1.5f)));
}

CTask::FSM_Return CTaskCoupleScenario::Leader_Waits_OnUpdate()
{
	CPed *pPed = GetPed();

	pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pPartnerPed->GetTransform().GetPosition()));

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
	taskAssert(pScenarioInfo->GetIsClass<CScenarioCoupleInfo>());
	if (static_cast<const CScenarioCoupleInfo*>(pScenarioInfo)->GetUseHeadLookAt())
	{
		if (!pPed->GetIkManager().IsLooking())
		{
			pPed->GetIkManager().LookAt(0, m_pPartnerPed, fwRandom::GetRandomNumberInRange(1000,5000), BONETAG_HEAD, NULL, LF_WIDE_YAW_LIMIT);
		}
	}

	// Partner has caught up a bit, resume walking
	const float fDistBetweenSqd = DistSquared(pPed->GetTransform().GetPosition(), m_pPartnerPed->GetTransform().GetPosition()).Getf();

	if (fDistBetweenSqd < sm_Tunables.m_ResumeDistSq && !IsLeaderDeliberatelyWaiting() )
	{
		SetState(State_Leader_Walks);
	}
	return FSM_Continue;
}

void CTaskCoupleScenario::Follower_Walks_OnEnter()
{
	CPed *pPed = GetPed();

	taskAssert(!GetIsLeader());

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, false );

#if __BANK
	if (ms_bStopFollower)
	{
		TUNE_GROUP_INT(COUPLE_DEBUG, followerWaitTime, 2000, 0, 10000, 100);
		SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveStandStill(followerWaitTime)));
	}
	else
#endif
	{
		// Get the offset from the scenario info
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
		taskAssert(pScenarioInfo->GetIsClass<CScenarioCoupleInfo>());

		CTaskMoveFollowEntityOffset* pMoveTask = rage_new CTaskMoveFollowEntityOffset(m_pPartnerPed, MOVEBLENDRATIO_WALK, sm_Tunables.m_TargetDistance, static_cast<const CScenarioCoupleInfo*>(pScenarioInfo)->GetMoveFollowOffset(), 9999999);
		SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
	}

	// TODO: We should probably respect the value of this, and force CTaskMoveFollowEntityOffset
	// to start out moving if it's set. May need to modify CTaskMoveFollowEntityOffset to support this, though.
	m_KeepMovingWhenFindingPath = false;
}

CTask::FSM_Return CTaskCoupleScenario::Follower_Walks_OnUpdate()
{
	CPed *pPed = GetPed();

#if __BANK
	TUNE_GROUP_BOOL(COUPLE_DEBUG, stopFollower, false)
		if (stopFollower)
		{
			ms_bStopFollower = true;
			stopFollower = false;
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}

		if (ms_bStopFollower)
		{
			if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				ms_bStopFollower = false;
				return FSM_Continue;
			}
		}
#endif

		CTaskMoveFollowEntityOffset * pFollowersFollowTask = IsFollowerDeliberatelyWaiting( *pPed );
		if ( pFollowersFollowTask )
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			pFollowersFollowTask->RequestStop();
			SetState(State_Follower_Waits);
			return FSM_Continue;
		}

		if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
		{	
			return FSM_Quit;
		}

		return FSM_Continue;
}

void CTaskCoupleScenario::Follower_Waits_OnEnter()
{
	//	TUNE_GROUP_INT(COUPLE_DEBUG, followEntityOffsetWaitTime, 500, 0, 10000, 1);
	//SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveStandStill(followEntityOffsetWaitTime)));
}

CTaskMoveFollowEntityOffset *CTaskCoupleScenario::IsFollowerDeliberatelyWaiting( const CPed &ped ) const
{
	CTaskMove* pLeadersMoveTask = m_pPartnerPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	CTaskMoveFollowEntityOffset* pFollowersFollowTask = static_cast<CTaskMoveFollowEntityOffset*>(ped.GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET));
	if (pLeadersMoveTask && pLeadersMoveTask->IsTaskMoving() && pFollowersFollowTask)
	{
		// We are about to call GetTarget(), which may assert if !HasTarget(). I think this might happen
		// if the leader stops to change direction or something, and the only task this has been observed
		// for has been CTaskMoveStandStill. In that case, returning NULL ("false") is probably the safest option,
		// so we don't risk the peds both waiting for each other or something.
		if(pLeadersMoveTask && !pLeadersMoveTask->HasTarget())
		{
			taskAssertf(pLeadersMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_STAND_STILL, "Unexpected task type: %d", (int)pLeadersMoveTask->GetTaskType());
			return NULL;
		}

		const Vector3 vFollowerPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
		Vector3 vToLeaderTargetPos = pLeadersMoveTask->GetTarget() - vFollowerPos;
		vToLeaderTargetPos.Normalize();
		Vector3 vToFollowersTargetPos = pFollowersFollowTask->GetTarget() - vFollowerPos;
		vToFollowersTargetPos.Normalize();

		TUNE_GROUP_FLOAT(COUPLE_DEBUG, stopDotTol, -0.5f, -1.0f, 1.0f, 0.01f);
		const float fDot = vToLeaderTargetPos.Dot(vToFollowersTargetPos);
		if (fDot < stopDotTol)
		{
			return pFollowersFollowTask;
		}
	}
	return NULL;
}

bool CTaskCoupleScenario::IsLeaderDeliberatelyWaiting() const
{
	// If our follower ped is running a Natural Motion control task then we are waiting for them
	if (m_pPartnerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL) )
	{
		return true;
	}
	return false;
}


CTask::FSM_Return CTaskCoupleScenario::Follower_Waits_OnUpdate()
{
	CPed *pPed = GetPed();

	CTaskMoveFollowEntityOffset* pFollowersFollowTask = static_cast<CTaskMoveFollowEntityOffset*>(pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET));
	if (!pFollowersFollowTask || pFollowersFollowTask->GetState() != CTaskMoveFollowEntityOffset::State_SlowingDown)
	{
		if ( !IsFollowerDeliberatelyWaiting(*pPed) )
		{
			SetState(State_Start);
		}
	}
	return FSM_Continue;
}


void CTaskCoupleScenario::PickWalkingState()
{
	if(GetIsLeader())
	{
		SetState(State_Leader_Walks);
	}
	else 
	{
		SetState(State_Follower_Walks);
	}
}

#if !__FINAL

void CTaskCoupleScenario::Debug() const
{
	if (GetSubTask())
	{
		GetSubTask()->Debug();
	}
}

const char * CTaskCoupleScenario::GetStaticStateName( s32 iState  )
{
	taskAssert( iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:			return "State_Start";
	case State_UsingScenario:	return "State_UsingScenario";
	case State_Leader_Walks:	return "State_Leader_Walks";
	case State_Leader_Waits:	return "State_Leader_Waits";
	case State_Follower_Walks:	return "State_Follower_Walks";
	case State_Follower_Waits:	return "State_Follower_Waits";
	case State_Finish:			return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif
