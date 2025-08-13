// Title	:	TaskStayInCover.cpp
// Author	:	Phil Hooker
// Started	:	05-01-09

// This selection of class' enables individual peds to seek and hide in cover

//Framework headers
#include "ai/task/taskchannel.h"

//Game headers.
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/Cover/TaskStayInCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Weapons/Info/WeaponInfoManager.h"

//Rage headers
#include "Vector/Vector3.h"

AI_OPTIMISATIONS()

CTask* CClonedStayInCoverInfo::CreateLocalTask(fwEntity* pEntity) 
{
	CPed* pPed = static_cast<CPed*>(pEntity);

	CTaskStayInCover* pStayInCoverTask = rage_new CTaskStayInCover();

	if (pPed && GetState() == CTaskStayInCover::State_InCoverInArea)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CCoverPoint* pCoverPoint = CCover::FindClosestCoverPoint(pPed, pos);

		if(pCoverPoint)
		{
			pPed->SetCoverPoint(pCoverPoint);
			pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);
		}
	}

	return pStayInCoverTask;
}

CTaskStayInCover::CTaskStayInCover() : 
m_bIsInDefensiveArea(false),
m_bCanUseGunTask(true),
m_goToAreaTimer(CTaskCombat::ms_Tunables.m_fGoToDefAreaTimeOut)
{
	SetInternalTaskType(CTaskTypes::TASK_STAY_IN_COVER);
}

CTaskStayInCover::~CTaskStayInCover()
{
}

void CTaskStayInCover::CleanUp( )
{
	CPed* pPed = GetPed();

	//Release the combat director.
	pPed->GetPedIntelligence()->ReleaseCombatDirector();
}

CTask::FSM_Return CTaskStayInCover::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForCover, true );

	m_goToAreaTimer.Tick();

	// Update whether we're in the defensive area assigned to us if we have one
	// Work out if we have a defensive area and are outside it
	CPedIntelligence* pPedIntell = pPed->GetPedIntelligence();
	if( pPedIntell->GetDefensiveArea()->IsActive() )
	{
		// If the ped has a cover point, use the cover position instead
		Vector3 vPositionToCheck = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if( pPed->GetCoverPoint() )
			pPed->GetCoverPoint()->GetCoverPointPosition(vPositionToCheck);

		m_bIsInDefensiveArea = true;

		if (!pPedIntell->GetDefensiveArea()->IsPointInDefensiveArea(vPositionToCheck))
		{
			m_bIsInDefensiveArea = false;
		}
	}

	CTaskEnterCover::PrestreamAiCoverEntryAnims(*pPed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	// Initial start to the task
	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	// Aim in the direction defined in the defensive area 
	FSM_State(State_StandAndAimInArea)
		FSM_OnEnter
			return StandAndAimInArea_OnEnter(pPed);
		FSM_OnUpdate
			return StandAndAimInArea_OnUpdate(pPed);

	// Go to defensive area direct
	FSM_State(State_GoToDefensiveArea)
		FSM_OnEnter
			return GoToDefensiveArea_OnEnter(pPed);
		FSM_OnUpdate
			return GoToDefensiveArea_OnUpdate(pPed);

	// Initial start to the task
	FSM_State(State_SearchForCoverInArea)
		FSM_OnEnter
			return SearchForCoverInArea_OnEnter(pPed);
		FSM_OnUpdate
			return SearchForCoverInArea_OnUpdate(pPed);

	// Fire from cover
	FSM_State(State_InCoverInArea)
		FSM_OnEnter
			InCoverInArea_OnEnter(pPed);
		FSM_OnUpdate
			return InCoverInArea_OnUpdate(pPed);

FSM_End
}

CTaskInfo*	CTaskStayInCover::CreateQueriableState() const 
{ 
	return rage_new CClonedStayInCoverInfo(GetState()); 
}

void CTaskStayInCover::CalculateAimAtPosition( CPed* pPed, Vector3& vTargetPosition, Vector3& vCoverSearchStartPosition)
{
	// Get the direction to provide cover from
	if( pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() )
	{
		vTargetPosition = pPed->GetPedIntelligence()->GetDefensiveArea()->GetDefaultCoverFromPosition();
		float fUnusedRadius = 0.0f;
		pPed->GetPedIntelligence()->GetDefensiveArea()->GetCentreAndMaxRadius(vCoverSearchStartPosition,fUnusedRadius);
		if( vTargetPosition.IsZero() )
		{
			vTargetPosition = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * 20.0f) + vCoverSearchStartPosition;
		}
	}
	else
	{
		vCoverSearchStartPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vTargetPosition = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * 20.0f) + vCoverSearchStartPosition;
	}
}

CTask::FSM_Return CTaskStayInCover::Start_OnUpdate(CPed* pPed)
{
	// Run the random chance once in this task so it is consistent
	m_bCanUseGunTask = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatStrafeWhenMovingChance);

	// Request a combat director for this ped
	pPed->GetPedIntelligence()->RequestCombatDirector();

	weaponAssert(pPed->GetWeaponManager());
 	pPed->GetWeaponManager()->EquipBestWeapon();

	// If we are coming from already being in cover then try to stay in cover
	CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
	if (pCoverPoint && pPed->GetIsInCover())
	{
		Vector3 vCoverCoords(Vector3::ZeroType);
		pCoverPoint->GetCoverPointPosition(vCoverCoords);

		Vec3V vToCover = VECTOR3_TO_VEC3V(vCoverCoords) - pPed->GetTransform().GetPosition();
		vToCover.SetZ(V_ZERO);
		if(IsLessThanAll(MagSquared(vToCover), ScalarVFromF32(square(.25f))))
		{
			// Make sure the cover position is in our defensive area (if we have one)
			CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
			if(!pDefensiveArea || !pDefensiveArea->IsActive() || pDefensiveArea->IsPointInDefensiveArea(vCoverCoords))
			{
				SetState(State_InCoverInArea);
				return FSM_Continue;
			}
		}
	}

	// We don't have a valid cover point so make sure to release one if we do have it
	pPed->ReleaseCoverPoint();

	// Check if we should move to the defensive area first
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_MoveToLocationBeforeCoverSearch))
	{
		SetState(State_GoToDefensiveArea);
	}
	else
	{
		SetState(State_SearchForCoverInArea);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::StandAndAimInArea_OnEnter(CPed* pPed)
{
	m_fCoverSearchTimer = 0.0f;

	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(m_bCanUseGunTask && pWeaponInfo && pWeaponInfo->GetIsGun())
	{
		// Calculate the aim at positions
		Vector3 vTargetPosition;
		Vector3 vCoverSearchStartPosition;
		CalculateAimAtPosition(pPed, vTargetPosition, vCoverSearchStartPosition);

		// Aim At a position 
		CTaskGun* pGunTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(vTargetPosition));
		pGunTask->SetFiringPatternHash(FIRING_PATTERN_BURST_FIRE);
		SetNewTask(pGunTask);
	}
	else
	{
		SetNewTask(rage_new CTaskDoNothing(-1));
	}

	return FSM_Continue;
}

dev_float TIME_BETWEEN_COVER_CHECKS = 1.0f;
CTask::FSM_Return CTaskStayInCover::StandAndAimInArea_OnUpdate( CPed* pPed )
{
	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	else
	{
		m_fCoverSearchTimer += fwTimer::GetTimeStep();
		// Periodically check we are still within the correct area
		if( m_fCoverSearchTimer > TIME_BETWEEN_COVER_CHECKS )
		{
			SetState(State_SearchForCoverInArea);
			m_fCoverSearchTimer = 0.0f;
		}
	}

	CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
	if (pIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) && !m_bIsInDefensiveArea)
	{
		SetState(State_GoToDefensiveArea);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::GoToDefensiveArea_OnEnter(CPed* pPed)
{
	Vector3 vDefensiveAreaPoint(0.0f, 0.0f, 0.0f);
	CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo( pPed, vDefensiveAreaPoint );

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, vDefensiveAreaPoint);

	// If already moving we should keep moving
	Vector2 vCurrentMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMbr);
	if(vCurrentMbr.IsNonZero())
	{
		pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
	m_goToAreaTimer.Reset();
	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::GoToDefensiveArea_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_SearchForCoverInArea);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::SearchForCoverInArea_OnEnter(CPed* pPed)
{
	// Calculate the aim at positions
	Vector3 vTargetPosition;
	Vector3 vCoverSearchStartPosition;
	CalculateAimAtPosition(pPed, vTargetPosition, vCoverSearchStartPosition);

	s32 iFlags = CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_CoverSearchByPos|CTaskCover::CF_SeekCover;

	// Aim At a position 
	CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(vTargetPosition));

	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() && !pPed->GetPedIntelligence()->GetDefensiveArea()->GetDefaultCoverFromPosition().IsZero())
	{
		pCoverTask->SetSearchType(CCover::CS_MUST_PROVIDE_COVER);
	}	
	
	// Only create the gun task if we are able to strafe when searching for cover
	if(m_bCanUseGunTask)
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetIsGun())
		{
			// Aim At a position 
			CTaskGun* pGunTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(vTargetPosition));
			pGunTask->SetFiringPatternHash(FIRING_PATTERN_BURST_FIRE);
			pCoverTask->SetSubtaskToUseWhilstSearching(pGunTask);
			iFlags |= (CTaskCover::CF_RunSubtaskWhenStationary|CTaskCover::CF_RunSubtaskWhenMoving);
		}
	}

	pCoverTask->SetSearchPosition(vCoverSearchStartPosition);
	pCoverTask->SetSearchFlags(iFlags);

	SetNewTask(pCoverTask);
	return FSM_Continue;
}

CTask::FSM_Return CTaskStayInCover::SearchForCoverInArea_OnUpdate( CPed* pPed )
{
	// Need to keep any cover point while in this state
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

	CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
	if (pIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) && !m_bIsInDefensiveArea && m_goToAreaTimer.IsFinished())
	{
		SetState(State_GoToDefensiveArea);
	}

	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
		if( pCoverPoint )
		{
			// Calculate the aim at positions
			Vector3 vTargetPosition;
			Vector3 vCoverSearchStartPosition;
			CalculateAimAtPosition(pPed, vTargetPosition, vCoverSearchStartPosition);

			if( !pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() ||
				pPed->GetPedIntelligence()->GetDefensiveArea()->GetDefaultCoverFromPosition().IsZero() ||
				CCover::DoesCoverPointProvideCover(pCoverPoint, pCoverPoint->GetArc(), vTargetPosition ) )
			{
				SetState(State_InCoverInArea);
			}
			else
			{
				SetState(State_StandAndAimInArea);
			}

			return FSM_Continue;
		}
		else
		{
			SetState(State_StandAndAimInArea);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskStayInCover::InCoverInArea_OnEnter( CPed* pPed )
{
	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() && !pPed->GetPedIntelligence()->GetDefensiveArea()->GetDefaultCoverFromPosition().IsZero())
	{
		// Calculate the aim at positions
		Vector3 vTargetPosition;
		Vector3 vCoverSearchStartPosition;
		CalculateAimAtPosition(pPed, vTargetPosition, vCoverSearchStartPosition);

		SetNewTask(rage_new CTaskCover(CAITarget(vTargetPosition), CTaskCover::CF_SkipIdleCoverTransition | CTaskCover::CF_AimOnly));
	}
	else
	{
		SetNewTask(rage_new CTaskCover(CAITarget(), CTaskCover::CF_SkipIdleCoverTransition | CTaskCover::CF_AimOnly));
	}
}

CTask::FSM_Return CTaskStayInCover::InCoverInArea_OnUpdate(CPed* pPed)
{
	// Need to keep any cover point while in this state
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

	// Check if our subtask has finished
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetState(State_StandAndAimInArea);
		return FSM_Continue;
	}

	// Periodically check we are still within the correct area
	m_fCoverSearchTimer += fwTimer::GetTimeStep();
	if( m_fCoverSearchTimer > TIME_BETWEEN_COVER_CHECKS )
	{
		if( pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
			!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) )
		{
			SetState(State_SearchForCoverInArea);
		}
		else if( pPed->GetCoverPoint() == NULL )
		{
			SetState(State_SearchForCoverInArea);
		}

		m_fCoverSearchTimer = 0.0f;
		return FSM_Continue;
	}

	// Keep the peds target up to date if we based it off the defensive area cover from position
	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() && !pPed->GetPedIntelligence()->GetDefensiveArea()->GetDefaultCoverFromPosition().IsZero())
	{
		CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER));
		if(pInCoverTask)
		{
			// Calculate the aim at positions
			Vector3 vTargetPosition;
			Vector3 vCoverSearchStartPosition;
			CalculateAimAtPosition(pPed, vTargetPosition, vCoverSearchStartPosition);

			pInCoverTask->UpdateTarget(NULL, &vTargetPosition);
		}
	}

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskStayInCover::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StandAndAimInArea",
		"State_SearchForCoverInArea",
		"State_GoToDefensiveArea",
		"State_InCoverInArea",
		"State_Finish"
	};

	return aStateNames[iState];
}


#endif // !__FINAL
