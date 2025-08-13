#include "TaskVehicleGoToAutomobile.h"

#include "math/vecmath.h"
#include "math/angmath.h"
#include "phbound/BoundComposite.h"

// Framework headers
#include "fwdebug/vectormap.h"
#include "fwmaths/geomutil.h"

// Game headers
#include "Vehicles/Planes.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vehicles/virtualroad.h"
#include "Vehicles/Trailer.h"
#include "debug/DebugScene.h"
#include "debug/DebugRecorder.h"
#include "game/ModelIndices.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Peds/ped.h"
#include "event/eventmovement.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Objects/Door.h"
#include "Objects/DoorTuning.h"
#include "scene/world/gameWorld.h"
#include "physics/PhysicsHelpers.h"
#include "physics/Physics.h"
#include "Peds/PedIntelligence.h"
#include "Vehicles/metadata/AIHandlingInfo.h"
#include "VehicleAi/driverpersonality.h"
#include "VehicleAi/task/TaskVehicleThreePointTurn.h"
#include "VehicleAi/task/TaskVehicleGoTo.h"
#include "VehicleAi/task/TaskVehicleCruise.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "VehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/task/TaskVehicleEscort.h"
#include "vehicleAi/task/DeferredTasks.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "control/trafficLights.h"
#include "task/Movement/TaskMoveWander.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "peds/Ped.h"
#include "peds/PedDebugVisualiser.h"
#include "phbound/BoundBox.h"
#include "renderer/HierarchyIds.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

// TODO: Clean up the code based on the current values of these #defines, at some point.
#define USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE 1
#define USE_FOLLOWROUTE_EDGES_FOR_AVOIDANCE 1

Vector2		CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_vAvoidXExtents(1.5f*PED_HUMAN_RADIUS, 0.0f);
Vector2		CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_vNmXExtents(0.4f*PED_HUMAN_RADIUS, 0.75f);
Vector2		CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_vNmVelocityScale(0.25f, 0.5f);

//dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fChangeLanesVelocityRatio = 1.25f;
//u32		CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_TimeBeforeOvertake = 0;
//u32		CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_TimeBeforeUndertake = 1000;
dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fAVOIDANCE_LOOKAHEAD_TIME = 1.7f;
dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fAVOIDANCE_LOOKAHEAD_TIME_FOR_FAST_VEHICLES = 3.0f;
dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fExtraWidthForBikes = 0.75f;
dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fExtraWidthForCars = 0.75f;
dev_float	CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_fMinLengthForDoors = 0.33f;
int			CTaskVehicleGoToPointWithAvoidanceAutomobile::ThreePointTurnInfo::ms_iMaxAttempts = 3;
#if __BANK
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bEnableNewAvoidanceDebugDraw = false;
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bDisplayObstructionProbeScores = false;
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bDisplayAvoidanceRoadSegments = false;
#endif //__BANK

PF_PAGE(GTA_LaneChange, "GTA LaneChange");
PF_GROUP(GTA_LANES);
PF_LINK(GTA_LaneChange, GTA_LANES);
PF_TIMER(AI_LaneChange, GTA_LANES);

PF_PAGE(GTA_VehicleAvoidance, "GTA Vehicle Avoidance");
PF_GROUP(GTA_VEHICLE_AVOIDANCE);
PF_LINK(GTA_VehicleAvoidance, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_Avoidance, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceShouldWaitForTrafficBeforeTurningLeft, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceTailgateCarInFront, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceFindAngleToWeaveThroughTraffic, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceFindAngleToAvoidBuildings, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceFindMaximumSpeedForThisCarInTraffic, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceSlowCarForOtherCar, GTA_VEHICLE_AVOIDANCE);
PF_TIMER(AI_AvoidanceFindMaximumSpeedForRacingCar, GTA_VEHICLE_AVOIDANCE);


PF_TIMER(AI_AvoidanceNewSearchForUnobstructedDirections, GTA_VEHICLE_AVOIDANCE);

static const ScalarV s_vScalarZero(V_ZERO);

static const int s_iNumObstructionProbes = 30;
static const int kMaxObstructionProbeDirections = ((s_iNumObstructionProbes + 3) & ~3);

/////////////////////////////////////////////////////////////////////////////////
// CTaskVehicleGoToPointWithAvoidanceAutomobile
/////////////////////////////////////////////////////////////////////////////////

CTaskVehicleGoToPointWithAvoidanceAutomobile::Tunables CTaskVehicleGoToPointWithAvoidanceAutomobile::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleGoToPointWithAvoidanceAutomobile, 0x6f61cca2);

/////////////////////////////////////////////////////////////////

CTaskVehicleGoToPointWithAvoidanceAutomobile::CTaskVehicleGoToPointWithAvoidanceAutomobile(
	const sVehicleMissionParams& params,
	const bool bAllowThreePointTurns,
	const bool bAllowAchieveMission	) :
CTaskVehicleGoTo(params)
{
	m_fSmallestCollisionTSoFar = 99999.9f;
	m_startTimeAvoidCarsUntilClear = 0;
	m_SteeringTargetPos = params.GetTargetPosition();//copy the target pos.

	m_pOptimization_pLastCarBlockingUs = NULL;
	m_pOptimization_pLastCarWeSlowedFor = NULL;
	m_pOptimization_pVehicleTooCloseToTailgate = NULL;

	m_bStoppingForTraffic = false;
	m_bSlowingDownForCar = false;
	m_bSlowingDownForPed = false;
	m_bReEnableSlowForCarsWhenDoneAvoiding = false;
	//m_bShouldChangeLanesForTraffic = false;
	//m_bWaitingToOvertakeThisFrame = false;
	//m_bWaitingToUndertakeThisFrame = false;
	m_bAvoidedLeftLastFrame = false;
	m_bAvoidedRightLastFrame = false;
	m_bAllowThreePointTurns = bAllowThreePointTurns;
	m_fAvoidedEntityCounter = 0.0f;
	m_fMostImminentCollisionThisFrame = FLT_MAX;
	m_fAvoidanceMarginForOtherLawEnforcementVehicles = -1.0f;
	m_bWaitingForPlayerPed = false;
	m_startTimeWaitingForPlayerPed = 0;
	m_lastTimeHonkedAtAnyPed = 0;
	m_bSlowlyPushingPlayerThisFrame = false;
	m_vCachedGiveWarningPosition.Zero();
	m_bAllowAchieveMission = bAllowAchieveMission;
	m_bAvoidingCarsUntilClear = false;
	m_bPathContainsUTurn = false;
	m_bOvertakeCurrentCarRequested = false;
	m_bReverseBeforeOvertakeCurrentCarRequested = false;

	//m_NextTimeAllowedToChangeLanesForTraffic = 0;
	//m_TimeWaitingForOvertake = 0;
	//m_TimeWaitingForUndertake = 0;

	m_iTargetSideFlags = 0;

	SetDrivingFlag(DF_TargetPositionOverridesEntity, true);
	//Assertf(m_Params.m_vTargetPos.Dist2(ORIGIN) > SMALL_FLOAT, "Low-level drive task being told to drive to the origin. You probably don't want this.");

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE);
}

/////////////////////////////////////////////////////////////////

CTaskVehicleGoToPointWithAvoidanceAutomobile::~CTaskVehicleGoToPointWithAvoidanceAutomobile()
{
	if (GetVehicle())
	{
		GetVehicle()->m_nVehicleFlags.bIsWaitingToTurnLeft = false;
		GetVehicle()->m_nVehicleFlags.bWillTurnLeftAgainstOncomingTraffic = false;
	}	
}


/////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToPointWithAvoidanceAutomobile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	pVehicle->m_nVehicleFlags.bIsWaitingToTurnLeft = false;
	pVehicle->m_nVehicleFlags.bWillTurnLeftAgainstOncomingTraffic = false;
	m_bBumperInsideBoundsThisFrame = false;

	//if (pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle)
// 	if (m_bWaitingForPlayerPed)
// 	{
// 		Vec3V vAboveVehPos = pVehicle->GetVehiclePosition();
// 		vAboveVehPos.SetZf(vAboveVehPos.GetZf() + 10.0f);
// 		CVehicle::ms_debugDraw.AddLine(pVehicle->GetVehiclePosition(), vAboveVehPos, Color_green);
// 	}
	///////////////////////

	FSM_Begin
		// Initial state
		FSM_State(State_GoToPoint)
			FSM_OnEnter
				GoToPointWithAvoidance_OnEnter(pVehicle);
			FSM_OnUpdate
				return GoToPointWithAvoidance_OnUpdate(pVehicle);
		//
		FSM_State(State_ThreePointTurn)
			FSM_OnEnter
				ThreePointTurn_OnEnter(pVehicle);
			FSM_OnUpdate
				return ThreePointTurn_OnUpdate(pVehicle);
			FSM_OnExit
				return ThreePointTurn_OnExit(pVehicle);
		//
		FSM_State(State_WaitForTraffic)
			FSM_OnEnter
				WaitForTraffic_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);

		//
		FSM_State(State_WaitForPed)
			FSM_OnEnter
				WaitForPed_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);

		//
		FSM_State(State_TemporarilyBrake)
			FSM_OnEnter
				TemporarilyBrake_OnEnter(pVehicle);
			FSM_OnUpdate
				return TemporarilyBrake_OnUpdate(pVehicle);

		FSM_State(State_Swerve)
			FSM_OnEnter
				SwerveForHeadOnCollision_OnEnter(pVehicle);
			FSM_OnUpdate
				return SwerveForHeadOnCollision_OnUpdate(pVehicle);
	FSM_End
}


bool CTaskVehicleGoToPointWithAvoidanceAutomobile::CheckIsAboutToMakeSharpTurn(const CVehicle& veh, Vec3V_In tgtPos) const
{
	const CVehicle* pVehicle = &veh;

	// Note: when this check was done in CTaskVehicleCruiseNew, we used the driving flag from that task,
	// while now we use the driving flags from CTaskVehicleGoToPointWithAvoidanceAutomobile. Need to
	// confirm that we deal with DF_DriveInReverse properly.

	const Vec3V svZero(V_ZERO);

	const Vec3V vDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse));
	const Vec3V vSteeringPos = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)); 
	const ScalarV maxSteeringAngleV(pVehicle->GetIntelligence()->FindMaxSteerAngle());

	// Compute the two vectors, flat.
	const Vec2V vec1FlatV = vDriveDir.GetXY();
	const Vec2V vec2FlatV = Subtract(tgtPos.GetXY(), vSteeringPos.GetXY());

	// Compute the dot product.
	const ScalarV dotNonNormV = Dot(vec1FlatV, vec2FlatV);

	// Compute the inverse of the product of the two magnitudes.
	const ScalarV invMagsSqV = InvSqrtFast(Scale(MagSquared(vec1FlatV), MagSquared(vec2FlatV)));

	// Compute what the dot product between the normalized vectors would have been.
	const ScalarV dotV = Scale(dotNonNormV, invMagsSqV);

	if(IsLessThanAll(dotV, maxSteeringAngleV))
	{
		return true;
	}
	return false;
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::ProcessSuperDummy(CVehicle* pVehicle, float fTimeStep) const
{
	if(pVehicle && pVehicle->m_nVehicleFlags.bTasksAllowDummying && pVehicle->GetVehicleAiLod().GetDummyMode() == VDM_SUPERDUMMY )
	{
		CVehicleFollowRouteHelper* pFollowRoute = pVehicle->GetIntelligence()->GetFollowRouteHelperMutable();
		if(pFollowRoute)
		{
			pFollowRoute->ProcessSuperDummy( pVehicle, ScalarV( GetCruiseSpeed() ), GetDrivingFlags(), fTimeStep );
		}
	}
}

float CTaskVehicleGoToPointWithAvoidanceAutomobile::CalculateOtherVehicleMarginForAvoidance(const CVehicle& rVehicle, const CVehicle& rOtherVehicle, bool bGiveWarning) const
{
	//Check if the other vehicle is a law enforcement vehicle.
	float fMargin = 0.0f;
	if(rOtherVehicle.IsLawEnforcementVehicle())
	{
		//Check if we don't have an override.
		//Give cops some space, by default.
		if(m_fAvoidanceMarginForOtherLawEnforcementVehicles < 0.0f)
		{
			fMargin = 0.5f;
		}
		else
		{
			fMargin = m_fAvoidanceMarginForOtherLawEnforcementVehicles;
		}
	}
	//if we have a trailer or are a bike, or the other vehicle is a cop, give the other guy some space
	else if ((bGiveWarning && rOtherVehicle.GetDriver() && rOtherVehicle.GetDriver()->IsPlayer()) 
		|| rVehicle.InheritsFromBike() || rVehicle.InheritsFromBoat())
	{
		fMargin = 0.5f;
	}
	else if (rVehicle.GetAttachedTrailer())
	{
		fMargin = 0.25f;
	}
	else if (rOtherVehicle.m_nVehicleFlags.bIsThisAParkedCar)
	{
		fMargin = 0.5f;
	}

	//if the other vehicle is on fire, REALLY avoid it
	if (rOtherVehicle.IsOnFire())
	{
		fMargin = rage::Max(0.75f, fMargin);
	}

	//add any additional buffer we've been given for this target
	const CPhysical* pOtherAvoidanceTarget = rVehicle.GetIntelligence()->GetAvoidanceCache().m_pTarget;
	if(&rOtherVehicle == pOtherAvoidanceTarget)
	{
		fMargin += rVehicle.GetIntelligence()->GetAvoidanceCache().m_fAdditionalBuffer;
	}
	
	return fMargin;
}

/////////////////////////////////////////////////////////////////////////////////
// State_GoTo
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::GoToPointWithAvoidance_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	m_iTargetSideFlags = 0;
	m_vCachedGiveWarningPosition.Zero();
	m_bStoppingForTraffic = false;
	//SetNewTask( rage_new CTaskVehicleGoToPointAutomobile( (DrivingModeType) m_iDrivingStyle, GetCruiseSpeed(), &m_vTargetPos, m_missionFlags ));
	SetNewTask( rage_new CTaskVehicleGoToPointAutomobile( m_Params));

	//Assertf(m_Params.m_TargetEntity.GetEntity() == NULL, "Entity Target being passed down to a low-level drive task that requires a position.");
	Assertf(m_Params.GetTargetPosition().Dist2(ORIGIN) > SMALL_FLOAT, "Low-level drive task being told to drive to the origin. You probably don't want this.");

	//SetNewTask( rage_new CTaskVehicleGoToPointAutomobile( (DrivingModeType) m_iDrivingStyle, GetCruiseSpeed(), &m_vTargetPos, m_missionFlags.GetAllFlags() ));
}

/////////////////////////////////////////////////////////////////////////////////

#if __STATS
EXT_PF_TIMER(VehicleAvoidanceTask_Run);
#endif // __STATS

aiTask::FSM_Return CTaskVehicleGoToPointWithAvoidanceAutomobile::GoToPointWithAvoidance_OnUpdate(CVehicle* pVehicle)
{
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	const Vector3 vVehicleSteeringPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse), true));
	const float fDistToTargetSq = rage::Min((m_Params.GetTargetPosition() - vVehicleSteeringPos).XYMag2(), (m_Params.GetTargetPosition() - vVehiclePos).XYMag2());

	//are we done?
	if(m_Params.m_fTargetArriveDist > 0.0f)//only check if the arrive distance is greater then 0
	{
		if(m_Params.GetTargetPosition().z < vVehicleSteeringPos.z+CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE && m_Params.GetTargetPosition().z > vVehicleSteeringPos.z-CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE)//make sure it's vaguely close in the Z
		{	
			//check whether we have achieved our destination 
			if (fDistToTargetSq < m_Params.m_fTargetArriveDist * m_Params.m_fTargetArriveDist)
			{
				if ( m_bAllowAchieveMission )
				{
					m_bMissionAchieved = true;
				}

				pVehicle->GetIntelligence()->InvalidateCachedNodeList();
				pVehicle->GetIntelligence()->InvalidateCachedFollowRoute();
				return FSM_Quit;
			}
		}
	}

	if( m_threePointTurnInfo.m_iNumRecentThreePointTurns > 0 )
	{
		if(fwTimer::GetTimeInMilliseconds() - m_threePointTurnInfo.m_fLastThreePointTurnTime > 7000 || 
			(vVehiclePos - m_threePointTurnInfo.m_vLastTurnPosition).XYMag2() > 100.0f)
		{
			//reset three point turns if we haven't done one recently, or have moved away from point
			m_threePointTurnInfo.m_iNumRecentThreePointTurns = 0;
		}
	}

	aiDeferredTasks::TaskData taskData(this, pVehicle, GetTimeStep(), true);

#if !__PS3
	TUNE_GROUP_BOOL(AI_DEFERRED_TASKS, USE_DEFERRED_AVOIDANCE, true);

	if(USE_DEFERRED_AVOIDANCE)
	{
		// Queue avoidance
		aiDeferredTasks::g_VehicleAvoidance.Queue(taskData);
	}
	else
	{
#endif
		PF_START(VehicleAvoidanceTask_Run);
		GoToPointWithAvoidance_OnDeferredTask(taskData);
		GoToPointWithAvoidance_OnDeferredTask_ProcessSuperDummy(taskData);
		GoToPointWithAvoidance_OnDeferredTaskCompletion(taskData);
		PF_STOP(VehicleAvoidanceTask_Run);
#if !__PS3
	}
#endif

	return FSM_Continue;
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::GoToPointWithAvoidance_OnDeferredTask(const aiDeferredTasks::TaskData& data)
{
	Assert(data.m_Task == this);

	ASSERT_ONLY(SetCanChangeState(true);)
	
	CVehicle* pVehicle = data.m_Vehicle;
	const float& fTimeStep = data.m_TimeStep;

	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	const Vector3 vVehicleSteeringPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse), true));
	float	dirToTargetOrientation;

	pVehicle->GetIntelligence()->CacheNodeList();
	pVehicle->GetIntelligence()->CacheFollowRoute();

	const bool bWasSlowlyPushingPlayer = pVehicle->GetIntelligence()->HasBeenSlowlyPushingPlayer();

	m_bSlowlyPushingPlayerThisFrame = false;
	m_bPathContainsUTurn = false;

	TUNE_GROUP_BOOL		(FOLLOW_PATH_AI_STEER,			handle3PointStuff, true);

	const float fDistToTargetSq = rage::Min((m_Params.GetTargetPosition() - vVehicleSteeringPos).XYMag2(), (m_Params.GetTargetPosition() - vVehiclePos).XYMag2());

	// the steering pos target should always start out the same as the desired target 
	m_SteeringTargetPos = m_Params.GetTargetPosition();

	// Get the current drive direction and orientation.
	const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
	vehDriveOrientation = fwAngle::LimitRadianAngleSafe(vehDriveOrientation);

	// Work out the target orientation.
	// To make things a little bit more stable we aim ahead a little bit.
	//	TargetPoint_Ahead = TargetPoint + pTargetCar->GetB() * 3.0f;
	Vector3 vTargetPos = m_Params.GetTargetPosition();
	dirToTargetOrientation = fwAngle::GetATanOfXY(vTargetPos.x - vVehicleSteeringPos.x, vTargetPos.y - vVehicleSteeringPos.y);
	dirToTargetOrientation = fwAngle::LimitRadianAngleSafe(dirToTargetOrientation);

	// If there is a big difference in target and car orientation we will consider doing a 3 point turn.
	if(handle3PointStuff && GetCruiseSpeed() > 0.0f)
	{
		float fDirToTargetOrientationFromOriginal = fwAngle::GetATanOfXY(vTargetPos.x - vVehiclePos.x, vTargetPos.y - vVehiclePos.y);
		fDirToTargetOrientationFromOriginal = fwAngle::LimitRadianAngleSafe(fDirToTargetOrientationFromOriginal);
		if(ShouldDoThreePointTurn(pVehicle, &fDirToTargetOrientationFromOriginal, &vehDriveOrientation))
		{
			pVehicle->GetIntelligence()->InvalidateCachedNodeList();
			pVehicle->GetIntelligence()->InvalidateCachedFollowRoute();

			SetState(State_ThreePointTurn);
			return;
		}
	}

	if (m_bOvertakeCurrentCarRequested)
	{
		m_bOvertakeCurrentCarRequested = false;
		if (StartOvertakeCurrentCar())
		{
			return;
		}
	}

	float cruiseSpeed = GetCruiseSpeed();

	bool bGiveWarning = false;
	if (aiVerify(FPIsFinite(dirToTargetOrientation)) && aiVerify(FPIsFinite(vehDriveOrientation)))
	{
		DealWithTrafficAndAvoidance( pVehicle, cruiseSpeed, dirToTargetOrientation, vehDriveOrientation, bGiveWarning, bWasSlowlyPushingPlayer, fTimeStep);
	}

	// boats should avoid shorelines
	DealWithShoreAvoidance(pVehicle, cruiseSpeed, dirToTargetOrientation, vehDriveOrientation);

	//if we didn't change states, reset the waiting for player ped flag
  	if (GetState() == State_GoToPoint && !m_bSlowlyPushingPlayerThisFrame)
  	{
  		SetWaitingForPlayerPed(false);
		m_startTimeWaitingForPlayerPed = 0;
  	}

	if (bGiveWarning)
	{
		bool bSwerve = true;
		if (pVehicle->GetIntelligence()->m_pSteeringAroundEntity && pVehicle->GetIntelligence()->m_pSteeringAroundEntity->GetIsTypeVehicle())
		{
			const CVehicle* pOtherVehicle = static_cast<const CVehicle*>(pVehicle->GetIntelligence()->m_pSteeringAroundEntity.Get());
			bSwerve = !pOtherVehicle->GetIntelligence()->m_bDontSwerveForUs;
		}

		if (bSwerve)
		{
			pVehicle->GetIntelligence()->InvalidateCachedNodeList();
			pVehicle->GetIntelligence()->InvalidateCachedFollowRoute();

			SetState(State_Swerve);

			return;
		}
	}

/*
	//very rubbish braking for a corner code.
	//adjust the speed depending on how big the difference between veh orientation and target orientation is
	float desiredAngleDiff = dirToTargetOrientation - vehDriveOrientation;
	desiredAngleDiff = fwAngle::LimitRadianAngle(desiredAngleDiff);
	
	float speedModifier = (rage::Abs(desiredAngleDiff)/PI)*5.0f;
	speedModifier = 1.0f -((cruiseSpeed/30.0f) * speedModifier);
	speedModifier =  rage::Min(1.0f, speedModifier);//clamp the modifier so we don't go above the desired cruise speed.
	speedModifier =  rage::Max(0.2f, speedModifier);//make sure we are always moving a little bit
	//modify cruise speed by the amount we are trying to turn.
	cruiseSpeed = cruiseSpeed * speedModifier;
*/

	// If we've circled the target, then it might be inside our turning circle - it will be necessary to slow down
	if(vVehiclePos.x < vTargetPos.x)
		m_iTargetSideFlags |= FLAG_NEG_X;
	else if(vVehiclePos.x > vTargetPos.x)
		m_iTargetSideFlags |= FLAG_POS_X;
	if(vVehiclePos.y < vTargetPos.y)
		m_iTargetSideFlags |= FLAG_NEG_Y;
	else if(vVehiclePos.y > vTargetPos.y)
		m_iTargetSideFlags |= FLAG_POS_Y;

	// Been all sides?  Then apply handbrake to slow down.
	const float desiredAngleDiff = fwAngle::LimitRadianAngleSafe(dirToTargetOrientation - vehDriveOrientation);
	if(((m_iTargetSideFlags ^ FLAG_ALL_SIDES) == 0) && Abs(desiredAngleDiff)>0.05f && fDistToTargetSq > 1.0f)
	{
		// TODO: adjust cruiseSpeed accordingly.
		// commented out for the moment, until this can be tested further..
		//cruiseSpeed = 1.0f;

#if __BANK
		if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
		{
			grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vVehiclePos+Vector3(0.0f,2.0f,0.0f)), 1.0f, Color_red);
			grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vTargetPos+Vector3(0.0f,2.0f,0.0f)), 1.0f, Color_green);
			grcDebugDraw::Line(RCC_VEC3V(vVehiclePos), RCC_VEC3V(vTargetPos), Color_red, Color_green);
		}
#endif
	}

	pVehicle->GetIntelligence()->m_fDesiredSpeedThisFrame = cruiseSpeed;
	SetCruiseSpeed(cruiseSpeed);

	//now turn the target orientation into a target vector
	Vector3 LineBase = vVehiclePos;
	Vector3 Offset1 = 5.0f * rage::Cosf(vehDriveOrientation - dirToTargetOrientation) * (IsDrivingFlagSet(DF_DriveInReverse) ? -1.0f : 1.0f) * VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
	Vector3 Offset2 = 5.0f * rage::Sinf(vehDriveOrientation - dirToTargetOrientation) * (IsDrivingFlagSet(DF_DriveInReverse) ? -1.0f : 1.0f) * VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());

	m_SteeringTargetPos = (LineBase + Offset1 + Offset2);

	// set the target for the goto point task
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE)
	{
		CTaskVehicleGoToPointAutomobile * pGoToTask = (CTaskVehicleGoToPointAutomobile*)GetSubTask();

		pGoToTask->SetTargetPosition(&m_SteeringTargetPos);
		pGoToTask->SetCruiseSpeed(GetCruiseSpeed());
	}

	// Check if we have computed a target position that looks like we are about to
	// make a sharp turn, and make sure that we don't use superdummy mode in the near
	// future.
	Vec3V tgtPosV;
	FindTargetCoors(pVehicle, RC_VECTOR3(tgtPosV));
	if(CheckIsAboutToMakeSharpTurn(*pVehicle, tgtPosV))
	{
		pVehicle->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = true;
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::GoToPointWithAvoidance_OnDeferredTask_ProcessSuperDummy(const aiDeferredTasks::TaskData& data)
{
	CVehicle* pVehicle = data.m_Vehicle;

	ProcessSuperDummy(pVehicle, data.m_TimeStep);

	pVehicle->GetIntelligence()->InvalidateCachedNodeList();
	pVehicle->GetIntelligence()->InvalidateCachedFollowRoute();

	ASSERT_ONLY(SetCanChangeState(false);)
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::GoToPointWithAvoidance_OnDeferredTaskCompletion(const aiDeferredTasks::TaskData& data)
{
	Assert(data.m_Task == this);

	CVehicle* pVeh = data.m_Vehicle;

	if (pVeh->GetIntelligence()->m_fDesiredSpeedThisFrame >= 1.0f 
		&& (pVeh->m_nVehicleFlags.bActAsIfHasSirenOn || (pVeh->m_nVehicleFlags.GetIsSirenOn() && pVeh->UsesSiren() 
		&& pVeh->GetModelIndex() != MI_CAR_TOWTRUCK && pVeh->GetModelIndex() != MI_CAR_TOWTRUCK_2 && pVeh->GetModelIndex() != MI_CAR_PBUS2))
		&& !pVeh->m_nVehicleFlags.bTellOthersToHurry
		&& pVeh->GetIntelligence()->GetSirenReactionDistributer().IsRegistered() 
		&& pVeh->GetIntelligence()->GetSirenReactionDistributer().ShouldBeProcessedThisFrame())
	{
		//static fn call
		MakeWayForCarWithSiren(pVeh);	
	}

	//also do this for vehs with bToldToGetOutOfTheWay, so we propgate it fwd
	if (pVeh->m_nVehicleFlags.bTellOthersToHurry || pVeh->m_nVehicleFlags.bToldToGetOutOfTheWay)
	{
		MakeWayForCarTellingOthersToFlee(pVeh);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// State_ThreePointTurn
/////////////////////////////////////////////////////////////////////////////////
void		CTaskVehicleGoToPointWithAvoidanceAutomobile::ThreePointTurn_OnEnter		(CVehicle* pVehicle)
{
	//start the three point turn vehicle sub task
	sVehicleMissionParams params = m_Params;
	params.m_fCruiseSpeed = 8.0f;
	params.ClearTargetEntity();
	m_threePointTurnInfo.m_vLastTurnPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	SetNewTask( rage_new CTaskVehicleThreePointTurn( params, m_bPathContainsUTurn ) );
}

//////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGoToPointWithAvoidanceAutomobile::ThreePointTurn_OnUpdate		(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN))
	{
		SetState(State_GoToPoint);		//finished three point turn, go back to driving to destination.
	}

	//if task thinks we should be stopped then return to goto point state
	if(GetCruiseSpeed() == 0.0f)
	{
		SetState(State_GoToPoint);
	}

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN)
	{
		CTaskVehicleThreePointTurn* pSubtask = static_cast<CTaskVehicleThreePointTurn*>(GetSubTask());
		pSubtask->SetTargetPosition(&m_Params.GetTargetPosition());
	}

	ProcessSuperDummy(pVehicle, GetTimeStep());

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGoToPointWithAvoidanceAutomobile::ThreePointTurn_OnExit		(CVehicle* UNUSED_PARAM(pVehicle))
{
	++m_threePointTurnInfo.m_iNumRecentThreePointTurns;
	m_threePointTurnInfo.m_fLastThreePointTurnTime = fwTimer::GetTimeInMilliseconds();

	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////
// State_WaitForTraffic
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::WaitForTraffic_OnEnter				(CVehicle* pVehicle)
{
	//don't wait if a junction's telling us to go somewhere
	u32	Wait = pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO ? 0 : CDriverPersonality::FindDelayBeforeAcceleratingAfterObstructionGone(pVehicle->GetDriver(), pVehicle);

	SetNewTask( rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + Wait) );
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleGoToPointWithAvoidanceAutomobile::Wait_OnUpdate				(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_WAIT))
	{
		SetState(State_GoToPoint);		//finished waiting so now head for target again.
	}

	ProcessSuperDummy(pVehicle, GetTimeStep());

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	return FSM_Continue;
}



///////////////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPointWithAvoidanceAutomobile::WaitForPed_OnEnter				(CVehicle* pVehicle)
{
	u32	Wait = pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO && !m_bWaitingForPlayerPed ? 0 
		: CDriverPersonality::FindDelayBeforeAcceleratingAfterObstructionGone(pVehicle->GetDriver(), pVehicle, m_bWaitingForPlayerPed);
	SetNewTask( rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + Wait) );
}


//////////////////////////////////////////////////////////////////////////////////////

void	CTaskVehicleGoToPointWithAvoidanceAutomobile::TemporarilyBrake_OnEnter		(CVehicle* pVehicle)
{
	u32	Wait = pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO && !m_bWaitingForPlayerPed ? 0 
		: CDriverPersonality::FindDelayBeforeAcceleratingAfterObstructionGone(pVehicle->GetDriver(), pVehicle, m_bWaitingForPlayerPed);
	SetNewTask( rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + Wait) );
}

/////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGoToPointWithAvoidanceAutomobile::TemporarilyBrake_OnUpdate	(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE))
	{
		SetState(State_GoToPoint);		//finished braking so now head for target again.
	}

	ProcessSuperDummy(pVehicle, GetTimeStep());

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	return FSM_Continue;
}
/////////////////////////////////////////////////////////////////////////////////

void	CTaskVehicleGoToPointWithAvoidanceAutomobile::SwerveForHeadOnCollision_OnEnter(CVehicle* pVehicle)
{
	u32	Wait = 2000;

	//avoid in the opposite direction of the opposing vehicle's velocity. the ShouldGiveWarning()
	//checks have already ensured that it's roughly in front of us and moving straight on to us anyway

	CTaskVehicleHeadonCollision::SwerveDirection swerveDir = CTaskVehicleHeadonCollision::Swerve_NoPreference;
	if (m_vCachedGiveWarningPosition.IsNonZero())
	{
		Vector3 vMyRightFlat = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetRight());
		vMyRightFlat.z = 0.0f;

		Vector3 vDelta = m_vCachedGiveWarningPosition - VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
		vDelta.z = 0.0f;

		CVehicleNodeList* pNodelist = pVehicle->GetIntelligence()->GetNodeList();
		bool bIsOnNarrowRoad = false;
		if (pNodelist)
		{
			const s32 iTargetNodeIndex = pNodelist->GetTargetNodeIndex();
			const s16 iTargetLinkIndex = pNodelist->GetPathLinkIndex(iTargetNodeIndex);
			const CPathNode* pTargetNode = pNodelist->GetPathNode(iTargetNodeIndex);
			const CPathNodeLink* pLink = (iTargetLinkIndex >= 0 && pTargetNode)
				? ThePaths.FindLinkPointerSafe(pTargetNode->GetAddrRegion(),iTargetLinkIndex)
			: NULL;
			bIsOnNarrowRoad = pLink && pLink->m_1.m_NarrowRoad;
		}
		

		const float fDot = vDelta.Dot(vMyRightFlat);
		if (bIsOnNarrowRoad)
		{
			swerveDir = CTaskVehicleHeadonCollision::Swerve_Straight;
		}
		else if (fDot > 0.0f)
		{
			swerveDir = CTaskVehicleHeadonCollision::Swerve_Left;
		}
		else
		{
			swerveDir = CTaskVehicleHeadonCollision::Swerve_Right;
		}
	}

	SetNewTask( rage_new CTaskVehicleHeadonCollision(NetworkInterface::GetSyncedTimeInMilliseconds() + Wait, swerveDir) );
}

/////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGoToPointWithAvoidanceAutomobile::SwerveForHeadOnCollision_OnUpdate(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_HEADON_COLLISION))
	{
		SetState(State_GoToPoint);		//finished braking so now head for target again.
	}

	ProcessSuperDummy(pVehicle, GetTimeStep());

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	return FSM_Continue;
}
/////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldDoThreePointTurn(CVehicle *pVehicle, float* pDirToTargetOrientation, float* pVehDriveOrientation)
{
	if (!m_bAllowThreePointTurns)
	{
		return false;
	}
	
	//B* 2160704; don't allow large vehicles to do three point turns when they enter goto state
	TUNE_GROUP_BOOL		(FOLLOW_PATH_AI_STEER, allowInstantThreePointTurning, false);
	if(!allowInstantThreePointTurning && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) && GetTimeInState() < 0.1f)
	{
		return false;
	}

	bool	isStuck;
	float desiredAngleDiff, absDesiredAngleDiff;
	u32 impatienceTimer;

	const bool bOnSmallRoad = pVehicle->GetIntelligence()->IsOnSmallRoad();

	bool bRouteContainsUTurn = false;
	CTaskVehicleCruiseNew* pTask = static_cast<CTaskVehicleCruiseNew*>(FindParentTaskOfType(CTaskTypes::TASK_VEHICLE_CRUISE_NEW));
	if (!pTask)
	{
		pTask = static_cast<CTaskVehicleGoToAutomobileNew*>(FindParentTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW));
	}
	if (pTask)
	{
		bRouteContainsUTurn = pTask->DoesPathContainUTurn();		
	}

	m_bPathContainsUTurn = bRouteContainsUTurn;

	return	CTaskVehicleThreePointTurn::ShouldDoThreePointTurn(pVehicle, pDirToTargetOrientation, pVehDriveOrientation, IsDrivingFlagSet(DF_StopAtLights), isStuck, desiredAngleDiff, absDesiredAngleDiff, impatienceTimer, bOnSmallRoad, bRouteContainsUTurn );
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldWaitForTrafficBeforeTurningLeft(CVehicle* pVeh, const CEntity* pException, const spdAABB& /*boundBox*/) const
{
	PF_FUNC(AI_AvoidanceShouldWaitForTrafficBeforeTurningLeft);

	if (pVeh->GetIntelligence()->GetJunctionTurnDirection() != BIT_TURN_LEFT)
	{
		return false;
	}
	
	CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
	if (!pNodeList)
	{
		return false;
	}

	//not a junction or a junction with train tracks. 
	//we don't want to left turn yield here because generally the spot we choose
	//for yielding is on the tracks
	const CNodeAddress& junctionNode = pVeh->GetIntelligence()->GetJunctionNode();
	CJunction* pJunction = pVeh->GetIntelligence()->GetJunction();
	if (!pJunction || pJunction->ShouldCarsStopForTrain())
	{
		return false;
	}

 	const CPathNode* pJunctionNode = ThePaths.FindNodePointerSafe(junctionNode);
	if (!pJunctionNode)
	{
		return false;
	}
	const Vector3 vVehicleBonnetPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
 	const Vector3 vJunctionPos = pJunctionNode->GetPos();

	Vector2 vEntryDir, vExitDir;
	pJunction->FindEntryAndExitDirs(pVeh, vEntryDir, vExitDir);

	CNodeAddress entryNode, exitNode;
	int entryLane = 0, exitLane = 0;
	pJunction->FindEntryAndExitNodes(pVeh, entryNode, exitNode, entryLane, exitLane);

	//don't wait if we aren't ready to turn
	//(as opposed to just have entered the intersection)
	bool bInGoodPositionToStop = false;
	if (!entryNode.IsEmpty())
	{
		s32 iEntranceIndex = pJunction->FindEntranceIndexWithNode(entryNode);
		if(iEntranceIndex >= 0)
		{
			const CJunctionEntrance& rMyEntrance = pJunction->GetEntrance(iEntranceIndex);
			const Vector3& vEntryPos = rMyEntrance.m_vPositionOfNode;
			const float fEntryLinkLengthSqr = vEntryPos.Dist2(vJunctionPos);

			const float fDistSqrToJunction = vJunctionPos.Dist2(vVehicleBonnetPos);
			
			static dev_float s_fEntryLinkLengthThresholdForStoppingSqr = 0.4f * 0.4f;
			if (fDistSqrToJunction < (fEntryLinkLengthSqr * s_fEntryLinkLengthThresholdForStoppingSqr))
			{
				bInGoodPositionToStop = true;
			}
		}
	}

	const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
	const Vec3V vehBonnetPos = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, IsDrivingFlagSet(DF_DriveInReverse));

	//have we already passed the junction?
	Vector3 vCarToJunction = vJunctionPos - vVehicleBonnetPos;
	vCarToJunction.z = 0.0f;
	static dev_float s_fOverJunctionThreshold = -1.0f;
	if (vCarToJunction.Dot(vehDriveDir) < s_fOverJunctionThreshold)
	{
		return false;
	}

	//if our light went red already, bail out and just get the hell out of the intersection
	if (CVehicleJunctionHelper::ApproachingRedLight(pVeh))
	{
		return false;
	}

	//next, iterate through every car that the junction knows about
	//and see if they might collide with us
	//	-they need to either share the link from the junction to our destination with us (turning in the same direction)
	//	-or they need to be traveling down the road we were coming from (going straight ahead)
	//	-can't be stopping for lights/traffic
	//	-can't have a give way node (that would mean they should defer to us)

	//int iOurEntrance;
	//float fOurDistToJunction;
	//pJunction->CalculateDistanceToJunction(pVeh, &iOurEntrance, fOurDistToJunction);

	for (int i = 0; i < CJunction::JUNCTION_MAX_VEHICLES; i++)
	{
		CVehicle* pOtherVehicle = pJunction->GetVehicle(i);
		if (!pOtherVehicle)
		{
			continue;
		}

		//don't stop for ourselves!
		if (pVeh == pOtherVehicle)
		{
			continue;
		}

		//don't stop for the exception vehicle
		if (pOtherVehicle == pException)
		{
			continue;
		}

		if (pOtherVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS
			|| pOtherVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC
			|| pOtherVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GIVE_WAY)
		{
			continue;
		}

		//if they've already passed us, don't bother
		const Vec3V vOtherBonnetPos = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pOtherVehicle, false);
		Vector3 vUsToThem = VEC3V_TO_VECTOR3(vOtherBonnetPos - vehBonnetPos);
		vUsToThem.z = 0.0f;
		if (vehDriveDir.Dot(vUsToThem) < 0.0f)
		{
			continue;
		}

		//don't yield for stopped cars either
		if (pOtherVehicle->GetAiXYSpeed() < 0.5f)
		{
			continue;
		}

		//from here on the tests involve iterating through the node lists...try and put simpler
		//rejections above here
// 		CVehicleNodeList* pOtherNodeList = pOtherVehicle->GetIntelligence()->GetNodeList();
// 		if (!pOtherNodeList)
// 		{
// 			continue;
// 		}

// 		if (pOtherNodeList->GivesWayBeforeNextJunction())
// 		{
// 			continue;
// 		}

		int iEntrance;
		float fDistToJunction;
		if (!pJunction->CalculateDistanceToJunction(pOtherVehicle, &iEntrance, fDistToJunction))
		{
			continue;
		}

		if (iEntrance == -1)
		{
			continue;
		}

		CJunctionEntrance& rEntrance = pJunction->GetEntrance(iEntrance);
		const CPathNode* pOtherEntranceNode = ThePaths.FindNodePointerSafe(rEntrance.m_Node);
		if (!pOtherEntranceNode || pOtherEntranceNode->IsGiveWay())
		{
			continue;
		}

		//not coming from in front of us
		if ((-rEntrance.m_vEntryDir).Dot(vEntryDir) > -0.7f)
		//if (rEntrance.m_vEntryDir.Dot(rMyEntrance.m_vEntryDir) > -0.7f)
		{
			continue;
		}

		//not turning into a conflicted lane
		//conflicted lanes:
		//	-any in the direction we're coming from
		//	-the direction we're going toward, unless it's
		//		in a lane to our right
		CNodeAddress rPrevNode, rNextNode;
		s32 iEntryLane, iExitLane;
		if (!pJunction->FindEntryAndExitNodes(pOtherVehicle, rPrevNode, rNextNode, iEntryLane, iExitLane))
		{
			continue;
		}

		const CPathNode* pOtherExitNode = ThePaths.FindNodePointerSafe(rNextNode);
		if (!pOtherExitNode)
		{
			continue;
		}

		Vector3 vOtherExitDir = pOtherExitNode->GetPos() - pJunction->GetJunctionCenter();
		vOtherExitDir.NormalizeSafe();
		if (vOtherExitDir.Dot(Vector3(vEntryDir.x, vEntryDir.y, 0.0f)) > -0.7f && vOtherExitDir.Dot(Vector3(vExitDir.x, vExitDir.y, 0.0f)) < 0.7f)
		{
			continue;
		}

		//if the other guy has a red light, don't wait for him
		const bool bApproachingRedLight = CVehicleJunctionHelper::ApproachingRedLight(pOtherVehicle);
		if (bApproachingRedLight)
		{
			continue;
		}

		const float fOtherVehSpeed = pOtherVehicle->GetAiVelocity().Mag();
		const float fTimeToJunction = fOtherVehSpeed < 0.01f ? LARGE_FLOAT : fDistToJunction / fOtherVehSpeed;
		TUNE_GROUP_FLOAT (FOLLOW_PATH_AI_STEER, fTimeItTakesUsToLeaveJunction, 1.5f, 0.0f, 100.0f, 0.1f)

		if (fTimeToJunction < fTimeItTakesUsToLeaveJunction)
		{
			if (bInGoodPositionToStop)
			{
#if __BANK
				if (CJunctions::m_bDebug)
				{
					//grcDebugDraw::Sphere(pVeh->GetVehiclePosition(), 1.5f, Color_red, false);
					grcDebugDraw::Line(pVeh->GetVehiclePosition(), pOtherVehicle->GetVehiclePosition(), Color_white, Color_orange);
				}
#endif //__BANK

				pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();

				return true;
			}
			else
			{
				pVeh->m_nVehicleFlags.bWillTurnLeftAgainstOncomingTraffic = true;
			}
			
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPointWithAvoidanceAutomobile::DealWithShoreAvoidance(const CVehicle *pVeh, float &desiredSpeed, float &toTargetOrientation, float UNUSED_PARAM(vehDriveOrientation))
{
	if ( pVeh->InheritsFromBoat() )
	{
			CBoatAvoidanceHelper* pBoatAvoidanceHelper = pVeh->GetIntelligence()->GetBoatAvoidanceHelper();
			if ( pBoatAvoidanceHelper )
			{
				pBoatAvoidanceHelper->SetDesiredOrientation(toTargetOrientation);

				static float s_AvoidanceMaxSlowDown = 0.95f;
				static float s_AvoidanceMaxSlowDownRacing = 0.95f;

				desiredSpeed = pBoatAvoidanceHelper->ComputeAvoidanceSpeed(desiredSpeed, pVeh->m_nVehicleFlags.bIsRacing ? s_AvoidanceMaxSlowDownRacing : s_AvoidanceMaxSlowDown);
				toTargetOrientation = pBoatAvoidanceHelper->ComputeAvoidanceOrientation(toTargetOrientation);
			}
	}
	
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::DealWithTrafficAndAvoidance(CVehicle *pVeh, float &desiredSpeed, float &toTargetOrientation, float vehDriveOrientation, bool& bGiveWarning, const bool bWasSlowlyPushingPlayer, const float fTimeStep)
{
	PF_FUNC(AI_Avoidance);

	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, WEAVEBLOCKSIZE, 21.0f, 0.0f, 100.0f, 0.1f)
	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, SMALLWEAVEBLOCKSIZE, 10.0f, 0.0f, 100.0f, 0.1f)
	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, WEAVE_SPEED_MULT, 24.0f, 0.0f, 100.0f, 0.1f)
	TUNE_GROUP_BOOL		(FOLLOW_PATH_AI_STEER,			handleTraffic, true);
	TUNE_GROUP_BOOL		(FOLLOW_PATH_AI_STEER,			handleBuildings, true);
	
	//Clear the obstruction information.
	m_ObstructionInformation.Clear();

	const CTaskVehicleMissionBase* pActiveTask = pVeh->GetIntelligence()->GetActiveTask();
	const bool bVehicleIsFleeing = pActiveTask && pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_FLEE;

	//ambient vehicles have DF_SwerveAroundAllCars set by default.
	//we may want to disable it if we have a non-aggressive driver
	const bool bShouldPreventBikeSwerving = pVeh->InheritsFromBike() 
		&& (pVeh->PopTypeIsRandom() || (pVeh->GetDriver() && pVeh->GetDriver()->PopTypeIsRandom()))
		&& (!pVeh->GetDriver() || !pVeh->GetDriver()->IsLawEnforcementPed())
		&& !CDriverPersonality::WeavesInBetweenLanesOnBike(pVeh->GetDriver(), pVeh)
		&& !bVehicleIsFleeing;

	CarAvoidanceLevel carAvoidanceLevel = CAR_AVOIDANCE_NONE;
	if( (IsDrivingFlagSet(DF_SwerveAroundAllCars) && !bShouldPreventBikeSwerving) || m_bAvoidingCarsUntilClear
		|| pVeh->m_nVehicleFlags.bToldToGetOutOfTheWay)
	{
		carAvoidanceLevel = CAR_AVOIDANCE_FULL;
	}
	else if( IsDrivingFlagSet(DF_SteerAroundStationaryCars) || IsDrivingFlagSet(DF_SwerveAroundAllCars) )
	{
		carAvoidanceLevel = CAR_AVOIDANCE_LITTLE;
		//avoidanceRadius = SMALLWEAVEBLOCKSIZE;
	}

	spdAABB tempBox;
	const spdAABB& boundBox = pVeh->GetLocalSpaceBoundBox(tempBox);

	// Increase the size of the area being checked for cars going faster.
	//const float fSpeed = rage::Sqrtf(pVeh->GetAiVelocity().x * pVeh->GetAiVelocity().x + pVeh->GetAiVelocity().y * pVeh->GetAiVelocity().y);
	const CAIHandlingInfo* pAIHandlingInfo = pVeh->GetAIHandlingInfo();
	Assert(pAIHandlingInfo);
	const float avoidanceRadius = Max(pAIHandlingInfo->GetDistToStopAtCurrentSpeed(pVeh->GetAiVelocity().XYMag()), pAIHandlingInfo->GetMinBrakeDistance()) 
		+ boundBox.GetMax().GetYf();
	
	//float fSpeedMult = 1.0f + (fSpeed / WEAVE_SPEED_MULT);
	//fSpeedMult = rage::Min(fSpeedMult, 2.0f);
	//avoidanceRadius *= fSpeedMult;

	//if we got fed up with the player after waiting for him and started driving,
	//we should be allowed to steer around peds here
	//also steer around peds if we'd normally be allowed to stop for them, and are on a junction with
	//a train appraoching
	const bool bOnJunctionWithTrainApproaching = pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO
		&& pVeh->GetIntelligence()->GetJunction() && pVeh->GetIntelligence()->GetJunction()->ShouldCarsStopForTrain();
	const bool bAvoidPeds = IsDrivingFlagSet(DF_SteerAroundPeds) || bWasSlowlyPushingPlayer 
		|| (IsDrivingFlagSet(DF_StopForPeds) && bOnJunctionWithTrainApproaching);
	const bool bAvoidObjs = IsDrivingFlagSet(DF_SteerAroundObjects);

	// TODO: Replace this with bitfields
	const bool bWillStopForCars = IsDrivingFlagSet(DF_StopForCars);
	const bool bWillStopForPeds = IsDrivingFlagSet(DF_StopForPeds);
	const bool bWillStopForLights = IsDrivingFlagSet(DF_StopAtLights);
	const bool bWillSwerveForObjects = IsDrivingFlagSet(DF_SteerAroundObjects);
	
	//const bool bCopperBlockedCouldLeaveCar = pVeh->m_nVehicleFlags.bCopperBlockedCouldLeaveCar;
	bool bHandleSteering = handleTraffic;

	//did we already have a lane change queued up?
	//const bool bAlreadyTryingToChangeLanes = m_bShouldChangeLanesForTraffic;

	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, TimeToKeepAvoiding, 5.0f, 0.0f, 100.0f, 0.1f);
	if( m_fAvoidedEntityCounter > 0.0f )
	{
		m_fAvoidedEntityCounter -= fTimeStep;

		// If we were recently avoiding something, don't allow timeslicing.
		if(!ShouldAllowTimeslicingWhenAvoiding())
		{
			pVeh->m_nVehicleFlags.bLodCanUseTimeslicing = false;
			pVeh->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
		}
		pVeh->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
	}
	else if(!m_bStoppingForTraffic && pVeh->m_nVehicleFlags.bLodShouldUseTimeslicing)
	{
		// In this case, we are not stopping for traffic, but the base class had set
		// the aggressive bLodShouldUseTimeslicing option. We now downgrade this to
		// bLodCanUseTimeslicing, meaning that timeslicing will only be used at a distance.
		pVeh->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
		pVeh->m_nVehicleFlags.bLodCanUseTimeslicing = true;
	}

	// Only periodically update the steering if nothing has been avoided recently.
	// As soon as something is avoided we update it every frame for a few seconds
	if( m_fAvoidedEntityCounter <= 0.0f && carAvoidanceLevel == CAR_AVOIDANCE_LITTLE )
	{
		const bool bOnlyCarOnJunction = pVeh->GetIntelligence()->GetJunction() && pVeh->GetIntelligence()->GetJunction()->GetNumberOfCars() <=1;
		const bool bHasGoCommand = pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO;
		const bool bUpdateDueToBeingInJunction = bHasGoCommand && !bOnlyCarOnJunction && !pVeh->IsSuperDummy();

		if(!bUpdateDueToBeingInJunction && pVeh->GetIntelligence()->GetSteeringDistributer().IsRegistered())
		{
			if(!pVeh->GetIntelligence()->GetSteeringDistributer().ShouldBeProcessedThisFrame())
			{
				bHandleSteering = false;
			}
		}
	}

	const CEntity* pException = NULL;
	if( IsDrivingFlagSet(DF_DontAvoidTarget) )
	{
		pException = GetTargetEntity();
	}

	if (handleTraffic && bWillStopForCars)
	{
		pVeh->m_nVehicleFlags.bIsWaitingToTurnLeft = ShouldWaitForTrafficBeforeTurningLeft(pVeh, pException, boundBox);
	}

	//m_bShouldChangeLanesForTraffic = CanSwitchLanesToAvoidObstruction(pVeh, pVeh->GetIntelligence()->GetCarWeAreBehind(), desiredSpeed);
	
	//There are a couple situations when tailgating should be disabled:
	//	1) This car does not stop for other cars.
	//	2) This car is in full avoidance mode (typically means the driver wants to swerve around other cars).
	bool bTailgating = false;
	if (handleTraffic && bWillStopForCars && (carAvoidanceLevel != CAR_AVOIDANCE_FULL) && CVehicleIntelligence::m_bTailgateOtherCars)
	{
		bTailgating = TailgateCarInFront(pVeh, &desiredSpeed);
	}

	float MaxSpeed = desiredSpeed;
	if(m_pOptimization_pLastCarBlockingUs)
	{
		if (handleTraffic && !pVeh->m_nVehicleFlags.bIsWaitingToTurnLeft && !bTailgating && bWillStopForCars && desiredSpeed > 0.0f
		&& (IsDrivingFlagSet(DF_StopForCars) || IsDrivingFlagSet(DF_StopForPeds)) )
		{
			bool bCheckForBraking = true;
			// Only periodically update the braking if the car is stationary.
			// As soon as it is free to move again we update hte brakes each frame
			if(pVeh->GetIntelligence()->GetBrakingDistributer().IsRegistered())
			{
				if(!pVeh->GetIntelligence()->GetBrakingDistributer().ShouldBeProcessedThisFrame())
				{
					bCheckForBraking = false;
				}
			}
			if( bCheckForBraking || m_bAvoidingCarsUntilClear)
			{
				CVehicle *pObstructor = m_pOptimization_pLastCarBlockingUs;
				m_pOptimization_pLastCarBlockingUs = NULL;

				//clear information about cars that are blocking us
				pVeh->GetIntelligence()->m_pCarThatIsBlockingUs = NULL;		// Will be filled in if we are blocked by a car later in this function.
				m_fSmallestCollisionTSoFar = 100.0f;						// This is used to make sure we find the nearest collision.

				const bool bPreventFullStop = false;
				SlowCarDownForOtherCar(pVeh, pObstructor, MaxSpeed, desiredSpeed, toTargetOrientation, true, bGiveWarning, boundBox, pVeh->GetBoundRadius(), carAvoidanceLevel, bPreventFullStop, true);
			}
		}
		else if (!IsDrivingFlagSet(DF_StopForCars) && !IsDrivingFlagSet(DF_StopForPeds))
		{
			// Make sure we don't early out from the avoidance angle tests - normally this is cleared or set via SlowCarDownForOtherCar
			m_pOptimization_pLastCarBlockingUs = NULL;
		}
	}
	else
	{
		//clear information about cars that are blocking us
		pVeh->GetIntelligence()->m_pCarThatIsBlockingUs = NULL;		// Will be filled in if we are blocked by a car later in this function.
		m_fSmallestCollisionTSoFar = 100.0f;						// This is used to make sure we find the nearest collision.
	}

	bool bUseAltSteerDirectionForSlowdownTest = false;
	const float fDesiredDirectionBeforeWeaveThroughTraffic = toTargetOrientation;
	if(handleTraffic && bHandleSteering && !pVeh->m_nVehicleFlags.bIsWaitingToTurnLeft && !bTailgating)
	{
		// If the car finds something to avoid set a timer to make sure we update the avoidance every frame for a few seconds.
		float newOrientation = FindAngleToWeaveThroughTraffic(pVeh, pException, toTargetOrientation, vehDriveOrientation, 1.0f, carAvoidanceLevel, bAvoidPeds, bAvoidObjs, avoidanceRadius, bGiveWarning, bWillStopForCars, boundBox, desiredSpeed);
		if( ABS(toTargetOrientation - newOrientation) > FLT_EPSILON)
		{
			toTargetOrientation = newOrientation;
			bUseAltSteerDirectionForSlowdownTest = true;
			m_fAvoidedEntityCounter = TimeToKeepAvoiding;

			// We are now avoiding something, don't allow timeslicing right now.
			if(!ShouldAllowTimeslicingWhenAvoiding())
			{
				pVeh->m_nVehicleFlags.bLodCanUseTimeslicing = false;
				pVeh->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
			}
			pVeh->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
			
			CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelperMutable();
			if (pFollowRoute)
			{
				pFollowRoute->GiveLaneSlack();
			}
		}
		else if (m_bReEnableSlowForCarsWhenDoneAvoiding)
		{
			SetDrivingFlag(DF_StopForCars, true);
			m_bReEnableSlowForCarsWhenDoneAvoiding = false;
		}

		//pVeh->m_nVehicleFlags.bCopperBlockedCouldLeaveCar = bCopperBlockedCouldLeaveCar;
		if(bGiveWarning)
		{
			//CVehicle::ms_debugDraw.AddLine(pVeh->GetVehiclePosition(), pVeh->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 10.0f), Color_yellow2);
			pVeh->GiveWarning();
		}
	}
	else
	{
		m_bAvoidedRightLastFrame = false;
		m_bAvoidedLeftLastFrame = false;

		if (m_bReEnableSlowForCarsWhenDoneAvoiding)
		{
			SetDrivingFlag(DF_StopForCars, true);
			m_bReEnableSlowForCarsWhenDoneAvoiding = false;
		}
	}

	// This bit avoids buildings by steering extremely if we are in the process of hitting one
	if(handleBuildings && pVeh->GetIntelligence()->m_bSteerForBuildings)
	{
		float desiredAngleDiff = SubtractAngleShorter(toTargetOrientation, vehDriveOrientation);
		toTargetOrientation = fwAngle::LimitRadianAngle(toTargetOrientation);
		toTargetOrientation = FindAngleToAvoidBuildings(pVeh, (desiredAngleDiff < 0.0f), vehDriveOrientation, toTargetOrientation, boundBox);
	}

	// Possibly slow down to avoid collisions with other cars.
	float speedMultiplierTraffic = pVeh->m_nVehicleFlags.bIsWaitingToTurnLeft ? 0.0f : 1.0f;
	if((handleTraffic && !pVeh->m_nVehicleFlags.bIsWaitingToTurnLeft && !bTailgating) || m_bBumperInsideBoundsThisFrame)
	{
		if( bWillStopForCars || bWillStopForPeds || bWillStopForLights || bWillSwerveForObjects || m_bBumperInsideBoundsThisFrame)
		{
			if(desiredSpeed > 0.0f)	// avoid div by zero
			{
				const float fMaxSpeed = FindMaximumSpeedForThisCarInTraffic(pVeh, toTargetOrientation, bUseAltSteerDirectionForSlowdownTest, bGiveWarning, MaxSpeed, avoidanceRadius, boundBox, carAvoidanceLevel, bWasSlowlyPushingPlayer);
				speedMultiplierTraffic = fMaxSpeed / desiredSpeed;
			}
			else
			{
				speedMultiplierTraffic = 0.0f;
			}
		}
		// @RSGNWE-JW handle the case where a crazy driver wants to bull rush, erm, ram through a Ped.
		else if ( IsDrivingFlagSet(DMode_PloughThrough) )
		{
			float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
			const Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
			GenerateMinMaxForPedAvoidance(vecVehiclePosition, avoidanceRadius, MinX, MinY, MaxX, MaxY);

			//even though we have bWasSlowlyPushingPlayer in this scope, if the driver is plowing through
			//we don't want to do anything different
			SlowCarDownForPedsSectorListHelper(this, pVeh, MinX, MinY, MaxX, MaxY, &MaxSpeed, MaxSpeed, false);
		}
	}

	desiredSpeed *= rage::Min(1.0f, speedMultiplierTraffic);

	if (!bWillStopForCars && pVeh->m_nVehicleFlags.bIsRacing)
	{
		desiredSpeed = rage::Min(desiredSpeed, FindMaximumSpeedForRacingCar(pVeh, desiredSpeed, boundBox));
	}
	
	if (desiredSpeed < SMALL_FLOAT)
	{
		//if we found a new direction but then stopped, 
		//and we were already stopped, don't change direction
		//this just causes us to flick our wheels in place.
		//allow it if we're moving or want to move
		if (pVeh->GetAiXYSpeed() < 0.1f * 0.1f)
		{
			toTargetOrientation = fDesiredDirectionBeforeWeaveThroughTraffic;

			//if we want to swerve around all cars, and also stop for them,
			//and we come to a complete halt, unset DF_StopForCars until we're moving again
			if (IsDrivingFlagSet(DF_SwerveAroundAllCars) && IsDrivingFlagSet(DF_StopForCars))
			{
				m_bReEnableSlowForCarsWhenDoneAvoiding = true;
				SetDrivingFlag(DF_StopForCars, false);
			}
		}
	}

	if (desiredSpeed < CVehicleIntelligence::ms_fMoveSpeedBelowWhichToCheckIfStuck)
	{
		//this is done down here now since it's not set on a per-vehicle basis anymore
		pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::GenerateMinMaxForPedAvoidance(const Vector3& vecVehiclePosition, const float avoidanceRadius, float& MinX, float& MinY, float& MaxX, float& MaxY)
{
	MinX = vecVehiclePosition.x - avoidanceRadius;
	MaxX = vecVehiclePosition.x + avoidanceRadius;
	MinY = vecVehiclePosition.y - avoidanceRadius;
	MaxY = vecVehiclePosition.y + avoidanceRadius;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindAngleToAvoidBuildings
// PURPOSE :  Calculates the illegal angles and returns the one closest to the
//			  original angle.
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAngleToAvoidBuildings(CVehicle* const pVeh, bool bTurningClockwise, float vehDriveOrientation, float dirToTargetOrientation, const spdAABB& boundBox)
{
	PF_FUNC(AI_AvoidanceFindAngleToAvoidBuildings);

	// Only mission vehicles do this as it's expensive.
	const bool bAcceptableVehOrDriver = pVeh->PopTypeIsMission() || (pVeh->GetDriver() && (pVeh->GetDriver()->PopTypeIsMission() && !pVeh->GetDriver()->IsPlayer()));

	if(!bAcceptableVehOrDriver)
	{
		return dirToTargetOrientation;
	}

	phIntersection testIntersection;
	phSegment testSegment;

#define LINESCANDIST	(4.0f)
	//#define LINESCANSIDEFRACTION(0.2f)
	if(IsDrivingFlagSet(DF_DriveInReverse))
	{
		if(bTurningClockwise)
		{
			testSegment.A = pVeh->TransformIntoWorldSpace(Vector3(boundBox.GetMin().GetXf(), -boundBox.GetMax().GetYf(), 0.4f));
			testSegment.B = testSegment.A -(LINESCANDIST * VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
		}
		else
		{
			testSegment.A = pVeh->TransformIntoWorldSpace(Vector3(boundBox.GetMax().GetXf(), -boundBox.GetMax().GetYf(), 0.4f));
			testSegment.B = testSegment.A -(LINESCANDIST * VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
		}
	}
	else
	{
		if(bTurningClockwise)
		{
			testSegment.A = pVeh->TransformIntoWorldSpace(Vector3(boundBox.GetMin().GetXf(), boundBox.GetMax().GetYf(), 0.4f));
			testSegment.B = testSegment.A +(LINESCANDIST * VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
		}
		else
		{
			testSegment.A = pVeh->TransformIntoWorldSpace(Vector3(boundBox.GetMax().GetXf(), boundBox.GetMax().GetYf(), 0.4f));
			testSegment.B = testSegment.A +(LINESCANDIST * VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
		}
	}
	bool	bHit;
	if(CPhysics::GetLevel()->TestProbe(testSegment, &testIntersection, pVeh->GetCurrentPhysicsInst(), ArchetypeFlags::GTA_MAP_TYPE_MOVER) &&
		testIntersection.GetNormal().GetZf() < 0.7f)
	{
		bHit = true;
	}
	else
	{
		bHit = false;
	}

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Color32 Col = Color32(155, 155, 155, 255);
		if(bHit)
		{
			Col = Color32(255, 255, 0, 255); 
		}
		grcDebugDraw::Line(testSegment.A, testSegment.B, Col);
	}
#endif

	if(bHit)
	{
		//count swerving for buildings as something that dirties a vehicle
		pVeh->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

		if(bTurningClockwise)
		{
			return vehDriveOrientation - 1.0f;
		}
		else
		{
			return vehDriveOrientation + 1.0f;
		}
	}
	return dirToTargetOrientation;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindAngleToWeaveThroughTraffic
// PURPOSE :  Calculates the illegal angles and returns the one closest to the
//			  original angle.
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAngleToWeaveThroughTraffic(CVehicle* pVeh, const CEntity *pException, float Direction
	, float vehDriveOrientation, float CheckDistMult, CarAvoidanceLevel carAvoidanceLevel, bool bAvoidPeds, bool bAvoidObjs
	, float avoidanceRadius, bool& bGiveWarning, const bool bWillStopForCars, const spdAABB& boundBox, const float fDesiredSpeed)
{
	PF_FUNC(AI_AvoidanceFindAngleToWeaveThroughTraffic);
	
	float	MinX, MaxX, MinY, MaxY;
//	float	LeftAngle, RightAngle, DiffL, DiffR;// OrientationDiff;
//	float	LeftAngleOld, RightAngleOld;
	//bool	bMustGoLeft = false, bMustGoRight = false;

	const float	originalDirection = Direction;

#if __DEV
	static CVehicle *pDebugVeh = NULL;
	if(pDebugVeh == pVeh)
	{
		Displayf("Our car\n");
	}
#endif

	// Identify scan area.(Block around car)
	// Shift it forwads in the direction of travel slightly to allow us to reduce the size of the box.
	float fAreaSize = avoidanceRadius * CheckDistMult;
	const Vector3 vecVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	Vector3 vecToTarget = m_Params.GetTargetPosition() - vecVehiclePos;
	vecToTarget.NormalizeFast();
	Vector3 vCentre = vecVehiclePos + vecToTarget*fAreaSize*0.33f;
	MinX = vCentre.x - fAreaSize;
	MaxX = vCentre.x + fAreaSize;
	MinY = vCentre.y - fAreaSize;
	MaxY = vCentre.y + fAreaSize;

#if __BANK
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, RENDER_AVOIDANCE_SEARCH, false );
	if( RENDER_AVOIDANCE_SEARCH && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		grcDebugDraw::BoxAxisAligned(Vec3V(MinX, MinY, vecVehiclePos.z - 7.0f), Vec3V(MaxX, MaxY, vecVehiclePos.z +7.0f ), Color_red);
	}
#endif // __BANK

	const bool bAvoidVehs = (carAvoidanceLevel != CAR_AVOIDANCE_NONE);

	PF_START(AI_AvoidanceNewSearchForUnobstructedDirections);
	Direction = TestDirectionsForObstructionsNew(pVeh, pException, MinX, MinY, MaxX, MaxY, originalDirection, vehDriveOrientation, carAvoidanceLevel, bAvoidVehs, bAvoidPeds, bAvoidObjs, bGiveWarning, bWillStopForCars, boundBox, fDesiredSpeed);
	PF_STOP(AI_AvoidanceNewSearchForUnobstructedDirections);

	Direction = fwAngle::LimitRadianAngleSafe(Direction);

	// If we haven't had to change direction we are clear and can re-set our driving mode.
	if(m_bAvoidingCarsUntilClear && AreNearlyEqual(originalDirection, Direction))
	{
		if(fwTimer::GetTimeInMilliseconds() > m_startTimeAvoidCarsUntilClear + 3000)
		{
			//SetDrivingFlag(DF_AvoidingCarsUntilClear, false);
			m_bAvoidingCarsUntilClear = false;
		}
	}

	return Direction;
}

void HelperDoDriveByOnTarget(CVehicle* pVeh, CPed* pDriver, const CPhysical* pTarget, const u32 uWeaponHash = -1)
{
	if (!pDriver || !pVeh || !pTarget)
	{
		return;
	}

	CAITarget target(pTarget);
	const u32 uFiringPattern = ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2);		
	const float fAbortRange = 40.0f;

	//only do this for wandering peds, even though all the logic lives in CTaskCarDrive, and it would be functional there too.
	CTask* pCurrent = pDriver->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_DEFAULT);
	CTaskCarDriveWander* pControlVehicleTask = pCurrent && pCurrent->GetTaskType()==CTaskTypes::TASK_CAR_DRIVE_WANDER ? static_cast<CTaskCarDriveWander*>(pCurrent) : NULL;
	if(pControlVehicleTask)
	{
		pControlVehicleTask->RequestDriveBy(target, 1.0f, fAbortRange, uFiringPattern, g_ReplayRand.GetRanged(3000, 6000), uWeaponHash);
	}
}

void HelperFlipOffTarget(CVehicle* pVeh, CPed* pDriver, const CPhysical* pTarget)
{
	if (pDriver)
	{
		HelperDoDriveByOnTarget(pVeh, pDriver, pTarget, pDriver->GetDefaultUnarmedWeaponHash());
	}
}

void HelperMaybePlayHorn(CVehicle* pVeh, const bool bPlayHorn, const bool bLongHorn, const CPhysical* pTarget, const bool bRestrictDriveBy = false, const bool bAllowDingDing = false)
{
	if (bPlayHorn)
	{
		const CVehicle* pOtherVehicle = NULL;
		if (pTarget && pTarget->GetIsTypeVehicle())
		{
			pOtherVehicle = static_cast<const CVehicle*>(pTarget);
		}
		else if (pTarget && pTarget->GetIsTypePed())
		{
			pOtherVehicle = static_cast<const CPed*>(pTarget)->GetVehiclePedInside();
		}

		if (pOtherVehicle && 
			(pOtherVehicle->IsLawEnforcementVehicle() 
			|| pOtherVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EMERGENCY_SERVICE)))
		{
			return;
		}

		//generally don't use our bell for bicycles, unless we're overtaking
		if (pVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE
			&& !bAllowDingDing)
		{
			return;
		}

		//don't honk if we're both racing
		if (pVeh->m_nVehicleFlags.bIsRacing && pOtherVehicle && pOtherVehicle->m_nVehicleFlags.bIsRacing)
		{
			return;
		}

		u32 hornTypeHash = bLongHorn ? ATSTRINGHASH("HELDDOWN",0x839504CB) : 0;
		pVeh->PlayCarHorn(true, hornTypeHash);

#if __DEV
		if (pTarget && pVeh && CPlayerInfo::GetDisplayRecklessDriving())
		{
			Color32 honkColor = bLongHorn ? Color_red : Color_yellow;
			grcDebugDraw::Sphere(pTarget->GetTransform().GetPosition(), 1.5f, honkColor, false);
			grcDebugDraw::Arrow(pVeh->GetVehiclePosition(), pTarget->GetTransform().GetPosition(), 1.0f, honkColor);
		}
#endif //__DEV

		if (bLongHorn && !bRestrictDriveBy)
		{
			HelperFlipOffTarget(pVeh, pVeh->GetDriver(), pTarget);
		}
	}
}

struct AvoidanceTaskObstructionData
{
	/// These are all the "input" parameters
	const CVehicle* pVeh;
	const CEntity *pException;
	float MinX, MinY, MaxX, MaxY;
	float fOriginalDirection;
	float fDesiredSpeed;
	float fCurrentSpeed;
	float vehDriveOrientation;
	u32 turnDir;
	CarAvoidanceLevel carAvoidanceLevel;
	Vec3V vBoundBoxMin;
	Vec3V vBoundBoxMax;
	Vec3V vPosOfOriginalObstruction;
	bool bIsCurrentlyOnJunction;
	bool bWillStopForCars;
	bool bOnSingleTrackRoad;
	bool bRacing;

	/// This is data that we precompute and pass around to various functions
	Vec3V vNormalizedDriveDir;
	Vec3V vFrontLeftCorner; 
	Vec3V vRearLeftCorner;
	Vec3V vBumperVector; 
	Vec3V vSideVector;
	Vec3V vBumperCenter;
	float fAvoidanceLookaheadTimeShort, fAvoidanceLookaheadTimeLong;
	__forceinline float GetAvoidanceLookaheadTime(bool longerProbe) const { return longerProbe ? fAvoidanceLookaheadTimeLong : fAvoidanceLookaheadTimeShort; }

	/// And these are the lists of obstructions themselves
	struct RoadSegment
	{
		Vec2f m_LeftStart, m_LeftEnd;
		Vec2f m_RightStart, m_RightEnd;
		bool m_Valid;
	};

	struct ObstructionPoly
	{
		static const int MAX_OBSTRUCTION_POLY_SIZE = 9; // Start with a triangle, sweep up to 3 times = 3 + 2*3 = 9
		// I'm not using an atFixedArray here because the dumbass compiler can't optimize out the constructor calls for the Vec2f constructor.
		Vec2f m_Verts[MAX_OBSTRUCTION_POLY_SIZE];
		int m_Count;
		const CVehicle* m_Vehicle;
	};

	// Polar coordinates (relative to the front bumper of the car) for where the circular obstruction is
	struct ObstructionCircle
	{
		float m_Orientation;
		float m_Distance;
		float m_HalfBlockedAngleRange; // the "radius" of the circle, as measured in radians for a circle at m_Distance
		const CEntity* m_Entity;
		CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo::eObstructionType m_ObstructionType;
	};

	atFixedArray<RoadSegment, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED> m_RoadSegments;
	atFixedArray<ObstructionPoly, CEntityScanner::MAX_NUM_ENTITIES> m_VehicleObstructions;
	atFixedArray<ObstructionCircle, 2 * CEntityScanner::MAX_NUM_ENTITIES> m_PedAndObjectObstructions;

#if 0
	inline bool CanCullSegment(Vec2f testPoint1, Vec2f testPoint2, Vec2f center, Vec2f planeNormal1, Vec2f planeNormal2, float radiusSq)
	{
		Vec2f testPoint1InWedgeSpace = testPoint1 - center;
		Vec2f testPoint2InWedgeSpace = testPoint2 - center;

		float dist1 = MagSquared(testPoint1InWedgeSpace);
		float dist2 = MagSquared(testPoint2InWedgeSpace);
		if (dist1 > radiusSq && dist2 > radiusSq)
		{
			return true;
		}

		float dot11 = Dot(testPoint1InWedgeSpace, planeNormal1);
		float dot21 = Dot(testPoint2InWedgeSpace, planeNormal1);
		if (dot11 < 0.0f && dot21 < 0.0f)
		{
			return true;
		}

		float dot12 = Dot(testPoint1InWedgeSpace, planeNormal2);
		float dot22 = Dot(testPoint2InWedgeSpace, planeNormal2);
		if (dot12 < 0.0f && dot22 < 0.0f)
		{
			return true;
		}

		return false;
	}

	inline bool CanCullPoint(Vec2f testPoint, Vec2f center, Vec2f planeNormal1, Vec2f planeNormal2)
	{
		Vec2f testPointInWedgeSpace = testPoint - center;
		float dot1 = Dot(testPointInWedgeSpace, planeNormal1);
		float dot2 = Dot(testPointInWedgeSpace, planeNormal2);

		return (dot1 < 0.0f || dot2 < 0.0f);
	}

	void WedgeCull(float minAngle, float maxAngle)
	{
		// Based on the test direction being computed like this:
		//		Vec2f testDir(::rage::Cosf(avoidanceInfo.fDirection), ::rage::Sinf(avoidanceInfo.fDirection));
		// I want a perpendicular vector to that, that points "in" to the wedge for minAngle and maxAngle

		const Vec3V& centerV = pVeh->GetVehiclePosition();
		Vec2f center(centerV.GetXf(), centerV.GetYf());

		Vec2f planeNormal1(-::rage::Sinf(minAngle), ::rage::Cosf(minAngle));
		Vec2f planeNormal2(::rage::Sinf(maxAngle), -::rage::Cosf(maxAngle));

		// cull it all!
		// Road segments
		float edgeCullDist = fDesiredSpeed * 1.7f;
		for(int i = 0; i < m_RoadSegments.GetCount(); i++)
		{
			if (!m_RoadSegments[i].m_Valid)
			{
				continue;
			}
			RoadSegment& rs = m_RoadSegments[i];
			rs.m_Culled = 
				CanCullSegment(rs.m_LeftStart, rs.m_LeftEnd, center, planeNormal1, planeNormal2, edgeCullDist) && 
				CanCullSegment(rs.m_RightStart, rs.m_RightEnd, center, planeNormal1, planeNormal2, edgeCullDist);
		}

		for(int i = 0; i < m_VehicleObstructions.GetCount(); i++)
		{
			ObstructionPoly& obs = m_VehicleObstructions[i];

			bool canCull = true;
			for(int i = 0; i < obs.m_Count; i++)
			{
				if (!CanCullPoint(obs.m_Verts[i], center, planeNormal1, planeNormal2))
				{
					canCull = false;
					break;
				}
			}
			obs.m_Culled = canCull;
		}
	}
#endif

	void DebugDraw(const CVehicle* pVeh)
	{
		// Road segments
		for(int i = 0; i < m_RoadSegments.GetCount(); i++)
		{
			if (!m_RoadSegments[i].m_Valid)
			{
				continue;
			}

			fwVectorMap::DrawLine(m_RoadSegments[i].m_LeftStart, m_RoadSegments[i].m_LeftEnd, Color_green, Color_green);
			fwVectorMap::DrawLine(m_RoadSegments[i].m_RightStart, m_RoadSegments[i].m_RightEnd, Color_blue, Color_blue);
		}

		for(int i = 0; i < m_VehicleObstructions.GetCount(); i++)
		{
			ObstructionPoly& poly = m_VehicleObstructions[i];
			Vec2f vecStart = poly.m_Verts[poly.m_Count-1];
			for(int j = 0; j < poly.m_Count; j++)
			{
				Vec2f vecEnd = poly.m_Verts[j];
				fwVectorMap::DrawLine(vecStart, vecEnd, Color_LightSalmon, Color_LightSalmon);
				fwVectorMap::DrawLine(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition()), Color_grey, Color_LightSalmon);
				vecStart = vecEnd;
			}
		}

		for(int i = 0; i < m_PedAndObjectObstructions.GetCount(); i++)
		{
			ObstructionCircle& circle = m_PedAndObjectObstructions[i];

			Vec3V pos = pVeh->GetVehiclePosition();

			// Convert "back" from polar coords
			float DiffX = rage::Cosf(circle.m_Orientation);
			float DiffY = rage::Sinf(circle.m_Orientation);

			pos += Vec3V(DiffX * circle.m_Distance, DiffY * circle.m_Distance, 0.0f);

			// tan(theta) = radius / distance
			float radius = /*rage::Tanf*/(circle.m_HalfBlockedAngleRange) * circle.m_Distance;

			Color32 color = circle.m_ObstructionType == CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo::Obstruction_Ped ? Color_gold : Color_LightBlue;
				
			fwVectorMap::DrawCircle(RCC_VECTOR3(pos), radius, color, false);
		}
	}
};

bool HelperShouldUseRacingLogic(const bool bIsRacing, const bool bIsBike, const float fCurrentSpeed, const CVehicle* pOtherCar)
{
	const float fOtherCarSpeed = pOtherCar->GetAiXYSpeed();

	bool bIsConservative = pOtherCar->m_nVehicleFlags.bIsRacingConservatively;
	bool bIsVelValid = bIsConservative;
	if(!bIsVelValid)
	{
		static dev_float s_fVelThresholdForRacingBike = 10.0f;
		static dev_float s_fVelThresholdForRacingCar = 10.0f;
		const float fVelThreshold = !bIsBike ? s_fVelThresholdForRacingCar : s_fVelThresholdForRacingBike;
		bIsVelValid = (fabsf(fCurrentSpeed - fOtherCarSpeed) < fVelThreshold);
	}

	return (bIsRacing && pOtherCar->m_nVehicleFlags.bIsRacing)
		&& fOtherCarSpeed > 1.0f
		&& bIsVelValid;
};

Vector3 HelperGetOtherVehsRacingVelocity(const CVehicle* pOtherVeh)
{
	//make this slightly larger so when two vehicles have the same 
	//desired velocity (common at the start of a race)
	//the one behind will yield
	//if the actual velocity is higher than the desired--use that
	static dev_float s_fVelocityMultiplier = 1.01f;
	const CTaskVehicleMissionBase* pOtherActiveTask = pOtherVeh->GetIntelligence()->GetActiveTask();
	const float fDesiredVel = pOtherActiveTask ? (pOtherActiveTask->GetCruiseSpeed() * s_fVelocityMultiplier)
		: pOtherVeh->GetIntelligence()->m_fDesiredSpeedThisFrame * s_fVelocityMultiplier;
	const float fAiXYSpeed = pOtherVeh->GetAiXYSpeed();
	if (fDesiredVel >= 0.0f && fDesiredVel > fAiXYSpeed)
	{
		return VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection() * ScalarV(fDesiredVel));
	}
	else
	{
		//we were just created and probably haven't cached a velocity yet
		return pOtherVeh->GetAiVelocity();
	}
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TestOneDirectionAgainstObstructions(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo& avoidanceInfo, bool testRoadEdges, bool& bShouldGiveWarning, const CPhysical** pHitEntity)
{	 
	const CVehicle* pVeh = obstructionData.pVeh;
	const Vector3 vOurVelocity = pVeh->GetAiVelocity();
	
	// Test against the vehicle polys
	Vec3V vehPos(pVeh->GetVehiclePosition());
	Vec2f vehPos2d(vehPos.GetXf(), vehPos.GetYf());

	const bool bCanUseLongerProbe = pVeh->PopTypeIsMission() || pVeh->m_nVehicleFlags.bIsLawEnforcementVehicle || (pVeh->GetDriver() && pVeh->GetDriver()->PopTypeIsMission());
	const float fLookaheadTime = obstructionData.GetAvoidanceLookaheadTime(bCanUseLongerProbe);

	// The car might be stopped... so the desired speed might not actually be achievable in fLookaheadTime.
	// Lets do a rough approximation of acceleration to rein in the lookahead distance to something more reasonable
	// Useful upper bound? - supercars go 0-60 in 2.5s (!), which is an acceleration of about 10m/s^2. 
	//TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fMaxAccelerationForLookahead, 5.0f, 0.0f, 100.0f, 0.1f);
	//const float fAchievableSpeed = Min(obstructionData.fDesiredSpeed, Max(0.0f, obstructionData.fCurrentSpeed + (s_fMaxAccelerationForLookahead * fLookaheadTime)));
	//const float fLookaheadDistance = obstructionData.fDesiredSpeed * fLookaheadTime;
	//obstructionData.fLookaheadDistance = fLookaheadDistance;
	//obstructionData.fAchievableDistance = fAchievableSpeed * fLookaheadTime;

	//TODO SPARR; Do we really want to use desired speed here?!
	Vec2f testDir(::rage::Cosf(avoidanceInfo.fDirection), ::rage::Sinf(avoidanceInfo.fDirection));
	testDir *= Vec2f(obstructionData.fDesiredSpeed * fLookaheadTime, obstructionData.fDesiredSpeed * fLookaheadTime); // Look ahead 1.7s into the future

	bool bHitSomething = false;

	// Note: we could test this single direction using the new vectorized functions, by simply doing this:
	//	TestAndScoreLineSegmentsAgainstVehicles(obstructionData, &avoidanceInfo, 1, &bHitSomething, bShouldGiveWarning, pHitEntity);
	//	TestAndScoreLineSegmentsAgainstCylindricalObstructions(obstructionData, &avoidanceInfo, 1, &bHitSomething, pHitEntity);
	// However, when I measured that, it was unfortunately slower that way, maybe by 2x for the vehicle case and 4x for
	// the ped/object case.

	int polyCount = obstructionData.m_VehicleObstructions.GetCount();
	for(int i = 0; i < polyCount; i++)
	{
		const AvoidanceTaskObstructionData::ObstructionPoly& poly = obstructionData.m_VehicleObstructions[i];

		float tEnter, tLeave;
		bool hitSomething = geom2D::Test2DSegVsPolygon(tEnter, tLeave, vehPos2d, vehPos2d + testDir, &poly.m_Verts[0], poly.m_Count);

		if (hitSomething)
		{
			// The distance isn't the intersection point, it's the distance between car centers
			const float fDistToOtherCar = tEnter * obstructionData.fDesiredSpeed * fLookaheadTime;
			bool bIsStationaryCar = IsThisAStationaryCar(poly.m_Vehicle);

			if (fDistToOtherCar < FLT_EPSILON)
			{
				// Problem! tEnter is actually inside the obstruction poly, making us consider every direction obstructed.
				// This is not helpful. So do the old test that lets this car move out of the way of the obstruction car
				// before the obstruction car actually obstructs.
				bool bIsParkedCar = IsThisAParkedCar(poly.m_Vehicle);
				bool bOldTestSuccess = false;

				//if the first test failed because our search pos was inside the poly, dont' add any
				//padding we may have done the first time
				const bool bPreventAddMarginForOtherCar = true;

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
				bOldTestSuccess = TestDirectionsAgainstSingleVehicle(obstructionData, const_cast<CVehicle*>(poly.m_Vehicle), bIsStationaryCar, bIsParkedCar, bShouldGiveWarning, &avoidanceInfo, 1, bPreventAddMarginForOtherCar);
#else
				bOldTestSuccess = TestDirectionAgainstSingleVehicle(obstructionData, const_cast<CVehicle*>(poly.m_Vehicle), bIsStationaryCar, bIsParkedCar, bShouldGiveWarning, avoidanceInfo, bPreventAddMarginForOtherCar);
#endif

				if (bOldTestSuccess)
				{
					bHitSomething = true;
					if (pHitEntity)
					{
						*pHitEntity = poly.m_Vehicle;
					}
				}

				continue; // The function we just called scored the obstruction, if there was one.
			}

			//const Vector3 vOtherVelocity = poly.m_Vehicle->GetAiVelocity();
			const Vector3 vOtherVelocity = HelperShouldUseRacingLogic(obstructionData.bRacing, pVeh->InheritsFromBike(), obstructionData.fCurrentSpeed, poly.m_Vehicle)
				? HelperGetOtherVehsRacingVelocity(poly.m_Vehicle)
				: poly.m_Vehicle->GetAiVelocity();
			const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_Vehicle, fDistToOtherCar, vOurVelocity, vOtherVelocity, obstructionData.fCurrentSpeed);
			if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
			{
				// dist to the other car is the dist to t value tEnter. Length of the segment is fDesiredSpeed * 1.7
				avoidanceInfo.fDistToObstruction = tEnter * obstructionData.fDesiredSpeed * fLookaheadTime;
//				avoidanceInfo.fDistToObstruction = Dist(poly.m_Vehicle->GetVehiclePosition(), vehPos).Getf();
				avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_Vehicle;
				avoidanceInfo.vVelocity = vOtherVelocity;
				Vector3 vDirToObstruction = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition() - vehPos);
				avoidanceInfo.fDirToObstruction = fwAngle::GetATanOfXY(vDirToObstruction.x, vDirToObstruction.y);
				avoidanceInfo.fDirToObstruction = fwAngle::LimitRadianAngle(avoidanceInfo.fDirToObstruction);
				avoidanceInfo.vPosition = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition());
				avoidanceInfo.fObstructionScore = fObstructionScore;
				avoidanceInfo.bIsStationaryCar = bIsStationaryCar;
				avoidanceInfo.bIsObstructedByLaw = IsObstructedByLawEnforcementVehicle(poly.m_Vehicle);
				avoidanceInfo.bIsTurningLeftAtJunction = poly.m_Vehicle->m_nVehicleFlags.bTurningLeftAtJunction || poly.m_Vehicle->m_nVehicleFlags.bIsWaitingToTurnLeft;
				avoidanceInfo.bIsHesitating = poly.m_Vehicle->m_nVehicleFlags.bIsHesitating;
				
				if (ShouldGiveWarning(pVeh, poly.m_Vehicle))
				{
					avoidanceInfo.bGiveWarning = true;
					bShouldGiveWarning = true;
					m_vCachedGiveWarningPosition = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition());
				}

				bHitSomething = true;
				if (pHitEntity)
				{
					*pHitEntity = poly.m_Vehicle;
				}
			}
		}
	}

	// Test against all circular obstructions

	// Use the coordinates of our front bumper.
	Vector3 bumperCrs = RCC_VECTOR3(obstructionData.vBumperCenter); 

	int circleCount = obstructionData.m_PedAndObjectObstructions.GetCount();
	for(int i = 0; i < circleCount; i++)
	{
		const AvoidanceTaskObstructionData::ObstructionCircle& circle = obstructionData.m_PedAndObjectObstructions[i];

		float desiredSteerAngle = SubtractAngleShorterFast(circle.m_Orientation, avoidanceInfo.fDirection);
		desiredSteerAngle = fwAngle::LimitRadianAngle(desiredSteerAngle);
//		bool bHitPed = false;
		if(ABS(desiredSteerAngle) < circle.m_HalfBlockedAngleRange)
		{
			Vector3 vOtherVelocity(Vector3::ZeroType);
			if (circle.m_ObstructionType == AvoidanceInfo::Obstruction_Ped)
			{
				vOtherVelocity = reinterpret_cast<const CPed*>(circle.m_Entity)->GetVelocity();
			}
			else if (circle.m_ObstructionType == AvoidanceInfo::Obstruction_Object)
			{
				vOtherVelocity = reinterpret_cast<const CObject*>(circle.m_Entity)->GetVelocity();
			}

			const float fObstructionScore = ScoreOneObstructionAngleIndependent(circle.m_ObstructionType, circle.m_Distance, vOurVelocity, vOtherVelocity, obstructionData.fCurrentSpeed);
			if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
			{
				avoidanceInfo.fDistToObstruction = circle.m_Distance;
				avoidanceInfo.ObstructionType = circle.m_ObstructionType;
				Vector3 vDirToObstruction = VEC3V_TO_VECTOR3(circle.m_Entity->GetTransform().GetPosition()) - bumperCrs;
				avoidanceInfo.fDirToObstruction = fwAngle::GetATanOfXY(vDirToObstruction.x, vDirToObstruction.y);
				avoidanceInfo.fDirToObstruction = fwAngle::LimitRadianAngle(avoidanceInfo.fDirToObstruction);
				avoidanceInfo.vPosition = VEC3V_TO_VECTOR3(circle.m_Entity->GetTransform().GetPosition());
				avoidanceInfo.fObstructionScore = fObstructionScore;
				avoidanceInfo.vVelocity = vOtherVelocity;
				avoidanceInfo.bIsObstructedByLaw = (
					((circle.m_ObstructionType == AvoidanceInfo::Obstruction_Ped) && IsObstructedByLawEnforcementPed((CPed*)circle.m_Entity)) ||
					((circle.m_ObstructionType == AvoidanceInfo::Obstruction_Vehicle) && IsObstructedByLawEnforcementVehicle((CVehicle*)circle.m_Entity)));
				avoidanceInfo.bIsTurningLeftAtJunction = false;
				avoidanceInfo.bIsHesitating = false;

				bHitSomething = true;
				if (pHitEntity)
				{
					*pHitEntity = (const CPhysical*)circle.m_Entity;
				}
			}
		}
	}

	if (!testRoadEdges || IsDrivingFlagSet(DF_GoOffRoadWhenAvoiding))
	{
		return bHitSomething;
	}

#if USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
	TestAndScoreLineSegmentsCrossRoadBoundariesUsingFollowRoute(obstructionData, &avoidanceInfo, 1, &bHitSomething);
#else	// USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
	// Test against road edges
	const Vector2 min2d(obstructionData.MinX, obstructionData.MinY);
	const Vector2 max2d(obstructionData.MaxX, obstructionData.MaxY);
	const float fTestDistance = min2d.Dist(max2d);
	float fDistToRoadEdge = -1.0f;
#if USE_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
	if (DoesLineSegmentCrossRoadBoundariesUsingFollowRoute(obstructionData, avoidanceInfo.fDirection, fTestDistance, fDistToRoadEdge))
#else
	if (DoesLineSegmentCrossRoadBoundariesUsingNodeList(obstructionData, avoidanceInfo.fDirection, fTestDistance, fDistToRoadEdge))
#endif 
	{
		const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_RoadEdge, fDistToRoadEdge, vOurVelocity, ORIGIN, obstructionData.fCurrentSpeed);
		if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
		{
			avoidanceInfo.fDistToObstruction = fDistToRoadEdge;
			avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_RoadEdge;
			avoidanceInfo.vVelocity = ORIGIN;
			avoidanceInfo.vPosition = ORIGIN; //TODO: return a position from DoesLineSegmentCrossRoadBoundaries
			avoidanceInfo.fDirToObstruction = avoidanceInfo.fDirection;
			avoidanceInfo.fObstructionScore = fObstructionScore;
			avoidanceInfo.bIsObstructedByLaw = false;
			avoidanceInfo.bIsWaitingToTurnLeft = false;
			bHitSomething = true;
		}
	}
#endif	// USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

	return bHitSomething;
}

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

void CTaskVehicleGoToPointWithAvoidanceAutomobile::TestAndScoreLineSegmentsAgainstVehicles(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut, bool& bShouldGiveWarning, const CPhysical** pHitEntity)
{
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, USE_HIGH_SPEED_PROJECTION, true );
	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, MAX_LOOKAHEAD_DIST, 80.0f, 0.0f, 200.0f, 1.0f)

	const CVehicle* pVeh = obstructionData.pVeh;
	const Vector3 vOurVelocity = pVeh->GetAiVelocity();
	const ScalarV fOurSpeed = ScalarV(vOurVelocity.Mag());

	// Test against the vehicle polys
	Vec3V vehPos(pVeh->GetVehiclePosition());
	Vec2f vehPos2d(vehPos.GetXf(), vehPos.GetYf());

	const bool bCanUseLongerProbe = pVeh->PopTypeIsMission() || pVeh->m_nVehicleFlags.bIsLawEnforcementVehicle || (pVeh->GetDriver() && pVeh->GetDriver()->PopTypeIsMission());
	const float fLookaheadTime = obstructionData.GetAvoidanceLookaheadTime(bCanUseLongerProbe);

	// The car might be stopped... so the desired speed might not actually be achievable in fLookaheadTime.
	// Lets do a rough approximation of acceleration to rein in the lookahead distance to something more reasonable
	// Useful upper bound? - supercars go 0-60 in 2.5s (!), which is an acceleration of about 10m/s^2. 
	//TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fMaxAccelerationForLookahead, 5.0f, 0.0f, 100.0f, 0.1f);
	//const float fAchievableSpeed = Min(obstructionData.fDesiredSpeed, Max(0.0f, obstructionData.fCurrentSpeed + (s_fMaxAccelerationForLookahead * fLookaheadTime)));
	//const float fLookaheadDistance = obstructionData.fDesiredSpeed * fLookaheadTime;
	//obstructionData.fLookaheadDistance = fLookaheadDistance;
	//obstructionData.fAchievableDistance = fAchievableSpeed * fLookaheadTime;

	Assert(numDirections <= kMaxObstructionProbeDirections);
	static const int kMaxVecs = kMaxObstructionProbeDirections/4;
	CompileTimeAssert(kMaxVecs*4 == kMaxObstructionProbeDirections);
	Vec4V testDeltaXArrayVecs[kMaxVecs];
	Vec4V testDeltaYArrayVecs[kMaxVecs];
	Vec4V distToIsectArrayVecs[kMaxVecs];
	VecBoolV hitArrayVecs[kMaxVecs];
	//VecBoolV tooCloseArrayVecs[kMaxVecs];

	const ScalarV desiredSpeedV = LoadScalar32IntoScalarV(obstructionData.fDesiredSpeed);
	const ScalarV lookAheadTimeV(fLookaheadTime);
	const ScalarV desiredLookAheadDistV = Scale(desiredSpeedV, lookAheadTimeV);
	const ScalarV lookAheadDistV = rage::Min(desiredLookAheadDistV, ScalarV(MAX_LOOKAHEAD_DIST));

	// Loop over the avoidance objects and initialize testDelta[X/Y]ArrayVecs.
	const int lastDir = numDirections - 1;
	for(int j = 0, k = 0; j < numDirections; j += 4, k++)
	{
		// Note: the Min() stuff here is a bit messy, but reading past the array passed in
		// by the user would be a bit risky.
		const AvoidanceInfo& avoidanceInfo0 = avoidanceInfoArray[j];
		const AvoidanceInfo& avoidanceInfo1 = avoidanceInfoArray[Min(j + 1, lastDir)];
		const AvoidanceInfo& avoidanceInfo2 = avoidanceInfoArray[Min(j + 2, lastDir)];
		const AvoidanceInfo& avoidanceInfo3 = avoidanceInfoArray[Min(j + 3, lastDir)];

		// Load up the four directions in vector registers.
		const ScalarV dir0V = LoadScalar32IntoScalarV(avoidanceInfo0.fDirection);
		const ScalarV dir1V = LoadScalar32IntoScalarV(avoidanceInfo1.fDirection);
		const ScalarV dir2V = LoadScalar32IntoScalarV(avoidanceInfo2.fDirection);
		const ScalarV dir3V = LoadScalar32IntoScalarV(avoidanceInfo3.fDirection);

		// Put them together in one vector, and compute the angle we want to compute cos and sin for.
		const Vec4V dirV(dir0V, dir1V, dir2V, dir3V);

		// Compute the sin and cos values of the four angles.
		Vec4V sinV, cosV;
		SinAndCos(sinV, cosV, dirV);

		const Vec4V testDirXV = Scale(cosV, lookAheadDistV);
		const Vec4V testDirYV = Scale(sinV, lookAheadDistV);

		// Store the probe deltas.
		testDeltaXArrayVecs[k] = testDirXV;
		testDeltaYArrayVecs[k] = testDirYV;
	}

	const int numVecs = (numDirections + 3)/4;

	// Test4x2DSegVsPolygon() needs an array of Vec2V's, not Vec2f, so we have to convert.
	// This array provides the space to do so. We probably don't want to store them as Vec2V's
	// due to the memory overhead, though it might be an option if this is just data that exists
	// temporarily.
	Vec2V vertsVecArray[AvoidanceTaskObstructionData::ObstructionPoly::MAX_OBSTRUCTION_POLY_SIZE];

	// Loop over the polygons (representing vehicles).
	const int polyCount = obstructionData.m_VehicleObstructions.GetCount();
	for(int i = 0; i < polyCount; i++)
	{
		Assert(numDirections <= kMaxObstructionProbeDirections);

		const AvoidanceTaskObstructionData::ObstructionPoly& poly = obstructionData.m_VehicleObstructions[i];

		// Convert the vertices to Vec2V format.
		const int numVerts = poly.m_Count;
		Assert(numVerts <= AvoidanceTaskObstructionData::ObstructionPoly::MAX_OBSTRUCTION_POLY_SIZE);
		for(int q = 0; q < numVerts; q++)
		{
			vertsVecArray[q] = Vec2V(poly.m_Verts[q].GetX(), poly.m_Verts[q].GetY());
		}

		// Splat the vehicle position.
		const Vec4V vehPos2dXV(vehPos.GetX());
		const Vec4V vehPos2dYV(vehPos.GetY());

		// We will keep track of if we hit anything, and if any of
		// the intersections were too close to be useful.
		VecBoolV hitAnything4V(V_F_F_F_F);
		VecBoolV anyTooClose4V(V_F_F_F_F);

		// Could also keep track of if all intersections are too close:
#if __ASSERT
		//VecBoolV allTooClose4V(V_T_T_T_T);
#endif

		const ScalarV tooCloseThresholdV(V_FLT_EPSILON);

		spdAABB tempBox;
		const spdAABB& boundBox = poly.m_Vehicle->GetLocalSpaceBoundBox(tempBox);
		const ScalarV passByThresholdVSqr(16.0f + boundBox.GetMax().GetYf() * boundBox.GetMax().GetYf());

		const Vec4V vOtherCarPos2DXV(poly.m_Vehicle->GetVehiclePosition().GetX());
		const Vec4V vOtherCarPos2DYV(poly.m_Vehicle->GetVehiclePosition().GetY());
		const Vec3V vOtherCarVel = VECTOR3_TO_VEC3V(poly.m_Vehicle->GetVelocity());

		Vector3 vOtherCarVelNorm = poly.m_Vehicle->GetVelocity();
		vOtherCarVelNorm.Normalize();
		Vector3 vOurVelNorm = vOurVelocity;
		vOurVelNorm.Normalize();

		float fVelDot = vOurVelNorm.Dot(vOtherCarVelNorm);
		//SPARR; limit this to chasing vehicles for now as don't want to risk affecting everything else
		bool bDoHighSpeedProjection = USE_HIGH_SPEED_PROJECTION && pVeh->GetIntelligence()->GetAvoidanceCache().m_pTarget != NULL && 
										pVeh->PopTypeIsMission() && IsGreaterThanAll(fOurSpeed, ScalarV(20.0f)) && fVelDot < 0.8f;
		ScalarV fIntersectRangeScalar = ScalarV(RampValue(Abs(fVelDot), 0.5f, 0.8f, 1.0f, 2.0f));

		for(int k = 0; k < numVecs; k++)
		{
			// Get coordinates of four 2D segments (directions) to test.
			const Vec4V testDeltaXV = testDeltaXArrayVecs[k];
			const Vec4V testDeltaYV = testDeltaYArrayVecs[k];

			// Note: Test4x2DSegVsPolygon() actually has to subtract these again, so there is an opportunity
			// to optimize that here.
			const Vec4V segEndXV = Add(vehPos2dXV, testDeltaXV);
			const Vec4V segEndYV = Add(vehPos2dYV, testDeltaYV);

			Vec4V tEnterV, tLeaveV;
			const VecBoolV hitV = geom2D::Test4x2DSegVsPolygon(tEnterV, tLeaveV, vehPos2dXV, vehPos2dYV, segEndXV, segEndYV, &vertsVecArray[0], poly.m_Count);

			// Compute the distance to the intersections by multiplying the T values by
			// the lookahead distance, which is the length of the segment. This only gives us something meaningful
			// for the ones that are hits, but we check that elsewhere.
			const Vec4V distToIsectsV = Scale(tEnterV, lookAheadDistV);
			VecBoolV hitWithinRangeV = hitV;
			if(bDoHighSpeedProjection)
			{
				//compute positions at intersect time
				const Vec4V fTimeToIsect = distToIsectsV / fOurSpeed;

				const Vec4V carFuturePosXV = Add(vehPos2dXV, Scale(testDeltaXV, tEnterV));
				const Vec4V carFuturePosYV = Add(vehPos2dYV, Scale(testDeltaYV, tEnterV));

				const Vec4V otherCarFuturePosXV = Add(vOtherCarPos2DXV, Scale(vOtherCarVel.GetX(), fTimeToIsect));
				const Vec4V otherCarFuturePosYV = Add(vOtherCarPos2DYV, Scale(vOtherCarVel.GetY(), fTimeToIsect));

				const Vec4V fDistAtFutureIsectXV = otherCarFuturePosXV - carFuturePosXV;
				const Vec4V fDistAtFutureIsectYV = otherCarFuturePosYV - carFuturePosYV;

				const Vec4V fSumXYDistsSqr = square(fDistAtFutureIsectXV) + square(fDistAtFutureIsectYV);
				const VecBoolV bOutsideDistRange = IsLessThan(fSumXYDistsSqr, Scale(Vec4V(passByThresholdVSqr), fIntersectRangeScalar));

				//ignore hits that are outside our intersect range
				hitWithinRangeV = And(bOutsideDistRange, hitV);
			}

			// Check if the distance is within the "too close" threshold, if it exists.
			const VecBoolV tooCloseNoHitCheckV = IsLessThan(distToIsectsV, Vec4V(tooCloseThresholdV));

			// Mask out anything that we thought may be too close, but wasn't actually a hit.
			const VecBoolV tooCloseV = And(tooCloseNoHitCheckV, hitWithinRangeV);

#if __ASSERT
			//allTooClose4V = And(allTooClose4V, Or(tooCloseNoHitCheckV, InvertBits(hitV)));
#endif

			// OR together our findings.
			anyTooClose4V = Or(anyTooClose4V, tooCloseV);
			hitAnything4V = Or(hitAnything4V, hitWithinRangeV);

			// Store the distance and hit status in arrays.
			distToIsectArrayVecs[k] = distToIsectsV;
			hitArrayVecs[k] = hitWithinRangeV;
		}

		// Check if we hit anything at all. If we didn't, there is nothing more to do with
		// this vehicle.
		if(IsFalseAll(hitAnything4V))
		{
			continue;
		}

		const bool bIsStationaryCar = IsThisAStationaryCar(poly.m_Vehicle);

		bool anyDirHit = false;

		if(!IsFalseAll(anyTooClose4V))
		{
			// If one of the directions start inside of the polygon, they probably all do,
			// so this would probably hold up:
			//	Assert(IsTrueAll(allTooClose4V));

			// Problem! tEnter is actually inside the obstruction poly, making us consider every direction obstructed.
			// This is not helpful. So do the old test that lets this car move out of the way of the obstruction car
			// before the obstruction car actually obstructs.

			bool bIsParkedCar = IsThisAParkedCar(poly.m_Vehicle);

			//if we failed the first test for being inside the poly, don't add
			//any margin to the other vehicle's bb this time
			const bool bPreventAddMarginForOtherVehicle = true;

			TestDirectionsAgainstSingleVehicle(obstructionData, const_cast<CVehicle*>(poly.m_Vehicle), bIsStationaryCar, bIsParkedCar, bShouldGiveWarning, avoidanceInfoArray, numDirections, bPreventAddMarginForOtherVehicle);

			// We are basically assuming here that if any of the intersections were too close (zero length),
			// they all are, and we should use the old algorithm in all directions. Also, we didn't check
			// which intersections were valid, because they should all be since all the directions started
			// inside the polygon.
			continue;
		}

		// Compute some stuff about the obstruction and its relation to us, independently of the test direction.
		const bool giveWarningIfHit = ShouldGiveWarning(pVeh, poly.m_Vehicle);
		const Vector3 vOtherVelocity = HelperShouldUseRacingLogic(obstructionData.bRacing, pVeh->InheritsFromBike(), obstructionData.fCurrentSpeed, poly.m_Vehicle)
			? HelperGetOtherVehsRacingVelocity(poly.m_Vehicle)
			: poly.m_Vehicle->GetAiVelocity();
		const Vector3 vDirToObstruction = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition() - vehPos);
		const float dirToObstruction = fwAngle::LimitRadianAngle(fwAngle::GetATanOfXY(vDirToObstruction.x, vDirToObstruction.y));

		// Reinterpret the hit/distance arrays to work on them without vectors.
		const u32 *hitArray = (const u32*)hitArrayVecs;
		const float* distToIsectArray = (const float*)distToIsectArrayVecs;
		for(int j = 0; j < numDirections; j++)
		{
			// Check if this direction was a hit.
			if(hitArray[j])
			{
				AvoidanceInfo& avoidanceInfo = avoidanceInfoArray[j];

				// Score the obstruction for this direction. Note that we pass in the distance to the intersection
				// here, so this score will not be the same for all directions.
				const float fDistToOtherCar = distToIsectArray[j];
				const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_Vehicle, fDistToOtherCar, vOurVelocity, vOtherVelocity, obstructionData.fCurrentSpeed);
				if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
				{
					// dist to the other car is the dist to t value tEnter. Length of the segment is fDesiredSpeed * 1.7
					avoidanceInfo.fDistToObstruction = fDistToOtherCar;
					avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_Vehicle;
					avoidanceInfo.vVelocity = vOtherVelocity;
					avoidanceInfo.fDirToObstruction = dirToObstruction;
					avoidanceInfo.vPosition = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition());
					avoidanceInfo.fObstructionScore = fObstructionScore;
					avoidanceInfo.bIsStationaryCar = bIsStationaryCar;
					avoidanceInfo.bIsObstructedByLaw = IsObstructedByLawEnforcementVehicle(poly.m_Vehicle);
					avoidanceInfo.bIsTurningLeftAtJunction = poly.m_Vehicle->m_nVehicleFlags.bTurningLeftAtJunction || poly.m_Vehicle->m_nVehicleFlags.bIsWaitingToTurnLeft;
					avoidanceInfo.bIsHesitating = poly.m_Vehicle->m_nVehicleFlags.bIsHesitating;

					anyDirHit = true;
					if (giveWarningIfHit)
					{
						avoidanceInfo.bGiveWarning = true;
					}

					// Record that we hit something.
					bHitSomethingArrayInOut[j] = true;

					// Store the entity. This really only makes sense if we have just one direction.
					if(pHitEntity)
					{
						*pHitEntity = poly.m_Vehicle;
					}
				}
			}
		}

		// If we had any intersections which beat the previous scores, do some warning-related stuff.
		// No need to do this for each direction.
		if(anyDirHit)
		{
			if (giveWarningIfHit)
			{
				bShouldGiveWarning = true;
				m_vCachedGiveWarningPosition = VEC3V_TO_VECTOR3(poly.m_Vehicle->GetVehiclePosition());
			}
		}
	}
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::TestAndScoreLineSegmentsAgainstCylindricalObstructions(
		const AvoidanceTaskObstructionData& obstructionData,
		AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut,
		const CPhysical** pHitEntity) const
{
	const CVehicle* pVeh = obstructionData.pVeh;
	const Vec3V ourVelocityV = VECTOR3_TO_VEC3V(pVeh->GetAiVelocity());

	// Test against all circular obstructions

	// Use the coordinates of our front bumper.
	const Vec3V bumperCrsV = obstructionData.vBumperCenter; 

	const int circleCount = obstructionData.m_PedAndObjectObstructions.GetCount();

	// We will need two arrays of vectors, one for the directions to test in,
	// and one for the results.
	Assert(numDirections <= kMaxObstructionProbeDirections);
	static const int kMaxVecs = kMaxObstructionProbeDirections/4;
	CompileTimeAssert(kMaxVecs*4 == kMaxObstructionProbeDirections);
	Vec4V avoidanceInfoDirVecs[kMaxVecs];
	VecBoolV hitThisObstructionVecs[kMaxVecs];

	// Look at four directions at a time, and stuff the angles into the avoidanceInfoDirVecs array.
	const int lastDir = numDirections - 1;
	for(int j = 0, k = 0; j < numDirections; j += 4, k++)
	{
		const AvoidanceInfo& avoidanceInfo0 = avoidanceInfoArray[j];
		const AvoidanceInfo& avoidanceInfo1 = avoidanceInfoArray[Min(j + 1, lastDir)];
		const AvoidanceInfo& avoidanceInfo2 = avoidanceInfoArray[Min(j + 2, lastDir)];
		const AvoidanceInfo& avoidanceInfo3 = avoidanceInfoArray[Min(j + 3, lastDir)];

		// Load up the four directions in vector registers.
		// TODO: Should consider stuffing this in the W component of the position or velocity,
		// so we can load them as aligned vectors.
		const ScalarV heading0V = LoadScalar32IntoScalarV(avoidanceInfo0.fDirection);
		const ScalarV heading1V = LoadScalar32IntoScalarV(avoidanceInfo1.fDirection);
		const ScalarV heading2V = LoadScalar32IntoScalarV(avoidanceInfo2.fDirection);
		const ScalarV heading3V = LoadScalar32IntoScalarV(avoidanceInfo3.fDirection);

		// Store the search positions.
		const Vec4V headingV(heading0V, heading1V, heading2V, heading3V);
		avoidanceInfoDirVecs[k] = headingV;
	}

	const int numVecs = (numDirections + 3)/4;

	// Some constants that we will need.
	const Vec4V piV(V_PI);
	const Vec4V minPiV(V_NEG_PI);
	const Vec4V twoPiV(V_TWO_PI);

	// Loop over the circular obstructions one by one.
	for(int i = 0; i < circleCount; i++)
	{
		// Grab the information from the obstruction that we need to do the tests.
		const AvoidanceTaskObstructionData::ObstructionCircle& circle = obstructionData.m_PedAndObjectObstructions[i];
		const Vec4V circleOrientationV(LoadScalar32IntoScalarV(circle.m_Orientation));
		const Vec4V halfBlockedAngleRangeV(LoadScalar32IntoScalarV(circle.m_HalfBlockedAngleRange));

		// Loop over the vectors containing avoidance directions, and test four directions at a
		// time against this obstruction.
		VecBoolV hitAny4V(V_FALSE);
		for(int k = 0; k < numVecs; k++)
		{
			const Vec4V dirV = avoidanceInfoDirVecs[k];

			// We basically want to do this test, for each direction:
			//	float desiredSteerAngle = SubtractAngleShorterFast(circle.m_Orientation, avoidanceInfo.fDirection);
			//	desiredSteerAngle = fwAngle::LimitRadianAngle(desiredSteerAngle);
			//	if(ABS(desiredSteerAngle) < circle.m_HalfBlockedAngleRange)

			// First, subtract the angle between the circle and the avoidance direction,
			// and add/subtract two times PI.
			const Vec4V angleV = Subtract(circleOrientationV, dirV);
			const Vec4V angleMinTwoPiV = Subtract(angleV, twoPiV);
			const Vec4V anglePlusTwoPiV = Add(angleV, twoPiV);

			// Next, use vector selection to wrap the angle to the -PI..PI range.
			// Note: looks like circle.m_Orientation is currently in the 0..2*PI range,
			// but the avoidance angles should be -PI..PI. The difference between the angles
			// must still be within the -3*PI..3*PI range, so should be OK to do it like this:
			//	if (angle>PI) return angle-2.0f*PI;
			//	if (angle<-PI) return angle+2.0f*PI;
			const Vec4V angleClampV = SelectFT(IsGreaterThan(angleV, piV), angleV, angleMinTwoPiV);
			const Vec4V desiredSteerAngleV = SelectFT(IsLessThan(angleV, minPiV), angleClampV, anglePlusTwoPiV);

			// This should hold up.
			//	Assert(desiredSteerAngleV.GetXf() >= -PI && desiredSteerAngleV.GetXf() <= PI);
			//	Assert(desiredSteerAngleV.GetYf() >= -PI && desiredSteerAngleV.GetYf() <= PI);
			//	Assert(desiredSteerAngleV.GetZf() >= -PI && desiredSteerAngleV.GetZf() <= PI);
			//	Assert(desiredSteerAngleV.GetWf() >= -PI && desiredSteerAngleV.GetWf() <= PI);

			// Check if the absolute value of the angle is within the range.
			const Vec4V absDesiredSteerAngleV = Abs(desiredSteerAngleV);
			const VecBoolV hitV = IsLessThan(absDesiredSteerAngleV, halfBlockedAngleRangeV);

			// OR together all the results, between vectors. Note that we don't
			// OR together between the components here, i.e. the X component of this will
			// be for direction 0, 4, 8, etc, Y would be 1, 5, 9, and so on.
			hitAny4V = Or(hitV, hitAny4V);

			// Store the results.
			hitThisObstructionVecs[k] = hitV;
		}

		// Check if there were any hits at all.
		if(!IsFalseAll(hitAny4V))
		{
			// Compute the score and other things that are constant between all the avoidance directions.
			Vec3V otherVelocityV(V_ZERO);
			bool obstructedByLaw = false;
			AvoidanceInfo::eObstructionType obstructionType = circle.m_ObstructionType;
			if (obstructionType == AvoidanceInfo::Obstruction_Ped)
			{
				otherVelocityV = VECTOR3_TO_VEC3V(reinterpret_cast<const CPed*>(circle.m_Entity)->GetVelocity());
				obstructedByLaw = IsObstructedByLawEnforcementPed((CPed*)circle.m_Entity);
			}
			else if (obstructionType == AvoidanceInfo::Obstruction_Object)
			{
				otherVelocityV = VECTOR3_TO_VEC3V(reinterpret_cast<const CObject*>(circle.m_Entity)->GetVelocity());
			}
			else if (obstructionType == AvoidanceInfo::Obstruction_Vehicle)		// Can this actually happen here?
			{
				obstructedByLaw = IsObstructedByLawEnforcementVehicle((CVehicle*)circle.m_Entity);
			}
			const float fObstructionScore = ScoreOneObstructionAngleIndependent(circle.m_ObstructionType, circle.m_Distance,
					VEC3V_TO_VECTOR3(ourVelocityV), VEC3V_TO_VECTOR3(otherVelocityV), obstructionData.fCurrentSpeed);
			const Vec3V entityPosV = circle.m_Entity->GetTransform().GetPosition();
			const Vec3V dirToObstructionV = Subtract(entityPosV, bumperCrsV);
			const float dirToObstruction = fwAngle::LimitRadianAngle(fwAngle::GetATanOfXY(dirToObstructionV.GetXf(), dirToObstructionV.GetYf()));
			const float distToObstruction = circle.m_Distance;

			// Loop over the result array, reinterpreting it as a u32 array of boolean values.
			const u32* bHitThisObstruction = reinterpret_cast<const u32*>(hitThisObstructionVecs);
			for(int j = 0; j < numDirections; j++)
			{
				// Check if this direction hit the obstruction.
				if(!bHitThisObstruction[j])
				{
					continue;
				}

				// Update the avoidance results, if this score is better than what we had before.
				AvoidanceInfo& avoidanceInfo = avoidanceInfoArray[j];
				if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
				{
					avoidanceInfo.fDistToObstruction = distToObstruction;
					avoidanceInfo.ObstructionType = obstructionType;
					avoidanceInfo.fDirToObstruction = dirToObstruction;
					avoidanceInfo.vPosition = VEC3V_TO_VECTOR3(entityPosV);
					avoidanceInfo.fObstructionScore = fObstructionScore;
					avoidanceInfo.vVelocity = VEC3V_TO_VECTOR3(otherVelocityV);
					avoidanceInfo.bIsObstructedByLaw = obstructedByLaw;
					avoidanceInfo.bIsTurningLeftAtJunction = false;
					avoidanceInfo.bIsHesitating = false;

					bHitSomethingArrayInOut[j] = true;

					if(pHitEntity)
					{
						*pHitEntity = (const CPhysical*)circle.m_Entity;
					}
				}
			}
		}
	}
}

#endif	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

#if USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

void CTaskVehicleGoToPointWithAvoidanceAutomobile::TestAndScoreLineSegmentsCrossRoadBoundariesUsingFollowRoute(const AvoidanceTaskObstructionData& obstructionData,
		 AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut) const
{
	const Vector2 min2d(obstructionData.MinX, obstructionData.MinY);
	const Vector2 max2d(obstructionData.MaxX, obstructionData.MaxY);
	const float fTestDistance = min2d.Dist(max2d);

	// We need a temporary vector-aligned array for distances to road edges for all the directions,
	// since we will do the tests in parallel.
	Assert(numDirections <= kMaxObstructionProbeDirections);
	static const int kMaxVecs = kMaxObstructionProbeDirections/4;
	CompileTimeAssert(kMaxVecs*4 == kMaxObstructionProbeDirections);

	Vec4V distToRoadEdgeVecArray[kMaxVecs];

#if !USE_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
# error "Only USE_FOLLOWROUTE_EDGES_FOR_AVOIDANCE is supported now."
#endif 

	// Perform the tests. This will just populate distToRoadEdgeVecArray[], either with -1 or
	// the distance to the road edge.
	if(!DoLineSegmentsCrossRoadBoundariesUsingFollowRoute(obstructionData, avoidanceInfoArray, numDirections, fTestDistance, distToRoadEdgeVecArray))
	{
		// In this case, DoLineSegmentsCrossRoadBoundariesUsingFollowRoute() bailed out before looking
		// for any actual intersections, so we should not do the stuff below.
		return;
	}

	const CVehicle* pVeh = obstructionData.pVeh;
	const Vec3V ourVelocityV = VECTOR3_TO_VEC3V(pVeh->GetAiVelocity());

	// We now need to access the array as floats, but also as integers for improved branching performance.
	const s32* distToRoadEdgeAsInt = (const s32*)&distToRoadEdgeVecArray[0];
	const float* distToRoadEdgeAsFloat = (const float*)&distToRoadEdgeVecArray[0];

	// Loop over the directions and update the scores.
	// Note: there would probably be some opportunities for vectorizing parts of this stuff too.
	for(int j = 0; j < numDirections; j++)
	{
		AvoidanceInfo& avoidanceInfo = avoidanceInfoArray[j];

		// Any non-negative valid float should also be a non-negative s32.
		if(distToRoadEdgeAsInt[j] >= 0)
		{
			// We hit the road edge, update the score if it's better than previous scores.
			const float fDistToRoadEdge = distToRoadEdgeAsFloat[j];
			const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_RoadEdge, fDistToRoadEdge, RCC_VECTOR3(ourVelocityV), ORIGIN, obstructionData.fCurrentSpeed);
			if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
			{
				avoidanceInfo.fDistToObstruction = fDistToRoadEdge;
				avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_RoadEdge;
				avoidanceInfo.vVelocity = ORIGIN;
				avoidanceInfo.vPosition = ORIGIN; //TODO: return a position from DoesLineSegmentCrossRoadBoundaries
				avoidanceInfo.fDirToObstruction = avoidanceInfo.fDirection;
				avoidanceInfo.fObstructionScore = fObstructionScore;
				avoidanceInfo.bIsObstructedByLaw = false;
				avoidanceInfo.bIsTurningLeftAtJunction = false;
				avoidanceInfo.bIsHesitating = false;

				bHitSomethingArrayInOut[j] = true;
			}
		}
	}
}

#endif	// USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

float CTaskVehicleGoToPointWithAvoidanceAutomobile::ScoreOneObstructionAngleIndependent(const AvoidanceInfo::eObstructionType obstructionType, const float fDistToObstruction, const Vector3& vOurVelocity, const Vector3& vTheirVelocity, const float fOurCurrentSpeed) const
{
	float fScore = 0.0f;

	//if this probe hit anything, weight it based on the type and distance
	//if (AvoidanceInfoElement.HasObstruction())
	{
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fDistanceScoreMultiplier, 100.0, -1000.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fVehicleScoreMultiplier, 30.0, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fPedScoreMultiplier, 30.0, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fObjectScoreMultiplier, 30.0, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fRoadEdgeScoreMultiplier, 30.0, -100.0f, 100.0f, 0.1f);
		//static dev_float s_fDistanceScoreMultiplier = 100.0f;
		//static dev_float s_fVehicleScoreMultiplier = 15.0f;
		//static dev_float s_fPedScoreMultiplier = 8.0f;
		//static dev_float s_fObjectScoreMultiplier = 15.0f;
		//static dev_float s_fRoadEdgeScoreMultiplier = 15.0f;
		//static dev_float s_fRelativeVelocityScoreMultiplier = -1.0f;

		//care more about obstructions that are closer
		if (fDistToObstruction >= SMALL_FLOAT)
		{
			fScore += s_fDistanceScoreMultiplier / (fDistToObstruction * fDistToObstruction);
		}

		//I guess these aren't really multipliers since they aren't being added in
		switch (obstructionType)
		{
		case AvoidanceInfo::Obstruction_Vehicle:
			fScore += s_fVehicleScoreMultiplier;
			break;
		case AvoidanceInfo::Obstruction_Ped:
			fScore += s_fPedScoreMultiplier;
			break;
		case AvoidanceInfo::Obstruction_Object:
			fScore += s_fObjectScoreMultiplier;
			break;
		case AvoidanceInfo::Obstruction_RoadEdge:
			//if we're willing to stop, REALLY prefer not going off road
			fScore += s_fRoadEdgeScoreMultiplier;
			break;
		default:
			Assertf(0, "Somehow we detected an obstruction without a type :(");
			break;
		}

		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fRelativeVelocityScoreMultiplier, -1.0, -100.0f, 100.0f, 0.1f);
		if (obstructionType == AvoidanceInfo::Obstruction_Vehicle)
		{
			//subtract some from the weight if this guy is moving in the same direction as us

			//map his velocity onto ours
			Vector3 vOurVelocityNormalized = vOurVelocity;
			vOurVelocityNormalized.z = 0.0f;
			vOurVelocityNormalized.NormalizeSafe(ORIGIN);
			Vector3 vTheirVelocityFlat = vTheirVelocity;
			vTheirVelocityFlat.z = 0.0f;
			const float fSpeedInOurDirection = vTheirVelocityFlat.Dot(vOurVelocityNormalized);

			//only do this if our velocities are generally in the same direction--
			//we only want to make cars moving in the same direction less of an obstruction,
			//not make the ones moving toward us bigger
			if (fSpeedInOurDirection - fOurCurrentSpeed > 0.0f)
			{
				fScore += (fSpeedInOurDirection - fOurCurrentSpeed) * s_fRelativeVelocityScoreMultiplier;

				//did we make this direction more desirable than it would be if there were no obstruction there?
				//Assert(scores[i] >= fUnobstructedScore);
				//fScore = rage::Max(fScore, fUnobstructedScore);
			}
		}

		// 			if (AvoidanceInfoArray[i].bGiveWarning)
		// 			{
		// 				//add another penalty
		// 				const float fGiveWarningPenalty = s_fVehicleScoreMultiplier;
		// 				scores[i] += fGiveWarningPenalty;
		// 			}
		// 			if (bGivesWarning)
		// 			{
		// 				//angles closer to the obstruction should be weighted higher
		// 				scores[i] += fabsf(SubtractAngleShorter(AvoidanceInfoArray[i].fDirection, fDirToCloseVehicleObstruction)) * s_fRecoveryAngleWeight;
		// 			}
	}

	return fScore;
}


float CTaskVehicleGoToPointWithAvoidanceAutomobile::ScoreAvoidanceDirections( const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* AvoidanceInfoArray,
																			  const float distToOriginalObstruction)
{
	const CVehicle* pVeh = obstructionData.pVeh;
	const float fOurSpeed = pVeh->GetAiXYSpeed();

	//are all directions blocked by the same vehicle?
	const int iNumDirsNeedToBeBlocked = (s_iNumObstructionProbes*1) / 2;
	int iNumDirsActuallyBlocked = 0;
	int iNumDirsActuallyBlockedByObject = 0;
	bool bEnoughDirectionsBlockedBySameVeh = true;
	bool bEnoughDirectionsBlockedBySameObjectOrPed = true;
	bool bFoundWayOut = false;
	bool bFoundWayOutForScoring = false;
	//float fAllBlockedDist = FLT_MAX;
	Vector3 vAllBlockedPos(Vector3::ZeroType);
	float fDirToCloseVehicleObstruction = 0.0f;
	float fDirToCloseObstacleOrPedObstruction = 0.0f;
	static const ScalarV svEpsilon(V_FLT_EPSILON);
	//bool bGivesWarning = false;

	if( s_iNumObstructionProbes > 0)
	{
		if (AvoidanceInfoArray[0].ObstructionType == AvoidanceInfo::Obstruction_Vehicle)
		{
			vAllBlockedPos = AvoidanceInfoArray[0].vPosition;
			//fAllBlockedDist = AvoidanceInfoArray[i].fDistToObstruction;
			fDirToCloseVehicleObstruction = AvoidanceInfoArray[0].fDirToObstruction;
			++iNumDirsActuallyBlocked;
		}
		else
		{
			bEnoughDirectionsBlockedBySameVeh = false;
		}

		if( bEnoughDirectionsBlockedBySameVeh)
		{
			for (int i=1; i < s_iNumObstructionProbes; i++)
			{
				if (IsCloseAll(VECTOR3_TO_VEC3V(vAllBlockedPos), VECTOR3_TO_VEC3V(AvoidanceInfoArray[i].vPosition), svEpsilon))
				{
					++iNumDirsActuallyBlocked;
				}
			}
		}

		if (AvoidanceInfoArray[0].ObstructionType == AvoidanceInfo::Obstruction_Object || AvoidanceInfoArray[0].ObstructionType == AvoidanceInfo::Obstruction_Ped)
		{
			//reuse vAllBlockedPos here
			vAllBlockedPos = AvoidanceInfoArray[0].vPosition;
			fDirToCloseObstacleOrPedObstruction = AvoidanceInfoArray[0].fDirToObstruction;
			++iNumDirsActuallyBlockedByObject;
		}
		else
		{
			bEnoughDirectionsBlockedBySameObjectOrPed = false;
		}

		if( bEnoughDirectionsBlockedBySameObjectOrPed)
		{
			for (int i=1; i < s_iNumObstructionProbes; i++)
			{
				//if (AreNearlyEqual(fAllBlockedDist, AvoidanceInfoArray[i].fDistToObstruction))
				if (IsCloseAll(VECTOR3_TO_VEC3V(vAllBlockedPos), VECTOR3_TO_VEC3V(AvoidanceInfoArray[i].vPosition), svEpsilon))
				{
					++iNumDirsActuallyBlockedByObject;
				}
			}
		}
	}

	if (iNumDirsActuallyBlocked < iNumDirsNeedToBeBlocked)
	{
		bEnoughDirectionsBlockedBySameVeh = false;
	}
	if (iNumDirsActuallyBlockedByObject < iNumDirsNeedToBeBlocked)
	{
		bEnoughDirectionsBlockedBySameObjectOrPed = false;
	}

	if (iNumDirsActuallyBlocked >= s_iNumObstructionProbes
		|| iNumDirsActuallyBlockedByObject >= s_iNumObstructionProbes)
	{
		m_ObstructionInformation.m_uFlags.SetFlag(ObstructionInformation::IsCompletelyObstructedBySingleObstruction);
	}

	//unfortunately, the closest thing we have to a key in this data structure is its vPosition
	const Vec3V vBumperCoords = obstructionData.vBumperCenter;
	float fFurthestDistToClosestRealObstruction = distToOriginalObstruction;
	const float fDistSqrToCenterOfClosestRealObstruction = DistSquared(vBumperCoords, obstructionData.vPosOfOriginalObstruction).Getf();
	for (int i=0; i < s_iNumObstructionProbes; i++)
	{
		if (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Vehicle
			|| AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Object
			|| AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Ped)
		{
			const float fDistSqrToThisRealObstruction = DistSquared(vBumperCoords, VECTOR3_TO_VEC3V(AvoidanceInfoArray[i].vPosition)).Getf();
			if (AreNearlyEqual(fDistSqrToCenterOfClosestRealObstruction, fDistSqrToThisRealObstruction))
			{
				if (AvoidanceInfoArray[i].fDistToObstruction > fFurthestDistToClosestRealObstruction)
				{
					fFurthestDistToClosestRealObstruction = AvoidanceInfoArray[i].fDistToObstruction;
				}
			}
		}
	}

	static dev_float fCloseVehiclePadding = 3.0f;
	fFurthestDistToClosestRealObstruction += fCloseVehiclePadding;

	//look for a way out
	for (int i=0; i < s_iNumObstructionProbes; i++)
	{
		const bool bWayOutConditionsForScoring = !AvoidanceInfoArray[i].HasObstruction() 
			|| (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_RoadEdge 
			&& AvoidanceInfoArray[i].fDistToObstruction > fFurthestDistToClosestRealObstruction);
		if (bWayOutConditionsForScoring
			|| (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Vehicle
				&& (AvoidanceInfoArray[i].vVelocity.Mag2() > 1.0f || AvoidanceInfoArray[i].bIsHesitating))
			)
		{
			bFoundWayOut = true;

			if (bWayOutConditionsForScoring)
			{
				bFoundWayOutForScoring = true;
				break;
			}
		}
	}

	//our velocity relative to the other
	//Vector3 ourVelocityNormalized = pVeh->GetAiVelocity();
	//ourVelocityNormalized.NormalizeSafe(ORIGIN);
	const float fSteeringAngle = pVeh->GetSteerAngle();

	//look ahead a bit further on the road from where our obstruction is
	//if we're turning to the right or left a significant amount, prefer that direction
	float fRightness = 0.0f;
	if (obstructionData.bIsCurrentlyOnJunction 
		&& AvoidanceInfoArray[0].HasObstruction()
		/*&& AvoidanceInfoArray[0].ObstructionType != AvoidanceInfo::Obstruction_RoadEdge*/)
	{
		const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();
		if (pFollowRoute && pFollowRoute->GetCurrentTargetPoint() > 0)
		{
			const int iPointAheadOnRoute = rage::Min(pFollowRoute->FindClosestPointToPosition(obstructionData.vPosOfOriginalObstruction, pFollowRoute->GetCurrentTargetPoint()) + 2, (pFollowRoute->GetNumPoints()-1));
			Assert(iPointAheadOnRoute >= 0);

			Vector3 vDirectionOfCurrentSegment = VEC3V_TO_VECTOR3(pFollowRoute->GetRoutePoints()[pFollowRoute->GetCurrentTargetPoint()].GetPosition() - pFollowRoute->GetRoutePoints()[pFollowRoute->GetCurrentTargetPoint()-1].GetPosition());
			vDirectionOfCurrentSegment.z = 0.0f;
			vDirectionOfCurrentSegment.NormalizeSafe(XAXIS);
			Vector3 vDirectionToPointAhead = VEC3V_TO_VECTOR3(pFollowRoute->GetRoutePoints()[iPointAheadOnRoute].GetPosition() - pFollowRoute->GetRoutePoints()[pFollowRoute->GetCurrentTargetPoint()-1].GetPosition());
			vDirectionToPointAhead.z = 0.0f;

			//don't need or want to normalize vDirectionToPointAhead--we want to know how far it is along the perp of vDirectionOfCurrentSegment
			Vector3 vCurrentSegmentRight(vDirectionOfCurrentSegment.y, -vDirectionOfCurrentSegment.x, 0.0f);

			//grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()) + (vCurrentSegmentRight * 5.0f), Color_orange );

			fRightness = vCurrentSegmentRight.Dot(vDirectionToPointAhead);	
		}
	}

	//get the segment length
	const Vector2 min2d(obstructionData.MinX, obstructionData.MinY);
	const Vector2 max2d(obstructionData.MaxX, obstructionData.MaxY);
	const float fTestDistance = min2d.Dist(max2d);

	const bool bIsFacingAVehicleTurningLeft = AvoidanceInfoArray[0].bIsTurningLeftAtJunction 
		|| AvoidanceInfoArray[1].bIsTurningLeftAtJunction 
		|| AvoidanceInfoArray[2].bIsTurningLeftAtJunction;

	//now weight all the factors and chose the least-obstructed direction
	//prefer smaller angles
	//prefer longer distances
	//prefer peds over vehicles over objects
	//in roughly that order
	atRangeArray<float, s_iNumObstructionProbes> scores;
	float fBestScore = FLT_MAX;
	int iBestIndex = -1;
	for (int i = 0; i < s_iNumObstructionProbes; i++)
	{
		//low score wins 
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fAngleScoreMultiplier, 0.5f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fLastDirectionScoreMultiplier, 0.75f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fDestinationAngleScoreMultiplier, 50.0f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fDestinationAngleScoreMultiplierStopped, 100.0f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fDestinationAngleScoreMultiplierForObstructions, 1.0f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fRouteAheadDirectionScoreMultiplier, 10.0f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fCloserThanCloseTargetScoreMultiplier, 15.0f, -100.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fSingleTrackRoadLeftSideMultiplier, 50.0f, -100.0f, 100.0f, 0.1f);

		//initialize score to the angle difference
		//TODO:  do something with bWillStopForCars.  I'm thinking if it's set, we should weight angle more heavily
		//so we're more likely to choose a direction close to the center and slow down for it
		//weight choice of direction toward velocity, not current heading
		float fWheelOrientation = obstructionData.vehDriveOrientation + fSteeringAngle;

		Assertf(fWheelOrientation <= THREE_PI, 
			"fWheelOrientation outside of acceptable range. fWheelOrientation: %.2f vehDriveOrientation %.2f fSteeringAngle %.2f"
			, fWheelOrientation, obstructionData.vehDriveOrientation, fSteeringAngle);

		//TODO: change this back to a LimitRadianAngle. there's no reason it shoudl be out of bounds
		//but bernie encountered a crash here, and I'd really like the magdemo build to not crash
		fWheelOrientation = fwAngle::LimitRadianAngleSafe(fWheelOrientation);

		//only care about angle difference if we're moving. if we're stopped or moving really slowly,
		//we shouldn't prefer closer angles since we don't have any momentum to conserve -JM
		const float fAngleScoreMultActual =  fOurSpeed >= 0.5f ? s_fAngleScoreMultiplier : 0.0f;
		scores[i] = fabsf(SubtractAngleShorter(fWheelOrientation, AvoidanceInfoArray[i].fDirection)) * fAngleScoreMultActual;

		//give a bump to the side we didn't avoid last time
		if (i != 0 && ((m_bAvoidedRightLastFrame && i % 2 == 0)
			|| (m_bAvoidedLeftLastFrame && i % 2 != 0)))
		{
			scores[i] += s_fLastDirectionScoreMultiplier;
		}

		//if we're on a single track road, prefer avoiding to the right, so add a penalty to all probes to the left
		//if front is also blocked by road edge then add multiplier to front as well to allow target angle to be dominant when all probes blocked by road edge
		if ((obstructionData.bOnSingleTrackRoad 
			|| (obstructionData.turnDir != BIT_TURN_LEFT && (AvoidanceInfoArray[i].bIsTurningLeftAtJunction || bIsFacingAVehicleTurningLeft))) 
			&& ( i != 0 || AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_RoadEdge )
			&& i % 2 == 0)
		{
			//CVehicle::ms_debugDraw.AddSphere(pVeh->GetVehiclePosition(), 2.5f, Color_purple2, 0, 0, false);

			scores[i] += s_fSingleTrackRoadLeftSideMultiplier;
		}
		else if ((i == 0 || (i % 2) != 0) && obstructionData.turnDir == BIT_TURN_LEFT 
			&& (AvoidanceInfoArray[i].bIsTurningLeftAtJunction || bIsFacingAVehicleTurningLeft))
		{
			//CVehicle::ms_debugDraw.AddSphere(pVeh->GetVehiclePosition(), 2.5f, Color_turquoise, 0, 0, false);

			//if we are turning left, and so is the other guy, add a penalty to all probes to the right
			scores[i] += s_fSingleTrackRoadLeftSideMultiplier;
		}

		//prefer angles closer to our target direction,
		//unless they are blocked by a non-road edge obstruction
		const float fRadiansWithinToNotCareAboutTargetDir = 0.0f;
		const float fDestinationAngleMultActual = fOurSpeed >= 1.0f ? s_fDestinationAngleScoreMultiplier : s_fDestinationAngleScoreMultiplierStopped;
		const float fAngleDifferenceBetweenProbeAndDesired = rage::Max(0.0f, 
			fabsf(SubtractAngleShorter(obstructionData.fOriginalDirection, AvoidanceInfoArray[i].fDirection)) - fRadiansWithinToNotCareAboutTargetDir);
		if (!AvoidanceInfoArray[i].HasObstruction() || 
			(AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_RoadEdge 
			&& AvoidanceInfoArray[i].fDistToObstruction > fFurthestDistToClosestRealObstruction))
		{
			scores[i] += ((fAngleDifferenceBetweenProbeAndDesired * fAngleDifferenceBetweenProbeAndDesired) * fDestinationAngleMultActual);
		}
		else
		{
			//otherwise add the maximum possible value the above block could generate
			//but differentiate a little bit in order to break ties
			scores[i] += ((PI * PI) * fDestinationAngleMultActual) + 
				((fAngleDifferenceBetweenProbeAndDesired * fAngleDifferenceBetweenProbeAndDesired) * s_fDestinationAngleScoreMultiplierForObstructions);
		}
		
		//if there's a clear way ahead, add a penalty to each probe that hits something closer than
		//the closest thing we want to avoid
		if (bFoundWayOutForScoring && AvoidanceInfoArray[i].HasObstruction()
			&& obstructionData.turnDir != BIT_TURN_LEFT
			&& obstructionData.turnDir != BIT_TURN_RIGHT
			&& AvoidanceInfoArray[i].fDistToObstruction < fFurthestDistToClosestRealObstruction)
		{
			scores[i] += s_fCloserThanCloseTargetScoreMultiplier;
		}

		//if we're getting out of a deadlock, prefer sharper angles
		//don't do this at high speeds though, can cause a bad-looking swerve
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fRecoveryAngleWeight, -15.0, -100.0f, 100.0f, 0.1f);
		//static dev_float s_fRecoveryAngleWeight = -5.0f;
		const float s_fRecoveryAngleScoreMultiplier = 90.0f * DtoR * s_fRecoveryAngleWeight;
		if ((fOurSpeed <= 5.0f || m_bBumperInsideBoundsThisFrame) && (m_bAvoidingCarsUntilClear || bEnoughDirectionsBlockedBySameVeh || 
			bEnoughDirectionsBlockedBySameObjectOrPed/* || m_threePointTurnInfo.m_iNumRecentThreePointTurns >= m_threePointTurnInfo.ms_iMaxAttempts*/))
		{
			if (bEnoughDirectionsBlockedBySameVeh)
			{
				//angles closer to the obstruction should be weighted higher
				scores[i] += fabsf(SubtractAngleShorter(AvoidanceInfoArray[i].fDirection, fDirToCloseVehicleObstruction)) * s_fRecoveryAngleWeight;
			}
			else
			{
				scores[i] += fabsf(SubtractAngleShorter(obstructionData.vehDriveOrientation,AvoidanceInfoArray[i].fDirection)) * s_fRecoveryAngleScoreMultiplier;
			}


#if __BANK
			if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(pVeh->TransformIntoWorldSpace(Vector3(0.0f, pVeh->GetBoundingBoxMax().y * 0.9f, 0.0f))), 1.5f, Color_orange, 0, 0, false);
			}
#endif //BANK
		}

		//if (m_bAvoidedRightLastFrame && i % 2 == 0
		//	|| m_bAvoidedLeftLastFrame && i % 2 != 0)
		if (i != 0 && ((fRightness > 0.0f && i % 2 == 0) || (fRightness < 0.0f && i % 2 != 0)))
		{
			scores[i] += fabsf(fRightness) * s_fRouteAheadDirectionScoreMultiplier;
		}	

		//cache this off
		const float fUnobstructedScore = scores[i];

		//add in the score based on distance and type of obstruction
		//TODO: maybe move this to the top, it's just here because this is where
		//the code used to be
		scores[i] += AvoidanceInfoArray[i].fObstructionScore;

		//if there's no obstruction, initialize the score to what it would be if we
		//hit a road edge at the very edge of our testing boundaries,
		//so we don't get a big discontinuity btwn adjacent probes where one hits a
		//road edge and the other stops before crossing one
		if (!AvoidanceInfoArray[i].HasObstruction())
		{
			scores[i] += ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_RoadEdge, fTestDistance, ORIGIN, ORIGIN, obstructionData.fCurrentSpeed);
		}

		//did we make this direction more desirable than it would be if there were no obstruction there?
		//Assert(scores[i] >= fUnobstructedScore);
		scores[i] = rage::Max(scores[i], fUnobstructedScore);

		if (scores[i] < fBestScore)
		{
			fBestScore = scores[i];
			iBestIndex = i;
		}	

#if __BANK
		if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			// Use the coordinates of our front bumper.
			Vector3 vBumperPos = pVeh->TransformIntoWorldSpace(Vector3(0.0f, obstructionData.vBoundBoxMax.GetYf() * 0.9f, 0.0f));
			Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetA()));
			Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));

			if (IsDrivingFlagSet(DF_DriveInReverse))
			{
				VehRightVector.Negate();
				VehForwardVector.Negate();
			}
			//Vector3 VehPosition(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()));

			const float fLineLength = AvoidanceInfoArray[i].HasObstruction() ? AvoidanceInfoArray[i].fDistToObstruction : 2.0f;
			Vector3 Offset1 = fLineLength * rage::Cosf(obstructionData.vehDriveOrientation /*+ fOriginalDirection*/ - AvoidanceInfoArray[i].fDirection) * VehForwardVector;
			Vector3 Offset2 = fLineLength * rage::Sinf(obstructionData.vehDriveOrientation /*+ fOriginalDirection*/ - AvoidanceInfoArray[i].fDirection) * VehRightVector;

			Vector3 vResultPos = vBumperPos + Offset1 + Offset2;
			if (!AvoidanceInfoArray[i].HasObstruction())
			{
				CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vBumperPos), VECTOR3_TO_VEC3V(vResultPos), Color_green);
				fwVectorMap::DrawLine(vBumperPos, vResultPos, Color_green, Color_green);
				fwVectorMap::DrawLine(vBumperPos, vResultPos, Color_green, Color_green);
			}
			else
			{
				float markerDist = AvoidanceInfoArray[i].fDistToObstruction;
				Vector3 vMarkerOffset = Offset1 + Offset2;
				vMarkerOffset.Scale(0.2f);
				vMarkerOffset.Scale(markerDist);

				Color32 debugColor = Color_red;
				if (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_RoadEdge)
				{
					debugColor = Color_blue;
				}
				else if (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Ped)
				{
					debugColor = Color_orange;
				}
				else if (AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Object)
				{
					debugColor = Color_yellow;
				}
				CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vBumperPos), VECTOR3_TO_VEC3V(vResultPos), debugColor);
				fwVectorMap::DrawLine(vBumperPos, vResultPos, debugColor, debugColor);
				fwVectorMap::DrawMarker(vBumperPos + vMarkerOffset, debugColor, 0.2f);
			}
			char sWeight[64];
			sprintf(sWeight, "%.2f", scores[i]);

			if (bFoundWayOutForScoring && AvoidanceInfoArray[i].HasObstruction()
				&& obstructionData.turnDir != BIT_TURN_LEFT
				&& obstructionData.turnDir != BIT_TURN_RIGHT
				&& AvoidanceInfoArray[i].fDistToObstruction < fFurthestDistToClosestRealObstruction)
			{
				CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vResultPos), VECTOR3_TO_VEC3V(vResultPos) + Vec3V(0.0f, 0.0f, 0.25f), Color_orange);
			}

			if (ms_bDisplayObstructionProbeScores)
			{
				grcDebugDraw::Text(vResultPos, Color_white, 0, 0, sWeight, true, -1);
			}
			//******************************
		}

#endif // __BANK
	}

	Assert(iBestIndex >= 0);

#if __BANK
	if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Vector3 vBumperPos = pVeh->TransformIntoWorldSpace(Vector3(0.0f, obstructionData.vBoundBoxMax.GetYf() * 0.9f, 0.0f));
		Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetA()));
		Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
		//Vector3 VehPosition(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()));

		Vector3 Offset1 = 4.0f * rage::Cosf(obstructionData.vehDriveOrientation /*+ fOriginalDirection*/ - AvoidanceInfoArray[iBestIndex].fDirection) * VehForwardVector;
		Vector3 Offset2 = 4.0f * rage::Sinf(obstructionData.vehDriveOrientation /*+ fOriginalDirection*/ - AvoidanceInfoArray[iBestIndex].fDirection) * VehRightVector;

		Vector3 vResultPos = vBumperPos + Offset1 + Offset2; vResultPos.z += 0.05f;
		CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vBumperPos), VECTOR3_TO_VEC3V(vResultPos), Color_black);
	}
#endif //__BANK

	const float fDistToOriginalObstruction = Min(AvoidanceInfoArray[0].fDistToObstruction, AvoidanceInfoArray[iBestIndex].fDistToObstruction);

	float fLerpAmount = 1.0f;
	static dev_float s_fMinSpeedToDoLerping = 10.0f;
	if (fOurSpeed > s_fMinSpeedToDoLerping)
	{
		const float fEstTimeToCollision = fDistToOriginalObstruction / fOurSpeed;
		//static dev_float s_fMinTimeToDoFullAvoidance = 1.0f;
		//static dev_float s_fMinLerp = 0.25f;
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fMinTimeToDoFullAvoidance, 1.0f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, s_fMinLerp, 0.05f, 0.0f, 1.0f, 0.01f);
		fLerpAmount = Clamp((1.0f - fEstTimeToCollision) + s_fMinTimeToDoFullAvoidance, s_fMinLerp, 1.0f);

// 		if (fEstTimeToCollision < s_fMinTimeToDoFullAvoidance)
// 		{
// 			grcDebugDraw::Sphere(pVeh->GetVehiclePosition(), 2.0f, Color_SkyBlue, false);
// 		}
	}	

	m_bAvoidedLeftLastFrame = false;
	m_bAvoidedRightLastFrame = false;

	if (iBestIndex % 2 != 0 && iBestIndex != 0)
	{
		m_bAvoidedRightLastFrame = true;
	}
	else if (iBestIndex != 0)
	{
		m_bAvoidedLeftLastFrame = true;
	}

	//Set the obstruction information.
	{
		//IsCompletelyObstructed may be set to true even if some of the obstructions
		//are far away road edges. we use the "close" variant to specify when the vehicle
		//is really blocked in all directions
		const float fStopCloseDistance = 10.0f + obstructionData.vBoundBoxMax.GetYf();
		if (!bFoundWayOut && fDistSqrToCenterOfClosestRealObstruction < (fStopCloseDistance * fStopCloseDistance))
		{
			m_ObstructionInformation.m_uFlags.SetFlag(ObstructionInformation::IsCompletelyObstructedClose);
		}
		//Iterate over the angles closely around the best direction -- in a lot of situations,
		//just checking the best direction is not good enough.  It results in a lot of collisions.
		static int s_iNumAnglesAroundBest = 1;
		for(int i = iBestIndex - s_iNumAnglesAroundBest; i <= iBestIndex + s_iNumAnglesAroundBest; ++i)
		{
			//Ensure the index is valid.
			if((i < 0) || (i >= s_iNumObstructionProbes))
			{
				continue;
			}

			//Check if the obstruction is law enforcement.
			bool bIsObstructedByLawEnforcementPed = (AvoidanceInfoArray[i].bIsObstructedByLaw &&
				(AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Ped));
			if(bIsObstructedByLawEnforcementPed)
			{
				m_ObstructionInformation.m_uFlags.SetFlag(ObstructionInformation::IsObstructedByLawEnforcementPed);
			}
			bool bIsObstructedByLawEnforcementVehicle = (AvoidanceInfoArray[i].bIsObstructedByLaw &&
				(AvoidanceInfoArray[i].ObstructionType == AvoidanceInfo::Obstruction_Vehicle));
			if(bIsObstructedByLawEnforcementVehicle)
			{
				m_ObstructionInformation.m_uFlags.SetFlag(ObstructionInformation::IsObstructedByLawEnforcementVehicle);
			}
			if(bIsObstructedByLawEnforcementPed || bIsObstructedByLawEnforcementVehicle)
			{
				m_ObstructionInformation.m_fDistanceToLawEnforcementObstruction = Min(
					m_ObstructionInformation.m_fDistanceToLawEnforcementObstruction,
					AvoidanceInfoArray[i].fDistToObstruction);
			}
		}
	}

	DR_ONLY(debugPlayback::RecordVehicleObstructionArray(pVeh->GetCurrentPhysicsInst(), AvoidanceInfoArray, s_iNumObstructionProbes, obstructionData.vehDriveOrientation, pVeh->GetTransform().GetA(), pVeh->GetVehicleForwardDirection()));

	const float fRetVal = fwAngle::LerpTowards(obstructionData.vehDriveOrientation, AvoidanceInfoArray[iBestIndex].fDirection, fLerpAmount);
	Assert(fRetVal > -THREE_PI && fRetVal < THREE_PI);
	return fRetVal;
}

float CTaskVehicleGoToPointWithAvoidanceAutomobile::TestDirectionsForObstructionsNew(CVehicle* pVeh, const CEntity *pException
	, float MinX, float MinY, float MaxX, float MaxY, const float fOriginalDirection, const float vehDriveOrientation
	, CarAvoidanceLevel carAvoidanceLevel, bool bAvoidVehs, bool bAvoidPeds, bool bAvoidObjs, bool& bGiveWarning
	, const bool bWillStopForCars, const spdAABB& boundBox, const float fDesiredSpeed)
{
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, RENDER_CAR_OBSTRUCTIONS, false );
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, USE_NEW_OBSTRUCTION_RESULTS, true);
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, USE_QUICK_BLOCKING_CHECK, true);

	//if we don't have "swerve around peds" set,
	//but we do have "swerve around objects" set,
	//it's probably because we've got "stop for peds" set instead.
	//we don't want to stop for dead peds though, so allow
	//steering around them
	const bool bOnlyAvoidTrulyStationaryPeds = !bAvoidPeds && bAvoidObjs;
	
	if (m_pOptimization_pLastCarBlockingUs && USE_QUICK_BLOCKING_CHECK)
	{
		// Another car was blocking us last frame, assume it still is. 
#if __BANK
		if (RENDER_CAR_OBSTRUCTIONS && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			{
				fwVectorMap::DrawLine(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), VEC3V_TO_VECTOR3(m_pOptimization_pLastCarBlockingUs->GetVehiclePosition()), Color_red, Color_red);
			}
		}
#endif
		return fOriginalDirection;
	}

	AvoidanceTaskObstructionData obstructionData;

	Vec3V bboxMin = boundBox.GetMin();
	Vec3V bboxMax = boundBox.GetMax();

	obstructionData.pVeh = pVeh;
	obstructionData.pException = pException;
	obstructionData.MinX = MinX;
	obstructionData.MinY = MinY;
	obstructionData.MaxX = MaxX;
	obstructionData.MaxY = MaxY;
	obstructionData.fOriginalDirection = fOriginalDirection;
	obstructionData.fDesiredSpeed = pVeh->pHandling ? rage::Min(fDesiredSpeed, pVeh->pHandling->m_fEstimatedMaxFlatVel) : fDesiredSpeed;
	obstructionData.fCurrentSpeed = pVeh->GetAiXYSpeed();
	obstructionData.vehDriveOrientation = vehDriveOrientation;
	obstructionData.carAvoidanceLevel = carAvoidanceLevel;
	obstructionData.vBoundBoxMin = bboxMin;
	obstructionData.vBoundBoxMax = bboxMax;
	obstructionData.bIsCurrentlyOnJunction = pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO;
	obstructionData.bWillStopForCars = bWillStopForCars;
	obstructionData.vPosOfOriginalObstruction.ZeroComponents();
	obstructionData.bOnSingleTrackRoad = pVeh->GetIntelligence()->IsOnSingleTrackRoad();
	obstructionData.bRacing = pVeh->m_nVehicleFlags.bIsRacing;

	if (!obstructionData.bIsCurrentlyOnJunction)
	{
		obstructionData.turnDir = BIT_NOT_SET;
	}
	else
	{
		//this fn will find the direction of the jn regardless of how far ahead it is,
		//but we only want to set this data if the vehicle is already on the junction
		obstructionData.turnDir = pVeh->GetIntelligence()->GetJunctionTurnDirection();
	}
	const Vec3V vNormalizedDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse));
	obstructionData.vNormalizedDriveDir = vNormalizedDriveDir;

	DEV_ONLY(static) ScalarV vfPercentOfLengthToUse(0.33f);
	const ScalarV vfCarMarginUs = !pVeh->InheritsFromBoat() ? ScalarV(V_ZERO) : ScalarV(V_QUARTER);

	Mat34V vehMat = pVeh->GetMatrix();
	ScalarV vfCarLength = (bboxMax - bboxMin).GetY();
	const ScalarV vfDistFromBumper = vfCarLength * vfPercentOfLengthToUse;
	//Vec3V aiCenter = vehMat.GetCol3();
	
	ScalarV vfZero(V_ZERO);

	Vec3V aiFrontLeftCornerLocalSpace = Vec3V(bboxMin.GetX() - vfCarMarginUs, bboxMax.GetY() + vfCarMarginUs, vfZero);
	Vec3V aiFrontLeftCornerWorldSpace = Transform(vehMat, aiFrontLeftCornerLocalSpace);

	Vec3V aiRearLeftCornerLocalSpace = Vec3V(bboxMin.GetX() - vfCarMarginUs, bboxMin.GetY() - vfCarMarginUs, vfZero);
	Vec3V aiRearLeftCornerWorldSpace = Transform(vehMat, aiRearLeftCornerLocalSpace);

	Vec3V aiBumperVectorLocalSpace = Vec3V(bboxMax.GetX() - bboxMin.GetX() + vfCarMarginUs + vfCarMarginUs, vfZero, vfZero);
	Vec3V aiBumperVectorWorldSpace = Transform3x3(vehMat, aiBumperVectorLocalSpace);

	Vec3V aiSideVectorLocalSpace = Vec3V(vfZero, Negate(vfDistFromBumper + vfCarMarginUs + vfCarMarginUs), vfZero);
	Vec3V aiSideVectorWorldSpace = Transform3x3(vehMat, aiSideVectorLocalSpace);

	Vec3V aiBumperCenterLocalSpace = Vec3V(vfZero, bboxMax.GetY() * ScalarV(0.9f), vfZero);
	Vec3V aiBumperCenterWorldSpace = Transform(vehMat, aiBumperCenterLocalSpace);

	obstructionData.vFrontLeftCorner = aiFrontLeftCornerWorldSpace;
	obstructionData.vRearLeftCorner = aiRearLeftCornerWorldSpace;
	obstructionData.vBumperVector = aiBumperVectorWorldSpace;
	obstructionData.vSideVector = aiSideVectorWorldSpace;
	obstructionData.vBumperCenter = aiBumperCenterWorldSpace;

	const float speed = pVeh->GetAiXYSpeed();
	obstructionData.fAvoidanceLookaheadTimeShort = GetAvoidanceLookaheadTime(speed, false);
	obstructionData.fAvoidanceLookaheadTimeLong = GetAvoidanceLookaheadTime(speed, true);

	if (bAvoidVehs)
	{
		FindVehicleObstructions(obstructionData, bGiveWarning);
	}
	if (bAvoidPeds || bOnlyAvoidTrulyStationaryPeds)
	{
		FindPedObstructions(obstructionData, bOnlyAvoidTrulyStationaryPeds);
	}
	if (bAvoidObjs)
	{
		FindObjectObstructions(obstructionData);
	}

	//Test the original direction
	AvoidanceInfo originalDirTest;
	originalDirTest.fDirection = fOriginalDirection;

	const CPhysical* pHitEntity = NULL;
	TestOneDirectionAgainstObstructions(obstructionData, originalDirTest, false, bGiveWarning, &pHitEntity);
	if (!originalDirTest.HasObstruction()) 
	{
#if __BANK
		if (RENDER_CAR_OBSTRUCTIONS && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			obstructionData.DebugDraw(pVeh);
		}
#endif

		m_bAvoidedLeftLastFrame = false;
		m_bAvoidedRightLastFrame = false;
		return fOriginalDirection;
	}
	else if (pHitEntity)
	{
		if (originalDirTest.bIsStationaryCar)
		{
			pVeh->m_nVehicleFlags.bIsOvertakingStationaryCar = true;
		}
		pVeh->GetIntelligence()->m_pSteeringAroundEntity = pHitEntity;
	}

	obstructionData.vPosOfOriginalObstruction = VECTOR3_TO_VEC3V(originalDirTest.vPosition);

	if (!IsDrivingFlagSet(DF_GoOffRoadWhenAvoiding) && !pVeh->m_nVehicleFlags.bDisableRoadExtentsForAvoidance)
	{
#if USE_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
		FindRoadEdgesUsingFollowRoute(obstructionData);
#else
		FindRoadEdgesUsingNodeList(obstructionData);
#endif
	}

	TUNE_GROUP_FLOAT( FOLLOW_PATH_AI_STEER, sfProbeAngleScaler, 1.75f, 0.0f, 10.0f, 0.1f);
	float fProbeScaler = 1.0f;

	if( originalDirTest.HasObstruction() && originalDirTest.ObstructionType == AvoidanceInfo::Obstruction_Object)
	{
		//we've been avoiding an object for a while now, increase probe angle to try and drive around
		if( m_threePointTurnInfo.m_iNumRecentThreePointTurns >= m_threePointTurnInfo.ms_iMaxAttempts)
		{
			fProbeScaler = sfProbeAngleScaler + ( m_threePointTurnInfo.m_iNumRecentThreePointTurns - m_threePointTurnInfo.ms_iMaxAttempts) * 0.2f;
		}
	}

	const float s_fMaxAvoidAngle = pVeh->pHandling->m_fSteeringLock * fProbeScaler;
	const float s_fRadiansBetweenProbes = (2.0f * s_fMaxAvoidAngle) / (float)s_iNumObstructionProbes;


#if 0
	if (USE_WEDGE_CULLING)
	{
		float minAngle = vehDriveOrientation + Min((s_iNumObstructionProbes / 2) * -1.0f * s_fRadiansBetweenProbes, s_fMaxAvoidAngle);
		minAngle = fwAngle::LimitRadianAngleSafe(minAngle);

		float maxAngle = vehDriveOrientation + Min((s_iNumObstructionProbes / 2) * 1.0f * s_fRadiansBetweenProbes, s_fMaxAvoidAngle);
		maxAngle = fwAngle::LimitRadianAngleSafe(maxAngle);

		obstructionData.WedgeCull(minAngle, maxAngle);
	}
#endif

#if __BANK
	if (RENDER_CAR_OBSTRUCTIONS && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		obstructionData.DebugDraw(pVeh);
	}
#endif

	atRangeArray<AvoidanceInfo, s_iNumObstructionProbes> AvoidanceInfoArray;
	AvoidanceInfoArray[0].fDirection = vehDriveOrientation;

	// Initialize the remaining directions
	int iOffset = 1;
	for (int i = 1; i < s_iNumObstructionProbes; i+=2)
	{
		AvoidanceInfoArray[i].fDirection = vehDriveOrientation + Min(iOffset * -1.0f * s_fRadiansBetweenProbes, s_fMaxAvoidAngle);
		AvoidanceInfoArray[i].fDirection = fwAngle::LimitRadianAngleSafe(AvoidanceInfoArray[i].fDirection);

		//don't overrun the array!
		if (i+1 < s_iNumObstructionProbes)
		{
			AvoidanceInfoArray[i+1].fDirection = vehDriveOrientation + Min(iOffset * s_fRadiansBetweenProbes, s_fMaxAvoidAngle);
			AvoidanceInfoArray[i+1].fDirection = fwAngle::LimitRadianAngleSafe(AvoidanceInfoArray[i+1].fDirection);
		}

		iOffset++;
	}
	
	//Note that at this point, we think we are completely obstructed.
	//This flag will be cleared if it turns out that this is no longer the case.
	m_ObstructionInformation.m_uFlags.SetFlag(ObstructionInformation::IsCompletelyObstructed);

#if !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
	for (int i = 0; i < s_iNumObstructionProbes; i++) // start at 0 (redo the straight-ahead test because we want to test road edges too now)
	{
		if(!TestOneDirectionAgainstObstructions(obstructionData, AvoidanceInfoArray[i], true, bGiveWarning))
		{
			//Note that we are not completely obstructed.
			m_ObstructionInformation.m_uFlags.ClearFlag(ObstructionInformation::IsCompletelyObstructed);
		}
	}
#else	// !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE
	bool bHitSomethingArray[s_iNumObstructionProbes];
#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	for (int i = 0; i < s_iNumObstructionProbes; i++) // start at 0 (redo the straight-ahead test because we want to test road edges too now)
	{
		bHitSomethingArray[i] = false;
	}

	// Test against all the vehicles, and update bHitsomethingArray and AvoidainceInfoArray.
	TestAndScoreLineSegmentsAgainstVehicles(obstructionData, AvoidanceInfoArray.GetElements(), s_iNumObstructionProbes, bHitSomethingArray, bGiveWarning);

	// Test against all the peds and objects, and update bHitsomethingArray and AvoidainceInfoArray.
	TestAndScoreLineSegmentsAgainstCylindricalObstructions(obstructionData, AvoidanceInfoArray.GetElements(), s_iNumObstructionProbes, bHitSomethingArray);
#else
	for (int i = 0; i < s_iNumObstructionProbes; i++) // start at 0 (redo the straight-ahead test because we want to test road edges too now)
	{
		bHitSomethingArray[i] = TestOneDirectionAgainstObstructions(obstructionData, AvoidanceInfoArray[i], false, bGiveWarning);
	}
#endif

	if(!IsDrivingFlagSet(DF_GoOffRoadWhenAvoiding))
	{
		TestAndScoreLineSegmentsCrossRoadBoundariesUsingFollowRoute(obstructionData, AvoidanceInfoArray.GetElements(), s_iNumObstructionProbes, bHitSomethingArray);
	}

	bool bHitEverything = true;
	for(int j = 0; j < s_iNumObstructionProbes; j++)
	{
		if(!bHitSomethingArray[j])
		{
			bHitEverything = false;
			break;
		}
	}

	if(!bHitEverything)
	{
		//Note that we are not completely obstructed.
		m_ObstructionInformation.m_uFlags.ClearFlag(ObstructionInformation::IsCompletelyObstructed);
	}
#endif	// !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

	const float fReturnDirection = ScoreAvoidanceDirections(obstructionData, &AvoidanceInfoArray[0], originalDirTest.fDistToObstruction);

	//potentially honk at the thing that was obstructing us originally:
	if (originalDirTest.HasObstruction() && originalDirTest.ObstructionType == AvoidanceInfo::Obstruction_Vehicle)
	{
		const CPed* pOtherCarDriver = pHitEntity && pHitEntity->GetIsTypeVehicle() ? (static_cast<const CVehicle*>(pHitEntity))->GetDriver() : NULL;
		if (pOtherCarDriver 
			&& pOtherCarDriver->IsPlayer()
			&& originalDirTest.bIsStationaryCar
			&& !m_ObstructionInformation.m_uFlags.IsFlagSet(ObstructionInformation::IsCompletelyObstructedClose)
			&& (pVeh->GetDriver() && pVeh->GetDriver()->CheckBraveryFlags(BF_PLAY_CAR_HORN))
			&& obstructionData.fCurrentSpeed < 0.1f)
		{
			//
			//grcDebugDraw::Sphere(pVeh->GetVehiclePosition(), 1.5f, Color_green, false, 10);
			////////

			CVehicle* pVehNonConst = const_cast<CVehicle*>(pVeh);

			if (pVehNonConst->m_iNextValidHornTime < fwTimer::GetTimeInMilliseconds())
			{
				HelperMaybePlayHorn(pVehNonConst, true, true, pOtherCarDriver, false, true);

				TUNE_GROUP_INT(CAR_AI, RndMinHonkDelay, 500, 0, 20000, 100);
				TUNE_GROUP_INT(CAR_AI, RndMaxHonkDelay, 7500, 0, 20000, 100);
				pVehNonConst->m_iNextValidHornTime = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(RndMinHonkDelay, RndMaxHonkDelay);
			}

	//		HelperMaybePlayHorn(pVehNonConst, true, true, pOtherCarDriver, false, true);
		}
	}

	return fReturnDirection;
}

CVehicle* CTaskVehicleGoToPointWithAvoidanceAutomobile::GetTrailerParentFromVehicle(const CVehicle* pVehicle)
{
	if( pVehicle  )
	{
		return pVehicle->GetAttachParentVehicleDummyOrPhysical();
	}

	return NULL;
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindVehicleObstructions(AvoidanceTaskObstructionData& obstructionData, const bool bGiveWarning)
{
	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_STEER, sfIgnoreCarForWeavingHeight, 5.0f, 0.0f, 100.0f, 0.1f);

	const CVehicle* pVeh = obstructionData.pVeh;

	// If the exception is a ped who's in a vehicle, ignore the vehicle
	const CEntity* pVehicleException = obstructionData.pException;
	if( pVehicleException && pVehicleException->GetIsTypePed() )
	{
		const CPed* pPed = static_cast<const CPed*>(pVehicleException);
		if( pPed && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() )
		{
			pVehicleException = pPed->GetMyVehicle();
		}
	}

	const Vector3 ourPosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());

	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		if(pEntity == pVehicleException)
		{
			continue;
		}

		//skip vehicles about to be removed
		if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		{
			continue;
		}

		Assert(pEntity->GetIsTypeVehicle());
		CVehicle *pVehObstacle =(CVehicle *)pEntity;

		//if we aren't going to stop for the cars later, still swerve around them here
		if ((pVehObstacle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle
			|| pVehObstacle->m_nVehicleFlags.bIsAmbulanceOnDuty
			|| pVehObstacle->m_nVehicleFlags.bIsFireTruckOnDuty)
			&& obstructionData.carAvoidanceLevel < CAR_AVOIDANCE_FULL)
		{
			continue;
		}

		Vector3 otherVehCentre = VEC3V_TO_VECTOR3(pVehObstacle->GetVehiclePosition());
		if (otherVehCentre.x < obstructionData.MinX
			|| otherVehCentre.x > obstructionData.MaxX
			|| otherVehCentre.y < obstructionData.MinY
			|| otherVehCentre.y > obstructionData.MaxY
			|| ABS(otherVehCentre.z - ourPosition.z) > sfIgnoreCarForWeavingHeight)
		{
			continue;
		}

		if (ShouldIgnoreNonDirtyVehicle(*pVeh, *pVehObstacle, IsDrivingFlagSet(DF_DriveInReverse), obstructionData.bOnSingleTrackRoad))
		{
			continue;
		}

		// Ignore trailers we're towing
		if(pVehObstacle->InheritsFromTrailer() && pVehObstacle->m_nVehicleFlags.bHasParentVehicle)
		{
			// check whether the trailer is attached to the parent vehicle
			//CTrailer* pTrailer = static_cast<CTrailer*>(pVehObstacle);
			const CVehicle* pAttachParent = GetTrailerParentFromVehicle(pVehObstacle);
			if(pAttachParent == pVeh || (pAttachParent == pVehicleException && pAttachParent != NULL))
			{
				continue;
			}
		}

		//ignore cars on the back of a trailer
		if (pVehObstacle->m_nVehicleFlags.bHasParentVehicle)
		{
			CVehicle* pVehParent = GetTrailerParentFromVehicle(pVehObstacle);
			if (pVehParent && (pVehParent->InheritsFromTrailer() || pVehParent == pVeh))
			{
				continue;
			}
		}

		//if our parent task has told us to exclude a certain vehicle, also
		//exclude its attached trailer, if any
		if (pVehicleException && pVehicleException->GetIsTypeVehicle())
		{
			const CVehicle* pExceptionVehicle = static_cast<const CVehicle*>(pVehicleException);
			const CTrailer* pExceptionTrailer = pExceptionVehicle->GetAttachedTrailer();
			if (pEntity == pExceptionTrailer)
			{
				continue;
			}

			//if we're supposed to avoid a trailer, also exclude its attach parent
			if (pExceptionVehicle->InheritsFromTrailer() && pVehObstacle == GetTrailerParentFromVehicle(pExceptionVehicle))
			{
				continue;
			}
		}

		{
			// Now actually do a collision threat analysis for this car
			CPed* driver = pVehObstacle->GetDriver();
			if (driver && driver->IsPlayer())
			{
				CPlayerInfo* playerInfo = CGameWorld::GetMainPlayerInfo();
				playerInfo->NearVehicleMiss(pVehObstacle, pVeh, false);
			}

			// Check if the car should not avoid the other car because they are in a convoy together.
			if(pVehObstacle != pVeh && !InConvoyTogether(pVeh, pVehObstacle) && !ShouldNotAvoidVehicle(*pVeh, *pVehObstacle)) //(pVeh->m_nVehicleFlags.bPartOfConvoy && pVehObstacle->m_nVehicleFlags.bPartOfConvoy))
			{
				bool bIsStationaryCar = IsThisAStationaryCar(pVehObstacle);
				bool bIsParkedCar = IsThisAParkedCar(pVehObstacle);
				//WeaveForOtherCar(pVeh, pVehObstacle, pLeftAngle, pRightAngle, bIsStationaryCar, bIsParkedCar, carAvoidanceLevel, bWeMustGoLeft, bWeMustGoRight, bIgnoreFurtherCars, bCopperBlockedCouldLeaveCar, bGiveWarning);
				AddObstructionForSingleVehicle(obstructionData, pVehObstacle, bIsStationaryCar, bIsParkedCar, bGiveWarning);
			}
		}
	}
}

bool HelperPedIsStationaryForAvoidance(const CPed* pPed)
{
	Assert(pPed);
	return pPed->IsDead() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInStationaryScenario);
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindPedObstructions(AvoidanceTaskObstructionData& obstructionData, bool bOnlyAvoidTrulyStationaryPeds)
{
	const CVehicle* pVeh = obstructionData.pVeh;

	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetPedScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		if(pEntity == obstructionData.pException)
		{
			continue;
		}

		CPed* pPed = static_cast<CPed*>(pEntity);

		if (pPed->GetIsInVehicle())
		{
			continue;
		}

		if (pPed->GetIsOnMount())
		{
			continue;
		}

		//skip peds about to be removed
		if (pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		{
			continue;
		}

		if (pPed->GetCapsuleInfo()->IsBird())
		{
			continue;
		}

		//skip non-dead peds if we don't want to aovid them
		//also support scenario peds here now.
		const bool bIsStationaryPed = HelperPedIsStationaryForAvoidance(pPed);

		if (bOnlyAvoidTrulyStationaryPeds 
			&& !bIsStationaryPed)
		{
			continue;
		}

		const Vector3 vecEntityPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fVehicleZ = pVeh->GetVehiclePosition().GetZf();
		if(	vecEntityPos.x > obstructionData.MinX && vecEntityPos.x < obstructionData.MaxX &&
			vecEntityPos.y > obstructionData.MinY && vecEntityPos.y < obstructionData.MaxY &&
			ABS(vecEntityPos.z - fVehicleZ) < 4.0f)
		{
			// if this guy is actually standing on the car we will avoid avoiding it.
			if(pPed->GetGroundPhysical() != pVeh && pPed->GetAttachParent() != pVeh)
			{
				// Now actually do a collision threat analysis for this ped
				AddObstructionForSinglePed(obstructionData, pPed);
			}
		}
	}
}

bool HelperShouldIgnoreDoorEntirely(const CDoor* pDoor)
{
	//we couldn't give a fuck about barrier arms that are all the way up,
	//even if their bounding boxes extend into our driveable space
	Assert(pDoor);
	static dev_float s_fDoorOpenRatio = 0.89f;
	float doorOpenRatio = pDoor->GetDoorOpenRatio();

	if((doorOpenRatio > s_fDoorOpenRatio && (pDoor->GetDoorType() == CDoor::DOOR_TYPE_BARRIER_ARM || pDoor->GetDoorType() == CDoor::DOOR_TYPE_BARRIER_ARM_SC)) ||
		(doorOpenRatio < (1.0f - s_fDoorOpenRatio ) && (pDoor->GetDoorType() == CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER || pDoor->GetDoorType() == CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER_SC)))
	{
		return true;
	}

	return false;
}

bool HelperWillDoorAutoOpen(const CDoor* pDoor)
{
	bool bIsAutoOpen = CDoor::DoorTypeAutoOpensForVehicles(pDoor->GetDoorType());
	bIsAutoOpen &= !pDoor->GetDoorSystemData() || !pDoor->GetDoorSystemData()->GetLocked();
	return bIsAutoOpen;
}

bool HelperShouldVehicleStopVsSwerveForDoor(Vec3V_In vVehFwdDir, ScalarV_In fVehicleWidth, const CDoor* pDoor)
{
	Assert(pDoor);
	static dev_float s_fDoorOpenRatio = 0.89f;
	if (Abs(pDoor->GetDoorOpenRatio()) > s_fDoorOpenRatio)
	{
		return false;
	}

	//don't stop for doors that are narrower than the car, we may have accidentally encountered a ped door
	spdAABB tempBox;
	const spdAABB& boundBox = pDoor->GetLocalSpaceBoundBox(tempBox);
	ScalarV sfDoorWidth = boundBox.GetMax().GetX() - boundBox.GetMin().GetX();

	//currently only for sliding doors, will want to change this in future
	if( pDoor->GetIsDoubleDoor() )
	{
		const CDoor* pOtherDoor = pDoor->GetOtherDoubleDoor();
		if( pOtherDoor != NULL)
		{
			const spdAABB& otherBoundBox = pOtherDoor->GetLocalSpaceBoundBox(tempBox);
			ScalarV otherWidth = otherBoundBox.GetMax().GetX() - otherBoundBox.GetMin().GetX();
			sfDoorWidth += otherWidth;
		}
	}
	else
	{
		//some double doors currently aren't marked as so, so add a small buffer
		//single ped doors will still be too small, but vehicle double doors will be valid
		static const ScalarV extraBuffer = ScalarV(0.25f);
		sfDoorWidth += extraBuffer;
	}

	if (IsGreaterThanAll(fVehicleWidth, sfDoorWidth))
	{
		return false;
	}

	Vec3V_In vObjFwdDir = pDoor->GetTransform().GetForward();
	static const ScalarV vDotThreshold(V_HALF);
	const ScalarV vDot = Dot(vVehFwdDir, vObjFwdDir);

	return (IsGreaterThanAll(Abs(vDot), vDotThreshold) != 0);
}

bool IsVehicleAtTollbooth(Vec3V_In vPos)
{
	static const spdAABB booth1(Vec3V(783.0f, -3174.2f, -5.9f), Vec3V(815.9f, -3137.1f, 15.9f));
	static const spdAABB booth2(Vec3V(1077.9f, -3341.8f, -5.9f), Vec3V(1109.1f, -3311.8f, 15.9f));

	if (!booth1.ContainsPointFlat(vPos)
		&& !booth2.ContainsPointFlat(vPos))
	{
		return false;
	}

	return true;
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindObjectObstructions(AvoidanceTaskObstructionData& obstructionData)
{
	const CVehicle* pVeh = obstructionData.pVeh;
	//const float fVehicleZ = pVeh->GetVehiclePosition().GetZf();
	static dev_float s_fPadding = 0.25f;
	const float fOurMinHeight = obstructionData.vBoundBoxMin.GetZf() - s_fPadding;
	const float fOurMaxHeight = obstructionData.vBoundBoxMax.GetZf() + s_fPadding;

	const bool bIsVehicleAtTollBooth = IsVehicleAtTollbooth(obstructionData.vBumperCenter);

	const CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetObjectScanner().GetIterator();
	for( const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		if (pEntity->GetBaseModelInfo()->GetNotAvoidedByPeds())
		{
			continue;
		}

		//skip objects about to be removed
		if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		{
			continue;
		}

		const Vec3V vOtherPosWorldSpace = pEntity->GetTransform().GetPosition();
		const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(vOtherPosWorldSpace);

		CObject* pObject = (CObject*)pEntity;

		if(	vecEntityPosition.x > obstructionData.MinX && vecEntityPosition.x < obstructionData.MaxX &&
			vecEntityPosition.y > obstructionData.MinY && vecEntityPosition.y < obstructionData.MaxY )
		{
			const CEntity* pAttachParent = static_cast<const CEntity*>(pEntity->GetAttachParent());
			if (pAttachParent && pAttachParent->GetIsTypeVehicle())
			{
				continue;
			}

			spdAABB aabb;
			pEntity->GetAABB(aabb);

			//const Vec3V vOtherPosCarSpace = pVeh->GetTransform().UnTransform(vOtherPosWorldSpace);
			const float fOtherMinHeightCarSpace = pVeh->GetTransform().UnTransform(aabb.GetMin()).GetZf();
			const float fOtherMaxHeightCarSpace = pVeh->GetTransform().UnTransform(aabb.GetMax()).GetZf();
			const bool bOtherMinIsWithinZRange = fOtherMinHeightCarSpace > fOurMinHeight && fOtherMinHeightCarSpace < fOurMaxHeight;
			const bool bOtherMaxIsWithinZRange = fOtherMaxHeightCarSpace > fOurMinHeight && fOtherMaxHeightCarSpace < fOurMaxHeight;
			const bool bOtherCompletelyWithinZRange = fOtherMinHeightCarSpace < fOurMinHeight && fOtherMaxHeightCarSpace > fOurMaxHeight;

			const float fOtherHeight = fOtherMaxHeightCarSpace - fOtherMinHeightCarSpace;
			const bool bObjectIsTallEnoughToAvoid = fOtherHeight > 0.9f;

			bool bObjectIsAutoOpenDoor = false;
			static const u32 barrierHash = ATSTRINGHASH("prop_sec_barier_04a", 2703851248);
			if (pObject->IsADoor())
			{
				const CDoor* pDoor = static_cast<const CDoor*>(pObject);
				bObjectIsAutoOpenDoor = HelperWillDoorAutoOpen(pDoor)
					&& HelperShouldVehicleStopVsSwerveForDoor(obstructionData.vNormalizedDriveDir, obstructionData.vBoundBoxMax.GetX() - obstructionData.vBoundBoxMin.GetX(), pDoor);

				if (HelperShouldIgnoreDoorEntirely(pDoor))
				{
					continue;
				}
			}
			else if ((pObject->GetBaseModelInfo() 
				&& pObject->GetBaseModelInfo()->GetModelNameHash() == barrierHash)
				|| bIsVehicleAtTollBooth)
			{
				//hack time: if the model is not a door, and is this one particular gate model,
				//ignore it and smash through

				continue;
			}

			if (!bObjectIsAutoOpenDoor && 
				(bOtherMinIsWithinZRange || bOtherMaxIsWithinZRange || bOtherCompletelyWithinZRange) && 
				(bObjectIsTallEnoughToAvoid || pObject->GetForceVehiclesToAvoid())
				)
			{
				// Now actually do a collision threat analysis for this object
				Assert(pEntity->GetIsTypeObject());
				//WeaveForObject(pVeh, (CObject *)pEntity, pLeftAngle, pRightAngle);
				AddObstructionForSingleObject(obstructionData, pObject);
			}
		}
	}
}

inline void BuildCarPoly(Vec2f verts[3], Vector3::Param ina, Vector3::Param inb, Vector3::Param inc)
{
	Vector3 a(ina), b(inb), c(inc);
	verts[0] = Vec2f(a.x, a.y);
	verts[1] = Vec2f(b.x, b.y);
	verts[2] = Vec2f(c.x, c.y);
}

inline void OffsetPoly(Vec2f* verts, int numVerts, Vec2f offset)
{
	for(int i = 0; i < numVerts; i++)
	{
		verts[i] += offset;
	}
}

void HelperGetHeliAvoidanceBounds(const CVehicle* pVehicle, spdAABB& boundBox)
{
	if(pVehicle->GetIsHeli() && !pVehicle->IsEngineOn())
	{
		//for helicopters that aren't moving, we use the bounds of the chassis and tail fin, as bounds for whole vehicle is huge
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pVehicle->GetVehicleFragInst()->GetArchetype()->GetBound()); 
		phBound* chassisBound = pBoundComp->GetBound(0);
		boundBox.SetMin(chassisBound->GetBoundingBoxMin());
		boundBox.SetMax(chassisBound->GetBoundingBoxMax());

		int tailIndex = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(HELI_TAIL));
		if(tailIndex != -1)
		{
			phBound* tailBound = pBoundComp->GetBound(tailIndex);
			if(tailBound)
			{
				Vector3 vecTemp;
				RCC_MATRIX34(pBoundComp->GetCurrentMatrix(tailIndex)).Transform(VEC3V_TO_VECTOR3(tailBound->GetBoundingBoxMin()), vecTemp);
				vecTemp.Min(vecTemp, VEC3V_TO_VECTOR3(boundBox.GetMin()));
				boundBox.SetMin(VECTOR3_TO_VEC3V(vecTemp));

				RCC_MATRIX34(pBoundComp->GetCurrentMatrix(tailIndex)).Transform(VEC3V_TO_VECTOR3(tailBound->GetBoundingBoxMax()), vecTemp);
				vecTemp.Max(vecTemp,VEC3V_TO_VECTOR3(boundBox.GetMax()));
				boundBox.SetMax(VECTOR3_TO_VEC3V(vecTemp));
			}
		}

		//add a bit of a buffer
		const Vec3V offset(0.5f, 0.5f,0.0f);
		boundBox.SetMax(boundBox.GetMax() + offset);
		boundBox.SetMin(boundBox.GetMin() - offset);
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::AddObstructionForSingleVehicle(AvoidanceTaskObstructionData& obstructionData, const CVehicle* const pOtherCar, bool bStationaryCar, bool bParkedCar, bool bGiveWarning)
{
	const CVehicle* pVeh = obstructionData.pVeh;

	if(obstructionData.carAvoidanceLevel == CAR_AVOIDANCE_NONE && !bParkedCar)
	{
		return;
	}

	Assert(pVeh);
	Assert(pOtherCar);

	//static dev_float s_fCarMarginUs = 0.0f;

	float s_fCarMarginOther = CalculateOtherVehicleMarginForAvoidance(*pVeh, *pOtherCar, bGiveWarning);
	
	const Vector3 vecVehPosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	const Vector3 vecOtherCarPosition = VEC3V_TO_VECTOR3(pOtherCar->GetVehiclePosition());
	const float DiffX = vecOtherCarPosition.x - vecVehPosition.x;
	const float DiffY = vecOtherCarPosition.y - vecVehPosition.y;
	const Vector3 vecPositionDiff = vecOtherCarPosition - vecVehPosition;

	// Get the current drive direction and orientation.
	Vector3 vehDriveDir = RCC_VECTOR3(obstructionData.vNormalizedDriveDir);

	// If the other car is already behind us there is no need trying to avoid it.
	// However, if the car is stationary (or moving very slow), test against any cars
	// that are further ahead than our rear bumper in case we're turning into it
	const bool bUseRacingLogic = HelperShouldUseRacingLogic(obstructionData.bRacing, pVeh->InheritsFromBike(), obstructionData.fCurrentSpeed, pOtherCar);
	const Vector3 vOtherVel = !bUseRacingLogic ? pOtherCar->GetAiVelocity() 
		: HelperGetOtherVehsRacingVelocity(pOtherCar);
	const float vOtherVelMag2 = vOtherVel.Mag2();
	const float fThresholdForAvoidBehind = vOtherVelMag2 > 1.0f ? 0.0f : obstructionData.vBoundBoxMin.GetYf();
	if(DiffX * vehDriveDir.x + DiffY * vehDriveDir.y < fThresholdForAvoidBehind) 
	{
		return;
	}

	// If the other car is attached to a trailer (riding on top of it),
	// don't avoid it
	if (CVehicle::IsEntityAttachedToTrailer(pOtherCar))
	{
		return;
	}

	// If we are not allowed to go crazy an the avoidance and the other car is in front of us facing away
	// from us and not stuck we don't want to try and avoid us;
	// It is probably just traffic.
	const Vector3 otherB = VEC3V_TO_VECTOR3(pOtherCar->GetVehicleForwardDirection());
	const float fDebugCarsFacingDot = vehDriveDir.Dot(otherB);
	if((obstructionData.carAvoidanceLevel == CAR_AVOIDANCE_LITTLE) && !bStationaryCar
		&& !pOtherCar->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle
		&& pOtherCar != m_pVehicleToIgnoreWhenStopping)
	{		
		// If the cars face roughly the same direction.
		static dev_float s_fCarsFacingDotThreshold = 0.6f;
		const float fAngleThresholdInRads = rage::Acosf(s_fCarsFacingDotThreshold);
		const float fOtherOrientation = fwAngle::LimitRadianAngle(fwAngle::GetATanOfXY(otherB.x, otherB.y));
		const float fThetaBetweenOurDesiredOtherActual = fabsf(SubtractAngleShorter(obstructionData.vehDriveOrientation, fOtherOrientation));
		if(fDebugCarsFacingDot > s_fCarsFacingDotThreshold || fThetaBetweenOurDesiredOtherActual < fAngleThresholdInRads)
		{
			// If the obstacle car is right in front of us.
			Vector3 diffNorm = Vector3(DiffX, DiffY, 0.0f);
			diffNorm.Normalize();
			const float fDebugOtherCarPositionDeltaDot = diffNorm.Dot(vehDriveDir);
			static dev_float s_fOtherCarPositionDeltaDotThreshold = 0.3f;
			if(fDebugOtherCarPositionDeltaDot > s_fOtherCarPositionDeltaDotThreshold)
			{
				// Don't bother avoiding this car. Instead we want to pull
				// up to the car and stop.
				return;
			}
		}
	}
	bool bWillingToStop = IsDrivingFlagSet(DF_StopForCars);
	if(bWillingToStop)
	{
		if(pVeh->m_nVehicleFlags.bIsRacingConservatively && !pOtherCar->m_nVehicleFlags.bIsRacingConservatively)
		{
			bWillingToStop = false;
		}
	}

	static dev_float s_fVelThresholdSqr = 1.0f * 1.0f;
	const float fDriveDirDotOtherVel = vehDriveDir.Dot(vOtherVel);
	const float fDriveDirDotOtherVelSqr = square(fDriveDirDotOtherVel);
	static dev_float s_fVelocityPercentThreshold = 0.6f;
	const float fOtherVelMag2Scaled = s_fVelocityPercentThreshold * s_fVelocityPercentThreshold * vOtherVelMag2;

	//if we aren't doing full avoidance, and the guy is moving perpendicular to us, we'd rather slow down
	if (bWillingToStop && vOtherVelMag2 > s_fVelThresholdSqr && fDriveDirDotOtherVelSqr < fOtherVelMag2Scaled)
	{
		//grcDebugDraw::Sphere(vecVehPosition, 1.5f, Color_red, false);
		return;
	}

	//sometimes while a vehicle is changing lane and coming towards us, the collision prediction (correctly)
	//assumes we'll collide in the future, but we actually won't as other vehicle will straighten up.
	//so we ignore vehicles that are changing lane, when we are happily driving in ours.
	if(fDebugCarsFacingDot < 0.0f && !obstructionData.bIsCurrentlyOnJunction)
	{
		const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();
		const CVehicleFollowRouteHelper* pOtherFollowRoute = pOtherCar->GetIntelligence()->GetFollowRouteHelper();

		if (pFollowRoute && pOtherFollowRoute && pOtherFollowRoute->CurrentRouteContainsLaneChange() )
		{
			s16 currentTargetPoint = pFollowRoute->GetCurrentTargetPoint();
			s16 otherCurrentTargetPoint = pOtherFollowRoute->GetCurrentTargetPoint();
			if(currentTargetPoint > 0 && otherCurrentTargetPoint > 0)
			{
				const CRoutePoint* pOtherPoint = &pOtherFollowRoute->GetRoutePoints()[otherCurrentTargetPoint];
				const CRoutePoint* pOtherLastPoint = &pOtherFollowRoute->GetRoutePoints()[otherCurrentTargetPoint - 1];
				//other vehicle is changing lane now
				if(rage::round(pOtherPoint->GetLane().Getf()) != rage::round(pOtherLastPoint->GetLane().Getf()))
				{
					const CRoutePoint* pPoint = &pFollowRoute->GetRoutePoints()[currentTargetPoint];
					const CRoutePoint* pLastPoint = &pFollowRoute->GetRoutePoints()[currentTargetPoint - 1];

					Vector3 laneDirection = VEC3V_TO_VECTOR3(pPoint->GetPosition() - pLastPoint->GetPosition());
					laneDirection.Normalize();

					float laneCentreOffset = pPoint->m_fCentralLaneWidth + rage::round(pPoint->GetLane().Getf()) * pPoint->m_fLaneWidth;
					Vector2 side = Vector2(pPoint->GetPosition().GetYf() -  pLastPoint->GetPosition().GetYf(),  pLastPoint->GetPosition().GetXf() - pPoint->GetPosition().GetXf());
					side.Normalize();
					Vector3 laneCentreOffsetVec((side.x * laneCentreOffset),(side.y * laneCentreOffset), 0.0f);
					//target point in our lane
					Vector3 laneTargetPos = VEC3V_TO_VECTOR3(pPoint->GetPosition()) + laneCentreOffsetVec;

					float distToTarget = geomDistances::DistanceLineToPoint(laneTargetPos, laneDirection, vecVehPosition);

					const static dev_float s_fLaneDotThreshold = 0.8f;
					//check we are driving along in our lane
					if( laneDirection.Dot(vehDriveDir) > s_fLaneDotThreshold && fabsf(distToTarget) < pPoint->m_fLaneWidth * 0.5f)
					{
						return;
					}
				}
			}
		}
	}

	//if we aren't doing full avoidance, and the guy is turning left across us at a junction,
	//prefer slowing down
	if (bWillingToStop && vOtherVelMag2 > s_fVelThresholdSqr && (DiffX * otherB.x + DiffY * otherB.y) < 0.0f
		&& (obstructionData.turnDir == BIT_TURN_STRAIGHT_ON || obstructionData.turnDir == BIT_NOT_SET)
		&& pOtherCar->m_nVehicleFlags.bTurningLeftAtJunction)
	{
		return;
	}

	//if someone's moving in the same direction as us, and going faster than us, don't avoid them
	if ((bWillingToStop || bUseRacingLogic) && vOtherVelMag2 >= 1.0f && vOtherVelMag2 > obstructionData.fCurrentSpeed * obstructionData.fCurrentSpeed)
	{
		Vector3 vOtherVelNorm = vOtherVel;
		vOtherVelNorm.NormalizeSafe(ORIGIN);
		if (vehDriveDir.Dot(vOtherVelNorm) > 0.7f)
		{
			//grcDebugDraw::Sphere(vecVehPosition, 1.5f, Color_orange, false);
			return;
		}
	}

	int numVerts = 3;
	Vec2f aiCarPoly[3];

	// Approximate the obstacle car as a triangle, using the nearest two sides.
	// If it has a velocity, sweep the triangle forward in time by N seconds to see where it will be, 
	// and stay out of this whole region. Then expand the region by the size of the 
	// pVeh car (actually just expand by the width of the pVeh car and some percentage of its length)

	Matrix34 otherVehMat = RCC_MATRIX34(pOtherCar->GetMatrixRef());
	spdAABB tempBox;
	const spdAABB& otherBox = pOtherCar->GetLocalSpaceBoundBox(tempBox);
	tempBox = otherBox;
	if(pOtherCar->GetIsHeli())
	{
		HelperGetHeliAvoidanceBounds(pOtherCar, tempBox);
	}

	const float minX = tempBox.GetMin().GetXf();
	const float maxX = tempBox.GetMax().GetXf();
	const float minY = tempBox.GetMin().GetYf();
	const float maxY = tempBox.GetMax().GetYf();

	float fTimePositionIsProjected=0.0f;
	if(!pVeh->m_nVehicleFlags.bDisableAvoidanceProjection)
	{
		Vector3 rvMyVel = pVeh->GetAiVelocity();
		float dodgyCombined = rvMyVel.Dot(rvMyVel - vOtherVel);
		fTimePositionIsProjected=5.0f;
		if (Abs(dodgyCombined) > SMALL_FLOAT)
		{
			float velDotDist = vecPositionDiff.Dot(rvMyVel);
			fTimePositionIsProjected = Clamp(velDotDist / dodgyCombined, 0.0f, fTimePositionIsProjected);
			otherVehMat.d += vOtherVel * fTimePositionIsProjected;
		}
	}

	Vector3 otherFrontLeftCorner;
	otherVehMat.Transform(Vector3(minX - s_fCarMarginOther, maxY + s_fCarMarginOther, 0.0f), otherFrontLeftCorner);
	Vector3 otherBumperVector;
	otherVehMat.Transform3x3(Vector3(maxX - minX + 2 * s_fCarMarginOther , 0.0f, 0.0f), otherBumperVector);
	Vector3 otherSideVector;
	otherVehMat.Transform3x3(Vector3(0.0f, maxY - minY + 2 * s_fCarMarginOther, 0.0f), otherSideVector);

	// The vert order here is selected so that we have clockwise winding orders
	if (otherSideVector.Dot(vecPositionDiff) < 0.0f) 		// Other car is facing AI car
	{
		if (otherBumperVector.Dot(vecPositionDiff) < 0.0f)	// Right side is closer
		{
			BuildCarPoly(aiCarPoly, otherFrontLeftCorner, otherFrontLeftCorner + otherBumperVector, otherFrontLeftCorner + otherBumperVector - otherSideVector);
		}
		else												// Left side is closer
		{
			BuildCarPoly(aiCarPoly, otherFrontLeftCorner, otherFrontLeftCorner + otherBumperVector, otherFrontLeftCorner - otherSideVector);
		}
	}
	else													// Other car is facing away from AI car
	{
		if (otherBumperVector.Dot(vecPositionDiff) < 0.0f)	// Right side is closer
		{
			BuildCarPoly(aiCarPoly, otherFrontLeftCorner - otherSideVector, otherFrontLeftCorner + otherBumperVector, otherFrontLeftCorner + otherBumperVector - otherSideVector);
		}
		else												// Left side is closer
		{
			BuildCarPoly(aiCarPoly, otherFrontLeftCorner, otherFrontLeftCorner + otherBumperVector - otherSideVector, otherFrontLeftCorner - otherSideVector);
		}
	}

	if (otherVehMat.c.z < 0.0f) { // vehicle is upside down
		SwapEm(aiCarPoly[0], aiCarPoly[1]); // need to reorder the verts in the triangle so that we still have a clockwise winding
	}

	const Matrix34& vehMat = RCC_MATRIX34(pVeh->GetMatrixRef());

	Vector3 aiCenter = vehMat.d;

	AvoidanceTaskObstructionData::ObstructionPoly& obsPoly = obstructionData.m_VehicleObstructions.Append();
	obsPoly.m_Count = 0;
	obsPoly.m_Vehicle = pOtherCar;
	if(pVeh->InheritsFromBike() && !bStationaryCar && pOtherCar->GetVehicleType() == VEHICLE_TYPE_CAR )
	{
		Vec3V vRelativeBumperPos = pOtherCar->GetTransform().UnTransform(obstructionData.vBumperCenter);
		m_bBumperInsideBoundsThisFrame |= otherBox.ContainsPoint(vRelativeBumperPos);
	}

	Vec2f aiCenter2d(aiCenter.x, aiCenter.y);
	Vec2f aiFrontLeftCorner2d(obstructionData.vFrontLeftCorner.GetXf(), obstructionData.vFrontLeftCorner.GetYf());
	Vec2f aiBumperVector2d(obstructionData.vBumperVector.GetXf(), obstructionData.vBumperVector.GetYf());
	Vec2f aiSideVector2d(obstructionData.vSideVector.GetXf(), obstructionData.vSideVector.GetYf());

	// Think of the two sweeps we're about to do as shrinking my car to a point while growing the other car.
	// To shrink my car we sweep along the bumper vector, then along the side vector. 
	// This will effectively shrink the whole car down to the point at the bottom right corner. So first offset the collision poly
	// by the distance from the bottom right corner to the car center, so that all tests can be done relative to the car center

	Vec2f aiBottomRightCorner2d = aiFrontLeftCorner2d + aiBumperVector2d + aiSideVector2d;
	OffsetPoly(aiCarPoly, numVerts, aiCenter2d - aiBottomRightCorner2d);

	// Is the obstacle car moving?
	if (vOtherVelMag2 > SMALL_FLOAT)
	{
		// Project it forward in time
		const bool bCanUseLongerProbe = pVeh->PopTypeIsMission() || pVeh->m_nVehicleFlags.bIsLawEnforcementVehicle || (pVeh->GetDriver() && pVeh->GetDriver()->PopTypeIsMission());
		const float fLookaheadTime = Max(obstructionData.GetAvoidanceLookaheadTime(bCanUseLongerProbe) - fTimePositionIsProjected, 0.0f);
		if (fLookaheadTime > 0.0f)
		{
			Vec2f vFuturePosOffset2d(vOtherVel.x * fLookaheadTime, vOtherVel.y * fLookaheadTime);

			Vec2f tempPoly0[10];
			Vec2f tempPoly1[10];

			geom2D::SweepPolygon(aiCarPoly, tempPoly0, numVerts, vFuturePosOffset2d); numVerts += 2;

			geom2D::SweepPolygon(tempPoly0, tempPoly1, numVerts, aiBumperVector2d); numVerts += 2;

			obsPoly.m_Count = numVerts + 2;
			geom2D::SweepPolygon(tempPoly1, &obsPoly.m_Verts[0], numVerts, aiSideVector2d);
		}
	}
	else
	{
		Vec2f tempPoly1[10];

		geom2D::SweepPolygon(aiCarPoly, tempPoly1, numVerts, aiBumperVector2d); numVerts += 2;

		obsPoly.m_Count = numVerts + 2;
		geom2D::SweepPolygon(tempPoly1, &obsPoly.m_Verts[0], numVerts, aiSideVector2d);

	}

	return;
}

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TestDirectionsAgainstSingleVehicle(const AvoidanceTaskObstructionData& obstructionData,
		CEntity* pEntity, bool bStationaryCar, bool bParkedCar, bool& bGiveWarning, AvoidanceInfo* avoidanceInfoArray, int numDirections, const bool bPreventAddMarginForOtherCar)
#else	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TestDirectionAgainstSingleVehicle(const AvoidanceTaskObstructionData& obstructionData, CEntity* pEntity, bool bStationaryCar, bool bParkedCar, bool& bGiveWarning, AvoidanceInfo& avoidanceInfo, const bool bPreventAddMarginForOtherCar)
#endif	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
{
	TUNE_GROUP_BOOL( FOLLOW_PATH_AI_STEER, RENDER_CAR_AVOIDANCE_DETAIL, false );

	const CVehicle* pVeh = obstructionData.pVeh;

	if(obstructionData.carAvoidanceLevel == CAR_AVOIDANCE_NONE && !bParkedCar)
	{
		return false;
	}

	Assert(pEntity);
	Assert(pVeh);
	//	float	oldLeftAngle = *pLeftAngle;
	//	float	oldRightAngle = *pRightAngle;
	//	bool	bFoundWayAroundLeft = false, bFoundWayAroundRight = false;
	bool	bHasChanged = false;

	CVehicle *pOtherCar =(CVehicle *)pEntity;
	//static dev_float s_fCarMarginUs = 0.0f;
	
	float s_fCarMarginOther = !bPreventAddMarginForOtherCar 
		? CalculateOtherVehicleMarginForAvoidance(*pVeh, *pOtherCar, bGiveWarning)
		: 0.0f;

	if ((pOtherCar->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle
		|| pOtherCar->m_nVehicleFlags.bIsAmbulanceOnDuty
		|| pOtherCar->m_nVehicleFlags.bIsFireTruckOnDuty)
		&& obstructionData.bWillStopForCars)
	{
		return false;
	}

	const Vector3 vecVehPosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	const Vector3 vecOtherCarPosition = VEC3V_TO_VECTOR3(pOtherCar->GetVehiclePosition());
	const float DiffX = vecOtherCarPosition.x - vecVehPosition.x;
	const float DiffY = vecOtherCarPosition.y - vecVehPosition.y;

	// Get the current drive direction and orientation.
	Vector3 vehDriveDir = RCC_VECTOR3(obstructionData.vNormalizedDriveDir);

	// If the other car is already behind us there is no need trying to avoid it.
	if(DiffX * vehDriveDir.x + DiffY * vehDriveDir.y < obstructionData.vBoundBoxMin.GetYf()) 
	{
		return false;
	}

	// If the other car is attached to a trailer (riding on top of it),
	// don't avoid it
	if (CVehicle::IsEntityAttachedToTrailer(pEntity))
	{
		return false;
	}

	// If we are not allowed to go crazy an the avoidance and the other car is in front of us facing away
	// from us and not stuck we don't want to try and avoid us;
	// It is probably just traffic.
	if((obstructionData.carAvoidanceLevel == CAR_AVOIDANCE_LITTLE) && !bStationaryCar)
	{
		Vector3 otherB = VEC3V_TO_VECTOR3(pOtherCar->GetVehicleForwardDirection());
		otherB.z = 0.0f;

		// If the cars face roughly the same direction.
		const float fDebugCarsFacingDot = vehDriveDir.Dot(otherB);
		static dev_float s_fCarsFacingDotThreshold = 0.6f;
		const float fAngleThresholdInRads = rage::Acosf(s_fCarsFacingDotThreshold);
		const float fOtherOrientation = fwAngle::LimitRadianAngle(fwAngle::GetATanOfXY(otherB.x, otherB.y));
		const float fThetaBetweenOurDesiredOtherActual = SubtractAngleShorter(obstructionData.vehDriveOrientation, fOtherOrientation);
		if(fDebugCarsFacingDot > s_fCarsFacingDotThreshold || fThetaBetweenOurDesiredOtherActual < fAngleThresholdInRads)
		{
			// If the obstacle car is right in front of us.
			Vector3 diffNorm = Vector3(DiffX, DiffY, 0.0f);
			diffNorm.Normalize();
			const float fDebugOtherCarPositionDeltaDot = diffNorm.Dot(vehDriveDir);
			static dev_float s_fOtherCarPositionDeltaDotThreshold = 0.6f;
			if(fDebugOtherCarPositionDeltaDot > s_fOtherCarPositionDeltaDotThreshold)
			{
				// Don't bother avoiding this car. Instead we want to pull
				// up to the car and stop.
				return false;
			}
		}
	}

	bool bWillingToStop = IsDrivingFlagSet(DF_StopForCars);
	if(bWillingToStop)
	{
		if(pVeh->m_nVehicleFlags.bIsRacingConservatively && !pOtherCar->m_nVehicleFlags.bIsRacingConservatively)
		{
			bWillingToStop = false;
		}
	}

	const bool bUseRacingLogic = HelperShouldUseRacingLogic(obstructionData.bRacing, pVeh->InheritsFromBike(), obstructionData.fCurrentSpeed, pOtherCar);
	const Vector3 vOtherVel = !bUseRacingLogic ? pOtherCar->GetAiVelocity() 
		: HelperGetOtherVehsRacingVelocity(pOtherCar);
	const float vOtherVelMag2 = vOtherVel.Mag2();
	static dev_float s_fVelThresholdSqr = 1.0f * 1.0f;

	//if we aren't doing full avoidance, and the guy is moving perpendicular to us, we'd rather slow down
	if (bWillingToStop && vOtherVelMag2 > s_fVelThresholdSqr && square(vehDriveDir.Dot(vOtherVel)) < 0.5f * 0.5f * vOtherVelMag2)
	{
		//grcDebugDraw::Sphere(vecVehPosition, 1.5f, Color_red, false);
		return false;
	}

	//if someone's moving in the same direction as us, and going faster than us, don't avoid them
	if ((bWillingToStop || bUseRacingLogic) && vOtherVelMag2 >= 1.0f && vOtherVelMag2 > obstructionData.fCurrentSpeed * obstructionData.fCurrentSpeed)
	{
		Vector3 vOtherVelNorm = vOtherVel;
		vOtherVelNorm.NormalizeSafe(ORIGIN);
		if (vehDriveDir.Dot(vOtherVelNorm) > 0.7f)
		{
			//grcDebugDraw::Sphere(vecVehPosition, 1.5f, Color_orange, false);
			return false;
		}
	}

	// Rather than looking at the car in it's current position we estimate when we will collide
	// with it. We then take his coordinates at that point in time.
	//	Vector3 otherCarPos = pOtherCar->GetPosition();

#if !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	const Vector3& vBoundBoxMax = RCC_VECTOR3(obstructionData.vBoundBoxMax);
#endif	// !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

	// Find the two lines in the ai car closest to the Other car.
	Vector3	Line1Start, Line2Start, Line1Delta, Line2Delta;
	float DotProd1 = vehDriveDir.x * DiffX + vehDriveDir.y * DiffY;
	Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()));
	float DotProd2 = VehRightVector.x * DiffX + VehRightVector.y * DiffY;

	bool bChoseFront = false;
	if(DotProd1 > 0.0f)
	{	// Front bumper line
		Line1Start = RCC_VECTOR3(obstructionData.vFrontLeftCorner);
		bChoseFront = true;
	}
	else
	{	// Rear bumper line
		Line1Start = RCC_VECTOR3(obstructionData.vRearLeftCorner);
	}
	Line1Delta = RCC_VECTOR3(obstructionData.vBumperVector);

	if(DotProd2 > 0.0f)
	{	// Right side of car line

		// ChoseFront:
		//		x = maxx + margin
		//		y = maxy - dist - margin
		// fl + bumper + side:
		//		x = (minx - margin) + (maxx - minx + 2margin) + (0) = maxx + margin
		//		y = (maxy + margin) + (0) + -(dist + 2margin) = maxy - dist - margin

		// !ChoseFront:
		//		x = maxx + margin
		//		y = miny + dist + margin  
		// rl + bumper - side			(the -side seems wrong, unless we change the Line2Delta also)
		//		x = (minx - margin) + (maxx - minx + 2margin) + (0) = maxx + margin
		//		y = (miny - margin) + (0) - -(dist + 2margin) = miny + dist + margin

		//Line2Start = pVeh->TransformIntoWorldSpace(Vector3(vBoundBoxMax.x + s_fCarMarginUs
		//	, (bChoseFront ? vBoundBoxMax.y - fDistFromBumper -s_fCarMarginUs 
		//	: vBoundBoxMin.y + fDistFromBumper + s_fCarMarginUs)
		//	, 0.0f));

		// I don't think the above tests are right. Seems to do the Line2 rear test from the middle of the car going forward
		// This makes more sense to me:
		Line2Start = Line1Start + Line1Delta; // (front right or rear right)
		
	}
	else
	{	// Left side of car line

		// ChoseFront:
		//		x = minx - margin
		//		y = maxy - dist - margin
		// fl + side
		//		x = (minx - margin) + (0)
		//		y = (maxy + margin) + -(dist + 2margin) = maxy - dist - margin
		
		// !ChoseFront:
		//		x = minx - margin
		//		y = miny + dist + margin
		// rl - side
		//		x = minx - margin + 0
		//		y = (miny - margin) - -(dist + 2margin) = miny + dist + margin

		//old code:
		//Line2Start = pVeh->TransformIntoWorldSpace(Vector3(vBoundBoxMin.x - s_fCarMarginUs
		//	, (bChoseFront ? vBoundBoxMax.y - fDistFromBumper -s_fCarMarginUs 
		//	: vBoundBoxMin.y + fDistFromBumper + s_fCarMarginUs)
		//	, 0.0f));				 
		
		Line2Start = Line1Start;
	}

	Line2Delta = bChoseFront ? VEC3V_TO_VECTOR3(obstructionData.vSideVector) : VEC3V_TO_VECTOR3(-obstructionData.vSideVector); // backward from front, or forward from back

	//grcDebugDraw::Line(Line1Start, Line1Start+Line1Delta, Color_green);
	//grcDebugDraw::Line(Line2Start, Line2Start+Line2Delta, Color_green);

	//Line2Delta = VEC3V_TO_VECTOR3(Negate(obstructionData.vSideVector));
	//vehMat.Transform3x3(Vector3(0.0f
	//	, (fDistFromBumper + 2.0f * s_fCarMarginUs)
	//	, 0.0f)
	//	, Line2Delta);


	// Find the two lines in the other car that are closest to the ai car.
	Vector3	Line1Start_Other, Line2Start_Other, Line1Delta_Other, Line2Delta_Other;
	Vector3 OtherCarRightVector(VEC3V_TO_VECTOR3(pOtherCar->GetVehicleRightDirection()));
	Vector3 OtherCarForwardVector(VEC3V_TO_VECTOR3(pOtherCar->GetVehicleForwardDirection()));

	DotProd1 = OtherCarForwardVector.x * DiffX + OtherCarForwardVector.y * DiffY;
	DotProd2 = OtherCarRightVector.x * DiffX + OtherCarRightVector.y * DiffY;

	spdAABB tempBox;
	const spdAABB& otherBox = pOtherCar->GetLocalSpaceBoundBox(tempBox);
	const float minX = otherBox.GetMin().GetXf();
	const float maxX = otherBox.GetMax().GetXf();
	const float minY = otherBox.GetMin().GetYf();
	const float maxY = otherBox.GetMax().GetYf();

	if(DotProd1 < 0.0f)
	{	// Front bumper line
		Line1Start_Other = pOtherCar->TransformIntoWorldSpace(Vector3(minX - s_fCarMarginOther, maxY + s_fCarMarginOther, 0.0f));
	}
	else
	{	// Rear bumper line
		Line1Start_Other = pOtherCar->TransformIntoWorldSpace(Vector3(minX - s_fCarMarginOther, minY - s_fCarMarginOther, 0.0f));
	}
	Line1Delta_Other = VEC3V_TO_VECTOR3(pOtherCar->GetTransform().Transform3x3(Vec3V(maxX - minX + 2 * s_fCarMarginOther, 0.0f, 0.0f)));

	if(DotProd2 < 0.0f)
	{	// Right side of car line
		Line2Start_Other = pOtherCar->TransformIntoWorldSpace(Vector3(maxX + s_fCarMarginOther, minY - s_fCarMarginOther, 0.0f));
	}
	else
	{	// Left side of car line
		Line2Start_Other = pOtherCar->TransformIntoWorldSpace(Vector3(minX - s_fCarMarginOther, minY - s_fCarMarginOther, 0.0f));
	}
	Line2Delta_Other = VEC3V_TO_VECTOR3(pOtherCar->GetTransform().Transform3x3(Vec3V(0.0f, maxY - minY + 2 * s_fCarMarginOther, 0.0f)));

	//grcDebugDraw::Line(Line1Start_Other, Line1Start_Other+Line1Delta_Other, Color_blue);
	//grcDebugDraw::Line(Line2Start_Other, Line2Start_Other+Line2Delta_Other, Color_blue);

#if !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	// Narrow vehicles(bikes) will not detect hitting wider vehicles sometimes.
	// This is why we have to change the order of the tests for them.
	bool bSwapRound = false;
	if(vBoundBoxMax.x <
		maxX)
	{
		bSwapRound = true;
	}
#endif	// !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	// Scale up the vehicles velocity so vehicles look further ahead when considering vehicles for avoidance.
	// This is to fix issues where vehicles have closely matched speeds.
	//static dev_float OUT_SPEED_SCALE = 1.5f;
	//float OurMoveSpeed = GetCruiseSpeed();//rage::Sqrtf(pVeh->GetAiVelocity().x * pVeh->GetAiVelocity().x + pVeh->GetAiVelocity().y * pVeh->GetAiVelocity().y)*OUT_SPEED_SCALE;
	float OurMoveSpeed = obstructionData.fCurrentSpeed;
	OurMoveSpeed = rage::Max(pVeh->GetAiVelocity().XYMag(), 10.0f);

	const bool bCanUseLongerProbe = pVeh->PopTypeIsMission() || pVeh->m_nVehicleFlags.bIsLawEnforcementVehicle || (pVeh->GetDriver() && pVeh->GetDriver()->PopTypeIsMission());
	const float fLookaheadTime = obstructionData.GetAvoidanceLookaheadTime(bCanUseLongerProbe);

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	Assert(numDirections <= kMaxObstructionProbeDirections);
	static const int kMaxVecs = kMaxObstructionProbeDirections/4;
	CompileTimeAssert(kMaxVecs*4 == kMaxObstructionProbeDirections);
	Vec2V testDirXYArray[kMaxVecs*4];

	const int lastDir = numDirections - 1;
	for(int j = 0, k = 0; j < numDirections; j += 4, k++)
	{
		// Note: the Min() stuff here is a bit messy, but reading past the array passed in
		// by the user would be a bit risky.
		const AvoidanceInfo& avoidanceInfo0 = avoidanceInfoArray[j];
		const AvoidanceInfo& avoidanceInfo1 = avoidanceInfoArray[Min(j + 1, lastDir)];
		const AvoidanceInfo& avoidanceInfo2 = avoidanceInfoArray[Min(j + 2, lastDir)];
		const AvoidanceInfo& avoidanceInfo3 = avoidanceInfoArray[Min(j + 3, lastDir)];

		// Load up the four directions in vector registers.
		const ScalarV dir0V = LoadScalar32IntoScalarV(avoidanceInfo0.fDirection);
		const ScalarV dir1V = LoadScalar32IntoScalarV(avoidanceInfo1.fDirection);
		const ScalarV dir2V = LoadScalar32IntoScalarV(avoidanceInfo2.fDirection);
		const ScalarV dir3V = LoadScalar32IntoScalarV(avoidanceInfo3.fDirection);

		// Put them together in one vector, and compute the angle we want to compute cos and sin for.
		const Vec4V dirV(dir0V, dir1V, dir2V, dir3V);

		// Compute the sin and cos values of the four angles.
		Vec4V sinV, cosV;
		SinAndCos(sinV, cosV, dirV);

		const Vec4V zeroV(V_ZERO);
		const Vec4V temp0V = MergeXY(cosV, zeroV);
		const Vec4V temp1V = MergeXY(sinV, zeroV);
		const Vec4V temp2V = MergeZW(cosV, zeroV);
		const Vec4V temp3V = MergeZW(sinV, zeroV);
		const Vec4V xy00AV = MergeXY(temp0V, temp1V);
		const Vec4V xy00BV = MergeZW(temp0V, temp1V);
		const Vec4V xy00CV = MergeXY(temp2V, temp3V);
		const Vec4V xy00DV = MergeZW(temp2V, temp3V);

		testDirXYArray[j    ] = xy00AV.GetXY();
		testDirXYArray[j + 1] = xy00BV.GetXY();
		testDirXYArray[j + 2] = xy00CV.GetXY();
		testDirXYArray[j + 3] = xy00DV.GetXY();
	}

	//const Vec3V otherCarVelV = VECTOR3_TO_VEC3V(pOtherCar->GetAiVelocity());
	bool hitAngleArray[kMaxObstructionProbeDirections];
	bool gotHits = false;
	for(int i = 0; i < numDirections; i++)
	{
		const bool hitThisAngle = TestForThisAngleNew(testDirXYArray[i],
				VECTOR3_TO_VEC3V(Line1Start).GetXY(), VECTOR3_TO_VEC3V(Line1Delta).GetXY(),
				VECTOR3_TO_VEC3V(Line2Start).GetXY(), VECTOR3_TO_VEC3V(Line2Delta).GetXY(),
				VECTOR3_TO_VEC3V(Line1Start_Other).GetXY(), VECTOR3_TO_VEC3V(Line1Delta_Other).GetXY(),
				VECTOR3_TO_VEC3V(Line2Start_Other).GetXY(), VECTOR3_TO_VEC3V(Line2Delta_Other).GetXY(),
				pOtherCar->GetAiVelocity().x, pOtherCar->GetAiVelocity().y,
				OurMoveSpeed, fLookaheadTime);
		hitAngleArray[i] = hitThisAngle;
		gotHits |= hitThisAngle;
	}

	if(gotHits)
	{
		const float fDistToOtherCar = (Line1Start - vecOtherCarPosition).XYMag();
		const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_Vehicle, fDistToOtherCar, pVeh->GetAiVelocity(), vOtherVel, obstructionData.fCurrentSpeed);

		const Vector3 vDirToObstruction = vecOtherCarPosition - Line1Start;
		const float dirToObstruction = fwAngle::LimitRadianAngle(fwAngle::GetATanOfXY(vDirToObstruction.x, vDirToObstruction.y));

		const bool shouldGiveWarningIfChanged = ShouldGiveWarning(pVeh, pOtherCar);

		for(int i = 0; i < numDirections; i++)
		{
			if (hitAngleArray[i])
			{
				AvoidanceInfo& avoidanceInfo = avoidanceInfoArray[i];

				//update this avoidanceInfo struct
				if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
				{
					avoidanceInfo.fDistToObstruction = fDistToOtherCar;
					avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_Vehicle;
					avoidanceInfo.vVelocity = pOtherCar->GetAiVelocity();
					avoidanceInfo.vPosition = vecOtherCarPosition;
					avoidanceInfo.fDirToObstruction = dirToObstruction;
					avoidanceInfo.fObstructionScore = fObstructionScore;
					avoidanceInfo.bIsStationaryCar = bStationaryCar;
					avoidanceInfo.bIsObstructedByLaw = false;
					avoidanceInfo.bIsTurningLeftAtJunction = pOtherCar->m_nVehicleFlags.bTurningLeftAtJunction || pOtherCar->m_nVehicleFlags.bIsWaitingToTurnLeft;
					avoidanceInfo.bIsHesitating = pOtherCar->m_nVehicleFlags.bIsHesitating;

					if(shouldGiveWarningIfChanged)
					{
						avoidanceInfo.bGiveWarning = true;
					}
					bHasChanged = true;
				}
			}
		}

		// See if we want to flash the lights
		if(bHasChanged)
		{
			// Possibly flash the headlights, etc.
			if(shouldGiveWarningIfChanged)
			{
				bGiveWarning = true;
				m_vCachedGiveWarningPosition = VEC3V_TO_VECTOR3(pOtherCar->GetVehiclePosition());
			}
		}

	}
#else
	if (TestForThisAngle(avoidanceInfo.fDirection, &Line1Start, &Line1Delta, &Line2Start, &Line2Delta, &Line1Start_Other, &Line1Delta_Other, &Line2Start_Other, &Line2Delta_Other, pOtherCar->GetAiVelocity().x, pOtherCar->GetAiVelocity().y, OurMoveSpeed, bSwapRound, fLookaheadTime))
	{
		//update this avoidanceInfo struct
		const float fDistToOtherCar = (Line1Start - vecOtherCarPosition).XYMag();
		const float fObstructionScore = ScoreOneObstructionAngleIndependent(AvoidanceInfo::Obstruction_Vehicle, fDistToOtherCar, pVeh->GetAiVelocity(), vOtherVel, obstructionData.fCurrentSpeed);
		if (!avoidanceInfo.HasObstruction() || fObstructionScore > avoidanceInfo.fObstructionScore)
		{
			avoidanceInfo.fDistToObstruction = fDistToOtherCar;
			avoidanceInfo.ObstructionType = AvoidanceInfo::Obstruction_Vehicle;
			avoidanceInfo.vVelocity = pOtherCar->GetAiVelocity();
			avoidanceInfo.vPosition = vecOtherCarPosition;
			Vector3 vDirToObstruction = vecOtherCarPosition - Line1Start;
			avoidanceInfo.fDirToObstruction = fwAngle::GetATanOfXY(vDirToObstruction.x, vDirToObstruction.y);
			avoidanceInfo.fDirToObstruction = fwAngle::LimitRadianAngle(avoidanceInfo.fDirToObstruction);
			avoidanceInfo.fObstructionScore = fObstructionScore;
			avoidanceInfo.bIsStationaryCar = bStationaryCar;
			avoidanceInfo.bIsObstructedByLaw = false;
			avoidanceInfo.bIsTurningLeftAtJunction = pOtherCar->m_nVehicleFlags.bTurningLeftAtJunction || pOtherCar->m_nVehicleFlags.bIsWaitingToTurnLeft;
			avoidanceInfo.bIsHesitating = pOtherCar->m_nVehicleFlags.bIsHesitating;

			bHasChanged = true;
		}
	}

	// See if we want to flash the lights
	if(bHasChanged)
	{
		// Possibly flash the headlights, etc.
		if(ShouldGiveWarning(pVeh, pOtherCar))
		{
			avoidanceInfo.bGiveWarning = true;
			bGiveWarning = true;
			m_vCachedGiveWarningPosition = VEC3V_TO_VECTOR3(pOtherCar->GetVehiclePosition());
		}
	}
#endif	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

#if __BANK
	if( RENDER_CAR_AVOIDANCE_DETAIL && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()) + ZAXIS*4.0f, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) + ZAXIS*4.0f, Color_red, Color_red);

#if 0
		// Do some vectormap rendering of the collisions we're doing
		fwVectorMap::DrawLine(Line1Start, Line1Start + Line1Delta, Color_BlueViolet, Color_BlueViolet);
		fwVectorMap::DrawLine(Line2Start, Line2Start + Line2Delta, Color_BlueViolet, Color_BlueViolet);
		fwVectorMap::DrawLine(Line1Start_Other, Line1Start_Other + Line1Delta_Other, Color_burlywood, Color_burlywood);
		fwVectorMap::DrawLine(Line2Start_Other, Line2Start_Other + Line2Delta_Other, Color_burlywood, Color_burlywood);

		// Now try out the sweeps and see if they will work?
		Vector3 lookAhead = vOtherVel;
		lookAhead.Scale(fLookaheadTime);
		Vec2f lookAhead2d(lookAhead.x, lookAhead.y);

		Vec2f occludedAreaPoly0[10];
		Vec2f occludedAreaPoly1[10];
		Vec2f occludedAreaPoly2[10];

		// This is annoying, the lines don't go in any particularly useful order
		Vector3 corner0, corner1, corner2;
		if (DotProd1 < 0.0f)
		{
			// Line1 is the front
			corner0 = Line1Start_Other;
			corner1 = Line1Start_Other + Line1Delta_Other;
			corner2 = Line2Start_Other;
		}
		else
		{
			// Line1 is the back
			corner0 = Line2Start_Other + Line2Delta_Other;
			corner1 = Line1Start_Other + Line1Delta_Other;
			corner2 = Line1Start_Other;
		}

		occludedAreaPoly0[0] = Vec2f(corner0.x, corner0.y);
		occludedAreaPoly0[1] = Vec2f(corner1.x, corner1.y);
		occludedAreaPoly0[2] = Vec2f(corner2.x, corner2.y);

		Vec2f bumperDir(Line1Delta.x, Line1Delta.y);
		Vec2f halfBumperDir = bumperDir * Vec2f(0.5f, 0.5f);
		Vec2f sideDir(Line2Delta.x, Line2Delta.y);

		// offset by half bumper dir
		for(int i = 0; i < 3; i++)
		{
			occludedAreaPoly0[i] = occludedAreaPoly0[i] - halfBumperDir;
		}

		int numVerts = 3;
		if (lookAhead.Mag2() > SMALL_FLOAT)
		{
			geom2D::SweepPolygon(occludedAreaPoly0, occludedAreaPoly1, numVerts, bumperDir); numVerts += 2;// -> 5-gon
			geom2D::SweepPolygon(occludedAreaPoly1, occludedAreaPoly2, numVerts, sideDir); numVerts += 2;  // -> 7-gon
			geom2D::SweepPolygon(occludedAreaPoly2, occludedAreaPoly0, numVerts, lookAhead2d); numVerts += 2;// -> 9-gon
		}
		else
		{
			geom2D::SweepPolygon(occludedAreaPoly0, occludedAreaPoly1, numVerts, bumperDir); numVerts += 2;// -> 5-gon
			geom2D::SweepPolygon(occludedAreaPoly1, occludedAreaPoly0, numVerts, sideDir); numVerts += 2;  // -> 7-gon
		}

		for(int i = 0; i < numVerts; i++)
		{
			fwVectorMap::DrawLine(occludedAreaPoly0[i], occludedAreaPoly0[(i+1)%numVerts], Color_LightSalmon, Color_LightSalmon);
		}

		// Check for a collision
		Vec2f probeStart = Vec2f(Line1Start.x, Line1Start.y) + halfBumperDir;

		float	OurSpeedX = OurMoveSpeed * rage::Cosf(avoidanceInfo.fDirection);
		float	OurSpeedY = OurMoveSpeed * rage::Sinf(avoidanceInfo.fDirection);

		Vec2f probeDir(OurSpeedX * fLookaheadTime, OurSpeedY * fLookaheadTime);
		Vec2f probeEnd = probeStart + probeDir;

		float tEnter, tLeave;
		bool hitSomething = geom2D::Test2DSegVsPolygon(tEnter, tLeave, probeStart, probeEnd, occludedAreaPoly0, numVerts);

		if (hitSomething)
		{
			fwVectorMap::DrawLine(probeStart, probeEnd, Color_orange, Color_orange);
			Vec2f isectPoint = Lerp(tEnter, probeStart, probeEnd);
			fwVectorMap::DrawMarker(Vector3(isectPoint.GetX(), isectPoint.GetY(), 0.0f), Color_red, 0.5f);
		}
		else
		{
			fwVectorMap::DrawLine(probeStart, probeEnd, Color_blue, Color_blue);
		}
#endif
	}
#endif // __BANK

#if __DEV

	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && bHasChanged && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
 		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()) + Vector3(0.0f,0.0f,1.0f),
 			VEC3V_TO_VECTOR3(pOtherCar->GetVehiclePosition()) + Vector3(0.0f,0.0f,1.0f),
 			Color32(100, 0, 200, 255));

		grcDebugDraw::Line(Line1Start + Vector3(0.0f,0.0f,2.0f),
			Line1Start + Line1Delta + Vector3(0.0f,0.0f,2.0f),
			Color32(100, 100, 200, 255));
		grcDebugDraw::Line(Line2Start + Vector3(0.0f,0.0f,2.0f),
			Line2Start + Line2Delta + Vector3(0.0f,0.0f,2.0f),
			Color32(100, 100, 200, 255));

		grcDebugDraw::Line(Line1Start_Other + Vector3(0.0f,0.0f,2.0f),
			Line1Start_Other + Line1Delta_Other + Vector3(0.0f,0.0f,2.0f),
			Color32(100, 100, 200, 255));
		grcDebugDraw::Line(Line2Start_Other + Vector3(0.0f,0.0f,2.0f),
			Line2Start_Other + Line2Delta_Other + Vector3(0.0f,0.0f,2.0f),
			Color32(100, 100, 200, 255));
	}
#endif //__DEV

	return bHasChanged;
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::AddObstructionForSinglePed(AvoidanceTaskObstructionData& obstructionData, CEntity* pEntity)
{
	Assert(pEntity);
	Assert(pEntity->GetIsTypePed());
	Assert(obstructionData.pVeh);

	TUNE_GROUP_FLOAT(VEH_AI_AVOIDANCE, pedAvoidanceScale, 0.4f, 0.0f, 10.0f, 0.01f);

	// There are some conditions in which we don't want to avoid the ped
	if(IsDrivingFlagSet(DF_DontSteerAroundPlayerPed) && pEntity == FindPlayerPed())
	{
		return;
	}

	// Use the coordinates of our front bumper.
	Vector3 bumperCrs = RCC_VECTOR3(obstructionData.vBumperCenter);

	const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	const float DiffX = vecEntityPosition.x - bumperCrs.x; //pVeh->GetPosition().x;
	const float DiffY = vecEntityPosition.y - bumperCrs.y; //pVeh->GetPosition().y;
	// Angle to other car
	const float Orientation = fwAngle::GetATanOfXY(DiffX, DiffY);
	// Distance to other car
	const float Distance = rage::Sqrtf(DiffX*DiffX + DiffY*DiffY);

	const float fCloseDistThreshold = !obstructionData.pVeh->InheritsFromBike() ? 1.0f : 0.25f;
	if(Distance < fCloseDistThreshold) 
	{
		return;	// Ped is too close. Probably standing on roof. Want to avoid div by zero below.
	}

	//Don't avoid anything that's further away than we want to check.
	//Slow moving cars care way too much about peds and objects otherwise
	const float fLookaheadTime = obstructionData.GetAvoidanceLookaheadTime(false);
	const float averagedSpeed = (obstructionData.fDesiredSpeed + obstructionData.fCurrentSpeed) * 0.5f;
	if (Distance > averagedSpeed * fLookaheadTime)
	{
		return;
	}

	// Calculated how much of the angles are blocked
	// Work out the proper steering angle.
	// Radius of a ped and a bit more + allow for the size of our
	// car(so that we steer wide a bit further)
	// static dev_float s_fExtraDistance = 0.0f;
	const float fExtraWidth = !obstructionData.pVeh->InheritsFromBike() ? ms_fExtraWidthForCars : ms_fExtraWidthForBikes;
	const CPed* pPed = static_cast<CPed*>(pEntity);

	float fProjectedSizeBase = pPed->GetIsDeadOrDying() ? 1.1f : 0.8f;
	if (pPed == m_Params.GetTargetEntity().GetEntity())
	{
		fProjectedSizeBase *= m_Params.GetTargetObstructionSizeModifier();
	}
	const float ProjectedSize = fProjectedSizeBase + ((obstructionData.vBoundBoxMax.GetXf() + fExtraWidth) * 2.4f);
	//const float halfBlockedAngleRange = 2.0f * rage::Atan2f(ProjectedSize, Distance + s_fExtraDistance) * pedAvoidanceScale;
	const float halfBlockedAngleRange = (ProjectedSize / Distance) * pedAvoidanceScale;

	AvoidanceTaskObstructionData::ObstructionCircle& circle = obstructionData.m_PedAndObjectObstructions.Append();
	circle.m_Distance = Distance;
	circle.m_Orientation = Orientation;
	circle.m_HalfBlockedAngleRange = halfBlockedAngleRange;
	circle.m_Entity = pEntity;
	circle.m_ObstructionType = AvoidanceInfo::Obstruction_Ped;
}



bool HelperObjectIsAFence(const CObject* pObject)
{
	static const int s_iNumFences = 6;
	static const u32 fences[s_iNumFences] = 
	{
		ATSTRINGHASH("prop_fnc_farm_01a", 0xf2e78a52),
		ATSTRINGHASH("prop_fnc_farm_01b", 0x5972fb1),
		ATSTRINGHASH("prop_fnc_farm_01c", 0x4ed9c235),
		ATSTRINGHASH("prop_fnc_farm_01d", 0xb9c69815),
		ATSTRINGHASH("prop_fnc_farm_01e", 0xcc003c88),
		ATSTRINGHASH("prop_fnc_farm_01f", 0x1649d11a)
	};

	if (!pObject->GetBaseModelInfo() )
	{
		return false;
	}

	const u32 modelNameHash = pObject->GetBaseModelInfo()->GetModelNameHash();

	for (int i = 0; i < s_iNumFences; i++)
	{
		if (modelNameHash == fences[i])
		{
			return true;
		}
	}

	return false;
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::AddObstructionForSingleObject(AvoidanceTaskObstructionData& obstructionData, CObject* pEntity)
{
	phBound *pBound = NULL;
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if(pModelInfo->GetFragType())
	{
		pBound =(phBound *)pModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();
	}
	else if(pModelInfo->GetPhysics())
	{
		phArchetype *pArchetype = pModelInfo->GetPhysics();
		pBound = pArchetype->GetBound();
	}

	if(!pBound)
	{
		return;
	}

	fwDynamicArchetypeComponent* pDynamicComponent = pModelInfo->GetDynamicComponentPtr();
	if(!pDynamicComponent)
	{
		//the following pool is not thread safe
		AI_AVOIDANCE_POOL_LOCK
		if(!fwDynamicArchetypeComponent::_ms_pPool->IsFull())
		{
			pDynamicComponent = &pModelInfo->CreateDynamicComponentIfMissing();
		}
	}

	if(!pDynamicComponent)
	{
		return;
	}

	Vec4V avoidancePoint = pDynamicComponent->GetAvoidancePoint();
	if(IsEqualAll(avoidancePoint, Vec4V(V_FLT_MAX))) //created as V_FLT_MAX = not yet checked
	{
		avoidancePoint = Vec4V(V_NEG_FLT_MAX); //assign to V_NEG_FLT_MAX to prevent checking multiple times
		if (!HelperObjectIsAFence(pEntity))
		{
			FindAvoidancePoints(pBound, avoidancePoint);
		}
		pDynamicComponent->SetAvoidancePoint(avoidancePoint);
	}

	//if avoidancePoint equals V_NEG_FLT_MAX then it's been checked and avoidance isn't required
	if(IsEqualAll(avoidancePoint, Vec4V(V_NEG_FLT_MAX)))
	{			
		return;
	}

	// Work out world coordinates of object
	Vector3 CollPoint = pEntity->TransformIntoWorldSpace(VEC3V_TO_VECTOR3(avoidancePoint.GetXYZ()));

	// Use the coordinates of our front bumper.
	Vector3 bumperCrs = RCC_VECTOR3(obstructionData.vBumperCenter);

	float DiffX = CollPoint.x - bumperCrs.x; //pVeh->GetPosition().x;
	float DiffY = CollPoint.y - bumperCrs.y; //pVeh->GetPosition().y;
	// Angle to object
	float Orientation = fwAngle::GetATanOfXY(DiffX, DiffY);
	
	
	// Distance to other car
	// Take into account its size
	const float Distance = rage::Sqrtf(DiffX*DiffX + DiffY*DiffY);
	const float fDistanceMinusRadius = rage::Max(0.0f, Distance - avoidancePoint.GetWf());

	//Don't avoid anything that's further away than we want to check.
	//Slow moving cars care way too much about peds and objects otherwise
	const float fLookaheadTime = obstructionData.GetAvoidanceLookaheadTime(false);
	const float averagedSpeed = (obstructionData.fDesiredSpeed + obstructionData.fCurrentSpeed) * 0.5f;
	if (fDistanceMinusRadius > averagedSpeed * fLookaheadTime)
	{
		return;
	}

	// Calculated how much of the angles are blocked
	// Work out the proper steering angle.

	TUNE_GROUP_FLOAT( FOLLOW_PATH_AI_STEER, sfProjectedSizeScaler, 0.75f, 0.0f, 10.0f, 0.1f);
	float fSizeScaler = 1.0f;
	if( m_threePointTurnInfo.m_iNumRecentThreePointTurns >= m_threePointTurnInfo.ms_iMaxAttempts)
	{
		fSizeScaler = Max(0.1f, sfProjectedSizeScaler - ( m_threePointTurnInfo.m_iNumRecentThreePointTurns - m_threePointTurnInfo.ms_iMaxAttempts) * 0.1f);
	}

	static dev_float s_fExtraProjectedSize = 0.2f;
	float ProjectedSize = s_fExtraProjectedSize + avoidancePoint.GetWf();	// Radius of a pole and a bit more

	// Allow for the size of our car(so that we steer wide a bit further)
	const float fExtraWidth = !obstructionData.pVeh->InheritsFromBike() ? ms_fExtraWidthForCars : ms_fExtraWidthForBikes;
	ProjectedSize += (obstructionData.vBoundBoxMax.GetXf() + fExtraWidth) * 2.4f;
	ProjectedSize *= fSizeScaler;

	float BlockedRange = ProjectedSize / Distance;

// 	TUNE_GROUP_BOOL(ASDF, DrawObjectAvoidance, true);
// 	if (DrawObjectAvoidance)
// 	{
// 		grcDebugDraw::Line(bumperCrs, CollPoint, Color_white, -1);
// 		grcDebugDraw::Sphere(CollPoint, ProjectedSize, Color_yellow3, false, -1);
// 	}

	AvoidanceTaskObstructionData::ObstructionCircle& circle = obstructionData.m_PedAndObjectObstructions.Append();
	circle.m_Distance = fDistanceMinusRadius; 
	circle.m_Orientation = Orientation;
	circle.m_HalfBlockedAngleRange = BlockedRange * 0.5f;
	circle.m_Entity = pEntity;
	circle.m_ObstructionType = AvoidanceInfo::Obstruction_Object;
}

float CTaskVehicleGoToPointWithAvoidanceAutomobile::FindMaximumSpeedForRacingCar(CVehicle* pVeh, const float fDesiredSpeed, const spdAABB& boundBox)
{
	PF_FUNC(AI_AvoidanceFindMaximumSpeedForRacingCar);

	Assert(pVeh->m_nVehicleFlags.bIsRacing);

	float	MinX, MaxX, MinY, MaxY;

	// Identify scan area.(Block around car)
	const Vector3 vecVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	GenerateMinMaxForPedAvoidance(vecVehiclePos, 20.0f, MinX, MinY, MaxX, MaxY);

	float fMaxSpeed = fDesiredSpeed;

	const float fActualSpeed = pVeh->GetAiXYSpeed();
	const bool bIsBike = pVeh->InheritsFromBike();

	if (bIsBike || pVeh->InheritsFromQuadBike() || pVeh->InheritsFromAmphibiousQuadBike())
	{
		return fMaxSpeed;
	}

	// Get the current drive direction and orientation.
	Vec3V vVehDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse));
	Vector3 vehDriveDir = VEC3V_TO_VECTOR3(vVehDriveDir);
	//float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);

	// If we are turning we turn the front vector a bit as well. This way we won't stop for cars directly ahead
	// of us when we are turning but we will stop for cars that we are turning into.
	float	TurnAngle = 0.5f * pVeh->GetSteerAngle();
	float	cosTurnAngle = rage::Cosf(TurnAngle);
	float	sinTurnAngle = rage::Sinf(TurnAngle);
	float	ourNewDriveDirX = vehDriveDir.x * cosTurnAngle - vehDriveDir.y * sinTurnAngle;
	vehDriveDir.y = vehDriveDir.y * cosTurnAngle + vehDriveDir.x * sinTurnAngle;
	vehDriveDir.x = ourNewDriveDirX;

	const float OurCarDX = vehDriveDir.x * fDesiredSpeed;		// 1 sec
	const float OurCarDY = vehDriveDir.y * fDesiredSpeed;

	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		CVehicle* pOtherVehicle = (CVehicle*) pEntity;

		if (!pOtherVehicle)
		{
			continue;
		}

		if(pOtherVehicle == pVeh)
		{
			continue;
		}

		if (!pOtherVehicle->m_nVehicleFlags.bIsRacing)
		{
			continue;
		}

		const bool bShouldUsingRacingLogic = HelperShouldUseRacingLogic(true, bIsBike, fActualSpeed, pOtherVehicle);

		//if we don't want to use the racing speed, check how far in front th
		if (!bShouldUsingRacingLogic)
		{
			continue;
		}	

		const Vector3 centre = VEC3V_TO_VECTOR3(pOtherVehicle->GetVehiclePosition()); // GetPosition is faster than pOtherVehicle->GetBoundCentre();
		if(	centre.x <= MinX ||
			centre.x >= MaxX ||
			centre.y <= MinY ||
			centre.y >= MaxY ||
			ABS(centre.z - vecVehiclePos.z) >= 5.0f)	// was 5.0f.
		{
			continue;
		}
		
		//const Vec3V vecOtherVehPosition = pOtherVehicle->GetVehiclePosition();
		//TUNE_GROUP_FLOAT(CAR_AI, fAvoidBehindDist, 0.0f, -100.0f, 100.0f, 0.1f);
		const Vec3V vDeltaPos = pOtherVehicle->GetVehiclePosition() - pVeh->GetVehiclePosition();
		//const Vec3V vDeltaPosNormalized = NormalizeFastSafe(vDeltaPos, Vec3V(V_ZERO));
		if (Dot(vDeltaPos.GetXY(), RCC_VEC3V(vehDriveDir).GetXY()).Getf() < boundBox.GetMin().GetYf())
		{
			continue;
		}

		Vec3V vOtherDriveDir = pOtherVehicle->GetVehicleForwardDirection();
		vOtherDriveDir.SetZ(ScalarV(V_ZERO));
		vOtherDriveDir = NormalizeFast(vOtherDriveDir);

		//don't do this if facing the wrong dir
		if (IsLessThanAll(Dot(vVehDriveDir, vOtherDriveDir), ScalarV(V_ZERO)))
		{
			continue;
		}	

		const float fOtherSpeed = pOtherVehicle->GetAiXYSpeed();
		if (fOtherSpeed < 1.0f)
		{
			continue;
		}

		spdAABB otherBoxModified;
		const spdAABB& otherBox = pOtherVehicle->GetLocalSpaceBoundBox(otherBoxModified);
		otherBoxModified = otherBox;

		//if the other vehicle is a bike, add some padding so we dont' sidle up next to bikes that aren't
		//in the exact center of a lane
		if (pOtherVehicle->InheritsFromBike())
		{
			const Vec3V offset(0.5f, 1.0f, 0.5f);
			otherBoxModified.SetMax(otherBoxModified.GetMax() + offset);
			otherBoxModified.SetMin(otherBoxModified.GetMin() - offset);
		}

		const Vector3& vOtherVel = pOtherVehicle->GetAiVelocity();
		const float OtherCarDX = vOtherVel.x;	// 1 sec
		const float OtherCarDY = vOtherVel.y;

		const float	DeltaSpeedX = OtherCarDX - OurCarDX;
		const float	DeltaSpeedY = OtherCarDY - OurCarDY;

		float CollisionT = Test2MovingRectCollision_OnlyFrontBumper(pVeh, pOtherVehicle,
			DeltaSpeedX, DeltaSpeedY,
			vehDriveDir, VEC3V_TO_VECTOR3(vOtherDriveDir), boundBox, otherBoxModified, false);

		const float CollisionT2 = Test2MovingRectCollision(	pOtherVehicle, pVeh, 
			-DeltaSpeedX, -DeltaSpeedY,
			VEC3V_TO_VECTOR3(vOtherDriveDir), vehDriveDir, otherBoxModified, boundBox, false);

		CollisionT = rage::Min(CollisionT, CollisionT2);

		const float fDistToCollision = fDesiredSpeed * CollisionT;

		static dev_float s_fMaxDistToDoRacingSlowdown = 2.0f;
		static dev_float s_fMaxDistToDoRacingSlowdownConservative = 5.0f;
		float fMaxDistToDoRacingSlowdown = !pVeh->m_nVehicleFlags.bIsRacingConservatively ?
			s_fMaxDistToDoRacingSlowdown : s_fMaxDistToDoRacingSlowdownConservative;
		if (fDistToCollision < fMaxDistToDoRacingSlowdown)
		{
			//CVehicle::ms_debugDraw.AddLine(pVeh->GetVehiclePosition(), pVeh->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 4.0f), Color_orange);

			static dev_float s_fSlowdown = 0.75f;
			static dev_float s_fSlowdownConservative = 3.0f;
			float fSlowdown = !pVeh->m_nVehicleFlags.bIsRacingConservatively ?
				s_fSlowdown : s_fSlowdownConservative;

			fMaxSpeed = rage::Min(fMaxSpeed, fOtherSpeed - fSlowdown);
			fMaxSpeed = rage::Max(0.0f, fMaxSpeed);
		}
	}

	return fMaxSpeed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMaximumSpeedForThisCarInTraffic
// PURPOSE :  Takes traffic into account and works out the maximum speed
//			  we can go at.
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPointWithAvoidanceAutomobile::FindMaximumSpeedForThisCarInTraffic(CVehicle* pVeh, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, float RequestedSpeed, float avoidanceRadius, const spdAABB& boundBox, CarAvoidanceLevel carAvoidanceLevel, bool bWasSlowlyPushingPlayer)
{
	PF_FUNC(AI_AvoidanceFindMaximumSpeedForThisCarInTraffic);
	
	float	MinX, MaxX, MinY, MaxY, MaxSpeed;
	//	s32	LoopX, LoopY;
	float	fOriginalSpeed = RequestedSpeed;
	const bool bAvoidVehs = IsDrivingFlagSet(DF_StopForCars);
	const bool bAvoidPeds = IsDrivingFlagSet(DF_StopForPeds);
	const bool bStopForTraffic = IsDrivingFlagSet(DF_StopAtLights);	//used for slowing for doors
	const bool bSteerAroundObjects = IsDrivingFlagSet(DF_SteerAroundObjects);	//also used for slowing for doors

	if(	!bAvoidVehs && !bAvoidPeds && !bStopForTraffic && !bSteerAroundObjects)
	{
		return(RequestedSpeed);
	}

	// Identify scan area.(Block around car)
	const Vector3 vecVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	GenerateMinMaxForPedAvoidance(vecVehiclePos, avoidanceRadius, MinX, MinY, MaxX, MaxY);

	// If we were blocked to stop by a car last frame we test that one first. Chances are it still blocks us and we can jump straight out.
	MaxSpeed = RequestedSpeed;		// Don't want to go faster than this anyhow
	if(m_pOptimization_pLastCarBlockingUs)
	{
		bool bCheckForBraking = true;
		// Only periodically update the braking if the car is stationary.
		// As soon as it is free to move again we update hte brakes each frame
		if(pVeh->GetIntelligence()->GetBrakingDistributer().IsRegistered())
		{
			if(!pVeh->GetIntelligence()->GetBrakingDistributer().ShouldBeProcessedThisFrame())
			{
				bCheckForBraking = false;
			}
		}
		if( bCheckForBraking || m_bAvoidingCarsUntilClear)
		{
//			CVehicle *pObstructor = m_pOptimization_pLastCarBlockingUs;
//			m_pOptimization_pLastCarBlockingUs = NULL;

//			SlowCarDownForOtherCar(pVeh, pObstructor, &MaxSpeed, fOriginalSpeed, SteerDirection, bUseAltSteerDirection, bGiveWarning, vBoundBoxMin, vBoundBoxMax, pVeh->GetBoundRadius(), true);
		}
		else 
		{
			MaxSpeed = 0.0f;
		}
	}

	bool bStoppedForDoor = false;
	if(MaxSpeed > SMALL_FLOAT)
	{
// 		if(pAvoidanceLists->bInAnInterior)
// 		{
// 			if(bAvoidVehs)
// 			{		
// 				SlowCarDownForCarsSectorList(pVeh, pAvoidanceLists->MloExtractedVehObjectList, MinX, MinY, MaxX, MaxY, &MaxSpeed, cruiseSpeedMult);
// 			}
// 
// 			if(bAvoidPeds)
// 			{
// 				SlowCarDownForPedsSectorList(pVeh, pAvoidanceLists->MloExtractedPedObjectList, MinX, MinY, MaxX, MaxY, &MaxSpeed, cruiseSpeedMult);
// 			}
// 		}
// 		else
		{		
			// get lists of entities to check against - don't access the sector blocks directly(it's not allowed...)

			// First we check for cars
			if(bAvoidVehs || m_bBumperInsideBoundsThisFrame)
			{
				SlowCarDownForCarsSectorList(pVeh, MinX, MinY, MaxX, MaxY, MaxSpeed, fOriginalSpeed, SteerDirection, bUseAltSteerDirection, bGiveWarning, boundBox, carAvoidanceLevel);
			}

			//if(bAvoidPeds)
			{
				SlowCarDownForPedsSectorList(pVeh, MinX, MinY, MaxX, MaxY, &MaxSpeed, fOriginalSpeed, bWasSlowlyPushingPlayer);
			}

			if (bAvoidVehs || bStopForTraffic || bSteerAroundObjects)
			{
				const bool bStopForBreakableDoors = bStopForTraffic;
				const float fMaxSpeedBeforeDoors = MaxSpeed;
				SlowCarDownForDoorsSectorList(pVeh, MinX, MinY, MaxX, MaxY, &MaxSpeed, fOriginalSpeed, boundBox, bStopForBreakableDoors);

				if (MaxSpeed <= 1.0f && fMaxSpeedBeforeDoors < 1.0f)
				{
					bStoppedForDoor = true;
				}
			}
		}
	}

	// set flag in vehicle so we know car has processed ped warnings
	pVeh->m_nVehicleFlags.bWarnedPeds = true;

	// If we can go and previously we were stuck we wait a little bit.(Cars slow to pull away when light goes green)
	if(MaxSpeed > 0.01f && m_bStoppingForTraffic)
	{
		m_bStoppingForTraffic = false;
		m_pOptimization_pLastCarWeSlowedFor = NULL;
		m_pOptimization_pLastCarBlockingUs = NULL;

		SetState(State_WaitForTraffic);
		return RequestedSpeed;
	}
	else if(MaxSpeed <= 0.0f && pVeh->GetVelocity().Mag2() < 0.25f * 0.25f)
	{
		m_bStoppingForTraffic = true;
		pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();
	}

	if(IsDrivingFlagSet(DF_StopForCars) || bStoppedForDoor)
	{
		return(MaxSpeed);
	}
	else
	{
		return((MaxSpeed + RequestedSpeed) * 0.5f);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindAvoidancePoints
// PURPOSE :  Goes through the collision primitives in a model and identifies the
//			  ones that a car could collide with(below 2 meters or so)
//			  For those a 
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAvoidancePoints(phBound *bound, Vec4V& Result)
{
	bool	bSomethingFound = false;
	Vector3 MinVec, MaxVec;
	MinVec = Vector3(999999.9f, 999999.9f, 999999.9f);
	MaxVec = Vector3(-999999.9f, -999999.9f, -999999.9f);
	Matrix34	tempMat;
	tempMat.Identity();

	FindAvoidancePointsBound(bound, MinVec, MaxVec, bSomethingFound, tempMat);

	if(bSomethingFound)
	{
		Result.SetX((MinVec.x + MaxVec.x) * 0.5f);
		Result.SetY((MinVec.y + MaxVec.y) * 0.5f);
		Result.SetZ(0.0f);
		Result.SetW(rage::Max(MaxVec.x - MinVec.x, MaxVec.y - MinVec.y) * 0.5f);
	}
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAvoidancePointsBound(phBound *bound, Vector3& MinVec, Vector3& MaxVec, bool& bResult, Matrix34 &mat)
{
	if(bound->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite *boundComposite =(phBoundComposite*)bound;
		for(s32 i=0; i<boundComposite->GetNumBounds(); i++)
		{
			phBound* pChildBound = boundComposite->GetBound(i);
			if(pChildBound)
			{
				Matrix34 localMatrix;
				localMatrix.Dot(RCC_MATRIX34(boundComposite->GetCurrentMatrix(i)), mat);

				FindAvoidancePointsBound(pChildBound, MinVec, MaxVec, bResult, localMatrix);
			}
		}
	}
	else if(bound->GetType() == phBound::GEOMETRY)
	{
		Assert(dynamic_cast<phBoundPolyhedron*>(bound));
		phBoundPolyhedron *boundPoly =static_cast<phBoundPolyhedron*>(bound);

		for(s32 i=0; i<boundPoly->GetNumPolygons(); i++)
		{
			const phPolygon& currPoly = boundPoly->GetPolygon(i);

			// Identify the verts(Treat quads as triangles. Shouldn't make any difference)
			Vector3 vtx0 = mat * VEC3V_TO_VECTOR3(boundPoly->GetVertex(currPoly.GetVertexIndex(0)));
			Vector3 vtx1 = mat * VEC3V_TO_VECTOR3(boundPoly->GetVertex(currPoly.GetVertexIndex(1)));
			Vector3 vtx2 = mat * VEC3V_TO_VECTOR3(boundPoly->GetVertex(currPoly.GetVertexIndex(2)));

			FindAvoidancePointsPoly(vtx0, vtx1 ,vtx2, MinVec, MaxVec, bResult);
		}
	}
	else if(bound->GetType() == phBound::BOX)
	{
		Assert(dynamic_cast<phBoundBox*>(bound));
		phBoundBox *boundBox =static_cast<phBoundBox*>(bound);
		FindAvoidancePointsBox(boundBox->GetBoxSize(), Transform(RCC_MAT34V(mat), boundBox->GetCentroidOffset()), MinVec, MaxVec, bResult);
	}
	else if(bound->GetType() == phBound::SPHERE)
	{
		phBoundSphere *boundSphere =(phBoundSphere*)bound;
		Vector3 transformedCenter;

		//store the current centroid offset
		Vector3 centroidOffset = VEC3V_TO_VECTOR3(boundSphere->GetCentroidOffset());

		mat.Transform(VEC3V_TO_VECTOR3(boundSphere->GetCentroidOffset()), transformedCenter);
		boundSphere->SetCentroidOffset(RCC_VEC3V(transformedCenter));

		FindAvoidancePointsSphere(boundSphere, MinVec, MaxVec, bResult);

		//restore the centroid offset
		boundSphere->SetCentroidOffset(RCC_VEC3V(centroidOffset));
	}
}


#define CUTOFFHEIGHT 2.0f
void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAvoidancePointsPoly(const Vector3 &vtx0, const Vector3 &vtx1, const Vector3 &vtx2, Vector3& MinVec, Vector3& MaxVec, bool& bResult)
{
	// Only consider this polygon if it has at least one vertex below 2 meters height
	if(vtx0.z < CUTOFFHEIGHT || vtx1.z < CUTOFFHEIGHT || vtx2.z < CUTOFFHEIGHT)
	{
		MinVec.x = rage::Min(vtx0.x, MinVec.x);
		MinVec.x = rage::Min(vtx1.x, MinVec.x);
		MinVec.x = rage::Min(vtx2.x, MinVec.x);

		MinVec.y = rage::Min(vtx0.y, MinVec.y);
		MinVec.y = rage::Min(vtx1.y, MinVec.y);
		MinVec.y = rage::Min(vtx2.y, MinVec.y);

		MaxVec.x = rage::Max(vtx0.x, MaxVec.x);
		MaxVec.x = rage::Max(vtx1.x, MaxVec.x);
		MaxVec.x = rage::Max(vtx2.x, MaxVec.x);

		MaxVec.y = rage::Max(vtx0.y, MaxVec.y);
		MaxVec.y = rage::Max(vtx1.y, MaxVec.y);
		MaxVec.y = rage::Max(vtx2.y, MaxVec.y);

		bResult = true;
	}
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAvoidancePointsSphere(const phBoundSphere *boundSphere, Vector3& MinVec, Vector3& MaxVec, bool& bResult)
{
	Vector3 Centre = VEC3V_TO_VECTOR3(boundSphere->GetCentroidOffset());
	float	Radius = boundSphere->GetRadius();

	if(Centre.z - Radius < CUTOFFHEIGHT)
	{
		MinVec.x = rage::Min(Centre.x - Radius, MinVec.x);
		MaxVec.x = rage::Max(Centre.x + Radius, MaxVec.x);

		MinVec.y = rage::Min(Centre.y - Radius, MinVec.y);
		MaxVec.y = rage::Max(Centre.y + Radius, MaxVec.y);

		bResult = true;
	}
}


void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindAvoidancePointsBox(Vec3V_In BoxSize, Vec3V_In CentroidOffset, Vector3& MinVec, Vector3& MaxVec, bool& bResult)
{
	Vector3 Centre = VEC3V_TO_VECTOR3(CentroidOffset);
	Vector3 boxSize = VEC3V_TO_VECTOR3(BoxSize);
	
	if( (Centre.z - (boxSize.z/2.0f)) < CUTOFFHEIGHT)
	{
		MinVec.x = rage::Min(Centre.x - (boxSize.x/2), MinVec.x);
		MaxVec.x = rage::Max(Centre.x + (boxSize.x/2), MaxVec.x);

		MinVec.y = rage::Min(Centre.y - (boxSize.y/2), MinVec.y);
		MaxVec.y = rage::Max(Centre.y + (boxSize.y/2), MaxVec.y);

		bResult = true;
	}
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsObstructedByLawEnforcementPed(const CPed* pPed) const
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the ped is law enforcement.
	if(!pPed->IsLawEnforcementPed())
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsObstructedByLawEnforcementVehicle(const CVehicle* pVehicle) const
{
	//Ensure the vehicle is valid.
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is law enforcement.
	if(!pVehicle->IsLawEnforcementVehicle())
	{
		return false;
	}
	
	//Ensure the vehicle is stationary.
	if(!IsThisAStationaryCar(pVehicle))
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsThisAStationaryCar(const CVehicle* const pVeh) const
{ 
	bool bIsStationary = pVeh->m_nVehicleFlags.bIsThisAStationaryCar;

	if (!bIsStationary || !pVeh->GetDriver() || !pVeh->GetDriver()->IsPlayer())
	{
		return bIsStationary;
	}
	
	//if the driver is a player, this bisThisAStationaryCar flag will be
	//set to true if the player doesn't move for a few seconds.
	//however, if the player is just sitting in traffic or at a red light,
	//we want to consider him as not stationary--the same as what an
	//AI car would be considered if it were stopped.  we will probably need
	//to test these cases separately

	CVehicle* pVehNonConstHack = const_cast<CVehicle*>(pVeh);

	const CVehicle* pMyVeh = GetVehicle();
	Assert(pMyVeh);

	//fast rejection--the junction has already told us to stop
	const s8 iJunctionCommand = pMyVeh->GetIntelligence()->GetJunctionCommand();
	if (iJunctionCommand == JUNCTION_COMMAND_WAIT_FOR_LIGHTS || iJunctionCommand == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC)
	{
		bIsStationary = false;
		//pVehNonConstHack->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds() - 3000;
	}

	//slower: is there any car in front of this guy that's stopped? or moving really slow?
	//in that case, just assume player knows what he's doing and stop for him
	const CVehicle* pCarPlayerBehind = pVeh->GetIntelligence()->GetCarWeAreBehind();
	if (pCarPlayerBehind 
		&& pCarPlayerBehind->GetAiXYSpeed() < 1.0f
		&& pVeh->GetIntelligence()->GetDistanceBehindCarSq() < 15.0f * 15.0f
		&& !pCarPlayerBehind->m_nVehicleFlags.bIsThisAStationaryCar
		&& !pCarPlayerBehind->m_nVehicleFlags.bIsThisAParkedCar
		&& IsGreaterThanAll(Dot(pCarPlayerBehind->GetVehicleForwardDirection(), pVeh->GetVehicleForwardDirection()), ScalarV(V_ZERO))
		&& IsLessThanAll(Abs(pCarPlayerBehind->GetVehiclePosition().GetZ() - pVeh->GetVehiclePosition().GetZ()), ScalarV(V_SIX)))
	{
		//grcDebugDraw::Line(pVeh->GetVehiclePosition(), pCarPlayerBehind->GetVehiclePosition(), Color_green, Color_yellow);

		bIsStationary = false;

		pVehNonConstHack->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds() - 3000;
	}

// 	if (bIsStationary)
// 	{
		//Draw a red line if we're going to do the expensive test.
// 		grcDebugDraw::Line(pMyVeh->GetVehiclePosition(), pVeh->GetVehiclePosition(), Color_orange, Color_red);
// 	}

	//B* 2076557; this is causing player driver to almost never be considered stationary so cars won't avoid it when stationary
	//slowest: iterate through our nodelist and see if there are any red lights
// 	if (bIsStationary && iJunctionCommand != JUNCTION_COMMAND_NOT_ON_JUNCTION && CVehicleJunctionHelper::ApproachingRedLight(pMyVeh))
// 	{
// 		bIsStationary = false;
// 
// 		pVehNonConstHack->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds() - 3000;
// 	}

	return bIsStationary;
} 

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsFirstCarBestOneToGo
// PURPOSE :  Given the positions of these 2 vehicles that are facing each other this function
//			  returns true if the first one is the best candidate to go.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsFirstCarBestOneToGo(const CVehicle* const pVeh, const CVehicle* const pOtherVeh
	, const bool bFirstCarIsAlreadyGoing)
{
	//never try and go around a car we're forced to stop for.
	if (pOtherVeh->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle
		|| pOtherVeh->m_nVehicleFlags.bIsAmbulanceOnDuty
		|| pOtherVeh->m_nVehicleFlags.bIsFireTruckOnDuty)
	{
		return false;
	}

	if (pOtherVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS
		|| pOtherVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC)
	{
		return true;
	}

	//if pOtherVeh is following pVeh, pVeh shouldn't ever wait for it, because if they're both deadlocked,
	//pOtherVeh's desired velocity is probably 0 and it probably won't ever move.
	//actually figuring out his desired velocity would require a bit of recursion, so we can just 
	//make use of the avoidance cache to see who's stopping for whom
	const CPhysical* pOtherAvoidanceTarget = pOtherVeh->GetIntelligence()->GetAvoidanceCache().m_pTarget;
	if (pOtherAvoidanceTarget && pOtherAvoidanceTarget == pVeh)
	{
		return true;
	}

	// We need to find out what the distance is of each cars' centre point to the centre line of the other.
	Vec3V diff = pOtherVeh->GetVehiclePosition() - pVeh->GetVehiclePosition();
//	float lengthAlong1 = diff.Dot(pVeh->GetB());
	ScalarV lengthAlong1(Dot(diff, pVeh->GetVehicleForwardDirection()));
//	float lengthAlong2 = -diff.Dot(pOtherVeh->GetB());
	ScalarV lengthAlong2(-Dot(diff, pOtherVeh->GetVehicleForwardDirection()));

	if (bFirstCarIsAlreadyGoing)
	{
		static const ScalarV scBonusForAlreadyAvoiding(V_ONE);
		lengthAlong1 -= scBonusForAlreadyAvoiding;
	}

//	return (lengthAlong1 < lengthAlong2);
	return (IsLessThanAll(lengthAlong1, lengthAlong2)) != 0;
}

// bool CTaskVehicleGoToPointWithAvoidanceAutomobile::CanSwitchLanesToAvoidObstruction(const CVehicle* const pVeh, const CPhysical* const pAvoidEnt, const float fDesiredSpeed) 
// {
// 	TUNE_GROUP_BOOL(CAR_AI, EnableLaneChanges, false);
// 	if (!EnableLaneChanges)
// 	{
// 		return false;
// 	}
// 	if (!IsDrivingFlagSet(DF_ChangeLanesAroundObstructions))
// 	{
// 		return false;
// 	}
// 
// 	const CPed* pDriver = pVeh->GetDriver();
// 	const bool bIsMission = pVeh->PopTypeIsMission() || pDriver && pDriver->PopTypeIsMission();
// 	const bool bIsAggressiveEnough = CDriverPersonality::WillChangeLanes(pDriver);
// 	if (!bIsMission && !bIsAggressiveEnough)
// 	{
// 		return false;
// 	}
// 
// 	if (!pAvoidEnt)
// 	{
// 		return false;
// 	}
// 
// 	const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();
// 	if (!pFollowRoute)
// 	{
// 		return false;
// 	}
// 
// 	//if there's only one lane, we can't change lanes
// 	const int iCurrentPoint = pFollowRoute->GetCurrentTargetPoint();
// 	Assert(iCurrentPoint > 0 && iCurrentPoint < pFollowRoute->GetNumPoints());
// 	const CRoutePoint* pTargetPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint];
// 	//CRoutePoint* pCurrentPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint-1];
// 
// 	if (pTargetPoint->m_iNoLanesThisSide <= 1)
// 	{
// 		return false;
// 	}
// 
// 	if (!ShouldChangeLanesForThisEntity(pVeh, pAvoidEnt, fDesiredSpeed, IsDrivingFlagSet(DF_DriveInReverse)))
// 	{
// 		return false;
// 	}
// 
// 	const int iCurrentLane = (int)Floorf(pTargetPoint->GetLane().Getf());
// 	if (iCurrentLane < pTargetPoint->m_iNoLanesThisSide - 1)
// 	{
// 		m_bWaitingToUndertakeThisFrame = true;
// 		m_TimeWaitingForUndertake += fwTimer::GetTimeStepInMilliseconds();
// 		m_TimeWaitingForUndertake = Min(m_TimeWaitingForUndertake, ms_TimeBeforeUndertake);
// 	}
// 	
// 	if (iCurrentLane > 0)
// 	{
// 		m_bWaitingToOvertakeThisFrame = true;
// 		m_TimeWaitingForOvertake += fwTimer::GetTimeStepInMilliseconds();
// 		m_TimeWaitingForOvertake = Min(m_TimeWaitingForOvertake, ms_TimeBeforeOvertake);
// 	}
// 
// 	return m_bWaitingToOvertakeThisFrame && m_TimeWaitingForOvertake >= ms_TimeBeforeOvertake
// 		|| m_bWaitingToUndertakeThisFrame && m_TimeWaitingForUndertake >= ms_TimeBeforeUndertake;
// }

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsThereRoomToOvertakeOtherVehicle
// PURPOSE :  Does a check to make sure there isn't any oncoming traffic in the
//			  way if we want to overtake this car.
//			  The car is probably a parked car.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsThereRoomToOvertakeVehicle(const CVehicle* const pVeh, const CVehicle* const pOtherVeh, const bool bOnlyWaitForCarsMovingInOppositeDirection) const
{
#define OVERTAKE_MARGIN 0.1f
	// First we identify the area that needs to be clear of oncoming traffic.

	// Find the width of our car.
	float VehHalfWidth = pVeh->GetBoundingBoxMax().x;
	const Vector3 vOtherBoundingBoxMax = pOtherVeh->GetBoundingBoxMax();
	float LineOffsetFromVehToOvertake = vOtherBoundingBoxMax.x;

	bool bGetTravelDirFromVehicle = true;

	// Find the direction we are travelling in.
	// We take the forward vector of the parked car.
	Vector3 TravelDir, vSide, vSideRightSeenFromUs;
#if 0
	TUNE_GROUP_BOOL(ASDF, EnableNewPlayerOvertakeDirTest, false);
	CPed* pOtherDriver = pOtherVeh->GetDriver();
	if (EnableNewPlayerOvertakeDirTest && pOtherDriver && pOtherDriver->IsLocalPlayer())
	{
		//assume we'll get this info from the link
		bGetTravelDirFromVehicle = false;

		const CPlayerInfo* pPlayerInfo = pOtherDriver->GetPlayerInfo();
		Assert(pPlayerInfo);
		const CPathNode* pPlayerNode = ThePaths.FindNodePointerSafe(pPlayerInfo->GetPlayerDataNodeAddress());
		if (!pPlayerNode)
		{
			bGetTravelDirFromVehicle = true;
		}
		const CPathNodeLink* pLink = &ThePaths.GetNodesLink(pPlayerNode, pPlayerInfo->GetPlayerDataLocalLinkIndex());
		if (pLink)
		{
			const CPathNode* pOtherNode = ThePaths.FindNodePointerSafe(pLink->m_OtherNode);
			if (pOtherNode)
			{
				TravelDir = pOtherNode->GetPos() - pPlayerNode->GetPos();
				TravelDir.z = 0.0f;
				TravelDir.Normalize();

				vSide = Vector3(TravelDir.y, -TravelDir.x, 0.0f);
				vSideRightSeenFromUs = vSide;

				const float fDotCarRight = fabsf(vSide.Dot(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleRightDirection())));
				const float fDotCarFwd = fabsf(vSide.Dot(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection())));

				//do some stupid cheap blending instead of getting the half length of the vehicle's 
				//bounding box on an arbitrary axis
				LineOffsetFromVehToOvertake = fDotCarRight * vOtherBoundingBoxMax.x + fDotCarFwd * fDotCarFwd;
			}
			else
			{
				bGetTravelDirFromVehicle = true;
			}
		}
		else
		{
			bGetTravelDirFromVehicle = true;
		}
	}
#endif //0

	if (IsThisAParkedCar(pOtherVeh))
	{
		//for parked cars, just use their direction since they're spawned 
		//parallel from the road
		bGetTravelDirFromVehicle = true;
	}
	else
	{
		//otherwise use the direction of our path node at that point
		//the followroute should be cached already here so this should be a cheap lookup
		const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();
		if (pFollowRoute)
		{
			s16 iClosestPointToPosition = pFollowRoute->FindClosestPointToPosition(pOtherVeh->GetVehiclePosition()
				, rage::Max(0, pFollowRoute->GetCurrentTargetPoint()-1));
			if (iClosestPointToPosition >= 0 && pFollowRoute->GetNumPoints() > 1)
			{
				if (iClosestPointToPosition < pFollowRoute->GetNumPoints()-1)
				{
					TravelDir = VEC3V_TO_VECTOR3(pFollowRoute->GetRoutePoints()[iClosestPointToPosition+1].GetPosition()
					- pFollowRoute->GetRoutePoints()[iClosestPointToPosition].GetPosition());
				}
				else
				{
					Assert(iClosestPointToPosition > 0);
					TravelDir = VEC3V_TO_VECTOR3(pFollowRoute->GetRoutePoints()[iClosestPointToPosition].GetPosition()
					- pFollowRoute->GetRoutePoints()[iClosestPointToPosition-1].GetPosition());
				}
				TravelDir.z = 0.0f;
				TravelDir.Normalize();

				vSide = Vector3(TravelDir.y, -TravelDir.x, 0.0f);
				vSideRightSeenFromUs = vSide;

				const float fDotCarRight = fabsf(vSide.Dot(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleRightDirection())));
				const float fDotCarFwd = fabsf(vSide.Dot(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection())));

				//do some stupid cheap blending instead of getting the half length of the vehicle's 
				//bounding box on an arbitrary axis
				LineOffsetFromVehToOvertake = fDotCarRight * vOtherBoundingBoxMax.x + fDotCarFwd * fDotCarFwd;

				bGetTravelDirFromVehicle = false;
			}
		}
	}

	if (bGetTravelDirFromVehicle)
	{
		const float VehToOvertakeHalfWidth = vOtherBoundingBoxMax.x;
		LineOffsetFromVehToOvertake = VehToOvertakeHalfWidth;

		TravelDir = (VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection()));
		TravelDir.z = 0.0f;
		TravelDir.Normalize();

		vSide = (VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleRightDirection()));
		vSide.z = 0.0f;
		vSide.Normalize();
		vSideRightSeenFromUs = vSide;

		// wipavoidance

		// Make sure it is pointing away from us.
		if(DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()), TravelDir) < 0.0f)
		{
			TravelDir = -TravelDir;
			vSideRightSeenFromUs = -vSideRightSeenFromUs;
		}
	}

	//CVehicle::ms_debugDraw.AddArrow(pOtherVeh->GetVehiclePosition(), pOtherVeh->GetVehiclePosition() + VECTOR3_TO_VEC3V(vSideRightSeenFromUs) * ScalarV(V_FIVE)
	//	, 0.25f, Color_orange);

	// The centre point of the line is to the left side of the parked car.
	Vector3 LineBase = VEC3V_TO_VECTOR3(pOtherVeh->GetVehiclePosition());
	float Offset = VehHalfWidth + LineOffsetFromVehToOvertake + OVERTAKE_MARGIN;

	// If the avoidance data is set up(parked car) we HAVE to go round the car a certain direction.
	// otherwise we have a choice.
	//	if(IsThisAParkedCar(pOtherVeh))

	Vec3V vFlatDelta = pOtherVeh->GetVehiclePosition()-pVeh->GetVehiclePosition();
	vFlatDelta.SetZ(ScalarV(V_ZERO));
	if(IsLessThanAll(Dot(pVeh->GetVehicleRightDirection(), (vFlatDelta)), ScalarV(V_ZERO)))
	{		// Going on right hand side of target car.
		vSide = vSideRightSeenFromUs;
	}
	else
	{
		vSide = -vSideRightSeenFromUs;
	}
	LineBase += Offset * vSide;

	// Find out how much we have to move the linebase back.

	//Back is expected to be negative
	float Back =Dot((pVeh->GetVehiclePosition() - pOtherVeh->GetVehiclePosition()), RCC_VEC3V(TravelDir)).Getf();
	float Forward = 15.0f;		// Look 15 meters ahead.
	LineBase += Back * TravelDir;
	Vector3 LineEnd = LineBase + TravelDir *(Forward - Back);

	// Now LineBase is the start of the line to test and LineEnd is the end.

	const Vector3 vecVehPosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());

	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		Assert(pEntity->GetIsTypeVehicle());
		CVehicle *pVehTraffic =(CVehicle *)pEntity;

		if(	!pVehTraffic ||
			pVehTraffic==pVeh ||
			pVehTraffic==pOtherVeh ||
			IsThisAStationaryCar(pVehTraffic))
		{
			continue;
		}

		// Make sure this vehicle is in the right z range
		const Vector3 vecVehTrafficPosition = VEC3V_TO_VECTOR3(pVehTraffic->GetVehiclePosition());
		if(rage::Abs(vecVehPosition.z - vecVehTrafficPosition.z) >= 4.0f)
		{
			continue;
		}

		// Make sure this vehicle is traveling towards us
		if(bOnlyWaitForCarsMovingInOppositeDirection && TravelDir.Dot(pVehTraffic->GetAiVelocity()) >= -0.2f)
		{
			continue;
		}

		// Now test whether this vehicle would actually block our path.
		float DistToTravelLine = fwGeom::DistToLine(LineBase, LineEnd, vecVehTrafficPosition);
		if(DistToTravelLine >= VehHalfWidth + pVehTraffic->GetBoundingBoxMax().x + OVERTAKE_MARGIN)
		{
			continue;
		}

		// Our path is blocked. Wait behind parked car for a bit.
#if __DEV
		if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(LineBase, LineEnd, Color32(255, 0, 0, 255));
		}
#endif //__DEV

		return false;
	}

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		grcDebugDraw::Line(LineBase, LineEnd, Color32(0, 255, 0, 255));
	}
#endif //__DEV

	return true;
}


bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldAllowTimeslicingWhenAvoiding() const
{
	const CVehicle& veh = *GetVehicle();

	// If we are standing still and are stopping for traffic and were already
	// set to use aggressive timeslicing, then continue to allow timeslicing.
	bool exceptionToStillAllow = m_bStoppingForTraffic
			&& veh.m_nVehicleFlags.bLodShouldUseTimeslicing
			&& veh.GetAiXYSpeed() < 0.5f;

	return exceptionToStillAllow;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShouldGiveWarning
// PURPOSE :  We may consider flashing headlights at the other car
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldGiveWarning(const CVehicle* const pVeh, const CVehicle* const pOtherVeh) const
{
	static dev_float s_fAbilityThresholdForGiveWarning = 0.2f;	//80% of drivers give warning
	if(!pOtherVeh->GetDriver() || !pOtherVeh->GetDriver()->IsPlayer()){return false;}

	//don't flash headlights if we aren't driving recklessly
	if (!pOtherVeh->m_nVehicleFlags.bShouldBeBeepedAt && !pOtherVeh->m_nVehicleFlags.bPlayerDrivingAgainstTraffic){ return false;}

	if(!pVeh->GetDriver() || pVeh->GetDriver()->GetPedType() == PEDTYPE_COP){return false;}

	// random seed can't be applied before the charge event, there shouldn't be any randomization for it
	if(CDriverPersonality::FindDriverAbility(pVeh->GetDriver(), pVeh) < s_fAbilityThresholdForGiveWarning) {return false;}

	if(pOtherVeh->GetAiVelocity().Mag2() <= 81.0f) {return false;}

	if(pVeh->GetAiXYSpeed() <= 1.0f) {return false;}

	if(Dot(pVeh->GetVehicleForwardDirection(), pOtherVeh->GetVehicleForwardDirection()).Getf() >= -0.93f) {return false;}

	// Make sure cars are 'head on' close to each others center lines.
	if(IsGreaterThanOrEqualAll(Dot(pOtherVeh->GetVehicleRightDirection(), (pVeh->GetVehiclePosition() - pOtherVeh->GetVehiclePosition())), ScalarV(V_TWO))) {return false;}

	//if(pOtherVeh->GetIntelligence()->m_bDontSwerveForUs) {return false;}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InConvoyTogether()
// PURPOSE :  Processes one sector pedList.
// RETURNS :  True if we are in a convoy together.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::InConvoyTogether(const CVehicle* const pVeh, const CVehicle* const pOtherVeh)
{
	if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_VEHICLE_ESCORT)
	{
		CTaskVehicleEscort *pEscortMission = static_cast<CTaskVehicleEscort*>(GetParent());
		return pEscortMission->InConvoyTogether(pVeh, pOtherVeh);
	}
	else
	{
		//Not escorting so not in a convoy
		return false;
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::SetWaitingForPlayerPed(bool bWaitingForPlayerPed)
{
	m_bWaitingForPlayerPed = bWaitingForPlayerPed;

	//we might get in here after we've already been stopped for the player,
	//so don't allow this to set the time to a later timestamp
	//if this is a continuous stoppage
	if (m_bWaitingForPlayerPed && m_startTimeWaitingForPlayerPed == 0)
	{
		m_startTimeWaitingForPlayerPed = fwTimer::GetTimeInMilliseconds();
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindPlayerNearMissTargets(const CVehicle* pPlayerVehicle, CPlayerInfo* playerInfo, bool trackPreciseNearMiss /* = false */)
{
	Assertf(pPlayerVehicle->GetDriver() && pPlayerVehicle->GetDriver()->IsPlayer(), "Near miss vehicle doesn't have player driver");

	const Vector3 ourPosition = VEC3V_TO_VECTOR3(pPlayerVehicle->GetVehiclePosition());
	CEntityScannerIterator entityList = pPlayerVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		Assert(pEntity->GetIsTypeVehicle());
		CVehicle *pVehObstacle =(CVehicle *)pEntity;

		Vector3 otherVehCentre = VEC3V_TO_VECTOR3(pVehObstacle->GetVehiclePosition());
		float fDist = otherVehCentre.Dist2(ourPosition);
		if (fDist > 30.0f*30.0f || ABS(otherVehCentre.z - ourPosition.z) > 5.0f)
		{
			continue;
		}

		playerInfo->NearVehicleMiss(pPlayerVehicle, pVehObstacle, trackPreciseNearMiss);
	}
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldIgnoreNonDirtyVehicle(const CVehicle& rVeh, const CVehicle& rOtherVeh, const bool bDriveInReverse, const bool bOnSingleTrackRoad) const
{
	//TUNE_GROUP_BOOL(CAR_AI, EnableDirtyFlagRejection, true);
	static dev_bool bEnableDirtyFlagRejection = true;
	if (!bEnableDirtyFlagRejection)
	{
		return false;
	}

	//if I'm dirty, I better be careful and avoid everyone else
	if (rVeh.m_nVehicleFlags.bAvoidanceDirtyFlag)
	{
		return false;
	}

	//if I'm on a single track road, I'll tend to encounter other vehicles
	//facing me, so don't ignore them here
	if (bOnSingleTrackRoad)
	{
		return false;
	}

	//parked and stationary cars are always considered dirty
	if (rOtherVeh.m_nVehicleFlags.bAvoidanceDirtyFlag || rOtherVeh.m_nVehicleFlags.bIsOvertakingStationaryCar || IsThisAParkedCar(&rOtherVeh))
	{
		return false;
	}

	//treat everyone facing the same direction as us the same as before
	static const ScalarV scZero(V_ZERO);
	if (IsGreaterThanAll(Dot(CVehicleFollowRouteHelper::GetVehicleDriveDir(&rVeh, bDriveInReverse).GetXY(), rOtherVeh.GetVehicleForwardDirection().GetXY()), scZero))
	{
		return false;
	}

	//if the other guy is in a junction, he should be treated as dirty,
	//but we don't actually set the dirty flag for him because we don't want those
	//vehicles doing avoidance on waiting traffic by returning early above
	//for having their dirty flag set when it's their turn to do avoidance.
	//so, we evaluate that here
	if (rOtherVeh.GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO)
	{
		const CJunction* pOtherJn = rOtherVeh.GetIntelligence()->GetJunction() ? rOtherVeh.GetIntelligence()->GetJunction() : rOtherVeh.GetIntelligence()->GetPreviousJunction();
		const bool bOtherJunctionIsFake = !pOtherJn || pOtherJn->IsOnlyJunctionBecauseHasSwitchedOffEntrances();
		if(!bOtherJunctionIsFake)
		{
			return false;
		}
	}

	//parked and stationary cars are always considered dirty
	if (IsThisAStationaryCar(&rOtherVeh) || rOtherVeh.m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle)
	{
		return false;
	}

	//always consider player driven clones as dirty; who knows what they'll do
	if(rOtherVeh.IsNetworkClone() && rOtherVeh.GetDriver() && rOtherVeh.GetDriver()->IsPlayer())
	{
		return false;
	}

#if __BANK
	if (CVehicleIntelligence::m_bDisplayDirtyCarPilons)
	{
		CVehicle::ms_debugDraw.AddLine(rVeh.GetVehiclePosition(), rOtherVeh.GetVehiclePosition(), Color_yellow3);
	}
#endif //__BANK

	return true;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldNotAvoidVehicle(const CVehicle& rVeh, const CVehicle& rOtherVeh)
{
	//Grab the avoidance target.
	const CVehicleIntelligence::AvoidanceCache& rCache = rVeh.GetIntelligence()->GetAvoidanceCache();
	const CEntity* pTarget = rCache.m_pTarget;
	if(!pTarget)
	{
		return false;
	}

	//If this vehicle is an attached trailer, reject it based on its parent cab
	const CVehicle* pOtherVehToCompare = &rOtherVeh;
	if (rOtherVeh.InheritsFromTrailer() && rOtherVeh.m_nVehicleFlags.bHasParentVehicle)
	{
		const CVehicle* pParentCab = GetTrailerParentFromVehicle(pOtherVehToCompare);
		if (pParentCab)
		{
			pOtherVehToCompare = pParentCab;
		}
	}
	
	//Ensure the avoidance targets match.
	const CVehicleIntelligence::AvoidanceCache& rOtherCache = pOtherVehToCompare->GetIntelligence()->GetAvoidanceCache();
	if(pTarget != rOtherCache.m_pTarget)
	{
		return false;
	}
	
	//The targets match.
	//Check if we are supposed to be behind the other vehicle.
	
	//Ensure our desired offset is greater than the other vehicle's desired offset.
	if(rCache.m_fDesiredOffset <= rOtherCache.m_fDesiredOffset)
	{
		return false;
	}

	//avoid if either vehicle on a small road
	if(rVeh.GetIntelligence()->IsOnSmallRoad() || rOtherVeh.GetIntelligence()->IsOnSmallRoad())
	{
		return false;
	}
	
	//We are supposed to be behind the other vehicle.
	//Check if we are actually behind them.
	
	//Grab the vehicle position.
	Vec3V vPosition = rVeh.GetVehiclePosition();

	//Grab the other vehicle position.
	Vec3V vOtherPosition = pOtherVehToCompare->GetVehiclePosition();
	
	//Grab the other vehicle's velocity.
	Vec3V vOtherVelocity = VECTOR3_TO_VEC3V(pOtherVehToCompare->GetAiVelocity());
	
	//Grab the other vehicle forward.
	Vec3V vOtherForward = pOtherVehToCompare->GetVehicleForwardDirection();
	
	//Calculate the other vehicle's direction.
	Vec3V vOtherDirection = NormalizeFastSafe(vOtherVelocity, vOtherForward);
	
	//Generate a vector from the other vehicle to the vehicle.
	Vec3V vOtherToVeh = Subtract(vPosition, vOtherPosition);

	ScalarV fDist = MagSquared(vOtherToVeh);
	if(IsLessThanAll(fDist, ScalarV(V_FOUR)) && rVeh.GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		return false;
	}
	
	//Ensure we are behind the other vehicle.
	ScalarV scDot = Dot(vOtherDirection, vOtherToVeh);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsAnyVehicleTooCloseToTailgate(const CVehicle& rVehicle, const CVehicle& rTailgateVehicle)
{
	//Generate the vehicle bounding box.
	//Increase the size of the box by a small amount.
	spdAABB vehicleBoundingBox = rVehicle.GetBaseModelInfo()->GetBoundingBox();
	vehicleBoundingBox.SetMin(Add(vehicleBoundingBox.GetMin(), Vec3V(V_NEGONE)));
	vehicleBoundingBox.SetMax(Add(vehicleBoundingBox.GetMax(), Vec3V(V_ONE)));
	
	//Calculate the max distance.
	//This is fairly arbitrary.
	ScalarV scMaxDistance = Subtract(vehicleBoundingBox.GetMax().GetX(), vehicleBoundingBox.GetMin().GetX());
	scMaxDistance = Scale(scMaxDistance, ScalarV(V_TWO));
	ScalarV scMaxDistanceSq = Scale(scMaxDistance, scMaxDistance);
	
	//Grab the vehicle matrix.
	Mat34V mVehicle = rVehicle.GetMatrix();
	
	//Translate the matrix one frame ahead.
	//This should make vehicles slightly more responsive to vehicles in front of them, as compared to those behind them.
	Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(rVehicle.GetAiVelocity());
	Vec3V vVehicleOffset = Scale(vVehicleVelocity, ScalarVFromF32(GetTimeStep()));
	mVehicle.SetCol3(Add(mVehicle.GetCol3(), vVehicleOffset));
	
	//Transform the bounding box.
	vehicleBoundingBox.Transform(mVehicle);
	
	//Check if we have a cached vehicle that is too close to tailgate.
	const CVehicle* pVehicleTooCloseToTailgate = m_pOptimization_pVehicleTooCloseToTailgate;
	if(pVehicleTooCloseToTailgate)
	{
		//Check if the vehicle is still too close to tailgate.
		if(IsVehicleTooCloseToTailgate(vehicleBoundingBox, *pVehicleTooCloseToTailgate))
		{
			return true;
		}
	}
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = rVehicle.GetTransform().GetPosition();
	
	//Iterate over the vehicles.
	CEntityScannerIterator entityList = rVehicle.GetIntelligence()->GetVehicleScanner().GetIterator();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Grab the other vehicle.
		const CVehicle* pOtherVehicle = static_cast<CVehicle *>(pEntity);
		
		//Ensure the vehicle does not match.
		if(pOtherVehicle == &rVehicle)
		{
			continue;
		}
		
		//Ensure the tailgate vehicle does not match.
		if(pOtherVehicle == &rTailgateVehicle)
		{
			continue;
		}
		
		//Ensure the optimization vehicle does not match (we should have already checked it above).
		if(pOtherVehicle == pVehicleTooCloseToTailgate)
		{
			continue;
		}
		
		//Grab the other vehicle position.
		Vec3V vOtherVehiclePosition = pOtherVehicle->GetTransform().GetPosition();
		
		//Ensure the distance has not exceeded the threshold.
		ScalarV scDistanceSq = DistSquared(vVehiclePosition, vOtherVehiclePosition);
		if(IsGreaterThanAll(scDistanceSq, scMaxDistanceSq))
		{
			break;
		}
		
		//Ensure the vehicle is too close to tailgate.
		if(!IsVehicleTooCloseToTailgate(vehicleBoundingBox, *pOtherVehicle))
		{
			continue;
		}
		
		//Set the vehicle that is too close to tailgate.
		m_pOptimization_pVehicleTooCloseToTailgate = pOtherVehicle;
		
		return true;
	}
	
	//Clear the vehicle that is too close to tailgate.
	m_pOptimization_pVehicleTooCloseToTailgate = NULL;
	
	return false;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::IsVehicleTooCloseToTailgate(const spdAABB& rVehicleBoundingBox, const CVehicle& rOtherVehicle) const
{
	//Grab the other vehicle bounding box.
	spdAABB otherVehicleBoundingBox;
	const spdAABB& rOtherVehicleBoundingBox = rOtherVehicle.GetBoundBox(otherVehicleBoundingBox);
	
	//Check if the bounding boxes intersect.
	return (rVehicleBoundingBox.IntersectsAABB(rOtherVehicleBoundingBox));
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TailgateCarInFront(const CVehicle* pVeh, float* pMaxSpeed)
{
	PF_FUNC(AI_AvoidanceTailgateCarInFront);
	
	//Check if we are behind someone.
	const CVehicle* pOtherVeh = pVeh->GetIntelligence()->GetCarWeAreBehind();
	if(!pOtherVeh)
	{
		return false;
	}
	
	//Grab the driver.
	CPed* pDriver = pVeh->GetDriver();
	if(pDriver && pDriver->PopTypeIsMission())
	{
		//If the driver is a mission ped, do not tailgate.  It is causing too many
		//issues with missions and for now it is best to promote stability.
		return false;
	}
	
	//Check if we have exceeded the max tailgate distance.
	float fDistSq = pVeh->GetIntelligence()->GetDistanceBehindCarSq();
	float fMaxDistSq = sm_Tunables.m_TailgateDistanceMaxSq;
	if(fDistSq > fMaxDistSq)
	{
		return false;
	}
	
	//Generate the magnitude of the other vehicle's velocity.
	float fOtherMag = pOtherVeh->GetAiXYSpeed();
	
	//Check if the other magnitude has exceeded the min tailgate velocity.
	if(fOtherMag < sm_Tunables.m_TailgateVelocityMin)
	{
		return false;
	}

	//Check if the player is closer than tailgate distance. Otherwise, we don't want to tailgate
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer)
	{
		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		const Vector3 vOurPos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		if ((vPlayerPos - vOurPos).XYMag2() < fMaxDistSq)
		{
			return false;
		}

		//also test the player's vehicle, since sometimes players like to park their car in the middle
		//of the street and walk away like an asshole
		const CVehicle* pPlayerVeh = pPlayer->GetMyVehicle();
		if (pPlayerVeh)
		{
			const Vector3 vPlayerVehPos = VEC3V_TO_VECTOR3(pPlayerVeh->GetVehiclePosition());
			if ((vPlayerVehPos - vOurPos).XYMag2() < fMaxDistSq)
			{
				return false;
			}
		}
	}

	//don't tailgate in junctions, I've seen too many cases of vehicles hitting each other
	//when turning across traffic
	if (pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO
		&& pVeh->GetIntelligence()->GetJunction()
		&& !pVeh->GetIntelligence()->GetJunction()->IsOnlyJunctionBecauseHasSwitchedOffEntrances()
		&& pVeh->GetIntelligence()->GetJunction()->GetNumEntrances() > 1)
	{
		return false;
	}
	
	//Check if we are too close to another vehicle to tailgate.
	if(IsAnyVehicleTooCloseToTailgate(*pVeh, *pOtherVeh))
	{
		return false;
	}
	
#if __BANK
	if(CVehicleIntelligence::m_bDisplayDebugTailgate && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Vec3V vPos = pVeh->GetTransform().GetPosition();
		vPos.SetZf(vPos.GetZf() + pVeh->GetBoundingBoxMax().z + 0.5f);
		grcDebugDraw::Sphere(vPos, 0.25f, Color_green);
	}
#endif

	//Calculate the ideal distance based on how fast the tailgated vehicle is traveling.
	float fIdealDistSq = rage::square(fOtherMag);
	const float fExtraStoppingDistanceFromPersonality = CDriverPersonality::GetExtraStoppingDistanceForTailgating(pDriver, pVeh);
	fIdealDistSq = rage::Clamp(fIdealDistSq
		, sm_Tunables.m_TailgateIdealDistanceMinSq + fExtraStoppingDistanceFromPersonality
		, sm_Tunables.m_TailgateIdealDistanceMaxSq + fExtraStoppingDistanceFromPersonality);
	
	//Calculate the speed multiplier.
	float fMult = (fDistSq / fIdealDistSq);
	const float fExtraSpeedMultiplierFromPersonality = CDriverPersonality::GetExtraSpeedMultiplierForTailgating(pDriver, pVeh);
	fMult = rage::Clamp(fMult
		, sm_Tunables.m_TailgateSpeedMultiplierMin - fExtraSpeedMultiplierFromPersonality
		, sm_Tunables.m_TailgateSpeedMultiplierMax + fExtraSpeedMultiplierFromPersonality);
	
	//Assign the speed.
	*pMaxSpeed = rage::Min(*pMaxSpeed, fOtherMag * fMult);

	//tell our vehicle intelligence we aren't stuck
	if (*pMaxSpeed <= 1.0f)
	{
		pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();
	}
	
	//Tailgating was successful.
	return true;
}

void HelperTellOtherVehicleToHurry(CVehicle* pOtherVeh, CPed* pDriver)
{
	if (pOtherVeh->m_nVehicleFlags.bToldToGetOutOfTheWay)
	{
		return;
	}

	//don't do this for mission vehicles
	if (pOtherVeh->PopTypeIsMission() || (pDriver && pDriver->PopTypeIsMission()))
	{
		return;
	}

	pOtherVeh->m_nVehicleFlags.bToldToGetOutOfTheWay = true;
	if (pDriver)
	{
		pDriver->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
	}
	else
	{
		pOtherVeh->GetIntelligence()->SetPretendOccupantAggressivness(1.0f);
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::MakeWayForCarTellingOthersToFlee(CVehicle* pVehicleToFlee)
{
	Assert(pVehicleToFlee);

	static const ScalarV scTwentyFive(25.0f);
	static const ScalarV scZero(V_ZERO);
	const Vec3V vehDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicleToFlee, IsDrivingFlagSet(DF_DriveInReverse));

	CEntityScannerIterator entityList = pVehicleToFlee->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		CVehicle* pVehicle = (CVehicle*)pEntity;

		if( pVehicle && (pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBike()) )
		{
			//test if the other car is relatively right in front of the car to flee
			const Vec3V vUsToThem = pVehicle->GetVehiclePosition() - CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicleToFlee, IsDrivingFlagSet(DF_DriveInReverse));
			const ScalarV scRightOffset = Dot(pVehicleToFlee->GetVehicleRightDirection(), vUsToThem);
			const ScalarV scFrontOffset = Dot(vehDriveDir, vUsToThem);
			const ScalarV scFrontThreshold = scTwentyFive;
			const ScalarV scSideThreshold = ScalarV(pVehicleToFlee->GetBoundingBoxMax().x + 1.0f);

			if (IsGreaterThanAll(Dot(vehDriveDir, pVehicle->GetVehicleForwardDirection()), scZero)
				&& IsLessThanAll(Abs(scRightOffset), scSideThreshold)
				&& IsLessThanAll(scFrontOffset, scFrontThreshold)
				&& IsGreaterThanAll(scFrontOffset, scZero)
				)
			{
				HelperTellOtherVehicleToHurry(pVehicle, pVehicle->GetDriver());
			}		
		}
	}
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::StartOvertakeCurrentCar()
{
	m_pVehicleToIgnoreWhenStopping = m_pOptimization_pLastCarWeSlowedFor;

	if (m_bReverseBeforeOvertakeCurrentCarRequested && GetVehicle())
	{
		GetVehicle()->GetIntelligence()->MillisecondsNotMoving = 5000;	//pretend we're stuck
		SetState(State_ThreePointTurn);

		//return true for a state change
		return true;
	}

	return false;
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::MakeWayForCarWithSiren(CVehicle *pVehicleWithSiren)
{
	Assert(pVehicleWithSiren);

	//CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	//s32 i=VehiclePool->GetSize();	
	float	Distance, DotProd, SirenCarSpeed, DotProd2;
	float	SirenCarForwardX, SirenCarForwardY;
	float	DeltaX, DeltaY, CarSpeedX, CarSpeedY, CarSpeed;

	//we sometimes call this from the player control update, which happens while
	//the vehicle cached ai data is invalid, so we can't use it here
	Vector3 vVelocityOfSirenCar = pVehicleWithSiren->GetVelocity();
	SirenCarSpeed = vVelocityOfSirenCar.Mag();

	if (SirenCarSpeed < 0.1f) return;	// Have to go above a certain speed.

	SirenCarForwardX = vVelocityOfSirenCar.x;
	SirenCarForwardY = vVelocityOfSirenCar.y;
	SirenCarForwardX /= SirenCarSpeed;
	SirenCarForwardY /= SirenCarSpeed;

	const bool bSirenCarDriverIsPlayer = pVehicleWithSiren->GetDriver() && pVehicleWithSiren->GetDriver()->IsPlayer();

	//while(i--)
	CEntityScannerIterator entityList = pVehicleWithSiren->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		CVehicle* pVehicle = (CVehicle*)pEntity;
		CTask* pAlreadyInPursuit = pVehicle->GetDriver() ? pVehicle->GetDriver()->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_REACT_TO_PURSUIT) : NULL;

		if( pVehicle && (pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBike()) )
		{
			//check car and driver (police can commandeer civilian cars)
			bool bLawVehicleOrPed = pVehicle->IsLawEnforcementVehicle() || ( pVehicle->GetDriver() && pVehicle->GetDriver()->IsLawEnforcementPed() );
			if (!bLawVehicleOrPed && pVehicle->PopTypeIsRandom() && !pAlreadyInPursuit && pVehicle->GetStatus() == STATUS_PHYSICS && 
				(pVehicle->GetDriver() == NULL || pVehicle->GetDriver()->PopTypeIsRandom()) &&		// Chis' vigilante mission has random cars driven by mission chars
				(pVehicle!=pVehicleWithSiren) && (!pVehicle->m_nVehicleFlags.bIsAmbulanceOnDuty) 
				&& (!pVehicle->m_nVehicleFlags.bIsFireTruckOnDuty) && (!pVehicle->m_nVehicleFlags.bMadDriver) // mad drivers need to ignore sirens or it fucks up the police chases
				/*&& !pVehicle->m_nVehicleFlags.bUsingPretendOccupants*/ && !pVehicle->m_nVehicleFlags.bActAsIfHasSirenOn) 
				
			{
				if ( ABS(pVehicleWithSiren->GetVehiclePosition().GetZf() - pVehicle->GetVehiclePosition().GetZf()) < 5.0f )
				{
					DeltaX = pVehicle->GetVehiclePosition().GetXf() - pVehicleWithSiren->GetVehiclePosition().GetXf();
					DeltaY = pVehicle->GetVehiclePosition().GetYf() - pVehicleWithSiren->GetVehiclePosition().GetYf();

					Distance = rage::Sqrtf( DeltaX*DeltaX + DeltaY*DeltaY );

					if (Distance < 60.0f + SirenCarSpeed)
					{	// Close enough

						Vector3 vOtherCarVelocity = pVehicle->GetVelocity();
						CarSpeedX = vOtherCarVelocity.x;
						CarSpeedY = vOtherCarVelocity.y;
						CarSpeed = rage::Sqrtf(CarSpeedX * CarSpeedX + CarSpeedY * CarSpeedY);

						CVehicleNodeList* pNodeList = NULL;

						if (CarSpeed > 0.05f)
						{

							//CMission *pMission = CCarIntelligence::FindUberMissionForCar(pVehicle);
							//VehTempActType tempAct = TEMPACT_NONE;
							CTaskVehicleReactToCopSiren::ReactToCopSirenType reactType = CTaskVehicleReactToCopSiren::React_JustWait;
							u32 nTempActDuration = 0;
							CTaskVehicleMissionBase* pActiveTask = pVehicle->GetIntelligence()->GetActiveTask();
							const bool bAlreadyPullingOver = pActiveTask && 
								(pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_PULL_OVER || pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_PARK_NEW);
							const bool bAlreadyBrakingOrWaiting = pActiveTask 
								&& (pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_WAIT
									||pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_BRAKE);

							if (pActiveTask && pActiveTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_FLEE)	//don't do for parked/stopped/empty cars
							{
								bool bGiveNewTask = false;

								// Make sure the other car is in front of the sirened car
								DotProd = (SirenCarForwardX * DeltaX + SirenCarForwardY * DeltaY) / Distance;
								if (DotProd > 0.8f && (!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG)))	// Car is indeed ahead of the car with siren
								{	// Right in front of us.
									// If the car is driving away from us he swirves left or right

									DotProd2 = SirenCarForwardX * pVehicle->GetVehicleForwardDirection().GetXf() +
										SirenCarForwardY * pVehicle->GetVehicleForwardDirection().GetYf();
									if (DotProd2 > 0.7f || DotProd2 < -0.9f)
									{	// Swerve
										if (!bAlreadyPullingOver)
										{	// Work out whether to swirve left or right
											if (DeltaX * SirenCarForwardY - DeltaY * SirenCarForwardX > 0.0f)
											{
												//tempAct = TEMPACT_SWERVERIGHT_STOP;
												reactType = CTaskVehicleReactToCopSiren::React_PullOverRight;
											}
											else
											{
												//tempAct = TEMPACT_SWERVELEFT_STOP;
												reactType = CTaskVehicleReactToCopSiren::React_PullOverLeft;
											}

											// Swap them round if the car is coming towards us.
											if (DotProd2 < 0.0f)
											{
												//for AI-controlled vehicles, don't swerve for oncoming sirens,
												//just stop. it looks weird when the player sees ambient traffic do this
												if (!bSirenCarDriverIsPlayer)
												{
													reactType = CTaskVehicleReactToCopSiren::React_JustWait;
												}
												else if (reactType == CTaskVehicleReactToCopSiren::React_PullOverRight)
												{
													reactType = CTaskVehicleReactToCopSiren::React_PullOverLeft;
													//tempAct = TEMPACT_SWERVELEFT_STOP;
												}
												else
												{
													reactType = CTaskVehicleReactToCopSiren::React_PullOverRight;
													//tempAct = TEMPACT_SWERVERIGHT_STOP;
												}
											}

											pNodeList = pVehicle->GetIntelligence()->GetNodeList();
											bool bInLeftLane = false;
											bool bHasOpposingLaneOfTraffic = false;
											bool bHasShoulderOnLeft = true;
											if (pNodeList && reactType == CTaskVehicleReactToCopSiren::React_PullOverLeft )
											{
												s32 iTargetNodeIndex = pNodeList->GetTargetNodeIndex();
												s32 iSrcNodeIndex = iTargetNodeIndex;
												s32 iLinkIndex = pNodeList->GetPathLinkIndex(iSrcNodeIndex);
												if (iLinkIndex < 0 && iSrcNodeIndex > 0)
												{
													iSrcNodeIndex = iTargetNodeIndex-1;
													iLinkIndex = pNodeList->GetPathLinkIndex(iSrcNodeIndex);
												}

												if (iLinkIndex >= 0)
												{
													const CNodeAddress& iSrcNodeAddr = pNodeList->GetPathNodeAddr(iSrcNodeIndex);
													const u32 iSrcNodeRegion = iSrcNodeAddr.GetRegion();
													if (!iSrcNodeAddr.IsEmpty() && ThePaths.IsRegionLoaded(iSrcNodeRegion))
													{
														CPathNodeLink* pLink = ThePaths.FindLinkPointerSafe(iSrcNodeRegion,iLinkIndex);
														if (aiVerify(pLink))
														{
															bHasOpposingLaneOfTraffic = pLink->m_1.m_LanesFromOtherNode > 0;
															bInLeftLane = pNodeList->GetPathLaneIndex(iSrcNodeIndex) == 0;

															//TODO: do something to set bHasSHoulderOnLeft
														}
													}
												}
											}

											// Don't ever swirve left. It confuses the player.
											// Unless we're already in the left lane and don't have any traffic coming
											// in the other direction
											if (reactType == CTaskVehicleReactToCopSiren::React_PullOverLeft && !(bInLeftLane && !bHasOpposingLaneOfTraffic && bHasShoulderOnLeft))
											{
												//tempAct = TEMPACT_WAIT;
												reactType = CTaskVehicleReactToCopSiren::React_JustWait;
											}

											nTempActDuration = 6000;
											bGiveNewTask = true;
										}
									}
									else
									{	// Only if this guy heads in our direction will we make him stop
										if ( vOtherCarVelocity.x*DeltaX + vOtherCarVelocity.y*DeltaY < 0.0f)
										{
											if (!bAlreadyBrakingOrWaiting)
											{
												reactType = CTaskVehicleReactToCopSiren::React_JustWait;
												nTempActDuration = 6000;
												bGiveNewTask = true;
											}
										}
									}
								}
								else
								{	// Only if this guy heads in our direction will we make him stop
									if ( vOtherCarVelocity.x*DeltaX + vOtherCarVelocity.y*DeltaY < 0.0f)
									{
										if (!bAlreadyBrakingOrWaiting)
										{
											reactType = CTaskVehicleReactToCopSiren::React_JustWait;
											nTempActDuration = 3000;
											bGiveNewTask = true;
										}
									}
								}

								if (bGiveNewTask)
								{
									//aiTask* pTask = reactType == CTaskVehicleReactToCopSiren::React_JustWait
									//	? CVehicleIntelligence::GetTempActionTaskFromTempActionType(tempAct, nTempActDuration)
									//	: rage_new CTaskVehiclePullOver(true, pulloverDir, 4000, true);
									aiTask* pTask = rage_new CTaskVehicleReactToCopSiren(reactType, nTempActDuration);
								
									pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);

									if (pVehicle->m_nVehicleFlags.bUsingPretendOccupants)
									{
										sVehicleMissionParams pullOverParams;
										pVehicle->GetIntelligence()->SetPretendOccupantEventData(VehicleEventPriority::VEHICLE_EVENT_PRIORITY_RESPOND_TO_SIREN, pullOverParams);
									}
									//Color32 sphereColor = reactType == CTaskVehicleReactToCopSiren::React_JustWait ? Color_orange : Color_red;
									//grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), 2.0f, sphereColor, false, 15);
								}
							}
						}
					}
				}
			}
		}
	}
}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::UpdatePlayerOnFootIsBlockingUs(CVehicle* pVeh, CPed* pDriver, CPed* pBlockingPed, float *pMaxSpeed, const float fDistFromSideOfCar, const float fDistFromFrontOfCarBumper)
{
	//this should have been set above
	Assert(m_startTimeWaitingForPlayerPed > 0);
	const int iTimeToWaitForPlayer = CDriverPersonality::FindDelayBeforeAcceleratingAfterObstructionGone(pDriver, pVeh, true);
	const int iTimeToWaitBeforeSwearingMs = iTimeToWaitForPlayer - 1;
	const int iTimeToWaitBeforeHonkingMs = (2 * iTimeToWaitForPlayer) - 1;
	const int iTimeToWaitBeforePushingPedOutOfWayMs = (3 * iTimeToWaitForPlayer) - 1;
	const int iTimeWaitingForPlayer = fwTimer::GetTimeInMilliseconds() - m_startTimeWaitingForPlayerPed;

	const bool bPlayerIsGettingUp = pBlockingPed->GetPedIntelligence()->IsPedGettingUp();
	const bool bPlayerIsInRagdoll = pBlockingPed->GetRagdollState()==RAGDOLL_STATE_PHYS;
	if (bPlayerIsInRagdoll || bPlayerIsGettingUp)
	{
		//don't honk or anything if they're knocked over
		SetState(State_TemporarilyBrake);
		return;
	}

	//Check if we will try to push the ped out of the way.
	bool bIsDriverAgitated = (pDriver && pDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated));
	bool bWillTryToPushPedOutOfWay = !bIsDriverAgitated;

	static dev_float s_fDistFromEdgeToConsiderSlightlyBlocked = 0.25f;
	static dev_float s_fDistFromFrontOfCarToConsiderThreePointTurn = 2.0f;

	//if the ped's only slightly blocking us, get antsy faster
	bool bForceRunOverNow = false;
	if (fDistFromSideOfCar < s_fDistFromEdgeToConsiderSlightlyBlocked
		&& iTimeWaitingForPlayer > iTimeToWaitBeforeSwearingMs)
	{
		bForceRunOverNow = true;
	}

	if ((bWillTryToPushPedOutOfWay && (bForceRunOverNow || (iTimeWaitingForPlayer > iTimeToWaitBeforePushingPedOutOfWayMs)))
		|| ((fwTimer::GetTimeInMilliseconds() - pVeh->GetIntelligence()->m_lastTimeTriedToPushPlayerPed < 30000) && pVeh->GetIntelligence()->m_lastTimeTriedToPushPlayerPed > 0))
	{
		//don't wait again here, we wanna push through
		*pMaxSpeed = 3.0f;
		m_bSlowlyPushingPlayerThisFrame = true;
		pVeh->GetIntelligence()->m_lastTimeTriedToPushPlayerPed = fwTimer::GetTimeInMilliseconds();

		if (fDistFromFrontOfCarBumper < s_fDistFromFrontOfCarToConsiderThreePointTurn)
		{
			pVeh->GetIntelligence()->MillisecondsNotMoving = 5000;	//pretend we're stuck
			SetState(State_ThreePointTurn);
		}
	}
	else if (iTimeWaitingForPlayer > iTimeToWaitBeforeHonkingMs)
	{
		bool bWillBecomeAgitated = (pDriver && pDriver->PopTypeIsRandom() &&
			pDriver->IsLawEnforcementPed() && !bIsDriverAgitated);
		if(bWillBecomeAgitated)
		{
			//Add an agitated event.
			CEventAgitated event(pBlockingPed, AT_Loitering);
			pDriver->GetPedIntelligence()->AddEvent(event);
		}
		else
		{
			mthRandom rnd(pVeh->GetRandomSeed());
			if(pDriver && pDriver->CheckBraveryFlags(BF_PLAY_CAR_HORN))
			{
				//only a small chance of flicking the player off when we honk here
				const bool bRestrictDriveBy = rnd.GetFloat() < 0.05f;
				HelperMaybePlayHorn(pVeh, true, true, pBlockingPed, bRestrictDriveBy);
			}
			else if (pDriver)
			{
				if (rnd.GetFloat() < 0.125f)
				{
					HelperFlipOffTarget(pVeh, pDriver, pBlockingPed);
				}
				if(!pBlockingPed->IsDead())
				{
					pDriver->NewSay("BLOCKED_GENERIC");
				}
			}
		}

		SetState(State_TemporarilyBrake);
	}
	else if (iTimeWaitingForPlayer > iTimeToWaitBeforeSwearingMs)
	{
		if (pDriver)
		{
			pDriver->NewSay("BLOCKED_GENERIC");
		}
		SetState(State_TemporarilyBrake);
	}
	else
	{
		SetState(State_TemporarilyBrake);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SlowCarDownForPedsSectorList
// PURPOSE :  Processes one sector list.(Slows down for the cars in it)
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::SlowCarDownForPedsSectorList(CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, bool bWasSlowlyPushingPlayer)
{
	SlowCarDownForPedsSectorListHelper(this, pVeh, MinX, MinY, MaxX, MaxY, pMaxSpeed, OriginalMaxSpeed, bWasSlowlyPushingPlayer);
}

#define BIKE_WIDTH_MULTIPLIER 		1.6f
#define BRAKE_DISTANCE 				13.0f
#define BRAKE_DISTANCE_PLAYER 		15.0f

void CTaskVehicleGoToPointWithAvoidanceAutomobile::SlowCarDownForPedsSectorListHelper(CTaskVehicleGoToPointWithAvoidanceAutomobile* pTask, 
		CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, bool bWasSlowlyPushingPlayer)
{
	//NOTE: Do not use GetAiVelocity inside of this function, it is indirectly
	//		called from places that are not part of the vehicle AI update!

	float	DotFront, DotSide, CarFrontSize, CarRearSize, CarRightSize, VelFront, VelRight;
	Vector3	Delta;

	if(pTask)
	{
		pTask->m_bSlowingDownForPed = false;
	}

	CPedScanner& scanner = pVeh->GetIntelligence()->GetPedScanner();
	const int numEntities = scanner.GetNumEntities();
	if(!numEntities)
	{
		return;
	}

	if (!aiVerifyf(pVeh->pHandling, "No vehicle handling info found for player car."))
	{
		return;
	}

	const Vector3& vBoundBoxMax = pVeh->GetBoundingBoxMax();
	CarRightSize = vBoundBoxMax.x;
	// make bikes a bit wider cause it might be leant over a bit
	if(pVeh->InheritsFromBike())
	{
		CarRightSize *= BIKE_WIDTH_MULTIPLIER;
	}

	VelFront = DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()), pVeh->GetVelocity());
	VelRight = DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()), pVeh->GetVelocity());

	CPed* pDriver = pVeh->GetDriver();

	TUNE_GROUP_BOOL(CAR_AI, UseNewStopDistance, true);
	const float fBrakingMinDist = UseNewStopDistance ? 
		pVeh->GetAIHandlingInfo()->GetMaxBrakeDistance()
		: Max(CDriverPersonality::GetStopDistanceForPeds(pDriver, pVeh), VelFront);

	// if the player's using their horn then extend the distance forward that peds will react to avoid car
	// 	if(pVeh->pDriver && pVeh->pDriver->IsPlayer() && pVeh->IsHornOn())
	// 		fWarnPedMinDist *= 2.0f;

	const bool bPlayingHorn = pDriver && pDriver->IsPlayer() && pVeh->IsHornOn();
	const bool bHasRecentlyRunSomeoneOver = pDriver && pDriver->IsPlayer() && (CTimeHelpers::GetTimeSince(pDriver->GetPlayerInfo()->GetLastTimeBumpedPedInVehicle()) < 7500);

	// Don't bother to avoid cars driving safely on rails					
	bool bShouldBeAvoided = 
		(pDriver && pDriver->IsPlayer()) ||
		pVeh->InheritsFromTrain() ||
		(!(pTask && pTask->IsDrivingFlagSet(DF_StopForCars))) ||
		pVeh->GetStatus() == STATUS_PHYSICS ||
 		(pTask && pTask->IsDrivingFlagSet(DMode_PloughThrough));	// @RSGNWE-JW make sure to dodge them crazy drivers.
	if(bShouldBeAvoided)
	{
		//Check if the vehicle is a heli.
		if(pVeh->InheritsFromHeli())
		{
			//Check if the flat speed does not exceed the threshold.
			const Vector3& vVelocity = pVeh->GetVelocity();
			float fXYSpeedSq = vVelocity.XYMag2();
			static dev_float s_fMinXYSpeed = 5.0f;
			float fMinXYSpeedSq = square(s_fMinXYSpeed);
			//if(pVeh->GetAiXYSpeed() < s_fMinXYSpeed)
			if (fXYSpeedSq < fMinXYSpeedSq)
			{
				bShouldBeAvoided = false;
			}
		}
	}

	CarFrontSize = vBoundBoxMax.y;
	CarRearSize = pVeh->GetBoundingBoxMin().y;

	//Calculate the multiplier.
	float fMultiplier = 1.0f;
	if(pVeh->GetDrivenDangerouslyTime() > sm_Tunables.m_MinTimeToConsiderDangerousDriving)
	{
		fMultiplier *= sm_Tunables.m_MultiplierForDangerousDriving;
	}

	float fSpeedAboveMin = rage::Max(0.0f, VelFront - sm_Tunables.m_MinSpeedForAvoid);
	if(VelFront < 0.0f)
		fSpeedAboveMin = -VelFront;

	const float fPercBetweenMinAndMax = fSpeedAboveMin /(sm_Tunables.m_MaxSpeedForAvoid - sm_Tunables.m_MinSpeedForAvoid);
	float fAvoidInFrontDistance	= 0.0f;
	float fAvoidToSideDistance	= 0.0f;
	if(fSpeedAboveMin > 0.0f)
	{
		fAvoidInFrontDistance = sm_Tunables.m_MinDistanceForAvoid +(fPercBetweenMinAndMax *(sm_Tunables.m_MaxDistanceForAvoid - sm_Tunables.m_MinDistanceForAvoid));
		fAvoidInFrontDistance *= fMultiplier;

		//When driving on pavement, avoid a bit more to the side.
		if(pVeh->m_nVehicleFlags.bHasWheelOnPavement && !pVeh->InheritsFromBike())
		{
			fAvoidToSideDistance = (fPercBetweenMinAndMax *(sm_Tunables.m_MaxDistanceToSideOnPavement - sm_Tunables.m_MinDistanceToSideOnPavement));
		}

		//When skidding, add some side distance based on velocity.
		float fScale = (Max(0.0f, VelRight) * ms_vNmVelocityScale.x);
		float fAmount = (CarRightSize * (1.0f + fScale)) - CarRightSize;
		fAvoidToSideDistance += fAmount;

		fAvoidToSideDistance *= fMultiplier;
	}

	Vector2 vAvoidXExtents	(ms_vAvoidXExtents.x + CarRightSize + fAvoidToSideDistance, CarFrontSize + fAvoidInFrontDistance);
	Vector2 vThreatXExtents (ms_vAvoidXExtents.x + CarRightSize, CarFrontSize + fAvoidInFrontDistance);
	Vector4 vNmXExtents	(	ms_vNmXExtents.x + (CarRightSize * (1 + (rage::Abs(rage::Min(0.0f,VelRight)) * ms_vNmVelocityScale.x))),	//LEFT
							ms_vNmXExtents.x + (CarRightSize * (1 + (rage::Max(0.0f,VelRight) * ms_vNmVelocityScale.x))),	//RIGHT
							CarFrontSize + rage::Min((Max(0.0f,VelFront)) * ms_vNmVelocityScale.y, sm_Tunables.m_MinDistanceForAvoid),	//FRONT 
							rage::Abs(CarRearSize) + rage::Min(Abs(Min(0.0f,VelFront)* ms_vNmVelocityScale.y), sm_Tunables.m_MinDistanceForAvoid));	//BACK
	if(VelFront < 0.0f)
	{
		vAvoidXExtents	= Vector2(ms_vAvoidXExtents.x + CarRightSize + fAvoidToSideDistance, CarRearSize - fAvoidInFrontDistance);
		vThreatXExtents	= Vector2(ms_vAvoidXExtents.x + CarRightSize, CarRearSize - fAvoidInFrontDistance);
		vNmXExtents		= Vector4(	ms_vNmXExtents.x + (CarRightSize * (1 + (rage::Abs(Min(0.0f,VelRight) * ms_vNmVelocityScale.x)))),
									ms_vNmXExtents.x + (CarRightSize * (1 + (rage::Max(0.0f,VelRight) * ms_vNmVelocityScale.x))),
									CarFrontSize + rage::Max((Max(0.0f,VelFront)) * ms_vNmVelocityScale.y, -sm_Tunables.m_MinDistanceForAvoid),
									rage::Abs(CarRearSize) + rage::Min(rage::Abs(rage::Min(0.0f,VelFront) * ms_vNmVelocityScale.y), sm_Tunables.m_MinDistanceForAvoid));
	}

	float fMinSpeedForAvoidDirected = sm_Tunables.m_MinSpeedForAvoidDirected;
	float fMaxSpeedForAvoidDirected = sm_Tunables.m_MaxSpeedForAvoidDirected;
	float fSpeedForAvoidDirected = Clamp(Abs(VelFront), fMinSpeedForAvoidDirected, fMaxSpeedForAvoidDirected);
	float fLerp = (fSpeedForAvoidDirected - fMinSpeedForAvoidDirected) / (fMaxSpeedForAvoidDirected - fMinSpeedForAvoidDirected);

	float fMinDistanceForAvoidDirected = sm_Tunables.m_MinDistanceForAvoidDirected;
	float fMaxDistanceForAvoidDirected = sm_Tunables.m_MaxDistanceForAvoidDirected;
	float fDistanceForAvoidDirected = Lerp(fLerp, fMinDistanceForAvoidDirected, fMaxDistanceForAvoidDirected);
	fDistanceForAvoidDirected *= fMultiplier;

	Vector2 vAvoidDirectedExtents = vAvoidXExtents;
	vAvoidDirectedExtents.x += fDistanceForAvoidDirected;
	vAvoidDirectedExtents.y = (VelFront >= 0.0f) ? CarFrontSize + fDistanceForAvoidDirected : CarRearSize - fDistanceForAvoidDirected;

#if __DEV
	TUNE_GROUP_BOOL(CAR_AI, RenderAvoidanceWarnings, false);
	if(RenderAvoidanceWarnings && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Vector3 avNmWarningBox[4];
		Vector3 avAvoidWarningBox[4];
		Vector3 avAvoidDirectedWarningBox[4];
		Vector3 VehicleRightVector(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetA()));
		Vector3 VehicleForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));

		Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		avNmWarningBox[0] = vecVehiclePosition - (VehicleRightVector * vNmXExtents.x) - (VehicleForwardVector * vNmXExtents.w);					// MiddleLeft
		avNmWarningBox[1] = vecVehiclePosition - (VehicleRightVector * vNmXExtents.x) + (VehicleForwardVector * vNmXExtents.z);					// ForwardLeft
		avNmWarningBox[2] = vecVehiclePosition + (VehicleRightVector * vNmXExtents.y) + (VehicleForwardVector * vNmXExtents.z);					// MiddleRight
		avNmWarningBox[3] = vecVehiclePosition + (VehicleRightVector * vNmXExtents.y) - (VehicleForwardVector * vNmXExtents.w);					// ForwardRight

		//////////////////////////////////////////////////////////////////////////
		// @RSNWE-JW
		// Offsetting to get rid of the annoying Z-fighting issue.
		for (int i = 0; i < 4; ++i)
		{
			avNmWarningBox[i].SetZ(avNmWarningBox[i].GetZ() + 0.1f);
		}
		
		avAvoidWarningBox[0] = vecVehiclePosition -(VehicleRightVector * vAvoidXExtents.x);								// MiddleLeft
		avAvoidWarningBox[1] = avAvoidWarningBox[0] +	(VehicleForwardVector * vAvoidXExtents.y);						// ForwardLeft
		avAvoidWarningBox[2] = vecVehiclePosition +(VehicleRightVector * vAvoidXExtents.x);								// MiddleRight
		avAvoidWarningBox[3] = avAvoidWarningBox[2] +	(VehicleForwardVector * vAvoidXExtents.y);						// ForwardRight

		avAvoidDirectedWarningBox[0] = vecVehiclePosition -(VehicleRightVector * vAvoidDirectedExtents.x);					// MiddleLeft
		avAvoidDirectedWarningBox[1] = avAvoidDirectedWarningBox[0] +	(VehicleForwardVector * vAvoidDirectedExtents.y);	// ForwardLeft
		avAvoidDirectedWarningBox[2] = vecVehiclePosition +(VehicleRightVector * vAvoidDirectedExtents.x);					// MiddleRight
		avAvoidDirectedWarningBox[3] = avAvoidDirectedWarningBox[2] +	(VehicleForwardVector * vAvoidDirectedExtents.y);	// ForwardRight

		for (int i = 0; i < 4; ++i)
		{
			avAvoidDirectedWarningBox[i].SetZ(avAvoidDirectedWarningBox[i].GetZ() - 0.1f);
		}

		grcDebugDraw::Poly(RCC_VEC3V(avNmWarningBox[0]), RCC_VEC3V(avNmWarningBox[1]), RCC_VEC3V(avNmWarningBox[2]), Color_red, true);
		grcDebugDraw::Poly(RCC_VEC3V(avNmWarningBox[3]), RCC_VEC3V(avNmWarningBox[2]), RCC_VEC3V(avNmWarningBox[0]), Color_red, true);

		grcDebugDraw::Poly(RCC_VEC3V(avAvoidWarningBox[0]), RCC_VEC3V(avAvoidWarningBox[1]), RCC_VEC3V(avAvoidWarningBox[3]), Color_blue, true);
		grcDebugDraw::Poly(RCC_VEC3V(avAvoidWarningBox[3]), RCC_VEC3V(avAvoidWarningBox[2]), RCC_VEC3V(avAvoidWarningBox[0]), Color_blue, true);

		grcDebugDraw::Poly(RCC_VEC3V(avAvoidDirectedWarningBox[0]), RCC_VEC3V(avAvoidDirectedWarningBox[1]), RCC_VEC3V(avAvoidDirectedWarningBox[3]), Color_purple, true);
		grcDebugDraw::Poly(RCC_VEC3V(avAvoidDirectedWarningBox[3]), RCC_VEC3V(avAvoidDirectedWarningBox[2]), RCC_VEC3V(avAvoidDirectedWarningBox[0]), Color_purple, true);

		//////////////////////////////////////////////////////////////////////////
		// @RSNWE-JW
		// Generate another debug draw plane for helping to visualize the 
		// side the ped needs to dodge towards.
		Vector3 avDividingPlane[4];
		avDividingPlane[0] = vecVehiclePosition;																	// BackBottom
		avDividingPlane[1] = avDividingPlane[0] + (VehicleForwardVector * vAvoidXExtents.y);						// ForwardBottom
		avDividingPlane[2] = avDividingPlane[0];																	// BackTop
		avDividingPlane[2].SetZ(avDividingPlane[2].GetZ() + 2.0f);
		avDividingPlane[3] = avDividingPlane[1];																	// ForwardTop
		avDividingPlane[3].SetZ(avDividingPlane[1].GetZ() + 2.0f);
		grcDebugDraw::Poly(RCC_VEC3V(avDividingPlane[0]), RCC_VEC3V(avDividingPlane[1]), RCC_VEC3V(avDividingPlane[3]), Color_orange, true);
		grcDebugDraw::Poly(RCC_VEC3V(avDividingPlane[3]), RCC_VEC3V(avDividingPlane[2]), RCC_VEC3V(avDividingPlane[0]), Color_orange, true);

//		Vector3 avScanBox[4];
//		avScanBox[0] = Vector3(MinX, MinY, vecVehiclePosition.z-0.1f);
// 		avScanBox[1] = Vector3(MinX, MaxY, vecVehiclePosition.z-0.1f);
// 		avScanBox[2] = Vector3(MaxX, MaxY, vecVehiclePosition.z-0.1f);
// 		avScanBox[3] = Vector3(MaxX, MinY, vecVehiclePosition.z-0.1f);
//		grcDebugDraw::Poly(RCC_VEC3V(avScanBox[0]), RCC_VEC3V(avScanBox[1]), RCC_VEC3V(avScanBox[2]), Color_DarkSeaGreen, false);
//		grcDebugDraw::Poly(RCC_VEC3V(avScanBox[3]), RCC_VEC3V(avScanBox[2]), RCC_VEC3V(avScanBox[0]), Color_DarkSeaGreen, false);
	}
#endif // __DEV

	// Get the current drive direction and orientation.
	const Vector3 vehFwdDir = VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection());
	//////////////////////////////////////////////////////////////////////////
	// @RSNWE-JW
	// Magic removed due to the cold, hard reality that reversing the 
	// vehDriveDir prevents a Ped from dodging a reversing car.

	const CPhysical* pFleeEntity = pVeh->GetIntelligence()->GetFleeEntity();

	const bool bOnJunctionWithTrainApproaching = pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO
		&& pVeh->GetIntelligence()->GetJunction() && pVeh->GetIntelligence()->GetJunction()->ShouldCarsStopForTrain();

	for(int entityIndex = 0; entityIndex < numEntities; entityIndex++)
	{
		const int nextEntityIndex = entityIndex + 1;
		if(nextEntityIndex < numEntities)
		{
			CEntity* pNextEntity = scanner.GetEntityByIndex(nextEntityIndex);
			if(pNextEntity)
			{
				// Prefetch some stuff that we are likely to need for the next ped:
				// - The first 256 bytes or so, for matrix pointers, IsBaseFlagSet(), etc.
				// - Config flags, for GetIsInVehicle(), etc.
				PrefetchBuffer<256>(pNextEntity);
				static_cast<CPed*>(pNextEntity)->PrefetchPedConfigFlags();
			}
		}

		CEntity* pEntity = scanner.GetEntityByIndex(entityIndex);
		if(!pEntity)
		{
			continue;
		}

		Assert(pEntity->GetIsTypePed());
		CPed * pPed = (CPed*)pEntity;

		const bool bPedIsStationaryForAvoidance = (pPed->IsDead());
		if(!bPedIsStationaryForAvoidance)
		{
			//don't stop for the thing we're fleeing
			if (pPed == pFleeEntity)
			{
				continue;
			}

			//skip peds in vehicles
			if (pPed->GetIsInVehicle())
			{
				continue;
			}

			//skip peds on mounts
			if (pPed->GetIsOnMount())
			{
				continue;
			}

			//skip peds about to be removed
			if (pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				continue;
			}

			//if this is the player and we were driving around him recently,
			//don't stop if he gets in the way
			if (bWasSlowlyPushingPlayer && pPed->IsPlayer())
			{
				continue;
			}

			if (pPed->GetCapsuleInfo()->IsBird())
			{
				continue;
			}

			Vector3 centre = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			const Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
			if(centre.x > MinX && centre.x < MaxX &&
				centre.y > MinY && centre.y < MaxY &&
				rage::Abs(centre.z - vecVehiclePosition.z) < 6.0f)
			{
				// Do a slightly more sophisticated z check. Work out what the height
				// of our car would be at the point nearest the other car.

				const float DistAhead = fwGeom::DistAlongLine2D(vecVehiclePosition.x, vecVehiclePosition.y, vehFwdDir.x, vehFwdDir.y, centre.x, centre.y);
				const float OurHeight = vecVehiclePosition.z + DistAhead * vehFwdDir.z;
				
				if(rage::Abs(centre.z - OurHeight) < 2.5f)	// This was 3.0 but there are subway tunnels RIGHT below the road surface.
				{
					// Now actually do a collision threat analysis for this car
					Delta = centre - vecVehiclePosition;
					DotFront = DotProduct(vehFwdDir, Delta);

					// Car braking
					// Only do this stuff if the car is friendly enough to stop for peds
					//(Not necessarily the case since the 'pedsjumpingoutoftheway' stuff will always call this function.	
					if(pTask && *pMaxSpeed > 0.0f && pTask->IsDrivingFlagSet(DF_StopForPeds) && !bOnJunctionWithTrainApproaching &&
						!HelperPedIsStationaryForAvoidance(pPed))
					{
						CPed *pPlayerPed = NULL;
						if(((CPed*)pEntity)->IsAPlayerPed())
							pPlayerPed = (CPed*)pEntity;

						if(!pEntity || !pPlayerPed || pPlayerPed->GetPlayerInfo()->GetLastTargetVehicle()!=pVeh)
						{
							//don't stop if the ped is within our bounding box on the font--he's probably standing on top of our car
							//so add a z check to make sure we stop for cars that might be underneath our car, even if they're 
							//within the bounding box
							if((DotFront > CarFrontSize || (centre.z < vecVehiclePosition.z && DotFront > 0.0f)) && DotFront - CarFrontSize < fBrakingMinDist)
							{
								DotSide = rage::Abs(DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()), Delta));

								if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad))
								{
									//estimate the ped's position at the time of collision, and test against whichever
									//point is closer to the car
									const float fEstTimeToCollision = (OriginalMaxSpeed >= SMALL_FLOAT && DotFront >= SMALL_FLOAT) ? DotFront / OriginalMaxSpeed : FLT_MAX;
									const Vector3 vEstFuturePosition = centre + (pPed->GetVelocity() * fEstTimeToCollision);

									//grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(centre), VECTOR3_TO_VEC3V(vEstFuturePosition), 0.25f, Color_cyan);

									if (fEstTimeToCollision < FLT_MAX)
									{
										DotFront = rage::Min(DotFront, DotProduct(vehFwdDir, (vEstFuturePosition - vecVehiclePosition)));
										DotSide = rage::Min(DotSide, rage::Abs(DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()), (vEstFuturePosition - vecVehiclePosition))));
									}
								}	

								const float fPadding = (pPed->IsLawEnforcementPed() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad)) ? 1.0f : 0.5f;
								if(DotSide <= CarRightSize + fPadding)
								{	
									const float fBrakeDistance = UseNewStopDistance ? Max(pVeh->GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(VelFront), pVeh->pHandling->GetAIHandlingInfo()->GetMinBrakeDistance())
										: ((pPlayerPed) ? BRAKE_DISTANCE_PLAYER:BRAKE_DISTANCE);

									// only do Car braking if within braking distance
									if((DotFront - CarFrontSize) < fBrakeDistance)
									{
										Assert(pEntity->GetIsTypePed());
										// For certain scenarios we don't stop for the ped as the ped will be close to a car. We avoid the car instead so that we can drive round it.
										s32 runningScenario =((CPed *)pEntity)->GetPedIntelligence()->GetQueriableInterface()->GetRunningScenarioType();
										const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(runningScenario);
										if( !pScenarioInfo || (pScenarioInfo && !pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoredByCars)) )
										{
											// We might have to slow down a bit
											static dev_float fTweakDistance = 1.0f;
											float fRatio = (DotFront - CarFrontSize - fTweakDistance) / fBrakeDistance;
											
											//If the ped is jay walking then increase the stopping distance. Let the standard behavior handle the rest.
											if(pPed->GetPedResetFlag(CPED_RESET_FLAG_JayWalking))
											{
												fRatio *= 1.5f;
											}

											fRatio = Clamp(fRatio, 0.0f, 1.0f);
											*pMaxSpeed = Clamp(fRatio * OriginalMaxSpeed, 0.0f, *pMaxSpeed );

#if __BANK
											if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
											{
												grcDebugDraw::Sphere(vecVehiclePosition, 2.0f, Color_yellow2, false);
											}
#endif //__BANK

											// Set flag manually if saved speed is used
											pTask->m_bSlowingDownForPed = true;

											//pTask->m_bShouldChangeLanesForTraffic = pTask->CanSwitchLanesToAvoidObstruction(pVeh, pPed, OriginalMaxSpeed);

											// Keep a note of the obstructive ped.
											pVeh->GetIntelligence()->m_pObstructingEntity = (CPhysical*)pEntity;

											//if(pUberMission)		// Only respond if we actually have a driver in control.

											if(DotFront - CarFrontSize < CDriverPersonality::GetStopDistanceForPeds(pDriver, pVeh))
											{
												pTask->SetWaitingForPlayerPed(pPlayerPed != NULL);

												if (pPlayerPed != NULL)
												{
													pTask->UpdatePlayerOnFootIsBlockingUs(pVeh, pDriver, pPlayerPed, pMaxSpeed, CarRightSize - DotSide, DotFront - CarFrontSize);
												}
												else
												{
													pTask->SetState(State_TemporarilyBrake);
												}

#if __BANK
												if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
												{
													grcDebugDraw::Sphere(vecVehiclePosition, 2.0f, Color_red, false);
												}
#endif //__BANK
											}
// 											else if(DotFront - CarFrontSize < CDriverPersonality::GetDriveSlowDistanceForPeds(pDriver))
// 											{
// 												pTask->SetWaitingForPlayerPed(pPlayerPed != NULL);
// 												pTask->SetState(State_WaitForPed);
// 
// #if __BANK
// 												if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
// 												{
// 													grcDebugDraw::Sphere(vecVehiclePosition, 2.0f, Color_orange, false);
// 												}
// #endif //__BANK
// 											}

											// If this is the player or a non-cop ped
											const int iModDenominator = pTask->m_bSlowlyPushingPlayerThisFrame ? 6 : 3;
											if(pDriver && pDriver->CheckBraveryFlags(BF_PLAY_CAR_HORN)
												&& (((pVeh->GetRandomSeed() + fwTimer::GetSystemFrameCount()) & iModDenominator) == 2)
												&& (pPlayerPed || !pPed->IsLawEnforcementPed()))
											{
												const u32 nCurrentTimeMs = fwTimer::GetTimeInMilliseconds();
												const u32 nTimeBtwnHonks = CDriverPersonality::GetTimeBetweenHonkingAtPeds(pDriver, pVeh);
												if (nCurrentTimeMs > pTask->m_lastTimeHonkedAtAnyPed + nTimeBtwnHonks)
												{
													//if it's been a while since we've honked at anyone, always start with a honk
													//otherwise, 50/50 chance of honking vs. playing BLOCKED_GENERIC
													if (nCurrentTimeMs > pTask->m_lastTimeHonkedAtAnyPed + (nTimeBtwnHonks * 2))
													{
														HelperMaybePlayHorn(pVeh, true, false, (CPhysical*)pEntity);
														if (OriginalMaxSpeed > 20.0f)
														{
															pDriver->NewSay("CAR_SLOW_DOWN_CURSE_HIGH");
														}
														else
														{
															pDriver->NewSay("CAR_SLOW_DOWN_CURSE_MED");
														}
													}
													else if (g_ReplayRand.GetRanged(0, 3) < 2)
													{
														HelperMaybePlayHorn(pVeh, true, g_ReplayRand.GetBool(), (CPhysical*)pEntity);
													}
													else
													{
														pDriver->NewSay("BLOCKED_GENERIC");
													}

													//always update the timer, even if we do a BLOCKED_GENERIC
													pTask->m_lastTimeHonkedAtAnyPed = fwTimer::GetTimeInMilliseconds();
												}
											}
										}
									}
								}
							}
						}
					}
					
					// Ped avoidance
					// We want peds to avoid cars which are reversing as well, and so
					// will need a separate section from the car-braking bit above
					// Want to Let Ped know it's in danger - and to step or dive away
					if(pEntity->GetIsTypePed())
					{
						//don't fire avoidance event for mission peds when we're not stopping
						if(pTask && !pTask->IsDrivingFlagSet(DF_StopForPeds) && pPed->PopTypeIsMission())
						{
							continue;
						}

						CPed *pPed =(CPed *)pEntity;
						bool bPedStandingOnCar = pPed->GetGroundPhysical() == pVeh;

#if __DEV
						//////////////////////////////////////////////////////////////////////////
						// @RSNWE-JW
						// Draw the expected direction in which a dodging Ped should dodge
						// towards.
						TUNE_GROUP_BOOL(CAR_AI, RenderExpectedDodgeDirection, false);
						if(RenderExpectedDodgeDirection && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
						{
							Vector3 vecVehicleRightVector = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetA());
							Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
							Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							Vector3 vecArrowEndPosition = vecVehicleRightVector * 3.0f;
							Vector3 vecPedOffset = vecPedPosition - vecVehiclePosition;
							if (0.0f > DotProduct(vecVehicleRightVector, vecPedOffset))
							{
								vecArrowEndPosition = -vecArrowEndPosition;
							}

							grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vecPedPosition), VECTOR3_TO_VEC3V(vecPedPosition + vecArrowEndPosition), 1.0f, Color_purple);
						}
#endif
						
						if(bShouldBeAvoided && !bPedStandingOnCar && VelFront != 0.0f &&
							rage::Abs(VelFront) > CVehiclePotentialCollisionScanner::ms_fMinAvoidSpeed &&
							!pPed->GetBlockingOfNonTemporaryEvents() && // Designers probably don't want any of these reactions if non-temporary events are blocked.
							!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableBraceForImpact) )
						{
							DotFront = DotProduct(vehFwdDir, Delta);
							DotSide = rage::Abs(DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()), Delta));
							float absDotFront = rage::Abs(DotFront);	

							bool bPedSeenCar = bPlayingHorn || bHasRecentlyRunSomeoneOver || (absDotFront < CarFrontSize) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AlwaysSeeApproachingVehicles) || DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()), Delta) < 0.0f;
							if(!bPedSeenCar)
							{
								bPedSeenCar = (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sm_Tunables.m_ChanceOfPedSeeingCarFromBehind);
							}
							
							if(bPedSeenCar)
							{
								bool bIsThreatened = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE);
								Vector2* pAvoidExtents = !bIsThreatened ? &vAvoidXExtents : &vThreatXExtents;
								float fExtraExtents = !bIsThreatened ? 2.0f : 0.0f;

								bool bInDiveYArea = rage::Abs(DotFront) > CarFrontSize && SIGN(DotFront) == SIGN(VelFront) && DotFront < pAvoidExtents->y && DotFront > CarFrontSize + fExtraExtents;
								if(VelFront < 0.0f)
									bInDiveYArea = DotFront > pAvoidExtents->y && DotFront < CarRearSize - fExtraExtents;

								bool bIsInDirectedArea = (SIGN(DotFront) == SIGN(VelFront)) && (Abs(DotFront) <= vAvoidDirectedExtents.y) &&
									(Abs(DotSide) <= vAvoidDirectedExtents.x);

								bool bIsAffectedByDirectedArea = bIsInDirectedArea && !bIsThreatened;
								if(bIsAffectedByDirectedArea)
								{
									if(pPed->IsSecurityPed() && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInStationaryScenario))
									{
										bIsAffectedByDirectedArea = false;
									}
								}
								if(bIsAffectedByDirectedArea)
								{
									const CTaskMoveCrossRoadAtTrafficLights* pTask = static_cast<CTaskMoveCrossRoadAtTrafficLights *>(
										pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS));
									if(pTask && !pTask->IsCrossingRoad())
									{
										bIsAffectedByDirectedArea = false;
									}
								}

								if(bIsAffectedByDirectedArea)
								{
									Vec3V vPedVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
									Vec3V vPedForward = pPed->GetTransform().GetForward();
									Vec3V vPedDirection = NormalizeFastSafe(vPedVelocity, vPedForward);
									Vec3V vVehVelocity = VECTOR3_TO_VEC3V(pVeh->GetVelocity());
									Vec3V vVehForward = pVeh->GetTransform().GetForward();
									Vec3V vVehDirection = NormalizeFastSafe(vVehVelocity, vVehForward);
									ScalarV scAbsDot = Abs(Dot(vPedDirection, vVehDirection));
									ScalarV scMaxDot = ScalarVFromF32(sm_Tunables.m_MaxAbsDotForAvoidDirected);
									if(IsGreaterThanAll(scAbsDot, scMaxDot))
									{
										bIsAffectedByDirectedArea = false;
									}
								}

								Vector3 pPedOffset = VEC3V_TO_VECTOR3(pVeh->GetTransform().UnTransform3x3(pPed->GetTransform().GetPosition()- pVeh->GetVehiclePosition()));

								bool bInNmYArea = pPedOffset.x > -vNmXExtents.x &&
												  pPedOffset.x < vNmXExtents.y &&
												  pPedOffset.y < vNmXExtents.z &&
												  pPedOffset.y > -vNmXExtents.w;

								bool useNm = bInNmYArea && rage::Abs(VelFront) < sm_Tunables.m_MaxSpeedForBrace
										 && DotSide <= pAvoidExtents->x;

								// Don't use the natural motion behaviour if already diving out the way,
								// it would only fail anyway because we'll be horizontal
								if(useNm)
								{
									if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP))
									{
										CTaskComplexEvasiveStep* pEvasiveTask =(CTaskComplexEvasiveStep*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP);
										if(pEvasiveTask && pEvasiveTask->GetIsDiving())
										{
											useNm = false;
										}
									}
								}

								if(useNm)
								{
#if __DEV
									if(RenderAvoidanceWarnings && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
										grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_red);
#endif

									if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM && 
										CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_IMPACT_CAR_WARNING, pVeh))
									{
										Vector3 vPedVelocity(pPed->GetVelocity());
										if (MagSquared(pPed->GetGroundVelocityIntegrated()).Getf() > vPedVelocity.Mag2())
										{
											vPedVelocity = VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
										}
										CTask* pTaskNM = rage_new CTaskNMBrace(1000, 30000, pVeh, CTaskNMBrace::BRACE_DEFAULT, vPedVelocity);

										/// set weak option based on agility and health
										static float fHealthThresholdMissionCharPlayer = 110.0f, fHealthThresholdOther = 140.0f;
										const float fHealthThreshold =(pPed->IsPlayer() ||(pPed->PopTypeIsMission()) ? fHealthThresholdMissionCharPlayer : fHealthThresholdOther);

										if(CTaskNMBehaviour::ms_bUseParameterSets &&(!pPed->CheckAgilityFlags(AF_RAGDOLL_BRACE_STRONG) ||(pPed->GetHealth() < fHealthThreshold))) {
											((CTaskNMBrace*)pTaskNM)->SetType(CTaskNMBrace::BRACE_WEAK);
										}

										CEventSwitch2NM eventRagdoll(30000, pTaskNM);
										pPed->SwitchToRagdoll(eventRagdoll);
										
										pPed->GetPedIntelligence()->Dodged(*pVeh);

										// Trigger audio to accompany this behaviour
										if(pPed->IsGangPed() && pDriver && pDriver->IsLocalPlayer())
										{
											if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.25f)
											{
												if(!pPed->NewSay("GANG_DODGE_WARNING"))
													pPed->NewSay("DODGE");
											}
										}
										else
										{
											pPed->RandomlySay("DODGE", 0.125f);
										}
									}
								}
								else if(((Abs(VelFront) > sm_Tunables.m_MinSpeedForDive) &&
									((DotSide <= pAvoidExtents->x) && bInDiveYArea)) || (bIsAffectedByDirectedArea))
								{
#if __DEV
									if(RenderAvoidanceWarnings && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
										grcDebugDraw::Line(pVeh->GetVehiclePosition(), pPed->GetTransform().GetPosition(), Color_blue);
#endif

#if DEBUG_DRAW
									if(!CPedDebugVisualiserMenu::ShouldDisableEvasiveDives())
#endif
									{
										//Check if we have not recently evaded the vehicle.
										static const u32 s_uMinTimeSinceLastEvaded = 3000;
										if((pPed->GetPedIntelligence()->GetLastEntityEvaded() != pVeh) ||
											((pPed->GetPedIntelligence()->GetLastTimeEvaded() + s_uMinTimeSinceLastEvaded) < fwTimer::GetTimeInMilliseconds()))
										{
											CEventPotentialGetRunOver new_event(pVeh);
											pPed->GetPedIntelligence()->AddEvent(new_event);
										}
									}
								}
							}
							else
							{
#if __DEV
								if(RenderAvoidanceWarnings && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
									grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_green, Color_green);
#endif
							}
						}
					}
				}
			}
		}
	}

}

void CTaskVehicleGoToPointWithAvoidanceAutomobile::SlowCarDownForDoorsSectorList(CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, const spdAABB& boundBox, const bool bStopForBreakableDoors)
{
	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetObjectScanner().GetIterator();
	if(!entityList.GetCount())
	{
		return;
	}

	static dev_float s_fPadding = 0.25f;
	const float fOurMinHeight = boundBox.GetMin().GetZf() - s_fPadding;
	const float fOurMaxHeight = boundBox.GetMax().GetZf() + s_fPadding;

	Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
	const Vec3V vecVehPosition = pVeh->GetVehiclePosition();
	//const Vector3 vOurVelocity = pVeh->GetAiVelocity();

	// If we are turning we turn the front vector a bit as well. This way we won't stop for cars directly ahead
	// of us when we are turning but we will stop for cars that we are turning into.
	const float	fTurnAngle = 0.5f * pVeh->GetSteerAngle();
	const float	cosTurnAngle = rage::Cosf(fTurnAngle);
	const float	sinTurnAngle = rage::Sinf(fTurnAngle);
	const float	ourNewDriveDirX = vehDriveDir.x * cosTurnAngle - vehDriveDir.y * sinTurnAngle;
	vehDriveDir.y = vehDriveDir.y * cosTurnAngle + vehDriveDir.x * sinTurnAngle;
	vehDriveDir.x = ourNewDriveDirX;

	const float OurCarDX = vehDriveDir.x * OriginalMaxSpeed;		// 1 sec
	const float OurCarDY = vehDriveDir.y * OriginalMaxSpeed;

	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		if (!pEntity)
		{
			continue;
		}

		const CObject* pObject = static_cast<const CObject*>(pEntity);
		if (!pObject->IsADoor())
		{
			continue;
		}

		const CDoor* pDoor = static_cast<const CDoor*>(pObject);
		const bool bObjectIsAutoOpenDoor = CDoor::DoorTypeAutoOpensForVehicles(pDoor->GetDoorType());

		if (!bObjectIsAutoOpenDoor)
		{
			continue;
		}

		const CDoorTuning* pDoorTuning = pDoor->GetTuning();
		const bool bDoorIsBreakable = (pDoorTuning && pDoorTuning->m_BreakableByVehicle) 
			|| pDoor->GetDoorType() == CDoor::DOOR_TYPE_STD
			|| pDoor->GetDoorType() == CDoor::DOOR_TYPE_STD_SC;

		if (!bStopForBreakableDoors && bDoorIsBreakable)
		{
			continue;
		}

		const Vec3V vOtherPosWorldSpace = pObject->GetTransform().GetPosition();
		const Vector3 vecObjectPosition = VEC3V_TO_VECTOR3(vOtherPosWorldSpace);

		//reject doors behind us
		const Vec3V vDeltaPos = vOtherPosWorldSpace - vecVehPosition;
		if (Dot(vDeltaPos.GetXY(), RCC_VEC3V(vehDriveDir).GetXY()).Getf() < boundBox.GetMin().GetYf())
		{
			continue;
		}

		// reject barrier arm doors that close to the opposite side of us
		if ((pDoor->GetDoorType() == CDoor::DOOR_TYPE_BARRIER_ARM || pDoor->GetDoorType() == CDoor::DOOR_TYPE_BARRIER_ARM_SC) && 
			IsGreaterThanAll(Dot(vDeltaPos, pDoor->GetTransform().GetA()), ScalarV(V_ZERO)))
		{
			continue;
		}

		const Vec3V vOtherFwd = pObject->GetTransform().GetForward();

		//if we're approaching at a sharp angle, swerve instead of stop
		if (!HelperShouldVehicleStopVsSwerveForDoor(RCC_VEC3V(vehDriveDir), boundBox.GetMax().GetX() - boundBox.GetMin().GetX(), pDoor))
		{
			continue;
		}

		if (HelperShouldIgnoreDoorEntirely(pDoor))
		{
			continue;
		}

		if(	vecObjectPosition.x > MinX && vecObjectPosition.x < MaxX &&
			vecObjectPosition.y > MinY && vecObjectPosition.y < MaxY )
		{
			//do we need to care about door height? possibly.
			spdAABB aabb;
			pEntity->GetAABB(aabb);

			//const Vec3V vOtherPosCarSpace = pVeh->GetTransform().UnTransform(vOtherPosWorldSpace);
			const float fOtherMinHeightCarSpace = pVeh->GetTransform().UnTransform(aabb.GetMin()).GetZf();
			const float fOtherMaxHeightCarSpace = pVeh->GetTransform().UnTransform(aabb.GetMax()).GetZf();
			const bool bOtherMinIsWithinZRange = fOtherMinHeightCarSpace > fOurMinHeight && fOtherMinHeightCarSpace < fOurMaxHeight;
			const bool bOtherMaxIsWithinZRange = fOtherMaxHeightCarSpace > fOurMinHeight && fOtherMaxHeightCarSpace < fOurMaxHeight;

			if (bOtherMinIsWithinZRange || bOtherMaxIsWithinZRange)
			{
				const float DeltaSpeedX = -OurCarDX;
				const float DeltaSpeedY = -OurCarDY;

				spdAABB otherBoxModified;
				const spdAABB& otherBox = pObject->GetLocalSpaceBoundBox(otherBoxModified);
				otherBoxModified = otherBox;

				//do some trajectory tests
 				float CollisionT = Test2MovingRectCollision_OnlyFrontBumper(pVeh, pObject,
 					DeltaSpeedX, DeltaSpeedY,
 					vehDriveDir, VEC3V_TO_VECTOR3(vOtherFwd), boundBox, otherBoxModified, true);
				//float CollisionT = Test2MovingRectCollision_OnlyFrontBumper(pObject, pVeh,
				//	-DeltaSpeedX, -DeltaSpeedY, VEC3V_TO_VECTOR3(vOtherFwd), vehDriveDir, otherBoxModified, boundBox);

 				//const float CollisionT2 = Test2MovingRectCollision(	pObject, pVeh, 
 				//	-DeltaSpeedX, -DeltaSpeedY,
 				//	VEC3V_TO_VECTOR3(vOtherFwd), vehDriveDir, otherBoxModified, boundBox);
 				const float CollisionT2 = Test2MovingRectCollision(pVeh, pObject,
 					DeltaSpeedX, DeltaSpeedY,
 					vehDriveDir, VEC3V_TO_VECTOR3(vOtherFwd), boundBox, otherBoxModified, true);

				CollisionT = rage::Min(CollisionT, CollisionT2);

				const float fDistToCollision = OriginalMaxSpeed * CollisionT;
				const float fDistToStop = pVeh->GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(OriginalMaxSpeed);
				const float fDriveSlowBeyondDistance = rage::Min((boundBox.GetMax().GetY() - boundBox.GetMin().GetY()).Getf(), 5.0f);

				if (fDistToCollision < fDistToStop && fDistToCollision < fDriveSlowBeyondDistance)
				{
					*pMaxSpeed = 0.0f;
					pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();
					pVeh->m_nVehicleFlags.bWasStoppedForDoor = true;

#if __BANK
					if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), 2.0f, Color_red, false);
					}
#endif //__BANK

					return;
				}
				else if (fDistToCollision < fDistToStop)
				{
					*pMaxSpeed = rage::Min(*pMaxSpeed, 1.0f);
					//pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();

#if __BANK
					if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), 2.0f, Color_yellow, false);
					}
#endif //__BANK
				}
			}
		}
	}
}

bool HelperShouldPreventFullStop(const CVehicle* /*pVeh*/, const CVehicle* pOtherVehicle, const CarAvoidanceLevel carAvoidanceLevel
								 , const bool bOnJunction, const bool bGoingStraightAtJunction, const bool bOnSingleTrackRoad, const bool bDrivingAgainstTraffic
								 , const bool bTrainApproaching, Vec3V_In vehDriveDir)
{
	//static const ScalarV scZero(V_ZERO);
	static const ScalarV scThreshold(-0.5f);

	//we only want to do this against vehicles that are going against us,
	//but the dot product is anded in at the end of the below if-statement
	const bool bPreferYieldingToLeftTurner = bGoingStraightAtJunction && pOtherVehicle->m_nVehicleFlags.bTurningLeftAtJunction
		&& pOtherVehicle->GetAiXYSpeed() >= 1.0f;

	if ((pOtherVehicle->m_nVehicleFlags.bIsWaitingToTurnLeft 
		|| (bOnJunction && pOtherVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO && !bPreferYieldingToLeftTurner)
		|| bOnSingleTrackRoad
		|| bDrivingAgainstTraffic
		|| (bOnJunction && bTrainApproaching && !pOtherVehicle->InheritsFromTrain()))
		&& carAvoidanceLevel >= CAR_AVOIDANCE_LITTLE
		&& IsLessThanAll(Dot(NormalizeFast(vehDriveDir.GetXY()), NormalizeFast(pOtherVehicle->GetVehicleForwardDirection().GetXY())), scThreshold))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SlowCarDownForCarsSectorList
// PURPOSE :  Processes one sector list.(Slows down for the cars in it)
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::SlowCarDownForCarsSectorList(CVehicle* pVeh, float MinX, float MinY, float MaxX, float MaxY, float &fMaxSpeed, float OriginalMaxSpeed, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, const spdAABB& boundBox, CarAvoidanceLevel carAvoidanceLevel)
{
	TUNE_GROUP_FLOAT(FOLLOW_PATH_AI_THROTTLE, sfIgnoreCarForStoppingHeight, 5.0f, 0.0f, 100.0f, 0.1f);

	const Vector3 ourPosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());

	m_fMostImminentCollisionThisFrame = FLT_MAX;

	m_bSlowingDownForCar = false;

	const CPhysical* pFleeEntity = pVeh->GetIntelligence()->GetFleeEntity();
	if (pFleeEntity && pFleeEntity->GetIsTypePed())
	{
		const CPed* pFleePed = static_cast<const CPed*>(pFleeEntity);
		const CVehicle* pFleeVehicle = pFleePed->GetVehiclePedInside();
		if (pFleeVehicle)
		{
			pFleeEntity = pFleeVehicle;
		}
	}

	//test the car we were slowing for last frame here, and skip it in the loop
	const float fThisBoundRadius = pVeh->GetBoundRadius();

	const Vec3V vehDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse));
	//float fLeftness, fDotProduct;
	//bool bSharpTurn = false;
	//const u32 turnDir = CPathNodeRouteSearchHelper::FindUpcomingJunctionTurnDirection(pVeh->GetIntelligence()->GetJunctionNode(), pVeh->GetIntelligence()->GetNodeList(), &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);
	//const bool bTurningLeft = turnDir == BIT_TURN_LEFT;
	const bool bOnSingleTrackRoad = pVeh->GetIntelligence()->IsOnSingleTrackRoad();
	const bool bOnJunction = pVeh->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO;
	const bool bGoingStraightAtJunction = pVeh->GetIntelligence()->GetJunctionFilter() == JUNCTION_FILTER_MIDDLE || !pVeh->m_nVehicleFlags.bTurningLeftAtJunction;
	
	const bool bDrivingAgainstTraffic =  IsDrivingFlagSet(DF_DriveIntoOncomingTraffic) || (!pVeh->GetIntelligence()->IsOnHighway() && IsDrivingFlagSet(DF_AvoidTargetCoors));

	const bool bTrainApproaching = pVeh->GetIntelligence()->GetJunction() && pVeh->GetIntelligence()->GetJunction()->ShouldCarsStopForTrain();

	if (m_pOptimization_pLastCarWeSlowedFor)
	{
		const bool bPreventFullStop = HelperShouldPreventFullStop(pVeh, m_pOptimization_pLastCarWeSlowedFor, carAvoidanceLevel, bOnJunction, bGoingStraightAtJunction, bOnSingleTrackRoad, bDrivingAgainstTraffic, bTrainApproaching, vehDriveDir);
		SlowCarDownForOtherCar(pVeh, m_pOptimization_pLastCarWeSlowedFor, fMaxSpeed, OriginalMaxSpeed, SteerDirection, bUseAltSteerDirection, bGiveWarning, boundBox, fThisBoundRadius, carAvoidanceLevel, bPreventFullStop, true);
	}

	TUNE_GROUP_BOOL(FOLLOW_AI_SLOW, sbAllowVehicleSlowdownRoundRobin, true);
	TUNE_GROUP_INT(FOLLOW_AI_SLOW, siMinDummyLevelForRoundRobin, VDM_DUMMY, VDM_REAL, VDM_SUPERDUMMY, 1);
	bool bCheckThisFrame = true;
	if (sbAllowVehicleSlowdownRoundRobin
		&& pVeh->GetVehicleAiLod().GetDummyMode() >= siMinDummyLevelForRoundRobin 
		&& pVeh->GetIntelligence()->GetDummyAvoidanceDistributer().IsRegistered() 
		&& !pVeh->GetIntelligence()->GetDummyAvoidanceDistributer().ShouldBeProcessedThisFrame()
		&& fwTimer::GetTimeInMilliseconds() != pVeh->m_TimeOfCreation)	//if this is the first update, definitely slow
	{
		bCheckThisFrame = false;
	}

	if (bCheckThisFrame)
	{
		const Vector3 vehDriveDirVector3 = RCC_VECTOR3(vehDriveDir);

		CVehicleScanner& scanner = pVeh->GetIntelligence()->GetVehicleScanner();
		const int numEntities = scanner.GetNumEntities();
		for(int entityIndex = 0; entityIndex < numEntities; entityIndex++)
		{
			const int nextEntityIndex = entityIndex + 1;
			if(nextEntityIndex < numEntities)
			{
				CEntity* pNextEntity = scanner.GetEntityByIndex(nextEntityIndex);
				if(pNextEntity)
				{
					// Prefetch some stuff that we are likely to need for the next vehicle,
					// incl. things like the transform pointer, IsBaseFlagSet(), etc.
					PrefetchBuffer<256>(pNextEntity);
				}
			}

			CEntity* pEntity = scanner.GetEntityByIndex(entityIndex);

			if(!pEntity)
			{
				continue;
			}

			CVehicle* pOtherVehicle = (CVehicle*) pEntity;

			if(pOtherVehicle == pVeh)
			{
				continue;
			}

			//skip the car we were slowing for last frame, because we should have tested it above, outside of the loop
			if (pOtherVehicle == m_pOptimization_pLastCarWeSlowedFor)
			{
				continue;
			}

			//don't slow down for the thing we're fleeing
			if (pOtherVehicle == pFleeEntity)
			{
				continue;
			}

			//skip vehs about to be removed
			if (pOtherVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				continue;
			}

			if(pVeh->m_nVehicleFlags.bIsRacingConservatively && !pOtherVehicle->m_nVehicleFlags.bIsRacingConservatively)
			{
				continue;
			}

			const Vector3 centre = VEC3V_TO_VECTOR3(pOtherVehicle->GetVehiclePosition()); // GetPosition is faster than pOtherVehicle->GetBoundCentre();
			if(	centre.x <= MinX ||
				centre.x >= MaxX ||
				centre.y <= MinY ||
				centre.y >= MaxY ||
				ABS(centre.z - ourPosition.z) >= sfIgnoreCarForStoppingHeight)	// was 5.0f.
			{
				continue;
			}

			//check the dirty bit
			if (ShouldIgnoreNonDirtyVehicle(*pVeh, *pOtherVehicle, IsDrivingFlagSet(DF_DriveInReverse), bOnSingleTrackRoad))
			{
				continue;
			}

			//B*2794186 prevent slowing down for player ped only for this mission
			if(pVeh->GetIntelligence()->m_bSwerveAroundPlayerVehicleForRiotVanMission && pOtherVehicle->GetDriver() &&pOtherVehicle->GetDriver()->IsPlayer())
			{
				continue;
			}

			//Potentially avoid vehicles following the same target as us
			if (ShouldNotAvoidVehicle(*pVeh, *pOtherVehicle))
			{
				continue;
			}

			//if the other car is waiting to turn left and facing us,
			//and we're able to do any steering, don't stop for them
			//(tends to get junctions jammed up)
			// Get the current drive direction and orientation.
			const bool bPreventFullStop = HelperShouldPreventFullStop(pVeh, pOtherVehicle, carAvoidanceLevel, bOnJunction, bGoingStraightAtJunction, bOnSingleTrackRoad, bDrivingAgainstTraffic, bTrainApproaching, vehDriveDir);

			// Ignore trailers we're towing
			if(pOtherVehicle->InheritsFromTrailer() && pOtherVehicle->m_nVehicleFlags.bHasParentVehicle)
			{
				// check whether the trailer is attached to the parent vehicle
				if (GetTrailerParentFromVehicle(pOtherVehicle) == pVeh)
				{
					continue;
				}
			}
			else if (pOtherVehicle->m_nVehicleFlags.bHasParentVehicle)
			{
				Assert(!pOtherVehicle->InheritsFromTrailer());
				const CVehicle* pOtherVehParent = GetTrailerParentFromVehicle(pOtherVehicle);
				//if we're not a trailer, but have an attach parent and it's a trailer,
				//or are attached to the back of this car
				//just exclude us
				if (pOtherVehParent && 
					(pOtherVehParent->InheritsFromTrailer() || pOtherVehParent == pVeh)
					)
				{
					continue;
				}
			}

			// Do a slightly more sophisticated z check. Work out what the height
			// of our car would be at the point nearest the other car.
			float DistAhead = fwGeom::DistAlongLine2D(ourPosition.x, ourPosition.y, vehDriveDirVector3.x, vehDriveDirVector3.y, centre.x, centre.y);
			float OurHeight = ourPosition.z + DistAhead * vehDriveDirVector3.z;

			if(ABS(centre.z - OurHeight) < 3.0f)
			{		
				// Now actually do a collision threat analysis for this car
				SlowCarDownForOtherCar(pVeh, pOtherVehicle, fMaxSpeed, OriginalMaxSpeed, SteerDirection, bUseAltSteerDirection, bGiveWarning, boundBox, fThisBoundRadius, carAvoidanceLevel, bPreventFullStop, false);
			}
		}
	}

	//if we didn't slow down any this frame, throw away the pointer
	if (fMaxSpeed > OriginalMaxSpeed-SMALL_FLOAT)
	{
		m_pOptimization_pLastCarWeSlowedFor = NULL;
	}
}

// Does a quick test to find upper and lower bounds on the time that two AABBs would collide.
// On return, check the lowerBound and upperBound:
// If lowerBound <= upperBound:
//	The two boxes will collide at some point in time
// More return values:
// If upperBound < 0:
//	The two boxes would have collided in the past, but not now
// If lowerBound == upperBound == + or - Infinity:
//	The two boxes will not collide (and are not moving relative to one another)
// If lowerBound == -Infinity and upperBound = +Infinity
//  The two boxes are colliding (and are not moving relative to one another)
__forceinline void FindAABBCollisionTime(
	ScalarV_InOut beginCollisionTime, ScalarV_InOut endCollisionTime, 
	Vec2V_In boxAMin, Vec2V_In boxAMax, 
	Vec2V_In boxBMin, Vec2V_In boxBMax, 
	Vec2V_In relativePosition, Vec2V_In relativeVelocity) // Relative Position is B - A
{
	// Combine the two AABBs so we can test a single AABB vs. a point
	Vec2V combinedAABBMax = boxBMax - boxAMin;
	Vec2V combinedAABBMin = boxBMin - boxAMax;

	Vec2V worldSpaceBoxMax = combinedAABBMax + relativePosition;
	Vec2V worldSpaceBoxMin = combinedAABBMin + relativePosition;

	// Find the times where the worldspace box edges cross 0
	Vec2V inverseVel = InvertSafe(relativeVelocity, Vec2V(V_INF));
	Vec2V isect1 = -worldSpaceBoxMax * inverseVel; // Solve p + t * v = 0 => t = -p/v
	Vec2V isect2 = -worldSpaceBoxMin * inverseVel;

	Vec2V intervalMins = Min(isect1, isect2); // earliest collision time for each axis
	Vec2V intervalMaxs = Max(isect1, isect2); // latest collision time for each axis
	beginCollisionTime = MaxElement(intervalMins); // lowest time where there's a collision on both axes
	endCollisionTime = MaxElement(intervalMaxs); // highest time where there's a collision on both axes
}

// Given local space corner points for a bounding box and a 2d world space direction vector
// Find the world space AABB that encloses the rotated box
__forceinline void FindAABBForOOBB(
	Vec2V_InOut outAABoxMin, Vec2V_InOut outAABoxMax, 
	Vec2V_In dir, Vec2V_In ooBoxMin, Vec2V_In ooBoxMax)
{
	Vec2V dirPerp(dir.GetY(), -dir.GetX());
	
	// Find world space corner points
	Vec2V a = Scale(dirPerp, ooBoxMin.GetX()) + Scale(dir, ooBoxMin.GetY());
	Vec2V b = Scale(dirPerp, ooBoxMax.GetX()) + Scale(dir, ooBoxMin.GetY());
	Vec2V c = Scale(dirPerp, ooBoxMin.GetX()) + Scale(dir, ooBoxMax.GetY());
	Vec2V d = Scale(dirPerp, ooBoxMax.GetX()) + Scale(dir, ooBoxMax.GetY());

	outAABoxMin = Min(a,b,c,d);
	outAABoxMax = Max(a,b,c,d);

	// TODO: This code seems shorter, but MinElement and MaxElement don't really exist. Maybe there's a workaround though?
	// The permutes here generate the four corner points we want (x1y1, x1y2, x2y1, x2y2)
	//Vec4V boxXCoords = GetFromTwo<Vec::X1, Vec::X2, Vec::X1, Vec::X2>(Vec4V(ooBoxMin.GetIntrin128()), Vec4V(ooBoxMax.GetIntrin128()));
	//Vec4V boxYCoords = GetFromTwo<Vec::Y1, Vec::Y1, Vec::Y2, Vec::Y2>(Vec4V(ooBoxMin.GetIntrin128()), Vec4V(ooBoxMax.GetIntrin128()));

	//Vec4V xformedXCoords = Scale(boxXCoords, dirPerp.GetX()) + Scale(boxYCoords, dir.GetX());
	//Vec4V xformedYCoords = Scale(boxXCoords, dirPerp.GetY()) + Scale(boxYCoords, dir.GetY());

	//outAABoxMin = Vec2V(MinElement(xformedXCoords), MinElement(xformedYCoords));
	//outAABoxMax = Vec2V(MaxElement(xformedXCoords), MaxElement(xformedYCoords));
}

void FindCollisionTimeUpperAndLowerBoundsFast(
	ScalarV_InOut lowerBound, ScalarV_InOut upperBound,
	Vec2V_In relativeVel,
	Vec2V_In myCarPos, Vec2V_In myCarDir, Vec2V_In myCarBboxMin, Vec2V_In myCarBboxMax,
	Vec2V_In otherCarPos, Vec2V_In otherCarDir, Vec2V_In otherCarBboxMin, Vec2V_In otherCarBboxMax
	)
{
	// Basic algorithm:
	// Find the AABB for myCar and otherCar
	// Expand the otherCar AABB by myCar's size.
	// Offset the AABB by the starting location of the otherCar relative to myCar
	// Solve for time intervals when the top and bottom AABB sides cross the X axis, and when the left and right sides cross the Y axis
	// The intersection of the two intervals is the collision time

	// vehicle code does a weird thing where it uses max.x and -max.x instead of max.x and min.x
	Vec2V myCarBboxMinLocal(myCarBboxMin);
	Vec2V otherCarBboxMinLocal(otherCarBboxMin);

	myCarBboxMinLocal.SetX(-myCarBboxMax.GetX());
	otherCarBboxMinLocal.SetX(-otherCarBboxMax.GetX());

	Vec2V myAABBMin, myAABBMax;
	FindAABBForOOBB(myAABBMin, myAABBMax, myCarDir, myCarBboxMinLocal, myCarBboxMax);

	// Test2MovingRectCollision has what appears to be a bug where 'our' bounding box ends up getting used as 
	// the other car's bounding box, because of the way the function is called where 'our' and 'other' are swapped.
	// So here I take the max of the two boxes as the other box.
	Vec2V combinedCarBboxMax = Max(otherCarBboxMax, myCarBboxMax);
	Vec2V combinedCarBboxMin = Min(otherCarBboxMinLocal, myCarBboxMinLocal);

	Vec2V otherAABBMin, otherAABBMax;
	FindAABBForOOBB(otherAABBMin, otherAABBMax, otherCarDir, combinedCarBboxMin, combinedCarBboxMax);

	FindAABBCollisionTime(lowerBound, upperBound,
		myAABBMin, myAABBMax,
		otherAABBMin, otherAABBMax,
		otherCarPos - myCarPos, relativeVel);
}

//if this is called, we have already detected an imminent collision using 2d box tests
void HelperMaybeDoLongHorn(const CVehicle* pVeh, const CVehicle* pOtherVeh, const bool bOtherDriverIsPlayer, bool& bShouldPlayCarHorn, bool& bShouldUseLongHorn)
{
	static dev_float s_fMinSpeedForLongHorn = 9.0f;

	if (pVeh->GetAiXYSpeed() >= s_fMinSpeedForLongHorn && pOtherVeh->GetAiXYSpeed() >= s_fMinSpeedForLongHorn)
	{
		bShouldPlayCarHorn = true;
		bShouldUseLongHorn = true;

		//if he's doing something dumb, and going against us relatively quickly, use a long horn
		if (!bShouldUseLongHorn && pOtherVeh->m_nVehicleFlags.bShouldBeBeepedAt
			&& IsLessThanAll(Dot(pVeh->GetVehicleForwardDirection().GetXY(), pOtherVeh->GetVehicleForwardDirection().GetXY()), ScalarV(V_ZERO)))
		{
			bShouldUseLongHorn = true;
		}
	}
	else if (pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG)
		&& bOtherDriverIsPlayer)
	{
		//only test for collisions if we're a big vehicle and testing against the player
		const CCollisionHistory* pCollisionHistory = pVeh->GetFrameCollisionHistory();
		if(pCollisionHistory->GetNumCollidedEntities() > 0)
		{
			//Vehicles
			if(pCollisionHistory->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_VEHICLE))
			{
				bShouldPlayCarHorn = true;
				bShouldUseLongHorn = true;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SlowCarDownForOtherCar
// PURPOSE :  How much should this car slow down taking into account that other car
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointWithAvoidanceAutomobile::SlowCarDownForOtherCar(CVehicle* pVeh, CVehicle* pOtherVeh, float &fMaxSpeed, float OriginalMaxSpeed, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, const spdAABB& boundBox, const float fBoundRadius, CarAvoidanceLevel carAvoidanceLevel, const bool bPreventFullStop, const bool bSkipSphereCheck)
{
	PF_FUNC(AI_AvoidanceSlowCarForOtherCar);
	float	DotPr;
	float	OurCarDX, OurCarDY, OtherCarDX, OtherCarDY;
	float	CollisionT;
	bool	bUseCloseRelSpeedPercentage = false;

	Assert(pOtherVeh);
	Assert(pVeh);

	float fMaxSpeedForThisCar = fMaxSpeed;

#if __DEV
static CVehicle *pStoredVeh0 = NULL;
static CVehicle *pStoredVeh1 = NULL;
	if((pStoredVeh0 == pOtherVeh && pStoredVeh1 == pVeh) ||
		(pStoredVeh1 == pOtherVeh && pStoredVeh0 == pVeh))
	{
		printf("Cars match\n");
	}
#endif

	// Get the current drive direction and orientation.
	Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
	//float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);

	// If we are turning we turn the front vector a bit as well. This way we won't stop for cars directly ahead
	// of us when we are turning but we will stop for cars that we are turning into.
	float	TurnAngle = 0.5f * pVeh->GetSteerAngle();
	float	cosTurnAngle = rage::Cosf(TurnAngle);
	float	sinTurnAngle = rage::Sinf(TurnAngle);
	float	ourNewDriveDirX = vehDriveDir.x * cosTurnAngle - vehDriveDir.y * sinTurnAngle;
	vehDriveDir.y = vehDriveDir.y * cosTurnAngle + vehDriveDir.x * sinTurnAngle;
	vehDriveDir.x = ourNewDriveDirX;

	// Reject cars that are behind us(too late to slow down for them)
	const Vec3V vecVehPosition = pVeh->GetVehiclePosition();
	const Vec3V vecOtherVehPosition = pOtherVeh->GetVehiclePosition();
	//TUNE_GROUP_FLOAT(CAR_AI, fAvoidBehindDist, 0.0f, -100.0f, 100.0f, 0.1f);
	const Vec3V vDeltaPos = vecOtherVehPosition - vecVehPosition;
	//const Vec3V vDeltaPosNormalized = NormalizeFastSafe(vDeltaPos, Vec3V(V_ZERO));
	if (Dot(vDeltaPos.GetXY(), RCC_VEC3V(vehDriveDir).GetXY()).Getf() < boundBox.GetMin().GetYf())
	{
		return;
	}

	//if we're swerving for this player, don't slow down as well, it prevents us from getting out
	//of the way in time
	CPed* pOtherDriver = pOtherVeh->GetDriver();
	if (bGiveWarning && pOtherDriver && pOtherDriver->IsPlayer() && carAvoidanceLevel > CAR_AVOIDANCE_NONE)
	{
		return;
	}

	Vec3V vOtherDriveDir = pOtherVeh->GetVehicleForwardDirection();
	vOtherDriveDir.SetZ(ScalarV(V_ZERO));
	vOtherDriveDir = NormalizeFast(vOtherDriveDir);

	if (pOtherVeh->GetAiXYSpeed() > 1.0f 
		&& IsLessThanAll(Dot(VECTOR3_TO_VEC3V(pOtherVeh->GetAiVelocity()).GetXY(), vOtherDriveDir.GetXY()), ScalarV(V_ZERO))
		)
	{
		vOtherDriveDir = -vOtherDriveDir;
	}

	bool bShouldPlayCarHorn = false;
	bool bShouldUseLongHorn = false;
	const bool bPersonalityAllowsHonking = pVeh->GetDriver() && pVeh->GetDriver()->CheckBraveryFlags(BF_PLAY_CAR_HORN);
	const bool bOtherDriverIsPlayer =  pOtherDriver && pOtherDriver->IsPlayer();
	const bool bShouldBeBeepedAt = pOtherVeh->m_nVehicleFlags.bShouldBeBeepedAt
		|| (pOtherVeh->m_nVehicleFlags.bPlayerDrivingAgainstTraffic 
		&& IsLessThanAll(Dot(RCC_VEC3V(vehDriveDir).GetXY(), vOtherDriveDir.GetXY()), ScalarV(V_ZERO))
		);
	if(bShouldBeBeepedAt && bOtherDriverIsPlayer)
	{
		//if((vecOtherVehPosition - vecVehPosition).XYMag() < 20.0f)
		const float fDistToHonk = rage::Max(20.0f, fabsf((pVeh->GetAiVelocity() - pOtherVeh->GetAiVelocity()).Mag()));
		if (DistXYFast(vecOtherVehPosition, vecVehPosition).Getf() < fDistToHonk)
		{
			if(bPersonalityAllowsHonking)
			{
				//defer this now, so if we have a close call with a vehicle, we can
				//chose to use the "HeldDown" horn pattern -JM
				if (pVeh->m_iNextValidHornTime < fwTimer::GetTimeInMilliseconds())
				{
					//don't honk for three seconds - enough to prevent this car honking until driven by
					pVeh->m_iNextValidHornTime = fwTimer::GetTimeInMilliseconds() + 1500;

					//B*1927556, lots more cars in NG, lets reduce number of horns
					float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
					if(fRandom < (pOtherDriver->GetPlayerInfo()->GetPlayerDataPlayerOnHighway() ? 0.25f : 0.5f))
					{
						bShouldPlayCarHorn = true;
					}
				}
			}
		}
	}

	// Ignore vehicles escorting us from the front. Otherwise we can get into a deadlock.
	// Only if they're stopped
	if (pOtherVeh->GetAiXYSpeed() < 0.1f)
	{
		const CPhysical* pOtherAvoidanceTarget = pOtherVeh->GetIntelligence()->GetAvoidanceCache().m_pTarget;
		if (pOtherAvoidanceTarget)
		{
			if (pOtherAvoidanceTarget == pVeh || pOtherAvoidanceTarget == pVeh->GetAttachedTrailer())
			{
				return;
			}
		}
	}

	// Ignore the vehicle the task has specified avoiding
	if (pOtherVeh == m_pVehicleToIgnoreWhenStopping)
	{
		return;
	}

	const Vector3 vOtherVel = pOtherVeh->GetAiVelocity();

	// Reject cars that are not going to collide over the next second or so.
	// test the two line segments against each other.
	vehDriveDir.z = 0.0f;
	OtherCarDX = vOtherVel.x;	// 1 sec
	OtherCarDY = vOtherVel.y;

	//if we're supposed to use the alternate steer direction, apply it here
	Vector3 vTestDir = vehDriveDir;
	TUNE_GROUP_BOOL(FOLLOW_AI_SLOW, bEnableNewSlowdownDirectionTest, true)
	if (bUseAltSteerDirection && bEnableNewSlowdownDirectionTest)
	{
		vTestDir = Vector3(rage::Cosf(SteerDirection), rage::Sinf(SteerDirection), 0.0f);
	}

	// For our car we use a line segment as if we were moving at full
	// speed in the direction we're heading.
	OurCarDX = vTestDir.x * OriginalMaxSpeed;		// 1 sec
	OurCarDY = vTestDir.y * OriginalMaxSpeed;

	TUNE_GROUP_BOOL(FOLLOW_AI_SLOW, bEnableCoarseSlowdownRejectDraw, false);

	//if the car isn't behind us (checked above)
	//and has a greater velocity in our desired direction than we do
	//we should never hit each other (right?)
	const Vec3V vOurDesiredVel = Vec3V(OurCarDX, OurCarDY, 0.0f);
	const Vec3V vRelVelocity = vOurDesiredVel - RCC_VEC3V(vOtherVel);

	//TUNE_GROUP_BOOL(FOLLOW_AI_SLOW, bEnableRelVelocityCheck, false);
	if (/*(bEnableRelVelocityCheck && (Dot(vOurDesiredVel, vRelVelocity) < s_vScalarZero).Getb()) || */
		(Dot(vRelVelocity, vDeltaPos) < s_vScalarZero).Getb())
	{
#if __BANK
		if (bEnableCoarseSlowdownRejectDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(vecVehPosition, vecOtherVehPosition, Color_green);
		}
#endif //__BANK

		const CPhysical* pHonkTarget = pOtherDriver ? pOtherDriver : static_cast<const CPhysical*>(pOtherVeh);
		HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
		return;
	}

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Vector3 LineBase = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		LineBase.z += 1.7f;
		grcDebugDraw::Line(LineBase,
							LineBase + Vector3(OurCarDX, OurCarDY, 0.0f),
							Color32(255, 255, 255, 255));
	}
#endif // __DEV

	//if the bounding sphere test result is greater than the result of a car we've already
	//chosen to stop for, ignore it and don't do the expensive test
	if (!bSkipSphereCheck && 
		(Test2MovingSphereCollision(pVeh, pOtherVeh, VEC3V_TO_VECTOR3(vOurDesiredVel), vOtherVel, fBoundRadius)
		>= m_fMostImminentCollisionThisFrame))
	{
#if __BANK
		if (bEnableCoarseSlowdownRejectDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(vecVehPosition, vecOtherVehPosition, Color_orange);
		}
#endif //__BANK

		const CPhysical* pHonkTarget = pOtherDriver ? pOtherDriver : static_cast<const CPhysical*>(pOtherVeh);
		HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
		return;
	}

	//Vector3	otherDriveDir(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection()));
	//otherDriveDir.z = 0.0f;
	//float Length = rage::Sqrtf(otherDriveDir.x * otherDriveDir.x + otherDriveDir.y * otherDriveDir.y);

	const float	DeltaSpeedX = OtherCarDX - OurCarDX;
	const float	DeltaSpeedY = OtherCarDY - OurCarDY;

#if __BANK
	if (bEnableCoarseSlowdownRejectDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		grcDebugDraw::Line(vecVehPosition, vecOtherVehPosition, Color_red);
	}
#endif //__BANK
	
	const float fRelativeSpeed = Min(vOtherVel.Dot(vTestDir), OriginalMaxSpeed);

	TUNE_GROUP_BOOL(CAR_AI, useNewVehicleSlowdownTest, true);

	ScalarV collisionTimeLowerBound, collisionTimeUpperBound;
	spdAABB otherBoxModified;
	const spdAABB& otherBox = pOtherVeh->GetLocalSpaceBoundBox(otherBoxModified);
	otherBoxModified = otherBox;

	//if the other vehicle is a bike, add some padding so we don't sidle up next to bikes that aren't in the exact center of a lane
	if (pOtherVeh->InheritsFromBike())
	{
		const Vec3V offset(0.5f, 1.0f, 0.5f);
		otherBoxModified.SetMax(otherBoxModified.GetMax() + offset);
		otherBoxModified.SetMin(otherBoxModified.GetMin() - offset);
	}
	else if(pOtherVeh->GetIsHeli())
	{
		HelperGetHeliAvoidanceBounds(pOtherVeh, otherBoxModified);
	}

	FindCollisionTimeUpperAndLowerBoundsFast(
		collisionTimeLowerBound, collisionTimeUpperBound, 
		-vRelVelocity.GetXY(),
		vecVehPosition.GetXY(), RCC_VEC3V(vTestDir).GetXY(), boundBox.GetMin().GetXY(), boundBox.GetMax().GetXY(),
		vecOtherVehPosition.GetXY(), vOtherDriveDir.GetXY(), otherBoxModified.GetMin().GetXY(), otherBoxModified.GetMax().GetXY());
	float startInterval = Max(0.0f, collisionTimeLowerBound.Getf());
	float endInterval = Min(5.0f, collisionTimeUpperBound.Getf()); // TODO: Fix the hardcoded value here, it comes from Test2MovingRectCollision

	bool bailEarly = (startInterval >= endInterval);

	if (bailEarly)
	{
		const CPhysical* pHonkTarget = pOtherDriver ? pOtherDriver : static_cast<const CPhysical*>(pOtherVeh);;
		HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
		return;
	}

	CollisionT = Test2MovingRectCollision_OnlyFrontBumper(pVeh, pOtherVeh,
		DeltaSpeedX, DeltaSpeedY,
		vTestDir, VEC3V_TO_VECTOR3(vOtherDriveDir), boundBox, otherBoxModified, false);

	// Unfortunately we have to do this the other way around as well.
	//(Otherwise collisions could go undetected if the points of carA intersect carB but
	// the points of carB don't intersect carA.
	//TUNE_GROUP_BOOL(CAR_AI, enable2ndVehicleSlowdownTest, true);

	//if (enable2ndVehicleSlowdownTest)
	{
		const float CollisionT2 = Test2MovingRectCollision(	pOtherVeh, pVeh, 
			-DeltaSpeedX, -DeltaSpeedY,
			VEC3V_TO_VECTOR3(vOtherDriveDir), vTestDir, otherBoxModified, boundBox, false);

		//
// 		if (CollisionT2 < CollisionT /*&& (CollisionT > 3.0f || CollisionT2 < 0.1f && CollisionT > 0.2f)*/)
// 		{
// 			Vec3V vecAboveVehPosition = vecVehPosition;
// 			vecAboveVehPosition.SetZf(vecVehPosition.GetZf() + 10.0f);
// 			grcDebugDraw::Line(vecVehPosition, vecAboveVehPosition, Color_purple);
// 			aiDisplayf("Car %p Using 2nd test: %.2f : %.2f", pVeh, CollisionT, CollisionT2);
// 		}
		////////////////
		CollisionT = rage::Min(CollisionT, CollisionT2);
	}
	

	// Draw a line indicating the distance we get before colliding
/*
	if(CollisionT >= 0.0f && CollisionT < 1.0f)
	{
		grcDebugDraw::Line(pVeh->GetPosition().x + CollisionT*OurCarDX, pVeh->GetPosition().y + CollisionT*OurCarDY, 20.0f,
							pVeh->GetPosition().x + CollisionT*OurCarDX, pVeh->GetPosition().y + CollisionT*OurCarDY, 10.0f,
							0xffff00ff, 0xffff00ff);
		grcDebugDraw::Line(pEntity->GetPosition().x + CollisionT*OtherCarDX, pEntity->GetPosition().y + CollisionT*OtherCarDY, 20.0f,
							pEntity->GetPosition().x + CollisionT*OtherCarDX, pEntity->GetPosition().y + CollisionT*OtherCarDY, 10.0f,
							0xffff00ff, 0xffff00ff);
	}
*/
/*
	// Just draw a line between the two cars for now.
	grcDebugDraw::Line(pEntity->GetPosition().x, pEntity->GetPosition().y, 16.0f,
						pVeh->GetPosition().x, pVeh->GetPosition().y, 16.0f,
						0xffff0000, 0xffffffff);

	// If the car is closeby we slow down more
	Distance = rage::Sqrtf((pEntity->GetPosition().x - pVeh->GetPosition().x)*(pEntity->GetPosition().x - pVeh->GetPosition().x) +
						(pEntity->GetPosition().y - pVeh->GetPosition().y)*(pEntity->GetPosition().y - pVeh->GetPosition().y));

	Distance = MAX(0.0f, Distance - 4.5f);		// within a certain distance we always stop

	fMaxSpeedForThisCar = rage::Min((fMaxSpeedForThisCar), Distance);
*/

	const float fDistToCollision = OriginalMaxSpeed * CollisionT;
	const float fDistToStop = pVeh->GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(OriginalMaxSpeed);

// 	if (pOtherVeh->GetDriver() && pOtherVeh->GetDriver()->IsPlayer())
// 	{
// 		Displayf("[JM] Address:  %p, CollisionT:  %f, DistToCollision: %f", pVeh, CollisionT, fDistToCollision);
// 	}

	const CPhysical* pHonkTarget = pOtherDriver ? pOtherDriver : static_cast<const CPhysical*>(pOtherVeh);
	TUNE_GROUP_FLOAT(FOLLOW_AI_SLOW, fPercentOfMaxSpeedWhenOvertakingStationary, 0.9f, 0.0f, 1.0f, 0.01f);

	//static dev_float s_fMaxTimeToCareAboutStopping = 3.0f;
	if((useNewVehicleSlowdownTest && fDistToCollision < fDistToStop)
		|| (!useNewVehicleSlowdownTest && CollisionT >= 0.0f && CollisionT < 1.5f))
	{
		const bool bIsOtherCarStationary = IsThisAStationaryCar(pOtherVeh);
			// Don't slow down for parked cars that we can pass.
		if(bIsOtherCarStationary || pOtherVeh->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle
			|| pOtherVeh == m_pVehicleToIgnoreWhenStopping)
		{
			//only ambients can be blocked close
			const bool bDriverIsPlayer = pVeh->GetDriver() && pVeh->GetDriver()->IsPlayer();
			bool bOtherCarWillNeverMove = !pOtherVeh->GetDriver() || pOtherVeh->GetDriver()->IsDead();
			const bool bBlockedClose = m_ObstructionInformation.m_uFlags.IsFlagSet(ObstructionInformation::IsCompletelyObstructedClose)
				&& !pVeh->PopTypeIsMission() 
				&& (!pOtherVeh->GetDriver() || !pOtherVeh->GetDriver()->IsPlayer())
				&& !pVeh->IsLawEnforcementVehicle()
				&& !pVeh->m_nVehicleFlags.bIsFireTruckOnDuty
				&& !pVeh->m_nVehicleFlags.bIsAmbulanceOnDuty
				&& !m_ObstructionInformation.m_uFlags.IsFlagSet(ObstructionInformation::IsCompletelyObstructedBySingleObstruction)
				&& pVeh->GetAiXYSpeed() < 1.0f
				&& !bDriverIsPlayer
				&& !bOtherCarWillNeverMove;
			if(carAvoidanceLevel > CAR_AVOIDANCE_NONE && IsThereRoomToOvertakeVehicle(pVeh, pOtherVeh, true)
				&& !bBlockedClose)
			{
				//when we're overtaking a stationary car, make everyone else wait for us
				//or else Bad Things happen
				pVeh->m_nVehicleFlags.bIsOvertakingStationaryCar = true;

				if(pVeh->GetAiVelocity().XYMag() < 1.0f)
				{
					pOtherVeh->WeAreBlockingSomeone();		// If the car is abandoned this may cause the car to be removed by ambient peds.(thiefs or police)
				}

				fMaxSpeedForThisCar = rage::Min(fMaxSpeedForThisCar, OriginalMaxSpeed * fPercentOfMaxSpeedWhenOvertakingStationary);
				fMaxSpeed = rage::Min(fMaxSpeed, fMaxSpeedForThisCar);

				HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
				return;
			}
			else
			{
				if(pVeh->GetAiVelocity().XYMag() < 1.0f)
				{
					pOtherVeh->WeAreBlockingSomeone();		// If the car is abandoned this may cause the car to be removed by ambient peds.(thiefs or police)
				}

				fMaxSpeedForThisCar = 0.0f;
				fMaxSpeed = fMaxSpeedForThisCar;	// Wait until we are clear to overtake
				pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();		// Make sure we don't go and start a 3point turn.
				HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
				return;
			}
		}

		m_bSlowingDownForCar = true;
		if (CollisionT < m_fMostImminentCollisionThisFrame)
		{
			m_pOptimization_pLastCarWeSlowedFor = pOtherVeh;
			m_fMostImminentCollisionThisFrame = CollisionT;
		}

		//m_bShouldChangeLanesForTraffic = CanSwitchLanesToAvoidObstruction(pVeh, pOtherVeh, OriginalMaxSpeed);

		//keep track of the obstructing vehicle
		pVeh->GetIntelligence()->m_pObstructingEntity = pOtherVeh;

		// If we are coming down a slope we should slow down earlier so we don't bump into car in front of us.
		float slopeMultiplier = 1.0f + 4.0f * 0.0f;//rage::Max(0.0f, FindMeasureForSlope(pVeh));//TODO add this in
		Assertf((slopeMultiplier < 9.0f),"Slope greater than 2:1; path nodes are probably set up wrong, please check pos %f, %f, %f.", pVeh->GetVehiclePosition().GetXf(), pVeh->GetVehiclePosition().GetYf(), pVeh->GetVehiclePosition().GetZf());
		CollisionT /= slopeMultiplier;

		//use the bounding box of the car we're stopping for, not our own.
		//this way cars naturally leave space for buses, rather than buses leaving space for small cars
		const float fOtherVehBoundingBoxMaxY = pOtherVeh->GetBoundingBoxMax().y;
		const u32 nMsHowLongAgoWeWereHonkedAt = fwTimer::GetTimeInMilliseconds() - pVeh->GetIntelligence()->LastTimeHonkedAt;
		const bool bHasBeenHonkedAtRecently = pVeh->GetIntelligence()->LastTimeHonkedAt > 0 
			&& nMsHowLongAgoWeWereHonkedAt < 5000 && nMsHowLongAgoWeWereHonkedAt > CDriverPersonality::FindDelayBeforeRespondingToVehicleHonk(pOtherVeh->GetDriver(), pOtherVeh);

		const bool bAllowPullForwardAfterBeingHonkedAt = CDriverPersonality::WillRespondToOtherVehiclesHonking(pOtherDriver, pOtherVeh);

// 		if (bHasBeenHonkedAtRecently && bAllowPullForwardAfterBeingHonkedAt)
// 		{
// 			grcDebugDraw::Sphere(pVeh->GetVehiclePosition(), 1.5f, Color_purple, false, 10);
// 		}

		const float fDriveStopDistNew = !bHasBeenHonkedAtRecently || !bAllowPullForwardAfterBeingHonkedAt ? fOtherVehBoundingBoxMaxY : fOtherVehBoundingBoxMaxY * 0.5f;
		float fDriveStopDistMultiplier = CDriverPersonality::GetStopDistanceMultiplierForOtherCars(pVeh->GetDriver(), pVeh);

		//if the other car is stationary, and we're here, it means we want to overtake it but we think the route ahead is blocked
		//by oncoming traffic. if so, pad out the distance we stop for it here--so when we decide to go, we'll have more room -JM
		if (bIsOtherCarStationary)
		{
			fDriveStopDistMultiplier *= 2.0f;
		}
		if (((!bPreventFullStop && (useNewVehicleSlowdownTest && fDistToCollision < fDriveStopDistNew * fDriveStopDistMultiplier))
			|| (!useNewVehicleSlowdownTest && CollisionT < CDriverPersonality::GetStopDistanceForOtherCarsOld(pVeh->GetDriver(), pVeh) / OriginalMaxSpeed)))
		{
			fMaxSpeedForThisCar = 0.0f;
			bUseCloseRelSpeedPercentage = true;

			//I don't think we need this anymore--FindMaximumSpeed will look at the MaxSpeed
			//and decide to call this if it's zero. We may not want to call this here as 
			//we could decide below to start avoiding this guy, and we might want to start a
			//3 point turn if we're stuck, which this would prevent
			//pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();		// Make sure we don't go and start a 3point turn.

			//this happens at the end of the function now, in case we want to 
			//bail out of setting our speed to 0.0 because the other car's
			//relative velocity is high enough that we dont have to stop for it
			//m_pOptimization_pLastCarBlockingUs = pOtherVeh;

			if (bPersonalityAllowsHonking)
			{
				const bool bOtherDriverIsPlayer =  pOtherDriver && pOtherDriver->IsPlayer();
				if(bOtherDriverIsPlayer)
				{
					if (pVeh->m_iNextValidHornTime < fwTimer::GetTimeInMilliseconds())
					{
						//don't honk for three seconds - enough to prevent this car honking until driven by
						pVeh->m_iNextValidHornTime = fwTimer::GetTimeInMilliseconds() + 1500;

						//B*1927556, lots more cars in NG, lets reduce number of horns on highways
						float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
						if(fRandom < (pOtherDriver->GetPlayerInfo()->GetPlayerDataPlayerOnHighway() ? 0.25f : 0.5f))
						{
							HelperMaybeDoLongHorn(pVeh, pOtherVeh, bOtherDriverIsPlayer, bShouldPlayCarHorn, bShouldUseLongHorn);	
						}
					}
				}
				else
				{
					HelperMaybeDoLongHorn(pVeh, pOtherVeh, bOtherDriverIsPlayer, bShouldPlayCarHorn, bShouldUseLongHorn);	
				}
			}

#if __BANK
			if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), 2.0f, Color_red, false);
			}
			if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				char debugText[100];
				sprintf(debugText, "Blocked by car:%.1f NotStuck:%d(%d)", CollisionT, pVeh->GetIntelligence()->LastTimeNotStuck, fwTimer::GetTimeInMilliseconds()-pVeh->GetIntelligence()->LastTimeNotStuck);
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()) + Vector3(0.f,0.f,2.5f), CRGBA(255, 255, 255, 255), debugText);
			}
#endif
		}
		// If we are within 3m we drive very slowly.
		else if(!useNewVehicleSlowdownTest && fDistToCollision < CDriverPersonality::GetDriveSlowDistanceForOtherCars(pVeh->GetDriver(), pVeh))
		{
			fMaxSpeedForThisCar = rage::Min((fMaxSpeedForThisCar), 1.0f);
			pVeh->GetIntelligence()->UpdateCarHasReasonToBeStopped();		// Make sure we don't go and start a 3point turn.

			if (bPersonalityAllowsHonking)
			{
				HelperMaybeDoLongHorn(pVeh, pOtherVeh, bOtherDriverIsPlayer, bShouldPlayCarHorn, bShouldUseLongHorn);		
			}

#if __BANK
			if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), 2.0f, Color_orange, false);
			}
#endif //__BANK

		}
		else
		{			
			// Slow down according to distance/initial speed.
			if (!useNewVehicleSlowdownTest)
			{
				CollisionT -= CDriverPersonality::GetDriveSlowDistanceForOtherCars(pVeh->GetDriver(), pVeh) / OriginalMaxSpeed;
				//			CollisionT = MAX(0.0f, (CollisionT - 0.3f)*(1.0f / 1.2f));
				fMaxSpeedForThisCar = rage::Min((fMaxSpeedForThisCar), CollisionT * OriginalMaxSpeed);
			}
			else
			{
				float fRatio = (fDistToCollision /*- fTweakDistance*/) / fDistToStop;
				fRatio = Clamp(fRatio, 0.0f, 1.0f);
				fMaxSpeedForThisCar = Clamp(fRatio * OriginalMaxSpeed, 0.0f, fMaxSpeedForThisCar );
			}

			if(OriginalMaxSpeed >= 1.0f)
			{
				fMaxSpeedForThisCar = rage::Max(fMaxSpeedForThisCar, 1.0f);
			}

#if __BANK
			if (CVehicleIntelligence::m_bDisplayDebugSlowForTraffic && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), 2.0f, Color_yellow2, false);
			}
#endif //__DEV

			// Possible flash the headlights, etc.
			if(/*!m_bShouldChangeLanesForTraffic && */ShouldGiveWarning(pVeh, pOtherVeh))
			{
				pVeh->GiveWarning();
			}
		}
	}

	// There is an exception: If the two cars are on collision course and
	// in fact in a deadlock situation, one of them is given the green
	// light. This might cause a collision but at least it keeps things
	// moving.
	// We also test whether the cars have been stationary for a while.
	static dev_float s_fMaxTValueForDeadlockResolution = 1.0f;
	if(CollisionT >= 0.0f && CollisionT < s_fMaxTValueForDeadlockResolution)
	{
		Assert(pOtherVeh);
		if(CollisionT < m_fSmallestCollisionTSoFar)		// This makes sure we store the closest car blocking us(otherwise some emergency measures later on don't work)
		{
			m_fSmallestCollisionTSoFar = CollisionT;
			pVeh->GetIntelligence()->m_pCarThatIsBlockingUs = pOtherVeh;
		}

		//allow us to fall through into here if we've already got the DF_AvoidingCarsUntilHere flag set.
		//the whole point of this code is to allow us to move, and once we move, the timer is reset,
		//preventing us from getting in here
		if(m_bAvoidingCarsUntilClear || ((fwTimer::GetTimeInMilliseconds() - pVeh->GetIntelligence()->LastTimeNotStuck > 4000) &&
			(fwTimer::GetTimeInMilliseconds() - pOtherVeh->GetIntelligence()->LastTimeNotStuck) > 4000))
		{
			CTaskVehicleMissionBase *pTask = pOtherVeh->GetIntelligence()->GetActiveTask();
			if(/*!IsDrivingFlagSet(DF_AvoidingCarsUntilClear) &&*/ IsDrivingFlagSet(DF_StopForCars) && pTask && pTask->IsDrivingFlagSet(DF_StopForCars) )
			{
				Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
				Vector3 OtherVehForwardVector(VEC3V_TO_VECTOR3(pOtherVeh->GetVehicleForwardDirection()));

				DotPr = VehForwardVector.x * OtherVehForwardVector.x +
							 VehForwardVector.y * OtherVehForwardVector.y;

				if(!pOtherDriver || !pOtherDriver->IsPlayer())
				{
					if(DotPr < -0.0f || pOtherVeh->GetIntelligence()->m_pCarThatIsBlockingUs == pVeh)
					{
						if(pOtherVeh->GetIntelligence()->m_pCarThatIsBlockingUs != pVeh 
							|| IsFirstCarBestOneToGo(pVeh, pOtherVeh, m_bAvoidingCarsUntilClear) )		// Decide which one is in the best position to avoid the other.(If the other car isn't waiting on us we should go; he is probably blocked by a 3rd car)
						{
							fMaxSpeedForThisCar = rage::Max(fMaxSpeedForThisCar, OriginalMaxSpeed * 0.2f);
							//SetDrivingFlag(DF_AvoidingCarsUntilClear, true);
							m_bAvoidingCarsUntilClear = true;
							m_startTimeAvoidCarsUntilClear = fwTimer::GetTimeInMilliseconds();
							SetCruiseSpeed(rage::Min(GetCruiseSpeed(), 10.0f));
							m_bStoppingForTraffic = false;		// Do this or the car will wait a little before starting to go and by that time will have reverted its driving mode.
						}
					}
				}
			}
		}
	}
	if(fMaxSpeedForThisCar < 0.03f && pVeh->GetAiVelocity().XYMag() < 1.0f)
	{
		pOtherVeh->WeAreBlockingSomeone();		// If the car is abandoned this may cause the car to be removed by ambient peds.(thiefs or police)
	}

	if (bPersonalityAllowsHonking && bOtherDriverIsPlayer && !bShouldUseLongHorn)
	{
		//we don't want this to actually cause us to play the horn where we wouldn't before,
		//just possibly use a long horn instead
		bool bFakeShouldPlayHorn = false;
		HelperMaybeDoLongHorn(pVeh, pOtherVeh, bOtherDriverIsPlayer, bFakeShouldPlayHorn, bShouldUseLongHorn);		
	}

	static dev_float REL_SPEED_PERC_BASE = 0.9f;
	static dev_float REL_SPEED_PERC_CLOSE_BASE = 0.5f;

	mthRandom rngSpeedPerc(pVeh->GetRandomSeed());
	const float fRelSpeedPercentageFar = rage::Min(0.95f, rngSpeedPerc.GetGaussian(REL_SPEED_PERC_BASE, 0.05f));
	const float fRelSpeedPercentageClose = rngSpeedPerc.GetGaussian(REL_SPEED_PERC_CLOSE_BASE, 0.05f);

	const float fRelSpeedPercentage = bUseCloseRelSpeedPercentage ? fRelSpeedPercentageClose : fRelSpeedPercentageFar;
	// Never slow down much more than the relative speed
	fMaxSpeedForThisCar = Max( fMaxSpeedForThisCar, fRelativeSpeed*fRelSpeedPercentage );
	fMaxSpeed = Min(fMaxSpeed, fMaxSpeedForThisCar);

	if (fMaxSpeedForThisCar < 0.03f)	//the 0.03 came from the check above
	{
		m_pOptimization_pLastCarBlockingUs = pOtherVeh;
	}

	//This needs to be called before every return from this function.
	HelperMaybePlayHorn(pVeh, bShouldPlayCarHorn, bShouldUseLongHorn, pHonkTarget);
}

//this test returns 
float CTaskVehicleGoToPointWithAvoidanceAutomobile::Test2MovingSphereCollision(const CVehicle* const pVeh, const CVehicle* const pOtherVeh, const Vector3& vOurVel, const Vector3& vTheirVel, const float fBoundRadius)
{
	//TODO: actually, this whole function looks like a really good candidate for vectorization
	//const float fMyRadius = pVeh->GetBoundRadius();
	const float fTheirRadius = pOtherVeh->GetBaseModelInfo()->GetBoundingSphereRadius() + pOtherVeh->GetForceAddToBoundRadius();//pOtherVeh->GetBoundRadius();

// #if __ASSERT
// 	const float fRadiusDiff = fTheirRadius - pOtherVeh->GetBoundRadius();
// 	Assertf(fRadiusDiff > -0.1f, "fRadiusDiff = %f", fRadiusDiff);
// #endif //__ASSERT

	const Vec3V vOurPos = pVeh->GetVehiclePosition();
	const Vec3V vTheirPos = pOtherVeh->GetVehiclePosition();
	const ScalarV svfDistBtwn = DistFast(vOurPos, vTheirPos);
	Vec3V vUsToThemNormalized(V_ZERO);
	if (svfDistBtwn.Getf() >= SMALL_FLOAT)
	{
		vUsToThemNormalized = (vTheirPos - vOurPos) / svfDistBtwn;
	}

	//get the relative velocity in the direction of the position delta
	//for vehicles moving fast in the same direction this should be small
	const Vec3V vRelVelocity = RCC_VEC3V(vOurVel) - RCC_VEC3V(vTheirVel);
	const ScalarV fRelSpeedInDeltaDirection = Dot(vUsToThemNormalized, vRelVelocity);

	const ScalarV fNumerator = svfDistBtwn - ScalarV(fBoundRadius) - ScalarV(fTheirRadius);
	return fRelSpeedInDeltaDirection.Getf() >= SMALL_FLOAT ? (fNumerator /fRelSpeedInDeltaDirection).Getf() : FLT_MAX;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Test2MovingRectCollision_OnlyFrontBumper
// PURPOSE :  Test whether the points of the second rectangle fall within the
//			  first at any point. Returns the time of the first collision(0..1)
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPointWithAvoidanceAutomobile::Test2MovingRectCollision_OnlyFrontBumper(const CPhysical* const pVeh, const CPhysical* const pOtherVeh, 
																							 float OtherSpeedX, float OtherSpeedY,
																							 const Vector3& ourNewDriveDir,
																							 const Vector3& otherDriveDir,
																							 const spdAABB& boundBox,
																							 const spdAABB& otherBox,
																							 const bool bOtherEntIsDoor)
{
	float	ReturnVal;
	s32	OtherPoints;
	Vector3	LineStart;

	// We had to reshuffle the way the arguments are passed. Otherwise the optimiser
	// would fuck stuff up. Basically arguments changed during the function call.
	// A printf before and after the call resulted in different results.
	Vector3 ourPosn = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
	Vector3 hisPosn = VEC3V_TO_VECTOR3(pOtherVeh->GetTransform().GetPosition());
	//Vector3 ourBBMax = pVeh->GetBoundingBoxMax();

	const float	OtherCarX = hisPosn.x;// - pVeh->GetPosition().x;
	const float	OtherCarY = hisPosn.y;// - pVeh->GetPosition().y;
	const float	OurCarX = ourPosn.x;
	const float	OurCarY = ourPosn.y;

	const float	OurBBHalfLengthPos = boundBox.GetMax().GetYf();
	const float	OurBBHalfWidth  = boundBox.GetMax().GetXf();
	const float OurBBHalfWidthNeg = -boundBox.GetMin().GetXf();
	//const float	OurBBHalfLengthNeg = -boundBox.GetMin().GetYf();
	float	OtherBBHalfLengthPos = otherBox.GetMax().GetYf();
	const float	OtherBBHalfWidth = otherBox.GetMax().GetXf();
	const float OtherBBHalfWidthNeg = -otherBox.GetMin().GetXf();	//still positive to make below code more readable
	float	OtherBBHalfLengthNeg = -otherBox.GetMin().GetYf();

	//really skinny doors have a way of messing up our shapetests below
	//artificially fatten the doors
	if (bOtherEntIsDoor)
	{
		OtherBBHalfLengthPos = rage::Max(ms_fMinLengthForDoors, OtherBBHalfLengthPos);
		OtherBBHalfLengthNeg = rage::Max(ms_fMinLengthForDoors, OtherBBHalfLengthNeg);
	}

	// ReturnVal == LARGE_FLOAT means no collision.
	ReturnVal = LARGE_FLOAT;

	static dev_float s_fMaxTimeToLookAhead = 5.0f;	//seconds

#if __DEV
	TUNE_GROUP_BOOL(FOLLOW_AI_SLOW, bDrawBumperTests, false);
#endif //__DEV

	// Front and Side perpendicular so:
	// pOtherSide->x = otherDriveDir.y
	// pOtherSide->y = -otherDriveDir.x

	// NOTE: we now only do the 2 front points of the ai car. We're not really
	// interested in collisions the rear point might cause(we can't do anything
	// about that anyway)

	// Calculate the 2 points of our bumper.
	Vector3	BumperFrontRight, BumperFrontLeft, BumperNormal;
	BumperFrontRight.x = OurCarX + OurBBHalfLengthPos*ourNewDriveDir.x + OurBBHalfWidth*ourNewDriveDir.y;
	BumperFrontRight.y = OurCarY + OurBBHalfLengthPos*ourNewDriveDir.y - OurBBHalfWidth*ourNewDriveDir.x;
	BumperFrontLeft.x = OurCarX + OurBBHalfLengthPos*ourNewDriveDir.x - OurBBHalfWidthNeg*ourNewDriveDir.y;
	BumperFrontLeft.y = OurCarY + OurBBHalfLengthPos*ourNewDriveDir.y + OurBBHalfWidthNeg*ourNewDriveDir.x;
	BumperNormal.x = ourNewDriveDir.x;
	BumperNormal.y = ourNewDriveDir.y;

	BumperFrontRight.z = ourPosn.z;
	BumperFrontLeft.z = ourPosn.z;

	Vector3 vOtherPoints[4];
	vOtherPoints[0].x = OtherCarX + OtherBBHalfLengthPos*otherDriveDir.x + OtherBBHalfWidth*otherDriveDir.y;
	vOtherPoints[0].y = OtherCarY + OtherBBHalfLengthPos*otherDriveDir.y - OtherBBHalfWidth*otherDriveDir.x;
	vOtherPoints[0].z = 0.0f;
	vOtherPoints[1].x = OtherCarX + OtherBBHalfLengthPos*otherDriveDir.x - OtherBBHalfWidthNeg*otherDriveDir.y;
	vOtherPoints[1].y = OtherCarY + OtherBBHalfLengthPos*otherDriveDir.y + OtherBBHalfWidthNeg*otherDriveDir.x;
	vOtherPoints[1].z = 0.0f;
	vOtherPoints[2].x = OtherCarX - OtherBBHalfLengthNeg*otherDriveDir.x + OtherBBHalfWidth*otherDriveDir.y;
	vOtherPoints[2].y = OtherCarY - OtherBBHalfLengthNeg*otherDriveDir.y - OtherBBHalfWidth*otherDriveDir.x;
	vOtherPoints[2].z = 0.0f;
	vOtherPoints[3].x = OtherCarX - OtherBBHalfLengthNeg*otherDriveDir.x - OtherBBHalfWidthNeg*otherDriveDir.y;
	vOtherPoints[3].y = OtherCarY - OtherBBHalfLengthNeg*otherDriveDir.y + OtherBBHalfWidthNeg*otherDriveDir.x;
	vOtherPoints[3].z = 0.0f;

	const Vector3 vLineDelta = Vector3(OtherSpeedX, OtherSpeedY, 0.0f) * s_fMaxTimeToLookAhead;
	//const Vector3 vLinePerp = Vector3(OtherSpeedY, -OtherSpeedX, 0.0f) * s_fMaxTimeToLookAhead;

	float fTempDot1s[4];
	float fTempDot2s[4];
	for (int i = 0; i < 4; i++)
	{
		fTempDot1s[i] =(BumperFrontRight.x - vOtherPoints[i].x) * OtherSpeedY -(BumperFrontRight.y - vOtherPoints[i].y) * OtherSpeedX;
		fTempDot2s[i] =(BumperFrontLeft.x - vOtherPoints[i].x) * OtherSpeedY -(BumperFrontLeft.y - vOtherPoints[i].y) * OtherSpeedX;
	}

	for(OtherPoints = 0; OtherPoints < 4; OtherPoints++)
	{
		LineStart = vOtherPoints[OtherPoints];
		const Vector3 LineEnd = LineStart + vLineDelta;

		// Now check whether the Line goes through the bumper.
		const float DotProduct1 =(LineStart.x - BumperFrontRight.x) * BumperNormal.x +(LineStart.y - BumperFrontRight.y) * BumperNormal.y;
		const float DotProduct2 =(LineEnd.x - BumperFrontRight.x) * BumperNormal.x +(LineEnd.y - BumperFrontRight.y) * BumperNormal.y;

		if(DotProduct1 > 0.0f && DotProduct2 < 0.0f)
		{		
			// Other points are on either side of our bumper. Potential collision.
			// If the dot products of this point to each of the front bumper points have the same sign,
			// they must both be to one side of the vehicle, therefore the vehicles won't collide.
			// Exception: both bumper points are in between the bumpers of the opposing car.
			// We'll test for that later

			const float CollTime = (DotProduct1 * s_fMaxTimeToLookAhead) /(DotProduct1 - DotProduct2);

			const bool bBumpersAreOnDifferentSidesOfPoint = fTempDot1s[OtherPoints] * fTempDot2s[OtherPoints] < 0.0f;
			if(bBumpersAreOnDifferentSidesOfPoint)
			{		
				// We have a collision.
				ReturnVal = rage::Min(ReturnVal, CollTime);	
			}
			else if (CollTime < ReturnVal)
			{
				//if this potential collision may be closer than the one we already detected,
				//do a more accurate test at the t-value of the collision
				const Vector3 vDeltaToPotentialCollision = Vector3(OtherSpeedX, OtherSpeedY, 0.0f) * CollTime;
				const Vector3 vPoint0 = vOtherPoints[0] + vDeltaToPotentialCollision;
				const Vector3 vPoint1 = vOtherPoints[1] + vDeltaToPotentialCollision;
				const Vector3 vPoint2 = vOtherPoints[2] + vDeltaToPotentialCollision;
				const Vector3 vPoint3 = vOtherPoints[3] + vDeltaToPotentialCollision;

#if __DEV
				if (bDrawBumperTests)
				{
					Vector3 vPoint0Draw = vPoint0;
					Vector3 vPoint1Draw = vPoint1;
					Vector3 vPoint2Draw = vPoint2;
					Vector3 vPoint3Draw = vPoint3;
					vPoint0Draw.z = vPoint1Draw.z = vPoint2Draw.z = vPoint3Draw.z = hisPosn.z;
					CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vPoint0Draw), VECTOR3_TO_VEC3V(vPoint1Draw), Color_purple);
					CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vPoint1Draw), VECTOR3_TO_VEC3V(vPoint3Draw), Color_purple);
					CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vPoint3Draw), VECTOR3_TO_VEC3V(vPoint2Draw), Color_purple);
					CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vPoint2Draw), VECTOR3_TO_VEC3V(vPoint0Draw), Color_purple);

					CVehicle::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(BumperFrontRight), 0.1f, Color_yellow);
					CVehicle::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(BumperFrontLeft), 0.1f, Color_yellow);
				}
#endif //__DEV

				//project each point into the future
				if (!bOtherEntIsDoor)
				{
					if (geom2D::IntersectPointFlat(BumperFrontLeft.x, BumperFrontLeft.y, vPoint0.x, vPoint0.y,
						vPoint1.x, vPoint1.y, vPoint2.x, vPoint2.y, vPoint3.x, vPoint3.y)
						&& geom2D::IntersectPointFlat(BumperFrontRight.x, BumperFrontRight.y, vPoint0.x, vPoint0.y,
						vPoint1.x, vPoint1.y, vPoint2.x, vPoint2.y, vPoint3.x, vPoint3.y))
					{

						////////////////////////////////////
						// 			 		Vec3V vVehPos = pVeh->GetVehiclePosition();
						// 			 		Vec3V vUpTopPos = vVehPos;
						// 			 		vUpTopPos.SetZf(vVehPos.GetZf() + 10.0f);
						// 			 		CVehicle::ms_debugDraw.AddLine(vVehPos, vUpTopPos, Color_purple);
						///////////////////////////////////

						ReturnVal = CollTime;	//already checked that it was less than ReturnVal above
					}
				}
				else
				{
					//float fDummyT1, fDummyT2; 
					if (geom2D::IntersectPointFlat(BumperFrontLeft.x, BumperFrontLeft.y, vPoint0.x, vPoint0.y,
						vPoint1.x, vPoint1.y, vPoint2.x, vPoint2.y, vPoint3.x, vPoint3.y)
						|| geom2D::IntersectPointFlat(BumperFrontRight.x, BumperFrontRight.y, vPoint0.x, vPoint0.y,
						vPoint1.x, vPoint1.y, vPoint2.x, vPoint2.y, vPoint3.x, vPoint3.y)
						/*|| geom2D::Test2DSegVsSegFast(fDummyT1, fDummyT2, BumperFrontLeft.x, BumperFrontLeft.y, BumperFrontRight.x, BumperFrontRight.y
						, vPoint0.x, vPoint0.y, vPoint1.x, vPoint1.y)
						|| geom2D::Test2DSegVsSegFast(fDummyT1, fDummyT2, BumperFrontLeft.x, BumperFrontLeft.y, BumperFrontRight.x, BumperFrontRight.y
						, vPoint2.x, vPoint2.y, vPoint3.x, vPoint3.y)*/)
					{

						////////////////////////////////////
						// 			 		Vec3V vVehPos = pVeh->GetVehiclePosition();
						// 			 		Vec3V vUpTopPos = vVehPos;
						// 			 		vUpTopPos.SetZf(vVehPos.GetZf() + 10.0f);
						// 			 		CVehicle::ms_debugDraw.AddLine(vVehPos, vUpTopPos, Color_purple);
						///////////////////////////////////

						ReturnVal = CollTime;	//already checked that it was less than ReturnVal above
					}
				}			
			}
		}
	}

	return(ReturnVal);
}

// void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindRoadEdgesUsingNodeList(AvoidanceTaskObstructionData& obstructionData)
// {
// 	const CVehicle* pVeh = obstructionData.pVeh;
// 	Assert(pVeh);
// 	CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
// 
// 	//for now, don't do this if we don't have a nodelist
// 	//TODO: replace this with a version that uses the follow route
// 	if (!pNodeList)
// 	{
// 		return;
// 	}
// 
// 	const float fVehicleHalfWidth = obstructionData.vBoundBoxMax.GetXf();
// 
// 	const int iNumNodes = CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED;
// 
// 	bool bClampRoadEdgesToCenter = obstructionData.carAvoidanceLevel <= CAR_AVOIDANCE_LITTLE 
// 		&& !IsDrivingFlagSet(DF_AvoidingCarsUntilClear) 
// 		&& !obstructionData.bIsCurrentlyOnJunction;
// 
// 	//this is a hack to allow cars to steer on the other side of the road when avoiding abandoned/stopped playercars,
// 	//while keeping them in their lanes for avoiding moving traffic -JM
// 	const CPhysical* pEntitySteeringAround = pVeh->GetIntelligence()->GetEntitySteeringAround();
// 	const CVehicle* pVehicleSteeringAround = pEntitySteeringAround && pEntitySteeringAround->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pEntitySteeringAround) : NULL;
// 	if (bClampRoadEdgesToCenter && pVehicleSteeringAround && 
// 		(IsThisAStationaryCar(pVehicleSteeringAround) || pVehicleSteeringAround->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle)
// 		&& IsThereRoomToOvertakeVehicle(pVeh, pVehicleSteeringAround, true))
// 	{
// 		bClampRoadEdgesToCenter = false;
// 	}
// 
// 	const Vector3 vBumperPos = RCC_VECTOR3(obstructionData.vBumperCenter);
// 	const Vec2f vVehiclePos2d(vBumperPos.x, vBumperPos.y);
// 
// 	Vector3 vLastLeftEnd(Vector3::ZeroType), vLastRightEnd(Vector3::ZeroType);
// 	int iLastEndIndex = -1;
// 	for (int i = 0; i < iNumNodes-1; i++)
// 	{
// 		Vector3 vLeftEnd, vRightEnd, vLeftStart, vRightStart;
// 		CNodeAddress fromNode = pNodeList->GetPathNodeAddr(i);
// 		CNodeAddress toNode = pNodeList->GetPathNodeAddr(i+1);
// 		CNodeAddress nextNode;
// 		if (i < iNumNodes-2)
// 		{
// 			nextNode = pNodeList->GetPathNodeAddr(i+2);
// 		}
// 
// 		if (!ThePaths.GetBoundarySegmentsBetweenNodes(fromNode, toNode, nextNode, bClampRoadEdgesToCenter, vLeftEnd, vRightEnd, vLeftStart, vRightStart, fVehicleHalfWidth))
// 		{
// 			AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
// 			newSeg.m_Valid = false;
// 			continue;
// 		}
// 
// 		Vec2f carToLeft = Vec2f(vLeftStart.x, vLeftStart.y) - vVehiclePos2d;
// 		Vec2f carToRight = Vec2f(vRightStart.x, vRightStart.y) - vVehiclePos2d;
// 		Vec2f leftToRight = Vec2f(vLeftStart.x, vLeftStart.y) - Vec2f(vRightStart.x, vRightStart.y);
// 
// 		const float fDotLeft = Dot(leftToRight, carToLeft);
// 		const float fDotRight = Dot(leftToRight, carToRight);
// 		if (fDotRight * fDotLeft > 0.0f)
// 		{
// 			// both positive or both negative
// 			//we're outside the bounds, try again with wider edges
// 			if (!ThePaths.GetBoundarySegmentsBetweenNodes(fromNode, toNode, nextNode, bClampRoadEdgesToCenter, vLeftEnd, vRightEnd, vLeftStart, vRightStart, 0.0f))
// 			{
// 				AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
// 				newSeg.m_Valid = false;
// 				continue;
// 			}
// 		}
// 
// 		//if the last coords we got were from the previous node in our path,
// 		//just use them
// 		if (iLastEndIndex > -1 && iLastEndIndex == i-1)
// 		{
// 			vLeftStart = vLastLeftEnd;
// 			vRightStart = vLastRightEnd;
// 		}
// 
// 		vLastLeftEnd = vLeftEnd;
// 		vLastRightEnd = vRightEnd;
// 		iLastEndIndex = i;
// 
// 		AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
// 		newSeg.m_LeftStart = Vec2f(vLeftStart.x, vLeftStart.y);
// 		newSeg.m_LeftEnd = Vec2f(vLeftEnd.x, vLeftEnd.y);
// 		newSeg.m_RightStart = Vec2f(vRightStart.x, vRightStart.y);
// 		newSeg.m_RightEnd = Vec2f(vRightEnd.x, vRightEnd.y);
// 		newSeg.m_Valid = true;
// 	}
// }

void CTaskVehicleGoToPointWithAvoidanceAutomobile::FindRoadEdgesUsingFollowRoute(AvoidanceTaskObstructionData& obstructionData)
{
	const CVehicle* pVeh = obstructionData.pVeh;
	Assert(pVeh);
	const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();

	//for now, don't do this if we don't have a followroute
	if (!pFollowRoute)
	{
		return;
	}

	const float fVehicleHalfWidth = obstructionData.vBoundBoxMax.GetXf();

	const int iNumNodes = pFollowRoute->GetNumPoints();

	bool bClampRoadEdgesToCenter = obstructionData.carAvoidanceLevel <= CAR_AVOIDANCE_LITTLE 
		&& !m_bAvoidingCarsUntilClear 
		&& !obstructionData.bIsCurrentlyOnJunction;

	//this is a hack to allow cars to steer on the other side of the road when avoiding abandoned/stopped playercars,
	//while keeping them in their lanes for avoiding moving traffic -JM
	const CPhysical* pEntitySteeringAround = pVeh->GetIntelligence()->GetEntitySteeringAround();
	const CVehicle* pVehicleSteeringAround = pEntitySteeringAround && pEntitySteeringAround->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pEntitySteeringAround) : NULL;
	if (bClampRoadEdgesToCenter 
		&& pVehicleSteeringAround 
		&& (IsThisAStationaryCar(pVehicleSteeringAround) || pVehicleSteeringAround->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle)
		&& IsThereRoomToOvertakeVehicle(pVeh, pVehicleSteeringAround, true))
	{
		bClampRoadEdgesToCenter = false;
	}

	const Vector3 vBumperPos = RCC_VECTOR3(obstructionData.vBumperCenter);
	const Vec2f vVehiclePos2d(vBumperPos.x, vBumperPos.y);

	Vector3 vLastLeftEnd(Vector3::ZeroType), vLastRightEnd(Vector3::ZeroType);
	int iLastEndIndex = -1;
	AvoidanceTaskObstructionData::RoadSegment* pLastSeg = NULL;
	AvoidanceTaskObstructionData::RoadSegment* pLastLastSeg = NULL;	
	const CRoutePoint* pRoutePoints = pFollowRoute->GetRoutePoints();
	for (int i = 0; i < iNumNodes-1; i++)
	{
		Vector3 vLeftEnd, vRightEnd, vLeftStart, vRightStart;
		int iNextIndex = -1;
		if (i < iNumNodes-2)
		{
			iNextIndex = i+2;
		}

		const CRoutePoint& rFromPoint = pRoutePoints[i];

		//on single-track roads, we want to prefer going right, and since the "avoid road edges"
		//weight is quadratic, moving them in closer by the half-width increases the weight of the right side
		//at a rate much quicker than on the left. this can overwhelm the score of the "prefer going right
		//on single track roads" rule
		const float fHalfWidthToUseForFirstTestLeftSide = fVehicleHalfWidth;
		float fHalfWidthToUseForFirstTestRightSide = fVehicleHalfWidth;
		static dev_float s_fHackExtraWidthBase = 1.0f;
		float fHalfWidthToUseForSecondTestRightSide = 0.0f;
		if (rFromPoint.m_bIsSingleTrack)
		{
			fHalfWidthToUseForFirstTestRightSide = -s_fHackExtraWidthBase;
			fHalfWidthToUseForSecondTestRightSide = -s_fHackExtraWidthBase;
		}
		else if (rFromPoint.m_bIsHighway)
		{
			fHalfWidthToUseForFirstTestRightSide = 0.0f;
		}

		if (!pFollowRoute->GetBoundarySegmentsBetweenPoints(i, i+1, iNextIndex, bClampRoadEdgesToCenter, vLeftEnd, vRightEnd, vLeftStart, vRightStart, fHalfWidthToUseForFirstTestLeftSide, fHalfWidthToUseForFirstTestRightSide, true))
		{
			AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
			newSeg.m_Valid = false;
			continue;
		}

		Vec2f carToLeft = Vec2f(vLeftStart.x, vLeftStart.y) - vVehiclePos2d;
		Vec2f carToRight = Vec2f(vRightStart.x, vRightStart.y) - vVehiclePos2d;
		Vec2f leftToRight = Vec2f(vLeftStart.x, vLeftStart.y) - Vec2f(vRightStart.x, vRightStart.y);

		const float fDotLeft = Dot(leftToRight, carToLeft);
		const float fDotRight = Dot(leftToRight, carToRight);
		if (fDotRight * fDotLeft > 0.0f)
		{
			// both positive or both negative
			//we're outside the bounds, try again with wider edges
			if (!pFollowRoute->GetBoundarySegmentsBetweenPoints(i, i+1, iNextIndex, bClampRoadEdgesToCenter, vLeftEnd, vRightEnd, vLeftStart, vRightStart, 0.0f, fHalfWidthToUseForSecondTestRightSide, true))
			{
				AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
				newSeg.m_Valid = false;
				continue;
			}
		}

#if __BANK
		if (ms_bDisplayAvoidanceRoadSegments && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			Color32 color = Color_white;

			if (pRoutePoints[i].m_bJunctionNode)
			{
				color = Color_green;
			}

			if (pRoutePoints[i+1].m_bJunctionNode)
			{
				color = Color_magenta;
			}

			if (iNextIndex >= 0 && pRoutePoints[iNextIndex].m_bJunctionNode)
			{
				color = Color_cyan;
			}

			const Vector3 vHeightOffset(0.0f, 0.0f, 5.0f);
			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vLeftStart + vHeightOffset), VECTOR3_TO_VEC3V(vLeftEnd + vHeightOffset), 0.5f, color, -1);
			grcDebugDraw::Line(vLeftEnd + vHeightOffset, vRightEnd + vHeightOffset, color, color, -1);
			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vRightStart + vHeightOffset), VECTOR3_TO_VEC3V(vRightEnd + vHeightOffset), 0.5f, color, -1);
			grcDebugDraw::Line(vRightStart + vHeightOffset, vLeftStart + vHeightOffset, color, color, -1);
		}
#endif // __BANK

		Vec2f thisLeftStart =  Vec2f(vLeftStart.x, vLeftStart.y);
		Vec2f thisRightStart = Vec2f(vRightStart.x, vRightStart.y);

		//if the last coords we got were from the previous node in our path, just use them
		if (iLastEndIndex > -1 && iLastEndIndex == i-1)
		{
			if (!rFromPoint.m_bIgnoreForNavigation && pRoutePoints[iLastEndIndex].m_bJunctionNode && pLastSeg && pLastLastSeg)
			{
				//work out what way we're going at this junction
				Vector3 vLastToJunction = VEC3V_TO_VECTOR3(pRoutePoints[iLastEndIndex].GetPosition() - pRoutePoints[iLastEndIndex-1].GetPosition());
				Vec2f lastToJunction = Vec2f(vLastToJunction.x, vLastToJunction.y);
				Vector3 vLastToCurrent = VEC3V_TO_VECTOR3(pRoutePoints[i].GetPosition() - pRoutePoints[iLastEndIndex-1].GetPosition());
				Vec2f lastToCurrent = Vec2f(vLastToCurrent.x, vLastToCurrent.y);

				//check we aren't looping back onto previous point
				if(IsZeroAll(lastToJunction) || IsZeroAll(lastToCurrent))
				{
					obstructionData.m_RoadSegments.clear();
					break;
				}
		
				lastToJunction = NormalizeFast(lastToJunction);
				lastToCurrent = NormalizeFast(lastToCurrent);

				float fDotJunction = Dot(lastToJunction, lastToCurrent);
				if(fDotJunction < 0.9f)
				{
					float fRoadWidthLeft=0.0f, fRoadWidthRight=0.0f;
					pRoutePoints[iLastEndIndex-1].FindRoadBoundaries(fRoadWidthLeft, fRoadWidthRight);
					static float s_fRoadWidthScalar = 0.25f;
					Vec2f vJunctionCenter = Vec2f(pRoutePoints[iLastEndIndex].GetPosition().GetXf(),pRoutePoints[iLastEndIndex].GetPosition().GetYf());
					if(Cross(lastToJunction, lastToCurrent) < 0.0f)
					{
						//going right
						//average start positions of segments either side of junction
						Vec2f lastLastStartToThisStart =  (pLastLastSeg->m_RightStart + thisRightStart) * 0.5f;
						Vec2f vLastStartToJunctionCenter = vJunctionCenter - lastLastStartToThisStart;
						vLastStartToJunctionCenter = Normalize(vLastStartToJunctionCenter);

						//add small buffer towards junction center to round out corner
						lastLastStartToThisStart += vLastStartToJunctionCenter * fRoadWidthRight * s_fRoadWidthScalar;
						pLastLastSeg->m_RightEnd = lastLastStartToThisStart;
						pLastSeg->m_RightStart = lastLastStartToThisStart;
					}
					else
					{
						//going left
						Vec2f lastLastStartToThisStart =  (pLastLastSeg->m_LeftStart + thisLeftStart) * 0.5f;
						Vec2f vLastStartToJunctionCenter = vJunctionCenter - lastLastStartToThisStart;
						vLastStartToJunctionCenter = Normalize(vLastStartToJunctionCenter);

						lastLastStartToThisStart += vLastStartToJunctionCenter * fRoadWidthLeft * s_fRoadWidthScalar;
						pLastLastSeg->m_LeftEnd = lastLastStartToThisStart;
						pLastSeg->m_LeftStart = lastLastStartToThisStart;
					}
				}
				else
				{
					//going straight
					//simply create segment that makes straight line across junction
					Vec2f lastLastStartToThisStartRight =  (pLastLastSeg->m_RightStart + thisRightStart) * 0.5f;
					Vec2f lastLastStartToThisStartLeft =  (pLastLastSeg->m_LeftStart + thisLeftStart) * 0.5f;	
					pLastLastSeg->m_LeftEnd = lastLastStartToThisStartLeft;
					pLastLastSeg->m_RightEnd = lastLastStartToThisStartRight;
					pLastSeg->m_LeftStart = lastLastStartToThisStartLeft;
					pLastSeg->m_RightStart = lastLastStartToThisStartRight;
				}

				pLastSeg->m_LeftEnd = thisLeftStart;
				pLastSeg->m_RightEnd = thisRightStart;
			}
			else
			{
				vLeftStart = vLastLeftEnd;
				vRightStart = vLastRightEnd;
			}
		}
		
		vLastLeftEnd = vLeftEnd;
		vLastRightEnd = vRightEnd;
		iLastEndIndex = i;

		AvoidanceTaskObstructionData::RoadSegment& newSeg = obstructionData.m_RoadSegments.Append();
		newSeg.m_LeftStart = Vec2f(vLeftStart.x, vLeftStart.y);
		newSeg.m_LeftEnd = Vec2f(vLeftEnd.x, vLeftEnd.y);
		newSeg.m_RightStart = Vec2f(vRightStart.x, vRightStart.y);
		newSeg.m_RightEnd = Vec2f(vRightEnd.x, vRightEnd.y);
		newSeg.m_Valid = true;

		//if this is the last segment, and the last point on the route
		//is a junction, don't add any segment. if a junction weren't
		//the last node, we'd remove the segment in the direction we're turning,
		//so since we don't know which way that may be, just don't prevent them
		//from going either way for now
		const CRoutePoint& rToPoint = pRoutePoints[i+1];
		if (IsDrivingFlagSet(DF_ExtendRouteWithWanderResults) && i==iNumNodes-2 && rToPoint.m_bJunctionNode)
		{
			newSeg.m_Valid = false;
		}

		pLastLastSeg = pLastSeg;
		pLastSeg = &newSeg;
	}
}


// bool CTaskVehicleGoToPointWithAvoidanceAutomobile::DoesLineSegmentCrossRoadBoundariesUsingNodeList(const AvoidanceTaskObstructionData& obstructionData, const float fHeading, const float fSegmentLength, float& fDistToIntersectionOut) const
// {
// 	const CVehicle* pVeh = obstructionData.pVeh;
// 
// 	Assert(pVeh);
// 	CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
// 
// 	// Fine to have no node list, no extents to check.
// 	if (!pNodeList)
// 	{
// 		return false;
// 	}
// 
// 	//if this is 0, it's probably because we didn't build this array due to not
// 	//having a nodelist.  while the above early return for !pNodeList is probably
// 	//sufficient, I'm adding this in the interest of robustness. if something were to happen
// 	//to give us a valid nodelist in the meantime, we don't care if we didn't
// 	//build the m_RoadSegments list up with information about the nodelist already
// 	if (obstructionData.m_RoadSegments.GetCount() == 0)
// 	{
// 		return false;
// 	}
// 
// 	fDistToIntersectionOut = -1.0f;
// 
// 	//if we have a valid nodelist,
// 	//iterate through each segment of our path
// 	//generate edges
// 	//test the segment against each edge
// 	//return early if we find a collision
// 	Vector3 vehDriveDir = RCC_VECTOR3(obstructionData.vNormalizedDriveDir);
// 	const float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
// 	//const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
// 	// Use the coordinates of our front bumper.
// 	Vector3 vBumperPos = RCC_VECTOR3(obstructionData.vBumperCenter);
// 	const Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()));
// 	const Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
// 	const Vector3 Offset1 = fSegmentLength * rage::Cosf(vehDriveOrientation - fHeading) * VehForwardVector;
// 	const Vector3 Offset2 = fSegmentLength * rage::Sinf(vehDriveOrientation - fHeading) * VehRightVector;
// 	const Vector3 vSearchPos = vBumperPos + Offset1 + Offset2;
// 	const Vec2f vVehiclePos2d(vBumperPos.x, vBumperPos.y);
// 	const Vec2f vSearchPos2d(vSearchPos.x, vSearchPos.y);
// 
// //?	const bool bClampRoadEdgesToCenter = carAvoidanceLevel <= CAR_AVOIDANCE_LITTLE;
// 
// 	const int iNumNodes = CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED;
// 
// 	s32 iTargetNode = pNodeList->GetTargetNodeIndex();
// 	s32 iOldNode = iTargetNode - 1;
// 
// 	//if we aren't on the road at all, don't let this code prevent us from getting back on
// 	const AvoidanceTaskObstructionData::RoadSegment& currentRoadSeg = obstructionData.m_RoadSegments[Max(iOldNode, 0)];
// 	if (currentRoadSeg.m_Valid)
// 	{
// 		Vec2f carToLeft = currentRoadSeg.m_LeftStart - vVehiclePos2d;
// 		Vec2f carToRight = currentRoadSeg.m_RightStart - vVehiclePos2d;
// 		Vec2f leftToRight = currentRoadSeg.m_LeftStart - currentRoadSeg.m_RightStart;
// 
// 		const float fDotLeft = Dot(leftToRight, carToLeft);
// 		const float fDotRight = Dot(leftToRight, carToRight);
// 
// 		if (fDotRight * fDotLeft > 0.0f)
// 		{
// 			// both positive or both negative
// #if __BANK
// 			if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
// 			{
// 				grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vBumperPos), 1.0f, Color_purple, 0, 0, false);
// 			}
// #endif //BANK
// 			return false;
// 		}
// 
// 	}
// 
// 	for (int i = 0; i < iNumNodes-1; i++)
// 	{
// 		const AvoidanceTaskObstructionData::RoadSegment& roadSeg = obstructionData.m_RoadSegments[i];
// 
// 		if (!roadSeg.m_Valid)
// 		{
// 			continue;
// 		}
// 
// #if __BANK
// 		if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
// 		{
// 			Vector3 vLeftStart(roadSeg.m_LeftStart.GetX(), roadSeg.m_LeftStart.GetY(), vBumperPos.z);
// 			Vector3 vLeftEnd(roadSeg.m_LeftEnd.GetX(), roadSeg.m_LeftEnd.GetY(), vBumperPos.z);
// 			Vector3 vRightStart(roadSeg.m_RightStart.GetX(), roadSeg.m_RightStart.GetY(), vBumperPos.z);
// 			Vector3 vRightEnd(roadSeg.m_RightEnd.GetX(), roadSeg.m_RightEnd.GetY(), vBumperPos.z);
// 			grcDebugDraw::Line(vLeftStart, vLeftEnd, Color_green);
// 			grcDebugDraw::Line(vRightStart, vRightEnd, Color_blue);
// 		}
// #endif //__BANK
// 		
// 		float fTBorder, fTTestSegment;
// 		if (geom2D::Test2DSegVsSegFast(fTTestSegment, fTBorder, 
// 			vVehiclePos2d.GetX(), vVehiclePos2d.GetY(), 
// 			vSearchPos2d.GetX(), vSearchPos2d.GetY(), 
// 			roadSeg.m_LeftStart.GetX(), roadSeg.m_LeftStart.GetY(), 
// 			roadSeg.m_LeftEnd.GetX(), roadSeg.m_LeftEnd.GetY()))
// 		{
// 			fDistToIntersectionOut = fTTestSegment * fSegmentLength;
// 			return true;
// 		}
// 		if (geom2D::Test2DSegVsSegFast(fTTestSegment, fTBorder, 
// 			vVehiclePos2d.GetX(), vVehiclePos2d.GetY(), 
// 			vSearchPos2d.GetX(), vSearchPos2d.GetY(), 
// 			roadSeg.m_RightStart.GetX(), roadSeg.m_RightStart.GetY(), 
// 			roadSeg.m_RightEnd.GetX(), roadSeg.m_RightEnd.GetY()))
// 		{
// 			fDistToIntersectionOut = fTTestSegment * fSegmentLength;
// 			return true;
// 		}
// 	}
// 	
// 	return false;
// }

#if !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::DoesLineSegmentCrossRoadBoundariesUsingFollowRoute(const AvoidanceTaskObstructionData& obstructionData, const float fHeading, const float fSegmentLength, float& fDistToIntersectionOut) const
{
	const CVehicle* pVeh = obstructionData.pVeh;

	Assert(pVeh);
	const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();

	// Fine to have no followroute, no extents to check.
	if (!pFollowRoute)
	{
		return false;
	}

	//if this is 0, it's probably because we didn't build this array due to not
	//having a nodelist.  while the above early return for !pFollowRoute is probably
	//sufficient, I'm adding this in the interest of robustness. if something were to happen
	//to give us a valid nodelist in the meantime, we don't care if we didn't
	//build the m_RoadSegments list up with information about the nodelist already
	if (obstructionData.m_RoadSegments.GetCount() == 0)
	{
		return false;
	}

	fDistToIntersectionOut = -1.0f;

	//if we have a valid nodelist,
	//iterate through each segment of our path
	//generate edges
	//test the segment against each edge
	//return early if we find a collision
	Vector3 vehDriveDir = RCC_VECTOR3(obstructionData.vNormalizedDriveDir);
	const float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
	//const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	// Use the coordinates of our front bumper.
	Vector3 vBumperPos = RCC_VECTOR3(obstructionData.vBumperCenter);
	const Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()));
	const Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
	const Vector3 Offset1 = fSegmentLength * rage::Cosf(vehDriveOrientation - fHeading) * VehForwardVector;
	const Vector3 Offset2 = fSegmentLength * rage::Sinf(vehDriveOrientation - fHeading) * VehRightVector;
	const Vector3 vSearchPos = vBumperPos + Offset1 + Offset2;
	const Vec2f vVehiclePos2d(vBumperPos.x, vBumperPos.y);
	const Vec2f vSearchPos2d(vSearchPos.x, vSearchPos.y);

	//?	const bool bClampRoadEdgesToCenter = carAvoidanceLevel <= CAR_AVOIDANCE_LITTLE;

	const int iNumNodes = CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED;

	s32 iTargetNode = pFollowRoute->GetCurrentTargetPoint();
	s32 iOldNode = iTargetNode - 1;

	if(Max(iOldNode, 0) >= obstructionData.m_RoadSegments.GetCount())
	{
		return false;
	}

	//if we aren't on the road at all, don't let this code prevent us from getting back on
	const AvoidanceTaskObstructionData::RoadSegment& currentRoadSeg = obstructionData.m_RoadSegments[Max(iOldNode, 0)];
	if (currentRoadSeg.m_Valid)
	{
		Vec2f carToLeft = currentRoadSeg.m_LeftStart - vVehiclePos2d;
		Vec2f carToRight = currentRoadSeg.m_RightStart - vVehiclePos2d;
		Vec2f leftToRight = currentRoadSeg.m_LeftStart - currentRoadSeg.m_RightStart;

		const float fDotLeft = Dot(leftToRight, carToLeft);
		const float fDotRight = Dot(leftToRight, carToRight);

		if (fDotRight * fDotLeft > 0.0f)
		{
			// both positive or both negative
#if __BANK
			if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vBumperPos), 1.0f, Color_purple, 0, 0, false);
			}
#endif //BANK
			return false;
		}

	}

	for (int i = 0; i < iNumNodes-1 && i < obstructionData.m_RoadSegments.GetCount(); i++)
	{
		const AvoidanceTaskObstructionData::RoadSegment& roadSeg = obstructionData.m_RoadSegments[i];

		if (!roadSeg.m_Valid)
		{
			continue;
		}

#if __BANK
		if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			Vector3 vLeftStart(roadSeg.m_LeftStart.GetX(), roadSeg.m_LeftStart.GetY(), vBumperPos.z);
			Vector3 vLeftEnd(roadSeg.m_LeftEnd.GetX(), roadSeg.m_LeftEnd.GetY(), vBumperPos.z);
			Vector3 vRightStart(roadSeg.m_RightStart.GetX(), roadSeg.m_RightStart.GetY(), vBumperPos.z);
			Vector3 vRightEnd(roadSeg.m_RightEnd.GetX(), roadSeg.m_RightEnd.GetY(), vBumperPos.z);
			//grcDebugDraw::Line(vLeftStart, vLeftEnd, Color_green);
			//grcDebugDraw::Line(vRightStart, vRightEnd, Color_blue);
			CVehicle::ms_debugDraw.AddLine(vLeftStart, vLeftEnd, Color_green);
			CVehicle::ms_debugDraw.AddLine(vRightStart, vRightEnd, Color_blue);
		}
#endif //__BANK

		float fTBorder, fTTestSegment;
		if (geom2D::Test2DSegVsSegFast(fTTestSegment, fTBorder, 
			vVehiclePos2d.GetX(), vVehiclePos2d.GetY(), 
			vSearchPos2d.GetX(), vSearchPos2d.GetY(), 
			roadSeg.m_LeftStart.GetX(), roadSeg.m_LeftStart.GetY(), 
			roadSeg.m_LeftEnd.GetX(), roadSeg.m_LeftEnd.GetY()))
		{
			fDistToIntersectionOut = fTTestSegment * fSegmentLength;
			return true;
		}
		if (geom2D::Test2DSegVsSegFast(fTTestSegment, fTBorder, 
			vVehiclePos2d.GetX(), vVehiclePos2d.GetY(), 
			vSearchPos2d.GetX(), vSearchPos2d.GetY(), 
			roadSeg.m_RightStart.GetX(), roadSeg.m_RightStart.GetY(), 
			roadSeg.m_RightEnd.GetX(), roadSeg.m_RightEnd.GetY()))
		{
			fDistToIntersectionOut = fTTestSegment * fSegmentLength;
			return true;
		}
	}

	return false;
}

#else	// !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

/*
PURPOSE
	Works like geom2D::Test2DSegVsSegFast, except that it operates on
	four segments to test against four other segments.
PARAMS
	hitT1OutV	- T values for the intersections, if they exist, along the first segments.
	start1xV	- X start coordinates of the first four segments.
	start1yV	- Y start coordinates of the first four segments.
	end1xV		- X end coordinates of the first four segments.
	end1yV		- Y end coordinates of the first four segments.
	start2xV	- X start coordinates of the second four segments.
	start2yV	- Y start coordinates of the second four segments.
	end2xV		- X end coordinates of the second four segments.
	end2yV		- Y end coordinates of the second four segments.
RETURNS
	A vector bool mask representing which intersections were found.
*/
static VecBoolV_Out sTest4x2DSegVsSegOneIsect(
		Vec4V_InOut hitT1OutV, /* Vec4V_InOut hitT2OutV, */
		Vec4V_In start1xV, Vec4V_In start1yV,
		Vec4V_In end1xV, Vec4V_In end1yV,
		Vec4V_In start2xV, Vec4V_In start2yV,
		Vec4V_In end2xV, Vec4V_In end2yV)
{
	const Vec4V parallelThresholdV(V_FLT_EPSILON);
	const Vec4V zeroV(V_ZERO);
	const Vec4V oneV(V_ONE);

	const Vec4V uXV = Subtract(end1xV, start1xV);
	const Vec4V uYV = Subtract(end1yV, start1yV);
	const Vec4V vXV = Subtract(end2xV, start2xV);
	const Vec4V negVYV = Subtract(start2yV, end2yV);

	const Vec4V vPerpXV = negVYV;
	const Vec4V vPerpYV = vXV;
	const Vec4V uPerpXV = Negate(uYV);
	const Vec4V uPerpYV = uXV;

	const Vec4V wXV = Subtract(start1xV, start2xV);
	const Vec4V wYV = Subtract(start1yV, start2yV);

	const Vec4V negVPerpDotUV = AddScaled(Scale(vPerpXV, uXV), vPerpYV, uYV);

	const Vec4V absVPerpDotUV = Abs(negVPerpDotUV);

	const Vec4V s0V = AddScaled(Scale(vPerpXV, wXV), vPerpYV, wYV);
	const Vec4V t0V = AddScaled(Scale(uPerpXV, wXV), uPerpYV, wYV);

	const VecBoolV vPerpDotUGreaterThanZero = IsLessThan(negVPerpDotUV, zeroV);

	const Vec4V sV = SelectFT(vPerpDotUGreaterThanZero, Negate(s0V), s0V);
	const Vec4V tV = SelectFT(vPerpDotUGreaterThanZero, Negate(t0V), t0V);

	const Vec4V hitT1V = InvScale(sV, absVPerpDotUV);

	// Could do this, if we needed the intersections with the second segments:
	//	const Vec4V hitT2V = InvScale(tV, absVPerpDotUV);

	const VecBoolV linesNotParallelV = IsGreaterThanOrEqual(absVPerpDotUV, parallelThresholdV);
	const VecBoolV isect1ValidV = And(IsGreaterThanOrEqual(sV, zeroV), IsLessThanOrEqual(sV, absVPerpDotUV));
	const VecBoolV isect2ValidV = And(IsGreaterThanOrEqual(tV, zeroV), IsLessThanOrEqual(tV, absVPerpDotUV));

	const VecBoolV retV = And(And(isect1ValidV, isect2ValidV), linesNotParallelV);

	hitT1OutV = hitT1V;

	// Could do this, if we needed the intersections with the second segments:
	//	hitT2OutV = hitT2V;

	return retV;
}

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::DoLineSegmentsCrossRoadBoundariesUsingFollowRoute(
		const AvoidanceTaskObstructionData& obstructionData, const AvoidanceInfo* avoidanceInfoArray, const int numDirections,
		const float& fSegmentLength, Vec4V* distToRoadEdgeVectorArrayOut) const
{
	const CVehicle* pVeh = obstructionData.pVeh;

	Assert(pVeh);
	const CVehicleFollowRouteHelper* pFollowRoute = pVeh->GetIntelligence()->GetFollowRouteHelper();

	// Fine to have no followroute, no extents to check.
	if (!pFollowRoute)
	{
		return false;
	}

	s32 iTargetNode = pFollowRoute->GetCurrentTargetPoint();
	s32 iOldNode = iTargetNode - 1;

	//possible if we don't have a fully valid route (u-turn etc)
	if(Max(iOldNode, 0) >= obstructionData.m_RoadSegments.GetCount())
	{
		return false;
	}

	const Vec2f vVehiclePos2d(obstructionData.vBumperCenter.GetXf(), obstructionData.vBumperCenter.GetYf());

	// This part doesn't actually depend on the directions at all, so we do the test outside of the loop over the directions
	// (could possibly make sense to even break it out of the function, since we may still do it more than once per vehicle now):

	//if we aren't on the road at all, don't let this code prevent us from getting back on
	const AvoidanceTaskObstructionData::RoadSegment& currentRoadSeg = obstructionData.m_RoadSegments[Max(iOldNode, 0)];
	if (currentRoadSeg.m_Valid)
	{
#if __BANK
		if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(Vector3(currentRoadSeg.m_LeftStart.GetX(), currentRoadSeg.m_LeftStart.GetY(), obstructionData.vBumperCenter.GetZf()), VEC3V_TO_VECTOR3(obstructionData.vBumperCenter), Color_purple, Color_purple);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(obstructionData.vBumperCenter), Vector3(currentRoadSeg.m_RightStart.GetX(), currentRoadSeg.m_RightStart.GetY(), obstructionData.vBumperCenter.GetZf()), Color_purple, Color_purple);
			grcDebugDraw::Line(Vector3(currentRoadSeg.m_RightStart.GetX(), currentRoadSeg.m_RightStart.GetY(), obstructionData.vBumperCenter.GetZf()), Vector3(currentRoadSeg.m_LeftStart.GetX(), currentRoadSeg.m_LeftStart.GetY(), obstructionData.vBumperCenter.GetZf()), Color_purple, Color_purple);

			grcDebugDraw::Arrow(Vec3V(currentRoadSeg.m_LeftStart.GetX(), currentRoadSeg.m_LeftStart.GetY(), obstructionData.vBumperCenter.GetZf()), Vec3V(currentRoadSeg.m_LeftEnd.GetX(), currentRoadSeg.m_LeftEnd.GetY(), obstructionData.vBumperCenter.GetZf()), 0.5f, Color_purple);
			grcDebugDraw::Line(Vector3(currentRoadSeg.m_LeftEnd.GetX(), currentRoadSeg.m_LeftEnd.GetY(), obstructionData.vBumperCenter.GetZf()), Vector3(currentRoadSeg.m_RightEnd.GetX(), currentRoadSeg.m_RightEnd.GetY(), obstructionData.vBumperCenter.GetZf()), Color_purple, Color_purple);
			grcDebugDraw::Arrow(Vec3V(currentRoadSeg.m_RightStart.GetX(), currentRoadSeg.m_RightStart.GetY(), obstructionData.vBumperCenter.GetZf()), Vec3V(currentRoadSeg.m_RightEnd.GetX(), currentRoadSeg.m_RightEnd.GetY(), obstructionData.vBumperCenter.GetZf()), 0.5f, Color_purple);
		}
#endif // __BANK

		Vec2f carToLeft = currentRoadSeg.m_LeftStart - vVehiclePos2d;
		Vec2f carToRight = currentRoadSeg.m_RightStart - vVehiclePos2d;
		Vec2f leftToRight = currentRoadSeg.m_LeftStart - currentRoadSeg.m_RightStart;

		const float fDotLeft = Dot(leftToRight, carToLeft);
		const float fDotRight = Dot(leftToRight, carToRight);

		if (fDotRight * fDotLeft > 0.0f)
		{
			// both positive or both negative
#if __BANK
			if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				const Vec3V bumperPosV = obstructionData.vBumperCenter;
				grcDebugDraw::Sphere(bumperPosV, 1.0f, Color_purple);
			}
#endif //BANK
			return false;
		}
	}

	const Vec3V bumperPosV = obstructionData.vBumperCenter;

	// We need temporary arrays for X and Y positions in SoA form:
	Assert(numDirections <= kMaxObstructionProbeDirections);
	static const int kMaxVecs = kMaxObstructionProbeDirections/4;
	CompileTimeAssert(kMaxVecs*4 == kMaxObstructionProbeDirections);
	Vec4V searchPosXArrayVecs[kMaxVecs];
	Vec4V searchPosYArrayVecs[kMaxVecs];

	const ScalarV segmentLengthV = LoadScalar32IntoScalarV(fSegmentLength);

	//if we have a valid nodelist,
	//iterate through each segment of our path
	//generate edges
	//test the segment against each edge
	//return early if we find a collision

	// This could be done more efficiently:
	const float vehDriveOrientation = fwAngle::GetATanOfXY(obstructionData.vNormalizedDriveDir.GetXf(), obstructionData.vNormalizedDriveDir.GetYf());
	const ScalarV vehDriveOrientationV(vehDriveOrientation);

	// Get directional vectors for the vehicle, and scale.
	const Vec3V vehRightVectorV = pVeh->GetVehicleRightDirection();
	const Vec3V vehForwardVectorV = pVeh->GetVehicleForwardDirection();
	const Vec3V scaledVehForwardV = Scale(vehForwardVectorV, segmentLengthV);
	const Vec3V scaledVehRightV = Scale(vehRightVectorV, segmentLengthV);

	// Loop over the avoidance objects and initialize searchPos[X/Y]ArrayVecs.
	const int lastDir = numDirections - 1;
	for(int j = 0, k = 0; j < numDirections; j += 4, k++)
	{
		// Note: the Min() stuff here is a bit messy, but reading past the array passed in
		// by the user would be a bit risky. And, we do want valid data at the end of the array,
		// so we can stop the algorithm early if we have find obstructions in all directions.
		const AvoidanceInfo& avoidanceInfo0 = avoidanceInfoArray[j];
		const AvoidanceInfo& avoidanceInfo1 = avoidanceInfoArray[Min(j + 1, lastDir)];
		const AvoidanceInfo& avoidanceInfo2 = avoidanceInfoArray[Min(j + 2, lastDir)];
		const AvoidanceInfo& avoidanceInfo3 = avoidanceInfoArray[Min(j + 3, lastDir)];

		// Load up the four directions in vector registers.
		const ScalarV heading0V = LoadScalar32IntoScalarV(avoidanceInfo0.fDirection);
		const ScalarV heading1V = LoadScalar32IntoScalarV(avoidanceInfo1.fDirection);
		const ScalarV heading2V = LoadScalar32IntoScalarV(avoidanceInfo2.fDirection);
		const ScalarV heading3V = LoadScalar32IntoScalarV(avoidanceInfo3.fDirection);

		// Put them together in one vector, and compute the angle we want to compute cos and sin for.
		const Vec4V headingV(heading0V, heading1V, heading2V, heading3V);
		const Vec4V angleV = Subtract(Vec4V(vehDriveOrientationV), headingV);

		// Compute the sin and cos values of the four angles.
		Vec4V sinV, cosV;
		SinAndCos(sinV, cosV, angleV);

		// Use the coordinates of our front bumper, and compute the search positions.
		const Vec4V searchPos1XV = AddScaled(Vec4V(bumperPosV.GetX()), cosV, scaledVehForwardV.GetX());
		const Vec4V searchPos1YV = AddScaled(Vec4V(bumperPosV.GetY()), cosV, scaledVehForwardV.GetY());
		const Vec4V searchPosXV = AddScaled(searchPos1XV, sinV, scaledVehRightV.GetX());
		const Vec4V searchPosYV = AddScaled(searchPos1YV, sinV, scaledVehRightV.GetY());

		// Store the search positions.
		searchPosXArrayVecs[k] = searchPosXV;
		searchPosYArrayVecs[k] = searchPosYV;
	}

	const int iNumNodes = pFollowRoute->GetNumPoints();

#if __BANK
	// This part used to be done for each direction, but it's actually direction-independent.
	if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		for (int i = 0; i < iNumNodes-1 && i < obstructionData.m_RoadSegments.GetCount(); i++)
		{
			const AvoidanceTaskObstructionData::RoadSegment& roadSeg = obstructionData.m_RoadSegments[i];

			if (!roadSeg.m_Valid)
			{
				continue;
			}

			const float bumperZ = bumperPosV.GetZf();

			Vector3 vLeftStart(roadSeg.m_LeftStart.GetX(), roadSeg.m_LeftStart.GetY(), bumperZ + 2.0f);
			Vector3 vLeftEnd(roadSeg.m_LeftEnd.GetX(), roadSeg.m_LeftEnd.GetY(), bumperZ + 2.0f);
			Vector3 vRightStart(roadSeg.m_RightStart.GetX(), roadSeg.m_RightStart.GetY(), bumperZ + 2.0f);
			Vector3 vRightEnd(roadSeg.m_RightEnd.GetX(), roadSeg.m_RightEnd.GetY(), bumperZ + 2.0f);
			//grcDebugDraw::Line(vLeftStart, vLeftEnd, Color_green);
			//grcDebugDraw::Line(vRightStart, vRightEnd, Color_blue);
			CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vLeftStart), VECTOR3_TO_VEC3V(vLeftEnd), Color_green);
			CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vRightStart), VECTOR3_TO_VEC3V(vRightEnd), Color_blue);
		}
	}
#endif

	const Vec4V vVehiclePosX(ScalarV(vVehiclePos2d.GetX()));
	const Vec4V vVehiclePosY(ScalarV(vVehiclePos2d.GetY()));

	const int numVecs = (numDirections + 3)/4;

	const Vec4V negOneV(V_NEGONE);
	const Vec4V zeroV(V_ZERO);

	// Initialize distToRoadEdgeVectorArrayOut, and also an array that we will use for keeping
	// track of which vectors have already hit something, and don't need to be tested further.
	bool resultsFinalized[kMaxVecs];
	for(int i = 0; i < numVecs; i++)
	{
		distToRoadEdgeVectorArrayOut[i] = negOneV;
		resultsFinalized[i] = false;
	}
	int numResultsFinalized = 0;

	for (int i = 0; i < iNumNodes-1 && i < obstructionData.m_RoadSegments.GetCount(); i++)
	{
		const AvoidanceTaskObstructionData::RoadSegment& roadSeg = obstructionData.m_RoadSegments[i];

		if (!roadSeg.m_Valid)
		{
			continue;
		}

		// Load up the road segment coordinates into splatted vector registers, straight from memory.
		const Vec4V leftStartXV(LoadScalar32IntoScalarV(roadSeg.m_LeftStart.GetIntrinConstRef().x));
		const Vec4V leftStartYV(LoadScalar32IntoScalarV(roadSeg.m_LeftStart.GetIntrinConstRef().y));
		const Vec4V leftEndXV(LoadScalar32IntoScalarV(roadSeg.m_LeftEnd.GetIntrinConstRef().x));
		const Vec4V leftEndYV(LoadScalar32IntoScalarV(roadSeg.m_LeftEnd.GetIntrinConstRef().y));
		const Vec4V rightStartXV(LoadScalar32IntoScalarV(roadSeg.m_RightStart.GetIntrinConstRef().x));
		const Vec4V rightStartYV(LoadScalar32IntoScalarV(roadSeg.m_RightStart.GetIntrinConstRef().y));
		const Vec4V rightEndXV(LoadScalar32IntoScalarV(roadSeg.m_RightEnd.GetIntrinConstRef().x));
		const Vec4V rightEndYV(LoadScalar32IntoScalarV(roadSeg.m_RightEnd.GetIntrinConstRef().y));

		// Loop over all the avoidance directions.
		for(int j = 0; j < numVecs; j++)
		{
			// Check if all the four directions in this vector have hit something already.
			// If so, we can skip the test below.
			if(resultsFinalized[j])
			{
				continue;
			}

			// Load the previous distances, and the search positions for these four directions.
			const Vec4V distToRoadEdgeBeforeV = distToRoadEdgeVectorArrayOut[j];
			const Vec4V searchPosXV = searchPosXArrayVecs[j];
			const Vec4V searchPosYV = searchPosYArrayVecs[j];

			// Check which intersections exist already.
			const VecBoolV existingIsectsV = IsGreaterThanOrEqual(distToRoadEdgeBeforeV, zeroV);

			// Test each of the four direction test line segments against the left and right side of this section of the road.
			Vec4V tTestSegmentLeftV;
			Vec4V tTestSegmentRightV;
			const VecBoolV leftIsectV = sTest4x2DSegVsSegOneIsect(tTestSegmentLeftV,
					vVehiclePosX, vVehiclePosY, searchPosXV, searchPosYV,
					leftStartXV, leftStartYV, leftEndXV, leftEndYV);
			const VecBoolV rightIsectV = sTest4x2DSegVsSegOneIsect(tTestSegmentRightV,
					vVehiclePosX, vVehiclePosY, searchPosXV, searchPosYV,
					rightStartXV, rightStartYV, rightEndXV, rightEndYV);

			// Compute the distances to road edges, if they were hits.
			const Vec4V distToCurrentRoadEdgeLeftV = Scale(tTestSegmentLeftV, segmentLengthV);
			const Vec4V distToCurrentRoadEdgeRightV = Scale(tTestSegmentRightV, segmentLengthV);

			// Mask in -1 for the distances that didn't hit.
			const Vec4V newDistToRoadEdgeLeftV = SelectFT(leftIsectV, negOneV, distToCurrentRoadEdgeLeftV);
			const Vec4V newDistToRoadEdgeRightV = SelectFT(rightIsectV, negOneV, distToCurrentRoadEdgeRightV);

			// Update with the left intersections.
			const Vec4V distToRoadEdgeWithLeftV = SelectFT(existingIsectsV, newDistToRoadEdgeLeftV, distToRoadEdgeBeforeV);
			const VecBoolV existingIsectsWithLeftV = Or(existingIsectsV, leftIsectV);

			// Update with the right intersections. Note that left takes precedence for no other reason
			// than preserving existing behavior. Not sure if we ever hit both left and right, and if so,
			// if it would make sense to try to pick the closest intersection or something.
			const Vec4V distToRoadEdgeWithLeftAndRightV = SelectFT(existingIsectsWithLeftV, newDistToRoadEdgeRightV, distToRoadEdgeWithLeftV);

			// Store back the results to the distance to road array.
			distToRoadEdgeVectorArrayOut[j] = distToRoadEdgeWithLeftAndRightV;

			// Probably worth checking if we are done with all four of these directions. If so,
			// we store that as a bool we can check cheaply later, and increase the count in
			// case we can stop completely.
			if(IsGreaterThanAll(distToRoadEdgeWithLeftAndRightV, zeroV))
			{
				resultsFinalized[j] = true;
				numResultsFinalized++;
			}
		}

		// Check if all directions have hit something, in which case we can stop early.
		if(numResultsFinalized == numVecs)
		{
			break;
		}
	}

#if __BANK && 0		// Can be enabled to debug draw the intersection positions.
	if (ms_bEnableNewAvoidanceDebugDraw && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		Vector3 vehDriveDir = RCC_VECTOR3(obstructionData.vNormalizedDriveDir);
		const float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
		//const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		// Use the coordinates of our front bumper.
		Vector3 vBumperPos = RCC_VECTOR3(obstructionData.vBumperCenter);

		for(int i = 0; i < numDirections; i++)
		{
			const float fHeading = avoidanceInfoArray[i].fDirection;

			const float distToRoadEdge = ((float*)distToRoadEdgeVectorArrayOut)[i];
			if(distToRoadEdge < 0.0f)
			{
				continue;
			}

			const Vector3 VehRightVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection()));
			const Vector3 VehForwardVector(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()));
			const Vector3 Offset1 = distToRoadEdge * rage::Cosf(vehDriveOrientation - fHeading) * VehForwardVector;
			const Vector3 Offset2 = distToRoadEdge * rage::Sinf(vehDriveOrientation - fHeading) * VehRightVector;
			const Vector3 vSearchPos = vBumperPos + Offset1 + Offset2;

			grcDebugDraw::Line(vSearchPos, vSearchPos + Vector3(0.0f, 0.0f, 3.0f), Color_yellow);
		}
	}
#endif	// __BANK

	return true;
}

#endif	// !USE_VECTORIZED_FOLLOWROUTE_EDGES_FOR_AVOIDANCE

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Test2MovingRectCollision
// PURPOSE :  Test whether the points of the second rectangle fall within the
//			  first at any point. Returns the time of the first collision(0..1)
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPointWithAvoidanceAutomobile::Test2MovingRectCollision(const CPhysical* const pVeh, const CPhysical* const pOtherVeh, 
												  float OtherSpeedX, float OtherSpeedY,
												  const Vector3& ourNewDriveDir,
												  const Vector3& otherDriveDir,
												  const spdAABB& boundBox,
												  const spdAABB& otherBox,
												  const bool bOtherEntIsDoor)
{
	float	ReturnVal, DistFromCentralLine, ChangeInDist;
	s16	OtherPoints;
	Vector3	LineStart;
	float	MinVal1, MaxVal1, MinVal2, MaxVal2;
	float	CrossVal, MinVal;

// We had to reshuffle the way the arguments are passed. Otherwise the optimiser
// would fuck stuff up. Basically arguments changed during the function call.
// A printf before and after the call resulted in different results.
	Vector3 hisPosn = VEC3V_TO_VECTOR3(pOtherVeh->GetTransform().GetPosition() - pVeh->GetTransform().GetPosition());
	//Vector3 ourBBMax = pVeh->GetBoundingBoxMax();
	//Vector3 vOtherBBMin = pOtherVeh->GetBoundingBoxMin();

	float	OtherCarX = hisPosn.x;
	float	OtherCarY = hisPosn.y;

	const float	OurBBHalfLengthPos = boundBox.GetMax().GetYf();
	const float	OurBBHalfWidth  = boundBox.GetMax().GetXf();
	const float	OurBBHalfWidthNeg = -boundBox.GetMin().GetXf();
	const float	OurBBHalfLengthNeg = -boundBox.GetMin().GetYf();
	float	OtherBBHalfLengthPos = otherBox.GetMax().GetYf();//hisBBMax.y;
	const float	OtherBBHalfWidth = otherBox.GetMax().GetXf();//hisBBMax.x;
	const float	OtherBBHalfWidthNeg = -otherBox.GetMin().GetXf();	//still a positive value to make below code more readable
	//float	OtherBBHalfLengthNeg = -pOther->GetBoundingBoxMin().y;

	if (bOtherEntIsDoor)
	{
		OtherBBHalfLengthPos = rage::Max(ms_fMinLengthForDoors, OtherBBHalfLengthPos);
	}


/*	float	OtherCarX = pOther->GetPosition().x - pVeh->GetPosition().x;
	float	OtherCarY = pOther->GetPosition().y - pVeh->GetPosition().y;

	float	OurBBHalfLengthPos = pVeh->GetBoundingBoxMax().y;
	float	OurBBHalfWidth  = pVeh->GetBoundingBoxMax().x;
	float	OurBBHalfLengthNeg = -pVeh->GetBoundingBoxMin().y;
	float	OtherBBHalfLengthPos = pOther->GetBoundingBoxMax().y;
	float	OtherBBHalfWidth = pOther->GetBoundingBoxMax().x;
//	float	OtherBBHalfLengthNeg = -pOther->GetBoundingBoxMin().y;*/

	// We could add some fast rejection tests here.
	// If the cars are moving away from eachother there is no point in proceeding

	// ReturnVal == LARGE_FLOAT means no collision.
	ReturnVal = LARGE_FLOAT;

	// Front and Side perpendicular so:
	// pOtherSide->x = otherDriveDir.y
	// pOtherSide->y = -otherDriveDir.x

		// NOTE: we now only do the 2 front points of the ai car. We're not really
		// interested in collisions the rear point might cause(we can't do anything
		// about that anyway)

	for(OtherPoints = 0; OtherPoints < 2; OtherPoints++)
	{
		switch(OtherPoints)
		{
			case 0:
				LineStart.x = OtherCarX + OtherBBHalfLengthPos*otherDriveDir.x + OtherBBHalfWidth*otherDriveDir.y;
				LineStart.y = OtherCarY + OtherBBHalfLengthPos*otherDriveDir.y - OtherBBHalfWidth*otherDriveDir.x;
				break;
			case 1:
				LineStart.x = OtherCarX + OtherBBHalfLengthPos*otherDriveDir.x - OtherBBHalfWidthNeg*otherDriveDir.y;
				LineStart.y = OtherCarY + OtherBBHalfLengthPos*otherDriveDir.y + OtherBBHalfWidthNeg*otherDriveDir.x;
				break;
//			case 2:
//				LineStart.x = OtherCarX - OtherBBHalfLengthNeg*otherDriveDir.x + OtherBBHalfWidth*otherDriveDir.y;
//				LineStart.y = OtherCarY - OtherBBHalfLengthNeg*otherDriveDir.y - OtherBBHalfWidthNeg*otherDriveDir.x;
//				break;
//			case 3:
//				LineStart.x = OtherCarX - OtherBBHalfLengthNeg*otherDriveDir.x - OtherBBHalfWidthNeg*otherDriveDir.y;
//				LineStart.y = OtherCarY - OtherBBHalfLengthNeg*otherDriveDir.y + OtherBBHalfWidth*otherDriveDir.x;
//				break;
		}

		// First deal with our line along initialLength of car
		MinVal1 = 0.0f;
		MaxVal1 = 1.0f;
		DistFromCentralLine = LineStart.x * ourNewDriveDir.y - LineStart.y * ourNewDriveDir.x;
		ChangeInDist = OtherSpeedX * ourNewDriveDir.y - OtherSpeedY * ourNewDriveDir.x;

		// Find out when we cross the line
		if(DistFromCentralLine > OurBBHalfWidth)
		{
			if(ChangeInDist < 0.0f)
			{
				CrossVal = -(DistFromCentralLine-OurBBHalfWidth) / ChangeInDist;
				if(CrossVal < 1.0f)
				{
					MinVal1 = CrossVal;
					// We might go out on the other side as well
					CrossVal -=(2.0f*OurBBHalfWidth) / ChangeInDist;
					if(CrossVal < 1.0f)
					{
						MaxVal1 = CrossVal;
					}
				}
				else
				{
					MinVal1 = 1.0f;
				}
			}
			else
			{
				MinVal1 = MaxVal1 = 1.0f;
			}
		}
		else if(DistFromCentralLine < -OurBBHalfWidthNeg)
		{
			if(ChangeInDist > 0.0f)
			{
				CrossVal = -(DistFromCentralLine+OurBBHalfWidthNeg) / ChangeInDist;	// Changed
				if(CrossVal < 1.0f)
				{
					MinVal1 = CrossVal;
					// We might go out on the other side as well
					CrossVal +=(2.0f*OurBBHalfWidthNeg) / ChangeInDist;
					if(CrossVal < 1.0f)
					{
						MaxVal1 = CrossVal;
					}
				}
				else
				{
					MinVal1 = 1.0f;
				}
			}
			else
			{
				MinVal1 = MaxVal1 = 1.0f;
			}
		}
		else		// We start in the middle. Check whether we jump out at some point
		{
			if(ChangeInDist > 0.0f)
			{
				CrossVal =(OurBBHalfWidth - DistFromCentralLine) / ChangeInDist;
				MaxVal1 = CrossVal;
			}
			else if(ChangeInDist < 0.0f)
			{
				CrossVal = -(OurBBHalfWidth + DistFromCentralLine) / ChangeInDist;
				MaxVal1 = CrossVal;
			}
		}

		// Now deal with our line along width of car
		MinVal2 = 0.0f;
		MaxVal2 = 1.0f;
		DistFromCentralLine = LineStart.x * ourNewDriveDir.x + LineStart.y * ourNewDriveDir.y;
		ChangeInDist = OtherSpeedX * ourNewDriveDir.x + OtherSpeedY * ourNewDriveDir.y;

		// Find out when we cross the line
		if(DistFromCentralLine > OurBBHalfLengthPos)
		{
			if(ChangeInDist < 0.0f)
			{
				CrossVal = -(DistFromCentralLine-OurBBHalfLengthPos) / ChangeInDist;
				if(CrossVal < 1.0f)
				{
					MinVal2 = CrossVal;
					// We might go out on the other side as well
					CrossVal -=(OurBBHalfLengthPos + OurBBHalfLengthNeg) / ChangeInDist;
					if(CrossVal < 1.0f)
					{
						MaxVal2 = CrossVal;
					}
				}
				else
				{
					MinVal2 = 1.0f;
				}
			}
			else
			{
				MinVal2 = MaxVal2 = 1.0f;
			}
		}
		else if(DistFromCentralLine < -OurBBHalfLengthNeg)
		{
			if(ChangeInDist > 0.0f)
			{
				CrossVal = -(DistFromCentralLine+OurBBHalfLengthNeg) / ChangeInDist;	// changed
				if(CrossVal < 1.0f)
				{
					MinVal2 = CrossVal;
					// We might go out on the other side as well
					CrossVal +=(OurBBHalfLengthPos + OurBBHalfLengthNeg) / ChangeInDist;
					if(CrossVal < 1.0f)
					{
						MaxVal2 = CrossVal;
					}
				}
				else
				{
					MinVal2 = 1.0f;
				}
			}
			else
			{
				MinVal2 = MaxVal2 = 1.0f;
			}
		}
		else		// We start in the middle. Check whether we jump out at some point
		{
			if(ChangeInDist > 0.0f)
			{
				CrossVal =(OurBBHalfLengthPos - DistFromCentralLine) / ChangeInDist;
				MaxVal2 = CrossVal;
			}
			else if(ChangeInDist < 0.0f)
			{
				CrossVal = -(OurBBHalfLengthNeg + DistFromCentralLine) / ChangeInDist;
				MaxVal2 = CrossVal;
			}
		}

		// Right. We now have two intervals(MinVal1, MaxVal1) and(MinVal2, MaxVal2).
		// The first time both are set is the time value we collide.
		MinVal = rage::Max(MinVal1, MinVal2);
		if(MinVal < MaxVal1 && MinVal < MaxVal2)
		{		// We found a collision here.
			ReturnVal = rage::Min(ReturnVal, MinVal);
		}
	}

	return(ReturnVal);
}

#if !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TestForThisAngle
// PURPOSE :  For a specific angle(direction) we test whether the given 2 lines and 3 points collide.
// RETURNS :  true for a collision.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TestForThisAngle(float Angle, Vector3 *pLine1Start, Vector3 *pLine1Delta, Vector3 *pLine2Start, Vector3 *pLine2Delta, Vector3 *pLine1Start_Other, Vector3 *pLine1Delta_Other, Vector3 *pLine2Start_Other, Vector3 *pLine2Delta_Other, float OtherCarMoveSpeedX, float OtherCarMoveSpeedY, float OurMoveSpeed, bool bSwapRound, float fLookaheadTime) const
{
	Vector3	Coors1, Coors2, Coors3, Coors4;

	float	OurSpeedX = OurMoveSpeed * rage::Cosf(Angle);
	float	OurSpeedY = OurMoveSpeed * rage::Sinf(Angle);

	// These speeds are such that the player car doesn't move and the other one does all the movement.
	float SpeedX_Combined = OtherCarMoveSpeedX - OurSpeedX;
	float SpeedY_Combined = OtherCarMoveSpeedY - OurSpeedY;

	// Work out the vehToNewNodeDelta vector for this line(combined speeds * time).
	Vector3 DeltaVec = Vector3(SpeedX_Combined, SpeedY_Combined, 0.0f) * fLookaheadTime;	// look 1.7 seconds ahead

	TUNE_GROUP_BOOL(FOLLOW_PATH_AI_STEER, bAlwaysSwapRound, true);
	if(bSwapRound || bAlwaysSwapRound)
	{
		Vector3	*pTemp;

		pTemp = pLine1Start;
		pLine1Start = pLine1Start_Other;
		pLine1Start_Other = pTemp;

		pTemp = pLine1Delta;
		pLine1Delta = pLine1Delta_Other;
		pLine1Delta_Other = pTemp;

		pTemp = pLine2Start;
		pLine2Start = pLine2Start_Other;
		pLine2Start_Other = pTemp;

		pTemp = pLine2Delta;
		pLine2Delta = pLine2Delta_Other;
		pLine2Delta_Other = pTemp;

		DeltaVec = -DeltaVec;
	}

	// The 2 lines of the other car both turn into 4(a rectangle) because of the speed.
	Coors1 = *pLine1Start_Other;
	Coors2 = *pLine1Start_Other + *pLine1Delta_Other;
	Coors3 = Coors1 + DeltaVec;
	Coors4 = Coors2 + DeltaVec;

	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;

	if (CPathFind::HelperIsPointWithinArbitraryArea(pLine1Start->x, pLine1Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y)
		|| CPathFind::HelperIsPointWithinArbitraryArea(pLine2Start->x, pLine2Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y))
	{
		return true;
	}

	Coors1 = *pLine2Start_Other;
	Coors2 = *pLine2Start_Other + *pLine2Delta_Other;
	Coors3 = Coors1 + DeltaVec;
	Coors4 = Coors2 + DeltaVec;
	
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;

	if (CPathFind::HelperIsPointWithinArbitraryArea(pLine1Start->x, pLine1Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y)
		|| CPathFind::HelperIsPointWithinArbitraryArea(pLine2Start->x, pLine2Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y))
	{
		return true;
	}

	return false;
}

#endif	// !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS


// PURPOSE:	Helper function to check if (each component of) two vectors have the same sign.
// PARAMS:	valAV	- First vector.
//			valBV	- Second vector.
// RETURNS:	True for each component where the two vectors have the same sign.
static __forceinline VecBoolV sSameSign(Vec4V_In valAV, Vec4V_In valBV)
{
	// XOR the two vectors together, then keep just the sign bit of that,
	// i.e. the XOR of the sign bits.
	const Vec4V signBitXorV = And(Xor(valAV, valBV), Vec4V(V_80000000));

	// Convert to a true bool vector. Note that we need to use IsEqualInt(),
	// not IsEqual(), because 0x80000000 is actually the negative zero,
	// which is also zero when compared as a float...
	const VecBoolV sameSignV = IsEqualInt(signBitXorV, Vec4V(V_ZERO));

	// Note: not sure that this is really important, but the original code
	// essentially did this:
	//	return valAV*valBV > 0.0f
	// i.e. if one of the values was zero, we would return false. We could do this
	// if we really need that behavior:
	//	const VecBoolV isZeroAV = IsZero(valAV);
	//	const VecBoolV isZeroBV = IsZero(valBV);
	//	const VecBoolV eitherZeroV = Or(isZeroAV, isZeroBV);
	//
	//	// Return true if they were both the same sign, using the XOR of the sign bit,
	//	// and neither vector was zero.
	//	return Andc(sameSignV, eitherZeroV);

	return sameSignV;
}

// PURPOSE:	Helper function used as a part of CTaskVehicleGoToPointWithAvoidanceAutomobile::TestForThisAngleNew(),
//			for testing intersections between 2D line segments. Four pairs of line segments are tested
//			by a single call to the function.
// PARAMS:	start1XV		- X coordinates of the starting points of the first segment in each pair.
//			start1YV		- Y coordinates of the starting points of the first segment in each pair.
//			delta1XV		- X coordinate of the first segment delta in each pair.
//			delta1YV		- Y coordinate of the first segment delta in each pair.
//			start2XV		- X coordinates of the starting points of the second segment in each pair.
//			start2YV		- Y coordinates of the starting points of the second segment in each pair.
//			delta2XV		- X coordinate of the second segment delta in each pair.
//			delta2YV		- Y coordinate of the second segment delta in each pair.
// RETURNS:	Vector where each element is true if the corresponding pair of segments intersects.
// NOTES:	The function works by finding out whether for line 1 both points of line 2 are on separate
//			side of the line and it will do the same thing for the other line.
static __forceinline VecBoolV sTest2DLinesAgainst2DLines(
		Vec4V_In start1XV, Vec4V_In start1YV, Vec4V_In delta1XV, Vec4V_In delta1YV,
		Vec4V_In start2XV, Vec4V_In start2YV, Vec4V_In delta2XV, Vec4V_In delta2YV)
{
	// Compute the delta between the start positions of the segments in each pair.
	const Vec4V start1To2XV = Subtract(start2XV, start1XV);
	const Vec4V start1To2YV = Subtract(start2YV, start1YV);

	// Compute some dot products.
	// This was all adapted from fwGeom::Test2DLineAgainst2DLine()
	//	float	DotPr1 = (Start2X - Start1X) * Delta1Y - (Start2Y - Start1Y) * Delta1X;
	//	float	DotPr2 = (Start2X - Start1X + Delta2X) * Delta1Y - (Start2Y - Start1Y + Delta2Y) * Delta1X;
	//	if (DotPr1 * DotPr2 > 0.0f) return false;
	//	DotPr1 = (Start1X - Start2X) * Delta2Y - (Start1Y - Start2Y) * Delta2X;
	//	DotPr2 = (Start1X - Start2X + Delta1X) * Delta2Y - (Start1Y - Start2Y + Delta1Y) * Delta2X;
	//	if (DotPr1 * DotPr2 > 0.0f) return false;
	//	return true;
	const Vec4V startDeltaTimesDeltaXV = Scale(start1To2XV, delta1YV);
	const Vec4V startDeltaTimesDeltaYV = Scale(start1To2YV, delta2XV);
	const Vec4V tmp1V = Scale(delta1XV, delta2YV);
	const Vec4V dotPr1V = SubtractScaled(startDeltaTimesDeltaXV, start1To2YV, delta1XV);
	const Vec4V dotPr3V = SubtractScaled(startDeltaTimesDeltaYV, start1To2XV, delta2YV);
	const Vec4V tmpV = SubtractScaled(tmp1V, delta1YV, delta2XV);
	const Vec4V dotPr2V = Subtract(dotPr1V, tmpV);
	const Vec4V dotPr4V = Add(dotPr3V, tmpV);

	// Check if the dot products have the same sign, meaning that the two end points
	// of one segment is on the same side of the infinite line through the other segment,
	// and vice versa.
	const VecBoolV sameSign12V = sSameSign(dotPr1V, dotPr2V);
	const VecBoolV sameSign34V = sSameSign(dotPr3V, dotPr4V);

	// If the signs are different from both directions, the segments intersect.
	return InvertBits(Or(sameSign12V, sameSign34V));
}

// PURPOSE:	Helper function used as a part of CTaskVehicleGoToPointWithAvoidanceAutomobile::TestForThisAngleNew(),
//			for testing one point against a convex 2D quad.
// PARAMS:	testPointXV - X coordinate of the point to test.
//			testPointYV - Y coordinate of the point to test.
//			quadVertsXV - X coordinates of the quad vertices, in CW or perhaps CCW order(?).
//			quadVertsYV - Y coordinates of the quad vertices.
//			quadDeltaXV	- X deltas of the edges of the quad vertices. Redundant given the vertex coordinates, but passed in since the caller code is already computing it.
//			quadDeltaYV	- Y deltas of the edges of the quad vertices.
// RETURNS:	A bool vector that is true if the point was within the quad.
// NOTES:	This was adapted from CPathFind::HelperIsPointWithinArbitraryArea, but uses a different
//			ordering of the vertices.
static __forceinline BoolV_Out sHelperIsPointWithinArbitraryArea(
		ScalarV_In testPointXV, ScalarV_In testPointYV,
		Vec4V_In quadVertsXV, Vec4V_In quadVertsYV,
		Vec4V_In quadDeltaXV, Vec4V_In quadDeltaYV)
{
	// Note: quadDeltaXV and quadDeltaYV should be computed like this:
	//	const Vec4V quadVertsShiftedXV = quadVertsXV.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();
	//	const Vec4V quadVertsShiftedYV = quadVertsYV.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();
	//	const Vec4V quadDeltaXV = Subtract(quadVertsShiftedXV, quadVertsXV);
	//	const Vec4V quadDeltaYV = Subtract(quadVertsShiftedYV, quadVertsYV);

	// Compute deltas between the test point and all quad vertices.
	const Vec4V testDeltaXV = Subtract(Vec4V(testPointXV), quadVertsXV);
	const Vec4V testDeltaYV = Subtract(Vec4V(testPointYV), quadVertsYV);

	// Basically compute a dot product between the (non-normalized) normal
	// of each edge and the vector between the test point and one of its vertices.
	const Vec4V temp1V = Scale(testDeltaXV, quadDeltaYV);
	const Vec4V temp2V = SubtractScaled(temp1V, testDeltaYV, quadDeltaXV);

	// Use the dot products to check if we are inside each edge of the polygon.
	const VecBoolV withinEdgesV = IsGreaterThanOrEqual(temp2V, Vec4V(V_ZERO));

	// We are interested in knowing if we are inside all of the edges, so we need
	// to AND together the components:
	const VecBoolV withinEdgesShifted1V = withinEdgesV.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();
	const VecBoolV combined1V = And(withinEdgesV, withinEdgesShifted1V);
	const VecBoolV combined1Shifted2V = combined1V.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
	const VecBoolV combinedV = And(combined1V, combined1Shifted2V);

	// Return the result.
	return combinedV.GetX();
}	

// PURPOSE:	Helper function used by CTaskVehicleGoToPointWithAvoidanceAutomobile::TestForThisAngle(),
//			basically testing a 2D polygon formed by extruding a line segment along a vector,
//			against two other line segments.
// PARAMS:	quadLineStartV		- Starting coordinates of the line which will be extruded into a polygon.
//			quadLineDeltaV		- Delta of the line to extrude, to get to its end point from its start.
//			deltaVecV			- The vector we will be extruding by.
//			line1StartV			- Starting point of the first line.
//			line1DeltaV			- Delta of the first line.
//			line2StartV			- Starting point of the second line.
//			line2DeltaV			- Delta of the second line.
// RETURNS:	True if any of the lines intersect an edge of the polygon, or if their starting point
//			is inside of it.
#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
static bool sTestForThisAngleHelper(Vec2V_In quadLineStartV, Vec2V_In quadLineDeltaV, Vec2V_In deltaVecV,
		Vec2V_In line1StartV, Vec2V_In line1DeltaV, Vec2V_In line2StartV, Vec2V_In line2DeltaV)
{
	// This function is basically equivalent to what this old code was doing:
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) return true;
	//	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) return true;
	//	if (CPathFind::HelperIsPointWithinArbitraryArea(pLine1Start->x, pLine1Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y)
	//		|| CPathFind::HelperIsPointWithinArbitraryArea(pLine2Start->x, pLine2Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y))
	//	{
	//		return true;
	//	}
	//	return false;

	// Compute the vertices of the polygon.
	const Vec2V coors1V = quadLineStartV;
	const Vec2V coors2V = Add(coors1V, quadLineDeltaV);
	const Vec2V coors3V = Add(coors1V, deltaVecV);
	const Vec2V coors4V = Add(coors2V, deltaVecV);

	// Transpose to SoA vector format, equivalent to this:
	//	const Vec4V coorsAXV(coors1V.GetX(), coors3V.GetX(), coors4V.GetX(), coors2V.GetX());
	//	const Vec4V coorsAYV(coors1V.GetY(), coors3V.GetY(), coors4V.GetY(), coors2V.GetY());
	// Note that the order here is shifted a bit, so that the vertices are in order of
	// the edges when going around the polygon.
	const Vec4V temp0AV = MergeXY(coors1V, coors4V);
	const Vec4V temp1AV = MergeXY(coors3V, coors2V);
	const Vec4V coorsAXV = MergeXY(temp0AV, temp1AV);
	const Vec4V coorsAYV = MergeZW(temp0AV, temp1AV);

	// Splat the X and Y coordinates of the lines to test against the polygon.
	const ScalarV line1StartXV(line1StartV.GetX());
	const ScalarV line1StartYV(line1StartV.GetY());
	const ScalarV line2StartXV(line2StartV.GetX());
	const ScalarV line2StartYV(line2StartV.GetY());
	const ScalarV line1DeltaXV(line1DeltaV.GetX());
	const ScalarV line1DeltaYV(line1DeltaV.GetY());
	const ScalarV line2DeltaXV(line2DeltaV.GetX());
	const ScalarV line2DeltaYV(line2DeltaV.GetY());

	// Rotate the coordinates by one element.
	const Vec4V coorsBXV = coorsAXV.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();
	const Vec4V coorsBYV = coorsAYV.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();

	// Subtract to compute the deltas of all edges of the polygon.
	const Vec4V deltaAToBXV = Subtract(coorsBXV, coorsAXV);
	const Vec4V deltaAToBYV = Subtract(coorsBYV, coorsAYV);

	// Check for intersections between the first line and all edges of the polygon.
	const VecBoolV lineTestResults1V = sTest2DLinesAgainst2DLines(
			Vec4V(line1StartXV), Vec4V(line1StartYV), Vec4V(line1DeltaXV), Vec4V(line1DeltaYV),
			coorsAXV, coorsAYV, deltaAToBXV, deltaAToBYV);

	// Check for intersections between the second line and all edges of the polygon.
	const VecBoolV lineTestResults2V = sTest2DLinesAgainst2DLines(
			Vec4V(line2StartXV), Vec4V(line2StartYV), Vec4V(line2DeltaXV), Vec4V(line2DeltaYV),
			coorsAXV, coorsAYV, deltaAToBXV, deltaAToBYV);

	// Check if the first line's starting point is inside of the polygon.
	const BoolV inArea1V = sHelperIsPointWithinArbitraryArea(line1StartXV, line1StartYV, coorsAXV, coorsAYV, deltaAToBXV, deltaAToBYV);

	// Check if the second line's starting point is inside of the polygon.
	const BoolV inArea2V = sHelperIsPointWithinArbitraryArea(line2StartXV, line2StartYV, coorsAXV, coorsAYV, deltaAToBXV, deltaAToBYV);

	// Combine the result and convert to a boolean value.
	// Note: we could keep this as a vector, but may be worth branching at this point.
	return !IsFalseAll(Or(Or(lineTestResults1V, lineTestResults2V), VecBoolV(Or(inArea1V, inArea2V))));
}
#endif

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

bool CTaskVehicleGoToPointWithAvoidanceAutomobile::TestForThisAngleNew(Vec2V_In dirXYV, Vec2V_In line1StartOtherV, Vec2V_In line1DeltaOtherV, Vec2V_In line2StartOtherV, Vec2V_In line2DeltaOtherV, Vec2V_In line1StartV, Vec2V_In line1DeltaV, Vec2V_In line2StartV, Vec2V_In line2DeltaV, float OtherCarMoveSpeedX, float OtherCarMoveSpeedY, const float& OurMoveSpeed, const float& fLookaheadTime) const
{
	// Load the floats as scalars.
	const ScalarV ourMoveSpeedV = LoadScalar32IntoScalarV(OurMoveSpeed);
	const ScalarV lookAheadTimeV = LoadScalar32IntoScalarV(fLookaheadTime);

	// Compute the relative velocity between the vehicles if we move in this direction,
	// and compute a delta vector representing the relative movement during the lookahead time.
	const Vec2V ourSpeedV = Scale(ourMoveSpeedV, dirXYV);
	const Vec2V otherCarMoveSpeedV(OtherCarMoveSpeedX, OtherCarMoveSpeedY);
	const Vec2V speedCombinedNegV = Subtract(ourSpeedV, otherCarMoveSpeedV);
	const Vec2V deltaVecV = Scale(speedCombinedNegV, lookAheadTimeV);

// This can be enabled to verify the results against the old algorithm.
#if __ASSERT && 0
	float	OurSpeedX = OurMoveSpeed * rage::Cosf(Angle);
	float	OurSpeedY = OurMoveSpeed * rage::Sinf(Angle);

	// These speeds are such that the player car doesn't move and the other one does all the movement.
	float SpeedX_Combined = OtherCarMoveSpeedX - OurSpeedX;
	float SpeedY_Combined = OtherCarMoveSpeedY - OurSpeedY;

	// Work out the vehToNewNodeDelta vector for this line(combined speeds * time).
	Vector3 DeltaVec = Vector3(SpeedX_Combined, SpeedY_Combined, 0.0f) * fLookaheadTime;	// look 1.7 seconds ahead

	TUNE_GROUP_BOOL(FOLLOW_PATH_AI_STEER, bAlwaysSwapRound, true);
	if(/*bSwapRound ||*/ bAlwaysSwapRound)
	{
		Vector3	*pTemp;

		pTemp = pLine1Start;
		pLine1Start = pLine1Start_Other;
		pLine1Start_Other = pTemp;

		pTemp = pLine1Delta;
		pLine1Delta = pLine1Delta_Other;
		pLine1Delta_Other = pTemp;

		pTemp = pLine2Start;
		pLine2Start = pLine2Start_Other;
		pLine2Start_Other = pTemp;

		pTemp = pLine2Delta;
		pLine2Delta = pLine2Delta_Other;
		pLine2Delta_Other = pTemp;

		DeltaVec = -DeltaVec;
	}

	// The 2 lines of the other car both turn into 4(a rectangle) because of the speed.
	const Vector3 Coors1 = *pLine1Start_Other;
	const Vector3 Coors2 = *pLine1Start_Other + *pLine1Delta_Other;
	const Vector3 Coors3 = Coors1 + DeltaVec;
	const Vector3 Coors4 = Coors2 + DeltaVec;


	bool ret = false;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors2.x - Coors1.x, Coors2.y - Coors1.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors1.x, Coors1.y, Coors3.x - Coors1.x, Coors3.y - Coors1.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors3.x, Coors3.y, Coors4.x - Coors3.x, Coors4.y - Coors3.y)) { ret = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors4.x, Coors4.y, Coors2.x - Coors4.x, Coors2.y - Coors4.y)) { ret = true; }

	//Assert(ret == retNew);

	bool inArea1Old = CPathFind::HelperIsPointWithinArbitraryArea(pLine1Start->x, pLine1Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y);
	bool inArea2Old = CPathFind::HelperIsPointWithinArbitraryArea(pLine2Start->x, pLine2Start->y, Coors1.x, Coors1.y, Coors2.x, Coors2.y, Coors3.x, Coors3.y, Coors4.x, Coors4.y);

	//Assert(inArea1 == inArea1Old);
	//Assert(inArea2 == inArea2Old);

	ret = ret | inArea1Old | inArea2Old;

	const Vector3 Coors5 = *pLine2Start_Other;
	const Vector3 Coors6 = *pLine2Start_Other + *pLine2Delta_Other;
	const Vector3 Coors7 = Coors5 + DeltaVec;
	const Vector3 Coors8 = Coors6 + DeltaVec;

	bool ret2 = false;
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors5.x, Coors5.y, Coors6.x - Coors5.x, Coors6.y - Coors5.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors5.x, Coors5.y, Coors7.x - Coors5.x, Coors7.y - Coors5.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors7.x, Coors7.y, Coors8.x - Coors7.x, Coors8.y - Coors7.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine1Start->x, pLine1Start->y, pLine1Delta->x, pLine1Delta->y, Coors8.x, Coors8.y, Coors6.x - Coors8.x, Coors6.y - Coors8.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors5.x, Coors5.y, Coors6.x - Coors5.x, Coors6.y - Coors5.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors5.x, Coors5.y, Coors7.x - Coors5.x, Coors7.y - Coors5.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors7.x, Coors7.y, Coors8.x - Coors7.x, Coors8.y - Coors7.y)) { ret2 = true; }
	if(fwGeom::Test2DLineAgainst2DLine(pLine2Start->x, pLine2Start->y, pLine2Delta->x, pLine2Delta->y, Coors8.x, Coors8.y, Coors6.x - Coors8.x, Coors6.y - Coors8.y)) { ret2 = true; }

	//Assert(ret2 == retNew2);

	bool inArea3Old = CPathFind::HelperIsPointWithinArbitraryArea(pLine1Start->x, pLine1Start->y, Coors5.x, Coors5.y, Coors6.x, Coors6.y, Coors7.x, Coors7.y, Coors8.x, Coors8.y);
	bool inArea4Old = CPathFind::HelperIsPointWithinArbitraryArea(pLine2Start->x, pLine2Start->y, Coors5.x, Coors5.y, Coors6.x, Coors6.y, Coors7.x, Coors7.y, Coors8.x, Coors8.y);

	//Assert(inArea3 == inArea3Old);
	//Assert(inArea4 == inArea4Old);

	//bool finalRetOld = ret || ret2 || inArea1Old || inArea2Old || inArea3Old || inArea4Old;
	//Assert(finalRetOld == !IsFalseAll(combinedV));
	//return !IsFalseAll(combinedV);

	ret2 = ret2 | inArea3Old | inArea4Old;

	bool retNew1 = sTestForThisAngleHelper(line1StartOtherV, line1DeltaOtherV, deltaVecV, line1StartV, line1DeltaV, line2StartV, line2DeltaV);
	bool retNew2 = sTestForThisAngleHelper(line2StartOtherV, line2DeltaOtherV, deltaVecV, line1StartV, line1DeltaV, line2StartV, line2DeltaV);
	Assert(ret == retNew1);
	Assert(ret2 == retNew2);

#endif	// __ASSERT

	// Use the helper function to perform the tests.
	// Note that we could consider using a vector bool to store the results and hold off on the branch,
	// but it looks like there is a large enough chance of intersecting during the first test that
	// it's probably worth the branch to avoid doing the second test sometimes.
	if(sTestForThisAngleHelper(line1StartOtherV, line1DeltaOtherV, deltaVecV, line1StartV, line1DeltaV, line2StartV, line2DeltaV))
	{
		return true;
	}
	if(sTestForThisAngleHelper(line2StartOtherV, line2DeltaOtherV, deltaVecV, line1StartV, line1DeltaV, line2StartV, line2DeltaV))
	{
		return true;
	}

	return false;
}

#endif	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

/////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleGoToPointWithAvoidanceAutomobile::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_GoToPoint&&iState<=State_Swerve);
	static const char* aStateNames[] = 
	{
		"State_GoToPoint",
		"State_ThreePointTurn",
		"State_WaitForTraffic",
		"State_WaitForPed",
		"State_TemporarilyBrake",
		"State_Swerve",
	};

	return aStateNames[iState];
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CTaskVehicleGoToPointAutomobile
/////////////////////////////////////////////////////////////////////////////////


CTaskVehicleGoToPointAutomobile::CTaskVehicleGoToPointAutomobile( const sVehicleMissionParams& params )
: CTaskVehicleGoTo(params)
, m_fMaxThrottleFromTraction(1.0f)
, m_bCanUseHandBrakeForTurns(true)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE);
}

/////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToPointAutomobile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_GoToPoint)
			FSM_OnEnter
				GoToPoint_OnEnter(pVehicle);
			FSM_OnUpdate
				return GoToPoint_OnUpdate(pVehicle);
	FSM_End
}


/////////////////////////////////////////////////////////////////////////////////
// State_GoTo
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointAutomobile::GoToPoint_OnEnter(CVehicle* pVehicle)
{
	pVehicle->SwitchEngineOn();
	// Update the local copy of the controls so its the same as the vehicles
	m_vehicleControls.SetSteerAngle(pVehicle->GetSteerAngle());
	m_vehicleControls.SetThrottle(pVehicle->GetThrottle());
	m_vehicleControls.SetBrake(pVehicle->GetBrake());
	m_vehicleControls.SetHandBrake(pVehicle->GetHandBrake());
}

EXT_PF_TIMER(VehicleGoToTask_Run);

aiTask::FSM_Return CTaskVehicleGoToPointAutomobile::GoToPoint_OnUpdate(CVehicle* pVehicle)
{
	aiDeferredTasks::TaskData taskData(this, pVehicle, GetTimeStep(), true);

#if !__PS3
	TUNE_GROUP_BOOL(AI_DEFERRED_TASKS, USE_DEFERRED_GOTO, true);

	if(USE_DEFERRED_GOTO)
	{
		aiDeferredTasks::g_VehicleGoTo.Queue(taskData);
	}
	else
	{
#endif
		PF_START(VehicleGoToTask_Run);
		GoToPoint_OnDeferredTask(taskData);
		PF_STOP(VehicleGoToTask_Run);
#if !__PS3
	}
#endif

	return FSM_Continue;
}

void CTaskVehicleGoToPointAutomobile::GoToPoint_OnDeferredTask(const aiDeferredTasks::TaskData& data)
{
	CVehicle* pVehicle = data.m_Vehicle;
	const float& fTimeStep = data.m_TimeStep;

	float	dirToTargetOrientation, desiredSteerAngle;
	Vector3	SpeedVector;
	Vector3	Side, PlayerSpeed, Front;

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
	{
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(1.0f,0.0f,0.0f),m_Params.GetTargetPosition() + Vector3(1.0f,0.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,1.0f,0.0f),m_Params.GetTargetPosition() + Vector3(0.0f,1.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,0.0f,1.0f),m_Params.GetTargetPosition() + Vector3(0.0f,0.0f,1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
	}
#endif

	// Get the current drive direction and orientation.
	Vec3f vehForward;
	LoadAsScalar(vehForward, pVehicle->GetVehicleForwardDirectionRef());
	Vec3f vehDriveDir = IsDrivingFlagSet(DF_DriveInReverse) ? -vehForward : vehForward;

	const float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.GetX(), vehDriveDir.GetY());

	// Work out the proper steering angle.
	// To make things a little bit more stable we aim ahead a little bit.
	//	TargetPoint_Ahead = TargetPoint + pTargetCar->GetB() * 3.0f;
	Vec3f vecVehiclePosition;
	LoadAsScalar(vecVehiclePosition, pVehicle->GetMatrixRef().GetCol3ConstRef());
	dirToTargetOrientation = fwAngle::GetATanOfXY(m_Params.GetTargetPosition().x - vecVehiclePosition.GetX(), m_Params.GetTargetPosition().y - vecVehiclePosition.GetY());

	desiredSteerAngle = SubtractAngleShorterFast(dirToTargetOrientation, vehDriveOrientation);
	desiredSteerAngle = IsDrivingFlagSet(DF_DriveInReverse) ? -desiredSteerAngle : desiredSteerAngle;

	// Clamp the steer angle to the vehicles ranges and adjust the gas
	// and brake pedal input to try to achieve the desired speed (taking
	// into account slopes).
	float desiredSpeed =   GetCruiseSpeed();

	//if we want to drive in reverse, ensure we have a negative speed.
	//if we want to drive forward, ensure we have a positive speed
#if __BANK
	if (desiredSpeed < 0.0f)
	{
		pVehicle->GetIntelligence()->PrintTasks();
	}
#endif //__BANK
	taskAssertf(desiredSpeed >= 0.0f, "Negative cruise speed %.2f used for a task unsupported. To drive in reverse, use the DF_DriveInReverse flag", desiredSpeed);

	if (IsDrivingFlagSet(DF_DriveInReverse))
	{
		desiredSpeed = -fabsf(desiredSpeed);
	}
	else
	{
		desiredSpeed = fabsf(desiredSpeed);
	}

	bool bIsInVehicleChase = (pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsInVehicleChase));
	bool bIsSteerIntoSkidsFlagSet = (pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_SteerIntoSkids));
	bool bWantsToSteerIntoSkids = (bIsInVehicleChase || bIsSteerIntoSkidsFlagSet);

#if __BANK
	TUNE_GROUP_BOOL(CAR_AI, bForceSteerIntoSkids, false);
	if(bForceSteerIntoSkids)
	{
		bWantsToSteerIntoSkids = true;
	}
#endif
	
	bool bSteerIntoSkids = (!pVehicle->m_nVehicleFlags.bIsDoingHandBrakeTurn && bWantsToSteerIntoSkids);
	if(bSteerIntoSkids)
	{
		TUNE_GROUP_FLOAT(CAR_AI, sfSideSlipSteerInfluence, -0.4f, -10.0f, 10.0f, 0.1f);
		float fSideSlip = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()), pVehicle->GetAiVelocity());
		fSideSlip /= Abs(DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()), pVehicle->GetAiVelocity())) + 1.0f;
		desiredSteerAngle += fSideSlip * sfSideSlipSteerInfluence;
	}

	//copy this over to the stack so we aren't constantly fetching it from memory.
	//it gets hammered a lot in AdjustControls and HumaniseCarControlInput
	CVehControls vehControls;
	vehControls.Copy(m_vehicleControls);

	AdjustControls(pVehicle, &vehControls, desiredSteerAngle, desiredSpeed);
	
	const float fCurrentVelocitySqr = pVehicle->GetAiVelocity().Mag2();

	//Check if we can use the handbrake for turns.
	bool bWasUsingHandBrake = vehControls.m_handBrake;
	if(m_bCanUseHandBrakeForTurns)
	{
		// If we're going very fast we use the handbrake to turn
		static dev_float s_fVelThresholdForModerateHandbrakeTurns = 20.0f;
		static dev_float s_fVelThresholdForExtremeHandbrakeTurns = 10.0f;
		if(fCurrentVelocitySqr > (s_fVelThresholdForModerateHandbrakeTurns * s_fVelThresholdForModerateHandbrakeTurns) && ABS(desiredSteerAngle) > 0.7f)
		{
			//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, Color_yellow, false);

			vehControls.m_handBrake = true;
		}
		else if (fCurrentVelocitySqr > (s_fVelThresholdForExtremeHandbrakeTurns * s_fVelThresholdForExtremeHandbrakeTurns) 
			&& ABS(desiredSteerAngle) > HALF_PI)
		{
			//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, Color_red, false);

			vehControls.m_handBrake = true;
		}
	}
	pVehicle->m_nVehicleFlags.bIsDoingHandBrakeTurn = (!bWasUsingHandBrake && vehControls.m_handBrake);

// 	TUNE_GROUP_BOOL(ASDF, EverybodyVeerRight, false);
// 	TUNE_GROUP_BOOL(ASDF, EverybodyVeerLeft, false);
// 	if (EverybodyVeerRight)
// 	{
// 		vehControls.m_steerAngle = -1.0f;
// 	}
// 	else if (EverybodyVeerLeft)
// 	{
// 		vehControls.m_steerAngle = 1.0f;
// 	}

	// The values we found may be completely different from the ones last frame.
	// The following function smooths out the values and in some cases clips them.
	HumaniseCarControlInput( pVehicle, &vehControls, pVehicle->GetIntelligence()->GetHumaniseControls(), (fCurrentVelocitySqr <(5.0f*5.0f)), fTimeStep, desiredSpeed);

	m_vehicleControls.Copy(vehControls);
	
	if ( pVehicle->InheritsFromPlane() )
	{
		static bool s_enable_plane_controls = true;
		if ( s_enable_plane_controls )
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			pPlane->SetThrottleControl(m_vehicleControls.GetThrottle());

			// turn off wing lift so we don't take off
			pPlane->SetVirtualSpeedControl(0.0f);

			static dev_float s_OrientationDeltaToTurnMax = 10.0f;
			float fOrientationDeltaScaled = desiredSteerAngle / ( s_OrientationDeltaToTurnMax * DtoR );

			const float fYawControl = -pPlane->GetPlaneIntelligence()->GetYawController().Update(fOrientationDeltaScaled);
			const float fYawControlClamped = Clamp(fYawControl, -5.0f, 5.0f);
			pPlane->SetYawControl(fYawControlClamped);

		}
	}

	//not sure how this could become > 1.0f, changing the compare value for now
	//since I'm not sure yet if it's a bug
	//Assertf(pVehicle->GetSteerAngle() > -1.1f && pVehicle->GetSteerAngle() < 1.1f, "Steer angle out of range: %.2f", pVehicle->GetSteerAngle());
}

/////////////////////////////////////////////////////////////////////////////////
// AdjustControls
// Clamp the steer angle to the vehicles ranges and adjust the gas
// and brake pedal input to try to achieve the desired speed (taking
// into account slopes).
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPointAutomobile::AdjustControls(CVehicle* pVehicle, CVehControls* pVehControls, const float desiredSteerAngle, const float desiredSpeed)
{
	Assertf(pVehicle, "AdjustControls expected a valid set vehicle.");
	Assertf(pVehControls, "AdjustControls expected a valid set of vehicle controls.");

	CVehControls vehControls;
	vehControls.Copy(*pVehControls);

#if __DEV
	if(CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
	{
		TUNE_GROUP_BOOL(CAR_AI, showSteerDir, false);
		if(showSteerDir)
		{
			// Get the current drive direction and orientation.
			const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
			const float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);

			TUNE_GROUP_FLOAT(CAR_AI, alpha, 1.0f, 0.0f, 1.0f, 0.01f);
			Color32 col(0.0f, 1.0f, 0.0f, alpha);
			Vector3 startPos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
			Vector3 endPos = startPos +(Vector3(rage::Cosf(desiredSteerAngle + vehDriveOrientation), rage::Sinf(desiredSteerAngle + vehDriveOrientation), 0.0f) * 5.0f);
			grcDebugDraw::Line(startPos, endPos, col);
		}
	}
#endif // __DEV

	// Clip steering to sensible values
	const float maxSteerAngle = pVehicle->GetIntelligence()->FindMaxSteerAngle();
	const float newSteerAngle = rage::Clamp(desiredSteerAngle, -maxSteerAngle, maxSteerAngle);
	Assert(newSteerAngle > -2.0f && newSteerAngle < 2.0f);
	TUNE_GROUP_FLOAT	(CAR_AI, MinSteerAngleToAdjustThrottle,	0.05f, 0.0f, 100.0f, 0.1f);

	const float fSteeringAngleToAffectThrottle =  rage::Clamp(rage::Abs(newSteerAngle)-MinSteerAngleToAdjustThrottle, 0.0f, maxSteerAngle);

	const float portionOfSteerMaxAngle = fSteeringAngleToAffectThrottle / maxSteerAngle;
	const float fInverseMaxGasAllowed = pVehicle->GetAiXYSpeed() > 10.0f ? 0.7f : 0.5f;
	const float maxGasAllowed = 1.0f -(fInverseMaxGasAllowed * portionOfSteerMaxAngle); // Only allowed 30% gas when at maximum steering angle.

	// Work out the speed so that we can adjust it with gas/brakes.
	Vec3f VehForwardVector;
	LoadAsScalar(VehForwardVector, pVehicle->GetVehicleForwardDirectionRef());
	Vec3f VehVelocity;
	LoadAsScalar(VehVelocity, pVehicle->GetAiVelocityConstRef());
	float forwardSpeed = Dot(VehVelocity, VehForwardVector);

	const float speedDiffDesired = desiredSpeed - forwardSpeed;	// forwardSpeed is in m/s. 40km/u=11.1m/s 60=16.7 80=22.2 100=27.8 120=33.3 140=38.9

	float slopeGasMultiplier = 1.0f;
	float slopeBrakeMultiplier = 1.0f;
	const float slope = VehForwardVector.GetZ();// Positive = uphill.
	const float slopeGasMultiplierForward		= 1.0f + rage::Max(0.0f, slope * 6.0f);
	const float slopeGasMultiplierBackwards		= 1.0f + rage::Max(0.0f, -slope * 5.0f);
	const float slopeBrakeMultiplierForward		= 1.0f + rage::Max(0.0f, -slope * 4.0f);
	const float slopeBrakeMultiplierBackwards	= 1.0f + rage::Max(0.0f, slope * 4.0f);

	const float maxGas = 1.0f;
	float maxBrake = 1.0f;
	const float minGas = -1.0f;
	const float minBrake = -1.0f;

	vehControls.m_brake = 0.0f;

	if (rage::Abs(desiredSpeed) > 0.05f || rage::Abs(speedDiffDesired) > 0.5f)		// was 0.03f
	{
		if(desiredSpeed < 0.0f)
		{	
			// Car is trying to go backwards, but moving forward at a moderate clip
			if(forwardSpeed > 2.0f)
			{
				vehControls.m_brake = 1.0f;
				vehControls.m_throttle = 0.0f;
			}
			else if(speedDiffDesired < 0.0f)
			{	
				// We should accelerate backwards
				if(forwardSpeed > -2.0f) // Accelerate quickly initially so that reverse fudge doesn't kick in
				{
					slopeGasMultiplier = slopeGasMultiplierBackwards;
					vehControls.m_throttle = speedDiffDesired / 4.0f;
				}
				else
				{
					slopeGasMultiplier = slopeGasMultiplierBackwards;
					vehControls.m_throttle = speedDiffDesired / 9.0f;
				}
			}
			else
			{	
				// We should slow down a bit
				vehControls.m_throttle = 0.0f;

				// Going too fast. Use brake to slow down
				slopeBrakeMultiplier = slopeBrakeMultiplierBackwards;
				maxBrake = 0.5f;
				vehControls.m_brake = speedDiffDesired / 12.0f;
			}
		}
		else // Traveling forward
		{
			if( speedDiffDesired > 0.0f )
			{
				TUNE_GROUP_FLOAT	(CAR_AI,			ACC_TO_THROTTLE_MAPPING_SLOW,		4.0f, 0.0f, 100.0f, 0.1f);
				TUNE_GROUP_FLOAT	(CAR_AI,			ACC_TO_THROTTLE_MAPPING_MED,		8.0f, 0.0f, 100.0f, 0.1f);
				TUNE_GROUP_FLOAT	(CAR_AI,			ACC_TO_THROTTLE_MAPPING_HIGH,		2.0f, 0.0f, 100.0f, 0.1f);
				TUNE_GROUP_FLOAT	(CAR_AI,			LOW_THROTTLE_MAPPING_THRESHOLD,		2.0f, 0.0f, 100.0f, 0.1f);
				TUNE_GROUP_FLOAT	(CAR_AI,			MED_THROTTLE_MAPPING_THRESHOLD,		10.0f, 0.0f, 100.0f, 0.1f);

				if(forwardSpeed < LOW_THROTTLE_MAPPING_THRESHOLD) // Accelerate quickly initially so that reverse fudge doesn't kick in
				{
					slopeGasMultiplier = slopeGasMultiplierForward;
					vehControls.m_throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_SLOW;
				}
				else if(forwardSpeed < MED_THROTTLE_MAPPING_THRESHOLD)
				{
					slopeGasMultiplier = slopeGasMultiplierForward;
					vehControls.m_throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_MED;
				}
				else
				{
					slopeGasMultiplier = slopeGasMultiplierForward;
					vehControls.m_throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_HIGH;
				}
			}
			else
			{
				if ( pVehicle->InheritsFromBoat() )
				{
					TUNE_GROUP_FLOAT	(CAR_AI, BOAT_SLOWDOWN_THROTTLE_MAPPING_THRESHOLD,	2.0f, 0.0f, 100.0f, 0.1f);
					vehControls.m_throttle = speedDiffDesired / BOAT_SLOWDOWN_THROTTLE_MAPPING_THRESHOLD;
				}
				else
				{
					vehControls.m_throttle = 0.0f;
				}


				// Going too fast. Use brake to slow down
				slopeBrakeMultiplier = slopeBrakeMultiplierForward;
				vehControls.m_brake = -speedDiffDesired / 10.0f;

				if(pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsInVehicleChase))
				{
					maxBrake = 1.0f;
				}
				else if(GetShouldObeyTrafficLights() && 
					(pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS 
					|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC
					|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GIVE_WAY))
				{
					maxBrake = 0.9f;	// more aggressive braking for stopping at junctions
				}
				else
				{
					maxBrake = 0.75f;	// original default value
				}
			}
		}

		// Depending on our steering angle we're only allowed to accelerate a certain amount.
		// This should avoid cars skidding out when they accelerate out of bends.
		vehControls.m_throttle = rage::Min(maxGasAllowed, vehControls.m_throttle);

		vehControls.m_throttle *= slopeGasMultiplier;
		vehControls.m_brake *= slopeBrakeMultiplier;

		vehControls.m_throttle = rage::Clamp((vehControls.m_throttle), minGas, maxGas);
		vehControls.m_brake = rage::Clamp((vehControls.m_brake), minBrake, maxBrake);
	}
	else 
	{
		// Special case if we want to stop and are going slowly. Use full brakes.
		// This stops cars from sneaking past traffic lights.
		vehControls.m_brake = 1.0f;
		vehControls.m_throttle = 0.0f;
	}	

	// We have the problem where the car is steering fully and accelerating at the same time.
	// This causes traction to be used up by the acceleration and the steering to not happen.
	// Fix this here(if steering extremely don't accelerate)
	const float gasDownMult = portionOfSteerMaxAngle * rage::Clamp(((forwardSpeed - 8.0f) / 8.0f), 0.0f, 1.0f);

	TUNE_GROUP_FLOAT	(CAR_AI,				SLIP_THROTTLE_REDUCTION_LERP,		0.3f, 0.0f, 1.0f, 0.01f);
	// Finally apply reduction based on the traction available through the wheels
	const float fMaxThrottleFromTraction = CalculateMaximumThrottleBasedOnTraction(pVehicle);
	const float reducedMaxThrottle = Lerp(SLIP_THROTTLE_REDUCTION_LERP, m_fMaxThrottleFromTraction, fMaxThrottleFromTraction);
	m_fMaxThrottleFromTraction = reducedMaxThrottle;

	// If we're slipping at all, remove any power increase
 	if( pVehicle->GetCheatPowerIncrease() > 1.0f )
 	{
 		if( m_fMaxThrottleFromTraction < 1.0f )
 		{
 			pVehicle->SetCheatPowerIncrease(Lerp(m_fMaxThrottleFromTraction, 1.0f, pVehicle->GetCheatPowerIncrease()));
 		}
 	}

	float fDesiredThrottle = (vehControls.m_throttle)*(1.0f - gasDownMult);
	fDesiredThrottle = rage::Min(fDesiredThrottle, reducedMaxThrottle);

	vehControls.m_throttle = fDesiredThrottle;
	vehControls.SetSteerAngle( newSteerAngle );

	//reverse the controls if we have momentum in the opposite direction from the one we want to go in
	if (
		((forwardSpeed < 0.0f && desiredSpeed > 1.0f)
		||
		(forwardSpeed > 0.0f && desiredSpeed < -1.0f))
		)
	{
		vehControls.SetSteerAngle( -newSteerAngle );
	}
	vehControls.m_handBrake = false;

	// Boats will use their handbrake of they need to make a tight corner.
	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		if(forwardSpeed > 2.0f && rage::Abs(desiredSteerAngle) > ( DtoR * 25.0f))
		{
			vehControls.m_handBrake = true;
		}
	}


	Assert(vehControls.m_steerAngle >= -HALF_PI && vehControls.m_steerAngle <= HALF_PI && vehControls.m_steerAngle == vehControls.m_steerAngle);
	Assert(vehControls.m_throttle >= -1.0f && vehControls.m_throttle <= 1.0f);
	Assert(vehControls.m_brake >= 0.0f && vehControls.m_brake <= 1.0f);

	pVehControls->Copy(vehControls);
}


////////////////////////////////////////////////////
// FUNCTION: HumaniseCarControlInput
// Takes the controls the AI has decided on and smooths them out.
// In the case of conservative driving they can also be clipped.
////////////////////////////////////////////////////
void CTaskVehicleGoToPointAutomobile::HumaniseCarControlInput(CVehicle* RESTRICT pVeh, CVehControls* RESTRICT pVehControls
	, bool bConservativeDriving, bool bGoingSlowly, const float fTimeStep, const float fDesiredSpeed)
{
	Assertf(pVeh, "HumaniseCarControlInput expected a valid set vehicle.");
	Assertf(pVehControls, "HumaniseCarControlInput expected a valid set of vehicle controls.");

	if( bConservativeDriving || bGoingSlowly )
	{
		// Humanize the steering
		const bool bStopped = pVeh->GetAiXYSpeed() < 0.1;
		const float steerAngleDiff = pVehControls->m_steerAngle - pVeh->GetSteerAngle();
		const float fMultiplier = bStopped ? 0.5f : 2.0f;
		const float maxsteerAngleChange = (fMultiplier) * fTimeStep;
		pVehControls->SetSteerAngle( pVeh->GetSteerAngle() + Clamp(steerAngleDiff, -maxsteerAngleChange, maxsteerAngleChange) );
	}


	// Check if we should bother humanizing the gas and brakes.
	if( bConservativeDriving )
	{
		// Humanize the brakes.
		const float brakeDiff = pVehControls->m_brake - pVeh->GetBrake();
		const float maxBrakeChange = Selectf(brakeDiff, 10.0f, 1.0f) * fTimeStep;
		pVehControls->m_brake = pVeh->GetBrake() + Clamp(brakeDiff, -maxBrakeChange, maxBrakeChange);	

		const bool bDrivingInReverse = fDesiredSpeed < -0.05f;
		const float fForwardSpeed = pVeh->GetAiVelocity().Dot(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, bDrivingInReverse)));

		//only worry about fiddling with the throttle if we're not reversing
		//and not moving backward
		if(pVehControls->m_throttle > 0.0f && fForwardSpeed > -0.05f)
		{
			// Humanize the gas.
			// Sensible cars don't go flat out.
			const float	gassDiff = pVehControls->m_throttle - pVeh->GetThrottle();
			const  float maxGasChange = Selectf(gassDiff, 1.0f, 10.0f) * fTimeStep;
			pVehControls->m_throttle = pVeh->GetThrottle() + Clamp(gassDiff, -maxGasChange, maxGasChange);

			// Add a little bit to the max gas allowed if we're on an upward slope.
			float maxGas = CDriverPersonality::FindMaxAcceleratorInput(pVeh->GetDriver(), pVeh);
			maxGas += rage::Max(0.0f, (4.0f * pVeh->GetVehicleForwardDirectionRef().GetZf()));

			//for conservative drivers, always end up clamping to 0.99 since there is some
			//special case "wheels spinning" behavior for when the throttle is exactly 1.0
			//this is intentionally done after the addition above since we had problems with
			//wheels slipping on upward slopes (see b* 1737917)
			maxGas = rage::Min(maxGas, 0.99f);

			pVehControls->m_throttle = rage::Min(pVehControls->m_throttle, maxGas);
		}
	}

	pVeh->SwitchEngineOn();
	pVeh->SetSteerAngle(pVehControls->m_steerAngle);
	pVeh->SetThrottle(pVehControls->m_throttle);
	pVeh->SetBrake(pVehControls->m_brake);
	pVeh->SetHandBrake(pVehControls->m_handBrake);
}

float CTaskVehicleGoToPointAutomobile::CalculateMaximumThrottleBasedOnTraction(const CVehicle* pVehicle)
{
	eVehicleDummyMode dummyMode = pVehicle->GetVehicleAiLod().GetDummyMode();
	if(dummyMode == VDM_SUPERDUMMY)
	{
		return 1.0f;
	}

	TUNE_GROUP_FLOAT	(CAR_AI,				SLIP_ANGLE_MIN,		0.95f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT	(CAR_AI,				SLIP_ANGLE_MAX,		2.5f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT	(CAR_AI,				SLIP_THROTTLE_REDUCTION_MIN,		0.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT	(CAR_AI,				SLIP_THROTTLE_REDUCTION_MAX,		1.0f, 0.0f, 10.0f, 0.01f);
	float fMaxSlipAngle = 0.0f;

	// Preload all the wheels (we'll still probably get a partial cache miss on the first wheel, but
	// by the time we need the next ones they should already be cached)
	const CWheel * const * ppWheels = pVehicle->GetWheels();
	const int iNumWheels = pVehicle->GetNumWheels();
	for(s32 i = 0; i < iNumWheels; i++)
	{
		const CWheel* wheel = ppWheels[i];
		PrefetchObject(&wheel->GetDynamicFlags());
		PrefetchObject(&wheel->GetEffectiveSlipAngleRef());
	}

	for(s32 i = 0 ; i < iNumWheels; i++)
	{
		const CWheel* wheel = ppWheels[i];
		if(wheel->GetIsTouching())
		{
			fMaxSlipAngle = rage::Max(fMaxSlipAngle, wheel->GetEffectiveSlipAngle());
		}
	}

	const float fSlipAnglePerc = Clamp((fMaxSlipAngle-SLIP_ANGLE_MIN)/(SLIP_ANGLE_MAX-SLIP_ANGLE_MIN), 0.0f, 1.0f);
	const float fMaxThrottle = Lerp(fSlipAnglePerc, SLIP_THROTTLE_REDUCTION_MAX, SLIP_THROTTLE_REDUCTION_MIN);
	return fMaxThrottle;
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleGoToPointAutomobile::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_GoToPoint&&iState<=State_GoToPoint);
	static const char* aStateNames[] = 
	{
		"State_GoToPoint",
	};

	return aStateNames[iState];
}
#endif

dev_float CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplier = 0.85f;
dev_float CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplierBike = 0.9f;
dev_float CTaskVehicleGoToNavmesh::ms_fMaxNavigableAngleRadians = 35.0f * DtoR;

CTaskVehicleGoToNavmesh::CTaskVehicleGoToNavmesh(const sVehicleMissionParams& params)
:CTaskVehicleMissionBase(params)
, m_fPathLength(0.0f)
, m_iOnRoadCheckTimer(0)
, m_bWasOnRoad(false)
{
	m_RouteSearchHelper.ResetSearch();
	m_iNumSearchFailures = 0;
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH);
}

CTaskVehicleGoToNavmesh::~CTaskVehicleGoToNavmesh()
{
	m_RouteSearchHelper.ResetSearch();
}

void CTaskVehicleGoToNavmesh::CleanUp()
{
	CVehicle *pVehicle = GetVehicle();
	if (pVehicle)
	{   
		pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = false;
	}
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle)
	{   
		//only allow dummying if we're on a road
		if(fwTimer::GetTimeInMilliseconds() - m_iOnRoadCheckTimer > 500)
		{
			m_bWasOnRoad = CTaskVehicleThreePointTurn::IsPointOnRoad(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), pVehicle);
			m_iOnRoadCheckTimer = fwTimer::GetTimeInMilliseconds();
		}

		pVehicle->m_nVehicleFlags.bTasksAllowDummying = m_bWasOnRoad;

		//assume all navmesh navigation is dirty
		pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

		//we don't have proper width or camber information, so prevent
		//dummying unless we have to
		pVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleGoToNavmesh::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		
		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_WaitingForResults)
			FSM_OnEnter
				return WaitingForResults_OnEnter();
			FSM_OnUpdate
				return WaitingForResults_OnUpdate();

		FSM_State(State_Goto)
			FSM_OnEnter
				return Goto_OnEnter();
			FSM_OnUpdate
				return Goto_OnUpdate();

		FSM_State(State_Stop)
			FSM_OnEnter
				return Stop_OnEnter();
			FSM_OnUpdate
				return Stop_OnUpdate();
	FSM_End
}

void CTaskVehicleGoToNavmesh::CloneUpdate(CVehicle* pVehicle)
{
    CTaskVehicleMissionBase::CloneUpdate(pVehicle);
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
}

const Vector3& CTaskVehicleGoToNavmesh::GetClosestPointFoundToTarget() const
{
	if ( GetState() == State_Goto )
	{
		return m_RouteSearchHelper.GetClosestPointFoundToTarget();
	}
	return m_Params.GetTargetPosition();
}


//TODO: this and its counterparts in CTaskVehicleCruiseNew and CTaskVehicleFollowWaypointRecording could be combined and
//moved into the follow route task helper
float CTaskVehicleGoToNavmesh::FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition, float& fOutSpeed )
{
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfDistanceToProgressRoute,			0.01f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfCruiseSpeedFraction,				0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfMaxLookaheadSpeed,					32.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfDefaultLookahead,					4.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfWheelbaseMultiplier,				0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfSpeedMultiplier,					1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfSpeedThreshold,					8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfNavmeshMaxLookahead,				8.0f, 0.0f, 100.0f, 0.01f);

	// Determine the amount to look down the path.
	// First work out an approximate speed we should consider for lookahead
	// Use the speed we calculated last time as a base for the lookahead distance.
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	const float fDesiredSpeed = pTask ? pTask->GetCruiseSpeed() : GetCruiseSpeed();
	const float fCurrentSpeed = pVehicle->GetVelocity().XYMag();
	float speedForLookAheadDist = rage::Min((sfCruiseSpeedFraction * fDesiredSpeed) + ((1.0f - sfCruiseSpeedFraction) * fCurrentSpeed), sfMaxLookaheadSpeed);

	// Include that whilst considering the wheel base length to work out a lookahead distance.
	float lookAheadDist = sfDefaultLookahead * (1.0f + sfWheelbaseMultiplier * pVehicle->GetVehicleModelInfo()->GetWheelBaseMultiplier()) + 
		sfSpeedMultiplier * rage::Max(0.0f, speedForLookAheadDist - sfSpeedThreshold);

	lookAheadDist = rage::Min(sfNavmeshMaxLookahead, lookAheadDist);

	const float fDistSearched = m_followRoute.FindTargetCoorsAndSpeed(pVehicle, lookAheadDist, sfDistanceToProgressRoute
			, RCC_VEC3V(vStartCoords), IsDrivingFlagSet(DF_DriveInReverse), RC_VEC3V(vTargetPosition), fOutSpeed, false, true, false, -1, false);

	// boat slow down
	if ( pVehicle->InheritsFromBoat() )
	{
		static bool s_SlowDownForDirectionToPath = true;
		if ( s_SlowDownForDirectionToPath )
		{
			Vector3 vTangent;
			Vec3V vForward = pVehicle->GetTransform().GetForward();
			Vec3V vSegment = VECTOR3_TO_VEC3V(m_followRoute.ComputeCurrentSegmentTangent(vTangent));
			fOutSpeed *= 0.5f + ((1.0f + Dot(vForward, vSegment).Getf()) / 2.0f) / 2.0f;
		}
	}


	return fDistSearched;
}

void CTaskVehicleGoToNavmesh::ResetLastTargetPos()
{
	FindTargetCoors(GetVehicle(), m_vLastTargetPosition);
}

bool CTaskVehicleGoToNavmesh::UpdateTargetMoved()
{
	//don't do this if we aren't something that can move
	if (!HasMovingTarget())
	{
		return false;
	}

	static dev_float s_fTargetMovedThresholdSqr = 5.0f * 5.0f;
	Vector3 vCurrentTarget;
	FindTargetCoors(GetVehicle(), vCurrentTarget);
	if (vCurrentTarget.Dist2(m_vLastTargetPosition) > s_fTargetMovedThresholdSqr)
	{
		m_vLastTargetPosition = vCurrentTarget;
		return true;
	}

	//don't update the last target pos otherwise, so if we're slowly creepin along,
	//we'll eventually trigger a replan when we hit the threshold, unstead of staying
	//underneath the threshold each frame
	return false;
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::Start_OnEnter()
{
	CVehicle* pVehicle = GetVehicle();

	FindTargetCoors(pVehicle, m_Params);
	u64 iPathFlags = PATH_FLAG_NEVER_CLIMB_OVER_STUFF|PATH_FLAG_NEVER_DROP_FROM_HEIGHT
		|PATH_FLAG_NEVER_USE_LADDERS|PATH_FLAG_DONT_LIMIT_SEARCH_EXTENTS|PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS;

	if (!(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT || pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE))
	{
		iPathFlags |= PATH_FLAG_NEVER_ENTER_WATER;
		iPathFlags |= PATH_FLAG_NEVER_START_IN_WATER;
	}
	else
	{
		iPathFlags |= PATH_FLAG_NEVER_LEAVE_WATER;
		iPathFlags |= PATH_FLAG_USE_BEST_ALTERNATE_ROUTE_IF_NONE_FOUND;
	}

	float fReferenceDistance = 0.0f;
	if ( IsDrivingFlagSet(DF_AvoidTargetCoors) )
	{
		static float s_FleeDistance = 30.0f;
		fReferenceDistance = s_FleeDistance;
		iPathFlags |= PATH_FLAG_FLEE_TARGET;
	}

	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	const float fBoundRadius = pVehicle->GetBoundRadius();
	spdAABB tempBox;
	const spdAABB& boundBox = pVehicle->GetLocalSpaceBoundBox(tempBox);

	//create the influence sphere. this originates at the rear bonnect position (or front if going in reverse)
	//of the vehicle and is used to prevent us from getting navmesh queries that originate from the from bonnet
	//and go directly behind the vehicle. it doesn't need to be too accurate--all we really want is for the pathfinder
	//to make the decision about whether to turn left or right for us, since avoidance doesn't do a great job of that
	static const u32 nNumInfluenceSpheres = 2;
	TInfluenceSphere influnceSpheres[nNumInfluenceSpheres];
	const Vector3 vInfluenceSphereCoordsBase = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, !IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vVehicleRightDir = VEC3V_TO_VECTOR3(pVehicle->GetVehicleRightDirection());

	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, LocalSphereInnerMultiplier, 5.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, LocalSphereOuterMultiplier, 1.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, LocalSphereRadiusMultiplier, 1.0f, 0.0f, 100.0f, 0.1f);
	const float fInfluenceSphereRadius = fBoundRadius * LocalSphereRadiusMultiplier;

	influnceSpheres[0].Init(vInfluenceSphereCoordsBase + vVehicleRightDir * boundBox.GetMax().GetXf(), fInfluenceSphereRadius
		, LocalSphereInnerMultiplier * INFLUENCE_SPHERE_MULTIPLIER, LocalSphereOuterMultiplier * INFLUENCE_SPHERE_MULTIPLIER);
	influnceSpheres[1].Init(vInfluenceSphereCoordsBase + vVehicleRightDir * boundBox.GetMin().GetXf(), fInfluenceSphereRadius
		, LocalSphereInnerMultiplier * INFLUENCE_SPHERE_MULTIPLIER, LocalSphereOuterMultiplier * INFLUENCE_SPHERE_MULTIPLIER);

	const float fBoundRadiusMultiplierToUse = pVehicle->InheritsFromBike() ? ms_fBoundRadiusMultiplierBike : ms_fBoundRadiusMultiplier;
	m_RouteSearchHelper.SetEntityRadius(fBoundRadius * fBoundRadiusMultiplierToUse);
	m_RouteSearchHelper.SetMaxNavigableAngle(ms_fMaxNavigableAngleRadians);
	m_RouteSearchHelper.StartSearch( NULL, vStartCoords, m_Params.GetTargetPosition(), m_Params.m_fTargetArriveDist, iPathFlags, nNumInfluenceSpheres, &influnceSpheres[0], fReferenceDistance, NULL, true );

	ResetLastTargetPos();

	return FSM_Continue;
}
aiTask::FSM_Return CTaskVehicleGoToNavmesh::Start_OnUpdate()
{
	SetState(State_WaitingForResults);
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::WaitingForResults_OnEnter()
{
	return FSM_Continue;
}
aiTask::FSM_Return CTaskVehicleGoToNavmesh::WaitingForResults_OnUpdate()
{
	if (!m_RouteSearchHelper.IsSearchActive())
	{
		if (m_iNumSearchFailures >= 3)
		{
			SetState(State_Stop);
		}
		else if( GetTimeInState() > 1.0f )
		{
			++m_iNumSearchFailures;
			SetState(State_Start);
		}

		return FSM_Continue;
	}

	float fDistance = 0.0f;
	Vector3 vLastPos(0.0f, 0.0f, 0.0f);
	SearchStatus eSearchStatus;
	static const s32 sMaxPathPoints = CVehicleFollowRouteHelper::MAX_ROUTE_SIZE;
	s32 iPathPoints = sMaxPathPoints;
	Vector3 pvRoutePoints[sMaxPathPoints];
	if (m_RouteSearchHelper.GetSearchResults(eSearchStatus, fDistance, vLastPos, &pvRoutePoints[0], &iPathPoints))
	{
		//we shouldn't have to check iPathPoints here, but I was seeing some cases where searchstatus was 
		//SS_SearchSuccessful and only one point was being returned. I put in a bug for JB to take a look at why that
		//is and am just handling the case here -JM
		if( eSearchStatus == SS_SearchSuccessful && iPathPoints > 1 )
		{
			m_iNumSearchFailures = 0;
			m_followRoute.ConstructFromNavmeshRoute(GetVehicle(), pvRoutePoints, iPathPoints);
			SetState(State_Goto);
			return FSM_Continue;
		}
		else if ( eSearchStatus == SS_SearchFailed )
		{
			++m_iNumSearchFailures;
			SetState(State_Stop);
			return FSM_Continue;
		}
	}

	//I saw some longer queries take long enough to trigger a 3 point turn
	GetVehicle()->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::Goto_OnEnter()
{
	// Calculate the initial target position
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();
	m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);

	sVehicleMissionParams params = m_Params;
	params.SetTargetPosition(vTargetPosition);
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = fSpeed;
	if (pVehicle->InheritsFromPlane())
	{
		params.m_iDrivingFlags = DMode_StopForCars_Strict;
	}
	//params.m_TargetEntity = NULL;
	SetNewTask( rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, true));
	pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = true;

	ResetLastTargetPos();

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::Goto_OnUpdate()
{
	CVehicle* pVehicle = GetVehicle();

	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfDistanceToFindNewPath, 25.0f, 0.0f, 999.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfMinTimeToFindNewPath, 0.5f, 0.0f, 100.0f, 0.1f);
	//TUNE_GROUP_FLOAT(VEHICLE_GOTO_NAVMESH, sfDistanceToConsiderRouteCompleted, 2.0f, 0.0f, 100.0f, 0.1f);

	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();
	float fDistanceToTheEndOfRoute = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);

	const float fDistToTargetSqr = (vStartCoords - m_RouteSearchHelper.GetClosestPointFoundToTarget()).XYMag2();
	if (fDistToTargetSqr < m_Params.m_fTargetArriveDist*m_Params.m_fTargetArriveDist)
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	//update progress on the waypoint recording here and regen our follow route if necessary
	if( fDistanceToTheEndOfRoute < rage::Min( m_fPathLength*0.5f, sfDistanceToFindNewPath ) && GetTimeInState() > sfMinTimeToFindNewPath )
	{
		//update path length
		m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);
		fDistanceToTheEndOfRoute = m_fPathLength;

		SetState(State_Start);
		return FSM_Continue;
	}

	//we may need to re-path to the target if it's moved a significant amount
	if (UpdateTargetMoved())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	// set the target for the gotopointwithavoidance task
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
		pGoToTask->SetTargetPosition(&vTargetPosition);
		pGoToTask->SetCruiseSpeed(fSpeed);
		pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = true;
	}
	return FSM_Continue;
}

void CTaskVehicleGoToNavmesh::Goto_OnExit()
{
	CVehicle* pVehicle = GetVehicle();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = false;
	}
}

aiTask::FSM_Return CTaskVehicleGoToNavmesh::Stop_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleStop());
	return FSM_Continue;
}
aiTask::FSM_Return CTaskVehicleGoToNavmesh::Stop_OnUpdate()
{
	//only quit if we've got 0 search failures (meaning a search success)
	//or more than some number, right now 3
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP) && (m_iNumSearchFailures == 0 || m_iNumSearchFailures >= 3))
	{
		return FSM_Quit;
	}

	if( GetTimeInState() > 1.0f && m_iNumSearchFailures > 0)
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskVehicleGoToNavmesh::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_WaitingForResults",
		"State_Goto",
		"State_Stop"
	};

	return aStateNames[iState];
}



void CTaskVehicleGoToPointWithAvoidanceAutomobile::Debug() const
{
#if DEBUG_DRAW
	if ( GetVehicle()->InheritsFromBoat() )
	{
		Vector3 vTargetPosition = m_Params.GetTargetPosition();
		const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
		grcDebugDraw::Sphere(vStartCoords + ZAXIS, 0.5f, Color_green, false);
		grcDebugDraw::Sphere(vTargetPosition + ZAXIS, Max(m_Params.m_fTargetArriveDist, 0.5f), Color_green, false);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStartCoords + ZAXIS), VECTOR3_TO_VEC3V(vTargetPosition + ZAXIS), 1.0f, Color_OrangeRed);

		if ( GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper() )
		{
			GetVehicle()->GetIntelligence()->GetBoatAvoidanceHelper()->Debug(*GetVehicle());
		}
	}


	if ( GetVehicle()->InheritsFromPlane() )
	{
		Vec3V vPlanePos = GetVehicle()->GetTransform().GetPosition();
		CPlaneIntelligence* pPlaneIntelligence = static_cast<CPlaneIntelligence*>(GetVehicle()->GetIntelligence());
		CVehPIDController& pitchContr = pPlaneIntelligence->GetPitchController();
		CVehPIDController& rollContr = pPlaneIntelligence->GetRollController();
		CVehPIDController& yawContr = pPlaneIntelligence->GetYawController();
		CVehPIDController& throttleContr = pPlaneIntelligence->GetThrottleController();

		char controllerDebugText[128];

		sprintf(controllerDebugText, "Pitch: %.3f, %.3f, %.3f -> %.3f",
			pitchContr.GetPreviousInput(), pitchContr.GetIntegral(), pitchContr.GetPreviousDerivative(), pitchContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 20, controllerDebugText);

		sprintf(controllerDebugText, "Roll: %.3f, %.3f, %.3f -> %.3f",
			rollContr.GetPreviousInput(), rollContr.GetIntegral(), rollContr.GetPreviousDerivative(), rollContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 30, controllerDebugText);

		sprintf(controllerDebugText, "Yaw: %.3f, %.3f, %.3f -> %.3f",
			yawContr.GetPreviousInput(), yawContr.GetIntegral(), yawContr.GetPreviousDerivative(), yawContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 40, controllerDebugText);

		sprintf(controllerDebugText, "Throttle: %.3f, %.3f, %.3f -> %.3f",
			throttleContr.GetPreviousInput(), throttleContr.GetIntegral(), throttleContr.GetPreviousDerivative(), throttleContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 50, controllerDebugText);

		sprintf(controllerDebugText, "Speed: %.3f", GetVehicle()->GetVelocity().Mag());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 60, controllerDebugText);
	}

// 	char stopDistanceText[128];
// 	const float fStopDistanceScalar = CDriverPersonality::GetStopDistanceMultiplierForOtherCars(GetVehicle()->GetDriver(), GetVehicle());
// 	const float fDriverAggressiveness = CDriverPersonality::FindDriverAggressiveness(GetVehicle()->GetDriver(), GetVehicle());
// 	//const u32 nCarRandomSeed = GetVehicle()->GetRandomSeed();
// 	//const u32 nDriverRandomSeed = GetVehicle()->GetDriver() ? GetVehicle()->GetDriver()->GetRandomSeed() : 0;
// 	sprintf(stopDistanceText, "Stop Distance Scale: %.2f", fStopDistanceScalar);
// 	grcDebugDraw::Text(GetVehicle()->GetVehiclePosition(), Color_white, 0, 0, stopDistanceText);
// 	sprintf(stopDistanceText, "Driver Aggressiveness: %.2f", fDriverAggressiveness);
// 	grcDebugDraw::Text(GetVehicle()->GetVehiclePosition(), Color_white, 0, 10, stopDistanceText);

#endif
}

// PURPOSE: Display debug information specific to this task
void CTaskVehicleGoToNavmesh::Debug() const
{
#if DEBUG_DRAW

#endif
}

#endif
