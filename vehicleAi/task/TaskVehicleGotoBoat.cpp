//
// boat cruise
// April 17, 2013
//

#include "math/vecmath.h"

#include "VehicleAi/task/TaskVehicleGoToBoat.h"
#include "VehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/Task/TaskVehiclePark.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Task/System/TaskHelpers.h"
#include "Peds/ped.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()


CTaskVehicleGoToBoat::Tunables CTaskVehicleGoToBoat::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleGoToBoat, 0xba1b001f);

// Constructor/destructor
CTaskVehicleGoToBoat::CTaskVehicleGoToBoat(const sVehicleMissionParams& params, const fwFlags16 uConfigFlags)
	: CTaskVehicleMissionBase(params)
	, m_fTimeBlocked(0.0f)
	, m_fTimeBlockThreePointTurn(0.0f)
	, m_fTimeRanRouteSearch(0.0f)
	, m_uConfigFlags(uConfigFlags)
	, m_bWasBlockedForThreePointTurn(false)
{

	Assertf(m_Params.GetTargetEntity().GetEntity() == NULL || m_uConfigFlags.IsFlagSet(CF_NeverNavMesh), "Can't have a target entity and use navmesh pathing for boats" );

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_BOAT);
}
	 
CTaskVehicleGoToBoat::CTaskVehicleGoToBoat(const CTaskVehicleGoToBoat& in_rhs)
	: CTaskVehicleMissionBase(in_rhs.m_Params)
	, m_fTimeBlocked(in_rhs.m_fTimeBlocked)
	, m_fTimeBlockThreePointTurn(in_rhs.m_fTimeBlockThreePointTurn)
	, m_uConfigFlags(in_rhs.m_uConfigFlags)
	, m_uRunningFlags(in_rhs.m_uRunningFlags)
	, m_fTimeRanRouteSearch(in_rhs.m_fTimeRanRouteSearch)
	, m_bWasBlockedForThreePointTurn(in_rhs.m_bWasBlockedForThreePointTurn)

{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_BOAT);
}

CTaskVehicleGoToBoat::~CTaskVehicleGoToBoat()
{

}

void CTaskVehicleGoToBoat::SetTargetPosition(const Vector3 *pTargetPosition)
{
	CTaskVehicleMissionBase::SetTargetPosition(pTargetPosition);
	m_uRunningFlags.ClearFlag(RF_IsRunningRouteSearch);
	m_fTimeRanRouteSearch = 0.0f;
}


aiTask::FSM_Return CTaskVehicleGoToBoat::ProcessPreFSM()
{
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	float length = (vTargetPosition - vStartCoords).XYMag();
	GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper()->SetAvoidanceFeelerMaxLength(Max(length, 2.0f));

	if ( GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper()->IsBlocked() )
	{
		m_fTimeBlocked += GetTimeStep();
	}
	m_fTimeBlockThreePointTurn -= GetTimeStep();

	UpdateSearchRoute(*GetVehicle());
	UpdateFollowRoute(*GetVehicle());

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleGoToBoat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle* pVehicle = GetVehicle();

	FSM_Begin
		// FindRoute state
		FSM_State(State_Init)
				FSM_OnUpdate		
					return Init_OnUpdate(*pVehicle);

		FSM_State(State_GotoStraightLine)
			FSM_OnEnter
				GotoStraightLine_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return GotoStraightLine_OnUpdate(*pVehicle);

		FSM_State(State_GotoNavmesh)
			FSM_OnEnter
				GotoNavmesh_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return GotoNavmesh_OnUpdate(*pVehicle);
			FSM_OnExit
				GotoNavmesh_OnExit(*pVehicle);

		FSM_State(State_ThreePointTurn)
			FSM_OnEnter
				ThreePointTurn_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return ThreePointTurn_OnUpdate(*pVehicle);
			FSM_OnExit
				ThreePointTurn_OnExit(*pVehicle);

		FSM_State(State_PauseForRoadRoute)
			FSM_OnEnter
				PauseForRoadRoute_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return PauseForRoadRoute_OnUpdate(*pVehicle);

		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter();
			FSM_OnUpdate		
				return Stop_OnUpdate();

	FSM_End
}
static float s_MaxSpeedToThreePointTurn = 10.0f;
static float s_MaxTimeBlocked = 0.5f;
static float s_AngleToThreePointTurn = 0.5f;
static float s_AngleToStopThreePointTurn = 0.6f;

bool CTaskVehicleGoToBoat::ShouldUseThreePointTurn(CVehicle& in_Vehicle, const Vector3& in_Target  ) const
{
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vBoatFwd = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetForward());
	const Vector3 vBoatVel = in_Vehicle.GetVelocity();
	Vector3 vDirectionToTarget = in_Target - vStartCoords;
	vDirectionToTarget.z = 0.0f;
	vDirectionToTarget.NormalizeSafe();

	CPed* pDriver = in_Vehicle.GetDriver();
	bool bOnlyTurnForBlocked = false;
	if(pDriver)
	{
		//commender this flag to prevent pickup boats turning around the large yachts when picking up player
		bOnlyTurnForBlocked = pDriver->GetPedConfigFlag(ePedConfigFlags::CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry);
	}

	float fAngleToThreePointTurn = bOnlyTurnForBlocked ? -10.0f : (m_uConfigFlags.IsFlagSet(CF_PreferForward) ? 0.0f : s_AngleToThreePointTurn);

	if ( m_fTimeBlockThreePointTurn < 0
		&& ( m_fTimeBlocked > s_MaxTimeBlocked
		|| (vDirectionToTarget.Dot(vBoatFwd) <= fAngleToThreePointTurn 
			&& vBoatVel.XYMag() <= s_MaxSpeedToThreePointTurn 
			&& ( !CPathServer::IsNavMeshLoadedAtPosition(VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition()), kNavDomainRegular) || in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->IsAvoiding()) ) ) )
	{
		return true;
	}
	return false;
}

void CTaskVehicleGoToBoat::UpdateFollowRoute(CVehicle& in_Vehicle)
{
	m_uRunningFlags.ClearFlag(RF_HasUpdatedSegmentThisFrame);

	if ( m_RouteFollowHelper.Progress(in_Vehicle, sm_Tunables.m_RouteLookAheadDistance ) )
	{
		if ( m_RouteFollowHelper.IsLastSegment() && m_uRunningFlags.IsFlagSet(RF_RouteIsPartial) && m_uRunningFlags.IsFlagSet(RF_HasValidRoute) )
		{
			m_uRunningFlags.ClearFlag(RF_HasValidRoute);
			m_fTimeRanRouteSearch = 0.0f;
		}

		m_uRunningFlags.SetFlag(RF_HasUpdatedSegmentThisFrame);
	}
}

void CTaskVehicleGoToBoat::UpdateSearchRoute(CVehicle& in_Vehicle)
{
	m_uRunningFlags.ClearFlag(RF_HasUpdatedRouteThisFrame);
	if ( !m_uConfigFlags.IsFlagSet(CF_NeverRoute) )
	{
		if ( m_uRunningFlags.IsFlagSet(RF_IsRunningRouteSearch) )
		{
			if( m_RouteSearchHelper.Update() == CTaskHelperFSM::FSM_Quit )
			{
				m_uRunningFlags.ClearFlag(RF_IsRunningRouteSearch);
				m_uRunningFlags.ClearFlag(RF_HasValidRoute);
				m_uRunningFlags.ClearFlag(RF_RouteIsPartial);
				m_uRunningFlags.SetFlag(RF_HasUpdatedRouteThisFrame);

				if ( m_RouteSearchHelper.GetState() == CPathNodeRouteSearchHelper::State_ValidRoute )
				{
					if ( m_RouteSearchHelper.GetCurrentNumNodes() >= CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED )
					{
						m_uRunningFlags.SetFlag(RF_RouteIsPartial);
					}

					m_uRunningFlags.SetFlag(RF_HasValidRoute);
					Vector3 vTargetPosition;
					m_Params.GetDominantTargetPosition(vTargetPosition);
					m_RouteFollowHelper.ConstructFromNodeList(in_Vehicle, m_RouteSearchHelper.GetNodeList(), vTargetPosition);
				}
			}
		}
		else
		{

			Vector3 vTargetCoords;
			FindTargetCoors(&in_Vehicle, vTargetCoords);

			const Vector3 vStartPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, IsDrivingFlagSet(DF_DriveInReverse)));

			const float fSearchMinX = Min(vStartPos.x, vTargetCoords.x);
			const float fSearchMinY = Min(vStartPos.y, vTargetCoords.y);
			const float fSearchMaxX = Max(vStartPos.x, vTargetCoords.x);
			const float fSearchMaxY = Max(vStartPos.y, vTargetCoords.y);
			if (ThePaths.AreNodesLoadedForArea(fSearchMinX, fSearchMaxX, fSearchMinY, fSearchMaxY))
			{
				if ( !m_uRunningFlags.IsFlagSet(RF_HasValidRoute) || m_uConfigFlags.IsFlagSet(CF_UseWanderRoute) || m_uConfigFlags.IsFlagSet(CF_UseFleeRoute) )
				{
					m_fTimeRanRouteSearch -= GetTimeStep();
					if ( m_fTimeRanRouteSearch < 0 )
					{
						static float s_RouteSearchTime = 5.0f;
						m_fTimeRanRouteSearch = s_RouteSearchTime;

						u32 iSearchFlags = CPathNodeRouteSearchHelper::Flag_IncludeWaterNodes;

						if (IsDrivingFlagSet(DF_AvoidTargetCoors))
						{
							iSearchFlags |= CPathNodeRouteSearchHelper::Flag_FindRouteAwayFromTarget;
						}
						if (IsDrivingFlagSet(DF_DriveIntoOncomingTraffic))
						{
							iSearchFlags |= CPathNodeRouteSearchHelper::Flag_DriveIntoOncomingTraffic;
						}
						if (IsDrivingFlagSet(DF_UseSwitchedOffNodes))
						{
							iSearchFlags |= CPathNodeRouteSearchHelper::Flag_UseSwitchedOffNodes;
						}
					
						if (IsDrivingFlagSet(DF_UseShortCutLinks))
						{
							iSearchFlags |= CPathNodeRouteSearchHelper::Flag_UseShortcutLinks;
						}					
					
						if (m_uConfigFlags.IsFlagSet(CF_UseWanderRoute))
						{
							iSearchFlags |= CPathNodeRouteSearchHelper::Flag_WanderRoute;
						}
					
						if ( m_uConfigFlags.IsFlagSet(CF_UseFleeRoute) )
						{	
							static u32 s_Flags = CPathNodeRouteSearchHelper::Flag_FindWanderRouteTowardsTarget | CPathNodeRouteSearchHelper::Flag_WanderRoute | CPathNodeRouteSearchHelper::Flag_JoinRoadInDirection;
							iSearchFlags |= s_Flags;
						}
				

						m_RouteSearchHelper.StartSearch(vStartPos, vTargetCoords, VEC3V_TO_VECTOR3(in_Vehicle.GetVehicleForwardDirection()), in_Vehicle.GetAiVelocity(), iSearchFlags, NULL);

						m_uRunningFlags.ClearFlag(RF_HasValidRoute);
						m_uRunningFlags.SetFlag(RF_IsRunningRouteSearch);
					}
				}
			}
			else
			{
				static float s_Padding = 10.0f;
				ThePaths.AddNodesRequiredRegionThisFrame(CPathFind::CPathNodeRequiredArea::CONTEXT_GAME, Vector3(fSearchMinX - s_Padding, fSearchMinY - s_Padding, 0.0f), Vector3(fSearchMaxX + s_Padding, fSearchMaxY + s_Padding, 0.0f), "Goto Boat");
			}
		}
	}
}


aiTask::FSM_Return CTaskVehicleGoToBoat::Init_OnUpdate(CVehicle& in_Vehicle)
{
	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	m_RouteFollowHelper.ConstructFromStartEnd(in_Vehicle, vTargetPosition);

	m_RouteFollowHelper.ComputeTarget(vTargetPosition, in_Vehicle);
 	
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vBoatVel = in_Vehicle.GetVelocity();
	Vector3 vDirectionToTarget = vTargetPosition - vStartCoords;  
	vDirectionToTarget.z = 0.0f;
	vDirectionToTarget.NormalizeSafe();
	float dirToTargetOrientation = fwAngle::GetATanOfXY(vDirectionToTarget.x, vDirectionToTarget.y);
	dirToTargetOrientation = fwAngle::LimitRadianAngleSafe(dirToTargetOrientation);

	m_fTimeBlocked = 0.0f;
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetSteeringEnabled(m_uConfigFlags.IsFlagSet(CF_AvoidShore));
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetAvoidanceOrientation(dirToTargetOrientation);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetDesiredOrientation(dirToTargetOrientation);

	if ( ShouldUseThreePointTurn(in_Vehicle, vTargetPosition) )
	{
		SetState(State_ThreePointTurn);
	}
	else
	{		
		SetState(State_GotoStraightLine);
	}
		
	return FSM_Continue;
}


void CTaskVehicleGoToBoat::GotoStraightLine_OnEnter(CVehicle& in_Vehicle)
{
	m_fTimeBlocked = 0.0f;
	sVehicleMissionParams params = m_Params;
	params.m_fTargetArriveDist = sm_Tunables.m_RouteArrivalDistance;
	m_RouteFollowHelper.ComputeTarget(params, in_Vehicle);
	
	m_uRunningFlags.ClearFlag(RF_HasUpdatedSegmentThisFrame);
	m_uRunningFlags.ClearFlag(RF_HasUpdatedRouteThisFrame);

#if __ASSERT
	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	Assertf(vTargetPosition.Dist2(ORIGIN) > SMALL_FLOAT, "CTaskVehicleGoToBoat::GotoStraightLine_OnEnter");
#endif // __ASSERT

	SetNewTask( rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, true, false));
	CBoatAvoidanceHelper * pBoatAvoidHelper = in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper();
	pBoatAvoidHelper->SetMaxAvoidanceOrientationLeft(-CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
	pBoatAvoidHelper->SetMaxAvoidanceOrientationRight(CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
}

aiTask::FSM_Return CTaskVehicleGoToBoat::GotoStraightLine_OnUpdate(CVehicle& in_Vehicle)
{
	bool bEndState = true;

	if ( ( !m_uRunningFlags.IsFlagSet(RF_HasValidRoute) || m_RouteFollowHelper.IsLastSegment() )
		&& !CPathServer::IsNavMeshLoadedAtPosition(VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition()), kNavDomainRegular)
		&& !m_uConfigFlags.IsFlagSet(CF_NeverRoute)
		&& !m_uConfigFlags.IsFlagSet(CF_NeverPause))
	{
		SetState(State_PauseForRoadRoute);
		return FSM_Continue;
	}
	
	if ( !m_uRunningFlags.IsFlagSet(RF_HasUpdatedRouteThisFrame) 
		&& !m_uRunningFlags.IsFlagSet(RF_HasUpdatedSegmentThisFrame) )
	{
		if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE )
		{
			bEndState = false;
			Vector3 vSegmentTarget;
			m_Params.GetDominantTargetPosition(vSegmentTarget);
			CTaskVehicleGoToPointWithAvoidanceAutomobile* pTaskVehicleGoToPointWithAvoidanceAutomobile = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(GetSubTask());
			pTaskVehicleGoToPointWithAvoidanceAutomobile->SetCruiseSpeed(ComputeDesiredSpeed());
			pTaskVehicleGoToPointWithAvoidanceAutomobile->SetTargetPosition(&m_RouteFollowHelper.ComputeSegmentTarget(vSegmentTarget, in_Vehicle));

			//
			// limit boat avoidance when following a valid road route
			//
			if ( m_uRunningFlags.IsFlagSet(RF_HasValidRoute) )
			{
				if ( in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper() )
				{
					const float fSegmentWidth = m_RouteFollowHelper.ComputeSegmentWidth();

					if ( fSegmentWidth > 0.0f )
					{
						Vec2V vDirection = in_Vehicle.GetVehicleForwardDirectionForDriving().GetXY();
						m_RouteFollowHelper.ComputeSegmentDirection(vDirection);
						Vec2V vRight = Vec2V(vDirection.GetY(), Negate(vDirection.GetX()));

						const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
						Vec2V vBoatPos = VECTOR3_TO_VEC3V(vStartCoords).GetXY();
						Vec2V vCurrent = VECTOR3_TO_VEC3V(vSegmentTarget).GetXY();

						Vec2V vTargetRight = vCurrent + vRight * ScalarV(fSegmentWidth);
						Vec2V vTargetLeft = vCurrent - vRight * ScalarV(fSegmentWidth);	

						float fDistSqrFromSegment = geomDistances::Distance2LineToPoint(vSegmentTarget, Vector3(vDirection.GetXf(), vDirection.GetYf(), 0.0f), vStartCoords);

						//don't limit if we're really far away from our desired segment
						if(fDistSqrFromSegment < square(fSegmentWidth*3.0f))
						{
							//	grcDebugDraw::Sphere(Vector3(vTargetRight.GetXf(), vTargetRight.GetYf(), vStartCoords.z), 1.0f, Color_blue, true, -1);
							//	grcDebugDraw::Sphere(Vector3(vTargetLeft.GetXf(), vTargetLeft.GetYf(), vStartCoords.z), 1.0f, Color_blue, true, -1);

							Vec2V vDirToTarget = NormalizeSafe(vCurrent - vBoatPos, vDirection);
							Vec2V vDirToLeftTarget = NormalizeSafe(vTargetLeft - vBoatPos, vDirection);
							Vec2V vDirToRightTarget = NormalizeSafe(vTargetRight - vBoatPos, vDirection);

							ScalarV angleLeft = Angle(vDirToTarget, vDirToLeftTarget);
							ScalarV angleRight = Angle(vDirToTarget, vDirToRightTarget);

							ScalarV fAngleLeft = FPIsFinite(angleLeft.Getf()) ? (angleLeft * ScalarV(RtoD) * ScalarV(Sign(Dot(vRight, vDirToLeftTarget).Getf()))) : ScalarV(V_ZERO);
							ScalarV fAngleRight = FPIsFinite(angleRight.Getf()) ? (angleRight * ScalarV(RtoD) * ScalarV(Sign(Dot(vRight, vDirToRightTarget).Getf()))) : ScalarV(V_ZERO);

							in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeftThisFrame(fAngleLeft.Getf());
							in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRightThisFrame(fAngleRight.Getf());
						}
					}
				}
			}

			if ( in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper() )
			{
				if ( in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->IsAvoiding() )
				{
					Vector3 vTargetCoords;
					m_Params.GetDominantTargetPosition(vTargetCoords);
					const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));

					if ( CanUseNavmeshToReachPoint(vStartCoords, vTargetCoords) )
					{
						// don't go back if we already failed
						if (!m_uRunningFlags.IsFlagSet(RF_IsUnableToFindNavPath) )
						{
							SetState(State_GotoNavmesh);
							return FSM_Continue;
						}
					}
				}
				if ( ShouldUseThreePointTurn(in_Vehicle, *pTaskVehicleGoToPointWithAvoidanceAutomobile->GetTargetPosition() ) )
				{
					SetState(State_ThreePointTurn);
					return FSM_Continue;
				}
				else if ( m_fTimeBlocked > s_MaxTimeBlocked )
				{
					pTaskVehicleGoToPointWithAvoidanceAutomobile->SetCruiseSpeed(Min(ComputeDesiredSpeed(), 5.0f));
				}
			}
		}
	}

	bool bReachedDestination = HasReachedDestination(in_Vehicle);
	if ( bEndState || bReachedDestination )
	{
		if ( bReachedDestination && !m_uConfigFlags.IsFlagSet(CF_NeverStop) )
		{
			SetState(State_Stop);
			if ( !m_uConfigFlags.IsFlagSet(CF_StopAtEnd) )
			{
				m_bMissionAchieved = true;
			}
		}
		else
		{
			m_uRunningFlags.ClearFlag(RF_IsUnableToFindNavPath);
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}

	return FSM_Continue;
}

void CTaskVehicleGoToBoat::GotoNavmesh_OnEnter(CVehicle& in_Vehicle)
{
	m_fTimeBlocked = 0.0f;

	sVehicleMissionParams params = m_Params;
	params.m_fTargetArriveDist = sm_Tunables.m_RouteArrivalDistance;
	m_RouteFollowHelper.ComputeTarget(params, in_Vehicle);

	SetNewTask(rage_new CTaskVehicleGoToNavmesh(params));

	static float s_MaxAvoidanceAngle = 45.0f;
	CBoatAvoidanceHelper * pBoatAvoidHelper = in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper();
	pBoatAvoidHelper->SetMaxAvoidanceOrientationLeft(-s_MaxAvoidanceAngle);
	pBoatAvoidHelper->SetMaxAvoidanceOrientationRight(s_MaxAvoidanceAngle);

}
	
aiTask::FSM_Return CTaskVehicleGoToBoat::GotoNavmesh_OnUpdate(CVehicle& in_Vehicle)
{
	bool bEndState = true;

	if ( ( !m_uRunningFlags.IsFlagSet(RF_HasValidRoute) || m_RouteFollowHelper.IsLastSegment() )
		&& !CPathServer::IsNavMeshLoadedAtPosition(VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition()), kNavDomainRegular)
		&& !m_uConfigFlags.IsFlagSet(CF_NeverRoute) 
		&& !m_uConfigFlags.IsFlagSet(CF_NeverPause))
	{
		SetState(State_PauseForRoadRoute);
		return FSM_Continue;
	}


	if ( !m_uRunningFlags.IsFlagSet(RF_HasUpdatedRouteThisFrame) 
		&& !m_uRunningFlags.IsFlagSet(RF_HasUpdatedSegmentThisFrame) )
	{
		if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH)
		{
			CTaskVehicleGoToNavmesh* pNavmeshTask = static_cast<CTaskVehicleGoToNavmesh*>(GetSubTask());

			bEndState = false;
			Vector3 vTargetPos = pNavmeshTask->GetClosestPointFoundToTarget();
			pNavmeshTask->SetCruiseSpeed(ComputeDesiredSpeed());
			pNavmeshTask->SetTargetPosition(&vTargetPos);
			if ( m_RouteFollowHelper.IsLastSegment() )
			{
				// @TODO: How does this work with an entity target?
				m_RouteFollowHelper.OverrideTarget(vTargetPos);
				m_Params.SetTargetPosition(vTargetPos);
			}

			if ( pNavmeshTask->GetSubTask() && pNavmeshTask->GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE )
			{
				CTaskVehicleGoToPointWithAvoidanceAutomobile* pGotoTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pNavmeshTask->GetSubTask());
				if ( ShouldUseThreePointTurn(in_Vehicle, *pGotoTask->GetTargetPosition() ) )
				{
					if ( !m_RouteFollowHelper.IsLastSegment() || !m_uConfigFlags.IsFlagSet(CF_ForceBeached) ) 			
					{	
						SetState(State_ThreePointTurn);
					}
				}
			}

			if ( HasReachedDestination(in_Vehicle) && !m_uConfigFlags.IsFlagSet(CF_NeverStop))
			{
				SetState(State_Stop);
			}

		}
	
		if (bEndState)
		{
			m_uRunningFlags.SetFlag(RF_IsUnableToFindNavPath);
			if ( m_RouteFollowHelper.IsLastSegment() )
			{
				if (  m_Params.m_fTargetArriveDist > 0.0f )
				{
					m_Params.m_fTargetArriveDist += in_Vehicle.GetBoundRadius();
				}
			}
			SetState(State_GotoStraightLine);
		}
	}


	return FSM_Continue;
}

void CTaskVehicleGoToBoat::GotoNavmesh_OnExit(CVehicle& in_Vehicle)
{
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeft(-CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRight(CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
	//GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper()->SetSteeringEnabled(true);
	//GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper()->SetAvoidanceFeelerScale(1.0f);
}

static float s_BackupAngle = 80.0f;
static float s_BackupDistance = 25.0f;
static float s_BackupSpeed = 15.0f;


void CTaskVehicleGoToBoat::ThreePointTurn_OnEnter(CVehicle& in_Vehicle)
{
	m_bWasBlockedForThreePointTurn = m_fTimeBlocked > s_MaxTimeBlocked;
	m_fTimeBlocked = 0.0f;
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetReverse(true);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetSlowDownEnabled(false);
	//in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetSteeringEnabled(false);
	sVehicleMissionParams params = m_Params;

	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vBoatFwd = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetForward());
	const Vector3 vBoatRight = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetRight());
	Vector3 vBoatBack = -vBoatFwd;
	
	Vector3 vDestination;
	m_Params.GetDominantTargetPosition(vDestination);
	m_RouteFollowHelper.ComputeTarget(vDestination, in_Vehicle);
	Vector3 vDirectionToTarget = vDestination - vStartCoords;
	vDirectionToTarget.z = 0.0f;
	vDirectionToTarget.NormalizeSafe();

	float fBackupAngle = s_BackupAngle;
	if ( vBoatRight.Dot(vDirectionToTarget) > 0.0f )
	{
		fBackupAngle = -fBackupAngle;
	}

	vBoatBack.RotateZ(fBackupAngle * DtoR);
	float dirToTargetOrientation = fwAngle::GetATanOfXY(vBoatBack.x, vBoatBack.y);
	dirToTargetOrientation = fwAngle::LimitRadianAngleSafe(dirToTargetOrientation);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetAvoidanceOrientation(dirToTargetOrientation);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetDesiredOrientation(dirToTargetOrientation);

	params.m_fCruiseSpeed = s_BackupSpeed;
	params.SetTargetPosition(vStartCoords + vBoatBack * s_BackupDistance);
	params.m_fTargetArriveDist = 5.0f;
	params.m_iDrivingFlags.SetFlag(DF_DriveInReverse);
	SetNewTask( rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, true, false));
}
 
aiTask::FSM_Return CTaskVehicleGoToBoat::ThreePointTurn_OnUpdate(CVehicle& in_Vehicle)
{

	if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE )
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile* pTaskVehicleGoToPointWithAvoidanceAutomobile = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(GetSubTask());

		const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
		const Vector3 vBoatFwd = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetForward());
		const Vector3 vBoatRight = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetRight());
		Vector3 vBoatBack = -vBoatFwd;

		Vector3 vDestination;
		m_Params.GetDominantTargetPosition(vDestination);
		m_RouteFollowHelper.ComputeTarget(vDestination, in_Vehicle);
		Vector3 vDirectionToTarget = vDestination - vStartCoords;
		vDirectionToTarget.z = 0.0f;
		vDirectionToTarget.NormalizeSafe();

		float fBackupAngle = s_BackupAngle;
		if ( vBoatRight.Dot(vDirectionToTarget) > 0.0f )
		{
			fBackupAngle = -fBackupAngle;
		}

		vBoatBack.RotateZ(fBackupAngle * DtoR);

		Vector3 vTargetPosition = vStartCoords + vBoatBack * s_BackupDistance;
		pTaskVehicleGoToPointWithAvoidanceAutomobile->SetTargetPosition(&vTargetPosition);
		pTaskVehicleGoToPointWithAvoidanceAutomobile->SetCruiseSpeed(s_BackupSpeed);

		float fAngleToStopThreePointTurn = m_uConfigFlags.IsFlagSet(CF_PreferForward) ? 0.1f : s_AngleToStopThreePointTurn;

		bool bCanGoForward = vDirectionToTarget.Dot(vBoatFwd) > fAngleToStopThreePointTurn;
		if(m_bWasBlockedForThreePointTurn)
		{
			bCanGoForward = GetTimeInState() > 2.0f;
		}

		if ( bCanGoForward || m_fTimeBlocked > s_MaxTimeBlocked )
		{
			static float s_TimeToBlockThreePointTurn = 3.0f;
			m_fTimeBlockThreePointTurn = s_TimeToBlockThreePointTurn;
			SetState(State_GotoStraightLine);
		}

		if ( HasReachedDestination(in_Vehicle) && !m_uConfigFlags.IsFlagSet(CF_NeverStop))
		{
			SetState(State_Stop);
		}

	}
	else
	{
		SetState(State_GotoStraightLine);
	}
	
	return FSM_Continue;
}

void CTaskVehicleGoToBoat::ThreePointTurn_OnExit(CVehicle& in_Vehicle)
{
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetReverse(false);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetSlowDownEnabled(true);

	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	Vector3 vDirectionToTarget = vTargetPosition - vStartCoords;
	vDirectionToTarget.z = 0.0f;
	vDirectionToTarget.NormalizeSafe();
	float dirToTargetOrientation = fwAngle::GetATanOfXY(vDirectionToTarget.x, vDirectionToTarget.y);
	dirToTargetOrientation = fwAngle::LimitRadianAngleSafe(dirToTargetOrientation);

	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetAvoidanceOrientation(dirToTargetOrientation);
	in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetDesiredOrientation(dirToTargetOrientation);

	//in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->SetSteeringEnabled(true);
}

void CTaskVehicleGoToBoat::PauseForRoadRoute_OnEnter(CVehicle& UNUSED_PARAM(in_Vehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

aiTask::FSM_Return CTaskVehicleGoToBoat::PauseForRoadRoute_OnUpdate(CVehicle& in_Vehicle)
{
	if ( (m_uRunningFlags.IsFlagSet(RF_HasValidRoute) && !m_RouteFollowHelper.IsLastSegment())
		|| CPathServer::IsNavMeshLoadedAtPosition(VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition()), kNavDomainRegular))
	{
		SetState(State_GotoStraightLine);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}  



void CTaskVehicleGoToBoat::Stop_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleStop());
}

aiTask::FSM_Return CTaskVehicleGoToBoat::Stop_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		m_bMissionAchieved = true;
		return FSM_Quit;
	}
	return FSM_Continue;
}   

bool CTaskVehicleGoToBoat::HasReachedDestination(CVehicle& in_Vehicle) const
{
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	if ( m_Params.m_fTargetArriveDist > 0 )
	{
		Vector3 vTargetPosition;
		m_Params.GetDominantTargetPosition(vTargetPosition);
		if ( (vTargetPosition - vStartCoords).XYMag2() < square(m_Params.m_fTargetArriveDist) )
		{
			return true;
		}
	}

	return false;
}



float CTaskVehicleGoToBoat::ComputeDesiredSpeed() const
{
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	const Vector3 vDelta = vTargetPosition - vStartCoords;
	float scalar = Min(vDelta.XYMag() / sm_Tunables.m_SlowdownDistance, 1.0f);
	return m_Params.m_fCruiseSpeed * scalar;
}

bool CTaskVehicleGoToBoat::CanUseNavmeshToReachPoint(const Vector3& in_vStartCoords, const Vector3& in_vTargetCoords ) const
{
	if ( !m_uConfigFlags.IsFlagSet(CF_NeverNavMesh) )
	{
		if (CPathServer::IsNavMeshLoadedAtPosition(in_vStartCoords, kNavDomainRegular)
			&& CPathServer::IsNavMeshLoadedAtPosition(in_vTargetCoords, kNavDomainRegular))
		{
			return true;
		}
	}
		
	return false;
}




#if !__FINAL
const char*  CTaskVehicleGoToBoat::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Init&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Init",
		"State_GotoStraightLine",
		"State_GotoNavmesh",
		"State_ThreePointTurn",
		"State_PauseForRoadRoute",
		"State_Stop",
	};

	return aStateNames[iState];
}

void CTaskVehicleGoToBoat::Debug() const
{
#if DEBUG_DRAW

	Vector3 vTargetPosition;
	m_Params.GetDominantTargetPosition(vTargetPosition);
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
	grcDebugDraw::Sphere(vStartCoords + ZAXIS, 0.5f, Color_green, false);
	grcDebugDraw::Sphere(vTargetPosition + ZAXIS, Max(m_Params.m_fTargetArriveDist, 0.5f), Color_green, false);
	grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStartCoords + ZAXIS), VECTOR3_TO_VEC3V(vTargetPosition + ZAXIS), 1.0f, Color_OrangeRed);
	m_RouteFollowHelper.Debug(*GetVehicle());

#endif
}
#endif //!__FINAL


