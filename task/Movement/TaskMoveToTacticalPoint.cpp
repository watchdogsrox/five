// FILE :    TaskMoveToTacticalPoint.h

// File header
#include "task/Movement/TaskMoveToTacticalPoint.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/Cover/TaskCover.h"
#include "task/Movement/TaskMoveWithinAttackWindow.h"
#include "task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskMoveToTacticalPoint
//=========================================================================

CTaskMoveToTacticalPoint::Tunables CTaskMoveToTacticalPoint::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskMoveToTacticalPoint, 0x58009afe);

CTaskMoveToTacticalPoint::CTaskMoveToTacticalPoint(const CAITarget& rTarget, fwFlags16 uConfigFlags)
: m_Target(rTarget)
, m_pSubTaskToUseDuringMovement(NULL)
, m_pCanScoreCoverPointCallback(CTaskMoveToTacticalPoint::DefaultCanScoreCoverPoint)
, m_pCanScoreNavMeshPointCallback(CTaskMoveToTacticalPoint::DefaultCanScoreNavMeshPoint)
, m_pScoreCoverPointCallback(CTaskMoveToTacticalPoint::DefaultScoreCoverPoint)
, m_pScoreNavMeshPointCallback(CTaskMoveToTacticalPoint::DefaultScoreNavMeshPoint)
, m_pIsCoverPointValidToMoveToCallback(CTaskMoveToTacticalPoint::DefaultIsCoverPointValidToMoveTo)
, m_pIsNavMeshPointValidToMoveToCallback(CTaskMoveToTacticalPoint::DefaultIsNavMeshPointValidToMoveTo)
, m_pSpheresOfInfluenceBuilder(CTaskMoveToTacticalPoint::DefaultSpheresOfInfluenceBuilder)
, m_fTimeSinceLastPointUpdate(0.0f)
, m_fTimeToWaitAtPosition(-1.0f)
, m_fTimeBetweenPointUpdates(-1.0f)
, m_fTimeToWaitBeforeFindingPointToMoveTo(-1.0f)
, m_fInfluenceSphereCheckTimer(0.0f)
, m_iMaxPositionsToExclude(3)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT);
}

CTaskMoveToTacticalPoint::~CTaskMoveToTacticalPoint()
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();
}

void CTaskMoveToTacticalPoint::ClearSubTaskToUseDuringMovement()
{
	//Free the task.
	CTask* pSubTaskToUseDuringMovement = m_pSubTaskToUseDuringMovement;
	delete pSubTaskToUseDuringMovement;

	//Clear the task.
	m_pSubTaskToUseDuringMovement = NULL;
}

bool CTaskMoveToTacticalPoint::DoesPointWeAreMovingToHaveClearLineOfSight() const
{
	//Ensure we are moving to a point.
	if(!taskVerifyf(IsMovingToPoint(), "The ped is not moving to a point."))
	{
		return false;
	}

	//Check if we are moving to a cover point.
	const CTacticalAnalysisCoverPoint* pCoverPoint = GetCoverPointToMoveTo();
	if(pCoverPoint)
	{
		return (pCoverPoint->IsLineOfSightClear());
	}

	//Check if we are moving to a nav mesh point.
	const CTacticalAnalysisNavMeshPoint* pNavMeshPoint = GetNavMeshPointToMoveTo();
	if(pNavMeshPoint)
	{
		return (pNavMeshPoint->IsLineOfSightClear());
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsAtPoint() const
{
	//Check the state.
	switch(GetState())
	{
		case State_WaitAtPosition:
		{
			return true;
		}
		case State_UseCover:
		{
			//Check if the sub-task is valid.
			const CTask* pSubTask = GetSubTask();
			if(pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_COVER))
			{
				//Check the state.
				const CTaskCover* pTaskCover = static_cast<const CTaskCover *>(pSubTask);
				switch(pTaskCover->GetState())
				{
					case CTaskCover::State_UseCover:
					{
						return true;
					}
					default:
					{
						break;
					}
				}

				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskMoveToTacticalPoint::IsMovingToPoint() const
{
	//Check the state.
	switch(GetState())
	{
		case State_MoveToPosition:
		{
			return (IsFollowingNavMeshRoute());
		}
		case State_UseCover:
		{
			//Check if the sub-task is valid.
			const CTask* pSubTask = GetSubTask();
			if(pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_COVER))
			{
				//Check the state.
				const CTaskCover* pTaskCover = static_cast<const CTaskCover *>(pSubTask);
				switch(pTaskCover->GetState())
				{
					case CTaskCover::State_MoveToCover:
					{
						return (IsFollowingNavMeshRoute());
					}
					case CTaskCover::State_EnterCover:
					{
						return true;
					}
					default:
					{
						break;
					}
				}

				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

void CTaskMoveToTacticalPoint::SetSubTaskToUseDuringMovement(CTask* pTask)
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();

	//Set the sub-task to use during movement.
	m_pSubTaskToUseDuringMovement = pTask;
}

bool CTaskMoveToTacticalPoint::DefaultCanScoreCoverPoint(const CanScoreCoverPointInput& UNUSED_PARAM(rInput))
{
	return true;
}

bool CTaskMoveToTacticalPoint::DefaultCanScoreNavMeshPoint(const CanScoreNavMeshPointInput& UNUSED_PARAM(rInput))
{
	return true;
}

bool CTaskMoveToTacticalPoint::DefaultIsCoverPointValidToMoveTo(const IsCoverPointValidToMoveToInput& UNUSED_PARAM(rInput))
{
	return true;
}

bool CTaskMoveToTacticalPoint::DefaultIsNavMeshPointValidToMoveTo(const IsNavMeshPointValidToMoveToInput& UNUSED_PARAM(rInput))
{
	return true;
}

float CTaskMoveToTacticalPoint::DefaultScoreCoverPoint(const ScoreCoverPointInput& rInput)
{
	//Grab the point.
	const CTacticalAnalysisCoverPoint* pPoint = rInput.m_pPoint;

	//Grab the cover point.
	CCoverPoint* pCoverPoint = pPoint->GetCoverPoint();

	//Grab the cover point position.
	Vec3V vPosition;
	pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vPosition));

	//Score the position.
	ScorePositionInput input;
	input.m_pTask = rInput.m_pTask;
	input.m_vPosition = vPosition;
	float fScore = DefaultScorePosition(input);

	//Apply the base multiplier.
	fScore *= sm_Tunables.m_Scoring.m_CoverPoint.m_Base;

	//Check if we are already moving to the cover point.
	if(rInput.m_pTask->IsMovingToCoverPoint(pCoverPoint))
	{
		//Apply the current bonus.
		fScore *= sm_Tunables.m_Scoring.m_CoverPoint.m_Bonus.m_Current;
	}

	//Check if the arc is invalid.
	if(!pPoint->DoesArcProvideCover())
	{
		//Apply the arc penalty.
		fScore *= sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_Arc;
	}

	//Check if the line of sight is invalid.
	if(!pPoint->IsLineOfSightClear())
	{
		//Apply the line of sight penalty.
		fScore *= sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_LineOfSight;
	}

	//Check if a ped is nearby (that isn't us).
	if(pPoint->HasNearby() && !pPoint->IsNearby(rInput.m_pTask->GetPed()))
	{
		//Apply the nearby penalty.
		fScore *= sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_Nearby;
	}

	//Check if the bad route value has exceeded the minimum.
	float fBadRouteValue = pPoint->GetBadRouteValue();
	if(fBadRouteValue >= sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_BadRoute.m_ValueForMin)
	{
		//Apply the penalty.
		float fValueForMin = sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_BadRoute.m_ValueForMin;
		float fValueForMax = sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_BadRoute.m_ValueForMax;
		float fLerp = Clamp(fBadRouteValue, fValueForMin, fValueForMax);
		fLerp /= (fValueForMax - fValueForMin);
		float fMinPenalty = sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_BadRoute.m_Min;
		float fMaxPenalty = sm_Tunables.m_Scoring.m_CoverPoint.m_Penalty.m_BadRoute.m_Max;
		float fPenalty = Lerp(fLerp, fMinPenalty, fMaxPenalty);
		fScore *= fPenalty;
	}

	return fScore;
}

float CTaskMoveToTacticalPoint::DefaultScoreNavMeshPoint(const ScoreNavMeshPointInput& rInput)
{
	//Grab the point.
	const CTacticalAnalysisNavMeshPoint* pPoint = rInput.m_pPoint;

	//Score the position.
	ScorePositionInput input;
	input.m_pTask = rInput.m_pTask;
	input.m_vPosition = pPoint->GetPosition();
	float fScore = DefaultScorePosition(input);

	//Apply the base multiplier.
	fScore *= sm_Tunables.m_Scoring.m_NavMeshPoint.m_Base;

	//Check if we are already moving to the nav mesh point.
	if(rInput.m_pTask->IsMovingToNavMeshPoint(pPoint->GetHandle()))
	{
		//Apply the current bonus.
		fScore *= sm_Tunables.m_Scoring.m_NavMeshPoint.m_Bonus.m_Current;
	}

	//Check if the line of sight is invalid.
	if(!pPoint->IsLineOfSightClear())
	{
		//Apply the line of sight penalty.
		fScore *= sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_LineOfSight;
	}

	//Check if a ped is nearby (that isn't us).
	if(pPoint->HasNearby() && !pPoint->IsNearby(rInput.m_pTask->GetPed()))
	{
		//Apply the nearby penalty.
		fScore *= sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_Nearby;
	}

	//Check if the bad route value has exceeded the minimum.
	float fBadRouteValue = pPoint->GetBadRouteValue();
	if(fBadRouteValue >= sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_BadRoute.m_ValueForMin)
	{
		//Apply the penalty.
		float fValueForMin = sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_BadRoute.m_ValueForMin;
		float fValueForMax = sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_BadRoute.m_ValueForMax;
		float fLerp = Clamp(fBadRouteValue, fValueForMin, fValueForMax);
		fLerp /= (fValueForMax - fValueForMin);
		float fMinPenalty = sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_BadRoute.m_Min;
		float fMaxPenalty = sm_Tunables.m_Scoring.m_NavMeshPoint.m_Penalty.m_BadRoute.m_Max;
		float fPenalty = Lerp(fLerp, fMinPenalty, fMaxPenalty);
		fScore *= fPenalty;
	}

	return fScore;
}

float CTaskMoveToTacticalPoint::DefaultScorePosition(const ScorePositionInput& rInput)
{
	//Initialize the score.
	float fScore = 1.0f;

	//Grab the ped.
	const CPed* pPed = rInput.m_pTask->GetPed();

	//Calculate the distance from the ped.
	float fDistanceFromPed = DistXYFast(pPed->GetTransform().GetPosition(), rInput.m_vPosition).Getf();

	//Normalize the value.
	float fMaxDistanceFromPed = sm_Tunables.m_Scoring.m_Position.m_MaxDistanceFromPed;
	fDistanceFromPed = Clamp(fDistanceFromPed, 0.0f, fMaxDistanceFromPed);
	fDistanceFromPed /= fMaxDistanceFromPed;

	//Apply the score for the ped.
	float fMaxScoreForPed = sm_Tunables.m_Scoring.m_Position.m_ValueForMinDistanceFromPed;
	float fMinScoreForPed = sm_Tunables.m_Scoring.m_Position.m_ValueForMaxDistanceFromPed;
	float fScoreForPed = Lerp(fDistanceFromPed, fMaxScoreForPed, fMinScoreForPed);
	fScore *= fScoreForPed;

	//Calculate the distance from the target.
	float fDistanceFromTarget = DistXYFast(rInput.m_pTask->GetTargetPed()->GetTransform().GetPosition(), rInput.m_vPosition).Getf();

	//Calculate the distance from the optimal.
	float fOptimalDistanceFromTarget = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatOptimalCoverDistance);
	float fDistanceFromOptimal = Abs(fDistanceFromTarget - fOptimalDistanceFromTarget);

	//Normalize the value.
	float fMaxDistanceFromOptimal = sm_Tunables.m_Scoring.m_Position.m_MaxDistanceFromOptimal;
	fDistanceFromOptimal = Clamp(fDistanceFromOptimal, 0.0f, fMaxDistanceFromOptimal);
	fDistanceFromOptimal /= fMaxDistanceFromOptimal;

	//Apply the score for the optimal.
	float fMaxScoreForOptimal = sm_Tunables.m_Scoring.m_Position.m_ValueForMinDistanceFromOptimal;
	float fMinScoreForOptimal = sm_Tunables.m_Scoring.m_Position.m_ValueForMaxDistanceFromOptimal;
	float fScoreForOptimal = Lerp(fDistanceFromOptimal, fMaxScoreForOptimal, fMinScoreForOptimal);
	fScore *= fScoreForOptimal;

	return fScore;
}

bool CTaskMoveToTacticalPoint::DefaultSpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed)
{
	return CTaskCombat::SpheresOfInfluenceBuilder(apSpheres, iNumSpheres, pPed);
}

#if !__FINAL
void CTaskMoveToTacticalPoint::Debug() const
{
	CTask::Debug();
}

const char* CTaskMoveToTacticalPoint::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_FindPointToMoveTo",
		"State_UseCover",
		"State_MoveToPosition",
		"State_WaitAtPosition",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskMoveToTacticalPoint::AbortNavMeshTask()
{
	//Check if the task is valid.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask)
	{
		//Abort the task.
		if(!pTask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
		{
			return false;
		}
	}

	return true;
}

void CTaskMoveToTacticalPoint::ActivateTimeslicing()
{
	TUNE_GROUP_BOOL(MOVE_TO_TACTICAL_POINT, ACTIVATE_TIMESLICING, true);
	if(ACTIVATE_TIMESLICING)
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

bool CTaskMoveToTacticalPoint::CanNeverScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//There are some conditions under which cover points can NEVER be scored, for any reason.
	//Those conditions are enforced here.

	//Check if the cover point is invalid to move to.
	if(!IsCoverPointValidToMoveTo(rPoint))
	{
		return true;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the cover point is not reserved by the ped, and can't be reserved by the ped.
	if(!rPoint.IsReservedBy(pPed) && !rPoint.CanReserve(pPed))
	{
		return true;
	}

	//Check if we have disabled points without a clear line of sight, and the line of sight is not clear.
	if(DisablePointsWithoutClearLos() && !rPoint.IsLineOfSightClear())
	{
		return true;
	}

	//Grab the cover point position.
	Vec3V vCoverPointPosition;
	rPoint.GetCoverPoint()->GetCoverPointPosition(RC_VECTOR3(vCoverPointPosition));

	//Check if the position is excluded.
	if(IsPositionExcluded(vCoverPointPosition))
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::CanNeverScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const
{
	//There are some conditions under which nav mesh points can NEVER be scored, for any reason.
	//Those conditions are enforced here.

	//Check if the nav mesh point is invalid to move to.
	if(!IsNavMeshPointValidToMoveTo(rPoint))
	{
		return true;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the nav mesh point is not reserved by the ped, and can't be reserved by the ped.
	if(!rPoint.IsReservedBy(pPed) && !rPoint.CanReserve(pPed))
	{
		return true;
	}

	//Check if we have disabled points without a clear line of sight, and the line of sight is not clear.
	if(DisablePointsWithoutClearLos() && !rPoint.IsLineOfSightClear())
	{
		return true;
	}

	//Check if the position is excluded.
	if(IsPositionExcluded(rPoint.GetPosition()))
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::CanScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//Ensure we can score the cover point.
	if(CanNeverScoreCoverPoint(rPoint))
	{
		return false;
	}

	//Check if we have a callback for checking if the cover point can be scored.
	if(m_pCanScoreCoverPointCallback)
	{
		//Ensure we can score the cover point.
		CanScoreCoverPointInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		if(!(*m_pCanScoreCoverPointCallback)(input))
		{
			return false;
		}
	}

	return true;
}

bool CTaskMoveToTacticalPoint::CanScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const
{
	//Ensure we can score the nav mesh point.
	if(CanNeverScoreNavMeshPoint(rPoint))
	{
		return false;
	}

	//Check if we have a callback for checking if the nav mesh point can be scored.
	if(m_pCanScoreNavMeshPointCallback)
	{
		//Ensure we can score the nav mesh point.
		CanScoreNavMeshPointInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		if(!(*m_pCanScoreNavMeshPointCallback)(input))
		{
			return false;
		}
	}

	return true;
}

bool CTaskMoveToTacticalPoint::CanUseCover() const
{
	//Ensure the ped can use cover.
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover))
	{
		return false;
	}

	return true;
}

int CTaskMoveToTacticalPoint::ChooseStateToMoveToPoint() const
{
	//Check if the cover point is valid.
	if(m_PointToMoveTo.m_pCoverPoint)
	{
		return State_UseCover;
	}
	//Check if the nav mesh point is valid.
	else if(m_PointToMoveTo.m_iHandleForNavMeshPoint >= 0)
	{
		return State_MoveToPosition;
	}

	return State_Finish;
}

int CTaskMoveToTacticalPoint::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Keep the sub-task.
	bKeepSubTask = true;

	//Check the previous state.
	int iPreviousState = GetPreviousState();
	switch(iPreviousState)
	{
		case State_UseCover:
		{
			//Ensure the cover point is valid.
			const CCoverPoint* pCoverPoint = m_PointToMoveTo.m_pCoverPoint;
			if(!pCoverPoint)
			{
				break;
			}
			else if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
			{
				break;
			}

			//Ensure someone else is not using the cover point.
			const CPed* pOccupiedBy = pCoverPoint->GetOccupiedBy();
			if(pOccupiedBy && (pOccupiedBy != GetPed()))
			{
				break;
			}

			//Do not keep the sub-task.
			bKeepSubTask = false;

			return iPreviousState;
		}
		case State_MoveToPosition:
		case State_WaitAtPosition:
		{
			return iPreviousState;
		}
		default:
		{
			break;
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_FindPointToMoveTo;
}

void CTaskMoveToTacticalPoint::ClearTimeUntilReleaseForCoverPoint()
{
	//Ensure the point is valid.
	CTacticalAnalysisCoverPoint* pPoint = GetPointForCoverPoint(m_PointToMoveTo.m_pCoverPoint);
	if(!pPoint)
	{
		return;
	}

	//Clear the time until release.
	pPoint->ClearTimeUntilRelease();
}

void CTaskMoveToTacticalPoint::ClearTimeUntilReleaseForNavMeshPoint()
{
	//Ensure the point is valid.
	CTacticalAnalysisNavMeshPoint* pPoint = GetPointForNavMeshPoint(m_PointToMoveTo.m_iHandleForNavMeshPoint);
	if(!pPoint)
	{
		return;
	}

	//Clear the time until release.
	pPoint->ClearTimeUntilRelease();
}

void CTaskMoveToTacticalPoint::ClearTimeUntilReleaseForPoint()
{
	//Clear the time until release for the cover point.
	ClearTimeUntilReleaseForCoverPoint();

	//Clear the time until release for the nav mesh point.
	ClearTimeUntilReleaseForNavMeshPoint();

	//Note that we have cleared the time until release for the point.
	m_uRunningFlags.ClearFlag(RF_HasSetTimeUntilReleaseForPoint);
}

CTask* CTaskMoveToTacticalPoint::CreateSubTaskToUseDuringMovement() const
{
	//Ensure the task is valid.
	CTask* pSubTaskToUseDuringMovement = m_pSubTaskToUseDuringMovement;
	if(!pSubTaskToUseDuringMovement)
	{
		return NULL;
	}

	return static_cast<CTask *>(pSubTaskToUseDuringMovement->Copy());
}

bool CTaskMoveToTacticalPoint::DisablePointsWithoutClearLos() const
{
	//Check if the flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_DisablePointsWithoutClearLos))
	{
		return true;
	}

	//Check if the flag is set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableTacticalPointsWithoutClearLos))
	{
		return true;
	}

	return false;
}

void CTaskMoveToTacticalPoint::ExcludePointToMoveTo()
{
	//Ensure the excluded positions to move to are not full.
	if(m_aExcludedPositions.IsFull())
	{
		return;
	}

	//Ensure the position to move to is valid.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return;
	}

	//Exclude the position to move to.
	m_aExcludedPositions.Push(vPositionToMoveTo);
}

bool CTaskMoveToTacticalPoint::FindPointToMoveTo(Point& rPointToMoveTo) const
{
	//Grab the tactical analysis.
	const CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();

	//Find the best point.
	struct BestPoint
	{
		BestPoint()
		: m_iIndex(-1)
		, m_fScore(FLT_MIN)
		, m_bIsCoverPoint(false)
		{}

		int		m_iIndex;
		float	m_fScore;
		bool	m_bIsCoverPoint;
	};
	BestPoint bestPoint;

	//Grab the cover points.
	const CTacticalAnalysisCoverPoints& rCoverPoints = pTacticalAnalysis->GetCoverPoints();

	//Grab the nav mesh points.
	const CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();

	//Check if we should score the cover points.
	if(ShouldScoreCoverPoints())
	{
		//Iterate over the cover points.
		for(int i = 0; i < rCoverPoints.GetNumPoints(); ++i)
		{
			//Ensure the cover point is valid to start moving to.
			const CTacticalAnalysisCoverPoint& rPoint = rCoverPoints.GetPointForIndex(i);
			if(!CanScoreCoverPoint(rPoint))
			{
				continue;
			}

			//Score the cover point.
			float fScore = ScoreCoverPoint(rPoint);

			//Ensure the score is better.
			if((bestPoint.m_iIndex >= 0) &&
				(bestPoint.m_fScore >= fScore))
			{
				continue;
			}

			//Reject any negative scores if instructed to do so
			if(fScore < 0.0f && m_uConfigFlags.IsFlagSet(CF_RejectNegativeScores))
			{
				continue;
			}

			//Set the best point.
			bestPoint.m_iIndex			= i;
			bestPoint.m_fScore			= fScore;
			bestPoint.m_bIsCoverPoint	= true;
		}
	}

	//Check if we should score the nav mesh points.
	if(ShouldScoreNavMeshPoints())
	{
		//Iterate over the nav mesh points.
		for(int i = 0; i < rNavMeshPoints.GetNumPoints(); ++i)
		{
			//Ensure the point is valid to start moving to.
			const CTacticalAnalysisNavMeshPoint& rPoint = rNavMeshPoints.GetPointForIndex(i);
			if(!CanScoreNavMeshPoint(rPoint))
			{
				continue;
			}

			//Score the point.
			float fScore = ScoreNavMeshPoint(rPoint);

			//Ensure the score is better.  Lower scores win.
			if((bestPoint.m_iIndex >= 0) &&
				(bestPoint.m_fScore >= fScore))
			{
				continue;
			}

			//Reject any negative scores if instructed to do so
			if(fScore < 0.0f && m_uConfigFlags.IsFlagSet(CF_RejectNegativeScores))
			{
				continue;
			}

			//Set the best point.
			bestPoint.m_iIndex			= i;
			bestPoint.m_fScore			= fScore;
			bestPoint.m_bIsCoverPoint	= false;
		}
	}

	//Ensure the index is valid.
	int iIndex = bestPoint.m_iIndex;
	if(iIndex < 0)
	{
		return false;
	}

	//Check if the best point is a cover point.
	if(bestPoint.m_bIsCoverPoint)
	{
		//Set the cover point.
		rPointToMoveTo.m_pCoverPoint = rCoverPoints.GetPointForIndex(iIndex).GetCoverPoint();
	}
	else
	{
		//Set the handle for the nav mesh point.
		rPointToMoveTo.m_iHandleForNavMeshPoint = rNavMeshPoints.GetPointForIndex(iIndex).GetHandle();
	}

	return true;
}

bool CTaskMoveToTacticalPoint::GetPositionToMoveTo(Vec3V_InOut vPositionToMoveTo) const
{
	//Check if we can get the position to move to from the cover point.
	if(GetPositionToMoveToFromCoverPoint(vPositionToMoveTo))
	{
		return true;
	}
	//Check if we can get the position to move to from the nav mesh point.
	else if(GetPositionToMoveToFromNavMeshPoint(vPositionToMoveTo))
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::GetPositionToMoveToFromCoverPoint(Vec3V_InOut vPositionToMoveTo) const
{
	//Ensure the cover point is valid.
	CCoverPoint* pCoverPoint = m_PointToMoveTo.m_pCoverPoint;
	if(!pCoverPoint)
	{
		return false;
	}
	else if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	//Ensure the cover point position is valid.
	if(!pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vPositionToMoveTo)))
	{
		return false;
	}

	//Ensure the position to move to is not close to zero.
	if(!taskVerifyf(!IsCloseAll(vPositionToMoveTo, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)), "The cover point position is too close to zero."))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::GetPositionToMoveToFromNavMeshPoint(Vec3V_InOut vPositionToMoveTo) const
{
	//Ensure the point is valid.
	const CTacticalAnalysisNavMeshPoint* pPoint = GetPointForNavMeshPoint(m_PointToMoveTo.m_iHandleForNavMeshPoint);
	if(!pPoint)
	{
		return false;
	}

	//Ensure the point is valid.
	if(!pPoint->IsValid())
	{
		return false;
	}

	//Ensure the position to move to is not close to zero.
	vPositionToMoveTo = pPoint->GetPosition();
	if(!taskVerifyf(!IsCloseAll(vPositionToMoveTo, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)), "The nav mesh position is too close to zero."))
	{
		return false;
	}

	return true;
}

CTacticalAnalysis* CTaskMoveToTacticalPoint::GetTacticalAnalysis() const
{
	const CPed* pTargetPed = GetTargetPed();
	if (!pTargetPed)
	{
		return nullptr;
	}

	return CTacticalAnalysis::Find(*pTargetPed);
}

const CPed* CTaskMoveToTacticalPoint::GetTargetPed() const
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

bool CTaskMoveToTacticalPoint::HasExcludedTooManyPositions() const
{
	//Check if the excluded positions are full.
	if(m_aExcludedPositions.IsFull())
	{
		return true;
	}
	//Check if we have reached the maximum amount of excluded positions.
	else if(m_aExcludedPositions.GetCount() > m_iMaxPositionsToExclude)
	{
		return true;
	}

	return false;
}

CTaskMoveToTacticalPoint::RouteFailedReason CTaskMoveToTacticalPoint::HasRouteFailed()
{
	//Check if the task is valid.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask && !pTask->GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		//Check if we are unable to find a route.
		if(pTask->IsUnableToFindRoute())
		{
			return RFR_UnableToFind;
		}

		m_fInfluenceSphereCheckTimer -= GetTimeStep();

		//Check if we can't move through the spheres of influence.
		if( m_fInfluenceSphereCheckTimer <= 0.0f && !m_uConfigFlags.IsFlagSet(CF_CanMoveThroughSpheresOfInfluence) &&
			(!m_uConfigFlags.IsFlagSet(CF_OnlyCheckInfluenceSpheresOnce) || !m_uRunningFlags.IsFlagSet(RF_HasCheckedInfluenceSpheres)))
		{
			m_fInfluenceSphereCheckTimer = sm_Tunables.m_TimeBetweenInfluenceSphereChecks;

			//Create the spheres of influence.
			TInfluenceSphere aInfluenceSpheres[MAX_NUM_INFLUENCE_SPHERES];
			int iNumInfluenceSpheres = 0;
			if(m_pSpheresOfInfluenceBuilder && (*m_pSpheresOfInfluenceBuilder)(aInfluenceSpheres, iNumInfluenceSpheres, GetPed()) && (iNumInfluenceSpheres > 0))
			{
				//Convert the spheres.
				spdSphere aSpheres[MAX_NUM_INFLUENCE_SPHERES];
				for(int i = 0; i < iNumInfluenceSpheres; ++i)
				{
					aSpheres[i].Set(VECTOR3_TO_VEC3V(aInfluenceSpheres[i].GetOrigin()), ScalarVFromF32(aInfluenceSpheres[i].GetRadius()));
				}

				//Check if the route intersects with the spheres.
				if(pTask->TestSpheresIntersectingRoute(iNumInfluenceSpheres, aSpheres, true))
				{
					return RFR_TooCloseToTarget;
				}

				// Don't consider ourselves having had tested the spheres again the route if the route doesn't currently have a size
				if(pTask->GetRouteSize())
				{
					m_uRunningFlags.SetFlag(RF_HasCheckedInfluenceSpheres);
				}
			}
		}
	}

	return RFR_None;
}

bool CTaskMoveToTacticalPoint::IsCloseToPositionToMoveTo() const
{
	//Ensure the position to move to is valid.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return false;
	}
	Vec3V vPositionToMoveToXY = vPositionToMoveTo;
	vPositionToMoveToXY.SetZ(ScalarV(V_ZERO));

	//Grab the ped position.
	Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();
	Vec3V vPedPositionXY = vPedPosition;
	vPedPositionXY.SetZ(ScalarV(V_ZERO));

	//Ensure the distance does not exceed the maximum.
	ScalarV scDistSq = DistSquared(vPositionToMoveToXY, vPedPositionXY);
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceToConsiderCloseToPositionToMoveTo));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::IsCoverPointNeverValidToMoveTo(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//Grab the cover point.
	CCoverPoint* pCoverPoint = rPoint.GetCoverPoint();

	//Check if the cover point is invalid.
	if(!pCoverPoint)
	{
		return true;
	}
	else if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return true;
	}

	//Check if the cover point is thin.
	if(pCoverPoint->GetIsThin())
	{
		return true;
	}

	//Check if the cover point is dangerous.
	if(pCoverPoint->IsDangerous())
	{
		return true;
	}

	//Grab the cover point position.
	Vec3V vCoverPointPosition;
	pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vCoverPointPosition));

	//Check if the position is outside the attack window.
	const CPed* pPed = GetPed();
	if( !m_uConfigFlags.IsFlagSet(CF_DontCheckAttackWindow) &&
		CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pPed, GetTargetPed(), vCoverPointPosition, true) )
	{
		return true;
	}

	//Check if the position is outside the defensive area.
	if( !m_uConfigFlags.IsFlagSet(CF_DontCheckDefensiveArea) && 
		pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
		!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(vCoverPointPosition)) )
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsCoverPointValidToMoveTo(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//Check if the cover point is never valid to move to.
	if(IsCoverPointNeverValidToMoveTo(rPoint))
	{
		return false;
	}

	//Check if we have a callback for checking if the cover point is valid to move to.
	if(m_pIsCoverPointValidToMoveToCallback)
	{
		//Check if the cover point is not valid to move to.
		IsCoverPointValidToMoveToInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		if(!(*m_pIsCoverPointValidToMoveToCallback)(input))
		{
			return false;
		}
	}

	return true;
}

bool CTaskMoveToTacticalPoint::IsFollowingNavMeshRoute() const
{
	//Ensure the nav mesh task is valid.
	const CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(
		GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(!pTask)
	{
		return false;
	}

	//Check if we are moving whilst waiting for a path.
	if(pTask->IsMovingWhilstWaitingForPath())
	{
		return true;
	}

	//Check if we are following the route.
	if(pTask->IsFollowingNavMeshRoute())
	{
		return true;
	}

	//Check if we are using a ladder.
	if(pTask->IsUsingLadder())
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsMovingToCoverPoint() const
{
	return (m_PointToMoveTo.m_pCoverPoint != NULL);
}

bool CTaskMoveToTacticalPoint::IsMovingToCoverPoint(CCoverPoint* pCoverPoint) const
{
	return (m_PointToMoveTo.m_pCoverPoint == pCoverPoint);
}

bool CTaskMoveToTacticalPoint::IsMovingToInvalidCoverPoint() const
{
	//Check if the cover point is valid.
	CCoverPoint* pCoverPoint = m_PointToMoveTo.m_pCoverPoint;
	if(pCoverPoint)
	{
		//Check if the point is valid.
		const CTacticalAnalysisCoverPoint* pPoint = GetPointForCoverPoint(pCoverPoint);
		if(pPoint)
		{
			return !IsCoverPointValidToMoveTo(*pPoint);
		}
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsMovingToInvalidNavMeshPoint() const
{
	//Check if the point on the nav mesh is valid.
	int iHandle = m_PointToMoveTo.m_iHandleForNavMeshPoint;
	if(iHandle >= 0)
	{
		//Check if the point is valid.
		const CTacticalAnalysisNavMeshPoint* pPoint = GetPointForNavMeshPoint(iHandle);
		if(pPoint)
		{
			return !IsNavMeshPointValidToMoveTo(*pPoint);
		}
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsMovingToInvalidPosition() const
{
	//Check if we are moving to an invalid cover point.
	if(IsMovingToInvalidCoverPoint())
	{
		return true;
	}
	//Check if we are moving to an invalid point on the nav mesh.
	else if(IsMovingToInvalidNavMeshPoint())
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsMovingToNavMeshPoint() const
{
	return (m_PointToMoveTo.m_iHandleForNavMeshPoint >= 0);
}

bool CTaskMoveToTacticalPoint::IsMovingToNavMeshPoint(int iHandle) const
{
	return (m_PointToMoveTo.m_iHandleForNavMeshPoint == iHandle);
}

bool CTaskMoveToTacticalPoint::IsMovingToPoint(const Point& rPoint) const
{
	//Check if the cover point matches.
	CCoverPoint* pCoverPoint = rPoint.m_pCoverPoint;
	if(pCoverPoint && (pCoverPoint == m_PointToMoveTo.m_pCoverPoint))
	{
		return true;
	}

	//Check if the handle to the nav mesh point matches.
	int iHandle = rPoint.m_iHandleForNavMeshPoint;
	if((iHandle >= 0) && (iHandle == m_PointToMoveTo.m_iHandleForNavMeshPoint))
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsNavMeshPointNeverValidToMoveTo(const CTacticalAnalysisNavMeshPoint& rPoint) const
{
	//Check if the point is invalid.
	if(!rPoint.IsValid())
	{
		return true;
	}

	//Check if the position is outside the attack window.
	const CPed* pPed = GetPed();
	if(!m_uConfigFlags.IsFlagSet(CF_DontCheckAttackWindow) &&
		CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pPed, GetTargetPed(), rPoint.GetPosition(), false) )
	{
		return true;
	}

	//Check if the position is outside the defensive area.
	if( !m_uConfigFlags.IsFlagSet(CF_DontCheckDefensiveArea) && 
		pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
		!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(rPoint.GetPosition())) )
	{
		return true;
	}

	return false;
}

bool CTaskMoveToTacticalPoint::IsNavMeshPointValidToMoveTo(const CTacticalAnalysisNavMeshPoint& rPoint) const
{
	//Check if the nav mesh point is never valid to move to.
	if(IsNavMeshPointNeverValidToMoveTo(rPoint))
	{
		return false;
	}

	//Check if we have a callback for checking if the nav mesh point is valid to move to.
	if(m_pIsNavMeshPointValidToMoveToCallback)
	{
		//Check if the nav mesh point is not valid to move to.
		IsNavMeshPointValidToMoveToInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		if(!(*m_pIsNavMeshPointValidToMoveToCallback)(input))
		{
			return false;
		}
	}

	return true;
}

bool CTaskMoveToTacticalPoint::IsPositionExcluded(Vec3V_In vPosition) const
{
	//Iterate over the excluded positions.
	int iCount = m_aExcludedPositions.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Check if the position is close.
		ScalarV scDistSq = DistSquared(vPosition, m_aExcludedPositions[i]);
		ScalarV scMaxDistSq = ScalarV(V_ONE);
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			return true;
		}
	}

	return false;
}

void CTaskMoveToTacticalPoint::KeepCoverPoint()
{
	//Keep the cover point.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
}

void CTaskMoveToTacticalPoint::OnRouteFailed(RouteFailedReason nReason)
{
	//Check the reason.
	switch(nReason)
	{
		case RFR_UnableToFind:
		{
			OnUnableToFindRoute();
			break;
		}
		case RFR_TooCloseToTarget:
		{
			OnRouteTooCloseToTarget();
			break;
		}
		default:
		{
			taskAssertf(false, "The reason is invalid: %d.", nReason);
			break;
		}
	}
}

void CTaskMoveToTacticalPoint::OnRouteTooCloseToTarget()
{
	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return;
	}

	//Ensure the position to move to is valid.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return;
	}

	//Notify the tactical analysis.
	pTacticalAnalysis->OnRouteTooCloseTooTarget(vPositionToMoveTo);
}

void CTaskMoveToTacticalPoint::OnUnableToFindRoute()
{
	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return;
	}

	//Ensure the position to move to is valid.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return;
	}

	//Notify the tactical analysis.
	pTacticalAnalysis->OnUnableToFindRoute(vPositionToMoveTo);
}

void CTaskMoveToTacticalPoint::ProcessCoverPoint()
{
	//Check if we should keep our cover point.
	if(ShouldKeepCoverPoint())
	{
		//Keep the cover point.
		KeepCoverPoint();
	}
}

void CTaskMoveToTacticalPoint::ReleaseCoverPoint()
{
	//Ensure the cover point is valid.
	CCoverPoint* pCoverPoint = m_PointToMoveTo.m_pCoverPoint;
	if(!pCoverPoint)
	{
		return;
	}

	//Clear the cover point.
	m_PointToMoveTo.m_pCoverPoint = NULL;

	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return;
	}

	//Release the handle.
	CTacticalAnalysisCoverPoints& rCoverPoints = pTacticalAnalysis->GetCoverPoints();
	rCoverPoints.ReleaseCoverPoint(GetPed(), pCoverPoint);
}

void CTaskMoveToTacticalPoint::ReleaseNavMeshPoint()
{
	//Ensure the handle is valid.
	int iHandle = m_PointToMoveTo.m_iHandleForNavMeshPoint;
	if(iHandle < 0)
	{
		return;
	}

	//Clear the handle.
	m_PointToMoveTo.m_iHandleForNavMeshPoint = -1;

	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return;
	}

	//Release the handle.
	CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
	rNavMeshPoints.ReleaseHandle(GetPed(), iHandle);
}

void CTaskMoveToTacticalPoint::ReleasePoint()
{
	//Release the cover point.
	ReleaseCoverPoint();

	//Release the nav mesh point.
	ReleaseNavMeshPoint();
}

bool CTaskMoveToTacticalPoint::ReserveCoverPoint(CCoverPoint* pCoverPoint, bool bForce)
{
	//Assert that the cover point is valid.
	taskAssert(pCoverPoint);
	taskAssert(pCoverPoint->GetType() != CCoverPoint::COVTYPE_NONE);

	//Check if we are not forcing.
	if(!bForce)
	{
		//Check if we have the point.
		if(pCoverPoint == m_PointToMoveTo.m_pCoverPoint)
		{
			return true;
		}
	}

	//Release the point.
	ReleasePoint();

	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return false;
	}

	//Keep the cover point.
	KeepCoverPoint();

	//Grab the ped.
	CPed* pPed = GetPed();

	//Reserve the cover point.
	CTacticalAnalysisCoverPoints& rCoverPoints = pTacticalAnalysis->GetCoverPoints();
	if(!rCoverPoints.ReserveCoverPoint(pPed, pCoverPoint))
	{
		return false;
	}

	//Set the cover point.
	m_PointToMoveTo.m_pCoverPoint = pCoverPoint;

	return true;
}

bool CTaskMoveToTacticalPoint::ReserveNavMeshPoint(int iHandle)
{
	//Assert that the handle is valid.
	taskAssert(iHandle >= 0);

	//Check if we have the point.
	if(iHandle == m_PointToMoveTo.m_iHandleForNavMeshPoint)
	{
		return true;
	}

	//Release the point.
	ReleasePoint();

	//Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Reserve the handle.
	CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
	if(!rNavMeshPoints.ReserveHandle(pPed, iHandle))
	{
		return false;
	}

	//Set the handle.
	m_PointToMoveTo.m_iHandleForNavMeshPoint = iHandle;

	return true;
}

bool CTaskMoveToTacticalPoint::ReservePoint(Point& rPoint)
{
	//Release the point.
	ReleasePoint();

	//Check if the cover point is valid.
	CCoverPoint* pCoverPoint = rPoint.m_pCoverPoint;
	if(pCoverPoint && ReserveCoverPoint(pCoverPoint))
	{
		return true;
	}

	//Check if the handle for the nav mesh point is valid.
	int iHandle = rPoint.m_iHandleForNavMeshPoint;
	if((iHandle >= 0) && ReserveNavMeshPoint(iHandle))
	{
		return true;
	}

	return false;
}

float CTaskMoveToTacticalPoint::ScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//Check if we have a callback for scoring the cover point.
	if(m_pScoreCoverPointCallback)
	{
		//Score the cover point.
		ScoreCoverPointInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		return (*m_pScoreCoverPointCallback)(input);
	}

	return 1.0f;
}

float CTaskMoveToTacticalPoint::ScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const
{
	//Check if we have a callback for scoring the nav mesh point.
	if(m_pScoreNavMeshPointCallback)
	{
		//Score the cover point.
		ScoreNavMeshPointInput input;
		input.m_pTask = this;
		input.m_pPoint = &rPoint;
		return (*m_pScoreNavMeshPointCallback)(input);
	}

	return 1.0f;
}

void CTaskMoveToTacticalPoint::SetStateForFailedToMoveToPoint()
{
	//Exclude the point to move to.
	ExcludePointToMoveTo();

	//Wait a bit before finding a new point to move to.
	static float s_fTimeToWait = 0.25f;
	m_fTimeToWaitBeforeFindingPointToMoveTo = s_fTimeToWait;

	//Find a new point to move to.
	SetState(State_FindPointToMoveTo);
}

void CTaskMoveToTacticalPoint::SetStateToMoveToPoint()
{
	//Choose the state to move to point.
	int iState = ChooseStateToMoveToPoint();

	//Check if the state is the same.
	if(iState == GetState())
	{
		//Restart the state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	else
	{
		//Set the state.
		SetState(iState);
	}
}

void CTaskMoveToTacticalPoint::SetTimeUntilReleaseForCoverPoint()
{
	//Ensure the point is valid.
	CTacticalAnalysisCoverPoint* pPoint = GetPointForCoverPoint(m_PointToMoveTo.m_pCoverPoint);
	if(!pPoint)
	{
		return;
	}

	//Set the time until release.
	pPoint->SetTimeUntilRelease(sm_Tunables.m_TimeUntilRelease);
}

void CTaskMoveToTacticalPoint::SetTimeUntilReleaseForNavMeshPoint()
{
	//Ensure the point is valid.
	CTacticalAnalysisNavMeshPoint* pPoint = GetPointForNavMeshPoint(m_PointToMoveTo.m_iHandleForNavMeshPoint);
	if(!pPoint)
	{
		return;
	}

	//Set the time until release.
	pPoint->SetTimeUntilRelease(sm_Tunables.m_TimeUntilRelease);
}

void CTaskMoveToTacticalPoint::SetTimeUntilReleaseForPoint()
{
	//Set the time until release for the cover point.
	SetTimeUntilReleaseForCoverPoint();

	//Set the time until release for the nav mesh point.
	SetTimeUntilReleaseForNavMeshPoint();

	//Note that we have set the time until release for the point.
	m_uRunningFlags.SetFlag(RF_HasSetTimeUntilReleaseForPoint);
}

bool CTaskMoveToTacticalPoint::ShouldKeepCoverPoint() const
{
	//Ensure the state is valid.
	switch(GetState())
	{
		case State_MoveToPosition:
		case State_WaitAtPosition:
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

bool CTaskMoveToTacticalPoint::ShouldNotUseLaddersOrClimbs() const
{
	//Ensure the position to move to is valid.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return false;
	}

	//Ensure the position we are moving to is away from the target.
	const CPed* pTargetPed = GetTargetPed();
	Vec3V vToTarget = Subtract(pTargetPed->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition());
	Vec3V vToPosition = Subtract(vPositionToMoveTo, GetPed()->GetTransform().GetPosition());
	ScalarV scDot = Dot(vToTarget, vToPosition);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}

	//Ensure we are the closest ped to the target.
	if(GetPed() != CNearestPedInCombatHelper::Find(*pTargetPed))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::ShouldQuitWhenPointIsReached() const
{
	//Ensure the flag is not set.
	if(m_uConfigFlags.IsFlagSet(CF_DontQuitWhenPointIsReached))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::ShouldReleasePointDueToCleanUp() const
{
	//Ensure we have not set the time until release.
	if(m_uRunningFlags.IsFlagSet(RF_HasSetTimeUntilReleaseForPoint))
	{
		return false;
	}

	//Check if we are moving to a cover point, and we are close.
	if(IsMovingToCoverPoint() && IsCloseToPositionToMoveTo())
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::ShouldScoreCoverPoints() const
{
	//Ensure the config flag is not set.
	if(m_uConfigFlags.IsFlagSet(CF_DisableCoverPoints))
	{
		return false;
	}

	//Ensure the running flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DisableCoverPoints))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::ShouldScoreNavMeshPoints() const
{
	//Ensure the flag is not set.
	if(m_uConfigFlags.IsFlagSet(CF_DisableNavMeshPoints))
	{
		return false;
	}

	return true;
}

bool CTaskMoveToTacticalPoint::ShouldSetTimeUntilReleaseDueToAbortFromEvent(const CEvent& rEvent) const
{
	//Check if the event is temporary.
	if(rEvent.IsTemporaryEvent())
	{
		return true;
	}

	//Check the type.
	switch(rEvent.GetEventType())
	{
		case EVENT_DAMAGE:
		{
			return true;
		}
		default:
		{
			break;
		}
	}

	return false;
}

bool CTaskMoveToTacticalPoint::UpdatePointToMoveTo()
{
	//Increase the time since the last point update.
	m_fTimeSinceLastPointUpdate += GetTimeStep();

	//Ensure we have a time between point updates.
	float fTimeBetweenPointUpdates = m_fTimeBetweenPointUpdates;
	if(fTimeBetweenPointUpdates < 0.0f)
	{
		return false;
	}

	//Ensure the time has expired.
	if(m_fTimeSinceLastPointUpdate < fTimeBetweenPointUpdates)
	{
		return false;
	}

	//Clear the timer.
	m_fTimeSinceLastPointUpdate = 0.0f;

	//Find a point to move to.
	Point pointToMoveTo;
	if(!FindPointToMoveTo(pointToMoveTo))
	{
		return false;
	}

	//Ensure we are not moving to the point.
	if(IsMovingToPoint(pointToMoveTo))
	{
		return false;
	}

	//Reserve the point.
	if(!ReservePoint(pointToMoveTo))
	{
		return false;
	}

	return true;
}

CTacticalAnalysisCoverPoint* CTaskMoveToTacticalPoint::GetCoverPointToMoveTo()
{
	return const_cast<CTacticalAnalysisCoverPoint *>(static_cast<const CTaskMoveToTacticalPoint &>(*this).GetCoverPointToMoveTo());
}

const CTacticalAnalysisCoverPoint* CTaskMoveToTacticalPoint::GetCoverPointToMoveTo() const
{
	return GetPointForCoverPoint(m_PointToMoveTo.m_pCoverPoint);
}

CTacticalAnalysisCoverPoint* CTaskMoveToTacticalPoint::GetPointForCoverPoint(CCoverPoint* pCoverPoint)
{
	return const_cast<CTacticalAnalysisCoverPoint *>(static_cast<const CTaskMoveToTacticalPoint &>(*this).GetPointForCoverPoint(pCoverPoint));
}

const CTacticalAnalysisCoverPoint* CTaskMoveToTacticalPoint::GetPointForCoverPoint(CCoverPoint* pCoverPoint) const
{
	//Ensure the tactical analysis is valid.
	const CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return NULL;
	}

	return pTacticalAnalysis->GetCoverPoints().GetPointForCoverPoint(pCoverPoint);
}

CTacticalAnalysisNavMeshPoint* CTaskMoveToTacticalPoint::GetPointForNavMeshPoint(int iHandle)
{
	return const_cast<CTacticalAnalysisNavMeshPoint *>(static_cast<const CTaskMoveToTacticalPoint &>(*this).GetPointForNavMeshPoint(iHandle));
}

const CTacticalAnalysisNavMeshPoint* CTaskMoveToTacticalPoint::GetPointForNavMeshPoint(int iHandle) const
{
	//Ensure the tactical analysis is valid.
	const CTacticalAnalysis* pTacticalAnalysis = GetTacticalAnalysis();
	if(!pTacticalAnalysis)
	{
		return NULL;
	}

	return pTacticalAnalysis->GetNavMeshPoints().GetPointForHandle(iHandle);
}

CTacticalAnalysisNavMeshPoint* CTaskMoveToTacticalPoint::GetNavMeshPointToMoveTo()
{
	return const_cast<CTacticalAnalysisNavMeshPoint *>(static_cast<const CTaskMoveToTacticalPoint &>(*this).GetNavMeshPointToMoveTo());
}

const CTacticalAnalysisNavMeshPoint* CTaskMoveToTacticalPoint::GetNavMeshPointToMoveTo() const
{
	return GetPointForNavMeshPoint(m_PointToMoveTo.m_iHandleForNavMeshPoint);
}

aiTask* CTaskMoveToTacticalPoint::Copy() const
{
	//Create the task.
	CTaskMoveToTacticalPoint* pTask = rage_new CTaskMoveToTacticalPoint(m_Target, m_uConfigFlags);

	//Set the callbacks.
	pTask->SetCanScoreCoverPointCallback(m_pCanScoreCoverPointCallback);
	pTask->SetCanScoreNavMeshPointCallback(m_pCanScoreNavMeshPointCallback);
	pTask->SetIsCoverPointValidToMoveToCallback(m_pIsCoverPointValidToMoveToCallback);
	pTask->SetIsNavMeshPointValidToMoveToCallback(m_pIsNavMeshPointValidToMoveToCallback);
	pTask->SetScoreCoverPointCallback(m_pScoreCoverPointCallback);
	pTask->SetScoreNavMeshPointCallback(m_pScoreNavMeshPointCallback);
	pTask->SetSpheresOfInfluenceBuilder(m_pSpheresOfInfluenceBuilder);

	//Set the max positions to exclude.
	pTask->SetMaxPositionsToExclude(m_iMaxPositionsToExclude);

	//Set the time to wait at the position.
	pTask->SetTimeToWaitAtPosition(m_fTimeToWaitAtPosition);

	//Set the time between point updates.
	pTask->SetTimeBetweenPointUpdates(m_fTimeBetweenPointUpdates);

	//Set the sub-task to use during movement.
	pTask->SetSubTaskToUseDuringMovement(CreateSubTaskToUseDuringMovement());

	return pTask;
}

void CTaskMoveToTacticalPoint::CleanUp()
{
	//Check if we should release the point due to cleanup.
	if(ShouldReleasePointDueToCleanUp())
	{
		//Release the point.
		ReleasePoint();
	}
}

void CTaskMoveToTacticalPoint::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the event is valid.
	if(pEvent)
	{
		//Check if we should set the time until release due to an abort from the event.
		if(ShouldSetTimeUntilReleaseDueToAbortFromEvent(*(CEvent *)pEvent))
		{
			//Set the time until release for the points.
			//This will allow us to keep our reservations around
			//for a little while, in case this task wants to resume.
			SetTimeUntilReleaseForPoint();
		}
	}

	//Call the base class version.
	CTask::DoAbort(iPriority, pEvent);
}

CTask::FSM_Return CTaskMoveToTacticalPoint::ProcessPreFSM()
{
	//Ensure the tactical analysis is valid.
	if(!GetTacticalAnalysis())
	{
		return FSM_Quit;
	}

	//Ensure the target ped is valid.
	if(!GetTargetPed())
	{
		return FSM_Quit;
	}

	//Process the cover point.
	ProcessCoverPoint();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveToTacticalPoint::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_FindPointToMoveTo)
			FSM_OnEnter
				FindPointToMoveTo_OnEnter();
			FSM_OnUpdate
				return FindPointToMoveTo_OnUpdate();
			FSM_OnExit
				FindPointToMoveTo_OnExit();

		FSM_State(State_UseCover)
			FSM_OnEnter
				UseCover_OnEnter();
			FSM_OnUpdate
				return UseCover_OnUpdate();

		FSM_State(State_MoveToPosition)
			FSM_OnEnter
				MoveToPosition_OnEnter();
			FSM_OnUpdate
				return MoveToPosition_OnUpdate();
			FSM_OnExit
				MoveToPosition_OnExit();

		FSM_State(State_WaitAtPosition)
			FSM_OnEnter
				WaitAtPosition_OnEnter();
			FSM_OnUpdate
				return WaitAtPosition_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskMoveToTacticalPoint::Start_OnUpdate()
{
	//Check if the ped can't use cover.
	if(!CanUseCover())
	{
		//Disable cover.
		m_uRunningFlags.SetFlag(RF_DisableCoverPoints);
	}

	//Find a point to move to.
	SetState(State_FindPointToMoveTo);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveToTacticalPoint::Resume_OnUpdate()
{
	//Clear the time until release for the points.
	ClearTimeUntilReleaseForPoint();

	//Set the state to resume to.
	bool bKeepSubTask = false;
	int iStateToResumeTo = ChooseStateToResumeTo(bKeepSubTask);
	SetState(iStateToResumeTo);

	//Check if we are resuming the cover state.
	if(GetState() == State_UseCover)
	{
		//Reserve the point.
		CCoverPoint* pCoverPoint = m_PointToMoveTo.m_pCoverPoint;
		taskAssert(pCoverPoint);
		ReserveCoverPoint(pCoverPoint, true);
	}

	//Check if we should keep the sub-task.
	if(bKeepSubTask)
	{
		//Keep the sub-task across the transition.
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

void CTaskMoveToTacticalPoint::FindPointToMoveTo_OnEnter()
{
	//Create the task.
	CTask* pTask = CreateSubTaskToUseDuringMovement();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveToTacticalPoint::FindPointToMoveTo_OnUpdate()
{
	//Check if we should wait before finding a point to move to.
	if((m_fTimeToWaitBeforeFindingPointToMoveTo > 0.0f) && (GetTimeInState() < m_fTimeToWaitBeforeFindingPointToMoveTo))
	{
		return FSM_Continue;
	}

	//Release the point.
	ReleasePoint();

	//Ensure we have not yet excluded too many positions.
	if(HasExcludedTooManyPositions())
	{
		//Finish the task.
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Ensure we can find a point to move to.
	Point pointToMoveTo;
	if(!FindPointToMoveTo(pointToMoveTo))
	{
		//Finish the task.
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Ensure we can reserve the point to move to.
	if(!ReservePoint(pointToMoveTo))
	{
		//Finish the task.
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Set the state for moving to the point.
	SetStateToMoveToPoint();

	return FSM_Continue;
}

void CTaskMoveToTacticalPoint::FindPointToMoveTo_OnExit()
{
	//Clear the time to wait.
	m_fTimeToWaitBeforeFindingPointToMoveTo = -1.0f;
}

void CTaskMoveToTacticalPoint::UseCover_OnEnter()
{
	// We're restarting a movement task so reset this flag
	m_uRunningFlags.ClearFlag(RF_HasCheckedInfluenceSpheres);

	//Clear the influence sphere check timer
	m_fInfluenceSphereCheckTimer = 0.0f;

	//Clear the timer.
	m_fTimeSinceLastPointUpdate = 0.0f;

	//Create the task.
	CTaskCover* pTask = rage_new CTaskCover(CWeaponTarget(m_Target));

	//Create the seek cover flags.
	int iSeekCoverFlags = CTaskCover::CF_RunSubtaskWhenMoving | CTaskCover::CF_RunSubtaskWhenStationary |
		CTaskCover::CF_NeverEnterWater | CTaskCover::CF_UseLargerSearchExtents | CTaskCover::CF_AllowClimbLadderSubtask;

	//Quit after cover entry, if we will quit when the point is reached.
	if(ShouldQuitWhenPointIsReached())
	{
		iSeekCoverFlags |= CTaskCover::CF_QuitAfterCoverEntry;
	}

	//Check if we shouldn't use ladders or climbs.
	if(ShouldNotUseLaddersOrClimbs())
	{
		iSeekCoverFlags |= CTaskCover::CF_DontUseLadders;
		iSeekCoverFlags |= CTaskCover::CF_DontUseClimbs;
	}

	//Set the search flags.
	pTask->SetSearchFlags(iSeekCoverFlags);

	//Set the sub-task to use whilst searching.
	pTask->SetSubtaskToUseWhilstSearching(CreateSubTaskToUseDuringMovement());

	//Set the spheres of influence builder.
	pTask->SetSpheresOfInfluenceBuilder(m_pSpheresOfInfluenceBuilder);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveToTacticalPoint::UseCover_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we failed to reach the position.
		if(!IsCloseToPositionToMoveTo())
		{
			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		else
		{
			//Check if we should quit when the point is reached.
			if(ShouldQuitWhenPointIsReached())
			{
				//Finish the task.
				SetState(State_Finish);
			}
			else
			{
				//Wait at the position.
				SetState(State_WaitAtPosition);
			}
		}
	}
	//Check if we are not climbing a ladder.
	else if(!GetPed()->GetPedIntelligence()->IsPedClimbingLadder())
	{
		//Check if the route has failed.
		RouteFailedReason nReason = HasRouteFailed();
		if(nReason != RFR_None)
		{
			//Note that the route failed.
			OnRouteFailed(nReason);

			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		//Check if we are moving to an invalid position.
		else if(IsMovingToInvalidPosition())
		{
			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		//Check if the point to move to has been updated.
		else if(UpdatePointToMoveTo())
		{
			//Set the state to move to the point.
			SetStateToMoveToPoint();
		}
	}

	return FSM_Continue;
}

void CTaskMoveToTacticalPoint::MoveToPosition_OnEnter()
{
	// We're restarting a movement task so reset this flag
	m_uRunningFlags.ClearFlag(RF_HasCheckedInfluenceSpheres);

	//Clear the influence sphere check timer
	m_fInfluenceSphereCheckTimer = 0.0f;

	//Clear the timer.
	m_fTimeSinceLastPointUpdate = 0.0f;

	//Get the position to move to.
	Vec3V vPositionToMoveTo;
	if(!GetPositionToMoveTo(vPositionToMoveTo))
	{
		return;
	}

	//Create the move params.
	CNavParams moveParams;
	moveParams.m_vTarget = VEC3V_TO_VECTOR3(vPositionToMoveTo);
	moveParams.m_fTargetRadius = sm_Tunables.m_TargetRadiusForMoveToPosition;
	moveParams.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	moveParams.m_pInfluenceSphereCallbackFn = m_pSpheresOfInfluenceBuilder;

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(moveParams);

	//Check if we should not use ladders or climbs.
	if(ShouldNotUseLaddersOrClimbs())
	{
		pMoveTask->SetDontUseLadders(true);
		pMoveTask->SetDontUseClimbs(true);
	}

	//Never enter water.
	pMoveTask->SetNeverEnterWater(true);

	//Use larger search extents.
	pMoveTask->SetUseLargerSearchExtents(true);

	//Create the sub-task.
	CTask* pSubTask = CreateSubTaskToUseDuringMovement();

	//Create the task.
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	pTask->SetAllowClimbLadderSubtask(true);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveToTacticalPoint::MoveToPosition_OnUpdate()
{
	//Check if the sub-task is invalid.
	if(!GetSubTask())
	{
		//Set the state for failing to move to the point.
		SetStateForFailedToMoveToPoint();
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we failed to reach the position.
		if(!IsCloseToPositionToMoveTo())
		{
			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		else
		{
			//Check if we should quit when the point is reached.
			if(ShouldQuitWhenPointIsReached())
			{
				//Finish the task.
				SetState(State_Finish);
			}
			else
			{
				//Wait at the position.
				SetState(State_WaitAtPosition);
			}
		}
	}
	//Check if we are not climbing a ladder.
	else if(!GetPed()->GetPedIntelligence()->IsPedClimbingLadder())
	{
		//Check if the route has failed.
		RouteFailedReason nReason = HasRouteFailed();
		if(nReason != RFR_None)
		{
			//Note that the route failed.
			OnRouteFailed(nReason);

			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		//Check if we are moving to an invalid position.
		else if(IsMovingToInvalidPosition())
		{
			//Set the state for failing to move to the point.
			SetStateForFailedToMoveToPoint();
		}
		//Check if the point to move to has been updated.
		else if(UpdatePointToMoveTo())
		{
			//Set the state to move to the point.
			SetStateToMoveToPoint();
		}
		else
		{
			//Activate timeslicing.
			ActivateTimeslicing();
		}
	}

	return FSM_Continue;
}

void CTaskMoveToTacticalPoint::MoveToPosition_OnExit()
{
	//Abort the nav mesh task. This is necessary due to the nav mesh task hanging around and giving stale results after a state change.
	AbortNavMeshTask();
}

void CTaskMoveToTacticalPoint::WaitAtPosition_OnEnter()
{
	//Create the task.
	CTask* pTask = CreateSubTaskToUseDuringMovement();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveToTacticalPoint::WaitAtPosition_OnUpdate()
{
	//Check if we have a time to wait at the position.
	float fTimeToWaitAtPosition = m_fTimeToWaitAtPosition;
	if(fTimeToWaitAtPosition >= 0.0f)
	{
		//Check if the time has expired.
		if(GetTimeInState() > fTimeToWaitAtPosition)
		{
			//Find a new point to move to.
			SetState(State_FindPointToMoveTo);
		}
	}

	return FSM_Continue;
}
