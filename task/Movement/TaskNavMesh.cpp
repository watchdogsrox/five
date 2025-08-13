// Rage headers
#include "math/angmath.h"

// Framework headers
#include "fwmaths/Random.h"
#include "fwmaths/vector.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "PathServer/PathServer.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"

// If a ped has been tasked to go to exactly (0,0,0) this is very likely an error
#define __ASSERT_ON_GOING_TO_ORIGIN				1
// Pull back flee targets by some factor, along the vector from the fleeing ped to their flee target
// This improves consistency of flee paths, but is a temporary fix until this can be fixed within the pathfinder

AI_NAVIGATION_OPTIMISATIONS()
AI_OPTIMISATIONS()

#if __PS3
CompileTimeAssert( sizeof(CTaskMoveFollowNavMesh) <= 416 );
#endif

CClonedMoveFollowNavMeshInfo::CClonedMoveFollowNavMeshInfo(const float    moveBlendRatio,
                                                           const Vector3 &targetPos,
                                                           const float    targetRadius,
                                                           const int      time,
                                                           const unsigned flags,
                                                           const float    targetHeading) :
m_MoveBlendRatio(moveBlendRatio)
, m_TargetPos(targetPos)
, m_TargetRadius(targetRadius)
, m_Time(time)
, m_Flags(flags)
, m_TargetHeading(fwAngle::LimitRadianAngleSafe(targetHeading))
{
}

CClonedMoveFollowNavMeshInfo::CClonedMoveFollowNavMeshInfo() :
m_MoveBlendRatio(0.0f)
, m_TargetPos(VEC3_ZERO)
, m_TargetRadius(0.0f)
, m_Time(0)
, m_Flags(0)
, m_TargetHeading(0)
{
}

CTask *CClonedMoveFollowNavMeshInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    CTaskMoveFollowNavMesh *pTaskFollowNavMesh = rage_new CTaskMoveFollowNavMesh(m_MoveBlendRatio, m_TargetPos, m_TargetRadius, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, m_Time);

    if(pTaskFollowNavMesh)
    {
        pTaskFollowNavMesh->SetIsScriptedRoute(true);
		pTaskFollowNavMesh->SetTargetStopHeading(m_TargetHeading);

		pTaskFollowNavMesh->SetScriptBehaviourFlags(m_Flags, NULL);
    }

    float fMainTaskTimer = ((float)m_Time)/1000.0f;

    if(fMainTaskTimer > 0.0f)
    {
        fMainTaskTimer += 2.0f;
    }

    CTask *pTask = rage_new CTaskComplexControlMovement( pTaskFollowNavMesh, NULL, CTaskComplexControlMovement::TerminateOnMovementTask, fMainTaskTimer );

    return pTask;
}

const float CTaskMoveFollowNavMesh::ms_fTargetRadius						= 0.5f;
const float CTaskMoveFollowNavMesh::ms_fSlowDownDistance					= 3.0f;
const float CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius				= 1.0f;
dev_s32 CTaskMoveFollowNavMesh::ms_iReEvaluateInfluenceSpheresFrequency		= 10000;

u64 CFollowNavMeshFlags::GetAsPathServerFlags() const
{
	u64 iFlags = 0;

	if(IsSet(FNM_FleeFromTarget))
		iFlags |= PATH_FLAG_FLEE_TARGET;
	if(IsSet(FNM_GoAsFarAsPossibleIfNavmeshNotLoaded))
		iFlags |= PATH_FLAG_SELECT_CLOSEST_LOADED_NAVMESH_TO_TARGET;
	if(IsSet(FNM_PreferPavements))
		iFlags |= PATH_FLAG_PREFER_PAVEMENTS;
	if(IsSet(FNM_NeverLeavePavements))
		iFlags |= PATH_FLAG_NEVER_LEAVE_PAVEMENTS;
	if(IsSet(FNM_NeverEnterWater))
		iFlags |= PATH_FLAG_NEVER_ENTER_WATER;
	if(IsSet(FNM_NeverStartInWater))
		iFlags |= PATH_FLAG_NEVER_START_IN_WATER;
	if(IsSet(FNM_FleeNeverEndInWater))
		iFlags |= PATH_FLAG_FLEE_NEVER_END_IN_WATER;
	if(IsSet(FNM_AvoidTrainTracks))
		iFlags |= PATH_FLAG_AVOID_TRAIN_TRACKS;

	return iFlags;
}

#if __BANK
bool CTaskMoveFollowNavMesh::ms_bRenderDebug = true;
#endif

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskMoveFollowNavMesh, 0xa3c535d3)
CTaskMoveFollowNavMesh::Tunables CTaskMoveFollowNavMesh::sm_Tunables;

CTaskMoveFollowNavMesh::CTaskMoveFollowNavMesh(
	const float fMoveBlendRatio, 
	const Vector3 & vTarget,
	const float fTargetRadius,
	const float fSlowDownDistance,
	const int iTime,
	const bool UNUSED_PARAM(bUseBlending),
	const bool bFleeFromTarget,
	SpheresOfInfluenceBuilder* pfnSpheresOfInfluenceBuilder,
	float fPathCompletionRadius,
	bool bSlowDownAtEnd)
: CTaskNavBase(fMoveBlendRatio, vTarget)
, m_iFollowerTeleportPathRequestHandle(PATH_HANDLE_NULL)
{
	CNavParams navParams;
	navParams.m_vTarget = vTarget;
	navParams.m_fTargetRadius = fTargetRadius;
	navParams.m_fMoveBlendRatio = fMoveBlendRatio;
	navParams.m_iWarpTimeMs = iTime;
	navParams.m_bFleeFromTarget = bFleeFromTarget;
	navParams.m_fCompletionRadius = fPathCompletionRadius;
	navParams.m_fSlowDownDistance = bSlowDownAtEnd ? fSlowDownDistance : 0.0f;
	navParams.m_pInfluenceSphereCallbackFn = pfnSpheresOfInfluenceBuilder;

	Init(navParams);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
}

CTaskMoveFollowNavMesh::CTaskMoveFollowNavMesh(const CNavParams & navParams)
: CTaskNavBase(navParams.m_fMoveBlendRatio, navParams.m_vTarget)
, m_iFollowerTeleportPathRequestHandle(PATH_HANDLE_NULL)
{
	Init(navParams);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
}

void CTaskMoveFollowNavMesh::Init(const CNavParams & navParams)
{
#if __ASSERT_ON_GOING_TO_ORIGIN
	Assertf(!navParams.m_vTarget.IsClose(VEC3_ZERO, SMALL_FLOAT), "Ped has been tasked to %s the origin (0,0,0).  This is is very likely an error.", navParams.m_bFleeFromTarget ? "flee from" : "navigate to");
#endif

	// Init vars from params
	m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_FleeFromTarget, navParams.m_bFleeFromTarget);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontStopForClimbs, navParams.m_bFleeFromTarget);	// For now yes
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SmoothPathCorners);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_UseAdaptiveSetTargetEpsilon);
	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_TargetIsDestination, !navParams.m_bFleeFromTarget);

	m_fPathCompletionRadius = navParams.m_fCompletionRadius;
	m_fMoveBlendRatio = navParams.m_fMoveBlendRatio;
	m_fSlowDownDistance = navParams.m_fSlowDownDistance;
	m_fTargetRadius = navParams.m_fTargetRadius;
	m_iWarpTimeMs = navParams.m_iWarpTimeMs;
	m_fFleeSafeDist = navParams.m_fFleeSafeDistance;
	m_fEntityRadiusOverride = navParams.m_fEntityRadiusOverride;
	m_pInfluenceSphereBuilderFn = navParams.m_pInfluenceSphereCallbackFn;
	m_pCachedInfluenceSpheres = NULL;
	
	m_iNavMeshRouteResult = NAVMESHROUTE_ROUTE_NOT_YET_TRIED;
	m_iNavMeshRouteMethod = eRouteMethod_Normal;
	m_iNumRouteAttempts = 0;

	// Init other vars to default values
	m_iTimeToPauseBetweenSearchAttempts = 1000;
	m_iTimeToPauseBetweenRepeatSearches = 5000;
}

CTaskMoveFollowNavMesh::~CTaskMoveFollowNavMesh()
{
	if(m_pCachedInfluenceSpheres)
	{
		delete m_pCachedInfluenceSpheres;
		m_pCachedInfluenceSpheres = NULL;
	}
}

aiTask * CTaskMoveFollowNavMesh::Copy() const
{
	// TODO: use the new 'CNavParams' constructor
	CTaskMoveFollowNavMesh * pNewTask = rage_new CTaskMoveFollowNavMesh(
		m_fInitialMoveBlendRatio,
		m_vTarget,
		m_fTargetRadius,
		m_fSlowDownDistance,
		m_iWarpTimeMs,
		true,
		m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeFromTarget),
		m_pInfluenceSphereBuilderFn,
		m_fPathCompletionRadius,
		(m_fSlowDownDistance != 0.0f)
	);
	// TODO: Clone other flags here
	CopySettings(pNewTask);

	pNewTask->SetFleeSafeDist(GetFleeSafeDist());
	pNewTask->SetCompletionRadius(GetCompletionRadius());
	pNewTask->SetTimeToPauseBetweenSearchAttempts(GetTimeToPauseBetweenSearchAttempts());
	pNewTask->SetTimeToPauseBetweenRepeatSearches(GetTimeToPauseBetweenRepeatSearches());
	pNewTask->SetGoAsFarAsPossibleIfNavMeshNotLoaded(GetGoAsFarAsPossibleIfNavMeshNotLoaded());
	pNewTask->SetKeepToPavements(GetKeepToPavements());
	pNewTask->SetDontStandStillAtEnd(GetDontStandStillAtEnd());
	pNewTask->SetKeepMovingWhilstWaitingForPath(GetKeepMovingWhilstWaitingForPath(), m_fDistanceToMoveWhileWaitingForPath);
	pNewTask->SetAvoidObjects(GetAvoidObjects());
	pNewTask->SetAvoidPeds(GetAvoidPeds());
	pNewTask->SetNeverEnterWater(GetNeverEnterWater());
	pNewTask->SetNeverStartInWater(GetNeverStartInWater());
	pNewTask->SetFleeNeverEndInWater(GetFleeNeverEndInWater());
	pNewTask->SetGoStraightToCoordIfPathNotFound(GetGoStraightToCoordIfPathNotFound());
	pNewTask->SetQuitTaskIfRouteNotFound(GetQuitTaskIfRouteNotFound());
	pNewTask->SetSupressStartAnimations(GetSupressStartAnimations());
	pNewTask->SetPreserveHeightInformationInPath(GetPreserveHeightInformationInPath());
	pNewTask->SetTargetStopHeading(m_fTargetStopHeading);
	pNewTask->SetStopExactly(GetStopExactly());
	pNewTask->SetSlowDownInsteadOfUsingRunStops(GetSlowDownInsteadOfUsingRunStops());
	pNewTask->SetPullBackFleeTargetFromPed(GetPullBackFleeTargetFromPed());
	pNewTask->SetPolySearchForward(GetPolySearchForward());
	pNewTask->SetReEvaluateInfluenceSpheres(GetReEvaluateInfluenceSpheres());

	return pNewTask;
}

bool CTaskMoveFollowNavMesh::HasTarget() const
{
	if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeFromTarget))
	{
		return false;
	}
	else
	{
		return true;
	}
}
Vector3 CTaskMoveFollowNavMesh::GetTarget() const
{
	Assert(!m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeFromTarget));
	return m_vTarget;
}

bool CTaskMoveFollowNavMesh::IsFollowingNavMeshRoute() const
{
	return GetState()==NavBaseState_FollowingPath;
}

bool CTaskMoveFollowNavMesh::IsMovingWhilstWaitingForPath() const
{
	return GetState()==NavBaseState_MovingWhilstWaitingForPath;
}

bool CTaskMoveFollowNavMesh::IsWaitingToLeaveVehicle() const
{
	return GetState()==NavBaseState_WaitingToLeaveVehicle;
}

bool CTaskMoveFollowNavMesh::IsUsingLadder() const
{
	return GetState()==NavBaseState_UsingLadder;
}

bool CTaskMoveFollowNavMesh::IsUnableToFindRoute() const
{ 
	const bool bUnable = 
		!IsFollowingNavMeshRoute() &&
		m_iNumRouteAttempts >= CTaskMoveFollowNavMesh::eRouteMethod_NumMethods;
		//&& GetNavMeshRouteResult() == NAVMESHROUTE_ROUTE_NOT_FOUND;

	return bUnable;
}

s32 CTaskMoveFollowNavMesh::GetDefaultStateAfterAbort() const
{
	return CTaskNavBase::GetDefaultStateAfterAbort();
}

CTask::FSM_Return CTaskMoveFollowNavMesh::ProcessPreFSM()
{
	if(GetState()==NavBaseState_FollowingPath)
	{
		// This is to make chop able to nail a point better, possibly for all quadruped stuffs
		if (GetStopExactly() && GetPed()->GetCapsuleInfo()->IsQuadruped())
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);

		if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_ReEvaluateInfluenceSpheres))
		{
			if(m_ReEvaluateInfluenceSpheresTimer.IsOutOfTime())
			{
				m_ReEvaluateInfluenceSpheresTimer.Set( fwTimer::GetTimeInMilliseconds(), ms_iReEvaluateInfluenceSpheresFrequency );

				if(!ReEvaluateInfluenceSpheres(GetPed()))
				{
					m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_RestartNextFrame, true);
					m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_RestartKeepMoving, true);
				}
			}
		}
	}

	return CTaskNavBase::ProcessPreFSM();
}

CTask::FSM_Return CTaskMoveFollowNavMesh::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	switch(iState)
	{
		case State_WaitingBetweenSearchAttempts:
		case State_WaitingBetweenRepeatSearches:
		case State_SlidingToCoord:
		case State_GetOntoMainNavMesh:
		case State_AttemptTeleportToLeader:
		{
			FSM_Begin
				FSM_State(State_WaitingBetweenSearchAttempts)
					FSM_OnEnter
						return StateWaitingBetweenSearchAttempts_OnEnter(pPed);
					FSM_OnUpdate
						return StateWaitingBetweenSearchAttempts_OnUpdate(pPed);
				FSM_State(State_WaitingBetweenRepeatSearches)
					FSM_OnEnter
						return StateWaitingBetweenRepeatSearches_OnEnter(pPed);
					FSM_OnUpdate
						return StateWaitingBetweenRepeatSearches_OnUpdate(pPed);
				FSM_State(State_SlidingToCoord)
					FSM_OnEnter
						return StateSlidingToCoord_OnEnter(pPed);
					FSM_OnUpdate
						return StateSlidingToCoord_OnUpdate(pPed);
				FSM_State(State_AttemptTeleportToLeader)
					FSM_OnEnter
						return StateAttemptTeleportToLeader_OnEnter(pPed);
					FSM_OnUpdate
						return StateAttemptTeleportToLeader_OnUpdate(pPed);

			FSM_End
		}
		default:
		{
			return CTaskNavBase::UpdateFSM(iState, iEvent);
		}
	}
}

CTaskInfo *CTaskMoveFollowNavMesh::CreateQueriableState() const
{
    unsigned flags = 0;

	if(GetKeepMovingWhilstWaitingForPath() &&
	   GetDontStandStillAtEnd())
    {
        flags |= ENAV_NO_STOPPING;
    }

    if(GetGoAsFarAsPossibleIfNavMeshNotLoaded())
    {
        flags |= ENAV_GO_FAR_AS_POSSIBLE_IF_TARGET_NAVMESH_NOT_LOADED;
    }

    if(GetKeepToPavements())
    {
        flags |= ENAV_KEEP_TO_PAVEMENTS;
    }

    if(GetNeverEnterWater())
    {
        flags |= ENAV_NEVER_ENTER_WATER;
    }

    if(GetAvoidObjects())
    {
        flags |= ENAV_DONT_AVOID_OBJECTS;
    }

	if(GetAvoidPeds())
	{
		flags |= ENAV_DONT_AVOID_PEDS;
	}

    if(GetStopExactly())
    {
        flags |= ENAV_STOP_EXACTLY;
    }

    if(GetConsiderRunStartForPathLookAhead())
    {
        flags |= ENAV_ACCURATE_WALKRUN_START;
    }

    return rage_new CClonedMoveFollowNavMeshInfo(m_fInitialMoveBlendRatio, m_vTarget, m_fTargetRadius, m_iWarpTimeMs, flags, m_fTargetStopHeading);
}

CTask::FSM_Return CTaskMoveFollowNavMesh::StateWaitingBetweenSearchAttempts_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveStandStill * pStandStill = rage_new CTaskMoveStandStill(m_iTimeToPauseBetweenSearchAttempts);
	SetNewTask(pStandStill);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveFollowNavMesh::StateWaitingBetweenSearchAttempts_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(NavBaseState_Initial);
	}
	else
	{
		HandleNewTarget(pPed);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveFollowNavMesh::StateWaitingBetweenRepeatSearches_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveStandStill * pStandStill = rage_new CTaskMoveStandStill(m_iTimeToPauseBetweenRepeatSearches);
	SetNewTask(pStandStill);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveFollowNavMesh::StateWaitingBetweenRepeatSearches_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(NavBaseState_Initial);
	}
	else
	{
		HandleNewTarget(pPed);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveFollowNavMesh::StateSlidingToCoord_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveFollowNavMesh::StateSlidingToCoord_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveFollowNavMesh::StateAttemptTeleportToLeader_OnEnter(CPed* pPed)
{
	CTaskMoveStandStill * pStandStill = rage_new CTaskMoveStandStill(m_iTimeToPauseBetweenSearchAttempts);
	SetNewTask(pStandStill);

	// If we might need to teleport to the target, start a path request backwards from the target
	if (pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportIfCantReachPlayer) && m_iFollowerTeleportPathRequestHandle == PATH_HANDLE_NULL)
	{
		TRequestPathStruct pathRequest;
		InitPathRequestPathStruct(pPed, pathRequest);
		ModifyPathFlagsForPedLod(pPed, pathRequest.m_iFlags);

		// InitPathRequestPathStruct sets the PathStart to be pPed's position and PathEnd to be the target. We need to reverse those.
		pathRequest.m_vPathStart = m_vTarget;
		pathRequest.m_vPathEnd   = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// The pathRequest StartNavmeshAndPoly would also have been set to pPed's, which we need to change.
		//  Note this assumes we've only set CPED_CONFIG_FLAG_TeleportIfCantReachPlayer on a following ped in a group relationship.
		CPedGroup* pGroup = pPed->GetPedsGroup();
		if (aiVerify(pGroup))
		{
			CPedGroupMembership* pMembership = pGroup->GetGroupMembership();
			if (aiVerify(pMembership))
			{
				CPed* pLeader = pMembership->GetLeader();
				if (aiVerify(pLeader))
				{
					pathRequest.m_StartNavmeshAndPoly = pLeader->GetNavMeshTracker().GetNavMeshAndPoly();
				}
			}
		}
		m_iFollowerTeleportPathRequestHandle = CPathServer::RequestPath(pathRequest);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveFollowNavMesh::StateAttemptTeleportToLeader_OnUpdate(CPed* pPed)
{
	if (CanPedTeleportToPlayer(pPed, ShouldTryToGetOnNavmesh(pPed)))
	{
		if (m_iFollowerTeleportPathRequestHandle != PATH_HANDLE_NULL)
		{
			EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_iFollowerTeleportPathRequestHandle, EPath);
			if (ret == ERequest_Ready)
			{
				int										iNumPoints = 0;
				Vector3*							pPoints = NULL;
				TNavMeshWaypointFlag* pWaypointFlags = NULL;
				TPathResultInfo				pathResultInfo;

				const int iPathReturnValue = CPathServer::LockPathResult(m_iFollowerTeleportPathRequestHandle, &iNumPoints, pPoints, pWaypointFlags, &pathResultInfo);
				aiAssert(iPathReturnValue != PATH_STILL_PENDING); // We should have just checked this.

				if (iPathReturnValue == PATH_FOUND || iPathReturnValue == PATH_NOT_FOUND)
				{
					bool bTeleported = false;
					Vector3 vTeleportLocation = pathResultInfo.m_vClosestPointFoundToTarget;
					if (pPed->GetPedType() == PEDTYPE_ANIMAL)
					{
						const u32 iFlags = GetClosestPos_ClearOfObjects | GetClosestPos_NotWater | GetClosestPos_OnlyNonIsolated;
						const float fMaxSearchRadius = 5.f;
						if (ENoPositionFound != CPathServer::GetClosestPositionForPed(pathResultInfo.m_vClosestPointFoundToTarget, vTeleportLocation, fMaxSearchRadius, iFlags))
						{
							bTeleported = TeleportFollowerToLeaderReversePath(pPed, vTeleportLocation);
						}
					}
					else 
					{
						bTeleported = TeleportFollowerToLeaderReversePath(pPed, vTeleportLocation);
					}

					if (bTeleported)
					{
						// Reset our count of attempted pathing requests.
						m_iNumRouteAttempts = 0;
					}
				}

				CPathServer::UnlockPathResultAndClear(m_iFollowerTeleportPathRequestHandle);

				if (GetSubTask())
				{
					const aiEvent* abortEvent = NULL;
					GetSubTask()->MakeAbortable(ABORT_PRIORITY_IMMEDIATE, abortEvent);
				}
			}
		}
	}

	// Our MoveStandStill subtask acts as a timeout should the pathing request or the teleport fail.
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(NavBaseState_Initial);
		m_iFollowerTeleportPathRequestHandle = PATH_HANDLE_NULL;
	}
	return FSM_Continue;
}

// Initialise the structure which is passed into the CPathServer::RequestPath() function
void CTaskMoveFollowNavMesh::InitPathRequestPathStruct(CPed * pPed, TRequestPathStruct & reqPathStruct, const float fDistanceAheadOfPed)
{
	reqPathStruct.m_iFlags = pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags();

	if(pPed->m_nFlags.bIsOnFire)
		reqPathStruct.m_iFlags |= PATH_FLAG_DONT_AVOID_FIRE;

	reqPathStruct.m_iFlags |= m_NavBaseConfigFlags.GetAsPathServerFlags();
	reqPathStruct.m_iFlags |= m_FollowNavMeshFlags.GetAsPathServerFlags();

	Assertf(!(reqPathStruct.m_iFlags & PATH_FLAG_AVOID_TRAIN_TRACKS), "Be aware when using FNM_AvoidTrainTracks that train-track polygons are a huge overestimate of the actual tracks; and the heuristic is only tuned for peds who are fleeing when already on pavement.");

	// Switch on train-track avoidance in very specific circumstances:
	// This is an ambient fleeing ped who is standing on pavement, and who wishes to remain on pavement, and who is in an interior.
	if( GetIsFleeing() && pPed->PopTypeIsRandom() && pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement
		&& pPed->GetPortalTracker()->IsInsideInterior()
		&& ( m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PreferPavements) || m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_NeverLeavePavements) ) )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_AVOID_TRAIN_TRACKS;
	}

	reqPathStruct.m_iFlags |= PATH_FLAG_PRESERVE_SLOPE_INFO_IN_PATH;

	// by default this flag is set in this task
	// (although it is not currently activated within the pathserver)
	reqPathStruct.m_iFlags |= PATH_FLAG_DEACTIVATE_OBJECTS_IF_CANT_RESOLVE_ENDPOINTS;

	switch(m_iNavMeshRouteMethod)
	{
		case eRouteMethod_ReducedObjectBoundingBoxes:
			reqPathStruct.m_iFlags |= PATH_FLAG_REDUCE_OBJECT_BBOXES;
			break;
		case eRouteMethod_OnlyAvoidNonMovableObjects:
			reqPathStruct.m_iFlags |= PATH_FLAG_IGNORE_NON_SIGNIFICANT_OBJECTS;
			break;
		case eRouteMethod_Normal:
		default:
			break;
	}

	// If we have failed to find a path on all our attempts, then we enable paths to travel up
	// steep slopes on our subsequent attempts.
	if(m_iNumRouteAttempts >= eRouteMethod_NumMethods)
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_ALLOW_TO_NAVIGATE_UP_STEEP_POLYGONS;
	}

	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_RunFromFiresAndExplosions ) )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_AVOID_POTENTIAL_EXPLOSIONS;
	}

	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AvoidTearGas ) )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_AVOID_TEAR_GAS;
	}

	if( pPed->PopTypeIsMission() )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_MISSION_PED;
	}

	if( m_fMoveBlendRatio < MBR_RUN_BOUNDARY )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_SOFTER_FLEE_HEURISTICS;
	}

	reqPathStruct.m_fCompletionRadius = m_fPathCompletionRadius;
	reqPathStruct.m_fMaxSlopeNavigable = m_fMaxSlopeNavigable;
	reqPathStruct.m_fClampMaxSearchDistance = GetClampMaxSearchDistance();
	reqPathStruct.m_fMaxDistanceToAdjustPathStart = GetMaxDistanceMayAdjustPathStartPosition();
	reqPathStruct.m_fMaxDistanceToAdjustPathEnd = GetMaxDistanceMayAdjustPathEndPosition();
	reqPathStruct.m_fReferenceDistance = m_fFleeSafeDist;
	reqPathStruct.m_iFreeSpaceReferenceValue = m_iFreeSpaceReferenceValue;

	reqPathStruct.m_iNumInfluenceSpheres = 0;
	if(m_pInfluenceSphereBuilderFn)
	{
		m_pInfluenceSphereBuilderFn(reqPathStruct.m_InfluenceSpheres, reqPathStruct.m_iNumInfluenceSpheres, pPed);

		if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_ReEvaluateInfluenceSpheres))
		{
			if(!m_pCachedInfluenceSpheres)
				m_pCachedInfluenceSpheres = rage_new CCachedInfluenceSpheres();

			if(m_pCachedInfluenceSpheres)
			{
				m_pCachedInfluenceSpheres->m_iNumSpheres = reqPathStruct.m_iNumInfluenceSpheres;
				memcpy(&m_pCachedInfluenceSpheres->m_Spheres, reqPathStruct.m_InfluenceSpheres, sizeof(TInfluenceSphere)*reqPathStruct.m_iNumInfluenceSpheres);
			}

			m_ReEvaluateInfluenceSpheresTimer.Set( fwTimer::GetTimeInMilliseconds(), ms_iReEvaluateInfluenceSpheresFrequency );
		}
	}

	reqPathStruct.m_pPed = pPed;
	reqPathStruct.m_vCoverOrigin = Vector3(m_vDirectionalCoverThreatPos[0], m_vDirectionalCoverThreatPos[1], 0.0f);
	reqPathStruct.m_pEntityPathIsOn = NULL;

	if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PolySearchForward))
		reqPathStruct.m_vPolySearchDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

	if(pPed->GetGroundPhysical())
	{
		if(pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->m_nVehicleFlags.bHasDynamicNavMesh)
		{
			reqPathStruct.m_pEntityPathIsOn = pPed->GetGroundPhysical();
			reqPathStruct.m_iFlags |= PATH_FLAG_DYNAMIC_NAVMESH_ROUTE;
		}
	}

	bool bIsExitingForPosition = false;
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// If we have passed a distance ahead of the ped, then the path will start from this distance along the current path
	// It implies we are calculating the path on the fly whilst following our current existing path.
	if(fDistanceAheadOfPed > 0.0f)
	{
		InitPathRequest_DistanceAheadCalculation(fDistanceAheadOfPed, pPed, reqPathStruct);
	}
	//else if( GetState()==NavBaseState_FollowingPath && !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastRoutePointIsTarget) && m_pRoute && m_pRoute->GetSize() > 0 && m_iProgress == m_pRoute->GetSize())
	else if( GetState()==NavBaseState_FollowingPath && !GetLastRoutePointIsTarget() && m_pRoute && m_pRoute->GetSize() > 0 && m_iProgress == m_pRoute->GetSize())
	{
		reqPathStruct.m_vPathStart = m_pRoute->GetPosition( m_pRoute->GetSize()-1 );
	}
	else
	{
		Vec3V vExitPosition;
		if(IsExitingVehicle(vExitPosition) || IsExitingScenario(vExitPosition))
		{
			bIsExitingForPosition = true;
			reqPathStruct.m_vPathStart = VEC3V_TO_VECTOR3(vExitPosition);
		}
		else
		{
#if __TRACK_PEDS_IN_NAVMESH
			if(pPed->GetNavMeshTracker().IsUpToDate(vPedPosition))
			{
				reqPathStruct.m_vPathStart = pPed->GetNavMeshTracker().GetLastPosition();
				reqPathStruct.m_StartNavmeshAndPoly = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
			}
			else
			{
				reqPathStruct.m_vPathStart = vPedPosition;
			}
#else
			reqPathStruct.m_vPathStart = vPedPosition;
#endif
		}
	}

	// This chunk is to make it less likely to end up on the other side of a thin wall due to dynamic objects
	if (!pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().IsLastNavMeshIntersectionValid())
	{
		Vector3 vNavDiff = reqPathStruct.m_vPathStart - pPed->GetNavMeshTracker().GetLastNavMeshIntersection();
		if (vNavDiff.XYMag2() < 0.3f * 0.3f)
		{
			reqPathStruct.m_vPathStart = pPed->GetNavMeshTracker().GetLastNavMeshIntersection();
			reqPathStruct.m_vPathStart.z += 1.0f;
		}
	}

	if(m_fEntityRadiusOverride < 0.0f)
	{
		reqPathStruct.m_fEntityRadius = pPed->GetCapsuleInfo()->GetHalfWidth();
	}
	else
	{
		reqPathStruct.m_fEntityRadius = m_fEntityRadiusOverride;
	}

#if __BANK
	if(CTaskNavBase::ms_fOverrideRadius != PATHSERVER_PED_RADIUS)
		reqPathStruct.m_fEntityRadius = ms_fOverrideRadius;
#endif

	Vector3 vTarget = m_vTarget;
	if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeFromTarget))
	{
		if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PullBackFleeTargetFromPed))
		{
			static dev_float fPullBackDist = 20.0f;
			Vector3 vPedToTarget = vTarget - vPedPosition;
			if(vPedToTarget.XYMag2() > SMALL_FLOAT)
			{
				vPedToTarget.Normalize();
				vTarget += vPedToTarget * fPullBackDist;
				reqPathStruct.m_fReferenceDistance += fPullBackDist;
			}
		}
		else	// For now, otherwise we would need to store more data and some more logic, right now, we would end up here most of the times
		{
			CTask* pTask = pPed->GetPedIntelligence()->GetTaskActive();
			if (pTask)
				reqPathStruct.m_pPositionEntity = pTask->GetTargetPositionEntity();

			// It is important that we don't mess up the startposition in these cases
			if (!bIsExitingForPosition && fDistanceAheadOfPed < ms_fDistanceAheadForSetNewTargetObstruction - 1.f)	// Scan for obstructions got 4 - 1 meter
				reqPathStruct.m_iFlags |= PATH_FLAG_KEEP_UPDATING_PED_START_POSITION;
		}
	}

	// If this is the player ped, or is in the same group as the player - set as a higher priority route
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		if(pPed == pPlayer || (pPed->GetPedGroupIndex() != PEDGROUP_INDEX_NONE && pPed->GetPedGroupIndex()==pPlayer->GetPedGroupIndex()))
		{
			reqPathStruct.m_iFlags |= PATH_FLAG_HIGH_PRIO_ROUTE;
		}
	}

	reqPathStruct.m_fDistAheadOfPed = fDistanceAheadOfPed;

	reqPathStruct.m_bIgnoreTypeObjects = (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeObject)==false);
	reqPathStruct.m_bIgnoreTypeVehicles = (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeVehicle)==false);

	reqPathStruct.m_vPathEnd = vTarget;
	reqPathStruct.m_vReferenceVector = vTarget;
}

float CTaskMoveFollowNavMesh::GetRouteDistanceFromEndForNextRequest() const
{
	if(GetIsFleeing() && m_pRoute && m_pRoute->GetSize())
	{
		// Find the vector from the flee-from position (m_vTarget) to the final point on our route
		Vector3 vDelta = m_pRoute->GetPosition(m_pRoute->GetSize()-1) - m_vTarget;
		// If flee target has not achieved the desired distance, set the task to request another route as it approached the end
		if(vDelta.Mag2() < square(GetFleeSafeDist()))
		{
			return ms_fDistanceFromRouteEndToRequestNextSection;
		}
	}
	return 0.0f;
}

float CTaskMoveFollowNavMesh::GetDistanceAheadForSetNewTarget() const
{
	static dev_float fFleeDistanceAhead = 1.5f;
	if(GetIsFleeing())
	{
		return fFleeDistanceAhead;
	}
	else
	{
		return CTaskNavBase::GetDistanceAheadForSetNewTarget();
	}
}

bool CTaskMoveFollowNavMesh::OnSuccessfulRoute(CPed * pPed)
{
	m_iNumRouteAttempts = 0;
	SetPolySearchForward(false);
	m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_RestartKeepMoving, false);

	CTaskNavBase::OnSuccessfulRoute(pPed);

	return true;
}

CTask::FSM_Return CTaskMoveFollowNavMesh::OnFailedRoute(CPed * pPed, const TPathResultInfo & UNUSED_PARAM(pathResultInfo))
{
#if __DEV
	if( pPed->GetPedIntelligence()->m_bAssertIfRouteNotFound )
	{
		if( m_iNavMeshRouteMethod >= eRouteMethod_ReducedObjectBoundingBoxes )
		{
			Assertf(false, "ped 0x%p at (%.1f, %.1f, %.1f) was unable to find a route with object avoidance enabled", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
		}
	}
#endif

	m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_RestartKeepMoving, false);

	m_iNumRouteAttempts++;
	/* if(m_iNumRouteAttempts >= 65535)	// This is impossible, m_iNumRouteAttempts is a u8, it will wrap at 255
		m_iNumRouteAttempts = 0; */

	if(pPed->GetVehiclePedInside())
	{
		m_iNavMeshRouteMethod = eRouteMethod_Normal;
		SetState(State_WaitingBetweenRepeatSearches);
		return FSM_Continue;
	}

	//--------------------------------------------------------------------------------------------

	m_iNavMeshRouteMethod++;

	// Have we tried all RouteMethods on our search? [2/12/2013 mdawe]
	if(m_iNavMeshRouteMethod >= eRouteMethod_NumMethods )
	{
		m_iNavMeshRouteMethod = eRouteMethod_Normal;

		if(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_QuitTaskIfRouteNotFound))
			return FSM_Quit;

		// TODO: Handle 'FNM_GoStraightToTargetIfPathNotFound' here

		//----------------------------------------------------------------------------------------
		// Recovery code - try to get stranded ped onto, or close to, a patch of the main navmesh

		bool bTryToGetOnNavmesh = ShouldTryToGetOnNavmesh(pPed) && (m_pOnEntityRouteData == NULL);

		if (CanPedTeleportToPlayer(pPed, bTryToGetOnNavmesh))
		{
			SetState(State_AttemptTeleportToLeader);
		}
		// Check to see if we should be trying to get back onto the main navmesh. [2/12/2013 mdawe]
		else if (bTryToGetOnNavmesh)
		{
			SetState(NavBaseState_GetOntoMainNavMesh);
		}
		else
		{
			SetState(State_WaitingBetweenRepeatSearches);
		}
	}
	else
	{
		SetState(State_WaitingBetweenSearchAttempts);		
	}

	return FSM_Continue;
}

bool CTaskMoveFollowNavMesh::ShouldTryToGetOnNavmesh( CPed * pPed )
{
	return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_TryToGetOntoMainNavMeshIfStranded) &&
		// Cannot in low LOD physics as we have no collision to prevent us moving through map/objects
		(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics)==false) &&
		// Doing anything other that this basic query on the existence of the navmesh pointer, would be very dangerous from a threading standpoint
		CPathServer::GetNavMeshFromIndex( CPathServer::GetNavMeshIndexFromPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), kNavDomainRegular), kNavDomainRegular);
}



bool CTaskMoveFollowNavMesh::ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const
{
	bMayUsePreviousTarget = true;

	if( m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_RestartKeepMoving) )
	{
		return true;
	}

	// Could this be the scenario stopping?
	if(m_iNumRouteAttempts > 0)
		return false;

	if (!IsPedMoving(pPed))
		return false;

	if( GetState()==NavBaseState_FollowingPath && m_pRoute &&
		(GetLastRoutePointIsTarget()==false) &&
		//(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastRoutePointIsTarget)==false) &&
		m_iProgress == m_pRoute->GetSize())
		return true;

	// If this task is in a sequence of movement tasks, then avoid pausing between them
	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_GotoTaskSequencedBefore))
		return true;

	if(GetState()==NavBaseState_FollowingPath && GetSubTask() &&
		(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
		GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL ||
		(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT && m_pRoute && m_iProgress==m_pRoute->GetSize()-1) ) )
	{
		return true;
	}

	// Base class checks the "CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath" flag
	if(CTaskNavBase::ShouldKeepMovingWhilstWaitingForPath(pPed, bMayUsePreviousTarget))
	{
		return true;
	}

	return false;
}

bool CTaskMoveFollowNavMesh::HandleNewTarget(CPed * pPed)
{
	if(GetState()<NavBaseState_NumStates)
	{
		return CTaskNavBase::HandleNewTarget(pPed);
	}

	if(!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HadNewTarget) || m_iPathHandle != PATH_HANDLE_NULL)
		return false;

	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_HadNewTarget);
	m_vTarget = m_vNewTarget;

	BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : CTaskMoveFollowNavMesh, Ped 0x%p inside HandleNewTarget; task state was %i", fwTimer::GetFrameCount(), pPed, GetState()); )

	RequestPath(pPed);

	if(GetState()==State_WaitingBetweenSearchAttempts || GetState()==State_WaitingBetweenRepeatSearches)
	{
		SetState(NavBaseState_StationaryWhilstWaitingForPath);
	}

	return true;
}

u32 CTaskMoveFollowNavMesh::GetInitialScanForObstructionsTime()
{
	static dev_u32 iFledFromVehicleTime = 250;

	if( GetIsFleeing() && m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_StartedFromInVehicle) )
	{
		return iFledFromVehicleTime;
	}
	else
	{
		return CTaskNavBase::GetInitialScanForObstructionsTime();
	}
}

// NAME : ReEvaluateInfluenceSpheres
// PURPOSE : Determine whether influence spheres are still valid; a return value of false indicate they are no longer valid
bool CTaskMoveFollowNavMesh::ReEvaluateInfluenceSpheres(CPed * pPed) const
{
	Assert(m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_ReEvaluateInfluenceSpheres));
	Assert(m_pInfluenceSphereBuilderFn);

	static dev_float fDistanceEps = 4.0f;
	static dev_float fRadiusEps = 4.0f;
	static dev_float fWeightingEps = 10.0f;

	if(m_pCachedInfluenceSpheres)
	{
		TInfluenceSphere newSpheres[MAX_NUM_INFLUENCE_SPHERES];
		s32 iNumNewSpheres;
		m_pInfluenceSphereBuilderFn(newSpheres, iNumNewSpheres, pPed);

		// Num spheres differs, so we definitely need to repath
		if(iNumNewSpheres != m_pCachedInfluenceSpheres->m_iNumSpheres)
			return false;

		Assertf(MAX_NUM_INFLUENCE_SPHERES < 32, "cannot use a u32 bitfield here any more");

		u32 iCachedMatches = 0;
		s32 iNumMatches = 0;

		for(s32 s1=0; s1<iNumNewSpheres; s1++)
		{
			TInfluenceSphere & newSphere = newSpheres[s1];

			for(s32 s2=0; s2<m_pCachedInfluenceSpheres->m_iNumSpheres; s2++)
			{
				// Only match any cached sphere once
				if( (iCachedMatches & (1 << s2)) == 0)
				{
					TInfluenceSphere & cachedSphere = m_pCachedInfluenceSpheres->m_Spheres[s2];

					if(newSphere.GetOrigin().IsClose(cachedSphere.GetOrigin(), fDistanceEps))
					{
						if(IsClose(newSphere.GetRadius(), cachedSphere.GetRadius(), fRadiusEps))
						{
							if(IsClose(newSphere.GetInnerWeighting(), cachedSphere.GetInnerWeighting(), fWeightingEps))
							{
								if(IsClose(newSphere.GetOuterWeighting(), cachedSphere.GetOuterWeighting(), fWeightingEps))
								{
									iCachedMatches |= (1 << s2);
									iNumMatches++;
								}
							}
						}
					}
				}
			}
		}

		return (iNumMatches == iNumNewSpheres);
	}

	return true;
}

// Attempts to teleport pPed to vPointToTeleportTo if that point is currently offscreen.
bool CTaskMoveFollowNavMesh::TeleportFollowerToLeaderReversePath(CPed* pPed, const Vector3& vPointToTeleportTo) const
{
	if (aiVerify(pPed))
	{
		aiAssert(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportIfCantReachPlayer));
		aiAssert(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen));

#if __DEV
		const bool bDebugDrawTeleportLocation = false;
		if (bDebugDrawTeleportLocation)
		{
			const float fRadius = 2.0f;
			const bool bSolid = false;
			const int  iFramesToLive = 66;
			grcDebugDraw::Sphere(vPointToTeleportTo, fRadius, Color_LightSlateGrey, bSolid, iFramesToLive);
		}
#endif // __DEV

		// Ensure vPointToReleportTo is offscreen
		//  First get the viewport
		CViewport* gameViewport = gVpMan.GetGameViewport();
		grcViewport& theViewport = gameViewport->GetGrcViewport();

		// Set up the sphere representing our teleport point
		const float fSphereRadius = 1.0f;
		Vec4V vSphere;
		vSphere.SetXYZ(VECTOR3_TO_VEC3V(vPointToTeleportTo));
		vSphere.SetW(fSphereRadius);
		
		// If the sphere is offscreen
		if (cullOutside == theViewport.GetSphereCullStatus(vSphere))
		{
			const bool bKeepTasks = true;
			pPed->Teleport(vPointToTeleportTo, pPed->GetDesiredHeading(), bKeepTasks);
			return true;
		}
	}
	return false;
}

bool CTaskMoveFollowNavMesh::CanPedTeleportToPlayer(CPed* pPed, bool bTryingToGetBackOnNavmesh) const
{
	const bool bHasNeededPedFlags = pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportIfCantReachPlayer) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen);
	const bool bMadeAttempts = m_iNumRouteAttempts >= sm_Tunables.uRepeatedAttemptsBeforeTeleportToLeader;
	const bool bHasGroupLeader = (pPed->GetPedsGroup() && pPed->GetPedsGroup()->GetGroupMembership() && pPed->GetPedsGroup()->GetGroupMembership()->GetLeader());

	return ( bHasNeededPedFlags && bHasGroupLeader && (bMadeAttempts || bTryingToGetBackOnNavmesh) );
}


// Set behaviour flags as passed in from script
void CTaskMoveFollowNavMesh::SetScriptBehaviourFlags(const int iScriptFlags, TNavMeshScriptStruct * pScriptStruct)
{
	//******************************************************************
	// ENAV_NO_STOPPING
	// Will ensure the ped continues to move whilst waiting for the path
	// to be found, and will not slow down at the end of their route.
	if(iScriptFlags & ENAV_NO_STOPPING)
	{
		SetKeepMovingWhilstWaitingForPath(true);
		SetDontStandStillAtEnd(true);
	}
	//*********************************************************************
	// ENAV_GO_FAR_AS_POSSIBLE_IF_TARGET_NAVMESH_NOT_LOADED
	// If the navmesh is not loaded in under the vTarget, then this will
	// cause the ped to get as close as is possible on whatever navmesh
	// is loaded.  The navmesh must still be loaded at the path start.
	if(iScriptFlags & ENAV_GO_FAR_AS_POSSIBLE_IF_TARGET_NAVMESH_NOT_LOADED)
	{
		SetGoAsFarAsPossibleIfNavMeshNotLoaded(true);
	}

	//*************************************************************************
	// ENAV_KEEP_TO_PAVEMENTS
	// Will only allow navigation on pavements.  If the path starts or ends
	// off the pavement, the commands will likely fail and the ped will remain
	// still.  Likewise if not pavement-only route can be found.
	if(iScriptFlags & ENAV_KEEP_TO_PAVEMENTS)
	{
		SetKeepToPavements(true);
	}
	//******************************************************
	// ENAV_NEVER_ENTER_WATER
	// Prevents the path from entering water at all
	if(iScriptFlags & ENAV_NEVER_ENTER_WATER)
	{
		SetNeverEnterWater(true);
	}
	//****************************************************
	// ENAV_DONT_AVOID_OBJECTS
	// Disables object-avoidance for this path
	if(iScriptFlags & ENAV_DONT_AVOID_OBJECTS)
	{
		SetAvoidObjects(false);
	}

	//****************************************************
	// ENAV_DONT_AVOID_PEDS
	// Disables ped-avoidance for this path
	if(iScriptFlags & ENAV_DONT_AVOID_PEDS)
	{
		SetAvoidPeds(false);
	}

	//**********************************************************************************************
	// ENAV_STOP_EXACTLY - DEPRACATED
	// Peds will always stop exactly unless ENAV_SUPPRESS_EXACT_STOP is specified.
	if(iScriptFlags & ENAV_STOP_EXACTLY)
	{
		//TODO: Make this assert, and eventually remove
	}

	//**********************************************************************************************
	// ENAV_SUPPRESS_EXACT_STOP
	// Disables the exact-stopping behaviour
	if(iScriptFlags & ENAV_SUPPRESS_EXACT_STOP)
	{
		SetStopExactly(false);
	}

	//**********************************************************************************************
	// ENAV_DONT_ADJUST_TARGET_POSITION
	// If target pos is inside the boundingbox of an object it will otherwise be pushed out
	if(iScriptFlags & ENAV_DONT_ADJUST_TARGET_POSITION)
	{
		SetDontAdjustTargetPosition(true);
	}

	//**********************************************************************************************
	// ENAV_ACCURATE_WALKRUN_START
	// 
    if(iScriptFlags & ENAV_ACCURATE_WALKRUN_START)
    {
		SetConsiderRunStartForPathLookAhead(true);
    }

	//******************************************************************************
	// ENAV_SUPRESS_START_ANIMATION
	// Prevents peds from playing their walkstart animation.  This is useful
	// if there's a momentary gap between movement tasks, where causing the ped to
	// invoke a walkstart would look bad.
	if(iScriptFlags & ENAV_SUPRESS_START_ANIMATION)
	{
		SetSupressStartAnimations(true);
	}

	if(iScriptFlags & ENAV_PULL_FROM_EDGE_EXTRA)
	{
		SetPullFromEdgeExtra(true);
	}

	//*****************************************************************************
	// The following flags require the TNavMeshScriptStruct struct to be passed in


	//*****************************************************************
	// ENAV_SLIDE_TO_COORD_AND_ACHIEVE_HEADING_AT_END
	// Performs a CTaskSimpleMoveSlideToCoord at the end of the route.
	// This requires that the following structure members are valid:
	//    > m_fSlideToCoordHeading
	if(iScriptFlags & ENAV_ADVANCED_SLIDE_TO_COORD_AND_ACHIEVE_HEADING_AT_END)
	{
		Assertf(pScriptStruct, "Using ENAV_ADVANCED_SLIDE_TO_COORD_AND_ACHIEVE_HEADING_AT_END requires a nav struct to be passed in.");
		if(pScriptStruct)
		{
			const float fHdg = fwAngle::LimitRadianAngle(( DtoR * pScriptStruct->m_fSlideToCoordHeading));
			SetSlideToCoordAtEnd(true, fHdg);
		}
	}
	//*******************************************************************
	// ENAV_USE_MAX_SLOPE_NAVIGABLE_VALUE
	// Prevents paths from going up slopes greater than this steepness
	if(iScriptFlags & ENAV_ADVANCED_USE_MAX_SLOPE_NAVIGABLE)
	{
		Assertf(pScriptStruct, "Using ENAV_ADVANCED_USE_MAX_SLOPE_NAVIGABLE requires a nav struct to be passed in.");
		if(pScriptStruct)
		{
			const float fAngle = DtoR * pScriptStruct->m_fMaxSlopeNavigable;
			SetMaxSlopeNavigable(fAngle);
		}
	}

	//*******************************************************************
	if(iScriptFlags & ENAV_ADVANCED_USE_CLAMP_MAX_SEARCH_DISTANCE)
	{
		Assertf(pScriptStruct, "Using ENAV_ADVANCED_USE_CLAMP_MAX_SEARCH_DISTANCE requires a nav struct to be passed in.");
		if(pScriptStruct)
		{
			Assertf(pScriptStruct->m_fClampMaxSearchDistance != 0.0f, "Using ENAV_ADVANCED_USE_CLAMP_MAX_SEARCH_DISTANCE requires a non-zero max search distance.");
			Assertf(pScriptStruct->m_fClampMaxSearchDistance > 0.0f && pScriptStruct->m_fClampMaxSearchDistance <= 255.0f, "Using ENAV_ADVANCED_USE_CLAMP_MAX_SEARCH_DISTANCE requires a positive value <= 255");

			SetClampMaxSearchDistance(pScriptStruct->m_fClampMaxSearchDistance);
		}
	}
}

#if !__FINAL 
void CTaskMoveFollowNavMesh::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Sphere(m_vTarget, m_fTargetRadius, Color_blue, false);

	if(m_pRoute)
	{
		static const Vector3 vAdjustZ(0.0f,0.0f,-0.75f);
		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, 0, m_pRoute->GetSize());

		for(int i=0; i<ms_iNumExpandedRouteVerts-1; i++)
		{
			Color32 col = (i < m_iClosestSegOnExpandedRoute) ? Color_cyan4 : Color_cyan;
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[i].GetPos()) + vAdjustZ, VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[i+1].GetPos()) + vAdjustZ, col, col);
		}
	}
#endif
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

#if !__FINAL && DEBUG_DRAW
void CTaskMoveFollowNavMesh::DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iOffsetY) const
{
	CTaskNavBase::DrawPedText(pPed, vWorldCoords, iTxtCol, iOffsetY);

	char taskText[256];

	//*************************
	// Num attempts

	sprintf(taskText, "NumAttempts: %i", m_iNumRouteAttempts);
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY += grcDebugDraw::GetScreenSpaceTextHeight();

	//******************************************
	// Route-finding method

	switch(m_iNavMeshRouteMethod)
	{
		case eRouteMethod_Normal:
			sprintf(taskText, "RouteMethod: normal");
			break;
		case eRouteMethod_ReducedObjectBoundingBoxes:
			sprintf(taskText, "RouteMethod: reduced bboxes");
			break;
		case eRouteMethod_OnlyAvoidNonMovableObjects:
			sprintf(taskText, "RouteMethod: only avoid non-uprootable");
			break;
		default:
			Assert(0);
			sprintf(taskText, "RouteMethod: undefined");
			break;
	}
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY += grcDebugDraw::GetScreenSpaceTextHeight();
}
#endif

















//***************************************************************
//	CTaskMoveWaitForNavMeshSpecialActionEvent
//	Very basic task which just waits until the event arrives


const int CTaskMoveWaitForNavMeshSpecialActionEvent::ms_iStandStillTime = 4000;

CTaskMoveWaitForNavMeshSpecialActionEvent::CTaskMoveWaitForNavMeshSpecialActionEvent(const int iDuration, const bool bForever) :
	CTaskMoveStandStill(iDuration, bForever)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WAIT_FOR_NAVMESH_SPECIAL_ACTION_EVENT);
}

CTaskMoveWaitForNavMeshSpecialActionEvent::~CTaskMoveWaitForNavMeshSpecialActionEvent()
{

}

// Helper function for putting climb/drop-down positions into world-space
void TransformPositionAndHeading(Vector3 & vPos, float & fHeading, const Matrix34 & mat)
{
	mat.Transform(vPos);
	Vector3 vHeading(-rage::Sinf(fHeading), rage::Cosf(fHeading), 0.0f);
	mat.Transform3x3(vHeading);
	vHeading.z = 0.0f;
	fHeading = fwAngle::GetRadianAngleBetweenPoints(vHeading.x, vHeading.y, 0.0f, 0.0f);
}

//********************************************************************************
//	CTaskUseClimbOnRoute
//	If this climb is taking place on a dynamic entity, then the vClimbPos and the
//	heading are in local-space & must be transformed by the entity matrix.

const s32 CTaskUseClimbOnRoute::ms_iDefaultTimeOutMs = 10000;

CTaskUseClimbOnRoute::CTaskUseClimbOnRoute(const float fMoveBlendRatio, const Vector3 & vClimbPos, const float fClimbHeading, const Vector3 & vClimbTarget, CEntity * pEntityRouteIsOn, const s32 iWarpTimer, const Vector3 & vWarpTarget, bool bForceJump) :
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_vClimbPos(vClimbPos),
	m_vClimbTarget(vClimbTarget),
	m_fClimbHeading(fClimbHeading),
	m_pEntityRouteIsOn(pEntityRouteIsOn),
	m_iWarpTimer(iWarpTimer),
	m_vWarpTarget(vWarpTarget),
	m_iFramesActive(0),
	m_bUseFasterClimb(false),
	m_bTryAvoidSlide(false),
	m_bAttemptedSecondClimb(false),
	m_bForceJump(bForceJump)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_CLIMB_ON_ROUTE);

	if(m_iWarpTimer >= 0)
		m_TimeOutTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iWarpTimer);
	else
		m_TimeOutTimer.Set(fwTimer::GetTimeInMilliseconds(), 10000);

	m_InitialNavMeshUnderfoot.Reset();
}

CTaskUseClimbOnRoute::~CTaskUseClimbOnRoute()
{
}

#if !__FINAL
void CTaskUseClimbOnRoute::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

bool CTaskUseClimbOnRoute::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent && pEvent->GetEventPriority() < E_PRIORITY_CLIMB_NAVMESH_ON_ROUTE)
		return false;

	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return
CTaskUseClimbOnRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_SlidingToCoord)
			FSM_OnEnter
				return StateSlidingToCoord_OnEnter(pPed);
			FSM_OnUpdate
				return StateSlidingToCoord_OnUpdate(pPed);
		FSM_State(State_Climbing)
			FSM_OnEnter
				return StateClimbing_OnEnter(pPed);
			FSM_OnUpdate
				return StateClimbing_OnUpdate(pPed);
		FSM_State(State_GoingToPoint)
			FSM_OnEnter
				return StateGoingToPoint_OnEnter(pPed);
			FSM_OnUpdate
				return StateGoingToPoint_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskUseClimbOnRoute::StateInitial_OnEnter(CPed * pPed)
{
	if(pPed->GetNavMeshTracker().GetIsValid())
	{
		m_InitialNavMeshUnderfoot = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
	}
	
	// Kinda figure out if the ped is in combat or fleeing and if so, hurry the fuck up!
	// Maybe not the most ideal way of doing this but meh
	if (pPed->GetPedIntelligence()->FindTaskCombat() || pPed->GetPedIntelligence()->FindTaskGun())
	{
		m_bUseFasterClimb = true;
	}

	if (pPed->GetPedIntelligence()->FindTaskSmartFlee())
	{
		m_bUseFasterClimb = true;
		m_bTryAvoidSlide = true;
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateInitial_OnUpdate(CPed * pPed)
{
	if (m_bTryAvoidSlide)
	{
		Vector3 vPos = m_vClimbPos;
		float fHeading = m_fClimbHeading;

		if (m_pEntityRouteIsOn)
			TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));

		float fDesiredHeadingChange = SubtractAngleShorter(
			fwAngle::LimitRadianAngleSafe(fHeading),
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading())
			);
		
		if (fabs(fDesiredHeadingChange) > PI * 0.25f) // Don't think we can rely on climb to fix more than this 
			SetState(State_SlidingToCoord);
		else
			SetState(State_Climbing);
	}
	else
	{
		SetState(State_SlidingToCoord);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateSlidingToCoord_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	Vector3 vPos = m_vClimbPos;
	float fHeading = m_fClimbHeading;

	if(m_pEntityRouteIsOn)
	{
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
	}

	CTaskSlideToCoord * pTaskSlide = rage_new CTaskSlideToCoord(vPos, fHeading, 100.0f);
	if (m_bUseFasterClimb)
		pTaskSlide->SetHeadingIncrement(PI * 4.0f);
	else
		pTaskSlide->SetHeadingIncrement(PI);

	pTaskSlide->SetPosAccuracy(0.33f);
	pTaskSlide->SetMaxTime(MAX_SLIDE_ON_ROUTE_TIME);
	SetNewTask(pTaskSlide);

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateSlidingToCoord_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD);
	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD && m_pEntityRouteIsOn)
	{
		Vector3 vPos = m_vClimbPos;
		float fHeading = m_fClimbHeading;
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
		((CTaskSlideToCoord*)GetSubTask())->SetTargetAndHeading(vPos, fHeading);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Climbing);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateClimbing_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	//! DMKH. Force vault. Allows us to re-evaluate climb as we do it. If we fail, we'll attempt to jump instead (is this ok?).
	static dev_float fRateMultiplier = 1.5f;
	fwFlags32 iFlags;
	if (m_bForceJump)
		iFlags = JF_DisableVault | JF_ShortJump;
	else
		iFlags = JF_ForceVault;

	CTaskJumpVault * pTaskClimb = rage_new CTaskJumpVault(iFlags);
	if (m_bUseFasterClimb)
		pTaskClimb->SetRateMultiplier(fRateMultiplier);

	SetNewTask(pTaskClimb);

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateClimbing_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(m_bForceJump)
		{
			return FSM_Quit;
		}

		// If the climb/vault subtask quit almost instantly, then we can assume it failed
		// Lets extend this logic for cases where the climb task has persisted for a long time w/o completion
		static dev_s32 iFailNumFames = 4;
		if(m_iFramesActive < iFailNumFames || GetTimeInState() > 15.0f)
		{
			if(!m_pEntityRouteIsOn)
				pPed->GetPedIntelligence()->RecordFailedClimbOnRoute(m_vClimbPos);
			return FSM_Quit;
		}

		if(!m_pEntityRouteIsOn)
		{
			SetState(State_GoingToPoint);
		}
		else
		{
			return FSM_Quit;
		}
	}
	m_iFramesActive++;
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateGoingToPoint_OnEnter(CPed * pPed)
{
	// If we have arrived on a piece of navmesh, which is not the same as we started on - then we can finish
	if(pPed->GetNavMeshTracker().GetIsValid())
	{
		const TNavMeshAndPoly & polyUnderfoot = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
		if(polyUnderfoot.m_iNavMeshIndex != m_InitialNavMeshUnderfoot.m_iNavMeshIndex ||
			polyUnderfoot.m_iPolyIndex != m_InitialNavMeshUnderfoot.m_iPolyIndex)
		{
			return FSM_Quit;
		}
	}

	// Only do this if the target would take us forward x meters
	Vector3 PedToTarget = m_vClimbTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if (PedToTarget.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())) > 1.f)
	{
		CTaskMoveGoToPoint * pTaskGoto = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, m_vClimbTarget);
		SetNewTask( rage_new CTaskComplexControlMovement( pTaskGoto ) );
	}
	else
	{
		// So we are done!
		return FSM_Quit;
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::StateGoingToPoint_OnUpdate(CPed * pPed)
{
	// If we have arrived on a piece of navmesh, which is not the same as we started on - then we can finish
	if(pPed->GetNavMeshTracker().GetIsValid())
	{
		const TNavMeshAndPoly & polyUnderfoot = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
		if(polyUnderfoot.m_iNavMeshIndex != m_InitialNavMeshUnderfoot.m_iNavMeshIndex ||
			polyUnderfoot.m_iPolyIndex != m_InitialNavMeshUnderfoot.m_iPolyIndex)
		{
			return FSM_Quit;
		}
	}

	// Detect if we require a second climb here.
	// Not very nice since the ped will visibly walk on the stop momentarily.
	// If we've already attempted a second climb, then fire an event to trigger a jump back
	const s32 iSecondJumpStuckFrames = CStaticMovementScanner::ms_iStaticCounterStuckLimit / 2;
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iSecondJumpStuckFrames && !m_bAttemptedSecondClimb)
	{
		m_bAttemptedSecondClimb = true;
		SetState(State_Climbing);
		return FSM_Continue;
	}
	else if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	else
	{
		if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
			CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
			if(pMoveTask && pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
			{
				CTaskMoveGoToPoint * pGoToTask = (CTaskMoveGoToPoint*)pMoveTask;
				//****************************************************************************************
				// If the goto target is significantly above the ped, then it is likely that the ped has
				// fallen off the drop already without passing through the target (quite likely, since we
				// won't trigger the target whilst in mid-air as we will be doing the fall task).
				// If this is the case, then just return.
				const float fZDiff = pGoToTask->GetTarget().z - pPed->GetTransform().GetPosition().GetZf();
				if(fZDiff > 1.0f && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					return FSM_Quit;
				}
			}
		}
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseClimbOnRoute::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// The ScanForClimb code needs to know that this ped is following a route
	// (to allow climbing on stairs when getting out of water)
	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );

	//Keep the cover point.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);

	// Prevent ped entering low-lod physics whilst climbing navmesh
	pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );

	// Abort the task if we take too long
	if(m_TimeOutTimer.IsSet() && m_TimeOutTimer.IsOutOfTime() && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		if(m_iWarpTimer>=0)
		{
			pPed->Teleport(m_vWarpTarget, pPed->GetDesiredHeading(), true);
		}
		return FSM_Quit;
	}

	return FSM_Continue;
}



//********************************************************************************
//	CTaskUseDropDownOnRoute

const s32 CTaskUseDropDownOnRoute::ms_iDefaultTimeOutMs = 10000;

CTaskUseDropDownOnRoute::CTaskUseDropDownOnRoute(const float fMoveBlendRatio, const Vector3 & vDropDownPos, const float fDropDownHeading, const Vector3 & vDropDownTarget, CEntity * pEntityRouteIsOn, const s32 iWarpTimer, const Vector3 & vWarpTarget, const bool bStepOffCarefully, const bool bUseDropDownTaskIfPossible) :
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_vDropDownPos(vDropDownPos),
	m_fDropDownHeading(fDropDownHeading),
	m_vDropDownTarget(vDropDownTarget),
	m_pEntityRouteIsOn(pEntityRouteIsOn),
	m_iWarpTimer(iWarpTimer),
	m_vWarpTarget(vWarpTarget),
	m_bStepOffCarefully(bStepOffCarefully),
	m_bUseDropDownTaskIfPossible(bUseDropDownTaskIfPossible)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_DROPDOWN_ON_ROUTE);

	m_TimeOutTimer.Set(fwTimer::GetTimeInMilliseconds(), (m_iWarpTimer>=0) ? m_iWarpTimer : 10000);
}

CTaskUseDropDownOnRoute::~CTaskUseDropDownOnRoute()
{
}

#if !__FINAL
void CTaskUseDropDownOnRoute::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

bool CTaskUseDropDownOnRoute::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent && pEvent->GetEventPriority() < E_PRIORITY_CLIMB_NAVMESH_ON_ROUTE)
		return false;

	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return
CTaskUseDropDownOnRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_SlidingToCoord)
			FSM_OnEnter
				return StateSlidingToCoord_OnEnter(pPed);
			FSM_OnUpdate
				return StateSlidingToCoord_OnUpdate(pPed);
		FSM_State(State_DroppingDown)
			FSM_OnEnter
				return StateDroppingDown_OnEnter(pPed);
			FSM_OnUpdate
				return StateDroppingDown_OnUpdate(pPed);
		FSM_State(State_DroppingDownWithTask)
			FSM_OnEnter
				return StateDroppingDownWithTask_OnEnter(pPed);
			FSM_OnUpdate
				return StateDroppingDownWithTask_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskUseDropDownOnRoute::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseDropDownOnRoute::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(m_bStepOffCarefully)
	{
		SetState(State_SlidingToCoord);
	}
	else
	{
		SetState(State_DroppingDown);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseDropDownOnRoute::StateSlidingToCoord_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	Vector3 vPos = m_vDropDownPos;
	float fHeading = m_fDropDownHeading;
	if(m_pEntityRouteIsOn)
	{
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
	}

	CTaskSlideToCoord * pTaskSlide = rage_new CTaskSlideToCoord(vPos, fHeading, 100.0f);
	pTaskSlide->SetHeadingIncrement(PI);
	pTaskSlide->SetPosAccuracy(0.1f);
	pTaskSlide->SetMaxTime(MAX_SLIDE_ON_ROUTE_TIME);
	SetNewTask(pTaskSlide);

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseDropDownOnRoute::StateSlidingToCoord_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD);

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD)
	{
		// Update heading with the orientation of any entity this route is on
		if(m_pEntityRouteIsOn)
		{
			Vector3 vPos = m_vDropDownPos;
			float fHeading = m_fDropDownHeading;
			TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
			((CTaskSlideToCoord*)GetSubTask())->SetTargetAndHeading(vPos, fHeading);
		}
		else
		{
			// Its possible that the ped fell off the edge whilst doing the slide task, or whilst stopping
			if((m_vDropDownPos.z - pPed->GetTransform().GetPosition().GetZf()) > 1.0f && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				return FSM_Quit;
			}
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_DroppingDown);
		return FSM_Continue;
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseDropDownOnRoute::StateDroppingDown_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if (m_fMoveBlendRatio < MBR_WALK_BOUNDARY) // No point trying to walk somewhere without any MBR
		return FSM_Quit;

	Vector3 vPos = m_vDropDownPos;
	float fHeading = m_fDropDownHeading;
	if(m_pEntityRouteIsOn)
	{
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
	}

	static const float fGoToDist = 2.0f;
	Matrix34 mat;
	mat.MakeRotateZ(fHeading);
	const Vector3 vHeadingDir = mat.b;

	CTaskMoveGoToPoint * pTaskGoTo = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, vPos + (vHeadingDir * fGoToDist));
	pTaskGoTo->SetTargetHeightDelta(10000.0f);
	SetNewTask( rage_new CTaskComplexControlMovement(pTaskGoTo) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseDropDownOnRoute::StateDroppingDown_OnUpdate(CPed * pPed)
{
	if(m_bUseDropDownTaskIfPossible)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown, true);

		const u8 nFlags = CDropDownDetector::DSF_AsyncTests | CDropDownDetector::DSF_TestForDive;
		//nFlags |= CDropDownDetector::DSF_PedRunning;

		pPed->GetPedIntelligence()->GetDropDownDetector().Process(NULL, NULL, nFlags);

		if(pPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDropDown())
		{
			SetState(State_DroppingDownWithTask);
			return FSM_Continue;
		}
	}

	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
		CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask && pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			CTaskMoveGoToPoint * pGoToTask = (CTaskMoveGoToPoint*)pMoveTask;

			if(m_pEntityRouteIsOn)
			{
				Vector3 vPos = m_vDropDownPos;
				float fHeading = m_fDropDownHeading;
				TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
				
				// Update the heading & target in the task if we're on a dynamic entity
				static const float fGoToDist = 2.0f;
				Matrix34 mat;
				mat.MakeRotateZ(fHeading);
				const Vector3 vHeadingDir = mat.b;

				Vector3 vTarget = vPos + (vHeadingDir * fGoToDist);
				pGoToTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vTarget));
			}

			//****************************************************************************************
			// If the goto target is significantly above the ped, then it is likely that the ped has
			// fallen off the drop already without passing through the target (quite likely, since we
			// won't trigger the target whilst in mid-air as we will be doing the fall task).
			// If this is the case, then just return.
			const float fZDiff = pGoToTask->GetTarget().z - pPed->GetTransform().GetPosition().GetZf();
			if(fZDiff > 1.0f && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				return FSM_Quit;
			}
		}
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskUseDropDownOnRoute::StateDroppingDownWithTask_OnEnter(CPed * pPed)
{
	const CDropDown &dropDown = pPed->GetPedIntelligence()->GetDropDownDetector().GetDetectedDropDown();

	SetNewTask( rage_new CTaskDropDown(dropDown) );

	return FSM_Continue;
}

CTask::FSM_Return CTaskUseDropDownOnRoute::StateDroppingDownWithTask_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskUseDropDownOnRoute::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// If we're stuck then fire an event to trigger a jump back
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	//Keep the cover point.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);

	// Prevent ped entering low-lod physics whilst using a drop
	pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );

	// Abort the task if we take too long
	if(m_TimeOutTimer.IsSet() && m_TimeOutTimer.IsOutOfTime() && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		if(m_iWarpTimer>=0)
		{
			pPed->Teleport(m_vWarpTarget, pPed->GetDesiredHeading(), true);
		}
		return FSM_Quit;
	}

	return FSM_Continue;
}



//********************************************************************************
//	CTaskUseLadderOnRoute

const s32 CTaskUseLadderOnRoute::ms_iDefaultTimeOutMs = 30000;

CTaskUseLadderOnRoute::CTaskUseLadderOnRoute(const float fMoveBlendRatio, const Vector3 & vGetOnPos, const float fLadderHeading, CEntity * pEntityRouteIsOn) :
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_vGetOnPos(vGetOnPos),
	m_fLadderHeading(fLadderHeading),
	m_pEntityRouteIsOn(pEntityRouteIsOn),
	m_pLadderClipRequestHelper(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_LADDER_ON_ROUTE);

	m_TimeOutTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_iDefaultTimeOutMs);
}

CTaskUseLadderOnRoute::~CTaskUseLadderOnRoute()
{
	if(m_pLadderClipRequestHelper)
	{
		CLadderClipSetRequestHelperPool::ReleaseClipSetRequestHelper(m_pLadderClipRequestHelper);
		m_pLadderClipRequestHelper = NULL;	// Will the destructor ever be called twice anyway!?
	}
}

#if !__FINAL
void CTaskUseLadderOnRoute::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

bool CTaskUseLadderOnRoute::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent && pEvent->GetEventPriority() < E_PRIORITY_CLIMB_LADDER_ON_ROUTE)
		return false;

	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return
CTaskUseLadderOnRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_SlidingToCoord)
			FSM_OnEnter
				return StateSlidingToCoord_OnEnter(pPed);
			FSM_OnUpdate
				return StateSlidingToCoord_OnUpdate(pPed);
		FSM_State(State_OrientatingToLadder)
				FSM_OnEnter
				return StateOrientatingToLadder_OnEnter(pPed);
			FSM_OnUpdate
				return StateOrientatingToLadder_OnUpdate(pPed);
		FSM_State(State_UsingLadder)
			FSM_OnEnter
				return StateUsingLadder_OnEnter(pPed);
			FSM_OnUpdate
				return StateUsingLadder_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskUseLadderOnRoute::StateInitial_OnEnter(CPed * pPed)
{
	// Stream the bloody ladder, again...
	if (!m_pLadderClipRequestHelper)
		m_pLadderClipRequestHelper = CLadderClipSetRequestHelperPool::GetNextFreeHelper(pPed);

	taskAssertf(NetworkInterface::IsGameInProgress() || m_pLadderClipRequestHelper, "CLadderClipSetRequestHelperPool::GetNextFreeHelper() Gave us NULL!? in CTaskUseLadderOnRoute()");

	if (m_pLadderClipRequestHelper)
	{
		int iStreamStates = CTaskClimbLadder::NEXT_STATE_CLIMBING;
		if (GetPed()->GetIsSwimming())
			iStreamStates |= CTaskClimbLadder::NEXT_STATE_WATERMOUNT;

		CTaskClimbLadder::StreamReqResourcesIn(m_pLadderClipRequestHelper, GetPed(), iStreamStates);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	static dev_bool bSlideToCoord = false;
	if(bSlideToCoord)
	{
		SetState(State_SlidingToCoord);
	}
	else
	{
		SetState(State_UsingLadder);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateSlidingToCoord_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	Vector3 vPos = m_vGetOnPos;
	float fHeading = m_fLadderHeading;
	if(m_pEntityRouteIsOn)
	{
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
	}

	CTaskSlideToCoord * pTaskSlide = rage_new CTaskSlideToCoord(vPos, fHeading, 100.0f);
	pTaskSlide->SetHeadingIncrement(PI);
	pTaskSlide->SetPosAccuracy(0.1f);
	pTaskSlide->SetMaxTime(MAX_SLIDE_ON_ROUTE_TIME);
	SetNewTask(pTaskSlide);

	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateSlidingToCoord_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD);
	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD && m_pEntityRouteIsOn)
	{
		Vector3 vPos = m_vGetOnPos;
		float fHeading = m_fLadderHeading;
		TransformPositionAndHeading(vPos, fHeading, MAT34V_TO_MATRIX34(m_pEntityRouteIsOn->GetMatrix()));
		((CTaskSlideToCoord*)GetSubTask())->SetTargetAndHeading(vPos, fHeading);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_UsingLadder);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateOrientatingToLadder_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveAchieveHeading * pAchieveHeading = rage_new CTaskMoveAchieveHeading(m_fLadderHeading, CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac, 0.1f);
	SetNewTask( rage_new CTaskComplexControlMovement(pAchieveHeading, NULL) );
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateOrientatingToLadder_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_UsingLadder);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateUsingLadder_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskClimbLadderFully * pTaskLadder = rage_new CTaskClimbLadderFully(m_fMoveBlendRatio);
	SetNewTask(pTaskLadder);
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::StateUsingLadder_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskUseLadderOnRoute::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (m_pLadderClipRequestHelper)
		m_pLadderClipRequestHelper->RequestAllClipSets();

	// If we're stuck then fire an event to trigger a jump back
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	// Prevent ped entering low-lod physics whilst using a ladder
	pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );

	// Abort the task if we take too long
	if(m_TimeOutTimer.IsSet() && m_TimeOutTimer.IsOutOfTime() && MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}


//***************************************************
//	CTaskMoveReturnToRoute
//

const float CTaskMoveReturnToRoute::ms_fTargetRadius = CTaskMoveGoToPoint::ms_fTargetRadius;

CTaskMoveReturnToRoute::CTaskMoveReturnToRoute(const float fMoveBlendRatio, const Vector3 & vTarget, const float fTargetRadius) :
	CTaskMove(fMoveBlendRatio),
	m_vTarget(vTarget),
	m_fTargetRadius(fTargetRadius)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_RETURN_TO_ROUTE);
}

CTaskMoveReturnToRoute::~CTaskMoveReturnToRoute()
{

}

#if !__FINAL
void
CTaskMoveReturnToRoute::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

CTask::FSM_Return
CTaskMoveReturnToRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveReturnToRoute::Initial_OnUpdate(CPed * pPed)
{
	if(!GetSubTask())
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		return FSM_Continue;
	}

	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	//*********************************************************************************************
	// This task will initially try to use the navmesh to get back onto the closest route segment.
	// If this doesn't work (route cannot be found) then a goto point is used instead.

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		switch(GetSubTask()->GetTaskType())
		{
			case CTaskTypes::TASK_MOVE_STAND_STILL:
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
				break;
			}
			case CTaskTypes::TASK_MOVE_GO_TO_POINT:
			case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
			{
				return FSM_Quit;
			}
		}
	}
	else if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		CTaskMoveFollowNavMesh* pNavTask = (CTaskMoveFollowNavMesh*)GetSubTask();
		if(pNavTask->IsUnableToFindRoute() && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed) );
		}
	}
	return FSM_Continue;
}

aiTask*
CTaskMoveReturnToRoute::CreateSubTask(const int iSubTaskType, CPed* UNUSED_PARAM(pPed))
{
	CTask * pSubTask=NULL;
	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			pSubTask = rage_new CTaskMoveStandStill(100);
			break;
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			pSubTask = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, m_vTarget);
			break;
		}
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			pSubTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vTarget);
			break;
		}
	}
	Assert(false);
	return pSubTask;
}




//*********************************************************************************
//	CTasMoveGoToSafePositionOnNavMesh
//	This task finds a safe position nearby to the ped, and attempts to get them
//	there using basic (non-navmesh) movement tasks.

const float CTasMoveGoToSafePositionOnNavMesh::ms_fTargetRadius = 0.25f;
const s32 CTasMoveGoToSafePositionOnNavMesh::ms_iStandStillTime = 2000;
const s32 CTasMoveGoToSafePositionOnNavMesh::ms_iGiveUpTime = 6000;

CTasMoveGoToSafePositionOnNavMesh::CTasMoveGoToSafePositionOnNavMesh(const float fMoveBlendRatio, bool bOnlyNonIsolatedPoly, s32 iTimeOutMs) :
	CTaskMove(fMoveBlendRatio),
	m_bOnlyNonIsolatedPoly(bOnlyNonIsolatedPoly),
	m_iTimeOutMs(iTimeOutMs)
{
	Init();

	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_SAFE_POSITION_ON_NAVMESH);
}

CTasMoveGoToSafePositionOnNavMesh::~CTasMoveGoToSafePositionOnNavMesh()
{

}

void
CTasMoveGoToSafePositionOnNavMesh::Init()
{
	m_bFoundSafePos = false;
	m_vSafePos = Vector3(0.0f,0.0f,0.0f);
}

#if !__FINAL
void
CTasMoveGoToSafePositionOnNavMesh::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif 

CTask::FSM_Return
CTasMoveGoToSafePositionOnNavMesh::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_Waiting)
			FSM_OnUpdate
				return Waiting_OnUpdate();
		FSM_State(State_Moving)
			FSM_OnUpdate
				return Moving_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTasMoveGoToSafePositionOnNavMesh::Initial_OnUpdate(CPed * pPed)
{
	// Just in case someone queries this task for its target before it has one..
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_vSafePos = vPedPosition;

	// Try to find a position on the navmesh
	const u32 iFlags = (GetClosestPos_ClearOfObjects | GetClosestPos_ExtraThorough);
	m_bFoundSafePos = (CPathServer::GetClosestPositionForPed(vPedPosition, m_vSafePos, 20.0f, iFlags) != ENoPositionFound);

	if(!m_bFoundSafePos)
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		SetState(State_Waiting);
	}
	else
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL, pPed) );
		SetState(State_Moving);
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	return FSM_Continue;
}

CTask::FSM_Return
CTasMoveGoToSafePositionOnNavMesh::Waiting_OnUpdate()
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		return FSM_Quit;
	return FSM_Continue;
}

CTask::FSM_Return
CTasMoveGoToSafePositionOnNavMesh::Moving_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	else
	{
		if(m_TimeOutTimer.IsSet() && m_TimeOutTimer.IsOutOfTime() && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			return FSM_Quit;
		}
		if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
		{
			CEventStaticCountReachedMax staticEvent(GetTaskType());
			pPed->GetPedIntelligence()->AddEvent(staticEvent);
		}
	}

	return FSM_Continue;
}

aiTask*
CTasMoveGoToSafePositionOnNavMesh::CreateSubTask(const int iSubTaskType, CPed* UNUSED_PARAM(pPed))
{
	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			return rage_new CTaskMoveStandStill(ms_iStandStillTime);
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
		{
			if(m_iTimeOutMs > 0)
				m_TimeOutTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iTimeOutMs);
			else
				m_TimeOutTimer.Unset();

			return rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_vSafePos, ms_fTargetRadius);
		}
		default:
		{
			Assert(false);
			return NULL;
		}
	}
}

