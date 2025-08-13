// FILE :    TaskMoveWithinDefensiveArea.cpp
// PURPOSE : Controls a peds movement within their defensive area
// CREATED : 02-04-2012

// File header
#include "Task/Movement/TaskMoveWithinDefensiveArea.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskMoveWithinDefensiveArea
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskMoveWithinDefensiveArea::CTaskMoveWithinDefensiveArea(const CPed* pPrimaryTarget)
: m_pPrimaryTarget(pPrimaryTarget)
, m_bHasSearchedForCover(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WITHIN_DEFENSIVE_AREA);
}

CTaskMoveWithinDefensiveArea::~CTaskMoveWithinDefensiveArea()
{}

CTask::FSM_Return CTaskMoveWithinDefensiveArea::ProcessPreFSM()
{
	if( !GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsActive() )
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWithinDefensiveArea::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_SearchForCover)
			FSM_OnEnter
				SearchForCover_OnEnter();
			FSM_OnUpdate
				return SearchForCover_OnUpdate();

		FSM_State(State_GoToDefensiveArea)
			FSM_OnEnter
				GoToDefensiveArea_OnEnter();
			FSM_OnUpdate
				return GoToDefensiveArea_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskMoveWithinDefensiveArea::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// If we don't have a target, can't use cover, or are just seeking cover (?) then go straight to the defensive area
	if( !m_pPrimaryTarget ||
		m_pPrimaryTarget->IsDead() ||
		m_uConfigFlags.IsFlagSet(CF_DontSearchForCover) ||
	    !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover) ||
	    pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) ||
	    pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_MoveToLocationBeforeCoverSearch) )
	{
		SetState(State_GoToDefensiveArea);
	}
	else
	{
		SetState(State_SearchForCover);
	}

	return FSM_Continue;
}

void CTaskMoveWithinDefensiveArea::SearchForCover_OnEnter()
{
	m_bHasSearchedForCover = true;

	// Setup our desired attributes and create our "cover" task
	CPed* pPed = GetPed();

	float fDesiredMBR = MOVEBLENDRATIO_RUN;
	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(pPed, bShouldStrafe, fDesiredMBR);

	s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, fDesiredMBR);

	Vector3 vDefensiveAreaPoint(0.0f, 0.0f, 0.0f);
	CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo( pPed, vDefensiveAreaPoint );
	const Vector3 vPrimaryTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
	CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(m_pPrimaryTarget));
	s32 iSearchFlags = CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_RequiresLosToTarget|CTaskCover::CF_RunSubtaskWhenMoving|CTaskCover::CF_RunSubtaskWhenStationary|CTaskCover::CF_CoverSearchByPos|CTaskCover::CF_SeekCover|CTaskCover::CF_AllowClimbLadderSubtask;
	if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsUnarmed())
	{
		iSearchFlags |= CTaskCover::CF_QuitAfterMovingToCover;
	}
	pCoverTask->SetSearchFlags(iSearchFlags);
	pCoverTask->SetSearchType(CCover::CS_MUST_PROVIDE_COVER);
	pCoverTask->SetSearchPosition(vDefensiveAreaPoint);
	pCoverTask->SetMoveBlendRatio(fDesiredMBR);
	pCoverTask->SetSubtaskToUseWhilstSearching(	rage_new CTaskCombatAdditionalTask( iCombatRunningFlags, m_pPrimaryTarget, vPrimaryTargetPosition ) );
	SetNewTask(pCoverTask);
}

CTask::FSM_Return CTaskMoveWithinDefensiveArea::SearchForCover_OnUpdate()
{
	CTask* pSubTask = GetSubTask();
	if( !pSubTask || GetIsSubtaskFinished(CTaskTypes::TASK_COVER) )
	{
		return FSM_Quit;
	}

	// Check if we should restart our current state because we're moving to a position no longer within the defensive area
	CTaskCover* pCoverSubTask = static_cast<CTaskCover*>(pSubTask);
	if(pCoverSubTask->GetState() == CTaskCover::State_MoveToSearchLocation)
	{
		UpdateGoToLocation();
	}

	return FSM_Continue;
}

void CTaskMoveWithinDefensiveArea::GoToDefensiveArea_OnEnter()
{
	// Go to the closest found position in the defensive area
	float fRadius = CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo( GetPed(), m_vDefensiveAreaPoint );
	float fNavRadius = fRadius - fRadius * CTaskCombat::ms_Tunables.m_SafetyProportionInDefensiveAreaMax;

	// Scale away a few % just to be sure we don't get any odd floating point overlapping bullshit
	fNavRadius = Min(fNavRadius * 0.95f, CTaskMoveFollowNavMesh::ms_fTargetRadius);

	// If we have a target we need to run our combat sub task
	if(m_pPrimaryTarget)
	{
		const Vector3 vPrimaryTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
		float fDesiredMBR = MOVEBLENDRATIO_RUN;
		bool bShouldStrafe = true;
		CTaskCombat::ChooseMovementAttributes(GetPed(), bShouldStrafe, fDesiredMBR);

		s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, fDesiredMBR) | CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions;

		CTaskMoveFollowNavMesh* pNavmeshTask = rage_new CTaskMoveFollowNavMesh(fDesiredMBR, m_vDefensiveAreaPoint, fNavRadius);
		CTaskCombatAdditionalTask* pAdditionalCombatTask = rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, vPrimaryTargetPosition);
		CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pNavmeshTask, pAdditionalCombatTask, CTaskComplexControlMovement::TerminateOnMovementTask);
		pTask->SetAllowClimbLadderSubtask(true);
		SetNewTask(pTask);
	}
	else
	{
		CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, m_vDefensiveAreaPoint, fNavRadius, 
																						CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false,
																						CTaskCombat::SpheresOfInfluenceBuilder));
		pTask->SetAllowClimbLadderSubtask(true);
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskMoveWithinDefensiveArea::GoToDefensiveArea_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{

		CPed* pPed = GetPed();
		if( !m_bHasSearchedForCover && m_pPrimaryTarget && !m_pPrimaryTarget->IsDead() && 
			!m_uConfigFlags.IsFlagSet(CF_DontSearchForCover) &&
			!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) &&
		    pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover) &&
		    pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_MoveToLocationBeforeCoverSearch) )
		{
			SetState(State_SearchForCover);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CPed* pPed = GetPed();
		if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ClearAreaSetDefensiveIfDefensiveCannotBeReached))
		{
			CTaskComplexControlMovement* pCtrlMove = static_cast<CTaskComplexControlMovement*>(pSubTask);
			CTask* pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
			if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
			{
				if(static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->IsUnableToFindRoute())
				{
					pPed->GetPedIntelligence()->GetDefensiveArea()->Reset();
					return FSM_Quit;
				}
			}
		}
	}

	UpdateGoToLocation();

	return FSM_Continue;
}

bool CTaskMoveWithinDefensiveArea::UpdateGoToLocation()
{
	CPed* pPed = GetPed();
	if(!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(m_vDefensiveAreaPoint))
	{
		// Check if we should restart our current state because we're moving to a position no longer within the defensive area
		CTaskMoveFollowNavMesh* pFollowNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
		if(pFollowNavMeshTask)
		{
			CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo(pPed, m_vDefensiveAreaPoint);
			pFollowNavMeshTask->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vDefensiveAreaPoint));
			return true;
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskMoveWithinDefensiveArea::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_SearchForCover:					return "State_SearchForCover";
		case State_GoToDefensiveArea:				return "State_GoToDefensiveArea";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL
