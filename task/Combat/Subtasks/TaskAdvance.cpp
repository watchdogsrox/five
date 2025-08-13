// FILE :    TaskAdvance.h

// File header
#include "Task/Combat/Subtasks/TaskAdvance.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/CombatManager.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Combat/Cover/cover.h"
#include "task/Movement/TaskMoveToTacticalPoint.h"
#include "task/Movement/TaskSeekEntity.h"
#include "network\events\NetworkEventTypes.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskAdvance
//=========================================================================

CTaskAdvance::Tunables CTaskAdvance::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskAdvance, 0x155b6f93);

CTaskAdvance::CTaskAdvance(const CAITarget& rTarget, fwFlags8 uConfigFlags)
: m_Target(rTarget)
, m_pSubTaskToUseDuringMovement(NULL)
, m_fTimeSinceLastSeekCheck(0.0f)
, m_uConfigFlags(uConfigFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_ADVANCE);
}

CTaskAdvance::~CTaskAdvance()
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();
}

void CTaskAdvance::ClearSubTaskToUseDuringMovement()
{
	//Free the task.
	CTask* pSubTaskToUseDuringMovement = m_pSubTaskToUseDuringMovement;
	delete pSubTaskToUseDuringMovement;

	//Clear the task.
	m_pSubTaskToUseDuringMovement = NULL;
}

const CPed* CTaskAdvance::GetTargetPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	const CPed* pPed = static_cast<const CPed *>(pEntity);
	return pPed;
}

bool CTaskAdvance::IsMovingToPoint() const
{
	//Check the state.
	switch(GetState())
	{
		case State_MoveToTacticalPoint:
		{
			//Ensure the sub-task is valid.
			const CTask* pSubTask = GetSubTask();
			if(!pSubTask)
			{
				break;
			}

			//Ensure the sub-task is the correct type.
			if(pSubTask->GetTaskType() != CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT)
			{
				break;
			}

			const CTaskMoveToTacticalPoint* pTask = static_cast<const CTaskMoveToTacticalPoint *>(pSubTask);
			return pTask->IsMovingToPoint();
		}
		default:
		{
			break;
		}
	}

	return false;
}

void CTaskAdvance::SetSubTaskToUseDuringMovement(CTask* pTask)
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();

	//Set the sub-task to use during movement.
	m_pSubTaskToUseDuringMovement = pTask;
}

float CTaskAdvance::ScoreCoverPoint(const CTaskMoveToTacticalPoint::ScoreCoverPointInput& rInput)
{
	//Call the default version.
	float fScore = CTaskMoveToTacticalPoint::DefaultScoreCoverPoint(rInput);

	//Grab the cover point position.
	Vec3V vPosition;
	rInput.m_pPoint->GetCoverPoint()->GetCoverPointPosition(RC_VECTOR3(vPosition));

	//Score the position.
	fScore *= ScorePosition(rInput.m_pTask, vPosition);

	return fScore;
}

float CTaskAdvance::ScoreNavMeshPoint(const CTaskMoveToTacticalPoint::ScoreNavMeshPointInput& rInput)
{
	//Call the default version.
	float fScore = CTaskMoveToTacticalPoint::DefaultScoreNavMeshPoint(rInput);

	//Score the position.
	fScore *= ScorePosition(rInput.m_pTask, rInput.m_pPoint->GetPosition());

	return fScore;
}

float CTaskAdvance::ScorePosition(const CTaskMoveToTacticalPoint* pTask, Vec3V_In vPosition)
{
	//Initialize the score.
	float fScore = 1.0f;

	//Grab the task.
	taskAssert(pTask->GetParent()->GetTaskType() == CTaskTypes::TASK_ADVANCE);
	const CTaskAdvance* pTaskAdvance = static_cast<const CTaskAdvance *>(pTask->GetParent());

	//Check if we are frustrated.
	if(pTaskAdvance->m_uConfigFlags.IsFlagSet(CF_IsFrustrated))
	{
		//Check if the target has a cover point, and is in cover.
		const CPed* pTargetPed = pTaskAdvance->GetTargetPed();
		CCoverPoint* pTargetCoverPoint = pTargetPed ? pTargetPed->GetCoverPoint() : NULL;
		if( pTargetCoverPoint &&
			pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true) )
		{
			//Check if the cover point provides cover against the position.
			if(CCover::DoesCoverPointProvideCover(pTargetCoverPoint, pTargetCoverPoint->GetArc(), VEC3V_TO_VECTOR3(vPosition)))
			{
				//Apply a significant penalty.  We are frustrated, and want a clear LOS.
				static float s_fPenalty = 0.05f;
				fScore *= s_fPenalty;
			}
		}
	}

	return fScore;
}

#if !__FINAL
void CTaskAdvance::Debug() const
{
	CTask::Debug();
}

const char* CTaskAdvance::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_MoveToTacticalPoint",
		"State_Seek",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskAdvance::CanSeek() const
{
	//Ensure the target is not outside our defensive area.
	if(GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
		!GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(GetTargetPed()->GetTransform().GetPosition())))
	{
		return false;
	}

	//Check if we are frustrated.
	if(m_uConfigFlags.IsFlagSet(CF_IsFrustrated))
	{
		return true;
	}

	//Check if the tactical analysis is not active.
	const CPed* pTarget = GetTargetPed();
	if(!CTacticalAnalysis::IsActive(*pTarget))
	{
		return true;
	}

	//Check if we have no combat director.
	const CCombatDirector* pCombatDirector = GetPed()->GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return true;
	}

	//Check if we have not disabled seek due to
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableSeekDueToLineOfSight))
	{
		//Check if we have not exceeded the max seekers.
		static const int s_iMaxSeekers = 3;
		int iSeekers = pCombatDirector->CountPedsAdvancingWithSeek(*pTarget, s_iMaxSeekers);
		if(iSeekers < s_iMaxSeekers)
		{
			//Check if we have not exceeded the desired seekers.
			int iNumPedsWithClearLineOfSightOnFoot = CCombatManager::GetCombatTaskManager()->CountPedsWithClearLosToTarget(
				*GetTargetPed(), CCombatTaskManager::CPWCLTTF_MustBeOnFoot);
			int iDesiredSeekers = s_iMaxSeekers - iNumPedsWithClearLineOfSightOnFoot;
			if(iSeekers < iDesiredSeekers)
			{
				return true;
			}
		}
	}

	return false;
}

int CTaskAdvance::ChooseStateToResumeTo(bool& bKeepSubTask) const
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
			case CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT:
			{
				return State_MoveToTacticalPoint;
			}
			default:
			{
				break;
			}
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Start;
}

CTask* CTaskAdvance::CreateSubTaskToUseDuringMovement() const
{
	//Ensure the task is valid.
	CTask* pSubTaskToUseDuringMovement = m_pSubTaskToUseDuringMovement;
	if(!pSubTaskToUseDuringMovement)
	{
		return NULL;
	}

	return static_cast<CTask *>(pSubTaskToUseDuringMovement->Copy());
}

aiTask* CTaskAdvance::Copy() const
{
	//Create the task.
	CTaskAdvance* pTask = rage_new CTaskAdvance(m_Target, m_uConfigFlags);

	//Set the sub-task to use during movement.
	pTask->SetSubTaskToUseDuringMovement(CreateSubTaskToUseDuringMovement());

	return pTask;
}

CTask::FSM_Return CTaskAdvance::ProcessPreFSM()
{
	//Ensure the target ped is valid.
	if(!GetTargetPed())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskAdvance::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_MoveToTacticalPoint)
			FSM_OnEnter
				MoveToTacticalPoint_OnEnter();
			FSM_OnUpdate
				return MoveToTacticalPoint_OnUpdate();

		FSM_State(State_Seek)
			FSM_OnEnter
				Seek_OnEnter();
			FSM_OnUpdate
				return Seek_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskAdvance::Start_OnUpdate()
{
	//Check if a tactical analysis is active for the target.
	if(CTacticalAnalysis::IsActive(*GetTargetPed()))
	{
		//Move to a tactical point.
		SetState(State_MoveToTacticalPoint);
	}
	//Check if we can seek.
	else if(CanSeek())
	{
		//Seek the entity.
		SetState(State_Seek);
	}
	else
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskAdvance::Resume_OnUpdate()
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

void CTaskAdvance::MoveToTacticalPoint_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT))
	{
		return;
	}

	//Clear the time since the last seek check.
	m_fTimeSinceLastSeekCheck = 0.0f;

	//Create the task.
	CTaskMoveToTacticalPoint* pTask = rage_new CTaskMoveToTacticalPoint(m_Target, CTaskMoveToTacticalPoint::CF_DontQuitWhenPointIsReached);

	//Set the callbacks.
	pTask->SetScoreCoverPointCallback(CTaskAdvance::ScoreCoverPoint);
	pTask->SetScoreNavMeshPointCallback(CTaskAdvance::ScoreNavMeshPoint);

	//Set the time to wait at the position.
	pTask->SetTimeToWaitAtPosition(sm_Tunables.m_TimeToWaitAtPosition);

	//Set the time between point updates.
	pTask->SetTimeBetweenPointUpdates(sm_Tunables.m_TimeBetweenPointUpdates);

	//Set the sub-task to use during movement.
	pTask->SetSubTaskToUseDuringMovement(CreateSubTaskToUseDuringMovement());

	// Check if we should require points with LOS and set the flag if this is the case
	const CPed* pPed = GetPed();
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseDefaultBlockedLosPositionAndDirection))
	{
		pTask->GetConfigFlags().SetFlag(CTaskMoveToTacticalPoint::CF_DisablePointsWithoutClearLos);
	}

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskAdvance::MoveToTacticalPoint_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we can seek.
		if(CanSeek())
		{
			//Move to the seek state.
			SetState(State_Seek);
		}
		else
		{
			//Finish the task.
			SetState(State_Finish);
		}
	}
	else
	{
		//Check if we are at the tactical point.
		CTaskMoveToTacticalPoint* pTask = static_cast<CTaskMoveToTacticalPoint *>(GetSubTask());
		if(!pTask->IsMovingToPoint() || !pTask->DoesPointWeAreMovingToHaveClearLineOfSight())
		{
			//Check if the time has expired.
			m_fTimeSinceLastSeekCheck += GetTimeStep();
			if(m_fTimeSinceLastSeekCheck > sm_Tunables.m_TimeBetweenSeekChecksAtTacticalPoint)
			{
				//Clear the timer.
				m_fTimeSinceLastSeekCheck = 0.0f;

				//Check if we can seek.
				if(CanSeek())
				{
					//Move to the seek state.
					SetState(State_Seek);
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskAdvance::Seek_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Grab the target ped.
	const CPed* pTargetPed = GetTargetPed();

	// Create the seek task, allow the ped to navigate 
	TTaskMoveSeekEntityLastNavMeshIntersection* pSeekTask = rage_new TTaskMoveSeekEntityLastNavMeshIntersection(pTargetPed);
	if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ) )
		pSeekTask->SetAllowNavigationUnderwater(true);

	// If we can only melee and are supposed to never lose our target then we don't want to pass the initial "is close enough" check in seek entity
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse() == CCombatData::TLR_NeverLoseTarget && 
		CCombatBehaviour::GetCombatType(pPed) == CCombatBehaviour::CT_OnFootUnarmed)
	{
		pSeekTask->SetTargetRadius(0.0f);
	}

	//Never enter water.
	pSeekTask->SetNeverEnterWater(true);

	//Quit the task if a route is not found.
	pSeekTask->SetQuitTaskIfRouteNotFound(true);

	//Create the sub-task.
	CTask* pSubTask = CreateSubTaskToUseDuringMovement();
	
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pSeekTask, pSubTask);
	pTask->SetAllowClimbLadderSubtask(true);
	SetNewTask(pTask);

	// Test for nearby friendlies
	static dev_s32 siMaxNumPedsToTest = 8;
	int iTestedPeds = 0;

	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt && iTestedPeds < siMaxNumPedsToTest; pEnt = entityList.GetNext(), iTestedPeds++)
	{
		const CPed * pNearbyPed = static_cast<const CPed*>(pEnt);
		if (pNearbyPed->GetPedIntelligence()->IsFriendlyWith(*pPed))	// It's friendly to this ped
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				pPed->NewSay("COVER_ME");
			}
			else
			{
				pPed->NewSay("MOVE_IN");
			}

			break;
		}
	}
}

CTask::FSM_Return CTaskAdvance::Seek_OnUpdate()
{
	//Check if the target is outside the defensive area.
	const CPed* pPed = GetPed();
	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
		!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(GetTargetPed()->GetTransform().GetPosition())))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
