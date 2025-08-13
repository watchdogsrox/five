// FILE :    TaskTargetUnreachable.h
// PURPOSE : Subtask of combat used for when the target is unreachable

// File header
#include "Task/Combat/Subtasks/TaskTargetUnreachable.h"

// Game headers
#include "pathserver/PathServer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskCombat.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/General/TaskBasic.h"
#include "task/Movement/TaskIdle.h"
#include "task/Movement/TaskMoveWithinAttackWindow.h"
#include "task/Movement/TaskNavMesh.h"
#include "Task/Weapons/TaskWeaponBlocked.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskTargetUnreachable
//=========================================================================

CTaskTargetUnreachable::Tunables CTaskTargetUnreachable::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskTargetUnreachable, 0xf87bd311);

CTaskTargetUnreachable::CTaskTargetUnreachable(const CAITarget& rTarget, fwFlags8 uConfigFlags)
: m_Target(rTarget)
, m_uConfigFlags(uConfigFlags)
, m_fTimeUntilNextRouteSearch(0.0f)
{
	m_routeSearchHelper.ResetSearch();
	SetInternalTaskType(CTaskTypes::TASK_TARGET_UNREACHABLE);
}

CTaskTargetUnreachable::~CTaskTargetUnreachable()
{
}

void CTaskTargetUnreachable::SetTarget(const CAITarget& rTarget)
{
	if(m_Target != rTarget)
	{
		m_routeSearchHelper.ResetSearch();
		m_Target = rTarget;

		if(GetState() == State_InExterior)
		{
			CTaskTargetUnreachableInExterior* pSubTask = static_cast<CTaskTargetUnreachableInExterior*>(FindSubTaskOfType(CTaskTypes::TASK_TARGET_UNREACHABLE_IN_EXTERIOR));
			if(pSubTask)
			{
				pSubTask->SetTarget(rTarget);
			}
		}
		else if(GetState() == State_InInterior)
		{
			CTaskTargetUnreachableInInterior* pSubTask = static_cast<CTaskTargetUnreachableInInterior*>(FindSubTaskOfType(CTaskTypes::TASK_TARGET_UNREACHABLE_IN_INTERIOR));
			if(pSubTask)
			{
				pSubTask->SetTarget(rTarget);
			}
		}
	}
}

#if !__FINAL
void CTaskTargetUnreachable::Debug() const
{
	CTask::Debug();
}

const char* CTaskTargetUnreachable::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_InInterior",
		"State_InExterior",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskTargetUnreachable::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return FSM_Quit;
	}
	
	//Check if we should quit due to line of sight being clear.
	if(ShouldQuitDueToLosClear())
	{
		return FSM_Quit;
	}

	// Check if we have a route to the target
	if(ProcessRouteSearch())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskTargetUnreachable::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_InInterior)
			FSM_OnEnter
				InInterior_OnEnter();
			FSM_OnUpdate
				return InInterior_OnUpdate();
		
		FSM_State(State_InExterior)
			FSM_OnEnter
				InExterior_OnEnter();
			FSM_OnUpdate
				return InExterior_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskTargetUnreachable::Start_OnUpdate()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if the ped is in an exterior.
	if(pPed->GetIsInExterior())
	{
		//Move to the exterior state.
		SetState(State_InExterior);
	}
	else
	{
		//Move to the interior state.
		SetState(State_InInterior);
	}
	
	return FSM_Continue;
}

void CTaskTargetUnreachable::InInterior_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskTargetUnreachableInInterior(m_Target);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskTargetUnreachable::InInterior_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskTargetUnreachable::InExterior_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskTargetUnreachableInExterior(m_Target);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskTargetUnreachable::InExterior_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

Vec3V_Out CTaskTargetUnreachable::GenerateTargetPositionForRouteSearch() const
{
	//Use the last nav mesh intersection.
	Vec3V vPosition;
	if(CLastNavMeshIntersectionHelper::Get(m_Target, vPosition))
	{
		return vPosition;
	}

	//Get the target position.
	m_Target.GetPosition(RC_VECTOR3(vPosition));

	return vPosition;
}

bool CTaskTargetUnreachable::ShouldQuitDueToLosClear() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_QuitIfLosClear))
	{
		return false;
	}
	
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(false);
	if(!pTargeting)
	{
		return false;
	}
	
	//Ensure the entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!taskVerifyf(pEntity, "The entity is invalid."))
	{
		return false;
	}
	
	//Ensure the LOS is clear.
	if(pTargeting->GetLosStatus(pEntity, false) != Los_clear)
	{
		return false;
	}
	
	return true;
}

bool CTaskTargetUnreachable::ProcessRouteSearch()
{
	// Only do these tests if told to
	if(!m_uConfigFlags.IsFlagSet(CTaskTargetUnreachable::CF_QuitIfRouteFound))
	{
		return false;
	}

	if(m_routeSearchHelper.IsSearchActive())
	{
		float fDistance = 0.0f;
		Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
		SearchStatus eSearchStatus;

		if(m_routeSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPoint ))
		{
			m_routeSearchHelper.ResetSearch();

			if(eSearchStatus == SS_SearchSuccessful)
			{
				return true;
			}
		}
	}
	else
	{
		// If we are still looking for a route and our timer has run up we will start a new search
		m_fTimeUntilNextRouteSearch -= GetTimeStep();
		if(m_fTimeUntilNextRouteSearch <= 0.0f)
		{
			const CEntity* pTarget = m_Target.GetEntity();
			if(pTarget && pTarget->GetIsTypePed() && IsTargetUnreachableUsingNavMesh(static_cast<const CPed&>(*pTarget)))
			{
				return false;
			}

			CPed* pPed = GetPed();
			Vec3V vTargetPosition = GenerateTargetPositionForRouteSearch();

			// If the target is outside of our defensive area then we know we won't be able to reach them so don't run the route check
			CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
			if(!pDefensiveArea || !pDefensiveArea->IsActive() || pDefensiveArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vTargetPosition)))
			{
				u64 iPathFlags = pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags() | PATH_FLAG_NEVER_ENTER_WATER;
				m_routeSearchHelper.StartSearch( pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(vTargetPosition), 1.0f, iPathFlags );
			}

			m_fTimeUntilNextRouteSearch = sm_Tunables.m_fTimeBetweenRouteSearches;
		}
	}

	return false;
}

bool CTaskTargetUnreachable::IsTargetUnreachableUsingNavMesh(const CPed& rTarget)
{
	CVehicle* pTargetVehicle = rTarget.GetVehiclePedInside();
	if( pTargetVehicle && (pTargetVehicle->InheritsFromPlane() || pTargetVehicle->InheritsFromHeli()) && !pTargetVehicle->HasContactWheels() )
	{
		return true;
	}

	return false;
}

//=========================================================================
// CTaskTargetUnreachableInInterior
//=========================================================================

CTaskTargetUnreachableInInterior::Tunables CTaskTargetUnreachableInInterior::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskTargetUnreachableInInterior, 0xf2b71c9f);

CTaskTargetUnreachableInInterior::CTaskTargetUnreachableInInterior(const CAITarget& rTarget)
: m_Target(rTarget)
, m_fAdditionalProbeRotationZ(0.0f)
, m_fOriginalDirection(-LARGE_FLOAT)
, m_bHasFacingDirection(false)
, m_fBestFacingDistSqToIntersection(-LARGE_FLOAT)
, m_fBestFacingDirection(-LARGE_FLOAT)
{
	m_asyncProbeHelper.ResetProbe();
	SetInternalTaskType(CTaskTypes::TASK_TARGET_UNREACHABLE_IN_INTERIOR);
}

CTaskTargetUnreachableInInterior::~CTaskTargetUnreachableInInterior()
{
}

#if !__FINAL
void CTaskTargetUnreachableInInterior::Debug() const
{
	CTask::Debug();
}

const char* CTaskTargetUnreachableInInterior::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_FaceDirection",
		"State_Idle",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskTargetUnreachableInInterior::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return FSM_Quit;
	}

	ProcessAsyncProbes();

	return FSM_Continue;
}

CTask::FSM_Return CTaskTargetUnreachableInInterior::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_FaceDirection)
			FSM_OnEnter
				FaceDirection_OnEnter();
			FSM_OnUpdate
				return FaceDirection_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskTargetUnreachableInInterior::Start_OnEnter()
{
	m_fOriginalDirection = fwAngle::LimitRadianAngleSafe(GetPed()->GetCurrentHeading());
	SetNewTask( rage_new CTaskDoNothing(-1) );
}

CTask::FSM_Return CTaskTargetUnreachableInInterior::Start_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	else if(m_bHasFacingDirection)
	{
		SetState(State_FaceDirection);
	}

	return FSM_Continue;
}

void CTaskTargetUnreachableInInterior::FaceDirection_OnEnter()
{
	// Face the best direction that we found
	Vector3 vMyPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Vector3 vTargetPosition(0.0f, sm_Tunables.m_fDirectionTestProbeLength, 0.0f);
	vTargetPosition.RotateZ(m_fBestFacingDirection);
	vTargetPosition += vMyPosition;

	SetNewTask( rage_new CTaskTurnToFaceEntityOrCoord(vTargetPosition) );
}

CTask::FSM_Return CTaskTargetUnreachableInInterior::FaceDirection_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Idle);
	}

	return FSM_Continue;
}

void CTaskTargetUnreachableInInterior::Idle_OnEnter()
{
	SetNewTask( rage_new CTaskWeaponBlocked(CWeaponController::WCT_Aim) );
}

CTask::FSM_Return CTaskTargetUnreachableInInterior::Idle_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskTargetUnreachableInInterior::ProcessAsyncProbes()
{
	// If we've got a direction or haven't set our original direction then we shouldn't run any probes
	if(m_bHasFacingDirection || m_fOriginalDirection < -PI)
	{
		return;
	}

	// Check our results if the probe is already active
	if(m_asyncProbeHelper.IsProbeActive())
	{
		Vector3 vIntersection;
		ProbeStatus probeStatus;
		if(m_asyncProbeHelper.GetProbeResultsWithIntersection(probeStatus, &vIntersection))
		{
			// If a probe fails we still need to check if it's our best possible direction
			if(probeStatus == PS_Blocked)
			{
				// Get the distance to the intersection and see if it's further than our current best
				float fDistSqToIntersection = vIntersection.Dist2(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()));
				if(fDistSqToIntersection > m_fBestFacingDistSqToIntersection)
				{
					m_fBestFacingDirection = (m_fOriginalDirection + m_fAdditionalProbeRotationZ);
					m_fBestFacingDistSqToIntersection = fDistSqToIntersection;
				}

				// If we've gone full circle with our tests, we'll just go with whatever our best direction is
				m_fAdditionalProbeRotationZ += fwRandom::GetRandomNumberInRange(.5f, .65f);
				if(m_fAdditionalProbeRotationZ > TWO_PI)
				{
					m_bHasFacingDirection = true;
				}
			}
			else
			{
				// if we didn't find an intersection then it means the entire probe was clear, go ahead and choose this direction
				m_fBestFacingDirection = (m_fOriginalDirection + m_fAdditionalProbeRotationZ);
				m_bHasFacingDirection = true;
			}
		}
	}
	else
	{
		// Setup the probe by starting at our position and going to a position at an angle with our desired probe length
		Vector3 vMyPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());

		Vector3 vTargetPosition(0.0f, sm_Tunables.m_fDirectionTestProbeLength, 0.0f);
		vTargetPosition.RotateZ(fwAngle::LimitRadianAngleSafe(m_fOriginalDirection + m_fAdditionalProbeRotationZ));
		vTargetPosition += vMyPosition;

		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), vTargetPosition);
		probeData.SetContext(WorldProbe::ELosCombatAI);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
		m_asyncProbeHelper.ResetProbe();
		m_asyncProbeHelper.StartTestLineOfSight(probeData);
	}
}

//=========================================================================
// CTaskTargetUnreachableInExterior
//=========================================================================

CTaskTargetUnreachableInExterior::Tunables CTaskTargetUnreachableInExterior::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskTargetUnreachableInExterior, 0xce6e1955);

CTaskTargetUnreachableInExterior::CTaskTargetUnreachableInExterior(const CAITarget& rTarget)
: m_vPosition(V_ZERO)
, m_Target(rTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_TARGET_UNREACHABLE_IN_EXTERIOR);
}

CTaskTargetUnreachableInExterior::~CTaskTargetUnreachableInExterior()
{
}

#if !__FINAL
void CTaskTargetUnreachableInExterior::Debug() const
{
	CTask::Debug();

#if DEBUG_DRAW

	static dev_bool s_bDebugDrawCoverPoints = false;
	if( s_bDebugDrawCoverPoints )
	{
		const CPed* pPed = GetPed();
		if( pPed )
		{
			const CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
			Vector3 vCoverPointPos;
			if( pCoverPoint && pCoverPoint->GetCoverPointPosition(vCoverPointPos) )
			{
				grcDebugDraw::Sphere(vCoverPointPos, 1.0f, Color_green, false);
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), vCoverPointPos, Color_green, Color_green);
			}

			const CCoverPoint* pDesiredCoverPoint = pPed->GetDesiredCoverPoint();
			Vector3 vDesiredCoverPointPos;
			if( pDesiredCoverPoint && pDesiredCoverPoint->GetCoverPointPosition(vDesiredCoverPointPos) )
			{
				grcDebugDraw::Sphere(vDesiredCoverPointPos, 1.0f, Color_blue, false);
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), vDesiredCoverPointPos, Color_blue, Color_blue);
			}
		}
	}

#endif // DEBUG_DRAW
}

const char* CTaskTargetUnreachableInExterior::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_UseCover",
		"State_FindPosition",
		"State_Reposition",
		"State_Wait",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskTargetUnreachableInExterior::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!GetTarget())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskTargetUnreachableInExterior::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_UseCover)
			FSM_OnEnter
				UseCover_OnEnter();
			FSM_OnUpdate
				return UseCover_OnUpdate();
		
		FSM_State(State_FindPosition)
			FSM_OnEnter
				FindPosition_OnEnter();
			FSM_OnUpdate
				return FindPosition_OnUpdate();

		FSM_State(State_Reposition)
			FSM_OnEnter
				Reposition_OnEnter();
			FSM_OnUpdate
				return Reposition_OnUpdate();
				
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

CTask::FSM_Return CTaskTargetUnreachableInExterior::Start_OnUpdate()
{
	if( HasValidDesiredCover() )
	{
		SetState(State_UseCover);
	}
	else
	{
		SetState(State_FindPosition);
	}
	
	return FSM_Continue;
}

void CTaskTargetUnreachableInExterior::UseCover_OnEnter()
{
	// Get local handle to ped
	CPed* pPed = GetPed();

	// If locally owned
	if(!pPed->IsNetworkClone())
	{
		// bail if no desired coverpoint at this stage
		if(!pPed->GetDesiredCoverPoint())
		{
			return;
		}

		weaponAssert(pPed->GetWeaponManager());
		taskAssertf(pPed->GetDesiredCoverPoint()->CanAccomodateAnotherPed(), "Desired cover can't add another ped");

		// setup the ped for moving to cover
		pPed->SetCoverPoint(pPed->GetDesiredCoverPoint());
		pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);
		pPed->SetDesiredCoverPoint(NULL);
		pPed->SetIsCrouching(false);
	}

	// Initialize cover task flags
	s32 iFlags = CTaskCover::CF_RunSubtaskWhenMoving | CTaskCover::CF_RunSubtaskWhenStationary | CTaskCover::CF_MoveToPedsCover |
		CTaskCover::CF_UseLargerSearchExtents | CTaskCover::CF_AllowClimbLadderSubtask;

	// If already moving we should keep moving
	Vector2 vCurrentMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMbr);
	if(vCurrentMbr.IsNonZero())
	{
		iFlags |= CTaskCover::CF_KeepMovingWhilstWaitingForPath;
	}

	// Compute move blend ratio and strafing
	float fDesiredMBR = MOVEBLENDRATIO_RUN;
	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(pPed, bShouldStrafe, fDesiredMBR);

	// Create the cover task and set parameters
	CTaskCover* pCoverTask = rage_new CTaskCover(m_Target);
	pCoverTask->SetSearchFlags(iFlags);
	pCoverTask->SetMoveBlendRatio(fDesiredMBR);
	CTask* pSubTask = CreateSubTaskForCombat();
	if( pSubTask )
	{
		pCoverTask->SetSubtaskToUseWhilstSearching(	pSubTask );
	}

	// Assign cover task as subtask
	SetNewTask(pCoverTask);
}

CTask::FSM_Return CTaskTargetUnreachableInExterior::UseCover_OnUpdate()
{
	// Local handle
	CPed* pPed = GetPed();

	// Mark our coverpoint as in use if we have one
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

	// If subtask quits
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COVER))
	{
		SetState(State_Wait);
		return FSM_Continue;
	}

	// If we have a target entity then we should update the in cover subtask
	if(m_Target.GetEntity())
	{
		CTaskInCover* pInCoverSubtask = static_cast<CTaskInCover*>(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER));
		if (pInCoverSubtask)
		{
			pInCoverSubtask->UpdateTarget(m_Target.GetEntity());
		}
	}

	return FSM_Continue;
}

void CTaskTargetUnreachableInExterior::FindPosition_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Grab the attack window.
	float fWindowMin = 0.0f;
	float fWindowMax = 0.0f;
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, fWindowMin, fWindowMax, false);
	
	//Calculate the range.
	float fRange = fWindowMax - fWindowMin;
	
	//Generate a random distance.
	float fDistance = fwRandom::GetRandomNumberInRange(fWindowMax - (fRange * sm_Tunables.m_RangePercentage), fWindowMax);
	
	//Generate a random angle.
	float fAngle = fwRandom::GetRandomNumberInRange(-PI, PI);
	
	//Grab the target position.
	Vector3 vTargetPosition;
	m_Target.GetPosition(vTargetPosition);
	
	//Calculate the offset.
	Vector3 vTargetOffset(0.0f, fDistance, 0.0f);
	vTargetOffset.RotateZ(fAngle);
	
	//Calculate the new position.
	Vector3 vNewPosition = vTargetPosition + vTargetOffset;
	
	//Move the height to the ped's level.
	vNewPosition.z = pPed->GetTransform().GetPosition().GetZf();
	
	//Find the closest position on the nav mesh.
	m_NavmeshClosestPositionHelper.ResetSearch();
	if(!m_NavmeshClosestPositionHelper.StartClosestPositionSearch(vNewPosition, sm_Tunables.m_MaxDistanceFromNavMesh,
		CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects|CNavmeshClosestPositionHelper::Flag_ConsiderExterior|
		CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand))
	{
		return;
	}

	//Create the task.
	CTask* pTask = CreateSubTaskForCombat();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskTargetUnreachableInExterior::FindPosition_OnUpdate()
{
	//Check if the search is not active.
	if(!m_NavmeshClosestPositionHelper.IsSearchActive())
	{
		SetState(State_Wait);
	}
	else
	{
		//Check if the search is done.
		SearchStatus nSearchStatus;
		static const int s_iMaxNumPositions = 1;
		Vector3 aPositions[1];
		s32 iNumPositions = 0;
		if(m_NavmeshClosestPositionHelper.GetSearchResults(nSearchStatus, iNumPositions, aPositions, s_iMaxNumPositions))
		{
			//Check if the search was successful.
			if((nSearchStatus == SS_SearchSuccessful) && (iNumPositions > 0) && !IsCloseAll(VECTOR3_TO_VEC3V(aPositions[0]), Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)))
			{
				//Set the position.
				m_vPosition = VECTOR3_TO_VEC3V(aPositions[0]);

				SetState(State_Reposition);
			}
			else
			{
				SetState(State_Wait);
			}
		}
	}

	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskTargetUnreachableInExterior::Reposition_OnEnter()
{
	//Assert that the position is valid.
	taskAssert(!IsCloseAll(m_vPosition, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	//Ensure the closest position is not outside the defensive area.
	if(GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
		!GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(m_vPosition)))
	{
		return;
	}
	
	//Create the params.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(m_vPosition);
	params.m_fTargetRadius = sm_Tunables.m_TargetRadius;
	params.m_fMoveBlendRatio = sm_Tunables.m_MoveBlendRatio;
	params.m_fCompletionRadius = sm_Tunables.m_CompletionRadius;
	
	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);

	//Use larger search extents.
	pMoveTask->SetUseLargerSearchExtents(true);

	//Never enter water.
	pMoveTask->SetNeverEnterWater(true);

	//Create the sub-task.
	CTask* pSubTask = CreateSubTaskForCombat();
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskTargetUnreachableInExterior::Reposition_OnUpdate()
{
	// No need to change state by default
	bool bNeedToChangeState = false;

	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		bNeedToChangeState = true;
	}
	else
	{
		//Find the task.
		CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
		if(pTask && pTask->IsUnableToFindRoute())
		{
			//Abort the task.
			if(!pTask->GetIsFlagSet(aiTaskFlags::IsAborted) && pTask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
			{
				bNeedToChangeState = true;				
			}
		}
	}

	if( bNeedToChangeState )
	{
		// Check to see if ped has cover to go to		
		if( HasValidDesiredCover() )
		{
			SetState(State_UseCover);
		}
		else
		{
			SetState(State_Wait);
		}

		return FSM_Continue;
	}

	ActivateTimeslicing();
	
	return FSM_Continue;
}

void CTaskTargetUnreachableInExterior::Wait_OnEnter()
{
	//Calculate the time to wait.
	float fTimeToWait = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTimeToWait, sm_Tunables.m_MaxTimeToWait);
	
	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveStandStill((int)(fTimeToWait * 1000.0f));
	
	//Create the sub-task.
	CTask* pSubTask = CreateSubTaskForCombat();
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskTargetUnreachableInExterior::Wait_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Check to see if ped has cover to go to		
		if( HasValidDesiredCover() )
		{
			SetState(State_UseCover);
		}
		else
		{
			//Grab the target.
			const CPed* pTarget = GetTarget();
			if(pTarget && CTaskTargetUnreachable::IsTargetUnreachableUsingNavMesh(*pTarget))
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			SetState(State_FindPosition);
		}
	}

	ActivateTimeslicing();
	
	return FSM_Continue;
}

CTask* CTaskTargetUnreachableInExterior::CreateSubTaskForCombat() const
{
	//Grab the target.
	const CPed* pTarget = GetTarget();

	//Create the sub-task.
	CTask* pTask = rage_new CTaskCombatAdditionalTask(
		CTaskCombatAdditionalTask::RT_Default | CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked | CTaskCombatAdditionalTask::RT_AllowAimFireToggle |
		CTaskCombatAdditionalTask::RT_AllowAimingOrFiringIfLosBlocked | CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions,
		pTarget, VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()));

	return pTask;
}

const CPed* CTaskTargetUnreachableInExterior::GetTarget() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}
	
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
	
	return static_cast<const CPed *>(pEntity);
}

bool CTaskTargetUnreachableInExterior::HasValidDesiredCover() const
{
	const CTaskCombat* pParentCombatTask = static_cast<const CTaskCombat*>(FindParentTaskOfType(CTaskTypes::TASK_COMBAT));
	if( pParentCombatTask )
	{
		return pParentCombatTask->HasValidatedDesiredCover();
	}

	// no valid desired cover by default
	return false;
}


void CTaskTargetUnreachableInExterior::ActivateTimeslicing()
{
	GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}
