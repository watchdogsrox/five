// FILE :    TaskMoveWithinAttackWindow.cpp
// PURPOSE : Controls a peds movement within their attack window (to cover, using nav mesh, etc)
// CREATED : 21-02-2012

// File header
#include "Task/Movement/TaskMoveWithinAttackWindow.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/CombatManager.h"
#include "Task/Combat/Cover/coverfilter.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Movement/TaskNavMesh.h"

// Rage headers
#include "math/angmath.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////////////////////////////

float CTaskMoveWithinAttackWindow::ms_AttackDistances[CCombatBehaviour::CT_NumTypes][CCombatData::CR_NumRanges][CR_NumTypes] = 
{
	// Near, medium, far
	{ {5.0f, 15.0f}, {7.0f,  30.0f}, {15.0f, 40.0f}, {22.0f, 45.0f } }, // On Foot armed
	{ {5.0f, 15.0f}, {7.0f,  30.0f}, {15.0f, 40.0f}, {22.0f, 45.0f } }, // On Foot armed, seek cover only
	{ {0.0f, 10.0f}, {0.0f,  10.0f}, {0.0f,  10.0f}, {0.0f,  10.0f } }, // On Foot unarmed
	{ {5.0f, 15.0f}, {10.0f, 20.0f}, {15.0f, 25.0f}, {20.0f, 30.0f } }, // underwater
};

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskMoveWithinAttackWindow, 0x99babc2a)
CTaskMoveWithinAttackWindow::Tunables CTaskMoveWithinAttackWindow::ms_Tunables;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskMoveWithinAttackWindow
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskMoveWithinAttackWindow::CTaskMoveWithinAttackWindow(const CPed* pPrimaryTarget)
: m_pPrimaryTarget(pPrimaryTarget)
, m_bHasSearchedForCover(false)
, m_fTargetOffsetDistance(0.0f)
, m_fAttackWindowMin(-1.0f)
, m_fAttackWindowMax(-1.0f)
, m_iHandleForNavMeshPoint(-1)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WITHIN_ATTACK_WINDOW);
}

CTaskMoveWithinAttackWindow::~CTaskMoveWithinAttackWindow()
{}

void CTaskMoveWithinAttackWindow::CleanUp()
{
	if(m_iHandleForNavMeshPoint >= 0 && m_pPrimaryTarget)
	{
		CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*m_pPrimaryTarget.Get());
		if(pTacticalAnalysis)
		{
			CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
			rNavMeshPoints.ReleaseHandle(GetPed(), m_iHandleForNavMeshPoint);
		}
	}
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::ProcessPreFSM()
{
	if(!m_pPrimaryTarget)
	{
		return FSM_Quit;
	}

	if (GetPed()->HasHurtStarted())
	{
		GetPed()->SetIsStrafing(true);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_SearchForCover)
			FSM_OnEnter
				SearchForCover_OnEnter();
			FSM_OnUpdate
				return SearchForCover_OnUpdate();

		FSM_State(State_FindTacticalPosition)
			FSM_OnUpdate
				return FindTacticalPosition_OnUpdate();

		FSM_State(State_FindAdvancePosition)
			FSM_OnEnter
				FindGoToPosition_OnEnter();
			FSM_OnUpdate
				return FindAdvancePosition_OnUpdate();

		FSM_State(State_FindFleePosition)
			FSM_OnEnter
				FindGoToPosition_OnEnter();
			FSM_OnUpdate
				return FindFleePosition_OnUpdate();

		FSM_State(State_CheckLosToPosition)
			FSM_OnEnter
				CheckLosToPosition_OnEnter();
			FSM_OnUpdate
				return CheckLosToPosition_OnUpdate();

		FSM_State(State_CheckRouteToAdvancePosition)
			FSM_OnEnter
				CheckRouteToAdvancePosition_OnEnter();
			FSM_OnUpdate
				return CheckRouteToAdvancePosition_OnUpdate();

		FSM_State(State_CheckRouteToFleePosition)
			FSM_OnEnter
				CheckRouteToFleePosition_OnEnter();
			FSM_OnUpdate
				return CheckRouteToFleePosition_OnUpdate();

		FSM_State(State_MoveWithNavMesh)
			FSM_OnEnter
				MoveWithNavMesh_OnEnter();
			FSM_OnUpdate
				return MoveWithNavMesh_OnUpdate();
			FSM_OnExit
				MoveWithNavMesh_OnExit();

		FSM_State(State_Wait)
			FSM_OnEnter
				Wait_OnEnter();
			FSM_OnUpdate
				return Wait_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	pPed->ReleaseCoverPoint();

	if( m_pPrimaryTarget->IsDead() ||
		!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover) ||
		(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_MoveToLocationBeforeCoverSearch) && !m_iRunningFlags.IsFlagSet(RF_FindPositionFailed)) )
	{
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_FindTacticalPosition);
	}
	else
	{
		SetState(State_SearchForCover);
	}

	m_iRunningFlags.ClearFlag(RF_FindPositionFailed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::Resume_OnUpdate()
{
	//Keep the sub-task across the transition.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	//Set the state to resume to.
	int iStateToResumeTo = ChooseStateToResumeTo();
	SetState(iStateToResumeTo);

	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::SearchForCover_OnEnter()
{
	// Check if we are resuming the state and keep the sub-task if possible
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
	{
		return;
	}

	m_bHasSearchedForCover = true;

	// Search for cover anywhere in the attack window
	const Vector3 vPrimaryTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
	
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(GetPed(), m_fAttackWindowMin, m_fAttackWindowMax, true);

	float fDesiredMBR = MOVEBLENDRATIO_RUN;
	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(GetPed(), bShouldStrafe, fDesiredMBR);

	s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, fDesiredMBR);

	const s32 iFlags = CTaskCover::CF_DontMoveIfNoCoverFound|CTaskCover::CF_SeekCover|CTaskCover::CF_SearchInAnArcAwayFromTarget|CTaskCover::CF_CoverSearchByPos|CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_RequiresLosToTarget|CTaskCover::CF_RunSubtaskWhenMoving|CTaskCover::CF_RunSubtaskWhenStationary|CTaskCover::CF_NeverEnterWater|CTaskCover::CF_AllowClimbLadderSubtask;
	CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(m_pPrimaryTarget));
	pCoverTask->SetSearchFlags(iFlags);
	pCoverTask->SetSearchType(CCover::CS_MUST_PROVIDE_COVER);
	pCoverTask->SetSearchPosition(vPrimaryTargetPosition);
	pCoverTask->SetMinMaxSearchDistances(m_fAttackWindowMin, m_fAttackWindowMax);
	pCoverTask->SetCoverPointFilterConstructFn(CCoverPointFilterTaskCombat::Construct);
	pCoverTask->SetMoveBlendRatio(fDesiredMBR);
	pCoverTask->SetSubtaskToUseWhilstSearching(	rage_new CTaskCombatAdditionalTask( iCombatRunningFlags, m_pPrimaryTarget, vPrimaryTargetPosition ) );
	SetNewTask(pCoverTask);
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::SearchForCover_OnUpdate()
{
	if( !GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_COVER) )
	{
		CPed* pPed = GetPed();
		if( pPed->GetCoverPoint() == NULL && CTaskMoveWithinAttackWindow::IsOutsideAttackWindow(pPed, m_pPrimaryTarget, false) )
		{
			SetState(State_FindTacticalPosition);
			return FSM_Continue;
		}
		// Find the new state from the table
		else
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::FindTacticalPosition_OnUpdate()
{
	CPed* pPed = GetPed();

	// First we get our attack window because the position we want will be somewhere between the min and max
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, m_fAttackWindowMin, m_fAttackWindowMax, false);

	// Ensure the tactical analysis is valid.
	CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*m_pPrimaryTarget.Get());
	if(pTacticalAnalysis)
	{
		// Store our defensive area
		CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() ? pPed->GetPedIntelligence()->GetDefensiveArea() : NULL;

		Vec3V vTargetPosition = m_pPrimaryTarget->GetTransform().GetPosition();
		Vec3V vTargetToPed = pPed->GetTransform().GetPosition() - vTargetPosition;

		const ScalarV scAttackWindowMinSq = ScalarVFromF32(square(m_fAttackWindowMin));
		const ScalarV scAttackWindowMaxSq = ScalarVFromF32(square(m_fAttackWindowMax));
		float fTargetOffsetDistance = (m_fAttackWindowMin + m_fAttackWindowMax) * fwRandom::GetRandomNumberInRange(0.45f, 0.6f);
		fTargetOffsetDistance = square(fTargetOffsetDistance);

		int	iBestIndex = -1;
		float fBestScore = LARGE_FLOAT;

		// Grab the nav mesh points
		CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();

		// Iterate over the nav mesh points.
		for(int i = 0; i < rNavMeshPoints.GetNumPoints(); ++i)
		{
			// Ensure the info can be reserved.
			const CTacticalAnalysisNavMeshPoint& rInfo = rNavMeshPoints.GetPointForIndex(i);
			if(!rInfo.CanReserve(pPed))
			{
				continue;
			}

			// Ensure the point has line of sight
			if(!rInfo.IsLineOfSightClear())
			{
				continue;
			}

			// Make sure it's not occupied
			if(rInfo.HasNearby() && !rInfo.IsNearby(pPed))
			{
				continue;
			}

			// Grab the position on the nav mesh
			Vec3V vPositionOnNavMesh = rInfo.GetPosition();

			// Ensure the position is not outside the attack window.
			const ScalarV scDistanceToTargetSq = DistSquared(vPositionOnNavMesh, vTargetPosition);
			if( IsGreaterThanAll(scDistanceToTargetSq, scAttackWindowMaxSq) || IsLessThanAll(scDistanceToTargetSq, scAttackWindowMinSq) )
			{
				continue;
			}

			// Make sure the point if between us and the target so we don't have to cross to the other side of the target
			Vec3V vTargetToPosition = vPositionOnNavMesh - vTargetPosition;
			ScalarV scDot = Dot(vTargetToPed, vTargetToPosition);
			if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
			{
				continue;
			}

			// Make sure if we have a defensive area that the point is within it
			if( pDefensiveArea && !pDefensiveArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vPositionOnNavMesh)) )
			{
				continue;
			}

			// Do some scoring
			float fScore = DistSquared(pPed->GetTransform().GetPosition(), vPositionOnNavMesh).Getf() + abs(scDistanceToTargetSq.Getf() - fTargetOffsetDistance);

			// Ensure the score is better. Lower scores win
			if(iBestIndex == -1 || fBestScore > fScore)
			{
				// Set the best position
				iBestIndex = i;
				fBestScore = fScore;
			}
		}

		// Ensure the index is valid
		if(iBestIndex >= 0)
		{
			if(m_iHandleForNavMeshPoint < 0 || rNavMeshPoints.ReleaseHandle(pPed, m_iHandleForNavMeshPoint))
			{
				m_iHandleForNavMeshPoint = -1;

				// Reserve the nav mesh point.
				CTacticalAnalysisNavMeshPoint& rPoint = rNavMeshPoints.GetPointForIndex(iBestIndex);
				if( rNavMeshPoints.ReserveHandle(pPed, rPoint.GetHandle()) )
				{
					m_iHandleForNavMeshPoint = rPoint.GetHandle();

					//Set the position.
					m_vTargetOffset = rPoint.GetPosition() - vTargetPosition;
					SetState(State_MoveWithNavMesh);
					return FSM_Continue;
				}
			}
		}
	}

	// Our position and distance to our target
	const float fDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf();

	// We only want to do this position finding if moving closer to the target, fleeing targets works well without this
	if(fDistanceToTargetSq > rage::square(m_fAttackWindowMax))
	{
		SetState(State_FindAdvancePosition);
	}
	else
	{
		SetState(State_FindFleePosition);
	}

	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::FindGoToPosition_OnEnter()
{
	if(!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK)
	{
		SetNewTask(rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition())));
	}
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::FindAdvancePosition_OnUpdate()
{
	CPed* pPed = GetPed();
	Vec3V_ConstRef vMyPos = pPed->GetTransform().GetPosition();

	m_vTargetOffset.ZeroComponents();
	m_iRunningFlags.ClearFlag(RF_IsFleeRouteCheck);
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	m_fTargetOffsetDistance = (m_fAttackWindowMin + m_fAttackWindowMax) * fwRandom::GetRandomNumberInRange(0.45f, 0.6f);

	float fHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
	float fMaxAllyDistanceSq = rage::square(CTaskMoveWithinAttackWindow::ms_Tunables.m_fMaxAllyDistance);
	int iNumAlliesOnLeft = 0;
	int iNumAlliesOnRight = 0;

	// We want to scan through our nearby peds and find the allies
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
		if(!pOtherPed->IsInjured() && pPed->GetPedIntelligence()->IsFriendlyWith(*pOtherPed))
		{
			// Check to see if the ally is close enough and if they are on the left or right side of us
			Vec3V_ConstRef vAllyPos = pOtherPed->GetTransform().GetPosition();
			if(DistSquared(vAllyPos, vMyPos).Getf() < fMaxAllyDistanceSq)
			{
				float fHeadingToOtherPed = fwAngle::LimitRadianAngleSafe(fwAngle::GetRadianAngleBetweenPoints(vAllyPos.GetXf(), vAllyPos.GetYf(), vMyPos.GetXf(), vMyPos.GetYf()));
				if(SubtractAngleShorter(fHeading, fHeadingToOtherPed) < 0.0f)
				{
					iNumAlliesOnLeft++;
				}
				else
				{
					iNumAlliesOnRight++;
				}
			}
		}
	}

	s32 iDesiredState = State_MoveWithNavMesh;
	Vec3V vTargetPos = m_pPrimaryTarget->GetTransform().GetPosition();

	// if we are not the center ally then we'll want to compute a position to the left or right side slightly
	if(iNumAlliesOnRight != iNumAlliesOnLeft)
	{
		int iLeftRightDiff = abs(iNumAlliesOnLeft - iNumAlliesOnRight);
		int iNumAllies = iNumAlliesOnLeft + iNumAlliesOnRight;

		// Get our vector from the target to us and normalize it
		Vector3 vTargetToPed = VEC3V_TO_VECTOR3(vMyPos) - VEC3V_TO_VECTOR3(vTargetPos);
		vTargetToPed.Normalize();

		// rotate the target to ped vector by the desired amount, based on if we have more allies on the left or right of us
		float fOffsetAmount = float(iLeftRightDiff) / float(iNumAllies);
		fOffsetAmount += fwRandom::GetRandomNumberInRange(-CTaskMoveWithinAttackWindow::ms_Tunables.m_fMaxRandomAdditionalOffset, CTaskMoveWithinAttackWindow::ms_Tunables.m_fMaxRandomAdditionalOffset);
		float fMaxOffset = Clamp((float)iNumAllies / CTaskMoveWithinAttackWindow::ms_Tunables.m_fMinAlliesForMaxAngleOffset, 0.0f, 1.0f);
		fMaxOffset = fMaxOffset * CTaskMoveWithinAttackWindow::ms_Tunables.m_fMaxAngleOffset;
		float fZRotation = iNumAlliesOnLeft > iNumAlliesOnRight ? (fMaxOffset * fOffsetAmount) : -(fMaxOffset * fOffsetAmount);
		vTargetToPed.RotateZ(fZRotation);

		// set the offset to the new rotated vector * the desired distance we chose
		m_vTargetOffset = VECTOR3_TO_VEC3V(vTargetToPed * m_fTargetOffsetDistance);
		iDesiredState = State_CheckLosToPosition;
	}

	// Go to the wait state if the desired position is outside the defensive area
	CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
	if(pDefensiveArea && pDefensiveArea->IsActive() && !pDefensiveArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(m_vTargetOffset + vTargetPos)))
	{
		m_vTargetOffset.ZeroComponents();
		iDesiredState = State_Wait;
	}
	
	SetState(iDesiredState);
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::FindFleePosition_OnUpdate()
{
	// if we are moving away from the target
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	m_iRunningFlags.SetFlag(RF_IsFleeRouteCheck);
	SetState(State_CheckRouteToFleePosition);
	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::CheckLosToPosition_OnEnter()
{
	// We start by checking for the ground position of where we want to check LOS from
	Vector3 vTargetPos, vGoToPosition;
	m_pPrimaryTarget->GetLockOnTargetAimAtPos(vTargetPos);

	if(m_iRunningFlags.IsFlagSet(RF_IsFleeRouteCheck))
	{
		vGoToPosition = VEC3V_TO_VECTOR3(m_vTargetOffset) + Vector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		vGoToPosition = vTargetPos + VEC3V_TO_VECTOR3(m_vTargetOffset);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(Vector3(vGoToPosition.x, vGoToPosition.y, 1000.0f), vGoToPosition);
		probeDesc.SetContext(WorldProbe::ELosCombatAI);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);

		WorldProbe::CShapeTestFixedResults<> probeResult;
		probeDesc.SetResultsStructure(&probeResult);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// if we hit the ground go ahead and use the ground and the basis for our Z position/offset
			vGoToPosition.z = probeResult[0].GetHitPosition().z + 1.0f;
			m_vTargetOffset.SetZf(vGoToPosition.z - vTargetPos.z);
		}
	}

	// next we start the probe to check if this position has line of sight to the target
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vGoToPosition, vTargetPos);
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE);
	probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
	m_asyncProbeHelper.ResetProbe();
	m_asyncProbeHelper.StartTestLineOfSight(probeData);
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::CheckLosToPosition_OnUpdate()
{
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	if(m_pFollowNavMeshTask)
	{
		static_cast<CTaskMoveFollowNavMesh*>(m_pFollowNavMeshTask.Get())->SetCheckedThisFrame(true);
	}

	// Check to see if the probe failed for any reason (not started, LOS blocked) or if we have clear LOS
	bool bFailed = false;
	if(!m_asyncProbeHelper.IsProbeActive())
	{
		bFailed = true;
	}
	else
	{
		ProbeStatus probeStatus;
		if(m_asyncProbeHelper.GetProbeResults(probeStatus))
		{
			if(probeStatus == PS_Blocked)
			{
				bFailed = true;
			}
			else if(probeStatus == PS_Clear)
			{
				SetState(m_iRunningFlags.IsFlagSet(RF_IsFleeRouteCheck) ? State_MoveWithNavMesh : State_CheckRouteToAdvancePosition);
			}
		}
	}

	if(bFailed)
	{
		m_vTargetOffset.ZeroComponents();
		SetState(m_iRunningFlags.IsFlagSet(RF_IsFleeRouteCheck) ? State_Wait : State_MoveWithNavMesh);
	}

	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::CheckRouteToAdvancePosition_OnEnter()
{
	CPed* pPed = GetPed();

	// Get our positions and build our influence sphere to try and avoid the target
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
	
	Vector3 vGoToPosition = vTargetPosition + VEC3V_TO_VECTOR3(m_vTargetOffset);
	TInfluenceSphere InfluenceSphere[1];
	InfluenceSphere[0].Init(vTargetPosition, CTaskCombat::ms_Tunables.m_TargetInfluenceSphereRadius, 1.0f*INFLUENCE_SPHERE_MULTIPLIER, 0.5f*INFLUENCE_SPHERE_MULTIPLIER);

	m_RouteSearchHelper.ResetSearch();
	m_RouteSearchHelper.StartSearch(pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), vGoToPosition, 1.0f, 0, 1, InfluenceSphere);
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::CheckRouteToAdvancePosition_OnUpdate()
{
	// if the search is not active then something went wrong and we just move to the target
	if(!m_RouteSearchHelper.IsSearchActive())
	{
		m_vTargetOffset.ZeroComponents();
		SetState(State_MoveWithNavMesh);
	}
	else
	{
		float fDistance = 0.0f;
		Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
		SearchStatus eSearchStatus;

		// get our search results to get the status, total distance and last point
		if(m_RouteSearchHelper.GetSearchResults(eSearchStatus, fDistance, vLastPoint))
		{
			if(eSearchStatus == SS_SearchSuccessful)
			{
				// even if we succeeded we need to make sure the route isn't extremely long, if it is we just go to the target directly
				float fDistToTarget = Dist(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf();
				fDistToTarget *= CTaskMoveWithinAttackWindow::ms_Tunables.m_fMaxRouteDistanceModifier;
				if(fDistance > fDistToTarget)
				{
					m_vTargetOffset.ZeroComponents();
				}
				
				SetState(State_MoveWithNavMesh);
			}
			else if(eSearchStatus == SS_SearchFailed)
			{
				// if the route search failed we'll just try to go directly to the target
				m_vTargetOffset.ZeroComponents();
				SetState(State_MoveWithNavMesh);
			}
		}
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::CheckRouteToFleePosition_OnEnter()
{
	CPed* pPed = GetPed();

	// Get our positions and build our influence sphere to try and avoid the target
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());

	CNavParams navParams;
	navParams.m_pInfluenceSphereCallbackFn = CTaskCombat::SpheresOfInfluenceBuilder;
	navParams.m_vTarget = vTargetPosition;
	navParams.m_fMoveBlendRatio = MOVEBLENDRATIO_STILL;
	navParams.m_bFleeFromTarget = true;

	// Set our desired distance from the target so we can use it later without having to get the attack window min/max
	m_fTargetOffsetDistance = (m_fAttackWindowMin + m_fAttackWindowMax) * fwRandom::GetRandomNumberInRange(0.45f, 0.55f);
	navParams.m_fFleeSafeDistance = m_fTargetOffsetDistance;

	CTaskMoveFollowNavMesh* pFollowNavMeshTask = rage_new CTaskMoveFollowNavMesh(navParams);
	pFollowNavMeshTask->SetPullBackFleeTargetFromPed(true);
	pFollowNavMeshTask->SetNeverEnterWater(true);
	NOTFINAL_ONLY(pFollowNavMeshTask->SetOwner(this));
	pFollowNavMeshTask->SetCheckedThisFrame(true);
	m_pFollowNavMeshTask = pFollowNavMeshTask;
	pPed->GetPedIntelligence()->AddTaskMovement(m_pFollowNavMeshTask);
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::CheckRouteToFleePosition_OnUpdate()
{
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	if(!m_pFollowNavMeshTask)
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(m_pFollowNavMeshTask.Get());
	pNavMeshTask->SetCheckedThisFrame(true);

	if(pNavMeshTask->IsUnableToFindRoute())
	{
		// In a failed flee case we need to wait briefly before checking for a good flee path
		SetState(State_Wait);
	}
	else if(pNavMeshTask->IsFollowingNavMeshRoute())
	{
		if(pNavMeshTask->GetPositionAlongRouteWithDistanceFromTarget(m_vTargetOffset, m_pPrimaryTarget->GetTransform().GetPosition(), m_fTargetOffsetDistance))
		{
			CDefensiveArea* pDefensiveArea = GetPed()->GetPedIntelligence()->GetDefensiveArea();
			if(pDefensiveArea && pDefensiveArea->IsActive() && !pDefensiveArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(m_vTargetOffset)))
			{
				SetState(State_Wait);
			}
			else
			{
				SetState(State_CheckLosToPosition);
			}
		}
		else
		{
			SetState(State_Wait);
		}
	}

	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::MoveWithNavMesh_OnEnter()
{
	// Check if we are resuming the state and keep the sub-task if possible
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		const CTaskComplexControlMovement* pComplexTask = static_cast<const CTaskComplexControlMovement*>(pSubTask);
		if(pComplexTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			return;
		}
	}

	CPed* pPed = GetPed();
	pPed->SetIsCrouching(false);

	const float fDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf();

	CNavParams navParams;
	navParams.m_pInfluenceSphereCallbackFn = CTaskCombat::SpheresOfInfluenceBuilder;
	navParams.m_vTarget = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(m_vTargetOffset);
	if (pPed->HasHurtStarted())
		navParams.m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	else
		navParams.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;

	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(pPed, bShouldStrafe, navParams.m_fMoveBlendRatio);
	s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, navParams.m_fMoveBlendRatio);

	CTaskMoveFollowNavMesh* pNavmeshTask = NULL;

	// Move toward target
	bool bMoveTowardTarget = fDistanceToTargetSq > rage::square(m_fAttackWindowMax);
	if( GetPreviousState() == State_FindTacticalPosition || bMoveTowardTarget )
	{
		navParams.m_bFleeFromTarget = false;
		navParams.m_fCompletionRadius = 1.0f;
		navParams.m_fTargetRadius = navParams.m_fCompletionRadius;
		iCombatRunningFlags |= CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked;
		if(bMoveTowardTarget)
		{
			iCombatRunningFlags |= CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions;
		}
		pNavmeshTask = rage_new CTaskMoveFollowNavMesh(navParams);
		pNavmeshTask->SetNeverEnterWater(true);
	}
	// move away from target
	else if(m_iRunningFlags.IsFlagSet(RF_IsFleeRouteCheck))
	{
		if(CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*m_pPrimaryTarget, CCombatTaskManager::OF_ReturnAfterInitialValidPed))
		{
			pPed->NewSay("GET_HIM");
		}

		if(Verifyf(m_pFollowNavMeshTask, "We are fleeing and our follow nav mesh task got removed. Something went wrong!"))
		{
			navParams.m_bFleeFromTarget = true;
			pNavmeshTask = static_cast<CTaskMoveFollowNavMesh*>(m_pFollowNavMeshTask.Get());
			if (pPed->HasHurtStarted())
				pNavmeshTask->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
			else
				pNavmeshTask->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
		}
	}

	if(pNavmeshTask == NULL)
	{
		return;
	}

	pNavmeshTask->SetQuitTaskIfRouteNotFound(true);
	CTaskCombatAdditionalTask* pCombatTask = rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
	SetNewTask(rage_new CTaskComplexControlMovement(pNavmeshTask, pCombatTask, CTaskComplexControlMovement::TerminateOnMovementTask, 0.0f, navParams.m_bFleeFromTarget, CTaskComplexControlMovement::Flag_AllowClimbLadderSubTask));
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::MoveWithNavMesh_OnUpdate()
{
	CPed* pPed = GetPed();

	// If we are moving to a nav mesh point
	if(m_iHandleForNavMeshPoint > -1)
	{
		CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*m_pPrimaryTarget.Get());
		if(pTacticalAnalysis)
		{
			// Make sure our position is still valid
			CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
			const CTacticalAnalysisNavMeshPoint* pPoint = rNavMeshPoints.GetPointForHandle(m_iHandleForNavMeshPoint);
			if(!pPoint || !pPoint->IsValid() || !pPoint->IsReservedBy(pPed) || (pPoint->HasNearby() && !pPoint->IsNearby(pPed)))
			{
				SetState(State_Wait);
				return FSM_Continue;
			}
		}
	}

	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		if( !m_bHasSearchedForCover &&
			!m_pPrimaryTarget->IsDead() && 
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

	// Check if we are fleeing and if we are then check if we've reached the safe distance
	CTaskMoveFollowNavMesh* pFollowNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pFollowNavMeshTask && m_pPrimaryTarget)
	{
		// Check our distance to the target and if we are fleeing
		Vec3V vTargetPosition = m_pPrimaryTarget->GetTransform().GetPosition();
		const float fDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), vTargetPosition).Getf();
		bool bIsFleeing = pFollowNavMeshTask->GetIsFleeing();
		bool bHasReachedFleeDistance = false;

		// if fleeing we need to check if we've reached either our desired flee distance or the max that we would currently set it to
		if(bIsFleeing)
		{
			// Get our targets position and set it in the nav mesh task
			pFollowNavMeshTask->SetTarget(pPed, vTargetPosition);

			// Check against the existing flee distance
			if(fDistanceToTargetSq >= rage::square(pFollowNavMeshTask->GetFleeSafeDist()))
			{
				bHasReachedFleeDistance = true;
			}
			else
			{
				// Otherwise check against our current attack window (the max value we would set it to if entering the state right now)
				CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, m_fAttackWindowMin, m_fAttackWindowMax, false);
				if(fDistanceToTargetSq >= rage::square((m_fAttackWindowMin + m_fAttackWindowMax) * 0.55f))
				{
					bHasReachedFleeDistance = true;
				}
			}
		}

		// if fleeing we should be further than the flee distance, if approaching we should be closer than the completion radius
		if( bHasReachedFleeDistance ||
			(!bIsFleeing && fDistanceToTargetSq <= rage::square(m_fTargetOffsetDistance)))
		{
			// once reaching the desired distance, see if we are supposed to search for cover, otherwise quit
			if( !m_bHasSearchedForCover &&
				!m_pPrimaryTarget->IsDead() && 
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
	}

	return FSM_Continue;
}

void CTaskMoveWithinAttackWindow::MoveWithNavMesh_OnExit()
{
	if(m_iHandleForNavMeshPoint >= 0 && m_pPrimaryTarget)
	{
		CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*m_pPrimaryTarget.Get());
		if(pTacticalAnalysis)
		{
			CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
			rNavMeshPoints.ReleaseHandle(GetPed(), m_iHandleForNavMeshPoint);
		}
	}

	m_iHandleForNavMeshPoint = -1;
}

void CTaskMoveWithinAttackWindow::Wait_OnEnter()
{
	if(!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK)
	{
		SetNewTask(rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition())));
	}

	m_iRunningFlags.SetFlag(RF_FindPositionFailed);
	m_iRunningFlags.ClearFlag(RF_IsFleeRouteCheck);
	m_fTimeToStayInWait = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fMinTimeToWait, ms_Tunables.m_fMaxTimeToWait);
}

CTask::FSM_Return CTaskMoveWithinAttackWindow::Wait_OnUpdate()
{
	CPed* pPed = GetPed();
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, m_fAttackWindowMin, m_fAttackWindowMax, false);
	const ScalarV scAttackWindowMinSq = ScalarVFromF32(square(m_fAttackWindowMin));
	const ScalarV scAttackWindowMaxSq = ScalarVFromF32(square(m_fAttackWindowMax));
	const ScalarV scDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	if( !IsGreaterThanAll(scDistanceToTargetSq, scAttackWindowMaxSq) && !IsLessThanAll(scDistanceToTargetSq, scAttackWindowMinSq) )
	{
		return FSM_Quit;
	}

	if(GetTimeInState() > m_fTimeToStayInWait)
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
		if( !pGunTask || !pGunTask->GetIsReloading() )
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetState(State_Start);
		}
	}

	return FSM_Continue;
}

int CTaskMoveWithinAttackWindow::ChooseStateToResumeTo() const
{
	//Check the previous state.
	int iPreviousState = GetPreviousState();
	switch(iPreviousState)
	{
		case State_SearchForCover:
		case State_MoveWithNavMesh:
		{
			return iPreviousState;
		}
		default:
		{
			return State_Start;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(const CPed* pPed, float& fMin, float& fMax, bool bUseCoverMax)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return;
	}

	//Get the combat type.
	CCombatBehaviour::CombatType combatType = CCombatBehaviour::GetCombatType(pPed);
	Assert( combatType >= CCombatBehaviour::CT_OnFootArmed && combatType < CCombatBehaviour::CT_NumTypes );

	//Get the attack range.
	const CCombatOrders* pCombatOrders = pPed->GetPedIntelligence()->GetCombatOrders();
	CCombatData::Range attackRange = (pCombatOrders && pCombatOrders->HasAttackRange()) ?
		pCombatOrders->GetAttackRange() : pPed->GetPedIntelligence()->GetCombatBehaviour().GetAttackRange();
	Assert( attackRange >= CCombatData::CR_Near && attackRange < CCombatData::CR_NumRanges );

	//Get the attack window.
	fMin = ms_AttackDistances[combatType][attackRange][CR_Min];
	fMax = ms_AttackDistances[combatType][attackRange][CR_Max];

	if(bUseCoverMax)
	{
		float fMaxDistanceForCover = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatAttackWindowDistanceForCover);
		if(fMaxDistanceForCover > 0.0f)
		{
			fMax = fMaxDistanceForCover;
		}
	}
}

bool CTaskMoveWithinAttackWindow::IsOutsideAttackWindow( const CPed* pPed, const CPed* pTarget, bool bUseCoverMax )
{
	bool bIsTooClose;
	return IsPositionOutsideAttackWindow(pPed, pTarget, pPed->GetTransform().GetPosition(), bUseCoverMax, bIsTooClose);
}

bool CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow( const CPed* pPed, const CPed* pTarget, Vec3V_In vPosition, bool bUseCoverMax )
{
	bool bIsTooClose;
	return IsPositionOutsideAttackWindow(pPed, pTarget, vPosition, bUseCoverMax, bIsTooClose);
}

bool CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow( const CPed* pPed, const CPed* pTarget, Vec3V_In vPosition, bool bUseCoverMax, bool &bIsTooClose )
{
	if(!pPed || !pTarget)
	{
		return true;
	}

	// Work out if we are currently inside or outside the attack window
	float fWindowMin = 0.0f;
	float fWindowMax = 0.0f;
	GetAttackWindowMinMax(pPed, fWindowMin, fWindowMax, bUseCoverMax);
	// HACK TO TEST SWIMMING CODE
	if( pPed->GetIsSwimming() )
	{
		fWindowMin = 7.5f;
		fWindowMax = 20.0f;
	}

	const ScalarV scAttackWindowMinSq = ScalarVFromF32(square(fWindowMin));
	const ScalarV scAttackWindowMaxSq = ScalarVFromF32(square(fWindowMax));
	const ScalarV scDistanceToTargetSq = DistSquared(vPosition, pTarget->GetTransform().GetPosition());
	if(IsGreaterThanAll(scDistanceToTargetSq, scAttackWindowMaxSq))
	{
		bIsTooClose = false;
		return true;
	}
	else if(IsLessThanAll(scDistanceToTargetSq, scAttackWindowMinSq))
	{
		bIsTooClose = true;
		return true;
	}

	return false;
}

#if !__FINAL
const char * CTaskMoveWithinAttackWindow::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_Resume:							return "State_Resume";
		case State_SearchForCover:					return "State_SearchForCover";
		case State_FindTacticalPosition:			return "State_FindTacticalPosition";
		case State_FindAdvancePosition:				return "State_FindAdvancePosition";
		case State_FindFleePosition:				return "State_FindFleePosition";
		case State_CheckLosToPosition:				return "State_CheckLosToPosition";
		case State_CheckRouteToAdvancePosition:		return "State_CheckRouteToAdvancePosition";
		case State_CheckRouteToFleePosition:		return "State_CheckRouteToFleePosition";
		case State_MoveWithNavMesh:					return "State_MoveWithNavMesh";
		case State_Wait:							return "State_Wait";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL
