#include "TaskMoveWander.h"

#include "fwscene/search/Search.h"
#include "Control/TrafficLights.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/Weather.h"
#include "PathServer/PathServer.h"
#include "Peds/Ped.h"
#include "Task/General/TaskBasic.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Crimes/RandomEventManager.h"
#include "VehicleAI/Junctions.h"

AI_OPTIMISATIONS()
AI_NAVIGATION_OPTIMISATIONS()

u32 CWanderFlags::GetAsPathServerFlags() const
{
	u32 iFlags = 0;
	if(IsSet(CWanderFlags::MW_StayOnPavements))
	{
		iFlags |= PATH_FLAG_PREFER_PAVEMENTS;
		iFlags |= PATH_FLAG_NEVER_LEAVE_PAVEMENTS;
	}
	return iFlags;
}

const float CTaskMoveWander::ms_fMinLeftBeforeRequestingNextPath = 5.0f;
const u32 CTaskMoveWander::ms_iUpdateWanderHeadingFreq = 500;
const float CTaskMoveWander::ms_fMinWanderDist = 9.0f;
const float CTaskMoveWander::ms_fMaxWanderDist = 21.0f;
const u32 CTaskMoveWander::ms_iMaxNumAttempts = 8;
const float CTaskMoveWander::ms_fWanderHeadingModifierInc = 22.5f * DtoR;
const u32 CTaskMoveWander::m_iTimeToPauseBetweenRepeatSearches = 5000;
const s32 CTaskMoveWander::ms_iMaxTimeDeltaToRejectDoubleBackedPathsMS = 6000;

CTaskMoveWander::CTaskMoveWander(
	const float fMoveBlendRatio,
	const float fHeading,
	const bool bWanderSensibly,
	const float fTargetRadius,
	const bool bStayOnPavements,
	const bool bContinueFromNetwork)
: CTaskNavBase(fMoveBlendRatio, Vector3(0.0f,0.0f,0.0f))
{
	CWanderParams wanderParams;
	wanderParams.m_fMoveBlendRatio = fMoveBlendRatio;
	wanderParams.m_fHeading = fHeading;
	wanderParams.m_bWanderSensibly = bWanderSensibly;
	wanderParams.m_fTargetRadius = fTargetRadius;
	wanderParams.m_bStayOnPavements = bStayOnPavements;
	wanderParams.m_bContinueFromNetwork = bContinueFromNetwork;

	Init(wanderParams);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WANDER);
}

CTaskMoveWander::CTaskMoveWander(const CWanderParams & wanderParams)
: CTaskNavBase(wanderParams.m_fMoveBlendRatio, Vector3(0.0f,0.0f,0.0f))
{
	Init(wanderParams);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WANDER);
}

void CTaskMoveWander::Init(const CWanderParams & wanderParams)
{
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SmoothPathCorners);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseLadders);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseClimbs);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseDrops);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath, wanderParams.m_bContinueFromNetwork);

	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidEntityTypeObject);
	// By default wander task does not avoid vehicles, because this does not work well in cases where a player
	// vehicle has blocked the entire pavement - peds would give up and walk back the way they came;
	// Instead we pathfind without vehicle avoidance, and when a vehicle collision is detected a response
	// task is created to walk around it (CTaskWalkRoundCarWhileWandering)
	m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_AvoidEntityTypeVehicle);

	m_WanderFlags.Set(CWanderFlags::MW_WanderSensibly, wanderParams.m_bWanderSensibly);
	m_WanderFlags.Set(CWanderFlags::MW_StayOnPavements, wanderParams.m_bStayOnPavements);
	m_WanderFlags.Set(CWanderFlags::MW_ContinueFromNetwork, wanderParams.m_bContinueFromNetwork);

	m_fTargetRadius = wanderParams.m_fTargetRadius;
	m_fWanderHeading = wanderParams.m_fHeading;
	m_fWanderHeadingModifier = 0.0f;
	m_fMinWanderDist = ms_fMinWanderDist;
	m_fMaxWanderDist = ms_fMaxWanderDist;
	m_iNumRouteAttempts = 0;
	m_iNavMeshRouteMethod = eRouteMethod_Normal;
	m_fTimeWanderingOffNavmesh = 0;
	m_bHadFailedInAllDirections = false;
	m_bNextPathDoublesBack = false;
	m_LastVehicleWanderedAround = NULL;

	m_fTimeWanderingOffNavmesh = 0.0f;
}

CTaskMoveWander::~CTaskMoveWander()
{
	Assert(m_iPathHandle == PATH_HANDLE_NULL);
}

aiTask * CTaskMoveWander::Copy() const
{
	// TODO: use the new 'CNavParams' constructor
	CTaskMoveWander * pNewTask = rage_new CTaskMoveWander(
		m_fInitialMoveBlendRatio,
		m_fWanderHeading,
		m_WanderFlags.IsSet(CWanderFlags::MW_WanderSensibly),
		m_fTargetRadius,
		m_WanderFlags.IsSet(CWanderFlags::MW_StayOnPavements),
		m_WanderFlags.IsSet(CWanderFlags::MW_ContinueFromNetwork)
	);
	pNewTask->SetSmoothPathCorners(GetSmoothPathCorners());
	pNewTask->SetDontUseLadders(GetDontUseLadders());
	pNewTask->SetDontUseClimbs(GetDontUseClimbs());
	pNewTask->SetDontUseDrops(GetDontUseDrops());

	// TODO: Clone other flags here
	return pNewTask;
}

int CTaskMoveWander::GetCurrentRouteProgress()
{
	if(m_pRoute && m_pRoute->GetSize()>0 && GetState()==NavBaseState_FollowingPath)
	{
		return m_iProgress;
	}
	Assertf(false, "CTaskMoveWander::GetCurrentRouteProgress() was called when state was not NavBaseState_FollowingPath.");
	return 0;
}

bool CTaskMoveWander::GetWanderTarget(Vector3 & vTarget)
{
	if(m_pRoute && m_pRoute->GetSize()>0)
	{
		vTarget = m_pRoute->GetPosition(m_pRoute->GetSize()-1);
		return true;
	}
	return false;
}

s32 CTaskMoveWander::GetDefaultStateAfterAbort() const
{
	return CTaskNavBase::GetDefaultStateAfterAbort();
}

void CTaskMoveWander::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	if(pEvent)
	{
		switch(pEvent->GetEventType())
		{
			case EVENT_POTENTIAL_WALK_INTO_VEHICLE:
			{
				m_WanderFlags.Set(CWanderFlags::MW_AbortedForWalkRoundVehicle);
				break;
			}
		}
	}
}

CTask::FSM_Return CTaskMoveWander::ProcessPreFSM()
{
	CPed * pPed = GetPed();

	if( GetIsFlagSet(aiTaskFlags::IsAborted) )
	{
		CTaskWander * pTaskWander = (CTaskWander*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER);
		if( pTaskWander && pTaskWander->GetLastVehicleWalkedAround() )
		{
			m_fWanderHeading = pPed->GetMotionData()->GetCurrentHeading();
			m_fWanderHeading = fwAngle::LimitRadianAngle(m_fWanderHeading);
			m_fWanderHeadingModifier = 0.0f;
		}
	}

	switch(GetState())
	{
		case NavBaseState_FollowingPath:
		case NavBaseState_MovingWhilstWaitingForPath:
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_Wandering, true);
			break;
		default:
			break;
	}

	Vector3 vWanderDirection;
	if(m_WanderFlags.IsSet(CWanderFlags::MW_HasNewWanderDirection))
	{
		m_fWanderHeadingModifier = 0.0f;
		vWanderDirection.x = -rage::Sinf(m_fWanderHeading);
		vWanderDirection.y = rage::Cosf(m_fWanderHeading);
		vWanderDirection.z = 0.0f;
		m_fWanderHeading = fwAngle::GetRadianAngleBetweenPoints(vWanderDirection.x, vWanderDirection.y, 0.0f, 0.0f);
		m_fWanderHeading = fwAngle::LimitRadianAngle(m_fWanderHeading);

		m_WanderFlags.Reset(CWanderFlags::MW_HasNewWanderDirection);
	}

	return CTaskNavBase::ProcessPreFSM();
}

CTask::FSM_Return CTaskMoveWander::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	switch(iState)
	{
		case State_WaitingBetweenRepeatSearches:
		{
			FSM_Begin
				FSM_State(State_WaitingBetweenRepeatSearches)
					FSM_OnEnter
						return StateWaitingBetweenRepeatSearches_OnEnter(pPed);
					FSM_OnUpdate
						return StateWaitingBetweenRepeatSearches_OnUpdate(pPed);
			FSM_End
		}
		case NavBaseState_FollowingPath:
		{
			FSM_Begin
				FSM_State(NavBaseState_FollowingPath)
					FSM_OnEnter
					{
						m_fTimeWanderingOffNavmesh = 0.0f;
						return CTaskNavBase::UpdateFSM(iState, iEvent);
					}
					FSM_OnUpdate
					{
						m_iTimeWasLastWanderingMS = fwTimer::GetTimeInMilliseconds();
						// Note : 'StateFollowingPath_OnUpdate' will call the base UpdateFSM
						return StateFollowingPath_OnUpdate(pPed);
					}
			FSM_End
		}
		default:
		{
			return CTaskNavBase::UpdateFSM(iState, iEvent);
		}
	}
}

CTask::FSM_Return CTaskMoveWander::StateFollowingPath_OnUpdate(CPed * pPed)
{
	if(CTaskNavBase::StateFollowingPath_OnUpdate(pPed) == FSM_Quit)
	{
		return FSM_Quit;
	}

	if(GetState()==NavBaseState_FollowingPath)
	{
		// Every few ticks update our wander heading, smoothing in the peds current heading
		if(!m_UpdateWanderHeadingTimer.IsSet() || m_UpdateWanderHeadingTimer.IsOutOfTime())
		{
			Vector2 vWanderHdg(-rage::Sinf(m_fWanderHeading), rage::Cosf(m_fWanderHeading));
			Vector2 vPedHdg(-rage::Sinf(pPed->GetCurrentHeading()), rage::Cosf(pPed->GetCurrentHeading()));
			Vector2 vNewHdg = (vWanderHdg * 0.75f) + (vPedHdg * 0.25f);
			vNewHdg.Normalize();
			m_fWanderHeading = fwAngle::GetRadianAngleBetweenPoints(vNewHdg.x, vNewHdg.y, 0.0f, 0.0f);
			m_fWanderHeading = fwAngle::LimitRadianAngle(m_fWanderHeading);

			m_UpdateWanderHeadingTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_iUpdateWanderHeadingFreq);
			m_fWanderHeadingModifier = 0.0f;
		}

		// Query the ped's navmesh tracked (CNavMeshTrackedObject) to see if we've come off the navmesh
		// If so, then request a new path.
		if(!pPed->GetNavMeshTracker().GetIsValid() && !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly) && !m_pNextRouteSection)
		{
			m_fTimeWanderingOffNavmesh += GetTimeStep();
			static dev_float fMaxTime = 1.0f;
			if(m_fTimeWanderingOffNavmesh >= fMaxTime)
			{
				m_fTimeWanderingOffNavmesh = 0.0f;

				BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p CTaskMoveWander, due to time off navmesh", fwTimer::GetFrameCount(), pPed); )

				RequestPath(pPed);
				SetState(NavBaseState_StationaryWhilstWaitingForPath);
				return FSM_Continue;
			}
		}
		else
		{
			m_fTimeWanderingOffNavmesh = 0.0f;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveWander::StateWaitingBetweenRepeatSearches_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveStandStill * pStandStill = rage_new CTaskMoveStandStill(m_iTimeToPauseBetweenRepeatSearches);
	SetNewTask(pStandStill);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveWander::StateWaitingBetweenRepeatSearches_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(NavBaseState_Initial);
	}
	/*
	else
	{
		HandleNewTarget(pPed);
	}
	*/

	// To prevent peds getting stuck at dead ends
	if (!m_bHadFailedInAllDirections &&
		(!m_UpdateWanderHeadingTimer.IsSet() || m_UpdateWanderHeadingTimer.IsOutOfTime()))
	{
		Vector2 vPedHdg(-rage::Sinf(pPed->GetCurrentHeading()), rage::Cosf(pPed->GetCurrentHeading()));
		m_fWanderHeading = fwAngle::GetRadianAngleBetweenPoints(vPedHdg.x, vPedHdg.y, 0.0f, 0.0f);
		m_fWanderHeading = fwAngle::LimitRadianAngle(m_fWanderHeading);

		m_UpdateWanderHeadingTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_iUpdateWanderHeadingFreq);
		m_fWanderHeadingModifier = 0.0f;
	}

	return FSM_Continue;
}

// static float START_POSITION_Z_REDUCTION = 0.0f;	//0.5f;
static float PATH_FAILED_SMALL_ROUTE_REQUEST = 2.0f;

// Initialise the structure which is passed into the CPathServer::RequestPath() function
void CTaskMoveWander::InitPathRequestPathStruct(CPed * pPed, TRequestPathStruct & reqPathStruct, const float fDistanceAheadOfPed)
{
	AssertMsg(PATH_FAILED_SMALL_ROUTE_REQUEST < m_fMinWanderDist, "min value must be somewhat larger than m_fMinWanderDist");

	const float fBaseWanderDistance = m_bHadFailedInAllDirections ?
		PATH_FAILED_SMALL_ROUTE_REQUEST : fwRandom::GetRandomNumberInRange(m_fMinWanderDist, m_fMaxWanderDist);

	float fWanderDistance = (pPed->GetCurrentMotionTask()->GetNavFlags() & PATH_FLAG_PREFER_DOWNHILL) ?
		fBaseWanderDistance * 3.0f : fBaseWanderDistance;

	reqPathStruct.m_iFlags = pPed->GetCurrentMotionTask()->GetNavFlags();
	reqPathStruct.m_iFlags |= m_NavBaseConfigFlags.GetAsPathServerFlags();
	reqPathStruct.m_iFlags |= m_WanderFlags.GetAsPathServerFlags();

	if(pPed->m_nFlags.bIsOnFire)
		reqPathStruct.m_iFlags |= PATH_FLAG_DONT_AVOID_FIRE;

	// If we have just walked around a vehicle, then allow leaving pavement for this path alone
	if(m_WanderFlags.IsSet(CWanderFlags::MW_AbortedForWalkRoundVehicle))
	{
		reqPathStruct.m_iFlags &= ~PATH_FLAG_NEVER_LEAVE_PAVEMENTS;
	}

	reqPathStruct.m_iFlags |= PATH_FLAG_WANDER;
	reqPathStruct.m_iFlags |= PATH_FLAG_NEVER_ENTER_WATER;
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

	// This flag is specially for wandering peds.  If the path starts of not on any pavement, then
	// we will allow the path to use drops & climbs.  This is intended to allow stranded peds to
	// return to nearby areas of pavement.
	reqPathStruct.m_iFlags |= PATH_FLAG_IF_NOT_ON_PAVEMENT_ALLOW_DROPS_AND_CLIMBS;

	// If the ped is standing on train-tracks, then allow the use of climbs and drops in order to escape
	if( pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bTrainTracks )
	{
		if( pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet( CPedNavCapabilityInfo::FLAG_MAY_CLIMB ) )
		{
			reqPathStruct.m_iFlags &= ~PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
		}
		if( pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet( CPedNavCapabilityInfo::FLAG_MAY_DROP ) )
		{
			reqPathStruct.m_iFlags &= ~PATH_FLAG_NEVER_DROP_FROM_HEIGHT;
		}
	}

	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_RunFromFiresAndExplosions ) )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_AVOID_POTENTIAL_EXPLOSIONS;
	}

	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AvoidTearGas ) )
	{
		reqPathStruct.m_iFlags |= PATH_FLAG_AVOID_TEAR_GAS;
	}

	const Vector3 vWanderDirection(
		-rage::Sinf(m_fWanderHeading + m_fWanderHeadingModifier),
		rage::Cosf(m_fWanderHeading + m_fWanderHeadingModifier),
		0.0f
	);


	// Get a list of influence spheres which we might want to avoid, from the shocking-events manager
	reqPathStruct.m_iNumInfluenceSpheres = BuildInfluenceSpheres(pPed, 20.0f, reqPathStruct.m_InfluenceSpheres, 4);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	reqPathStruct.m_vPathStart = vPedPosition;
	reqPathStruct.m_vPathEnd = vPedPosition;
	reqPathStruct.m_vReferenceVector = vWanderDirection;
	reqPathStruct.m_fReferenceDistance = fWanderDistance;
	reqPathStruct.m_fCompletionRadius = 0.0f;
	reqPathStruct.m_pPed = pPed;
	reqPathStruct.m_pEntityPathIsOn = NULL;
	reqPathStruct.m_fClampMaxSearchDistance = GetClampMaxSearchDistance();
	reqPathStruct.m_fMaxDistanceToAdjustPathStart = GetMaxDistanceMayAdjustPathStartPosition();
	reqPathStruct.m_fMaxDistanceToAdjustPathEnd = GetMaxDistanceMayAdjustPathEndPosition();

	reqPathStruct.m_fEntityRadius = pPed->GetCapsuleInfo()->GetHalfWidth();

#if __BANK
	if(CTaskNavBase::ms_fOverrideRadius != PATHSERVER_PED_RADIUS)
		reqPathStruct.m_fEntityRadius = ms_fOverrideRadius;
#endif

#if __TRACK_PEDS_IN_NAVMESH
	if(pPed->GetNavMeshTracker().IsUpToDate(vPedPosition))
	{
		reqPathStruct.m_StartNavmeshAndPoly = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
		reqPathStruct.m_vPathStart = pPed->GetNavMeshTracker().GetLastPosition();
		reqPathStruct.m_vPathEnd = reqPathStruct.m_vPathStart;
	}
#endif

	// Override the m_vPathStart if we have a non-zero distance passed in
	if(fDistanceAheadOfPed > 0.0f)
	{
		InitPathRequest_DistanceAheadCalculation(fDistanceAheadOfPed, pPed, reqPathStruct);
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

	reqPathStruct.m_bIgnoreTypeObjects = (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeObject)==false);
	reqPathStruct.m_bIgnoreTypeVehicles = (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeVehicle)==false);

	if( GetIsFlagSet(aiTaskFlags::IsAborted) || !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HasPathed) )
	{
		CTaskWander * pTaskWander = (CTaskWander*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER);
		if(pTaskWander)
		{ 
			CVehicle* pVehicle = pTaskWander->GetLastVehicleWalkedAround();
			if (!pVehicle && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasJustLeftCar) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_JustBumpedIntoVehicle)))
				pVehicle = pPed->GetPedIntelligence()->GetClosestVehicleInRange(true);

			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JustBumpedIntoVehicle, false);

			if (pVehicle)
			{
				reqPathStruct.m_iNumIncludeObjects = 1;
				reqPathStruct.m_IncludeObjects[0] = pVehicle;
				reqPathStruct.m_iFlags |= PATH_FLAG_REDUCE_OBJECT_BBOXES;
			}
		}
	}

	// If we wish to be on pavement, but our navmesh tracker indicates we are not - then extend the wander distance to
	// be at least 40m, giving us more chance of locating pavement.
	// Exceptions:
	// - not if we are in an interior, as is this may compromise mission characters' ability to wander off after a mission ends
	// - not if we've already failed to find a route using all methods; it's better to move a little than not at all
	if( (reqPathStruct.m_iFlags & PATH_FLAG_PREFER_PAVEMENTS) || (reqPathStruct.m_iFlags & PATH_FLAG_NEVER_LEAVE_PAVEMENTS) )
	{
		if( pPed->GetNavMeshTracker().GetIsValid() && !pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement && !pPed->GetNavMeshTracker().GetNavPolyData().m_bInterior)
		{
			if(m_iNumRouteAttempts < eRouteMethod_NumMethods)
				reqPathStruct.m_fReferenceDistance = Max(reqPathStruct.m_fReferenceDistance, 40.0f);
		}
	}
}

bool CTaskMoveWander::ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const
{
	bMayUsePreviousTarget = true;

	CTaskWander * pTaskWander = (CTaskWander*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER);
	if(pTaskWander && pTaskWander->GetLastVehicleWalkedAround())
	{
		bMayUsePreviousTarget = (m_WanderFlags.IsSet(CWanderFlags::MW_AbortedForWalkRoundVehicle) == false);
		return true;
	}

	return CTaskNavBase::ShouldKeepMovingWhilstWaitingForPath(pPed, bMayUsePreviousTarget);
}

bool CTaskMoveWander::OnSuccessfulRoute(CPed * pPed)
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	//------------------------------------------------------------------------------------------------------------
	// If this path intersects any wander blocking regions, whilst the ped is outside of the region, then reject

	for(s32 s=0; s<CPathServer::GetNumPathRegionSwitches(); s++)
	{
		const TPathRegionSwitch & pathSwitch = CPathServer::GetPathRegionSwitch(s);
		if(pathSwitch.m_Switch == SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS)
		{
			// Reduce the region to test for ped intersection.
			// The idea is that we allow peds already within the region to wander through.
			// But we want to avoid newly arriving peds from walking though.
			// Reducing the region means we're less likely to admit peds, and also accounts for
			// peds who walk on a little after hitting this code once already.
			static dev_float fReduceAmt = 1.0f;
			Vector3 vShrunkMin = pathSwitch.m_vMin + Vector3(fReduceAmt, fReduceAmt, 0.0f);
			Vector3 vShrunkMax = pathSwitch.m_vMax - Vector3(fReduceAmt, fReduceAmt, 0.0f);

			if(vPedPos.x < vShrunkMin.x || vPedPos.y < vShrunkMin.y || vPedPos.z < vShrunkMin.z ||
				vPedPos.x >= vShrunkMax.x || vPedPos.y >= vShrunkMax.y || vPedPos.z >= vShrunkMax.z)
			{
				if( GetPathIntersectsRegion(pathSwitch.m_vMin, pathSwitch.m_vMax) )
				{
					UpdateWanderHeadingModifier();

					m_iNumRouteAttempts++;

					return false;
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------------

	// If the wander path doubles back, then have a few attempts at finding more suitable ones
	static dev_bool bProcessDoubleBack = false;
	if(bProcessDoubleBack && DoesWanderPathDoubleBack(pPed) && (fwTimer::GetTimeInMilliseconds() - m_iTimeWasLastWanderingMS) < ms_iMaxTimeDeltaToRejectDoubleBackedPathsMS)
	{
		if(m_iNumRouteAttempts < ms_iMaxNumAttempts)
		{
			m_iNavMeshRouteMethod = eRouteMethod_ReducedObjectBoundingBoxes;

			UpdateWanderHeadingModifier();

			m_iNumRouteAttempts++;
			SetState(State_WaitingBetweenRepeatSearches);

			return false;
		}
	}

	// If the wander path doubles back, mark this to inform the parent task
	if( DoesNextWanderPathDoubleBack() )
	{
		// set member flag to true
		m_bNextPathDoublesBack = true;
	}
	else
	{
		// set member flag to false
		m_bNextPathDoublesBack = false;
	}

	m_fWanderHeadingModifier = 0.0f;

	m_iNavMeshRouteMethod = eRouteMethod_Normal;
	m_iNumRouteAttempts = 0;

	if(m_pRoute->GetLength() > PATH_FAILED_SMALL_ROUTE_REQUEST)
	{
		m_iNavMeshRouteMethod = eRouteMethod_Normal;
		m_bHadFailedInAllDirections = false;
	}

	m_WanderFlags.Reset(CWanderFlags::MW_AbortedForWalkRoundVehicle);

	return CTaskNavBase::OnSuccessfulRoute(pPed);
}

void CTaskMoveWander::UpdateWanderHeadingModifier()
{
	// This code alternately increases the m_fWanderHeadingModifier to the left/right
	if(m_iNumRouteAttempts==0)
	{
		m_fWanderHeadingModifier = ms_fWanderHeadingModifierInc;
	}
	else
	{
		if(m_fWanderHeadingModifier > 0.0f)
		{
			m_fWanderHeadingModifier = -m_fWanderHeadingModifier;
		}
		else
		{
			float fCurrModifier = Abs(m_fWanderHeadingModifier);
			fCurrModifier += ms_fWanderHeadingModifierInc;
			fCurrModifier *= Sign(m_fWanderHeadingModifier);
			fCurrModifier *= -1.0f;
			m_fWanderHeadingModifier = fCurrModifier;
		}
	}
}

CTask::FSM_Return CTaskMoveWander::OnFailedRoute(CPed * DEV_ONLY(pPed), const TPathResultInfo & UNUSED_PARAM(pathResultInfo))
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

	// can't double back on no route
	m_bNextPathDoublesBack = false;

	if(m_iNumRouteAttempts < ms_iMaxNumAttempts)
	{
		m_iNavMeshRouteMethod = eRouteMethod_ReducedObjectBoundingBoxes;

		// If we've failed to find a path in all direction, then randomise our wander heading
		if(m_bHadFailedInAllDirections)
			m_fWanderHeading = fwRandom::GetRandomNumberInRange(-PI, PI);

		UpdateWanderHeadingModifier();

		m_iNumRouteAttempts++;
	}
	else
	{
		m_iNavMeshRouteMethod = eRouteMethod_ReducedObjectBoundingBoxes;
		m_iNumRouteAttempts = 0;
		m_bHadFailedInAllDirections = true;
	}

	SetState(State_WaitingBetweenRepeatSearches);
	return FSM_Continue;
}

bool CTaskMoveWander::DoesWanderPathDoubleBack(const CPed * pPed)
{
	Assert(m_pRoute);
	if(m_pRoute->GetSize() < 2)
		return false;
	if(m_bHadFailedInAllDirections)
		return false;

	static const float fMaxWanderDiff = 110.0f*DtoR;
	static const float fMinPathDist = rage::square(3.f);
	static const int nNodesAhead = 2;

	const Vector3 vToWanderPos[nNodesAhead] = { m_pRoute->GetPosition(1), m_pRoute->GetPosition(rage::Min(2, m_pRoute->GetSize()-1)) };
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	float fBestHeadingDiff = 180.0f*DtoR;

	int nNodesAheadToCheck = 2;
	if (vPedPos.Dist2(m_pRoute->GetPosition(1)) > fMinPathDist)
		nNodesAheadToCheck = 1;

	for (int i = 0; i < nNodesAheadToCheck; ++i)
	{
		float fHeading = fwAngle::GetRadianAngleBetweenPoints(vToWanderPos[i].x, vToWanderPos[i].y, vPedPos.x, vPedPos.y);
		fHeading = fwAngle::LimitRadianAngle(fHeading);

		float fHeadingDiff = m_fWanderHeading - fHeading;
		fHeadingDiff = Abs(fwAngle::LimitRadianAngle(fHeadingDiff));

		fBestHeadingDiff = rage::Min(fHeadingDiff, fBestHeadingDiff);
	}

	if(fBestHeadingDiff > fMaxWanderDiff)
		return true;

	return false;
}

bool CTaskMoveWander::DoesNextWanderPathDoubleBack()
{
	Assert(m_pRoute);
	if(m_pRoute->GetSize() < 2)
	{
		return false;
	}

	static const float fMaxWanderDiff = 110.0f*DtoR;
	static const int nNodesAhead = 2;

	const Vector3 vToWanderPos[nNodesAhead] = { m_pRoute->GetPosition(1), m_pRoute->GetPosition(rage::Min(2, m_pRoute->GetSize()-1)) };

	float fPathHeading = fwAngle::GetRadianAngleBetweenPoints(vToWanderPos[1].x, vToWanderPos[1].y, vToWanderPos[0].x, vToWanderPos[0].y);
	fPathHeading = fwAngle::LimitRadianAngle(fPathHeading);

	float fHeadingDiff = m_fWanderHeading - fPathHeading;
	fHeadingDiff = Abs(fwAngle::LimitRadianAngle(fHeadingDiff));

	if(fHeadingDiff > fMaxWanderDiff)
	{
		return true;
	}

	return false;
}


// Builds a list of influence spheres which wandering peds can use to avoid shocking event locations
int CTaskMoveWander::BuildInfluenceSpheres(const CPed * pPed, const float&  fRadiusFromPed, TInfluenceSphere * pSpheres, const int iMaxNum)
{
	int iNumSpheres = 0;

	const ScalarV radiusFromPedV = LoadScalar32IntoScalarV(fRadiusFromPed);
	const ScalarV radiusSquaredFromPedV = Scale(radiusFromPedV, radiusFromPedV);

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);

		const CEventShocking::Tunables& tuning = shocking.GetTunables();
		const float fRadius = tuning.m_WanderInfluenceSphereRadius;
		if(fRadius <= SMALL_FLOAT)
		{
			continue;
		}

		const Vec3V originV = shocking.GetEntityOrSourcePosition();

		const ScalarV distSqV = DistSquared(originV, pPed->GetTransform().GetPosition());
		if(IsLessThanAll(distSqV, radiusSquaredFromPedV))
		{
			pSpheres[iNumSpheres].Init(RCC_VECTOR3(originV), fRadius, 1.0f*INFLUENCE_SPHERE_MULTIPLIER, 0.5f*INFLUENCE_SPHERE_MULTIPLIER);

			iNumSpheres++;
			if(iNumSpheres >= iMaxNum)
				break;
		}
	}

	return iNumSpheres;
}


#if !__FINAL && DEBUG_DRAW
void CTaskMoveWander::DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iTextStartLineOffset) const
{
	CTaskNavBase::DrawPedText(pPed, vWorldCoords, iTxtCol, iTextStartLineOffset);
}
#endif

#if !__FINAL
void CTaskMoveWander::Debug() const
{
	// Draw the point route which we keep local to the task (separate from the subtask's point-route).
	// This is so that the head-tracking code & avoidance code can have an accurate idea of where the
	// ped is going at any time..
#if DEBUG_DRAW
	static bool bDrawLocalPointRoute=true;
	if(bDrawLocalPointRoute && m_pRoute)
	{
		for(int p=0; p<m_pRoute->GetSize()-1; p++)
		{
			Vector3 v1 = m_pRoute->GetPosition(p) - Vector3(0.0f,0.0f,0.125f);
			Vector3 v2 = m_pRoute->GetPosition(p+1) - Vector3(0.0f,0.0f,0.125f);

			grcDebugDraw::Line(v1, v2, Color_azure1);
		}
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	const Vector3 vOffsetZ = ZAXIS * 0.5f;
	// Draw the wander dir
	Vector3 vWdrHdg(-rage::Sinf(m_fWanderHeading), rage::Cosf(m_fWanderHeading), 0.0f);
	grcDebugDraw::Line(vPedPosition - vOffsetZ, vPedPosition - vOffsetZ + (vWdrHdg * 2.0f), Color_orange4);

	// Draw the wander dir + the modifier
	float fHeadingPlusModifier = fwAngle::LimitRadianAngleSafe(m_fWanderHeading + m_fWanderHeadingModifier);
	Vector3 vWdrHdgMod(-rage::Sinf(fHeadingPlusModifier), rage::Cosf(fHeadingPlusModifier), 0.0f);
	grcDebugDraw::Line(vPedPosition - vOffsetZ, vPedPosition - vOffsetZ + (vWdrHdgMod * 1.5f), Color_purple2);
#endif

	if(GetSubTask())
		GetSubTask()->Debug();

}
#endif












//********************************************************************
//	CTaskMoveCrossRoadAtTrafficLights
//	The purpose of this task is to get the ped from one side of the
//	road to the other, taking into account the traffic lights
//********************************************************************
//static const u32 manhattanHash = atStringhash("Manhattan");
const float CTaskMoveCrossRoadAtTrafficLights::ms_fDefaultTargetRadius = 1.0f;
const s32 CTaskMoveCrossRoadAtTrafficLights::ms_iCheckLightsMinFreqMS = 100;
const s32 CTaskMoveCrossRoadAtTrafficLights::ms_iCheckLightsMaxFreqMS = 200;

CTaskMoveCrossRoadAtTrafficLights::Tunables CTaskMoveCrossRoadAtTrafficLights::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskMoveCrossRoadAtTrafficLights, 0x1f97b6c9);

CTaskMoveCrossRoadAtTrafficLights::CTaskMoveCrossRoadAtTrafficLights(const Vector3 & vStartPos, const Vector3 & vEndPos, const Vector3 * pExistingMoveTarget, int crossDelayMS, bool bIsTrafficLightCrossing, bool bIsMidRoadCrossing, bool bIsDriveWayCrossing)
: CTaskMove(MOVEBLENDRATIO_WALK)
, m_vStartPos(vStartPos)
, m_vEndPos(vEndPos)
//, m_vCrossingDir initialized below
, m_CheckLightsTimer()
//, m_vExistingMoveTarget initialized below
#if DEBUG_DRAW
, m_vDebugLightPosition(vStartPos)
#endif // DEBUG_DRAW
, m_fCrossingDistSq(0.0f)
, m_eLastLightState(0)
, m_iCrossingJunctionIndex(-1)
, m_iCrossDelayMS(crossDelayMS)
, m_pathBlockingObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_junctionStatus(eNotRegistered)
, m_bIsTrafficLightCrossing(bIsTrafficLightCrossing)
, m_bIsMidRoadCrossing(bIsMidRoadCrossing)
, m_bIsDriveWayCrossing(bIsDriveWayCrossing)
, m_bFirstTimeRun(false)
, m_bCrossingRoad(false)
, m_bWaiting(false)
, m_bPedHasExistingMoveTarget(false)
, m_bHasDecidedToRun(false)
, m_bCrossWithoutStopping(false)
, m_bPedIsNearToJunction(false)
, m_bPedIsJayWalking(false)
, m_bReadyToChainNextCrossing(false)
, m_bArrivingAtPavement(false)
#if DEBUG_DRAW
, m_bIsResumingCrossing(false)
#endif // DEBUG_DRAW
{
	m_vCrossingDir = m_vEndPos - m_vStartPos;
	m_fCrossingDistSq = m_vCrossingDir.Mag2();
	m_vCrossingDir.Normalize();

	if(pExistingMoveTarget)
	{
		m_vExistingMoveTarget = *pExistingMoveTarget;
		m_bPedHasExistingMoveTarget = true;
	}
	SetInternalTaskType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS);
}

CTaskMoveCrossRoadAtTrafficLights::~CTaskMoveCrossRoadAtTrafficLights()
{
}

aiTask* CTaskMoveCrossRoadAtTrafficLights::Copy() const
{
	CTaskMoveCrossRoadAtTrafficLights* pCopyToReturn = rage_new CTaskMoveCrossRoadAtTrafficLights(m_vStartPos, m_vEndPos, 
		m_bPedHasExistingMoveTarget ? &m_vExistingMoveTarget : NULL, m_iCrossDelayMS, m_bIsTrafficLightCrossing, m_bIsMidRoadCrossing, m_bIsDriveWayCrossing);
	if( AssertVerify(pCopyToReturn) )
	{
		pCopyToReturn->m_eLastLightState = m_eLastLightState;
		pCopyToReturn->m_iCrossingJunctionIndex = m_iCrossingJunctionIndex;
		pCopyToReturn->m_bCrossingRoad = m_bCrossingRoad;
		pCopyToReturn->m_bWaiting = m_bWaiting;
		pCopyToReturn->m_bHasDecidedToRun = m_bHasDecidedToRun;
		pCopyToReturn->m_bCrossWithoutStopping = m_bCrossWithoutStopping;
		pCopyToReturn->m_bPedIsJayWalking = m_bPedIsJayWalking;
		pCopyToReturn->m_bReadyToChainNextCrossing = m_bReadyToChainNextCrossing;
		pCopyToReturn->m_bArrivingAtPavement = m_bArrivingAtPavement;
		pCopyToReturn->m_junctionStatus = m_junctionStatus;
#if DEBUG_DRAW
		pCopyToReturn->m_bIsResumingCrossing = m_bIsResumingCrossing;
#endif // DEBUG_DRAW
	}
	return pCopyToReturn;
}

void CTaskMoveCrossRoadAtTrafficLights::CleanUp()
{
	HelperNotifyJunction(eNotRegistered);
	RemovePathBlockingObject();
	
	CTaskMove::CleanUp();
}

#if !__FINAL
void
CTaskMoveCrossRoadAtTrafficLights::Debug() const
{
#if DEBUG_DRAW

	Vector3 v1 = m_vStartPos;
	v1.z += 1.0f;
	Vector3 v2 = m_vEndPos;
	v2.z += 1.0f;

	// Draw the ped's light state knowledge
	if( IsCrossingAtLights() )
	{
		UpdateDebugLightPosition();
		Vector3 vLightStatusPos = m_vStartPos + Vector3(0.0f,0.0f,2.2f);
		float fLightStatusRadius = 0.2f;
		switch(m_eLastLightState)
		{
		case 0: // GREEN, peds should wait so draw RED
			grcDebugDraw::Sphere(vLightStatusPos, fLightStatusRadius, Color_red);
			grcDebugDraw::Line(vLightStatusPos, m_vDebugLightPosition, Color_red);
			break;
		case 1: // AMBER, peds should wait so draw RED
			grcDebugDraw::Sphere(vLightStatusPos, fLightStatusRadius, Color_red);
			grcDebugDraw::Line(vLightStatusPos, m_vDebugLightPosition, Color_red);
			break;
		case 2: // RED, peds can cross, so draw WHITE
			grcDebugDraw::Sphere(vLightStatusPos, fLightStatusRadius, Color_white);
			grcDebugDraw::Line(vLightStatusPos, m_vDebugLightPosition, Color_white);
			break;
		default:
			grcDebugDraw::Sphere(vLightStatusPos, fLightStatusRadius, Color_black);
			break;
		}
	}

	// Initialize line color
	Color32 lineCol(0xa0, 0xa0, 0x00, 0xff);
	// If the ped is holding a prop, use orange for the cross line
	if (GetPed()->IsHoldingProp())
	{
		lineCol = Color_orange;
	}
	// Draw a line between the start & end
	grcDebugDraw::Line(v1, v2, lineCol);

	// Initialize position crosses color
	Color32 posCol(0xff, 0xff, 0x00, 0xff);
	// If resuming crossing, use white for the start and end cross lines
	if( m_bIsResumingCrossing )
	{
		posCol = Color_white;
	}
	// Otherwise, if jaywalking, use black for the start and end cross lines
	else if(m_bPedIsJayWalking)
	{
		posCol = Color_black;
	}

	// Draw the start & end positions as crosses
	grcDebugDraw::Line(v1 - Vector3(1.0f, 0, 0), v1 + Vector3(1.0f, 0, 0), posCol);
	grcDebugDraw::Line(v1 - Vector3(0, 1.0f, 0), v1 + Vector3(0, 1.0f, 0), posCol);
	grcDebugDraw::Line(v2 - Vector3(1.0f, 0, 0), v2 + Vector3(1.0f, 0, 0), posCol);
	grcDebugDraw::Line(v2 - Vector3(0, 1.0f, 0), v2 + Vector3(0, 1.0f, 0), posCol);
#endif // DEBUG_DRAW

	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif // !__FINAL

#if DEBUG_DRAW
void
CTaskMoveCrossRoadAtTrafficLights::UpdateDebugLightPosition() const
{
	// Set debug position to the associated junction center
	if( m_iCrossingJunctionIndex != -1 )
	{
		CJunction* pJunction = CJunctions::GetJunctionByIndex(m_iCrossingJunctionIndex);
		if(pJunction)
		{
			m_vDebugLightPosition = pJunction->GetJunctionCenter();
			return;
		}
	}
	// otherwise, set debug position to start position
	m_vDebugLightPosition = m_vStartPos;
}
#endif // DEBUG_DRAW


CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(m_bFirstTimeRun)
	{
		// Compute whether we are not mid-road and close to junction
		m_bPedIsNearToJunction = false;
		if (!m_bIsMidRoadCrossing)
		{
			m_bPedIsNearToJunction = CJunctions::IsPedNearToJunction(m_vStartPos);
		}
		
		// Initialize known state of traffic light
		const bool bConsiderTimeRemaining = true;
		HelperUpdateKnownLightState(bConsiderTimeRemaining);
		// Use a random interval so peds are less likely to react to light change in unison
		m_CheckLightsTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(ms_iCheckLightsMinFreqMS, ms_iCheckLightsMaxFreqMS));

		m_bFirstTimeRun = false;
	}

FSM_Begin
	FSM_State(State_InitialDecide)
		FSM_OnUpdate
			return InitialDecide_OnUpdate(pPed);
	FSM_State(State_WalkingToCrossingPoint)
		FSM_OnEnter
			return WalkingToCrossingPoint_OnEnter(pPed);
		FSM_OnUpdate
			return WalkingToCrossingPoint_OnUpdate(pPed);
	FSM_State(State_TurningToFaceCrossing)
		FSM_OnEnter
			return TurningToFaceCrossing_OnEnter(pPed);
		FSM_OnUpdate
			return TurningToFaceCrossing_OnUpdate(pPed);
	FSM_State(State_WaitingForGapInTraffic);
		FSM_OnEnter
			return WaitingForGapInTraffic_OnEnter(pPed);
		FSM_OnUpdate
			return WaitingForGapInTraffic_OnUpdate(pPed);
		FSM_OnExit
			return WaitingForGapInTraffic_OnExit(pPed);
	FSM_State(State_WaitingForLightsToChange);
		FSM_OnEnter
			return WaitingForLightsToChange_OnEnter(pPed);
		FSM_OnUpdate
			return WaitingForLightsToChange_OnUpdate(pPed);
		FSM_OnExit
			return WaitingForLightsToChange_OnExit(pPed);
	FSM_State(State_CrossingRoad)
		FSM_OnEnter
			return CrossingRoad_OnEnter(pPed);
		FSM_OnUpdate
			return CrossingRoad_OnUpdate(pPed);
		FSM_OnExit
			return CrossingRoad_OnExit(pPed);
FSM_End
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::InitialDecide_OnUpdate(CPed * pPed)
{
	// Check if ped was already crossing the road previously
	if( pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->ShouldResumeCrossingRoad() )
	{
		// cross to the destination point
		SetState(State_CrossingRoad);
#if DEBUG_DRAW
		m_bIsResumingCrossing = true;
#endif // DEBUG_DRAW
	}
	else
	{
		// otherwise walk to the crossing point
		SetState(State_WalkingToCrossingPoint);
	}
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WalkingToCrossingPoint_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	//***********************************************************************************************
	// Get the state of the traffic-lights. NB : This is the lights for the PED not for the cars!
	//
	// If lights are amber or red, then we will make the ped walk to the start-pos (on their side
	// of the road), then turn to face their destination and then wait for a second or two (maybe
	// looking around - play the 'road-cross' anim).
	// If lights are green, we can just cross the road immediately.

	const float fCrossSpeed = MOVEBLENDRATIO_WALK;
	/*
	const float fCrossSpeed = (pPed->GetPedType()==PEDTYPE_COP) ?
		MOVEBLENDRATIO_WALK :
		MOVEBLENDRATIO_WALK + fwRandom::GetRandomNumberInRange(0.0f, 0.5f);
	*/

	// Compute our desired heading to cross
	const float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_vEndPos.x, m_vEndPos.y, m_vStartPos.x, m_vStartPos.y);

	CTaskMoveFollowNavMesh * pTaskNavMesh = rage_new CTaskMoveFollowNavMesh(fCrossSpeed, m_vStartPos, ms_fDefaultTargetRadius);
	pTaskNavMesh->SetKeepMovingWhilstWaitingForPath(true);
	pTaskNavMesh->SetGoStraightToCoordIfPathNotFound(true);
	pTaskNavMesh->SetTargetStopHeading(fHeading);

	SetNewTask(pTaskNavMesh);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WalkingToCrossingPoint_OnUpdate(CPed * pPed)
{
	// We should be running a navmesh follow subtask
	if(AssertVerify(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
	{
		// Check if we have traffic lights to consider
		if( IsCrossingAtLights() )
		{
			// Monitor the light state
			const bool bConsiderTimeRemaining = true;
			HelperUpdateKnownLightState(bConsiderTimeRemaining);
		}

		// Check if we can transition directly to crossing
		if( CanBeginCrossingDriveWayDirectly(pPed) || CanBeginCrossingRoadDirectly(pPed) )
		{
			// Mark flag to indicate no stopping, please
			m_bCrossWithoutStopping = true;

			// Mark flag to keep current subtask running
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

			// Go directly to crossing
			SetState(State_CrossingRoad);
			
			return FSM_Continue;
		}
	}

	// Check if the move goal is obstructed by the player
	bool bMoveGoalObstructedByPlayer = false;
	if( IsPedCloseToPosition(VECTOR3_TO_VEC3V(m_vStartPos), sm_Tunables.m_fPlayerObstructionCheckRadius) && IsPlayerObstructingPosition(VECTOR3_TO_VEC3V(m_vStartPos)) )
	{
		bMoveGoalObstructedByPlayer = true;
	}

	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bMoveGoalObstructedByPlayer )
	{
		// Compute our desired heading to cross
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_vEndPos.x, m_vEndPos.y, vPedPosition.x, vPedPosition.y);

		// If we are already facing acceptably, wait or cross
		if( CTaskMoveAchieveHeading::IsHeadingAchieved(pPed, fHeading) )
		{
			SetWaitOrCrossRoadState(pPed);
		}
		else // Otherwise, turn to face as needed
		{
			SetState(State_TurningToFaceCrossing);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::TurningToFaceCrossing_OnEnter(CPed * pPed)
{
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_vEndPos.x, m_vEndPos.y, vPedPosition.x, vPedPosition.y);
	CTaskMoveAchieveHeading * pTaskHeading = rage_new CTaskMoveAchieveHeading(fHeading);
	SetNewTask(pTaskHeading);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::TurningToFaceCrossing_OnUpdate(CPed * pPed)
{
	Assert(!GetSubTask() || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_ACHIEVE_HEADING);

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetWaitOrCrossRoadState(pPed);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WaitingForGapInTraffic_OnEnter(CPed * pPed)
{
	HelperNotifyJunction(eWaiting);
	AddPathBlockingObject(pPed);
	SetNewTask(CreateWaitTask());
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WaitingForGapInTraffic_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_WAIT_FOR_TRAFFIC);

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		CTaskMoveWaitForTraffic * pTrafficTask = (CTaskMoveWaitForTraffic*)GetSubTask();

		if(pTrafficTask->GetWasSafeToCross())
		{
			SetState(State_CrossingRoad);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	// Use kinematic physics while waiting
	pPed->SetPedResetFlag(CPED_RESET_FLAG_UseKinematicPhysics, true);

	return FSM_Continue;
}

CTask::FSM_Return
	CTaskMoveCrossRoadAtTrafficLights::WaitingForGapInTraffic_OnExit(CPed * UNUSED_PARAM(pPed))
{
	RemovePathBlockingObject();
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WaitingForLightsToChange_OnEnter(CPed * pPed)
{
	HelperNotifyJunction(eWaiting);
	// Periodically check lights, on short interval for performance
	const int perfWaitMS = 100;
	m_CheckLightsTimer.Set(fwTimer::GetTimeInMilliseconds(), perfWaitMS);

	AddPathBlockingObject(pPed);

	// Set task to wait forever
	SetNewTask(CreateWaitTask());

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::WaitingForLightsToChange_OnUpdate(CPed * pPed)
{
	// If timer is out of time
	if( m_CheckLightsTimer.IsOutOfTime() )
	{
		// check lights for crossing
		const bool bConsiderTimeRemaining = true;
		HelperUpdateKnownLightState(bConsiderTimeRemaining);
		
		// if lights are red, or we are going to jaywalk
		if( m_eLastLightState == LIGHT_RED || CanJayWalkAtLights(pPed) )
		{
			// Set new wait subtask using assigned crossing delay as duration
			SetNewTask(rage_new CTaskMoveStandStill(m_iCrossDelayMS));

			// Clear the check timer, so IsOutOfTime will be false from now on
			m_CheckLightsTimer.Unset();
		}
		// we should keep waiting
		else
		{
			// Periodically check lights, on short interval for performance
			const int perfWaitMS = 100;
			m_CheckLightsTimer.Set(fwTimer::GetTimeInMilliseconds(), perfWaitMS);
		}
	}

	// If waiting subtask has ended
	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetState(State_CrossingRoad);
	}

	// Use kinematic physics while waiting
	pPed->SetPedResetFlag(CPED_RESET_FLAG_UseKinematicPhysics, true);

	return FSM_Continue;
}

CTask::FSM_Return
	CTaskMoveCrossRoadAtTrafficLights::WaitingForLightsToChange_OnExit(CPed * UNUSED_PARAM(pPed))
{
	RemovePathBlockingObject();
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::CrossingRoad_OnEnter(CPed * pPed)
{
	// No longer waiting
	m_bWaiting = false;

	// Actively crossing
	m_bCrossingRoad = true;

	// Record the ped beginning to cross the road, including start and end positions
	pPed->GetPedIntelligence()->RecordBeganCrossingRoad(m_vStartPos, m_vEndPos);

	// Add this ped to the junction count
	HelperNotifyJunction(eCrossing);

	// If we should cross without stopping
	// and we have an actively running nav mesh follow subtask
	if( m_bCrossWithoutStopping && GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		// Get a handle to the subtask
		CTaskMoveFollowNavMesh* pNavTask = static_cast<CTaskMoveFollowNavMesh*>(GetSubTask());

		// Mark it to keep moving while waiting for a path
		pNavTask->SetKeepMovingWhilstWaitingForPath(true);

		// Give it the crossing endpoint as a new destination
		pNavTask->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vEndPos));
	}
	else // otherwise start a new crossing task
	{
		SetNewTask(CreateCrossRoadTask(pPed));
	}

	//Reset and start the pavement search helper
	const float fSearchRadius = 2.f;
	m_pavementHelper.ResetSearchResults();
	m_pavementHelper.SetSearchRadius(fSearchRadius);
	m_pavementHelper.StartPavementFloodFillRequest(pPed, m_vEndPos);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::CrossingRoad_OnUpdate(CPed * pPed)
{
	m_pavementHelper.UpdatePavementFloodFillRequest(pPed, m_vEndPos);

	// Check if the move goal is obstructed by the player
	bool bGoalObstructedByPlayer = false;
	if( IsPedCloseToPosition(VECTOR3_TO_VEC3V(m_vEndPos), sm_Tunables.m_fPlayerObstructionCheckRadius) && IsPlayerObstructingPosition(VECTOR3_TO_VEC3V(m_vEndPos)) )
	{
		bGoalObstructedByPlayer = true;
	}

	// If there is not subtask or it is finished
	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bGoalObstructedByPlayer )
	{
		// Mark road as crossed
		pPed->GetPedIntelligence()->RecordFinishedCrossingRoad();

		// We have crossed the road, so quit
		return FSM_Quit;
	}

	// Check if the ped is getting close to the end
	// Local variable used by a few different checks
	const float fCloseToEndDist = 4.0f;
	bool bIsCloseToEndPos = IsPedCloseToPosition(VECTOR3_TO_VEC3V(m_vEndPos), fCloseToEndDist);

	// Check if we are arriving at pavement close to the goal
	if( !m_bArrivingAtPavement )
	{
		// First check for the pavement flag
		bool bPedWillArriveAtPavement = m_pavementHelper.HasPavement();
		if( bPedWillArriveAtPavement )
		{
			// Check if we are getting close to the goal position
			if( bIsCloseToEndPos )
			{
				if( pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement)
				{
					// mark member flag to signal parent task
					m_bArrivingAtPavement = true;
				}
			}
		}
	}

	// Check if we are crossing at lights and ready to chain into another crossing ahead
	if( !m_bReadyToChainNextCrossing && IsCrossingAtLights() )
	{
		// Check if we are getting close to the goal position
		if( bIsCloseToEndPos )
		{
			// mark member flag to signal parent task
			m_bReadyToChainNextCrossing = true;
		}
	}

	// If the ped has decided to hurry across and has a navmesh follow subtask
	if( m_bHasDecidedToRun && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		// get move follow nav mesh subtask
		CTaskMoveFollowNavMesh * pNavTask = static_cast<CTaskMoveFollowNavMesh*>(GetSubTask());

		// check for still or walking
		const bool bIsStill = CPedMotionData::GetIsStill(pNavTask->GetMoveBlendRatio());
		const bool bIsWalking = CPedMotionData::GetIsWalking(pNavTask->GetMoveBlendRatio());

		// if stationary or walking and not close to the end
		if( !bIsCloseToEndPos && (bIsStill || bIsWalking) )
		{
			// move at a randomized increased speed
			const float fRunAmt = fwRandom::GetRandomNumberInRange(0.25f, 1.0f);
			float fMBR = MOVEBLENDRATIO_WALK + fRunAmt;

			// When peds run with brollies it looks daft
			if(CTaskWander::GetDoesPedHaveBrolly(pPed))
			{
				fMBR = Max(fMBR, 1.5f);
			}

			pNavTask->SetMoveBlendRatio(fMBR);

			pPed->RandomlySay("LATE", 0.5f);
		}
		// otherwise, if close to the end, and moving faster than a walk speed
		else if( bIsCloseToEndPos && (!bIsStill && !bIsWalking) )
		{
			// slow to a walk
			pNavTask->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
		}
	}

	//*******************************************************************************************
	// If the ped is currently crossing the road, and the traffic lights have turned amber -
	// then we can randomly get some peds to run..  Only do this if we're not crossing mid-road,
	// since peds will already be hurrying in that case.
	const float s_fForceHurryCrossingDistSq = 24.0f * 24.0f;
	if( !m_bHasDecidedToRun && pPed->GetPedType()!=PEDTYPE_COP && !m_bIsMidRoadCrossing && IsCrossingAtLights() && !pPed->IsHoldingProp() 
		&& (pPed->GetPedModelInfo()->GetPersonalitySettings().AllowRoadCrossHurryOnLightChange() || (m_fCrossingDistSq >= s_fForceHurryCrossingDistSq)) )
	{	
		// Periodically update the light state
		if(!m_CheckLightsTimer.IsSet() || m_CheckLightsTimer.IsOutOfTime())
		{
			const bool bConsiderTimeRemaining = false;
			HelperUpdateKnownLightState(bConsiderTimeRemaining);

			// Use a random interval so peds are less likely to react to light change in unison
			m_CheckLightsTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(ms_iCheckLightsMinFreqMS, ms_iCheckLightsMaxFreqMS));
		}

		// If the light is amber or green
		if( m_eLastLightState==LIGHT_AMBER || m_eLastLightState==LIGHT_GREEN )
		{
			// Check to prevent starting to run too close to the destination
			const float fTooCloseToEndDist = 4.0f;
			if(!IsPedCloseToPosition(VECTOR3_TO_VEC3V(m_vEndPos), fTooCloseToEndDist))
			{
				// decide to run in response to the light change
				m_bHasDecidedToRun = true;
			}
		}
	}
	else if(m_bIsMidRoadCrossing)
	{
		//Limit the speed if the ped is holding an object
		if(pPed->GetHeldObject(*pPed) != NULL && pPed->GetVelocity().Mag2() > 4.0f )
		{
			CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)GetSubTask();
			const float fRunAmt = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);
			pNavTask->SetMoveBlendRatio( MOVEBLENDRATIO_WALK + fRunAmt);
		}

		//We are jay walking in the middle of the street, allow vehicles to run the ped over.
		m_bPedIsJayWalking = true;
	}

	//If the ped is jay walking on purpose then set the reset flag so that vehicles don't stop for them and run them over.
	if (m_bPedIsJayWalking)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_JayWalking, true);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrossRoadAtTrafficLights::CrossingRoad_OnExit(CPed * UNUSED_PARAM(pPed))
{
	HelperNotifyJunction(eNotRegistered);

	m_pavementHelper.CleanupPavementFloodFillRequest();

	return FSM_Continue;
}

void CTaskMoveCrossRoadAtTrafficLights::AddPathBlockingObject(const CPed* pPed)
{
	if( m_pathBlockingObjectIndex == DYNAMIC_OBJECT_INDEX_NONE )
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vSize = (pPed->GetBoundingBoxMax() - pPed->GetBoundingBoxMin())/2;
		vSize.x = Max(0.0f, vSize.x - 0.5f);	// MAGIC! Reduce bounds slightly to reduce instances of sidewalks blocked for wandering peds
		vSize.y = Max(0.0f, vSize.y - 0.5f);
		float heading = pPed->GetTransform().GetHeading();
		m_pathBlockingObjectIndex = CPathServerGta::AddBlockingObject(vPos, vSize, heading, TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	}
}

void CTaskMoveCrossRoadAtTrafficLights::RemovePathBlockingObject()
{
	if( m_pathBlockingObjectIndex != DYNAMIC_OBJECT_INDEX_NONE )
	{
		CPathServerGta::RemoveBlockingObject(m_pathBlockingObjectIndex);
		m_pathBlockingObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}
}

int CTaskMoveCrossRoadAtTrafficLights::SetWaitOrCrossRoadState(CPed * pPed)
{
	const bool bConsiderTimeRemaining = true;
	HelperUpdateKnownLightState(bConsiderTimeRemaining);

	int iNextState;

	bool bIgnoreRules = CanJayWalkAtLights(pPed);
	const bool bCrossAtLights = IsCrossingAtLights();

	if((bCrossAtLights && m_eLastLightState==LIGHT_RED) || bIgnoreRules)
	{
		iNextState = State_CrossingRoad;
	}
	else
	{
		iNextState = bCrossAtLights ? State_WaitingForLightsToChange : State_WaitingForGapInTraffic;
	}

	SetState(iNextState);
	return iNextState;
}

bool CTaskMoveCrossRoadAtTrafficLights::CanJayWalkAtLights(CPed * pPed)
{
	// Check if crossing a driveway
	if( m_bIsDriveWayCrossing )
	{
		return false;
	}

	// Check if the crossing distance is too great
	const float s_fMaxCrossDistForJayWalkSq = 18.0f * 18.0f;
	if( m_fCrossingDistSq >= s_fMaxCrossDistForJayWalkSq )
	{
		return false;
	}

	//Check random event manager it's allowed to jay walk
	if (!CRandomEventManager::GetInstance().CanCreateEventOfType(RC_PED_JAY_WALK_LIGHTS))
	{
		return false;
	}

	//Check that the ped is within 50meters of the player, this will make sure the vehicle isn't dummy
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if ((DistSquared(pPed->GetTransform().GetPosition(), pPlayer->GetTransform().GetPosition())).Getf() > rage::square(50.0f))
	{
		return false;
	}

	m_bPedIsJayWalking = true;
	CRandomEventManager::GetInstance().SetEventStarted(RC_PED_JAY_WALK_LIGHTS);
	return true;
}

bool CTaskMoveCrossRoadAtTrafficLights::CanBeginCrossingRoadDirectly(CPed* pPed) const
{
	// Check if there are no traffic lights to consider
	if( !IsCrossingAtLights() )
	{
		// no launch position to skip over in our path
		return false;
	}

	// Check if the ped is not close or misaligned to start
	if( !CanBeginCrossingPosAndHeading(pPed) )
	{
		// cannot cross
		return false;
	}

	// Check if the light for the cross traffic is red
	if( m_eLastLightState == LIGHT_RED )
	{
		// yes, it is safe for us to cross
		return true;
	}

	// Cannot cross by default
	return false;
}

bool
CTaskMoveCrossRoadAtTrafficLights::CanBeginCrossingDriveWayDirectly(CPed* pPed) const
{
	// First check that this is a drive way crossing
	if( !m_bIsDriveWayCrossing )
	{
		return false;
	}

	// Check if the ped is close and aligned to start
	if( CanBeginCrossingPosAndHeading(pPed) )
	{
		// all check passed
		return true;
	}

	// Cannot cross by default
	return false;
}

bool
CTaskMoveCrossRoadAtTrafficLights::CanBeginCrossingPosAndHeading(CPed* pPed) const
{
	// Check if we are getting close to the crossing start position
	const float fConsiderCrossingDist = 4.0f;
	if( !IsPedCloseToPosition(VECTOR3_TO_VEC3V(m_vStartPos), fConsiderCrossingDist) )
	{
		// too far from the crossing start position, we cannot cross yet
		return false;
	}

	// Check if our heading aligns enough with the crossing direction
	const float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_vEndPos.x, m_vEndPos.y, m_vStartPos.x, m_vStartPos.y);
	const float fHeadingTolerance = 1.04f;// about 60 degrees
	if( !CTaskMoveAchieveHeading::IsHeadingAchieved(pPed, fHeading, fHeadingTolerance) )
	{
		// heading is too far off, we cannot cross yet
		return false;
	}

	// Checks passed, may cross
	return true;
}

bool
CTaskMoveCrossRoadAtTrafficLights::IsCrossingAtLights() const
{
	return (!m_bIsDriveWayCrossing && (m_bIsTrafficLightCrossing || m_bPedIsNearToJunction));
}

bool
CTaskMoveCrossRoadAtTrafficLights::IsPedCloseToPosition(const Vec3V& queryPosition, float fQueryDistance) const
{
	const CPed* pPed = GetPed();
	if( pPed )
	{
		const ScalarV queryDistSq = LoadScalar32IntoScalarV(rage::square(fQueryDistance));
		const Vec3V pedPosition = pPed->GetTransform().GetPosition();
		const ScalarV distSq = DistSquared(pedPosition, queryPosition);
		if( IsLessThanOrEqualAll(distSq, queryDistSq) )
		{
			return true;
		}
	}
	return false;
}

bool
CTaskMoveCrossRoadAtTrafficLights::IsPlayerObstructingPosition(const Vec3V& queryPosition) const
{
	// Get a handle to the local player, if any
	const CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if( pPlayerPed )
	{
		// Compute the local player position, adjusting the height down by the root to ground offset
		const float fPedHeightOffset = pPlayerPed->GetCapsuleInfo() ? pPlayerPed->GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;
		const Vec3V vLocalPlayerGroundPosition = pPlayerPed->GetTransform().GetPosition() + Vec3V(0.0f,0.0f,-fPedHeightOffset);
		
		// Get the distance squared from the query point to the local player's ground position
		const ScalarV queryDistSq = DistSquared(queryPosition, vLocalPlayerGroundPosition);

		// Check if the distance is within obstruction range
		if( IsLessThanOrEqualAll(queryDistSq, LoadScalar32IntoScalarV(rage::square(sm_Tunables.m_fPlayerObstructionRadius))) )
		{
			return true;
		}
	}
	
	return false;
}

// CreateWaitTask
// This creates the task we'll use to wait until the lights turn red
CTask *
CTaskMoveCrossRoadAtTrafficLights::CreateWaitTask()
{
	m_bWaiting = true;
	if(IsCrossingAtLights())
	{
		const bool bForever = true;
		CTaskMoveStandStill * pTaskStill = rage_new CTaskMoveStandStill(-1, bForever);
		return pTaskStill;
	}
	else
	{
		return rage_new CTaskMoveWaitForTraffic(m_vStartPos, m_vEndPos, m_bIsMidRoadCrossing);
	}
}

CTask *
CTaskMoveCrossRoadAtTrafficLights::CreateCrossRoadTask(CPed * pPed)
{
	float fCrossSpeed = m_bIsMidRoadCrossing ?
		MOVEBLENDRATIO_WALK + fwRandom::GetRandomNumberInRange(0.75f, 1.5f)	:
			MOVEBLENDRATIO_WALK + fwRandom::GetRandomNumberInRange(0.0f, 0.25f);

	// When peds run with brollies it looks daft
	if(CTaskWander::GetDoesPedHaveBrolly(pPed))
		fCrossSpeed = Max(fCrossSpeed, 1.5f);

	static dev_bool bRecalculatePaths = true;

	CTaskMoveFollowNavMesh * pTaskNavMesh = rage_new CTaskMoveFollowNavMesh(fCrossSpeed, m_vEndPos);
	pTaskNavMesh->SetGoStraightToCoordIfPathNotFound(true);
	pTaskNavMesh->SetDontStandStillAtEnd(true);
	if(bRecalculatePaths)
		pTaskNavMesh->SetRecalcPathFrequency(6000, fwRandom::GetRandomNumberInRange(4000, 6000));

	return pTaskNavMesh;
}

void 
CTaskMoveCrossRoadAtTrafficLights::HelperUpdateKnownLightState(bool bConsiderTimeRemaining)
{
	// NOTE: When m_iCrossingJunctionIndex is -1, we scan for closest junction and set the index
	//       Thereafter we use the index directly with no need to search
	const Vector3 vCrossingJunctionSearchCentre = (m_vStartPos + m_vEndPos)/2.0f;
	m_eLastLightState = (eTrafficLightColour) CJunctions::GetLightStatusForPed(vCrossingJunctionSearchCentre, m_vCrossingDir, bConsiderTimeRemaining, m_iCrossingJunctionIndex);
}

void
CTaskMoveCrossRoadAtTrafficLights::HelperNotifyJunction(JunctionStatus newStatus)
{
	if(newStatus != m_junctionStatus)
	{
		// Find the closest junction to the crossing start position
		if( m_iCrossingJunctionIndex != -1 )
		{
			CJunction* pJunction = CJunctions::GetJunctionByIndex(m_iCrossingJunctionIndex);
			if(pJunction)
			{
				switch (newStatus)
				{
				case eNotRegistered:
					{
						if(m_junctionStatus == eCrossing)
							pJunction->RemoveActivelyCrossingPed();

						if(m_junctionStatus == eWaiting)
							pJunction->RemoveWaitingForLightPed();
					}
					break;
				case eCrossing:
					{
						if(m_junctionStatus == eWaiting)
							pJunction->RemoveWaitingForLightPed();

						pJunction->RegisterActivelyCrossingPed();
					}
					break;
				case eWaiting:
					{
						if(m_junctionStatus == eCrossing)
							pJunction->RemoveActivelyCrossingPed();

						pJunction->RegisterWaitingForLightPed();
					}
				default:
					break;
				}

				m_junctionStatus = newStatus;
			}
		}
	}	
}


//*******************************************************************************
//	CTaskMoveWaitForTraffic
//	This task is used when peds are to cross the road at a mid-road crossing
//	spot, where there are no traffic lights to dictate when its safe to cross.
//	The task periodically examines the passing traffic to try and determine when
//	is a safe time to cross to the other side.
//	It is currently only used by 'CTaskMoveCrossRoadAtTrafficLights'
//	(which is confusingly named since it implies the presence of traffic lights)

const u32 CTaskMoveWaitForTraffic::ms_iScanTime = 2000;
const u32 CTaskMoveWaitForTraffic::ms_iDefaultGiveUpTime = 10000;

CTaskMoveWaitForTraffic::CTaskMoveWaitForTraffic(const Vector3 & vStartPos, const Vector3 & vEndPos, const bool bMidRoadCrossing)
: CTaskMove(MOVEBLENDRATIO_STILL),
	m_vStartPos(vStartPos),
	m_vEndPos(vEndPos),
	m_bWasSafe(false),
	m_bIsMidRoadCrossing(bMidRoadCrossing)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WAIT_FOR_TRAFFIC);
}

CTaskMoveWaitForTraffic::~CTaskMoveWaitForTraffic()
{

}

#if !__FINAL
void CTaskMoveWaitForTraffic::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

CTask::FSM_Return
CTaskMoveWaitForTraffic::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_WaitingUntilSafeToCross)
			FSM_OnEnter
				return WaitingUntilSafeToCross_OnEnter(pPed);
			FSM_OnUpdate
				return WaitingUntilSafeToCross_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveWaitForTraffic::Initial_OnUpdate(CPed* pPed)
{
	m_GiveUpTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_iDefaultGiveUpTime);

	if(IsSafeToCross(pPed))
	{
		m_bWasSafe = true;
		return FSM_Quit;
	}
	else
	{
		SetState(State_WaitingUntilSafeToCross);
		return FSM_Continue;
	}
}

CTask::FSM_Return
CTaskMoveWaitForTraffic::WaitingUntilSafeToCross_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskMoveStandStill(5000));
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveWaitForTraffic::WaitingUntilSafeToCross_OnUpdate(CPed * pPed)
{
	// Unexpected state?  Not quite sure how this can not have a subtask since the FSM changeover :s
	if(!GetSubTask() || GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_STAND_STILL)
	{
		return FSM_Quit;
	}
	if(m_GiveUpTimer.IsOutOfTime() && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		return FSM_Quit;
	}

	if(!m_ScanTimer.IsSet() || m_ScanTimer.IsOutOfTime())
	{
		if(IsSafeToCross(pPed) && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			m_bWasSafe = true;
			return FSM_Quit;
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_WaitingUntilSafeToCross);
	}

	return FSM_Continue;
}


bool gWaitForTraffic_CrossingBlocked;
Vector3 gWaitForTraffic_StartPos;
Vector3 gWaitForTraffic_EndPos;
Vector3 gWaitForTraffic_RightVec;
float gWaitForTraffic_RightPlaneDist;
Vector3 gWaitForTraffic_CrossingNormal;
float gWaitForTraffic_CrossingPlaneDist;
float gCrossingWidth;

// return TRUE to continue with further callbacks
bool IsCarPreventingCrossing(CEntity* pEntity, void * pData)
{
	Assert(pEntity->GetIsTypeVehicle());
	CVehicle * pVehicle = (CVehicle*)pEntity;

	Vector3 vPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

	// Is vehicle roughly same Z ?
	if(!rage::IsNearZero(vPos.z - gWaitForTraffic_StartPos.z, 4.0f))
	{
#if DEBUG_DRAW
		if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff) grcDebugDraw::Line(gWaitForTraffic_StartPos, vPos, Color_green);
#endif
		return true;
	}

	// Is vehicle between the start & end points, as we look across the road?
	float fDistInFront = DotProduct(vPos, gWaitForTraffic_CrossingNormal) + gWaitForTraffic_CrossingPlaneDist;
	if(fDistInFront < 0.0f || fDistInFront > gCrossingWidth)
	{
#if DEBUG_DRAW
		if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff) grcDebugDraw::Line(gWaitForTraffic_StartPos, vPos, Color_green);
#endif
		return true;
	}

	// Get speed projected onto right vector
	float fDistToRight = DotProduct(vPos, gWaitForTraffic_RightVec) + gWaitForTraffic_RightPlaneDist;
	Vector3 vMoveSpeed = pVehicle->GetVelocity();
	float fSpeedInTangent = DotProduct(vMoveSpeed, gWaitForTraffic_RightVec);

	static const float fStationarySpeed = 0.5f;

	// Is vehicle roughly stationary, and sitting in the way?
	Assert( ((CEntity*)pData)->GetIsTypePed());
	CPed * pPed = (CPed*)pData;
	CEntity & entRef = *pEntity;
	CEntityBoundAI bnd(entRef, vPos.z, pPed->GetCapsuleInfo()->GetHalfWidth());
	if(!bnd.TestLineOfSight(VECTOR3_TO_VEC3V(gWaitForTraffic_StartPos), VECTOR3_TO_VEC3V(gWaitForTraffic_EndPos)) && fSpeedInTangent < fStationarySpeed)
	{
		gWaitForTraffic_CrossingBlocked = true;

#if DEBUG_DRAW
		if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff) grcDebugDraw::Line(gWaitForTraffic_StartPos, vPos, Color_red);
#endif
		return true;
	}

	// Is vehicle approaching from either side?
	if((fDistToRight > 0.0f && fSpeedInTangent < -fStationarySpeed) || (fDistToRight < 0.0f && fSpeedInTangent > fStationarySpeed))
	{
		gWaitForTraffic_CrossingBlocked = true;

#if DEBUG_DRAW
		if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff) grcDebugDraw::Line(gWaitForTraffic_StartPos, vPos, Color_red);
#endif
		return true;
	}

#if DEBUG_DRAW
	if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff) grcDebugDraw::Line(gWaitForTraffic_StartPos, vPos, Color_green);
#endif

	return true;
}

bool
CTaskMoveWaitForTraffic::IsSafeToCross(CPed * pPed)
{
	Vector3 vCrossingMid = (m_vStartPos + m_vEndPos) * 0.5f;
	Vector3 vCrossingDir = m_vEndPos - m_vStartPos;
	Vector3 vCrossingNormal = vCrossingDir;
	vCrossingNormal.Normalize();

	Vector3 vRight = CrossProduct(vCrossingNormal, Vector3(0.0f,0.0f,1.0f));

	gWaitForTraffic_StartPos = m_vStartPos;
	gWaitForTraffic_EndPos = m_vEndPos;
	gWaitForTraffic_CrossingNormal = vCrossingNormal;
	gWaitForTraffic_CrossingPlaneDist = - DotProduct(m_vStartPos, vCrossingNormal);
	gCrossingWidth = (m_vEndPos - m_vStartPos).XYMag();
	gWaitForTraffic_RightVec = vRight;
	gWaitForTraffic_RightPlaneDist = - DotProduct(m_vStartPos, vRight);
	gWaitForTraffic_CrossingBlocked = false;

	ScalarV vSearchSize(20.f);
	if(m_bIsMidRoadCrossing)
	{
		vSearchSize = ScalarV(30.0f);
	}
	fwIsSphereIntersecting sp(spdSphere(RCC_VEC3V(vCrossingMid), vSearchSize));
	CGameWorld::ForAllEntitiesIntersecting(&sp, IsCarPreventingCrossing, (void*)pPed, ENTITY_TYPE_MASK_VEHICLE, SEARCH_LOCATION_EXTERIORS);

	m_ScanTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_iScanTime);

	return !gWaitForTraffic_CrossingBlocked;
}







//********************************************************************
//	CTaskMoveGoToShelterAndWait
//	Locates a sheltered position using the navmesh, and moves to it.

const float CTaskMoveGoToShelterAndWait::ms_fShelterTargetRadius = 1.0f;

CTaskMoveGoToShelterAndWait::CTaskMoveGoToShelterAndWait(const float fMoveBlendRatio, const float fMaxSearchDist, s32 iMaxWaitTime) :
	CTaskMove(fMoveBlendRatio),
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_fMaxShelterSearchDist(fMaxSearchDist),
	m_iMaxWaitTime(iMaxWaitTime),
	m_vShelterPosition(0.0f,0.0f,0.0f),
	m_bFirstTime(true),
	m_bHasSpecifiedTarget(false),
	m_hFloodFillPathHandle(PATH_HANDLE_NULL)
{
	Assert(m_iMaxWaitTime==-1 || m_iMaxWaitTime > 0);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_SHELTER_AND_WAIT);
}

CTaskMoveGoToShelterAndWait::CTaskMoveGoToShelterAndWait(const float fMoveBlendRatio, const Vector3 & vTargetPos, s32 iMaxWaitTime) :
	CTaskMove(fMoveBlendRatio),
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_fMaxShelterSearchDist(0.0f),
	m_iMaxWaitTime(iMaxWaitTime),
	m_vShelterPosition(vTargetPos),
	m_bFirstTime(true),
	m_bHasSpecifiedTarget(true),
	m_hFloodFillPathHandle(PATH_HANDLE_NULL)
{
	Assert(m_iMaxWaitTime==-1 || m_iMaxWaitTime > 0);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_SHELTER_AND_WAIT);
}

CTaskMoveGoToShelterAndWait::~CTaskMoveGoToShelterAndWait()
{
	Assert(m_hFloodFillPathHandle==PATH_HANDLE_NULL);

	if(m_hFloodFillPathHandle!=PATH_HANDLE_NULL)
		CPathServer::CancelRequest(m_hFloodFillPathHandle);
}

#if !__FINAL
void
CTaskMoveGoToShelterAndWait::Debug() const
{
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
#endif

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_WaitingToFindShelter)
			FSM_OnEnter
				return WaitingToFindShelter_OnEnter(pPed);
			FSM_OnUpdate
				return WaitingToFindShelter_OnUpdate(pPed);
		FSM_State(State_MovingToShelter)
			FSM_OnEnter
				return MovingToShelter_OnEnter(pPed);
			FSM_OnUpdate
				return MovingToShelter_OnUpdate(pPed);
		FSM_State(State_WaitingInShelter)
			FSM_OnEnter
				return WaitingInShelter_OnEnter(pPed);
			FSM_OnUpdate
				return WaitingInShelter_OnUpdate(pPed);
	FSM_End
}


CTask::FSM_Return
CTaskMoveGoToShelterAndWait::Initial_OnUpdate(CPed * pPed)
{
	Assert(m_hFloodFillPathHandle==PATH_HANDLE_NULL);
	Assert(GetState()==State_Initial);

	if(m_bFirstTime)
	{
		if(!m_bHasSpecifiedTarget)
		{
			m_vShelterPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		}

		m_bFirstTime = false;
	}

	if(m_bHasSpecifiedTarget)
	{
		SetState(State_MovingToShelter);
	}
	else
	{
		if(m_hFloodFillPathHandle!=PATH_HANDLE_NULL)
		{
			CPathServer::CancelRequest(m_hFloodFillPathHandle);
		}

		SetState(State_WaitingToFindShelter);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::WaitingToFindShelter_OnEnter(CPed * pPed)
{
	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::WaitingToFindShelter_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	// If we're waiting for a floodfill result, then abort wait task when it becomes available
	bool bJustAborted = false;
	if(m_hFloodFillPathHandle != PATH_HANDLE_NULL)
	{
		EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hFloodFillPathHandle);

		if(ret == ERequest_Ready && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			bJustAborted = true;
		}
		else if(ret == ERequest_NotFound)
		{
			m_hFloodFillPathHandle = PATH_HANDLE_NULL;
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bJustAborted)
	{
		// If we've been waiting and have no path handle, then its likely we were unable to
		// even issue a request in the first place.. Just quit out.
		if(m_hFloodFillPathHandle==PATH_HANDLE_NULL)
		{
			return FSM_Quit;
		}
		// If the request is not ready yet then continue waiting
		EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hFloodFillPathHandle);
		if(ret == ERequest_Ready)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
			return FSM_Continue;
		}
		else if(ret == ERequest_NotFound)
		{
			m_hFloodFillPathHandle = 0;
			return FSM_Quit;
		}

		// Get the result
		const EPathServerErrorCode eErr = CPathServer::GetClosestShelteredPolySearchResultAndClear(m_hFloodFillPathHandle, m_vShelterPosition);
		m_hFloodFillPathHandle = PATH_HANDLE_NULL;

		// If no position could be found, then just quit
		if(eErr != PATH_NO_ERROR)
		{
			return FSM_Quit;
		}
		// Try to go to the shelter position
		SetState(State_MovingToShelter);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::MovingToShelter_OnEnter(CPed * pPed)
{
	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::MovingToShelter_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_ACHIEVE_HEADING));

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING, pPed) );
		}
		else
		{
			SetState(State_WaitingInShelter);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::WaitingInShelter_OnEnter(CPed * pPed)
{
	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToShelterAndWait::WaitingInShelter_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	// Abort as soon as ped brings out a brolly, instead of waiting the couple of seconds until this wait task finishes.
	if(CTaskWander::GetDoesPedHaveBrolly(pPed) && fwRandom::GetRandomNumberInRange(0,100) > 75)
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			return FSM_Quit;
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// If the input time was -1 then just wait until its stopped raining, or until
		// the ped gets their brolly out..
		if(!m_WaitTimer.IsSet())
		{
			// Is the ped carrying an umbrella?  Don't shelter if so..
			const bool bHasBrolly = CTaskWander::GetDoesPedHaveBrolly(pPed);

			Assert(m_iMaxWaitTime==-1);
			if(g_weather.IsRaining() && !bHasBrolly)
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
			}
			else
			{
				return FSM_Quit;
			}
		}
		// Otherwise continue waiting until the timer has expired
		else if(!m_WaitTimer.IsOutOfTime())
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else
		{
			return FSM_Quit;
		}
	}
	return FSM_Continue;
}

CTask *
CTaskMoveGoToShelterAndWait::CreateSubTask(const int iTaskType, const CPed * pPed)
{
	switch(iTaskType)
	{
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			// Its important in this task to call SetState() before SetNewTask(), since we examine the current state
			// to decide what parameters to use when creating the task.

			if(GetState()==State_WaitingToFindShelter)
			{
				Assertf(m_hFloodFillPathHandle==PATH_HANDLE_NULL, "Path handle already set");
				m_hFloodFillPathHandle = CPathServer::RequestClosestShelteredPolySearch(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), m_fMaxShelterSearchDist, (void*)pPed);
				
				return rage_new CTaskMoveStandStill(4000);
			}
			else if(GetState()==State_WaitingInShelter)
			{
				u32 iWaitTime;
				if(m_iMaxWaitTime!=-1)
				{
					if(!m_WaitTimer.IsSet())
						m_WaitTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iMaxWaitTime);
					iWaitTime = m_iMaxWaitTime;
				}
				else
				{
					m_WaitTimer.Unset();
					iWaitTime = fwRandom::GetRandomNumberInRange(8000, 16000);
				}
				return rage_new CTaskMoveStandStill(iWaitTime);
			}
			else
			{
				Assertf(0, "Invalid state to be doing a wait task.");
				return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}
		}

		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			Assert(GetState()==State_MovingToShelter);
			CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(
				m_fMoveBlendRatio,
				m_vShelterPosition,
				1.0f
			);
			pNavMeshTask->SetCompletionRadius(4.0f);
			return pNavMeshTask;
		}

		// Once we've got into shelter, turn to face the direction we arrived in.
		// Otherwise we often end up facing a wall which looks daft.
		// Set setting the opposite of the current heading should be sufficient.
		// NB: If this doesn't work, then we can look at the last segment of the navmesh route?
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			const float fHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() + PI);
			return rage_new CTaskMoveAchieveHeading(fHeading);
		}
		case CTaskTypes::TASK_FINISHED:
		{
			if(m_hFloodFillPathHandle!=PATH_HANDLE_NULL)
				CPathServer::CancelRequest(m_hFloodFillPathHandle);

			return NULL;
		}
	}

	Assert(false);
	return NULL;
}

bool
CTaskMoveGoToShelterAndWait::HasTarget() const
{
	return (GetState()==State_MovingToShelter || GetState()==State_WaitingInShelter);
}

void CTaskMoveGoToShelterAndWait::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(m_hFloodFillPathHandle!=PATH_HANDLE_NULL)
		CPathServer::CancelRequest(m_hFloodFillPathHandle);

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}




